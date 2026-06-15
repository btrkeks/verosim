"""PERF-10K identity benchmark for the C++ batch runner.

Runs `verosim --batch` on a fixed sample of PERF-10K self-pairs and writes a
small Markdown report. If a Python oracle summary is supplied, the report also
records the speedup factor against that wall time.
"""

from __future__ import annotations

import argparse
import json
import random
import subprocess
import tempfile
import time
from pathlib import Path

from . import DEFAULT_DATA_ROOT
from .gates import RecordCompleteness
from .lists import read_list
from .versions import REPO_ROOT

PERF_BENCH_SEED = "verosim-perf-bench"


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    i = round((len(values) - 1) * pct)
    return sorted(values)[i]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", type=Path, required=True)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--sample", type=int, default=1000)
    ap.add_argument("--jobs", type=int, default=0, help="0 = hardware concurrency in verosim")
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--python-summary", type=Path, help="optional run_oracle summary JSON")
    args = ap.parse_args(argv)

    files = read_list(REPO_ROOT / "corpora" / "perf10k.txt")
    sample_n = min(args.sample, len(files))
    sample = random.Random(PERF_BENCH_SEED).sample(files, sample_n)

    with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False) as tmp:
        for path in sample:
            tmp.write(f"{path}\t{path}\n")
        pairs_path = tmp.name

    cmd = [
        str(args.bin),
        "--batch",
        pairs_path,
        "--base-dir",
        str(args.data_root),
        "--jobs",
        str(args.jobs),
    ]
    start = time.monotonic()
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    wall_s = time.monotonic() - start

    records = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    ok = [r for r in records if r.get("ok")]
    failures = [r for r in records if not r.get("ok")]
    completeness = RecordCompleteness(expected=sample_n, emitted=len(records))
    omr_nonzero = [r for r in ok if r.get("distance") != 0]
    per_record_ms = [
        sum((r.get("timings_ms") or {}).values())
        for r in records
        if isinstance(r.get("timings_ms"), dict)
    ]

    python_wall_s = None
    speedup = None
    if args.python_summary and args.python_summary.is_file():
        summary = json.loads(args.python_summary.read_text(encoding="utf-8"))
        python_wall_s = summary.get("wall_s")
        if python_wall_s and wall_s > 0:
            speedup = python_wall_s / wall_s

    lines = [
        "# PERF-10K Identity Benchmark",
        "",
        f"- sample seed: `{PERF_BENCH_SEED}`",
        f"- requested sample: {args.sample}",
        f"- records: {len(records)}",
        f"- missing records: {completeness.missing}",
        f"- loaded ok: {len(ok)}",
        f"- load failures: {len(failures)}",
        f"- nonzero identity distances: {len(omr_nonzero)}",
        f"- C++ wall time: {wall_s:.3f}s",
        f"- C++ throughput: {len(records) / wall_s:.1f} pairs/s" if wall_s > 0 else "- C++ throughput: n/a",
    ]
    if per_record_ms:
        lines += [
            f"- per-record internal timing p50: {percentile(per_record_ms, 0.50):.1f} ms",
            f"- per-record internal timing p95: {percentile(per_record_ms, 0.95):.1f} ms",
        ]
    if python_wall_s is not None:
        lines.append(f"- Python oracle wall time: {python_wall_s:.3f}s")
    if speedup is not None:
        lines.append(f"- speedup factor vs Python: {speedup:.1f}x")
    if failures:
        lines += ["", "## Load failures", ""]
        for r in failures[:20]:
            lines.append(f"- `{r['pair']['pred']}`: {r.get('error')}")
    if omr_nonzero:
        lines += ["", "## Nonzero identity distances", ""]
        for r in omr_nonzero[:20]:
            lines.append(f"- `{r['pair']['pred']}`: distance={r.get('distance')}")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    return 1 if not completeness.ok or failures or omr_nonzero else 0


if __name__ == "__main__":
    raise SystemExit(main())
