"""Bounded MMC5603 readiness adapter; simulation/replay never opens a device."""
from __future__ import annotations

import argparse
import datetime as dt
import json
from pathlib import Path
import re
import threading
import time

from battery_console import Cancelled, Console
from battery_lab import Journal, read_progress
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import ProtocolError, RecordedClient, RecordedSession, SessionChanged
from mag_protocol import cached, frames, identity, ordinary
from light_protocol import active as active_light, states as light_states

ROOT = Path(__file__).resolve().parents[1]
LABELS = ("front", "right", "rear", "left", "front_return")
TITLES = ("朝桌前方 ↑", "转向右侧 →", "转向桌后方 ↓", "转向左侧 ←", "回到前方 ↑")


def require(condition, message):
    if not condition:
        raise ProtocolError(message)


class MagClient(RecordedClient):
    COMMAND_PATTERN = r"wf_pm status|wf_(?:mag|light|steps|wrist|ui) status|wf_mag sample encoded 120|wf_mag stop"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL")

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.lines = []
        self.stream_buffer = b""
        self.stream_bytes = 0

    def read_chunk(self):
        chunk = super().read_chunk()
        self.stream_bytes += len(chunk)
        if self.stream_bytes > 2097152:
            raise ProtocolError("Session evidence byte bound exceeded")
        self.stream_buffer += chunk
        while b"\n" in self.stream_buffer:
            line, self.stream_buffer = self.stream_buffer.split(b"\n", 1)
            line = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", line).replace(b"\r", b"")
            self.lines.append(line.decode("ascii", errors="replace"))
        return chunk

    @property
    def text(self):
        return "\n".join(self.lines) + "\n"

    def pump(self):
        self.read_chunk()
        self.take_lines()
        if self.cancel.is_set():
            raise Cancelled("Operator cancelled")

    def status(self, *, cleanup=False):
        for attempt in range(2):
            try:
                response = self.request("wf_mag status", lambda line: "[mag-mode] " in line, cleanup=cleanup)
                return cached("\n".join(response) + "\n")
            except (ProtocolError, TimeoutError) as exc:
                if isinstance(exc, SessionChanged) or attempt:
                    raise
                self.journal.event("status_retry", error=repr(exc))

    def light(self):
        reply = self.request("wf_light status", lambda line: "[auto-light] " in line)
        return active_light("\n".join(reply)+"\n")

    def snapshots(self, *, cleanup=False):
        result = {}
        for command, marker in (("wf_steps status", "[steps-hours]"), ("wf_wrist status", "[wrist-wake]"),
                                ("wf_ui status", "[ui-ring] current first=")):
            result[command] = self.request(command, lambda line, marker=marker: marker in line, cleanup=cleanup)
        return result


