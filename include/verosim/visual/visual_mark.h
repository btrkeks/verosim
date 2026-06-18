#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "verosim/model/sym_score.h"
#include "verosim/visual/svg_selector.h"

namespace verosim {

enum class VisualSide { kPred, kGt };
enum class VisualRole { kInserted, kDeleted, kChanged };
enum class VisualTargetKind { kNote, kExtra, kMeasure, kAccidental };

struct VisualSymbolRef {
    VisualTargetKind kind = VisualTargetKind::kNote;
    SymbolLocator locator;
    std::string primary_id;
    std::string fallback_id;
    ExtraKind extra_kind = ExtraKind::kClef;
    bool has_extra_kind = false;
};

struct VisualMark {
    VisualSide side = VisualSide::kPred;
    VisualRole role = VisualRole::kChanged;
    VisualSymbolRef target;
    std::string op_name;
    std::string category;
    long cost = 0;
    std::string label;
};

struct ResolvedVisualMark {
    std::size_t source_index = 0;
    VisualMark mark;
    std::vector<SvgSelector> selectors;
};

struct VisualPlan {
    std::vector<VisualMark> marks;
};

std::string_view VisualSideName(VisualSide side);
std::string_view VisualRoleName(VisualRole role);
std::string_view VisualTargetKindName(VisualTargetKind kind);

} // namespace verosim
