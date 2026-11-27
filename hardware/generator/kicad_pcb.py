"""Build the four editable PCB projects from reviewed land patterns and KiCad netlists.

Run with KiCad 10's Python. Assembly coordinates are in mm, +Y towards J1.
"""

import argparse
import csv
import hashlib
import json
import math
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

import pcbnew as pcb
from kicad_schematic import BOARDS, MAIN_ADDITIONS, cross_check, equivalence, read_board, read_bom, strip
from kicad_sexpr import quote

DATA = Path(__file__).resolve().parents[1] / "boards/b-r2-main"
mm = pcb.FromMM


def point(x, y):
    return pcb.VECTOR2I(mm(150 + x), mm(110 - y))


def assembly(pt):
    return [round(pcb.ToMM(pt.x) - 150, 6), round(110 - pcb.ToMM(pt.y), 6)]


def revise_lands(board, which):
    if which == "main":
        for ref, spec in MAIN_ADDITIONS.items():
            f = pcb.Cast_to_FOOTPRINT(board.FindFootprintByReference(spec["copy_part"]).Duplicate(False))
            f.SetReference(ref)
            for pad in f.Pads():
                pad.SetNet(board.FindNet(spec["nets"][pad.GetNumber()]))
            board.Add(f)
        u3 = board.FindFootprintByReference("U3")
        for ref in ("U3", "U14", "U2", "U19"):
            fp = board.FindFootprintByReference(ref)
            for item in list(fp.GraphicalItems()):
                if item.GetLayer() in (pcb.F_Paste, pcb.B_Paste):
                    fp.Delete(item)
        for pad in u3.Pads():
            row, column = pad.GetNumber()
            pad.SetPosition(u3.GetPosition() + pcb.VECTOR2I(mm((int(column)-2)*0.5), mm(("ABC".index(row)-1)*0.5)))
            pad.SetShape(pcb.PAD_SHAPE_CIRCLE)
            pad.SetSize(pcb.VECTOR2I(mm(0.26), mm(0.26)))
            pad.SetLocalSolderMaskMargin(mm(0.05))
            pad.SetLocalSolderPasteMargin(mm(0.005))
        for ref, pin in (("C6","1"), ("C7","1"), ("C8","1"), ("R64","2")):
            pad = board.FindFootprintByReference(ref).FindPadByNumber(pin)
            pad.SetLocalZoneConnection(pcb.ZONE_CONNECTION_THERMAL)
        module = board.FindFootprintByReference("U1")
        for item in module.GraphicalItems():
            if item.GetLayer() == pcb.F_SilkS and item.GetBoundingBox().GetBottom() >= point(0, -15).y:
                item.SetLayer(pcb.F_Fab)
        connector = board.FindFootprintByReference("J201")
        for a, b in ((25,27),(26,28),(33,35),(34,36)):
            # Join the overlapping breadth of the two openings across the 0.08 mm web.
            pa, pb = [connector.FindPadByNumber(str(n)).GetBoundingBox() for n in (a,b)]
            shape = pcb.PCB_SHAPE(connector)
            shape.SetShape(pcb.SHAPE_T_RECT)
            shape.SetStart(pcb.VECTOR2I(max(pa.GetLeft(), pb.GetLeft()), min(pa.GetCenter().y, pb.GetCenter().y)))
            shape.SetEnd(pcb.VECTOR2I(min(pa.GetRight(), pb.GetRight()), max(pa.GetCenter().y, pb.GetCenter().y)))
            shape.SetWidth(0)
            shape.SetFilled(True)
            shape.SetLayer(pcb.B_Mask)
            connector.Add(shape)
    no_paste = {"JF1","CH1","CH2","SP1","SP2","MT1","MT2", *(f"P{i}" for i in range(1,5)),
                *(f"TP{i}" for i in range(11,16))}
    for f in board.GetFootprints():
        if f.GetReference() in no_paste:
            for pad in f.Pads():
                mask = pad.GetLayerSet()
                mask.RemoveLayer(pcb.F_Paste)
                mask.RemoveLayer(pcb.B_Paste)
                pad.SetLayerSet(mask)


