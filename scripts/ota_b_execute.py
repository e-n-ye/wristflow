"""Fixed006 B execution and boot/B restoration with the original RAM runtime."""
import argparse
import copy
from pathlib import Path
import subprocess
import time

from lab_evidence import digest, file_record, verify_files, write_json
from ota_b_sector import load, LIMITS
from ota_stub_observe import ROOT, REVISION, STUB_SHA
from ota_backup_review import review_pair
from ota_probe_package import parse_capture
from ota_boot_check import check as check_boot
from ota_slot_check import check as check_slot
from ota_rom_backup import recovery, healthy, timestamp

BOOT_SHA = "db747d9070aeb3642a64e765045a0b6671b922d16f72d5860e6e3bbb334c2303"
B_SHA = "14102c47df5a8078fd934f9efaea7a271ef790eb7ffd265546abf4cf5b4440fe"
ORIGINAL_BOOT_SHA = "f59f948f987362f1270b1da541405e82bd887c1a16c306364c04913c9c07f667"
ORIGINAL_B_SHA = "154ff4e9b4b26b0a5f13ce0f4ec94f32418d48cbabdb276ae48f842879713070"
TAG = [2306271408, 2453024267, 2305287774, 1289714410]
SCENARIOS = ("success", "fragmented", "before-changed", "b-compare", "boot-compare", "write-fail",
    "restore-boot-fail", "restore-b-fail", "restore-read-fail", "alias-retained", "no-frames",
    "bad-tag", "bad-crc", "short-frames", "duplicate-frames", "execute-hang", "rtc-reset",
    "initial-register-fail", "restore-register-fail", "restore-alias-fail", "probe-start-fail", "original-start-fail")
AUTHORIZATIONS = ("b_sector_write_authorized", "boot_write_authorized", "probe_start_authorized",
                  "restore_entry_authorized", "restore_writes_authorized")


def prepare(options):
    if not isinstance(options, dict) or set(options) != {"build", "first", "second", "package", "recovery_from"}:
        raise ValueError("Explicit writer, backup pair, fixed006 package and recovery selection required")
    options = {k: str(Path(v).resolve()) if v is not None else None for k, v in options.items()}
    if any(options[k] is None for k in ("build", "first", "second", "package")):
        raise ValueError("Missing execution inputs")
    build = load(options["build"])
    if (build.get("kind") != "ota-b-execute" or build.get("passed") is not True
            or build.get("source_revision") != REVISION or build.get("flash_write_entry") is not True
            or build.get("device_actions") is not False or build.get("stub", {}).get("sha256") != STUB_SHA
            or Path(build.get("executable", {}).get("path", "")).name != "wristflow-ota-b-execute.exe"):
        raise ValueError("Separate verified probe execution binary required")
    records = build["sources"] + [build["executable"], build["sdk_lock"], build["stub"]]
    verify_files(records, ROOT)
    review_pair(options["first"], options["second"])
    package = Path(options["package"])
    review = load(package / "review.json")
    binding = load(package / "binding.json")
    if review["tag"] != TAG or binding["tag"] != TAG or review["probe"]["marker_address"] != 0x12020674:
        raise ValueError("Fixed006 result identity required")
    for name, size, sha in (("boot-compatible-58496.bin", 58496, BOOT_SHA), ("probe-b-sector.bin", 4096, B_SHA)):
        if (package / name).stat().st_size != size or digest(package / name) != sha:
            raise ValueError("Fixed006 candidate changed")
    snapshots = {digest(p): p for p in (package / "source-snapshot").iterdir() if p.is_file()}
    current_sources = []
    for name in ("slot-build", "boot-build"):
        source = load(package / name / "result.json")
        for item in source["project_sources"]:
            path = Path(item["path"]).resolve()
            if not path.is_relative_to(ROOT):
                raise ValueError("Probe source outside repository")
            original = snapshots.get(item["sha256"])
            if original is None or original.read_bytes().decode("utf-8").replace("\r\n", "\n") != path.read_bytes().decode("utf-8").replace("\r\n", "\n"):
                raise ValueError("Probe build source content changed")
            current_sources.extend([file_record(original), file_record(path)])
    for name in ("frozen-slot", "frozen-boot"):
        frozen = load(package / name / "frozen-manifest.json")
        verify_files(frozen["files"] + [frozen["original_result"]])
        records += frozen["files"] + [frozen["original_result"]]
    records += current_sources
    check_slot(package / "frozen-slot/main.elf", package / "frozen-slot/main.bin")
    checked = check_boot(package / "frozen-boot/main.elf", package / "frozen-boot/main.bin", package / "binding.json")
    if not checked["probe_execution_bound"]:
        raise ValueError("Literal fixed probe boot binding required")
    paths = [Path(options[k]) for k in ("build", "first", "second")]
    paths += [package / name for name in ("review.json", "binding.json", "boot-compatible-58496.bin", "probe-b-sector.bin",
        "frozen-slot/main.elf", "frozen-slot/main.bin", "frozen-boot/main.elf", "frozen-boot/main.bin")]
    if options["recovery_from"]:
        prior = Path(options["recovery_from"])
        prior_config = load(prior / "config.json")
        if prior_config.get("profile") != "stub-b-execute":
            raise ValueError("Recovery must name an existing execution window")
        request_path = prior / "worker/b-execute-request.json"
        request = load(request_path)
        if request["simulation"] is not prior_config["simulation"] or request["recovery_only"] is not False:
            raise ValueError("Recovery origin must be a bound exercise, not recursive recovery")
        for name, size, sha in (("boot-restore64k.bin", 65536, ORIGINAL_BOOT_SHA), ("b-restore4k.bin", 4096, ORIGINAL_B_SHA)):
            path = prior / "worker/b-execute" / name
            if path.stat().st_size != size or digest(path) != sha:
                raise ValueError("Missing or corrupt prior-window originals")
            paths.append(path)
        # A failed/missing child report is permitted. Frozen original files and
        # the exact prior input/worker binding, not a success boolean, are required.
        for record in prior_config["inputs"]:
            verify_files([record])
        paths += [prior / "config.json", request_path, prior / "worker/before-snapshot.json", prior / "worker/before-uart.bin"]
    records += [file_record(p) for p in paths]
    for name in ("ota_probe_package.py", "ota_boot_check.py", "ota_slot_check.py"):
        records.append(file_record(ROOT / "scripts" / name))
    # Deduplicate shared records without hiding conflicting digests.
    unique = {}
    for record in records:
        key = str((ROOT / record["path"]).resolve())
        if key in unique and unique[key]["sha256"] != record["sha256"]:
            raise ValueError("Conflicting evidence identity")
        unique[key] = dict(record, path=key)
    return options, list(unique.values())


