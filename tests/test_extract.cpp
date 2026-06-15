// Field-level assertions on the Layer 2 extraction, against the HAND-10
// fixtures (committed in corpora/hand10, oracle-cross-checked — see the
// README there). test_hand10.cpp gates the totals; this file pins the
// individual fields: offsets, chord splitting, beams, tuplets, graces,
// extras, the measure drop rule, and the repr format.

#include <catch2/catch_test_macros.hpp>

#include "verosim/extraction/extract.h"
#include "verosim/extraction/vrv_bridge.h"

using namespace verosim;

namespace {

ExtractResult Extract(const std::string &name, SourceFormat format)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_HAND10_DIR) + "/" + name));
    return ExtractSymScore(bridge.GetDoc(), format);
}

ExtractResult ExtractFixture(const std::string &name, SourceFormat format)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_TEST_FIXTURE_DIR) + "/" + name));
    return ExtractSymScore(bridge.GetDoc(), format);
}

ExtractResult ExtractFixture(
    const std::string &name, SourceFormat format, const ExtractOptions &options)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_TEST_FIXTURE_DIR) + "/" + name));
    return ExtractSymScore(bridge.GetDoc(), format, options);
}

} // namespace

TEST_CASE("mono.krn: offsets, beams, dots", "[extract]")
{
    const ExtractResult result = Extract("mono.krn", SourceFormat::kKern);
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    const SymPart &part = result.score.parts[0];
    REQUIRE(part.bar_list.size() == 3);

    const SymMeasure &m1 = part.bar_list[0];
    REQUIRE(m1.notes.size() == 5);
    CHECK(m1.notes[0].pitches[0].step_octave == "C4");
    CHECK(m1.notes[0].note_offset == Fraction(0));
    CHECK(m1.notes[1].note_offset == Fraction(1));
    CHECK(m1.notes[2].note_offset == Fraction(2)); // first beamed eighth
    CHECK(m1.notes[2].beamings == std::vector<BeamValue>{ BeamValue::kStart });
    CHECK(m1.notes[3].beamings == std::vector<BeamValue>{ BeamValue::kStop });
    CHECK(m1.notes[3].note_offset == Fraction(5, 2));
    CHECK(m1.notes[4].note_offset == Fraction(3));
    // initial signature extras, sorted clef < keysig < timesig
    REQUIRE(m1.extras.size() == 3);
    CHECK(m1.extras[0].kind == ExtraKind::kClef);
    CHECK(m1.extras[0].symbolic == "G2");
    CHECK(m1.extras[1].kind == ExtraKind::kKeySig);
    CHECK(m1.extras[1].notation_size() == 1); // explicit *k[] = 1 symbol
    CHECK(m1.extras[2].kind == ExtraKind::kTimeSig);
    CHECK(m1.extras[2].notation_size() == 2);

    const SymMeasure &m2 = part.bar_list[1];
    REQUIRE(m2.notes.size() == 2);
    CHECK(m2.notes[1].dots == 1); // 2.a
    CHECK(m2.notes[1].note_dur_type == "half");
    CHECK(m2.notes[1].str() == "[A4]2*");
}

TEST_CASE("chords.krn: chord splitting and carrier fields", "[extract]")
{
    const ExtractResult result = Extract("chords.krn", SourceFormat::kKern);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    // 4c 4e 4g -> three SymNotes, diatonic ascending, sharing offset 0
    REQUIRE(m1.notes.size() >= 3);
    CHECK(m1.notes[0].pitches[0].step_octave == "C4");
    CHECK(m1.notes[1].pitches[0].step_octave == "E4");
    CHECK(m1.notes[2].pitches[0].step_octave == "G4");
    for (int i = 0; i < 3; ++i) {
        CHECK(m1.notes[i].is_in_chord);
        CHECK(m1.notes[i].note_idx_in_chord == i);
        CHECK(m1.notes[i].note_offset == Fraction(0));
        CHECK(m1.notes[i].vrv_id == m1.notes[0].vrv_id); // the chord's id
    }
    // 8.eL 16fJ: dotted-8th/16th beam pair
    const SymNote &e8 = m1.notes[6];
    CHECK(e8.pitches[0].step_octave == "E4");
    CHECK(e8.dots == 1);
    CHECK(e8.note_offset == Fraction(2));
    CHECK(e8.beamings == std::vector<BeamValue>{ BeamValue::kStart });
    const SymNote &f16 = m1.notes[7];
    CHECK(f16.note_offset == Fraction(11, 4));
    CHECK(f16.beamings == std::vector<BeamValue>{ BeamValue::kStop, BeamValue::kPartial });
}