def link_board(board, netlist, directory):
    tree = ET.parse(netlist)
    symbols = {
        c.get("ref"): c
        for c in tree.findall(".//components/comp")
        if c.find('property[@name="exclude_from_board"]') is None
    }
    fps = {f.GetReference(): f for f in board.GetFootprints()}
    if fps.keys() != symbols.keys():
        raise ValueError(f"Board/schematic references differ: {fps.keys() ^ symbols.keys()}")
    pads = {
        (n.get("ref"), n.get("pin")): (net.get("name"), n)
        for net in tree.findall(".//nets/net")
        for n in net.findall("node")
    }
    lib = directory / "WF_B.pretty"
    lib.mkdir()
    for ref, f in sorted(fps.items()):
        c = symbols[ref]
        f.SetFPIDAsString(c.findtext("footprint"))
        land = c.findtext("footprint").split(":", 1)[1]
        label = f.Reference()
        label.SetPosition(f.GetPosition())
        label.SetLayer(pcb.B_Fab if f.IsFlipped() else pcb.F_Fab)
        label.SetTextSize(pcb.VECTOR2I(mm(0.45), mm(0.45)))
        label.SetTextThickness(mm(0.08))
        label.SetHorizJustify(pcb.GR_TEXT_H_ALIGN_CENTER)
        label.SetVertJustify(pcb.GR_TEXT_V_ALIGN_CENTER)
        label.SetVisible(True)
        f.Value().SetVisible(False)
        f.Models().clear()
        model = pcb.FP_3DMODEL()
        model.m_Filename = "${KIPRJMOD}/models/" + land + ".step"
        f.Add3DModel(model)
        if not (lib / (land + ".kicad_mod")).exists():
            template = pcb.FOOTPRINT(f)
            template.SetReference("REF**")
            template.SetValue(land)
            pcb.PCB_IO_KICAD_SEXPR().FootprintSave(str(lib.resolve()), template)
        f.SetPath(pcb.KIID_PATH(c.find("sheetpath").get("tstamps") + c.findtext("tstamps")))
        f.SetValue(c.findtext("value"))
        for prop in c.findall("property"):
            key, value = prop.get("name"), prop.get("value", "")
            if key == "Sheetname":
                f.SetSheetname(c.find("sheetpath").get("names"))
            elif key == "Sheetfile":
                f.SetSheetfile(value)
            elif key == "dnp":
                f.SetDNP(True)
            else:
                f.SetField(key, value)
                f.GetField(key).SetVisible(False)
                f.GetField(key).SetPosition(f.GetPosition())
        no_connects = Counter()
        for p in f.Pads():
            if not p.GetNumber():
                continue
            name, node = pads[ref, p.GetNumber()]
            reviewed = "" if name.startswith("unconnected-") else strip(name)
            if reviewed != p.GetNetname():
                raise ValueError(f"{ref}.{p.GetNumber()}: imported {p.GetNetname()} vs {name}")
            if name.startswith("unconnected-"):
                # F8 gives each unconnected physical land its own network,
                # including the two mechanical tabs with the same pin number.
                ordinal = no_connects[name]
                no_connects[name] += 1
                if ordinal:
                    name = f"{name}_{ordinal}"
            net = board.FindNet(name)
            if not net:
                net = pcb.NETINFO_ITEM(board, name)
                board.Add(net)
            p.SetNet(net)
            p.SetPinFunction(node.get("pinfunction", ""))
            p.SetPinType(node.get("pintype", ""))
    (directory / "fp-lib-table").write_text(
        '(fp_lib_table (version 7) (lib (name "WF_B")(type "KiCad")'
        '(uri "${KIPRJMOD}/WF_B.pretty")(options "")(descr "B-R2 reviewed local land patterns")))\n',
        encoding="utf-8",
    )
    return fps


def trial_tracks(board, fps):
    layers = {"F": pcb.F_Cu, "B": pcb.B_Cu, "L3": pcb.In2_Cu, "L4": pcb.In3_Cu}
    ldo = pcb.PCB_GROUP(board)
    ldo.SetName("VSYS_1 LDO branches")
    board.Add(ldo)
    for route in json.loads((DATA / "fanout.json").read_text()):
        ref, pin = route["pad"].split(".")
        pad = fps[ref].FindPadByNumber(pin)
        if "via" in route:
            via = pcb.PCB_VIA(board)
            via.SetViaType(pcb.VIATYPE_THROUGH)
            via.SetPosition(point(*route["via"]))
            via.SetWidth(mm(route["diameter"]))
            via.SetDrill(mm(route["drill"]))
            via.SetNet(pad.GetNet())
            via.SetRemoveUnconnected(True)
            via.SetKeepStartEnd(True)
            via.SetFillingMode(pcb.FILLING_MODE_FILLED)
            via.SetCappingMode(pcb.CAPPING_MODE_CAPPED)
            if route["pad"] == "U3.B2":
                via.SetFrontTentingMode(pcb.TENTING_MODE_NOT_TENTED)
            board.Add(via)
            continue
        for start, end in zip(route["path"], route["path"][1:]):
            track = pcb.PCB_TRACK(board)
            track.SetStart(point(*start))
            track.SetEnd(point(*end))
            track.SetLayer(layers[route["layer"]])
            track.SetWidth(mm(route["width"]))
            track.SetNet(pad.GetNet())
            board.Add(track)
            if route["pad"] in ("U4.1", "U15.1", "C9.2", "C33.2"):
                ldo.AddItem(track)


