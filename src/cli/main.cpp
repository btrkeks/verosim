#include <algorithm>
#include <atomic>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "verosim/cli/args.h"
#include "verosim/cli/check.h"
#include "verosim/cli/compare_cli.h"
#include "verosim/cli/count_symbols.h"
#include "verosim/cli/dump_tree.h"
#include "verosim/extraction/vrv_bridge.h"

namespace {

void PrintUsage(std::ostream &os)
{
    os << "usage: verosim <pred> <gt> [--ops] [--detail tierA|tierAB|tierAB_dir]\n"
          "                                      compare two scores, OMR-NED as JSON\n"
          "                                      (--ops includes the per-edit operation list)\n"
          "       verosim --pairs <tsv> [--base-dir <dir>] [--ops]\n"
          "                                      compare every pred<TAB>gt pair in <tsv>\n"
          "                                      (JSONL, one record per pair, always exits 0)\n"
          "       verosim --batch <tsv> [--base-dir <dir>] [--jobs N] [--ops]\n"
          "                                      parallel JSONL batch compare, one Toolkit per worker\n"
          "       verosim --dump-tree <file>     print the parsed Verovio object tree\n"
          "       verosim --check <file>         load one file, report warnings/errors (JSONL)\n"
          "       verosim --check --files-from <list> [--base-dir <dir>]\n"
          "                                      check every file in <list> (one path per line,\n"
          "                                      '#' comments; always exits 0 — failures are data)\n"
          "       verosim --count-symbols [--per-measure] <file>\n"
          "                                      Tier-A symbol counts as JSON (M2 Layer 2)\n"
          "       verosim --count-symbols --files-from <list> [--base-dir <dir>]\n"
          "                                      counts for every file in <list>, JSONL, exits 0\n"
          "Supported inputs: MusicXML, .mxl, Humdrum/kern, MEI, ... (Verovio autodetect)\n";
}

int DumpTreeMain(const std::string &path)
{
    verosim::VrvBridge bridge;
    if (!bridge.LoadScoreFile(path)) {
        std::cerr << "verosim: failed to load " << path << '\n';
        return 1;
    }
    verosim::DumpTree(&bridge.GetDoc(), std::cout);
    return 0;
}

int CompareMain(
    const std::string &predPath, const std::string &gtPath, const verosim::CompareCliOptions &options)
{
    verosim::VrvBridge bridge;
    return verosim::ComparePairToJson(bridge, predPath, gtPath, options, std::cout) ? 0 : 1;
}

int ComparePairsMain(
    const std::string &listPath, const std::string &baseDir, const verosim::CompareCliOptions &options)
{
    std::ifstream list(listPath);
    if (!list) {
        std::cerr << "verosim: cannot read pairs list " << listPath << '\n';
        return 2;
    }
    verosim::VrvBridge bridge;
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            std::cerr << "verosim: skipping malformed pairs line (no tab): " << line << '\n';
            continue;
        }
        std::string pred = line.substr(0, tab);
        std::string gt = line.substr(tab + 1);
        if (!baseDir.empty()) {
            pred = baseDir + "/" + pred;
            gt = baseDir + "/" + gt;
        }
        verosim::ComparePairToJson(bridge, pred, gt, options, std::cout);
        std::cout.flush(); // same durability contract as --check list mode
    }
    return 0; // failures are data (recorded per pair), like the other list modes
}

struct PairRow {
    std::string pred;
    std::string gt;
};

std::vector<PairRow> ReadPairsList(const std::string &listPath)
{
    std::ifstream list(listPath);
    if (!list) throw std::runtime_error("cannot read pairs list " + listPath);
    std::vector<PairRow> pairs;
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        const auto tab = line.find('\t');
        if (tab == std::string::npos) {
            std::cerr << "verosim: skipping malformed pairs line (no tab): " << line << '\n';
            continue;
        }
        pairs.push_back({ line.substr(0, tab), line.substr(tab + 1) });
    }
    return pairs;
}

int CompareBatchMain(
    const std::string &listPath, const std::string &baseDir, int jobs,
    const verosim::CompareCliOptions &options)
{
    std::vector<PairRow> pairs;
    try {
        pairs = ReadPairsList(listPath);
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 2;
    }

    if (jobs <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        jobs = hw == 0 ? 1 : static_cast<int>(hw);
    }
    jobs = std::max(1, std::min(jobs, static_cast<int>(std::max<std::size_t>(pairs.size(), 1))));

    std::vector<std::string> records(pairs.size());

    std::atomic<std::size_t> next{ 0 };
    std::vector<std::future<void>> workers;
    for (int w = 0; w < jobs; ++w) {
        workers.push_back(std::async(std::launch::async, [&] {
            verosim::VrvBridge bridge;
            for (;;) {
                const std::size_t i = next.fetch_add(1);
                if (i >= pairs.size()) break;
                std::string pred = pairs[i].pred;
                std::string gt = pairs[i].gt;
                if (!baseDir.empty()) {
                    pred = baseDir + "/" + pred;
                    gt = baseDir + "/" + gt;
                }
                std::ostringstream out;
                verosim::ComparePairToJson(bridge, pred, gt, options, out);
                records[i] = out.str();
            }
        }));
    }
    for (auto &worker : workers) worker.get();

    for (const std::string &record : records) std::cout << record;
    return 0; // failures are data, as with --pairs
}

