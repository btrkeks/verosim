#include <catch2/catch_test_macros.hpp>

#include "verosim/extraction/vrv_bridge.h"

TEST_CASE("test framework is wired up")
{
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("bridge loads minimal kern from memory")
{
    verosim::VrvBridge bridge;
    // '*' as first byte triggers Verovio's Humdrum autodetection
    REQUIRE(bridge.LoadData("**kern\n4c\n*-\n"));
    REQUIRE(bridge.GetDoc().GetChildCount() > 0);
}
