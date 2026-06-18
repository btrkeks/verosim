#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "verosim/visual/visual_mark.h"

namespace verosim {

struct SvgAnnotationResult {
    std::string svg;
    std::vector<bool> resolved;
    bool parse_ok = true;
    std::string error;
};

SvgAnnotationResult AnnotateSvg(
    const std::string &svg, const std::vector<ResolvedVisualMark> &marks, std::size_t source_count);

} // namespace verosim
