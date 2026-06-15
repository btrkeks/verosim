#!/usr/bin/env bash
# Crash-resuming wrapper around `verosim --check --files-from`.
# A hard crash (e.g. humlib segfault) kills the whole process; this records the
# killer file as {"ok":false,"exception":true,...} and resumes with the rest.
# Crash files seed the clean-failure regression suite.
# usage: run_sweep.sh <list> <base-dir> <out.jsonl>   (BIN=build/verosim)
# PROBE: known-good file used to distinguish "crashed on the first remaining
# file" from "binary cannot start at all" when an iteration makes no progress.
set -u
LIST="$1"; BASE="$2"; OUT="$3"; BIN="${BIN:-build/verosim}"
PROBE="${PROBE:-tests/fixtures/tiny.krn}"

touch "$OUT"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
grep -v '^#' "$LIST" | grep -v '^$' > "$tmp/all"

# JSON-decode the done paths so they compare equal to the raw list lines even
# for characters WriteJsonString escapes; a truncated tail line (crash mid-
# write) is skipped, so that file simply gets re-checked.
extract_done() {
    python3 - "$OUT" > "$tmp/done" <<'EOF'
import json, sys
for line in open(sys.argv[1], encoding="utf-8"):
    line = line.strip()
    if not line:
        continue
    try:
        print(json.loads(line)["path"])
    except (json.JSONDecodeError, KeyError):
        pass
EOF
}

while :; do
    extract_done
    grep -vxF -f "$tmp/done" "$tmp/all" > "$tmp/remaining" || true
    [ -s "$tmp/remaining" ] || break
    n_before=$(wc -l < "$tmp/done")

    "$BIN" --check --files-from "$tmp/remaining" --base-dir "$BASE" >> "$OUT"
    status=$?
    [ $status -eq 0 ] && continue  # loop re-checks remaining; exits when empty

    extract_done
    n_after=$(wc -l < "$tmp/done")
    if [ "$n_after" -eq "$n_before" ]; then
        # Zero new records. Either the binary crashed on the very first
        # remaining file, or it cannot start at all (wrong BIN, missing Verovio
        # resources). Only the probe tells them apart; attributing a startup
        # failure to files would fabricate a crash record for the whole corpus.
        if [ ! -f "$PROBE" ] || ! "$BIN" --check "$PROBE" > /dev/null 2>&1; then
            echo "sweep: $BIN exited $status without checking any file — aborting" >&2
            exit "$status"
        fi
    fi
    killer="$(grep -vxF -f "$tmp/done" "$tmp/remaining" | head -1)"
    if [ -z "$killer" ]; then break; fi
    echo "sweep: $BIN crashed (exit $status) on $killer — recording and resuming" >&2
    python3 - "$killer" "$status" >> "$OUT" <<'EOF'
import json, sys
print(json.dumps({
    "path": sys.argv[1], "ok": False, "exception": True,
    "n_warnings": 0, "n_errors": 1, "warnings": [],
    "errors": [f"verosim process crashed (exit {sys.argv[2]})"], "load_ms": 0,
}))
EOF
done
