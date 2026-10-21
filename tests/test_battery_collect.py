"""Protocol, ownership, cleanup and local-console checks; never opens a device."""
import json
import pathlib
import re
import subprocess
import sys
import threading
import time
import unittest
import urllib.error
import urllib.request
import uuid
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import battery_collect as collect
from battery_console import Cancelled, Console
from battery_lab import Journal, load_trace, read_progress

FIXTURE = ROOT / "tests/fixtures/battery-b2-20261021.json"


class CollectorTests(unittest.TestCase):
    def setUp(self):
        self.folder = ROOT / "artifacts/battery-collector-tests" / uuid.uuid4().hex
        self.folder.mkdir(parents=True)
        self.fixture = load_trace(FIXTURE)

    def make(self, *, fault=None, chunk=73, timeout=.5):
        journal = Journal(self.folder / "run", binding={"simulated": True}, mode="simulation")
        device = collect.SimulatedDevice(self.fixture, fault=fault, chunk=chunk)
        cancel = threading.Event()
        collector = collect.Collector(journal, device, cancel, self.folder / "locks", timeout)
        return journal, device, cancel, collector

    def test_fragmented_transport_exports_exact_fixture_and_cleans_up(self):
        journal, device, _, collector = self.make(chunk=1, timeout=2)
        with collector:
            collector.arm(480)
            self.assertFalse(device.is_open)
            device.finish_offline()
            result = collector.export()
        self.assertEqual(result, {"status": self.fixture["status"], "records": self.fixture["records"]})
        self.assertTrue(collector.cleanup_confirmed)
        self.assertFalse(device.is_open)
        self.assertEqual(device.allowed, 1)
        self.assertEqual(len(list(journal.directory.glob("page-*.bin"))), 8)
        self.assertIn(b"[background]", (journal.directory / "uart.bin").read_bytes())
        self.assertEqual(device.queue, b"")

    def test_start_ack_loss_is_resolved_without_restarting(self):
        def fault(command, reply, device):
            return b"" if command.startswith("wf_battery start") else reply
        journal, device, _, collector = self.make(fault=fault, timeout=.03)
        with collector:
            collector.arm(480)
        self.assertEqual(device.commands.count("wf_battery start 480"), 1)
        self.assertEqual(device.status["state"], 2)
        self.assertTrue(collector.cleanup_confirmed)
        self.assertIn("start_ack_lost", [event["kind"] for event in journal.events])

    def test_existing_trace_is_preserved(self):
        _, device, _, collector = self.make()
        device.status["state"] = 1
        with self.assertRaisesRegex(collect.ProtocolError, "Existing running"):
            with collector:
                collector.arm(480)
        self.assertFalse(any(" start " in command or command.endswith(" stop") for command in device.commands))
        self.assertEqual(device.status["state"], 1)

    def test_nonempty_stopped_trace_is_preserved(self):
        _, device, _, collector = self.make()
        device.status["count"] = 121
        with self.assertRaisesRegex(collect.ProtocolError, "nonempty trace preserved"):
            with collector:
                collector.arm(480)
        self.assertFalse(any(" start " in command or command.endswith(" stop") for command in device.commands))

    def test_preexisting_pm_hold_is_preserved(self):
        _, device, _, collector = self.make()
        device.allowed = 0
        with self.assertRaisesRegex(collect.ProtocolError, "Pre-existing PM"):
            with collector:
                collector.arm(480)
        self.assertEqual(device.allowed, 0)
        self.assertNotIn("wf_pm allow", device.commands)

    def test_bad_page_retries_same_range_and_preserves_bad_bytes(self):
        def fault(command, reply, device):
            if command == "wf_battery dump 3 0 16" and device.commands.count(command) == 1:
                return reply.replace(b"adc=", b"adc=\xff", 1)
            return reply
        journal, device, _, collector = self.make(fault=fault)
        with collector:
            collector.arm(480)
            device.finish_offline()
            result = collector.export()
        self.assertEqual(len(result["records"]), 121)
        self.assertEqual(device.commands.count("wf_battery dump 3 0 16"), 2)
        self.assertIn(b"\xff", (journal.directory / "page-000-attempt-0.bin").read_bytes())
        self.assertIn(b"\xff", (journal.directory / "uart.bin").read_bytes())

    def test_session_change_rejects_page_without_retry(self):
        def fault(command, reply, device):
            return reply.replace(b"session=3 state=3", b"session=4 state=3", 1) if " dump " in command else reply
        journal, device, _, collector = self.make(fault=fault)
        with self.assertRaises(collect.SessionChanged):
            with collector:
                collector.arm(480)
                device.finish_offline()
                collector.export()
        self.assertEqual(device.commands.count("wf_battery dump 3 0 16"), 1)
        self.assertFalse((journal.directory / "records.json").exists())

    def test_missing_completion_has_bounded_retry(self):
        def fault(command, reply, device):
            if " dump " in command:
                return reply[:reply.index(b"[bat-trace] page=")]
            return reply
        journal, device, _, collector = self.make(fault=fault, timeout=.03)
        with self.assertRaises(TimeoutError):
            with collector:
                collector.arm(480)
                device.finish_offline()
                collector.export()
        self.assertEqual(device.commands.count("wf_battery dump 3 0 16"), 2)
        self.assertTrue((journal.directory / "page-000-attempt-1.bin").exists())
        self.assertTrue(collector.cleanup_confirmed)

    def test_cancellation_during_receive_still_releases_hold(self):
        journal, device, cancel, collector = self.make()
        def fault(command, reply, transport):
            if " dump " in command:
                cancel.set()
                return b""
            return reply
        device.fault = fault
        with self.assertRaises(Cancelled):
            with collector:
                collector.arm(480)
                device.finish_offline()
                collector.export()
        self.assertTrue(collector.cleanup_confirmed)
        self.assertEqual(device.allowed, 1)
        self.assertFalse(device.is_open)

    def test_lost_hold_ack_is_cleaned_up(self):
        def fault(command, reply, device):
            return b"" if command == "wf_pm hold" else reply
        _, device, _, collector = self.make(fault=fault, timeout=.03)
        with self.assertRaises(TimeoutError):
            with collector:
                collector.arm(480)
        self.assertTrue(collector.cleanup_confirmed)
        self.assertEqual(device.allowed, 1)
        self.assertFalse(device.is_open)

    def test_cleanup_ack_failure_is_not_confirmed(self):
        def fault(command, reply, device):
            if command == "wf_pm allow" and device.commands.count(command) > 1:
                return b""
            return reply
        _, device, _, collector = self.make(fault=fault, timeout=.03)
        with collector:
            collector.arm(480)
            device.finish_offline()
            collector.export()
        self.assertFalse(collector.cleanup_confirmed)
        self.assertFalse(device.is_open)

    def test_reboot_rejects_ram_continuity(self):
        def fault(command, reply, device):
            if command == "wf_battery status" and device.status["state"] == 3:
                return b"[product] storage=ready\r\n" + reply
            return reply
        journal, device, _, collector = self.make(fault=fault)
        with self.assertRaises(collect.SessionChanged):
            with collector:
                collector.arm(480)
                device.finish_offline()
                collector.export()
        self.assertFalse(collector.cleanup_confirmed)
        self.assertFalse((journal.directory / "records.json").exists())

    def test_cleanup_does_not_stop_same_session_with_different_window(self):
        _, device, _, collector = self.make()
        with collector:
            collector.arm(480)
            device.status["start"] += 1
        self.assertFalse(collector.cleanup_confirmed)
        self.assertNotIn("wf_battery stop", device.commands)

    def test_kernel_lease_blocks_other_process_and_releases_on_exit(self):
        lockdir = self.folder / "locks"
        code = ("import sys;sys.path.insert(0,sys.argv[1]);from battery_collect import PortLease;"
                "lease=PortLease('SIMULATED:BATTERY-B2',sys.argv[2]);lease.__enter__()")
        args = [sys.executable, "-c", code, str(ROOT / "scripts"), str(lockdir)]
        with collect.PortLease("simulated:battery-b2", lockdir):
            result = subprocess.run(args, capture_output=True, timeout=10)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn(b"already owned", result.stderr)
        result = subprocess.run(args, capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        with collect.PortLease("simulated:battery-b2", lockdir):
            pass

    def test_serial_controls_are_false_before_open_and_never_pulsed(self):
        log = []
        class Stub:
            def __init__(self, **kwargs):
                log.append(("construct", kwargs))
            def __setattr__(self, key, value):
                log.append((key, value))
                object.__setattr__(self, key, value)
            def open(self):
                log.append(("open", self.dtr, self.rts, self.port))
            def close(self):
                log.append(("close",))
        transport = collect.SerialTransport("UNOPENED-TEST", factory=Stub)
        transport.open()
        transport.close()
        self.assertEqual(log[-2], ("open", False, False, "UNOPENED-TEST"))
        self.assertNotIn(("rts", True), log)
        self.assertEqual(log[0][1]["port"], None)

    def test_complete_simulation_and_partial_window_are_distinct(self):
        result = collect.simulate(FIXTURE, self.folder / "complete", 0, 1, ["continue"]*2)
        self.assertTrue(result["passed"])
        self.assertEqual(result["completed_phases"], 5)
        self.assertEqual(result["record_count"], 121)
        self.assertFalse(result["window"]["complete"])
        self.assertEqual(result["window"]["observed_battery_span_ms"], 418910)
        self.assertFalse(result["hardware_verified"])
        self.assertTrue(read_progress(self.folder / "complete")["last_event"]["passed"])

    def test_offline_cancel_stops_owned_trace_and_leaves_later_phases_pending(self):
        result = collect.simulate(FIXTURE, self.folder / "cancel", 0, 1, ["continue", "cancel"])
        self.assertFalse(result["passed"])
        self.assertEqual(result["outcome"], "cancelled")
        self.assertEqual(result["completed_phases"], 3)  # Preparation, arm, cancellation cleanup.
        self.assertTrue(result["cleanup_confirmed"])

    def test_cancel_during_cleanup_cannot_be_reported_passed(self):
        original = collect.Collector.__exit__
        def cancel_after_cleanup(collector, *args):
            original(collector, *args)
            collector.cancel.set()
        with patch.object(collect.Collector, "__exit__", cancel_after_cleanup):
            result = collect.simulate(FIXTURE, self.folder / "late-cancel", 0, 1, ["continue"]*2)
        self.assertEqual(result["outcome"], "cancelled")
        self.assertFalse(result["passed"])
        self.assertEqual(result["completed_phases"], 5)  # Cleanup ran despite the accepted cancel.

    def test_window_excludes_reconnected_usb_and_handles_tick_wrap(self):
        rows = [{"obs": 0xfffffff0, "usb_valid": 1, "usb_good": 0, "phase": 0},
                {"obs": 20, "usb_valid": 1, "usb_good": 0, "phase": 0},
                {"obs": 500000, "usb_valid": 1, "usb_good": 1, "phase": 0}]
        window = collect.window_summary({"status": {"start": 0xffffffe0}, "records": rows})
        self.assertEqual(window["observed_battery_span_ms"], 36)
        self.assertEqual(window["unplug_delay_ms"], 16)
        self.assertTrue(window["usb_returned_in_trace"])
        self.assertFalse(window["complete"])


class ConsoleTests(unittest.TestCase):
    def test_prompt_identity_origin_cancel_and_exact_completion_count(self):
        folder = ROOT / "artifacts/battery-console-tests" / uuid.uuid4().hex
        journal = Journal(folder, binding={"simulated": True})
        with Console(journal, [("one", "第一步"), ("two", "第二步")]) as console:
            page = urllib.request.urlopen(console.url).read().decode()
            token = json.loads(re.search(r"const token=(.*?);", page)[1])
            errors = []
            def work():
                try:
                    console.prompt("one", "Confirm", 2)
                    console.stage("one", "complete")
                    console.cancel()
                    console.finish("cancelled", "Software test only")
                except BaseException as exc:
                    errors.append(exc)
            worker = threading.Thread(target=work)
            worker.start()
            deadline = time.monotonic()+1
            while not console.snapshot()["prompt"] and time.monotonic() < deadline:
                time.sleep(.002)
            prompt = console.snapshot()["prompt"]
            def post(path, body, origin=None):
                request = urllib.request.Request(console.url+path, data=json.dumps(body).encode(),
                    headers={"Origin": origin or console.url, "X-Workflow-Token": token}, method="POST")
                return urllib.request.urlopen(request)
            with self.assertRaises(urllib.error.HTTPError) as rejected:
                post("/reply", {"id": prompt["id"], "answer": "continue"}, "https://example.invalid")
            self.assertEqual(rejected.exception.code, 403)
            with self.assertRaises(urllib.error.HTTPError) as rejected:
                post("/reply", {"id": "stale", "answer": "continue"})
            self.assertEqual(rejected.exception.code, 409)
            post("/reply", {"id": prompt["id"], "answer": "continue"}).close()
            worker.join(2)
            self.assertFalse(worker.is_alive())
            self.assertFalse(errors)
            state = json.loads(urllib.request.urlopen(console.url+"/state").read())
            self.assertEqual(state["completed"], 1)
            self.assertEqual(state["phases"][1]["state"], "pending")
            self.assertEqual(state["result"]["outcome"], "cancelled")
            post("/seen", {}).close()
        self.assertFalse(console.thread.is_alive())

    def test_completion_and_cancel_have_a_single_atomic_boundary(self):
        folder = ROOT / "artifacts/battery-console-tests" / uuid.uuid4().hex
        journal = Journal(folder, binding={"simulated": True})
        with Console(journal, [("one", "第一步")]) as console:
            console.stage("one", "running")
            console.cancel()
            with self.assertRaises(Cancelled):
                console.stage("one", "complete")
            self.assertEqual(console.seal("passed"), "cancelled")
            console.finish("passed", "Must not pass")
            self.assertEqual(console.snapshot()["result"]["outcome"], "cancelled")
        other = Journal(folder.parent / uuid.uuid4().hex, binding={"simulated": True})
        with Console(other, [("one", "第一步")]) as console:
            console.stage("one", "complete")
            with self.assertRaises(ValueError):
                console.cancel()
            self.assertFalse(console.cancelled.is_set())
            self.assertEqual(console.seal("passed"), "passed")

    def test_cancel_is_sticky_and_timeout_does_not_complete_a_phase(self):
        folder = ROOT / "artifacts/battery-console-tests" / uuid.uuid4().hex
        journal = Journal(folder, binding={"simulated": True})
        with Console(journal, [("one", "第一步")]) as console:
            with self.assertRaises(TimeoutError):
                console.prompt("one", "Timeout", .01)
            self.assertEqual(console.snapshot()["completed"], 0)
            console.cancel()
            with self.assertRaises(Cancelled):
                console.prompt("one", "Cancelled", 1)
        self.assertFalse(console.thread.is_alive())


if __name__ == "__main__":
    unittest.main()
