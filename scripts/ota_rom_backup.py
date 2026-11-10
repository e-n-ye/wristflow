"""Bounded dual-NOR ROM read; preserve application state and never write Flash.

capture still resets the chip and runs the official RAM stub. It requires a
fresh, explicit operator window; simulation never enumerates/opens a COM port.
"""
import argparse
import datetime as dt
import json
from pathlib import Path
import re
import secrets
import struct
import time

from battery_console import Cancelled, Console
from battery_lab import Journal
from cycle_log_lab import status as cycle_status
from device_snapshot import DATE, QUERIES, timestamp
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import PortLease, SerialTransport, lease_directory
from ota_backup_review import FLASH_BYTES, IMAGE_OFFSET, RUN_OFFSET, review_pair
from ota_rom_process import ReadProcess, ReaderCleanupError
from runtime_export import ExportClient, Simulation as ExportSimulation, request_snapshot
from runtime_record_decode import decode_capture
from serial_lab import ROOT, prepare_capture

TOOL = ROOT / ".tools/sifli/tools/sftool/0.2.5/sftool.exe"
SOURCES = ("ota_rom_backup.py", "ota_rom_process.py", "ota_backup_review.py", "runtime_export.py",
           "runtime_record_decode.py", "device_snapshot.py", "cycle_log_lab.py", "cycle_protocol.py",
           "storage_protocol.py", "fs_protocol.py")
PHASES = [("prepare", "备份前准备"), ("before", "保全应用前态"), ("read", "ROM双份读取"),
          ("return", "确认原程序返回"), ("after", "读取应用后态"), ("final", "末态确认")]
RETAINED = []  # Unknown process/UART closure must keep the endpoint lease alive.


class Observer(ExportClient):
    COMMAND_PATTERN = ExportClient.COMMAND_PATTERN + r"|show_date|wf_(?:steps|pm|cycle) status"


def command(tool, port):
    if not re.fullmatch(r"COM[1-9][0-9]*", port):
        raise ValueError("Canonical COM endpoint required")
    return [str(Path(tool).resolve()), "--chip", "SF32LB52", "--memory", "nor", "--port", port,
            "--baud", "500000", "--connect-attempts", "3", "--before", "default_reset",
            "--after", "soft_reset", "read_flash", "nor-first.bin@0x12000000:0x1000000",
            "nor-second.bin@0x12000000:0x1000000"]


def healthy(records):
    if (records["failed"] or records["saved"] is None or
            not records["current"]["flags"] & 2 or records["current"]["failures"]):
        raise ValueError("A running recorder with verified saved state and no storage failure is required")


def application(phase, transport, journal, console, identity, *, cycle):
    """One UART open/close under the caller's continuous outer PortLease."""
    if console.cancelled.is_set():
        raise Cancelled("Cancelled before application snapshot")
    identity()
    if console.cancelled.is_set():
        raise Cancelled("Cancelled during identity recheck")
    path = journal.directory / (phase + "-uart.bin")
    nonce = secrets.randbits(64) or 1
    replies = {}
    with path.open("xb") as raw:
        client = Observer(transport, raw, journal, console.cancelled, timeout=6)
        try:
            transport.open()
            journal.event("transport_open", phase=phase, reset=False)
            deadline = time.monotonic() + 60
            request_snapshot(client, nonce, deadline)
            records = decode_capture(path.read_bytes(), nonce)
            # Retain the prefix even if a subsequent status/close fails.
            write_json(journal.directory / (phase + "-runtime-prefix.json"), records)
            healthy(records)
            for key in ("date", "steps", "pm"):
                text, marker = QUERIES[key]
                complete = (lambda line: bool(DATE.fullmatch(line.strip()))) if marker is None else (
                    lambda line, marker=marker: marker in line)
                replies[key] = client.request(text, complete, deadline=deadline)
            if cycle:
                replies["cycle"] = cycle_status(client)
                if (replies["cycle"]["state"] not in (0, 5, 6) or replies["cycle"]["queued"] or
                        replies["cycle"]["accepted"] != replies["cycle"]["saved"]):
                    raise ValueError("CycleLog activity/pending records prevent a ROM reset")
            # Check UTC syntax independently; no clock write belongs to this tool.
            timestamp(replies["date"])
            if time.monotonic() >= deadline:
                raise TimeoutError("Application snapshot exceeded 60 seconds")
            if console.cancelled.is_set():
                raise Cancelled("Cancelled while preserving application state")
        finally:
            # Also close a partially acquired handle when open() raises.
            try:
                transport.close()
                journal.event("transport_closed", phase=phase)
            except BaseException as exc:
                journal.event("transport_close_failed", phase=phase, error=repr(exc))
                raise
    # Cache queries append to the same raw stream. Bind final records to its
    # complete bytes, while preserving the independently CRC-checked prefix.
    records = decode_capture(path.read_bytes(), nonce)
    write_json(journal.directory / (phase + "-records.json"), records)
    result = dict(records=records, replies=replies, closed_ns=time.time_ns(),
                  raw=file_record(path))
    write_json(journal.directory / (phase + "-snapshot.json"), result)
    journal.event("application_snapshot", phase=phase, boot=records["current"]["boot"],
                  unsaved_recorded_ms=records["unsaved_recorded_ms"])
    return result


