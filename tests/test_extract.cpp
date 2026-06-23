// Field-level assertions on the Layer 2 extraction, against the HAND-10
// fixtures (committed in corpora/hand10, oracle-cross-checked — see the
// README there). test_hand10.cpp gates the totals; this file pins the
// individual fields: offsets, chord splitting, beams, tuplets, graces,
// extras, the measure drop rule, and the repr format.

#include <algorithm>

#include <catch2/catch_test_macros.hpp>

#include "durationinterface.h"
#include "verosim/engine/interner.h"
#include "verosim/extraction/extract.h"
#include "verosim/extraction/typed_space_policy.h"
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
    VrvBridgeConfig config;
    config.typed_space_handling = options.typed_space_handling;
    VrvBridge bridge(config);
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_TEST_FIXTURE_DIR) + "/" + name));
    return ExtractSymScore(bridge.GetDoc(), format, options);
}

ExtractResult ExtractData(
    const std::string &data, vrv::FileFormat inputFormat, SourceFormat sourceFormat)
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreData(data, inputFormat));
    return ExtractSymScore(bridge.GetDoc(), sourceFormat);
}

ExtractResult ExtractData(const std::string &data, vrv::FileFormat inputFormat,
    SourceFormat sourceFormat, const ExtractOptions &options)
{
    VrvBridgeConfig config;
    config.typed_space_handling = options.typed_space_handling;
    VrvBridge bridge(config);
    REQUIRE(bridge.LoadScoreData(data, inputFormat));
    return ExtractSymScore(bridge.GetDoc(), sourceFormat, options);
}

ExtractResult ExtractKernData(const std::string &data)
{
    return ExtractData(data, vrv::HUMDRUM, SourceFormat::kKern);
}

ExtractResult ExtractMeiData(const std::string &data)
{
    return ExtractData(data, vrv::MEI, SourceFormat::kOther);
}

ExtractResult ExtractMeiData(const std::string &data, const ExtractOptions &options)
{
    return ExtractData(data, vrv::MEI, SourceFormat::kOther, options);
}

std::vector<const SymExtra *> ExtrasOfKind(const SymMeasure &measure, ExtraKind kind)
{
    std::vector<const SymExtra *> found;
    for (const SymExtra &extra : measure.extras) {
        if (extra.kind == kind) found.push_back(&extra);
    }
    return found;
}