TEST_CASE("tuplet.krn: tuplet types, info, offsets", "[extract]")
{
    const ExtractResult result = Extract("tuplet.krn", SourceFormat::kKern);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.notes.size() == 5);
    CHECK(m1.notes[0].tuplets == std::vector<TupletValue>{ TupletValue::kStart });
    CHECK(m1.notes[1].tuplets == std::vector<TupletValue>{ TupletValue::kContinue });
    CHECK(m1.notes[2].tuplets == std::vector<TupletValue>{ TupletValue::kStop });
    CHECK(m1.notes[0].tuplet_info == std::vector<std::string>{ "3" });
    CHECK(m1.notes[1].tuplet_info == std::vector<std::string>{ "" });
    // triplet eighths: offsets 0, 1/3, 2/3, then 1
    CHECK(m1.notes[1].note_offset == Fraction(1, 3));
    CHECK(m1.notes[2].note_offset == Fraction(2, 3));
    CHECK(m1.notes[3].note_offset == Fraction(1));
    CHECK(m1.notes[3].tuplets.empty());
    CHECK(m1.notes[0].str() == "[C4]4BsrTsr(3)");
}

TEST_CASE("grace.krn: kern grace conventions", "[extract]")
{
    const ExtractResult result = Extract("grace.krn", SourceFormat::kKern);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.notes.size() == 4);
    CHECK(m1.notes[0].grace_type == "acc"); // qq
    CHECK_FALSE(m1.notes[0].grace_slash);
    CHECK(m1.notes[0].note_is_grace);
    CHECK(m1.notes[0].note_offset == Fraction(0));
    CHECK(m1.notes[1].note_offset == Fraction(0)); // grace advances nothing
    CHECK(m1.notes[2].grace_type == "unacc"); // q
    CHECK(m1.notes[2].grace_slash);
    CHECK(m1.notes[2].note_offset == Fraction(1));
    // no *k token at all -> no keysig extra
    REQUIRE(m1.extras.size() == 2);
    CHECK(m1.extras[0].kind == ExtraKind::kClef);
    CHECK(m1.extras[1].kind == ExtraKind::kTimeSig);
}

TEST_CASE("changes.krn: mid-score signatures, mRest, hidden rest", "[extract]")
{
    const ExtractResult result = Extract("changes.krn", SourceFormat::kKern);
    const SymPart &part = result.score.parts[0];
    REQUIRE(part.bar_list.size() == 3);

    const SymMeasure &m1 = part.bar_list[0];
    // mid-measure clef change F4 at beat 2; extras sorted by (kind, offset)
    REQUIRE(m1.extras.size() == 4);
    CHECK(m1.extras[0].symbolic == "G2");
    CHECK(m1.extras[1].symbolic == "F4");
    CHECK(m1.extras[1].offset == Fraction(2));
    // explicit natural on 4c in 2-sharp key
    CHECK(m1.notes[0].pitches[0].accid == "natural");
    CHECK(m1.notes[1].pitches[0].accid == "None");
    CHECK(m1.notes[1].pitches[0].sounding_alter == 0); // d, not in key of 2 sharps

    const SymMeasure &m2 = part.bar_list[1];
    // keysig change to 1 flat + meter change to 3/4 at offset 0
    REQUIRE(m2.extras.size() == 2);
    CHECK(m2.extras[0].kind == ExtraKind::kKeySig);
    CHECK(m2.extras[0].notation_size() == 1);
    CHECK(m2.extras[1].kind == ExtraKind::kTimeSig);
    // full-measure rest in 3/4 -> dotted half
    REQUIRE(m2.notes.size() == 1);
    CHECK(m2.notes[0].pitches[0].step_octave == "R");
    CHECK(m2.notes[0].note_dur_type == "half");
    CHECK(m2.notes[0].dots == 1);

    const SymMeasure &m3 = part.bar_list[2];
    // hidden rest 4ryy: skipped, but advances the cursor
    REQUIRE(m3.notes.size() == 2);
    CHECK(m3.notes[0].note_offset == Fraction(1));
    CHECK(m3.notes[1].note_offset == Fraction(2));
}

