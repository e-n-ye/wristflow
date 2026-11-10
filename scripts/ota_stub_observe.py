"""Bound official-stub observation. Capture has no Flash write/erase entry."""
import argparse
import json
from pathlib import Path
import subprocess
import time

from lab_evidence import digest, file_record, verify_files, write_json
from ota_backup_review import review_pair

ROOT = Path(__file__).resolve().parents[1]
LIMITS = dict(bind=30, before=60, rom=600, after=60, grace=10)
GEOMETRY_LIMITS = dict(bind=30, before=60, rom=60, after=60, grace=10)
SCENARIOS = ("success", "fragmented", "short-read", "geometry-bad-pointer", "lcpu-running", "register-read-fail", "immutable-changed", "pair-changed", "crc-fail", "hang-read", "rtc-reset")
REVISION = "be2adab37bfb0ef4eb623b7e47e7b8693918b1e1"
STUB_SHA = "d4db9c27a0539da431fd253ee4b09c8d3ddb033bf0dc1ae4cc3bf322fa069e43"
HEADERS = ("register.h", "lpsys_aon.h", "lpsys_rcc.h", "dmac.h", "extdma.h")


def load(path):
    path = Path(path)
    if path.stat().st_size > 2 * 1024 * 1024:
        raise ValueError("Observer JSON too large")
    return json.loads(path.read_text(encoding="utf-8"))


def prepare(options):
    if not isinstance(options, dict) or set(options) != {"build", "first", "second"}:
        raise ValueError("Explicit observer build and original backup pair required")
    options = {k: str(Path(v).resolve()) for k, v in options.items()}
    build = load(options["build"])
    if (not isinstance(build, dict) or build.get("kind") != "ota-stub-observer" or build.get("passed") is not True
            or build.get("source_revision") != REVISION or build.get("flash_write_entry") is not False
            or build.get("device_actions") is not False or build.get("stub", {}).get("sha256") != STUB_SHA):
        raise ValueError("A successful original-library observer build is required")
    records = build["sources"] + [build["executable"], build["sdk_lock"], build["stub"]]
    verify_files(records, ROOT)
    pair = review_pair(options["first"], options["second"])
    if not pair["passed"]:
        raise ValueError("Original backup pair rejected")
    records += [file_record(options[k]) for k in ("build", "first", "second")]
    records += [file_record(ROOT / "vendor/SiFli-SDK/drivers/cmsis/sf32lb52x" / name) for name in HEADERS]
    records.append(file_record(ROOT / "vendor/SiFli-SDK/drivers/cmsis/Include/usart.h"))
    records = [dict(item, path=str((ROOT / item["path"]).resolve())) for item in records]
    return options, records


def sample_summary(value):
    """Describe finite observations without treating them as a write permit."""
    expected = [base + 0x14 * i for base in (0x50081008, 0x40001008) for i in range(8)] + [0x50001008]
    if (not isinstance(value, dict) or not isinstance(value.get("channels"), list)
            or any(not isinstance(channel, dict) for channel in value["channels"])):
        raise ValueError("Invalid register snapshot shape")
    if (any(type(value.get(key)) is not int or not 0 <= value[key] <= 0xffffffff for key in ("pmr", "rstr1"))
            or [c.get("address") for c in value.get("channels", [])] != expected):
        raise ValueError("Incomplete or foreign register snapshot")
    enabled = []
    for channel in value["channels"]:
        if any(type(channel.get(k)) is not int or not 0 <= channel[k] <= 0xffffffff
               for k in ("ccr", "count", "source", "destination")):
            raise ValueError("Invalid DMA register word")
        if channel["ccr"] & 1:
            endpoints = (channel["source"], channel["destination"])
            flash = any(0x12000000 <= address < 0x13000000 for address in endpoints)
            uart = any(address in (0x50084024, 0x50084028) for address in endpoints)
            ram = any(0x20000000 <= address < 0x20080000 for address in endpoints)
            enabled.append(dict(address=channel["address"], flash_endpoint=flash,
                                uart1_ram_endpoints=uart and ram, count=channel["count"]))
    return dict(cpuwait=bool(value["pmr"] & 4), lcpu_reset_asserted=bool(value["rstr1"] & 1),
                enabled_dma=enabled, all_flash_users_controlled=False,
                limits="Finite values and endpoint hints only; no DMA ownership or continuous quiescence proof")