int CheckOneMain(const std::string &path)
{
    verosim::VrvBridge bridge({ .log_level = vrv::LOG_WARNING, .capture_log = true });
    const verosim::CheckResult result = verosim::CheckFile(bridge, path);
    verosim::WriteCheckJsonl(result, std::cout);
    return result.ok ? 0 : 1;
}

int CheckListMain(const std::string &listPath, const std::string &baseDir)
{
    std::ifstream list(listPath);
    if (!list) {
        std::cerr << "verosim: cannot read file list " << listPath << '\n';
        return 2;
    }
    verosim::VrvBridge bridge({ .log_level = vrv::LOG_WARNING, .capture_log = true });
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::string path = baseDir.empty() ? line : baseDir + "/" + line;
        verosim::CheckResult result = verosim::CheckFile(bridge, path);
        result.path = line; // report the list-relative path, not the joined one
        verosim::WriteCheckJsonl(result, std::cout);
        // run_sweep.sh's crash-resume protocol requires every completed file's
        // record to be durable when a later file segfaults the process.
        std::cout.flush();
    }
    return 0;
}

int CountSymbolsOneMain(const std::string &path, bool perMeasure)
{
    verosim::VrvBridge bridge;
    const verosim::CountSymbolsOptions options{ .per_measure = perMeasure };
    return verosim::CountSymbolsFile(bridge, path, options, std::cout) ? 0 : 1;
}

int CountSymbolsListMain(const std::string &listPath, const std::string &baseDir)
{
    std::ifstream list(listPath);
    if (!list) {
        std::cerr << "verosim: cannot read file list " << listPath << '\n';
        return 2;
    }
    verosim::VrvBridge bridge;
    const verosim::CountSymbolsOptions options;
    std::string line;
    while (std::getline(list, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::string path = baseDir.empty() ? line : baseDir + "/" + line;
        verosim::CountSymbolsFile(bridge, path, options, std::cout);
        std::cout.flush(); // same durability contract as --check list mode
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    // --ops and --detail modify the compare modes only; strip before dispatch.
    verosim::CompareCliOptions compareOptions;
    std::string parseError;
    if (!verosim::StripCompareOptions(args, compareOptions, parseError)) {
        PrintUsage(std::cerr);
        return 2;
    }
    try {
        if (auto pairs = verosim::ParsePairsArgs(args)) {
            return ComparePairsMain(pairs->list_path, pairs->base_dir, compareOptions);
        }
        if (auto batch = verosim::ParseBatchArgs(args)) {
            return CompareBatchMain(batch->list_path, batch->base_dir, batch->jobs, compareOptions);
        }
        if (args.size() == 2 && args[0] == "--dump-tree") {
            return DumpTreeMain(args[1]);
        }
        if (!args.empty() && args[0] == "--check") {
            if (args.size() == 2 && args[1][0] != '-') {
                return CheckOneMain(args[1]);
            }
            std::string listPath, baseDir;
            for (std::size_t i = 1; i + 1 < args.size(); i += 2) {
                if (args[i] == "--files-from") listPath = args[i + 1];
                else if (args[i] == "--base-dir") baseDir = args[i + 1];
                else { listPath.clear(); break; }
            }
            if (!listPath.empty() && (args.size() == 3 || args.size() == 5)) {
                return CheckListMain(listPath, baseDir);
            }
        }
        if (!args.empty() && args[0] == "--count-symbols") {
            if (args.size() == 2 && args[1][0] != '-') {
                return CountSymbolsOneMain(args[1], false);
            }
            if (args.size() == 3 && args[1] == "--per-measure" && args[2][0] != '-') {
                return CountSymbolsOneMain(args[2], true);
            }
            std::string listPath, baseDir;
            for (std::size_t i = 1; i + 1 < args.size(); i += 2) {
                if (args[i] == "--files-from") listPath = args[i + 1];
                else if (args[i] == "--base-dir") baseDir = args[i + 1];
                else { listPath.clear(); break; }
            }
            if (!listPath.empty() && (args.size() == 3 || args.size() == 5)) {
                return CountSymbolsListMain(listPath, baseDir);
            }
        }
        if (args.size() == 2 && args[0][0] != '-' && args[1][0] != '-') {
            return CompareMain(args[0], args[1], compareOptions);
        }
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 1;
    }
    PrintUsage(std::cerr);
    return 2;
}
