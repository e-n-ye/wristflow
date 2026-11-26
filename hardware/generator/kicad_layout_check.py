"""Measure saved-board copper from kicad_pcb.geometry, in assembly millimetres."""

import argparse
import csv
import hashlib
import heapq
import itertools
import json
import math
from collections import defaultdict
from pathlib import Path

from shapely import hausdorff_distance, make_valid
from shapely.geometry import LineString, Point, Polygon
from shapely.ops import linemerge, unary_union


def alignment(a, b):
    """Acute angle and length of common projection on the first segment."""
    (x, y), (ex, ey) = a.coords
    (bx, by), (cx, cy) = b.coords
    ux, uy = (ex - x) / a.length, (ey - y) / a.length
    cosine = abs((ux * (cx - bx) + uy * (cy - by)) / b.length)
    lo, hi = sorted(((bx - x) * ux + (by - y) * uy, (cx - x) * ux + (cy - y) * uy))
    return math.degrees(math.acos(min(1.0, cosine))), max(
        0.0, min(a.length, hi) - max(0.0, lo)
    )


def check_inner_crossings(g):
    """L3 and L4 have no intervening ground plane in JLC060811-3313."""
    layers = {
        s: [
            (t, LineString([t["start"], t["end"]]))
            for t in g["tracks"]
            if t["side"] == s
        ]
        for s in ("L3", "L4")
    }
    crossings = []
    for ta, a in layers["L3"]:
        for tb, b in layers["L4"]:
            if (
                ta["net"] == tb["net"]
                or a.distance(b) > (ta["width"] + tb["width"]) / 2
            ):
                continue
            angle, overlap = alignment(a, b)
            if angle < 20 and overlap > 1e-6:
                raise ValueError(f"L3/L4 parallel copper overlap: {ta} vs {tb}")
            if a.intersects(b):
                if angle < 70 - 1e-6:
                    raise ValueError(
                        f"L3/L4 crossing at {angle:.2f} degrees: {ta} vs {tb}"
                    )
                crossings.append(angle)
    return {"crossings": len(crossings), "minimum_angle": min(crossings, default=None)}


def microphone_report(g, pins, output):
    groups = {
        "BTL": ["L1.1", "L1.2", "L2.1", "L2.2"],
        "电机": ["MT1.1", "MT2.1", "Q2.1", "Q2.3", "U1.17", "R36.1", "R36.2", "R37.1"],
        "充电": [
            "D19.1",
            "D19.2",
            "U17.8",
            "U3.A2",
            "U3.A3",
            "JP1.1",
            "R65.2",
            "U15.1",
        ],
        **{
            name: [res + ".1", res + ".2"]
            for name, res in zip(
                ("CLK", "D0", "D1", "D2", "D3"), ("R18", "R19", "R20", "R16", "R17")
            )
        },
        "CS": ["J201.14"],
        "TE": ["J201.13"],
        "RSTB": ["J201.15"],
        "屏供电": [
            "J201.4",
            "J201.5",
            "J201.19",
            "R206.1",
            "R206.2",
            "C201.1",
            "R49.1",
            "R49.2",
            "R50.1",
        ],
    }
    segments = defaultdict(list)
    for t in g["tracks"]:
        segments[t["net"]].append(
            (t["side"], LineString([t["start"], t["end"]]), t["width"])
        )
    rows, errors = [], []
    for source in ("U1.37", "U1.36", "R30.2", "U9.2"):
        net = pins[source]["net"]
        for group, keys in groups.items():
            nets = sorted({pins[key]["net"] for key in keys})
            parallel = []
            near = []
            crossings = []
            same_layer_gaps = []
            count = sum(len(segments[n]) for n in nets)
            for layer, a, wa in segments[net]:
                for other in nets:
                    for side, b, wb in segments[other]:
                        if side != layer and {side, layer} != {"L3", "L4"}:
                            continue
                        gap = a.distance(b) - (wa + wb) / 2
                        if side == layer:
                            same_layer_gaps.append(gap)
                        angle, overlap = alignment(a, b)
                        if a.intersects(b):
                            crossings.append(angle)
                            if angle < 70 - 1e-6:
                                errors.append(
                                    (
                                        net,
                                        other,
                                        layer,
                                        side,
                                        "cross",
                                        angle,
                                        list(a.coords),
                                        list(b.coords),
                                    )
                                )
                        if angle >= 20 or overlap <= 1e-6:
                            continue
                        parallel.append(gap)
                        if gap < 1.0 - 1e-6:
                            # Restrict the length to the actual close part of the finite segments.
                            clipped = a.intersection(b.buffer(1 + (wa + wb) / 2))
                            length = min(overlap, clipped.length)
                            near.append(length)
                            errors.append(
                                (
                                    net,
                                    other,
                                    layer,
                                    side,
                                    "parallel",
                                    gap,
                                    list(a.coords),
                                    list(b.coords),
                                )
                            )
            rows.append(
                [
                    net,
                    group,
                    " | ".join(nets),
                    count,
                    round(min(same_layer_gaps), 5) if same_layer_gaps else "",
                    round(min(parallel), 5) if parallel else "",
                    round(max(near, default=0), 5),
                    len(crossings),
                    round(min(crossings), 3) if crossings else "",
                ]
            )
    with output.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(
            [
                "受检网",
                "干扰源",
                "包含网络",
                "干扰线段数",
                "同层最小铜边距mm",
                "同向最小铜边距mm",
                "同层或无地邻层违规最长段mm",
                "交叉数",
                "最小交角度",
            ]
        )
        writer.writerows(rows)
    return errors


