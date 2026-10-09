#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <jsonxx.h>
#include "staffdef.h"

#include "engine_fixtures.h"
#include "verosim/app/evaluation_profile_loader.h"
#include "verosim/app/score_pipeline.h"
#include "verosim/cli/args.h"
#include "verosim/cli/count_symbols.h"
#include "verosim/engine/note_diff.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/visual/score_renderer.h"
#include "verosim/visual/visual_plan.h"
#include "verosim/visual/visual_resolver.h"

using namespace verosim;
using namespace verosim::test;

namespace {

std::string FixturePath(const std::string &name)
{
    return std::string(VEROSIM_TEST_FIXTURE_DIR) + "/" + name;
}

std::string ReadFixture(const std::string &name)
{
    std::ifstream in(FixturePath(name));
    REQUIRE(in.good());
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

ExtractResult ExtractFixture(const std::string &name, bool enabled)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(FixturePath(name)));
    return ExtractSymScore(bridge.GetDoc(), name.ends_with(".krn")
            ? SourceFormat::kKern : SourceFormat::kOther,
        ExtractOptions{ .surface = MetricSurface{ .score_display_staff = enabled } });
}

std::string Mei(const std::string &staffs, const std::string &definitions
    = "<staffDef n=\"10\" lines=\"5\"/><staffDef n=\"30\" lines=\"5\"/>")
{
    return "<mei xmlns=\"http://www.music-encoding.org/ns/mei\" meiversion=\"5.0\">"
        "<music><body><mdiv><score><scoreDef><staffGrp>" + definitions
        + "</staffGrp></scoreDef><section><measure n=\"1\">" + staffs
        + "</measure></section></score></mdiv></body></music></mei>";
}

ExtractResult ExtractMei(const std::string &data, bool enabled = true)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreData(data, vrv::MEI));
    return ExtractSymScore(bridge.GetDoc(), SourceFormat::kOther,
        ExtractOptions{ .surface = MetricSurface{ .score_display_staff = enabled } });
}

ExtractResult ExtractKern(const std::string &data, bool enabled = true)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreData(data, vrv::HUMDRUM));
    return ExtractSymScore(bridge.GetDoc(), SourceFormat::kKern,
        ExtractOptions{ .surface = MetricSurface{ .score_display_staff = enabled } });
}

const SymNote &NoteWithId(const SymScore &score, const std::string &id)
{
    for (const SymPart &part : score.parts) {
        for (const SymMeasure &measure : part.bar_list) {
            for (const SymNote &note : measure.notes) {
                if (note.visual_id == id) return note;
            }
        }
    }
    FAIL("missing note " + id);
    throw std::logic_error("missing note");
}

CompareOptions Musical()
{
    return CompareOptions{ .note_position_policy = NotePositionPolicy::kMusicalOnset };
}

} // namespace

