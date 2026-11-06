"""Evaluate frozen, independently reviewed correspondences rather than edit costs."""

from __future__ import annotations

import argparse
import base64
import hashlib
import itertools
import json
import re
from collections import Counter
from pathlib import Path


SIDES = ("prediction", "ground_truth")
LEVELS = ("part", "measure", "note")


def digest(value: bytes) -> str:
    return "sha256:" + hashlib.sha256(value).hexdigest()


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def inventory(trace: dict) -> dict[str, dict[str, dict]]:
    result = {}
    for side in SIDES:
        symbols = trace["symbols"][side]
        result[side] = {symbol["key"]: symbol for symbol in symbols}
        if len(result[side]) != len(symbols):
            raise ValueError(f"duplicate {side} symbol key")
    return result


def stable_inventory(trace: dict) -> dict:
    # Synthesized Verovio objects get fresh XML ids on each load. Indexed keys
    # and notation content identify them; XML ids only resolve this run's SVG.
    return {side: {key: {k: v for k, v in symbol.items() if k not in ("source_id", "visual_id")}
        | {"source_address": symbol["source_id"] if re.search(r"-L\d+", symbol["source_id"]) else None}
        for key, symbol in symbols.items()} for side, symbols in inventory(trace).items()}


def trace_input_bytes(trace: dict, side: str) -> bytes:
    value = trace["inputs"][side]
    if value.get("encoding") != "base64":
        raise ValueError("trace input must declare base64 encoding")
    return base64.b64decode(value["data"], validate=True)


def validate_trace(trace: dict) -> None:
    if trace.get("schema_version") not in (1, 2):
        raise ValueError("unsupported matching trace schema")
    symbols = inventory(trace)
    for side in SIDES:
        trace_input_bytes(trace, side)
    seen = {side: Counter() for side in SIDES}
    for decision in trace["decisions"]:
        if not any(endpoint_keys(decision[side]) for side in SIDES):
            raise ValueError("empty alignment decision")
        for side in SIDES:
            keys = endpoint_keys(decision[side])
            if isinstance(decision[side], list) and (
                trace["schema_version"] != 2 or decision["level"] != "measure"
            ):
                raise ValueError("grouped endpoints require schema 2 measure decisions")
            for key in keys:
                if key not in symbols[side] or symbols[side][key]["kind"] != decision["level"]:
                    raise ValueError(f"unknown or wrong-kind {side} endpoint: {key}")
                seen[side][key] += 1
    for side in SIDES:
        if set(seen[side]) != set(symbols[side]) or any(count != 1 for count in seen[side].values()):
            raise ValueError(f"trace does not cover every {side} symbol exactly once")


def endpoint_keys(endpoint: str | list[str] | None) -> list[str]:
    if endpoint is None:
        return []
    if isinstance(endpoint, str):
        return [endpoint]
    if isinstance(endpoint, list) and endpoint and all(isinstance(key, str) for key in endpoint):
        return endpoint
    raise ValueError("invalid alignment endpoint")


def validate_candidate(frozen: dict, candidate: dict) -> None:
    validate_trace(candidate)
    for side in SIDES:
        if digest(trace_input_bytes(frozen, side)) != digest(trace_input_bytes(candidate, side)):
            raise ValueError(f"candidate changed prepared {side} input")
    if stable_inventory(frozen) != stable_inventory(candidate):
        raise ValueError("candidate changed symbol identities/extraction; migrate and re-review labels")
    for option in ("mode", "layout", "typed_space_handling", "display_staff"):
        if frozen["options"].get(option) != candidate["options"].get(option):
            raise ValueError(f"candidate changed evaluation scope: {option}")
    if frozen.get("evaluation_profile") != candidate.get("evaluation_profile"):
        raise ValueError("candidate changed evaluation profile")


def relation_sets(label: dict) -> tuple[set[tuple[str, str]], dict[str, set[str]]]:
    links = set()
    used = {side: set() for side in SIDES}
    for group in label.get("links", []):
        if group.get("relation", "corresponding") not in ("corresponding", "moved"):
            raise ValueError("unknown correspondence relation")
        if not group.get("prediction") or not group.get("ground_truth"):
            raise ValueError("a match group requires endpoints on both sides")
        for side in SIDES:
            keys = group[side]
            if len(keys) != len(set(keys)) or used[side].intersection(keys):
                raise ValueError("an endpoint appears in multiple match groups")
            used[side].update(keys)
        links.update(itertools.product(group["prediction"], group["ground_truth"]))
    unmatched = {side: set(label.get("unmatched_" + side, [])) for side in SIDES}
    for side in SIDES:
        values = label.get("unmatched_" + side, [])
        if len(values) != len(unmatched[side]) or used[side] & unmatched[side]:
            raise ValueError("duplicate or both matched and unmatched endpoint")
    return links, unmatched


