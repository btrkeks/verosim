#pragma once

#include <string>

#include "toolkit.h"

namespace verosim {

// Layer 1 wrapper: a vrv::Toolkit configured for parse-only use (breaks:none,
// no layout, no rendering — D2) that exposes the parsed Object tree, which
// Toolkit keeps protected.
class VrvBridge : public vrv::Toolkit {
public:
    // Throws std::runtime_error if the Verovio resource path is unusable
    // (LoadData refuses to run without font resources even though we never
    // render, and its own error message is misleading).
    VrvBridge();

    // Loads any supported format from a file path. LoadFile (not LoadData)
    // is required for compressed .mxl and UTF-16 inputs.
    bool LoadScoreFile(const std::string &path) { return this->LoadFile(path); }

    vrv::Doc &GetDoc() { return m_doc; }
    const vrv::Doc &GetDoc() const { return m_doc; }
};

} // namespace verosim
