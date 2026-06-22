#include "verosim/extraction/score_normalizer.h"

#include <cstddef>

#include "verosim/extraction/typed_space_policy.h"

namespace verosim {

bool IsVerovioRhythmRepairSpace(const vrv::Object *obj)
{
    return IsStraddleOrFillerSpace(obj);
}

std::size_t NormalizeVerovioRhythmRepairSpaces(vrv::Doc &doc)
{
    return NormalizeTypedSpaces(doc, TypedSpaceHandling::kSuppressStraddleFiller);
}

} // namespace verosim
