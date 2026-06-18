#pragma once

#include <optional>
#include <string>
#include <vector>

#include "verosim/visual/visual_mark.h"

namespace verosim {

class SvgSymbolIndex {
public:
    static SvgSymbolIndex Build(const std::string &svg, int measure_idx_offset = 0);

    bool parse_ok() const { return parse_ok_; }
    const std::string &error() const { return error_; }
    int measure_count() const { return measure_count_; }

    std::optional<SvgSelector> Resolve(const VisualSymbolRef &ref) const;

private:
    struct Candidate {
        VisualTargetKind kind = VisualTargetKind::kNote;
        SymbolLocator locator;
        SvgSelector selector;
        ExtraKind extra_kind = ExtraKind::kClef;
        bool has_extra_kind = false;
        std::optional<SvgSelector> accidental_selector;
    };

    std::optional<SvgSelector> ExactSelector(const std::string &id) const;
    std::optional<SvgSelector> StructuralSelector(const VisualSymbolRef &ref) const;
    const Candidate *FindCandidate(const VisualSymbolRef &ref) const;

    bool parse_ok_ = true;
    std::string error_;
    int measure_count_ = 0;
    std::vector<std::string> ids_;
    std::vector<std::string> class_tokens_;
    std::vector<Candidate> candidates_;
};

} // namespace verosim
