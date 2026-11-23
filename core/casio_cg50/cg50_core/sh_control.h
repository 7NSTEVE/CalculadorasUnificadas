#pragma once

#include <array>
#include <cstdint>

namespace cg50 {

struct ControlOpcodeResult {
    bool handled = false;
    bool success = false;
    bool privilegeViolation = false;
    std::uint32_t faultAddress = 0;
    bool faultWrite = false;
};

namespace detail {

inline ControlOpcodeResult ControlOk() noexcept
{
    ControlOpcodeResult result{};
    result.handled = true;
    result.success = true;
    return result;
}

inline ControlOpcodeResult ControlPrivilegeViolation() noexcept
{
    ControlOpcodeResult result{};
    result.handled = true;
    result.privilegeViolation = true;
    return result;
}

inline ControlOpcodeResult ControlFault(const std::uint32_t address,
                                        const bool write) noexcept
{
    ControlOpcodeResult result{};
    result.handled = true;
    result.faultAddress = address;
    result.faultWrite = write;
    return result;
}

} // namespace detail

// SH-4/SH-4A bank/debug control transfers used by exception prologues.
// `inactiveBank` is the bank not currently selected by SR.RB. Cg50NativeCore
// keeps exactly that invariant by swapping R0-R7 with alternateBank_ whenever
// SetSr() toggles RB.
//
// SGR is architecturally saved on exception and is exposed here only through
// STC/STC.L. DBR supports STC/LDC and their stack forms. All operations in this
// helper are privileged; the owning core routes violations through EXPEVT.
template <typename Read32Fn, typename Write32Fn>
ControlOpcodeResult ExecuteBankedControlOpcode(
    const std::uint16_t opcode,
    std::array<std::uint32_t, 16>& r,
    std::array<std::uint32_t, 8>& inactiveBank,
    const bool privileged,
    const std::uint32_t sgr,
    std::uint32_t& dbr,
    Read32Fn&& read32,
    Write32Fn&& write32) noexcept
{
    const std::uint32_t n = (opcode >> 8) & 0x0Fu;

    // STC SGR,Rn
    if ((opcode & 0xF0FFu) == 0x003Au) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        r[n] = sgr;
        return detail::ControlOk();
    }

    // STC DBR,Rn
    if ((opcode & 0xF0FFu) == 0x00FAu) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        r[n] = dbr;
        return detail::ControlOk();
    }

    // STC Rm_BANK,Rn. Bits 6:4 select the inactive bank register.
    if ((opcode & 0xF08Fu) == 0x0082u) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t bank = (opcode >> 4) & 0x07u;
        r[n] = inactiveBank[bank];
        return detail::ControlOk();
    }

    // LDC Rm,DBR
    if ((opcode & 0xF0FFu) == 0x40FAu) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        dbr = r[n];
        return detail::ControlOk();
    }

    // LDC Rm,Rn_BANK. Bits 6:4 select the inactive bank destination.
    if ((opcode & 0xF08Fu) == 0x408Eu) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t bank = (opcode >> 4) & 0x07u;
        inactiveBank[bank] = r[n];
        return detail::ControlOk();
    }

    // STC.L SGR,@-Rn
    if ((opcode & 0xF0FFu) == 0x4032u) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t address = r[n] - 4u;
        if (!write32(address, sgr)) return detail::ControlFault(address, true);
        r[n] = address;
        return detail::ControlOk();
    }

    // STC.L DBR,@-Rn
    if ((opcode & 0xF0FFu) == 0x40F2u) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t address = r[n] - 4u;
        if (!write32(address, dbr)) return detail::ControlFault(address, true);
        r[n] = address;
        return detail::ControlOk();
    }

    // STC.L Rm_BANK,@-Rn
    if ((opcode & 0xF08Fu) == 0x4083u) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t bank = (opcode >> 4) & 0x07u;
        const std::uint32_t address = r[n] - 4u;
        if (!write32(address, inactiveBank[bank]))
            return detail::ControlFault(address, true);
        r[n] = address;
        return detail::ControlOk();
    }

    // LDC.L @Rm+,DBR
    if ((opcode & 0xF0FFu) == 0x40F6u) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t address = r[n];
        std::uint32_t value = 0;
        if (!read32(address, value)) return detail::ControlFault(address, false);
        r[n] += 4u;
        dbr = value;
        return detail::ControlOk();
    }

    // LDC.L @Rm+,Rn_BANK
    if ((opcode & 0xF08Fu) == 0x4087u) {
        if (!privileged) return detail::ControlPrivilegeViolation();
        const std::uint32_t bank = (opcode >> 4) & 0x07u;
        const std::uint32_t address = r[n];
        std::uint32_t value = 0;
        if (!read32(address, value)) return detail::ControlFault(address, false);
        r[n] += 4u;
        inactiveBank[bank] = value;
        return detail::ControlOk();
    }

    return {};
}

} // namespace cg50
