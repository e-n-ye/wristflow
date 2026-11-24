"""Local workspace for the dated B enclosure studies."""

import os
from pathlib import Path

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parents[1]
WORKSPACE = Path(os.environ.get("WRISTFLOW_B_WORKSPACE", ROOT / "artifacts/wristflow-b")).resolve()


def study_dir(script):
    return WORKSPACE / Path(script).resolve().parent.relative_to(SOURCE)
