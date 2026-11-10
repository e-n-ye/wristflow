"""Fixed B4KiB write/readback/restore under the pinned official RAM runtime."""
import argparse
import json
from pathlib import Path
import subprocess
import time

from lab_evidence import digest, file_record, verify_files, write_json
from ota_backup_review import review_pair
from ota_stub_observe import ROOT, REVISION, STUB_SHA, HEADERS, sample_summary

ADDRESS = 0x12420000
CANDIDATE_SHA = "14102c47df5a8078fd934f9efaea7a271ef790eb7ffd265546abf4cf5b4440fe"
LIMITS = dict(bind=30, before=60, rom=180, after=60, grace=10)
SCENARIOS = ("success", "fragmented", "lcpu-running", "register-read-fail", "before-changed", "prestate-crc-fail",
             "candidate-mismatch", "guard-change", "restore-mismatch", "write-start-fail", "candidate-data-timeout",
             "readback-crc-fail", "restore-data-fail", "b-hang-read", "rtc-reset")


def load(path):
    path = Path(path)
    if path.stat().st_size > 2 * 1024 * 1024:
        raise ValueError("Oversized sector evidence")
    return json.loads(path.read_text(encoding="utf-8"))


def prepare(options):
    if not isinstance(options, dict) or set(options) != {"build", "first", "second", "candidate"}:
        raise ValueError("Explicit sector build, original backup pair and fixed candidate required")
    options = {k: str(Path(v).resolve()) for k, v in options.items()}
    build = load(options["build"])
    if (build.get("kind") != "ota-b-sector" or build.get("passed") is not True
            or build.get("source_revision") != REVISION or build.get("flash_write_entry") is not True
            or build.get("device_actions") is not False or build.get("stub", {}).get("sha256") != STUB_SHA
            or build.get("executable", {}).get("path", "").replace("\\", "/").split("/")[-1] != "wristflow-ota-b-sector.exe"):
        raise ValueError("A successful separate B-sector writer build is required")
    records = build["sources"] + [build["executable"], build["sdk_lock"], build["stub"]]
    verify_files(records, ROOT)
    review_pair(options["first"], options["second"])
    candidate = Path(options["candidate"])
    if candidate.stat().st_size != 4096 or digest(candidate) != CANDIDATE_SHA:
        raise ValueError("Only the reviewed fixed B-sector candidate is accepted")
    records += [file_record(options[k]) for k in options]
    records += [file_record(ROOT / "vendor/SiFli-SDK/drivers/cmsis/sf32lb52x" / name) for name in HEADERS]
    return options, [dict(r, path=str((ROOT / r["path"]).resolve())) for r in records]


def verified_result(value, directory, request, request_path):
    """Independently compare frozen files; never trust target or child booleans alone."""
    if (not isinstance(value, dict) or value.get("schema") != 1 or value.get("kind") != "ota-b-sector"
            or value.get("simulation") is not request["simulation"] or value.get("config_sha") != digest(request_path)
            or value.get("source_rev") != REVISION or value.get("stub_sha256") != STUB_SHA
            or value.get("rtc_time_write") is not False or value.get("baud") != 1000000 or value.get("same_port_object") is not True):
        raise ValueError("Foreign sector report")
    keys = ("entry_attempted", "candidate_write_attempted", "candidate_write_completed", "restore_write_attempted",
            "restore_write_completed", "candidate_verified", "candidate_guard_unchanged", "restored_verified",
            "restored_guard_unchanged", "b_may_be_changed", "normal_start_command_completed", "fallback_attempted", "passed")
    if any(type(value.get(k)) is not bool for k in keys):
        raise ValueError("Invalid sector facts")
    if "ready_registers" in value:
        sample_summary(value["ready_registers"])
    def read(name, size):
        data = (directory / name).read_bytes()
        if len(data) != size:
            raise ValueError("Incomplete sector evidence file")
        return data
    before = None
    if value.get("fresh_prestate_verified") is True:
        before = read("before-b64k.bin", 65536)
        if before != Path(request["baseline"]["path"]).read_bytes()[0x420000:0x430000]:
            raise ValueError("Fresh prestate differs from bound baseline")
        if read("fresh-restore-b4k.bin", 4096) != before[:4096]:
            raise ValueError("Fresh restoration file differs")
    if value["candidate_write_attempted"] and before is None:
        raise ValueError("Mutation without a complete fresh prestate")
    candidate_file = directory / "candidate-b64k.bin"
    if candidate_file.exists():
        data = read(candidate_file.name, 65536)
        if before is None or value["candidate_verified"] is not (data[:4096] == Path(request["candidate"]["path"]).read_bytes()) or value["candidate_guard_unchanged"] is not (data[4096:] == before[4096:]):
            raise ValueError("Candidate readback facts disagree with raw bytes")
    elif value["candidate_verified"] or value["candidate_guard_unchanged"]:
        raise ValueError("Claimed missing candidate readback")
    restored = directory / "restored-b64k.bin"
    if restored.exists():
        data = read(restored.name, 65536)
        if before is None or value["restored_verified"] is not (data == before) or value["restored_guard_unchanged"] is not (data[4096:] == before[4096:]):
            raise ValueError("Restore readback facts disagree with raw bytes")
    elif value["restored_verified"] or value["restored_guard_unchanged"]:
        raise ValueError("Claimed missing restoration")
    if value["b_may_be_changed"] is not (value["candidate_write_attempted"] and not value["restored_verified"]):
        raise ValueError("Possible B mutation state inconsistent")
    if value["restore_write_attempted"] and not value["candidate_write_completed"]:
        raise ValueError("Restoration attempted after unknown write state")
    if (value["candidate_write_completed"] and not value["candidate_write_attempted"]
            or value["restore_write_completed"] and not value["restore_write_attempted"]
            or value["restored_verified"] and not value["restore_write_completed"]):
        raise ValueError("Out-of-order sector completion facts")
    if value["passed"] and not all(value[k] for k in ("candidate_verified", "candidate_guard_unchanged", "restored_verified", "normal_start_command_completed")):
        raise ValueError("Incomplete experiment claimed success")
    return value


