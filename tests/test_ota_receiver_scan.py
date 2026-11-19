import asyncio
from datetime import datetime, timezone
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from ota_receiver_scan import scan, OTA_UUID_LE, SERVICE, DATA, CCCD


class ReceiverScan(unittest.TestCase):
    def test_discovery_uses_live_bound_receiver_and_reports_missing_attributes(self):
        board = SimpleNamespace(address="board")
        other = SimpleNamespace(address="other")
        connects = []
        config = {}
        observed = []

        class Scanner:
            def __init__(self, detection_callback, scanning_mode):
                self.callback = detection_callback

            async def __aenter__(self):
                def packet(kind, name, ad_type, data):
                    return SimpleNamespace(advertisement_type=kind, timestamp=datetime.now(timezone.utc),
                        raw_signal_strength_in_dbm=-57, advertisement=SimpleNamespace(local_name=name,
                        data_sections=[SimpleNamespace(data_type=ad_type, data=data)]))
                pair = (packet(0, "", 7, OTA_UUID_LE), packet(4, "WristFlow OTA", 9, b"WristFlow OTA"))
                for device in observed:
                    self.callback(device, SimpleNamespace(platform_data=(None, pair)))
                return self

            async def __aexit__(self, *_):
                pass

        class Client:
            def __init__(self, device, **kwargs):
                self.is_connected = False
                self.services = [SimpleNamespace(uuid=SERVICE, handle=11, characteristics=config["characteristics"])]
                connects.append((device,kwargs))

            async def connect(self):
                self.is_connected = True

            async def disconnect(self):
                self.is_connected = False

        with tempfile.TemporaryDirectory() as directory, patch.dict(sys.modules, {"bleak": SimpleNamespace(
                BleakScanner=Scanner, BleakClient=Client)}):
            path = Path(directory)
            observed.append(other)
            with self.assertRaisesRegex(ValueError, "No fresh OTA"):
                asyncio.run(scan(path/"other.jsonl", 0, gatt_address="board"))
            self.assertFalse(connects)
            observed.append(board)
            char = SimpleNamespace(uuid=DATA, handle=14, properties=["notify","write-without-response"],
                                   descriptors=[SimpleNamespace(uuid=CCCD, handle=15)])
            for present, characteristics in [(True, [char]), (False, [])]:
                config["characteristics"] = characteristics
                out = path/f"{present}.jsonl"
                asyncio.run(scan(out, 0, gatt_address="board"))
                report = [json.loads(line) for line in out.read_bytes().splitlines()]
                table, = [e for e in report if e["event"] == "gatt_table"]
                self.assertEqual(table["data_with_cccd"], present)
                self.assertEqual(report[-1]["event"], "gatt_session_closed")
                self.assertFalse(report[-1]["is_connected"])
                self.assertEqual(connects[-1], (board, dict(pair=False, timeout=18, winrt={"use_cached_services":False})))
        self.assertEqual(len(connects), 2)

    def test_separate_advertisement_and_scan_response(self):
        def packet(kind, name, ad_type, data):
            section = SimpleNamespace(data_type=ad_type, data=data)
            return SimpleNamespace(advertisement_type=kind, timestamp=datetime.now(timezone.utc),
                                   raw_signal_strength_in_dbm=-55,
                                   advertisement=SimpleNamespace(local_name=name, data_sections=[section]))

        adv = packet(0, "", 7, OTA_UUID_LE)
        response = packet(4, "WristFlow OTA", 9, b"WristFlow OTA")
        unrelated = packet(4, "unrelated", 9, b"unrelated")

        class Scanner:
            def __init__(self, detection_callback, scanning_mode):
                self.callback = detection_callback

            async def __aenter__(self):
                for address, pair in [("other", (adv, unrelated)), ("board", (adv, None)),
                                      ("board", (adv, response)), ("board", (adv, response))]:
                    self.callback(SimpleNamespace(address=address), SimpleNamespace(platform_data=(None, pair)))
                return self

            async def __aexit__(self, *_):
                pass

        with tempfile.TemporaryDirectory() as directory, patch.dict(sys.modules, {"bleak": SimpleNamespace(BleakScanner=Scanner)}):
            path = Path(directory) / "air.jsonl"
            asyncio.run(scan(path, 0))
            events = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()]
        self.assertEqual([e["event"] for e in events], ["start", "packet", "packet", "end"])
        self.assertEqual(events[1]["sections"], [{"type": 7, "hex": OTA_UUID_LE.hex()}])
        self.assertEqual(events[2]["name"], "WristFlow OTA")
        self.assertEqual(events[-1]["receiver_addresses"], ["board"])