def recovery(before, after, *, elapsed_seconds=None, check_rtc=True):
    old, new = before["records"], after["records"]
    healthy(new)
    expected_boot = (old["current"]["boot"] + 1) & 0xffffffff
    if new["current"]["boot"] != expected_boot:
        raise ValueError("Expected exactly one new application boot; preserve both snapshots")
    a, b = old["saved"], new["saved"]
    if ((b["generation"] - a["generation"]) & 0xffffffff) >= 0x80000000 or any(
            b[key] < a[key] for key in ("total_recorded_ms",)) or any(
            b["scene_ms"][key] < value for key, value in a["scene_ms"].items()):
        raise ValueError("Verified persisted runtime state moved backwards")
    elapsed = (after["closed_ns"] - before["closed_ns"]) / 1e9 if elapsed_seconds is None else elapsed_seconds
    if not 0 <= elapsed < float("inf"):
        raise ValueError("Invalid application snapshot elapsed time")
    rtc_continuity = abs(timestamp(after["replies"]["date"]) - timestamp(before["replies"]["date"]) - elapsed) <= 60
    if check_rtc and not rtc_continuity:
        raise ValueError("RTC continuity changed; time recovery needs separate authorization")
    return dict(runtime_saved_not_regressed=True, expected_new_boot=True, rtc_continuity=rtc_continuity,
                unsaved_tail_preserved_on_host_ms=old["unsaved_recorded_ms"],
                limits="Runtime saved counters and UTC only; settings, all files and bad-boot rescue are not verified")


