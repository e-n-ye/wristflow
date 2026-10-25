"""Host-only checks of existing build records, explicit evidence and changed docs.

Never builds, flashes, enumerates ports or opens a device. Every invocation reads
the inputs again; a saved report is evidence of that invocation, not a cache.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from urllib.parse import unquote, urlsplit
import zipfile

from lab_evidence import file_record, verify_files, write_json

ROOT = Path(__file__).resolve().parents[1]
TEXT = {".md", ".json", ".py", ".ps1", ".c", ".h", ".xml", ".txt", ".cmake"}
ADDRESSES = {"bootloader.bin": "0x12010000", "main.bin": "0x12020000", "ftab.bin": "0x12000000"}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def git(root, *args):
    return subprocess.check_output(["git", "-C", str(root), *args])


def load(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def resolve(path, root):
    path = Path(path)
    return (root / path if not path.is_absolute() else path).resolve()


def records(value, root):
    require(isinstance(value, list) and bool(value), "Explicit file list must not be empty")
    verify_files(value, root)
    return [file_record(resolve(item["path"], root)) for item in value]


def check_files(manifest, key="files", *, root=ROOT):
    value = load(manifest)
    for part in key.split(".") if key else []:
        value = value[part]
    return {"manifest": file_record(manifest), "key": key, "files": records(value, root)}


def check_sdk(root, lock):
    sdk = root / "vendor/SiFli-SDK"
    require(git(sdk, "rev-parse", "HEAD").decode().strip() == lock["sdk"]["commit"], "SDK HEAD differs from lock")
    for name, revision in lock["submodules"].items():
        require(git(sdk / name, "rev-parse", "HEAD").decode().strip() == revision, "SDK submodule differs: " + name)
    require(not git(sdk, "status", "--porcelain").strip(), "SDK worktree is dirty")
    return {"commit": lock["sdk"]["commit"], "submodules": lock["submodules"], "clean": True}


def check_build(result_path, *, root=ROOT, source_archive=None, flash=None):
    root = Path(root).resolve()
    value, lock = load(result_path), load(root / "sdk.lock.json")
    require(value["exit_code"] == 0 and value["artifact_validation_passed"] is True,
            "Build record did not accept compilation/artifacts")
    require(value["sdk_commit"] == lock["sdk"]["commit"] and not value["sdk_changes"], "Build SDK identity differs")
    built_submodules = "\n".join(value["submodules"])
    for name, revision in lock["submodules"].items():
        require(any(line.startswith(" " + revision + " " + name + " ") for line in built_submodules.splitlines()),
                "Build submodule identity differs: " + name)
    sdk = check_sdk(root, lock)
    artifacts = records(value["artifacts"], root)
    build_dir = resolve(value["build_directory"], root)
    by_path = {Path(item["path"]): item for item in artifacts}
    for name in ("main.elf", ".config", "rtconfig.h", "sftool_param.json"):
        require(build_dir / name in by_path and by_path[build_dir / name]["bytes"] > 0, "Missing required artifact: " + name)
    manifest = load(build_dir / "sftool_param.json")
    require(manifest["chip"] == "SF32LB52" and manifest["memory"] == "NOR" and manifest["write_flash"]["verify"] is True,
            "Flash manifest differs from supported board")
    require(lock["board"] == "sf32lb52-lchspi-ulp", "Unsupported locked board")
    images = []
    for entry in manifest["write_flash"]["files"]:
        path = resolve(entry["path"], build_dir)
        require(path.is_relative_to(build_dir) and path.name in ADDRESSES and entry["address"] == ADDRESSES[path.name],
                "Invalid image path/address: " + str(path))
        require(path in by_path and by_path[path]["bytes"] > 0, "Image missing from artifact manifest")
        images.append(dict(by_path[path], name=path.name, address=entry["address"]))
    require(len(images) == 3 and {item["name"] for item in images} == set(ADDRESSES), "Exactly three unique flash images required")
    firmware = resolve(value["firmware_path"], root)
    require(firmware == next(Path(item["path"]) for item in images if item["name"] == "main.bin"), "Firmware path mismatch")
    sources, endings, seen = [], [], set()
    archive = zipfile.ZipFile(source_archive) if source_archive else None
    try:
        require(bool(value["project_sources"]), "Build source list is empty")
        for item in value["project_sources"]:
            path = resolve(item["path"], root)
            require(path not in seen and path.is_relative_to(root), "Duplicate/outside project source: " + str(path))
            seen.add(path)
            current = file_record(path)
            if current["sha256"] != item["sha256"]:
                require(archive is not None, "Source changed (strict bytes): " + str(path))
                original = archive.read(path.relative_to(root).as_posix())
                require(hashlib.sha256(original).hexdigest() == item["sha256"], "Source snapshot does not match build hash")
                require(original.decode("utf-8").replace("\r\n", "\n") == path.read_bytes().decode("utf-8").replace("\r\n", "\n"),
                        "Source content changed: " + str(path))
                endings.append(str(path))
            sources.append(current)
    finally:
        if archive:
            archive.close()
    report = {"build_result": file_record(result_path), "sdk_lock": file_record(root / "sdk.lock.json"), "sdk": sdk,
              "recorded_sources": sources, "line_endings_only": endings, "artifacts": artifacts, "images": images,
              "source_archive": file_record(source_archive) if source_archive else None,
              "hardware_verified": False, "device_identity_verified": False,
              "limits": "Checks recorded build inputs and local bytes, not dependency completeness or board identity. New build inputs require a fresh build."}
    if flash:
        transferred = load(flash)
        require(transferred["exit_code"] == 0 and transferred["board"] == lock["board"] and transferred["example"] == value["example"],
                "Flash result did not accept the same target")
        actual = records(transferred["images"], root)
        require(len(actual) == 3, "Flash result needs three images")
        require({(str(resolve(item["path"], root)), item["sha256"], item["address"], item["name"]) for item in transferred["images"]} ==
                {(item["path"], item["sha256"], item["address"], item["name"]) for item in images}, "Flash/build images differ")
        report.update(flash_result=file_record(flash), flash_port=transferred["port"])
    return report


def markdown_lines(text):
    """Keep line numbers while excluding fenced code."""
    fence = None
    for number, line in enumerate(text.splitlines(), 1):
        marker = re.match(r"^\s{0,3}(`{3,}|~{3,})", line)
        if marker:
            run = marker[1]
            if fence is None:
                fence = run
            elif run[0] == fence[0] and len(run) >= len(fence):
                fence = None
            continue
        if fence is None:
            yield number, line


def anchors(text):
    lines = list(markdown_lines(text))
    visible = "\n".join(re.sub(r"(`+).*?\1", "", line) for _, line in lines)
    found = set(re.findall(r"<(?:a|h[1-6])\b[^>]*\b(?:id|name)=[\"']([^\"']+)", visible, re.I))
    counts = {}
    for _, line in lines:
        heading = re.match(r"^ {0,3}#{1,6}\s+(.+?)\s*#*\s*$", line)
        if not heading:
            continue
        label = re.sub(r"!?\[([^\]]+)\]\([^)]*\)", r"\1", heading[1])
        label = re.sub(r"<[^>]*>", "", label).lower()
        label = re.sub(r"[^\w\- ]", "", label).replace(" ", "-")
        count = counts.get(label, 0)
        counts[label] = count + 1
        found.add(label + ("-" + str(count) if count else ""))
    return found


def added_lines(root, base, name, untracked):
    if name in untracked:
        return None  # Entire new file.
    numbers, number = set(), 0
    patch = git(root, "diff", "--no-ext-diff", "--no-textconv", "--unified=0", base, "--", name).decode("utf-8")
    for line in patch.splitlines():
        hunk = re.match(r"@@ .*? \+(\d+)", line)
        if hunk:
            number = int(hunk[1])
        elif line.startswith("+") and not line.startswith("+++"):
            numbers.add(number)
            number += 1
        elif line.startswith(" "):
            number += 1
    return numbers


def check_docs(*, root=ROOT, base="HEAD", paths=None):
    root = Path(root).resolve()
    untracked = {p for p in git(root, "ls-files", "--others", "--exclude-standard", "-z").decode("utf-8").split("\0") if p}
    changed = {p for p in git(root, "diff", "--name-only", "--diff-filter=ACMR", "-z", base, "--").decode("utf-8").split("\0") if p} | untracked
    if paths is not None:
        selected = {resolve(p, root).relative_to(root).as_posix() for p in paths}
        require(selected <= changed, "--path must select changed files")
        changed &= selected
    inspected, targets, links = [], {}, 0
    for name in sorted(changed):
        path = root / name
        if path.suffix.lower() not in TEXT:
            continue
        text = path.read_text(encoding="utf-8-sig")
        require("\ufffd" not in text and "\u951f\u65a4\u62f7" not in text, "Encoding corruption: " + name)
        inspected.append(file_record(path, root))
        if path.suffix.lower() != ".md":
            continue
        added = added_lines(root, base, name, untracked)
        lines = list(markdown_lines(text))
        definitions = {m[1].casefold(): m[2] for _, line in lines if (m := re.match(r"^\s{0,3}\[([^\]]+)\]:\s*(<[^>]+>|\S+)", line))}
        for number, line in lines:
            if added is not None and number not in added:
                continue
            line = re.sub(r"(`+).*?\1", "", line)
            hrefs = re.findall(r"\]\(\s*(<[^>]*>|(?:[^\s()]|\([^()]*\))+)\s*(?:[\"'][^\n]*[\"']\s*)?\)", line)
            definition = re.match(r"^\s{0,3}\[([^\]]+)\]:\s*(<[^>]+>|\S+)", line)
            if definition:
                hrefs.append(definition[2])
            for label, reference in re.findall(r"\[([^\]]+)\]\[([^\]]*)\]", line):
                key = (reference or label).casefold()
                require(key in definitions, f"Undefined reference: {name}:{number}: {key}")
                hrefs.append(definitions[key])
            for href in hrefs:
                url = urlsplit(href.strip("<>"))
                if url.scheme or url.netloc:
                    continue
                target = resolve(unquote(url.path), path.parent) if url.path else path
                require(target.exists(), f"Missing link: {name}:{number}: {href}")
                if url.fragment and target.suffix.lower() == ".md":
                    require(unquote(url.fragment) in anchors(target.read_text(encoding="utf-8-sig")), f"Missing anchor: {name}:{number}: {href}")
                if target.is_file():
                    targets[str(target)] = file_record(target)
                links += 1
    subprocess.run(["git", "-C", str(root), "diff", "--check", base, "--", *sorted(changed)], check=True)
    return {"base": git(root, "rev-parse", base).decode().strip(), "changed_text": inspected,
            "added_local_links": links, "link_targets": list(targets.values()),
            "limits": "UTF-8 of changed text; added Markdown inline/reference links, ATX headings and explicit anchors. No network check or semantic documentation approval."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--out", type=Path, help="New JSON result; existing evidence is never overwritten")
    commands = parser.add_subparsers(dest="command", required=True)
    build = commands.add_parser("build", help="Recheck existing Build.ps1 result and optional Flash.ps1 result")
    build.add_argument("--result", type=Path, required=True)
    build.add_argument("--source-archive", type=Path, help="Original root-relative ZIP; permits proven CRLF-only source differences")
    build.add_argument("--flash", type=Path)
    files = commands.add_parser("files", help="Check one explicit file-record list, without recursive hash guessing")
    files.add_argument("--manifest", type=Path, required=True)
    files.add_argument("--key", default="files", help="Dotted key; empty selects a top-level list")
    docs = commands.add_parser("docs", help="Check worktree changes (including staged and untracked) against a base")
    docs.add_argument("--base", default="HEAD")
    docs.add_argument("--path", type=Path, action="append")
    args = parser.parse_args()
    if args.out:
        require(not args.out.exists(), "Report path already exists")
        args.out.parent.mkdir(parents=True, exist_ok=True)
    try:
        if args.command == "build":
            details = check_build(args.result, root=args.root, source_archive=args.source_archive, flash=args.flash)
        elif args.command == "files":
            details = check_files(args.manifest, args.key, root=args.root)
        else:
            details = check_docs(root=args.root, base=args.base, paths=args.path)
        report = {"passed": True, "kind": args.command, "checker": file_record(Path(__file__)), "details": details}
    except (ValueError, OSError, KeyError, TypeError, zipfile.BadZipFile, subprocess.CalledProcessError) as exc:
        report = {"passed": False, "kind": args.command, "error": str(exc)}
    if args.out:
        write_json(args.out, report)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
