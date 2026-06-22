#pragma once

#include <cstddef>
#include <optional>
#include <string>

namespace vrv {
class Doc;
class Object;
}

namespace verosim {

enum class TypedSpaceHandling {
    kPreserve,
    kSuppressStraddleFiller,
};

const char *TypedSpaceHandlingName(TypedSpaceHandling handling);
std::optional<TypedSpaceHandling> ParseTypedSpaceHandling(const std::string &value);

bool IsStraddleOrFillerSpace(const vrv::Object *obj);
bool ShouldSuppressTypedSpaceDuration(const vrv::Object *obj, TypedSpaceHandling handling);
bool ShouldStripTypedSpaceSvgGroup(const std::string &class_attr, TypedSpaceHandling handling);

std::size_t NormalizeTypedSpaces(vrv::Doc &doc, TypedSpaceHandling handling);

} // namespace verosim
