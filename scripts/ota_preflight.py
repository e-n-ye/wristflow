"""Inspect an official offline2 main-only zlib package without opening a device.

This is a host preflight, not a packer, firmware installer or signature verifier.
The legacy package CRC covers the image payload only. Header allowlisting and
matching a locally verified build do not add publisher authentication on-device.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import zlib

from lab_check import ROOT, check_build, load, require
from lab_evidence import file_record, write_json


def _crc_table():
    table = []
    for value in range(256):
        value <<= 24
        for _ in range(8):
            value = ((value << 1) ^ (0x04C11DB7 if value & 0x80000000 else 0)) & 0xFFFFFFFF
        table.append(value)
    return tuple(table)


CRC_TABLE = _crc_table()
# Locked SDK dfu_ctrl.c:337-344 selects 4 KiB for non-55x NOR package erase.
NOR_ERASE_BYTES = 4096


def crc32_mpeg2(data):
    crc = 0xFFFFFFFF
    for byte in data:
        crc = ((crc << 8) & 0xFFFFFFFF) ^ CRC_TABLE[(crc >> 24) ^ byte]
    return crc


def inspect_package(package, firmware, main_capacity):
    """Check the locked tool's offline2, one main image, 10 KiB zlib format."""
    require(0 < len(firmware) <= main_capacity, "Firmware exceeds the main partition or is empty")
    require(len(package) >= 26, "Truncated offline2 package")
    magic, version, state, count, expected_crc = struct.unpack_from("<4sBBHI", package)
    require((magic, version, state, count) == (b"FCES", 1, 0xFF, 1),
            "Expected an uninstalled offline2 v1 package containing exactly one image")
    image_id, flags, image_bytes = struct.unpack_from("<BBI", package, 12)
    require((image_id, flags) == (0, 16), "Only compressed main image ID 0 is permitted")
    require(image_bytes == len(package) - 18, "Image length differs from the exact package extent")
    payload = memoryview(package)[18:]
    require(crc32_mpeg2(payload) == expected_crc, "Offline2 payload CRC mismatch")
    decoded_bytes, block_bytes = struct.unpack_from("<II", payload)
    padded = firmware + bytes((-len(firmware)) % 16)
    require(decoded_bytes == len(padded) and decoded_bytes <= main_capacity,
            "Declared decoded extent differs from the build or exceeds main")
    require(block_bytes == 10240, "Unsupported block size; expected the official 10 KiB default")
    position, decoded, blocks = 8, bytearray(), 0
    while len(decoded) < decoded_bytes:
        require(position + 4 <= len(payload), "Missing compressed block length")
        compressed_bytes, = struct.unpack_from("<I", payload, position)
        position += 4
        require(0 < compressed_bytes <= len(payload) - position, "Compressed block exceeds package")
        compressed = payload[position:position + compressed_bytes]
        position += compressed_bytes
        expected = min(block_bytes, decoded_bytes - len(decoded))
        decoder = zlib.decompressobj()
        try:
            block = decoder.decompress(compressed, expected + 1)
        except zlib.error as exc:
            raise ValueError("Invalid zlib block") from exc
        require(len(block) == expected and decoder.eof and not decoder.unused_data
                and not decoder.unconsumed_tail, "Decoded block length/stream extent mismatch")
        decoded.extend(block)
        blocks += 1
    require(position == len(payload), "Trailing bytes after the final compressed block")
    require(decoded == padded, "Decoded image differs from the verified build including alignment")
    return dict(format="sifli-offline2-v1-main-zlib", image_id=image_id,
                source_bytes=len(firmware), aligned_bytes=decoded_bytes,
                package_bytes=len(package), compressed_image_bytes=image_bytes,
                block_bytes=block_bytes, blocks=blocks,
                crc32_mpeg2=f"{expected_crc:08x}",
                aligned_sha256=hashlib.sha256(decoded).hexdigest(),
                exact_build_roundtrip_verified=True,
                header_covered_by_crc=False, board_identity_in_package=False,
                publisher_authentication_verified=False,
                required_device_decoder="DFU_DECOMPRESS_USING_SOFTWARE")


