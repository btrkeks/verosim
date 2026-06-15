"""Pinned-component versions that participate in oracle cache keys."""

from __future__ import annotations

import functools
import hashlib
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


@functools.lru_cache(maxsize=1)
def get_versions() -> dict[str, str]:
    from importlib.metadata import version

    def _git(*args: str) -> str:
        return subprocess.run(
            ["git", "-C", str(REPO_ROOT / "musicdiff"), *args],
            capture_output=True, text=True, check=True,
        ).stdout

    try:
        musicdiff_sha = _git("rev-parse", "HEAD").strip()
        # The editable install runs the working tree, not HEAD: mix uncommitted
        # changes into the pin so locally patched costing code can't be served
        # stale cache hits. (Untracked files contribute names via status, not
        # contents — good enough; the realistic case is editing tracked files.)
        dirty = _git("status", "--porcelain") + _git("diff", "HEAD")
        if dirty:
            musicdiff_sha += "-dirty-" + hashlib.sha256(dirty.encode()).hexdigest()[:12]
    except (subprocess.CalledProcessError, FileNotFoundError) as exc:
        # A constant fallback ("unknown") would alias different musicdiff
        # states into one cache key; refuse instead.
        raise RuntimeError(
            "cannot pin the vendored musicdiff version (git unavailable or "
            f"{REPO_ROOT / 'musicdiff'} is not a git checkout); refusing to "
            "compute oracle cache keys against an unknown spec"
        ) from exc

    return {
        "musicdiff": musicdiff_sha,
        "music21": version("music21"),
        "converter21": version("converter21"),
    }
