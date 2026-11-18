"""Produce an Android NOR Offline file carrying a signed WristFlow A/B release."""
import argparse
import base64
import hashlib
from pathlib import Path
import struct
import subprocess
from zipfile import ZipFile

from lab_check import ROOT, check_build, load, require
from ota_preflight import crc32_mpeg2, inspect_partitions


def signed_header(board, role, address, version, body):
    manifest = struct.pack("<4sI24s4I32s", b"WFR1", 1, board.encode("ascii"), role, address,
                           len(body), version, hashlib.sha256(body).digest())
    signer = ROOT / "scripts/Ota-ReleaseKey.ps1"
    signed = subprocess.run(["pwsh", "-NoProfile", "-NonInteractive", "-File", str(signer), "-Mode", "Sign",
                             "-ManifestBase64", base64.b64encode(manifest).decode("ascii")],
                            check=True, stdout=subprocess.PIPE, timeout=30)
    signature = base64.b64decode(signed.stdout.strip(), validate=True)
    require(len(signature) == 256, "Unexpected RSA signature length")
    return manifest + signature


def enroll_original_a(directory):
    """Issue a detached identity only for the original, wired-installed CycleLog."""
    evidence = load(ROOT / "docs/evidence/2026-11-03/battery-usable-curve.json")["firmware"]
    archive = ROOT / evidence["archive"]["path"]
    require(hashlib.sha256(archive.read_bytes()).hexdigest() == evidence["archive"]["sha256"], "Archive changed")
    with ZipFile(archive) as z:
        main, = [p for p in z.namelist() if p.endswith("build_sf32lb52-lchspi-ulp_hcpu/main.bin")]
        body = z.read(main)
        build = load(ROOT / evidence["result"])
        require(z.read(evidence["result"]) == (ROOT / evidence["result"]).read_bytes(), "Archived build changed")
    board = load(ROOT / "sdk.lock.json")["board"]
    flash = load(ROOT / evidence["flash"])
    image, = [i for i in flash["images"] if i["name"] == "main.bin"]
    digest = hashlib.sha256(body).hexdigest()
    require(build["cycle_log"] and build["artifact_validation_passed"] and flash["exit_code"] == 0 and
            flash["board"] == board and int(image["address"], 0) == 0x12020000 and
            len(body) == image["bytes"] == evidence["main_bytes"] == 3055760 and
            digest == image["sha256"] == evidence["main_sha256"] ==
            "10560604d08b7951516a1b0b7914b8f6ffa22427a6a373e8f0ad1cc3f432d28d", "Unexpected original A identity")
    for name in ("nor-first.bin", "nor-second.bin"):
        backup = (ROOT / "artifacts/ota-rom-backup-20261108/device-001" / name).read_bytes()
        require(len(backup) == 0x1000000 and hashlib.sha256(backup).hexdigest() ==
                "9140bb4e1bb700c8a7b1d06f29f510c3f18b13fb6da86e34b7f453fec639ab74" and
                backup[0x20000:0x20000 + len(body)] == body, "Original device backup differs")
    header = signed_header(board, 2, 0x12020000, 2026110301, body)
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=False)
    (directory / "original_a.bin").write_bytes(header)
    output = directory / "original_a.inc"
    with output.open("x", encoding="ascii", newline="\n") as stream:
        for i in range(0, len(header), 12):
            stream.write(" ".join(f"0x{x:02x}," for x in header[i:i + 12]) + "\n")
    return output


def create_release(result, version, directory):
    require(0 < version <= 0xffffffff, "Release must be a positive uint32")
    result = Path(result)
    build = load(result)
    require(build.get("example") == "product" and not any(build.get(flag) for flag in
            ("resource_probe", "storage_probe", "fs_probe", "cycle_log")), "Ordinary Product build required")
    verified = check_build(result)
    main = next(entry for entry in verified["images"] if entry["name"] == "main.bin")
    board = load(ROOT / "sdk.lock.json")["board"]
    table = ROOT / "apps/ota_loader/project" / (board + "_hcpu") / "ptab.json"
    regions = inspect_partitions(load(table))
    a, b = (regions["WF_OTA_SLOT_" + slot] for slot in "AB")
    require(int(main["address"], 0) == a["start"] and a["bytes"] == b["bytes"], "Image/slot geometry mismatch")
    require(8 <= main["bytes"] <= a["bytes"] - 336, "Main image exceeds the slot payload extent")
    body = Path(main["path"]).read_bytes()
    digest = hashlib.sha256(body).digest()
    require(len(body) == main["bytes"] and digest.hex() == main["sha256"], "Main changed after build verification")
    header = signed_header(board, 1, a["start"], version, body)
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=False)
    work = directory / "packer"
    work.mkdir()
    payload = header + body
    (work / "hcpu.bin").write_bytes(payload)
    tool = ROOT / "vendor/SiFli-SDK/tools/secureboot/imgtoolv37.exe"
    with (work / "imgtool.log").open("xb") as log:
        subprocess.run([str(tool), "gen_dfu", "--img_para", "hcpu", "0", "0", "--offline_img=2",
                        "--dst_dir=."], cwd=work, check=True, stdout=log, stderr=subprocess.STDOUT, timeout=120)
    generated = work / "offline_install.bin"
    wire = generated.read_bytes()
    header = struct.pack("<4sBBHIBBI", b"FCES", 1, 0xff, 1, crc32_mpeg2(payload), 0, 0, len(payload))
    require(wire == header + payload, "Official wrapper differs from the signed single-image release")
    output = directory / "offline_install.bin"
    generated.rename(output)
    return output


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--build-result", type=Path)
    target.add_argument("--enroll-original-a", action="store_true")
    parser.add_argument("--release", type=int)
    parser.add_argument("--out-dir", type=Path, required=True, help="New release directory")
    args = parser.parse_args()
    if args.enroll_original_a:
        if args.release is not None:
            parser.error("Original A has a fixed release identity")
        print(enroll_original_a(args.out_dir))
    elif args.release is None:
        parser.error("An ordinary release requires --release")
    else:
        print(create_release(args.build_result, args.release, args.out_dir))
