import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from ota_probe_package import parse_capture,pad,prepare,freeze
from test_ota_backup_review import backup

TAG=[1,2,3,4]
REVIEW={'tag':TAG,'probe':{'bytes':2048,'marker_address':0x12020500}}
def frame(kind):
    context=[0x12020480,0x20001f00,0x12020000] if kind==3 else [0x20021000,0x20041f00,0x20020000]
    words=[0x504f4657,0x10000|kind,*TAG,kind,1,*context,0,1,
           *([0,0,0] if kind==1 else [0x20000,0x420000,0x400000]),
           0 if kind==1 else 0x42584950,0x12020500,2048]
    raw=struct.pack('<19I',*words)
    return raw+struct.pack('<I',zlib.crc32(raw))
def mutate(raw,index,value):
    words=list(struct.unpack('<20I',raw));words[index]=value
    data=struct.pack('<19I',*words[:19]);return data+struct.pack('<I',zlib.crc32(data))

class ProbePackage(unittest.TestCase):
    def test_tagged_three_phase_capture(self):
        report=parse_capture(b'boot noise\r\n'+b''.join(frame(i) for i in (1,2,3)),REVIEW)
        self.assertTrue(report['passed']);self.assertFalse(report['hardware_verified'])
        self.assertEqual(report['frame_count'],3)
    def test_rejects_crc_tag_context_and_bounds(self):
        good=frame(1)+frame(2)
        corrupt=bytearray(frame(3));corrupt[40]^=1
        for raw in (bytes(corrupt),mutate(frame(3),2,5),mutate(frame(3),12,0),
                    mutate(frame(3),11,0x10000),mutate(frame(3),15,0),mutate(frame(3),8,0x12420480),
                    mutate(frame(3),9,0x20003000),mutate(frame(3),10,0x12420000),
                    mutate(frame(3),16,0),mutate(frame(3),18,2049),frame(3)[:-1]):
            with self.subTest(raw=raw.hex()[:30]),self.assertRaises(ValueError):parse_capture(good+raw,REVIEW)
        for raw in (frame(1)+frame(3),frame(2)+frame(1)+frame(3),good+frame(3)*2,b'x'*1048577):
            with self.assertRaises(ValueError):parse_capture(raw,REVIEW)
    def test_exact_compatibility_padding(self):
        data=bytes(range(16));value=pad(data,58496)
        self.assertEqual(len(value),58496);self.assertEqual(value[:16],data)
        self.assertEqual(value[16:],b'\xff'*(58496-16))
        for data,size in ((b'',58496),(b'x'*9,8)):
            with self.assertRaises(ValueError):pad(data,size)
    def test_prepare_preserves_exact_original_ranges(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'artifacts') as directory:
            root=Path(directory);data=backup();a=root/'a.bin';b=root/'b.bin'
            a.write_bytes(data);b.write_bytes(data)
            prepare(root/'out',a,b)
            self.assertEqual((root/'out/restore-boot-64k.bin').read_bytes(),data[0x10000:0x20000])
            self.assertEqual((root/'out/restore-b-first4k.bin').read_bytes(),data[0x420000:0x421000])
            plan=json.loads((root/'out/plan.json').read_text())
            self.assertFalse(plan['deployable'] or plan['device_write_authorized'])
            with self.assertRaises(ValueError):prepare(root/'out',a,b)
            self.assertEqual(a.read_bytes(),data)
    def test_c_generated_frame_matches_python_wire(self):
        producer=ROOT/'artifacts/host-ui-build/ota_probe.exe'
        if not producer.exists():self.skipTest('Build ota_probe host producer first')
        with tempfile.TemporaryDirectory(dir=ROOT/'artifacts') as directory:
            path=Path(directory)/'frame.bin'
            subprocess.run([str(producer),str(path)],check=True,capture_output=True)
            self.assertEqual(path.read_bytes(),frame(3))
    def test_foreign_sdk_build_is_rejected_before_copy(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'artifacts') as directory:
            root=Path(directory)
            record={'kind':'test','artifact_validation_passed':True,'exit_code':0,'deployable':False,
                    'sdk_changes':[],'board':json.loads((ROOT/'sdk.lock.json').read_text())['board'],
                    'sdk_commit':'other-sdk'}
            path=root/'result.json';path.write_text(json.dumps(record),encoding='utf-8')
            with mock.patch('ota_probe_package.check_sdk'),self.assertRaisesRegex(ValueError,'SDK commit'):
                freeze(path,root/'frozen','test')
            self.assertFalse((root/'frozen').exists())

if __name__=='__main__':unittest.main()
