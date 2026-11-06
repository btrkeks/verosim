#include <map>
#include <set>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <jsonxx.h>

#include "engine_fixtures.h"
#include "verosim/cli/alignment_trace_json.h"
#include "verosim/cli/args.h"
#include "verosim/engine/alignment_trace.h"
#include "verosim/engine/compare.h"

using namespace verosim;
using namespace verosim::test;

namespace {

void CheckComplete(const AlignmentTrace &trace, const SymScore &pred, const SymScore &gt)
{
    std::map<const void *, int> seen_pred, seen_gt;
    const auto address = [](const OpSide &side) -> const void * {
        switch (side.kind) {
            case OpSide::Kind::kPart: return side.part;
            case OpSide::Kind::kMeasure: return side.measure;
            case OpSide::Kind::kNote: return side.note;
            case OpSide::Kind::kExtra: return side.extra;
            case OpSide::Kind::kNone: return nullptr;
        }
        return nullptr;
    };
    for (const auto &decision : trace.decisions) {
        if (const void *key = address(decision.prediction)) ++seen_pred[key];
        if (const void *key = address(decision.ground_truth)) ++seen_gt[key];
    }
    const auto check = [](const SymScore &score, const std::map<const void *, int> &seen) {
        std::size_t count = 0;
        for (const auto &part : score.parts) {
            CHECK(seen.at(&part) == 1); ++count;
            for (const auto &measure : part.bar_list) {
                CHECK(seen.at(&measure) == 1); ++count;
                for (const auto &note : measure.notes) { CHECK(seen.at(&note) == 1); ++count; }
                for (const auto &extra : measure.extras) { CHECK(seen.at(&extra) == 1); ++count; }
            }
        }
        CHECK(seen.size() == count);
    };
    check(pred, seen_pred);
    check(gt, seen_gt);
}

} // namespace

TEST_CASE("split merge traces serialize original bar groups and note identities",
    "[engine][trace][split-merge]")
{
    auto single = MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1))});
    auto first = MakeMeasure({MakeNote("C4", Fraction(0))});
    auto second = MakeMeasure({MakeNote("E4", Fraction(0))});
    single.content_duration = Fraction(2);
    first.content_duration = second.content_duration = Fraction(1);
    const auto a = MakeScore({{single}}), b = MakeScore({{first, second}});
    AlignmentTrace trace;
    const auto result = CompareScores(a, b, {.alignment_policy = AlignmentPolicy::kShared,
        .alignment_trace = &trace});
    REQUIRE(result.cost == 1);
    std::ostringstream json;
    json << '{'; WriteAlignmentTraceJson(a, b, trace, json); json << '}';
    jsonxx::Object parsed;
    REQUIRE(parsed.parse(json.str()));
    int groups = 0;
    for (const auto *value : parsed.get<jsonxx::Array>("decisions").values())
    {
        const auto &decision = value->get<jsonxx::Object>();
        if (!decision.has<jsonxx::Array>("prediction")) continue;
        ++groups;
        CHECK(decision.get<jsonxx::String>("level") == "measure");
        CHECK(decision.get<jsonxx::Array>("prediction").size() == 1);
        CHECK(decision.get<jsonxx::Array>("ground_truth").size() == 2);
    }
    CHECK(groups == 1);
}

