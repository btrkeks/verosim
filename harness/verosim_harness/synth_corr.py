"""Same-format correlation audit, isolating the comparison engine.

The DEV-200 gate correlates C++ vs oracle on xml<->krn pairs, where each
stack's score is dominated by its own cross-importer gap (D5/D8): music21's
xml and kern importers disagree in ways Verovio's don't, and vice versa, so
rank agreement saturates well below 1.0 for reasons that are not engine
fidelity. This audit removes the confound: both stacks compare the SAME two
kern files — an original and a rhythm-safe randomly mutated copy (pitch-letter
swaps only, k per file varied to spread ranks), so any rank disagreement is
attributable to the comparison pipeline, not to importer disagreement about
what the piece contains.

Usage:
  python -m verosim_harness.synth_corr --bin build/verosim \\
      [--data-root <root>] [--corpus corpora/dev200.tsv] [--detail tierAB] [--n 100] \\
      [--out oracle/synth_corr_tierAB.jsonl] [--report-append oracle/reports/tier_ab_correlation_audit.md]
"""

from __future__ import annotations

import argparse
import json
import random
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from . import DEFAULT_DATA_ROOT, DETAIL_LEVELS
from .gates import threshold_status
from .lists import read_list
from .stats import format_rho, spearman_or_none
from .versions import REPO_ROOT

SEED = "verosim-synthcorr-validation"
LETTERS = "abcdefg"

# A run of one repeated kern pitch letter (the repetition count is the octave,
# so preserving it preserves rhythm AND octave register shape).
PITCH_RUN = re.compile(r"([a-gA-G])\1*")


