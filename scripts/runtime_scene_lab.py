"""Guided BAT scene windows; simulation by default, existing firmware only."""
import argparse
import json
from pathlib import Path
import threading
import time

from battery_console import Console, Cancelled
from battery_lab import Journal, load_trace
from battery_collect import Client, FROZEN, PREFIX, parse_page, parse_status, trace_lines
from lab_evidence import file_record, digest, verify_files, write_json
from lab_serial import RecordedSession, SerialTransport, ProtocolError, SessionChanged, lease_directory
from serial_lab import prepare_capture
from runtime_export import collect as export_runtime
from device_snapshot import collect as export_cache

SCENES = {"on": (1, "普通亮屏"), "flashlight": (2, "手电筒"), "off": (4, "OFF息屏")}


class TraceClient(Client):
    COMMAND_PATTERN = r"wf_pm status|wf_battery (?:status|start 900|stop|dump \d+ \d+ \d+)"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL", b"RT-Thread")

    def status(self, *, cleanup=False):
        lines = self.request("wf_battery status", lambda s: s.startswith(PREFIX+"start="), cleanup=cleanup)
        return parse_status(trace_lines(lines))


class Trace:
    """One single-use RAM trace; protect before arm, stop only matched ownership."""
    def __init__(self, transport, binding, identity, locks, cancel, timeout=6, expected=None):
        self.transport, self.binding, self.identity, self.locks, self.cancel = transport, binding, identity, locks, cancel
        self.possible = self.owned = None
        self.stopped = False
        self.stop_sent = False
        self.timeout = timeout
        self.expected = expected

    def matches(self, s):
        if self.owned:
            return all(s[k] == self.owned[k] for k in ("session", "start", "duration"))
        p = self.possible
        return bool(p and s["session"] == p["session"] and s["duration"] == 900000 and
                    (s["start"]-p["now"]) % 2**32 <= 5000)

    def dump(self, client, status, journal, name):
        rows = []
        for offset in range(0, status["count"], 4):
            count = min(4, status["count"]-offset)
            for attempt in range(2):
                try:
                    if attempt:
                        observed = client.status()
                        if any(observed[k] != status[k] for k in FROZEN):
                            raise SessionChanged("Frozen trace changed; no page retry")
                    lines = client.request(f"wf_battery dump {status['session']} {offset} {count}",
                        lambda s: s.startswith(PREFIX+"page=") or s.startswith(PREFIX+"rejected") or s.startswith(PREFIX+"aborted"))
                    page = parse_page(lines, status, offset, count)
                    rows.extend(page)
                    break
                except (ProtocolError, TimeoutError) as exc:
                    journal.event("page_rejected", offset=offset, attempt=attempt, error=repr(exc))
                    if isinstance(exc, SessionChanged) or attempt:
                        raise
                finally:
                    (journal.directory/f"{name}-page-{offset:03d}-attempt-{attempt}.bin").write_bytes(bytes(client.last_bytes))
        value = {"status": status, "records": rows}
        path = journal.directory/name
        write_json(path, value)
        if rows:
            load_trace(path)
        return value

    def cleanup(self, client, journal):
        if not self.possible or client.booted:
            return
        try:
            s = client.status(cleanup=True)
            if not self.matches(s):
                raise SessionChanged("Foreign RAM trace; no cleanup stop")
            if s["state"] == 1 and not self.stop_sent:
                self.stop_sent = True
                try:
                    client.control("wf_battery stop", cleanup=True)
                except TimeoutError:
                    journal.event("cleanup_stop_ack_lost")
                s = client.status(cleanup=True)  # One observation; never repeat stop.
            self.stopped = self.matches(s) and s["state"] in (2, 3, 4)
        except Exception as exc:
            journal.event("trace_cleanup_uncertain", error=repr(exc))

    def arm(self, out):
        if self.possible:
            raise ProtocolError("Trace arm is single-use")
        journal = Journal(out, binding=self.binding, mode="scene-trace-arm")
        with RecordedSession(journal, self.transport, self.cancel, TraceClient,
                             identity=self.identity, locks=self.locks, timeout=self.timeout) as client:
            if client.pm("status") != 1:
                raise ProtocolError("Pre-existing PM hold preserved")
            old = client.status()
            if old["state"] == 1 or (old["state"] == 0 and any(old[k] for k in ("session", "count", "start", "duration"))):
                raise ProtocolError("Existing running/untrusted trace preserved")
            protected = self.dump(client, old, journal, "protected.json")
            if self.expected and (any(old[k] != self.expected["status"][k] for k in FROZEN) or
                                  protected["records"] != self.expected["records"]):
                raise SessionChanged("Existing trace differs from prior complete export; no start")
            saved = digest(out/"protected.json")
            again = client.status()
            if any(old[k] != again[k] for k in FROZEN) or digest(out/"protected.json") != saved:
                raise SessionChanged("Protected RAM trace changed; no start")
            self.possible = {"session": (again["session"]+1) % 2**32 or 1, "now": again["now"]}
            write_json(out/"ownership.json", {"possible": self.possible, "protected_sha256": saved})
            try:
                new = client.control("wf_battery start 900")
                if not self.matches(new) or new["state"] != 1:
                    raise SessionChanged("New RAM trace attribution failed")
                self.owned = new
                write_json(out/"ownership.json", {"possible": self.possible, "owned": new, "protected_sha256": saved})
            except BaseException:
                self.cleanup(client, journal)
                raise

    def finish(self, out):
        if not self.owned:
            raise ProtocolError("No confirmed owned trace")
        journal = Journal(out, binding=self.binding, mode="scene-trace-finish")
        with RecordedSession(journal, self.transport, self.cancel, TraceClient,
                             identity=self.identity, locks=self.locks, timeout=self.timeout) as client:
            try:
                s = client.status()
                if not self.matches(s):
                    raise SessionChanged("Foreign trace preserved; no stop")
                if s["state"] == 1:
                    if self.stop_sent:
                        raise ProtocolError("Single stop already sent; preserve uncertainty")
                    self.stop_sent = True
                    try:
                        client.control("wf_battery stop")
                    except TimeoutError:
                        journal.event("stop_ack_lost")
                    s = client.status()  # Independently confirm the single stop.
                if not self.matches(s) or s["state"] not in (2, 3, 4):
                    raise ProtocolError("Owned trace freeze not confirmed")
                self.stopped = True
                return self.dump(client, s, journal, "records.json")
            except BaseException:
                self.cleanup(client, journal)
                raise


