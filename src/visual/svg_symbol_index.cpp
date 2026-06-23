#include "verosim/visual/svg_symbol_index.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
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

std::vector<std::string> ClassTokens(const std::string &classes)
{
    std::vector<std::string> tokens;
    std::size_t pos = 0;
    while (pos < classes.size()) {
        while (pos < classes.size() && classes[pos] == ' ') ++pos;
        const std::size_t start = pos;
        while (pos < classes.size() && classes[pos] != ' ') ++pos;
        if (pos > start) tokens.push_back(classes.substr(start, pos - start));
    }
    return tokens;
}

std::string ClassAttr(pugi::xml_node node)
{
    const pugi::xml_attribute attr = node.attribute("class");
    return attr ? std::string(attr.value()) : std::string();
}

std::optional<SvgSelector> SelectorForNode(pugi::xml_node node)
{
    const pugi::xml_attribute id = node.attribute("id");
    if (id && id.value()[0] != '\0') {
        return SvgSelector{ .kind = SvgSelectorKind::kId, .value = id.value() };
    }
    const std::string classes = ClassAttr(node);
    for (const std::string &token : ClassTokens(classes)) {
        if (token.rfind("id-", 0) == 0) {
            return SvgSelector{ .kind = SvgSelectorKind::kClassToken, .value = token };
        }
    }
    return std::nullopt;
}

std::optional<std::string> StaffNFromSvgId(const std::string &id)
{
    const std::size_t f = id.find('F');
    if (f == std::string::npos || f + 1 >= id.size()) return std::nullopt;
    std::size_t pos = f + 1;
    const std::size_t start = pos;
    while (pos < id.size() && std::isdigit(static_cast<unsigned char>(id[pos]))) ++pos;
    if (pos == start) return std::nullopt;
    return id.substr(start, pos - start);
}

bool IsNoteLike(const std::string &classes)
{
    if (HasClassToken(classes, "bounding-box") || HasClassToken(classes, "content-bounding-box")) {
        return false;
    }
    return HasClassToken(classes, "note") || HasClassToken(classes, "rest")
        || HasClassToken(classes, "mRest");
}

bool IsBarlineLike(const std::string &classes)
{
    return HasClassToken(classes, "barLine") || HasClassToken(classes, "barLineAttr");
}

std::optional<ExtraKind> ExtraKindFromClasses(const std::string &classes)
{
    if (HasClassToken(classes, "clef")) return ExtraKind::kClef;
    if (HasClassToken(classes, "keySig")) return ExtraKind::kKeySig;
    if (HasClassToken(classes, "meterSig")) return ExtraKind::kTimeSig;
    if (HasClassToken(classes, "mensur")) return ExtraKind::kTimeSig;
    if (HasClassToken(classes, "octave")) return ExtraKind::kOttava;
    return std::nullopt;
}

void CollectAllIdsAndClassTokens(
    pugi::xml_node node, std::vector<std::string> &ids, std::vector<std::string> &class_tokens)
{
    const pugi::xml_attribute id = node.attribute("id");
    if (id && id.value()[0] != '\0') ids.emplace_back(id.value());
    for (const std::string &token : ClassTokens(ClassAttr(node))) class_tokens.push_back(token);

    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
        if (child.type() == pugi::node_element) CollectAllIdsAndClassTokens(child, ids, class_tokens);
    }
}

void CollectDescendants(pugi::xml_node node, std::vector<pugi::xml_node> &out)
{
    for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
        if (child.type() != pugi::node_element) continue;
        out.push_back(child);
        CollectDescendants(child, out);
    }
}

std::optional<SvgSelector> FirstAccidentalSelector(pugi::xml_node note_node)
{
    std::vector<pugi::xml_node> descendants;
    CollectDescendants(note_node, descendants);
    for (pugi::xml_node node : descendants) {
        if (!HasClassToken(ClassAttr(node), "accid")) continue;
        return SelectorForNode(node);
    }
    return std::nullopt;
}

