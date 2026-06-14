"""M5 clean-failure regression audit for known PERF/Lieder parse failures.

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


def expected_count(path: Path) -> int:
    return sum(
        1
        for line in path.read_text(encoding="utf-8").splitlines()
        if line.strip() and not line.startswith("#")
    )


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", type=Path, required=True)
    ap.add_argument("--files-from", type=Path, required=True)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT)
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
    expected = expected_count(args.files_from)

    missing_records = expected - len(records)
    recovered_loads = [r for r in records if r.get("ok")]
    missing_errors = [r for r in records if not r.get("ok") and not r.get("errors")]

    lines = [
        "# M5 clean-failure regression audit",
        "",
        f"- input list: `{args.files_from}`",
        f"- expected records: {expected}",
        f"- emitted records: {len(records)}",
        f"- missing records: {missing_records}",
        f"- recovered successful loads: {len(recovered_loads)}",
        f"- failures without messages: {len(missing_errors)}",
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
    return 1 if missing_records or missing_errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
