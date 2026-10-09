"""Generate only the transparent sun track bitmap; C is exported by LVGL Pro."""
from pathlib import Path
import math

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "ui/xml/images/weather_sun_track.png"
WIDTH, HEIGHT, SCALE = 336, 128, 4

# Redmi Watch 4 photo geometry normalized to 390x450, without its perspective,
# display scan lines or camera exposure. The XML places the image at (27, 157).
# Global horizon=240, crossings=(72,240)/(318,240), apex=(195,160).
HORIZON, APEX, RISE_X, SET_X = 83, 3, 45, 291
canvas = Image.new("RGBA", (WIDTH * SCALE, HEIGHT * SCALE), (0, 0, 0, 0))
draw = ImageDraw.Draw(canvas)


def track_y(x):
    return HORIZON - (HORIZON - APEX) * math.sin(math.pi * (x - RISE_X) / (SET_X - RISE_X))


def stroke(first, last, color):
    points = [(round(x * SCALE), round(track_y(x) * SCALE))
              for x in (first + i / SCALE for i in range(round((last - first) * SCALE) + 1))]
    draw.line(points, fill=color, width=4 * SCALE, joint="curve")


# Black with alpha darkens either weather background, without baking in blue.
stroke(3, RISE_X, (0, 0, 0, 42))
stroke(SET_X, WIDTH - 3, (0, 0, 0, 42))
stroke(RISE_X, SET_X, (255, 255, 255, 220))
for x in range(3, WIDTH - 3, 12):
    draw.line([(x * SCALE, HORIZON * SCALE), ((x + 4) * SCALE, HORIZON * SCALE)],
              fill=(255, 255, 255, 180), width=2 * SCALE)

canvas.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS).save(OUTPUT)
print(f"{OUTPUT.relative_to(ROOT)}: {OUTPUT.stat().st_size} bytes, RGBA {WIDTH}x{HEIGHT}")
