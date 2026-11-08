"""Fault cases for host inspection of the SDK's main-only offline2 package."""
from pathlib import Path
from contextlib import redirect_stdout
import hashlib
import io
import json
import struct
import sys
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from ota_preflight import crc32_mpeg2, inspect_package, inspect_partitions
import ota_preflight as ota


def fixture(firmware, block_transform=lambda block: block):
    padded = firmware + bytes((-len(firmware)) % 16)
    payload = struct.pack("<II", len(padded), 10240)
    for offset in range(0, len(padded), 10240):
        compressed = zlib.compress(block_transform(padded[offset:offset + 10240]), 9)
        payload += struct.pack("<I", len(compressed)) + compressed
    return (struct.pack("<4sBBHI", b"FCES", 1, 0xFF, 1, crc32_mpeg2(payload))
            + struct.pack("<BBI", 0, 16, len(payload)) + payload)


def with_payload_crc(package):
    package = bytearray(package)
    struct.pack_into("<I", package, 8, crc32_mpeg2(package[18:]))
    return bytes(package)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.firmware = bytes(range(256)) * 45 + b"unaligned"
        self.package = fixture(self.firmware)
        self.capacity = 16384

    def test_independent_crc_known_vector_and_multiblock_alignment(self):
        self.assertEqual(crc32_mpeg2(b"123456789"), 0x0376E6E7)
        report = inspect_package(self.package, self.firmware, self.capacity)
        self.assertEqual(report["blocks"], 2)
        self.assertEqual(report["aligned_bytes"] % 16, 0)
        self.assertTrue(report["exact_build_roundtrip_verified"])
        self.assertFalse(report["publisher_authentication_verified"])
        self.assertFalse(report["header_covered_by_crc"])

    def test_payload_damage_truncation_and_trailing_bytes(self):
        changed = bytearray(self.package)
        changed[-1] ^= 1
        for invalid in (changed, self.package[:-1], self.package + b"extra", b""):
            with self.subTest(size=len(invalid)), self.assertRaises(ValueError):
                inspect_package(invalid, self.firmware, self.capacity)

    def test_uncovered_header_cannot_change_target_or_extent(self):
        # None of these changes affects the legacy payload CRC.
        for offset, value in ((0, 0), (4, 2), (5, 0x7F), (6, 2),
                              (12, 6), (12, 10), (13, 0), (14, 0)):
            invalid = bytearray(self.package)
            invalid[offset] = value
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                inspect_package(invalid, self.firmware, self.capacity)

    def test_valid_crc_cannot_hide_wrong_build_or_padding(self):
        changed = bytes([self.firmware[0] ^ 1]) + self.firmware[1:]
        with self.assertRaises(ValueError):
            inspect_package(fixture(changed), self.firmware, self.capacity)
        bad_padding = fixture(self.firmware, lambda block: block[:-1] + b"x")
        with self.assertRaises(ValueError):
            inspect_package(bad_padding, self.firmware, self.capacity)

    def test_bounded_decompression_and_declared_extent(self):
        cases = [fixture(self.firmware, lambda block: block * 100)]
        for offset, value in ((18, 0xFFFFFFFF), (22, 0xFFFFFFFF), (26, 0xFFFFFFFF)):
            invalid = bytearray(self.package)
            struct.pack_into("<I", invalid, offset, value)
            cases.append(with_payload_crc(invalid))
        for invalid in cases:
            with self.subTest(size=len(invalid)), self.assertRaises(ValueError):
                inspect_package(invalid, self.firmware, self.capacity)
        with self.assertRaises(ValueError):
            inspect_package(self.package, self.firmware, 1024)

    def test_recomputed_crc_does_not_hide_extra_zlib_stream(self):
        payload = bytearray(self.package[18:])
        block_length, = struct.unpack_from("<I", payload, 8)
        extra = zlib.compress(b"unexpected")
        payload[12 + block_length:12 + block_length] = extra
        struct.pack_into("<I", payload, 8, block_length + len(extra))
        invalid = bytearray(self.package[:18]) + payload
        struct.pack_into("<I", invalid, 14, len(payload))
        with self.assertRaises(ValueError):
            inspect_package(with_payload_crc(invalid), self.firmware, self.capacity)


