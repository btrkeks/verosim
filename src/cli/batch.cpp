#include "verosim/cli/batch.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <future>
#include <map>
#include <sstream>
#include <thread>
#include <unordered_set>

#include <jsonxx.h>

#include "verosim/cli/args.h"
#include "verosim/cli/json_util.h"
#include "verosim/cli/list_io.h"
#include "verosim/extraction/vrv_bridge.h"

namespace verosim {

namespace {

struct JsonlScoreRecord {
    std::string pred;
    std::string gt;
    std::string prefix_json = "{";
    std::string group;
    std::string early_error;
};

struct SummaryBucket {
    long count = 0;
    long scored = 0;
    long failures = 0;
    double total = 0.0;
};

void WriteJsonValue(const jsonxx::Value &value, std::ostream &out)
{
    out << value;
}

bool IsScalar(const jsonxx::Value &value)
{
    return value.is<jsonxx::String>() || value.is<jsonxx::Number>() || value.is<jsonxx::Boolean>()
        || value.is<jsonxx::Null>();
}

void WriteObjectFieldOrNull(
    const jsonxx::Object &object, const std::string &input_field, const std::string &output_field,
    bool &first, std::ostream &out)
{
    if (!first) out << ",";
    first = false;
    WriteJsonString(output_field, out);
    out << ":";
    const auto it = object.kv_map().find(input_field);
    if (it != object.kv_map().end() && IsScalar(*it->second)) {
        WriteJsonValue(*it->second, out);
    }
    else {
        out << "null";
    }
}

std::string ScalarToKey(const jsonxx::Value &value)
{
    if (value.is<jsonxx::String>()) return value.get<jsonxx::String>();
    std::ostringstream out;
    WriteJsonValue(value, out);
    return out.str();
}

std::string Trim(const std::string &s)
{
    const std::size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    const std::size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::vector<std::string> SplitCommaList(const std::string &s)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= s.size()) {
        const std::size_t comma = s.find(',', start);
        parts.push_back(Trim(s.substr(start, comma == std::string::npos ? comma : comma - start)));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    parts.erase(std::remove(parts.begin(), parts.end(), ""), parts.end());
    return parts;
}

std::string DedupeKey(const jsonxx::Object &object, const std::vector<std::string> &fields)
{
    if (fields.empty()) return "";
    std::ostringstream key;
    bool first = true;
    for (const std::string &field : fields) {
        const auto it = object.kv_map().find(field);
        if (it == object.kv_map().end() || !IsScalar(*it->second)) return "";
        if (!first) key << '\x1f';
        first = false;
        key << ScalarToKey(*it->second);
    }
    return key.str();
}

std::string GroupValue(const jsonxx::Object &object, const std::string &field)
{
    const auto it = object.kv_map().find(field);
    if (it == object.kv_map().end() || !IsScalar(*it->second)
        || it->second->is<jsonxx::Null>()) {
        return "";
    }
    return ScalarToKey(*it->second);
}

JsonlScoreRecord MakeEarlyFailure(
    const std::string &error, const std::string &prefix_json = "{", const std::string &group = "")
{
    JsonlScoreRecord record;
    record.prefix_json = prefix_json;
    record.group = group;
    record.early_error = error;
    return record;
}

std::string FailureRecordJson(const JsonlScoreRecord &record)
{
    std::ostringstream out;
    out << record.prefix_json << (record.prefix_json == "{" ? "" : ",") << "\"ok\":false,\"error\":";
    WriteJsonString(record.early_error, out);
    out << ",\"distance\":null,\"n_pred\":null,\"n_gt\":null,\"omr_ned\":null,"
           "\"edit_distances_dict\":null,\"warnings\":[],\"runtime_s\":0}\n";
    return out.str();
}

std::string PrefixForObject(
    const jsonxx::Object &object, const BatchJsonlArgs &args, bool &has_pred, bool &has_gt)
{
    std::ostringstream prefix;
    prefix << "{";
    bool first = true;
    WriteObjectFieldOrNull(object, args.id_field, "id", first, prefix);
    WriteObjectFieldOrNull(object, args.group_field, "group", first, prefix);
    if (object.has<jsonxx::String>("source")) {
        if (!first) prefix << ",";
        first = false;
        prefix << "\"source\":";
        WriteJsonString(object.get<jsonxx::String>("source"), prefix);
    }
    has_pred = object.has<jsonxx::String>(args.pred_field);
    has_gt = object.has<jsonxx::String>(args.gt_field);
    return prefix.str();
}

std::vector<JsonlScoreRecord> ReadJsonlScoreRecords(const BatchJsonlArgs &args, std::string &error)
{
    std::ifstream in(args.jsonl_path);
    if (!in) {
        error = "cannot read " + args.jsonl_path;
        return {};
    }

    const std::vector<std::string> dedupe_fields = SplitCommaList(args.dedupe_key);
    std::unordered_set<std::string> seen;
    std::vector<JsonlScoreRecord> records;
    std::string line;
    long line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty()) continue;
        jsonxx::Object object;
        if (!object.parse(line)) {
            records.push_back(MakeEarlyFailure("invalid JSON on line " + std::to_string(line_no)));
            continue;
        }
        const std::string key = DedupeKey(object, dedupe_fields);
        if (!key.empty()) {
            const auto [_, inserted] = seen.insert(key);
            if (!inserted) continue;
        }

