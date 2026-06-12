#pragma once

#include <iosfwd>
#include <string>

namespace verosim {

class VrvBridge;

struct CompareCliOptions {
    bool emit_ops = false; // include the per-edit operation list (large)
};

// Loads pred and gt, extracts both, runs the comparison engine, and writes
// one JSON object (newline-terminated) whose fields join 1:1 against the
// oracle harness records (harness/verosim_harness/oracle.py:new_record):
// {pair, ok, error, distance, n_pred, n_gt, omr_ned, edit_distances_dict,
//  edit_ops?, warnings, runtime_s}. Returns false when either side fails to
// load (a record with ok=false and error is still written).
bool ComparePairToJson(VrvBridge &bridge, const std::string &pred_path,
    const std::string &gt_path, const CompareCliOptions &options, std::ostream &out);

} // namespace verosim
