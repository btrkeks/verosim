#include "verosim/engine/score_alignment.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "verosim/engine/alignment_trace.h"
#include "verosim/engine/set_distance.h"

namespace verosim
{
namespace
{

enum class Action
{
    kPair,
    kDelete,
    kInsert,
    kSplit,
    kMerge
};

struct Step
{
    Action action;
    int prediction;
    int ground_truth;
    long local_cost;
    long total_cost;
    std::optional<long> pair_total;
    std::optional<long> delete_total;
    std::optional<long> insert_total;
    std::optional<long> split_total;
    std::optional<long> merge_total;
};

int PredictionCount(Action action)
{
    return action == Action::kInsert ? 0 : action == Action::kMerge ? 2 : 1;
}

int GroundTruthCount(Action action)
{
    return action == Action::kDelete ? 0 : action == Action::kSplit ? 2 : 1;
}

struct SequenceAlignment
{
    long cost = 0;
    std::vector<Step> steps;
};

SequenceAlignment AlignSequence(const std::vector<long> &prediction_sizes,
                                const std::vector<long> &ground_truth_sizes,
                                const std::vector<long> &pair_costs,
                                const std::vector<std::optional<long>> &split_costs = {},
                                const std::vector<std::optional<long>> &merge_costs = {})
{
    const int m = static_cast<int>(prediction_sizes.size());
    const int n = static_cast<int>(ground_truth_sizes.size());
    std::vector<long> costs(static_cast<std::size_t>(m + 1) * (n + 1));
    std::vector<Action> choices(costs.size(), Action::kPair);
    const auto at = [&](int i, int j) -> long &
    { return costs[static_cast<std::size_t>(i) * (n + 1) + j]; };
    for (int i = m; i >= 0; --i)
    {
        for (int j = n; j >= 0; --j)
        {
            if (i == m && j == n)
                continue;
            Action choice = i < m && j < n ? Action::kPair
                            : i < m        ? Action::kDelete
                                           : Action::kInsert;
            long best = i < m && j < n ? pair_costs[i * n + j] + at(i + 1, j + 1)
                        : i < m        ? prediction_sizes[i] + at(i + 1, j)
                                       : ground_truth_sizes[j] + at(i, j + 1);
            if (i < m && prediction_sizes[i] + at(i + 1, j) < best)
            {
                best = prediction_sizes[i] + at(i + 1, j);
                choice = Action::kDelete;
            }
            if (j < n && ground_truth_sizes[j] + at(i, j + 1) < best)
            {
                best = ground_truth_sizes[j] + at(i, j + 1);
                choice = Action::kInsert;
            }
            if (i < m && j + 1 < n && !split_costs.empty() && split_costs[i * n + j] &&
                *split_costs[i * n + j] + at(i + 1, j + 2) < best)
            {
                best = *split_costs[i * n + j] + at(i + 1, j + 2);
                choice = Action::kSplit;
            }
            if (i + 1 < m && j < n && !merge_costs.empty() && merge_costs[i * n + j] &&
                *merge_costs[i * n + j] + at(i + 2, j + 1) < best)
            {
                best = *merge_costs[i * n + j] + at(i + 2, j + 1);
                choice = Action::kMerge;
            }
            at(i, j) = best;
            choices[i * (n + 1) + j] = choice;
        }
    }
    SequenceAlignment result{.cost = at(0, 0)};
    int i = 0, j = 0;
    while (i < m || j < n)
    {
        const Action action = choices[i * (n + 1) + j];
        const long suffix = at(i + PredictionCount(action), j + GroundTruthCount(action));
        result.steps.push_back(
            {.action = action,
             .prediction = i,
             .ground_truth = j,
             .local_cost = at(i, j) - suffix,
             .total_cost = at(i, j),
             .pair_total = i < m && j < n
                               ? std::optional<long>(pair_costs[i * n + j] + at(i + 1, j + 1))
                               : std::nullopt,
             .delete_total =
                 i < m ? std::optional<long>(prediction_sizes[i] + at(i + 1, j)) : std::nullopt,
             .insert_total =
                 j < n ? std::optional<long>(ground_truth_sizes[j] + at(i, j + 1)) : std::nullopt,
             .split_total = i < m && j + 1 < n && !split_costs.empty() && split_costs[i * n + j]
                                ? std::optional<long>(*split_costs[i * n + j] + at(i + 1, j + 2))
                                : std::nullopt,
             .merge_total = i + 1 < m && j < n && !merge_costs.empty() && merge_costs[i * n + j]
                                ? std::optional<long>(*merge_costs[i * n + j] + at(i + 2, j + 1))
                                : std::nullopt});
        i += PredictionCount(action);
        j += GroundTruthCount(action);
    }
    return result;
}

std::vector<AlignmentAlternative> Alternatives(const Step &step, const OpSide &a, const OpSide &b,
                                               long prediction_size, long ground_truth_size,
                                               long pair_cost, const OpSide &a2 = OpSide::None(),
                                               const OpSide &b2 = OpSide::None(),
                                               std::optional<long> split_cost = std::nullopt,
                                               std::optional<long> merge_cost = std::nullopt)
{
    std::vector<AlignmentAlternative> result;
    if (step.pair_total)
        result.push_back({.action = "pair",
                          .prediction = a,
                          .ground_truth = b,
                          .local_cost = pair_cost,
                          .total_cost = step.pair_total,
                          .preferred = step.action == Action::kPair});
    if (step.delete_total)
        result.push_back({.action = "delete",
                          .prediction = a,
                          .local_cost = prediction_size,
                          .total_cost = step.delete_total,
                          .preferred = step.action == Action::kDelete});
    if (step.insert_total)
        result.push_back({.action = "insert",
                          .ground_truth = b,
                          .local_cost = ground_truth_size,
                          .total_cost = step.insert_total,
                          .preferred = step.action == Action::kInsert});
    if (step.split_total)
        result.push_back({.action = "split",
                          .local_cost = split_cost,
                          .total_cost = step.split_total,
                          .preferred = step.action == Action::kSplit,
                          .prediction_group = {a},
                          .ground_truth_group = {b, b2}});
    if (step.merge_total)
        result.push_back({.action = "merge",
                          .local_cost = merge_cost,
                          .total_cost = step.merge_total,
                          .preferred = step.action == Action::kMerge,
                          .prediction_group = {a, a2},
                          .ground_truth_group = {b}});
    return result;
}

bool CanJoin(const SymMeasure &single, const SymMeasure &first, const SymMeasure &second)
{
    const int i = first.locator.measure_idx, j = second.locator.measure_idx;
    return ((i < 0 && j < 0) || (i >= 0 && j == i + 1)) && first.content_duration > Fraction(0) &&
           second.content_duration > Fraction(0) &&
           single.content_duration == first.content_duration + second.content_duration;
}

// Temporary span coordinates never escape this comparison. Operations and
// trace references are rebound to the caller's original symbols below.
struct Span
{
    SymMeasure measure;
    std::vector<const SymNote *> notes;
    std::vector<const SymExtra *> extras;

