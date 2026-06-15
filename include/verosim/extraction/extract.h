#pragma once

#include <string>
#include <vector>

#include "verosim/model/detail_tier.h"
#include "verosim/model/sym_score.h"

namespace vrv {
class Doc;
}

namespace verosim {

// Source-format quirks the MEI tree cannot express uniformly (currently only
// the grace-slash convention, see docs/symbol_mapping.md "Grace notes").
enum class SourceFormat { kKern, kMusicXml, kOther };

struct ExtractResult {
    SymScore score;
    // Non-fatal anomalies (unknown elements skipped, unmapped accidentals,
    // resolver/display cross-check mismatches). Data for triage, not errors.
    std::vector<std::string> warnings;
};

struct ExtractOptions {
    DetailTier detail = DetailTier::kTierA; // count/extraction default remains Tier A
};

// Walk the parse-only Verovio Object tree of the first <score> into the
// Layer 2 SymScore (Tier A symbols). Tree shape and per-element mapping are
// documented in docs/symbol_mapping.md.
ExtractResult ExtractSymScore(
    vrv::Doc &doc, SourceFormat format, const ExtractOptions &options = ExtractOptions());

} // namespace verosim
