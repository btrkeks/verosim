#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "verosim/cli/args.h"
#include "verosim/cli/batch.h"
#include "verosim/cli/check.h"
#include "verosim/cli/compare_cli.h"
#include "verosim/cli/count_symbols.h"
#include "verosim/cli/dump_tree.h"
#include "verosim/cli/list_io.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/visual/svg_bundle.h"
#include "verosim/visual/visualize.h"

namespace {

void PrintUsage(std::ostream &os)
{
    os << "usage: verosim <pred> <gt> [--ops] [--detail tierA|tierAB|tierAB_dir]\n"
          "                                      [--note-position visual|musical]\n"
          "                                      compare two scores, OMR-NED as JSON\n"
          "                                      (--ops includes the per-edit operation list)\n"
          "       verosim --visualize <pred> <gt> --out <html> [--detail tierA|tierAB|tierAB_dir]\n"
          "                                      [--note-position visual|musical]\n"
          "                                      compare and write a side-by-side SVG HTML report\n"
          "       verosim --visualize <pred> <gt> --out-dir <dir> --output-format svg\n"
          "                                      compare and write raw annotated SVG pages\n"
          "       verosim --pairs <tsv> [--base-dir <dir>] [--ops]\n"
          "                                      compare every pred<TAB>gt pair in <tsv>\n"
          "                                      (JSONL, one record per pair, always exits 0)\n"
          "       verosim --batch <tsv> [--base-dir <dir>] [--jobs N] [--ops]\n"
          "                                      parallel JSONL batch compare, one Toolkit per worker\n"
          "       verosim --batch-jsonl <jsonl> [--pred-field prediction] [--gt-field target]\n"
          "                                      [--id-field sample_id] [--group-field val_set]\n"
          "                                      [--dedupe-key fields] [--format kern]\n"
          "                                      [--failure-score N]\n"
          "                                      [--summary-json path] [--jobs N] [--ops]\n"
          "                                      compare in-memory kern strings from JSONL records\n"
          "       verosim --dump-tree <file>     print the parsed Verovio object tree\n"
          "       verosim --check <file>         load one file, report warnings/errors (JSONL)\n"
          "       verosim --check --files-from <list> [--base-dir <dir>]\n"
          "                                      check every file in <list> (one path per line,\n"
          "                                      '#' comments; always exits 0 — failures are data)\n"
          "       verosim --count-symbols [--per-measure] <file>\n"
          "                                      Tier-A symbol counts as JSON\n"
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
    std::vector<verosim::PairRow> pairs;
    try {
        pairs = verosim::ReadPairList(listPath, std::cerr);
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 2;
    }
    verosim::VrvBridge bridge;
    for (const verosim::PairRow &pair : pairs) {
        const std::string pred = verosim::JoinBaseDir(baseDir, pair.pred);
        const std::string gt = verosim::JoinBaseDir(baseDir, pair.gt);
        verosim::ComparePairToJson(bridge, pred, gt, options, std::cout);
        std::cout.flush(); // same durability contract as --check list mode
    }
    return 0; // failures are data (recorded per pair), like the other list modes
}

int CompareBatchMain(
    const std::string &listPath, const std::string &baseDir, int jobs,
    const verosim::CompareCliOptions &options)
{
    std::vector<verosim::PairRow> pairs;
    try {
        pairs = verosim::ReadPairList(listPath, std::cerr);
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 2;
    }
    verosim::CompareBatchToJson(pairs, baseDir, jobs, options, std::cout);
    return 0; // failures are data, as with --pairs
}

int CompareJsonlBatchMain(
    const verosim::BatchJsonlArgs &args, const verosim::CompareCliOptions &options)
{
    std::string error;
    if (!verosim::CompareJsonlBatchToJson(args, options, std::cout, error)) {
        std::cerr << "verosim: " << error << '\n';
        return 2;
    }
    return 0; // failures are data, as with --batch
}

int VisualizeMain(
    const verosim::VisualizeArgs &args, const verosim::CompareCliOptions &options)
{
    std::string error;
    if (args.output_kind == verosim::VisualizeArgs::OutputKind::kHtml) {
        if (!verosim::VisualizePairToHtml(args.pred_path, args.gt_path, args.out_path, options, error)) {
            std::cerr << "verosim: " << error << '\n';
            return 1;
        }
        return 0;
    }

    verosim::VisualReport report;
    verosim::SvgAssetBundle bundle;
    if (!verosim::BuildVisualComparison(args.pred_path, args.gt_path, options, report, error)
        || !verosim::WriteSvgAssetBundle(report, args.out_dir, bundle, error)) {
        std::cerr << "verosim: " << error << '\n';
        return 1;
    }
    return 0;
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
    std::vector<std::string> files;
    try {
        files = verosim::ReadFileList(listPath);
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 2;
    }
    verosim::VrvBridge bridge({ .log_level = vrv::LOG_WARNING, .capture_log = true });
    for (const std::string &line : files) {
        const std::string path = verosim::JoinBaseDir(baseDir, line);
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
    std::vector<std::string> files;
    try {
        files = verosim::ReadFileList(listPath);
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 2;
    }
    verosim::VrvBridge bridge;
    const verosim::CountSymbolsOptions options;
    for (const std::string &line : files) {
        const std::string path = verosim::JoinBaseDir(baseDir, line);
        verosim::CountSymbolsFile(bridge, path, options, std::cout);
        std::cout.flush(); // same durability contract as --check list mode
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
        PrintUsage(std::cout);
        return 0;
    }
    // Global comparison options modify the compare modes only; strip before dispatch.
    verosim::CompareCliOptions compareOptions;
    std::string parseError;
    if (!verosim::StripCompareOptions(args, compareOptions, parseError)) {
        if (!parseError.empty()) std::cerr << "verosim: " << parseError << '\n';
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
        if (auto batchJsonl = verosim::ParseBatchJsonlArgs(args)) {
            return CompareJsonlBatchMain(*batchJsonl, compareOptions);
        }
        if (auto visual = verosim::ParseVisualizeArgs(args)) {
            return VisualizeMain(*visual, compareOptions);
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
