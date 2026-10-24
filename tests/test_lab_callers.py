"""Second callers exercise the shared transport without any physical endpoint."""
import json
import pathlib
import sys
import threading
import unittest
import uuid
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from battery_lab import Journal, read_progress
from lab_serial import ProtocolError
import serial_lab as lab


class CallerTests(unittest.TestCase):
    def setUp(self):
        self.directory = ROOT / "artifacts/lab-caller-tests" / uuid.uuid4().hex
        self.directory.mkdir(parents=True)

    def record(self, mode, **kwargs):
        peer = kwargs.pop("peer", lab.Simulation(mode))
        report = lab.record(self.directory / "run", peer, mode=mode, seconds=.15, interval=.03,
                            identity_check=lambda: {"simulation": True}, binding={"simulation": True},
                            locks=self.directory / "locks", **kwargs)
        return peer, report

    def test_light_only_uses_cached_query_and_records_complete_status(self):
        peer, report = self.record("light")
        self.assertTrue(report["passed"], report)
        self.assertGreaterEqual(len(report["observations"]), 2)
        self.assertEqual(set(peer.commands), {"wf_light status"})
        self.assertTrue(report["resources_closed"])
        self.assertFalse(report["hardware_verified"])
        self.assertEqual(read_progress(self.directory / "run")["state"], "finished")

    def test_passive_never_transmits_and_preserves_raw_bytes(self):
        peer, report = self.record("passive")
        self.assertTrue(report["passed"])
        self.assertEqual(peer.commands, [])
        self.assertGreater(report["uart"]["bytes"], 0)
        self.assertIn(b"[aod] simulated", (self.directory / "run/uart.bin").read_bytes())
        self.assertTrue(peer.closed)

    def test_passive_client_rejects_even_readonly_commands(self):
        peer = lab.Simulation("passive")
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        import io
        client = lab.PassiveClient(peer, io.BytesIO(), journal, threading.Event())
        with self.assertRaises(ValueError):
            client.request("wf_light status", lambda line: True, cleanup=True)
        self.assertEqual(peer.commands, [])

    def test_cancel_closes_without_issuing_cleanup_commands(self):
        peer, report = self.record("passive", ready=lambda cancel: cancel.set())
        self.assertEqual(report["outcome"], "cancelled")
        self.assertFalse(report["passed"])
        self.assertEqual(peer.commands, [])
        self.assertTrue(report["resources_closed"])

    def test_invalid_light_value_fails_without_byte_repair(self):
        peer = lab.Simulation("light")
        original = peer.write
        def write(value):
            result = original(value)
            peer.queue[:] = peer.queue.replace(b"brightness=23", b"brightness=101")
            return result
        peer.write = write
        _, report = self.record("light", peer=peer)
        self.assertEqual(report["outcome"], "error")
        self.assertIn(b"brightness=101", (self.directory / "run/uart.bin").read_bytes())
        self.assertEqual(len(peer.commands), 2)
        self.assertTrue(report["resources_closed"])

    def test_partial_readonly_write_has_only_one_bounded_retry(self):
        peer = lab.Simulation("light")
        original = peer.write
        peer.write = lambda value: original(value)-1
        _, report = self.record("light", peer=peer)
        self.assertFalse(report["passed"])
        # Read-only protocol errors may have one bounded repeat, unlike control.
        self.assertEqual(len(peer.commands), 2)

    def test_cancel_during_final_drain_cannot_pass(self):
        peer = lab.Simulation("passive")
        cancel = threading.Event()
        original = peer.read
        def read(count):
            value = original(count)
            cancel.set()
            return value
        peer.read = read
        _, report = self.record("passive", peer=peer, cancel=cancel)
        self.assertEqual(report["outcome"], "cancelled")
        self.assertFalse(report["passed"])
        self.assertTrue(report["resources_closed"])

    def test_expired_budget_never_sends_or_retries(self):
        peer = lab.Simulation("light")
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        import io
        client = lab.LightClient(peer, io.BytesIO(), journal, threading.Event())
        with self.assertRaises(TimeoutError):
            client.status(time.monotonic()-.01)
        self.assertEqual(peer.commands, [])

    def test_cancel_during_prequery_drain_never_transmits(self):
        peer = lab.Simulation("light")
        cancel = threading.Event()
        peer.read = lambda count: (cancel.set(), b"")[1]
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        import io
        from battery_console import Cancelled
        client = lab.LightClient(peer, io.BytesIO(), journal, cancel)
        with self.assertRaises(Cancelled):
            client.status(time.monotonic()+1)
        self.assertEqual(peer.commands, [])

    def test_historical_light_and_aod_raw_replays(self):
        light = lab.replay(ROOT / "tests/fixtures/light-status-20261024.json", self.directory / "light")
        passive = lab.replay(ROOT / "tests/fixtures/aod-passive-20261023.json", self.directory / "passive")
        self.assertEqual([row["seq"] for row in light["observations"]], [11, 16, 21])
        self.assertEqual(passive["bytes"], 5901)
        self.assertEqual(passive["source"]["session"]["commands"], [])
        self.assertFalse(light["hardware_verified"])
        self.assertFalse(passive["hardware_verified"])


if __name__ == "__main__":
    unittest.main()
