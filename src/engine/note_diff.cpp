#include "verosim/engine/note_diff.h"

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <vector>

// All recursive suffix Levenshteins in comparison.py are ported as bottom-up
// DP over suffix indices (i, j). Equivalence: the Python memo keys are reprs
// embedding unique object ids, so within one call they are a bijection of
// (i, j); each cell's choice depends only on child cells. Tie-break: Python's
// min() over an insertion-ordered dict returns the FIRST minimal key, so
// candidates are tried in source order (del, ins, edit) and replaced only on
// strictly smaller cost.
//
// Op-list order: the recursion appends each step's op AFTER the recursive
// result, so the final list runs back-to-front over the alignment. The
// backtrack below collects per-step op groups walking forward from (0, 0)
// and emits the groups reversed — byte-order-identical to Python, which the
// triage workflow (diffing op lists against stored oracle edit_ops) relies on.
//
// Cost-only mode: when only the scalar cost is needed (BlockDiffLin DP fill),
// FillDpCostOnly skips the choice array and returns dp.CostAt(0,0) directly,
// avoiding the Backtrack pass and all EditOp vector allocations.

namespace verosim {

namespace {

enum class Choice : std::uint8_t { kDel, kIns, kEdit };

struct DpTable {
    int m = 0, n = 0;
    std::vector<long> cost; // (m+1) x (n+1)
    std::vector<Choice> choice;

