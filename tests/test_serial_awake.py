import io
from pathlib import Path
import sys
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from lab_serial import ProtocolError, SessionChanged
from serial_awake import AwakeClient


class Replies:
    def __init__(self, response=b"", before=b""):
        self.response = response
        self.buffer = before
        self.writes = []

    def write(self, wire):
        self.writes.append(wire)
        self.buffer = self.response
        return len(wire)

    def read(self, count):
        chunk, self.buffer = self.buffer[:count], self.buffer[count:]
        return chunk

    def event(self, *args, **kwargs):
        pass


class AwakeReceiptTest(unittest.TestCase):
    def client(self, response=b"", before=b""):
        self.io = Replies(response, before)
        return AwakeClient(self.io, io.BytesIO(), self.io, threading.Event(), timeout=0.05)

    def test_start_requires_fresh_matching_token(self):
        client = self.client(b"msh />[wf-awake] token=01020304 result=active\r\n")
        self.assertEqual(client.start("01020304", 300), "active")
        self.assertEqual(self.io.writes, [b"wf_awake start 01020304 300\r\n"])
        for receipt in (b"[wf-awake] token=01020304 result=active\n", b""):
            client = self.client(b"[wf-awake] token=01020305 result=active\n", before=receipt)
            with self.assertRaises(TimeoutError):
                client.start("01020304", 300)
            self.assertEqual(len(self.io.writes), 1)

    def test_stop_does_not_claim_another_owner(self):
        client = self.client(b"[wf-awake] token=01020304 result=other_owner\n")
        with self.assertRaisesRegex(ProtocolError, "other_owner"):
            client.stop("01020304")
        client = self.client(b"[wf-awake] token=01020304 result=inactive\n")
        self.assertEqual(client.stop("01020304"), "inactive")

    def test_bad_duration_and_boot_do_not_send_control(self):
        client = self.client()
        with self.assertRaises(ValueError):
            client.start("01020304", 1201)
        self.assertEqual(self.io.writes, [])
        client = self.client(before=b"[product] storage=ready\r\n")
        with self.assertRaises(SessionChanged):
            client.start("01020304", 300)
        self.assertEqual(self.io.writes, [])
