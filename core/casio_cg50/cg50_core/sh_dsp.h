#pragma once

#include <array>
#include <cstdint>

namespace cg50 {

// ============================================================================
// Renesas SH4AL-DSP Architectural Extension (confirmed in Casio CPU73050.dll)
//
// Registers:
//   A0 (32b + 8b guard A0G -> 40-bit signed), A1 (32b + 8b guard A1G)
//   M0, M1 (32b multiplier result registers)
//   X0, X1, Y0, Y1 (32b data registers)
//   RS, RE (32b zero-overhead repeat loop start / end addresses)
//   MOD (32b modulo addressing register: bits 31..16 = ME, bits 15..0 = MS)
//   DSR (32b DSP status register: DC bit 0, GT/EQ/N/Z/V/S, signed/unsigned mode)
//
// SR bits used by SH4AL-DSP:
//   SR.DSP = bit 12 (0x00001000)
//   SR.RC  = bits 27..16 (12-bit repeat loop counter, mask 0x0FFF0000)
// ============================================================================
struct ShDspState {
    // 40-bit accumulators stored sign-extended in int64_t (bits 39..0 active)
    std::int64_t a0 = 0;
    std::int64_t a1 = 0;
    std::uint32_t m0 = 0;
    std::uint32_t m1 = 0;
    std::uint32_t x0 = 0;
    std::uint32_t x1 = 0;
    std::uint32_t y0 = 0;
    std::uint32_t y1 = 0;
    std::uint32_t rs = 0;
    std::uint32_t re = 0;
    std::uint32_t mod = 0;
    std::uint32_t dsr = 0;

    static constexpr std::uint32_t kSrDspBit = 0x00001000u;
    static constexpr std::uint32_t kSrRcMask = 0x0FFF0000u;
    static constexpr std::uint32_t kSrRcShift = 16u;

    static constexpr std::uint32_t kDsrDc = 0x00000001u; // Condition bit
    static constexpr std::uint32_t kDsrGt = 0x00000002u; // Greater than
    static constexpr std::uint32_t kDsrEq = 0x00000004u; // Equal
    static constexpr std::uint32_t kDsrN  = 0x00000008u; // Negative
    static constexpr std::uint32_t kDsrZ  = 0x00000010u; // Zero
    static constexpr std::uint32_t kDsrV  = 0x00000020u; // Overflow
    static constexpr std::uint32_t kDsrS  = 0x00000040u; // Saturation mode
    static constexpr std::uint32_t kDsrCsMask = 0x000000E0u;

    void Reset() noexcept
    {
        a0 = a1 = 0;
        m0 = m1 = 0;
        x0 = x1 = y0 = y1 = 0;
        rs = re = mod = dsr = 0;
    }

    static std::int64_t SignExtend40(const std::int64_t v) noexcept
    {
        return (v << 24) >> 24;
    }

    static std::int64_t Saturate32(const std::int64_t v) noexcept
    {
        if (v > 0x7FFFFFFFll) return 0x7FFFFFFFll;
        if (v < -0x80000000ll) return -0x80000000ll;
        return v;
    }

    std::uint32_t GetA0Low() const noexcept { return static_cast<std::uint32_t>(a0 & 0xFFFFFFFFull); }
    std::uint32_t GetA1Low() const noexcept { return static_cast<std::uint32_t>(a1 & 0xFFFFFFFFull); }
    std::uint32_t GetA0Guard() const noexcept { return static_cast<std::uint32_t>((a0 >> 32) & 0xFFull); }
    std::uint32_t GetA1Guard() const noexcept { return static_cast<std::uint32_t>((a1 >> 32) & 0xFFull); }

