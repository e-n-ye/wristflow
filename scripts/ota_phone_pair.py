"""Observe two manually selected Android fixtures during one read-only receiver entry."""
from contextlib import contextmanager
from pathlib import Path
import re
import subprocess
import time

from ota_first_install import require
from ota_phone_board import (FILE_BYTES, SIGNATURE_FILE, FIRST_DATA_REJECTED, START,
                             PhoneObservation, verified_member, verify_fixture)

BODY_FILE = "04-bad-body.bin"
BODY_AT_100 = BODY_FILE + "，已看到 pro100"
BODY_SHA = "675453cdf33b7c7bcb8befeab42adadc90acdef40f8ede672fb36b041b6a1fec"
BLOCKS = (FILE_BYTES + 2047) // 2048
ACTIVE_SECONDS = 1440
LISTEN_SECONDS = ACTIVE_SECONDS + 180
BODY_MIN_SECONDS = 1080
START_LINE, REJECT_LINE = START.decode(), FIRST_DATA_REJECTED.decode()
IDLE_END = "[ota-radio] request=42 result=4 accepted=0 sent=10"
BODY_END = "[ota-radio] request=42 result=1 accepted=0 sent=10"
RETRY = re.compile(r"\[ota-radio\] request=40 result=(1|13) accepted=(\d+) sent=16")
PACKET = re.compile(r"offline packet response (\d+), retrans (\d+), completed (\d+)")


def pair_files(archive):
    return [verify_fixture(archive, SIGNATURE_FILE), verified_member(archive, BODY_FILE, BODY_SHA)]


def uart_receipts(state):
    replies = state["replies"]
    signature = replies[:2] == [START_LINE, REJECT_LINE]
    tail = replies[2:]
    if tail[:1] == [IDLE_END]:
        tail = tail[1:]
    retries = []
    body = signature and tail[:1] == [START_LINE] and tail[-1:] == [BODY_END]
    if body:
        for line in tail[1:-1]:
            match = RETRY.fullmatch(line)
            if not match or int(match[2]) >= BLOCKS:
                body = False
                break
            retries.append([int(match[1]), int(match[2])])
    return dict(signature_rejected=signature, body_end_rejected=body, body_retries=retries)


class PairedObservation(PhoneObservation):
    def __init__(self, *args, android_log, collector, **kwargs):
        super().__init__(*args, **kwargs)
        self.android_log = android_log
        self.collector = collector
        self.latest = None
        self.active_until = None
        self.body_announced = False

    def poll(self):
        state = super().poll()
        if self.active_until is None and self.uart.exists():
            self.active_until = time.monotonic() + ACTIVE_SECONDS
        if state:
            self.latest = state
        state = self.latest
        if not state or not self.invites_open:
            return
        remaining = self.active_until - time.monotonic()
        second_start = state["replies"].count(START_LINE) >= 2
        timed_out = remaining <= 0 or (not second_start and remaining < BODY_MIN_SECONDS)
        capture_lost = self.collector.poll() is not None
        receipts = uart_receipts(state)
        if state["idle_return"] or state["install_started"] or timed_out or capture_lost or receipts["body_end_rejected"]:
            self.invites_open = False
            self.console.stage("phone", "waiting" if self.body_announced else "error")
            # Only the joined phone/board report can confirm that 04 reached its final block.
            self.console.stage("body", "waiting" if receipts["body_end_rejected"] else "error")
            if (timed_out or capture_lost) and not state["idle_return"] and not receipts["body_end_rejected"]:
                self.console.stage("disconnect", "running")
                print("Phone window ended. If still transferring, disconnect and wait for the original watch face.", flush=True)
            return
        if not self.announced or self.body_announced:
            return
        if state["replies"] in ([START_LINE, REJECT_LINE], [START_LINE, REJECT_LINE, IDLE_END]):
            raw = self.android_log.read_text(encoding="utf-8", errors="replace")
            if not android_receipts(raw, self.expected_address)["signature_rejected"]:
                return
            self.console.stage("phone", "waiting")
            self.console.stage("body", "running")
            self.body_announced = True
            print("Signature fixture refused. Save its result, then select " + BODY_FILE + " once.", flush=True)