const vrv::Object *FindStraddleOrFillerSpace(const vrv::Doc &doc)
{
    for (const vrv::Object *obj : doc.FindAllDescendantsByType(vrv::SPACE)) {
        if (IsStraddleOrFillerSpace(obj)) return obj;
    }
    return nullptr;
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
    CHECK(m1.locator.part_idx == 0);
    CHECK(m1.locator.staff_n == "1");
    CHECK(m1.locator.measure_idx == 0);
    CHECK(m1.locator.measure_vrv_id == m1.vrv_id);
    REQUIRE(m1.notes.size() == 5);
    CHECK(m1.notes[0].pitches[0].step_octave == "C4");
    CHECK(m1.notes[0].note_offset == Fraction(0));
    CHECK(m1.notes[0].locator.part_idx == 0);
    CHECK(m1.notes[0].locator.staff_n == "1");
    CHECK(m1.notes[0].locator.measure_idx == 0);
    CHECK(m1.notes[0].locator.measure_vrv_id == m1.vrv_id);
    CHECK(m1.notes[0].locator.offset == Fraction(0));
    CHECK(m1.notes[0].locator.occurrence == 0);
    CHECK(m1.notes[1].note_offset == Fraction(1));
    CHECK(m1.notes[2].note_offset == Fraction(2)); // first beamed eighth
    CHECK(m1.notes[2].beamings == std::vector<BeamValue>{ BeamValue::kStart });
    CHECK(m1.notes[3].beamings == std::vector<BeamValue>{ BeamValue::kStop });
    CHECK(m1.notes[3].note_offset == Fraction(5, 2));
    CHECK(m1.notes[3].locator.offset == Fraction(5, 2));
    CHECK(m1.notes[3].locator.occurrence == 3);
    CHECK(m1.notes[4].note_offset == Fraction(3));
    // initial signature extras, sorted clef < keysig < timesig
    REQUIRE(m1.extras.size() == 3);
    CHECK(m1.extras[0].kind == ExtraKind::kClef);
    CHECK(m1.extras[0].symbolic == "G2");
    CHECK(m1.extras[0].locator.part_idx == 0);
    CHECK(m1.extras[0].locator.staff_n == "1");
    CHECK(m1.extras[0].locator.measure_idx == 0);
    CHECK(m1.extras[0].locator.measure_vrv_id == m1.vrv_id);
    CHECK(m1.extras[0].locator.offset == Fraction(0));
    CHECK(m1.extras[0].locator.occurrence == 0);
    CHECK(m1.extras[1].kind == ExtraKind::kKeySig);
    CHECK(m1.extras[1].notation_size() == 1); // explicit *k[] = 1 symbol
    CHECK(m1.extras[2].kind == ExtraKind::kTimeSig);
    CHECK(m1.extras[2].notation_size() == 2);
    CHECK(m1.extras[2].locator.part_idx == 0);
    CHECK(m1.extras[2].locator.staff_n == "1");
    CHECK(m1.extras[2].locator.measure_idx == 0);
    CHECK(m1.extras[2].locator.offset == Fraction(0));
    CHECK(m1.extras[2].locator.occurrence == 0);

    SymNote relocated_note = m1.notes[0];
    const int note_size = relocated_note.notation_size();
    const std::string note_str = relocated_note.str();
    const std::string note_repr = relocated_note.repr();
    relocated_note.locator = SymbolLocator{ .part_idx = 7,
        .staff_n = "99",
        .measure_idx = 23,
        .measure_vrv_id = "elsewhere",
        .measure_n = "z",
        .offset = Fraction(42),
        .occurrence = 19 };
    CHECK(relocated_note.notation_size() == note_size);
    CHECK(relocated_note.str() == note_str);
    CHECK(relocated_note.repr() == note_repr);

    SymExtra relocated_extra = m1.extras[2];
    const int extra_size = relocated_extra.notation_size();
    const std::string extra_str = relocated_extra.str();
    const std::string extra_key = ExtraComparisonKey(relocated_extra);
    relocated_extra.locator = relocated_note.locator;
    CHECK(relocated_extra.notation_size() == extra_size);
    CHECK(relocated_extra.str() == extra_str);
    CHECK(ExtraComparisonKey(relocated_extra) == extra_key);

    SymMeasure relocated_measure = m1;
    const int measure_size = relocated_measure.notation_size();
    relocated_measure.locator = relocated_note.locator;
    CHECK(relocated_measure.notation_size() == measure_size);

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
        CHECK_FALSE(m1.notes[i].visual_id.empty());
        CHECK(m1.notes[i].visual_id != m1.notes[i].vrv_id);
        CHECK(m1.notes[i].locator.part_idx == 0);
        CHECK(m1.notes[i].locator.staff_n == "1");
        CHECK(m1.notes[i].locator.measure_idx == 0);
        CHECK(m1.notes[i].locator.occurrence == i);
    }
    CHECK(m1.notes[0].visual_id != m1.notes[1].visual_id);
    CHECK(m1.notes[1].visual_id != m1.notes[2].visual_id);
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
    CHECK(m2.locator.measure_idx == 1);
    CHECK(m2.locator.measure_vrv_id == m2.vrv_id);
    // keysig change to 1 flat + meter change to 3/4 at offset 0
    REQUIRE(m2.extras.size() == 2);
    CHECK(m2.extras[0].kind == ExtraKind::kKeySig);
    CHECK(m2.extras[0].notation_size() == 1);
    CHECK(m2.extras[1].kind == ExtraKind::kTimeSig);
    CHECK(m2.extras[1].locator.measure_idx == 1);
    CHECK(m2.extras[1].locator.measure_vrv_id == m2.vrv_id);
    CHECK(m2.extras[1].locator.offset == Fraction(0));
    CHECK(m2.extras[1].locator.occurrence == 0);
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

