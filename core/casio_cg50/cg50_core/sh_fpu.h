#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

namespace cg50 {

// SH-4A FPU numeric helpers. Bank selection, memory addressing and exception
// entry live in Cg50NativeCore::ExecuteFpuOpcode; everything here is pure
// arithmetic over IEEE-754 bit patterns so it stays unit-testable on its own.
//
// FPSCR rounding modes (RM bits 1-0): 00 nearest, 01 to zero, 10 toward
// +infinity, 11 toward -infinity. Flag/cause grouping per the SH-4 manual:
// inexact/overflow share one bit, underflow/divide-by-zero share one bit and
// invalid uses its own bit (enable 10-12, cause 13-15, flag 16-18).
inline constexpr std::uint32_t kFpuRoundingNearest = 0u;
inline constexpr std::uint32_t kFpuRoundingZero = 1u;
inline constexpr std::uint32_t kFpuRoundingPositiveInfinity = 2u;
inline constexpr std::uint32_t kFpuRoundingNegativeInfinity = 3u;

inline float BitsToFloat(const std::uint32_t bits)
{
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::uint32_t FloatToBits(const float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

inline double BitsToDouble(const std::uint32_t highWord, const std::uint32_t lowWord)
{
    const std::uint64_t bits =
        (static_cast<std::uint64_t>(highWord) << 32) | lowWord;
    double value = 0.0;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline void DoubleToBits(const double value, std::uint32_t& highWord, std::uint32_t& lowWord)
{
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    highWord = static_cast<std::uint32_t>(bits >> 32);
    lowWord = static_cast<std::uint32_t>(bits);
}

struct FpuEventReport {
    bool inexactOrOverflow = false;
    bool underflowOrDivZero = false;
    bool invalid = false;
};

// Rounds a value computed in host double precision to the destination
// precision selected by FPSCR.PR, honouring FPSCR.RM. Reports the IEEE event
// classification for the cause/flag triplets.
inline double FpuRoundToPrecision(const double value,
                                  const bool doublePrecision,
                                  const std::uint32_t roundingMode,
                                  FpuEventReport& report)
{
    double rounded = value;
    if (!doublePrecision) {
        float narrow = 0.0f;
        switch (roundingMode & 3u) {
        case kFpuRoundingNearest:
            narrow = static_cast<float>(value);
            break;
        case kFpuRoundingZero:
            narrow = static_cast<float>(value >= 0.0 ? std::floor(value) : std::ceil(value));
            break;
        case kFpuRoundingPositiveInfinity:
            narrow = static_cast<float>(std::ceil(value));
            break;
        default:
            narrow = static_cast<float>(std::floor(value));
            break;
        }
        rounded = static_cast<double>(narrow);
    } else {
        switch (roundingMode & 3u) {
        case kFpuRoundingNearest:
            rounded = value;
            break;
        case kFpuRoundingZero:
            rounded = value >= 0.0 ? std::floor(value) : std::ceil(value);
            break;
        case kFpuRoundingPositiveInfinity:
            rounded = std::ceil(value);
            break;
        default:
            rounded = std::floor(value);
            break;
        }
    }

    report.inexactOrOverflow = (rounded != value) ||
        std::isinf(value) ||
        (std::isinf(rounded) && !std::isinf(value));
    if (doublePrecision) {
        report.underflowOrDivZero =
            (rounded != 0.0 && std::fpclassify(rounded) == FP_SUBNORMAL);
    } else {
        // Single-precision destination: anything that rounds below the float
        // minimum normal counts as underflow.
        report.underflowOrDivZero =
            (rounded != 0.0 && std::fabs(rounded) < 1.1754943508222875e-38) ||
            (rounded != 0.0 && std::fpclassify(rounded) == FP_SUBNORMAL);
    }
    // Documented simplification: any NaN result (NaN operand or generated
    // inf-inf / 0*inf) reports invalid.
    report.invalid = std::isnan(rounded);
    if (std::isnan(rounded)) {
        // Normalize the quiet NaN the host produces to the SH-4 default NaN
        // (all-ones payload), so behaviour does not depend on the host libc.
        if (!doublePrecision) rounded = std::nanf("");
        else rounded = std::nan("");
    }
    return rounded;
}

} // namespace cg50
