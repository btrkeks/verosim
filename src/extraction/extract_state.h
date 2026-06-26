#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "vrv_includes.h"

#include "verosim/extraction/effective_pitch.h"
#include "verosim/extraction/extract.h"
#include "verosim/model/duration.h"
#include "verosim/model/sym_score.h"

namespace verosim {

// One carrier event (chord = one event) or inline signature element,
// collected from the layers of one (measure, staff) in flat order.
struct Event {
    const vrv::Object *obj = nullptr;
    Fraction offset;
    Fraction dur_ql;
    std::string dur_type; // m21 type name; "complex" when inexpressible
    int dots = 0;
    Fraction type_num;
    bool is_rest = false;
    std::string grace_type;
    bool grace_slash = false;
    const vrv::Object *beam = nullptr; // innermost enclosing Beam; beamSpan is resolved later
    std::vector<const vrv::Tuplet *> tuplets; // outermost first
    std::vector<std::string> articulations;
};

struct PendingSig {
    // heap copies from GetClefCopy()/... (attribute forms materialized)
    std::shared_ptr<const vrv::Clef> clef;
    std::shared_ptr<const vrv::KeySig> keysig;
    std::shared_ptr<const vrv::Mensur> mensur;
    std::shared_ptr<const vrv::MeterSig> metersig;
    bool staffdef_visible_meter_hides_mensur = false;
};

struct StaffState {
    int part_idx = -1;
    AccidentalState accids;
    Fraction meter_ql = Fraction(4); // governing measure length
    Fraction beat_ql = Fraction(1); // QL represented by one MEI timestamp beat
    bool has_meter = false;
    int measure_idx = 0; // source/rendered measure order, including metric-dropped empty measures
    Fraction score_offset; // absolute QL offset at the start of the current measure
    PendingSig pending; // signature extras to emit at offset 0 of the next measure
};

struct EventLocation {
    std::string staff_n;
    Fraction abs_offset;
    Fraction dur_ql;
};

struct PendingSpan {
    std::string extra_id;
    std::string end_id;
    Fraction start_abs;
};

struct PendingLayoutBreak {
    ExtraKind kind = ExtraKind::kSystemBreak;
    std::string vrv_id;
};

struct EmittedExtraLocation {
    int part_idx = -1;
    std::size_t measure_idx = 0;
    std::size_t extra_idx = 0;
};

enum class BarlineLocation { kLeft, kRight, kMiddle };

struct ExtractionOutput {
    ExtractResult result;
    std::set<std::string> warned;

    SymScore &score() { return result.score; }
    const SymScore &score() const { return result.score; }

    void Warn(const std::string &message)
    {
        if (warned.insert(message).second) result.warnings.push_back(message);
    }
};

struct StaffRegistry {
    bool Contains(const std::string &staff_n) const { return by_number_.contains(staff_n); }

    StaffState &EnsurePart(const std::string &staff_n, SymScore &score)
    {
        auto found = by_number_.find(staff_n);
        if (found != by_number_.end()) return found->second;

        StaffState &state = by_number_[staff_n]; // default-construct
        order_.push_back(staff_n);
        score.parts.emplace_back();
        score.parts.back().staff_n = staff_n;
        state.part_idx = static_cast<int>(score.parts.size()) - 1;
        return state;
    }

    StaffState &At(const std::string &staff_n) { return by_number_.at(staff_n); }
    const StaffState &At(const std::string &staff_n) const { return by_number_.at(staff_n); }

    const std::vector<std::string> &OrderedNumbers() const { return order_; }
    bool empty() const { return order_.empty(); }
    const std::string &front() const { return order_.front(); }

    void AssignPartIndices(SymScore &score) const
    {
        for (std::size_t i = 0; i < order_.size(); ++i) {
            score.parts[i].part_idx = static_cast<int>(i);
        }
    }

private:
    std::map<std::string, StaffState> by_number_; // staff @n -> state
    std::vector<std::string> order_;
};

struct TieRegistry {
    // note ids referenced by tie control elements. A <tie> is a child of the
    // measure where the tie STARTS, but its @endid may point into the next
    // measure, so both sets persist across measures; entries are consumed
    // when the referenced note is resolved (MakeSymNote).
    void AddEndRef(std::string ref) { AddRef(std::move(ref), end_ids_); }
    void AddStartRef(std::string ref) { AddRef(std::move(ref), start_ids_); }
    bool ConsumeEndId(const std::string &id) { return end_ids_.erase(id) > 0; }
    bool ConsumeStartId(const std::string &id) { return start_ids_.erase(id) > 0; }

private:
    std::set<std::string> end_ids_;
    std::set<std::string> start_ids_;

    static void AddRef(std::string ref, std::set<std::string> &ids)
    {
        if (!ref.empty() && ref[0] == '#') ref.erase(0, 1);
        if (!ref.empty()) ids.insert(std::move(ref));
    }
};

struct ControlSpanState {
    void RegisterEventLocation(const std::string &id, const EventLocation &loc)
    {
        if (!id.empty()) event_locations_[id] = loc;
    }

    const EventLocation *FindEvent(const std::string &id) const
    {
        const auto it = event_locations_.find(id);
        return it == event_locations_.end() ? nullptr : &it->second;
    }

    bool HasEvent(const std::string &id) const { return event_locations_.contains(id); }
    void AddPendingSpan(PendingSpan span) { pending_spans_.push_back(std::move(span)); }

    void ResolvePendingSpans(SymScore &score)
    {
        auto pending = pending_spans_.begin();
        while (pending != pending_spans_.end()) {
            const EventLocation *loc = FindEvent(pending->end_id);
            if (!loc) {
                ++pending;
                continue;
            }
            Fraction duration = loc->abs_offset + loc->dur_ql - pending->start_abs;
            if (duration < Fraction(0)) duration = Fraction(0);
            const auto extra_loc = emitted_extra_locations_.find(pending->extra_id);
            if (extra_loc != emitted_extra_locations_.end()) {
                const EmittedExtraLocation &extra = extra_loc->second;
                score.parts[extra.part_idx]
                    .bar_list[extra.measure_idx]
                    .extras[extra.extra_idx]
                    .duration = duration;
            }
            pending = pending_spans_.erase(pending);
        }
    }

    void RegisterEmittedExtra(
        const std::string &id, int part_idx, std::size_t measure_idx, std::size_t extra_idx)
    {
        if (!id.empty()) {
            emitted_extra_locations_.emplace(
                id, EmittedExtraLocation{ part_idx, measure_idx, extra_idx });
        }
    }

private:
    std::map<std::string, EventLocation> event_locations_;
    std::vector<PendingSpan> pending_spans_;
    std::map<std::string, EmittedExtraLocation> emitted_extra_locations_;
};

struct LayoutBreakState {
    void AddSystemBreak(std::string vrv_id)
    {
        pending_breaks_.push_back(PendingLayoutBreak{
            .kind = ExtraKind::kSystemBreak,
            .vrv_id = std::move(vrv_id),
        });
    }

    bool empty() const { return pending_breaks_.empty(); }
    void Clear() { pending_breaks_.clear(); }

    std::vector<PendingLayoutBreak> TakeForPart(int part_idx)
    {
        if (part_idx != 0) return {};
        std::vector<PendingLayoutBreak> breaks = std::move(pending_breaks_);
        pending_breaks_.clear();
        return breaks;
    }

private:
    std::vector<PendingLayoutBreak> pending_breaks_;
};

} // namespace verosim
