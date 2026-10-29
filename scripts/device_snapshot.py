"""Bounded pre/post experiment snapshots and separately authorized RTC startup.

Uses the shared recorded session; importing or simulating never accesses ports.
"""
from __future__ import annotations

import argparse
import datetime as dt
import json
from pathlib import Path
import re
import threading
import time

from battery_console import Cancelled
from battery_lab import Journal, read_progress
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import RecordedClient, RecordedSession, SerialTransport, lease_directory
from serial_lab import prepare_capture
from storage_protocol import parse_storage, simulated_storage

ROOT = Path(__file__).resolve().parents[1]
QUERIES = {
    "date": ("show_date", None), "steps": ("wf_steps status", "[steps-hours]"),
    "wrist": ("wf_wrist status", "[wrist-wake]"), "light": ("wf_light status", "[auto-light]"),
    "pm": ("wf_pm status", "[pm-p1] phone="), "compass": ("wf_compass status", "[compass-raw]"),
    "probe": ("wf_cprobe status", "[cp-param]"), "threads": ("list_thread", "wf_imu"),
    "memory": ("free", "maximum allocated"),
    "storage": ("wf_storage scan", "[storage-done]"),
}
DEFAULT_QUERIES = ("date", "steps", "wrist", "light", "pm")
DATE = re.compile(r"[A-Z][a-z]{2} [A-Z][a-z]{2}\s+\d{1,2} \d{2}:\d{2}:\d{2} \d{4}")


class SnapshotClient(RecordedClient):
    COMMAND_PATTERN = "|".join(re.escape(command) for command, _ in QUERIES.values())
    BOOT_MARKERS = (b"[product] storage=", b"SFBL")


class StartupClient(SnapshotClient):
    COMMAND_PATTERN = SnapshotClient.COMMAND_PATTERN + r"|wf_time \d+"


def timestamp(lines):
    dates = [line.strip() for line in lines if DATE.fullmatch(line.strip())]
    if len(dates) != 1:
        raise ValueError("One complete UTC date required")
    return dt.datetime.strptime(dates[0], "%a %b %d %H:%M:%S %Y").replace(tzinfo=dt.timezone.utc).timestamp()


def validate(queries, startup, allow_time_sync, seconds):
    queries = tuple(queries)
    if not queries or len(set(queries)) != len(queries) or any(key not in QUERIES for key in queries):
        raise ValueError("Select unique known query names")
    if not 0 < seconds <= 60 or startup != allow_time_sync:
        raise ValueError("Window must be 0..60 seconds; startup requires explicit time-sync permission")
    if startup and "storage" in queries:
        raise ValueError("Storage metadata must use capture, without RTC writes")


def collect(output, transport, *, binding, identity_check, locks, queries=DEFAULT_QUERIES,
            startup=False, allow_time_sync=False, seconds=60, cancel=None):
    queries = tuple(queries)
    validate(queries, startup, allow_time_sync, seconds)
    cancel = cancel or threading.Event()
    binding = dict(binding, queries=queries, startup=startup, allow_time_sync=allow_time_sync,
                   seconds=seconds, tool=file_record(Path(__file__)))
    journal = Journal(output, binding=binding, mode="startup" if startup else "device_snapshot")
    session = RecordedSession(journal, transport, cancel, StartupClient if startup else SnapshotClient,
                              identity=identity_check, locks=locks, timeout=6)
    replies, outcome, detail, attempted = {}, "passed", "", False
    try:
        with session as client:
            deadline = time.monotonic() + seconds

            def query(key):
                command, marker = QUERIES[key]
                complete = (lambda line: bool(DATE.fullmatch(line.strip()))) if marker is None else (lambda line: marker in line)
                return client.request(command, complete, deadline=deadline)

            if startup:
                replies["date_before"] = query("date")
                if abs(timestamp(replies["date_before"]) - time.time()) > 60:
                    attempted = True
                    journal.event("clock_write_intent", single_send=True)
                    replies["time_sync"] = client.request("wf_time " + str(int(time.time())),
                        lambda line: "[product] RTC synchronized:" in line, deadline=deadline)
                replies["date_after"] = query("date")
                if abs(timestamp(replies["date_after"]) - time.time()) > (8 if attempted else 60):
                    raise ValueError("RTC readback differs from current UTC")
            for key in queries:
                replies[key] = replies["date_after"] if startup and key == "date" else query(key)
                if key == "storage":
                    parse_storage(replies[key])
            if time.monotonic() >= deadline:
                raise TimeoutError("Snapshot completed outside its window")
            if cancel.is_set():
                raise Cancelled("Snapshot cancelled")
    except BaseException as exc:
        outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
        detail = repr(exc)
        journal.event("snapshot_error", outcome=outcome, error=detail)
    if cancel.is_set() and outcome == "passed":
        outcome, detail = "cancelled", "Cancelled while closing host resources"
    report = {"passed": outcome == "passed" and session.resources_closed, "outcome": outcome, "detail": detail,
              "simulation": binding["simulation"], "hardware_verified": False, "device_identity_verified": False,
              "startup": startup, "clock_write_attempted": attempted, "replies": replies,
              "reset": False, "pm_control": False, "resources_closed": session.resources_closed,
              "limits": "Complete reply markers only; health, persistence and device continuity require separate interpretation."}
    raw = output / "uart.bin"
    report["uart"] = file_record(raw) if raw.exists() else None
    write_json(output / "snapshot.json", report)
    journal.event("finished", outcome=outcome, passed=report["passed"], report_sha256=digest(output / "snapshot.json"))
    return report


