"""No device: UART fixtures, owned-reader failures and actual host process cleanup."""
import json
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import threading
import time
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_rom_backup as lab
from lab_serial import PortLease
from ota_rom_process import ReadProcess, ReaderCleanupError
from test_runtime_record_decode import summary, seal, decoder


def runtime_fixture(boot=1, failed=False):
    current = bytearray(summary())
    struct.pack_into("<I", current, 12, boot)
    current = seal(current)
    saved = summary()
    token = 7
    lines = [f"[record-snapshot] schema=1 token=7 samples=0 saved=1 failed={int(failed)} bytes=512 "
             f"crc={decoder.snapshot_crc(token, 0, 1, int(failed)):08x}"]
    for kind, data in (("current", current), ("saved", saved)):
        lines.append(f"[record-summary] token=7 kind={kind}")
        lines.extend(f"[record-data] token=7 offset={offset} hex={data[offset:offset+32].hex()}"
                     for offset in range(0, 512, 32))
        lines.append("[record-summary-end] token=7")
    return ("\r\n".join(lines) + "\r\n").encode()


class BackupTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=ROOT / "artifacts")
        self.root = Path(self.tmp.name)
        self.retained_start = len(lab.RETAINED)
    def tearDown(self):
        # These are synthetic owners from this test, never a device resource.
        while len(lab.RETAINED) > self.retained_start:
            _, lease = lab.RETAINED.pop()
            lease.__exit__(None, None, None)
        self.tmp.cleanup()

    def exercise(self, *, scenario=None, reader=None, identity=None, factory=None, after_boot=2):
        peers, launches, ownership = [], [], []
        locks = self.root / "locks"
        def check_lock():
            try:
                with PortLease("SIMULATED-OTA-ROM", locks):
                    ownership.append(False)
            except RuntimeError:
                ownership.append(True)
        def transport(phase):
            check_lock()
            peer = lab.Simulation(runtime_fixture(1 if phase == "before" else after_boot,
                                                   failed=scenario == "bad-baseline"))
            if scenario == "close-failure":
                peer.close = lambda: (_ for _ in ()).throw(OSError("close failed"))
            if scenario == "unsolicited-boot" and phase == "before":
                peer.queue.extend(b"RT-Thread boot\r\n")
            peers.append(peer)
            return factory(phase, peer) if factory else peer
        def launch(argv, output):
            check_lock()
            self.assertTrue(peers[0].closed)
            launches.append(argv)
            return (reader or lab.SimulatedReader)(argv, output)
        def ready(console):
            def operator():
                while not console.sealed:
                    with console.condition:
                        prompt = console.prompt_value
                        if prompt and scenario == "cancel-before" and prompt["phase"] == "prepare":
                            console.cancel()
                        elif prompt and scenario == "cancel-read" and prompt.get("automatic"):
                            console.cancel()
                        elif prompt and not prompt.get("automatic") and console.answer is None:
                            console.respond(prompt["id"], "continue")
                    time.sleep(.003)
            threading.Thread(target=operator, daemon=True).start()
        result = lab.run(self.root / "run", binding={"simulation": True, "cycle_log": True},
            identity=identity or (lambda: {}), transport_factory=transport, process_factory=launch,
            locks=locks, seconds=.08 if scenario == "timeout" else 2, timeout=1, ready=ready)
        return result, peers, launches, ownership

    def test_success_single_reader_continuous_lease_and_closed_uarts(self):
        result, peers, launches, ownership = self.exercise()
        self.assertTrue(result["passed"], result)
        self.assertEqual(len(launches), 1)
        self.assertEqual(len(peers), 2)
        self.assertTrue(all(p.closed for p in peers))
        self.assertTrue(all(ownership))
        self.assertTrue(result["backup_pair_passed"] and result["application_recovery_passed"])
        self.assertIsInstance(result["verification"], list)
        self.assertTrue(all(item["passed"] for item in result["verification"]))
        self.assertFalse(result["device_read_pair_matches"] or result["hardware_verified"] or
                         result["deployment_authorized"] or result["rom_rescue_verified"])
        for peer in peers:
            self.assertTrue(all(re.fullmatch(lab.Observer.COMMAND_PATTERN, c) for c in peer.commands))
            self.assertFalse(any(c.startswith(("reboot", "wf_time", "wf_cycle arm")) for c in peer.commands))
        with PortLease("SIMULATED-OTA-ROM", self.root / "locks"):
            pass

    def test_command_has_only_one_fixed_dual_read(self):
        argv = lab.command(Path("space dir/sftool.exe"), "COM5")
        self.assertEqual(argv.count("read_flash"), 1)
        self.assertEqual(argv[-2:], ["nor-first.bin@0x12000000:0x1000000", "nor-second.bin@0x12000000:0x1000000"])
        self.assertNotIn("write_flash", argv)
        self.assertNotIn("erase_flash", argv)
        for value in ("com5", "COM0", "COM5 --after no_reset", "COM5\n"):
            with self.subTest(value=value), self.assertRaises(ValueError):
                lab.command("tool", value)

    def test_final_runtime_hash_matches_complete_uart(self):
        result, _, _, _ = self.exercise()
        self.assertTrue(result["passed"])
        for phase in ("before", "after"):
            root = self.root / "run"
            records = json.loads((root / (phase + "-records.json")).read_text(encoding="utf-8"))
            raw = (root / (phase + "-uart.bin")).read_bytes()
            self.assertEqual(records["capture_sha256"], hashlib.sha256(raw).hexdigest())
            self.assertEqual(decoder.decode_capture(raw, records["token"]), records)

    def test_bad_or_unsolicited_baseline_never_launches(self):
        for scenario in ("bad-baseline", "unsolicited-boot", "cancel-before", "close-failure"):
            with self.subTest(scenario=scenario):
                self.root = Path(self.tmp.name) / scenario
                result, peers, launches, _ = self.exercise(scenario=scenario)
                self.assertFalse(result["passed"])
                self.assertEqual(launches, [])
                self.assertFalse(result["process_attempted"])
                self.assertEqual(result["resources_closed"], scenario != "close-failure")

    def test_cancel_and_timeout_preserve_partial_files_without_reopen(self):
        class Partial:
            def __init__(self, argv, output):
                (output / "nor-first.bin").write_bytes(b"partial")
                (output / "sftool.log").write_bytes(b"reader started")
                self.closed = False
            def poll(self): return None
            def close(self): self.closed = True
        for scenario in ("cancel-read", "timeout"):
            with self.subTest(scenario=scenario):
                self.root = Path(self.tmp.name) / scenario
                result, peers, launches, _ = self.exercise(scenario=scenario, reader=Partial)
                self.assertEqual(result["outcome"], "cancelled" if scenario == "cancel-read" else "timeout")
                self.assertEqual(len(launches), 1)
                self.assertEqual(len(peers), 1)
                self.assertTrue(result["recovery_required"] and result["resources_closed"])
                self.assertEqual((self.root / "run/nor-first.bin").read_bytes(), b"partial")

    def test_nonzero_reader_never_reopens_or_retries(self):
        class Failed(lab.SimulatedReader):
            def poll(self): return 101
        result, peers, launches, _ = self.exercise(reader=Failed)
        self.assertFalse(result["passed"])
        self.assertEqual(result["process_exit"], 101)
        self.assertEqual(len(peers), 1)
        self.assertEqual(len(launches), 1)

    def test_mismatching_pair_preserves_both_and_recovery(self):
        class Different(lab.SimulatedReader):
            def __init__(self, argv, output):
                super().__init__(argv, output)
                with (output / "nor-second.bin").open("r+b") as stream:
                    stream.seek(0x420000); stream.write(b"difference")
        result, peers, launches, _ = self.exercise(reader=Different)
        self.assertFalse(result["passed"] or result["backup_pair_passed"])
        self.assertTrue(result["application_recovery_passed"])
        self.assertEqual(len(peers), 2)
        self.assertEqual(len(launches), 1)
        self.assertTrue((self.root / "run/nor-first.bin").exists())
        self.assertTrue((self.root / "run/nor-second.bin").exists())

    def test_changed_identity_prevents_rom_launch(self):
        calls = []
        def identity():
            calls.append(1)
            if len(calls) == 2:
                raise ValueError("endpoint changed")
        result, peers, launches, _ = self.exercise(identity=identity)
        self.assertFalse(result["passed"])
        self.assertEqual(len(peers), 1)
        self.assertEqual(launches, [])

    def test_unexpected_second_boot_requires_recovery(self):
        result, _, _, _ = self.exercise(after_boot=3)
        self.assertFalse(result["passed"] or result["application_recovery_passed"])
        self.assertTrue(result["backup_pair_passed"])
        self.assertTrue(result["recovery_required"])

    def test_cleanup_failure_cannot_report_closed(self):
        class CannotClose(lab.SimulatedReader):
            def close(self):
                raise OSError("owned process exit uncertain")
        result, peers, launches, _ = self.exercise(reader=CannotClose)
        self.assertFalse(result["passed"] or result["resources_closed"])
        self.assertTrue(result["recovery_required"])
        self.assertEqual(len(peers), 1)
        self.assertEqual(len(launches), 1)
        self.assertTrue(result["lease_retained"])
        self.assertIsInstance(lab.RETAINED[-1][0], CannotClose)
        with self.assertRaises(RuntimeError):
            with PortLease("SIMULATED-OTA-ROM", self.root / "locks"): pass

    def test_constructor_cleanup_uncertainty_stays_owned_and_fails_closed(self):
        class CannotClose:
            def __init__(self, argv, output):
                raise ReaderCleanupError(self, "construction cleanup uncertain")
            def close(self):
                raise OSError("exit not confirmed")
        result, peers, launches, _ = self.exercise(reader=CannotClose)
        self.assertFalse(result["passed"] or result["resources_closed"])
        self.assertTrue(result["recovery_required"])
        self.assertEqual(len(peers), 1)
        self.assertEqual(len(launches), 1)
        self.assertTrue(result["lease_retained"])
        self.assertIsInstance(lab.RETAINED[-1][0], CannotClose)
        with self.assertRaises(RuntimeError):
            with PortLease("SIMULATED-OTA-ROM", self.root / "locks"): pass

    def test_recovery_refuses_rtc_jump_and_persisted_regression(self):
        before = dict(records=decoder.decode_capture(runtime_fixture(1), 7),
            replies={"date": ["Sun Nov 08 12:00:00 2026"]}, closed_ns=time.time_ns())
        after = dict(records=decoder.decode_capture(runtime_fixture(2), 7),
            replies={"date": ["Sun Nov 08 12:00:01 2026"]}, closed_ns=before["closed_ns"] + 1_000_000_000)
        self.assertTrue(lab.recovery(before, after)["rtc_continuity"])
        after["replies"]["date"] = ["Sun Nov 08 00:00:00 2026"]
        with self.assertRaisesRegex(ValueError, "RTC"):
            lab.recovery(before, after)
        after["replies"]["date"] = ["Sun Nov 08 12:00:01 2026"]
        after["records"]["saved"]["total_recorded_ms"] -= 1
        with self.assertRaisesRegex(ValueError, "backwards"):
            lab.recovery(before, after)

    def test_real_run_needs_explicit_reset_scope_before_any_io(self):
        with self.assertRaisesRegex(ValueError, "authorization"):
            lab.run(self.root / "blocked", binding={"simulation": False}, identity=lambda: {},
                transport_factory=None, process_factory=None, locks=self.root / "locks")
        self.assertFalse((self.root / "blocked").exists())


