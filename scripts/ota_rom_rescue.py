"""ROM reset-catch state machine; transport and clock ownership stay explicit.

The model lab uses a logical clock. The device adapter runs the same operations
with a monotonic clock in a separately owned process; this module opens no port.
"""
from __future__ import annotations

import struct

from battery_console import Cancelled
import ota_rom_protocol as p

RESET, HALT, READY, DEBUG = 1 << 25, 1 << 17, 1 << 16, 1


class NoReply(TimeoutError):
    pass


class ModelClock:
    """Logical protocol timing only; never evidence of real device timing."""
    def __init__(self): self.value = 0.0
    def now(self): return self.value
    def sleep(self, seconds): self.value += seconds
    kind = "logical"


class Link:
    def __init__(self, transport, raw, journal, clock, cancel, budget_check=lambda: None,
                 active_seconds=22, cleanup_seconds=8):
        self.transport, self.raw, self.journal = transport, raw, journal
        self.clock, self.cancel, self.budget_check = clock, cancel, budget_check
        if not 0 < active_seconds <= 22 or not 0 < cleanup_seconds <= 8:
            raise ValueError("Active/cleanup budgets exceed the ROM window")
        self.started = clock.now()
        self.deadline = self.started + active_seconds
        self.cleanup_seconds = cleanup_seconds
        self.window_end = self.deadline + cleanup_seconds
        self.guard = p.WriteGuard()
        self.commands = 0
        self.entry_intent = False
        self.entry_released_at = None
        self.first_enter_attempted = False
        self.release_reply_attempted = False
        self.exit_sent = False
        self.exit_reset_intent = False
        self.closed = False
        self.rts_high = False
        self.cleaning = False

    def check(self):
        self.budget_check()
        if self.clock.now() >= self.deadline:
            raise TimeoutError("ROM phase deadline")
        if not self.cleaning and self.cancel.is_set():
            raise Cancelled("ROM experiment cancelled")

    def pulse(self, *, exit=False):
        self.check()
        if self.clock.now() + .2 > self.deadline:
            raise TimeoutError("Insufficient ROM time for the complete RTS pulse")
        if exit:
            if self.exit_reset_intent or not self.entry_intent:
                raise p.ProtocolError("Only one owned exit reset is permitted")
            self.exit_reset_intent = True
        else:
            if self.entry_intent:
                raise p.ProtocolError("ROM entry is never retried")
            self.entry_intent = True
        self.journal.event("rts_pulse_intent", exit=exit, clock_seconds=self.clock.now())
        self.rts_high = True  # A setter failure may still have changed the wire.
        try:
            self.transport.set_rts(True)
            self.clock.sleep(.1)
        finally:
            # Always release our high level, including cancellation/timeout.
            release_started = self.clock.now()
            self.transport.set_rts(False)
            self.rts_high = False
            if not exit:
                # A conservative timestamp: driver/journal latency cannot move
                # the first-response limit later than the release call began.
                self.entry_released_at = release_started
        self.clock.sleep(.1)
        self.journal.event("rts_pulse_complete", exit=exit, clock_seconds=self.clock.now())

    def request(self, wire, code, *, deadline=None, startup=False, release=False, strict_reply=False):
        self.check()
        if release and (not self.cleaning or not self.guard.cleaning or not self.guard.owned or
                        self.guard.counts.get((p.DHCSR, 0xA05F0000)) != 1 or self.release_reply_attempted or code != 0xD3 or
                        wire != bytes.fromhex("7e790c0010004077f0ed00f0010000005fa0")):
            raise p.ProtocolError("Startup tail belongs only to the owned debug-disable write")
        if release:
            self.release_reply_attempted = True
        if startup:
            if (wire != p.frame(p.ENTER) or code != 0xD1 or self.cleaning or
                    not self.entry_intent or self.entry_released_at is None or
                    self.commands or self.first_enter_attempted):
                raise p.ProtocolError("Startup preamble belongs only to the single first Enter")
            self.first_enter_attempted = True  # Consume even if pre-read fails.
            deadline = min(deadline if deadline is not None else self.deadline,
                           self.entry_released_at + 1.9)
        end = min(self.deadline, self.clock.now() + 1,
                  deadline if deadline is not None else self.deadline)
        if deadline is not None and self.clock.now() >= deadline:
            raise TimeoutError("Transfer deadline expired before transmission")
        self.commands += 1
        if self.commands > 128:
            raise p.ProtocolError("ROM command count exceeded")
        # First Enter alone may consume one known boot banner, including read
        # fragmentation. A quiet read is required after the complete banner;
        # old replies/partial frame headers and all other text still fail.
        preamble = bytearray()
        banners = (b"SFBL\n", b"SFBL\r\n")
        while True:
            self.check()
            if self.clock.now() >= end:
                raise TimeoutError("Pre-request receive exhausted the transaction deadline; nothing sent")
            orphan = self.transport.read(256)
            if orphan:
                self.raw.write(orphan); self.raw.flush()
                preamble.extend(orphan)
                if not startup or not any(banner.startswith(preamble) for banner in banners):
                    raise p.ProtocolError("Unsolicited bytes before a new request")
            elif not preamble or bytes(preamble) in banners:
                break
            else:
                self.clock.sleep(max(0, min(.002, end - self.clock.now())))
        self.check()
        if self.clock.now() >= end:
            raise TimeoutError("Pre-request receive exhausted the transaction deadline; nothing sent")
        if preamble:
            self.journal.event("rom_startup_preamble", hex=preamble.hex(), bytes=len(preamble),
                               clock_seconds=self.clock.now(), deadline=end)
        start = self.raw.tell()
        self.journal.event("rom_tx_intent", hex=wire.hex(), offset=start,
                           cleanup=self.cleaning, clock_seconds=self.clock.now())
        self.check()
        if self.clock.now() >= end:
            raise TimeoutError("Pre-transmit work exhausted the transaction deadline; nothing sent")
        if self.transport.write(wire) != len(wire):
            raise p.ProtocolError("Short control write; no retransmission")
        self.journal.event("rom_tx_complete", hex=wire.hex(), cleanup=self.cleaning)
        if code is None:
            self.exit_sent = True
        if self.clock.now() >= end:
            raise NoReply("I/O consumed the transaction deadline")
        if code is None:
            # Official Exit has no required ACK. Application recovery is separate.
            return None
        parser = p.DebugReleaseReply() if release else p.Reply(code, strict_start=strict_reply)
        while self.clock.now() < end:
            self.check()
            chunk = self.transport.read(256)
            if chunk:
                self.raw.write(chunk); self.raw.flush()
                if self.clock.now() >= end:
                    raise NoReply("Response arrived after the transaction deadline")
                value = parser.feed(chunk)
                if value is not None:
                    self.journal.event("rom_response", code=code, start=start, end=self.raw.tell(),
                                       clock_seconds=self.clock.now(), cleanup=self.cleaning)
                    return value
            else:
                self.check()
                if self.clock.now() >= end:
                    raise NoReply("Quiet read arrived after the transaction deadline")
                if release and parser.quiet():
                    self.journal.event("rom_response", code=code, start=start, end=start + len(parser.ACK),
                                       clock_seconds=self.clock.now(), cleanup=self.cleaning)
                    self.journal.event("rom_debug_release_tail", hex=parser.buffer[len(parser.ACK):].hex(),
                                       start=start + len(parser.ACK), end=self.raw.tell(), deadline=end)
                    return True
                self.clock.sleep(max(0, min(.002, end - self.clock.now())))
        raise NoReply("No complete reply within the fixed request deadline")

    def enter(self, *, first=False):
        # The absolute release timestamp includes the post-pulse 100ms wait.
        return self.request(p.frame(p.ENTER), 0xD1,
                            startup=first)

    def read(self, address, *, deadline=None, strict_reply=False):
        return self.request(p.read_word(address), 0xD2, deadline=deadline, strict_reply=strict_reply)
    def write(self, address, value):
        wire = self.guard.command(address, value)
        release = self.cleaning and address == p.DHCSR and value == 0xA05F0000
        return self.request(wire, 0xD3, release=release)

    def close(self):
        self.transport.close()
        self.closed = True
        self.journal.event("rom_transport_closed")


