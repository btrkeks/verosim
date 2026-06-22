"""Parallel oracle driver.

    python -m verosim_harness.run_oracle --corpus corpora/dev200.tsv \
        --mode active --jobs 2 --out oracle/dev200_active.jsonl \
        [--summary-out corpora/summaries/dev200_active.json] [--data-root DIR]

Reads a pair list (TSV: pred<TAB>gt, '#' comments), distributes pairs over N
persistent worker subprocesses (verosim_harness.oracle serve mode, line
protocol), appends JSONL records to --out as they complete (resume = pairs
already in --out are skipped), and caches every record by content hash.

Workers are real subprocesses on purpose: music21/converter21 can hang, eat
memory until the kernel OOM-kills them, or crash in C extensions on
pathological corpus files. Here any of those costs exactly one pair — the
driver records an error for the in-flight pair, respawns the worker, and
keeps going. (A ProcessPoolExecutor variant deadlocked on exactly this.)
"""

from __future__ import annotations

import argparse
import json
import queue
import statistics
import subprocess
import sys
import threading
import time
from pathlib import Path

from . import DEFAULT_DATA_ROOT, METRIC_MODES
from . import cache
from .oracle import new_record
from .versions import get_versions


def read_pairs(corpus_path: Path) -> list[tuple[str, str]]:
    pairs = []
    for line in corpus_path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        cols = line.split("\t")
        if len(cols) != 2:
            raise ValueError(f"{corpus_path}: expected 2 tab-separated columns, got {line!r}")
        pairs.append((cols[0], cols[1]))
    return pairs


def error_record(pred: str, gt: str, mode_name: str, message: str) -> dict:
    record = new_record(pred, gt, mode_name)
    record["error"] = message
    return record


class Worker:
    """One persistent oracle subprocess; restarted on crash/timeout."""

    def __init__(self, mem_gb: float):
        self.mem_gb = mem_gb
        self.proc: subprocess.Popen | None = None
        # kill() runs on both the worker thread and the watchdog timer thread;
        # the lock prevents a double-kill None-deref, and the flag (set under
        # the lock by the timer) is what distinguishes timeout from crash —
        # Timer.is_alive() cannot, since it is True while the callback runs.
        self._lock = threading.Lock()
        self._timed_out = False

    def _spawn(self) -> None:
        with self._lock:
            self.proc = subprocess.Popen(
                [sys.executable, "-m", "verosim_harness.oracle", "--mem-gb", str(self.mem_gb)],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.DEVNULL,
                text=True,
            )

    def kill(self, *, timed_out: bool = False) -> None:
        with self._lock:
            if timed_out:
                self._timed_out = True
            if self.proc is not None:
                self.proc.kill()
                self.proc.wait()
                self.proc = None

    def run(self, pred: str, gt: str, mode: str, data_root: str, timeout_s: float) -> dict:
        if self.proc is None or self.proc.poll() is not None:
            self._spawn()
        assert self.proc is not None and self.proc.stdin and self.proc.stdout
        try:
            self.proc.stdin.write(
                json.dumps({"pred": pred, "gt": gt, "mode": mode, "data_root": data_root})
                + "\n"
            )
            self.proc.stdin.flush()
        except (BrokenPipeError, OSError):
            self.kill()
            return error_record(pred, gt, mode, "worker died before accepting the pair")

        # readline with a hang watchdog: the timer kills the subprocess, which
        # unblocks readline with EOF.
        self._timed_out = False
        timer = threading.Timer(timeout_s, lambda: self.kill(timed_out=True))
        timer.start()
        try:
            line = self.proc.stdout.readline()
        finally:
            timer.cancel()
        if not line:
            # Takes the lock, so an in-flight timer callback finishes first
            # and its _timed_out write is visible below.
            self.kill()
            reason = (
                f"worker timeout after {timeout_s}s"
                if self._timed_out
                else "worker died (OOM kill or crash)"
            )
            return error_record(pred, gt, mode, reason)
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            self.kill()
            return error_record(
                pred, gt, mode, f"worker emitted a non-record line: {line[:200]!r}"
            )


