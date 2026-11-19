"""Exercise the same fixed storage transaction as the wired executable on archived NOR."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from ota_first_install import BASE_SHA, REGIONS, prepare, sha
from lab_evidence import verify_files
from ota_first_window import review_wire


@unittest.skipUnless(os.environ.get("WRISTFLOW_FIRST_INSTALL_BUILD"), "Requires an explicitly frozen native bridge")
class FirstInstall(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        result=json.loads(Path(os.environ["WRISTFLOW_FIRST_INSTALL_BUILD"]).read_text(encoding="utf-8"))
        assert result["kind"] == "original-board-first-install"
        verify_files([*result["sources"], result["executable"], result["sdk_lock"]], ROOT)
        cls.exe=ROOT/result["executable"]["path"]
        cls.directory=Path(tempfile.mkdtemp(prefix="first-install-model-",dir=ROOT/"artifacts"))
        cls.old=ROOT/"artifacts/ota-rom-backup-20261108/device-001/nor-first.bin"
        cls.bundle=cls.directory/"bundle"
        prepare(cls.bundle,cls.old,cls.old.with_name("nor-second.bin"))

    def invoke(self, label, action, fixture, expected=True, source=None, bundle=None):
        request=dict(action=action,simulation=True,port="SIMULATED",out=str(self.directory/label),bundle=str(bundle or self.bundle),
                     fixture=str(fixture),before=str(source) if source else None,
                     before_sha256=sha(source.read_bytes()) if source else None,
                     cancel=str(self.directory/(label+"-cancel")), seconds=60,listen_seconds=0,authorization="simulation",
                     stub=str(ROOT/"vendor/SiFli-SDK/tools/uart_download/file/ram_patch_52X.bin"))
        path=self.directory/(label+".json")
        path.write_text(json.dumps(request),encoding="utf-8")
        command=[str(self.exe),"--config",str(path),sha(path.read_bytes())]
        run=subprocess.run(command,capture_output=True,timeout=60)
        self.assertEqual(run.returncode==0,expected,run.stderr)
        return self.directory/label

    @unittest.skipUnless(os.environ.get("WRISTFLOW_TRACE_A_BUILD"), "Requires the frozen early-marker boot")
    def test_trace_only_three_ranges_and_independent_torn_restore(self):
        from ota_trace_a_window import prepare as trace_bundle, REGIONS as trace_regions
        bundle=self.directory/"trace-bundle"
        historical=self.directory/"historical-enrollment"
        prepare(historical,self.old,self.old.with_name("nor-second.bin"),historical_pair=True)
        self.invoke("superseded-fresh-install","install",self.old,False,bundle=historical)
        mixed = self.directory/"mixed-boot-receiver"
        mixed.mkdir()
        for name in ("boot.bin", "journal.bin", "receiver.bin"):
            parent = historical if name == "receiver.bin" else self.bundle
            (mixed/name).write_bytes((parent/name).read_bytes())
        self.invoke("mixed-fresh-install","install",self.old,False,bundle=mixed)
        trace_bundle(bundle,Path(os.environ["WRISTFLOW_TRACE_A_BUILD"]),historical)
        out=self.invoke("trace","trace-original-a",self.old,bundle=bundle)
        previous=self.old.read_bytes()
        expected=bytearray(previous)
        for name,(off,n,_) in trace_regions.items():
            expected[off:off+n]=(bundle/(name+".bin")).read_bytes()
        self.assertEqual(expected,(out/"simulated-nor.bin").read_bytes())
        source=out/"originals.bin"
        self.assertEqual(len(source.read_bytes()),86016)
        self.assertFalse((out/"before.bin").exists())
        # Preserve newly written user records even if the first boot programming was interrupted.
        expected[0x10020:0x10080]=b"\xff"*96
        expected[0xda8720]^=1
        expected[0x9a1800]^=1
        desired=bytearray(previous)
        desired[0xda8720]^=1
        desired[0x9a1800]^=1
        fixture=self.directory/"incomplete-trace.bin"
        fixture.write_bytes(expected)
        restored=self.invoke("restore-trace","restore-trace-a",fixture,source=source,bundle=bundle)
        self.assertEqual((restored/"simulated-nor.bin").read_bytes(),desired)
        expected[0xda2200]^=1
        fixture.write_bytes(expected)
        denied=self.invoke("trace-out-of-range","restore-trace-a",fixture,False,source,bundle)
        self.assertNotIn('"intent"',(denied/"media-events.jsonl").read_text())
        self.invoke("trace-normal-package-mix","install",self.old,False,bundle=bundle)

    def test_install_reentry_and_independent_restore(self):
        before=self.old.read_bytes()
        self.assertEqual(sha(before),BASE_SHA)
        root=self.invoke("installed","install",self.old)
        expected=bytearray(before)
        for name,off in (("receiver",0x820000),("journal",0xda0000),("boot",0x10000)):
            d=(self.bundle/(name+".bin")).read_bytes();expected[off:off+len(d)]=d
        self.assertEqual((root/"after.bin").read_bytes(),expected)
        self.assertEqual((root/"before.bin").read_bytes(),before)
        self.assertEqual((root/"original-b-4MiB.bin").read_bytes(),before[0x420000:0x820000])
        self.invoke("entry","enter-receiver",root/"after.bin")
        self.invoke("reinstall-refused","install",root/"after.bin",False)
        source=root/"before.bin"
        # A newer setting and runtime/FS bytes must survive recovery.
        expected[0x9a1700]^=0x42;expected[0xda8700]^=0x81
        new=bytearray(before);new[0x9a1700]^=0x42;new[0xda8700]^=0x81
        interrupted=self.directory/"torn.bin"
        # Torn erase/program in the boot after the receiver and journal were written.
        expected[0x10010:0x10070]=b"\xff"*96
        interrupted.write_bytes(expected)
        fixed=self.invoke("restore-torn","restore",interrupted,source=source)
        self.assertEqual((fixed/"after.bin").read_bytes(),new)
        expected[0x420002]^=0x24;expected[0x81ffff]^=0xc1
        interrupted.write_bytes(expected)
        rejected=self.invoke("b-needs-consent","restore",interrupted,False,source)
        self.assertNotIn('"intent"',(rejected/"media-events.jsonl").read_text())
        repaired=self.invoke("restore-owned-b","restore-with-b",interrupted,source=source)
        self.assertEqual((repaired/"after.bin").read_bytes(),new)
        expected[0x820000-8192]^=1;interrupted.write_bytes(expected)
        self.invoke("protected-gap","restore-with-b",interrupted,False,source)
        print("Verified complete NOR, interrupted boot, independent B recovery, preserved new settings/FS:",root,flush=True)

    def test_refuse_wrong_original_before_writes(self):
        d=bytearray(self.old.read_bytes());d[REGIONS["a"][1]-3]^=1
        p=self.directory/"changed-a.bin";p.write_bytes(d)
        out=self.invoke("wrong-a","install",p,False)
        self.assertNotIn('"intent"',(out/"media-events.jsonl").read_text())

    def test_read_only_archives_original_without_programming(self):
        out=self.invoke("qualify","verify-original",self.old)
        before=self.old.read_bytes()
        self.assertEqual((out/"before.bin").read_bytes(),before)
        self.assertEqual((out/"original-b-4MiB.bin").read_bytes(),before[0x420000:0x820000])
        events=[json.loads(line) for line in (out/"media-events.jsonl").read_text().splitlines()]
        self.assertTrue(any(e.get("checked_original_only") and e.get("flash_writes")==0 for e in events))
        self.assertFalse(any("intent" in e for e in events))
        self.assertFalse((out/"after.bin").exists())

        # A complete shell trailer can accompany a short payload, as observed
        # on the board. A same-address retry must pass its own CRC and archive.
        log=[]
        with (out/"rx.bin").open("xb") as rx, (out/"tx.bin").open("xb") as tx:
            for off in range(0,len(before),65536):
                block=before[off:off+65536]
                name=f"before-{off:08x}"
                (out/(name+".bin")).write_bytes(block)
                crc=zlib.crc32(block,0xffffffff)^0xffffffff
                for attempt in range(2 if off==65536 else 1):
                    short=off==65536 and attempt==0
                    log.append(dict(kind="read_intent",name=name,offset=off,bytes=len(block),attempt=attempt))
                    command=f"burn_read 0x{0x12000000+off:08x} 0x00010000\r".encode()
                    log.append(dict(kind="tx",offset=tx.tell(),bytes=len(command)))
                    tx.write(command)
                    payload=block[92:] if short else block
                    frame=b"start_trans\r\n"+payload+f"CRC:0x{crc:08x}\r\nOK\r\nmsh >".encode()
                    log.append(dict(kind="rx",offset=rx.tell(),bytes=len(frame)))
                    rx.write(frame)
                    if short:
                        log.append(dict(kind="read_failed",name=name,offset=off,bytes=len(block),retry=True))
                    else:
                        log.append(dict(kind="read_verified",name=name,bytes=len(block),sha256=sha(block)))
        (out/"wire-events.jsonl").write_text("".join(json.dumps(e)+"\n" for e in log),encoding="utf-8")
        review=review_wire(out,"verify-original",self.bundle,None)
        self.assertEqual(len(review["reads"]),256)
        self.assertEqual(review["read_retries"],["before-00010000"])
        self.assertEqual(review["writes"],[])
        # The same single pass is used before receiver entry, without a second
        # B backup or any programming/read-after records.
        (out/"original-b-4MiB.bin").rename(out/"original-b-archived.bin")
        self.assertEqual(review_wire(out,"enter-receiver",self.bundle,None),review)
        (out/"original-b-archived.bin").rename(out/"original-b-4MiB.bin")
        with (out/"tx.bin").open("r+b") as tx:
            tx.seek(12)
            tx.write(b"9")
        with self.assertRaisesRegex(ValueError,"Actual NOR read address"):
            review_wire(out,"verify-original",self.bundle,None)