    long &CostAt(int i, int j) { return cost[static_cast<std::size_t>(i) * (n + 1) + j]; }
    Choice &ChoiceAt(int i, int j) { return choice[static_cast<std::size_t>(i) * (n + 1) + j]; }
};

// Fills the suffix DP for a Levenshtein with per-element del/ins sizes and an
// edit cost callback. Boundaries are the forced ins/del chains of the Python
// base cases.
template <typename DelSize, typename InsSize, typename EditCost>
DpTable FillDp(int m, int n, DelSize del_size, InsSize ins_size, EditCost edit_cost)
{
    DpTable dp;
    dp.m = m;
    dp.n = n;
    dp.cost.resize(static_cast<std::size_t>(m + 1) * (n + 1), 0);
    dp.choice.resize(static_cast<std::size_t>(m + 1) * (n + 1), Choice::kEdit);

    for (int i = m - 1; i >= 0; --i) {
        dp.CostAt(i, n) = dp.CostAt(i + 1, n) + del_size(i);
        dp.ChoiceAt(i, n) = Choice::kDel;
    }
    for (int j = n - 1; j >= 0; --j) {
        dp.CostAt(m, j) = dp.CostAt(m, j + 1) + ins_size(j);
        dp.ChoiceAt(m, j) = Choice::kIns;
    }
    for (int i = m - 1; i >= 0; --i) {
        for (int j = n - 1; j >= 0; --j) {
            long best = dp.CostAt(i + 1, j) + del_size(i); // del first (tie winner)
            Choice pick = Choice::kDel;
            const long ins = dp.CostAt(i, j + 1) + ins_size(j);
            if (ins < best) {
                best = ins;
                pick = Choice::kIns;
            }
            const long edit = dp.CostAt(i + 1, j + 1) + edit_cost(i, j);
            if (edit < best) {
                best = edit;
                pick = Choice::kEdit;
            }
            dp.CostAt(i, j) = best;
            dp.ChoiceAt(i, j) = pick;
        }
    }
    return dp;
}

// Cost-only DP fill: same recurrence as FillDp but stores only the cost
// table, not the choice array. Returns the optimal alignment cost directly.
template <typename DelSize, typename InsSize, typename EditCost>
long FillDpCostOnly(int m, int n, DelSize del_size, InsSize ins_size, EditCost edit_cost)
{
    std::vector<long> cost(static_cast<std::size_t>(m + 1) * (n + 1), 0);
    const auto cost_at = [&](int i, int j) -> long & {
        return cost[static_cast<std::size_t>(i) * (n + 1) + j];
    };

    for (int i = m - 1; i >= 0; --i)
        cost_at(i, n) = cost_at(i + 1, n) + del_size(i);
    for (int j = n - 1; j >= 0; --j)
        cost_at(m, j) = cost_at(m, j + 1) + ins_size(j);
    for (int i = m - 1; i >= 0; --i) {
        for (int j = n - 1; j >= 0; --j) {
            long best = cost_at(i + 1, j) + del_size(i);
            const long ins = cost_at(i, j + 1) + ins_size(j);
            if (ins < best) best = ins;
            const long edit = cost_at(i + 1, j + 1) + edit_cost(i, j);
            if (edit < best) best = edit;
            cost_at(i, j) = best;
        }
    }
    return cost_at(0, 0);
}

long PitchesLevenshteinDiffCost(const SymNote &n1, const SymNote &n2)
{
    const std::vector<SymPitch> &orig = n1.pitches;
    const std::vector<SymPitch> &comp = n2.pitches;
    return FillDpCostOnly(
        static_cast<int>(orig.size()), static_cast<int>(comp.size()),
        [&](int i) { return static_cast<long>(PitchSize(orig[i])); },
        [&](int j) { return static_cast<long>(PitchSize(comp[j])); },
        [&](int i, int j) {
            return PitchEq(orig[i], comp[j]) ? 0
                                             : PitchesDiff(orig[i], comp[j], &n1, &n2, i, j).cost;
        });
}

template <typename T>
long BeamTupletLevenshteinDiffCost(const std::vector<T> &orig, const std::vector<T> &comp)
{
    return FillDpCostOnly(
        static_cast<int>(orig.size()), static_cast<int>(comp.size()),
        [](int) { return 1L; }, [](int) { return 1L; },
        [&](int i, int j) { return orig[i] == comp[j] ? 0L : 1L; });
}

long GenericLevenshteinDiffCost(
    const std::vector<std::string> &orig, const std::vector<std::string> &comp)
{
    return FillDpCostOnly(
        static_cast<int>(orig.size()), static_cast<int>(comp.size()),
        [](int) { return 1L; }, [](int) { return 1L; },
        [&](int i, int j) { return orig[i] == comp[j] ? 0L : 1L; });
}

// Walks the DP path from (0, 0), collecting one op group per step via
// emit_group(choice, i, j, group&), then emits groups in reverse step order.
template <typename EmitGroup>
DiffResult Backtrack(DpTable &dp, EmitGroup emit_group)
{
    std::vector<std::vector<EditOp>> groups;
    int i = 0, j = 0;
    while (i < dp.m || j < dp.n) {
        const Choice choice = dp.ChoiceAt(i, j);
        emit_group(choice, i, j, groups.emplace_back());
        switch (choice) {
            case Choice::kDel: ++i; break;
            case Choice::kIns: ++j; break;
            case Choice::kEdit:
                ++i;
                ++j;
                break;
        }
    }
    DiffResult result;
    result.cost = dp.CostAt(0, 0);
    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        result.ops.insert(result.ops.end(), it->begin(), it->end());
    }
    return result;
}

EditOp PitchOp(OpName name, const SymNote *n1, const SymNote *n2, long cost, int i, int j)
{
    return EditOp{ .name = name,
        .a = OpSide::Note(n1),
        .b = OpSide::Note(n2),
        .cost = cost,
        .ids_kind = EditOp::IdsKind::kPitchPair,
        .ids0 = i,
        .ids1 = j };
}

// Comparison._pitches_levenshtein_diff (comparison.py:278-352) over the two
// notes' pitch lists, initial ids (0, 0). At v1 tiers both lists have exactly
// one element (chords are pre-split), so ins/del branches are unreachable in
// practice; ported in full regardless.
DiffResult PitchesLevenshteinDiff(const SymNote &n1, const SymNote &n2)
{
    const std::vector<SymPitch> &orig = n1.pitches;
    const std::vector<SymPitch> &comp = n2.pitches;
    const int m = static_cast<int>(orig.size());
    const int n = static_cast<int>(comp.size());

    DpTable dp = FillDp(
        m, n, [&](int i) { return static_cast<long>(PitchSize(orig[i])); },
        [&](int j) { return static_cast<long>(PitchSize(comp[j])); },
        [&](int i, int j) {
            return PitchEq(orig[i], comp[j]) ? 0
                                             : PitchesDiff(orig[i], comp[j], &n1, &n2, i, j).cost;
        });

    return Backtrack(dp, [&](Choice choice, int i, int j, std::vector<EditOp> &group) {
        switch (choice) {
            case Choice::kDel:
                group.push_back(PitchOp(OpName::kDelPitch, &n1, &n2, PitchSize(orig[i]), i, j));
                break;
            case Choice::kIns:
                group.push_back(PitchOp(OpName::kInsPitch, &n1, &n2, PitchSize(comp[j]), i, j));
                break;
            case Choice::kEdit:
                if (!PitchEq(orig[i], comp[j])) {
                    group = PitchesDiff(orig[i], comp[j], &n1, &n2, i, j).ops;
                }
                break;
        }
    });
}

// Comparison._beamtuplet_levenshtein_diff (comparison.py:1009-1070): unit
// ins/del/edit costs over beaming/tuplet/tuplet_info sequences. which selects
// the op-name family ("beam" or "tuplet" — tuplet_info also uses "tuplet").
template <typename T>
DiffResult BeamTupletLevenshteinDiff(const std::vector<T> &orig, const std::vector<T> &comp,
    const SymNote &n1, const SymNote &n2, bool beam)
{
    const int m = static_cast<int>(orig.size());
    const int n = static_cast<int>(comp.size());

    DpTable dp = FillDp(
        m, n, [](int) { return 1L; }, [](int) { return 1L; },
        [&](int i, int j) { return orig[i] == comp[j] ? 0L : 1L; });

    const OpName del = beam ? OpName::kDelBeam : OpName::kDelTuplet;
    const OpName ins = beam ? OpName::kInsBeam : OpName::kInsTuplet;
    const OpName edit = beam ? OpName::kEditBeam : OpName::kEditTuplet;
    const auto unit_op = [&](OpName name) {
        return EditOp{ .name = name, .a = OpSide::Note(&n1), .b = OpSide::Note(&n2), .cost = 1 };
    };

    return Backtrack(dp, [&](Choice choice, int i, int j, std::vector<EditOp> &group) {
        switch (choice) {
            case Choice::kDel: group.push_back(unit_op(del)); break;
            case Choice::kIns: group.push_back(unit_op(ins)); break;
            case Choice::kEdit:
                if (!(orig[i] == comp[j])) group.push_back(unit_op(edit));
                break;
        }
    });
}

DiffResult GenericLevenshteinDiff(const std::vector<std::string> &orig,
    const std::vector<std::string> &comp, const SymNote &n1, const SymNote &n2,
    OpName del, OpName ins, OpName edit)
{
    const int m = static_cast<int>(orig.size());
    const int n = static_cast<int>(comp.size());

    DpTable dp = FillDp(
        m, n, [](int) { return 1L; }, [](int) { return 1L; },
        [&](int i, int j) { return orig[i] == comp[j] ? 0L : 1L; });

    const auto unit_op = [&](OpName name) {
        return EditOp{ .name = name, .a = OpSide::Note(&n1), .b = OpSide::Note(&n2), .cost = 1 };
    };

    return Backtrack(dp, [&](Choice choice, int i, int j, std::vector<EditOp> &group) {
        switch (choice) {
            case Choice::kDel: group.push_back(unit_op(del)); break;
            case Choice::kIns: group.push_back(unit_op(ins)); break;
            case Choice::kEdit:
                if (orig[i] != comp[j]) group.push_back(unit_op(edit));
                break;
        }
    });
}

void Extend(DiffResult &into, DiffResult &&part)
{
    into.cost += part.cost;
    into.ops.insert(into.ops.end(), std::make_move_iterator(part.ops.begin()),
        std::make_move_iterator(part.ops.end()));
}

} // namespace

