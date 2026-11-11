"""Fixed64KiB receive/rate comparison; only synthetic serial endpoints."""
import copy
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"scripts"))
import ota_stub_observe as observer
import ota_rom_device as device
from ota_rom_backup import SimulatedReader
import test_ota_stub_observe as existing_tests

class ReceiveBounds(unittest.TestCase):
    def test_short26_with_complete_original_crc_never_becomes_a_valid_read(self):
        original=bytes(range(256))*256
        short=original[:19648]+original[19662:29934]+original[29946:]
        checksum=(zlib.crc32(original,0xffffffff)^0xffffffff)&0xffffffff
        raw=b"start_trans\r\n"+short+f"CRC:0x{checksum:08x}\r\nOK\r\nmsh >".encode()
        checked=observer.fixed_stream_review(raw,original)
        self.assertEqual(checked["received_bytes"],65510)
        self.assertTrue(checked["early_tail_diagnostic_only"])
        self.assertFalse(checked["passed"] or checked["crc_matches"])

    def test_rate_plan_and_faults_cannot_escape_the_diagnostic_before_output(self):
        root=Path(tempfile.mkdtemp(prefix="boot-receive-plan-",dir=ROOT/"artifacts"))
        for i,(plan,scenario) in enumerate((("dual-nor","boot-short26"),("ftab","speed-fail"),("boot64k-1m","speed-fail"),("boot64k-500k","pair-changed"))):
            with patch.object(observer,"prepare",return_value=({},[])):
                with self.assertRaises(ValueError):device.run(root/str(i),profile="stub-observe",observer={},observer_plan=plan,scenario=scenario)
            self.assertFalse((root/str(i)).exists())