def snapshot(link, *, first):
    status = link.read(p.DHCSR)
    fault = link.read(p.DFSR)
    if status & (HALT | DEBUG) != HALT | DEBUG or not fault & 8 or (first and not status & RESET):
        raise p.ProtocolError("No fresh vector-catch halt before any post-reset halt request")
    result = dict(dhcsr=status, dfsr=fault, demcr=link.read(p.DEMCR), vtor=link.read(p.VTOR),
                  vector_sp=link.read(0), vector_pc=link.read(4))
    for name, selector in (("pc", 15), ("sp", 13)):
        link.write(p.DCRSR, selector)  # REGWnR remains zero.
        end = min(link.deadline, link.clock.now() + .1)
        while True:
            link.check()
            if link.read(p.DHCSR, deadline=end) & READY:
                break
            if link.clock.now() >= end:
                raise NoReply("Core-register transfer not ready within 100ms")
            link.clock.sleep(.01)
        result[name] = link.read(p.DCRDR, deadline=end)
    if (result["demcr"] != link.guard.baseline | 1 or result["vtor"] != 0 or
            not result["vector_pc"] & 1 or not 0 <= result["pc"] < 0x10000 or
            result["pc"] & ~1 != result["vector_pc"] & ~1 or
            not 0x20000000 < result["sp"] <= 0x20080000 or result["sp"] % 8 or
            result["sp"] != result["vector_sp"]):
        raise p.ProtocolError("PC/SP/VTOR or ROM vectors do not match the reset entrance")
    result["clock_seconds"] = link.clock.now()
    result["clock_kind"] = getattr(link.clock, "kind", "unspecified")
    return result


