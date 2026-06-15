#!/usr/bin/env bash
# Oracle smoke check: exercise the riskiest assumptions on one
# lieder pair before any architecture-dependent code.
#   Q1: does Python musicdiff run end-to-end incl. the kern side via music21? (R2 probe)
#   Q2: can DetailLevel express "Tier A only"? (-i notesandrests beams signatures)
#   Q3: per-pair runtime, to size the DEV-200 x per-tier oracle precompute.
# Deliberately no `set -e`: a musicdiff failure is a finding to record, not a crash.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="$ROOT/harness/.venv/bin/python"
: "${DATA_ROOT:?Set DATA_ROOT=/path/to/data/interim before running harness/oracle_smoke.sh}"
DATA="$DATA_ROOT/train/openscore-lieder"
XML="$DATA/0_raw_xml/lc28688206.xml"
KRN="$DATA/1_kern_conversions/lc28688206.krn"
OUT="$ROOT/oracle/reports/oracle_smoke_results.md"
mkdir -p "$(dirname "$OUT")"

run_case() { # name, then musicdiff args...
    local name="$1"; shift
    local t0 t1 dt rc output
    t0=$(date +%s.%N)
    # -P: keep cwd off sys.path, or the musicdiff *checkout dir* shadows the
    # installed package when run from the repo root
    output=$("$PY" -P -m musicdiff "$@" 2>&1); rc=$?
    t1=$(date +%s.%N)
    dt=$(echo "$t1 - $t0" | bc)
    {
        printf '## %s\n\n' "$name"
        printf -- '- command: `python -m musicdiff %s`\n' "$*"
        printf -- '- exit code: %s\n- wall time: %.1fs\n\n' "$rc" "$dt"
        printf '```\n%s\n```\n\n' "$output"
    } >>"$OUT"
    printf '%s' "$dt"
}

{
    echo "# Oracle Smoke Results ($(date -u +%FT%TZ))"
    echo
    echo "- pair: lc28688206 (openscore-lieder, .xml vs paired .krn)"
    echo "- musicdiff: $(git -C "$ROOT/musicdiff" rev-parse HEAD)"
    echo "- python: $("$PY" --version 2>&1), music21 $("$PY" -c 'import music21; print(music21.__version__)' 2>&1)"
    echo
} >"$OUT"

# positionals first: -i/-o are nargs='*' and would greedily swallow the file args
T_FULL=$(run_case "Q1: full detail (default allobjects), omrned output — kern via music21 (R2 probe)" \
    "$XML" "$KRN" -o omrned)
T_TIERA=$(run_case "Q2: Tier A only (-i notesandrests beams signatures)" \
    "$XML" "$KRN" -i notesandrests beams signatures -o omrned)

{
    echo "## Q3: oracle precompute sizing"
    echo
    printf -- '- per-pair wall time: full detail %.1fs, Tier A %.1fs\n' "$T_FULL" "$T_TIERA"
    printf -- '- DEV-200 x 3 detail levels, single-threaded upper bound (full-detail rate): %.0f min\n' \
        "$(echo "$T_FULL * 200 * 3 / 60" | bc -l)"
    NPROC=$(nproc)
    printf -- '- parallel over %s cores: ~%.0f min\n' "$NPROC" \
        "$(echo "$T_FULL * 200 * 3 / 60 / $NPROC" | bc -l)"
} >>"$OUT"

echo "wrote $OUT"
