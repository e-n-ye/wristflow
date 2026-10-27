"""Host-only official EZIP conversion/roundtrip probe; never changes UI sources.

Run with Pillow 11.3.0. All converted files stay in a new output directory.
The pixel comparison uses the existing official XML export as the oracle.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

from PIL import Image

from lab_check import ROOT, check_sdk, load, require
from lab_evidence import file_record
from ezip_rgb565 import exported_image, prepare

SAMPLES = ("hour_1", "hour_7", "hour_unknown")


def pixels565(image):
    result = bytearray()
    for r, g, b in image.convert("RGB").getdata():
        result.extend((((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)).to_bytes(2, "little"))
    return bytes(result)


def exported_pixels(path, symbol):
    text = path.read_text(encoding="utf-8")
    match = re.search(r"\b" + re.escape(symbol) + r"_map\s*\[[^\]]*\][^=]*=\s*\{([^}]+)\}", text)
    require(match is not None, "Missing exported image map")
    return bytes(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})\b", match.group(1)))


def native(path):
    # This locked Windows tool splits source basenames on backslashes only.
    return str(path.resolve()).replace("/", "\\")


def convert(tool, source, destination, flags, log):
    destination.mkdir(parents=True, exist_ok=True)
    command = [str(tool), "-convert", native(source), *flags, "-outdir", native(destination)]
    process = subprocess.run(command, capture_output=True)
    log.write_bytes(process.stdout + process.stderr)
    require(process.returncode == 0, "Official converter failed; see " + str(log))
    require(b"converted!!!" in process.stdout and b"error_num 0" in process.stdout,
            "Official converter did not confirm conversion; see " + str(log))
    return command


def probe(output):
    require(not output.exists(), "Choose a new output directory; prior evidence is preserved")
    sdk = check_sdk(ROOT, load(ROOT / "sdk.lock.json"))
    tool = ROOT / "vendor/SiFli-SDK/tools/png2ezip/ezip.exe"
    output.mkdir(parents=True)
    samples = []
    for name in SAMPLES:
        directory = output / name
        directory.mkdir()
        original = ROOT / "ui/xml/images" / (name + ".png")
        source = directory / (name + "_data.png")
        exported = ROOT / "ui/xml/images" / (name + "_data.c")
        width, height, expected = exported_image(exported)
        dimensions = (width, height)
        prepare(exported, source)
        with Image.open(source) as image:
            require(pixels565(image) == expected, "Temporary PNG differs from existing RGB565 export")
        commands = []
        flags = ["-rgb565", "-lvgl_version", "9", "-chip", "sf52x", "-dpt", "1", "-section", "rodata"]
        commands.append(convert(tool, source, directory, flags + ["-cfile", "2"], directory / "encode-c.log"))
        commands.append(convert(tool, source, directory, flags + ["-binfile", "2"], directory / "encode-bin.log"))
        generated = directory / (name + "_data.c")
        binary = directory / (name + "_data.bin")
        require(generated.is_file() and binary.is_file(), "Converter output missing")
        text = generated.read_text(encoding="utf-8")
        require(re.search(r"lv_image_dsc_t\s+" + name + r"_data\s", text), "Descriptor name changed")
        require(".header.flags = LV_IMAGE_FLAGS_USER1" in text, "Missing EZIP descriptor flag")
        compressed = exported_pixels(generated, name + "_data")
        require(binary.read_bytes()[12:] == compressed, "V9 binary header/payload differs from C output")
        payload = directory / "payload.bin"
        payload.write_bytes(compressed)
        decoded = directory / "decoded"
        commands.append(convert(tool, payload, decoded,
                                ["-spt", "1", "-dpt", "0", "-dec_off_no_header", "0"], directory / "decode.log"))
        pngs = list(decoded.glob("*.png"))
        require(len(pngs) == 1, "Expected one decoded PNG")
        with Image.open(pngs[0]) as image:
            require(image.size == dimensions, "Roundtrip dimensions differ")
            require(pixels565(image) == expected, "EZIP roundtrip differs from existing RGB565 export")
        samples.append(dict(name=name, dimensions=dimensions, original=file_record(original),
                            exported=file_record(exported), prepared=file_record(source), generated=file_record(generated),
                            binary=file_record(binary), decoded=file_record(pngs[0]), commands=commands,
                            raw_pixel_bytes=len(expected), ezip_payload_bytes=len(compressed),
                            payload_saving_bytes=len(expected) - len(compressed), roundtrip_equal=True))
    report = dict(sdk=sdk, tool=file_record(tool), samples=samples,
                  total_payload_saving_bytes=sum(s["payload_saving_bytes"] for s in samples),
                  hardware_verified=False,
                  limits="RGB565 pixel roundtrip only; Product link delta, EPIC display, latency, memory peaks and power require separate validation.")
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = probe(args.output.resolve())
    print(json.dumps({"samples": [{key: sample[key] for key in ("name", "raw_pixel_bytes", "ezip_payload_bytes", "roundtrip_equal")} for sample in report["samples"]],
                      "total_payload_saving_bytes": report["total_payload_saving_bytes"]}, indent=2))


if __name__ == "__main__":
    main()