class Simulation:
    """Text-protocol peer; no serial imports, endpoint enumeration, or hardware."""
    port = "SIMULATED-MMC5603"

    def __init__(self, fault=None):
        self.queue = bytearray()
        self.commands = []
        self.session, self.start, self.seq = 2, 100, 0
        self.active, self.closed = False, True
        self.next = 0
        self.fault = fault
        self.boot_sent = False

    def open(self):
        self.closed = False

    def close(self):
        self.closed = True
        if self.fault == "close":
            raise OSError("Injected close failure")

    def state(self):
        end = 0 if self.active else 2
        return (f"[mag] session={self.session} phase={1 if self.active else 2} end={end} id=10 identity_valid=1 owned={int(self.active)} stopped={int(not self.active)} errors=0\r\n"
                f"[mag] start={self.start} duration=120000 ms={self.start+self.seq} seq={self.seq} status=51 after=10 xyz=100/200/300 valid=1 saturated=0\r\n"
                "[mag] selftest_valid=1 passed=1 status=55 factory=63/7a/72 threshold=5c/13/2c; uncalibrated counts, no heading\r\n"
                f"[mag-mode] session={self.session} selftest_only=0 threshold=encoded experimental=1\r\n")

    def write(self, wire):
        command = wire.decode("ascii").strip()
        self.commands.append(command)
        reply = ""
        if command == "wf_pm status":
            reply = "[pm-p1] tick=900 allow=1 off=0\r\n[pm-p1] screen_off=0 screen_on=0 tick_hz=1000\r\n[pm-p1] phone=0 phone_off=0 background=0\r\n"
        elif command == "wf_mag status":
            reply = self.state()
        elif command == "wf_mag sample encoded 120":
            self.session += 1
            self.seq, self.start, self.active = 0, 1000, True
            self.next = time.monotonic()+.05
            reply = (f"[mag-data] session={self.session} seq=0 ms=0 status=00 xyz=0/0/0 valid=0 saturated=0; raw counts\r\n"
                     f"[mag-selftest] session={self.session} valid=1 passed=1\r\n")
            if self.fault != "lost_start_ack":
                reply += "[mag] queued=1\r\n[mag] accepted=1\r\n"
            if self.fault == "cold_start":
                reply = "SFBL\r\n"
        elif command == "wf_mag stop":
            self.active = False
            reply = "" if self.fault == "lost_stop_ack" else "[mag] queued=1\r\n"
        elif command == "wf_light status":
            reply = f"[auto-light] enabled=1 active=1 valid=1 fault=0 cleanup=0 brightness=23 mlux=100000 ms=1 session=1 seq={self.seq+1}; uncalibrated\r\n"
        elif command == "wf_steps status":
            reply = "[steps] valid=1 total=0\r\n[steps-store] available=1 verified=1 dirty=0 failed=0 generation=112\r\n[steps-hours] valid=0 sum=0 saved_valid=0\r\n"
        elif command == "wf_wrist status":
            reply = "[wrist-wake] requested=1 continuous=1 armed=1 owned=1\r\n"
        elif command == "wf_ui status":
            reply = "[ui-ring] current first=0,390,0x4 last=1950,390,0x4\r\n"
        else:
            reply = f"[{command.split()[0]}] simulated=1\r\n"
        self.queue.extend(reply.encode())
        return len(wire)-1 if self.fault == "short_write" else len(wire)

    def read(self, count):
        time.sleep(.002)
        if self.fault == "cold_boot" and self.seq >= 2 and not self.boot_sent:
            self.boot_sent = True
            self.queue.extend(b"SFBL\r\n")
        if self.active and time.monotonic() >= self.next and self.fault != "no_samples":
            self.next = time.monotonic()+.035
            self.seq += 1
            self.queue.extend(f"[mag-data] session={self.session} seq={self.seq} ms={self.start+self.seq} status=51 xyz=100/200/300 valid=1 saturated=0; raw counts\r\n".encode())
        value = bytes(self.queue[:min(count, 97)])
        del self.queue[:len(value)]
        return value


