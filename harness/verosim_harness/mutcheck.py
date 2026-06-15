"""Mutation-oracle cross-check.

    python -m verosim_harness.mutcheck [--manifest corpora/mutations/manifest.json]
        [--out oracle/mutcheck.jsonl] [--only CASE_ID ...]

Runs musicdiff (via oracle.run_pair, pred=mutated, gt=base) over every mutation
case and requires analytic expectation == oracle output: exact distance match,
and op-name multiset match where the manifest pins expected_ops. Any mismatch
means we misread a costing rule - resolve (fix the expectation, or record a
paper-vs-code disagreement in the mapping decision, per D12) before relying
on the C++ implementation.
Exits nonzero on any mismatch.
"""

from __future__ import annotations

import argparse
import collections
import json
import sys
from pathlib import Path

from .versions import REPO_ROOT


def check_case(case: dict, mutations_dir: Path) -> dict:
    from .oracle import run_pair

    record = run_pair(
        mutations_dir / case["mutated"],
        mutations_dir / case["base"],
        case["tier"],
    )
    result = {
        "id": case["id"],
        "tier": case["tier"],
        "expected_cost": case["expected_cost"],
        "actual_cost": record["distance"],
        "expected_ops": case["expected_ops"],
        "actual_ops": None,
        "error": record["error"],
        "ok": False,
    }
    if record["error"] is not None or record["pred_parse_error"] or record["gt_parse_error"]:
        result["error"] = record["error"] or record["pred_parse_error"] or record["gt_parse_error"]
        return result
    actual_ops = dict(collections.Counter(op["op"] for op in record["edit_ops"]))
    result["actual_ops"] = actual_ops
    cost_ok = record["distance"] == case["expected_cost"]
    ops_ok = case["expected_ops"] is None or actual_ops == case["expected_ops"]
    result["ok"] = cost_ok and ops_ok
    return result


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--manifest", type=Path, default=REPO_ROOT / "corpora" / "mutations" / "manifest.json"
    )
    ap.add_argument("--out", type=Path)
    ap.add_argument("--only", nargs="*", help="run only these case ids")
    args = ap.parse_args(argv)

    manifest = json.loads(args.manifest.read_text(encoding="utf-8"))
    mutations_dir = args.manifest.parent
    cases = manifest["cases"]
    if args.only:
        cases = [c for c in cases if c["id"] in set(args.only)]

    results = [check_case(c, mutations_dir) for c in cases]

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with open(args.out, "w", encoding="utf-8") as f:
            for r in results:
                f.write(json.dumps(r) + "\n")

    failures = [r for r in results if not r["ok"]]
    for r in failures:
        print(f"MISMATCH {r['id']} ({r['tier']})")
        print(f"  cost: expected {r['expected_cost']}, got {r['actual_cost']}")
        if r["expected_ops"] is not None and r["actual_ops"] != r["expected_ops"]:
            print(f"  ops:  expected {r['expected_ops']}")
            print(f"        got      {r['actual_ops']}")
        if r["error"]:
            print(f"  error: {r['error'].strip().splitlines()[-1]}")
    print(f"mutcheck: {len(results) - len(failures)}/{len(results)} cases match", file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
