"""Run with KiCad Python. Refill and reopen each independent fault on a new copy."""

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

GENERATOR = Path(__file__).resolve().parents[2] / "generator"
sys.path.insert(0, str(GENERATOR))


def probe(case, directory):
    import kicad_layout_check as check
    data = json.loads((directory / "geometry.json").read_text(encoding="utf-8"))
    pins = check.read_pins(data)
    if case.startswith("microphone"):
        errors = check.microphone_report(data, pins, directory / "microphone.csv")
        if errors:
            raise ValueError(f"Unshielded microphone conflicts: {errors}")
    elif case == "parallel":
        check.check_inner_crossings(data)
    elif case in ("six-vias", "j201-vias", "cutout-via", "cutout-growth", "edge-notch", "u3-window"):
        check.check_planes(data, pins)
    else:
        check.check_layout(data, directory)


def inject(board, case):
    def pad(key):
        ref, num = key.split(".")
        return board.FindFootprintByReference(ref).FindPadByNumber(num)

    def via(xy, key):
        item = pcb.PCB_VIA(board)
        item.SetPosition(point(*xy))
        item.SetNet(pad(key).GetNet())
        item.SetWidth(mm(.40))
        item.SetDrill(mm(.20))
        item.SetRemoveUnconnected(True)
        item.SetKeepStartEnd(True)
        item.SetCappingMode(pcb.CAPPING_MODE_CAPPED)
        item.SetFillingMode(pcb.FILLING_MODE_FILLED)
        board.Add(item)
        # Otherwise KiCad may reassign an isolated test via to the GND pour.
        anchor = pad(key)
        track(anchor.GetPosition(), point(*xy), .10, anchor.GetParentFootprint().GetLayer(), key)

    def track(start, end, width, layer, key):
        item = pcb.PCB_TRACK(board)
        item.SetStart(start)
        item.SetEnd(end)
        item.SetLayer(layer)
        item.SetWidth(mm(width))
        item.SetNet(pad(key).GetNet())
        board.Add(item)

    def void(name, corners):
        zone = pcb.ZONE(board)
        zone.SetZoneName(name)
        zone.SetIsRuleArea(True)
        # Apply to both planes via a local copper-free zone on *all* copper
        # for this particular fault. Keep its shape out of the antenna datum.
        zone.SetLayerSet(pcb.LSET.AllCuMask(6))
        zone.SetDoNotAllowZoneFills(True)
        zone.Outline().NewOutline()
        for xy in corners:
            zone.Outline().Append(point(*xy))
        board.Add(zone)

    if case == "six-vias":
        for i in range(6):
            via((-13 + i*.55, -14), "D19.2")
    elif case == "j201-vias":
        via((10.15, 4.15), "D19.2")
        via((10.15, 3.85), "D19.2")
    elif case == "cutout-via":
        via((-2.9, 12.0), "D19.2")
    elif case == "cutout-growth":
        void(case, [(-5, 10), (-3, 10), (-3, 12.4), (-5, 12.4)])
    elif case == "edge-notch":
        void(case, [(-4, 22), (-3, 22), (-3, 19.9), (-4, 19.9)])
    elif case == "parallel":
        track(point(-3, -8), point(3, -8), .16, pcb.In2_Cu, "J201.21")
        track(point(-3, -7.72), point(3, -7.72), .16, pcb.In3_Cu, "J201.20")
    elif case == "microphone-pad":
        item = pad("R60.1")
        item.SetPosition(point(-1.7, 13.30))
    elif case == "microphone-via":
        via((-1, 12.9), "U1.27")
    elif case == "dock-width":
        for item in board.GetTracks():
            if isinstance(item, pcb.PCB_TRACK) and not isinstance(item, pcb.PCB_VIA):
                if item.GetNetname() == "DOCK_5V_IN" and item.GetLayer() == pcb.B_Cu:
                    item.SetWidth(mm(.16))
    elif case == "u3-window":
        pad("R60.1").SetPosition(point(.5, 17.1))
    elif case in ("inherited-fill", "inherited-cap"):
        item = next(v for v in board.GetTracks() if isinstance(v, pcb.PCB_VIA))
        if case == "inherited-fill":
            item.SetFillingMode(pcb.FILLING_MODE_FROM_BOARD)
        else:
            item.SetCappingMode(pcb.CAPPING_MODE_FROM_BOARD)
    elif case == "width-drc":
        for key in ("U1.27", "R1.2", "U6.5", "U6.8", "MT1.1", "MT2.1", "U1.17"):
            start = pad(key).GetPosition()
            track(start, start + pcb.VECTOR2I(mm(.8), mm(.15)), .16, pad(key).GetParentFootprint().GetLayer(), key)
        track(pad("U2.A1").GetPosition(), pad("R49.1").GetPosition(), .35, pcb.B_Cu, "U2.A1")
        for item in board.GetTracks():
            if item.GetParentGroup() and item.GetParentGroup().GetName() == "VSYS_1 LDO branches":
                item.SetWidth(mm(.16))
    else:
        raise ValueError(case)


