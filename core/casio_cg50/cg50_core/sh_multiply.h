#pragma once

#include <cstdint>

namespace cg50 {

struct Multiply64Result {
    std::uint32_t mach = 0;
    std::uint32_t macl = 0;
};

inline std::uint32_t MultiplyLongLow(const std::uint32_t lhs,
                                     const std::uint32_t rhs) noexcept
{
    // MUL.L keeps only the low 32 bits; signedness does not affect those bits.
    return static_cast<std::uint32_t>(
        static_cast<std::uint64_t>(lhs) * static_cast<std::uint64_t>(rhs));
}

inline std::uint32_t MultiplyUnsignedWord(const std::uint32_t lhs,
                                          const std::uint32_t rhs) noexcept
{
    return static_cast<std::uint32_t>(lhs & 0xFFFFu) *
           static_cast<std::uint32_t>(rhs & 0xFFFFu);
}

inline std::uint32_t MultiplySignedWord(const std::uint32_t lhs,
                                        const std::uint32_t rhs) noexcept
{
    const auto a = static_cast<std::int32_t>(static_cast<std::int16_t>(lhs & 0xFFFFu));
    const auto b = static_cast<std::int32_t>(static_cast<std::int16_t>(rhs & 0xFFFFu));
    return static_cast<std::uint32_t>(a * b);
}

inline Multiply64Result MultiplyUnsignedLong(const std::uint32_t lhs,
                                             const std::uint32_t rhs) noexcept
{
    const std::uint64_t product =
        static_cast<std::uint64_t>(lhs) * static_cast<std::uint64_t>(rhs);
    return {
        static_cast<std::uint32_t>(product >> 32),
        static_cast<std::uint32_t>(product)
    };
}

inline Multiply64Result MultiplySignedLong(const std::uint32_t lhs,
                                           const std::uint32_t rhs) noexcept
{
    const std::int64_t product =
        static_cast<std::int64_t>(static_cast<std::int32_t>(lhs)) *
        static_cast<std::int64_t>(static_cast<std::int32_t>(rhs));
    const std::uint64_t bits = static_cast<std::uint64_t>(product);
    return {
        static_cast<std::uint32_t>(bits >> 32),
        static_cast<std::uint32_t>(bits)
    };
}

} // namespace cg50
