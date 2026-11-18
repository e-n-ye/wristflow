"""Attended original-board installation and recovery with a continuous endpoint lease."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import re
import sys
import time
from types import SimpleNamespace
import zlib

from battery_console import Cancelled, Console
from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import PortLease, RecordedClient, lease_directory
from ota_first_install import ROOT, BASE_SHA, read_json, require, sha
from ota_b_execute import recovery_check
from ota_rom_backup import application, Simulation
from ota_rom_device import FileCancel
from ota_rom_process import ReadProcess, ReaderCleanupError
from ota_rom_transport import BoundedSerial
from serial_lab import prepare_capture

SCOPE = {"install": "first-install-receiver-journal-boot-and-enter",
         "verify-original": "verify-original-nor-and-start-original",
         "restore": "restore-first-install-and-start-original",
         "restore-with-b": "restore-first-install-and-owned-b-extents-and-start-original"}
APP = "10560604d08b7951516a1b0b7914b8f6ffa22427a6a373e8f0ad1cc3f432d28d"
APP_EVIDENCE = ROOT / "docs/evidence/2026-11-03/battery-usable-curve.json"
STUB = ROOT / "vendor/SiFli-SDK/tools/uart_download/file/ram_patch_52X.bin"
PHASES = [("before","原程序记录与供电"),("media","有线读回与独立校验"),("return","确认原表盘返回"),
          ("after","读取返回后的记录"),("final","关闭临时常亮")]
RETAINED = []


class ChargeStop(RecordedClient):
    COMMAND_PATTERN = r"wf_charger stop"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL", b"RT-Thread")


def inhibit(client, records, replies, deadline):
    ticks = [int(m[1]) for s in replies["pm"] if (m := re.search(r"\[pm-p1\] tick=(\d+) ", s))]
    require(len(ticks) == 1, "Missing current Product tick")
    samples = records["samples"] + [records["current"]["last"]]
    fresh = [s for s in samples if s["sequence"] and s["flags"] & 1 and
             (ticks[0] - s["sampled_ms"]) % (1 << 32) < 30000]
    require(fresh and 3600 < fresh[-1]["millivolts"] <= 4350, "A recent eligible ADC observation is required")
    stop = ChargeStop(client.transport, client.raw, client.journal, client.cancel, timeout=12)
    lines = stop.request("wf_charger stop",lambda s:"[charger] entry_fault=" in s,deadline=deadline)
    text = "\n".join(lines)
    reg = re.search(r"\[charger\] REG01=([0-9a-fA-F]{2}) ",text)
    require("[charger] state=inhibited reason=manual " in text and reg and int(reg[1],16)&8 and
            re.search(r" stop=0/REG00; hold_required=0",text) and "input_valid=1 good=1 cycle_locked=1" in text,
            "USB input or the fresh charge inhibit readback is unconfirmed")
    replies["power_guard"] = dict(millivolts=fresh[-1]["millivolts"],sampled_ms=fresh[-1]["sampled_ms"],
                                  pm_tick=ticks[0],charger_stop_lines=lines)


class AppSimulation(Simulation):
    def write(self, wire):
        if wire.strip() == b"wf_charger stop":
            self.queue.extend(b"[charger] state=inhibited reason=manual busy=0\r\n[charger] REG01=0c REG05=00\r\n"
                              b"[charger] cause=0/REG00 stop=0/REG00; hold_required=0\r\n"
                              b"[charger] auto_usb=1 input_valid=1 good=1 cycle_locked=1\r\n[charger] entry_fault=00\r\n")
            return len(wire)
        if wire.strip() == b"wf_pm status":
            self.queue.extend(f"[pm-p1] tick={self.fake_tick} allow=1\r\n[pm-p1] phone=0 simulation=1\r\n".encode())
            return len(wire)
        return super().write(wire)


def app_worker(cfg):
    out, phase = Path(cfg["root"]), cfg["phase"]
    require(phase in ("before","after"), "Unknown application phase")
    journal = Journal(out / ("app-"+phase),binding=cfg,mode="original-cyclelog")
    cancel = FileCancel(out/"cancel")
    deadline = time.monotonic()+65
    def check():
        if cancel.is_set() or time.monotonic() >= deadline:
            raise Cancelled("Application window expired or cancelled")
    if cfg["simulation"]:
        fixture = Path(cfg["fixture"])
        raw = re.sub(rb"(?m)^(?:msh />)+(?=\[record-)", b"", fixture.read_bytes())
        device = AppSimulation(raw)
        state=read_json(fixture.with_name(phase+"-snapshot.json"))
        device.fake_tick=next(s["sampled_ms"]+300 for s in reversed(state["records"]["samples"]) if s["sequence"])
        identity=check
    else:
        allowed = ((cfg["action"] == "return-original" and phase == "after" and
                    cfg["authorization"] == "return-original-after-incomplete-backup") or
                   (cfg["action"] in SCOPE and cfg["authorization"]==SCOPE[cfg["action"]]) or
                   (cfg["action"]=="ordinary-a-window" and phase=="before" and
                    cfg["authorization"]=="trace-ordinary-a-68k-and-restore"))
        require(os.name=="nt" and allowed, "Explicit device window required")
        f=read_json(APP_EVIDENCE)["firmware"]
        _, identity=prepare_capture(ROOT/f["result"],ROOT/f["flash"],APP,"COM5","1-1.1",True,
                                    build_archive=ROOT/f["archive"]["path"],recorded_root=ROOT)
        device=BoundedSerial("COM5",phase,journal,check,identity)
    return application(phase,device,journal,SimpleNamespace(cancelled=cancel),identity,cycle=True,
                       before_close=inhibit if phase=="before" else None)


def run_owned(argv, directory, console, cancel, seconds):
    deadline=time.monotonic()+seconds
    child=ReadProcess(argv,directory)
    try:
        while child.poll() is None:
            if console.cancelled.is_set() or time.monotonic()>=deadline:
                cancel.touch(exist_ok=True)
                grace=time.monotonic()+3
                while child.poll() is None and time.monotonic()<grace: time.sleep(.05)
                raise Cancelled("Owned worker cancelled or timed out")
            time.sleep(.15)
        return child.poll()
    finally:
        try:
            child.close()
        except OSError as exc:
            raise ReaderCleanupError(child, "Worker exit could not be confirmed") from exc


@contextmanager
def owned_endpoint(endpoint, report):
    lease = PortLease(endpoint, lease_directory())
    lease.__enter__()
    try:
        yield
    except ReaderCleanupError as exc:
        try:
            exc.owner.close()
        except Exception:
            RETAINED.append((exc.owner, lease))
            report["lease_retained"] = True
            raise
        raise
    finally:
        if not report.get("lease_retained"):
            lease.__exit__(None, None, None)


def app_phase(root, phase, args, console, *, worker_seconds=95):
    directory=root/("job-"+phase);directory.mkdir()
    req=dict(root=str(root),phase=phase,action=args.action,simulation=args.mode=="simulate",
             authorization=args.authorize,fixture=str(getattr(args,phase+"_fixture","")))
    cfg=directory/"request.json";write_json(cfg,req)
    code=run_owned([sys.executable,"-B",str(Path(__file__).resolve()),"app-worker",str(cfg),digest(cfg)],
                   directory,console,root/"cancel",worker_seconds)
    require(code==0, "Application "+phase+" failed; inspect "+str(directory/"sftool.log"))
    return read_json(root/("app-"+phase)/(phase+"-snapshot.json"))


def review_wire(directory, action, bundle, source):
    """Independently reconstruct the authorized writes, every read and final NOR."""
    raw=(directory/"rx.bin").read_bytes()
    tx=(directory/"tx.bin").read_bytes()
    before=(directory/"before.bin").read_bytes()
    target=bytearray(before)
    spans=[]
    entries=read_json(bundle/"bundle.json")["entries"] if action!="verify-original" else []
    for entry in entries:
        start, n, role = entry["offset"], entry["bytes"], entry["role"]
        if source is None:
            payload=(bundle/(role+".bin")).read_bytes()
        else:
            if role=="journal": n=16384
            payload=source[start:start+n]
        require(len(payload)==n, "Unexpected installation or restoration source size")
        spans.append((role,start,payload))
    if source is not None:
        order={"boot":0,"receiver":1,"journal":2}
        spans.sort(key=lambda span:order[span[0]])
        if action=="restore-with-b":
            spans.extend([("b-body",0x420000,source[0x420000:0x420000+3035136]),
                          ("b-tail",0x81f000,source[0x81f000:0x820000])])
    chunks=[]
    for role,start,payload in spans:
        for i in range(0,len(payload),65536):
            offset=start+i
            part=payload[i:i+65536]
            if part != before[offset:offset+len(part)]:
                chunks.append((role,offset,part))
        target[start:start+len(payload)]=payload
    require(len(before)==0x1000000,"Incomplete whole-NOR backup")
    expected=[(f"before-{i:08x}",i,before[i:i+65536]) for i in range(0,len(before),65536)]
    if source is None:
        require((directory/"original-b-4MiB.bin").read_bytes()==before[0x420000:0x820000],"Original B archive differs")
    if action!="verify-original":
        require(target==(directory/"after.bin").read_bytes(),"Unexpected whole-NOR result")
        expected.extend((f"{role}-{off:08x}",off,part) for role,off,part in chunks)
        expected.extend((f"after-{i:08x}",i,target[i:i+65536]) for i in range(0,len(target),65536))
    cursor=0; sent=0; active=None; writing=None; results=[]; writes=[]; repeats=[]
    pending_retry=False
    for e in wire_events(directory):
        if e["kind"]=="rx":
            require(e["offset"]==cursor,"Noncontiguous raw RX");cursor+=e["bytes"]
        elif e["kind"]=="tx":
            require(e["offset"]==sent,"Noncontiguous raw TX");sent+=e["bytes"]
        elif e["kind"]=="write_intent":
            require(active is None and writing is None and len(writes)<len(chunks),"Unexpected write intent")
            role,offset,part=chunks[len(writes)]
            require((e["role"],e["offset"],e["bytes"],e["sha256"])==(role,offset,len(part),sha(part)),
                    "Wire intent differs from expected fixed chunk")
            writing=(sent,cursor)
        elif e["kind"]=="write_ack":
            require(writing is not None,"Write acknowledgement lacks an intent")
            begin,rx_begin=writing
            command=f"burn_erase_write 0x{0x12000000+offset:08x} 0x{len(part):08x}\r".encode()
            require(tx[begin:sent]==command+part and re.search(rb"RX_WAIT.*?OK",raw[rx_begin:cursor],re.S),
                    "Raw Flash command, payload or acknowledgement differs")
            writes.append(dict(role=role,offset=offset,bytes=len(part),tx_offset=begin,sha256=sha(part)))
            writing=None
        elif e["kind"]=="read_intent":
            require(active is None and writing is None and len(results)<len(expected),"Unexpected read")
            name,off,wanted=expected[len(results)]
            require((name,off,len(wanted))==(e["name"],e["offset"],e["bytes"]) and
                    e["attempt"]==int(pending_retry),"Unexpected read range or retry")
            active=(cursor,sent,e["name"],e["offset"],e["bytes"])
        elif e["kind"] in ("read_verified","read_failed"):
            require(active is not None,"Read lacks intent")
            start,sent_start,name,off,count=active;active=None
            require(name==e["name"] and count==e["bytes"],"Mismatched read receipt")
            command=f"burn_read 0x{0x12000000+off:08x} 0x{count:08x}\r".encode()
            require(tx[sent_start:sent]==command,"Actual NOR read address or size differs")
            span=raw[start:cursor];marker=b"start_trans\r\n";pos=span.index(marker)+len(marker)
            if e["kind"]=="read_failed":
                require(e["retry"] and not pending_retry and re.search(rb"CRC:0x[0-9a-fA-F]{8}\r\nOK\r\nmsh >$",span),
                        "Missing completed stub frame before read retry")
                repeats.append(name)
                pending_retry=True
                continue
            pending_retry=False
            data=span[pos:pos+count]
            m=re.match(rb"(?:\r?\n)?CRC:0x([0-9a-f]{8})\r\nOK\r\n",span[pos+count:],re.I)
            crc=zlib.crc32(data,0xffffffff)^0xffffffff
            require(m and int(m[1],16)==crc and data==wanted and data==(directory/(name+".bin")).read_bytes() and
                    sha(data)==e["sha256"],"Raw NOR transfer mismatch: "+name)
            results.append(dict(name=name,bytes=count,crc=f"{crc:08x}",rx_offset=start+pos,sha256=e["sha256"]))
    require(active is None and writing is None and not pending_retry and cursor==len(raw) and sent==len(tx) and
            len(writes)==len(chunks) and len(results)==len(expected),"Incomplete wire accounting")
    return dict(reads=results,writes=writes,read_retries=repeats)


def wire_events(directory):
    with (directory/"wire-events.jsonl").open(encoding="utf-8") as stream:
        for line in stream:
            yield json.loads(line)


def prior_source(window, real):
    c=read_json(window/"media-request.json")
    require(c["action"]=="install" and c["simulation"] is not real,"Recovery needs the matching original installation window")
    media=window/"media";p=media/"before.bin"
    entries=[json.loads(s) for s in (media/"media-events.jsonl").read_text().splitlines()]
    originals=[e for e in entries if e.get("original_verified")]
    require(len(originals)==1 and digest(p)==originals[0]["before_sha256"] and
            digest(media/"original-b-4MiB.bin")==originals[0]["b"],"Missing verified fresh original or B backup")
    return p


def window(args):
    real=args.mode=="capture"
    require(args.action in SCOPE and ((real and os.name=="nt" and args.authorize==SCOPE[args.action]) or
            (not real and args.authorize=="simulation")), "Current action-specific authorization required")
    require(args.action.startswith("restore")==bool(args.prior),"Independent recovery requires its original window")
    source=prior_source(args.prior.resolve(),real) if args.prior else None
    build=read_json(args.build)
    require(build["kind"]=="original-board-first-install" and build["device_actions"] is False,"Wrong bridge build")
    inputs=[*build["sources"],build["executable"],build["sdk_lock"]]
    verify_files(inputs,ROOT)
    exe=ROOT/build["executable"]["path"]
    bundle=args.bundle.resolve()
    pkg=read_json(bundle/"bundle.json")
    require(pkg["baseline_sha256"]==BASE_SHA and pkg["scope"]=="original-cyclelog-first-install","Wrong installation bundle")
    verify_files(pkg["entries"])
    root=args.out.resolve()
    journal=Journal(root,binding=dict(action=args.action,simulation=not real,build=file_record(args.build),
                    bundle=file_record(bundle/"bundle.json"),operator_scope=args.authorize),mode="first-install-window")
    def operator(console, phase, message):
        if real: console.prompt(phase,message,180)
        else: journal.event("simulated_operator",phase=phase,message=message)
        console.stage(phase,"complete")
    report=dict(simulation=not real,action=args.action,flash_verified=False,wire_verified=False,
                receiver_advertising=False,idle_return=False,application_return=False,hardware_verified=False)
    with Console(journal,PHASES,mode="黄山派有线首装与恢复" if real else "首装离线演练") as console:
        print(console.url,flush=True)
        try:
            with owned_endpoint("COM5" if real else "SIMULATED-FIRST-INSTALL",report):
                if real: port_identity("COM5","1-1.1")
                console.stage("before","running")
                if source is None:
                    before=app_phase(root,"before",args,console)
                else:
                    before=read_json(args.prior/"app-before/before-snapshot.json")
                    journal.event("recovery_baseline",source=str(source))
                console.stage("before","complete")
                cfg=dict(action=args.action,simulation=not real,port="COM5" if real else "SIMULATED",out=str(root/"media"),
                         bundle=str(bundle),before=str(source) if source else None,before_sha256=digest(source) if source else None,
                         fixture=None if real else str(args.nor_fixture.resolve()),cancel=str(root/"cancel"),stub=str(STUB),
                         # On COM5 a 64 KiB read with its own framing takes about
                         # 2.52 seconds. Allow both 256-block passes and the idle return.
                         seconds=800 if args.action=="verify-original" else 1750,
                         listen_seconds=(155 if args.action=="install" else 16) if real else 0,authorization=args.authorize)
                path=root/"media-request.json";write_json(path,cfg)
                job=root/"job-media";job.mkdir()
                console.stage("media","running")
                verify_files(inputs,ROOT)
                if real: port_identity("COM5","1-1.1")
                code=run_owned([str(exe),"--config",str(path),digest(path)],job,console,root/"cancel",cfg["seconds"]+20)
                media=root/"media"
                events=[json.loads(s) for s in (media/"media-events.jsonl").read_text().splitlines()]
                fact="checked_original_only" if args.action=="verify-original" else "entire_nor_verified"
                report["flash_verified"]=any(e.get(fact) for e in events)
                report["reset_ack"]=any(e.get("soft_reset_ack") for e in events)
                if real and report["flash_verified"]:
                    try:
                        review=review_wire(media,args.action,bundle,source.read_bytes() if source else None)
                        write_json(root/"wire-review.json",review)
                        report["wire_verified"]=True
                    except Exception as exc:
                        report["wire_review_error"]=repr(exc)
                report["worker_exit"]=code
                require(report["flash_verified"] and (report["reset_ack"] if real else code==0),
                        "Transport stopped. Keep exact media state and consult the independent recovery window.")
                if real:
                    log=(media/"startup-uart.bin").read_bytes()
                    ad=log.find(b"[ota-radio] advertising=0 name=WristFlow OTA")
                    idle=log.find(b"[ota-radio] inactive, returning to boot selection")
                    report.update(receiver_advertising=ad>=0,idle_return=ad>=0 and idle>ad)
                console.stage("media","complete")
                operator(console,"return","请确认原 CycleLog 表盘、显示和触摸已返回，打开临时常亮读取后态。")
                console.stage("after","running")
                after=app_phase(root,"after",args,console)
                report["continuity"]=recovery_check(before,after,(after["closed_ns"]-before["closed_ns"])/1e9,
                                                     args.action.startswith("restore"))
                report["application_return"]=True
                console.stage("after","complete")
            operator(console,"final","请关闭临时常亮，回到表盘并确认显示、触摸和 KEY1。")
            require(not real or args.action!="install" or report["idle_return"],
                    "Flash and original app evidence retained; receiver advertising or idle return not observed")
            require(not real or report["wire_verified"], "Raw wire review incomplete; see window report")
            report["hardware_verified"]=real
            report["passed"]=True
        except (Exception,KeyboardInterrupt) as e:
            report.update(passed=False,error=repr(e))
        finally:
            write_json(root/"window.json",report)
            console.finish("passed" if report["passed"] else "error", report.get("error","本窗证据已保存。"))
    return report


if __name__=="__main__":
    if len(sys.argv)==4 and sys.argv[1]=="app-worker":
        p=Path(sys.argv[2]);require(digest(p)==sys.argv[3],"Changed worker configuration")
        app_worker(read_json(p))
    else:
        parser=argparse.ArgumentParser(description=__doc__)
        parser.add_argument("mode",choices=("capture","simulate"))
        parser.add_argument("--action",choices=SCOPE,required=True)
        parser.add_argument("--authorize",required=True)
        for opt in ("out","build","bundle"):
            parser.add_argument("--"+opt,required=True,type=Path)
        for opt in ("nor-fixture","before-fixture","after-fixture","prior"):
            parser.add_argument("--"+opt,type=Path)
        a=parser.parse_args()
        for k in ("before_fixture","after_fixture"):
            if getattr(a,k): setattr(a,k,getattr(a,k).resolve())
        result=window(a);print(json.dumps(result,indent=2),flush=True)
        raise SystemExit(0 if result["passed"] else 1)
