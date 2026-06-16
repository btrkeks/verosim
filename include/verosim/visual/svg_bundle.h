#pragma once

#include <string>
#include <vector>

#include "verosim/visual/visual_report.h"

namespace verosim {

struct SvgBundlePage {
    int page = 1;
    std::string path;
};

struct SvgBundleSide {
    std::vector<SvgBundlePage> pages;
};

struct SvgAssetBundle {
    SvgBundleSide prediction;
    SvgBundleSide ground_truth;
    std::string manifest_path;
};

bool WriteSvgAssetBundle(
    const VisualReport &report, const std::string &out_dir, SvgAssetBundle &bundle, std::string &error);

} // namespace verosim
