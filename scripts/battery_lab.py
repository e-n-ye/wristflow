"""Offline battery experiment and OpenHTF human/AI workflow rehearsal.

No serial, flashing, device input, or firmware mutation is available here.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import subprocess
import sys
import time
import threading

from battery_console import Cancelled, Console

ROOT = pathlib.Path(__file__).resolve().parents[1]
POLICIES = ("baseline", "retain_30s", "settle_60s", "combined")
OUTPUT_KEYS = ("percent_valid", "percent", "filter", "filter_valid", "candidate", "mode", "decision")
ROW_KEYS = ("session index obs eval seq adc age mv raw_valid display_valid usb_valid usb_good "
            "charge reason cycle mode decision filter filter_valid candidate percent percent_valid "
            "surface phase flags requested ack").split()


def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    path = pathlib.Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def load_trace(path):
    data = json.loads(pathlib.Path(path).read_text(encoding="utf-8"))
    status, rows = data["status"], data["records"]
    if (any(type(status.get(key)) is not int for key in
            ("schema", "session", "state", "count", "cap", "start", "duration", "now")) or
            status["cap"] != 256 or not 0 < status["duration"] <= 900000):
        raise ValueError("Invalid trace status/capacity/duration")
    if status["schema"] != 1 or status["state"] not in (2, 3, 4):
        raise ValueError("Trace must be schema 1 and stopped/expired/full")
    if not 0 < status["session"] or not 0 < len(rows) == status["count"] <= 256:
        raise ValueError("Invalid session or incomplete record count")
    previous = None
    identities = {}
    for index, row in enumerate(rows):
        if any(type(row.get(key)) is not int for key in ROW_KEYS):
            raise ValueError(f"Row {index}: missing or non-integer field")
        if row["index"] != index or row["session"] != status["session"]:
            raise ValueError(f"Row {index}: mixed session or missing/duplicate index")
        if any(value < 0 or value > 0xffffffff for value in row.values()):
            raise ValueError(f"Row {index}: unsigned field out of range")
        if any(row[key] not in (0, 1) for key in
               ("raw_valid", "display_valid", "usb_valid", "usb_good", "filter_valid", "percent_valid")):
            raise ValueError(f"Row {index}: invalid boolean")
        if (row["mv"] > 65535 or row["charge"] > 5 or row["decision"] > 9 or
                row["mode"] not in (0, 1, 2, 255) or row["percent"] > 100 or
                row["candidate"] > 99 or row["requested"] > 100 or row["ack"] > 100):
            raise ValueError(f"Row {index}: invalid enum/value")
        elapsed = (row["obs"] - status["start"]) & 0xffffffff
        if elapsed >= status["duration"] or (previous is not None and elapsed < previous):
            raise ValueError(f"Row {index}: observation outside/behind logical window")
        previous = elapsed
        if row["seq"]:
            if row["age"] != (row["eval"] - row["adc"]) & 0xffffffff:
                raise ValueError(f"Row {index}: ADC age mismatch")
            identity = (row["adc"], row["mv"])
            if row["seq"] in identities and identities[row["seq"]] != identity:
                raise ValueError(f"Row {index}: ADC identity changed")
            identities[row["seq"]] = identity
    return data


def production_compare(fixture, output, cc):
    """Compare current production to a fixed old-device input; never fake a match."""
    fixture, output = pathlib.Path(fixture).resolve(), pathlib.Path(output).resolve()
    data = load_trace(fixture)
    rows = data["records"]
    output.mkdir(parents=True, exist_ok=False)
    historical = replay(fixture, output / "historical", cc)
    engine = output / "battery_current_replay.exe"
    sources = [ROOT / "scripts/battery_current_replay.c", ROOT / "core/battery_estimate.c"]
    command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "core"),
               *map(str, sources), "-o", str(engine)]
    build = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    if build.returncode:
        raise RuntimeError("Current production host compilation failed")
    first = rows[0]
    wire = [f"1 {first['mode']} {first['percent']} {first['seq']} {first['filter']} {first['adc']}"]
    wire += [" ".join(str(row[k]) for k in
             ("index", "eval", "adc", "seq", "mv", "raw_valid", "usb_valid", "usb_good", "charge")) for row in rows[1:]]
    result = subprocess.run([str(engine)], input="\n".join(wire) + "\n", text=True,
                            capture_output=True, check=True, timeout=30)
    (output / "engine-input.txt").write_text("\n".join(wire) + "\n", encoding="utf-8")
    (output / "engine.txt").write_text(result.stdout, encoding="utf-8")
    current = [dict(first)]
    for line in result.stdout.splitlines():
        index, *values = map(int, line.split())
        if index != len(current) or len(values) != len(OUTPUT_KEYS):
            raise ValueError("Incomplete/invalid current engine output")
        current.append(dict(rows[index], **dict(zip(OUTPUT_KEYS, values))))
    if len(current) != len(rows):
        raise ValueError("Incomplete current prediction count")
    for original, actual in zip(rows[1:], current[1:]):
        for key in OUTPUT_KEYS:
            if key == "percent":
                continue
            if key == "decision" and actual[key] == 3 and original[key] in (0, 1, 2):
                continue
            if actual[key] != original[key]:
                raise ValueError(f"Unexpected production change: row {actual['index']} {key}")
    write_json(output / "predictions.json", {"recorded": rows, "current": current})
    report = {"schema": 1, "kind": "current_production_comparison", "hardware_verified": False,
              "fixture_sha256": digest(fixture), "record_count": len(rows), "historical_checked_rows": historical["checked_rows"],
              "build_command": command, "build_exit": build.returncode, "engine_sha256": digest(engine),
              "source_sha256": {str(p.relative_to(ROOT)): digest(p) for p in
                    sources + [ROOT / "core/battery_estimate.h", pathlib.Path(__file__).resolve()]},
              "changed_percent_rows": [{"index": new["index"], "adc": new["adc"], "surface": new["surface"],
                    "candidate": new["candidate"], "recorded": old["percent"], "current": new["percent"]}
                    for old, new in zip(rows, current) if old["percent"] != new["percent"]],
              "unchanged_fields": [k for k in OUTPUT_KEYS if k != "percent"],
              "warm_state_row": 0, "pending_history_at_start": "unknown; starts empty, no claim about preceding dwell",
              "limits": ["Old device data fed into current C; this is not new firmware hardware acceptance.",
                         "Only display behavior changes; SOC and consumed charge were not measured."]}
    write_json(output / "comparison.json", report)
    return report


def replay(fixture, output, cc, *, guarded=None):
    fixture, output = pathlib.Path(fixture).resolve(), pathlib.Path(output).resolve()
    data = load_trace(fixture)
    rows = data["records"]
    output.mkdir(parents=True, exist_ok=False)
    engine = output / "battery_replay.exe"
    baseline = ROOT / "scripts/battery_baseline_20261021"
    sources = [ROOT / "scripts/battery_replay.c", ROOT / "scripts/battery_policy.c", baseline / "battery_estimate.c"]
    command = [cc, "-std=c11", "-Wall", "-Wextra", "-Werror", "-I", str(baseline), "-I", str(ROOT / "core"),
               *map(str, sources), "-o", str(engine)]
    build = subprocess.run(command, capture_output=True, text=True, timeout=60)
    (output / "build.log").write_text(build.stdout + build.stderr, encoding="utf-8")
    if build.returncode:
        raise RuntimeError(f"Host compilation failed: {output / 'build.log'}")
    # Recording began mid-session: row 0 supplies the observable warm state.
    # Its preceding history cannot be independently reconstructed.
    first = rows[0]
    if not first["filter_valid"] or not first["percent_valid"] or first["decision"] != 3:
        raise ValueError("This replay requires a valid cached warm-state first row")
    wire = [f"1 {first['mode']} {first['percent']} {first['seq']} {first['filter']}"]
    policies = POLICIES if guarded is None else ("baseline", "retention", "settling", "combined")
    if guarded is not None:
        if (set(guarded) != {"retain_ms", "settle_ms", "settle_samples", "max_sample_gap_ms"} or
            any(type(v) is not int or not 0 < v <= 900000 for v in guarded.values()) or
            not 2 <= guarded["settle_samples"] <= 256):
            raise ValueError("Invalid guarded host policy parameters")
        wire[0] += f" {first['eval']} {first['adc']} {first['mv']}"
    for row in rows[1:]:
        wire.append(" ".join(str(row[k]) for k in
                    ("index", "eval", "adc", "seq", "mv", "raw_valid", "usb_valid", "usb_good", "charge")))
    engine_command = [str(engine)] + ([] if guarded is None else
        [str(guarded[k]) for k in ("retain_ms", "settle_ms", "settle_samples", "max_sample_gap_ms")])
    result = subprocess.run(engine_command, input="\n".join(wire) + "\n", text=True,
                            capture_output=True, timeout=30, check=True)
    (output / "engine-input.txt").write_text("\n".join(wire) + "\n", encoding="utf-8")
    (output / "engine.txt").write_text(result.stdout, encoding="utf-8")
    predictions = {name: [dict(first)] for name in policies}
    aliases = []
    for line in result.stdout.splitlines():
        index, policy, *values = map(int, line.split())
        keys = OUTPUT_KEYS if guarded is None else OUTPUT_KEYS + ("restored", "history_reset", "settled")
        if len(values) != len(keys):
            raise ValueError("Unexpected engine output width")
        prediction = dict(rows[index], **dict(zip(keys, values)))
        predictions[policies[policy]].append(prediction)
        if policy:
            continue
        for key in OUTPUT_KEYS:
            # Context rows may retain NEW/SEED diagnostics from an earlier eval;
            # calling the same sequence again yields CACHED without a new ADC.
            if key == "decision" and prediction["decision"] == 3 and rows[index][key] in (0, 1, 2):
                aliases.append(index)
                continue
            if prediction[key] != rows[index][key]:
                raise ValueError(f"Historical production replay mismatch at row {index}, {key}: "
                                 f"{prediction[key]} != {rows[index][key]}")
    if any(len(group) != len(rows) for group in predictions.values()):
        raise ValueError("Incomplete engine output")
    unplug = next(row for row in rows if row["usb_valid"] and not row["usb_good"])
    light = [row for row in rows if row["surface"] == 5]
    if not light:
        raise ValueError("Missing flashlight context")
    exit_row = next(row for row in rows if row["index"] > light[-1]["index"] and row["surface"] != 5)
    indices = [47, 51, 53, 62, 86, 108]
    points = []
    for index in indices:
        if index >= len(rows):
            continue
        row = rows[index]
        points.append({"index": index, "adc_ms_from_exit": row["adc"] - exit_row["obs"],
                       "raw_mv": row["mv"], "candidate": row["candidate"],
                       **{name: group[index]["percent"] if group[index]["percent_valid"] else None
                          for name, group in predictions.items()}})
    report = {
        "schema": 1, "kind": "offline_replay", "hardware_verified": False,
        "fixture_sha256": digest(fixture), "fixture_source": data.get("source", {}),
        "source_sha256": {str(path.relative_to(ROOT)): digest(path) for path in
                          [*sources, pathlib.Path(__file__).resolve(),
                           ROOT / "scripts/battery_policy.h", baseline / "battery_estimate.h", ROOT / "core/power_state.h"]},
        "baseline_kind": "frozen production at17a9de5; not current firmware",
        "build_command": command, "build_exit": build.returncode,
        "engine_command": engine_command, "guarded_parameters": guarded,
        "compiler_version": subprocess.run([cc, "--version"], text=True, capture_output=True,
                                           check=True, timeout=10).stdout.splitlines()[0],
        "python_version": sys.version, "engine_sha256": digest(engine),
        "record_count": len(rows), "checked_rows": len(rows) - 1,
        "warm_state_row": 0, "cached_decision_aliases": aliases,
        "unique_adc": len({row["seq"] for row in rows if row["seq"]}),
        "unplug_delay_ms": unplug["obs"] - data["status"]["start"],
        "confirmed_battery_span_ms": rows[-1]["obs"] - unplug["obs"],
        "logical_duration_ms": data["status"]["duration"],
        "points": points,
        "policies": {name: {"changed_rows": [i for i in range(len(rows)) if
                           group[i]["percent"] != rows[i]["percent"]],
                           "invalid_rows": [i for i, row in enumerate(group) if not row["percent_valid"]]}
                     for name, group in predictions.items()},
        "limits": ["Warm start uses recorded row 0; earlier history is unavailable.",
                   "Host experiment parameters are not selected firmware constants.",
                   "OFF gaps contain no voltage samples; no interpolation or SOC calibration.",
                   "UI/PG observations are not proof of physical input actions or DEEP."]}
    write_json(output / "replay.json", report)
    write_json(output / "predictions.json", predictions)
    summary = ["# 电量离线回放", "", f"固定历史生产C回放：{len(rows)-1}行匹配；首行采用已记录热状态。",
               f"拔线延迟 {report['unplug_delay_ms']/1000:.3f}s，确认BAT跨度 "
               f"{report['confirmed_battery_span_ms']/1000:.3f}s。", "",
               "| index | 原始mV | 原版% | 保留策略% | 收敛策略% | 组合% |",
               "|---|---|---|---|---|---|"]
    summary += [f"| {p['index']} | {p['raw_mv']} | " + " | ".join(str(p[n]) for n in policies) + " |" for p in points]
    summary += ["", "以上为主机策略比较，未修改固件、未新增设备验证，也不代表真实SOC精度。"]
    (output / "report.md").write_text("\n".join(summary) + "\n", encoding="utf-8")
    return report


class Journal:
    """Flush every transition; interruptions leave evidence, never a silent pass."""
    def __init__(self, directory, report=None, *, binding=None, mode="rehearsal"):
        self.directory = pathlib.Path(directory).resolve()
        self.directory.mkdir(parents=True, exist_ok=False)
        self.events = []
        self.lock = threading.RLock()
        name = "replay.json" if report is not None else "binding.json"
        if report is not None:
            (self.directory / name).write_bytes(pathlib.Path(report).read_bytes())
        else:
            write_json(self.directory / name, binding)
        self.report_sha = digest(self.directory / name)
        self.event("created", mode=mode, report_sha256=self.report_sha,
                   evidence_name=name, python_version=sys.version)

    def event(self, kind, **values):
        with self.lock:
            item = {"sequence": len(self.events), "wall_ns": time.time_ns(),
                    "monotonic_ns": time.monotonic_ns(), "kind": kind, **values}
            with (self.directory / "events.jsonl").open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(item, ensure_ascii=False) + "\n")
                stream.flush()
            self.events.append(item)
            write_json(self.directory / "progress.json", item)


def read_progress(directory):
    directory = pathlib.Path(directory)
    events = []
    partial_tail = False
    for line in (directory / "events.jsonl").read_text(encoding="utf-8").splitlines():
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            partial_tail = True
            break
        if event.get("sequence") != len(events):
            raise ValueError("Journal sequence is incomplete")
        events.append(event)
    if not events or digest(directory / events[0].get("evidence_name", "replay.json")) != events[0]["report_sha256"]:
        raise ValueError("Missing journal or changed replay evidence")
    last = events[-1]
    return {"state": "incomplete" if partial_tail else
            last["kind"] if last["kind"] in ("finished", "interrupted")
            else "running_or_interrupted", "last_event": last,
            "partial_tail": partial_tail, "hardware_verified": False}


def guide(report_path, output, port, timeout, scripted=None):
    """OpenHTF rehearsal. Scripted responses are restricted to software tests."""
    import openhtf as htf
    from openhtf.output.callbacks import json_factory

    if not 0 <= port <= 65535 or not 0 < timeout <= 900:
        raise ValueError("Port must be 0..65535; prompt timeout must be 0..900 seconds")
    report = json.loads(pathlib.Path(report_path).read_text(encoding="utf-8"))
    if report.get("kind") != "offline_replay" or report.get("hardware_verified") is not False:
        raise ValueError("Guide requires an offline replay report")
    journal = Journal(output, report_path)
    responses = iter(scripted) if scripted is not None else None
    console = None
    outcome = "passed"

    @htf.PhaseOptions()
    def review(test, phase, message):
        nonlocal outcome
        try:
            if responses is None:
                console.prompt(phase, message + "\n本轮为离线演练，请勿操作设备。", timeout)
            else:
                answer = next(responses)
                if answer == "timeout":
                    raise TimeoutError("Scripted timeout")
                if answer.strip().lower() != "continue":
                    raise Cancelled("Scripted cancel")
                journal.event("confirmed", phase=phase)
            if console:
                console.stage(phase, "complete")
        except Cancelled:
            outcome = "cancelled"
            journal.event("cancelled", phase=phase)
            if console:
                console.stage(phase, "cancelled")
            return htf.PhaseResult.STOP
        except TimeoutError:
            outcome = "timeout"
            journal.event("timeout", phase=phase)
            if console:
                console.stage(phase, "timeout")
            return htf.PhaseResult.STOP

    @htf.measures(htf.Measurement("production_rows").equals(report["record_count"] - 1))
    def evidence(test):
        test.measurements.production_rows = report["checked_rows"]
        test.attach_from_file(str(pathlib.Path(report_path).resolve()), name="replay.json")
        journal.event("evidence_checked", rows=report["checked_rows"])

    phases = [evidence,
              review.with_args(phase="preparation", message="准备：核对历史BIN、会话与记录数量。"),
              review.with_args(phase="boundary", message=f"窗口：480秒包含拔线前的{report['unplug_delay_ms']/1000:.3f}秒；"
                               "未来实测须分别记录设备启动与人工动作时间。"),
              review.with_args(phase="result", message="结果：查看策略对比；显示一致性与SOC准确度须分别判断。")]
    test = htf.Test(*phases, test_name="WristFlow battery workflow rehearsal")
    record_path = journal.directory / "openhtf.json"
    exporter = json_factory.OutputToJSON(str(record_path), indent=2)
    saved = []

    def persist_record(record):
        # OpenHTF logs/swallow output callback errors. Treat durable output as
        # our own success condition, independently of Test.execute's result.
        try:
            exporter(record)
            value = json.loads(record_path.read_text(encoding="utf-8"))
            if value["outcome"] != record.outcome.name:
                raise ValueError("Persisted OpenHTF outcome mismatch")
            journal.event("record_saved", outcome=value["outcome"], sha256=digest(record_path))
            saved.append(value["outcome"])
        except Exception as exc:
            journal.event("output_error", error=type(exc).__name__, detail=str(exc))
            raise

    test.add_output_callbacks(persist_record)
    def execute():
        nonlocal outcome
        passed = test.execute(test_start=lambda: "offline-rehearsal")
        if len(saved) != 1 or (passed and saved[0] != "PASS"):
            raise RuntimeError("OpenHTF final report was not successfully persisted")
        if not passed and outcome == "passed":
            outcome = "error"
        if console:
            outcome = console.seal(outcome)
            passed = passed and outcome == "passed"
        journal.event("finished", passed=passed, outcome=outcome, hardware_verified=False)
        return bool(passed)

    try:
        if scripted is not None:
            return execute()
        else:
            with Console(journal, [("preparation", "准备证据"), ("boundary", "核对窗口"),
                                   ("result", "判断结果")], port) as console:
                print(f"Workflow rehearsal: {console.url}", flush=True)
                try:
                    passed = execute()
                    console.finish(outcome, "离线流程结束；未操作设备，详情见已保存的阶段和报告。")
                    return passed
                except BaseException:
                    console.finish("error", "报告或流程异常；请读取保存的事件，不计通过。")
                    raise
    except BaseException as exc:
        journal.event("interrupted", error=type(exc).__name__, detail=str(exc))
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="operation", required=True)
    command = commands.add_parser("replay")
    command.add_argument("--fixture", type=pathlib.Path,
                         default=ROOT / "tests/fixtures/battery-b2-20261021.json")
    command.add_argument("--out", required=True, type=pathlib.Path)
    command.add_argument("--cc", default="gcc")
    command = commands.add_parser("compare-production")
    command.add_argument("--fixture", required=True, type=pathlib.Path)
    command.add_argument("--out", required=True, type=pathlib.Path)
    command.add_argument("--cc", default="gcc")
    command = commands.add_parser("guide")
    command.add_argument("--report", required=True, type=pathlib.Path)
    command.add_argument("--out", required=True, type=pathlib.Path)
    command.add_argument("--port", default=4444, type=int)
    command.add_argument("--timeout", default=300, type=float)
    command = commands.add_parser("status")
    command.add_argument("--run", required=True, type=pathlib.Path)
    args = parser.parse_args()
    if args.operation == "replay":
        report = replay(args.fixture, args.out, args.cc)
        print(json.dumps({key: report[key] for key in
                          ("checked_rows", "record_count", "unplug_delay_ms", "confirmed_battery_span_ms")}, indent=2))
    elif args.operation == "compare-production":
        report = production_compare(args.fixture, args.out, args.cc)
        print(json.dumps({key: report[key] for key in
                          ("record_count", "historical_checked_rows", "changed_percent_rows")}, indent=2))
    elif args.operation == "guide":
        return 0 if guide(args.report, args.out, args.port, args.timeout) else 1
    else:
        print(json.dumps(read_progress(args.run), ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
