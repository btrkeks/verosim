# Evaluation-Scope Projection Delivery Plan

Status: **in progress**
Mode/source: `supplied plan; delegated production implementation contract`
Goal/council: `active goal; three agents, two passes`
External plan review: `degraded: correctly preflighted single Fable attempt rejected at account usage limit`
Base/branch: `local main @ 7e46cb82e416b7d63fc9a115405de0caea59f99c; feature/evaluation-scope-projection (plus prerequisite parallel-parser and arpeggio commits)`

Freeze Sections 1–3 after the council. Keep status, evidence, commits, deviations, and blockers mutable.

## 1. Product contract (frozen)

**Outcome:** VeroSim users can select a versioned external evaluation profile that projects fully extracted prediction and ground-truth `SymScore`s onto one explicit, representation-neutral scoring scope before every comparison-sensitive operation, with deterministic audit metadata and unchanged behavior when no profile is selected.

**In scope**

- A strict v1 positive profile contract with a model-owned exhaustive feature taxonomy and `scored`, `ignored`, and implementation-reserved `unsupported` dispositions.
- Pure, symmetric canonical projection after maximal supported extraction and before notation-size calculation, preparation, matching, edit-path selection, edit categorization, and OMR-NED computation.
- `--evaluation-profile PATH` for pair, `--pairs`, `--batch`, mixed-format `--batch-jsonl`, and both visualization outputs.
- Deterministic profile identity and per-side removal audits in profile-aware JSON, batch summary, HTML, and SVG manifest outputs.
- Focused model, extraction, pipeline, CLI/batch, and visualization tests; user documentation; repository-prescribed verification; local semantic commits.
- A concrete handoff for Transcoda to own and version its benchmark profile externally and pass its path consistently.

**Non-goals**

- No Transcoda repository edits or Transcoda-specific profile embedded in VeroSim.
- No MusicXML, MEI, Humdrum/kern, or other source mutation/rewrite.
- No post-hoc subtraction of edit categories and no filtering switches inside comparison algorithms.
- No new canonical extraction for currently absent ornaments, lyrics, fingering, pedal, general text, or page-break semantics.
- No push, merge, deployment, or worktree cleanup.

**Acceptance criteria**

1. A valid profile is loaded exactly once per command, forces extraction of all currently implemented canonical features (`experimental` plus system breaks), and projects both sides symmetrically before all comparison-sensitive work.
2. The v1 schema requires every registered taxonomy feature exactly once; rejects unknown/missing keys, invalid types/dispositions, ignored mandatory carriers, scored unsupported features, and conflicting explicit `--mode`/`--layout` options with deterministic actionable errors.
3. Ignored accidentals, ties, beams, grace slashes, gaps, articulations/expressions, and every supported extra are invariant to mutation; identity/rhythm-bearing note dimensions remain scored-only; scored mutations remain sensitive; projection is idempotent and conserves notation size (`projected = full - removed`).
4. Tie projection clears only its scored bit after extraction, retaining accidental-carry-derived `sounding_alter`, matching keys, offsets, IDs, and locators; newly empty measures are pruned using the existing extraction invariant while parts remain stable.
5. Profile-mode comparisons own projected inputs in address-stable storage for the complete edit-op/visual serialization lifetime; denominators, alignment, edit paths, distances, and visual marks come only from projected scores.
6. Profile-aware outputs contain schema/version/id plus lowercase SHA-256 of exact profile bytes and deterministic registry-ordered per-side removed counts/totals. Each successfully loaded side is projected and audited even if the other side fails; unavailable sides are null. Batch summaries carry the same identity and aggregate removals overall and by group from available sides. No-profile JSON/HTML/SVG fields and metric results remain unchanged.
7. Equivalent MusicXML and kern inputs exhibit equivalent projection behavior, including ignored tie/slur/articulation cases, and the mixed-format JSONL path propagates the same profile contract.
8. Focused tests, full `make test`, deterministic source-blind CLI checks, phase/final autoreview, documentation, and Transcoda handoff are complete with no in-scope uncommitted changes.

## 2. Target design (frozen)

**Responsibilities and flow:** `verosim_model` owns `EvaluationFeature`, capability/disposition metadata, `EvaluationProfile`, per-feature removal counts, and pure `ProjectSymScore`. A small app-layer loader owns strict JSON validation and exact-byte SHA-256, producing one immutable profile shared by run options. With a profile, extraction uses the maximal implemented `MetricSurface`; without one, existing mode/layout extraction is untouched. `CompareLoadedScores` creates heap-stable projected inputs, computes the entire comparison from them, and returns a shared audit model used by CLI and visual serializers. CLI parsing owns option compatibility and command-level profile-load failures. Transcoda owns external profile contents.

