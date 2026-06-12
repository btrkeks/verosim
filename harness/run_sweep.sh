#!/usr/bin/env bash
# Crash-resuming wrapper around `verosim --check --files-from` (M1 sweep).
# A hard crash (e.g. humlib segfault) kills the whole process; this records the
# killer file as {"ok":false,"exception":true,...} and resumes with the rest.
# Crash files seed M5's regression suite.
# usage: run_sweep.sh <list> <base-dir> <out.jsonl>   (BIN=build/verosim)
set -u
LIST="$1"; BASE="$2"; OUT="$3"; BIN="${BIN:-build/verosim}"

touch "$OUT"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
grep -v '^#' "$LIST" | grep -v '^$' > "$tmp/all"

while :; do
    sed -n 's/^{"path":"\([^"]*\)".*/\1/p' "$OUT" > "$tmp/done"
    grep -vxF -f "$tmp/done" "$tmp/all" > "$tmp/remaining" || true
    [ -s "$tmp/remaining" ] || break

    "$BIN" --check --files-from "$tmp/remaining" --base-dir "$BASE" >> "$OUT"
    status=$?
    [ $status -eq 0 ] && continue  # loop re-checks remaining; exits when empty

    sed -n 's/^{"path":"\([^"]*\)".*/\1/p' "$OUT" > "$tmp/done"
    killer="$(grep -vxF -f "$tmp/done" "$tmp/remaining" | head -1)"
    if [ -z "$killer" ]; then break; fi
    echo "sweep: $BIN crashed (exit $status) on $killer — recording and resuming" >&2
    printf '{"path":"%s","ok":false,"exception":true,"n_warnings":0,"n_errors":1,"warnings":[],"errors":["verosim process crashed (exit %s)"],"load_ms":0}\n' \
        "$killer" "$status" >> "$OUT"
done