TEST_CASE("display staff adds one symbol and one edit without changing note pairing",
    "[display-staff][engine]")
{
    SymNote home = MakeNote("C4", Fraction(0), { .id = "home" });
    const std::string home_str = home.str();
    const std::string home_repr = home.repr();
    CHECK(home.notation_size() == 2);
    CHECK(home_str == "[C4]4");
    CHECK(home_repr.find("displayStaff") == std::string::npos);

    for (const int displacement : { -2, -1, 1, 2 }) {
        SymNote cross = home;
        cross.display_staff_offset = displacement;
        CHECK(cross.notation_size() == 3);
        CHECK(cross.str() != home_str);
        CHECK(cross.repr() != home_repr);
        for (const int target : { -2, -1, 0, 1, 2 }) {
            SymNote other = home;
            other.display_staff_offset = target;
            const long expected = displacement == target ? 0 : 1;
            const DiffResult diff = AnnotatedNoteDiff(cross, other);
            CHECK(diff.cost == expected);
            CHECK(AnnotatedNoteDiffCost(cross, other) == expected);
            CHECK(diff.ops.size() == static_cast<std::size_t>(expected));
            if (expected != 0) {
                CHECK(OpNameStr(diff.ops[0].name) == "displaystaffedit");
                CHECK(EditOpCategory(diff.ops[0]) == "wrong display staff OMR-ED");
                CHECK(diff.ops[0].cost == 1);
                CHECK(diff.ops[0].a.note == &cross);
                CHECK(diff.ops[0].b.note == &other);
            }
        }
        const SymScore pred = MakeScore({ { MakeMeasure({ cross }) } });
        const SymScore gt = MakeScore({ { MakeMeasure({ home }) } });
        CHECK(CountSymbols(pred).display_staff == 1);
        CHECK(CountSymbols(pred).total() == 3);
        for (const CompareOptions options : { CompareOptions{}, Musical() }) {
            const CompareResult comparison = CompareScores(pred, gt, options);
            CHECK(comparison.cost == 1);
            REQUIRE(comparison.op_list.size() == 1);
            CHECK(comparison.op_list[0].name == OpName::kDisplayStaffEdit);
            CHECK(CompareScores(gt, pred, options).cost == 1);
        }
    }

    SymNote cross = home;
    cross.display_staff_offset = -1;
    cross.pitches[0].step_octave = "D4";
    const SymScore shorter = MakeScore({ { MakeMeasure({ home }) } });
    const SymScore longer = MakeScore({ { MakeMeasure({ home, cross }) } });
    const CompareResult inserted = CompareScores(shorter, longer);
    const CompareResult deleted = CompareScores(longer, shorter);
    CHECK(inserted.cost == 3);
    CHECK(deleted.cost == 3);
    REQUIRE(inserted.op_list.size() == 1);
    REQUIRE(deleted.op_list.size() == 1);
    CHECK(inserted.op_list[0].name == OpName::kNoteIns);
    CHECK(deleted.op_list[0].name == OpName::kNoteDel);
}

TEST_CASE("real kern RDF above and below are opt-in display placements",
    "[display-staff][extract]")
{
    const ExtractResult home = ExtractFixture("display_staff_home.krn", true);
    REQUIRE(home.score.parts.size() == 2);
    CHECK(home.score.notation_size() == 12);
    CHECK(CountSymbols(home.score).display_staff == 0);
    CHECK(home.warnings.empty());
    for (const std::string name : { "display_staff_above.krn", "display_staff_below.krn" }) {
        const ExtractResult legacy = ExtractFixture(name, false);
        const ExtractResult enabled = ExtractFixture(name, true);
        CHECK(CompareScores(legacy.score, home.score).cost == 0);
        CHECK(enabled.score.notation_size() == 13);
        CHECK(CountSymbols(enabled.score).display_staff == 1);
        CHECK(CountSymbols(enabled.score).total() == 13);
        CHECK(enabled.warnings.empty());
        const int part = name == "display_staff_above.krn" ? 1 : 0;
        const SymNote &note = enabled.score.parts[part].bar_list[0].notes[0];
        const SymNote &original = legacy.score.parts[part].bar_list[0].notes[0];
        CHECK(note.display_staff_offset == (part == 1 ? -1 : 1));
        CHECK(note.pitches[0].step_octave == original.pitches[0].step_octave);
        CHECK(note.note_offset == original.note_offset);
        CHECK(note.locator.part_idx == part);
        CHECK(note.visual_id == original.visual_id);
        for (const CompareOptions options : { CompareOptions{}, Musical() }) {
            CHECK(CompareScores(enabled.score, home.score, options).cost == 1);
            CHECK(CompareScores(home.score, enabled.score, options).cost == 1);
        }
    }

    const std::string no_rdf = "**kern\t**kern\n*clefF4\t*clefG2\n"
        "*M4/4\t*M4/4\n=1\t=1\n1C\t1c<\n==\t==\n*-\t*-\n";
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreData(no_rdf, vrv::HUMDRUM));
    const ExtractResult no_definition = ExtractSymScore(bridge.GetDoc(), SourceFormat::kKern,
        ExtractOptions{ .surface = MetricSurface{ .score_display_staff = true } });
    CHECK(CountSymbols(no_definition.score).display_staff == 0);
    CHECK(CompareScores(no_definition.score, home.score).cost == 0);
}

