#include <set>

#include <catch2/catch_test_macros.hpp>

#include "engine_fixtures.h"
#include "verosim/cli/args.h"
#include "verosim/engine/alignment_trace.h"
#include "verosim/engine/compare.h"

using namespace verosim;
using namespace verosim::test;

namespace
{

SymScore Indexed(SymScore score)
{
    for (auto &part : score.parts)
        for (std::size_t i = 0; i < part.bar_list.size(); ++i)
            part.bar_list[i].locator.measure_idx = static_cast<int>(i);
    return score;
}

SymMeasure Note(const std::string &pitch)
{
    auto measure = MakeMeasure({MakeNote(pitch, Fraction(0))});
    measure.content_duration = Fraction(1);
    return measure;
}

bool Paired(const AlignmentTrace &trace, const SymMeasure &a, const SymMeasure &b)
{
    for (const auto &decision : trace.decisions)
        if (decision.prediction.kind == OpSide::Kind::kMeasure &&
            decision.ground_truth.kind == OpSide::Kind::kMeasure &&
            decision.prediction.measure == &a && decision.ground_truth.measure == &b)
            return true;
    return false;
}

const void *Address(const OpSide &side)
{
    switch (side.kind)
    {
    case OpSide::Kind::kPart:
        return side.part;
    case OpSide::Kind::kMeasure:
        return side.measure;
    case OpSide::Kind::kNote:
        return side.note;
    case OpSide::Kind::kExtra:
        return side.extra;
    case OpSide::Kind::kNone:
        return nullptr;
    }
    return nullptr;
}

void Complete(const AlignmentTrace &trace, const SymScore &a, const SymScore &b)
{
    for (const bool prediction : {true, false})
    {
        std::set<const void *> expected, actual;
        for (const auto &part : (prediction ? a : b).parts)
        {
            expected.insert(&part);
            for (const auto &measure : part.bar_list)
            {
                expected.insert(&measure);
                for (const auto &note : measure.notes)
                    expected.insert(&note);
                for (const auto &extra : measure.extras)
                    expected.insert(&extra);
            }
        }
        for (const auto &decision : trace.decisions)
        {
            if (const auto *address =
                    Address(prediction ? decision.prediction : decision.ground_truth))
                REQUIRE(actual.insert(address).second);
            for (const auto &side : prediction ? decision.prediction_group : decision.ground_truth_group)
                REQUIRE(actual.insert(Address(side)).second);
        }
        CHECK(actual == expected);
    }
}

void CostAgrees(const CompareResult &result)
{
    long sum = 0;
    for (const auto &op : result.op_list)
        sum += op.cost;
    CHECK(sum == result.cost);
}

} // namespace

