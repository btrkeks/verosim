#pragma once

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "verosim/app/compare_options.h"
#include "verosim/app/evaluation_audit.h"
#include "verosim/app/score_format.h"
#include "verosim/engine/compare.h"
#include "verosim/extraction/extract.h"
#include "verosim/model/sym_score.h"

namespace verosim {

class VrvBridge;

struct LoadedScore {
    SymScore score;
    std::vector<std::string> warnings;
    std::string error;
    ScoreDataFormat format = ScoreDataFormat::kAuto;
    std::string parser = "verovio";
    bool ok = false;
};

struct ProjectedScorePair {
    SymScore pred;
    SymScore gt;
};

struct ScoreComparison {
    ScoreComparison() = default;
    ScoreComparison(const ScoreComparison &) = delete;
    ScoreComparison &operator=(const ScoreComparison &) = delete;
    ScoreComparison(ScoreComparison &&) noexcept = default;
    ScoreComparison &operator=(ScoreComparison &&) noexcept = default;

    // In profile mode, owns the canonical projected inputs so pointers held by
    // diff remain valid across return and moves. Null in the legacy path,
    // where diff points into the caller-owned input scores as before.
    std::unique_ptr<ProjectedScorePair> projected_scores;
    CompareResult diff;
    long n_pred = 0;
    long n_gt = 0;
    double omr_ned = 0.0;
    std::map<std::string, long> edit_distances;
    std::optional<EvaluationAudit> evaluation_audit;
};

ExtractOptions ExtractOptionsForRun(const CompareRunOptions &options);
CompareOptions EngineOptionsForRun(const CompareRunOptions &options);

bool HumdrumTimingIsClean(VrvBridge &bridge, const std::string &data, ScoreDataFormat format);

LoadedScore LoadAndExtractScoreFile(
    VrvBridge &bridge, const std::string &path, const ExtractOptions &options);
LoadedScore LoadAndExtractKernData(VrvBridge &bridge, const std::string &data,
    const std::string &label, const ExtractOptions &options);
LoadedScore LoadAndExtractScoreData(VrvBridge &bridge, const std::string &data,
    ScoreDataFormat format, const std::string &label, const ExtractOptions &options);

ScoreComparison CompareLoadedScores(
    const SymScore &pred, const SymScore &gt, const CompareRunOptions &options);

void AppendPrefixedWarnings(std::vector<std::string> &out, const std::string &prefix,
    const std::vector<std::string> &warnings);

} // namespace verosim
