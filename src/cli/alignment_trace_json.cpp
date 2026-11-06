#include "verosim/cli/alignment_trace_json.h"

#include <map>
#include <ostream>
#include <string>

#include "verosim/support/json_util.h"

namespace verosim {
namespace {

const void *Address(const OpSide &side)
{
    switch (side.kind) {
        case OpSide::Kind::kPart: return side.part;
        case OpSide::Kind::kMeasure: return side.measure;
        case OpSide::Kind::kNote: return side.note;
        case OpSide::Kind::kExtra: return side.extra;
        case OpSide::Kind::kNone: return nullptr;
    }
    return nullptr;
}

const char *Kind(const OpSide &side)
{
    switch (side.kind) {
        case OpSide::Kind::kPart: return "part";
        case OpSide::Kind::kMeasure: return "measure";
        case OpSide::Kind::kNote: return "note";
        case OpSide::Kind::kExtra: return "extra";
        case OpSide::Kind::kNone: return "none";
    }
    return "none";
}

using SymbolKeys = std::map<const void *, std::string>;

void WriteRef(const OpSide &side, const SymbolKeys &keys, std::ostream &out)
{
    if (side.kind == OpSide::Kind::kNone) out << "null";
    else WriteJsonString(keys.at(Address(side)), out);
}

void WriteEndpoint(const OpSide &side, const std::vector<OpSide> &group,
    const SymbolKeys &keys, std::ostream &out)
{
    if (group.empty()) {
        WriteRef(side, keys, out);
        return;
    }
    out << '[';
    for (std::size_t i = 0; i < group.size(); ++i) {
        if (i) out << ',';
        WriteRef(group[i], keys, out);
    }
    out << ']';
}

void WriteOptionalCost(const std::optional<long> &cost, std::ostream &out)
{
    if (cost.has_value()) out << *cost;
    else out << "null";
}

SymbolKeys WriteInventory(const SymScore &score, std::ostream &out)
{
    SymbolKeys keys;
    bool first = true;
    const auto begin = [&](const OpSide &side, const std::string &key, const std::string &parent,
                           const std::string &id, const std::string &visual_id,
                           int part, int measure, const std::string &staff, long size) {
        keys.emplace(Address(side), key);
        if (!first) out << ',';
        first = false;
        out << "{\"key\":";
        WriteJsonString(key, out);
        out << ",\"kind\":\"" << Kind(side) << "\",\"parent\":";
        if (parent.empty()) out << "null";
        else WriteJsonString(parent, out);
        out << ",\"source_id\":";
        WriteJsonString(id, out);
        out << ",\"visual_id\":";
        WriteJsonString(visual_id, out);
        out << ",\"part_index\":" << part << ",\"measure_index\":" << measure << ",\"staff\":";
        WriteJsonString(staff, out);
        out << ",\"notation_size\":" << size;
    };
    out << '[';
    for (std::size_t p = 0; p < score.parts.size(); ++p) {
        const SymPart &part = score.parts[p];
        const std::string part_key = "p" + std::to_string(p);
        begin(OpSide::Part(&part), part_key, {}, part.staff_n, {}, p, -1, part.staff_n,
            part.notation_size());
        out << '}';
        for (std::size_t m = 0; m < part.bar_list.size(); ++m) {
            const SymMeasure &measure = part.bar_list[m];
            const std::string measure_key = part_key + "/m" + std::to_string(m);
            begin(OpSide::Measure(&measure), measure_key, part_key, measure.vrv_id,
                measure.vrv_id, p, measure.locator.measure_idx, part.staff_n, measure.notation_size());
            out << ",\"measure_label\":";
            WriteJsonString(measure.measure_n, out);
            out << '}';
            for (std::size_t n = 0; n < measure.notes.size(); ++n) {
                const SymNote &note = measure.notes[n];
                begin(OpSide::Note(&note), measure_key + "/n" + std::to_string(n), measure_key,
                    note.vrv_id, note.visual_id, p, measure.locator.measure_idx, part.staff_n,
                    note.notation_size());
                out << ",\"chord_member\":" << note.note_idx_in_chord << ",\"pitch\":";
                WriteJsonString(note.pitches[0].step_octave, out);
                out << ",\"accidental\":";
                WriteJsonString(note.pitches[0].accid, out);
                out << ",\"onset\":";
                WriteJsonString(note.note_offset.str(), out);
                out << ",\"duration_type\":";
                WriteJsonString(note.note_dur_type, out);
                out << ",\"dots\":" << note.note_dur_dots
                    << ",\"grace\":" << (note.note_is_grace ? "true" : "false") << ",\"content\":";
                WriteJsonString(note.str(), out);
                out << '}';
            }
            for (std::size_t e = 0; e < measure.extras.size(); ++e) {
                const SymExtra &extra = measure.extras[e];
                begin(OpSide::Extra(&extra), measure_key + "/e" + std::to_string(e), measure_key,
                    extra.vrv_id, extra.vrv_id, p, measure.locator.measure_idx, part.staff_n,
                    extra.notation_size());
                out << ",\"extra_kind\":";
                WriteJsonString(std::string(ExtraKindName(extra.kind)), out);
                out << ",\"onset\":";
                WriteJsonString(extra.offset.str(), out);
                out << ",\"content\":";
                WriteJsonString(extra.str(), out);
                out << '}';
            }
        }
    }
    out << ']';
    return keys;
}

} // namespace

void WriteAlignmentTraceJson(const SymScore &prediction, const SymScore &ground_truth,
    const AlignmentTrace &trace, std::ostream &out)
{
    out << "\"symbols\":{\"prediction\":";
    const SymbolKeys pred_keys = WriteInventory(prediction, out);
    out << ",\"ground_truth\":";
    const SymbolKeys gt_keys = WriteInventory(ground_truth, out);
    out << "},\"decisions\":[";
    bool first = true;
    for (const AlignmentDecision &decision : trace.decisions) {
        if (!first) out << ',';
        first = false;
        out << "{\"level\":\""
            << Kind(!decision.prediction_group.empty() ? decision.prediction_group.front()
                : decision.prediction.kind == OpSide::Kind::kNone
                    ? decision.ground_truth : decision.prediction) << "\",\"prediction\":";
        WriteEndpoint(decision.prediction, decision.prediction_group, pred_keys, out);
        out << ",\"ground_truth\":";
        WriteEndpoint(decision.ground_truth, decision.ground_truth_group, gt_keys, out);
        out << ",\"algorithm\":";
        WriteJsonString(decision.algorithm, out);
        out << ",\"reason\":";
        WriteJsonString(decision.reason, out);
        out << ",\"local_cost\":" << decision.local_cost << ",\"total_cost\":";
        WriteOptionalCost(decision.total_cost, out);
        out << ",\"alternatives\":[";
        bool first_alternative = true;
        for (const AlignmentAlternative &alternative : decision.alternatives) {
            if (!first_alternative) out << ',';
            first_alternative = false;
            out << "{\"action\":";
            WriteJsonString(alternative.action, out);
            out << ",\"prediction\":";
            WriteEndpoint(alternative.prediction, alternative.prediction_group, pred_keys, out);
            out << ",\"ground_truth\":";
            WriteEndpoint(alternative.ground_truth, alternative.ground_truth_group, gt_keys, out);
            out << ",\"rejection\":";
            WriteJsonString(alternative.rejection, out);
            out << ",\"local_cost\":";
            WriteOptionalCost(alternative.local_cost, out);
            out << ",\"total_cost\":";
            WriteOptionalCost(alternative.total_cost, out);
            out << ",\"preferred\":" << (alternative.preferred ? "true" : "false") << '}';
        }
        out << "]}";
    }
    out << ']';
}

} // namespace verosim
