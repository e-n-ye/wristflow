"""Decode one complete bounded wf_record snapshot from saved UART; no device I/O."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib

SCENES = ("unknown", "on", "flashlight", "dim", "off", "aod")
MIN_UTC, MAX_UTC = 1767225600, 4102444799


def require(condition, message):
    if not condition:
        raise ValueError(message)


def snapshot_crc(token, count, saved, failed):
    return zlib.crc32(struct.pack("<QIBB2x", token, count, saved, failed))


def sample_valid(s):
    require(0 <= s["ms"] < 2**64 and 0 <= s["sequence"] < 2**32, "sample identity")
    require(0 <= s["sampled_ms"] < 2**32 and 0 <= s["millivolts"] < 2**16, "sample range")
    require(s["utc"] == 0 or MIN_UTC <= s["utc"] <= MAX_UTC, "sample UTC")
    require(0 <= s["scene"] < 6 and 0 <= s["charge"] <= 5 and 0 <= s["flags"] < 64, "sample context")
    for key in ("percent", "requested", "acknowledged"):
        require(0 <= s[key] <= 255, "sample byte range")
    require(not s["flags"] & 2 or s["percent"] <= 100, "estimate range")
    require(not s["flags"] & 16 or max(s["requested"], s["acknowledged"]) <= 100, "brightness range")


def decode_summary(data):
    require(len(data) == 512 and data[:8] == b"WFRN\x01\x00\x00\x02", "summary format")
    zeroed = data[:28] + bytes(4) + data[32:]
    require(struct.unpack_from("<I", data, 28)[0] == zlib.crc32(zeroed), "summary CRC")
    generation, boot, flags, utc, revision = struct.unpack_from("<5I", data, 8)
    require(boot != 0 and flags & ~7 == 0, "summary flags/boot")
    require(MIN_UTC <= utc <= MAX_UTC if flags & 1 else utc == 0, "summary UTC")
    uptime, total, *durations = struct.unpack_from("<8Q", data, 32)
    require(uptime <= total and min(sum(durations), 2**64 - 1) == total, "duration conservation")
    result = dict(generation=generation, boot=boot, flags=flags, utc=utc, clock_revision=revision,
                  recorded_ms=uptime, total_recorded_ms=total, scene_ms=dict(zip(SCENES, durations)))
    names = ("ms", "sequence", "sampled_ms", "utc", "millivolts", "flags", "percent",
             "requested", "acknowledged", "charge", "scene")
    for label, offset in (("segment_first", 96), ("last", 128)):
        sample = dict(zip(names, struct.unpack_from("<QIIIHH5B", data, offset)))
        sample_valid(sample)
        require(data[offset + 29:offset + 32] == bytes(3), "sample reserved")
        result[label] = sample
    counters = ("fault_mask", "fault_known", "faults", "recoveries", "clock_errors", "event_drops",
                "sample_drops", "samples", "attempts", "successes", "failures", "periodic_saves",
                "explicit_saves", "events_total", "io_retries", "io_recoveries")
    result.update(zip(counters, struct.unpack_from("<16I", data, 160)))
    require(result["fault_mask"] & ~result["fault_known"] == 0, "unknown fault bits")
    mode, raw, saved_uptime, count, segment = struct.unpack_from("<IIQII", data, 224)
    require(count <= 8 and saved_uptime <= uptime and data[248:256] == bytes(8), "summary tail")
    result.update(boot_mode_raw=mode, boot_source_raw=raw, prior_verified_recorded_ms=saved_uptime, segment=segment)
    result["events"] = []
    for i in range(count):
        e = dict(zip(("ms", "utc", "value", "detail", "boot", "clock_revision", "kind", "scene", "flags"),
                     struct.unpack_from("<QIIIIIHBB", data, 256 + 32*i)))
        require(e["boot"] != 0 and 1 <= e["kind"] <= 9 and e["scene"] < 6 and e["flags"] & ~1 == 0, "event fields")
        require(MIN_UTC <= e["utc"] <= MAX_UTC if e["flags"] & 1 else e["utc"] == 0, "event UTC")
        result["events"].append(e)
    require(data[256 + 32*count:] == bytes(32*(8-count)), "unused event bytes")
    return result


def decode_capture(raw, token):
    require(len(raw) <= 1024*1024 and 0 < token < 2**64, "capture bound/token")
    text = raw.decode("utf-8", errors="strict")
    header = None
    blocks, partial, samples, ends = {}, None, {}, {}
    for line in text.splitlines():
        if header is not None and re.search(r"SFBL|RT-Thread|\[product\] storage=", line):
            raise ValueError("boot/reset marker inside export session")
        match = re.search(r"\[record-([a-z-]+)\] (.*)$", line)
        if not match:
            continue
        kind, payload = match.groups()
        fields = {}
        for field in payload.split():
            require("=" in field, "malformed field")
            key, value = field.split("=", 1)
            require(key not in fields, "duplicate field")
            fields[key] = value
        if header is not None and "token" in fields and fields["token"] != str(token):
            raise ValueError("export session changed")
        if "token" not in fields or fields["token"] != str(token):
            continue
        wire_fields = {
            "snapshot": {"schema", "token", "samples", "saved", "failed", "bytes", "crc"},
            "summary": {"token", "kind"}, "data": {"token", "offset", "hex"},
            "summary-end": {"token"}, "sample": {"token", "index", "crc"},
            "sample-data": {"token", "index", "hex"}, "samples-end": {"token", "page", "count"},
        }
        require(kind in wire_fields and set(fields) == wire_fields[kind], "wire fields")
        if kind == "snapshot":
            require(header is None, "reused token/multiple sessions")
            header = {key: int(value) for key, value in fields.items() if key != "crc"}
            require(header.get("schema") == 1 and header.get("bytes") == 512 and 0 <= header.get("samples", -1) <= 64,
                    "snapshot header")
            require(header.get("saved") in (0, 1) and header.get("failed") in (0, 1), "snapshot state")
            require(re.fullmatch("[0-9a-f]{8}", fields["crc"]) and int(fields["crc"], 16) ==
                    snapshot_crc(token, header["samples"], header["saved"], header["failed"]), "snapshot CRC")
        elif kind == "summary":
            label = fields.get("kind")
            require(header is not None and partial is None and label in ("current", "saved") and label not in blocks, "summary start")
            partial = (label, {})
        elif kind == "data":
            require(partial is not None, "orphan data")
            offset = int(fields["offset"])
            require(0 <= offset < 512 and offset % 32 == 0 and offset not in partial[1], "chunk offset")
            require(re.fullmatch("[0-9a-f]{64}", fields.get("hex", "")), "chunk hex")
            partial[1][offset] = bytes.fromhex(fields["hex"])
        elif kind == "summary-end":
            require(partial is not None and len(partial[1]) == 16, "incomplete summary")
            blocks[partial[0]] = decode_summary(b"".join(partial[1][i] for i in range(0, 512, 32)))
            partial = None
        elif kind in ("sample", "sample-data"):
            require(header is not None, "sample before snapshot")
            index = int(fields["index"])
            require(0 <= index < header["samples"], "sample index")
            row = samples.setdefault(index, {})
            require(set(fields)==({"token","index","crc"} if kind=="sample" else {"token","index","hex"}), "sample wire fields")
            for key, value in fields.items():
                if key in ("token", "index"):
                    continue
                require(key not in row, "duplicate sample field")
                row[key] = value
        elif kind == "samples-end":
            page, count = int(fields["page"]), int(fields["count"])
            require(header is not None and page not in ends and 0 <= page < 8, "sample page")
            require(count == min(8, header["samples"] - page*8) and count > 0, "page count")
            ends[page] = count
    require(header is not None and partial is None and "current" in blocks, "missing snapshot/summary")
    require(("saved" in blocks) == bool(header["saved"]), "missing/unexpected saved summary")
    require(set(samples) == set(range(header["samples"])) and set(ends) == set(range((header["samples"]+7)//8)), "incomplete sample pages")
    for index,row in samples.items():
        require(set(row)=={"crc","hex"} and re.fullmatch("[0-9a-f]{8}",row["crc"]) and re.fullmatch("[0-9a-f]{64}",row["hex"]), "sample wire data")
        data=bytes.fromhex(row["hex"])
        require(zlib.crc32(data)==int(row["crc"],16) and data[29:]==bytes(3), "sample CRC/reserved")
        s=dict(zip(("ms","sequence","sampled_ms","utc","millivolts","flags","percent","requested","acknowledged","charge","scene"),
                   struct.unpack_from("<QIIIHH5B",data)))
        sample_valid(s)
        samples[index]=s
    current, saved = blocks["current"], blocks.get("saved")
    require(not saved or (current["generation"]==saved["generation"] and current["total_recorded_ms"]>=saved["total_recorded_ms"]), "summary relationship")
    times=[samples[i]["ms"] for i in sorted(samples)]
    require(times==sorted(times) and (not times or times[-1]<=current["last"]["ms"]), "sample time relationship")
    tail = current["recorded_ms"] - saved["recorded_ms"] if saved and saved["boot"] == current["boot"] else current["recorded_ms"]
    require(tail >= 0, "saved position ahead of current")
    return dict(schema=1, token=token, failed=bool(header["failed"]), current=current, saved=saved,
                unsaved_recorded_ms=tail, samples=[samples[i] for i in sorted(samples)],
                capture_sha256=hashlib.sha256(raw).hexdigest(), hardware_verified=False,
                limits="Saved summaries and bounded recent samples; UTC is cached, recorded time excludes explicit stops. No continuity, power or battery-life claim.")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--input", type=Path, required=True)
    p.add_argument("--token", type=int, required=True)
    p.add_argument("--out", type=Path, required=True)
    args = p.parse_args()
    require(args.input.stat().st_size <= 1024*1024, "capture size bound")
    result = decode_capture(args.input.read_bytes(), args.token)
    args.out.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
