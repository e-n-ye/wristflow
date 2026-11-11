"""No hardware: actual pinned-library binary, protocol faults and owned workers."""
import json
import copy
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import unittest
import zlib
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
import ota_stub_observe as observer
import ota_rom_device as device
from lab_evidence import digest, write_json
from ota_rom_backup import SimulatedReader
from test_ota_rom_backup import runtime_fixture


def registers():
    addresses = [base + i * 0x14 for base in (0x50081008, 0x40001008) for i in range(8)] + [0x50001008]
    return dict(pmr=4, rstr1=0, channels=[dict(address=address, ccr=0, count=0,
                 source=0, destination=0, enabled=False) for address in addresses])


class RegisterTests(unittest.TestCase):
    def test_lcpu_waiting_does_not_claim_all_users_controlled(self):
        value = observer.sample_summary(registers())
        self.assertTrue(value["cpuwait"])
        self.assertFalse(value["lcpu_reset_asserted"] or value["all_flash_users_controlled"])

    def test_enabled_uart_dma_and_flash_endpoint_remain_observations(self):
        value = registers()
        value["channels"][0].update(ccr=1, source=0x50084024, destination=0x20060000)
        value["channels"][1].update(ccr=1, source=0x12420000, destination=0x20060000)
        result = observer.sample_summary(value)
        self.assertTrue(result["enabled_dma"][0]["uart1_ram_endpoints"])
        self.assertTrue(result["enabled_dma"][1]["flash_endpoint"])
        self.assertFalse(result["all_flash_users_controlled"])

    def test_bad_or_missing_words_are_not_inferred_zero(self):
        for change in (lambda v: v["channels"].pop(),
                       lambda v: v.update(pmr=-1),
                       lambda v: v["channels"][0].update(ccr=True),
                       lambda v: v["channels"].__setitem__(0, None),
                       lambda v: v["channels"][0].update(address=0x50001000)):
            value = registers()
            change(value)
            with self.assertRaises(ValueError):
                observer.sample_summary(value)


class FtabStreamTests(unittest.TestCase):
    def setUp(self):
        self.original = bytes(range(256)) * 16

    def stream(self, payload, *, checksum=None):
        checksum = (zlib.crc32(payload, 0xffffffff) ^ 0xffffffff) & 0xffffffff if checksum is None else checksum
        return b"msh >start_trans\r\n" + payload + f"CRC:0x{checksum:08x}\r\nOK\r\nmsh >".encode()

    def test_full_payload_exact_boundary_crc_and_original_bytes_are_all_required(self):
        self.assertTrue(observer.ftab_stream_review(self.stream(self.original), self.original)["passed"])
        changed = bytes([self.original[0] ^ 1]) + self.original[1:]
        review = observer.ftab_stream_review(self.stream(changed), self.original)
        self.assertTrue(review["complete_length"] and review["crc_matches"])
        self.assertFalse(review["passed"] or review["original_bytes_match"])
        self.assertFalse(observer.ftab_stream_review(self.stream(self.original, checksum=0), self.original)["passed"])

    def test_short18_with_original_crc_and_ok_cannot_be_padded_or_accepted(self):
        crc = (zlib.crc32(self.original, 0xffffffff) ^ 0xffffffff) & 0xffffffff
        review = observer.ftab_stream_review(self.stream(self.original[:-18], checksum=crc), self.original)
        self.assertEqual(review["received_bytes"], 4078)
        self.assertTrue(review["early_tail_diagnostic_only"])
        self.assertFalse(review["passed"] or review["complete_length"] or review["crc_matches"])

    def test_payload_can_contain_marker_and_crc_text_without_changing_its_boundary(self):
        misleading = b"start_trans\r\nCRC:0x00000000\r\nOK\r\nmsh >"
        original = misleading + self.original[len(misleading):]
        self.assertTrue(observer.ftab_stream_review(self.stream(original), original)["passed"])

    def test_missing_truncated_tail_extra_bytes_and_oversized_stream_reject(self):
        full = self.stream(self.original)
        for wire in (b"", full[:-8], full + b"unexpected", full.replace(b"OK\r\n", b"Fail\r\n")):
            with self.subTest(length=len(wire)):
                self.assertFalse(observer.ftab_stream_review(wire, self.original)["passed"])
        with self.assertRaises(ValueError):
            observer.ftab_stream_review(b"x" * 65537, self.original)


