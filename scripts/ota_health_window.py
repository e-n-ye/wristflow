"""Attended 06 interruption and healthy B window. The phone operator selects both files."""
import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
from types import SimpleNamespace

from battery_console import Console
from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from ota_b_execute import recovery_check
from ota_first_install import ROOT, read_json, require
from ota_first_window import STUB, app_phase, owned_endpoint, review_wire, run_owned
from ota_health_evidence import ADDRESS, SCOPE, boot_result, image_receipt, partial_phone, saved_settings, wire_image_receipt
import ota_health_media as health
from ota_phone_board import confirmed_identity, verified_member
from ota_phone_pair import android_capture
from ota_rollback_evidence import verify_passive_wire

NATIVE = {health.ENTER: "verify-post-05-keys-and-enter-for-manual-06",
          health.INSPECT: "verify-confirmed-06-keys-and-ordinary-restart"}
PHASES = [("initial","保存原 A 记录、检查供电"),
          ("ready-partial","第一次传输前确认手机就绪"),
          ("partial","第一次选 06，约 10% 时截下进度并关闭手机蓝牙"),
          ("return","确认已断开并返回原 A"),
          ("ready-complete","核过 B 和启动记录后，打开手机蓝牙并确认就绪"),
          ("complete","第二次从头选 06，传完后等待 B 自行重启"),
          ("stable","确认新版表盘和触摸"), ("ordinary","核对启动记录并普通重启"),
          ("final","确认普通启动后的表盘、触摸、KEY1 并关闭临时常亮")]


