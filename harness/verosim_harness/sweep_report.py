"""Aggregate `verosim --check` JSONL into the parse-coverage report.

    python -m verosim_harness.sweep_report oracle/sweep_*.jsonl --out oracle/reports/parse_coverage.md
"""

from __future__ import annotations

import argparse
import collections
import datetime
import json
import sys
from pathlib import Path


def dataset_of(path: str) -> str:
    parts = Path(path).parts
    if len(parts) >= 2 and parts[0] in ("train", "val"):
        return f"{parts[0]}/{parts[1]}"
    return "(other)"


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("jsonl", nargs="+", type=Path)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--top", type=int, default=20, help="top-N warning strings to list")
    args = ap.parse_args(argv)

    by_dataset: dict[str, list[dict]] = collections.defaultdict(list)
    for path in args.jsonl:
        for line in path.read_text(encoding="utf-8").splitlines():
            if line:
                r = json.loads(line)
                by_dataset[dataset_of(r["path"])].append(r)

    lines = [
        "# Verovio Parse-Coverage Sweep",
        "",
        f"Generated {datetime.date.today().isoformat()} by "
        "`verosim --check --files-from <list>` + `verosim_harness.sweep_report` "
        f"over: {', '.join(p.name for p in args.jsonl)}.",
        "",
        "Counts are per file; warning counts are unique-messages-per-file "
        "(Verovio dedups identical log lines within one load).",
        "",
        "| dataset | files | loaded ok | % ok | with warnings | % warn | exceptions |",
        "|---|---|---|---|---|---|---|",
    ]
    for ds in sorted(by_dataset):
        rs = by_dataset[ds]
        n = len(rs)
        ok = sum(r["ok"] for r in rs)
        warn = sum(1 for r in rs if r["n_warnings"] > 0)
        exc = sum(1 for r in rs if r["exception"])
        lines.append(
            f"| {ds} | {n} | {ok} | {100 * ok / n:.2f}% | {warn} | {100 * warn / n:.2f}% | {exc} |"
        )

    for ds in sorted(by_dataset):
        rs = by_dataset[ds]
        failures = [r for r in rs if not r["ok"]]
        counter: collections.Counter[str] = collections.Counter()
        for r in rs:
            counter.update(r["warnings"])
        lines += ["", f"## {ds}", ""]
        if failures:
            lines.append(f"### load failures ({len(failures)})")
            lines.append("")
            for r in failures[:50]:
                first_err = r["errors"][0] if r["errors"] else "(no error message)"
                lines.append(f"- `{r['path']}` — {first_err}")
            if len(failures) > 50:
                lines.append(f"- … and {len(failures) - 50} more")
            lines.append("")
        if counter:
            lines.append(f"### top warnings ({sum(counter.values())} total, {len(counter)} distinct)")
            lines.append("")
            for msg, count in counter.most_common(args.top):
                lines.append(f"- {count}× {msg}")
        else:
            lines.append("No warnings.")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
