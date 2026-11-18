"""Produce an Android NOR Offline file carrying a signed WristFlow A/B release."""
import argparse
import base64
import hashlib
from pathlib import Path
import struct
import subprocess

from lab_check import ROOT, check_build, load, require
from ota_preflight import crc32_mpeg2, inspect_partitions


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
    manifest = struct.pack("<4sI24s4I32s", b"WFR1", 1, board.encode("ascii"), 1, a["start"],
                           len(body), version, digest)
    signer = ROOT / "scripts/Ota-ReleaseKey.ps1"
    signed = subprocess.run(["pwsh", "-NoProfile", "-NonInteractive", "-File", str(signer), "-Mode", "Sign",
                             "-ManifestBase64", base64.b64encode(manifest).decode("ascii")],
                            check=True, stdout=subprocess.PIPE, timeout=30)
    signature = base64.b64decode(signed.stdout.strip(), validate=True)
    require(len(signature) == 256, "Unexpected RSA signature length")
    directory = Path(directory).resolve()
    directory.mkdir(parents=True, exist_ok=False)
    work = directory / "packer"
    work.mkdir()
    payload = manifest + signature + body
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
    parser.add_argument("--build-result", type=Path, required=True)
    parser.add_argument("--release", type=int, required=True)
    parser.add_argument("--out-dir", type=Path, required=True, help="New release directory")
    args = parser.parse_args()
    print(create_release(args.build_result, args.release, args.out_dir))
