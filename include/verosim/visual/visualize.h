#pragma once

#include <string>

#include "verosim/cli/compare_cli.h"

namespace verosim {

bool VisualizePairToHtml(const std::string &pred_path, const std::string &gt_path,
    const std::string &out_path, const CompareCliOptions &options, std::string &error);

} // namespace verosim