TEST_CASE("alignment traces cover unchanged anchors and unmatched subtrees", "[engine][trace]")
{
    const SymScore a = MakeScore({
        { MakeMeasure({ MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1)) }, {MakeClef("G2")}),
          MakeMeasure({ MakeNote("D4", Fraction(0)) }) },
        { MakeMeasure({ MakeNote("C3", Fraction(0)) }) } });
    const SymScore b = MakeScore({
        { MakeMeasure({ MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1)) }, {MakeClef("G2")}),
          MakeMeasure({ MakeNote("F4", Fraction(0)) }) } });
    for (const auto policy : {NotePositionPolicy::kVisualEventOrder, NotePositionPolicy::kMusicalOnset}) {
        AlignmentTrace trace;
        const auto plain = CompareScores(a, b, {.note_position_policy = policy});
        const auto traced = CompareScores(a, b, {.note_position_policy = policy, .alignment_trace = &trace});
        CHECK(plain.cost == traced.cost);
        REQUIRE(plain.op_list.size() == traced.op_list.size());
        for (std::size_t i = 0; i < plain.op_list.size(); ++i) {
            CHECK(plain.op_list[i].name == traced.op_list[i].name);
            CHECK(plain.op_list[i].cost == traced.op_list[i].cost);
        }
        CheckComplete(trace, a, b);
        std::ostringstream json;
        json << '{'; WriteAlignmentTraceJson(a, b, trace, json); json << '}';
        jsonxx::Object parsed;
        REQUIRE(parsed.parse(json.str()));
        CHECK(parsed.has<jsonxx::Array>("decisions"));
        CHECK(parsed.get<jsonxx::Object>("symbols").get<jsonxx::Array>("prediction").size() == 10);
    }
}

TEST_CASE("bar traces expose equal cost delete and pair alternatives", "[engine][trace]")
{
    const auto a = MakeScore({{MakeMeasure({MakeNote("C4", Fraction(0))})}});
    const auto b = MakeScore({{MakeMeasure({MakeNote("D4", Fraction(0))})}});
    AlignmentTrace trace;
    CompareScores(a, b, {.alignment_policy = AlignmentPolicy::kIndependent, .alignment_trace = &trace});
    bool saw_tie = false;
    for (const auto &decision : trace.decisions) {
        if (decision.prediction.kind != OpSide::Kind::kMeasure || decision.alternatives.empty()) continue;
        REQUIRE(decision.alternatives.size() == 3);
        CHECK(decision.alternatives[0].action == "delete");
        CHECK(decision.alternatives[0].total_cost == decision.alternatives[2].total_cost);
        saw_tie = true;
    }
    CHECK(saw_tie);
    CheckComplete(trace, a, b);
}

TEST_CASE("note traces explain rejected pitches and retain zero cost matches", "[engine][trace]")
{
    const auto a = MakeScore({{MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1))})}});
    const auto b = MakeScore({{MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("F4", Fraction(1))})}});
    for (const auto policy : {NotePositionPolicy::kVisualEventOrder, NotePositionPolicy::kMusicalOnset}) {
        AlignmentTrace trace;
        CompareScores(a, b, {.note_position_policy = policy, .alignment_trace = &trace});
        bool rejected_pitch = false, retained_match = false;
        for (const auto &decision : trace.decisions) {
            if (decision.prediction.note == &a.parts[0].bar_list[0].notes[0]
                && decision.ground_truth.note == &b.parts[0].bar_list[0].notes[0]) retained_match = true;
            for (const auto &alternative : decision.alternatives) {
                if (alternative.rejection == "pitch_step_or_octave") rejected_pitch = true;
            }
        }
        CHECK(rejected_pitch);
        CHECK(retained_match);
        CheckComplete(trace, a, b);
    }
}

TEST_CASE("trace CLI validates scope and selected source measures", "[cli][trace]")
{
    std::string error;
    const auto parsed = ParseCommand({"a.krn", "b.krn", "--trace-json", "trace.json", "--trace-bars", "0,14,15"}, error);
    REQUIRE(parsed.has_value());
    const auto &options = std::get<CompareCommand>(*parsed).options;
    CHECK(options.trace_json_path == "trace.json");
    CHECK(options.trace_measures == std::set<int>{0,14,15});
    CHECK_FALSE(ParseCommand({"a", "b", "--trace-bars", "1"}, error));
    CHECK_FALSE(ParseCommand({"a", "b", "--trace-json", "x", "--trace-bars", "1,"}, error));
    CHECK_FALSE(ParseCommand({"a", "b", "--trace-json", "x", "--trace-bars", "-1"}, error));
    CHECK_FALSE(ParseCommand({"--batch", "pairs.tsv", "--trace-json", "x"}, error));
    CHECK_FALSE(ParseCommand({"--visualize", "a", "b", "--out", "x", "--trace-json", "y"}, error));
}
