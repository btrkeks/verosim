// Engine unit tests over hand-built SymScore fixtures (no Verovio involved):
// the within-note diffs, the greedy set distances, the Myers grouping, the
// block DP tie-breaks, and the top-level assembly — each pinned against the
// behavior of the corresponding comparison.py function.

#include <map>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "engine_fixtures.h"
#include "verosim/engine/compare.h"
#include "verosim/engine/interner.h"
#include "verosim/engine/myers.h"
#include "verosim/engine/note_diff.h"
#include "verosim/engine/set_distance.h"

using namespace verosim;
using namespace verosim::test;

namespace {

std::vector<std::string> OpNames(const std::vector<EditOp> &ops)
{
    std::vector<std::string> names;
    for (const EditOp &op : ops) names.emplace_back(OpNameStr(op.name));
    return names;
}

std::map<std::string, int> OpMultiset(const std::vector<EditOp> &ops)
{
    std::map<std::string, int> counts;
    for (const EditOp &op : ops) counts[std::string(OpNameStr(op.name))] += 1;
    return counts;
}

CompareOptions MusicalOptions()
{
    return CompareOptions{ .note_position_policy = NotePositionPolicy::kMusicalOnset };
}

DiffResult MeasureDiff(
    const SymMeasure &a, const SymMeasure &b, const CompareOptions &options = CompareOptions())
{
    StringInterner interner;
    const SymScore sa = MakeScore({ { a } });
    const SymScore sb = MakeScore({ { b } });
    const PreparedScore pa = PrepareScore(sa, interner);
    const PreparedScore pb = PrepareScore(sb, interner);
    DiffResult notes = NotesSetDistance(sa.parts[0].bar_list[0], sb.parts[0].bar_list[0],
        pa.parts[0].measures[0], pb.parts[0].measures[0], options);
    DiffResult extras = ExtrasSetDistance(sa.parts[0].bar_list[0], sb.parts[0].bar_list[0],
        pa.parts[0].measures[0], pb.parts[0].measures[0]);
    notes.cost += extras.cost;
    notes.ops.insert(notes.ops.end(), extras.ops.begin(), extras.ops.end());
    return notes;
}

} // namespace

TEST_CASE("pitches_diff covers every branch", "[engine]")
{
    const SymNote a = MakeNote("C4", Fraction(0));
    const SymNote b = MakeNote("D4", Fraction(0));

    SECTION("pitch name edit")
    {
        const DiffResult r = PitchesDiff(a.pitches[0], b.pitches[0], &a, &b, 0, 0);
        CHECK(r.cost == 1);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "pitchnameedit" });
        CHECK(r.ops[0].ids_kind == EditOp::IdsKind::kPitchPair);
    }
    SECTION("rest vs note is pitchtypeedit (xor)")
    {
        const SymNote rest = MakeNote("R", Fraction(0));
        const DiffResult r = PitchesDiff(a.pitches[0], rest.pitches[0], &a, &rest, 0, 0);
        CHECK(r.cost == 1);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "pitchtypeedit" });
    }
    SECTION("accidental ins / del / edit")
    {
        const SymNote sharp = MakeNote("C4", Fraction(0), { .accid = "sharp" });
        const SymNote flat = MakeNote("C4", Fraction(0), { .accid = "flat" });
        const DiffResult ins = PitchesDiff(a.pitches[0], sharp.pitches[0], &a, &sharp, 0, 0);
        CHECK(ins.cost == 1);
        CHECK(OpNames(ins.ops) == std::vector<std::string>{ "accidentins" });
        const DiffResult del = PitchesDiff(sharp.pitches[0], a.pitches[0], &sharp, &a, 0, 0);
        CHECK(del.cost == 1);
        CHECK(OpNames(del.ops) == std::vector<std::string>{ "accidentdel" });
        const DiffResult edit = PitchesDiff(sharp.pitches[0], flat.pitches[0], &sharp, &flat, 0, 0);
        CHECK(edit.cost == 2); // delete one accidental, add the other
        CHECK(OpNames(edit.ops) == std::vector<std::string>{ "accidentedit" });
    }
    SECTION("tie ins / del")
    {
        const SymNote tied = MakeNote("C4", Fraction(0), { .tie = true });
        const DiffResult ins = PitchesDiff(a.pitches[0], tied.pitches[0], &a, &tied, 0, 0);
        CHECK(ins.cost == 1);
        CHECK(OpNames(ins.ops) == std::vector<std::string>{ "tieins" });
        const DiffResult del = PitchesDiff(tied.pitches[0], a.pitches[0], &tied, &a, 0, 0);
        CHECK(OpNames(del.ops) == std::vector<std::string>{ "tiedel" });
    }
    SECTION("combined name+accidental+tie accumulates")
    {
        const SymNote x = MakeNote("C4", Fraction(0), { .accid = "sharp", .tie = true });
        const DiffResult r = PitchesDiff(x.pitches[0], b.pitches[0], &x, &b, 0, 0);
        CHECK(r.cost == 3); // name 1 + accidentdel 1 + tiedel 1
    }
}

