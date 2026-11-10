from pathlib import Path
import struct
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'scripts'))
from ota_backup_review import FLASH_BASE, FLASH_BYTES, IMAGE_OFFSET, RUN_OFFSET, inspect, review_pair


def backup():
    data = bytearray(b'\xff') * FLASH_BYTES
    struct.pack_into('<I', data, 0, 0x53454346)
    for core, index, length, base, size, execution in (
        (1, 3, 58496, 0x12010000, 0x10000, 0x20020000),
        (2, 4, 3028888, 0x12020000, 0x800000, 0x12020000),
    ):
        offset = IMAGE_OFFSET + (index - 2) * 512
        struct.pack_into('<4I', data, 4 + 16 * index, base, size, execution, 0)
        struct.pack_into('<IHH', data, offset, length, 512, 2)
        struct.pack_into('<I', data, RUN_OFFSET + core * 4, FLASH_BASE + offset)
    struct.pack_into('<II', data, 0x10000, 0x20042000, 0x20020101)
    return data


class BackupReview(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = backup()

    def test_supported_original_layout(self):
        result = inspect(self.original)
        self.assertEqual(result['boot']['length'], 58496)
        self.assertEqual(result['main']['length'], 3028888)
        self.assertEqual(sum(r['bytes'] for r in result['regions'].values()), FLASH_BYTES)

    def test_rejects_truncation_and_bad_magic(self):
        with self.assertRaises(ValueError):
            inspect(self.original[:-1])
        damaged = self.original.copy(); damaged[0] ^= 1
        with self.assertRaisesRegex(ValueError, 'magic'):
            inspect(damaged)

    def test_rejects_running_pointer_not_header(self):
        for pointer in (0xffffffff, FLASH_BASE, FLASH_BASE + IMAGE_OFFSET + 1,
                        FLASH_BASE + RUN_OFFSET):
            damaged = self.original.copy(); struct.pack_into('<I', damaged, RUN_OFFSET + 4, pointer)
            with self.subTest(pointer=pointer), self.assertRaises(ValueError):
                inspect(damaged)

    def test_refuses_encryption_flags_and_wrong_boot(self):
        for offset, value in ((IMAGE_OFFSET + 512 + 6, 3), (4 + 16 * 3 + 8, 0x12010000)):
            damaged = self.original.copy(); struct.pack_into('<I' if offset % 4 == 0 else '<H', damaged, offset, value)
            with self.subTest(offset=offset), self.assertRaises(ValueError):
                inspect(damaged)

    def test_refuses_main_that_would_overlap_b(self):
        damaged = self.original.copy(); struct.pack_into('<I', damaged, IMAGE_OFFSET + 2 * 512, 0x400001)
        with self.assertRaisesRegex(ValueError, 'overlaps'):
            inspect(damaged)

    def test_rejects_bad_boot_vectors(self):
        for sp, pc in ((0x20042001, 0x20020101), (0x20040000, 0x20020101),
                       (0x20042000, 0x20020100), (0x20042000, 0x12020101)):
            damaged = self.original.copy(); struct.pack_into('<II', damaged, 0x10000, sp, pc)
            with self.subTest(sp=sp, pc=pc), self.assertRaises(ValueError):
                inspect(damaged)

    def test_rejects_truncated_reset_instruction(self):
        damaged = self.original.copy()
        struct.pack_into('<I', damaged, IMAGE_OFFSET + 512, 0x101)
        with self.assertRaisesRegex(ValueError, 'vectors'):
            inspect(damaged)

    def test_pair_requires_distinct_and_equal_files(self):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory)/'one.bin', Path(directory)/'two.bin'
            a.write_bytes(self.original); b.write_bytes(self.original)
            result = review_pair(a, b)
            self.assertTrue(result['backup_files_match'])
            self.assertFalse(result['deployment_authorized'])
            self.assertFalse(result['rom_rescue_verified'])
            with self.assertRaises(ValueError):
                review_pair(a, a)
            with b.open('r+b') as stream:
                stream.seek(0x420000); stream.write(b'changed')
            with self.assertRaisesRegex(ValueError, 'differ'):
                review_pair(a, b)


if __name__ == '__main__':
    unittest.main()
