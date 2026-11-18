"""Bounded authenticated A boot and independent restoration on the original board."""
import argparse
import json
import os
import re
import struct
import sys
import zlib
from pathlib import Path
from types import SimpleNamespace

from battery_console import Console
from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from ota_first_install import ROOT, read_json, require, save, sha
from ota_first_window import STUB, app_phase, owned_endpoint, run_owned, wire_events
from ota_probe_package import MAGIC, parse_startup
from ota_rom_process import ReaderCleanupError

SCOPE = "trace-ordinary-a-68k-and-restore"
RESCUE = "restore-three-trace-ranges"
ORIGINALS = "bd6573264067ad33693ed21f2432d3607641709fadb66d8c897784a267b6ed01"
BASE = 0x12000000
# Offset, allowed write size and independent read size. Full boot and both
# journal units include the adjacent bytes that the writer must preserve.
REGIONS = {"certificate": (0x86c000,4096,4096), "journal": (0xda0000,4096,16384),
           "boot": (0x10000,61440,65536)}
PHASES = [("before","原表快照和供电"),("trace","普通 A 启动追踪"),("restore","恢复三个范围"),
          ("final","实看原表盘、触摸和 KEY1")]


def prepare(bundle, traced, ordinary):
    record=read_json(traced/"result.json")
    elf=read_json(traced/"elf-check.json")
    require(record["artifact_validation_passed"] and record["startup_trace"] and elf["passed"] and
            elf["original_a_enrollment_address"]=="0x1286c404" and len(elf["startup_trace_tag"])==4,
            "Verified original-A trace build required")
    boot=(traced/"frozen/main.bin").read_bytes()
    trusted = {
        1: ("0bfe18ab80ad7274a49506601f5a3401ced2a1b9a9eff4e1521e970f75200442", 0x31544257),
        2: ("bfd32ba5557a6e4c762b30a9bc768093cccbe583c2eb56140c17ff67c134b2e7", 0x32544257),
        3: ("c47e5232fe7d2cc6645ed19e912cb708764906ea03aa6ef8e2be5c6fa74f9e27", 0x33544257),
    }
    version = elf.get("slot_verification_trace", 1)
    require(version in trusted, "Unknown diagnostic boot")
    expected_hash, domain = trusted[version]
    tag = list(struct.unpack("<4I", bytes.fromhex(elf["receiver_sha256"])[:16]))
    tag[0] ^= domain
    require(sha(boot)==elf["bin_sha256"]==expected_hash and elf["startup_trace_tag"]==tag and
            len(boot)==elf["bin_bytes"]<=58496, "Wrong early-marker boot")
    receiver=(ordinary/"receiver.bin").read_bytes()
    require(sha(receiver[:elf["receiver_bytes"]])==elf["receiver_sha256"], "Wrong enrollment receiver")
    cert=bytearray(b"\xff"*4096)
    cert[0x404:0x554]=receiver[0x4c404:0x4c554]
    journal=(ordinary/"journal.bin").read_bytes()
    require(sha(cert)=="f1236ac70baaa7a327d6872d62e1c1aae6e2c1f810359f1ac6fe9abb05691d76" and
            sha(journal)=="0cec98cc451a579cc237715a1c259637a633b07dc9b9ad44cba901a6a3914c52","Original-A inputs differ")
    bundle.mkdir(parents=True,exist_ok=False)
    for name,data in (("certificate",cert),("journal",journal),("boot",boot.ljust(61440,b"\xff"))):
        save(bundle/(name+".bin"),data)
    write_json(bundle/"trace-check.json",elf)


MANIFEST_SHA = "25f046b0fca4bc43888d0d2f32fd061c2b0284cc1056b7f1a539130b791dd004"
BODY_SHA = "10560604d08b7951516a1b0b7914b8f6ffa22427a6a373e8f0ad1cc3f432d28d"
SHORT_SHA = "c083a4928d78f5743a30d431daf6c669b3f079a0e4033c805471789bdf8d41b0"
CHECK_PATH = "rri" + "m"*8 + "n"*8 + "kw" + "b"*8 + "c"*8 + "utq"
REJECTIONS = {
    1: "slot header read", 2: "detached enrollment read", 3: "detached manifest fields",
    4: "manifest hardware hash", 5: "manifest public key or signature", 6: "body digest comparison",
}


