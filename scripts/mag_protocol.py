"""Pure MMC5603 diagnostic text parsing; no I/O or experimental policy."""
from __future__ import annotations

import re

from lab_serial import ProtocolError

HEX = r"[0-9a-fA-F]{2}"
STATE = re.compile(r"\[mag\] session=(\d+) phase=(\d+) end=(\d+) id=("+HEX+r") identity_valid=([01]) owned=([01]) stopped=([01]) errors=(\d+)\r?\n")
MODE = re.compile(r"\[mag-mode\] session=(\d+) selftest_only=([01]) threshold=(raw80|encoded) experimental=([01])\r?\n")
NATIVE = re.compile(r"\[mag\] selftest_valid=([01]) passed=([01]) status=("+HEX+r") factory=((?:"+HEX+r"/){2}"+HEX+r") threshold=((?:"+HEX+r"/){2}"+HEX+r"); uncalibrated counts, no heading\r?\n")
SUMMARY = re.compile(r"\[mag\] start=(\d+) duration=(\d+) ms=(\d+) seq=(\d+) status=("+HEX+r") after=("+HEX+r") xyz=(-?\d+)/(-?\d+)/(-?\d+) valid=([01]) saturated=([01])\r?\n")
FRAME = re.compile(r"\[mag-data\] session=(\d+) seq=(\d+) ms=(\d+) status=("+HEX+r") xyz=(-?\d+)/(-?\d+)/(-?\d+) valid=([01]) saturated=([01]); raw counts\r?\n")


def cached(text):
    states = list(STATE.finditer(text))
    if not states:
        raise ProtocolError("Complete cached state missing")
    match = states[-1]
    section = text[match.end():]
    values = [int(v, 16) if i == 3 else int(v) for i, v in enumerate(match.groups())]
    state = dict(zip(("session", "phase", "end", "id", "identity_valid", "owned", "stopped", "errors"), values))
    modes, results, samples = list(MODE.finditer(section)), list(NATIVE.finditer(section)), list(SUMMARY.finditer(section))
    if len(modes) != 1 or len(results) != 1 or len(samples) != 1 or int(modes[0][1]) != state["session"]:
        raise ProtocolError("Incomplete, duplicated or mixed cached response")
    mode, native, sample = modes[0], results[0], samples[0]
    state.update(selftest_only=bool(int(mode[2])), mode=mode[3], experimental=bool(int(mode[4])),
                 native_valid=bool(int(native[1])), passed=bool(int(native[2])), status=int(native[3], 16),
                 factory=[int(v, 16) for v in native[4].split("/")], threshold=[int(v, 16) for v in native[5].split("/")],
                 start_ms=int(sample[1]), duration_ms=int(sample[2]), sample_ms=int(sample[3]),
                 samples=int(sample[4]), after_status=int(sample[6], 16), sample_valid=bool(int(sample[10])))
    if any(state[k] > 0xffffffff for k in ("session", "errors", "start_ms", "duration_ms", "sample_ms", "samples")):
        raise ProtocolError("Cached integer outside uint32 range")
    if state["phase"] not in range(4) or state["end"] not in range(9):
        raise ProtocolError("Unknown diagnostic state")
    return state


def frames(text, session):
    result = []
    for match in FRAME.finditer(text):
        if int(match[1]) != session:
            continue
        values = [int(v, 16) if i == 3 else int(v) for i, v in enumerate(match.groups())]
        row = dict(zip(("session", "seq", "ms", "status", "x", "y", "z", "valid", "saturated"), values))
        if any(row[k] > 0xffffffff for k in ("session", "seq", "ms")):
            raise ProtocolError("Frame integer outside uint32 range")
        if any(not -524288 <= row[k] <= 524287 for k in ("x", "y", "z")):
            raise ProtocolError("Frame counts outside centered 20-bit range")
        result.append(row)  # Invalid and seq0 records remain visible to the adapter.
    return result


def identity(state):
    return tuple(state[k] for k in ("session", "start_ms", "duration_ms", "mode", "selftest_only"))


def ordinary(text, state):
    """Readiness policy: recognize one exact reset notice before self-test only."""
    session = state["session"]
    matches = [m for m in FRAME.finditer(text) if int(m[1]) == session]
    rows = frames(text, session)
    reset = dict(session=session, seq=0, ms=0, status=0, x=0, y=0, z=0, valid=0, saturated=0)
    excluded = False
    if rows and rows[0] == reset:
        native = text.find(f"[mag-selftest] session={session} ")
        if len(matches) < 2 or not matches[0].end() <= native < matches[1].start():
            raise ProtocolError("Reset notification lacks pre-selftest provenance")
        rows = rows[1:]
        excluded = True
    if len(rows) != state["samples"] or [r["seq"] for r in rows] != list(range(1, len(rows)+1)):
        raise ProtocolError("Ordinary frame gap/duplicate/count mismatch")
    previous = None
    for row in rows:
        elapsed = (row["ms"]-state["start_ms"]) & 0xffffffff
        if not row["valid"] or row["saturated"] or row["status"] & 0x70 != 0x50:
            raise ProtocolError("Invalid or saturated ordinary frame")
        if not 0 < elapsed < state["duration_ms"] or previous is not None and elapsed <= previous:
            raise ProtocolError("Frame time outside ordered session window")
        previous = elapsed
    return rows, excluded
