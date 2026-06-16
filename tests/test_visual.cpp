#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine_fixtures.h"
#include "verosim/engine/edit_op.h"
#include "verosim/visual/svg_annotator.h"
#include "verosim/visual/svg_bundle.h"
#include "verosim/visual/svg_overlap.h"
#include "verosim/visual/visual_plan.h"
#include "verosim/visual/visualize.h"

using namespace verosim;
using namespace verosim::test;

namespace {

std::string ReadFile(const std::filesystem::path &path)
{
    std::ifstream in(path);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

TEST_CASE("VisualPlanBuilder maps paired note edits to changed marks on both sides", "[visual]")
{
    const SymNote pred = MakeNote("C4", Fraction(0), { .id = "pred-note" });
    const SymNote gt = MakeNote("C4", Fraction(0), { .accid = "sharp", .id = "gt-note" });
    const EditOp op{ .name = OpName::kAccidentIns,
        .a = OpSide::Note(&pred),
        .b = OpSide::Note(&gt),
        .cost = 1,
        .ids_kind = EditOp::IdsKind::kPitchPair,
        .ids0 = 0,
        .ids1 = 0 };

    const VisualPlan plan = BuildVisualPlan({ op });
    REQUIRE(plan.marks.size() == 2);
    CHECK(plan.marks[0].side == VisualSide::kPred);
    CHECK(plan.marks[0].role == VisualRole::kChanged);
    CHECK(plan.marks[0].target_id == "pred-note");
    CHECK(plan.marks[0].category == "wrong accidental OMR-ED");
    CHECK(plan.marks[1].side == VisualSide::kGt);
    CHECK(plan.marks[1].role == VisualRole::kChanged);
    CHECK(plan.marks[1].target_id == "gt-note");
}

TEST_CASE("VisualPlanBuilder maps single-sided note and part ops", "[visual]")
{
    const SymNote inserted = MakeNote("D4", Fraction(0), { .id = "inserted-note" });
    const EditOp note_ins{ .name = OpName::kNoteIns,
        .a = OpSide::None(),
        .b = OpSide::Note(&inserted),
        .cost = 2 };

    SymMeasure m1 = MakeMeasure({ MakeNote("C4", Fraction(0)) });
    m1.vrv_id = "measure-one";
    SymMeasure m2 = MakeMeasure({ MakeNote("D4", Fraction(0)) });
    m2.vrv_id = "measure-two";
    SymPart deleted_part;
    deleted_part.staff_n = "1";
    deleted_part.bar_list = { m1, m2 };
    const EditOp part_del{ .name = OpName::kDelPart,
        .a = OpSide::Part(&deleted_part),
        .b = OpSide::None(),
        .cost = 4 };

    const VisualPlan plan = BuildVisualPlan({ note_ins, part_del });
    REQUIRE(plan.marks.size() == 3);
    CHECK(plan.marks[0].side == VisualSide::kGt);
    CHECK(plan.marks[0].role == VisualRole::kInserted);
    CHECK(plan.marks[0].target_id == "inserted-note");
    CHECK(plan.marks[1].side == VisualSide::kPred);
    CHECK(plan.marks[1].role == VisualRole::kDeleted);
    CHECK(plan.marks[1].target_kind == VisualTargetKind::kMeasure);
    CHECK(plan.marks[1].target_id == "measure-one");
    CHECK(plan.marks[2].target_id == "measure-two");
}

TEST_CASE("SvgAnnotator marks exact IDs and spanning class IDs", "[visual]")
{
    const VisualMark note_mark{ .side = VisualSide::kGt,
        .role = VisualRole::kInserted,
        .target_kind = VisualTargetKind::kNote,
        .target_id = "note-L6F1",
        .fallback_id = "note-L6F1",
        .op_name = "noteins",
        .category = "wrong note OMR-ED",
        .cost = 2,
        .label = "inserted noteins | wrong note OMR-ED | cost 2" };
    const VisualMark span_mark{ .side = VisualSide::kGt,
        .role = VisualRole::kChanged,
        .target_kind = VisualTargetKind::kExtra,
        .target_id = "slur-L1",
        .fallback_id = "slur-L1",
        .op_name = "slursymboledit",
        .category = "wrong slur OMR-ED",
        .cost = 2,
        .label = "changed slursymboledit | wrong slur OMR-ED | cost 2" };

    const std::string svg
        = "<svg><g id=\"note-L6F1\" class=\"note\"><path/></g><g class=\"slur id-slur-L1 spanning\"/></svg>";
    const SvgAnnotationResult result = AnnotateSvg(svg, { note_mark, span_mark });
    REQUIRE(result.parse_ok);
    REQUIRE(result.resolved.size() == 2);
    CHECK(result.resolved[0]);
    CHECK(result.resolved[1]);
    CHECK(result.svg.find("verosim-role-inserted") != std::string::npos);
    CHECK(result.svg.find("data-verosim-op=\"noteins\"") != std::string::npos);
    CHECK(result.svg.find("verosim-role-changed") != std::string::npos);
    CHECK(result.svg.find("data-verosim-category=\"wrong slur OMR-ED\"") != std::string::npos);
}

TEST_CASE("SvgAnnotator reports unresolved IDs without failing", "[visual]")
{
    const VisualMark missing{ .side = VisualSide::kPred,
        .role = VisualRole::kDeleted,
        .target_kind = VisualTargetKind::kNote,
        .target_id = "missing",
        .fallback_id = "missing",
        .op_name = "notedel",
        .category = "wrong note OMR-ED",
        .cost = 2,
        .label = "deleted notedel | wrong note OMR-ED | cost 2" };
    const SvgAnnotationResult result = AnnotateSvg("<svg><g id=\"present\"/></svg>", { missing });
    REQUIRE(result.parse_ok);
    REQUIRE(result.resolved.size() == 1);
    CHECK_FALSE(result.resolved[0]);
    CHECK(result.svg.find("verosim-mark") == std::string::npos);
}

TEST_CASE("SvgOverlap detects overlaps from normalized boxes", "[visual]")
{
    const std::vector<SvgBBox> boxes = {
        SvgBBox{ .object_id = "note-a", .kind = "note", .x = 0.0, .y = 0.0, .width = 10.0, .height = 10.0 },
        SvgBBox{ .object_id = "note-b", .kind = "note", .x = 5.0, .y = 5.0, .width = 10.0, .height = 10.0 },
        SvgBBox{ .object_id = "clef-c", .kind = "clef", .x = 0.0, .y = 0.0, .width = 10.0, .height = 10.0 },
    };

    SvgOverlapSummary summary = DetectBBoxOverlaps(boxes, 4);
    CHECK(summary.page_no == 4);
    CHECK(summary.candidate_count == 2);
    REQUIRE(summary.overlap_count == 1);
    REQUIRE(summary.worst_pairs.size() == 1);
    CHECK(summary.worst_pairs[0].first_id == "note-a");
    CHECK(summary.worst_pairs[0].second_id == "note-b");
    CHECK(summary.worst_pairs[0].ratio == 0.25);
}

TEST_CASE("SvgOverlap detects note/rest bbox collisions and ignores score furniture", "[visual]")
{
    const std::string svg = R"SVG(
<svg>
  <g id="bbox-staff-L1F1" class="bounding-box"><rect x="0" y="0" width="200" height="20"/></g>
  <g id="bbox-clef-L1F1" class="bounding-box"><rect x="0" y="0" width="20" height="20"/></g>
  <g id="bbox-note-a" class="bounding-box"><rect x="0" y="0" width="10" height="10"/></g>
  <g id="cbbox-note-a" class="content-bounding-box"><rect x="0" y="0" width="20" height="20"/></g>
  <g id="bbox-note-b" class="bounding-box"><rect x="5" y="5" width="10" height="10"/></g>
  <g id="bbox-rest-c" class="bounding-box"><rect x="100" y="100" width="10" height="10"/></g>
</svg>
)SVG";

    SvgOverlapResult result = DetectSvgBBoxOverlaps(svg, 3);
    REQUIRE(result.parse_ok);
    CHECK(result.summary.page_no == 3);
    CHECK(result.summary.candidate_count == 3);
    REQUIRE(result.summary.overlap_count == 1);
    REQUIRE(result.summary.worst_pairs.size() == 1);
    CHECK(result.summary.worst_pairs[0].first_id == "note-a");
    CHECK(result.summary.worst_pairs[0].second_id == "note-b");
    CHECK(result.summary.worst_pairs[0].ratio == 0.25);

    const std::string warning = FormatSvgOverlapWarning(result.summary);
    CHECK(warning.find("page 3") != std::string::npos);
    CHECK(warning.find("note-a vs note-b") != std::string::npos);
}

TEST_CASE("SvgOverlap prefers notehead boxes over note content boxes", "[visual]")
{
    const std::string svg = R"SVG(
<svg>
  <g id="note-a" class="note">
    <g id="cbbox-note-a" class="note content-bounding-box"><rect x="0" y="0" width="10" height="120"/></g>
    <g id="bbox-note-a" class="note bounding-box"><rect x="0" y="110" width="10" height="10"/></g>
  </g>
  <g id="note-b" class="note">
    <g id="cbbox-note-b" class="note content-bounding-box"><rect x="0" y="0" width="10" height="10"/></g>
    <g id="bbox-note-b" class="note bounding-box"><rect x="0" y="0" width="10" height="10"/></g>
  </g>
</svg>
)SVG";

