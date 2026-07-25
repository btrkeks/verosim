#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "verosim/app/evaluation_profile_loader.h"
#include "verosim/engine/compare.h"
#include "verosim/model/evaluation_profile.h"

using namespace verosim;

namespace {

EvaluationProfile CompleteProfile(
    EvaluationFeature ignored = EvaluationFeature::kCount)
{
    EvaluationFeatureDispositions dispositions;
    for (const EvaluationFeatureSpec &spec : EvaluationFeatureRegistryV1()) {
        dispositions[EvaluationFeatureIndex(spec.feature)]
            = spec.capability == EvaluationFeatureCapability::kKnownUnsupported
            ? FeatureDisposition::kUnsupported
            : FeatureDisposition::kScored;
    }
    if (ignored != EvaluationFeature::kCount) {
        dispositions[EvaluationFeatureIndex(ignored)] = FeatureDisposition::kIgnored;
    }
    std::string error;
    auto profile = MakeEvaluationProfileV1(
        "test.profile", "sha256:test", dispositions, error);
    REQUIRE(profile.has_value());
    REQUIRE(error.empty());
    return std::move(*profile);
}

SymScore RichScore()
{
    SymPitch pitch;
    pitch.step_octave = "F4";
    pitch.accid = "sharp";
    pitch.tie = true;
    pitch.sounding_alter = 1;

    SymNote note;
    note.vrv_id = "note";
    note.visual_id = "note";
    note.pitches = { pitch };
    note.note_head = Fraction(4);
    note.dots = 1;
    note.beamings = { BeamValue::kStart, BeamValue::kStop };
    note.tuplets = { TupletValue::kStart };
    note.tuplet_info = { "3:2" };
    note.articulations = { "Accent", "Staccato" };
    note.expressions = { "Trill" };
    note.grace_type = "acc";
    note.grace_slash = true;
    note.gap_dur = Fraction(1, 8);
    note.note_offset = Fraction(3, 2);
    note.note_dur_type = "eighth";
    note.note_dur_dots = 1;
    note.note_is_grace = true;

    SymMeasure measure;
    measure.vrv_id = "measure";
    measure.notes.push_back(note);
    const std::array<ExtraKind, 12> kinds = { ExtraKind::kArpeggio,
        ExtraKind::kBarline, ExtraKind::kClef, ExtraKind::kCrescendo,
        ExtraKind::kDiminuendo, ExtraKind::kDynamic, ExtraKind::kKeySig,
        ExtraKind::kOttava, ExtraKind::kRepeat, ExtraKind::kSlur,
        ExtraKind::kSystemBreak, ExtraKind::kTimeSig };
    for (const ExtraKind kind : kinds) {
        SymExtra extra;
        extra.vrv_id = std::string(ExtraKindName(kind));
        extra.kind = kind;
        extra.symbolic = std::string(ExtraKindName(kind));
        extra.offset = Fraction(0);
        measure.extras.push_back(std::move(extra));
    }

    SymPart part;
    part.staff_n = "1";
    part.bar_list.push_back(std::move(measure));
    SymScore score;
    score.parts.push_back(std::move(part));
    return score;
}

std::string ValidProfileJson(const std::string &id = "test.profile")
{
    std::ostringstream out;
    out << "{\"schema\":\"verosim.evaluation-profile\",\"schema_version\":1,\"id\":\""
        << id << "\",\"features\":{";
    bool first = true;
    for (const EvaluationFeatureSpec &spec : EvaluationFeatureRegistryV1()) {
        if (!first) out << ",";
        first = false;
        out << "\"" << spec.key << "\":\""
            << (spec.capability == EvaluationFeatureCapability::kKnownUnsupported
                    ? "unsupported"
                    : "scored")
            << "\"";
    }
    out << "}}";
    return out.str();
}

std::filesystem::path WriteProfile(const std::string &name, const std::string &text)
{
    const std::filesystem::path path
        = std::filesystem::temp_directory_path() / ("verosim-profile-" + name + ".json");
    std::ofstream out(path, std::ios::binary);
    REQUIRE(out);
    out << text;
    REQUIRE(out.good());
    return path;
}

} // namespace

