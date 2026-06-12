#include "verosim/extraction/notation_rules.h"

#include <algorithm>

namespace verosim {

namespace {

// number of beams/flags implied by a duration type: log2(type_num / 4)
int FlagCount(const Fraction &typeNum)
{
    if (!(typeNum > Fraction(4))) return 0;
    int count = 0;
    Fraction n = typeNum;
    while (n > Fraction(4)) {
        n = n / Fraction(2);
        ++count;
    }
    return count;
}

} // namespace

std::vector<std::vector<BeamValue>> DeriveBeamTypes(const std::vector<BeamMember> &members)
{
    std::vector<std::vector<BeamValue>> out(members.size());

    int maxDepth = 0;
    for (const BeamMember &m : members) {
        if (!m.is_rest) maxDepth = std::max(maxDepth, m.n_beams);
    }

    for (int depth = 1; depth <= maxDepth; ++depth) {
        // runs of consecutive notes carrying >= depth beams; rests are
        // transparent (importers beam straight across them); @breaksec ends
        // the run for depths above its value.
        std::size_t i = 0;
        while (i < members.size()) {
            if (members[i].is_rest || members[i].n_beams < depth) {
                ++i;
                continue;
            }
            // collect the run [i..j) of participating notes, remembering the
            // note indices (rests in between stay transparent)
            std::vector<std::size_t> run;
            std::size_t j = i;
            while (j < members.size()) {
                if (members[j].is_rest) {
                    ++j;
                    continue;
                }
                if (members[j].n_beams < depth) break;
                run.push_back(j);
                const bool breaksHere = members[j].breaksec > 0 && depth > members[j].breaksec;
                ++j;
                if (breaksHere) break;
            }
            if (run.size() == 1) {
                // a lone beam at this depth is a partial hook (depth 1 inside
                // a Beam container cannot be lone unless neighbors are rests
                // only; music21 renders that as partial too)
                out[run[0]].push_back(BeamValue::kPartial);
            }
            else {
                for (std::size_t k = 0; k < run.size(); ++k) {
                    out[run[k]].push_back(k == 0 ? BeamValue::kStart
                            : k + 1 == run.size() ? BeamValue::kStop
                                                  : BeamValue::kContinue);
                }
            }
            i = j;
        }
    }
    return out;
}

std::vector<std::vector<BeamValue>> EnhanceBeamings(const std::vector<RawEvent> &events)
{
    // Port of get_enhance_beamings (m21utils.py:468-556). The Python builds
    // _mod_beam_list and new_mod_beam_list with copy.copy(), which SHARES the
    // inner per-note lists — so its second pass reads values it has already
    // overwritten. We replicate that with one array mutated in place, in the
    // exact iteration order.
    const std::size_t n = events.size();
    std::vector<std::vector<BeamValue>> beams(n);
    for (std::size_t i = 0; i < n; ++i) beams[i] = events[i].raw_beams;

    // Pass 1: flags for entries that have no beam list. The beamed-rest
    // lookahead reads the NEXT entry, which forward iteration has not
    // appended to yet — i.e. it sees the raw list, like the Python.
    for (std::size_t i = 0; i < n; ++i) {
        if (!beams[i].empty()) continue;
        const int rangeEnd = FlagCount(events[i].type_num);
        for (int ii = 0; ii < rangeEnd; ++ii) {
            const bool beamedRest = events[i].is_rest && i + 1 < n
                && static_cast<int>(beams[i + 1].size()) > ii
                && (beams[i + 1][ii] == BeamValue::kContinue || beams[i + 1][ii] == BeamValue::kStop);
            beams[i].push_back(beamedRest ? BeamValue::kContinue : BeamValue::kPartial);
        }
    }

    // Pass 2: lone start/stop/continue fixups (safe_get pass), depth-major,
    // reading the same (already partially fixed) array like the Python does.
    std::size_t maxLen = 0;
    for (const auto &b : beams) maxLen = std::max(maxLen, b.size());

    const auto get = [&beams, n](std::ptrdiff_t idx, std::size_t depth) -> std::optional<BeamValue> {
        if (idx < 0 || idx >= static_cast<std::ptrdiff_t>(n)) return std::nullopt;
        if (depth >= beams[static_cast<std::size_t>(idx)].size()) return std::nullopt;
        return beams[static_cast<std::size_t>(idx)][depth];
    };

    for (std::size_t depth = 0; depth < maxLen; ++depth) {
        for (std::size_t i = 0; i < n; ++i) {
            const auto cur = get(static_cast<std::ptrdiff_t>(i), depth);
            const auto prev = get(static_cast<std::ptrdiff_t>(i) - 1, depth);
            const auto next = get(static_cast<std::ptrdiff_t>(i) + 1, depth);
            if (cur == BeamValue::kStart && !next.has_value()) {
                beams[i][depth] = BeamValue::kPartial;
            }
            else if (cur == BeamValue::kStop && !prev.has_value()) {
                beams[i][depth] = BeamValue::kPartial;
            }
            else if (cur == BeamValue::kContinue && !prev.has_value() && !next.has_value()) {
                beams[i][depth] = BeamValue::kPartial;
            }
            else if (cur == BeamValue::kContinue && !prev.has_value() && next.has_value()) {
                beams[i][depth] = BeamValue::kStart;
            }
        }
    }
    return beams;
}

std::vector<std::vector<TupletValue>> CorrectTuplets(const std::vector<RawEvent> &events)
{
    // Port of get_tuplets_type (m21utils.py:637-672).
    const std::size_t n = events.size();
    std::vector<std::vector<TupletValue>> out(n);
    std::size_t maxLen = 0;
    for (const RawEvent &e : events) maxLen = std::max(maxLen, e.raw_tuplets.size());

    for (std::size_t i = 0; i < n; ++i) {
        out[i].resize(events[i].raw_tuplets.size(), TupletValue::kContinue);
        for (std::size_t k = 0; k < events[i].raw_tuplets.size(); ++k) {
            if (events[i].raw_tuplets[k].has_value()) out[i][k] = *events[i].raw_tuplets[k];
        }
    }

    for (std::size_t ii = 0; ii < maxLen; ++ii) {
        bool open = false; // the Python's start_index, used only as a flag
        for (std::size_t i = 0; i < n; ++i) {
            if (events[i].raw_tuplets.size() <= ii) continue;
            const std::optional<TupletValue> &t = events[i].raw_tuplets[ii];
            if (!t.has_value()) {
                if (!open) {
                    open = true;
                    out[i][ii] = TupletValue::kStart;
                }
                else {
                    out[i][ii] = TupletValue::kContinue;
                }
            }
            else if (*t == TupletValue::kStart) {
                open = true;
            }
            else if (*t == TupletValue::kStop || *t == TupletValue::kStartStop) {
                open = false;
            }
        }
    }
    return out;
}

std::vector<std::vector<std::string>> TupletInfo(const std::vector<RawEvent> &events)
{
    // Port of get_tuplets_info without Style (m21utils.py:586-633): the
    // number string on RAW start entries, "" on everything else.
    std::vector<std::vector<std::string>> out(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        out[i].resize(events[i].raw_tuplets.size());
        for (std::size_t k = 0; k < events[i].raw_tuplets.size(); ++k) {
            if (events[i].raw_tuplets[k] == TupletValue::kStart) {
                out[i][k] = events[i].tuplet_nums[k];
            }
        }
    }
    return out;
}

} // namespace verosim
