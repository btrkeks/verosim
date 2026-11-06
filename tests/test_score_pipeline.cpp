#include <initializer_list>
#include <memory>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "engine_fixtures.h"
#include "verosim/app/score_pipeline.h"
#include "verosim/engine/alignment_trace.h"
#include "verosim/extraction/vrv_bridge.h"

using namespace verosim;
using namespace verosim::test;

namespace {

std::shared_ptr<const EvaluationProfile> TestProfile(
    std::initializer_list<EvaluationFeature> ignored = {})
{
    EvaluationFeatureDispositions dispositions;
    for (const EvaluationFeatureSpec &spec : EvaluationFeatureRegistryV1()) {
        dispositions[EvaluationFeatureIndex(spec.feature)]
            = spec.capability == EvaluationFeatureCapability::kKnownUnsupported
            ? FeatureDisposition::kUnsupported
            : FeatureDisposition::kScored;
    }
    for (const EvaluationFeature feature : ignored) {
        dispositions[EvaluationFeatureIndex(feature)] = FeatureDisposition::kIgnored;
    }
    std::string error;
    std::optional<EvaluationProfile> profile
        = MakeEvaluationProfileV1("test.pipeline", "sha256:pipeline", dispositions, error);
    REQUIRE(profile.has_value());
    REQUIRE(error.empty());
    return std::make_shared<const EvaluationProfile>(std::move(*profile));
}

SymScore OneNoteScore(const std::string &pitch = "C4")
{
    return MakeScore({ { MakeMeasure({ MakeNote(pitch, Fraction(0), { .id = "note" }) }) } });
}

} // namespace

TEST_CASE("Mozart missing boundary aligns the pickup and full bar on both staffs",
    "[pipeline][shared-alignment][split-merge]")
{
    VrvBridge bridge;
    CompareRunOptions options;
    options.surface.layout = LayoutSurface::kSystemBreaks;
    options.trace_alignment = true;
    const std::string base = std::string(VEROSIM_TEST_FIXTURE_DIR) + "/matching/mozart-k331-p13.";
    const auto pred = LoadAndExtractScoreFile(bridge, base + "prediction.krn", ExtractOptionsForRun(options));
    const auto gt = LoadAndExtractScoreFile(bridge, base + "ground_truth.krn", ExtractOptionsForRun(options));
    REQUIRE(pred.ok);
    REQUIRE(gt.ok);
    const auto comparison = CompareLoadedScores(pred.score, gt.score, options);
    CHECK(options.alignment_policy == AlignmentPolicy::kShared);
    options.alignment_policy = AlignmentPolicy::kIndependent;
    const auto legacy = CompareLoadedScores(pred.score, gt.score, options);
    CHECK(legacy.diff.cost > comparison.diff.cost);
    int groups = 0, boundaries = 0;
    for (const auto &d : comparison.alignment_trace->decisions)
        if (!d.prediction_group.empty())
        {
            ++groups;
            REQUIRE(d.prediction_group.size() == 1);
            REQUIRE(d.ground_truth_group.size() == 2);
            CHECK(d.prediction_group[0].measure->locator.measure_idx == 26);
            CHECK(d.ground_truth_group[0].measure->locator.measure_idx == 26);
            CHECK(d.ground_truth_group[1].measure->locator.measure_idx == 27);
        }
    CHECK(groups == 2);
    for (const auto &op : comparison.diff.op_list)
    {
        CHECK(op.name != OpName::kInsBar);
        CHECK(op.name != OpName::kDelBar);
        if (op.name == OpName::kBarlineIns) ++boundaries;
    }
    CHECK(boundaries == 2);
    CHECK(comparison.edit_distances.at("wrong note OMR-ED") > 0);
    CHECK(comparison.edit_distances.at("wrong accidental OMR-ED") > 0);
}

TEST_CASE("shared boundary penalties follow the barline evaluation profile",
    "[evaluation-profile][pipeline][split-merge]")
{
    auto single = MakeMeasure({MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1))});
    auto first = MakeMeasure({MakeNote("C4", Fraction(0))});
    auto second = MakeMeasure({MakeNote("E4", Fraction(0))});
    single.content_duration = Fraction(2);
    first.content_duration = second.content_duration = Fraction(1);
    const auto pred = MakeScore({{single}}), gt = MakeScore({{first, second}});
    CompareRunOptions options;
    options.alignment_policy = AlignmentPolicy::kShared;
    options.evaluation_profile = TestProfile();
    CHECK(CompareLoadedScores(pred, gt, options).diff.cost == 1);
    options.evaluation_profile = TestProfile({EvaluationFeature::kBarline});
    const auto ignored = CompareLoadedScores(pred, gt, options);
    CHECK(ignored.diff.cost == 0);
    CHECK(ignored.diff.op_list.empty());
}

