"""No hardware: golden wire bytes, reset-catch faults and owned cleanup."""
import io
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_rom_protocol as p
import ota_rom_rescue as engine
import ota_rom_rescue_lab as lab
from battery_console import Cancelled
from lab_serial import PortLease
from test_ota_rom_backup import runtime_fixture


class Journal:
    def __init__(self): self.events = []
    def event(self, kind, **values): self.events.append(dict(kind=kind, **values))


def peer_run(scenario="success", *, cancel_step=None, allow_exit_reset=True, fragment=3):
    peer, journal, cancel, raw = engine.Simulation(scenario, fragment), Journal(), threading.Event(), io.BytesIO()
    link = engine.Link(peer, raw, journal, engine.ModelClock(), cancel)
    observations, error = {}, None
    try:
        for step in engine.experiment(link, observations):
            if step == cancel_step: cancel.set()
    except BaseException as exc:
        error = exc
    result = engine.cleanup(link, allow_exit_reset=allow_exit_reset)
    return peer, link, observations, result, error, journal, raw


class ProtocolTests(unittest.TestCase):
    def test_golden_requests_use_official_uart_physical_map(self):
        # Literal external-reference vectors; do not derive expected bytes with codec helpers.
        self.assertEqual(p.frame(p.ENTER).hex(), "7e79080010004154534633320521")
        self.assertEqual(p.frame(p.EXIT).hex(), "7e79080010004154534633321821")
        self.assertEqual(p.read_word(0).hex(), "7e79080010004072000000a00100")
        self.assertEqual(p.read_word(p.DHCSR).hex(), "7e79080010004072f0ed00f00100")
        guard = p.WriteGuard(); guard.baseline_demcr(0x01000000)
        self.assertEqual(guard.command(p.DHCSR, 0xA05F0003).hex(),
                         "7e790c0010004077f0ed00f0010003005fa0")

    def test_arbitrary_flash_ram_and_core_register_writes_rejected(self):
        guard = p.WriteGuard(); guard.baseline_demcr(0)
        for address in (0x12010000, 0x62010000, 0xA0000000, 0x20000000, p.DCRDR, 0x500CB030, 0x40000000):
            with self.subTest(address=hex(address)), self.assertRaises(p.ProtocolError):
                guard.command(address, 0)
        for address in (0x12010000, 0x12420000, p.DCRSR, p.AIRCR, True):
            with self.subTest(address=address), self.assertRaises(p.ProtocolError): p.read_word(address)
        with self.assertRaises(p.ProtocolError): p.frame(b"arbitrary")
        with self.assertRaises(p.ProtocolError): p.frame(b"burn_all")

    def test_write_phase_and_one_shot_guards(self):
        guard = p.WriteGuard(); guard.baseline_demcr(0)
        with self.assertRaises(p.ProtocolError): guard.command(p.AIRCR, 0x05FA0004)
        guard.command(p.DHCSR, 0xA05F0003)
        guard.command(p.DFSR, 8)
        with self.assertRaises(p.ProtocolError): guard.command(p.DFSR, 1)
        guard.command(p.DEMCR, 1)
        guard.command(p.AIRCR, 0x05FA0004)
        for addr, val in ((p.AIRCR, 0x05FA0004), (p.DHCSR, 0xA05F0003), (p.DCRSR, 0x1000f), (p.DCRDR, 0)):
            with self.subTest(addr=addr,val=val), self.assertRaises(p.ProtocolError): guard.command(addr,val)
        for selector in (13, 15):
            guard.command(p.DCRSR, selector); guard.command(p.DCRSR, selector)
            with self.assertRaises(p.ProtocolError): guard.command(p.DCRSR, selector)

    def test_response_at_every_split_and_noise(self):
        wire = bytes.fromhex("7e7906001000d27856341206")
        for split in range(1, len(wire)):
            parser = p.Reply(0xD2)
            self.assertIsNone(parser.feed(b"boot\r\n" + wire[:split]))
            self.assertEqual(parser.feed(wire[split:]), 0x12345678)
        parser = p.Reply(0xD2)
        value = None
        for byte in wire: value = parser.feed(bytes([byte]))
        self.assertEqual(value, 0x12345678)

    def test_corrupt_oversized_unknown_and_duplicate_replies_rejected(self):
        original = bytes.fromhex("7e7906001000d27856341206")
        for index in (2, 3, 4, 5, 6, 11):
            wire = bytearray(original); wire[index] ^= 1
            with self.subTest(index=index), self.assertRaises(p.ProtocolError): p.Reply(0xD2).feed(wire)
        for wire in (original+original, b"x"*513, bytes.fromhex("7e79ffff1000")):
            with self.subTest(wire=wire[:12]), self.assertRaises(p.ProtocolError): p.Reply(0xD2).feed(wire)
        with self.assertRaises(p.ProtocolError): p.Reply(0xD1).feed(bytes.fromhex("7e7903001000d10006"))