def run(output, transport, *, identity_check, binding, locks, port=0, timeout=300,
        sample_timeout=10, settle_seconds=2.2, ready=None):
    binding = dict(binding, tool_sources=[file_record(ROOT / "scripts" / name, ROOT) for name in
                    ("mmc5603_lab.py", "mag_protocol.py", "light_protocol.py", "lab_serial.py",
                     "lab_evidence.py", "battery_console.py", "battery_lab.py")])
    journal = Journal(output, binding=binding, mode="mmc5603_readiness")
    phases = [("prepare", "准备")]+list(zip(LABELS, TITLES))+[("stop", "停止与关口"), ("confirm_end", "末态确认")]
    outcome, detail = "passed", ""
    before = after = owned = None
    start_attempted = stop_attempted = stop_confirmed = False
    windows = []
    lights, snapshots = [], {}
    session = None
    reset_excluded = False
    lower_tick = None
    with Console(journal, phases, port, mode="模拟地磁流程" if binding["simulation"] else "地磁相对旋转") as console:
        write_json(output / "console-ready.json", {"url": console.url, "simulation": binding["simulation"]})
        print(console.url, flush=True)
        if ready:
            ready(console)
        try:
            console.prompt("prepare", "仅模拟时无需操作设备。实机时：确认授权的板屏、供电、自动亮度和接线条件，平放朝前。", timeout)
            console.stage("prepare", "complete")
            session = RecordedSession(journal, transport, console.cancelled, MagClient,
                                      identity=identity_check, locks=locks)
            with session as client:
                try:
                    pm = client.request("wf_pm status", lambda line: "[pm-p1] tick=" in line)
                    require(any(re.search(r"allow=1\b", line) for line in pm), "Foreign PM hold")
                    before = client.status()
                    require(before["phase"] != 1 and not before["owned"], "Protect previous operation")
                    lights.append(client.light())
                    snapshots["before"] = client.snapshots()
                    if console.cancelled.is_set():
                        raise Cancelled("Cancelled before start")
                    clock_lines = client.request("wf_pm status", lambda line: "[pm-p1] phone=" in line)
                    clock = "\n".join(clock_lines)
                    tick = re.search(r"\[pm-p1\] tick=(\d+) allow=1 ", clock)
                    require(tick and "tick_hz=1000" in clock, "Start clock boundary missing")
                    lower_tick = int(tick[1])
                    require(lower_tick <= 0xffffffff, "Invalid PM clock")
                    start_attempted = True
                    total_deadline = time.monotonic()+(900 if binding["simulation"] else 120)
                    journal.event("start_intent", expected_session=(before["session"]+1) & 0xffffffff,
                                  duration_ms=120000, mode="encoded", lower_tick=lower_tick, max_start_delay_ms=5000, no_retry=True)
                    start_error = None
                    try:
                        client.request("wf_mag sample encoded 120", lambda line: "[mag] accepted=1" in line)
                    except SessionChanged:
                        raise
                    except (TimeoutError, ProtocolError, Cancelled) as exc:
                        start_error = exc
                        journal.event("start_ack_missing", action="Bounded status observation; never resend start")
                    current = client.status(cleanup=True)
                    require(current["session"] == (before["session"]+1) & 0xffffffff and current["phase"] == 1 and
                            current["mode"] == "encoded" and not current["selftest_only"] and current["duration_ms"] == 120000 and
                            ((current["start_ms"]-lower_tick) & 0xffffffff) <= 5000,
                            "Start window identity unconfirmed")
                    owned = identity(current)
                    journal.event("ownership", identity=owned)
                    if start_error is not None and not isinstance(start_error, TimeoutError):
                        raise start_error
                    require(current["id"] == 0x10 and current["identity_valid"] and current["experimental"] and
                            current["native_valid"] and current["passed"] and not current["errors"], "Native identity/selftest not passed")
                    expected_threshold = [min(255, (abs(v-128)*32-(abs(v-128)*32)//5)//8) for v in current["factory"]]
                    require(current["threshold"] == expected_threshold, "Encoded threshold mismatch")

                    def pump():
                        client.pump()
                        if re.search(r"\[mag-state\] session="+str(owned[0])+r" phase=[023] ", client.text):
                            raise ProtocolError("Window ended before all positions")
                        if time.monotonic() >= total_deadline:
                            raise TimeoutError("Total observation deadline")

                    for index, label in enumerate(LABELS):
                        if index:
                            console.prompt(label, TITLES[index]+"；保持屏幕朝上，停稳后确认，不牵拉接线。",
                                           min(timeout, 300 if binding["simulation"] else 30), pump=pump)
                            end = time.monotonic()+settle_seconds
                            while time.monotonic() < end:
                                pump()
                        offset = len(client.text)
                        console.automatic(label, TITLES[index]+"；保持不动，程序采集6帧。", sample_timeout,
                                          pump=pump, until=lambda: len(frames(client.text[offset:], owned[0])) >= 6)
                        items = frames(client.text[offset:], owned[0])[:6]
                        require(all(v["valid"] and not v["saturated"] and v["status"] & 0x70 == 0x50 for v in items), "Bad position frame")
                        windows.append({"label": label, "seqs": [v["seq"] for v in items]})
                        light = client.light()
                        require(light["session"] == lights[0]["session"] and
                                0 < ((light["seq"]-lights[-1]["seq"]) & 0xffffffff) < 0x80000000,
                                "Automatic brightness window changed or did not progress")
                        lights.append(light)
                    require(time.monotonic() < total_deadline, "Total observation deadline")
                    current = client.status()
                    require(identity(current) == owned and current["phase"] == 1, "Window changed before stop")
                    require(time.monotonic() < total_deadline, "Deadline reached before explicit stop")
                    stop_attempted = True
                    client.request("wf_mag stop", lambda line: "[mag] queued=1" in line, cleanup=True)
                    after = client.status(cleanup=True)
                    require(identity(after) == owned and after["phase"] == 2 and after["end"] == 2 and
                            not after["owned"] and after["stopped"] and not after["errors"] and
                            after["after_status"] & 0x50 == 0x10, "Stop baseline unconfirmed")
                    stop_confirmed = True
                    rows, reset_excluded = ordinary(client.text, after)
                    require(len(rows) >= 30, "Missing readiness windows")
                    snapshots["after"] = client.snapshots()
                    final_pm = client.request("wf_pm status", lambda line: "[pm-p1] tick=" in line)
                    require(any(re.search(r"allow=1\b", line) for line in final_pm), "Final PM baseline changed")
                finally:
                    if start_attempted and owned is None and not client.booted:
                        try:
                            candidate = client.status(cleanup=True)
                            require(candidate["session"] == (before["session"]+1) & 0xffffffff and
                                    candidate["mode"] == "encoded" and not candidate["selftest_only"] and
                                    candidate["duration_ms"] == 120000 and candidate["identity_valid"] and candidate["id"] == 0x10 and
                                    ((candidate["start_ms"]-lower_tick) & 0xffffffff) <= 5000,
                                    "Start intent does not identify observed window; preserve foreign state")
                            owned = identity(candidate)
                            journal.event("ownership_resolved_for_cleanup", identity=owned)
                        except BaseException as exc:
                            journal.event("ownership_unresolved", error=repr(exc))
                    if owned is not None and not stop_confirmed and not client.booted:
                        try:
                            current = client.status(cleanup=True)
                            require(identity(current) == owned, "Foreign window; no stop")
                            if current["phase"] == 1 and not stop_attempted:
                                stop_attempted = True
                                client.request("wf_mag stop", lambda line: "[mag] queued=1" in line, cleanup=True)
                                current = client.status(cleanup=True)
                            after = current
                            require(identity(current) == owned and current["phase"] != 1 and not current["owned"] and
                                    current["stopped"] and not current["errors"] and current["after_status"] & 0x50 == 0x10,
                                    "Cleanup stop unconfirmed")
                            stop_confirmed = True
                            journal.event("cleanup_confirmed", identity=owned)
                        except BaseException as exc:
                            journal.event("cleanup_unconfirmed", error=repr(exc))
            console.stage("stop", "complete")
            console.prompt("confirm_end", "五方向已结束。实机时关闭临时常亮，确认动作、显示、触摸及时间正常，保留原设置。", timeout)
            console.stage("confirm_end", "complete")
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, Cancelled) else "timeout" if isinstance(exc, TimeoutError) else "error"
            detail = repr(exc)
            journal.event("workflow_error", outcome=outcome, error=detail)
            if stop_confirmed:
                console.stage("stop", "cleaned")
        outcome = console.seal(outcome)
        report = {"outcome": outcome, "passed": outcome == "passed", "simulation": binding["simulation"],
                  "hardware_verified": False, "before": before, "after": after, "windows": windows,
                  "lights": lights, "snapshots": snapshots,
                  "start_attempted": start_attempted, "stop_attempted": stop_attempted,
                  "stop_confirmed": stop_confirmed, "recovery_required": start_attempted and not stop_confirmed,
                  "port_closed": bool(session and session.port_closed),
                  "resources_closed": bool(session and session.resources_closed),
                  "reset_notice_excluded": reset_excluded,
                  "operator_end_confirmed": any(e["kind"] == "confirmed" and e.get("phase") == "confirm_end" for e in journal.events),
                  "detail": detail}
        try:
            write_json(output / "workflow.json", report)
            journal.event("finished", outcome=outcome, passed=report["passed"], report_sha256=digest(output / "workflow.json"))
        except BaseException as exc:
            outcome = "error"
            report.update(outcome="error", passed=False, detail="Report finalization failed: "+repr(exc))
            write_json(output / "workflow.json", report)
            console.finish("error", report["detail"])
            raise
        console.finish(outcome, "模拟／软件流程结果；不证明磁北精度或新增实机验收。"+detail,
                       verification=[{"title": "五方向窗口", "passed": len(windows) == 5},
                                     {"title": "设备窗口停止确认", "passed": stop_confirmed},
                                     {"title": "主机资源关闭", "passed": report["resources_closed"]},
                                     {"title": "人工末态确认", "passed": report["operator_end_confirmed"]}])
    return report


def replay(fixture, output):
    data = json.loads(fixture.read_text(encoding="utf-8"))
    verify_files([data["uart"]], fixture.parent)
    raw = (fixture.parent / data["uart"]["path"]).read_bytes()
    text = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", raw).decode("ascii", errors="strict")
    require(data["original_workflow"]["outcome"] == "error" and not data["original_workflow"]["passed"], "Historical failure must remain")
    after = cached(text)
    rows, excluded = ordinary(text, after)
    require(after["phase"] == 2 and after["end"] == 2 and after["stopped"] and not after["owned"] and not after["errors"], "Historical stop missing")
    require(after["after_status"] == 0x10 and after["native_valid"] and after["passed"] and after["mode"] == "encoded", "Historical identity/baseline missing")
    require(len(rows) == data["ordinary_frames"] and excluded, "Historical ordinary/reset count mismatch")
    require([w["label"] for w in data["windows"]] == list(LABELS), "Missing position window")
    for window in data["windows"]:
        seqs = window["seqs"]
        require(len(seqs) == 6 and seqs == list(range(seqs[0], seqs[0]+6)) and all(1 <= seq <= len(rows) for seq in seqs), "Invalid position indices")
    journal = Journal(output, binding={"kind": "historical_mag_replay", "fixture": file_record(fixture), "uart": data["uart"]})
    report = {"passed": True, "kind": "historical_mag_replay", "hardware_verified": False,
              "ordinary_frames": len(rows), "reset_notices": int(excluded), "windows": data["windows"],
              "after": after, "original_workflow": data["original_workflow"],
              "operator_end_confirmation_source": "Separate historical chat; not synthesized from UART or workflow",
              "limits": "Historical bounded relative rotation; no new device test or absolute heading accuracy"}
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
    sim = commands.add_parser("simulate", help="Shared console with a fake device; no endpoint access")
    sim.add_argument("--out", type=Path, required=True)
    sim.add_argument("--port", type=int, default=0)
    sim.add_argument("--timeout", type=float, default=300)
    rep = commands.add_parser("replay", help="Reconcile historical raw bytes without changing old reports")
    rep.add_argument("--fixture", type=Path, default=ROOT / "tests/fixtures/mag-readiness-20261024.json")
    rep.add_argument("--out", type=Path, required=True)
    status = commands.add_parser("status", help="Read the saved journal; never resumes a device operation")
    status.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    if args.command == "status":
        report = read_progress(args.run)
    elif args.command == "replay":
        report = replay(args.fixture, args.out)
    else:
        report = run(args.out, Simulation(), identity_check=lambda: {"kind": "simulation"},
                     binding={"simulation": True}, locks=args.out.parent / "simulation-locks", port=args.port,
                     timeout=args.timeout, settle_seconds=.02)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if report.get("passed") is False:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
