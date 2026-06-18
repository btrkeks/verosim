#include "extract_internal.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

namespace verosim {

using namespace extract_detail;

ExtractResult Extractor::Run()
{
    bool sawScore = false;
    WalkPageTree(&doc_, sawScore);

    // drop empty measures (annotation.py:1401) — already skipped at emit time;
    // assign part indices in staffDef document order
    for (std::size_t i = 0; i < staff_order_.size(); ++i) {
        result_.score.parts[i].part_idx = static_cast<int>(i);
    }
    return result_;
}

void Extractor::WalkPageTree(const vrv::Object *obj, bool &sawScore)
{
    for (const vrv::Object *child : obj->GetChildren()) {
        if (child->Is(vrv::SCORE)) {
            if (sawScore) {
                // multi-movement file: the oracle compares scores[0] only
                Warn("multiple <score> milestones; only the first is extracted");
                return;
            }
            sawScore = true;
            ApplyInitialScoreDef(vrv_cast<const vrv::Score *>(child));
        }
        else if (child->Is(vrv::SCOREDEF)) {
            ApplyScoreDefChange(vrv_cast<const vrv::ScoreDef *>(child));
        }
        else if (child->Is(vrv::MEASURE)) {
            HandleMeasure(vrv_cast<const vrv::Measure *>(child));
        }
        else {
            WalkPageTree(child, sawScore);
        }
    }
}

void Extractor::ApplyInitialScoreDef(const vrv::Score *score)
{
    const vrv::ScoreDef *scoreDef = score->GetScoreDef();
    if (!scoreDef) {
        Warn("score has no scoreDef");
        return;
    }
    for (const vrv::Object *obj : scoreDef->FindAllDescendantsByType(vrv::STAFFDEF)) {
        ApplyStaffDef(vrv_cast<const vrv::StaffDef *>(obj), scoreDef);
    }
}

void Extractor::ApplyScoreDefChange(const vrv::ScoreDef *scoreDef)
{
    // Mid-score change. Components at scoreDef level apply to every staff;
    // components inside a staffDef apply to that staff only (both forms occur
    // in one element — see docs/symbol_mapping.md "Tree shape").
    std::unique_ptr<const vrv::Clef> clef(scoreDef->HasClefInfo(1) ? scoreDef->GetClefCopy() : nullptr);
    std::unique_ptr<const vrv::KeySig> keysig(
        scoreDef->HasKeySigInfo(1) ? scoreDef->GetKeySigCopy() : nullptr);
    std::unique_ptr<const vrv::MeterSig> metersig(
        scoreDef->HasMeterSigInfo(1) ? scoreDef->GetMeterSigCopy() : nullptr);

    if (clef || keysig || metersig) {
        std::shared_ptr<const vrv::Clef> sharedClef(std::move(clef));
        std::shared_ptr<const vrv::KeySig> sharedKeySig(std::move(keysig));
        std::shared_ptr<const vrv::MeterSig> sharedMeterSig(std::move(metersig));
        for (const std::string &n : staff_order_) {
            StaffState &state = staves_[n];
            if (sharedClef) state.pending.clef = sharedClef;
            if (sharedKeySig) state.pending.keysig = sharedKeySig;
            if (sharedMeterSig) state.pending.metersig = sharedMeterSig;
        }
    }

    // pass the scoreDef as parent so a staffDef first seen here (a staff
    // introduced mid-score) inherits scoreDef-level components too; existing
    // staves just get an equivalent copy of what the loop above already set
    for (const vrv::Object *obj : scoreDef->FindAllDescendantsByType(vrv::STAFFDEF)) {
        ApplyStaffDef(vrv_cast<const vrv::StaffDef *>(obj), scoreDef);
    }
}

void Extractor::ApplyStaffDef(const vrv::StaffDef *staffDef, const vrv::ScoreDef *parentScoreDef)
{
    const std::string n = std::to_string(staffDef->GetN());
    StaffState &state = EnsureStaffPart(n, false);

    const auto component = [&](auto hasInfo, auto getCopy) {
        using Ptr = decltype((staffDef->*getCopy)());
        if ((staffDef->*hasInfo)(1)) return Ptr((staffDef->*getCopy)());
        if (parentScoreDef && (parentScoreDef->*hasInfo)(1)) return Ptr((parentScoreDef->*getCopy)());
        return Ptr(nullptr);
    };

    std::unique_ptr<const vrv::Clef> clef(
        component(&vrv::ScoreDefElement::HasClefInfo, &vrv::ScoreDefElement::GetClefCopy));
    std::unique_ptr<const vrv::KeySig> keysig(
        component(&vrv::ScoreDefElement::HasKeySigInfo, &vrv::ScoreDefElement::GetKeySigCopy));
    std::unique_ptr<const vrv::MeterSig> metersig(
        component(&vrv::ScoreDefElement::HasMeterSigInfo, &vrv::ScoreDefElement::GetMeterSigCopy));

    if (clef) state.pending.clef = std::shared_ptr<const vrv::Clef>(std::move(clef));
    if (keysig) state.pending.keysig = std::shared_ptr<const vrv::KeySig>(std::move(keysig));
    if (metersig) state.pending.metersig = std::shared_ptr<const vrv::MeterSig>(std::move(metersig));
}

StaffState &Extractor::EnsureStaffPart(const std::string &staffN, bool warnIfImplicit)
{
    auto found = staves_.find(staffN);
    if (found != staves_.end()) return found->second;

    if (warnIfImplicit) {
        Warn("staff n=" + staffN + " has no staffDef; creating an implicit part");
    }
    StaffState &state = staves_[staffN]; // default-construct
    staff_order_.push_back(staffN);
    result_.score.parts.emplace_back();
    result_.score.parts.back().staff_n = staffN;
    state.part_idx = static_cast<int>(result_.score.parts.size()) - 1;
    return state;
}

void Extractor::StartStaffMeasure(StaffState &state)
{
    state.accids.StartMeasure();
    if (state.pending.keysig) {
        state.accids.SetKeySig([&] {
            const int count = state.pending.keysig->GetAccidCount();
            return state.pending.keysig->GetAccidType() == vrv::ACCIDENTAL_WRITTEN_f ? -count
                                                                                     : count;
        }());
    }
}

void Extractor::ApplyPendingSignatures(StaffState &state, std::vector<SymExtra> &extras)
{
    if (state.pending.clef) extras.push_back(MakeClefExtra(*state.pending.clef, Fraction(0)));
    if (state.pending.keysig) {
        extras.push_back(MakeKeySigExtra(*state.pending.keysig, Fraction(0)));
    }
    if (state.pending.metersig) {
        extras.push_back(MakeMeterSigExtra(*state.pending.metersig, Fraction(0)));
        if (MeterQLFromMeterSig(*state.pending.metersig, state.meter_ql)) {
            state.has_meter = true;
        }
        state.beat_ql = BeatQLFromMeterSig(*state.pending.metersig);
    }
    state.pending = PendingSig();
}

void Extractor::CollectStaffLayerEvents(const vrv::Staff *staff, std::vector<Event> &events,
    std::vector<SymExtra> &extras, StaffState &state)
{
    for (const vrv::Object *staffChild : staff->GetChildren()) {
        if (!staffChild->Is(vrv::LAYER)) continue;
        Fraction cursor(0);
        std::vector<const vrv::Tuplet *> tupletStack;
        CollectLayerEvents(staffChild, events, extras, cursor, state, tupletStack, nullptr);
    }
}

Fraction Extractor::MeasureSpan(const StaffState &state, const std::vector<Event> &events) const
{
    Fraction measureSpan = state.meter_ql;
    for (const Event &ev : events) {
        const Fraction relEnd = ev.offset + ev.dur_ql;
        if (relEnd > measureSpan) measureSpan = relEnd;
    }
    return measureSpan;
}

void Extractor::FinishStaffMeasure(StaffState &state, const Fraction &measureSpan)
{
    state.score_offset = state.score_offset + measureSpan;
    ++state.measure_idx;
    ResolvePendingSpans();
}

void Extractor::HandleMeasure(const vrv::Measure *measure)
{
    // tie control elements: @startid notes carry the sounding alter across
    // the barline (RegisterTieStart), @endid notes are tied-from-previous
    const auto addRef = [](const std::string &ref, std::set<std::string> &ids) {
        std::string id = ref;
        if (!id.empty() && id[0] == '#') id.erase(0, 1);
        if (!id.empty()) ids.insert(std::move(id));
    };
    for (const vrv::Object *child : measure->GetChildren()) {
        if (!child->Is(vrv::TIE)) continue;
        const vrv::TimeSpanningInterface *interface = child->GetTimeSpanningInterface();
        if (!interface) continue;
        if (interface->HasEndid()) addRef(interface->GetEndid(), tie_end_ids_);
        if (interface->HasStartid()) addRef(interface->GetStartid(), tie_start_ids_);
    }

    for (const vrv::Object *child : measure->GetChildren()) {
        if (!child->Is(vrv::STAFF)) continue;
        const vrv::Staff *staff = vrv_cast<const vrv::Staff *>(child);
        const std::string n = std::to_string(staff->GetN());
        StaffState &state = EnsureStaffPart(n, true);
        StartStaffMeasure(state);
        std::vector<Event> events;
        std::vector<SymExtra> extras;
        ApplyPendingSignatures(state, extras);
        CollectStaffLayerEvents(staff, events, extras, state);
        RegisterEventLocations(n, events);
        CollectControlExtras(measure, n, events, extras);
        const Fraction measureSpan = MeasureSpan(state, events);
        EmitMeasure(measure, n, std::move(events), std::move(extras), state);
        FinishStaffMeasure(state, measureSpan);
    }
}

ExtractResult ExtractSymScore(vrv::Doc &doc, SourceFormat format, const ExtractOptions &options)
{
    Extractor extractor(doc, format, options);
    return extractor.Run();
}

} // namespace verosim