TEST_CASE("display staff uses definition order and independent chord member and rest overrides",
    "[display-staff][extract]")
{
    const ExtractResult result = ExtractFixture("display_staff_mixed.mei", true);
    REQUIRE(result.score.parts.size() == 3);
    CHECK(result.score.parts[0].staff_n == "30");
    CHECK(result.score.parts[1].staff_n == "10");
    CHECK(result.score.parts[2].staff_n == "90");
    CHECK(result.warnings.empty());
    CHECK(NoteWithId(result.score, "note-inherited").display_staff_offset == 1);
    CHECK(NoteWithId(result.score, "note-home").display_staff_offset == 0);
    CHECK(NoteWithId(result.score, "note-later-inherited").display_staff_offset == 1);
    CHECK(NoteWithId(result.score, "note-distant").display_staff_offset == 2);
    CHECK(NoteWithId(result.score, "rest-cross").display_staff_offset == 1);
    CHECK(NoteWithId(result.score, "note-above").display_staff_offset == -1);
    CHECK(NoteWithId(result.score, "note-upper").display_staff_offset == -1);
    CHECK(NoteWithId(result.score, "note-inherited").vrv_id == "placement-chord");
    CHECK(NoteWithId(result.score, "note-distant").locator.staff_n == "30");
    CHECK(NoteWithId(result.score, "rest-cross").pitches[0].step_octave == "R");
    CHECK(result.score.notation_size() == 23);
    CHECK(CountSymbols(result.score).display_staff == 6);
    CHECK(CountSymbols(result.score).total() == 23);
    const ExtractResult legacy = ExtractFixture("display_staff_mixed.mei", false);
    CHECK(legacy.score.notation_size() == 17);
    CHECK(CountSymbols(legacy.score).display_staff == 0);
    CHECK(CompareScores(legacy.score, result.score).cost == 6);
    CHECK(CompareScores(legacy.score, result.score, Musical()).cost == 6);
}

TEST_CASE("real kern whole-chord placements count per member and expose imported boundary errors",
    "[display-staff][extract][cli]")
{
    const auto kern = [](const std::string &top) {
        return "**kern\t**kern\n*clefF4\t*clefG2\n*M4/4\t*M4/4\n=1\t=1\n1C\t"
            + top + "\n==\t==\n*-\t*-\n!!!RDF**kern: > = above\n!!!RDF**kern: < = below\n";
    };
    const ExtractResult home = ExtractKern(kern("1c 1e"));
    const ExtractResult cross = ExtractKern(kern("1c< 1e<"));
    const ExtractResult legacy = ExtractKern(kern("1c< 1e<"), false);
    CHECK(home.score.notation_size() == 14);
    CHECK(cross.score.notation_size() == 16);
    CHECK(CountSymbols(cross.score).display_staff == 2);
    CHECK(legacy.score.notation_size() == 14);
    CHECK(cross.warnings.empty());
    for (const CompareOptions options : { CompareOptions{}, Musical() }) {
        const CompareResult diff = CompareScores(home.score, cross.score, options);
        CHECK(diff.cost == 2);
        REQUIRE(diff.op_list.size() == 2);
        CHECK(diff.op_list[0].name == OpName::kDisplayStaffEdit);
        CHECK(diff.op_list[1].name == OpName::kDisplayStaffEdit);
        CHECK(CompareScores(cross.score, home.score, options).cost == 2);
    }

    const ExtractResult invalid_chord = ExtractKern(kern("1c> 1e>"));
    CHECK(invalid_chord.score.parts.size() == 2);
    CHECK(CountSymbols(invalid_chord.score).display_staff == 0);
    REQUIRE(invalid_chord.warnings.size() == 1);
    CHECK(invalid_chord.warnings[0].find("chord-L5F2") != std::string::npos);
    CHECK(invalid_chord.warnings[0].find("target '0'") != std::string::npos);
    CHECK(CompareScores(invalid_chord.score, home.score).cost == 0);
    const ExtractResult invalid_single = ExtractKern(kern("1c>"));
    CHECK(CountSymbols(invalid_single.score).display_staff == 0);
    CHECK(invalid_single.warnings.empty());

    CompareCliOptions options;
    options.surface.score_display_staff = true;
    VrvBridge bridge;
    std::ostringstream out;
    REQUIRE(CompareScoreDataToJson(bridge, kern("1c> 1e>"), kern("1c 1e"), options, "{", out));
    jsonxx::Object record;
    REQUIRE(record.parse(out.str()));
    CHECK(record.get<jsonxx::Number>("distance") == 0);
    REQUIRE(record.get<jsonxx::Array>("warnings").size() == 1);
    CHECK(out.str().find("using home staff 1") != std::string::npos);
}

