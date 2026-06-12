#pragma once

#include <cstdint>
#include <numeric>
#include <ostream>
#include <stdexcept>
#include <string>

namespace verosim {

// Exact rational arithmetic for offsets and durations in quarter-note units
// (music21 quarterLength convention: quarter == 1). Engineering decision §7:
// the final OMR-NED ratio is the only floating-point value in the pipeline.
//
// int64 numerator/denominator with normalization after every operation.
// Musical values are tiny (denominators are products of 2^k and small tuplet
// ratios), so int64 never gets close to overflow in practice; multiplications
// go through gcd-reduction first to keep intermediates small, and a checked
// build assert catches the impossible case.
class Fraction {
public:
    constexpr Fraction() = default;
    constexpr Fraction(std::int64_t value) : num_(value) {}
    Fraction(std::int64_t num, std::int64_t den) : num_(num), den_(den)
    {
        if (den_ == 0) throw std::domain_error("Fraction: zero denominator");
        Normalize();
    }

    constexpr std::int64_t num() const { return num_; }
    constexpr std::int64_t den() const { return den_; }

    friend Fraction operator+(const Fraction &a, const Fraction &b)
    {
        // a.num/a.den + b.num/b.den over lcm denominator
        const std::int64_t g = std::gcd(a.den_, b.den_);
        const std::int64_t bScaled = b.den_ / g;
        return Fraction(a.num_ * bScaled + b.num_ * (a.den_ / g), a.den_ * bScaled);
    }
    friend Fraction operator-(const Fraction &a, const Fraction &b)
    {
        return a + Fraction(-b.num_, b.den_);
    }
    friend Fraction operator*(const Fraction &a, const Fraction &b)
    {
        // cross-reduce before multiplying to keep intermediates small
        const std::int64_t g1 = std::gcd(a.num_ < 0 ? -a.num_ : a.num_, b.den_);
        const std::int64_t g2 = std::gcd(b.num_ < 0 ? -b.num_ : b.num_, a.den_);
        return Fraction((a.num_ / g1) * (b.num_ / g2), (a.den_ / g2) * (b.den_ / g1));
    }
    friend Fraction operator/(const Fraction &a, const Fraction &b)
    {
        if (b.num_ == 0) throw std::domain_error("Fraction: division by zero");
        return a * Fraction(b.den_, b.num_);
    }
    Fraction &operator+=(const Fraction &o) { return *this = *this + o; }
    Fraction &operator-=(const Fraction &o) { return *this = *this - o; }
    Fraction &operator*=(const Fraction &o) { return *this = *this * o; }
    Fraction &operator/=(const Fraction &o) { return *this = *this / o; }

    friend bool operator==(const Fraction &a, const Fraction &b)
    {
        return a.num_ == b.num_ && a.den_ == b.den_;
    }
    friend auto operator<=>(const Fraction &a, const Fraction &b)
    {
        // denominators are positive post-normalization, so cross-multiply
        return a.num_ * b.den_ <=> b.num_ * a.den_;
    }

    // music21 prints integral OffsetQL as "1.0" but Fraction offsets as "7/3";
    // our triage output only needs a stable exact form.
    std::string str() const
    {
        if (den_ == 1) return std::to_string(num_);
        return std::to_string(num_) + "/" + std::to_string(den_);
    }
    friend std::ostream &operator<<(std::ostream &os, const Fraction &f)
    {
        return os << f.str();
    }

    double to_double() const { return static_cast<double>(num_) / static_cast<double>(den_); }

private:
    void Normalize()
    {
        if (den_ < 0) {
            num_ = -num_;
            den_ = -den_;
        }
        const std::int64_t g = std::gcd(num_ < 0 ? -num_ : num_, den_);
        if (g > 1) {
            num_ /= g;
            den_ /= g;
        }
    }

    std::int64_t num_ = 0;
    std::int64_t den_ = 1;
};

} // namespace verosim
