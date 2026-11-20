"""Offline reference models and independent readback checks for the 06 device window."""
import argparse
from pathlib import Path
import struct
from zipfile import ZipFile
import zlib

from lab_evidence import file_record, write_json
from ota_first_install import ROOT, read_json, require, save, sha
import ota_journal_window as dma

ENTER = "enter-health-b"
INSPECT = "inspect-health-b"
RESTORE = "restore-health-b"
POST_05 = "116219ecabdcf9c5ce01043e821ad3885d86e6251a199e5a166f1ede23bc95ed"
STABLE_06 = "3e24c4b965433b6225b001c3a47744e833594c7663ba25cf7a9efa097f18ea47"
MODELS = {ENTER: POST_05, INSPECT: STABLE_06, RESTORE: POST_05}
FILE = "06-WristFlow-2026111802.bin"
PROTECTED = (dma.PROTECTED[0], ("boot", 0x10000, 0x1f000), *dma.PROTECTED[1:])
OWNED = (("bank1", 0xda2000, 0xda4000), ("bank0", 0xda0000, 0xda2000),
         ("b-body", 0x420000, 0x705000), ("b-tail", 0x81f000, 0x820000))


def prepare(out):
    original = dma.baseline().read_bytes()
    physical = read_json(ROOT / "docs/evidence/2026-11-19/ota-05-dma-finished.json")["physical_check"]
    media = ROOT / physical["capture"] / "media"
    before = bytearray(original)
    for _, off, end in dma.chunks(dma.PROTECTED + dma.OWNED):
        data = (media / f"key-{off:08x}.bin").read_bytes()
        require(len(data) == end-off, "Short 05 physical readback")
        before[off:end] = data
    require(sha(before) == POST_05, "05 model differs from the sealed rollback state")
    release = read_json(ROOT / "docs/evidence/2026-11-18/ota-product-health.json")
    with ZipFile(ROOT / release["phone_kit"]) as kit:
        matches = [item for item in kit.infolist() if item.filename == FILE]
        require(len(matches) == 1 and matches[0].file_size == release["package_bytes"], "Ambiguous 06 fixture")
        wire = kit.read(matches[0])
    body, header = wire[354:], wire[18:354]
    require(sha(wire) == release["package_sha256"] and sha(body) == release["main_sha256"] and
            header[48:80] == bytes.fromhex(sha(body)) and struct.unpack_from("<II", header, 40) ==
            (len(body), 2026111802), "06 signed release identity differs")
    after = bytearray(before)
    after[0x420000:0x705000] = body.ljust(0x2e5000, b"\xff")
    after[0x81f000:0x820000] = header.rjust(4096, b"\xff")
    for off, generation, stable, phase, pending in ((0xda0000, 7, 1, 0, 255), (0xda2000, 6, 0, 2, 1)):
        record = before[0xda2000:0xda2080]
        struct.pack_into("<I3B", record, 8, generation, stable, phase, pending)
        record[56:96] = header[40:80]
        struct.pack_into("<I", record, 124, zlib.crc32(record[:124]))
        after[off:off+8192] = record.ljust(8192, b"\xff")
    require(sha(after) == STABLE_06, "Stable 06 model differs")
    out.mkdir(parents=True, exist_ok=False)
    for name, data in (("post-05-model", before), ("stable-06-model", after), ("boot", before[0x10000:0x1f000]),
                       ("receiver", before[0x820000:0x879000]), ("journal", original[0xda0000:0xda1000])):
        save(out / (name + ".bin"), data)
    write_json(out / "models.json", dict(scope="06-host-models", hardware_verified=False,
               files=[file_record(p) for p in sorted(out.glob("*.bin"))]))


def transfers(directory, action, model):
    require(action in MODELS and sha(model) == MODELS[action], "Wrong 06 window reference")
    reads, writes = [], []
    for role, off, end in dma.chunks(PROTECTED + OWNED):
        name = f"key-{off:08x}"
        actual = (directory / (name + ".bin")).read_bytes()
        want = model[off:end]
        require(len(actual) == len(want), "Short 06 physical readback")
        if action == RESTORE and role in {"bank1", "bank0", "b-body", "b-tail"}:
            if actual != want:
                writes.append((role, off, want))
        else:
            require(actual == want, f"Unexpected 06 {role} at {off:#x}")
        reads.append((name, off, actual))
    if action == RESTORE:
        reads.extend((f"{role}-{off:08x}", off, data) for role, off, data in writes)
        reads.extend((f"restored-{off:08x}", off, model[off:end]) for _, off, end in dma.chunks(OWNED))
    else:
        settings = (directory / "settings.bin").read_bytes()
        require(len(settings) == 16384, "Incomplete settings snapshot")
        reads.append(("settings", 0xda8000, settings))
    return reads, writes


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--prepare", type=Path, required=True)
    prepare(parser.parse_args().prepare.resolve())
