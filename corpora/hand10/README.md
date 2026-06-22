# HAND-10: hand-counted active symbol inventories

Ten very small scores with manually derived active-mode (DetailLevel 243)
symbol counts: the only fully human-verified
ground truth in the validation suite. Authored before any C++ extraction
existed (fixtures-before-code, D11), with the same discipline as the mutation
corpus: counts were first derived by hand from the active counting rules
(AnnNote.notation_size / AnnExtra.notation_size, see
`docs/symbol_mapping.md`), then cross-checked against the vendored
musicdiff via `python -m verosim_harness.count_oracle <file>`; all ten
matched exactly (2026-06-12).

| File | Exercises |
|---|---|
| mono.krn | single line, beamed pair, dotted half (mutation-corpus base) |
| accid.krn | within-measure accidental carry, natural cancelling a flat (base) |
| chords.krn | chord splitting, dotted-8th/16th beam pair (base) |
| grand.krn | two staves = two parts, measure alignment (base) |
| keysig.krn | keysig-governed sharps: no accidental symbols on notes (base) |
| tuplet.krn | beamed eighth triplet: beams + tuplets + tuplet_info (base) |
| grace.krn | accented (`qq`) + unaccented/slashed (`q`) graces; no keysig token |
| changes.krn | mid-score key+meter change, mid-measure clef, full-measure rest (mRest), hidden rest (`ryy` → space, not counted) |
| voices.krn | two layers (`*^`/`*v`), chords in one layer, flat note order |
| tiny.xml | MusicXML side: chord, beams, triplet, explicit + carried accidentals, slashed grace |

Expected files (`expected/<name>.expected.json`): `total`, per-category
breakdown (categories mirror `SymbolCounts` in
`include/verosim/model/sym_score.h` and `count_oracle.py`), and
per-part/per-measure notation sizes. `tests/test_hand10.cpp` compares
`verosim --count-symbols` output against these exactly; this gate always
runs in CI (committed data, no external corpus needed).

Conventions pinned while authoring (recorded in `docs/symbol_mapping.md`):

- A kern file without any `*k[...]` token produces **no** keysig extra;
  explicit `*k[]` produces a 1-symbol KeySignature(0) (`flats/sharps: none`).
- A MusicXML `<alter>` without an `<accidental>` element is **not** a visible
  accidental (music21 displayStatus stays false); matches counting Verovio
  `@accid` values only (D13).
- Grace mapping: kern `qq` → ('acc', no slash), `q` → ('unacc', slash);
  MusicXML `slash="yes"` → ('unacc', slash); MusicXML grace without a slash
  attribute → music21 defaults to slash=True → ('unacc', slash).
- Hidden kern rests (`4ryy`) become `space` in Verovio and are skipped by
  musicdiff (`style.hideObjectOnPrint`) — they advance time, count nothing.
- A full-measure rest becomes `mRest` (no `@dur`): its duration is the
  governing meter's measure length (dotted-half in 3/4 → head + dot).
