# Matching traces and human correspondence evaluation

Edit distance alone cannot tell whether VeroSim matched the intended bars or
notes. This workflow freezes real prediction/reference pairs, proposes
correspondences for human review, and evaluates approved correspondences by
endpoint identity. Draft proposals never count as ground truth.

## Inspect a comparison

```sh
build/verosim prediction.krn reference.krn --ops \
  --mode active --layout system-breaks --note-position visual \
  --trace-json trace.json --trace-bars 14,15,20,21
```

Tracing is available for a single file pair. The normal comparison JSON stays
on stdout; the separate trace file contains schema version 2, exact input
bytes, extraction options, the full symbol inventory, and the chosen matching
decisions. `--trace-bars` uses zero-based source measure indices and restricts
detailed alternatives only. The chosen mapping always covers every extracted
staff, bar, note/chord member, and extra exactly once, including zero-cost
matches and descendants of unmatched bars or staffs. With no bar filter,
alternatives are collected throughout the score.

Each input has `path`, `encoding: "base64"`, and `data`. Decode `data` to
recover the exact bytes, including compressed MusicXML and UTF-16 files.
Indexed inventory keys such as `p0/m14/n2` identify objects within this frozen
extraction. `source_id`, `visual_id`, and `chord_member` connect them to source
notation and rendered SVG. Verovio can synthesize different XML IDs between
loads; those IDs are rendering references, not persistent label identities.

Each decision names its algorithm and reason, both endpoints (one can be
null), local cost, and optional suffix total. Measure split/merge decisions
use arrays of original inventory keys on both sides; ordinary decisions keep
scalar keys. Each grouped endpoint is still covered exactly once. The evaluator
accepts both schema 1 and schema 2 traces, including frozen schema 1 reviews.
Detailed dynamic-programming
alternatives show deletion, insertion, and pairing costs at the chosen state;
note candidates also explain rejection by pitch, onset, grace, or order.
`preferred` marks the action selected by the actual tie order. These are
diagnostics of the current algorithm, not confidence estimates or an exhaustive
search over every possible alignment. Greedy extras report chosen links;
equality shortcuts expand matches by extracted order without running note DP.
Costs at different hierarchy levels overlap and must not be summed.

The engine accepts an optional borrowed `AlignmentTrace*` in `CompareOptions`.
Its symbol references have the same lifetime as edit-operation references.
`ScoreComparison` owns the trace and any projected score copies needed to keep
them valid. Cost-only computation does not collect traces.

## Freeze a review set

```sh
PYTHONPATH=harness python -m verosim_harness.matching_review \
  --predictions /path/to/predictions.rank0.jsonl \
  --windows corpora/matching_review/windows.json \
  --verosim build/verosim \
  --baseline-verosim /path/to/previous/verosim \
  --alignment independent \
  --kern-parser /path/to/kern_parser \
  --out /path/to/new-review-set
```

Use the tracing-enabled executable for `--verosim`. The optional baseline is
an older executable to check scoring equivalence. The generator converts the
Transcoda dialect using the recorded parser command, freezes both raw and
prepared inputs, copies executables and Verovio resources, and records hashes.
`--verovio-data` overrides the repository resource directory. It compares
tracing on/off and, if supplied, the baseline executable. Only runtime and
synthesized XML IDs are normalized; costs, operations, source addresses, and
notation content must agree. Per-sample `verification.json` records these
checks and complete symbol coverage.

The output includes a self-contained `review.html`, `bundle.json`,
`labels.draft.json`, and `baseline.json`, plus source maps, score SVGs, original
scans when available, parser warnings, and traces under `samples/`. Existing
bundles cannot be overwritten. All pages of a work stay in the same dev or
holdout split. The supplied pilot has 20 windows across seven pages and five
works, including healthy controls. It is a diagnostic set, not a prevalence
estimate of matching errors across the full dataset.

Open `review.html` in a current browser. Start with staffs and bars; review notes
separately. Bar numbers are zero-based within the page. Click objects or use
the lists, then link them, mark them unmatched, or mark their identity uncertain.
Shift-click and multi-select permit grouped correspondences. The original scan
and current matcher decisions are available for inspection. Current edit
highlights are hidden initially to reduce anchoring on existing decisions.
This toggle removes edit colors only; notation remains visible. Scoped bars
carry their page index and staff number. Selection uses a blue frame, proposed
partners use green frames, and a text summary names the selected pair.

Approve each level separately. Editing a staff invalidates bar and note
approvals throughout that sample; editing bars invalidates note approvals.
Browser storage preserves progress where available. Export the JSON to retain
and evaluate it; a file export is the durable review artifact. Imports validate
before replacing active labels, and invalid saved state falls back to drafts.

## Label policy and format

The proposed policy is event identity: a wrong pitch or duration can still be
the same intended event. Actual additions and omissions are unmatched. Repeated
passages preserve occurrence order when context establishes it. Unresolvable
unisons or repeated occurrences should remain ambiguous rather than receive an
arbitrary unique link. Human approval establishes the policy for this set.

Each window has `part`, `measure`, and `note` levels, independently marked
`unreviewed` or `approved`. Approval requires `reviewed_by` and `reviewed_at`.
Each level partitions its complete frozen scope into:

- `links`: groups with arrays of `prediction` and `ground_truth` keys. A
  split/merge can have multiple keys on either side.
- `unmatched_prediction` and `unmatched_ground_truth`: explicit omissions or
  additions.
- `ambiguous`: disjoint endpoint groups. Without acceptable assignments they
  are excluded from accuracy, with excluded coverage reported.

