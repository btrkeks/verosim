#include "extract_internal.h"

#include <optional>
#include <string>
#include <vector>

namespace verosim {

using namespace extract_detail;

namespace extract_detail {

std::string StripIdRef(std::string ref)
{
    if (!ref.empty() && ref[0] == '#') ref.erase(0, 1);
    return ref;
}

bool StaffMatches(const vrv::Object *obj, const std::string &staffN)
{
    const auto *staffIdent = dynamic_cast<const vrv::AttStaffIdent *>(obj);
    if (!staffIdent || !staffIdent->HasStaff()) return true;
    for (const int n : staffIdent->GetStaff()) {
        if (std::to_string(n) == staffN) return true;
    }
    return false;
}

bool HasStaffIdent(const vrv::Object *obj)
{
    const auto *staffIdent = dynamic_cast<const vrv::AttStaffIdent *>(obj);
    return staffIdent && staffIdent->HasStaff();
}

} // namespace extract_detail

void Extractor::RegisterEventLocations(const std::string &staffN, const std::vector<Event> &events)
{
    const Fraction start_abs = staves_[staffN].score_offset;
    for (const Event &ev : events) {
        const EventLocation loc{ staffN, start_abs + ev.offset, ev.dur_ql };
        if (!ev.obj->GetID().empty()) event_locations_[ev.obj->GetID()] = loc;
        if (ev.obj->Is(vrv::CHORD)) {
            for (const vrv::Object *child : ev.obj->GetChildren()) {
                if (child->Is(vrv::NOTE) && !child->GetID().empty()) {
                    event_locations_[child->GetID()] = loc;
                }
            }
        }
    }
}

void Extractor::ResolvePendingSpans()
{
    auto pending = pending_spans_.begin();
    while (pending != pending_spans_.end()) {
        const auto loc = event_locations_.find(pending->end_id);
        if (loc == event_locations_.end()) {
            ++pending;
            continue;
        }
        Fraction duration = loc->second.abs_offset + loc->second.dur_ql - pending->start_abs;
        if (duration < Fraction(0)) duration = Fraction(0);
        const auto extraLoc = emitted_extra_locations_.find(pending->extra_id);
        if (extraLoc != emitted_extra_locations_.end()) {
            const EmittedExtraLocation &extra = extraLoc->second;
            result_.score.parts[extra.part_idx]
                .bar_list[extra.measure_idx]
                .extras[extra.extra_idx]
                .duration = duration;
        }
        pending = pending_spans_.erase(pending);
    }
}

Fraction Extractor::TstampToOffset(double tstamp, const std::string &staffN) const
{
    return FractionFromDouble(tstamp - 1.0) * staves_.at(staffN).beat_ql;
}

Fraction Extractor::Tstamp2ToOffset(
    const vrv::data_MEASUREBEAT &tstamp2, const std::string &staffN) const
{
    const StaffState &state = staves_.at(staffN);
    return state.meter_ql * Fraction(tstamp2.first)
        + FractionFromDouble(tstamp2.second - 1.0) * state.beat_ql;
}

std::optional<Fraction> Extractor::ControlPointOffset(const vrv::Object *obj,
    const vrv::TimePointInterface *tpi, const std::string &staffN, const Fraction &startAbs)
{
    if (!tpi) return std::nullopt;
    if (tpi->HasStartid()) {
        const auto it = event_locations_.find(StripIdRef(tpi->GetStartid()));
        if (it != event_locations_.end() && it->second.staff_n == staffN) {
            return it->second.abs_offset - startAbs;
        }
        if (it != event_locations_.end()) return std::nullopt;
        if (!tpi->HasTstamp()) return std::nullopt;
    }
    if (tpi->HasTstamp()) {
        return TstampToOffset(tpi->GetTstamp(), staffN);
    }
    Warn("control element <" + obj->GetClassName() + "> lacks a resolvable start offset");
    return std::nullopt;
}

std::optional<Fraction> Extractor::ControlSpanDuration(const vrv::TimeSpanningInterface *tsi,
    const std::string &staffN, const Fraction &startAbs, const Fraction &start)
{
    if (!tsi) return std::nullopt;
    if (tsi->HasEndid()) {
        const auto it = event_locations_.find(StripIdRef(tsi->GetEndid()));
        if (it != event_locations_.end() && it->second.staff_n == staffN) {
            Fraction dur = it->second.abs_offset + it->second.dur_ql - (startAbs + start);
            if (dur < Fraction(0)) dur = Fraction(0);
            return dur;
        }
        return Fraction(0); // still counts the span duration symbol across measures
    }
    if (tsi->HasTstamp2()) {
        Fraction dur = Tstamp2ToOffset(tsi->GetTstamp2(), staffN) - start;
        if (dur < Fraction(0)) dur = Fraction(0);
        return dur;
    }
    return std::nullopt;
}

void Extractor::CollectControlExtras(const vrv::Measure *measure, const std::string &staffN,
    const std::vector<Event> &events, std::vector<SymExtra> &extras)
{
    const Fraction start_abs = staves_[staffN].score_offset;
    (void)events;

    for (const vrv::Object *obj : measure->GetChildren()) {
        if (!StaffMatches(obj, staffN)) continue;
        const vrv::TimePointInterface *timePoint = obj->GetTimePointInterface();
        if (!HasStaffIdent(obj) && (!timePoint || !timePoint->HasStartid())
            && !staff_order_.empty() && staffN != staff_order_.front()) {
            continue;
        }
        if (DetailIncludesDirections(options_.detail) && obj->Is(vrv::DYNAM)) {
            const vrv::Dynam *dynam = vrv_cast<const vrv::Dynam *>(obj);
            if (auto off = ControlPointOffset(obj, dynam->GetTimePointInterface(), staffN, start_abs)) {
                extras.push_back(MakeDynamicExtra(*dynam, *off));
            }
        }
        else if (DetailIncludesDirections(options_.detail) && obj->Is(vrv::HAIRPIN)) {
            const vrv::Hairpin *hairpin = vrv_cast<const vrv::Hairpin *>(obj);
            if (auto off
                = ControlPointOffset(obj, hairpin->GetTimePointInterface(), staffN, start_abs)) {
                extras.push_back(MakeHairpinExtra(*hairpin, *off,
                    ControlSpanDuration(hairpin->GetTimeSpanningInterface(), staffN, start_abs, *off)));
                if (hairpin->GetTimeSpanningInterface()->HasEndid()) {
                    const std::string end_id = StripIdRef(hairpin->GetTimeSpanningInterface()->GetEndid());
                    if (!event_locations_.contains(end_id)) {
                        pending_spans_.push_back({ hairpin->GetID(), end_id, start_abs + *off });
                    }
                }
            }
        }
        else if (DetailIncludesTierB(options_.detail) && obj->Is(vrv::SLUR)) {
            const vrv::Slur *slur = vrv_cast<const vrv::Slur *>(obj);
            if (auto off = ControlPointOffset(obj, slur->GetTimePointInterface(), staffN, start_abs)) {
                extras.push_back(MakeSlurExtra(*slur, *off,
                    ControlSpanDuration(slur->GetTimeSpanningInterface(), staffN, start_abs, *off)));
                if (slur->GetTimeSpanningInterface()->HasEndid()) {
                    const std::string end_id = StripIdRef(slur->GetTimeSpanningInterface()->GetEndid());
                    if (!event_locations_.contains(end_id)) {
                        pending_spans_.push_back({ slur->GetID(), end_id, start_abs + *off });
                    }
                }
            }
        }
    }
}

} // namespace verosim