TEST_CASE("invalid display targets warn and use home without phantom parts or chord inheritance",
    "[display-staff][extract]")
{
    for (const std::string target : { "999", "10 30", "0", "-1" }) {
        const std::string data = Mei("<staff n=\"10\"><layer><chord dur=\"1\" staff=\"30\">"
            "<note xml:id=\"invalid-child\" pname=\"c\" oct=\"4\" staff=\"" + target
            + "\"/><note xml:id=\"inherited\" pname=\"e\" oct=\"4\"/>"
            "</chord></layer></staff><staff n=\"30\"><layer><rest dur=\"1\"/></layer></staff>");
        const ExtractResult result = ExtractMei(data);
        REQUIRE(result.score.parts.size() == 2);
        CHECK(NoteWithId(result.score, "invalid-child").display_staff_offset == 0);
        CHECK(NoteWithId(result.score, "inherited").display_staff_offset == 1);
        REQUIRE(result.warnings.size() == 1);
        CHECK(result.warnings[0].find("invalid-child") != std::string::npos);
        CHECK(result.warnings[0].find(target) != std::string::npos);
        CHECK(ExtractMei(data, false).warnings.empty());
    }
    const ExtractResult absent = ExtractMei(Mei(
        "<staff n=\"10\"><layer><note xml:id=\"absent\" pname=\"c\" oct=\"4\" dur=\"1\" staff=\"30\"/>"
        "</layer></staff>"));
    CHECK(NoteWithId(absent.score, "absent").display_staff_offset == 0);
    REQUIRE(absent.warnings.size() == 1);
    CHECK(absent.warnings[0].find("30") != std::string::npos);

    const ExtractResult invalid_carrier = ExtractMei(Mei(
        "<staff n=\"10\"><layer><chord xml:id=\"invalid-chord\" dur=\"1\" staff=\"999\">"
        "<note xml:id=\"first-member\" pname=\"c\" oct=\"4\"/>"
        "<note xml:id=\"second-member\" pname=\"e\" oct=\"4\"/>"
        "</chord></layer></staff><staff n=\"30\"><layer><rest dur=\"1\"/></layer></staff>"));
    CHECK(NoteWithId(invalid_carrier.score, "first-member").display_staff_offset == 0);
    CHECK(NoteWithId(invalid_carrier.score, "second-member").display_staff_offset == 0);
    REQUIRE(invalid_carrier.warnings.size() == 1);
    CHECK(invalid_carrier.warnings[0].find("invalid-chord") != std::string::npos);
    CHECK(invalid_carrier.warnings[0].find("999") != std::string::npos);

    const ExtractResult duplicate = ExtractMei(Mei(
        "<staff n=\"10\"><layer><note xml:id=\"duplicate\" pname=\"c\" oct=\"4\" dur=\"1\" staff=\"30 30\"/>"
        "</layer></staff><staff n=\"30\"><layer><rest dur=\"1\"/></layer></staff>"));
    CHECK(NoteWithId(duplicate.score, "duplicate").display_staff_offset == 1);
    CHECK(duplicate.warnings.empty());

    const std::string implicit_data = Mei(
        "<staff n=\"10\"><layer><note xml:id=\"implicit-target\" pname=\"c\" oct=\"4\" dur=\"1\" staff=\"30\"/>"
        "</layer></staff><staff n=\"30\"><layer><rest dur=\"1\"/></layer></staff>");
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreData(implicit_data, vrv::MEI));
    for (vrv::Object *object : bridge.GetDoc().FindAllDescendantsByType(vrv::STAFFDEF)) {
        auto *definition = dynamic_cast<vrv::StaffDef *>(object);
        REQUIRE(definition != nullptr);
        if (definition->GetN() == 30) REQUIRE(definition->GetParent()->DeleteChild(definition));
    }
    const ExtractResult implicit = ExtractSymScore(bridge.GetDoc(), SourceFormat::kOther,
        ExtractOptions{ .surface = MetricSurface{ .score_display_staff = true } });
    const ExtractResult implicit_legacy = ExtractSymScore(bridge.GetDoc(), SourceFormat::kOther);
    REQUIRE(implicit.score.parts.size() == 2);
    CHECK(implicit.score.parts[0].staff_n == "10");
    CHECK(implicit.score.parts[1].staff_n == "30");
    CHECK(NoteWithId(implicit.score, "implicit-target").display_staff_offset == 1);
    CHECK(implicit.warnings == implicit_legacy.warnings);
}

