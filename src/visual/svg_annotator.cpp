#include "verosim/visual/svg_annotator.h"

#include <sstream>
#include <string>
#include <vector>

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

void AppendClassToken(pugi::xml_node node, const std::string &token)
{
    pugi::xml_attribute attr = node.attribute("class");
    const std::string current = attr ? attr.value() : "";
    if (HasClassToken(current, token)) return;
    const std::string next = current.empty() ? token : current + " " + token;
    if (attr) attr.set_value(next.c_str());
    else node.append_attribute("class").set_value(next.c_str());
}

bool NodeMatchesId(pugi::xml_node node, const std::string &id)
{
    if (id.empty()) return false;
    const pugi::xml_attribute id_attr = node.attribute("id");
    if (id_attr && id_attr.value() == id) return true;
    const pugi::xml_attribute data_id = node.attribute("data-id");
    if (data_id && data_id.value() == id) return true;

    const pugi::xml_attribute class_attr = node.attribute("class");
    if (!class_attr) return false;
    return HasClassToken(class_attr.value(), "id-" + id);
}

bool NodeMatchesSelector(pugi::xml_node node, const SvgSelector &selector)
{
    switch (selector.kind) {
        case SvgSelectorKind::kId: return NodeMatchesId(node, selector.value);
        case SvgSelectorKind::kClassToken: {
            const pugi::xml_attribute class_attr = node.attribute("class");
            return class_attr && HasClassToken(class_attr.value(), selector.value);
        }
    }
    return false;
}

void CollectMatchingNodes(
    pugi::xml_node node, const SvgSelector &selector, std::vector<pugi::xml_node> &matches)
{
    if (NodeMatchesSelector(node, selector)) matches.push_back(node);
    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_element) CollectMatchingNodes(child, selector, matches);
    }
}

void SetOrAppendAttr(pugi::xml_node node, const char *name, const std::string &value)
{
    pugi::xml_attribute attr = node.attribute(name);
    if (attr) attr.set_value(value.c_str());
    else node.append_attribute(name).set_value(value.c_str());
}

void AppendLabel(pugi::xml_node node, const std::string &label)
{
    pugi::xml_attribute attr = node.attribute("data-verosim-label");
    if (attr) {
        const std::string combined = std::string(attr.value()) + " | " + label;
        attr.set_value(combined.c_str());
    }
    else {
        node.append_attribute("data-verosim-label").set_value(label.c_str());
    }

    pugi::xml_node title = node.prepend_child("title");
    title.append_attribute("class").set_value("verosim-visual-title");
    title.text().set(label.c_str());
}

void ApplyMark(pugi::xml_node node, const VisualMark &mark)
{
    AppendClassToken(node, "verosim-mark");
    AppendClassToken(node, "verosim-role-" + std::string(VisualRoleName(mark.role)));
    AppendClassToken(node, "verosim-kind-" + std::string(VisualTargetKindName(mark.target.kind)));
    SetOrAppendAttr(node, "data-verosim-role", std::string(VisualRoleName(mark.role)));
    SetOrAppendAttr(node, "data-verosim-kind", std::string(VisualTargetKindName(mark.target.kind)));
    SetOrAppendAttr(node, "data-verosim-op", mark.op_name);
    SetOrAppendAttr(node, "data-verosim-category", mark.category);
    SetOrAppendAttr(node, "data-verosim-cost", std::to_string(mark.cost));
    AppendLabel(node, mark.label);
}

} // namespace

SvgAnnotationResult AnnotateSvg(
    const std::string &svg, const std::vector<ResolvedVisualMark> &marks, std::size_t source_count)
{
    SvgAnnotationResult result;
    result.resolved.assign(source_count, false);

    pugi::xml_document doc;
    const pugi::xml_parse_result parsed = doc.load_string(svg.c_str(), pugi::parse_default);
    if (!parsed) {
        result.svg = svg;
        result.parse_ok = false;
        result.error = parsed.description();
        return result;
    }

    for (std::size_t i = 0; i < marks.size(); ++i) {
        std::vector<pugi::xml_node> matches;
        for (const SvgSelector &selector : marks[i].selectors) {
            CollectMatchingNodes(doc, selector, matches);
        }
        if (matches.empty()) continue;
        if (marks[i].source_index < result.resolved.size()) result.resolved[marks[i].source_index] = true;
        for (pugi::xml_node node : matches) ApplyMark(node, marks[i].mark);
    }

    std::ostringstream out;
    doc.save(out, "  ", pugi::format_default | pugi::format_no_declaration);
    result.svg = out.str();
    return result;
}

} // namespace verosim
