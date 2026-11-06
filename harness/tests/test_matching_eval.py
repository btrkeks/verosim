from copy import deepcopy

import pytest

from verosim_harness.matching_eval import actual_sets, counts_for, evaluate, validate_candidate, validate_trace
from verosim_harness.matching_review import draft_window, proposed_note_links, scoring_identity


def fixture():
    symbols = [
        {"key": "p0", "kind": "part", "parent": None, "part_index": 0, "measure_index": -1, "source_id": "1", "visual_id": ""},
        {"key": "p0/m0", "kind": "measure", "parent": "p0", "part_index": 0, "measure_index": 0, "source_id": "measure-L4", "visual_id": "measure-L4"},
    ]
    for i, pitch in enumerate(("C4", "D4")):
        symbols.append({"key": f"p0/m0/n{i}", "kind": "note", "parent": "p0/m0", "part_index": 0,
            "measure_index": 0, "source_id": f"note-L{5+i}F1", "visual_id": f"note-L{5+i}F1",
            "pitch": pitch, "content": pitch, "grace": False})
    trace = {"schema_version": 1, "inputs": {side: {"encoding": "base64", "data": "ZnJvemVu"} for side in ("prediction", "ground_truth")},
        "options": {"mode": "active", "layout": "system-breaks", "typed_space_handling": "preserve", "display_staff": False},
        "symbols": {side: deepcopy(symbols) for side in ("prediction", "ground_truth")},
        "decisions": [{"level": s["kind"], "prediction": s["key"], "ground_truth": s["key"]} for s in symbols]}
    sample = {"sample_id": "S", "work_id": "work", "split": "dev", "trace": trace}
    spec = {"id": "window", "sample": "s", "category": "control", "prediction": [0], "ground_truth": [0], "measure_links": [[0, 0]]}
    window, labels = draft_window(spec, sample, [[0, 0]], True)
    bundle = {"schema_version": 1, "dataset_id": "frozen-set", "windows": [window], "samples": {"S": sample}}
    return bundle, {"schema_version": 1, "dataset_id": "frozen-set", "windows": [labels]}


def approve(labels, *levels):
    for level in levels:
        labels["windows"][0]["levels"][level].update(status="approved", reviewed_by="human", reviewed_at="2026-11-05")


def test_drafts_do_not_become_accuracy_or_exact_success():
    bundle, labels = fixture()
    result = evaluate(bundle, labels)
    assert result["status"] == "awaiting_human_review"
    assert result["approved_windows"] == 0
    assert result["levels"]["note"]["link_f1"] is None
    assert not result["levels"]["note"].get("exact_windows")


def test_endpoint_accuracy_detects_wrong_matches_with_identical_counts():
    bundle, labels = fixture()
    approve(labels, "measure", "note")
    correct = evaluate(bundle, labels)
    assert correct["levels"]["note"]["link_f1"] == 1
    candidate = deepcopy(bundle["samples"]["S"]["trace"])
    candidate["decisions"][2]["ground_truth"] = "p0/m0/n1"
    candidate["decisions"][3]["ground_truth"] = "p0/m0/n0"
    wrong = evaluate(bundle, labels, {"S": candidate})
    assert wrong["levels"]["note"]["link_tp"] == 0
    assert wrong["levels"]["note"]["link_fp"] == 2
    assert wrong["levels"]["note"]["link_fn"] == 2
    assert wrong["levels"]["note"]["exact_windows"] == 0


def test_incomplete_duplicate_and_cross_level_labels_are_rejected():
    bundle, labels = fixture()
    approve(labels, "note")
    label = labels["windows"][0]["levels"]["note"]
    label["links"].pop()
    with pytest.raises(ValueError, match="cover their scope"):
        evaluate(bundle, labels)
    label["links"].append(deepcopy(label["links"][0]))
    with pytest.raises(ValueError, match="multiple match groups"):
        evaluate(bundle, labels)


def test_unknown_ambiguity_is_excluded_with_visible_coverage():
    bundle, labels = fixture()
    approve(labels, "note")
    label = labels["windows"][0]["levels"]["note"]
    label["links"] = []
    label["ambiguous"] = [{side: bundle["windows"][0]["scope"]["note"][side] for side in ("prediction", "ground_truth")}]
    result = evaluate(bundle, labels)["levels"]["note"]
    assert result["ambiguous_prediction"] == 2
    assert result["evaluated_prediction"] == 0
    assert result["link_f1"] is None
    assert result["exact_windows"] == 0


