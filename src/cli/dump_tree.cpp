#include "verosim/cli/dump_tree.h"

#include <string>

#include "object.h"
#include "score.h"

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

    // The initial scoreDef is not a child of the score milestone element; it is
    // kept on the Score object itself (score.h m_scoreDefSubtree) and would be
    // invisible to a plain children walk.
    if (obj->Is(vrv::SCORE)) {
        const vrv::Object *subtree = const_cast<vrv::Score *>(vrv_cast<const vrv::Score *>(obj))->GetScoreDefSubtree();
        if (subtree) {
            DumpTree(subtree, os, depth + 1);
        }
    }

    for (const vrv::Object *child : obj->GetChildren()) {
        DumpTree(child, os, depth + 1);
    }
}

} // namespace verosim
