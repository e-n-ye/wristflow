"""Battery collection adapter. CLI exposes the simulated device only."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import pathlib
import re
import threading
import time

from battery_console import Cancelled, Console
from battery_lab import Journal, ROOT, digest, load_trace, write_json

PREFIX = "[bat-trace] "
STATUS = [r"schema=(\d+) session=(\d+) state=(\d+) count=(\d+) cap=(\d+)",
          r"start=(\d+) duration=(\d+) now=(\d+); RAM only, no off-screen samples"]
STATUS_KEYS = [("schema", "session", "state", "count", "cap"), ("start", "duration", "now")]
ROW_PATTERNS = [r"row=(\d+)/(\d+) obs=(\d+) eval=(\d+) seq=(\d+)",
                r"adc=(\d+) age=(\d+) mv=(\d+) raw_valid=(\d+) display_valid=(\d+)",
                r"usb=(\d+)/(\d+)", r"charge=(\d+) reason=(\d+) cycle=(\d+) mode=(\d+) decision=(\d+)",
                r"filter=(\d+) valid=(\d+) candidate=(\d+) percent=(\d+) valid=(\d+)",
                r"surface=(\d+) phase=(\d+) flags=(\d+) requested=(\d+) ack=(\d+)", r"end=(\d+)/(\d+)"]
ROW_KEYS = [("session", "index", "obs", "eval", "seq"), ("adc", "age", "mv", "raw_valid", "display_valid"),
            ("usb_valid", "usb_good"), ("charge", "reason", "cycle", "mode", "decision"),
            ("filter", "filter_valid", "candidate", "percent", "percent_valid"),
            ("surface", "phase", "flags", "requested", "ack"), ("end_session", "end_index")]
FROZEN = ("schema", "session", "state", "count", "cap", "start", "duration")


class ProtocolError(Exception):
    pass


class SessionChanged(ProtocolError):
    pass


def trace_lines(lines):
    return [line[len(PREFIX):] for line in lines if line.startswith(PREFIX)]


def parse_status(lines):
    if len(lines) != 2:
        raise ProtocolError("Incomplete/extra trace status lines")
    result = {}
    for pattern, keys, line in zip(STATUS, STATUS_KEYS, lines):
        match = re.fullmatch(pattern, line)
        if not match:
            raise ProtocolError("Malformed trace status")
        result.update(zip(keys, map(int, match.groups())))
    if (any(value > 0xffffffff for value in result.values()) or result["schema"] != 1 or
            result["cap"] != 256 or result["count"] > 256 or result["state"] not in range(5)):
        raise ProtocolError("Invalid trace status values")
    return result


def parse_page(lines, expected, offset, count):
    lines = trace_lines(lines)
    actual = parse_status(lines[:2])
    if any(actual[key] != expected[key] for key in FROZEN):
        raise SessionChanged("Trace identity/state changed during export")
    if len(lines) != 3 + count * 7:
        raise ProtocolError("Incomplete/extra page rows")
    records = []
    for position in range(count):
        row = {}
        for pattern, keys, line in zip(ROW_PATTERNS, ROW_KEYS, lines[2+position*7:2+(position+1)*7]):
            match = re.fullmatch(pattern, line)
            if not match:
                raise ProtocolError("Malformed row; no byte repair")
            row.update(zip(keys, map(int, match.groups())))
        if (row.pop("end_session") != expected["session"] or row.pop("end_index") != offset+position or
                row["session"] != expected["session"] or row["index"] != offset+position):
            raise SessionChanged("Mixed session/index or mismatched row trailer")
        records.append(row)
    if lines[-1] != f"page={expected['session']}/{offset}/{count} complete":
        raise ProtocolError("Page completion mismatch")
    return records


class PortLease:
    """Kernel lock survives no process; stale metadata is never an ownership claim."""
    def __init__(self, endpoint, directory):
        directory = pathlib.Path(directory).resolve()
        directory.mkdir(parents=True, exist_ok=True)
        self.path = directory / (hashlib.sha256(endpoint.strip().casefold().encode()).hexdigest() + ".lock")
        self.endpoint = endpoint
        self.stream = None

    def __enter__(self):
        self.stream = self.path.open("a+b")
        try:
            self.stream.seek(0, 2)
            if not self.stream.tell():
                self.stream.write(b"0")
                self.stream.flush()
            self.stream.seek(0)
            if os.name == "nt":
                import msvcrt
                msvcrt.locking(self.stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                import fcntl
                fcntl.flock(self.stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError as exc:
            self.stream.close()
            self.stream = None
            raise RuntimeError("Endpoint already owned by another collector") from exc
        return self

    def __exit__(self, *args):
        if self.stream:
            self.stream.close()
            self.stream = None


class SerialTransport:
    """Future real-device boundary; never instantiated by this round's CLI."""
    def __init__(self, port, factory=None):
        if factory is None:
            import serial
            factory = serial.Serial
        self.port = port
        self.serial = factory(port=None, baudrate=1000000, timeout=.05, write_timeout=1,
                              rtscts=False, dsrdtr=False, xonxoff=False)
        self.serial.dtr = False
        self.serial.rts = False
        self.serial.port = port

    def open(self):
        self.serial.open()

    def close(self):
        self.serial.close()

    def read(self, count):
        return self.serial.read(count)

    def write(self, value):
        written = self.serial.write(value)
        self.serial.flush()
        return written


