"""Freeze real prediction pairs and build a portable human matching reviewer."""

from __future__ import annotations

import argparse
import base64
import gzip
import json
import os
import re
import shutil
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from .matching_eval import LEVELS, SIDES, digest, evaluate, inventory, read_json, validate_trace


def run(command: list[str], *, text: str | None = None, timeout: int = 90, env: dict | None = None) -> subprocess.CompletedProcess:
    result = subprocess.run(command, input=text, capture_output=True, text=True, timeout=timeout, env=env)
    if result.returncode:
        raise RuntimeError(f"command failed ({result.returncode}): {command}\n{result.stderr[-3000:]}")
    return result


def json_stdout(result: subprocess.CompletedProcess) -> dict:
    for line in reversed(result.stdout.splitlines()):
        try:
            value = json.loads(line)
        except ValueError:
            continue
        if isinstance(value, dict):
            return value
    raise ValueError("command did not return a JSON object")


def scoring_identity(value: dict) -> dict:
    result = {k: v for k, v in value.items() if k != "runtime_s"}
    if "edit_ops" in result:
        operations = []
        for op in result["edit_ops"]:
            normalized = dict(op)
            for side in ("a", "b"):
                endpoint = op.get(side)
                if endpoint is None:
                    continue
                endpoint = dict(endpoint)
                source_id = endpoint.get("id", "")
                if not re.search(r"-L\d+", source_id) and not source_id.isdigit():
                    endpoint["repr"] = endpoint["repr"].replace(source_id, "<render-id>", 1)
                    endpoint["id"] = "<render-id>"
                normalized[side] = endpoint
            operations.append(normalized)
        result["edit_ops"] = operations
    return result


def proposed_note_links(prediction: list[dict], ground_truth: list[dict]) -> list[tuple[str, str]]:
    """Independent soft sequence proposal, explicitly requiring human approval."""
    m, n = len(prediction), len(ground_truth)
    costs = [[0] * (n + 1) for _ in range(m + 1)]
    choices = {}
    for i in range(m, -1, -1):
        for j in range(n, -1, -1):
            if i == m and j == n:
                continue
            candidates = []
            if i < m and j < n:
                a, b = prediction[i], ground_truth[j]
                pair_cost = 0 if a["content"] == b["content"] else 1 if a["pitch"] == b["pitch"] else 6
                if a["grace"] != b["grace"]:
                    pair_cost += 4
                candidates.append((pair_cost + costs[i + 1][j + 1], "pair"))
            if i < m:
                candidates.append((4 + costs[i + 1][j], "delete"))
            if j < n:
                candidates.append((4 + costs[i][j + 1], "insert"))
            costs[i][j], choices[i, j] = min(candidates, key=lambda item: item[0])
    links = []
    i = j = 0
    while i < m or j < n:
        action = choices[i, j]
        if action == "pair":
            links.append((prediction[i]["key"], ground_truth[j]["key"]))
            i += 1
            j += 1
        elif action == "delete":
            i += 1
        else:
            j += 1
    return links


def group(a: str, b: str) -> dict:
    return {"prediction": [a], "ground_truth": [b]}


def draft_window(window: dict, sample: dict, part_links: list, first_for_sample: bool) -> tuple[dict, dict]:
    symbols = inventory(sample["trace"])
    scope = {level: {side: [] for side in SIDES} for level in LEVELS}
    for side in SIDES:
        for key, symbol in symbols[side].items():
            level = symbol["kind"]
            if level == "part" and first_for_sample:
                scope[level][side].append(key)
            elif level in ("measure", "note") and symbol["measure_index"] in window[side]:
                scope[level][side].append(key)
    measure_by_index = {
        side: {(symbol["part_index"], symbol["measure_index"]): key for key, symbol in symbols[side].items() if symbol["kind"] == "measure"}
        for side in SIDES
    }
    part_groups = [group(f"p{a}", f"p{b}") for a, b in part_links] if first_for_sample else []
    measure_groups, note_groups = [], []
    for a, b in part_links:
        for i, j in window["measure_links"]:
            pred_key = measure_by_index["prediction"].get((a, i))
            gt_key = measure_by_index["ground_truth"].get((b, j))
            if pred_key is None or gt_key is None:
                continue
            measure_groups.append(group(pred_key, gt_key))
            pred_notes = [value for value in symbols["prediction"].values() if value["kind"] == "note" and value["parent"] == pred_key]
            gt_notes = [value for value in symbols["ground_truth"].values() if value["kind"] == "note" and value["parent"] == gt_key]
            note_groups.extend(group(x, y) for x, y in proposed_note_links(pred_notes, gt_notes))
    levels = {}
    for level, links in zip(LEVELS, (part_groups, measure_groups, note_groups)):
        levels[level] = {"status": "unreviewed", "links": links, "ambiguous": [], "reviewed_by": "", "reviewed_at": "", "notes": ""}
        for side in SIDES:
            used = {key for link in links for key in link[side]}
            levels[level]["unmatched_" + side] = [key for key in scope[level][side] if key not in used]
    frozen_window = {**window, "sample_id": sample["sample_id"], "scope": scope}
    frozen_window.pop("sample")
    return frozen_window, {"id": window["id"], "sample_id": sample["sample_id"], "levels": levels}


