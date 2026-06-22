#pragma once

#include <cstddef>
#include <string>

#include "toolkit.h"
#include "toolkitdef.h"
#include "verosim/extraction/typed_space_policy.h"

namespace verosim {

struct VrvBridgeConfig {
    // Verovio logging is process-global state (vrv::logLevel, vrv::logBuffer),
    // not per-Toolkit: the last constructed bridge wins, and capture_log only
    // works single-threaded. Consistent with one-Toolkit-per-thread (D7); this
    // hook is what the parse-coverage audit and deferred D4 consume (D13).
    vrv::LogLevel log_level = vrv::LOG_OFF;
    bool capture_log = false;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
};

// Layer 1 wrapper: a vrv::Toolkit configured for parse-only use (breaks:none,
// no layout, no rendering — D2) that exposes the parsed Object tree, which
// Toolkit keeps protected.
class VrvBridge : public vrv::Toolkit {
public:
    // Throws std::runtime_error if the Verovio resource path is unusable
    // (LoadData refuses to run without font resources even though we never
    // render, and its own error message is misleading).
    explicit VrvBridge(const VrvBridgeConfig &config = {});

    // Loads any supported format from a file path. LoadFile (not LoadData)
    // is required for compressed .mxl and UTF-16 inputs. Records the
    // identified input format (LoadData identifies but does not store it),
    // which extraction needs for format-specific conventions (grace slash).
    bool LoadScoreFile(const std::string &path);

    // Loads score data from memory using the requested Verovio input format.
    // JSONL validation mode uses HUMDRUM/kern by default and does not repair
    // malformed or headerless inputs.
    bool LoadScoreData(const std::string &data, vrv::FileFormat format = vrv::HUMDRUM);

    // Input format of the most recent LoadScoreFile (UNKNOWN before any load;
    // ZIP is resolved to MUSICXML — .mxl payloads are MusicXML).
    vrv::FileFormat last_input_format() const { return m_lastInputFormat; }

    TypedSpaceHandling typed_space_handling() const { return m_typedSpaceHandling; }
    void set_typed_space_handling(TypedSpaceHandling handling) { m_typedSpaceHandling = handling; }

    std::size_t last_normalized_rhythm_repair_spaces() const
    {
        return m_lastNormalizedRhythmRepairSpaces;
    }

    // With capture_log: the buffered log for the most recent load, cleared on
    // read. LoadFile resets the buffer on entry, so reusing one bridge across
    // files yields clean per-file capture. Verovio dedups identical messages
    // within one load, so line counts are unique-message counts.
    std::string TakeLog() { return this->GetLog(); }

    vrv::Doc &GetDoc() { return m_doc; }
    const vrv::Doc &GetDoc() const { return m_doc; }

private:
    void NormalizeImportedScore();

    vrv::FileFormat m_lastInputFormat = vrv::UNKNOWN;
    TypedSpaceHandling m_typedSpaceHandling = TypedSpaceHandling::kSuppressStraddleFiller;
    std::size_t m_lastNormalizedRhythmRepairSpaces = 0;
};

} // namespace verosim
