#include "verosim/visual/score_renderer.h"

#include <sstream>

#include "verosim/extraction/vrv_bridge.h"
#include "verosim/visual/svg_overlap.h"

#include <pugixml.hpp>

namespace verosim {
namespace {

std::string RenderOptionsJson(bool include_bounding_boxes, const std::string &breaks)
{
    std::ostringstream out;
    out << R"({"breaks":")" << breaks
        << R"(","header":"none","footer":"none","adjustPageHeight":true,"adjustPageWidth":true,"svgViewBox":true,"pageWidth":2400,"noJustification":true)";
    if (include_bounding_boxes) {
        out << R"(,"svgBoundingBoxes":true,"svgContentBoundingBoxes":true})";
    }
    else {
        out << R"(,"svgBoundingBoxes":false,"svgContentBoundingBoxes":false})";
    }
    return out.str();
}

bool HasClassToken(const std::string &classes, const std::string &token)
{
    std::size_t pos = 0;
    while (pos < classes.size()) {
        while (pos < classes.size() && classes[pos] == ' ') ++pos;
        const std::size_t start = pos;
        while (pos < classes.size() && classes[pos] != ' ') ++pos;
        if (classes.substr(start, pos - start) == token) return true;
    }
    return false;
}

bool IsRepairSpaceGroup(pugi::xml_node node)
{
    if (std::string(node.name()) != "g") return false;
    const pugi::xml_attribute class_attr = node.attribute("class");
    if (!class_attr) return false;
    const std::string classes = class_attr.value();
    return HasClassToken(classes, "space") && HasClassToken(classes, "straddle");
}

void RemoveRepairSpaceGroups(pugi::xml_node node)
{
    for (pugi::xml_node child = node.first_child(); child;) {
        pugi::xml_node next = child.next_sibling();
        if (child.type() == pugi::node_element && IsRepairSpaceGroup(child)) {
            node.remove_child(child);
        }
        else {
            RemoveRepairSpaceGroups(child);
        }
        child = next;
    }
}

std::string StripRepairSpaceGroupsFromSvg(const std::string &svg)
{
    pugi::xml_document doc;
    const pugi::xml_parse_result parsed = doc.load_string(svg.c_str(), pugi::parse_default);
    if (!parsed) return svg;

    RemoveRepairSpaceGroups(doc);

    std::ostringstream out;
    doc.save(out, "  ", pugi::format_default | pugi::format_no_declaration);
    return out.str();
}

void AppendOverlapWarnings(const std::string &svg, int page_no, std::vector<std::string> &warnings)
{
    const SvgOverlapResult overlap = DetectSvgBBoxOverlaps(svg, page_no);
    if (!overlap.parse_ok) {
        warnings.push_back(
            "page " + std::to_string(page_no) + ": failed to parse SVG bounding boxes: " + overlap.error);
        return;
    }
    if (overlap.summary.overlap_count == 0) return;
    warnings.push_back(FormatSvgOverlapWarning(overlap.summary));
}

} // namespace

bool ConfigureScoreRenderOptions(
    VrvBridge &bridge, bool include_bounding_boxes, std::string &error)
{
    if (!bridge.SetOptions(RenderOptionsJson(include_bounding_boxes, "encoded"))) {
        error = include_bounding_boxes ? "failed to configure Verovio bounding-box rendering options"
                                       : "failed to configure Verovio rendering options";
        return false;
    }
    bridge.RedoLayout();
    return true;
}

bool RenderScoreToSvgPages(VrvBridge &bridge, RenderedScore &rendered, std::string &error)
{
    rendered.pages.clear();
    rendered.warnings.clear();

    if (!ConfigureScoreRenderOptions(bridge, true, error)) return false;

    std::string first_bbox = bridge.RenderToSVG(1, false);
    if (!first_bbox.empty()) {
        AppendOverlapWarnings(first_bbox, 1, rendered.warnings);
    }

    int page_count = bridge.GetPageCount();
    if (page_count < 1) page_count = 1;
    for (int page = 2; page <= page_count; ++page) {
        std::string bbox_svg = bridge.RenderToSVG(page, false);
        if (bbox_svg.empty()) continue;
        AppendOverlapWarnings(bbox_svg, page, rendered.warnings);
    }

    if (!ConfigureScoreRenderOptions(bridge, false, error)) return false;

    std::string first = bridge.RenderToSVG(1, false);
    if (first.empty()) {
        error = "Verovio rendered an empty SVG for page 1";
        return false;
    }
    first = StripRepairSpaceGroupsFromSvg(first);
    rendered.pages.push_back(RenderedPage{ .page_no = 1, .svg = std::move(first) });

    for (int page = 2; page <= page_count; ++page) {
        std::string svg = bridge.RenderToSVG(page, false);
        if (svg.empty()) {
            rendered.warnings.push_back("Verovio rendered an empty SVG for page " + std::to_string(page));
            continue;
        }
        svg = StripRepairSpaceGroupsFromSvg(svg);
        rendered.pages.push_back(RenderedPage{ .page_no = page, .svg = std::move(svg) });
    }
    return true;
}

} // namespace verosim
