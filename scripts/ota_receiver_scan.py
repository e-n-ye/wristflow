"""Capture named WristFlow advertising packets with Bleak's Windows backend."""
import argparse
import asyncio
import hashlib
import json
import time
from pathlib import Path
from uuid import UUID

NAMES = {"WristFlow OTA", "Bangle.js WristFlow"}
OTA_UUID_LE = bytes.fromhex("7369666c695f73640000000000000000")
SERVICE = str(UUID(bytes=OTA_UUID_LE[::-1]))
DATA = "00000000-0000-0200-6473-5f696c666973"
CCCD = "00002902-0000-1000-8000-00805f9b34fb"


def installed_address(path):
    evidence = Path(__file__).resolve().parents[1] / "docs/evidence/2026-11-19/ota-receiver-first-install.json"
    expected = json.loads(evidence.read_bytes())["capture_sha256"]["windows-air.jsonl"]
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != expected:
        raise ValueError("The receiver address must come from the verified original-board air capture")
    last = json.loads(raw.splitlines()[-1])
    address, = last["receiver_addresses"]
    return address


async def discover(device, record):
    from bleak import BleakClient

    client = BleakClient(device, pair=False, timeout=18, winrt={"use_cached_services": False})
    record("gatt_start", address=device.address, use_cached_services=False, pair=False)
    try:
        async with asyncio.timeout(24):
            await client.connect()
            services = list(client.services)
            rows = []
            for service in services:
                if service.uuid == SERVICE:
                    for char in service.characteristics:
                        rows.append(dict(uuid=char.uuid, handle=char.handle, properties=list(char.properties),
                                         descriptors=[dict(uuid=d.uuid, handle=d.handle) for d in char.descriptors]))
            record("gatt_table", services=[dict(uuid=s.uuid, handle=s.handle) for s in services],
                   serial_characteristics=rows, data_with_cccd=any(c["uuid"] == DATA and
                   any(d["uuid"] == CCCD for d in c["descriptors"]) for c in rows))
    finally:
        async with asyncio.timeout(8):
            await client.disconnect()
        record("gatt_session_closed", is_connected=client.is_connected)


def receiver_service(sections):
    for section in sections:
        if section["type"] in (6, 7):
            data = bytes.fromhex(section["hex"])
            if any(data[i:i+16] == OTA_UUID_LE for i in range(0, len(data), 16)):
                return True
    return False


async def scan(path, seconds, *, gatt_address=None, on_target=None):
    from bleak import BleakScanner

    if on_target is not None and gatt_address is None:
        raise ValueError("An attended GATT session requires the recorded receiver address")
    last_packet = {}
    seen = set()
    callbacks = 0
    counts = {name: 0 for name in NAMES}
    name_addresses = set()
    service_addresses = set()
    target = None
    target_ready = asyncio.Event()
    fresh = {}
    started = time.time()
    with path.open("x", encoding="utf-8", buffering=1) as stream:
        def record(event, **values):
            stream.write(json.dumps({"event": event, "wall_ns": time.time_ns(), **values}) + "\n")

        def received(device, advertisement):
            nonlocal callbacks, target
            callbacks += 1
            if gatt_address is not None and device.address != gatt_address:
                return
            # Windows delivers the service and the name in separate packets.
            # Limit recording to addresses with a matching advertised name.
            packets = [p for p in advertisement.platform_data[1] if p is not None]
            if not any(p.advertisement.local_name in NAMES for p in packets):
                return
            for packet in packets:
                name = packet.advertisement.local_name
                kind = int(packet.advertisement_type)
                stamp = packet.timestamp.isoformat()
                key = (device.address, kind)
                if last_packet.get(key) == stamp:
                    continue
                last_packet[key] = stamp
                sections = [{"type": int(s.data_type), "hex": bytes(s.data).hex()}
                            for s in packet.advertisement.data_sections]
                service = receiver_service(sections)
                if name not in NAMES and not service:
                    continue
                if service:
                    service_addresses.add(device.address)
                if name == "WristFlow OTA":
                    name_addresses.add(device.address)
                if name in NAMES:
                    counts[name] += 1
                # WinRT packet timestamps and the host wall clock may round a
                # new event to opposite sides of the scan start by one tick.
                if gatt_address is not None and started - 0.05 <= packet.timestamp.timestamp() <= time.time() + 0.05:
                    if name == "Bangle.js WristFlow":
                        target = None
                        fresh.clear()
                    if service:
                        fresh["service"] = packet.timestamp.timestamp()
                    if name == "WristFlow OTA" and any(s["type"] == 9 and bytes.fromhex(s["hex"]) == b"WristFlow OTA"
                                                      for s in sections):
                        fresh["name"] = packet.timestamp.timestamp()
                    if len(fresh) == 2:
                        target = device
                        target_ready.set()
                record("packet", packet_time=stamp, address=device.address, name=name,
                       kind=kind, rssi=int(packet.raw_signal_strength_in_dbm),
                       receiver_uuid=service, sections=sections)
                if name in NAMES and name not in seen:
                    seen.add(name)
                    print("Received " + name, flush=True)

        record("start", seconds=seconds)
        async with BleakScanner(detection_callback=received, scanning_mode="active"):
            if on_target is None:
                await asyncio.sleep(seconds)
            else:
                await asyncio.wait_for(target_ready.wait(), seconds)
        record("end", callbacks=callbacks, packets=counts,
               receiver_addresses=sorted(name_addresses & service_addresses))
        if gatt_address is not None:
            try:
                if target is None or time.time() - min(fresh.values()) > 5:
                    raise ValueError("No fresh OTA name and service UUID from the installed board")
                await (on_target or discover)(target, record)
            except Exception as exc:
                record("gatt_error", error=repr(exc))
                raise
    print(json.dumps(counts), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seconds", type=int, required=True)
    parser.add_argument("--gatt-from", type=Path, help="Discover attributes once on the receiver from its verified air capture")
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("--seconds must be between 1 and 300")
    address = installed_address(args.gatt_from) if args.gatt_from else None
    asyncio.run(scan(args.out, args.seconds, gatt_address=address))
