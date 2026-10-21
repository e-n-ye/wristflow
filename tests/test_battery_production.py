"""Current production fixes real stalls; historical inputs remain unchanged."""
import hashlib
import json
import os
import pathlib
import sys
import unittest
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from battery_lab import production_compare


class ProductionReplayTests(unittest.TestCase):
    def compare(self, name):
        folder = ROOT / "artifacts/battery-production-tests" / uuid.uuid4().hex
        report = production_compare(ROOT / "tests/fixtures" / name, folder, os.environ.get("CC", "gcc"))
        groups = json.loads((folder / "predictions.json").read_text(encoding="utf-8"))
        self.assertFalse(report["hardware_verified"])
        self.assertNotIn("percent", report["unchanged_fields"])
        self.assertTrue(all(r["recorded"] != r["current"] for r in report["changed_percent_rows"]))
        return report, groups

    def test_first_trace_recovers_before_sleep_without_changing_validity_or_curve(self):
        report, groups = self.compare("battery-b2-20261021.json")
        self.assertEqual(report["historical_checked_rows"], 120)
        self.assertEqual(groups["recorded"][65]["percent"], 97)
        self.assertEqual(groups["current"][65]["percent"], 98)
        self.assertFalse(groups["current"][105]["percent_valid"])
        self.assertEqual(groups["current"][108]["percent"], 98)

    def test_second_trace_recovers_without_sleep_and_only_display_changes(self):
        report, groups = self.compare("battery-guided-20261021.json")
        self.assertEqual(report["historical_checked_rows"], 109)
        self.assertEqual(groups["current"][65]["percent"], 98)
        self.assertEqual(groups["recorded"][65]["percent"], 97)
        for old, new in zip(groups["recorded"], groups["current"]):
            for key in ("filter", "candidate", "percent_valid", "filter_valid", "mode"):
                self.assertEqual(old[key], new[key])
        self.assertTrue(any(r["surface"] == 5 for r in report["changed_percent_rows"]))

    def test_frozen_estimator_matches_accepted_old_source_content(self):
        # Source provenance at17a9de5; normalize Git checkout line endings only.
        expected = {
            "battery_estimate.c": "112e143c8fe843b78245eb12a2dce27709a414002e8d9788a852a60ed060b697",
            "battery_estimate.h": "a14b3b53bd528215547175389b6ee4433b430166ccef43a11b35810138dcb32a"}
        for name, checksum in expected.items():
            contents = (ROOT / "scripts/battery_baseline_20261021" / name).read_bytes().replace(b"\r\n", b"\n")
            self.assertEqual(hashlib.sha256(contents).hexdigest(), checksum)


if __name__ == "__main__":
    unittest.main()