def summarize(records: list[dict], mode_name: str, wall_s: float) -> dict:
    ok = [r for r in records if r["error"] is None]
    errors = [r for r in records if r["error"] is not None]
    parse_issues = [r for r in ok if r["pred_parse_error"] or r["gt_parse_error"]]
    neds = sorted(r["omr_ned"] for r in ok if r["omr_ned"] is not None)
    summary = {
        "mode": mode_name,
        "n_pairs": len(records),
        "n_ok": len(ok),
        "n_error": len(errors),
        "n_ok_with_parse_substitution": len(parse_issues),
        "omr_ned": None,
        "wall_s": round(wall_s, 1),
        "versions": get_versions(),
        "errors": [
            {
                "pair": r["pair"],
                "error": r["error"].strip().splitlines()[-1] if r["error"] else None,
            }
            for r in errors
        ],
        "parse_substitutions": [
            {"pair": r["pair"], "side": "pred" if r["pred_parse_error"] else "gt"}
            for r in parse_issues
        ],
    }
    if neds:
        quartiles = statistics.quantiles(neds, n=4) if len(neds) >= 2 else [neds[0]] * 3
        summary["omr_ned"] = {
            "min": neds[0],
            "q1": quartiles[0],
            "median": quartiles[1],
            "q3": quartiles[2],
            "max": neds[-1],
            "mean": statistics.fmean(neds),
        }
    return summary


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--corpus", required=True, type=Path)
    ap.add_argument("--mode", required=True, choices=sorted(METRIC_MODES))
    ap.add_argument("--jobs", type=int, default=2)
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--summary-out", type=Path)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--no-cache", action="store_true", help="recompute even on cache hit")
    ap.add_argument(
        "--worker-mem-gb", type=float, default=8.0,
        help="per-worker RLIMIT_AS in GiB (0 = unlimited). Generous on purpose: "
             "near the limit CPython grinds in GC before failing; workers are "
             "isolated subprocesses, so even a kernel OOM kill costs one pair.",
    )
    ap.add_argument("--timeout", type=float, default=600.0, help="per-pair timeout in seconds")
    args = ap.parse_args(argv)

    pairs = read_pairs(args.corpus)
    args.out.parent.mkdir(parents=True, exist_ok=True)

    # Resume: skip pairs already present in --out; keep their records for the summary.
    done: dict[tuple[str, str], dict] = {}
    if args.out.is_file():
        for line in args.out.read_text(encoding="utf-8").splitlines():
            if line:
                r = json.loads(line)
                done[(r["pair"]["pred"], r["pair"]["gt"])] = r

    todo = [(p, g) for p, g in pairs if (p, g) not in done]
    records = [done[(p, g)] for p, g in pairs if (p, g) in done]
    print(
        f"{args.corpus.name} @ {args.mode}: {len(pairs)} pairs, "
        f"{len(records)} already in {args.out}, {len(todo)} to run",
        file=sys.stderr,
    )

    start = time.monotonic()
    mode_value = METRIC_MODES[args.mode]

    out_lock = threading.Lock()
    work: queue.Queue[tuple[str, str]] = queue.Queue()
    n_done = 0

    with open(args.out, "a", encoding="utf-8") as out:

        def emit(record: dict) -> None:
            nonlocal n_done
            with out_lock:
                out.write(json.dumps(record) + "\n")
                out.flush()
                records.append(record)
                n_done += 1
                if n_done % 20 == 0 or n_done == len(todo):
                    print(f"  {n_done}/{len(todo)} done", file=sys.stderr)

        # Cheap cache pass in the parent (hashing only, no music21).
        for pred, gt in todo:
            hit = None
            if not args.no_cache:
                try:
                    key = cache.cache_key(
                        Path(args.data_root, pred), Path(args.data_root, gt), mode_value
                    )
                except OSError as exc:
                    # Missing/unreadable corpus file costs exactly this pair,
                    # same contract as a worker-side failure.
                    emit(error_record(pred, gt, args.mode, f"cache pre-pass: {exc}"))
                    continue
                hit = cache.load(key)
            if hit is not None:
                # The key is content-based; the stored pair paths are from the
                # run that computed the record. Re-stamp them so the resume
                # index and downstream joins see the pair we were asked for.
                emit(dict(hit, pair={"pred": pred, "gt": gt}))
            else:
                work.put((pred, gt))

        def worker_thread() -> None:
            worker = Worker(args.worker_mem_gb)
            try:
                while True:
                    try:
                        pred, gt = work.get_nowait()
                    except queue.Empty:
                        return
                    record = worker.run(pred, gt, args.mode, args.data_root, args.timeout)
                    emit(record)
            finally:
                worker.kill()

        threads = [
            threading.Thread(target=worker_thread, daemon=True)
            for _ in range(max(1, args.jobs))
        ]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

    summary = summarize(records, args.mode, time.monotonic() - start)
    print(
        f"done: {summary['n_ok']}/{summary['n_pairs']} ok, "
        f"{summary['n_error']} errors, {summary['wall_s']}s",
        file=sys.stderr,
    )
    if args.summary_out:
        args.summary_out.parent.mkdir(parents=True, exist_ok=True)
        args.summary_out.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