class EngineTests(unittest.TestCase):
    def test_fresh_reset_rom_context_and_hold_with_one_reset(self):
        peer, link, obs, clean, error, journal, raw = peer_run()
        self.assertIsNone(error)
        self.assertTrue(obs["observation_passed"])
        self.assertGreaterEqual(obs["last"]["clock_seconds"]-obs["first"]["clock_seconds"],2.5)
        self.assertEqual((peer.pulses,peer.resets,peer.halt_writes,peer.exits),(1,1,1,1))
        self.assertEqual(peer.demcr, 0x01000000)
        self.assertTrue(clean["debug_release_confirmed"] and clean["resources_closed"] and peer.closed)
        self.assertFalse(clean["exit_acknowledged"])
        self.assertFalse(peer.debug or peer.halted)
        self.assertTrue(raw.getvalue())
        intents = [e for e in journal.events if e["kind"] == "rom_tx_intent"]
        aircr = next(i for i,e in enumerate(intents) if bytes.fromhex(e["hex"])[6:12] == bytes.fromhex("40770ced00f0"))
        later_halt = "7e790c0010004077f0ed00f0010003005fa0"
        self.assertFalse(any(e["hex"]==later_halt for e in intents[aircr+1:]))

    def test_lost_aircr_ack_is_not_retransmitted(self):
        peer, _, obs, clean, error, journal, _ = peer_run("aircr-no-reply")
        self.assertIsNone(error)
        self.assertEqual(peer.resets,1)
        self.assertTrue(obs["observation_passed"] and clean["debug_release_confirmed"])
        self.assertTrue(any(e["kind"]=="reset_response_missing" for e in journal.events))

    def test_bad_reset_evidence_or_context_cannot_pass(self):
        for scenario in ("no-catch","stale-catch","no-reset-flag","pc-outside","pc-mismatch","unstable","vtor","regrdy"):
            with self.subTest(scenario=scenario):
                peer, link, obs, clean, error, _, _ = peer_run(scenario)
                self.assertIsNotNone(error)
                self.assertFalse(obs.get("observation_passed",False))
                self.assertEqual(peer.resets,1)
                self.assertEqual(peer.halt_writes,1)
                self.assertTrue(clean["resources_closed"])

    def test_wire_failures_never_reach_sysreset(self):
        for scenario in ("bad-marker","bad-terminator","long-ack","no-reply","extra-frame","foreign-debug"):
            with self.subTest(scenario=scenario):
                peer, _, obs, clean, error, _, raw = peer_run(scenario)
                self.assertIsNotNone(error)
                self.assertEqual(peer.resets,0)
                self.assertFalse(obs.get("observation_passed",False))
                self.assertTrue(clean["resources_closed"])
                if scenario!="no-reply": self.assertTrue(raw.getvalue())

    def test_cancel_keeps_only_owned_exit_and_no_repeated_reset(self):
        for step in ("entered","caught","holding"):
            with self.subTest(step=step):
                peer, _, obs, clean, error, _, _ = peer_run(cancel_step=step)
                self.assertIsInstance(error,Cancelled)
                self.assertLessEqual(peer.resets,1)
                self.assertEqual(peer.pulses,1)
                self.assertTrue(clean["resources_closed"])
                self.assertFalse(peer.debug or peer.halted)

    def test_short_write_does_not_retry_and_cleanup_is_separate(self):
        peer, _, obs, clean, error, _, _ = peer_run("short-write")
        self.assertIsNotNone(error)
        self.assertEqual(peer.resets,0)
        halt = bytes.fromhex("7e790c0010004077f0ed00f0010003005fa0")
        self.assertEqual(peer.commands.count(halt),1)
        self.assertTrue(clean["debug_release_confirmed"])

    def test_exit_failure_preserves_proof_and_limits_recovery_reset(self):
        for allowed in (False, True):
            with self.subTest(allowed=allowed):
                peer, _, obs, clean, error, _, _ = peer_run("exit-fail",allow_exit_reset=allowed)
                self.assertIsNone(error)
                self.assertTrue(obs["observation_passed"])
                self.assertTrue(clean["errors"])
                self.assertEqual(peer.pulses,2 if allowed else 1)
                self.assertEqual(peer.exits,1)
                self.assertEqual(peer.resets,1)

    def test_close_or_rts_failure_not_reported_as_recovered(self):
        peer, _, obs, clean, error, _, _ = peer_run("close-fail")
        self.assertFalse(clean["resources_closed"])
        self.assertTrue(clean["errors"])
        peer, _, obs, clean, error, _, _ = peer_run("rts-release-fail")
        self.assertIsNotNone(error)
        self.assertFalse(clean["rts_released"])
        self.assertTrue(clean["errors"])


