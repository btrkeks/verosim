#include "verosim/cli/args.h"

#include <cstddef>
#include <exception>

#include "verosim/model/detail_tier.h"

namespace verosim {

bool StripCompareOptions(
    std::vector<std::string> &args, CompareCliOptions &options, std::string &error)
{
    for (std::size_t i = 0; i < args.size();) {
        if (args[i] == "--ops") {
            options.emit_ops = true;
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i));
        }
        else if (args[i] == "--detail") {
            if (i + 1 >= args.size()) {
                error = "--detail requires tierA, tierAB, or tierAB_dir";
                return false;
            }
            const std::optional<DetailTier> detail = ParseDetailTier(args[i + 1]);
            if (!detail.has_value()) {
                error = "unknown detail tier " + args[i + 1];
                return false;
            }
            options.detail = *detail;
            args.erase(args.begin() + static_cast<std::ptrdiff_t>(i),
                args.begin() + static_cast<std::ptrdiff_t>(i + 2));
        }
        else {
            ++i;
        }
    }
    return true;
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

} // namespace verosim
