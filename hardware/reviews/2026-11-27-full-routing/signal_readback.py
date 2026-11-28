"""Read saved copper for the pre-filter BTL pair and both sides of each QSPI resistor."""
import argparse
import csv
import hashlib
import json
import math
import sys
from pathlib import Path

from shapely import STRtree, get_parts, hausdorff_distance, make_valid
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union
from qspi_antipads import CONTROL_PADS, QSPI_PADS

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "generator"))
from kicad_layout_check import alignment, check_inner_crossings, copper_path, read_pins
from qspi_routing import check_scheme3_leg


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


def ground_polygons(fills):
    return {name: unary_union([make_valid(Polygon(p["outer"], p["holes"])) for p in fills[name]])
            for name in ("GND-L2", "GND-L5")}


def qspi_report(g, pins, comparison=None, signal_pads=QSPI_PADS):
    planes = ground_polygons(g["zone_fills"])
    signal_vias = [v for v in g["vias"] if v["net"] != "GND"]
    via_tree = STRtree([Point(v["xy"]) for v in signal_vias])
    probe_planes = {}
    if comparison is not None:
        if comparison["pcb_sha256"] != g["pcb_sha256"]:
            raise ValueError("Regenerate antipad probe fills for the current PCB")
        probe_planes = {label: ground_polygons(fills) for label, fills in comparison["planes"].items()}
    gnd = [v for v in g["vias"] if v["net"] == "GND"]
    result = {}
    for name, contacts in signal_pads.items():
        nets = [pins[key]["net"] for key in contacts]
        changes = [v for net in nets for v in g["vias"] if v["net"] == net]
        excluded = unary_union([Point(v["xy"]).buffer(.64) for v in changes])
        tracks = [(t["side"], LineString([t["start"], t["end"]])) for t in g["tracks"] if t["net"] in nets]
        length = sum(line.length for _, line in tracks)
        layers = {}
        for side, reference in (("F", "GND-L2"), ("B", "GND-L5"), ("L3", "GND-L2"), ("L4", "GND-L5")):
            centerline = unary_union([line for layer, line in tracks if layer == side])
            plane = planes[reference]
            gap = centerline.difference(plane)
            shared = []
            for ring in plane.interiors:
                void = Polygon(ring)
                inside = centerline.intersection(void).length
                if inside <= 1e-8:
                    continue
                members = [signal_vias[i] for i in via_tree.query(void, predicate="covers")]
                if any(v["net"] in nets for v in members) and any(v["net"] not in nets for v in members):
                    shared.append({"centerline_mm": inside, "vias": [(v["net"], v["xy"]) for v in members]})
            foreign = None
            if probe_planes:
                # Restrict attribution to missing copper on the real board. Both probes
                # retain the original board edge, keepouts, and ground connections.
                foreign = gap.intersection(probe_planes["no_signal_vias"][reference]).difference(
                    probe_planes[name][reference]
                ).length
            layers[side] = {"reference": reference, "raw_gap_mm": gap.length,
                            "gap_excluding_own_vias_mm": gap.difference(excluded).length,
                            "foreign_antipad_mm": foreign, "shared_voids": shared}
        returns = []
        for via in changes:
            nearest = min(gnd, key=lambda v: math.dist(via["xy"], v["xy"]))
            used_layers = sorted({t["side"] for t in g["tracks"] if t["net"] == via["net"]
                                  and LineString([t["start"], t["end"]]).distance(Point(via["xy"])) <
                                  via["diameter"]/2 + t["width"]/2 + 1e-6})
            returns.append({"xy": via["xy"], "signal_layers": used_layers, "ground_xy": nearest["xy"],
                            "ground_distance_mm": math.dist(via["xy"], nearest["xy"])})
        result[name] = {"copper_mm": length, "through_vias": len(changes),
                        "copper_plus_board_thickness_upper_mm": length + len(changes) * g["thickness"],
                        "raw_plane_gap_mm": sum(v["raw_gap_mm"] for v in layers.values()),
                        "gap_excluding_own_vias_mm": sum(v["gap_excluding_own_vias_mm"] for v in layers.values()),
                        "foreign_antipad_mm": sum(v["foreign_antipad_mm"] for v in layers.values()) if probe_planes else None,
                        "shared_void_centerline_mm": sum(void["centerline_mm"] for v in layers.values()
                                                         for void in v["shared_voids"]),
                        "layers": layers, "transitions": returns}
    return result


