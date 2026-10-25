"""Parse complete foreground-compass cache pairs; no north/physical claims."""
import re
from lab_serial import ProtocolError

STATUS = re.compile(r"\[compass\] requested=([01]) active=([01]) valid=([01]) fault=([01]) cleanup=([01]) generation=(\d+) session=(\d+) seq=(\d+) ms=(\d+) heading=(\d+) reason=(\d+)\r?\n")
RAW = re.compile(r"\[compass-raw\] xyz=(-?\d+)/(-?\d+)/(-?\d+) selftest=([01]) stopped=([01]) errors=(\d+); uncalibrated XY, mounting zero/tilt unverified(?:\r?\n|$)")
KEYS = ("requested", "active", "valid", "fault", "cleanup", "generation", "session", "seq", "ms", "heading", "reason")


def snapshots(text):
    """Keep invalid cache values; never join a broken pair across other lines."""
    result = []
    for match in STATUS.finditer(text):
        raw = RAW.match(text, match.end())
        if raw is None:
            continue  # The raw bytes stay available; an incomplete pair is not a snapshot.
        value = dict(zip(KEYS, map(int, match.groups())))
        value.update(zip(("x", "y", "z", "selftest", "stopped", "errors"), map(int, raw.groups())))
        if (any(value[k] > 0xffffffff for k in ("generation", "session", "seq", "ms", "errors")) or
                value["heading"] >= 360 or value["reason"] > 5 or
                any(not -524288 <= value[k] <= 524287 for k in ("x", "y", "z"))):
            raise ProtocolError("Compass cache field outside protocol range")
        result.append(value)
    return result
