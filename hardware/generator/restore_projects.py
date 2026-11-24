"""Assemble the four editable EasyEDA Pro projects from the design and local libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import sqlite3
import subprocess
import sys
from tempfile import mkdtemp
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

from workspace import SOURCE


def records(content):
    return [row for line in content.splitlines() if line.strip() and (row := json.loads(line))]


def encode(rows):
    return ("\n".join(json.dumps(row, ensure_ascii=False, separators=(",", ":")) for row in rows) + "\n").encode()


def embedded_footprints(reference):
    result = {}
    for path in reference.namelist():
        if not path.endswith(".epcb"):
            continue
        rows = records(reference.read(path))
        starts = [i for i, row in enumerate(rows) if row[0] == "DOCTYPE"]
        for first, end in zip(starts, starts[1:]):
            section = rows[first:end]
            if section[0][1] == "FOOTPRINT":
                result[section[1][1]["uuid"]] = section
    return result


def member_data(entry, design, reference, embedded, library):
    if "drawing" in entry:
        return design.read(entry["drawing"])
    if "reference" in entry:
        rows = records(reference.read(entry["reference"]))
    elif "embedded" in entry:
        rows = list(embedded[entry["embedded"]])
    else:
        row = library.execute("SELECT dataStr FROM components WHERE uuid = ?", (entry["library"],)).fetchone()
        if row is None:
            raise ValueError(f"Missing system library item {entry['library']}")
        rows = records(row[0])
    for index, record in entry.get("edits", []):
        rows[index] = record
    return encode(rows)


def restore_archive(args, staging):
    with ZipFile(args.design) as design, ZipFile(args.reference) as reference:
        spec = json.loads(design.read("manifest.json"))
        with args.reference.open("rb") as file:
            if hashlib.file_digest(file, "sha256").hexdigest() != spec["reference_sha256"]:
                raise ValueError("Reference revision does not match the design")
        embedded = embedded_footprints(reference)
        library = sqlite3.connect(args.library.resolve().as_uri() + "?mode=ro", uri=True)
        try:
            staging.mkdir()
            for name, entries in spec["projects"].items():
                if Path(name).name != name or Path(name).suffix != ".epro":
                    raise ValueError(f"Invalid project filename: {name}")
                with ZipFile(staging / name, "w") as project:
                    for entry in entries:
                        data = member_data(entry, design, reference, embedded, library)
                        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                            raise ValueError(f"Revision mismatch for {name}/{entry['name']}")
                        info = ZipInfo(entry["name"], tuple(entry["date"]))
                        project.writestr(info, data, compress_type=ZIP_DEFLATED)
        finally:
            library.close()


def restore_reviewed(args, staging):
    subprocess.run(
        [
            sys.executable, "-B", str(SOURCE / "write_project.py"),
            "--reviewed-placement",
            "--reference", str(args.reference), "--library", str(args.library),
            "--out", str(staging),
        ],
        check=True,
    )
    boards = SOURCE.parent / "boards"
    manifests = sorted(boards.glob("*/members.sha256"))
    verified = set()
    for manifest in manifests:
        name, *entries = manifest.read_text(encoding="utf-8").splitlines()
        expected = {path: digest for digest, path in (line.split("  ", 1) for line in entries)}
        with ZipFile(staging / name) as archive:
            actual = {path: hashlib.sha256(archive.read(path)).hexdigest() for path in archive.namelist()}
        if expected != actual:
            mismatches = sorted(path for path in expected.keys() | actual.keys() if expected.get(path) != actual.get(path))
            raise ValueError(f"{name} differs from the reviewed revision: {', '.join(mismatches)}")
        verified.add(name)
        print(f"{name}: verified {len(expected)} members")
    if len(verified) != 4 or verified != {p.name for p in staging.glob("*.epro")}:
        raise ValueError("The reviewed manifests do not cover all four generated projects")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path, help="Official Huangshan V1.2 .epro")
    parser.add_argument("library", type=Path, help="JLCEDA Pro 4.1.105 resources/app/assets/db/lceda-std.elib")
    parser.add_argument("output", type=Path, help="Destination for new .epro files")
    parser.add_argument("--design", type=Path, help="Restore a private design.zip instead of recreating the reviewed version")
    args = parser.parse_args()
    if args.output.exists():
        raise FileExistsError(f"Choose a new output directory: {args.output}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    scratch = Path(mkdtemp(prefix=".wristflow-eda-", dir=args.output.parent))
    staging = scratch / "projects"
    if args.design:
        restore_archive(args, staging)
    else:
        restore_reviewed(args, staging)
    names = sorted(p.name for p in staging.glob("*.epro"))
    staging.rename(args.output)
    scratch.rmdir()
    for name in names:
        print(args.output / name)


if __name__ == "__main__":
    main()
