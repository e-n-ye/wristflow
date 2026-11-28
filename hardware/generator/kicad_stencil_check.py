"""Read an exported KiCad main-board Gerber job with Gerbonara and Shapely."""

import argparse
import csv
import json
from pathlib import Path

from gerbonara import GerberFile
from gerbonara.graphic_objects import Flash
from gerbonara.graphic_primitives import Circle, Line, Rectangle
from shapely.affinity import rotate
from shapely.geometry import LineString, Point, Polygon, box
from shapely.ops import unary_union


def polygon(primitive):
    if isinstance(primitive, Circle):
        return Point(primitive.x, primitive.y).buffer(primitive.r, quad_segs=256)
    if isinstance(primitive, Line):
        return LineString([(primitive.x1, primitive.y1), (primitive.x2, primitive.y2)]).buffer(
            primitive.width / 2, quad_segs=256
        )
    if isinstance(primitive, Rectangle):
        # Preserve the four corners of rotated stencil flashes before union.
        x, y = primitive.x, primitive.y
        half_w, half_h = primitive.w / 2, primitive.h / 2
        aperture = box(x - half_w, y - half_h, x + half_w, y + half_h)
        return rotate(aperture, primitive.rotation, origin=(x, y), use_radians=True)
    outline = primitive.to_arc_poly()
    if any(outline.arc_centers):
        outline = outline.approximate_arcs(0.000001)
    return Polygon(outline.outline)


def inspect_stencil(board, gerbers, output):
    job_file, = gerbers.glob("*.gbrjob")
    job = json.loads(job_file.read_text(encoding="utf-8"))
    finish = job["GeneralSpecs"]["Finish"]
    if finish != "ENIG":
        raise ValueError(f"Gerber job finish: {finish}")
    geometry = json.loads((board / "geometry.json").read_text(encoding="utf-8"))
    rows = []
    for side, extension in (("F", "gtp"), ("B", "gbp")):
        path, = gerbers.glob(f"*.{extension}")
        objects = []
        for obj in GerberFile.open(path).objects:
            shapes = []
            for primitive in obj.to_primitives():
                if not primitive.polarity_dark:
                    raise ValueError(f"Unexpected clear object in {path.name}")
                shapes.append(polygon(primitive))
            objects.append((obj, unary_union(shapes)))
        centers = []
        for ref, footprint in geometry["footprints"].items():
            if footprint["side"] != side:
                continue
            for pad in footprint["pads"]:
                x, y = pad["xy"]
                center = Point(150 + x, y - 110)
                centers.append((f"{ref}.{pad['pin']}", center))
                if ref not in ("U2", "U19", "U14", "U3"):
                    continue
                hits = [obj for obj, shape in objects if shape.covers(center)]
                expected = 0.270 if ref == "U3" else 0.200
                if len(hits) != 1 or not isinstance(hits[0], Flash):
                    raise ValueError(f"{ref}.{pad['pin']}: expected one circular flash, got {len(hits)} objects")
                primitives = list(hits[0].to_primitives())
                if (len(primitives) != 1 or not isinstance(primitives[0], Circle)
                        or abs(2 * primitives[0].r - expected) > 1e-6
                        or center.distance(Point(primitives[0].x, primitives[0].y)) > 1e-6):
                    raise ValueError(f"{ref}.{pad['pin']}: off-centre or wrong aperture")
                disk = polygon(primitives[0])
                if any(shape.intersection(disk).area > 1e-9 for obj, shape in objects if obj is not hits[0]):
                    raise ValueError(f"{ref}.{pad['pin']}: overlapping exposure")
        openings = unary_union([shape for _, shape in objects])
        for opening in openings.geoms:
            pads = sorted(key for key, center in centers if opening.covers(center))
            at = opening.centroid
            size, perimeter = opening.area, opening.length
            rows.append((side, "+".join(pads), round(at.x-150, 5), round(at.y+110, 5),
                         round(size, 7), round(perimeter, 7),
                         round(size/(perimeter*.08), 5), round(size/(perimeter*.10), 5)))
    with output.open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["面","覆盖盘心","X mm","Y mm","开口面积 mm²","周长 mm","面积比 0.08 mm","面积比 0.10 mm"])
        writer.writerows(sorted(rows))
    return len(rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("project", "gerbers", "output"):
        parser.add_argument(name, type=Path)
    args = parser.parse_args()
    print(inspect_stencil(args.project, args.gerbers, args.output), "openings; ENIG")
