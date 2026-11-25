"""Build the installed thin assembly from R2. Older comparison and service poses stay in R2."""

import hashlib
import json
from dataclasses import asdict

from model import (
    CANDIDATES,
    GROUPS,
    R2,
    Dimensions,
    bom_rows,
    box_z,
    envelope,
    join,
    scaled_z,
    slab_map,
)
from rear import rear, tray
from review_revision import App, Part, block, routed_lead, shifted


def front(s, p):
    board_shift = p.board_shift
    front_shift = p.flex_bottom - 16.01
    rim_shift = p.rim - 19.40
    # Preserve complete R2 upper features. Shorten the approach to the screen seat and
    # the straight wall within the screen thickness, below the independent ALS lens.
    upper = [
        (14.30, 14.30 + board_shift),
        (15.92, 15.92 + front_shift),
        (16.70, 16.70 + front_shift),
        (18.52, 18.52 + rim_shift),
        (20, 20 + rim_shift),
    ]
    stock = s["Frame"].cut(s["MotorSeat"]).removeSplitter()
    # The acoustic port and rear screw pillars stay on the back datum. The crown
    # cartridge, sleeve bore, pressure plate and both fixing axes follow the main PCB.
    crown_zone = block(11.2, -6.2, -5, 14, 12.4, 27)
    lower = box_z(stock, 0.9, 4.60).cut(crown_zone)
    middle = box_z(stock, 4.60 - board_shift, 14.30).cut(crown_zone)
    cartridge = box_z(stock.common(crown_zone), 0.90 - board_shift, 14.30)
    s["Frame"] = join(
        [lower, shifted(middle, board_shift), shifted(cartridge, board_shift), slab_map(stock, upper)]
    )
    motor_shift = p.separator_top + 0.70 - s["MotorSeat"].BoundBox.ZMin
    for key in ("MotorSeat", "MotorTape", "Motor"):
        s[key] = shifted(s[key], motor_shift)
    s["Frame"] = join([s["Frame"], s["MotorSeat"]])
    for i, (ex, ey) in enumerate(((16.60, -7.40), (17.30, -9.10))):
        x = 12.75 + i * 0.70
        # Run at the board-side height limit, away from the swollen cell.
        mid = p.separator_top + 0.97
        s["MotorWire" + str(i)] = routed_lead(
            [
                (x, -11.016, 12.99 + motor_shift),
                (x, -10.40, 12.99 + motor_shift),
                (x, -10.40, mid),
                (ex, ey, mid),
                (ex, ey, p.board - 0.06),
            ]
        )
    for key in ("ALSPipe", "ALSBaffle"):
        s[key] = slab_map(s[key], [(12, 12 + board_shift), *upper])
    for key in ("ALSLens", "ALSSeal", "ActiveArea"):
        s[key] = shifted(s[key], rim_shift)
    for key in ("GlassTape", "FrontRun"):
        s[key] = shifted(s[key], front_shift)
    s["Screen"] = scaled_z(s["Screen"], 16.28, p.flex_bottom + 0.27, p.screen_thickness / 2.955)
    # Stack readback measures the outer stiffener, keeping the connector's XY and board face fixed.
    tail_scale = p.j201_stack / 0.96
    for key in ("CN1", "FlexPlug", "RearRun", "Stiffener"):
        s[key] = scaled_z(s[key], 11.30, p.board, tail_scale)
    low = lambda z: p.board + (z - 11.30) * tail_scale
    s["Crossing"] = slab_map(
        s["Crossing"],
        [(10.3, low(10.3)), (11.50, low(11.50)), (15.30, 15.30 + front_shift), (16.4, 16.4 + front_shift)],
    )
    s["ShieldPET"] = block(-15.5, -16.1, p.board + 0.8 + 3.1, 18, 21.8, 0.05)


def harness(s, p):
    end_y = -5.15 + p.cell_y / 2
    port_z = 1.35 + p.cell_z - 0.8
    # Two live pack leads in the existing 3P housing. Pin 2 stays vacant.
    for name, x in (("Lead0", -9.66), ("Lead2", -8.34)):
        s[name] = routed_lead(
            [
                (x, 13.26, 9.98 + p.board_shift),
                (x, 12.65, 9.95 + p.board_shift),
                (x, end_y + 0.75, port_z + 0.12),
                (x, end_y, port_z),
            ]
        ).cut(s["Battery"])
    # Optional space for the insulated sensing tip and fixing tape (wiring to be decided).
    s["OptionalNTC"] = block(0.5, end_y - 2.5, p.separator_top, 4, 2.3, 0.55)