def mutate_kern(text: str, rng: random.Random) -> tuple[str, int]:
    """Apply k rhythm-safe pitch/articulation edits; returns (mutated, k applied)."""
    lines = text.splitlines(keepends=True)
    # candidate (line, cell) positions: data lines only, cells with a pitch run.
    # Articulation edits append/remove kern staccato marks and preserve rhythm.
    candidates = []
    for i, line in enumerate(lines):
        stripped = line.rstrip("\n")
        if not stripped or stripped[0] in "*!=":
            continue
        for j, cell in enumerate(stripped.split("\t")):
            if cell != "." and PITCH_RUN.search(cell):
                candidates.append((i, j))
    if not candidates:
        return text, 0

    k = min(rng.randint(1, 12), len(candidates))
    for i, j in rng.sample(candidates, k):
        cells = lines[i].rstrip("\n").split("\t")
        eol = "\n" if lines[i].endswith("\n") else ""
        cell = cells[j]
        if rng.random() < 0.25:
            cells[j] = cell.replace("'", "", 1) if "'" in cell else cell + "'"
        else:
            runs = list(PITCH_RUN.finditer(cell))
            run = rng.choice(runs)
            old = run.group(1)
            new = rng.choice([c for c in LETTERS if c != old.lower()])
            if old.isupper():
                new = new.upper()
            cells[j] = cell[: run.start()] + new * len(run.group(0)) + cell[run.end():]
        lines[i] = "\t".join(cells) + eol
    return "".join(lines), k


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--bin", type=Path, required=True)
    ap.add_argument("--data-root", default=DEFAULT_DATA_ROOT, required=not DEFAULT_DATA_ROOT)
    ap.add_argument("--corpus", type=Path, default=REPO_ROOT / "corpora" / "dev200.tsv")
    ap.add_argument("--detail", default="tierAB", choices=sorted(DETAIL_LEVELS))
    ap.add_argument("--n", type=int, default=100)
    ap.add_argument("--jobs", type=int, default=8)
    ap.add_argument("--out", type=Path)
    ap.add_argument("--report-append", type=Path)
    ap.add_argument("--workdir", type=Path, help="keep generated files here (default: temp)")
    ap.add_argument(
        "--oracle-timeout",
        type=float,
        default=180.0,
        help="per-pair oracle timeout; successful records are content-cached, so only "
        "the handful of pathological pairs re-run on a repeat invocation",
    )
    ap.add_argument(
        "--explained",
        type=Path,
        help="TSV of piece_id<TAB>reason (see corr_audit) — these sources are "
        "rhythm-broken, so the oracle's converter21 parse degenerates even "
        "same-format; Spearman is also reported without them",
    )
    ap.add_argument(
        "--exclude-oracle-degenerate-threshold",
        type=float,
        help="for same-format rhythm-safe mutations, exclude oracle records at or "
        "above this OMR-NED as parser-degenerate before applying --threshold",
    )
    ap.add_argument(
        "--threshold",
        type=float,
        help="exit nonzero if the engine-isolating Spearman (healthy subset when "
        "--explained is given) falls below this",
    )
    args = ap.parse_args(argv)

    explained_ids: set[str] = set()
    if args.explained:
        for line in args.explained.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line and not line.startswith("#"):
                explained_ids.add(line.split("\t")[0])

    # sample kern files from the corpus gt column
    gt_files = []
    for line in read_list(args.corpus):
        gt_files.append(line.split("\t")[1])
    files = random.Random(SEED).sample(gt_files, min(args.n, len(gt_files)))

    workdir = args.workdir or Path(tempfile.mkdtemp(prefix="verosim-synthcorr-"))
    workdir.mkdir(parents=True, exist_ok=True)
    pairs_path = workdir / "pairs.tsv"
    rng = random.Random(SEED + "-edits")
    with open(pairs_path, "w") as pairs:
        for rel in files:
            name = Path(rel).stem
            text = (Path(args.data_root) / rel).read_text(encoding="utf-8", errors="replace")
            mutated, k = mutate_kern(text, rng)
            if k == 0:
                continue
            (workdir / f"{name}.orig.krn").write_text(text, encoding="utf-8")
            (workdir / f"{name}.mut.krn").write_text(mutated, encoding="utf-8")
            pairs.write(f"{name}.mut.krn\t{name}.orig.krn\n")

    # oracle side: reuse run_oracle's isolated-worker driver on the same pairs
    oracle_out = workdir / "oracle.jsonl"
    subprocess.run(
        [
            sys.executable,
            "-m",
            "verosim_harness.run_oracle",
            "--corpus",
            str(pairs_path),
            "--detail",
            args.detail,
            "--jobs",
            str(args.jobs),
            "--data-root",
            str(workdir),
            "--out",
            str(oracle_out),
            "--timeout",
            str(args.oracle_timeout),
        ],
        check=True,
        cwd=REPO_ROOT,
    )

    # C++ side
    cpp_cmd = [str(args.bin), "--batch", str(pairs_path), "--base-dir", str(workdir), "--jobs", str(args.jobs)]
    if args.detail != "tierAB":
        cpp_cmd += ["--detail", args.detail]
    proc = subprocess.run(
        cpp_cmd,
        capture_output=True,
        text=True,
        check=True,
    )
    cpp = {}
    for line in proc.stdout.splitlines():
        if line.strip():
            r = json.loads(line)
            cpp[Path(r["pair"]["pred"]).name] = r

    rows = []
    skipped = []
    for line in oracle_out.read_text().splitlines():
        if not line.strip():
            continue
        o = json.loads(line)
        key = Path(o["pair"]["pred"]).name
        c = cpp.get(key)
        if o.get("error") or o.get("omr_ned") is None or c is None or not c["ok"]:
            skipped.append(key)
            continue
        rows.append(
            {
                "pair": key,
                "oracle_omr_ned": o["omr_ned"],
                "cpp_omr_ned": c["omr_ned"],
                "oracle_distance": o["distance"],
                "cpp_distance": c["distance"],
                "exact_distance_match": o["distance"] == c["distance"],
            }
        )

    rho = spearman_or_none(rows)
    exact = sum(r["exact_distance_match"] for r in rows)
    summary = [
        f"same-format pairs scored by both stacks: {len(rows)} (skipped: {len(skipped)})",
        f"Spearman (same-format, engine-isolating): {format_rho(rho)}",
        f"exact distance matches: {exact}/{len(rows)}",
    ]
    gate_rows = rows
    gate_rho = rho
    if explained_ids:
        healthy = [r for r in rows if r["pair"].split(".")[0] not in explained_ids]
        rho_h = spearman_or_none(healthy)
        summary.append(
            f"healthy sources only ({len(healthy)} pairs, explained rhythm-broken "
            f"sources excluded): Spearman {format_rho(rho_h)}, exact "
            f"{sum(r['exact_distance_match'] for r in healthy)}/{len(healthy)}"
        )
        gate_rows = healthy
        gate_rho = rho_h
    if args.exclude_oracle_degenerate_threshold is not None:
        threshold = args.exclude_oracle_degenerate_threshold
        filtered = [r for r in gate_rows if r["oracle_omr_ned"] < threshold]
        excluded = len(gate_rows) - len(filtered)
        rho_f = spearman_or_none(filtered)
        summary.append(
            f"excluding {excluded} oracle-degenerate same-format records "
            f"(oracle OMR-NED >= {threshold:g}): Spearman {format_rho(rho_f)}, exact "
            f"{sum(r['exact_distance_match'] for r in filtered)}/{len(filtered)}"
        )
        gate_rows = filtered
        gate_rho = rho_f
    if args.threshold is not None:
        summary.append(
            f"gate (engine-isolating Spearman >= {args.threshold}): "
            f"{threshold_status(gate_rho, args.threshold)}"
        )
    summary.append(f"workdir: {workdir}")
    print("synth_corr:", *summary, sep="\n  ")

    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        with open(args.out, "w") as f:
            for row in rows:
                f.write(json.dumps(row) + "\n")

    if args.report_append:
        corpus_label = args.corpus.stem
        with open(args.report_append, "a") as f:
            f.write(
                f"\n## Same-format (engine-isolating) correlation: {corpus_label}\n\n"
                "Both stacks compare identical kern file pairs (original vs "
                f"rhythm-safe random {args.detail} pitch/articulation mutations; "
                "`verosim_harness/synth_corr.py`), removing the cross-importer "
                "confound of the xml<->krn gate:\n\n"
            )
            f.writelines(f"- {s}\n" for s in summary)

    if args.threshold is not None and (gate_rho is None or gate_rho < args.threshold):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
