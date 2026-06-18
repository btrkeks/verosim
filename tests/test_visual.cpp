#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine_fixtures.h"
#include "verosim/engine/edit_op.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/visual/score_renderer.h"
#include "verosim/visual/svg_annotator.h"
#include "verosim/visual/svg_bundle.h"
#include "verosim/visual/svg_overlap.h"
#include "verosim/visual/svg_symbol_index.h"
#include "verosim/visual/visual_plan.h"
#include "verosim/visual/visual_resolver.h"
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

std::size_t CountOccurrences(const std::string &text, const std::string &needle)
{
    std::size_t count = 0;
    for (std::size_t pos = text.find(needle); pos != std::string::npos;
         pos = text.find(needle, pos + needle.size())) {
        ++count;
    }
    return count;
}

SymbolLocator TestLocator(int occurrence = 0, int measure_idx = 0)
{
    return SymbolLocator{ .part_idx = 0,
        .staff_n = "1",
        .measure_idx = measure_idx,
        .measure_vrv_id = "measure-L1",
        .offset = Fraction(0),
        .occurrence = occurrence };
}

ResolvedVisualMark ResolvedMark(
    std::size_t source_index, const VisualMark &mark, const SvgSelector &selector)
{
    return ResolvedVisualMark{ .source_index = source_index, .mark = mark, .selectors = { selector } };
}

} // namespace

TEST_CASE("VisualPlanBuilder maps paired note edits to changed marks on both sides", "[visual]")
{
    SymNote pred = MakeNote("C4", Fraction(0), { .id = "pred-note" });
    pred.locator = TestLocator();
    SymNote gt = MakeNote("C4", Fraction(0), { .id = "gt-note" });
    gt.locator = TestLocator();
    const EditOp op{ .name = OpName::kHeadEdit,
        .a = OpSide::Note(&pred),
        .b = OpSide::Note(&gt),
        .cost = 2 };

    const VisualPlan plan = BuildVisualPlan({ op });
    REQUIRE(plan.marks.size() == 2);
    CHECK(plan.marks[0].side == VisualSide::kPred);
    CHECK(plan.marks[0].role == VisualRole::kChanged);
    CHECK(plan.marks[0].target.kind == VisualTargetKind::kNote);
    CHECK(plan.marks[0].target.primary_id == "pred-note");
    CHECK(plan.marks[0].target.locator.measure_idx == 0);
    CHECK(plan.marks[0].category == "wrong note head OMR-ED");
    CHECK(plan.marks[1].side == VisualSide::kGt);
    CHECK(plan.marks[1].role == VisualRole::kChanged);
    CHECK(plan.marks[1].target.primary_id == "gt-note");
}

TEST_CASE("VisualPlanBuilder maps accidental insertion to visible accidental glyph only", "[visual]")
{
    SymNote pred = MakeNote("C4", Fraction(0), { .id = "pred-note" });
    pred.locator = TestLocator();
    SymNote gt = MakeNote("C4", Fraction(0), { .accid = "sharp", .id = "gt-note" });
    gt.locator = TestLocator();
    const EditOp op{ .name = OpName::kAccidentIns,
        .a = OpSide::Note(&pred),
        .b = OpSide::Note(&gt),
        .cost = 1,
        .ids_kind = EditOp::IdsKind::kPitchPair,
        .ids0 = 0,
        .ids1 = 0 };

    const VisualPlan plan = BuildVisualPlan({ op });
    REQUIRE(plan.marks.size() == 1);
    CHECK(plan.marks[0].side == VisualSide::kGt);
    CHECK(plan.marks[0].role == VisualRole::kChanged);
    CHECK(plan.marks[0].target.kind == VisualTargetKind::kAccidental);
    CHECK(plan.marks[0].target.primary_id == "accid-gt-note");
    CHECK(plan.marks[0].target.fallback_id == "accid-gt-note");
    CHECK(plan.marks[0].target.locator.occurrence == 0);
    CHECK(plan.marks[0].category == "wrong accidental OMR-ED");
}

