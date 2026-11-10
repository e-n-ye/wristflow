"""Build/test the pinned host observer, freeze its executable; never open a device."""
import argparse
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import subprocess
import tomllib

from lab_evidence import file_record, verify_files, write_json

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "scripts/ota_stub_bridge"
REVISION = "be2adab37bfb0ef4eb623b7e47e7b8693918b1e1"
SOURCES = ("scripts/ota_stub_build.py", "scripts/ota_stub_bridge/Cargo.toml",
           "scripts/ota_stub_bridge/Cargo.lock", "scripts/ota_stub_bridge/src/main.rs",
           "scripts/ota_stub_bridge/src/port.rs", "scripts/ota_stub_bridge/src/simulated.rs",
           "scripts/ota_stub_bridge/src/geometry.rs", "scripts/ota_stub_bridge/src/common.rs",
           "scripts/ota_stub_bridge/src/b_sector.rs", "scripts/ota_stub_bridge/src/sector.rs",
           "scripts/ota_stub_bridge/src/b_execute.rs")


def build(output, cargo, cargo_home, target_dir, cc, *, runner=subprocess.run, b_sector=False, b_execute=False):
    if b_sector and b_execute:
        raise ValueError("Select one independent host binary")
    output = Path(output).resolve()
    if output.exists():
        raise ValueError("New build archive directory required")
    manifest = tomllib.loads((PROJECT / "Cargo.toml").read_text(encoding="utf-8"))
    lock = tomllib.loads((PROJECT / "Cargo.lock").read_text(encoding="utf-8"))
    dependency = manifest["dependencies"]["sftool-lib"]
    packages = [p for p in lock["package"] if p["name"] == "sftool-lib"]
    if (dependency["rev"] != REVISION or dependency["git"] != "https://github.com/OpenSiFli/sftool"
            or len(packages) != 1 or not packages[0]["source"].endswith("#" + REVISION)):
        raise ValueError("Original pinned sftool library required")
    source_records = [file_record(ROOT / name, ROOT) for name in SOURCES]
    output.mkdir(parents=True, exist_ok=False)
    cargo, cc = Path(cargo).resolve(), Path(cc).resolve()
    env = dict(os.environ, CARGO_HOME=str(Path(cargo_home).resolve()),
               CARGO_TARGET_DIR=str(Path(target_dir).resolve()), CC=str(cc),
               CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=str(cc),
               CARGO_NET_GIT_FETCH_WITH_CLI="true", RUSTC=str(cargo.with_name("rustc.exe")))
    # The host compiler's runtime DLLs must precede an older Rust MinGW runtime.
    env["PATH"] = os.pathsep.join((str(cc.parent), str(cargo.parent), env.get("PATH", "")))
    results = []
    for mode in ("test", "build"):
        command = [str(cargo), mode, "--locked", "--target", "x86_64-pc-windows-gnu",
                   "--manifest-path", str(PROJECT / "Cargo.toml"), "-j", "4"]
        if mode == "build":
            command += ["--release"]
        with (output / (mode + ".log")).open("xb") as stream:
            result = runner(command, cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT,
                            stdin=subprocess.DEVNULL, timeout=1200)
        results.append(dict(command=command, exit_code=result.returncode))
        if result.returncode:
            write_json(output / "failed.json", dict(commands=results, sources=source_records, device_actions=False))
            raise RuntimeError("Host observer " + mode + " failed; archive retained")
    verify_files(source_records, ROOT)
    binary = "wristflow-ota-b-execute.exe" if b_execute else "wristflow-ota-b-sector.exe" if b_sector else "wristflow-ota-stub-observer.exe"
    source = Path(target_dir).resolve() / "x86_64-pc-windows-gnu/release" / binary
    target = output / source.name
    shutil.copyfile(source, target)
    if source.read_bytes() != target.read_bytes():
        raise ValueError("Frozen observer differs from linked executable")
    versions = {}
    for name in ("cargo", "rustc"):
        result = runner([str(cargo.with_name(name + ".exe")), "--version"], cwd=ROOT,
                        env=env, capture_output=True, text=True, encoding="utf-8", timeout=15)
        if result.returncode:
            raise RuntimeError("Tool version read failed")
        versions[name] = result.stdout.strip()
    result = dict(schema=1, kind="ota-b-execute" if b_execute else "ota-b-sector" if b_sector else "ota-stub-observer", created_utc=dt.datetime.now(dt.timezone.utc).isoformat(),
                  source_revision=REVISION, sources=source_records, commands=results, versions=versions,
                  executable=file_record(target, ROOT), sdk_lock=file_record(ROOT / "sdk.lock.json", ROOT),
                  stub=file_record(ROOT / "vendor/SiFli-SDK/tools/uart_download/file/ram_patch_52X.bin", ROOT),
                  device_actions=False, flash_write_entry=b_sector or b_execute, firmware_build=False, passed=True)
    write_json(output / "result.json", result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("out", "cargo", "cargo-home", "target-dir", "cc"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--b-sector", action="store_true", help="Freeze the separate fixed-sector writer, not the read-only observer")
    parser.add_argument("--b-execute", action="store_true", help="Freeze the separate fixed boot/probe execution and restoration binary")
    args = parser.parse_args()
    result = build(args.out, args.cargo, args.cargo_home, args.target_dir, args.cc, b_sector=args.b_sector, b_execute=args.b_execute)
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