TEST_CASE("system breaks are an opt-in layout surface", "[extract]")
{
    const char *mei = R"mei(<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2" meter.count="4" meter.unit="4"/>
              <staffDef n="2" lines="5" clef.shape="F" clef.line="4" meter.count="4" meter.unit="4"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1" xml:id="m1">
              <staff n="1"><layer n="1"><note dur="1" oct="4" pname="c"/></layer></staff>
              <staff n="2"><layer n="1"><note dur="1" oct="3" pname="c"/></layer></staff>
            </measure>
            <sb xml:id="sb_before_m2"/>
            <measure n="2" xml:id="m2">
              <staff n="1"><layer n="1"><note dur="1" oct="4" pname="d"/></layer></staff>
              <staff n="2"><layer n="1"><note dur="1" oct="3" pname="d"/></layer></staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>)mei";

    const ExtractResult default_result = ExtractMeiData(mei);
    REQUIRE(default_result.score.parts.size() == 2);
    REQUIRE(default_result.score.parts[0].bar_list.size() == 2);
    CHECK(ExtrasOfKind(default_result.score.parts[0].bar_list[1], ExtraKind::kSystemBreak).empty());
    CHECK(ExtrasOfKind(default_result.score.parts[1].bar_list[1], ExtraKind::kSystemBreak).empty());

    const ExtractOptions options{ .surface = MetricSurface{
        .layout = LayoutSurface::kSystemBreaks } };
    const ExtractResult result = ExtractMeiData(mei, options);
    REQUIRE(result.score.parts.size() == 2);
    REQUIRE(result.score.parts[0].bar_list.size() == 2);
    REQUIRE(result.score.parts[1].bar_list.size() == 2);
    const std::vector<const SymExtra *> breaks
        = ExtrasOfKind(result.score.parts[0].bar_list[1], ExtraKind::kSystemBreak);
    REQUIRE(breaks.size() == 1);
    CHECK(breaks[0]->vrv_id == "sb_before_m2");
    CHECK(breaks[0]->offset == Fraction(0));
    CHECK(breaks[0]->symbolic == "systembreak");
    CHECK(breaks[0]->notation_size() == 1);
    CHECK(breaks[0]->str() == "systembreak@0:systembreak");
    CHECK(breaks[0]->locator.part_idx == 0);
    CHECK(breaks[0]->locator.measure_idx == 1);
    CHECK(breaks[0]->locator.measure_vrv_id == result.score.parts[0].bar_list[1].vrv_id);
    CHECK(ExtrasOfKind(result.score.parts[1].bar_list[1], ExtraKind::kSystemBreak).empty());
    CHECK(CountSymbols(result.score).other_extras
        == CountSymbols(default_result.score).other_extras + 1);
    CHECK(result.score.notation_size() == default_result.score.notation_size() + 1);
}

TEST_CASE("MusicXML new-system print emits a system break in layout mode", "[extract]")
{
    const char *musicxml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE score-partwise PUBLIC "-//Recordare//DTD MusicXML 4.0 Partwise//EN" "http://www.musicxml.org/dtds/partwise.dtd">
<score-partwise version="4.0">
  <part-list>
    <score-part id="P1"><part-name>Music</part-name></score-part>
  </part-list>
  <part id="P1">
    <measure number="1">
      <attributes>
        <divisions>1</divisions>
        <key><fifths>0</fifths></key>
        <time><beats>4</beats><beat-type>4</beat-type></time>
        <clef><sign>G</sign><line>2</line></clef>
      </attributes>
      <note><pitch><step>C</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
    </measure>
    <measure number="2">
      <print new-system="yes"/>
      <note><pitch><step>D</step><octave>4</octave></pitch><duration>4</duration><type>whole</type></note>
    </measure>
  </part>
</score-partwise>)xml";

    const ExtractOptions options{ .surface = MetricSurface{
        .layout = LayoutSurface::kSystemBreaks } };
    const ExtractResult result
        = ExtractData(musicxml, vrv::MUSICXML, SourceFormat::kMusicXml, options);
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 2);
    const std::vector<const SymExtra *> breaks
        = ExtrasOfKind(result.score.parts[0].bar_list[1], ExtraKind::kSystemBreak);
    REQUIRE(breaks.size() == 1);
    CHECK(breaks[0]->offset == Fraction(0));
    CHECK(breaks[0]->symbolic == "systembreak");
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
    CHECK(start.pitches[0].tie);
    const SymNote &middle = part.bar_list[1].notes[0]; // 4f#_
    CHECK(middle.pitches[0].accid == "None");
    CHECK(middle.pitches[0].sounding_alter == 1);
    CHECK(middle.pitches[0].tie);
    const SymNote &end = part.bar_list[2].notes[0]; // 4f#]
    CHECK(end.pitches[0].accid == "None");
    CHECK(end.pitches[0].sounding_alter == 1);
    CHECK_FALSE(end.pitches[0].tie);
}

