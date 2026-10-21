"""Protected real-entry tests use injected transports; never open hardware."""
import copy
import pathlib
import sys
import threading
import unittest
import uuid
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import battery_collect as collect
import battery_device as device
from battery_lab import Journal, digest, load_trace, read_progress, write_json
from battery_console import Cancelled


class ProtectedTests(unittest.TestCase):
    def setUp(self):
        self.folder = ROOT / "artifacts/battery-device-tests" / uuid.uuid4().hex
        self.expected = load_trace(ROOT / "tests/fixtures/battery-b2-20261021.json")
        self.transport = collect.SimulatedDevice(self.expected, chunk=1)
        self.transport.finish_offline()
        self.cancel = threading.Event()

    def make(self, **kwargs):
        journal = Journal(self.folder / "run", binding={"simulated": True})
        return device.ProtectedExport(journal, self.transport, self.cancel, self.folder / "locks", self.expected, **kwargs)

    def assert_read_only(self):
        self.assertFalse(any(command in ("wf_pm hold", "wf_pm allow", "wf_battery stop") or
                             command.startswith("wf_battery start") for command in self.transport.commands))

    def test_exports_reopens_and_preserves_preexisting_hold_and_rows(self):
        self.transport.allowed = 0
        collector = self.make()
        with collector:
            first = collector.export("protected")
            collector.close()
            collector.open()
            second = collector.export("reopened")
        self.assertEqual(first, second)
        self.assertEqual(first["records"], self.expected["records"])
        self.assertTrue(collector.cleanup_confirmed)
        self.assertEqual(self.transport.allowed, 0)
        self.assertEqual(self.transport.status, self.expected["status"])
        self.assert_read_only()

    def test_running_or_different_trace_is_preserved_and_rejected(self):
        self.transport.status["state"] = 1
        collector = self.make()
        with self.assertRaises(collect.SessionChanged):
            with collector:
                collector.export("protected")
        self.assertEqual(self.transport.status["state"], 1)
        self.assertTrue(collector.cleanup_confirmed)
        self.assert_read_only()

    def test_read_only_client_rejects_every_control_path(self):
        with self.make() as collector:
            for command in ("wf_pm hold", "wf_pm allow", "wf_battery start 1", "wf_battery stop", "reboot"):
                with self.assertRaises(ValueError):
                    collector.client.request(command, lambda _: True, cleanup=True)
        self.assertEqual(self.transport.commands, [])

    def test_cancel_during_page_saves_bytes_and_releases_resources(self):
        def fault(command, reply, transport):
            if " dump " in command:
                self.cancel.set()
            return reply
        self.transport.fault = fault
        collector = self.make()
        with self.assertRaises(Cancelled):
            with collector:
                collector.export("protected")
        self.assertTrue(collector.cleanup_confirmed)
        self.assertFalse(self.transport.is_open)
        with collect.PortLease(self.transport.port, self.folder / "locks"):
            pass
        self.assertTrue((self.folder / "run/uart.bin").read_bytes())
        self.assert_read_only()

    def test_close_failure_cannot_claim_cleanup(self):
        collector = self.make()
        with collector:
            self.transport.close = lambda: (_ for _ in ()).throw(OSError("close failure"))
        self.assertFalse(collector.cleanup_confirmed)

    def test_partial_open_failure_still_closes_and_releases_lease(self):
        collector = self.make()
        def fail_open():
            self.transport.is_open = True
            raise OSError("partial open")
        self.transport.open = fail_open
        with self.assertRaises(OSError):
            with collector:
                pass
        self.assertFalse(self.transport.is_open)
        with collect.PortLease(self.transport.port, self.folder / "locks"):
            pass

    def test_endpoint_guard_runs_before_each_open(self):
        calls = []
        def guard():
            calls.append(1)
            if len(calls) == 2:
                raise collect.SessionChanged("Changed port")
        collector = self.make(identity_guard=guard)
        with self.assertRaises(collect.SessionChanged):
            with collector:
                collector.close()
                collector.open()
        self.assertEqual(len(calls), 2)
        self.assertFalse(self.transport.is_open)

    def test_cold_boot_invalidates_continuity_and_stops_commands(self):
        self.transport.queue.extend(b"[product] storage=ready\r\n")
        collector = self.make()
        with self.assertRaises(collect.SessionChanged):
            with collector:
                collector.export("protected")
        self.assertFalse(collector.cleanup_confirmed)
        self.assertEqual(self.transport.commands, [])

    def test_cancel_before_or_during_identity_check_never_opens(self):
        for during_guard in (False, True):
            with self.subTest(during_guard=during_guard):
                self.folder = self.folder.parent / uuid.uuid4().hex
                self.cancel.clear()
                if not during_guard:
                    self.cancel.set()
                collector = self.make(identity_guard=self.cancel.set if during_guard else None)
                with patch.object(self.transport, "open") as opened:
                    with self.assertRaises(Cancelled):
                        with collector:
                            pass
                    opened.assert_not_called()

    def test_foreign_page_and_changed_rows_cannot_match_evidence(self):
        changed = copy.deepcopy(self.expected)
        changed["records"][0]["mv"] += 1
        self.transport.fixture = changed
        collector = self.make()
        with self.assertRaisesRegex(collect.SessionChanged, "records differ"):
            with collector:
                collector.export("protected")
        self.assertTrue((self.folder / "run/protected.json").exists())
        self.assert_read_only()

    def test_cancel_flow_saves_exports_and_reports_cancelled_not_pass(self):
        result = device.run_export(self.expected, {"simulated": True}, self.folder / "flow", port=0,
                                   transport=self.transport, scripted=["continue", "cancel"])
        self.assertFalse(result["passed"])
        self.assertEqual(result["outcome"], "cancelled")
        self.assertEqual(result["completed_phases"], 4)
        self.assertTrue(result["protected_export_matched"])
        self.assertTrue(result["reopened_export_matched"])
        self.assertTrue(result["cancel_cleanup_observed"])
        self.assertTrue(result["bounded_checks_met"])
        self.assertTrue(result["cleanup_confirmed"])
        self.assertEqual(result["record_count"], 121)
        self.assertEqual(read_progress(self.folder / "flow")["state"], "finished")
        self.assertTrue(all(check["passed"] for check in device.verification_checks(result)))
        self.assert_read_only()

    def test_timeout_before_open_has_no_hardware_commands(self):
        result = device.run_export(self.expected, {"simulated": True}, self.folder / "flow", port=0,
                                   transport=self.transport, scripted=["timeout"])
        self.assertEqual(result["outcome"], "timeout")
        self.assertFalse(result["passed"])
        self.assertFalse(result["cleanup_confirmed"])
        self.assertEqual(self.transport.commands, [])
        self.assertFalse(any(check["passed"] for check in device.verification_checks(result)))

    def test_skipping_required_cancel_cannot_complete_acceptance(self):
        result = device.run_export(self.expected, {"simulated": True}, self.folder / "flow", port=0,
                                   transport=self.transport, scripted=["continue", "continue"])
        self.assertFalse(result["passed"])
        self.assertFalse(result["bounded_checks_met"])
        self.assertEqual(result["outcome"], "error")
        self.assertFalse(result["cancel_cleanup_observed"])
        checks = device.verification_checks(result)
        self.assertEqual(sum(check["passed"] for check in checks), 4)
        self.assertFalse(checks[3]["passed"])
        self.assertEqual(result["completed_phases"], 4)
        self.assertFalse(result["new_battery_experiment_complete"])
        self.assert_read_only()

    def test_verification_view_keeps_run_cancelled_and_partial_stage_count(self):
        from battery_console import Console
        journal = Journal(self.folder / "view", binding={"simulated": True})
        with Console(journal, [("cancel", "取消入口"), ("cleanup", "清理")]) as console:
            console.cancel()
            console.stage("cancel", "cancelled")
            console.stage("cleanup", "cleaned")
            console.finish("cancelled", "Expected cancellation", verification=[{"title": "取消验证", "passed": True}])
            snapshot = console.snapshot()
            self.assertEqual(snapshot["completed"], 1)
            self.assertEqual(snapshot["phases"][0]["state"], "cancelled")
            self.assertEqual(snapshot["result"]["outcome"], "cancelled")
            self.assertTrue(snapshot["result"]["verification"][0]["passed"])

    def test_serial_constructor_failure_never_allocates_console(self):
        with patch.object(device, "SerialTransport", side_effect=OSError("constructor")), \
                patch.object(device, "Console") as console:
            with self.assertRaises(OSError):
                device.run_export(self.expected, {"endpoint": {"device": "COM5"}}, self.folder / "flow")
            console.assert_not_called()

    def test_binding_rejects_incomplete_duplicate_or_unlisted_firmware(self):
        self.folder.mkdir(parents=True)
        result_path = self.folder / "result.json"
        firmware = self.folder / "main.bin"
        firmware.write_bytes(b"test firmware only")
        other = self.folder / "other.bin"
        other.write_bytes(b"test artifact")
        expected = copy.deepcopy(self.expected)
        expected["source"]["bin_sha256"] = digest(firmware)
        evidence = self.folder / "evidence.json"
        expected["source"]["evidence"] = str(evidence)
        port = {"device": "COM5", "vid": 6790, "pid": 29987, "location": "test-location"}
        write_json(evidence, {"preflight": {"build": str(result_path), "artifacts": 2, "ports": [port]}})
        result = {"exit_code": 0, "artifact_validation_passed": True, "firmware_path": str(firmware),
                  "artifacts": [{"path": str(p), "sha256": digest(p)} for p in (firmware, other)]}
        with patch.object(device, "load_trace", return_value=expected), \
                patch.object(device, "port_identity", return_value=port):
            write_json(result_path, result)
            binding = device.prepare_binding(result_path, result_path, "COM5", "test-location")
            self.assertEqual(binding["firmware_sha256"], digest(firmware))
            for artifacts in ([], result["artifacts"][:1], [result["artifacts"][1]] * 2):
                write_json(result_path, dict(result, artifacts=artifacts))
                with self.assertRaisesRegex(ValueError, "manifest"):
                    device.prepare_binding(result_path, result_path, "COM5", "test-location")
            write_json(result_path, result)
            other.write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "Changed build artifact"):
                device.prepare_binding(result_path, result_path, "COM5", "test-location")


if __name__ == "__main__":
    unittest.main()
