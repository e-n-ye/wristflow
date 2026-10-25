# Static Hour Artwork

`hour_1.png` through `hour_12.png` are reproducible static bitmap assets made by
`scripts/Generate-Ui-Assets.py`. They use Montserrat Bold from the locked LVGL
test assets, with original cyan bands and gaps. Font metadata identifies
Copyright 2011 The Montserrat Project Authors
(https://github.com/JulietaUla/Montserrat), licensed under SIL OFL 1.1.
The font file itself is not redistributed here.

Each image is 342x282 with a black background and is exported as RGB565.
The twelve uncompressed pixel arrays total 2,314,656 bytes (about 2.21 MiB).
These are static complete-hour images, not independently animated fragments.
Breathing animation is deferred until hardware is available, per user decision.

`hour_seven.png` and its earlier generated C are retained as historical source
assets; `globals.xml` no longer registers them, the official generated CMake
list excludes them, and the handwritten SCons source list now excludes only
the legacy `hour_seven_data.c`. The original preview evidence remains in
docs/evidence.

## Control center glyphs

The nine `control_*.png` files are original WristFlow artwork, rendered by
`scripts/Generate-Control-Icons.py` with Pillow. Each is a 64×64 white glyph with
an antialiased transparent background, registered as ARGB8888. The Redmi Watch 4
photos guide the agreed function mapping and appearance; no pixels are copied
from those photos. Generated image data is exported by the official LVGL editor.

## Breathing icon and compass gesture

`breathing_icon.png` is original 100×100 concentric-circle artwork from
`scripts/Generate-Breathing-Icon.py`, guided by the user's app-icon reference.
No photo pixels are copied. It is shared by list and grid launcher entries.
The compass figure-eight path from `Generate-Compass-Assets.py` uses neutral
gray as requested on 2026-10-25; this artwork does not implement calibration.
