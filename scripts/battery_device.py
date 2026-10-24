"""Protect an existing stopped battery trace; no arm/reset/PM-control commands."""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import re

from battery_collect import (Client, FROZEN, PREFIX, PortLease, ProtocolError,
                             SerialTransport, SessionChanged, parse_page)
from battery_console import Cancelled, Console
from battery_lab import Journal, ROOT, digest, load_trace, write_json
from lab_serial import lease_directory


class ReadOnlyClient(Client):
    def request(self, command, completion, *, cleanup=False):
        if not re.fullmatch(r"wf_pm status|wf_battery (?:status|dump \d+ \d+ \d+)", command):
            raise ValueError("Protected export permits status/dump only")
        return super().request(command, completion, cleanup=cleanup)


def port_identity(endpoint, location):
    """Read host enumeration/driver configuration without opening the UART."""
    from serial.tools import list_ports
    if os.name != "nt" or not re.fullmatch(r"COM[1-9]\d*", endpoint):
        raise ValueError("This bounded device pilot requires a canonical Windows COM port")
    matches = [p for p in list_ports.comports() if p.device == endpoint]
    if len(matches) != 1:
        raise ValueError("Serial endpoint is missing or ambiguous")
    item = matches[0]
    if (item.vid, item.pid, item.location) != (0x1a86, 0x7523, location):
        raise ValueError("USB identity/location differs from the confirmed CH340 endpoint")
    import winreg
    settings = []
    with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE,
                        r"SYSTEM\CurrentControlSet\Enum\USB\VID_1A86&PID_7523") as key:
        for index in range(winreg.QueryInfoKey(key)[0]):
            child = winreg.EnumKey(key, index)
            with winreg.OpenKey(key, child + r"\Device Parameters") as device:
                if winreg.QueryValueEx(device, "PortName")[0] == endpoint:
                    settings.append(winreg.QueryValueEx(device, "DisableModemOutHandShake")[0])
    if settings != [1]:
        raise ValueError("Previously validated DisableModemOutHandShake=1 is not confirmed")
    return {"device": item.device, "vid": item.vid, "pid": item.pid, "location": item.location,
            "serial_number": item.serial_number, "hwid": item.hwid, "disable_modem": 1}


def prepare_binding(result_path, expected_path, endpoint, location):
    expected = load_trace(expected_path)
    result = json.loads(pathlib.Path(result_path).read_text(encoding="utf-8-sig"))
    if result["exit_code"] != 0 or not result["artifact_validation_passed"]:
        raise ValueError("Firmware build was not accepted")
    bin_hash = digest(result["firmware_path"])
    if bin_hash != expected["source"]["bin_sha256"]:
        raise ValueError("Local firmware differs from the expected trace's BIN")
    historical = json.loads((ROOT / expected["source"]["evidence"]).read_text(encoding="utf-8"))
    artifacts = result["artifacts"]
    paths = [pathlib.Path(item["path"]).resolve() for item in artifacts]
    firmware = pathlib.Path(result["firmware_path"]).resolve()
    if (len(artifacts) != historical["preflight"]["artifacts"] or len(paths) != len(set(paths)) or
            firmware not in paths):
        raise ValueError("Build artifact manifest is incomplete or duplicated")
    if pathlib.Path(result_path).resolve() != pathlib.Path(historical["preflight"]["build"]).resolve():
        raise ValueError("Build result differs from the accepted historical build")
    for item in result["artifacts"]:
        if digest(item["path"]) != item["sha256"]:
            raise ValueError("Changed build artifact: " + item["path"])
    actual_port = port_identity(endpoint, location)
    if not any(all(actual_port[key] == previous[key] for key in ("device", "vid", "pid", "location"))
               for previous in historical["preflight"]["ports"]):
        raise ValueError("Endpoint differs from the accepted historical port")
    return {"kind": "protected_device_export", "endpoint": actual_port,
            "build_result": str(pathlib.Path(result_path).resolve()), "build_result_sha256": digest(result_path),
            "firmware_sha256": bin_hash, "expected_trace_sha256": digest(expected_path),
            "expected_status": expected["status"], "device_identity_verified": False,
            "identity_basis": "Operator confirms same board/BIN and uninterrupted supply; host USB metadata and local artifacts match. No firmware attestation protocol.",
            "commands": ["wf_pm status", "wf_battery status", "wf_battery dump"],
            "source_hashes": {name: digest(ROOT / "scripts" / name) for name in
                              ("battery_device.py", "battery_collect.py", "battery_console.py", "battery_lab.py")}}


