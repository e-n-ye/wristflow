import sys
from pathlib import Path
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from ota_slot_check import check_entry


class SlotEntryGuard(unittest.TestCase):
    def test_vector_entry_accepts_thumb_symbol(self):
        check_entry('  Entry point address:               0x12020475\n', 0x12020475)
        check_entry('  Entry point address:               0x12020474\n', 0x12020475)

    def test_rejects_fallback_main_entry(self):
        with self.assertRaisesRegex(ValueError, 'bypasses'):
            check_entry('  Entry point address:               0x12020400\n', 0x12020475)

    def test_rejects_missing_entry(self):
        with self.assertRaisesRegex(ValueError, 'bypasses'):
            check_entry('invalid header', 0x12020475)


if __name__ == '__main__':
    unittest.main()
