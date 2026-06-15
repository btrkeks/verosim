"""DEV-200 correlation audit of C++ OMR-NED vs the Python oracle.

Joins one `verosim --pairs` run against the cached oracle records and computes
the Spearman rank correlation of omr_ned, twice:

  raw      — against the oracle's omr_ned as stored.
  adjusted — against the oracle's omr_ned with the 'bad kern syntax OMR-ED'
             component removed: that cost comes from converter21's
             acceptSyntaxErrors fix count, which the C++ side deliberately
             does not model (D4 deferred). If raw fails the gate but adjusted
             passes, that is a gate-interpretation decision (new Dn), not an
             engine bug to chase.

The gate (exit status) is the RAW Spearman vs --threshold. The report ranks
the most divergent pairs by |cpp - oracle| omr_ned with per-category
edit_distances_dict deltas, so triage starts at the worst offender with its
divergence already localized to a category; re-run a single pair with
`verosim <pred> <gt> --ops` to diff op lists against the oracle's edit_ops.

Usage:
  python -m verosim_harness.corr_audit --oracle oracle/dev200_tierA.jsonl \\
      --bin build/verosim --data-root <root> \\
      --out oracle/corr_audit_dev200.jsonl --report oracle/reports/tier_a_correlation_audit.md
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from . import DEFAULT_DATA_ROOT
from .gates import threshold_status
from .stats import spearman

SYNTAX_KEY = "bad kern syntax OMR-ED"


def load_oracle(path: Path) -> list[dict]:
    records = []
    with open(path) as f:
        for line in f:
            if not line.strip():
                continue
            r = json.loads(line)
            if r.get("error") is None and r.get("omr_ned") is not None:
                records.append(r)
    return records


def run_pairs(bin_path: Path, pairs: list[tuple[str, str]], data_root: str, detail: str) -> dict[tuple, dict]:
    with tempfile.NamedTemporaryFile("w", suffix=".tsv", delete=False) as tmp:
        for pred, gt in pairs:
            tmp.write(f"{pred}\t{gt}\n")
        pairs_path = tmp.name
    cmd = [str(bin_path), "--batch", pairs_path, "--base-dir", data_root]
    if detail != "tierAB":
        cmd += ["--detail", detail]
    proc = subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=True,
    )
    out: dict[tuple, dict] = {}
    for line in proc.stdout.splitlines():
        if not line.strip():
            continue
        r = json.loads(line)
        pred = r["pair"]["pred"]
        gt = r["pair"]["gt"]
        if pred.startswith(data_root):
            pred = pred[len(data_root):].lstrip("/")
        if gt.startswith(data_root):
            gt = gt[len(data_root):].lstrip("/")
        out[(pred, gt)] = r
    return out


def detail_from_oracle_path(path: Path) -> str:
    stem = path.stem
    for detail in ("tierAB_dir", "tierAB", "tierA"):
        if stem.endswith(f"_{detail}"):
            return detail
    raise ValueError(
        f"cannot infer detail tier from {path}; expected a name ending in "
        "tierA, tierAB, or tierAB_dir"
    )


def oracle_adjusted_omr_ned(r: dict) -> float:
    syntax = (r.get("edit_distances_dict") or {}).get(SYNTAX_KEY, 0)
    total = r["n_pred"] + r["n_gt"]
    if total == 0:
        return 0.0
    return (r["distance"] - syntax) / total


def dict_delta(cpp: dict | None, oracle: dict | None) -> dict[str, int]:
    delta = {}
    for key in sorted(set(cpp or {}) | set(oracle or {})):
        d = (cpp or {}).get(key, 0) - (oracle or {}).get(key, 0)
        if d != 0:
            delta[key] = d
    return delta


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--oracle", type=Path, required=True)
    ap.add_argument("--bin", type=Path, required=True)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--report", type=Path)
    ap.add_argument("--threshold", type=float, default=0.95)
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument(
        "--detail",
        choices=("tierA", "tierAB", "tierAB_dir"),
        help="detail tier for the C++ run; inferred from --oracle filename by default",
    )
    ap.add_argument(
        "--explained",
        type=Path,
        help="TSV of piece_id<TAB>reason pairs whose divergence is an explained "
        "importer difference (D6); Spearman is reported with and without them",
    )
    args = ap.parse_args(argv)

    explained: dict[str, str] = {}
    if args.explained:
        for line in args.explained.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                piece, reason = line.split("\t")
                explained[piece] = reason

    oracle = load_oracle(args.oracle)
    pairs = [(r["pair"]["pred"], r["pair"]["gt"]) for r in oracle]
    detail_label = args.detail or detail_from_oracle_path(args.oracle)
    cpp = run_pairs(args.bin, pairs, str(args.data_root), detail_label)

    rows = []
    cpp_failures = []
    for r in oracle:
        key = (r["pair"]["pred"], r["pair"]["gt"])
        c = cpp.get(key)
        if c is None or not c["ok"]:
            cpp_failures.append({"pair": r["pair"], "error": (c or {}).get("error", "missing")})
            continue
        piece = Path(r["pair"]["pred"]).stem
        rows.append(
            {
                "pair": r["pair"],
                "explained": explained.get(piece),
                "oracle_omr_ned": r["omr_ned"],
                "oracle_omr_ned_adj": oracle_adjusted_omr_ned(r),
                "cpp_omr_ned": c["omr_ned"],
                "delta": c["omr_ned"] - r["omr_ned"],
                "oracle_n": [r["n_pred"], r["n_gt"]],
                "cpp_n": [c["n_pred"], c["n_gt"]],
                "oracle_distance": r["distance"],
                "cpp_distance": c["distance"],
                "dict_delta": dict_delta(c.get("edit_distances_dict"), r.get("edit_distances_dict")),
            }
        )

    raw = spearman([x["cpp_omr_ned"] for x in rows], [x["oracle_omr_ned"] for x in rows])
    adj = spearman([x["cpp_omr_ned"] for x in rows], [x["oracle_omr_ned_adj"] for x in rows])
    unexplained = [x for x in rows if not x["explained"]]
    minus_explained = (
        spearman(
            [x["cpp_omr_ned"] for x in unexplained], [x["oracle_omr_ned"] for x in unexplained]
        )
        if explained
        else None
    )
    divergent = sorted(rows, key=lambda x: abs(x["delta"]), reverse=True)

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with open(args.out, "w") as f:
            for row in rows:
                f.write(json.dumps(row) + "\n")

    summary = [
        f"pairs joined: {len(rows)} (oracle usable: {len(oracle)}, C++ failures: {len(cpp_failures)})",
        f"Spearman (raw):      {raw:.4f}",
        f"Spearman (adjusted): {adj:.4f}  [oracle minus '{SYNTAX_KEY}', D4]",
    ]
    if minus_explained is not None:
        summary.append(
            f"Spearman (minus {len(rows) - len(unexplained)} explained pairs): "
            f"{minus_explained:.4f}  [{args.explained}]"
        )
    summary.append(
        f"regression floor (cross-importer audit): raw >= {args.threshold} -> "
        f"{threshold_status(raw, args.threshold)}"
    )
    print("corr_audit:", *summary, sep="\n  ")

    if args.report:
        corpus_label = args.oracle.stem
        suffix = f"_{detail_label}"
        if corpus_label.endswith(suffix):
            corpus_label = corpus_label[: -len(suffix)]
        lines = [
            f"# {corpus_label} {detail_label} correlation audit",
            "",
            "C++ `verosim` OMR-NED vs the cached Python oracle "
            f"(`{args.oracle}`, DetailLevel {detail_label}). Generated by "
            "`verosim_harness.corr_audit`.",
            "",
            *(f"- {s}" for s in summary),
            "",
            f"## Top {args.top} divergent pairs",
            "",
            "Positive delta = C++ scores the pair as more different than the oracle does.",
            "Per-category deltas localize the divergence; re-run a pair with",
            "`verosim <pred> <gt> --ops` to diff op lists against the oracle's edit_ops.",
            "",
            "| pair (pred) | oracle | oracle adj | C++ | delta | category deltas (C++ - oracle) |",
            "|---|---|---|---|---|---|",
        ]
        for row in divergent[: args.top]:
            cats = ", ".join(f"{k}: {v:+d}" for k, v in row["dict_delta"].items()) or "—"
            if row["explained"]:
                cats = f"**explained: {row['explained']}** — {cats}"
            lines.append(
                f"| {row['pair']['pred']} | {row['oracle_omr_ned']:.4f} "
                f"| {row['oracle_omr_ned_adj']:.4f} | {row['cpp_omr_ned']:.4f} "
                f"| {row['delta']:+.4f} | {cats} |"
            )
        if cpp_failures:
            lines += ["", "## C++ failures", ""]
            lines += [f"- {f['pair']['pred']}: {f['error']}" for f in cpp_failures]
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text("\n".join(lines) + "\n", encoding="utf-8")

    return 0 if raw >= args.threshold else 1


if __name__ == "__main__":
    sys.exit(main())
