#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "verosim/model/sym_score.h"

namespace verosim {

// String -> dense id with exact-equality resolution. Replaces every place the
// Python code compares content via hash(str) (AnnMeasure.precomputed_str/repr,
// the int64 arrays fed to the Myers diff): equal id <=> equal string, so the
// engine never compares raw hashes and the Python hash-collision corner cannot
// exist here. One instance is shared by both scores of a comparison so content
// equality across scores is id equality.
class StringInterner {
public:
    std::int32_t Intern(const std::string &s)
    {
        const auto [it, inserted] = ids_.try_emplace(s, static_cast<std::int32_t>(ids_.size()));
        return it->second;
    }
    std::size_t size() const { return ids_.size(); }

private:
    std::unordered_map<std::string, std::int32_t> ids_;
};

// AnnExtra.__str__-equivalent comparison key (annotation.py:837-859) over the
// fields materialized at v1 tiers: kind, content, symbolic, offset, duration,
// infodict. Not byte-compatible with Python (both operands are C++-side);
// equality semantics are what must match.
std::string ExtraComparisonKey(const SymExtra &extra);

// Per-comparison precompute: interned content ids and cached notation sizes,
// so the diff never re-touches strings on hot paths.
struct PreparedMeasure {
    std::int32_t content_id = -1; // AnnMeasure.__str__ equivalent
    std::vector<std::int32_t> note_str_ids; // parallel to SymMeasure::notes
    std::vector<std::int32_t> extra_str_ids; // parallel to SymMeasure::extras
    long notation_size = 0;
};

struct PreparedPart {
    std::vector<PreparedMeasure> measures; // parallel to SymPart::bar_list
};

struct PreparedScore {
    const SymScore *score = nullptr;
    std::vector<PreparedPart> parts; // parallel to SymScore::parts
};

PreparedScore PrepareScore(const SymScore &score, StringInterner &interner);

} // namespace verosim
