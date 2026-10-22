"""Render WristFlow's original control glyphs. No XML or C export."""
from pathlib import Path
from math import cos, sin, pi
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "ui/xml/images"
SCALE = 4


def icon(name, paint):
    mask = Image.new("L", (64 * SCALE, 64 * SCALE))
    draw = ImageDraw.Draw(mask)

    class Pen:
        def ellipse(self, box, fill=255):
            draw.ellipse(tuple(v * SCALE for v in box), fill=fill)

        def rect(self, box, radius=0, fill=255):
            draw.rounded_rectangle(tuple(v * SCALE for v in box), radius * SCALE, fill=fill)

        def line(self, points, width=4, fill=255):
            draw.line([(x * SCALE, y * SCALE) for x, y in points], fill=fill,
                      width=width * SCALE, joint="curve")

        def polygon(self, points, fill=255):
            draw.polygon([(x * SCALE, y * SCALE) for x, y in points], fill=fill)

        def arc(self, box, start, end, width=3):
            draw.arc(tuple(v * SCALE for v in box), start, end, fill=255, width=width * SCALE)

    paint(Pen())
    rgba = Image.new("RGBA", mask.size, "white")
    rgba.putalpha(mask)
    path = OUT / f"control_{name}.png"
    rgba.resize((64, 64), Image.Resampling.LANCZOS).save(path)
    print(path.relative_to(ROOT))


def moon(p):
    p.ellipse((7, 6, 57, 57))
    p.ellipse((26, -2, 66, 39), 0)


def silent(p):
    p.ellipse((18, 10, 46, 39))
    p.polygon([(18, 25), (46, 25), (46, 40), (52, 47), (12, 47), (18, 40)])
    p.ellipse((27, 48, 37, 55))
    p.line([(10, 7), (55, 55)], 9, 0)
    p.line([(10, 7), (55, 55)], 4)


def wrist(p):
    p.rect((5, 24, 19, 42), 4)
    p.rect((8, 17, 16, 24), 2)
    p.rect((8, 42, 16, 49), 2)
    p.polygon([(24, 18), (46, 10), (52, 13), (58, 40), (55, 44),
               (36, 48), (29, 43), (22, 43), (22, 27)])
    p.rect((31, 45, 48, 51), 3)


def gear(p):
    p.polygon([(32 + 27 * cos(pi / 6 + i * pi / 3),
                32 + 27 * sin(pi / 6 + i * pi / 3)) for i in range(6)])
    p.ellipse((18, 18, 46, 46), 0)
    p.ellipse((24, 24, 40, 40))


def flashlight(p):
    p.rect((17, 6, 47, 12), 2)
    p.polygon([(17, 17), (47, 17), (41, 31), (38, 34), (38, 55), (26, 55), (26, 34), (23, 31)])
    p.rect((26, 49, 38, 58), 4)
    p.rect((30, 35, 34, 45), 2, 0)


def phone(p):
    p.rect((20, 8, 44, 57), 4)
    p.rect((27, 50, 37, 53), 1, 0)
    p.arc((10, 20, 24, 47), 110, 250)
    p.arc((2, 15, 23, 52), 110, 250)
    p.arc((40, 20, 54, 47), -70, 70)
    p.arc((41, 15, 62, 52), -70, 70)


def watch(p):
    p.rect((17, 16, 45, 48), 6)
    p.rect((24, 5, 38, 17), 2)
    p.rect((24, 47, 38, 59), 2)


def drain(p):
    watch(p)
    p.rect((21, 20, 41, 44), 3, 0)
    p.line([(24, 32), (37, 32)], 3)
    p.line([(31, 26), (37, 32), (31, 38)], 3)
    p.polygon([(53, 26), (48, 35), (48, 39), (53, 42), (58, 39), (58, 35)])
    p.ellipse((49, 11, 55, 17))


def battery(p):
    p.rect((6, 18, 53, 47), 6)
    p.rect((56, 25, 61, 40), 2)
    p.line([(29, 25), (29, 40)], 5, 0)
    p.line([(21, 32), (37, 32)], 5, 0)


def awake(p):
    watch(p)
    for left, right in [((5, 19), (11, 22)), ((3, 32), (10, 32)), ((5, 45), (11, 42)),
                        ((51, 22), (57, 19)), ((52, 32), (59, 32)), ((51, 42), (57, 45))]:
        p.line([left, right], 4)


for name, paint in [("dnd", moon), ("silent", silent), ("wrist", wrist),
                    ("settings", gear), ("flashlight", flashlight), ("phone", phone),
                    ("drain", drain), ("battery", battery), ("awake", awake)]:
    icon(name, paint)