    explicit Span(const std::vector<const SymMeasure *> &bars)
    {
        measure.locator = bars.front()->locator;
        std::vector<std::pair<SymNote, const SymNote *>> ns;
        std::vector<std::pair<SymExtra, const SymExtra *>> es;
        Fraction offset;
        for (const auto *bar : bars)
        {
            for (const auto &note : bar->notes)
            {
                auto copy = note;
                copy.note_offset += offset;
                ns.emplace_back(std::move(copy), &note);
            }
            for (const auto &extra : bar->extras)
            {
                auto copy = extra;
                copy.offset += offset;
                es.emplace_back(std::move(copy), &extra);
            }
            offset += bar->content_duration;
        }
        measure.content_duration = offset;
        std::stable_sort(ns.begin(), ns.end(), [](const auto &a, const auto &b)
                         { return a.first.note_offset < b.first.note_offset; });
        std::stable_sort(es.begin(), es.end(), [](const auto &a, const auto &b)
                         { return a.first.kind == b.first.kind ? a.first.offset < b.first.offset
                                                              : a.first.kind < b.first.kind; });
        for (auto &[note, source] : ns)
        {
            measure.notes.push_back(std::move(note));
            notes.push_back(source);
        }
        for (auto &[extra, source] : es)
        {
            measure.extras.push_back(std::move(extra));
            extras.push_back(source);
        }
    }

