"""Mutation corpus generator for exact ground-truth fixtures.

    python -m verosim_harness.gen_mutations --write    # regenerate cases/ + manifest.json
    python -m verosim_harness.gen_mutations --verify   # regenerate in memory, compare (CI)

Each case applies one controlled textual mutation (one or two single-token
replacements at stated lines — two only where one *logical* symbol spans two
tokens, e.g. a slur's open+close) to a committed base kern file, with the
analytically expected OMR-ED cost derived by hand from the paper's costing
rules as implemented by vendored musicdiff (D12: vendored code is the spec).

Orientation: mutcheck runs run_pair(pred=mutated, gt=base), and musicdiff op
names are original(pred) -> compare_to(gt); e.g. a symbol the mutation ADDED
shows up as a *del* op (delete from pred to reach gt).

Key costing rules used in the derivations (musicdiff comparison.py,
annotation.py, m21utils.py; converter21 representation facts probed on the
committed base files 2026-06-12):
- Notes pair within a measure by step+octave AND offset AND graceness
  (comparison.py:1140); accidental changes therefore cost 1-2 as pitch edits,
  while pitch-letter/octave/offset changes cost notation_size del + ins.
- notation_size(note) = 1 pitch + 1 visible accidental + 1 tie + 1 head
  + dots + beams/flags + tuplet marks + articulations (annotation.py:264).
- Accidentals count only when VISIBLE (note2tuple displayStatus,
  m21utils.py:299): key signature governance and within-measure carry decide
  visibility — the D13 effective-pitch semantics the extractor must reproduce.
- converter21 represents an unbeamed flag as beaming ['partial'], so flags
  cost 1 even at detail levels that include Beams.
- Signatures are AnnExtras: clef = 1 symbol (edit cost 2), timesig = infodict
  per component (change = del+add = 2), keysig = infodict per accidental.
- Measure ins/del costs the measure's notation_size (insbar/delbar).
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import DETAIL_LEVELS
from .versions import REPO_ROOT

MUTATIONS_DIR = REPO_ROOT / "corpora" / "mutations"

# fmt: off
CASES: list[dict] = [
    # ---------------- mono.krn: pitch / duration / structure basics ----------------
    dict(id="mono_pitch_letter", base="mono.krn", tier="tierA",
         edits=[dict(line=6, find="4c", replace="4d")],
         expected_cost=4, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Pitch-letter change: set matching pairs notes by step+octave+offset, so a "
                   "changed letter is unmatched on both sides: del(2) + ins(2) quarter notes."),
    dict(id="mono_octave_up", base="mono.krn", tier="tierA",
         edits=[dict(line=6, find="4c", replace="4cc")],
         expected_cost=4, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Octave change: C5 vs C4 differ in pitches[0][0]; del(2) + ins(2)."),
    dict(id="mono_accidental_sharp", base="mono.krn", tier="tierA",
         edits=[dict(line=6, find="4c", replace="4c#")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="Visible sharp added: note still pairs (step+octave+offset equal), "
                   "accidental edit costs 1 (accidentdel: pred has it, gt doesn't)."),
    dict(id="mono_accidental_flat", base="mono.krn", tier="tierA",
         edits=[dict(line=7, find="4d", replace="4d-")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="Same as mono_accidental_sharp with a flat."),
    dict(id="mono_double_sharp", base="mono.krn", tier="tierA",
         edits=[dict(line=6, find="4c", replace="4c##")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="Double sharp is still one visible accidental symbol: cost 1."),
    dict(id="mono_dot_drop", base="mono.krn", tier="tierA",
         edits=[dict(line=13, find="2.a", replace="2a")],
         expected_cost=1, expected_ops={"dotins": 1},
         rationale="Dot dropped from the last note of the measure (no downstream offset "
                   "shift; measure underfull): pairs via duration fallback, dot diff = 1."),
    dict(id="mono_head_change", base="mono.krn", tier="tierA",
         edits=[dict(line=13, find="2.a", replace="4.a")],
         expected_cost=2, expected_ops={"headedit": 1},
         rationale="Dotted half -> dotted quarter: note_head 2 vs 4, headedit = 2 (del+add)."),
    dict(id="mono_note_delete_end", base="mono.krn", tier="tierA",
         edits=[dict(line=18, find="4c", replace=".")],
         expected_cost=2, expected_ops={"noteins": 1},
         rationale="Last note of last measure replaced by null token: gt note unmatched, "
                   "noteins = notation_size = 2."),
    dict(id="mono_rest_for_note", base="mono.krn", tier="tierA",
         edits=[dict(line=18, find="4c", replace="4r")],
         expected_cost=4, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Rest vs note never pair at measure level (pitches[0][0] 'R' vs 'C4'): "
                   "del rest(2) + ins note(2). (pitchtypeedit only fires for paired pitches.)"),
    dict(id="mono_flag_eighth", base="mono.krn", tier="tierA",
         edits=[dict(line=18, find="4c", replace="8c")],
         expected_cost=1, expected_ops={"delbeam": 1},
         rationale="Quarter -> unbeamed eighth at measure end: note_head is 4 for both "
                   "(types >= quarter cap at 4); the difference is the flag, which "
                   "converter21 encodes as beaming ['partial'] -> one beam del."),
    dict(id="mono_clef", base="mono.krn", tier="tierA",
         edits=[dict(line=2, find="*clefG2", replace="*clefF4")],
         expected_cost=2, expected_ops={"extrasymboledit": 1},
         rationale="Clef is one symbolic symbol; change = del+add = 2. Kern pitches are "
                   "absolute, so no note changes."),
    dict(id="mono_timesig_num", base="mono.krn", tier="tierA",
         edits=[dict(line=4, find="*M4/4", replace="*M3/4")],
         expected_cost=2, expected_ops={"extrainfoedit": 1},
         rationale="Time signature numerator component changed = del+add = 2. Explicit kern "
                   "barlines keep measures intact (overfull tolerated)."),
    dict(id="mono_timesig_denom", base="mono.krn", tier="tierA",
         edits=[dict(line=4, find="*M4/4", replace="*M4/2")],
         expected_cost=2, expected_ops={"extrainfoedit": 1},
         rationale="Denominator component changed = del+add = 2."),
    dict(id="mono_keysig_add", base="mono.krn", tier="tierA",
         edits=[dict(line=3, find="*k[]", replace="*k[f#]")],
         expected_cost=4, expected_ops={"extrainfoedit": 1, "accidentdel": 2},
         rationale="D13 family. Keysig infodict {flats/sharps:none} vs {sharp0:F} = 2; AND "
                   "the two written-f naturals (m1, m3) now display naturals under the "
                   "sharp key: 2 x accidentdel."),
    dict(id="mono_barline_drop", base="mono.krn", tier="tierA",
         edits=[dict(line=11, find="=2", replace=".")],
         expected_cost=10, expected_ops={"insbar": 1, "notedel": 2},
         rationale="Barline removed merges m1+m2. Myers leaves m3 common; cheapest edit is "
                   "editbar(merged, m1) = del g@4(2)+a@5(3) from pred, plus insbar(m2) = 5. "
                   "Barlines themselves are not counted (Barlines bit excluded)."),
    dict(id="mono_grace_end", base="mono.krn", tier="tierA",
         edits=[dict(line=18, find="4c", replace="8qc")],
         expected_cost=7, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Grace for quarter at measure end: graceness blocks pairing; grace eighth "
                   "w/ slash = pitch1+head1+flag1+grace1+slash1 = 5 del, + 2 ins."),
    # tier-scoping pins: same mutation, different tier
    dict(id="mono_artic_staccato", base="mono.krn", tier="tierAB",
         edits=[dict(line=12, find="4g", replace="4g'")],
         expected_cost=1, expected_ops={"delarticulation": 1},
         rationale="Staccato added: paired note, articulation Levenshtein = 1."),
    dict(id="mono_artic_staccato_tierA", base="mono.krn", tier="tierA",
         edits=[dict(line=12, find="4g", replace="4g'")],
         expected_cost=0, expected_ops={},
         rationale="Tier-scoping pin: Articulations excluded at tierA -> cost 0."),
    dict(id="mono_tie_hanging", base="mono.krn", tier="tierAB",
         edits=[dict(line=12, find="4g", replace="[4g")],
         expected_cost=1, expected_ops={"tiedel": 1},
         rationale="Hanging tie start survives converter21 (probed): pitch tie flag True "
                   "vs False on the paired note = 1."),
    dict(id="mono_tie_hanging_tierA", base="mono.krn", tier="tierA",
         edits=[dict(line=12, find="4g", replace="[4g")],
         expected_cost=0, expected_ops={},
         rationale="Tier-scoping pin: Ties excluded at tierA -> cost 0."),
    dict(id="mono_slur", base="mono.krn", tier="tierAB",
         edits=[dict(line=8, find="8eL", replace="(8eL"),
                dict(line=9, find="8fJ", replace="8fJ)")],
         expected_cost=1, expected_ops=None,
         rationale="Slur (one logical symbol, two tokens): AnnExtra kind=slur, "
                   "notation_size = 1 (duration only; no content/symbolic). Op name "
                   "unpinned (extras set distance)."),
    dict(id="mono_slur_tierA", base="mono.krn", tier="tierA",
         edits=[dict(line=8, find="8eL", replace="(8eL"),
                dict(line=9, find="8fJ", replace="8fJ)")],
         expected_cost=0, expected_ops={},
         rationale="Tier-scoping pin: Slurs excluded at tierA -> cost 0."),

    # ---------------- keysig.krn (G major): D13 effective-pitch family ----------------
    dict(id="keysig_f_natural", base="keysig.krn", tier="tierA",
         edits=[dict(line=10, find="4f#", replace="4f")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="D13 family. F-natural under a one-sharp key displays a natural sign: "
                   "same step+octave pairs, visible accidental diff = 1 (NOT a pitch edit, "
                   "even though the sounding pitch changed by a semitone)."),
    dict(id="keysig_f_explicit_n", base="keysig.krn", tier="tierA",
         edits=[dict(line=10, find="4f#", replace="4fn")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="Same as keysig_f_natural with kern's explicit 'n' marker: the displayed "
                   "result (a natural sign) is identical."),
    dict(id="keysig_remove", base="keysig.krn", tier="tierA",
         edits=[dict(line=4, find="*k[f#]", replace="*k[]")],
         expected_cost=4, expected_ops={"extrainfoedit": 1, "accidentdel": 2},
         rationale="D13 family. Keysig infodict diff = 2; both written f# now display "
                   "sharps (carry resets per measure): 2 x accidentdel."),
    dict(id="keysig_add_csharp", base="keysig.krn", tier="tierA",
         edits=[dict(line=4, find="*k[f#]", replace="*k[f#c#]")],
         expected_cost=1, expected_ops={"extrainfoedit": 1},
         rationale="One accidental added to keysig infodict (+1); no written c in the file, "
                   "so no display changes."),
    dict(id="keysig_to_flats", base="keysig.krn", tier="tierA",
         edits=[dict(line=4, find="*k[f#]", replace="*k[b-e-a-]")],
         expected_cost=9, expected_ops={"extrainfoedit": 1, "accidentdel": 5},
         rationale="Keysig infodict: 3 flats not in gt (+3), sharp0 not in pred (+1) = 4. "
                   "Display: naturals appear on written b (m1), a (m1), e (m2); sharps "
                   "appear on both f# = 5 x accidentdel. Total 9."),
    dict(id="keysig_gsharp", base="keysig.krn", tier="tierA",
         edits=[dict(line=12, find="4g", replace="4g#")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="Sharp not in the key: displayed, cost 1."),
    dict(id="keysig_clef_bass", base="keysig.krn", tier="tierA",
         edits=[dict(line=3, find="*clefG2", replace="*clefF4")],
         expected_cost=2, expected_ops={"extrasymboledit": 1},
         rationale="Clef symbolic edit = 2; independent of the key signature."),

    # ---------------- accid.krn: within-measure accidental carry (D13) ----------------
    dict(id="accid_carry_break", base="accid.krn", tier="tierA",
         edits=[dict(line=10, find="8f#", replace="8f")],
         expected_cost=1, expected_ops={"accidentdel": 1},
         rationale="D13 family. Second f# was carried (invisible); f-natural after f# "
                   "displays a natural: visible-accidental diff = 1."),
    dict(id="accid_carrier_change", base="accid.krn", tier="tierA",
         edits=[dict(line=9, find="8f#", replace="8f")],
         expected_cost=2, expected_ops={"accidentins": 1, "accidentdel": 1},
         rationale="D13 family. Mutating the FIRST f# changes the display of the second "
                   "(unchanged!) token: pred note1 loses its sharp (accidentins to restore "
                   "gt), pred note2 now displays sharp (accidentdel). One token, cost 2."),
    dict(id="accid_natural_to_carried_flat", base="accid.krn", tier="tierA",
         edits=[dict(line=16, find="4Bn", replace="4B-")],
         expected_cost=1, expected_ops={"accidentins": 1},
         rationale="D13 family. Second B-flat is carried from the first (invisible); gt's "
                   "natural sign is one visible accidental = 1."),
    dict(id="accid_flat_to_sharp", base="accid.krn", tier="tierA",
         edits=[dict(line=15, find="4B-", replace="4B#")],
         expected_cost=2, expected_ops={"accidentedit": 1},
         rationale="Different visible accidental on a paired note: accidentedit = 2 "
                   "(del+add). The following Bn still displays its natural either way."),
    dict(id="accid_flat_drop", base="accid.krn", tier="tierA",
         edits=[dict(line=15, find="4B-", replace="4B")],
         expected_cost=1, expected_ops={"accidentins": 1},
         rationale="Pred B3 plain vs gt B-flat = 1. Cross-check pinned: converter21 keeps "
                   "an explicit kern 'n' natural DISPLAYED even when redundant (no "
                   "preceding flat), so the following Bn matches on both sides."),
    dict(id="accid_tie_f_sharps", base="accid.krn", tier="tierAB",
         edits=[dict(line=9, find="8f#", replace="[8f#"),
                dict(line=10, find="8f#", replace="8f#]")],
         expected_cost=1, expected_ops={"tiedel": 1},
         rationale="Tie between the two f#: tie flag counts on the start note only "
                   "(note2tuple: type start/continue); carry display unchanged."),
    dict(id="accid_artic_accent", base="accid.krn", tier="tierAB",
         edits=[dict(line=11, find="4g", replace="4g^")],
         expected_cost=1, expected_ops={"delarticulation": 1},
         rationale="Kern '^' = accent articulation: paired note, +1."),

    # ---------------- chords.krn: chords, dotted values, beams ----------------
    dict(id="chord_member_delete", base="chords.krn", tier="tierA",
         edits=[dict(line=13, find="2c 2e", replace="2c")],
         expected_cost=2, expected_ops={"noteins": 1},
         rationale="Chord members are separate AnnNotes sharing an offset: one missing "
                   "member = noteins(2)."),
    dict(id="chord_member_pitch", base="chords.krn", tier="tierA",
         edits=[dict(line=13, find="2c 2e", replace="2c 2f")],
         expected_cost=4, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Changed chord member = unmatched both sides: del(2)+ins(2)."),
    dict(id="chord_member_add", base="chords.krn", tier="tierA",
         edits=[dict(line=13, find="2c 2e", replace="2c 2e 2g")],
         expected_cost=2, expected_ops={"notedel": 1},
         rationale="Extra chord member in pred = notedel(2)."),
    dict(id="chords_dot_shift", base="chords.krn", tier="tierA",
         edits=[dict(line=9, find="8.eL", replace="8eL")],
         expected_cost=13, expected_ops={"dotins": 1, "notedel": 2, "noteins": 2},
         rationale="Dot dropped mid-measure: e pairs via duration fallback (dotins 1) but "
                   "EVERY following note shifts by 0.25 and unpairs: 16th f (sz 4) and "
                   "quarter g (sz 2), del+ins each = 12. Offset-sensitivity pin: a dot "
                   "error costs 1 only at measure end, 13 here."),
    dict(id="chords_halves_to_dotted_quarters", base="chords.krn", tier="tierA",
         edits=[dict(line=14, find="2d 2f", replace="4.d 4.f")],
         expected_cost=6, expected_ops={"headedit": 2, "dotdel": 2},
         rationale="Last chord of the measure: both members pair via fallback; "
                   "per member headedit(2) + dotdel(1)."),
    dict(id="chords_artic_staccato", base="chords.krn", tier="tierAB",
         edits=[dict(line=11, find="4g", replace="4g'")],
         expected_cost=1, expected_ops={"delarticulation": 1},
         rationale="Staccato on a plain note in the chords file: +1."),

    # ---------------- grand.krn: two staves ----------------
    dict(id="grand_bass_pitch", base="grand.krn", tier="tierA",
         edits=[dict(line=7, find="2C", replace="2D")],
         expected_cost=4, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Pitch change in the bass spine only: parts diff independently; "
                   "del(2)+ins(2) in the bass part."),
    dict(id="grand_bass_delete", base="grand.krn", tier="tierA",
         edits=[dict(line=9, find="2G", replace=".")],
         expected_cost=3, expected_ops={"noteins": 1},
         rationale="Last bass note of m1 nulled: noteins(2). Cross-check pinned: +1 'bad "
                   "kern syntax' - converter21 (acceptSyntaxErrors=True, pred side only) "
                   "flags the resulting bass-spine timing gap as a fixed syntax error, and "
                   "annotated_scores_diff adds the fix count to the distance "
                   "(comparison.py:1665). Single-spine trailing nulls do not trigger this.",
         verovio_divergence="Rhythm-broken kern (bass spine short of the treble): humlib "
                   "repairs with straddle <space> filler that shifts note offsets, so the "
                   "C++ side unpairs differently than converter21; the +1 syntax-fix cost "
                   "is oracle-only regardless (D4 deferred). Excluded from the C++ exact "
                   "gate per D6 (Tier-A validation record)."),
    dict(id="grand_whole_to_half", base="grand.krn", tier="tierA",
         edits=[dict(line=12, find="1g", replace="2g")],
         expected_cost=2, expected_ops={"headedit": 1},
         rationale="Whole -> half in treble m2: note_head 1 vs 2, headedit = 2.",
         verovio_divergence="Rhythm-broken kern (treble m2 now shorter than bass): humlib "
                   "inserts straddle <space> filler before both m2 notes, shifting their "
                   "offsets so neither pairs; converter21 keeps the note at offset 0. "
                   "Excluded from the C++ exact gate per D6 (Tier-A validation record)."),
    dict(id="grand_bass_octave", base="grand.krn", tier="tierA",
         edits=[dict(line=12, find="1CC", replace="1C")],
         expected_cost=4, expected_ops={"insbar": 1, "delbar": 1},
         rationale="C2 -> C3 in bass m2: octave change unpairs, cost 4. The bar holds only "
                   "this note, so replacing the whole bar ties del+ins of the note at "
                   "cost 4 and musicdiff's min() tie-break reports bar-level ops."),
    dict(id="grand_treble_clef", base="grand.krn", tier="tierA",
         edits=[dict(line=3, find="*clefG2", replace="*clefC3")],
         expected_cost=2, expected_ops={"extrasymboledit": 1},
         rationale="Treble clef G2 -> C3: extrasymboledit = 2 in the treble part."),

    # ---------------- tuplet.krn: tuplets ----------------
    dict(id="tuplet_pitch", base="tuplet.krn", tier="tierA",
         edits=[dict(line=8, find="12d", replace="12e")],
         expected_cost=10, expected_ops={"notedel": 1, "noteins": 1},
         rationale="Middle triplet note changed: each triplet note carries pitch1+head1+"
                   "beam1+tuplet1+tuplet_info1 = 5; del(5)+ins(5)."),
    dict(id="tuplet_dot_add_mid", base="tuplet.krn", tier="tierA",
         edits=[dict(line=10, find="4f", replace="4.f")],
         expected_cost=5, expected_ops={"dotdel": 1, "notedel": 1, "noteins": 1},
         rationale="Dot added mid-measure after the triplet: f pairs (dotdel 1), following "
                   "g shifts 2 -> 2.5 and unpairs: del(2)+ins(2)."),
    dict(id="tuplet_del_last_note", base="tuplet.krn", tier="tierA",
         edits=[dict(line=11, find="2g", replace=".")],
         expected_cost=2, expected_ops={"noteins": 1},
         rationale="Last note nulled: noteins(2); the triplet is untouched."),
]
# fmt: on


def apply_case(base_text: str, case: dict) -> str:
    lines = base_text.splitlines(keepends=True)
    for edit in case["edits"]:
        idx = edit["line"] - 1
        line = lines[idx]
        if line.count(edit["find"]) != 1:
            raise ValueError(
                f"{case['id']}: line {edit['line']} of {case['base']} contains "
                f"{line.count(edit['find'])} occurrences of {edit['find']!r} (need exactly 1): {line!r}"
            )
        lines[idx] = line.replace(edit["find"], edit["replace"], 1)
    return "".join(lines)


def build() -> tuple[dict, dict[str, str]]:
    """Returns (manifest, {case_id: mutated_text})."""
    ids = [c["id"] for c in CASES]
    if len(ids) != len(set(ids)):
        raise ValueError("duplicate case ids")
    mutated: dict[str, str] = {}
    cases_out = []
    for case in CASES:
        base_path = MUTATIONS_DIR / "base" / case["base"]
        mutated[case["id"]] = apply_case(base_path.read_text(encoding="utf-8"), case)
        case_out = {
            "id": case["id"],
            "base": f"base/{case['base']}",
            "mutated": f"cases/{case['id']}.krn",
            "tier": case["tier"],
            "expected_cost": case["expected_cost"],
            "expected_ops": case["expected_ops"],
            "edits": case["edits"],
            "rationale": case["rationale"],
        }
        # Cases whose mutation breaks kern spine-rhythm consistency parse
        # differently under Verovio humlib than under converter21 (D6); the
        # expectations above hold for the oracle, and the C++ mutation gate
        # (tests/test_mutation_gate.cpp) skips cases carrying this field.
        if "verovio_divergence" in case:
            case_out["verovio_divergence"] = case["verovio_divergence"]
        cases_out.append(case_out)
    manifest = {
        "schema": 1,
        "generated_by": "harness/verosim_harness/gen_mutations.py",
        "orientation": "pred=mutated, gt=base; op names are pred->gt",
        "tier_details": {name: DETAIL_LEVELS[name] for name in sorted({c["tier"] for c in CASES})},
        "n_cases": len(cases_out),
        "cases": cases_out,
    }
    return manifest, mutated


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    mode = ap.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--verify", action="store_true")
    args = ap.parse_args(argv)

    manifest, mutated = build()
    manifest_path = MUTATIONS_DIR / "manifest.json"
    cases_dir = MUTATIONS_DIR / "cases"
    manifest_text = json.dumps(manifest, indent=2) + "\n"

    if args.write:
        cases_dir.mkdir(parents=True, exist_ok=True)
        for case_id, text in mutated.items():
            (cases_dir / f"{case_id}.krn").write_text(text, encoding="utf-8")
        manifest_path.write_text(manifest_text, encoding="utf-8")
        print(f"wrote {len(mutated)} cases + {manifest_path}", file=sys.stderr)
        return 0

    # --verify: byte-compare regenerated content against committed files
    problems = []
    for case_id, text in mutated.items():
        path = cases_dir / f"{case_id}.krn"
        if not path.is_file():
            problems.append(f"missing {path}")
        elif path.read_text(encoding="utf-8") != text:
            problems.append(f"stale {path}")
    extra = {p.name for p in cases_dir.glob("*.krn")} - {f"{i}.krn" for i in mutated}
    problems += [f"orphan cases/{name}" for name in sorted(extra)]
    if not manifest_path.is_file() or manifest_path.read_text(encoding="utf-8") != manifest_text:
        problems.append(f"stale {manifest_path}")
    if problems:
        print("gen_mutations --verify FAILED:", *problems, sep="\n  ", file=sys.stderr)
        return 1
    print(f"gen_mutations --verify ok ({len(mutated)} cases)", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