TEST_CASE("evaluation profile v1 registry is exact and exhaustive", "[evaluation-profile]")
{
    const auto registry = EvaluationFeatureRegistryV1();
    CHECK(registry.size() == EvaluationFeatureIndex(EvaluationFeature::kCount));
    std::set<std::string> keys;
    for (const EvaluationFeatureSpec &spec : registry) {
        CHECK(keys.insert(std::string(spec.key)).second);
    }
    CHECK(keys.contains("note.tie"));
    CHECK(keys.contains("extra.slur"));
    CHECK(keys.contains("extra.arpeggio"));
    CHECK(keys.contains("layout.system_break"));
    CHECK(keys.contains("note.ornament"));
    CHECK(keys.contains("extra.pedal"));
    CHECK(keys.contains("layout.page_break"));
}

TEST_CASE("projection removes only ignored note dimensions and preserves resolved state",
    "[evaluation-profile]")
{
    SymScore score = RichScore();
    EvaluationFeatureDispositions dispositions = CompleteProfile().dispositions();
    for (const EvaluationFeature feature : { EvaluationFeature::kAccidental,
             EvaluationFeature::kTie, EvaluationFeature::kBeam,
             EvaluationFeature::kGraceSlash, EvaluationFeature::kGap,
             EvaluationFeature::kArticulation, EvaluationFeature::kExpression }) {
        dispositions[EvaluationFeatureIndex(feature)] = FeatureDisposition::kIgnored;
    }
    std::string error;
    auto profile = MakeEvaluationProfileV1(
        "ignore.note.details", "sha256:test", dispositions, error);
    REQUIRE(profile.has_value());

    const long original_size = score.notation_size();
    ProjectionResult result = ProjectSymScore(score, *profile);
    REQUIRE(result.score.parts.size() == 1);
    REQUIRE(result.score.parts[0].bar_list.size() == 1);
    REQUIRE(result.score.parts[0].bar_list[0].notes.size() == 1);
    const SymNote &note = result.score.parts[0].bar_list[0].notes[0];
    REQUIRE(note.pitches.size() == 1);
    CHECK(note.pitches[0].step_octave == "F4");
    CHECK(note.pitches[0].accid == "None");
    CHECK_FALSE(note.pitches[0].tie);
    CHECK(note.pitches[0].sounding_alter == 1);
    CHECK(note.note_head == Fraction(4));
    CHECK(note.dots == 1);
    CHECK(note.tuplets.size() == 1);
    CHECK(note.tuplet_info.size() == 1);
    CHECK(note.grace_type == "acc");
    CHECK(note.note_is_grace);
    CHECK(note.note_offset == Fraction(3, 2));
    CHECK(note.note_dur_type == "eighth");
    CHECK(note.note_dur_dots == 1);
    CHECK(note.beamings.empty());
    CHECK_FALSE(note.grace_slash);
    CHECK(note.gap_dur == Fraction(0));
    CHECK(note.articulations.empty());
    CHECK(note.expressions.empty());
    CHECK(result.removed[EvaluationFeature::kAccidental] == 1);
    CHECK(result.removed[EvaluationFeature::kTie] == 1);
    CHECK(result.removed[EvaluationFeature::kBeam] == 2);
    CHECK(result.removed[EvaluationFeature::kGraceSlash] == 1);
    CHECK(result.removed[EvaluationFeature::kGap] == 1);
    CHECK(result.removed[EvaluationFeature::kArticulation] == 2);
    CHECK(result.removed[EvaluationFeature::kExpression] == 1);
    CHECK(result.score.notation_size() == original_size - result.removed.total());
}

