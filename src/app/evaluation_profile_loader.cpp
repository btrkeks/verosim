#include "verosim/app/evaluation_profile_loader.h"

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>

#include <jsonxx.h>

namespace verosim {
namespace {

constexpr std::array<std::uint32_t, 64> kSha256RoundConstants = { 0x428a2f98,
    0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
    0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
    0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6,
    0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152,
    0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351,
    0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354,
    0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70,
    0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
    0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa,
    0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

std::string Sha256(std::string_view input)
{
    std::array<std::uint32_t, 8> hash = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372,
        0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    std::string padded(input);
    const std::uint64_t bit_length = static_cast<std::uint64_t>(padded.size()) * 8;
    padded.push_back(static_cast<char>(0x80));
    while (padded.size() % 64 != 56) padded.push_back('\0');
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<char>((bit_length >> shift) & 0xff));
    }

    for (std::size_t offset = 0; offset < padded.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t i = 0; i < 16; ++i) {
            const std::size_t p = offset + i * 4;
            words[i] = (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[p])) << 24)
                | (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[p + 1])) << 16)
                | (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[p + 2])) << 8)
                | static_cast<std::uint32_t>(static_cast<unsigned char>(padded[p + 3]));
        }
        for (std::size_t i = 16; i < words.size(); ++i) {
            const std::uint32_t s0 = std::rotr(words[i - 15], 7)
                ^ std::rotr(words[i - 15], 18) ^ (words[i - 15] >> 3);
            const std::uint32_t s1 = std::rotr(words[i - 2], 17)
                ^ std::rotr(words[i - 2], 19) ^ (words[i - 2] >> 10);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = hash[0], b = hash[1], c = hash[2], d = hash[3];
        std::uint32_t e = hash[4], f = hash[5], g = hash[6], h = hash[7];
        for (std::size_t i = 0; i < words.size(); ++i) {
            const std::uint32_t sum1
                = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const std::uint32_t choice = (e & f) ^ (~e & g);
            const std::uint32_t temp1
                = h + sum1 + choice + kSha256RoundConstants[i] + words[i];
            const std::uint32_t sum0
                = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }
        hash[0] += a;
        hash[1] += b;
        hash[2] += c;
        hash[3] += d;
        hash[4] += e;
        hash[5] += f;
        hash[6] += g;
        hash[7] += h;
    }

    std::ostringstream out;
    out << "sha256:" << std::hex << std::setfill('0');
    for (const std::uint32_t word : hash) out << std::setw(8) << word;
    return out.str();
}

std::string Prefix(const std::string &path, const std::string &message)
{
    return "evaluation profile " + path + ": " + message;
}

bool HasOnlyFields(const jsonxx::Object &object, std::string &error)
{
    static constexpr std::array<std::string_view, 4> kFields
        = { "schema", "schema_version", "id", "features" };
    for (const auto &[key, _] : object.kv_map()) {
        bool known = false;
        for (const std::string_view field : kFields) known = known || key == field;
        if (!known) {
            error = "unknown top-level field '" + key + "'";
            return false;
        }
    }
    return true;
}

bool HasExactSchemaVersionToken(std::string_view bytes)
{
    int object_depth = 0;
    int array_depth = 0;
    for (std::size_t i = 0; i < bytes.size();) {
        const char ch = bytes[i];
        if (ch == '"') {
            const std::size_t start = i++;
            bool escaped = false;
            while (i < bytes.size()) {
                const char string_ch = bytes[i++];
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (string_ch == '\\') {
                    escaped = true;
                    continue;
                }
                if (string_ch == '"') break;
            }
            if (i > bytes.size() || bytes[i - 1] != '"') return false;
            if (object_depth != 1 || array_depth != 0) continue;

            jsonxx::Value key;
            const std::string raw_key(bytes.substr(start, i - start));
            if (!key.parse(raw_key) || !key.is<jsonxx::String>()
                || key.get<jsonxx::String>() != "schema_version") {
                continue;
            }
            while (i < bytes.size()
                && (bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\r'
                    || bytes[i] == '\n')) {
                ++i;
            }
            if (i >= bytes.size() || bytes[i++] != ':') continue;
            while (i < bytes.size()
                && (bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\r'
                    || bytes[i] == '\n')) {
                ++i;
            }
            if (i >= bytes.size() || bytes[i++] != '1') return false;
            while (i < bytes.size()
                && (bytes[i] == ' ' || bytes[i] == '\t' || bytes[i] == '\r'
                    || bytes[i] == '\n')) {
                ++i;
            }
            return i < bytes.size() && (bytes[i] == ',' || bytes[i] == '}');
        }
        if (ch == '{') ++object_depth;
        else if (ch == '}') --object_depth;
        else if (ch == '[') ++array_depth;
        else if (ch == ']') --array_depth;
        ++i;
    }
    return false;
}

} // namespace

