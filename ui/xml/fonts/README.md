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

The original subsets were generated using fontTools 4.66.0. The 2026-10-11
static UI addition for the alarm capacity text used fontTools 4.60.1;
message and icon subsets were preserved in that change. The 2026-10-27 common
notification subset uses fontTools 4.60.1. The official Editor v2.0.1
converts them to `*_data.c`. That editor requires one explicit contiguous
`range` for icon fonts; space-separated ranges fail in its converter.

For additions to static UI text or icons, update the glyph lists in
`scripts/Generate-Ui-Assets.py` and run it with `--static-fonts-only`, recording
the fontTools version in the relevant evidence. This updates only the static text and icon TTF subsets;
it does not regenerate notification fonts or images. Check the source TTF
cmap for every new codepoint, then export and recompile in the official
editor. Expanding XML `range` or `symbols` cannot restore a glyph already
removed from the source subset.

`WristFlowMessages-Regular.ttf` is a separate subset of the same locked Noto
Sans SC source, renamed `WristFlow Messages` under the same OFL. On 2026-10-27
the user selected the 6,500 Hanzi in levels 1/2 of the 2013 General Standard
Chinese Characters table, plus Latin, digits, punctuation and useful symbols.
The actual TTF and official export contain **7,174 codepoints: 6,500 Hanzi and
674 supplemental characters**. Supplemental candidate ranges are intersected
with the source font; they are not a promise of every codepoint in each range.

The ordered Hanzi list is committed at
[`scripts/data/tgh2013-levels-1-2.txt`](../../../scripts/data/tgh2013-levels-1-2.txt).
It is derived from Unicode 17.0.0 Unihan `kTGH` entries for 2013, indices 1..6500;
the pinned archive/mapping hashes and Unicode License V3 accompany the list.
[`font_charset.py`](../../../scripts/font_charset.py) validates/imports the list
offline and defines the supplemental candidates. Source-font coverage of all
6,500 Hanzi is required before generation.

Three selected level-2 Hanzi are outside the BMP: U+20CD0, U+25ED7, U+2C27C.
The XML export range extends through `0x2ffff` so those three are included;
the already-subset TTF controls actual coverage. Kana, bopomofo, broad CJK
Extension A and compatibility-ideograph ranges are no longer retained wholesale.
Missing characters use LVGL's existing placeholder; notification text itself
is not replaced or rewritten. Full traditional Chinese, rare personal names
and emoji are not guaranteed.

Regenerate only this TTF with
`uv run --with fonttools==4.60.1 --no-project python scripts/Generate-Ui-Assets.py --message-font-only`,
then export and compile with the official Editor. Fixed UI/icon subsets and
images are preserved by this mode. No network is used by routine generation.

The official editor generates `notification_22_data.c` at 22 px, 2 bpp,
with `--no-compress` and bitmap format 0 (plain). Font decompression remains
disabled. Product, UI Demo and host tests retain `LV_FONT_FMT_TXT_LARGE`;
this change does not alter the font ABI. Do not edit generated font data.
Line height remains 31 px, and notification layouts reserve at least 34 px.

Historical comparison: the prior notification subset contained 29,598 BMP
codepoints (20,976 Basic CJK and 6,582 Extension A plus other ranges) and cost
3,832,051 linked bytes. The common subset costs 914,067 linked bytes. It is
not a lossless whole-font compression: unsupported characters are deliberately
removed, and official rerasterization produces minor pixel differences in some
retained glyphs. See [Flash evidence](../../../docs/FLASH-CAPACITY.md).
