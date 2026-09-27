# UI and notification fonts

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
the glyphs used by the UI; the Bluetooth mark represents Bluetooth.
This is an existing icon library, not a collection of custom drawn symbols.

The current subsets were generated using fontTools 4.66.0. The official Editor v2.0.1
converts them to `*_data.c`. That editor requires one explicit contiguous
`range` for icon fonts; space-separated ranges fail in its converter.

`WristFlowMessages-Regular.ttf` is a separate subset of the same locked Noto
Sans SC source, renamed `WristFlow Messages` under the same OFL. It covers the
source font's Basic CJK and Extension A glyphs, Latin/Latin Extended A/B,
common punctuation, arrows, symbols, kana, bopomofo, compatibility ideographs,
and fullwidth characters. The exact ranges are in `Generate-Ui-Assets.py`.
U+3031 and U+3032 (two-line vertical repeat marks) are excluded to avoid
inflating horizontal text line height. Coverage is limited by the source
font; this is not a promise to render arbitrary Unicode, emoji, or CJK
extensions outside the BMP. The exported input contains 29,598 supported
codepoints: 20,976 Basic CJK and 6,582 Extension A, plus the other ranges.

The official editor generates `notification_22_data.c` at 22 px, 2 bpp.
Its bitmap exceeds the small-format index limit, so Product, UI Demo and
host tests enable `LV_FONT_FMT_TXT_LARGE`. Do not edit generated font data
to bypass that requirement. The resulting font line height is 31 px, and
notification layouts reserve at least 34 px for each single-line label.