def analyze(scene, seconds, before, after, trace):
    a, b = before["current"], after["current"]
    if a["boot"] != b["boot"] or b["total_recorded_ms"] < a["total_recorded_ms"]:
        raise SessionChanged("Recorder boot/position changed")
    delta = {k: b["scene_ms"][k]-a["scene_ms"][k] for k in a["scene_ms"]}
    if min(delta.values()) < 0:
        raise SessionChanged("Scene counters went backwards")
    checks = {"recording": bool(a["flags"] & b["flags"] & 2),
              "no_save_failure": not before["failed"] and not after["failed"] and b["failures"] == a["failures"],
              "target_duration_supported": delta[scene] >= seconds*1000-2000,
              "no_new_fault": b["faults"] == a["faults"],
              "no_active_fault": not a["fault_mask"] and not b["fault_mask"]}
    rows = [x for x in trace["records"] if x["usb_valid"] and not x["usb_good"] and x["charge"] in (1, 4) and
            x["raw_valid"] and x["seq"] and x["age"] <= 15000 and x["mode"] == 0]
    target = [x for x in rows if x["phase"] == 0 and (x["surface"] == 5) == (scene == "flashlight")]
    start = target[0] if target else None
    if start:
        # Exclude cached ADC from before entering the observed load and initial settling.
        origin = start["obs"]
        target = [x for x in target if 5000 <= (x["adc"]-origin) % 2**32 < 900000]
        start = target[0] if target else None
    samples = [x for x in after["samples"] if x["flags"] & 1 and x["flags"] & 4 and not x["flags"] & 8 and x["charge"] in (1, 4) and
               (x["ms"] % 2**32-x["sampled_ms"]) % 2**32 <= 15000 and
               0 <= b["last"]["ms"]-x["ms"] <= 120000 and
               x["scene"] == (1 if scene == "off" else SCENES[scene][0])]
    end = samples[-1] if samples else None
    return {"scene": scene, "checks": checks, "recorder_checks_passed": all(checks.values()),
            "scene_delta_ms": delta, "before_generation": a["generation"], "after_generation": b["generation"],
            "saved_successes_delta": b["successes"]-a["successes"], "unsaved_recorded_ms": after["unsaved_recorded_ms"],
            "voltage_start": start, "voltage_end": end,
            "voltage_endpoints_available": bool(start and end),
            "limits": "Counter delta includes preparation/reconnection; not exact isolated BAT duration or continuous scene proof. RAM trace expires900s after USB arm, before the full offline window ends. End uses bounded recent runtime samples; OFF endpoints are awake readings. No SOC, charge, current or battery-life accuracy claim."}


