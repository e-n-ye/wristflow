"""Notification font profile: TGH2013 levels 1/2 plus Latin and UI symbols.

The committed ordered Hanzi list is imported offline from pinned Unicode data.
Font generation uses that local list; it never downloads data implicitly.
"""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]
COMMON_HANZI = ROOT / "scripts/data/tgh2013-levels-1-2.txt"
UNIHAN_SHA256 = "f7a48b2b545acfaa77b2d607ae28747404ce02baefee16396c5d2d7a8ef34b5e"
# Retain Latin, punctuation, currency, arrows/math and useful UI symbols.
# Kana, bopomofo, CJK radicals and compatibility ideographs are outside this profile.
SUPPLEMENT_RANGES = ((0x20, 0x024f), (0x2000, 0x206f), (0x20a0, 0x20cf),
                     (0x2190, 0x22ff), (0x25a0, 0x26ff), (0x3000, 0x3030),
                     (0xfe10, 0xfe1f), (0xfe50, 0xfe6f), (0xff00, 0xff65),
                     (0xffe0, 0xffef))


def common_hanzi(path=COMMON_HANZI):
    text = "".join(line for line in Path(path).read_text(encoding="utf-8").splitlines()
                   if not line.startswith("#"))
    text = "".join(text.split())
    points = {ord(char) for char in text}
    if len(text) != 6500 or len(points) != 6500:
        raise ValueError("Expected exactly 6500 unique TGH2013 level 1/2 Hanzi")
    if any(not (0x3400 <= cp <= 0x4dbf or 0x4e00 <= cp <= 0x9fff or
                0x20000 <= cp <= 0x2ee5f) for cp in points):
        raise ValueError("Unexpected non-CJK entry in common Hanzi list")
    return points


def message_codepoints():
    points = common_hanzi()
    for first, last in SUPPLEMENT_RANGES:
        points.update(range(first, last + 1))
    points.update(ord(char) for char in "℃℉№™✓✕")
    return points


def import_unihan(archive, output):
    archive = Path(archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest() != UNIHAN_SHA256:
        raise ValueError("Expected pinned Unicode 17.0.0 Unihan.zip")
    with zipfile.ZipFile(archive) as source:
        data = source.read("Unihan_OtherMappings.txt")
    rows = []
    for line in data.decode("utf-8").splitlines():
        parts = line.split("\t")
        if len(parts) == 3 and parts[1] == "kTGH":
            year, index = parts[2].split(":")
            if year != "2013":
                raise ValueError("Unexpected TGH table year")
            rows.append((int(index), chr(int(parts[0][2:], 16))))
    rows.sort()
    if [index for index, _ in rows] != list(range(1, 8106)) or len({c for _, c in rows}) != 8105:
        raise ValueError("TGH2013 mapping must contain unique, consecutive indices 1..8105")
    chars = "".join(char for _, char in rows[:6500])
    header = ("# TGH2013 levels 1/2, indices 1..6500, in table order.\n"
              "# Derived from Unicode 17.0.0 Unihan_OtherMappings.txt, kTGH.\n"
              "# https://www.unicode.org/Public/17.0.0/ucd/Unihan.zip\n"
              "# Copyright © 2025 Unicode®, Inc. Unicode License V3; see UNICODE-LICENSE.txt.\n"
              "# Mapping SHA256: " + hashlib.sha256(data).hexdigest() + "\n")
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(header + "\n".join(chars[i:i + 50] for i in range(0, len(chars), 50)) + "\n",
                      encoding="utf-8")
    common_hanzi(output)
    print(f"Imported 6500 Hanzi to {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--import-unihan", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=COMMON_HANZI)
    args = parser.parse_args()
    import_unihan(args.import_unihan, args.output)
