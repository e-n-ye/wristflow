"""One guided BAT-only load/recovery window, with explicit firmware provenance."""
from __future__ import annotations

import argparse
import json
import pathlib
import re
import statistics
import threading
import time

from battery_collect import Client, FROZEN, PREFIX, ProtocolError, SessionChanged, SerialTransport, parse_page
from battery_console import Cancelled, Console
from battery_device import ProtectedExport, port_identity, prepare_binding
from battery_lab import Journal, ROOT, digest, load_trace, write_json
from lab_serial import lease_directory


class ExperimentClient(Client):
    def request(self, command, completion, *, cleanup=False):
        if not re.fullmatch(r"wf_pm status|wf_battery (?:status|stop|start 900|dump \d+ \d+ \d+)", command):
            raise ValueError("Experiment permits trace control and observations only; no PM/charger control")
        return super().request(command, completion, cleanup=cleanup)


class Experiment(ProtectedExport):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.owned = self.possible_arm = None
        self.recorder_stopped = False
        self.protected_sha = None

    def protect(self):
        result = super().export("protected")
        self.protected_sha = digest(self.journal.directory / "protected.json")
        return result

    def matching(self, status):
        if self.owned:
            return all(status[k] == self.owned[k] for k in ("session", "start", "duration"))
        return self.possible_arm is not None and all(status[k] == self.possible_arm[k] for k in ("session", "duration")) and (
            status["start"] - self.possible_arm["earliest_start"]) & 0xffffffff <= 5000

    def arm(self):
        if self.possible_arm is not None:
            raise ProtocolError("Start is single-use; never retry arm")
        if not self.protected_sha or digest(self.journal.directory / "protected.json") != self.protected_sha:
            raise ProtocolError("Existing record protection is missing or changed")
        old = self.probe()  # Exact stopped prestate again, immediately before the single start.
        if self.pm_allow != 1:
            raise ProtocolError("Pre-existing PM hold; preserve it")
        self.client = ExperimentClient(self.transport, self.raw, self.journal, self.cancel, self.timeout)
        self.possible_arm = {"session": (old["session"] + 1) & 0xffffffff or 1,
                             "duration": 900000, "earliest_start": old["now"]}
        write_json(self.journal.directory / "ownership.json", {"possible_arm": self.possible_arm,
                   "prestate": old, "protected_sha256": self.protected_sha, "owned": None})
        try:
            status = self.client.control("wf_battery start 900")
        except TimeoutError:
            self.journal.event("start_ack_lost")
            status = self.client.status()  # Observe once; no second start.
        if not self.matching(status) or status["state"] != 1:
            raise SessionChanged("New trace cannot be attributed to the single start")
        self.owned = status
        write_json(self.journal.directory / "ownership.json", {"possible_arm": self.possible_arm,
                   "prestate": old, "protected_sha256": self.protected_sha, "owned": status})
        self.journal.event("armed", status=status)
        self.close()
        return status

    def stop(self, *, cleanup=False):
        status = self.client.status(cleanup=cleanup)
        if not self.matching(status):
            raise SessionChanged("Owned trace changed; no stop sent")
        if status["state"] == 1:
            try:
                stopped = self.client.control("wf_battery stop", cleanup=cleanup)
            except TimeoutError:
                self.journal.event("stop_ack_lost")
                stopped = self.client.status(cleanup=cleanup)
            if not self.matching(stopped) or stopped["count"] < status["count"]:
                raise SessionChanged("Stop identity/count is not confirmed")
            status = stopped
        if status["state"] not in (2, 3, 4):
            raise ProtocolError("Recorder stop is not confirmed")
        self.recorder_stopped = True
        self.journal.event("recorder_stopped", status=status)
        return status

    def collect(self):
        self.open()  # Only called after the operator confirms physical reconnection.
        status = self.stop()
        if not status["count"]:
            raise ProtocolError("No new records")
        records = []
        for offset in range(0, status["count"], 16):
            count = min(16, status["count"] - offset)
            for attempt in range(2):
                try:
                    lines = self.client.request(f"wf_battery dump {status['session']} {offset} {count}",
                        lambda line: line.startswith(PREFIX + "page=") or line.startswith(PREFIX + "rejected") or
                        line.startswith(PREFIX + "aborted"))
                    records.extend(parse_page(lines, status, offset, count))
                    break
                except (ProtocolError, TimeoutError) as exc:
                    self.journal.event("page_rejected", offset=offset, attempt=attempt, error=str(exc))
                    if isinstance(exc, SessionChanged) or attempt:
                        raise
                finally:
                    (self.journal.directory / f"page-{offset:03d}-attempt-{attempt}.bin").write_bytes(self.client.last_bytes)
        value = {"status": status, "records": records}
        write_json(self.journal.directory / "records.json", value)
        load_trace(self.journal.directory / "records.json")
        self.journal.event("export_complete", count=len(records), sha256=digest(self.journal.directory / "records.json"))
        return value

    def __exit__(self, *args):
        # No automatic reconnect while USB is physically absent. The device
        # has a 900s logical deadline; this does not mean stopped was observed.
        if self.possible_arm and self.opened and not self.client.booted:
            try:
                self.stop(cleanup=True)
            except Exception as exc:
                self.journal.event("recorder_cleanup_uncertain", error=str(exc))
        if self.possible_arm and not self.recorder_stopped:
            self.journal.event("recorder_recovery_required", possible_arm=self.possible_arm, owned=self.owned,
                               note="No offline serial reopen; logical expiry is bounded but not observed")
        super().__exit__(*args)