class DeviceBackend:
    def __init__(self, args, cancel):
        binding, identity = prepare_capture(args.result, args.flash, args.expected_bin, args.port, args.location,
                                            args.confirmed, source_archive=args.source_archive)
        sources = [file_record(Path(__file__))]
        self.expected = load_trace(args.protected_trace) if args.protected_trace else None
        if args.protected_trace:
            sources.append(file_record(args.protected_trace))
        if args.restore_cache:
            sources.append(file_record(args.restore_cache))
        self.binding, self.args, self.cancel = dict(binding, adapter=sources[0], adapter_inputs=sources,
            restore_cache=file_record(args.restore_cache) if args.restore_cache else None), args, cancel
        def guarded():
            verify_files(sources)
            return identity()
        self.identity, self.locks, self.sources = guarded, lease_directory(), sources
        self.trace = None

    def snapshot(self, out):
        report = export_runtime(out, SerialTransport(self.args.port), binding=self.binding,
            identity_check=self.identity, locks=self.locks, cancel=self.cancel, seconds=45)
        if not report["passed"]:
            raise ProtocolError("Runtime export failed; raw bytes preserved")
        return json.loads((out/"records.json").read_text(encoding="utf-8"))

    def cache(self, out):
        report = export_cache(out, SerialTransport(self.args.port), binding=self.binding,
            identity_check=self.identity, locks=self.locks, cancel=self.cancel, seconds=30)
        if not report["passed"]:
            raise ProtocolError("Business cache readback failed")
        return report

    def arm(self, out):
        self.trace = Trace(SerialTransport(self.args.port), self.binding, self.identity, self.locks, self.cancel, expected=self.expected)
        self.trace.arm(out)

    def finish(self, out):
        value = self.trace.finish(out)
        if not value["records"]:
            raise ProtocolError("No RAM diagnostic observations; preserve stopped trace")
        self.expected = value
        self.sources.append(file_record(out/"records.json"))
        return value


class SimulationBackend:
    binding = {"simulation": True, "hardware_verified": False}
    trace = None
    def __init__(self, args, *unused):
        self.n = 0
        self.seconds = args.seconds
        self.durations = {x: 0 for x in ("unknown", "on", "flashlight", "dim", "off", "aod")}
    def snapshot(self, out):
        out.mkdir(parents=True, exist_ok=False)
        self.n += 1
        if not self.n % 2:
            self.durations[self.scene] += int(self.seconds*1000)
        s = {"boot": 2, "generation": self.n, "flags": 2, "failures": 0, "faults": 0, "fault_mask": 0,
             "successes": self.n, "total_recorded_ms": sum(self.durations.values()),
             "scene_ms": dict(self.durations), "last": {"ms": sum(self.durations.values())}}
        d = {"current": s, "failed": False, "samples": [], "unsaved_recorded_ms": 1000}
        write_json(out/"records.json", d)
        return d
    def cache(self, out):
        out.mkdir(parents=True, exist_ok=False)
        d = {"simulation": True, "passed": True, "resources_closed": True}
        write_json(out/"snapshot.json", d)
        return d
    def arm(self, out):
        out.mkdir(parents=True, exist_ok=False)
        write_json(out/"ownership.json", {"simulation": True})
    def finish(self, out):
        out.mkdir(parents=True, exist_ok=False)
        d = {"records": [], "simulation": True}
        write_json(out/"records.json", d)
        return d