def verification_details(records):
    """Interpret only the ordinary A check, before any receiver activity."""
    result = {"digest": {}, "rejection": None, "checks_passed": False}
    if not records or records[0]["point"] != "S" or records[0]["detail"] != 0:
        return result
    words = {}
    for record in records[1:]:
        point, value = record["point"], record["detail"]
        if point in ("S", "V", "!"):
            break
        words.setdefault(point, []).append(value)
        if point == "x":
            result["rejection"] = REJECTIONS.get(value, "unknown release rejection")
        elif point in ("d", "g"):
            result["offset"] = value
            result["rejection"] = "body read" if point == "d" else "hardware hash"
        elif point in ("v", "z"):
            result["rejection"] = "vector preparation" if point == "v" else "stable identity comparison"
    for key, name in (("m", "manifest_hw"), ("n", "manifest_cpu"), ("b", "body_hw"), ("c", "body_cpu")):
        if len(words.get(key, [])) == 8:
            result["digest"][name] = struct.pack("<8I", *words[key]).hex()
    for key, name in (("k", "public_key_status"), ("w", "rsa_status")):
        if words.get(key):
            result[name] = struct.unpack("<i", struct.pack("<I", words[key][-1]))[0]
    result["checks_passed"] = (
        result["rejection"] is None and words.get("r") == [0x1241feb0, 0x1286c404]
        and words.get("i") == [3055760] and words.get("k") == words.get("w") == [0]
        and result["digest"] == {"manifest_hw": MANIFEST_SHA, "manifest_cpu": MANIFEST_SHA,
                                 "body_hw": BODY_SHA, "body_cpu": BODY_SHA}
        and words.get("u") == [0x20002000] and words.get("t") == [0x1202baa1] and words.get("q") == [1]
    )
    return result


def hash_qualification(records):
    """Check the short results and the full A digest independently of the device's U/C flags."""
    result = {"short": [], "full": [], "complete": False, "selected": None}
    cursor = 0

    def next_point():
        return records[cursor]["point"] if cursor < len(records) else None

    def take(point):
        nonlocal cursor
        require(next_point() == point, "Expected HASH probe frame " + point)
        value = records[cursor]["detail"]
        cursor += 1
        return value

    def digest_words(point):
        return struct.pack("<8I", *(take(point) for _ in range(8))).hex()

    try:
        mask = 0
        for method in range(7):
            require(take("K") == method, "Wrong short-vector method")
            case = {"method": method, "blocks": []}
            result["short"].append(case)
            for off in ([0] if method == 0 else [0, 256, 512]):
                block = {"offset": off}
                case["blocks"].append(block)
                if off and method != 5 and next_point() == "Y":
                    block["iv_mismatch"] = take("Y")
                    block["loaded_length"] = take("L")
                    if off == 256 and block["iv_mismatch"]:
                        block["iv_readback"] = digest_words("a")
                if next_point() == "e":
                    require(take("e") == off, "Short-vector failure offset differs")
                    break
                require(not off or method == 5 or "iv_mismatch" in block, "Missing IV observation")
                block["result_length"] = take("G")
                block["digest"] = digest_words("W")
            if case["blocks"][-1].get("digest") == SHORT_SHA:
                mask |= 1 << method
        require(take("U") == mask, "Short-vector decision differs from the observed digests")
        result["mask"] = mask
        chosen = 7
        for method in (2, 4, 6, 3, 5):
            if not mask & 1 or not mask & (1 << method):
                continue
            require(take("I") == method, "Wrong full-image method")
            attempt = {"method": method}
            result["full"].append(attempt)
            if next_point() == "f":
                attempt["failure_offset"] = take("f")
                break
            attempt["digest"] = digest_words("Z")
            if attempt["digest"] == BODY_SHA:
                chosen = method
                break
        require(take("C") == chosen and cursor == len(records), "Full-image selection differs")
        result.update(complete=True, selected=chosen)
    except ValueError as exc:
        result["error"] = str(exc)
    return result