TEST_CASE("voices.krn: layers flatten in order", "[extract]")
{
    const ExtractResult result = Extract("voices.krn", SourceFormat::kKern);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    // layer 1: two chords (3 notes each), then layer 2: three notes
    REQUIRE(m1.notes.size() == 9);
    CHECK(m1.notes[0].is_in_chord);
    CHECK(m1.notes[6].pitches[0].step_octave == "C5"); // first layer-2 note
    CHECK(m1.notes[6].note_offset == Fraction(0));
    CHECK(m1.notes[8].note_offset == Fraction(2));
}

TEST_CASE("grand.krn: one part per staff", "[extract]")
{
    const ExtractResult result = Extract("grand.krn", SourceFormat::kKern);
    REQUIRE(result.score.parts.size() == 2);
    // staff n=1 is the top (treble) staff = right kern spine
    CHECK(result.score.parts[0].staff_n == "1");
    CHECK(result.score.parts[0].part_idx == 0);
    CHECK(result.score.parts[0].bar_list[0].notes[0].pitches[0].step_octave == "C4");
    CHECK(result.score.parts[1].bar_list[0].notes[0].pitches[0].step_octave == "C3");
    CHECK(result.score.parts[1].bar_list[0].extras[0].symbolic == "F4");
}

TEST_CASE("keysig.krn: in-key accidentals are invisible but sound", "[extract]")
{
    const ExtractResult result = Extract("keysig.krn", SourceFormat::kKern);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    const SymNote &fsharp = m1.notes[3]; // 4f# under *k[f#]
    CHECK(fsharp.pitches[0].step_octave == "F4");
    CHECK(fsharp.pitches[0].accid == "None"); // governed by the key signature
    CHECK(fsharp.pitches[0].sounding_alter == 1); // resolver: keysig applies
}

TEST_CASE("tiny.xml: MusicXML side", "[extract]")
{
    const ExtractResult result = Extract("tiny.xml", SourceFormat::kMusicXml);
    REQUIRE(result.score.parts.size() == 1);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    // grace with slash="yes"
    CHECK(m1.notes[0].grace_type == "unacc");
    CHECK(m1.notes[0].grace_slash);
    // C#5 with <alter> but no <accidental>: invisible, but sounding
    CHECK(m1.notes[1].pitches[0].step_octave == "C5");
    CHECK(m1.notes[1].pitches[0].accid == "None");
    CHECK(m1.notes[1].pitches[0].sounding_alter == 1);
    // explicit natural
    CHECK(m1.notes[3].pitches[0].accid == "natural");
    CHECK(m1.notes[3].pitches[0].sounding_alter == 0);
    // triplet of eighths in 3/4 starting at beat 2
    CHECK(m1.notes[4].note_offset == Fraction(2));
    CHECK(m1.notes[5].note_offset == Fraction(7, 3));
    CHECK(m1.notes[5].pitches[0].sounding_alter == 1); // F# from the key signature

    const SymMeasure &m2 = result.score.parts[0].bar_list[1];
    // chord members carry the dot each; carried C# stays invisible
    CHECK(m2.notes[0].dots == 1);
    CHECK(m2.notes[0].pitches[0].accid == "sharp");
    CHECK(m2.notes[2].pitches[0].accid == "None");
    CHECK(m2.notes[2].pitches[0].sounding_alter == 1); // within-measure carry
}

