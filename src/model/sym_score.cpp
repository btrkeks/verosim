#include "verosim/model/sym_score.h"

#include <array>
#include <numeric>
#include <sstream>

namespace verosim {

namespace {

enum class SymbolCountBucket {
    kBarline,
    kClef,
    kKeySig,
    kTimeSig,
    kOtherExtras,
};

struct ExtraKindTraits {
    ExtraKind kind;
    std::string_view name;
    std::string_view edit_header;
    bool has_metric_offset;
    bool has_metric_duration;
    SymbolCountBucket count_bucket;
    bool rendered_as_svg_symbol;
};

constexpr std::array<ExtraKindTraits, 11> kExtraKindTraits = { {
    { ExtraKind::kBarline, "barline", "wrong barline OMR-ED", false, false,
        SymbolCountBucket::kBarline, true },
    { ExtraKind::kClef, "clef", "wrong clef OMR-ED", true, true,
        SymbolCountBucket::kClef, true },
    { ExtraKind::kCrescendo, "crescendo", "wrong crescendo OMR-ED", true, true,
        SymbolCountBucket::kOtherExtras, true },
    { ExtraKind::kDiminuendo, "diminuendo", "wrong diminuendo OMR-ED", true, true,
        SymbolCountBucket::kOtherExtras, true },
    { ExtraKind::kDynamic, "dynamic", "wrong dynamic OMR-ED", true, true,
        SymbolCountBucket::kOtherExtras, true },
    { ExtraKind::kKeySig, "keysig", "wrong keysig OMR-ED", true, true,
        SymbolCountBucket::kKeySig, true },
    { ExtraKind::kOttava, "ottava", "wrong ottava OMR-ED", true, true,
        SymbolCountBucket::kOtherExtras, true },
    { ExtraKind::kRepeat, "repeat", "wrong barline OMR-ED", false, false,
        SymbolCountBucket::kBarline, true },
    { ExtraKind::kSlur, "slur", "wrong slur OMR-ED", true, true,
        SymbolCountBucket::kOtherExtras, true },
    { ExtraKind::kSystemBreak, "systembreak", "wrong system break OMR-ED", true, false,
        SymbolCountBucket::kOtherExtras, false },
    { ExtraKind::kTimeSig, "timesig", "wrong timesig OMR-ED", true, true,
        SymbolCountBucket::kTimeSig, true },
} };

const ExtraKindTraits *FindExtraKindTraits(ExtraKind kind)
{
    for (const ExtraKindTraits &traits : kExtraKindTraits) {
        if (traits.kind == kind) return &traits;
    }
    return nullptr;
}

// AnnNote.__str__ encodes head as str(int) or str(Fraction); our Fraction
// prints "2" / "1/2" identically.
void AppendBeamCode(std::string &out, BeamValue b)
{
    switch (b) {
        case BeamValue::kStart: out += "sr"; break;
        case BeamValue::kContinue: out += "co"; break;
        case BeamValue::kStop: out += "sp"; break;
        case BeamValue::kPartial: out += "pa"; break;
    }
}

void AppendTupletCode(std::string &out, TupletValue t)
{
    switch (t) {
        case TupletValue::kStart: out += "sr"; break;
        case TupletValue::kContinue: out += "co"; break;
        case TupletValue::kStop: out += "sp"; break;
        case TupletValue::kStartStop: out += "ss"; break;
    }
}

} // namespace

std::string_view BeamValueName(BeamValue value)
{
    switch (value) {
        case BeamValue::kStart: return "start";
        case BeamValue::kContinue: return "continue";
        case BeamValue::kStop: return "stop";
        case BeamValue::kPartial: return "partial";
    }
    return "?";
}

std::string_view TupletValueName(TupletValue value)
{
    switch (value) {
        case TupletValue::kStart: return "start";
        case TupletValue::kContinue: return "continue";
        case TupletValue::kStop: return "stop";
        case TupletValue::kStartStop: return "startStop";
    }
    return "?";
}

std::string_view ExtraKindName(ExtraKind kind)
{
    if (const ExtraKindTraits *traits = FindExtraKindTraits(kind)) return traits->name;
    return "?";
}

std::size_t ExtraKindSortRank(ExtraKind kind)
{
    for (std::size_t i = 0; i < kExtraKindTraits.size(); ++i) {
        if (kExtraKindTraits[i].kind == kind) return i;
    }
    return kExtraKindTraits.size();
}

std::string_view ExtraKindEditHeader(ExtraKind kind)
{
    if (const ExtraKindTraits *traits = FindExtraKindTraits(kind)) return traits->edit_header;
    return "wrong direction OMR-ED";
}

bool ExtraKindHasMetricOffset(ExtraKind kind)
{
    if (const ExtraKindTraits *traits = FindExtraKindTraits(kind)) {
        return traits->has_metric_offset;
    }
    return true;
}

bool ExtraKindHasMetricDuration(ExtraKind kind)
{
    if (const ExtraKindTraits *traits = FindExtraKindTraits(kind)) {
        return traits->has_metric_duration;
    }
    return true;
}

bool ExtraKindRenderedAsSvgSymbol(ExtraKind kind)
{
    if (const ExtraKindTraits *traits = FindExtraKindTraits(kind)) {
        return traits->rendered_as_svg_symbol;
    }
    return true;
}

int SymNote::notation_size() const
{
    // AnnNote.notation_size (annotation.py:264-315), for fields materialized
    // by the active/experimental metric modes.
    int size = 0;
    for (const SymPitch &p : pitches) {
        size += 1; // pitch name
        if (p.accid != "None") size += 1;
        if (p.tie) size += 1;
    }
    size += 1; // note head
    size += dots * static_cast<int>(pitches.size());
    size += static_cast<int>(beamings.size());
    size += static_cast<int>(tuplets.size());
    size += static_cast<int>(tuplet_info.size());
    size += static_cast<int>(articulations.size());
    size += static_cast<int>(expressions.size());
    if (!grace_type.empty()) {
        size += 1;
        if (grace_slash) size += 1;
    }
    if (gap_dur != Fraction(0)) size += 1;
    return size;
}

std::string SymNote::str() const
{
    // AnnNote.__str__ (annotation.py:544-624), v1-tier fields only.
    std::string s = "[";
    for (std::size_t i = 0; i < pitches.size(); ++i) {
        if (i > 0) s += ",";
        s += pitches[i].step_octave;
        if (pitches[i].accid != "None") s += pitches[i].accid;
        if (pitches[i].tie) s += "T";
    }
    s += "]";
    s += note_head.str();
    for (int i = 0; i < dots; ++i) s += "*";
    if (!grace_type.empty()) {
        s += grace_type;
        if (grace_slash) s += "/";
    }
    if (!beamings.empty()) {
        s += "B";
        for (const BeamValue b : beamings) AppendBeamCode(s, b);
    }
    if (!tuplets.empty()) {
        s += "T";
        for (std::size_t i = 0; i < tuplets.size(); ++i) {
            AppendTupletCode(s, tuplets[i]);
            const std::string &ti = i < tuplet_info.size() ? tuplet_info[i] : std::string();
            if (!ti.empty()) s += "(" + ti + ")";
        }
    }
    if (!articulations.empty()) {
        s += "A";
        for (const std::string &a : articulations) s += a;
    }
    if (!expressions.empty()) {
        s += "E";
        for (const std::string &e : expressions) s += e;
    }
    if (gap_dur != Fraction(0)) s += " spaceBefore=" + gap_dur.str();
    return s;
}

std::string SymNote::repr() const
{
    // AnnNote.__repr__ (annotation.py:533-542) with Python list/tuple/bool
    // formatting, so it diffs cleanly against the reprs the oracle stores in
    // edit_ops. The leading GeneralNote(m21-id) is replaced by the MEI id.
    std::ostringstream os;
    os << "GeneralNote(" << vrv_id << "),G:" << (gap_dur == Fraction(0) ? "0.0" : gap_dur.str())
       << ",P:[";
    for (std::size_t i = 0; i < pitches.size(); ++i) {
        if (i > 0) os << ", ";
        os << "('" << pitches[i].step_octave << "', '" << pitches[i].accid << "', "
           << (pitches[i].tie ? "True" : "False") << ")";
    }
    os << "],H:" << note_head.str() << ",D:" << dots << ",B:[";
    for (std::size_t i = 0; i < beamings.size(); ++i) {
        if (i > 0) os << ", ";
        os << "'" << BeamValueName(beamings[i]) << "'";
    }
    os << "],T:[";
    for (std::size_t i = 0; i < tuplets.size(); ++i) {
        if (i > 0) os << ", ";
        os << "'" << TupletValueName(tuplets[i]) << "'";
    }
    os << "],TI:[";
    for (std::size_t i = 0; i < tuplet_info.size(); ++i) {
        if (i > 0) os << ", ";
        os << "'" << tuplet_info[i] << "'";
    }
    os << "],A:[";
    for (std::size_t i = 0; i < articulations.size(); ++i) {
        if (i > 0) os << ", ";
        os << "'" << articulations[i] << "'";
    }
    os << "],E:[";
    for (std::size_t i = 0; i < expressions.size(); ++i) {
        if (i > 0) os << ", ";
        os << "'" << expressions[i] << "'";
    }
    os << "],S:{}";
    return os.str();
}

int SymExtra::notation_size() const
{
    // AnnExtra.notation_size (annotation.py:717-737); content is per-character.
    int size = 0;
    if (content.has_value()) size += static_cast<int>(content->size());
    if (symbolic.has_value()) size += 1;
    if (duration.has_value()) size += 1;
    size += static_cast<int>(infodict.size());
    return size;
}

std::string SymExtra::str() const
{
    std::string s(ExtraKindName(kind));
    s += "@";
    s += ExtraKindHasMetricOffset(kind) ? offset.str() : "None";
    s += ":";
    if (content.has_value()) s += "content=" + *content;
    if (symbolic.has_value()) s += *symbolic;
    for (const auto &[k, v] : infodict) s += " " + k + ":" + v;
    return s;
}

int SymMeasure::notation_size() const
{
    int size = 0;
    for (const SymNote &n : notes) size += n.notation_size();
    for (const SymExtra &e : extras) size += e.notation_size();
    return size;
}

int SymPart::notation_size() const
{
    int size = 0;
    for (const SymMeasure &m : bar_list) size += m.notation_size();
    return size;
}

int SymScore::notation_size() const
{
    int size = 0;
    for (const SymPart &p : parts) size += p.notation_size();
    return size;
}

long SymbolCounts::total() const
{
    return pitches + accidentals + ties + noteheads + dots + beams + tuplets + tuplet_info
        + grace + grace_slash + gaps + articulations + expressions + style + barline + clef
        + keysig + timesig + other_extras;
}

SymbolCounts CountSymbols(const SymScore &score)
{
    SymbolCounts c;
    for (const SymPart &part : score.parts) {
        for (const SymMeasure &measure : part.bar_list) {
            for (const SymNote &note : measure.notes) {
                for (const SymPitch &p : note.pitches) {
                    c.pitches += 1;
                    if (p.accid != "None") c.accidentals += 1;
                    if (p.tie) c.ties += 1;
                }
                c.noteheads += 1;
                c.dots += note.dots * static_cast<long>(note.pitches.size());
                c.beams += static_cast<long>(note.beamings.size());
                c.tuplets += static_cast<long>(note.tuplets.size());
                c.tuplet_info += static_cast<long>(note.tuplet_info.size());
                c.articulations += static_cast<long>(note.articulations.size());
                c.expressions += static_cast<long>(note.expressions.size());
                if (!note.grace_type.empty()) {
                    c.grace += 1;
                    if (note.grace_slash) c.grace_slash += 1;
                }
                if (note.gap_dur != Fraction(0)) c.gaps += 1;
            }
            for (const SymExtra &extra : measure.extras) {
                long body = extra.notation_size();
                const ExtraKindTraits *traits = FindExtraKindTraits(extra.kind);
                switch (traits ? traits->count_bucket : SymbolCountBucket::kOtherExtras) {
                    case SymbolCountBucket::kBarline: c.barline += body; break;
                    case SymbolCountBucket::kClef: c.clef += body; break;
                    case SymbolCountBucket::kKeySig: c.keysig += body; break;
                    case SymbolCountBucket::kTimeSig: c.timesig += body; break;
                    case SymbolCountBucket::kOtherExtras: c.other_extras += body; break;
                }
            }
        }
    }
    return c;
}

} // namespace verosim
