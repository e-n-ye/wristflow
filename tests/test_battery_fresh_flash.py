"""Fresh firmware entry guards; all UART and flash records are simulated."""
import copy
import hashlib
import json
import pathlib
import sys
import threading
import unittest
import uuid
from unittest.mock import patch

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from battery_collect import ProtocolError, SessionChanged, SimulatedDevice
from battery_experiment import FreshFlashExperiment, prepare_fresh_binding
from battery_lab import Journal, load_trace


class FreshFlashTests(unittest.TestCase):
    def setUp(self):
        self.folder = ROOT / "artifacts/battery-fresh-tests" / uuid.uuid4().hex
        self.folder.mkdir(parents=True)
        fixture = load_trace(ROOT / "tests/fixtures/battery-b2-20261021.json")
        self.device = SimulatedDevice(fixture, chunk=8192)
        self.device.status.update(session=0, state=0, count=0, start=0, duration=0)
        original = self.device.write
        def write(wire):
            if wire.startswith(b"wf_battery start "):
                self.device.status["start"] = self.device.status["now"]
            return original(wire)
        self.device.write = write
        self.journal = Journal(self.folder / "run", binding={"test_only": True})
        self.collector = FreshFlashExperiment(self.journal, self.device, threading.Event(),
                                              self.folder / "locks", None, timeout=.02)
        names = ["main.bin", "bootloader.bin", "ftab.bin"] + [f"artifact{i}" for i in range(26)]
        artifacts = []
        for name in names:
            path = self.folder / name
            path.write_bytes(name.encode())
            artifacts.append({"path": str(path), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
        self.build = {"example": "product", "exit_code": 0, "artifact_validation_passed": True,
                      "firmware_path": artifacts[0]["path"], "artifacts": artifacts}
        addresses = ["0x12020000", "0x12010000", "0x12000000"]
        self.flash = {"example": "product", "exit_code": 0, "board": "sf32lb52-lchspi-ulp", "port": "COM5",
                      "images": [{**item, "name": names[i], "address": addresses[i],
                                  "bytes": pathlib.Path(item["path"]).stat().st_size}
                                 for i, item in enumerate(artifacts[:3])]}

    def binding(self):
        result, flash = self.folder / "build.json", self.folder / "flash.json"
        result.write_text(json.dumps(self.build), encoding="utf-8")
        flash.write_text(json.dumps(self.flash), encoding="utf-8")
        with patch("battery_experiment.port_identity", return_value={"device": "COM5", "location": "1-1.1"}):
            return prepare_fresh_binding(result, flash, "COM5", "1-1.1")

    def test_accepts_bound_flash_but_does_not_claim_firmware_attestation(self):
        binding = self.binding()
        self.assertEqual(binding["firmware_sha256"], self.build["artifacts"][0]["sha256"])
        self.assertFalse(binding["device_identity_verified"])

    def test_failed_or_wrong_port_flash_rejected(self):
        for key, value in (("exit_code", 1), ("port", "COM7")):
            with self.subTest(key=key):
                old = copy.deepcopy(self.flash)
                self.flash[key] = value
                with self.assertRaises(ValueError): self.binding()
                self.flash = old

    def test_changed_artifact_and_duplicate_manifest_rejected(self):
        pathlib.Path(self.build["artifacts"][-1]["path"]).write_bytes(b"changed")
        with self.assertRaises(ValueError): self.binding()
        self.build["artifacts"][-1] = self.build["artifacts"][0]
        with self.assertRaises(ValueError): self.binding()

    def test_wrong_image_address_or_hash_rejected(self):
        for key, value in (("address", "0x12000000"), ("sha256", "0" * 64)):
            with self.subTest(key=key):
                old = copy.deepcopy(self.flash)
                self.flash["images"][0][key] = value
                with self.assertRaises(ValueError): self.binding()
                self.flash = old

    def test_empty_prestate_is_saved_before_exactly_one_start(self):
        with self.collector:
            saved = self.collector.protect()
            self.assertTrue(saved["fresh_after_flash"])
            self.collector.arm()
            with self.assertRaises(ProtocolError): self.collector.arm()
        self.assertEqual(self.device.commands.count("wf_battery start 900"), 1)
        self.assertNotIn("wf_battery stop", self.device.commands)
        self.assertTrue(self.collector.cleanup_confirmed)

    def test_nonempty_or_previously_used_recorder_never_overwritten(self):
        for key, value in (("session", 4), ("count", 1), ("state", 1)):
            with self.subTest(key=key):
                self.device.status[key] = value
                collector = FreshFlashExperiment(Journal(self.folder / key, binding={"test_only": True}),
                    self.device, threading.Event(), self.folder / "locks", None, timeout=.02)
                with collector:
                    with self.assertRaises(SessionChanged): collector.protect()
                self.device.status[key] = 0
        self.assertFalse(any(c.startswith("wf_battery start") for c in self.device.commands))

    def test_empty_prestate_changed_before_start_is_rejected(self):
        with self.collector:
            self.collector.protect()
            self.device.status.update(session=1, state=1)
            with self.assertRaises(SessionChanged): self.collector.arm()
        self.assertNotIn("wf_battery start 900", self.device.commands)

    def test_changed_saved_empty_prestate_cannot_arm(self):
        with self.collector:
            self.collector.protect()
            (self.journal.directory / "protected.json").write_text("{}", encoding="utf-8")
            with self.assertRaises(ProtocolError): self.collector.arm()
        self.assertNotIn("wf_battery start 900", self.device.commands)


if __name__ == "__main__":
    unittest.main()