TEST_CASE("VisualPlanBuilder maps accidental deletion and edit to accidental glyphs", "[visual]")
{
    SymNote pred_sharp = MakeNote("C4", Fraction(0), { .accid = "sharp", .id = "pred-note" });
    pred_sharp.locator = TestLocator();
    SymNote gt_plain = MakeNote("C4", Fraction(0), { .id = "gt-note" });
    gt_plain.locator = TestLocator();
    const EditOp del{ .name = OpName::kAccidentDel,
        .a = OpSide::Note(&pred_sharp),
        .b = OpSide::Note(&gt_plain),
        .cost = 1,
        .ids_kind = EditOp::IdsKind::kPitchPair,
        .ids0 = 0,
        .ids1 = 0 };

    const VisualPlan del_plan = BuildVisualPlan({ del });
    REQUIRE(del_plan.marks.size() == 1);
    CHECK(del_plan.marks[0].side == VisualSide::kPred);
    CHECK(del_plan.marks[0].target.kind == VisualTargetKind::kAccidental);
    CHECK(del_plan.marks[0].target.primary_id == "accid-pred-note");
    CHECK(del_plan.marks[0].label == "changed accidentdel | wrong accidental OMR-ED | cost 1");

    SymNote gt_flat = MakeNote("C4", Fraction(0), { .accid = "flat", .id = "gt-note" });
    gt_flat.locator = TestLocator();
    const EditOp edit{ .name = OpName::kAccidentEdit,
        .a = OpSide::Note(&pred_sharp),
        .b = OpSide::Note(&gt_flat),
        .cost = 2,
        .ids_kind = EditOp::IdsKind::kPitchPair,
        .ids0 = 0,
        .ids1 = 0 };

    const VisualPlan edit_plan = BuildVisualPlan({ edit });
    REQUIRE(edit_plan.marks.size() == 2);
    CHECK(edit_plan.marks[0].side == VisualSide::kPred);
    CHECK(edit_plan.marks[0].target.primary_id == "accid-pred-note");
    CHECK(edit_plan.marks[1].side == VisualSide::kGt);
    CHECK(edit_plan.marks[1].target.primary_id == "accid-gt-note");
    CHECK(edit_plan.marks[1].label == "changed accidentedit | wrong accidental OMR-ED | cost 2");
}

TEST_CASE("VisualPlanBuilder targets chord member accidental IDs", "[visual]")
{
    SymNote pred = MakeNote("E4", Fraction(0), { .id = "chord-L13F1" });
    pred.is_in_chord = true;
    pred.note_idx_in_chord = 1;
    pred.visual_id = "note-L13F1S2";
    pred.locator = TestLocator(1);
    SymNote gt = pred;
    gt.pitches[0].accid = "sharp";
    const EditOp op{ .name = OpName::kAccidentIns,
        .a = OpSide::Note(&pred),
        .b = OpSide::Note(&gt),
        .cost = 1,
        .ids_kind = EditOp::IdsKind::kPitchPair,
        .ids0 = 0,
        .ids1 = 0 };

    const VisualPlan plan = BuildVisualPlan({ op });
    REQUIRE(plan.marks.size() == 1);
    CHECK(plan.marks[0].side == VisualSide::kGt);
    CHECK(plan.marks[0].target.kind == VisualTargetKind::kAccidental);
    CHECK(plan.marks[0].target.primary_id == "accid-L13F1S2");
    CHECK(plan.marks[0].target.fallback_id == "accid-L13F1S2");
    CHECK(plan.marks[0].target.locator.occurrence == 1);
}

TEST_CASE("VisualPlanBuilder maps single-sided note and part ops", "[visual]")
{
    SymNote inserted = MakeNote("D4", Fraction(0), { .id = "inserted-note" });
    inserted.locator = TestLocator();
    const EditOp note_ins{ .name = OpName::kNoteIns,
        .a = OpSide::None(),
        .b = OpSide::Note(&inserted),
        .cost = 2 };

    SymMeasure m1 = MakeMeasure({ MakeNote("C4", Fraction(0)) });
    m1.vrv_id = "measure-one";
    m1.locator = TestLocator(0, 0);
    SymMeasure m2 = MakeMeasure({ MakeNote("D4", Fraction(0)) });
    m2.vrv_id = "measure-two";
    m2.locator = TestLocator(0, 1);
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
    CHECK(plan.marks[0].target.primary_id == "inserted-note");
    CHECK(plan.marks[1].side == VisualSide::kPred);
    CHECK(plan.marks[1].role == VisualRole::kDeleted);
    CHECK(plan.marks[1].target.kind == VisualTargetKind::kMeasure);
    CHECK(plan.marks[1].target.primary_id == "measure-one");
    CHECK(plan.marks[1].target.locator.measure_idx == 0);
    CHECK(plan.marks[2].target.primary_id == "measure-two");
    CHECK(plan.marks[2].target.locator.measure_idx == 1);
}

