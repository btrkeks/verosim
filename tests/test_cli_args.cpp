#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <variant>

#include <catch2/catch_test_macros.hpp>
#include <jsonxx.h>

#include "verosim/cli/args.h"
#include "verosim/cli/batch.h"
#include "verosim/cli/list_io.h"
#include "verosim/model/metric_mode.h"

using namespace verosim;

TEST_CASE("MetricMode helpers map the public mode names", "[cli]")
{
    REQUIRE(ParseMetricMode("active") == MetricMode::kActive);
    REQUIRE(ParseMetricMode("experimental") == MetricMode::kExperimental);
    CHECK_FALSE(ParseMetricMode("tierAB").has_value());
    CHECK_FALSE(ParseMetricMode("allobjects").has_value());

    CHECK(MetricModeName(MetricMode::kActive) == "active");
    CHECK_FALSE(MetricModeIncludesDirections(MetricMode::kActive));
    CHECK(MetricModeIncludesDirections(MetricMode::kExperimental));

    REQUIRE(ParseLayoutSurface("none") == LayoutSurface::kNone);
    REQUIRE(ParseLayoutSurface("system-breaks") == LayoutSurface::kSystemBreaks);
    CHECK_FALSE(ParseLayoutSurface("page-breaks").has_value());
    CHECK(LayoutSurfaceName(LayoutSurface::kSystemBreaks) == "system-breaks");
    CHECK(MetricSurfaceIncludesSystemBreaks(
        MetricSurface{ .layout = LayoutSurface::kSystemBreaks }));

    REQUIRE(ParseNotePositionPolicy("visual") == NotePositionPolicy::kVisualEventOrder);
    REQUIRE(ParseNotePositionPolicy("musical") == NotePositionPolicy::kMusicalOnset);
    CHECK(NotePositionPolicyName(NotePositionPolicy::kVisualEventOrder) == "visual");
    CHECK_FALSE(ParseNotePositionPolicy("rhythmic").has_value());

    REQUIRE(ParseTypedSpaceHandling("preserve") == TypedSpaceHandling::kPreserve);
    REQUIRE(ParseTypedSpaceHandling("suppress-straddle-filler")
        == TypedSpaceHandling::kSuppressStraddleFiller);
    CHECK(TypedSpaceHandlingName(TypedSpaceHandling::kPreserve) == std::string("preserve"));
    CHECK_FALSE(ParseTypedSpaceHandling("auto").has_value());
}

TEST_CASE("ParseCommand defaults compare and count-symbols modes to active", "[cli]")
{
    std::string error;
    const auto compare_cmd = ParseCommand({ "pred.krn", "gt.krn" }, error);
    REQUIRE(compare_cmd.has_value());
    const auto *compare = std::get_if<CompareCommand>(&*compare_cmd);
    REQUIRE(compare != nullptr);
    CHECK(compare->pred_path == "pred.krn");
    CHECK(compare->gt_path == "gt.krn");
    CHECK(compare->options.surface.mode == MetricMode::kActive);
    CHECK(compare->options.surface.layout == LayoutSurface::kNone);
    CHECK(compare->options.note_position_policy == NotePositionPolicy::kVisualEventOrder);
    CHECK(compare->options.typed_space_handling == TypedSpaceHandling::kSuppressStraddleFiller);

    error.clear();
    const auto count_cmd = ParseCommand({ "--count-symbols", "score.krn" }, error);
    REQUIRE(count_cmd.has_value());
    const auto *count = std::get_if<CountSymbolsCommand>(&*count_cmd);
    REQUIRE(count != nullptr);
    CHECK(count->input_kind == CountSymbolsCommand::InputKind::kFile);
    CHECK(count->path == "score.krn");
    CHECK(count->surface.mode == MetricMode::kActive);
    CHECK(count->surface.layout == LayoutSurface::kNone);
}