TEST_CASE("tie_chain.krn: sounding alter carries across barlines via ties", "[extract]")
{
    // <tie> control elements live in the measure where the tie STARTS, with
    // @endid pointing into the next measure; the middle note of the chain is
    // both a tie target and a new tie start. music21 reference (pinned
    // 2026-06-12): alter +1 on every link, visible accidental on the first only.
    const ExtractResult result = ExtractFixture("tie_chain.krn", SourceFormat::kKern);
    CHECK(result.warnings.empty());
    const SymPart &part = result.score.parts[0];
    REQUIRE(part.bar_list.size() == 3);
    const SymNote &start = part.bar_list[0].notes[1]; // [4f#
    CHECK(start.pitches[0].accid == "sharp");
    CHECK(start.pitches[0].sounding_alter == 1);
    const SymNote &middle = part.bar_list[1].notes[0]; // 4f#_
    CHECK(middle.pitches[0].accid == "None");
    CHECK(middle.pitches[0].sounding_alter == 1);
    const SymNote &end = part.bar_list[2].notes[0]; // 4f#]
    CHECK(end.pitches[0].accid == "None");
    CHECK(end.pitches[0].sounding_alter == 1);
}

TEST_CASE("inline_meter.mei: layer meterSig governs a following mRest", "[extract]")
{
    // a meterSig inline in a layer (MEI input only; both importers emit
    // scoreDef changes instead) must update the governing measure length
    const ExtractResult result = ExtractFixture("inline_meter.mei", SourceFormat::kOther);
    CHECK(result.warnings.empty());
    const SymPart &part = result.score.parts[0];
    REQUIRE(part.bar_list.size() == 2);
    REQUIRE(part.bar_list[0].extras.size() == 3); // clef, 4/4, inline 3/4
    CHECK(part.bar_list[0].extras[2].kind == ExtraKind::kTimeSig);
    CHECK(part.bar_list[0].extras[2].offset == Fraction(2));
    const SymNote &mrest = part.bar_list[1].notes[0];
    CHECK(mrest.pitches[0].step_octave == "R");
    CHECK(mrest.note_dur_type == "half"); // 3/4 full-measure rest = dotted half
    CHECK(mrest.dots == 1);
}

TEST_CASE("repair_space_beamspan.mei: repair spaces and beamSpan controls", "[extract]")
{
    const ExtractResult result = ExtractFixture("repair_space_beamspan.mei", SourceFormat::kOther);
    CHECK(result.warnings.empty());
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.notes.size() == 5);

    // Verovio uses typed spaces such as type=straddle/type=filler to repair
    // malformed rhythm for layout. They are hidden and must not move note
    // offsets, unlike normal hidden space durations.
    CHECK(m1.notes[0].note_offset == Fraction(0));
    CHECK(m1.notes[1].note_offset == Fraction(1, 4));
    CHECK(m1.notes[2].note_offset == Fraction(1, 2));
    CHECK(m1.notes[3].note_offset == Fraction(3, 4));
    CHECK(m1.notes[4].note_offset == Fraction(2));

    CHECK(m1.notes[0].beamings == std::vector<BeamValue>{ BeamValue::kStart, BeamValue::kStart });
    CHECK(m1.notes[1].beamings == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kContinue });
    CHECK(m1.notes[2].beamings == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kContinue });
    CHECK(m1.notes[3].beamings == std::vector<BeamValue>{ BeamValue::kStop, BeamValue::kStop });
    CHECK(m1.notes[4].beamings.empty());
}

TEST_CASE("VrvBridge normalizes Verovio rhythm-repair spaces after import", "[extract]")
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_TEST_FIXTURE_DIR) + "/repair_space_beamspan.mei"));

    CHECK(bridge.last_normalized_rhythm_repair_spaces() == 1);
    CHECK(bridge.GetDoc().FindAllDescendantsByType(vrv::SPACE).size() == 2);
}

TEST_CASE("cross_measure_beamspan.mei: beamSpan continues across barlines", "[extract]")
{
    const ExtractResult result = ExtractFixture("cross_measure_beamspan.mei", SourceFormat::kOther);
    CHECK(result.warnings.empty());
    const SymPart &part = result.score.parts[0];
    REQUIRE(part.bar_list.size() == 2);
    REQUIRE(part.bar_list[0].notes.size() == 2);
    REQUIRE(part.bar_list[1].notes.size() == 2);

    CHECK(part.bar_list[0].notes[0].beamings == std::vector<BeamValue>{ BeamValue::kStart });
    CHECK(part.bar_list[0].notes[1].beamings == std::vector<BeamValue>{ BeamValue::kContinue });
    // The raw span is resolved across both measures; the musicdiff-style
    // per-measure enhancement then rewrites the first local continuation to a
    // local start rather than dropping the beam entirely.
    CHECK(part.bar_list[1].notes[0].beamings == std::vector<BeamValue>{ BeamValue::kStart });
    CHECK(part.bar_list[1].notes[1].beamings == std::vector<BeamValue>{ BeamValue::kStop });
}

