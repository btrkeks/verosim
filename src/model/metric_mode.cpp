#include "verosim/model/metric_mode.h"

namespace verosim {

std::string_view MetricModeName(MetricMode mode)
{
    switch (mode) {
        case MetricMode::kActive: return "active";
        case MetricMode::kExperimental: return "experimental";
    }
    return "?";
}

std::optional<MetricMode> ParseMetricMode(std::string_view text)
{
    if (text == "active") return MetricMode::kActive;
    if (text == "experimental") return MetricMode::kExperimental;
    return std::nullopt;
}

std::string_view LayoutSurfaceName(LayoutSurface layout)
{
    switch (layout) {
        case LayoutSurface::kNone: return "none";
        case LayoutSurface::kSystemBreaks: return "system-breaks";
    }
    return "?";
}

std::optional<LayoutSurface> ParseLayoutSurface(std::string_view text)
{
    if (text == "none") return LayoutSurface::kNone;
    if (text == "system-breaks") return LayoutSurface::kSystemBreaks;
    return std::nullopt;
}

bool MetricModeIncludesDirections(MetricMode mode)
{
    return mode == MetricMode::kExperimental;
}

bool MetricModeIncludesBarlines(MetricMode)
{
    return true;
}

bool MetricSurfaceIncludesSystemBreaks(const MetricSurface &surface)
{
    return surface.layout == LayoutSurface::kSystemBreaks;
}

} // namespace verosim