    void SetA0Low(const std::uint32_t v) noexcept
    {
        a0 = static_cast<std::int64_t>(static_cast<std::int32_t>(v));
    }
    void SetA1Low(const std::uint32_t v) noexcept
    {
        a1 = static_cast<std::int64_t>(static_cast<std::int32_t>(v));
    }
    void SetA0Guard(const std::uint32_t g) noexcept
    {
        const std::uint64_t lo = static_cast<std::uint64_t>(a0) & 0xFFFFFFFFull;
        const std::uint64_t hi = static_cast<std::uint64_t>(g & 0xFFu) << 32;
        a0 = SignExtend40(static_cast<std::int64_t>(hi | lo));
    }
    void SetA1Guard(const std::uint32_t g) noexcept
    {
        const std::uint64_t lo = static_cast<std::uint64_t>(a1) & 0xFFFFFFFFull;
        const std::uint64_t hi = static_cast<std::uint64_t>(g & 0xFFu) << 32;
        a1 = SignExtend40(static_cast<std::int64_t>(hi | lo));
    }

    // DSP register index used by parallel ALU/MOVS (SH-DSP manual table):
    //   0=M0, 1=M1, 2=A0, 3=A1, 4=X0, 5=X1, 6=Y0, 7=Y1, 8=A0G, 9=A1G
    std::int64_t ReadReg40(const std::uint32_t idx) const noexcept
    {
        switch (idx & 0xFu) {
        case 0: return static_cast<std::int64_t>(static_cast<std::int32_t>(m0));
        case 1: return static_cast<std::int64_t>(static_cast<std::int32_t>(m1));
        case 2: return a0;
        case 3: return a1;
        case 4: return static_cast<std::int64_t>(static_cast<std::int32_t>(x0));
        case 5: return static_cast<std::int64_t>(static_cast<std::int32_t>(x1));
        case 6: return static_cast<std::int64_t>(static_cast<std::int32_t>(y0));
        case 7: return static_cast<std::int64_t>(static_cast<std::int32_t>(y1));
        case 8: return static_cast<std::int64_t>(static_cast<std::int8_t>(GetA0Guard()));
        case 9: return static_cast<std::int64_t>(static_cast<std::int8_t>(GetA1Guard()));
        default: return 0;
        }
    }

    void WriteReg40(const std::uint32_t idx, const std::int64_t val) noexcept
    {
        const std::int64_t clamped = ((dsr & kDsrS) != 0u) ? Saturate32(val) : SignExtend40(val);
        switch (idx & 0xFu) {
        case 0: m0 = static_cast<std::uint32_t>(clamped & 0xFFFFFFFFull); break;
        case 1: m1 = static_cast<std::uint32_t>(clamped & 0xFFFFFFFFull); break;
        case 2: a0 = clamped; break;
        case 3: a1 = clamped; break;
        case 4: x0 = static_cast<std::uint32_t>(clamped & 0xFFFFFFFFull); break;
        case 5: x1 = static_cast<std::uint32_t>(clamped & 0xFFFFFFFFull); break;
        case 6: y0 = static_cast<std::uint32_t>(clamped & 0xFFFFFFFFull); break;
        case 7: y1 = static_cast<std::uint32_t>(clamped & 0xFFFFFFFFull); break;
        case 8: SetA0Guard(static_cast<std::uint32_t>(val & 0xFFu)); break;
        case 9: SetA1Guard(static_cast<std::uint32_t>(val & 0xFFu)); break;
        default: break;
        }
    }

    void UpdateFlags(const std::int64_t result, const bool overflow = false, const bool carry = false) noexcept
    {
        std::uint32_t next = dsr & ~(kDsrDc | kDsrGt | kDsrEq | kDsrN | kDsrZ | kDsrV);
        if (result == 0) next |= (kDsrZ | kDsrEq);
        if (result < 0)  next |= kDsrN;
        if (result > 0)  next |= kDsrGt;
        if (overflow)    next |= kDsrV;
        // CS bits (7..5) choose which condition updates DC (bit 0)
        const std::uint32_t cs = (dsr >> 5) & 0x7u;
        bool dc = carry;
        switch (cs) {
        case 0: dc = carry; break;
        case 1: dc = (result < 0); break;
        case 2: dc = (result == 0); break;
        case 3: dc = (result > 0); break;
        case 4: dc = (result >= 0); break;
        case 5: dc = overflow; break;
        default: dc = carry; break;
        }
        if (dc) next |= kDsrDc;
        dsr = next;
    }