class Simulation:
    port = "SIMULATED-SNAPSHOT"

    def __init__(self, stale_clock=False):
        self.queue, self.commands = bytearray(), []
        self.clock = 946684800 if stale_clock else None
        self.opened = self.closed = False

    def open(self):
        self.opened = True

    def close(self):
        self.closed = True

    def write(self, wire):
        command = wire.decode("ascii").strip()
        self.commands.append(command)
        if command == "show_date":
            reply = dt.datetime.fromtimestamp(self.clock or time.time(), dt.timezone.utc).strftime("%a %b %d %H:%M:%S %Y")
        elif command.startswith("wf_time "):
            self.clock = int(command.split()[1])
            reply = "[product] RTC synchronized: simulated"
        elif command == "wf_storage scan":
            reply = "\r\n".join(simulated_storage())
        else:
            reply = next(marker for query, marker in QUERIES.values() if query == command) + " simulated"
        self.queue.extend((reply + "\r\n").encode("ascii"))
        return len(wire)

    def read(self, count):
        value = bytes(self.queue[:min(count, 43)])
        del self.queue[:len(value)]
        return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    for mode in ("simulate", "capture", "startup"):
        sub = commands.add_parser(mode)
        sub.add_argument("--out", type=Path, required=True)
        sub.add_argument("--query", choices=tuple(QUERIES), action="append", help="Repeat to select only required cached queries; default: date/steps/wrist/light/pm")
        sub.add_argument("--seconds", type=float, default=60)
        if mode == "simulate":
            sub.add_argument("--startup", action="store_true", help="Simulate startup with an invalid RTC")
        else:
            for option in ("port", "location", "expected-bin"):
                sub.add_argument("--" + option, required=True)
            for option in ("result", "flash"):
                sub.add_argument("--" + option, type=Path, required=True)
            sub.add_argument("--source-archive", type=Path)
            sub.add_argument("--build-archive", type=Path)
            sub.add_argument("--recorded-root", type=Path)
            sub.add_argument("--confirm-same-board-and-firmware", action="store_true", required=True)
            if mode == "startup":
                sub.add_argument("--allow-time-sync", action="store_true", required=True,
                                 help="Separate current authorization to write RTC once if >60s from UTC")
    status = commands.add_parser("status")
    status.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "status":
        report = read_progress(args.run)
    else:
        startup = args.startup if args.command == "simulate" else args.command == "startup"
        allow_time_sync = startup if args.command == "simulate" else getattr(args, "allow_time_sync", False)
        queries = args.query or DEFAULT_QUERIES
        validate(queries, startup, allow_time_sync, args.seconds)
        if args.out.exists():
            raise ValueError("Output directory must be new")
        if args.command == "simulate":
            binding, identity = {"simulation": True}, lambda: {"simulation": True}
            peer, locks = Simulation(stale_clock=startup), args.out.parent / "simulation-locks"
        else:
            if "storage" in queries and json.loads(args.result.read_text(encoding="utf-8-sig")).get("storage_probe") is not True:
                raise ValueError("Storage metadata requires an explicit StorageProbe build")
            binding, guard = prepare_capture(args.result, args.flash, args.expected_bin, args.port, args.location,
                args.confirm_same_board_and_firmware, source_archive=args.source_archive,
                build_archive=args.build_archive, recorded_root=args.recorded_root)
            tool = file_record(Path(__file__))
            protocol = file_record(ROOT / "scripts/storage_protocol.py")

            def identity():
                verify_files([tool, protocol])
                return guard()

            binding["policy"] = "One bounded snapshot; " + ("RTC sync authorized once if needed" if startup else "cached queries or explicit read-only storage scan")
            binding["storage_protocol"] = protocol
            peer, locks = SerialTransport(args.port), lease_directory()
        report = collect(args.out, peer, binding=binding, identity_check=identity, locks=locks,
                         queries=queries, startup=startup, allow_time_sync=allow_time_sync, seconds=args.seconds)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 1 if report.get("passed") is False else 0


if __name__ == "__main__":
    raise SystemExit(main())
