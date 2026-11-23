#pragma once

#include <array>
#include <cstdint>

namespace cg50 {

struct StackOpcodeResult {
    bool handled = false;
    bool success = false;
    bool privilegeViolation = false;
    bool setSr = false;
    std::uint32_t srValue = 0;
    std::uint32_t faultAddress = 0;
    bool faultWrite = false;
};

namespace detail {

inline StackOpcodeResult StackOk() noexcept
{
    StackOpcodeResult result{};
    result.handled = true;
    result.success = true;
    return result;
}

inline StackOpcodeResult StackFault(const std::uint32_t address,
                                    const bool write) noexcept
{
    StackOpcodeResult result{};
    result.handled = true;
    result.faultAddress = address;
    result.faultWrite = write;
    return result;
}

inline StackOpcodeResult StackPrivilegeViolation() noexcept
{
    StackOpcodeResult result{};
    result.handled = true;
    result.privilegeViolation = true;
    return result;
}

} // namespace detail

// Stack-oriented SH system/control transfers. Arithmetic instructions are not
// dispatched here: DIV0S/DIV1 and MAC.L/MAC.W have dedicated decoder boundaries
// in Cg50NativeCore so their guest execution path cannot be hidden inside an
// unrelated stack helper.
//
// The machine supplies memory callbacks; no board/MMIO address is assumed here.
// SR restoration is deferred so the owning core can perform its bank-switch side
// effect with SetSr().
template <typename Read32Fn, typename Write32Fn>
StackOpcodeResult ExecuteSystemStackOpcode(
    const std::uint16_t opcode,
    std::array<std::uint32_t, 16>& r,
    const bool privileged,
    const std::uint32_t sr,
    std::uint32_t& gbr,
    std::uint32_t& vbr,
    std::uint32_t& ssr,
    std::uint32_t& spc,
    std::uint32_t& mach,
    std::uint32_t& macl,
    std::uint32_t& pr,
    std::uint32_t& fpul,
    std::uint32_t& fpscr,
    Read32Fn&& read32,
    Write32Fn&& write32) noexcept
{
    const std::uint32_t n = (opcode >> 8) & 0x0Fu;
    const std::uint16_t low = opcode & 0x00FFu;

    // STS.L MACH/MACL/PR/FPUL/FPSCR,@-Rn
    if ((opcode & 0xF0FFu) == 0x4002u ||
        (opcode & 0xF0FFu) == 0x4012u ||
        (opcode & 0xF0FFu) == 0x4022u ||
        (opcode & 0xF0FFu) == 0x4056u ||
        (opcode & 0xF0FFu) == 0x4066u) {
        const std::uint32_t value = low == 0x02u ? mach :
                                    low == 0x12u ? macl :
                                    low == 0x22u ? pr :
                                    low == 0x56u ? fpul : fpscr;
        const std::uint32_t address = r[n] - 4u;
        if (!write32(address, value)) return detail::StackFault(address, true);
        r[n] = address;
        return detail::StackOk();
    }

    // LDS.L @Rn+,MACH/MACL/PR/FPUL/FPSCR
    if ((opcode & 0xF0FFu) == 0x4006u ||
        (opcode & 0xF0FFu) == 0x4016u ||
        (opcode & 0xF0FFu) == 0x4026u ||
        (opcode & 0xF0FFu) == 0x4057u ||
        (opcode & 0xF0FFu) == 0x4067u) {
        const std::uint32_t address = r[n];
        std::uint32_t value = 0;
        if (!read32(address, value)) return detail::StackFault(address, false);
        r[n] += 4u;
        if (low == 0x06u) mach = value;
        else if (low == 0x16u) macl = value;
        else if (low == 0x26u) pr = value;
        else if (low == 0x57u) fpul = value;
        else fpscr = value;
        return detail::StackOk();
    }

    // STC.L SR/GBR/VBR/SSR/SPC,@-Rn. Only GBR is legal in user mode.
    if ((opcode & 0xF0FFu) == 0x4003u ||
        (opcode & 0xF0FFu) == 0x4013u ||
        (opcode & 0xF0FFu) == 0x4023u ||
        (opcode & 0xF0FFu) == 0x4033u ||
        (opcode & 0xF0FFu) == 0x4043u) {
        if (low != 0x13u && !privileged)
            return detail::StackPrivilegeViolation();

        const std::uint32_t value = low == 0x03u ? sr :
                                    low == 0x13u ? gbr :
                                    low == 0x23u ? vbr :
                                    low == 0x33u ? ssr : spc;
        const std::uint32_t address = r[n] - 4u;
        if (!write32(address, value)) return detail::StackFault(address, true);
        r[n] = address;
        return detail::StackOk();
    }

    // LDC.L @Rn+,SR/GBR/VBR/SSR/SPC. SR is returned to the core so bank
    // switching remains centralized in Cg50NativeCore::SetSr().
    if ((opcode & 0xF0FFu) == 0x4007u ||
        (opcode & 0xF0FFu) == 0x4017u ||
        (opcode & 0xF0FFu) == 0x4027u ||
        (opcode & 0xF0FFu) == 0x4037u ||
        (opcode & 0xF0FFu) == 0x4047u) {
        if (low != 0x17u && !privileged)
            return detail::StackPrivilegeViolation();

        const std::uint32_t address = r[n];
        std::uint32_t value = 0;
        if (!read32(address, value)) return detail::StackFault(address, false);
        r[n] += 4u;

        if (low == 0x07u) {
            auto result = detail::StackOk();
            result.setSr = true;
            result.srValue = value;
            return result;
        }
        if (low == 0x17u) gbr = value;
        else if (low == 0x27u) vbr = value;
        else if (low == 0x37u) ssr = value;
        else spc = value;
        return detail::StackOk();
    }

    return {};
}

} // namespace cg50
