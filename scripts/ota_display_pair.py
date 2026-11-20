"""Prepare, rehearse or exchange the display receiver and its bound boot."""
import argparse
import json
import os
from pathlib import Path
from types import SimpleNamespace

from battery_console import Console
from battery_device import port_identity
from battery_lab import Journal
from lab_evidence import digest, file_record, verify_files, write_json
from ota_first_install import ROOT, read_json, require, save, sha
from ota_first_window import SCOPE, STUB, app_phase, owned_endpoint, review_transfers, review_trial_commands, run_owned
from ota_health_evidence import boot_result
from ota_health_media import STABLE_06
from ota_rollback_evidence import verify_passive_wire

INSTALL = "install-display-pair"
RESTORE = "restore-display-pair"
BOOT = (0x10000, 0x1F000)
RECEIVER = (0x820000, 0x8A0000)
CHUNK = 0x10000
PROTECTED = ((0, 0x10000), (0xDA0000, 0xDA4000), (0x81F000, 0x820000))
PHASES = [("preflight", "核对同一块表及稳定 B"), ("pair", "写入并读回配套镜像"),
          ("normal", "确认稳定 B 表盘已返回")]


def frozen_build(path, kind):
    record = read_json(path)
    lock = read_json(ROOT / "sdk.lock.json")
    require(record["kind"] == kind and record["artifact_validation_passed"] is True and
            record["sdk_commit"] == lock["sdk"]["commit"] and record["board"] == lock["board"] and
            not record["sdk_changes"], "Unexpected firmware build")
    [binary] = [item for item in record["artifacts"] if Path(item["path"]).name == "main.bin"]
    data = (path.parent / "frozen/main.bin").read_bytes()
    require(len(data) == binary["bytes"] and sha(data) == binary["sha256"], "Frozen binary differs from build")
    return data


def prepare(receiver_result, boot_result_path, model, out):
    receiver = frozen_build(receiver_result, "standalone-ota-android-receiver")
    boot = frozen_build(boot_result_path, "ota-ab-boot")
    check = read_json(boot_result_path.parent / "elf-check.json")
    require(check["passed"] and check["receiver_bytes"] == len(receiver) and check["receiver_sha256"] == sha(receiver)
            and check["bin_bytes"] == len(boot) and check["bin_sha256"] == sha(boot)
            and len(boot) <= 58496 and 0x60000 < len(receiver) <= RECEIVER[1] - RECEIVER[0],
            "Boot does not bind the expanded receiver")
    known = model.read_bytes()
    require(len(known) == 0x1000000 and sha(known) == STABLE_06, "Wrong stable B model")
    out.mkdir(parents=True)
    save(out / "boot.bin", boot.ljust(BOOT[1] - BOOT[0], b"\xff"))
    save(out / "receiver.bin", receiver.ljust(RECEIVER[1] - RECEIVER[0], b"\xff"))
    save(out / "stable-b.bin", known)
    records = [file_record(out / name) for name in ("boot.bin", "receiver.bin", "stable-b.bin")]
    write_json(out / "pair.json", {"files": records, "receiver_bytes": len(receiver), "boot_bytes": len(boot),
                                  "receiver_sha256": sha(receiver), "boot_sha256": sha(boot)})


def chunks(region):
    start, end = region
    return [(off, min(off + CHUNK, end)) for off in range(start, end, CHUNK)]


def pair_transfers(directory, action, old, boot, receiver):
    """Rebuild the expected transaction from the same-window preflight files."""
    restoring = action == RESTORE
    reads, writes, final = [], [], []
    current = {}
    for region in (PROTECTED[0], BOOT, RECEIVER, *PROTECTED[1:]):
        for start, end in chunks(region):
            key = f"key-{start:08x}"
            incoming = (directory / (key + ".bin")).read_bytes()
            require(len(incoming) == end - start, "Short live NOR block")
            if not restoring or region in PROTECTED:
                require(incoming == old[start:end], "Stable B preflight changed")
            current[start] = incoming
            reads.append((key, start, incoming))
    regions = [("receiver", RECEIVER, receiver), ("boot", BOOT, boot)]
    if restoring:
        regions = [(role, region, old[region[0]:region[1]]) for role, region, _ in reversed(regions)]
    for role, region, data in regions:
        for start, end in chunks(region):
            target = data[start - region[0]:end - region[0]]
            if target != current[start]:
                writes.append((role, start, target))
                reads.append((f"{role}-{start:08x}", start, target))
            final.append((f"pair-{start:08x}", start, target))
    reads.extend(final)
    require(all((directory / (name + ".bin")).read_bytes() == expected for name, _, expected in reads),
            "Independent display-pair readback differs")
    return reads, writes