    SvgOverlapResult result = DetectSvgBBoxOverlaps(svg, 1);
    REQUIRE(result.parse_ok);
    CHECK(result.summary.candidate_count == 2);
    CHECK(result.summary.overlap_count == 0);
}

TEST_CASE("SvgOverlap ignores expected same-event notehead sharing", "[visual]")
{
    const std::string svg = R"SVG(
<svg>
  <g id="chord-L10F1" class="chord">
    <g id="note-L10F1S1" class="note">
      <g id="bbox-note-L10F1S1" class="note bounding-box"><rect x="0" y="0" width="10" height="10"/></g>
    </g>
    <g id="note-L10F1S2" class="note">
      <g id="bbox-note-L10F1S2" class="note bounding-box"><rect x="0" y="0" width="10" height="10"/></g>
    </g>
  </g>
  <g id="bbox-note-L20F1" class="note bounding-box"><rect x="20" y="0" width="10" height="10"/></g>
  <g id="bbox-note-L20F2" class="note bounding-box"><rect x="20" y="0" width="10" height="10"/></g>
  <g id="bbox-note-L30F1" class="note bounding-box"><rect x="40" y="0" width="10" height="10"/></g>
  <g id="bbox-note-L31F2" class="note bounding-box"><rect x="44" y="0" width="10" height="10"/></g>
</svg>
)SVG";

    SvgOverlapResult result = DetectSvgBBoxOverlaps(svg, 2);
    REQUIRE(result.parse_ok);
    CHECK(result.summary.candidate_count == 6);
    CHECK(result.summary.overlap_count == 0);
}