def ground_plane(board, layer, name, corners, clearance):
    zone = pcb.ZONE(board)
    zone.SetZoneName(name)
    zone.SetLayer(layer)
    zone.SetNet(board.FindNet("GND"))
    zone.SetLocalClearance(mm(clearance))
    zone.SetPadConnection(pcb.ZONE_CONNECTION_FULL)
    if layer == pcb.F_Cu:
        zone.SetThermalReliefGap(mm(0.18))
        zone.SetThermalReliefSpokeWidth(mm(0.16))
    zone.SetMinThickness(mm(0.15))
    zone.Outline().NewOutline()
    for x, y in corners:
        zone.Outline().Append(point(x, y))
    board.Add(zone)


def reserve(board, name, corners, layers, copper):
    z = pcb.ZONE(board)
    z.SetIsRuleArea(True)
    z.SetZoneName(name)
    z.SetLayerSet(layers)
    z.SetDoNotAllowTracks(copper)
    z.SetDoNotAllowVias(copper)
    z.SetDoNotAllowPads(copper)
    z.SetDoNotAllowZoneFills(copper)
    z.SetDoNotAllowFootprints(False)
    z.Outline().NewOutline()
    for x, y in corners:
        z.Outline().Append(point(x, y))
    board.Add(z)


def main_layout(board, fps):
    board.SetCopperLayerCount(6)
    board.GetDesignSettings().m_FillVias = True
    board.GetDesignSettings().m_CapVias = True
    for layer, name in ((pcb.In1_Cu,"GND-L2"),(pcb.In2_Cu,"Display-L3"),
                        (pcb.In3_Cu,"Analog-Power-L4"),(pcb.In4_Cu,"GND-L5")):
        board.SetLayerName(layer, name)
    with (DATA / "layout.csv").open(encoding="utf-8") as stream:
        for r in csv.DictReader(stream):
            f = fps[r["ref"]]
            if f.IsFlipped() != (r["side"] == "B"):
                f.Flip(f.GetPosition(), pcb.FLIP_DIRECTION_TOP_BOTTOM)
            f.SetOrientationDegrees(float(r["degrees"]))
            f.SetPosition(point(float(r["x"]), float(r["y"])))
    for ref in [
        "U1",
        "J201",
        "J1",
        "J2",
        "U16",
        "K1",
        "K2",
        "K3",
        "K4",
        "MT1",
        "MT2",
        "JP1",
        "R49",
        "R50",
        "TP11",
        "TP12",
        "TP13",
        "TP14",
        "TP15",
    ]:
        fps[ref].SetLocked(True)
    reserve(
        board,
        "Module antenna / all copper layers",
        [(-15.5, -22.2), (2.5, -22.2), (2.5, -16.1), (-15.5, -16.1)],
        pcb.LSET.AllCuMask(6),
        True,
    )
    reserve(
        board,
        "Battery projection / B height 0.96 mm / see assembly check",
        [(-17, -20.4), (15, -20.4), (15, 10.1), (-17, 10.1)],
        pcb.LSET().AddLayer(pcb.B_Cu),
        False,
    )
    surfaces = pcb.LSET().AddLayer(pcb.F_Cu).AddLayer(pcb.B_Cu)
    # Reserve the west approach to both removable 3V3 bridge lands.
    for pad in fps["R66"].Pads():
        bounds = pad.GetBoundingBox()
        x, y = assembly(bounds.GetOrigin())
        bottom = y - pcb.ToMM(bounds.GetHeight())
        reserve(board, f"R66.{pad.GetNumber()} soldering access",
                [(x-1, bottom+.07), (x, bottom+.07), (x, y-.07), (x-1, y-.07)],
                pcb.LSET().AddLayer(pcb.F_Cu), True)
    for i, (cx, cy) in enumerate([(5.15, -9.8), (14.55, 9.7), (-15.5, 8.4)], 1):
        corners = [
            (cx + 1.522 * math.cos(t * math.tau / 64), cy + 1.522 * math.sin(t * math.tau / 64))
            for t in range(64)
        ]
        reserve(board, f"Clamp {i} / head and bearing surface", corners, surfaces, True)
    trial_tracks(board, fps)
    for layer in (pcb.In1_Cu, pcb.In4_Cu):
        ground_plane(board, layer, board.GetLayerName(layer), [(-19,-22),(19,-22),(19,23),(-19,23)], 0.20)
    x, y = assembly(fps["U3"].GetPosition())
    cx, cy = assembly(fps["U3"].FindPadByNumber("C3").GetPosition())
    keepout = pcb.ZONE(board)
    keepout.SetZoneName("U3 C3 NSMD opening")
    keepout.SetIsRuleArea(True)
    keepout.SetLayer(pcb.F_Cu)
    keepout.SetDoNotAllowZoneFills(True)
    keepout.SetDoNotAllowPads(False)
    keepout.SetDoNotAllowTracks(False)
    keepout.SetDoNotAllowVias(False)
    keepout.SetDoNotAllowFootprints(False)
    keepout.Outline().NewOutline()
    for i in range(64):
        angle = math.tau * i / 64
        keepout.Outline().Append(point(cx + 0.25*math.cos(angle), cy + 0.25*math.sin(angle)))
    board.Add(keepout)
    ground_plane(board, pcb.F_Cu, "U3 thermal ground", [(x-1.5,y-2),(x+5,y-2),(x+5,y+4.5),(x-1.5,y+4.5)], 0.15)