def geometry_summary(value, ready, stub):
    """Recheck bounded pointer reads independently; never return a write permit."""
    uart = 0x20042260
    fixed = [(0x20041ff4, 90), (0x200007ac, 18), (0x20000464, 2), (0x20000730, 2), (uart, 30)]
    handles = [0x200007ac] + [0x20041ff4 + i * 72 for i in range(5)]
    words = {}
    def block(item, address, count):
        if (not isinstance(item, dict) or type(item.get("address")) is not int or item["address"] != address
                or not isinstance(item.get("words"), list) or len(item["words"]) != count
                or any(type(v) is not int or not 0 <= v <= 0xffffffff for v in item["words"])):
            raise ValueError("Incomplete or foreign geometry RAM block")
        return item["words"]
    if not isinstance(value, dict) or not isinstance(value.get("blocks"), list) or len(value["blocks"]) != len(fixed):
        raise ValueError("Missing fixed geometry blocks")
    for item, (address, count) in zip(value["blocks"], fixed):
        words.update({address + i * 4: v for i, v in enumerate(block(item, address, count))})
    if type(value.get("uart_address")) is not int or value["uart_address"] != uart:
        raise ValueError("UART observation is not the pinned RTOS object")
    pointer = words[uart + 0x6c]
    if type(value.get("uart_dma_pointer")) is not int or value["uart_dma_pointer"] != pointer:
        raise ValueError("UART DMA pointer differs from fixed RAM")
    valid_dma = pointer % 4 == 0 and 0x20000000 <= pointer <= 0x20059fc0
    dma = block(value.get("uart_dma"), pointer, 16) if valid_dma else None
    if not valid_dma and value.get("uart_dma") is not None:
        raise ValueError("DMA pointer escaped bounded stub low RAM")
    tables = value.get("tables")
    if not isinstance(tables, list) or len(tables) != 6:
        raise ValueError("Missing geometry command table observations")
    decoded = {}
    for item, handle in zip(tables, handles):
        t = words[handle + 12]
        if (not isinstance(item, dict) or type(item.get("handle")) is not int or item["handle"] != handle
                or type(item.get("pointer")) is not int or item["pointer"] != t):
            raise ValueError("Command table pointer differs from fixed handle")
        expected = [(index, (t + offset) & ~3, t + offset) for index, offset in ((27, 0x103), (41, 0x181))
                    if 0x2005a000 <= ((t + offset) & ~3) and ((t + offset) & ~3) + 12 <= 0x2005a000 + len(stub)]
        records = item.get("records")
        if not isinstance(records, list) or len(records) != len(expected):
            raise ValueError("Foreign or incomplete command table records")
        decoded[handle] = {}
        for record, (index, address, start) in zip(records, expected):
            if not isinstance(record, dict) or type(record.get("index")) is not int or record["index"] != index:
                raise ValueError("Foreign command index")
            data = b"".join(v.to_bytes(4, "little") for v in block(record.get("block"), address, 3))
            offset = start - address
            decoded[handle][index] = dict(opcode=data[offset], bytes=data[offset:offset + 9].hex(),
                matches_pinned_image=data == stub[address - 0x2005a000:address - 0x2005a000 + 12])
    target = 0x12420000
    def contains(start, size): return start <= target < ((start + size) & 0xffffffff)
    selected = 0x200007ac if contains(words[0x200007e8], words[0x200007ec]) else next(
        (h for h in handles[1:] if contains(words[h + 0x3c], words[h + 0x40])), None)
    fields = {}
    if selected is not None:
        data = b"".join(words[selected + i * 4].to_bytes(4, "little") for i in range(18))
        fields = dict(handle=selected, controller=words[selected], table=words[selected + 12],
            address_base=words[selected + 16], capacity_field=words[selected + 20],
            field_23=data[0x23], field_24=data[0x24], callback=words[selected + 0x2c],
            sector_size_if_nor=(4096 << data[0x24]) if data[0x23] == 0 and data[0x24] in (0, 1) else None,
            commands=decoded[selected])
    instance = dma[0] if dma else None
    channel = next((c for c in ready["channels"] if c["address"] == instance), None)
    uart_bound = bool(dma and channel and channel["ccr"] & 1 and words[uart] == 0x50084000
        and dma[12] == uart and dma[13:16] == [0x20062321, 0x20062355, 0x200622cb]
        and channel["source"] == 0x50084024 and channel["destination"] == words[uart + 0x54])
    return dict(selected_flash=fields, uart_rx_dma_bound_at_samples=uart_bound,
        uart_address=uart, uart_dma_pointer=pointer, uart_dma_instance=instance, target=target,
        all_flash_users_controlled=False, physical_erase_envelope_verified=False,
        limits="Sequential finite RAM values, conditional bin interpretation; no continuous ownership or physical erase proof")


