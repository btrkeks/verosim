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

struct BatchJsonlArgs {
    std::string jsonl_path;
    std::string pred_field = "prediction";
    std::string gt_field = "target";
    std::string id_field = "sample_id";
    std::string group_field = "val_set";
    std::string dedupe_key;
    std::string summary_json;
    std::string format = "kern";
    double failure_score = 100.0;
    int jobs = 0;
};

bool StripCompareOptions(
    std::vector<std::string> &args, CompareCliOptions &options, std::string &error);
std::optional<PairsArgs> ParsePairsArgs(const std::vector<std::string> &args);
std::optional<BatchArgs> ParseBatchArgs(const std::vector<std::string> &args);
std::optional<BatchJsonlArgs> ParseBatchJsonlArgs(const std::vector<std::string> &args);

} // namespace verosim