    OpSide Original(const OpSide &side) const
    {
        if (side.kind == OpSide::Kind::kNote)
            return OpSide::Note(notes[side.note - measure.notes.data()]);
        if (side.kind == OpSide::Kind::kExtra)
            return OpSide::Extra(extras[side.extra - measure.extras.data()]);
        return side;
    }
};

long SpanCost(const std::vector<const SymMeasure *> &prediction,
              const std::vector<const SymMeasure *> &ground_truth, const CompareOptions &options)
{
    const Span a(prediction), b(ground_truth);
    StringInterner interner;
    const auto pa = PrepareMeasure(a.measure, interner), pb = PrepareMeasure(b.measure, interner);
    return (options.score_barline_boundaries ? 1 : 0) +
           NotesSetDistanceCost(a.measure, b.measure, pa, pb, options) +
           ExtrasSetDistanceCost(a.measure, b.measure, pa, pb);
}

DiffResult SpanDistance(const std::vector<const SymMeasure *> &prediction,
                        const std::vector<const SymMeasure *> &ground_truth,
                        const CompareOptions &options)
{
    const Span a(prediction), b(ground_truth);
    StringInterner interner;
    const auto pa = PrepareMeasure(a.measure, interner), pb = PrepareMeasure(b.measure, interner);
    AlignmentTrace trace;
    if (options.alignment_trace)
    {
        trace.detailed_measures = options.alignment_trace->detailed_measures;
        for (const auto &bars : {prediction, ground_truth})
            for (const auto *bar : bars)
                if (trace.detailed_measures.contains(bar->locator.measure_idx))
                    trace.detailed_measures.clear();
    }
    CompareOptions local = options;
    local.alignment_trace = options.alignment_trace ? &trace : nullptr;
    auto notes = NotesSetDistance(a.measure, b.measure, pa, pb, local);
    auto extras = ExtrasSetDistance(a.measure, b.measure, pa, pb, local);
    notes.cost += extras.cost + (options.score_barline_boundaries ? 1 : 0);
    notes.ops.insert(notes.ops.end(), std::make_move_iterator(extras.ops.begin()),
                     std::make_move_iterator(extras.ops.end()));
    for (auto &op : notes.ops)
    {
        op.a = a.Original(op.a);
        op.b = b.Original(op.b);
    }
    const bool split = ground_truth.size() == 2;
    if (options.score_barline_boundaries)
        notes.ops.push_back({.name = split ? OpName::kBarlineIns : OpName::kBarlineDel,
                         .a = split ? OpSide::None() : OpSide::Measure(prediction.front()),
                         .b = split ? OpSide::Measure(ground_truth.front()) : OpSide::None(),
                         .cost = 1});
    if (options.alignment_trace)
        for (auto &decision : trace.decisions)
        {
            decision.prediction = a.Original(decision.prediction);
            decision.ground_truth = b.Original(decision.ground_truth);
            for (auto &alternative : decision.alternatives)
            {
                alternative.prediction = a.Original(alternative.prediction);
                alternative.ground_truth = b.Original(alternative.ground_truth);
            }
            options.alignment_trace->decisions.push_back(std::move(decision));
        }
    return notes;
}

struct PartCosts
{
    int prediction;
    int ground_truth;
    int width;
    std::vector<long> prediction_sizes;
    std::vector<long> ground_truth_sizes;
    std::vector<long> pairs;
    std::vector<std::optional<long>> splits;
    std::vector<std::optional<long>> merges;

