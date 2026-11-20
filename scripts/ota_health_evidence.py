"""Correlate 06 Product boots, live settings values and the interrupted phone session."""
import re
import struct
import zlib
import json
from datetime import datetime
from pathlib import Path

from ota_first_install import read_json, require, save, sha
from lab_evidence import write_json
from ota_phone_board import ADVERTISING, IDLE, PRODUCT, START, confirmed_identity
from ota_phone_pair import BLOCKS, android_receipts
from ota_rollback_evidence import A_TOUCH, INSTALL, uart_lines

ADDRESS = "E9:23:73:48:DA:73"
SCOPE = "interrupt-manual-06-return-a-retransmit-confirm-b-compare-settings-and-records"
END = b"[ota-radio] request=42 result=0 accepted=1482 sent=10"
COMMIT = b"[ota-ready] release=2026111802 bytes=3033572 trial committed"
TRIAL = b"[ota-product] slot=B release=2026111802 trial=1 services=1"
STABLE = b"[ota-product] slot=B release=2026111802 trial=0 services=1"
STABLE_DISPLAY_A = b"[ota-product] slot=A release=2026112001 trial=0 services=1"
STABLE_DISPLAY_A_UART_SHA256 = "d1265499b1b49c6d2bba4cc1a572640439fa2a03f83621a13acef5637275a041"
HANDOFF = re.compile(rb"\[ota-product\] handoff_intent=healthy saved_generation=(\d+)")


def saved_settings(data):
    """Return the live preferences, layout and alarm-definition values, independent of GC offsets."""
    require(len(data) == 16384, "Incomplete settings partition")
    values = {}
    for start in (0, 8192):
        sector = data[start:start+8192]
        require(sector[0] in (0x7f, 0x3f, 0x1f) and sector[1] in (0x7f, 0x3f) and
                sector[2:16] == b"\xff\xffFDB0" + b"\xff"*8, "Unknown or unfinished settings sector")
        off = 16
        if sector[0] == 0x7f:
            require(sector[off:] == b"\xff"*(len(sector)-off), "Nonempty unused settings sector")
            continue
        while off < len(sector) and sector[off] != 255:
            h = sector[off:off+24]
            require(len(h) == 24 and h[0] in (0x3f, 0x0f) and h[1:8] == b"\xff\xff\xffKV00" and
                    h[17:20] == b"\xff"*3 and 0 < h[16] <= 64, "Invalid or pending settings record")
            size, crc = struct.unpack_from("<II", h, 8)
            n = h[16]
            require(size == 24+n+int.from_bytes(h[20:], "little") and size <= len(sector)-off,
                    "Settings record exceeds sector")
            payload = sector[off+24:off+size]
            require(zlib.crc32(h[16:]+payload) == crc, "Invalid settings record CRC")
            if h[0] == 0x3f and payload[:n] in (b"preferences", b"layout_a", b"layout_b", b"alarm_a", b"alarm_b", b"alarm_ca", b"alarm_cb"):
                key = payload[:n].decode()
                require(key not in values, "Ambiguous live setting")
                values[key] = payload[n:]
            off += size
        require(sector[off:] == b"\xff"*(len(sector)-off), "Unparsed settings tail")
    require("preferences" in values and ("layout_a" in values or "layout_b" in values), "Missing saved preferences or layout")
    # alarm_store.c publishes alternating payload and commit-marker pairs.
    # Boot can republish the same alarm data with a new generation and CRC.
    published = []
    for suffix in ("a", "b"):
        alarm = values.pop("alarm_"+suffix, None)
        marker = values.pop("alarm_c"+suffix, None)
        if alarm is None or marker is None:
            continue
        require(len(alarm) == len(marker) == 104 and alarm[:5] == b"WFAL\x01" and marker[:5] == b"WFAC\x01"
                and all(zlib.crc32(r[:-4]) == int.from_bytes(r[-4:], "little") for r in (alarm,marker)),
                "Invalid stored alarm or publication CRC")
        if alarm[8:12] == marker[8:12] and alarm[-4:] == marker[12:16]:
            published.append((int.from_bytes(alarm[8:12],"little"), alarm[:8]+alarm[12:-4]))
    if published:
        generation, data = published[0]
        for other, content in published[1:]:
            require(other != generation or content == data, "Ambiguous published alarm generation")
            if 0 < (other-generation) % (1 << 32) < (1 << 31):
                generation, data = other, content
        values["published_alarms"] = data
    return {key:sha(value) for key,value in values.items()}


