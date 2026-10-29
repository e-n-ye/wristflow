import argparse
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import resource_acceptance as target


class Console:
    def __init__(self, *args, **kwargs):
        self.cancelled = threading.Event()
        self.url = "simulated"

    def __enter__(self): return self
    def __exit__(self, *args): pass
    def prompt(self, *args, **kwargs): time.sleep(.05)
    def stage(self, *args): pass
    def seal(self, outcome): return outcome
    def finish(self, *args): pass


class Acceptance(unittest.TestCase):
    def run_case(self, peer_type=target.Simulation, console_type=Console, hours=False, start_mode=4):
        with tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1] / "artifacts") as folder:
            out = Path(folder) / "run"
            args = argparse.Namespace(port=None, out=out, hours=hours, start_mode=start_mode)
            with patch.object(target, "Console", console_type), patch.object(target, "Simulation", peer_type):
                code = target.run(args)
            report = json.loads((out / "workflow.json").read_text(encoding="utf-8"))
            return code, report

    def test_four_views_release_and_close(self):
        code, report = self.run_case()
        self.assertEqual(code, 0, report)
        self.assertEqual(len(report["views"]), 4)
        self.assertFalse(report["final_status"]["active"])
        self.assertTrue(report["resources_closed"])
        self.assertFalse(report["physical_observations_confirmed"])

    def test_lost_start_ack_queries_owner_and_stops(self):
        class LostAck(target.Simulation):
            instance = None
            def __init__(self):
                super().__init__(); LostAck.instance = self
            def write(self, wire):
                count = super().write(wire)
                if b" start " in wire: self.queue.clear()
                return count
        code, report = self.run_case(LostAck)
        self.assertEqual(code, 1)
        self.assertFalse(LostAck.instance.value["active"])
        self.assertFalse(report["recovery_required"])
        self.assertTrue(report["resources_closed"])

    def test_ten_hours_and_thumbnail_modes_release(self):
        code, report = self.run_case(hours=True)
        self.assertEqual(code, 0, report)
        self.assertEqual([view["before"]["mode"] for view in report["views"]], list(range(4, 15)))
        self.assertTrue(report["resources_closed"])

    def test_resume_retains_original_modes(self):
        code, report = self.run_case(hours=True, start_mode=12)
        self.assertEqual(code, 0, report)
        self.assertEqual([view["before"]["mode"] for view in report["views"]], [12, 13, 14])
        self.assertTrue(report["resources_closed"])

    def test_foreign_status_never_stopped(self):
        class Foreign(target.Simulation):
            stopped = False
            def write(self, wire):
                if b" stop " in wire: Foreign.stopped = True
                count = super().write(wire)
                if b" start " in wire: self.value["tag"] += 1
                return count
        code, report = self.run_case(Foreign)
        self.assertEqual(code, 1)
        self.assertFalse(Foreign.stopped)
        self.assertTrue(report["recovery_required"])

    def test_cancellation_stops_matched_view(self):
        class Cancel(Console):
            def prompt(self, key, *args, **kwargs):
                if key == "hour1":
                    self.cancelled.set()
                    raise target.Cancelled("test cancel")
        code, report = self.run_case(console_type=Cancel)
        self.assertEqual(code, 1)
        self.assertEqual(report["outcome"], "cancelled")
        self.assertFalse(report["recovery_required"])
        self.assertTrue(report["resources_closed"])

    def test_close_error_keeps_report(self):
        class CloseError(target.Simulation):
            def close(self): raise OSError("test close error")
        code, report = self.run_case(CloseError)
        self.assertEqual(code, 1)
        self.assertIn("close_error", report)
        self.assertFalse(report["resources_closed"])

    def test_parser_rejects_ambiguous_or_invalid_reply(self):
        line = "[resource] tag=1 mode=0 active=1 pending=0 available=0 start=100 elapsed=12 reason=0"
        self.assertEqual(target.parse_status([line])["start"], 100)
        for lines in ([line, line], [line.replace("mode=0", "mode=15")], []):
            with self.assertRaises(target.ProtocolError): target.parse_status(lines)


if __name__ == "__main__": unittest.main()
