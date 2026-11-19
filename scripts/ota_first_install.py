"""Prepare and verify the fixed original-board wired images without a serial port."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import zlib

from lab_evidence import digest, file_record, verify_files, write_json
from ota_backup_review import REGIONS, require, review_pair

ROOT = Path(__file__).resolve().parents[1]
EVIDENCE = ROOT / "docs/evidence/2026-11-18/ota-original-a-enrollment.json"
HASH_FIX = ROOT / "docs/evidence/2026-11-18/ota-hash-resume.json"
IDENTITY = ROOT / "docs/evidence/2026-11-19/ota-receiver-identity.json"
BASE_SHA = "9140bb4e1bb700c8a7b1d06f29f510c3f18b13fb6da86e34b7f453fec639ab74"
ORIGINAL_B_SHA = "4205c2f63f4417b614d34edf9422e9f3adb3d9899cd5d9db379c7fce3ccda0a0"
META = 0xda0000
SECTOR = 4096


def sha(data):
    return hashlib.sha256(data).hexdigest()


def read_json(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def frozen(evidence, kind):
    result = ROOT / evidence[kind + "_build"]
    require(digest(result) == evidence[kind + "_result_sha256"], "Build evidence changed: " + kind)
    build = read_json(result)
    require(build["artifact_validation_passed"] and build["board"] == "sf32lb52-lchspi-ulp" and
            not build["sdk_changes"] and build["sdk_commit"] == read_json(ROOT / "sdk.lock.json")["sdk"]["commit"],
            "Build or SDK mismatch: " + kind)
    path = result.parent / "frozen/main.bin"
    record, = [item for item in build["artifacts"] if Path(item["path"]).name == "main.bin"]
    data = path.read_bytes()
    require(sha(data) == record["sha256"] == evidence[kind + "_bin_sha256"] and
            len(data) == record["bytes"] == evidence[kind + "_bin_bytes"], "Frozen BIN changed: " + kind)
    return data


def initial_record(enrollment):
    record = bytearray(128)
    struct.pack_into("<4sHHIBBB", record, 0, b"WFAB", 1, len(record), 1, 0, 0, 255)
    record[16:56] = enrollment[40:80]
    struct.pack_into("<I", record, 124, zlib.crc32(record[:124]))
    return bytes(record)


def save(path, data):
    with path.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    require(path.read_bytes() == data, "Archive readback mismatch")


def prepare(directory, first, second, *, historical_pair=False, identity_pair=False):
    evidence = read_json(EVIDENCE)
    build_pair = evidence if historical_pair else read_json(HASH_FIX)["first_install"]
    if identity_pair:
        require(not historical_pair, "Select one receiver generation")
        build_pair = read_json(IDENTITY)
    baseline = review_pair(first, second)
    require(baseline["flash_sha256"] == BASE_SHA and baseline["boot"]["length"] == 58496 and
            baseline["regions"]["a"]["sha256"] == evidence["entire_original_a_sha256"] and
            baseline["regions"]["b"]["sha256"] == ORIGINAL_B_SHA, "Original board backup mismatch")
    original = first.read_bytes()
    boot, receiver = frozen(build_pair, "boot"), frozen(build_pair, "receiver")
    addr = int(build_pair["enrollment_address"], 0)
    off = addr - (0x12000000 + REGIONS["loader"][0])
    require(0 <= off <= len(receiver) - 336, "Enrollment outside receiver")
    enrollment = receiver[off:off + 336]
    require(sha(enrollment) == evidence["signed_enrollment_sha256"] and
            struct.unpack_from("<IIII", enrollment, 32) == (2, 0x12020000, 3055760, 2026110301) and
            enrollment[48:80].hex() == evidence["signed_body_sha256"] ==
            sha(original[0x20000:0x20000 + 3055760]), "Detached enrollment differs from original A")
    require(original[0x820000:0x880000] == b"\xff" * 0x60000 and
            original[META:META + 16384] == b"\xff" * 16384, "Initial reservations not erased")
    record = initial_record(enrollment)
    require(sha(record) == "c5daf159f02d8da1c8386c53bf5695753cbd10a87a69e665e8d7995a2ccea555",
            "Initial stable journal identity mismatch")
    images = (("receiver", 0x820000, receiver, len(receiver)),
              ("journal", META, record, len(record)),
              ("boot", 0x10000, boot, baseline["boot"]["length"]))
    directory.mkdir(parents=True, exist_ok=False)
    entries = []
    for name, start, data, extent in images:
        size = (extent + SECTOR - 1) // SECTOR * SECTOR
        output = directory / (name + ".bin")
        save(output, data.ljust(size, b"\xff"))
        entries.append(dict(role=name, offset=start, **file_record(output)))
    scope = "original-cyclelog-identity-pair" if identity_pair else "original-cyclelog-first-install"
    report = dict(schema=1, scope=scope, baseline_sha256=BASE_SHA,
                  enrollment_address=hex(addr),
                  entries=entries, original_a_sha256=evidence["entire_original_a_sha256"],
                  original_b_sha256=ORIGINAL_B_SHA, hardware_verified=False)
    write_json(directory / "bundle.json", report)
    return report


def build_host(args):
    crate = ROOT / "scripts/ota_stub_bridge"
    src = [crate / p for p in ("Cargo.toml", "Cargo.lock", "src/bin/wristflow-ota-first-install.rs",
           "src/first_media.rs", "src/trace_media.rs", "src/first_wire.rs", "src/first_reads.rs", "src/common.rs", "src/port.rs",
           "src/sector.rs", "src/simulated.rs")]
    records = [file_record(p, ROOT) for p in src]
    cargo, cc = args.cargo.resolve(), args.cc.resolve()
    target = args.target_dir.resolve()
    env = dict(os.environ, CARGO_HOME=str(args.cargo_home.resolve()), CARGO_TARGET_DIR=str(target),
               RUSTC=str(cargo.with_name("rustc.exe")), CC=str(cc), CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER=str(cc))
    env["PATH"] = os.pathsep.join((str(cc.parent), str(cargo.parent), env.get("PATH", "")))
    args.out.mkdir(parents=True, exist_ok=False)
    name = "wristflow-ota-first-install"
    for mode in ("test", "build"):
        cmd = [str(cargo), mode, "--locked", "--offline", "--target", "x86_64-pc-windows-gnu",
               "--manifest-path", str(crate / "Cargo.toml"), "--bin", name, "-j4"]
        if mode == "build":
            cmd.append("--release")
        with (args.out / (mode + ".log")).open("xb") as log:
            subprocess.run(cmd, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=1200)
    verify_files(records, ROOT)
    exe = args.out / (name + ".exe")
    shutil.copyfile(target / "x86_64-pc-windows-gnu/release" / exe.name, exe)
    result = dict(kind="original-board-first-install", sources=records, executable=file_record(exe, ROOT),
                  device_actions=False, sdk_lock=file_record(ROOT / "sdk.lock.json", ROOT))
    write_json(args.out / "result.json", result)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    kit = modes.add_parser("prepare")
    for opt in ("first", "second", "out"):
        kit.add_argument("--"+opt, required=True, type=Path)
    kit.add_argument("--identity-pair", action="store_true")
    builder = modes.add_parser("build")
    for opt in ("cargo", "cc", "cargo-home", "target-dir", "out"):
        builder.add_argument("--"+opt, required=True, type=Path)
    args = parser.parse_args()
    result = prepare(args.out.resolve(), args.first, args.second, identity_pair=args.identity_pair) if args.mode == "prepare" else build_host(args)
    print(json.dumps(result, indent=2))
