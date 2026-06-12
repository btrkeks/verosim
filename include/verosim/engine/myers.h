#pragma once

#include <cstdint>
#include <vector>

namespace verosim {

// One run of differing measures between two common (equal-content) runs:
// indices into the two bar_lists (Comparison._non_common_subsequences_of_measures
// retrieves measure objects; indices are the faithful C++ translation of its
// repr-hash lookup, since reprs embed unique ids).
struct NcsBlock {
    std::vector<int> original;
    std::vector<int> compare_to;
};

// Line-by-line port of Comparison._myers_diff + _non_common_subsequences_myers
// (comparison.py:157-247) over (content_id, index) rows: Myers LCS on the
// content ids, then grouping of the non-common runs split at equal steps.
std::vector<NcsBlock> NonCommonSubsequences(
    const std::vector<std::int32_t> &original_ids, const std::vector<std::int32_t> &compare_to_ids);

} // namespace verosim