TEST_CASE("VisualPlanBuilder targets chord member visual IDs", "[visual]")
{
    SymNote member = MakeNote("E4", Fraction(0), { .id = "chord-L13F1" });
    member.is_in_chord = true;
    member.note_idx_in_chord = 1;
    member.visual_id = "note-L13F1S2";
    member.locator = TestLocator(1);
    const EditOp note_ins{ .name = OpName::kNoteIns,
        .a = OpSide::None(),
        .b = OpSide::Note(&member),
        .cost = 2,
        .ids_kind = EditOp::IdsKind::kChordIdx,
        .ids0 = 1 };

    const VisualPlan plan = BuildVisualPlan({ note_ins });
    REQUIRE(plan.marks.size() == 1);
    CHECK(plan.marks[0].side == VisualSide::kGt);
    CHECK(plan.marks[0].role == VisualRole::kInserted);
    CHECK(plan.marks[0].target.kind == VisualTargetKind::kNote);
    CHECK(plan.marks[0].target.primary_id == "note-L13F1S2");
    CHECK(plan.marks[0].target.fallback_id == "chord-L13F1");
    CHECK(plan.marks[0].target.locator.occurrence == 1);
}

TEST_CASE("VisualPlanBuilder emits locator-only symbol refs", "[visual]")
{
    SymNote pred = MakeNote("C4", Fraction(0), { .id = "" });
    pred.vrv_id.clear();
    pred.visual_id.clear();
    pred.locator = TestLocator(2);
    SymNote gt = MakeNote("C4", Fraction(0), { .accid = "sharp", .id = "" });
    gt.vrv_id.clear();
    gt.visual_id.clear();
    gt.locator = TestLocator(2);

    SymExtra extra = MakeTimeSig("3", "4");
    extra.vrv_id.clear();
    extra.locator = TestLocator();

    SymMeasure measure = MakeMeasure({});
    measure.vrv_id.clear();
    measure.locator = TestLocator(0, 1);
    SymPart part;
    part.staff_n = "1";
    part.part_idx = 0;
    part.bar_list = { measure };

    const VisualPlan plan = BuildVisualPlan({
        EditOp{ .name = OpName::kAccidentIns,
            .a = OpSide::Note(&pred),
            .b = OpSide::Note(&gt),
            .cost = 1 },
        EditOp{ .name = OpName::kExtraIns,
            .a = OpSide::None(),
            .b = OpSide::Extra(&extra),
            .cost = 2 },
        EditOp{ .name = OpName::kDelPart,
            .a = OpSide::Part(&part),
            .b = OpSide::None(),
            .cost = 4 },
    });

    REQUIRE(plan.marks.size() == 3);
    CHECK(plan.marks[0].target.kind == VisualTargetKind::kAccidental);
    CHECK(plan.marks[0].target.primary_id.empty());
    CHECK(plan.marks[0].target.locator.occurrence == 2);
    CHECK(plan.marks[1].target.kind == VisualTargetKind::kExtra);
    CHECK(plan.marks[1].target.primary_id.empty());
    CHECK(plan.marks[1].target.extra_kind == ExtraKind::kTimeSig);
    CHECK(plan.marks[1].target.has_extra_kind);
    CHECK(plan.marks[1].target.locator.measure_idx == 0);
    CHECK(plan.marks[2].target.kind == VisualTargetKind::kMeasure);
    CHECK(plan.marks[2].target.primary_id.empty());
    CHECK(plan.marks[2].target.locator.measure_idx == 1);
}

