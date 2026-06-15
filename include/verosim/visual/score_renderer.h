#pragma once

#include <string>
#include <vector>

namespace verosim {

class VrvBridge;

struct RenderedPage {
    int page_no = 1;
    std::string svg;
};

struct RenderedScore {
    std::vector<RenderedPage> pages;
    std::vector<std::string> warnings;
};

bool ConfigureScoreRenderOptions(
    VrvBridge &bridge, bool include_bounding_boxes, std::string &error);

bool RenderScoreToSvgPages(VrvBridge &bridge, RenderedScore &rendered, std::string &error);

} // namespace verosim