def run(output, *, binding, identity, transport_factory, process_factory, locks,
        tool=TOOL, seconds=900, timeout=120, ready=None):
    output = Path(output).resolve()
    if output.exists() or not 0 < seconds <= 900 or not 0 < timeout <= 120:
        raise ValueError("New output directory, read <=900s and prompt <=120s required")
    simulated = binding["simulation"] is True
    if not simulated and (binding.get("rom_read_reset_authorized") is not True or
                          Path(tool).resolve() != TOOL.resolve()):
        raise ValueError("Explicit ROM reset/read authorization and the pinned sftool path are required")
    if not simulated and seconds < 720:
        raise ValueError("A complete dual read needs at least a 720s bounded window")
    sources = [file_record(Path(__file__).with_name(name)) for name in SOURCES]
    if not simulated:
        sources.append(file_record(tool))
    argv = command(tool, "COM1" if simulated else binding["endpoint"]["device"])
    journal = Journal(output, binding=dict(binding, adapter_sources=sources, argv=argv,
        read_seconds=seconds, policy="One ROM read process; no Flash write/erase, retry, RTC or application control"),
        mode="ota-rom-backup")
    report = dict(simulation=simulated, hardware_verified=False, device_identity_verified=False,
        deployment_authorized=False, rom_rescue_verified=False, flash_write=False, clock_write=False,
        process_attempted=False, process_exit=None, resources_closed=True, lease_retained=False, backup_pair_passed=False,
        application_recovery_passed=False, physical_observations_confirmed=False)
    process = None
    outcome, detail = "passed", ""
    with Console(journal, PHASES, mode="ROM备份模拟（无设备）" if simulated else "ROM双NOR备份") as console:
        print(console.url, flush=True)
        if ready:
            ready(console)
        try:
            console.prompt("prepare", ("【仅模拟】" if simulated else "") +
                "确认本轮ROM备份已获授权：仍为同板同固件，USB和电池供电接线固定、人在旁，屏触正常。"
                "开启20分钟临时常亮供前态读取；ROM会复位并中断原程序，RAM尾段仅复制电脑，RTC可能改变。"
                "一次读取两份16MiB，最多15分钟；期间保持接线，取消会终止读取并保留文件。", timeout)
            console.stage("prepare", "complete")
            lease = PortLease("SIMULATED-OTA-ROM" if simulated else binding["endpoint"]["device"], locks)
            lease.__enter__()
            try:
                journal.event("lease_acquired")
                def check():
                    verify_files(sources)
                    return identity()
                console.stage("before", "running")
                before = application("before", transport_factory("before"), journal, console, check,
                                     cycle=binding.get("cycle_log", False))
                console.stage("before", "complete")
                check()  # Same lease; UART has closed, tool and endpoint are rechecked.
                if not 0 <= time.time_ns() - before["closed_ns"] <= 120_000_000_000:
                    raise ValueError("Fresh prestate expired before ROM entry")
                if console.cancelled.is_set():
                    raise Cancelled("Cancelled before ROM entry")
                journal.event("rom_read_intent", argv=argv, single_process=True)
                report["process_attempted"] = True  # Includes an uncertain launch failure.
                try:
                    try:
                        process = process_factory(argv, output)
                    except ReaderCleanupError as exc:
                        process = exc.owner
                        # Retain this uncertain resource and clean it under the lease.
                        report["resources_closed"] = False
                        raise
                    console.automatic("read", "ROM读取中；保持供电和接线。取消会终止本次读取，不重试。",
                                      seconds, until=lambda: process.poll() is not None)
                    report["process_exit"] = process.poll()
                finally:
                    if process is not None:
                        try:
                            process.close()
                            report["resources_closed"] = True
                            journal.event("reader_closed", exit_code=report["process_exit"])
                            process = None
                        except BaseException as exc:
                            # Cleanup stays under the lease; never launch another reader.
                            report["resources_closed"] = False
                            journal.event("reader_close_failed", error=repr(exc))
                            try:
                                process.close()
                                report["resources_closed"] = True
                                journal.event("reader_cleanup_confirmed")
                                process = None
                            except BaseException as cleanup:
                                report["resources_closed"] = False
                                journal.event("reader_cleanup_uncertain", error=repr(cleanup))
                            raise
                if report["process_exit"] != 0:
                    raise ValueError("sftool failed; preserve files, no automatic reconnect/reset/retry")
                try:
                    pair = review_pair(output / "nor-first.bin", output / "nor-second.bin")
                except (OSError, ValueError, struct.error) as exc:
                    pair = dict(passed=False, error=repr(exc), deployment_authorized=False,
                                rom_rescue_verified=False)
                pair["simulation"] = simulated
                write_json(output / "pair-review.json", pair)
                report["backup_pair_passed"] = pair["passed"]
                console.prompt("return", "读取进程已退出。确认原程序已返回、屏触正常；若屏幕已熄灭按一次KEY1。"
                    "开启临时常亮供只读后态采集；异常请取消并在聊天说明，程序不追加复位。", timeout)
                console.stage("return", "complete")
                console.stage("after", "running")
                after = application("after", transport_factory("after"), journal, console, check,
                                    cycle=binding.get("cycle_log", False))
                report["recovery"] = recovery(before, after)
                report["application_recovery_passed"] = True
                console.stage("after", "complete")
                if not pair["passed"]:
                    raise ValueError("Backup pair review failed; preserve both files: " + pair["error"])
                journal.event("lease_releasing", reader_exited=True, uart_closed=True)
            finally:
                if any(event["kind"] == "transport_close_failed" for event in journal.events):
                    report["resources_closed"] = False
                if report["resources_closed"]:
                    lease.__exit__(None, None, None)
                else:
                    RETAINED.append((process, lease))
                    report["lease_retained"] = True
            console.prompt("final", "关闭临时常亮，回到表盘，确认显示、触摸、KEY1和时间正常。"
                "两份文件及前后记录已留存；此步骤不执行校时、Flash恢复写入或下一轮实验。", timeout)
            console.stage("final", "complete")
            report["physical_observations_confirmed"] = not simulated
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else (
                "timeout" if isinstance(exc, TimeoutError) else "error")
            detail = repr(exc)
            journal.event("backup_error", outcome=outcome, error=detail)
        finally:
            if any(event["kind"] == "transport_close_failed" for event in journal.events):
                report["resources_closed"] = False
            # Raw files, including truncated copies, are never deleted or replaced.
            report["files"] = [file_record(output / name) for name in (
                "before-uart.bin", "before-runtime-prefix.json", "before-records.json", "before-snapshot.json", "sftool.log",
                "nor-first.bin", "nor-second.bin", "after-uart.bin", "after-runtime-prefix.json", "after-records.json",
                "after-snapshot.json", "pair-review.json") if (output / name).is_file()]
        outcome = console.seal(outcome)
        report.update(outcome=outcome, detail=detail, passed=outcome == "passed",
            recovery_required=report["process_attempted"] and (outcome != "passed" or not report["application_recovery_passed"]),
            device_read_pair_matches=not simulated and report["backup_pair_passed"])
        report["verification"] = [
            {"title": "双份NOR一致与FTAB离线核验", "passed": report["backup_pair_passed"]},
            {"title": "应用保存记录与RTC后态", "passed": report["application_recovery_passed"]},
            {"title": "主机资源关闭", "passed": report["resources_closed"]},
            {"title": "模拟末态确认" if simulated else "人工末态确认", "passed": outcome == "passed"},
        ]
        write_json(output / "backup.json", report)
        journal.event("finished", outcome=outcome, report_sha256=digest(output / "backup.json"))
        console.finish(outcome, "本轮结束，文件全部保留。取消／异常后关闭临时常亮；若原程序未恢复，保持现场并在聊天说明。"
                       "成功读取不代表坏boot强制救援已验证。", verification=report["verification"])
    return report