def probe(config, directory, port, phase_end):
    """Run one fixed observer under the outer worker's inherited Windows Job."""
    directory = Path(directory)
    options, _ = prepare(config["observer"])
    build = load(options["build"])
    binary = (ROOT / build["executable"]["path"]).resolve()
    cancel = directory.parent / "cancel.json"
    request = dict(schema=1, simulation=config["simulation"], port=port,
        plan=config.get("observer_plan", "dual-nor"),
        out=str(directory / "stub-observer"),
        stub={**build["stub"], "path": str((ROOT / build["stub"]["path"]).resolve())},
        baseline=file_record(options["first"]), cancel=str(cancel),
        seconds=max(1, min(600, int(phase_end - time.monotonic()) - 1)),
        entry_reset_authorized=config["rom_reset_authorized"], ram_stub_authorized=True,
        register_observation_authorized=True, original_start_authorized=True,
        exit_reset_authorized=config["allow_exit_reset"],
        scenario="success" if config["scenario"] == "rtc-reset" else config["scenario"])
    path = directory / "stub-request.json"
    if path.exists():
        raise ValueError("Never replace a stub request")
    write_json(path, request)
    argv = [str(binary), "--config", str(path), digest(path)]
    child = None
    forced, closed, exit_code = False, True, None
    with (directory / "stub-process.log").open("xb") as log:
        try:
            # The outer ReadProcess Job owns this process too. It spawns no descendants.
            child = subprocess.Popen(argv, cwd=directory, stdin=subprocess.DEVNULL,
                                     stdout=log, stderr=subprocess.STDOUT)
            grace_end = None
            while child.poll() is None:
                if cancel.exists() or time.monotonic() >= phase_end:
                    if not cancel.exists():
                        write_json(cancel, dict(reason="Observer phase budget ended"))
                    if grace_end is None:
                        grace_end = time.monotonic() + 6
                    if time.monotonic() >= grace_end:
                        forced = True
                        break
                time.sleep(.02)
            exit_code = child.poll()
        finally:
            if child is not None:
                if child.poll() is None:
                    forced = True
                    child.kill()
                try:
                    child.wait(timeout=2)
                    exit_code = child.returncode
                except subprocess.TimeoutExpired:
                    closed = False
    result_path = directory / "stub-observer/observer.json"
    result, summaries, pair, evidence_error = {}, {}, {}, ""
    try:
        result = load(result_path) if result_path.exists() else {}
        if not isinstance(result, dict):
            raise ValueError("Observer result must be an object")
        if result and (result.get("schema") != 1 or result.get("simulation") is not config["simulation"]
                       or result.get("config_sha") != digest(path) or result.get("plan") != request["plan"]
                       or result.get("source_rev") != REVISION or result.get("flash_write") is not False
                       or result.get("erase") is not False or result.get("stub_sha256") != STUB_SHA):
            raise ValueError("Foreign observer result")
        for label in ("ready", "after"):
            if label + "_registers" in result:
                summaries[label] = sample_summary(result[label + "_registers"])
        if result.get("geometry_snapshot_complete") is True:
            if request["plan"] != "geometry" or "ready" not in summaries:
                raise ValueError("Geometry evidence belongs to a different plan or lacks ready state")
            summaries["geometry"] = geometry_summary(result["geometry"], result["ready_registers"], Path(request["stub"]["path"]).read_bytes())
        if result.get("read_pair_passed") is True:
            pair = review_pair(directory / "stub-observer/nor-first.bin", directory / "stub-observer/nor-second.bin")
            if not pair["passed"] or pair["flash_sha256"] != result.get("nor_sha256"):
                raise ValueError("Observer read-pair claim differs from independently parsed files")
    except (OSError, ValueError, KeyError, TypeError) as exc:
        # The process may already have reset the device. Malformed evidence cannot
        # turn unknown entry into a claim that no device action occurred.
        evidence_error = "Observer evidence rejected: " + repr(exc)
        result, summaries, pair = {}, {}, {}
    error = "" if exit_code == 0 and result.get("passed") is True and not forced else (
        "Observer failed or needed termination; raw files and completed facts retained: "
        + repr(result.get("errors", [])) + "; exit=" + str(exit_code))
    if evidence_error:
        error = evidence_error
    observed = dict(observation_passed=(result.get("geometry_snapshot_complete") is True if request["plan"] == "geometry"
                    else result.get("snapshots_complete") is True and result.get("read_pair_passed") is True),
                    read_pair_passed=result.get("read_pair_passed") is True,
                    summaries=summaries, pair_review=pair, all_flash_users_controlled=False,
                    physical_erase_envelope_verified=False, report=result)
    return dict(observations=observed, entry_attempted=result.get("entry_attempted"),
                cleanup=dict(resources_closed=closed, child_forced=forced, child_exit=exit_code,
                             application_return_needs_operator=True),
                error=error, argv=argv, request=file_record(path))


