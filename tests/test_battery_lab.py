"""Meaningful evidence-integrity and interrupted-workflow checks; no hardware."""
import copy
import importlib.util
import json
import os
import pathlib
import subprocess
import unittest
import uuid
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("battery_lab", ROOT / "scripts/battery_lab.py")
lab = importlib.util.module_from_spec(spec)
spec.loader.exec_module(lab)
FIXTURE = ROOT / "tests/fixtures/battery-b2-20261021.json"


class BatteryLabTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = ROOT / "artifacts/battery-lab-tests" / uuid.uuid4().hex
        cls.directory.mkdir(parents=True)
        cls.report = lab.replay(FIXTURE, cls.directory / "replay", os.environ.get("CC", "gcc"))
        cls.report_path = cls.directory / "replay/replay.json"

    def rejected(self, mutate):
        data = copy.deepcopy(json.loads(FIXTURE.read_text(encoding="utf-8")))
        mutate(data)
        path = self.directory / "bad.json"
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaises(ValueError):
            lab.load_trace(path)

    def test_production_c_matches_recorded_states(self):
        self.assertEqual(self.report["checked_rows"], 120)
        self.assertEqual(self.report["unique_adc"], 98)
        self.assertEqual(self.report["policies"]["baseline"]["changed_rows"], [])
        self.assertEqual(self.report["unplug_delay_ms"], 58988)
        self.assertEqual(self.report["confirmed_battery_span_ms"], 418910)

    def test_experimental_convergence_and_retention_are_separate(self):
        points = {point["index"]: point for point in self.report["points"]}
        self.assertEqual(points[86]["baseline"], 97)
        self.assertEqual(points[86]["settle_60s"], 98)
        self.assertEqual(points[108]["baseline"], 98)
        self.assertEqual(points[108]["retain_30s"], 97)
        invalid = self.report["policies"]["baseline"]["invalid_rows"]
        for policy in lab.POLICIES:
            self.assertEqual(self.report["policies"][policy]["invalid_rows"], invalid)

    def test_missing_count_rejected(self):
        self.rejected(lambda data: data["records"].pop())

    def test_mixed_session_rejected(self):
        self.rejected(lambda data: data["records"][5].update(session=4))

    def test_duplicate_index_rejected(self):
        self.rejected(lambda data: data["records"][5].update(index=4))

    def test_adc_age_corruption_rejected(self):
        self.rejected(lambda data: data["records"][5].update(age=999))

    def test_observation_after_deadline_rejected(self):
        self.rejected(lambda data: data["records"][5].update(obs=data["status"]["start"]+480000))

    def test_adc_identity_mutation_rejected(self):
        self.rejected(lambda data: data["records"][0].update(seq=33))

    def test_unfinished_progress_preserved_and_never_marked_pass(self):
        journal = lab.Journal(self.directory / "unfinished", self.report_path)
        journal.event("prompt", phase="boundary")
        with (journal.directory / "events.jsonl").open("a", encoding="utf-8") as stream:
            stream.write('{"sequence":')
        result = lab.read_progress(journal.directory)
        self.assertEqual(result["state"], "incomplete")
        self.assertTrue(result["partial_tail"])
        self.assertEqual(result["last_event"]["phase"], "boundary")
        with self.assertRaises(FileExistsError):
            lab.Journal(journal.directory, self.report_path)

    def test_changed_replay_evidence_rejected(self):
        journal = lab.Journal(self.directory / "changed", self.report_path)
        (journal.directory / "replay.json").write_text("{}", encoding="utf-8")
        with self.assertRaises(ValueError):
            lab.read_progress(journal.directory)

    def test_retention_deadline_and_charge_guard(self):
        engine = self.directory / "replay/battery_replay.exe"
        for end_ms, charge, good, retained in ((30001, 4, 0, True),
                                              (30002, 4, 0, False),
                                              (2000, 2, 1, False)):
            with self.subTest(end_ms=end_ms, charge=charge):
                wire = ("1 0 97 1 41430\n"
                        "1 1 0 0 0 0 0 0 4\n"
                        f"2 {end_ms} {end_ms} 2 4143 1 1 {good} {charge}\n")
                result = subprocess.run([str(engine)], input=wire, text=True,
                                        capture_output=True, check=True)
                values = [list(map(int, line.split())) for line in result.stdout.splitlines()]
                self.assertTrue(all(row[2] == 0 for row in values[:4]))
                baseline, retention = values[4], values[5]
                self.assertEqual(retention[3], 97 if retained else baseline[3])

    def test_openhtf_success_cancel_timeout(self):
        for name, responses, passed, expected in (
                ("success", ["continue"]*3, True, "confirmed"),
                ("cancel", ["cancel"], False, "cancelled"),
                ("timeout", ["timeout"], False, "timeout")):
            with self.subTest(name=name):
                output = self.directory / name
                self.assertEqual(lab.guide(self.report_path, output, 0, 1, responses), passed)
                events = [json.loads(line) for line in (output / "events.jsonl").read_text(encoding="utf-8").splitlines()]
                self.assertIn(expected, [event["kind"] for event in events])
                self.assertEqual(events[-1]["passed"], passed)
                self.assertTrue((output / "openhtf.json").exists())

    def test_output_failure_cannot_pass(self):
        from openhtf.output.callbacks import json_factory
        output = self.directory / "output-failure"

        def fail(record):
            raise OSError("injected output failure")

        with patch.object(json_factory, "OutputToJSON", return_value=fail):
            with self.assertRaisesRegex(RuntimeError, "not successfully persisted"):
                lab.guide(self.report_path, output, 0, 1, ["continue"]*3)
        events = [json.loads(line) for line in (output / "events.jsonl").read_text(encoding="utf-8").splitlines()]
        self.assertIn("output_error", [event["kind"] for event in events])
        self.assertEqual(events[-1]["kind"], "interrupted")
        self.assertFalse(any(event.get("passed") for event in events))


if __name__ == "__main__":
    unittest.main()
