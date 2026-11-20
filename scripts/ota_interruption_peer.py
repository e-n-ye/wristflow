"""Exercise DFU cancellation and a connected receive timeout using only the first two blocks of release 06."""
import argparse
import asyncio
from contextlib import asynccontextmanager
from importlib.metadata import version
import json
import os
from pathlib import Path
import struct
import time
from zipfile import ZipFile

from lab_evidence import write_json
from ota_first_install import ROOT, read_json, require, sha
from ota_health_evidence import ADDRESS
from ota_health_media import FILE
from ota_preflight import crc32_mpeg2
from ota_receiver_scan import CCCD, DATA, SERVICE, scan

SCOPE = "one-stable-b-ble-session-cancel-and-timeout-two-block-prefix"
PREFIX_SHA = "d1196dfd05379cfc2b90bba137b555e273c7c657afee9f53bddcfa57a97bfbd2"


def prefix_requests():
    release = read_json(ROOT/"docs/evidence/2026-11-18/ota-product-health.json")
    with ZipFile(ROOT/release["phone_kit"]) as kit:
        entries = [e for e in kit.infolist() if e.filename == FILE]
        require(len(entries) == 1 and entries[0].file_size == release["package_bytes"] == 3033926,
                "Expected the distributed 06 Android member")
        with kit.open(entries[0]) as stream:
            prefix = stream.read(4096)
    require(len(prefix) == 4096 and sha(prefix) == PREFIX_SHA, "Wrong 06 prefix")
    start = struct.pack("<HHIIII", 38, 16, 3033926, 1482, int.from_bytes(prefix[8:12], "little"), 1)
    requests = {"start":start, "cancel":struct.pack("<HHH", 37, 2, 0)}
    for index in (1, 2):
        data = prefix[2048*(index-1):2048*index]
        requests[f"block{index}"] = struct.pack("<HHIII",40,12+len(data),index,len(data),crc32_mpeg2(data))+data
    return requests


def fragments(payload, maximum):
    """SDK Serial category 1. A complete message or first fragment carries its little-endian length."""
    require(20 <= maximum <= 512, "Unsupported GATT write length")
    if len(payload)+4 <= maximum:
        yield struct.pack("<BBH",1,0,len(payload))+payload
        return
    offset = maximum-4
    yield struct.pack("<BBH",1,1,len(payload))+payload[:offset]
    while offset < len(payload):
        part = payload[offset:offset+maximum-2]
        offset += len(part)
        yield bytes((1,3 if offset == len(payload) else 2))+part


def response(op, status, count):
    return struct.pack("<HHHHI",op,8,status,int(op == 39),count)


async def cases(peer, record):
    req = prefix_requests()
    async def exchange(name, expected):
        record("request", name=name, bytes=len(req[name]), sha256=sha(req[name]))
        actual = await peer.exchange(req[name], expected is not None)
        record("response", name=name, hex=actual.hex() if actual is not None else None)
        require(actual == expected, "Unexpected receiver response to "+name)

    start, one = response(39,0,0), response(41,0,1)
    not_ready = response(41,4,0)
    await exchange("start", start)
    await exchange("block1", one)
    accepted_at = peer.now()
    await exchange("cancel", None)
    await exchange("block2", not_ready)
    require(peer.now()-accepted_at < 55, "Receive timeout could account for the cancellation result")
    record("cancel_verified")

    await exchange("start", start)
    await exchange("block1", one)
    first = peer.now()
    await peer.silent(45)
    await exchange("block2", response(41,0,2))
    second = peer.now()
    require(45 <= second-first < 60, "Missing positive control before the receive deadline")
    await peer.silent(65)
    await exchange("block2", not_ready)
    elapsed = peer.now()-second
    require(65 <= elapsed < 115, "Idle return could account for the timeout result")
    record("receive_timeout_verified", early_control_seconds=second-first, silent_probe_seconds=elapsed)