TEST_CASE("active mode extracts articulations", "[extract]")
{
    const ExtractResult result = ExtractKernData("**kern\n*clefG2\n*k[]\n*M4/4\n=1\n4c'\n4d\n4e\n4f\n==\n*-\n");
    CHECK(result.warnings.empty());
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.notes.size() == 4);
    CHECK(std::find(m1.notes[0].articulations.begin(), m1.notes[0].articulations.end(), "staccato")
        != m1.notes[0].articulations.end());
}

TEST_CASE("Humdrum common and cut time symbols count as one visible timesig", "[extract]")
{
    struct Case {
        std::string name;
        std::string signatureTokens;
        std::string symbol;
    };
    const std::vector<Case> cases = {
        { "lowercase common", "*met(c)\n", "common" },
        { "lowercase cut", "*met(c|)\n", "cut" },
        { "uppercase common mensur", "*met(C)\n", "common" },
        { "uppercase cut mensur", "*met(C|)\n", "cut" },
        { "hidden numeric plus uppercase common", "*M4/4\n*met(C)\n", "common" },
        { "hidden numeric plus uppercase cut", "*M2/2\n*met(C|)\n", "cut" },
    };

    for (const Case &testCase : cases) {
        INFO(testCase.name);
        const ExtractResult result = ExtractKernData(
            "**kern\n*clefG2\n" + testCase.signatureTokens + "=1\n1c\n*-\n");
        CHECK(result.warnings.empty());
        REQUIRE(result.score.parts.size() == 1);
        REQUIRE(result.score.parts[0].bar_list.size() == 1);
        const std::vector<const SymExtra *> timesigs
            = ExtrasOfKind(result.score.parts[0].bar_list[0], ExtraKind::kTimeSig);
        REQUIRE(timesigs.size() == 1);
        CHECK(timesigs[0]->notation_size() == 1);
        CHECK(timesigs[0]->infodict
            == std::vector<std::pair<std::string, std::string>>{ { "symbol", testCase.symbol } });
    }
}

TEST_CASE("hidden numeric meterSig governs mRest while mensur is the visible timesig", "[extract]")
{
    const ExtractResult result = ExtractMeiData(R"MEI(
<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2">
                <meterSig count="2" unit="1" visible="false"/>
                <mensur sign="C" slash="1"/>
              </staffDef>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <mRest/>
                </layer>
              </staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>
)MEI");
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    const std::vector<const SymExtra *> timesigs = ExtrasOfKind(m1, ExtraKind::kTimeSig);
    REQUIRE(timesigs.size() == 1);
    CHECK(timesigs[0]->infodict
        == std::vector<std::pair<std::string, std::string>>{ { "symbol", "cut" } });
    REQUIRE(m1.notes.size() == 1);
    CHECK(m1.notes[0].pitches[0].step_octave == "R");
    CHECK(m1.notes[0].note_dur_type == "breve");
    CHECK(m1.notes[0].dots == 0);
}

TEST_CASE("visible meterSig is extracted when paired with scoreDef mensur", "[extract]")
{
    const ExtractResult result = ExtractMeiData(R"MEI(
<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2">
                <meterSig count="4" unit="4"/>
                <mensur sign="C"/>
              </staffDef>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <note dur="4" oct="4" pname="c"/>
                </layer>
              </staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>
)MEI");
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    const std::vector<const SymExtra *> timesigs = ExtrasOfKind(m1, ExtraKind::kTimeSig);
    REQUIRE(timesigs.size() == 1);
    CHECK(timesigs[0]->notation_size() == 2);
    CHECK(timesigs[0]->infodict
        == std::vector<std::pair<std::string, std::string>>{
            { "numerator", "4" }, { "denominator", "4" } });
}