def prior_before(config, directory):
    """Reuse the explicit prior raw baseline; no attempt to read a stopped probe."""
    prior = Path(config["observer"]["recovery_from"])
    old = load(prior / "worker/before-snapshot.json")
    raw = (prior / "worker/before-uart.bin").read_bytes()
    from runtime_record_decode import decode_capture
    if decode_capture(raw, old["records"]["token"]) != old["records"]:
        raise ValueError("Prior application baseline differs from raw")
    directory = Path(directory)
    target = directory / "before-uart.bin"
    target.write_bytes(raw)
    value = copy.deepcopy(old)
    value.update(raw=file_record(target), baseline_from_prior_window=True,
                 prior_snapshot=file_record(prior / "worker/before-snapshot.json"), closed_monotonic_ns=time.monotonic_ns())
    write_json(directory / "before-snapshot.json", value)
    return value


def recovery_check(before, after, elapsed, recovery_only):
    if not recovery_only:
        value = recovery(before, after, elapsed_seconds=elapsed, check_rtc=False)
    else:
        healthy(after["records"])
        a, b = before["records"]["saved"], after["records"]["saved"]
        if ((b["generation"] - a["generation"]) & 0xffffffff) >= 0x80000000 or b["total_recorded_ms"] < a["total_recorded_ms"] or any(
                b["scene_ms"][k] < v for k, v in a["scene_ms"].items()):
            raise ValueError("Saved application baseline regressed during recovery")
        value = dict(runtime_saved_not_regressed=True, expected_new_boot=None, prior_window_baseline=True,
            unsaved_tail_preserved_on_host_ms=before["records"]["unsaved_recorded_ms"],
            rtc_continuity=False, limits="Explicit prior-window baseline; exact boot delta and RTC continuity are not inferred")
    # Year2000 equal replies cannot establish a valid clock in this profile.
    value["rtc_valid"] = all(timestamp(s["replies"]["date"]) >= 1704067200 and s["records"]["current"]["utc"] > 0 for s in (before, after))
    return value


