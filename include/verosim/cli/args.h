#pragma once

#include <optional>
#include <string>
#include <vector>

#include "verosim/cli/compare_cli.h"

namespace verosim {

struct PairsArgs {
    std::string list_path;
    std::string base_dir;
};

struct BatchArgs {
    std::string list_path;
    std::string base_dir;
    int jobs = 1;
};

bool StripCompareOptions(
    std::vector<std::string> &args, CompareCliOptions &options, std::string &error);
std::optional<PairsArgs> ParsePairsArgs(const std::vector<std::string> &args);
std::optional<BatchArgs> ParseBatchArgs(const std::vector<std::string> &args);

} // namespace verosim
