#pragma once

#include <string>
#include <vector>

#include "verosim/visual/visual_mark.h"

namespace verosim {

struct VisualResolveResult {
    std::vector<ResolvedVisualMark> marks;
    std::vector<bool> resolved;
    bool parse_ok = true;
    std::string error;
    int measure_count = 0;
};

VisualResolveResult ResolveVisualMarks(
    const std::string &svg, const std::vector<VisualMark> &marks, int measure_idx_offset = 0);

} // namespace verosim
