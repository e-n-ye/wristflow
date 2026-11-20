"""Replay Windows characteristic writes into the native receiver; no Bluetooth adapter is opened."""
import asyncio
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"scripts"))
import ota_interruption_peer as ota
import ota_interruption_window as window
from unittest.mock import patch


def assemble(frames):
    size = int.from_bytes(frames[0][2:4],"little")
    assert frames[0][:2] == bytes((1, int(len(frames)>1)))
    result = bytearray(frames[0][4:])
    for i,frame in enumerate(frames[1:], 1):
        assert frame[:2] == bytes((1, 3 if i == len(frames)-1 else 2))
        result.extend(frame[2:])
    assert len(result) == size
    return bytes(result)


class Framing(unittest.TestCase):
    def test_fragments_preserve_payload_at_mtu_boundaries(self):
        for limit in (20,23,64,185,244,512):
            for size in (6,20,limit-4,limit-3,2064):
                wire = bytes(i%251 for i in range(size))
                frames = list(ota.fragments(wire,limit))
                self.assertTrue(all(len(f)<=limit for f in frames))
                self.assertEqual(assemble(frames),wire)


class PassiveWindow(unittest.IsolatedAsyncioTestCase):
    async def test_only_the_menu_handoff_starts_the_peer_and_the_serial_port_only_reads(self):
        handoff = b"msh />[ota-product] handoff_intent=receiver saved_generation=250"
        address = ("[ota-radio] receiver address="+ota.ADDRESS+" type=1").encode()
        entry = b"\r\n".join([handoff,b"SFBL",address,window.ADVERTISING])+b"\r\n"
        end = b"\r\n".join([window.START,window.NOT_READY,window.START,window.NOT_READY,window.ADVERTISING,
            window.IDLE,b"SFBL",b"[product] layout restore=yes generation=56 pages=3",
            b"[product] storage=ready restore=yes brightness=50 face=diffusion breathing=1 rtc=available",window.STABLE])+b"\r\n"
        self.assertFalse(window.entered(entry.replace(handoff,b"")))
        self.assertFalse(window.entered(entry.replace(ota.ADDRESS.encode(),b"AA:BB:CC:DD:EE:FF")))
        with self.assertRaises(ValueError):
            window.review_uart(entry+end.replace(window.NOT_READY,b"ignored",1))
        complete = asyncio.Event()
        pieces = [entry[:20],entry[20:]]
        class ReadOnly:
            def __init__(self,*args):
                self.closed = False
            def open(self):
                pass
            def read(self,n):
                if pieces:
                    return pieces.pop(0)
                return end if complete.is_set() else b""
            def close(self):
                self.closed = True
        serial = ReadOnly()
        async def fake_peer(args):
            self.assertEqual(args.authorize,ota.SCOPE)
            complete.set()
        if os.name != "nt":
            self.skipTest("Windows capture entry point")
        with tempfile.TemporaryDirectory() as folder, patch.object(window,"BoundedSerial",return_value=serial), patch.object(ota,"run",fake_peer):
            await asyncio.wait_for(window.observe(dict(root=folder,authorization=window.SCOPE)),2)
            uart = (Path(folder)/"passive/uart.bin").read_bytes()
            self.assertTrue(serial.closed)
            self.assertEqual(window.review_uart(uart)["saved_generation"],250)


@unittest.skipUnless(os.environ.get("WRISTFLOW_OTA_RELEASE_TEST"), "Requires the built host ota_release receiver and archived 06")
class PeerReplay(unittest.IsolatedAsyncioTestCase):
    async def test_raw_gatt_reassembly_and_timeout_evidence(self):
        events = []
        def record(event, **values):
            events.append(dict(event=event, monotonic_ns=round(host.now()*1e9), **values))

        class Client:
            mtu_size, is_connected = 23, True
            async def write_gatt_char(self, characteristic, frame, *, response):
                assert response and characteristic is char
                pending.append(frame)
                if frame[1] in (0,3):
                    payload = assemble(pending)
                    pending.clear()
                    ack = await host.exchange(payload,payload[0]!=37)
                    if ack is not None:
                        peer.notification(char, struct.pack("<BBH",1,0,len(ack))+ack)

        async with ota.host_receiver(Path(os.environ["WRISTFLOW_OTA_RELEASE_TEST"])) as host:
            pending = []
            char = SimpleNamespace(handle=19)
            peer = ota.GattPeer(Client(),char,asyncio.Event(),record)
            peer.now = host.now
            peer.silent = host.silent
            record("end", receiver_addresses=[ota.ADDRESS])
            record("gatt_connected",address=ota.ADDRESS,handle=19,write_limit=20)
            await ota.cases(peer,record)
            record("gatt_disconnected")
            record("gatt_closed")
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/"ble.jsonl"
            def save(rows):
                path.write_text("\n".join(json.dumps(row) for row in rows), encoding="utf-8")
            save(events)
            self.assertEqual(ota.review_gatt(path)["second_quiet_seconds"],65)
            req = [i for i,r in enumerate(events) if r["event"] == "request"]
            for pos, row in ((req[7]+1,dict(event="gatt_disconnected")),
                             (req[1]+1,dict(events[req[1]+1],hex="010006002a0002000000"))):
                altered = events[:pos]+[row]+events[pos:]
                save(altered)
                with self.assertRaises(ValueError):
                    ota.review_gatt(path)
            # Same replies, but the second probe was sent before 60 seconds.
            altered = [dict(row) for row in events]
            altered[req[-1]+1]["monotonic_ns"] -= 10_000_000_000
            save(altered)
            with self.assertRaises(ValueError):
                ota.review_gatt(path)
