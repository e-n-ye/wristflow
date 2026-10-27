"""Audit a verified local Product build without opening a device.

The ELF load span is authoritative. Only nonoverlapping UI resource sections
are attributed from the GNU map; other content and padding form the remainder.
Merged strings make a sum of every map input section incorrect.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess

from lab_check import ROOT, check_build, load, require
from lab_evidence import file_record


def load_segments(output):
    result = []
    for line in output.splitlines():
        fields = line.split()
        if fields and fields[0] == "LOAD":
            require(len(fields) >= 8, "Incomplete readelf LOAD line")
            offset, vma, lma, size, memory = (int(x, 16) for x in fields[1:6])
            result.append(dict(offset=offset, vma=vma, lma=lma,
                               file_bytes=size, memory_bytes=memory))
    require(bool(result), "No ELF load segments")
    return result


def resource_sections(text, start, end):
    text = text.split("Linker script and memory map", 1)[1]
    text = text.split("Cross Reference Table", 1)[0]
    pattern = r"^ (\.[^\s]+)[ \t\r\n]+(0x[0-9a-fA-F]+)[ \t]+(0x[0-9a-fA-F]+)[ \t]+([^\r\n]+)"
    resources = {}
    intervals = []
    for section, address, size, obj in re.findall(pattern, text, re.MULTILINE):
        obj = obj.replace("\\", "/").strip()
        match = re.search(r"/ui/(fonts|images)/([^/]+)\.o$", obj)
        address, size = int(address, 16), int(size, 16)
        # Official EZIP resources are in a generated child of the UI variant.
        if match is None:
            match = re.search(r"/ui/(ezip)/([^/]+)\.tmp\.o$", obj)
        if match is None or not size or not start <= address < end:
            continue
        require(address + size <= end, "Resource exceeds the loaded image")
        kind, name = match.groups()
        kind = "images" if kind == "ezip" else kind
        key = kind + "/" + name
        resources[key] = resources.get(key, 0) + size
        intervals.append((address, address + size, key, section))
    intervals.sort()
    for previous, current in zip(intervals, intervals[1:]):
        require(previous[1] <= current[0], "Overlapping resource map sections")
    require(bool(resources), "No UI resources attributed; unsupported map layout")
    return resources


def audit(result_path, readelf, source_archive=None):
    verified = check_build(result_path, source_archive=source_archive)
    record = load(result_path)
    build = Path(record["build_directory"])
    map_text = (build / "main.map").read_text(encoding="utf-8")
    memory = map_text.split("Memory Configuration", 1)[1].split("Linker script and memory map", 1)[0]
    match = re.search(r"^ROM\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s", memory, re.MULTILINE)
    require(match is not None, "Missing linker application ROM region")
    start, capacity = (int(x, 16) for x in match.groups())
    segments = load_segments(subprocess.check_output(
        [str(readelf), "-lW", str(build / "main.elf")], text=True))
    flash = [s for s in segments if s["file_bytes"] and start <= s["lma"] < start + capacity]
    require(bool(flash), "No Flash load segment in configured application partition")
    end = max(s["lma"] + s["file_bytes"] for s in flash)
    require(end <= start + capacity, "Application exceeds its partition")
    used = end - start
    require((build / "main.bin").stat().st_size == used, "BIN size differs from ELF load span")
    resources = resource_sections(map_text, start, end)
    fonts = sum(n for key, n in resources.items() if key.startswith("fonts/"))
    images = sum(n for key, n in resources.items() if key.startswith("images/"))
    require(fonts + images <= used, "Resource attribution exceeds image size")
    return dict(build_result=file_record(result_path), firmware=file_record(build / "main.bin"),
                elf=file_record(build / "main.elf"), map=file_record(build / "main.map"),
                source_count=len(verified["recorded_sources"]),
                artifact_count=len(verified["artifacts"]),
                line_endings_only=verified["line_endings_only"],
                partition_start=hex(start), partition_bytes=capacity, load_end=hex(end),
                used_bytes=used, free_bytes=capacity - used,
                flash_load_file_bytes=sum(s["file_bytes"] for s in flash),
                categories=dict(ui_fonts=fonts, ui_images=images,
                                other_code_data_padding=used - fonts - images),
                resources=dict(sorted(resources.items(), key=lambda item: (-item[1], item[0]))),
                hardware_verified=False,
                limits="ELF load span includes copied SRAM initializers and gaps. Resource totals exclude linker alignment. Other includes SDK fonts, code, data and padding; it is not code alone.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--result", type=Path, required=True)
    parser.add_argument("--readelf", type=Path, required=True)
    parser.add_argument("--source-archive", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.result, args.readelf, args.source_archive)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("used_bytes", "free_bytes", "categories")}, indent=2))


if __name__ == "__main__":
    main()
