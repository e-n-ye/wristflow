"""Check the Product charger hook's retained SRAM placement in a linked ARM ELF.

This checks the entry's actual symbol/section, not source attributes. Its DEEP
false-path instructions still require scoped disassembly review.
"""
import struct
import sys
from pathlib import Path


def check(path):
    data = Path(path).read_bytes()
    if data[:7] != b"\x7fELF\x01\x01\x01":
        raise ValueError("expected little-endian ELF32")
    if struct.unpack_from("<H", data, 18)[0] != 40:
        raise ValueError("expected ARM ELF")
    offset = struct.unpack_from("<I", data, 32)[0]
    size, count, names_index = struct.unpack_from("<HHH", data, 46)
    sections = [struct.unpack_from("<10I", data, offset + i * size) for i in range(count)]

    def contents(section):
        return data[section[4]:section[4] + section[5]]

    def string(table, start):
        return table[start:table.index(b"\0", start)].decode("ascii")

    names = contents(sections[names_index])
    matches = []
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        table = contents(sections[section[6]])
        for start in range(section[4], section[4] + section[5], section[9]):
            name, value, _, info, _, index = struct.unpack_from("<IIIBBH", data, start)
            if string(table, name) != "BSP_PowerUpCustom":
                continue
            target = sections[index]
            matches.append((value & ~1, info, string(names, target[0])))
    if len(matches) != 1:
        raise ValueError("expected exactly one BSP_PowerUpCustom definition")
    address, info, section_name = matches[0]
    if info != 0x12:  # STB_GLOBAL, STT_FUNC; do not accept the vendor weak hook
        raise ValueError("hook is not the project strong function")
    if not 0x20000000 <= address < 0x20100000 or section_name != ".retm_data":
        raise ValueError(f"hook is outside retained SRAM: 0x{address:08X} {section_name}")
    return f"charger hook: strong retained SRAM 0x{address:08X} {section_name}"


if __name__ == "__main__":
    try:
        if len(sys.argv) != 2:
            raise ValueError("usage: check_charger_layout.py main.elf")
        print(check(sys.argv[1]))
    except (ValueError, IndexError, OSError, struct.error) as error:
        print(f"charger layout check failed: {error}", file=sys.stderr)
        sys.exit(1)
