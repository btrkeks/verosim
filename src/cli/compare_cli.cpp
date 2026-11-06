#include "verosim/cli/compare_cli.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "verosim/app/evaluation_profile_loader.h"
#include "verosim/app/score_pipeline.h"
#include "verosim/engine/compare.h"
#include "verosim/cli/alignment_trace_json.h"
#include "verosim/extraction/vrv_bridge.h"
#include "verosim/support/json_util.h"

namespace verosim {

namespace {

std::string ReadTraceInput(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot read trace input " + path);
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (input.bad()) throw std::runtime_error("failed reading trace input " + path);
    return bytes.str();
}

void WriteBase64(const std::string &bytes, std::ostream &out)
{
    constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    out << '"';
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const auto a = static_cast<unsigned char>(bytes[i]);
        const auto b = i + 1 < bytes.size() ? static_cast<unsigned char>(bytes[i + 1]) : 0;
        const auto c = i + 2 < bytes.size() ? static_cast<unsigned char>(bytes[i + 2]) : 0;
        out << alphabet[a >> 2] << alphabet[((a & 3) << 4) | (b >> 4)]
            << (i + 1 < bytes.size() ? alphabet[((b & 15) << 2) | (c >> 6)] : '=')
            << (i + 2 < bytes.size() ? alphabet[c & 63] : '=');
    }
    out << '"';
}

void WriteTraceFile(const LoadedScore &pred, const LoadedScore &gt,
    const ScoreComparison &comparison, const CompareCliOptions &options,
    const std::string &pred_path, const std::string &gt_path)
{
    const SymScore &prediction = comparison.projected_scores ? comparison.projected_scores->pred : pred.score;
    const SymScore &ground_truth = comparison.projected_scores ? comparison.projected_scores->gt : gt.score;
    std::ofstream trace(options.trace_json_path, std::ios::binary);
    if (!trace) throw std::runtime_error("cannot write trace " + options.trace_json_path);
    trace << "{\"schema_version\":2,\"options\":{\"note_position\":\""
          << NotePositionPolicyName(options.note_position_policy) << "\",\"alignment\":\""
          << AlignmentPolicyName(options.alignment_policy) << "\",\"mode\":\""
          << MetricModeName(options.surface.mode) << "\",\"layout\":\""
          << LayoutSurfaceName(options.surface.layout) << "\",\"typed_space_handling\":\""
          << TypedSpaceHandlingName(options.typed_space_handling) << "\",\"display_staff\":"
          << (options.surface.score_display_staff ? "true" : "false") << ",\"detailed_measures\":[";
    bool first = true;
    for (const int index : options.trace_measures) {
        if (!first) trace << ',';
        first = false;
        trace << index;
    }
    trace << "]},\"inputs\":{\"prediction\":{\"path\":";
    WriteJsonString(pred_path, trace);
    trace << ",\"encoding\":\"base64\",\"data\":";
    WriteBase64(ReadTraceInput(pred_path), trace);
    trace << "},\"ground_truth\":{\"path\":";
    WriteJsonString(gt_path, trace);
    trace << ",\"encoding\":\"base64\",\"data\":";
    WriteBase64(ReadTraceInput(gt_path), trace);
    trace << "}},\"distance\":" << comparison.diff.cost;
    if (comparison.evaluation_audit) {
        trace << ",\"evaluation_profile\":";
        WriteEvaluationAuditJson(*comparison.evaluation_audit, trace);
    }
    trace << ',';
    WriteAlignmentTraceJson(prediction, ground_truth, *comparison.alignment_trace, trace);
    std::vector<std::string> warnings;
    AppendPrefixedWarnings(warnings, "pred: ", pred.warnings);
    AppendPrefixedWarnings(warnings, "gt: ", gt.warnings);
    trace << ",\"warnings\":";
    WriteJsonStringArray(warnings, trace);
    trace << "}\n";
    trace.close();
    if (!trace) throw std::runtime_error("failed writing trace " + options.trace_json_path);
}

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
    const std::chrono::steady_clock::time_point &start, std::ostream &out,
    const std::string &pred_path = {}, const std::string &gt_path = {})
{
    const char *field_sep = json_prefix == "{" ? "" : ",";
    if (!pred.ok || !gt.ok) {
        out << json_prefix;
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
    if (comparison.alignment_trace) {
        WriteTraceFile(pred, gt, comparison, options, pred_path, gt_path);
    }
    const double runtime
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    out << json_prefix << field_sep << "\"ok\":true,\"error\":null,\"distance\":" << comparison.diff.cost
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
    if (!options.trace_json_path.empty()) {
        for (const std::string &input : { pred_path, gt_path, options.evaluation_profile_path }) {
            if (input.empty()) continue;
            if (std::filesystem::weakly_canonical(options.trace_json_path)
                    == std::filesystem::weakly_canonical(input)
                || (std::filesystem::exists(options.trace_json_path)
                    && std::filesystem::exists(input)
                    && std::filesystem::equivalent(options.trace_json_path, input))) {
                throw std::invalid_argument("--trace-json must not overwrite a score or evaluation profile");
            }
        }
    }
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
    return WriteComparisonJson(pred, gt, options, prefix.str(), start, out, pred_path, gt_path);
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
    if (!options.trace_json_path.empty()) {
        throw std::invalid_argument("--trace-json requires a file-pair comparison");
    }
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
