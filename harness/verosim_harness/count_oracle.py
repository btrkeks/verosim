"""Per-file musicdiff active/experimental symbol-count breakdown.

Parses one score with the pinned music21/converter21 stack, builds
AnnScore(score, mode bitmask) exactly like the oracle does, and emits a JSON
breakdown whose categories mirror the C++ SymbolCounts struct
(include/verosim/model/sym_score.h), so that

    hand count  ==  count_oracle.py  ==  verosim --count-symbols

is a three-way exact comparison. Used to checksum the HAND-10 expected
files before any C++ extraction exists (D11 fixtures-before-code), and as
the per-note offset/repr reference during extraction bring-up
(--per-note).

Category accounting reproduces AnnNote.notation_size (annotation.py:264)
and AnnExtra.notation_size (annotation.py:717) term by term; the sum of
categories is asserted equal to AnnScore.notation_size().
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from . import METRIC_MODES
from .oracle import ensure_converter21

# Mirrors SymbolCounts in include/verosim/model/sym_score.h. Keep in sync.
CATEGORIES = [
    "pitches", "accidentals", "ties", "noteheads", "dots", "beams",
    "tuplets", "tuplet_info", "grace", "grace_slash", "gaps",
    "articulations", "expressions", "style",
    "clef", "keysig", "timesig", "other_extras",
]


def _empty_counts() -> dict[str, int]:
    return {c: 0 for c in CATEGORIES}


def _count_note(note, counts: dict[str, int]) -> int:
    """AnnNote.notation_size, accumulated per category. Returns the size."""
    size = 0
    for pitch in note.pitches:
        counts["pitches"] += 1
        size += 1
        if pitch[1] != "None":
            counts["accidentals"] += 1
            size += 1
        if pitch[2]:
            counts["ties"] += 1
            size += 1
    counts["noteheads"] += 1
    size += 1
    counts["dots"] += note.dots * len(note.pitches)
    size += note.dots * len(note.pitches)
    counts["beams"] += len(note.beamings)
    size += len(note.beamings)
    counts["tuplets"] += len(note.tuplets)
    size += len(note.tuplets)
    counts["tuplet_info"] += len(note.tuplet_info)
    size += len(note.tuplet_info)
    counts["articulations"] += len(note.articulations)
    size += len(note.articulations)
    counts["expressions"] += len(note.expressions)
    size += len(note.expressions)
    if note.graceType:
        counts["grace"] += 1
        size += 1
        if note.graceSlash is True:
            counts["grace_slash"] += 1
            size += 1
    style = 0
    if note.noteshape != "normal":
        style += 1
    if note.noteheadFill is not None:
        style += 1
    if note.noteheadParenthesis:
        style += 1
    if note.stemDirection != "unspecified":
        style += 1
    if note.styledict:
        style += 1
    counts["style"] += style
    size += style
    if note.gap_dur != 0:
        counts["gaps"] += 1
        size += 1
    assert size == note.notation_size(), (size, note.notation_size(), repr(note))
    return size


def _count_extra(extra, counts: dict[str, int]) -> int:
    """AnnExtra.notation_size, accumulated per category. Returns the size."""
    size = 0
    if extra.content is not None:
        size += len(extra.content)
    if extra.symbolic is not None:
        size += 1
    if extra.duration is not None:
        size += 1
    size += len(extra.infodict)
    if extra.styledict:
        counts["style"] += 1
        size += 1
    body = size - (1 if extra.styledict else 0)
    if extra.kind in ("clef", "keysig", "timesig"):
        counts[extra.kind] += body
    else:
        counts["other_extras"] += body
    assert size == extra.notation_size(), (size, extra.notation_size(), repr(extra))
    return size


def count_file(path: Path, mode_name: str, per_note: bool = False) -> dict:
    ensure_converter21()
    import music21 as m21
    from musicdiff.annotation import AnnScore

    mode_value = METRIC_MODES[mode_name]
    score = m21.converter.parse(path, forceSource=True)
    if isinstance(score, m21.stream.Opus):
        score = score.scores[0]
    ann = AnnScore(score, mode_value)

    counts = _empty_counts()
    parts_out = []
    for part in ann.part_list:
        measures_out = []
        for bar in part.bar_list:
            assert not bar.includes_voicing, "active/experimental never set Voicing"
            m_size = 0
            notes_out = []
            for note in bar.annot_notes:
                m_size += _count_note(note, counts)
                if per_note:
                    notes_out.append({
                        "offset": str(note.note_offset),
                        "dur_type": note.note_dur_type,
                        "dur_dots": note.note_dur_dots,
                        "grace": note.note_is_grace,
                        "size": note.notation_size(),
                        "repr": repr(note),
                    })
            extras_out = []
            for extra in bar.extras_list:
                m_size += _count_extra(extra, counts)
                if per_note:
                    extras_out.append({
                        "kind": extra.kind,
                        "offset": str(extra.offset),
                        "symbolic": extra.symbolic,
                        "infodict": extra.infodict,
                        "size": extra.notation_size(),
                    })
            assert m_size == bar.notation_size()
            measure_rec: dict = {"n": bar.measureNumber, "size": m_size}
            if per_note:
                measure_rec["notes"] = notes_out
                measure_rec["extras"] = extras_out
            measures_out.append(measure_rec)
        parts_out.append({
            "part_idx": part.part_idx,
            "n_measures": len(part.bar_list),
            "size": part.notation_size(),
            "measures": measures_out,
        })

    # AnnScore.notation_size also sums staff groups + metadata; both are off
    # in the current modes, but use the authoritative total anyway.
    total = ann.notation_size()
    assert total == sum(counts.values()), (total, counts)
    return {
        "path": str(path),
        "mode": mode_name,
        "total": total,
        "categories": counts,
        "parts": parts_out,
    }


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("file", type=Path)
    ap.add_argument("--mode", default="active", choices=sorted(METRIC_MODES))
    ap.add_argument("--per-note", action="store_true",
                    help="include per-note offsets/reprs and per-extra details")
    args = ap.parse_args(argv)
    result = count_file(args.file, args.mode, per_note=args.per_note)
    json.dump(result, sys.stdout, indent=2)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
