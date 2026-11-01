"""One guided, single-file CycleLog BAT qualification; no flashing or formatting."""
import argparse
import json
from pathlib import Path
import time

from battery_console import Console, Cancelled
from battery_lab import Journal
from cycle_log_lab import collect
from cycle_protocol import decode_file
from lab_evidence import file_record, verify_files, write_json
from lab_serial import SerialTransport, lease_directory
from serial_lab import prepare_capture

PHASES = [("prepare", "确认FULL与测试准备"), ("arm", "保存起点并关闭串口"),
          ("unplug", "拔USB并进入手电筒"), ("timer", "纯电池计时"),
          ("exit", "退出手电筒并等待保存"), ("reconnect", "接回USB"),
          ("read", "核对停止并完整导出"), ("finish", "确认设备末态")]


def analyze(rows, seconds):
    samples = [r for r in rows if r["kind"] == 2]
    valid = [r for r in samples if r["flags"] & 50 == 50 and
             r["usb_flags"] & 3 == 1 and r["surface"] == 5 and
             r["requested"] == r["acknowledged"] == 100 and
             r["context_flags"] & 7 == 7 and not r["context_flags"] & 56 and
             r["phase"] == 0 and r["sample_age_ms"] <= 15000 and r["raw_mv"] > 3600]
    span = valid[-1]["elapsed_ms"] - valid[0]["elapsed_ms"] if valid else 0
    checks = {"load_exit_end": rows[-1]["kind"] == 3 and rows[-1]["stop_reason"] == 4,
              "all_samples_battery_flashlight": len(valid) == len(samples) and bool(samples),
              "sample_span": span >= max(0, seconds * 1000 - 15000),
              "adc_identity_progress": len({(r["adc_sequence"], r["adc_sampled_ms"]) for r in valid}) >= 2}
    return {"checks": checks, "passed": all(checks.values()), "sample_count": len(samples),
            "sample_span_ms": span, "raw_first_mv": valid[0]["raw_mv"] if valid else None,
            "raw_last_mv": valid[-1]["raw_mv"] if valid else None,
            "limits": "Host timing starts at operator confirmation; sampled load span is checked separately. No low-voltage cutoff, current, capacity or battery-life proof."}


class Device:
    def __init__(self, args):
        self.args = args
        self.binding, guard = prepare_capture(args.result, args.flash, args.expected_bin, args.port,
            args.location, args.confirmed, build_archive=args.build_archive, recorded_root=args.recorded_root)
        own = [file_record(Path(__file__).with_name(n)) for n in
               ("cycle_short_lab.py", "cycle_log_lab.py", "cycle_protocol.py", "battery_console.py")]
        self.binding = dict(self.binding, short_tools=own)
        def identity():
            verify_files(own)
            return guard()
        self.identity = identity

    def action(self, action, out, session, cancel):
        return collect(out, SerialTransport(self.args.port), binding=self.binding, identity=self.identity,
            locks=lease_directory(), action=action, kind="cycle", session=session, cancel=cancel)


class Simulation:
    """UI-only substitute; never imports serial or enumerates endpoints."""
    binding = {"simulation": True}
    def action(self, action, out, session, cancel):
        self.calls = getattr(self, "calls", []) + [action]
        if cancel.is_set():
            raise Cancelled("Simulation cancelled")
        if action == "status":
            value = dict(state=2 if self.calls.count("status") == 1 else 5, session=session,
                         accepted=1 if self.calls.count("status") == 1 else 5,
                         saved=1 if self.calls.count("status") == 1 else 5,
                         queued=0, dropped=0, reason=0 if self.calls.count("status") == 1 else 4, inhibit=0)
            return dict(passed=True, resources_closed=True, status=value)
        return dict(passed=True, resources_closed=True)