class GattPeer:
    now = staticmethod(time.monotonic)

    def __init__(self, client, characteristic, lost, record):
        self.client, self.characteristic, self.lost, self.record = client, characteristic, lost, record
        self.replies = asyncio.Queue(maxsize=2)
        self.failed = False
        self.maximum = min(244, client.mtu_size-3)

    def notification(self, characteristic, data):
        raw = bytes(data)
        self.record("notify", handle=characteristic.handle, hex=raw.hex())
        if (characteristic.handle != self.characteristic.handle or len(raw) != 16 or
                raw[:4] != b"\x01\x00\x0c\x00" or self.replies.full()):
            self.failed = True
        else:
            self.replies.put_nowait(raw[4:])

    def connected(self):
        require(not self.failed and not self.lost.is_set() and self.client.is_connected, "GATT session changed")

    async def exchange(self, payload, expects_reply):
        self.connected()
        require(self.replies.empty(), "Unsolicited or late notification")
        async with asyncio.timeout(18):
            for frame in fragments(payload,self.maximum):
                self.connected()
                self.record("write_intent", handle=self.characteristic.handle, hex=frame.hex())
                await self.client.write_gatt_char(self.characteristic, frame, response=True)
            self.record("gatt_writes_completed")
            reply = await self.replies.get() if expects_reply else None
        self.connected()
        return reply

    async def silent(self, seconds):
        self.connected()
        try:
            await asyncio.wait_for(self.lost.wait(), seconds)
        except TimeoutError:
            pass
        self.connected()
        require(self.replies.empty(), "Unexpected DFU traffic while waiting")


async def connected_cases(device, log):
    from bleak import BleakClient
    require(device.address == ADDRESS, "Unexpected receiver address")
    def record(event, **values):
        log(event, monotonic_ns=time.monotonic_ns(), **values)
    lost = asyncio.Event()
    closed = False
    def disconnected(_):
        lost.set()
        if not closed:
            record("gatt_disconnected")
    client = BleakClient(device, disconnected_callback=disconnected, timeout=18, pair=False,
                         winrt={"use_cached_services":False})
    try:
        async with asyncio.timeout(165):
            await client.connect()
            service = client.services.get_service(SERVICE)
            char = service.get_characteristic(DATA) if service else None
            require(char and {"notify","write"} <= set(char.properties) and
                    any(d.uuid == CCCD for d in char.descriptors), "Expected uncached SiFli Serial data characteristic")
            peer = GattPeer(client,char,lost,record)
            record("gatt_connected", address=device.address, handle=char.handle, write_limit=peer.maximum)
            await client.start_notify(char, peer.notification)
            await cases(peer,record)
            peer.connected()
            await client.stop_notify(char)
    finally:
        async with asyncio.timeout(8):
            await client.disconnect()
        require(not client.is_connected, "Bluetooth session did not close")
        closed = True
        record("gatt_closed")


def review_gatt(path):
    """Reconstruct the eight messages from recorded characteristic writes and the seven raw notifications."""
    rows = [json.loads(line) for line in path.read_bytes().splitlines()]
    connected, = [i for i,r in enumerate(rows) if r["event"] == "gatt_connected"]
    link = rows[connected]
    require(link["address"] == ADDRESS and any(r["event"] == "end" and ADDRESS in r["receiver_addresses"]
            for r in rows[:connected]), "No matching fresh advertisement before connection")
    rows = rows[connected+1:]
    require(any(r["event"] == "gatt_closed" for r in rows), "GATT cleanup missing")
    # Stop the evidence interval at completion; deliberate teardown may then report disconnection.
    verified, = [i for i,r in enumerate(rows) if r["event"] == "receive_timeout_verified"]
    active = rows[:verified]
    require(not any(r["event"] == "gatt_disconnected" for r in active), "Link lost before timeout proof")
    require(not any(r["event"] in ("request","write_intent","notify") for r in rows[verified:]), "Unexpected late DFU traffic")
    requests = prefix_requests()
    order = ("start","block1","cancel","block2","start","block1","block2","block2")
    acks = (response(39,0,0),response(41,0,1),None,response(41,4,0),
            response(39,0,0),response(41,0,1),response(41,0,2),response(41,4,0))
    starts = [i for i,r in enumerate(active) if r["event"] == "request"]
    require(len(starts) == len(order) and not any(r["event"] in ("write_intent","notify") for r in active[:starts[0]]),
            "Unexpected number of DFU requests or early traffic")
    incoming, outgoing = [], []
    for begin,end,name,ack in zip(starts,starts[1:]+[len(active)],order,acks):
        part = active[begin:end]
        frames = [r for r in part if r["event"] == "write_intent"]
        expected = list(fragments(requests[name],link["write_limit"]))
        require(part[0]["name"] == name and [bytes.fromhex(r["hex"]) for r in frames] == expected and
                all(r["handle"] == link["handle"] for r in frames) and
                sum(r["event"] == "gatt_writes_completed" for r in part) == 1, "Unexpected transmitted DFU bytes")
        notifications = [r for r in part if r["event"] == "notify"]
        require([bytes.fromhex(r["hex"]) for r in notifications] == ([] if ack is None else [b"\x01\x00\x0c\x00"+ack])
                and all(r["handle"] == link["handle"] for r in notifications), "Unexpected DFU notification")
        finished, = [r for r in part if r["event"] == "gatt_writes_completed"]
        require(finished["monotonic_ns"] >= frames[-1]["monotonic_ns"] and
                all(r["monotonic_ns"] >= frames[-1]["monotonic_ns"] for r in notifications),
                "Reply or write completion precedes the final fragment")
        outgoing.append(frames[0]["monotonic_ns"]/1e9)
        incoming.append(notifications[0]["monotonic_ns"]/1e9 if notifications else None)
    early = outgoing[6]-incoming[5]
    late = outgoing[7]-incoming[6]
    require(0 <= incoming[3]-incoming[1] < 55 and 45 <= early < 60 and 65 <= late < 115,
            "Recorded wire times do not distinguish cancellation from the receive and idle deadlines")
    return dict(requests=list(order), notifications=7, distinct_file_bytes=4096, first_quiet_seconds=early,
                second_quiet_seconds=late, cancel_result=4, timeout_result=4, completed_after_each=0)


