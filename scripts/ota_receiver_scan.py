"""Capture named WristFlow advertising packets with Bleak's Windows backend."""
import argparse
import asyncio
import json
import time
from pathlib import Path

NAMES = {"WristFlow OTA", "Bangle.js WristFlow"}
OTA_UUID_LE = bytes.fromhex("7369666c695f73640000000000000000")


def receiver_service(sections):
    for section in sections:
        if section["type"] in (6, 7):
            data = bytes.fromhex(section["hex"])
            if any(data[i:i+16] == OTA_UUID_LE for i in range(0, len(data), 16)):
                return True
    return False


async def scan(path, seconds):
    from bleak import BleakScanner

    last_packet = {}
    seen = set()
    callbacks = 0
    counts = {name: 0 for name in NAMES}
    name_addresses = set()
    service_addresses = set()
    with path.open("x", encoding="utf-8", buffering=1) as stream:
        def record(event, **values):
            stream.write(json.dumps({"event": event, "wall_ns": time.time_ns(), **values}) + "\n")

        def received(device, advertisement):
            nonlocal callbacks
            callbacks += 1
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
                record("packet", packet_time=stamp, address=device.address, name=name,
                       kind=kind, rssi=int(packet.raw_signal_strength_in_dbm),
                       receiver_uuid=service, sections=sections)
                if name in NAMES and name not in seen:
                    seen.add(name)
                    print("Received " + name, flush=True)

        record("start", seconds=seconds)
        async with BleakScanner(detection_callback=received, scanning_mode="active"):
            await asyncio.sleep(seconds)
        record("end", callbacks=callbacks, packets=counts,
               receiver_addresses=sorted(name_addresses & service_addresses))
    print(json.dumps(counts), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seconds", type=int, required=True)
    args = parser.parse_args()
    if not 1 <= args.seconds <= 300:
        parser.error("--seconds must be between 1 and 300")
    asyncio.run(scan(args.out, args.seconds))