def domains(label: dict) -> dict[str, set[str]]:
    links, unmatched = relation_sets(label)
    return {
        "prediction": {a for a, _ in links} | unmatched["prediction"],
        "ground_truth": {b for _, b in links} | unmatched["ground_truth"],
    }


def actual_sets(trace: dict, level: str) -> tuple[set[tuple[str, str]], dict[str, set[str]]]:
    links, unmatched = set(), {side: set() for side in SIDES}
    for decision in trace["decisions"]:
        if decision["level"] != level:
            continue
        a, b = (endpoint_keys(decision[side]) for side in SIDES)
        if a and b:
            links.update(itertools.product(a, b))
        else:
            unmatched["prediction"].update(a)
            unmatched["ground_truth"].update(b)
    return links, unmatched


def resolved_label(label: dict, actual: tuple, scope: dict) -> tuple[dict, dict]:
    """Select one complete acceptable assignment per disjoint ambiguous group."""
    result = {"links": list(label.get("links", []))}
    for side in SIDES:
        result["unmatched_" + side] = list(label.get("unmatched_" + side, []))
    covered = domains(result)
    ignored = {side: set() for side in SIDES}
    for group in label.get("ambiguous", []):
        group_domain = {side: set(group[side]) for side in SIDES}
        for side in SIDES:
            if len(group_domain[side]) != len(group[side]) or covered[side] & group_domain[side] or ignored[side] & group_domain[side]:
                raise ValueError("overlapping ambiguous endpoints")
        choices = group.get("acceptable_assignments", [])
        if choices:
            for choice in choices:
                if domains(choice) != group_domain:
                    raise ValueError("acceptable assignment must cover its entire ambiguous group")
            actual_links, actual_unmatched = actual
            def agreement(choice: dict) -> int:
                links, unmatched = relation_sets(choice)
                return len(links & actual_links) + sum(len(unmatched[s] & actual_unmatched[s]) for s in SIDES)
            selected = max(choices, key=agreement)
            result["links"].extend(selected.get("links", []))
            for side in SIDES:
                result["unmatched_" + side].extend(selected.get("unmatched_" + side, []))
                covered[side].update(group_domain[side])
        else:
            for side in SIDES:
                ignored[side].update(group_domain[side])
    for side in SIDES:
        if covered[side] | ignored[side] != set(scope[side]):
            raise ValueError("approved labels must cover their scope; mark unresolved endpoints ambiguous")
    return result, ignored


def counts_for(label: dict, actual: tuple, allowed: dict | None = None) -> tuple[Counter, set]:
    expected_links, expected_unmatched = relation_sets(label)
    scoped = domains(label)
    if allowed is not None:
        scoped = {s: scoped[s] & allowed[s] for s in SIDES}
        expected_links = {(a, b) for a, b in expected_links if a in scoped["prediction"] and b in scoped["ground_truth"]}
        for side in SIDES:
            expected_unmatched[side] &= scoped[side]
        scoped = {"prediction": {a for a, _ in expected_links} | expected_unmatched["prediction"],
            "ground_truth": {b for _, b in expected_links} | expected_unmatched["ground_truth"]}
    actual_links, actual_unmatched = actual
    actual_links = {(a, b) for a, b in actual_links if a in scoped["prediction"] or b in scoped["ground_truth"]}
    result = Counter(link_tp=len(expected_links & actual_links), link_fp=len(actual_links - expected_links),
        link_fn=len(expected_links - actual_links), evaluated_prediction=len(scoped["prediction"]),
        evaluated_ground_truth=len(scoped["ground_truth"]))
    for side in SIDES:
        predicted = actual_unmatched[side] & scoped[side]
        expected = expected_unmatched[side]
        result["unmatched_tp"] += len(predicted & expected)
        result["unmatched_fp"] += len(predicted - expected)
        result["unmatched_fn"] += len(expected - predicted)
    result["windows"] = int(bool(scoped["prediction"] or scoped["ground_truth"]))
    result["exact_windows"] = int(bool(result["windows"]) and not any(result[key] for key in ("link_fp", "link_fn", "unmatched_fp", "unmatched_fn")))
    return result, expected_links & actual_links


