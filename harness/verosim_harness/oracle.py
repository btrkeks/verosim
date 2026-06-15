"""Run the musicdiff oracle on one pair, capturing edit_ops.

run_pair() mirrors musicdiff._diff_omr_ned_metrics (musicdiff/__init__.py:248,
the OMR-NED recipe) line for line — pred parsed with acceptSyntaxErrors=True,
gt strict, parse failure -> empty Score, notation_size() counted before the
diff, one annotated_scores_diff() call whose cost already includes syntax-fix
costs — while additionally serializing the per-edit operation list, which the
musicdiff CLI cannot emit. Per D12 the vendored code is the spec; any drift
between this mirror and _diff_omr_ned_metrics is a harness bug.

converter21.register() must have been called in this process (run_oracle's
worker initializer does it; call ensure_converter21() for ad-hoc use).
"""

from __future__ import annotations

import time
import traceback
from fractions import Fraction
from pathlib import Path

from . import DETAIL_LEVELS, SCHEMA_VERSION
from .cache import cache_key
from .versions import get_versions

_registered = False


def ensure_converter21() -> None:
    global _registered
    if not _registered:
        import converter21

        converter21.register()
        _registered = True


def _jsonable(value):
    if isinstance(value, Fraction):
        return str(value)
    if isinstance(value, (int, float, str, bool)) or value is None:
        return value
    return str(value)


def _serialize_op_side(obj) -> dict | None:
    if obj is None:
        return None
    d = {"type": type(obj).__name__, "repr": repr(obj)}
    for attr in ("general_note", "extra", "kind", "note_offset", "offset"):
        if hasattr(obj, attr):
            d[attr] = _jsonable(getattr(obj, attr))
    try:
        d["readable"] = obj.readable_str()
    except Exception:
        pass
    return d


def _parse(path: Path, accept_syntax_errors: bool):
    """Parse like the oracle does; returns (score, traceback_or_None)."""
    import music21 as m21

    try:
        score = m21.converter.parse(
            path, forceSource=True, acceptSyntaxErrors=accept_syntax_errors
        )
        if isinstance(score, m21.stream.Opus):
            score = score.scores[0]
        return score, None
    except Exception:
        return m21.stream.Score(), traceback.format_exc()


def new_record(pred_rel: str, gt_rel: str, detail_name: str) -> dict:
    """The one authoritative constructor of the oracle JSONL record schema.

    Every record in an oracle output file comes from here — run_pair fills in
    results, run_oracle's driver fills in errors for pairs that never reached
    a worker (timeout, OOM kill, crash). Keep the schema in this one place."""
    return {
        "pair": {"pred": str(pred_rel), "gt": str(gt_rel)},
        "detail": {"name": detail_name, "value": DETAIL_LEVELS[detail_name]},
        "distance": None,
        "n_pred": None,
        "n_gt": None,
        "omr_ned": None,
        "edit_distances_dict": None,
        "edit_ops": None,
        "runtime_s": None,
        "error": None,
        "pred_parse_error": None,
        "gt_parse_error": None,
        "versions": dict(get_versions(), schema=SCHEMA_VERSION),
        "cache_key": None,
    }