def verified_result(value, directory, request, path, review):
    if (value.get("schema") != 1 or value.get("kind") != "ota-b-execute" or value.get("source_rev") != REVISION
            or value.get("config_sha") != digest(path) or value.get("stub_sha256") != STUB_SHA
            or value.get("simulation") is not request["simulation"] or value.get("recovery_only") is not request["recovery_only"]
            or value.get("rtc_time_write") is not False or value.get("same_port_object") is not True or value.get("baud") != 1000000):
        raise ValueError("Foreign execution report")
    flags = ("entry_attempted", "boot_may_be_changed", "b_may_be_changed", "b_install_attempted", "b_install_completed",
        "boot_install_attempted", "boot_install_completed", "boot_restore_attempted", "boot_restore_completed", "b_restore_attempted", "b_restore_completed",
        "fresh_prestate_verified", "b_candidate_verified", "boot_candidate_verified", "probe_start_attempted", "probe_start_completed", "probe_verified",
        "boot_restored", "b_restored", "restored_verified", "original_start_completed", "fallback_attempted", "passed")
    if any(type(value.get(k)) is not bool for k in flags) or value["fallback_attempted"]:
        raise ValueError("Malformed execution facts")
    baseline = Path(request["baseline"]["path"]).read_bytes()
    boot, b = baseline[0x10000:0x20000], baseline[0x420000:0x430000]
    def data(name, size):
        raw = (directory / name).read_bytes()
        if len(raw) != size:
            raise ValueError("Incomplete raw execution evidence")
        return raw
    if value["fresh_prestate_verified"]:
        if (data("before-ftab4k.bin", 4096) != baseline[:4096] or data("before-boot64k.bin", 65536) != boot
                or data("before-b64k.bin", 65536) != b or data("boot-restore64k.bin", 65536) != boot
                or data("b-restore4k.bin", 4096) != b[:4096]):
            raise ValueError("Fresh originals differ")
    if not request["recovery_only"] and (value["b_install_attempted"] or value["boot_install_attempted"]) and not value["fresh_prestate_verified"]:
        raise ValueError("Mutation without fresh originals")
    for prefix, name, expected, tail in (("b", "installed-b64k.bin", Path(request["candidate"]["path"]).read_bytes(), b[4096:]),
                                        ("boot", "installed-boot64k.bin", Path(request["boot"]["path"]).read_bytes(), boot[58496:])):
        file = directory / name
        if file.exists():
            raw = data(name, 65536)
            if value[prefix + "_candidate_verified"] is not (raw == expected + tail):
                raise ValueError("Installed image facts differ from raw")
        elif value[prefix + "_candidate_verified"]:
            raise ValueError("Missing installed readback")
    for prefix, expected in (("boot", boot), ("b", b)):
        attempted = value[prefix + "_restore_attempted"]
        if attempted and value[prefix + "_restore_completed"] and (directory / ("restored-" + prefix + "64k.bin")).exists():
            if value[prefix + "_restored"] is not (data("restored-" + prefix + "64k.bin", 65536) == expected):
                raise ValueError("Restoration facts differ from raw")
        elif value[prefix + "_restored"] and (attempted or value[prefix + "_install_attempted"] or request["recovery_only"]):
            raise ValueError("Missing restoration proof")
        if value[prefix + "_restored"] and value[prefix + "_may_be_changed"]:
            raise ValueError("Contradictory restoration state")
        for role in ("install", "restore"):
            if value[prefix + "_" + role + "_completed"] and not value[prefix + "_" + role + "_attempted"]:
                raise ValueError("Out-of-order completion")
    if value["restored_verified"] is not (value["boot_restored"] and value["b_restored"]):
        raise ValueError("Incomplete full restoration")
    if value["probe_verified"]:
        parsed = parse_capture(data("probe-uart.bin", (directory / "probe-uart.bin").stat().st_size), review)
        if parsed["frames"] != value["probe_frames"]:
            raise ValueError("Probe facts differ from raw")
    if value["passed"] and not (value["restored_verified"] and value["original_start_completed"] and (request["recovery_only"] or value["probe_verified"])):
        raise ValueError("Incomplete execution claimed success")
    if request["recovery_only"] and (value["probe_start_attempted"] or value["boot_install_attempted"] or value["b_install_attempted"]):
        raise ValueError("Recovery-only scope expanded")
    return value


