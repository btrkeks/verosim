#include "verosim/app/score_pipeline.h"

#include <exception>
#include <utility>

#include "verosim/extraction/vrv_bridge.h"

namespace verosim {
namespace {

SourceFormat SourceFormatFromBridge(const VrvBridge &bridge)
{
    switch (bridge.last_input_format()) {
        case vrv::HUMDRUM: return SourceFormat::kKern;
        case vrv::MUSICXML:
        case vrv::MUSICXMLHUM: return SourceFormat::kMusicXml;
        default: return SourceFormat::kOther;
    }
}

vrv::FileFormat VrvFormat(ScoreDataFormat format)
{
    switch (format) {
        case ScoreDataFormat::kKern: return vrv::HUMDRUM;
        case ScoreDataFormat::kMusicXml: return vrv::MUSICXML;
        case ScoreDataFormat::kMei: return vrv::MEI;
        case ScoreDataFormat::kAuto: return vrv::UNKNOWN;
    }
    return vrv::UNKNOWN;
}

ScoreDataFormat ScoreFormatFromBridge(const VrvBridge &bridge, ScoreDataFormat fallback)
{
    switch (bridge.last_input_format()) {
        case vrv::HUMDRUM: return ScoreDataFormat::kKern;
        case vrv::MUSICXML:
        case vrv::MUSICXMLHUM: return ScoreDataFormat::kMusicXml;
        case vrv::MEI: return ScoreDataFormat::kMei;
        default: return fallback;
    }
}

} // namespace

std::optional<ScoreDataFormat> ParseScoreDataFormat(const std::string &value)
{
    if (value == "kern" || value == "humdrum") return ScoreDataFormat::kKern;
    if (value == "musicxml") return ScoreDataFormat::kMusicXml;
    if (value == "mei") return ScoreDataFormat::kMei;
    if (value == "auto") return ScoreDataFormat::kAuto;
    return std::nullopt;
}

const char *ScoreDataFormatName(ScoreDataFormat format)
{
    switch (format) {
        case ScoreDataFormat::kKern: return "kern";
        case ScoreDataFormat::kMusicXml: return "musicxml";
        case ScoreDataFormat::kMei: return "mei";
        case ScoreDataFormat::kAuto: return "auto";
    }
    return "auto";
}

ExtractOptions ExtractOptionsForRun(const CompareRunOptions &options)
{
    return ExtractOptions{ .surface = options.surface,
        .typed_space_handling = options.typed_space_handling };
}

CompareOptions EngineOptionsForRun(const CompareRunOptions &options)
{
    return CompareOptions{ .note_position_policy = options.note_position_policy };
}

LoadedScore LoadAndExtractScoreFile(
    VrvBridge &bridge, const std::string &path, const ExtractOptions &options)
{
    LoadedScore loaded;
    // Verovio throws on some adversarial inputs (the PERF-10K sweep's known
    // failure modes, e.g. vector::_M_default_append). Surface that as data so
    // batch callers can keep producing complete JSONL records.
    try {
        bridge.set_typed_space_handling(options.typed_space_handling);
        if (!bridge.LoadScoreFile(path)) {
            loaded.error = "failed to load " + path;
            return loaded;
        }
        ExtractResult result = ExtractSymScore(bridge.GetDoc(), SourceFormatFromBridge(bridge), options);
        loaded.score = std::move(result.score);
        loaded.warnings = std::move(result.warnings);
        loaded.ok = true;
    }
    catch (const std::exception &e) {
        loaded.error = "exception loading " + path + ": " + e.what();
    }
    return loaded;
}

LoadedScore LoadAndExtractKernData(VrvBridge &bridge, const std::string &data,
    const std::string &label, const ExtractOptions &options)
{
    return LoadAndExtractScoreData(bridge, data, ScoreDataFormat::kKern, label, options);
}

LoadedScore LoadAndExtractScoreData(VrvBridge &bridge, const std::string &data,
    ScoreDataFormat format, const std::string &label, const ExtractOptions &options)
{
    LoadedScore loaded;
    loaded.format = format;
    try {
        bridge.set_typed_space_handling(options.typed_space_handling);
        if (!bridge.LoadScoreData(data, VrvFormat(format))) {
            loaded.format = ScoreFormatFromBridge(bridge, format);
            loaded.error = "failed to load " + label;
            return loaded;
        }
        loaded.format = ScoreFormatFromBridge(bridge, format);
        ExtractResult result = ExtractSymScore(bridge.GetDoc(), SourceFormatFromBridge(bridge), options);
        loaded.score = std::move(result.score);
        loaded.warnings = std::move(result.warnings);
        loaded.ok = true;
    }
    catch (const std::exception &e) {
        loaded.error = "exception loading " + label + ": " + e.what();
    }
    return loaded;
}

ScoreComparison CompareLoadedScores(
    const SymScore &pred, const SymScore &gt, const CompareRunOptions &options)
{
    ScoreComparison comparison;
    comparison.diff = CompareScores(pred, gt, EngineOptionsForRun(options));
    comparison.n_pred = pred.notation_size();
    comparison.n_gt = gt.notation_size();
    comparison.omr_ned = OmrNed(comparison.diff.cost, comparison.n_pred, comparison.n_gt);
    comparison.edit_distances = EditDistancesDict(comparison.diff.op_list);
    return comparison;
}

void AppendPrefixedWarnings(std::vector<std::string> &out, const std::string &prefix,
    const std::vector<std::string> &warnings)
{
    for (const std::string &warning : warnings) out.push_back(prefix + warning);
}

} // namespace verosim
