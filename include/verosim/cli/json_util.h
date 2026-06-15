#pragma once

#include <cstdio>
#include <ostream>
#include <string>
#include <vector>

namespace verosim {

inline void WriteJsonString(const std::string &s, std::ostream &os)
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

inline void WriteJsonStringArray(const std::vector<std::string> &items, std::ostream &os)
{
    os << '[';
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i > 0) os << ',';
        WriteJsonString(items[i], os);
    }
    os << ']';
}

inline void WriteDouble(double value, std::ostream &out)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.12g", value);
    out << buf;
}

} // namespace verosim