def run(args, backend=None, console_type=Console):
    simulated = args.port is None
    if not 0 < args.seconds <= 120 or (not simulated and args.seconds < 60):
        raise ValueError("Device window60..120 seconds")
    if not 0 < args.session < 2**32:
        raise ValueError("Explicit nonzero uint32 session required")
    backend = backend or (Simulation() if simulated else Device(args))
    journal = Journal(args.out, binding=dict(backend.binding, session=args.session, seconds=args.seconds,
        adapter=file_record(Path(__file__))), mode="cycle-short")
    outcome, detail, current = "passed", "", "prepare"
    possible = owned = stopped = reconnect_confirmed = False
    analysis = None
    with console_type(journal, PHASES, mode="Cycle短窗模拟（无设备）" if simulated else "Cycle纯电池短窗") as console:
        print(console.url, flush=True)
        def prompt(key, text, timeout=180):
            nonlocal current
            current = key
            console.prompt(key, text, timeout)
            console.stage(key, "complete")
        def action(name, directory):
            value = backend.action(name, args.out / directory, args.session, console.cancelled)
            if not value["passed"] or not value["resources_closed"]:
                raise ValueError("Cycle action failed; no retry: " + repr(value))
            return value
        try:
            prompt("prepare", "保持原电池＋USB，设备已刷CycleLog并mount。确认电池页显示已充满，临时常亮已开启，等至少6秒刷新ADC；准备好后确认。不要现在拔线。")
            current = "arm"; console.stage(current, "running")
            possible = True  # Persist uncertainty even if ARM acknowledgement is lost.
            journal.event("arm_intent", session=args.session)
            action("arm", "arm")
            for attempt in range(10):
                s = action("status", f"armed-status-{attempt}")["status"]
                if s["session"] != args.session or s["state"] != 2:
                    raise ValueError("ARM ownership/state not confirmed")
                owned = True
                if s["dropped"] or s["accepted"] != 1:
                    raise ValueError("Unexpected START counters")
                if s["saved"] == 1 and not s["queued"]:
                    break
                console.cancelled.wait(.25)
            else:
                raise ValueError("START persistence not confirmed")
            armed_at = time.monotonic()
            console.stage(current, "complete")
            prompt("unplug", "串口已关闭。现在拔USB，只留原电池，打开手电筒。确认白屏持续全亮后点击继续；不要更改电池接线。异常时退出手电筒、接USB并取消。", 120)
            if time.monotonic() - armed_at > 120:
                raise TimeoutError("Battery preparation exceeded120s")
            current = "timer"
            console.automatic(current, f"保持手电筒白屏和USB拔出，程序计时{args.seconds:g}秒。不要提前退出。异常时立即退出手电筒、接USB并取消。", args.seconds)
            prompt("exit", "计时结束，USB仍保持拔出。手动退出手电筒，回电池页，等待约10秒让停止和保存完成；确认手电筒已退出后继续。")
            prompt("reconnect", "现在接回原电脑USB端口，开启临时常亮并保持亮屏，等Windows连接稳定后确认。确认后才重新开串口。")
            reconnect_confirmed = True
            current = "read"; console.stage(current, "running")
            s = action("status", "ended-status")["status"]
            if s["session"] != args.session or s["state"] not in (5, 6):
                raise ValueError("Owned terminal session not confirmed; no foreign/active stop")
            stopped = True
            # Preserve the full file even when terminal health fails qualification.
            exported = action("export", "export")
            if simulated:
                analysis = dict(passed=True, checks={"simulated_workflow": True})
            else:
                raw = (args.out / "export" / "cycle.bin").read_bytes()
                rows = decode_file(raw, exported["info"])
                analysis = analyze(rows, args.seconds)
            write_json(args.out / "analysis.json", analysis)
            if s["state"] != 5 or s["reason"] != 4 or s["dropped"] or s["queued"] or s["saved"] != s["accepted"] or not analysis["passed"]:
                raise ValueError("Short-window qualification failed; exported evidence preserved")
            console.stage(current, "complete")
            prompt("finish", "日志已导出，串口已关闭。保持USB连接，确认显示、触摸正常且无保存失败提示；随后由助手恢复普通版。不要自行重启或重刷。")
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
            detail = repr(exc)
            journal.event("short_failed", phase=current, error=detail)
            console.stage(current, outcome)
        outcome = console.seal(outcome)
        report = dict(passed=outcome == "passed", outcome=outcome, detail=detail, simulation=simulated,
            hardware_verified=False, session=args.session, possible_arm=possible, owned=owned,
            stopped=stopped, reconnect_confirmed=reconnect_confirmed,
            recovery_required=possible and not stopped, analysis=analysis,
            limits="Single cycle.bin consumed. No format, reset, flash or clock writes. Errors/cancellation never reconnect or send blind stop; operator exits load and reconnects, then assistant independently verifies device state.")
        write_json(args.out / "experiment.json", report)
        console.finish(outcome, "短窗流程完成，保持USB连接等待恢复普通版。" if report["passed"] else
            "流程未通过。请立即退出手电筒并接回USB，保持设备亮屏并告知助手；不要重启或重刷。主机关口不证明设备停止。" + detail)
        return report


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--seconds", type=float, default=120)
    p.add_argument("--session", type=int, required=True)
    p.add_argument("--port")
    p.add_argument("--location", default="1-1.1")
    for key in ("result", "flash", "build-archive", "recorded-root"):
        p.add_argument("--" + key, type=Path)
    p.add_argument("--expected-bin")
    p.add_argument("--confirmed", action="store_true")
    args = p.parse_args()
    if args.port and not all((args.result, args.flash, args.build_archive, args.recorded_root, args.expected_bin, args.confirmed)):
        p.error("Device requires explicit build/flash/archive/root/hash and current confirmation")
    return 0 if run(args)["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
