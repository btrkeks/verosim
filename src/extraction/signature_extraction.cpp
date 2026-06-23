#include "extract_internal.h"

#include <optional>
#include <string>

namespace verosim {

using namespace extract_detail;

namespace extract_detail {

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

std::optional<std::string> ModernMensurSymbol(const vrv::Mensur &mensur)
{
    if (!mensur.HasSign() || mensur.GetSign() != vrv::MENSURATIONSIGN_C) return std::nullopt;
    if (mensur.HasDot() && mensur.GetDot() == vrv::BOOLEAN_true) return std::nullopt;
    if (mensur.HasOrient()) return std::nullopt;
    if (mensur.HasNum() || mensur.HasNumbase()) return std::nullopt;
    return mensur.HasSlash() ? std::optional<std::string>("cut")
                             : std::optional<std::string>("common");
}

bool IsVisibleMeterSig(const vrv::MeterSig &metersig)
{
    return metersig.GetVisible() != vrv::BOOLEAN_false;
}

} // namespace extract_detail

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

SymExtra Extractor::MakeMensurTimeSigExtra(const vrv::Mensur &mensur, const Fraction &offset)
{
    SymExtra extra;
    extra.vrv_id = mensur.GetID();
    extra.kind = ExtraKind::kTimeSig;
    extra.offset = offset;
    if (const std::optional<std::string> symbol = ModernMensurSymbol(mensur)) {
        extra.infodict.emplace_back("symbol", *symbol);
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
    if (metersig.GetForm() != vrv::METERFORM_num) {
        // MusicXML symbol="single-number" (iomusxml: @form="num") shows the
        // numerator only; m21 timesig_to_infodict then emits one entry
        extra.infodict.emplace_back("denominator", std::to_string(metersig.GetUnit()));
    }
    return extra;
}

SymExtra Extractor::MakeDynamicExtra(const vrv::Dynam &dynam, const Fraction &offset)
{
    SymExtra extra;
    extra.vrv_id = dynam.GetID();
    extra.kind = ExtraKind::kDynamic;
    extra.offset = offset;
    const std::string text = vrv::UTF32to8(dynam.GetText());
    // musicdiff stores Dynamic.value via dynamic_to_symbolic(); content is only
    // for TextExpression-like direction payloads.
    if (!text.empty()) extra.symbolic = text;
    return extra;
}

SymExtra Extractor::MakeHairpinExtra(const vrv::Hairpin &hairpin, const Fraction &offset,
    const std::optional<Fraction> &duration)
{
    SymExtra extra;
    extra.vrv_id = hairpin.GetID();
    extra.kind = hairpin.GetForm() == vrv::hairpinLog_FORM_dim ? ExtraKind::kDiminuendo
                                                               : ExtraKind::kCrescendo;
    extra.offset = offset;
    extra.duration = duration;
    return extra;
}

SymExtra Extractor::MakeSlurExtra(const vrv::Slur &slur, const Fraction &offset,
    const std::optional<Fraction> &duration)
{
    SymExtra extra;
    extra.vrv_id = slur.GetID();
    extra.kind = ExtraKind::kSlur;
    extra.offset = offset;
    extra.duration = duration;
    return extra;
}

std::optional<SymExtra> Extractor::MakeOttavaExtra(const vrv::Octave &octave,
    const Fraction &offset, const std::optional<Fraction> &duration)
{
    if (!octave.HasDis() || !octave.HasDisPlace()) {
        Warn("octave control lacks a supported displacement or placement");
        return std::nullopt;
    }

    std::string symbolic;
    const bool below = octave.GetDisPlace() == vrv::STAFFREL_basic_below;
    switch (octave.GetDis()) {
        case vrv::OCTAVE_DIS_8: symbolic = below ? "8vb" : "8va"; break;
        case vrv::OCTAVE_DIS_15: symbolic = below ? "15mb" : "15ma"; break;
        case vrv::OCTAVE_DIS_22:
            Warn("octave control with 22ma/22mb displacement is unsupported");
            return std::nullopt;
        default:
            Warn("octave control lacks a supported displacement or placement");
            return std::nullopt;
    }

    SymExtra extra;
    extra.vrv_id = octave.GetID();
    extra.kind = ExtraKind::kOttava;
    extra.symbolic = symbolic;
    extra.offset = offset;
    extra.duration = duration;
    return extra;
}

} // namespace verosim
