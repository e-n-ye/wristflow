"""Closed rear assembly, referenced to the flat back face at Z=0."""

import itertools

from model import envelope, join, scaled_z, slab_map
from one_rear_flex import rounded
from review_revision import App, V, block, cylinder, section, shifted


def form_strip(shape, profile):
    """Bend the flat runs onto the shallow pocket floors; keep the foil gauge in Z."""
    pieces = []
    for (y0, dz0), (y1, dz1) in itertools.pairwise(profile):
        piece = shape.common(block(-30, y0, -3, 60, y1 - y0, 20))
        if piece.Solids:
            if dz0 == dz1:
                piece = shifted(piece, dz0)
            else:
                matrix = App.Matrix()
                matrix.A32 = (dz1 - dz0) / (y1 - y0)
                matrix.A34 = dz0 - y0 * matrix.A32
                piece = piece.transformGeometry(matrix)
            pieces.append(piece)
    return join(pieces)


def rear(s, p, rows):
    # Optical face rests 0.10 above the windows. The steel sits on the opposite FPC face.
    window_bottom = -p.bump + 0.02
    chip_bottom = window_bottom + 0.50 + 0.10
    island = chip_bottom + 1.55
    dz = island - 1.02
    profile = [
        (-20, 0),
        (-14.0, 0),
        (-12.5, -0.15),
        (-6.4, -0.15),
        (-5.0, dz),
        (5.05, dz),
        (6.15, -0.15),
        (10.15, -0.15),
        (11.55, 0),
        (22, 0),
    ]
    # Only the vertical tangent of the U-fold shortens. Both circular folds and terminal stay intact.
    tail = slab_map(
        s["LidFlexNew"], [(-1, -1), (4.20, 4.20), (9.75, 9.75 + p.board_shift), (12, 12 + p.board_shift)]
    )
    s["LidFlexNew"] = form_strip(tail, profile)
    s["LidBond"] = form_strip(s["LidBond"], profile)
    # Cut the actual recess into both lid members. Its floor follows the same profile as the adhesive.
    tracks = [
        block(-2.64, -15.8, 0.90, 5.28, 3.4, 0.65),
        block(-1.84, -14.1, 0.90, 3.68, 9.5, 0.65),
        block(0.24, 4.8, 0.90, 5.92, 7, 0.65),
        block(-1.51, 10.1, 0.90, 2, 1.5, 0.65),
        rounded(12.4, 10.4, 0.65, 0.8, 0, 0, 0.90),
    ]
    groove = form_strip(join(tracks), profile)
    s["Back"] = s["Back"].cut(groove).removeSplitter()
    baffle = cylinder(8, p.bump, (0, 0, -p.bump)).fuse(cylinder(7, 0.90, (0, 0, 0)))
    baffle = baffle.cut(groove)
    gasket = shifted(s["PpgStiffAdhesive"], island - 0.12 - 1.14)
    gasket = scaled_z(gasket, island - 0.12, island - 0.12, 0.12 / 0.05)
    # Trial emitter / receiver apertures on the long axis of the MAX30102 package.
    for i, x in enumerate((-1.45, 1.45)):
        s["Optic" + str(i)] = cylinder(0.92, 0.50, (x, 0, window_bottom))
        baffle = baffle.cut(cylinder(0.96, p.bump + 1, (x, 0, -p.bump)))
    s["U301"] = block(-2.8, -1.65, chip_bottom, 5.6, 3.3, 1.55)
    for row in rows:
        if row["板"] == "rear" and row["位号"] in ("C301", "C302", "C303", "C304"):
            s[row["位号"]] = envelope(row, island - float(row["装配Z预算 mm"]))
    for name in ("U301", "C301", "C302", "C303", "C304"):
        b = s[name].BoundBox
        clearance = 0.05 if name == "U301" else 0.12
        pocket = block(
            b.XMin - 0.12,
            b.YMin - 0.12,
            b.ZMin - clearance,
            b.XLength + 0.24,
            b.YLength + 0.24,
            island - b.ZMin + 0.3,
        )
        baffle = baffle.cut(pocket)
        gasket = gasket.cut(pocket)
    s["Baffle"] = baffle.removeSplitter()
    s["PpgMount"] = gasket.removeSplitter()
    s["PpgStiffAdhesive"] = shifted(s["PpgStiffAdhesive"], dz)
    s["PpgStiffener"] = scaled_z(s["PpgStiffener"], 1.19, island + 0.17, 0.20 / 0.30)
    for key in ("LidEnd", "RearFingers"):
        s[key] = shifted(s[key], p.board_shift)


def tray(s, p):
    # Retain R2 screw heads, shoulder heights and frame threads. Form the plate down
    # between the two mounting ears so neither screw tip pierces the 0.90 mm back.
    original = s["Tray"]
    thin = shifted(section(original, 2.035).extrude(V(0, 0, 0.20)), 1.05 - 2.035)
    parts = []
    for x, y in ((12.50, -21.45), (-17.50, 10.96)):
        local = cylinder(1.32, 4, (x, y, 0))
        ear = original.common(local)
        ring = section(ear, 1.955).extrude(V(0, 0, 0.855))
        ring = shifted(ring, 1.05 - 1.955).cut(cylinder(1.01, 2, (x, y, 0.8)))
        thin = thin.cut(cylinder(1.01, 2, (x, y, 0.8)))
        parts.extend([ear, ring])
    cell_relief = block(
        -1 - p.cell_x / 2 - 0.10, -5.15 - p.cell_y / 2 - 0.10, 1.25, p.cell_x + 0.20, p.cell_y + 0.20, 2
    )
    s["Tray"] = join([thin, *parts]).cut(cell_relief).cut(rounded(12.5, 10.6, 1, 0.85, 0, 0, 0.8))
    # The raised charge landing connects the unchanged pin tails; relieve the foil locally.
    s["Tray"] = s["Tray"].cut(rounded(5.4, 3.9, 1, 0.4, 0, -14.45, 0.8)).removeSplitter()
    s["CellTape"] = shifted(s["CellTape"], -1)
    # Pack centre stays on the R2 datum, positive Y is the J1 strap end.
    s["Battery"] = block(-1 - p.cell_x / 2, -5.15 - p.cell_y / 2, 1.35, p.cell_x, p.cell_y, p.cell_z)
    s["Separator"] = block(-1 - p.cell_x / 2, -5.15 - p.cell_y / 2, 1.35 + p.cell_z, p.cell_x, p.cell_y, 0.10)