TEST_CASE("annotated_note_diff per-field costs", "[engine]")
{
    const SymNote base = MakeNote("C4", Fraction(0));

    SECTION("note head edit costs 2")
    {
        const SymNote half = MakeNote("C4", Fraction(0), { .head = Fraction(2) });
        const DiffResult r = AnnotatedNoteDiff(base, half);
        CHECK(r.cost == 2);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "headedit" });
    }
    SECTION("dots cost the count difference")
    {
        const SymNote dotted = MakeNote("C4", Fraction(0), { .dots = 2 });
        const DiffResult ins = AnnotatedNoteDiff(base, dotted);
        CHECK(ins.cost == 2);
        CHECK(OpNames(ins.ops) == std::vector<std::string>{ "dotins" });
        const DiffResult del = AnnotatedNoteDiff(dotted, base);
        CHECK(OpNames(del.ops) == std::vector<std::string>{ "dotdel" });
    }
    SECTION("graceness")
    {
        const SymNote grace
            = MakeNote("C4", Fraction(0), { .grace_type = "acc", .is_grace = true });
        const SymNote slashed = MakeNote(
            "C4", Fraction(0), { .grace_type = "acc", .grace_slash = true, .is_grace = true });
        CHECK(AnnotatedNoteDiff(base, grace).cost == 2); // graceedit
        const DiffResult r = AnnotatedNoteDiff(grace, slashed);
        CHECK(r.cost == 1);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "graceslashedit" });
    }
    SECTION("beam Levenshtein: edit beats del+ins strictly")
    {
        const SymNote a = MakeNote("C4", Fraction(0), { .beams = { BeamValue::kStart } });
        const SymNote b = MakeNote("C4", Fraction(0), { .beams = { BeamValue::kStop } });
        const DiffResult r = AnnotatedNoteDiff(a, b);
        CHECK(r.cost == 1);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "editbeam" });
    }
    SECTION("beam Levenshtein: pure insertion")
    {
        const SymNote a = MakeNote("C4", Fraction(0), { .beams = { BeamValue::kStart } });
        const SymNote b = MakeNote(
            "C4", Fraction(0), { .beams = { BeamValue::kStart, BeamValue::kContinue } });
        const DiffResult r = AnnotatedNoteDiff(a, b);
        CHECK(r.cost == 1);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "insbeam" });
    }
    SECTION("tuplet_info diff uses tuplet op names")
    {
        SymNote a = MakeNote("C4", Fraction(0),
            { .tuplets = { TupletValue::kStart }, .tuplet_info = { "3:2" } });
        SymNote b = MakeNote("C4", Fraction(0),
            { .tuplets = { TupletValue::kStart }, .tuplet_info = { "3:2q" } });
        const DiffResult r = AnnotatedNoteDiff(a, b);
        CHECK(r.cost == 1);
        CHECK(OpNames(r.ops) == std::vector<std::string>{ "edittuplet" });
    }
    SECTION("sounding_alter is not comparison content")
    {
        const SymNote resolved = MakeNote("C4", Fraction(0), { .sounding_alter = 1 });
        CHECK(AnnotatedNoteDiff(base, resolved).cost == 0);
        StringInterner interner;
        CHECK(interner.Intern(base.str()) == interner.Intern(resolved.str()));
    }
}

