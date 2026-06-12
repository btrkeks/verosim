#include "verosim/cli/check.h"

#include <chrono>
#include <exception>
#include <sstream>

#include "verosim/extraction/vrv_bridge.h"

namespace verosim {

namespace {

void SplitLogByLevel(const std::string &log, CheckResult &result)
{
    std::istringstream lines(log);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("[Warning] ", 0) == 0) {
            result.warnings.push_back(line.substr(10));
        }
        else if (line.rfind("[Error] ", 0) == 0) {
            result.errors.push_back(line.substr(8));
        }
        // [Info]/[Debug] lines are not interesting to the sweep
    }
}

void WriteJsonString(const std::string &s, std::ostream &os)
{
    os << '"';
    for (const char c : s) {
        switch (c) {
            case '"': os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n"; break;
            case '\r': os << "\\r"; break;
            case '\t': os << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    os << buf;
                }
                else {
                    os << c;
                }
        }
    }
    os << '"';
}

void WriteJsonStringArray(const std::vector<std::string> &items, std::ostream &os)
{
    os << '[';
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) os << ',';
        WriteJsonString(items[i], os);
    }
    os << ']';
}

} // namespace

CheckResult CheckFile(VrvBridge &bridge, const std::string &path)
{
    CheckResult result;
    result.path = path;
    const auto start = std::chrono::steady_clock::now();
    try {
        result.ok = bridge.LoadScoreFile(path);
    }
    catch (const std::exception &e) {
        result.ok = false;
        result.exception = true;
        result.errors.push_back(std::string("exception: ") + e.what());
    }
    const auto end = std::chrono::steady_clock::now();
    result.load_ms
        = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    SplitLogByLevel(bridge.TakeLog(), result);
    return result;
}

void WriteCheckJsonl(const CheckResult &result, std::ostream &os)
{
    os << "{\"path\":";
    WriteJsonString(result.path, os);
    os << ",\"ok\":" << (result.ok ? "true" : "false")
       << ",\"exception\":" << (result.exception ? "true" : "false")
       << ",\"n_warnings\":" << result.warnings.size()
       << ",\"n_errors\":" << result.errors.size() << ",\"warnings\":";
    WriteJsonStringArray(result.warnings, os);
    os << ",\"errors\":";
    WriteJsonStringArray(result.errors, os);
    os << ",\"load_ms\":" << result.load_ms << "}\n";
}

} // namespace verosim
