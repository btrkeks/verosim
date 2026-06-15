#!/usr/bin/env bash
# Creates the oracle venv. Python and music21 pins: see PINNED_VERSIONS.md
# (music21 must stay <10: converter21 4.0.1 breaks kern parsing on music21 10).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
uv venv --python 3.12 "$ROOT/harness/.venv"
uv pip install --python "$ROOT/harness/.venv/bin/python" -e "$ROOT/musicdiff" 'music21<10'
uv pip install --python "$ROOT/harness/.venv/bin/python" -e "$ROOT/harness"
