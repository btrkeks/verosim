#pragma once

#include <optional>
#include <string>

namespace verosim {

// Text score formats accepted by the in-memory JSONL interface. Compressed
// MusicXML (.mxl) is intentionally absent because JSON strings cannot carry
// its ZIP container without an additional binary encoding contract.
enum class ScoreDataFormat {
    kKern,
    kMusicXml,
    kMei,
    kAuto,
};

std::optional<ScoreDataFormat> ParseScoreDataFormat(const std::string &value);
const char *ScoreDataFormatName(ScoreDataFormat format);

} // namespace verosim
