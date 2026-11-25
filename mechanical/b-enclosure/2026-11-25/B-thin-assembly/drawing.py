"""Two dimensioned sections cut directly from the reopened FreeCAD assembly."""

from PIL import Image, ImageDraw, ImageFont
from review_revision import Part, V

INK = "#182e3d"
GROUPS = [
    ("壳体与凸台", "#81939c", "Frame Back Baffle"),
    ("屏组件", "#405c79", "Screen"),
    ("屏尾与后盖柔板", "#d78030", "GlassTape FrontRun Crossing RearRun Stiffener LidFlexNew"),
    ("主板与表冠立板", "#40856e", "Board Daughter CrownBridge ModuleSubstrate"),
    ("503030 电池", "#dec487", "Battery"),
    ("托板与钢片", "#607582", "Tray PpgStiffener"),
    ("光窗与传感器", "#44a7ad", "Optic0 Optic1 U301"),
    ("模组、电机与编码器", "#a08aa7", "ModuleParts Motor EncoderBody Crown CrownShaft"),
    ("胶带、绝缘与 PET", "#bd8959", "CellTape Separator ShieldPET MotorTape PpgStiffAdhesive"),
    ("充电触点", "#be893a", "Contact0 Contact1 ChargeTab0 ChargeTab1"),
]


def draw(shapes, report, out):
    image = Image.new("RGB", (1500, 1050), "#ffffff")
    pen = ImageDraw.Draw(image)

    def label(x, y, text, size, color=INK):
        font = ImageFont.truetype("msyh.ttc", size)
        pen.text((x, y), text, font=font, fill=color, anchor="lm")

    label(52, 51, "B 方案减薄装配", 38)
    label(52, 103, "X–Z 剖面  ·  两图同尺度  ·  平背外表面为 Z = 0 mm", 21)
    for baseline, y, title, notes in (
        (
            493,
            0,
            "表冠轴线　Y = 0",
            [
                ("总厚", f"{report['total_mm']:.3f} mm"),
                ("平背至表圈", f"{report['body_mm']:.3f} mm"),
                ("主板背面", f"Z {report['board_bottom_mm']:.2f} mm"),
            ],
        ),
        (
            889,
            -15,
            "电机与充电触点　Y = −15",
            [
                ("托架下沿", f"Z {shapes['MotorSeat'].BoundBox.ZMin:.2f} mm"),
                ("触点尾部最高", "Z 1.02 mm"),
                ("托板下表面", "Z 1.05 mm"),
            ],
        ),
    ):
        label(52, baseline - 323, title, 24)
        plane = Part.Face(
            Part.makePolygon([V(x, y, z) for x, z in [(-28, -3), (27, -3), (27, 22), (-28, 22), (-28, -3)]])
        )
        for _, color, keys in GROUPS:
            for key in keys.split():
                section = shapes[key].common(plane)
                for face in section.Faces:
                    mask = Image.new("1", image.size)
                    brush = ImageDraw.Draw(mask)
                    for wire in [face.OuterWire] + [w for w in face.Wires if not w.isSame(face.OuterWire)]:
                        pts = [
                            (round(527 + 18 * p.x), round(baseline - 18 * p.z))
                            for p in wire.discretize(Deflection=0.018)
                        ]
                        if len(pts) > 2:
                            brush.polygon(pts, fill=int(wire.isSame(face.OuterWire)))
                    image.paste(color, mask=mask)
        for x in range(54, 1015, 14):
            pen.line((x, baseline, x + 6, baseline), fill="#53666d")
        label(53, baseline + 18, "Z = 0", 15)
        for i, (key, value) in enumerate(notes):
            label(1040, baseline - 260 + i * 64, key, 20, "#506773")
            label(1220, baseline - 260 + i * 64, value, 23)
        pen.line((52, baseline + 48, 1448, baseline + 48), fill="#ccd6da")
    for i, (name, color, _) in enumerate(GROUPS):
        x, y = 53 + (i % 5) * 290, 967 + (i // 5) * 41
        pen.rectangle((x, y - 8, x + 18, y + 8), fill=color)
        label(x + 28, y, name, 17)
    image.save(out / "section.png")
