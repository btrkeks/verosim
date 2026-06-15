#pragma once

#include <map>
#include <string>
#include <vector>

#include "verosim/engine/edit_op.h"
#include "verosim/model/sym_score.h"

namespace verosim {

struct CompareResult {
    std::vector<EditOp> op_list; // ops point into pred/gt — they must outlive this
    long cost = 0;
};

// Comparison.annotated_scores_diff (comparison.py:1578-1674) at v1 tiers:
// pred = score1/original, gt = score2/compare_to (the oracle convention).
CompareResult CompareScores(const SymScore &pred, const SymScore &gt);

// Visualization.get_edit_distances_dict (visualization.py:3182-3209): op
// names rewritten by extra kind, bucketed under musicdiff's header names,
// plus the always-present 'bad kern syntax OMR-ED' (0 here — D4 deferred).
std::map<std::string, long> EditDistancesDict(const std::vector<EditOp> &ops);
std::string EditOpCategory(const EditOp &op);

// Visualization.get_omr_ned (visualization.py:3167-3179).
double OmrNed(long cost, long n_pred, long n_gt);

} // namespace verosim
