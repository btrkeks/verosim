"""Audit musicdiff's coarse Directions bit in experimental mode.

`DetailLevel.Directions` bundles dynamics/hairpins with text
directions, pedal marks, endings, tempos, and other non-v1 categories. This
script quantifies that traffic from cached oracle JSONL records so the
validation gate can keep active mode separate from broader diagnostic reporting.

Usage:
  python -m verosim_harness.directions_audit \
      --oracle oracle/dev200_experimental.jsonl \
      --out oracle/reports/experimental_directions_audit.md
"""

from __future__ import annotations

import argparse
import collections
import json
import re
from pathlib import Path


EXTRA_KIND_RE = re.compile(r"\):([^,@]+)")


def extra_kind_from_repr(repr_text: str) -> str | None:
    """Return the AnnExtra kind from oracle.py's serialized repr, if present."""
    match = EXTRA_KIND_RE.search(repr_text)
    if not match:
        return None
    return match.group(1)


def classify_extra_kind(op: dict) -> str:
    """Classify an extra edit op by inspecting whichever side carries an AnnExtra."""
    for side_name in ("a", "b"):
        side = op.get(side_name)
        if not isinstance(side, dict):
            continue
        if side.get("type") != "AnnExtra":
            continue
        kind = extra_kind_from_repr(side.get("repr", ""))
        if kind:
            return kind
    return "unknown"


def audit(path: Path) -> dict:
    by_kind: collections.Counter[str] = collections.Counter()
    by_kind_op: collections.Counter[tuple[str, str]] = collections.Counter()
    by_header: collections.Counter[str] = collections.Counter()
    examples: dict[str, list[dict]] = collections.defaultdict(list)
    n_records = 0
    n_ok = 0
    n_error = 0

    with path.open(encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            record = json.loads(line)
            n_records += 1
            if record.get("error") is not None or record.get("omr_ned") is None:
                n_error += 1
                continue
            n_ok += 1
            for header, cost in (record.get("edit_distances_dict") or {}).items():
                by_header[header] += cost
            for op in record.get("edit_ops") or []:
                op_name = str(op.get("op", ""))
                if not op_name.startswith("extra"):
                    continue
                cost = int(op.get("cost", 0))
                kind = classify_extra_kind(op)
                by_kind[kind] += cost
                by_kind_op[(kind, op_name)] += cost
                if len(examples[kind]) < 3:
                    examples[kind].append(
                        {
                            "pair": record["pair"]["pred"],
                            "op": op_name,
                            "cost": cost,
                            "repr": [
                                side.get("repr")
                                for side in (op.get("a"), op.get("b"))
                                if isinstance(side, dict) and side.get("type") == "AnnExtra"
                            ],
                        }
                    )

    return {
        "n_records": n_records,
        "n_ok": n_ok,
        "n_error": n_error,
        "by_kind": by_kind,
        "by_kind_op": by_kind_op,
        "by_header": by_header,
        "examples": examples,
    }


def write_markdown(result: dict, oracle_path: Path, out_path: Path) -> None:
    by_kind: collections.Counter[str] = result["by_kind"]
    by_kind_op: collections.Counter[tuple[str, str]] = result["by_kind_op"]
    by_header: collections.Counter[str] = result["by_header"]
    examples: dict[str, list[dict]] = result["examples"]

    lines = [
        "# Experimental Directions Audit",
        "",
        f"Generated from `{oracle_path}` by `verosim_harness.directions_audit`.",
        "",
        f"- records: {result['n_records']}",
        f"- usable oracle records: {result['n_ok']}",
        f"- oracle errors: {result['n_error']}",
        "",
        "## Interpretation",
        "",
        "The `experimental` oracle run is useful diagnostic evidence, but it is not a clean "
        "active-mode gate: musicdiff's Directions bit includes dynamics/hairpins as well "
        "as non-v1 text directions, pedal marks, endings, tempos, and rehearsal marks. "
        "Validation gates correlation on the cleaner `active` mode and reports this "
        "experimental decomposition separately.",
        "",
        "## Extra Cost By Kind",
        "",
        "| kind | cost |",
        "|---|---:|",
    ]
    for kind, cost in by_kind.most_common():
        lines.append(f"| `{kind}` | {cost} |")

    lines += [
        "",
        "## Header Totals",
        "",
        "| header | cost |",
        "|---|---:|",
    ]
    for header, cost in by_header.most_common():
        if cost:
            lines.append(f"| {header} | {cost} |")

    lines += [
        "",
        "## Extra Cost By Kind And Op",
        "",
        "| kind | op | cost |",
        "|---|---|---:|",
    ]
    for (kind, op_name), cost in by_kind_op.most_common():
        lines.append(f"| `{kind}` | `{op_name}` | {cost} |")

    lines += ["", "## Examples", ""]
    for kind, rows in sorted(examples.items()):
        lines.append(f"### `{kind}`")
        lines.append("")
        for row in rows:
            reprs = "<br>".join(f"`{r}`" for r in row["repr"])
            lines.append(f"- `{row['pair']}`: `{row['op']}` cost {row['cost']} {reprs}")
        lines.append("")

    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--oracle", type=Path, required=True)
    ap.add_argument("--out", type=Path)
    args = ap.parse_args(argv)

    result = audit(args.oracle)
    print(
        "directions_audit:",
        f"records={result['n_records']}",
        f"ok={result['n_ok']}",
        f"errors={result['n_error']}",
        sep="\n  ",
    )
    for kind, cost in result["by_kind"].most_common():
        print(f"  {kind}: {cost}")
    if args.out:
        write_markdown(result, args.oracle, args.out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
