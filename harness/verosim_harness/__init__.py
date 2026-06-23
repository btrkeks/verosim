"""VeroSim oracle harness.

METRIC_MODES pins the mode<->DetailLevel mapping. Values are literal ints so
this package imports without musicdiff (sampling is stdlib-only);
test_metric_modes.py recomputes them from musicdiff.DetailLevel to catch
vendored-enum drift.

  active       = NotesAndRests | Beams | Signatures
                 | Ties | Slurs | Articulations | Barlines   = 755
  experimental = active | Directions                         = 1011

The Lyrics bit (16384) is never set (v1 excludes lyrics). Directions bundles
dynamics/wedges with tempo/text/rehearsal/pedal marks, so it remains a broader
diagnostic mode rather than the default supported surface.
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

METRIC_MODES: dict[str, int] = {
    "active": 755,
    "experimental": 1011,
}

SCHEMA_VERSION = 2

DEFAULT_DATA_ROOT = _os.environ.get("DATA_ROOT", "")
