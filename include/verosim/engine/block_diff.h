#pragma once

#include "verosim/engine/edit_op.h"
#include "verosim/engine/interner.h"
#include "verosim/engine/myers.h"
#include "verosim/model/sym_score.h"

namespace verosim {

// Comparison._block_diff_lin (comparison.py:407-490) over one non-common
// subsequence of measures: suffix Levenshtein with measure notation sizes as
// ins/del costs and the inside-bar set distances as the edit cost.
DiffResult BlockDiffLin(const SymPart &orig_part, const PreparedPart &orig_prep,
    const SymPart &comp_part, const PreparedPart &comp_prep, const NcsBlock &block);

} // namespace verosim
