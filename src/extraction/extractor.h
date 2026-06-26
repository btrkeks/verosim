#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "extract_detail.h"
#include "extract_state.h"

#include "verosim/extraction/extract.h"

namespace verosim {

class Extractor {
public:
    Extractor(vrv::Doc &doc, SourceFormat format, ExtractOptions options)
        : doc_(doc), format_(format), options_(options)
    {
    }

    ExtractResult Run();

private:
    void Warn(const std::string &message)
    {
        output_.Warn(message);
    }

    void WalkPageTree(const vrv::Object *obj, bool &sawScore);
    void ApplyInitialScoreDef(const vrv::Score *score);
    void ApplyScoreDefChange(const vrv::ScoreDef *scoreDef);
    void ApplyStaffDef(const vrv::StaffDef *staffDef, const vrv::ScoreDef *parentScoreDef);
    void HandleMeasure(const vrv::Measure *measure);
    StaffState &EnsureStaffPart(const std::string &staffN, bool warnIfImplicit);
    void StartStaffMeasure(StaffState &state);
    void ApplyPendingSignatures(StaffState &state, std::vector<SymExtra> &extras);
    void ApplyPendingLayoutBreaks(const vrv::Measure *measure, StaffState &state,
        std::vector<SymExtra> &extras);
    void CollectStaffLayerEvents(const vrv::Staff *staff, std::vector<Event> &events,
        std::vector<SymExtra> &extras, StaffState &state);
    Fraction MeasureSpan(const StaffState &state, const std::vector<Event> &events) const;
    void FinishStaffMeasure(StaffState &state, const Fraction &measureSpan);
    void CollectControlExtras(const vrv::Measure *measure, const std::string &staffN,
        const std::vector<Event> &events, std::vector<SymExtra> &extras);
    void CollectMeasureBarlines(const vrv::Measure *measure, const std::string &staffN,
        const Fraction &measureSpan, std::vector<SymExtra> &extras);
    void RegisterEventLocations(const std::string &staffN, const std::vector<Event> &events);
    void RegisterEmittedExtras(int partIdx, std::size_t measureIdx);
    Fraction TstampToOffset(double tstamp, const std::string &staffN) const;
    Fraction Tstamp2ToOffset(
        const vrv::data_MEASUREBEAT &tstamp2, const std::string &staffN) const;
    std::optional<Fraction> ControlPointOffset(const vrv::Object *obj,
        const vrv::TimePointInterface *tpi, const std::string &staffN,
        const Fraction &startAbs);
    std::optional<Fraction> ControlSpanDuration(const vrv::TimeSpanningInterface *tsi,
        const std::string &staffN, const Fraction &startAbs, const Fraction &start);
    void CollectLayerEvents(const vrv::Object *obj, std::vector<Event> &events,
        std::vector<SymExtra> &extras, Fraction &cursor, StaffState &state,
        std::vector<const vrv::Tuplet *> &tupletStack, const vrv::Object *beam);
    Event MakeCarrierEvent(const vrv::Object *obj, const Fraction &cursor, StaffState &state,
        const std::vector<const vrv::Tuplet *> &tupletStack, const vrv::Object *beam);
    void EmitMeasure(const vrv::Measure *measure, const std::string &staffN,
        std::vector<Event> events, std::vector<SymExtra> extras, StaffState &state);
    std::vector<SymNote> BuildSymNotes(const std::vector<Event> &events,
        const std::vector<std::vector<BeamValue>> &beamings,
        const std::vector<std::vector<TupletValue>> &tuplets,
        const std::vector<std::vector<std::string>> &tupletInfo, StaffState &state);
    SymNote MakeSymNote(const Event &ev, const vrv::Note *note, const vrv::Object *carrier,
        int idxInChord, StaffState &state);

    SymExtra MakeClefExtra(const vrv::Clef &clef, const Fraction &offset);
    SymExtra MakeKeySigExtra(const vrv::KeySig &keysig, const Fraction &offset);
    SymExtra MakeMensurTimeSigExtra(const vrv::Mensur &mensur, const Fraction &offset);
    SymExtra MakeMeterSigExtra(const vrv::MeterSig &metersig, const Fraction &offset);
    SymExtra MakeDynamicExtra(const vrv::Dynam &dynam, const Fraction &offset);
    SymExtra MakeHairpinExtra(const vrv::Hairpin &hairpin, const Fraction &offset,
        const std::optional<Fraction> &duration);
    SymExtra MakeSlurExtra(const vrv::Slur &slur, const Fraction &offset,
        const std::optional<Fraction> &duration);
    std::optional<SymExtra> MakeOttavaExtra(const vrv::Octave &octave, const Fraction &offset,
        const std::optional<Fraction> &duration);
    SymExtra MakeSystemBreakExtra(
        const PendingLayoutBreak &layout_break, const vrv::Measure *measure) const;
    std::vector<SymExtra> MakeBarlineExtras(vrv::data_BARRENDITION form,
        BarlineLocation location, const Fraction &offset, const std::string &id);

    vrv::Doc &doc_;
    SourceFormat format_;
    ExtractOptions options_;
    ExtractionOutput output_;
    StaffRegistry staves_;
    TieRegistry ties_;
    ControlSpanState controls_;
    LayoutBreakState layout_;
};

} // namespace verosim
