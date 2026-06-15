#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace verosim {

enum class VisualSide { kPred, kGt };
enum class VisualRole { kInserted, kDeleted, kChanged };
enum class VisualTargetKind { kNote, kExtra, kMeasure };

struct VisualMark {
    VisualSide side = VisualSide::kPred;
    VisualRole role = VisualRole::kChanged;
    VisualTargetKind target_kind = VisualTargetKind::kNote;
    std::string target_id;
    std::string fallback_id;
    std::string op_name;
    std::string category;
    long cost = 0;
    std::string label;
};

struct VisualPlan {
    std::vector<VisualMark> marks;
};

std::string_view VisualSideName(VisualSide side);
std::string_view VisualRoleName(VisualRole role);
std::string_view VisualTargetKindName(VisualTargetKind kind);

} // namespace verosim