TEST_CASE("ParseCommand keeps compare options with compare-like commands", "[cli]")
{
    std::string error;
    auto parsed = ParseCommand({ "pred.krn", "gt.krn", "--ops", "--mode", "experimental",
        "--layout", "system-breaks", "--note-position", "musical",
        "--typed-space-handling", "preserve" }, error);
    REQUIRE(parsed.has_value());
    const auto *compare = std::get_if<CompareCommand>(&*parsed);
    REQUIRE(compare != nullptr);
    CHECK(compare->options.emit_ops);
    CHECK(compare->options.surface.mode == MetricMode::kExperimental);
    CHECK(compare->options.surface.layout == LayoutSurface::kSystemBreaks);
    CHECK(compare->options.note_position_policy == NotePositionPolicy::kMusicalOnset);
    CHECK(compare->options.typed_space_handling == TypedSpaceHandling::kPreserve);

    error.clear();
    parsed = ParseCommand({ "--pairs", "pairs.tsv", "--base-dir", "data", "--ops",
        "--mode", "experimental" }, error);
    REQUIRE(parsed.has_value());
    const auto *pairs = std::get_if<PairsCommand>(&*parsed);
    REQUIRE(pairs != nullptr);
    CHECK(pairs->args.list_path == "pairs.tsv");
    CHECK(pairs->args.base_dir == "data");
    CHECK(pairs->options.emit_ops);
    CHECK(pairs->options.surface.mode == MetricMode::kExperimental);

    error.clear();
    parsed = ParseCommand({ "--batch", "pairs.tsv", "--jobs", "4", "--base-dir", "data",
        "--mode", "experimental", "--layout", "system-breaks" }, error);
    REQUIRE(parsed.has_value());
    const auto *batch = std::get_if<BatchCommand>(&*parsed);
    REQUIRE(batch != nullptr);
    CHECK(batch->args.list_path == "pairs.tsv");
    CHECK(batch->args.base_dir == "data");
    CHECK(batch->args.jobs == 4);
    CHECK(batch->options.surface.mode == MetricMode::kExperimental);
    CHECK(batch->options.surface.layout == LayoutSurface::kSystemBreaks);

    error.clear();
    parsed = ParseCommand({ "--batch-jsonl", "validation.jsonl", "--pred-field", "pred",
        "--gt-field", "gold", "--mode", "experimental" }, error);
    REQUIRE(parsed.has_value());
    const auto *batch_jsonl = std::get_if<BatchJsonlCommand>(&*parsed);
    REQUIRE(batch_jsonl != nullptr);
    CHECK(batch_jsonl->args.jsonl_path == "validation.jsonl");
    CHECK(batch_jsonl->args.pred_field == "pred");
    CHECK(batch_jsonl->args.gt_field == "gold");
    CHECK(batch_jsonl->options.surface.mode == MetricMode::kExperimental);

    error.clear();
    parsed = ParseCommand({ "--mode", "experimental", "--layout", "system-breaks",
        "--batch-jsonl", "validation.jsonl" }, error);
    REQUIRE(parsed.has_value());
    batch_jsonl = std::get_if<BatchJsonlCommand>(&*parsed);
    REQUIRE(batch_jsonl != nullptr);
    CHECK(batch_jsonl->args.jsonl_path == "validation.jsonl");
    CHECK(batch_jsonl->options.surface.mode == MetricMode::kExperimental);
    CHECK(batch_jsonl->options.surface.layout == LayoutSurface::kSystemBreaks);

    error.clear();
    parsed = ParseCommand({ "--visualize", "pred.krn", "gt.krn", "--out", "report.html",
        "--mode", "experimental", "--layout", "system-breaks" }, error);
    REQUIRE(parsed.has_value());
    const auto *visual = std::get_if<VisualizeCommand>(&*parsed);
    REQUIRE(visual != nullptr);
    CHECK(visual->args.output_kind == VisualizeArgs::OutputKind::kHtml);
    CHECK(visual->options.surface.mode == MetricMode::kExperimental);
    CHECK(visual->options.surface.layout == LayoutSurface::kSystemBreaks);
}