def run(args):
    live = args.operation == "capture"
    require(args.authorize == (SCOPE[args.action] if live else "simulation") and
            (not live or os.name == "nt" and not args.fixture) and
            bool(args.prior) == (args.action == RESTORE) and
            (live or args.action != RESTORE or bool(args.fixture)) and
            bool(args.stable_uart) == (live and args.action == INSTALL), "Unexpected device scope or input")
    bundle, out = args.bundle.resolve(), args.out.resolve()
    pair = read_json(bundle / "pair.json")
    verify_files(pair["files"])
    boot, receiver, model = [bundle / name for name in ("boot.bin", "receiver.bin", "stable-b.bin")]
    require([str(p) for p in (boot, receiver, model)] == [r["path"] for r in pair["files"]],
            "Bundle must use the frozen paths")
    old = model.read_bytes()
    require(sha(old) == STABLE_06, "Stable B model differs")
    if args.prior:
        prior = args.prior.resolve()
        prior_request = read_json(prior / "media-request.json")
        require(prior_request["action"] == INSTALL and prior_request["simulation"] == (not live)
                and prior_request["display_pair_sha256"] == [digest(boot), digest(receiver)], "Wrong installation window")
        events = (prior / "media/media-events.jsonl").read_text(encoding="utf-8").splitlines()
        require(any(json.loads(line).get("intent") in ("boot", "receiver") for line in events),
                "Installation has no Flash write intent")
        for start, end in chunks(BOOT) + chunks(RECEIVER):
            require((prior / f"media/key-{start:08x}.bin").read_bytes() == old[start:end],
                    "Installation preimage differs from stable B")
    build = read_json(args.build.resolve())
    require(build["kind"] == "original-board-first-install" and not build["device_actions"], "Unexpected bridge")
    verify_files([*build["sources"], build["executable"], build["sdk_lock"]], ROOT)
    journal = Journal(out, binding=dict(action=args.action, simulation=not live, pair=pair), mode="ota-display-pair")
    report = {"passed": False, "simulation": not live, "hardware_verified": False}
    with Console(journal, PHASES, mode="OTA 接收器和配套 boot") as console:
        print(console.url, flush=True)
        try:
            if live:
                os.startfile(console.url)
            with owned_endpoint("COM5" if live else "SIMULATED-DISPLAY", report):
                if live:
                    port_identity("COM5", "1-1.1")
                    console.prompt("preflight", "确认原黄山派接原电池和板载 USB、接线固定、人员在旁。", 150)
                    if args.action == INSTALL:
                        app_phase(out, "before", SimpleNamespace(action=args.action, mode="capture",
                                  authorize=args.authorize, health_boot=args.stable_uart.resolve()), console)
                else:
                    journal.event("simulated_prompt", phase="preflight")
                console.stage("preflight", "complete")
                media, request = out / "media", out / "media-request.json"
                fixture = args.fixture.resolve() if args.fixture else model
                cfg = dict(action=args.action, simulation=not live, port="COM5" if live else "SIMULATED",
                           out=str(media), bundle=str(bundle), before=str(model), before_sha256=STABLE_06,
                           fixture=None if live else str(fixture), cancel=str(out / "cancel"), stub=str(STUB),
                           seconds=180, listen_seconds=25 if live else 0, authorization=args.authorize,
                           display_pair_sha256=[digest(boot), digest(receiver)])
                write_json(request, cfg)
                code = run_owned([str(ROOT / build["executable"]["path"]), "--config", str(request), digest(request)],
                                 out, console, out / "cancel", 195)
                require(code == 0, "Bridge did not complete. Review the recorded phase before recovery")
                reads, writes = pair_transfers(media, args.action, old, boot.read_bytes(), receiver.read_bytes())
                if live:
                    trace = review_transfers(media, reads, writes)
                    review_trial_commands((media / "tx.bin").read_bytes(), reads, writes, trace["read_retries"])
                    uart = (media / "startup-uart.bin").read_bytes()
                    verify_passive_wire(media, uart)
                    boot_result(uart, "ordinary")
                else:
                    expected = bytearray(fixture.read_bytes())
                    for _, start, part in writes:
                        expected[start:start + len(part)] = part
                    require((media / "simulated-nor.bin").read_bytes() == expected, "Simulation touched unrelated Flash")
                report["reads"], report["writes"] = len(reads), len(writes)
                console.stage("pair", "complete")
                if live:
                    console.prompt("normal", "确认稳定 B 表盘、触摸和 KEY1。", 150)
                else:
                    journal.event("simulated_prompt", phase="normal")
                console.stage("normal", "complete")
            require(console.seal("passed") == "passed", "Workflow incomplete")
            report.update(passed=True, hardware_verified=live)
        except (Exception, KeyboardInterrupt) as exc:
            report["error"] = repr(exc)
        finally:
            write_json(out / "window.json", report)
            console.finish("passed" if report["passed"] else "error", report.get("error", "窗口记录已保存。"))
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="operation", required=True)
    packing = modes.add_parser("prepare")
    for name in ("receiver-result", "boot-result", "model", "out"):
        packing.add_argument("--" + name, type=Path, required=True)
    for mode in ("simulate", "capture"):
        window = modes.add_parser(mode)
        window.add_argument("--action", choices=(INSTALL, RESTORE), required=True)
        window.add_argument("--authorize", required=True)
        for name in ("build", "bundle", "out", "stable-uart", "fixture", "prior"):
            window.add_argument("--" + name, type=Path, required=name in ("build", "bundle", "out"))
    args = parser.parse_args()
    if args.operation == "prepare":
        prepare(args.receiver_result.resolve(), args.boot_result.resolve(), args.model.resolve(), args.out.resolve())
    else:
        result = run(args)
        print(json.dumps(result, ensure_ascii=False), flush=True)
        raise SystemExit(0 if result["passed"] else 1)
