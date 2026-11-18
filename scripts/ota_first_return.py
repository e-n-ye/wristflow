"""Return the original app after a failed initial read with zero Flash writes."""
import argparse
import json
from pathlib import Path
import re
import sys
import time
from types import SimpleNamespace

from battery_console import Console
from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import digest, file_record, write_json
from ota_first_window import app_phase, owned_endpoint, run_owned
from ota_b_execute import recovery_check
from ota_first_install import read_json, require
from ota_rom_transport import BoundedSerial

SCOPE = "return-original-after-incomplete-backup"
READ = b"burn_read 0x12000000 0x01000000\r"
RESET = b"burn_reset\r"


def check_prior(window):
    cfg = read_json(window / "media-request.json")
    report = read_json(window / "window.json")
    require(cfg["port"] == "COM5" and cfg["simulation"] is False and cfg["action"] in ("install","verify-original") and
            report["passed"] is False and report["reset_ack"] is False, "Wrong prior window")
    media = window / "media"
    events = [json.loads(s) for s in (media / "media-events.jsonl").read_text().splitlines()]
    require(events[-1]["ok"] is False and any(e.get("uart_closed") for e in events) and
            not any("intent" in e or "original_verified" in e for e in events),
            "Only the failed first read may use this exit")
    tx = (media / "tx.bin").read_bytes()
    payload = tx[tx.rindex(b"burn_speed 500000 10\r"):]
    speed = b"burn_speed 500000 10\r\r\n"
    require(payload.startswith(speed), "Unconfirmed stub baud")
    commands = payload[len(speed):]
    if commands != READ:
        blocks = re.findall(rb"burn_read 0x([0-9a-f]{8}) 0x00010000\r", commands)
        expected = b"".join(b"burn_read 0x"+addr+b" 0x00010000\r" for addr in blocks)
        require(blocks and commands == expected, "Unexpected previous stub commands")
        addresses = [int(a,16) for a in blocks]
        require(addresses[0] == 0x12000000 and addresses[-1] < 0x13000000 and
                all(b-a in (0,65536) for a,b in zip(addresses,addresses[1:])), "Unexpected original read ranges")
    rx = (media / "rx.bin").read_bytes()
    require(re.search(rb"CRC:0x[0-9a-fA-F]{8}\r\nOK\r\nmsh >$", rx), "Missing terminal RAM shell prompt")
    return read_json(window / "app-before/before-snapshot.json")


def reset_worker(root):
    journal = Journal(root / "reset", binding={"command": "burn_reset", "baud": 500000}, mode="return-original")
    deadline = time.monotonic() + 10
    def check():
        require(not (root / "cancel").exists() and time.monotonic() < deadline, "Exit timeout or cancellation")
    serial = BoundedSerial("COM5", "rom", journal, check, lambda: port_identity("COM5", "1-1.1"))
    serial.serial.baudrate = 500000
    try:
        serial.open()
        with (root / "reset/rx.bin").open("xb") as stream:
            journal.event("single_command_intent", hex=RESET.hex())
            require(serial.write(RESET) == len(RESET), "Incomplete single reset write")
            raw = bytearray()
            while time.monotonic() < deadline and len(raw) < 32768:
                part = serial.read(1024)
                raw.extend(part)
                stream.write(part)
                if re.search(rb"(?:^|[\r\n])OK(?:[\r\n]|$)", raw):
                    journal.event("reset_acknowledged")
                    return
            raise ValueError("Reset ACK not observed")
    finally:
        serial.close()


def run(args):
    require(args.authorize == SCOPE, "Separate return authorization required")
    prior, root = args.prior.resolve(), args.out.resolve()
    before = check_prior(prior)
    receipt = file_record(prior / "window.json")
    journal = Journal(root, binding={"prior": receipt, "reset": not args.already_returned}, mode="return-original")
    phases = [("reset", "返回原固件"), ("return", "确认原表盘"), ("after", "只读后态"), ("final", "关闭临时常亮")]
    report = {"reset_requested": False, "application_return": False, "flash_writes": 0}
    with Console(journal, phases, mode="原 CycleLog 返回与只读收尾") as console:
        print(console.url, flush=True)
        try:
            with owned_endpoint("COM5", report):
                port_identity("COM5", "1-1.1")
                console.stage("reset", "running")
                if not args.already_returned:
                    # Reserve the single attempt before opening the UART.
                    with (prior / "original-return-claimed.json").open("x", encoding="utf-8") as claim:
                        json.dump({"out": str(root), "authorization": args.authorize}, claim)
                    report["reset_requested"] = True
                    directory = root / "job-reset"
                    directory.mkdir()
                    cmd = [sys.executable, "-B", str(Path(__file__).resolve()), "reset-worker",
                           str(prior), str(root), digest(prior / "window.json"), args.authorize]
                    report["reset_worker_exit"] = run_owned(cmd, directory, console, root / "cancel", 16)
                console.stage("reset", "complete")
                console.prompt("return", "请查看原 CycleLog 表盘，确认显示和触摸并开启临时常亮。", 180)
                console.stage("return", "complete")
                console.stage("after", "running")
                options = SimpleNamespace(action="return-original", mode="capture", authorize=SCOPE)
                after = app_phase(root, "after", options, console)
                report["continuity"] = recovery_check(before, after, (after["closed_ns"]-before["closed_ns"])/1e9, False)
                report["application_return"] = True
                console.stage("after", "complete")
            console.prompt("final", "请关闭临时常亮，回表盘确认显示、触摸和 KEY1。", 180)
            console.stage("final", "complete")
            report["passed"] = True
        except (Exception, KeyboardInterrupt) as exc:
            report.update(passed=False, error=repr(exc))
        write_json(root / "window.json", report)
        console.finish("passed" if report["passed"] else "error", report.get("error", "本次返回已完成。"))
    return report


if __name__ == "__main__":
    if len(sys.argv) == 6 and sys.argv[1] == "reset-worker":
        prior, root = Path(sys.argv[2]), Path(sys.argv[3])
        require(sys.argv[5] == SCOPE and digest(prior / "window.json") == sys.argv[4] and
                read_json(prior / "original-return-claimed.json") == {"out": str(root), "authorization": SCOPE},
                "Missing bound original-return attempt")
        check_prior(prior)
        reset_worker(root)
    else:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--out", type=Path, required=True)
        parser.add_argument("--prior", type=Path, required=True)
        parser.add_argument("--authorize", required=True)
        parser.add_argument("--already-returned", action="store_true")
        result = run(parser.parse_args())
        print(json.dumps(result, indent=2), flush=True)
        raise SystemExit(0 if result["passed"] else 1)