def main():
    from ota_rom_device import run
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    for mode in ("simulate", "capture"):
        cmd = sub.add_parser(mode)
        cmd.add_argument("--out", type=Path, required=True)
        cmd.add_argument("--observer-build", type=Path, required=True)
        cmd.add_argument("--baseline-first", type=Path, required=True)
        cmd.add_argument("--baseline-second", type=Path, required=True)
        cmd.add_argument("--geometry-only", action="store_true", help="Fixed bounded RAM structures only; no NOR request")
        if mode == "simulate":
            cmd.add_argument("--before-fixture", type=Path, required=True)
            cmd.add_argument("--after-fixture", type=Path, required=True)
            cmd.add_argument("--scenario", choices=SCENARIOS, default="success")
        else:
            for key in ("result", "flash", "build-archive", "recorded-root"):
                cmd.add_argument("--" + key, type=Path, required=True)
            for key in ("expected-bin", "port", "location"):
                cmd.add_argument("--" + key, required=True)
            for flag in ("confirm-same-board-and-firmware", "allow-rom-reset", "allow-stub-observation",
                         "allow-original-start", "allow-exit-reset"):
                cmd.add_argument("--" + flag, action="store_true", required=True)
    cmd = sub.add_parser("status")
    cmd.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    if args.mode == "status":
        print((args.run / "device.json").read_text(encoding="utf-8"))
        return 0
    options = dict(build=args.observer_build, first=args.baseline_first, second=args.baseline_second)
    plan = "geometry" if args.geometry_only else "dual-nor"
    timing = dict(limits=GEOMETRY_LIMITS, total_seconds=300, prompt_seconds=60) if args.geometry_only else dict(limits=LIMITS, total_seconds=900)
    if args.mode == "simulate":
        result = run(args.out, profile="stub-observe", observer=options,
            fixtures=dict(before=args.before_fixture.read_bytes(), after=args.after_fixture.read_bytes()),
            scenario=args.scenario, observer_plan=plan, **timing)
    else:
        capture = {key: str(getattr(args, key)) for key in
                   ("result", "flash", "build_archive", "recorded_root", "expected_bin", "port", "location")}
        capture.update(conditions_confirmed=args.confirm_same_board_and_firmware,
                       rom_reset_authorized=args.allow_rom_reset,
                       stub_observation_authorized=args.allow_stub_observation,
                       original_start_authorized=args.allow_original_start,
                       exit_reset_authorized=args.allow_exit_reset)
        result = run(args.out, profile="stub-observe", observer=options, capture=capture,
                     observer_plan=plan, **timing)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
