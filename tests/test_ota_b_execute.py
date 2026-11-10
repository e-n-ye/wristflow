"""Pinned original library against synthetic UART; no device endpoints."""
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
import ota_b_execute as execution
import ota_rom_device as device
from lab_evidence import digest, file_record, write_json
from test_ota_rom_backup import runtime_fixture


class ScopeGuards(unittest.TestCase):
    def test_scope_and_no_fallback_are_required_before_any_worker(self):
        output = ROOT / "artifacts/denied-probe-must-not-exist"
        capture = dict(conditions_confirmed=True, rom_reset_authorized=True, port="COM5",
                       stub_observation_authorized=True, original_start_authorized=True)
        with patch.object(execution, "prepare", return_value=({}, [])):
            with self.assertRaisesRegex(ValueError, "execution and restoration scope"):
                device.run(output, profile="stub-b-execute", capture=capture, allow_exit_reset=False)
            with self.assertRaisesRegex(ValueError, "no fallback"):
                device.run(output, profile="stub-b-execute", capture=capture)
        self.assertFalse(output.exists())


@unittest.skipUnless(os.environ.get("WRISTFLOW_B_EXECUTE_BUILD"), "Set verified fixed execution build")
class ExecutionBinary(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root = Path(tempfile.mkdtemp(prefix="ota-b-execute-tests-", dir=ROOT / "artifacts"))
        cls.options = dict(build=os.environ["WRISTFLOW_B_EXECUTE_BUILD"],
            first=ROOT / "artifacts/ota-rom-backup-20261108/device-001/nor-first.bin",
            second=ROOT / "artifacts/ota-rom-backup-20261108/device-001/nor-second.bin",
            package=ROOT / "artifacts/ota-execution-probe-20261108-006", recovery_from=None)
        execution.prepare(cls.options)
        cls.build = execution.load(cls.options["build"])
        cls.exe = ROOT / cls.build["executable"]["path"]
        cls.review = execution.load(cls.options["package"] / "review.json")

    def binary(self, name, scenario="success", changes=None):
        out = self.root / name
        request = dict(schema=1, simulation=True, port="SIMULATED-STUB", out=str(out),
            stub=dict(self.build["stub"], path=str((ROOT / self.build["stub"]["path"]).resolve())),
            baseline=file_record(self.options["first"]), candidate=file_record(self.options["package"] / "probe-b-sector.bin"),
            boot=file_record(self.options["package"] / "boot-compatible-58496.bin"), restore_boot=None, restore_b=None, recovery_only=False,
            cancel=str(self.root / (name + "-cancel.json")), seconds=30, listen_seconds=1, scenario=scenario,
            entry_reset_authorized=True, ram_stub_authorized=True, register_observation_authorized=True,
            b_sector_write_authorized=True, boot_write_authorized=True, probe_start_authorized=True,
            restore_entry_authorized=True, restore_writes_authorized=True, original_start_authorized=True)
        request.update(changes or {})
        path = self.root / (name + "-request.json")
        write_json(path, request)
        process = subprocess.run([str(type(self).exe), "--config", str(path), digest(path)], capture_output=True, timeout=40)
        report = execution.load(out / "execution.json") if (out / "execution.json").exists() else {}
        if report:
            execution.verified_result(report, out, request, path, self.review)
        return out, request, path, process, report

    def owned(self, name, scenario="success", recovery_from=None, cancel=False):
        out = self.root / name
        threads = []
        def ready(console):
            def operator():
                while not console.sealed:
                    wire = out / "worker/b-execute/wire-events.jsonl"
                    trigger = '"role":"boot_install"' if cancel == "after-boot" else '"kind":"tx"'
                    if cancel and wire.exists() and trigger in wire.read_text(encoding="utf-8"):
                        console.cancel()
                        return
                    with console.condition:
                        prompt = console.prompt_value
                        if prompt and not prompt.get("automatic") and console.answer is None:
                            console.respond(prompt["id"], "continue")
                    time.sleep(.003)
            thread = threading.Thread(target=operator, daemon=True)
            thread.start()
            threads.append(thread)
        report = device.run(out, profile="stub-b-execute", observer=dict(self.options, recovery_from=recovery_from),
            fixtures=dict(before=runtime_fixture(1), after=runtime_fixture(2)), scenario=scenario, allow_exit_reset=False,
            limits=dict(bind=15, before=10, rom=40, after=10, grace=10), total_seconds=110, prompt_seconds=3, ready=ready)
        for thread in threads:
            thread.join(timeout=2)
        return out, report

    def test_success_four_fixed_writes_nine_reads_three_frames_and_two_entries(self):
        out, request, path, process, report = self.binary("success")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertTrue(report["probe_verified"] and report["restored_verified"] and report["original_start_completed"])
        events = [json.loads(x) for x in (out / "wire-events.jsonl").read_text().splitlines()]
        writes = [v for v in events if v["kind"] == "execution_write_intent"]
        self.assertEqual([(v["role"], v["address"], v["bytes"]) for v in writes], [
            ("b_install", 0x12420000, 4096), ("boot_install", 0x12010000, 58496),
            ("boot_restore", 0x12010000, 65536), ("b_restore", 0x12420000, 4096)])
        self.assertEqual(len([v for v in events if v["kind"] == "execution_read_intent"]), 9)
        self.assertEqual(len([v for v in events if v["kind"] == "rts_intent" and v["value"]]), 2)
        self.assertEqual((out / "restored-boot64k.bin").read_bytes(), (out / "before-boot64k.bin").read_bytes())
        self.assertEqual((out / "restored-b64k.bin").read_bytes(), (out / "before-b64k.bin").read_bytes())
        for changes in (dict(probe_verified=True, probe_frames=[]), dict(restored_verified=False), dict(boot_restored=False)):
            changed = copy.deepcopy(report)
            changed.update(changes)
            with self.assertRaises(ValueError):
                execution.verified_result(changed, out, request, path, self.review)

    def test_fragmented_reads_and_probe_frames_keep_same_finite_scope(self):
        _, _, _, process, report = self.binary("fragmented", "fragmented")
        self.assertEqual(process.returncode, 0, process.stderr)
        self.assertTrue(report["passed"])

    def test_ftab_change_stops_before_candidate(self):
        _, _, _, process, report = self.binary("ftab-change", "before-changed")
        self.assertEqual(process.returncode, 1)
        self.assertFalse(report["b_install_attempted"] or report["boot_install_attempted"] or report["probe_start_attempted"])

    def test_complete_install_comparison_failure_restores_without_starting_probe(self):
        for scenario in ("b-compare", "boot-compare"):
            with self.subTest(scenario=scenario):
                _, _, _, process, report = self.binary(scenario, scenario)
                self.assertEqual(process.returncode, 1)
                self.assertTrue(report["restored_verified"] and report["original_start_completed"])
                self.assertFalse(report["probe_start_attempted"])
                if scenario == "b-compare":
                    self.assertFalse(report["boot_install_attempted"] or report["boot_restore_attempted"])

    def test_missing_crc_tag_truncated_or_duplicate_frames_fail_but_restore_once(self):
        for scenario in ("no-frames", "bad-tag", "bad-crc", "short-frames", "duplicate-frames"):
            with self.subTest(scenario=scenario):
                _, _, _, process, report = self.binary(scenario, scenario)
                self.assertEqual(process.returncode, 1)
                self.assertFalse(report["probe_verified"])
                self.assertTrue(report["restored_verified"] and report["original_start_completed"])

    def test_nonzero_second_entry_alias_blocks_all_restoration_access(self):
        out, _, _, process, report = self.binary("alias", "alias-retained")
        self.assertEqual(process.returncode, 1)
        self.assertTrue(report["boot_may_be_changed"] and report["b_may_be_changed"])
        self.assertFalse(report["boot_restore_attempted"] or report["b_restore_attempted"] or report["original_start_completed"])
        self.assertFalse((out / "reentry-boot64k.bin").exists())

    def test_unknown_write_or_restore_errors_never_reset_or_retry(self):
        for scenario in ("write-fail", "restore-boot-fail", "restore-b-fail", "restore-read-fail"):
            with self.subTest(scenario=scenario):
                _, _, _, process, report = self.binary(scenario, scenario)
                self.assertEqual(process.returncode, 1)
                self.assertFalse(report["original_start_completed"] or report["fallback_attempted"] or report["restored_verified"])
                if scenario == "restore-boot-fail":
                    self.assertFalse(report["b_restore_attempted"])

    def test_arguments_and_partial_scope_rejected_before_open(self):
        for i, changes in enumerate((dict(port="COM5"), dict(address=0x12000000), dict(boot_write_authorized=False),
                dict(restore_entry_authorized=False), dict(restore_writes_authorized=False), dict(seconds=9999))):
            out, _, _, process, _ = self.binary("denied-" + str(i), changes=changes)
            self.assertEqual(process.returncode, 2)
            self.assertFalse(out.exists())

    def test_failed_start_ack_never_claims_started_or_adds_reset(self):
        for scenario in ("probe-start-fail", "original-start-fail"):
            out, _, _, process, report = self.binary(scenario, scenario)
            self.assertEqual(process.returncode, 1)
            self.assertFalse(report["original_start_completed"])
            if scenario == "probe-start-fail":
                self.assertFalse(report["probe_start_completed"] or report["boot_restore_attempted"])
            events = [json.loads(x) for x in (out / "wire-events.jsonl").read_text().splitlines()]
            self.assertEqual(sum(v["kind"] == "rts_intent" and v["value"] for v in events), 1 if scenario == "probe-start-fail" else 2)

    def test_initial_or_second_entry_register_failure_stops_all_new_tx(self):
        for scenario in ("initial-register-fail", "restore-register-fail", "restore-alias-fail"):
            out, _, _, process, report = self.binary(scenario, scenario)
            self.assertEqual(process.returncode, 1)
            self.assertFalse(report["original_start_completed"] or report["fallback_attempted"])
            events = [json.loads(x) for x in (out / "wire-events.jsonl").read_text().splitlines()]
            final = [v for v in events if v["kind"] == "tx"][-1]
            raw = (out / "tx.bin").read_bytes()[final["offset"]:final["offset"] + final["bytes"]]
            self.assertEqual(raw[6:8], b"@r")
            self.assertEqual(int.from_bytes(raw[8:12], "little"), 0x5004206c if scenario == "restore-alias-fail" else 0x40040000)
            self.assertEqual(sum(v["kind"] == "rts_intent" and v["value"] for v in events), 1 if scenario == "initial-register-fail" else 2)

    def test_owned_rtc_invalid_is_separate_from_execution_and_restoration(self):
        out, report = self.owned("owned-rtc", "rtc-reset")
        self.assertTrue(report["passed"] and report["restored_verified"] and report["application_recovery_passed"], report)
        self.assertFalse(report["verification"][-1]["passed"])
        self.assertTrue(report["resources_closed"] and report["clock_recovery_required"])
        self.assertFalse(report["recovery_required"] or report["hardware_verified"])

    def test_explicit_recovery_reuses_prior_raw_and_restores_without_reinstall_or_probe(self):
        prior, failed = self.owned("prior-failed", "restore-boot-fail")
        self.assertTrue(failed["recovery_required"])
        out, report = self.owned("recovery", recovery_from=prior)
        self.assertTrue(report["passed"] and report["restored_verified"] and report["application_recovery_passed"], report)
        child = execution.load(out / "worker/b-execute/execution.json")
        self.assertTrue(child["recovery_only"])
        self.assertFalse(child["b_install_attempted"] or child["boot_install_attempted"] or child["probe_start_attempted"])
        before = execution.load(out / "worker/before-snapshot.json")
        self.assertTrue(before["baseline_from_prior_window"])
        self.assertNotEqual((out / "worker/b-execute/before-boot64k.bin").read_bytes(),
                            (out / "worker/b-execute/restored-boot64k.bin").read_bytes())
        self.assertNotEqual((out / "worker/b-execute/before-b64k.bin").read_bytes(),
                            (out / "worker/b-execute/restored-b64k.bin").read_bytes())
        events = (out / "worker/events.jsonl").read_text()
        self.assertNotIn('"kind": "transport_open", "phase": "before"', events)
        self.assertFalse(report["recovery_required"])

    def test_owned_valid_probe_result_survives_later_alias_restore_blocker(self):
        _, report = self.owned("owned-alias", "alias-retained")
        self.assertFalse(report["passed"] or report["restored_verified"])
        self.assertTrue(report["verification"][0]["passed"])
        self.assertFalse(report["verification"][1]["passed"])
        self.assertTrue(report["resources_closed"] and report["recovery_required"])

    def test_hung_child_cancel_keeps_unknown_mutation_and_reaps_resources(self):
        _, report = self.owned("hung", "execute-hang", cancel=True)
        self.assertIsNone(report["boot_may_be_changed"])
        self.assertIsNone(report["b_may_be_changed"])
        self.assertTrue(report["resources_closed"] and report["recovery_required"])
        self.assertFalse(report["lease_retained"])

    def test_cancel_after_boot_mutation_never_automatically_starts_recovery(self):
        out, report = self.owned("cancel-after-boot", cancel="after-boot")
        self.assertFalse(report["passed"] or report.get("restored_verified"))
        self.assertTrue(report["resources_closed"] and report["recovery_required"])
        child_path = out / "worker/b-execute/execution.json"
        if child_path.exists():
            child = execution.load(child_path)
            self.assertTrue(child["boot_install_attempted"])
            self.assertFalse(child["boot_restore_attempted"] or child["b_restore_attempted"] or child["original_start_completed"])


if __name__ == "__main__":
    unittest.main()