TEST_CASE("display staff option reaches every compare and count form in either position",
    "[display-staff][cli]")
{
    const std::vector<std::vector<std::string>> forms = {
        { "pred.krn", "gt.krn" }, { "--pairs", "pairs.tsv" },
        { "--batch", "pairs.tsv", "--jobs", "2" },
        { "--batch-jsonl", "scores.jsonl" },
        { "--visualize", "pred.krn", "gt.krn", "--out", "report.html" },
        { "--visualize", "pred.krn", "gt.krn", "--out-dir", "pages", "--output-format", "svg" },
        { "--count-symbols", "score.krn" },
        { "--count-symbols", "--per-measure", "score.krn" },
        { "--count-symbols", "--files-from", "scores.txt", "--base-dir", "scores" },
    };
    for (const auto &form : forms) {
        for (const bool leading : { false, true }) {
            auto args = form;
            args.insert(leading ? args.begin() : args.end(), "--display-staff");
            std::string error;
            const auto command = ParseCommand(args, error);
            INFO(error);
            REQUIRE(command.has_value());
            std::visit([](const auto &cmd) {
                using CommandType = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<CommandType, CountSymbolsCommand>) {
                    CHECK(cmd.surface.score_display_staff);
                }
                else if constexpr (std::is_same_v<CommandType, CompareCommand>
                    || std::is_same_v<CommandType, PairsCommand>
                    || std::is_same_v<CommandType, BatchCommand>
                    || std::is_same_v<CommandType, BatchJsonlCommand>
                    || std::is_same_v<CommandType, VisualizeCommand>) {
                    CHECK(cmd.options.surface.score_display_staff);
                }
                else {
                    FAIL("unexpected command");
                }
            }, *command);
        }
    }
    for (const std::vector<std::string> args : {
        std::vector<std::string>{ "--display-staff", "--check", "score.krn" },
        { "--dump-tree", "score.krn", "--display-staff" } }) {
        std::string error;
        CHECK_FALSE(ParseCommand(args, error).has_value());
    }
}

TEST_CASE("display staff count JSON preserves the default schema and totals",
    "[display-staff][cli]")
{
    for (const bool enabled : { false, true }) {
        VrvBridge bridge;
        std::ostringstream out;
        REQUIRE(CountSymbolsFile(bridge, FixturePath("display_staff_below.krn"),
            CountSymbolsOptions{ .per_measure = true,
                .surface = MetricSurface{ .score_display_staff = enabled } }, out));
        jsonxx::Object record;
        REQUIRE(record.parse(out.str()));
        CHECK(record.get<jsonxx::Number>("total") == (enabled ? 13 : 12));
        const auto &categories = record.get<jsonxx::Object>("categories");
        CHECK(categories.has<jsonxx::Number>("display_staff") == enabled);
        if (enabled) CHECK(categories.get<jsonxx::Number>("display_staff") == 1);
        CHECK((out.str().find("displayStaff=1") != std::string::npos) == enabled);
    }
}

