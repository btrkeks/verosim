#include "verosim/visual/visual_mark.h"

namespace verosim {

std::string_view VisualSideName(VisualSide side)
{
    switch (side) {
        case VisualSide::kPred: return "prediction";
        case VisualSide::kGt: return "ground-truth";
    }
    return "?";
}

std::string_view VisualRoleName(VisualRole role)
{
    switch (role) {
        case VisualRole::kInserted: return "inserted";
        case VisualRole::kDeleted: return "deleted";
        case VisualRole::kChanged: return "changed";
    }
    return "?";
}

std::string_view VisualTargetKindName(VisualTargetKind kind)
{
    switch (kind) {
        case VisualTargetKind::kNote: return "note";
        case VisualTargetKind::kExtra: return "extra";
        case VisualTargetKind::kMeasure: return "measure";
        case VisualTargetKind::kAccidental: return "accidental";
    }
    return "?";
}

} // namespace verosim
