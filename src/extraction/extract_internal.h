#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "accid.h"
#include "artic.h"
#include "beam.h"
#include "beamspan.h"
#include "chord.h"
#include "clef.h"
#include "doc.h"
#include "dynam.h"
#include "gracegrp.h"
#include "hairpin.h"
#include "keysig.h"
#include "layer.h"
#include "layerelement.h"
#include "measure.h"
#include "mensur.h"
#include "metersig.h"
#include "note.h"
#include "object.h"
#include "rest.h"
#include "score.h"
#include "scoredef.h"
#include "slur.h"
#include "staff.h"
#include "staffdef.h"
#include "timeinterface.h"
#include "tuplet.h"
#include "vrv.h"

#include "verosim/extraction/effective_pitch.h"
#include "verosim/extraction/extract.h"
#include "verosim/model/duration.h"

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

struct EmittedExtraLocation {
    int part_idx = -1;
    std::size_t measure_idx = 0;
    std::size_t extra_idx = 0;
};

namespace extract_detail {

char StepFromPname(vrv::data_PITCHNAME pname);
int DiatonicIndex(char step);
std::string TypeNameFromDur(vrv::data_DURATION dur);
bool AccidWrittenInfo(vrv::data_ACCIDENTAL_WRITTEN accid, std::string &name, int &alter);
bool AccidGesturalAlter(vrv::data_ACCIDENTAL_GESTURAL accid, int &alter);
std::string ClefSign(vrv::data_CLEFSHAPE shape);
void AppendArticulations(const vrv::Object *obj, std::vector<std::string> &out);
std::string StripIdRef(std::string ref);
bool StaffMatches(const vrv::Object *obj, const std::string &staffN);
bool HasStaffIdent(const vrv::Object *obj);
std::optional<std::string> ModernMensurSymbol(const vrv::Mensur &mensur);
bool IsVisibleMeterSig(const vrv::MeterSig &metersig);
bool MeterQLFromMeterSig(const vrv::MeterSig &metersig, Fraction &ql);
bool MeterQLFromMensur(const vrv::Mensur &mensur, Fraction &ql);
Fraction BeatQLFromMeterSig(const vrv::MeterSig &metersig);
Fraction FractionFromDouble(double value);

} // namespace extract_detail

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
        if (warned_.insert(message).second) result_.warnings.push_back(message);
    }

    void WalkPageTree(const vrv::Object *obj, bool &sawScore);
    void ApplyInitialScoreDef(const vrv::Score *score);
    void ApplyScoreDefChange(const vrv::ScoreDef *scoreDef);
    void ApplyStaffDef(const vrv::StaffDef *staffDef, const vrv::ScoreDef *parentScoreDef);
    void HandleMeasure(const vrv::Measure *measure);
    StaffState &EnsureStaffPart(const std::string &staffN, bool warnIfImplicit);
    void StartStaffMeasure(StaffState &state);
    void ApplyPendingSignatures(StaffState &state, std::vector<SymExtra> &extras);
    void CollectStaffLayerEvents(const vrv::Staff *staff, std::vector<Event> &events,
        std::vector<SymExtra> &extras, StaffState &state);
    Fraction MeasureSpan(const StaffState &state, const std::vector<Event> &events) const;
    void FinishStaffMeasure(StaffState &state, const Fraction &measureSpan);
    void CollectControlExtras(const vrv::Measure *measure, const std::string &staffN,
        const std::vector<Event> &events, std::vector<SymExtra> &extras);
    void RegisterEventLocations(const std::string &staffN, const std::vector<Event> &events);
    void ResolvePendingSpans();
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

    vrv::Doc &doc_;
    SourceFormat format_;
    ExtractOptions options_;
    ExtractResult result_;
    std::set<std::string> warned_;
    std::map<std::string, StaffState> staves_; // staff @n -> state
    std::vector<std::string> staff_order_;
    // note ids referenced by tie control elements. A <tie> is a child of the
    // measure where the tie STARTS, but its @endid may point into the next
    // measure, so both sets persist across measures; entries are consumed
    // when the referenced note is resolved (MakeSymNote).
    std::set<std::string> tie_end_ids_;
    std::set<std::string> tie_start_ids_;
    std::map<std::string, EventLocation> event_locations_;
    std::vector<PendingSpan> pending_spans_;
    std::map<std::string, EmittedExtraLocation> emitted_extra_locations_;
};

} // namespace verosim