        bool has_pred = false;
        bool has_gt = false;
        JsonlScoreRecord record;
        record.group = GroupValue(object, args.group_field);
        record.prefix_json = PrefixForObject(object, args, has_pred, has_gt);
        if (!has_pred) {
            record.early_error = "missing or non-string field " + args.pred_field;
        }
        else if (!has_gt) {
            record.early_error = "missing or non-string field " + args.gt_field;
        }
        else {
            record.pred = object.get<jsonxx::String>(args.pred_field);
            record.gt = object.get<jsonxx::String>(args.gt_field);
        }
        records.push_back(std::move(record));
    }
    return records;
}

void AddSummaryRecord(
    const JsonlScoreRecord &source, const std::string &record_json, double failure_score,
    SummaryBucket &overall, std::map<std::string, SummaryBucket> &by_group)
{
    jsonxx::Object object;
    const bool parsed = object.parse(record_json);
    const bool ok = parsed && object.has<jsonxx::Boolean>("ok") && object.get<jsonxx::Boolean>("ok");
    double contribution = failure_score;
    if (ok && object.has<jsonxx::Number>("omr_ned")) {
        contribution = static_cast<double>(object.get<jsonxx::Number>("omr_ned"));
    }

    auto add = [&](SummaryBucket &bucket) {
        ++bucket.count;
        bucket.total += contribution;
        if (ok) ++bucket.scored;
        else ++bucket.failures;
    };
    add(overall);
    add(by_group[source.group]);
}

void WriteSummaryBucket(const SummaryBucket &bucket, double failure_score, std::ostream &out)
{
    out << "{\"count\":" << bucket.count << ",\"scored\":" << bucket.scored
        << ",\"failures\":" << bucket.failures << ",\"omr_ned\":";
    WriteDouble(bucket.count == 0 ? 0.0 : bucket.total / static_cast<double>(bucket.count), out);
    out << ",\"failure_score\":";
    WriteDouble(failure_score, out);
    out << "}";
}

bool WriteSummaryJson(const std::string &path, const SummaryBucket &overall,
    const std::map<std::string, SummaryBucket> &by_group, double failure_score, std::string &error)
{
    std::ofstream out(path);
    if (!out) {
        error = "cannot write " + path;
        return false;
    }
    out << "{\"overall\":";
    WriteSummaryBucket(overall, failure_score, out);
    out << ",\"by_group\":{";
    bool first = true;
    for (const auto &[group, bucket] : by_group) {
        if (!first) out << ",";
        first = false;
        WriteJsonString(group, out);
        out << ":";
        WriteSummaryBucket(bucket, failure_score, out);
    }
    out << "}}\n";
    return true;
}

} // namespace

void CompareBatchToJson(const std::vector<PairRow> &pairs, const std::string &base_dir, int jobs,
    const CompareCliOptions &options, std::ostream &out)
{
    if (jobs <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        jobs = hw == 0 ? 1 : static_cast<int>(hw);
    }
    jobs = std::max(1, std::min(jobs, static_cast<int>(std::max<std::size_t>(pairs.size(), 1))));

    std::vector<std::string> records(pairs.size());

    std::atomic<std::size_t> next{ 0 };
    std::vector<std::future<void>> workers;
    for (int w = 0; w < jobs; ++w) {
        workers.push_back(std::async(std::launch::async, [&] {
            VrvBridge bridge;
            for (;;) {
                const std::size_t i = next.fetch_add(1);
                if (i >= pairs.size()) break;
                const std::string pred = JoinBaseDir(base_dir, pairs[i].pred);
                const std::string gt = JoinBaseDir(base_dir, pairs[i].gt);
                std::ostringstream record;
                ComparePairToJson(bridge, pred, gt, options, record);
                records[i] = record.str();
            }
        }));
    }
    for (auto &worker : workers) worker.get();

    for (const std::string &record : records) out << record;
}

bool CompareJsonlBatchToJson(
    const BatchJsonlArgs &args, const CompareCliOptions &options, std::ostream &out, std::string &error)
{
    std::vector<JsonlScoreRecord> inputs = ReadJsonlScoreRecords(args, error);
    if (!error.empty()) return false;

    int jobs = args.jobs;
    if (jobs <= 0) {
        const unsigned hw = std::thread::hardware_concurrency();
        jobs = hw == 0 ? 1 : static_cast<int>(hw);
    }
    jobs = std::max(1, std::min(jobs, static_cast<int>(std::max<std::size_t>(inputs.size(), 1))));

    std::vector<std::string> records(inputs.size());
    std::atomic<std::size_t> next{ 0 };
    std::vector<std::future<void>> workers;
    for (int w = 0; w < jobs; ++w) {
        workers.push_back(std::async(std::launch::async, [&] {
            VrvBridge bridge;
            for (;;) {
                const std::size_t i = next.fetch_add(1);
                if (i >= inputs.size()) break;
                if (!inputs[i].early_error.empty()) {
                    records[i] = FailureRecordJson(inputs[i]);
                    continue;
                }
                std::ostringstream record;
                CompareScoreDataToJson(
                    bridge, inputs[i].pred, inputs[i].gt, options, inputs[i].prefix_json, record);
                records[i] = record.str();
            }
        }));
    }
    for (auto &worker : workers) worker.get();

    SummaryBucket overall;
    std::map<std::string, SummaryBucket> by_group;
    for (std::size_t i = 0; i < records.size(); ++i) {
        out << records[i];
        AddSummaryRecord(inputs[i], records[i], args.failure_score, overall, by_group);
    }

    if (!args.summary_json.empty()) {
        return WriteSummaryJson(args.summary_json, overall, by_group, args.failure_score, error);
    }
    return true;
}

} // namespace verosim
