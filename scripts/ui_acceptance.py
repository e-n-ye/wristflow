"""Bounded operator acceptance of the 2026-10-25 UI changes; no device control."""
from __future__ import annotations

import argparse
from pathlib import Path
import time

from battery_console import Cancelled, Console
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from lab_serial import RecordedClient, RecordedSession, SerialTransport
from serial_lab import prepare_capture


STEPS = [
    ("temperature", "天气组件温度与选择器", "在主页长按已有天气组件进入编辑，检查温度后的°正常、没有方框。点天气槽位查看同尺寸的两种配色预览，滚动后返回；本步不用更换组件。退出编辑后触摸和返回正常。正常才确认；有问题请取消本轮并在聊天说明。"),
    ("countdown", "倒计时配色与操作", "打开倒计时，设置约30秒。检查运行页左侧取消为灰色、右侧暂停为蓝色；暂停、继续各一次。到期后检查左侧关闭灰色、右侧重复蓝色，点关闭退出，确认没有留下运行中的倒计时。"),
    ("breathing", "呼吸图标与训练", "查看呼吸的同心圆应用图标和灰色开始键。选1分钟开始，横滑到操作页查看灰色结束／重新开始；试一次重新开始，再自然训练满1分钟，检查结果页灰色完成键，点完成退出。不要只等网页倒计时，本步骤由你观察手表结果。"),
    ("stopwatch", "秒表50条计次与短睡", "1. 开始秒表，连续计次至50条，检查最新在上、列表可滚动、满后有上限提示且不能再加。\n2. 暂停后继续，确认时间和计次保留。\n3. 关闭临时常亮，回到运行中的秒表，等自然黑屏约5秒后按一次KEY1，确认仍在秒表且计次保留。\n4. 暂停并重置，确认时间和列表清空，再退出。可在后续页面操作时重新开启临时常亮，最后关闭。"),
    ("battery", "电池页风格与返回", "打开电池页，比较电压信息卡与功能卡的灰底、圆角和文字层级，确认字形齐全、没有裁切或重叠。查看说明后正常返回。本步不用开启长续航或修改充电设置。"),
    ("weather", "天气五页与分页", "使用手机刚更新的真实天气：检查当前、小时、每日、指数、日升日落五页。小时横滑到第6组再返回，未来天气横滑到第2组，核分页点和末页两行；中间区域上下滑应能切换大页。检查温度、风速单位、图标／中文无方框，正常边缘返回并重入一次。缺失字段应保持--，不要用样例数据替代。若天气仍空或操作异常，请取消并在聊天说明。"),
    ("final", "退出与末态", "回到表盘，确认显示、触摸、KEY1及时间正常，临时常亮已关闭，倒计时／呼吸／秒表测试已结束。原抬腕、自动亮度和AOD偏好保持。请在屏幕仍亮时确认，程序只读收尾并关闭串口；无需额外重启。"),
]


class Observer(RecordedClient):
    COMMAND_PATTERN = r"wf_(?:ui|steps|wrist|light|pm) status|show_date"
    BOOT_MARKERS = (b"[product] storage=", b"SFBL")


def run(args):
    simulated = args.port is None
    if simulated:
        binding, identity = {"simulation": True}, None
    else:
        if not all((args.result, args.flash, args.expected_bin, args.source_archive, args.confirmed)):
            raise ValueError("Device mode needs build/flash/archive/hash and current operator confirmation")
        binding, identity = prepare_capture(args.result, args.flash, args.expected_bin,
                                            args.port, args.location, args.confirmed,
                                            source_archive=args.source_archive)
    own_source = file_record(Path(__file__))
    binding.update(adapter=own_source, policy="Passive during manual actions; cached status at boundaries only. No PM, reset, clock, charger or sensor control.")
    journal = Journal(args.out, binding=binding, mode="ui-acceptance")
    report = {"simulation": simulated, "hardware_verified": False,
              "operator_confirmed": [], "snapshot_warnings": [], "device_control_sent": False}
    started = time.monotonic()
    with Console(journal, [(k, t) for k, t, _ in STEPS],
                 mode="UI验收模拟（不操作手表）" if simulated else "①②③同版UI实机验收") as console:
        print(console.url, flush=True)
        session = client = None
        def pump():
            if console.cancelled.is_set():
                raise Cancelled("Operator cancelled")
            if time.monotonic() - started > 1200:
                raise TimeoutError("Bounded UI acceptance reached 20 minutes")
            if client:
                client.read_chunk(); client.take_lines(); client.last_bytes.clear()
            else:
                time.sleep(.02)
        def snapshots():
            if not client:
                return
            commands = [("show_date", "2026"), ("wf_steps status", "[steps-hours]"),
                        ("wf_wrist status", "[wrist-wake]"), ("wf_light status", "[auto-light]"),
                        ("wf_ui status", "[ui-ring] current settled="), ("wf_pm status", "[pm-p1] tick=")]
            for command, marker in commands:
                try:
                    client.request(command, lambda line, marker=marker: marker in line)
                except TimeoutError as exc:
                    report["snapshot_warnings"].append({"command": command, "error": str(exc)})
                    journal.event("snapshot_timeout", command=command)
        try:
            if not simulated:
                def guarded_identity():
                    verify_files([own_source])
                    return identity()
                session = RecordedSession(journal, SerialTransport(args.port), console.cancelled,
                                          Observer, identity=guarded_identity, timeout=1)
                client = session.__enter__()
                snapshots()
            for key, title, message in STEPS:
                console.prompt(key, ("【仅模拟】" if simulated else "") + message, 600, pump=pump)
                report["operator_confirmed"].append(key)
                journal.event("operator_observation", phase=key, title=title, statement=message)
                console.stage(key, "complete")
            snapshots()
            outcome = "passed"
        except BaseException as exc:
            outcome = "cancelled" if isinstance(exc, (Cancelled, KeyboardInterrupt)) else "timeout" if isinstance(exc, TimeoutError) else "error"
            report["error"] = repr(exc)
            journal.event("workflow_error", error=repr(exc))
        finally:
            if session:
                session.__exit__()
            report["port_closed"] = session.port_closed if session else None
            report["resources_closed"] = session.resources_closed if session else True
        outcome = console.seal(outcome)
        report.update(outcome=outcome, workflow_passed=outcome == "passed",
                      physical_observations_confirmed=not simulated and outcome == "passed")
        write_json(args.out / "workflow.json", report)
        journal.event("finished", outcome=outcome, report_sha256=digest(args.out / "workflow.json"))
        console.finish(outcome, "本轮流程结束；人工观察与UART分别核验。取消／异常时请退出测试功能并关闭临时常亮；程序已释放其主机资源。")
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
    raise SystemExit(run(p.parse_args()))
