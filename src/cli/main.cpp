#include <exception>
#include <iostream>
#include <string>

#include "verosim/cli/dump_tree.h"
#include "verosim/extraction/vrv_bridge.h"

namespace {

void PrintUsage(std::ostream &os)
{
    os << "usage: verosim <fileA> <fileB>        compare two scores (placeholder until M3)\n"
          "       verosim --dump-tree <file>     print the parsed Verovio object tree\n"
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

} // namespace

int main(int argc, char **argv)
{
    try {
        if (argc == 3 && std::string(argv[1]) == "--dump-tree") {
            return DumpTreeMain(argv[2]);
        }
        if (argc == 3 && argv[1][0] != '-' && argv[2][0] != '-') {
            return CompareMain(argv[1], argv[2]);
        }
    }
    catch (const std::exception &e) {
        std::cerr << "verosim: " << e.what() << '\n';
        return 1;
    }
    PrintUsage(std::cerr);
    return 2;
}
