#include "verosim/engine/myers.h"

#include <cassert>
#include <map>
#include <utility>

namespace verosim {

namespace {

// History step: (0, original index) | (1, compare_to index) | (2, original
// index) for equal, matching the tuples _myers_diff appends.
struct Step {
    std::int8_t op;
    int payload;
};

// Comparison._myers_diff (comparison.py:157-218). The frontier dict is keyed
// on diagonal k; the Python access pattern only ever reads keys written at
// the previous d (or the {1: (0, [])} seed), which .at() enforces here.
// TODO(consolidate): the per-step history copies are O(d^2) memory like the
// Python original; if profiling flags this, switch to parent-pointer
// reconstruction.
std::vector<Step> MyersDiff(
    const std::vector<std::int32_t> &a_lines, const std::vector<std::int32_t> &b_lines)
{
    struct Frontier {
        int x = 0;
        std::vector<Step> history;
    };
    std::map<int, Frontier> frontier;
    frontier[1] = Frontier{ 0, {} };

    const int a_max = static_cast<int>(a_lines.size());
    const int b_max = static_cast<int>(b_lines.size());
    for (int d = 0; d <= a_max + b_max; ++d) {
        for (int k = -d; k <= d; k += 2) {
            const bool go_down
                = (k == -d) || (k != d && frontier.at(k - 1).x < frontier.at(k + 1).x);

            int x;
            std::vector<Step> history;
            if (go_down) {
                x = frontier.at(k + 1).x;
                history = frontier.at(k + 1).history; // copy: others may reuse it
            }
            else {
                x = frontier.at(k - 1).x + 1;
                history = frontier.at(k - 1).history;
            }
            int y = x - k;

            if (1 <= y && y <= b_max && go_down) {
                history.push_back({ 1, y - 1 }); // compare_to step
            }
            else if (1 <= x && x <= a_max) {
                history.push_back({ 0, x - 1 }); // original step
            }

            while (x < a_max && y < b_max && a_lines[x] == b_lines[y]) {
                ++x;
                ++y;
                history.push_back({ 2, x - 1 }); // equal step
            }

            if (x >= a_max && y >= b_max) return history;

            frontier[k] = Frontier{ x, std::move(history) };
        }
    }
    assert(false && "Could not find edit script");
    return {};
}

} // namespace

std::vector<NcsBlock> NonCommonSubsequences(
    const std::vector<std::int32_t> &original_ids, const std::vector<std::int32_t> &compare_to_ids)
{
    // _non_common_subsequences_myers (comparison.py:220-247): the double
    // [::-1] is a net no-op, so the history is consumed in forward order.
    const std::vector<Step> history = MyersDiff(original_ids, compare_to_ids);
    std::vector<NcsBlock> blocks;
    blocks.emplace_back();
    for (const Step &step : history) {
        switch (step.op) {
            case 2: blocks.emplace_back(); break;
            case 0: blocks.back().original.push_back(step.payload); break;
            case 1: blocks.back().compare_to.push_back(step.payload); break;
        }
    }
    std::erase_if(
        blocks, [](const NcsBlock &b) { return b.original.empty() && b.compare_to.empty(); });
    return blocks;
}

} // namespace verosim
