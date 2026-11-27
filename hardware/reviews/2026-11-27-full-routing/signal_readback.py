"""Read saved copper for the pre-filter BTL pair and both sides of each QSPI resistor."""
import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

from shapely import get_parts, hausdorff_distance, make_valid
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "generator"))
from kicad_layout_check import copper_path, read_pins


def btl_report(g, pins):
    legs = []
    for start, end in (("U6.5", "L1.1"), ("U6.8", "L2.1")):
        net = pins[start]["net"]
        segments = [t for t in g["tracks"] if t["net"] == net]
        if not segments or min(t["width"] for t in segments) < .30:
            raise ValueError(f"{start}: below pre-filter BTL width")
        length, coords = copper_path(g, pins, start, end, .30)
        legs.append(LineString(coords))
    loop = make_valid(Polygon([*legs[0].coords, *reversed(legs[1].coords)]))
    lobes = [poly.area for member in get_parts(loop) for poly in get_parts(member)
             if poly.geom_type == "Polygon"]
    if not lobes or not math.isclose(sum(lobes), loop.area):
        raise ValueError("Unable to resolve BTL projection into closed lobes")
    # Millimetres in assembly coordinates. Both boundaries fall on vertices
    # of the pre-filter paths, bracketing their sustained common projection.
    sections = {}
    for name, low, high in (("launch", -30, -6.68), ("parallel", -6.68, -4.74), ("bead_entry", -4.74, 30)):
        parts = [leg.intersection(box(low, -30, high, 30)) for leg in legs]
        if any(part.is_empty for part in parts):
            raise ValueError(f"Review BTL section cuts for {name}")
        sections[name] = {"x_mm": [low, high], "lengths_mm": [part.length for part in parts],
                          "max_center_distance_mm": float(hausdorff_distance(*parts, densify=.01))}
    return {"path_lengths_mm": [leg.length for leg in legs], "absolute_lobe_areas_mm2": sorted(lobes),
            "sum_area_mm2": sum(lobes), "max_center_distance_mm": float(hausdorff_distance(*legs, densify=.01)),
            "sections": sections}


def qspi_report(g, pins):
    planes = {name: unary_union([make_valid(Polygon(p["outer"], p["holes"])) for p in g["zone_fills"][name]])
              for name in ("GND-L2", "GND-L5")}
    gnd = [v for v in g["vias"] if v["net"] == "GND"]
    result = {}
    for name, resistor in zip(("CLK", "D0", "D1", "D2", "D3"), ("R18", "R19", "R20", "R16", "R17")):
        nets = {pins[resistor+"."+pin]["net"] for pin in ("1", "2")}
        changes = [v for v in g["vias"] if v["net"] in nets]
        excluded = unary_union([Point(v["xy"]).buffer(.64, quad_segs=64) for v in changes])
        missing = raw = length = 0
        for t in g["tracks"]:
            if t["net"] not in nets:
                continue
            line = LineString([t["start"], t["end"]])
            plane = planes["GND-L2" if t["side"] in ("F", "L3") else "GND-L5"]
            gap = line.difference(plane)
            raw += gap.length
            missing += gap.difference(excluded).length
            length += line.length
        returns = []
        for via in sorted(changes, key=lambda v: v["xy"]):
            nearest = min(gnd, key=lambda v: math.dist(via["xy"], v["xy"]))
            used_layers = sorted({t["side"] for t in g["tracks"] if t["net"] == via["net"]
                                  and LineString([t["start"], t["end"]]).distance(Point(via["xy"])) <
                                  via["diameter"]/2 + t["width"]/2 + 1e-6})
            returns.append({"xy": via["xy"], "signal_layers": used_layers, "ground_xy": nearest["xy"],
                            "ground_distance_mm": math.dist(via["xy"], nearest["xy"])})
        result[name] = {"copper_mm": length, "raw_plane_gap_mm": raw,
                        "gap_excluding_own_vias_mm": missing, "transitions": returns}
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    args = parser.parse_args()
    pcb = args.project / (args.project.name + ".kicad_pcb")
    geometry = json.loads((args.project / "geometry.json").read_text(encoding="utf-8"))
    if hashlib.sha256(pcb.read_bytes()).hexdigest() != geometry["pcb_sha256"]:
        raise ValueError("Run kicad_pcb.py --readback on the current PCB")
    pins = read_pins(geometry)
    report = {"pcb_sha256": geometry["pcb_sha256"], "btl": btl_report(geometry,pins),
              "qspi": qspi_report(geometry,pins)}
    print(json.dumps(report, ensure_ascii=False, indent=2))
