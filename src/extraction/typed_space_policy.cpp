#include "verosim/extraction/typed_space_policy.h"

#include <string>
#include <vector>

#include "doc.h"
#include "durationinterface.h"
#include "layerelement.h"
#include "object.h"
#include "vrv.h"

namespace verosim {
namespace {

bool IsSuppressionMode(TypedSpaceHandling handling)
{
    return handling == TypedSpaceHandling::kSuppressStraddleFiller;
}

bool IsStraddleOrFillerValue(const std::string &value)
{
    return value == "straddle" || value == "filler";
}

bool HasClassToken(const std::string &classes, const std::string &token)
{
    std::size_t pos = 0;
    while (pos < classes.size()) {
        while (pos < classes.size() && classes[pos] == ' ') ++pos;
        const std::size_t start = pos;
        while (pos < classes.size() && classes[pos] != ' ') ++pos;
        if (classes.substr(start, pos - start) == token) return true;
    }
    return false;
}

void CollectTypedSpaces(vrv::Object *obj, std::vector<vrv::Object *> &out)
{
    if (!obj) return;
    if (IsStraddleOrFillerSpace(obj)) out.push_back(obj);

    for (vrv::Object *child : obj->GetChildren()) {
        CollectTypedSpaces(child, out);
    }
}

void NeutralizeDuration(vrv::Object *obj)
{
    vrv::DurationInterface *duration = obj->GetDurationInterface();
    if (!duration) return;

    duration->SetDur(vrv::DURATION_1024);
    if (duration->HasDots()) {
        duration->SetDots(0);
    }
}

} // namespace

const char *TypedSpaceHandlingName(TypedSpaceHandling handling)
{
    switch (handling) {
        case TypedSpaceHandling::kPreserve: return "preserve";
        case TypedSpaceHandling::kSuppressStraddleFiller: return "suppress-straddle-filler";
    }
    return "suppress-straddle-filler";
}

std::optional<TypedSpaceHandling> ParseTypedSpaceHandling(const std::string &value)
{
    if (value == "preserve") return TypedSpaceHandling::kPreserve;
    if (value == "suppress-straddle-filler") {
        return TypedSpaceHandling::kSuppressStraddleFiller;
    }
    return std::nullopt;
}

bool IsStraddleOrFillerSpace(const vrv::Object *obj)
{
    if (!obj || (!obj->Is(vrv::SPACE) && !obj->Is(vrv::MSPACE))) return false;

    const auto *element = vrv_cast<const vrv::LayerElement *>(obj);
    if (!element || !element->HasType()) return false;

    return IsStraddleOrFillerValue(element->GetType());
}

bool ShouldSuppressTypedSpaceDuration(const vrv::Object *obj, TypedSpaceHandling handling)
{
    return IsSuppressionMode(handling) && IsStraddleOrFillerSpace(obj);
}

bool ShouldStripTypedSpaceSvgGroup(const std::string &class_attr, TypedSpaceHandling handling)
{
    return IsSuppressionMode(handling) && HasClassToken(class_attr, "space")
        && (HasClassToken(class_attr, "straddle") || HasClassToken(class_attr, "filler"));
}

std::size_t NormalizeTypedSpaces(vrv::Doc &doc, TypedSpaceHandling handling)
{
    if (!IsSuppressionMode(handling)) return 0;

    std::vector<vrv::Object *> typed_spaces;
    CollectTypedSpaces(&doc, typed_spaces);

    for (vrv::Object *space : typed_spaces) {
        NeutralizeDuration(space);
    }
    return typed_spaces.size();
}

} // namespace verosim