TEST_CASE("ParseCommand accepts count-symbols mode placements", "[cli]")
{
    std::string error;
    auto parsed = ParseCommand(
        { "--count-symbols", "--mode", "experimental", "score.mei" }, error);
    REQUIRE(parsed.has_value());
    const auto *count = std::get_if<CountSymbolsCommand>(&*parsed);
    REQUIRE(count != nullptr);
    CHECK(count->input_kind == CountSymbolsCommand::InputKind::kFile);
    CHECK(count->path == "score.mei");
    CHECK(count->surface.mode == MetricMode::kExperimental);

    error.clear();
    parsed = ParseCommand(
        { "--count-symbols", "--per-measure", "score.krn", "--mode", "experimental",
            "--layout", "system-breaks" },
        error);
    REQUIRE(parsed.has_value());
    count = std::get_if<CountSymbolsCommand>(&*parsed);
    REQUIRE(count != nullptr);
    CHECK(count->per_measure);
    CHECK(count->path == "score.krn");
    CHECK(count->surface.mode == MetricMode::kExperimental);
    CHECK(count->surface.layout == LayoutSurface::kSystemBreaks);

    error.clear();
    parsed = ParseCommand({ "--count-symbols", "--files-from", "files.txt", "--base-dir",
        "data", "--mode", "active" }, error);
    REQUIRE(parsed.has_value());
    count = std::get_if<CountSymbolsCommand>(&*parsed);
    REQUIRE(count != nullptr);
    CHECK(count->input_kind == CountSymbolsCommand::InputKind::kFileList);
    CHECK(count->list_path == "files.txt");
    CHECK(count->base_dir == "data");
    CHECK(count->surface.mode == MetricMode::kActive);

    error.clear();
    parsed = ParseCommand({ "--mode", "experimental", "--layout", "system-breaks",
        "--count-symbols", "score.mei" }, error);
    REQUIRE(parsed.has_value());
    count = std::get_if<CountSymbolsCommand>(&*parsed);
    REQUIRE(count != nullptr);
    CHECK(count->input_kind == CountSymbolsCommand::InputKind::kFile);
    CHECK(count->path == "score.mei");
    CHECK(count->surface.mode == MetricMode::kExperimental);
    CHECK(count->surface.layout == LayoutSurface::kSystemBreaks);
}

TEST_CASE("ParseCommand rejects legacy detail and command-specific invalid options", "[cli]")
{
    std::string error;
    CHECK_FALSE(ParseCommand({ "pred.krn", "gt.krn", "--detail", "tierAB" }, error).has_value());
    CHECK(error.find("--detail has been removed") != std::string::npos);

    error.clear();
    CHECK_FALSE(ParseCommand({ "--check", "--mode", "experimental", "score.krn" }, error)
                    .has_value());
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(ParseCommand({ "--check", "--layout", "system-breaks", "score.krn" }, error)
                    .has_value());
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(ParseCommand({ "--dump-tree", "score.krn", "--mode", "experimental" }, error)
                    .has_value());
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(ParseCommand({ "--layout", "system-breaks", "--dump-tree", "score.krn" }, error)
                    .has_value());
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(ParseCommand({ "pred.krn", "gt.krn", "--layout", "page-breaks" }, error)
                    .has_value());
    CHECK(error.find("unknown layout surface") != std::string::npos);

    error.clear();
    CHECK_FALSE(ParseCommand({ "--count-symbols", "--ops", "score.krn" }, error).has_value());
    CHECK_FALSE(error.empty());

    error.clear();
    CHECK_FALSE(ParseCommand({ "--visualize", "pred.krn", "gt.krn", "--out", "report.html",
                    "--ops" }, error)
                    .has_value());
    CHECK_FALSE(error.empty());
}

TEST_CASE("ParseCommand preserves check and dump-tree typed-space handling", "[cli]")
{
    std::string error;
    auto parsed = ParseCommand(
        { "--check", "--typed-space-handling", "preserve", "score.krn" }, error);
    REQUIRE(parsed.has_value());
    const auto *check = std::get_if<CheckCommand>(&*parsed);
    REQUIRE(check != nullptr);
    CHECK(check->input_kind == CheckCommand::InputKind::kFile);
    CHECK(check->path == "score.krn");
    CHECK(check->typed_space_handling == TypedSpaceHandling::kPreserve);

    error.clear();
    parsed = ParseCommand(
        { "--typed-space-handling", "preserve", "--check", "score.krn" }, error);
    REQUIRE(parsed.has_value());
    check = std::get_if<CheckCommand>(&*parsed);
    REQUIRE(check != nullptr);
    CHECK(check->input_kind == CheckCommand::InputKind::kFile);
    CHECK(check->path == "score.krn");
    CHECK(check->typed_space_handling == TypedSpaceHandling::kPreserve);

    error.clear();
    parsed = ParseCommand(
        { "--check", "--files-from", "files.txt", "--base-dir", "data",
            "--typed-space-handling", "preserve" },
        error);
    REQUIRE(parsed.has_value());
    check = std::get_if<CheckCommand>(&*parsed);
    REQUIRE(check != nullptr);
    CHECK(check->input_kind == CheckCommand::InputKind::kFileList);
    CHECK(check->list_path == "files.txt");
    CHECK(check->base_dir == "data");
    CHECK(check->typed_space_handling == TypedSpaceHandling::kPreserve);

    error.clear();
    parsed = ParseCommand(
        { "--dump-tree", "score.krn", "--typed-space-handling", "preserve" }, error);
    REQUIRE(parsed.has_value());
    const auto *dump_tree = std::get_if<DumpTreeCommand>(&*parsed);
    REQUIRE(dump_tree != nullptr);
    CHECK(dump_tree->path == "score.krn");
    CHECK(dump_tree->typed_space_handling == TypedSpaceHandling::kPreserve);
}