bool SameMeasure(const SymbolLocator &a, const SymbolLocator &b)
{
    if (!a.measure_vrv_id.empty() && !b.measure_vrv_id.empty()) {
        return a.measure_vrv_id == b.measure_vrv_id;
    }
    if (a.measure_idx >= 0 && b.measure_idx >= 0) return a.measure_idx == b.measure_idx;
    return true;
}

bool SameStaff(const SymbolLocator &a, const SymbolLocator &b)
{
    if (a.part_idx >= 0 && b.part_idx >= 0) return a.part_idx == b.part_idx;
    if (!a.staff_n.empty() && !b.staff_n.empty() && a.staff_n == b.staff_n) return true;
    return a.part_idx < 0 && a.staff_n.empty();
}

bool SameOccurrence(const SymbolLocator &a, const SymbolLocator &b)
{
    return a.occurrence < 0 || b.occurrence < 0 || a.occurrence == b.occurrence;
}

pugi::xml_node AncestorStaffWithinMeasure(pugi::xml_node node)
{
    for (pugi::xml_node parent = node.parent(); parent; parent = parent.parent()) {
        if (HasClassToken(ClassAttr(parent), "measure")) return pugi::xml_node();
        if (HasClassToken(ClassAttr(parent), "staff")) return parent;
    }
    return pugi::xml_node();
}

int PartIdxForStaff(pugi::xml_node measure, pugi::xml_node target_staff,
    const std::map<std::string, int> &rendered_staff_order)
{
    int staff_order = 0;
    for (pugi::xml_node staff = measure.first_child(); staff; staff = staff.next_sibling()) {
        if (staff.type() != pugi::node_element || !HasClassToken(ClassAttr(staff), "staff")) {
            continue;
        }
        if (staff == target_staff) {
            const std::optional<std::string> staff_n = StaffNFromSvgId(staff.attribute("id").value());
            if (staff_n) {
                const auto rendered_order = rendered_staff_order.find(*staff_n);
                if (rendered_order != rendered_staff_order.end()) return rendered_order->second;
            }
            return staff_order;
        }
        ++staff_order;
    }
    return -1;
}

SymbolLocator BarlineLocatorForNode(pugi::xml_node measure, pugi::xml_node barline,
    const std::map<std::string, int> &rendered_staff_order, int measure_idx,
    const std::string &measure_id, int occurrence)
{
    SymbolLocator loc{ .measure_idx = measure_idx,
        .measure_vrv_id = measure_id,
        .offset = Fraction(0),
        .occurrence = occurrence };

    pugi::xml_node staff = AncestorStaffWithinMeasure(barline);
    if (staff) {
        loc.part_idx = PartIdxForStaff(measure, staff, rendered_staff_order);
        loc.staff_n = StaffNFromSvgId(staff.attribute("id").value()).value_or("");
    }
    return loc;
}

bool HasSpecificStaff(const SymbolLocator &loc)
{
    return loc.part_idx >= 0 || !loc.staff_n.empty();
}

} // namespace

