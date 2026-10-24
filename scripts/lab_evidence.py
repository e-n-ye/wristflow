"""Small evidence helpers shared by local experiments; never opens a device."""
from __future__ import annotations

import hashlib
import json
import pathlib


def digest(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def write_json(path, value):
    path = pathlib.Path(path)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def file_record(path, root=None):
    path = pathlib.Path(path).resolve()
    return {"path": path.relative_to(root).as_posix() if root else str(path),
            "bytes": path.stat().st_size, "sha256": digest(path)}


def verify_files(records, root=None):
    """Check an explicit manifest, rejecting duplicate paths and missing bytes."""
    seen = set()
    for item in records:
        path = pathlib.Path(item["path"])
        if not path.is_absolute():
            if root is None:
                raise ValueError("Relative evidence path requires a root")
            path = pathlib.Path(root) / path
        path = path.resolve()
        if path in seen:
            raise ValueError("Duplicate evidence path: " + str(path))
        seen.add(path)
        if (digest(path) != item["sha256"] or
                "bytes" in item and path.stat().st_size != item["bytes"]):
            raise ValueError("Evidence changed: " + str(path))
    return len(seen)
