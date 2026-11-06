#pragma once

#include <iosfwd>

#include "verosim/engine/alignment_trace.h"

namespace verosim {

void WriteAlignmentTraceJson(const SymScore &prediction, const SymScore &ground_truth,
    const AlignmentTrace &trace, std::ostream &out);

} // namespace verosim
