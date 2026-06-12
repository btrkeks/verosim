#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "verosim/cli/check.h"
#include "verosim/cli/dump_tree.h"
#include "verosim/extraction/vrv_bridge.h"

namespace {

void PrintUsage(std::ostream &os)
{
    os << "usage: verosim <fileA> <fileB>        compare two scores (placeholder until M3)\n"
          "       verosim --dump-tree <file>     print the parsed Verovio object tree\n"
          "       verosim --check <file>         load one file, report warnings/errors (JSONL)\n"
          "       verosim --check --files-from <list> [--base-dir <dir>]\n"
          "                                      check every file in <list> (one path per line,\n"
          "                                      '#' comments; always exits 0 — failures are data)\n"
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

int CompareMain(const std::string &pathA, const std::string &pathB)
{
    verosim::VrvBridge bridgeA;
    verosim::VrvBridge bridgeB;
    const bool loadedA = bridgeA.LoadScoreFile(pathA);
    const bool loadedB = bridgeB.LoadScoreFile(pathB);
    if (!loadedA) std::cerr << "verosim: failed to load " << pathA << '\n';
    if (!loadedB) std::cerr << "verosim: failed to load " << pathB << '\n';
    std::cout << "{\"loaded\":[" << (loadedA ? "true" : "false") << ','
              << (loadedB ? "true" : "false")
              << "],\"note\":\"comparison engine lands in M2/M3\"}\n";
    return (loadedA && loadedB) ? 0 : 1;
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

} // namespace

int main(int argc, char **argv)
{
    const std::vector<std::string> args(argv + 1, argv + argc);
    try {
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
        if (args.size() == 2 && args[0][0] != '-' && args[1][0] != '-') {
            return CompareMain(args[0], args[1]);
        }
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 1;
    }
    PrintUsage(std::cerr);
    return 2;
}
