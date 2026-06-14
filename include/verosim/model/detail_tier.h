#pragma once

#include <optional>
#include <string_view>

namespace verosim {

enum class DetailTier {
    kTierA,
    kTierAB,
    kTierABDir,
};

std::string_view DetailTierName(DetailTier tier);
std::optional<DetailTier> ParseDetailTier(std::string_view text);
bool DetailIncludesTierB(DetailTier tier);
bool DetailIncludesDirections(DetailTier tier);

} // namespace verosim
