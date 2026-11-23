"""Scratch directories for host tests under the ignored artifacts/ folder.

Each directory is removed when its test or test class finishes. Set
WRISTFLOW_KEEP_TEST_ARTIFACTS=1 to keep them for manual inspection; copy any
output that becomes evidence into docs/evidence instead of citing scratch paths.
"""
import os
from pathlib import Path
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]
KEEP_ENV = "WRISTFLOW_KEEP_TEST_ARTIFACTS"


def keep_requested():
    return os.environ.get(KEEP_ENV, "").strip() not in ("", "0")


def scratch_dir(prefix, register_cleanup):
    """Create artifacts/<prefix>XXXX and register its removal.

    register_cleanup is TestCase.addCleanup or TestCase.addClassCleanup.
    """
    path = Path(tempfile.mkdtemp(prefix=prefix, dir=ROOT / "artifacts"))
    if not keep_requested():
        register_cleanup(shutil.rmtree, path, True)
    return path
