#include "verosim/extraction/vrv_bridge.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

#include "toolkitdef.h"
#include "verosim/extraction/score_normalizer.h"
#include "vrv.h"

namespace verosim {
namespace {

std::string ExecutableDir()
{
#if defined(__linux__)
    std::vector<char> buffer(4096);
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) return {};
    buffer[static_cast<std::size_t>(length)] = '\0';
    return std::filesystem::path(buffer.data()).parent_path().string();
#else
    return {};
#endif
}

std::vector<std::string> ResourcePathCandidates()
{
    std::vector<std::string> candidates;
    if (const char *envPath = std::getenv("VEROSIM_VRV_DATA_DIR"); envPath && envPath[0] != '\0') {
        candidates.emplace_back(envPath);
    }

    const std::filesystem::path exeDir = ExecutableDir();
    if (!exeDir.empty()) {
        candidates.push_back((exeDir / "verovio" / "data").string());
        candidates.push_back((exeDir.parent_path() / "verovio" / "data").string());
        candidates.push_back((exeDir.parent_path() / "share" / "verovio").string());
    }

    candidates.emplace_back(VEROSIM_VRV_DATA_DIR);
    return candidates;
}

std::string JoinTriedPaths(const std::vector<std::string> &paths)
{
    std::ostringstream out;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        if (i != 0) out << ", ";
        out << paths[i];
    }
    return out.str();
}

bool LooksLikeResourcePath(const std::string &path)
{
    if (path.empty()) return false;
    const std::filesystem::path root(path);
    return std::filesystem::is_regular_file(root / "Bravura.xml")
        && std::filesystem::is_regular_file(root / "Leipzig.xml")
        && std::filesystem::is_regular_file(root / "text" / "Times.xml");
}

} // namespace

VrvBridge::VrvBridge(const VrvBridgeConfig &config)
    : vrv::Toolkit(false), m_normalizeRepairSpaces(config.normalize_repair_spaces)
{
    vrv::EnableLog(config.log_level);
    vrv::EnableLogToBuffer(config.capture_log);
    const std::vector<std::string> resourcePaths = ResourcePathCandidates();
    bool loadedResources = false;
    for (const std::string &resourcePath : resourcePaths) {
        if (!LooksLikeResourcePath(resourcePath)) continue;
        if (this->SetResourcePath(resourcePath)) {
            loadedResources = true;
            break;
        }
    }
    if (!loadedResources) {
        throw std::runtime_error(
            "VrvBridge: cannot load Verovio resources; tried: " + JoinTriedPaths(resourcePaths)
            + ". Set VEROSIM_VRV_DATA_DIR to the Verovio data directory.");
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
    const bool loaded = this->LoadFile(path);
    if (loaded) NormalizeImportedScore();
    else m_lastNormalizedRhythmRepairSpaces = 0;
    return loaded;
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
    const bool loaded = this->LoadData(data);
    if (loaded) NormalizeImportedScore();
    else m_lastNormalizedRhythmRepairSpaces = 0;
    return loaded;
}

void VrvBridge::NormalizeImportedScore()
{
    if (!m_normalizeRepairSpaces) {
        m_lastNormalizedRhythmRepairSpaces = 0;
        return;
    }
    m_lastNormalizedRhythmRepairSpaces = NormalizeVerovioRhythmRepairSpaces(this->GetDoc());
}

} // namespace verosim
