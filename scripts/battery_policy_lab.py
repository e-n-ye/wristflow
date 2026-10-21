"""Bounded host policy matrix; no serial, firmware mutation, or SOC calibration."""
from __future__ import annotations

import argparse
import itertools
import json
import pathlib

from battery_lab import ROOT, digest, load_trace, replay, write_json


def matrix(fixture, output, cc):
    output = pathlib.Path(output).resolve()
    output.mkdir(parents=True, exist_ok=False)
    data = load_trace(fixture)
    runs = []
    for retain, settle, gap in itertools.product((15000, 30000, 60000), (30000, 60000, 120000), (6000, 15000)):
        config = dict(retain_ms=retain, settle_ms=settle, settle_samples=3, max_sample_gap_ms=gap)
        directory = output / f"retain-{retain}-settle-{settle}-gap-{gap}"
        report = replay(fixture, directory, cc, guarded=config)
        predictions = json.loads((directory / "predictions.json").read_text(encoding="utf-8"))
        baseline = predictions["baseline"]
        if not all([r["percent_valid"] for r in group] == [r["percent_valid"] for r in baseline]
                   for group in predictions.values()):
            raise ValueError("Experiment changed known/unknown display validity")
        changes = {name: {"restored_rows": [r["index"] for r in group if r.get("restored")],
                          "reset_rows": [r["index"] for r in group if r.get("history_reset")],
                          "settled_rows": [r["index"] for r in group if r.get("settled")]}
                   for name, group in predictions.items()}
        runs.append({"parameters": config, "checked_rows": report["checked_rows"],
                     "points": report["points"], "events": changes,
                     "invalid_rows_unchanged": True, "directory": directory.name,
                     "replay_sha256": digest(directory / "replay.json")})
    result = {"schema": 1, "kind": "guarded_host_policy_matrix", "hardware_verified": False,
              "fixture_sha256": digest(fixture), "fixture_records": len(data["records"]),
              "source_sha256": {str(p.relative_to(ROOT)): digest(p) for p in
                  [pathlib.Path(__file__).resolve(), ROOT / "scripts/battery_policy.c",
                   ROOT / "scripts/battery_policy.h", ROOT / "scripts/battery_replay.c",
                   ROOT / "scripts/battery_lab.py", ROOT / "core/battery_estimate.c"]},
              "runs": runs, "selected_firmware_parameters": None,
              "limits": ["One near-full historical B2 input cannot select production durations or establish SOC accuracy.",
                         "Missing PG may preserve only a tentative saved copy; a new trusted discharge ADC is required.",
                         "The bound starts at the last accepted ADC completion, not a missing-input callback.",
                         "No evidence excludes an unobserved mode change or a complete unobserved counter wrap.",
                         "Elapsed observation/ADC ordering uses bounded uint32 modular differences (<2^31)."]}
    write_json(output / "matrix.json", result)
    lines = ["# 电量显示恢复：主机边界比较", "",
             "同一B2近满电记录；生产C后续120行逐项匹配。未修改固件，未新增设备数据。", "",
             "| 保留上限ms | 收敛跨度ms | 相邻支持上限ms | 组合收敛行 | 组合恢复行 |",
             "|---:|---:|---:|---|---|"]
    for run in runs:
        p, e = run["parameters"], run["events"]["combined"]
        lines.append(f"| {p['retain_ms']} | {p['settle_ms']} | {p['max_sample_gap_ms']} | "
                     f"{e['settled_rows']} | {e['restored_rows']} |")
    lines += ["", "三份有序新ADC之外还须满足时间跨度；重复UI读数不推进累计。无效或模式未知、候选改变和支持超时会重置累计。",
              "保留只在期限内的新鲜、可信放电样本恢复；无数据期间始终显示未知。长间隔、坏样本及模式不明按契约重播种。",
              "这些是主机候选边界，不是已选定固件参数，也不是本电池SOC校准。"]
    (output / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=pathlib.Path, default=ROOT / "tests/fixtures/battery-b2-20261021.json")
    parser.add_argument("--out", required=True, type=pathlib.Path)
    parser.add_argument("--cc", default="gcc")
    args = parser.parse_args()
    result = matrix(args.fixture, args.out, args.cc)
    print(f"{len(result['runs'])} guarded host configurations completed: {args.out / 'report.md'}")


if __name__ == "__main__":
    main()
