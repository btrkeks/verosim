#pragma once

#include <map>
#include <string>
#include <vector>

#include "verosim/app/compare_options.h"
#include "verosim/engine/compare.h"
#include "verosim/extraction/extract.h"
#include "verosim/model/sym_score.h"

namespace verosim {

class VrvBridge;

struct LoadedScore {
    SymScore score;
    std::vector<std::string> warnings;
    std::string error;
    bool ok = false;
};

struct ScoreComparison {
    CompareResult diff; // ops point into the input scores passed to CompareLoadedScores
    long n_pred = 0;
    long n_gt = 0;
    double omr_ned = 0.0;
    std::map<std::string, long> edit_distances;
};

ExtractOptions ExtractOptionsForRun(const CompareRunOptions &options);
CompareOptions EngineOptionsForRun(const CompareRunOptions &options);

LoadedScore LoadAndExtractScoreFile(
    VrvBridge &bridge, const std::string &path, const ExtractOptions &options);
LoadedScore LoadAndExtractKernData(VrvBridge &bridge, const std::string &data,
    const std::string &label, const ExtractOptions &options);

ScoreComparison CompareLoadedScores(
    const SymScore &pred, const SymScore &gt, const CompareRunOptions &options);

void AppendPrefixedWarnings(std::vector<std::string> &out, const std::string &prefix,
    const std::vector<std::string> &warnings);

} // namespace verosim
