"""Offline ELF/BIN checks for the fixed, non-deployable A-linked slot fixture."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess


def require(value, reason):
    if not value:
        raise ValueError(reason)


def check_entry(header, reset_pc):
    entry = re.search(r'Entry point address:\s+(0x[0-9a-fA-F]+)', header)
    require(entry and (int(entry[1], 16) & ~1) == (reset_pc & ~1), 'ELF entry bypasses Reset_Handler')


def check(elf, binary):
    data = binary.read_bytes()
    require(1024 < len(data) <= 4096, 'Fixture outside one 4 KiB erase region')
    symbols = subprocess.check_output(['arm-none-eabi-nm', '-S', str(elf)], text=True, encoding='utf-8')
    found = {}
    for name in ('Reset_Handler', 'wf_ota_slot_fault', 'wf_ota_slot_main', 'wf_ota_slot_marker', 'wf_ota_probe_tag',
                 'wf_ota_probe_uart_frame','wf_ota_probe_send'):
        match = re.search(r'(?m)^([0-9a-f]+) ([0-9a-f]+) [TtRr] ' + name + '$', symbols)
        require(match, 'Missing fixture symbol: ' + name)
        address, size = int(match[1], 16), int(match[2], 16)
        require(0x12020400 <= address < address + size <= 0x12020000 + len(data), 'Code/data outside fixture: ' + name)
        found[name] = address
    vectors = struct.unpack_from('<256I', data)
    require(vectors[0] == 0x20002000 and vectors[1] == found['Reset_Handler'] | 1, 'Wrong stack/reset vector')
    require(all(value == found['wf_ota_slot_fault'] | 1 for value in vectors[2:]), 'Unbounded exception vector')
    header = subprocess.check_output(['arm-none-eabi-readelf', '-h', str(elf)], text=True, encoding='utf-8')
    check_entry(header, vectors[1])
    marker = found['wf_ota_slot_marker'] - 0x12020000
    require(struct.unpack_from('<I', data, marker)[0] == 0x42584950, 'Wrong fixture marker')
    tag=struct.unpack_from('<4I',data,found['wf_ota_probe_tag']-0x12020000)
    require(not re.search(r'(?m)^[0-9a-f]+ [0-9a-f]+ [Tt] (?:HAL_\w+|SystemInit|entry|rt_\w+|__aeabi_\w+)$', symbols), 'Runtime or HAL dependency linked')
    sections = subprocess.check_output(['arm-none-eabi-objdump', '-h', str(elf)], text=True, encoding='utf-8')
    seen = set()
    for match in re.finditer(r'(?m)^\s*\d+ (\S+)\s+([0-9a-f]+)\s+([0-9a-f]+)\s+([0-9a-f]+)[^\n]*\n\s+([^\n]+)', sections):
        name, size, vma, lma, flags = match.groups()
        size, vma, lma = (int(value, 16) for value in (size, vma, lma))
        if not size or 'ALLOC' not in flags:
            continue
        seen.add(name)
        if name == '.stack':
            require(vma == 0x20001000 and size == 4096 and 'LOAD' not in flags, 'Stack allocation differs')
        elif name == '.result':
            require(vma == 0x20003000 and size == 256 and 'LOAD' not in flags, 'Result allocation differs')
        else:
            require('LOAD' in flags and vma == lma and 0x12020000 <= vma < vma + size <= 0x12020000 + len(data), 'Unexpected allocation: ' + name)
    require({'.vectors', '.text', '.stack', '.result'} <= seen, 'Missing required allocations')
    return {'passed': True, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'elf_sha256': hashlib.sha256(elf.read_bytes()).hexdigest(), 'linked_address': '0x12020000',
            'candidate_storage': '0x12420000', 'candidate_erase_bytes': 4096, 'symbols': found,
            'initial_sp': hex(vectors[0]), 'reset_pc': hex(vectors[1]), 'ram_result': '0x20003000',
            'tag':list(tag),'bounded_uart':True,'deployable': False, 'hardware_verified': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--bin', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = check(args.elf, args.bin)
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        result = {'passed': False, 'error': str(exc)}
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result))
    return 0 if result['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