class Client:
    def __init__(self, transport, raw, journal, cancel, timeout=1):
        self.transport, self.raw, self.journal, self.cancel = transport, raw, journal, cancel
        self.timeout = timeout
        self.buffer = b""
        self.tail = b""
        self.last_bytes = bytearray()
        self.booted = False

    def read_chunk(self):
        chunk = self.transport.read(8192)
        if chunk:
            self.raw.write(chunk)
            self.raw.flush()
            self.last_bytes.extend(chunk)
            if b"[product] storage=" in self.tail + chunk:
                self.booted = True
                raise SessionChanged("Cold boot observed; RAM continuity cannot be assumed")
            self.tail = (self.tail + chunk)[-64:]
            self.buffer += chunk
            if len(self.last_bytes) > 262144 or len(self.buffer) > 262144:
                raise ProtocolError("Response byte bound exceeded")
        return chunk

    def drain(self):
        """Bounded tail capture, not a claim to record future/unbounded UART."""
        start = self.raw.tell()
        end, quiet = time.monotonic()+.1, time.monotonic()+.01
        while time.monotonic() < end:
            if self.read_chunk():
                quiet = time.monotonic()+.01
            elif time.monotonic() >= quiet:
                break
            else:
                time.sleep(.002)
        if self.raw.tell() != start:
            self.journal.event("drain_span", start=start, end=self.raw.tell())

    def take_lines(self):
        lines = []
        while b"\n" in self.buffer:
            line, self.buffer = self.buffer.split(b"\n", 1)
            line = re.sub(rb"\x1b\[[0-9;]*[A-Za-z]", b"", line).replace(b"\r", b"")
            lines.append(line.decode("ascii", errors="replace"))
        return lines

    def request(self, command, completion, *, cleanup=False):
        if not re.fullmatch(r"wf_pm (?:status|hold|allow)|wf_battery (?:status|stop|start \d+|dump \d+ \d+ \d+)", command):
            raise ValueError("Command outside collection allowlist")
        if not cleanup and self.cancel.is_set():
            raise Cancelled("Collection cancelled")
        self.last_bytes = bytearray()
        self.drain()
        if self.buffer:
            self.journal.event("orphan_bytes", count=len(self.buffer))
        self.buffer = b""  # Already persisted; never join different command replies.
        response_start = self.raw.tell()
        self.journal.event("tx", command=command, cleanup=cleanup, raw_offset=response_start)
        self.last_bytes = bytearray()
        wire = (command + "\r\n").encode("ascii")
        if self.transport.write(wire) != len(wire):
            raise ProtocolError("Partial command write; control command is not retried")
        deadline = time.monotonic() + self.timeout
        lines = []
        while time.monotonic() < deadline:
            if not cleanup and self.cancel.is_set():
                raise Cancelled("Collection cancelled")
            chunk = self.read_chunk()
            if not chunk:
                time.sleep(.002)
                continue
            lines.extend(self.take_lines())
            if any(completion(line) for line in lines):
                self.drain()
                lines.extend(self.take_lines())
                self.journal.event("response_span", command=command, start=response_start, end=self.raw.tell())
                return lines
        raise TimeoutError("No complete response before deadline")

    def status(self, *, cleanup=False):
        for attempt in range(2):
            try:
                lines = self.request("wf_battery status", lambda line: line.startswith(PREFIX + "start="), cleanup=cleanup)
                return parse_status(trace_lines(lines))
            except (ProtocolError, TimeoutError) as exc:
                if isinstance(exc, SessionChanged) or attempt:
                    raise
                self.journal.event("status_retry", error=str(exc))

    def control(self, command, *, cleanup=False):
        lines = self.request(command, lambda line: line.startswith(PREFIX + "start="), cleanup=cleanup)
        return parse_status(trace_lines(lines))

    def pm(self, action, *, cleanup=False):
        lines = self.request("wf_pm " + action, lambda line: line.startswith("[pm-p1] phone="), cleanup=cleanup)
        values = [line for line in lines if line.startswith("[pm-p1] ")]
        patterns = [r"\[pm-p1\] tick=\d+ allow=(\d+) off=\d+ own_idle=\d+ sdk_idle=\d+",
                    r"\[pm-p1\] enter=\d+ exit=\d+ deep_elapsed=\d+",
                    r"\[pm-p1\] deep_ticks=\d+ max_ticks=\d+ wake=0x[0-9a-fA-F]+",
                    r"\[pm-p1\] screen_off=\d+ screen_on=\d+ tick_hz=\d+",
                    r"\[pm-p1\] phone=\d+ phone_off=\d+ background=\d+"]
        if len(values) != 5 or any(not re.fullmatch(pattern, line) for pattern, line in zip(patterns, values)):
            raise ProtocolError("Incomplete PM response")
        return int(re.fullmatch(patterns[0], values[0]).group(1))