def geometry(board):
    outline = pcb.SHAPE_POLY_SET()
    if not board.GetBoardPolygonOutlines(outline, False) or outline.OutlineCount() != 1:
        raise ValueError("Invalid board outline")

    def ring(chain):
        return [assembly(chain.CPoint(i)) for i in range(chain.PointCount())]

    data = {}
    for f in board.GetFootprints():
        data[f.GetReference()] = {
            "side": "B" if f.IsFlipped() else "F",
            "xy": assembly(f.GetPosition()),
            "land": f.GetFPIDAsString().split(":", 1)[1],
            "degrees": f.GetOrientationDegrees(),
            "pads": [],
        }
        for p in f.Pads():
            shape = p.GetEffectivePolygon(f.GetLayer())
            mask = pcb.SHAPE_POLY_SET()
            cu = f.GetLayer()
            if p.IsOnLayer(pcb.B_Mask if f.IsFlipped() else pcb.F_Mask):
                p.TransformShapeToPolygon(mask, cu, p.GetSolderMaskExpansion(cu), 500, pcb.ERROR_OUTSIDE)
            data[f.GetReference()]["pads"].append(
                {
                    "pin": p.GetNumber(),
                    "net": p.GetNetname(),
                    "xy": assembly(p.GetPosition()),
                    "polygons": [ring(shape.COutline(i)) for i in range(shape.OutlineCount())],
                    "mask": [ring(mask.COutline(i)) for i in range(mask.OutlineCount())],
                    "circle_radius": pcb.ToMM(p.GetSizeX())/2 if p.GetShape(cu) == pcb.PAD_SHAPE_CIRCLE else None,
                    "thermal": p.GetLocalZoneConnection() == pcb.ZONE_CONNECTION_THERMAL,
                }
            )
    tracks, vias = [], []
    for t in board.GetTracks():
        if isinstance(t, pcb.PCB_VIA):
            if t.GetViaType() != pcb.VIATYPE_THROUGH or (t.TopLayer(), t.BottomLayer()) != (
                pcb.F_Cu,
                pcb.B_Cu,
            ):
                raise ValueError("Unexpected blind, buried, or microvia")
            vias.append(
                {
                    "xy": assembly(t.GetPosition()),
                    "diameter": pcb.ToMM(t.GetWidth(pcb.F_Cu)),
                    "drill": pcb.ToMM(t.GetDrillValue()),
                    "layer_diameters": {
                        side: pcb.ToMM(t.GetWidth(layer) if t.FlashLayer(layer) else t.GetDrillValue())
                        for side, layer in (("F", pcb.F_Cu), ("B", pcb.B_Cu),
                                            ("L3", pcb.In2_Cu), ("L4", pcb.In3_Cu))
                    },
                    "net": t.GetNetname(),
                    "filling": t.GetFillingMode() == pcb.FILLING_MODE_FILLED,
                    "capping": t.GetCappingMode() == pcb.CAPPING_MODE_CAPPED,
                    "remove_unconnected": t.GetRemoveUnconnected(),
                    "keep_outer_rings": t.GetKeepStartEnd(),
                }
            )
        else:
            layer = {pcb.F_Cu: "F", pcb.B_Cu: "B", pcb.In2_Cu: "L3", pcb.In3_Cu: "L4"}[t.GetLayer()]
            tracks.append(
                {
                    "start": assembly(t.GetStart()),
                    "end": assembly(t.GetEnd()),
                    "net": t.GetNetname(),
                    "side": layer,
                    "width": pcb.ToMM(t.GetWidth()),
                }
            )
    mask_graphics = []
    module_court = []
    module = board.FindFootprintByReference("U1")
    if module and board.GetCopperLayerCount() == 6:
        court = module.GetCourtyard(pcb.F_CrtYd)
        module_court = [ring(court.COutline(i)) for i in range(court.OutlineCount())]
    fills, fill_layers, plane_keepouts = {}, {}, []
    for zone in board.Zones():
        if zone.GetIsRuleArea():
            if (zone.GetZoneName() == "Module antenna / all copper layers"
                    and zone.IsOnLayer(pcb.In1_Cu) and zone.GetDoNotAllowZoneFills()):
                boundary = zone.Outline()
                plane_keepouts.extend(ring(boundary.COutline(i)) for i in range(boundary.OutlineCount()))
            continue
        layer = zone.GetLayer()
        fill_layers[zone.GetZoneName()] = [pcb.LayerName(layer), zone.GetNetname()]
        polys = zone.GetFilledPolysList(layer)
        fills[zone.GetZoneName()] = [
            {"outer": ring(polys.COutline(i)),
             "holes": [ring(polys.CHole(i, j)) for j in range(polys.HoleCount(i))]}
            for i in range(polys.OutlineCount())
        ]
    for f in board.GetFootprints():
        for item in f.GraphicalItems():
            if item.GetLayer() in (pcb.F_Mask, pcb.B_Mask):
                shape = pcb.SHAPE_POLY_SET()
                item.TransformShapeToPolygon(shape, item.GetLayer(), 0, 500, pcb.ERROR_OUTSIDE)
                mask_graphics.extend(ring(shape.COutline(i)) for i in range(shape.OutlineCount()))
    return {
        "mask_graphics": mask_graphics,
        "module_court": module_court,
        "zone_fills": fills,
        "zone_layers": fill_layers,
        "plane_keepouts": plane_keepouts,
        "outline": ring(outline.COutline(0)),
        "holes": [ring(outline.CHole(0, i)) for i in range(outline.HoleCount(0))],
        "footprints": data,
        "tracks": tracks,
        "vias": vias,
        "copper_layers": board.GetCopperLayerCount(),
        "fill_vias": bool(board.GetDesignSettings().m_FillVias),
        "cap_vias": bool(board.GetDesignSettings().m_CapVias),
        "thickness": pcb.ToMM(board.GetDesignSettings().GetBoardThickness()),
    }


