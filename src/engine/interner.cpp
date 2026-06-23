#include "verosim/engine/interner.h"

namespace verosim {

std::string ExtraComparisonKey(const SymExtra &extra)
{
    std::string key(ExtraKindName(extra.kind));
    if (extra.content.has_value()) key += ",content=" + *extra.content;
    if (extra.symbolic.has_value()) key += ",symbol=" + *extra.symbolic;
    if (ExtraKindHasMetricOffset(extra.kind)) key += ",off=" + extra.offset.str();
    if (ExtraKindHasMetricDuration(extra.kind) && extra.duration.has_value()) {
        key += ",dur=" + extra.duration->str();
    }
    if (!extra.infodict.empty()) key += ",info:";
    for (const auto &[k, v] : extra.infodict) key += "," + k + "=" + v;
    return key;
}

namespace {

// AnnMeasure.__str__ equivalent as an id sequence (annotation.py:1277-1289):
// the note strs in order, then the extras strs in order. Serialized with
// unambiguous separators and re-interned, so measure equality (the Myers
// alphabet and AnnMeasure.__eq__) is one integer compare.
std::string MeasureContentKey(const PreparedMeasure &measure)
{
    std::string key = "N:";
    for (const std::int32_t id : measure.note_str_ids) key += std::to_string(id) + ",";
    key += "|E:";
    for (const std::int32_t id : measure.extra_str_ids) key += std::to_string(id) + ",";
    return key;
}

} // namespace

PreparedScore PrepareScore(const SymScore &score, StringInterner &interner)
{
    PreparedScore prepared;
    prepared.score = &score;
    prepared.parts.reserve(score.parts.size());
    for (const SymPart &part : score.parts) {
        PreparedPart &p = prepared.parts.emplace_back();
        p.measures.reserve(part.bar_list.size());
        for (const SymMeasure &measure : part.bar_list) {
            PreparedMeasure &m = p.measures.emplace_back();
            m.note_str_ids.reserve(measure.notes.size());
            for (const SymNote &note : measure.notes) {
                m.note_str_ids.push_back(interner.Intern(note.str()));
            }
            m.extra_str_ids.reserve(measure.extras.size());
            for (const SymExtra &extra : measure.extras) {
                m.extra_str_ids.push_back(interner.Intern(ExtraComparisonKey(extra)));
            }
            m.content_id = interner.Intern(MeasureContentKey(m));
            m.notation_size = measure.notation_size();
        }
    }
    return prepared;
}

} // namespace verosim