TEST_CASE("SvgAnnotator marks exact IDs and spanning class IDs", "[visual]")
{
    const VisualMark note_mark{ .side = VisualSide::kGt,
        .role = VisualRole::kInserted,
        .target = VisualSymbolRef{ .kind = VisualTargetKind::kNote,
            .primary_id = "note-L6F1",
            .fallback_id = "note-L6F1" },
        .op_name = "noteins",
        .category = "wrong note OMR-ED",
        .cost = 2,
        .label = "inserted noteins | wrong note OMR-ED | cost 2" };
    const VisualMark span_mark{ .side = VisualSide::kGt,
        .role = VisualRole::kChanged,
        .target = VisualSymbolRef{ .kind = VisualTargetKind::kExtra,
            .primary_id = "slur-L1",
            .fallback_id = "slur-L1" },
        .op_name = "slursymboledit",
        .category = "wrong slur OMR-ED",
        .cost = 2,
        .label = "changed slursymboledit | wrong slur OMR-ED | cost 2" };

    const std::string svg
        = "<svg><g id=\"note-L6F1\" class=\"note\"><path/></g><g class=\"slur id-slur-L1 spanning\"/></svg>";
    const std::vector<ResolvedVisualMark> marks{
        ResolvedMark(0, note_mark, SvgSelector{ .kind = SvgSelectorKind::kId, .value = "note-L6F1" }),
        ResolvedMark(
            1, span_mark, SvgSelector{ .kind = SvgSelectorKind::kClassToken, .value = "id-slur-L1" }),
    };
    const SvgAnnotationResult result = AnnotateSvg(svg, marks, 2);
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
        .target = VisualSymbolRef{ .kind = VisualTargetKind::kNote,
            .primary_id = "missing",
            .fallback_id = "missing" },
        .op_name = "notedel",
        .category = "wrong note OMR-ED",
        .cost = 2,
        .label = "deleted notedel | wrong note OMR-ED | cost 2" };
    const SvgAnnotationResult result = AnnotateSvg("<svg><g id=\"present\"/></svg>",
        { ResolvedMark(0, missing, SvgSelector{ .kind = SvgSelectorKind::kId, .value = "missing" }) },
        1);
    REQUIRE(result.parse_ok);
    REQUIRE(result.resolved.size() == 1);
    CHECK_FALSE(result.resolved[0]);
    CHECK(result.svg.find("verosim-mark") == std::string::npos);
}

TEST_CASE("SvgSymbolIndex resolves structural measures notes accidentals and extras", "[visual]")
{
    const std::string svg = R"SVG(
<svg>
  <g id="measure-L1" class="measure">
    <g id="staff-L1F1" class="staff">
      <g id="bbox-random-note-id" class="note bounding-box"/>
      <g id="random-note-id" class="note">
        <g id="random-accid-id" class="accid"/>
      </g>
      <g id="random-meter-id" class="meterSig"/>
    </g>
  </g>
</svg>
)SVG";
    const SvgSymbolIndex index = SvgSymbolIndex::Build(svg);
    REQUIRE(index.parse_ok());
    CHECK(index.measure_count() == 1);
    const SymbolLocator loc = TestLocator();

    const std::optional<SvgSelector> measure = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kMeasure, .locator = loc, .primary_id = "missing-measure" });
    REQUIRE(measure.has_value());
    CHECK(measure->kind == SvgSelectorKind::kId);
    CHECK(measure->value == "measure-L1");

    const std::optional<SvgSelector> note = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kNote, .locator = loc, .primary_id = "missing-note" });
    REQUIRE(note.has_value());
    CHECK(note->kind == SvgSelectorKind::kId);
    CHECK(note->value == "random-note-id");

    SymbolLocator later_page_loc = loc;
    later_page_loc.measure_idx = 12;
    const std::optional<SvgSelector> later_page_note = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kNote,
        .locator = later_page_loc,
        .primary_id = "missing-later-page-note" });
    REQUIRE(later_page_note.has_value());
    CHECK(later_page_note->value == "random-note-id");

    SymbolLocator other_staff_loc = loc;
    other_staff_loc.part_idx = 1;
    const std::optional<SvgSelector> other_staff_note = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kNote,
        .locator = other_staff_loc,
        .primary_id = "missing-other-staff" });
    CHECK_FALSE(other_staff_note.has_value());

    const std::optional<SvgSelector> accid = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kAccidental, .locator = loc, .primary_id = "missing-accid" });
    REQUIRE(accid.has_value());
    CHECK(accid->kind == SvgSelectorKind::kId);
    CHECK(accid->value == "random-accid-id");

    const std::optional<SvgSelector> timesig = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kExtra,
        .locator = loc,
        .primary_id = "missing-timesig",
        .extra_kind = ExtraKind::kTimeSig,
        .has_extra_kind = true });
    REQUIRE(timesig.has_value());
    CHECK(timesig->kind == SvgSelectorKind::kId);
    CHECK(timesig->value == "random-meter-id");

    const SvgSymbolIndex later_page_index = SvgSymbolIndex::Build(svg, 12);
    REQUIRE(later_page_index.parse_ok());
    CHECK(later_page_index.measure_count() == 1);
    SymbolLocator ordinal_only_loc = loc;
    ordinal_only_loc.measure_idx = 12;
    ordinal_only_loc.measure_vrv_id.clear();
    const std::optional<SvgSelector> ordinal_only_note
        = later_page_index.Resolve(VisualSymbolRef{ .kind = VisualTargetKind::kNote,
            .locator = ordinal_only_loc,
            .primary_id = "missing-ordinal-note" });
    REQUIRE(ordinal_only_note.has_value());
    CHECK(ordinal_only_note->value == "random-note-id");

    ordinal_only_loc.measure_idx = 0;
    const std::optional<SvgSelector> page_local_note
        = later_page_index.Resolve(VisualSymbolRef{ .kind = VisualTargetKind::kNote,
            .locator = ordinal_only_loc,
            .primary_id = "missing-page-local-note" });
    CHECK_FALSE(page_local_note.has_value());
}

