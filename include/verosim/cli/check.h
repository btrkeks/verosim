#pragma once

#include <ostream>
#include <string>
#include <vector>

namespace verosim {

class VrvBridge;

// Outcome of loading one file, with the per-file Verovio log split by level.
// Backs `verosim --check` (parse-coverage sweep). Requires a bridge built
// with capture_log = true; warnings additionally need log_level >= LOG_WARNING.
struct CheckResult {
    std::string path;
    bool ok = false;        // LoadScoreFile returned true
    bool exception = false; // load threw (message recorded in errors)
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    long load_ms = 0;
};

CheckResult CheckFile(VrvBridge &bridge, const std::string &path);

// One JSON object per line (JSONL), newline-terminated.
void WriteCheckJsonl(const CheckResult &result, std::ostream &os);

} // namespace verosim
