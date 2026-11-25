"""Build the four editable PCB projects from reviewed land patterns and KiCad netlists.

Run with KiCad 10's Python. Assembly coordinates are in mm, +Y towards J1.
"""

import argparse
import csv
import json
import math
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
from collections import Counter
from pathlib import Path

import pcbnew as pcb
from kicad_schematic import BOARDS, cross_check, equivalence, read_board, read_bom, strip
from kicad_sexpr import quote

DATA = Path(__file__).resolve().parents[1] / "boards/b-r2-main"
mm = pcb.FromMM


def point(x, y):
    return pcb.VECTOR2I(mm(150 + x), mm(110 - y))


def assembly(pt):
    return [round(pcb.ToMM(pt.x) - 150, 6), round(110 - pcb.ToMM(pt.y), 6)]


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
    for route in json.loads((DATA / "fanout.json").read_text()):
        ref, pin = route["pad"].split(".")
        pad = fps[ref].FindPadByNumber(pin)
        start = pad.GetPosition()
        for xy in route["path"]:
            end = point(*xy)
            t = pcb.PCB_TRACK(board)
            t.SetStart(start)
            t.SetEnd(end)
            t.SetWidth(mm(route["width"]))
            t.SetLayer(fps[ref].GetLayer())
            t.SetNet(pad.GetNet())
            board.Add(t)
            start = end
        if route.get("via"):
            v = pcb.PCB_VIA(board)
            v.SetPosition(start)
            v.SetViaType(pcb.VIATYPE_THROUGH)
            v.SetWidth(mm(0.50))
            v.SetDrill(mm(0.30))
            v.SetNet(pad.GetNet())
            board.Add(v)
            for xy in route.get("inner", []):
                end = point(*xy)
                t = pcb.PCB_TRACK(board)
                t.SetStart(start)
                t.SetEnd(end)
                t.SetWidth(mm(0.15))
                t.SetLayer(pcb.In2_Cu)
                t.SetNet(pad.GetNet())
                board.Add(t)
                start = end


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
    board.SetCopperLayerCount(4)
    board.SetLayerName(pcb.In1_Cu, "GND")
    board.SetLayerName(pcb.In2_Cu, "Signals-Power")
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
        pcb.LSET.AllCuMask(4),
        True,
    )
    reserve(
        board,
        "Battery projection / B height 0.96 mm / see assembly check",
        [(-17, -20.4), (15, -20.4), (15, 10.1), (-17, 10.1)],
        pcb.LSET().AddLayer(pcb.B_Cu),
        False,
    )
    reserve(
        board,
        "U3 center ball escape",
        [(-0.68, 15.8), (1.4, 15.8), (1.4, 16.85), (-0.68, 16.85)],
        pcb.LSET().AddLayer(pcb.F_Cu),
        False,
    )
    surfaces = pcb.LSET().AddLayer(pcb.F_Cu).AddLayer(pcb.B_Cu)
    for i, (cx, cy) in enumerate([(5.15, -9.8), (14.55, 9.7), (-15.5, 8.4)], 1):
        corners = [
            (cx + 1.522 * math.cos(t * math.tau / 64), cy + 1.522 * math.sin(t * math.tau / 64))
            for t in range(64)
        ]
        reserve(board, f"Clamp {i} / head and bearing surface", corners, surfaces, True)
    trial_tracks(board, fps)


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
            data[f.GetReference()]["pads"].append(
                {
                    "pin": p.GetNumber(),
                    "net": p.GetNetname(),
                    "xy": assembly(p.GetPosition()),
                    "polygons": [ring(shape.COutline(i)) for i in range(shape.OutlineCount())],
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
                    "net": t.GetNetname(),
                }
            )
        else:
            layer = {pcb.F_Cu: "F", pcb.B_Cu: "B", pcb.In2_Cu: "L3"}[t.GetLayer()]
            tracks.append(
                {
                    "start": assembly(t.GetStart()),
                    "end": assembly(t.GetEnd()),
                    "net": t.GetNetname(),
                    "side": layer,
                    "width": pcb.ToMM(t.GetWidth()),
                }
            )
    return {
        "outline": ring(outline.COutline(0)),
        "holes": [ring(outline.CHole(0, i)) for i in range(outline.HoleCount(0))],
        "footprints": data,
        "tracks": tracks,
        "vias": vias,
        "copper_layers": board.GetCopperLayerCount(),
        "thickness": pcb.ToMM(board.GetDesignSettings().GetBoardThickness()),
    }