TEST_CASE("profile v1 rejects display staff requests before projection or loading",
    "[display-staff][evaluation-profile]")
{
    for (const std::vector<std::string> args : {
        std::vector<std::string>{ "--display-staff", "pred.krn", "gt.krn", "--evaluation-profile", "scope.json" },
        { "--evaluation-profile", "scope.json", "pred.krn", "gt.krn", "--display-staff" } }) {
        std::string error;
        CHECK_FALSE(ParseCommand(args, error).has_value());
        CHECK(error == kDisplayStaffProfileConflict);
    }
    std::string error;
    const auto profile = LoadEvaluationProfileFile(
        FixturePath("evaluation_profile_ignore_projectable_v1.json"), error);
    REQUIRE(profile != nullptr);
    CompareCliOptions cli;
    cli.evaluation_profile = profile;
    cli.surface.score_display_staff = true;
    CHECK_FALSE(PrepareEvaluationProfile(cli, error));
    CHECK(error == kDisplayStaffProfileConflict);
    CompareRunOptions options;
    options.evaluation_profile = profile;
    options.surface.score_display_staff = true;
    const SymScore score = MakeScore({ { MakeMeasure({ MakeNote("C4", Fraction(0)) }) } });
    CHECK_THROWS_AS(ExtractOptionsForRun(options), std::invalid_argument);
    CHECK_THROWS_AS(CompareLoadedScores(score, score, options), std::invalid_argument);
    SymScore cross = score;
    cross.parts[0].bar_list[0].notes[0].display_staff_offset = 1;
    CHECK_THROWS_AS(ProjectSymScore(cross, *profile), std::invalid_argument);
    CHECK(ProjectSymScore(score, *profile).score.notation_size() == 2);
}

TEST_CASE("display staff chord member and rest edits resolve to their real SVG IDs",
    "[display-staff][visual]")
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(FixturePath("display_staff_mixed.mei")));
    const ExtractResult extracted = ExtractSymScore(bridge.GetDoc(), SourceFormat::kOther,
        ExtractOptions{ .surface = MetricSurface{ .score_display_staff = true } });
    const SymNote &cross = NoteWithId(extracted.score, "note-inherited");
    const SymNote &rest = NoteWithId(extracted.score, "rest-cross");
    SymNote home = cross;
    home.display_staff_offset = 0;
    SymNote home_rest = rest;
    home_rest.display_staff_offset = 0;
    const DiffResult note_diff = AnnotatedNoteDiff(cross, home);
    const DiffResult rest_diff = AnnotatedNoteDiff(rest, home_rest);
    std::vector<EditOp> ops = note_diff.ops;
    ops.insert(ops.end(), rest_diff.ops.begin(), rest_diff.ops.end());
    const VisualPlan plan = BuildVisualPlan(ops);
    REQUIRE(plan.marks.size() == 4);
    CHECK(plan.marks[0].target.primary_id == "note-inherited");
    CHECK(plan.marks[0].target.primary_id != cross.vrv_id);
    CHECK(plan.marks[2].target.primary_id == "rest-cross");
    RenderedScore rendered;
    std::string error;
    REQUIRE(RenderScoreToSvgPages(bridge, rendered, error));
    REQUIRE(rendered.pages.size() == 1);
    const VisualResolveResult resolved = ResolveVisualMarks(rendered.pages[0].svg, plan.marks);
    REQUIRE(resolved.parse_ok);
    REQUIRE(resolved.resolved.size() == 4);
    CHECK(std::all_of(resolved.resolved.begin(), resolved.resolved.end(), [](bool value) { return value; }));
    CHECK(resolved.marks.size() == 4);
}
