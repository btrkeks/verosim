"""Content-hash cache for oracle records.

Key = sha256 over (sha256(pred bytes), sha256(gt bytes), detail int, component
versions, schema version), so a cache hit is valid only for identical inputs
under identical pinned musicdiff/music21/converter21. Records live at
harness/cache/<key[:2]>/<key>.json (gitignored).
"""

from __future__ import annotations

import hashlib
import json
from pathlib import Path

from . import SCHEMA_VERSION
from .versions import REPO_ROOT, get_versions

CACHE_DIR = REPO_ROOT / "harness" / "cache"


def _sha256_file(path: str | Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def cache_key(pred_path: str | Path, gt_path: str | Path, detail_value: int) -> str:
    versions = get_versions()
    parts = [
        _sha256_file(pred_path),
        _sha256_file(gt_path),
        str(detail_value),
        versions["musicdiff"],
        versions["music21"],
        versions["converter21"],
        f"schema{SCHEMA_VERSION}",
    ]
    return hashlib.sha256("|".join(parts).encode()).hexdigest()


def _cache_path(key: str) -> Path:
    return CACHE_DIR / key[:2] / f"{key}.json"


def load(key: str) -> dict | None:
    path = _cache_path(key)
    if not path.is_file():
        return None
    try:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None  # corrupt/partial entry: recompute


def store(key: str, record: dict) -> None:
    path = _cache_path(key)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(".tmp")
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(record, f)
    tmp.replace(path)