TEST_CASE("scoreDef-level visible meterSig and mensur are both extracted", "[extract]")
{
    const ExtractResult result = ExtractMeiData(R"MEI(
<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <meterSig count="4" unit="4"/>
            <mensur sign="C"/>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <note dur="4" oct="4" pname="c"/>
                </layer>
              </staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>
)MEI");
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    const std::vector<const SymExtra *> timesigs = ExtrasOfKind(m1, ExtraKind::kTimeSig);
    REQUIRE(timesigs.size() == 2);
    bool sawCommon = false;
    bool sawNumeric = false;
    for (const SymExtra *timesig : timesigs) {
        if (timesig->infodict
            == std::vector<std::pair<std::string, std::string>>{ { "symbol", "common" } }) {
            sawCommon = true;
        }
        if (timesig->infodict
            == std::vector<std::pair<std::string, std::string>>{
                { "numerator", "4" }, { "denominator", "4" } }) {
            sawNumeric = true;
        }
    }
    CHECK(sawCommon);
    CHECK(sawNumeric);
}

TEST_CASE("inline mensur updates governing meter for following mRest", "[extract]")
{
    const ExtractResult result = ExtractMeiData(R"MEI(
<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2" meter.count="3" meter.unit="4"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <mensur sign="C"/>
                  <note dur="4" oct="4" pname="c"/>
                </layer>
              </staff>
            </measure>
            <measure n="2">
              <staff n="1">
                <layer n="1">
                  <mRest/>
                </layer>
              </staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>
)MEI");
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 2);
    const SymMeasure &m2 = result.score.parts[0].bar_list[1];
    REQUIRE(m2.notes.size() == 1);
    CHECK(m2.notes[0].pitches[0].step_octave == "R");
    CHECK(m2.notes[0].note_dur_type == "whole");
    CHECK(m2.notes[0].dots == 0);
}

TEST_CASE("inline mensur preserves paired hidden numeric meter for following mRest", "[extract]")
{
    const ExtractResult result = ExtractMeiData(R"MEI(
<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <meterSig count="2" unit="1" visible="false"/>
                  <mensur sign="C" slash="1"/>
                  <note dur="4" oct="4" pname="c"/>
                </layer>
              </staff>
            </measure>
            <measure n="2">
              <staff n="1">
                <layer n="1">
                  <mRest/>
                </layer>
              </staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>
)MEI");
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 2);
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    const std::vector<const SymExtra *> timesigs = ExtrasOfKind(m1, ExtraKind::kTimeSig);
    REQUIRE(timesigs.size() == 1);
    CHECK(timesigs[0]->infodict
        == std::vector<std::pair<std::string, std::string>>{ { "symbol", "cut" } });
    const SymMeasure &m2 = result.score.parts[0].bar_list[1];
    REQUIRE(m2.notes.size() == 1);
    CHECK(m2.notes[0].pitches[0].step_octave == "R");
    CHECK(m2.notes[0].note_dur_type == "breve");
    CHECK(m2.notes[0].dots == 0);
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

TEST_CASE("repair_space_beamspan.mei: preserve mode keeps typed space duration", "[extract]")
{
    const ExtractOptions options{ .surface = MetricSurface{ .mode = MetricMode::kActive },
        .typed_space_handling = TypedSpaceHandling::kPreserve };
    const ExtractResult result = ExtractFixture("repair_space_beamspan.mei", SourceFormat::kOther, options);
    CHECK(result.warnings.empty());
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.notes.size() == 5);

    CHECK(m1.notes[0].note_offset == Fraction(1));
    CHECK(m1.notes[1].note_offset == Fraction(5, 4));
    CHECK(m1.notes[2].note_offset == Fraction(3, 2));
    CHECK(m1.notes[3].note_offset == Fraction(7, 4));
    CHECK(m1.notes[4].note_offset == Fraction(3));
}