class PartitionTests(unittest.TestCase):
    def table(self):
        return [{"mem": "flash2", "base": "0x12000000", "regions": [
            {"offset": "0x20000", "max_size": "0x800000", "tags": ["HCPU_FLASH_CODE"]},
            {"offset": "0x880000", "max_size": "0x120000", "tags": ["DFU_DOWNLOAD_REGION"]},
        ]}]

    def test_exact_capacity_and_address(self):
        tags = inspect_partitions(self.table())
        self.assertEqual(tags["DFU_DOWNLOAD_REGION"]["bytes"], 1179648)
        self.assertEqual(tags["HCPU_FLASH_CODE"]["start"], 0x12020000)

    def test_overlap_duplicate_and_32_bit_overflow_rejected(self):
        for field, value in (("offset", "0x40000"), ("offset", "0x880001"),
                             ("max_size", "0x120001"), ("max_size", "0xFFFFFFFF"),
                             ("tags", ["HCPU_FLASH_CODE"])):
            table = self.table()
            table[0]["regions"][1][field] = value
            with self.subTest(field=field), self.assertRaises(ValueError):
                inspect_partitions(table)


class BindingTests(unittest.TestCase):
    def audit(self, firmware, package, diagnostic=False, changed_after_check=False):
        firmware_path = ota.ROOT / "artifacts/verified/main.bin"
        partition_path = ota.ROOT / "apps/product/project/sf32lb52-lchspi-ulp_hcpu/ptab.json"
        # The raw record deliberately points elsewhere; only check_build's
        # normalized, verified image is authoritative, irrespective of cwd.
        build = dict(example="product", firmware_path="decoy/main.bin", cycle_log=diagnostic)
        verified = dict(recorded_sources=[dict(path=str(partition_path))], artifacts=[{}],
                        images=[dict(name="main.bin", path=str(firmware_path),
                                     sha256=hashlib.sha256(firmware).hexdigest())])
        def load(path):
            if str(path) == "build.json":
                return build
            if Path(path).name == "sdk.lock.json":
                return {"board": "sf32lb52-lchspi-ulp"}
            return PartitionTests().table()
        def read_bytes(path):
            if path == firmware_path:
                return firmware + (b"changed" if changed_after_check else b"")
            if str(path) == "package.bin":
                return package
            raise AssertionError("Read an unverified path: " + str(path))
        with patch.object(ota, "check_build", return_value=verified) as check, \
             patch.object(ota, "load", side_effect=load), \
             patch.object(Path, "read_bytes", read_bytes), \
             patch.object(Path, "stat", return_value=SimpleNamespace(st_size=len(package))), \
             patch.object(ota, "file_record", side_effect=lambda path: {"path": str(path)}):
            report = ota.audit(Path("build.json"), Path("package.bin"))
            check.assert_called_once_with(Path("build.json"))
            return report

    def test_reads_only_normalized_verified_image_and_reports_limits(self):
        firmware = b"verified-build" * 20
        report = self.audit(firmware, fixture(firmware))
        self.assertTrue(report["passed"])
        self.assertEqual(report["download_erase_bytes"], 4096)
        self.assertFalse(report["hardware_verified"] or report["deployment_authorized"])

    def test_rejects_diagnostic_build_and_postverification_changes(self):
        firmware = b"verified-build" * 20
        for flags in ({"diagnostic": True}, {"changed_after_check": True}):
            with self.subTest(flags=flags), self.assertRaises(ValueError):
                self.audit(firmware, fixture(firmware), **flags)

    def test_cli_capacity_rejection_and_invalid_input_have_distinct_exit_codes(self):
        for returned, expected_exit in (({"passed": False}, 2), ({"passed": True}, 0)):
            with patch.object(sys, "argv", ["ota_preflight", "--build-result", "build.json", "--package", "package.bin"]), \
                 patch.object(ota, "audit", return_value=returned), redirect_stdout(io.StringIO()) as output:
                self.assertEqual(ota.main(), expected_exit)
                self.assertEqual(json.loads(output.getvalue())["passed"], returned["passed"])


if __name__ == "__main__":
    unittest.main()
