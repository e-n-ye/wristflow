"""Observed debug-disable ACK/tail and independent release evidence; no device."""
import io
from pathlib import Path
import sys
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_rom_protocol as p
import ota_rom_rescue as engine
from test_ota_rom_rescue import Journal, peer_run

RELEASE = bytes.fromhex("7e790c0010004077f0ed00f0010000005fa0")
ACK = bytes.fromhex("7e7902001000d306")
FIXTURE = ROOT / "tests/fixtures/ota-rom-release-sfbl-20261109.uart"


class ReleaseTests(unittest.TestCase):
    def prepared(self, reply=None, fragment=256):
        peer, raw, clock, journal = engine.Simulation(fragment=fragment), io.BytesIO(), engine.ModelClock(), Journal()
        link = engine.Link(peer, raw, journal, clock, threading.Event())
        observations = {}
        for _ in engine.experiment(link, observations): pass
        original = peer.write
        def write(wire):
            count = original(wire)
            if wire == RELEASE and reply is not None:
                peer.queue[:] = reply
            return count
        peer.write = write
        return link, peer, raw, journal

    def assert_once(self, peer, fallback=False):
        self.assertEqual(peer.commands.count(RELEASE), 1)
        self.assertEqual((peer.resets, peer.halt_writes, peer.pulses), (1, 1, 2 if fallback else 1))
        self.assertTrue(peer.closed)

    def test_observed_fixture_and_every_fragment_have_independent_readback(self):
        fixture = FIXTURE.read_bytes()
        self.assertEqual(fixture, ACK + bytes.fromhex("5346424c0a"))
        for reply in (ACK, fixture, ACK + b"SFBL\r\n"):
            for fragment in range(1, len(reply) + 1):
                with self.subTest(reply=reply, fragment=fragment):
                    link, peer, raw, journal = self.prepared(reply, fragment)
                    result = engine.cleanup(link, allow_exit_reset=False)
                    self.assertEqual(result["errors"], [])
                    self.assertTrue(result["debug_release_acknowledged"] and result["debug_release_confirmed"])
                    self.assertIsInstance(result["debug_release_readback"], int)
                    self.assertFalse(result["debug_release_readback"] & (engine.HALT | engine.DEBUG))
                    index = peer.commands.index(RELEASE)
                    self.assertEqual(peer.commands[index + 1:], [p.read_word(p.DHCSR), p.frame(p.EXIT)])
                    tail = next(e for e in journal.events if e["kind"] == "rom_debug_release_tail")
                    self.assertEqual(raw.getvalue()[tail["start"]:tail["end"]], reply[len(ACK):])
                    self.assert_once(peer)

    def test_every_two_chunk_split_preserves_exact_raw_tail(self):
        fixture = FIXTURE.read_bytes()
        for split in range(1, len(fixture)):
            link, peer, raw, _ = self.prepared()
            write, read, chunks = peer.write, peer.read, []
            def send(wire):
                count = write(wire)
                if wire == RELEASE:
                    peer.queue.clear(); chunks.extend((fixture[:split], fixture[split:]))
                return count
            def receive(count): return chunks.pop(0) if chunks else read(count)
            peer.write, peer.read = send, receive
            result = engine.cleanup(link, allow_exit_reset=False)
            self.assertTrue(result["debug_release_confirmed"], (split, result))
            self.assertIn(fixture, raw.getvalue())
            self.assert_once(peer)

    def test_partial_tail_gaps_wait_without_extending_deadline(self):
        link, peer, _, _ = self.prepared(ACK + b"SFBL\n", 1)
        original, calls = peer.read, 0
        def read(count):
            nonlocal calls
            if peer.commands and peer.commands[-1] == RELEASE:
                calls += 1
                # Gaps inside ACK/line; a quiet complete ACK may end its transaction.
                if calls % 2 == 0 and len(peer.queue) != 5: return b""
            return original(count)
        peer.read = read
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertTrue(result["debug_release_confirmed"], result)
        self.assert_once(peer)

    def test_unknown_repeated_and_old_frames_never_become_readback(self):
        bad_tails = (b"x", b"\0", b"\xff", b"SFBL\nSFBL\n", b"SFBL\n~", b"~y",
                     ACK, bytes.fromhex("7e7902001000d106"),
                     bytes.fromhex("7e7906001000d20000000006"))
        for tail in bad_tails:
            for fragment in (1, 3, 256):
                with self.subTest(tail=tail, fragment=fragment):
                    link, peer, _, _ = self.prepared(ACK + tail, fragment)
                    result = engine.cleanup(link, allow_exit_reset=False)
                    self.assertTrue(result["errors"])
                    self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
                    self.assertEqual(peer.commands[-1], RELEASE)
                    self.assert_once(peer)

    def test_bad_ack_cannot_be_hidden_by_valid_startup_text(self):
        for index in range(len(ACK)):
            wire = bytearray(ACK + b"SFBL\n"); wire[index] ^= 1
            link, peer, _, _ = self.prepared(wire)
            result = engine.cleanup(link, allow_exit_reset=False)
            self.assertFalse(result["debug_release_acknowledged"] or result["debug_release_confirmed"])
            self.assertTrue(result["errors"])
            self.assert_once(peer)

    def test_partial_ack_or_tail_timeout_preserves_raw_and_cleanup_limit(self):
        for reply in (ACK[:3], ACK + b"S", ACK + b"SFBL", ACK + b"SFBL\r", b""):
            link, peer, raw, _ = self.prepared(reply)
            before = link.clock.now()
            result = engine.cleanup(link, allow_exit_reset=False)
            self.assertTrue(result["errors"])
            self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
            self.assertLessEqual(link.clock.now() - before, 1.003)
            if reply: self.assertTrue(raw.getvalue().endswith(reply))
            self.assert_once(peer)

    def test_ack_and_banner_cannot_substitute_for_independent_state(self):
        for scenario in ("release-old-frame", "release-readback-fail"):
            peer, _, obs, result, error, _, _ = peer_run(scenario)
            self.assertIsNone(error)
            self.assertTrue(obs["observation_passed"])
            self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
            self.assertTrue(result["errors"])
            self.assert_once(peer, fallback=True)
        self.assertTrue(result["debug_release_acknowledged"])
        self.assertTrue(result["debug_release_readback"] & (engine.HALT | engine.DEBUG))

    def test_no_readback_reply_cannot_report_release(self):
        link, peer, _, _ = self.prepared(ACK + b"SFBL\n")
        original = peer.write
        def write(wire):
            count = original(wire)
            if wire == p.read_word(p.DHCSR) and RELEASE in peer.commands: peer.queue.clear()
            return count
        peer.write = write
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertTrue(result["debug_release_acknowledged"])
        self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
        self.assertIsNone(result["debug_release_readback"])
        self.assert_once(peer)

    def test_late_ack_or_late_quiet_read_is_rejected(self):
        for delay_empty in (False, True):
            link, peer, raw, _ = self.prepared(ACK + b"SFBL\n")
            original = peer.read
            def read(count):
                value = original(count)
                if peer.commands[-1] == RELEASE and bool(value) != delay_empty: link.clock.sleep(1.01)
                return value
            peer.read = read
            result = engine.cleanup(link, allow_exit_reset=False)
            self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
            self.assertTrue(result["errors"])
            self.assertIn(ACK + b"SFBL\n", raw.getvalue())
            self.assert_once(peer)

    def test_cancelled_experiment_still_finishes_only_owned_cleanup(self):
        link, peer, _, _ = self.prepared(ACK + b"SFBL\n")
        link.cancel.set()
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertTrue(result["debug_release_confirmed"])
        self.assert_once(peer)

    def test_release_exception_requires_owned_cleanup_and_is_single_use(self):
        link, peer, _, _ = self.prepared()
        with self.assertRaises(p.ProtocolError): link.request(RELEASE, 0xD3, release=True)
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertTrue(result["debug_release_confirmed"])
        count = len(peer.commands)
        with self.assertRaises(p.ProtocolError): link.request(RELEASE, 0xD3, release=True)
        self.assertEqual(len(peer.commands), count)

    def test_later_banner_keeps_independent_readback_pre_read_strict(self):
        link, peer, _, _ = self.prepared()
        original = peer.read
        injected = False
        def read(count):
            nonlocal injected
            value = original(count)
            if peer.commands[-1] == RELEASE and not value and not injected:
                injected = True; peer.queue.extend(b"SFBL\n")
            return value
        peer.read = read
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertTrue(result["debug_release_acknowledged"])
        self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
        self.assertEqual(peer.commands[-1], RELEASE)
        self.assert_once(peer)

    def test_readback_response_rejects_late_text_and_old_frames(self):
        for prefix in (b"SFBL\n", b"x", ACK, bytes.fromhex("7e7902001000d106"), b"~"):
            for fragment in (1, 256):
                with self.subTest(prefix=prefix, fragment=fragment):
                    link, peer, _, _ = self.prepared(ACK, fragment)
                    original = peer.write
                    def write(wire):
                        count = original(wire)
                        if wire == p.read_word(p.DHCSR) and RELEASE in peer.commands:
                            peer.queue[:0] = prefix
                        return count
                    peer.write = write
                    result = engine.cleanup(link, allow_exit_reset=False)
                    self.assertTrue(result["debug_release_acknowledged"])
                    self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
                    self.assertTrue(result["errors"])
                    self.assert_once(peer)

    def test_release_request_is_cut_short_by_remaining_cleanup_time(self):
        link, peer, _, _ = self.prepared(ACK + b"S")
        started = link.clock.now()
        link.window_end = started + .25
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertAlmostEqual(link.clock.now() - started, .25)
        self.assertTrue(result["errors"] and result["resources_closed"])
        self.assertFalse(result["debug_release_confirmed"] or result["exit_sent"])
        self.assert_once(peer)


if __name__ == "__main__": unittest.main()
