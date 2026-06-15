#include "verosim/cli/count_symbols.h"

#include <cassert>
#include <exception>

#include "verosim/cli/json_util.h"
#include "verosim/extraction/extract.h"
#include "verosim/extraction/source_format_util.h"
#include "verosim/extraction/vrv_bridge.h"

namespace verosim {

namespace {

void WriteCategories(const SymbolCounts &c, std::ostream &os)
{
    // key order mirrors count_oracle.py CATEGORIES
    os << "{\"pitches\":" << c.pitches << ",\"accidentals\":" << c.accidentals
       << ",\"ties\":" << c.ties << ",\"noteheads\":" << c.noteheads << ",\"dots\":" << c.dots
       << ",\"beams\":" << c.beams << ",\"tuplets\":" << c.tuplets
       << ",\"tuplet_info\":" << c.tuplet_info << ",\"grace\":" << c.grace
       << ",\"grace_slash\":" << c.grace_slash << ",\"gaps\":" << c.gaps
       << ",\"articulations\":" << c.articulations << ",\"expressions\":" << c.expressions
       << ",\"style\":" << c.style << ",\"clef\":" << c.clef << ",\"keysig\":" << c.keysig
       << ",\"timesig\":" << c.timesig << ",\"other_extras\":" << c.other_extras << "}";
}

void WritePerMeasure(const SymScore &score, std::ostream &os)
{
    os << ",\"parts\":[";
    for (std::size_t p = 0; p < score.parts.size(); ++p) {
        const SymPart &part = score.parts[p];
        if (p > 0) os << ',';
        os << "{\"part_idx\":" << part.part_idx << ",\"staff_n\":";
        WriteJsonString(part.staff_n, os);
        os << ",\"size\":" << part.notation_size() << ",\"measures\":[";
        for (std::size_t m = 0; m < part.bar_list.size(); ++m) {
            const SymMeasure &measure = part.bar_list[m];
            if (m > 0) os << ',';
            os << "{\"n\":";
            WriteJsonString(measure.measure_n, os);
            os << ",\"size\":" << measure.notation_size() << ",\"notes\":[";
            for (std::size_t i = 0; i < measure.notes.size(); ++i) {
                const SymNote &note = measure.notes[i];
                if (i > 0) os << ',';
                os << "{\"offset\":";
                WriteJsonString(note.note_offset.str(), os);
                os << ",\"dur_type\":";
                WriteJsonString(note.note_dur_type, os);
                os << ",\"dur_dots\":" << note.note_dur_dots
                   << ",\"grace\":" << (note.note_is_grace ? "true" : "false")
                   << ",\"size\":" << note.notation_size() << ",\"sounding_alter\":"
                   << (note.pitches.empty() ? 0 : note.pitches.front().sounding_alter)
                   << ",\"str\":";
                WriteJsonString(note.str(), os);
                os << ",\"repr\":";
                WriteJsonString(note.repr(), os);
                os << ",\"id\":";
                WriteJsonString(note.vrv_id, os);
                os << "}";
            }
            os << "],\"extras\":[";
            for (std::size_t i = 0; i < measure.extras.size(); ++i) {
                const SymExtra &extra = measure.extras[i];
                if (i > 0) os << ',';
                os << "{\"kind\":";
                WriteJsonString(std::string(ExtraKindName(extra.kind)), os);
                os << ",\"offset\":";
                WriteJsonString(extra.offset.str(), os);
                os << ",\"size\":" << extra.notation_size() << ",\"str\":";
                WriteJsonString(extra.str(), os);
                os << "}";
            }
            os << "]}";
        }
        os << "]}";
    }
    os << ']';
}

} // namespace

bool CountSymbolsFile(VrvBridge &bridge, const std::string &path,
    const CountSymbolsOptions &options, std::ostream &os)
{
    os << "{\"path\":";
    WriteJsonString(path, os);

    bool loaded = false;
    std::string error;
    try {
        loaded = bridge.LoadScoreFile(path);
        if (!loaded) error = "load failed";
    }
    catch (const std::exception &e) {
        error = std::string("exception: ") + e.what();
    }
    if (!loaded) {
        os << ",\"ok\":false,\"error\":";
        WriteJsonString(error, os);
        os << "}\n";
        return false;
    }

    ExtractResult result = ExtractSymScore(bridge.GetDoc(), SourceFormatFromBridge(bridge));
    const SymbolCounts counts = CountSymbols(result.score);
    const int total = result.score.notation_size();
    // the per-category split must tile the notation size exactly
    assert(counts.total() == total);

    long nMeasures = 0;
    long nNotes = 0;
    for (const SymPart &part : result.score.parts) {
        nMeasures += static_cast<long>(part.bar_list.size());
        for (const SymMeasure &measure : part.bar_list) {
            nNotes += static_cast<long>(measure.notes.size());
        }
    }

    os << ",\"ok\":true,\"total\":" << total << ",\"n_parts\":" << result.score.parts.size()
       << ",\"n_measures\":" << nMeasures << ",\"n_notes\":" << nNotes << ",\"categories\":";
    WriteCategories(counts, os);
    os << ",\"warnings\":";
    WriteJsonStringArray(result.warnings, os);
    if (options.per_measure) WritePerMeasure(result.score, os);
    os << "}\n";
    return true;
}

} // namespace verosim
