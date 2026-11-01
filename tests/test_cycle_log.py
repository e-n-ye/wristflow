import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from types import SimpleNamespace
import time
import zlib
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'scripts'))
from cycle_protocol import decode_file,decode_row,parse_info,parse_page,csv_text
from cycle_log_lab import collect
import cycle_log_lab as lab

def row(sequence,kind):
    raw=bytearray(64);raw[:4]=b'WFCL';raw[4:7]=bytes((1,kind,63))
    struct.pack_into('<IIQIIII',raw,8,123,sequence,sequence*5000,1793530000+sequence*5,1,sequence*5000,sequence+1)
    struct.pack_into('<HH',raw,40,4100,41000);raw[44:52]=bytes((96,97,0,0,1,0,100,100));raw[56]=6 if kind==3 else 0
    struct.pack_into('<I',raw,60,zlib.crc32(raw[:60]));return bytes(raw)

class Peer:
    port='SIMULATED-CYCLE'
    def __init__(self,failed=False):
        self.raw=b''.join(row(i,1 if i==0 else 3 if i==5 else 2) for i in range(6))
        self.queue=bytearray();self.commands=[];self.closed=False;self.failed=failed
    def open(self):pass
    def close(self):self.closed=True
    def write(self,wire):
        command=wire.decode().strip();self.commands.append(command);words=command.split()
        if words[1]=='info':
            code=7;lines=[f'[cycle-info] id=123 rows=6 bytes=384 crc={zlib.crc32(self.raw):08x} terminal=1']
        elif words[1]=='page':
            code=8;start,n=int(words[4]),int(words[5]);lines=[]
            for i in range(start,start+n):
                for p in (0,1):lines.append(f'[cycle-data] id=123 i={i} p={p} hex={self.raw[i*64+p*32:i*64+(p+1)*32].hex()}')
            if self.failed:lines[0]=lines[0][:-1]+'z'
        else:code=1;lines=[]
        self.queue.extend(('\r\n'.join([f'[cycle-accepted] cmd={code} accepted=1',*lines,f'[cycle-done] cmd={code} result=0'])+'\r\n').encode())
        return len(wire)
    def read(self,n):
        result=bytes(self.queue[:min(43,n)]);del self.queue[:len(result)];return result

class CycleTests(unittest.TestCase):
    def test_rows_crc_fields_and_order(self):
        raw=row(0,1)+row(1,3);info=dict(session=123,rows=2,bytes=128,crc=zlib.crc32(raw),terminal=True)
        rows=decode_file(raw,info);self.assertEqual(rows[0]['raw_mv'],4100);self.assertEqual(rows[1]['elapsed_ms'],5000)
        self.assertIn('filtered_decimillivolts',csv_text(rows))
        for i in range(64):
            bad=bytearray(raw[:64]);bad[i]^=1
            with self.assertRaises(ValueError):decode_row(bad)
        for broken in (raw[:-1],raw[64:]+raw[:64],raw+row(2,2)):
            with self.assertRaises(ValueError):decode_file(broken,info)
    def test_manifest_duplicates_and_page_failure(self):
        lines=['[cycle-info] id=123 rows=1 bytes=64 crc=12345678 terminal=1']
        self.assertEqual(parse_info(lines)['rows'],1)
        with self.assertRaises(ValueError):parse_info(lines+lines)
        with self.assertRaises(ValueError):parse_info([lines[0].replace('bytes=64','bytes=65')])
        with self.assertRaises(ValueError):parse_page([],123,0,1)
    def test_full_fragmented_export_and_failed_page_close_no_retry(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            for fail in (False,True):
                peer=Peer(fail)
                result=collect(root/str(fail),peer,binding={'simulation':True},identity=lambda:{},locks=root/'locks',action='export')
                self.assertEqual(result['passed'],not fail);self.assertTrue(result['resources_closed'] and peer.closed)
                self.assertEqual(peer.commands.count('wf_cycle page smoke 123 0 4'),1)
                if not fail:
                    self.assertTrue(result['record_complete']);self.assertEqual((root/str(fail)/'cycle.bin').read_bytes(),peer.raw)
                    self.assertEqual(len((root/str(fail)/'cycle.csv').read_text().splitlines()),7)
    def test_real_wrong_build_rejected_before_open(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);path=root/'build.json';path.write_text(json.dumps({'cycle_log':False}))
            with self.assertRaises(ValueError):collect(root/'out',Peer(),binding={'build':{'build_result':{'path':str(path)}}},identity=lambda:{},locks=root/'locks',action='probe')
            self.assertFalse((root/'out').exists())
    def test_explicit_session_and_actual_report_session(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            for expected,passed in ((999,False),(123,True)):
                peer=Peer();result=collect(root/str(expected),peer,binding={'simulation':True},identity=lambda:{},locks=root/'locks',action='export',session=expected)
                self.assertEqual(result['passed'],passed);self.assertTrue(result['resources_closed'])
                if passed:self.assertEqual(result['session'],123)
    def test_export_shared_deadline_and_late_final_manifest_fails(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);offset=[0.0];original=lab.command;info_count=[0];deadlines=[]
            real_now=time.monotonic
            def command(client,text,code,*,deadline=None):
                self.assertIsNotNone(deadline);deadlines.append(deadline)
                result=original(client,text,code,deadline=deadline)
                if text.startswith('info'):
                    info_count[0]+=1
                    if info_count[0]==2:offset[0]=1300.0
                return result
            with patch.object(lab,'command',side_effect=command),patch.object(lab,'time',SimpleNamespace(monotonic=lambda:real_now()+offset[0],sleep=time.sleep)):
                r=collect(root/'run',Peer(),binding={'simulation':True},identity=lambda:{},locks=root/'locks',action='export')
            self.assertFalse(r['passed']);self.assertTrue(r['resources_closed']);self.assertIn('deadline',r['error'])
            self.assertEqual(len(set(deadlines)),1)

if __name__=='__main__':unittest.main()
