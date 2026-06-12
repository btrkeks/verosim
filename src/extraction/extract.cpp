#include "verosim/extraction/extract.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "accid.h"
#include "beam.h"
#include "chord.h"
#include "clef.h"
#include "doc.h"
#include "gracegrp.h"
#include "keysig.h"
#include "layer.h"
#include "measure.h"
#include "metersig.h"
#include "note.h"
#include "object.h"
#include "rest.h"
#include "score.h"
#include "scoredef.h"
#include "staff.h"
#include "staffdef.h"
#include "timeinterface.h"
#include "tuplet.h"

#include "verosim/extraction/effective_pitch.h"
#include "verosim/extraction/notation_rules.h"
#include "verosim/model/duration.h"

namespace verosim {

namespace {

// ---------------------------------------------------------------------------
// Attribute-value translation tables (MEI -> music21 vocabulary)
// ---------------------------------------------------------------------------

char StepFromPname(vrv::data_PITCHNAME pname)
{
    switch (pname) {
        case vrv::PITCHNAME_c: return 'c';
        case vrv::PITCHNAME_d: return 'd';
        case vrv::PITCHNAME_e: return 'e';
        case vrv::PITCHNAME_f: return 'f';
        case vrv::PITCHNAME_g: return 'g';
        case vrv::PITCHNAME_a: return 'a';
        case vrv::PITCHNAME_b: return 'b';
        default: return 0;
    }
}

int DiatonicIndex(char step)
{
    static const std::string kOrder = "cdefgab";
    return static_cast<int>(kOrder.find(step));
}

// m21 duration type name from MEI @dur
std::string TypeNameFromDur(vrv::data_DURATION dur)
{
    switch (dur) {
        case vrv::DURATION_maxima: return "maxima";
        case vrv::DURATION_long: return "longa";
        case vrv::DURATION_breve: return "breve";
        case vrv::DURATION_1: return "whole";
        case vrv::DURATION_2: return "half";
        case vrv::DURATION_4: return "quarter";
        case vrv::DURATION_8: return "eighth";
        case vrv::DURATION_16: return "16th";
        case vrv::DURATION_32: return "32nd";
        case vrv::DURATION_64: return "64th";
        case vrv::DURATION_128: return "128th";
        case vrv::DURATION_256: return "256th";
        case vrv::DURATION_512: return "512th";
        case vrv::DURATION_1024: return "1024th";
        case vrv::DURATION_2048: return "2048th";
        default: return "";
    }
}

// music21 accidental name + alter from MEI @accid
bool AccidWrittenInfo(vrv::data_ACCIDENTAL_WRITTEN accid, std::string &name, int &alter)
{
    switch (accid) {
        case vrv::ACCIDENTAL_WRITTEN_s: name = "sharp"; alter = 1; return true;
        case vrv::ACCIDENTAL_WRITTEN_f: name = "flat"; alter = -1; return true;
        case vrv::ACCIDENTAL_WRITTEN_n: name = "natural"; alter = 0; return true;
        case vrv::ACCIDENTAL_WRITTEN_ss:
        case vrv::ACCIDENTAL_WRITTEN_x: name = "double-sharp"; alter = 2; return true;
        case vrv::ACCIDENTAL_WRITTEN_ff: name = "double-flat"; alter = -2; return true;
        case vrv::ACCIDENTAL_WRITTEN_xs:
        case vrv::ACCIDENTAL_WRITTEN_sx:
        case vrv::ACCIDENTAL_WRITTEN_ts: name = "triple-sharp"; alter = 3; return true;
        case vrv::ACCIDENTAL_WRITTEN_tf: name = "triple-flat"; alter = -3; return true;
        default: return false;
    }
}

bool AccidGesturalAlter(vrv::data_ACCIDENTAL_GESTURAL accid, int &alter)
{
    switch (accid) {
        case vrv::ACCIDENTAL_GESTURAL_s: alter = 1; return true;
        case vrv::ACCIDENTAL_GESTURAL_f: alter = -1; return true;
        case vrv::ACCIDENTAL_GESTURAL_n: alter = 0; return true;
        case vrv::ACCIDENTAL_GESTURAL_ss: alter = 2; return true;
        case vrv::ACCIDENTAL_GESTURAL_ff: alter = -2; return true;
        case vrv::ACCIDENTAL_GESTURAL_ts: alter = 3; return true;
        case vrv::ACCIDENTAL_GESTURAL_tf: alter = -3; return true;
        default: return false;
    }
}

std::string ClefSign(vrv::data_CLEFSHAPE shape)
{
    switch (shape) {
        case vrv::CLEFSHAPE_G: return "G";
        case vrv::CLEFSHAPE_GG: return "G"; // m21 has no GG clef
        case vrv::CLEFSHAPE_F: return "F";
        case vrv::CLEFSHAPE_C: return "C";
        case vrv::CLEFSHAPE_perc: return "percussion";
        case vrv::CLEFSHAPE_TAB: return "TAB";
        default: return "";
    }
}

// ---------------------------------------------------------------------------
// Extraction context
// ---------------------------------------------------------------------------

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
    const vrv::Object *beam = nullptr; // innermost enclosing Beam
    std::vector<const vrv::Tuplet *> tuplets; // outermost first
};

struct PendingSig {
    // heap copies from GetClefCopy()/... (attribute forms materialized)
    std::shared_ptr<const vrv::Clef> clef;
    std::shared_ptr<const vrv::KeySig> keysig;
    std::shared_ptr<const vrv::MeterSig> metersig;
};

struct StaffState {
    int part_idx = -1;
    AccidentalState accids;
    Fraction meter_ql = Fraction(4); // governing measure length
    bool has_meter = false;
    PendingSig pending; // signature extras to emit at offset 0 of the next measure
};

class Extractor {
public:
    Extractor(vrv::Doc &doc, SourceFormat format) : doc_(doc), format_(format) {}

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
    void CollectLayerEvents(const vrv::Object *obj, std::vector<Event> &events,
        std::vector<SymExtra> &extras, Fraction &cursor, StaffState &state,
        std::vector<const vrv::Tuplet *> &tupletStack, const vrv::Object *beam);
    Event MakeCarrierEvent(const vrv::Object *obj, const Fraction &cursor, StaffState &state,
        const std::vector<const vrv::Tuplet *> &tupletStack, const vrv::Object *beam);
    void EmitMeasure(const vrv::Measure *measure, const std::string &staffN,
        std::vector<Event> events, std::vector<SymExtra> extras, StaffState &state);
    SymNote MakeSymNote(const Event &ev, const vrv::Note *note, const vrv::Object *carrier,
        int idxInChord, StaffState &state, bool tiedFromPrevious);

