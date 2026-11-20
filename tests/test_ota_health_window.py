"""06 acceptance must distinguish an interrupted transfer, a trial and an ordinary stable boot."""
from pathlib import Path
import sys
import struct
import tempfile
import unittest
import zlib

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"scripts"))
import ota_health_evidence as health
from lab_evidence import write_json


def phone(*extra):
    messages = ["start with 1.1.38", "connecting to "+health.ADDRESS,
                "send offline start req, file size 3033926, file count 1482",
                "receive offline start response 0, freq 1", *extra]
    return "\n".join("11-19 23:59:59.550  4983  4808 I sifli-DFU: "+msg for msg in messages)


class HealthEvidence(unittest.TestCase):
    def test_alarm_comparison_uses_the_published_data(self):
        def partition(generation, hour):
            def node(name, value):
                length = len(name)+len(value)+24
                body = bytes([len(name)])+b"\xff"*3+struct.pack("<I",len(value))+name+value
                return b"\x3f\xff\xff\xffKV00"+struct.pack("<II", length, zlib.crc32(body))+body
            alarm = bytearray(104)
            alarm[:6] = b"WFAL\x01\x01"
            struct.pack_into("<III",alarm,8,generation,2,1)
            alarm[20] = hour
            struct.pack_into("<I",alarm,100,zlib.crc32(alarm[:100]))
            marker = bytearray(104)
            marker[:5] = b"WFAC\x01"
            marker[8:16] = alarm[8:12]+alarm[100:]
            struct.pack_into("<I",marker,100,zlib.crc32(marker[:100]))
            header = b"\x3f\x7f\xff\xffFDB0"+b"\xff"*8
            used = header+node(b"preferences",b"pref")+node(b"layout_b",b"layout")+node(b"alarm_a",alarm)+node(b"alarm_ca",marker)
            empty = b"\x7f"+header[1:]
            return used.ljust(8192,b"\xff")+empty.ljust(8192,b"\xff")
        first = health.saved_settings(partition(1301,7))
        self.assertEqual(first,health.saved_settings(partition(1302,7)))
        self.assertNotEqual(first,health.saved_settings(partition(1302,8)))

    def test_without_data_ack_only_a_current_reviewed_partial_image_is_usable(self):
        # Use a fixture signature for the byte-binding test. A human inspects the
        # real screenshot before recording its displayed selection time and pro value.
        image = b"\xff\xd8\xfffixture"
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            (root/"partial").mkdir()
            raw = phone()
            (root/"partial/android-dfu.log").write_text(raw, encoding="utf-8")
            write_json(root/"binding.json", {"authorization":health.SCOPE})
            source = root/"phone.jpg"
            source.write_bytes(image)
            health.record_image(root, source, 10, "00:00:00")
            receipt = health.read_json(root/"partial-progress.json")
            self.assertEqual(receipt, health.image_receipt(raw, image, 10, "00:00:00"))
            self.assertEqual((root/"partial-progress.image").read_bytes(), image)
            for progress, stamp in ((0,"00:00:00"), (100,"00:00:00"), (True,"00:00:00"), (10,"23:59:45")):
                with self.assertRaises(ValueError):
                    health.image_receipt(raw, image, progress, stamp)
            with self.assertRaises(ValueError):
                health.image_receipt(phone("sendDfuImageEndReq"), image, 10, "00:00:00")

    def test_full_ack_and_wrong_device_cannot_prove_interruption(self):
        self.assertEqual(health.partial_phone(phone("offline packet response 0, retrans 0, completed 100"))["completed"],100)
        for invalid in (phone("offline packet response 0, retrans 0, completed 1482"),
                        phone().replace(health.ADDRESS, "AA:BB:CC:DD:EE:FF"), phone()+"\n"+phone()):
            with self.assertRaises(ValueError):
                health.partial_phone(invalid)

    def test_disconnect_can_readvertise_before_idle_return(self):
        address = ("[ota-radio] receiver address="+health.ADDRESS+" type=1").encode()
        rows = [address, health.ADVERTISING, health.START, health.ADVERTISING, health.IDLE, b"SFBL",
                health.A_TOUCH, b"[product] layout restore=yes generation=56 pages=3",
                health.PRODUCT+b"brightness=50 face=diffusion breathing=1 rtc=available"]
        self.assertEqual(health.boot_result(b"\r\n".join(rows)+b"\r\n","partial")["slot"],"A")
        rows.insert(3, health.START)
        with self.assertRaises(ValueError):
            health.boot_result(b"\r\n".join(rows)+b"\r\n","partial")

    def test_healthy_intent_without_an_independent_stable_boot_is_insufficient(self):
        identity = ("[ota-radio] receiver address="+health.ADDRESS+" type=1").encode()
        settings = [b"[product] layout restore=yes generation=56 pages=3",
                    health.PRODUCT+b"brightness=50 face=diffusion breathing=1 rtc=available"]
        trial = [identity, health.ADVERTISING, health.START, health.END, health.INSTALL, health.COMMIT,
                 b"SFBL", *settings, health.TRIAL, b"[ota-product] handoff_intent=healthy saved_generation=245"]
        stable = [b"SFBL", *settings, health.STABLE]
        def uart(lines):
            return b"\r\n".join(lines)+b"\r\n"
        self.assertEqual(health.boot_result(uart(trial+stable), "complete")["handoff_generation"], 245)
        self.assertEqual(health.boot_result(uart(stable), "ordinary")["slot"], "B")
        for lines, stage in ((trial,"complete"), (trial+stable,"partial"), (trial+stable,"ordinary"),
                             (trial+stable[:-1]+[health.TRIAL],"complete")):
            with self.assertRaises(ValueError):
                health.boot_result(uart(lines), stage)
