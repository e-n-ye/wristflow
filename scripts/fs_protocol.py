"""Strict FS scan schema; observed bytes/SDK geometry never authorize formatting."""
import re

FIELDS = {
    "layout": ("schema base offset bytes end chunk budget_ms", {"base", "offset", "end"}),
    "device": ("base bytes rdid table_bytes table_match dual erase handles nor", {"base", "rdid"}),
    "pass": ("i bytes calls ff nonff crc ms", {"crc"}),
    "preview": ("pass edge data", {"data"}),
    "done": ("result complete stable guard", set()),
}


def parse_fs(lines):
    rows = {name: [] for name in FIELDS}
    order = []
    for line in lines:
        if "[fs-" not in line:
            continue
        match = re.search(r"\[fs-(\w+)\] (.+)$", line)
        if not match or line.count("[fs-") != 1 or match[1] not in FIELDS:
            raise ValueError("Malformed FS record")
        kind, body = match.groups()
        names, hexadecimal = FIELDS[kind]
        tokens = body.split()
        if len(tokens) != len(names.split()):
            raise ValueError("Incomplete FS fields")
        row = {}
        for name, token in zip(names.split(), tokens):
            pattern = r"[0-9a-fA-F]{32}" if name == "data" else r"[0-9a-fA-F]{8}" if name in hexadecimal else r"\d+"
            field = re.fullmatch(name + "=(" + pattern + ")", token)
            if not field:
                raise ValueError("Malformed FS field: " + name)
            value = field[1].lower() if name == "data" else int(field[1], 16 if name in hexadecimal else 10)
            if name != "data" and value > 0xffffffff:
                raise ValueError("FS field exceeds uint32")
            row[name] = value
        rows[kind].append(row)
        order.append(kind)
    if len(rows["done"]) == 1 and rows["done"][0] != {"result": 0, "complete": 1, "stable": 1, "guard": 0}:
        raise ValueError("FS scan failed: " + str(rows["done"][0]))
    if order != ["layout", "device", "pass", "preview", "preview", "pass", "preview", "preview", "done"]:
        raise ValueError("Missing, duplicate or reordered FS records")
    layout, device = rows["layout"][0], rows["device"][0]
    if layout != dict(schema=1, base=0x12000000, offset=0x9a0000, bytes=0x400000,
                      end=0x12da0000, chunk=1024, budget_ms=12000):
        raise ValueError("FS configured region differs")
    if (device["base"] != layout["base"] or device["bytes"] != device["table_bytes"] or
            device["bytes"] < layout["offset"] + layout["bytes"] or device["rdid"] > 0xffffff or
            device["rdid"] & 255 in (0, 255) or
            any(device[key] != 1 for key in ("table_match", "handles", "nor")) or
            device["dual"] not in (0, 1) or device["erase"] != 4096 << device["dual"]):
        raise ValueError("FS SDK metadata or guard differs")
    passes = rows["pass"]
    for i, scan in enumerate(passes):
        if (scan["i"] != i or scan["bytes"] != layout["bytes"] or scan["calls"] != 4096 or
                scan["ff"] + scan["nonff"] != scan["bytes"] or scan["ms"] >= layout["budget_ms"]):
            raise ValueError("Incomplete or late FS pass")
    if any(passes[0][key] != passes[1][key] for key in ("bytes", "calls", "ff", "nonff", "crc")):
        raise ValueError("FS pass mismatch")
    previews = rows["preview"]
    if [(p["pass"], p["edge"]) for p in previews] != [(0, 0), (0, 1), (1, 0), (1, 1)]:
        raise ValueError("FS preview ordering differs")
    if any(previews[i]["data"] != previews[i+2]["data"] for i in (0, 1)):
        raise ValueError("FS preview mismatch")
    if not passes[0]["nonff"]:
        import zlib
        if passes[0]["crc"] != zlib.crc32(b"\xff" * layout["bytes"]) or any(p["data"] != "ff" * 16 for p in previews):
            raise ValueError("FS blank accounting contradicts CRC or preview")
    return dict(layout=layout, device=device, passes=passes, previews=previews, done=rows["done"][0],
                observed_all_ff=passes[0]["nonff"] == 0, physical_capacity_verified=False,
                ownership_verified=False, format_or_erase_authorized=False)


def simulated_fs():
    import zlib
    crc = zlib.crc32(b"\xff" * 0x400000)
    return ["[fs-layout] schema=1 base=12000000 offset=009a0000 bytes=4194304 end=12da0000 chunk=1024 budget_ms=12000",
            "[fs-device] base=12000000 bytes=16777216 rdid=001840ef table_bytes=16777216 table_match=1 dual=0 erase=4096 handles=1 nor=1",
            *[line for i in range(2) for line in (
                f"[fs-pass] i={i} bytes=4194304 calls=4096 ff=4194304 nonff=0 crc={crc:08x} ms=100",
                f"[fs-preview] pass={i} edge=0 data={'ff'*16}",
                f"[fs-preview] pass={i} edge=1 data={'ff'*16}")],
            "[fs-done] result=0 complete=1 stable=1 guard=0"]
