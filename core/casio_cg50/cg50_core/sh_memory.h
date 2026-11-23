#pragma once

#include <array>
#include <cstdint>

namespace cg50 {

struct MemoryOpcodeResult {
    bool handled = false;
    bool success = false;
    bool tChanged = false;
    bool t = false;
    std::uint32_t faultAddress = 0;
    bool faultWrite = false;
};

namespace detail {

inline std::uint32_t MemoryRegN(const std::uint16_t opcode) noexcept
{
    return (opcode >> 8) & 0x0Fu;
}

inline MemoryOpcodeResult MemoryFault(const std::uint32_t address,
                                      const bool write) noexcept
{
    MemoryOpcodeResult result{};
    result.handled = true;
    result.success = false;
    result.faultAddress = address;
    result.faultWrite = write;
    return result;
}

inline MemoryOpcodeResult MemoryOk() noexcept
{
    MemoryOpcodeResult result{};
    result.handled = true;
    result.success = true;
    return result;
}

} // namespace detail

// Common SH-4/SH-4A memory forms that are independent of the CG50 board map.
// Read/write callbacks are deliberately supplied by the machine so an address
// that is not proven RAM/ROM/MMIO still fails through the core's normal gate.
template <typename Read8Fn, typename Read16Fn, typename Read32Fn,
          typename Write8Fn, typename Write16Fn, typename Write32Fn>
MemoryOpcodeResult ExecuteCommonMemoryOpcode(
    const std::uint16_t opcode,
    std::array<std::uint32_t, 16>& r,
    const std::uint32_t gbr,
    Read8Fn&& read8,
    Read16Fn&& read16,
    Read32Fn&& read32,
    Write8Fn&& write8,
    Write16Fn&& write16,
    Write32Fn&& write32) noexcept
{
    const std::uint16_t high = opcode & 0xFF00u;
    const std::uint32_t disp8 = opcode & 0x00FFu;

    // MOV.{B,W,L} R0,@(disp,GBR)
    if (high == 0xC000u || high == 0xC100u || high == 0xC200u) {
        const std::uint32_t width = high == 0xC000u ? 1u : (high == 0xC100u ? 2u : 4u);
        const std::uint32_t address = gbr + disp8 * width;
        const bool ok = width == 1u
            ? write8(address, static_cast<std::uint8_t>(r[0]))
            : width == 2u
                ? write16(address, static_cast<std::uint16_t>(r[0]))
                : write32(address, r[0]);
        return ok ? detail::MemoryOk() : detail::MemoryFault(address, true);
    }

    // MOV.{B,W,L} @(disp,GBR),R0
    if (high == 0xC400u || high == 0xC500u || high == 0xC600u) {
        const std::uint32_t width = high == 0xC400u ? 1u : (high == 0xC500u ? 2u : 4u);
        const std::uint32_t address = gbr + disp8 * width;
        if (width == 1u) {
            std::uint8_t value = 0;
            if (!read8(address, value)) return detail::MemoryFault(address, false);
            r[0] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
        } else if (width == 2u) {
            std::uint16_t value = 0;
            if (!read16(address, value)) return detail::MemoryFault(address, false);
            r[0] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
        } else {
            std::uint32_t value = 0;
            if (!read32(address, value)) return detail::MemoryFault(address, false);
            r[0] = value;
        }
        return detail::MemoryOk();
    }

    // TST/AND/XOR/OR.B #imm,@(R0,GBR)
    if (high >= 0xCC00u && high <= 0xCF00u) {
        const std::uint32_t address = gbr + r[0];
        const std::uint8_t immediate = static_cast<std::uint8_t>(opcode);
        std::uint8_t oldValue = 0;
        if (!read8(address, oldValue)) return detail::MemoryFault(address, false);

        if (high == 0xCC00u) {
            auto result = detail::MemoryOk();
            result.tChanged = true;
            result.t = (oldValue & immediate) == 0;
            return result;
        }

        const std::uint8_t newValue = high == 0xCD00u
            ? static_cast<std::uint8_t>(oldValue & immediate)
            : high == 0xCE00u
                ? static_cast<std::uint8_t>(oldValue ^ immediate)
                : static_cast<std::uint8_t>(oldValue | immediate);
        if (!write8(address, newValue)) return detail::MemoryFault(address, true);
        return detail::MemoryOk();
    }

    // TAS.B @Rn
    if ((opcode & 0xF0FFu) == 0x401Bu) {
        const std::uint32_t n = detail::MemoryRegN(opcode);
        const std::uint32_t address = r[n];
        std::uint8_t oldValue = 0;
        if (!read8(address, oldValue)) return detail::MemoryFault(address, false);
        if (!write8(address, static_cast<std::uint8_t>(oldValue | 0x80u)))
            return detail::MemoryFault(address, true);
        auto result = detail::MemoryOk();
        result.tChanged = true;
        result.t = oldValue == 0;
        return result;
    }

    // Compact R0 displacement forms: R0 <-> @(disp,Rm), byte/word.
    if (high == 0x8000u || high == 0x8100u ||
        high == 0x8400u || high == 0x8500u) {
        const std::uint32_t m = (opcode >> 4) & 0x0Fu;
        const std::uint32_t disp4 = opcode & 0x000Fu;
        const bool word = high == 0x8100u || high == 0x8500u;
        const std::uint32_t address = r[m] + disp4 * (word ? 2u : 1u);

        if (high == 0x8000u) {
            if (!write8(address, static_cast<std::uint8_t>(r[0])))
                return detail::MemoryFault(address, true);
        } else if (high == 0x8100u) {
            if (!write16(address, static_cast<std::uint16_t>(r[0])))
                return detail::MemoryFault(address, true);
        } else if (high == 0x8400u) {
            std::uint8_t value = 0;
            if (!read8(address, value)) return detail::MemoryFault(address, false);
            r[0] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int8_t>(value)));
        } else {
            std::uint16_t value = 0;
            if (!read16(address, value)) return detail::MemoryFault(address, false);
            r[0] = static_cast<std::uint32_t>(
                static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
        }
        return detail::MemoryOk();
    }

    return {};
}

} // namespace cg50