def frames(raw, check):
    result = parse_startup(raw, check)
    records = result["frames"]
    path = "".join(record["point"] for record in records)
    details = {record["point"]: record["detail"] for record in records}
    product = b"[product] storage=ready"
    ordinary_a = False
    checked_path = "PEFNJSVOMT"
    checks_passed = True
    version = check.get("slot_verification_trace")
    if version in (2, 3):
        start = path.find("S")
        verification = verification_details(records[start:] if start >= 0 else [])
        result["slot_verification"] = verification
        checked_path = "PEFNJS" + CHECK_PATH + "VOMT"
        checks_passed = verification["checks_passed"]
    if version == 3:
        start = path.find("K")
        end = path.find("C", max(0, start))
        selection = hash_qualification(records[start:end+1] if 0 <= start <= end else
                                       records[start:] if start >= 0 else [])
        result["hash_qualification"] = selection
        checks_passed = checks_passed and selection["complete"] and selection["selected"] != 7
        if start >= 0 and end >= start:
            checked_path = "PEFNJ" + path[start:end+1] + "S" + CHECK_PATH + "VOMT"
    if path == checked_path and checks_passed and result["decode_error"] is None:
        header = struct.pack("<6I", MAGIC, 0x10001, *check["startup_trace_tag"])
        after_jump_frame = raw.rindex(header) + 80
        ordinary_a = (
            details["J"] == details["S"] == details["V"] == 0
            and details["O"] == 0x12020000
            and details["M"] == details["T"] == 0x1202baa1
            and product in raw[after_jump_frame:]
        )
    result.update(path=path, product_startup=product in raw, ordinary_a=ordinary_a)
    return result


def originals(path):
    data=path.read_bytes()
    require(sha(data)==ORIGINALS,"Same-window boot/identity/journal archive differs")
    return {"boot":data[:65536],"certificate":data[65536:69632],"journal":data[69632:]}


def review(media,bundle,source=None):
    """Replay the raw flash command stream and compare each returned block and CRC."""
    tx=(media/"tx.bin").read_bytes()
    rx=(media/"rx.bin").read_bytes()
    expected=[("read","ftab",0,65536),("read","a-vectors",0x20000,4096)]
    start={}
    for role in ("boot","certificate","journal"):
        offset,_,count=REGIONS[role]
        start[role]=(media/(role+"-before.bin")).read_bytes()
        expected.append(("read",role+"-before",offset,count))
    archived=originals(source if source else media/"originals.bin")
    if source is None:
        require(start==archived,"Flash reads differ from fresh originals")
    else:
        for role, (_, size, _) in REGIONS.items():
            require(start[role][size:] == archived[role][size:], "Protected tail differs from original " + role)
    for role in (("boot","certificate","journal") if source else ("certificate","journal","boot")):
        off,size,count=REGIONS[role]
        payload=archived[role][:size] if source else (bundle/(role+".bin")).read_bytes()
        if start[role][:size]==payload:
            continue
        expected.extend([("write",role,off,size),("read","verified-"+role,off,count)])
        require((media/("verified-"+role+".bin")).read_bytes()==payload+start[role][size:],
                "Programmed range or its protected tail differs")
    tc=rc=index=0
    active=None
    start_tx=None
    end_tx=None
    results=[]
    for e in wire_events(media):
        kind=e["kind"]
        if kind=="tx":
            require(e["offset"]==tc,"Discontinuous TX stream")
            tc+=e["bytes"]
        elif kind=="rx":
            require(e["offset"]==rc,"Discontinuous RX stream")
            rc+=e["bytes"]
        elif kind in ("read_intent","write_intent"):
            require(active is None and index<len(expected),"Unexpected media intent")
            op,name,off,n=expected[index]
            require((kind,e.get("name",e.get("role")),e["offset"],e["bytes"])==
                    (op+"_intent",name,off,n),"Unexpected flash command range")
            if start_tx is None: start_tx=tc
            elif end_tx is not None: require(tc==end_tx,"Unaccounted command between flash operations")
            active=(tc,rc)
        elif kind in ("read_verified","write_ack"):
            require(active is not None,"Unbound media acknowledgement")
            ts,rs=active
            response=rx[rs:rc]
            if op=="read":
                command=f"burn_read 0x{BASE+off:08x} 0x{n:08x}\r".encode()
                prefix,data=response.split(b"start_trans\r\n",1)
                require(kind=="read_verified" and tx[ts:tc]==command and command+b"\n" in prefix,
                        "Unexpected read command or board echo")
                payload=data[:n]
                crc=zlib.crc32(payload,0xffffffff)^0xffffffff
                require(len(payload)==n and re.match(rb"(?:\r?\n)?CRC:0x%08x\r\nOK\r\n"%crc,data[n:],re.I)
                        and payload==(media/(name+".bin")).read_bytes() and sha(payload)==e["sha256"],
                        "Independent read CRC or archive mismatch")
            else:
                payload=archived[name][:n] if source else (bundle/(name+".bin")).read_bytes()
                command=f"burn_erase_write 0x{BASE+off:08x} 0x{n:08x}\r".encode()
                require(kind=="write_ack" and tx[ts:tc]==command+payload and
                        re.search(rb"RX_WAIT.*?OK",response,re.S),"Unexpected programming command, bytes or acknowledgement")
            results.append({"op":op,"role":name,"offset":off,"bytes":n,"sha256":sha(payload)})
            end_tx=tc
            active=None
            index+=1
    require(index==len(expected) and active is None and tc==len(tx) and rc==len(rx) and
            tx[:start_tx].endswith(b"burn_speed 500000 10\r\r\n") and
            tx[end_tx:]==b"burn_speed 1000000 10\r\r\nburn_reset\r", "Incomplete or unexpected media stream")
    return results