TEST_CASE("VisualizePairToHtml writes a mutation report", "[visual]")
{
    const std::filesystem::path out = std::filesystem::temp_directory_path()
        / "verosim-visual-report-test.html";
    const std::filesystem::path pred = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "base" / "mono.krn";
    const std::filesystem::path gt
        = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "cases" / "mono_accidental_sharp.krn";

    std::string error;
    REQUIRE(VisualizePairToHtml(pred.string(), gt.string(), out.string(), CompareCliOptions{}, error));
    CHECK(error.empty());

    const std::string html = ReadFile(out);
    CHECK(html.find("VeroSim OMR-NED Visualization") != std::string::npos);
    CHECK(html.find("wrong accidental OMR-ED") != std::string::npos);
    CHECK(html.find("note-L6F1") != std::string::npos);
    CHECK(html.find("verosim-role-changed") != std::string::npos);

    std::filesystem::remove(out);
}

TEST_CASE("WriteSvgAssetBundle writes annotated SVG pages and manifest", "[visual]")
{
    const std::filesystem::path out = std::filesystem::temp_directory_path()
        / "verosim-svg-bundle-test";
    const std::filesystem::path pred = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "base" / "mono.krn";
    const std::filesystem::path gt
        = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "cases" / "mono_accidental_sharp.krn";
    std::filesystem::remove_all(out);

    std::string error;
    VisualReport report;
    REQUIRE(BuildVisualComparison(pred.string(), gt.string(), CompareCliOptions{}, report, error));
    CHECK(error.empty());

    SvgAssetBundle bundle;
    REQUIRE(WriteSvgAssetBundle(report, out.string(), bundle, error));
    CHECK(error.empty());
    REQUIRE(bundle.prediction.pages.size() == 1);
    REQUIRE(bundle.ground_truth.pages.size() == 1);
    CHECK(bundle.prediction.pages[0].page == 1);
    CHECK(bundle.prediction.pages[0].path == "prediction/page-1.svg");
    CHECK(bundle.ground_truth.pages[0].path == "ground_truth/page-1.svg");

    const std::filesystem::path manifest_path = out / "visualization.json";
    const std::filesystem::path pred_svg_path = out / "prediction" / "page-1.svg";
    const std::filesystem::path gt_svg_path = out / "ground_truth" / "page-1.svg";
    REQUIRE(std::filesystem::is_regular_file(manifest_path));
    REQUIRE(std::filesystem::is_regular_file(pred_svg_path));
    REQUIRE(std::filesystem::is_regular_file(gt_svg_path));

    const std::string manifest = ReadFile(manifest_path);
    CHECK(manifest.find("\"schema_version\":1") != std::string::npos);
    CHECK(manifest.find("\"path\":\"prediction/page-1.svg\"") != std::string::npos);
    CHECK(manifest.find("\"path\":\"ground_truth/page-1.svg\"") != std::string::npos);

    const std::string pred_svg = ReadFile(pred_svg_path);
    const std::string gt_svg = ReadFile(gt_svg_path);
    CHECK(pred_svg.find("verosim-role-changed") != std::string::npos);
    CHECK(gt_svg.find("verosim-role-changed") != std::string::npos);
    CHECK(gt_svg.find("note-L6F1") != std::string::npos);

    std::filesystem::remove_all(out);
}
