#pragma once

#include <optional>
#include <string_view>

namespace verosim {

enum class MetricMode {
    kActive,
    kExperimental,
};

enum class LayoutSurface {
    kNone,
    kSystemBreaks,
};

struct MetricSurface {
    MetricMode mode = MetricMode::kActive;
    LayoutSurface layout = LayoutSurface::kNone;
};

std::string_view MetricModeName(MetricMode mode);
std::optional<MetricMode> ParseMetricMode(std::string_view text);
std::string_view LayoutSurfaceName(LayoutSurface layout);
std::optional<LayoutSurface> ParseLayoutSurface(std::string_view text);
bool MetricModeIncludesDirections(MetricMode mode);
bool MetricModeIncludesBarlines(MetricMode mode);
bool MetricSurfaceIncludesSystemBreaks(const MetricSurface &surface);

} // namespace verosim
