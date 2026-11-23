#pragma once

#include <cstdint>

namespace cg50 {

struct IntegerResult {
    std::uint32_t value = 0;
    bool t = false;
};

inline bool SignedAddOverflow(const std::uint32_t a, const std::uint32_t b,
                              const std::uint32_t result) noexcept
{
    return ((~(a ^ b) & (a ^ result)) & 0x80000000u) != 0;
}

inline bool SignedSubOverflow(const std::uint32_t a, const std::uint32_t b,
                              const std::uint32_t result) noexcept
{
    return (((a ^ b) & (a ^ result)) & 0x80000000u) != 0;
}

inline IntegerResult AddWithCarry(const std::uint32_t rn, const std::uint32_t rm,
                                  const bool oldT) noexcept
{
    const std::uint64_t sum = static_cast<std::uint64_t>(rn) + rm + (oldT ? 1u : 0u);
    return {static_cast<std::uint32_t>(sum), (sum >> 32) != 0};
}

inline IntegerResult AddWithOverflow(const std::uint32_t rn,
                                     const std::uint32_t rm) noexcept
{
    const std::uint32_t result = rn + rm;
    return {result, SignedAddOverflow(rn, rm, result)};
}

inline IntegerResult SubWithCarry(const std::uint32_t rn, const std::uint32_t rm,
                                  const bool oldT) noexcept
{
    const std::uint64_t rhs = static_cast<std::uint64_t>(rm) + (oldT ? 1u : 0u);
    const std::uint32_t result =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(rn) - rhs);
    return {result, static_cast<std::uint64_t>(rn) < rhs};
}

inline IntegerResult SubWithOverflow(const std::uint32_t rn,
                                     const std::uint32_t rm) noexcept
{
    const std::uint32_t result = rn - rm;
    return {result, SignedSubOverflow(rn, rm, result)};
}

inline IntegerResult NegWithCarry(const std::uint32_t rm, const bool oldT) noexcept
{
    const std::uint64_t rhs = static_cast<std::uint64_t>(rm) + (oldT ? 1u : 0u);
    return {static_cast<std::uint32_t>(0ull - rhs), rhs != 0};
}

inline std::uint32_t DynamicArithmeticShift(const std::uint32_t value,
                                            const std::uint32_t rawCount) noexcept
{
    const std::int32_t count = static_cast<std::int32_t>(rawCount);
    if (count >= 0)
        return value << (static_cast<std::uint32_t>(count) & 0x1Fu);

    const std::uint32_t amount = ((~rawCount) & 0x1Fu) + 1u;
    if (amount >= 32u)
        return static_cast<std::int32_t>(value) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> amount);
}

inline std::uint32_t DynamicLogicalShift(const std::uint32_t value,
                                         const std::uint32_t rawCount) noexcept
{
    const std::int32_t count = static_cast<std::int32_t>(rawCount);
    if (count >= 0)
        return value << (static_cast<std::uint32_t>(count) & 0x1Fu);

    const std::uint32_t amount = ((~rawCount) & 0x1Fu) + 1u;
    return amount >= 32u ? 0u : (value >> amount);
}

inline IntegerResult RotateLeft(const std::uint32_t value) noexcept
{
    const bool top = (value & 0x80000000u) != 0;
    return {(value << 1) | (top ? 1u : 0u), top};
}

inline IntegerResult RotateRight(const std::uint32_t value) noexcept
{
    const bool low = (value & 1u) != 0;
    return {(value >> 1) | (low ? 0x80000000u : 0u), low};
}

inline IntegerResult RotateCarryLeft(const std::uint32_t value,
                                     const bool oldT) noexcept
{
    const bool top = (value & 0x80000000u) != 0;
    return {(value << 1) | (oldT ? 1u : 0u), top};
}

inline IntegerResult RotateCarryRight(const std::uint32_t value,
                                      const bool oldT) noexcept
{
    const bool low = (value & 1u) != 0;
    return {(value >> 1) | (oldT ? 0x80000000u : 0u), low};
}

inline bool CompareStringBytes(const std::uint32_t rn,
                               const std::uint32_t rm) noexcept
{
    const std::uint32_t x = rn ^ rm;
    return (x & 0x000000FFu) == 0 ||
           (x & 0x0000FF00u) == 0 ||
           (x & 0x00FF0000u) == 0 ||
           (x & 0xFF000000u) == 0;
}

inline std::uint32_t ExtractMiddle(const std::uint32_t rn,
                                   const std::uint32_t rm) noexcept
{
    return (rn >> 16) | (rm << 16);
}

} // namespace cg50
