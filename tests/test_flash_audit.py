"""Fail-closed checks for the host-only Flash accounting/image preprocessor."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from flash_audit import load_segments, resource_sections
from ezip_rgb565 import exported_image


class FlashAuditTests(unittest.TestCase):
    def test_load_address_not_runtime_address(self):
        rows = load_segments("LOAD 0x1000 0x20002000 0x12020000 0x40 0x80 RW 0x1000")
        self.assertEqual(rows[0]["lma"], 0x12020000)
        self.assertEqual(rows[0]["file_bytes"], 64)
        self.assertEqual(rows[0]["memory_bytes"], 128)

    def test_ui_sections_exclude_discarded_and_other_merged_strings(self):
        text = """ .rodata.x 0x00000000 0x10 build/ui/images/discarded.o
Linker script and memory map
 .rodata.x 0x12020000 0x10 build/ui/images/a_data.o
 .rodata.y
                0x12020010 0x8 build/ui/ezip/b_data.tmp.o
 .rodata.str1.1 0x12020020 0x20 build/sdk/a.o
 .rodata.str1.1 0x12020020 0x30 build/sdk/b.o
Cross Reference Table
"""
        self.assertEqual(resource_sections(text, 0x12020000, 0x12020100),
                         {"images/a_data": 16, "images/b_data": 8})

    def test_resource_overlap_rejected(self):
        text = """Linker script and memory map
 .rodata.x 0x12020000 0x10 build/ui/images/a_data.o
 .rodata.y 0x12020008 0x10 build/ui/images/b_data.o
"""
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            resource_sections(text, 0x12020000, 0x12020100)

    def test_preprocessor_rejects_alpha_stride_and_truncation(self):
        valid = """const uint8_t a_map[] = {0x00,0x00,0xff,0xff};
 .cf = LV_COLOR_FORMAT_RGB565,
 .w = 2,
 .h = 1,
 .stride = 4,
"""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "image.c"
            path.write_text(valid, encoding="utf-8")
            self.assertEqual(exported_image(path), (2, 1, bytes([0, 0, 255, 255])))
            for broken in (valid.replace("RGB565,", "ARGB8888,"),
                           valid.replace(".stride = 4", ".stride = 8"),
                           valid.replace("0x00,0x00,", "")):
                path.write_text(broken, encoding="utf-8")
                with self.assertRaises(ValueError):
                    exported_image(path)


if __name__ == "__main__":
    unittest.main()