TEST_CASE("VisualResolver keeps exact ID fast path and class-token fallback", "[visual]")
{
    const std::string svg = R"SVG(
<svg>
  <g id="measure-L1" class="measure">
    <g id="staff-L1F1" class="staff">
      <g id="exact-note" class="note"/>
      <g class="slur id-slur-L1 spanning"/>
    </g>
  </g>
</svg>
)SVG";
    const std::vector<VisualMark> marks{
        VisualMark{ .side = VisualSide::kGt,
            .role = VisualRole::kInserted,
            .target = VisualSymbolRef{ .kind = VisualTargetKind::kNote,
                .locator = TestLocator(),
                .primary_id = "exact-note" },
            .op_name = "noteins",
            .category = "wrong note OMR-ED",
            .cost = 1,
            .label = "note" },
        VisualMark{ .side = VisualSide::kGt,
            .role = VisualRole::kChanged,
            .target = VisualSymbolRef{ .kind = VisualTargetKind::kExtra,
                .primary_id = "slur-L1",
                .extra_kind = ExtraKind::kSlur,
                .has_extra_kind = true },
            .op_name = "slursymboledit",
            .category = "wrong slur OMR-ED",
            .cost = 1,
            .label = "slur" },
    };

    const VisualResolveResult result = ResolveVisualMarks(svg, marks);
    REQUIRE(result.parse_ok);
    CHECK(result.measure_count == 1);
    REQUIRE(result.resolved.size() == 2);
    REQUIRE(result.marks.size() == 2);
    CHECK(result.resolved[0]);
    CHECK(result.marks[0].selectors[0].kind == SvgSelectorKind::kId);
    CHECK(result.marks[0].selectors[0].value == "exact-note");
    CHECK(result.resolved[1]);
    CHECK(result.marks[1].selectors[0].kind == SvgSelectorKind::kClassToken);
    CHECK(result.marks[1].selectors[0].value == "id-slur-L1");
}

TEST_CASE("SvgSymbolIndex keeps rendered staff order stable across a page", "[visual]")
{
    const std::string svg = R"SVG(
<svg>
  <g id="measure-L1" class="measure">
    <g id="staff-L1F2" class="staff">
      <g id="top-note" class="note"/>
    </g>
    <g id="staff-L1F1" class="staff">
      <g id="bottom-note" class="note"/>
    </g>
  </g>
  <g id="measure-L2" class="measure">
    <g id="staff-L2F1N1" class="staff">
      <g id="bottom-only-note" class="note"/>
    </g>
  </g>
</svg>
)SVG";

    const SvgSymbolIndex index = SvgSymbolIndex::Build(svg);
    REQUIRE(index.parse_ok());

    const SymbolLocator bottom_staff{ .part_idx = 1,
        .measure_idx = 1,
        .measure_vrv_id = "measure-L2",
        .offset = Fraction(0),
        .occurrence = 0 };
    const std::optional<SvgSelector> bottom_note = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kNote,
        .locator = bottom_staff,
        .primary_id = "missing-bottom-note" });
    REQUIRE(bottom_note.has_value());
    CHECK(bottom_note->value == "bottom-only-note");

    SymbolLocator top_staff = bottom_staff;
    top_staff.part_idx = 0;
    const std::optional<SvgSelector> top_note = index.Resolve(VisualSymbolRef{
        .kind = VisualTargetKind::kNote,
        .locator = top_staff,
        .primary_id = "missing-top-note" });
    CHECK_FALSE(top_note.has_value());
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