TEST_CASE("shared alignment preserves music across a missing or extra boundary",
          "[engine][shared-alignment][split-merge]")
{
    auto single = MakeMeasure({MakeNote("A4", Fraction(0)), MakeNote("B4", Fraction(1, 2)),
                               MakeNote("A5", Fraction(1)), MakeNote("E5", Fraction(2))},
                              {MakeKeySig("3", Fraction(1))});
    single.content_duration = Fraction(3);
    auto pickup = MakeMeasure({MakeNote("A4", Fraction(0)), MakeNote("B4", Fraction(1, 2))});
    pickup.content_duration = Fraction(1);
    auto full = MakeMeasure({MakeNote("C5", Fraction(0)), MakeNote("E5", Fraction(1))},
                            {MakeKeySig("3")});
    full.content_duration = Fraction(2);
    auto bass_single = MakeMeasure({MakeNote("R", Fraction(0)), MakeNote("A3", Fraction(1)),
                                    MakeNote("E3", Fraction(2))});
    bass_single.content_duration = Fraction(3);
    auto bass_pickup = MakeMeasure({MakeNote("R", Fraction(0))});
    bass_pickup.content_duration = Fraction(1);
    auto bass_full = MakeMeasure({MakeNote("A3", Fraction(0)), MakeNote("E3", Fraction(1))});
    bass_full.content_duration = Fraction(2);
    const auto merged = Indexed(MakeScore({{single}, {bass_single}}));
    const auto divided = Indexed(MakeScore({{pickup, full}, {bass_pickup, bass_full}}));
    for (const auto policy : {NotePositionPolicy::kVisualEventOrder, NotePositionPolicy::kMusicalOnset})
        for (const bool reverse : {false, true})
        {
            const auto &a = reverse ? divided : merged;
            const auto &b = reverse ? merged : divided;
            AlignmentTrace trace;
            const auto result = CompareScores(a, b, {.note_position_policy = policy,
                .alignment_policy = AlignmentPolicy::kShared, .alignment_trace = &trace});
            CHECK(result.cost == 6); // two boundaries plus a real pitch error
            int boundaries = 0, note_errors = 0, groups = 0;
            for (const auto &op : result.op_list)
            {
                CHECK(op.name != OpName::kInsBar);
                CHECK(op.name != OpName::kDelBar);
                if (op.name == (reverse ? OpName::kBarlineDel : OpName::kBarlineIns))
                {
                    ++boundaries;
                    CHECK(EditOpCategory(op) == "wrong barline OMR-ED");
                }
                if (op.name == OpName::kNoteDel || op.name == OpName::kNoteIns) ++note_errors;
            }
            CHECK(boundaries == 2);
            CHECK(note_errors == 2);
            for (const auto &d : trace.decisions)
                if (!d.prediction_group.empty())
                {
                    ++groups;
                    CHECK(d.prediction_group.size() == (reverse ? 2 : 1));
                    CHECK(d.ground_truth_group.size() == (reverse ? 1 : 2));
                }
            CHECK(groups == 2);
            Complete(trace, a, b);
            CostAgrees(result);
            const auto plain = CompareScores(a, b, {.note_position_policy = policy,
                .alignment_policy = AlignmentPolicy::kShared});
            CHECK(plain.cost == result.cost);
            REQUIRE(plain.op_list.size() == result.op_list.size());
            for (std::size_t i = 0; i < plain.op_list.size(); ++i)
            {
                CHECK(plain.op_list[i].name == result.op_list[i].name);
                CHECK(Address(plain.op_list[i].a) == Address(result.op_list[i].a));
                CHECK(Address(plain.op_list[i].b) == Address(result.op_list[i].b));
            }
        }
}

TEST_CASE("split merge alignment orders voices in shared span coordinates",
          "[engine][shared-alignment][split-merge]")
{
    auto single = MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1)),
                               MakeNote("G4", Fraction(0)), MakeNote("A4", Fraction(1))});
    single.content_duration = Fraction(2, 3);
    single.notes[1].note_offset = single.notes[3].note_offset = Fraction(1, 3);
    auto first = MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("G4", Fraction(0))});
    auto second = MakeMeasure({MakeNote("E4", Fraction(0)), MakeNote("A4", Fraction(0))});
    first.content_duration = second.content_duration = Fraction(1, 3);
    const auto a = Indexed(MakeScore({{single}}));
    const auto b = Indexed(MakeScore({{first, second}}));
    AlignmentTrace trace;
    const auto result = CompareScores(a, b, {.alignment_policy = AlignmentPolicy::kShared,
        .alignment_trace = &trace});
    CHECK(result.cost == 1);
    REQUIRE(result.op_list.size() == 1);
    CHECK(result.op_list[0].name == OpName::kBarlineIns);
    CHECK(result.op_list[0].b.measure == &b.parts[0].bar_list[0]);
    Complete(trace, a, b);
    CostAgrees(result);
}