    long At(int a, int b) const
    {
        return pairs[a * width + b];
    }
};

PartCosts PreparePartCosts(int a, int b, const SymScore &prediction, const SymScore &ground_truth,
                           const PreparedScore &pa, const PreparedScore &pb,
                           const CompareOptions &options)
{
    const auto &ma = prediction.parts[a].bar_list;
    const auto &mb = ground_truth.parts[b].bar_list;
    PartCosts result{.prediction = a, .ground_truth = b, .width = static_cast<int>(mb.size())};
    for (const auto &measure : pa.parts[a].measures)
        result.prediction_sizes.push_back(measure.notation_size);
    for (const auto &measure : pb.parts[b].measures)
        result.ground_truth_sizes.push_back(measure.notation_size);
    result.pairs.reserve(ma.size() * mb.size());
    result.splits.resize(ma.size() * mb.size());
    result.merges.resize(ma.size() * mb.size());
    for (std::size_t i = 0; i < ma.size(); ++i)
    {
        for (std::size_t j = 0; j < mb.size(); ++j)
        {
            const auto &p = pa.parts[a].measures[i];
            const auto &q = pb.parts[b].measures[j];
            result.pairs.push_back(p.content_id == q.content_id
                                       ? 0
                                       : NotesSetDistanceCost(ma[i], mb[j], p, q, options) +
                                             ExtrasSetDistanceCost(ma[i], mb[j], p, q));
            if (j + 1 < mb.size() && CanJoin(ma[i], mb[j], mb[j + 1]))
                result.splits[i * mb.size() + j] = SpanCost({&ma[i]}, {&mb[j], &mb[j + 1]}, options);
            if (i + 1 < ma.size() && CanJoin(mb[j], ma[i], ma[i + 1]))
                result.merges[i * mb.size() + j] = SpanCost({&ma[i], &ma[i + 1]}, {&mb[j]}, options);
        }
    }
    return result;
}

struct Frame
{
    std::vector<int> measures;
    long size = 0;
};

std::vector<Frame> Frames(const SymScore &score, const std::vector<PartCosts> &parts,
                          bool prediction)
{
    std::map<int, Frame> by_source_measure;
    for (std::size_t p = 0; p < parts.size(); ++p)
    {
        const auto &part = score.parts[prediction ? parts[p].prediction : parts[p].ground_truth];
        for (std::size_t i = 0; i < part.bar_list.size(); ++i)
        {
            const auto &measure = part.bar_list[i];
            const int index =
                measure.locator.measure_idx < 0 ? static_cast<int>(i) : measure.locator.measure_idx;
            auto [it, inserted] =
                by_source_measure.try_emplace(index, Frame{std::vector<int>(parts.size(), -1)});
            it->second.measures[p] = static_cast<int>(i);
            it->second.size += measure.notation_size();
        }
    }
    std::vector<Frame> result;
    for (auto &[index, frame] : by_source_measure)
        result.push_back(std::move(frame));
    return result;
}

void UnmatchedPart(CompareResult &result, const SymPart &part, bool prediction,
                   AlignmentTrace *trace, std::vector<AlignmentAlternative> alternatives,
                   long total_cost)
{
    result.op_list.push_back({.name = prediction ? OpName::kDelPart : OpName::kInsPart,
                              .a = prediction ? OpSide::Part(&part) : OpSide::None(),
                              .b = prediction ? OpSide::None() : OpSide::Part(&part),
                              .cost = part.notation_size()});
    result.cost += part.notation_size();
    if (trace)
        trace->RecordUnmatched(OpSide::Part(&part), prediction,
                               "unmatched_staff_sequence_alignment", std::move(alternatives),
                               total_cost);
}

} // namespace

CompareResult CompareSharedAlignment(const SymScore &prediction, const SymScore &ground_truth,
                                     const PreparedScore &pa, const PreparedScore &pb,
                                     const CompareOptions &options)
{
    CompareResult result;
    std::vector<long> prediction_sizes, ground_truth_sizes, staff_pair_costs;
    for (const auto &part : prediction.parts)
        prediction_sizes.push_back(part.notation_size());
    for (const auto &part : ground_truth.parts)
        ground_truth_sizes.push_back(part.notation_size());
    std::vector<PartCosts> all_part_costs;
    for (std::size_t i = 0; i < prediction.parts.size(); ++i)
    {
        for (std::size_t j = 0; j < ground_truth.parts.size(); ++j)
        {
            auto costs = PreparePartCosts(i, j, prediction, ground_truth, pa, pb, options);
            staff_pair_costs.push_back(
                AlignSequence(costs.prediction_sizes, costs.ground_truth_sizes, costs.pairs,
                              costs.splits, costs.merges).cost);
            all_part_costs.push_back(std::move(costs));
        }
    }
    const auto staff_alignment =
        AlignSequence(prediction_sizes, ground_truth_sizes, staff_pair_costs);
    std::vector<PartCosts> parts;
    for (const auto &step : staff_alignment.steps)
    {
        const auto *a = step.prediction < static_cast<int>(prediction.parts.size())
                            ? &prediction.parts[step.prediction]
                            : nullptr;
        const auto *b = step.ground_truth < static_cast<int>(ground_truth.parts.size())
                            ? &ground_truth.parts[step.ground_truth]
                            : nullptr;
        std::vector<AlignmentAlternative> alternatives;
        if (options.alignment_trace)
            alternatives = Alternatives(
                step, a ? OpSide::Part(a) : OpSide::None(), b ? OpSide::Part(b) : OpSide::None(),
                a ? prediction_sizes[step.prediction] : 0,
                b ? ground_truth_sizes[step.ground_truth] : 0,
                a && b ? staff_pair_costs[step.prediction * ground_truth.parts.size() +
                                          step.ground_truth]
                       : 0);
        if (step.action == Action::kDelete)
        {
            UnmatchedPart(result, *a, true, options.alignment_trace, std::move(alternatives),
                          step.total_cost);
        }
        else if (step.action == Action::kInsert)
        {
            UnmatchedPart(result, *b, false, options.alignment_trace, std::move(alternatives),
                          step.total_cost);
        }
        else
        {
            parts.push_back(std::move(
                all_part_costs[step.prediction * ground_truth.parts.size() + step.ground_truth]));
            if (options.alignment_trace)
                options.alignment_trace->decisions.push_back(
                    {.prediction = OpSide::Part(a),
                     .ground_truth = OpSide::Part(b),
                     .algorithm = "staff_sequence_dp",
                     .reason = "minimum_staff_span_alignment_cost_pair_first_ties",
                     .local_cost = step.local_cost,
                     .total_cost = step.total_cost,
                     .alternatives = std::move(alternatives)});
        }
    }

    const auto a = Frames(prediction, parts, true);
    const auto b = Frames(ground_truth, parts, false);
    std::vector<long> a_sizes, b_sizes, pair_costs;
    for (const auto &frame : a)
        a_sizes.push_back(frame.size);
    for (const auto &frame : b)
        b_sizes.push_back(frame.size);
    std::vector<std::optional<long>> split_costs(a.size() * b.size());
    std::vector<std::optional<long>> merge_costs(a.size() * b.size());
    for (std::size_t x = 0; x < a.size(); ++x)
        for (std::size_t y = 0; y < b.size(); ++y)
        {
            const auto &fa = a[x], &fb = b[y];
            long cost = 0;
            std::optional<long> split = y + 1 < b.size() ? std::optional<long>(0) : std::nullopt;
            std::optional<long> merge = x + 1 < a.size() ? std::optional<long>(0) : std::nullopt;
            for (std::size_t p = 0; p < parts.size(); ++p)
            {
                const int i = fa.measures[p], j = fb.measures[p];
                cost += i >= 0 && j >= 0 ? parts[p].At(i, j)
                        : i >= 0         ? parts[p].prediction_sizes[i]
                        : j >= 0         ? parts[p].ground_truth_sizes[j]
                                         : 0;
                const int i2 = x + 1 < a.size() ? a[x + 1].measures[p] : -1;
                const int j2 = y + 1 < b.size() ? b[y + 1].measures[p] : -1;
                const auto &pc = parts[p];
                if (split && i >= 0 && j >= 0 && j2 == j + 1 && pc.splits[i * pc.width + j])
                    *split += *pc.splits[i * pc.width + j];
                else
                    split = std::nullopt;
                if (merge && i >= 0 && j >= 0 && i2 == i + 1 && pc.merges[i * pc.width + j])
                    *merge += *pc.merges[i * pc.width + j];
                else
                    merge = std::nullopt;
            }
            pair_costs.push_back(cost);
            split_costs[x * b.size() + y] = split;
            merge_costs[x * b.size() + y] = merge;
        }
    const auto alignment = AlignSequence(a_sizes, b_sizes, pair_costs, split_costs, merge_costs);
    for (const auto &step : alignment.steps)
    {
        for (std::size_t p = 0; p < parts.size(); ++p)
        {
            const int i =
                step.prediction < static_cast<int>(a.size()) ? a[step.prediction].measures[p] : -1;
            const int j = step.ground_truth < static_cast<int>(b.size())
                              ? b[step.ground_truth].measures[p]
                              : -1;
            const auto *ma = i >= 0 ? &prediction.parts[parts[p].prediction].bar_list[i] : nullptr;
            const auto *mb =
                j >= 0 ? &ground_truth.parts[parts[p].ground_truth].bar_list[j] : nullptr;
            const int i2 = step.prediction + 1 < static_cast<int>(a.size())
                               ? a[step.prediction + 1].measures[p] : -1;
            const int j2 = step.ground_truth + 1 < static_cast<int>(b.size())
                               ? b[step.ground_truth + 1].measures[p] : -1;
            const auto *ma2 = i2 >= 0 ? &prediction.parts[parts[p].prediction].bar_list[i2] : nullptr;
            const auto *mb2 = j2 >= 0 ? &ground_truth.parts[parts[p].ground_truth].bar_list[j2] : nullptr;
            std::vector<AlignmentAlternative> alternatives;
            auto *trace = options.alignment_trace;
            if (trace && ma && mb && (trace->WantsDetails(*ma, *mb) ||
                (ma2 && trace->WantsDetails(*ma2, *mb)) ||
                (mb2 && trace->WantsDetails(*ma, *mb2))))
            {
                alternatives =
                    Alternatives(step, OpSide::Measure(ma), OpSide::Measure(mb),
                                 a_sizes[step.prediction], b_sizes[step.ground_truth],
                                 pair_costs[step.prediction * b.size() + step.ground_truth],
                                 ma2 ? OpSide::Measure(ma2) : OpSide::None(),
                                 mb2 ? OpSide::Measure(mb2) : OpSide::None(),
                                 split_costs[step.prediction * b.size() + step.ground_truth],
                                 merge_costs[step.prediction * b.size() + step.ground_truth]);
            }
            if (step.action == Action::kSplit || step.action == Action::kMerge)
            {
                const bool split = step.action == Action::kSplit;
                const std::vector<const SymMeasure *> aa = split
                    ? std::vector<const SymMeasure *>{ma} : std::vector<const SymMeasure *>{ma, ma2};
                const std::vector<const SymMeasure *> bb = split
                    ? std::vector<const SymMeasure *>{mb, mb2} : std::vector<const SymMeasure *>{mb};
                auto diff = SpanDistance(aa, bb, options);
                result.cost += diff.cost;
                result.op_list.insert(result.op_list.end(), std::make_move_iterator(diff.ops.begin()),
                                      std::make_move_iterator(diff.ops.end()));
                if (trace)
                {
                    AlignmentDecision decision{
                        .algorithm = "shared_bar_span_dp",
                        .reason = split ? "missing_predicted_boundary" : "extra_predicted_boundary",
                        .local_cost = diff.cost,
                        .total_cost = step.total_cost,
                        .alternatives = std::move(alternatives)};
                    for (const auto *bar : aa) decision.prediction_group.push_back(OpSide::Measure(bar));
                    for (const auto *bar : bb) decision.ground_truth_group.push_back(OpSide::Measure(bar));
                    trace->decisions.push_back(std::move(decision));
                }
            }
            else if (step.action == Action::kInsert || (step.action == Action::kPair && !ma))
            {
                if (!mb)
                    continue;
                result.op_list.push_back({.name = OpName::kInsBar,
                                          .b = OpSide::Measure(mb),
                                          .cost = mb->notation_size()});
                result.cost += mb->notation_size();
                if (trace)
                    trace->RecordUnmatched(OpSide::Measure(mb), false,
                                           "shared_frame_insert_pair_first_ties",
                                           std::move(alternatives), step.total_cost);
            }
            else if (step.action == Action::kDelete || !mb)
            {
                if (!ma)
                    continue;
                result.op_list.push_back({.name = OpName::kDelBar,
                                          .a = OpSide::Measure(ma),
                                          .cost = ma->notation_size()});
                result.cost += ma->notation_size();
                if (trace)
                    trace->RecordUnmatched(OpSide::Measure(ma), true,
                                           "shared_frame_delete_pair_first_ties",
                                           std::move(alternatives), step.total_cost);
            }
            else
            {
                const auto &qa = pa.parts[parts[p].prediction].measures[i];
                const auto &qb = pb.parts[parts[p].ground_truth].measures[j];
                if (qa.content_id == qb.content_id)
                {
                    if (trace)
                        trace->RecordEqualMeasure(*ma, *mb, "shared_frame_content_equality",
                                                  std::move(alternatives), step.total_cost);
                    continue;
                }
                auto notes = NotesSetDistance(*ma, *mb, qa, qb, options);
                auto extras = ExtrasSetDistance(*ma, *mb, qa, qb, options);
                result.cost += notes.cost + extras.cost;
                result.op_list.insert(result.op_list.end(),
                                      std::make_move_iterator(notes.ops.begin()),
                                      std::make_move_iterator(notes.ops.end()));
                result.op_list.insert(result.op_list.end(),
                                      std::make_move_iterator(extras.ops.begin()),
                                      std::make_move_iterator(extras.ops.end()));
                if (trace)
                    trace->decisions.push_back(
                        {.prediction = OpSide::Measure(ma),
                         .ground_truth = OpSide::Measure(mb),
                         .algorithm = "shared_bar_dp",
                         .reason = "minimum_shared_frame_suffix_cost_pair_first_ties",
                         .local_cost = notes.cost + extras.cost,
                         .total_cost = step.total_cost,
                         .alternatives = std::move(alternatives)});
            }
        }
    }
    return result;
}

} // namespace verosim
