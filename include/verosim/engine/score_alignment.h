#pragma once

#include "verosim/engine/compare.h"
#include "verosim/engine/interner.h"

namespace verosim
{

// Match staves in order with omissions, then align source-measure frames
// across all matched staves, including duration-supported 1:2 / 2:1 spans.
// Each changed boundary costs one per staff; notation edit weights stay unchanged.
CompareResult CompareSharedAlignment(const SymScore &prediction, const SymScore &ground_truth,
                                     const PreparedScore &prediction_prepared,
                                     const PreparedScore &ground_truth_prepared,
                                     const CompareOptions &options);

} // namespace verosim
