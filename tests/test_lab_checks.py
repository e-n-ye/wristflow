"""Negative-path checks for host evidence validation; no devices or builds."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import lab_check as check
import serial_lab as lab
from lab_evidence import file_record, write_json


class CheckTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.build = self.root / "build"
        self.build.mkdir()
        self.source = self.root / "source.c"
        self.source.write_bytes(b"/* original */\r\n")
        self.lock = {"sdk": {"commit": "a"*40}, "submodules": {"module": "b"*40}, "board": "sf32lb52-lchspi-ulp"}
        write_json(self.root / "sdk.lock.json", self.lock)
        for name in ("main.elf", ".config", "rtconfig.h", *check.ADDRESSES):
            (self.build / name).write_bytes(name.encode())
        self.manifest = {"chip": "SF32LB52", "memory": "NOR", "write_flash": {"verify": True,
                         "files": [{"path": name, "address": address} for name, address in check.ADDRESSES.items()]}}
        write_json(self.build / "sftool_param.json", self.manifest)
        self.value = {"exit_code": 0, "artifact_validation_passed": True, "sdk_commit": "a"*40, "sdk_changes": [],
                      "submodules": [" " + "b"*40 + " module (v1)"], "build_directory": str(self.build), "example": "product",
                      "firmware_path": str(self.build / "main.bin"), "project_sources": [file_record(self.source)],
                      "artifacts": [file_record(p) for p in self.build.iterdir()]}
        self.result = self.root / "result.json"

    def run_build(self, value=None, **kwargs):
        write_json(self.result, value or self.value)
        with patch.object(check, "check_sdk", return_value={"clean": True}):
            return check.check_build(self.result, root=self.root, **kwargs)

    def init_git(self):
        def git(*args):
            return subprocess.check_output(["git", "-C", str(self.root), *args], stderr=subprocess.STDOUT)
        git("init", "-q")
        git("config", "user.name", "Host Test")
        git("config", "user.email", "test@example.invalid")
        git("config", "core.autocrlf", "false")
        git("add", ".")
        git("commit", "-qm", "fixture")
        return git

    def test_build_and_flash_are_local_records_only(self):
        report = self.run_build()
        flash = dict(exit_code=0, example="product", board=self.lock["board"], port="COM5", images=report["images"])
        path = self.root / "flash.json"
        write_json(path, flash)
        result = self.run_build(flash=path)
        self.assertFalse(result["hardware_verified"])
        self.assertFalse(result["device_identity_verified"])
        flash["images"][0]["address"] = "0x0"
        write_json(path, flash)
        with self.assertRaisesRegex(ValueError, "images differ"):
            self.run_build(flash=path)

    def test_failed_empty_duplicate_and_missing_manifests_fail(self):
        cases = []
        for key, value in (("exit_code", 1), ("artifact_validation_passed", False), ("project_sources", []), ("artifacts", [])):
            changed = copy.deepcopy(self.value)
            changed[key] = value
            cases.append(changed)
        changed = copy.deepcopy(self.value)
        changed["artifacts"].append(changed["artifacts"][0])
        cases.append(changed)
        changed = copy.deepcopy(self.value)
        changed["artifacts"] = [a for a in changed["artifacts"] if Path(a["path"]).name != "main.elf"]
        cases.append(changed)
        for value in cases:
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.run_build(value)

    def test_artifact_change_fails_even_if_size_and_mtime_preserved(self):
        (self.build / "main.bin").write_bytes(b"xxxxxxxx")
        with self.assertRaisesRegex(ValueError, "Evidence changed"):
            self.run_build()

    def test_crlf_requires_matching_original_snapshot_and_is_source_only(self):
        archive = self.root / "sources.zip"
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("source.c", self.source.read_bytes())
        self.source.write_bytes(b"/* original */\n")
        with self.assertRaisesRegex(ValueError, "strict bytes"):
            self.run_build()
        self.assertEqual(self.run_build(source_archive=archive)["line_endings_only"], [str(self.source)])
        self.source.write_bytes(b"/* content changed */\n")
        with self.assertRaisesRegex(ValueError, "content changed"):
            self.run_build(source_archive=archive)
        with zipfile.ZipFile(archive, "w") as output:
            output.writestr("source.c", b"/* content changed */\r\n")
        with self.assertRaisesRegex(ValueError, "snapshot does not match"):
            self.run_build(source_archive=archive)

    def archived_fixture(self):
        # Build.ps1 records source hashes without a size; artifacts include size.
        self.value["project_sources"][0].pop("bytes", None)
        archive = self.root / "build.zip"
        with zipfile.ZipFile(archive, "w") as output:
            for item in self.value["project_sources"] + self.value["artifacts"]:
                path = Path(item["path"])
                output.write(path, path.relative_to(self.root).as_posix())
        images = [dict(item, name=Path(item["path"]).name,
                       address=check.ADDRESSES[Path(item["path"]).name])
                  for item in self.value["artifacts"] if Path(item["path"]).name in check.ADDRESSES]
        flash = self.root / "flash.json"
        write_json(flash, dict(exit_code=0, example="product", board=self.lock["board"], port="COM5", images=images))
        return archive, flash

    def test_archive_verifies_old_bytes_with_explicit_original_root(self):
        archive, flash = self.archived_fixture()
        self.source.write_bytes(b"new source")
        (self.build / "main.bin").write_bytes(b"new firmware")
        report = self.run_build(build_archive=archive, recorded_root=self.root, flash=flash)
        self.assertEqual(report["source_policy"], "archived_build")
        self.assertFalse(report["current_source_verified"] or report["device_identity_verified"])
        self.assertEqual(report["recorded_sources"] + report["artifacts"], [])
        self.assertEqual(len(report["archived_artifacts"]), len(self.value["artifacts"]))
        with self.assertRaises(ValueError):
            self.run_build()
        other_root = self.root / "new-checkout"
        other_root.mkdir()
        write_json(other_root / "sdk.lock.json", self.lock)
        with patch.object(check, "check_sdk", return_value={}):
            mapped = check.check_build(self.result, root=other_root, build_archive=archive,
                                       recorded_root=self.root, flash=flash)
        self.assertEqual(mapped["images"], report["images"])

    def test_archive_requires_complete_unique_entries_and_hashes(self):
        archive, flash = self.archived_fixture()
        with zipfile.ZipFile(archive) as original:
            entries = {name: original.read(name) for name in original.namelist()}
        for kind in ("missing", "duplicate", "changed", "wrong-size", "outside"):
            value = copy.deepcopy(self.value)
            with zipfile.ZipFile(archive, "w") as output:
                for name, raw in entries.items():
                    if kind == "missing" and name == "source.c":
                        continue
                    output.writestr(name, b"tampered" if kind == "changed" and name == "source.c" else raw)
                if kind == "duplicate":
                    import warnings
                    with warnings.catch_warnings():
                        warnings.simplefilter("ignore", UserWarning)
                        output.writestr("source.c", entries["source.c"])
            if kind == "wrong-size":
                value["artifacts"][0]["bytes"] += 1
            if kind == "outside":
                value["project_sources"][0]["path"] = str(self.root.parent / "source.c")
            with self.subTest(kind=kind), self.assertRaises(ValueError):
                self.run_build(value, build_archive=archive, recorded_root=self.root, flash=flash)

    def test_archive_policy_and_flash_mismatch_are_rejected(self):
        archive, flash = self.archived_fixture()
        for kwargs in ({"build_archive": archive}, {"recorded_root": self.root},
                       {"build_archive": archive, "recorded_root": self.root, "source_archive": archive}):
            with self.assertRaises(ValueError):
                self.run_build(**kwargs)
        value = check.load(flash)
        value["images"][0]["address"] = "0x0"
        write_json(flash, value)
        with self.assertRaisesRegex(ValueError, "images differ"):
            self.run_build(build_archive=archive, recorded_root=self.root, flash=flash)

    def test_archived_binding_rechecks_zip_instead_of_overwritten_files(self):
        archive, flash = self.archived_fixture()
        report = self.run_build(build_archive=archive, recorded_root=self.root, flash=flash)
        self.source.write_bytes(b"new version")
        (self.build / "main.bin").write_bytes(b"new image")
        with patch("battery_device.port_identity", return_value={"device": "COM5"}), patch.object(check, "check_sdk"):
            binding, guard = self.binding(report)
            self.assertFalse(binding["build"]["current_source_verified"])
            guard()
            archive.write_bytes(archive.read_bytes() + b"changed")
            with self.assertRaisesRegex(ValueError, "Evidence changed"):
                guard()

    def test_duplicate_images_or_path_escape_fail(self):
        for entry in (self.manifest["write_flash"]["files"][0], {"path": "../main.bin", "address": "0x12020000"}):
            value = copy.deepcopy(self.manifest)
            value["write_flash"]["files"][1] = entry
            write_json(self.build / "sftool_param.json", value)
            self.value["artifacts"] = [file_record(p) for p in self.build.iterdir()]
            with self.assertRaises(ValueError):
                self.run_build()

    def test_sdk_record_and_current_lock_mismatch_fail(self):
        self.value["submodules"] = ["+" + "b"*40 + " module (v1)"]
        with self.assertRaisesRegex(ValueError, "Build submodule"):
            self.run_build()
        with patch.object(check, "git", return_value=b"wrong\n"), self.assertRaisesRegex(ValueError, "SDK HEAD"):
            check.check_sdk(self.root, self.lock)

    def test_explicit_evidence_section_never_traverses_historical_hashes(self):
        path = self.root / "evidence.json"
        write_json(path, {"files": [file_record(self.source, self.root)], "historical": {"path": "missing", "sha256": "invalid"}})
        self.assertEqual(len(check.check_files(path, root=self.root)["files"]), 1)
        self.source.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "Evidence changed"):
            check.check_files(path, root=self.root)
        write_json(path, {"files": []})
        with self.assertRaisesRegex(ValueError, "empty"):
            check.check_files(path, root=self.root)

    def test_docs_added_links_staged_untracked_and_unrelated_history(self):
        doc = self.root / "doc.md"
        doc.write_text("# Existing\n[old broken](absent.md)\n", encoding="utf-8", newline="\n")
        git = self.init_git()
        doc.write_text(doc.read_text() + "\n[ok](new.md#重复标题-1)\n", encoding="utf-8", newline="\n")
        git("add", "doc.md")
        (self.root / "new.md").write_text("# 重复标题\n# 重复标题\n", encoding="utf-8")
        result = check.check_docs(root=self.root)
        self.assertEqual(result["added_local_links"], 1)
        self.assertEqual({p["path"] for p in result["changed_text"]}, {"doc.md", "new.md"})
        (self.root / "new.md").write_text("# Different\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "Missing anchor"):
            check.check_docs(root=self.root)

    def test_docs_references_fences_explicit_anchors_and_encoded_paths(self):
        self.init_git()
        (self.root / "new.md").write_text("# New\n```md\n[x](missing.md)\n```\n[x][target]\n[target]: <target%20doc.md#real>\n", encoding="utf-8")
        (self.root / "target doc.md").write_text("<a id='real'></a>\n# Title\n", encoding="utf-8")
        self.assertEqual(check.check_docs(root=self.root)["added_local_links"], 2)
        (self.root / "target doc.md").unlink()
        with self.assertRaisesRegex(ValueError, "Missing link"):
            check.check_docs(root=self.root)

    def test_docs_scope_and_changed_encoding_fail_closed(self):
        self.init_git()
        with self.assertRaisesRegex(ValueError, "select changed"):
            check.check_docs(root=self.root, paths=[self.source])
        (self.root / "changed.md").write_bytes(b"\xff")
        with self.assertRaises(UnicodeDecodeError):
            check.check_docs(root=self.root)

    def test_markdown_anchor_visible_heading_and_fenced_examples(self):
        text = "## Use `foo`\n## [Guide](guide.md)\n```html\n<a id='fake'></a>\n```\n`<a id='inline'></a>`\n"
        found = check.anchors(text)
        self.assertIn("use-foo", found)
        self.assertIn("guide", found)
        self.assertNotIn("fake", found)
        self.assertNotIn("inline", found)

    def binding(self, report):
        with patch.object(check, "check_build", return_value=report), patch.object(check, "load", return_value=self.lock):
            return lab.prepare_capture(self.result, self.root / "flash.json",
                                       next(i["sha256"] for i in report["images"] if i["name"] == "main.bin"),
                                       "COM5", "1-1.1", True)

    def test_capture_requires_current_confirmation_hash_and_flash_endpoint(self):
        with patch.object(check, "check_build") as build:
            with self.assertRaisesRegex(ValueError, "confirmation required"):
                lab.prepare_capture(self.result, self.result, "wrong", "COM5", "1-1.1", False)
            build.assert_not_called()
        report = self.run_build()
        report.update(flash_port="COM5", flash_result=file_record(self.result))
        with patch.object(check, "check_build", return_value=report):
            with self.assertRaisesRegex(ValueError, "firmware hash"):
                lab.prepare_capture(self.result, self.result, "wrong", "COM5", "1-1.1", True)
            report["flash_port"] = "COM6"
            with self.assertRaisesRegex(ValueError, "flash endpoint"):
                self.binding(report)

    def test_bound_capture_rechecks_files_sdk_and_endpoint_before_open(self):
        report = self.run_build()
        write_json(self.root / "flash.json", {"fixture": "separate transfer record"})
        report.update(flash_port="COM5", flash_result=file_record(self.root / "flash.json"))
        endpoint = {"device": "COM5", "location": "1-1.1"}
        with patch("battery_device.port_identity", return_value=endpoint) as port, patch.object(check, "check_sdk") as sdk:
            binding, guard = self.binding(report)
            self.assertFalse(binding["device_identity_verified"])
            self.assertEqual(guard(), endpoint)
            sdk.assert_called_once()
            self.assertEqual(port.call_count, 2)
            self.source.write_bytes(b"changed after preparation")
            with self.assertRaisesRegex(ValueError, "Evidence changed"):
                guard()
            self.assertEqual(port.call_count, 2)

    def test_endpoint_change_rejected_under_lease_without_opening_or_commands(self):
        report = self.run_build()
        write_json(self.root / "flash.json", {"fixture": "separate transfer record"})
        report.update(flash_port="COM5", flash_result=file_record(self.root / "flash.json"))
        endpoint = {"device": "COM5", "serial_number": "one"}
        with patch("battery_device.port_identity", return_value=endpoint) as port, patch.object(check, "check_sdk"):
            binding, guard = self.binding(report)
            port.return_value = dict(endpoint, serial_number="other")
            peer = lab.Simulation("light")
            result = lab.record(self.root / "capture", peer, mode="light", seconds=.1,
                                identity_check=guard, binding=binding, locks=self.root / "locks")
        self.assertFalse(result["passed"])
        self.assertIn("Endpoint identity changed", result["detail"])
        self.assertFalse(peer.opened)
        self.assertTrue(peer.closed)
        self.assertEqual(peer.commands, [])

    def test_keyboard_cancel_preserves_evidence_and_closes_without_device_cleanup(self):
        peer = lab.Simulation("passive")
        def interrupt(_):
            raise KeyboardInterrupt()
        result = lab.record(self.root / "capture", peer, mode="passive", seconds=.1, ready=interrupt,
                            identity_check=lambda: {}, binding={"simulation": True}, locks=self.root / "locks")
        self.assertEqual(result["outcome"], "cancelled")
        self.assertTrue(result["resources_closed"])
        self.assertEqual(peer.commands, [])


if __name__ == "__main__":
    unittest.main()
