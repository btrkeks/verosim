#pragma once

#include <optional>

#include "verosim/engine/compare.h"
#include "verosim/engine/edit_op.h"
#include "verosim/engine/interner.h"
#include "verosim/model/sym_score.h"

namespace verosim {

// Comparison._areDifferentEnough (comparison.py:565-583): offsets differ only
// beyond a 0.0001 tolerance. The Python threshold is the double literal
// 0.0001 (~1e-4 * (1 + 2.1e-17)); an exact Fraction strictly between 1/10000
// and that double needs a denominator beyond int64, so the exact comparison
// below is equivalent in range.
bool AreDifferentEnough(const Fraction &a, const Fraction &b);
bool AreDifferentEnough(const std::optional<Fraction> &a, const std::optional<Fraction> &b);

// Comparison._notes_set_distance (comparison.py:1140-1230): greedy
// order-dependent pairing by (visual pitch, offset, graceness), preferring
// exact visual-duration matches, with the first offset/pitch/grace match as
// fallback. Prepared ids supply AnnNote.__eq__ for the paired-notes shortcut.
DiffResult NotesSetDistance(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep,
    const CompareOptions &options = CompareOptions());

// Cost-only variant of NotesSetDistance: returns just the cost scalar.
long NotesSetDistanceCost(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep,
    const CompareOptions &options = CompareOptions());

// Comparison._extras_set_distance (comparison.py:1234-1337).
DiffResult ExtrasSetDistance(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep);

// Cost-only variant of ExtrasSetDistance.
long ExtrasSetDistanceCost(const SymMeasure &orig, const SymMeasure &comp,
    const PreparedMeasure &orig_prep, const PreparedMeasure &comp_prep);

// Comparison._annotated_extra_diff (comparison.py:586-648), v1-tier fields
// (content and styledict are never set at v1 tiers).
DiffResult AnnotatedExtraDiff(const SymExtra &e1, const SymExtra &e2);

// Cost-only variant.
long AnnotatedExtraDiffCost(const SymExtra &e1, const SymExtra &e2);

} // namespace verosim
