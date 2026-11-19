"""Correlate the fixed 05 trial's UART boots with its physical B and journal readbacks."""
import argparse
import json
import re
from pathlib import Path
import struct
import zlib
from zipfile import ZipFile

from ota_first_install import ROOT, read_json, require, sha
from ota_identity_window import BOOT, RECEIVER
from ota_phone_board import ADVERTISING, START, PRODUCT, PhoneObservation
from ota_probe_package import MAGIC, parse_startup

FILE = "05-rollback-2026111801.bin"
PACKAGE_SHA = "e0dab39f912eec9d4a39f5182181a70b4825444e2707b2495c830b135581121d"
RELEASE = 2026111801
IMAGE_BYTES = 3028888
B_START, B_END = 0x420000, 0x820000
META, UNIT = 0xda0000, 8192
A_TOUCH = b"[I/TOUCH] Regist touch screen driver, probe=12027f35"
B_TOUCH = b"[I/TOUCH] Regist touch screen driver, probe=12027edd"
COMMIT = b"[ota-ready] release=2026111801 bytes=3028888 trial committed"
INSTALL = b"[ota-radio] complete signed file, starting local install"
END_OK = b"[ota-radio] request=42 result=0 accepted=1480 sent=10"
TRACE_SHA = "ba2bce4faa2b51455131b9f584e33542cdb08c7b22485c3a8f1e09d64a827424"
TRACE_CHECK = dict(passed=True, startup_trace_tag=[1727834883,1910445575,324627080,2531195360],
                   handoff_trace=4)


def uart_lines(raw):
    # The original Product wraps touch registration in terminal color escapes.
    return [re.sub(rb"\x1b\[[0-9;]*m", b"", line).strip() for line in raw.split(b"\n")[:-1]]


def handoff_binary(path):
    data = path.read_bytes()
    require(len(data) == 58264 and sha(data) == TRACE_SHA, "Wrong 05 diagnostic boot")
    return data.ljust(61440,b"\xff")


def handoff_report(raw, check=TRACE_CHECK):
    """Decode only frames after the signed install commitment; retain partial captures."""
    before, separator, after = raw.partition(COMMIT + b"\r\n")
    if not separator:
        return {"committed": False, "frames": []}
    cursor = 0
    for marker in (ADVERTISING, START, END_OK, INSTALL):
        line = marker + b"\r\n"
        pos = before.find(line,cursor)
        require(pos >= cursor, "Missing or out-of-order 05 transfer receipt")
        cursor = pos + len(line)
    require(COMMIT not in after, "Ambiguous 05 commit")
    result = parse_startup(after, check)
    # Frames are binary and do not end with a newline. Remove only frames
    # whose CRC, tag and phase were validated by parse_startup.
    parts = []
    cursor = 0
    for _ in result["frames"]:
        pos = after.index(struct.pack("<I",MAGIC),cursor)
        parts.append(after[cursor:pos])
        parts.append(b"\n")
        cursor = pos + 80
    if result["decode_error"] is None:
        parts.append(after[cursor:])
    text = b"".join(parts)
    result.update(committed=True, automatic_sequence_observed=False, original_storage_seen=False)
    lines = uart_lines(text)
    boots = [i for i, line in enumerate(lines) if line == b"SFBL"]
    last_boot = lines[boots[-1]+1:] if boots else []
    if A_TOUCH in last_boot and B_TOUCH not in last_boot:
        result["original_storage_seen"] = any(s.startswith(PRODUCT) for s in last_boot[last_boot.index(A_TOUCH)+1:])
    try:
        trial_boots(before + separator + text)
        result["automatic_sequence_observed"] = result["decode_error"] is None
    except ValueError:
        pass
    return result


def pretrace_source(window, real, reference, bundle):
    request = read_json(window/"media-request.json")
    require(request["action"] == "trace-trial-b" and request["simulation"] == (not real),
            "Recovery requires the matching diagnostic window")
    media = window/"media"
    path = media/"before.bin"
    before = path.read_bytes()
    verify_before(before,reference,bundle)
    events = [json.loads(row) for row in (media/"media-events.jsonl").read_text().splitlines()]
    archived = [e for e in events if e.get("pretrace_archived")]
    require(len(archived)==1 and archived[0]["before_sha256"]==sha(before) and
            (media/"original-b-4MiB.bin").read_bytes()==before[B_START:B_END] and
            archived[0]["b_sha256"]==sha(before[B_START:B_END]), "Missing same-window archives")
    return path


def signed_file(archive):
    with ZipFile(archive) as package:
        matches = [item for item in package.infolist() if item.filename == FILE]
        require(len(matches) == 1 and matches[0].file_size == 354 + IMAGE_BYTES, "Missing or ambiguous 05 fixture")
        wire = package.read(matches[0])
    require(sha(wire) == PACKAGE_SHA, "05 differs from the accepted signed release")
    return wire


def trial_boots(raw):
    lines = uart_lines(raw)
    markers = [ADVERTISING, START, END_OK, INSTALL, COMMIT]
    position = 0
    for marker in markers:
        require(marker in lines[position:], "Missing or out-of-order trial marker: " + marker.decode())
        position = lines.index(marker, position) + 1
    boots = [i for i in range(position,len(lines)) if lines[i] == b"SFBL"]
    require(len(boots) == 2, "Expected a trial boot and an independent return boot after commit")
    trial, returned = lines[boots[0]+1:boots[1]], lines[boots[1]+1:]
    for section, expected, other in ((trial,B_TOUCH,A_TOUCH),(returned,A_TOUCH,B_TOUCH)):
        require(section.count(expected) == 1 and other not in section, "Product build marker differs")
    require(any(line.startswith(PRODUCT) for line in returned[returned.index(A_TOUCH)+1:]), "Original A storage return missing")
    return {"commit":COMMIT.decode(), "trial_probe":"12027edd", "return_probe":"12027f35"}