TEST_CASE("span timing includes hidden trailing carriers and preserves short pickups",
    "[pipeline][split-merge]")
{
    VrvBridge bridge;
    const std::string kern = "**kern\n*M4/4\n=1\n8c\n8d\n=2\n4e\n4ryy\n=3\n4f\n*-\n";
    const auto score = LoadAndExtractKernData(bridge, kern, "span-timing", ExtractOptions{});
    REQUIRE(score.ok);
    REQUIRE(score.score.parts.size() == 1);
    const auto &bars = score.score.parts[0].bar_list;
    REQUIRE(bars.size() == 3);
    CHECK(bars[0].content_duration == Fraction(1));
    CHECK(bars[1].content_duration == Fraction(2));
    REQUIRE(bars[1].notes.size() == 1);
    CHECK(bars[2].content_duration == Fraction(1));
}

TEST_CASE("evaluation profiles force maximal extraction while legacy options remain unchanged",
    "[evaluation-profile][pipeline]")
{
    CompareRunOptions legacy{ .surface = MetricSurface{ .mode = MetricMode::kActive,
        .layout = LayoutSurface::kNone } };
    const ExtractOptions legacy_extract = ExtractOptionsForRun(legacy);
    CHECK(legacy_extract.surface.mode == MetricMode::kActive);
    CHECK(legacy_extract.surface.layout == LayoutSurface::kNone);

    legacy.evaluation_profile = TestProfile();
    const ExtractOptions profiled_extract = ExtractOptionsForRun(legacy);
    CHECK(profiled_extract.surface.mode == MetricMode::kExperimental);
    CHECK(profiled_extract.surface.layout == LayoutSurface::kSystemBreaks);
}

TEST_CASE("ignored mutations are projected symmetrically before matching and denominator",
    "[evaluation-profile][pipeline]")
{
    SymScore pred = OneNoteScore();
    SymScore gt = OneNoteScore();
    SymNote &pred_note = pred.parts[0].bar_list[0].notes[0];
    pred_note.pitches[0].tie = true;
    pred_note.articulations = { "Staccato" };
    SymExtra slur;
    slur.kind = ExtraKind::kSlur;
    slur.vrv_id = "slur";
    slur.duration = Fraction(1);
    pred.parts[0].bar_list[0].extras.push_back(slur);

    CompareRunOptions options;
    options.evaluation_profile = TestProfile({ EvaluationFeature::kTie,
        EvaluationFeature::kArticulation, EvaluationFeature::kSlur });
    const ScoreComparison comparison = CompareLoadedScores(pred, gt, options);

    CHECK(comparison.diff.cost == 0);
    CHECK(comparison.n_pred == gt.notation_size());
    CHECK(comparison.n_gt == gt.notation_size());
    CHECK(comparison.omr_ned == 0.0);
    REQUIRE(comparison.projected_scores != nullptr);
    REQUIRE(comparison.evaluation_audit.has_value());
    REQUIRE(comparison.evaluation_audit->pred_removed.has_value());
    REQUIRE(comparison.evaluation_audit->gt_removed.has_value());
    CHECK((*comparison.evaluation_audit->pred_removed)[EvaluationFeature::kTie] == 1);
    CHECK((*comparison.evaluation_audit->pred_removed)[EvaluationFeature::kArticulation] == 1);
    CHECK((*comparison.evaluation_audit->pred_removed)[EvaluationFeature::kSlur] == 1);
    CHECK(comparison.evaluation_audit->gt_removed->total() == 0);
}