def media_phase(action,args,root,console,bundle,exe,source=None,fixture=None):
    real=args.mode=="capture"
    out=root/action
    directory=root/("job-"+action)
    directory.mkdir()
    cfg=dict(action=action,simulation=not real,port="COM5" if real else "SIMULATED",out=str(out),
             bundle=str(bundle),before=str(source) if source else None,before_sha256=digest(source) if source else None,
             fixture=str(fixture) if not real else None,cancel=str(root/"cancel"),stub=str(STUB),seconds=85,
             listen_seconds=(30 if source is None else 12) if real else 0,
             authorization=("ordinary-a-three-range-trace" if source is None else RESCUE) if real else "simulation")
    req=root/(action+".json")
    write_json(req,cfg)
    if real: port_identity("COM5","1-1.1")
    code=run_owned([str(exe),"--config",str(req),digest(req)],directory,console,root/"cancel",90)
    events=[json.loads(line) for line in (out/"media-events.jsonl").read_bytes().splitlines()]
    valid=any(e.get("three_ranges_verified") and e.get("action")==action for e in events)
    result=dict(worker_exit=code,readback=valid,reset_ack=any(e.get("soft_reset_ack") for e in events),
                uart_closed=any(e.get("uart_closed") for e in events))
    if real and result["reset_ack"] and valid:
        result["wire"]=review(out,bundle,source)
    if real and (out/"startup-uart.bin").exists():
        raw=(out/"startup-uart.bin").read_bytes()
        result["startup"]=frames(raw,read_json(bundle/"trace-check.json"))
    write_json(root/(action+"-facts.json"),result)
    return result


