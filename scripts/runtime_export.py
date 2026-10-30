"""One bounded, read-only runtime export through the shared recorded session.

No start/stop, Flash command, RTC write, reconnect or device cleanup command.
"""
import argparse
import json
from pathlib import Path
import re
import secrets
import threading
import time

from battery_console import Cancelled
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import RecordedClient, RecordedSession, SerialTransport, lease_directory
from runtime_record_decode import decode_capture, snapshot_crc
from serial_lab import prepare_capture


class ExportClient(RecordedClient):
    COMMAND_PATTERN = r"wf_record (?:snapshot [1-9][0-9]{0,19}|summary [1-9][0-9]{0,19} (?:current|saved)|samples [1-9][0-9]{0,19} [0-7])"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL", b"RT-Thread")


def collect(output, transport, *, binding, identity_check, locks, seconds=60, cancel=None):
    if not 0 < seconds <= 60 or output.exists():
        raise ValueError("New output directory and a 0..60s bound required")
    nonce = secrets.randbits(64) or 1
    cancel = cancel or threading.Event()
    tool, protocol = file_record(Path(__file__)), file_record(Path(__file__).with_name("runtime_record_decode.py"))
    binding = dict(binding, seconds=seconds, nonce=nonce, policy="RAM snapshot and cached pages only; no control retries",
                   tool=tool, protocol=protocol)
    journal = Journal(output, binding=binding, mode="runtime_export")

    def identity():
        verify_files([tool, protocol])
        return identity_check()

    session = RecordedSession(journal, transport, cancel, ExportClient, identity=identity, locks=locks, timeout=6)
    outcome, detail, decoded = "passed", "", None
    try:
        with session as client:
            deadline = time.monotonic() + seconds
            replies = client.request(f"wf_record snapshot {nonce}",
                lambda line: f"[record-snapshot] schema=1 token={nonce} " in line, deadline=deadline)
            headers = [line for line in replies if f"[record-snapshot] schema=1 token={nonce} " in line]
            if len(headers) != 1:
                raise ValueError("One snapshot header required")
            fields = dict(re.findall(r"(\w+)=(\d+)", headers[0]))
            count, saved = int(fields["samples"]), int(fields["saved"])
            if not 0 <= count <= 64 or saved not in (0, 1):
                raise ValueError("Snapshot bounds")
            for label in (("current", "saved") if saved else ("current",)):
                client.request(f"wf_record summary {nonce} {label}",
                    lambda line: f"[record-summary-end] token={nonce}" in line, deadline=deadline)
            for page in range((count+7)//8):
                client.request(f"wf_record samples {nonce} {page}",
                    lambda line, page=page: f"[record-samples-end] token={nonce} page={page} " in line, deadline=deadline)
            if cancel.is_set():
                raise Cancelled("Export cancelled")
            if time.monotonic() >= deadline:
                raise TimeoutError("Export window expired")
        decoded = decode_capture((output / "uart.bin").read_bytes(), nonce)
        write_json(output / "records.json", decoded)
    except BaseException as exc:
        outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
        detail = repr(exc)
        journal.event("export_error", outcome=outcome, error=detail)
    if cancel.is_set() and outcome == "passed":
        outcome, detail = "cancelled", "Cancelled while closing host resources"
    report = dict(passed=outcome == "passed" and session.resources_closed, outcome=outcome, detail=detail,
                  simulation=binding["simulation"], hardware_verified=False, device_identity_verified=False,
                  nonce=nonce, resources_closed=session.resources_closed, reset=False, pm_control=False,
                  recording_control=False, failed_recording=decoded["failed"] if decoded else None)
    raw = output / "uart.bin"
    report["uart"] = file_record(raw) if raw.exists() else None
    write_json(output / "export.json", report)
    journal.event("finished", outcome=outcome, passed=report["passed"], report_sha256=digest(output / "export.json"))
    return report


class Simulation:
    port = "SIMULATED-RUNTIME-EXPORT"

    def __init__(self, raw):
        header = re.search(rb"\[record-snapshot\] schema=1 token=(\d+) ", raw)
        if not header:
            raise ValueError("Missing fixture header")
        self.original = int(header[1])
        decode_capture(raw, self.original)
        self.lines = raw.decode("ascii").splitlines()
        self.queue, self.commands = bytearray(), []
        self.opened = self.closed = False

    def open(self):
        self.opened = True

    def close(self):
        self.closed = True

    def write(self, wire):
        command = wire.decode("ascii").strip()
        self.commands.append(command)
        parts = command.split()
        nonce = int(parts[2])
        if parts[1] == "snapshot":
            lines = [line for line in self.lines if line.startswith("[record-snapshot]")]
        elif parts[1] == "summary":
            lines, active = [], False
            for line in self.lines:
                if line.startswith("[record-summary]"):
                    active = f"kind={parts[3]}" in line
                if active:
                    lines.append(line)
                if line.startswith("[record-summary-end]"):
                    active = False
        else:
            page = int(parts[3])
            lines = []
            for line in self.lines:
                index = re.search(r"index=(\d+)", line)
                if (line.startswith(("[record-sample]", "[record-sample-data]")) and index and int(index[1])//8 == page) or (
                        line.startswith("[record-samples-end]") and f"page={page} " in line):
                    lines.append(line)
        lines = [line.replace(f"token={self.original}", f"token={nonce}") for line in lines]
        if parts[1] == "snapshot":
            fields = dict(re.findall(r"(\w+)=(\d+)", lines[0]))
            checksum = snapshot_crc(nonce, int(fields["samples"]), int(fields["saved"]), int(fields["failed"]))
            lines[0] = re.sub(r"crc=[0-9a-f]{8}", f"crc={checksum:08x}", lines[0])
        reply = "\r\n".join(lines)+"\r\n"
        self.queue.extend(reply.encode("ascii"))
        return len(wire)

    def read(self, count):
        value = bytes(self.queue[:min(count, 43)])
        del self.queue[:len(value)]
        return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    for mode in ("simulate", "capture"):
        sub = modes.add_parser(mode)
        sub.add_argument("--out", type=Path, required=True)
        sub.add_argument("--seconds", type=float, default=60)
        if mode == "simulate":
            sub.add_argument("--fixture", type=Path, required=True)
        else:
            for option in ("port", "location", "expected-bin"):
                sub.add_argument("--"+option, required=True)
            for option in ("result", "flash", "source-archive", "build-archive", "recorded-root"):
                sub.add_argument("--"+option, type=Path, required=option in ("result", "flash"))
            sub.add_argument("--confirm-same-board-and-firmware", action="store_true", required=True)
    args = parser.parse_args()
    if args.mode == "simulate":
        binding, identity = {"simulation": True}, lambda: {"simulation": True}
        transport, locks = Simulation(args.fixture.read_bytes()), args.out.parent / "simulation-locks"
    else:
        build = json.loads(args.result.read_text(encoding="utf-8-sig"))
        if build.get("storage_probe") or build.get("resource_probe") or not any(
                p["path"].replace("\\", "/").endswith("/product_runtime_record.c") for p in build.get("project_sources", [])):
            raise ValueError("An ordinary recorder build is required")
        binding, identity = prepare_capture(args.result, args.flash, args.expected_bin, args.port, args.location,
            args.confirm_same_board_and_firmware, source_archive=args.source_archive,
            build_archive=args.build_archive, recorded_root=args.recorded_root)
        transport, locks = SerialTransport(args.port), lease_directory()
    report = collect(args.out, transport, binding=binding, identity_check=identity, locks=locks, seconds=args.seconds)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
