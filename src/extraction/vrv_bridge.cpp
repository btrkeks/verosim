#include "verosim/extraction/vrv_bridge.h"

#include <fstream>
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

bool VrvBridge::LoadScoreFile(const std::string &path)
{
    // IdentifyInputFrom only looks at the head of the data (regex search over
    // the first 2000 bytes), so a prefix read is enough.
    m_lastInputFormat = vrv::UNKNOWN;
    this->SetInputFrom("auto");
    std::ifstream in(path, std::ios::binary);
    if (in) {
        std::string head(2100, '\0');
        in.read(head.data(), static_cast<std::streamsize>(head.size()));
        head.resize(static_cast<std::size_t>(in.gcount()));
        if (head.rfind("PK", 0) == 0) {
            // zip signature (Toolkit::IsZip is private): a compressed .mxl,
            // whose payload is MusicXML
            m_lastInputFormat = vrv::MUSICXML;
        }
        else {
            m_lastInputFormat = this->IdentifyInputFrom(head);
        }
    }
    return this->LoadFile(path);
}

bool VrvBridge::LoadScoreData(const std::string &data, vrv::FileFormat format)
{
    m_lastInputFormat = format;
    switch (format) {
        case vrv::HUMDRUM: this->SetInputFrom("humdrum"); break;
        case vrv::MUSICXML: this->SetInputFrom("musicxml"); break;
        case vrv::MUSICXMLHUM: this->SetInputFrom("musicxml-hum"); break;
        case vrv::MEI: this->SetInputFrom("mei"); break;
        default:
            this->SetInputFrom("auto");
            m_lastInputFormat = this->IdentifyInputFrom(data);
            break;
    }
    return this->LoadData(data);
}

} // namespace verosim