class Collector:
    def __init__(self, journal, transport, cancel, lease_directory, timeout=1):
        self.journal, self.transport, self.cancel = journal, transport, cancel
        self.lease = PortLease(transport.port, lease_directory)
        self.timeout = timeout
        self.session = None
        self.start_expected = None
        self.opened = False
        self.hold_maybe = False
        self.cleanup_confirmed = False
        self.cleanup_attempted = False

    def __enter__(self):
        self.lease.__enter__()
        try:
            self.raw = (self.journal.directory / "uart.bin").open("xb")
            self.client = Client(self.transport, self.raw, self.journal, self.cancel, self.timeout)
            self.open()
            return self
        except BaseException:
            try:
                self.transport.close()
            except Exception as exc:
                self.journal.event("close_error", error=str(exc))
            if hasattr(self, "raw"):
                self.raw.close()
            self.lease.__exit__()
            raise

    def open(self):
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

    def hold(self):
        self.hold_maybe = True  # A lost acknowledgement can still have applied.
        if self.client.pm("hold") != 0:
            raise ProtocolError("PM hold was not acknowledged")

    def allow(self, *, cleanup=False):
        if self.client.pm("allow", cleanup=cleanup) != 1:
            raise ProtocolError("PM allow was not acknowledged")
        self.hold_maybe = False

    def arm(self, seconds):
        if not 1 <= seconds <= 900:
            raise ValueError("Recorder duration must be 1..900 seconds")
        if self.client.pm("status") != 1:
            raise ProtocolError("Pre-existing PM hold; preserve it and stop preparation")
        previous = self.client.status()
        if previous["state"] == 1 or previous["count"]:
            raise ProtocolError("Existing running or nonempty trace preserved; export it first")
        self.hold()
        expected = (previous["session"] + 1) & 0xffffffff or 1
        self.start_expected = {"session": expected, "duration": seconds * 1000}
        try:
            status = self.client.control(f"wf_battery start {seconds}")
        except TimeoutError:
            self.journal.event("start_ack_lost", expected_session=expected)
            status = self.client.status()  # Resolve state, never repeat start.
        if status["session"] != expected or status["state"] != 1 or status["duration"] != seconds * 1000:
            raise SessionChanged("Arm acknowledgement does not match expected new trace")
        self.session = status
        self.journal.event("armed", status=status)
        self.allow()
        self.close()
        return status

    def stop_owned(self, status, *, cleanup=False):
        try:
            stopped = self.client.control("wf_battery stop", cleanup=cleanup)
        except TimeoutError:
            self.journal.event("stop_ack_lost")
            stopped = self.client.status(cleanup=cleanup)
        if (any(stopped[key] != status[key] for key in ("session", "start", "duration")) or
                stopped["state"] not in (2, 3, 4) or stopped["count"] < status["count"]):
            raise SessionChanged("Stop state/identity was not confirmed")
        return stopped

    def export(self):
        self.open()
        self.hold()
        status = self.client.status()
        if any(status[key] != self.session[key] for key in ("session", "start", "duration")):
            raise SessionChanged("Recorder session changed while disconnected")
        if status["state"] == 1:
            status = self.stop_owned(status)
        if status["state"] not in (2, 3, 4) or not status["count"]:
            raise ProtocolError("No complete stopped trace to export")
        records = []
        for offset in range(0, status["count"], 16):
            count = min(16, status["count"] - offset)
            for attempt in range(2):
                try:
                    lines = self.client.request(f"wf_battery dump {status['session']} {offset} {count}",
                                                lambda line: line.startswith(PREFIX + "page=") or
                                                line.startswith(PREFIX + "rejected") or line.startswith(PREFIX + "aborted"))
                    page = parse_page(lines, status, offset, count)
                    records.extend(page)
                    break
                except (ProtocolError, TimeoutError) as exc:
                    self.journal.event("page_rejected", offset=offset, attempt=attempt, error=str(exc))
                    if isinstance(exc, SessionChanged) or attempt:
                        raise
                finally:
                    (self.journal.directory / f"page-{offset:03d}-attempt-{attempt}.bin").write_bytes(self.client.last_bytes)
        value = {"status": status, "records": records}
        path = self.journal.directory / "records.json"
        write_json(path, value)
        load_trace(path)
        self.journal.event("export_complete", count=len(records), session=status["session"], sha256=digest(path))
        return value

    def __exit__(self, *args):
        self.cleanup_attempted = True
        errors = []
        try:
            if self.client.booted:
                errors.append("Device restarted; do not operate a new RAM session")
            elif self.start_expected is not None or self.hold_maybe:
                try:
                    if not self.opened:
                        self.open()
                    if self.session is None and self.start_expected is not None:
                        observed = self.client.status(cleanup=True)
                        if all(observed[key] == value for key, value in self.start_expected.items()):
                            self.session = observed
                            self.journal.event("arm_resolved_during_cleanup", status=observed)
                        else:
                            raise SessionChanged("Unconfirmed arm state; no stop sent")
                    if self.session is not None:
                        status = self.client.status(cleanup=True)
                        if any(status[key] != self.session[key] for key in ("session", "start", "duration")):
                            raise SessionChanged("Cleanup sees a different session; no stop sent")
                        if status["state"] == 1:
                            self.stop_owned(status, cleanup=True)
                except Exception as exc:
                    errors.append(str(exc))
                if self.hold_maybe:
                    try:
                        self.allow(cleanup=True)
                    except Exception as exc:
                        errors.append(str(exc))
        finally:
            try:
                self.close()
            except Exception as exc:
                errors.append("Transport close failed: " + str(exc))
            finally:
                try:
                    self.raw.close()
                finally:
                    self.lease.__exit__()
            self.cleanup_confirmed = not errors
            self.journal.event("cleanup", confirmed=self.cleanup_confirmed, errors=errors)


