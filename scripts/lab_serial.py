"""Bounded UART primitives; adapters retain commands and device ownership."""
from __future__ import annotations

import hashlib
import os
import pathlib
import re
import subprocess
import time

from battery_console import Cancelled


class ProtocolError(Exception):
    pass


class SessionChanged(ProtocolError):
    pass


def lease_directory(root=None):
    """Use the primary checkout's existing lock directory across Git worktrees."""
    root = pathlib.Path(root or pathlib.Path(__file__).resolve().parents[1]).resolve()
    common = pathlib.Path(subprocess.check_output(
        ["git", "rev-parse", "--git-common-dir"], cwd=root, text=True).strip())
    common = (root / common).resolve() if not common.is_absolute() else common.resolve()
    return common.parent / "artifacts" / "battery-port-locks"


class PortLease:
    """Kernel lock survives no process; stale metadata is never ownership."""
    def __init__(self, endpoint, directory):
        directory = pathlib.Path(directory).resolve()
        directory.mkdir(parents=True, exist_ok=True)
        self.path = directory / (hashlib.sha256(endpoint.strip().casefold().encode()).hexdigest() + ".lock")
        self.endpoint, self.stream = endpoint, None

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
    """Opening a real endpoint is explicit; control lines are set beforehand."""
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


class RecordedClient:
    """Raw-first I/O; no control retries, reconnects, or device cleanup policy."""
    COMMAND_PATTERN = None  # A concrete adapter must declare its allowlist.
    BOOT_MARKERS = (b"[product] storage=",)

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
            if any(marker in self.tail + chunk for marker in self.BOOT_MARKERS):
                self.booted = True
                raise SessionChanged("Cold boot observed; RAM continuity cannot be assumed")
            self.tail = (self.tail + chunk)[-64:]
            self.buffer += chunk
            if len(self.last_bytes) > 262144 or len(self.buffer) > 262144:
                raise ProtocolError("Response byte bound exceeded")
        return chunk

    def drain(self):
        """Capture a bounded tail, without claiming future/unbounded UART."""
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

    def request(self, command, completion, *, cleanup=False, deadline=None):
        if self.COMMAND_PATTERN is None or not re.fullmatch(self.COMMAND_PATTERN, command):
            raise ValueError("Command outside collection allowlist")
        if not cleanup and self.cancel.is_set():
            raise Cancelled("Collection cancelled")
        self.last_bytes = bytearray()
        self.drain()
        if not cleanup and self.cancel.is_set():
            raise Cancelled("Cancelled during pre-request capture")
        if deadline is not None and time.monotonic() >= deadline:
            raise TimeoutError("Request budget expired before transmission")
        if self.buffer:
            self.journal.event("orphan_bytes", count=len(self.buffer))
        self.buffer = b""  # Persisted bytes must not become a new command reply.
        response_start = self.raw.tell()
        self.journal.event("tx", command=command, cleanup=cleanup, raw_offset=response_start)
        self.last_bytes = bytearray()
        wire = (command + "\r\n").encode("ascii")
        if self.transport.write(wire) != len(wire):
            raise ProtocolError("Partial command write; control command is not retried")
        deadline = min(deadline, time.monotonic()+self.timeout) if deadline is not None else time.monotonic()+self.timeout
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


class RecordedSession:
    """Own host resources only; the adapter must stop its device window first."""
    def __init__(self, journal, transport, cancel, client_type, *, identity, locks=None, timeout=1):
        self.journal, self.transport, self.cancel = journal, transport, cancel
        self.client_type, self.identity, self.timeout = client_type, identity, timeout
        self.lease = PortLease(transport.port, locks or lease_directory())
        self.raw = None
        self.port_closed = False
        self.resources_closed = False

    def __enter__(self):
        self.lease.__enter__()
        try:
            if self.cancel.is_set():
                raise Cancelled("Cancelled before opening endpoint")
            self.endpoint = self.identity()  # Recheck immediately before every open.
            if self.cancel.is_set():
                raise Cancelled("Cancelled during endpoint identity check")
            self.raw = (self.journal.directory / "uart.bin").open("xb")
            self.client = self.client_type(self.transport, self.raw, self.journal, self.cancel, self.timeout)
            self.transport.open()
            self.journal.event("transport_open", endpoint=self.endpoint, reset=False)
            return self.client
        except BaseException:
            self.__exit__()
            raise

    def __exit__(self, *args):
        failure = None
        try:
            self.transport.close()
            self.port_closed = True
            self.journal.event("transport_closed")
        except BaseException as exc:
            failure = exc
            self.journal.event("close_error", error=repr(exc))
        finally:
            try:
                if self.raw:
                    self.raw.close()
            finally:
                self.lease.__exit__()
        self.resources_closed = self.port_closed and (self.raw is None or self.raw.closed)
        if failure:
            raise failure