    SymExtra MakeClefExtra(const vrv::Clef &clef, const Fraction &offset);
    SymExtra MakeKeySigExtra(const vrv::KeySig &keysig, const Fraction &offset);
    SymExtra MakeMeterSigExtra(const vrv::MeterSig &metersig, const Fraction &offset);

    vrv::Doc &doc_;
    SourceFormat format_;
    ExtractResult result_;
    std::set<std::string> warned_;
    std::map<std::string, StaffState> staves_; // staff @n -> state
    std::vector<std::string> staff_order_;
    // tie targets in the current measure (@endid of tie control elements)
    std::set<std::string> tie_end_ids_;
};

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
    // in one element — see docs/mei_symbol_mapping.md "Tree shape").
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

    for (const vrv::Object *obj : scoreDef->FindAllDescendantsByType(vrv::STAFFDEF)) {
        ApplyStaffDef(vrv_cast<const vrv::StaffDef *>(obj), nullptr);
    }
}

void Extractor::ApplyStaffDef(const vrv::StaffDef *staffDef, const vrv::ScoreDef *parentScoreDef)
{
    const std::string n = std::to_string(staffDef->GetN());
    if (!staves_.count(n)) {
        staves_[n]; // default-construct
        staff_order_.push_back(n);
        result_.score.parts.emplace_back();
        result_.score.parts.back().staff_n = n;
        staves_[n].part_idx = static_cast<int>(result_.score.parts.size()) - 1;
    }
    StaffState &state = staves_[n];

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

void Extractor::HandleMeasure(const vrv::Measure *measure)
{
    // tie control elements: targets (@endid) are tied-from-previous notes
    tie_end_ids_.clear();
    for (const vrv::Object *child : measure->GetChildren()) {
        if (!child->Is(vrv::TIE)) continue;
        const vrv::TimeSpanningInterface *interface
            = child->GetTimeSpanningInterface();
        if (interface && interface->HasEndid()) {
            std::string endid = interface->GetEndid();
            if (!endid.empty() && endid[0] == '#') endid.erase(0, 1);
            tie_end_ids_.insert(endid);
        }
    }

    for (const vrv::Object *child : measure->GetChildren()) {
        if (!child->Is(vrv::STAFF)) continue;
        const vrv::Staff *staff = vrv_cast<const vrv::Staff *>(child);
        const std::string n = std::to_string(staff->GetN());
        if (!staves_.count(n)) {
            Warn("staff n=" + n + " has no staffDef; creating an implicit part");
            staves_[n];
            staff_order_.push_back(n);
            result_.score.parts.emplace_back();
            result_.score.parts.back().staff_n = n;
            staves_[n].part_idx = static_cast<int>(result_.score.parts.size()) - 1;
        }
        StaffState &state = staves_[n];
        state.accids.StartMeasure();
        if (state.pending.keysig) {
            state.accids.SetKeySig([&] {
                const int count = state.pending.keysig->GetAccidCount();
                return state.pending.keysig->GetAccidType() == vrv::ACCIDENTAL_WRITTEN_f ? -count
                                                                                         : count;
            }());
        }

        std::vector<Event> events;
        std::vector<SymExtra> extras;

        // pending signature changes surface at offset 0 of this measure
        if (state.pending.clef) extras.push_back(MakeClefExtra(*state.pending.clef, Fraction(0)));
        if (state.pending.keysig) {
            extras.push_back(MakeKeySigExtra(*state.pending.keysig, Fraction(0)));
        }
        if (state.pending.metersig) {
            extras.push_back(MakeMeterSigExtra(*state.pending.metersig, Fraction(0)));
            const vrv::data_METERCOUNT_pair count = state.pending.metersig->GetCount();
            int total = 0;
            for (const int c : count.first) total += c;
            if (total > 0 && state.pending.metersig->GetUnit() > 0) {
                state.meter_ql = Fraction(4 * total, state.pending.metersig->GetUnit());
                state.has_meter = true;
            }
            else if (state.pending.metersig->GetSym() != vrv::METERSIGN_NONE) {
                state.meter_ql = Fraction(4); // common or cut: 4 QL either way
                state.has_meter = true;
            }
        }
        state.pending = PendingSig();

        for (const vrv::Object *staffChild : staff->GetChildren()) {
            if (!staffChild->Is(vrv::LAYER)) continue;
            Fraction cursor(0);
            std::vector<const vrv::Tuplet *> tupletStack;
            CollectLayerEvents(staffChild, events, extras, cursor, state, tupletStack, nullptr);
        }

        EmitMeasure(measure, n, std::move(events), std::move(extras), state);
    }
}

void Extractor::CollectLayerEvents(const vrv::Object *obj, std::vector<Event> &events,
    std::vector<SymExtra> &extras, Fraction &cursor, StaffState &state,
    std::vector<const vrv::Tuplet *> &tupletStack, const vrv::Object *beam)
{
    for (const vrv::Object *child : obj->GetChildren()) {
        if (child->Is(vrv::BEAM)) {
            CollectLayerEvents(child, events, extras, cursor, state, tupletStack, child);
        }
        else if (child->Is(vrv::TUPLET)) {
            tupletStack.push_back(vrv_cast<const vrv::Tuplet *>(child));
            CollectLayerEvents(child, events, extras, cursor, state, tupletStack, beam);
            tupletStack.pop_back();
        }
        else if (child->Is(vrv::GRACEGRP)) {
            CollectLayerEvents(child, events, extras, cursor, state, tupletStack, beam);
        }
        else if (child->Is(vrv::BTREM) || child->Is(vrv::FTREM)) {
            Warn("tremolo container encountered; contained notes extracted plainly");
            CollectLayerEvents(child, events, extras, cursor, state, tupletStack, beam);
        }
        else if (child->Is(vrv::NOTE) || child->Is(vrv::CHORD) || child->Is(vrv::REST)
            || child->Is(vrv::MREST) || child->Is(vrv::MULTIREST) || child->Is(vrv::SPACE)
            || child->Is(vrv::MSPACE)) {
            Event ev = MakeCarrierEvent(child, cursor, state, tupletStack, beam);
            const bool hidden = child->Is(vrv::SPACE) || child->Is(vrv::MSPACE);
            if (!hidden) events.push_back(ev);
            if (ev.grace_type.empty()) cursor += ev.dur_ql;
        }
        else if (child->Is(vrv::CLEF)) {
            extras.push_back(MakeClefExtra(*vrv_cast<const vrv::Clef *>(child), cursor));
        }
        else if (child->Is(vrv::KEYSIG)) {
            const vrv::KeySig *keysig = vrv_cast<const vrv::KeySig *>(child);
            extras.push_back(MakeKeySigExtra(*keysig, cursor));
            const int count = keysig->GetAccidCount();
            state.accids.SetKeySig(
                keysig->GetAccidType() == vrv::ACCIDENTAL_WRITTEN_f ? -count : count);
        }
        else if (child->Is(vrv::METERSIG)) {
            extras.push_back(
                MakeMeterSigExtra(*vrv_cast<const vrv::MeterSig *>(child), cursor));
        }
        else if (child->Is(vrv::BARLINE) || child->Is(vrv::MNUM) || child->Is(vrv::TUPLET_NUM)
            || child->Is(vrv::TUPLET_BRACKET)) {
            // barlines are not Tier A; tupletNum/tupletBracket are visual
            // companions of <tuplet>, whose @num we already read
        }
        else {
            Warn("unhandled layer element <" + child->GetClassName() + "> skipped");
        }
    }
}

Event Extractor::MakeCarrierEvent(const vrv::Object *obj, const Fraction &cursor,
    StaffState &state, const std::vector<const vrv::Tuplet *> &tupletStack,
    const vrv::Object *beam)
{
    Event ev;
    ev.obj = obj;
    ev.offset = cursor;
    ev.beam = beam;
    ev.tuplets = tupletStack;
    ev.is_rest = obj->Is(vrv::REST) || obj->Is(vrv::MREST) || obj->Is(vrv::MULTIREST)
        || obj->Is(vrv::SPACE) || obj->Is(vrv::MSPACE);

    // duration attributes live on the carrier (chord-level for chords)
    const vrv::DurationInterface *durInterface = obj->GetDurationInterface();
    vrv::data_DURATION dur = vrv::DURATION_NONE;
    int dots = 0;
    if (durInterface) {
        dur = durInterface->GetDur();
        if (durInterface->HasDots()) dots = durInterface->GetDots();
    }

    // grace
    vrv::data_GRACE grace = vrv::GRACE_NONE;
    if (const vrv::AttGraced *graced = dynamic_cast<const vrv::AttGraced *>(obj)) {
        grace = graced->GetGrace();
    }
    if (grace == vrv::GRACE_NONE) {
        if (const vrv::Object *graceGrp = obj->GetFirstAncestor(vrv::GRACEGRP)) {
            grace = vrv_cast<const vrv::GraceGrp *>(graceGrp)->GetGrace();
            if (grace == vrv::GRACE_NONE) grace = vrv::GRACE_unacc;
        }
    }
    if (grace == vrv::GRACE_unacc) {
        ev.grace_type = "unacc";
        ev.grace_slash = true;
    }
    else if (grace == vrv::GRACE_acc || grace == vrv::GRACE_unknown) {
        // kern qq is a true appoggiatura; MusicXML <grace> without a slash
        // attribute maps here too, but music21 defaults GraceDuration.slash
        // to True ('unacc') — see docs/mei_symbol_mapping.md "Grace notes"
        if (format_ == SourceFormat::kKern) {
            ev.grace_type = "acc";
            ev.grace_slash = false;
        }
        else {
            ev.grace_type = "unacc";
            ev.grace_slash = true;
        }
    }

    if (obj->Is(vrv::MREST) || obj->Is(vrv::MULTIREST) || obj->Is(vrv::MSPACE)) {
        // full-measure rest: duration is the governing meter's measure length
        if (!state.has_meter) {
            Warn("mRest without a governing meter; assuming 4/4");
        }
        if (obj->Is(vrv::MULTIREST)) {
            Warn("multiRest treated as a single full-measure rest");
        }
        ev.dur_ql = state.meter_ql;
        std::string type;
        int mrDots = 0;
        if (QLToTypeDots(ev.dur_ql, type, mrDots)) {
            ev.dur_type = type;
            ev.dots = mrDots;
            ev.type_num = TypeNumFromName(type);
        }
        else {
            ev.dur_type = "complex";
            ev.dots = 0;
            ev.type_num = TypeNumFromQL(ev.dur_ql, "complex");
        }
        return ev;
    }

    ev.dots = dots;
    ev.dur_type = TypeNameFromDur(dur);
    if (ev.dur_type.empty()) {
        Warn("event <" + obj->GetClassName() + "> without usable @dur; assuming quarter");
        ev.dur_type = "quarter";
    }
    ev.type_num = TypeNumFromName(ev.dur_type);
    Fraction ql = QLFromTypeNum(ev.type_num);
    // dot factor: (2^(d+1) - 1) / 2^d
    if (dots > 0) {
        ql = ql * Fraction((std::int64_t(1) << (dots + 1)) - 1, std::int64_t(1) << dots);
    }
    for (const vrv::Tuplet *tuplet : tupletStack) {
        if (tuplet->GetNum() > 0 && tuplet->GetNumbase() > 0) {
            ql = ql * Fraction(tuplet->GetNumbase(), tuplet->GetNum());
        }
    }
    ev.dur_ql = ql;
    return ev;
}

void Extractor::EmitMeasure(const vrv::Measure *measure, const std::string &staffN,
    std::vector<Event> events, std::vector<SymExtra> extras, StaffState &state)
{
    // --- extras: offset-sort, consecutive-equivalent clef dedup (within the
    // measure, m21utils.py:877-904), then (kind, offset) sort ---
    std::stable_sort(extras.begin(), extras.end(),
        [](const SymExtra &a, const SymExtra &b) { return a.offset < b.offset; });
    std::vector<SymExtra> dedupedExtras;
    const std::string *mostRecentClef = nullptr;
    for (SymExtra &extra : extras) {
        if (extra.kind == ExtraKind::kClef) {
            if (mostRecentClef && extra.symbolic.has_value() && *mostRecentClef == *extra.symbolic) {
                continue;
            }
            dedupedExtras.push_back(std::move(extra));
            mostRecentClef = &*dedupedExtras.back().symbolic;
        }
        else {
            dedupedExtras.push_back(std::move(extra));
        }
    }
    std::stable_sort(dedupedExtras.begin(), dedupedExtras.end(),
        [](const SymExtra &a, const SymExtra &b) {
            if (a.kind != b.kind) return a.kind < b.kind;
            return a.offset < b.offset;
        });

    // --- notes: build RawEvents over the flat cross-layer list ---
    // raw beam types per Beam container
    std::map<const vrv::Object *, std::vector<std::size_t>> beamMembers;
    for (std::size_t i = 0; i < events.size(); ++i) {
        if (events[i].beam) beamMembers[events[i].beam].push_back(i);
    }
    std::vector<std::vector<BeamValue>> rawBeams(events.size());
    for (const auto &[beamObj, members] : beamMembers) {
        std::vector<BeamMember> bm;
        bm.reserve(members.size());
        for (const std::size_t idx : members) {
            BeamMember m;
            m.is_rest = events[idx].is_rest;
            if (!m.is_rest) {
                Fraction t = events[idx].type_num;
                while (t > Fraction(4)) {
                    t = t / Fraction(2);
                    ++m.n_beams;
                }
            }
            if (const vrv::DurationInterface *di = events[idx].obj->GetDurationInterface()) {
                // @breaksec lives on AttBeamSecondary, a DurationInterface base
                if (di->HasBreaksec()) m.breaksec = di->GetBreaksec();
            }
            bm.push_back(m);
        }
        const std::vector<std::vector<BeamValue>> derived = DeriveBeamTypes(bm);
        for (std::size_t k = 0; k < members.size(); ++k) rawBeams[members[k]] = derived[k];
    }

    // raw tuplet types: first/last visible event per tuplet
    std::map<const vrv::Tuplet *, std::pair<std::size_t, std::size_t>> tupletSpan;
    for (std::size_t i = 0; i < events.size(); ++i) {
        for (const vrv::Tuplet *t : events[i].tuplets) {
            auto it = tupletSpan.find(t);
            if (it == tupletSpan.end()) {
                tupletSpan[t] = { i, i };
            }
            else {
                it->second.second = i;
            }
        }
    }

    std::vector<RawEvent> raw(events.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        raw[i].is_rest = events[i].is_rest;
        raw[i].type_num = events[i].type_num;
        raw[i].raw_beams = rawBeams[i];
        for (const vrv::Tuplet *t : events[i].tuplets) {
            const auto &[first, last] = tupletSpan.at(t);
            std::optional<TupletValue> type;
            if (first == i && last == i) type = TupletValue::kStartStop;
            else if (first == i) type = TupletValue::kStart;
            else if (last == i) type = TupletValue::kStop;
            raw[i].raw_tuplets.push_back(type);
            raw[i].tuplet_nums.push_back(std::to_string(t->GetNum()));
        }
    }

    const std::vector<std::vector<BeamValue>> beamings = EnhanceBeamings(raw);
    const std::vector<std::vector<TupletValue>> tuplets = CorrectTuplets(raw);
    const std::vector<std::vector<std::string>> tupletInfo = TupletInfo(raw);

    // --- emit SymNotes (chords split into members, diatonic ascending) ---
    SymMeasure symMeasure;
    symMeasure.vrv_id = measure->GetID();
    symMeasure.measure_n = measure->GetN();
    symMeasure.extras = std::move(dedupedExtras);

    for (std::size_t i = 0; i < events.size(); ++i) {
        const Event &ev = events[i];
        const auto fillShared = [&](SymNote &sn) {
            sn.note_head = NoteHeadFromTypeNum(ev.type_num);
            sn.dots = ev.dots;
            sn.beamings = beamings[i];
            sn.tuplets = tuplets[i];
            sn.tuplet_info = tupletInfo[i];
            sn.grace_type = ev.grace_type;
            sn.grace_slash = ev.grace_slash;
            sn.note_offset = ev.offset;
            sn.note_dur_type = ev.dur_type;
            sn.note_dur_dots = ev.dots;
            sn.note_is_grace = !ev.grace_type.empty();
        };

        if (ev.obj->Is(vrv::CHORD)) {
            const vrv::Chord *chord = vrv_cast<const vrv::Chord *>(ev.obj);
            std::vector<const vrv::Note *> members;
            for (const vrv::Object *c : chord->GetChildren()) {
                if (c->Is(vrv::NOTE)) members.push_back(vrv_cast<const vrv::Note *>(c));
            }
            std::stable_sort(members.begin(), members.end(),
                [](const vrv::Note *a, const vrv::Note *b) {
                    const int da = a->GetOct() * 7 + DiatonicIndex(StepFromPname(a->GetPname()));
                    const int db = b->GetOct() * 7 + DiatonicIndex(StepFromPname(b->GetPname()));
                    return da < db;
                });
            int idx = 0;
            for (const vrv::Note *member : members) {
                const bool tied = tie_end_ids_.count(member->GetID()) > 0;
                SymNote sn = MakeSymNote(ev, member, chord, idx++, state, tied);
                fillShared(sn);
                symMeasure.notes.push_back(std::move(sn));
            }
        }
        else if (ev.is_rest) {
            SymNote sn;
            sn.vrv_id = ev.obj->GetID();
            sn.pitches.push_back(SymPitch{ "R", "None", false, 0 });
            fillShared(sn);
            symMeasure.notes.push_back(std::move(sn));
        }
        else {
            const vrv::Note *note = vrv_cast<const vrv::Note *>(ev.obj);
            const bool tied = tie_end_ids_.count(note->GetID()) > 0;
            SymNote sn = MakeSymNote(ev, note, note, -1, state, tied);
            fillShared(sn);
            symMeasure.notes.push_back(std::move(sn));
        }
    }

    if (symMeasure.n_of_elements() > 0) {
        result_.score.parts[staves_[staffN].part_idx].bar_list.push_back(std::move(symMeasure));
    }
}

SymNote Extractor::MakeSymNote(const Event &ev, const vrv::Note *note,
    const vrv::Object *carrier, int idxInChord, StaffState &state, bool tiedFromPrevious)
{
    SymNote sn;
    sn.vrv_id = carrier->GetID();
    sn.is_in_chord = idxInChord >= 0;
    sn.note_idx_in_chord = idxInChord;

    SymPitch pitch;
    const char step = StepFromPname(note->GetPname());
    if (step == 0) {
        Warn("pitched note without @pname (unpitched?); using C4");
        pitch.step_octave = "C4";
    }
    else {
        pitch.step_octave = static_cast<char>(step - 'a' + 'A') + std::to_string(note->GetOct());
    }

    // visible accidental: the @accid VALUE on the accid child (iohumdrum
    // attaches empty accid children to every pitched note — D13)
    const int *writtenAlter = nullptr;
    const int *gesturalAlter = nullptr;
    int writtenValue = 0;
    int gesturalValue = 0;
    for (const vrv::Object *c : note->GetChildren()) {
        if (!c->Is(vrv::ACCID)) continue;
        const vrv::Accid *accid = vrv_cast<const vrv::Accid *>(c);
        if (accid->HasAccid()) {
            std::string name;
            if (AccidWrittenInfo(accid->GetAccid(), name, writtenValue)) {
                pitch.accid = name;
                writtenAlter = &writtenValue;
            }
            else {
                Warn("unmapped @accid value on " + note->GetID());
            }
        }
        if (accid->HasAccidGes()) {
            if (AccidGesturalAlter(accid->GetAccidGes(), gesturalValue)) {
                gesturalAlter = &gesturalValue;
            }
        }
        break;
    }

    // tie: control element (@endid) or @tie attribute on the note
    bool tied = tiedFromPrevious;
    if (note->HasTie()
        && (note->GetTie() == vrv::TIE_m || note->GetTie() == vrv::TIE_t)) {
        tied = true;
    }

    if (step != 0) {
        pitch.sounding_alter
            = state.accids.Resolve(step, note->GetOct(), writtenAlter, gesturalAlter, tied);
        const bool tieStart = note->HasTie()
            && (note->GetTie() == vrv::TIE_i || note->GetTie() == vrv::TIE_m);
        if (tieStart) {
            state.accids.RegisterTieStart(step, note->GetOct(), pitch.sounding_alter);
        }
    }

    sn.pitches.push_back(std::move(pitch));
    return sn;
}

SymExtra Extractor::MakeClefExtra(const vrv::Clef &clef, const Fraction &offset)
{
    SymExtra extra;
    extra.vrv_id = clef.GetID();
    extra.kind = ExtraKind::kClef;
    extra.offset = offset;
    // m21utils clef_to_symbolic: f"{sign}{line}{octave:+}" with 8 per octave
    std::string symbolic = ClefSign(clef.GetShape());
    symbolic += std::to_string(clef.HasLine() ? clef.GetLine() : 0);
    if (clef.GetDis() != vrv::OCTAVE_DIS_NONE) {
        int octaves = 0;
        switch (clef.GetDis()) {
            case vrv::OCTAVE_DIS_8: octaves = 1; break;
            case vrv::OCTAVE_DIS_15: octaves = 2; break;
            case vrv::OCTAVE_DIS_22: octaves = 3; break;
            default: break;
        }
        if (clef.GetDisPlace() == vrv::STAFFREL_basic_below) octaves = -octaves;
        if (octaves != 0) {
            symbolic += (octaves > 0 ? "+" : "") + std::to_string(8 * octaves);
        }
    }
    extra.symbolic = symbolic;
    return extra;
}

SymExtra Extractor::MakeKeySigExtra(const vrv::KeySig &keysig, const Fraction &offset)
{
    SymExtra extra;
    extra.vrv_id = keysig.GetID();
    extra.kind = ExtraKind::kKeySig;
    extra.offset = offset;
    // m21utils keysig_to_infodict: per-accidental entries, circle-of-fifths
    static const char *kFlatNames[] = { "B", "E", "A", "D", "G", "C", "F" };
    static const char *kSharpNames[] = { "F", "C", "G", "D", "A", "E", "B" };
    const int count = keysig.GetAccidCount();
    if (count == 0) {
        extra.infodict.emplace_back("flats/sharps", "none");
    }
    else if (keysig.GetAccidType() == vrv::ACCIDENTAL_WRITTEN_f) {
        for (int i = 0; i < count; ++i) {
            extra.infodict.emplace_back("flat" + std::to_string(i), kFlatNames[i % 7]);
        }
    }
    else {
        for (int i = 0; i < count; ++i) {
            extra.infodict.emplace_back("sharp" + std::to_string(i), kSharpNames[i % 7]);
        }
    }
    return extra;
}

SymExtra Extractor::MakeMeterSigExtra(const vrv::MeterSig &metersig, const Fraction &offset)
{
    SymExtra extra;
    extra.vrv_id = metersig.GetID();
    extra.kind = ExtraKind::kTimeSig;
    extra.offset = offset;
    // m21utils timesig_to_infodict
    if (metersig.GetSym() == vrv::METERSIGN_common) {
        extra.infodict.emplace_back("symbol", "common");
        return extra;
    }
    if (metersig.GetSym() == vrv::METERSIGN_cut) {
        extra.infodict.emplace_back("symbol", "cut");
        return extra;
    }
    const vrv::data_METERCOUNT_pair count = metersig.GetCount();
    int total = 0;
    for (const int c : count.first) total += c;
    extra.infodict.emplace_back("numerator", std::to_string(total));
    extra.infodict.emplace_back("denominator", std::to_string(metersig.GetUnit()));
    return extra;
}

} // namespace

ExtractResult ExtractSymScore(vrv::Doc &doc, SourceFormat format)
{
    Extractor extractor(doc, format);
    return extractor.Run();
}

} // namespace verosim
