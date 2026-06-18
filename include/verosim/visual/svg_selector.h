#pragma once

#include <string>

namespace verosim {

enum class SvgSelectorKind { kId, kClassToken };

struct SvgSelector {
    SvgSelectorKind kind = SvgSelectorKind::kId;
    std::string value;
};

} // namespace verosim