def probe(config, directory, port, phase_end):
    directory = Path(directory)
    options, _ = prepare(config["observer"])
    build = load(options["build"])
    binary = (ROOT / build["executable"]["path"]).resolve()
    cancel = directory.parent / "cancel.json"
    seconds = min(180, int(phase_end - time.monotonic()) - 1)
    if seconds < 15 or cancel.exists():
        raise ValueError("Insufficient active budget or cancelled before sector child")
    request = dict(schema=1, simulation=config["simulation"], port=port, out=str(directory / "b-sector"),
        stub=dict(build["stub"], path=str((ROOT / build["stub"]["path"]).resolve())),
        baseline=file_record(options["first"]), candidate=file_record(options["candidate"]), cancel=str(cancel),
        seconds=seconds,
        entry_reset_authorized=config["rom_reset_authorized"], ram_stub_authorized=True, register_observation_authorized=True,
        b_sector_write_authorized=config["simulation"] or config["capture"].get("b_sector_write_authorized") is True,
        b_sector_restore_authorized=config["simulation"] or config["capture"].get("b_sector_restore_authorized") is True,
        original_start_authorized=True, exit_reset_authorized=config["allow_exit_reset"],
        scenario="success" if config["scenario"] == "rtc-reset" else config["scenario"])
    path = directory / "b-sector-request.json"
    if path.exists():
        raise ValueError("Never reuse a sector request")
    write_json(path, request)
    argv = [str(binary), "--config", str(path), digest(path)]
    child, exit_code, forced, closed = None, None, False, True
    with (directory / "b-sector-process.log").open("xb") as log:
        try:
            child = subprocess.Popen(argv, cwd=directory, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
            grace_end = None
            while child.poll() is None:
                if cancel.exists() or time.monotonic() >= phase_end:
                    if not cancel.exists():
                        write_json(cancel, dict(reason="Sector phase ended"))
                    if grace_end is None:
                        grace_end = time.monotonic() + 6
                    if time.monotonic() >= grace_end:
                        forced = True
                        break
                time.sleep(.02)
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
    result, error = {}, ""
    try:
        result = verified_result(load(directory / "b-sector/sector.json"), directory / "b-sector", request, path)
    except (OSError, ValueError, KeyError, TypeError) as exc:
        error = "Sector evidence rejected; entry and possible mutation unknown: " + repr(exc)
    if not error and (exit_code != 0 or forced or result["passed"] is not True):
        error = "B experiment failed; raw files retained: " + repr(result.get("errors"))
    return dict(entry_attempted=result.get("entry_attempted"),
        candidate_write_attempted=result.get("candidate_write_attempted"), b_may_be_changed=result.get("b_may_be_changed"),
        restored_verified=result.get("restored_verified", False),
        observations=dict(observation_passed=result.get("passed") is True, report=result,
                          all_flash_users_controlled=False, physical_erase_envelope_verified=False),
        cleanup=dict(resources_closed=closed, child_forced=forced, child_exit=exit_code, application_return_needs_operator=True),
        error=error, argv=argv, request=file_record(path))


def main():
    from ota_rom_device import run
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    for mode in ("simulate", "capture"):
        cmd = sub.add_parser(mode)
        for name in ("out", "sector-build", "baseline-first", "baseline-second", "candidate"):
            cmd.add_argument("--" + name, type=Path, required=True)
        if mode == "simulate":
            cmd.add_argument("--before-fixture", type=Path, required=True)
            cmd.add_argument("--after-fixture", type=Path, required=True)
            cmd.add_argument("--scenario", choices=SCENARIOS, default="success")
        else:
            for key in ("result", "flash", "build-archive", "recorded-root"):
                cmd.add_argument("--" + key, type=Path, required=True)
            for key in ("expected-bin", "port", "location"):
                cmd.add_argument("--" + key, required=True)
            for flag in ("confirm-same-board-and-firmware", "allow-rom-reset", "allow-stub-observation", "allow-original-start",
                         "allow-exit-reset", "allow-b-sector-write", "allow-b-sector-restore"):
                cmd.add_argument("--" + flag, action="store_true", required=True)
    cmd = sub.add_parser("status")
    cmd.add_argument("--run", type=Path, required=True)
    args = parser.parse_args()
    if args.mode == "status":
        print((args.run / "device.json").read_text(encoding="utf-8"))
        return 0
    options = dict(build=args.sector_build, first=args.baseline_first, second=args.baseline_second, candidate=args.candidate)
    kwargs = dict(profile="stub-b-sector", observer=options, limits=LIMITS, total_seconds=600, prompt_seconds=120)
    if args.mode == "simulate":
        result = run(args.out, fixtures=dict(before=args.before_fixture.read_bytes(), after=args.after_fixture.read_bytes()), scenario=args.scenario, **kwargs)
    else:
        capture = {k: str(getattr(args, k)) for k in ("result", "flash", "build_archive", "recorded_root", "expected_bin", "port", "location")}
        capture.update(conditions_confirmed=args.confirm_same_board_and_firmware, rom_reset_authorized=args.allow_rom_reset,
            stub_observation_authorized=args.allow_stub_observation, original_start_authorized=args.allow_original_start,
            exit_reset_authorized=args.allow_exit_reset, b_sector_write_authorized=args.allow_b_sector_write,
            b_sector_restore_authorized=args.allow_b_sector_restore)
        result = run(args.out, capture=capture, **kwargs)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