class Simulation(ExportSimulation):
    def write(self, wire):
        text = wire.decode("ascii").strip()
        if text.startswith("wf_record "):
            return super().write(wire)
        self.commands.append(text)
        replies = {"show_date": dt.datetime.now(dt.timezone.utc).strftime("%a %b %d %H:%M:%S %Y"),
            "wf_steps status": "[steps-hours] simulation=1", "wf_pm status": "[pm-p1] phone=0 simulation=1",
            "wf_cycle status": "[cycle-status] state=0 session=0 accepted=0 saved=0 queued=0 dropped=0 reason=0 inhibit=0\r\n[cycle-prefix] simulation=1"}
        self.queue.extend((replies[text] + "\r\n").encode())
        return len(wire)


class SimulatedReader:
    """Synthetic FTAB only; these files are explicitly tagged simulation."""
    def __init__(self, argv, directory):
        data = bytearray(b"\xff") * FLASH_BYTES
        struct.pack_into("<I", data, 0, 0x53454346)
        for core, index, length, base, size, execution in (
            (1, 3, 58496, 0x12010000, 0x10000, 0x20020000),
            (2, 4, 3028888, 0x12020000, 0x800000, 0x12020000)):
            offset = IMAGE_OFFSET + (index - 2) * 512
            struct.pack_into("<4I", data, 4 + 16 * index, base, size, execution, 0)
            struct.pack_into("<IHH", data, offset, length, 512, 2)
            struct.pack_into("<I", data, RUN_OFFSET + core * 4, 0x12000000 + offset)
        struct.pack_into("<II", data, 0x10000, 0x20042000, 0x20020101)
        for name in ("nor-first.bin", "nor-second.bin"):
            with (directory / name).open("xb") as stream:
                stream.write(data)
        (directory / "sftool.log").write_text("SIMULATION: synthetic FTAB; no device or sftool opened.\n", encoding="utf-8")
        self.deadline = time.monotonic() + .15
    def poll(self):
        return 0 if time.monotonic() >= self.deadline else None
    def close(self):
        self.deadline = 0


def main():
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="mode", required=True)
    for mode in ("simulate", "capture"):
        s = sub.add_parser(mode)
        s.add_argument("--out", type=Path, required=True)
        s.add_argument("--seconds", type=float, default=900 if mode == "capture" else 2)
        if mode == "simulate":
            s.add_argument("--before-fixture", type=Path, required=True)
            s.add_argument("--after-fixture", type=Path, required=True)
        else:
            for name in ("result", "flash", "build-archive", "recorded-root"):
                s.add_argument("--" + name, type=Path, required=True)
            for name in ("port", "location", "expected-bin"):
                s.add_argument("--" + name, required=True)
            s.add_argument("--confirm-same-board-and-firmware", action="store_true", required=True)
            s.add_argument("--allow-rom-read-reset", action="store_true", required=True)
    args = p.parse_args()
    if args.mode == "simulate":
        fixtures = {"before": args.before_fixture.read_bytes(), "after": args.after_fixture.read_bytes()}
        binding, identity = {"simulation": True, "cycle_log": True}, lambda: {"simulation": True}
        factory, reader = lambda phase: Simulation(fixtures[phase]), SimulatedReader
        locks = args.out.parent / "simulation-locks"
    else:
        build = json.loads(args.result.read_text(encoding="utf-8-sig"))
        if any(build.get(flag) for flag in ("storage_probe", "resource_probe", "fs_probe")) or not any(
                item["path"].replace("\\", "/").endswith("/product_runtime_record.c")
                for item in build.get("project_sources", [])):
            raise ValueError("An ordinary recorder build is required")
        binding, identity = prepare_capture(args.result, args.flash, args.expected_bin, args.port,
            args.location, args.confirm_same_board_and_firmware, build_archive=args.build_archive,
            recorded_root=args.recorded_root)
        binding.update(cycle_log=build.get("cycle_log") is True, rom_read_reset_authorized=args.allow_rom_read_reset)
        factory, reader, locks = lambda phase: SerialTransport(args.port), ReadProcess, lease_directory()
    report = run(args.out, binding=binding, identity=identity, transport_factory=factory,
                 process_factory=reader, locks=locks, seconds=args.seconds)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
