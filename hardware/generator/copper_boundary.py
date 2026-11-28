"""Board-edge clearance of a disk or a swept track against the saved Edge.Cuts."""
import argparse
import json
from pathlib import Path

from shapely.geometry import LineString, Point, Polygon


def board_domain(geometry):
    return Polygon(geometry["outline"], geometry["holes"])


def edge_gap(board, center, radius):
    if not board.covers(center):
        return -radius
    return center.distance(board.boundary) - radius


def minimum_edge_gaps(geometry):
    board = board_domain(geometry)
    tracks = [edge_gap(board, LineString([t["start"], t["end"]]), t["width"]/2)
              for t in geometry["tracks"]]
    vias = [edge_gap(board, Point(v["xy"]), v["diameter"]/2) for v in geometry["vias"]]
    return {"tracks": min(tracks, default=None), "vias": min(vias, default=None)}


def check_candidate(board, routes, minimum=.30):
    for route in routes:
        if "via" in route:
            center, radius = Point(route["via"]), route["diameter"]/2
        else:
            center, radius = LineString(route["path"]), route["width"]/2
        gap = edge_gap(board, center, radius)
        if gap < minimum - 1e-6:
            raise ValueError(f"{route['pad']}: edge clearance {gap:.6f} < {minimum} mm at {center.bounds}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("geometry", type=Path, help="Geometry read back from the candidate board outline")
    parser.add_argument("routes", type=Path, help="Proposed fanout.json to check before generating copper")
    args = parser.parse_args()
    check_candidate(board_domain(json.loads(args.geometry.read_bytes())), json.loads(args.routes.read_bytes()))