def read_pins(g):
    return {
        ref + "." + p["pin"]: dict(p, side=fp["side"])
        for ref, fp in g["footprints"].items()
        for p in fp["pads"]
    }


def copper_path(g, pins, start, end, minimum_width=0):
    """Conservative centreline path. Only a common via connects different layers."""
    src, dst = pins[start], pins[end]
    net = src["net"]
    if net != dst["net"]:
        raise ValueError(f"{start}, {end}: different nets")
    traces = [
        (t, LineString([t["start"], t["end"]]))
        for t in g["tracks"]
        if t["net"] == net and t["width"] >= minimum_width
    ]
    graph = defaultdict(list)
    coordinates = {start: tuple(src["xy"]), end: tuple(dst["xy"])}

    def connect(a, b):
        length = math.dist(coordinates[a], coordinates[b])
        graph[a].append((length, b))
        graph[b].append((length, a))

    def node(layer, xy):
        key = (layer, round(xy[0], 6), round(xy[1], 6))
        coordinates[key] = xy
        return key

    for t, line in traces:
        layer = t["side"]
        cuts = set(line.coords)
        for other, crossing in traces:
            if layer != other["side"]:
                continue
            # Include T joints and crossings whose intersection is inside a segment.
            contact = line.intersection(crossing)
            if contact.geom_type in ("Point", "LineString"):
                cuts.update(contact.coords)
        contacts = []
        for name in (start, end):
            pad = pins[name]
            if layer != pad["side"]:
                continue
            shape = unary_union([Polygon(r) for r in pad["polygons"]])
            if line.distance(shape) <= t["width"] / 2:
                contacts.append((name, tuple(pad["xy"])))
        for v in g["vias"]:
            if v["net"] == net and line.distance(Point(v["xy"])) <= v["diameter"] / 2:
                name = ("via", *v["xy"])
                coordinates[name] = tuple(v["xy"])
                contacts.append((name, coordinates[name]))

        for name, xy in contacts:
            land = line.interpolate(line.project(Point(xy))).coords[0]
            cuts.add(land)
            connect(name, node(layer, land))
        ordered = sorted(cuts, key=lambda xy: line.project(Point(xy)))
        for a, b in itertools.pairwise(ordered):
            connect(node(layer, a), node(layer, b))
    distances, previous = {start: 0.0}, {}
    queue = [(0.0, 0, start)]
    serial = 0
    while queue:
        distance, _, a = heapq.heappop(queue)
        if a == end:
            nodes = [end]
            while nodes[-1] != start:
                nodes.append(previous[nodes[-1]])
            return distance, [coordinates[n] for n in reversed(nodes)]
        if distance > distances[a]:
            continue
        for weight, b in graph[a]:
            new = distance + weight
            if new < distances.get(b, math.inf):
                distances[b], previous[b] = new, a
                serial += 1
                heapq.heappush(queue, (new, serial, b))
    raise ValueError(f"No {minimum_width:g} mm copper path from {start} to {end}")


