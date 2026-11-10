"""Bounded SF32LB52 UART DAP codec. No serial import or endpoint discovery.

Host protocol reference: locked SiFli-SDK tools/sdk_py_actions/sifli_uart_dap.py.
The final 0x06 is a terminator, not a checksum. Strict ACK length is our
conservative admission rule; extended responses are retained and rejected.
"""
from __future__ import annotations

import struct

START = b"\x7e\x79"
MARKER = b"\x10\x00"
ENTER = b"ATSF32\x05!"
EXIT = b"ATSF32\x18!"
ROM_WORDS = frozenset((0, 4))
DHCSR, DCRSR, DCRDR, DEMCR = 0xE000EDF0, 0xE000EDF4, 0xE000EDF8, 0xE000EDFC
AIRCR, DFSR, VTOR = 0xE000ED0C, 0xE000ED30, 0xE000ED08
READS = ROM_WORDS | {DHCSR, DCRDR, DEMCR, DFSR, VTOR}


class ProtocolError(ValueError):
    pass


def mapped(address):
    """Only the logical addresses this probe owns; never map a Flash address."""
    if type(address) is not int:
        raise ProtocolError("Integer logical address required")
    if address in ROM_WORDS:
        return 0xA0000000 + address
    if address in READS | {DCRSR, AIRCR}:
        return address + 0x10000000
    raise ProtocolError("Address outside ROM observation allowlist")


def _frame(payload):
    if len(payload) not in (8, 12):
        raise ProtocolError("Only fixed single-word commands are permitted")
    return START + struct.pack("<H", len(payload)) + MARKER + payload


def frame(payload):
    if payload not in (ENTER, EXIT):
        raise ProtocolError("Only Enter/Exit can be framed without an operation guard")
    return _frame(payload)


def read_word(address):
    if address not in READS:
        raise ProtocolError("Read address outside allowlist")
    return _frame(b"@r" + struct.pack("<IH", mapped(address), 1))


class WriteGuard:
    """One reset, owned debug bits, and read-only core-register selectors."""
    def __init__(self):
        self.baseline = None
        self.counts = {}
        self.owned = False
        self.armed = False
        self.reset = False
        self.cleaning = False

    def baseline_demcr(self, value):
        if type(value) is not int or not 0 <= value <= 0xffffffff or self.baseline is not None or value & 1:
            raise ProtocolError("Existing reset catch or repeated baseline")
        self.baseline = value

    def command(self, address, value):
        if type(value) is not int or not 0 <= value <= 0xffffffff:
            raise ProtocolError("32-bit integer required")
        key = (address, value)
        cap = 1
        allowed = False
        if self.cleaning:
            allowed = self.owned and ((address == DEMCR and value == self.baseline) or
                                      (address == DHCSR and value == 0xA05F0000))
        elif address == DHCSR:
            allowed = self.baseline is not None and value == 0xA05F0003 and not self.reset
        elif address == DFSR:
            allowed = (self.owned and not self.armed and 0 <= value <= 0x1f and
                       not any(a == DFSR for a, _ in self.counts))
        elif address == DEMCR:
            allowed = self.owned and not self.reset and value == self.baseline | 1
        elif address == AIRCR:
            allowed = self.armed and not self.reset and value == 0x05FA0004
        elif address == DCRSR:
            allowed, cap = self.reset and value in (13, 15), 2
        if not allowed or self.counts.get(key, 0) >= cap:
            raise ProtocolError("Write outside owned operation/phase/count allowlist")
        # Consume before I/O, including partial writes and lost reset replies.
        self.counts[key] = self.counts.get(key, 0) + 1
        if address == DHCSR and value == 0xA05F0003:
            self.owned = True
        elif address == DEMCR and not self.cleaning:
            self.armed = True
        elif address == AIRCR:
            self.reset = True
        return _frame(b"@w" + struct.pack("<IHI", mapped(address), 1, value))


class Reply:
    """One expected reply; bounded noise, exact length, no leftover frame reuse."""
    def __init__(self, code, *, strict_start=False):
        if code not in (0xD1, 0xD2, 0xD3):
            raise ProtocolError("Unsupported reply kind")
        self.code, self.size = code, 6 if code == 0xD2 else 2
        self.strict_start = strict_start
        self.buffer = bytearray()
        self.received = 0
        self.started = False
        self.complete = False

    def feed(self, chunk):
        if self.complete:
            raise ProtocolError("Reply already consumed")
        self.received += len(chunk)
        if self.received > 512:
            raise ProtocolError("Response byte limit exceeded")
        self.buffer.extend(chunk)
        if not self.started:
            if self.strict_start and not (self.buffer.startswith(START) or START.startswith(self.buffer)):
                raise ProtocolError("Unexpected bytes before independent debug-release readback")
            index = self.buffer.find(START)
            if index < 0:
                self.buffer[:] = self.buffer[-1:] if self.buffer.endswith(b"\x7e") else b""
                return None
            del self.buffer[:index]
            self.started = True
        if len(self.buffer) < 6:
            return None
        length = struct.unpack_from("<H", self.buffer, 2)[0]
        if length != self.size or self.buffer[4:6] != MARKER:
            raise ProtocolError("Unknown reply length or marker")
        total = 6 + length
        if len(self.buffer) < total:
            return None
        if len(self.buffer) != total:
            raise ProtocolError("Extra bytes/frame cannot become a later reply")
        payload = bytes(self.buffer[6:])
        if payload[0] != self.code or payload[-1] != 0x06:
            raise ProtocolError("Unexpected reply kind or terminator")
        self.complete = True
        return struct.unpack_from("<I", payload, 1)[0] if self.code == 0xD2 else True


class DebugReleaseReply:
    """One exact D3 ACK, optionally followed by one observed startup line.

    Only the owned debug-disable transaction uses this parser. A quiet read
    completes it; incomplete lines keep the caller's original deadline.
    No byte is carried into a later reply and no leading noise is skipped.
    """
    ACK = bytes.fromhex("7e7902001000d306")
    ALLOWED = (ACK, ACK + b"SFBL\n", ACK + b"SFBL\r\n")

    def __init__(self):
        self.buffer = bytearray()
        self.complete = False

    def feed(self, chunk):
        if self.complete:
            raise ProtocolError("Reply already consumed")
        self.buffer.extend(chunk)
        if not any(value.startswith(self.buffer) for value in self.ALLOWED):
            raise ProtocolError("Unexpected debug-release ACK or startup tail")

    def quiet(self):
        if self.complete:
            raise ProtocolError("Reply already consumed")
        if bytes(self.buffer) not in self.ALLOWED:
            return False
        self.complete = True
        return True
