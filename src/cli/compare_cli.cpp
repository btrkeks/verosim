#include "verosim/cli/compare_cli.h"

#include <chrono>
#include <cstdio>
#include <ostream>
#include <vector>

#include "verosim/cli/json_util.h"
#include "verosim/engine/compare.h"
#include "verosim/extraction/extract.h"
#include "verosim/extraction/vrv_bridge.h"

namespace verosim {

namespace {

void WriteDouble(double value, std::ostream &out)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.12g", value);
    out << buf;
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

struct LoadedScore {
    SymScore score;
    std::vector<std::string> warnings;
    std::string error;
    bool ok = false;
};

LoadedScore LoadAndExtract(
    VrvBridge &bridge, const std::string &path, const ExtractOptions &options)
{
    LoadedScore loaded;
    // Verovio throws on some adversarial inputs (the PERF-10K sweep's known
    // failure modes, e.g. vector::_M_default_append) — a pair must fail as a
    // record, never abort a --pairs run.
    try {
        if (!bridge.LoadScoreFile(path)) {
            loaded.error = "failed to load " + path;
            return loaded;
        }
        const SourceFormat format = bridge.last_input_format() == vrv::HUMDRUM
            ? SourceFormat::kKern
            : SourceFormat::kMusicXml;
        ExtractResult result = ExtractSymScore(bridge.GetDoc(), format, options);
        loaded.score = std::move(result.score);
        loaded.warnings = std::move(result.warnings);
        loaded.ok = true;
    }
    catch (const std::exception &e) {
        loaded.error = "exception loading " + path + ": " + e.what();
    }
    return loaded;
}

} // namespace

bool ComparePairToJson(VrvBridge &bridge, const std::string &pred_path,
    const std::string &gt_path, const CompareCliOptions &options, std::ostream &out)
{
    const auto start = std::chrono::steady_clock::now();

    // One bridge, two loads: SymScore is self-contained, so the first
    // extraction survives the second load.
    const ExtractOptions extract_options{
        .include_tier_b = options.include_tier_b,
        .include_directions = options.include_directions
    };
    const LoadedScore pred = LoadAndExtract(bridge, pred_path, extract_options);
    const LoadedScore gt = pred.ok ? LoadAndExtract(bridge, gt_path, extract_options) : LoadedScore{};

    const auto write_header = [&] {
        out << "{\"pair\":{\"pred\":";
        WriteJsonString(pred_path, out);
        out << ",\"gt\":";
        WriteJsonString(gt_path, out);
        out << "}";
    };

    if (!pred.ok || !gt.ok) {
        write_header();
        out << ",\"ok\":false,\"error\":";
        WriteJsonString(!pred.ok ? pred.error : gt.error, out);
        out << ",\"distance\":null,\"n_pred\":null,\"n_gt\":null,\"omr_ned\":null,"
               "\"edit_distances_dict\":null,\"warnings\":[],\"runtime_s\":0}\n";
        return false;
    }

    const CompareResult result = CompareScores(pred.score, gt.score);
    const long n_pred = pred.score.notation_size();
    const long n_gt = gt.score.notation_size();
    const std::map<std::string, long> dict = EditDistancesDict(result.op_list);
    const double runtime
        = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    write_header();
    out << ",\"ok\":true,\"error\":null,\"distance\":" << result.cost << ",\"n_pred\":" << n_pred
        << ",\"n_gt\":" << n_gt << ",\"omr_ned\":";
    WriteDouble(OmrNed(result.cost, n_pred, n_gt), out);
    out << ",\"edit_distances_dict\":{";
    bool first = true;
    for (const auto &[name, value] : dict) {
        if (!first) out << ",";
        first = false;
        WriteJsonString(name, out);
        out << ":" << value;
    }
    out << "}";
    if (options.emit_ops) {
        out << ",\"edit_ops\":";
        WriteEditOps(result.op_list, out);
    }
    std::vector<std::string> warnings;
    for (const std::string &w : pred.warnings) warnings.push_back("pred: " + w);
    for (const std::string &w : gt.warnings) warnings.push_back("gt: " + w);
    out << ",\"warnings\":";
    WriteJsonStringArray(warnings, out);
    out << ",\"runtime_s\":";
    WriteDouble(runtime, out);
    out << "}\n";
    return true;
}

} // namespace verosim