**Interfaces and compatibility:** The JSON contract is exactly `{ "schema": "verosim.evaluation-profile", "schema_version": 1, "id": <nonempty stable string>, "features": { <every v1 registry key>: "scored"|"ignored"|"unsupported" } }`, with no unknown or duplicate top-level/feature keys. The following v1 registry is immutable; adding a category or changing an unsupported category to supported requires schema v2. Old v1 profiles never silently inherit later `experimental` features: an extracted extra unknown to v1 fails actionably. `--evaluation-profile PATH` is accepted anywhere shared comparison options are accepted, but explicit `--mode` or `--layout` (including their default values) with it is rejected. Parsing records `mode_was_set`/`layout_was_set` rather than inferring presence from final values. New audit fields are emitted only with a profile.

| Exact v1 feature key | Capability / allowed disposition | Canonical field or kind | Removed-count unit / neutralization |
|---|---|---|---|
| `note.pitch` | mandatory carrier; `scored` only | `SymPitch.step_octave` | one per pitch; never neutralized |
| `note.accidental` | projectable; `scored` or `ignored` | `SymPitch.accid` | one visible accidental; set to `None`, retain `sounding_alter` |
| `note.tie` | projectable; `scored` or `ignored` | `SymPitch.tie` | one true tie bit; set false after accidental carry resolution |
| `note.notehead` | rhythm carrier; `scored` only | `SymNote.note_head`, `note_dur_type` | one per note; never neutralized |
| `note.dot` | rhythm-bearing; `scored` only | `dots`, `note_dur_dots`, derived offsets | existing per-pitch dot units; never neutralized |
| `note.beam` | projectable; `scored` or `ignored` | `beamings` | one per beam entry; clear vector |
| `note.tuplet` | rhythm-bearing; `scored` only | `tuplets` and derived duration/offset | one per tuplet entry; never neutralized |
| `note.tuplet_info` | coupled rhythm notation; `scored` only | `tuplet_info` | one per info entry; never neutralized |
| `note.grace` | identity/rhythm-bearing; `scored` only | `grace_type`, `note_is_grace`, derived timing | one present grace type; never neutralized |
| `note.grace_slash` | projectable; `scored` or `ignored` | `grace_slash` | one true slash bit; set false (grace remains scored) |
| `note.gap` | projectable; `scored` or `ignored` | `gap_dur` | one nonzero gap; set zero |
| `note.articulation` | projectable; `scored` or `ignored` | `articulations` | one per entry; clear vector |
| `note.expression` | projectable; `scored` or `ignored` | `expressions` | one per entry; clear vector |
| `extra.arpeggio` | projectable; `scored` or `ignored` | `ExtraKind::kArpeggio` | `SymExtra::notation_size()`; erase extra |
| `extra.barline` | projectable; `scored` or `ignored` | `ExtraKind::kBarline` | `SymExtra::notation_size()`; erase extra |
| `extra.clef` | projectable; `scored` or `ignored` | `ExtraKind::kClef` | `SymExtra::notation_size()`; erase extra |
| `extra.hairpin.crescendo` | projectable; `scored` or `ignored` | `ExtraKind::kCrescendo` | `SymExtra::notation_size()`; erase extra |
| `extra.hairpin.diminuendo` | projectable; `scored` or `ignored` | `ExtraKind::kDiminuendo` | `SymExtra::notation_size()`; erase extra |
| `extra.dynamic` | projectable; `scored` or `ignored` | `ExtraKind::kDynamic` | `SymExtra::notation_size()`; erase extra |
| `extra.keysig` | projectable; `scored` or `ignored` | `ExtraKind::kKeySig` | `SymExtra::notation_size()`; erase extra |
| `extra.ottava` | projectable; `scored` or `ignored` | `ExtraKind::kOttava` | `SymExtra::notation_size()`; erase extra |
| `extra.repeat` | projectable; `scored` or `ignored` | `ExtraKind::kRepeat` | `SymExtra::notation_size()`; erase extra |
| `extra.slur` | projectable; `scored` or `ignored` | `ExtraKind::kSlur` | `SymExtra::notation_size()`; erase extra |
| `extra.timesig` | projectable; `scored` or `ignored` | `ExtraKind::kTimeSig` | `SymExtra::notation_size()`; erase extra |
| `layout.system_break` | projectable; `scored` or `ignored` | `ExtraKind::kSystemBreak` | `SymExtra::notation_size()`; erase extra |
| `note.ornament` | known absent; `unsupported` only | no canonical field | zero; scoring/ignoring rejected in v1 |
| `note.style` | known absent; `unsupported` only | style fields not materialized | zero; scoring/ignoring rejected in v1 |
| `note.lyric` | known absent; `unsupported` only | no canonical field | zero; scoring/ignoring rejected in v1 |
| `note.fingering` | known absent; `unsupported` only | no canonical field | zero; scoring/ignoring rejected in v1 |
| `extra.text` | known absent; `unsupported` only | no standalone text kind | zero; scoring/ignoring rejected in v1 |
| `extra.pedal` | known absent; `unsupported` only | no canonical kind | zero; scoring/ignoring rejected in v1 |
| `layout.page_break` | known absent; `unsupported` only | no canonical kind | zero; scoring/ignoring rejected in v1 |

