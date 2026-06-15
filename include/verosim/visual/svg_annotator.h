#pragma once

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

SvgAnnotationResult AnnotateSvg(const std::string &svg, const std::vector<VisualMark> &marks);

} // namespace verosim
