"""Pure cached automatic-brightness status parsing."""
import re
from lab_serial import ProtocolError

FIELDS = ("enabled", "active", "valid", "fault", "cleanup", "brightness", "mlux", "ms", "session", "seq")
PATTERN = re.compile(r"\[auto-light\] enabled=([01]) active=([01]) valid=([01]) fault=([01]) cleanup=([01]) brightness=(\d+) mlux=(\d+) ms=(\d+) session=(\d+) seq=(\d+); uncalibrated(?:\r?\n|$)")


def states(text):
    result = [dict(zip(FIELDS, map(int, match.groups()))) for match in PATTERN.finditer(text)]
    if any(row["brightness"] > 100 or any(row[k] > 0xffffffff for k in ("mlux", "ms", "session", "seq")) for row in result):
        raise ProtocolError("Automatic brightness status outside field range")
    return result


def active(text):
    rows = states(text)
    if not rows:
        raise ProtocolError("Complete automatic-brightness status missing")
    value = rows[-1]
    if not (value["enabled"] and value["active"] and value["valid"] and not value["fault"] and not value["cleanup"]):
        raise ProtocolError("Automatic brightness not active/valid")
    return value
