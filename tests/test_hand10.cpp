// Rung 1 of the test architecture: HAND-10 symbol-count audits, exact.
// For every corpora/hand10/expected/<name>.expected.json, extract the score
// and compare total, per-category counts, and per-part/per-measure notation
// sizes — all must match the hand counts exactly (which were themselves
// cross-checked against the vendored musicdiff; see corpora/hand10/README.md).

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "verosim/extraction/extract.h"
#include "verosim/extraction/vrv_bridge.h"

using namespace verosim;
namespace fs = std::filesystem;

namespace {

// Minimal reader for the flat expected.json schema:
// {"file": "...", "total": N, "categories": {"k": v, ...},
//  "per_part": [{"per_measure": [n, ...]}, ...]}
struct Expected {
    std::string file;
    long total = -1;
    std::map<std::string, long> categories;
    std::vector<std::vector<long>> per_part_measures;
};

Expected ParseExpected(const fs::path &path)
{
    std::ifstream in(path);
    REQUIRE(in);
    std::stringstream buf;
    buf << in.rdbuf();
    const std::string text = buf.str();

    Expected expected;
    const auto stringAfter = [&](const std::string &key) -> std::string {
        const auto pos = text.find("\"" + key + "\"");
        REQUIRE(pos != std::string::npos);
        const auto q1 = text.find('"', text.find(':', pos));
        const auto q2 = text.find('"', q1 + 1);
        return text.substr(q1 + 1, q2 - q1 - 1);
    };
    const auto longAfter = [&](const std::string &key, std::size_t from = 0) -> long {
        const auto pos = text.find("\"" + key + "\"", from);
        REQUIRE(pos != std::string::npos);
        return std::stol(text.substr(text.find(':', pos) + 1));
    };

    expected.file = stringAfter("file");
    expected.total = longAfter("total");

    // categories: every "key": number pair inside the categories object
    const auto catStart = text.find("\"categories\"");
    const auto catOpen = text.find('{', catStart);
    const auto catClose = text.find('}', catOpen);
    std::string cats = text.substr(catOpen + 1, catClose - catOpen - 1);
    std::size_t pos = 0;
    while ((pos = cats.find('"', pos)) != std::string::npos) {
        const auto q2 = cats.find('"', pos + 1);
        const std::string key = cats.substr(pos + 1, q2 - pos - 1);
        const long value = std::stol(cats.substr(cats.find(':', q2) + 1));
        expected.categories[key] = value;
        pos = cats.find(',', q2);
        if (pos == std::string::npos) break;
    }

    // per_part: arrays of ints
    auto partPos = text.find("\"per_part\"");
    REQUIRE(partPos != std::string::npos);
    auto cursor = text.find('[', partPos);
    while (true) {
        const auto measuresPos = text.find("\"per_measure\"", cursor);
        if (measuresPos == std::string::npos) break;
        const auto open = text.find('[', measuresPos);
        const auto close = text.find(']', open);
        std::vector<long> sizes;
        std::stringstream nums(text.substr(open + 1, close - open - 1));
        std::string token;
        while (std::getline(nums, token, ',')) {
            if (!token.empty()) sizes.push_back(std::stol(token));
        }
        expected.per_part_measures.push_back(std::move(sizes));
        cursor = close;
    }
    return expected;
}

long CategoryValue(const SymbolCounts &c, const std::string &key)
{
    static const std::map<std::string, long SymbolCounts::*> kFields = {
        { "pitches", &SymbolCounts::pitches },
        { "accidentals", &SymbolCounts::accidentals },
        { "ties", &SymbolCounts::ties },
        { "noteheads", &SymbolCounts::noteheads },
        { "dots", &SymbolCounts::dots },
        { "beams", &SymbolCounts::beams },
        { "tuplets", &SymbolCounts::tuplets },
        { "tuplet_info", &SymbolCounts::tuplet_info },
        { "grace", &SymbolCounts::grace },
        { "grace_slash", &SymbolCounts::grace_slash },
        { "gaps", &SymbolCounts::gaps },
        { "articulations", &SymbolCounts::articulations },
        { "expressions", &SymbolCounts::expressions },
        { "style", &SymbolCounts::style },
        { "clef", &SymbolCounts::clef },
        { "keysig", &SymbolCounts::keysig },
        { "timesig", &SymbolCounts::timesig },
        { "other_extras", &SymbolCounts::other_extras },
    };
    REQUIRE(kFields.count(key));
    return c.*kFields.at(key);
}

} // namespace

TEST_CASE("HAND-10 symbol counts are exact", "[hand10]")
{
    const fs::path dir(VEROSIM_HAND10_DIR);
    const fs::path expectedDir = dir / "expected";
    REQUIRE(fs::is_directory(expectedDir));

    std::vector<fs::path> expectedFiles;
    for (const auto &entry : fs::directory_iterator(expectedDir)) {
        if (entry.path().extension() == ".json") expectedFiles.push_back(entry.path());
    }
    std::sort(expectedFiles.begin(), expectedFiles.end());
    REQUIRE(expectedFiles.size() == 10);

    VrvBridge bridge;
    for (const fs::path &expectedPath : expectedFiles) {
        const Expected expected = ParseExpected(expectedPath);
        DYNAMIC_SECTION(expected.file)
        {
            REQUIRE(bridge.LoadScoreFile((dir / expected.file).string()));
            const SourceFormat format = bridge.last_input_format() == vrv::HUMDRUM
                ? SourceFormat::kKern
                : SourceFormat::kMusicXml;
            const ExtractResult result = ExtractSymScore(bridge.GetDoc(), format);

            CHECK(result.warnings.empty());
            CHECK(result.score.notation_size() == expected.total);

            const SymbolCounts counts = CountSymbols(result.score);
            CHECK(counts.total() == expected.total);
            for (const auto &[key, value] : expected.categories) {
                INFO("category " << key);
                CHECK(CategoryValue(counts, key) == value);
            }

            REQUIRE(result.score.parts.size() == expected.per_part_measures.size());
            for (std::size_t p = 0; p < result.score.parts.size(); ++p) {
                const SymPart &part = result.score.parts[p];
                REQUIRE(part.bar_list.size() == expected.per_part_measures[p].size());
                for (std::size_t m = 0; m < part.bar_list.size(); ++m) {
                    INFO("part " << p << " measure " << m);
                    CHECK(part.bar_list[m].notation_size() == expected.per_part_measures[p][m]);
                }
            }
        }
    }
}
