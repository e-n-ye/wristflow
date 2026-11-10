"""No hardware: real worker/clock, fake UART faults and parent ownership."""
import io
import json
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_rom_device as device
import ota_rom_rescue as engine
from ota_rom_transport import BoundedSerial
from lab_serial import PortLease
from test_ota_rom_backup import runtime_fixture
from test_ota_rom_rescue import Journal


class IpcReadTests(unittest.TestCase):
    def setUp(self):
        self.path = Path(tempfile.mkdtemp(prefix="ota-ipc-read-", dir=ROOT / "artifacts")) / "request.json"
        self.path.write_text('{"schema":1}', encoding="utf-8")

    def test_sharing_denial_retries_same_completed_file_then_decodes(self):
        with patch.object(Path, "read_text", side_effect=[PermissionError("sharing"), '{"schema":1}']) as read:
            with patch.object(device.time, "sleep") as sleep:
                self.assertEqual(device.load(self.path), {"schema": 1})
        self.assertEqual(read.call_count, 2)
        self.assertEqual(sleep.call_args.args, (.01,))

    def test_persistent_denial_has_a_finite_deadline(self):
        with patch.object(Path, "read_text", side_effect=PermissionError("sharing")) as read:
            with patch.object(device.time, "monotonic", side_effect=[0, .1, .2, .3]):
                with patch.object(device.time, "sleep"):
                    with self.assertRaises(PermissionError):
                        device.load(self.path)
        self.assertEqual(read.call_count, 3)

    def test_malformed_json_is_never_retried(self):
        with patch.object(Path, "read_text", return_value="{broken") as read:
            with patch.object(device.time, "sleep") as sleep:
                with self.assertRaises(json.JSONDecodeError):
                    device.load(self.path)
        self.assertEqual(read.call_count, 1)
        sleep.assert_not_called()


class TransportTests(unittest.TestCase):
    def test_controls_precede_open_and_no_flush_is_used(self):
        events = []
        class Serial:
            def __init__(self, **kw):
                self.__dict__["kw"] = kw
                events.append(("construct", kw))
            def __setattr__(self, key, value):
                events.append((key, value)); self.__dict__[key] = value
            def open(self): events.append(("open", self.port))
            def read(self, count): return b"x"[:count]
            def write(self, wire): return len(wire)
            def close(self): events.append(("close",))
            def flush(self): raise AssertionError("Unbounded flush is forbidden")
        journal = Journal()
        transport = BoundedSerial("FAKE", "rom", journal, lambda: None,
                                  lambda: events.append(("identity",)), factory=Serial)
        self.assertIsNone(events[0][1]["port"])
        self.assertGreater(events[0][1]["write_timeout"], 0)
        self.assertEqual(events[1:4], [("dtr", False), ("rts", False), ("port", "FAKE")])
        transport.open(); self.assertEqual(transport.write(b"control"), 7)
        self.assertEqual(transport.read(1), b"x")
        transport.set_rts(True); transport.set_rts(False); transport.close()
        self.assertTrue(transport.closed)
        self.assertLess(events.index(("identity",)), events.index(("open", "FAKE")))
        self.assertEqual([e["kind"] for e in journal.events][-2:],
                         ["serial_close_intent", "serial_close_complete"])

    def test_open_checks_identity_and_cancel_still_allows_rts_release(self):
        class Serial:
            def __init__(self, **kw): self.opened = False
            def open(self): self.opened = True
            def close(self): pass
        def fail(): raise TimeoutError("cancelled")
        transport = BoundedSerial("FAKE", "rom", Journal(), lambda: None, fail, factory=Serial)
        with self.assertRaises(TimeoutError): transport.open()
        self.assertFalse(transport.serial.opened)
        transport.check = fail
        with self.assertRaises(TimeoutError): transport.set_rts(True)
        transport.set_rts(False); transport.close()
        before = BoundedSerial("FAKE", "before", Journal(), lambda: None, lambda: None, factory=Serial)
        with self.assertRaises(ValueError): before.set_rts(True)
        before.close()

    def test_late_empty_read_does_not_sleep_negative(self):
        class Clock:
            def __init__(self): self.value = 0
            def now(self): self.value += .03; return self.value
            def sleep(self, seconds):
                if seconds < 0: raise AssertionError("negative sleep")
                self.value += seconds
        peer = engine.Simulation("no-reply")
        link = engine.Link(peer, io.BytesIO(), Journal(), Clock(), threading.Event())
        with self.assertRaises(engine.NoReply): link.enter()

    def test_delayed_driver_read_cannot_extend_request_or_accept_late_ack(self):
        for before_send in (True, False):
            with self.subTest(before_send=before_send):
                clock = engine.ModelClock()
                class Serial(engine.Simulation):
                    def read(self, count):
                        if before_send and not self.queue: clock.sleep(2.0)
                        elif self.queue: clock.sleep(1.1)
                        return super().read(count)
                peer, raw = Serial(), io.BytesIO()
                link = engine.Link(peer, raw, Journal(), clock, threading.Event())
                with self.assertRaises((TimeoutError, engine.NoReply)): link.enter()
                self.assertEqual(len(peer.commands), 0 if before_send else 1)
                self.assertEqual(bool(raw.getvalue()), not before_send)

    def test_active_timeout_keeps_reserved_debug_exit_time(self):
        peer, clock = engine.Simulation(), engine.ModelClock()
        link = engine.Link(peer, io.BytesIO(), Journal(), clock, threading.Event())
        observations = {}
        steps = engine.experiment(link, observations)
        self.assertEqual(next(steps), "entered")
        self.assertEqual(next(steps), "caught")
        clock.value = link.deadline
        with self.assertRaises(TimeoutError): next(steps)
        result = engine.cleanup(link, allow_exit_reset=False)
        self.assertTrue(result["debug_release_confirmed"] and result["resources_closed"])
        self.assertFalse(result["errors"] or peer.halted or peer.debug)


class DeviceTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=ROOT / "artifacts", prefix="ROM device 测试 ")
        self.root = Path(self.tmp.name)
        self.threads = []

    def tearDown(self):
        for thread in self.threads: thread.join(timeout=2)
        self.tmp.cleanup()

    def exercise(self, scenario="success", *, cancel_phase=None, cancel_after_catch=False,
                 process_factory=device.ReadProcess, after_boot=2, limits=None, tamper=False):
        output = self.root / "run"
        def ready(console):
            if tamper: (output / "before-fixture.bin").write_bytes(b"changed")
            def operator():
                while not console.sealed:
                    with console.condition:
                        prompt = console.prompt_value
                        if prompt:
                            caught = output / "worker/events.jsonl"
                            if (prompt["phase"] == cancel_phase or
                                    cancel_after_catch and caught.exists() and '"step": "caught"' in caught.read_text(encoding="utf-8")):
                                console.cancel(); return
                            if not prompt.get("automatic") and console.answer is None:
                                console.respond(prompt["id"], "continue")
                    time.sleep(.002)
            thread = threading.Thread(target=operator, daemon=True)
            self.threads.append(thread); thread.start()
        return device.run(output, fixtures={"before": runtime_fixture(1), "after": runtime_fixture(after_boot)},
            scenario=scenario, ready=ready, limits=limits or dict(bind=5, before=5, rom=6, after=5, grace=1),
            total_seconds=40, prompt_seconds=2, process_factory=process_factory)

    def assert_released(self):
        with PortLease("SIMULATED-ROM-WALL", self.root / "simulation-locks"): pass

    def test_real_worker_success_has_wall_clock_and_bound_evidence(self):
        result = self.exercise()
        self.assertTrue(result["passed"], result)
        self.assertTrue(result["observation_passed"] and result["application_recovery_passed"])
        self.assertTrue(result["resources_closed"])
        self.assertEqual(result["worker_exit"], 0)
        self.assertFalse(result["worker_forced"] or result["recovery_required"])
        self.assertFalse(any(result[k] for k in ("hardware_verified", "rom_rescue_verified", "deployment_authorized",
                         "flash_write", "clock_write", "physical_observations_confirmed")))
        obs = result["rom"]["observations"]
        self.assertEqual(obs["last"]["clock_kind"], "monotonic")
        self.assertGreaterEqual(obs["last"]["clock_seconds"] - obs["first"]["clock_seconds"], 2.5)
        for sequence, phase in enumerate(("before", "rom", "after"), 1):
            reply = device.load(self.root / f"run/response-{sequence}.json")
            self.assertEqual(reply["request_sha"], device.digest(self.root / f"run/request-{sequence}.json"))
            self.assertEqual(reply["phase"], phase)
        self.assert_released()

    def test_cancel_before_launch_never_starts_worker(self):
        result = self.exercise(cancel_phase="prepare")
        self.assertEqual(result["outcome"], "cancelled")
        self.assertFalse(result["worker_attempted"] or result["entry_attempted"])
        self.assertFalse((self.root / "run/process").exists())

    def test_cancel_after_catch_exits_debug_without_after_open(self):
        result = self.exercise(cancel_after_catch=True)
        self.assertEqual(result["outcome"], "cancelled", result)
        self.assertTrue(result["entry_attempted"] and result["resources_closed"])
        self.assertTrue(result["rom"]["cleanup"]["debug_release_confirmed"])
        self.assertFalse(result["worker_forced"])
        self.assertFalse((self.root / "run/worker/after-uart.bin").exists())
        self.assert_released()

    def test_driver_hangs_are_killed_and_report_unknown_recovery(self):
        for scenario in ("hang-before", "hang-open", "hang-rom", "hang-close"):
            with self.subTest(scenario=scenario):
                self.root = Path(self.tmp.name) / scenario; self.root.mkdir()
                limits = dict(bind=5, before=.5, rom=3.5 if scenario == "hang-close" else 1, after=5, grace=.2)
                start = time.monotonic(); result = self.exercise(scenario, limits=limits)
                self.assertFalse(result["passed"], result)
                self.assertTrue(result["worker_forced"] and result["resources_closed"] and result["recovery_required"], result)
                self.assertLess(time.monotonic()-start, 12)
                if scenario in ("hang-rom", "hang-close"): self.assertIsNone(result["entry_attempted"])
                self.assertFalse((self.root / "run/worker/after-uart.bin").exists())
                self.assert_released()

    def test_identity_change_refuses_rom_open(self):
        result = self.exercise("identity-change")
        self.assertFalse(result["passed"] or result["entry_attempted"])
        events = [json.loads(line) for line in (self.root / "run/worker/events.jsonl").read_text(encoding="utf-8").splitlines()]
        self.assertFalse(any(e["kind"] == "serial_open_intent" and e.get("role") == "rom" for e in events))
        self.assert_released()

    def test_failed_after_preserves_rom_observation(self):
        result = self.exercise(after_boot=3)
        self.assertFalse(result["passed"] or result["application_recovery_passed"])
        self.assertTrue(result["observation_passed"] and result["recovery_required"])
        self.assert_released()

    def test_rejected_rom_response_keeps_device_entry_unknown(self):
        original = device.load
        def load(path):
            value = original(path)
            if Path(path).name == "response-2.json":
                value["run_id"] = "foreign"
            return value
        with patch.object(device, "load", load):
            result = self.exercise()
        self.assertFalse(result["passed"])
        self.assertIsNone(result["entry_attempted"])
        self.assertTrue(result["recovery_required"])
        self.assertTrue(result["resources_closed"])
        self.assertFalse((self.root / "run/worker/after-uart.bin").exists())
        self.assert_released()

    def test_observed_startup_line_completes_real_clock_worker(self):
        result = self.exercise("startup-banner")
        self.assertTrue(result["passed"], result)
        self.assertEqual(result["errors"], [])
        self.assertTrue((self.root / "run/worker/rom-rx.bin").read_bytes().startswith(b"SFBL\n"))
        self.assert_released()

    def test_release_banner_completes_worker_with_separate_state_readback(self):
        result = self.exercise("release-banner")
        self.assertTrue(result["passed"], result)
        cleaned = result["rom"]["cleanup"]
        self.assertTrue(cleaned["debug_release_acknowledged"] and cleaned["debug_release_confirmed"])
        self.assertFalse(cleaned["debug_release_readback"] & (engine.HALT | engine.DEBUG))
        self.assertFalse(cleaned["exit_reset_attempted"])

    def test_release_old_frame_or_asserted_debug_cannot_pass_worker(self):
        for scenario in ("release-old-frame", "release-readback-fail"):
            with self.subTest(scenario=scenario):
                self.root = Path(self.tmp.name) / scenario; self.root.mkdir()
                result = self.exercise(scenario)
                self.assertFalse(result["passed"])
                self.assertTrue(result["observation_passed"] and result["resources_closed"])
                self.assertFalse(result["rom"]["cleanup"]["debug_release_confirmed"])
                self.assertTrue(any(item["phase"] == "rom" for item in result["errors"]))
        self.assert_released()

    def test_rom_and_rtc_failures_are_preserved_independently(self):
        result = self.exercise("startup-old-frame-rtc-fail")
        self.assertFalse(result["passed"] or result["observation_passed"] or result["application_recovery_passed"])
        by_phase = {item["phase"]: item["error"] for item in result["errors"]}
        self.assertIn("Unsolicited", by_phase["rom"])
        self.assertIn("RTC continuity", by_phase["recovery"])
        self.assertEqual(len(result["errors"]), 2)
        self.assertIn(by_phase["rom"], result["verification"][0]["detail"])
        self.assertIn(by_phase["recovery"], result["verification"][1]["detail"])
        self.assertIn("ROM观测与退出", result["detail"])
        self.assertIn("应用记录与RTC恢复", result["detail"])
        self.assertTrue((self.root / "run/worker/after-uart.bin").exists())
        self.assertTrue(result["resources_closed"])
        self.assert_released()

    def test_no_catch_or_exit_error_cannot_pass(self):
        for scenario in ("no-catch", "exit-fail"):
            with self.subTest(scenario=scenario):
                self.root = Path(self.tmp.name) / scenario; self.root.mkdir()
                result = self.exercise(scenario)
                self.assertFalse(result["passed"], result)
                self.assertEqual(result["observation_passed"], scenario == "exit-fail")
                self.assertTrue(any(error["phase"] == "rom" for error in result["errors"]))
                self.assertTrue(result["resources_closed"])
                self.assert_released()

    def test_fixture_changed_before_start_fails_without_worker(self):
        result = self.exercise(tamper=True)
        self.assertFalse(result["passed"] or result["worker_attempted"])
        self.assertIn("Evidence changed", result["detail"])
        self.assert_released()

    def test_cleanup_storage_or_poll_error_still_closes_process(self):
        for fault in ("publish", "poll", "close"):
            with self.subTest(fault=fault):
                self.root = Path(self.tmp.name) / fault; self.root.mkdir()
                processes = []
                class Process:
                    def __init__(self, argv, directory): self.closed = False; processes.append(self)
                    def poll(self):
                        if fault == "poll": raise OSError("poll failed")
                        return None
                    def close(self):
                        self.closed = True
                        if fault == "close": raise OSError("exit unknown")
                original = device.publish
                def publish(path, value):
                    if fault == "publish" and Path(path).name == "cancel.json": raise OSError("disk full")
                    return original(path, value)
                with patch.object(device, "publish", publish):
                    result = self.exercise(cancel_phase="bind", process_factory=Process)
                self.assertTrue(processes[0].closed)
                self.assertFalse(result["passed"])
                self.assertEqual(result["resources_closed"], fault != "close")
                self.assertEqual(result["lease_retained"], fault == "close")
                if fault == "close":
                    with self.assertRaises(RuntimeError): self.assert_released()
                    owner, lease = device.RETAINED.pop(); lease.__exit__(None, None, None)
                self.assert_released()

    def test_capture_requires_fresh_authorization_before_output_or_io(self):
        with self.assertRaisesRegex(ValueError, "authorization"):
            device.run(self.root / "blocked", capture={"port": "COM5"})
        self.assertFalse((self.root / "blocked").exists())

    def test_rtc_uses_monotonic_elapsed_despite_host_clock_jump(self):
        before = dict(records=device.decode_capture(runtime_fixture(1), 7),
            replies={"date": ["Sun Nov 08 12:00:00 2026"]}, closed_ns=1_000_000_000)
        after = dict(records=device.decode_capture(runtime_fixture(2), 7),
            replies={"date": ["Sun Nov 08 12:00:01 2026"]}, closed_ns=3_602_000_000_000)
        self.assertTrue(device.recovery(before, after, elapsed_seconds=1)["rtc_continuity"])
        after["replies"]["date"] = ["Sun Nov 08 13:00:01 2026"]
        with self.assertRaisesRegex(ValueError, "RTC"): device.recovery(before, after, elapsed_seconds=1)


if __name__ == "__main__": unittest.main()