def run(args):
    scenes = args.scenes or ["on", "flashlight", "off"]
    if len(scenes) != len(set(scenes)) or any(x not in SCENES for x in scenes):
        raise ValueError("Unique known scenes required")
    simulated = args.port is None
    if not (0 < args.seconds <= 900) or (not simulated and args.seconds < 60):
        raise ValueError("Device scene windows60..900 seconds")
    if not simulated and not all((args.result, args.flash, args.expected_bin, args.source_archive, args.confirmed)):
        raise ValueError("Explicit build/flash/archive/hash and current field confirmation required")
    cancel = threading.Event()
    backend = (SimulationBackend if simulated else DeviceBackend)(args, cancel)
    phases = [("prepare", "保存原始偏好与前态")]
    for scene in scenes:
        phases += [(scene+"-"+k, SCENES[scene][1]+"："+t) for k, t in
                   [("prepare", "设置并备份"), ("unplug", "拔USB"), ("settle", "电池供电起点"), ("enter", "进入场景"),
                    ("timer", "自动计时"), ("end", "结束并保留电池末样"), ("reconnect", "接回USB"), ("read", "导出核验")]]
    phases += [("restore", "保留设置并确认设备" if args.keep_settings else "恢复原偏好并关闭常亮")]
    journal = Journal(args.out, binding=dict(backend.binding, adapter=file_record(Path(__file__)),
        scenes=scenes, seconds=args.seconds), mode="runtime-scenes")
    results, outcome, detail, current = [], "passed", "", "prepare"
    with Console(journal, phases, mode="三场景模拟（无设备）" if simulated else "三场景纯电池记录验收") as console:
        backend.cancel = console.cancelled
        print(console.url, flush=True)
        def prompt(key, text):
            nonlocal current
            current = key
            console.prompt(key, text, 300)
            console.stage(key, "complete")
        try:
            prompt("prepare", ("按已确认选择保留测试设置；先前原偏好缓存仅作证据保留。" if args.keep_settings else
                "接续剩余场景：恢复目标仍是第一次测试前记下的原偏好，勿把当前测试设置当作原设置。" if args.restore_cache else
                "保持原电池＋USB；记住原亮度、自动亮度、抬腕、AOD、勿扰设置。")+
                "先开启临时常亮10分钟，回数字表盘，确认后保存当前缓存。不重启、不校时。")
            backend.cache(args.out/"initial-cache")
            for scene in scenes:
                backend.scene = scene
                title = SCENES[scene][1]
                prompt(scene+"-prepare", "USB保持连接。手动亮度设50，关闭自动亮度、抬腕和AOD，开启勿扰；蓝牙保持原状态，各组相同。重新选择持续亮屏20分钟，回设置→电池页。准备好后确认，先导出长期记录并保护旧RAM诊断，再单次启动900秒RAM诊断；长期记录器保持运行。")
                group = args.out/scene
                before = backend.snapshot(group/"before")
                if before["failed"] or not before["current"]["flags"] & 2 or before["current"]["fault_mask"]:
                    raise ProtocolError("Long recorder is stopped or failed; preserve it")
                backend.arm(group/"trace-arm")
                armed_at = time.monotonic()
                prompt(scene+"-unplug", "串口已关闭。现在拔掉USB，只留原电池，保持电池页；确认已经拔线后点确认。全程不要改电池接线。")
                current = scene+"-settle"
                console.automatic(current, "USB继续拔出，保持电池页，等待已有采样形成电池供电起点。", min(10, args.seconds))
                instructions = {"on": "保持同一电池页、亮度50及常亮，不再操作。",
                    "flashlight": "打开手电筒，白屏显示后再确认。",
                    "off": "关闭临时常亮，保持AOD／抬腕关闭，等屏幕自然完全熄灭后再确认。"}
                prompt(scene+"-enter", "USB继续拔出。"+instructions[scene]+"点击确认后才开始本组自动计时。")
                if time.monotonic()-armed_at > 120:
                    raise TimeoutError("Battery setup exceeded120s; do not mislabel a shortened trace/window")
                current = scene+"-timer"
                console.automatic(current, f"保持{title}，USB继续拔出。程序自动计时{args.seconds:g}秒；按钮不能跳过。若意外亮屏／退出场景或有异常，请取消并告知助手。", args.seconds)
                prompt(scene+"-end", "计时完成，USB仍保持拔出。"+("按KEY1唤醒，" if scene == "off" else "退出手电筒，" if scene == "flashlight" else "")+"回电池页，等电压获得新读数（约10秒）后确认。OFF末样仅解释为唤醒后的读数。")
                prompt(scene+"-reconnect", "现在开启临时常亮10分钟，接回原电脑USB端口，等Windows连接稳定，保持电池页亮着后确认。只有本次确认后才重开串口。")
                current = scene+"-read"; console.stage(current, "running")
                trace = backend.finish(group/"trace-end")
                after = backend.snapshot(group/"after")
                result = analyze(scene, args.seconds, before, after, trace)
                write_json(group/"analysis.json", result); results.append(result)
                if not result["recorder_checks_passed"]:
                    raise ProtocolError("Recorder scene checks incomplete; preserve evidence")
                console.stage(current, "complete")
            prompt("restore", "各组导出已完成，串口均已关闭。"+
                ("按你的选择保留当前设置；" if args.keep_settings else "请恢复测试前亮度、自动亮度、抬腕、AOD和勿扰，最后关闭临时常亮；")+
                "确认显示、触摸、时间正常且没有保存失败提示。之后不再发送设备命令。")
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
            detail = repr(exc); journal.event("experiment_failed", phase=current, error=detail)
            console.stage(current, outcome)
        outcome = console.seal(outcome)
        report = {"outcome": outcome, "passed": outcome == "passed", "simulation": simulated,
            "hardware_verified": False, "results": results, "detail": detail,
            "owned_trace_stopped": None if backend.trace is None or backend.trace.possible is None else backend.trace.stopped,
            "recovery_required": bool(backend.trace and backend.trace.possible and not backend.trace.stopped),
            "limits": "Workflow and recorder observations only. Voltage is auxiliary; no SOC/current/battery-life verification. No long-recorder control, reset, time/PM/charger control or flash."}
        write_json(args.out/"experiment.json", report)
        console.finish(outcome, "本轮已保存。"+(("按选择保留设置，设备动作结束。" if args.keep_settings else "恢复原偏好后设备动作结束。") if outcome == "passed" else "若USB已拔出，请接回并告知助手；勿重启或重刷。RAM诊断有900秒逻辑截止，但停止状态可能尚未读回。")+detail)
        return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=900)
    p.add_argument("--scenes", choices=tuple(SCENES), action="append")
    p.add_argument("--port"); p.add_argument("--location", default="1-1.1")
    for option in ("result", "flash", "source-archive"):
        p.add_argument("--"+option, type=Path)
    p.add_argument("--expected-bin"); p.add_argument("--confirmed", action="store_true")
    p.add_argument("--protected-trace", type=Path, help="Prior complete frozen RAM trace; must match before replacing it")
    p.add_argument("--restore-cache", type=Path, help="Initial pre-test cache from an earlier window; retain original preference restoration target")
    p.add_argument("--keep-settings", action="store_true", help="Operator chose to retain test settings; do not request preference restoration")
    args = p.parse_args()
    return 0 if run(args)["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