def local_ground_vias(g, pins):
    """Find a separate, directly connected ground via within each capacitor's local disk."""
    ground = [via for via in g["vias"] if via["net"] == "GND"]
    copper = {}
    for side in ("F", "B"):
        lands = [Polygon(ring) for pad in pins.values() if pad["net"] == "GND" and pad["side"] == side
                 for ring in pad["polygons"]]
        tracks = [LineString([t["start"], t["end"]]).buffer(t["width"]/2) for t in g["tracks"]
                  if t["net"] == "GND" and t["side"] == side]
        copper[side] = unary_union(lands + tracks + [Point(v["xy"]).buffer(v["diameter"]/2) for v in ground])
    result = {}
    owners = {}
    for key, pad in sorted(pins.items()):
        if not key.startswith("C") or pad["net"] != "GND":
            continue
        center = Point(pad["xy"])
        # Circumscribe the disk by the CAD readback tolerance at its rim.
        local = copper[pad["side"]].intersection(center.buffer(.800001, quad_segs=1024))
        region = next(piece for piece in get_parts(local) if piece.covers(center))
        candidates = [(center.distance(Point(v["xy"])), tuple(v["xy"])) for v in ground
                      if center.distance(Point(v["xy"])) <= .800001 and region.covers(Point(v["xy"]))]
        if not candidates:
            raise ValueError(f"{key} has no directly connected local GND via within 0.80 mm")
        distance, via = min(candidates)
        if via in owners:
            raise ValueError(f"{key} and {owners[via]} share their local GND via")
        owners[via] = key
        result[key] = {"ground_via_xy": via, "pad_to_via_mm": distance}
    return result


def write_transition_returns(g, output):
    """Count a via once if attached copper uses both L2- and L5-referenced layers."""
    from collections import defaultdict

    copper = defaultdict(list)
    for track in g["tracks"]:
        shape = LineString([track["start"], track["end"]])
        copper[track["net"]].append((track["side"], shape, track["width"] / 2))
    for fp in g["footprints"].values():
        for pad in fp["pads"]:
            copper[pad["net"]].extend((fp["side"], Polygon(ring), 0) for ring in pad["polygons"])

    ground = [v["xy"] for v in g["vias"] if v["net"] == "GND"]
    rows = []
    for via in g["vias"]:
        if via["net"] == "GND":
            continue
        center = Point(via["xy"])
        layers = {layer for layer, shape, radius in copper[via["net"]]
                  if center.distance(shape) < radius + via["layer_diameters"][layer] / 2 + 1e-6}
        if not layers.intersection(("F", "L3")) or not layers.intersection(("L4", "B")):
            continue
        nearest = min(ground, key=lambda xy: math.dist(xy, via["xy"]))
        distance = math.dist(nearest, via["xy"])
        rows.append((via["net"], *via["xy"], "/".join(sorted(layers)), *nearest,
                     round(distance, 6), "是" if distance > 1.5 else "否"))
    with output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["网络", "孔 X mm", "孔 Y mm", "实际接铜层", "地孔 X mm", "地孔 Y mm",
                         "中心距 mm", "超过 1.5 mm"])
        writer.writerows(sorted(rows, key=lambda row: (-row[6], row[0], row[1], row[2])))


