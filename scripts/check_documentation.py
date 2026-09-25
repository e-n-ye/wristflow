#!/usr/bin/env python3
"""Route documentation-only changes without downloading the firmware SDK."""

import argparse
import json
import os
from pathlib import Path
import subprocess


ROOT_DOCUMENTS = {
    "AGENTS.md", "CONTEXT.md", "CONTRIBUTING.md", "README.md",
    ".github/pull_request_template.md",
}
DOCUMENT_SUFFIXES = {".md", ".txt", ".json", ".config", ".png", ".jpg", ".jpeg", ".svg", ".pdf"}


def is_documentation(path):
    return path in ROOT_DOCUMENTS or (
        path.startswith("docs/") and Path(path).suffix in DOCUMENT_SUFFIXES
    )


def changed_paths(base, head, event):
    # No rename detection: moving source into docs must still build the source deletion.
    args = ["git", "diff", "--no-renames", "--name-only", "-z"]
    if event == "pull_request":
        args.append("--merge-base")
    raw = subprocess.check_output([*args, base, head, "--"])
    return [os.fsdecode(path) for path in raw.split(b"\0") if path]


def check_markdown(paths):
    checked = 0
    for name in paths:
        path = Path(name)
        # Deleted Markdown and binary evidence are valid documentation changes.
        if path.suffix != ".md" or not path.is_file():
            continue
        content = path.read_text(encoding="utf-8")
        if "\x00" in content or "\ufffd" in content:
            raise ValueError(f"Invalid text in {name}")
        if any(line.startswith(("<<<<<<< ", ">>>>>>> ")) for line in content.splitlines()):
            raise ValueError(f"Unresolved conflict in {name}")
        checked += 1
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base", default=os.environ.get("BASE_SHA", ""))
    parser.add_argument("--head", default=os.environ.get("HEAD_SHA", "HEAD"))
    parser.add_argument("--event", default=os.environ.get("EVENT_NAME", "push"))
    args = parser.parse_args()
    forced = args.event == "workflow_dispatch" or not args.base or set(args.base) == {"0"}
    paths = [] if forced else changed_paths(args.base, args.head, args.event)
    # Unknown base, an empty diff, or an unrecognised file falls back to all baselines.
    docs_only = bool(paths) and all(is_documentation(path) for path in paths)
    checked = check_markdown(paths)
    result = {"docs_only": docs_only, "changed_files": len(paths), "markdown_checked": checked}
    print(json.dumps(result))
    if output := os.environ.get("GITHUB_OUTPUT"):
        with open(output, "a", encoding="utf-8") as stream:
            stream.write(f"docs_only={str(docs_only).lower()}\n")
    if summary := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(summary, "a", encoding="utf-8") as stream:
            stream.write("Documentation only: SDK installation and all firmware/host tests skipped.\n"
                         if docs_only else "Full firmware and host baselines required.\n")


if __name__ == "__main__":
    main()
