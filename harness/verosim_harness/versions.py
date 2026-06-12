"""Pinned-component versions that participate in oracle cache keys."""

from __future__ import annotations

import functools
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


@functools.lru_cache(maxsize=1)
def get_versions() -> dict[str, str]:
    from importlib.metadata import version

    try:
        musicdiff_sha = subprocess.run(
            ["git", "-C", str(REPO_ROOT / "musicdiff"), "rev-parse", "HEAD"],
            capture_output=True, text=True, check=True,
        ).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        musicdiff_sha = "unknown"

    return {
        "musicdiff": musicdiff_sha,
        "music21": version("music21"),
        "converter21": version("converter21"),
    }
