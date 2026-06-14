#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <catch2/catch_test_macros.hpp>

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
