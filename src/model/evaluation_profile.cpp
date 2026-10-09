#include "verosim/model/evaluation_profile.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <numeric>
#include <stdexcept>

namespace verosim {
namespace {

constexpr std::array<EvaluationFeatureSpec, kEvaluationFeatureCount> kRegistry = { {
    { EvaluationFeature::kPitch, "note.pitch", EvaluationFeatureCapability::kRequiredScored,
        std::nullopt },
    { EvaluationFeature::kAccidental, "note.accidental", EvaluationFeatureCapability::kProjectable,
        std::nullopt },
    { EvaluationFeature::kTie, "note.tie", EvaluationFeatureCapability::kProjectable, std::nullopt },
    { EvaluationFeature::kNotehead, "note.notehead", EvaluationFeatureCapability::kRequiredScored,
        std::nullopt },
    { EvaluationFeature::kDot, "note.dot", EvaluationFeatureCapability::kRequiredScored,
        std::nullopt },
    { EvaluationFeature::kBeam, "note.beam", EvaluationFeatureCapability::kProjectable,
        std::nullopt },
    { EvaluationFeature::kTuplet, "note.tuplet", EvaluationFeatureCapability::kRequiredScored,
        std::nullopt },
    { EvaluationFeature::kTupletInfo, "note.tuplet_info",
        EvaluationFeatureCapability::kRequiredScored, std::nullopt },
    { EvaluationFeature::kGrace, "note.grace", EvaluationFeatureCapability::kRequiredScored,
        std::nullopt },
    { EvaluationFeature::kGraceSlash, "note.grace_slash",
        EvaluationFeatureCapability::kProjectable, std::nullopt },
    { EvaluationFeature::kGap, "note.gap", EvaluationFeatureCapability::kProjectable,
        std::nullopt },
    { EvaluationFeature::kArticulation, "note.articulation",
        EvaluationFeatureCapability::kProjectable, std::nullopt },
    { EvaluationFeature::kExpression, "note.expression",
        EvaluationFeatureCapability::kProjectable, std::nullopt },
    { EvaluationFeature::kArpeggio, "extra.arpeggio", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kArpeggio },
    { EvaluationFeature::kBarline, "extra.barline", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kBarline },
    { EvaluationFeature::kClef, "extra.clef", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kClef },
    { EvaluationFeature::kCrescendo, "extra.hairpin.crescendo",
        EvaluationFeatureCapability::kProjectable, ExtraKind::kCrescendo },
    { EvaluationFeature::kDiminuendo, "extra.hairpin.diminuendo",
        EvaluationFeatureCapability::kProjectable, ExtraKind::kDiminuendo },
    { EvaluationFeature::kDynamic, "extra.dynamic", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kDynamic },
    { EvaluationFeature::kKeySig, "extra.keysig", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kKeySig },
    { EvaluationFeature::kOttava, "extra.ottava", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kOttava },
    { EvaluationFeature::kRepeat, "extra.repeat", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kRepeat },
    { EvaluationFeature::kSlur, "extra.slur", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kSlur },
    { EvaluationFeature::kTimeSig, "extra.timesig", EvaluationFeatureCapability::kProjectable,
        ExtraKind::kTimeSig },
    { EvaluationFeature::kSystemBreak, "layout.system_break",
        EvaluationFeatureCapability::kProjectable, ExtraKind::kSystemBreak },
    { EvaluationFeature::kOrnament, "note.ornament",
        EvaluationFeatureCapability::kKnownUnsupported, std::nullopt },
    { EvaluationFeature::kStyle, "note.style", EvaluationFeatureCapability::kKnownUnsupported,
        std::nullopt },
    { EvaluationFeature::kLyric, "note.lyric", EvaluationFeatureCapability::kKnownUnsupported,
        std::nullopt },
    { EvaluationFeature::kFingering, "note.fingering",
        EvaluationFeatureCapability::kKnownUnsupported, std::nullopt },
    { EvaluationFeature::kText, "extra.text", EvaluationFeatureCapability::kKnownUnsupported,
        std::nullopt },
    { EvaluationFeature::kPedal, "extra.pedal", EvaluationFeatureCapability::kKnownUnsupported,
        std::nullopt },
    { EvaluationFeature::kPageBreak, "layout.page_break",
        EvaluationFeatureCapability::kKnownUnsupported, std::nullopt },
} };

bool ValidProfileId(const std::string &id)
{
    if (id.empty() || id.size() > 128) return false;
    return std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '.' || c == '_' || c == '-' || c == '/' || c == ':';
    });
}