TEST_CASE("retained mutations remain sensitive and projected results survive moves",
    "[evaluation-profile][pipeline]")
{
    SymScore pred = OneNoteScore("C4");
    pred.parts[0].bar_list[0].notes[0].note_head = Fraction(2);
    SymScore gt = OneNoteScore("C4");
    CompareRunOptions options;
    options.evaluation_profile = TestProfile();

    ScoreComparison comparison = CompareLoadedScores(pred, gt, options);
    REQUIRE(comparison.diff.cost > 0);
    REQUIRE(comparison.projected_scores != nullptr);
    REQUIRE_FALSE(comparison.diff.op_list.empty());
    const SymNote *owned_pred
        = &comparison.projected_scores->pred.parts[0].bar_list[0].notes[0];
    const SymNote *owned_gt
        = &comparison.projected_scores->gt.parts[0].bar_list[0].notes[0];
    bool saw_owned_note = false;
    for (const EditOp &op : comparison.diff.op_list) {
        if (op.a.note) saw_owned_note = saw_owned_note || op.a.note == owned_pred;
        if (op.b.note) saw_owned_note = saw_owned_note || op.b.note == owned_gt;
    }
    CHECK(saw_owned_note);

    ScoreComparison moved = std::move(comparison);
    REQUIRE(moved.projected_scores != nullptr);
    for (const EditOp &op : moved.diff.op_list) {
        if (op.a.note) CHECK(op.a.note == owned_pred);
        if (op.b.note) CHECK(op.b.note == owned_gt);
    }
}

TEST_CASE("no-profile comparison preserves the legacy path",
    "[evaluation-profile][pipeline]")
{
    const SymScore pred = OneNoteScore("D4");
    const SymScore gt = OneNoteScore("C4");
    const CompareResult direct = CompareScores(pred, gt);
    const ScoreComparison comparison = CompareLoadedScores(pred, gt, CompareRunOptions{});

    CHECK(comparison.projected_scores == nullptr);
    CHECK_FALSE(comparison.evaluation_audit.has_value());
    CHECK(comparison.diff.cost == direct.cost);
    CHECK(comparison.n_pred == pred.notation_size());
    CHECK(comparison.n_gt == gt.notation_size());
    CHECK(comparison.edit_distances == EditDistancesDict(direct.op_list));
}

TEST_CASE("MusicXML and kern compare representation-neutrally after projection",
    "[evaluation-profile][pipeline]")
{
    const std::string musicxml = R"xml(<?xml version="1.0"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>Music</part-name></score-part></part-list>
  <part id="P1"><measure number="1">
    <attributes><divisions>1</divisions><time><beats>4</beats><beat-type>4</beat-type></time><clef><sign>G</sign><line>2</line></clef></attributes>
    <note>
      <pitch><step>C</step><octave>4</octave></pitch><duration>2</duration><type>half</type>
      <tie type="start"/>
      <notations><tied type="start"/><slur type="start" number="1"/><articulations><staccato/></articulations></notations>
    </note>
    <note>
      <pitch><step>C</step><octave>4</octave></pitch><duration>2</duration><type>half</type>
      <tie type="stop"/>
      <notations><tied type="stop"/><slur type="stop" number="1"/></notations>
    </note>
  </measure></part>
</score-partwise>)xml";
    const std::string kern = "**kern\n*clefG2\n*M4/4\n=1\n2c\n2c\n*-\n";
    CompareRunOptions options;
    options.evaluation_profile = TestProfile({ EvaluationFeature::kTie,
        EvaluationFeature::kArticulation, EvaluationFeature::kSlur });
    const ExtractOptions extract_options = ExtractOptionsForRun(options);
    VrvBridge pred_bridge;
    VrvBridge gt_bridge;
    const LoadedScore pred = LoadAndExtractScoreData(
        pred_bridge, musicxml, ScoreDataFormat::kMusicXml, "prediction", extract_options);
    const LoadedScore gt = LoadAndExtractScoreData(
        gt_bridge, kern, ScoreDataFormat::kKern, "ground truth", extract_options);
    REQUIRE(pred.ok);
    REQUIRE(gt.ok);

    const ScoreComparison comparison = CompareLoadedScores(pred.score, gt.score, options);
    CHECK(comparison.diff.cost == 0);
    REQUIRE(comparison.evaluation_audit.has_value());
    CHECK((*comparison.evaluation_audit->pred_removed)[EvaluationFeature::kTie] == 1);
    CHECK((*comparison.evaluation_audit->pred_removed)[EvaluationFeature::kArticulation] == 1);
    CHECK((*comparison.evaluation_audit->pred_removed)[EvaluationFeature::kSlur] == 1);
    CHECK(comparison.evaluation_audit->gt_removed->total() == 0);
}