TEST_CASE("notes_set_distance pairing", "[engine]")
{
    SECTION("pitch change is del+ins, accidental change is a pitch edit")
    {
        // the D14 mutation-corpus finding: matching keys on step+octave+offset
        const SymMeasure m1 = MakeMeasure({ MakeNote("C4", Fraction(0)) });
        const SymMeasure changed_letter = MakeMeasure({ MakeNote("D4", Fraction(0)) });
        const SymMeasure changed_accid
            = MakeMeasure({ MakeNote("C4", Fraction(0), { .accid = "sharp" }) });

        const DiffResult del_ins = MeasureDiff(m1, changed_letter);
        CHECK(del_ins.cost == 4); // del(2) + ins(2)
        CHECK(OpMultiset(del_ins.ops) == std::map<std::string, int>{ { "notedel", 1 },
                  { "noteins", 1 } });

        const DiffResult edit = MeasureDiff(m1, changed_accid);
        CHECK(edit.cost == 1);
        CHECK(OpMultiset(edit.ops) == std::map<std::string, int>{ { "accidentins", 1 } });
    }
    SECTION("offset shift beyond tolerance unpairs")
    {
        const SymMeasure m1 = MakeMeasure({ MakeNote("C4", Fraction(0)) });
        const SymMeasure m2 = MakeMeasure({ MakeNote("C4", Fraction(1, 2)) });
        CHECK(MeasureDiff(m1, m2, MusicalOptions()).cost == 4);
    }
    SECTION("visual mode pairs same notes across offset shifts")
    {
        const SymMeasure m1 = MakeMeasure({ MakeNote("C4", Fraction(0)) });
        const SymMeasure m2 = MakeMeasure({ MakeNote("C4", Fraction(1, 2)) });
        CHECK(MeasureDiff(m1, m2).cost == 0);
    }
    SECTION("graceness mismatch unpairs")
    {
        const SymMeasure m1 = MakeMeasure({ MakeNote("C4", Fraction(0)) });
        const SymMeasure m2 = MakeMeasure(
            { MakeNote("C4", Fraction(0), { .grace_type = "acc", .is_grace = true }) });
        const DiffResult r = MeasureDiff(m1, m2);
        CHECK(OpMultiset(r.ops) == std::map<std::string, int>{ { "notedel", 1 },
                  { "noteins", 1 } });
    }
    SECTION("exact duration match is preferred over the first fallback")
    {
        // orig quarter scans comp [half, quarter]: the half is only a
        // fallback; the quarter is the perfect match even though it is later.
        const SymMeasure orig = MakeMeasure({
            MakeNote("C4", Fraction(0), { .dur_type = "quarter", .id = "o0" }),
            MakeNote("C4", Fraction(0), { .head = Fraction(2), .dur_type = "half", .id = "o1" }),
        });
        const SymMeasure comp = MakeMeasure({
            MakeNote("C4", Fraction(0), { .head = Fraction(2), .dur_type = "half", .id = "c0" }),
            MakeNote("C4", Fraction(0), { .dur_type = "quarter", .id = "c1" }),
        });
        CHECK(MeasureDiff(orig, comp, MusicalOptions()).cost == 0); // cross-paired exactly
    }
    SECTION("fallback pairing produces a duration sub-diff")
    {
        const SymMeasure orig = MakeMeasure({ MakeNote("C4", Fraction(0)) });
        const SymMeasure comp = MakeMeasure(
            { MakeNote("C4", Fraction(0), { .head = Fraction(2), .dur_type = "half" }) });
        const DiffResult r = MeasureDiff(orig, comp, MusicalOptions());
        CHECK(r.cost == 2); // headedit via the fallback pair, not del+ins (4)
        CHECK(OpMultiset(r.ops) == std::map<std::string, int>{ { "headedit", 1 } });
    }
    SECTION("visual sequence alignment skips an inserted note and matches later notes")
    {
        const SymMeasure orig = MakeMeasure({
            MakeNote("C4", Fraction(0)),
            MakeNote("D4", Fraction(1)),
        });
        const SymMeasure comp = MakeMeasure({
            MakeNote("E4", Fraction(0)),
            MakeNote("C4", Fraction(1, 2)),
            MakeNote("D4", Fraction(3, 2)),
        });
        const DiffResult r = MeasureDiff(orig, comp);
        CHECK(r.cost == 2);
        CHECK(OpMultiset(r.ops) == std::map<std::string, int>{ { "noteins", 1 } });
    }
}