class ProtectedExport:
    """Observes an existing session without acquiring recorder or PM ownership."""
    def __init__(self, journal, transport, cancel, lease_directory, expected, timeout=2, identity_guard=None):
        self.journal, self.transport, self.cancel = journal, transport, cancel
        self.lease = PortLease(transport.port, lease_directory)
        self.expected, self.timeout = expected, timeout
        self.identity_guard = identity_guard
        self.opened = False
        self.cleanup_confirmed = False
        self.cleanup_attempted = False
        self.pm_allow = None
        self.raw = None

    def __enter__(self):
        self.lease.__enter__()
        try:
            self.raw = (self.journal.directory / "uart.bin").open("xb")
            self.client = ReadOnlyClient(self.transport, self.raw, self.journal, self.cancel, self.timeout)
            self.open()
            return self
        except BaseException:
            self.__exit__()
            raise

    def open(self):
        if self.opened:
            raise RuntimeError("Transport is already open")
        if self.cancel.is_set():
            raise Cancelled("Cancelled before opening transport")
        if self.identity_guard is not None:
            self.identity_guard()
        if self.cancel.is_set():
            raise Cancelled("Cancelled during endpoint validation")
        self.transport.open()
        self.opened = True
        self.journal.event("transport_open", endpoint=self.transport.port, reset=False)

    def close(self):
        if self.opened:
            try:
                self.client.last_bytes = bytearray()
                if not self.client.booted:
                    self.client.drain()
            finally:
                self.transport.close()
                self.opened = False
                self.journal.event("transport_closed")

    def probe(self):
        allowed = self.client.pm("status")
        if self.pm_allow is not None and allowed != self.pm_allow:
            raise SessionChanged("PM allow changed despite read-only export")
        self.pm_allow = allowed
        status = self.client.status()
        self.journal.event("observed", status=status, pm_allow=allowed)
        if (status["state"] not in (2, 3, 4) or not status["count"] or
                any(status[key] != self.expected["status"][key] for key in FROZEN)):
            raise SessionChanged("Existing stopped trace does not match expected identity; no start/stop sent")
        return status

    def export(self, name):
        if name not in ("protected", "reopened"):
            raise ValueError("Unknown export name")
        status = self.probe()
        records = []
        for offset in range(0, status["count"], 16):
            count = min(16, status["count"] - offset)
            for attempt in range(2):
                try:
                    lines = self.client.request(f"wf_battery dump {status['session']} {offset} {count}",
                        lambda line: line.startswith(PREFIX + "page=") or
                        line.startswith(PREFIX + "rejected") or line.startswith(PREFIX + "aborted"))
                    records.extend(parse_page(lines, status, offset, count))
                    break
                except (ProtocolError, TimeoutError) as exc:
                    self.journal.event("page_rejected", export=name, offset=offset, attempt=attempt, error=str(exc))
                    if isinstance(exc, SessionChanged) or attempt:
                        raise
                finally:
                    (self.journal.directory / f"{name}-page-{offset:03d}-attempt-{attempt}.bin").write_bytes(self.client.last_bytes)
        value = {"status": status, "records": records}
        path = self.journal.directory / (name + ".json")
        write_json(path, value)  # Preserve the observation even if historical comparison fails.
        load_trace(path)
        if records != self.expected["records"]:
            raise SessionChanged("Observed records differ from protected historical evidence")
        self.probe()
        self.journal.event("export_complete", export=name, records=len(records), sha256=digest(path))
        return value

    def __exit__(self, *args):
        self.cleanup_attempted = True
        errors = []
        try:
            self.close()
        except Exception as exc:
            errors.append(str(exc))
        finally:
            try:
                # Also close a transport whose open raised after partially acquiring the handle.
                if not self.opened:
                    self.transport.close()
            except Exception as exc:
                errors.append(str(exc))
            finally:
                try:
                    if self.raw is not None:
                        self.raw.close()
                finally:
                    self.lease.__exit__()
        if hasattr(self, "client") and self.client.booted:
            errors.append("Cold boot observed; RAM continuity invalid")
        self.cleanup_confirmed = not errors
        self.journal.event("cleanup", confirmed=self.cleanup_confirmed, errors=errors,
                           no_recorder_or_pm_control=True)


def verification_checks(report):
    """Expected cancellation is a verified check, separate from run termination."""
    return [{"title": title, "passed": bool(passed)} for title, passed in (
        ("设备准备已确认", report.get("preparation_confirmed", report["protected_export_matched"])),
        ("已有记录已保护并完整导出", report["protected_export_matched"]),
        ("关重开后记录一致", report["reopened_export_matched"]),
        ("取消入口已触发并完成清理", report["cancel_cleanup_observed"]),
        ("串口和资源已关闭", report["cleanup_confirmed"]))]


