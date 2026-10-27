"""Probe workflow boundary tests; fake device only."""
import pathlib
import sys
import threading
import time
import unittest
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/"scripts"))
import compass_probe_lab as lab


class ProbeTests(unittest.TestCase):
    def exercise(self, profile="smoke", cancel=False, peer=None):
        peer = peer or lab.Simulation()
        out = ROOT/"artifacts/compass-probe-tests"/uuid.uuid4().hex
        def ready(console):
            def operator():
                while not console.sealed:
                    with console.condition:
                        p = console.prompt_value
                        if p and not p.get("automatic") and console.answer is None:
                            console.answer = "continue"
                            console.condition.notify_all()
                        if cancel and p and p.get("automatic"):
                            console.cancelled.set()
                            console.condition.notify_all()
                    time.sleep(.005)
            threading.Thread(target=operator, daemon=True).start()
        result = lab.run(out, peer, identity_check=lambda:{"simulation":True}, binding={"simulation":True},
                         locks=out.parent/"locks", profile=profile, ready=ready, timeout=2)
        return peer, result, out

    def test_three_profiles_and_ram_freeze(self):
        for profile in ("smoke", "axes", "calibrate"):
            peer, r, out = self.exercise(profile)
            self.assertTrue(r["passed"], r["detail"])
            self.assertTrue(r["resources_closed"] and r["stop_confirmed"])
            self.assertFalse(r["hardware_verified"] or r["axes_verified"])
            self.assertEqual(sum(c.startswith(("wf_cprobe start", "wf_cprobe inputs")) for c in peer.commands), 1)
            if profile == "calibrate":
                self.assertIn("wf_cprobe freeze", peer.commands)
            self.assertTrue((out/"uart.bin").stat().st_size)

    def test_cancel_stops_owned_window_and_closes(self):
        peer, r, _ = self.exercise(cancel=True)
        self.assertEqual(r["outcome"], "cancelled")
        self.assertTrue(r["stop_confirmed"] and r["resources_closed"])
        self.assertEqual(peer.commands.count("wf_cprobe stop"), 1)

    def test_lost_start_ack_is_resolved_without_repeating(self):
        peer = lab.Simulation()
        original = peer.write
        def write(wire):
            n = original(wire)
            if wire.startswith(b"wf_cprobe start"):
                peer.queue.clear()
            return n
        peer.write = write
        _, r, _ = self.exercise(peer=peer)
        self.assertTrue(r["passed"], r["detail"])

    def test_foreign_window_is_preserved(self):
        peer = lab.Simulation()
        original = peer.write
        def write(wire):
            n = original(wire)
            if wire.startswith(b"wf_cprobe start"):
                peer.id += 1
            return n
        peer.write = write
        _, r, _ = self.exercise(peer=peer)
        self.assertFalse(r["passed"])
        self.assertNotIn("wf_cprobe stop", peer.commands)
        self.assertTrue(r["recovery_required"] and r["resources_closed"])

    def test_lost_stop_ack_never_repeats_control_or_passes(self):
        for ended in (1, 2):
            with self.subTest(end=ended):
                peer = lab.Simulation()
                original = peer.write
                def write(wire):
                    n = original(wire)
                    if wire.startswith(b"wf_cprobe stop"):
                        peer.end = ended
                        peer.queue.clear()
                    return n
                peer.write = write
                _, r, _ = self.exercise(peer=peer)
                self.assertFalse(r["passed"])
                self.assertEqual(r["outcome"], "timeout")
                self.assertTrue(r["stop_confirmed"] and r["resources_closed"])
                self.assertEqual(r["after"]["state"]["end"], ended)
                self.assertEqual(peer.commands.count("wf_cprobe stop"), 1)

    def test_complete_reports_only_and_session_consistency(self):
        p = lab.Simulation()
        self.assertEqual(len(lab.records(p.state())), 1)
        self.assertEqual(lab.records(p.state().replace("[cp-raw]", "[broken]")), [])
        with self.assertRaises(lab.ProtocolError):
            lab.records(p.state().replace("[cp-raw] id=0", "[cp-raw] id=1"))
        self.assertEqual(lab.axes("2,-1,3"), (2,-1,3))
        for invalid in ("1,1,3", "1,2,-3", "4,2,3"):
            with self.assertRaises(lab.argparse.ArgumentTypeError):
                lab.axes(invalid)

    def test_continuous_rejection_cannot_pass(self):
        peer = lab.Simulation()
        state = peer.state
        peer.state = lambda:state().replace("status=01", "status=00").replace("output=1", "output=0")
        _, r, _ = self.exercise(peer=peer)
        self.assertFalse(r["passed"])
        self.assertTrue(r["stop_confirmed"] and r["resources_closed"])

    def test_axes_rejects_unused_maps_before_opening(self):
        with self.assertRaises(lab.ProtocolError):
            lab.run(ROOT/"artifacts/unused-probe-test", lab.Simulation(), identity_check=lambda:None,
                    binding={"simulation":True}, profile="axes", acc=(2,-1,3))

    def test_delayed_worker_transitions_only_repeat_status(self):
        peer = lab.Simulation()
        original = peer.write
        pending = []
        def write(wire):
            if wire.startswith((b"wf_cprobe start ", b"wf_cprobe freeze", b"wf_cprobe stop")):
                self.assertFalse(pending)
                pending.extend((wire, 2))
                peer.queue.extend(b"[cp-command] stop=1\n" if b"stop" in wire else b"[cp-command] queued=1\n")
                return len(wire)
            if wire.startswith(b"wf_cprobe status") and pending:
                if pending[1]:
                    pending[1] -= 1
                    peer.queue.extend(peer.state().encode())
                    return len(wire)
                original(pending[0])
                pending.clear()
            return original(wire)
        peer.write = write
        _, r, _ = self.exercise(profile="calibrate", peer=peer)
        self.assertTrue(r["passed"], r["detail"])
        for c in ("wf_cprobe freeze", "wf_cprobe stop"):
            self.assertEqual(peer.commands.count(c), 1)


if __name__ == "__main__":
    unittest.main()
