"""Small read-only callers of the shared UART/session tools."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import threading
import time

from battery_lab import Journal, read_progress
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import ProtocolError, RecordedClient, RecordedSession
from battery_console import Cancelled
from light_protocol import states

ROOT = Path(__file__).resolve().parents[1]


class LightClient(RecordedClient):
    COMMAND_PATTERN = r"wf_light status"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL")

    def status(self, deadline):
        for attempt in range(2):
            try:
                remaining = deadline-time.monotonic()
                if remaining <= 0:
                    raise TimeoutError("Read-only capture deadline")
                self.timeout = min(1, remaining)
                lines = self.request("wf_light status", lambda line: "[auto-light] " in line, deadline=deadline)
                if time.monotonic() >= deadline:
                    raise TimeoutError("Response completed outside capture window")
                values = states("\n".join(lines)+"\n")
                if len(values) != 1:
                    raise ProtocolError("One complete light status required")
                return values[0]
            except (ProtocolError, TimeoutError) as exc:
                from lab_serial import SessionChanged
                if isinstance(exc, SessionChanged) or attempt:
                    raise
                self.journal.event("readonly_retry", command="wf_light status", error=repr(exc))


class PassiveClient(RecordedClient):
    # No allowed commands and no interpretation of unsolicited bytes.
    BOOT_MARKERS = ()


class Simulation:
    port = "SIMULATED-READONLY"

    def __init__(self, mode):
        self.mode, self.seq = mode, 0
        self.queue = bytearray()
        self.commands = []
        self.opened = self.closed = False
        self.next = 0

    def open(self):
        self.opened = True

    def close(self):
        self.closed = True

    def write(self, value):
        self.commands.append(value.decode("ascii").strip())
        self.seq += 1
        self.queue.extend((f"[auto-light] enabled=1 active=1 valid=1 fault=0 cleanup=0 brightness=23 mlux=100000 ms={self.seq*1000} session=1 seq={self.seq}; uncalibrated\r\n").encode())
        return len(value)

    def read(self, count):
        time.sleep(.002)
        if self.mode == "passive" and time.monotonic() >= self.next:
            self.next = time.monotonic()+.01
            self.queue.extend(b"[aod] simulated passive observation\r\n")
        value = bytes(self.queue[:min(count, 43)])
        del self.queue[:len(value)]
        return value


def record(output, transport, *, mode, seconds, interval=5, cancel=None, identity_check,
           binding, locks, ready=None):
    if mode not in ("light", "passive") or not 0 < seconds <= (30 if mode == "light" else 600):
        raise ValueError("Light window must be 0..30s; passive window 0..600s")
    if not .01 <= interval <= 30:
        raise ValueError("Query interval must be .01..30 seconds")
    cancel = cancel or threading.Event()
    binding = dict(binding, mode=mode, seconds=seconds, interval=interval,
                   sources=[file_record(ROOT / "scripts" / name, ROOT) for name in
                            ("serial_lab.py", "lab_serial.py", "lab_evidence.py", "light_protocol.py", "battery_lab.py")])
    journal = Journal(output, binding=binding, mode="readonly_"+mode)
    session = RecordedSession(journal, transport, cancel, LightClient if mode == "light" else PassiveClient,
                              identity=identity_check, locks=locks)
    observations, outcome, detail = [], "passed", ""
    try:
        with session as client:
            if ready:
                ready(cancel)
            begin = time.monotonic()
            deadline, next_query = begin+seconds, begin
            while time.monotonic() < deadline:
                if cancel.is_set():
                    raise Cancelled("Read-only capture cancelled")
                if (mode == "light" and time.monotonic() >= next_query and
                        deadline-time.monotonic() >= min(1, interval, seconds/3)):
                    observations.append(client.status(deadline))
                    journal.event("light_observation", value=observations[-1])
                    next_query = time.monotonic()+interval
                else:
                    client.read_chunk()
                    client.take_lines()  # Bounded response buffer, raw bytes remain authoritative.
            client.drain()
            if cancel.is_set():
                raise Cancelled("Cancelled during final raw-byte capture")
    except BaseException as exc:
        outcome = "cancelled" if isinstance(exc, Cancelled) else "timeout" if isinstance(exc, TimeoutError) else "error"
        detail = repr(exc)
        journal.event("capture_error", outcome=outcome, error=detail)
    if cancel.is_set() and outcome == "passed":
        outcome, detail = "cancelled", "Cancelled while closing host resources"
    report = {"outcome": outcome, "passed": outcome == "passed", "simulation": binding["simulation"],
              "hardware_verified": False, "mode": mode, "observations": observations,
              "commands": [] if mode == "passive" else ["wf_light status"],
              "port_closed": session.port_closed, "resources_closed": session.resources_closed,
              "device_control": False, "reset": False, "clock_write": False, "pm_control": False,
              "detail": detail}
    raw = output / "uart.bin"
    report["uart"] = file_record(raw) if raw.exists() else None
    try:
        write_json(output / "capture.json", report)
        journal.event("finished", outcome=outcome, passed=report["passed"], report_sha256=digest(output / "capture.json"))
    except BaseException:
        report.update(outcome="error", passed=False)
        write_json(output / "capture.json", report)
        raise
    return report


def replay(fixture, output):
    value = json.loads(fixture.read_text(encoding="utf-8"))
    verify_files([value["uart"]], fixture.parent)
    raw = (fixture.parent / value["uart"]["path"]).read_bytes()
    import re
    text = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", raw).decode("ascii", errors="replace")
    rows = states(text) if value["mode"] == "light" else []
    if value["mode"] not in ("light", "passive") or len(rows) != value["expected_observations"]:
        raise ProtocolError("Replay mode/observation mismatch")
    journal = Journal(output, binding={"simulation": False, "historical": True, "fixture": file_record(fixture)})
    report = {"passed": True, "kind": "historical_readonly_replay", "hardware_verified": False,
              "mode": value["mode"], "bytes": len(raw), "observations": rows,
              "source": value["source"], "limits": "Raw historical observation only; no new device operation or state-control claim"}
    try:
        write_json(output / "replay.json", report)
        journal.event("finished", outcome="passed", report_sha256=digest(output / "replay.json"))
    except BaseException:
        report.update(passed=False, outcome="error")
        write_json(output / "replay.json", report)
        raise
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    sim = commands.add_parser("simulate", help="Read-only light/passive fake peer; no endpoint access")
    sim.add_argument("--mode", choices=("light", "passive"), required=True)
    sim.add_argument("--seconds", type=float, default=1)
    sim.add_argument("--out", type=Path, required=True)
    rep = commands.add_parser("replay", help="Historical explicit UART fixture")
    rep.add_argument("--fixture", type=Path, required=True)
    rep.add_argument("--out", type=Path, required=True)
    status = commands.add_parser("status")
    status.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "status":
        report = read_progress(args.run)
    elif args.command == "replay":
        report = replay(args.fixture, args.out)
    else:
        report = record(args.out, Simulation(args.mode), mode=args.mode, seconds=args.seconds, interval=.1,
                        identity_check=lambda: {"kind": "simulation"}, binding={"simulation": True},
                        locks=args.out.parent / "simulation-locks")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if report.get("passed") is False:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
