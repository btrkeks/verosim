#include "extract_detail.h"

#include <cstdint>
#include <string>

namespace verosim {
namespace extract_detail {

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

bool MeterQLFromMeterSig(const vrv::MeterSig &metersig, Fraction &ql)
{
    const vrv::data_METERCOUNT_pair count = metersig.GetCount();
    int total = 0;
    for (const int c : count.first) total += c;
    if (total > 0 && metersig.GetUnit() > 0) {
        ql = Fraction(4 * total, metersig.GetUnit());
        return true;
    }
    if (metersig.GetSym() != vrv::METERSIGN_NONE) {
        ql = Fraction(4); // common or cut: 4 QL either way
        return true;
    }
    return false;
}

bool MeterQLFromMensur(const vrv::Mensur &mensur, Fraction &ql)
{
    if (!ModernMensurSymbol(mensur)) return false;
    ql = Fraction(4); // modern common/cut fallback when no numeric meter exists
    return true;
}

Fraction BeatQLFromMeterSig(const vrv::MeterSig &metersig)
{
    if (metersig.GetUnit() > 0) return Fraction(4, metersig.GetUnit());
    return Fraction(1);
}

Fraction FractionFromDouble(double value)
{
    return Fraction(static_cast<std::int64_t>(value * 10000), 10000);
}

} // namespace extract_detail
} // namespace verosim
