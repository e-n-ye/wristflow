"""Bounded diagnostic-image/font acceptance; simulation is the default."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import secrets
import time

from battery_console import Cancelled, Console
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import ProtocolError, RecordedClient, RecordedSession, SerialTransport
from serial_lab import prepare_capture


STATUS = re.compile(r"\[resource\] tag=(\d+) mode=(\d+) active=([01]) pending=([01]) available=([01]) start=(\d+) elapsed=(\d+) reason=(\d+)$")
FIELDS = ("tag", "mode", "active", "pending", "available", "start", "elapsed", "reason")
VIEWS = [
    ("hour1", "1点压缩图", "检查中央的1点图：图形完整、位置正确，无花屏、色块或残影。"),
    ("hour7", "7点压缩图", "检查中央的7点图：图形完整、位置正确，无花屏、色块或残影；切图后界面仍响应。"),
    ("unknown", "未知时间压缩图", "检查未知时间图（不是修改RTC）：图形完整、位置正确，无花屏、色块或残影。"),
    ("font", "6500常用汉字字库", "上下滚动消息样本，检查常用汉字、英文数字、标点和三个二级字𠳐／𥻗／𬉼可见。范围外的龘／𠮷／😀允许显示缺字占位。检查文字没有重叠或滚动异常。"),
]
HOUR_VIEWS = [(f"hour{hour}", f"{hour}点压缩图", f"检查{hour}点图完整、位置正确，无花屏、色块、残影，切图后仍响应。")
              for hour in (2, 3, 4, 5, 6, 8, 9, 10, 11, 12)]
HOUR_VIEWS.append(("thumbnail", "10点缩略图", "检查表盘选择器的10点缩略图：缩放完整、比例正确、边缘无异常。"))


def parse_status(lines):
    values = [dict(zip(FIELDS, map(int, match.groups()))) for line in lines
              if (match := STATUS.search(line))]
    if len(values) != 1:
        raise ProtocolError("Expected one complete resource status")
    value = values[0]
    if value["mode"] > 14 or value["reason"] > 4:
        raise ProtocolError("Invalid resource status")
    return value


def require_owner(value, tag, mode, start=None):
    if value["tag"] != tag or value["mode"] != mode or (start is not None and value["start"] != start):
        raise ProtocolError("Resource ownership changed; no foreign stop is permitted")


class Client(RecordedClient):
    COMMAND_PATTERN = r"wf_resource (?:status|start (?:[0-9]|1[0-4]) [1-9][0-9]{0,8}|stop [1-9][0-9]{0,8})"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL")

    def status(self, cleanup=False):
        return parse_status(self.request("wf_resource status", lambda s: STATUS.search(s), cleanup=cleanup))

    def wait_state(self, tag, mode, active, start=None, cleanup=False):
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            value = self.status(cleanup)
            # The UI thread may not have consumed the accepted mailbox yet.
            if value["pending"]:
                time.sleep(.02)
                continue
            require_owner(value, tag, mode, start)
            if value["active"] == active:
                return value
            time.sleep(.02)
        raise TimeoutError("Resource UI acknowledgement did not arrive")


class Simulation:
    port = "SIMULATED-RESOURCE"

    def __init__(self):
        self.queue = bytearray()
        self.value = dict(zip(FIELDS, (0, 0, 0, 0, 1, 0, 0, 0)))

    def open(self):
        pass

    def close(self):
        pass

    def read(self, count):
        time.sleep(.002)
        data = bytes(self.queue[:count])
        del self.queue[:len(data)]
        return data

    def write(self, wire):
        parts = wire.decode("ascii").split()
        if parts[1] == "start":
            self.value.update(tag=int(parts[3]), mode=int(parts[2]), active=1, available=0,
                              start=self.value["start"] + 100, elapsed=0, reason=0)
            reply = f"[resource] accepted=1 tag={parts[3]}"
        elif parts[1] == "stop":
            owned = int(parts[2]) == self.value["tag"] and self.value["active"]
            if owned:
                self.value.update(active=0, available=1, reason=2)
            reply = f"[resource] stop_owned={int(bool(owned))} tag={parts[2]}"
        else:
            reply = "[resource] " + " ".join(f"{k}={self.value[k]}" for k in FIELDS)
        self.queue.extend((reply + "\r\n").encode())
        return len(wire)


def run(args):
    simulated = args.port is None
    all_hours = getattr(args, "hours", False)
    first_mode = getattr(args, "start_mode", 4)
    if first_mode not in range(4, 15) or (not all_hours and first_mode != 4):
        raise ValueError("Resume mode requires --hours and a mode from 4 through 14")
    views = HOUR_VIEWS[first_mode - 4:] if all_hours else VIEWS
    if simulated:
        binding, identity = {"simulation": True}, lambda: {"simulation": True}
    else:
        if not all((args.result, args.flash, args.expected_bin, args.source_archive, args.confirmed)):
            raise ValueError("Device mode requires build/flash/archive/hash and current confirmation")
        binding, identity = prepare_capture(args.result, args.flash, args.expected_bin,
                                            args.port, args.location, args.confirmed,
                                            source_archive=args.source_archive)
        if binding.get("simulation"):
            raise ValueError("Device binding cannot be simulated")
        import json
        if not json.loads(args.result.read_text(encoding="utf-8-sig")).get("resource_probe"):
            raise ValueError("Requires an explicit resource diagnostic build")
    own_source = file_record(Path(__file__))
    binding.update(adapter=own_source, policy="Bounded 60s UI-only views; matched tag/mode/start cleanup. No reset, clock, PM, sensor or business-data injection.")
    journal = Journal(args.out, binding=binding, mode="resource-acceptance")
    phases = [("ready", "准备"), *[(k, title) for k, title, _ in views], ("final", "退出与末态")]
    report = {"simulation": simulated, "hardware_verified": False, "operator_confirmed": [], "views": []}
    started, owned = time.monotonic(), None
    with Console(journal, phases, mode="资源诊断模拟（不操作手表）" if simulated else "小时图与缩略图验收" if all_hours else "三图EZIP与6500字库实机验收") as console:
        print(console.url, flush=True)
        session = client = None

        def pump():
            if console.cancelled.is_set():
                raise Cancelled("Operator cancelled")
            if time.monotonic() - started > 1200:
                raise TimeoutError("Resource workflow reached twenty minutes")
            if client:
                client.read_chunk(); client.take_lines(); client.last_bytes.clear()
            else:
                time.sleep(.02)

        def confirm(key, message, seconds):
            console.prompt(key, ("【仅模拟】" if simulated else "") + message, seconds, pump=pump)
            report["operator_confirmed"].append(key)
            journal.event("operator_observation", phase=key, statement=message)
            console.stage(key, "complete")

        def stop_owned():
            nonlocal owned
            if owned is None or client.booted:
                return
            tag, mode, tick = owned
            current = client.status(cleanup=True)
            if current["pending"]:
                raise ProtocolError("Start is still pending; ownership is uncertain")
            require_owner(current, tag, mode, tick)
            if tick is None:
                tick = current["start"]
            if current["active"]:
                lines = client.request(f"wf_resource stop {tag}", lambda s: f"[resource] stop_owned=1 tag={tag}" in s, cleanup=True)
                journal.event("stop_ack", lines=lines)
            stopped = client.wait_state(tag, mode, 0, tick, cleanup=True)
            journal.event("stopped", value=stopped)
            owned = None

        try:
            confirm("ready", "请将手表唤醒，返回默认数字表盘（第一页）。本轮每张图最多60秒；网页出现后及时观察并确认。不要修改设置，不要按KEY1（会退出本张图）。若图像或操作异常，取消并在聊天说明。", 180)
            def guard():
                verify_files([own_source])
                return identity()
            peer = Simulation() if simulated else SerialTransport(args.port)
            session = RecordedSession(journal, peer, console.cancelled, Client, identity=guard,
                                      locks=args.out.parent / "simulation-locks" if simulated else None, timeout=3)
            client = session.__enter__()
            before = client.status()
            if before["active"] or before["pending"] or not before["available"]:
                raise ProtocolError("Resource diagnostic not available on settled awake HOME")
            for mode, (key, title, message) in enumerate(views, start=first_mode if all_hours else 0):
                tag = secrets.randbelow(999999999) + 1
                # Mark uncertainty before transmission; no blind retry or foreign stop.
                owned = (tag, mode, None)
                client.request(f"wf_resource start {mode} {tag}", lambda s: f"[resource] accepted=1 tag={tag}" in s)
                active = client.wait_state(tag, mode, 1)
                owned = (tag, mode, active["start"])
                view_started = time.monotonic()
                confirm(key, message + " 正常才确认；异常请取消。", 45)
                after = client.status()
                require_owner(after, *owned[:2], owned[2])
                if not after["active"] or after["elapsed"] >= 60000 or time.monotonic() - view_started >= 55:
                    raise ProtocolError("View ended before observation completed")
                report["views"].append({"title": title, "before": active, "after": after})
                stop_owned()
            confirm("final", "诊断图已退出，请确认回到正常表盘，显示、触摸、KEY1正常，诊断画面没有残留。确认后关闭串口，随后会恢复普通Product并另验自然息屏／唤醒。", 120)
            report["final_status"] = client.status()
            if report["final_status"]["active"] or report["final_status"]["pending"]:
                raise ProtocolError("Diagnostic remains active")
            outcome = "passed"
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
            report["error"] = repr(exc)
            journal.event("workflow_error", error=repr(exc))
        finally:
            try:
                if client:
                    stop_owned()
            except BaseException as exc:
                report["cleanup_error"] = repr(exc)
                outcome = "error"
            report["recovery_required"] = owned is not None
            if session:
                try:
                    session.__exit__()
                except BaseException as exc:
                    report["close_error"] = repr(exc)
                    outcome = "error"
            report["port_closed"] = session.port_closed if session else None
            report["resources_closed"] = session.resources_closed if session else True
        outcome = console.seal(outcome)
        report.update(outcome=outcome, workflow_passed=outcome == "passed",
                      physical_observations_confirmed=not simulated and outcome == "passed")
        write_json(args.out / "workflow.json", report)
        journal.event("finished", outcome=outcome, report_sha256=digest(args.out / "workflow.json"))
        console.finish(outcome, "流程结束，串口已关闭。人工确认与UART分别核验；异常时诊断最多60秒自动退出，也可按KEY1退出。普通Product恢复另行记录。")
    return 0 if outcome == "passed" else 1


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--port")
    p.add_argument("--location", default="1-1.1")
    p.add_argument("--result", type=Path)
    p.add_argument("--flash", type=Path)
    p.add_argument("--source-archive", type=Path)
    p.add_argument("--expected-bin")
    p.add_argument("--confirmed", action="store_true")
    p.add_argument("--hours", action="store_true", help="Validate ten additional hours and the scaled thumbnail")
    p.add_argument("--start-mode", type=int, choices=range(4, 15), default=4,
                   help="Start a new guarded hours session at this mode; earlier evidence is kept separately")
    raise SystemExit(run(p.parse_args()))