def inspect_partitions(table):
    """Read explicit flash2 geometry; do not infer physical capacity or free tail."""
    memories = [entry for entry in table if entry.get("mem") == "flash2"]
    require(len(memories) == 1, "Expected exactly one flash2 partition map")
    memory = memories[0]
    base = int(memory["base"], 0)
    require(0 <= base <= 0xFFFFFFFF, "Invalid flash2 base")
    tags, intervals = {}, []
    for entry in memory["regions"]:
        offset, size = int(entry["offset"], 0), int(entry["max_size"], 0)
        require(0 <= offset and size > 0 and base + offset + size <= 0x100000000,
                "Invalid or overflowing flash2 region")
        require((base + offset) % NOR_ERASE_BYTES == 0 and size % NOR_ERASE_BYTES == 0,
                "Flash2 region is not aligned to the SDK NOR erase granule")
        region = dict(start=base + offset, bytes=size, end=base + offset + size)
        intervals.append(region)
        for tag in entry.get("tags", []):
            require(tag not in tags, "Duplicate flash2 partition tag: " + tag)
            tags[tag] = region
    intervals.sort(key=lambda item: item["start"])
    for previous, current in zip(intervals, intervals[1:]):
        require(previous["end"] <= current["start"], "Overlapping flash2 partitions")
    require("HCPU_FLASH_CODE" in tags and "DFU_DOWNLOAD_REGION" in tags,
            "Missing main or download partition")
    return tags


def audit(result_path, package_path):
    verified = check_build(result_path)
    record = load(result_path)
    require(record["example"] == "product", "Expected a Product build")
    for flag in ("resource_probe", "storage_probe", "fs_probe", "cycle_log"):
        require(record.get(flag, False) is False, "Expected ordinary Product, not a diagnostic build")
    lock = load(ROOT / "sdk.lock.json")
    partition_path = ROOT / "apps/product/project" / (lock["board"] + "_hcpu") / "ptab.json"
    require(partition_path.resolve() in {Path(item["path"]).resolve()
                                       for item in verified["recorded_sources"]},
            "Partition map is absent from verified build inputs")
    partitions = inspect_partitions(load(partition_path))
    main, download = partitions["HCPU_FLASH_CODE"], partitions["DFU_DOWNLOAD_REGION"]
    firmware_record = next(item for item in verified["images"] if item["name"] == "main.bin")
    firmware_path = Path(firmware_record["path"])
    firmware = firmware_path.read_bytes()
    require(hashlib.sha256(firmware).hexdigest() == firmware_record["sha256"],
            "Firmware changed after build verification")
    # Bound input before reading it; this tool supports the locked 8 MiB main maximum.
    require(0 < Path(package_path).stat().st_size <= 2 * main["bytes"] + 4096,
            "Package is empty or exceeds the bounded preflight input size")
    package = Path(package_path).read_bytes()
    decoded = inspect_package(package, firmware, main["bytes"])
    erase_bytes = (len(package) + NOR_ERASE_BYTES - 1) // NOR_ERASE_BYTES * NOR_ERASE_BYTES
    fits = erase_bytes <= download["bytes"]
    return dict(schema=1, kind="ota-preflight", passed=fits,
                build_result=file_record(result_path), firmware=file_record(firmware_path),
                package=file_record(package_path), partitions=file_record(partition_path),
                sdk_lock=file_record(ROOT / "sdk.lock.json"),
                source_count=len(verified["recorded_sources"]),
                artifact_count=len(verified["artifacts"]),
                package_validation=decoded, main_partition=main, download_partition=download,
                main_free_bytes=main["bytes"] - len(firmware),
                download_free_bytes=download["bytes"] - len(package),
                download_erase_bytes=erase_bytes,
                download_erase_granule=NOR_ERASE_BYTES,
                download_erase_free_bytes=download["bytes"] - erase_bytes,
                fits_current_download=fits, hardware_verified=False,
                deployment_authorized=False,
                limits="Host roundtrip and capacity only; no device authentication, installation or recovery proof.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-result", required=True, type=Path)
    parser.add_argument("--package", required=True, type=Path)
    parser.add_argument("--output", type=Path, help="New JSON file under this checkout's artifacts directory")
    args = parser.parse_args()
    try:
        if args.output:
            output = args.output.resolve()
            require(output.is_relative_to(ROOT / "artifacts") and output.suffix == ".json",
                    "Output must be a JSON report under artifacts")
            require(not output.exists() and not output.with_suffix(".json.tmp").exists(),
                    "Refusing to overwrite an existing report or temporary file")
        report = audit(args.build_result, args.package)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            write_json(args.output, report)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["passed"] else 2
    except (ValueError, KeyError, TypeError, OSError, struct.error) as exc:
        print(json.dumps(dict(passed=False, error=str(exc), hardware_verified=False),
                         ensure_ascii=False))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
