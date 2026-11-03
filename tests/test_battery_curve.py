"""Check compiled production lookup against the approved floating-point fit."""
import bisect
import json
import os
from pathlib import Path
import subprocess
import unittest
import uuid

ROOT = Path(__file__).resolve().parents[1]


class UsableCurveTests(unittest.TestCase):
    def test_all_adc_voltages_follow_approved_piecewise_curve_in_both_modes(self):
        candidate = json.loads((ROOT / "docs/evidence/2026-11-02/battery-curve-candidate.json").read_text(encoding="utf-8"))
        nodes = candidate["nodes"]
        voltages = [p["mv"] for p in nodes]
        self.assertEqual(candidate["join_mv"], 3600)
        self.assertEqual(voltages, list(range(3300, 4141, 10)) + [4142])
        out = ROOT / "artifacts/battery-curve-tests" / uuid.uuid4().hex
        out.mkdir(parents=True, exist_ok=False)
        helper = out / "lookup.c"
        helper.write_text('''#include "battery_estimate.h"
#include <stdio.h>
int main(void) {
    for(unsigned mv=3000;mv<=4250;++mv)
        printf("%u,%u,%u\\n",mv,wristflow_battery_reference_percent(mv,false),
            wristflow_battery_reference_percent(mv,true));
    return 0;
}
''', encoding="utf-8")
        engine = out / "lookup.exe"
        build = subprocess.run([os.environ.get("CC", "gcc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-I", str(ROOT / "core"), str(helper), str(ROOT / "core/battery_estimate.c"), "-o", str(engine)],
            capture_output=True, text=True, timeout=60)
        (out / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
        self.assertEqual(build.returncode, 0, build.stderr)
        result = subprocess.run([str(engine)], check=True, capture_output=True, text=True, timeout=30)
        (out / "lookup.csv").write_text(result.stdout, encoding="utf-8")
        rows = [tuple(map(int, line.split(","))) for line in result.stdout.splitlines()]
        self.assertEqual([r[0] for r in rows], list(range(3000, 4251)))
        previous = 0
        for mv, actual, charging in rows:
            if mv <= voltages[0]:
                expected = 0.0
            elif mv >= voltages[-1]:
                expected = 100.0
            else:
                i = bisect.bisect_left(voltages, mv)
                low, high = nodes[i - 1:i + 1]
                expected = low["percent"] + (high["percent"] - low["percent"]) * (mv-low["mv"]) / (high["mv"]-low["mv"])
            with self.subTest(mv=mv):
                self.assertEqual(actual, charging)
                self.assertGreaterEqual(actual, previous)
                # 0.005pp node quantization, then the existing integer floor.
                self.assertLessEqual(actual, expected + 0.005001)
                self.assertGreater(actual, expected - 1.005001)
            previous = actual


if __name__ == "__main__":
    unittest.main()
