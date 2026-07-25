#pragma once

#include <optional>
#include <string>

#include "verosim/model/evaluation_profile.h"

namespace verosim {

// Reproducibility data attached to every comparison when a profile is active.
// Optional sides support load/parse failures that occur before both scores are
// available; successful comparisons always populate both.
struct EvaluationAudit {
    std::string schema = std::string(kEvaluationProfileSchema);
    int schema_version = kEvaluationProfileSchemaVersion;
    std::string profile_id;
    std::string profile_digest;
    std::optional<EvaluationFeatureCounts> pred_removed;
    std::optional<EvaluationFeatureCounts> gt_removed;
};

EvaluationAudit MakeEvaluationAudit(const EvaluationProfile &profile);

} // namespace verosim
