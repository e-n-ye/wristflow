"""Behavioral counterexamples for the guarded host candidate, never hardware."""
import json
import os
import pathlib
import subprocess
import sys
import unittest
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from battery_lab import replay


class PolicyBoundaries(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.folder = ROOT / "artifacts/battery-policy-tests" / uuid.uuid4().hex
        cls.config = dict(retain_ms=30000, settle_ms=20000, settle_samples=3, max_sample_gap_ms=15000)
        cls.report = replay(ROOT / "tests/fixtures/battery-b2-20261021.json", cls.folder,
                            os.environ.get("CC", "gcc"), guarded=cls.config)
        cls.engine = cls.folder / "battery_replay.exe"

    def run_rows(self, rows, *, warm=(1, 0, 97, 10, 41430, 0, 0, 4143), config=None, rejected=False):
        settings = dict(self.config, **(config or {}))
        wire = [" ".join(map(str, warm))]
        wire += [" ".join(map(str, (index, *row))) for index, row in enumerate(rows, 1)]
        run = subprocess.run([str(self.engine)] + [str(settings[k]) for k in
            ("retain_ms", "settle_ms", "settle_samples", "max_sample_gap_ms")],
            input="\n".join(wire) + "\n", capture_output=True, text=True, timeout=5)
        if rejected:
            self.assertEqual(run.returncode, 2, run.stdout)
            self.assertIn("Rejected unordered", run.stderr)
        else:
            self.assertEqual(run.returncode, 0, run.stderr)
            values = [list(map(int, line.split())) for line in run.stdout.splitlines()]
            self.assertEqual(len(values), 4 * len(rows))
            return {policy: [line for line in values if line[1] == policy] for policy in range(4)}

    @staticmethod
    def fresh(time, sequence, mv=4143, charge=4, good=0, known=1, adc=None):
        return (time, time if adc is None else adc, sequence, mv, 1, known, good, charge)

    @staticmethod
    def missing(time, charge=4, good=0):
        return (time, 0, 0, 0, 0, 0, good, charge)

    def test_b2_baseline_still_matches_production_and_validity_is_not_invented(self):
        self.assertEqual(self.report["checked_rows"], 120)
        groups = json.loads((self.folder / "predictions.json").read_text(encoding="utf-8"))
        for name, rows in groups.items():
            self.assertEqual([r["percent_valid"] for r in rows], [r["percent_valid"] for r in groups["baseline"]], name)

    def test_retention_inclusive_bound_is_from_last_adc_not_first_missing_callback(self):
        for time, restored in ((30000, 1), (30001, 0)):
            with self.subTest(time=time):
                groups = self.run_rows([self.missing(14000), self.missing(29000), self.fresh(time, 11)])
                self.assertEqual(groups[1][-1][9], restored)
                self.assertEqual(groups[1][-1][3], 97 if restored else 98)
                self.assertEqual(groups[1][0][2], 0)

    def test_returning_same_adc_cannot_restore_or_count_as_new_support(self):
        rows = self.run_rows([self.missing(100), self.fresh(200, 10, adc=0)])
        self.assertEqual(rows[1][-1][9], 0)
        self.assertEqual(rows[3][-1][11], 0)

    def test_bad_or_stale_return_discards_saved_history(self):
        for bad in (self.fresh(1000, 11, mv=2999), self.fresh(16000, 11, adc=0),
                    (1000, 1000, 11, 4143, 0, 1, 0, 4)):
            with self.subTest(bad=bad):
                rows = self.run_rows([self.missing(100), bad, self.fresh(20000, 12)])
                self.assertEqual(rows[1][1][2], 0)
                self.assertEqual(rows[1][-1][3], 98)
                self.assertEqual(rows[1][-1][9], 0)

    def test_known_input_or_charging_transition_cancels_tentative_copy(self):
        for charge, good in ((2, 1), (3, 1), (5, 0), (0, 0), (4, 1)):
            with self.subTest(charge=charge, good=good):
                rows = self.run_rows([self.missing(100), self.missing(200, charge, good), self.fresh(1000, 11)])
                self.assertEqual(rows[1][-1][9], 0)
                self.assertEqual(rows[1][-1][3], 98)

    def test_fresh_unknown_mode_never_reuses_old_filter_when_mode_returns(self):
        rows = self.run_rows([self.fresh(1000, 11, known=0), self.fresh(2000, 12)])
        self.assertEqual(rows[1][0][2], 0)
        self.assertEqual(rows[1][-1][3], 98)

    def test_silent_long_gap_is_bounded_even_without_an_invalid_callback(self):
        rows = self.run_rows([self.fresh(30001, 11)])
        self.assertEqual(rows[0][-1][3], 97)
        self.assertEqual(rows[1][-1][3], 98)
        self.assertEqual(rows[1][-1][10], 1)

    def test_three_samples_and_elapsed_span_are_both_required(self):
        for times, final in (((1, 2, 3), 97), ((1, 10001, 20000), 97), ((1, 10001, 20001), 98)):
            with self.subTest(times=times):
                rows = self.run_rows([self.fresh(t, 11+i) for i, t in enumerate(times)])
                self.assertEqual(rows[2][-1][3], final)

    def test_cached_ui_rows_cannot_complete_settling(self):
        rows = self.run_rows([self.fresh(1, 11), self.fresh(10001, 12),
                              self.fresh(20001, 12, adc=10001)])
        self.assertEqual(rows[2][-1][3], 97)
        self.assertEqual(rows[2][-1][11], 0)

    def test_support_timeout_does_not_reseed_a_still_fresh_cached_adc(self):
        rows = self.run_rows([self.fresh(1, 11), self.fresh(7001, 11, adc=1), self.fresh(8001, 12)],
                             config={"max_sample_gap_ms": 6000})
        self.assertEqual([r[3] for r in rows[2]], [97, 97, 97])
        self.assertEqual([r[10] for r in rows[2]], [0, 0, 0])
        self.assertEqual([r[11] for r in rows[2]], [0, 0, 0])

    def test_support_gap_is_inclusive_and_expiry_restarts_dwell(self):
        for gap, settled in ((15000, 1), (15001, 0)):
            with self.subTest(gap=gap):
                rows = self.run_rows([self.fresh(1, 11), self.fresh(10001, 12), self.fresh(10001+gap, 13)])
                self.assertEqual(rows[2][-1][11], settled)
                if gap == 15001:
                    self.assertEqual(rows[2][-1][10], 1)  # Fresh seed, not false convergence.

    def test_reset_cannot_bypass_same_adc_mode_wait(self):
        rows = self.run_rows([self.fresh(1, 11, charge=2, good=1),
                              self.fresh(1000, 11, adc=1), self.fresh(2000, 12)])
        for policy in (0, 1, 2, 3):
            self.assertEqual(rows[policy][1][2], 0)
            self.assertEqual(rows[policy][1][8], 9)
            self.assertEqual(rows[policy][2][2], 1)

    def test_invalid_unknown_and_candidate_change_restart_settling(self):
        for middle in (self.missing(15001), self.fresh(15001, 13, known=0), self.fresh(15001, 13, mv=4050)):
            with self.subTest(middle=middle):
                rows = self.run_rows([self.fresh(1, 11), self.fresh(10001, 12), middle, self.fresh(20001, 14)])
                self.assertEqual(rows[2][-1][11], 0)

    def test_downward_one_point_can_settle_without_forcing_discharge_monotonicity(self):
        rows = self.run_rows([self.fresh(t, 11+i) for i, t in enumerate((1, 10001, 20001))],
                             warm=(1, 0, 99, 10, 41430, 0, 0, 4143))
        self.assertEqual(rows[2][-1][3], 98)
        self.assertEqual(rows[2][-1][11], 1)

    def test_full_is_still100_only_with_valid_adc_and_charging_does_not_settle(self):
        rows = self.run_rows([self.fresh(1, 11, charge=3, good=1), self.missing(2, charge=3, good=1)])
        self.assertEqual(rows[3][0][3], 100)
        self.assertEqual(rows[3][1][2], 0)
        for policy in (1, 2, 3):self.assertTrue(all(r[11] == 0 for r in rows[policy]))
        rows = self.run_rows([self.fresh(t, 11+i, charge=2, good=1) for i, t in enumerate((1, 10001, 20001))],
                             warm=(1, 1, 97, 10, 41800, 0, 0, 4180))
        self.assertTrue(all(r[11] == 0 for r in rows[3]))

    def test_old_adc_reappearance_and_identity_mutation_are_rejected(self):
        for bad in (self.fresh(20001, 10), self.fresh(20001, 12),
                    self.fresh(20001, 12, mv=4144, adc=10001)):
            with self.subTest(bad=bad):
                self.run_rows([self.fresh(1, 11), self.fresh(10001, 12), bad], rejected=True)

    def test_backwards_observation_or_adc_is_rejected(self):
        for bad in (self.fresh(999, 12), self.fresh(2000, 12, adc=999)):
            with self.subTest(bad=bad):
                self.run_rows([self.fresh(1000, 11), bad], rejected=True)

    def test_single_clock_and_sequence_wrap_preserve_modular_boundaries(self):
        start = 0xfffffff0
        rows = self.run_rows([self.fresh((start+d)&0xffffffff, s) for d,s in
                             ((1, 0xfffffffe), (10001, 0xffffffff), (20001, 1))],
                             warm=(1, 0, 97, 0xfffffffd, 41430, start, start, 4143))
        self.assertEqual(rows[2][-1][3], 98)
        self.assertEqual(rows[2][-1][11], 1)
        rows = self.run_rows([self.missing((start+1)&0xffffffff), self.fresh((start+30000)&0xffffffff, 1)],
                             warm=(1, 0, 97, 0xffffffff, 41430, start, start, 4143))
        self.assertEqual(rows[1][-1][9], 1)

    def test_dwell_uses_adc_completion_not_delayed_ui_observation(self):
        rows = self.run_rows([self.fresh(1, 11), self.fresh(10001, 12), self.fresh(20001, 13, adc=19001)])
        self.assertEqual(rows[2][-1][11], 0)

    def test_parameter_validation_rejects_zero_negative_and_unbounded_values(self):
        for args in ((0, 1, 3, 1), (-1, 1, 3, 1), (1, 1, 1, 1), (900001, 1, 3, 1)):
            with self.subTest(args=args):
                r = subprocess.run([str(self.engine), *map(str, args)], input="", capture_output=True, timeout=5)
                self.assertEqual(r.returncode, 2)


if __name__ == "__main__":
    unittest.main()