std::shared_ptr<const EvaluationProfile> LoadEvaluationProfileFile(
    const std::string &path, std::string &error)
{
    error.clear();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        error = Prefix(path, "cannot read file");
        return nullptr;
    }
    std::ostringstream bytes_stream;
    bytes_stream << input.rdbuf();
    if (!input.good() && !input.eof()) {
        error = Prefix(path, "failed while reading file");
        return nullptr;
    }
    const std::string bytes = bytes_stream.str();

    jsonxx::Object object;
    std::istringstream json_input(bytes);
    if (!object.parse(json_input)) {
        error = Prefix(path, "invalid JSON (duplicate keys are not allowed)");
        return nullptr;
    }
    json_input >> std::ws;
    if (!json_input.eof()) {
        error = Prefix(path, "trailing content after JSON object");
        return nullptr;
    }
    std::string validation_error;
    if (!HasOnlyFields(object, validation_error)) {
        error = Prefix(path, validation_error);
        return nullptr;
    }
    if (!object.has<jsonxx::String>("schema")) {
        error = Prefix(path, "field 'schema' must be a string");
        return nullptr;
    }
    if (object.get<jsonxx::String>("schema") != kEvaluationProfileSchema) {
        error = Prefix(path, "unsupported schema '" + object.get<jsonxx::String>("schema")
                + "' (expected '" + std::string(kEvaluationProfileSchema) + "')");
        return nullptr;
    }
    if (!HasExactSchemaVersionToken(bytes)
        || !object.has<jsonxx::Number>("schema_version")
        || object.get<jsonxx::Number>("schema_version")
            != static_cast<double>(kEvaluationProfileSchemaVersion)) {
        error = Prefix(path, "field 'schema_version' must be the exact JSON integer 1");
        return nullptr;
    }
    if (!object.has<jsonxx::String>("id")) {
        error = Prefix(path, "field 'id' must be a string");
        return nullptr;
    }
    if (!object.has<jsonxx::Object>("features")) {
        error = Prefix(path, "field 'features' must be an object");
        return nullptr;
    }

    const jsonxx::Object &features = object.get<jsonxx::Object>("features");
    for (const auto &[key, _] : features.kv_map()) {
        if (!EvaluationFeatureFromKey(key).has_value()) {
            error = Prefix(path, "unknown feature '" + key + "'");
            return nullptr;
        }
    }

    EvaluationFeatureDispositions dispositions;
    for (const EvaluationFeatureSpec &spec : EvaluationFeatureRegistryV1()) {
        if (!features.has<jsonxx::String>(std::string(spec.key))) {
            error = Prefix(path,
                "missing feature '" + std::string(spec.key) + "' or disposition is not a string");
            return nullptr;
        }
        const std::string &text = features.get<jsonxx::String>(std::string(spec.key));
        const std::optional<FeatureDisposition> disposition = ParseFeatureDisposition(text);
        if (!disposition.has_value()) {
            error = Prefix(path, "feature '" + std::string(spec.key)
                    + "' has invalid disposition '" + text
                    + "' (expected 'scored', 'ignored', or 'unsupported')");
            return nullptr;
        }
        dispositions[EvaluationFeatureIndex(spec.feature)] = *disposition;
    }

    std::optional<EvaluationProfile> profile = MakeEvaluationProfileV1(
        object.get<jsonxx::String>("id"), Sha256(bytes), dispositions, validation_error);
    if (!profile.has_value()) {
        error = Prefix(path, validation_error);
        return nullptr;
    }
    return std::make_shared<const EvaluationProfile>(std::move(*profile));
}

} // namespace verosim
