#pragma once

#include "toolkitdef.h"

#include "verosim/extraction/extract.h"
#include "verosim/extraction/vrv_bridge.h"

namespace verosim {

inline SourceFormat SourceFormatFromBridge(const VrvBridge &bridge)
{
    switch (bridge.last_input_format()) {
        case vrv::HUMDRUM: return SourceFormat::kKern;
        case vrv::MUSICXML:
        case vrv::MUSICXMLHUM: return SourceFormat::kMusicXml;
        default: return SourceFormat::kOther;
    }
}

} // namespace verosim