TEST_CASE("extras set distance and diff", "[engine]")
{
    SECTION("clef pairing prefers equal symbolic")
    {
        // two simultaneous clefs must not be cross-paired
        const SymMeasure orig = MakeMeasure({}, { MakeClef("G2"), MakeClef("F4") });
        const SymMeasure comp = MakeMeasure({}, { MakeClef("F4"), MakeClef("G2") });
        CHECK(MeasureDiff(orig, comp).cost == 0);
    }
    SECTION("clef change costs a symbol edit")
    {
        const SymMeasure orig = MakeMeasure({}, { MakeClef("G2") });
        const SymMeasure comp = MakeMeasure({}, { MakeClef("F4") });
        const DiffResult r = MeasureDiff(orig, comp);
        CHECK(r.cost == 2);
        CHECK(OpMultiset(r.ops) == std::map<std::string, int>{ { "extrasymboledit", 1 } });
    }
    SECTION("keysig infodict value change costs 2 (delete + add)")
    {
        const SymMeasure orig = MakeMeasure({}, { MakeKeySig("2") });
        const SymMeasure comp = MakeMeasure({}, { MakeKeySig("3") });
        const DiffResult r = MeasureDiff(orig, comp);
        CHECK(r.cost == 2);
        CHECK(OpMultiset(r.ops) == std::map<std::string, int>{ { "extrainfoedit", 1 } });
    }
    SECTION("unmatched extra is del + ins of its notation size")
    {
        const SymMeasure orig = MakeMeasure({}, { MakeTimeSig("3", "4") });
        const SymMeasure comp = MakeMeasure({}, {});
        const DiffResult r = MeasureDiff(orig, comp);
        CHECK(r.cost == 2); // numerator + denominator
        CHECK(OpMultiset(r.ops) == std::map<std::string, int>{ { "extradel", 1 } });
    }
}

TEST_CASE("are_different_enough tolerance", "[engine]")
{
    CHECK_FALSE(AreDifferentEnough(Fraction(1, 2), Fraction(1, 2)));
    CHECK_FALSE(AreDifferentEnough(Fraction(1, 2), Fraction(1, 2) + Fraction(1, 10000)));
    CHECK(AreDifferentEnough(Fraction(1, 2), Fraction(1, 2) + Fraction(1, 9999)));
    const std::optional<Fraction> none;
    CHECK_FALSE(AreDifferentEnough(none, none));
    CHECK(AreDifferentEnough(none, std::optional<Fraction>(Fraction(1))));
}

