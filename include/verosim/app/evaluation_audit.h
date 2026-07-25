#pragma once

#include <optional>
#include <iosfwd>
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
EvaluationAudit AuditAvailableScores(const EvaluationProfile &profile,
    const SymScore *pred, const SymScore *gt);

// Deterministic registry-order serializers shared by CLI, summary, and visual
// outputs. The identity form omits per-side counts for batch summary headers.
void WriteEvaluationFeatureCountsJson(
    const std::optional<EvaluationFeatureCounts> &counts, std::ostream &out);
void WriteEvaluationProfileIdentityJson(const EvaluationAudit &audit, std::ostream &out);
void WriteEvaluationAuditJson(const EvaluationAudit &audit, std::ostream &out);

} // namespace verosim