TEST_CASE("overlapping_beamspans.mei: nested spans preserve outer beam depth", "[extract]")
{
    const ExtractResult result = ExtractFixture("overlapping_beamspans.mei", SourceFormat::kOther);
    CHECK(result.warnings.empty());
    const SymMeasure &m1 = result.score.parts[0].bar_list[0];
    REQUIRE(m1.notes.size() == 5);

    CHECK(m1.notes[0].beamings == std::vector<BeamValue>{ BeamValue::kStart });
    CHECK(m1.notes[1].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStart });
    CHECK(m1.notes[2].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kContinue });
    CHECK(m1.notes[3].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kContinue });
    CHECK(m1.notes[4].beamings == std::vector<BeamValue>{ BeamValue::kStop, BeamValue::kStop });

    const SymMeasure &m2 = result.score.parts[0].bar_list[1];
    REQUIRE(m2.notes.size() == 5);

    CHECK(m2.notes[0].beamings == std::vector<BeamValue>{ BeamValue::kStart, BeamValue::kPartial });
    CHECK(m2.notes[1].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStart });
    CHECK(m2.notes[2].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kContinue });
    CHECK(m2.notes[3].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStop });
    CHECK(m2.notes[4].beamings == std::vector<BeamValue>{ BeamValue::kStop, BeamValue::kPartial });

    const SymMeasure &m3 = result.score.parts[0].bar_list[2];
    REQUIRE(m3.notes.size() == 5);

    CHECK(m3.notes[0].beamings == std::vector<BeamValue>{ BeamValue::kStart, BeamValue::kStart });
    CHECK(m3.notes[1].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStop });
    CHECK(m3.notes[2].beamings == std::vector<BeamValue>{ BeamValue::kContinue });
    CHECK(m3.notes[3].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStart });
    CHECK(m3.notes[4].beamings == std::vector<BeamValue>{ BeamValue::kStop, BeamValue::kStop });

    const SymMeasure &m4 = result.score.parts[0].bar_list[3];
    REQUIRE(m4.notes.size() == 5);

    CHECK(m4.notes[0].beamings
        == std::vector<BeamValue>{ BeamValue::kStart, BeamValue::kPartial, BeamValue::kPartial });
    CHECK(m4.notes[1].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStart, BeamValue::kStart });
    CHECK(m4.notes[2].beamings
        == std::vector<BeamValue>{
               BeamValue::kContinue, BeamValue::kContinue, BeamValue::kContinue });
    CHECK(m4.notes[3].beamings
        == std::vector<BeamValue>{ BeamValue::kContinue, BeamValue::kStop, BeamValue::kStop });
    CHECK(m4.notes[4].beamings
        == std::vector<BeamValue>{ BeamValue::kStop, BeamValue::kPartial, BeamValue::kPartial });
}

TEST_CASE("VrvBridge normalizes Verovio rhythm-repair spaces after import", "[extract]")
{
    VrvBridge bridge;
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_TEST_FIXTURE_DIR) + "/repair_space_beamspan.mei"));

    CHECK(bridge.last_normalized_rhythm_repair_spaces() == 1);
    CHECK(bridge.GetDoc().FindAllDescendantsByType(vrv::SPACE).size() == 2);

    const vrv::Object *repair = FindStraddleOrFillerSpace(bridge.GetDoc());
    REQUIRE(repair != nullptr);
    const vrv::DurationInterface *duration = repair->GetDurationInterface();
    REQUIRE(duration != nullptr);
    CHECK(duration->GetDur() == vrv::DURATION_1024);
}

