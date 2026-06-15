// Rung 3 CI slice: extraction + comparison identity over the committed
// HAND-10 corpus — OMR-NED(x, x) is definitionally 0. The full identity gate
// (DEV-200 + PERF-10K sample) runs via harness identity_audit.

#include <algorithm>
#include <filesystem>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "verosim/engine/compare.h"
#include "verosim/extraction/extract.h"
#include "verosim/extraction/vrv_bridge.h"

using namespace verosim;
namespace fs = std::filesystem;

TEST_CASE("HAND-10 self-comparison is exactly zero", "[engine][identity]")
{
    const fs::path dir(VEROSIM_HAND10_DIR);
    std::vector<fs::path> files;
    for (const auto &entry : fs::directory_iterator(dir)) {
        const auto ext = entry.path().extension();
        if (ext == ".krn" || ext == ".xml") files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 10);

    VrvBridge bridge;
    for (const fs::path &file : files) {
        DYNAMIC_SECTION(file.filename().string())
        {
            REQUIRE(bridge.LoadScoreFile(file.string()));
            const SourceFormat format = bridge.last_input_format() == vrv::HUMDRUM
                ? SourceFormat::kKern
                : SourceFormat::kMusicXml;
            const ExtractResult result = ExtractSymScore(bridge.GetDoc(), format);
            const CompareResult diff = CompareScores(result.score, result.score);
            CHECK(diff.cost == 0);
            CHECK(diff.op_list.empty());
            CHECK(OmrNed(diff.cost, result.score.notation_size(), result.score.notation_size())
                == 0.0);
        }
    }
}
