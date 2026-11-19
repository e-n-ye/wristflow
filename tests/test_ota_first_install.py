"""Exercise the same fixed storage transaction as the wired executable on archived NOR."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zlib
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from ota_first_install import BASE_SHA, REGIONS, prepare, sha
from lab_evidence import verify_files
from ota_first_window import review_trial_commands, review_wire


class RescueWireTests(unittest.TestCase):
    def test_trial_write_must_be_between_original_and_immediate_readback(self):
        name = "b-tail-0081f000"
        payload = b"\xff"*4096
        read = b"burn_read 0x1281f000 0x00001000\r"
        write = b"burn_erase_write 0x1281f000 0x00001000\r" + payload
        begin, end = b"burn_speed 500000 10\r\r\n", b"burn_speed 1000000 10\r"
        reads = [(n,0x81f000,payload) for n in ("before-0081f000",name,"after-0081f000")]
        writes = [("b-tail",0x81f000,payload)]
        review_trial_commands(begin+read+write+read*3+end,reads,writes,[name])
        for invalid in (read*2+write+read*2, read+write+write+read*3, read+write+read*2):
            with self.assertRaises(ValueError):
                review_trial_commands(begin+invalid+end,reads,writes,[name])


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

    @unittest.skipUnless((ROOT/"artifacts/ota-05-dma-finished-inspect-device-003/media/key-00da2000.bin").is_file(),
                         "Requires the physical 05 rollback readbacks")
    def test_health_entry_and_restart_bind_the_actual_b_extents(self):
        import ota_health_media as health
        bundle = self.directory/"health-models"
        health.prepare(bundle)
        before, after = bundle/"post-05-model.bin", bundle/"stable-06-model.bin"
        for action, model in ((health.ENTER, before), (health.INSPECT, after)):
            out = self.invoke(action, action, model, source=model, bundle=bundle)
            reads, writes = health.transfers(out, action, model.read_bytes())
            self.assertEqual((59, 3551232, []), (len(reads), sum(len(d) for _, _, d in reads), writes))
        changed = bytearray(before.read_bytes())
        changed[0x704fff] ^= 1
        fixture = self.directory/"unexpected-06-page.bin"
        fixture.write_bytes(changed)
        for label, action, model, actual in (("extra-page", health.ENTER, before, fixture),
                                             ("late-disconnect", health.ENTER, before, after),
                                             ("still-a", health.INSPECT, after, before)):
            out = self.invoke(label, action, actual, expected=False, source=model, bundle=bundle)
            events = [json.loads(line) for line in (out/"media-events.jsonl").read_text().splitlines()]
            self.assertFalse(any("intent" in event or event.get("health_keys_verified") for event in events))

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

    def test_gatt_entry_only_checks_fixed_installed_regions(self):
        from ota_first_window import discovery_reference
        installed, _ = discovery_reference()
        out = self.invoke("installed-gatt", "inspect-receiver", installed, source=installed)
        spans = [(0,65536),(0x10000,61440),(0x20000,4096),(0x820000,360448),
                 (0xda0000,4096),(0xda2000,4096)]
        nor = bytearray(installed.read_bytes())
        expected = {f"key-{off:08x}.bin": nor[off:min(off+65536,start+n)]
                    for start,n in spans for off in range(start,start+n,65536)}
        self.assertEqual({f.name:f.read_bytes() for f in out.glob("*.bin")}, expected)
        checkpoint, terminal = [json.loads(line) for line in (out/"media-events.jsonl").read_bytes().splitlines()]
        self.assertEqual(checkpoint, dict(entry_ranges_verified=True, read_bytes=499712, flash_writes=0))
        self.assertTrue(terminal["ok"])
        # Each fixed region must gate the request independently.
        for start, n in spans:
            nor[start+n-1] ^= 1
            altered = self.directory/"gatt-altered.bin"
            altered.write_bytes(nor)
            denied = self.invoke(f"gatt-denied-{start:x}", "inspect-receiver", altered, False, installed)
            self.assertNotIn(b"entry_ranges_verified", (denied/"media-events.jsonl").read_bytes())
            nor[start+n-1] ^= 1

    def test_restore_05_after_trial_or_partial_b_keeps_installed_pair_and_user_data(self):
        from ota_first_window import discovery_reference
        from ota_identity_window import BOOT, RECEIVER
        ref, _ = discovery_reference()
        baseline = bytearray(ref.read_bytes())
        bundle = self.directory/"rollback-identity"
        prepare(bundle,self.old,self.old.with_name("nor-second.bin"),identity_pair=True)
        for role,(off,n) in [("boot",BOOT),("receiver",RECEIVER)]:
            baseline[off:off+n] = (bundle/(role+".bin")).read_bytes()
        baseline[0xda8123] ^= 0x39  # A newer saved record survives rescue.
        with ZipFile(ROOT/"artifacts/WristFlow-Android-OTA-20261118.zip") as archive:
            package = archive.read("05-rollback-2026111801.bin")
        self.assertEqual(sha(package),"e0dab39f912eec9d4a39f5182181a70b4825444e2707b2495c830b135581121d")
        damaged = bytearray(baseline)
        damaged[0x420000:0x704000] = package[354:].ljust(0x2e4000,b"\xff")
        damaged[0x81f000:0x820000] = b"\xff"*(4096-336) + package[18:354]
        damaged[0xda0000:0xda4000] = b"\x55"*16384
        fixture = self.directory/"rollback-live.bin"
        for label in ("trial", "torn", "already-restored"):
            if label == "torn":
                damaged[0x420000:0x42a000] = baseline[0x420000:0x42a000]
                damaged[0xda0000:0xda2000] = baseline[0xda0000:0xda2000]
            if label == "already-restored":
                damaged = bytearray(baseline)
            fixture.write_bytes(damaged)
            out = self.invoke(label,"restore-trial-b",fixture,True,self.old,bundle)
            self.assertEqual((out/"after.bin").read_bytes(), baseline)
            events = [json.loads(row) for row in (out/"media-events.jsonl").read_bytes().splitlines()]
            roles = [event["intent"] for event in events if "intent" in event]
            if label == "trial":
                self.assertEqual(list(dict.fromkeys(roles)),["stable-a","other-record","b-body","b-tail"])
            if label == "already-restored":
                self.assertEqual(roles,[])
        for off in (0x704000, 0x878004, 0x10060):
            damaged[off] ^= 1
            fixture.write_bytes(damaged)
            out = self.invoke(f"reject-{off:x}","restore-trial-b",fixture,False,self.old,bundle)
            self.assertNotIn(b'"intent"',(out/"media-events.jsonl").read_bytes())
            damaged[off] ^= 1

    def test_installed_identity_switch_reentry_and_torn_pair_recovery(self):
        from ota_first_window import discovery_reference
        from ota_identity_window import BOOT, RECEIVER, KEYS, blocks, review_pair
        reference, _ = discovery_reference()
        bundle = self.directory/"identity-bundle"
        prepare(bundle, self.old, self.old.with_name("nor-second.bin"), identity_pair=True)
        old = bytearray(reference.read_bytes())
        old[0xda8123] ^= 23
        fixture = self.directory/"identity-fixture.bin"
        fixture.write_bytes(old)
        destination = bytearray(old)
        for role, (off,n) in [("receiver",RECEIVER),("boot",BOOT)]:
            destination[off:off+n] = (bundle/(role+".bin")).read_bytes()
        out = self.invoke("switch-identity","install-identity",fixture,True,reference,bundle)
        self.assertEqual((out/"simulated-nor.bin").read_bytes(),destination)
        self.assertFalse((out/"before.bin").exists())
        changes = [e for e in map(json.loads,(out/"media-events.jsonl").read_bytes().splitlines()) if "intent" in e]
        self.assertEqual([e["intent"] for e in changes], ["receiver"]*6+["boot"])
        # Wire audit checks the unbroken command stream including every payload.
        command = lambda off,n: f"burn_read 0x{0x12000000+off:08x} 0x{n:08x}\r".encode()
        tx = bytearray(b"burn_speed 500000 10\r\r\n")
        for start,n in KEYS:
            for off,size in blocks(start,n): tx.extend(command(off,size))
        for event in changes:
            off,n=event["offset"],event["bytes"]
            tx.extend(f"burn_erase_write 0x{0x12000000+off:08x} 0x{n:08x}\r".encode())
            tx.extend(destination[off:off+n])
            tx.extend(command(off,n))
        for start,n in (BOOT,RECEIVER):
            for off,size in blocks(start,n): tx.extend(command(off,size))
        tx.extend(b"burn_speed 1000000 10\r")
        (out/"tx.bin").write_bytes(tx)
        reads,writes = review_pair(out,"install-identity",bundle,reference.read_bytes())
        self.assertEqual((len(reads),len(writes)),(25,7))
        self.assertTrue(all((out/(name+".bin")).read_bytes()==part for name,_,part in reads))
        fixture.write_bytes(destination)
        self.invoke("checked-identity","enter-identity",fixture,True,reference,bundle)
        self.invoke("duplicate-identity-install","install-identity",fixture,False,reference,bundle)
        destination[0x10010:0x10080] = b"\xff"*112
        destination[0x878000:0x878020] = b"\x12"*32
        fixture.write_bytes(destination)
        restored = self.invoke("recover-identity","restore-identity",fixture,True,reference,bundle)
        self.assertEqual((restored/"simulated-nor.bin").read_bytes(),old)
        old[0x878123] ^= 1
        fixture.write_bytes(old)
        refused = self.invoke("nonempty-extra-page","install-identity",fixture,False,reference,bundle)
        self.assertNotIn(b'"intent"', (refused/"media-events.jsonl").read_bytes())

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