TEST_CASE("split merge candidates require duration evidence and adjacent source bars",
          "[engine][shared-alignment][split-merge]")
{
    auto single = MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1))});
    single.content_duration = Fraction(2);
    auto first = Note("C4"), second = Note("E4");
    first.content_duration = second.content_duration = Fraction(1);
    const auto a = Indexed(MakeScore({{single}}));
    for (const int invalid : {0, 1, 2})
    {
        auto b = Indexed(MakeScore({{first, second}}));
        if (invalid == 0) b.parts[0].bar_list[0].content_duration = Fraction(0);
        if (invalid == 1) b.parts[0].bar_list[1].content_duration = Fraction(2);
        if (invalid == 2) b.parts[0].bar_list[1].locator.measure_idx = 2;
        AlignmentTrace trace;
        const auto result = CompareScores(a, b, {.alignment_policy = AlignmentPolicy::kShared,
            .alignment_trace = &trace});
        for (const auto &d : trace.decisions) CHECK(d.prediction_group.empty());
        for (const auto &op : result.op_list)
        {
            CHECK(op.name != OpName::kBarlineIns);
            CHECK(op.name != OpName::kBarlineDel);
        }
        Complete(trace, a, b);
    }
}

TEST_CASE("shared alignment keeps repeated accompaniment with the melody after "
          "a missing bar",
          "[engine][shared-alignment]")
{
    const auto a = Indexed(MakeScore({{Note("C6"), Note("D6"), Note("F6"), Note("G6")},
                                      {Note("C3"), Note("C3"), Note("C3"), Note("C3")}}));
    const auto b =
        Indexed(MakeScore({{Note("C6"), Note("D6"), Note("E6"), Note("F6"), Note("G6")},
                           {Note("C3"), Note("C3"), Note("C3"), Note("C3"), Note("C3")}}));
    AlignmentTrace trace;
    const auto result = CompareScores(
        a, b, {.alignment_policy = AlignmentPolicy::kShared, .alignment_trace = &trace});
    CHECK(result.cost ==
          b.parts[0].bar_list[2].notation_size() + b.parts[1].bar_list[2].notation_size());
    for (int p = 0; p < 2; ++p)
    {
        CHECK(Paired(trace, a.parts[p].bar_list[2], b.parts[p].bar_list[3]));
        CHECK(Paired(trace, a.parts[p].bar_list[3], b.parts[p].bar_list[4]));
    }
    Complete(trace, a, b);
    CostAgrees(result);
    const auto plain = CompareScores(a, b, {.alignment_policy = AlignmentPolicy::kShared});
    CHECK(plain.cost == result.cost);
    REQUIRE(plain.op_list.size() == result.op_list.size());
    for (std::size_t i = 0; i < plain.op_list.size(); ++i)
    {
        CHECK(plain.op_list[i].name == result.op_list[i].name);
        CHECK(Address(plain.op_list[i].a) == Address(result.op_list[i].a));
        CHECK(Address(plain.op_list[i].b) == Address(result.op_list[i].b));
    }
}

TEST_CASE("shared alignment joins staves by source measure index after empty "
          "extraction",
          "[engine][shared-alignment]")
{
    auto a = Indexed(MakeScore({{Note("C6"), Note("E6")}, {Note("C3"), Note("D3"), Note("E3")}}));
    a.parts[0].bar_list[1].locator.measure_idx = 2;
    const auto b = Indexed(
        MakeScore({{Note("C6"), Note("R"), Note("E6")}, {Note("C3"), Note("D3"), Note("E3")}}));
    AlignmentTrace trace;
    const auto result = CompareScores(
        a, b, {.alignment_policy = AlignmentPolicy::kShared, .alignment_trace = &trace});
    CHECK(Paired(trace, a.parts[0].bar_list[1], b.parts[0].bar_list[2]));
    CHECK(Paired(trace, a.parts[1].bar_list[1], b.parts[1].bar_list[1]));
    CHECK(result.cost == b.parts[0].bar_list[1].notation_size());
    Complete(trace, a, b);
    CostAgrees(result);
}

