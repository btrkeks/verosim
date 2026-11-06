#pragma once

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "verosim/engine/edit_op.h"
#include "verosim/model/sym_score.h"

namespace verosim {

struct AlignmentAlternative {
    std::string action;
    OpSide prediction;
    OpSide ground_truth;
    std::string rejection;
    std::optional<long> local_cost;
    std::optional<long> total_cost;
    bool preferred = false;
    std::vector<OpSide> prediction_group;
    std::vector<OpSide> ground_truth_group;
};

struct AlignmentDecision {
    OpSide prediction;
    OpSide ground_truth;
    std::string algorithm;
    std::string reason;
    long local_cost = 0;
    std::optional<long> total_cost;
    std::vector<AlignmentAlternative> alternatives;
    std::vector<OpSide> prediction_group;
    std::vector<OpSide> ground_truth_group;
};

// References have the same lifetime as EditOp sides: the compared scores must
// outlive the trace. Costs at different hierarchy levels must not be summed.
struct AlignmentTrace {
    std::set<int> detailed_measures;
    std::vector<AlignmentDecision> decisions;

    bool WantsDetails(const SymMeasure &prediction, const SymMeasure &ground_truth) const;
    void RecordUnmatched(const OpSide &symbol, bool prediction, const std::string &reason,
        std::vector<AlignmentAlternative> alternatives = {},
        std::optional<long> total_cost = std::nullopt);
    void RecordEqualMeasure(const SymMeasure &prediction, const SymMeasure &ground_truth,
        const std::string &reason, std::vector<AlignmentAlternative> alternatives = {},
        std::optional<long> total_cost = std::nullopt);
};

} // namespace verosim
