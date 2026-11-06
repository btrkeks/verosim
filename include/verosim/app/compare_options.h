#pragma once

#include <memory>
#include <set>

#include "verosim/engine/compare.h"
#include "verosim/extraction/typed_space_policy.h"
#include "verosim/model/evaluation_profile.h"
#include "verosim/model/metric_mode.h"

namespace verosim {

inline constexpr const char *kDisplayStaffProfileConflict
    = "--display-staff cannot be combined with --evaluation-profile; "
      "schema v1 does not support display staff scoring";

// Options shared by all score-comparison entry points. CLI code may add
// serialization-specific knobs around this, but loading, extraction,
// comparison, and visualization all consume only this metric surface.
struct CompareRunOptions {
    MetricSurface surface;
    NotePositionPolicy note_position_policy = NotePositionPolicy::kVisualEventOrder;
    AlignmentPolicy alignment_policy = kDefaultAlignmentPolicy;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
    std::shared_ptr<const EvaluationProfile> evaluation_profile;
    bool trace_alignment = false;
    std::set<int> trace_measures;
};

} // namespace verosim