TEST_CASE("VrvBridge preserve mode leaves typed spaces unchanged after import", "[extract]")
{
    VrvBridgeConfig config;
    config.typed_space_handling = TypedSpaceHandling::kPreserve;
    VrvBridge bridge(config);
    REQUIRE(bridge.LoadScoreFile(std::string(VEROSIM_TEST_FIXTURE_DIR) + "/repair_space_beamspan.mei"));

    CHECK(bridge.last_normalized_rhythm_repair_spaces() == 0);
    CHECK(bridge.GetDoc().FindAllDescendantsByType(vrv::SPACE).size() == 2);

    const vrv::Object *repair = FindStraddleOrFillerSpace(bridge.GetDoc());
    REQUIRE(repair != nullptr);
    const vrv::DurationInterface *duration = repair->GetDurationInterface();
    REQUIRE(duration != nullptr);
    CHECK(duration->GetDur() == vrv::DURATION_4);
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

TEST_CASE("MEI barlines are extracted as barline and repeat extras", "[extract]")
{
    const char *mei = R"mei(<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2" meter.count="4" meter.unit="4"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1" left="single" right="dbl">
              <staff n="1"><layer n="1"><note dur="1" oct="4" pname="c"/></layer></staff>
            </measure>
            <measure n="2" left="rptstart" right="rptend">
              <staff n="1"><layer n="1"><note dur="1" oct="4" pname="d"/></layer></staff>
            </measure>
            <measure n="3" right="invis">
              <staff n="1">
                <layer n="1">
                  <note dur="2" oct="4" pname="e"/>
                  <barLine form="dbl" xml:id="mid_dbl"/>
                  <note dur="2" oct="4" pname="f"/>
                </layer>
              </staff>
            </measure>
            <measure n="4" right="end">
              <staff n="1"><layer n="1"><note dur="1" oct="4" pname="g"/></layer></staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>)mei";
    const ExtractResult result = ExtractMeiData(mei);
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    const SymPart &part = result.score.parts[0];
    REQUIRE(part.bar_list.size() == 4);

    const std::vector<const SymExtra *> m1_barlines
        = ExtrasOfKind(part.bar_list[0], ExtraKind::kBarline);
    REQUIRE(m1_barlines.size() == 1);
    CHECK(m1_barlines[0]->symbolic == "double");
    CHECK(m1_barlines[0]->str() == "barline@None:double");

    const std::vector<const SymExtra *> m2_repeats
        = ExtrasOfKind(part.bar_list[1], ExtraKind::kRepeat);
    REQUIRE(m2_repeats.size() == 2);
    CHECK(m2_repeats[0]->symbolic == "heavy-light");
    CHECK(m2_repeats[0]->infodict
        == std::vector<std::pair<std::string, std::string>>{ { "repeatdirection", "start" } });
    CHECK(m2_repeats[0]->notation_size() == 2);
    CHECK(m2_repeats[1]->symbolic == "final");
    CHECK(m2_repeats[1]->infodict
        == std::vector<std::pair<std::string, std::string>>{ { "repeatdirection", "end" } });
    CHECK(m2_repeats[1]->notation_size() == 2);

    const std::vector<const SymExtra *> m3_barlines
        = ExtrasOfKind(part.bar_list[2], ExtraKind::kBarline);
    REQUIRE(m3_barlines.size() == 1);
    CHECK(m3_barlines[0]->vrv_id == "mid_dbl");
    CHECK(m3_barlines[0]->symbolic == "double");
    CHECK(m3_barlines[0]->offset == Fraction(2));

    const std::vector<const SymExtra *> m4_barlines
        = ExtrasOfKind(part.bar_list[3], ExtraKind::kBarline);
    REQUIRE(m4_barlines.size() == 1);
    CHECK(m4_barlines[0]->symbolic == "final");
    CHECK(CountSymbols(result.score).barline == 7);
    CHECK(result.score.notation_size() == 20);
}

TEST_CASE("regular layer barline at a partial-measure end is ignored", "[extract]")
{
    const char *mei = R"mei(<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2" meter.count="4" meter.unit="4"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <note dur="2" oct="4" pname="c"/>
                  <barLine form="single" xml:id="partial_regular"/>
                </layer>
              </staff>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>)mei";
    const ExtractResult result = ExtractMeiData(mei);
    CHECK(result.warnings.empty());
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    CHECK(ExtrasOfKind(result.score.parts[0].bar_list[0], ExtraKind::kBarline).empty());
    CHECK(CountSymbols(result.score).barline == 0);
}

TEST_CASE("mei_controls.mei: timestamp controls use active meter and staff resolution", "[extract]")
{
    const ExtractResult result = ExtractFixture(
        "mei_controls.mei", SourceFormat::kOther,
        ExtractOptions{ .surface = MetricSurface{ .mode = MetricMode::kExperimental } });
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

    const SymExtra *octave_8va = find_extra(staff1_m1, "octave_8va_staff1");
    REQUIRE(octave_8va != nullptr);
    CHECK(octave_8va->kind == ExtraKind::kOttava);
    CHECK(octave_8va->symbolic == "8va");
    CHECK(octave_8va->offset == Fraction(1, 2));
    REQUIRE(octave_8va->duration.has_value());
    CHECK(*octave_8va->duration == Fraction(5, 2));
    CHECK(find_extra(staff2_m1, "octave_8va_staff1") == nullptr);

    const SymExtra *octave_8vb = find_extra(staff2_m1, "octave_8vb_staff2");
    REQUIRE(octave_8vb != nullptr);
    CHECK(octave_8vb->kind == ExtraKind::kOttava);
    CHECK(octave_8vb->symbolic == "8vb");
    CHECK(octave_8vb->offset == Fraction(0));
    REQUIRE(octave_8vb->duration.has_value());
    CHECK(*octave_8vb->duration == Fraction(2));
    CHECK(find_extra(staff1_m1, "octave_8vb_staff2") == nullptr);

    const SymExtra *cross_octave = find_extra(staff1_m1, "octave_cross_measure");
    REQUIRE(cross_octave != nullptr);
    CHECK(cross_octave->kind == ExtraKind::kOttava);
    CHECK(cross_octave->symbolic == "15ma");
    CHECK(cross_octave->offset == Fraction(2));
    REQUIRE(cross_octave->duration.has_value());
    CHECK(*cross_octave->duration == Fraction(4));

    long ottava_symbols = 0;
    for (const SymPart &part : result.score.parts) {
        for (const SymMeasure &measure : part.bar_list) {
            for (const SymExtra &extra : measure.extras) {
                if (extra.kind == ExtraKind::kOttava) ottava_symbols += extra.notation_size();
            }
        }
    }
    CHECK(ottava_symbols == 6);
}