@unittest.skipUnless(os.environ.get("WRISTFLOW_STUB_OBSERVER_BUILD"),"Set verified observer build")
class BootReceiveBinary(unittest.TestCase):
    exercise=existing_tests.OwnedObserverTests.exercise
    @classmethod
    def setUpClass(cls):
        cls.root=Path(tempfile.mkdtemp(prefix="boot-receive-tests-",dir=ROOT/"artifacts"))
        baseline=cls.root/"baseline";baseline.mkdir();SimulatedReader([],baseline)
        cls.options=dict(build=os.environ["WRISTFLOW_STUB_OBSERVER_BUILD"],first=baseline/"nor-first.bin",second=baseline/"nor-second.bin")
        observer.prepare(cls.options)

    def wire(self,out):
        directory=out/"worker/stub-observer"
        events=[json.loads(x) for x in (directory/"wire-events.jsonl").read_text().splitlines()]
        raw=(directory/"tx.bin").read_bytes()
        return directory,events,[raw[e["offset"]:e["offset"]+e["bytes"]] for e in events if e["kind"]=="tx"]

    def test_both_rates_and_fragments_keep_fixed_three_reads_and_ordered_official_handshake(self):
        for plan in ("boot64k-1m","boot64k-500k"):
            for scenario in ("success","fragmented"):
                with self.subTest(plan=plan,scenario=scenario):
                    out,result=self.exercise(scenario,plan=plan)
                    self.assertTrue(result["passed"] and result["observation_passed"],result)
                    self.assertTrue(result["resources_closed"] and result["application_recovery_passed"])
                    summary=result["rom"]["observations"]["summaries"]["boot_receive"]
                    self.assertEqual(len(summary["independent_streams"]),3)
                    self.assertTrue(all(r["passed"] and r["received_bytes"]==65536 for r in summary["independent_streams"]))
                    directory,events,commands=self.wire(out)
                    self.assertEqual(commands.count(b"burn_read 0x12010000 0x00010000\r"),3)
                    self.assertEqual(commands.count(b"burn_reset\r"),1)
                    exits=[e for e in events if e["kind"]=="debug_exit_response"]
                    self.assertEqual([e["phase"] for e in exits],["ready_snapshot","physical_aliases"])
                    self.assertTrue(all(e["frame_hex"]=="7e7902001000d006" for e in exits))
                    self.assertFalse(any(c.startswith(b"burn_erase") for c in commands))
                    intents=[e for e in events if e["kind"]=="boot_receive_read_intent"]
                    self.assertEqual([e["index"] for e in intents],[0,1,2])
                    self.assertEqual([e["baud"] for e in intents],[500000 if plan.endswith("500k") else 1000000]*3)
                    self.assertFalse(any(e["kind"]=="clear_buffers" for e in events if e["elapsed_ns"]>=intents[0]["elapsed_ns"]))
                    self.assertEqual(commands.count(b"burn_speed 500000 10\r"),int(plan.endswith("500k")))
                    if plan.endswith("500k"):
                        transition=next(e for e in events if e["kind"]=="receive_speed_intent")
                        complete=next(e for e in events if e["kind"]=="receive_speed_complete")
                        self.assertLess(complete["elapsed_ns"],intents[0]["elapsed_ns"])
                        self.assertTrue(any(e["kind"]=="clear_buffers" and transition["elapsed_ns"]<e["elapsed_ns"]<complete["elapsed_ns"] for e in events))
                    corrupt=copy.deepcopy(summary);corrupt["reads"][0]["host_after"]["errors"]=True
                    with self.assertRaises(ValueError):observer.boot_receive_summary(corrupt,directory,Path(self.options["first"]).read_bytes(),plan)

    def test_short_crc_or_status_failure_stops_before_next_sample_and_preserves_failed_status(self):
        for plan in ("boot64k-1m","boot64k-500k"):
            for scenario in ("boot-short26","boot-crc-fail","boot-changed","host-error-after","host-after-fail"):
                with self.subTest(plan=plan,scenario=scenario):
                    out,result=self.exercise(scenario,plan=plan)
                    self.assertFalse(result["observation_passed"] or result["passed"])
                    self.assertTrue(result["resources_closed"] and result["application_recovery_passed"])
                    report=result["rom"]["observations"]["report"]
                    self.assertTrue(report["fallback_completed"])
                    self.assertFalse(report["normal_start_attempted"])
                    summary=result["rom"]["observations"]["summaries"]["boot_receive"]
                    self.assertEqual(len(summary["reads"]),1)
                    directory,events,commands=self.wire(out)
                    self.assertEqual(commands.count(b"burn_read 0x12010000 0x00010000\r"),1)
                    self.assertEqual(commands[-1],b"burn_read 0x12010000 0x00010000\r")
                    if scenario=="boot-short26":
                        self.assertEqual(summary["independent_streams"][0]["received_bytes"],65510)
                        self.assertFalse((directory/"boot-receive-0.bin").exists())
                    if scenario=="host-error-after":self.assertEqual(summary["reads"][0]["host_after"]["errors"],2)
                    if scenario=="host-after-fail":self.assertIn("host_after_error",summary["reads"][0])

    def test_speed_and_physical_or_native_precheck_failure_never_start_payload(self):
        for scenario in ("speed-fail","host-error-before","host-status-fail","ftab-alias-nonzero","ftab-alias-fail","lcpu-running"):
            out,result=self.exercise(scenario,plan="boot64k-500k")
            self.assertFalse(result["observation_passed"])
            _,events,commands=self.wire(out)
            self.assertFalse(any(e["kind"]=="boot_receive_read_intent" for e in events))
            self.assertFalse(any(c.startswith(b"burn_read") or c==b"burn_reset\r" for c in commands))
            self.assertTrue(result["resources_closed"])
            if scenario=="speed-fail":self.assertEqual(commands[-1],b"burn_speed 500000 10\r")

    def test_cancel_before_entry_and_blocked_driver_close_owned_resources(self):
        _,cancelled=self.exercise(plan="boot64k-500k",cancel_phase="prepare")
        self.assertFalse(cancelled["worker_started"] or cancelled["entry_attempted"])
        _,hung=self.exercise("hang-read",plan="boot64k-500k",rom_seconds=2,cancel_phase="return")
        self.assertTrue(hung["resources_closed"])
        self.assertFalse(hung["lease_retained"] or hung["observation_passed"])

if __name__=="__main__":unittest.main()
