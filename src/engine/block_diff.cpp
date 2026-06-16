#include "verosim/engine/block_diff.h"

#include <cstdint>
#include <iterator>
#include <vector>

#include "verosim/engine/set_distance.h"

namespace verosim {

namespace {

enum class Choice : std::uint8_t { kDelBar, kInsBar, kEditBar };

// editbar inside diff (comparison.py:444-486): notes set distance then extras
// set distance, ops in that order (notes first; lyrics are excluded in v1).
DiffResult InsideBarDiff(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep,
    const CompareOptions &options)
{
    DiffResult result = NotesSetDistance(orig, comp, orig_prep, comp_prep, options);
    DiffResult extras = ExtrasSetDistance(orig, comp, orig_prep, comp_prep);
    result.cost += extras.cost;
    result.ops.insert(result.ops.end(), std::make_move_iterator(extras.ops.begin()),
        std::make_move_iterator(extras.ops.end()));
    return result;
}

// Cost-only inside-bar diff: same pairing and edit-distance logic as
// InsideBarDiff but skips all EditOp construction.
long InsideBarCost(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep,
    const CompareOptions &options)
{
    return NotesSetDistanceCost(orig, comp, orig_prep, comp_prep, options)
        + ExtrasSetDistanceCost(orig, comp, orig_prep, comp_prep);
}

} // namespace

DiffResult BlockDiffLin(const SymPart &orig_part, const PreparedPart &orig_prep,
    const SymPart &comp_part, const PreparedPart &comp_prep, const NcsBlock &block,
    const CompareOptions &options)
{
    // Bottom-up suffix DP replacing the memoized recursion; the Python memo
    // keys (reprs with unique measure ids) are a bijection of (i, j) within
    // one block, so this is exactly equivalent — see compare.cpp for the
    // memoization rationale.
    //
    // The DP fill uses cost-only variants of the inside-bar diff (no EditOp
    // vectors allocated). The full op lists are built only during backtracking
    // along the optimal path.
    const std::vector<int> &orig_idx = block.original;
    const std::vector<int> &comp_idx = block.compare_to;
    const int m = static_cast<int>(orig_idx.size());
    const int n = static_cast<int>(comp_idx.size());

    const auto orig_measure = [&](int i) -> const SymMeasure & {
        return orig_part.bar_list[orig_idx[i]];
    };
    const auto comp_measure = [&](int j) -> const SymMeasure & {
        return comp_part.bar_list[comp_idx[j]];
    };
    const auto orig_pm = [&](int i) -> const PreparedMeasure & {
        return orig_prep.measures[orig_idx[i]];
    };
    const auto comp_pm = [&](int j) -> const PreparedMeasure & {
        return comp_prep.measures[comp_idx[j]];
    };

    const std::size_t table_size = static_cast<std::size_t>(m + 1) * (n + 1);
    std::vector<long> cost(table_size, 0);
    std::vector<Choice> choice(table_size, Choice::kEditBar);
    const auto cost_at = [&](int i, int j) -> long & {
        return cost[static_cast<std::size_t>(i) * (n + 1) + j];
    };
    const auto choice_at = [&](int i, int j) -> Choice & {
        return choice[static_cast<std::size_t>(i) * (n + 1) + j];
    };

    for (int i = m - 1; i >= 0; --i) {
        cost_at(i, n) = cost_at(i + 1, n) + orig_pm(i).notation_size;
        choice_at(i, n) = Choice::kDelBar;
    }
    for (int j = n - 1; j >= 0; --j) {
        cost_at(m, j) = cost_at(m, j + 1) + comp_pm(j).notation_size;
        choice_at(m, j) = Choice::kInsBar;
    }
    for (int i = m - 1; i >= 0; --i) {
        for (int j = n - 1; j >= 0; --j) {
            // candidate order = Python dict insertion order (delbar, insbar,
            // editbar); replace only on strictly smaller cost (min() returns
            // the first minimal key).
            long best = cost_at(i + 1, j) + orig_pm(i).notation_size;
            Choice pick = Choice::kDelBar;
            const long ins = cost_at(i, j + 1) + comp_pm(j).notation_size;
            if (ins < best) {
                best = ins;
                pick = Choice::kInsBar;
            }
            long inside = 0;
            if (orig_pm(i).content_id != comp_pm(j).content_id) {
                inside = InsideBarCost(
                    orig_measure(i), comp_measure(j), orig_pm(i), comp_pm(j), options);
            }
            const long edit = cost_at(i + 1, j + 1) + inside;
            if (edit < best) {
                best = edit;
                pick = Choice::kEditBar;
            }
            cost_at(i, j) = best;
            choice_at(i, j) = pick;
        }
    }

    // Backtrack: collect one op group per step walking forward, emit groups
    // reversed (the recursion appends each step's op after the recursive
    // result, so the Python list runs back-to-front over the alignment).
    // Only edit-bar cells on the chosen path need the full InsideBarDiff.
    std::vector<std::vector<EditOp>> groups;
    int i = 0, j = 0;
    while (i < m || j < n) {
        std::vector<EditOp> &group = groups.emplace_back();
        switch (choice_at(i, j)) {
            case Choice::kDelBar:
                group.push_back(EditOp{ .name = OpName::kDelBar,
                    .a = OpSide::Measure(&orig_measure(i)),
                    .b = OpSide::None(),
                    .cost = orig_pm(i).notation_size });
                ++i;
                break;
            case Choice::kInsBar:
                group.push_back(EditOp{ .name = OpName::kInsBar,
                    .a = OpSide::None(),
                    .b = OpSide::Measure(&comp_measure(j)),
                    .cost = comp_pm(j).notation_size });
                ++j;
                break;
            case Choice::kEditBar:
                if (orig_pm(i).content_id != comp_pm(j).content_id) {
                    group = InsideBarDiff(orig_measure(i), comp_measure(j), orig_pm(i), comp_pm(j), options)
                                .ops;
                }
                ++i;
                ++j;
                break;
        }
    }

    DiffResult result;
    result.cost = cost_at(0, 0);
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        result.ops.insert(result.ops.end(), it->begin(), it->end());
    }
    return result;
}

} // namespace verosim
