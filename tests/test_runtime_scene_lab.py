import argparse
import copy
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/"scripts"))
from runtime_scene_lab import Trace, TraceClient, analyze, SimulationBackend
from battery_collect import SimulatedDevice
from lab_serial import SessionChanged, ProtocolError


class Peer(SimulatedDevice):
    def __init__(self, fixture, fault=None):
        super().__init__(fixture, chunk=41, fault=fault)
        self.status.update(session=0, state=0, count=0, start=0, duration=0, now=fixture["status"]["start"])
    def write(self, wire):
        if wire.startswith(b"wf_battery start"):
            self.status["start"] = self.status["now"]
        return super().write(wire)


class SceneTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]/"artifacts")
        self.root = Path(self.tmp.name)
        fixture = json.loads((Path(__file__).parent/"fixtures/battery-b2-20261021.json").read_text(encoding="utf-8"))
        fixture["status"].update(session=1, duration=900000)
        for row in fixture["records"]:
            row["session"] = 1
        self.fixture = fixture
    def tearDown(self):
        self.tmp.cleanup()
    def trace(self, peer):
        return Trace(peer, {"simulation": True}, lambda: {}, self.root/"locks", threading.Event(), timeout=.1)
    def test_idle_arm_protect_and_owned_export(self):
        peer = Peer(self.fixture); trace = self.trace(peer)
        trace.arm(self.root/"arm")
        self.assertFalse(peer.is_open)
        self.assertEqual(json.loads((self.root/"arm/protected.json").read_text())["records"], [])
        peer.finish_offline(); value = trace.finish(self.root/"end")
        self.assertEqual(value["records"], self.fixture["records"])
        self.assertTrue(trace.stopped); self.assertFalse(peer.is_open)
        self.assertEqual(peer.commands.count("wf_battery start 900"), 1)
        self.assertFalse(any(x.startswith("wf_pm ") and x!="wf_pm status" for x in peer.commands))
        with self.assertRaises(ProtocolError):trace.arm(self.root/"arm2")
    def test_protect_nonempty_before_replacement(self):
        fixture=copy.deepcopy(self.fixture);fixture["status"]["session"]=2
        for row in fixture["records"]:row["session"]=2
        peer=Peer(fixture);peer.status=dict(fixture["status"]);trace=self.trace(peer)
        trace.arm(self.root/"arm")
        saved=json.loads((self.root/"arm/protected.json").read_text())
        self.assertEqual(saved["records"],fixture["records"])
        self.assertEqual(trace.owned["session"],3)
        self.assertLess(next(i for i,x in enumerate(peer.commands) if x.startswith("wf_battery dump")),peer.commands.index("wf_battery start 900"))
    def test_running_trace_is_preserved(self):
        peer=Peer(self.fixture);peer.status["state"]=1;trace=self.trace(peer)
        with self.assertRaises(ProtocolError):trace.arm(self.root/"arm")
        self.assertNotIn("wf_battery start 900",peer.commands);self.assertNotIn("wf_battery stop",peer.commands)
    def test_foreign_trace_never_stopped(self):
        peer=Peer(self.fixture);trace=self.trace(peer);trace.arm(self.root/"arm")
        peer.status["session"]+=1
        with self.assertRaises(SessionChanged):trace.finish(self.root/"end")
        self.assertNotIn("wf_battery stop",peer.commands);self.assertFalse(peer.is_open)
    def test_owned_running_trace_is_stopped(self):
        peer=Peer(self.fixture);trace=self.trace(peer);trace.arm(self.root/"arm")
        value=trace.finish(self.root/"end")
        self.assertTrue(trace.stopped);self.assertEqual(value["records"],[])
        self.assertEqual(peer.commands.count("wf_battery stop"),1)
    def test_readonly_allowlist(self):
        self.assertIsNone(__import__('re').fullmatch(TraceClient.COMMAND_PATTERN,"wf_record stop"))
        self.assertIsNone(__import__('re').fullmatch(TraceClient.COMMAND_PATTERN,"wf_pm hold"))
    def test_lost_start_ack_never_rearms_and_cleans_only_owned(self):
        def fault(command, reply, peer):
            return b'' if command=='wf_battery start 900' else reply
        peer=Peer(self.fixture,fault=fault);trace=self.trace(peer)
        with self.assertRaises(TimeoutError):trace.arm(self.root/"arm")
        self.assertEqual(peer.commands.count('wf_battery start 900'),1)
        self.assertEqual(peer.commands.count('wf_battery stop'),1)
        self.assertTrue(trace.stopped);self.assertFalse(peer.is_open)
    def test_lost_stop_ack_observes_without_retry(self):
        def fault(command, reply, peer):return b'' if command=='wf_battery stop' else reply
        peer=Peer(self.fixture,fault=fault);trace=self.trace(peer);trace.arm(self.root/"arm")
        trace.finish(self.root/"end")
        self.assertTrue(trace.stopped);self.assertEqual(peer.commands.count('wf_battery stop'),1)
    def test_stop_not_effective_is_not_retried(self):
        def fault(command, reply, peer):
            if command=='wf_battery stop':peer.status['state']=1;return b''
            return reply
        peer=Peer(self.fixture,fault=fault);trace=self.trace(peer);trace.arm(self.root/"arm")
        with self.assertRaises(ProtocolError):trace.finish(self.root/"end")
        self.assertFalse(trace.stopped);self.assertEqual(peer.commands.count('wf_battery stop'),1)
    def test_offline_cancel_does_not_reopen(self):
        peer=Peer(self.fixture);trace=self.trace(peer);trace.arm(self.root/"arm")
        before=list(peer.commands);trace.cancel.set()
        from battery_console import Cancelled
        with self.assertRaises(Cancelled):trace.finish(self.root/"end")
        self.assertEqual(before,peer.commands);self.assertFalse(peer.is_open);self.assertFalse(trace.stopped)
    def snapshots(self):
        b=SimulationBackend(argparse.Namespace(seconds=900));b.scene="on"
        return b.snapshot(self.root/"before"),b.snapshot(self.root/"after")
    def test_analysis_no_aux_voltage_fabrication(self):
        a,b=self.snapshots();v=analyze("on",900,a,b,{"records":[]})
        self.assertTrue(v["recorder_checks_passed"]);self.assertFalse(v["voltage_endpoints_available"])
        self.assertIsNone(v["voltage_start"]);self.assertIsNone(v["voltage_end"])
    def test_analysis_reset_or_counter_regression(self):
        a,b=self.snapshots();b["current"]["boot"]+=1
        with self.assertRaises(SessionChanged):analyze("on",900,a,b,{"records":[]})
    def test_analysis_failure_and_short_scene(self):
        a,b=self.snapshots();b["failed"]=True;b["current"]["scene_ms"]["on"]=1000
        v=analyze("on",900,a,b,{"records":[]});self.assertFalse(v["recorder_checks_passed"])
    def test_endpoint_rejects_usb_and_old_adc(self):
        a,b=self.snapshots();b["samples"]=[dict(ms=900000,sampled_ms=900000,sequence=1,flags=13,scene=1,millivolts=4200)]
        self.assertIsNone(analyze("on",900,a,b,{"records":[]})["voltage_end"])
        b["samples"][0].update(flags=5,sampled_ms=800000,charge=1)
        self.assertIsNone(analyze("on",900,a,b,{"records":[]})["voltage_end"])
    def test_off_endpoint_must_be_recent_awake_observation(self):
        a,b=self.snapshots();b["samples"]=[dict(ms=1000,sampled_ms=1000,sequence=1,flags=5,scene=1,millivolts=4000,charge=1)]
        self.assertIsNone(analyze("off",900,a,b,{"records":[]})["voltage_end"])
        b["samples"][0].update(ms=899000,sampled_ms=899000)
        self.assertIsNotNone(analyze("off",900,a,b,{"records":[]})["voltage_end"])
    def test_mixed_charging_context_not_battery_endpoint(self):
        a,b=self.snapshots();b["samples"]=[dict(ms=899000,sampled_ms=899000,sequence=1,flags=5,scene=1,millivolts=4000,charge=2)]
        self.assertIsNone(analyze('on',900,a,b,{'records':[]})['voltage_end'])
    def test_mixed_charging_context_not_battery_start(self):
        a,b=self.snapshots()
        rows=[dict(usb_valid=1,usb_good=0,raw_valid=1,seq=i+1,age=0,mode=0,
                   phase=0,surface=9,obs=i*5000,adc=i*5000,charge=2) for i in range(3)]
        self.assertIsNone(analyze('on',900,a,b,{'records':rows})['voltage_start'])
        for row in rows:row['charge']=4
        self.assertEqual(analyze('on',900,a,b,{'records':rows})['voltage_start']['seq'],2)
    def test_readonly_page_interleave_retry(self):
        damaged=[False]
        def fault(cmd,reply,peer):
            if cmd.startswith('wf_battery dump') and not damaged[0]:
                damaged[0]=True;return reply.replace(b'[bat-trace] filter=',b'[bat-trace] fi[power] mixed\n',1)
            return reply
        peer=Peer(self.fixture,fault=fault);trace=self.trace(peer);trace.arm(self.root/'arm');peer.finish_offline()
        value=trace.finish(self.root/'end');self.assertEqual(value['records'],self.fixture['records'])
        commands=[c for c in peer.commands if c.startswith('wf_battery dump')]
        self.assertEqual(commands[0],commands[1]);self.assertTrue(all(int(c.split()[-1])<=4 for c in commands))
    def test_prior_export_mismatch_blocks_overwrite(self):
        peer=Peer(self.fixture);peer.status=dict(self.fixture['status'])
        expected=copy.deepcopy(self.fixture);expected['records'][0]['mv']+=1
        trace=Trace(peer,{'simulation':True},lambda:{},self.root/'locks',threading.Event(),timeout=.1,expected=expected)
        with self.assertRaises(SessionChanged):trace.arm(self.root/'arm')
        self.assertNotIn('wf_battery start 900',peer.commands)


if __name__=="__main__":unittest.main()
