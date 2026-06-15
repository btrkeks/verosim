#pragma once

#include <optional>
#include <string>
#include <vector>

#include "verosim/model/fraction.h"
#include "verosim/model/sym_score.h"

namespace verosim {

// Pure post-processing ports of musicdiff's per-measure note-list rules
// (m21utils.py). No Verovio dependency: extraction lowers the tree into
// RawEvent records (one per carrier event — a chord is ONE event here, the
// split into per-member SymNotes happens afterwards), these functions
// reproduce music21+musicdiff semantics over the flat measure-order list.

struct RawEvent {
    bool is_rest = false;
    Fraction type_num; // musicdiff get_type_num (complex already resolved)
    // Raw music21-equivalent beam-type list (n.beams.getTypes()): derived
    // from Beam containers by DeriveBeamTypes below; empty for unbeamed
    // notes and ALWAYS empty for rests (m21utils.py:38-39).
    std::vector<BeamValue> raw_beams;
    // Per active tuplet (outermost first): music21 raw tuplet types — set on
    // the first/last event of each tuplet, nullopt in the middle.
    std::vector<std::optional<TupletValue>> raw_tuplets;
    // Parallel to raw_tuplets: the tuplet's @num (numberNotesActual) string.
    std::vector<std::string> tuplet_nums;
};

// One member of a Beam container, in document order.
struct BeamMember {
    bool is_rest = false;
    int n_beams = 0; // log2(type_num/4) for notes; 0 for rests
    int breaksec = 0; // @breaksec: beams above this count break after this note
};

// Per-depth start/continue/stop/partial lists for the members of one Beam
// container, mirroring what music21's importers produce for the same
// notation: runs of consecutive notes carrying depth-d beams (rests are
// transparent: they break nothing and get no entries), a run of length one
// at depth d>1 is a "partial" hook, @breaksec ends runs above its value.
std::vector<std::vector<BeamValue>> DeriveBeamTypes(const std::vector<BeamMember> &members);

// Port of M21Utils.get_enhance_beamings (m21utils.py:468-556), Beams detail
// on: flags for unbeamed notes/rests, beamed-rest continuation, and the
// lone-start/stop/continue fixups, replicating the Python's in-place
// aliasing semantics exactly.
std::vector<std::vector<BeamValue>> EnhanceBeamings(const std::vector<RawEvent> &events);

// Port of M21Utils.get_tuplets_type (m21utils.py:637-672): fill in
// missing start/continue values.
std::vector<std::vector<TupletValue>> CorrectTuplets(const std::vector<RawEvent> &events);

// Port of M21Utils.get_tuplets_info (m21utils.py:586-633), without Style:
// str(numberNotesActual) on raw-start notes, "" otherwise.
std::vector<std::vector<std::string>> TupletInfo(const std::vector<RawEvent> &events);

} // namespace verosim
