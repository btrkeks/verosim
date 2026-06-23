#pragma once

// Hand-built SymScore fixtures for engine unit tests: the comparison engine
// is exercised without Verovio, so these tests run (and the engine was
// developed) independently of the Layer 2 extraction.

#include <string>
#include <utility>
#include <vector>

#include "verosim/model/sym_score.h"

namespace verosim::test {

struct NoteOpts {
    std::string accid = "None";
    bool tie = false;
    int sounding_alter = 0;
    Fraction head = Fraction(4);
    int dots = 0;
    std::vector<BeamValue> beams = {};
    std::vector<TupletValue> tuplets = {};
    std::vector<std::string> tuplet_info = {};
    std::string grace_type = "";
    bool grace_slash = false;
    std::string dur_type = "quarter";
    int dur_dots = 0;
    bool is_grace = false;
    std::string id = "n";
};

inline SymNote MakeNote(const std::string &step_octave, Fraction offset, NoteOpts o = {})
{
    SymNote note;
    note.vrv_id = o.id;
    note.pitches.push_back(SymPitch{
        .step_octave = step_octave, .accid = o.accid, .tie = o.tie,
        .sounding_alter = o.sounding_alter });
    note.note_head = o.head;
    note.dots = o.dots;
    note.beamings = o.beams;
    note.tuplets = o.tuplets;
    note.tuplet_info = o.tuplet_info;
    if (note.tuplet_info.empty()) note.tuplet_info.resize(note.tuplets.size());
    note.grace_type = o.grace_type;
    note.grace_slash = o.grace_slash;
    note.note_offset = offset;
    note.note_dur_type = o.dur_type;
    note.note_dur_dots = o.dur_dots;
    note.note_is_grace = o.is_grace;
    return note;
}

inline SymExtra MakeClef(const std::string &symbolic, Fraction offset = Fraction(0))
{
    SymExtra extra;
    extra.vrv_id = "clef";
    extra.kind = ExtraKind::kClef;
    extra.symbolic = symbolic;
    extra.offset = offset;
    return extra;
}

inline SymExtra MakeBarline(const std::string &symbolic, Fraction offset = Fraction(0))
{
    SymExtra extra;
    extra.vrv_id = "barline";
    extra.kind = ExtraKind::kBarline;
    extra.symbolic = symbolic;
    extra.offset = offset;
    return extra;
}

inline SymExtra MakeRepeat(
    const std::string &symbolic, const std::string &direction, Fraction offset = Fraction(0))
{
    SymExtra extra;
    extra.vrv_id = "repeat";
    extra.kind = ExtraKind::kRepeat;
    extra.symbolic = symbolic;
    extra.infodict = { { "repeatdirection", direction } };
    extra.offset = offset;
    return extra;
}

inline SymExtra MakeTimeSig(
    const std::string &num, const std::string &den, Fraction offset = Fraction(0))
{
    SymExtra extra;
    extra.vrv_id = "timesig";
    extra.kind = ExtraKind::kTimeSig;
    extra.infodict = { { "numerator", num }, { "denominator", den } };
    extra.offset = offset;
    return extra;
}

inline SymExtra MakeKeySig(const std::string &sharps, Fraction offset = Fraction(0))
{
    SymExtra extra;
    extra.vrv_id = "keysig";
    extra.kind = ExtraKind::kKeySig;
    extra.infodict = { { "sharps", sharps } };
    extra.offset = offset;
    return extra;
}

inline SymExtra MakeSystemBreak(Fraction offset = Fraction(0))
{
    SymExtra extra;
    extra.vrv_id = "systembreak";
    extra.kind = ExtraKind::kSystemBreak;
    extra.symbolic = "systembreak";
    extra.offset = offset;
    return extra;
}

inline SymMeasure MakeMeasure(std::vector<SymNote> notes, std::vector<SymExtra> extras = {})
{
    SymMeasure measure;
    measure.vrv_id = "m";
    measure.notes = std::move(notes);
    measure.extras = std::move(extras);
    return measure;
}

inline SymScore MakeScore(std::vector<std::vector<SymMeasure>> parts)
{
    SymScore score;
    for (std::size_t p = 0; p < parts.size(); ++p) {
        SymPart part;
        part.staff_n = std::to_string(p + 1);
        part.part_idx = static_cast<int>(p);
        part.bar_list = std::move(parts[p]);
        score.parts.push_back(std::move(part));
    }
    return score;
}

} // namespace verosim::test
