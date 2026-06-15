"""VeroSim oracle harness.

DETAIL_LEVELS pins the tier<->DetailLevel mapping (decision D14). Values are
literal ints so this package imports without musicdiff (sampling is
stdlib-only); test_detail_levels.py recomputes them from musicdiff.DetailLevel
to catch vendored-enum drift.

  tierA      = NotesAndRests | Beams | Signatures            = 1 | 2 | 128
  tierAB     = tierA | Ties | Slurs | Articulations          = | 32 | 64 | 16
  tierAB_dir = tierAB | Directions                           = | 256

The Lyrics bit (16384) is never set (v1 excludes lyrics). Directions bundles
dynamics/wedges with tempo/text/rehearsal/pedal marks; the Tier-B validation
choice between tierAB and tierAB_dir is recorded in D16.
"""

import os as _os
import sys as _sys
from pathlib import Path as _Path

# When run from the repo root, sys.path[0]='' makes the vendored checkout dir
# `musicdiff/` (no __init__.py) shadow the installed package as a namespace
# package. Put the checkout root first so its real `musicdiff/musicdiff`
# package wins regardless of cwd.
_vendored = str(_Path(__file__).resolve().parents[2] / "musicdiff")
if _vendored not in _sys.path:
    _sys.path.insert(0, _vendored)

DETAIL_LEVELS: dict[str, int] = {
    "tierA": 131,
    "tierAB": 243,
    "tierAB_dir": 499,
}

SCHEMA_VERSION = 1

DEFAULT_DATA_ROOT = _os.environ.get("DATA_ROOT", "")
