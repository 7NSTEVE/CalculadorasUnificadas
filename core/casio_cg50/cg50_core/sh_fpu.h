#pragma once

#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace cg50 {

// SH-4A FPU numeric helpers (opt-in legacy SH-4A profile; note Casio SH7305
// silicon is SH4AL-DSP without a hardware FPU per CPU73050.dll audit).
//
// SH-4A FPSCR.RM (bits 1-0):
//   00 = Round to Nearest (IEEE-754 FE_TONEAREST)
//   01 = Round to Zero    (IEEE-754 FE_TOWARDZERO)
//   10, 11 = Reserved in SH-4A (fall back to FE_TONEAREST).
inline constexpr std::uint32_t kFpuRoundingNearest = 0u;
inline constexpr std::uint32_t kFpuRoundingZero = 1u;
inline constexpr std::uint32_t kFpuRoundingPositiveInfinity = 2u; // Reserved in SH-4A
inline constexpr std::uint32_t kFpuRoundingNegativeInfinity = 3u; // Reserved in SH-4A

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

// Exact IEEE-754 mantissa rounding to float or double precision honoring
// SH-4A FPSCR.RM (00 = Nearest, 01 = TowardZero; 10/11 reserved -> Nearest).
// Fixes the historical floor()/ceil() integer-truncation bug.
inline double FpuRoundToPrecision(const double value,
                                  const bool doublePrecision,
                                  const std::uint32_t roundingMode,
                                  FpuEventReport& report)
{
    double rounded = value;
    const std::uint32_t rm = roundingMode & 3u;

    if (!doublePrecision) {
        if (std::isnan(value) || std::isinf(value) || value == 0.0) {
            rounded = static_cast<double>(static_cast<float>(value));
        } else if (rm == kFpuRoundingZero) {
            // Exact software round-toward-zero from IEEE-754 double to float
            // (portable across MSVC/Clang/GCC without relying on FENV_ACCESS).
            float nearest = static_cast<float>(value);
            if (std::isinf(nearest)) {
                // Overflow toward zero saturates to +/- FLT_MAX
                constexpr std::uint32_t kFltMaxBits = 0x7F7FFFFFu;
                const std::uint32_t sign = (value < 0.0) ? 0x80000000u : 0u;
                nearest = BitsToFloat(sign | kFltMaxBits);
            } else if (std::fabs(static_cast<double>(nearest)) > std::fabs(value)) {
                // Nearest rounded away from zero -> step 1 ULP back toward zero.
                const std::uint32_t bits = FloatToBits(nearest);
                if ((bits & 0x7FFFFFFFu) > 0u) {
                    nearest = BitsToFloat(bits - 1u);
                }
            }
            rounded = static_cast<double>(nearest);
        } else {
            // RM == 00 (Nearest) and reserved 10/11 -> IEEE-754 round-to-nearest-even
            const int prev = std::fegetround();
            if (prev != FE_TONEAREST) std::fesetround(FE_TONEAREST);
            volatile double vd = value;
            volatile float vf = static_cast<float>(vd);
            rounded = static_cast<double>(vf);
            if (prev != FE_TONEAREST) std::fesetround(prev);
        }
    } else {
        // Double precision result is already in IEEE-754 binary64; never truncate to integer!
        rounded = value;
    }

    report.inexactOrOverflow = (rounded != value) ||
        std::isinf(value) ||
        (std::isinf(rounded) && !std::isinf(value));
    if (doublePrecision) {
        report.underflowOrDivZero =
            (rounded != 0.0 && std::fpclassify(rounded) == FP_SUBNORMAL);
    } else {
        report.underflowOrDivZero =
            (rounded != 0.0 && std::fabs(rounded) < 1.1754943508222875e-38) ||
            (rounded != 0.0 && std::fpclassify(rounded) == FP_SUBNORMAL);
    }
    report.invalid = std::isnan(rounded);
    if (std::isnan(rounded)) {
        if (!doublePrecision) rounded = BitsToDouble(0x7FF80000u, 0x00000000u);
        else rounded = BitsToDouble(0x7FF80000u, 0x00000000u);
    }
    return rounded;
}

} // namespace cg50
