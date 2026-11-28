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
from copper_boundary import minimum_edge_gaps


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


POWER_RAILS = {
    "VBAT", "VBAT_S", "VSYS", "VSYS_S", "VSYS_1", "VBUS_5V",
    "OLED_3V3", "OLED_VBAT", "OLED_VCI", "VCC_3V3", "VCC_3V3_S",
}


def check_inner_crossings(g, touching_nets=None):
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
    minimum_gap = math.inf
    for ta, a in layers["L3"]:
        for tb, b in layers["L4"]:
            if ta["net"] == tb["net"]:
                continue
            if touching_nets is not None and not ({ta["net"], tb["net"]} & touching_nets):
                continue
            gap = a.distance(b) - (ta["width"] + tb["width"]) / 2
            angle, overlap = alignment(a, b)
            if angle < 20 and overlap > 1e-6:
                power_pair = all(t["net"].split("/")[-1] in POWER_RAILS for t in (ta, tb))
                # Power pairs retain the earlier prohibition on overlapping copper.
                if not power_pair:
                    minimum_gap = min(minimum_gap, gap)
                if gap < (0 if power_pair else 0.20) - 1e-6:
                    raise ValueError(f"L3/L4 parallel gap {gap:.4f} mm: {ta} vs {tb}")
            if a.intersects(b):
                if angle < 70 - 1e-6:
                    raise ValueError(
                        f"L3/L4 crossing at {angle:.2f} degrees: {ta} vs {tb}"
                    )
                crossings.append(angle)
    return {"crossings": len(crossings), "minimum_angle": min(crossings, default=None),
            "minimum_parallel_signal_gap": minimum_gap if math.isfinite(minimum_gap) else None}