TEST_CASE("projection erases every ignored extra and renormalizes empty measures",
    "[evaluation-profile]")
{
    SymScore score = RichScore();
    SymMeasure ignored_only;
    ignored_only.vrv_id = "ignored-only";
    SymExtra slur;
    slur.kind = ExtraKind::kSlur;
    slur.vrv_id = "slur-only";
    slur.duration = Fraction(1);
    ignored_only.extras.push_back(slur);
    score.parts[0].bar_list.insert(score.parts[0].bar_list.begin(), ignored_only);

    EvaluationFeatureDispositions dispositions = CompleteProfile().dispositions();
    for (const EvaluationFeatureSpec &spec : EvaluationFeatureRegistryV1()) {
        if (spec.capability == EvaluationFeatureCapability::kProjectable
            && spec.extra_kind.has_value()) {
            dispositions[EvaluationFeatureIndex(spec.feature)] = FeatureDisposition::kIgnored;
        }
    }
    std::string error;
    auto profile = MakeEvaluationProfileV1(
        "ignore.extras", "sha256:test", dispositions, error);
    REQUIRE(profile.has_value());

    const long original_size = score.notation_size();
    ProjectionResult once = ProjectSymScore(score, *profile);
    REQUIRE(once.score.parts.size() == 1);
    REQUIRE(once.score.parts[0].bar_list.size() == 1);
    CHECK(once.score.parts[0].bar_list[0].vrv_id == "measure");
    CHECK(once.score.parts[0].bar_list[0].extras.empty());
    CHECK(once.score.notation_size() == original_size - once.removed.total());
    CHECK(once.removed[EvaluationFeature::kSlur] == 2);

    ProjectionResult twice = ProjectSymScore(once.score, *profile);
    CHECK(twice.removed.total() == 0);
    CHECK(twice.score.notation_size() == once.score.notation_size());
    CHECK(CompareScores(once.score, twice.score).cost == 0);
}

TEST_CASE("profile validation fails closed for dispositions and schema drift",
    "[evaluation-profile]")
{
    EvaluationFeatureDispositions dispositions = CompleteProfile().dispositions();
    std::string error;

    dispositions[EvaluationFeatureIndex(EvaluationFeature::kPitch)]
        = FeatureDisposition::kIgnored;
    CHECK_FALSE(MakeEvaluationProfileV1("bad", "sha256:test", dispositions, error).has_value());
    CHECK(error.find("note.pitch") != std::string::npos);

    dispositions = CompleteProfile().dispositions();
    dispositions[EvaluationFeatureIndex(EvaluationFeature::kOrnament)]
        = FeatureDisposition::kScored;
    error.clear();
    CHECK_FALSE(MakeEvaluationProfileV1("bad", "sha256:test", dispositions, error).has_value());
    CHECK(error.find("note.ornament") != std::string::npos);
}

TEST_CASE("profile loader validates exact v1 JSON and hashes exact bytes",
    "[evaluation-profile]")
{
    const std::string valid = ValidProfileJson();
    const std::filesystem::path path = WriteProfile("valid", valid);
    std::string error;
    std::shared_ptr<const EvaluationProfile> profile = LoadEvaluationProfileFile(path.string(), error);
    REQUIRE(profile != nullptr);
    CHECK(error.empty());
    CHECK(profile->id() == "test.profile");
    CHECK(profile->digest().starts_with("sha256:"));
    CHECK(profile->digest().size() == 71);

    const std::filesystem::path whitespace_path = WriteProfile("whitespace", valid + "\n");
    std::shared_ptr<const EvaluationProfile> whitespace
        = LoadEvaluationProfileFile(whitespace_path.string(), error);
    REQUIRE(whitespace != nullptr);
    CHECK(whitespace->digest() != profile->digest());

    std::filesystem::remove(path);
    std::filesystem::remove(whitespace_path);
}

TEST_CASE("profile loader rejects missing unknown duplicate and invalid features",
    "[evaluation-profile]")
{
    const std::string valid = ValidProfileJson();
    const std::array<std::pair<std::string, std::string>, 5> cases = { {
        { "unknown", valid.substr(0, valid.size() - 1) + ",\"unknown\":1}" },
        { "duplicate", "{\"schema\":\"verosim.evaluation-profile\",\"schema\":\"verosim.evaluation-profile\",\"schema_version\":1,\"id\":\"x\",\"features\":{}}" },
        { "missing", "{\"schema\":\"verosim.evaluation-profile\",\"schema_version\":1,\"id\":\"x\",\"features\":{}}" },
        { "version", "{\"schema\":\"verosim.evaluation-profile\",\"schema_version\":2,\"id\":\"x\",\"features\":{}}" },
        { "type", "{\"schema\":\"verosim.evaluation-profile\",\"schema_version\":1,\"id\":1,\"features\":{}}" },
    } };
    for (const auto &[name, json] : cases) {
        const std::filesystem::path path = WriteProfile(name, json);
        std::string error;
        CHECK(LoadEvaluationProfileFile(path.string(), error) == nullptr);
        CHECK_FALSE(error.empty());
        std::filesystem::remove(path);
    }
}
