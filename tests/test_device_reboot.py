from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"scripts"))
from device_reboot import OnceTransport, Simulation, collect, fresh
from device_reboot import baseline
from test_runtime_record_decode import capture, decoder
import json


class RebootTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parents[1]/"artifacts"); self.root=Path(self.tmp.name)
    def tearDown(self): self.tmp.cleanup()
    def run_peer(self,peer,cancel=None):
        return collect(self.root/"run",peer,binding={"simulation":True},identity_check=lambda:{},
                       locks=self.root/"locks",seconds=.03,cancel=cancel)
    def test_single_reboot_boot_and_closed(self):
        peer=Simulation(); r=self.run_peer(peer)
        self.assertTrue(r["passed"]); self.assertTrue(peer.closed)
        self.assertEqual(peer.commands,[b"reboot\r\n"])
        self.assertFalse(r["recording_control"] or r["clock_write"] or r["pm_control"])
    def test_preexisting_boot_prevents_send(self):
        peer=Simulation(); peer.queue.extend(b"RT-Thread unsolicited\r\n")
        r=self.run_peer(peer)
        self.assertFalse(r["passed"]); self.assertFalse(r["reboot_write_attempted"])
        self.assertEqual(peer.commands,[]); self.assertTrue(peer.closed)
    def test_lost_response_no_retry(self):
        peer=Simulation()
        def lost(wire): peer.commands.append(wire); return len(wire)
        peer.write=lost; r=self.run_peer(peer)
        self.assertFalse(r["passed"]); self.assertTrue(r["reboot_outcome_uncertain"])
        self.assertEqual(peer.commands,[b"reboot\r\n"]); self.assertTrue(peer.closed)
    def test_short_write_no_retry(self):
        peer=Simulation()
        def short(wire): peer.commands.append(wire); return 1
        peer.write=short; r=self.run_peer(peer)
        self.assertFalse(r["passed"]); self.assertTrue(r["reboot_outcome_uncertain"])
        self.assertEqual(len(peer.commands),1); self.assertTrue(peer.closed)
    def test_cancel_before_open(self):
        peer=Simulation(); cancel=threading.Event(); cancel.set()
        r=self.run_peer(peer,cancel)
        self.assertFalse(r["passed"]); self.assertEqual(peer.commands,[])
    def test_wrapper_refuses_other_and_second_commands(self):
        peer=Simulation(); one=OnceTransport(peer)
        with self.assertRaises(ValueError): one.write(b"wf_time 1\r\n")
        one.write(b"reboot\r\n")
        with self.assertRaises(ValueError): one.write(b"reboot\r\n")
        self.assertEqual(len(peer.commands),1)
    def test_fresh_baseline_bound(self):
        fresh(time.time_ns())
        with self.assertRaises(ValueError): fresh(time.time_ns()-121_000_000_000)
        with self.assertRaises(ValueError): fresh(time.time_ns()+1_000_000_000)
    def baseline_files(self):
        rows=capture().decode().splitlines()
        rows[0]=rows[0].replace('saved=0 failed=1','saved=1 failed=0').replace(
            f'{decoder.snapshot_crc(7,1,0,1):08x}',f'{decoder.snapshot_crc(7,1,1,0):08x}')
        saved=list(rows[1:19]); saved[0]=saved[0].replace('kind=current','kind=saved')
        raw=('\r\n'.join(rows[:19]+saved+rows[19:])+'\r\n').encode()
        (self.root/'uart.bin').write_bytes(raw)
        value=decoder.decode_capture(raw,7)
        for name,v in [('records.json',value),('export.json',dict(passed=True,resources_closed=True,simulation=False)),
                       ('binding.json',{} )]:
            (self.root/name).write_text(json.dumps(v),encoding='utf-8')
        (self.root/'events.jsonl').write_text(json.dumps(dict(kind='transport_closed',wall_ns=time.time_ns()))+'\n')
    def test_complete_baseline_and_raw_corruption(self):
        self.baseline_files(); value,_,_,files=baseline(self.root)
        self.assertEqual(value['saved']['generation'],1); self.assertEqual(len(files),5)
        raw=(self.root/'uart.bin').read_bytes(); (self.root/'uart.bin').write_bytes(raw.replace(b'hex=5746',b'hex=5747',1))
        with self.assertRaises(ValueError): baseline(self.root)
    def test_simulated_baseline_refused_for_real_reboot(self):
        self.baseline_files()
        p=self.root/'export.json'; r=json.loads(p.read_text()); r['simulation']=True;p.write_text(json.dumps(r))
        with self.assertRaises(ValueError): baseline(self.root)


if __name__=="__main__": unittest.main()
