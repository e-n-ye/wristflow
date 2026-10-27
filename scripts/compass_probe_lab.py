"""Bounded foreground compass experiment; simulation never opens a serial port."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import time

from battery_console import Cancelled, Console
from battery_lab import Journal, read_progress
from compass_protocol import snapshots
from lab_evidence import digest, file_record, write_json
from lab_serial import ProtocolError, RecordedSession, SerialTransport, SessionChanged, lease_directory
from mmc5603_lab import MagClient, require

ROOT = Path(__file__).resolve().parents[1]
TAGS = ("state", "window", "map", "time", "raw", "quality", "field", "angle", "param")
FORMATS = (
    r"id=\d+ gen=\d+ active=[01] algo=[01] frozen=[01] end=[0-6] frames=\d+ drop=\d+",
    r"id=\d+ start=\d+ duration=\d+",
    r"id=\d+ a=-?[123]/-?[123]/-?[123] m=-?[123]/-?[123]/-?[123] verified=0; experimental axis hypothesis",
    r"id=\d+ seq=\d+ mag=\d+ begin=\d+ end=\d+ src=[012] mask=\d+",
    r"id=\d+ seq=\d+ a=-?\d+/-?\d+/-?\d+ m=-?\d+/-?\d+/-?\d+",
    r"id=\d+ seq=\d+ bank=[0-9a-f]{2} xl=[0-9a-f]{2} ctrl3=[0-9a-f]{2} status=[0-9a-f]{2} accuracy=-?\d+ saturation=-?\d+",
    r"id=\d+ seq=\d+ corrected_uG=-?\d+/-?\d+/-?\d+ valid=[01]",
    r"id=\d+ seq=\d+ vendor_mdeg=-?\d+ front_mdeg=-?\d+ output=[01] projected=[01]",
    r"id=\d+ updates=\d+ valid=[01] uG=-?\d+/-?\d+/-?\d+/-?\d+; RAM only",
)
BLOCK = re.compile("\n".join(r"\[cp-"+tag+r"\] "+fmt for tag, fmt in zip(TAGS, FORMATS))+r"(?:\n|$)")


def records(text):
    """Only contiguous complete reports; never repair truncated/interleaved fields."""
    result = []
    for match in BLOCK.finditer(text):
        report = {}
        for tag, line in zip(TAGS, match[0].splitlines()):
            fields = {}
            for key, value in re.findall(r"(\w+)=(-?[0-9a-f]+(?:/-?[0-9a-f]+)*)", line):
                base = 16 if tag == "quality" and key in ("bank", "xl", "ctrl3", "status") else 10
                fields[key] = [int(v, base) for v in value.split("/")] if "/" in value else int(value, base)
            report[tag] = fields
        state = report["state"]
        require(all(report[t]["id"] == state["id"] for t in TAGS), "Mixed probe session report")
        require(all(report[t]["seq"] == state["frames"] for t in ("time", "raw", "quality", "field", "angle")), "Mixed probe frame report")
        require(all(0 <= state[k] <= 0xffffffff for k in ("id", "gen", "frames", "drop")), "Probe counter overflow")
        require(all(-32768 <= v <= 32767 for v in report["raw"]["a"]) and
                all(-524288 <= v <= 524287 for v in report["raw"]["m"]), "Raw sensor range")
        result.append(report)
    return result


def identity(r):
    return (r["state"]["id"], r["state"]["gen"], r["window"]["start"], r["window"]["duration"],
            r["state"]["algo"], tuple(r["map"]["a"]), tuple(r["map"]["m"]))


def usable(r, algorithm):
    t, q = r["time"], r["quality"]
    gap = ((t["end"]-t["mag"]+0x80000000)&0xffffffff)-0x80000000
    return (t["mask"] == 31 and t["src"] in (1,2) and q["bank"] == 0 and q["xl"] == 0x20 and
            q["ctrl3"] == 0x44 and q["status"]&1 and (t["end"]-t["begin"])&0xffffffff <= 50 and
            abs(gap) <= 50 and (not algorithm or (r["field"]["valid"] and r["angle"]["output"])))


def axes(value):
    try:
        a = tuple(map(int, value.split(",")))
        require(len(a) == 3 and sorted(abs(v) for v in a) == [1, 2, 3], "Three signed unique axes required")
        det = (-1)**sum(v < 0 for v in a)
        det *= (-1)**sum(abs(a[i]) > abs(a[j]) for i in range(3) for j in range(i+1, 3))
        require(det == 1, "Axis map must be a proper rotation")
        return a
    except (ValueError, ProtocolError) as exc:
        raise argparse.ArgumentTypeError(str(exc)) from exc


class ProbeClient(MagClient):
    COMMAND_PATTERN = (r"wf_(?:pm|mag|light|steps|wrist|ui|compass) status|"
                       r"wf_cprobe (?:status|stop|freeze|inputs [1-9]\d{0,2}|start [1-9]\d{0,2} -?[123],-?[123],-?[123] -?[123],-?[123],-?[123])")

    def status(self, *, cleanup=False):
        response = self.request("wf_cprobe status", lambda line: "[cp-param] " in line, cleanup=cleanup)
        parsed = records("\n".join(response)+"\n")
        # Session zero has no axis map and is intentionally not a full report.
        if not parsed and any("[cp-state] id=0 gen=0 active=0" in line for line in response):
            return {"state": {"id": 0, "active": 0}}
        require(bool(parsed), "No complete probe status report")
        return parsed[-1]

    def compass(self):
        reply = self.request("wf_compass status", lambda line: "[compass-raw] " in line)
        values = snapshots("\n".join(reply)+"\n")
        require(bool(values), "Incomplete compass cache")
        s = values[-1]
        require(s["requested"] and s["active"] and s["valid"] and s["selftest"] and
                not (s["fault"] or s["cleanup"] or s["errors"]), "Open and keep the foreground compass healthy")
        return s

    def await_status(self, predicate, *, cleanup=False):
        deadline = time.monotonic()+2
        while True:
            current = self.status(cleanup=cleanup)
            if predicate(current):
                return current
            if time.monotonic() >= deadline:
                raise TimeoutError("Probe cache transition unconfirmed")


def run(output, transport, *, identity_check, binding, profile="smoke", acc=(1,2,3), mag=(1,2,3),
        locks=None, port=0, timeout=600, ready=None):
    require(profile in ("smoke", "axes", "calibrate"), "Unknown experiment")
    require(profile != "axes" or (acc == (1,2,3) and mag == (1,2,3)), "Raw inputs use identity maps")
    simulation = binding["simulation"]
    binding = dict(binding, profile=profile, acc_axes=acc, mag_axes=mag, axes_verified=False,
                   tool_sources=[file_record(ROOT/"scripts"/n, ROOT) for n in
                   ("compass_probe_lab.py", "mmc5603_lab.py", "compass_protocol.py", "lab_serial.py",
                    "serial_lab.py", "battery_console.py", "battery_lab.py", "lab_evidence.py")])
    if profile == "axes":
        actions = [("up", "屏幕朝天花板平放"), ("right", "3点方向的右边缘在最高处，屏幕竖直"),
                   ("front", "12点方向的上边缘在最高处，屏幕竖直"), ("down", "屏幕朝地面"),
                   ("left", "9点方向的左边缘在最高处，屏幕竖直"), ("rear", "6点方向的下边缘在最高处，屏幕竖直")]
    elif profile == "calibrate":
        actions = [("rotate", "缓慢绕三个方向转动，覆盖不同朝向；不要牵拉线或触碰金属物"),
                   ("flat", "屏幕朝上平放并保持不动"),
                   ("compare", "保持屏幕朝上，缓慢转一整圈，再小幅倾斜，观察日常页面；电脑记录候选数据")]
    else:
        actions = [("observe", "屏幕朝上保持不动；短时检查实际库和采样，不需旋转")]
    journal = Journal(output, binding=binding, mode="compass_probe")
    phases = [("prepare", "就绪")]+[(key, title) for key, title in actions]+[("stop", "停止与关口"), ("confirm_end", "末态确认")]
    session = None
    before = after = owned = None
    attempted = stopped = stop_sent = False
    lower_tick = None
    windows, health = [], {}
    outcome, detail = "passed", ""
    duration = {"smoke": 6, "axes": 90, "calibrate": 100}[profile]
    algorithm = profile != "axes"
    with Console(journal, phases, port, mode=("模拟" if simulation else "实机")+"指南针限时实验") as console:
        write_json(output/"console-ready.json", {"url": console.url, "simulation": simulation})
        print(console.url, flush=True)
        if ready:
            ready(console)
        try:
            console.prompt("prepare", "模拟无需操作设备。实机请开启临时常亮、进入指南针页面，屏幕朝上，接线留有转动余量。"+
                           "本轮轴向仍是假设；页面保持现有未校准显示。准备好后确认。", timeout)
            console.stage("prepare", "complete")
            session = RecordedSession(journal, transport, console.cancelled, ProbeClient,
                                      identity=identity_check, locks=locks)
            with session as client:
                try:
                    before = client.status()
                    require(not before["state"]["active"], "Protect existing probe")
                    foreground = client.compass()
                    health["before"] = client.snapshots()
                    health["light_before"] = client.light()
                    pm = client.request("wf_pm status", lambda line: "[pm-p1] phone=" in line)
                    pm = "\n".join(pm)
                    clock = re.search(r"\[pm-p1\] tick=(\d+) allow=1 ", pm)
                    require(clock and "tick_hz=1000" in pm, "Normal PM/clock boundary required")
                    lower_tick = int(clock[1])
                    expected = (before["state"]["id"]+1) & 0xffffffff
                    def resolve():
                        current = client.await_status(lambda r:r["state"]["id"] != before["state"]["id"], cleanup=True)
                        require(current["state"]["id"] == expected and current["state"]["gen"] == foreground["generation"] and
                                current["state"]["algo"] == int(algorithm) and current["window"]["duration"] == duration*1000 and
                                ((current["window"]["start"]-lower_tick)&0xffffffff) <= 5000 and
                                current["map"]["a"] == list(acc) and current["map"]["m"] == list(mag), "Unconfirmed or foreign probe; preserve")
                        return current
                    attempted = True
                    deadline = time.monotonic()+duration
                    command = (f"wf_cprobe start {duration} "+",".join(map(str,acc))+" "+",".join(map(str,mag))
                               if algorithm else f"wf_cprobe inputs {duration}")
                    journal.event("start_intent", command=command, expected_id=expected, generation=foreground["generation"], lower_tick=lower_tick)
                    start_error = None
                    try:
                        client.request(command, lambda line: "[cp-command] queued=" in line)
                    except (TimeoutError, ProtocolError, Cancelled) as exc:
                        if isinstance(exc, SessionChanged):
                            raise
                        start_error = exc
                        journal.event("start_ack_missing", error=repr(exc), retry=False)

                    current = resolve()
                    owned = identity(current)
                    journal.event("ownership", identity=owned)
                    if start_error is not None and not isinstance(start_error, TimeoutError):
                        raise start_error
                    require(current["state"]["active"], "Probe did not start")

                    def pump():
                        client.pump()
                        rs = records(client.text)
                        if rs and identity(rs[-1]) == owned and not rs[-1]["state"]["active"]:
                            raise ProtocolError("Probe ended during observation: "+str(rs[-1]["state"]["end"]))
                        if time.monotonic() >= deadline:
                            raise TimeoutError("Device window observation deadline")

                    for key, title in actions:
                        offset = len(client.text)
                        seconds = .2 if simulation else (2 if profile == "smoke" else 10 if profile == "axes" else {"rotate":35,"flat":4,"compare":25}[key])
                        console.automatic(key, title+"；本步自动计时。", seconds, pump=pump)
                        items = [r for r in records(client.text[offset:]) if identity(r) == owned and r["state"]["active"]]
                        unique = {r["state"]["frames"]:r for r in items if r["state"]["frames"] and usable(r, algorithm)}
                        require(len(unique) >= (1 if simulation else 4), "Missing complete position frames")
                        windows.append({"label":key,"frames":list(unique.values())})
                        if profile == "calibrate" and key == "flat":
                            current = client.status()
                            if current["param"]["valid"]:
                                client.request("wf_cprobe freeze", lambda line:"[cp-command] queued=" in line)
                                frozen = client.await_status(lambda r:identity(r) != owned or r["state"]["frozen"] or not r["state"]["active"])
                                require(identity(frozen) == owned and frozen["state"]["active"] and frozen["state"]["frozen"], "Freeze unconfirmed")
                                journal.event("parameters_frozen", parameters=frozen["param"])
                            else:
                                journal.event("parameters_unavailable", action="Continue observation without claiming calibration")
                    current = client.status()
                    require(identity(current) == owned and current["state"]["active"], "Window changed before explicit stop")
                    stop_sent = True
                    client.request("wf_cprobe stop", lambda line:"[cp-command] stop=" in line, cleanup=True)
                    after = client.await_status(lambda r:identity(r) != owned or not r["state"]["active"], cleanup=True)
                    require(identity(after) == owned and not after["state"]["active"] and after["state"]["end"] == 1, "Probe stop not confirmed")
                    stopped = True
                    health["after"] = client.snapshots()
                    health["light_after"] = client.light()
                    health["foreground_after"] = client.compass()
                finally:
                    if attempted and not stopped and not client.booted:
                        try:
                            current = resolve()
                            if owned is None:
                                owned = identity(current)
                                journal.event("cleanup_ownership_resolved", identity=owned)
                            require(identity(current) == owned, "Foreign probe; no stop")
                            if current["state"]["active"] and not stop_sent:
                                stop_sent = True
                                client.request("wf_cprobe stop", lambda line:"[cp-command] stop=" in line, cleanup=True)
                                current = client.await_status(lambda r:identity(r) != owned or not r["state"]["active"], cleanup=True)
                            require(identity(current) == owned and not current["state"]["active"], "Cleanup unconfirmed")
                            after, stopped = current, True
                            journal.event("cleanup_confirmed", end=current["state"]["end"])
                        except BaseException as exc:
                            journal.event("cleanup_unconfirmed", error=repr(exc))
            console.stage("stop", "complete")
            console.prompt("confirm_end", "本轮采集已停止、串口已关。实机请离开指南针页并关闭临时常亮，确认显示、触摸、时间正常。", timeout)
            console.stage("confirm_end", "complete")
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, Cancelled) else "timeout" if isinstance(exc, TimeoutError) else "error"
            detail = repr(exc)
            journal.event("workflow_error", outcome=outcome, error=detail)
            if stopped:
                console.stage("stop", "cleaned")
        outcome = console.seal(outcome)
        report = dict(outcome=outcome, passed=outcome == "passed", simulation=simulation, hardware_verified=False,
                      axes_verified=False, north_accuracy_verified=False, before=before, after=after, windows=windows,
                      health=health, start_attempted=attempted, stop_confirmed=stopped,
                      recovery_required=attempted and not stopped, resources_closed=bool(session and session.resources_closed), detail=detail)
        write_json(output/"workflow.json", report)
        journal.event("finished", outcome=outcome, passed=report["passed"], report_sha256=digest(output/"workflow.json"))
        console.finish(outcome, "流程结果；采样／校准与绝对精度分别评估。"+detail,
                       verification=[{"title":"完整观察窗口","passed":len(windows)==len(actions)},
                                     {"title":"探针停止确认","passed":stopped},
                                     {"title":"主机资源关闭","passed":report["resources_closed"]},
                                     {"title":"人工末态确认","passed":any(e["kind"]=="confirmed" and e.get("phase")=="confirm_end" for e in journal.events)}])
    return report


class Simulation:
    port = "SIMULATED-COMPASS-PROBE"

    def __init__(self):
        self.queue = bytearray()
        self.commands = []
        self.id = self.seq = self.active = self.algorithm = self.frozen = 0
        self.end = self.start = 0
        self.duration = 6000
        self.acc = self.mag = "1/2/3"
        self.next = 0

    def open(self):
        pass

    def close(self):
        pass

    def state(self):
        i, q = self.id, self.seq
        return (f"[cp-state] id={i} gen={int(bool(i))} active={self.active} algo={self.algorithm} frozen={self.frozen} end={self.end} frames={q} drop=0\n"
                f"[cp-window] id={i} start={self.start} duration={self.duration}\n"
                f"[cp-map] id={i} a={self.acc} m={self.mag} verified=0; experimental axis hypothesis\n"
                f"[cp-time] id={i} seq={q} mag={1000+q*10} begin={1000+q*10} end={1001+q*10} src=1 mask=31\n"
                f"[cp-raw] id={i} seq={q} a=0/0/16384 m=8000/0/500\n"
                f"[cp-quality] id={i} seq={q} bank=00 xl=20 ctrl3=44 status=01 accuracy=3 saturation=0\n"
                f"[cp-field] id={i} seq={q} corrected_uG=500000/0/0 valid={self.algorithm}\n"
                f"[cp-angle] id={i} seq={q} vendor_mdeg=0 front_mdeg=0 output={self.algorithm} projected={self.algorithm}\n"
                f"[cp-param] id={i} updates={q} valid={self.algorithm} uG=100/-200/300/500000; RAM only\n")

    def write(self, wire):
        command = wire.decode().strip()
        self.commands.append(command)
        if command == "wf_cprobe status":
            reply = self.state()
        elif command.startswith(("wf_cprobe inputs ", "wf_cprobe start ")):
            parts = command.split()
            self.id += 1
            self.active, self.algorithm, self.start, self.end, self.seq, self.frozen = 1, int(parts[1] == "start"), 1000, 0, 0, 0
            self.duration = int(parts[2])*1000
            self.acc, self.mag = (parts[3].replace(",","/"),parts[4].replace(",","/")) if self.algorithm else ("1/2/3","1/2/3")
            self.next = time.monotonic()
            reply = "[cp-command] queued=1\n"
        elif command == "wf_cprobe stop":
            self.active, self.end = 0, 1
            reply = "[cp-command] stop=1\n"
        elif command == "wf_cprobe freeze":
            self.frozen = 1
            reply = "[cp-command] queued=1\n"
        elif command == "wf_compass status":
            reply = "[compass] requested=1 active=1 valid=1 fault=0 cleanup=0 generation=1 session=1 seq=1 ms=1000 heading=0 reason=0\n[compass-raw] xyz=8000/0/500 selftest=1 stopped=0 errors=0; uncalibrated XY, mounting zero/tilt unverified\n"
        elif command == "wf_pm status":
            reply = "[pm-p1] tick=900 allow=1 off=0\n[pm-p1] screen_off=0 screen_on=0 tick_hz=1000\n[pm-p1] phone=0 phone_off=0 background=0\n"
        elif command == "wf_light status":
            reply = "[auto-light] enabled=1 active=1 valid=1 fault=0 cleanup=0 brightness=23 mlux=100000 ms=1 session=1 seq=1; uncalibrated\n"
        else:
            reply = {"wf_steps status":"[steps-hours] valid=0 sum=0\n", "wf_wrist status":"[wrist-wake] requested=1\n", "wf_ui status":"[ui-ring] current first=0,390,0x4\n"}[command]
        self.queue.extend(reply.encode())
        return len(wire)

    def read(self, count):
        time.sleep(.002)
        if self.active and time.monotonic() >= self.next:
            self.next = time.monotonic()+.02
            self.seq += 1
            self.queue.extend(self.state().encode())
        value = bytes(self.queue[:min(count, 1100)])
        del self.queue[:len(value)]
        return value


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="command", required=True)
    status = subs.add_parser("status")
    status.add_argument("--run", type=Path, required=True)
    for name in ("simulate", "device"):
        p = subs.add_parser(name)
        p.add_argument("--out", type=Path, required=True)
        p.add_argument("--profile", choices=("smoke","axes","calibrate"), default="smoke")
        p.add_argument("--acc-map", type=axes, default=(1,2,3))
        p.add_argument("--mag-map", type=axes, default=(1,2,3))
        p.add_argument("--http-port", type=int, default=0)
        p.add_argument("--timeout", type=float, default=600)
        if name == "device":
            p.add_argument("--port", required=True)
            p.add_argument("--location", required=True)
            p.add_argument("--build", type=Path, required=True)
            p.add_argument("--flash", type=Path, required=True)
            p.add_argument("--expected-bin", required=True)
            p.add_argument("--source-archive", type=Path, help="Original source ZIP for proven line-ending differences only")
            p.add_argument("--confirmed-same-board-supply", action="store_true")
    args = parser.parse_args()
    if args.command == "status":
        result = read_progress(args.run)
    else:
        if args.command == "simulate":
            transport, binding, check, locks = Simulation(), {"simulation":True}, lambda:{"kind":"simulation"}, args.out.parent/"simulation-locks"
        else:
            from serial_lab import prepare_capture
            binding, check = prepare_capture(args.build, args.flash, args.expected_bin, args.port, args.location,
                                             args.confirmed_same_board_supply, source_archive=args.source_archive)
            binding["policy"] = "Bounded explicit foreground probe only; stop identified owned window; no reset/time/PM control or persistent calibration"
            transport, locks = SerialTransport(args.port), lease_directory()
        result = run(args.out, transport, identity_check=check, binding=binding, locks=locks,
                     profile=args.profile, acc=args.acc_map, mag=args.mag_map, port=args.http_port, timeout=args.timeout)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    if result.get("passed") is False:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
