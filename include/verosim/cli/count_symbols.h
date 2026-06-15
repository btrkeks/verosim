#pragma once

#include <ostream>
#include <string>

namespace verosim {

class VrvBridge;

struct CountSymbolsOptions {
    bool per_measure = false; // include per-part/measure/note triage detail
};

// Load one file, extract the Tier-A SymScore, and write one JSON record
// (single line unless per_measure). Errors are data: {"ok":false,...} and a
// false return, never a throw. The record schema mirrors count_oracle.py.
bool CountSymbolsFile(VrvBridge &bridge, const std::string &path,
    const CountSymbolsOptions &options, std::ostream &os);

} // namespace verosim
