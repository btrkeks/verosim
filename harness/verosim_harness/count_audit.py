"""DEV-200 active symbol-count audit: C++ extraction vs the cached oracle.

Joins `verosim --count-symbols` totals against the per-pair notation sizes
(n_pred / n_gt) stored in an oracle JSONL (run_oracle output), per side:
pred = .xml, gt = .krn (D14 pair convention). Validation criterion:
aggregate |delta| within ~2% per side, outliers triaged.

Counts are NOT expected identical pair-by-pair (D6: different parsers);
the report ranks outliers and fingerprints them by category so divergence
triage starts at the worst offender.

Usage:
  python -m verosim_harness.count_audit --oracle oracle/dev200_active.jsonl \\
      --bin build/verosim --data-root <root> \\
      --out oracle/count_audit_dev200_active.jsonl --report oracle/reports/active_count_audit.md
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

from . import DEFAULT_DATA_ROOT


def load_oracle_pairs(oracle_path: Path) -> list[dict]:
    pairs = []
    with open(oracle_path) as f:
        for line in f:
            if not line.strip():
                continue
            record = json.loads(line)
            if record.get("error") is not None or record.get("n_pred") is None:
                continue
            pairs.append(record)
    return pairs


def run_counts(bin_path: Path, files: list[str], data_root: str) -> dict[str, dict]:
    """Run verosim --count-symbols over files (relative to data_root)."""
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False) as tmp:
        tmp.write("\n".join(files) + "\n")
        list_path = tmp.name
    try:
        proc = subprocess.run(
            [str(bin_path), "--count-symbols", "--files-from", list_path, "--base-dir", data_root],
            capture_output=True, text=True, check=True,
        )
    finally:
        os.unlink(list_path)
    out: dict[str, dict] = {}
    for line in proc.stdout.splitlines():
        if not line.strip():
            continue
        record = json.loads(line)
        # list mode reports the joined path; recover the relative key
        path = record["path"]
        if path.startswith(data_root):
            path = path[len(data_root):].lstrip("/")
        out[path] = record
    return out


def join_audit(pairs: list[dict], counts: dict[str, dict]) -> list[dict]:
    """One audit row per (pair, side). Pure function — unit-tested."""
    rows = []
    for record in pairs:
        for side, key in (("pred", "n_pred"), ("gt", "n_gt")):
            rel = record["pair"][side]
            cpp = counts.get(rel)
            row = {
                "file": rel,
                "side": side,
                "n_oracle": record[key],
                "n_cpp": None,
                "delta": None,
                "categories": None,
                "warnings": None,
                "error": None,
            }
            if cpp is None:
                row["error"] = "no_cpp_record"
            elif not cpp.get("ok"):
                row["error"] = cpp.get("error", "cpp_failed")
            else:
                row["n_cpp"] = cpp["total"]
                row["delta"] = cpp["total"] - record[key]
                row["categories"] = cpp["categories"]
                row["warnings"] = cpp.get("warnings") or None
            rows.append(row)
    return rows


def summarize(rows: list[dict]) -> dict:
    summary: dict = {"sides": {}, "n_rows": len(rows)}
    for side in ("pred", "gt"):
        side_rows = [r for r in rows if r["side"] == side and r["error"] is None]
        n_total = sum(r["n_oracle"] for r in side_rows)
        abs_delta = sum(abs(r["delta"]) for r in side_rows)
        signed_delta = sum(r["delta"] for r in side_rows)
        summary["sides"][side] = {
            "n_files": len(side_rows),
            "n_errors": sum(1 for r in rows if r["side"] == side and r["error"]),
            "sum_n_oracle": n_total,
            "sum_abs_delta": abs_delta,
            "sum_signed_delta": signed_delta,
            "abs_delta_pct": round(100.0 * abs_delta / n_total, 4) if n_total else None,
            "signed_delta_pct": round(100.0 * signed_delta / n_total, 4) if n_total else None,
            "exact_matches": sum(1 for r in side_rows if r["delta"] == 0),
        }
    return summary


def write_report(rows: list[dict], summary: dict, report_path: Path, top_n: int = 20,
        notes_path: Path | None = None) -> None:
    lines = ["# DEV-200 Active Symbol-Count Audit", ""]
    lines.append("C++ `--count-symbols` totals vs oracle notation sizes "
                 "(`n_pred`/`n_gt`, mode active). Pair convention: pred = .xml, gt = .krn.")
    lines.append("")
    lines.append("| side | files | errors | Σn (oracle) | Σ\\|Δ\\| | Σ\\|Δ\\| % | ΣΔ % | exact |")
    lines.append("|---|---|---|---|---|---|---|---|")
    for side, s in summary["sides"].items():
        lines.append(
            f"| {side} | {s['n_files']} | {s['n_errors']} | {s['sum_n_oracle']} "
            f"| {s['sum_abs_delta']} | {s['abs_delta_pct']}% | {s['signed_delta_pct']}% "
            f"| {s['exact_matches']} |")
    lines.append("")
    lines.append(f"## Top {top_n} outliers by |Δ| / n")
    lines.append("")
    lines.append("| file | side | n oracle | n c++ | Δ | Δ% |")
    lines.append("|---|---|---|---|---|---|")
    scored = [r for r in rows if r["error"] is None and r["n_oracle"]]
    scored.sort(key=lambda r: abs(r["delta"]) / r["n_oracle"], reverse=True)
    for r in scored[:top_n]:
        pct = round(100.0 * r["delta"] / r["n_oracle"], 2)
        lines.append(f"| {r['file']} | {r['side']} | {r['n_oracle']} | {r['n_cpp']} "
                     f"| {r['delta']:+d} | {pct:+.2f}% |")
    errors = [r for r in rows if r["error"]]
    if errors:
        lines.append("")
        lines.append("## C++-side failures")
        lines.append("")
        for r in errors:
            lines.append(f"- `{r['file']}` ({r['side']}): {r['error']}")
    if notes_path and notes_path.exists():
        lines.append("")
        lines.append(notes_path.read_text().rstrip())
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text("\n".join(lines) + "\n")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--oracle", type=Path, required=True)
    ap.add_argument("--bin", type=Path, default=Path("build/verosim"))
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--out", type=Path, help="write per-row audit JSONL here")
    ap.add_argument("--report", type=Path, help="write markdown report here")
    ap.add_argument("--notes", type=Path,
                    help="committed triage-notes markdown appended to the report")
    ap.add_argument("--top", type=int, default=20)
    args = ap.parse_args(argv)

    pairs = load_oracle_pairs(args.oracle)
    files = sorted({record["pair"][side] for record in pairs for side in ("pred", "gt")})
    print(f"{len(pairs)} usable oracle pairs, {len(files)} unique files", file=sys.stderr)

    counts = run_counts(args.bin, files, args.data_root)
    rows = join_audit(pairs, counts)
    summary = summarize(rows)

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with open(args.out, "w") as f:
            for row in rows:
                f.write(json.dumps(row) + "\n")
    if args.report:
        write_report(rows, summary, args.report, args.top, args.notes)

    print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
