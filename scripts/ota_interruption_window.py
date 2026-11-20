"""Observe stable B's settings-menu OTA entry, two prefix-only BLE cases and idle return."""
import argparse
import asyncio
import json
import os
from pathlib import Path
import re
import sys
import time
from types import SimpleNamespace

from battery_console import Console
from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import digest, file_record, write_json
from ota_first_install import read_json, require
from ota_first_window import owned_endpoint, run_owned
from ota_health_evidence import ADDRESS, IDLE, STABLE, restored_values
import ota_interruption_peer as peer
from ota_phone_board import ADVERTISING, START, confirmed_identity
from ota_rollback_evidence import uart_lines
from ota_rom_transport import BoundedSerial

PHASES = [("entry","监听开启后，从手表设置 → 系统操作 → 固件升级确认一次"),
          ("peer","电脑核对主动取消和连接内接收超时"), ("return","等待手表自动回到稳定 B")]
HANDOFF = re.compile(rb"\[ota-product\] handoff_intent=receiver saved_generation=(\d+)")
NOT_READY = b"[ota-radio] request=40 result=4 accepted=0 sent=16"
SCOPE = "passive-com5-and-bounded-cancel-timeout-on-stable-06"


def console_lines(raw):
    return [line.removeprefix(b"msh />").lstrip() for line in uart_lines(raw)]


def entered(raw):
    lines = console_lines(raw)
    intent = [i for i,s in enumerate(lines) if HANDOFF.fullmatch(s)]
    if len(intent) != 1 or ADVERTISING not in lines:
        return False
    return (lines[intent[0]+1:lines.index(ADVERTISING)].count(b"SFBL") == 1 and
            confirmed_identity(raw, None, ADDRESS) == ADDRESS)


def returned(raw):
    lines = console_lines(raw)
    if IDLE not in lines:
        return False
    after = lines[lines.index(IDLE)+1:]
    return b"SFBL" in after and STABLE in after[after.index(b"SFBL")+1:]


def review_uart(raw):
    require(entered(raw) and returned(raw), "Receiver entry or stable B return missing")
    lines = console_lines(raw)
    require(lines.count(IDLE) == 1 and lines.count(b"SFBL") == 2 and lines.count(STABLE) == 1,
            "Unexpected boot in attended window")
    session = lines[lines.index(ADVERTISING)+1:lines.index(IDLE)]
    require([s for s in session if s.startswith(b"[ota-radio] request=")] == [START, NOT_READY, START, NOT_READY],
            "Board START and discarded-progress replies differ from the bounded peer")
    require(not any(b"local install" in s or b"[ota-ready]" in s for s in lines), "Unexpected local install")
    handoff, = [HANDOFF.fullmatch(s) for s in lines if HANDOFF.fullmatch(s)]
    return dict(saved_generation=int(handoff[1]), stable_release=2026111802,
                restored_settings=restored_values(lines[lines.index(IDLE)+1:]))


async def observe(config):
    root = Path(config["root"])
    require(config["authorization"] == SCOPE and os.name == "nt", "Expected authorized original Windows endpoint")
    start = time.monotonic()
    def check():
        require(not (root/"cancel").exists() and time.monotonic()-start < 470, "Window cancelled or expired")
    journal = Journal(root/"passive", mode="passive-uart", binding={"port":"COM5","commands":[]})
    serial = BoundedSerial("COM5","after",journal,check,lambda: port_identity("COM5","1-1.1"))
    task = None
    raw = bytearray()
    advertised = None
    try:
        with (root/"passive/uart.bin").open("xb",buffering=0) as stream:
            serial.open()
            (root/"listening").touch(exist_ok=False)
            while True:
                check()
                chunk = serial.read(8192)
                if chunk:
                    stream.write(chunk)
                    raw.extend(chunk)
                    require(len(raw)<=2*1024*1024, "Passive UART exceeds bound")
                    if task is None and entered(raw):
                        advertised = time.monotonic()
                        task = asyncio.create_task(peer.run(SimpleNamespace(mode="capture", authorize=peer.SCOPE,
                                                   receiver=None, out=root/"peer")))
                    if returned(raw):
                        break
                if advertised is None:
                    require(time.monotonic()-start < 170, "No confirmed receiver within menu-entry window")
                await asyncio.sleep(.025)
    finally:
        try:
            serial.close()
        finally:
            if task is not None and not task.done():
                task.cancel()
            if task is not None:
                await task
    write_json(root/"return.json",review_uart(raw))


def capture(args):
    require(os.name == "nt" and args.authorize == SCOPE, "Fresh device authorization required")
    peer.prefix_requests()
    root = args.out.resolve()
    journal = Journal(root,binding={"authorization":SCOPE,"receiver_address":ADDRESS,
                                   "transmit_file_prefix_bytes":4096},mode="cancel-timeout")
    cfg = root/"request.json"
    write_json(cfg,dict(root=str(root),authorization=SCOPE))
    report = dict(passed=False)
    with Console(journal,PHASES,mode="06 稳定 B 的取消与超时") as console:
        try:
            print(console.url,flush=True)
            os.startfile(console.url)
            with owned_endpoint("COM5",report):
                port_identity("COM5","1-1.1")
                job = root/"job"
                job.mkdir()
                stage = None
                def update():
                    nonlocal stage
                    desired = "return" if (root/"peer/result.json").exists() else (
                        "peer" if (root/"peer").exists() else "entry" if (root/"listening").exists() else None)
                    if desired and desired != stage:
                        if stage is not None:
                            console.stage(stage,"complete")
                        console.stage(desired,"running")
                        print(desired,flush=True)
                        stage = desired
                code = run_owned([sys.executable,"-B",str(Path(__file__).resolve()),"worker",str(cfg),digest(cfg)],
                                 job,console,root/"cancel",480,observe=update)
                report["worker_exit"] = code
                # Review raw evidence even if the worker's final summary write failed.
                report["board"] = review_uart((root/"passive/uart.bin").read_bytes())
                report["ble"] = peer.review_gatt(root/"peer/ble.jsonl")
                report["uart"] = file_record(root/"passive/uart.bin")
                report["ble_trace"] = file_record(root/"peer/ble.jsonl")
                require(read_json(root/"passive/progress.json")["kind"]=="serial_close_complete", "UART close not confirmed")
                for key,_ in PHASES:
                    console.stage(key,"complete")
            report["passed"] = console.seal("passed") == "passed"
        except (Exception,KeyboardInterrupt) as exc:
            report["error"] = repr(exc)
        finally:
            write_json(root/"window.json",report)
            console.finish("passed" if report["passed"] else "error", report.get("error","原始应答和稳定 B 启动日志已保存。"))
    return report


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "worker":
        cfg = Path(sys.argv[2])
        require(digest(cfg) == sys.argv[3], "Changed worker request")
        asyncio.run(observe(read_json(cfg)))
    else:
        p = argparse.ArgumentParser(description=__doc__)
        p.add_argument("--authorize", required=True)
        p.add_argument("--out", type=Path, required=True)
        result = capture(p.parse_args())
        print(json.dumps(result, ensure_ascii=False),flush=True)
        raise SystemExit(0 if result["passed"] else 1)
