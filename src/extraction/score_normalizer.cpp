#include "verosim/extraction/score_normalizer.h"

#include <cstddef>
#include <string>
#include <vector>

#include "doc.h"
#include "durationinterface.h"
#include "layerelement.h"
#include "object.h"
#include "vrv.h"

namespace verosim {
namespace {

void CollectRhythmRepairSpaces(vrv::Object *obj, std::vector<vrv::Object *> &out)
{
    if (!obj) return;
    if (IsVerovioRhythmRepairSpace(obj)) out.push_back(obj);

    for (vrv::Object *child : obj->GetChildren()) {
        CollectRhythmRepairSpaces(child, out);
    }
}

void NeutralizeRepairSpaceDuration(vrv::Object *obj)
{
    vrv::DurationInterface *duration = obj->GetDurationInterface();
    if (!duration) return;

    duration->SetDur(vrv::DURATION_1024);
    if (duration->HasDots()) {
        duration->SetDots(0);
    }
}

} // namespace

bool IsVerovioRhythmRepairSpace(const vrv::Object *obj)
{
    if (!obj || (!obj->Is(vrv::SPACE) && !obj->Is(vrv::MSPACE))) return false;

    const auto *element = vrv_cast<const vrv::LayerElement *>(obj);
    if (!element || !element->HasType()) return false;

    const std::string type = element->GetType();
    return type == "straddle" || type == "filler";
}

std::size_t NormalizeVerovioRhythmRepairSpaces(vrv::Doc &doc)
{
    std::vector<vrv::Object *> repair_spaces;
    CollectRhythmRepairSpaces(&doc, repair_spaces);

    for (vrv::Object *space : repair_spaces) {
        NeutralizeRepairSpaceDuration(space);
    }
    return repair_spaces.size();
}

} // namespace verosim
