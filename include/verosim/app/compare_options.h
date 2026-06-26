#pragma once

#include "verosim/engine/compare.h"
#include "verosim/extraction/typed_space_policy.h"
#include "verosim/model/metric_mode.h"

namespace verosim {

// Options shared by all score-comparison entry points. CLI code may add
// serialization-specific knobs around this, but loading, extraction,
// comparison, and visualization all consume only this metric surface.
struct CompareRunOptions {
    MetricSurface surface;
    NotePositionPolicy note_position_policy = NotePositionPolicy::kVisualEventOrder;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
};

} // namespace verosim
