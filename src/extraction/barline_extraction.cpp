#include "extract_internal.h"

#include <optional>
#include <string>
#include <vector>

namespace verosim {

namespace {

bool IsBoundaryLocation(BarlineLocation location)
{
    return location == BarlineLocation::kLeft || location == BarlineLocation::kRight;
}

std::optional<std::string> BarlineSymbol(vrv::data_BARRENDITION form)
{
    switch (form) {
        case vrv::BARRENDITION_dashed: return "dashed";
        case vrv::BARRENDITION_dotted: return "dotted";
        case vrv::BARRENDITION_dbl: return "double";
        case vrv::BARRENDITION_dbldashed: return "dbldashed";
        case vrv::BARRENDITION_dbldotted: return "dbldotted";
        case vrv::BARRENDITION_dblheavy: return "heavy-heavy";
        case vrv::BARRENDITION_dblsegno: return "dblsegno";
        case vrv::BARRENDITION_end: return "final";
        case vrv::BARRENDITION_heavy: return "heavy";
        case vrv::BARRENDITION_segno: return "segno";
        case vrv::BARRENDITION_single: return "regular";
        case vrv::BARRENDITION_NONE:
        case vrv::BARRENDITION_invis:
        case vrv::BARRENDITION_rptstart:
        case vrv::BARRENDITION_rptboth:
        case vrv::BARRENDITION_rptend:
        case vrv::BARRENDITION_MAX: return std::nullopt;
    }
    return std::nullopt;
}

SymExtra MakeRepeatExtra(const std::string &id, const Fraction &offset, const std::string &symbolic,
    const std::string &direction)
{
    SymExtra extra;
    extra.vrv_id = id;
    extra.kind = ExtraKind::kRepeat;
    extra.offset = offset;
    extra.symbolic = symbolic;
    extra.infodict.emplace_back("repeatdirection", direction);
    return extra;
}

SymExtra MakePlainBarlineExtra(
    const std::string &id, const Fraction &offset, const std::string &symbolic)
{
    SymExtra extra;
    extra.vrv_id = id;
    extra.kind = ExtraKind::kBarline;
    extra.offset = offset;
    extra.symbolic = symbolic;
    return extra;
}

std::string BarlineId(
    const vrv::Measure *measure, const vrv::BarLine *barline, const std::string &suffix)
{
    if (barline && !barline->GetID().empty()) return barline->GetID();
    if (!measure->GetID().empty()) return measure->GetID() + ":" + suffix;
    return suffix;
}

} // namespace

std::vector<SymExtra> Extractor::MakeBarlineExtras(vrv::data_BARRENDITION form,
    BarlineLocation location, const Fraction &offset, const std::string &id)
{
    std::vector<SymExtra> extras;
    if (!MetricModeIncludesBarlines(options_.mode)) return extras;

    switch (form) {
        case vrv::BARRENDITION_NONE:
        case vrv::BARRENDITION_invis:
        case vrv::BARRENDITION_MAX: return extras;
        case vrv::BARRENDITION_rptstart:
            extras.push_back(MakeRepeatExtra(id, offset, "heavy-light", "start"));
            return extras;
        case vrv::BARRENDITION_rptend:
            extras.push_back(MakeRepeatExtra(id, offset, "final", "end"));
            return extras;
        case vrv::BARRENDITION_rptboth:
            extras.push_back(MakeRepeatExtra(id + ":end", offset, "final", "end"));
            extras.push_back(MakeRepeatExtra(id + ":start", offset, "heavy-light", "start"));
            return extras;
        default: break;
    }

    const std::optional<std::string> symbolic = BarlineSymbol(form);
    if (!symbolic.has_value()) return extras;
    if (*symbolic == "regular" && IsBoundaryLocation(location)) return extras;

    extras.push_back(MakePlainBarlineExtra(id, offset, *symbolic));
    return extras;
}

void Extractor::CollectMeasureBarlines(const vrv::Measure *measure, const std::string &staffN,
    const Fraction &measureSpan, std::vector<SymExtra> &extras)
{
    if (!MetricModeIncludesBarlines(options_.mode)) return;

    const int staffNumber = staffN.empty() ? 0 : std::stoi(staffN);
    const vrv::data_BARRENDITION left = measure->HasLeft()
        ? measure->GetLeft()
        : measure->GetDrawingLeftBarLineByStaffN(staffNumber);
    const vrv::data_BARRENDITION right = measure->HasRight()
        ? measure->GetRight()
        : measure->GetDrawingRightBarLineByStaffN(staffNumber);

    std::vector<SymExtra> leftExtras = MakeBarlineExtras(left, BarlineLocation::kLeft,
        Fraction(0), BarlineId(measure, measure->GetLeftBarLine(), "leftBarline"));
    extras.insert(extras.end(), leftExtras.begin(), leftExtras.end());

    std::vector<SymExtra> rightExtras = MakeBarlineExtras(right, BarlineLocation::kRight,
        measureSpan, BarlineId(measure, measure->GetRightBarLine(), "rightBarline"));
    extras.insert(extras.end(), rightExtras.begin(), rightExtras.end());
}

} // namespace verosim