def prepare_fresh_binding(result_path, flash_path, endpoint, location):
    """Bind a successful product flash to current local artifacts, before UART I/O."""
    result = json.loads(pathlib.Path(result_path).read_text(encoding="utf-8-sig"))
    flash = json.loads(pathlib.Path(flash_path).read_text(encoding="utf-8-sig"))
    if (result.get("example") != "product" or result.get("exit_code") != 0 or
            result.get("artifact_validation_passed") is not True or
            flash.get("example") != "product" or flash.get("exit_code") != 0 or
            flash.get("board") != "sf32lb52-lchspi-ulp" or flash.get("port") != endpoint):
        raise ValueError("Successful matching product build and flash are required")
    artifacts = result["artifacts"]
    paths = {pathlib.Path(item["path"]).resolve(): item["sha256"] for item in artifacts}
    firmware = pathlib.Path(result["firmware_path"]).resolve()
    if len(artifacts) != 29 or len(paths) != 29 or firmware not in paths:
        raise ValueError("Product artifact manifest is incomplete or duplicated")
    for path, expected in paths.items():
        if digest(path) != expected:
            raise ValueError("Changed build artifact: " + str(path))
    addresses = {"main.bin": "0x12020000", "bootloader.bin": "0x12010000", "ftab.bin": "0x12000000"}
    images = flash["images"]
    if len(images) != 3 or {image["name"] for image in images} != set(addresses):
        raise ValueError("Flash must identify exactly the three expected images")
    for image in images:
        path = pathlib.Path(image["path"]).resolve()
        if (path.name != image["name"] or image["address"] != addresses[image["name"]] or
                paths.get(path) != image["sha256"] or path.stat().st_size != image["bytes"]):
            raise ValueError("Flash image differs from accepted build")
    if pathlib.Path(next(image["path"] for image in images if image["name"] == "main.bin")).resolve() != firmware:
        raise ValueError("Flashed main image is not the accepted firmware")
    return {"kind": "fresh_flash_battery_experiment", "endpoint": port_identity(endpoint, location),
            "build_result": str(pathlib.Path(result_path).resolve()), "build_result_sha256": digest(result_path),
            "flash_result": str(pathlib.Path(flash_path).resolve()), "flash_result_sha256": digest(flash_path),
            "firmware_sha256": paths[firmware], "device_identity_verified": False,
            "identity_basis": "Operator-confirmed board and successful local flash record; no firmware attestation.",
            "source_hashes": {name: digest(ROOT / "scripts" / name) for name in
                              ("battery_device.py", "battery_collect.py", "battery_console.py", "battery_lab.py", "battery_experiment.py")}}


