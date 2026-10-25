"""Draw original compass icon / calibration gesture assets, never XML or C."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "ui/xml/images"
SCALE = 4


def canvas(width, height):
    image = Image.new("RGBA", (width * SCALE, height * SCALE))
    return image, ImageDraw.Draw(image)


def save(image, name):
    image.resize((image.width // SCALE, image.height // SCALE), Image.Resampling.LANCZOS).save(OUTPUT / name)


def polygon(draw, points, fill):
    draw.polygon([(round(x * SCALE), round(y * SCALE)) for x, y in points], fill=fill)


image, draw = canvas(24, 24)
polygon(draw, [(2, 4), (22, 4), (12, 22)], "white")
save(image, "compass_pointer.png")

image, draw = canvas(100, 100)
draw.ellipse((0, 0, 400, 400), fill="#173a69")
draw.ellipse((15 * SCALE, 15 * SCALE, 85 * SCALE, 85 * SCALE), outline="#3b699c", width=2 * SCALE)
for degree in (90, 180, 270):
    angle = math.radians(degree)
    a = (50 + 30 * math.cos(angle), 50 + 30 * math.sin(angle))
    b = (50 + 35 * math.cos(angle), 50 + 35 * math.sin(angle))
    draw.line([(round(x * SCALE), round(y * SCALE)) for x, y in (a, b)], fill="#b6d8f4", width=2 * SCALE)
font = ImageFont.truetype(str(ROOT / "ui/xml/fonts/WristFlowSans-Regular.ttf"), 10 * SCALE)
draw.text((50 * SCALE, 14 * SCALE), "N", font=font, anchor="mm", fill="#e5f4ff")
polygon(draw, [(78, 25), (57, 56), (48, 48)], "#ff354f")
polygon(draw, [(22, 77), (48, 48), (57, 56)], "#e5f4ff")
polygon(draw, [(78, 25), (51, 51), (48, 48)], "#ff7684")
polygon(draw, [(22, 77), (51, 51), (57, 56)], "#acd1eb")
save(image, "compass_icon.png")

image, draw = canvas(280, 150)
# Smooth horizontal figure eight with two arrowheads following the path.
points = []
for i in range(721):
    t = 2 * math.pi * i / 720
    points.append(((140 + 126 * math.cos(t)) * SCALE,
                   (75 + 54 * math.sin(2 * t)) * SCALE))
draw.line(points, fill="#a8a8a8", width=7 * SCALE, joint="curve")
for x, y in points:
    r = 3.5 * SCALE
    draw.ellipse((x - r, y - r, x + r, y + r), fill="#a8a8a8")
for t in (math.pi * 0.70, math.pi * 1.70):
    x, y = 140 + 126 * math.cos(t), 75 + 54 * math.sin(2 * t)
    dx, dy = -126 * math.sin(t), 108 * math.cos(2 * t)
    norm = math.hypot(dx, dy)
    dx, dy = dx / norm, dy / norm
    tip = (x + 8 * dx, y + 8 * dy)
    polygon(draw, [tip, (x - 10 * dx + 8 * dy, y - 10 * dy - 8 * dx),
                   (x - 10 * dx - 8 * dy, y - 10 * dy + 8 * dx)], "#d0d0d0")
save(image, "compass_calibration.png")
