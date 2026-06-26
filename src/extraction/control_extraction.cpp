#include "extractor.h"

#include <optional>
#include <string>
#include <utility>
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
    const Fraction start_abs = staves_.At(staffN).score_offset;
    for (const Event &ev : events) {
        const EventLocation loc{ staffN, start_abs + ev.offset, ev.dur_ql };
        controls_.RegisterEventLocation(ev.obj->GetID(), loc);
        if (ev.obj->Is(vrv::CHORD)) {
            for (const vrv::Object *child : ev.obj->GetChildren()) {
                if (child->Is(vrv::NOTE) && !child->GetID().empty()) {
                    controls_.RegisterEventLocation(child->GetID(), loc);
                }
            }
        }
    }
}

Fraction Extractor::TstampToOffset(double tstamp, const std::string &staffN) const
{
    return FractionFromDouble(tstamp - 1.0) * staves_.At(staffN).beat_ql;
}

Fraction Extractor::Tstamp2ToOffset(
    const vrv::data_MEASUREBEAT &tstamp2, const std::string &staffN) const
{
    const StaffState &state = staves_.At(staffN);
    return state.meter_ql * Fraction(tstamp2.first)
        + FractionFromDouble(tstamp2.second - 1.0) * state.beat_ql;
}

std::optional<Fraction> Extractor::ControlPointOffset(const vrv::Object *obj,
    const vrv::TimePointInterface *tpi, const std::string &staffN, const Fraction &startAbs)
{
    if (!tpi) return std::nullopt;
    if (tpi->HasStartid()) {
        const EventLocation *event = controls_.FindEvent(StripIdRef(tpi->GetStartid()));
        if (event && event->staff_n == staffN) {
            return event->abs_offset - startAbs;
        }
        if (event) return std::nullopt;
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
        const EventLocation *event = controls_.FindEvent(StripIdRef(tsi->GetEndid()));
        if (event && event->staff_n == staffN) {
            Fraction dur = event->abs_offset + event->dur_ql - (startAbs + start);
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
    const Fraction start_abs = staves_.At(staffN).score_offset;
    (void)events;
    const auto emit_spanning_extra = [&](const vrv::Object *obj,
                                         const vrv::TimePointInterface *timePoint,
                                         const vrv::TimeSpanningInterface *timeSpan,
                                         auto makeExtra) {
        const auto off = ControlPointOffset(obj, timePoint, staffN, start_abs);
        if (!off) return;
        std::optional<SymExtra> extra = makeExtra(
            *off, ControlSpanDuration(timeSpan, staffN, start_abs, *off));
        if (!extra) return;
        const std::string extra_id = extra->vrv_id;
        extras.push_back(std::move(*extra));
        if (timeSpan && timeSpan->HasEndid()) {
            const std::string end_id = StripIdRef(timeSpan->GetEndid());
            if (!controls_.HasEvent(end_id)) {
                controls_.AddPendingSpan({ extra_id, end_id, start_abs + *off });
            }
        }
    };

    for (const vrv::Object *obj : measure->GetChildren()) {
        if (!StaffMatches(obj, staffN)) continue;
        const vrv::TimePointInterface *timePoint = obj->GetTimePointInterface();
        if (!HasStaffIdent(obj) && (!timePoint || !timePoint->HasStartid())
            && !staves_.empty() && staffN != staves_.front()) {
            continue;
        }
        if (MetricModeIncludesDirections(options_.surface.mode) && obj->Is(vrv::DYNAM)) {
            const vrv::Dynam *dynam = vrv_cast<const vrv::Dynam *>(obj);
            if (auto off = ControlPointOffset(obj, dynam->GetTimePointInterface(), staffN, start_abs)) {
                extras.push_back(MakeDynamicExtra(*dynam, *off));
            }
        }
        else if (MetricModeIncludesDirections(options_.surface.mode) && obj->Is(vrv::HAIRPIN)) {
            const vrv::Hairpin *hairpin = vrv_cast<const vrv::Hairpin *>(obj);
            emit_spanning_extra(obj, hairpin->GetTimePointInterface(),
                hairpin->GetTimeSpanningInterface(),
                [&](const Fraction &offset, const std::optional<Fraction> &duration) {
                    return std::optional<SymExtra>(MakeHairpinExtra(*hairpin, offset, duration));
                });
        }
        else if (obj->Is(vrv::SLUR)) {
            const vrv::Slur *slur = vrv_cast<const vrv::Slur *>(obj);
            emit_spanning_extra(obj, slur->GetTimePointInterface(), slur->GetTimeSpanningInterface(),
                [&](const Fraction &offset, const std::optional<Fraction> &duration) {
                    return std::optional<SymExtra>(MakeSlurExtra(*slur, offset, duration));
                });
        }
        else if (MetricModeIncludesOttavas(options_.surface.mode) && obj->Is(vrv::OCTAVE)) {
            const vrv::Octave *octave = vrv_cast<const vrv::Octave *>(obj);
            emit_spanning_extra(obj, octave->GetTimePointInterface(), octave->GetTimeSpanningInterface(),
                [&](const Fraction &offset, const std::optional<Fraction> &duration) {
                    return MakeOttavaExtra(*octave, offset, duration);
                });
        }
    }
}

} // namespace verosim
