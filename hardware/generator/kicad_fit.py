"""Read the KiCad board STEP and placed land patterns against the closed thin assembly.

Use FreeCAD's Python with the same WRISTFLOW_B_WORKSPACE as the EPRO restoration.
"""

import argparse
import csv
import json
import subprocess
from pathlib import Path

import FreeCAD as App
import Part
from kicad_schematic import BOARDS, MAIN_ADDITIONS
from library import parts, transform
from shapely.geometry import Point, Polygon, box
from shapely.ops import unary_union
from workspace import SOURCE

V = App.Vector
BOM = SOURCE.parent / "reviews/2026-11-22-b-r2/placement-BOM.csv"


def extrusion(poly, z, h):
    wire = Part.makePolygon([V(x, y, z) for x, y in poly.exterior.coords])
    return Part.Face(wire).extrude(V(0, 0, h))


def overlap(a, b):
    return a.common(b).Volume if a.BoundBox.intersect(b.BoundBox) else 0


def run(directory, assembly, cli):
    g = json.loads((directory / "geometry.json").read_text(encoding="utf-8"))
    thin = json.loads((assembly / "geometry.json").read_text(encoding="utf-8"))
    doc = App.openDocument(str(next(assembly.glob("*.FCStd")).resolve()))
    closed = Part.read(str(next(assembly.glob("*.step")).resolve()))
    if len(closed.Solids) != 169 or abs(closed.BoundBox.ZLength - thin["total_mm"]) > 0.001:
        raise ValueError("Unexpected closed-assembly STEP")
    # --board-only exports the dielectric body between the outer copper layers.
    dielectric = Part.read(str((directory / "board.step").resolve()))
    faces = [f for f in dielectric.Faces if f.BoundBox.ZLength < 0.00001]
    top = max(faces, key=lambda f: (f.Area, f.CenterOfMass.z))
    rear = thin["board_bottom_mm"]
    thickness = thin["layers_mm"]["board"]
    board = top.translated(V(0, 0, rear - top.CenterOfMass.z)).extrude(V(0, 0, thickness))
    pcb_xy = Polygon(g["outline"], g["holes"])
    if not board.isValid() or len(board.Solids) != 1 or abs(board.Volume / thickness - pcb_xy.area) > 0.1:
        raise ValueError("PCB STEP and artwork disagree")
    if g["copper_layers"] != 6 or abs(g["thickness"] - thickness) > 0.002:
        raise ValueError("Wrong main board stack")
    fixed = {
        "U1": ["ModuleParts", "ModuleSubstrate"],
        "J1": ["BatHeader", "BatPlug"],
        "J201": ["CN1"],
        "J2": ["RearSocket", "RearContactTips"],
        "U16": ["AmbientALS"],
    }
    for i in range(4):
        fixed["K" + str(i + 1)] = ["CrownLeaf" + str(i)]
    for i in range(5):
        fixed["TP" + str(11 + i)] = ["DBG" + str(i)]
    for i in range(2):
        fixed["MT" + str(i + 1)] = ["MotorLand" + str(i)]
    for i, ref in enumerate(("JP1", "R49", "R50")):
        fixed[ref] = [f"I{i}{j}" for j in (0, 1)] + [f"Sleeve{i}{j}" for j in (0, 1)]
    reviewed = {r[2]: r for r in list(csv.reader(BOM.open(encoding="utf-8-sig")))[1:] if r[0] == "main"}
    groups = (
        "Fixed",
        "Display",
        "Circuit",
        "Fasteners",
        "Cell",
        "Mated",
        "CrownAssembly",
        "Lid",
        "Links",
        "Band",
    )
    obstacles = {
        o.Name: o.Shape
        for group in groups
        for o in doc.getObject(group).Group
        if o.Name not in ("Board", "Antenna", "ActiveArea")
    }
    obstacles["MotorSeat"] = doc.MotorSeat.Shape
    preferred_contacts = {
        "U1": {"ShieldPET"},
        "J1": {"Lead0", "Lead2"},
        "J201": {"FlexPlug", "Stiffener", "RearRun"},
        "J2": {"LidFlexNew", "RearFingers", "LidEnd"},
        "U16": {"ALSPipe", "ALSBaffle"},
    }
    for i in range(4):
        preferred_contacts["K" + str(i + 1)] = {f"CrownPad{i}"}
    for i in range(2):
        preferred_contacts["MT" + str(i + 1)] = {f"MotorWire{i}"}
    for i, ref in enumerate(("JP1", "R49", "R50")):
        preferred_contacts[ref] = {"KeyBody", f"Short{i}"}
    battery = doc.Battery.Shape.BoundBox
    projection = box(battery.XMin, battery.YMin, battery.XMax, battery.YMax)
    swollen_ntc = doc.OptionalNTC.Shape.translated(V(0, 0, 0.50))
    solids, envelopes, battery_items, near, ntc, clashes = {}, {}, {}, {}, {}, []
    wire_contacts, lateral = {}, {}
    for r, f in g["footprints"].items():
        side, x, y, rot = f["side"], *f["xy"], f["degrees"]
        part = parts[MAIN_ADDITIONS[r]["copy_part"] if r in MAIN_ADDITIONS else r]
        env = transform(part["env"], x, y, rot, side).envelope
        h = part["height"]
        envelopes[r] = [side, *env.bounds, h]
        poly = transform(part["body"], x, y, rot, side)
        if r in fixed:
            old = reviewed[r]
            angle = 270 if r == "J201" else float(old[14])
            if (
                side != old[1]
                or abs(x - float(old[12])) > 0.001
                or abs(y - float(old[13])) > 0.001
                or abs((rot - angle + 180) % 360 - 180) > 0.001
            ):
                raise ValueError(f"{r}: fixed interface has moved")
            solid = Part.makeCompound([doc.getObject(n).Shape for n in fixed[r]])
            if r == "U16":
                solid = extrusion(poly, rear + thickness, h)
        else:
            solid = extrusion(poly, rear + thickness if side == "F" else rear, h if side == "F" else -h)
            if not pcb_xy.buffer(0.004).covers(env):
                clashes.append([r, "off-board land/body envelope"])
        solids[r] = solid
        if side == "B" and projection.intersects(env):
            if r == "J201":
                h = rear - doc.Stiffener.Shape.BoundBox.ZMin
            gap = rear - h - doc.Separator.Shape.BoundBox.ZMax
            battery_items[r] = [round(h, 4), round(gap, 4)]
            if h > 0.96001 or gap < 0.69999:
                clashes.append([r, "battery height", h, gap])
            ntc[r] = round(solid.distToShape(swollen_ntc)[0], 5)
            if ntc[r] < 0.19999:
                clashes.append([r, "swollen optional NTC", ntc[r]])
        excluded = set(fixed.get(r, []))
        candidates = []
        for name, body in obstacles.items():
            if name in excluded:
                continue
            # AABB lower bound lets us evaluate only structures which could be nearest.
            a, b = solid.BoundBox, body.BoundBox
            lower = (
                sum(
                    max(
                        0,
                        getattr(a, ax + "Min") - getattr(b, ax + "Max"),
                        getattr(b, ax + "Min") - getattr(a, ax + "Max"),
                    )
                    ** 2
                    for ax in "XYZ"
                )
                ** 0.5
            )
            candidates.append((lower, name, body))
        best = (float("inf"), "")
        for lower, name, body in sorted(candidates, key=lambda v: v[0]):
            if lower > best[0] + 0.00001:
                break
            vol = overlap(solid, body)
            if r == "J1" and name in ("Lead0", "Lead2"):
                # The harness conductors terminate inside the mated plug model.
                wire_contacts[name] = round(overlap(doc.BatPlug.Shape, body), 6)
                vol = overlap(doc.BatHeader.Shape, body)
            if vol > 0.00001:
                clashes.append([r, name, round(vol, 6)])
            if name not in preferred_contacts.get(r, set()):
                best = min(best, (solid.distToShape(body)[0], name))
        near[r] = [best[1], round(best[0], 5)]
        # Offset horizontally by 0.30 mm across the entire component, including
        # structures hidden by the nearest object. Rear flex also gets its 0.05 mm Z tolerance.
        lateral[r] = {}
        for name in ("Frame","Clamp1","Clamp2","Clamp3","RearRun","Crossing"):
            if name in preferred_contacts.get(r, set()):
                continue
            obstacle = obstacles[name]
            margin_z = 0.055 if name == "RearRun" else 0.001
            bounds, other = solid.BoundBox, obstacle.BoundBox
            if bounds.ZMax < other.ZMin-margin_z or bounds.ZMin > other.ZMax+margin_z:
                continue
            distance, pairs, _ = solid.distToShape(obstacle)
            if distance <= 0.36:
                if r not in fixed:
                    # A whole-body XY sweep catches another wall even when the
                    # closest pair returned by OCC is on a horizontal shelf.
                    guard = extrusion(poly.buffer(0.3001, quad_segs=32),
                                      bounds.ZMin-margin_z, bounds.ZLength+2*margin_z)
                    if overlap(guard, obstacle) > 0.000001:
                        clashes.append([r, name, "0.30 mm lateral envelope"])
                dx = min(((a.x-b.x)**2+(a.y-b.y)**2)**0.5 for a,b in pairs)
                dz = min(abs(a.z-b.z) for a,b in pairs)
                if dz <= margin_z:
                    lateral[r][name] = round(dx,5)
                    if dx < 0.29999:
                        clashes.append([r, name, "lateral clearance", round(dx,5)])
    refs = sorted(solids)
    for i, r in enumerate(refs):
        for other in refs[i + 1 :]:
            vol = overlap(solids[r], solids[other])
            if vol > 0.00001:
                clashes.append([r, other, round(vol, 6)])
    pad_shapes = unary_union(
        [Polygon(p) for f in g["footprints"].values() for pad in f["pads"] for p in pad["polygons"]]
    )
    b2 = next(p for p in g["footprints"]["U3"]["pads"] if p["pin"] == "B2")
    in_pad = [v for v in g["vias"] if v["xy"] == b2["xy"] and v["net"] == b2["net"]]
    if len(in_pad) != 1 or (in_pad[0]["diameter"], in_pad[0]["drill"]) != (0.26,0.15):
        raise ValueError("Missing capped U3.B2 through via")
    ordinary = [v for v in g["vias"] if v not in in_pad]
    via_gap = min(Point(v["xy"]).distance(pad_shapes) - v["diameter"] / 2 for v in ordinary)
    if via_gap <= 0 or any((v["diameter"],v["drill"]) not in ((0.50,0.30),(0.40,0.20)) for v in ordinary):
        raise ValueError("Unexpected via in SMD land or drill size")
    board_clash = {
        n: round(overlap(board, obstacles[n]), 6)
        for n in (
            "Frame",
            "MotorSeat",
            "Daughter",
            "CrownBridge",
            "Motor",
            "Speaker",
            "Clamp1",
            "Clamp2",
            "Clamp3",
        )
    }
    clashes.extend(["board", n, v] for n, v in board_clash.items() if v > 0.0001)
    antenna = box(-15.5, -22.2, 2.5, -16.1)
    for ref, (_side, x0, y0, x1, y1, _height) in envelopes.items():
        if ref != "U1" and antenna.intersects(box(x0, y0, x1, y1)):
            clashes.append([ref, "module antenna footprint keepout"])
    copper = unary_union([pad_shapes, *[Point(v["xy"]).buffer(v["diameter"] / 2) for v in g["vias"]]])
    from shapely.geometry import LineString

    copper = unary_union(
        [copper, *[LineString([t["start"], t["end"]]).buffer(t["width"] / 2) for t in g["tracks"]]]
    )
    if antenna.intersects(copper) or antenna.intersection(pcb_xy).area > 0.00001:
        raise ValueError("Antenna clearance")
    results = {
        "step_solids": len(closed.Solids),
        "assembly_height": closed.BoundBox.ZLength,
        "board_area": top.Area,
        "exported_body_thickness": dielectric.BoundBox.ZLength,
        "board_z": [rear, rear + thickness],
        "pad_to_via_land_min": round(via_gap, 5),
        "antenna_to_copper": round(antenna.distance(copper), 5),
        "enclosed_battery": battery_items,
        "swollen_ntc": ntc,
        "nearest": near,
        "lateral_near_mm": lateral,
        "collisions": clashes,
        "envelopes": envelopes,
        "board_obstacles": board_clash,
        "j1_wire_to_plug_mm3": wire_contacts,
        "als_to_lightguide": round(solids["U16"].distToShape(doc.ALSPipe.Shape)[0], 5),
    }
    # Preserve the checked assembly orientation in the local review STEP.
    Part.makeCompound([board, *solids.values()]).exportStep(
        str((directory / "placed-envelopes.step").resolve())
    )
    # Native KiCad 3D viewer resolves these project-local models. Each license-
    # bounded land pattern has its own model in footprint-local coordinates.
    for project in BOARDS.values():
        path = directory.parent / project / "geometry.json"
        other = json.loads(path.read_text(encoding="utf-8"))
        models = path.parent / "models"
        models.mkdir(exist_ok=True)
        written = set()
        for ref, f in sorted(other["footprints"].items()):
            land = f["land"]
            if land in written:
                continue
            if path.parent == directory:
                x, y = f["xy"]
                front = f["side"] == "F"
                local = solids[ref].translated(V(-x, -y, -rear - (thickness if front else 0)))
                local.rotate(V(0, 0, 0), V(0, 0, 1), -f["degrees"])
                if not front:
                    local.rotate(V(0, 0, 0), V(1, 0, 0), 180)
            else:
                local = extrusion(parts[ref]["body"], 0, parts[ref]["height"])
            local.exportStep(str((models / (land + ".step")).resolve()))
            written.add(land)
    from kicad_views import render

    render(directory, g, results)
    subprocess.run(
        [
            cli,
            "pcb",
            "render",
            "--width",
            "1500",
            "--height",
            "1150",
            "--quality",
            "high",
            "--rotate",
            "325,0,335",
            "--background",
            "opaque",
            "--zoom",
            ".83",
            "-o",
            directory / "assembly-3d.png",
            directory / (directory.name + ".kicad_pcb"),
        ],
        check=True,
    )
    (directory / "fit.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    print("Clashes", clashes, "under battery", len(battery_items), "via edge gap", via_gap)
    if clashes:
        raise ValueError("See fit.json")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("project", type=Path)
    ap.add_argument("assembly", type=Path)
    ap.add_argument("--cli", default="kicad-cli")
    a = ap.parse_args()
    run(a.project, a.assembly, a.cli)
