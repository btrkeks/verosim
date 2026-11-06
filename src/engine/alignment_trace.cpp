#include "verosim/engine/alignment_trace.h"

#include <utility>

namespace verosim {

bool AlignmentTrace::WantsDetails(
    const SymMeasure &prediction, const SymMeasure &ground_truth) const
{
    return detailed_measures.empty() || detailed_measures.contains(prediction.locator.measure_idx)
        || detailed_measures.contains(ground_truth.locator.measure_idx);
}

void AlignmentTrace::RecordUnmatched(const OpSide &symbol, bool prediction,
    const std::string &reason, std::vector<AlignmentAlternative> alternatives,
    std::optional<long> total_cost)
{
    long size = 0;
    switch (symbol.kind) {
        case OpSide::Kind::kPart: size = symbol.part->notation_size(); break;
        case OpSide::Kind::kMeasure: size = symbol.measure->notation_size(); break;
        case OpSide::Kind::kNote: size = symbol.note->notation_size(); break;
        case OpSide::Kind::kExtra: size = symbol.extra->notation_size(); break;
        case OpSide::Kind::kNone: return;
    }
    decisions.push_back({ .prediction = prediction ? symbol : OpSide::None(),
        .ground_truth = prediction ? OpSide::None() : symbol,
        .algorithm = "unmatched",
        .reason = reason,
        .local_cost = size,
        .total_cost = total_cost,
        .alternatives = std::move(alternatives) });
    if (symbol.kind == OpSide::Kind::kPart) {
        for (const SymMeasure &measure : symbol.part->bar_list) {
            RecordUnmatched(OpSide::Measure(&measure), prediction, "ancestor_unmatched");
        }
    }
    if (symbol.kind == OpSide::Kind::kMeasure) {
        for (const SymNote &note : symbol.measure->notes) {
            RecordUnmatched(OpSide::Note(&note), prediction, "ancestor_unmatched");
        }
        for (const SymExtra &extra : symbol.measure->extras) {
            RecordUnmatched(OpSide::Extra(&extra), prediction, "ancestor_unmatched");
        }
    }
}

void AlignmentTrace::RecordEqualMeasure(const SymMeasure &prediction,
    const SymMeasure &ground_truth, const std::string &reason,
    std::vector<AlignmentAlternative> alternatives, std::optional<long> total_cost)
{
    decisions.push_back({ .prediction = OpSide::Measure(&prediction),
        .ground_truth = OpSide::Measure(&ground_truth),
        .algorithm = "content_equality",
        .reason = reason,
        .total_cost = total_cost,
        .alternatives = std::move(alternatives) });
    for (std::size_t i = 0; i < prediction.notes.size(); ++i) {
        decisions.push_back({ .prediction = OpSide::Note(&prediction.notes[i]),
            .ground_truth = OpSide::Note(&ground_truth.notes[i]),
            .algorithm = "content_equality",
            .reason = "measure_content_equality_shortcut" });
    }
    for (std::size_t i = 0; i < prediction.extras.size(); ++i) {
        decisions.push_back({ .prediction = OpSide::Extra(&prediction.extras[i]),
            .ground_truth = OpSide::Extra(&ground_truth.extras[i]),
            .algorithm = "content_equality",
            .reason = "measure_content_equality_shortcut" });
    }
}

} // namespace verosim
