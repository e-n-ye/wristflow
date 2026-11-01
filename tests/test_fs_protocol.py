import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from fs_protocol import parse_fs, simulated_fs
import device_snapshot as lab
import runtime_export


class FsTests(unittest.TestCase):
    def test_success_retains_observation_limits(self):
        report = parse_fs(simulated_fs())
        self.assertTrue(report["observed_all_ff"])
        self.assertFalse(report["ownership_verified"] or report["physical_capacity_verified"] or report["format_or_erase_authorized"])

    def test_corruption_duplicate_truncation_and_reordering(self):
        lines = simulated_fs()
        bad = [lines[:-1], lines + [lines[-1]], lines + ["[fs-pass]"], [lines[1], lines[0], *lines[2:]]]
        for old, new in (("bytes=4194304", "bytes=4194303"), ("table_match=1", "table_match=0"),
                         ("result=0", "result=2"), ("stable=1", "stable=0"), ("guard=0", "guard=8"),
                         ("ff=4194304", "ff=4294967296"), ("ms=100", "ms=12000"),
                         ("end=12da0000", "end=12da0001"), ("dual=0", "dual=32"),
                         ("rdid=001840ef", "rdid=00000000"), ("calls=4096", "calls=4095"),
                         ("edge=1", "edge=0"), ("ffffffff", "fffffffe")):
            bad.append([line.replace(old, new) for line in lines])
        bad.append([line.replace("nonff=0", "nonff=1") if "i=1" in line else line for line in lines])
        for case in bad:
            with self.subTest(case=case), self.assertRaises(ValueError): parse_fs(case)

    def test_fragmented_capture_and_failed_reply_close_without_retry(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for failed in (False, True):
                peer = lab.Simulation()
                original = peer.write
                def write(wire):
                    n = original(wire)
                    if failed: peer.queue[:] = bytes(peer.queue).replace(b"stable=1", b"stable=0")
                    return n
                peer.write = write
                report = lab.collect(root / str(failed), peer, binding={"simulation": True},
                    identity_check=lambda: {}, locks=root / "locks", queries=("fs",))
                self.assertEqual(report["passed"], not failed)
                self.assertTrue(report["resources_closed"])
                self.assertEqual(peer.commands, ["wf_fs scan"])

    def test_no_rtc_mixed_scan_or_wrong_diagnostic_build(self):
        with self.assertRaises(ValueError): lab.validate(("fs",), True, True, 60)
        with self.assertRaises(ValueError): lab.validate(("fs", "storage"), False, False, 60)
        for invalid in (False, "true", 1, None):
            with self.assertRaises(ValueError): lab.validate_build({"fs_probe": invalid}, ("fs",))
        with self.assertRaises(ValueError): lab.validate_build({"fs_probe": True, "storage_probe": True}, ("fs",))
        lab.validate_build({"fs_probe": True}, ("fs",))

    def test_real_callers_refuse_wrong_build_before_open(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = root / "result.json"
            binding = {"simulation": False, "build": {"build_result": {"path": str(result)}}}
            for api, flags, kwargs in ((lab.collect, {}, {"queries": ("fs",)}),
                                      (runtime_export.collect, {"fs_probe": True}, {})):
                result.write_text(json.dumps(flags), encoding="utf-8")
                peer = lab.Simulation()
                with self.assertRaises(ValueError):
                    api(root / "run", peer, binding=binding, identity_check=lambda: {}, locks=root / "locks", **kwargs)
                self.assertFalse(peer.opened or peer.commands or (root / "run").exists())


if __name__ == "__main__":
    unittest.main()