def read_saved_board(path):
    board = pcb.LoadBoard(str(path))
    data = geometry(board)
    data["pcb_sha256"] = hashlib.sha256(path.read_bytes()).hexdigest()
    (path.parent / "geometry.json").write_text(json.dumps(data), encoding="utf-8")
    return board


def stackup():
    # JLC060811-3313 (20220413072136), two 0.109 mm 2116 plies at the midplane.
    names = ["F.Cu", "In1.Cu", "In2.Cu", "In3.Cu", "In4.Cu", "B.Cu"]
    outer = ("prepreg", 0.092, "3313 RC57%", 4.10)
    core = ("core", 0.100, "FR4", 4.17)
    dielectric = [outer, core, ("prepreg", 0.218, "2 x 2116 RC54%", 4.16), core, outer]
    layers = []
    for i, name in enumerate(names):
        layers.append(f'(layer "{name}" (type "copper") (thickness {0.035 if i in (0, 5) else 0.030}))')
        if i < len(dielectric):
            kind, thick, material, dk = dielectric[i]
            layers.append(
                f'(layer "dielectric {i + 1}" (type "{kind}") (thickness {thick}) '
                f'(material "{material}") (epsilon_r {dk}))'
            )
    return "(stackup\n" + "\n".join(layers) + '\n(copper_finish "ENIG"))'


