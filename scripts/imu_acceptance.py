"""Bounded read-only acceptance of native daily steps; simulation is the default."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import time

from battery_console import Cancelled, Console
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import ProtocolError, RecordedClient, RecordedSession, SerialTransport
from light_protocol import states
from serial_lab import prepare_capture


def row(lines, pattern):
    matches = [m.groups() for line in lines
               if (m := re.fullmatch(pattern, line[line.find("["):]))]
    if len(matches) != 1:
        raise ProtocolError("One complete, unambiguous cached row required: " + pattern)
    return tuple(int(value, 16 if value.startswith("0x") else 10) for value in matches[0])


def steps(lines):
    phase, end, valid, count, stopped = row(lines, r"\[imu-steps\] phase=(\d+) end=(\d+) counter_valid=(\d+) steps=(\d+) stop_verified=(\d+)")
    who, threshold, debounce, start_ms, duration, sample_ms = row(lines, r"\[imu-steps\] who=(0x[0-9a-fA-F]+) ths=(0x[0-9a-fA-F]+) deb=(0x[0-9a-fA-F]+) start=(\d+) duration=(\d+) sample=(\d+)")
    retries, recovered, failure, reg, ms = row(lines, r"\[imu-recovery\] retries=(\d+) recovered=(\d+) last_failure=(\d+) reg=(0x[0-9a-fA-F]+) ms=(\d+)")
    daily_valid, total, day, utc, version, native, restored = row(lines, r"\[steps\] valid=(\d+) total=(\d+) day=(\d+) utc=(\d+) version=(\d+) native=(\d+) partial=1 restored=(\d+)")
    available, verified, dirty, failed, attempts, saved_day, saved_total, generation = row(lines, r"\[steps-store\] available=(\d+) verified=(\d+) dirty=(\d+) failed=(\d+) attempts=(\d+) day=(\d+) total=(\d+) generation=(\d+)")
    mask, summed, saved_mask = row(lines, r"\[steps-hours\] valid=(0x[0-9a-fA-F]+) sum=(\d+) saved_valid=(0x[0-9a-fA-F]+)")
    hours = {}
    for line in lines:
        content = line[line.find("["):]
        m = re.fullmatch(r"\[steps-hour\] hour=(\d+) total=(\d+)", content)
        if content.startswith("[steps-hour]") and not m:
            raise ProtocolError("Malformed hour row")
        if m:
            hour, value = map(int, m.groups())
            if hour >= 24 or hour in hours or value > 0xffffffff:
                raise ProtocolError("Invalid/duplicate hour")
            hours[hour] = value
    if (mask >= 1 << 24 or saved_mask >= 1 << 24 or
            set(hours) != {h for h in range(24) if mask & (1 << h)} or
            sum(hours.values()) != summed or summed > total or total > 0xffffffff or
            count != native or native > 65535 or recovered > retries or
            not 1767225600 <= utc <= 4102444799 or day != (utc + 28800) // 86400):
        raise ProtocolError("Incomplete/inconsistent native daily snapshot")
    return dict(phase=phase, end=end, valid=valid, daily_valid=daily_valid, native=native,
                start_ms=start_ms, sample_ms=sample_ms,
                total=total, day=day, utc=utc, version=version, hours=hours, mask=mask,
                available=available, verified=verified, dirty=dirty, failed=failed,
                attempts=attempts, saved_day=saved_day, saved_total=saved_total,
                generation=generation, saved_mask=saved_mask, retries=retries,
                recovered=recovered, last_failure=failure, failure_reg=reg, failure_ms=ms)


def continuity(before, after, elapsed_ms=None):
    delta = after["total"] - before["total"]
    hourly_delta = sum(after["hours"].values()) - sum(before["hours"].values())
    same_hour = before["utc"] // 3600 == after["utc"] // 3600
    hour = (after["utc"] + 28800) // 3600 % 24
    sample_elapsed = (after["sample_ms"] - before["sample_ms"]) % (1 << 32)
    if (after["day"] != before["day"] or after["utc"] < before["utc"] or
            after["start_ms"] != before["start_ms"] or
            (elapsed_ms is not None and not max(0, elapsed_ms-70000) <= sample_elapsed <= elapsed_ms+70000) or
            delta != (after["native"] - before["native"]) % 65536 or
            not 0 <= hourly_delta <= delta or (same_hour and hourly_delta != delta) or
            (same_hour and any(value != before["hours"].get(h, 0)
                               for h, value in after["hours"].items() if h != hour)) or
            any(after["hours"].get(h, -1) < value for h, value in before["hours"].items()) or
            after["recovered"] < before["recovered"] or after["retries"] < before["retries"]):
        raise ProtocolError("Native/daily/hour continuity changed; preserve evidence for review")


class Observer(RecordedClient):
    COMMAND_PATTERN = r"wf_(?:steps|wrist|light) status"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL")

    def snapshot(self, *, active_light=True, deadline=None):
        s = steps(self.request("wf_steps status", lambda line: "[steps-hours]" in line, deadline=deadline))
        wrist = self.request("wf_wrist status", lambda line: "[wrist-wake]" in line, deadline=deadline)
        phase, end, configured, source, stopped = row(wrist, r"\[imu-wrist\] phase=(\d+) end=(\d+) configured=(\d+) source_valid=(\d+) stop_verified=(\d+)")
        requested, continuous, armed, owned, irqs, wakes = row(wrist, r"\[wrist-wake\] requested=(\d+) continuous=(\d+) armed=(\d+) owned=(\d+) irqs=(\d+) wakes=(\d+); software counts, not physical DEEP proof")
        light = states("\n".join(self.request("wf_light status", lambda line: "[auto-light]" in line, deadline=deadline)) + "\n")
        if len(light) != 1:
            raise ProtocolError("One auto-light cache required")
        light = light[0]
        if (s["phase"] != 2 or s["end"] != 0 or s["valid"] != 1 or s["daily_valid"] != 1 or
                not s["available"] or s["failed"] or
                (phase, end, configured, requested, continuous, armed, owned) != (2, 0, 1, 1, 1, 1, 1) or
                light["enabled"] != 1 or light["fault"] or light["cleanup"] or
                (active_light and (light["active"] != 1 or light["valid"] != 1))):
            raise ProtocolError("A dependent sensor/cache is not healthy")
        return dict(steps=s, wrist=dict(irqs=irqs, wakes=wakes), light=light)


class Simulation:
    port = "SIMULATED-IMU-ACCEPTANCE"

    def __init__(self):
        self.queue = bytearray()
        self.count, self.saved, self.seq = 17, 330, 0
        self.closed = False
        self.started = time.monotonic()

    def open(self):
        self.closed = False

    def close(self):
        self.closed = True

    def write(self, wire):
        command = wire.decode("ascii").strip()
        if command == "wf_steps status":
            total = 313 + self.count
            elapsed = int((time.monotonic()-self.started)*1000)
            text = (f"[imu-steps] phase=2 end=0 counter_valid=1 steps={self.count} stop_verified=0\n"
                    f"[imu-steps] who=0x6a ths=0x10 deb=0x6e start=1573 duration=0 sample={1000000+elapsed}\n"
                    "[imu-recovery] retries=0 recovered=0 last_failure=0 reg=0x00 ms=0\n"
                    f"[steps] valid=1 total={total} day=20745 utc={1792411300+elapsed//1000} version=1 native={self.count} partial=1 restored=1\n"
                    f"[steps-store] available=1 verified=1 dirty={int(total != self.saved)} failed=0 attempts=0 day=20745 total={self.saved} generation=23\n"
                    f"[steps-hours] valid=0x180000 sum={130+self.count} saved_valid=0x180000\n"
                    f"[steps-hour] hour=19 total=50\n[steps-hour] hour=20 total={80+self.count}\n")
        elif command == "wf_wrist status":
            text = ("[imu-wrist] phase=2 end=0 configured=1 source_valid=1 stop_verified=0\n"
                    "[wrist-wake] requested=1 continuous=1 armed=1 owned=1 irqs=0 wakes=0; software counts, not physical DEEP proof\n")
        elif command == "wf_light status":
            self.seq += 1
            text = f"[auto-light] enabled=1 active=1 valid=1 fault=0 cleanup=0 brightness=23 mlux=10000 ms={self.seq*1000} session=1 seq={self.seq}; uncalibrated\n"
        else:
            raise AssertionError("Simulation command outside policy")
        self.queue.extend(text.encode("ascii"))
        return len(wire)

    def read(self, count):
        time.sleep(.002)
        data = bytes(self.queue[:min(count, 47)])
        del self.queue[:len(data)]
        return data


PHASES = [("prepare", "确认新版与准备"), ("walk", "非零原生增量"),
          ("save", "自然保存与状态观察"), ("final", "关闭临时常亮并收尾")]


def run(args):
    simulated = args.port is None
    offline = args.offline_walk
    bound = 900 if offline else 600
    if simulated:
        binding, identity, transport = {"simulation": True}, lambda: {"simulation": True}, Simulation()
    else:
        if not all((args.result, args.flash, args.expected_bin, args.source_archive, args.confirmed)):
            raise ValueError("Explicit build/flash/archive/full hash and current confirmation required")
        binding, identity = prepare_capture(args.result, args.flash, args.expected_bin,
                                            args.port, args.location, args.confirmed,
                                            source_archive=args.source_archive)
        transport = SerialTransport(args.port)
    own_source = file_record(Path(__file__))
    binding.update(adapter=own_source, policy="Cached steps/wrist/light only; offline reopen requires explicit operator confirmation. No reset, time, PM, save or sensor controls.")
    journal = Journal(args.out, binding=binding, mode="imu-acceptance")
    report = dict(simulation=simulated, hardware_verified=False, snapshots=[], operator_confirmed=[],
                  disconnected_walk=offline, sessions=[])
    started = time.monotonic()
    phases = (PHASES[:1] + [("unplug", "关闭串口后切电池"), PHASES[1], ("reconnect", "接回原USB端口")] + PHASES[2:]) if offline else PHASES
    with Console(journal, phases, mode="IMU验收模拟（不开设备）" if simulated else "日常IMU有界验收") as console:
        print(console.url, flush=True)
        def guarded_identity():
            verify_files([own_source])
            return identity()
        session = client = None
        session_label = None
        def session_record():
            report["sessions"].append(dict(label=session_label, port_closed=session.port_closed,
                                          resources_closed=session.resources_closed))
        def open_session(label):
            nonlocal session, client, session_label
            session_label = label
            log = Journal(args.out / label, binding=binding, mode="imu-observer") if offline else journal
            session = RecordedSession(log, transport, console.cancelled, Observer, identity=guarded_identity, timeout=2)
            try:
                client = session.__enter__()
            except BaseException:
                session_record()
                session = None
                raise
        def close_session():
            nonlocal session, client
            if session:
                try:
                    session.__exit__()
                finally:
                    session_record()
                    session = client = None
        def pump():
            if console.cancelled.is_set():
                raise Cancelled("Operator cancelled")
            if time.monotonic() - started >= bound:
                raise TimeoutError("Acceptance reached its bounded deadline")
            if client:
                client.read_chunk(); client.take_lines(); client.last_bytes.clear()
            else:
                time.sleep(.02)
        def capture(stage, active_light=True):
            value = client.snapshot(active_light=active_light, deadline=started+bound)
            observed = time.monotonic()
            if report["snapshots"]:
                previous = report["snapshots"][-1]
                continuity(previous["value"]["steps"], value["steps"], (observed-previous["host_monotonic"])*1000)
            report["snapshots"].append(dict(stage=stage, value=value, host_monotonic=observed))
            journal.event("cached_observation", phase=stage, value=value)
            return value["steps"]
        def prompt(key, text):
            console.prompt(key, ("【仅模拟】" if simulated else "") + text, 300 if offline else 180, pump=pump)
            report["operator_confirmed"].append(key)
        try:
            prompt("prepare", "确认下载后的显示、触摸、KEY1、时间与原步数正常。抬腕、自动亮度保持开启，临时常亮设10分钟，回到普通表盘。此页不下载或校时；准备好后确认。")
            open_session("before")
            baseline = capture("baseline")
            console.stage("prepare", "complete")
            if offline:
                close_session()
                prompt("unplug", "程序已关闭串口并释放租约。请拔下USB，确认手表由原电池继续供电，原页面、时间和临时常亮正常后确认。出现重启或异常请取消并说明。")
                console.stage("unplug", "complete")
            prompt("walk", ("现在用电池正常走动，" if offline else "在接线不受力范围内自然走动，") +
                   "约40个单脚步后回来确认；无需精确计数，不测精度。即使数字没有增加，也请结束本次动作并确认，让程序保存失败证据，不要一直反复走。不要改开关或重启。")
            console.stage("walk", "complete")
            if offline:
                prompt("reconnect", "请把USB接回原电脑端口，等电脑识别约3秒，确认手表原页面／时间正常后再确认。程序仅重新核端点并只读，不重启、校时或发送save。")
                open_session("after")
                console.stage("reconnect", "complete")
            if simulated:
                transport.count += 12
            walked = capture("walk")
            if walked["total"] <= baseline["total"]:
                raise ProtocolError("No nonzero daily increment observed")
            watch_started = time.monotonic()
            next_query = 0
            def saved():
                nonlocal next_query
                if time.monotonic() < next_query:
                    return False
                next_query = time.monotonic() + (0.3 if simulated else 5)
                if simulated:
                    transport.saved = 313 + transport.count
                value = capture("save")
                return (time.monotonic()-watch_started >= (4 if simulated else 30) and
                        value["verified"] == 1 and not value["dirty"] and
                        value["saved_day"] == value["day"] and value["saved_total"] == value["total"] and
                        value["saved_mask"] == value["mask"])
            console.automatic("save", "请静置并保持常亮；程序观察至少30秒、最多90秒，核自然保存及传感器缓存，不发送save、重启或控制命令。", 12 if simulated else 90, pump=pump, until=saved)
            prompt("final", "请关闭临时常亮，确认显示、触摸、KEY1和时间正常，抬腕／自动亮度按本轮要求保持开启，回到普通表盘。确认后程序只读末态并关闭串口。")
            capture("final", active_light=False)
            console.stage("final", "complete")
            outcome = "passed"
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
            report["error"] = repr(exc)
            journal.event("workflow_error", error=repr(exc))
        finally:
            try:
                close_session()
            except BaseException as exc:
                outcome = "error"
                report["close_error"] = repr(exc)
        outcome = console.seal(outcome)
        report.update(outcome=outcome, workflow_passed=outcome == "passed",
                      port_closed=all(s["port_closed"] for s in report["sessions"]) if report["sessions"] else None,
                      resources_closed=all(s["resources_closed"] for s in report["sessions"]),
                      device_control_sent=False,
                      limits="Artificial peer in simulation. A clean run without a new recovery counter is non-reproduction, not proof of real fault recovery; saved hours expose a mask, not per-hour stored bytes. Reopen checks start/sample timing against host time (70s sampling allowance), not a unique boot attestation.")
        write_json(args.out / "workflow.json", report)
        journal.event("finished", outcome=outcome, report_sha256=digest(args.out / "workflow.json"))
        console.finish(outcome, "流程结束，人工确认与UART分别核验。取消或异常时请关闭临时常亮；工具仅释放主机资源，不停已有服务或重启设备。")
    return 0 if outcome == "passed" else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--port")
    parser.add_argument("--location", default="1-1.1")
    parser.add_argument("--result", type=Path)
    parser.add_argument("--flash", type=Path)
    parser.add_argument("--source-archive", type=Path)
    parser.add_argument("--expected-bin")
    parser.add_argument("--confirmed", action="store_true")
    parser.add_argument("--offline-walk", action="store_true", help="Close UART before unplug/walk; reopen only after explicit reconnect confirmation")
    raise SystemExit(run(parser.parse_args()))