    // Modulo address update for MOVX/MOVY (when MOD is active)
    std::uint32_t StepModuloAddress(const std::uint32_t addr, const std::int32_t step) const noexcept
    {
        const std::uint32_t ms = mod & 0xFFFFu;
        const std::uint32_t me = (mod >> 16) & 0xFFFFu;
        if (ms == 0u && me == 0u) return static_cast<std::uint32_t>(addr + step);
        const std::uint32_t low = addr & 0xFFFFu;
        if (step > 0 && low == me) {
            return (addr & 0xFFFF0000u) | ms;
        }
        return static_cast<std::uint32_t>(addr + step);
    }

    // Execute 32-bit SH4AL-DSP parallel instruction (op1 = 0xF400..0xF7FF or 0xF800..0xFBFF,
    // op2 = secondary ALU/MAC word).
    // Supports parallel MOVX/MOVY + PMULS + PADD/PADDC/PSUB/PSUBC/PABS/PNEG/PCLR/PCOPY/
    // PDEC/PINC/PDMSB/PRND/PSHA/PSHL/PAND/POR/PXOR.
    template <typename Read16Fn, typename Write16Fn, typename Read32Fn, typename Write32Fn>
    bool ExecuteParallel32(const std::uint16_t op1,
                           const std::uint16_t op2,
                           std::array<std::uint32_t, 16>& gpr,
                           Read16Fn&& read16,
                           Write16Fn&& write16,
                           Read32Fn&& read32,
                           Write32Fn&& write32) noexcept
    {
        // 1. Decode secondary ALU/MAC word (op2) if op1 is 0xF800..0xFBFF
        //    (or execute single-transfer MOVS when op1 is 0xF400..0xF7FF).
        const bool hasAlu = (op1 & 0xFC00u) == 0xF800u;

        // Decode data transfer field in op1 (bits 9..0)
        // MOVX uses R4/R5 (Ax), MOVY uses R6/R7 (Ay), index R8/R9.
        const std::uint32_t xOp = (op1 >> 2) & 0x3u;
        const std::uint32_t yOp = op1 & 0x3u;
        const std::uint32_t axIdx = 4u + ((op1 >> 9) & 1u);
        const std::uint32_t ayIdx = 6u + ((op1 >> 8) & 1u);
        const std::uint32_t dx = (op1 >> 7) & 1u; // 0=X0, 1=X1
        const std::uint32_t dy = (op1 >> 6) & 1u; // 0=Y0, 1=Y1
        const bool xStore = ((op1 >> 5) & 1u) != 0u;
        const bool yStore = ((op1 >> 4) & 1u) != 0u;

        if ((op1 & 0xFC00u) == 0xF400u) {
            // Single data transfer MOVS.W / MOVS.L
            const std::uint32_t asIdx = (op1 >> 8) & 0x3u;
            const std::uint32_t regIdx = (op1 >> 4) & 0xFu;
            const bool isStore = (op1 & 0x0008u) != 0u;
            const bool isLong = (op1 & 0x0004u) != 0u;
            const std::uint32_t mode = op1 & 0x0003u;
            const std::uint32_t addr = gpr[asIdx];
            if (isStore) {
                const std::int64_t val = ReadReg40(regIdx);
                if (isLong) {
                    (void)write32(addr, static_cast<std::uint32_t>(val & 0xFFFFFFFFull));
                } else {
                    (void)write16(addr, static_cast<std::uint16_t>((val >> 16) & 0xFFFFull));
                }
            } else {
                if (isLong) {
                    std::uint32_t v = 0;
                    if (read32(addr, v)) WriteReg40(regIdx, static_cast<std::int32_t>(v));
                } else {
                    std::uint16_t v = 0;
                    if (read16(addr, v)) WriteReg40(regIdx, static_cast<std::int32_t>(static_cast<std::uint32_t>(v) << 16));
                }
            }
            const std::int32_t step = isLong ? 4 : 2;
            if (mode == 1u) gpr[asIdx] = static_cast<std::uint32_t>(gpr[asIdx] + step);
            else if (mode == 2u) gpr[asIdx] = static_cast<std::uint32_t>(gpr[asIdx] + static_cast<std::int32_t>(gpr[8]));
            return true;
        }

        // Snapshot inputs before parallel transfer + ALU (hardware executes simultaneously)
        const std::uint32_t oldX0 = x0, oldX1 = x1, oldY0 = y0, oldY1 = y1;
        const std::int64_t oldA0 = a0, oldA1 = a1;
        const std::uint32_t oldM0 = m0, oldM1 = m1;

        // Execute parallel ALU/MAC on pre-transfer operand snapshot
        if (hasAlu) {
            const std::uint32_t aluTop = (op2 >> 12) & 0xFu;
            const std::uint32_t sx = (op2 >> 6) & 0x3u;
            const std::uint32_t sy = (op2 >> 4) & 0x3u;
            const std::uint32_t dz = op2 & 0xFu;

            const auto readSnap = [&](const std::uint32_t sel) -> std::int64_t {
                switch (sel & 0xFu) {
                case 0: return static_cast<std::int32_t>(oldM0);
                case 1: return static_cast<std::int32_t>(oldM1);
                case 2: return oldA0;
                case 3: return oldA1;
                case 4: return static_cast<std::int32_t>(oldX0);
                case 5: return static_cast<std::int32_t>(oldX1);
                case 6: return static_cast<std::int32_t>(oldY0);
                case 7: return static_cast<std::int32_t>(oldY1);
                default: return 0;
                }
            };

            if ((op2 & 0xC000u) == 0x4000u) {
                // PMULS Se, Sf, Dg (fixed-point signed 16x16 -> 32 fractional multiply)
                const std::int32_t se = static_cast<std::int16_t>((readSnap(4u + ((op2 >> 12) & 1u)) >> 16) & 0xFFFF);
                const std::int32_t sf = static_cast<std::int16_t>((readSnap(6u + ((op2 >> 10) & 1u)) >> 16) & 0xFFFF);
                const std::int64_t prod = static_cast<std::int64_t>(se) * static_cast<std::int64_t>(sf) * 2ll;
                WriteReg40(dz & 0x3u, prod);
            } else {
                static constexpr std::uint32_t kSxMap[4] = {4u, 5u, 2u, 3u}; // X0, X1, A0, A1
                static constexpr std::uint32_t kSyMap[4] = {6u, 7u, 0u, 1u}; // Y0, Y1, M0, M1
                const std::uint32_t aluOp = (op2 >> 8) & 0x3Fu;
                const std::int64_t lhs = readSnap(kSxMap[sx]);
                const std::int64_t rhs = readSnap(kSyMap[sy]);
                std::int64_t res = 0;
                bool carry = false;
                bool ovf = false;
                bool writes = true;

                switch (aluOp) {
                case 0x00u: writes = false; break; // NOPX / NOPY
                case 0x01u: // PSUB Sx, Sy, Dz
                    res = lhs - rhs;
                    carry = (static_cast<std::uint64_t>(lhs) < static_cast<std::uint64_t>(rhs));
                    break;
                case 0x02u: // PADD Sx, Sy, Dz
                    res = lhs + rhs;
                    break;
                case 0x03u: // PADDC Sx, Sy, Dz
                    res = lhs + rhs + ((dsr & kDsrDc) ? 1ll : 0ll);
                    carry = ((res >> 32) & 1ll) != 0ll;
                    break;
                case 0x04u: // PSUBC Sx, Sy, Dz
                    res = lhs - rhs - ((dsr & kDsrDc) ? 1ll : 0ll);
                    carry = (res < 0);
                    break;
                case 0x05u: // PAND Sx, Sy, Dz
                    res = (lhs & rhs) & 0xFFFF0000ll;
                    break;
                case 0x06u: // PXOR Sx, Sy, Dz
                    res = (lhs ^ rhs) & 0xFFFF0000ll;
                    break;
                case 0x07u: // POR Sx, Sy, Dz
                    res = (lhs | rhs) & 0xFFFF0000ll;
                    break;
                case 0x08u: // PDEC Sx, Dz
                    res = lhs - 0x00010000ll;
                    break;
                case 0x09u: // PINC Sx, Dz
                    res = lhs + 0x00010000ll;
                    break;
                case 0x0Au: // PCLR Dz
                    res = 0;
                    break;
                case 0x0Bu: // PDMSB Sx, Dz (leading identical sign bits count)
                {
                    const std::uint32_t u = static_cast<std::uint32_t>(lhs & 0xFFFFFFFFull);
                    const bool sign = (u & 0x80000000u) != 0u;
                    int cnt = 0;
                    for (int b = 30; b >= 0; --b) {
                        if ((((u >> b) & 1u) != 0u) == sign) ++cnt;
                        else break;
                    }
                    res = static_cast<std::int64_t>(cnt) << 16;
                    break;
                }
                case 0x0Cu: // PNEG Sx, Dz
                    res = -lhs;
                    break;
                case 0x0Du: // PCOPY Sx, Dz
                    res = lhs;
                    break;
                case 0x0Eu: // PABS Sx, Dz
                    res = (lhs < 0) ? -lhs : lhs;
                    break;
                case 0x0Fu: // PRND Sx, Dz (round lower 16 bits)
                    res = (lhs + 0x8000ll) & ~0xFFFFll;
                    break;
                case 0x10u: // PSHA Sx, Sy, Dz (arithmetic shift by signed shift count in Sy[22:16])
                {
                    const int sh = static_cast<std::int8_t>((rhs >> 16) & 0xFF);
                    if (sh >= 0 && sh < 40) res = lhs << sh;
                    else if (sh < 0 && -sh < 40) res = lhs >> (-sh);
                    else res = (sh >= 0) ? 0 : (lhs < 0 ? -1ll : 0ll);
                    break;
                }
                case 0x11u: // PSHL Sx, Sy, Dz (logical shift)
                {
                    const int sh = static_cast<std::int8_t>((rhs >> 16) & 0xFF);
                    const std::uint32_t u = static_cast<std::uint32_t>(lhs & 0xFFFFFFFFull);
                    if (sh >= 0 && sh < 32) res = static_cast<std::int64_t>(static_cast<std::int32_t>(u << sh));
                    else if (sh < 0 && -sh < 32) res = static_cast<std::int64_t>(static_cast<std::int32_t>(u >> (-sh)));
                    else res = 0;
                    break;
                }
                default:
                    if (aluTop == 0u) writes = false;
                    else res = lhs;
                    break;
                }

                if (writes) {
                    ovf = (res > 0x7FFFFFFFll || res < -0x80000000ll);
                    WriteReg40(dz, res);
                    UpdateFlags(res, ovf, carry);
                }
            }
        }

        // Execute parallel X/Y bus transfers
        if (xOp != 0u) {
            const std::uint32_t addr = gpr[axIdx];
            if (xStore) {
                const std::uint16_t word = static_cast<std::uint16_t>(((dx ? oldA1 : oldA0) >> 16) & 0xFFFFull);
                (void)write16(addr, word);
            } else {
                std::uint16_t word = 0;
                if (read16(addr, word)) {
                    const std::uint32_t v = static_cast<std::uint32_t>(word) << 16;
                    if (dx) x1 = v; else x0 = v;
                }
            }
            if (xOp == 1u) gpr[axIdx] = StepModuloAddress(addr, 2);
            else if (xOp == 2u) gpr[axIdx] = StepModuloAddress(addr, static_cast<std::int32_t>(gpr[8]));
        }
        if (yOp != 0u) {
            const std::uint32_t addr = gpr[ayIdx];
            if (yStore) {
                const std::uint16_t word = static_cast<std::uint16_t>(((dy ? oldA1 : oldA0) >> 16) & 0xFFFFull);
                (void)write16(addr, word);
            } else {
                std::uint16_t word = 0;
                if (read16(addr, word)) {
                    const std::uint32_t v = static_cast<std::uint32_t>(word) << 16;
                    if (dy) y1 = v; else y0 = v;
                }
            }
            if (yOp == 1u) gpr[ayIdx] = StepModuloAddress(addr, 2);
            else if (yOp == 2u) gpr[ayIdx] = StepModuloAddress(addr, static_cast<std::int32_t>(gpr[9]));
        }
        return true;
    }
};

} // namespace cg50