def check_screen(board, reference):
    socket = board.FindFootprintByReference("J201")
    pads = {pad.GetNumber(): pad for pad in socket.Pads()}
    if (not socket.IsFlipped() or set(pads) != set(map(str, range(1, 41)))
            or assembly(pads["1"].GetPosition()) != [9.665, 7.06]):
        raise ValueError("J201 registration disagrees with the original display")
    if reference is None:
        return
    official = pcb.LoadBoard(str(reference))
    windows = [z.GetBoundingBox() for z in official.Zones()
               if abs(pcb.ToMM(z.GetBoundingBox().GetWidth())-14) < 0.01
               and abs(pcb.ToMM(z.GetBoundingBox().GetHeight())-3) < 0.01]
    window, = windows
    anchor = window.GetCenter()
    cn1 = official.FindFootprintByReference("CN1")
    if not cn1.IsFlipped() or len(list(cn1.Pads())) != 40:
        raise ValueError("Unexpected official display connector")
    for pad in cn1.Pads():
        delta = pad.GetPosition() - anchor
        expected = [-0.455 + pcb.ToMM(delta.x), 9.990 - pcb.ToMM(delta.y)]
        actual = assembly(pads[pad.GetNumber()].GetPosition())
        if math.dist(expected, actual) > 0.01:
            raise ValueError(f"J201.{pad.GetNumber()}: expected {expected}, got {actual}")


def check_fanout(board):
    """Walk actual connected copper, including vias, after reloading the saved board."""
    board.BuildConnectivity()
    connectivity = board.GetConnectivity()
    footprints = {f.GetReference(): f for f in board.GetFootprints()}
    u3 = footprints["U3"]
    if any(shape.GetLayer() == pcb.F_Paste for shape in u3.GraphicalItems()):
        raise ValueError("Extra U3 stencil aperture")
    for pad in u3.Pads():
        margin = pad.GetSolderPasteMargin(pcb.F_Cu)
        if (not pad.IsOnLayer(pcb.F_Paste) or pad.GetShape(pcb.F_Cu) != pcb.PAD_SHAPE_CIRCLE
                or (pad.GetSizeX()+2*margin.x, pad.GetSizeY()+2*margin.y) != (mm(.27),mm(.27))):
            raise ValueError(f"U3.{pad.GetNumber()}: expected a single round 0.27 mm paste aperture")
    targets = {
        "J201": [str(i) for i in range(1, 24)],
        "U3": [p.GetNumber() for p in footprints["U3"].Pads()],
        "U1": [
            p.GetNumber()
            for p in footprints["U1"].Pads()
            if int(p.GetNumber()) <= 48
            and p.GetNetname() != "GND"
            and not p.GetNetname().startswith("unconnected-")
        ],
    }
    companions = {
        "U3.A1": "C3.2",
        "U3.A2": "C6.2",
        "U3.A3": "C8.2",
        "U3.B3": "C7.2",
        "U1.39": "R17.1",
        "U1.40": "R16.1",
        "U1.41": "R20.1",
        "U1.42": "R19.1",
        "U1.43": "R18.1",
    }
    for pin, resistor in enumerate(("R18", "R19", "R20", "R16", "R17"), 8):
        companions[f"J201.{pin}"] = resistor + ".2"
    companions.update({"U3.B1":"R60.2", "U3.C1":"R43.1", "U3.C2":"R42.1", "U3.C3":"C3.1", "U3.B2":"Q1.3", "U1.37":"U9.1", "U1.36":"R30.1"})
    for ref in ("C26","C27","U9","L4","C41"):
        targets[ref] = [pad.GetNumber() for pad in footprints[ref].Pads()]
    companions.update({"U9.4":"R30.2", "C26.2":"U9.4", "C27.2":"U9.1", "C41.2":"U1.61"})
    companions.update({"C2.1":"U3.C3", "C33.1":"U3.C3", "C42.1":"U3.C3", "C42.2":"U17.8",
                       "C22.1":"L3.2", "C22.2":"J2.2", "C23.1":"J2.1", "C23.2":"J2.2",
                       "C25.1":"J2.1", "C25.2":"L3.2"})
    for ref in ("C2","C33","C42","C22","C23","C25"):
        targets[ref] = [pad.GetNumber() for pad in footprints[ref].Pads()]
    for pin in ("C26.1","C27.1","U9.2","U9.3"):
        companions[pin] = "L4.2"
    inner = {f"J201.{i}" for i in range(8, 13)}
    result = {}
    for ref, pins in targets.items():
        for number in pins:
            key = ref + "." + number
            start = footprints[ref].FindPadByNumber(number)
            connected = connectivity.GetConnectedItems(start)
            pads = []
            vias, inner_count = 0, 0
            planes = []
            for item in connected:
                if item.Type() == pcb.PCB_PAD_T:
                    pad = pcb.Cast_to_PAD(item)
                    pads.append(pad.GetParentFootprint().GetReference() + "." + pad.GetNumber())
                elif item.Type() == pcb.PCB_VIA_T:
                    vias += 1
                elif item.Type() == pcb.PCB_TRACE_T and item.GetLayer() == pcb.In2_Cu:
                    inner_count += 1
                elif item.Type() == pcb.PCB_ZONE_T:
                    planes.append(pcb.Cast_to_ZONE(item).GetZoneName())
            if (
                (key in companions and companions[key] not in pads)
                or (ref in ("U1","U3","J201") and key not in companions and not vias)
                or (key in inner and not inner_count)
            ):
                raise ValueError(
                    f"{key}: incomplete fanout, reached {pads}, {vias} vias, {inner_count} L3 segments"
                )
            result[key] = {"pads": sorted(pads), "vias": vias, "L3_segments": inner_count, "zones": sorted(set(planes))}
    return result


