"""Protect the explicit common-Hanzi coverage and deterministic offline profile."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from font_charset import common_hanzi, message_codepoints


class FontCharsetTests(unittest.TestCase):
    def test_exact_common_hanzi_scope(self):
        points = common_hanzi()
        self.assertEqual(len(points), 6500)
        self.assertTrue({ord(c) for c in "中文通知测试图书馆"} <= points)
        self.assertTrue({0x2c27c, 0x20cd0, 0x25ed7} <= points)
        self.assertFalse({0x9f98, 0x9ad4, 0x3400, 0x20bb7} & points)

    def test_latin_punctuation_and_placeholder_retained(self):
        points = message_codepoints()
        self.assertTrue({ord(c) for c in "AZaz09，。！？：℃°→□"} <= points)
        self.assertFalse({0x3031, 0x3032, 0x3042, 0x3105, 0x1f642} & points)

    def test_incomplete_or_duplicate_list_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.txt"
            for text in ("中", "中" * 6500):
                path.write_text(text, encoding="utf-8")
                with self.assertRaises(ValueError):
                    common_hanzi(path)


if __name__ == "__main__":
    unittest.main()
