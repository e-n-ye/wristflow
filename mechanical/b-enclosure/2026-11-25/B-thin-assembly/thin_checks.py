"""Acceptance measurements on solids reopened from the saved thin FCStd."""

from model import CANDIDATES, envelope
from review_revision import V, block, flat_section, gap, hit, shifted

TOL = 1e-5


def near(value, expected, name):
    assert abs(value - expected) < TOL, (name, value, expected)


def verify(s, p, rows):
    for name, sh in s.items():
        assert sh.isValid() and sh.Solids, name
    for name in ("Frame", "Tray", "Back", "Baffle", "LidFlexNew", "Board", "MotorSeat"):
        assert len(s[name].Solids) == 1, name
    b = {k: v.BoundBox for k, v in s.items()}
    measured = {}

    def clearance(a, b, minimum=0):
        v = hit(s[a], s[b])
        assert v < TOL, ("overlap", a, b, v)
        distance = gap(s[a], s[b])
        assert distance >= minimum - TOL, ("clearance", a, b, distance, minimum)
        measured[a + "/" + b] = round(distance, 5)
        return distance

    def contact(a, b):
        near(clearance(a, b), 0, (a, b))

    layers = {
        "bump": b["Back"].ZMin - b["Baffle"].ZMin,
        "bb": b["Battery"].ZMin - b["Back"].ZMin,
        "batt": b["Battery"].ZLength,
        "ins": b["Separator"].ZLength,
        "rgap": b["Board"].ZMin - b["Separator"].ZMax,
        "board": b["Board"].ZLength,
        "module": b["ModuleParts"].ZMax - b["Board"].ZMax,
        "fclr": b["FrontRun"].ZMin - b["ModuleParts"].ZMax,
        "fpc_tape": b["Screen"].ZMin - b["FrontRun"].ZMin,
        "screen": b["Screen"].ZLength,
        "rim": b["Frame"].ZMax - b["Screen"].ZMax,
    }
    targets = {
        "bump": p.bump,
        "bb": 1.35,
        "batt": p.cell_z,
        "ins": 0.10,
        "rgap": p.j201_stack + 0.70,
        "board": 0.80,
        "module": 3.10,
        "fclr": 0.30,
        "fpc_tape": 0.27,
        "screen": p.screen_thickness,
        "rim": 0.165,
    }
    for name, value in layers.items():
        near(value, targets[name], name)
    body = b["Frame"].ZMax - b["Back"].ZMin
    total = b["Frame"].ZMax - b["Baffle"].ZMin
    near(total, sum(layers.values()), "total")
    near(b["Battery"].XLength, p.cell_x, "cell X")
    near(b["Battery"].YLength, p.cell_y, "cell Y")
    near(
        b["Board"].ZMin - min(b[n].ZMin for n in ("CN1", "FlexPlug", "RearRun", "Stiffener")),
        p.j201_stack,
        "J201 stack",
    )
    near(b["HeightKeepout"].ZMax, b["Board"].ZMin - p.j201_stack, "height limit")
    for a, c in (
        ("ModuleParts", "ShieldPET"),
        ("Battery", "Separator"),
        ("CellTape", "Battery"),
        ("CellTape", "Tray"),
        ("Screen", "GlassTape"),
        ("GlassTape", "Frame"),
        ("FrontRun", "Crossing"),
        ("Crossing", "RearRun"),
        ("RearRun", "Stiffener"),
        ("LidFlexNew", "RearFingers"),
        ("LidFlexNew", "LidEnd"),
        ("U301", "LidFlexNew"),
        ("LidFlexNew", "PpgStiffAdhesive"),
        ("PpgStiffAdhesive", "PpgStiffener"),
    ):
        contact(a, c)
    clearance("ShieldPET", "FrontRun", 0.25)
    clearance("PpgStiffener", "Battery", 0.15)

    for n in (
        "CN1",
        "FlexPlug",
        "Stiffener",
        "RearRun",
        "Crossing",
        "MotorSeat",
        "Clamp1",
        "Clamp2",
        "Clamp3",
    ):
        clearance("Separator", n, 0.70)
        clearance("Battery", n, 0.80)
        clearance("HeightKeepout", n)
    contact("Separator", "OptionalNTC")
    near(b["OptionalNTC"].ZMin - b["Battery"].ZMax, 0.10, "optional tip above insulation")
    assert b["OptionalNTC"].XMax < min(b[n].XMin for n in ("CN1", "FlexPlug", "Stiffener"))
    swollen_tip = shifted(s["OptionalNTC"], 0.50)
    for n in (
        "CN1",
        "FlexPlug",
        "Stiffener",
        "RearRun",
        "Crossing",
        "Clamp1",
        "Clamp2",
        "Clamp3",
        "Board",
        "Lead0",
        "Lead2",
    ):
        clearance("OptionalNTC", n)
        assert hit(swollen_tip, s[n]) < TOL and gap(swollen_tip, s[n]) >= 0.20 - TOL, ("swollen NTC", n)
    # Include all BOM underside envelopes meeting the actual (parameterized) pack footprint.
    # J201 has a hollow mating solid, handled above instead of filling its socket with a box.
    underside = {}
    board = b["Board"]
    occupied = []
    for row in rows:
        if row["板"] != "main":
            continue
        ref = row["位号"]
        h = float(row["装配Z预算 mm"])
        if row["面 F=屏侧 B=后盖侧（主板）"] == "F":
            if ref != "U1":
                occupied.append((ref, envelope(row, board.ZMax)))
        elif ref not in CANDIDATES and ref != "J201":
            obj = envelope(row, board.ZMin - h)
            if obj.BoundBox.XMax <= b["Battery"].XMin or obj.BoundBox.XMin >= b["Battery"].XMax:
                continue
            if obj.BoundBox.YMax <= b["Battery"].YMin or obj.BoundBox.YMin >= b["Battery"].YMax:
                continue
            assert hit(obj, s["HeightKeepout"]) < TOL and h <= p.j201_stack + TOL, ("under board", ref, h)
            underside[ref] = round(gap(obj, s["Separator"]), 5)
            assert hit(obj, swollen_tip) < TOL and gap(obj, swollen_tip) >= 0.20 - TOL, ("NTC/BOM", ref)
    for name in CANDIDATES:
        shape = s[name]
        contact(name, "Board")
        # The full underside of each candidate must land on laminate, clear of holes and edges.
        foot = shape.translated(V(0, 0, -0.01))
        near(hit(foot, s["Board"]), b[name].XLength * b[name].YLength * 0.01, name + " support")
        for n in (
            "Frame",
            "Motor",
            "CrownBridge",
            "Crossing",
            "FrontRun",
            "ModuleParts",
            "ModuleSubstrate",
            "MotorWire0",
            "MotorWire1",
            "Clamp1",
            "Clamp2",
            "Clamp3",
        ):
            clearance(name, n, 0.05)
        for ref, other in occupied:
            assert gap(shape, other) > 0.05, ("candidate/BOM", name, ref)
        occupied.append((name, shape))

    near(b["MotorSeat"].ZMin, p.board - p.j201_stack, "motor seat lower edge")
    near(hit(s["MotorSeat"], s["Frame"]), s["MotorSeat"].Volume, "integral motor seat")
    contact("MotorTape", "MotorSeat")
    contact("MotorTape", "Motor")
    for n in ("Frame", "Board", "FrontRun", "Battery", "Separator"):
        clearance("Motor", n)
    for i in range(2):
        n = "MotorWire" + str(i)
        for obstacle in ("Battery", "Separator", "Board", "Frame", "HeightKeepout"):
            clearance(n, obstacle)
        near(gap(s[n], s["MotorLand" + str(i)]), 0, "motor lead to pad")
    for n in ("EncoderBody", "Daughter", "Retainer", "CrownBridge", "Crown", "CrownShaft"):
        assert b[n].ZMin > b["Back"].ZMax and b[n].ZMax < body
        clearance(n, "Frame")
        clearance(n, "Battery")
    near(b["EncoderBody"].ZMin, 5.85 + p.board_shift, "encoder")
    near(b["Daughter"].ZMin, 4.63 + p.board_shift, "daughter")
    contact("Daughter", "CrownBridge")
    contact("Daughter", "EncoderBody")
    contact("CrownBridge", "Frame")
    contact("Retainer", "Daughter")
    contact("RetainerScrews", "Retainer")
    shelf = flat_section(s["Frame"], b["CrownBridge"].ZMax + 0.002)
    for i in range(4):
        leaf, pad = "CrownLeaf" + str(i), "CrownPad" + str(i)
        contact(leaf, "Board")
        contact(leaf, pad)
        contact(pad, "CrownBridge")
        nose = flat_section(s[leaf], b[leaf].ZMax - 0.001)
        assert nose.area > 0 and nose.difference(shelf).area < TOL, ("spring backing", leaf)
    for moving in ("Crown", "CrownShaft", "SwitchFollower"):
        pressed = s[moving].translated(V(-0.25, 0, 0))
        for fixed in ("Frame", "GuideSleeve", "EncoderBody", "EncoderRotor", "Battery", "Daughter"):
            assert hit(pressed, s[fixed]) < TOL, ("crown press", moving, fixed)
    margins = {}
    for n in ("BatPlug", "KeyBody"):
        clearance(n, "Speaker", 0.20)
        margins[n] = round(b[n].ZMin - b["Speaker"].ZMax - 0.20, 5)
        assert margins[n] > 0
    for i in (0, 2):
        wire = s["Lead" + str(i)]
        near(gap(wire, s["BatPlug"]), 0, "pack plug")
        near(gap(wire, s["Battery"]), 0, "pack edge")
        assert hit(wire, s["Battery"]) < TOL
        for n in ("Speaker", "Board", "Frame", "LidFlexNew", "Separator"):
            clearance("Lead" + str(i), n)
    for i in range(2):
        tab = "ChargeTab" + str(i)
        contact(tab, "LidFlexNew")
        contact(tab, "Contact" + str(i))
        near(b[tab].ZMax, 1.02, tab)
        near(b["Tray"].ZMin - b[tab].ZMax, 0.03, "tail/tray planes")
        clearance(tab, "Tray", 0.03)
    for n in ("U301", "C301", "C302", "C303", "C304"):
        contact(n, "LidFlexNew")
        clearance(n, "Baffle", 0.05)
        clearance(n, "PpgMount", 0.05)
        for i in range(2):
            clearance(n, "Optic" + str(i), 0.10)
    near(b["U301"].ZMin - b["Optic0"].ZMax, 0.10, "chip optical gap")
    # Probe a constant-depth section of each routed branch. Back material below the
    # pocket floor, bonded foil above it, and the actual tray are measured independently.
    grooves = {}
    for name, x, y in (("charge", 0, -10), ("tail", 3, 8.6)):
        probe = block(x - 0.1, y - 0.1, 0, 0.2, 0.2, 2)
        floor = s["Back"].common(probe).BoundBox.ZMax
        foil = s["LidFlexNew"].common(probe).BoundBox
        near(0.90 - floor, 0.15, name + " recess")
        near(foil.ZMin - floor, 0.12, name + " adhesive")
        near(foil.ZLength, 0.12, name + " FPC")
        grooves[name] = round(0.9 - floor, 5)
    for n in ("Tray", "Battery", "Back", "Baffle", "RearSocket"):
        clearance("LidFlexNew", n)
    contact("LidBond", "Back")
    for n in ("Frame", "Back", "Battery", "Speaker", "TrayScrew1", "TrayScrew2", "PpgStiffener"):
        clearance("Tray", n)
    return {
        "layers_mm": {k: round(v, 5) for k, v in layers.items()},
        "body_mm": round(body, 5),
        "total_mm": round(total, 5),
        "board_bottom_mm": round(board.ZMin, 5),
        "strap_margin_after_assembly_mm": margins,
        "groove_depth_mm": grooves,
        "clearance_mm": measured,
        "under_board_to_separator_mm": underside,
    }