TEST_CASE("ParsePairsArgs accepts optional base-dir only", "[cli]")
{
    const auto plain = ParsePairsArgs({ "--pairs", "pairs.tsv" });
    REQUIRE(plain.has_value());
    CHECK(plain->list_path == "pairs.tsv");
    CHECK(plain->base_dir.empty());

    const auto based = ParsePairsArgs({ "--pairs", "pairs.tsv", "--base-dir", "data" });
    REQUIRE(based.has_value());
    CHECK(based->list_path == "pairs.tsv");
    CHECK(based->base_dir == "data");

    CHECK_FALSE(ParsePairsArgs({ "--pairs", "pairs.tsv", "--jobs", "2" }).has_value());
}

TEST_CASE("ParseBatchArgs accepts base-dir and jobs", "[cli]")
{
    const auto parsed = ParseBatchArgs({ "--batch", "pairs.tsv", "--jobs", "4", "--base-dir", "data" });
    REQUIRE(parsed.has_value());
    CHECK(parsed->list_path == "pairs.tsv");
    CHECK(parsed->base_dir == "data");
    CHECK(parsed->jobs == 4);

    CHECK_FALSE(ParseBatchArgs({ "--batch", "pairs.tsv", "--jobs" }).has_value());
    CHECK_FALSE(ParseBatchArgs({ "--batch", "pairs.tsv", "--jobs", "abc" }).has_value());
    CHECK_FALSE(ParseBatchArgs({ "--batch", "pairs.tsv", "--jobs", "2x" }).has_value());
    CHECK_FALSE(ParseBatchArgs({ "--batch", "pairs.tsv", "--unknown", "x" }).has_value());
}

TEST_CASE("ParseBatchJsonlArgs accepts validation JSONL options", "[cli]")
{
    const auto defaults = ParseBatchJsonlArgs({ "--batch-jsonl", "validation.jsonl" });
    REQUIRE(defaults.has_value());
    CHECK(defaults->jsonl_path == "validation.jsonl");
    CHECK(defaults->pred_field == "prediction");
    CHECK(defaults->gt_field == "target");
    CHECK(defaults->id_field == "sample_id");
    CHECK(defaults->group_field == "val_set");
    CHECK(defaults->failure_score == 100.0);
    CHECK(defaults->jobs == 0);

    const auto parsed = ParseBatchJsonlArgs({ "--batch-jsonl", "validation.jsonl",
        "--pred-field", "pred", "--gt-field", "gold", "--id-field", "id", "--group-field",
        "split", "--dedupe-key", "split,id", "--summary-json", "summary.json",
        "--failure-score", "42.5", "--jobs", "3", "--format", "kern" });
    REQUIRE(parsed.has_value());
    CHECK(parsed->pred_field == "pred");
    CHECK(parsed->gt_field == "gold");
    CHECK(parsed->id_field == "id");
    CHECK(parsed->group_field == "split");
    CHECK(parsed->dedupe_key == "split,id");
    CHECK(parsed->summary_json == "summary.json");
    CHECK(parsed->failure_score == 42.5);
    CHECK(parsed->jobs == 3);

    CHECK_FALSE(ParseBatchJsonlArgs({ "--batch-jsonl", "validation.jsonl", "--jobs" }).has_value());
    CHECK_FALSE(
        ParseBatchJsonlArgs({ "--batch-jsonl", "validation.jsonl", "--failure-score", "nan" })
            .has_value());
    CHECK_FALSE(
        ParseBatchJsonlArgs({ "--batch-jsonl", "validation.jsonl", "--format", "musicxml" })
            .has_value());
}

TEST_CASE("ParseVisualizeArgs accepts the pair report form only", "[cli]")
{
    const auto parsed = ParseVisualizeArgs({ "--visualize", "pred.krn", "gt.krn", "--out", "report.html" });
    REQUIRE(parsed.has_value());
    CHECK(parsed->pred_path == "pred.krn");
    CHECK(parsed->gt_path == "gt.krn");
    CHECK(parsed->out_path == "report.html");
    CHECK(parsed->output_kind == VisualizeArgs::OutputKind::kHtml);

    CHECK_FALSE(ParseVisualizeArgs({ "--visualize", "pred.krn", "gt.krn" }).has_value());
    CHECK_FALSE(ParseVisualizeArgs({ "--visualize", "pred.krn", "gt.krn", "--html", "report.html" })
                    .has_value());
    CHECK_FALSE(ParseVisualizeArgs({ "--visualize", "--bad", "gt.krn", "--out", "report.html" })
                    .has_value());
}

