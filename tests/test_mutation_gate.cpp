// Rung 2 of the test architecture, and M3 exit gate (b): every Tier-A
// mutation case (corpora/mutations/manifest.json, oracle-cross-checked in M1)
// must produce exactly the analytically expected cost — and the expected op
// multiset where the manifest pins one. Convention from mutcheck.py:
// pred = mutated, gt = base.

#include <fstream>
#include <map>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <jsonxx.h>

#include "verosim/engine/compare.h"
#include "verosim/extraction/extract.h"
#include "verosim/extraction/vrv_bridge.h"

using namespace verosim;

namespace {

SymScore ExtractFile(VrvBridge &bridge, const std::string &path)
{
    REQUIRE(bridge.LoadScoreFile(path));
    const SourceFormat format
        = bridge.last_input_format() == vrv::HUMDRUM ? SourceFormat::kKern : SourceFormat::kMusicXml;
    ExtractResult result = ExtractSymScore(bridge.GetDoc(), format);
    CHECK(result.warnings.empty());
    return std::move(result.score);
}

} // namespace

TEST_CASE("Tier-A mutation cases produce the analytically expected cost", "[mutations]")
{
    const std::string dir(VEROSIM_MUTATIONS_DIR);
    std::ifstream in(dir + "/manifest.json");
    REQUIRE(in);
    std::stringstream buf;
    buf << in.rdbuf();

    jsonxx::Object manifest;
    REQUIRE(manifest.parse(buf.str()));
    REQUIRE(manifest.has<jsonxx::Array>("cases"));
    const jsonxx::Array &cases = manifest.get<jsonxx::Array>("cases");

    VrvBridge bridge;
    int tier_a_cases = 0;
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const jsonxx::Object &c = cases.get<jsonxx::Object>(static_cast<unsigned>(i));
        if (c.get<jsonxx::String>("tier") != "tierA") continue; // tierAB activates at M4
        // Rhythm-broken kern parses differently under humlib than under
        // converter21 (straddle-space repair vs syntax-fix costing); those
        // cases pin oracle behavior only — D6, see the manifest field text.
        if (c.has<jsonxx::String>("verovio_divergence")) continue;
        ++tier_a_cases;

        DYNAMIC_SECTION(c.get<jsonxx::String>("id"))
        {
            // SymScore is self-contained, so the bridge can be reused between
            // the two loads.
            const SymScore pred = ExtractFile(bridge, dir + "/" + c.get<jsonxx::String>("mutated"));
            const SymScore gt = ExtractFile(bridge, dir + "/" + c.get<jsonxx::String>("base"));
            const CompareResult result = CompareScores(pred, gt);

            CHECK(result.cost == static_cast<long>(c.get<jsonxx::Number>("expected_cost")));

            if (c.has<jsonxx::Object>("expected_ops")) {
                const jsonxx::Object &expected = c.get<jsonxx::Object>("expected_ops");
                std::map<std::string, long> actual;
                for (const EditOp &op : result.op_list) {
                    actual[std::string(OpNameStr(op.name))] += 1;
                }
                std::map<std::string, long> want;
                for (const auto &[name, value] : expected.kv_map()) {
                    want[name] = static_cast<long>(value->get<jsonxx::Number>());
                }
                CHECK(actual == want);
            }
        }
    }
    CHECK(tier_a_cases >= 40); // 44 at M3; guards against silent filtering
}