def run_export(expected, binding, output, *, port=4444, timeout=300, transport=None, scripted=None):
    journal = Journal(output, binding=binding, mode="protected_device_export")
    phases = [("prepare", "确认设备准备"), ("export", "保护并导出现有记录"),
              ("reopen", "关重开后核对记录"), ("cancel_check", "验证取消入口"), ("cleanup", "关闭串口并释放资源")]
    responses = iter(scripted) if scripted is not None else None
    device = transport if transport is not None else SerialTransport(binding["endpoint"]["device"])
    console = Console(journal, phases, port, mode="实机只读导出 · 保留现有记录")
    def identity_guard():
        actual = port_identity(binding["endpoint"]["device"], binding["endpoint"]["location"])
        if actual != binding["endpoint"]:
            raise SessionChanged("Host endpoint identity changed before open")
    current, outcome, detail = "prepare", "passed", ""
    exported = reopened = None

    def prompt(key, message):
        if responses is None:
            return console.prompt(key, message, timeout)
        console.stage(key, "waiting")
        answer = next(responses)
        if answer == "cancel":
            console.cancel()
            raise Cancelled("Scripted cancellation")
        if answer == "timeout":
            raise TimeoutError("Scripted timeout")
        if answer != "continue":
            raise ValueError("Invalid scripted answer")
        journal.event("confirmed", phase=key)

    with console:
        collector = ProtectedExport(journal, device, console.cancelled, lease_directory(ROOT), expected,
                                    identity_guard=identity_guard if transport is None else None)
        print(f"Protected battery export: {console.url}", flush=True)
        try:
            prompt(current, "保持原电池＋电脑USB。请在设备上临时开启持续亮屏10分钟，回到普通页面，再点确认。不要拔线、复位或烧录；本轮只读取已有记录。")
            console.stage(current, "complete")
            with collector:
                current = "export"
                console.stage(current, "running")
                exported = collector.export("protected")
                console.stage(current, "complete")
                current = "reopen"
                console.stage(current, "running")
                collector.close()
                collector.open()
                reopened = collector.export("reopened")
                console.stage(current, "complete")
                current = "cancel_check"
                prompt(current, "已有记录已保存，关重开核对完成。现在请点「取消本轮」，验证串口和锁的退出清理；整体会标记为取消，已保存记录保留。无需操作设备。本步必须实际取消，点确认不能完成本轮验证。")
                raise ProtocolError("Cancellation check was skipped; bounded acceptance is incomplete")
        except Cancelled as exc:
            outcome, detail = "cancelled", str(exc)
        except TimeoutError as exc:
            outcome, detail = "timeout", str(exc)
        except Exception as exc:
            outcome, detail = "error", str(exc)
        if outcome != "passed":
            console.stage(current, outcome)
            if collector.cleanup_attempted:
                console.stage("cleanup", "cleaned" if collector.cleanup_confirmed else "error")
        outcome = console.seal(outcome)
        checks_met = (exported is not None and reopened is not None and current == "cancel_check" and
                      outcome == "cancelled" and collector.cleanup_confirmed)
        report = {"kind": "protected_device_export", "outcome": outcome,
                  "passed": False, "bounded_checks_met": checks_met,
                  "device_identity_verified": False, "new_battery_experiment_complete": False,
                  "transport_kind": "injected_test" if transport is not None else "serial",
                  "preparation_confirmed": console.snapshot()["phases"][0]["state"] == "complete",
                  "protected_export_matched": exported is not None, "reopened_export_matched": reopened is not None,
                  "cancel_cleanup_observed": current == "cancel_check" and outcome == "cancelled" and collector.cleanup_confirmed,
                  "record_count": len(exported["records"]) if exported else 0,
                  "cleanup_confirmed": collector.cleanup_confirmed, "detail": detail,
                  "completed_phases": console.snapshot()["completed"], "total_phases": len(phases)}
        write_json(journal.directory / "collection.json", report)
        journal.event("finished", outcome=outcome, passed=report["passed"],
                      cleanup_confirmed=collector.cleanup_confirmed,
                      report_sha256=digest(journal.directory / "collection.json"))
        finish_detail = (f"已保存{report['record_count']}条既有记录。" +
                         ("本轮按要求取消，验证项全部通过。" if checks_met else "尚有验证项未完成。") +
                         "请关闭设备临时持续亮屏，保持原供电。" + (detail if outcome == "error" else ""))
        console.finish(outcome, finish_detail, verification=verification_checks(report))
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--serial-port", required=True)
    parser.add_argument("--usb-location", required=True)
    parser.add_argument("--build-result", required=True, type=pathlib.Path)
    parser.add_argument("--expected-trace", required=True, type=pathlib.Path)
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--port", type=int, default=4444, help="Loopback WEB port")
    parser.add_argument("--timeout", type=float, default=300)
    args = parser.parse_args()
    if not 0 <= args.port <= 65535 or not 0 < args.timeout <= 900:
        parser.error("Invalid web port/confirmation timeout")
    binding = prepare_binding(args.build_result, args.expected_trace, args.serial_port, args.usb_location)
    result = run_export(load_trace(args.expected_trace), binding, args.out, port=args.port, timeout=args.timeout)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