class FreshFlashExperiment(Experiment):
    """Accept only the initial empty recorder; never reinterpret an old session."""
    def probe(self):
        allowed = self.client.pm("status")
        if self.pm_allow is not None and allowed != self.pm_allow:
            raise SessionChanged("PM allow changed before fresh experiment")
        self.pm_allow = allowed
        status = self.client.status()
        self.journal.event("observed", status=status, pm_allow=allowed)
        if any(status[key] != value for key, value in
               {"schema": 1, "session": 0, "state": 0, "count": 0, "cap": 256, "start": 0, "duration": 0}.items()):
            raise SessionChanged("Fresh flash requires the initial empty recorder; no start/stop sent")
        return status

    def protect(self):
        value = {"status": self.probe(), "records": [], "fresh_after_flash": True}
        write_json(self.journal.directory / "protected.json", value)
        self.protected_sha = digest(self.journal.directory / "protected.json")
        self.journal.event("empty_prestate_saved", sha256=self.protected_sha)
        return value


def analyze(value):
    """Only device-confirmed BAT/discharge samples before the first USB return."""
    rows = value["records"]
    start = next((i for i, r in enumerate(rows) if r["usb_valid"] and not r["usb_good"]), None)
    battery = []
    if start is not None:
        for r in rows[start:]:
            if r["usb_valid"] and r["usb_good"]:
                break
            battery.append(r)
    samples = {}
    for r in battery:
        if r["seq"] and r["raw_valid"] and r["percent_valid"] and r["usb_valid"] and not r["usb_good"] and r["mode"] == 0:
            samples[r["seq"]] = r
    fresh = sorted(samples.values(), key=lambda r: (r["adc"] - value["status"]["start"]) & 0xffffffff)
    white = [r for r in fresh if r["surface"] == 5]
    first_white = next((r["obs"] for r in battery if r["surface"] == 5), None)
    last_white = next((r["obs"] for r in reversed(battery) if r["surface"] == 5), None)
    exit_row = next((r for r in battery if last_white is not None and r["surface"] != 5 and
                     0 < (r["obs"] - last_white) & 0xffffffff < 900000), None)
    def stats(group):
        return {"unique_adc": len(group), "mv_median": statistics.median(r["mv"] for r in group) if group else None,
                "mv_min": min((r["mv"] for r in group), default=None), "mv_max": max((r["mv"] for r in group), default=None),
                "display_percent": sorted({r["percent"] for r in group}),
                "candidate_percent": sorted({r["candidate"] for r in group}),
                "brightness_ack": sorted({r["ack"] for r in group})}
    before = [r for r in fresh if first_white is not None and (first_white-r["obs"]) & 0xffffffff < 900000 and r["surface"] != 5]
    post = [r for r in fresh if exit_row and (r["adc"] - exit_row["obs"]) & 0xffffffff < 900000]
    bands = {f"after_{lo}_{hi}s": stats([r for r in post if lo*1000 <=
             (r["adc"]-exit_row["obs"]) & 0xffffffff < hi*1000]) for lo, hi in ((0, 30), (30, 60), (60, 120), (120, 180), (180, 300))}
    def span(group):
        return (group[-1]["adc"]-group[0]["adc"]) & 0xffffffff if len(group) >= 2 else 0
    normal_ack = sorted({r["ack"] for r in before})
    post_ack = sorted({r["ack"] for r in post})
    unknown = sum(not r["usb_valid"] for r in battery)
    off = sum(r["phase"] == 2 for r in battery)
    # Sampling quantization permits 10s below each instructed segment. This is
    # observed window support only, not continuous current/energy measurement.
    checks = {"baseline_at_least_80s": span(before) >= 80000,
              "flashlight_at_least_80s": span(white) >= 80000,
              "recovery_and_control_at_least_260s": span(post) >= 260000,
              "normal_brightness_unchanged": len(normal_ack) == 1 and normal_ack == post_ack,
              "flashlight_ack100": bool(white) and all(r["ack"] == 100 for r in white),
              "no_off_or_unknown_pg_inside_bat_window": off == 0 and unknown == 0,
              "recorder_not_full": value["status"]["state"] != 4}
    return {"new_hardware_data": True, "soc_or_consumed_charge_measured": False,
            "battery_observation_span_ms": (battery[-1]["obs"]-battery[0]["obs"]) & 0xffffffff if battery else 0,
            "battery_unique_adc": len(fresh), "unknown_pg_rows": unknown,
            "off_rows": off, "before_flashlight": stats(before),
            "flashlight": stats(white), "recovery": bands,
            "white_context_span_ms": (last_white-first_white) & 0xffffffff if white else 0,
            "exit_observed_ms": exit_row["obs"] if exit_row else None,
            "load_window_observed": bool(before and white and post),
            "observed_window_checks": checks, "observed_window_checks_met": all(checks.values()),
            "limits": "Only raw voltage/estimator/observed load context. No current integration, SOC calibration or matched-energy conclusion."}


