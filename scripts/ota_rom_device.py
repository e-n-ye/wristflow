"""Bound ROM rescue adapter: simulate, capture, status, and an owned worker.

Only capture can select real serial. Parent owns the endpoint lease and Job;
one worker tree owns all UART handles. Flash writes require a separate fixed
writer profile and explicit scope; there is no automatic time sync.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re
import secrets
import sys
import time
from types import SimpleNamespace

from battery_console import Cancelled, Console
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import PortLease, lease_directory
from ota_rom_backup import application, recovery, healthy, Simulation as ApplicationSimulation
from ota_rom_process import ReadProcess, ReaderCleanupError
from ota_rom_rescue import Link, Simulation, experiment, cleanup
from ota_rom_rescue_lab import SOURCES as MODEL_SOURCES
from ota_rom_transport import BoundedSerial, WallClock
from runtime_record_decode import decode_capture
from serial_lab import ROOT, prepare_capture

SOURCES = tuple(dict.fromkeys(MODEL_SOURCES + ("ota_rom_device.py", "ota_rom_transport.py",
    "ota_rom_process.py", "serial_lab.py", "lab_check.py", "battery_device.py",
    "battery_collect.py", "light_protocol.py", "compass_protocol.py", "ota_backup_review.py")))
PHASES = [("prepare", "窗口准备"), ("bind", "核对构建与端点"), ("before", "保全应用前态"),
          ("rom", "ROM捕获与退出"), ("return", "确认原程序返回"), ("after", "读取应用后态"),
          ("final", "末态确认")]
LIMITS = dict(bind=30, before=60, rom=22, after=60, grace=8)
SCENARIOS = ("success", "no-catch", "exit-fail", "aircr-no-reply", "hang-before",
             "hang-rom", "hang-open", "hang-close", "identity-change", "startup-banner",
             "startup-old-frame-rtc-fail", "release-banner", "release-old-frame", "release-readback-fail")
RETAINED = []  # Do not release a live endpoint lease when process exit is unknown.
STUB_PROFILES = ("stub-observe", "stub-b-sector", "stub-b-execute")


def load(path):
    # Publication already uses atomic replacement. A transient Windows sharing
    # denial may still follow it; retry this same read only, never a device action.
    deadline = time.monotonic() + .25
    while True:
        try:
            if Path(path).stat().st_size > 2 * 1024 * 1024:
                raise ValueError("IPC/evidence JSON too large")
            return json.loads(Path(path).read_text(encoding="utf-8"))
        except PermissionError:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise
            time.sleep(min(.01, remaining))


def publish(path, value):
    if Path(path).exists():
        raise ValueError("Never overwrite a phase request/response")
    write_json(path, value)


class FileCancel:
    def __init__(self, path): self.path = Path(path)
    def is_set(self): return self.path.exists()


class FakeSerial:
    """Serial API peer for the process/real-clock rehearsal; no serial import."""
    def __init__(self, role, fixture, scenario, **kwargs):
        assert kwargs["port"] is None and kwargs["timeout"] > 0 and kwargs["write_timeout"] > 0
        self.role, self.scenario = role, scenario
        rom_scenario = "startup-old-frame" if scenario == "startup-old-frame-rtc-fail" else scenario
        self.peer = Simulation(rom_scenario if rom_scenario in ("success", "no-catch", "exit-fail", "aircr-no-reply", "startup-banner", "startup-old-frame", "release-banner", "release-old-frame", "release-readback-fail") else "success") if role == "rom" else ApplicationSimulation(fixture)
        self.opened, self.port, self.dtr, self._rts = False, None, False, False
    @property
    def rts(self): return self._rts
    @rts.setter
    def rts(self, high):
        self._rts = high
        if self.opened and self.role == "rom": self.peer.set_rts(high)
    def open(self):
        if self.scenario == "hang-open" and self.role == "before":
            while True: time.sleep(1)
        self.opened = True
        if self.role != "rom": self.peer.open()
    def read(self, count):
        if self.scenario == "hang-before" and self.role == "before":
            while True: time.sleep(1)
        return self.peer.read(count)
    def write(self, wire):
        if self.scenario == "hang-rom" and self.role == "rom" and b"@w" in wire:
            while True: time.sleep(1)
        result = self.peer.write(wire)
        if self.scenario in ("startup-old-frame-rtc-fail", "rtc-reset") and self.role == "after" and wire.strip() == b"show_date":
            self.peer.queue[:] = b"Sat Jan  1 00:00:27 2000\r\n"
        return result
    def close(self):
        if self.scenario == "hang-close" and self.role == "rom":
            while True: time.sleep(1)
        self.peer.close()
        self.opened = False


def worker(config_path, expected_sha):
    config_path = Path(config_path).resolve()
    if digest(config_path) != expected_sha:
        raise ValueError("Worker config hash changed")
    config = load(config_path)
    if (config.get("schema") != 1 or not re.fullmatch(r"[0-9a-f]{32}", config.get("run_id", "")) or
            type(config.get("simulation")) is not bool):
        raise ValueError("Invalid owned worker config")
    simulation = config["simulation"]
    profile = config.get("profile", "rom-capture")
    if profile not in ("rom-capture", *STUB_PROFILES):
        raise ValueError("Unknown fixed device profile")
    if not simulation and (os.name != "nt" or config.get("rom_reset_authorized") is not True or
                           config.get("conditions_confirmed") is not True):
        raise ValueError("Fresh Windows reset window and physical conditions required")
    if not simulation and profile in STUB_PROFILES and any(
            config["capture"].get(key) is not True for key in
            ("stub_observation_authorized", "original_start_authorized")):
        raise ValueError("Fresh official-stub observation and original-start authorization required")
    if not simulation and profile == "stub-b-sector" and any(config["capture"].get(k) is not True for k in
            ("b_sector_write_authorized", "b_sector_restore_authorized")):
        raise ValueError("Fresh fixed B write AND restoration authorization required")
    if not simulation and profile == "stub-b-execute":
        from ota_b_execute import AUTHORIZATIONS
        if any(config["capture"].get(k) is not True for k in AUTHORIZATIONS):
            raise ValueError("Fresh fixed B/boot execution and restoration scope required")
    verify_files(config["sources"] + config["inputs"])
    out = config_path.parent
    journal = Journal(out / "worker", binding=config, mode="ota-rom-owned-worker")
    cancel = FileCancel(out / "cancel.json")
    console = SimpleNamespace(cancelled=cancel)
    token = config["run_id"]
    phase_end = config["active_end"]
    phase = "bind"
    link = None
    def check():
        cleaning = link is not None and link.cleaning
        deadline = config["cleanup_end"] if cleaning else min(phase_end, config["active_end"])
        if time.monotonic() >= deadline: raise TimeoutError("Worker/global phase deadline")
        if not cleaning and cancel.is_set(): raise Cancelled("Parent cancelled this worker")
    def integrity():
        if digest(config_path) != expected_sha: raise ValueError("Owned config changed")
        verify_files(config["sources"] + config["inputs"])
    check()
    if simulation:
        binding = dict(simulation=True, cycle_log=True, endpoint={"device": "SIMULATED-ROM-WALL"})
        def identity():
            integrity(); check()
            if config["scenario"] == "identity-change" and phase == "rom":
                raise ValueError("SIMULATION endpoint identity changed before ROM open")
            return binding["endpoint"]
    else:
        args = config["capture"]
        build = load(args["result"])
        if any(build.get(k) for k in ("storage_probe", "resource_probe", "fs_probe")) or not any(
                item["path"].replace("\\", "/").endswith("/product_runtime_record.c")
                for item in build.get("project_sources", [])):
            raise ValueError("An ordinary runtime-recorder build is required")
        binding, original_identity = prepare_capture(args["result"], args["flash"], args["expected_bin"],
            args["port"], args["location"], True, build_archive=args["build_archive"], recorded_root=args["recorded_root"])
        binding.update(cycle_log=build.get("cycle_log") is True,
                       policy=("Fixed006 boot/B execution or explicit restoration; no other Flash or RTC time write"
                               if profile == "stub-b-execute" else
                               "Fixed B4KiB candidate and same-window restoration; no other Flash or RTC time write"
                               if profile == "stub-b-sector" else
                               "Original stub writes BKP0R/PA21; no Flash write/erase or RTC time write"
                               if profile in STUB_PROFILES else
                               "Owned ROM debug window; fixed register operations, no Flash/RTC write"))
        def identity(): integrity(); check(); return original_identity()
    check()
    publish(out / "ready.json", dict(schema=1, run_id=token, config_sha=expected_sha, binding=binding))
    before, before_at = None, None
    for sequence, phase in enumerate(("before", "rom", "after"), 1):
        request_path = out / f"request-{sequence}.json"
        while not request_path.exists():
            if (out / "stop.json").exists(): return 0
            check(); time.sleep(.01)
        request = load(request_path)
        if request != dict(schema=1, run_id=token, sequence=sequence, phase=phase):
            raise ValueError("Foreign, stale or out-of-order worker request")
        phase_end = min(config["active_end"], time.monotonic() + config["limits"][phase])
        link = None
        value, error, transport = {}, "", None
        try:
            check(); integrity()
            if phase == "rom" and (before is None or not 0 <= time.monotonic() - before_at <= 120):
                raise ValueError("ROM requires this worker's fresh successful prestate")
            fixture = Path(config[phase + "_fixture"]).read_bytes() if simulation and phase != "rom" else b""
            factory = (lambda **kw: FakeSerial(phase, fixture, config["scenario"], **kw)) if simulation else None
            if phase == "before" and profile == "stub-b-execute" and config["observer"].get("recovery_from"):
                from ota_b_execute import prior_before
                identity()
                value = prior_before(config, journal.directory)
                before, before_at = value, time.monotonic()
                journal.event("prior_application_baseline", physical_prestate_read=False, source=value["prior_snapshot"])
            elif phase == "rom" and profile in STUB_PROFILES:
                if profile == "stub-b-execute":
                    from ota_b_execute import probe
                elif profile == "stub-b-sector":
                    from ota_b_sector import probe
                else:
                    from ota_stub_observe import probe
                identity()
                phase_end = min(phase_end, config["active_end"] - 10)
                check()
                value = dict(entry_attempted=None, cleanup=dict(resources_closed=False), observations={})
                value = probe(config, journal.directory,
                              "SIMULATED-STUB" if simulation else config["capture"]["port"], phase_end)
                if value["error"]:
                    raise ValueError(value["error"])
            else:
                transport = BoundedSerial("SIMULATED-ROM-WALL" if simulation else config["capture"]["port"],
                                          phase, journal, check, identity, factory=factory)
                if phase != "rom":
                    value = application(phase, transport, journal, console, identity, cycle=binding.get("cycle_log", False))
                    value["closed_monotonic_ns"] = time.monotonic_ns()
                    if phase == "before": before, before_at = value, time.monotonic()
                else:
                    observed = {}
                    with (journal.directory / "rom-rx.bin").open("xb") as raw:
                        link = Link(transport, raw, journal, WallClock(), cancel, check,
                                    active_seconds=min(22, config["limits"]["rom"]))
                        try:
                            transport.open()
                            for step in experiment(link, observed):
                                journal.event("rom_step", step=step, monotonic=time.monotonic())
                        finally:
                            value = dict(observations=observed, entry_attempted=link.entry_intent,
                                         cleanup=cleanup(link, allow_exit_reset=config["allow_exit_reset"]))
                    if value["cleanup"]["errors"] or not value["cleanup"]["debug_release_confirmed"]:
                        raise ValueError("Debug exit incomplete; preserve observation separately")
        except BaseException as exc:
            error = repr(exc)
            journal.event("phase_error", phase=phase, error=error)
        finally:
            if transport is not None and not transport.closed:
                try: transport.close()
                except BaseException as exc:
                    error += " Transport close: " + repr(exc)
        publish(out / f"response-{sequence}.json", dict(schema=1, run_id=token, sequence=sequence,
            phase=phase, request_sha=digest(request_path), ok=not error, error=error, value=value))
        # Manual return confirmation can take longer than the completed phase.
        # Idle never inherits a previous phase deadline or cleanup exemption.
        phase_end, link = config["active_end"], None
    return 0


def run(output, *, capture=None, fixtures=None, scenario="success", ready=None, limits=None,
        total_seconds=300, prompt_seconds=120, process_factory=ReadProcess, allow_exit_reset=True,
        profile="rom-capture", observer=None, observer_plan="dual-nor"):
    """Parent owns all process/endpoint resources, never a UART handle."""
    output = Path(output).resolve()
    simulation = capture is None
    if profile not in ("rom-capture", *STUB_PROFILES):
        raise ValueError("Unknown fixed device profile")
    if observer_plan not in ("dual-nor", "geometry", "ftab") or profile != "stub-observe" and observer_plan != "dual-nor":
        raise ValueError("Unknown or misplaced fixed observer plan")
    maximum_limits, maximum_total = LIMITS, 300
    observer_inputs = []
    if profile == "stub-b-execute":
        from ota_b_execute import LIMITS as EXEC_LIMITS, SCENARIOS as EXEC_SCENARIOS, prepare
        if scenario not in EXEC_SCENARIOS or allow_exit_reset:
            raise ValueError("Fixed execution scenario and no fallback reset required")
        observer, observer_inputs = prepare(observer)
        maximum_limits, maximum_total = EXEC_LIMITS, 600
    elif profile == "stub-b-sector":
        from ota_b_sector import LIMITS as SECTOR_LIMITS, SCENARIOS as SECTOR_SCENARIOS, prepare
        if scenario not in SECTOR_SCENARIOS:
            raise ValueError("Unknown fixed B-sector simulation")
        observer, observer_inputs = prepare(observer)
        maximum_limits, maximum_total = SECTOR_LIMITS, 600
    elif profile in STUB_PROFILES:
        from ota_stub_observe import LIMITS as STUB_LIMITS, GEOMETRY_LIMITS, SCENARIOS as STUB_SCENARIOS, FTAB_SCENARIOS, prepare
        if scenario not in STUB_SCENARIOS:
            raise ValueError("Unknown stub simulation scenario")
        observer, observer_inputs = prepare(observer)
        maximum_limits, maximum_total = (GEOMETRY_LIMITS, 300) if observer_plan in ("geometry", "ftab") else (STUB_LIMITS, 900)
        if observer_plan != "ftab" and (scenario.startswith("ftab-") or scenario.startswith("host-")):
            raise ValueError("FTAB fault scenario belongs only to FTAB plan")
        if observer_plan == "ftab" and scenario not in FTAB_SCENARIOS:
            raise ValueError("Scenario does not exercise FTAB plan")
    elif observer is not None:
        raise ValueError("Observer inputs belong only to the fixed stub profile")
    limits = dict(maximum_limits if limits is None else limits)
    if (output.exists() or set(limits) != set(maximum_limits) or any(not 0 < v <= maximum_limits[k] for k,v in limits.items()) or
            not 13 < total_seconds <= maximum_total or not 0 < prompt_seconds <= (60 if observer_plan in ("geometry", "ftab") else 120)):
        raise ValueError("New output and bounded phase/global limits required")
    if not simulation and (os.name != "nt" or capture.get("conditions_confirmed") is not True or
                           capture.get("rom_reset_authorized") is not True or
                           allow_exit_reset and capture.get("exit_reset_authorized") is not True or
                           not re.fullmatch(r"COM[1-9][0-9]*", capture.get("port", ""))):
        raise ValueError("Explicit current ROM reset authorization, conditions and canonical port required")
    if not simulation and profile in STUB_PROFILES and any(capture.get(key) is not True for key in
            ("stub_observation_authorized", "original_start_authorized")):
        raise ValueError("Official-stub observation and original-start authorization required before any worker")
    if not simulation and profile == "stub-b-sector" and any(capture.get(k) is not True for k in
            ("b_sector_write_authorized", "b_sector_restore_authorized")):
        raise ValueError("Explicit fixed B write AND restore authorization required before output or worker")
    if not simulation and profile == "stub-b-execute":
        from ota_b_execute import AUTHORIZATIONS
        if any(capture.get(k) is not True for k in AUTHORIZATIONS):
            raise ValueError("Explicit fixed B/boot execution and restoration scope required before output or worker")
    token = secrets.token_hex(16)
    sources = [file_record(Path(__file__).with_name(name)) for name in SOURCES]
    sources.append(file_record(sys.executable))
    if profile in STUB_PROFILES:
        sources.append(file_record(Path(__file__).with_name("ota_stub_observe.py")))
    if profile == "stub-b-sector":
        sources.append(file_record(Path(__file__).with_name("ota_b_sector.py")))
    if profile == "stub-b-execute":
        sources.append(file_record(Path(__file__).with_name("ota_b_execute.py")))
    capture_paths = ("result", "flash", "build_archive")
    inputs = [] if simulation else [file_record(capture[k]) for k in capture_paths]
    inputs.extend(observer_inputs)
    journal = Journal(output, binding=dict(simulation=simulation, sources=sources, inputs=inputs,
        requested_capture=capture, observer_plan=observer_plan, run_id=token, policy=("Fixed006 probe execution and original boot/B restoration under pinned official runtime"
            if profile == "stub-b-execute" else "One fixed B4KiB candidate/restore under original RAM stub; no other Flash or RTC time write"
            if profile == "stub-b-sector" else "One owned worker tree; original stub BKP0R/PA21 only; no Flash or RTC time write"
            if profile in STUB_PROFILES else "One owned worker, no parent UART, no Flash/RTC write")), mode="ota-rom-device")
    if profile == "stub-b-sector":
        journal.event("fixed_sector_scope", address="0x12420000", bytes=4096, restore_required=True,
                      policy="One candidate write and one fresh-prestate restore; no other Flash write or RTC time write")
    end = time.monotonic() + total_seconds
    config = dict(schema=1, simulation=simulation, run_id=token, sources=sources, inputs=inputs,
        active_end=end-13, cleanup_end=end-5, limits=limits, scenario=scenario,
        allow_exit_reset=allow_exit_reset, conditions_confirmed=simulation or capture["conditions_confirmed"],
        rom_reset_authorized=simulation or capture["rom_reset_authorized"], capture=capture)
    if profile in STUB_PROFILES:
        config.update(profile=profile, observer=observer, observer_plan=observer_plan)
    if simulation:
        if fixtures is None or set(fixtures) != {"before", "after"}: raise ValueError("Explicit simulation fixtures required")
        for phase in ("before", "after"):
            path=output/(phase+"-fixture.bin"); path.write_bytes(fixtures[phase])
            config[phase+"_fixture"] = str(path)
            inputs.append(file_record(path))
    else:
        config["capture"] = dict(capture, **{k: str(Path(capture[k]).resolve()) for k in capture_paths+("recorded_root",)})
    publish(output/"config.json",config)
    config_sha=digest(output/"config.json")
    argv=[sys.executable,"-X","utf8",str(Path(__file__).resolve()),"worker","--config",str(output/"config.json"),"--sha",config_sha]
    process, lease = None, None
    report=dict(simulation=simulation, hardware_verified=False, rom_rescue_verified=False, deployment_authorized=False,
        flash_write=False, clock_write=False, observation_passed=False, application_recovery_passed=False,
        physical_observations_confirmed=False, entry_attempted=False, worker_attempted=False, worker_started=False, worker_exit=None,
        worker_forced=False, resources_closed=True, lease_retained=False, argv=argv, errors=[])
    outcome, detail = "passed", ""
    final_confirmed = False
    current_phase = "prepare"
    labels = dict(prepare="准备", bind="绑定", before="应用前态", rom="ROM观测与退出", return_="原程序返回",
                  after="应用后态", recovery="应用记录与RTC恢复", rtc="RTC连续性", final="末态确认", cleanup="资源清理", worker="工作进程", evidence="证据记录")
    if profile == "stub-b-sector":
        labels["rom"] = "B首4KiB写入与恢复"
    if profile == "stub-b-execute":
        labels["rom"] = "固定B启动与boot／B恢复"
    def mutation_facts(value):
        if profile == "stub-b-execute":
            report.update(flash_write=value.get("mutation_attempted"), boot_may_be_changed=value.get("boot_may_be_changed"),
                b_may_be_changed=value.get("b_may_be_changed"), restored_verified=value.get("restored_verified", False))
    def add_error(phase, error):
        item = dict(phase=phase, error=str(error))
        if item not in report["errors"]:
            report["errors"].append(item)
            try:
                journal.event("validation_error", **item)
            except OSError as exc:
                # Failed error logging must not bypass owned process cleanup.
                logging_error = dict(phase="evidence", error=repr(exc))
                if logging_error not in report["errors"]: report["errors"].append(logging_error)
    def errors_for(*phases):
        return " / ".join(item["error"] for item in report["errors"] if item["phase"] in phases)
    def remaining(maximum=120):
        value=min(maximum,end-13-time.monotonic())
        if value<=0: raise TimeoutError("Workflow active budget exhausted; cleanup time is reserved")
        return value
    def response(sequence, phase):
        value=load(output/f"response-{sequence}.json")
        if any(value.get(k)!=v for k,v in dict(schema=1,run_id=token,sequence=sequence,phase=phase,
                request_sha=digest(output/f"request-{sequence}.json")).items()) or type(value.get("ok")) is not bool:
            raise ValueError("Foreign/stale/invalid worker response")
        return value
    def app_result(value, phase):
        raw=output/"worker"/(phase+"-uart.bin")
        verify_files([value["raw"]])
        if Path(value["raw"]["path"]).resolve()!=raw.resolve() or decode_capture(raw.read_bytes(),value["records"]["token"])!=value["records"]:
            raise ValueError("Worker application evidence does not match its raw bytes")
        healthy(value["records"])
        return value
    stub_label = "固定RAM配置读取" if observer_plan == "geometry" else "FTAB单次读取与接收完整性" if observer_plan == "ftab" else "原版stub状态与双读"
    phases = [(key, stub_label if key == "rom" and profile in STUB_PROFILES else label)
              for key, label in PHASES]
    mode = ("原版stub观测模拟（无设备）" if simulation else "原版stub观测（零Flash写入）") if profile in STUB_PROFILES else (
        "ROM真实时钟／假串口演练" if simulation else "ROM救援观测（零Flash写入）")
    if observer_plan == "geometry":
        mode = "固定RAM配置模拟（无设备）" if simulation else "固定RAM配置观测（零Flash写入）"
    if observer_plan == "ftab":
        mode = "FTAB接收完整性模拟（无设备）" if simulation else "FTAB接收完整性（只读4KiB）"
    if profile == "stub-b-sector":
        stub_label = "B首4KiB写入与原字节恢复"
        phases = [(key, stub_label if key == "rom" else label) for key, label in PHASES]
        mode = "B首4KiB写入恢复模拟（无设备）" if simulation else "B首4KiB写入与恢复"
    if profile == "stub-b-execute":
        recovering = bool(observer.get("recovery_from"))
        phases = [(key, ("先前应用基线（不读设备）" if recovering and key == "before" else
                   "固定B启动与boot／B恢复" if key == "rom" else label)) for key, label in PHASES]
        mode = ("固定原boot／B恢复" if recovering else "固定B启动与boot／B恢复") + ("模拟（无设备）" if simulation else "")
    with Console(journal,phases,mode=mode) as console:
        print(console.url,flush=True)
        if ready: ready(console)
        def wait_file(path, seconds, phase):
            def pump():
                remaining()
                if process.poll() is not None and not path.exists(): raise RuntimeError("Worker exited before its phase response")
                console.cancelled.wait(.02)
            console.automatic(phase,"【假串口】"+phase+"，保持当前窗口，可取消。" if simulation else
                "正在执行"+phase+"；保持接线，此阶段原程序暂停，KEY1不能恢复应用。可取消本轮。",
                remaining(seconds),pump=pump,until=path.exists)
        def phase_request(sequence, phase):
            publish(output/f"request-{sequence}.json",dict(schema=1,run_id=token,sequence=sequence,phase=phase))
            wait_file(output/f"response-{sequence}.json",limits[phase]+(limits["grace"] if phase=="rom" else 0),phase)
            return response(sequence,phase)
        try:
            preparation = "【假串口／无设备】确认开始完整时钟、子进程与取消演练。" if simulation else (
                "确认本窗已授权一次主动入口RTS、一次SYSRESETREQ，装载固定官方RAM stub，"
                "按官方流程设置BKP0R及PA21、仅观察LCPU／DMA寄存器并读取两份NOR；"
                "最后单发burn_reset返回原程序，异常时最多一次退出RTS，零Flash写擦。"
                if profile in STUB_PROFILES else
                "确认本轮已授权一次入口RTS、一次内部SYSRESETREQ，以及退出异常时最多一次RTS。")
            if not simulation and observer_plan == "geometry":
                preparation = ("确认本窗已授权一次主动入口RTS、一次SYSRESETREQ，装载固定原版RAM stub及官方BKP0R／PA21设置；"
                    "读取ready寄存器和固定RAM中的Flash句柄／UART对象，以及有界指针所指的DMA结构与命令表，总计最多1056B。"
                    "没有NOR读取或Flash写擦；最后单发burn_reset，异常时最多一次退出RTS。")
            if observer_plan == "ftab":
                preparation = (("【假串口／无设备】" if simulation else "") +
                    "本窗仅一次入口RTS／SYSRESETREQ和固定原版RAM stub（含BKP0R／PA21）；"
                    "ready及MPI2 alias三字全零后，只读0x12000000的4096B FTAB一次。"
                    "读取前后记录Windows接收错误位；该查询会确认清除错误位，不丢弃接收字节。"
                    "长度、CRC及与原备份逐字节匹配才通过；不补零、不重读、不安装boot／B、不校时。"
                    "正常单发burn_reset返回原程序；失败停止协议，仅按本窗授权最多一次退出RTS。")
            if profile == "stub-b-sector":
                preparation = (("【假串口／无设备】模拟以下固定流程。" if simulation else "") +
                    "确认本窗已授权一次入口RTS／SYSRESETREQ及固定原版RAM stub（含BKP0R／PA21），"
                    "先读B首64KiB并核旧备份；只向0x12420000写4096B候选一次，独立读回，"
                    "再用本窗原4096B恢复一次并读回，后60KiB须不变；不换boot、不运行B。"
                    "协议／取消／日志异常可能使B未恢复，此时停止新协议；正常单发burn_reset，异常最多一次退出RTS。")
            if profile == "stub-b-execute":
                preparation = (("【假串口／无设备】" if simulation else "") +
                    ("本窗仅恢复先前窗封存的原boot64KiB及原B首4KiB；一次入口RTS／SYSRESETREQ及原stub，不再次安装候选或启动B。应用前态复用先前原流，不读取当前停住的程序。"
                     if recovering else
                     "本窗仅安装固定006的B首4096B及兼容boot58496B，各一次；独立读回后单发burn_reset，接收固定tag的PREPARED→MAPPED→EXECUTED，再按计划第二次入口RTS／SYSRESETREQ及原stub恢复。") +
                    "每次原stub沿用BKP0R／PA21，ready及MPI2映射三字须为零才读取Flash。恢复顺序为原boot64KiB写一次／读回，再原B首4KiB写一次／64KiB读回；全部匹配才单发burn_reset返回原程序。"
                    "仅固定范围、FTAB不写；不自动校时。协议／CRC／取消／日志错误停止新命令，无额外退出RTS；可能留下boot／B未恢复，保留接线及原文件，另开明确恢复窗。")
            if not simulation:
                preparation += ("同板同固件，USB／电池接线固定、人在旁、显示触摸正常。"
                    "开启临时常亮供前后态读取；RAM尾段仅保全到电脑，RTC可能重置，不自动校时。")
            console.prompt("prepare",preparation,
                remaining(prompt_seconds)); console.stage("prepare","complete")
            lease=PortLease("SIMULATED-ROM-WALL" if simulation else capture["port"],
                output.parent/"simulation-locks" if simulation else lease_directory())
            lease.__enter__(); journal.event("lease_acquired")
            verify_files(sources+inputs)
            (output/"process").mkdir()
            report["worker_attempted"]=True
            current_phase="bind"
            try:
                process=process_factory(argv,output/"process")
                report["worker_started"]=True
            except ReaderCleanupError as exc:
                process=exc.owner; report["worker_started"]=None; report["resources_closed"]=False; raise
            wait_file(output/"ready.json",limits["bind"],"bind")
            bound=load(output/"ready.json")
            if (bound.get("schema")!=1 or bound.get("run_id")!=token or bound.get("config_sha")!=config_sha or
                    not isinstance(bound.get("binding"),dict) or bound["binding"].get("simulation") is not simulation):
                raise ValueError("Unbound worker readiness")
            report["binding"]=bound["binding"]
            current_phase="before"
            before_reply=phase_request(1,"before")
            if not before_reply["ok"]: raise ValueError(before_reply["error"])
            before=app_result(before_reply["value"],"before")
            current_phase="rom"
            # The worker may act as soon as this request is published. Only a
            # validated reply may establish that no physical entry was attempted.
            report["entry_attempted"]=None
            if profile == "stub-b-sector":
                report.update(flash_write=None, b_may_be_changed=None, restored_verified=False)
            if profile == "stub-b-execute":
                mutation_facts({})
            rom=phase_request(2,"rom")
            if not rom["ok"]: add_error("rom",rom["error"])
            value=rom["value"]
            report.update(entry_attempted=value.get("entry_attempted"),rom=value)
            if profile == "stub-b-sector":
                report.update(flash_write=value.get("candidate_write_attempted"), b_may_be_changed=value.get("b_may_be_changed"),
                              restored_verified=value.get("restored_verified",False))
            mutation_facts(value)
            report["observation_passed"]=value.get("observations",{}).get("observation_passed",False)
            if not value.get("cleanup",{}).get("resources_closed"):
                raise ValueError("ROM transport exit is uncertain; no new UART open")
            current_phase="return_"
            console.prompt("return","【假串口】确认开始模拟恢复检查。" if simulation else
                "确认原程序已返回、显示和触摸正常；屏幕熄灭时按一次KEY1。开启临时常亮供后态读取；"
                "异常请取消，程序不另开串口补复位。",remaining(prompt_seconds)); console.stage("return","complete")
            current_phase="after"
            after_reply=phase_request(3,"after")
            if not after_reply["ok"]: raise ValueError(after_reply["error"])
            after=app_result(after_reply["value"],"after")
            elapsed=(after["closed_monotonic_ns"]-before["closed_monotonic_ns"])/1e9
            current_phase="recovery"
            try:
                if profile == "stub-b-execute":
                    from ota_b_execute import recovery_check
                    report["application_recovery"] = recovery_check(before, after, elapsed, recovering)
                else:
                    report["application_recovery"]=recovery(before,after,elapsed_seconds=elapsed,
                        check_rtc=profile not in STUB_PROFILES)
                report["application_recovery_passed"]=True
                if profile == "stub-b-execute" and (not report["application_recovery"].get("rtc_valid") or not report["application_recovery"]["rtc_continuity"]):
                    report["clock_recovery_required"] = True
                    journal.event("rtc_separate", valid=report["application_recovery"].get("rtc_valid"), continuity=report["application_recovery"]["rtc_continuity"], automatic_time_write=False)
                elif not report["application_recovery"]["rtc_continuity"]:
                    report["clock_recovery_required"]=True
                    add_error("rtc","RTC continuity changed; core data retained, time recovery needs separate authorization")
            except ValueError as exc:
                add_error("recovery",repr(exc))
            if report["errors"]:
                outcome="error"
                journal.event("validation_failed",errors=report["errors"])
            if not report["errors"] or (profile in STUB_PROFILES and all(item["phase"] == "rtc" for item in report["errors"])) or profile in ("stub-b-sector", "stub-b-execute") or observer_plan == "ftab":
                current_phase="final"
                console.prompt("final","【假串口】确认演练结束；无设备操作。" if simulation else
                    ("关闭临时常亮，回表盘，确认显示、触摸与KEY1正常。RTC变化已独立记录，校时另行确认。"
                     if profile in STUB_PROFILES else "关闭临时常亮，回表盘，确认显示、触摸、KEY1与时间正常。异常请取消并在聊天说明。"),
                    remaining(prompt_seconds)); console.stage("final","complete")
                report["physical_observations_confirmed"]=not simulation
                final_confirmed = True
        except BaseException as exc:
            outcome="cancelled" if isinstance(exc,(Cancelled,KeyboardInterrupt)) else ("timeout" if isinstance(exc,TimeoutError) else "error")
            detail=repr(exc); journal.event("device_error",outcome=outcome,error=detail)
            add_error(current_phase,detail)
        finally:
            if process is not None:
                try:
                    if outcome!="passed" and not (output/"cancel.json").exists(): publish(output/"cancel.json",dict(run_id=token))
                    if not (output/"stop.json").exists(): publish(output/"stop.json",dict(run_id=token))
                    stop_end=min(time.monotonic()+limits["grace"],end-5)
                    while process.poll() is None and time.monotonic()<stop_end: time.sleep(.02)
                    report["worker_exit"]=process.poll()
                    report["worker_forced"]=report["worker_exit"] is None
                    if report["worker_forced"] and (output/"request-2.json").exists() and not (output/"response-2.json").exists():
                        report["entry_attempted"]=None  # Worker may be blocked after a physical control write.
                except BaseException as exc:
                    report["worker_forced"]=True
                    if (output/"request-2.json").exists() and not (output/"response-2.json").exists():
                        report["entry_attempted"]=None
                    detail += " Cleanup: "+repr(exc)
                    add_error("cleanup",repr(exc))
                finally:
                    # IPC storage or poll failure must never bypass owned kill/reap.
                    try:
                        process.close()
                        report["resources_closed"]=True
                    except BaseException as exc:
                        report["resources_closed"]=False
                        detail += " Process close: "+repr(exc)
                        add_error("cleanup",repr(exc))
                # Collect a completed ROM response even when cancellation won the UI race.
                if (output/"response-2.json").exists():
                    try:
                        late_reply=response(2,"rom")
                        if not late_reply["ok"]: add_error("rom",late_reply["error"])
                        late=late_reply["value"]
                        report.update(rom=late,entry_attempted=late.get("entry_attempted"),
                            observation_passed=late.get("observations",{}).get("observation_passed",False))
                        if profile == "stub-b-sector":
                            report.update(flash_write=late.get("candidate_write_attempted"),b_may_be_changed=late.get("b_may_be_changed"),
                                          restored_verified=late.get("restored_verified",False))
                        mutation_facts(late)
                    except BaseException as exc:
                        report["entry_attempted"]=None
                        if profile == "stub-b-sector":
                            report.update(flash_write=None,b_may_be_changed=None,restored_verified=False)
                        if profile == "stub-b-execute":
                            mutation_facts({})
                        detail += " Late response: "+repr(exc)
                        add_error("rom",repr(exc))
                if report["worker_forced"] or report["worker_exit"] != 0 or not report["resources_closed"]:
                    add_error("worker","Owned worker required termination or exited abnormally; exit="+str(report["worker_exit"]))
                    if (output/"request-2.json").exists() and not (output/"response-2.json").exists():
                        add_error("rom","ROM result unknown: worker stopped before a complete response")
                    if outcome=="passed": outcome="error"
            if lease is not None:
                if report["resources_closed"]: lease.__exit__(None,None,None)
                else:
                    RETAINED.append((process,lease)); report["lease_retained"]=True
            report["recovery_required"]=bool(report["worker_forced"] or not report["resources_closed"] or
                (report["entry_attempted"] is not False and not report["application_recovery_passed"]))
            if profile == "stub-b-sector" and report.get("flash_write") is not False and not report.get("restored_verified"):
                report["recovery_required"]=True
            if profile == "stub-b-execute" and (report.get("boot_may_be_changed") is not False or report.get("b_may_be_changed") is not False):
                report["recovery_required"] = True
        outcome=console.seal(outcome)
        detail="\n".join(labels.get(item["phase"],item["phase"])+"："+item["error"] for item in report["errors"])
        report.update(passed=outcome=="passed",outcome=outcome,detail=detail)
        report["verification"]=[dict(title=(("固定RAM配置读取" if observer_plan == "geometry" else "FTAB单次读取与接收完整性" if observer_plan == "ftab" else "原版stub观测与双份NOR") if profile in STUB_PROFILES else "ROM观测与退出")+("（假串口）" if simulation else ""),
                passed=report["observation_passed"] and not errors_for("rom"),detail=errors_for("rom")),
            dict(title="应用保存记录" if profile in STUB_PROFILES else "应用记录与RTC后态",passed=report["application_recovery_passed"],detail=errors_for("before","after","recovery")),
            dict(title="进程与端点租约关闭",passed=report["resources_closed"] and not report["lease_retained"],detail=errors_for("cleanup","worker")),
            dict(title="模拟末态确认" if simulation else "人工末态确认",passed=outcome=="passed",detail=errors_for("prepare","return_","final"))]
        if profile in STUB_PROFILES:
            report["profile"] = profile
            report["observation_plan"] = observer_plan
            report["verification"].append(dict(title="RTC连续性", passed=report.get("application_recovery", {}).get("rtc_continuity", False), detail=errors_for("rtc")))
            report["verification"][3]["passed"] = final_confirmed
        if profile == "stub-b-sector":
            report["verification"][0]["title"] = "B首4KiB候选与保护区核验" + ("（模拟）" if simulation else "")
            report["verification"].insert(1,dict(title="本窗原字节恢复及64KiB读回",passed=report.get("restored_verified") is True,
                detail="" if report.get("restored_verified") else "原字节恢复尚未证明；保留本窗前态，禁止盲目补写"))
        if profile == "stub-b-execute":
            result = report.get("rom", {}).get("observations", {}).get("report", {})
            report["verification"][0]["title"] = "固定B启动结果" if not recovering else "固定恢复范围"
            report["verification"][0]["passed"] = result.get("restored_verified") is True if recovering else result.get("probe_verified") is True
            report["verification"][0]["detail"] = ("固定三阶段原流已核验；后续恢复状态另列" if not recovering and result.get("probe_verified") else errors_for("rom"))
            report["verification"].insert(1, dict(title="原boot与B原字节恢复", passed=report.get("restored_verified") is True,
                detail="" if report.get("restored_verified") else "boot／B恢复未证，保持接线并保留原文件"))
            report["verification"][-1] = dict(title="RTC有效性与连续性（独立记录）",
                passed=report.get("application_recovery", {}).get("rtc_valid", False) and report.get("application_recovery", {}).get("rtc_continuity", False),
                detail="不参与核心写入／执行／恢复结果，不自动校时")
        write_json(output/"device.json",report)
        journal.event("finished",outcome=outcome,report_sha=digest(output/"device.json"))
        ending = "窗口结束，全部证据保留。请关闭临时常亮，异常时保持接线并在聊天说明；无自动校时或Flash恢复。"
        if profile == "stub-b-sector":
            ending = ("窗口结束，全部证据保留。请关闭临时常亮；不追加复位、校时或恢复写入。" +
                ("本窗原字节恢复和64KiB读回已核验。" if report.get("restored_verified") else
                 "B首4KiB可能已改变，原字节恢复尚未证明；保留接线及本窗前态，在聊天说明。"))
        if profile == "stub-b-execute":
            ending = ("窗口结束，证据保留；关闭临时常亮，不追加复位或校时。" +
                ("原boot64KiB及B64KiB恢复已逐字节核验。" if report.get("restored_verified") else
                 "boot／B可能尚未恢复。保持接线和原文件，需要另一次明确恢复窗口。"))
        console.finish(outcome,ending+("\n"+detail if detail else ""),
                       verification=report["verification"])
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest="mode",required=True)
    for mode in ("simulate","capture"):
        command=sub.add_parser(mode); command.add_argument("--out",type=Path,required=True)
        if mode=="simulate":
            command.add_argument("--before-fixture",type=Path,required=True); command.add_argument("--after-fixture",type=Path,required=True)
            command.add_argument("--scenario",choices=SCENARIOS,default="success")
        else:
            for key in ("result","flash","build-archive","recorded-root"): command.add_argument("--"+key,type=Path,required=True)
            for key in ("expected-bin","port","location"): command.add_argument("--"+key,required=True)
            command.add_argument("--confirm-same-board-and-firmware",action="store_true",required=True)
            command.add_argument("--allow-rom-rescue-reset",action="store_true",required=True)
            command.add_argument("--allow-exit-reset",action="store_true",required=True)
    child=sub.add_parser("worker"); child.add_argument("--config",type=Path,required=True); child.add_argument("--sha",required=True)
    status=sub.add_parser("status"); status.add_argument("--run",type=Path,required=True)
    args=parser.parse_args()
    if args.mode=="worker": return worker(args.config,args.sha)
    if args.mode=="status": print((args.run/"device.json").read_text(encoding="utf-8")); return 0
    if args.mode=="simulate": result=run(args.out,fixtures=dict(before=args.before_fixture.read_bytes(),after=args.after_fixture.read_bytes()),scenario=args.scenario)
    else:
        capture={k:getattr(args,k) for k in ("result","flash","build_archive","recorded_root","expected_bin","port","location")}
        capture={k:str(v) if isinstance(v,Path) else v for k,v in capture.items()}
        capture.update(conditions_confirmed=args.confirm_same_board_and_firmware,rom_reset_authorized=args.allow_rom_rescue_reset)
        capture["exit_reset_authorized"]=args.allow_exit_reset
        result=run(args.out,capture=capture,allow_exit_reset=args.allow_exit_reset)
    print(json.dumps(result,ensure_ascii=False,indent=2)); return 0 if result["passed"] else 1


if __name__=="__main__": raise SystemExit(main())