def experiment(link, observations):
    """Generator lets the shared Console expose automatic work/cancellation."""
    link.pulse()
    link.enter(first=True)
    yield "entered"
    if link.read(p.DHCSR) & DEBUG:
        raise p.ProtocolError("An existing debug owner prevents this experiment")
    link.guard.baseline_demcr(link.read(p.DEMCR))
    link.write(p.DHCSR, 0xA05F0003)
    if link.read(p.DHCSR) & (HALT | DEBUG) != HALT | DEBUG:
        raise p.ProtocolError("Initial debug halt was not confirmed")
    link.write(p.DFSR, link.read(p.DFSR) & 0x1f)
    link.read(p.DHCSR)  # Consume the initial power-reset sticky flag.
    link.write(p.DEMCR, link.guard.baseline | 1)
    if link.read(p.DEMCR) != link.guard.baseline | 1:
        raise p.ProtocolError("Reset catch did not arm")
    try:
        link.write(p.AIRCR, 0x05FA0004)
    except NoReply:
        link.journal.event("reset_response_missing", retransmitted=False)
    link.clock.sleep(.01)
    link.enter()
    # There is deliberately NO halt write after SYSRESETREQ.
    observations["first"] = snapshot(link, first=True)
    yield "caught"
    target = link.clock.now() + 2.5
    while link.clock.now() < target:
        link.check()
        link.clock.sleep(min(.1, target - link.clock.now()))
        yield "holding"
    observations["last"] = snapshot(link, first=False)
    for key in ("pc", "sp", "vector_sp", "vector_pc", "vtor", "demcr", "dfsr"):
        if observations["first"][key] != observations["last"][key]:
            raise p.ProtocolError("ROM stop context changed during the hold")
    if observations["last"]["clock_seconds"] - observations["first"]["clock_seconds"] < 2.5:
        raise p.ProtocolError("Insufficient observation separation")
    observations["observation_passed"] = True


