"""Compare each planned net with copper read from a saved R3 rebuild."""
import argparse
import csv
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path

from shapely.geometry import LineString, Point, Polygon


def layer_rows(project, budget):
    geometry = json.loads((project / "geometry.json").read_bytes())
    board = project / (project.name + ".kicad_pcb")
    if hashlib.sha256(board.read_bytes()).hexdigest() != geometry["pcb_sha256"]:
        raise ValueError("Geometry and saved PCB must match")
    components = json.loads((project / "components.json").read_bytes())
    copper = defaultdict(list)
    track_layers = defaultdict(set)
    endpoints = defaultdict(set)
    for segment in geometry["tracks"]:
        net, layer = segment["net"], segment["side"]
        track_layers[net].add(layer)
        copper[net].append((layer, LineString([segment["start"], segment["end"]]), segment["width"] / 2))
    for footprint in geometry["footprints"].values():
        for pad in footprint["pads"]:
            net, layer = pad["net"], footprint["side"]
            endpoints[net].add(layer)
            copper[net].extend((layer, Polygon(ring), 0) for ring in pad["polygons"])
    physical, effective, crossings = Counter(), Counter(), Counter()
    for via in geometry["vias"]:
        net = via["net"]
        center = Point(via["xy"])
        contacts = {layer for layer, shape, radius in copper[net]
                    if shape.distance(center) < radius + via["layer_diameters"][layer] / 2 + 1e-6}
        physical[net] += 1
        effective[net] += len(contacts) >= 2
        crossings[net] += bool(net != "GND" and contacts & {"F", "L3"} and contacts & {"B", "L4"})
    with budget.open(encoding="utf-8") as stream:
        planned = list(csv.DictReader(stream))
    if {r["网名"] for r in planned} != set(components):
        raise ValueError("Plan and PCB must contain the same functional nets")
    for row in planned:
        net = row["网名"]
        allowed = set(row["三_预计用层"].split("/"))
        used = track_layers[net]
        limit = int(row["三_单网孔预算上限"])
        yield {
            "网名": net, "统计类": row["统计类"], "实盘面": "/".join(sorted(endpoints[net])),
            "规划层": row["三_预计用层"], "实铜层": "/".join(sorted(used)), "用层数": len(used),
            "名义孔数": row["三_名义实体孔"], "孔上限": limit, "实体孔": physical[net],
            "有效换层孔": effective[net], "跨参考组孔": crossings[net],
            "规划外层": "/".join(sorted(used - allowed)), "超预算孔": max(0, physical[net] - limit),
            "焊盘分量数": len(components[net]),
        }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--budget", type=Path, default=Path(__file__).resolve().parents[1]
                        / "2026-11-28-scheme3-plan/layer-budget.csv")
    args = parser.parse_args()
    rows = list(layer_rows(args.project, args.budget))
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, rows[0], lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
