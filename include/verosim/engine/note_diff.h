#pragma once

#include "verosim/engine/edit_op.h"
#include "verosim/model/sym_score.h"

namespace verosim {

// musicdiff pitch-tuple equality: (step_octave, accid, tie) only.
// sounding_alter is resolver metadata (D13), never comparison content.
inline bool PitchEq(const SymPitch &a, const SymPitch &b)
{
    return a.step_octave == b.step_octave && a.accid == b.accid && a.tie == b.tie;
}

// M21Utils.pitch_size (m21utils.py:343): name + accidental + tie.
inline int PitchSize(const SymPitch &p)
{
    return 1 + (p.accid != "None" ? 1 : 0) + (p.tie ? 1 : 0);
}

// Comparison._pitches_diff (comparison.py:355-404). ids = (i, j) indices of
// the pitches within their notes' pitch lists.
DiffResult PitchesDiff(
    const SymPitch &p1, const SymPitch &p2, const SymNote *n1, const SymNote *n2, int id1, int id2);

// Comparison._annotated_note_diff (comparison.py:872-1005), v1-tier fields.
DiffResult AnnotatedNoteDiff(const SymNote &n1, const SymNote &n2);

// Cost-only variant: returns the edit cost without building the op list.
// Used during the DP fill phase of BlockDiffLin where only the scalar cost
// is needed to populate the cost table; the full op list is built later
// during backtracking for the cells on the optimal path only.
long AnnotatedNoteDiffCost(const SymNote &n1, const SymNote &n2);

} // namespace verosim