def status_text(status):
    return (f"[bat-trace] schema=1 session={status['session']} state={status['state']} count={status['count']} cap=256\r\n"
            f"[bat-trace] start={status['start']} duration={status['duration']} now={status['now']}; RAM only, no off-screen samples\r\n")


def window_summary(exported):
    rows = exported["records"]
    start = next((i for i, row in enumerate(rows) if row["usb_valid"] and not row["usb_good"]), None)
    selected, returned = [], False
    if start is not None:
        for row in rows[start:]:
            if row["usb_valid"] and row["usb_good"]:
                returned = True
                break
            selected.append(row)
    confirmed = [row for row in selected if row["usb_valid"] and not row["usb_good"]]
    span = (confirmed[-1]["obs"]-confirmed[0]["obs"]) & 0xffffffff if confirmed else 0
    return {"target_battery_ms": 480000, "observed_battery_span_ms": span,
            "unplug_delay_ms": (rows[start]["obs"]-exported["status"]["start"]) & 0xffffffff if start is not None else None,
            "usb_returned_in_trace": returned, "unknown_pg_rows": sum(not row["usb_valid"] for row in selected),
            "off_rows": sum(row["phase"] == 2 for row in selected),
            "target_span_met": span >= 480000, "complete": False,
            "note": "Historical/simulated data cannot verify a new hardware experiment. Span excludes confirmed USB return, includes unsampled gaps, and is not continuous BAT sampling."}