class Window:
    def __init__(self, args, console, report):
        self.args, self.console, self.report = args, console, report
        self.live = args.mode == "capture"
        self.root = args.out.resolve()
        self.sim = args.replay.resolve() if args.replay else None
        self.bundle = args.bundle.resolve()
        self.baseline_settings = None
        self.build = read_json(args.build)
        require(self.build["kind"] == "original-board-first-install" and self.build["device_actions"] is False,
                "Unexpected native build")
        self.inputs = [*self.build["sources"], self.build["executable"], self.build["sdk_lock"]]
        verify_files(self.inputs, ROOT)
        for name, expected in (("post-05-model", health.POST_05), ("stable-06-model", health.STABLE_06)):
            require(digest(self.bundle/(name+".bin")) == expected, "Wrong 06 model")
        verify_files(read_json(self.bundle/"models.json")["files"])
        release = read_json(ROOT/"docs/evidence/2026-11-18/ota-product-health.json")
        self.report["android_file"] = verified_member(ROOT/release["phone_kit"], health.FILE, release["package_sha256"])

    def ask(self, key, message, choices=None):
        if self.live:
            return self.console.prompt(key, message, 150, choices=choices)
        self.console.journal.event("simulated_prompt", phase=key, message=message)
        return choices[0] if choices else None

    def snapshot(self, name, phase, previous_media=None):
        root = self.root/("snapshot-"+name)
        root.mkdir()
        fixture = self.sim/name/(phase+"-uart.bin") if self.sim else None
        options = SimpleNamespace(action="combined-06", mode=self.args.mode, authorize=SCOPE,
                                  health_boot=previous_media)
        setattr(options, phase+"_fixture", fixture)
        return app_phase(root, phase, options, self.console)

    def media(self, stage):
        phone = stage != "ordinary"
        action = health.ENTER if phone else health.INSPECT
        directory = self.root/stage
        directory.mkdir()
        media = directory/"media"
        model = self.bundle/("post-05-model.bin" if phone else "stable-06-model.bin")
        cfg = dict(action=action, simulation=not self.live, port="COM5" if self.live else "SIMULATED",
                   out=str(media), bundle=str(self.bundle), before=str(model), before_sha256=health.MODELS[action],
                   fixture=None if self.live else str(model), cancel=str(self.root/"cancel"), stub=str(STUB),
                   seconds=800, listen_seconds=(350 if phone else 20) if self.live else 0,
                   authorization=NATIVE[action] if self.live else "simulation")
        config = directory/"media-request.json"
        write_json(config, cfg)
        job = directory/"job"
        job.mkdir()
        ready = announced = False
        gate_error = None

        def phone_ready():
            values = saved_settings((media/"settings.bin").read_bytes())
            if self.baseline_settings is None:
                self.baseline_settings = values
            require(values == self.baseline_settings, "Saved settings changed before receiver entry")
            if (stage == "partial" and self.args.first_phone_ready) or (stage == "complete" and self.args.continue_partial):
                self.console.journal.event("operator_ready_in_chat", phase="ready-"+stage)
            else:
                self.ask("ready-"+stage, "手机在 SIFLI BLE 扫描列表时确认。广播后连接 WristFlow OTA（尾号 DA:73），"
                         "点 NOR DFU → OFFLINE，只选 "+health.FILE+
                         ("，约 10% 时截图并关闭手机蓝牙。" if stage == "partial" else "，本次保持连接传完。"))
            self.console.stage("ready-"+stage, "complete")

        def observe():
            nonlocal ready, announced, gate_error
            if phone and not ready and (media/"await-phone").exists():
                ready = True
                try:
                    phone_ready()
                    (media/"phone-ready").touch(exist_ok=False)
                except (ValueError, TimeoutError, OSError) as exc:
                    gate_error = str(exc)
                    (media/"phone-unavailable").touch(exist_ok=False)
            uart = media/"startup-uart.bin"
            if not uart.exists():
                return
            raw = uart.read_bytes()
            if phone and not announced and confirmed_identity(raw, None, ADDRESS):
                self.console.stage(stage, "running")
                announced = True
            try:
                boot_result(raw, stage)
            except ValueError:
                return
            (media/"finish-listening").touch(exist_ok=True)

        verify_files(self.inputs, ROOT)
        if self.live:
            port_identity("COM5","1-1.1")
        collect = android_capture(self.args.adb, directory) if self.live and stage == "partial" else nullcontext()
        with collect:
            code = run_owned([str(ROOT/self.build["executable"]["path"]), "--config", str(config), digest(config)],
                             job, self.console, self.root/"cancel", 825, observe=observe if self.live else None)
        events = [json.loads(s) for s in (media/"media-events.jsonl").read_bytes().splitlines()]
        require(not gate_error and code == 0 and any(e.get("health_keys_verified") for e in events),
                gate_error or "06 media check did not complete; retain the evidence and supply")
        if self.live:
            require(any(e.get("soft_reset_ack") for e in events) and any(e.get("uart_closed") for e in events)
                    and (not phone or any(e.get("phone_ready") for e in events)), "Incomplete device start or close")
            review = review_wire(media, action, self.bundle, model.read_bytes())
            write_json(directory/"wire-review.json", review)
            raw = (media/"startup-uart.bin").read_bytes()
            verify_passive_wire(media, raw)
        else:
            health.transfers(media, action, model.read_bytes())
            if phone:
                phone_ready()
                self.console.stage(stage,"running")
            raw = (self.sim/(stage+".bin")).read_bytes()
        result = boot_result(raw, stage)
        result["preferences"] = saved_settings((media/"settings.bin").read_bytes())
        require(result["preferences"] == self.baseline_settings, "Persisted preferences, layout or alarm definitions changed")
        self.report[stage] = result
        self.console.stage(stage, "complete")
        return media

    def continuity(self, label, before, after, boots):
        result = recovery_check(before, after, (after["closed_ns"]-before["closed_ns"])/1e9, False, boot_deltas=(boots,))
        require(after["records"]["current"]["failures"] == before["records"]["current"]["failures"],
                "Runtime save failure count changed")
        self.report[label] = result

    def resume_partial(self):
        prior = self.args.continue_partial.resolve()
        require(not (prior/"complete").exists() and read_json(prior/"binding.json")["authorization"] == SCOPE and
                read_json(prior/"window.json")["passed"] is False, "Expected an interrupted combined window before second entry")
        part = prior/"partial"
        cfg = read_json(part/"media-request.json")
        require(cfg["simulation"] is False and cfg["port"] == "COM5" and cfg["action"] == health.ENTER and
                cfg["before_sha256"] == health.POST_05, "Wrong partial window binding")
        media = part/"media"
        events = [json.loads(s) for s in (media/"media-events.jsonl").read_bytes().splitlines()]
        require(events[-1].get("ok") and any(e.get("phone_ready") for e in events) and
                any(e.get("uart_closed") for e in events), "Previous receiver did not finish and close")
        model = (self.bundle/"post-05-model.bin").read_bytes()
        write_json(self.root/"prior-wire-review.json", review_wire(media, health.ENTER, self.bundle, model))
        raw = (media/"startup-uart.bin").read_bytes()
        verify_passive_wire(media, raw)
        result = boot_result(raw, "partial")
        photo = read_json(prior/"reviewed-progress.json")
        receipt = wire_image_receipt(media, (prior/"partial-progress.image").read_bytes(), photo["percent"],
                                     photo["selection_time"], photo["discovery_time"])
        require(receipt == photo, "Prior phone evidence differs")
        before = read_json(prior/"snapshot-initial/app-before/before-snapshot.json")
        verify_files([before["raw"]])
        self.baseline_settings = saved_settings((media/"settings.bin").read_bytes())
        result["preferences"] = self.baseline_settings
        self.report.update(partial=result, partial_progress_image=receipt, prior_partial=str(prior),
                           operator_confirmed_in_chat=True)
        # The archived partial transfer may supply exactly one second attempt.
        claim = prior/"continued-partial.json"
        if self.args.after_declined_entry:
            declined = self.args.after_declined_entry.resolve()
            require(Path(read_json(claim)["next"]).resolve() == declined and
                    read_json(declined/"binding.json")["authorization"] == SCOPE and
                    read_json(declined/"window.json")["passed"] is False, "Wrong unused second entry")
            old = declined/"complete/media"
            events = [json.loads(s) for s in (old/"media-events.jsonl").read_bytes().splitlines()]
            require(events[-1].get("ok") and any(e.get("uart_closed") for e in events) and
                    [e["phone_ready"] for e in events if "phone_ready" in e] == [False] and
                    not any(e.get("intent") == "one-shot-receiver" for e in events),
                    "Cannot reuse an entry that may already have broadcast")
            old_raw = (old/"startup-uart.bin").read_bytes()
            verify_passive_wire(old, old_raw)
            require(b"[ota-radio]" not in old_raw and b"probe=12027f35" in old_raw and
                    b"[product] storage=ready restore=yes " in old_raw, "Stock A ordinary return missing")
            review_wire(old, health.ENTER, self.bundle, model)
            require(saved_settings((old/"settings.bin").read_bytes()) == self.baseline_settings,
                    "Semantic settings mismatch in declined entry")
            previous = read_json(declined/"snapshot-returned/app-before/before-snapshot.json")
            verify_files([previous["raw"]])
            self.continuity("original_partial_return",before,previous,1)
            before = previous
            claim = declined/"continued-unused-entry.json"
        with claim.open("x", encoding="utf-8") as stream:
            json.dump({"next":str(self.root)},stream)
        for key in ("initial", "ready-partial", "partial"):
            self.console.stage(key, "complete")
        return before

    def first_transfer(self):
        self.console.stage("initial","running")
        before = self.snapshot("initial","before")
        self.console.stage("initial","complete")
        self.media("partial")
        log = (self.root/"partial" if self.live else self.sim)/"android-dfu.log"
        raw = log.read_text(encoding="utf-8", errors="replace")
        android = partial_phone(raw) if "sifli-DFU: " in raw else dict(success_ack_rows=0, completed=None, log_available=False)
        self.report["partial_phone"] = android
        option = health.FILE+"，进度大于 0% 且小于 100% 时已关闭手机蓝牙"
        chosen = self.ask("return", "核对第一次所选文件和断开时机。请确认原表盘和触摸，开临时常亮。"
                          "若手机没有记录成功 DATA 回执，请将断开前的进度截图发到聊天。", [option,"未能确认"])
        require(chosen == option, "Interrupted file or positive progress unconfirmed")
        if not android["success_ack_rows"]:
            # SDK 1.1.38 can omit successful DATA rows. Preserve the original
            # phone image after the operator/host has inspected this session's
            # timestamp, filename selection and 0 < pro < 100. Never invent a count.
            folder = self.root if self.live else self.sim
            receipt = folder/"partial-progress.json"
            if self.live and not receipt.is_file():
                self.console.automatic("return","请发送断开前的进度截图，电脑核对并保存后会自动继续。", 180,
                                       until=receipt.is_file)
            entry = read_json(receipt)
            image = (folder/"partial-progress.image").read_bytes()
            checked = (image_receipt(raw, image, entry["percent"], entry["selection_time"]) if "sifli-DFU: " in raw else
                       wire_image_receipt(self.root/"partial/media", image, entry["percent"],
                                          entry["selection_time"], entry["discovery_time"]))
            require(checked == entry, "Reviewed partial image or Android session changed")
            self.report["partial_progress_image"] = checked
        return before

    def run(self):
        before = self.resume_partial() if self.args.continue_partial else self.first_transfer()
        returned = self.snapshot("returned","before")
        self.continuity("interrupted_return", before, returned, 1)
        self.console.stage("return","complete")
        b_media = self.media("complete")
        require(self.report["complete"]["settings"] == self.report["partial"]["settings"], "B restored different settings")
        require(self.ask("stable","请确认第二次所选文件。新版表盘和触摸正常后开启临时常亮，供保存记录和普通重启。",
                         [health.FILE, "其他文件或表盘异常"]) == health.FILE, "Successful file or display unconfirmed")
        stable = self.snapshot("confirmed","before",b_media)
        self.continuity("healthy_return", returned, stable, 2)
        self.console.stage("stable","complete")
        self.console.stage("ordinary","running")
        b_media = self.media("ordinary")
        require(self.report["ordinary"]["settings"] == self.report["partial"]["settings"], "Ordinary B restored different settings")
        self.ask("final","普通重启已进入新版 B。请确认表盘和触摸，开临时常亮供最后一次读记录。")
        final = self.snapshot("final","after",b_media)
        self.continuity("ordinary_restart", stable, final, 1)
        self.ask("final","记录已保存。请关闭临时常亮，回到表盘，确认显示、触摸和 KEY1。")
        self.console.stage("final","complete")


