#pragma once

#include <string_view>
#include <vector>

namespace verosim {

struct SymExtra;
struct SymMeasure;
struct SymNote;
struct SymPart;

// musicdiff edit-operation names reachable on the v1-tier comparison paths
// (comparison.py op tuples, spellings preserved). The pitch ins/del/name/type
// ops are unreachable at v1 (notes pair by step+octave and pitch lists have
// exactly one element) but are ported because _pitches_diff emits them.
enum class OpName {
    kInsPart,
    kDelPart,
    kInsBar,
    kDelBar,
    kNoteIns,
    kNoteDel,
    kInsPitch,
    kDelPitch,
    kPitchNameEdit,
    kPitchTypeEdit,
    kAccidentIns,
    kAccidentDel,
    kAccidentEdit,
    kTieIns,
    kTieDel,
    kHeadEdit,
    kDotIns,
    kDotDel,
    kGraceEdit,
    kGraceSlashEdit,
    kInsBeam,
    kDelBeam,
    kEditBeam,
    kInsTuplet,
    kDelTuplet,
    kEditTuplet,
    kInsArticulation,
    kDelArticulation,
    kEditArticulation,
    kInsExpression,
    kDelExpression,
    kEditExpression,
    kInsSpace,
    kDelSpace,
    kEditSpace,
    kExtraIns,
    kExtraDel,
    kExtraContentEdit,
    kExtraSymbolEdit,
    kExtraInfoEdit,
    kExtraOffsetEdit,
    kExtraDurationEdit,
    kExtraStyleEdit,
};

std::string_view OpNameStr(OpName name); // "insbar", "noteins", ... (musicdiff spelling)

// One side of an op: a pointer into one of the two caller-owned SymScores
// (musicdiff op tuples carry the Ann object or None).
struct OpSide {
    enum class Kind { kNone, kNote, kExtra, kMeasure, kPart };
    Kind kind = Kind::kNone;
    const SymNote *note = nullptr;
    const SymExtra *extra = nullptr;
    const SymMeasure *measure = nullptr;
    const SymPart *part = nullptr;

    static OpSide None() { return {}; }
    static OpSide Note(const SymNote *n) { return { .kind = Kind::kNote, .note = n }; }
    static OpSide Extra(const SymExtra *e) { return { .kind = Kind::kExtra, .extra = e }; }
    static OpSide Measure(const SymMeasure *m) { return { .kind = Kind::kMeasure, .measure = m }; }
    static OpSide Part(const SymPart *p) { return { .kind = Kind::kPart, .part = p }; }
};

struct EditOp {
    OpName name;
    OpSide a; // original / pred side
    OpSide b; // compare_to / gt side
    long cost = 0;
    // Optional 5th tuple element: (i, j) pitch indices for within-note pitch
    // ops (comparison.py:278-404), note_idx_in_chord for the set-distance
    // notedel/noteins (comparison.py:1209,1215), absent otherwise.
    enum class IdsKind { kNone, kPitchPair, kChordIdx };
    IdsKind ids_kind = IdsKind::kNone;
    int ids0 = 0;
    int ids1 = 0;
};

struct DiffResult {
    std::vector<EditOp> ops;
    long cost = 0;
};

} // namespace verosim