def validate_label_document(bundle: dict, labels: dict) -> dict:
    def assignment_shape(value: dict) -> None:
        if not isinstance(value, dict):
            raise ValueError("acceptable assignment must be an object")
        if not isinstance(value.get("links", []), list):
            raise ValueError("assignment links must be an array")
        for group in value.get("links", []):
            if not isinstance(group, dict) or not all(
                isinstance(group.get(side), list) and all(isinstance(k, str) for k in group[side])
                for side in SIDES
            ):
                raise ValueError("invalid correspondence group")
        for side in SIDES:
            keys = value.get("unmatched_" + side, [])
            if not isinstance(keys, list) or not all(isinstance(k, str) for k in keys):
                raise ValueError("symbol references must be string arrays")
        if value.get("ambiguous"):
            raise ValueError("acceptable assignments cannot contain nested ambiguity")

    if labels.get("schema_version") != 1 or labels.get("dataset_id") != bundle["dataset_id"]:
        raise ValueError("labels belong to a different frozen review set")
    if not isinstance(labels.get("windows"), list):
        raise ValueError("labels require a window array")
    frozen = {w["id"]: w for w in bundle["windows"]}
    result = {}
    for window in labels["windows"]:
        if not isinstance(window, dict) or window.get("id") not in frozen or window["id"] in result:
            raise ValueError("labels must contain each frozen window exactly once")
        original = frozen[window["id"]]
        if window.get("sample_id") != original["sample_id"] or not isinstance(window.get("levels"), dict) or set(window["levels"]) != set(LEVELS):
            raise ValueError("label window has invalid sample or levels")
        for level in LEVELS:
            value = window["levels"][level]
            if not isinstance(value, dict) or value.get("status") not in ("unreviewed", "approved"):
                raise ValueError("unknown review status")
            for field in ("links", "ambiguous", "unmatched_prediction", "unmatched_ground_truth"):
                if not isinstance(value.get(field), list):
                    raise ValueError(f"label level requires {field} array")
            for side in SIDES:
                if not all(isinstance(key, str) for key in value["unmatched_" + side]):
                    raise ValueError("symbol references must be strings")
            for item in value["links"] + value["ambiguous"]:
                if not isinstance(item, dict) or not all(isinstance(item.get(side), list) and all(isinstance(k, str) for k in item[side]) for side in SIDES):
                    raise ValueError("invalid correspondence group")
            assignment_shape({key: value[key] for key in ("links", "unmatched_prediction", "unmatched_ground_truth")})
            for group in value["ambiguous"]:
                choices = group.get("acceptable_assignments", [])
                if not isinstance(choices, list):
                    raise ValueError("acceptable assignments must be an array")
                for choice in choices:
                    assignment_shape(choice)
            if value["status"] == "approved" and not (isinstance(value.get("reviewed_by"), str) and value["reviewed_by"].strip() and isinstance(value.get("reviewed_at"), str) and value["reviewed_at"]):
                raise ValueError("approved labels require reviewer and review time")
            trace = bundle["samples"][original["sample_id"]]["trace"]
            resolved_label(value, actual_sets(trace, level), original["scope"][level])
        result[window["id"]] = window
    if set(result) != set(frozen):
        raise ValueError("labels must contain each frozen window exactly once")
    return result


def validate_hierarchy(samples: dict, approved: dict) -> None:
    for sample_id, levels in approved.items():
        symbols = inventory(samples[sample_id]["trace"])
        for parent_level, child_level in (("part", "measure"), ("measure", "note")):
            allowed, known = set(), {side: set() for side in SIDES}
            choices = []
            for label in levels[parent_level]:
                links, unmatched = relation_sets(label)
                allowed.update(links)
                for side in SIDES:
                    known[side].update(domains(label)[side])
                for ambiguous in label["ambiguous"]:
                    assignments = ambiguous.get("acceptable_assignments", [])
                    if assignments:
                        alternatives = [relation_sets(assignment)[0] for assignment in assignments]
                        choices.append((ambiguous, alternatives))
                        allowed.update(set().union(*alternatives))
                        for side in SIDES:
                            known[side].update(ambiguous[side])
            child_parents = set()
            for label in levels[child_level]:
                groups = list(label["links"])
                for ambiguous in label["ambiguous"]:
                    for assignment in ambiguous.get("acceptable_assignments", []):
                        groups.extend(assignment.get("links", []))
                for group in groups:
                    if group.get("relation") == "moved":
                        continue
                    for a, b in itertools.product(group["prediction"], group["ground_truth"]):
                        pair = (symbols["prediction"][a]["parent"], symbols["ground_truth"][b]["parent"])
                        child_parents.add(pair)
                        if (pair[0] in known["prediction"] or pair[1] in known["ground_truth"]) and pair not in allowed:
                            raise ValueError(f"approved {child_level} link contradicts {parent_level} labels; correct it or explicitly label a moved event")
            for group, alternatives in choices:
                constraints = {pair for pair in child_parents if pair[0] in group["prediction"] or pair[1] in group["ground_truth"]}
                if not any(constraints <= alternative for alternative in alternatives):
                    raise ValueError("child links mix incompatible parent ambiguity assignments")


