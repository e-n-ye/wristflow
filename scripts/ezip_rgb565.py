"""Repack an official exported RGB565 image as a temporary, lossless RGB PNG.

This is image preprocessing for the SDK converter, not an XML exporter. Keep
the official descriptor/C source intact. Only opaque, packed RGB565 is accepted.
No third-party package is needed inside the locked SDK Python environment.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import struct
import zlib


def exported_image(path):
    text = Path(path).read_text(encoding="utf-8")
    if not re.search(r"\.cf\s*=\s*LV_COLOR_FORMAT_RGB565\s*,", text):
        raise ValueError("Expected official packed RGB565 export")
    values = {}
    for field in ("w", "h", "stride"):
        match = re.search(r"\." + field + r"\s*=\s*(\d+)\s*,", text)
        if not match:
            raise ValueError("Missing image field: " + field)
        values[field] = int(match.group(1))
    width, height = values["w"], values["h"]
    if not (0 < width <= 4096 and 0 < height <= 4096 and values["stride"] == width * 2):
        raise ValueError("Unsupported image geometry/stride")
    match = re.search(r"\b(\w+)_map\s*\[[^\]]*\][^=]*=\s*\{([^}]+)\}", text)
    if not match:
        raise ValueError("Missing official exported pixel array")
    pixels = bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})\b", match.group(2)))
    if len(pixels) != width * height * 2:
        raise ValueError("Exported RGB565 payload length differs from geometry")
    return width, height, pixels


def chunk(name, data):
    return struct.pack(">I", len(data)) + name + data + struct.pack(">I", zlib.crc32(name + data))


def prepare(source, target):
    width, height, pixels = exported_image(source)
    rows = bytearray()
    for y in range(height):
        rows.append(0)  # PNG filter: None.
        for x in range(width):
            index = (y * width + x) * 2
            value = int.from_bytes(pixels[index:index + 2], "little")
            r, g, b = value >> 11, (value >> 5) & 63, value & 31
            rows.extend(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b"")
    target = Path(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(png)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("target", type=Path)
    args = parser.parse_args()
    prepare(args.source, args.target)