DiffResult PitchesDiff(
    const SymPitch &p1, const SymPitch &p2, const SymNote *n1, const SymNote *n2, int id1, int id2)
{
    DiffResult result;
    // pitch name
    if (p1.step_octave != p2.step_octave) {
        result.cost += 1;
        const bool rest1 = !p1.step_octave.empty() && p1.step_octave[0] == 'R';
        const bool rest2 = !p2.step_octave.empty() && p2.step_octave[0] == 'R';
        result.ops.push_back(PitchOp(
            rest1 != rest2 ? OpName::kPitchTypeEdit : OpName::kPitchNameEdit, n1, n2, 1, id1, id2));
    }
    // accidental
    if (p1.accid != p2.accid) {
        if (p1.accid == "None") {
            result.cost += 1;
            result.ops.push_back(PitchOp(OpName::kAccidentIns, n1, n2, 1, id1, id2));
        }
        else if (p2.accid == "None") {
            result.cost += 1;
            result.ops.push_back(PitchOp(OpName::kAccidentDel, n1, n2, 1, id1, id2));
        }
        else { // different alteration: delete then add
            result.cost += 2;
            result.ops.push_back(PitchOp(OpName::kAccidentEdit, n1, n2, 2, id1, id2));
        }
    }
    // tie
    if (p1.tie != p2.tie) {
        result.cost += 1;
        result.ops.push_back(
            PitchOp(p1.tie ? OpName::kTieDel : OpName::kTieIns, n1, n2, 1, id1, id2));
    }
    return result;
}

