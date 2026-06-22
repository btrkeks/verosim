#include "verosim/cli/args.h"

#include <cmath>
#include <cstddef>
#include <exception>

#include "verosim/model/metric_mode.h"

namespace verosim {

namespace {

bool IsValue(const std::string &arg)
{
    return !arg.empty() && arg[0] != '-';
}

bool MissingValue(const std::vector<std::string> &args, std::size_t i)
{
    return i + 1 >= args.size();
}

bool IsCommandFlag(const std::string &arg)
{
    return arg == "--pairs" || arg == "--batch" || arg == "--batch-jsonl"
        || arg == "--visualize" || arg == "--dump-tree" || arg == "--check"
        || arg == "--count-symbols";
}

std::optional<std::string> FirstCommandFlag(const std::vector<std::string> &args)
{
    for (std::size_t i = 0; i < args.size();) {
        if (IsCommandFlag(args[i])) return args[i];
        if (args[i] == "--ops") {
            ++i;
        }
        else if (args[i] == "--mode" || args[i] == "--detail"
            || args[i] == "--note-position" || args[i] == "--typed-space-handling") {
            i += 2;
        }
        else {
            break;
        }
    }
    return std::nullopt;
}

void EraseArgs(std::vector<std::string> &args, std::size_t first, std::size_t count)
{
    args.erase(args.begin() + static_cast<std::ptrdiff_t>(first),
        args.begin() + static_cast<std::ptrdiff_t>(first + count));
}

bool StripTypedSpaceOptions(
    std::vector<std::string> &args, TypedSpaceHandling &handling, std::string &error)
{
    for (std::size_t i = 0; i < args.size();) {
        if (args[i] == "--detail") {
            error = "--detail has been removed; use --mode active or --mode experimental";
            return false;
        }
        if (args[i] != "--typed-space-handling") {
            ++i;
            continue;
        }
        if (MissingValue(args, i)) {
            error = "--typed-space-handling requires preserve or suppress-straddle-filler";
            return false;
        }
        const std::optional<TypedSpaceHandling> parsed = ParseTypedSpaceHandling(args[i + 1]);
        if (!parsed.has_value()) {
            error = "unknown typed space handling " + args[i + 1];
            return false;
        }
        handling = *parsed;
        EraseArgs(args, i, 2);
    }
    return true;
}

bool StripModeOptions(std::vector<std::string> &args, MetricMode &mode, std::string &error)
{
    for (std::size_t i = 0; i < args.size();) {
        if (args[i] == "--detail") {
            error = "--detail has been removed; use --mode active or --mode experimental";
            return false;
        }
        if (args[i] != "--mode") {
            ++i;
            continue;
        }
        if (MissingValue(args, i)) {
            error = "--mode requires active or experimental";
            return false;
        }
        const std::optional<MetricMode> parsed = ParseMetricMode(args[i + 1]);
        if (!parsed.has_value()) {
            error = "unknown metric mode " + args[i + 1];
            return false;
        }
        mode = *parsed;
        EraseArgs(args, i, 2);
    }
    return true;
}

bool StripComparisonOptionsForCommand(std::vector<std::string> &args, CompareCliOptions &options,
    bool allow_ops, std::string &error)
{
    for (std::size_t i = 0; i < args.size();) {
        if (args[i] == "--ops") {
            if (!allow_ops) {
                error = "--ops is not valid for this command";
                return false;
            }
            options.emit_ops = true;
            EraseArgs(args, i, 1);
        }
        else if (args[i] == "--note-position") {
            if (MissingValue(args, i)) {
                error = "--note-position requires visual or musical";
                return false;
            }
            const std::optional<NotePositionPolicy> parsed = ParseNotePositionPolicy(args[i + 1]);
            if (!parsed.has_value()) {
                error = "unknown note position " + args[i + 1];
                return false;
            }
            options.note_position_policy = *parsed;
            EraseArgs(args, i, 2);
        }
        else {
            const std::size_t before = args.size();
            if (!StripModeOptions(args, options.mode, error)) return false;
            if (args.size() != before) continue;
            if (!StripTypedSpaceOptions(args, options.typed_space_handling, error)) return false;
            if (args.size() != before) continue;
            ++i;
        }
    }
    return true;
}

bool StripCountOptions(std::vector<std::string> &args, CountSymbolsCommand &command,
    std::string &error)
{
    if (!StripModeOptions(args, command.mode, error)) return false;
    return StripTypedSpaceOptions(args, command.typed_space_handling, error);
}

} // namespace

std::optional<Command> ParseCommand(const std::vector<std::string> &args, std::string &error)
{
    error.clear();
    if (args.empty()) return std::nullopt;

    const std::optional<std::string> command_flag = FirstCommandFlag(args);
    if (command_flag == "--pairs") {
        PairsCommand command;
        std::vector<std::string> stripped = args;
        if (!StripComparisonOptionsForCommand(stripped, command.options, true, error)) return std::nullopt;
        const std::optional<PairsArgs> parsed = ParsePairsArgs(stripped);
        if (!parsed.has_value()) return std::nullopt;
        command.args = *parsed;
        return command;
    }
    if (command_flag == "--batch") {
        BatchCommand command;
        std::vector<std::string> stripped = args;
        if (!StripComparisonOptionsForCommand(stripped, command.options, true, error)) return std::nullopt;
        const std::optional<BatchArgs> parsed = ParseBatchArgs(stripped);
        if (!parsed.has_value()) return std::nullopt;
        command.args = *parsed;
        return command;
    }
    if (command_flag == "--batch-jsonl") {
        BatchJsonlCommand command;
        std::vector<std::string> stripped = args;
        if (!StripComparisonOptionsForCommand(stripped, command.options, true, error)) return std::nullopt;
        const std::optional<BatchJsonlArgs> parsed = ParseBatchJsonlArgs(stripped);
        if (!parsed.has_value()) return std::nullopt;
        command.args = *parsed;
        return command;
    }
    if (command_flag == "--visualize") {
        VisualizeCommand command;
        std::vector<std::string> stripped = args;
        if (!StripComparisonOptionsForCommand(stripped, command.options, false, error)) return std::nullopt;
        const std::optional<VisualizeArgs> parsed = ParseVisualizeArgs(stripped);
        if (!parsed.has_value()) return std::nullopt;
        command.args = *parsed;
        return command;
    }
    if (command_flag == "--dump-tree") {
        DumpTreeCommand command;
        std::vector<std::string> stripped = args;
        if (!StripTypedSpaceOptions(stripped, command.typed_space_handling, error)) {
            return std::nullopt;
        }
        if (stripped.size() != 2 || stripped[0] != "--dump-tree" || !IsValue(stripped[1])) {
            if (error.empty()) error = "invalid --dump-tree arguments";
            return std::nullopt;
        }
        command.path = stripped[1];
        return command;
    }
    if (command_flag == "--check") {
        CheckCommand command;
        std::vector<std::string> stripped = args;
        if (!StripTypedSpaceOptions(stripped, command.typed_space_handling, error)) {
            return std::nullopt;
        }
        if (stripped.size() == 2 && stripped[0] == "--check" && IsValue(stripped[1])) {
            command.input_kind = CheckCommand::InputKind::kFile;
            command.path = stripped[1];
            return command;
        }
        for (std::size_t i = 1; i + 1 < stripped.size(); i += 2) {
            if (stripped[i] == "--files-from") command.list_path = stripped[i + 1];
            else if (stripped[i] == "--base-dir") command.base_dir = stripped[i + 1];
            else {
                command.list_path.clear();
                break;
            }
        }
        if (!command.list_path.empty() && (stripped.size() == 3 || stripped.size() == 5)) {
            command.input_kind = CheckCommand::InputKind::kFileList;
            return command;
        }
        if (error.empty()) error = "invalid --check arguments";
        return std::nullopt;
    }
    if (command_flag == "--count-symbols") {
        CountSymbolsCommand command;
        std::vector<std::string> stripped = args;
        if (!StripCountOptions(stripped, command, error)) return std::nullopt;
        if (stripped.size() == 2 && stripped[0] == "--count-symbols" && IsValue(stripped[1])) {
            command.input_kind = CountSymbolsCommand::InputKind::kFile;
            command.path = stripped[1];
            return command;
        }
        if (stripped.size() == 3 && stripped[1] == "--per-measure" && IsValue(stripped[2])) {
            command.input_kind = CountSymbolsCommand::InputKind::kFile;
            command.per_measure = true;
            command.path = stripped[2];
            return command;
        }
        for (std::size_t i = 1; i + 1 < stripped.size(); i += 2) {
            if (stripped[i] == "--files-from") command.list_path = stripped[i + 1];
            else if (stripped[i] == "--base-dir") command.base_dir = stripped[i + 1];
            else {
                command.list_path.clear();
                break;
            }
        }
        if (!command.list_path.empty() && (stripped.size() == 3 || stripped.size() == 5)) {
            command.input_kind = CountSymbolsCommand::InputKind::kFileList;
            return command;
        }
        if (error.empty()) error = "invalid --count-symbols arguments";
        return std::nullopt;
    }

    CompareCommand command;
    std::vector<std::string> stripped = args;
    if (!StripComparisonOptionsForCommand(stripped, command.options, true, error)) return std::nullopt;
    if (stripped.size() != 2 || !IsValue(stripped[0]) || !IsValue(stripped[1])) {
        if (error.empty()) error = "invalid compare arguments";
        return std::nullopt;
    }
    command.pred_path = stripped[0];
    command.gt_path = stripped[1];
    return command;
}

std::optional<PairsArgs> ParsePairsArgs(const std::vector<std::string> &args)
{
    if (args.empty() || args[0] != "--pairs") return std::nullopt;
    if (args.size() < 2 || args[1].empty() || args[1][0] == '-') return std::nullopt;

    PairsArgs parsed{ .list_path = args[1] };
    if (args.size() == 2) return parsed;
    if (args.size() == 4 && args[2] == "--base-dir") {
        parsed.base_dir = args[3];
        return parsed;
    }
    return std::nullopt;
}

std::optional<BatchArgs> ParseBatchArgs(const std::vector<std::string> &args)
{
    if (args.empty() || args[0] != "--batch") return std::nullopt;
    if (args.size() < 2 || args[1].empty() || args[1][0] == '-') return std::nullopt;

    BatchArgs parsed{ .list_path = args[1] };
    for (std::size_t i = 2; i < args.size();) {
        if (i + 1 >= args.size()) return std::nullopt;
        if (args[i] == "--base-dir") {
            parsed.base_dir = args[i + 1];
            i += 2;
        }
        else if (args[i] == "--jobs") {
            try {
                std::size_t consumed = 0;
                parsed.jobs = std::stoi(args[i + 1], &consumed);
                if (consumed != args[i + 1].size()) return std::nullopt;
            }
            catch (const std::exception &) {
                return std::nullopt;
            }
            i += 2;
        }
        else {
            return std::nullopt;
        }
    }
    return parsed;
}

std::optional<BatchJsonlArgs> ParseBatchJsonlArgs(const std::vector<std::string> &args)
{
    if (args.empty() || args[0] != "--batch-jsonl") return std::nullopt;
    if (args.size() < 2 || args[1].empty() || args[1][0] == '-') return std::nullopt;

    BatchJsonlArgs parsed{ .jsonl_path = args[1] };
    for (std::size_t i = 2; i < args.size();) {
        if (i + 1 >= args.size()) return std::nullopt;
        const std::string &flag = args[i];
        const std::string &value = args[i + 1];
        if (flag == "--pred-field") {
            if (value.empty()) return std::nullopt;
            parsed.pred_field = value;
        }
        else if (flag == "--gt-field") {
            if (value.empty()) return std::nullopt;
            parsed.gt_field = value;
        }
        else if (flag == "--id-field") {
            if (value.empty()) return std::nullopt;
            parsed.id_field = value;
        }
        else if (flag == "--group-field") {
            if (value.empty()) return std::nullopt;
            parsed.group_field = value;
        }
        else if (flag == "--dedupe-key") {
            if (value.empty()) return std::nullopt;
            parsed.dedupe_key = value;
        }
        else if (flag == "--summary-json") {
            if (value.empty()) return std::nullopt;
            parsed.summary_json = value;
        }
        else if (flag == "--format") {
            if (value != "kern" && value != "humdrum") return std::nullopt;
            parsed.format = value;
        }
        else if (flag == "--failure-score") {
            try {
                std::size_t consumed = 0;
                parsed.failure_score = std::stod(value, &consumed);
                if (consumed != value.size() || !std::isfinite(parsed.failure_score)) {
                    return std::nullopt;
                }
            }
            catch (const std::exception &) {
                return std::nullopt;
            }
        }
        else if (flag == "--jobs") {
            try {
                std::size_t consumed = 0;
                parsed.jobs = std::stoi(value, &consumed);
                if (consumed != value.size()) return std::nullopt;
            }
            catch (const std::exception &) {
                return std::nullopt;
            }
        }
        else {
            return std::nullopt;
        }
        i += 2;
    }
    return parsed;
}

std::optional<VisualizeArgs> ParseVisualizeArgs(const std::vector<std::string> &args)
{
    if (args.empty() || args[0] != "--visualize") return std::nullopt;
    if (args.size() != 5 && args.size() != 7) return std::nullopt;
    if (args[1].empty() || args[1][0] == '-') return std::nullopt;
    if (args[2].empty() || args[2][0] == '-') return std::nullopt;

    VisualizeArgs parsed{ .pred_path = args[1], .gt_path = args[2] };
    bool saw_out = false;
    bool saw_out_dir = false;
    bool saw_output_format = false;
    for (std::size_t i = 3; i < args.size(); i += 2) {
        if (i + 1 >= args.size()) return std::nullopt;
        const std::string &flag = args[i];
        const std::string &value = args[i + 1];
        if (value.empty() || value[0] == '-') return std::nullopt;
        if (flag == "--out") {
            if (saw_out) return std::nullopt;
            saw_out = true;
            parsed.out_path = value;
        }
        else if (flag == "--out-dir") {
            if (saw_out_dir) return std::nullopt;
            saw_out_dir = true;
            parsed.out_dir = value;
        }
        else if (flag == "--output-format") {
            if (saw_output_format) return std::nullopt;
            saw_output_format = true;
            parsed.output_format = value;
        }
        else {
            return std::nullopt;
        }
    }

    if (saw_out) {
        if (saw_out_dir || saw_output_format) return std::nullopt;
        parsed.output_kind = VisualizeArgs::OutputKind::kHtml;
        return parsed;
    }
    if (saw_out_dir && saw_output_format && parsed.output_format == "svg") {
        parsed.output_kind = VisualizeArgs::OutputKind::kSvgBundle;
        return parsed;
    }
    return std::nullopt;
}

} // namespace verosim