def run_pair(
    pred_path: str | Path,
    gt_path: str | Path,
    detail_name: str,
    data_root: str | Path | None = None,
) -> dict:
    """Returns the oracle record (JSONL schema, see plan). Never raises."""
    ensure_converter21()

    detail_value = DETAIL_LEVELS[detail_name]
    pred_rel, gt_rel = str(pred_path), str(gt_path)
    pred_abs = Path(data_root, pred_path) if data_root else Path(pred_path)
    gt_abs = Path(data_root, gt_path) if data_root else Path(gt_path)

    record = new_record(pred_rel, gt_rel, detail_name)

    start = time.monotonic()
    try:
        record["cache_key"] = cache_key(pred_abs, gt_abs, detail_value)

        from musicdiff import _getInputExtensionsList
        from musicdiff.annotation import AnnScore
        from musicdiff.comparison import Comparison
        from musicdiff.visualization import Visualization

        supported = _getInputExtensionsList()
        if pred_abs.suffix not in supported or gt_abs.suffix not in supported:
            record["error"] = "unsupported_extension"
            return record

        pred_score, record["pred_parse_error"] = _parse(pred_abs, accept_syntax_errors=True)
        gt_score, record["gt_parse_error"] = _parse(gt_abs, accept_syntax_errors=False)

        if len(list(gt_score.parts)) == 0:
            # the oracle returns None here (gt unusable) rather than scoring 1.0
            record["error"] = "gt_empty_or_unparseable"
            return record

        ann_pred = AnnScore(pred_score, detail_value)
        ann_gt = AnnScore(gt_score, detail_value)
        n_pred = ann_pred.notation_size()
        n_gt = ann_gt.notation_size()
        op_list, distance = Comparison.annotated_scores_diff(ann_pred, ann_gt)

        record["distance"] = distance
        record["n_pred"] = n_pred
        record["n_gt"] = n_gt
        record["edit_distances_dict"] = Visualization.get_edit_distances_dict(
            op_list, ann_pred.num_syntax_errors_fixed, detail_value
        )
        record["omr_ned"] = Visualization.get_omr_ned(distance, n_pred, n_gt)
        # ops are (op_name, obj1, obj2, cost) plus an optional 5th element with
        # chord-internal note ids (comparison.py:376-403)
        record["edit_ops"] = [
            {
                "op": op[0],
                "cost": op[3],
                "a": _serialize_op_side(op[1]),
                "b": _serialize_op_side(op[2]),
                "ids": _jsonable(op[4]) if len(op) > 4 else None,
            }
            for op in op_list
        ]
    except Exception:
        record["error"] = traceback.format_exc()
    finally:
        record["runtime_s"] = round(time.monotonic() - start, 3)

    return record


def serve() -> int:
    """Persistent worker (used by run_oracle): one JSON request per stdin line
    {pred, gt, detail, data_root}, one JSON record per stdout line. Runs each
    pair in an isolated process so an OOM kill / crash / hang costs exactly
    one pair: the parent times out, records an error, and respawns."""
    import argparse
    import json
    import os
    import resource
    import sys

    ap = argparse.ArgumentParser()
    ap.add_argument("--mem-gb", type=float, default=0.0)
    args = ap.parse_args()
    if args.mem_gb > 0:
        limit = int(args.mem_gb * 1024**3)
        resource.setrlimit(resource.RLIMIT_AS, (limit, limit))

    # Records go out on a private dup of stdout; fd 1 itself is pointed at
    # stderr, because music21/converter21 print() stray text to stdout and
    # that would corrupt the line protocol.
    protocol_out = os.fdopen(os.dup(1), "w", encoding="utf-8")
    os.dup2(2, 1)
    sys.stdout = sys.stderr

    ensure_converter21()
    from . import cache

    for line in sys.stdin:
        if not line.strip():
            continue
        req = json.loads(line)
        record = run_pair(req["pred"], req["gt"], req["detail"], req["data_root"])
        if record["cache_key"] and record["error"] is None:
            cache.store(record["cache_key"], record)
        protocol_out.write(json.dumps(record) + "\n")
        protocol_out.flush()
        # annotated_scores_diff clears its memoizer at the START of the next
        # diff, so a MemoryError mid-diff would otherwise leave a near-rlimit
        # dict alive through the next pair's parse — one monster pair then
        # poisons the worker and every pair after it MemoryErrors (observed:
        # 84 consecutive failures in the dev200 tierAB run). Clear it now.
        try:
            from musicdiff.comparison import Comparison
            Comparison._clear_memoizer_caches()
        except Exception:
            pass
        if record["error"] and "MemoryError" in record["error"]:
            # Even after the clear, CPython's freed arenas stay mapped and
            # count against RLIMIT_AS. Exit; the parent respawns a fresh
            # worker with a clean address space.
            return 0
    return 0


if __name__ == "__main__":
    raise SystemExit(serve())