def run(out):
    p = Dimensions.read()
    rows = bom_rows()
    r2 = App.openDocument(str(R2))
    members = {g: [o.Name for o in r2.getObject(g).Group] for g in GROUPS}
    names = [name for group in members.values() for name in group] + ["Separator", "MotorSeat"]
    s = {n: r2.getObject(n).Shape.copy() for n in names}
    labels = {n: r2.getObject(n).Label for n in names}
    colors = {n: r2.getObject(n).ViewObject.ShapeColor[:3] for n in names}
    App.closeDocument(r2.Name)
    for key in ("OpticalDice", "Optic2", "Lead1", "SurfaceNTC", "NTCReturn"):
        del s[key]
    for key in (
        members["Circuit"]
        + members["Fasteners"]
        + members["CrownAssembly"]
        + members["Links"]
        + ["Crown", "CrownShaft", "BatPlug"]
    ):
        if key != "CN1":
            s[key] = shifted(s[key], p.board_shift)
    front(s, p)
    rear(s, p, rows)
    tray(s, p)
    harness(s, p)
    cell = s["Battery"].BoundBox
    s["HeightKeepout"] = block(cell.XMin, cell.YMin, p.separator_top, p.cell_x, p.cell_y, 0.70)
    for row in rows:
        if row["位号"] in CANDIDATES and row["板"] == "main":
            ref = row["位号"]
            s[ref] = envelope(row, p.board + 0.80, CANDIDATES[ref], turn=ref == "U21")
    members["Cell"].append("Separator")
    members["Lid"] += ["U301", "C301", "C302", "C303", "C304"]
    members["Fixed"].append("ShieldPET")
    members["Candidates"] = list(CANDIDATES)
    members["References"] = ["OptionalNTC", "HeightKeepout", "MotorSeat"]
    labels.update(
        Battery="两线 503030 · 成品最大包络",
        Tray="薄托板 · 光学开窗与充电避让",
        Frame="薄型中框 · 表冠孔与一体电机托架",
        PpgStiffener="PPG 0.20 mm 钢片",
        Baffle="后盖凸台 · 发射与接收双窗试位",
        OptionalNTC="可选 · 外贴 NTC 与胶带预留",
        HeightKeepout="电池投影内板下禁布区 · 保留装配与鼓胀空间",
        MotorSeat="托架局部 · 已并入中框",
        ShieldPET="屏蔽罩顶 0.05 mm PET",
    )
    doc = App.newDocument("ThinAssembly")
    doc.Label = "WristFlow B · R2 减薄装配"
    for group, keys in members.items():
        g = doc.addObject("App::DocumentObjectGroup", group)
        for key in keys:
            if key not in s:
                continue
            obj = doc.addObject("Part::Feature", key)
            g.addObject(obj)
            obj.Label = labels.get(key, key)
            obj.Shape = s[key]
            obj.ViewObject.ShapeColor = colors.get(key, (0.85, 0.51, 0.19))
            obj.Visibility = group != "References"
    doc.recompute()
    import FreeCADGui as Gui

    view = Gui.getDocument(doc.Name).activeView()
    view.viewAxonometric()
    view.fitAll()
    model = out / "WristFlow-B-R2-thin.FCStd"
    doc.saveAs(str(model))
    App.closeDocument(doc.Name)
    doc = App.openDocument(str(model))
    try:
        from thin_checks import verify

        shapes = {key: doc.getObject(key).Shape for key in s}
        report = verify(shapes, p, rows)
        report.update(
            parameters=asdict(p),
            input_sha256=hashlib.sha256(R2.read_bytes()).hexdigest(),
            freecad=App.Version()[:3],
        )
        # Export only installed objects. The separately displayed motor-seat copy belongs to Frame.
        import Import

        installed = [o for g in members if g != "References" for o in doc.getObject(g).Group]
        step = out / "WristFlow-B-R2-thin.step"
        Import.export(installed, str(step))
        exchange = Part.read(str(step))
        assert abs(exchange.BoundBox.ZMax - p.rim) < 0.001 and abs(exchange.BoundBox.ZMin + p.bump) < 0.001
        assert len(exchange.Solids) == sum(len(o.Shape.Solids) for o in installed)
        volume = sum(o.Shape.Volume for o in installed)
        report["step_volume_error_ppm"] = round(abs(exchange.Volume - volume) / volume * 1e6, 3)
        assert report["step_volume_error_ppm"] < 10
        report["step_solids"] = len(exchange.Solids)
        from drawing import draw

        draw(shapes, report, out)
        (out / "geometry.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    finally:
        App.closeDocument(doc.Name)