class HostPeer:
    """Feed actual DFU bytes to the compiled production receiver with a simulated radio clock."""
    def __init__(self, process):
        self.process, self.clock = process, 0

    def now(self):
        return self.clock

    async def line(self, command):
        self.process.stdin.write(command+b"\n")
        await self.process.stdin.drain()
        reply = await asyncio.wait_for(self.process.stdout.readline(), 10)
        require(reply.endswith(b"\n"), "Host receiver closed before reply")
        return reply.strip()

    async def exchange(self, payload, expects_reply):
        raw = bytes.fromhex((await self.line(payload.hex().encode())).decode("ascii"))
        require(expects_reply or not raw, "Unexpected response to cancellation")
        return raw if expects_reply else None

    async def silent(self, seconds):
        self.clock += seconds
        if seconds > 60:
            require(await self.line(b"expire") == b"expired", "Host did not model the receive deadline")


@asynccontextmanager
async def host_receiver(exe):
    process = await asyncio.create_subprocess_exec(str(exe.resolve()), "--interruption-peer",
        stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE)
    try:
        yield HostPeer(process)
    finally:
        process.stdin.close()
        try:
            await asyncio.wait_for(process.wait(),5)
        except TimeoutError:
            process.kill()
            await process.wait()
    require(process.returncode == 0, "Compiled host receiver failed")


async def run(args):
    live = args.mode == "capture"
    require((live and os.name == "nt" and args.authorize == SCOPE and args.receiver is None) or
            (not live and args.authorize == "simulation" and args.receiver), "Wrong mode or scope")
    prefix_requests()  # Bind inputs before opening a scanner or process.
    args.out.mkdir(parents=True, exist_ok=False)
    report = dict(mode=args.mode, passed=False)
    try:
        if live:
            require(version("bleak") == "3.0.2", "Expected reviewed Windows BLE client")
            await scan(args.out/"ble.jsonl", 90, gatt_address=ADDRESS, on_target=connected_cases)
            report["wire"] = review_gatt(args.out/"ble.jsonl")
        else:
            with (args.out/"receiver.jsonl").open("x",encoding="utf-8") as stream:
                def record(event, **values):
                    print(json.dumps(dict(event=event, **values)),file=stream,flush=True)
                async with host_receiver(args.receiver) as peer:
                    await cases(peer, record)
        report["passed"] = True
    finally:
        write_json(args.out/"result.json", report)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("simulate","capture"))
    parser.add_argument("--receiver", type=Path, help="Compiled ota_release host test for simulation")
    parser.add_argument("--authorize", required=True)
    parser.add_argument("--out", type=Path, required=True)
    asyncio.run(run(parser.parse_args()))