An ambiguous group can carry `acceptable_assignments`: complete alternative
partitions of that group's endpoints into links and unmatched objects. Each
alternative must preserve the full assignment, including cardinality. The
evaluator chooses the alternative with most correct endpoint links plus
correct unmatched endpoints; ties follow label order.

Links default to `relation: "corresponding"`. Approved child links must agree
with approved parent mappings, including mappings approved in other windows of
the same sample. Mark an intentional cross-bar/staff move explicitly with
`relation: "moved"` (the review page has a checkbox). Parent ambiguity must
admit a complete assignment consistent with the child links.

## Evaluate and improve

Shared matching is the default:

```sh
build/verosim prediction.krn reference.krn --alignment shared \
  --mode active --layout system-breaks --ops --trace-json candidate.json
```

Explicit `--alignment independent` retains the previous matching behavior.
Shared alignment first matches the ordered staff sequences using each possible
staff pair's unrestricted bar-alignment cost, allowing missing leading/middle
staffs. It then aligns a single sequence of source-measure frames across all
matched staffs. Frame costs sum the existing notation edit costs on those
staffs. It also considers one predicted bar against two reference bars and
the reverse. These candidates require consecutive source indices, known
positive performed durations, and exact agreement of the combined duration
on every matched staff. This keeps a genuinely missing full bar from being
absorbed into an ordinary neighboring bar just to reduce edit cost.

Span comparison rebases note and extra offsets into a common timeline,
orders events by onset (preserving extraction order for simultaneous events),
and uses the existing note/extra edit costs. It charges one additional unit
per staff for the missing or extra boundary. Evaluation profiles that ignore
barlines also ignore this boundary charge. `barlineins` and `barlinedel` use
the original divided-side measure as the boundary anchor and highlight its
right barline. Temporary span coordinates never escape into operations,
inventories, or trace references. The trace explains chosen spans with
`shared_bar_span_dp` and `missing_predicted_boundary` / `extra_predicted_boundary`.

Equal-cost choices prefer pairing, deletion, insertion, split, then merge.
Repeated content at the same source index no longer creates a hard boundary
between independently aligned staffs. Note pairing, edit weights, and
metric symbols remain unchanged. Performed duration is metadata computed from
the longest staff-layer cursor, including hidden timing carriers and short
pickups; it does not alter the existing meter-based control-span handling.

This is a two-stage method, not a joint global optimization over arbitrary
staff permutations and bar alignments. It preserves staff order. Only adjacent
1:2 and 2:1 spans are supported: multiple consecutive missing boundaries,
displaced boundaries requiring 2:2 alignment, unknown/unequal durations, and
spans with extracted empty bars absent on a matched staff remain limitations.
It computes all bar-pair costs for each possible
staff pair, so time/memory grow with the product of the staff counts and bar
counts; initial validation covers pages rather than long complete scores.

In shared traces, staff alternatives report the first-stage staff-sequence
objective. Bar decision local costs belong to that staff's bar, while bar
alternative local/suffix totals aggregate the shared frames across matched
staffs. Costs from those two stages must not be summed.

To reproduce a candidate pass on all development pages (without running
holdout pages or treating draft labels as gold):

```sh
PYTHONPATH=harness python -m verosim_harness.matching_experiment \
  --review /path/to/review-set --labels /path/to/reviewed-labels.json \
  --verosim build/verosim --alignment shared --out /path/to/new-experiment
```

The runner pins the executable and labels, validates input/extraction identity
and complete trace coverage, checks tracing on/off equivalence, and writes
evaluation and per-window receipts. Reruns require a new output directory.

```sh
PYTHONPATH=harness python -m verosim_harness.matching_eval \
  /path/to/review-set/bundle.json /path/to/reviewed-labels.json \
  --out /path/to/baseline-reviewed.json

PYTHONPATH=harness python -m verosim_harness.matching_eval \
  /path/to/review-set/bundle.json /path/to/reviewed-labels.json \
  --candidate-dir /path/to/candidate-traces \
  --out /path/to/candidate-result.json
```

Candidate traces live at `SAMPLE_ID/trace.json`. Run the candidate executable on
the frozen prepared pair with the same extraction options and evaluation
profile. Candidate input bytes and stable extracted inventories must agree
with the frozen bundle. A change to extraction requires a label migration and
human re-review. The note-position algorithm may change.

Reports include link and unmatched precision/recall/F1, exact windows,
evaluated/ambiguous coverage, per-work/category/split results, and note accuracy
conditioned on correctly matched parent bars. A fully unreviewed set reports
`awaiting_human_review` with null accuracy. Windows are the evaluation units;
overlapping scopes are not deduplicated into a corpus-wide endpoint metric.

Gold and schema 2 candidate traces both represent split/merge groups. Group
links are evaluated as the Cartesian product of their original endpoints,
using the same correspondence semantics as human labels. The Mozart K.331
page-13 regression inputs live in `tests/fixtures/matching/`; their known
missing boundary is a diagnostic fixture, separate from human-approved labels.

Use approved dev labels to compare targeted changes, keeping controls and
runtime/cost changes visible. Reserve holdout works for evaluating chosen
changes rather than tuning every candidate against them. Initial hypotheses
from the pilot are staff assignment when instruments are omitted, a shared bar
alignment across staves, and repeated-content anchors/tie choices. The tracing
and reviewer themselves do not change matching or improve measured accuracy.

To check a generated reviewer's actual SVG visibility and selection behavior,
run the browser regression check in a fresh browser context:

```sh
PLAYWRIGHT_MODULE=/path/to/node_modules/@playwright/test \
  node harness/tests/check_matching_reviewer.cjs /path/to/review-set/review.html
```

The check covers every proposed bar link in the pilot and verifies that edit
color toggles preserve visible notation. It does not change human browser
storage or labels.