def test_whole_acceptable_assignments_preserve_unison_cardinality():
    bundle, labels = fixture()
    approve(labels, "note")
    label = labels["windows"][0]["levels"]["note"]
    original = deepcopy(label["links"])
    crossed = [{"prediction": ["p0/m0/n0"], "ground_truth": ["p0/m0/n1"]}, {"prediction": ["p0/m0/n1"], "ground_truth": ["p0/m0/n0"]}]
    label["links"] = []
    label["ambiguous"] = [{"prediction": ["p0/m0/n0", "p0/m0/n1"], "ground_truth": ["p0/m0/n0", "p0/m0/n1"],
        "acceptable_assignments": [{"links": original}, {"links": crossed}]}]
    candidate = deepcopy(bundle["samples"]["S"]["trace"])
    candidate["decisions"][2]["ground_truth"] = "p0/m0/n1"
    candidate["decisions"][3]["ground_truth"] = "p0/m0/n0"
    assert evaluate(bundle, labels, {"S": candidate})["levels"]["note"]["link_f1"] == 1
    label["ambiguous"][0]["acceptable_assignments"][1]["links"][1]["prediction"] = ["p0/m0/n0"]
    with pytest.raises(ValueError):
        evaluate(bundle, labels, {"S": candidate})


def test_traces_require_complete_unique_symbol_coverage():
    bundle, _ = fixture()
    trace = deepcopy(bundle["samples"]["S"]["trace"])
    validate_trace(trace)
    trace["decisions"].pop()
    with pytest.raises(ValueError, match="exactly once"):
        validate_trace(trace)


def test_schema_two_evaluates_grouped_bars_without_duplicating_endpoints():
    bundle, labels = fixture()
    frozen = bundle["samples"]["S"]["trace"]
    extra = deepcopy(frozen["symbols"]["ground_truth"][1])
    extra.update(key="p0/m1", measure_index=1, source_id="measure-L10", visual_id="measure-L10")
    frozen["symbols"]["ground_truth"].append(extra)
    frozen["decisions"].append({"level": "measure", "prediction": None, "ground_truth": "p0/m1"})
    candidate = deepcopy(frozen)
    candidate["schema_version"] = 2
    candidate["decisions"][1].update(prediction=["p0/m0"], ground_truth=["p0/m0", "p0/m1"])
    candidate["decisions"].pop()
    validate_candidate(frozen, candidate)
    links, unmatched = actual_sets(candidate, "measure")
    assert links == {("p0/m0", "p0/m0"), ("p0/m0", "p0/m1")}
    assert not any(unmatched.values())
    bundle["windows"][0]["scope"]["measure"]["ground_truth"].append("p0/m1")
    labels["windows"][0]["levels"]["measure"]["links"][0]["ground_truth"].append("p0/m1")
    approve(labels, "measure")
    assert evaluate(bundle, labels, {"S": candidate})["levels"]["measure"]["link_f1"] == 1
    duplicate = deepcopy(candidate)
    duplicate["decisions"][1]["ground_truth"].append("p0/m1")
    with pytest.raises(ValueError, match="exactly once"):
        validate_trace(duplicate)
    candidate["schema_version"] = 1
    with pytest.raises(ValueError, match="schema 2"):
        validate_trace(candidate)


@pytest.mark.parametrize("endpoint", [[], [None], 1, {}])
def test_grouped_trace_rejects_invalid_endpoint_shapes(endpoint):
    bundle, _ = fixture()
    trace = deepcopy(bundle["samples"]["S"]["trace"])
    trace["schema_version"] = 2
    trace["decisions"][1]["prediction"] = endpoint
    with pytest.raises(ValueError, match="invalid alignment endpoint"):
        validate_trace(trace)


def test_candidate_inputs_and_extraction_are_frozen_but_generated_ids_are_not():
    bundle, _ = fixture()
    frozen = bundle["samples"]["S"]["trace"]
    candidate = deepcopy(frozen)
    candidate["inputs"]["prediction"]["data"] = "b3RoZXIgaW5wdXQ="
    with pytest.raises(ValueError, match="changed prepared"):
        validate_candidate(frozen, candidate)
    candidate = deepcopy(frozen)
    candidate["symbols"]["prediction"][2]["pitch"] = "E4"
    with pytest.raises(ValueError, match="symbol identities"):
        validate_candidate(frozen, candidate)
    frozen = deepcopy(frozen)
    frozen["symbols"]["prediction"][2]["source_id"] = "random123"
    candidate = deepcopy(frozen)
    candidate["symbols"]["prediction"][2]["source_id"] = "random456"
    candidate["symbols"]["prediction"][2]["visual_id"] = "random789"
    validate_candidate(frozen, candidate)


