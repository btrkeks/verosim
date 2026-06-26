#include "extractor.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "verosim/extraction/typed_space_policy.h"

namespace verosim {

using namespace extract_detail;

namespace extract_detail {

std::string ArticulationName(vrv::data_ARTICULATION artic)
{
    switch (artic) {
        case vrv::ARTICULATION_acc: return "accent";
        case vrv::ARTICULATION_acc_inv: return "inverted accent";
        case vrv::ARTICULATION_acc_long: return "strong accent";
        case vrv::ARTICULATION_acc_soft: return "soft accent";
        case vrv::ARTICULATION_stacc: return "staccato";
        case vrv::ARTICULATION_ten: return "tenuto";
        case vrv::ARTICULATION_stacciss: return "staccatissimo";
        case vrv::ARTICULATION_marc: return "strong accent";
        case vrv::ARTICULATION_spicc: return "spiccato";
        case vrv::ARTICULATION_doit: return "doit";
        case vrv::ARTICULATION_scoop: return "scoop";
        case vrv::ARTICULATION_fall: return "falloff";
        case vrv::ARTICULATION_dnbow: return "down bow";
        case vrv::ARTICULATION_upbow: return "up bow";
        case vrv::ARTICULATION_harm: return "harmonic";
        case vrv::ARTICULATION_snap: return "snap pizzicato";
        case vrv::ARTICULATION_open: return "open string";
        case vrv::ARTICULATION_stop: return "stopped";
        case vrv::ARTICULATION_dbltongue: return "double tongue";
        case vrv::ARTICULATION_trpltongue: return "triple tongue";
        default: return "";
    }
}

void AppendArticulations(const vrv::Object *obj, std::vector<std::string> &out)
{
    const auto append_list = [&](const vrv::data_ARTICULATION_List &list) {
        for (const vrv::data_ARTICULATION artic : list) {
            std::string name = ArticulationName(artic);
            if (!name.empty()) out.push_back(std::move(name));
        }
    };
    if (const auto *att = dynamic_cast<const vrv::AttArticulation *>(obj)) {
        if (att->HasArtic()) append_list(att->GetArtic());
    }
    for (const vrv::Object *child : obj->GetChildren()) {
        if (!child->Is(vrv::ARTIC)) continue;
        const auto *artic = vrv_cast<const vrv::Artic *>(child);
        if (artic->HasArtic()) append_list(artic->GetArtic());
    }
}

} // namespace extract_detail

namespace {

bool IsHiddenCarrier(const vrv::Object *obj)
{
    return obj->Is(vrv::SPACE) || obj->Is(vrv::MSPACE);
}

} // namespace

void Extractor::CollectLayerEvents(const vrv::Object *obj, std::vector<Event> &events,
    std::vector<SymExtra> &extras, Fraction &cursor, StaffState &state,
    std::vector<const vrv::Tuplet *> &tupletStack, const vrv::Object *beam)
{
    std::optional<Fraction> meterSigContextOffset;
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
            const bool hidden = IsHiddenCarrier(child);
            if (!hidden) events.push_back(ev);
            if (ev.grace_type.empty()
                && !ShouldSuppressTypedSpaceDuration(child, options_.typed_space_handling)) {
                cursor += ev.dur_ql;
            }
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
            const vrv::MeterSig *metersig = vrv_cast<const vrv::MeterSig *>(child);
            if (IsVisibleMeterSig(*metersig)) {
                extras.push_back(MakeMeterSigExtra(*metersig, cursor));
            }
            // mirror the inline KEYSIG case: the new meter governs any
            // following mRest (in this measure and beyond)
            if (MeterQLFromMeterSig(*metersig, state.meter_ql)) {
                state.has_meter = true;
                meterSigContextOffset = cursor;
            }
            state.beat_ql = BeatQLFromMeterSig(*metersig);
        }
        else if (child->Is(vrv::MENSUR)) {
            const vrv::Mensur *mensur = vrv_cast<const vrv::Mensur *>(child);
            if (ModernMensurSymbol(*mensur)) {
                extras.push_back(MakeMensurTimeSigExtra(*mensur, cursor));
                const bool hasPairedMeterSigContext
                    = meterSigContextOffset && (*meterSigContextOffset == cursor);
                if (!hasPairedMeterSigContext && MeterQLFromMensur(*mensur, state.meter_ql)) {
                    state.has_meter = true;
                    state.beat_ql = Fraction(1);
                }
            }
        }
        else if (child->Is(vrv::BARLINE)) {
            const vrv::BarLine *barline = vrv_cast<const vrv::BarLine *>(child);
            BarlineLocation location = BarlineLocation::kMiddle;
            if (cursor == Fraction(0)) {
                location = BarlineLocation::kLeft;
            }
            else if (cursor == state.meter_ql) {
                location = BarlineLocation::kRight;
            }
            std::vector<SymExtra> barlineExtras
                = MakeBarlineExtras(barline->GetForm(), location, cursor, barline->GetID());
            extras.insert(extras.end(), barlineExtras.begin(), barlineExtras.end());
        }
        else if (child->Is(vrv::MNUM) || child->Is(vrv::TUPLET_NUM)
            || child->Is(vrv::TUPLET_BRACKET)) {
            // tupletNum/tupletBracket are visual companions of <tuplet>,
            // whose @num we already read
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
    AppendArticulations(obj, ev.articulations);

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
        // to True ('unacc') — see docs/symbol_mapping.md "Grace notes"
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

} // namespace verosim