void RemoveIgnoredNoteFeatures(
    SymNote &note, const EvaluationProfile &profile, EvaluationFeatureCounts &removed)
{
    for (SymPitch &pitch : note.pitches) {
        if (profile.disposition(EvaluationFeature::kAccidental) == FeatureDisposition::kIgnored
            && pitch.accid != "None") {
            ++removed[EvaluationFeature::kAccidental];
            pitch.accid = "None";
        }
        if (profile.disposition(EvaluationFeature::kTie) == FeatureDisposition::kIgnored
            && pitch.tie) {
            ++removed[EvaluationFeature::kTie];
            pitch.tie = false;
        }
    }
    if (profile.disposition(EvaluationFeature::kBeam) == FeatureDisposition::kIgnored) {
        removed[EvaluationFeature::kBeam] += static_cast<long>(note.beamings.size());
        note.beamings.clear();
    }
    if (profile.disposition(EvaluationFeature::kGraceSlash) == FeatureDisposition::kIgnored
        && note.grace_slash) {
        ++removed[EvaluationFeature::kGraceSlash];
        note.grace_slash = false;
    }
    if (profile.disposition(EvaluationFeature::kGap) == FeatureDisposition::kIgnored
        && note.gap_dur != Fraction(0)) {
        ++removed[EvaluationFeature::kGap];
        note.gap_dur = Fraction(0);
    }
    if (profile.disposition(EvaluationFeature::kArticulation) == FeatureDisposition::kIgnored) {
        removed[EvaluationFeature::kArticulation]
            += static_cast<long>(note.articulations.size());
        note.articulations.clear();
    }
    if (profile.disposition(EvaluationFeature::kExpression) == FeatureDisposition::kIgnored) {
        removed[EvaluationFeature::kExpression]
            += static_cast<long>(note.expressions.size());
        note.expressions.clear();
    }
}

} // namespace

std::span<const EvaluationFeatureSpec> EvaluationFeatureRegistryV1()
{
    return kRegistry;
}

const EvaluationFeatureSpec &EvaluationFeatureSpecFor(EvaluationFeature feature)
{
    const std::size_t index = EvaluationFeatureIndex(feature);
    if (index >= kRegistry.size()) throw std::out_of_range("invalid evaluation feature");
    return kRegistry[index];
}

std::optional<EvaluationFeature> EvaluationFeatureFromKey(std::string_view key)
{
    for (const EvaluationFeatureSpec &spec : kRegistry) {
        if (spec.key == key) return spec.feature;
    }
    return std::nullopt;
}

std::optional<EvaluationFeature> EvaluationFeatureForExtraKind(ExtraKind kind)
{
    for (const EvaluationFeatureSpec &spec : kRegistry) {
        if (spec.extra_kind == kind) return spec.feature;
    }
    return std::nullopt;
}

std::string_view FeatureDispositionName(FeatureDisposition disposition)
{
    switch (disposition) {
        case FeatureDisposition::kScored: return "scored";
        case FeatureDisposition::kIgnored: return "ignored";
        case FeatureDisposition::kUnsupported: return "unsupported";
    }
    return "?";
}

std::optional<FeatureDisposition> ParseFeatureDisposition(std::string_view text)
{
    if (text == "scored") return FeatureDisposition::kScored;
    if (text == "ignored") return FeatureDisposition::kIgnored;
    if (text == "unsupported") return FeatureDisposition::kUnsupported;
    return std::nullopt;
}

std::optional<EvaluationProfile> MakeEvaluationProfileV1(const std::string &id,
    const std::string &digest, const EvaluationFeatureDispositions &dispositions,
    std::string &error)
{
    error.clear();
    if (!ValidProfileId(id)) {
        error = "profile id must be 1-128 characters using letters, digits, '.', '_', '-', '/', or ':'";
        return std::nullopt;
    }
    for (const EvaluationFeatureSpec &spec : kRegistry) {
        const FeatureDisposition disposition = dispositions[EvaluationFeatureIndex(spec.feature)];
        const bool valid = spec.capability == EvaluationFeatureCapability::kProjectable
            ? disposition == FeatureDisposition::kScored
                || disposition == FeatureDisposition::kIgnored
            : spec.capability == EvaluationFeatureCapability::kRequiredScored
            ? disposition == FeatureDisposition::kScored
            : disposition == FeatureDisposition::kUnsupported;
        if (!valid) {
            error = "feature " + std::string(spec.key) + " is "
                + (spec.capability == EvaluationFeatureCapability::kRequiredScored
                        ? "required and must be 'scored'"
                        : spec.capability == EvaluationFeatureCapability::kKnownUnsupported
                        ? "not supported by schema v1 and must be 'unsupported'"
                        : "projectable and must be 'scored' or 'ignored'");
            return std::nullopt;
        }
    }
    EvaluationProfile profile;
    profile.id_ = id;
    profile.digest_ = digest;
    profile.dispositions_ = dispositions;
    return profile;
}

long EvaluationFeatureCounts::total() const
{
    return std::accumulate(values_.begin(), values_.end(), 0L);
}

ProjectionResult ProjectSymScore(const SymScore &score, const EvaluationProfile &profile)
{
    ProjectionResult result{ .score = score };
    for (SymPart &part : result.score.parts) {
        for (SymMeasure &measure : part.bar_list) {
            for (SymNote &note : measure.notes) {
                if (note.display_staff_offset != 0) {
                    throw std::invalid_argument(
                        "evaluation profile schema v1 cannot project nonzero display staff placement");
                }
                RemoveIgnoredNoteFeatures(note, profile, result.removed);
            }
            std::erase_if(measure.extras, [&](const SymExtra &extra) {
                const std::optional<EvaluationFeature> feature
                    = EvaluationFeatureForExtraKind(extra.kind);
                if (!feature.has_value()) {
                    throw std::invalid_argument("schema v1 does not classify extracted extra kind "
                        + std::string(ExtraKindName(extra.kind)));
                }
                if (profile.disposition(*feature) != FeatureDisposition::kIgnored) return false;
                result.removed[*feature] += extra.notation_size();
                return true;
            });
        }
        std::erase_if(part.bar_list,
            [](const SymMeasure &measure) { return measure.n_of_elements() == 0; });
    }
    return result;
}

} // namespace verosim
