import importlib.util
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("runtime_decode", ROOT / "scripts/runtime_record_decode.py")
decoder = importlib.util.module_from_spec(spec)
spec.loader.exec_module(decoder)


def seal(data):
    struct.pack_into("<I", data, 28, 0)
    struct.pack_into("<I", data, 28, zlib.crc32(data))
    return bytes(data)


def summary():
    data = bytearray(512)
    data[:8] = b"WFRN\x01\x00\x00\x02"
    struct.pack_into("<5I", data, 8, 1, 1, 2, 0, 0)
    struct.pack_into("<8Q", data, 32, 100, 100, 100, 0, 0, 0, 0, 0)
    struct.pack_into("<QIIIHH5B",data,128,100,1,100,0,3800,0,0,0,0,0,0)
    return seal(data)


def capture():
    data = summary()
    checksum = decoder.snapshot_crc(7, 1, 0, 1)
    lines = [f"[record-snapshot] schema=1 token=7 samples=1 saved=0 failed=1 bytes=512 crc={checksum:08x}",
             "[record-summary] token=7 kind=current"]
    lines += [f"[record-data] token=7 offset={i} hex={data[i:i+32].hex()}" for i in range(0, 512, 32)]
    sample=data[128:160]
    lines += ["[record-summary-end] token=7", f"[record-sample] token=7 index=0 crc={zlib.crc32(sample):08x}",
              f"[record-sample-data] token=7 index=0 hex={sample.hex()}",
              "[record-samples-end] token=7 page=0 count=1"]
    return ("\r\n".join(lines)+"\r\n").encode()


class RuntimeDecode(unittest.TestCase):
    def test_complete(self):
        result = decoder.decode_capture(capture(), 7)
        self.assertEqual(result["unsaved_recorded_ms"], 100)
        self.assertTrue(result["failed"])
        self.assertEqual(result["samples"][0]["millivolts"], 3800)

    def test_corruptions(self):
        data = summary()
        for i in range(512):
            bad = bytearray(data)
            bad[i] ^= 1
            with self.assertRaises(ValueError):
                decoder.decode_summary(bad)

    def test_semantic_and_reserved(self):
        for offset, value in ((16, 128), (12, 0), (96+27, 7), (96+29, 1), (240, 9), (248, 1), (48, 101)):
            bad = bytearray(summary())
            bad[offset] = value
            with self.assertRaises(ValueError):
                decoder.decode_summary(seal(bad))

    def test_missing_and_duplicate(self):
        data = capture()
        rows = data.splitlines(keepends=True)
        for i in (0, 1, 4, 18, 19, 20, 21):
            with self.assertRaises(ValueError):
                decoder.decode_capture(b"".join(rows[:i]+rows[i+1:]), 7)
        for i in (0, 4, 19, 20, 21):
            with self.assertRaises(ValueError):
                decoder.decode_capture(b"".join(rows[:i]+[rows[i]]+rows[i:]), 7)

    def test_wrong_token_and_ranges(self):
        for old, new in ((b"token=7", b"token=8"), (b"samples=1", b"samples=65"),
                         (b"crc=", b"crc=zz"), (b"index=0", b"index=65"),
                         (b"bytes=512", b"bytes=1024"), (b"failed=1", b"failed=2")):
            with self.assertRaises(ValueError):
                decoder.decode_capture(capture().replace(old, new), 7)

    def test_bounds(self):
        for raw, token in ((b"x"*(1024*1024+1), 7), (capture(), 0), (capture(), 2**64)):
            with self.assertRaises(ValueError):
                decoder.decode_capture(raw, token)

    def test_manifest_integrity_and_fields(self):
        for old, new in ((b"failed=1", b"failed=0"), (b"saved=0", b"saved=1"),
                         (b"samples=1", b"samples=0"),
                         (b"[record-summary-end] token=7", b"[record-summary-end] token=7 crc=garbled")):
            with self.assertRaises(ValueError):
                decoder.decode_capture(capture().replace(old, new), 7)

    def test_session_and_reset(self):
        rows=capture().splitlines(keepends=True)
        for marker in (b"SFBL reboot\n",b"RT-Thread boot\n",b"[product] storage=ready\n",
                       b"[record-snapshot] schema=1 token=8 samples=0 saved=0 failed=0 bytes=512\n"):
            with self.assertRaises(ValueError):
                decoder.decode_capture(b"".join(rows[:19])+marker+b"".join(rows[19:]),7)

    def test_sample_crc_and_semantic(self):
        rows=capture().splitlines(keepends=True)
        bad=bytearray(summary()[128:160]);bad[28]=9
        rows[19]=f"[record-sample] token=7 index=0 crc={zlib.crc32(bad):08x}\n".encode()
        rows[20]=f"[record-sample-data] token=7 index=0 hex={bad.hex()}\n".encode()
        with self.assertRaises(ValueError):decoder.decode_capture(b"".join(rows),7)
        rows=capture().splitlines(keepends=True);rows[20]=rows[20].replace(b"hex=",b"hex=00",1)
        with self.assertRaises(ValueError):decoder.decode_capture(b"".join(rows),7)

    def test_real_c_export_roundtrip(self):
        producer = ROOT / "artifacts/host-ui-build/product_runtime_record.exe"
        if not producer.exists():
            self.skipTest("Build product_runtime_record host producer first")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "export.uart"
            subprocess.run([str(producer), str(path)], check=True, capture_output=True)
            result = decoder.decode_capture(path.read_bytes(), 1)
            self.assertTrue(result["failed"])
            self.assertIsNotNone(result["saved"])
            self.assertGreater(result["unsaved_recorded_ms"], 0)
            self.assertEqual(result["current"]["failures"], 1)
            self.assertTrue(all(len(line) < 127 for line in path.read_bytes().splitlines()))


if __name__ == "__main__":
    unittest.main()