TEST_CASE("shared alignment handles empty scores and preserves local notation edits",
          "[engine][shared-alignment]")
{
    const auto a = Indexed(MakeScore({{Note("C6")}, {Note("C3")}}));
    const auto b = Indexed(MakeScore(
        {{MakeMeasure({MakeNote("C6", Fraction(0), {.accid = "sharp"})})}, {Note("C3")}}));
    for (const auto &other : {a, b, SymScore{}})
    {
        AlignmentTrace trace;
        const auto result = CompareScores(
            a, other, {.alignment_policy = AlignmentPolicy::kShared, .alignment_trace = &trace});
        Complete(trace, a, other);
        CostAgrees(result);
    }
    CHECK(CompareScores(a, a, {.alignment_policy = AlignmentPolicy::kShared}).cost == 0);
    CHECK(CompareScores(a, b, {.alignment_policy = AlignmentPolicy::kShared}).cost == 1);
    CHECK(CompareScores(SymScore{}, SymScore{}, {.alignment_policy = AlignmentPolicy::kShared})
              .cost == 0);
}

TEST_CASE("shared alignment can omit a leading or middle staff without "
          "shifting the remaining instruments",
          "[engine][shared-alignment]")
{
    const std::vector<SymMeasure> upper{Note("C6"), Note("E6"), Note("G6")};
    const std::vector<SymMeasure> bass{Note("C3"), Note("D3"), Note("E3")};
    const std::vector<SymMeasure> cello{Note("G2"), Note("A2"), Note("B2")};
    const auto a = Indexed(MakeScore({upper, bass}));
    for (const bool leading : {true, false})
    {
        const auto b =
            Indexed(leading ? MakeScore({cello, upper, bass}) : MakeScore({upper, cello, bass}));
        AlignmentTrace trace;
        const auto result = CompareScores(
            a, b, {.alignment_policy = AlignmentPolicy::kShared, .alignment_trace = &trace});
        for (int i = 0; i < 3; ++i)
        {
            CHECK(Paired(trace, a.parts[0].bar_list[i], b.parts[leading ? 1 : 0].bar_list[i]));
            CHECK(Paired(trace, a.parts[1].bar_list[i], b.parts[2].bar_list[i]));
        }
        CHECK(result.cost == b.parts[leading ? 0 : 1].notation_size());
        Complete(trace, a, b);
        CostAgrees(result);
        AlignmentTrace reverse;
        const auto inverse = CompareScores(
            b, a, {.alignment_policy = AlignmentPolicy::kShared, .alignment_trace = &reverse});
        CHECK(inverse.cost == result.cost);
        Complete(reverse, b, a);
        CostAgrees(inverse);
    }
}

TEST_CASE("alignment policy is explicit in CLI comparisons", "[cli][shared-alignment]")
{
    CHECK(ParseAlignmentPolicy("shared") == AlignmentPolicy::kShared);
    CHECK(ParseAlignmentPolicy("independent") == AlignmentPolicy::kIndependent);
    CHECK_FALSE(ParseAlignmentPolicy("joint").has_value());
    CHECK(AlignmentPolicyName(AlignmentPolicy::kShared) == "shared");
    std::string error;
    const auto parsed =
        ParseCommand({"prediction.krn", "reference.krn", "--alignment", "shared"}, error);
    REQUIRE(parsed.has_value());
    const auto *compare = std::get_if<CompareCommand>(&*parsed);
    REQUIRE(compare != nullptr);
    CHECK(compare->options.alignment_policy == AlignmentPolicy::kShared);
    CHECK(RunOptionsForCli(compare->options).alignment_policy == AlignmentPolicy::kShared);
    CHECK_FALSE(ParseCommand({"a.krn", "b.krn", "--alignment", "typo"}, error).has_value());
    CHECK_FALSE(ParseCommand({"a.krn", "b.krn", "--alignment"}, error).has_value());
}
