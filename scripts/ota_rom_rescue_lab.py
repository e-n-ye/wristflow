"""ROM rescue workflow rehearsal. CLI supports simulation/status ONLY.

No serial package, port enumeration, real reset or Flash operation is reachable
from this entry point. Logical wire timing is synthetic, not hardware evidence.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import time

from battery_console import Cancelled, Console
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import PortLease
from ota_rom_backup import application, recovery, Simulation as ApplicationSimulation
from ota_rom_rescue import ModelClock, Link, Simulation, experiment, cleanup

PHASES = [("prepare", "模拟前提确认"), ("before", "保全模拟应用前态"),
          ("rom", "模拟ROM捕获与退出"), ("return", "模拟原程序返回"),
          ("after", "读取模拟应用后态"), ("final", "模拟末态确认")]
SCENARIOS = ("success", "aircr-no-reply", "no-catch", "stale-catch", "no-reset-flag",
    "pc-outside", "pc-mismatch", "unstable", "vtor", "regrdy", "foreign-debug",
    "no-reply", "short-write", "bad-marker", "bad-terminator", "long-ack",
    "extra-frame", "exit-fail", "rts-release-fail", "close-fail",
    "release-banner", "release-old-frame", "release-readback-fail")
SOURCES = ("ota_rom_rescue_lab.py", "ota_rom_rescue.py", "ota_rom_protocol.py", "ota_rom_backup.py",
    "runtime_export.py", "runtime_record_decode.py", "battery_console.py", "battery_lab.py",
    "lab_serial.py", "lab_evidence.py", "device_snapshot.py", "cycle_log_lab.py",
    "cycle_protocol.py", "storage_protocol.py", "fs_protocol.py")


class Budget:
    def __init__(self, seconds):
        if not 0 < seconds <= 300:
            raise ValueError("Workflow budget must be 0..300 seconds")
        self.end = time.monotonic() + seconds
    def remaining(self, limit=120):
        left = min(limit, self.end - time.monotonic())
        if left <= 0:
            raise TimeoutError("Global five-minute workflow budget expired")
        return left


class BudgetTransport:
    def __init__(self, peer, budget): self.peer, self.budget = peer, budget
    def open(self): self.budget.remaining(); return self.peer.open()
    def read(self, count): self.budget.remaining(); return self.peer.read(count)
    def write(self, wire): self.budget.remaining(); return self.peer.write(wire)
    def close(self): return self.peer.close()


def simulate(output, before_fixture, after_fixture, *, scenario="success", ready=None,
             budget_seconds=300, prompt_seconds=120, step_delay=.1, allow_exit_reset=True,
             wire_factory=Simulation, application_factory=ApplicationSimulation):
    output = Path(output).resolve()
    if output.exists() or scenario not in SCENARIOS or not 0 < prompt_seconds <= 120 or not 0 <= step_delay <= .1:
        raise ValueError("New directory, supported simulation and bounded waits required")
    sources = [file_record(Path(__file__).with_name(name)) for name in SOURCES]
    fixtures = {"before": bytes(before_fixture), "after": bytes(after_fixture)}
    budget = Budget(budget_seconds)
    journal = Journal(output, binding=dict(simulation=True, adapter_sources=sources,
        scenario=scenario, workflow_seconds=budget_seconds, step_delay=step_delay,
        policy="SIMULATION ONLY: logical protocol clock; no device or Flash entry"), mode="ota-rom-rescue-simulation")
    report = dict(simulation=True, hardware_verified=False, rom_rescue_verified=False,
        deployment_authorized=False, flash_write=False, clock_write=False, device_opened=False,
        observation_passed=False, application_recovery_passed=False, resources_closed=True,
        physical_observations_confirmed=False, entry_attempted=False)
    observations, link = {}, None
    outcome, detail = "passed", ""
    with Console(journal, PHASES, mode="ROM救援模拟（无设备／逻辑协议时钟）") as console:
        print(console.url, flush=True)
        if ready: ready(console)
        def check():
            budget.remaining()
            verify_files(sources)
            return {"simulation": True}
        try:
            console.prompt("prepare", "【仅模拟】将使用替身应用记录和UART数据演练ROM捕获、保持及退出。"
                "本工具不打开COM，不复位设备，也不写Flash；模拟通过不代表实机通过。",
                budget.remaining(prompt_seconds))
            console.stage("prepare", "complete")
            with PortLease("SIMULATED-OTA-ROM-RESCUE", output.parent / "simulation-locks"):
                journal.event("lease_acquired", simulation=True)
                def app(phase):
                    console.stage(phase, "running")
                    value = application(phase, BudgetTransport(application_factory(fixtures[phase]), budget),
                                        journal, console, check, cycle=True)
                    budget.remaining()
                    console.stage(phase, "complete")
                    return value
                before = app("before")
                check()
                if not 0 <= time.time_ns() - before["closed_ns"] <= 120_000_000_000:
                    raise ValueError("Fresh prestate expired")
                with (output / "rom-rx.bin").open("xb") as raw:
                    clock, peer = ModelClock(), wire_factory(scenario)
                    try:
                        link = Link(peer, raw, journal, clock, console.cancelled, budget.remaining)
                        steps = experiment(link, observations)
                        complete = False
                        def pump():
                            nonlocal complete
                            check()
                            try:
                                journal.event("model_step", step=next(steps), clock_seconds=clock.now())
                            except StopIteration:
                                complete = True
                            if step_delay:
                                console.cancelled.wait(min(step_delay, budget.remaining()))
                        console.automatic("rom", "【仅模拟】程序自动检查复位捕获和ROM上下文；"
                            "确认按钮不能完成本阶段，可以取消。协议保持时间由逻辑时钟模拟。",
                            budget.remaining(30), pump=pump, until=lambda: complete)
                    finally:
                        # Cleanup stays under the lease and raw-file lifetime.
                        if link is None:
                            try:
                                peer.close()
                                journal.event("unbound_peer_closed")
                            except BaseException as exc:
                                report["resources_closed"] = False
                                journal.event("unbound_peer_close_failed", error=repr(exc))
                        else:
                            report["entry_attempted"] = link.entry_intent
                            report["cleanup"] = cleanup(link, allow_exit_reset=allow_exit_reset)
                            report["resources_closed"] = report["cleanup"]["resources_closed"]
                report["observation_passed"] = observations.get("observation_passed", False)
                if report["cleanup"]["errors"] or not report["cleanup"]["debug_release_confirmed"]:
                    raise ValueError("Simulated debug exit incomplete; preserve separate observation")
                console.prompt("return", "【仅模拟】确认进入模拟原程序恢复检查；此点击不表示观察过真实设备。",
                               budget.remaining(prompt_seconds))
                console.stage("return", "complete")
                after = app("after")
                report["application_recovery"] = recovery(before, after)
                report["application_recovery_passed"] = True
                journal.event("lease_releasing", rom_transport_closed=link.closed)
            console.prompt("final", "【仅模拟】确认本轮演练结束。实机救援、屏触、KEY1及RTC未在本轮验证。",
                           budget.remaining(prompt_seconds))
            console.stage("final", "complete")
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else (
                "timeout" if isinstance(exc, TimeoutError) else "error")
            detail = repr(exc)
            journal.event("rescue_error", outcome=outcome, error=detail)
        finally:
            # Model proof remains available even when cleanup or poststate fails.
            report["observation_passed"] = observations.get("observation_passed", False)
            if any(e["kind"] == "transport_close_failed" for e in journal.events):
                report["resources_closed"] = False
            report["observations"] = observations
            report["recovery_required"] = report["entry_attempted"] and not report["application_recovery_passed"]
            report["verification"] = [
                {"title": "模拟ROM捕获与逻辑保持", "passed": report["observation_passed"]},
                {"title": "模拟调试退出", "passed": bool(report.get("cleanup", {}).get("debug_release_confirmed")) and not report.get("cleanup", {}).get("errors", [])},
                {"title": "模拟应用记录与RTC恢复", "passed": report["application_recovery_passed"]},
                {"title": "主机资源关闭", "passed": report["resources_closed"]}]
        outcome = console.seal(outcome)
        report.update(passed=outcome == "passed", outcome=outcome, detail=detail)
        report["files"] = [file_record(path) for path in sorted(output.iterdir()) if path.is_file()]
        write_json(output / "rescue.json", report)
        journal.event("finished", outcome=outcome, report_sha256=digest(output / "rescue.json"))
        console.finish(outcome, "模拟结束，原始数据和退出结果已保存；本轮没有设备操作。",
                       verification=report["verification"])
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    sim = sub.add_parser("simulate")
    sim.add_argument("--out", type=Path, required=True)
    sim.add_argument("--before-fixture", type=Path, required=True)
    sim.add_argument("--after-fixture", type=Path, required=True)
    sim.add_argument("--scenario", choices=SCENARIOS, default="success")
    status = sub.add_parser("status")
    status.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    if args.mode == "status":
        print((args.run / "rescue.json").read_text(encoding="utf-8"))
        return 0
    report = simulate(args.out, args.before_fixture.read_bytes(), args.after_fixture.read_bytes(), scenario=args.scenario)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
