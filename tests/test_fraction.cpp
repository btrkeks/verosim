#include <catch2/catch_test_macros.hpp>

#include "verosim/model/duration.h"
#include "verosim/model/fraction.h"

using verosim::Fraction;

TEST_CASE("Fraction arithmetic is exact and normalized", "[fraction]")
{
    CHECK(Fraction(1, 2) + Fraction(1, 3) == Fraction(5, 6));
    CHECK(Fraction(1, 2) - Fraction(1, 3) == Fraction(1, 6));
    CHECK(Fraction(2, 3) * Fraction(3, 4) == Fraction(1, 2));
    CHECK(Fraction(1, 2) / Fraction(1, 4) == Fraction(2));
    CHECK(Fraction(4, 8) == Fraction(1, 2));
    CHECK(Fraction(-1, -2) == Fraction(1, 2));
    CHECK(Fraction(1, -2) == Fraction(-1, 2));
    CHECK(Fraction(0, 7) == Fraction(0));
}

TEST_CASE("Fraction ordering", "[fraction]")
{
    CHECK(Fraction(1, 3) < Fraction(1, 2));
    CHECK(Fraction(2, 3) > Fraction(1, 2));
    CHECK(Fraction(-1, 2) < Fraction(0));
    CHECK(Fraction(7, 7) <= Fraction(1));
}

TEST_CASE("Fraction string form matches Python Fraction str()", "[fraction]")
{
    CHECK(Fraction(2).str() == "2");
    CHECK(Fraction(1, 2).str() == "1/2");
    CHECK(Fraction(-3, 4).str() == "-3/4");
    CHECK(Fraction(0).str() == "0");
}

TEST_CASE("Fraction errors", "[fraction]")
{
    CHECK_THROWS(Fraction(1, 0));
    CHECK_THROWS(Fraction(1) / Fraction(0));
}

TEST_CASE("A typical offset accumulation stays exact", "[fraction]")
{
    // a triplet of eighths: each 1/2 * 2/3 QL
    const Fraction tripletEighth = Fraction(1, 2) * Fraction(2, 3);
    Fraction cursor(0);
    for (int i = 0; i < 3; ++i) cursor += tripletEighth;
    CHECK(cursor == Fraction(1));
}

TEST_CASE("Duration tables mirror music21", "[duration]")
{
    using namespace verosim;
    CHECK(TypeNumFromName("quarter") == Fraction(4));
    CHECK(TypeNumFromName("breve") == Fraction(1, 2));
    CHECK(QLFromTypeNum(Fraction(8)) == Fraction(1, 2));
    CHECK(ClosestTypeFromQL(Fraction(3)) == "half"); // m21 quarterLengthToClosestType(3.0)
    CHECK(ClosestTypeFromQL(Fraction(4)) == "whole");

    std::string type;
    int dots = 0;
    REQUIRE(QLToTypeDots(Fraction(3), type, dots)); // 3/4 full-measure rest
    CHECK(type == "half");
    CHECK(dots == 1);
    REQUIRE(QLToTypeDots(Fraction(4), type, dots)); // 4/4 full-measure rest
    CHECK(type == "whole");
    CHECK(dots == 0);
    REQUIRE(QLToTypeDots(Fraction(7, 2), type, dots)); // double-dotted half
    CHECK(type == "half");
    CHECK(dots == 2);
    CHECK_FALSE(QLToTypeDots(Fraction(7, 3), type, dots)); // complex

    CHECK(NoteHeadFromTypeNum(Fraction(16)) == Fraction(4));
    CHECK(NoteHeadFromTypeNum(Fraction(1, 2)) == Fraction(1, 2));
}