**Invariants**

- Full parsing and semantic extraction precede projection; projection never touches source text or parser trees.
- The profile is the only feature-scope authority in profile mode and is immutable after command setup.
- Every supported/known feature has exactly one v1 disposition. The v1 registry is frozen; implementation growth either remains outside v1 and fails closed when encountered or ships under a new schema version.
- Projection changes only ignored comparison-bearing feature values. IDs, locators, offsets, duration/matching fields for scored-only dimensions, and resolved pitch state remain intact; any future ignorable dimension with derived matching fields must specify and neutralize them in a new schema version.
- Ignored extras are erased, and measures made element-empty are erased to match extraction normalization; staff/part order remains unchanged.
- `ScoreComparison` is move-only. Without a profile, edit-op pointers reference caller inputs; with a profile, they reference its non-null heap-owned projected pair and remain valid after moving the result.
- Removed counts use the same symbol units as `notation_size`; their total is the denominator delta.
- No-profile execution and serialized field sets remain unchanged.

**Rejected alternatives**

- Post-hoc edit-category subtraction cannot correct denominators or earlier alignment/edit-path decisions.
- Source-level MusicXML/kern rewriting would duplicate parser semantics, damage derived accidental/tie state, and violate representation neutrality.
- Applying a profile atop active/experimental extraction leaves two scope authorities and silently prevents auditing omitted features.
- Passing masks into interning, matching, or edit-distance code duplicates taxonomy policy and risks inconsistent comparison phases.
- Keeping newly empty projected measures creates zero-cost structural edit ops and visual marks for ignored-only content.
- Canonicalizing profile JSON before hashing hides the exact external artifact used; exact-byte SHA-256 gives an unambiguous reproducibility identity.

## 3. Proof and phases (frozen)

| Behavior/risk | Lowest proof | Test/repro | Diagnostic observation | Source-blind check |
|---|---|---|---|---|
| Exhaustive strict schema and stable digest | loader unit | valid profile plus missing/unknown/duplicate/type/disposition/version cases; known SHA-256 fixture | path-qualified validation error and exact digest | invalid CLI profile exits 2 before score work, stdout/summary absent |
| Projection correctness and resolved semantics | model unit | table-driven note/extra projection; idempotence; size conservation; tie `sounding_alter` retention | per-feature removed count and total | n/a |
| Comparison ordering, lifetime, denominator | app/engine unit | ignored/scored mutations, adversarial multi-note alignment, result move, and op inspection after `CompareLoadedScores` | owned projected scores and audit sizes | pair JSON distance/counts match expected |
| Representation neutrality | extraction/app integration | equivalent synthesized MusicXML/kern with tie/slur/articulation variants | equal feature removals/projected comparison | mixed file-pair CLI emits zero ignored-feature distance |
| Shared CLI/batch propagation | CLI integration | option placement/presence conflicts, pairs, parallel batch, mixed JSONL, summary overall/by-group aggregation | identical profile identity and ordered audits | run executable and parse emitted JSON/JSONL; invalid profile emits only stderr and exit 2 |
| Visual propagation | visual integration | HTML and SVG manifest with ignored and scored mutations | ignored edits yield no marks; audit present | inspect generated artifacts without source knowledge |
| Backward compatibility | characterization/full suite | existing metric/JSON/visual tests plus explicit no-profile field-set checks (pair JSON compared after removing nondeterministic `runtime_s`) | unchanged golden values | existing CLI examples retain ordered fields/values and deterministic artifacts remain byte-compatible |

Allowed phase statuses: `not started`, `in progress`, `complete`, `blocked`.

| ID | Outcome | Depends on | Required gate | Status | Commit |
|---|---|---|---|---|---|
| P1 | Strict v1 taxonomy/profile loader/digest and pure canonical projector | none | focused schema/projection tests, phase autoreview | complete | pending commit |
| P2 | Profile-aware extraction and comparison lifecycle with stable ownership/audit | P1 | pipeline/engine/cross-format/backcompat tests, phase autoreview | in progress | — |
| P3 | CLI, pairs, batch/JSONL, summary, HTML, and SVG propagation | P2 | CLI/visual tests and source-blind commands, phase autoreview | not started | — |
| P4 | User documentation and Transcoda integration handoff | P3 | documented examples/schema checked against executable behavior, phase autoreview if executable docs change | not started | — |
| F1 | Final verification/closeout and latest-main recheck | all phases | `make test`, behavior matrix, final branch autoreview, clean diff | not started | `HEAD when complete` |