class HostProcessTests(unittest.TestCase):
    def test_native_process_exit_and_unicode_arguments(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "artifacts", prefix="ROM 测试 ") as directory:
            path = Path(directory)
            child = ReadProcess([sys.executable, "-c", "import sys; print(sys.argv[1])", "space value"], path)
            try:
                end = time.monotonic() + 5
                while child.poll() is None and time.monotonic() < end:
                    time.sleep(.02)
                self.assertEqual(child.poll(), 0)
            finally:
                child.close()
            self.assertIn(b"space value", (path / "sftool.log").read_bytes())

    def test_native_kills_descendants_on_cleanup(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "artifacts") as directory:
            path = Path(directory)
            code = ("import subprocess,sys,time; from pathlib import Path; "
                    "subprocess.Popen([sys.executable,'-c',\"import time; from pathlib import Path; time.sleep(1); Path('late').write_text('orphan')\"]); "
                    "Path('ready').write_text('ready'); time.sleep(30)")
            child = ReadProcess([sys.executable, "-c", code], path)
            try:
                end = time.monotonic() + 5
                while not (path / "ready").exists() and time.monotonic() < end:
                    time.sleep(.02)
                self.assertTrue((path / "ready").exists())
            finally:
                child.close()
            time.sleep(1.1)
            self.assertFalse((path / "late").exists())

    def test_missing_executable_keeps_launch_log(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "artifacts") as directory:
            path = Path(directory)
            with self.assertRaises(OSError):
                ReadProcess([str(path / "missing.exe")], path)
            self.assertTrue((path / "sftool.log").exists())

    @unittest.skipUnless(os.name == "nt", "Windows kill-on-parent-exit job")
    def test_parent_death_kills_reader(self):
        with tempfile.TemporaryDirectory(dir=ROOT / "artifacts") as directory:
            path = Path(directory)
            child_code = ("import time; from pathlib import Path; Path('ready').write_text('ready'); "
                          "time.sleep(1); Path('late').write_text('orphan'); time.sleep(30)")
            parent_code = ("import sys,time; from pathlib import Path; sys.path.insert(0,sys.argv[1]); "
                           "from ota_rom_process import ReadProcess; "
                           "p=ReadProcess([sys.executable,'-c',sys.argv[3]],Path(sys.argv[2])); time.sleep(30)")
            parent = subprocess.Popen([sys.executable, "-c", parent_code, str(ROOT / "scripts"), str(path), child_code],
                creationflags=subprocess.CREATE_NO_WINDOW, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            try:
                end = time.monotonic() + 5
                while not (path / "ready").exists() and time.monotonic() < end:
                    time.sleep(.02)
                self.assertTrue((path / "ready").exists())
            finally:
                parent.kill()
                parent.wait(timeout=5)
            time.sleep(1.1)
            self.assertFalse((path / "late").exists())


if __name__ == "__main__":
    unittest.main()