def run(args):
    real=args.mode=="capture"
    scope=RESCUE if args.prior else SCOPE
    require((real and os.name=="nt" and args.authorize==scope) or
            (not real and args.authorize=="simulation" and args.fixture),"Current scope required")
    if not real and not args.prior:
        require(args.before_fixture and args.before_fixture.is_file(),
                "Simulation needs the original application's before-uart.bin and adjacent before-snapshot.json")
    build=read_json(args.build)
    verify_files([*build["sources"],build["sdk_lock"],build["executable"]],ROOT)
    exe=ROOT/build["executable"]["path"]
    root=args.out.resolve()
    journal=Journal(root,binding={"scope":scope,"simulation":not real,"build":file_record(args.build)},mode="ordinary-a")
    bundle=root/"bundle"
    prepare(bundle,args.trace_build,args.ordinary_bundle)
    report=dict(simulation=not real,restored=False,ordinary_a_verified=False)
    with Console(journal,PHASES,mode="普通 A 启动和三处恢复") as console:
        print(console.url,flush=True)
        try:
            with owned_endpoint("COM5" if real else "SIMULATED-TRACE-A",report):
                if real: port_identity("COM5","1-1.1")
                console.stage("before","running")
                if args.prior:
                    prior=args.prior.resolve()
                    cfg=read_json(prior/"trace-original-a.json")
                    require(cfg["action"]=="trace-original-a" and cfg["simulation"] is not real and
                            Path(cfg["out"]).resolve()==prior/"trace-original-a","Wrong recovery window")
                else:
                    prior=root
                    app_args=SimpleNamespace(mode=args.mode,action="ordinary-a-window",authorize=SCOPE,
                                             before_fixture=args.before_fixture)
                    app_phase(root,"before",app_args,console,worker_seconds=35)
                console.stage("before","complete")
                console.stage("trace","running")
                source=prior/"trace-original-a/originals.bin"
                fixture=args.fixture.resolve() if args.fixture else None
                if not args.prior:
                    try:
                        trace=media_phase("trace-original-a",args,root,console,bundle,exe,fixture=fixture)
                        report["trace"]=trace
                        report["ordinary_a_verified"] = bool(
                            real and trace["worker_exit"] == 0 and trace["readback"] and
                            trace["reset_ack"] and trace["uart_closed"] and trace.get("wire") and
                            trace.get("startup", {}).get("ordinary_a")
                        )
                    except ReaderCleanupError:
                        raise
                    except (ValueError,OSError) as exc:
                        if not source.is_file(): raise
                        report["trace_error"]=repr(exc)
                    # If the original Product is running again, it may have
                    # restarted charging. Preserve its short run and recheck
                    # inhibit before taking the second ROM entry.
                    first_raw=root/"trace-original-a/startup-uart.bin"
                    live_product=real and first_raw.exists() and b"[product] storage=ready" in first_raw.read_bytes()
                    if live_product or not real:
                        middle=root/"trial-app"
                        middle.mkdir()
                        if live_product and read_json(bundle/"trace-check.json").get("slot_verification_trace") == 3:
                            print("Signed trial Product started; waiting for awake display before the return snapshot.", flush=True)
                            console.prompt("trace", "请确认诊断启动的原表盘和触摸，并开启临时常亮，以便读记录和停充后恢复。",120)
                        app_phase(middle,"before",app_args,console,worker_seconds=35)
                    if not real: fixture=root/"trace-original-a/simulated-nor.bin"
                originals(source)
                console.stage("trace","complete")
                console.stage("restore","running")
                restored=media_phase("restore-trace-a",args,root,console,bundle,exe,source,fixture)
                report["restoration"]=restored
                require(restored["worker_exit"]==0 and restored["readback"] and
                        (not real or (restored["uart_closed"] and restored["reset_ack"] and restored["wire"])),
                        "Restoration is incomplete; preserve the same-window originals")
                if not real and not args.prior:
                    require((root/"restore-trace-a/simulated-nor.bin").read_bytes()==args.fixture.read_bytes(),
                            "Simulated full image changed after restoration")
                report["restored"]=True
                console.stage("restore","complete")
            if real:
                print("Three ranges restored and serial lease released; waiting for the operator's visual check.", flush=True)
                console.prompt("final","请实看原表盘、触摸及 KEY1，并确认临时常亮已关闭。",120)
            else:
                journal.event("simulated_operator",phase="final")
            console.stage("final","complete")
            report["window_complete"]=True
        except (Exception,KeyboardInterrupt) as exc:
            report.update(window_complete=False,error=repr(exc))
        write_json(root/"window.json",report)
        console.finish("passed" if report["window_complete"] else "error",report.get("error","原版已恢复，现场收尾完成。"))
    return report


if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("mode",choices=("simulate","capture"))
    p.add_argument("--authorize",required=True)
    for name in ("out","build","trace-build","ordinary-bundle","prior","fixture","before-fixture"):
        p.add_argument("--"+name,type=Path,required=name in ("out","build","trace-build","ordinary-bundle"))
    a=p.parse_args()
    if a.before_fixture: a.before_fixture=a.before_fixture.resolve()
    result=run(a)
    print(json.dumps({k:v for k,v in result.items() if k not in ("trace","restoration")}),flush=True)
    raise SystemExit(0 if result["window_complete"] else 1)
