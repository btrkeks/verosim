#include "verosim/cli/compare_cli.h"

#include <chrono>
#include <ostream>
#include <sstream>
#include <vector>

#include "verosim/app/evaluation_profile_loader.h"
#include "verosim/app/score_pipeline.h"
#include "verosim/engine/compare.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/support/json_util.h"

namespace verosim {

namespace {

// Mirrors oracle.py:_serialize_op_side {type, repr, ...}: enough identity for
// op-level triage diffs against the stored oracle edit_ops. The note repr is
// the byte-compatible SymNote::repr().
void WriteOpSide(const OpSide &side, std::ostream &out)
{
    switch (side.kind) {
        case OpSide::Kind::kNone: out << "null"; return;
        case OpSide::Kind::kNote:
            out << "{\"type\":\"AnnNote\",\"repr\":";
            WriteJsonString(side.note->repr(), out);
            out << ",\"id\":";
            WriteJsonString(side.note->vrv_id, out);
            break;
        case OpSide::Kind::kExtra:
            out << "{\"type\":\"AnnExtra\",\"repr\":";
            WriteJsonString("Extra(" + side.extra->vrv_id + "):" + side.extra->str(), out);
            out << ",\"id\":";
            WriteJsonString(side.extra->vrv_id, out);
            break;
        case OpSide::Kind::kMeasure:
            out << "{\"type\":\"AnnMeasure\",\"repr\":";
            WriteJsonString("Measure(" + side.measure->vrv_id + ") n=" + side.measure->measure_n,
                out);
            out << ",\"id\":";
            WriteJsonString(side.measure->vrv_id, out);
            break;
        case OpSide::Kind::kPart:
            out << "{\"type\":\"AnnPart\",\"repr\":";
            WriteJsonString("Part(staff " + side.part->staff_n + ")", out);
            out << ",\"id\":";
            WriteJsonString(side.part->staff_n, out);
            break;
    }
    out << "}";
}

void WriteEditOps(const std::vector<EditOp> &ops, std::ostream &out)
{
    out << "[";
    for (std::size_t i = 0; i < ops.size(); ++i) {
        const EditOp &op = ops[i];
        if (i > 0) out << ",";
        out << "{\"op\":\"" << OpNameStr(op.name) << "\",\"cost\":" << op.cost << ",\"a\":";
        WriteOpSide(op.a, out);
        out << ",\"b\":";
        WriteOpSide(op.b, out);
        out << ",\"ids\":";
        switch (op.ids_kind) {
            case EditOp::IdsKind::kNone: out << "null"; break;
            case EditOp::IdsKind::kPitchPair:
                out << "[" << op.ids0 << "," << op.ids1 << "]";
                break;
            case EditOp::IdsKind::kChordIdx: out << op.ids0; break;
        }
        out << "}";
    }
    out << "]";
}

std::string AppendDataProvenance(
    const std::string &json_prefix, const LoadedScore &pred, const LoadedScore &gt)
{
    std::ostringstream out;
    out << json_prefix << (json_prefix == "{" ? "" : ",") << "\"pred_format\":";
    WriteJsonString(ScoreDataFormatName(pred.format), out);
    out << ",\"gt_format\":";
    WriteJsonString(ScoreDataFormatName(gt.format), out);
    out << ",\"pred_parser\":";
    WriteJsonString(pred.parser, out);
    out << ",\"gt_parser\":";
    WriteJsonString(gt.parser, out);
    return out.str();
}

bool WriteComparisonJson(const LoadedScore &pred, const LoadedScore &gt,
    const CompareCliOptions &options, const std::string &json_prefix,
    const std::chrono::steady_clock::time_point &start, std::ostream &out)
{
    out << json_prefix;
    const char *field_sep = json_prefix == "{" ? "" : ",";
    if (!pred.ok || !gt.ok) {
        out << field_sep << "\"ok\":false,\"error\":";
        WriteJsonString(!pred.ok ? pred.error : gt.error, out);
        out << ",\"distance\":null,\"n_pred\":null,\"n_gt\":null,\"omr_ned\":null,"
               "\"edit_distances_dict\":null";
        if (options.evaluation_profile) {
            const EvaluationAudit audit = AuditAvailableScores(*options.evaluation_profile,
                pred.ok ? &pred.score : nullptr, gt.ok ? &gt.score : nullptr);
            out << ",\"evaluation_profile\":";
            WriteEvaluationAuditJson(audit, out);
        }
        out << ",\"warnings\":[],\"runtime_s\":0}\n";
        return false;
    }

    const CompareRunOptions run_options = RunOptionsForCli(options);
    const ScoreComparison comparison = CompareLoadedScores(pred.score, gt.score, run_options);
    const double runtime
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    out << field_sep << "\"ok\":true,\"error\":null,\"distance\":" << comparison.diff.cost
        << ",\"n_pred\":" << comparison.n_pred << ",\"n_gt\":" << comparison.n_gt << ",\"omr_ned\":";
    WriteDouble(comparison.omr_ned, out);
    out << ",\"edit_distances_dict\":{";
    bool first = true;
    for (const auto &[name, value] : comparison.edit_distances) {
        if (!first) out << ",";
        first = false;
        WriteJsonString(name, out);
        out << ":" << value;
    }
    out << "}";
    if (options.emit_ops) {
        out << ",\"edit_ops\":";
        WriteEditOps(comparison.diff.op_list, out);
    }
    if (comparison.evaluation_audit.has_value()) {
        out << ",\"evaluation_profile\":";
        WriteEvaluationAuditJson(*comparison.evaluation_audit, out);
    }
    std::vector<std::string> warnings;
    AppendPrefixedWarnings(warnings, "pred: ", pred.warnings);
    AppendPrefixedWarnings(warnings, "gt: ", gt.warnings);
    out << ",\"warnings\":";
    WriteJsonStringArray(warnings, out);
    out << ",\"runtime_s\":";
    WriteDouble(runtime, out);
    out << "}\n";
    return true;
}

} // namespace

bool PrepareEvaluationProfile(CompareCliOptions &options, std::string &error)
{
    error.clear();
    if (options.surface.score_display_staff
        && (!options.evaluation_profile_path.empty() || options.evaluation_profile)) {
        error = kDisplayStaffProfileConflict;
        return false;
    }
    if (options.evaluation_profile_path.empty()) return true;
    if (options.evaluation_profile) return true;
    options.evaluation_profile
        = LoadEvaluationProfileFile(options.evaluation_profile_path, error);
    return options.evaluation_profile != nullptr;
}

bool ComparePairToJson(VrvBridge &bridge, const std::string &pred_path,
    const std::string &gt_path, const CompareCliOptions &options, std::ostream &out)
{
    const auto start = std::chrono::steady_clock::now();

    // One bridge, two loads: SymScore is self-contained, so the first
    // extraction survives the second load.
    ExtractOptions extract_options = ExtractOptionsForRun(RunOptionsForCli(options));
    extract_options.humdrum_timing_trusted
        = bridge.HumdrumTimingIsCleanFile(pred_path) && bridge.HumdrumTimingIsCleanFile(gt_path);
    const LoadedScore pred = LoadAndExtractScoreFile(bridge, pred_path, extract_options);
    const LoadedScore gt
        = pred.ok ? LoadAndExtractScoreFile(bridge, gt_path, extract_options) : LoadedScore{};

    std::ostringstream prefix;
    prefix << "{\"pair\":{\"pred\":";
    WriteJsonString(pred_path, prefix);
    prefix << ",\"gt\":";
    WriteJsonString(gt_path, prefix);
    prefix << "}";
    return WriteComparisonJson(pred, gt, options, prefix.str(), start, out);
}

bool CompareScoreDataToJson(VrvBridge &bridge, const std::string &pred_data,
    const std::string &gt_data, const CompareCliOptions &options, const std::string &json_prefix,
    std::ostream &out)
{
    return CompareScoreDataToJson(bridge, pred_data, gt_data, ScoreDataFormat::kKern,
        ScoreDataFormat::kKern, "prediction", "target", options, json_prefix, out);
}

bool CompareScoreDataToJson(VrvBridge &bridge, const std::string &pred_data,
    const std::string &gt_data, ScoreDataFormat pred_format, ScoreDataFormat gt_format,
    const std::string &pred_label, const std::string &gt_label, const CompareCliOptions &options,
    const std::string &json_prefix, std::ostream &out)
{
    const auto start = std::chrono::steady_clock::now();
    ExtractOptions extract_options = ExtractOptionsForRun(RunOptionsForCli(options));
    extract_options.humdrum_timing_trusted = HumdrumTimingIsClean(bridge, pred_data, pred_format)
        && HumdrumTimingIsClean(bridge, gt_data, gt_format);
    const LoadedScore pred
        = LoadAndExtractScoreData(bridge, pred_data, pred_format, pred_label, extract_options);
    LoadedScore gt;
    gt.format = gt_format;
    if (pred.ok) {
        gt = LoadAndExtractScoreData(bridge, gt_data, gt_format, gt_label, extract_options);
    }
    return WriteComparisonJson(
        pred, gt, options, AppendDataProvenance(json_prefix, pred, gt), start, out);
}

} // namespace verosim
