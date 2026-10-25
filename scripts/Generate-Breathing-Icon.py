"""Draw an original concentric breathing icon; no XML or C conversion."""
from pathlib import Path
from PIL import Image, ImageDraw

output = Path(__file__).resolve().parents[1] / "ui/xml/images/breathing_icon.png"
scale = 4
image = Image.new("RGBA", (100 * scale, 100 * scale))
draw = ImageDraw.Draw(image)
# Echo the breathing page's ring, with readable layers at 44 px menu size.
for inset, color in ((0, "#0867c6"), (21, "#248ad4"), (28, "#48b3e3"), (34, "#73d4ee")):
    draw.ellipse((inset * scale, inset * scale,
                  (100 - inset) * scale - 1, (100 - inset) * scale - 1), fill=color)
image.resize((100, 100), Image.Resampling.LANCZOS).save(output)
print(f"{output.name}: {output.stat().st_size} bytes")
