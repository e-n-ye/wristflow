"""Inventory short tracks and acute junctions on a saved PCB, in assembly coordinates."""
import argparse
import csv
import hashlib
import itertools
import json
import math
from collections import Counter, defaultdict
from pathlib import Path

from shapely.geometry import Point, Polygon


def nm(point):
    return tuple(round(mm * 1_000_000) for mm in point)


def inventory(geometry):
    edges = defaultdict(list)
    lands = defaultdict(list)
    holes = {(via["net"], nm(via["xy"])) for via in geometry["vias"]}
    for ref, fp in geometry["footprints"].items():
        for pad in fp["pads"]:
            lands[pad["net"], fp["side"]].extend(
                (Polygon(ring), f"{ref}.{pad['pin']}") for ring in pad["polygons"]
            )
    short = []
    for track in geometry["tracks"]:
        side, net = track["side"], track["net"]
        a, b = nm(track["start"]), nm(track["end"])
        dx, dy = b[0] - a[0], b[1] - a[1]
        for start, end in ((a, b), (b, a)):
            edges[side, net, start].append((end, track["width"]))
        if dx * dx + dy * dy >= 120_000 ** 2:
            continue
        points = [Point(track[key]) for key in ("start", "end")]
        touched = sorted({ref for polygon, ref in lands[net, side] if any(polygon.covers(p) for p in points)})
        short.append({"layer": side, "net": net, "start": track["start"], "end": track["end"],
                      "length_mm": math.hypot(dx, dy) / 1e6, "width_mm": track["width"],
                      "pads_at_ends": ", ".join(touched), "via_at_end": any((net, pt) in holes for pt in (a, b))})
    corners = []
    for (side, net, center), neighbors in edges.items():
        for (a, _), (b, _) in itertools.combinations(neighbors, 2):
            ux, uy = a[0] - center[0], a[1] - center[1]
            vx, vy = b[0] - center[0], b[1] - center[1]
            dot = ux * vx + uy * vy
            if dot <= 0:
                continue
            corners.append({"layer": side, "net": net, "xy": [v / 1e6 for v in center],
                            "angle_deg": math.degrees(math.atan2(abs(ux * vy - uy * vx), dot)),
                            "incident_tracks": len(neighbors), "via_at_vertex": (net, center) in holes})
    return short, corners


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("--csv-directory", type=Path)
    args = parser.parse_args()
    board = args.project / (args.project.name + ".kicad_pcb")
    geometry = json.loads((args.project / "geometry.json").read_text(encoding="utf-8"))
    if hashlib.sha256(board.read_bytes()).hexdigest() != geometry["pcb_sha256"]:
        raise ValueError("Refresh geometry.json from the saved PCB")
    for label, rows in zip(("short-segments", "acute-junctions"), inventory(geometry)):
        print(label, dict(sorted(Counter(row["layer"] for row in rows).items())))
        if args.csv_directory and rows:
            with (args.csv_directory / (label + ".csv")).open("w", newline="", encoding="utf-8") as output:
                writer = csv.DictWriter(output, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(rows)
