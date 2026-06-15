#pragma once

#include <cstddef>

namespace vrv {
class Doc;
class Object;
}

namespace verosim {

bool IsVerovioRhythmRepairSpace(const vrv::Object *obj);

std::size_t NormalizeVerovioRhythmRepairSpaces(vrv::Doc &doc);

} // namespace verosim