def check_vias(g, pins, output):
    if not (g["fill_vias"] and g["cap_vias"] and g["vias"]):
        raise ValueError("Missing board-level via filling or capping")
    all_pads = [
        (ref + "." + p["pin"], p)
        for ref, f in g["footprints"].items()
        for p in f["pads"]
    ]
    masks = unary_union(
        [Polygon(r) for _, p in all_pads for r in p["mask"]]
        + [Polygon(r) for r in g["mask_graphics"]]
    )
    lands = [
        (k, p, unary_union([Polygon(r) for r in p["polygons"]])) for k, p in all_pads
    ]
    rows = []
    gaps = []
    b2 = pins["U3.B2"]
    exceptions = 0
    for via in g["vias"]:
        if not all(
            via[k]
            for k in ("filling", "capping", "remove_unconnected", "keep_outer_rings")
        ):
            raise ValueError(f"Via protection or inner ring setting at {via['xy']}")
        if via["xy"] == b2["xy"] and via["net"] == b2["net"]:
            exceptions += 1
            if (via["diameter"], via["drill"]) != (0.26, 0.15):
                raise ValueError("U3.B2 drill/land dimensions")
            continue
        center = Point(via["xy"])
        radius = via["diameter"] / 2
        mask_gap = center.distance(masks) - radius
        gaps.append(mask_gap)
        if mask_gap < 0.05 - 1e-6:
            raise ValueError(f"Via {via['xy']} mask clearance {mask_gap:.6f} mm")
        distances = (
            (
                math.dist(via["xy"], p["xy"]) - p["circle_radius"] - radius
                if p["circle_radius"] is not None
                else center.distance(shape) - radius,
                key,
            )
            for key, p, shape in lands
        )
        copper_gap, key = min(distances)
        if copper_gap < 0.10 - 1e-7:
            rows.append(
                [
                    *via["xy"],
                    via["net"],
                    via["drill"],
                    via["diameter"],
                    key,
                    round(copper_gap, 6),
                    round(mask_gap, 6),
                ]
            )
    if exceptions != 1:
        raise ValueError(f"Expected one U3.B2 via, found {exceptions}")
    with output.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(
            [
                "X mm",
                "Y mm",
                "网络",
                "钻径mm",
                "环外径mm",
                "最近焊盘",
                "孔环到铜mm",
                "孔环到开窗mm",
            ]
        )
        writer.writerows(sorted(rows))
    return {
        "vias": len(g["vias"]),
        "near_lands": len(rows),
        "minimum_mask_gap": min(gaps),
        "minimum_pad_gap": min((row[-2] for row in rows), default=None),
    }


def check_planes(g, pins):
    fills = {
        name: unary_union(
            [make_valid(Polygon(p["outer"], p["holes"])) for p in polygons]
        )
        for name, polygons in g["zone_fills"].items()
    }
    cutouts = [Polygon(h).representative_point() for h in g["holes"]]
    device = Point(g["footprints"]["U3"]["xy"]).buffer(1, quad_segs=64)
    results = {}
    for name in ("GND-L2", "GND-L5"):
        plane = fills[name]
        if plane.geom_type != "Polygon" or plane.area < 1080:
            raise ValueError(f"{name}: insufficient continuous ground")
        voids = [Polygon(h) for h in plane.interiors]
        normal = [h for h in voids if not any(h.covers(center) for center in cutouts)]
        largest = max(
            max(x1 - x0, y1 - y0) for x0, y0, x1, y1 in (h.bounds for h in normal)
        )
        holes = max(sum(h.contains(Point(v["xy"])) for v in g["vias"]) for h in normal)
        cover = device.intersection(plane).area / device.area
        if holes > 5 or largest > 3.5 or (name == "GND-L2" and cover < 0.60):
            raise ValueError(
                f"{name}: {holes} vias, {largest:.3f} mm void, U3 coverage {cover:.1%}"
            )
        results[name] = {
            "area": plane.area,
            "max_vias": holes,
            "max_span": largest,
            "u3_coverage": cover,
        }
    thermal = fills["U3 thermal ground"]
    for key in ("C6.1", "C7.1", "C8.1", "R64.2"):
        if not pins[key]["thermal"]:
            raise ValueError(f"{key}: expected thermal spokes")
    for p in g["footprints"]["U3"]["pads"]:
        land = Point(p["xy"]).buffer(0.13, quad_segs=96)
        window = unary_union([Polygon(r) for r in p["mask"]])
        copper = [land, thermal]
        copper.extend(
            LineString([t["start"], t["end"]]).buffer(t["width"] / 2, quad_segs=32)
            for t in g["tracks"]
            if t["side"] == "F" and t["net"] == p["net"]
        )
        copper.extend(
            Point(v["xy"]).buffer(v["diameter"] / 2, quad_segs=64)
            for v in g["vias"]
            if v["net"] == p["net"]
        )
        ratio = window.intersection(unary_union(copper)).area / land.area
        if ratio > 1.15:
            raise ValueError(
                f"U3.{p['pin']}: copper in mask opening {ratio:.3f} × land"
            )
        results["U3." + p["pin"]] = ratio
    return results


