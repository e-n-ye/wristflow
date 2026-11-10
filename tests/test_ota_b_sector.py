"""No device: pinned library, synthetic UART, fixed ranges and owned cleanup."""
import copy
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_b_sector as sector
import ota_rom_device as device
from lab_evidence import file_record, digest, write_json
from test_ota_rom_backup import runtime_fixture
from ota_rom_backup import SimulatedReader


class InputGuards(unittest.TestCase):
    def test_capture_requires_both_write_and_restore_before_output(self):
        out = ROOT / "artifacts/denied-b-sector-should-not-exist"
        capture = dict(port="COM5", conditions_confirmed=True, rom_reset_authorized=True,
            exit_reset_authorized=True, stub_observation_authorized=True, original_start_authorized=True,
            b_sector_write_authorized=True)
        # Isolate input preparation and exercise the exact scope guard, rather
        # than accidentally passing this test on a missing build-input error.
        with patch.object(sector, "prepare", return_value=({}, [])):
            with self.assertRaisesRegex(ValueError, "Explicit fixed B write AND restore authorization"):
                device.run(out, capture=capture, profile="stub-b-sector")
        self.assertFalse(out.exists())


@unittest.skipUnless(os.environ.get("WRISTFLOW_B_SECTOR_BUILD"), "Set verified B-sector host build")
class SectorBinary(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(tempfile.mkdtemp(prefix="ota-b-sector-tests-", dir=ROOT / "artifacts"))
        baseline = cls.root / "baseline"
        baseline.mkdir()
        SimulatedReader([], baseline)
        cls.options = dict(build=os.environ["WRISTFLOW_B_SECTOR_BUILD"], first=baseline / "nor-first.bin",
            second=baseline / "nor-second.bin", candidate=ROOT / "artifacts/ota-execution-probe-20261108-006/probe-b-sector.bin")
        sector.prepare(cls.options)
        cls.build = sector.load(cls.options["build"])
        cls.exe = ROOT / cls.build["executable"]["path"]

    def binary(self, name, scenario="success", changes=None):
        out = self.root / name
        request = dict(schema=1, simulation=True, port="SIMULATED-STUB", out=str(out),
            stub=dict(self.build["stub"], path=str((ROOT / self.build["stub"]["path"]).resolve())),
            baseline=file_record(self.options["first"]), candidate=file_record(self.options["candidate"]),
            cancel=str(self.root / (name + "-cancel.json")), seconds=30, scenario=scenario,
            entry_reset_authorized=True, ram_stub_authorized=True, register_observation_authorized=True,
            b_sector_write_authorized=True, b_sector_restore_authorized=True, original_start_authorized=True, exit_reset_authorized=True)
        request.update(changes or {})
        path = self.root / (name + "-request.json")
        write_json(path, request)
        result = subprocess.run([str(type(self).exe), "--config", str(path), digest(path)],
            capture_output=True, timeout=40)
        report = sector.load(out / "sector.json") if (out / "sector.json").exists() else {}
        return out, request, path, result, report

    def exercise(self, name, scenario="success", cancel_after_write=False, cancel_hang=False):
        output = self.root / name
        threads = []
        def ready(console):
            def operator():
                while not console.sealed:
                    events = output / "worker/b-sector/wire-events.jsonl"
                    if cancel_hang and events.exists() and '"kind":"tx"' in events.read_text(encoding="utf-8"):
                        console.cancel()
                        return
                    if cancel_after_write and events.exists() and '"role":"candidate"' in events.read_text(encoding="utf-8"):
                        console.cancel()
                        return
                    with console.condition:
                        prompt = console.prompt_value
                        if prompt and not prompt.get("automatic") and console.answer is None:
                            console.respond(prompt["id"], "continue")
                    time.sleep(.002)
            thread = threading.Thread(target=operator, daemon=True)
            threads.append(thread)
            thread.start()
        report = device.run(output, profile="stub-b-sector", observer=self.options,
            fixtures=dict(before=runtime_fixture(1), after=runtime_fixture(2)), scenario=scenario,
            limits=dict(bind=15, before=10, rom=30, after=10, grace=10), total_seconds=100, prompt_seconds=3, ready=ready)
        for t in threads:
            t.join(timeout=2)
        return output, report

    def test_success_original_protocol_two_exact_writes_three_reads_and_normal_start(self):
        out, request, path, process, report = self.binary("success")
        self.assertEqual(process.returncode, 0, process.stderr)
        sector.verified_result(report, out, request, path)
        self.assertTrue(report["passed"] and report["restored_verified"])
        self.assertFalse(report["b_may_be_changed"] or report["fallback_attempted"])
        events = [json.loads(x) for x in (out / "wire-events.jsonl").read_text().splitlines()]
        writes = [e for e in events if e["kind"] == "sector_write_intent"]
        self.assertEqual([e["role"] for e in writes], ["candidate", "restore"])
        self.assertTrue(all((e["address"], e["bytes"]) == (0x12420000, 4096) for e in writes))
        self.assertEqual(len([e for e in events if e["kind"] == "sector_read_intent"]), 3)
        before = (out / "before-b64k.bin").read_bytes()
        self.assertEqual((out / "fresh-restore-b4k.bin").read_bytes(), before[:4096])
        self.assertEqual((out / "restored-b64k.bin").read_bytes(), before)
        self.assertEqual((out / "candidate-b64k.bin").read_bytes()[:4096], Path(self.options["candidate"]).read_bytes())
        self.assertEqual((out / "candidate-b64k.bin").read_bytes()[4096:], before[4096:])
        self.assertTrue(any(sector.load(p)["candidate_write_attempted"] for p in out.glob("state-*.json")))
        for corrupt in (lambda v: v.update(restored_verified=False), lambda v: v.update(b_may_be_changed=True),
                        lambda v: v.update(candidate_write_attempted=False), lambda v: v.update(config_sha="foreign")):
            value = copy.deepcopy(report)
            corrupt(value)
            with self.assertRaises(ValueError):
                sector.verified_result(value, out, request, path)

    def test_finite_prewrite_failures_never_attempt_candidate_or_restore(self):
        for name in ("lcpu-running", "register-read-fail", "before-changed", "prestate-crc-fail"):
            with self.subTest(name=name):
                out, request, path, process, report = self.binary(name, name)
                self.assertEqual(process.returncode, 1)
                sector.verified_result(report, out, request, path)
                self.assertFalse(report["candidate_write_attempted"] or report["restore_write_attempted"] or report["b_may_be_changed"])
                self.assertFalse((out / "candidate-b64k.bin").exists())

    def test_fragmented_official_reads_complete_without_expanding_flash_range(self):
        out, request, path, process, report = self.binary("fragmented", "fragmented")
        self.assertEqual(process.returncode, 0, process.stderr)
        sector.verified_result(report, out, request, path)
        self.assertTrue(report["passed"] and report["restored_verified"])

    def test_complete_bad_candidate_is_restored_once_but_experiment_fails(self):
        out, request, path, process, report = self.binary("bad-candidate", "candidate-mismatch")
        self.assertEqual(process.returncode, 1)
        sector.verified_result(report, out, request, path)
        self.assertFalse(report["candidate_verified"])
        self.assertTrue(report["restored_verified"] and report["normal_start_command_completed"])
        self.assertFalse(report["b_may_be_changed"])

    def test_guard_damage_and_failed_restore_never_claim_full_recovery(self):
        for name in ("guard-change", "restore-mismatch"):
            with self.subTest(name=name):
                out, request, path, process, report = self.binary(name, name)
                self.assertEqual(process.returncode, 1)
                sector.verified_result(report, out, request, path)
                self.assertTrue(report["restore_write_attempted"] and report["b_may_be_changed"])
                self.assertFalse(report["restored_verified"] or report["normal_start_command_completed"])
                self.assertEqual((out / "fresh-restore-b4k.bin").stat().st_size, 4096)

    def test_unknown_protocol_after_mutation_never_sends_blind_restore(self):
        for name in ("write-start-fail", "candidate-data-timeout", "readback-crc-fail"):
            with self.subTest(name=name):
                out, request, path, process, report = self.binary(name, name)
                self.assertEqual(process.returncode, 1)
                sector.verified_result(report, out, request, path)
                self.assertTrue(report["candidate_write_attempted"] and report["b_may_be_changed"])
                self.assertFalse(report["restore_write_attempted"] or report["normal_start_command_completed"])
        out, request, path, process, report = self.binary("restore-data-fail", "restore-data-fail")
        sector.verified_result(report, out, request, path)
        self.assertTrue(report["restore_write_attempted"] and report["b_may_be_changed"])
        self.assertFalse(report["restore_write_completed"] or report["normal_start_command_completed"])

    def test_argument_injection_and_partial_authorization_refused_before_any_open(self):
        cases = (dict(port="COM5"), dict(b_sector_restore_authorized=False), dict(b_sector_write_authorized=False),
                 dict(address=0x12010000), dict(scenario="unknown"), dict(seconds=9999))
        for i, change in enumerate(cases):
            out, _, _, process, report = self.binary("denied-" + str(i), changes=change)
            self.assertEqual(process.returncode, 2)
            self.assertFalse(out.exists())

    def test_unchanged_candidate_is_not_reported_as_a_write_experiment(self):
        data = bytearray(Path(self.options["first"]).read_bytes())
        data[0x420000:0x421000] = Path(self.options["candidate"]).read_bytes()
        baseline = self.root / "same-as-candidate.bin"
        baseline.write_bytes(data)
        out, request, path, process, report = self.binary("same-candidate", changes=dict(baseline=file_record(baseline)))
        self.assertEqual(process.returncode, 1)
        self.assertFalse(report["candidate_write_attempted"] or report["restore_write_attempted"])
        sector.verified_result(report, out, request, path)

    def test_owned_parent_success_and_rtc_independent_with_original_app_recovery(self):
        for name in ("success", "rtc-reset"):
            with self.subTest(name=name):
                out, report = self.exercise("owned-" + name, name)
                self.assertTrue(report["observation_passed"] and report["application_recovery_passed"] and report["restored_verified"], report)
                self.assertTrue(report["flash_write"])
                self.assertFalse(report["b_may_be_changed"] or report["recovery_required"])
                self.assertTrue(report["resources_closed"] and not report["lease_retained"])
                self.assertEqual(report["passed"], name == "success")
                self.assertFalse(report["hardware_verified"] or report["deployment_authorized"])

    def test_owned_unknown_mutation_still_needs_recovery_after_application_returns(self):
        _, report = self.exercise("owned-unknown", "write-start-fail")
        self.assertFalse(report["passed"] or report["restored_verified"])
        self.assertTrue(report["application_recovery_passed"] and report["b_may_be_changed"] and report["recovery_required"])

    def test_cancel_after_mutation_stops_protocol_and_retains_possible_change(self):
        out, report = self.exercise("owned-cancel", "candidate-data-timeout", cancel_after_write=True)
        self.assertFalse(report["passed"] or report["restored_verified"])
        self.assertTrue(report["resources_closed"] and report["recovery_required"])
        result_path = out / "worker/b-sector/sector.json"
        if result_path.exists():
            v = sector.load(result_path)
            self.assertTrue(v["candidate_write_attempted"])
            self.assertFalse(v["restore_write_attempted"])

    def test_hung_child_is_reaped_and_missing_report_cannot_claim_no_mutation(self):
        _, report = self.exercise("owned-hang", "b-hang-read", cancel_hang=True)
        self.assertFalse(report["passed"] or report["restored_verified"])
        self.assertIsNone(report["flash_write"])
        self.assertIsNone(report["b_may_be_changed"])
        self.assertTrue(report["resources_closed"] and report["recovery_required"])
        self.assertFalse(report["lease_retained"])


if __name__ == "__main__":
    unittest.main()