def test_wrong_set_and_unknown_status_fail_closed():
    bundle, labels = fixture()
    labels["dataset_id"] = "another-set"
    with pytest.raises(ValueError, match="different frozen"):
        evaluate(bundle, labels)
    labels["dataset_id"] = bundle["dataset_id"]
    labels["windows"][0]["levels"]["note"]["status"] = "probably"
    with pytest.raises(ValueError, match="unknown review status"):
        evaluate(bundle, labels)


def test_proposals_can_link_wrong_pitch_without_becoming_gold():
    a = [{"key": "a", "content": "C4", "pitch": "C4", "grace": False}]
    b = [{"key": "b", "content": "D4", "pitch": "D4", "grace": False}]
    assert proposed_note_links(a, b) == [("a", "b")]
    bundle, labels = fixture()
    assert labels["windows"][0]["levels"]["note"]["status"] == "unreviewed"


def test_repeatability_normalizes_only_ephemeral_operation_ids():
    a = {"distance": 2, "runtime_s": 1, "edit_ops": [{"op": "extrasymboledit", "cost": 2,
        "a": {"id": "random123", "repr": "Extra(random123):clef@0:G2", "type": "AnnExtra"}, "b": None}]}
    b = deepcopy(a)
    b["runtime_s"] = 3
    b["edit_ops"][0]["a"] = {"id": "random456", "repr": "Extra(random456):clef@0:G2", "type": "AnnExtra"}
    assert scoring_identity(a) == scoring_identity(b)
    b["edit_ops"][0]["cost"] = 3
    assert scoring_identity(a) != scoring_identity(b)


def test_approved_parent_child_contradictions_require_explicit_moved_relation():
    bundle, labels = fixture()
    approve(labels, "part", "measure")
    part = labels["windows"][0]["levels"]["part"]
    part.update(links=[], unmatched_prediction=["p0"], unmatched_ground_truth=["p0"])
    with pytest.raises(ValueError, match="measure link contradicts part"):
        evaluate(bundle, labels)
    labels["windows"][0]["levels"]["measure"]["links"][0]["relation"] = "moved"
    assert evaluate(bundle, labels)["status"] == "evaluated"


def test_staff_hierarchy_is_checked_across_windows_of_a_sample():
    bundle, labels = fixture()
    approve(labels, "part", "measure")
    second_window = deepcopy(bundle["windows"][0])
    second_window["id"] = "bars-in-other-window"
    second_labels = deepcopy(labels["windows"][0])
    second_labels["id"] = second_window["id"]
    for frozen, label, levels in (
        (bundle["windows"][0], labels["windows"][0], ("measure", "note")),
        (second_window, second_labels, ("part",)),
    ):
        for level in levels:
            frozen["scope"][level] = {side: [] for side in ("prediction", "ground_truth")}
            label["levels"][level].update(status="unreviewed", links=[], ambiguous=[], unmatched_prediction=[], unmatched_ground_truth=[])
    bundle["windows"].append(second_window)
    labels["windows"].append(second_labels)
    labels["windows"][0]["levels"]["part"].update(links=[], unmatched_prediction=["p0"], unmatched_ground_truth=["p0"])
    with pytest.raises(ValueError, match="measure link contradicts part"):
        evaluate(bundle, labels)


def test_conditioned_notes_exclude_orphan_endpoints_after_gold_edge_filtering():
    label = {"links": [{"prediction": ["a0"], "ground_truth": ["b0"]},
                       {"prediction": ["a1"], "ground_truth": ["b1"]}]}
    actual = ({("a0", "b0")}, {"prediction": {"a1"}, "ground_truth": {"b1"}})
    counts, _ = counts_for(label, actual, {"prediction": {"a0", "a1"}, "ground_truth": {"b0"}})
    assert counts["evaluated_prediction"] == counts["evaluated_ground_truth"] == 1
    assert counts["link_tp"] == 1
    assert counts["unmatched_fp"] == 0
    assert counts["exact_windows"] == 1


@pytest.mark.parametrize("choices", [{}, [None], [{"links": "bad"}], [{"links": [{"prediction": "bad", "ground_truth": []}]}]])
def test_malformed_acceptable_assignments_fail_before_evaluation(choices):
    bundle, labels = fixture()
    note = labels["windows"][0]["levels"]["note"]
    note["links"] = []
    note["ambiguous"] = [{**bundle["windows"][0]["scope"]["note"], "acceptable_assignments": choices}]
    with pytest.raises(ValueError):
        evaluate(bundle, labels)
