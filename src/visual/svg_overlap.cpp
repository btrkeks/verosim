#include "verosim/visual/svg_overlap.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <string>

#include <pugixml.hpp>

namespace verosim {
namespace {

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

bool StartsWith(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string StripTrailingVoiceSuffix(const std::string &object_id)
{
    std::size_t pos = object_id.size();
    while (pos > 0 && std::isdigit(static_cast<unsigned char>(object_id[pos - 1]))) --pos;
    if (pos == object_id.size() || pos == 0 || object_id[pos - 1] != 'S') return "";
    return object_id.substr(0, pos - 1);
}

std::string SourceLineId(const std::string &object_id)
{
    std::size_t pos = object_id.find("-L");
    if (pos == std::string::npos) return "";
    ++pos;
    std::size_t end = pos + 1;
    while (end < object_id.size() && std::isdigit(static_cast<unsigned char>(object_id[end]))) ++end;
    return object_id.substr(pos, end - pos);
}

std::string AncestorIdWithPrefix(pugi::xml_node node, const std::string &prefix)
{
    for (pugi::xml_node parent = node.parent(); parent; parent = parent.parent()) {
        const pugi::xml_attribute id_attr = parent.attribute("id");
        if (id_attr && StartsWith(id_attr.value(), prefix)) return id_attr.value();
    }
    return "";
}

std::string LogicalGroupId(pugi::xml_node group, const std::string &object_id)
{
    const std::string chord_id = AncestorIdWithPrefix(group, "chord-");
    if (!chord_id.empty()) return chord_id;

    const std::string without_voice_suffix = StripTrailingVoiceSuffix(object_id);
    if (!without_voice_suffix.empty()) return without_voice_suffix;

    return "";
}

bool IsTargetKind(const std::string &kind, const SvgOverlapOptions &options)
{
    return std::find(options.target_kinds.begin(), options.target_kinds.end(), kind)
        != options.target_kinds.end();
}

std::optional<SvgBBox> BBoxFromGroup(pugi::xml_node group, const SvgOverlapOptions &options)
{
    const pugi::xml_attribute class_attr = group.attribute("class");
    if (!class_attr) return std::nullopt;
    const std::string classes = class_attr.value();

    const bool is_content = HasClassToken(classes, "content-bounding-box");
    if (!is_content && !HasClassToken(classes, "bounding-box")) return std::nullopt;

    const pugi::xml_attribute id_attr = group.attribute("id");
    if (!id_attr) return std::nullopt;
    const std::string id = id_attr.value();
    const std::string prefix = is_content ? "cbbox-" : "bbox-";
    if (!StartsWith(id, prefix)) return std::nullopt;

    const std::string object_id = id.substr(prefix.size());
    const std::size_t dash = object_id.find('-');
    if (dash == std::string::npos) return std::nullopt;
    const std::string kind = object_id.substr(0, dash);
    if (!IsTargetKind(kind, options)) return std::nullopt;

    double left = std::numeric_limits<double>::infinity();
    double top = std::numeric_limits<double>::infinity();
    double right = -std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();
    bool has_rect = false;

    for (pugi::xml_node rect = group.child("rect"); rect; rect = rect.next_sibling("rect")) {
        const double x = rect.attribute("x").as_double();
        const double y = rect.attribute("y").as_double();
        const double width = rect.attribute("width").as_double();
        const double height = rect.attribute("height").as_double();
        if (width == 0.0 || height == 0.0) continue;
        left = std::min(left, std::min(x, x + width));
        top = std::min(top, std::min(y, y + height));
        right = std::max(right, std::max(x, x + width));
        bottom = std::max(bottom, std::max(y, y + height));
        has_rect = true;
    }

    if (!has_rect || right <= left || bottom <= top) return std::nullopt;

    return SvgBBox{
        .object_id = object_id,
        .group_id = LogicalGroupId(group, object_id),
        .source_id = SourceLineId(object_id),
        .kind = kind,
        .x = left,
        .y = top,
        .width = right - left,
        .height = bottom - top,
        .content = is_content,
    };
}

bool ShouldReplaceBox(const SvgBBox &current, const SvgBBox &candidate)
{
    if (candidate.kind == "note") {
        // Verovio's content box for a note can include stems, flags, and accidentals.
        // The plain note bbox tracks the notehead and is a better proxy for visual collisions.
        return current.content && !candidate.content;
    }

    return candidate.content && !current.content;
}

void CollectBoxes(pugi::xml_node node, const SvgOverlapOptions &options, std::map<std::string, SvgBBox> &boxes)
{
    if (node.type() == pugi::node_element) {
        if (std::optional<SvgBBox> box = BBoxFromGroup(node, options)) {
            auto it = boxes.find(box->object_id);
            if (it == boxes.end() || ShouldReplaceBox(it->second, *box)) {
                boxes[box->object_id] = *box;
            }
        }
    }

    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
        CollectBoxes(child, options, boxes);
    }
}

double Area(const SvgBBox &box)
{
    return box.width * box.height;
}

double OverlapArea(const SvgBBox &a, const SvgBBox &b)
{
    const double x_overlap = std::max(0.0, std::min(a.x + a.width, b.x + b.width) - std::max(a.x, b.x));
    const double y_overlap = std::max(0.0, std::min(a.y + a.height, b.y + b.height) - std::max(a.y, b.y));
    return x_overlap * y_overlap;
}

double CenterX(const SvgBBox &box)
{
    return box.x + box.width / 2.0;
}

double CenterY(const SvgBBox &box)
{
    return box.y + box.height / 2.0;
}

bool IsSharedNoteheadColumn(const SvgBBox &a, const SvgBBox &b, const SvgOverlapOptions &options)
{
    if (!options.ignore_shared_notehead_columns) return false;
    if (a.kind != "note" || b.kind != "note") return false;

    const double min_width = std::min(a.width, b.width);
    const double min_height = std::min(a.height, b.height);
    if (min_width <= 0.0 || min_height <= 0.0) return false;

    const double y_tolerance = std::max(4.0, min_height * 0.10);
    const double x_tolerance = std::max(4.0, min_width * 0.50);
    return std::abs(CenterY(a) - CenterY(b)) <= y_tolerance
        && std::abs(CenterX(a) - CenterX(b)) <= x_tolerance;
}

bool ShouldIgnorePair(const SvgBBox &a, const SvgBBox &b, const SvgOverlapOptions &options)
{
    if (!a.group_id.empty() && a.group_id == b.group_id) return true;
    if (options.ignore_same_source && !a.source_id.empty() && a.source_id == b.source_id) return true;
    return IsSharedNoteheadColumn(a, b, options);
}

std::string FormatDouble(double value)
{
    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(2);
    out << value;
    return out.str();
}

} // namespace

SvgOverlapResult DetectSvgBBoxOverlaps(
    const std::string &svg, int page_no, const SvgOverlapOptions &options)
{
    SvgOverlapResult result;
    result.summary.page_no = page_no;

    pugi::xml_document doc;
    const pugi::xml_parse_result parsed = doc.load_string(svg.c_str(), pugi::parse_default);
    if (!parsed) {
        result.parse_ok = false;
        result.error = parsed.description();
        return result;
    }

    std::map<std::string, SvgBBox> box_by_id;
    CollectBoxes(doc, options, box_by_id);

    std::vector<SvgBBox> boxes;
    boxes.reserve(box_by_id.size());
    for (const auto &[_, box] : box_by_id) boxes.push_back(box);
    result.summary.candidate_count = boxes.size();

    std::vector<SvgOverlapPair> overlaps;
    for (std::size_t i = 0; i < boxes.size(); ++i) {
        for (std::size_t j = i + 1; j < boxes.size(); ++j) {
            if (ShouldIgnorePair(boxes[i], boxes[j], options)) continue;
            const double overlap_area = OverlapArea(boxes[i], boxes[j]);
            if (overlap_area <= 0.0) continue;
            const double min_area = std::min(Area(boxes[i]), Area(boxes[j]));
            if (min_area <= 0.0) continue;
            const double ratio = overlap_area / min_area;
            if (ratio < options.min_overlap_ratio) continue;
            overlaps.push_back(SvgOverlapPair{ .first_id = boxes[i].object_id,
                .second_id = boxes[j].object_id,
                .first_kind = boxes[i].kind,
                .second_kind = boxes[j].kind,
                .area = overlap_area,
                .ratio = ratio });
        }
    }

    std::sort(overlaps.begin(), overlaps.end(), [](const SvgOverlapPair &a, const SvgOverlapPair &b) {
        if (a.ratio != b.ratio) return a.ratio > b.ratio;
        return a.area > b.area;
    });

    result.summary.overlap_count = overlaps.size();
    const std::size_t keep = std::min(options.max_reported_pairs, overlaps.size());
    result.summary.worst_pairs.assign(overlaps.begin(), overlaps.begin() + static_cast<std::ptrdiff_t>(keep));
    return result;
}

std::string FormatSvgOverlapWarning(const SvgOverlapSummary &summary)
{
    std::ostringstream out;
    out << "page " << summary.page_no << ": detected " << summary.overlap_count
        << " note/rest bounding-box overlaps above threshold among " << summary.candidate_count
        << " candidates";
    if (!summary.worst_pairs.empty()) {
        out << "; worst";
        for (std::size_t i = 0; i < summary.worst_pairs.size(); ++i) {
            const SvgOverlapPair &pair = summary.worst_pairs[i];
            out << (i == 0 ? ": " : ", ") << pair.first_id << " vs " << pair.second_id
                << " (" << FormatDouble(pair.ratio * 100.0) << "%)";
        }
    }
    return out.str();
}

} // namespace verosim