TEST_CASE("ParseVisualizeArgs accepts SVG bundle output only with explicit format", "[cli]")
{
    const auto parsed = ParseVisualizeArgs(
        { "--visualize", "pred.krn", "gt.krn", "--out-dir", "bundle", "--output-format", "svg" });
    REQUIRE(parsed.has_value());
    CHECK(parsed->pred_path == "pred.krn");
    CHECK(parsed->gt_path == "gt.krn");
    CHECK(parsed->out_dir == "bundle");
    CHECK(parsed->output_format == "svg");
    CHECK(parsed->output_kind == VisualizeArgs::OutputKind::kSvgBundle);

    const auto reversed = ParseVisualizeArgs(
        { "--visualize", "pred.krn", "gt.krn", "--output-format", "svg", "--out-dir", "bundle" });
    REQUIRE(reversed.has_value());
    CHECK(reversed->output_kind == VisualizeArgs::OutputKind::kSvgBundle);

    CHECK_FALSE(ParseVisualizeArgs({ "--visualize", "pred.krn", "gt.krn", "--out-dir", "bundle" })
                    .has_value());
    CHECK_FALSE(ParseVisualizeArgs(
                    { "--visualize", "pred.krn", "gt.krn", "--out-dir", "bundle", "--output-format",
                        "html" })
                    .has_value());
    CHECK_FALSE(ParseVisualizeArgs(
                    { "--visualize", "pred.krn", "gt.krn", "--out", "report.html", "--out-dir",
                        "bundle" })
                    .has_value());
    CHECK_FALSE(ParseVisualizeArgs(
                    { "--visualize", "pred.krn", "gt.krn", "--out", "report.html", "--output-format",
                        "svg" })
                    .has_value());
}

TEST_CASE("List readers skip comments and preserve relative rows", "[cli]")
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path()
        / std::filesystem::path("verosim-list-test.txt");
    {
        std::ofstream out(dir);
        out << "# comment\n\npred.krn\tgt.krn\nmalformed\nnext.xml\tnext.krn\n";
    }

    std::ostringstream err;
    const std::vector<PairRow> pairs = ReadPairList(dir.string(), err);
    REQUIRE(pairs.size() == 2);
    CHECK(pairs[0].pred == "pred.krn");
    CHECK(pairs[0].gt == "gt.krn");
    CHECK(pairs[1].pred == "next.xml");
    CHECK(pairs[1].gt == "next.krn");
    CHECK(err.str().find("malformed pairs line") != std::string::npos);

    std::filesystem::remove(dir);
}

TEST_CASE("File list reader and base-dir joining match list-mode contracts", "[cli]")
{
    const std::filesystem::path path = std::filesystem::temp_directory_path()
        / std::filesystem::path("verosim-files-test.txt");
    {
        std::ofstream out(path);
        out << "# comment\nalpha.krn\n\nbeta.xml\n";
    }

    const std::vector<std::string> files = ReadFileList(path.string());
    CHECK(files == std::vector<std::string>{ "alpha.krn", "beta.xml" });
    CHECK(JoinBaseDir("", "alpha.krn") == "alpha.krn");
    CHECK(JoinBaseDir("data", "alpha.krn") == "data/alpha.krn");

    std::filesystem::remove(path);
}

TEST_CASE("Batch runner accepts jobs zero and preserves output order", "[cli]")
{
    const std::vector<PairRow> pairs{ { "first.krn", "first.gt.krn" }, { "second.krn", "second.gt.krn" } };
    std::ostringstream out;

    CompareBatchToJson(pairs, "missing-base", 0, CompareCliOptions{}, out);
    const std::string records = out.str();

    const auto first = records.find("missing-base/first.krn");
    const auto second = records.find("missing-base/second.krn");
    REQUIRE(first != std::string::npos);
    REQUIRE(second != std::string::npos);
    CHECK(first < second);
}

