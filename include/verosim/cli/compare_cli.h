#pragma once

#include <iosfwd>
#include <string>

#include "verosim/engine/compare.h"
#include "verosim/extraction/typed_space_policy.h"
#include "verosim/model/detail_tier.h"

namespace verosim {

class VrvBridge;

struct CompareCliOptions {
    bool emit_ops = false; // include the per-edit operation list (large)
    DetailTier detail = DetailTier::kTierAB; // default compare surface is Tier A+B
    NotePositionPolicy note_position_policy = NotePositionPolicy::kVisualEventOrder;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
};

// Loads pred and gt, extracts both, runs the comparison engine, and writes
// one JSON object (newline-terminated) whose fields join 1:1 against the
// oracle harness records (harness/verosim_harness/oracle.py:new_record):
// {pair, ok, error, distance, n_pred, n_gt, omr_ned, edit_distances_dict,
//  edit_ops?, warnings, runtime_s}. Returns false when either side fails to
// load (a record with ok=false and error is still written).
bool ComparePairToJson(VrvBridge &bridge, const std::string &pred_path,
    const std::string &gt_path, const CompareCliOptions &options, std::ostream &out);

// Same comparison path as ComparePairToJson, but both sides are loaded from
// in-memory score data. `json_prefix` must contain the opening object and any
// provenance fields already written, e.g. {"id":123,"group":"x"}.
bool CompareScoreDataToJson(VrvBridge &bridge, const std::string &pred_data,
    const std::string &gt_data, const CompareCliOptions &options, const std::string &json_prefix,
    std::ostream &out);

} // namespace verosim
