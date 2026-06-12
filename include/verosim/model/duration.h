#pragma once

#include <array>
#include <string>
#include <string_view>

#include "verosim/model/fraction.h"

namespace verosim {

// music21 duration-type model (m21.duration), the unit system musicdiff's
// note_head / note_dur_type fields live in. type_num is m21
// convertTypeToNumber ("4 means a black note, 2 white, 1 whole, ..."); the
// quarterLength of a type is 4 / type_num.
struct DurationType {
    std::string_view name; // m21 duration.type string
    Fraction type_num;
};

// Ordered longest first (matches m21 ordinal order; extraction only needs
// lookups, ClosestTypeFromQL needs the order).
inline constexpr int kNumDurationTypes = 15;
inline const std::array<DurationType, kNumDurationTypes> &DurationTypes()
{
    static const std::array<DurationType, kNumDurationTypes> kTypes = { {
        { "maxima", Fraction(1, 8) },
        { "longa", Fraction(1, 4) },
        { "breve", Fraction(1, 2) },
        { "whole", Fraction(1) },
        { "half", Fraction(2) },
        { "quarter", Fraction(4) },
        { "eighth", Fraction(8) },
        { "16th", Fraction(16) },
        { "32nd", Fraction(32) },
        { "64th", Fraction(64) },
        { "128th", Fraction(128) },
        { "256th", Fraction(256) },
        { "512th", Fraction(512) },
        { "1024th", Fraction(1024) },
        { "2048th", Fraction(2048) },
    } };
    return kTypes;
}

inline Fraction TypeNumFromName(std::string_view name)
{
    for (const DurationType &t : DurationTypes()) {
        if (t.name == name) return t.type_num;
    }
    return Fraction(0); // m21 'zero'/'complex' have no direct number
}

inline Fraction QLFromTypeNum(const Fraction &typeNum) { return Fraction(4) / typeNum; }

// m21.duration.quarterLengthToClosestType: the longest type whose duration
// is <= qLen; exact match when qLen is exactly a type duration. Used by
// musicdiff get_type_num for 'complex' durations (m21utils.py:441-446).
inline std::string_view ClosestTypeFromQL(const Fraction &ql)
{
    for (const DurationType &t : DurationTypes()) {
        if (QLFromTypeNum(t.type_num) <= ql) return t.name;
    }
    return DurationTypes().back().name;
}

// Express ql as a (possibly dotted) single type: ql == typeQL * (2 - 2^-dots).
// Mirrors what m21 Duration(quarterLength=...) yields for expressible values
// (type + dots); returns false for inexpressible ('complex') durations.
// Needed to synthesize mRest durations from the governing meter.
inline bool QLToTypeDots(const Fraction &ql, std::string &type, int &dots)
{
    if (ql <= Fraction(0)) return false;
    for (const DurationType &t : DurationTypes()) {
        const Fraction typeQL = QLFromTypeNum(t.type_num);
        Fraction dotted = typeQL;
        Fraction add = typeQL;
        for (int d = 0; d <= 4; ++d) {
            if (dotted == ql) {
                type = std::string(t.name);
                dots = d;
                return true;
            }
            add = add / Fraction(2);
            dotted = dotted + add;
        }
    }
    return false;
}

// musicdiff get_type_num (m21utils.py:441-446): the duration type as a
// number, resolving 'complex' via the closest type.
inline Fraction TypeNumFromQL(const Fraction &ql, std::string_view typeName)
{
    if (typeName != "complex" && typeName != "zero") {
        const Fraction n = TypeNumFromName(typeName);
        if (n != Fraction(0)) return n;
    }
    return TypeNumFromName(ClosestTypeFromQL(ql));
}

// AnnNote note_head (annotation.py:193-200): min(type_num, 4).
inline Fraction NoteHeadFromTypeNum(const Fraction &typeNum)
{
    return typeNum >= Fraction(4) ? Fraction(4) : typeNum;
}

} // namespace verosim
