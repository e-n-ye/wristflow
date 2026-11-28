"""Report the saved copper along selected main-board load paths."""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "generator"))
from kicad_layout_check import copper_path, read_pins


# Comparison currents in mA. Downstream branches with unknown peak current
# use 100 mA so the reader can scale the copper drop to a measured load.
PATHS = (
    ("J2.10", "D19.2", (330, 350)), ("D19.1", "U17.1", (330, 350)),
    ("U17.8", "U3.A1", (330, 350)), ("U3.A2", "R65.1", (330, 450)),
    ("R65.2", "U2.A2", (330, 450)), ("R65.2", "U19.A2", (330, 450)),
    ("R65.2", "C4.2", (100,)), ("C4.2", "U19.A2", (100,)),
    ("U19.A1", "R1.2", (100,)), ("R1.1", "U1.27", (100,)),
    ("C1.2", "U1.27", (100,)), ("R65.2", "R26.1", (100,)),
    ("R26.2", "U6.6", (100,)), ("U6.5", "L1.1", (350,)),
    ("U6.8", "L2.1", (350,)), ("L1.2", "J2.2", (350,)), ("L2.2", "J2.1", (350,)),
    ("U2.A1", "R49.1", (100,)), ("U2.A1", "U4.1", (100,)), ("U2.A1", "U15.1", (100,)),
    ("U4.5", "R66.1", (100,)), ("R66.2", "R50.1", (100,)), ("R50.2", "J201.19", (100,)),
    ("R49.2", "R206.1", (100,)), ("R206.2", "J201.4", (100,)),
    ("U15.5", "R52.1", (100,)), ("R52.2", "U21.1", (100,)), ("R52.2", "J2.4", (100,)),
    ("U21.5", "J2.5", (100,)), ("J1.3", "JP1.1", (450,)), ("JP1.2", "U3.A3", (450,)),
    ("JP1.2", "U1.61", (100,)), ("U1.17", "R36.1", (68, 110)),
    ("R36.2", "MT1.1", (68, 110)), ("MT2.1", "Q2.3", (68, 110)),
)


def read_paths(directory):
    pcb = directory / (directory.name + ".kicad_pcb")
    raw = pcb.read_bytes()
    g = json.loads((directory / "geometry.json").read_text(encoding="utf-8"))
    if hashlib.sha256(raw).hexdigest() != g["pcb_sha256"]:
        raise ValueError("PCB differs from the geometry readback")
    # Physical copper layer names in the stackup retain KiCad's canonical names.
    stack = dict(re.findall(r'\(layer\s+"([^"]+)"\s+\(type\s+"copper"\)\s+\(thickness\s+([\d.]+)\)',
                            raw.decode("utf-8")))
    thickness = {key: float(stack[name]) for key, name in
                 (("F", "F.Cu"), ("B", "B.Cu"), ("L3", "In2.Cu"), ("L4", "In3.Cu"))}
    pins = read_pins(g)
    rows = []
    for start, end, currents in PATHS:
        steps = []
        length, _ = copper_path(g, pins, start, end, steps=steps)
        traces = [s for s in steps if s["width_mm"] is not None and s["length_mm"] > 1e-9]
        for step in traces:
            # Bulk copper at 20 °C. Coordinates, width and foil thickness are in mm.
            step["copper_mohm"] = .01724 * step["length_mm"] / (step["width_mm"] * thickness[step["layer"]])
        resistance = sum(s["copper_mohm"] for s in traces)
        rows.append({"start": start, "end": end, "net": pins[start]["net"], "centerline_mm": length,
                     "minimum_width_mm": min(s["width_mm"] for s in traces),
                     "trace_length_mm": sum(s["length_mm"] for s in traces),
                     "trace_mohm_20C": resistance,
                     "drop_mV_at_mA": {mA: resistance * mA / 1000 for mA in currents}, "traces": traces})
    return {"pcb_sha256": g["pcb_sha256"], "copper_thickness_mm": thickness,
            "model": "Planar tracks at 20 C. Excludes via barrels, pad spreading, switches, connectors and load return.",
            "paths": rows}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("project", type=Path)
    print(json.dumps(read_paths(parser.parse_args().project), ensure_ascii=False, indent=2))
