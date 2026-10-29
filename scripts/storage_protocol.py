"""Strict metadata-only storage reply validation; never repairs UART bytes."""
import re

FIELDS = {
    "boot": ("mode raw", {"raw"}),
    "layout": ("schema bytes sector sectors guard", set()),
    "copy": ("bytes calls ms", set()),
    "sector": ("i store dirty valid live stale tail unknown pending", {"store", "dirty"}),
    "total": ("header live stale pending tail unknown", set()),
    "keys": ("known dup extra bad crc reads", {"known"}),
    "done": ("result complete coherent", set()),
}
BUCKETS = ("live", "stale", "pending", "tail", "unknown")


def parse_storage(lines):
    rows = {name: [] for name in FIELDS}
    for line in lines:
        if "[storage-" not in line:
            continue
        match = re.search(r"\[storage-(\w+)\] (.+)$", line)
        if not match or line.count("[storage-") != 1:
            raise ValueError("Malformed storage record")
        kind, body = match.groups()
        if kind not in FIELDS:
            raise ValueError("Unknown storage record")
        names, hexadecimal = FIELDS[kind]
        tokens = body.split()
        if len(tokens) != len(names.split()):
            raise ValueError("Incomplete storage fields")
        row = {}
        for name, token in zip(names.split(), tokens):
            pattern = name + (r"=([0-9a-fA-F]+)" if name in hexadecimal else r"=(\d+)")
            value = re.fullmatch(pattern, token)
            if not value:
                raise ValueError("Malformed storage field: " + name)
            row[name] = int(value[1], 16 if name in hexadecimal else 10)
            if row[name] > 0xffffffff:
                raise ValueError("Storage field exceeds uint32")
        rows[kind].append(row)
    if len(rows["done"]) == 1 and (rows["done"][0]["result"] or rows["done"][0]["complete"] != 1):
        layout = rows["layout"][0] if len(rows["layout"]) == 1 else {}
        raise ValueError(f"Storage operation failed: result={rows['done'][0]['result']} "
                         f"sector={layout.get('sector', 'unknown')} guard={layout.get('guard', 'unknown')}")
    for kind, values in rows.items():
        if len(values) != (2 if kind == "sector" else 1):
            raise ValueError("Missing or duplicate storage record: " + kind)
    result = {kind: values if kind == "sector" else values[0] for kind, values in rows.items()}
    done, total = result["done"], result["total"]
    if result["layout"] != {"schema": 2, "bytes": 16384, "sector": 8192, "sectors": 2, "guard": 0}:
        raise ValueError("Storage runtime geometry or preflight differs")
    if done["result"] or done["complete"] != 1 or done["coherent"] not in (0, 1):
        raise ValueError("Storage operation did not complete successfully")
    if result["copy"]["bytes"] != 16384 or result["copy"]["calls"] != 128:
        raise ValueError("Storage snapshot incomplete")
    sectors = result["sector"]
    if [s["i"] for s in sectors] != [0, 1]:
        raise ValueError("Storage sector ordering changed")
    for sector in sectors:
        if sector["valid"] not in (0, 1) or max(sector["store"], sector["dirty"]) > 255:
            raise ValueError("Invalid storage sector fields")
        if sum(sector[key] for key in BUCKETS) != 8176:
            raise ValueError("Storage sector accounting differs")
    if total["header"] != 32 or total["header"] + sum(total[key] for key in BUCKETS) != 16384:
        raise ValueError("Storage total accounting differs")
    if any(total[key] != sum(sector[key] for sector in sectors) for key in BUCKETS):
        raise ValueError("Storage sector totals differ")
    if result["keys"]["known"] > 0xfff:
        raise ValueError("Storage key mask exceeds schema")
    if done["coherent"] and (total["unknown"] or total["pending"] or
            any(result["keys"][key] for key in ("dup", "extra", "bad", "crc", "reads")) or
            any(not sector["valid"] or sector["store"] not in (0x7f, 0x3f, 0x1f) or
                sector["dirty"] not in (0x7f, 0x3f) for sector in sectors)):
        raise ValueError("Storage coherent flag contradicts metadata")
    return result


def simulated_storage():
    return ["[storage-boot] mode=0 raw=00000000", "[storage-layout] schema=2 bytes=16384 sector=8192 sectors=2 guard=0",
            "[storage-copy] bytes=16384 calls=128 ms=10",
            *[f"[storage-sector] i={i} store=3f dirty=7f valid=1 live=0 stale=0 tail=8176 unknown=0 pending=0" for i in range(2)],
            "[storage-total] header=32 live=0 stale=0 pending=0 tail=16352 unknown=0",
            "[storage-keys] known=000 dup=0 extra=0 bad=0 crc=0 reads=0",
            "[storage-done] result=0 complete=1 coherent=1"]