DiffResult AnnotatedNoteDiff(const SymNote &n1, const SymNote &n2)
{
    DiffResult result;
    const auto note_op = [&](OpName name, long cost) {
        return EditOp{ .name = name, .a = OpSide::Note(&n1), .b = OpSide::Note(&n2), .cost = cost };
    };

    // pitches (Levenshtein; lists are already ordered)
    const bool pitches_equal = n1.pitches.size() == n2.pitches.size()
        && std::equal(n1.pitches.begin(), n1.pitches.end(), n2.pitches.begin(), PitchEq);
    if (!pitches_equal) {
        Extend(result, PitchesLevenshteinDiff(n1, n2));
    }
    // note head: delete one, add the other
    if (n1.note_head != n2.note_head) {
        result.cost += 2;
        result.ops.push_back(note_op(OpName::kHeadEdit, 2));
    }
    // dots: one per added/deleted dot
    if (n1.dots != n2.dots) {
        const long dots_diff = std::abs(n1.dots - n2.dots);
        result.cost += dots_diff;
        result.ops.push_back(
            note_op(n1.dots > n2.dots ? OpName::kDotDel : OpName::kDotIns, dots_diff));
    }
    // graceness: delete the wrong, add the right
    if (n1.grace_type != n2.grace_type) {
        result.cost += 2;
        result.ops.push_back(note_op(OpName::kGraceEdit, 2));
    }
    if (n1.grace_slash != n2.grace_slash) {
        result.cost += 1;
        result.ops.push_back(note_op(OpName::kGraceSlashEdit, 1));
    }
    // beamings / tuplets / tuplet info (the latter also under the tuplet ops)
    if (n1.beamings != n2.beamings) {
        Extend(result, BeamTupletLevenshteinDiff(n1.beamings, n2.beamings, n1, n2, /*beam=*/true));
    }
    if (n1.tuplets != n2.tuplets) {
        Extend(result, BeamTupletLevenshteinDiff(n1.tuplets, n2.tuplets, n1, n2, /*beam=*/false));
    }
    if (n1.tuplet_info != n2.tuplet_info) {
        Extend(result,
            BeamTupletLevenshteinDiff(n1.tuplet_info, n2.tuplet_info, n1, n2, /*beam=*/false));
    }
    // articulations / expressions: same generic unit-cost Levenshtein as
    // musicdiff._generic_levenshtein_diff.
    if (n1.articulations != n2.articulations) {
        Extend(result, GenericLevenshteinDiff(n1.articulations, n2.articulations, n1, n2,
                           OpName::kDelArticulation, OpName::kInsArticulation,
                           OpName::kEditArticulation));
    }
    if (n1.expressions != n2.expressions) {
        Extend(result, GenericLevenshteinDiff(n1.expressions, n2.expressions, n1, n2,
                           OpName::kDelExpression, OpName::kInsExpression,
                           OpName::kEditExpression));
    }

    // gap from previous note (horizontal position shift); always 0 in flat
    // (non-voicing) mode, ported for completeness.
    if (n1.gap_dur != n2.gap_dur) {
        result.cost += 1;
        OpName name = OpName::kEditSpace;
        if (n1.gap_dur == Fraction(0)) name = OpName::kInsSpace;
        else if (n2.gap_dur == Fraction(0)) name = OpName::kDelSpace;
        result.ops.push_back(note_op(name, 1));
    }
    // noteshape / noteheadFill / noteheadParenthesis / stemDirection /
    // styledict: Style-gated (Tier D), constant-equal at v1 tiers.
    return result;
}

long AnnotatedNoteDiffCost(const SymNote &n1, const SymNote &n2)
{
    long cost = 0;

    const bool pitches_equal = n1.pitches.size() == n2.pitches.size()
        && std::equal(n1.pitches.begin(), n1.pitches.end(), n2.pitches.begin(), PitchEq);
    if (!pitches_equal) {
        cost += PitchesLevenshteinDiffCost(n1, n2);
    }
    if (n1.note_head != n2.note_head) cost += 2;
    if (n1.dots != n2.dots) cost += std::abs(n1.dots - n2.dots);
    if (n1.grace_type != n2.grace_type) cost += 2;
    if (n1.grace_slash != n2.grace_slash) cost += 1;
    if (n1.beamings != n2.beamings) {
        cost += BeamTupletLevenshteinDiffCost(n1.beamings, n2.beamings);
    }
    if (n1.tuplets != n2.tuplets) {
        cost += BeamTupletLevenshteinDiffCost(n1.tuplets, n2.tuplets);
    }
    if (n1.tuplet_info != n2.tuplet_info) {
        cost += BeamTupletLevenshteinDiffCost(n1.tuplet_info, n2.tuplet_info);
    }
    if (n1.articulations != n2.articulations) {
        cost += GenericLevenshteinDiffCost(n1.articulations, n2.articulations);
    }
    if (n1.expressions != n2.expressions) {
        cost += GenericLevenshteinDiffCost(n1.expressions, n2.expressions);
    }
    if (n1.gap_dur != n2.gap_dur) cost += 1;
    return cost;
}

} // namespace verosim
