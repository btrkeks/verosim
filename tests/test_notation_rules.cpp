// Exact ports of musicdiff's beam/tuplet post-processing, tested against
// outputs transcribed from running the vendored Python functions
// (get_enhance_beamings / get_tuplets_type, m21utils.py) on the same inputs.

#include <catch2/catch_test_macros.hpp>

#include "verosim/extraction/notation_rules.h"

using namespace verosim;

namespace {

constexpr BeamValue SR = BeamValue::kStart;
constexpr BeamValue CO = BeamValue::kContinue;
constexpr BeamValue SP = BeamValue::kStop;
constexpr BeamValue PA = BeamValue::kPartial;

RawEvent Ev(std::vector<BeamValue> beams, int typeNum, bool rest = false)
{
    RawEvent e;
    e.is_rest = rest;
    e.type_num = Fraction(typeNum);
    e.raw_beams = std::move(beams);
    return e;
}

using BeamLists = std::vector<std::vector<BeamValue>>;

} // namespace

TEST_CASE("EnhanceBeamings matches vendored get_enhance_beamings", "[notation_rules]")
{
    // expectations generated from the vendored Python on 2026-06-12
    SECTION("lone 8th gets a flag") { CHECK(EnhanceBeamings({ Ev({}, 8) }) == BeamLists{ { PA } }); }
    SECTION("beamed pair unchanged")
    {
        CHECK(EnhanceBeamings({ Ev({ SR }, 8), Ev({ SP }, 8) }) == BeamLists{ { SR }, { SP } });
    }
    SECTION("lone start becomes partial")
    {
        CHECK(EnhanceBeamings({ Ev({ SR }, 8), Ev({}, 4) }) == BeamLists{ { PA }, {} });
    }
    SECTION("lone stop becomes partial")
    {
        CHECK(EnhanceBeamings({ Ev({}, 4), Ev({ SP }, 8) }) == BeamLists{ {}, { PA } });
    }
    SECTION("leading continue becomes start")
    {
        CHECK(EnhanceBeamings({ Ev({ CO }, 8), Ev({ SP }, 8) }) == BeamLists{ { SR }, { SP } });
    }
    SECTION("isolated continue becomes partial")
    {
        CHECK(EnhanceBeamings({ Ev({}, 4), Ev({ CO }, 8), Ev({}, 4) })
            == BeamLists{ {}, { PA }, {} });
    }
    SECTION("dotted 8th + 16th pair")
    {
        CHECK(EnhanceBeamings({ Ev({ SR }, 8), Ev({ SP, PA }, 16) })
            == BeamLists{ { SR }, { SP, PA } });
    }
    SECTION("beamed rest between notes continues the beam")
    {
        CHECK(EnhanceBeamings({ Ev({ SR }, 8), Ev({}, 8, true), Ev({ SP }, 8) })
            == BeamLists{ { SR }, { CO }, { SP } });
    }
    SECTION("16th rest between beamed 16ths continues at both depths")
    {
        CHECK(EnhanceBeamings({ Ev({ SR, SR }, 16), Ev({}, 16, true), Ev({ SP, SP }, 16) })
            == BeamLists{ { SR, SR }, { CO, CO }, { SP, SP } });
    }
    SECTION("trailing 8th rest gets a partial flag")
    {
        CHECK(EnhanceBeamings({ Ev({ SR }, 8), Ev({ SP }, 8), Ev({}, 8, true) })
            == BeamLists{ { SR }, { SP }, { PA } });
    }
    SECTION("16th-8th-16th hooks survive")
    {
        CHECK(EnhanceBeamings({ Ev({ SR, PA }, 16), Ev({ CO }, 8), Ev({ SP, PA }, 16) })
            == BeamLists{ { SR, PA }, { CO }, { SP, PA } });
    }
}