TEST_CASE("RenderScoreToSvgPages honors encoded Humdrum line breaks", "[visual]")
{
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / "verosim-encoded-linebreak-test.krn";
    {
        std::ofstream out(path);
        out << R"KERN(**kern
*clefG2
*k[]
*M4/4
=1
4c
4d
4e
4f
=2
4g
4a
4b
4cc
!!LO:LB:g=original
=3
4cc
4b
4a
4g
=4
4f
4e
4d
4c
==
*-
)KERN";
    }

    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(path.string()));

    RenderedScore rendered;
    std::string error;
    REQUIRE(RenderScoreToSvgPages(bridge, rendered, error));
    CHECK(error.empty());
    REQUIRE(rendered.pages.size() == 1);
    CHECK(CountOccurrences(rendered.pages[0].svg, "class=\"system\"") == 2);

    std::filesystem::remove(path);
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
    CHECK(html.find("id=\"accid-L6F1\" class=\"accid verosim-mark") != std::string::npos);
    CHECK(html.find("id=\"note-L6F1\" class=\"note verosim-mark") == std::string::npos);
    CHECK(html.find("verosim-kind-accidental") != std::string::npos);

    std::filesystem::remove(out);
}

TEST_CASE("VisualizePairToHtml marks missing chord members by notehead group", "[visual]")
{
    const std::filesystem::path out = std::filesystem::temp_directory_path()
        / "verosim-visual-chord-member-test.html";
    const std::filesystem::path pred
        = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "cases" / "chord_member_delete.krn";
    const std::filesystem::path gt = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "base" / "chords.krn";

    std::string error;
    REQUIRE(VisualizePairToHtml(pred.string(), gt.string(), out.string(), CompareCliOptions{}, error));
    CHECK(error.empty());

    const std::string html = ReadFile(out);
    CHECK(html.find("id=\"note-L13F1S2\" class=\"note verosim-mark") != std::string::npos);
    CHECK(html.find("id=\"chord-L13F1\" class=\"chord verosim-mark") == std::string::npos);
    CHECK(html.find("inserted noteins | wrong note OMR-ED | cost 2") != std::string::npos);

    std::filesystem::remove(out);
}

TEST_CASE("BuildVisualComparison marks time signature extras", "[visual]")
{
    const std::filesystem::path pred
        = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "cases" / "mono_timesig_num.krn";
    const std::filesystem::path gt = std::filesystem::path(VEROSIM_MUTATIONS_DIR) / "base" / "mono.krn";

    std::string error;
    VisualReport report;
    REQUIRE(BuildVisualComparison(pred.string(), gt.string(), CompareCliOptions{}, report, error));
    CHECK(error.empty());
    CHECK(report.unresolved_marks.empty());
    REQUIRE(report.pred.pages.size() == 1);
    REQUIRE(report.gt.pages.size() == 1);

    CHECK(report.pred.pages[0].svg.find("class=\"meterSig verosim-mark") != std::string::npos);
    CHECK(report.gt.pages[0].svg.find("class=\"meterSig verosim-mark") != std::string::npos);
    CHECK(report.pred.pages[0].svg.find("wrong timesig OMR-ED") != std::string::npos);
    CHECK(report.gt.pages[0].svg.find("wrong timesig OMR-ED") != std::string::npos);
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
    CHECK(pred_svg.find("verosim-mark.verosim-role-changed") != std::string::npos);
    CHECK(pred_svg.find("fill: #b97900 !important") != std::string::npos);
    CHECK(pred_svg.find("id=\"accid-L6F1\" class=\"accid verosim-mark") == std::string::npos);
    CHECK(pred_svg.find("id=\"note-L6F1\" class=\"note verosim-mark") == std::string::npos);
    CHECK(gt_svg.find("id=\"accid-L6F1\" class=\"accid verosim-mark") != std::string::npos);
    CHECK(gt_svg.find("id=\"note-L6F1\" class=\"note verosim-mark") == std::string::npos);
    CHECK(gt_svg.find("verosim-kind-accidental") != std::string::npos);
    CHECK(gt_svg.find("note-L6F1") != std::string::npos);

    std::filesystem::remove_all(out);
}
