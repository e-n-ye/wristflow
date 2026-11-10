"""Inspect two explicit NOR backup files offline; never opens a device.

This recognizes only the locked SDK's SF32LB52 unencrypted FTAB layout.
Matching files and plausible headers are evidence inputs, not ROM rescue proof
or permission to write. Unknown layouts fail closed and backups stay intact.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

FLASH_BASE, FLASH_BYTES = 0x12000000, 0x1000000
HEADER_BYTES, IMAGE_OFFSET, RUN_OFFSET = 512, 4096, 4096 + 14 * 512
REGIONS = {
    'ftab_and_calibration': (0, 0x10000),
    'boot': (0x10000, 0x20000),
    'a': (0x20000, 0x420000),
    'b': (0x420000, 0x820000),
    'loader': (0x820000, 0x880000),
    'download': (0x880000, 0x9a0000),
    'fs': (0x9a0000, 0xda0000),
    'kvdb': (0xda0000, 0xdac000),
    'remaining_flash': (0xdac000, FLASH_BYTES),
}


def require(value, reason):
    if not value:
        raise ValueError(reason)


def image_header(data, core):
    pointer = struct.unpack_from('<I', data, RUN_OFFSET + 4 * core)[0]
    offset = pointer - FLASH_BASE - IMAGE_OFFSET
    require(offset >= 0 and offset % HEADER_BYTES == 0 and offset // HEADER_BYTES < 14,
            'Running image pointer outside fixed FTAB headers')
    image = offset // HEADER_BYTES
    length, block, flags = struct.unpack_from('<IHH', data, IMAGE_OFFSET + offset)
    base, capacity, execution, table_flags = struct.unpack_from('<4I', data, 4 + 16 * (image + 2))
    require(length > 0 and length <= capacity and base >= FLASH_BASE and
            base - FLASH_BASE + capacity <= FLASH_BYTES, 'Image exceeds physical region')
    require(flags == 2 and table_flags == 0, 'Unsupported encrypted/flagged FTAB image')
    return {'header_pointer': hex(pointer), 'partition_index': image + 2,
            'length': length, 'block_bytes': block, 'flags': flags,
            'base': base, 'capacity': capacity, 'execution': execution}


def inspect(data):
    require(len(data) == FLASH_BYTES, 'Expected a complete 16 MiB NOR backup')
    require(struct.unpack_from('<I', data)[0] == 0x53454346, 'Unknown FTAB magic')
    boot, app = image_header(data, 1), image_header(data, 2)
    require(boot['partition_index'] in (3, 7) and boot['base'] == 0x12010000 and
            boot['capacity'] == 0x10000 and boot['execution'] == 0x20020000,
            'Unsupported boot storage/execution geometry')
    require(app['partition_index'] in (4, 8) and app['base'] == 0x12020000 and
            app['execution'] == app['base'] and app['capacity'] == 0x800000,
            'Unsupported current Product geometry')
    # B is inside the old 8 MiB main allocation, not presumed erased/free.
    # Refuse a probe overwrite if the declared current main crosses into B.
    require(app['length'] <= 0x400000, 'Current main overlaps candidate B slot')
    vectors = struct.unpack_from('<II', data, 0x10000)
    require(0x20040000 < vectors[0] <= 0x20050000 and vectors[0] % 8 == 0 and
            vectors[1] & 1 and 0x20020000 <= (vectors[1] & ~1) and
            (vectors[1] & ~1) + 2 <= 0x20020000 + boot['length'],
            'Stored boot vectors do not match the supported RAM boot layout')
    regions = {name: {'start': hex(FLASH_BASE + start), 'bytes': end - start,
                      'sha256': hashlib.sha256(data[start:end]).hexdigest()}
               for name, (start, end) in REGIONS.items()}
    return {'flash_sha256': hashlib.sha256(data).hexdigest(), 'boot': boot, 'main': app,
            'boot_vectors': [hex(value) for value in vectors], 'regions': regions}


def review_pair(first, second):
    first, second = Path(first).resolve(), Path(second).resolve()
    require(first != second, 'Two distinct backup paths are required')
    require(first.stat().st_size == FLASH_BYTES and second.stat().st_size == FLASH_BYTES,
            'Both backups must be exactly 16 MiB')
    a, b = first.read_bytes(), second.read_bytes()
    require(a == b, 'Backups differ; preserve both and investigate before writing')
    report = inspect(a)
    report.update(passed=True, backup_files_match=True, hardware_verified=False,
                  deployment_authorized=False, rom_rescue_verified=False,
                  limits='Offline bytes only; separate filenames do not prove separate device reads, board identity or a usable ROM rescue route.')
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--first', type=Path, required=True)
    parser.add_argument('--second', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        report = review_pair(args.first, args.second)
    except (OSError, ValueError, struct.error) as exc:
        report = {'passed': False, 'error': str(exc), 'deployment_authorized': False}
    # Never replace a backup or previous report by accident.
    with args.output.open('x', encoding='utf-8') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps(report))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