def probe(config, directory, port, phase_end):
    directory = Path(directory)
    options, _ = prepare(config["observer"])
    build = load(options["build"])
    package = Path(options["package"])
    recovery_only = options["recovery_from"] is not None
    if recovery_only and load(Path(options["recovery_from"]) / "config.json")["simulation"] is not config["simulation"]:
        raise ValueError("Cannot cross real/simulation recovery evidence")
    cancel = directory.parent / "cancel.json"
    seconds = min(180, int(phase_end - time.monotonic()) - 1)
    if seconds < 20 or cancel.exists():
        raise ValueError("Insufficient execution budget")
    capture = config.get("capture") or {}
    request = dict(schema=1, simulation=config["simulation"], port=port, out=str(directory / "b-execute"),
        stub=dict(build["stub"], path=str((ROOT / build["stub"]["path"]).resolve())),
        baseline=file_record(options["first"]), candidate=file_record(package / "probe-b-sector.bin"), boot=file_record(package / "boot-compatible-58496.bin"),
        restore_boot=None, restore_b=None, recovery_only=recovery_only, cancel=str(cancel), seconds=seconds, listen_seconds=1 if config["simulation"] else 10,
        scenario="success" if config["scenario"] == "rtc-reset" else config["scenario"],
        entry_reset_authorized=config["rom_reset_authorized"], ram_stub_authorized=True, register_observation_authorized=True,
        original_start_authorized=True, **{k: config["simulation"] or capture.get(k) is True for k in AUTHORIZATIONS})
    if recovery_only:
        original = Path(options["recovery_from"]) / "worker/b-execute"
        request.update(restore_boot=file_record(original / "boot-restore64k.bin"), restore_b=file_record(original / "b-restore4k.bin"))
    path = directory / "b-execute-request.json"
    if path.exists():
        raise ValueError("Never reuse an execution request")
    write_json(path, request)
    binary = ROOT / build["executable"]["path"]
    argv = [str(binary), "--config", str(path), digest(path)]
    child, forced, closed, exit_code = None, False, True, None
    with (directory / "b-execute-process.log").open("xb") as log:
        try:
            child = subprocess.Popen(argv, cwd=directory, stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
            grace_end = None
            while child.poll() is None:
                if cancel.exists() or time.monotonic() >= phase_end:
                    if not cancel.exists():
                        write_json(cancel, dict(reason="Execution phase ended"))
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
        result = verified_result(load(directory / "b-execute/execution.json"), directory / "b-execute", request, path, load(package / "review.json"))
    except (OSError, ValueError, KeyError, TypeError) as exc:
        error = "Execution report rejected; entry and boot/B state unknown: " + repr(exc)
    if not error and (forced or exit_code != 0 or result["passed"] is not True):
        error = "Execution failed; raw/original files retained: " + repr(result.get("errors"))
    return dict(entry_attempted=result.get("entry_attempted"), mutation_attempted=None if not result else any(result[k + "_attempted"] for k in ("b_install", "boot_install", "b_restore", "boot_restore")),
        boot_may_be_changed=result.get("boot_may_be_changed"), b_may_be_changed=result.get("b_may_be_changed"), restored_verified=result.get("restored_verified", False),
        observations=dict(observation_passed=result.get("passed") is True, report=result, all_flash_users_controlled=False),
        cleanup=dict(resources_closed=closed, child_forced=forced, child_exit=exit_code, application_return_needs_operator=True), error=error, argv=argv, request=file_record(path))


def main():
    from ota_rom_device import run
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="mode", required=True)
    for mode in ("simulate", "capture"):
        cmd = sub.add_parser(mode)
        for key in ("out", "execution-build", "baseline-first", "baseline-second", "package"):
            cmd.add_argument("--" + key, type=Path, required=True)
        cmd.add_argument("--recovery-from", type=Path)
        if mode == "simulate":
            cmd.add_argument("--before-fixture", type=Path, required=True)
            cmd.add_argument("--after-fixture", type=Path, required=True)
            cmd.add_argument("--scenario", choices=SCENARIOS, default="success")
        else:
            for key in ("result", "flash", "build-archive", "recorded-root"):
                cmd.add_argument("--" + key, type=Path, required=True)
            for key in ("expected-bin", "port", "location"):
                cmd.add_argument("--" + key, required=True)
            for key in ("confirm-same-board-and-firmware", "allow-rom-reset", "allow-stub-observation", "allow-original-start", "allow-fixed-execution-and-restoration"):
                cmd.add_argument("--" + key, action="store_true", required=True)
    args = parser.parse_args()
    options = dict(build=args.execution_build, first=args.baseline_first, second=args.baseline_second, package=args.package, recovery_from=args.recovery_from)
    kwargs = dict(profile="stub-b-execute", observer=options, limits=LIMITS, total_seconds=600, prompt_seconds=120, allow_exit_reset=False)
    if args.mode == "simulate":
        result = run(args.out, fixtures=dict(before=args.before_fixture.read_bytes(), after=args.after_fixture.read_bytes()), scenario=args.scenario, **kwargs)
    else:
        capture = {k: str(getattr(args, k)) for k in ("result", "flash", "build_archive", "recorded_root", "expected_bin", "port", "location")}
        capture.update(conditions_confirmed=args.confirm_same_board_and_firmware, rom_reset_authorized=args.allow_rom_reset,
            stub_observation_authorized=args.allow_stub_observation, original_start_authorized=args.allow_original_start,
            **{k: args.allow_fixed_execution_and_restoration for k in AUTHORIZATIONS})
        result = run(args.out, capture=capture, **kwargs)
    print(result)
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