def check_scheme3_qspi(g, pins, comparison):
    if comparison is None:
        raise ValueError("Scheme 3 QSPI acceptance requires same-board foreign-antipad refill probes")
    signals = qspi_report(g, pins, comparison)
    screen_nets = set()
    for i, (name, contacts) in enumerate(QSPI_PADS.items()):
        for leg, (start, end) in enumerate(((f"U1.{43-i}", contacts[0]), (contacts[1], f"J201.{8+i}"))):
            copper_path(g, pins, start, end)
            net = pins[start]["net"]
            screen_nets.add(net)
            layers = {t["side"] for t in g["tracks"] if t["net"] == net}
            holes = sum(v["net"] == net for v in g["vias"])
            check_scheme3_leg(name, leg == 0, layers, holes)
        row = signals[name]
        if row["gap_excluding_own_vias_mm"] > 1e-6 or row["foreign_antipad_mm"] > 1e-6:
            raise ValueError(f"{name}: reference-plane gap or foreign antipad crossing")
        if any(t["ground_distance_mm"] > 1.5 + 1e-6 for t in row["transitions"]):
            raise ValueError(f"{name}: return via outside 1.5 mm")
    lengths = [row["copper_mm"] for row in signals.values()]
    spread = max(lengths) - min(lengths)
    if spread > 1.900 + 1e-6:
        raise ValueError(f"Five-line skew {spread:.6f} mm exceeds the locked 1.900 mm")
    mic_nets = {pins[k]["net"] for k in ("U1.37", "U1.36", "R30.2", "U9.2")}
    mic_tracks = [t for t in g["tracks"] if t["net"] in mic_nets and t["side"] in ("L3", "L4")]
    checked_gap = math.inf
    for track in g["tracks"]:
        if track["net"] not in screen_nets or track["side"] != "L4":
            continue
        a = LineString([track["start"], track["end"]])
        for mic in mic_tracks:
            b = LineString([mic["start"], mic["end"]])
            angle, overlap = alignment(a, b)
            if angle < 20 and overlap > 1e-6:
                gap = a.distance(b) - (track["width"] + mic["width"]) / 2
                checked_gap = min(checked_gap, gap)
                if gap < 1.0 - 1e-6:
                    raise ValueError(f"{track['net']} L4 and {mic['net']} {mic['side']} parallel gap {gap:.6f} mm")
    return {"qspi": signals, "length_difference_mm": spread,
            "inner_crossings": check_inner_crossings(g, touching_nets=screen_nets),
            "l4_mic_min_parallel_gap_mm": checked_gap if math.isfinite(checked_gap) else None}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--antipad-planes", type=Path, help="planes.json from qspi_antipads.py")
    parser.add_argument("--routing-scheme", type=int, choices=(1, 2, 3))
    parser.add_argument("--qspi-only", action="store_true", help="Run the scheme 3 QSPI acceptance on an intermediate board")
    parser.add_argument("--return-vias", type=Path, help="CSV of all non-ground reference changes")
    args = parser.parse_args()
    pcb = args.project / (args.project.name + ".kicad_pcb")
    geometry = json.loads((args.project / "geometry.json").read_text(encoding="utf-8"))
    if hashlib.sha256(pcb.read_bytes()).hexdigest() != geometry["pcb_sha256"]:
        raise ValueError("Run kicad_pcb.py --readback on the current PCB")
    pins = read_pins(geometry)
    if args.return_vias:
        write_transition_returns(geometry, args.return_vias)
    comparison = json.loads(args.antipad_planes.read_text(encoding="utf-8")) if args.antipad_planes else None
    profile = args.project / "routing-scheme.json"
    scheme = args.routing_scheme or (json.loads(profile.read_bytes())["scheme"] if profile.exists() else 2)
    scheme3 = check_scheme3_qspi(geometry, pins, comparison) if scheme == 3 else None
    if args.qspi_only:
        if scheme3 is None:
            parser.error("--qspi-only requires scheme 3")
        print(json.dumps({"pcb_sha256": geometry["pcb_sha256"], **scheme3}, indent=2))
        sys.exit(0)
    report = {"pcb_sha256": geometry["pcb_sha256"], "btl": btl_report(geometry, pins),
              "qspi": qspi_report(geometry, pins, comparison),
              "display_control": qspi_report(geometry, pins, comparison, CONTROL_PADS),
              "capacitor_ground": local_ground_vias(geometry, pins)}
    lengths = [row["copper_mm"] for row in report["qspi"].values()]
    report["qspi_maximum_length_difference_mm"] = max(lengths) - min(lengths)
    if scheme3 is not None:
        report["scheme3_qspi"] = scheme3
    print(json.dumps(report, ensure_ascii=False, indent=2))