def run(args):
    cases = {
        "six-vias": "vias,", "j201-vias": "vias,", "cutout-via": "mechanical cutout",
        "cutout-growth": "mechanical cutout", "edge-notch": "board-edge notch",
        "parallel": "L3/L4 parallel gap", "microphone-pad": "Unshielded microphone conflicts",
        "microphone-via": "Unshielded microphone conflicts", "dock-width": "DOCK_5V_IN minimum",
        "u3-window": "foreign copper in mask opening",
        "inherited-fill": "Via protection", "inherited-cap": "Via protection",
        "width-drc": "track_width",
    }
    args.output.mkdir(parents=True, exist_ok=False)
    summary = {}
    for name, expected in cases.items():
        dest = args.output / name / args.board.parent.name
        dest.mkdir(parents=True)
        path = dest / args.board.name
        for ext in (".kicad_dru", ".kicad_pro"):
            shutil.copyfile(args.board.with_suffix(ext), path.with_suffix(ext))
        board = pcb.LoadBoard(str(args.board))
        inject(board, name)
        pcb.SaveBoard(str(path), board)
        drc = dest / "drc.json"
        subprocess.run([args.cli, "pcb", "drc", "--all-track-errors", "--refill-zones",
                        "--save-board", "--format", "json", "-o", drc, path], check=True, stdout=subprocess.DEVNULL)
        read_saved_board(path)
        if name == "width-drc":
            widths = [v for v in json.loads(drc.read_text(encoding="utf-8"))["violations"] if v["type"] == expected]
            descriptions = [(v["description"], v["items"][0]["description"]) for v in widths]
            for net in ("MOD_VSYS", "MOD_SW_OUT", "N_31N8678", "N_31N8717", "MOTOR_P", "MOTOR_N", "VDD33_VOUT"):
                if not any(net in item for _, item in descriptions):
                    raise ValueError(f"DRC missed narrow {net}: {descriptions}")
            for rule in ("VSYS_1 regulator branches", "VSYS_1 display feed"):
                if not any(rule in why for why, _ in descriptions):
                    raise ValueError(f"DRC missed {rule}: {descriptions}")
            summary[name] = descriptions
        else:
            result = subprocess.run([args.geometry_python, "-B", Path(__file__), "--probe", name, dest],
                                    capture_output=True, encoding="utf-8", errors="replace")
            if result.returncode == 0 or expected not in result.stderr.splitlines()[-1]:
                raise ValueError(f"{name}: expected {expected}, got {result.stderr[-1100:]}")
            summary[name] = result.stderr.splitlines()[-1]
        print(name, summary[name], flush=True)
        (args.output / "results.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2),encoding="utf-8")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--probe"]:
        probe(sys.argv[2], Path(sys.argv[3]))
        sys.exit(0)
    import pcbnew as pcb
    import wx
    wx.Log.SetActiveTarget(wx.LogStderr())
    from kicad_pcb import mm, point, read_saved_board
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ("board", "cli", "geometry_python", "output"):
        parser.add_argument(key, type=Path)
    run(parser.parse_args())