TEST_CASE("myers non-common subsequences", "[engine]")
{
    SECTION("equal sequences yield no blocks")
    {
        CHECK(NonCommonSubsequences({ 1, 2, 3 }, { 1, 2, 3 }).empty());
    }
    SECTION("middle deletion")
    {
        const auto blocks = NonCommonSubsequences({ 1, 2, 3, 4 }, { 1, 3, 4 });
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].original == std::vector<int>{ 1 });
        CHECK(blocks[0].compare_to.empty());
    }
    SECTION("leading insertion")
    {
        const auto blocks = NonCommonSubsequences({ 1, 2 }, { 3, 1, 2 });
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].original.empty());
        CHECK(blocks[0].compare_to == std::vector<int>{ 0 });
    }
    SECTION("two separated differing runs stay separate blocks")
    {
        const auto blocks = NonCommonSubsequences({ 1, 9, 3, 8 }, { 1, 2, 3, 4 });
        REQUIRE(blocks.size() == 2);
        CHECK(blocks[0].original == std::vector<int>{ 1 });
        CHECK(blocks[0].compare_to == std::vector<int>{ 1 });
        CHECK(blocks[1].original == std::vector<int>{ 3 });
        CHECK(blocks[1].compare_to == std::vector<int>{ 3 });
    }
    SECTION("disjoint sequences are one whole block")
    {
        const auto blocks = NonCommonSubsequences({ 1, 2 }, { 3, 4, 5 });
        REQUIRE(blocks.size() == 1);
        CHECK(blocks[0].original == std::vector<int>{ 0, 1 });
        CHECK(blocks[0].compare_to == std::vector<int>{ 0, 1, 2 });
    }
}

TEST_CASE("block diff tie-break and op order", "[engine]")
{
    // Single-note measures with different pitch: editbar inside cost (4) ties
    // with delbar+insbar (2+2). Python's min() picks the first minimal key in
    // {delbar, insbar, editbar} insertion order -> delbar, and the recursion
    // appends ops after the recursive result -> [insbar, delbar].
    const SymScore a = MakeScore({ { MakeMeasure({ MakeNote("C4", Fraction(0)) }) } });
    const SymScore b = MakeScore({ { MakeMeasure({ MakeNote("D4", Fraction(0)) }) } });
    const CompareResult r = CompareScores(a, b);
    CHECK(r.cost == 4);
    CHECK(OpNames(r.op_list) == std::vector<std::string>{ "insbar", "delbar" });
}

TEST_CASE("block diff prefers editbar when strictly cheaper", "[engine]")
{
    // Two-note measures, one note differs in pitch letter: inside cost 4 <
    // delbar(4) + insbar(4).
    const SymScore a = MakeScore({ { MakeMeasure(
        { MakeNote("C4", Fraction(0)), MakeNote("E4", Fraction(1)) }) } });
    const SymScore b = MakeScore({ { MakeMeasure(
        { MakeNote("C4", Fraction(0)), MakeNote("F4", Fraction(1)) }) } });
    const CompareResult r = CompareScores(a, b);
    CHECK(r.cost == 4);
    CHECK(OpMultiset(r.op_list) == std::map<std::string, int>{ { "notedel", 1 },
              { "noteins", 1 } });
}

TEST_CASE("visual compare keeps repeated measures positionally aligned", "[engine]")
{
    const auto plain = [](const std::string &pitch) {
        return MakeMeasure({ MakeNote(pitch, Fraction(0)) });
    };
    const auto dotted = [](const std::string &pitch) {
        return MakeMeasure({ MakeNote(pitch, Fraction(0), { .dots = 1 }) });
    };

    const SymScore pred = MakeScore({ {
        dotted("C4"),
        dotted("D4"),
        dotted("E4"),
        dotted("F4"),
        plain("C4"),
        plain("A4"),
        plain("B4"),
        plain("G4"),
    } });
    const SymScore gt = MakeScore({ {
        plain("C4"),
        plain("D4"),
        plain("E4"),
        plain("F4"),
        dotted("C4"),
        plain("A4"),
        plain("B4"),
        plain("G4"),
    } });

    const CompareResult visual = CompareScores(pred, gt);
    CHECK(visual.cost == 5);
    CHECK(OpMultiset(visual.op_list)
        == std::map<std::string, int>{ { "dotdel", 4 }, { "dotins", 1 } });

    const CompareResult musical = CompareScores(pred, gt, MusicalOptions());
    CHECK(musical.cost > visual.cost);
}