def stackup():
    # JLC04081H-3313. Copper + pressed dielectric = 0.7992 mm.
    names = ["F.Cu", "In1.Cu", "In2.Cu", "B.Cu"]
    dielectric = [("prepreg", 0.0994, 4.10), ("core", 0.500, 4.48), ("prepreg", 0.0994, 4.10)]
    layers = []
    for i, name in enumerate(names):
        layers.append(f'(layer "{name}" (type "copper") (thickness {0.035 if i in (0, 3) else 0.0152}))')
        if i < len(dielectric):
            kind, thick, dk = dielectric[i]
            layers.append(
                f'(layer "dielectric {i + 1}" (type "{kind}") (thickness {thick}) '
                f'(material "FR4") (epsilon_r {dk}))'
            )
    return "(stackup\n" + "\n".join(layers) + ")"


def check_fanout(board):
    """Walk actual connected copper, including vias, after reloading the saved board."""
    board.BuildConnectivity()
    connectivity = board.GetConnectivity()
    footprints = {f.GetReference(): f for f in board.GetFootprints()}
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
    inner = {"U3.B2", "U1.7", "U1.31", *(f"J201.{i}" for i in range(8, 13))}
    result = {}
    for ref, pins in targets.items():
        for number in pins:
            key = ref + "." + number
            start = footprints[ref].FindPadByNumber(number)
            queue, seen, pads, vias, inner_count = [start], set(), [], 0, 0
            while queue:
                item = queue.pop()
                uuid = item.m_Uuid.AsString()
                if uuid in seen:
                    continue
                seen.add(uuid)
                if isinstance(item, pcb.PAD):
                    pads.append(item.GetParentFootprint().GetReference() + "." + item.GetNumber())
                elif item.Type() == pcb.PCB_VIA_T:
                    vias += 1
                elif item.GetLayer() == pcb.In2_Cu:
                    inner_count += 1
                queue.extend(connectivity.GetConnectedPads(item))
                queue.extend(connectivity.GetConnectedTracks(item))
            if (
                (key in companions and companions[key] not in pads)
                or (key not in ("U3.A1", "U3.A2", "U3.A3", "U3.B3") and not vias)
                or (key in inner and not inner_count)
            ):
                raise ValueError(
                    f"{key}: incomplete fanout, reached {pads}, {vias} vias, {inner_count} L3 segments"
                )
            result[key] = {"pads": sorted(pads), "vias": vias, "L3_segments": inner_count}
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
        pro.write_text(json.dumps(settings, indent=2), encoding="utf-8")
        shutil.copyfile(DATA / "main.kicad_dru", path.with_suffix(".kicad_dru"))
    path.write_text(raw, encoding="utf-8")
    board = pcb.LoadBoard(str(path))
    if which == "main":
        (out / "fanout-readback.json").write_text(json.dumps(check_fanout(board), indent=2), encoding="utf-8")
    (out / "geometry.json").write_text(json.dumps(geometry(board)), encoding="utf-8")
    cli(
        "pcb",
        "drc",
        "--schematic-parity",
        "--all-track-errors",
        "--format",
        "json",
        "-o",
        out / "drc.json",
        path,
    )
    cli("sch", "erc", "--format", "json", "-o", out / "erc.json", sch)
    result = json.loads((out / "drc.json").read_text(encoding="utf-8"))
    if result["schematic_parity"]:
        raise ValueError(f"{project}: see schematic_parity in {out}/drc.json")
    allowed = (
        {"courtyards_overlap", "silk_edge_clearance", "via_dangling", "track_dangling"}
        if which == "main"
        else {"courtyards_overlap", "copper_edge_clearance"}
    )
    counts = Counter(v["type"] for v in result["violations"])
    if counts.keys() - allowed:
        raise ValueError(f"{project}: unexpected DRC findings {counts.keys() - allowed}")
    erc = json.loads((out / "erc.json").read_text(encoding="utf-8"))
    if any(v["type"] != "power_pin_not_driven" for sheet in erc["sheets"] for v in sheet["violations"]):
        raise ValueError(f"{project}: unexpected ERC findings")
    print(project, len(fps), "footprints;", len(result["unconnected_items"]), "unconnected")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for arg in ("projects", "schematics", "out", "cli"):
        parser.add_argument("--" + arg, type=Path, required=True)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    for which, project in BOARDS.items():
        build(args, which, project)
