#pragma once

#include <cstdint>

namespace cg50 {

// Architectural event codes from the SH exception vector table. These are CPU
// event values, not MMIO addresses.
inline constexpr std::uint32_t kExpevtAddressErrorRead  = 0x000000E0u;
inline constexpr std::uint32_t kExpevtAddressErrorWrite = 0x00000100u;

// The current core explicitly implements only P1/P2 direct mapping. Those two
// segments are privileged when accessed by the emulated guest. P3/P4 are not
// classified here: they remain fail-closed until MMU/P4 semantics are evidenced.
constexpr bool IsPrivilegedDirectSegment(const std::uint32_t address) noexcept
{
    return address >= 0x80000000u && address < 0xC0000000u;
}

constexpr bool DirectSegmentAccessViolatesUserMode(
    const std::uint32_t sr, const std::uint32_t mdBit,
    const std::uint32_t address) noexcept
{
    return (sr & mdBit) == 0 && IsPrivilegedDirectSegment(address);
}

constexpr std::uint32_t AddressErrorEvent(const bool write) noexcept
{
    return write ? kExpevtAddressErrorWrite : kExpevtAddressErrorRead;
}

} // namespace cg50