TEST_CASE("visual compare keeps whole-measure insertion tails in one block", "[engine]")
{
    const auto plain = [](const std::string &pitch) {
        return MakeMeasure({ MakeNote(pitch, Fraction(0)) });
    };

    const SymScore pred = MakeScore({ { plain("C4"), plain("D4"), plain("E4") } });
    const SymScore gt = MakeScore({ { plain("C4"), plain("F4"), plain("D4"), plain("E4") } });

    const CompareResult visual = CompareScores(pred, gt);
    CHECK(visual.cost == 2);
    CHECK(OpMultiset(visual.op_list) == std::map<std::string, int>{ { "insbar", 1 } });
}

TEST_CASE("compare_scores part handling", "[engine]")
{
    const auto measure = [] { return MakeMeasure({ MakeNote("C4", Fraction(0)) }); };
    SECTION("extra pred part is delpart")
    {
        const SymScore two = MakeScore({ { measure() }, { measure() } });
        const SymScore one = MakeScore({ { measure() } });
        const CompareResult r = CompareScores(two, one);
        CHECK(r.cost == 2);
        CHECK(OpNames(r.op_list) == std::vector<std::string>{ "delpart" });
        const CompareResult ins = CompareScores(one, two);
        CHECK(OpNames(ins.op_list) == std::vector<std::string>{ "inspart" });
    }
    SECTION("identity is zero")
    {
        const SymScore s = MakeScore({
            { MakeMeasure({ MakeNote("C4", Fraction(0), { .accid = "sharp" }),
                              MakeNote("D4", Fraction(1), { .dots = 1 }) },
                  { MakeClef("G2"), MakeTimeSig("4", "4"), MakeKeySig("1") }),
                MakeMeasure({ MakeNote("R", Fraction(0), { .dur_type = "whole" }) }) },
            { MakeMeasure({ MakeNote("C2", Fraction(0),
                  { .beams = { BeamValue::kStart, BeamValue::kStop } }) }) },
        });
        const CompareResult r = CompareScores(s, s);
        CHECK(r.cost == 0);
        CHECK(r.op_list.empty());
    }
}

TEST_CASE("edit distances dict and omr_ned", "[engine]")
{
    SECTION("extra ops are rewritten by kind")
    {
        const SymScore a = MakeScore({ { MakeMeasure({ MakeNote("C4", Fraction(0)) },
            { MakeKeySig("2") }) } });
        const SymScore b = MakeScore({ { MakeMeasure({ MakeNote("C4", Fraction(0)) },
            { MakeKeySig("3") }) } });
        const CompareResult r = CompareScores(a, b);
        const auto dict = EditDistancesDict(r.op_list);
        CHECK(dict.at("wrong keysig OMR-ED") == 2);
        CHECK(dict.at("bad kern syntax OMR-ED") == 0);
    }
    SECTION("note ops bucket under wrong note")
    {
        const SymScore a = MakeScore({ { MakeMeasure({ MakeNote("C4", Fraction(0)),
            MakeNote("E4", Fraction(1)) }) } });
        const SymScore b = MakeScore({ { MakeMeasure({ MakeNote("C4", Fraction(0)),
            MakeNote("F4", Fraction(1)) }) } });
        const auto dict = EditDistancesDict(CompareScores(a, b).op_list);
        CHECK(dict.at("wrong note OMR-ED") == 4);
    }
    SECTION("omr_ned")
    {
        CHECK(OmrNed(0, 0, 0) == 0.0);
        CHECK(OmrNed(4, 10, 10) == 0.2);
    }
}