def countdown(console, key, message, seconds):
    console.automatic(key, message, seconds)


def run(expected, binding, output, *, port=4444, timeout=900):
    fresh = binding["kind"] == "fresh_flash_battery_experiment"
    journal = Journal(output, binding=binding, mode="guided_battery_experiment")
    phases = [("prepare", "准备设备"), ("protect", "核对空记录器" if fresh else "备份已有记录"), ("arm", "启动新记录"),
              ("unplug", "拔USB确认"), ("baseline", "普通页90秒"), ("flashlight", "手电筒90秒"),
              ("recovery", "恢复180秒"), ("control", "普通页对照90秒"), ("reconnect", "接回USB"),
              ("export", "导出并分析"), ("cleanup", "释放资源")]
    transport = SerialTransport(binding["endpoint"]["device"])
    def guard():
        if port_identity(binding["endpoint"]["device"], binding["endpoint"]["location"]) != binding["endpoint"]:
            raise SessionChanged("Host endpoint changed")
    with Console(journal, phases, port, mode="实机纯电池负载／恢复对照") as console:
        collector_type = FreshFlashExperiment if fresh else Experiment
        collector = collector_type(journal, transport, console.cancelled, lease_directory(ROOT),
                               expected, identity_guard=guard)
        current, outcome, detail, analysis = "prepare", "passed", "", None
        print(f"Guided battery experiment: {console.url}", flush=True)
        try:
            console.prompt(current, "保持原电池＋电脑USB，正常亮度不改。请开启临时持续亮屏10分钟，回设置→电池页后点确认。" +
                           ("本次已更新固件，将核对空记录器后启动新测试；旧磁盘记录保留。" if fresh else
                            "先备份已有记录，再用新实验替换板上旧RAM记录；磁盘备份保留。"), timeout)
            console.stage(current, "complete")
            with collector:
                current = "protect"; console.stage(current, "running")
                collector.protect(); console.stage(current, "complete")
                current = "arm"; console.stage(current, "running")
                collector.arm(); console.stage(current, "complete")
                current = "unplug"
                console.prompt(current, "新记录已启动，串口已关闭。现在拔掉USB，只保留电池，保持电池页，再点确认开始计时。"
                               "后面请一直保持USB拔出，跟随网页提示。", 120)
                console.stage(current, "complete")
                current = "baseline"; countdown(console, current, "保持普通电池页90秒；USB保持拔出，不改亮度。结束后页面会提示进入手电筒。", 90)
                current = "flashlight"
                console.prompt(current, "现在在手表上打开手电筒，完成后点确认。随后保持90秒；USB继续拔出。", 60)
                countdown(console, current, "保持手电筒90秒。不要接USB；结束后页面会提示退出。", 90)
                current = "recovery"
                console.prompt(current, "现在退出手电筒，回到设置→电池页，完成后点确认。保持原亮度，USB继续拔出。", 60)
                countdown(console, current, "保持电池页，观察恢复180秒，不需要抄数字。USB继续拔出。", 180)
                current = "control"; countdown(console, current, "继续保持同一电池页90秒，作为普通负载对照；USB继续拔出。", 90)
                current = "reconnect"
                console.prompt(current, "纯电池观察结束。现在接回电脑USB，保持电池页，等Windows连接稳定后点确认。程序将停止记录并导出；接USB后的样本单列。", 120)
                console.stage(current, "complete")
                current = "export"; console.stage(current, "running")
                value = collector.collect(); analysis = analyze(value)
                write_json(journal.directory / "analysis.json", analysis)
                console.stage(current, "complete")
            current = "cleanup"; console.stage(current, "complete" if collector.cleanup_confirmed and collector.recorder_stopped else "error")
            if not collector.cleanup_confirmed or not collector.recorder_stopped:
                raise ProtocolError("Host cleanup or owned recorder stop is not confirmed")
        except Cancelled as exc:
            outcome, detail = "cancelled", str(exc)
        except TimeoutError as exc:
            outcome, detail = "timeout", str(exc)
        except Exception as exc:
            outcome, detail = "error", str(exc)
        if outcome != "passed":
            console.stage(current, outcome)
        collection_completed = outcome == "passed"
        if collection_completed and (analysis is None or not analysis["observed_window_checks_met"]):
            outcome, detail = "error", "采集已完成，但设备观测窗口检查有缺项；不记实验通过。"
        outcome = console.seal(outcome)
        report = {"kind": "guided_battery_experiment", "outcome": outcome,
                  "collection_completed": collection_completed,
                  "passed": outcome == "passed" and analysis is not None and analysis["observed_window_checks_met"],
                  "host_resources_closed": collector.cleanup_confirmed, "recorder_stopped_confirmed": collector.recorder_stopped,
                  "analysis": analysis, "detail": detail, "soc_accuracy_verified": False}
        write_json(journal.directory / "collection.json", report)
        journal.event("finished", outcome=outcome, passed=report["passed"], report_sha256=digest(journal.directory / "collection.json"))
        console.finish(outcome, ("记录已保存；" + ("窗口检查齐全。" if report["passed"] else "窗口检查有缺项，不记实验通过。") +
                       "接下来由助手分析。请关闭临时持续亮屏，保持原供电。" if outcome == "passed" else
                       "请关闭临时持续亮屏。" + ("记录已保存，设备停止已确认。" if collector.recorder_stopped else
                       "若正在纯电池运行，请接回USB并告知助手；设备记录停止尚未确认。")) + detail)
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port", required=True)
    parser.add_argument("--usb-location", required=True)
    parser.add_argument("--build-result", required=True, type=pathlib.Path)
    provenance = parser.add_mutually_exclusive_group(required=True)
    provenance.add_argument("--expected-trace", type=pathlib.Path)
    provenance.add_argument("--flash-result", type=pathlib.Path, help="Successful fresh product flash; requires empty recorder")
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--port", type=int, default=4444)
    args = parser.parse_args()
    if not 0 <= args.port <= 65535: parser.error("Invalid loopback port")
    if args.flash_result:
        binding = prepare_fresh_binding(args.build_result, args.flash_result, args.serial_port, args.usb_location)
        expected = None
    else:
        binding = prepare_binding(args.build_result, args.expected_trace, args.serial_port, args.usb_location)
        binding["kind"] = "guided_battery_experiment"
        expected = load_trace(args.expected_trace)
    binding["source_hashes"]["battery_experiment.py"] = digest(__file__)
    result = run(expected, binding, args.out, port=args.port)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
