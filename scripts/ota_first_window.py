"""Attended original-board installation and recovery with a continuous endpoint lease."""
import argparse
from contextlib import contextmanager, nullcontext
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
from ota_phone_board import (BOARD_FILE, FILE_DIGESTS, READY_PHASE, PhoneObservation,
                             confirmed_identity, phone_evidence, verify_fixture)
from ota_phone_pair import (ACTIVE_SECONDS, BODY_AT_100, BODY_FILE, BODY_MIN_SECONDS, LISTEN_SECONDS, PairedObservation,
                            android_capture, android_receipts, body_completion, pair_files, uart_receipts)
from ota_identity_window import IDENTITY_ACTIONS, restore_source, review_pair
from ota_receiver_scan import installed_address
from ota_b_execute import recovery_check
from ota_rom_backup import application, Simulation
from ota_rom_device import FileCancel
from ota_rom_process import ReadProcess, ReaderCleanupError
from ota_rom_transport import BoundedSerial
from serial_lab import prepare_capture

SCOPE = {"install": "first-install-receiver-journal-boot-and-enter",
         "verify-original": "verify-original-nor-and-start-original",
         "enter-receiver": "enter-pinned-receiver-without-flash-write",
         "inspect-receiver": "read-installed-key-regions-enter-for-gatt-and-return",
         "install-identity": "replace-installed-receiver-and-boot-enter-identity",
         "enter-identity": "enter-installed-identity-receiver-read-only",
         "restore-identity": "restore-provisioned-pair-after-identity-window",
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


def run_owned(argv, directory, console, cancel, seconds, *, observe=None):
    deadline=time.monotonic()+seconds
    child=ReadProcess(argv,directory)
    try:
        while child.poll() is None:
            if console.cancelled.is_set() or time.monotonic()>=deadline:
                cancel.touch(exist_ok=True)
                grace=time.monotonic()+3
                while child.poll() is None and time.monotonic()<grace: time.sleep(.05)
                raise Cancelled("Owned worker cancelled or timed out")
            if observe is not None:
                observe()
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
    if action in IDENTITY_ACTIONS:
        expected, chunks = review_pair(directory, action, bundle, source)
        return review_transfers(directory, expected, chunks)
    if action == "inspect-receiver":
        ranges = [(0,65536),(0x10000,61440),(0x20000,4096),(0x820000,360448),
                  (0xda0000,4096),(0xda2000,4096)]
        expected = []
        for start, size in ranges:
            for off in range(start, start + size, 65536):
                part = source[off:min(off + 65536, start + size)]
                expected.append((f"key-{off:08x}", off, part))
        tx = (directory/"tx.bin").read_bytes()
        enter = b"burn_speed 500000 10\r\r\n"
        leave = b"burn_speed 1000000 10\r"
        begin = tx.rindex(enter) + len(enter)
        commands = b"".join(f"burn_read 0x{0x12000000+off:08x} 0x{len(data):08x}\r".encode()
                            for _, off, data in expected)
        require(tx[begin:tx.index(leave, begin)] == commands, "Unexpected command in critical-region pass")
        return review_transfers(directory, expected, [])
    before=(directory/"before.bin").read_bytes()
    target=bytearray(before)
    spans=[]
    read_only = action in ("verify-original", "enter-receiver")
    entries=[] if read_only else read_json(bundle/"bundle.json")["entries"]
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
    if source is None and action != "enter-receiver":
        require((directory/"original-b-4MiB.bin").read_bytes()==before[0x420000:0x820000],"Original B archive differs")
    if not read_only:
        require(target==(directory/"after.bin").read_bytes(),"Unexpected whole-NOR result")
        expected.extend((f"{role}-{off:08x}",off,part) for role,off,part in chunks)
        expected.extend((f"after-{i:08x}",i,target[i:i+65536]) for i in range(0,len(target),65536))
    return review_transfers(directory, expected, chunks)


def review_transfers(directory, expected, chunks):
    raw=(directory/"rx.bin").read_bytes()
    tx=(directory/"tx.bin").read_bytes()
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


def discovery_reference():
    evidence = read_json(ROOT/"docs/evidence/2026-11-19/ota-receiver-first-install.json")
    window = ROOT/evidence["capture"]
    flash = evidence["flash_before_first_receiver_boot"]
    before = prior_source(window, True)
    require(digest(before) == flash["backup_sha256"], "Original full rescue backup changed")
    installed = window/"media/after.bin"
    require(digest(installed) == flash["expected_and_readback_sha256"], "Installed NOR reference changed")
    air = window/"windows-air.jsonl"
    installed_address(air)
    return installed, air


def observe_gatt(root, console, report, air):
    if "gatt_worker_exit" in report:
        return
    uart = root/"media/startup-uart.bin"
    if not uart.exists():
        return
    state = phone_evidence(uart.read_bytes())
    if not state["advertising"] or state["idle_return"]:
        return
    job = root/"job-gatt"
    job.mkdir()
    console.stage("gatt", "running")
    code = run_owned([sys.executable,"-B",str(ROOT/"scripts/ota_receiver_scan.py"),
                      "--out",str(root/"gatt.jsonl"),"--seconds","6","--gatt-from",str(air)],
                     job,console,root/"cancel",45)
    report["gatt_worker_exit"] = code
    console.stage("gatt", "complete" if code == 0 else "error")


def window(args):
    real=args.mode=="capture"
    phone = args.action in ("enter-receiver", "install-identity", "enter-identity")
    inspect = args.action == "inspect-receiver"
    identity = args.action in IDENTITY_ACTIONS
    original_address = None
    expected_address = None
    phone_zip = getattr(args, "phone_zip", None)
    phone_file = getattr(args, "phone_file", None)
    pair = getattr(args, "signature_then_body", False)
    adb = getattr(args, "phone_adb", None)
    require(not pair or (args.action == "enter-identity" and phone_file is None),
            "Paired fixtures require one read-only entry without a separate file override")
    require(not adb or (real and pair), "USB phone log capture applies only to a live paired window")
    require(not (real and pair) or adb is not None, "Automated paired acceptance requires the authorized Android DFU log")
    require(phone == bool(phone_zip) and (phone or phone_file is None), "A phone window requires the distributed ZIP")
    require(phone_file in (None, BOARD_FILE) or args.action == "enter-identity",
            "This fixture requires read-only entry on the installed pair")
    fixture = None
    if pair:
        fixtures = pair_files(phone_zip)
        fixture = fixtures[0]
    elif phone:
        fixture = verify_fixture(phone_zip, phone_file or BOARD_FILE)
    require(args.action in SCOPE and ((real and os.name=="nt" and args.authorize==SCOPE[args.action]) or
            (not real and args.authorize=="simulation")), "Current action-specific authorization required")
    require(args.action.startswith("restore")==bool(args.prior),"Independent recovery requires its original window")
    source=None
    if inspect or identity:
        source, air = discovery_reference()
        original_address = installed_address(air) if identity else None
        if args.action == "enter-identity":
            previous = read_json(ROOT/"docs/evidence/2026-11-19/ota-identity-device.json")
            uart = "media/startup-uart.bin"
            raw = (ROOT/previous["capture"]/uart).read_bytes()
            require(sha(raw) == previous["sha256"][uart], "First receiver boot UART differs from its verified capture")
            expected_address = confirmed_identity(raw, original_address)
            require(expected_address, "Missing verified receiver address from its first boot")
        if args.action == "restore-identity":
            restore_source(args.prior.resolve(), real, source)
        if inspect and real:
            from importlib.metadata import version
            from bleak.backends.winrt.client import BleakClientWinRT
            require(version("bleak") == "3.0.2", "Use the reviewed Windows Bleak version")
    elif args.prior:
        source = prior_source(args.prior.resolve(),real)
    build=read_json(args.build)
    require(build["kind"]=="original-board-first-install" and build["device_actions"] is False,"Wrong bridge build")
    inputs=[*build["sources"],build["executable"],build["sdk_lock"]]
    verify_files(inputs,ROOT)
    exe=ROOT/build["executable"]["path"]
    bundle=args.bundle.resolve()
    pkg=read_json(bundle/"bundle.json")
    scope = "original-cyclelog-identity-pair" if identity else "original-cyclelog-first-install"
    require(pkg["baseline_sha256"]==BASE_SHA and pkg["scope"]==scope,"Wrong installation bundle")
    verify_files(pkg["entries"])
    root=args.out.resolve()
    journal=Journal(root,binding=dict(action=args.action,simulation=not real,build=file_record(args.build),
                    bundle=file_record(bundle/"bundle.json"),operator_scope=args.authorize,
                    signature_then_body=pair,phone_dfu_log=bool(adb)),mode="first-install-window")
    def operator(console, phase, message, choices=None):
        answer = None
        if real:
            answer = console.prompt(phase,message,180,choices=choices)
        else:
            journal.event("simulated_operator",phase=phase,message=message)
        console.stage(phase,"complete")
        return answer
    report=dict(simulation=not real,action=args.action,flash_verified=False,wire_verified=False,
                receiver_advertising=False,idle_return=False,application_return=False,hardware_verified=False)
    phases = PHASES
    mode = "黄山派有线首装与恢复" if real else "首装离线演练"
    if phone:
        report["android_fixture"] = fixture
        filename = fixture["filename"]
        phone_phase = ("phone", f"此行显示进行中时立即扫描 WristFlow OTA，点 NOR DFU → OFFLINE，选 {filename} 一次")
        phases = [*PHASES[:2], READY_PHASE, phone_phase, *PHASES[2:]]
        mode = f"Android {filename} " + ("与原表盘返回" if real else "离线演练")
    if pair:
        report["android_fixtures"] = fixtures
        report["phone_budget_seconds"] = dict(active=ACTIVE_SECONDS, minimum_at_body_start=BODY_MIN_SECONDS,
                                              return_reserve=LISTEN_SECONDS-ACTIVE_SECONDS)
        phases = [*PHASES[:2], READY_PHASE, phone_phase,
                  ("body", f"本行显示进行中时，保留 03 结果，只选一次 {BODY_FILE}，等待传完"),
                  ("disconnect","仅当本行显示进行中时，停止发包，若仍在传输请断开手机，等待原表盘"),
                  ("first_file","记录第一次选包"), *PHASES[2:]]
        mode = "Android 03 签名和 04 正文" + ("同窗验收" if real else "离线演练")
    if inspect:
        phases = [*PHASES[:2], ("gatt","电脑读取接收器 GATT 服务与特征"), *PHASES[2:]]
        mode = "接收器关键区和 GATT 只读检查" if real else "接收器关键区离线演练"
    with Console(journal,phases,mode=mode) as console:
        print(console.url,flush=True)
        try:
            with owned_endpoint("COM5" if real else "SIMULATED-FIRST-INSTALL",report):
                if real: port_identity("COM5","1-1.1")
                console.stage("before","running")
                if not args.action.startswith("restore"):
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
                if phone:
                    # One physical read pass followed by the Android exchange and idle return.
                    cfg.update(seconds=1160, listen_seconds=360 if real else 0)
                if inspect:
                    cfg.update(seconds=235, listen_seconds=152 if real else 0)
                if identity:
                    cfg.update(seconds=460, listen_seconds=(300 if phone else 16) if real else 0)
                if pair:
                    cfg.update(signature_then_body=True, seconds=1740, listen_seconds=LISTEN_SECONDS if real else 0)
                path=root/"media-request.json";write_json(path,cfg)
                job=root/"job-media";job.mkdir()
                console.stage("media","running")
                verify_files(inputs,ROOT)
                if real: port_identity("COM5","1-1.1")
                media=root/"media"
                with android_capture(adb, root) if adb else nullcontext() as collector:
                    observe = None
                    if phone and real:
                        options = dict(filename=filename, original_address=original_address, expected_address=expected_address)
                        if pair:
                            observation = PairedObservation(media,console,android_log=root/"android-dfu.log",
                                                            collector=collector,**options)
                        else:
                            observation = PhoneObservation(media,console,**options)
                        observe = observation.poll
                    if inspect and real:
                        observe = lambda: observe_gatt(root, console, report, air)
                    code=run_owned([str(exe),"--config",str(path),digest(path)],job,console,root/"cancel",cfg["seconds"]+20,
                                   observe=observe)
                events=[json.loads(s) for s in (media/"media-events.jsonl").read_text().splitlines()]
                fact="checked_original_only" if args.action=="verify-original" else "entire_nor_verified"
                if phone:
                    fact = "stable_a_verified"
                if inspect:
                    fact = "entry_ranges_verified"
                if identity:
                    fact = "entry_ranges_verified" if args.action == "enter-identity" else "identity_pair_verified"
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
                if inspect:
                    if real:
                        report["receiver_uart"] = phone_evidence(log)
                    else:
                        journal.event("simulated_discovery_window")
                        console.stage("gatt", "complete")
                message = "请确认原 CycleLog 表盘、显示和触摸已返回，打开临时常亮读取后态。"
                file_choices = None
                if pair:
                    choices = [*FILE_DIGESTS, BODY_FILE, "其他文件、未发送或无法确认"]
                    if real:
                        state = phone_evidence(log)
                        summary = uart_receipts(state)
                        report["phone_uart"] = state
                        report["pair_uart"] = summary
                        report["repeat_identity_verified"] = bool(confirmed_identity(log, original_address, expected_address))
                        if adb:
                            raw = (root/"android-dfu.log").read_text(encoding="utf-8", errors="replace")
                            report["pair_android"] = android_receipts(raw, expected_address)
                        require(not state["install_started"] and state["product_return"],
                                "Paired window closed without a confirmed idle return to Product. Inspect the saved capture.")
                        console.stage("disconnect", "complete")
                        console.stage("phone", "waiting")
                        console.stage("body", "waiting")
                        report["first_selected_file"] = operator(console,"first_file","请选择手机第一次实际发送的文件。",choices)
                        message = "请选择第二次实际发送的文件。若 04 结果页到了 100%，选择带 pro100 的一项。请确认原表盘和触摸正常，临时常亮已开。"
                        file_choices = [BODY_AT_100, *choices]
                    else:
                        for phase in ("phone_ready","phone","body","disconnect","first_file"):
                            console.stage(phase,"complete")
                        journal.event("simulated_phone_pair", files=[f["filename"] for f in fixtures])
                elif phone:
                    if real:
                        transfer = phone_evidence(log)
                        report["phone_uart"] = transfer
                        if identity:
                            report["distinct_identity_verified"] = bool(confirmed_identity(log, original_address))
                        if expected_address is not None:
                            report["repeat_identity_verified"] = bool(confirmed_identity(log, original_address, expected_address))
                        require(not transfer["install_started"], "Unexpected installation on this phone window; inspect the board")
                        received = (transfer["first_data_rejected"] and transfer["product_return"] and
                                    report.get("distinct_identity_verified", True) and report.get("repeat_identity_verified", True))
                        console.stage("phone", "complete" if received else "error")
                        if received:
                            message = f"请核实手机本次只选了 {filename}，并确认原表盘和触摸正常、临时常亮已开。"
                            if filename != BOARD_FILE:
                                message = "请选择这一次手机实际发送的文件，并确认原表盘和触摸正常、临时常亮已开。"
                                file_choices = [*FILE_DIGESTS, "其他文件或无法确认"]
                    else:
                        journal.event("simulated_phone", filename=fixture["filename"])
                        console.stage("phone_ready", "complete")
                        console.stage("phone", "complete")
                selected_file = operator(console,"return",message,file_choices)
                if pair and real:
                    saw_pro100 = selected_file == BODY_AT_100
                    report["second_selected_file"] = BODY_FILE if saw_pro100 else selected_file
                    report["operator_saw_pro100"] = saw_pro100
                    phone_report = report["pair_android"]
                    completion = body_completion(phone_report, saw_pro100)
                    report["body_completion_evidence"] = completion
                    signature_ok = (report["repeat_identity_verified"] and report["first_selected_file"] == filename and
                                    summary["signature_rejected"] and phone_report["signature_rejected"])
                    body_ok = (signature_ok and report["second_selected_file"] == BODY_FILE and summary["body_end_rejected"] and
                               completion is not None and summary["body_retries"] == phone_report["sessions"][1]["retries"])
                    console.stage("phone", "complete" if signature_ok else "error")
                    console.stage("body", "complete" if body_ok else "error")
                    report["phone_cases"] = {filename:signature_ok, BODY_FILE:body_ok}
                elif file_choices:
                    report["operator_selected_file"] = selected_file
                    if selected_file != filename:
                        console.stage("phone", "error")
                console.stage("after","running")
                after=app_phase(root,"after",args,console)
                report["continuity"]=recovery_check(before,after,(after["closed_ns"]-before["closed_ns"])/1e9,
                                                     args.action.startswith("restore"))
                report["application_return"]=True
                console.stage("after","complete")
            operator(console,"final","请关闭临时常亮，回到表盘并确认显示、触摸和 KEY1。")
            require(not real or args.action!="install" or report["idle_return"],
                    "Flash and original app evidence retained; receiver advertising or idle return not observed")
            if real and pair:
                require(all(report["phone_cases"].values()), "Paired phone acceptance incomplete. See both receipts and file selections.")
            elif real and phone:
                require(received, "Android first-DATA rejection and idle Product return were not both observed")
                if filename != BOARD_FILE:
                    require(report.get("operator_selected_file") == filename, "Selected phone file did not match the requested fixture")
            if real and inspect:
                state = report["receiver_uart"]
                require(state["product_return"] and not state["install_started"] and not state["replies"]
                        and report.get("gatt_worker_exit") == 0, "See receiver return and independent GATT observation")
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
        phone_args = parser.add_mutually_exclusive_group()
        phone_args.add_argument("--phone-file", choices=FILE_DIGESTS, help="Single distributed first-block rejection fixture")
        phone_args.add_argument("--signature-then-body",action="store_true",help="One read-only entry for 03 followed by 04")
        parser.add_argument("--phone-adb",type=Path,help="Collect the paired window's USB SIFLI BLE DFU tag with operator permission")
        for opt in ("out","build","bundle"):
            parser.add_argument("--"+opt,required=True,type=Path)
        for opt in ("nor-fixture","before-fixture","after-fixture","prior","phone-zip"):
            parser.add_argument("--"+opt,type=Path)
        a=parser.parse_args()
        for k in ("before_fixture","after_fixture"):
            if getattr(a,k): setattr(a,k,getattr(a,k).resolve())
        result=window(a);print(json.dumps(result,indent=2),flush=True)
        raise SystemExit(0 if result["passed"] else 1)