TEST_CASE("single_number.xml: single-number timesig counts one symbol", "[extract]")
{
    // <time symbol="single-number"> -> @form="num" -> numerator-only
    // infodict; cross-checked against count_oracle.py (total 9 == 9)
    const ExtractResult result = ExtractFixture("single_number.xml", SourceFormat::kMusicXml);
    CHECK(result.warnings.empty());
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.extras.size() == 3);
    const SymExtra &timesig = m1.extras[2];
    CHECK(timesig.kind == ExtraKind::kTimeSig);
    CHECK(timesig.notation_size() == 1);
    CHECK(result.score.notation_size() == 9);
}

TEST_CASE("mei_controls.mei: timestamp controls use active meter and staff resolution", "[extract]")
{
    const ExtractResult result = ExtractFixture(
        "mei_controls.mei", SourceFormat::kOther, ExtractOptions{ .detail = DetailTier::kTierABDir });
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 2);
    REQUIRE(result.score.parts[0].bar_list.size() == 2);
    REQUIRE(result.score.parts[1].bar_list.size() == 2);

    const auto find_extra = [](const SymMeasure &measure, const std::string &id) -> const SymExtra * {
        for (const SymExtra &extra : measure.extras) {
            if (extra.vrv_id == id) return &extra;
        }
        return nullptr;
    };

    const SymMeasure &staff1_m1 = result.score.parts[0].bar_list[0];
    const SymMeasure &staff2_m1 = result.score.parts[1].bar_list[0];

    const SymExtra *unstaffed = find_extra(staff1_m1, "dyn_unstaffed");
    REQUIRE(unstaffed != nullptr);
    CHECK(unstaffed->kind == ExtraKind::kDynamic);
    CHECK(unstaffed->offset == Fraction(2)); // 6/8: (tstamp 5 - 1) * 1/2
    CHECK(find_extra(staff2_m1, "dyn_unstaffed") == nullptr);

    const SymExtra *staff2_dynamic = find_extra(staff2_m1, "dyn_staff2");
    REQUIRE(staff2_dynamic != nullptr);
    CHECK(staff2_dynamic->kind == ExtraKind::kDynamic);
    CHECK(staff2_dynamic->offset == Fraction(1)); // (tstamp 3 - 1) * 1/2
    CHECK(find_extra(staff1_m1, "dyn_staff2") == nullptr);

    const SymExtra *hairpin = find_extra(staff1_m1, "hp_tstamp2");
    REQUIRE(hairpin != nullptr);
    CHECK(hairpin->kind == ExtraKind::kCrescendo);
    CHECK(hairpin->offset == Fraction(1, 2));
    REQUIRE(hairpin->duration.has_value());
    CHECK(*hairpin->duration == Fraction(3, 2)); // tstamp2 0m+5 in 6/8 ends at QL 2

    const SymExtra *startid_slur = find_extra(staff2_m1, "slur_startid_staff2");
    REQUIRE(startid_slur != nullptr);
    CHECK(startid_slur->kind == ExtraKind::kSlur);
    CHECK(startid_slur->offset == Fraction(0));
    REQUIRE(startid_slur->duration.has_value());
    CHECK(*startid_slur->duration == Fraction(2));
    CHECK(find_extra(staff1_m1, "slur_startid_staff2") == nullptr);

    const SymExtra *cross_slur = find_extra(staff1_m1, "slur_cross_measure");
    REQUIRE(cross_slur != nullptr);
    CHECK(cross_slur->offset == Fraction(1));
    REQUIRE(cross_slur->duration.has_value());
    CHECK(*cross_slur->duration == Fraction(4));
}