class FtabPlanGuards(unittest.TestCase):
    def test_ftab_faults_and_unrelated_plans_are_rejected_before_output_or_worker(self):
        root = Path(tempfile.mkdtemp(prefix="ftab-plan-guard-", dir=ROOT / "artifacts"))
        for index, (plan, scenario) in enumerate((("geometry", "ftab-short18"), ("dual-nor", "host-error-after"),
                                                ("ftab", "short-read"), ("ftab", "pair-changed"))):
            with patch.object(observer, "prepare", return_value=({}, [])):
                with self.assertRaises(ValueError):
                    device.run(root / str(index), profile="stub-observe", observer={}, observer_plan=plan, scenario=scenario)
            self.assertFalse((root / str(index)).exists())


@unittest.skipUnless(os.environ.get("WRISTFLOW_STUB_OBSERVER_BUILD"),
                     "Set WRISTFLOW_STUB_OBSERVER_BUILD to a verified host build for binary/process tests")
class OwnedObserverTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # Retain simulation evidence under ignored artifacts; never use a device.
        cls.root = Path(tempfile.mkdtemp(prefix="stub-observer-tests-", dir=ROOT / "artifacts"))
        baseline = cls.root / "baseline"
        baseline.mkdir()
        SimulatedReader([], baseline)
        cls.options = dict(build=os.environ["WRISTFLOW_STUB_OBSERVER_BUILD"],
                           first=baseline / "nor-first.bin", second=baseline / "nor-second.bin")
        observer.prepare(cls.options)

    def exercise(self, scenario="success", *, cancel_phase=None, rom_seconds=45, plan="dual-nor"):
        output = self.root / (self._testMethodName + "-" + scenario + "-" + plan)
        threads = []
        def ready(console):
            def operator():
                while not console.sealed:
                    with console.condition:
                        prompt = console.prompt_value
                        if prompt and prompt["phase"] == cancel_phase:
                            console.cancel()
                            return
                        if prompt and not prompt.get("automatic") and console.answer is None:
                            console.respond(prompt["id"], "continue")
                    time.sleep(.002)
            thread = threading.Thread(target=operator, daemon=True)
            threads.append(thread)
            thread.start()
        result = device.run(output, profile="stub-observe", observer=self.options,
            fixtures=dict(before=runtime_fixture(1), after=runtime_fixture(2)), scenario=scenario,
            limits=dict(bind=15, before=10, rom=rom_seconds, after=10, grace=10),
            total_seconds=120, prompt_seconds=3, ready=ready, observer_plan=plan)
        for thread in threads:
            thread.join(timeout=2)
        return output, result

    def test_success_reuses_port_and_never_approves_writes(self):
        output, result = self.exercise()
        self.assertTrue(result["passed"], result)
        self.assertTrue(result["observation_passed"] and result["application_recovery_passed"])
        self.assertFalse(result["hardware_verified"] or result["deployment_authorized"])
        report = result["rom"]["observations"]["report"]
        self.assertFalse(report["all_flash_users_controlled"] or report["physical_erase_envelope_verified"])
        self.assertTrue(report["same_port_object"] and report["read_pair_passed"])
        raw = (output / "worker/stub-observer/tx-intents.bin").read_bytes()
        events = [json.loads(line) for line in (output / "worker/stub-observer/wire-events.jsonl").read_text().splitlines()]
        commands = [raw[e["offset"]:e["offset"]+e["bytes"]] for e in events if e["kind"] == "tx_intent"]
        self.assertEqual(commands.count(b"burn_read 0x12000000 0x01000000\r"), 2)
        self.assertEqual(commands.count(b"burn_reset\r"), 1)
        self.assertEqual([e["role"] for e in events if e["kind"] == "rts_pulse_intent"], ["entry"])
        self.assertTrue(result["resources_closed"] and not result["lease_retained"])
        self.assertTrue(all(item["passed"] for item in result["verification"]))

    def test_lcpu_running_is_visible_without_discarding_valid_reads(self):
        _, result = self.exercise("lcpu-running")
        self.assertTrue(result["observation_passed"], result)
        self.assertFalse(result["rom"]["observations"]["summaries"]["ready"]["cpuwait"])
        self.assertFalse(result["rom"]["observations"]["all_flash_users_controlled"])

    def test_full_nor_in_one_and_31_byte_fragments_keeps_crc_and_batches_trace(self):
        output, result = self.exercise("fragmented", rom_seconds=100)
        self.assertTrue(result["observation_passed"], result)
        report = result["rom"]["observations"]["report"]
        self.assertTrue(report["trace"]["flushed"])
        self.assertLess(report["trace"]["writes"]["rx"], 2000)
        self.assertLess(report["trace"]["writes"]["events"], 10000)
        offset, count = 0, 0
        with (output / "worker/stub-observer/wire-events.jsonl").open(encoding="utf-8") as stream:
            for line in stream:
                event = json.loads(line)
                if event["kind"] == "rx":
                    self.assertEqual(event["offset"], offset)
                    offset += event["bytes"]
                    count += 1
        self.assertGreater(count, 1_000_000)
        self.assertEqual(offset, (output / "worker/stub-observer/rx.bin").stat().st_size)

    def test_missing_152_bytes_with_crc_and_ok_cannot_be_adopted(self):
        output, result = self.exercise("short-read")
        self.assertFalse(result["observation_passed"])
        report = result["rom"]["observations"]["report"]
        self.assertIn("0x12FFC000", str(report["errors"]))
        self.assertTrue(report["fallback_completed"] and report["trace"]["flushed"])
        self.assertFalse((output / "worker/stub-observer/nor-first.bin").exists())
        self.assertFalse((output / "worker/stub-observer/nor-second.bin").exists())

    def test_geometry_reads_only_bounded_ram_and_retains_conditional_facts(self):
        output, result = self.exercise(plan="geometry")
        self.assertTrue(result["observation_passed"] and result["passed"], result)
        report = result["rom"]["observations"]["report"]
        summary = result["rom"]["observations"]["summaries"]["geometry"]
        self.assertEqual(summary["uart_address"], 0x20042260)
        self.assertTrue(summary["uart_rx_dma_bound_at_samples"])
        self.assertEqual(summary["selected_flash"]["handle"], 0x20041ff4)
        self.assertFalse(summary["all_flash_users_controlled"] or summary["physical_erase_envelope_verified"])
        self.assertFalse(report["read_pair_passed"])
        events = [json.loads(line) for line in (output / "worker/stub-observer/wire-events.jsonl").read_text().splitlines()]
        reads = [e for e in events if e["kind"] == "geometry_read_intent"]
        self.assertLessEqual(sum(e["bytes"] for e in reads), 776)
        self.assertTrue(all(0x20000000 <= e["address"] and e["address"]+e["bytes"] <= 0x20080000 for e in reads))
        self.assertFalse(any(e["kind"] == "nor_read_intent" for e in events))
        self.assertTrue(report["normal_start_command_completed"])
        build = observer.load(self.options["build"])
        stub = (ROOT / build["stub"]["path"]).read_bytes()
        corruptions = (lambda v: v["blocks"].pop(),
            lambda v: v["blocks"][0]["words"].pop(),
            lambda v: v["uart_dma"].update(address=0x50000000),
            lambda v: v["tables"][1]["records"][0]["block"].update(address=0x12000000),
            lambda v: v["tables"][1]["records"][0].update(index=27.0),
            lambda v: v["blocks"][0]["words"].__setitem__(0, True),
            lambda v: v.update(uart_address=0x20000ad8),
            lambda v: v["tables"][0].update(pointer=False))
        for corrupt in corruptions:
            value = copy.deepcopy(report["geometry"])
            corrupt(value)
            with self.assertRaises(ValueError):
                observer.geometry_summary(value, report["ready_registers"], stub)

    def test_geometry_bad_pointers_are_reported_without_dereference_or_write_permit(self):
        output, result = self.exercise("geometry-bad-pointer", plan="geometry")
        self.assertTrue(result["observation_passed"], result)
        summary = result["rom"]["observations"]["summaries"]["geometry"]
        self.assertFalse(summary["uart_rx_dma_bound_at_samples"])
        self.assertFalse(summary["selected_flash"]["commands"])
        events = [json.loads(line) for line in (output / "worker/stub-observer/wire-events.jsonl").read_text().splitlines()]
        self.assertTrue(all(0x20000000 <= e["address"] < 0x20080000 for e in events if e["kind"] == "geometry_read_intent"))

    def test_ftab_only_reads_same4k_once_and_independently_verifies_stream(self):
        for scenario in ("success", "fragmented"):
            output, result = self.exercise(scenario, plan="ftab")
            self.assertTrue(result["observation_passed"] and result["passed"], result)
            summary = result["rom"]["observations"]["summaries"]["ftab"]
            self.assertTrue(summary["independent_stream_passed"])
            self.assertEqual(summary["stream"]["received_bytes"], 4096)
            self.assertEqual(summary["host_after"]["errors"], 0)
            directory = output / "worker/stub-observer"
            events = [json.loads(line) for line in (directory / "wire-events.jsonl").read_text().splitlines()]
            wire = (directory / "tx.bin").read_bytes()
            commands = [wire[e["offset"]:e["offset"]+e["bytes"]] for e in events if e["kind"] == "tx"]
            self.assertEqual(commands.count(b"burn_read 0x12000000 0x00001000\r"), 1)
            self.assertEqual(commands.count(b"burn_reset\r"), 1)
            self.assertEqual(len([e for e in events if e["kind"] == "ftab_read_intent"]), 1)
            self.assertFalse(any(e["kind"] == "nor_read_intent" for e in events))
            self.assertFalse((directory / "nor-first.bin").exists())
            changed = copy.deepcopy(summary)
            changed["host_after"]["errors"] = True
            with self.assertRaises(ValueError):
                observer.ftab_summary(changed, directory, Path(self.options["first"]).read_bytes())

    def test_ftab_short_crc_or_changed_bytes_stop_and_keep_original_return_separate(self):
        for scenario in ("ftab-short18", "ftab-crc-fail", "ftab-changed", "host-error-after"):
            output, result = self.exercise(scenario, plan="ftab")
            self.assertFalse(result["passed"] or result["observation_passed"])
            self.assertTrue(result["application_recovery_passed"] and result["resources_closed"])
            report = result["rom"]["observations"]["report"]
            self.assertTrue(report["fallback_completed"])
            self.assertFalse(report["normal_start_attempted"])
            summary = result["rom"]["observations"]["summaries"]["ftab"]
            if scenario == "ftab-short18":
                self.assertEqual(summary["stream"]["received_bytes"], 4078)
                self.assertFalse((output / "worker/stub-observer/ftab4k.bin").exists())
            if scenario == "host-error-after":
                self.assertEqual(summary["host_after"]["errors"], 2)
                self.assertTrue(summary["independent_stream_passed"])

    def test_ftab_alias_and_host_status_gates_prevent_flash_read(self):
        for scenario in ("ftab-alias-nonzero", "ftab-alias-fail", "host-error-before", "host-status-fail", "lcpu-running"):
            output, result = self.exercise(scenario, plan="ftab")
            self.assertFalse(result["observation_passed"])
            events = [json.loads(line) for line in (output / "worker/stub-observer/wire-events.jsonl").read_text().splitlines()]
            self.assertFalse(any(e["kind"] in ("ftab_read_intent", "nor_read_intent", "original_start_intent") for e in events))
            self.assertTrue(result["resources_closed"])

    def test_crc_error_and_immutable_change_stop_before_second_read(self):
        for scenario in ("crc-fail", "immutable-changed"):
            output, result = self.exercise(scenario)
            self.assertFalse(result["passed"] or result["observation_passed"])
            self.assertTrue(result["resources_closed"])
            report = result["rom"]["observations"]["report"]
            self.assertTrue(report["fallback_attempted"] and report["fallback_completed"])
            self.assertFalse((output / "worker/stub-observer/nor-second.bin").exists())

    def test_register_error_is_not_a_zero_filled_success(self):
        _, result = self.exercise("register-read-fail")
        self.assertFalse(result["observation_passed"])
        self.assertFalse(result["rom"]["observations"]["summaries"])
        self.assertTrue(result["rom"]["observations"]["report"]["fallback_completed"])

    def test_pair_change_retains_both_files_and_stops(self):
        output, result = self.exercise("pair-changed")
        self.assertFalse(result["observation_passed"])
        for name in ("nor-first.bin", "nor-second.bin"):
            self.assertEqual((output / "worker/stub-observer" / name).stat().st_size, 0x1000000)

    def test_rtc_reset_preserves_core_result_and_final_confirmation(self):
        _, result = self.exercise("rtc-reset")
        self.assertFalse(result["passed"])
        self.assertTrue(result["observation_passed"] and result["application_recovery_passed"])
        self.assertTrue(result["clock_recovery_required"])
        self.assertTrue(result["verification"][3]["passed"])
        self.assertFalse(result["verification"][4]["passed"])
        self.assertEqual({item["phase"] for item in result["errors"]}, {"rtc"})

    def test_cancel_before_entry_never_starts_bridge(self):
        output, result = self.exercise(cancel_phase="prepare")
        self.assertFalse(result["worker_attempted"])
        self.assertFalse((output / "worker/stub-request.json").exists())

    def test_blocking_driver_is_killed_under_owned_job(self):
        _, result = self.exercise("hang-read", rom_seconds=2, cancel_phase="return")
        self.assertFalse(result["passed"] or result["observation_passed"])
        self.assertTrue(result["resources_closed"])
        self.assertFalse(result["lease_retained"])
        self.assertTrue(result["recovery_required"])
        self.assertTrue(result["rom"]["cleanup"]["child_forced"])
        self.assertIsInstance(result["rom"]["cleanup"]["child_exit"], int)

    def test_corrupt_result_preserves_unknown_entry_and_confirmed_process_close(self):
        for index, payload in enumerate(("{broken", "[]", "null")):
            with self.subTest(payload=payload):
                directory = self.root / (self._testMethodName + str(index))
                directory.mkdir()
                class Child:
                    def __init__(self, argv, **kwargs):
                        request = observer.load(argv[2])
                        out = Path(request["out"])
                        out.mkdir()
                        (out / "observer.json").write_text(payload, encoding="utf-8")
                        self.returncode = 0
                    def poll(self):
                        return 0
                    def wait(self, **kwargs):
                        return 0
                config = dict(observer=self.options, simulation=True, rom_reset_authorized=True,
                              allow_exit_reset=True, scenario="success")
                with patch.object(observer.subprocess, "Popen", Child):
                    value = observer.probe(config, directory, "SIMULATED-STUB", time.monotonic() + 10)
                self.assertIsNone(value["entry_attempted"])
                self.assertTrue(value["cleanup"]["resources_closed"])
                self.assertFalse(value["observations"]["observation_passed"])
                self.assertIn("evidence rejected", value["error"])

    def test_invalid_capture_request_cannot_create_output(self):
        options, _ = observer.prepare(self.options)
        build = observer.load(options["build"])
        binary = (ROOT / build["executable"]["path"]).resolve()
        for index, changes in enumerate((dict(port="COM5"), dict(entry_reset_authorized=False),
                                         dict(flash_address=0x12420000), dict(scenario="unknown"), dict(plan="unknown"))):
            out = self.root / ("denied-" + str(index))
            request = dict(schema=1, simulation=True, plan="dual-nor", port="SIMULATED-STUB", out=str(out),
                stub={**build["stub"], "path": str((ROOT / build["stub"]["path"]).resolve())},
                baseline={"path": options["first"], "bytes":0x1000000,"sha256":digest(options["first"])},
                cancel=str(self.root / "nonexistent-cancel"), seconds=10, entry_reset_authorized=True,
                ram_stub_authorized=True, register_observation_authorized=True,
                original_start_authorized=True, exit_reset_authorized=True, scenario="success")
            request.update(changes)
            path = self.root / ("denied-request-" + str(index) + ".json")
            write_json(path, request)
            result = subprocess.run([str(binary), "--config", str(path), digest(path)], capture_output=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertFalse(out.exists())


if __name__ == "__main__":
    unittest.main()