SvgSymbolIndex SvgSymbolIndex::Build(const std::string &svg, int measure_idx_offset)
{
    SvgSymbolIndex index;

    pugi::xml_document doc;
    const pugi::xml_parse_result parsed = doc.load_string(svg.c_str(), pugi::parse_default);
    if (!parsed) {
        index.parse_ok_ = false;
        index.error_ = parsed.description();
        return index;
    }

    CollectAllIdsAndClassTokens(doc, index.ids_, index.class_tokens_);

    std::vector<pugi::xml_node> all_nodes;
    CollectDescendants(doc, all_nodes);
    std::map<std::string, int> rendered_staff_order;
    for (pugi::xml_node measure : all_nodes) {
        if (!HasClassToken(ClassAttr(measure), "measure")) continue;
        for (pugi::xml_node staff = measure.first_child(); staff; staff = staff.next_sibling()) {
            if (staff.type() != pugi::node_element || !HasClassToken(ClassAttr(staff), "staff")) {
                continue;
            }
            const std::optional<std::string> staff_n = StaffNFromSvgId(staff.attribute("id").value());
            if (staff_n && rendered_staff_order.find(*staff_n) == rendered_staff_order.end()) {
                rendered_staff_order.emplace(
                    *staff_n, static_cast<int>(rendered_staff_order.size()));
            }
        }
    }

    int page_measure_idx = 0;
    for (pugi::xml_node measure : all_nodes) {
        if (!HasClassToken(ClassAttr(measure), "measure")) continue;
        const int measure_idx = measure_idx_offset + page_measure_idx;
        const std::optional<SvgSelector> measure_selector = SelectorForNode(measure);
        const std::string measure_id = measure.attribute("id").value();
        if (measure_selector) {
            index.candidates_.push_back(Candidate{ .kind = VisualTargetKind::kMeasure,
                .locator = SymbolLocator{ .measure_idx = measure_idx,
                    .measure_vrv_id = measure_id,
                    .offset = Fraction(0),
                    .occurrence = 0 },
                .selector = *measure_selector });
        }

        int barline_occurrence = 0;
        std::vector<pugi::xml_node> measure_nodes;
        CollectDescendants(measure, measure_nodes);
        for (pugi::xml_node child : measure_nodes) {
            if (!IsBarlineLike(ClassAttr(child))) continue;
            const std::optional<SvgSelector> selector = SelectorForNode(child);
            if (!selector) continue;
            SymbolLocator loc = BarlineLocatorForNode(
                measure, child, rendered_staff_order, measure_idx, measure_id, barline_occurrence++);
            index.candidates_.push_back(Candidate{ .kind = VisualTargetKind::kBarline,
                .locator = loc,
                .selector = *selector });
        }

        int staff_order = 0;
        for (pugi::xml_node staff = measure.first_child(); staff; staff = staff.next_sibling()) {
            if (staff.type() != pugi::node_element || !HasClassToken(ClassAttr(staff), "staff")) {
                continue;
            }
            const std::string staff_id = staff.attribute("id").value();
            const std::optional<std::string> staff_n = StaffNFromSvgId(staff_id);
            int part_idx = staff_order;
            if (staff_n) {
                const auto rendered_order = rendered_staff_order.find(*staff_n);
                if (rendered_order != rendered_staff_order.end()) part_idx = rendered_order->second;
            }
            const SymbolLocator staff_base{ .part_idx = part_idx,
                .staff_n = staff_n.value_or(""),
                .measure_idx = measure_idx,
                .measure_vrv_id = measure_id,
                .offset = Fraction(0),
                .occurrence = 0 };

            std::vector<pugi::xml_node> staff_nodes;
            CollectDescendants(staff, staff_nodes);

            int note_occurrence = 0;
            std::map<ExtraKind, int> extra_occurrences;
            for (pugi::xml_node node : staff_nodes) {
                const std::string classes = ClassAttr(node);
                if (IsNoteLike(classes)) {
                    if (const std::optional<SvgSelector> selector = SelectorForNode(node)) {
                        SymbolLocator loc = staff_base;
                        loc.occurrence = note_occurrence++;
                        index.candidates_.push_back(Candidate{ .kind = VisualTargetKind::kNote,
                            .locator = loc,
                            .selector = *selector,
                            .accidental_selector = FirstAccidentalSelector(node) });
                    }
                    continue;
                }
                const std::optional<ExtraKind> extra_kind = ExtraKindFromClasses(classes);
                if (!extra_kind) continue;
                if (const std::optional<SvgSelector> selector = SelectorForNode(node)) {
                    SymbolLocator loc = staff_base;
                    loc.occurrence = extra_occurrences[*extra_kind]++;
                    index.candidates_.push_back(Candidate{ .kind = VisualTargetKind::kExtra,
                        .locator = loc,
                        .selector = *selector,
                        .extra_kind = *extra_kind,
                        .has_extra_kind = true });
                }
            }
            ++staff_order;
        }
        ++page_measure_idx;
    }
    index.measure_count_ = page_measure_idx;

    return index;
}

std::optional<SvgSelector> SvgSymbolIndex::ExactSelector(const std::string &id) const
{
    if (id.empty()) return std::nullopt;
    if (std::find(ids_.begin(), ids_.end(), id) != ids_.end()) {
        return SvgSelector{ .kind = SvgSelectorKind::kId, .value = id };
    }
    const std::string class_token = "id-" + id;
    if (std::find(class_tokens_.begin(), class_tokens_.end(), class_token) != class_tokens_.end()) {
        return SvgSelector{ .kind = SvgSelectorKind::kClassToken, .value = class_token };
    }
    return std::nullopt;
}