def metrics(counts: Counter) -> dict:
    result = dict(counts)
    for prefix in ("link", "unmatched"):
        tp, fp, fn = (counts[prefix + suffix] for suffix in ("_tp", "_fp", "_fn"))
        result[prefix + "_precision"] = tp / (tp + fp) if tp + fp else None
        result[prefix + "_recall"] = tp / (tp + fn) if tp + fn else None
        result[prefix + "_f1"] = 2 * tp / (2 * tp + fp + fn) if 2 * tp + fp + fn else None
    return result


def evaluate(bundle: dict, labels: dict, candidates: dict[str, dict] | None = None) -> dict:
    label_windows = validate_label_document(bundle, labels)
    candidates = candidates or {}
    windows = {window["id"]: window for window in bundle["windows"]}
    traces = {}
    for sample_id, sample in bundle["samples"].items():
        trace = candidates.get(sample_id, sample["trace"])
        validate_candidate(sample["trace"], trace)
        traces[sample_id] = trace
    totals = {level: Counter() for level in LEVELS}
    groups: dict[str, dict[str, Counter]] = {}
    conditioned = Counter()
    details = []
    approved = 0
    approved_labels = {sid: {level: [] for level in LEVELS} for sid in bundle["samples"]}
    for window_id, window in windows.items():
        labeling = label_windows[window_id]
        sample_id = window["sample_id"]
        if labeling.get("sample_id") != sample_id:
            raise ValueError("label window sample changed")
        sample = bundle["samples"][sample_id]
        symbols = inventory(traces[sample_id])
        row = {"id": window_id, "sample_id": sample_id, "split": sample["split"], "category": window["category"], "levels": {}}
        correct_measures = None
        any_approved = False
        for level in LEVELS:
            label = labeling["levels"][level]
            if label.get("status") not in ("unreviewed", "approved"):
                raise ValueError("unknown review status")
            if label["status"] != "approved":
                totals[level]["unreviewed_windows"] += 1
                continue
            actual = actual_sets(traces[sample_id], level)
            resolved, ignored = resolved_label(label, actual, window["scope"][level])
            approved_labels[sample_id][level].append(label)
            for side in SIDES:
                for key in window["scope"][level][side]:
                    if key not in symbols[side] or symbols[side][key]["kind"] != level:
                        raise ValueError("label scope references an invalid symbol")
            counts, correct = counts_for(resolved, actual)
            counts["ambiguous_prediction"] += len(ignored["prediction"])
            counts["ambiguous_ground_truth"] += len(ignored["ground_truth"])
            totals[level].update(counts)
            for group in ("split:" + sample["split"], "category:" + window["category"], "work:" + sample["work_id"]):
                groups.setdefault(group, {name: Counter() for name in LEVELS})[level].update(counts)
            row["levels"][level] = metrics(counts)
            any_approved = True
            if level == "measure":
                correct_measures = correct
            if level == "note" and correct_measures is not None:
                allowed = {
                    "prediction": {key for key, symbol in symbols["prediction"].items() if symbol["parent"] in {a for a, _ in correct_measures}},
                    "ground_truth": {key for key, symbol in symbols["ground_truth"].items() if symbol["parent"] in {b for _, b in correct_measures}},
                }
                conditioned.update(counts_for(resolved, actual, allowed)[0])
        approved += any_approved
        details.append(row)
    validate_hierarchy(bundle["samples"], approved_labels)
    return {"schema_version": 1, "dataset_id": bundle["dataset_id"],
        "status": "evaluated" if approved else "awaiting_human_review", "approved_windows": approved,
        "window_count": len(windows), "levels": {k: metrics(v) for k, v in totals.items()},
        "notes_with_correct_measure_alignment": metrics(conditioned),
        "groups": {k: {level: metrics(value) for level, value in levels.items()} for k, levels in groups.items()},
        "windows": details}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("labels", type=Path)
    parser.add_argument("--candidate-dir", type=Path, help="directory of SAMPLE_ID/trace.json from the candidate")
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    bundle = read_json(args.bundle)
    candidates = None
    if args.candidate_dir:
        candidates = {sample_id: read_json(args.candidate_dir / sample_id / "trace.json") for sample_id in bundle["samples"]}
    try:
        result = evaluate(bundle, read_json(args.labels), candidates)
    except (ValueError, KeyError, TypeError) as error:
        parser.error(str(error))
    args.out.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in ("status", "approved_windows", "window_count")}))


if __name__ == "__main__":
    main()
