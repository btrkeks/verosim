#include "extractor.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace verosim {

using namespace extract_detail;

ExtractResult Extractor::Run()
{
    bool sawScore = false;
    WalkPageTree(&doc_, sawScore);
    if (!layout_.empty()) {
        Warn("layout system break not followed by a measure; dropping it");
        layout_.Clear();
    }

    // drop empty measures (annotation.py:1401) — already skipped at emit time;
    // assign part indices in staffDef document order
    staves_.AssignPartIndices(output_.score());
    return std::move(output_.result);
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
        else if (child->Is(vrv::SB)) {
            if (MetricSurfaceIncludesSystemBreaks(options_.surface)) {
                const vrv::Sb *sb = vrv_cast<const vrv::Sb *>(child);
                layout_.AddSystemBreak(sb ? sb->GetID() : child->GetID());
            }
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
    std::unique_ptr<const vrv::Mensur> mensur(
        scoreDef->HasMensurInfo(1) ? scoreDef->GetMensurCopy() : nullptr);
    std::unique_ptr<const vrv::MeterSig> metersig(
        scoreDef->HasMeterSigInfo(1) ? scoreDef->GetMeterSigCopy() : nullptr);

    if (clef || keysig || mensur || metersig) {
        std::shared_ptr<const vrv::Clef> sharedClef(std::move(clef));
        std::shared_ptr<const vrv::KeySig> sharedKeySig(std::move(keysig));
        std::shared_ptr<const vrv::Mensur> sharedMensur(std::move(mensur));
        std::shared_ptr<const vrv::MeterSig> sharedMeterSig(std::move(metersig));
        for (const std::string &n : staves_.OrderedNumbers()) {
            StaffState &state = staves_.At(n);
            if (sharedClef) state.pending.clef = sharedClef;
            if (sharedKeySig) state.pending.keysig = sharedKeySig;
            if (sharedMensur) {
                state.pending.mensur = sharedMensur;
                state.pending.staffdef_visible_meter_hides_mensur = false;
            }
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
    const bool staffDefHasMensur = staffDef->HasMensurInfo(1);
    const bool staffDefHasMeterSig = staffDef->HasMeterSigInfo(1);

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
    std::unique_ptr<const vrv::Mensur> mensur(
        component(&vrv::ScoreDefElement::HasMensurInfo, &vrv::ScoreDefElement::GetMensurCopy));
    std::unique_ptr<const vrv::MeterSig> metersig(
        component(&vrv::ScoreDefElement::HasMeterSigInfo, &vrv::ScoreDefElement::GetMeterSigCopy));
    const bool staffDefVisibleMeterHidesMensur
        = staffDefHasMensur && staffDefHasMeterSig && metersig && IsVisibleMeterSig(*metersig);

    if (clef) state.pending.clef = std::shared_ptr<const vrv::Clef>(std::move(clef));
    if (keysig) state.pending.keysig = std::shared_ptr<const vrv::KeySig>(std::move(keysig));
    if (mensur) {
        state.pending.mensur = std::shared_ptr<const vrv::Mensur>(std::move(mensur));
        state.pending.staffdef_visible_meter_hides_mensur = staffDefVisibleMeterHidesMensur;
    }
    if (metersig) state.pending.metersig = std::shared_ptr<const vrv::MeterSig>(std::move(metersig));
}

StaffState &Extractor::EnsureStaffPart(const std::string &staffN, bool warnIfImplicit)
{
    if (warnIfImplicit && !staves_.Contains(staffN)) {
        Warn("staff n=" + staffN + " has no staffDef; creating an implicit part");
    }
    return staves_.EnsurePart(staffN, output_.score());
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
        if (MeterQLFromMeterSig(*state.pending.metersig, state.meter_ql)) {
            state.has_meter = true;
        }
        state.beat_ql = BeatQLFromMeterSig(*state.pending.metersig);
    }
    const bool hasVisibleMeterSig
        = state.pending.metersig && IsVisibleMeterSig(*state.pending.metersig);
    const bool suppressMensur
        = hasVisibleMeterSig && state.pending.staffdef_visible_meter_hides_mensur;
    if (state.pending.mensur && ModernMensurSymbol(*state.pending.mensur) && !suppressMensur) {
        extras.push_back(MakeMensurTimeSigExtra(*state.pending.mensur, Fraction(0)));
        if (!state.pending.metersig && MeterQLFromMensur(*state.pending.mensur, state.meter_ql)) {
            state.has_meter = true;
            state.beat_ql = Fraction(1);
        }
    }
    if (hasVisibleMeterSig) {
        extras.push_back(MakeMeterSigExtra(*state.pending.metersig, Fraction(0)));
    }
    state.pending = PendingSig();
}

SymExtra Extractor::MakeSystemBreakExtra(
    const PendingLayoutBreak &layout_break, const vrv::Measure *measure) const
{
    SymExtra extra;
    extra.vrv_id = layout_break.vrv_id;
    if (extra.vrv_id.empty() && measure != nullptr && !measure->GetID().empty()) {
        extra.vrv_id = measure->GetID() + ":systembreak";
    }
    extra.kind = ExtraKind::kSystemBreak;
    extra.symbolic = "systembreak";
    extra.offset = Fraction(0);
    return extra;
}

void Extractor::ApplyPendingLayoutBreaks(
    const vrv::Measure *measure, StaffState &state, std::vector<SymExtra> &extras)
{
    for (const PendingLayoutBreak &layout_break : layout_.TakeForPart(state.part_idx)) {
        extras.push_back(MakeSystemBreakExtra(layout_break, measure));
    }
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
    controls_.ResolvePendingSpans(output_.score());
}

void Extractor::HandleMeasure(const vrv::Measure *measure)
{
    // tie control elements: @startid notes carry the sounding alter across
    // the barline (RegisterTieStart), @endid notes are tied-from-previous
    for (const vrv::Object *child : measure->GetChildren()) {
        if (!child->Is(vrv::TIE)) continue;
        const vrv::TimeSpanningInterface *interface = child->GetTimeSpanningInterface();
        if (!interface) continue;
        if (interface->HasEndid()) ties_.AddEndRef(interface->GetEndid());
        if (interface->HasStartid()) ties_.AddStartRef(interface->GetStartid());
    }

    for (const vrv::Object *child : measure->GetChildren()) {
        if (!child->Is(vrv::STAFF)) continue;
        const vrv::Staff *staff = vrv_cast<const vrv::Staff *>(child);
        const std::string n = std::to_string(staff->GetN());
        StaffState &state = EnsureStaffPart(n, true);
        StartStaffMeasure(state);
        std::vector<Event> events;
        std::vector<SymExtra> extras;
        ApplyPendingLayoutBreaks(measure, state, extras);
        ApplyPendingSignatures(state, extras);
        CollectStaffLayerEvents(staff, events, extras, state);
        RegisterEventLocations(n, events);
        CollectControlExtras(measure, n, events, extras);
        const Fraction measureSpan = MeasureSpan(state, events);
        CollectMeasureBarlines(measure, n, measureSpan, extras);
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