bool SvgSymbolIndex::CandidateMatchesRef(
    const SvgSymbolIndex::Candidate &candidate, const VisualSymbolRef &ref) const
{
    if (candidate.kind != ref.kind) return false;
    if (ref.kind == VisualTargetKind::kExtra) {
        if (!ref.has_extra_kind || !candidate.has_extra_kind || candidate.extra_kind != ref.extra_kind) {
            return false;
        }
    }
    if (!SameMeasure(candidate.locator, ref.locator)) return false;
    if (ref.kind != VisualTargetKind::kMeasure && ref.kind != VisualTargetKind::kBarline
        && !SameStaff(candidate.locator, ref.locator)) {
        return false;
    }
    return SameOccurrence(candidate.locator, ref.locator);
}

const SvgSymbolIndex::Candidate *SvgSymbolIndex::FindCandidate(const VisualSymbolRef &ref) const
{
    for (const Candidate &candidate : candidates_) {
        if (CandidateMatchesRef(candidate, ref)) return &candidate;
    }
    return nullptr;
}

const SvgSymbolIndex::Candidate *SvgSymbolIndex::FindBarlineCandidate(const VisualSymbolRef &ref) const
{
    std::vector<const Candidate *> matches;
    for (const Candidate &candidate : candidates_) {
        if (candidate.kind != VisualTargetKind::kBarline) continue;
        if (!SameMeasure(candidate.locator, ref.locator)) continue;
        matches.push_back(&candidate);
    }
    if (matches.empty()) return nullptr;
    if (matches.size() == 1) return matches.front();

    std::vector<const Candidate *> staff_matches;
    std::vector<const Candidate *> unstaffed_matches;
    for (const Candidate *candidate : matches) {
        if (HasSpecificStaff(candidate->locator) && SameStaff(candidate->locator, ref.locator)) {
            staff_matches.push_back(candidate);
        }
        else if (!HasSpecificStaff(candidate->locator)) {
            unstaffed_matches.push_back(candidate);
        }
    }

    const std::vector<const Candidate *> &candidates = ref.barline_boundary && !unstaffed_matches.empty()
        ? unstaffed_matches
        : !staff_matches.empty() ? staff_matches
        : !unstaffed_matches.empty() ? unstaffed_matches
                                     : matches;

    if (!ref.barline_boundary && !staff_matches.empty() && ref.locator.occurrence >= 0) {
        for (const Candidate *candidate : candidates) {
            if (candidate->locator.occurrence == ref.locator.occurrence) return candidate;
        }
    }

    if (ref.locator.offset == Fraction(0)) return candidates.front();
    return candidates.back();
}

std::optional<SvgSelector> SvgSymbolIndex::StructuralSelector(const VisualSymbolRef &ref) const
{
    if (ref.kind == VisualTargetKind::kBarline) {
        const Candidate *candidate = FindBarlineCandidate(ref);
        if (!candidate) return std::nullopt;
        return candidate->selector;
    }
    if (ref.kind == VisualTargetKind::kAccidental) {
        VisualSymbolRef note_ref = ref;
        note_ref.kind = VisualTargetKind::kNote;
        note_ref.has_extra_kind = false;
        const Candidate *note = FindCandidate(note_ref);
        if (note && note->accidental_selector) return note->accidental_selector;
        return std::nullopt;
    }
    const Candidate *candidate = FindCandidate(ref);
    if (!candidate) return std::nullopt;
    return candidate->selector;
}

std::optional<SvgSelector> SvgSymbolIndex::Resolve(const VisualSymbolRef &ref) const
{
    if (const std::optional<SvgSelector> selector = ExactSelector(ref.primary_id)) return selector;
    if (const std::optional<SvgSelector> selector = ExactSelector(ref.fallback_id)) return selector;
    return StructuralSelector(ref);
}

} // namespace verosim
