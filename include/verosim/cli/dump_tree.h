#pragma once

#include <ostream>

namespace vrv {
class Object;
}

namespace verosim {

// Prints the Verovio Object tree (class name, ID, all set MEI attributes),
// one indented line per node. Debugging aid behind `verosim --dump-tree`.
void DumpTree(const vrv::Object *obj, std::ostream &os, int depth = 0);

} // namespace verosim