def check_layout(g, directory):
    pcb = directory / (directory.name + ".kicad_pcb")
    if hashlib.sha256(pcb.read_bytes()).hexdigest() != g["pcb_sha256"]:
        raise ValueError(
            "PCB changed since geometry export; run kicad_pcb.py --readback"
        )
    pins = read_pins(g)
    result = check_vias(g, pins, directory / "near-pad-vias.csv")
    result["pcb_sha256"] = g["pcb_sha256"]
    result.update(check_planes(g, pins))
    result["L3/L4"] = check_inner_crossings(g)
    errors = microphone_report(g, pins, directory / "microphone.csv")
    if errors:
        raise ValueError(f"Unshielded microphone conflicts: {errors[:6]}")
    for start, end, width, limit in (
        ("U17.1", "C2.2", 0, 3),
        ("U17.1", "D17.3", 0.30, 3),
        ("U17.1", "D19.1", 0.40, 15),
        ("U17.1", "R2.2", 0, 15),
        ("U17.8", "C42.2", 0, 3),
        ("U15.1", "C33.2", 0, 3),
        ("J2.1", "D2.2", 0, 3),
        ("J2.2", "D1.2", 0, 3),
    ):
        length, _ = copper_path(g, pins, start, end, width)
        if length > limit:
            raise ValueError(f"{start} to {end}: {length:.3f} > {limit} mm")
        result[start + " to " + end] = length
    dock = pins["U17.1"]["net"]
    dock_tracks = [t for t in g["tracks"] if t["net"] == dock]
    court = unary_union([Polygon(r) for r in g["module_court"]])
    if court.is_empty or any(
        LineString([t["start"], t["end"]]).distance(court) <= t["width"] / 2
        for t in dock_tracks
    ):
        raise ValueError("DOCK_PROTECTED enters the module courtyard")
    total = sum(math.dist(t["start"], t["end"]) for t in dock_tracks)
    if total > 15 or math.dist(g["footprints"]["R2"]["xy"], pins["U17.1"]["xy"]) > 5:
        raise ValueError("Input protection copper length or R2 distance")
    result["dock_total"] = total
    for key, limit in (("C2.1", 0.7), ("C33.1", 0.8)):
        distance = min(
            math.dist(pins[key]["xy"], v["xy"])
            for v in g["vias"]
            if v["net"] == pins[key]["net"]
        )
        if distance > limit + 1e-6:
            raise ValueError(f"{key} to ground via: {distance:.3f} mm")
        result[key + " to ground via"] = distance
    # Follow just the load-carrying pair. TVS and shunt branches are narrower.
    legs = []
    pad_paths = []
    for jack, bead in (("J2.1", "L2.2"), ("J2.2", "L1.2")):
        _, pad_path = copper_path(g, pins, jack, bead, 0.35)
        pad_paths.append(pad_path)
        copper = [
            t
            for t in g["tracks"]
            if t["net"] == pins[jack]["net"] and t["width"] >= 0.35
        ]
        if {t["side"] for t in copper} != {pins[jack]["side"]}:
            raise ValueError("BTL pair changes layer")
        path = linemerge([LineString([t["start"], t["end"]]) for t in copper])
        if path.geom_type != "LineString" or not path.is_simple:
            raise ValueError(f"{jack}: ambiguous BTL trunk")
        if math.dist(path.coords[-1], pins[jack]["xy"]) < math.dist(
            path.coords[0], pins[jack]["xy"]
        ):
            path = path.reverse()
        legs.append(path)
    loop = Polygon([*legs[0].coords, *reversed(legs[1].coords)])
    pad_loop = Polygon([*pad_paths[0], *reversed(pad_paths[1])])
    spacing = hausdorff_distance(*legs, densify=0.01)
    if not loop.is_valid or max(loop.area, pad_loop.area) > 10 or spacing > 0.600001:
        raise ValueError(
            f"BTL loop {loop.area:.3f} mm², centre spacing {spacing:.3f} mm"
        )
    result.update(
        btl_area=loop.area, btl_pad_center_area=pad_loop.area, btl_spacing=spacing
    )
    (directory / "layout-check.json").write_text(
        json.dumps(result, indent=2), encoding="utf-8"
    )
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    project = parser.parse_args().project
    check_layout(
        json.loads((project / "geometry.json").read_text(encoding="utf-8")), project
    )
