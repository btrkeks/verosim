#include "verosim/visual/visual_resolver.h"

#include <optional>

#include "verosim/visual/svg_symbol_index.h"

namespace verosim {

VisualResolveResult ResolveVisualMarks(
    const std::string &svg, const std::vector<VisualMark> &marks, int measure_idx_offset)
{
    VisualResolveResult result;
    result.resolved.assign(marks.size(), false);

    const SvgSymbolIndex index = SvgSymbolIndex::Build(svg, measure_idx_offset);
    if (!index.parse_ok()) {
        result.parse_ok = false;
        result.error = index.error();
        return result;
    }
    result.measure_count = index.measure_count();

    for (std::size_t i = 0; i < marks.size(); ++i) {
        const std::optional<SvgSelector> selector = index.Resolve(marks[i].target);
        if (!selector) continue;
        result.resolved[i] = true;
        result.marks.push_back(
            ResolvedVisualMark{ .source_index = i, .mark = marks[i], .selectors = { *selector } });
    }

    return result;
}

} // namespace verosim