namespace {

constexpr TupletValue TSR = TupletValue::kStart;
constexpr TupletValue TCO = TupletValue::kContinue;
constexpr TupletValue TSP = TupletValue::kStop;
constexpr TupletValue TSS = TupletValue::kStartStop;

RawEvent Tup(std::vector<std::optional<TupletValue>> raw)
{
    RawEvent e;
    e.type_num = Fraction(8);
    e.raw_tuplets = std::move(raw);
    e.tuplet_nums.assign(e.raw_tuplets.size(), "3");
    return e;
}

using TupletLists = std::vector<std::vector<TupletValue>>;

} // namespace

TEST_CASE("CorrectTuplets matches vendored get_tuplets_type", "[notation_rules]")
{
    SECTION("triplet with None middle")
    {
        CHECK(CorrectTuplets({ Tup({ TSR }), Tup({ std::nullopt }), Tup({ TSP }) })
            == TupletLists{ { TSR }, { TCO }, { TSP } });
    }
    SECTION("missing start is synthesized")
    {
        CHECK(CorrectTuplets({ Tup({ std::nullopt }), Tup({ std::nullopt }), Tup({ TSP }) })
            == TupletLists{ { TSR }, { TCO }, { TSP } });
    }
    SECTION("nested tuplets")
    {
        CHECK(CorrectTuplets({ Tup({ TSR, TSR }), Tup({ std::nullopt, std::nullopt }),
                  Tup({ std::nullopt, TSP }), Tup({ TSP }) })
            == TupletLists{ { TSR, TSR }, { TCO, TCO }, { TCO, TSP }, { TSP } });
    }
    SECTION("startStop resets the open state")
    {
        CHECK(CorrectTuplets({ Tup({ TSS }), Tup({ TSR }), Tup({ TSP }) })
            == TupletLists{ { TSS }, { TSR }, { TSP } });
    }
    SECTION("two triplets in a row")
    {
        CHECK(CorrectTuplets({ Tup({ TSR }), Tup({ std::nullopt }), Tup({ TSP }), Tup({ TSR }),
                  Tup({ std::nullopt }), Tup({ TSP }) })
            == TupletLists{ { TSR }, { TCO }, { TSP }, { TSR }, { TCO }, { TSP } });
    }
}

TEST_CASE("TupletInfo puts the number on raw starts only", "[notation_rules]")
{
    const auto info = TupletInfo({ Tup({ TSR }), Tup({ std::nullopt }), Tup({ TSP }) });
    CHECK(info == std::vector<std::vector<std::string>>{ { "3" }, { "" }, { "" } });
}

TEST_CASE("DeriveBeamTypes produces music21-equivalent raw beams", "[notation_rules]")
{
    const auto member = [](int nBeams, bool rest = false, int breaksec = 0) {
        BeamMember m;
        m.is_rest = rest;
        m.n_beams = nBeams;
        m.breaksec = breaksec;
        return m;
    };

    SECTION("two eighths")
    {
        CHECK(DeriveBeamTypes({ member(1), member(1) }) == BeamLists{ { SR }, { SP } });
    }
    SECTION("dotted 8th + 16th: hook on the 16th")
    {
        CHECK(DeriveBeamTypes({ member(1), member(2) }) == BeamLists{ { SR }, { SP, PA } });
    }
    SECTION("four 16ths")
    {
        CHECK(DeriveBeamTypes({ member(2), member(2), member(2), member(2) })
            == BeamLists{ { SR, SR }, { CO, CO }, { CO, CO }, { SP, SP } });
    }
    SECTION("rest inside the beam is transparent")
    {
        CHECK(DeriveBeamTypes({ member(1), member(0, true), member(1) })
            == BeamLists{ { SR }, {}, { SP } });
    }
    SECTION("breaksec=1 splits the 16th beam, not the 8th beam")
    {
        CHECK(DeriveBeamTypes({ member(2), member(2, false, 1), member(2), member(2) })
            == BeamLists{ { SR, SR }, { CO, SP }, { CO, SR }, { SP, SP } });
    }
    SECTION("16th between eighths gets a hook")
    {
        CHECK(DeriveBeamTypes({ member(1), member(2), member(1) })
            == BeamLists{ { SR }, { CO, PA }, { SP } });
    }
}
