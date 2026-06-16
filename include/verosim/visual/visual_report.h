#pragma once

#include <map>
#include <string>
#include <vector>

#include "verosim/visual/score_renderer.h"
#include "verosim/visual/visual_mark.h"

namespace verosim {

struct VisualizedScore {
    std::string title;
    std::vector<RenderedPage> pages;
};

struct VisualReport {
    std::string pred_path;
    std::string gt_path;
    VisualizedScore pred;
    VisualizedScore gt;
    long distance = 0;
    long n_pred = 0;
    long n_gt = 0;
    double omr_ned = 0.0;
    std::map<std::string, long> edit_distances;
    std::vector<VisualMark> unresolved_marks;
    std::vector<std::string> warnings;
};

} // namespace verosim
