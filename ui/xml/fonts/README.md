# Demo Font

`WristFlowSans-Regular.ttf` is a glyph subset of the fixed SDK's
`external/lvgl_v9/tests/src/test_files/fonts/noto/NotoSansSC-Regular.ttf`.
It retains ASCII and the Chinese characters needed by these static demos.
It is distributed under SIL Open Font License 1.1; see `OFL.txt`.

The reserved name in the source license is `Source`. This subset uses the
family name `WristFlow Sans` to distinguish it from the original font.
It is a demo subset, not a complete font for incoming notification text.

The editor converts this TTF to embedded bitmap fonts at the sizes declared
in `globals.xml`. Generated fonts and their subsets remain covered by the OFL.

`WristFlowIcons.ttf` is a renamed subset of the fixed SDK's
`scripts/built_in_font/FontAwesome5-Solid+Brands+Regular.woff` (Fonticons, Inc.).
Its OFL and attribution are retained in `ICONS-LICENSE.txt`. It contains only
the ten glyphs used by the demo; the Bluetooth mark represents Bluetooth.
This is an existing icon library, not a collection of custom drawn symbols.

Subsets were generated using fontTools 4.60.1. The official Editor v2.0.1
converts them to `*_data.c`. That editor requires one explicit contiguous
`range` for icon fonts; space-separated ranges fail in its converter.
