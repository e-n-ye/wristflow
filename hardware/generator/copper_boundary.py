"""Board-edge clearance of a disk or a swept track against the saved Edge.Cuts."""
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
