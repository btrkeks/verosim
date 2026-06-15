#pragma once

#include <vector>

#include "verosim/engine/edit_op.h"
#include "verosim/visual/visual_mark.h"

namespace verosim {

VisualPlan BuildVisualPlan(const std::vector<EditOp> &ops);

} // namespace verosim
