import copy
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from battery_console import Console, Cancelled
from cycle_short_lab import Simulation, analyze, run


class AutoConsole(Console):
    cancel_at = None
    def prompt(self, key, message, timeout):
        self.stage(key, "waiting")
        if key == self.cancel_at:
            self.cancel()
            raise Cancelled("Test cancellation")
        self.journal.event("test_operator_confirmation", phase=key)
    def automatic(self, key, message, seconds):
        self.stage(key, "complete")


class ShortTests(unittest.TestCase):
    def args(self, out):
        return SimpleNamespace(out=out, port=None, seconds=1, session=123)

    def test_workflow_and_offline_cancellation_no_reopen(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            peer = Simulation()
            r = run(self.args(root / "good"), peer, AutoConsole)
            self.assertTrue(r["passed"] and r["owned"] and r["stopped"], r)
            self.assertEqual(peer.calls, ["arm", "status", "status", "export"])
            class Cancel(AutoConsole):
                cancel_at = "reconnect"
            peer = Simulation()
            r = run(self.args(root / "cancel"), peer, Cancel)
            self.assertFalse(r["passed"])
            self.assertTrue(r["recovery_required"])
            self.assertEqual(peer.calls, ["arm", "status"])

    def test_foreign_status_and_lost_arm_never_stop_or_retry(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            class Foreign(Simulation):
                def action(self, action, *args):
                    r = super().action(action, *args)
                    if action == "status": r["status"]["session"] = 456
                    return r
            peer = Foreign()
            r = run(self.args(root / "foreign"), peer, AutoConsole)
            self.assertTrue(r["recovery_required"])
            self.assertFalse(r["owned"])
            self.assertEqual(peer.calls, ["arm", "status"])
            class Lost(Simulation):
                def action(self, action, *args):
                    super().action(action, *args)
                    return dict(passed=False, resources_closed=True, error="Lost ACK")
            peer = Lost()
            r = run(self.args(root / "lost"), peer, AutoConsole)
            self.assertTrue(r["possible_arm"] and r["recovery_required"])
            self.assertEqual(peer.calls, ["arm"])
    def test_full_handoff_closes_host_without_claiming_discharge_done(self):
        with tempfile.TemporaryDirectory() as temp:
            peer=Simulation();args=self.args(Path(temp)/"full");args.full=True
            r=run(args,peer,AutoConsole)
            self.assertTrue(r["passed"] and r["full_handoff"] and r["device_may_be_active"])
            self.assertFalse(r["stopped"] or r["device_running_verified"] or r["recovery_required"])
            self.assertIsNone(r["analysis"])
            self.assertEqual(peer.calls,["arm","status"])
            self.assertIsNone(json.loads((args.out/"binding.json").read_text(encoding="utf-8"))["seconds"])
            class Cancel(AutoConsole):
                cancel_at="handoff"
            peer=Simulation();args.out=Path(temp)/"cancel-full"
            r=run(args,peer,Cancel)
            self.assertFalse(r["passed"] or r["full_handoff"])
            self.assertTrue(r["recovery_required"])
            self.assertEqual(peer.calls,["arm","status"])

    def test_failed_terminal_still_exports_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            class Failed(Simulation):
                def action(self, action, *args):
                    r = super().action(action, *args)
                    if action == "status" and self.calls.count("status") == 2:
                        r["status"].update(state=6, reason=7)
                    return r
            peer = Failed()
            r = run(self.args(Path(temp) / "failed"), peer, AutoConsole)
            self.assertFalse(r["passed"])
            self.assertTrue(r["stopped"])
            self.assertEqual(peer.calls[-1], "export")

    def test_analysis_requires_load_duration_adc_and_end(self):
        base = dict(kind=2, flags=50, usb_flags=1, surface=5, requested=100, acknowledged=100,
                    context_flags=7, phase=0, sample_age_ms=0, raw_mv=4100,
                    adc_sequence=1, adc_sampled_ms=5000, elapsed_ms=5000)
        rows = [dict(base, kind=1), base, dict(base, adc_sequence=2, adc_sampled_ms=125000, elapsed_ms=125000),
                dict(base, kind=3, stop_reason=4)]
        self.assertTrue(analyze(rows, 120)["passed"])
        for change in ({"usb_flags": 3}, {"flags": 34}, {"context_flags": 23},
                       {"acknowledged": 50}, {"elapsed_ms": 6000},
                       {"adc_sequence": 1, "adc_sampled_ms": 5000}):
            bad = copy.deepcopy(rows)
            bad[2].update(change)
            self.assertFalse(analyze(bad, 120)["passed"], change)
        rows[-1]["stop_reason"] = 5
        self.assertFalse(analyze(rows, 120)["passed"])


if __name__ == "__main__":
    unittest.main()
