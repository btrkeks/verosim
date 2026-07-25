#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "verosim/model/sym_score.h"

namespace verosim {

inline constexpr std::string_view kEvaluationProfileSchema = "verosim.evaluation-profile";
inline constexpr int kEvaluationProfileSchemaVersion = 1;

// This order is the public v1 audit order. It is immutable: adding or changing
// feature support requires a new schema version rather than widening v1.
enum class EvaluationFeature : std::size_t {
    kPitch,
    kAccidental,
    kTie,
    kNotehead,
    kDot,
    kBeam,
    kTuplet,
    kTupletInfo,
    kGrace,
    kGraceSlash,
    kGap,
    kArticulation,
    kExpression,
    kArpeggio,
    kBarline,
    kClef,
    kCrescendo,
    kDiminuendo,
    kDynamic,
    kKeySig,
    kOttava,
    kRepeat,
    kSlur,
    kTimeSig,
    kSystemBreak,
    kOrnament,
    kStyle,
    kLyric,
    kFingering,
    kText,
    kPedal,
    kPageBreak,
    kCount,
};

constexpr std::size_t EvaluationFeatureIndex(EvaluationFeature feature)
{
    return static_cast<std::size_t>(feature);
}

inline constexpr std::size_t kEvaluationFeatureCount
    = EvaluationFeatureIndex(EvaluationFeature::kCount);

enum class EvaluationFeatureCapability {
    kRequiredScored,
    kProjectable,
    kKnownUnsupported,
};

enum class FeatureDisposition {
    kScored,
    kIgnored,
    kUnsupported,
};

struct EvaluationFeatureSpec {
    EvaluationFeature feature;
    std::string_view key;
    EvaluationFeatureCapability capability;
    std::optional<ExtraKind> extra_kind;
};

using EvaluationFeatureDispositions
    = std::array<FeatureDisposition, kEvaluationFeatureCount>;

std::span<const EvaluationFeatureSpec> EvaluationFeatureRegistryV1();
const EvaluationFeatureSpec &EvaluationFeatureSpecFor(EvaluationFeature feature);
std::optional<EvaluationFeature> EvaluationFeatureFromKey(std::string_view key);
std::optional<EvaluationFeature> EvaluationFeatureForExtraKind(ExtraKind kind);
std::string_view FeatureDispositionName(FeatureDisposition disposition);
std::optional<FeatureDisposition> ParseFeatureDisposition(std::string_view text);

class EvaluationProfile {
public:
    const std::string &id() const { return id_; }
    const std::string &digest() const { return digest_; }
    FeatureDisposition disposition(EvaluationFeature feature) const
    {
        return dispositions_[EvaluationFeatureIndex(feature)];
    }
    const EvaluationFeatureDispositions &dispositions() const { return dispositions_; }

private:
    friend std::optional<EvaluationProfile> MakeEvaluationProfileV1(const std::string &,
        const std::string &, const EvaluationFeatureDispositions &, std::string &);

    std::string id_;
    std::string digest_;
    EvaluationFeatureDispositions dispositions_{};
};

std::optional<EvaluationProfile> MakeEvaluationProfileV1(const std::string &id,
    const std::string &digest, const EvaluationFeatureDispositions &dispositions,
    std::string &error);

class EvaluationFeatureCounts {
public:
    long &operator[](EvaluationFeature feature)
    {
        return values_[EvaluationFeatureIndex(feature)];
    }
    long operator[](EvaluationFeature feature) const
    {
        return values_[EvaluationFeatureIndex(feature)];
    }
    const std::array<long, kEvaluationFeatureCount> &values() const { return values_; }
    long total() const;

private:
    std::array<long, kEvaluationFeatureCount> values_{};
};

struct ProjectionResult {
    SymScore score;
    EvaluationFeatureCounts removed;
};

// Pure canonical projection. Parsing, tie/accidental resolution, and all
// source-specific interpretation must already be complete.
ProjectionResult ProjectSymScore(const SymScore &score, const EvaluationProfile &profile);

} // namespace verosim
