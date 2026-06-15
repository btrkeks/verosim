"""Clean-failure regression audit for known PERF/Lieder parse failures.

Every listed file must produce a JSONL record. Failed loads must carry at least
one error message; recovered successful loads are reported so the list can be
cleaned up deliberately, but they are not a robustness regression.
"""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path

from . import DEFAULT_DATA_ROOT
from .gates import clean_failure_audit
from .lists import read_list


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", type=Path, required=True)
    ap.add_argument("--files-from", type=Path, required=True)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args(argv)

    cmd = [
        str(args.bin),
        "--check",
        "--files-from",
        str(args.files_from),
        "--base-dir",
        str(args.data_root),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True, check=True)
    records = [json.loads(line) for line in proc.stdout.splitlines() if line.strip()]
    expected = len(read_list(args.files_from))
    audit = clean_failure_audit(expected, records)

    lines = [
        "# Clean-Failure Regression Audit",
        "",
        f"- input list: `{args.files_from}`",
        f"- expected records: {expected}",
        f"- emitted records: {len(records)}",
        f"- missing records: {audit.completeness.missing}",
        f"- recovered successful loads: {len(audit.recovered_loads)}",
        f"- failures without messages: {len(audit.missing_errors)}",
        "",
        "| path | exception | first error |",
        "|---|---|---|",
    ]
    for r in records:
        first_error = (r.get("errors") or [""])[0]
        lines.append(f"| `{r['path']}` | {r.get('exception')} | {first_error} |")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {args.out}")
    return 0 if audit.ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
