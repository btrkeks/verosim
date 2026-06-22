#pragma once

#include <optional>
#include <string>
#include <variant>
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

struct VisualizeArgs {
    enum class OutputKind {
        kHtml,
        kSvgBundle,
    };

    std::string pred_path;
    std::string gt_path;
    std::string out_path;
    std::string out_dir;
    std::string output_format;
    OutputKind output_kind = OutputKind::kHtml;
};

struct CompareCommand {
    std::string pred_path;
    std::string gt_path;
    CompareCliOptions options;
};

struct PairsCommand {
    PairsArgs args;
    CompareCliOptions options;
};

struct BatchCommand {
    BatchArgs args;
    CompareCliOptions options;
};

struct BatchJsonlCommand {
    BatchJsonlArgs args;
    CompareCliOptions options;
};

struct VisualizeCommand {
    VisualizeArgs args;
    CompareCliOptions options;
};

struct DumpTreeCommand {
    std::string path;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
};

struct CheckCommand {
    enum class InputKind {
        kFile,
        kFileList,
    };

    InputKind input_kind = InputKind::kFile;
    std::string path;
    std::string list_path;
    std::string base_dir;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
};

struct CountSymbolsCommand {
    enum class InputKind {
        kFile,
        kFileList,
    };

    InputKind input_kind = InputKind::kFile;
    std::string path;
    std::string list_path;
    std::string base_dir;
    bool per_measure = false;
    MetricMode mode = MetricMode::kActive;
    TypedSpaceHandling typed_space_handling = TypedSpaceHandling::kSuppressStraddleFiller;
};

using Command = std::variant<CompareCommand, PairsCommand, BatchCommand, BatchJsonlCommand,
    VisualizeCommand, DumpTreeCommand, CheckCommand, CountSymbolsCommand>;

std::optional<Command> ParseCommand(const std::vector<std::string> &args, std::string &error);
std::optional<PairsArgs> ParsePairsArgs(const std::vector<std::string> &args);
std::optional<BatchArgs> ParseBatchArgs(const std::vector<std::string> &args);
std::optional<BatchJsonlArgs> ParseBatchJsonlArgs(const std::vector<std::string> &args);
std::optional<VisualizeArgs> ParseVisualizeArgs(const std::vector<std::string> &args);

} // namespace verosim