## 4. Execution ledger (mutable)

Current phase/status: `P2; in progress`

### P1 — Strict taxonomy, loader, digest, and projector

- Status/commit: `complete; commit pending`
- Repro and diagnostic evidence: added focused tests first for the exact registry, note/extra projection, resolved tie/accidental state, size conservation, idempotence, strict profile validation, duplicate/unknown fields, and exact-byte digest sensitivity.
- Implementation: immutable v1 feature registry and capability contract; strict external JSON loader; exact-byte SHA-256 digest; pure canonical `SymScore` projection with deterministic per-feature removal counts and empty-measure renormalization.
- Verification: `build/tests/verosim_tests '[evaluation-profile]'` — 6 cases, 124 assertions passed; `git diff --check` clean.
- Diff/instrumentation disposition: scoped phase autoreview clean (`overall: patch is correct (0.98)`); no temporary instrumentation retained.
- Deviations: none

### P2 — Profile-aware extraction, comparison ownership, and audit

- Status/commit: `in progress; preceding commit pending`
- Repro and diagnostic evidence: pending tests-first additions
- Implementation: pending
- Verification: pending
- Diff/instrumentation disposition: pending
- Deviations: none

## 5. Planning review decisions (mutable until freeze)

| Lane/pass | Decision or blocker | Evidence | Resolution/rationale |
|---|---|---|---|
| Council 1 | Profile must force maximal supported extraction | mode/layout gates omit controls before projection | Profile becomes sole scope owner; no-profile path unchanged; explicit mode/layout conflicts rejected. |
| Council 1 | Projected inputs must be address-stable | edit ops store raw pointers used after pipeline return | `ScoreComparison` owns heap-stable projected scores in profile mode. |
| Council 1 | Taxonomy must be exhaustive without speculative canonical fields | `SymNote`, `ExtraKind`, and count traits define current support | One registry distinguishes scored-only carriers, projectable support, and named unsupported future classes. |
| Council 1 | Empty-measure handling disagreement | extraction already drops element-empty measures; retained zero-size measures can affect DP/visual ops | Reapply extraction normalization after extra filtering; retain parts and all surviving metadata. |
| Council 1 | Audit must span failures and all visual/batch artifacts | serializers are separate but consume shared pipeline result/report | One audit data model and shared JSON helper; absent entirely without a profile. |
| Council 1 | Concurrent mixed-format main work | local main advanced to `7e46cb8`; rebase conflict was additive test adjacency only | Rebased before edits and retained both mixed-format and parallel regression tests; recheck main before F1. |
| Council 2 | Ignorable rhythm/grace fields could alter matching through derived metadata | `set_distance.cpp` consumes `note_dur_dots`, `note_is_grace`, and duration keys | Freeze identity/rhythm-bearing fields as scored-only in v1; only independently safe note fields are projectable. |
| Council 2 | The public v1 feature set was underspecified | `SymbolCounts`, `SymNote`, and 12 `ExtraKind`s expose different grouping boundaries | Added the complete immutable key/capability/storage/count table to the frozen contract. |
| Council 2 | Failure and summary audit semantics were ambiguous | pair load failures precede comparison; summary has overall/by-group buckets | Project every available side; null unavailable sides; aggregate deterministic removals overall and by group. |
| Council 2 | Output compatibility cannot include dynamic runtime bytes | `runtime_s` uses steady-clock duration | Compare exact ordered fields/values after removing only `runtime_s`; deterministic HTML/SVG remain exact. |
| Fable | Single independent review attempt unavailable | CLI returned `You've reached your Fable 5 limit` | Recorded degraded mode per skill; no substitute model and no delivery blocker. |

## 6. Blockers and deviations (mutable)

| Date/phase | Evidence and attempted alternatives | Contract impact | Required decision |
|---|---|---|---|
| 2026-07-25/planning | Original archived worktree was absent; recreated clean worktree and branch from local repository. | none | none |
| 2026-07-25/planning | Rebase onto updated local main conflicted only where two independent test cases were adjacent; resolved by retaining both verbatim and rebase completed. | none | none |
| 2026-07-25/P1 | Concurrent unstaged `Makefile`/`README.md` build-resource changes appeared with a separate timestamp while P1 was in progress. They are coherent but outside this feature. | none; preserved unmodified and excluded from feature commits/reviews | none |
