#include "verosim/app/evaluation_audit.h"

#include <ostream>

#include "verosim/support/json_util.h"

namespace verosim {

EvaluationAudit MakeEvaluationAudit(const EvaluationProfile &profile)
{
    EvaluationAudit audit;
    audit.profile_id = profile.id();
    audit.profile_digest = profile.digest();
    return audit;
}

EvaluationAudit AuditAvailableScores(const EvaluationProfile &profile,
    const SymScore *pred, const SymScore *gt)
{
    EvaluationAudit audit = MakeEvaluationAudit(profile);
    if (pred) audit.pred_removed = ProjectSymScore(*pred, profile).removed;
    if (gt) audit.gt_removed = ProjectSymScore(*gt, profile).removed;
    return audit;
}

void WriteEvaluationFeatureCountsJson(
    const std::optional<EvaluationFeatureCounts> &counts, std::ostream &out)
{
    if (!counts.has_value()) {
        out << "null";
        return;
    }
    out << "{\"total\":" << counts->total() << ",\"features\":{";
    bool first = true;
    for (const EvaluationFeatureSpec &spec : EvaluationFeatureRegistryV1()) {
        if (!first) out << ",";
        first = false;
        WriteJsonString(std::string(spec.key), out);
        out << ":" << (*counts)[spec.feature];
    }
    out << "}}";
}

void WriteEvaluationProfileIdentityJson(const EvaluationAudit &audit, std::ostream &out)
{
    out << "{\"schema\":";
    WriteJsonString(audit.schema, out);
    out << ",\"schema_version\":" << audit.schema_version << ",\"id\":";
    WriteJsonString(audit.profile_id, out);
    out << ",\"digest\":";
    WriteJsonString(audit.profile_digest, out);
    out << "}";
}

void WriteEvaluationAuditJson(const EvaluationAudit &audit, std::ostream &out)
{
    out << "{\"schema\":";
    WriteJsonString(audit.schema, out);
    out << ",\"schema_version\":" << audit.schema_version << ",\"id\":";
    WriteJsonString(audit.profile_id, out);
    out << ",\"digest\":";
    WriteJsonString(audit.profile_digest, out);
    out << ",\"removed\":{\"pred\":";
    WriteEvaluationFeatureCountsJson(audit.pred_removed, out);
    out << ",\"gt\":";
    WriteEvaluationFeatureCountsJson(audit.gt_removed, out);
    out << "}}";
}

} // namespace verosim