TEST_CASE("mei_controls.mei: active mode extracts slurs but skips directions and ottavas", "[extract]")
{
    const ExtractResult result = ExtractFixture("mei_controls.mei", SourceFormat::kOther);
    CHECK(result.warnings.empty());
    const SymMeasure &staff1_m1 = result.score.parts[0].bar_list[0];
    const SymMeasure &staff2_m1 = result.score.parts[1].bar_list[0];

    CHECK(ExtrasOfKind(staff1_m1, ExtraKind::kDynamic).empty());
    CHECK(ExtrasOfKind(staff1_m1, ExtraKind::kCrescendo).empty());
    CHECK(ExtrasOfKind(staff1_m1, ExtraKind::kOttava).empty());
    CHECK(ExtrasOfKind(staff2_m1, ExtraKind::kDynamic).empty());
    CHECK(ExtrasOfKind(staff2_m1, ExtraKind::kOttava).empty());
    CHECK(ExtrasOfKind(staff1_m1, ExtraKind::kSlur).size() == 1);
    CHECK(ExtrasOfKind(staff2_m1, ExtraKind::kSlur).size() == 1);
}

TEST_CASE("unsupported octave displacement warns and is skipped", "[extract]")
{
    const char *mei = R"mei(<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2" meter.count="4" meter.unit="4"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <note xml:id="n1" dur="4" oct="4" pname="c"/>
                </layer>
              </staff>
              <octave xml:id="octave_22" staff="1" startid="#n1" endid="#n1" dis="22" dis.place="above"/>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>)mei";
    const ExtractResult result = ExtractMeiData(
        mei, ExtractOptions{ .surface = MetricSurface{ .mode = MetricMode::kExperimental } });
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0] == "octave control with 22ma/22mb displacement is unsupported");
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    CHECK(ExtrasOfKind(result.score.parts[0].bar_list[0], ExtraKind::kOttava).empty());
}

TEST_CASE("octave without placement warns and is skipped", "[extract]")
{
    const char *mei = R"mei(<?xml version="1.0" encoding="UTF-8"?>
<mei xmlns="http://www.music-encoding.org/ns/mei" meiversion="5.0">
  <music>
    <body>
      <mdiv>
        <score>
          <scoreDef>
            <staffGrp>
              <staffDef n="1" lines="5" clef.shape="G" clef.line="2" meter.count="4" meter.unit="4"/>
            </staffGrp>
          </scoreDef>
          <section>
            <measure n="1">
              <staff n="1">
                <layer n="1">
                  <note xml:id="n1" dur="4" oct="4" pname="c"/>
                </layer>
              </staff>
              <octave xml:id="octave_no_place" staff="1" startid="#n1" endid="#n1" dis="8"/>
            </measure>
          </section>
        </score>
      </mdiv>
    </body>
  </music>
</mei>)mei";
    const ExtractResult result = ExtractMeiData(
        mei, ExtractOptions{ .surface = MetricSurface{ .mode = MetricMode::kExperimental } });
    REQUIRE(result.warnings.size() == 1);
    CHECK(result.warnings[0] == "octave control lacks a supported displacement or placement");
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    CHECK(ExtrasOfKind(result.score.parts[0].bar_list[0], ExtraKind::kOttava).empty());
}
