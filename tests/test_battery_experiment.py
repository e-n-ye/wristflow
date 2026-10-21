"""Real-entry safety checks with injected transports only."""
import copy
import json
import os
import pathlib
import sys
import threading
import unittest
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import battery_experiment as experiment
from battery_collect import PortLease, ProtocolError, SessionChanged, SimulatedDevice
from battery_console import Cancelled, Console
from battery_lab import Journal, load_trace, replay


class GuidedTests(unittest.TestCase):
    def setUp(self):
        self.folder = ROOT / "artifacts/battery-experiment-tests" / uuid.uuid4().hex
        self.fixture = load_trace(ROOT / "tests/fixtures/battery-b2-20261021.json")
        self.device = SimulatedDevice(self.fixture, chunk=8192)
        self.device.finish_offline()
        original = self.device.write
        def write(wire):
            if wire.startswith(b"wf_battery start "):
                self.device.status["start"] = self.device.status["now"]
            return original(wire)
        self.device.write = write
        self.cancel = threading.Event()
        self.journal = Journal(self.folder / "run", binding={"test_only": True})
        self.collector = experiment.Experiment(self.journal, self.device, self.cancel, self.folder / "locks", self.fixture, timeout=.05)

    def test_back_up_then_one_start_and_no_pm_or_charger_controls(self):
        with self.collector:
            self.collector.protect()
            self.collector.arm()
            self.assertFalse(self.device.is_open)
        self.assertEqual(self.device.commands.count("wf_battery start 900"), 1)
        self.assertTrue(self.collector.cleanup_confirmed)
        self.assertFalse(self.collector.recorder_stopped)
        self.assertNotIn("wf_battery stop", self.device.commands)
        self.assertFalse(any(c in ("wf_pm hold", "wf_pm allow") or "charger" in c for c in self.device.commands))
        with PortLease(self.device.port, self.folder / "locks"): pass

    def test_without_backup_or_changed_backup_cannot_arm(self):
        with self.collector:
            with self.assertRaises(ProtocolError): self.collector.arm()
            self.collector.protect()
            (self.journal.directory / "protected.json").write_text("{}", encoding="utf-8")
            with self.assertRaises(ProtocolError): self.collector.arm()
        self.assertFalse(any(c.startswith("wf_battery start") for c in self.device.commands))

    def test_prestate_changed_after_protection_cannot_be_overwritten(self):
        with self.collector:
            self.collector.protect()
            self.device.status["session"] += 1
            with self.assertRaises(SessionChanged): self.collector.arm()
        self.assertNotIn("wf_battery start 900", self.device.commands)

    def test_lost_start_ack_is_observed_not_retried(self):
        with self.collector:
            self.collector.protect()
            self.device.fault = lambda cmd, data, _: b"" if cmd.startswith("wf_battery start") else data
            self.collector.arm()
        self.assertEqual(self.device.commands.count("wf_battery start 900"), 1)
        self.assertTrue(any(e["kind"] == "start_ack_lost" for e in self.journal.events))

    def test_cancellation_offline_never_reopens_and_retains_recovery_identity(self):
        with self.collector:
            self.collector.protect(); self.collector.arm(); self.cancel.set()
        kinds = [e["kind"] for e in self.journal.events]
        self.assertEqual(kinds.count("transport_open"), 1)
        self.assertIn("recorder_recovery_required", kinds)
        self.assertTrue((self.journal.directory / "ownership.json").exists())
        self.assertFalse(self.collector.recorder_stopped)

    def test_reconnect_different_window_does_not_stop_it(self):
        with self.collector:
            self.collector.protect(); self.collector.arm()
            self.device.status["start"] += 10
            with self.assertRaises(SessionChanged): self.collector.collect()
        self.assertNotIn("wf_battery stop", self.device.commands)
        self.assertFalse(self.collector.recorder_stopped)

    def test_reconnect_stops_and_exports_only_owned_new_session(self):
        with self.collector:
            self.collector.protect(); self.collector.arm()
            new = copy.deepcopy(self.fixture)
            new["status"].update(self.device.status)
            for row in new["records"]:
                row["session"] = self.device.status["session"]
                row["obs"] = self.device.status["start"] + row["index"] * 1000
            self.device.fixture = new
            self.device.status.update(count=121)
            result = self.collector.collect()
            self.assertEqual(len(result["records"]), 121)
        self.assertTrue(self.collector.recorder_stopped)
        self.assertTrue(self.collector.cleanup_confirmed)
        self.assertEqual(self.device.commands.count("wf_battery stop"), 1)

    def test_analysis_excludes_usb_and_reports_voltage_not_charge_consumption(self):
        result = experiment.analyze(self.fixture)
        self.assertFalse(result["soc_or_consumed_charge_measured"])
        self.assertEqual(result["battery_observation_span_ms"], 418910)
        self.assertTrue(result["load_window_observed"])
        self.assertEqual(result["flashlight"]["brightness_ack"], [100])
        self.assertFalse(result["observed_window_checks_met"])  # Old B2 is not a full new target window.

    def test_usb_only_or_full_trace_never_meets_window_acceptance(self):
        value = copy.deepcopy(self.fixture)
        for row in value["records"]: row["usb_good"] = 1
        self.assertFalse(experiment.analyze(value)["observed_window_checks_met"])
        value = copy.deepcopy(self.fixture)
        value["status"]["state"] = 4
        self.assertFalse(experiment.analyze(value)["observed_window_checks"]["recorder_not_full"])

    def test_new_bat_trace_replays_production_stall_without_sleep_or_usb(self):
        fixture = ROOT / "tests/fixtures/battery-guided-20261021.json"
        recorded = load_trace(fixture)
        observed = experiment.analyze(recorded)
        self.assertTrue(observed["observed_window_checks_met"])
        self.assertEqual(observed["battery_unique_adc"], 95)
        report = replay(fixture, self.folder / "new-replay", os.environ.get("CC", "gcc"),
                        guarded=dict(retain_ms=15000, settle_ms=60000, settle_samples=3, max_sample_gap_ms=15000))
        self.assertEqual(report["checked_rows"], 109)
        groups = json.loads((self.folder / "new-replay/predictions.json").read_text(encoding="utf-8"))
        post = {r["seq"]: r for r in groups["baseline"] if r["percent_valid"] and
                r["usb_valid"] and not r["usb_good"] and r["mode"] == 0 and
                30000 <= (r["adc"]-observed["exit_observed_ms"]) & 0xffffffff < 900000}
        self.assertEqual(len(post), 50)
        self.assertTrue(all(r["candidate"] == 98 and r["percent"] == 97 for r in post.values()))
        for rows in groups.values():
            self.assertEqual([r["percent_valid"] for r in rows], [r["percent_valid"] for r in groups["baseline"]])
        self.assertEqual(groups["settling"][65]["percent"], 98)
        self.assertTrue(groups["settling"][65]["settled"])

    def test_countdown_is_automatic_and_cancel_retains_incomplete_phase(self):
        with Console(self.journal, [("wait", "wait")]) as console:
            experiment.countdown(console, "wait", "test", .001)
            self.assertEqual(console.snapshot()["completed"], 1)
        journal = Journal(self.folder / "cancel", binding={"test_only": True})
        with Console(journal, [("wait", "wait")]) as console:
            console.cancel()
            with self.assertRaises(Cancelled): experiment.countdown(console, "wait", "test", 1)
            self.assertEqual(console.snapshot()["completed"], 0)


if __name__ == "__main__": unittest.main()
