#include "verosim/model/detail_tier.h"

namespace verosim {

std::string_view DetailTierName(DetailTier tier)
{
    switch (tier) {
        case DetailTier::kTierA: return "tierA";
        case DetailTier::kTierAB: return "tierAB";
        case DetailTier::kTierABDir: return "tierAB_dir";
    }
    return "?";
}

std::optional<DetailTier> ParseDetailTier(std::string_view text)
{
    if (text == "tierA") return DetailTier::kTierA;
    if (text == "tierAB") return DetailTier::kTierAB;
    if (text == "tierAB_dir") return DetailTier::kTierABDir;
    return std::nullopt;
}

bool DetailIncludesTierB(DetailTier tier)
{
    return tier == DetailTier::kTierAB || tier == DetailTier::kTierABDir;
}

bool DetailIncludesDirections(DetailTier tier)
{
    return tier == DetailTier::kTierABDir;
}

} // namespace verosim