def restored_values(lines):
    storage = [m.groups() for line in lines if (m := re.fullmatch(
        re.escape(PRODUCT)+rb"brightness=(\d+) face=(\S+) breathing=(\d+) rtc=\S+", line))]
    layout = [m.groups() for line in lines if (m := re.fullmatch(
        rb"\[product\] layout restore=yes generation=(\d+) pages=(\d+)", line))]
    require(len(storage) == len(layout) == 1, "Missing unambiguous restored settings")
    return [v.decode() for v in storage[0]+layout[0]]


def boot_result(raw, stage, *, stable_slot="B"):
    lines = uart_lines(raw)
    if stage == "ordinary":
        stable = {"A": STABLE_DISPLAY_A, "B": STABLE}[stable_slot]
        require(lines.count(b"SFBL") == 1 and ADVERTISING not in lines, "Unexpected ordinary restart")
        lines = lines[lines.index(b"SFBL")+1:]
        require([s for s in lines if s.startswith(b"[ota-product]")] == [stable],
                f"Ordinary reboot did not run the expected stable slot {stable_slot}")
        return dict(slot=stable_slot, settings=restored_values(lines))
    # A disconnect can restart advertising without rebooting the receiver.
    require(confirmed_identity(raw, None, ADDRESS) == ADDRESS and ADVERTISING in lines, "Receiver identity mismatch")
    session = lines[lines.index(ADVERTISING)+1:]
    replies = [s for s in session if s.startswith(b"[ota-radio] request=")]
    if stage == "partial":
        require(replies == [START] and session.count(IDLE) == 1 and session.index(START) < session.index(IDLE)
                and INSTALL not in session and not any(b"[ota-ready]" in s for s in session), "Transfer ended or began installing")
        tail = session[session.index(IDLE)+1:]
        require(tail.count(b"SFBL") == 1 and A_TOUCH in tail[tail.index(b"SFBL")+1:] and STABLE not in tail,
                "Stock A did not return after disconnect")
        return dict(slot="A", settings=restored_values(tail))
    require(stage == "complete" and replies[0:1] == [START] and replies[-1:] == [END] and
            all(re.fullmatch(rb"\[ota-radio\] request=40 result=(?:1|13) accepted=\d+ sent=16", s) for s in replies[1:-1]),
            "Missing successful 06 transfer")
    require(session.count(INSTALL) == session.count(COMMIT) == 1 and session.index(END) < session.index(INSTALL) < session.index(COMMIT),
            "Missing signed 06 install commitment")
    tail = session[session.index(COMMIT)+1:]
    boots = [i for i, line in enumerate(tail) if line == b"SFBL"]
    require(len(boots) == 2, "Expected autonomous trial and stable boots")
    trial, stable = tail[boots[0]+1:boots[1]], tail[boots[1]+1:]
    markers = [s for s in trial if s.startswith(b"[ota-product]")]
    require(len(markers) == 2 and markers[0] == TRIAL and HANDOFF.fullmatch(markers[1]) and
            [s for s in stable if s.startswith(b"[ota-product]")] == [STABLE], "06 health confirmation or stable boot missing")
    settings = restored_values(stable)
    require(restored_values(trial) == settings, "Settings differ between trial and stable boots")
    return dict(slot="B", settings=settings, handoff_generation=int(HANDOFF.fullmatch(markers[1])[1]))


def display_a_reference(raw):
    require(sha(raw) == STABLE_DISPLAY_A_UART_SHA256, "Stable A UART evidence changed")
    lines = uart_lines(raw)
    last_boot = max(i for i, line in enumerate(lines) if line == b"SFBL")
    return boot_result(b"\n".join(lines[last_boot:]) + b"\n", "ordinary", stable_slot="A")


