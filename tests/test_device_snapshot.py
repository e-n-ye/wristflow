"""Exercise snapshot permissions, bounded failures and raw evidence with fake UART."""
import io
from pathlib import Path
import sys
import tempfile
import threading
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import device_snapshot as lab
from battery_lab import Journal, read_progress


class SnapshotTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def collect(self, peer=None, **kwargs):
        peer = peer or lab.Simulation()
        report = lab.collect(self.root / "run", peer, binding={"simulation": True},
                             identity_check=kwargs.pop("identity_check", lambda: {}), locks=self.root / "locks", **kwargs)
        return peer, report

    def test_selected_snapshot_uses_shared_session_without_clock_control(self):
        peer, report = self.collect(queries=("steps", "wrist"))
        self.assertTrue(report["passed"] and report["resources_closed"])
        self.assertEqual(peer.commands, ["wf_steps status", "wf_wrist status"])
        self.assertFalse(report["clock_write_attempted"] or report["hardware_verified"])
        self.assertIn(b"[steps-hours]", (self.root / "run/uart.bin").read_bytes())
        self.assertEqual(read_progress(self.root / "run")["state"], "finished")
        journal = Journal(self.root / "allowlist", binding={})
        client = lab.SnapshotClient(peer, io.BytesIO(), journal, threading.Event())
        for command in ("wf_time 100", "reboot", "wf_cprobe stop", "wf_pm allow", "wf_steps save"):
            with self.subTest(command=command), self.assertRaises(ValueError):
                client.request(command, lambda line: True)

    def test_startup_sync_is_explicit_single_send_and_reads_back(self):
        peer, report = self.collect(lab.Simulation(stale_clock=True), startup=True, allow_time_sync=True, queries=("steps",))
        self.assertTrue(report["passed"], report)
        self.assertTrue(report["clock_write_attempted"])
        self.assertEqual(sum(c.startswith("wf_time ") for c in peer.commands), 1)
        self.assertEqual(peer.commands.count("show_date"), 2)

    def test_storage_metadata_snapshot_is_bounded_and_validated(self):
        peer, report = self.collect(queries=("storage",))
        self.assertTrue(report["passed"], report)
        self.assertEqual(peer.commands, ["wf_storage scan"])
        self.assertEqual(lab.parse_storage(report["replies"]["storage"])["total"]["tail"], 16320)

    def test_storage_bad_accounting_or_duplicate_rows_fail_closed(self):
        from storage_protocol import simulated_storage, parse_storage
        lines = simulated_storage()
        for bad in (lines + [lines[-1]], lines[:-1], [line.replace("tail=16320", "tail=16321") for line in lines],
                    [line.replace("coherent=1", "coherent=2") for line in lines]):
            with self.assertRaises(ValueError): parse_storage(bad)
        peer = lab.Simulation()
        original = peer.write
        def write(wire):
            count = original(wire)
            peer.queue[:] = bytes(peer.queue).replace(b"tail=16320", b"tail=16321")
            return count
        peer.write = write
        _, report = self.collect(peer, queries=("storage",))
        self.assertFalse(report["passed"])
        self.assertTrue(report["resources_closed"])

    def test_storage_rejects_fragments_failed_results_and_startup(self):
        from storage_protocol import simulated_storage, parse_storage
        lines = simulated_storage()
        for bad in (lines + ["[storage-sector]"],
                    [line.replace("result=0", "result=4") for line in lines],
                    [line.replace("live=0", "live=4294967296") for line in lines]):
            with self.assertRaises(ValueError): parse_storage(bad)
        self.assertEqual(parse_storage([line.replace("coherent=1", "coherent=0") for line in lines])["done"]["coherent"], 0)
        with self.assertRaises(ValueError): lab.validate(("storage",), True, True, 60)

    def test_storage_requires_diagnostic_build_before_binding(self):
        import json
        result = self.root / "result.json"
        args = ["device_snapshot.py", "capture", "--query", "storage", "--out", str(self.root / "cli"),
                "--port", "COM5", "--location", "1-1.1", "--expected-bin", "a"*64,
                "--result", str(result), "--flash", "flash.json", "--confirm-same-board-and-firmware"]
        for invalid in (False, "false", 1, None):
            result.write_text(json.dumps({"storage_probe": invalid}), encoding="utf-8")
            with patch.object(sys, "argv", args), patch.object(lab, "prepare_capture") as prepare:
                with self.assertRaises(ValueError): lab.main()
                prepare.assert_not_called()

    def test_current_rtc_is_not_rewritten(self):
        peer, report = self.collect(startup=True, allow_time_sync=True, queries=("steps",))
        self.assertTrue(report["passed"])
        self.assertFalse(report["clock_write_attempted"])

    def test_clock_within_sync_threshold_is_accepted_without_write(self):
        import time
        peer = lab.Simulation()
        peer.clock = int(time.time()) - 30
        _, report = self.collect(peer, startup=True, allow_time_sync=True, queries=("steps",))
        self.assertTrue(report["passed"], report)
        self.assertFalse(report["clock_write_attempted"])

    def test_lost_time_ack_fails_without_repeat_or_reconnect(self):
        peer = lab.Simulation(stale_clock=True)
        original = peer.write
        def write(wire):
            count = original(wire)
            if wire.startswith(b"wf_time "):
                peer.queue.clear()
            return count
        peer.write = write
        _, report = self.collect(peer, startup=True, allow_time_sync=True, seconds=.1)
        self.assertEqual(report["outcome"], "timeout")
        self.assertTrue(report["clock_write_attempted"] and report["resources_closed"])
        self.assertEqual(sum(c.startswith("wf_time ") for c in peer.commands), 1)

    def test_truncated_reply_and_short_write_fail_without_retry(self):
        for kind in ("truncated", "short_write"):
            peer = lab.Simulation()
            original = peer.write
            def write(wire):
                count = original(wire)
                if kind == "truncated":
                    peer.queue[:] = b"[steps-hours] partial"
                return count - 1 if kind == "short_write" else count
            peer.write = write
            out = self.root / kind
            report = lab.collect(out, peer, binding={"simulation": True}, identity_check=lambda: {},
                                 locks=self.root / "locks", queries=("steps",), seconds=.08)
            self.assertFalse(report["passed"])
            self.assertTrue(report["resources_closed"])
            self.assertEqual(peer.commands, ["wf_steps status"])

    def test_cancel_identity_failure_and_cold_boot_close_resources(self):
        cancel = threading.Event()
        cancel.set()
        peer, report = self.collect(cancel=cancel)
        self.assertFalse(peer.opened)
        self.assertTrue(report["resources_closed"])
        self.assertEqual(peer.commands, [])
        def fail():
            raise ValueError("changed binding")
        other = self.root / "other"
        peer = lab.Simulation()
        report = lab.collect(other, peer, binding={"simulation": True}, identity_check=fail, locks=self.root / "locks")
        self.assertFalse(peer.opened or report["passed"])
        self.assertTrue(report["resources_closed"])
        peer = lab.Simulation()
        peer.queue.extend(b"SFBL cold boot\r\n")
        report = lab.collect(self.root / "boot", peer, binding={"simulation": True}, identity_check=lambda: {}, locks=self.root / "locks")
        self.assertFalse(report["passed"])
        self.assertTrue(report["resources_closed"])
        self.assertEqual(peer.commands, [])

    def test_invalid_plan_and_missing_startup_permission_fail_before_open(self):
        for kwargs in ({"queries": ("steps", "steps")}, {"queries": ("reboot",)}, {"seconds": 61},
                       {"startup": True}, {"allow_time_sync": True}):
            peer = lab.Simulation()
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                self.collect(peer, **kwargs)
            self.assertFalse(peer.opened)
        args = ["device_snapshot.py", "startup", "--out", str(self.root / "cli"), "--port", "COM5",
                "--location", "1-1.1", "--expected-bin", "a"*64, "--result", "build.json", "--flash", "flash.json",
                "--confirm-same-board-and-firmware"]
        with patch.object(sys, "argv", args), patch.object(lab, "prepare_capture") as prepare:
            with self.assertRaises(SystemExit):
                lab.main()
            prepare.assert_not_called()


if __name__ == "__main__":
    unittest.main()
