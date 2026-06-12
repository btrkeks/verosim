#include "verosim/extraction/vrv_bridge.h"

#include <stdexcept>

#include "toolkitdef.h"
#include "vrv.h"

namespace verosim {

VrvBridge::VrvBridge(const VrvBridgeConfig &config) : vrv::Toolkit(false)
{
    vrv::EnableLog(config.log_level);
    vrv::EnableLogToBuffer(config.capture_log);
    if (!this->SetResourcePath(VEROSIM_VRV_DATA_DIR)) {
        throw std::runtime_error(
            "VrvBridge: cannot load Verovio resources from " VEROSIM_VRV_DATA_DIR);
    }
    this->SetOptions(R"({"breaks":"none","header":"none","footer":"none"})");
}

} // namespace verosim