def passive_boots(media):
    raw = (media/"startup-uart.bin").read_bytes()
    verify_passive_wire(media, raw)
    result = trial_boots(raw)
    return result


def verify_passive_wire(media, raw):
    rx = (media/"rx.bin").read_bytes()
    require(raw and rx.endswith(raw), "Boot transcript differs from physical RX")
    begin = len(rx) - len(raw)
    received = 0
    for line in (media/"wire-events.jsonl").read_bytes().splitlines():
        event = json.loads(line)
        if received > begin:
            require(event["kind"] not in ("tx_intent","tx","rts_intent","rts_result","rts_pulse_intent","clear_buffers"),
                    "Host intervened during passive boot observation")
        if event["kind"] == "rx":
            require(event["offset"] == received, "RX capture gap")
            received += event["bytes"]
    require(received == len(rx), "Incomplete RX events")


class TrialObservation(PhoneObservation):
    def __init__(self, media, console, *, handoff=False, check=TRACE_CHECK, **options):
        super().__init__(media,console,**options)
        self.last_reads = Path(media)/"before-00e80000.bin"
        self.handoff = handoff
        self.check = check
        self.ready_answered = False
        if check.get("handoff_trace") == 5:
            self.last_reads = Path(media)/"key-005a0000.bin"

    def poll(self):
        directory = self.uart.parent
        if self.check.get("handoff_trace")==5 and not self.ready_answered and (directory/"await-phone").exists():
            self.ready_answered = True
            try:
                self.console.prompt("phone_ready","手机已停在 SIFLI BLE 扫描列表时点“确认并继续”，随即开始广播。",150)
                (directory/"phone-ready").touch(exist_ok=False)
                self.console.stage("phone_ready","complete")
                self.prepared = True
            except TimeoutError:
                (directory/"phone-unavailable").touch(exist_ok=False)
        state = super().poll()
        if not state or self.finished or not self.announced or not state["install_started"]:
            return
        if self.invites_open:
            self.invites_open = False
            self.console.stage("phone","waiting")
        try:
            raw = self.uart.read_bytes()
            if self.handoff:
                if not handoff_report(raw,self.check).get("automatic_sequence_observed"):
                    return
            else:
                trial_boots(raw)
        except ValueError:
            return
        self.uart.with_name("finish-listening").touch(exist_ok=True)
        self.finished = True


def firmware_ranges_match(actual, expected):
    require(len(actual) == len(expected) == 0x1000000, "Expected a full 16 MiB readback")
    # FS, BLE KV and settings KV can legitimately change when either Product runs.
    for start, end in ((0,0x9a0000),(META,META+2*UNIT),(0xdac000,0x1000000)):
        require(actual[start:end] == expected[start:end], f"Unexpected NOR data in {start:#x}..{end:#x}")


def verify_before(before, reference, bundle):
    require(sha(reference) == "d862c6aab38f33490519c70be2583fc56cc5d2bc9c465543efe6aa41ab8d2258",
            "Unexpected original installation reference")
    expected = bytearray(reference)
    for role,(off,n),digest in (
            ("boot",BOOT,"a7452af386289216c3357d61e329cf83e709cd76c2866c2940a4dcfbecbb520c"),
            ("receiver",RECEIVER,"7f52f7482b5e4ae2fe81bf354ede5ad58c71c4a47fca8e94057071f29af755d4")):
        data = (bundle/(role+".bin")).read_bytes()
        require(len(data) == n and sha(data) == digest, "Installed pair differs")
        expected[off:off+n] = data
    firmware_ranges_match(before,expected)


def verify_after(before, after, package):
    require(len(before) == 0x1000000 and sha(package) == PACKAGE_SHA, "Unknown trial input")
    expected = bytearray(before)
    header, body = package[18:354], package[354:]
    erase_bytes = (len(body)+4095)//4096*4096
    expected[B_START:B_START+erase_bytes] = body.ljust(erase_bytes,b"\xff")
    expected[B_END-4096:B_END] = (b"\xff"*(4096-len(header))) + header
    initial = before[META:META+128]
    require(struct.unpack_from("<I3B",initial,8) == (1,0,0,255), "Trial must begin with the original stable A")
    for bank, generation, phase, pending in ((0,3,2,1),(1,4,0,255)):
        record = bytearray(initial)
        struct.pack_into("<I3B",record,8,generation,0,phase,pending)
        record[56:96] = header[40:80]
        struct.pack_into("<I",record,124,zlib.crc32(record[:124]))
        start = META+bank*UNIT
        expected[start:start+UNIT] = record.ljust(UNIT,b"\xff")
    firmware_ranges_match(after,expected)
    changed = [hex(off) for off in range(0,len(before),4096)
               if expected[off:off+4096] != after[off:off+4096]]
    return {"b_sha256":sha(after[B_START:B_END]),"journal":["generation 3 TRIED B","generation 4 STABLE A"],
            "changed_user_pages":changed}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("archive","bundle","before","after","media"):
        parser.add_argument("--"+name,type=Path,required=True)
    args = parser.parse_args()
    first = read_json(ROOT/"docs/evidence/2026-11-19/ota-receiver-first-install.json")
    reference = (ROOT/first["capture"]/"media/after.bin").read_bytes()
    before = args.before.read_bytes()
    verify_before(before,reference,args.bundle)
    print(json.dumps(dict(boots=passive_boots(args.media),
                          flash=verify_after(before,args.after.read_bytes(),signed_file(args.archive))),indent=2))
