#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "verosim/cli/check.h"
#include "verosim/extraction/vrv_bridge.h"

namespace {

const std::string kFixtureDir = VEROSIM_TEST_FIXTURE_DIR;

verosim::VrvBridge MakeCheckBridge()
{
    return verosim::VrvBridge({ .log_level = vrv::LOG_WARNING, .capture_log = true });
}

} // namespace

TEST_CASE("CheckFile loads a clean kern file with no warnings")
{
    verosim::VrvBridge bridge = MakeCheckBridge();
    const verosim::CheckResult result = verosim::CheckFile(bridge, kFixtureDir + "/tiny.krn");
    CHECK(result.ok);
    CHECK_FALSE(result.exception);
    CHECK(result.warnings.empty());
    CHECK(result.errors.empty());
}

TEST_CASE("CheckFile captures warnings from a loadable-but-odd kern file")
{
    verosim::VrvBridge bridge = MakeCheckBridge();
    const verosim::CheckResult result = verosim::CheckFile(bridge, kFixtureDir + "/warn.krn");
    CHECK(result.ok);
    REQUIRE_FALSE(result.warnings.empty());
    CHECK(result.warnings.front().find("bogusUnknownOption") != std::string::npos);
}

TEST_CASE("CheckFile reports a missing file as not ok")
{
    verosim::VrvBridge bridge = MakeCheckBridge();
    const verosim::CheckResult result
        = verosim::CheckFile(bridge, kFixtureDir + "/does_not_exist.krn");
    CHECK_FALSE(result.ok);
}

TEST_CASE("warnings do not leak across sequential CheckFile calls on one bridge")
{
    verosim::VrvBridge bridge = MakeCheckBridge();
    const verosim::CheckResult warned = verosim::CheckFile(bridge, kFixtureDir + "/warn.krn");
    REQUIRE_FALSE(warned.warnings.empty());
    const verosim::CheckResult clean = verosim::CheckFile(bridge, kFixtureDir + "/tiny.krn");
    CHECK(clean.ok);
    CHECK(clean.warnings.empty());
}

TEST_CASE("WriteCheckJsonl escapes JSON special characters")
{
    verosim::CheckResult result;
    result.path = "a\"b\\c.krn";
    result.ok = true;
    result.warnings = { "line1\nline2\ttab" };
    std::ostringstream os;
    verosim::WriteCheckJsonl(result, os);
    const std::string line = os.str();
    CHECK(line.find(R"("path":"a\"b\\c.krn")") != std::string::npos);
    CHECK(line.find(R"(line1\nline2\ttab)") != std::string::npos);
    CHECK(line.find("\"n_warnings\":1") != std::string::npos);
    CHECK(line.back() == '\n');
}