def run(args):
    live = args.mode == "capture"
    require((live and os.name == "nt" and args.authorize == SCOPE and args.adb and not args.replay) or
            (not live and args.authorize == "simulation" and args.replay and not args.adb and not args.continue_partial),
            "Wrong 06 scope or inputs")
    root = args.out.resolve()
    journal = Journal(root, binding=dict(simulation=not live, authorization=args.authorize,
                      build=file_record(args.build), bundle=file_record(args.bundle/"models.json")), mode="06-window")
    report = dict(passed=False, hardware_verified=False, simulation=not live)
    with Console(journal, PHASES, mode="06 断连、重传与健康确认" if live else "06 离线协作验证") as console:
        print(console.url, flush=True)
        try:
            w = Window(args, console, report)
            if live:
                os.startfile(console.url)
            with owned_endpoint("COM5" if live else "SIMULATED-06", report):
                w.run()
            require(console.seal("passed") == "passed", "Workflow incomplete or cancelled")
            report.update(passed=True, hardware_verified=live)
        except (Exception, KeyboardInterrupt) as exc:
            report["error"] = repr(exc)
        finally:
            write_json(root/"window.json", report)
            console.finish("passed" if report["passed"] else "error", report.get("error", "本轮证据已保存。"))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("capture","simulate"))
    parser.add_argument("--authorize", required=True)
    parser.add_argument("--first-phone-ready", action="store_true", help="The operator explicitly confirmed the first scan in chat")
    parser.add_argument("--continue-partial", type=Path, help="Closed partial window; operator confirmed stock A, keepawake and second scan")
    parser.add_argument("--after-declined-entry", type=Path, help="Verified ordinary A return with no receiver request, separately authorized")
    for name in ("build","bundle","out","replay","adb"):
        parser.add_argument("--"+name, type=Path, required=name in ("build","bundle","out"))
    result = run(parser.parse_args())
    print(json.dumps(result, ensure_ascii=False), flush=True)
    raise SystemExit(0 if result["passed"] else 1)
