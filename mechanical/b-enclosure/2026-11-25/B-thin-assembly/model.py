"""Installed R2 solids and parametric layer datums. All lengths in mm."""

import csv
import json
import math
import os
import sys
from dataclasses import dataclass
from pathlib import Path

from workspace import ROOT, SOURCE, WORKSPACE

sys.path.insert(0, str(SOURCE / "2026-11-22/B-crown"))
import itertools

from review_revision import App, block, shifted

R2 = WORKSPACE / "2026-11-22/B-crown/B-review-R2/WristFlow-B-Review-R2.FCStd"
BOM = ROOT / "hardware/reviews/2026-11-22-b-r2/placement-BOM.csv"
GROUPS = [
    "Fixed",
    "Band",
    "Display",
    "Circuit",
    "Fasteners",
    "Cell",
    "Mated",
    "Lid",
    "Links",
    "CrownAssembly",
]
CANDIDATES = {"U4": (7, 7), "U15": (10.5, 9), "U21": (12, 19), "D19": (10, 13)}


@dataclass(frozen=True)
class Dimensions:
    j201_stack: float
    screen_thickness: float
    cell_x: float
    cell_y: float
    cell_z: float
    bump: float

    @classmethod
    def read(cls):
        default = Path(__file__).with_name("parameters.json")
        data = json.loads(Path(os.getenv("WRISTFLOW_B_THIN_PARAMETERS", default)).read_text(encoding="utf-8"))
        if any(type(v) not in (int, float) or not math.isfinite(v) or v <= 0 for v in data.values()):
            raise ValueError("All dimensions must be finite positive numbers")
        return cls(**data)

    @property
    def separator_top(self):
        return 1.35 + self.cell_z + 0.10

    @property
    def board(self):
        return self.separator_top + 0.50 + 0.20 + self.j201_stack

    @property
    def board_shift(self):
        return self.board - 11.30

    @property
    def flex_bottom(self):
        return self.board + 0.80 + 3.10 + 0.30

    @property
    def rim(self):
        return self.flex_bottom + 0.27 + self.screen_thickness + 0.165


def box_z(shape, z0, z1):
    return shape.common(block(-45, -45, z0, 90, 90, z1 - z0))


def scaled_z(shape, old0, new0, scale):
    """Affine transform about a mating plane; preserve XY and both end sections."""
    if abs(scale - 1) < 1e-9:
        return shifted(shape, new0 - old0)
    matrix = App.Matrix()
    matrix.A33 = scale
    matrix.A34 = new0 - scale * old0
    return shape.transformGeometry(matrix)


def join(parts):
    parts = [s for s in parts if s.Solids]
    return parts[0].multiFuse(parts[1:]).removeSplitter() if len(parts) > 1 else parts[0]


def slab_map(shape, stations):
    """Map each R2 Z interval to its new endpoints, retaining mating sections."""
    pieces = []
    for (z0, a), (z1, b) in itertools.pairwise(stations):
        if b <= a:
            raise ValueError(f"Invalid Z interval {z0, z1} -> {a, b}")
        if shape.BoundBox.ZMax <= z0 or shape.BoundBox.ZMin >= z1:
            continue
        part = box_z(shape, z0, z1)
        if part.Solids:
            pieces.append(scaled_z(part, z0, a, (b - a) / (z1 - z0)))
    return join(pieces)


def bom_rows():
    with BOM.open(encoding="utf-8-sig", newline="") as file:
        return list(csv.DictReader(file))


def envelope(row, z, xy=None, turn=False):
    w, h, height = (float(row[k]) for k in ("占位X mm", "占位Y mm", "装配Z预算 mm"))
    if turn:
        w, h = h, w
    x, y = xy or (float(row["原点X mm"]), float(row["原点Y mm"]))
    return block(x - w / 2, y - h / 2, z, w, h, height)
