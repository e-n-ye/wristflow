"""One explicitly authorized SDK reboot; preserve boot bytes, never retry."""
import argparse
import json
from pathlib import Path
import threading
import time

from battery_console import Cancelled
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import RecordedClient, RecordedSession, SerialTransport, SessionChanged, lease_directory
from runtime_record_decode import decode_capture
from serial_lab import prepare_capture


class OnceTransport:
    def __init__(self, inner):
        self.inner, self.port, self.attempted = inner, inner.port, False
    def open(self): return self.inner.open()
    def close(self): return self.inner.close()
    def read(self, count): return self.inner.read(count)
    def write(self, wire):
        if wire != b"reboot\r\n" or self.attempted:
            raise ValueError("Exactly one SDK reboot is allowed")
        self.attempted = True  # Includes a short write or lost acknowledgement.
        return self.inner.write(wire)


class RebootClient(RecordedClient):
    COMMAND_PATTERN = r"reboot"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL", b"RT-Thread")


def baseline(directory):
    value = json.loads((directory/"records.json").read_text(encoding="utf-8"))
    report = json.loads((directory/"export.json").read_text(encoding="utf-8"))
    binding = json.loads((directory/"binding.json").read_text(encoding="utf-8"))
    if (not report["passed"] or not report["resources_closed"] or report["simulation"] or
            value["failed"] or value["saved"] is None or not value["current"]["flags"] & 2 or
            decode_capture((directory/"uart.bin").read_bytes(), value["token"]) != value):
        raise ValueError("A complete, running, healthy-storage real baseline is required")
    events = [json.loads(line) for line in (directory/"events.jsonl").read_text().splitlines()]
    closed = [x["wall_ns"] for x in events if x["kind"] == "transport_closed"]
    if len(closed) != 1:
        raise ValueError("One closed baseline session required")
    return value, binding, closed[0], [file_record(directory/name) for name in
        ("records.json", "export.json", "binding.json", "uart.bin", "events.jsonl")]


def fresh(closed_ns):
    if not 0 <= time.time_ns()-closed_ns <= 120_000_000_000:
        raise ValueError("Baseline older than120s; capture a new baseline, do not reboot")


def collect(output, transport, *, binding, identity_check, locks, seconds=20, cancel=None):
    if output.exists() or not 0 < seconds <= 30:
        raise ValueError("New output directory and0..30s bound required")
    cancel = cancel or threading.Event()
    source = file_record(Path(__file__))
    journal = Journal(output, binding=dict(binding, adapter=source, seconds=seconds,
        policy="One SDK reboot, passive boot capture, no retry/reopen/RTC/PM/save/flash"), mode="sdk_reboot")
    wrapped = OnceTransport(transport)
    def identity():
        verify_files([source])
        return identity_check()
    session = RecordedSession(journal, wrapped, cancel, RebootClient,
        identity=identity, locks=locks, timeout=seconds)
    observed, outcome, detail = False, "passed", ""
    try:
        with session as client:
            deadline = time.monotonic()+seconds
            journal.event("reboot_intent", single_send=True)
            try:
                client.request("reboot", lambda line: False, deadline=deadline)
            except SessionChanged:
                if not wrapped.attempted:
                    raise  # An unsolicited pre-send boot invalidates the baseline.
                observed = True
                journal.event("expected_boot_observed")
            client.BOOT_MARKERS = ()  # Only after the single send and expected boot.
            while time.monotonic() < deadline:
                if cancel.is_set(): raise Cancelled("Cancelled after reboot; no further control")
                if not client.read_chunk(): time.sleep(.002)
                client.take_lines()
            client.drain()
    except BaseException as exc:
        outcome = "cancelled" if isinstance(exc,(Cancelled,KeyboardInterrupt)) else "timeout" if isinstance(exc,TimeoutError) else "error"
        detail = repr(exc)
        journal.event("reboot_capture_error", error=detail)
    raw = output/"uart.bin"
    starts = raw.read_bytes().count(b"[product] storage=") if raw.exists() else 0
    started = observed and starts == 1
    report = dict(passed=outcome=="passed" and started and session.resources_closed,
        outcome=outcome, detail=detail, simulation=binding["simulation"], hardware_verified=False,
        reboot_write_attempted=wrapped.attempted, boot_observed=observed, product_start_count=starts, product_start_observed=started,
        reboot_outcome_uncertain=wrapped.attempted and not started, resources_closed=session.resources_closed,
        recording_control=False, clock_write=False, pm_control=False, flash=False,
        uart=file_record(raw) if raw.exists() else None)
    if not report["passed"] and outcome=="passed": report["outcome"]="incomplete"
    write_json(output/"reboot.json",report)
    journal.event("finished", passed=report["passed"], report_sha256=digest(output/"reboot.json"))
    return report


class Simulation:
    port = "SIMULATED-SDK-REBOOT"
    def __init__(self): self.queue=bytearray(); self.commands=[]; self.closed=False
    def open(self): pass
    def close(self): self.closed=True
    def write(self,wire):
        self.commands.append(wire)
        self.queue.extend(b"RT-Thread simulated\r\n[product] storage=ready restore=loaded rtc=in range\r\n")
        return len(wire)
    def read(self,count):
        data=bytes(self.queue[:count]); del self.queue[:len(data)]; return data


def main():
    p=argparse.ArgumentParser(description=__doc__)
    sub=p.add_subparsers(dest="mode",required=True)
    for mode in ("simulate","capture"):
        s=sub.add_parser(mode); s.add_argument("--out",type=Path,required=True)
        s.add_argument("--seconds",type=float,default=20 if mode=="capture" else .1)
        if mode=="capture":
            for key in ("result","flash","source-archive","before"):
                s.add_argument("--"+key,type=Path,required=True)
            for key in ("port","location","expected-bin"): s.add_argument("--"+key,required=True)
            s.add_argument("--confirm-same-board-and-firmware",action="store_true",required=True)
    args=p.parse_args()
    if args.mode=="simulate":
        binding, check, transport, locks={"simulation":True},lambda:{},Simulation(),args.out.parent/"simulation-locks"
    else:
        value, prior, closed, files=baseline(args.before); fresh(closed)
        binding, endpoint_check=prepare_capture(args.result,args.flash,args.expected_bin,args.port,args.location,
            args.confirm_same_board_and_firmware,source_archive=args.source_archive)
        if prior["endpoint"]!=binding["endpoint"] or prior["build"]["images"]!=binding["build"]["images"]:
            raise ValueError("Baseline and reboot build/endpoint differ")
        binding=dict(binding,baseline_files=files,baseline_boot=value["current"]["boot"],
            baseline_generation=value["saved"]["generation"],unsaved_recorded_ms=value["unsaved_recorded_ms"])
        def check():
            verify_files(files); fresh(closed); return endpoint_check()
        transport,locks=SerialTransport(args.port),lease_directory()
    result=collect(args.out,transport,binding=binding,identity_check=check,locks=locks,seconds=args.seconds)
    print(json.dumps(result,indent=2)); return 0 if result["passed"] else 1


if __name__=="__main__": raise SystemExit(main())
