#pragma once

#include <array>
#include <cstdint>
#include <limits>

namespace cg50 {

inline constexpr std::uint32_t kMacSrS = 0x00000002u;

struct MacResult {
    std::uint32_t mach = 0;
    std::uint32_t macl = 0;
    bool saturated = false;
};

struct MacOpcodeResult {
    bool handled = false;
    bool success = false;
    std::uint32_t faultAddress = 0;
};

inline std::uint64_t MacBits(const std::uint32_t mach,
                             const std::uint32_t macl) noexcept
{
    return (static_cast<std::uint64_t>(mach) << 32) | macl;
}

inline MacResult MacParts(const std::uint64_t bits, const bool saturated = false) noexcept
{
    return {
        static_cast<std::uint32_t>(bits >> 32),
        static_cast<std::uint32_t>(bits),
        saturated
    };
}

inline std::int64_t Signed64FromBits(const std::uint64_t bits) noexcept
{
    if ((bits & 0x8000000000000000ull) == 0)
        return static_cast<std::int64_t>(bits);

    const std::uint64_t magnitude = (~bits) + 1ull;
    if (magnitude == 0x8000000000000000ull)
        return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
}

inline std::int64_t Signed32FromBits(const std::uint32_t bits) noexcept
{
    return bits <= 0x7FFFFFFFu
        ? static_cast<std::int64_t>(bits)
        : static_cast<std::int64_t>(bits) - 0x100000000ll;
}

inline std::int32_t Signed16FromBits(const std::uint32_t bits) noexcept
{
    const std::uint32_t low = bits & 0xFFFFu;
    return low <= 0x7FFFu
        ? static_cast<std::int32_t>(low)
        : static_cast<std::int32_t>(low) - 0x10000;
}

// MAC.L: signed 32x32 multiply plus the 64-bit MACH:MACL accumulator.
// Without SR.S the accumulator wraps at 64 bits. With SR.S the mathematical
// result saturates to signed 48-bit range [-2^47, 2^47-1].
inline MacResult AccumulateLong(const std::uint32_t mach,
                                const std::uint32_t macl,
                                const std::uint32_t lhsBits,
                                const std::uint32_t rhsBits,
                                const bool saturation) noexcept
{
    const std::int64_t lhs = Signed32FromBits(lhsBits);
    const std::int64_t rhs = Signed32FromBits(rhsBits);
    const std::int64_t product = lhs * rhs;
    const std::uint64_t accumulatorBits = MacBits(mach, macl);
    const std::uint64_t productBits = static_cast<std::uint64_t>(product);
    const std::uint64_t sumBits = accumulatorBits + productBits;

    if (!saturation)
        return MacParts(sumBits);

    constexpr std::uint64_t kMin48Bits = 0xFFFF800000000000ull; // -2^47
    constexpr std::uint64_t kMax48Bits = 0x00007FFFFFFFFFFFull; //  2^47-1
    constexpr std::uint64_t kSign = 0x8000000000000000ull;

    const bool accumulatorNegative = (accumulatorBits & kSign) != 0;
    const bool productNegative = (productBits & kSign) != 0;
    const bool sumNegative = (sumBits & kSign) != 0;
    const bool signedOverflow =
        accumulatorNegative == productNegative && sumNegative != accumulatorNegative;

    if (signedOverflow)
        return MacParts(accumulatorNegative ? kMin48Bits : kMax48Bits, true);

    if (!sumNegative && sumBits > kMax48Bits)
        return MacParts(kMax48Bits, true);
    if (sumNegative && sumBits < kMin48Bits)
        return MacParts(kMin48Bits, true);

    return MacParts(sumBits);
}

// MAC.W: signed 16x16 multiply. Without SR.S it accumulates into the full
// 64-bit MACH:MACL value. With SR.S only MACL participates in signed 32-bit
// saturation; MACH is left untouched unless saturation occurs, in which case
// it is set to 1 as the overflow indicator used by SH implementations.
inline MacResult AccumulateWord(const std::uint32_t mach,
                                const std::uint32_t macl,
                                const std::uint32_t lhsBits,
                                const std::uint32_t rhsBits,
                                const bool saturation) noexcept
{
    const std::int32_t lhs = Signed16FromBits(lhsBits);
    const std::int32_t rhs = Signed16FromBits(rhsBits);
    const std::int32_t product = lhs * rhs;

    if (!saturation) {
        const std::uint64_t result =
            MacBits(mach, macl) + static_cast<std::uint64_t>(static_cast<std::int64_t>(product));
        return MacParts(result);
    }

    const std::int64_t current = Signed32FromBits(macl);
    const std::int64_t sum = current + static_cast<std::int64_t>(product);
    if (sum > std::numeric_limits<std::int32_t>::max())
        return {1u, 0x7FFFFFFFu, true};
    if (sum < std::numeric_limits<std::int32_t>::min())
        return {1u, 0x80000000u, true};

    return {
        mach,
        static_cast<std::uint32_t>(static_cast<std::int32_t>(sum)),
        false
    };
}

// MAC.L @Rm+,@Rn+ = 0000 nnnn mmmm 1111 (0x0nmF).
// Both source addresses are captured before either post-increment, matching the
// SH architectural behavior when Rm and Rn name the same register.
template <typename Read32Fn>
MacOpcodeResult ExecuteMacLongOpcode(
    const std::uint16_t opcode,
    std::array<std::uint32_t, 16>& r,
    std::uint32_t& mach,
    std::uint32_t& macl,
    const std::uint32_t sr,
    Read32Fn&& read32) noexcept
{
    if ((opcode & 0xF00Fu) != 0x000Fu)
        return {};

    const std::uint32_t n = (opcode >> 8) & 0x0Fu;
    const std::uint32_t m = (opcode >> 4) & 0x0Fu;
    const std::uint32_t addressM = r[m];
    const std::uint32_t addressN = r[n];

    std::uint32_t lhs = 0;
    std::uint32_t rhs = 0;
    if (!read32(addressM, lhs)) return {true, false, addressM};
    if (!read32(addressN, rhs)) return {true, false, addressN};

    r[m] += 4u;
    r[n] += 4u;

    const auto result = AccumulateLong(
        mach, macl, lhs, rhs, (sr & kMacSrS) != 0);
    mach = result.mach;
    macl = result.macl;
    return {true, true, 0u};
}

// MAC.W @Rm+,@Rn+ = 0100 nnnn mmmm 1111 (0x4nmF).
// The native dispatcher supplies its real guest Read16 callback here, so normal
// privilege/address-error/MMIO failure policy is shared with MOV.W and other
// 16-bit memory instructions instead of faking word loads through Read32.
template <typename Read16Fn>
MacOpcodeResult ExecuteMacWordOpcode(
    const std::uint16_t opcode,
    std::array<std::uint32_t, 16>& r,
    std::uint32_t& mach,
    std::uint32_t& macl,
    const std::uint32_t sr,
    Read16Fn&& read16) noexcept
{
    if ((opcode & 0xF00Fu) != 0x400Fu)
        return {};

    const std::uint32_t n = (opcode >> 8) & 0x0Fu;
    const std::uint32_t m = (opcode >> 4) & 0x0Fu;
    const std::uint32_t addressM = r[m];
    const std::uint32_t addressN = r[n];

    std::uint16_t lhs = 0;
    std::uint16_t rhs = 0;
    if (!read16(addressM, lhs)) return {true, false, addressM};
    if (!read16(addressN, rhs)) return {true, false, addressN};

    r[m] += 2u;
    r[n] += 2u;

    const auto result = AccumulateWord(
        mach, macl, lhs, rhs, (sr & kMacSrS) != 0);
    mach = result.mach;
    macl = result.macl;
    return {true, true, 0u};
}

} // namespace cg50
