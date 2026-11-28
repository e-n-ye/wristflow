"""Project the installed screen tail into the tolerance band of each proposed component."""
import argparse
import csv
import json
from pathlib import Path

from shapely.geometry import Polygon, mapping
from shapely.ops import unary_union

TAIL_Z_ALLOWANCE = 0.055
MIN_LATERAL = 0.31999


def tail_shadow(tail, board_bottom, thickness, side, height):
    import FreeCAD as App
    import Part

    surface = board_bottom + (thickness if side == "F" else 0)
    low, high = sorted((surface, surface + (height if side == "F" else -height)))
    low -= TAIL_Z_ALLOWANCE
    high += TAIL_Z_ALLOWANCE
    bounds = tail.BoundBox
    slab = Part.makeBox(bounds.XLength + 2, bounds.YLength + 2, high-low,
                        App.Vector(bounds.XMin-1, bounds.YMin-1, low))
    section = tail.common(slab)
    if not section.Solids:
        return Polygon()
    vertices, triangles = section.tessellate(.003)
    faces = [Polygon([(vertices[i].x, vertices[i].y) for i in tri]) for tri in triangles]
    # A small additional offset covers chord error in the geometric projection.
    return unary_union([face for face in faces if face.area > 1e-10]).buffer(MIN_LATERAL + .004)


def body_parameters(ref):
    from kicad_schematic import MAIN_ADDITIONS
    from library import parts
    from module_pins import RESET_LANDS
    from shapely.geometry import Point

    if ref in RESET_LANDS:
        return Point(0, 0).buffer(.35), .035
    part = parts[MAIN_ADDITIONS.get(ref, {}).get("copy_part", ref)]
    return part["body"], part["height"]


def check(layout, shadows):
    from library import transform

    violations = []
    for r in layout:
        # The fixed mating connector is in intentional contact with this flex.
        if r["ref"] == "J201":
            continue
        body, h = body_parameters(r["ref"])
        geometry = transform(body, float(r["x"]), float(r["y"]), float(r["degrees"]), r["side"])
        if geometry.intersects(shadows[r["side"], h]):
            violations.append(r["ref"])
    return sorted(violations)


def main():
    import FreeCAD as App

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("assembly", type=Path)
    parser.add_argument("layout", type=Path)
    parser.add_argument("--export", type=Path, help="Local footprint-height keepouts for placement search")
    args = parser.parse_args()
    with args.layout.open(encoding="utf-8") as stream:
        layout = list(csv.DictReader(stream))
    thin = json.loads((args.assembly / "geometry.json").read_bytes())
    doc = App.openDocument(str(next(args.assembly.glob("*.FCStd")).resolve()))
    heights = {body_parameters(r["ref"])[1] for r in layout}
    shadows = {(s, h): tail_shadow(doc.RearRun.Shape, thin["board_bottom_mm"], thin["layers_mm"]["board"], s, h)
               for s in ("F", "B") for h in heights}
    if args.export:
        args.export.write_text(json.dumps({f"{s}:{h}": mapping(poly) for (s, h), poly in shadows.items()}),
                               encoding="utf-8")
    violations = check(layout, shadows)
    print("RearRun XY/Z precheck:", len(layout), "footprints;", len(violations), "conflicts", *violations)
    return bool(violations)


if __name__ == "__main__":
    raise SystemExit(main())
