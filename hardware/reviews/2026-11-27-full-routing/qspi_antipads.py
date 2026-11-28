"""Isolate foreign-via voids by refilling copies of the saved PCB with KiCad Python."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

READBACK = Path(__file__).resolve().parents[2] / "generator" / "kicad_pcb.py"
QSPI_PADS = {name: (ref + ".1", ref + ".2") for name, ref in
             zip(("CLK", "D0", "D1", "D2", "D3"), ("R18", "R19", "R20", "R16", "R17"))}
CONTROL_PADS = {"CS": ("J201.14",), "TE": ("J201.13",), "RSTB": ("J201.15",)}


def via_set(holes):
    return Counter((v["net"], *v["xy"], v["diameter"], v["drill"]) for v in holes)


def track_set(tracks):
    return Counter((t["net"], t["side"], t["width"], *t["start"], *t["end"]) for t in tracks)


def remove_vias(path, copy, nets):
    import pcbnew
    import wx
    wx.Log.SetActiveTarget(wx.LogStderr())
    board = pcbnew.LoadBoard(str(path))
    for item in list(board.GetTracks()):
        if isinstance(item, pcbnew.PCB_VIA) and item.GetNetname() != "GND":
            if not nets or item.GetNetname() in nets:
                board.Remove(item)
    if not pcbnew.SaveBoard(str(copy), board, True):
        raise OSError(copy)


def run(project, output, cli):
    name = project.name
    path = project / (name + ".kicad_pcb")
    result = {"pcb_sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "planes": {}}
    original = json.loads((project / "geometry.json").read_text(encoding="utf-8"))
    if original["pcb_sha256"] != result["pcb_sha256"]:
        raise ValueError("Refresh geometry.json from the saved board")
    pins = {ref + "." + pad["pin"]: pad["net"] for ref, fp in original["footprints"].items() for pad in fp["pads"]}
    output.mkdir()
    for label, contacts in {"no_signal_vias": (), **QSPI_PADS, **CONTROL_PADS}.items():
        nets = {pins[k] for k in contacts}
        directory = output / label
        directory.mkdir()
        copy = directory / path.name
        for extension in (".kicad_pro", ".kicad_dru"):
            shutil.copyfile(path.with_suffix(extension), copy.with_suffix(extension))
        subprocess.run([sys.executable, "-B", __file__, "--make-copy", path, copy, *sorted(nets)], check=True)
        subprocess.run([cli, "pcb", "drc", "--refill-zones", "--save-board", "--format", "json",
                        "-o", directory / "drc.json", copy], check=True)
        # A fresh process also releases KiCad's cached project/footprint tables
        # before re-opening the board written by the CLI's separate process.
        subprocess.run([sys.executable, "-B", READBACK, "--readback", copy], check=True)
        saved = json.loads((directory / "geometry.json").read_text(encoding="utf-8"))
        expected = [via for via in original["vias"] if via["net"] == "GND" or (nets and via["net"] not in nets)]
        if via_set(saved["vias"]) != via_set(expected) or track_set(saved["tracks"]) != track_set(original["tracks"]):
            raise ValueError(f"{label}: unexpected copper in refilled probe")
        fills = saved["zone_fills"]
        result["planes"][label] = {layer: fills[layer] for layer in ("GND-L2", "GND-L5")}
    (output / "planes.json").write_text(json.dumps(result), encoding="utf-8")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--make-copy"]:
        remove_vias(Path(sys.argv[2]), Path(sys.argv[3]), set(sys.argv[4:]))
        sys.exit()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--cli", required=True)
    args = parser.parse_args()
    run(args.project, args.output, args.cli)
