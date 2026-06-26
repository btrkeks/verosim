#include "verosim/cli/compare_cli.h"

#include <chrono>
#include <ostream>
#include <vector>

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
               "\"edit_distances_dict\":null,\"warnings\":[],\"runtime_s\":0}\n";
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

bool ComparePairToJson(VrvBridge &bridge, const std::string &pred_path,
    const std::string &gt_path, const CompareCliOptions &options, std::ostream &out)
{
    const auto start = std::chrono::steady_clock::now();

    // One bridge, two loads: SymScore is self-contained, so the first
    // extraction survives the second load.
    const ExtractOptions extract_options = ExtractOptionsForRun(RunOptionsForCli(options));
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
    const auto start = std::chrono::steady_clock::now();
    const ExtractOptions extract_options = ExtractOptionsForRun(RunOptionsForCli(options));
    const LoadedScore pred = LoadAndExtractKernData(bridge, pred_data, "prediction", extract_options);
    const LoadedScore gt = pred.ok ? LoadAndExtractKernData(bridge, gt_data, "target", extract_options)
                                   : LoadedScore{};
    return WriteComparisonJson(pred, gt, options, json_prefix, start, out);
}

} // namespace verosim
