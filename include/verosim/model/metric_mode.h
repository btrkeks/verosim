#pragma once

#include <optional>
#include <string_view>

namespace verosim {

enum class MetricMode {
    kActive,
    kExperimental,
};

std::string_view MetricModeName(MetricMode mode);
std::optional<MetricMode> ParseMetricMode(std::string_view text);
bool MetricModeIncludesDirections(MetricMode mode);
bool MetricModeIncludesBarlines(MetricMode mode);

} // namespace verosim