def cleanup(link, *, allow_exit_reset):
    """Only owned cleanup; no experiment continuation, reconnect or retry."""
    link.cleaning = link.guard.cleaning = True
    # Keep exit time available when the experiment itself times out.
    link.deadline = min(link.window_end, link.clock.now() + link.cleanup_seconds)
    result = dict(debug_release_confirmed=False, debug_release_acknowledged=False,
                  debug_release_readback=None, exit_sent=False, exit_acknowledged=False,
                  exit_reset_attempted=False, resources_closed=False, errors=[])
    try:
        if link.rts_high:
            link.transport.set_rts(False)
            link.rts_high = False
        if link.guard.owned:
            current = link.read(p.DEMCR)
            if current & ~1 != link.guard.baseline:
                raise p.ProtocolError("Other debug bits changed; do not overwrite them")
            # A catch write may have failed before reaching the device.
            if current & 1:
                link.write(p.DEMCR, link.guard.baseline)
                if link.read(p.DEMCR) != link.guard.baseline:
                    raise p.ProtocolError("Reset catch clear not confirmed")
            link.write(p.DHCSR, 0xA05F0000)
            result["debug_release_acknowledged"] = True
            result["debug_release_readback"] = link.read(p.DHCSR, strict_reply=True)
            if result["debug_release_readback"] & (HALT | DEBUG):
                raise p.ProtocolError("Debug release not confirmed")
            result["debug_release_confirmed"] = True
        if link.entry_intent:
            link.request(p.frame(p.EXIT), None)
    except BaseException as exc:
        result["errors"].append(repr(exc))
        if allow_exit_reset and link.entry_intent:
            try:
                link.pulse(exit=True)
            except BaseException as reset_error:
                result["errors"].append(repr(reset_error))
    finally:
        result.update(exit_sent=link.exit_sent, exit_reset_attempted=link.exit_reset_intent,
                      rts_released=not link.rts_high)
        try:
            link.close()
            result["resources_closed"] = True
        except BaseException as exc:
            result["errors"].append(repr(exc))
    return result


