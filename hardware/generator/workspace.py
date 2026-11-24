"""Paths for private B-R2 inputs and outputs."""

import os
from pathlib import Path

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parents[1]
WORKSPACE = Path(os.environ.get('WRISTFLOW_B_WORKSPACE', ROOT / 'artifacts/wristflow-b')).resolve()
REFERENCE = WORKSPACE / 'reference/official-v12.epro'
LIBRARY = WORKSPACE / 'reference/lceda-std.elib'
CAD = WORKSPACE / '2026-11-22/B-crown/B-review-R2'
OUTPUT = WORKSPACE / 'eda'
CAD_INPUTS = WORKSPACE / 'eda-inputs'
FONT = Path(os.environ.get('SystemRoot', 'C:/Windows')) / 'Fonts/msyh.ttc'