TEST_CASE("JSONL batch scores in-memory kern records and writes summary", "[cli]")
{
    const std::filesystem::path input = std::filesystem::temp_directory_path()
        / std::filesystem::path("verosim-validation-jsonl-test.jsonl");
    const std::filesystem::path summary = std::filesystem::temp_directory_path()
        / std::filesystem::path("verosim-validation-jsonl-summary.json");
    const std::string kern = "**kern\\n*clefG2\\n*M4/4\\n=1\\n4c\\n*-\\n";
    {
        std::ofstream out(input);
        out << "{\"val_set\":\"polish\",\"sample_id\":1,\"source\":\"example\",\"cer\":9,"
               "\"prediction\":\""
            << kern << "\",\"target\":\"" << kern << "\"}\n";
        out << "{\"val_set\":\"polish\",\"sample_id\":1,\"source\":\"duplicate\","
               "\"prediction\":\""
            << kern << "\",\"target\":\"**kern\\n4d\\n*-\\n\"}\n";
        out << "{\"val_set\":\"polish\",\"sample_id\":2,\"prediction\":\"" << kern << "\"}\n";
    }

    BatchJsonlArgs args;
    args.jsonl_path = input.string();
    args.summary_json = summary.string();
    args.dedupe_key = "val_set,sample_id";
    args.jobs = 1;

    std::ostringstream output;
    std::string error;
    REQUIRE(CompareJsonlBatchToJson(args, CompareCliOptions{}, output, error));
    CHECK(error.empty());

    std::istringstream lines(output.str());
    std::string line1, line2, line3;
    REQUIRE(std::getline(lines, line1));
    REQUIRE(std::getline(lines, line2));
    CHECK_FALSE(std::getline(lines, line3));

    jsonxx::Object first;
    REQUIRE(first.parse(line1));
    REQUIRE(first.has<jsonxx::Boolean>("ok"));
    CHECK(first.get<jsonxx::Boolean>("ok"));
    CHECK(first.get<jsonxx::Number>("id") == 1.0);
    CHECK(first.get<jsonxx::String>("group") == "polish");
    CHECK(first.get<jsonxx::String>("source") == "example");
    CHECK_FALSE(first.has<jsonxx::Number>("cer"));

    jsonxx::Object second;
    REQUIRE(second.parse(line2));
    REQUIRE(second.has<jsonxx::Boolean>("ok"));
    CHECK_FALSE(second.get<jsonxx::Boolean>("ok"));
    CHECK(second.get<jsonxx::String>("error").find("target") != std::string::npos);

    std::ifstream summary_in(summary);
    REQUIRE(summary_in);
    std::stringstream summary_text;
    summary_text << summary_in.rdbuf();
    jsonxx::Object summary_obj;
    REQUIRE(summary_obj.parse(summary_text.str()));
    const jsonxx::Object &overall = summary_obj.get<jsonxx::Object>("overall");
    CHECK(overall.get<jsonxx::Number>("count") == 2.0);
    CHECK(overall.get<jsonxx::Number>("scored") == 1.0);
    CHECK(overall.get<jsonxx::Number>("failures") == 1.0);
    CHECK(overall.get<jsonxx::Number>("omr_ned") == 50.0);

    std::filesystem::remove(input);
    std::filesystem::remove(summary);
}

TEST_CASE("JSONL batch reports malformed input records as data failures", "[cli]")
{
    const std::filesystem::path input = std::filesystem::temp_directory_path()
        / std::filesystem::path("verosim-validation-jsonl-failures.jsonl");
    {
        std::ofstream out(input);
        out << "{\"sample_id\":1,\"prediction\":\"**kern\\n4c\\n*-\\n\",\"target\":\"**kern\\n4c\\n*-\\n\"}\n";
        out << "{not json}\n";
    }

    BatchJsonlArgs args;
    args.jsonl_path = input.string();
    args.jobs = 1;

    std::ostringstream output;
    std::string error;
    REQUIRE(CompareJsonlBatchToJson(args, CompareCliOptions{}, output, error));

    CHECK(output.str().find("invalid JSON on line 2") != std::string::npos);
    std::istringstream lines(output.str());
    std::string line1, line2;
    REQUIRE(std::getline(lines, line1));
    REQUIRE(std::getline(lines, line2));
    jsonxx::Object malformed;
    REQUIRE(malformed.parse(line2));
    CHECK_FALSE(malformed.get<jsonxx::Boolean>("ok"));

    args.jsonl_path = (std::filesystem::temp_directory_path() / "missing-verosim-jsonl.jsonl").string();
    output.str("");
    output.clear();
    CHECK_FALSE(CompareJsonlBatchToJson(args, CompareCliOptions{}, output, error));
    CHECK(error.find("cannot read") != std::string::npos);

    std::filesystem::remove(input);
}