class StartupTests(unittest.TestCase):
    """The observed boot line is an entry preamble, never a reusable reply."""
    def link(self, data, fragment=256):
        peer, clock, raw, journal = engine.Simulation(fragment=fragment), engine.ModelClock(), io.BytesIO(), Journal()
        original = peer.set_rts
        def rts(high):
            original(high)
            if not high and peer.pulses == 1: peer.queue.extend(data)
        peer.set_rts = rts
        link = engine.Link(peer, raw, journal, clock, threading.Event())
        link.pulse()
        return link, peer, raw, journal

    def test_observed_fixture_and_crlf_at_every_fragment_reach_first_enter(self):
        fixture = (ROOT / "tests/fixtures/ota-rom-entry-sfbl-20261109.uart").read_bytes()
        self.assertEqual(fixture, bytes.fromhex("5346424c0a"))
        for banner in (fixture, b"SFBL\r\n"):
            for fragment in range(1, len(banner)+1):
                with self.subTest(banner=banner, fragment=fragment):
                    link, peer, raw, journal = self.link(banner, fragment)
                    self.assertTrue(link.enter(first=True))
                    self.assertEqual(peer.commands, [p.frame(p.ENTER)])
                    self.assertTrue(raw.getvalue().startswith(banner))
                    event = next(e for e in journal.events if e["kind"] == "rom_startup_preamble")
                    self.assertEqual(event["hex"], banner.hex())

    def test_old_replies_and_partial_headers_cannot_become_first_reply(self):
        frames = [bytes.fromhex(wire) for wire in
                  ("7e7902001000d106", "7e7906001000d27856341206", "7e7902001000d306", "7e", "7e79", "7e7902")]
        for prefix in (b"", b"SFBL\n"):
            for frame in frames:
                for fragment in (1, 3, 256):
                    with self.subTest(prefix=prefix, frame=frame, fragment=fragment):
                        link, peer, raw, _ = self.link(prefix+frame, fragment)
                        with self.assertRaises(p.ProtocolError): link.enter(first=True)
                        self.assertEqual(peer.commands, [])
                        self.assertTrue(raw.getvalue())

    def test_unknown_binary_text_and_repeated_lines_are_rejected(self):
        for value in (b"junk\n", b"SFBL\njunk", b"SFBL\nSFBL\n", b"\0", b"\xff", b"~", b"~y", b"SFBL\0"):
            with self.subTest(value=value):
                link, peer, _, _ = self.link(value)
                with self.assertRaises(p.ProtocolError): link.enter(first=True)
                self.assertEqual(peer.commands, [])

    def test_incomplete_line_has_fixed_deadline_and_no_transmission(self):
        for value in (b"S", b"SFBL", b"SFBL\r"):
            with self.subTest(value=value):
                link, peer, raw, _ = self.link(value, 1)
                start = link.clock.now()
                with self.assertRaises(TimeoutError): link.enter(first=True)
                self.assertLessEqual(link.clock.now()-start, 1.003)
                self.assertEqual(raw.getvalue(), value)
                self.assertEqual(peer.commands, [])

    def test_entry_exception_requires_owned_release_and_is_consumed_once(self):
        peer = engine.Simulation()
        link = engine.Link(peer, io.BytesIO(), Journal(), engine.ModelClock(), threading.Event())
        with self.assertRaises(p.ProtocolError): link.enter(first=True)
        link, peer, _, _ = self.link(b"unknown")
        with self.assertRaises(p.ProtocolError): link.enter(first=True)
        peer.queue.clear()
        with self.assertRaises(p.ProtocolError): link.enter(first=True)
        self.assertEqual(peer.commands, [])

    def test_later_enter_reads_writes_and_exit_keep_strict_pre_read(self):
        for operation in (lambda link: link.enter(), lambda link: link.read(0),
                          lambda link: link.write(p.DHCSR, 0xA05F0003),
                          lambda link: link.request(p.frame(p.EXIT), None)):
            link, peer, _, _ = self.link(b"")
            self.assertTrue(link.enter(first=True))
            link.guard.baseline_demcr(0)
            peer.queue.extend(b"SFBL\n")
            with self.assertRaises(p.ProtocolError): operation(link)
            self.assertEqual(peer.commands, [p.frame(p.ENTER)])

    def test_release_deadline_cannot_move_with_late_first_call_or_read(self):
        for delay, late_read in ((1.901, False), (1.85, True)):
            with self.subTest(delay=delay):
                link, peer, _, _ = self.link(b"")
                link.clock.value = link.entry_released_at + delay
                if late_read:
                    original = peer.read
                    def read(count): link.clock.sleep(.06); return original(count)
                    peer.read = read
                with self.assertRaises(TimeoutError): link.enter(first=True)
                self.assertEqual(peer.commands, [])

    def test_first_ack_must_complete_before_absolute_release_limit(self):
        for epsilon in (-.001, .001):
            with self.subTest(epsilon=epsilon):
                link, peer, raw, _ = self.link(b"")
                link.clock.value = link.entry_released_at + 1.85
                original = peer.read
                def read(count):
                    if peer.commands: link.clock.value = link.entry_released_at + 1.9 + epsilon
                    return original(count)
                peer.read = read
                if epsilon < 0: self.assertTrue(link.enter(first=True))
                else:
                    with self.assertRaises(engine.NoReply): link.enter(first=True)
                self.assertEqual(peer.commands, [p.frame(p.ENTER)])
                self.assertTrue(raw.getvalue())

    def test_cancel_during_preamble_keeps_raw_and_sends_no_enter(self):
        link, peer, raw, _ = self.link(b"SFBL\n", 1)
        original = peer.read
        def read(count):
            value = original(count); link.cancel.set(); return value
        peer.read = read
        with self.assertRaises(Cancelled): link.enter(first=True)
        self.assertEqual(raw.getvalue(), b"S")
        self.assertEqual(peer.commands, [])

    def test_gaps_between_startup_bytes_stay_within_one_deadline(self):
        link, peer, raw, _ = self.link(b"SFBL\r\n", 1)
        original, calls = peer.read, 0
        def read(count):
            nonlocal calls
            calls += 1
            if not peer.commands and calls % 2 == 0: return b""
            return original(count)
        peer.read = read
        self.assertTrue(link.enter(first=True))
        self.assertTrue(raw.getvalue().startswith(b"SFBL\r\n"))
        self.assertLess(link.clock.now(), link.entry_released_at+1.9)

    def test_continuous_noise_cannot_grow_the_allowed_preamble(self):
        for value in (b"S", b"SFBL\n", b"\n"):
            with self.subTest(value=value):
                link, peer, _, _ = self.link(b"")
                reads = []
                def read(count): reads.append(count); return value
                peer.read = read
                with self.assertRaises(p.ProtocolError): link.enter(first=True)
                self.assertLessEqual(len(reads), 2)
                self.assertEqual(peer.commands, [])

    def test_logging_deadline_or_cancel_prevents_actual_transmission(self):
        for event in ("rom_startup_preamble", "rom_tx_intent"):
            for cancel in (False, True):
                with self.subTest(event=event, cancel=cancel):
                    link, peer, _, journal = self.link(b"SFBL\n")
                    original = journal.event
                    def log(kind, **values):
                        original(kind, **values)
                        if kind == event:
                            if cancel: link.cancel.set()
                            else: link.clock.sleep(1.1)
                    journal.event = log
                    with self.assertRaises(Cancelled if cancel else TimeoutError): link.enter(first=True)
                    self.assertEqual(peer.commands, [])

    def test_deassert_driver_time_does_not_shift_release_limit(self):
        peer = engine.Simulation()
        clock = engine.ModelClock()
        original = peer.set_rts
        def rts(high):
            if not high: clock.sleep(1.95)
            original(high)
        peer.set_rts = rts
        link = engine.Link(peer, io.BytesIO(), Journal(), clock, threading.Event())
        link.pulse()
        self.assertLess(link.entry_released_at, .11)
        with self.assertRaises(TimeoutError): link.enter(first=True)
        self.assertEqual(peer.commands, [])

    def test_preamble_cancellation_cleanup_closes_with_only_allowed_fallback(self):
        for allowed in (False, True):
            with self.subTest(allowed=allowed):
                link, peer, raw, _ = self.link(b"SFBL\n", 1)
                original = peer.read
                def read(count):
                    value = original(count)
                    if not link.cleaning: link.cancel.set()
                    return value
                peer.read = read
                with self.assertRaises(Cancelled): link.enter(first=True)
                result = engine.cleanup(link, allow_exit_reset=allowed)
                self.assertTrue(result["resources_closed"] and result["rts_released"] and peer.closed)
                self.assertFalse(result["debug_release_confirmed"])
                self.assertEqual(peer.pulses, 2 if allowed else 1)
                self.assertFalse(any(wire == p.frame(p.ENTER) for wire in peer.commands))
                self.assertEqual(peer.resets, 0)
                self.assertTrue(raw.getvalue().startswith(b"SF"))

    def test_banner_peer_completes_full_model_without_extra_reset(self):
        peer, _, observations, cleaned, error, _, raw = peer_run("startup-banner")
        self.assertIsNone(error)
        self.assertTrue(observations["observation_passed"] and cleaned["debug_release_confirmed"])
        self.assertTrue(raw.getvalue().startswith(b"SFBL\n"))
        self.assertEqual((peer.pulses, peer.resets), (1, 1))


