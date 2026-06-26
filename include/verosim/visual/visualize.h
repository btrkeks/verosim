#pragma once

#include <string>

#include "verosim/app/compare_options.h"
#include "verosim/visual/visual_report.h"

namespace verosim {

bool BuildVisualComparison(const std::string &pred_path, const std::string &gt_path,
    const CompareRunOptions &options, VisualReport &report, std::string &error);

bool VisualizePairToHtml(const std::string &pred_path, const std::string &gt_path,
    const std::string &out_path, const CompareRunOptions &options, std::string &error);

} // namespace verosim