class SimulatedDevice:
    """Stateful command transport, including arbitrary fragmentation and faults."""
    port = "simulated:battery-b2"

    def __init__(self, fixture, *, chunk=73, fault=None):
        self.fixture, self.chunk, self.fault = fixture, chunk, fault
        self.status = dict(fixture["status"], session=fixture["status"]["session"]-1, state=2, count=0)
        self.allowed = 1
        self.queue = bytearray()
        self.commands = []
        self.is_open = False

    def open(self):
        self.is_open = True

    def close(self):
        self.is_open = False

    def read(self, count):
        if not self.is_open:
            raise OSError("Simulated disconnected transport")
        size = min(count, self.chunk)
        data = bytes(self.queue[:size])
        del self.queue[:size]
        return data

    def finish_offline(self):
        self.status = dict(self.fixture["status"])

    def write(self, wire):
        if not self.is_open:
            raise OSError("Simulated disconnected transport")
        command = wire.decode("ascii").strip()
        self.commands.append(command)
        if command.startswith("wf_pm "):
            action = command.split()[-1]
            if action in ("hold", "allow"):
                self.allowed = int(action == "allow")
            text = (f"[pm-p1] tick=1 allow={self.allowed} off=0 own_idle=1 sdk_idle=1\r\n"
                    "[pm-p1] enter=0 exit=0 deep_elapsed=0\r\n[pm-p1] deep_ticks=0 max_ticks=0 wake=0x0\r\n"
                    "[pm-p1] screen_off=0 screen_on=0 tick_hz=1000\r\n[pm-p1] phone=0 phone_off=0 background=0\r\n")
        else:
            operation = command.split()[1]
            if operation == "start":
                self.status.update(session=self.status["session"]+1, state=1, count=0, duration=int(command.split()[2])*1000)
            elif operation == "stop" and self.status["state"] == 1:
                self.status["state"] = 2
            text = status_text(self.status)
            if operation == "dump":
                session, offset, count = map(int, command.split()[2:])
                for row in self.fixture["records"][offset:offset+count]:
                    formats = ["row={session}/{index} obs={obs} eval={eval} seq={seq}",
                               "adc={adc} age={age} mv={mv} raw_valid={raw_valid} display_valid={display_valid}",
                               "usb={usb_valid}/{usb_good}", "charge={charge} reason={reason} cycle={cycle} mode={mode} decision={decision}",
                               "filter={filter} valid={filter_valid} candidate={candidate} percent={percent} valid={percent_valid}",
                               "surface={surface} phase={phase} flags={flags} requested={requested} ack={ack}", "end={session}/{index}"]
                    text += "".join(PREFIX + pattern.format(**row) + "\r\n" for pattern in formats)
                text += f"[bat-trace] page={session}/{offset}/{count} complete\r\n"
        reply = ("\r\n" + command + "\r\n" + text + "[background] sample unchanged\r\n").encode()
        if self.fault:
            reply = self.fault(command, reply, self)
        self.queue.extend(reply)
        return len(wire)


