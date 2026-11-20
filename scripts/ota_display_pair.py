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
from ota_health_evidence import boot_result, display_a_reference
from ota_health_media import STABLE_06
from ota_rollback_evidence import verify_passive_wire

INSTALL = "install-display-pair"
RESTORE = "restore-display-pair"
INSTALL_A = "install-display-pair-a"
RESTORE_A = "restore-display-pair-a"
STABLE_A = "f242e48a6df6215e215461ec36137fa82aa3a98c0b68f7a9e8ad4eceaa59003b"
BOOT = (0x10000, 0x1F000)
RECEIVER = (0x820000, 0x8A0000)
CHUNK = 0x10000
PROTECTED = ((0, 0x10000), (0xDA0000, 0xDA4000), (0x81F000, 0x820000))
A_SIGNATURE = (0x41F000, 0x420000)


def stable_slot(action):
    return "A" if action in (INSTALL_A, RESTORE_A) else "B"


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
    known_sha = sha(known)
    require(len(known) == 0x1000000 and known_sha in (STABLE_06, STABLE_A), "Wrong stable display model")
    model_name = "stable-a.bin" if known_sha == STABLE_A else "stable-b.bin"
    out.mkdir(parents=True)
    save(out / "boot.bin", boot.ljust(BOOT[1] - BOOT[0], b"\xff"))
    save(out / "receiver.bin", receiver.ljust(RECEIVER[1] - RECEIVER[0], b"\xff"))
    save(out / model_name, known)
    records = [file_record(out / name) for name in ("boot.bin", "receiver.bin", model_name)]
    write_json(out / "pair.json", {"files": records, "receiver_bytes": len(receiver), "boot_bytes": len(boot),
                                  "receiver_sha256": sha(receiver), "boot_sha256": sha(boot)})


def chunks(region):
    start, end = region
    return [(off, min(off + CHUNK, end)) for off in range(start, end, CHUNK)]


def pair_transfers(directory, action, old, boot, receiver):
    """Rebuild the expected transaction from the same-window preflight files."""
    restoring = action in (RESTORE, RESTORE_A)
    protected = PROTECTED + ((A_SIGNATURE,) if stable_slot(action) == "A" else ())
    reads, writes, final = [], [], []
    current = {}
    for region in (protected[0], BOOT, RECEIVER, *protected[1:]):
        for start, end in chunks(region):
            key = f"key-{start:08x}"
            incoming = (directory / (key + ".bin")).read_bytes()
            require(len(incoming) == end - start, "Short live NOR block")
            if not restoring or region in protected:
                require(incoming == old[start:end], f"Stable display preflight changed at {start:#x}")
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
    slot = stable_slot(args.action)
    restoring = args.action in (RESTORE, RESTORE_A)
    expected_model = STABLE_A if slot == "A" else STABLE_06
    install_action = INSTALL_A if slot == "A" else INSTALL
    require(args.authorize == (SCOPE[args.action] if live else "simulation") and
            (not live or os.name == "nt" and not args.fixture) and
            args.on_site_confirmed == live and
            bool(args.prior) == restoring and
            (live or not restoring or bool(args.fixture)) and
            bool(args.stable_uart) == (live and not restoring), "Unexpected device scope or input")
    bundle, out = args.bundle.resolve(), args.out.resolve()
    pair = read_json(bundle / "pair.json")
    verify_files(pair["files"])
    boot, receiver, model = [bundle / name for name in ("boot.bin", "receiver.bin", f"stable-{slot.lower()}.bin")]
    require([str(p) for p in (boot, receiver, model)] == [r["path"] for r in pair["files"]],
            "Bundle must use the frozen paths")
    old = model.read_bytes()
    require(sha(old) == expected_model, f"Stable {slot} model differs")
    if args.stable_uart and slot == "A":
        display_a_reference(args.stable_uart.read_bytes())
    if args.prior:
        prior = args.prior.resolve()
        prior_request = read_json(prior / "media-request.json")
        require(prior_request["action"] == install_action and prior_request["simulation"] == (not live)
                and prior_request["before_sha256"] == expected_model
                and prior_request["display_pair_sha256"] == [digest(boot), digest(receiver)], "Wrong installation window")
        events = (prior / "media/media-events.jsonl").read_text(encoding="utf-8").splitlines()
        require(any(json.loads(line).get("intent") in ("boot", "receiver") for line in events),
                "Installation has no Flash write intent")
        for start, end in chunks(BOOT) + chunks(RECEIVER):
            require((prior / f"media/key-{start:08x}.bin").read_bytes() == old[start:end],
                    f"Installation preimage differs from stable {slot}")
    build = read_json(args.build.resolve())
    require(build["kind"] == "original-board-first-install" and not build["device_actions"], "Unexpected bridge")
    verify_files([*build["sources"], build["executable"], build["sdk_lock"]], ROOT)
    journal = Journal(out, binding=dict(action=args.action, simulation=not live, pair=pair), mode="ota-display-pair")
    report = {"passed": False, "simulation": not live, "hardware_verified": False}
    phases = [("preflight", f"核对同一块表及稳定 {slot}"), ("pair", "写入并读回配套镜像"),
              ("normal", f"从串口核对稳定 {slot} 启动")]
    with Console(journal, phases, mode="OTA 接收器和配套 boot") as console:
        try:
            with owned_endpoint("COM5" if live else "SIMULATED-DISPLAY", report):
                if live:
                    port_identity("COM5", "1-1.1")
                    journal.event("operator_ready_in_chat", phase="preflight")
                    if not restoring:
                        app_phase(out, "before", SimpleNamespace(action=args.action, mode="capture",
                                  authorize=args.authorize, health_boot=args.stable_uart.resolve()), console)
                else:
                    journal.event("simulated_prompt", phase="preflight")
                console.stage("preflight", "complete")
                media, request = out / "media", out / "media-request.json"
                fixture = args.fixture.resolve() if args.fixture else model
                cfg = dict(action=args.action, simulation=not live, port="COM5" if live else "SIMULATED",
                           out=str(media), bundle=str(bundle), before=str(model), before_sha256=expected_model,
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
                    boot_result(uart, "ordinary", stable_slot=slot)
                else:
                    expected = bytearray(fixture.read_bytes())
                    for _, start, part in writes:
                        expected[start:start + len(part)] = part
                    require((media / "simulated-nor.bin").read_bytes() == expected, "Simulation touched unrelated Flash")
                report["reads"], report["writes"] = len(reads), len(writes)
                console.stage("pair", "complete")
                if live:
                    journal.event("operator_check_in_chat", phase="normal")
                console.stage("normal", "complete")
            require(console.seal("passed") == "passed", "Workflow incomplete")
            report.update(passed=True, hardware_verified=live, operator_check_pending=live)
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
        window.add_argument("--action", choices=(INSTALL, RESTORE, INSTALL_A, RESTORE_A), required=True)
        window.add_argument("--authorize", required=True)
        window.add_argument("--on-site-confirmed", action="store_true",
                            help="Record the operator's existing confirmation of board, power and presence in chat")
        for name in ("build", "bundle", "out", "stable-uart", "fixture", "prior"):
            window.add_argument("--" + name, type=Path, required=name in ("build", "bundle", "out"))
    args = parser.parse_args()
    if args.operation == "prepare":
        prepare(args.receiver_result.resolve(), args.boot_result.resolve(), args.model.resolve(), args.out.resolve())
    else:
        result = run(args)
        print(json.dumps(result, ensure_ascii=False), flush=True)
        raise SystemExit(0 if result["passed"] else 1)
