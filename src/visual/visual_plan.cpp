#include "verosim/visual/visual_plan.h"

#include <optional>
#include <string>
#include <string_view>

#include "verosim/engine/compare.h"
#include "verosim/model/sym_score.h"

namespace verosim {
namespace {

struct ElementRef {
    VisualSymbolRef symbol;
};

bool IsAccidentalOp(OpName name)
{
    return name == OpName::kAccidentIns || name == OpName::kAccidentDel
        || name == OpName::kAccidentEdit;
}

bool IsBarlineVisualExtraKind(ExtraKind kind)
{
    return kind == ExtraKind::kBarline || kind == ExtraKind::kRepeat;
}

VisualTargetKind VisualTargetKindForExtra(ExtraKind kind)
{
    if (!ExtraKindRenderedAsSvgSymbol(kind)) return VisualTargetKind::kMeasure;
    return IsBarlineVisualExtraKind(kind) ? VisualTargetKind::kBarline : VisualTargetKind::kExtra;
}

std::string VisualFallbackIdForExtra(const SymExtra &extra)
{
    if (!ExtraKindRenderedAsSvgSymbol(extra.kind)) return extra.locator.measure_vrv_id;
    if (!IsBarlineVisualExtraKind(extra.kind)) return extra.vrv_id;
    constexpr std::string_view end_suffix = ":end";
    constexpr std::string_view start_suffix = ":start";
    const auto strip_suffix = [&](std::string_view suffix) -> std::optional<std::string> {
        if (extra.vrv_id.size() <= suffix.size()) return std::nullopt;
        const std::size_t suffix_pos = extra.vrv_id.size() - suffix.size();
        if (extra.vrv_id.compare(suffix_pos, suffix.size(), suffix) != 0) return std::nullopt;
        return extra.vrv_id.substr(0, suffix_pos);
    };
    if (const std::optional<std::string> base = strip_suffix(end_suffix)) return *base;
    if (const std::optional<std::string> base = strip_suffix(start_suffix)) return *base;
    return extra.vrv_id;
}

bool HasVisibleAccidental(const SymNote *note)
{
    if (note == nullptr) return false;
    for (const SymPitch &pitch : note->pitches) {
        if (pitch.accid != "None") return true;
    }
    return false;
}

std::string AccidentalIdFromNoteId(const std::string &note_id)
{
    constexpr std::string_view note_prefix = "note-";
    if (note_id.rfind(note_prefix, 0) == 0) {
        return "accid-" + note_id.substr(note_prefix.size());
    }
    return "accid-" + note_id;
}

bool AccidentalRefFromSide(const OpSide &side, ElementRef &ref)
{
    if (side.kind != OpSide::Kind::kNote || side.note == nullptr) return false;
    if (!HasVisibleAccidental(side.note)) return false;

    const std::string note_id
        = side.note->visual_id.empty() ? side.note->vrv_id : side.note->visual_id;
    const std::string accid_id = note_id.empty() ? std::string() : AccidentalIdFromNoteId(note_id);
    ref.symbol = VisualSymbolRef{ .kind = VisualTargetKind::kAccidental,
        .locator = side.note->locator,
        .primary_id = accid_id,
        .fallback_id = accid_id };
    return true;
}

bool RefFromSide(const OpSide &side, ElementRef &ref)
{
    switch (side.kind) {
        case OpSide::Kind::kNone: return false;
        case OpSide::Kind::kNote:
            if (side.note == nullptr) return false;
            ref.symbol = VisualSymbolRef{ .kind = VisualTargetKind::kNote,
                .locator = side.note->locator,
                .primary_id = side.note->visual_id.empty() ? side.note->vrv_id : side.note->visual_id,
                .fallback_id = side.note->vrv_id };
            return true;
        case OpSide::Kind::kExtra:
            if (side.extra == nullptr) return false;
            ref.symbol = VisualSymbolRef{ .kind = VisualTargetKindForExtra(side.extra->kind),
                .locator = side.extra->locator,
                .primary_id = ExtraKindRenderedAsSvgSymbol(side.extra->kind)
                    ? side.extra->vrv_id
                    : side.extra->locator.measure_vrv_id,
                .fallback_id = VisualFallbackIdForExtra(*side.extra),
                .extra_kind = side.extra->kind,
                .has_extra_kind = ExtraKindRenderedAsSvgSymbol(side.extra->kind),
                .barline_boundary = side.extra->visual_barline_boundary };
            return true;
        case OpSide::Kind::kMeasure:
            if (side.measure == nullptr) return false;
            ref.symbol = VisualSymbolRef{ .kind = VisualTargetKind::kMeasure,
                .locator = side.measure->locator,
                .primary_id = side.measure->vrv_id,
                .fallback_id = side.measure->vrv_id };
            return true;
        case OpSide::Kind::kPart:
            return false; // part ops are expanded to measure marks below
    }
    return false;
}

std::string LabelFor(
    VisualRole role, const std::string &op_name, const std::string &category, long cost)
{
    std::string label;
    label += std::string(VisualRoleName(role)) + " ";
    label += op_name;
    label += " | ";
    label += category;
    label += " | cost ";
    label += std::to_string(cost);
    return label;
}

void AddMark(VisualPlan &plan, VisualSide side, VisualRole role, const ElementRef &ref,
    const std::string &op_name, const std::string &category, long cost)
{
    if (ref.symbol.primary_id.empty() && ref.symbol.fallback_id.empty()
        && ref.symbol.locator.measure_idx < 0) {
        return;
    }
    plan.marks.push_back(VisualMark{ .side = side,
        .role = role,
        .target = ref.symbol,
        .op_name = op_name,
        .category = category,
        .cost = cost,
        .label = LabelFor(role, op_name, category, cost) });
}

void AddPartMarks(VisualPlan &plan, VisualSide side, VisualRole role, const SymPart *part,
    const std::string &op_name, const std::string &category, long cost)
{
    if (part == nullptr) return;
    for (const SymMeasure &measure : part->bar_list) {
        AddMark(plan, side, role,
            ElementRef{ .symbol = VisualSymbolRef{ .kind = VisualTargetKind::kMeasure,
                .locator = measure.locator,
                .primary_id = measure.vrv_id,
                .fallback_id = measure.vrv_id } },
            op_name, category, cost);
    }
}

} // namespace

VisualPlan BuildVisualPlan(const std::vector<EditOp> &ops)
{
    VisualPlan plan;
    for (const EditOp &op : ops) {
        const std::string op_name(OpNameStr(op.name));
        const std::string category = EditOpCategory(op);

        switch (op.name) {
            case OpName::kInsPart:
                AddPartMarks(plan, VisualSide::kGt, VisualRole::kInserted, op.b.part, op_name,
                    category, op.cost);
                continue;
            case OpName::kDelPart:
                AddPartMarks(plan, VisualSide::kPred, VisualRole::kDeleted, op.a.part, op_name,
                    category, op.cost);
                continue;
            default: break;
        }

        ElementRef ref;
        if (IsAccidentalOp(op.name)) {
            if ((op.name == OpName::kAccidentDel || op.name == OpName::kAccidentEdit)
                && AccidentalRefFromSide(op.a, ref)) {
                AddMark(plan, VisualSide::kPred, VisualRole::kChanged, ref, op_name, category,
                    op.cost);
            }
            if ((op.name == OpName::kAccidentIns || op.name == OpName::kAccidentEdit)
                && AccidentalRefFromSide(op.b, ref)) {
                AddMark(plan, VisualSide::kGt, VisualRole::kChanged, ref, op_name, category,
                    op.cost);
            }
            continue;
        }

        if (op.a.kind == OpSide::Kind::kNone) {
            if (RefFromSide(op.b, ref)) {
                AddMark(plan, VisualSide::kGt, VisualRole::kInserted, ref, op_name, category, op.cost);
            }
            continue;
        }
        if (op.b.kind == OpSide::Kind::kNone) {
            if (RefFromSide(op.a, ref)) {
                AddMark(plan, VisualSide::kPred, VisualRole::kDeleted, ref, op_name, category, op.cost);
            }
            continue;
        }

        if (RefFromSide(op.a, ref)) {
            AddMark(plan, VisualSide::kPred, VisualRole::kChanged, ref, op_name, category, op.cost);
        }
        if (RefFromSide(op.b, ref)) {
            AddMark(plan, VisualSide::kGt, VisualRole::kChanged, ref, op_name, category, op.cost);
        }
    }
    return plan;
}

} // namespace verosim
