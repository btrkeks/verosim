#include "verosim/visual/visual_plan.h"

#include <string>

#include "verosim/engine/compare.h"
#include "verosim/model/sym_score.h"

namespace verosim {
namespace {

struct ElementRef {
    VisualTargetKind kind = VisualTargetKind::kNote;
    std::string id;
    std::string fallback_id;
};

bool RefFromSide(const OpSide &side, ElementRef &ref)
{
    switch (side.kind) {
        case OpSide::Kind::kNone: return false;
        case OpSide::Kind::kNote:
            if (side.note == nullptr || side.note->vrv_id.empty()) return false;
            ref = { .kind = VisualTargetKind::kNote,
                .id = side.note->visual_id.empty() ? side.note->vrv_id : side.note->visual_id,
                .fallback_id = side.note->vrv_id };
            return true;
        case OpSide::Kind::kExtra:
            if (side.extra == nullptr || side.extra->vrv_id.empty()) return false;
            ref = { .kind = VisualTargetKind::kExtra,
                .id = side.extra->vrv_id,
                .fallback_id = side.extra->vrv_id };
            return true;
        case OpSide::Kind::kMeasure:
            if (side.measure == nullptr || side.measure->vrv_id.empty()) return false;
            ref = { .kind = VisualTargetKind::kMeasure,
                .id = side.measure->vrv_id,
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
    if (ref.id.empty()) return;
    plan.marks.push_back(VisualMark{ .side = side,
        .role = role,
        .target_kind = ref.kind,
        .target_id = ref.id,
        .fallback_id = ref.fallback_id.empty() ? ref.id : ref.fallback_id,
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
        if (measure.vrv_id.empty()) continue;
        AddMark(plan, side, role,
            ElementRef{ .kind = VisualTargetKind::kMeasure, .id = measure.vrv_id },
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
