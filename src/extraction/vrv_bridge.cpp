#include "verosim/extraction/vrv_bridge.h"

#include <stdexcept>

#include "toolkitdef.h"
#include "vrv.h"

namespace verosim {

VrvBridge::VrvBridge() : vrv::Toolkit(false)
{
    vrv::EnableLog(vrv::LOG_ERROR);
    if (!this->SetResourcePath(VEROSIM_VRV_DATA_DIR)) {
        throw std::runtime_error(
            "VrvBridge: cannot load Verovio resources from " VEROSIM_VRV_DATA_DIR);
    }
    this->SetOptions(R"({"breaks":"none","header":"none","footer":"none"})");
}

} // namespace verosim
