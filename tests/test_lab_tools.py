"""Host-only UART, workflow and historical-replay regression checks."""
import io
import json
import pathlib
import sys
import threading
import time
import unittest
import uuid
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from battery_console import Console
from battery_lab import Journal
from lab_serial import ProtocolError, RecordedClient, RecordedSession, SessionChanged
import mag_protocol
import mmc5603_lab as mag


class LabTests(unittest.TestCase):
    def setUp(self):
        self.directory = ROOT / "artifacts/lab-tool-tests" / uuid.uuid4().hex
        self.directory.mkdir(parents=True)

    def workflow(self, action="pass", fault=None):
        peer = mag.Simulation(fault)
        workers = []

        def ready(console):
            def operator():
                answered = set()
                deadline = time.monotonic()+15
                while time.monotonic() < deadline and not console.result:
                    value = console.snapshot()["prompt"]
                    if value and not value.get("automatic") and value["id"] not in answered:
                        if value["phase"] == "right" and action in ("cancel", "foreign"):
                            if action == "foreign":
                                peer.start += 1
                            console.cancel()
                            return
                        if value["phase"] == "right" and action == "timeout":
                            return
                        try:
                            console.respond(value["id"], "continue")
                            answered.add(value["id"])
                        except ValueError:
                            pass
                    time.sleep(.002)
            worker = threading.Thread(target=operator)
            worker.start()
            workers.append(worker)

        try:
            report = mag.run(self.directory / "run", peer, identity_check=lambda: {"simulation": True},
                             binding={"simulation": True}, locks=self.directory / "locks", timeout=.4,
                             sample_timeout=.5, settle_seconds=.005, ready=ready)
        finally:
            for worker in workers:
                worker.join(2)
                self.assertFalse(worker.is_alive())
        return peer, report

    def test_simulated_five_windows_confirm_and_stop_once(self):
        peer, report = self.workflow()
        self.assertTrue(report["passed"], report)
        self.assertEqual(len(report["windows"]), 5)
        self.assertTrue(report["reset_notice_excluded"])
        self.assertTrue(report["operator_end_confirmed"])
        self.assertTrue(report["resources_closed"])
        self.assertEqual(peer.commands.count("wf_mag sample encoded 120"), 1)
        self.assertEqual(peer.commands.count("wf_mag stop"), 1)

    def test_cancel_stops_owned_window_and_closes_resources(self):
        peer, report = self.workflow("cancel")
        self.assertEqual(report["outcome"], "cancelled", report)
        self.assertTrue(report["stop_confirmed"])
        self.assertFalse(report["passed"])
        self.assertFalse(report["operator_end_confirmed"])
        self.assertTrue(peer.closed)
        self.assertEqual(peer.commands.count("wf_mag stop"), 1)

    def test_timeout_stops_owned_window_and_retains_incomplete_result(self):
        peer, report = self.workflow("timeout")
        self.assertEqual(report["outcome"], "timeout")
        self.assertTrue(report["stop_confirmed"])
        self.assertTrue(report["resources_closed"])
        self.assertEqual(len(report["windows"]), 1)

    def test_same_session_different_start_is_not_stopped(self):
        peer, report = self.workflow("foreign")
        self.assertFalse(report["stop_confirmed"])
        self.assertTrue(report["recovery_required"])
        self.assertEqual(peer.commands.count("wf_mag stop"), 0)
        self.assertTrue(report["resources_closed"])

    def test_missing_start_ack_observes_without_restarting(self):
        peer, report = self.workflow(fault="lost_start_ack")
        self.assertTrue(report["passed"], report)
        self.assertEqual(peer.commands.count("wf_mag sample encoded 120"), 1)

    def test_partial_write_fails_and_closes(self):
        peer, report = self.workflow(fault="short_write")
        self.assertEqual(report["outcome"], "error")
        self.assertFalse(report["start_attempted"])
        self.assertTrue(report["resources_closed"])
        self.assertNotIn("wf_mag stop", peer.commands)

    def test_no_samples_timeout_does_not_pass(self):
        _, report = self.workflow(fault="no_samples")
        self.assertEqual(report["outcome"], "timeout")
        self.assertTrue(report["stop_confirmed"])

    def test_lost_stop_ack_is_not_retried_and_status_confirms_cleanup(self):
        peer, report = self.workflow(fault="lost_stop_ack")
        self.assertEqual(report["outcome"], "timeout")
        self.assertTrue(report["stop_confirmed"])
        self.assertEqual(peer.commands.count("wf_mag stop"), 1)

    def test_cold_boot_revokes_cleanup_authority(self):
        peer, report = self.workflow(fault="cold_boot")
        self.assertEqual(report["outcome"], "error")
        self.assertFalse(report["stop_confirmed"])
        self.assertEqual(peer.commands.count("wf_mag stop"), 0)
        self.assertTrue(report["recovery_required"])
        self.assertTrue(report["resources_closed"])

    def test_cold_boot_during_start_does_not_send_further_commands(self):
        peer, report = self.workflow(fault="cold_start")
        self.assertEqual(report["outcome"], "error")
        self.assertEqual(peer.commands[-1], "wf_mag sample encoded 120")
        self.assertTrue(report["resources_closed"])
        self.assertTrue(report["recovery_required"])

    def test_close_failure_is_separate_from_device_stop(self):
        _, report = self.workflow(fault="close")
        self.assertEqual(report["outcome"], "error")
        self.assertTrue(report["stop_confirmed"])
        self.assertFalse(report["resources_closed"])

    def test_historical_raw_replay_preserves_original_failure(self):
        report = mag.replay(ROOT / "tests/fixtures/mag-readiness-20261024.json", self.directory / "replay")
        self.assertEqual(report["ordinary_frames"], 86)
        self.assertEqual(report["reset_notices"], 1)
        self.assertEqual(report["original_workflow"]["outcome"], "error")
        self.assertFalse(report["hardware_verified"])

    def test_parser_rejects_mixed_status_and_arbitrary_invalid_frame(self):
        text = mag.Simulation().state()
        with self.assertRaises(ProtocolError):
            mag_protocol.cached(text.replace("[mag-mode] session=2", "[mag-mode] session=3"))
        peer = mag.Simulation()
        peer.seq = 1
        bad = "[mag-data] session=2 seq=1 ms=200 status=00 xyz=0/0/0 valid=0 saturated=0; raw counts\n"
        with self.assertRaises(ProtocolError):
            mag_protocol.ordinary(bad, mag_protocol.cached(peer.state()))

    def test_unqualified_base_client_cannot_send_any_command(self):
        peer = mag.Simulation()
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        client = RecordedClient(peer, io.BytesIO(), journal, threading.Event())
        with self.assertRaises(ValueError):
            client.request("wf_pm status", lambda line: True, cleanup=True)
        self.assertEqual(peer.commands, [])

    def test_split_cold_boot_is_preserved_before_rejection(self):
        class Peer:
            def __init__(self):
                self.chunks = iter((b"[prod", b"uct] storage=ok\n"))
            def read(self, count):
                return next(self.chunks)
        raw = io.BytesIO()
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        client = RecordedClient(Peer(), raw, journal, threading.Event())
        client.read_chunk()
        with self.assertRaises(SessionChanged):
            client.read_chunk()
        self.assertEqual(raw.getvalue(), b"[product] storage=ok\n")
        self.assertTrue(client.booted)

    def test_stream_half_line_survives_command_response_boundary(self):
        frame = b"[mag-data] session=2 seq=5 ms=200 status=51 xyz=1/2/3 valid=1 saturated=0; raw counts\r\n"
        class Peer:
            def __init__(self):
                self.queue = bytearray(frame[:28])
            def read(self, count):
                value = bytes(self.queue[:count])
                del self.queue[:len(value)]
                return value
            def write(self, value):
                self.queue.extend(frame[28:]+mag.Simulation().state().encode())
                return len(value)
        peer = Peer()
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        client = mag.MagClient(peer, io.BytesIO(), journal, threading.Event())
        state = client.status()
        self.assertEqual(state["session"], 2)
        self.assertEqual([row["seq"] for row in mag_protocol.frames(client.text, 2)], [5])

    def test_automatic_stage_rejects_confirmation_click(self):
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        with Console(journal, [("sample", "Sample")]) as console:
            def pump():
                with self.assertRaises(ValueError):
                    console.respond(console.snapshot()["prompt"]["id"], "continue")
                console.cancel()
            from battery_console import Cancelled
            with self.assertRaises(Cancelled):
                console.automatic("sample", "Observe", .1, pump=pump)
            self.assertEqual(console.snapshot()["completed"], 0)

    def test_last_frame_received_after_deadline_cannot_complete_stage(self):
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        received = []
        with Console(journal, [("sample", "Sample")]) as console:
            def pump():
                time.sleep(.025)
                received.append(True)
            with self.assertRaises(TimeoutError):
                console.automatic("sample", "Observe", .01, pump=pump, until=lambda: bool(received))
            self.assertEqual(console.snapshot()["completed"], 0)

    def test_cancel_during_identity_check_does_not_open(self):
        journal = Journal(self.directory / "journal", binding={"simulation": True})
        cancelled = threading.Event()
        opened = []
        peer = mag.Simulation()
        peer.open = lambda: opened.append(True)
        def identity_check():
            cancelled.set()
            return {"simulation": True}
        from battery_console import Cancelled
        with self.assertRaises(Cancelled):
            with RecordedSession(journal, peer, cancelled, mag.MagClient,
                                 identity=identity_check, locks=self.directory / "locks"):
                self.fail("Must not enter a cancelled session")
        self.assertEqual(opened, [])

    def test_finished_event_write_failure_downgrades_saved_pass(self):
        original = Journal.event
        def event(journal, kind, **values):
            if kind == "finished":
                raise OSError("Injected finished persistence failure")
            return original(journal, kind, **values)
        with patch.object(Journal, "event", event), self.assertRaises(OSError):
            self.workflow()
        report = json.loads((self.directory / "run/workflow.json").read_text(encoding="utf-8"))
        self.assertFalse(report["passed"])
        self.assertEqual(report["outcome"], "error")
        self.assertTrue(report["resources_closed"])


if __name__ == "__main__":
    unittest.main()