def microphone_report(g, pins, output):
    groups = {
        "BTL": "SPK_OUTP SPK_OUTN N_31N8678 N_31N8717",
        "电机": "MOTOR_P MOTOR_N MOTOR_GATE VDD33_VOUT PA_20_VIB_PWM",
        "充电": "DOCK_5V_IN DOCK_PROTECTED VBUS_5V VBAT VBAT_S VSYS VSYS_S VSYS_1 MOD_VSYS MOD_SW_OUT CHG_VDD",
        "CLK": "PA_04_LCD_QSPI_CLK N_20N32248",
        "D0": "PA_05_LCD_QSPI_D0 N_20N32291",
        "D1": "PA_06_LCD_QSPI_D1 N_20N32377",
        "D2": "PA_07_LCD_QSPI_D2 N_20N32162",
        "D3": "PA_08_LCD_QSPI_D3 N_20N32208",
        "CS": "PA_03_LCD_QSPI_CS", "TE": "PA_02_LCD_QSPI_TE", "RSTB": "PA_00_LCD_QSPI_RSTB",
        "屏供电": "OLED_VBAT OLED_VCI OLED_3V3 VCC_3V3 VCC_3V3_S VSYS_1",
    }
    nets_by_name = defaultdict(set)
    for pad in pins.values():
        nets_by_name[pad["net"].split("/")[-1]].add(pad["net"])
    monitored = {}
    for group, names in groups.items():
        missing = set(names.split()) - nets_by_name.keys()
        if missing:
            raise ValueError(f"Missing microphone aggressor networks: {missing}")
        monitored[group] = sorted(set.union(*(nets_by_name[n] for n in names.split())))
    victims = {pins[key]["net"] for key in ("U1.37", "U1.36", "R30.2", "U9.2")}
    other_nets = {p["net"] for p in pins.values()} - victims - {"GND", ""}
    other_nets.difference_update(set.union(*(set(n) for n in monitored.values())))
    segments = defaultdict(list)
    for t in g["tracks"]:
        segments[t["net"]].append(
            (t["side"], LineString([t["start"], t["end"]]), t["width"])
        )
    rows, errors, observations = [], [], []
    for source in ("U1.37", "U1.36", "R30.2", "U9.2"):
        net = pins[source]["net"]
        for group, nets in [*monitored.items(), ("清单外", sorted(other_nets))]:
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
                        if group == "清单外":
                            angle, overlap = alignment(a, b)
                            if gap < 1 and ((angle < 20 and overlap > 1e-6) or gap < 0.30):
                                observations.append([net, other, layer, side, "track", list(b.coords),
                                                     round(gap, 5), round(overlap, 5), round(angle, 3)])
                            continue
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
            if group == "清单外":
                continue
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
    # Through holes carry the barrel through all layers. Use the saved flash
    # state on each layer for annulus versus barrel copper.
    victim_copper = defaultdict(list)
    features = []
    for fp in g["footprints"].values():
        for pad in fp["pads"]:
            features.append((pad["net"], fp["side"], "pad", pad["xy"],
                             unary_union([Polygon(r) for r in pad["polygons"]])))
    for via in g["vias"]:
        for layer, diameter in via["layer_diameters"].items():
            features.append((via["net"], layer, "via", via["xy"], Point(via["xy"]).buffer(diameter/2)))
    for n, side, _, _, shape in features:
        if n in victims:
            victim_copper[n, side].append(shape)
    for n in victims:
        for side, line, width in segments[n]:
            victim_copper[n, side].append(line.buffer(width/2))
    approved = set.union(*(set(n) for n in monitored.values()))
    for (victim, side), shapes in victim_copper.items():
        copper = unary_union(shapes)
        for n, layer, kind, xy, shape in features:
            if n in victims or n in ("GND", "") or side != layer:
                continue
            gap = copper.distance(shape)
            if gap < 1:
                observations.append([victim, n, side, layer, kind, xy, round(gap, 5), "", ""])
            if n in approved and gap < 0.30 - 1e-6:
                errors.append((victim, n, kind, xy, gap))
    with output.with_name("microphone-proximity.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["受检网","邻网","受检层","邻层","对象","坐标mm","铜边距mm","同向投影mm","夹角度"])
        writer.writerows(sorted(observations, key=str))
    return errors


def read_pins(g):
    return {
        ref + "." + p["pin"]: dict(p, side=fp["side"])
        for ref, fp in g["footprints"].items()
        for p in fp["pads"]
    }


def copper_path(g, pins, start, end, minimum_width=0, *, steps=None):
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

    def connect(a, b, layer, width=None):
        length = math.dist(coordinates[a], coordinates[b])
        graph[a].append((length, b, layer, width))
        graph[b].append((length, a, layer, width))

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
            connect(name, node(layer, land), layer)
        ordered = sorted(cuts, key=lambda xy: line.project(Point(xy)))
        for a, b in itertools.pairwise(ordered):
            connect(node(layer, a), node(layer, b), layer, t["width"])
    distances, previous = {start: 0.0}, {}
    queue = [(0.0, 0, start)]
    serial = 0
    while queue:
        distance, _, a = heapq.heappop(queue)
        if a == end:
            nodes = [end]
            while nodes[-1] != start:
                nodes.append(previous[nodes[-1]][0])
            if steps is not None:
                for target in reversed(nodes[:-1]):
                    parent, layer, width = previous[target]
                    steps.append({"layer": layer, "width_mm": width,
                                  "start": coordinates[parent], "end": coordinates[target],
                                  "length_mm": math.dist(coordinates[parent], coordinates[target])})
            return distance, [coordinates[n] for n in reversed(nodes)]
        if distance > distances[a]:
            continue
        for weight, b, layer, width in graph[a]:
            new = distance + weight
            if new < distances.get(b, math.inf):
                distances[b], previous[b] = new, (a, layer, width)
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


def check_ground_planes(g, fills):
    cutouts = [Polygon(h) for h in g["holes"]]
    # The older, more widely cleared saved plane is the budget datum. Charge
    # 100 further non-ground vias at the largest ordinary land size, even when
    # the unconnected inner ring is removed and only the drill clears copper.
    min_area = 1084.168 - 100 * math.pi * (0.50 / 2 + 0.20) ** 2
    nominal = Polygon(g["outline"], g["holes"]).buffer(-0.30)
    nominal = nominal.difference(unary_union([Polygon(r) for r in g["plane_keepouts"]]))
    device = Point(g["footprints"]["U3"]["xy"]).buffer(1, quad_segs=64)
    foreign = [Point(v["xy"]) for v in g["vias"] if v["net"] != "GND"]
    results = {}
    for name in ("GND-L2", "GND-L5"):
        plane = fills[name]
        if plane.geom_type != "Polygon" or plane.area < min_area:
            raise ValueError(f"{name}: insufficient continuous ground, {plane.area:.3f} mm²")
        voids = [Polygon(r) for r in plane.interiors]
        normal = []
        extra = []
        for void in voids:
            matched = [cut for cut in cutouts if void.covers(cut.representative_point())]
            if not matched:
                normal.append(void)
                continue
            growth = void.difference(unary_union([cut.buffer(.30) for cut in matched])).area
            if growth > .5 or any(void.covers(p) for p in foreign):
                raise ValueError(f"{name}: mechanical cutout absorbed copper/via at {void.bounds}, {growth:.4f} mm²")
            extra.append(growth)
        exterior = Polygon(plane.exterior)
        # Allow 20 µm for polygon discretization and pour rounding when
        # splitting edge-connected losses into individual notches.
        missing = nominal.buffer(-.02).difference(exterior)
        pieces = getattr(missing, "geoms", [missing])
        for piece in pieces:
            if piece.is_empty:
                continue
            for cut in cutouts:
                if piece.distance(cut) > .33:
                    continue
                if piece.area > .5 or any(piece.covers(p) for p in foreign):
                    raise ValueError(f"{name}: edge-open mechanical cutout at {piece.bounds}")
        # A 0.80 mm inward offset allows a single ordinary antipad at the
        # outer edge. Any deeper notch, including one without a via, fails.
        edge_core = nominal.buffer(-.80)
        if edge_core.difference(exterior).area > 1e-6:
            raise ValueError(f"{name}: board-edge notch exceeds 0.80 mm depth")
        largest = max((max(h.bounds[2]-h.bounds[0], h.bounds[3]-h.bounds[1]) for h in normal), default=0)
        holes = max((sum(h.covers(p) for p in foreign) for h in normal), default=0)
        cover = device.intersection(plane).area / device.area
        if holes > 5 or largest > 3.5 or (name == "GND-L2" and cover < 0.60):
            raise ValueError(f"{name}: {holes} vias, {largest:.3f} mm void, U3 coverage {cover:.1%}")
        results[name] = {"area": plane.area, "minimum_area": min_area, "max_vias": holes,
                         "max_span": largest, "u3_coverage": cover, "cutout_growth": extra}
    return results


def check_planes(g, pins):
    fills = {
        name: unary_union(
            [make_valid(Polygon(p["outer"], p["holes"])) for p in polygons]
        )
        for name, polygons in g["zone_fills"].items()
    }
    results = check_ground_planes(g, fills)
    top = defaultdict(list)
    for name, (layer, net) in g["zone_layers"].items():
        if layer == "F.Cu":
            top[net].append(fills[name])
    for fp in g["footprints"].values():
        if fp["side"] == "F":
            for pad in fp["pads"]:
                if pad["circle_radius"] is not None:
                    top[pad["net"]].append(Point(pad["xy"]).buffer(pad["circle_radius"], quad_segs=96))
                else:
                    top[pad["net"]].extend(Polygon(r) for r in pad["polygons"])
    for t in g["tracks"]:
        if t["side"] == "F":
            top[t["net"]].append(LineString([t["start"], t["end"]]).buffer(t["width"]/2, quad_segs=32))
    for v in g["vias"]:
        top[v["net"]].append(Point(v["xy"]).buffer(v["diameter"]/2, quad_segs=64))
    top = {net: unary_union(parts) for net, parts in top.items()}
    for key in ("C6.1", "C7.1", "C8.1", "R64.2"):
        if not pins[key]["thermal"]:
            raise ValueError(f"{key}: expected thermal spokes")
    for p in g["footprints"]["U3"]["pads"]:
        land = Point(p["xy"]).buffer(0.13, quad_segs=96)
        window = unary_union([Polygon(r) for r in p["mask"]])
        foreign_area = sum(window.intersection(cu).area for net, cu in top.items() if net != p["net"])
        if foreign_area > 1e-9:
            raise ValueError(f"U3.{p['pin']}: foreign copper in mask opening {foreign_area:.8f} mm²")
        # KiCad's inscribed polygon approximation cuts corners off round pads.
        # Use the saved radius for circular lands in the exposed-area ratio.
        if p["circle_radius"] != .13:
            raise ValueError(f"U3.{p['pin']}: unexpected ball land size")
        ratio = window.intersection(top[p["net"]]).area / land.area
        if ratio > 1.15:
            raise ValueError(
                f"U3.{p['pin']}: copper in mask opening {ratio:.3f} × land"
            )
        results["U3." + p["pin"]] = ratio
    results["u3_foreign_copper_mm2"] = 0.0
    return results


def check_layout(g, directory):
    pcb = directory / (directory.name + ".kicad_pcb")
    if hashlib.sha256(pcb.read_bytes()).hexdigest() != g["pcb_sha256"]:
        raise ValueError(
            "PCB changed since geometry export; run kicad_pcb.py --readback"
        )
    pins = read_pins(g)
    result = check_vias(g, pins, directory / "near-pad-vias.csv")
    result["minimum_edge_gaps_mm"] = minimum_edge_gaps(g)
    if any(d is not None and d < .30 - 1e-6 for d in result["minimum_edge_gaps_mm"].values()):
        raise ValueError(f"Copper reaches board edge clearance: {result['minimum_edge_gaps_mm']}")
    result["pcb_sha256"] = g["pcb_sha256"]
    result.update(check_planes(g, pins))
    result["L3/L4"] = check_inner_crossings(g)
    errors = microphone_report(g, pins, directory / "microphone.csv")
    if errors:
        raise ValueError(f"Unshielded microphone conflicts: {errors[:6]}")
    dock_input_width = min(t["width"] for t in g["tracks"] if t["net"] == pins["J2.10"]["net"])
    if dock_input_width < .40:
        raise ValueError(f"DOCK_5V_IN minimum width {dock_input_width:.3f} mm")
    result["dock_input_minimum_width"] = dock_input_width
    for start, end, width, limit in (
        ("J2.10", "D19.2", 0.40, 15),
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
