"""Run an alignment candidate on frozen development inputs and approved labels."""

from __future__ import annotations

import argparse
import json
import os
import shutil
from pathlib import Path

from .matching_eval import digest, evaluate, read_json, validate_candidate
from .matching_review import json_stdout, run, scoring_identity


def experiment(args: argparse.Namespace) -> dict:
    review = args.review.resolve()
    bundle = read_json(review / "bundle.json")
    labels = read_json(args.labels)
    baseline = evaluate(bundle, labels)
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    shutil.copy2(args.labels, out / "labels.reviewed.json")
    binary = out / "verosim"
    shutil.copy2(args.verosim.resolve(), binary)
    env = dict(os.environ, VEROSIM_VRV_DATA_DIR=str(review / "runtime/verovio/data"))
    candidates, receipts = {}, {}
    for sample_id, sample in bundle["samples"].items():
        if sample["split"] != "dev":
            continue
        frozen = sample["trace"]
        if "evaluation_profile" in frozen:
            raise ValueError("this pilot runner requires an unprofiled review set")
        root = out / "samples" / sample_id
        root.mkdir(parents=True)
        prepared = review / "samples" / sample_id
        options = frozen["options"]
        command = [
            str(binary),
            str(prepared / "prediction.krn"),
            str(prepared / "ground_truth.krn"),
            "--ops",
            "--mode",
            options["mode"],
            "--layout",
            options["layout"],
            "--typed-space-handling",
            options["typed_space_handling"],
            "--note-position",
            options["note_position"],
            "--alignment",
            args.alignment,
        ]
        if options["display_staff"]:
            command.append("--display-staff")
        comparison = run([*command, "--trace-json", str(root / "trace.json")], env=env)
        result = json_stdout(comparison)
        if not result.get("ok"):
            raise ValueError(f"comparison failed: {sample_id}")
        candidate = read_json(root / "trace.json")
        validate_candidate(frozen, candidate)
        plain = json_stdout(run(command, env=env))
        if scoring_identity(result) != scoring_identity(plain):
            raise ValueError(f"tracing changed the candidate's scoring: {sample_id}")
        candidates[sample_id] = candidate
        receipts[sample_id] = {
            "input_and_inventory_identity": True,
            "trace_on_off_scoring_equivalent": True,
            "baseline_distance": frozen["distance"],
            "candidate_distance": result["distance"],
            "runtime_s_single_run_diagnostic": result["runtime_s"],
        }
        (root / "comparison.json").write_text(json.dumps(result, indent=2) + "\n")
        (root / "parser.stderr.log").write_text(comparison.stderr)
    result = evaluate(bundle, labels, candidates)
    before = {w["id"]: w for w in baseline["windows"]}
    changes = [
        {
            "id": w["id"],
            "before": before[w["id"]]["levels"]["measure"],
            "after": w["levels"]["measure"],
        }
        for w in result["windows"]
        if "measure" in w["levels"]
    ]
    report = {
        "dataset_id": bundle["dataset_id"],
        "labels_sha256": digest(args.labels.read_bytes()),
        "candidate_binary_sha256": digest(binary.read_bytes()),
        "alignment": args.alignment,
        "holdout": "not run or scored; draft labels excluded",
        "samples": receipts,
        "windows": changes,
    }
    for name, value in (
        ("evaluation.json", result),
        ("baseline.json", baseline),
        ("receipt.json", report),
    ):
        (out / name).write_text(json.dumps(value, indent=2) + "\n")
    return {
        "approved_windows": result["approved_windows"],
        "measure": result["levels"]["measure"],
        "samples_run": len(candidates),
        "out": str(out),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--review", type=Path, required=True)
    parser.add_argument("--labels", type=Path, required=True)
    parser.add_argument("--verosim", type=Path, required=True)
    parser.add_argument(
        "--alignment", choices=("independent", "shared"), default="shared"
    )
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(experiment(args), indent=2))


if __name__ == "__main__":
    main()
