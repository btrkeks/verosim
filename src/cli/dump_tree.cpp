#include "verosim/cli/dump_tree.h"

#include <string>

#include "object.h"

namespace verosim {

void DumpTree(const vrv::Object *obj, std::ostream &os, int depth)
{
    vrv::ArrayOfStrAttr attrs;
    obj->GetAttributes(&attrs);

    os << std::string(2 * static_cast<size_t>(depth), ' ') << obj->GetClassName() << " #"
       << obj->GetID();
    for (const auto &[name, value] : attrs) {
        os << ' ' << name << "=\"" << value << '"';
    }
    os << '\n';

    for (const vrv::Object *child : obj->GetChildren()) {
        DumpTree(child, os, depth + 1);
    }
}

} // namespace verosim
