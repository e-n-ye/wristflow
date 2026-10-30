from pathlib import Path
import re
import subprocess
import sys
import tempfile
import threading
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from runtime_export import ExportClient, Simulation, collect


class RuntimeExport(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        producer = ROOT / "artifacts/host-ui-build/product_runtime_record.exe"
        if not producer.exists():
            raise unittest.SkipTest("Build Product recorder host producer first")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "export.uart"
            subprocess.run([str(producer), str(path)], check=True, capture_output=True)
            cls.fixture = path.read_bytes()

    def run_collect(self, root, transport, **kwargs):
        return collect(root / "capture", transport, binding={"simulation": True},
                       identity_check=lambda: {"simulation": True}, locks=root / "locks", **kwargs)

    def test_bounded_read_only_roundtrip(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            peer = Simulation(self.fixture)
            report = self.run_collect(root, peer, seconds=10)
            self.assertTrue(report["passed"], report)
            self.assertTrue(report["failed_recording"])
            self.assertTrue(peer.closed and report["resources_closed"])
            self.assertEqual(len(peer.commands), 4)
            self.assertTrue(all(re.fullmatch(ExportClient.COMMAND_PATTERN, command) for command in peer.commands))
            self.assertFalse(report["recording_control"])
            self.assertTrue((root / "capture/records.json").exists())

    def test_cancel_before_open(self):
        with tempfile.TemporaryDirectory() as directory:
            peer = Simulation(self.fixture)
            cancel = threading.Event()
            cancel.set()
            report = self.run_collect(Path(directory), peer, seconds=10, cancel=cancel)
            self.assertFalse(report["passed"])
            self.assertEqual(report["outcome"], "cancelled")
            self.assertEqual(peer.commands, [])
            self.assertTrue(report["resources_closed"])

    def test_truncation_closes_without_retry(self):
        class Truncated(Simulation):
            def write(self, wire):
                result = super().write(wire)
                if b" summary " in wire:
                    self.queue = bytearray(re.sub(rb"\[record-summary-end\][^\n]*\n", b"", self.queue))
                return result
        with tempfile.TemporaryDirectory() as directory:
            peer = Truncated(self.fixture)
            report = self.run_collect(Path(directory), peer, seconds=1)
            self.assertFalse(report["passed"])
            self.assertEqual(report["outcome"], "timeout")
            self.assertTrue(report["resources_closed"])
            self.assertEqual(len(peer.commands), len(set(peer.commands)))

    def test_controls_not_allowed(self):
        for command in ("wf_record stop", "wf_record start", "reboot", "wf_time 1793348102", "wf_storage scan"):
            self.assertIsNone(re.fullmatch(ExportClient.COMMAND_PATTERN, command))


if __name__ == "__main__":
    unittest.main()