def partial_phone(raw):
    sessions = android_receipts(raw, ADDRESS)["sessions"]
    require(len(sessions) == 1, "Expected one phone session for partial 06")
    s = sessions[0]
    require(s["start_ok"] and s["ordered"] and not s["failures"] and not s["end_requested"]
            and not s["end_received"] and (s["completed"] is None or 0 < s["completed"] < BLOCKS),
            "Android capture is not a partial transfer")
    return s


def image_receipt(raw, image, percent, start_time):
    """Bind a reviewed progress image to this Android session by its visible selection time."""
    partial_phone(raw)
    stamps = re.findall(r"(?m)^\s*\d\d-\d\d (\d\d:\d\d:\d\d)\.\d+ .*?sifli-DFU: start with ", raw)
    require(len(stamps) == 1 and type(percent) is int and 0 < percent < 100, "Missing Android start time or partial progress")
    def seconds(value):
        t = datetime.strptime(value, "%H:%M:%S")
        return t.hour*3600+t.minute*60+t.second
    difference = (seconds(stamps[0])-seconds(start_time)+43200) % 86400-43200
    require(abs(difference) <= 3, "Image selection time differs from the current DFU session")
    require(image.startswith((b"\xff\xd8\xff", b"\x89PNG\r\n\x1a\n")), "Expected the original phone screenshot")
    return dict(percent=percent, selection_time=start_time, sdk_start=stamps[0], sha256=sha(image))


def wire_image_receipt(media, image, percent, selection, discovery):
    """Use raw board START time when Android returned no tagged log rows."""
    raw = (media/"startup-uart.bin").read_bytes()
    boot_result(raw, "partial")
    rx = (media/"rx.bin").read_bytes()
    require(rx.endswith(raw) and raw.count(START) == 1, "Ambiguous recorded receiver START")
    offset = len(rx)-len(raw)+raw.index(START)
    events = (media/"wire-events.jsonl").read_bytes().splitlines()
    matches = [e for line in events if (e := json.loads(line))["kind"] == "rx"
               and e["offset"] <= offset < e["offset"]+e["bytes"]]
    require(len(matches) == 1 and 0 < percent < 100 and type(percent) is int and
            image.startswith((b"\xff\xd8\xff", b"\x89PNG\r\n\x1a\n")), "Missing screenshot or matching RX")
    trace = media/"wire-events.jsonl"
    created = trace.stat().st_birthtime
    t = datetime.fromtimestamp(created+matches[0]["elapsed_ns"]/1e9)
    require(isinstance(discovery, str), "Read the Services discovered time from this screenshot")
    visible = datetime.strptime(discovery, "%H:%M:%S")
    selected = datetime.strptime(selection, "%H:%M:%S")
    delta = (t.hour-visible.hour)*3600+(t.minute-visible.minute)*60+t.second-visible.second
    require(abs((delta+43200)%86400-43200) <= 4 and 0 <= (visible-selected).seconds <= 30,
            "Visible service discovery does not match this board START")
    return dict(percent=percent, selection_time=selection, discovery_time=discovery,
                board_start=t.isoformat(), sha256=sha(image), source="reviewed screenshot and board START")


def record_image(root, source, percent, start_time, discovery=None):
    raw = (root/"partial/android-dfu.log").read_text(encoding="utf-8", errors="replace")
    require(read_json(root/"binding.json")["authorization"] == SCOPE, "Wrong 06 window")
    data = source.read_bytes()
    receipt = (image_receipt(raw, data, percent, start_time) if "sifli-DFU: " in raw else
               wire_image_receipt(root/"partial/media", data, percent, start_time, discovery))
    target = root/"partial-progress.image"
    if target.exists():
        require(target.read_bytes() == data, "Already archived a different screenshot")
    else:
        save(target, data)
    write_json(root/"partial-progress.json", receipt)


if __name__ == "__main__":
    import argparse
    p = argparse.ArgumentParser(description="Record the percentage and selection time read from this 06 screenshot")
    p.add_argument("--window", type=Path, required=True)
    p.add_argument("--image", type=Path, required=True)
    p.add_argument("--percent", type=int, required=True)
    p.add_argument("--selection-time", required=True)
    p.add_argument("--discovery-time", help="Visible Services discovered time when no Android DFU rows were captured")
    a = p.parse_args()
    record_image(a.window, a.image, a.percent, a.selection_time, a.discovery_time)