class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=ROOT/"artifacts")
        self.root = Path(self.tmp.name)
    def tearDown(self): self.tmp.cleanup()

    def run_lab(self, scenario="success", *, cancel_phase=None, after_boot=2, budget=10, wire_factory=None):
        ownership = []
        peers = []
        def factory(phase):
            try:
                with PortLease("SIMULATED-OTA-ROM-RESCUE", self.root/"simulation-locks"):
                    ownership.append(False)
            except RuntimeError: ownership.append(True)
            peer=(wire_factory or engine.Simulation)(phase)
            peers.append(peer)
            return peer
        def ready(console):
            def operator():
                while not console.sealed:
                    with console.condition:
                        prompt=console.prompt_value
                        if prompt and prompt["phase"]==cancel_phase:
                            console.cancel(); return
                        if prompt and not prompt.get("automatic") and console.answer is None:
                            console.respond(prompt["id"],"continue")
                    time.sleep(.001)
            threading.Thread(target=operator,daemon=True).start()
        result=lab.simulate(self.root/"run",runtime_fixture(1),runtime_fixture(after_boot),scenario=scenario,
            ready=ready,budget_seconds=budget,prompt_seconds=1,step_delay=.001,wire_factory=factory)
        return result,ownership,peers

    def test_success_has_no_hardware_claims_and_continuous_lease(self):
        result,ownership,peers=self.run_lab()
        self.assertTrue(result["passed"],result)
        self.assertTrue(all(ownership))
        self.assertEqual(len(peers),1)
        self.assertTrue(all(item["passed"] for item in result["verification"]))
        self.assertFalse(any(result[k] for k in ("hardware_verified","rom_rescue_verified","deployment_authorized","flash_write","clock_write","device_opened","physical_observations_confirmed")))
        self.assertTrue((self.root/"run/rom-rx.bin").read_bytes())
        with PortLease("SIMULATED-OTA-ROM-RESCUE",self.root/"simulation-locks"): pass

    def test_cancel_before_entry_does_not_create_wire_peer(self):
        result,_,peers=self.run_lab(cancel_phase="prepare")
        self.assertEqual(result["outcome"],"cancelled")
        self.assertFalse(result["entry_attempted"])
        self.assertEqual(peers,[])

    def test_cancel_automatic_stage_closes_owned_peer(self):
        result,_,peers=self.run_lab(cancel_phase="rom")
        self.assertEqual(result["outcome"],"cancelled")
        self.assertTrue(result["resources_closed"])
        self.assertTrue(peers[0].closed)
        self.assertLessEqual(peers[0].pulses,1)

    def test_failed_poststate_does_not_hide_rom_proof(self):
        result,_,_=self.run_lab(after_boot=3)
        self.assertFalse(result["passed"] or result["application_recovery_passed"])
        self.assertTrue(result["observation_passed"] and result["recovery_required"])

    def test_exit_and_close_failure_cannot_report_workflow_success(self):
        for scenario in ("exit-fail","close-fail"):
            with self.subTest(scenario=scenario):
                self.root=Path(self.tmp.name)/scenario; self.root.mkdir()
                result,_,_=self.run_lab(scenario)
                self.assertFalse(result["passed"])
                self.assertTrue(result["observation_passed"])
                self.assertEqual(result["resources_closed"],scenario!="close-fail")

    def test_global_deadline_stops_work_and_retains_files(self):
        started=time.monotonic()
        result,_,_=self.run_lab(budget=.02)
        self.assertEqual(result["outcome"],"timeout")
        self.assertLess(time.monotonic()-started,2)
        self.assertTrue((self.root/"run/rescue.json").is_file())

    def test_cli_has_no_capture_or_com_entry(self):
        path=ROOT/"scripts/ota_rom_rescue_lab.py"
        for args in (("capture",),("simulate","--port","COM5")):
            result=subprocess.run([sys.executable,str(path),*args],capture_output=True,timeout=5)
            self.assertNotEqual(result.returncode,0)
        source=path.read_text(encoding="utf-8")
        self.assertNotIn("import serial",source)

    def test_link_constructor_failure_closes_already_owned_peer(self):
        with patch.object(lab,"Link",side_effect=RuntimeError("constructor failed")):
            result,_,peers=self.run_lab()
        self.assertFalse(result["passed"] or result["entry_attempted"])
        self.assertTrue(result["resources_closed"] and peers[0].closed)

    def test_budget_expiry_during_automatic_call_still_cleans_peer(self):
        original=lab.Budget.remaining
        def budget_failure(budget,limit=120):
            if limit==30: raise TimeoutError("budget expired at automatic argument")
            return original(budget,limit)
        with patch.object(lab.Budget,"remaining",budget_failure):
            result,_,peers=self.run_lab()
        self.assertEqual(result["outcome"],"timeout")
        self.assertTrue(result["resources_closed"] and peers[0].closed)


if __name__ == "__main__": unittest.main()
