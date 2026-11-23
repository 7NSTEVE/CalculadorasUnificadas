#pragma once

#include <array>
#include <cstdint>

namespace cg50 {

inline constexpr std::uint32_t kDivideSrT = 0x00000001u;
inline constexpr std::uint32_t kDivideSrQ = 0x00000100u;
inline constexpr std::uint32_t kDivideSrM = 0x00000200u;

struct DivisionState {
    std::uint32_t value = 0;
    bool q = false;
    bool m = false;
    bool t = false;
};

struct DivideOpcodeResult {
    bool handled = false;
    std::uint32_t sr = 0;
};

inline DivisionState DivideSignedInitialize(const std::uint32_t rn,
                                            const std::uint32_t rm) noexcept
{
    DivisionState state{};
    state.value = rn;
    state.q = (rn & 0x80000000u) != 0;
    state.m = (rm & 0x80000000u) != 0;
    state.t = state.q != state.m;
    return state;
}

// One architectural SH DIV1 step. Renesas describes DIV1 as a one-bit divide
// combining subtraction/addition with a rotate through T. The transition below
// preserves the documented Q/M/T state machine and uses only unsigned arithmetic
// so 32-bit wraparound is explicit and well-defined.
inline DivisionState DivideOneBit(const std::uint32_t rn,
                                  const std::uint32_t rm,
                                  const bool oldQ,
                                  const bool m,
                                  const bool oldT) noexcept
{
    DivisionState state{};
    state.m = m;

    const bool shiftedSign = (rn & 0x80000000u) != 0;
    std::uint32_t value = (rn << 1) | (oldT ? 1u : 0u);
    bool q = shiftedSign;

    if (!oldQ) {
        if (!m) {
            const std::uint32_t before = value;
            value -= rm;
            const bool borrow = value > before;
            q = q ? !borrow : borrow;
        } else {
            const std::uint32_t before = value;
            value += rm;
            const bool carry = value < before;
            q = q ? carry : !carry;
        }
    } else {
        if (!m) {
            const std::uint32_t before = value;
            value += rm;
            const bool carry = value < before;
            q = q ? !carry : carry;
        } else {
            const std::uint32_t before = value;
            value -= rm;
            const bool borrow = value > before;
            q = q ? borrow : !borrow;
        }
    }

    state.value = value;
    state.q = q;
    state.t = q == m;
    return state;
}

inline std::uint32_t ApplyDivisionFlags(std::uint32_t sr,
                                        const DivisionState& state) noexcept
{
    sr &= ~(kDivideSrT | kDivideSrQ | kDivideSrM);
    if (state.t) sr |= kDivideSrT;
    if (state.q) sr |= kDivideSrQ;
    if (state.m) sr |= kDivideSrM;
    return sr;
}

// Decoder-ready boundary. DIV0S Rm,Rn = 0010 nnnn mmmm 0111 (0x2nm7)
// and DIV1 Rm,Rn = 0011 nnnn mmmm 0100 (0x3nm4).
inline DivideOpcodeResult ExecuteDivideOpcode(
    const std::uint16_t opcode,
    std::array<std::uint32_t, 16>& r,
    const std::uint32_t sr) noexcept
{
    const std::uint32_t n = (opcode >> 8) & 0x0Fu;
    const std::uint32_t m = (opcode >> 4) & 0x0Fu;

    if ((opcode & 0xF00Fu) == 0x2007u) {
        const auto state = DivideSignedInitialize(r[n], r[m]);
        return {true, ApplyDivisionFlags(sr, state)};
    }

    if ((opcode & 0xF00Fu) == 0x3004u) {
        const auto state = DivideOneBit(
            r[n], r[m],
            (sr & kDivideSrQ) != 0,
            (sr & kDivideSrM) != 0,
            (sr & kDivideSrT) != 0);
        r[n] = state.value;
        return {true, ApplyDivisionFlags(sr, state)};
    }

    return {false, sr};
}

} // namespace cg50
