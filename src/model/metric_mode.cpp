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

bool MetricModeIncludesDirections(MetricMode mode)
{
    return mode == MetricMode::kExperimental;
}

bool MetricModeIncludesBarlines(MetricMode)
{
    return true;
}

} // namespace verosim