def android_receipts(raw, address):
    """Use the peer and START of each DFU run. An SDK exit code is not a file receipt."""
    sessions = []
    for line in raw.splitlines():
        _, tag, msg = line.partition("sifli-DFU: ")
        if not tag:
            continue
        if msg.startswith("start with "):
            sessions.append([])
        if sessions:
            sessions[-1].append(msg)
    reports = []
    for messages in sessions:
        peer = [m.removeprefix("connecting to ").upper() for m in messages if m.startswith("connecting to ")]
        start = f"send offline start req, file size {FILE_BYTES}, file count {BLOCKS}"
        ack = "receive offline start response 0, freq 1"
        start_ok = (peer == [address] and messages.count(start) == messages.count(ack) == 1
                    and messages.index(start) < messages.index(ack))
        progress, ack_rows, retries, failures = 0, 0, [], []
        ordered, end_requested = True, False
        end_received, end_responses = False, []
        for msg in messages[messages.index(ack) + 1:] if start_ok else ():
            if msg == "sendDfuImageEndReq":
                end_requested = True
            elif match := re.fullmatch(r"(?:receive )?offline (?:image )?end response(?: (\d+))?", msg):
                if end_received or not end_requested:
                    ordered = False
                end_received = True
                if match[1] is not None:
                    end_responses.append(int(match[1]))
            elif msg.startswith("offline packet response "):
                match = PACKET.fullmatch(msg)
                if not match or end_requested:
                    ordered = False
                    continue
                result, retry, count = map(int, match.groups())
                # The receiver reports its cumulative sequential count. A missing
                # intermediate Android log row does not undo a later ACK.
                if result == retry == 0 and progress < count <= BLOCKS and not failures:
                    progress = count
                    ack_rows += 1
                elif retry == 1 and result in (1, 13) and progress <= count < BLOCKS and not failures:
                    progress = count
                    retries.append([result, count])
                else:
                    failures.append([result, retry, count])
        last_completed = progress if ack_rows or retries or failures else None
        reports.append(dict(start_ok=start_ok, ordered=ordered, completed=last_completed, success_ack_rows=ack_rows, retries=retries,
                            failures=failures, end_requested=end_requested, end_received=end_received,
                            end_responses=end_responses, sdk=messages[0].removeprefix("start with "),
                            exit_errors=[m for m in messages if m.startswith("handle exit ") and m != "handle exit 0"]))
    signature = body = False
    if reports:
        a = reports[0]
        signature = (a["start_ok"] and a["ordered"] and a["completed"] == 0 and not a["retries"]
                     and a["failures"] == [[1, 0, 0]])
    if len(reports) == 2:
        b = reports[1]
        body = (b["start_ok"] and b["ordered"] and b["completed"] == BLOCKS and not b["failures"]
                and b["end_requested"] and b["end_responses"] in ([], [1]))
    return dict(sessions=reports, signature_rejected=signature, body_all_acked=body)


def body_completion(android, saw_pro100):
    if android["body_all_acked"]:
        return "cumulative DATA acknowledgment"
    if len(android["sessions"]) == 2 and saw_pro100:
        body = android["sessions"][1]
        # 1.1.38 can report successful progress only on screen. Preserve an
        # explicit missing DATA count and corroborate pro100 with its END receipt.
        if (body["sdk"] == "1.1.38" and body["start_ok"] and body["ordered"] and body["success_ack_rows"] == 0
                and not body["failures"] and not body["exit_errors"] and body["end_requested"] and body["end_received"]
                and body["end_responses"] in ([], [1])):
            return "operator pro100 and Android END receipt"
    return None


@contextmanager
def android_capture(adb, directory):
    """Read only the USB app's DFU tag from the current phone time through the watch window."""
    command = [str(Path(adb).resolve()), "-d", "shell"]
    def query(cmd):
        return subprocess.check_output([*command, cmd], timeout=12, encoding="utf-8").strip()
    uid = re.fullmatch(r"package:com\.sifli\.siflible uid:(\d+)", query("pm list packages -U com.sifli.siflible"))
    stamp = query("date '+%m-%d %H:%M:%S'")
    require(uid and re.fullmatch(r"\d\d-\d\d \d\d:\d\d:\d\d", stamp), "USB phone package or clock unavailable")
    fields = ("start with |connecting to |find serial trans UUID|send offline start req|"
              "receive offline start response|offline packet response|sendDfuImageEndReq|"
              "(receive )?offline (image )?end response|end with |handle exit |release$")
    logcat = f"logcat -b main -v threadtime -T '{stamp}.000' --uid={uid[1]} -e '^({fields})' 'sifli-DFU:V' '*:S'"
    with (directory/"android-dfu.log").open("xb") as out, (directory/"android-logcat.stderr").open("xb") as err:
        proc = subprocess.Popen([*command, logcat], stdout=out, stderr=err)
        try:
            try:
                proc.wait(timeout=0.3)
            except subprocess.TimeoutExpired:
                pass
            require(proc.poll() is None, "Android DFU log stream unavailable; see android-logcat.stderr")
            yield proc
        finally:
            if proc.poll() is None:
                proc.terminate()
                try:
                    proc.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait(timeout=3)