class Simulation:
    """Independent wire peer; all values and timing are synthetic."""
    def __init__(self, scenario="success", fragment=3):
        self.scenario, self.fragment = scenario, fragment
        self.queue = bytearray()
        self.closed, self.rts = False, False
        self.pulses, self.resets, self.exits, self.halt_writes = 0, 0, 0, 0
        self.commands = []
        self.debug = self.halted = self.reset_sticky = False
        self.demcr, self.dfsr, self.selected = 0x01000000, 8, 0
        self.pc, self.sp = 0x234, 0x20008000
        self.transfers = 0
        self.reset_caught = False

    def set_rts(self, high):
        if high:
            self.pulses += 1
        elif self.scenario == "rts-release-fail":
            raise OSError("SIMULATION RTS release failed")
        self.rts = high
        if not high:
            self.debug = self.halted = False
            self.demcr = 0x01000000
            self.queue.clear()
            if self.pulses == 1 and self.scenario in ("startup-banner", "startup-old-frame"):
                self.queue.extend(b"SFBL\n")
                if self.scenario == "startup-old-frame":
                    self.queue.extend(bytes.fromhex("7e7902001000d106"))

    def read(self, count):
        count = min(count, self.fragment)
        data = bytes(self.queue[:count]); del self.queue[:count]
        return data

    def word(self, address):
        if address == p.DHCSR:
            status = (DEBUG if self.debug else 0) | (HALT if self.halted else 0)
            status |= 0 if self.scenario == "regrdy" and self.selected else READY
            status |= RESET if self.reset_sticky else 0
            self.reset_sticky = False
            return status
        if address == p.DEMCR: return self.demcr
        if address == p.DFSR: return self.dfsr
        if address == p.VTOR: return 0x12020000 if self.scenario == "vtor" else 0
        if address == 0: return 0x20008000
        if address == 4: return 0x135
        if address == p.DCRDR:
            if self.selected == 13: return self.sp
            self.transfers += 1
            if self.scenario == "pc-outside": return 0x20020000
            if self.scenario == "pc-mismatch": return 0x138
            if self.scenario == "unstable" and self.transfers > 1: return 0x138
            return self.pc
        raise ValueError("Simulator has no such read register")

    def write(self, wire):
        self.commands.append(wire)
        if wire[:2] != b"\x7e\x79" or wire[4:6] != b"\x10\x00":
            raise ValueError("Invalid simulated request")
        size = struct.unpack_from("<H", wire, 2)[0]
        payload = wire[6:]
        if size != len(payload): raise ValueError("Invalid simulated request size")
        if payload == b"ATSF32\x05!":
            if self.scenario == "no-reply": return len(wire)
            if self.scenario == "foreign-debug" and not self.resets: self.debug = True
            reply = b"\xd1\x06"
        elif payload == b"ATSF32\x18!":
            self.exits += 1
            if self.scenario == "exit-fail": raise OSError("SIMULATION Exit failure")
            return len(wire)  # No ACK promised or used.
        else:
            physical, count = struct.unpack_from("<IH", payload, 2)
            if count != 1: raise ValueError("Only simulated single-word access")
            logical = physical - 0xA0000000 if physical in (0xA0000000, 0xA0000004) else physical - 0x10000000
            if payload[:2] == b"@r":
                reply = b"\xd2" + struct.pack("<I", self.word(logical)) + b"\x06"
            elif payload[:2] == b"@w":
                value = struct.unpack_from("<I", payload, 8)[0]
                if self.scenario == "short-write" and logical == p.DHCSR and value & 1:
                    return len(wire) - 1
                if logical == p.DHCSR:
                    self.debug, self.halted = bool(value & 1), bool(value & 2)
                    self.halt_writes += bool(value & 2)
                elif logical == p.DFSR: self.dfsr &= ~value
                elif logical == p.DEMCR: self.demcr = value
                elif logical == p.DCRSR: self.selected = value
                elif logical == p.AIRCR:
                    self.resets += 1
                    self.reset_sticky = self.scenario != "no-reset-flag"
                    self.reset_caught = bool(self.debug and self.demcr & 1)
                    self.halted = self.reset_caught and self.scenario != "no-catch"
                    self.dfsr = 8 if self.halted and self.scenario != "stale-catch" else 0
                    self.pc = 0x134
                    if self.scenario == "aircr-no-reply": return len(wire)
                else: raise ValueError("Simulator has no such write register")
                reply = b"\xd3\x06"
            else: raise ValueError("Invalid simulated opcode")
        marker = b"\x11\x00" if self.scenario == "bad-marker" else b"\x10\x00"
        if self.scenario == "bad-terminator": reply = reply[:-1] + b"\x00"
        if self.scenario == "long-ack" and reply[0] == 0xD1: reply += b"\x06"
        self.queue.extend(b"\x7e\x79" + struct.pack("<H", len(reply)) + marker + reply)
        if self.scenario == "extra-frame": self.queue.extend(b"\x7e\x79\x02\x00\x10\x00\xd1\x06")
        if payload[:2] == b"@w" and logical == p.DHCSR and value == 0xA05F0000:
            if self.scenario in ("release-banner", "release-old-frame", "release-readback-fail"):
                self.queue.extend(b"SFBL\n")
                if self.scenario == "release-old-frame": self.queue.extend(p.DebugReleaseReply.ACK)
                if self.scenario == "release-readback-fail": self.debug = self.halted = True
        return len(wire)

    def close(self):
        if self.scenario == "close-fail": raise OSError("SIMULATION close failed")
        self.closed = True
