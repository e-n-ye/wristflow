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
from ota_receiver_scan import scan, OTA_UUID_LE


class ReceiverScan(unittest.TestCase):
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
