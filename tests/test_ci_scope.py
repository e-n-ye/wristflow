"""Exercise CI routing with real Git diffs, including renames and PR divergence."""

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "scripts/check_documentation.py"


class ScopeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.repo = Path(self.temp.name)
        self.git("init", "-q", "-b", "main")
        self.git("config", "core.autocrlf", "false")
        self.git("config", "user.name", "CI test")
        self.git("config", "user.email", "ci@example.invalid")
        self.write("docs/state.md", b"initial\n")
        self.write("core/main.c", b"int main(void) { return 0; }\n")
        self.base = self.commit()

    def git(self, *args):
        return subprocess.check_output(["git", *args], cwd=self.repo).decode().strip()

    def write(self, name, data):
        path = self.repo / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def commit(self):
        self.git("add", "-A")
        self.git("-c", "core.hooksPath=/dev/null", "commit", "-qm", "fixture")
        return self.git("rev-parse", "HEAD")

    def route(self, base=None, event="pull_request", success=True):
        env = dict(os.environ)
        env.pop("GITHUB_OUTPUT", None)
        env.pop("GITHUB_STEP_SUMMARY", None)
        result = subprocess.run(
            [sys.executable, str(SCRIPT), "--base", self.base if base is None else base,
             "--head", "HEAD", "--event", event],
            cwd=self.repo, env=env, capture_output=True, text=True,
        )
        if not success:
            self.assertNotEqual(result.returncode, 0)
            return
        self.assertEqual(result.returncode, 0, result.stderr)
        return json.loads(result.stdout)["docs_only"]

    def test_documentation_with_image_and_unicode_name(self):
        self.write("docs/时间.md", "真实时间\n".encode())
        self.write("docs/evidence/image.png", b"\x89PNG\0\xff")
        self.commit()
        self.assertTrue(self.route())
        self.assertTrue(self.route(event="push"))

    def test_deleted_document(self):
        (self.repo / "docs/state.md").unlink()
        self.commit()
        self.assertTrue(self.route())

    def test_source_rename_into_docs(self):
        self.git("mv", "core/main.c", "docs/example.txt")
        self.commit()
        self.assertFalse(self.route())

    def test_code_and_infrastructure_require_build(self):
        for path in ["core/main.c", ".github/workflows/build.yml", "sdk.lock.json",
                     "scripts/example.py", "docs/new_script.py"]:
            with self.subTest(path=path):
                self.git("reset", "--hard", self.base)
                self.write(path, b"changed\n")
                self.commit()
                self.assertFalse(self.route())

    def test_manual_unknown_and_empty_require_build(self):
        self.assertFalse(self.route())
        self.write("docs/state.md", b"updated\n")
        self.commit()
        self.assertFalse(self.route(event="workflow_dispatch"))
        self.assertFalse(self.route(base=""))
        self.assertFalse(self.route(base="0" * 40))
        self.route(base="invalid-reference", success=False)

    def test_pr_uses_merge_base(self):
        self.git("checkout", "-qb", "feature")
        self.write("docs/state.md", b"feature docs\n")
        self.commit()
        self.git("checkout", "-q", "main")
        self.write("core/main.c", b"main changed independently\n")
        advanced_base = self.commit()
        self.git("checkout", "-q", "feature")
        self.assertTrue(self.route(base=advanced_base))

    def test_invalid_markdown_fails_scope_check(self):
        for content in [b"\xff", b"<<<<<<< HEAD\nconflict\n", b"text\0"]:
            with self.subTest(content=content):
                self.write("docs/state.md", content)
                self.commit()
                self.route(success=False)


if __name__ == "__main__":
    unittest.main()
