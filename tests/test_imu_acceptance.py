import copy
import io
from pathlib import Path
import sys
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
from imu_acceptance import Observer, ProtocolError, Simulation, continuity, steps
from lab_serial import SessionChanged


class Journal:
    def event(self, *args, **kwargs):
        pass


class AcceptanceTest(unittest.TestCase):
    def client(self, peer=None):
        return Observer(peer or Simulation(), io.BytesIO(), Journal(), threading.Event())

    def lines(self):
        peer = Simulation()
        peer.write(b"wf_steps status\r\n")
        return peer.queue.decode().splitlines()

    def test_complete_snapshot(self):
        value = self.client().snapshot()
        self.assertEqual(value["steps"]["total"], 330)
        self.assertEqual(value["steps"]["hours"], {19: 50, 20: 97})
        self.assertEqual(value["light"]["enabled"], 1)

    def test_missing_and_duplicate_rows(self):
        lines = self.lines()
        for bad in [lines[:-1], lines + [lines[-1]], lines + [lines[0]]]:
            with self.subTest(bad=bad), self.assertRaises(ProtocolError):
                steps(bad)

    def test_inconsistent_fields(self):
        text = "\n".join(self.lines())
        for old, new in [("sum=147", "sum=148"), ("day=20745 utc", "day=20746 utc"),
                         ("native=17", "native=18"), ("recovered=0", "recovered=1"),
                         ("valid=0x180000 sum", "valid=0x1180000 sum")]:
            with self.subTest(new=new), self.assertRaises(ProtocolError):
                steps(text.replace(old, new).splitlines())

    def test_malformed_hour_not_silently_accepted(self):
        lines = self.lines()
        for bad in [lines + ["[steps-hour] malformed"], lines[:-1]+[lines[-1]+"[unrelated] interleaved"]]:
            with self.subTest(bad=bad), self.assertRaises(ProtocolError):
                steps(bad)

    def test_continuity_and_wrap(self):
        before = steps(self.lines())
        after = copy.deepcopy(before)
        after.update(total=333, native=20)
        after["hours"][20] += 3
        continuity(before, after)
        before["native"] = 65535
        after.update(total=332, native=1)
        after["hours"][20] = before["hours"][20]+2
        continuity(before, after)

    def test_hour_delta_and_cross_hour(self):
        before = steps(self.lines())
        after = copy.deepcopy(before)
        after.update(total=333, native=20)
        with self.assertRaises(ProtocolError):
            continuity(before, after)
        after["hours"][20] += 4
        with self.assertRaises(ProtocolError):
            continuity(before, after)
        after["hours"] = dict(before["hours"])
        after["utc"] += 3600
        continuity(before, after)  # Daily delta known, cross-hour split unknown.

    def test_loss_reset_and_date_change_rejected(self):
        before = steps(self.lines())
        for change in [dict(total=329), dict(native=0), dict(day=20746), dict(hours={20: 97})]:
            after = dict(before, **change)
            with self.subTest(change=change), self.assertRaises(ProtocolError):
                continuity(before, after)

    def test_reconnect_uptime_tracks_host_interval(self):
        before = steps(self.lines())
        after = copy.deepcopy(before)
        after["sample_ms"] += 90000
        after["utc"] += 90
        continuity(before, after, 90000)
        for change in [dict(start_ms=0), dict(sample_ms=1000),
                       dict(sample_ms=before["sample_ms"]+200000)]:
            with self.subTest(change=change), self.assertRaises(ProtocolError):
                continuity(before, dict(after, **change), 90000)

    def test_reconnect_uptime_wrap_and_sleep_cache_allowance(self):
        before = steps(self.lines())
        before["sample_ms"] = (1 << 32)-10000
        after = dict(before, sample_ms=20000)
        continuity(before, after, 30000)
        continuity(before, after, 90000)  # Up to 60s between sleeping samples.
        with self.assertRaises(ProtocolError):
            continuity(before, after, 110000)

    def test_controls_rejected_before_io(self):
        peer = Simulation()
        client = self.client(peer)
        for command in ["reboot", "wf_time 1792411300", "wf_steps save", "wf_steps retry", "wf_pm hold", "wf_mag start 1"]:
            with self.subTest(command=command), self.assertRaises(ValueError):
                client.request(command, lambda line: True)
        self.assertFalse(peer.queue)

    def test_cold_boot_rejected(self):
        peer = Simulation()
        peer.queue.extend(b"[product] storage=yes\n")
        with self.assertRaises(SessionChanged):
            self.client(peer).snapshot()


if __name__ == "__main__":
    unittest.main()