def add_units(raw, net):
    """KiCad 10 exposes FP_UNIT_INFO but has no Python binding for its vector."""
    by_path = {}
    for c in ET.parse(net).findall(".//components/comp"):
        if c.find('property[@name="exclude_from_board"]') is not None:
            continue
        path = c.find("sheetpath").get("tstamps") + c.findtext("tstamps")
        units = []
        for unit in c.findall("units/unit"):
            pins = " ".join(quote(n.get("num")) for n in unit.findall("pins/pin"))
            units.append(f"(unit (name {quote(unit.get('name'))}) (pins {pins}))")
        by_path[path] = "(units " + " ".join(units) + ")"

    def insert(match):
        return match[0] + "\n\t\t" + by_path.pop(match[1])

    raw = re.sub(r'(?m)^\t\t\(path "([^"]+)"\)$', insert, raw)
    if by_path:
        raise ValueError(f"Missing footprint paths: {list(by_path)}")
    return raw


def build(args, which, project):
    src = args.schematics / project
    qa = args.projects / "native-qa" / project
    out = args.out / project
    out.mkdir()
    for p in sorted(src.iterdir()):
        if p.suffix in (".kicad_sch", ".kicad_pro", ".kicad_sym") or p.name == "sym-lib-table":
            shutil.copyfile(p, out / p.name)
    sch = out / (project + ".kicad_sch")
    path = out / (project + ".kicad_pcb")
    net = out / "netlist.xml"

    def cli(*argv):
        subprocess.run([args.cli, *argv], check=True, stdout=subprocess.DEVNULL)

    cli("sch", "export", "netlist", "--format", "kicadxml", "-o", net, sch)
    comparison = equivalence(read_board(args.projects / (project + ".epro")), read_bom(), net)
    if not comparison["equivalent"] or cross_check(net, qa / "compiled.xml")["differences"]:
        raise ValueError(f"{project}: reviewed schematic comparison failed")
    board = pcb.LoadBoard(str(qa / "imported.kicad_pcb"))
    revise_lands(board, which)
    fps = link_board(board, net, out)
    thickness = {"main": 0.8, "rear": 0.12, "crown": 0.54, "bridge": 0.60}[which]
    board.GetDesignSettings().SetBoardThickness(mm(thickness))
    if which == "main":
        main_layout(board, fps)
    pcb.SaveBoard(str(path), board)
    raw = add_units(path.read_text(encoding="utf-8"), net)
    if which == "main":
        if raw.count("(setup\n") != 1 or "(stackup" in raw:
            raise ValueError("Expected a single PCB setup without an existing physical stackup")
        raw = raw.replace("(setup\n", "(setup\n" + stackup() + "\n", 1)
        pro = path.with_suffix(".kicad_pro")
        settings = json.loads(pro.read_text(encoding="utf-8"))
        settings["board"] = json.loads((DATA / "board-rules.json").read_text())
        settings["net_settings"] = {
            "meta": {"version": 3},
            "classes": [
                {
                    "name": "Default",
                    "clearance": 0.15,
                    "track_width": 0.16,
                    "via_diameter": 0.5,
                    "via_drill": 0.3,
                }
            ],
        }
        classes = settings["net_settings"]["classes"]
        patterns = []
        active = {p.GetNetname() for f in fps.values() for p in f.Pads()}
        for name, width, nets in (
            ("Supply", 0.50, ("DOCK_5V_IN","DOCK_PROTECTED","VBUS_5V","VBAT","VBAT_S","VSYS","VSYS_S","VSYS_1")),
            ("BTL", 0.35, ("SPK_OUTP","SPK_OUTN","SPK_GND")),
            ("Amplifier outputs", 0.30, ("N_31N8678","N_31N8717")),
            ("Motor current", 0.25, ("MOTOR_P","MOTOR_N","VDD33_VOUT")),
            ("Module supply", 0.30, ("MOD_SW_OUT","MOD_VSYS")),
            ("Amplifier supply", 0.35, ("N_31N8386",)),
            ("Panel", 0.35, ("OLED_VBAT","OLED_VCI","OLED_3V3","VCC_3V3_S","VCC_3V3")),
        ):
            classes.append(dict(classes[0], name=name, track_width=width))
            patterns.extend({"netclass": name, "pattern": n} for n in active if strip(n) in nets)
        settings["net_settings"]["netclass_patterns"] = sorted(patterns, key=lambda row: row["pattern"])
        pro.write_text(json.dumps(settings, indent=2), encoding="utf-8")
        shutil.copyfile(DATA / "main.kicad_dru", path.with_suffix(".kicad_dru"))
    path.write_text(raw, encoding="utf-8")
    cli(
        "pcb",
        "drc",
        "--schematic-parity",
        "--all-track-errors",
        *(("--refill-zones", "--save-board") if which == "main" else ()),
        "--format",
        "json",
        "-o",
        out / "drc.json",
        path,
    )
    cli("sch", "erc", "--format", "json", "-o", out / "erc.json", sch)
    board = read_saved_board(path)
    if which == "main":
        check_screen(board, args.screen_reference)
        (out / "fanout-readback.json").write_text(json.dumps(check_fanout(board), indent=2), encoding="utf-8")
    result = json.loads((out / "drc.json").read_text(encoding="utf-8"))
    if result["schematic_parity"]:
        raise ValueError(f"{project}: see schematic_parity in {out}/drc.json")
    allowed = (
        {"courtyards_overlap", "via_dangling"}
        if which == "main"
        else {"courtyards_overlap", "copper_edge_clearance"}
    )
    counts = Counter(v["type"] for v in result["violations"])
    if counts.keys() - allowed:
        raise ValueError(f"{project}: unexpected DRC findings {counts.keys() - allowed}")
    if which == "main" and (counts["courtyards_overlap"] > 3 or counts["via_dangling"] > 32
                            or len(result["unconnected_items"]) > 192):
        raise ValueError(f"{project}: six-layer layout DRC regression")
    erc = json.loads((out / "erc.json").read_text(encoding="utf-8"))
    if any(v["type"] != "power_pin_not_driven" for sheet in erc["sheets"] for v in sheet["violations"]):
        raise ValueError(f"{project}: unexpected ERC findings")
    print(project, len(fps), "footprints;", len(result["unconnected_items"]), "unconnected")


if __name__ == "__main__":
    # Keep font substitutions in CLI logs on Windows rather than a wx dialog.
    import wx
    wx.Log.SetActiveTarget(wx.LogStderr())
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--readback", type=Path, help="Export geometry.json from a saved, refilled PCB")
    for arg in ("projects", "schematics", "out", "cli"):
        parser.add_argument("--" + arg, type=Path)
    parser.add_argument("--screen-reference", type=Path, help="KiCad import of the official display PCB alone")
    args = parser.parse_args()
    if args.readback:
        if args.readback.suffix != ".kicad_pcb" or not args.readback.is_file():
            parser.error("--readback needs an existing .kicad_pcb")
        read_saved_board(args.readback)
        parser.exit()
    if not all((args.projects, args.schematics, args.out, args.cli)):
        parser.error("building requires --projects, --schematics, --out and --cli")
    args.out.mkdir(parents=True, exist_ok=False)
    for which, project in BOARDS.items():
        build(args, which, project)
