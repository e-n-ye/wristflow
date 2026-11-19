"""Independent media accounting for the provisioned board's receiver-pair change."""
import json

from ota_first_install import read_json, require

IDENTITY_ACTIONS = {"install-identity", "enter-identity", "restore-identity"}
BOOT = (0x10000, 61440)
RECEIVER = (0x820000, 364544)
KEYS = [(0, 65536), BOOT, (0x20000, 4096), RECEIVER, (0xda0000, 4096), (0xda2000, 4096)]


def blocks(start, size):
    for offset in range(start, start + size, 65536):
        yield offset, min(65536, start + size - offset)


def restore_source(prior, real, reference):
    config = read_json(prior/"media-request.json")
    require(config["action"] == "install-identity" and config["simulation"] == (not real),
            "Recovery requires the corresponding identity installation window")
    media = prior/"media"
    events = [json.loads(line) for line in (media/"media-events.jsonl").read_bytes().splitlines()]
    require(any(e.get("identity_keys_archived") for e in events) and
            any(e.get("intent") in ("receiver", "boot") for e in events),
            "No archived prewrite checkpoint and pair write intent in the earlier window")
    original = reference.read_bytes()
    for start, size in KEYS:
        for off, n in blocks(start, size):
            require((media/f"key-{off:08x}.bin").read_bytes() == original[off:off+n],
                    "Prior archived key range differs from provisioned-board reference")


def review_pair(directory, action, bundle, source):
    """Return the exact ordered reads and writes after checking the complete 500K command stream."""
    incoming = bytearray(source)
    target = bytearray(source)
    areas = {"receiver": RECEIVER, "boot": BOOT}
    for role, (off, n) in areas.items():
        payload = (bundle/(role+".bin")).read_bytes()
        require(len(payload) == n, "Unexpected receiver pair image extent")
        if action == "enter-identity":
            incoming[off:off+n] = payload
        if action != "restore-identity":
            target[off:off+n] = payload
    reads, writes = [], []
    commands = bytearray()

    def record_read(name, off, part):
        reads.append((name, off, part))
        commands.extend(f"burn_read 0x{0x12000000+off:08x} 0x{len(part):08x}\r".encode())

    for start, size in KEYS:
        for off, n in blocks(start, size):
            name = f"key-{off:08x}"
            live = (directory/(name+".bin")).read_bytes()
            owned = action == "restore-identity" and (start, size) in areas.values()
            require(len(live) == n and (owned or live == incoming[off:off+n]), "Unexpected prewrite range: "+name)
            incoming[off:off+n] = live
            record_read(name, off, live)
    if action != "enter-identity":
        order = ("boot","receiver") if action == "restore-identity" else ("receiver","boot")
        for role in order:
            for off, n in blocks(*areas[role]):
                data = bytes(target[off:off+n])
                if data == incoming[off:off+n]:
                    continue
                writes.append((role, off, data))
                commands.extend(f"burn_erase_write 0x{0x12000000+off:08x} 0x{n:08x}\r".encode())
                commands.extend(data)
                record_read(f"{role}-{off:08x}", off, data)
        for role in ("boot", "receiver"):
            for off,n in blocks(*areas[role]):
                record_read(f"pair-{off:08x}", off, bytes(target[off:off+n]))
    tx = (directory/"tx.bin").read_bytes()
    enter = b"burn_speed 500000 10\r\r\n"
    begin = tx.rindex(enter) + len(enter)
    require(tx.startswith(commands + b"burn_speed 1000000 10\r", begin), "Pair command sequence differs")
    return reads, writes