def simulate(fixture_path, output, port=4444, timeout=300, scripted=None):
    fixture = load_trace(fixture_path)
    journal = Journal(output, binding={"kind": "simulated_collection", "fixture_sha256": digest(fixture_path),
                                     "firmware_claim": fixture.get("source", {}), "device_identity_verified": False}, mode="simulation")
    phases = [("prepare", "准备证据"), ("arm", "启动记录并关闭串口"), ("offline", "确认模拟离线窗口"),
              ("export", "重新连接并导出"), ("cleanup", "停止与释放资源")]
    console = Console(journal, phases, port, mode="模拟采集 · 没有连接真实设备")
    responses = iter(scripted) if scripted is not None else None
    device = SimulatedDevice(fixture)
    collector = Collector(journal, device, console.cancelled, ROOT / "artifacts/battery-port-locks")
    outcome, detail, exported = "passed", "", None

    def prompt(key, message):
        if responses is None:
            console.prompt(key, message, timeout)
        else:
            console.stage(key, "waiting")
            answer = next(responses)
            if answer == "cancel":
                console.cancelled.set()
                raise Cancelled("Scripted cancel")
            if answer == "timeout":
                raise TimeoutError("Scripted timeout")
            if answer != "continue":
                raise ValueError("Invalid scripted answer")
            journal.event("confirmed", phase=key)

    current = "prepare"
    with console:
        print(f"Simulated battery collection: {console.url}", flush=True)
        try:
            prompt(current, "本轮连接的是模拟器，历史数据不是新测量。请勿操作实机。确认后验证启动、离线和分页导出。")
            console.stage(current, "complete")
            with collector:
                current = "arm"
                console.stage(current, "running")
                collector.arm(fixture["status"]["duration"] // 1000)
                console.stage(current, "complete")
                current = "offline"
                prompt(current, "模拟拔USB→普通页→手电筒→恢复→接回USB。本例不等待8分钟；点击确认将回放历史窗口，不代表物理动作已发生。")
                device.finish_offline()
                console.stage(current, "complete")
                current = "export"
                console.stage(current, "running")
                exported = collector.export()
                console.stage(current, "complete")
                current = "cleanup"
                console.stage(current, "running")
            if not collector.cleanup_confirmed:
                raise ProtocolError("Cleanup was not confirmed")
            console.stage(current, "complete")
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
        passed = outcome == "passed" and collector.cleanup_confirmed and exported is not None
        window = window_summary(exported) if exported else None
        report = {"kind": "simulated_collection", "passed": passed, "outcome": outcome,
                  "hardware_verified": False, "device_identity_verified": False,
                  "cleanup_confirmed": collector.cleanup_confirmed, "detail": detail,
                  "record_count": len(exported["records"]) if exported else 0, "window": window,
                  "completed_phases": console.snapshot()["completed"], "total_phases": len(phases)}
        write_json(journal.directory / "collection.json", report)
        journal.event("finished", passed=passed, outcome=outcome, hardware_verified=False,
                      cleanup_confirmed=collector.cleanup_confirmed,
                      report_sha256=digest(journal.directory / "collection.json"))
        window_detail = (f"BAT观测跨度{window['observed_battery_span_ms']/1000:.3f}秒，目标480秒；"
                         "历史／模拟数据不能完成新的硬件实验。") if window else ""
        console.finish(outcome, f"模拟采集结束；记录{report['record_count']}条。无新硬件证据。" + window_detail + detail)
        return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=pathlib.Path, default=ROOT / "tests/fixtures/battery-b2-20261021.json")
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--port", type=int, default=4444, help="Loopback WEB port; never a serial port")
    parser.add_argument("--timeout", type=float, default=300)
    args = parser.parse_args()
    if not 0 <= args.port <= 65535 or not 0 < args.timeout <= 900:
        parser.error("Invalid web port/confirmation timeout")
    return 0 if simulate(args.fixture, args.out, args.port, args.timeout)["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
