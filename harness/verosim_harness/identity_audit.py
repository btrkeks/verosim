"""Identity validation: OMR-NED(x, x) == 0, exactly, everywhere.

Builds (x, x) pairs from every file of DEV-200 (both columns: 200 xml + 200
krn) plus a fixed-seed sample of PERF-10K, runs one `verosim --pairs` pass,
and requires distance == 0 on every pair that loads. Files Verovio cannot
load are reported separately (parse coverage is rung-independent, measured in
the parse-coverage audit); a loadable file with nonzero self-distance fails
the gate.

Usage:
  python -m verosim_harness.identity_audit --bin build/verosim \\
      [--data-root <root>] [--perf-sample 1000] [--out oracle/identity_gate.jsonl]
"""

from __future__ import annotations

import argparse
import json
import random
import subprocess
import sys
import tempfile
from pathlib import Path

from . import DEFAULT_DATA_ROOT
from .gates import RecordCompleteness
from .lists import read_list
from .versions import REPO_ROOT

PERF_SAMPLE_SEED = "verosim-identity-gate"


def collect_files(perf_sample: int) -> list[str]:
    files: list[str] = []
    for row in read_list(REPO_ROOT / "corpora" / "dev200.tsv"):
        pred, gt = row.split("\t")
        files.extend((pred, gt))
    perf = read_list(REPO_ROOT / "corpora" / "perf10k.txt")
    files.extend(random.Random(PERF_SAMPLE_SEED).sample(perf, perf_sample))
    return files


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", type=Path, required=True)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--perf-sample", type=int, default=1000)
    ap.add_argument("--out", type=Path, help="write the per-pair JSONL records here")
    args = ap.parse_args(argv)

    files = collect_files(args.perf_sample)
    with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False) as tmp:
        for f in files:
            tmp.write(f"{f}\t{f}\n")
        pairs_path = tmp.name

    proc = subprocess.run(
        [str(args.bin), "--pairs", pairs_path, "--base-dir", str(args.data_root)],
        capture_output=True,
        text=True,
        check=True,
    )

    records = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(proc.stdout, encoding="utf-8")

    load_failures = [r for r in records if not r["ok"]]
    nonzero = [r for r in records if r["ok"] and r["distance"] != 0]
    completeness = RecordCompleteness(expected=len(files), emitted=len(records))

    print(f"identity_audit: {len(records)} self-pairs ({len(files)} requested)")
    print(f"  loadable, distance == 0: {len(records) - len(load_failures) - len(nonzero)}")
    print(f"  load failures (not gate-relevant, see parse coverage): {len(load_failures)}")
    print(f"  GATE: loadable with distance != 0: {len(nonzero)}")
    for r in nonzero[:20]:
        print(f"    {r['pair']['pred']}: distance={r['distance']}")

    if not completeness.ok:
        print(f"identity_audit: record count mismatch ({len(records)} != {len(files)})")
        return 2
    return 1 if nonzero else 0


if __name__ == "__main__":
    sys.exit(main())
