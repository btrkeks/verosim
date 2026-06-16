#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>
#include <jsonxx.h>

#include "verosim/cli/args.h"
#include "verosim/cli/batch.h"
#include "verosim/cli/list_io.h"
#include "verosim/model/detail_tier.h"

using namespace verosim;

TEST_CASE("DetailTier helpers map the public tier names", "[cli]")
{
    REQUIRE(ParseDetailTier("tierA") == DetailTier::kTierA);
    REQUIRE(ParseDetailTier("tierAB") == DetailTier::kTierAB);
    REQUIRE(ParseDetailTier("tierAB_dir") == DetailTier::kTierABDir);
    CHECK_FALSE(ParseDetailTier("allobjects").has_value());

    CHECK(DetailTierName(DetailTier::kTierA) == "tierA");
    CHECK_FALSE(DetailIncludesTierB(DetailTier::kTierA));
    CHECK_FALSE(DetailIncludesDirections(DetailTier::kTierA));
    CHECK(DetailIncludesTierB(DetailTier::kTierAB));
    CHECK_FALSE(DetailIncludesDirections(DetailTier::kTierAB));
    CHECK(DetailIncludesTierB(DetailTier::kTierABDir));
    CHECK(DetailIncludesDirections(DetailTier::kTierABDir));
}

TEST_CASE("StripCompareOptions parses detail and ops without changing dispatch args", "[cli]")
{
    CompareCliOptions options;
    std::vector<std::string> args{ "pred.krn", "gt.krn", "--ops", "--detail", "tierAB_dir" };
    std::string error;

    REQUIRE(StripCompareOptions(args, options, error));
    CHECK(options.emit_ops);
    CHECK(options.detail == DetailTier::kTierABDir);
    CHECK(args == std::vector<std::string>{ "pred.krn", "gt.krn" });
}

TEST_CASE("Compare detail defaults to tierAB and rejects unknown tiers", "[cli]")
{
    CompareCliOptions options;
    CHECK(options.detail == DetailTier::kTierAB);

    std::vector<std::string> args{ "pred.krn", "gt.krn", "--detail", "bogus" };
    std::string error;
    CHECK_FALSE(StripCompareOptions(args, options, error));
    CHECK_FALSE(error.empty());
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