def write_reviewer(bundle: dict, labels: dict, path: Path) -> None:
    payload = json.dumps({"bundle": bundle, "labels": labels}, ensure_ascii=False, separators=(",", ":")).encode()
    encoded = base64.b64encode(gzip.compress(payload, mtime=0)).decode()
    template = Path(__file__).with_name("matching_review.html").read_text(encoding="utf-8")
    path.write_text(template.replace("__PAYLOAD_GZIP_BASE64__", encoded), encoding="utf-8")


def prepare(args: argparse.Namespace) -> None:
    specification = read_json(args.windows)
    out = args.out.resolve()
    if (out / "bundle.json").exists():
        raise ValueError("review set already exists; use a new output directory to preserve reviews")
    out.mkdir(parents=True, exist_ok=True)
    wanted = {value["sample_id"] for value in specification["samples"].values()}
    records = {}
    for line in args.predictions.read_text(encoding="utf-8").splitlines():
        record = json.loads(line)
        if record["sample_id"] in wanted:
            if record["sample_id"] in records:
                raise ValueError("prediction source contains duplicate sample IDs")
            records[record["sample_id"]] = record
    if wanted != set(records):
        raise ValueError("prediction source is missing requested samples")
    work_splits = {}
    for spec in specification["samples"].values():
        if work_splits.setdefault(spec["work_id"], spec["split"]) != spec["split"]:
            raise ValueError("a musical work appears in multiple splits")
    runtime_dir = out / "runtime"
    runtime_dir.mkdir(exist_ok=True)
    binary = runtime_dir / "verosim"
    shutil.copy2(args.verosim.resolve(), binary)
    parser = runtime_dir / "kern_parser"
    shutil.copy2(args.kern_parser.resolve(), parser)
    resources = runtime_dir / "verovio" / "data"
    shutil.copytree(args.verovio_data.resolve(), resources, dirs_exist_ok=True)
    runtime_env = dict(os.environ, VEROSIM_VRV_DATA_DIR=str(resources))
    resource_hashes = {str(path.relative_to(resources)): digest(path.read_bytes())
        for path in sorted(resources.rglob("*")) if path.is_file()}
    provenance = {"verosim_sha256": digest(binary.read_bytes()), "kern_parser_sha256": digest(parser.read_bytes()),
        "verovio_resources_sha256": digest(json.dumps(resource_hashes, sort_keys=True).encode()),
        "source_predictions_sha256": digest(args.predictions.read_bytes()), "window_spec_sha256": digest(args.windows.read_bytes()),
        "created_at": datetime.now(timezone.utc).isoformat(),
        "alignment": args.alignment,
        "conversion": ["serialize-kern", "--preset", "humdrum-kern", "--input-dialect", "transcoda-v2", "--emit", "json"]}
    baseline_binary = None
    if args.baseline_verosim:
        baseline_binary = runtime_dir / "verosim.baseline"
        shutil.copy2(args.baseline_verosim.resolve(), baseline_binary)
        provenance["baseline_verosim_sha256"] = digest(baseline_binary.read_bytes())
    samples = {}
    windows, label_windows = [], []
    for alias, spec in specification["samples"].items():
        sample_id = spec["sample_id"]
        record = records[sample_id]
        root = out / "samples" / sample_id
        root.mkdir(parents=True, exist_ok=True)
        hashes = {}
        for side, field in (("prediction", "prediction"), ("ground_truth", "target")):
            raw = record[field]
            (root / f"{side}.raw.krn").write_text(raw, encoding="utf-8")
            serialized = run([str(parser), *provenance["conversion"]], text=raw)
            source_map = json_stdout(serialized)
            (root / f"{side}.source-map.json").write_text(json.dumps(source_map, indent=2), encoding="utf-8")
            prepared = source_map["kern"]
            (root / f"{side}.krn").write_text(prepared, encoding="utf-8")
            hashes[side] = {"raw": digest(raw.encode()), "prepared": digest(prepared.encode())}
        selected = sorted({index for window in specification["windows"] if window["sample"] == alias for side in SIDES for index in window[side]})
        pair = [str(root / "prediction.krn"), str(root / "ground_truth.krn")]
        options = ["--mode", "active", "--layout", "system-breaks", "--note-position", "visual",
            "--alignment", args.alignment]
        comparison = run([str(binary), *pair, "--ops", *options, "--trace-json", str(root / "trace.json"), "--trace-bars", ",".join(map(str, selected))], env=runtime_env)
        result = json_stdout(comparison)
        if not result.get("ok"):
            raise ValueError(f"failed to compare {sample_id}: {result.get('error')}")
        (root / "comparison.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
        (root / "parser.stderr.log").write_text(comparison.stderr, encoding="utf-8")
        plain = json_stdout(run([str(binary), *pair, "--ops", *options], env=runtime_env))
        if scoring_identity(result) != scoring_identity(plain):
            raise ValueError(f"tracing changed scoring for {sample_id}")
        if baseline_binary:
            # Pre-tracing independent binaries do not have --alignment.
            baseline_options = options[:-2] if args.alignment == "independent" else options
            baseline = json_stdout(run([str(baseline_binary), *pair, "--ops", *baseline_options], env=runtime_env))
            if scoring_identity(result) != scoring_identity(baseline):
                raise ValueError(f"new executable changed baseline scoring for {sample_id}")
        trace = read_json(root / "trace.json")
        validate_trace(trace)
        verification = {
            "trace_on_off_scoring_equivalent": True,
            "baseline_scoring_equivalent": True if baseline_binary else None,
            "complete_unique_symbol_coverage": True,
            "normalization": "runtime and synthesized XML ids only; source addresses, operations, costs and notation retained",
        }
        (root / "verification.json").write_text(json.dumps(verification, indent=2) + "\n", encoding="utf-8")
        rendered = run([str(binary), "--visualize", *pair, "--out-dir", str(root / "render"), "--output-format", "svg", *options], env=runtime_env)
        (root / "render.stderr.log").write_text(rendered.stderr, encoding="utf-8")
        manifest = read_json(root / "render" / "visualization.json")
        svgs = {side: [(root / "render" / page["path"]).read_text(encoding="utf-8") for page in manifest[side]["pages"]] for side in SIDES}
        sample = {**spec, "sample_id": sample_id, "hashes": hashes, "trace": trace, "svg": svgs,
            "parser_stderr": comparison.stderr, "cer": record.get("cer"), "provenance": provenance,
            "verification": verification}
        source = Path(record["source"])
        image = source / "image.webp"
        if image.is_file():
            shutil.copy2(image, root / "scan.webp")
            sample["scan_data"] = "data:image/webp;base64," + base64.b64encode(image.read_bytes()).decode()
        samples[sample_id] = sample
        print(f"prepared {alias}: {len(trace['decisions'])} complete decisions", flush=True)
    seen = set()
    for window in specification["windows"]:
        spec = specification["samples"][window["sample"]]
        sample_id = spec["sample_id"]
        sample = samples[sample_id]
        symbol_inventory = inventory(sample["trace"])
        part_counts = [sum(s["kind"] == "part" for s in symbol_inventory[side].values()) for side in SIDES]
        default_parts = [[i, i] for i in range(min(part_counts))]
        frozen, labels = draft_window(window, sample, spec.get("part_links", default_parts), sample_id not in seen)
        for side in SIDES:
            available = {s["measure_index"] for s in symbol_inventory[side].values() if s["kind"] == "measure"}
            if not set(window[side]) <= available:
                raise ValueError(f"review window has absent source measures: {window['id']} {side}")
        windows.append(frozen)
        label_windows.append(labels)
        seen.add(sample_id)
    identity = {"specification": specification, "provenance": {k: v for k, v in provenance.items() if k != "created_at"},
        "inputs": {key: value["hashes"] for key, value in samples.items()}}
    dataset_id = digest(json.dumps(identity, sort_keys=True).encode())
    bundle = {"schema_version": 1, "dataset_id": dataset_id, "provenance": provenance, "samples": samples, "windows": windows}
    labels = {"schema_version": 1, "dataset_id": dataset_id, "windows": label_windows}
    report = evaluate(bundle, labels)
    for name, value in (("bundle.json", bundle), ("labels.draft.json", labels), ("baseline.json", report)):
        (out / name).write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    write_reviewer(bundle, labels, out / "review.html")
    print(f"Review: {out / 'review.html'}", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--predictions", type=Path, required=True)
    parser.add_argument("--windows", type=Path, required=True)
    parser.add_argument("--verosim", type=Path, required=True)
    parser.add_argument("--baseline-verosim", type=Path)
    parser.add_argument("--alignment", choices=("shared", "independent"), default="shared")
    parser.add_argument("--kern-parser", type=Path, required=True)
    parser.add_argument("--verovio-data", type=Path, default=Path(__file__).resolve().parents[2] / "verovio" / "data")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(args)
    except (ValueError, RuntimeError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
