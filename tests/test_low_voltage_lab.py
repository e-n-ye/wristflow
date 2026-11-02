from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from battery_console import Console,Cancelled
from low_voltage_lab import run,Simulation

class AutoConsole(Console):
    cancel_at=None
    def prompt(self,key,message,timeout):
        self.stage(key,'waiting')
        if key==self.cancel_at:
            self.cancel();raise Cancelled('Test cancellation')
        self.journal.event('test_operator_confirmation',phase=key)
    def automatic(self,key,message,seconds):
        self.stage(key,'complete')

class LowTests(unittest.TestCase):
    def args(self,path):return SimpleNamespace(out=path,port=None,session=123)
    def test_qualification_and_handoff_do_not_claim_discharge(self):
        with tempfile.TemporaryDirectory() as temp:
            peer=Simulation();r=run(self.args(Path(temp)/'good'),peer,AutoConsole)
            self.assertTrue(r['passed'] and r['owned'] and r['off_sampling_qualified'] and r['handed_off'])
            self.assertFalse(r['discharge_complete'] or r['device_running_verified'] or r['recovery_required'])
            self.assertEqual(peer.calls,['arm-low','status','status'])
    def test_foreign_legacy_or_failed_off_qualification_never_unplug_or_retry(self):
        for failure in ('foreign','legacy','off','keep'):
            with self.subTest(failure=failure),tempfile.TemporaryDirectory() as temp:
                class Bad(Simulation):
                    def action(self,action,*args):
                        r=super().action(action,*args)
                        if action=='status':
                            if failure=='foreign':r['status']['session']=456
                            if failure=='legacy':r['status']['low_mode']=False
                            if failure=='off':r['status']['off_samples']=0
                            if failure=='keep':r['status']['keep_remaining_ms']=0
                        return r
                peer=Bad();r=run(self.args(Path(temp)/failure),peer,AutoConsole)
                self.assertFalse(r['passed'] or r['handed_off']);self.assertTrue(r['recovery_required'])
                self.assertEqual(peer.calls.count('arm-low'),1)
                self.assertFalse(any(x in peer.calls for x in ('arm','stop','export')))
    def test_lost_arm_and_offline_cancel_no_blind_stop(self):
        with tempfile.TemporaryDirectory() as temp:
            class Lost(Simulation):
                def action(self,action,*args):
                    super().action(action,*args);return dict(passed=False,resources_closed=True,error='Lost ACK')
            peer=Lost();r=run(self.args(Path(temp)/'lost'),peer,AutoConsole)
            self.assertTrue(r['recovery_required']);self.assertEqual(peer.calls,['arm-low'])
            class Cancel(AutoConsole):cancel_at='handoff'
            peer=Simulation();r=run(self.args(Path(temp)/'cancel'),peer,Cancel)
            self.assertTrue(r['recovery_required']);self.assertFalse(r['handed_off'])
            self.assertEqual(peer.calls,['arm-low','status','status'])

if __name__=='__main__':unittest.main()
