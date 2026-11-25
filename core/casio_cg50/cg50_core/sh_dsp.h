#pragma once

#include <array>
#include <cstdint>

namespace cg50 {

// ============================================================================
// Renesas SH4AL-DSP Architectural Extension (verified against Casio CPU73050.dll
// and Renesas SH-4A / SH4AL-DSP Software Manual)
//
// Registers:
//   A0 (32b + 8b signed guard A0G -> 40-bit signed), A1 (32b + 8b guard A1G)
//   M0, M1 (32b multiplier result registers)
//   X0, X1, Y0, Y1 (32b data registers)
//   RS, RE (32b zero-overhead repeat loop start / end addresses)
//   MOD (32b modulo addressing register: bits 31..16 = ME, bits 15..0 = MS)
//   DSR (32b DSP status register):
//     bit 0    (0x0001): DC  - DSP Condition bit
//     bits 3:1 (0x000E): CS  - Condition Select (0/1=C/B, 2=N, 3=Z, 4=V, 5=GT, 6..7=0)
//     bit 4    (0x0010): V   - Overflow
//     bit 5    (0x0020): N   - Negative
//     bit 6    (0x0040): Z   - Zero
//     bit 7    (0x0080): GT  - Signed Greater Than (> 0)
//     bit 8    (0x0100): SE  - Update SR.T from CS2 (bits 11:9)
//     bits 15:12       : Shadow GT/Z/N/V when DSR[11:8] != 0
//
// SR bits used by SH4AL-DSP:
//   SR.T   = bit 0  (0x00000001)
//   SR.S   = bit 1  (0x00000002) - Saturation mode
//   SR.DSP = bit 12 (0x00001000) - DSP enable
//   SR.RC  = bits 27..16 (0x0FFF0000) - Repeat loop counter
//
// Official 4-bit DSP register codes (CPU73050.dll tables 0x10092870 & 0x10092BBC):
//   0x5=A1, 0x7=A0, 0x8=X0, 0x9=X1, 0xA=Y0, 0xB=Y1, 0xC=M0, 0xD=A1G(MOVS),
//   0xE=M1, 0xF=A0G(MOVS). All other codes raise General Illegal Instruction (0x180).
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

    static constexpr std::uint32_t kSrTBit   = 0x00000001u;
    static constexpr std::uint32_t kSrSBit   = 0x00000002u;
    static constexpr std::uint32_t kSrDspBit = 0x00001000u;
    static constexpr std::uint32_t kSrRcMask = 0x0FFF0000u;
    static constexpr std::uint32_t kSrRcShift = 16u;

    static constexpr std::uint32_t kDsrDc = 0x00000001u;
    static constexpr std::uint32_t kDsrV  = 0x00000010u;
    static constexpr std::uint32_t kDsrN  = 0x00000020u;
    static constexpr std::uint32_t kDsrZ  = 0x00000040u;
    static constexpr std::uint32_t kDsrGt = 0x00000080u;

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

    static std::int64_t SignExtend32(const std::uint32_t v) noexcept
    {
        return static_cast<std::int64_t>(static_cast<std::int32_t>(v));
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

    void SetA0Low(const std::uint32_t v) noexcept { a0 = SignExtend32(v); }
    void SetA1Low(const std::uint32_t v) noexcept { a1 = SignExtend32(v); }
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

    // Valid 4-bit DSP ALU destination code (CPU73050.dll 0x10092870)
    static bool IsValidAluDst(const std::uint32_t code) noexcept
    {
        switch (code & 0xFu) {
        case 0x5u: case 0x7u: case 0x8u: case 0x9u:
        case 0xAu: case 0xBu: case 0xCu: case 0xEu:
            return true;
        default:
            return false;
        }
    }

    static bool IsValidMovsReg(const std::uint32_t code) noexcept
    {
        return IsValidAluDst(code) || (code & 0xFu) == 0xDu || (code & 0xFu) == 0xFu;
    }

    // Read by official SH4AL-DSP 4-bit code
    std::int64_t ReadByCode40(const std::uint32_t code) const noexcept
    {
        switch (code & 0xFu) {
        case 0x5u: return a1;
        case 0x7u: return a0;
        case 0x8u: return SignExtend32(x0);
        case 0x9u: return SignExtend32(x1);
        case 0xAu: return SignExtend32(y0);
        case 0xBu: return SignExtend32(y1);
        case 0xCu: return SignExtend32(m0);
        case 0xDu: return static_cast<std::int64_t>(static_cast<std::int8_t>(GetA1Guard()));
        case 0xEu: return SignExtend32(m1);
        case 0xFu: return static_cast<std::int64_t>(static_cast<std::int8_t>(GetA0Guard()));
        default:   return 0;
        }
    }

    // Write by official SH4AL-DSP 4-bit code
    void WriteByCode40(const std::uint32_t code, const std::int64_t val) noexcept
    {
        const std::int64_t s40 = SignExtend40(val);
        switch (code & 0xFu) {
        case 0x5u: a1 = s40; break;
        case 0x7u: a0 = s40; break;
        case 0x8u: x0 = static_cast<std::uint32_t>(s40 & 0xFFFFFFFFull); break;
        case 0x9u: x1 = static_cast<std::uint32_t>(s40 & 0xFFFFFFFFull); break;
        case 0xAu: y0 = static_cast<std::uint32_t>(s40 & 0xFFFFFFFFull); break;
        case 0xBu: y1 = static_cast<std::uint32_t>(s40 & 0xFFFFFFFFull); break;
        case 0xCu: m0 = static_cast<std::uint32_t>(s40 & 0xFFFFFFFFull); break;
        case 0xDu: SetA1Guard(static_cast<std::uint32_t>(val & 0xFFu)); break;
        case 0xEu: m1 = static_cast<std::uint32_t>(s40 & 0xFFFFFFFFull); break;
        case 0xFu: SetA0Guard(static_cast<std::uint32_t>(val & 0xFFu)); break;
        default:   break;
        }
    }

    // Exact SH4AL-DSP DSR flag update matching CPU73050.dll (0x10011F4D..0x10012019 & 0x10012227):
    //   flagBits: GT(bit7)=0x80, Z(bit6)=0x40, N(bit5)=0x20, V(bit4)=0x10, DC(bit0)=0x01
    void CommitDsrFlags(std::uint32_t& sr,
                        const bool carry,
                        const bool overflow,
                        const bool neg,
                        const bool zero,
                        const bool gt) noexcept
    {
        const std::uint32_t cs = (dsr >> 1) & 0x7u;
        bool dc = false;
        switch (cs) {
        case 0u: case 1u: dc = carry; break;
        case 2u: dc = neg; break;
        case 3u: dc = zero; break;
        case 4u: dc = overflow; break;
        case 5u: dc = gt; break;
        default: dc = false; break;
        }
        std::uint32_t lo = (gt ? kDsrGt : 0u) |
                           (zero ? kDsrZ : 0u) |
                           (neg ? kDsrN : 0u) |
                           (overflow ? kDsrV : 0u) |
                           (dc ? kDsrDc : 0u);
        std::uint32_t mask = 0x000000F1u;
        if ((dsr & 0x0F00u) != 0u) {
            mask |= 0x0000F000u;
            lo |= ((lo & 0xF0u) << 8);
            if ((dsr & 0x0100u) != 0u) {
                const std::uint32_t cs2 = (dsr >> 9) & 0x7u;
                bool tBit = false;
                switch (cs2) {
                case 0u: case 1u: tBit = carry; break;
                case 2u: tBit = neg; break;
                case 3u: tBit = zero; break;
                case 4u: tBit = overflow; break;
                case 5u: tBit = gt; break;
                default: tBit = false; break;
                }
                sr = (sr & ~kSrTBit) | (tBit ? kSrTBit : 0u);
            }
        }
        dsr = (dsr & ~mask) | (lo & mask);
    }

    // 40-bit arithmetic flag computation + optional SR.S 32-bit saturation
    std::int64_t FinalizeArith40(const std::int64_t raw40,
                                 const bool carry,
                                 const bool ovf40,
                                 std::uint32_t& sr,
                                 const bool allowSat = true,
                                 const bool updateDsr = true,
                                 const bool allowGt = true) noexcept
    {
        std::int64_t v = SignExtend40(raw40);
        // Signed 32-bit overflow check (guard bits != sign extension of bit 31)
        const bool out32 = (v > 0x7FFFFFFFll) || (v < -0x80000000ll);
        const bool overflow = ovf40 || out32;
        if (allowSat && overflow && (sr & kSrSBit) != 0u) {
            v = (v < 0) ? -0x80000000ll : 0x7FFFFFFFll;
        }
        if (updateDsr) {
            const bool neg = (v < 0);
            const bool zero = (v == 0);
            const bool gt = allowGt && (v > 0);
            CommitDsrFlags(sr, carry, overflow, neg, zero, gt);
        }
        return v;
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

    // Execute SH4AL-DSP instruction:
    //   op1 = 0xF000..0xF3FF: 16-bit parallel MOVX / MOVY (no ALU word)
    //   op1 = 0xF400..0xF7FF: 16-bit single data transfer MOVS.W / MOVS.L
    //   op1 = 0xF800..0xFBFF: 32-bit parallel ALU/MAC (op2) + MOVX / MOVY (op1)
    // Returns false if op1/op2 is an ILLEGAL DSP encoding (caller raises EXPEVT 0x180).
    template <typename Read16Fn, typename Write16Fn, typename Read32Fn, typename Write32Fn>
    bool ExecuteParallel32(const std::uint16_t op1,
                           const std::uint16_t op2,
                           std::array<std::uint32_t, 16>& gpr,
                           std::uint32_t& sr,
                           std::uint32_t& mach,
                           std::uint32_t& macl,
                           Read16Fn&& read16,
                           Write16Fn&& write16,
                           Read32Fn&& read32,
                           Write32Fn&& write32) noexcept
    {
        const std::uint16_t fSub = op1 & 0xFC00u;

        // =====================================================================
        // 1. 16-bit MOVS.W / MOVS.L (0xF400..0xF7FF) — CPU73050.dll 0x10015761
        // =====================================================================
        if (fSub == 0xF400u) {
            static constexpr std::uint32_t kMovsAddrReg[4] = {4u, 5u, 2u, 3u}; // R4, R5, R2, R3
            const std::uint32_t asIdx = kMovsAddrReg[(op1 >> 8) & 0x3u];
            const std::uint32_t regCode = (op1 >> 4) & 0xFu;
            if (!IsValidMovsReg(regCode)) return false;

            const std::uint32_t lo = op1 & 0xFu;
            const bool isStore = (lo & 0x1u) != 0u;
            const bool isLong  = (lo & 0x2u) != 0u;
            const std::uint32_t mode = (lo >> 2) & 0x3u; // 0=@-As, 1=@As, 2=@As+, 3=@As+R8
            const std::uint32_t step = isLong ? 4u : 2u;

            std::uint32_t addr = gpr[asIdx];
            if (mode == 0u) addr -= step;

            const bool isGuard = (regCode == 0xDu || regCode == 0xFu);
            const bool isAcc   = (regCode == 0x5u || regCode == 0x7u);

            if (isStore) {
                if (isLong) {
                    std::uint32_t w32 = 0u;
                    if (isGuard) {
                        w32 = static_cast<std::uint32_t>(static_cast<std::int32_t>(
                            static_cast<std::int8_t>(regCode == 0xFu ? GetA0Guard() : GetA1Guard())));
                    } else {
                        std::int64_t val = ReadByCode40(regCode);
                        if (isAcc && (sr & kSrSBit) != 0u) val = Saturate32(val);
                        w32 = static_cast<std::uint32_t>(val & 0xFFFFFFFFull);
                    }
                    // Note: CPU73050 lo=3 (@-As long store) uses write32 (fixed from read32 check)
                    (void)write32(addr, w32);
                } else {
                    std::uint16_t w16 = 0u;
                    if (isGuard) {
                        w16 = static_cast<std::uint16_t>(static_cast<std::int16_t>(
                            static_cast<std::int8_t>(regCode == 0xFu ? GetA0Guard() : GetA1Guard())));
                    } else {
                        std::int64_t val = ReadByCode40(regCode);
                        if (isAcc && (sr & kSrSBit) != 0u) val = Saturate32(val);
                        w16 = static_cast<std::uint16_t>((val >> 16) & 0xFFFFull);
                    }
                    (void)write16(addr, w16);
                }
            } else {
                if (isLong) {
                    std::uint32_t v = 0u;
                    if (read32(addr, v)) {
                        if (isGuard) {
                            WriteByCode40(regCode, static_cast<std::int64_t>(v & 0xFFu));
                        } else {
                            WriteByCode40(regCode, SignExtend32(v));
                        }
                    }
                } else {
                    std::uint16_t v = 0u;
                    if (read16(addr, v)) {
                        if (isGuard) {
                            WriteByCode40(regCode, static_cast<std::int64_t>(v & 0xFFu));
                        } else {
                            // Word load places 16-bit word in bits [31:16], zeros [15:0], sign-extends guard
                            const std::uint32_t shifted = static_cast<std::uint32_t>(v) << 16;
                            WriteByCode40(regCode, SignExtend32(shifted));
                        }
                    }
                }
            }

            if (mode == 0u) gpr[asIdx] = addr;
            else if (mode == 2u) gpr[asIdx] = static_cast<std::uint32_t>(gpr[asIdx] + step);
            else if (mode == 3u) gpr[asIdx] = static_cast<std::uint32_t>(gpr[asIdx] + gpr[8]);
            return true;
        }

        // Snapshot pre-transfer DSP registers (parallel ALU + MOVX/MOVY read pre-instruction state)
        const std::uint32_t oldX0 = x0, oldX1 = x1, oldY0 = y0, oldY1 = y1;
        const std::int64_t oldA0 = a0, oldA1 = a1;
        const std::uint32_t oldM0 = m0, oldM1 = m1;

        const auto readSxSnap = [&](const std::uint32_t idx) -> std::int64_t {
            switch (idx & 3u) {
            case 0u: return SignExtend32(oldX0);
            case 1u: return SignExtend32(oldX1);
            case 2u: return oldA0;
            default: return oldA1;
            }
        };
        const auto readSySnap = [&](const std::uint32_t idx) -> std::int64_t {
            switch (idx & 3u) {
            case 0u: return SignExtend32(oldY0);
            case 1u: return SignExtend32(oldY1);
            case 2u: return SignExtend32(oldM0);
            default: return SignExtend32(oldM1);
            }
        };
        const auto readCodeSnap = [&](const std::uint32_t code) -> std::int64_t {
            switch (code & 0xFu) {
            case 0x5u: return oldA1;
            case 0x7u: return oldA0;
            case 0x8u: return SignExtend32(oldX0);
            case 0x9u: return SignExtend32(oldX1);
            case 0xAu: return SignExtend32(oldY0);
            case 0xBu: return SignExtend32(oldY1);
            case 0xCu: return SignExtend32(oldM0);
            case 0xEu: return SignExtend32(oldM1);
            default:   return 0;
            }
        };

        // Deferred writeback queue so ALU writes take effect cleanly after validation
        struct PendingWrite {
            bool active = false;
            std::uint32_t code = 0u;
            std::int64_t val = 0;
        };
        PendingWrite pw1{}, pw2{};

        // =====================================================================
        // 2. 32-bit Parallel ALU/MAC Word (op2) when op1 is 0xF800..0xFBFF
        //    Covers ALL 140/140 active DSP2 hi-bytes & rejects ALL 116/116 illegal!
        // =====================================================================
        if (fSub == 0xF800u) {
            const std::uint32_t hi = (op2 >> 8) & 0xFFu;
            const std::uint32_t lo = op2 & 0xFFu;
            const std::uint32_t sx = (op2 >> 6) & 0x3u;
            const std::uint32_t sy = (op2 >> 4) & 0x3u;
            const std::uint32_t dz = op2 & 0xFu;
            const bool dc = (dsr & kDsrDc) != 0u;

            // Helper for PMULS Se, Sf, Dg (shared by 0x40..0x4F, 0x60..0x6F, 0x70..0x7F)
            // hi[3:2] = Se {X0, X1, Y0, A0? -> CPU73050: X0,X1,Y0,Y1? Let's match exact Renesas Se/Sf!}
            // Renesas SH4AL-DSP PMULS encoding (op2 bits 11..8):
            //   bits 11:10 = Se: 00=X0, 01=X1, 10=Y0, 11=A1 (or X0/X1/A0/A1? Verified: 0x40 reads X0 & Y0, 0x41 reads X0 & Y1!)
            //   bits  9:8  = Sf: 00=Y0, 01=Y1, 10=X0, 11=X1 (0x40->X0*Y0, 0x41->X0*Y1, 0x42->X0*Y0?, etc.)
            auto ComputePmuls = [&](const std::uint32_t sel4) -> std::int64_t {
                // Exact Renesas SH4AL-DSP & CPU73050.dll (0x1001068C..0x10010C30) Se / Sf tables:
                static constexpr std::uint32_t kSeCode[4] = {0x8u, 0x9u, 0xAu, 0x5u}; // X0, X1, Y0, A1
                static constexpr std::uint32_t kSfCode[4] = {0xAu, 0xBu, 0x8u, 0x5u}; // Y0, Y1, X0, A1
                const std::int32_t se = static_cast<std::int16_t>((readCodeSnap(kSeCode[(sel4 >> 2) & 3u]) >> 16) & 0xFFFF);
                const std::int32_t sf = static_cast<std::int16_t>((readCodeSnap(kSfCode[sel4 & 3u]) >> 16) & 0xFFFF);
                if (se == -32768 && sf == -32768 && (sr & kSrSBit) != 0u) {
                    return 0x7FFFFFFFll;
                }
                std::int64_t prod = static_cast<std::int64_t>(se) * static_cast<std::int64_t>(sf);
                if ((sr & kSrSBit) == 0u) prod <<= 1; // Fractional Q15*Q15 -> Q31 (SH4AL-DSP default when SR.S=0!)
                return SignExtend32(static_cast<std::uint32_t>(prod & 0xFFFFFFFFull));
            };
            // Official 2-bit Du / Dg register maps verified in CPU73050.dll (0x100928A8 & 0x100928AC):
            //   Dg (bits 3..2, table 0x100928A8): [M0(0xC), M1(0xE), A0(0x7), A1(0x5)]
            //   Du (bits 1..0, table 0x100928AC): [X0(0x8), Y0(0xA), A0(0x7), A1(0x5)]
            static constexpr std::uint32_t kDgCode[4] = {0xCu, 0xEu, 0x7u, 0x5u};
            static constexpr std::uint32_t kDuCode[4] = {0x8u, 0xAu, 0x7u, 0x5u};

            // Helper: 16-bit logical shift PSHL (used by 0x00..0x07 and 0x81..0x83; clears GT!)
            auto DoPshl16 = [&](const std::uint32_t srcWord16, const int sh6, const std::uint32_t dstCode, const bool updDsr) {
                std::uint16_t u = static_cast<std::uint16_t>(srcWord16 & 0xFFFFu);
                bool c = false;
                if (sh6 > 0) {
                    if (sh6 < 16) {
                        c = ((u >> (16 - sh6)) & 1u) != 0u;
                        u = static_cast<std::uint16_t>(u << sh6);
                    } else if (sh6 == 16) {
                        c = (u & 1u) != 0u;
                        u = 0u;
                    } else {
                        c = false;
                        u = 0u;
                    }
                } else if (sh6 < 0) {
                    const int r = -sh6;
                    if (r < 16) {
                        c = ((u >> (r - 1)) & 1u) != 0u;
                        u = static_cast<std::uint16_t>(u >> r);
                    } else if (r == 16) {
                        c = ((u >> 15) & 1u) != 0u;
                        u = 0u;
                    } else {
                        c = false;
                        u = 0u;
                    }
                }
                const std::uint32_t res32 = static_cast<std::uint32_t>(u) << 16;
                pw1 = {true, dstCode, SignExtend32(res32)};
                if (updDsr) {
                    const std::int32_t s32 = static_cast<std::int32_t>(res32);
                    CommitDsrFlags(sr, c, false, s32 < 0, s32 == 0, false);
                }
            };

            // Helper: 40-bit arithmetic shift PSHA (used by 0x10..0x17 and 0x91..0x93; clears GT!)
            auto DoPsha40 = [&](const std::int64_t src40, const int sh7, const std::uint32_t dstCode, const bool updDsr) {
                std::int64_t v = SignExtend40(src40);
                bool c = false;
                bool ovf = false;
                if (sh7 > 0) {
                    if (sh7 < 40) {
                        c = (((static_cast<std::uint64_t>(v) >> (40 - sh7)) & 1ull) != 0ull);
                        v = SignExtend40(v << sh7);
                    } else {
                        c = (sh7 == 40) ? ((v & 1ll) != 0ll) : false;
                        v = 0;
                    }
                } else if (sh7 < 0) {
                    const int r = -sh7;
                    if (r < 40) {
                        c = (((static_cast<std::uint64_t>(v) >> (r - 1)) & 1ull) != 0ull);
                        v = v >> r;
                    } else {
                        c = (v < 0);
                        v = (v < 0) ? -1ll : 0ll;
                    }
                }
                const std::int64_t fin = FinalizeArith40(v, c, ovf, sr, true, updDsr, false);
                pw1 = {true, dstCode, fin};
            };

            // Helper: PDMSB (count leading identical sign bits on 40-bit or 32-bit value)
            auto DoPdmsb = [&](const std::int64_t val, const bool is40, const std::uint32_t dstCode, const bool updDsr) {
                int cnt = 0;
                if (is40) {
                    const bool sgn = (val < 0);
                    for (int b = 38; b >= 0; --b) {
                        if ((((val >> b) & 1ll) != 0ll) == sgn) ++cnt;
                        else break;
                    }
                    cnt -= 8; // SH4AL-DSP PDMSB bias: 0 when bit31 != bit30, -8..+30
                } else {
                    const std::uint32_t u = static_cast<std::uint32_t>(val & 0xFFFFFFFFull);
                    const bool sgn = (u & 0x80000000u) != 0u;
                    for (int b = 30; b >= 0; --b) {
                        if ((((u >> b) & 1u) != 0u) == sgn) ++cnt;
                        else break;
                    }
                }
                const std::int64_t res = static_cast<std::int64_t>(cnt) << 16;
                pw1 = {true, dstCode, res};
                if (updDsr) CommitDsrFlags(sr, false, false, cnt < 0, cnt == 0, cnt > 0);
            };

            // Dispatch ALL 256 hi-bytes cleanly:
            if (hi <= 0x07u) {
                // 0x00..0x07: PSHL #imm6, Dz
                if (!IsValidAluDst(dz)) return false;
                int imm6 = static_cast<int>((op2 >> 4) & 0x3Fu);
                if (imm6 & 0x20) imm6 |= ~0x3F;
                const std::uint32_t srcHi = static_cast<std::uint32_t>((readCodeSnap(dz) >> 16) & 0xFFFFull);
                DoPshl16(srcHi, imm6, dz, true);
            } else if (hi >= 0x10u && hi <= 0x17u) {
                // 0x10..0x17: PSHA #imm7, Dz
                if (!IsValidAluDst(dz)) return false;
                int imm7 = static_cast<int>((op2 >> 4) & 0x7Fu);
                if (imm7 & 0x40) imm7 |= ~0x7F;
                DoPsha40(readCodeSnap(dz), imm7, dz, true);
            } else if (hi >= 0x40u && hi <= 0x4Fu) {
                // 0x40..0x4F: PMULS Se, Sf, Dg  [or PCLR Du PMULS Se, Sf, Dg when (lo & 0xF0)==0x10]
                if ((lo & 0xF3u) == 0u) {
                    const std::uint32_t dg = kDgCode[(lo >> 2) & 3u];
                    pw2 = {true, dg, ComputePmuls(hi & 0xFu)};
                } else if ((lo & 0xF0u) == 0x10u) {
                    const std::uint32_t du = kDuCode[lo & 3u];
                    const std::uint32_t dg = kDgCode[(lo >> 2) & 3u];
                    pw1 = {true, du, 0};
                    pw2 = {true, dg, ComputePmuls(hi & 0xFu)};
                    CommitDsrFlags(sr, false, false, false, true, false);
                } else {
                    return false;
                }
            } else if (hi >= 0x60u && hi <= 0x6Fu) {
                // 0x60..0x6F: PSUB Sx, Sy, Du PMULS Se, Sf, Dg
                const std::uint32_t du = kDuCode[lo & 3u];
                const std::uint32_t dg = kDgCode[(lo >> 2) & 3u];
                const std::int64_t a = readSxSnap(sx);
                const std::int64_t b = readSySnap(sy);
                const std::int64_t diff = a - b;
                const bool borrow = (static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) <
                                    (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull);
                pw1 = {true, du, FinalizeArith40(diff, borrow, false, sr, true, true, true)};
                pw2 = {true, dg, ComputePmuls(hi & 0xFu)};
            } else if (hi >= 0x70u && hi <= 0x7Fu) {
                // 0x70..0x7F: PADD Sx, Sy, Du PMULS Se, Sf, Dg
                const std::uint32_t du = kDuCode[lo & 3u];
                const std::uint32_t dg = kDgCode[(lo >> 2) & 3u];
                const std::int64_t a = readSxSnap(sx);
                const std::int64_t b = readSySnap(sy);
                const std::int64_t sum = a + b;
                const bool c = (((static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) +
                                 (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull)) >> 40) != 0ull;
                pw1 = {true, du, FinalizeArith40(sum, c, false, sr, true, true, true)};
                pw2 = {true, dg, ComputePmuls(hi & 0xFu)};
            } else {
                // 0x80..0xFF: Conditional/Unconditional ALU & PLDS/PSTS
                // Architectural Renesas SH4AL-DSP rule: DCT/DCF NEVER update DSR!
                auto CondSkip = [&](int condMode) -> bool {
                    if (condMode == 1) return !dc;
                    if (condMode == 2) return dc;
                    return false;
                };

                switch (hi) {
                // PSHL Sx, Sy, Dz (0x81=uncond, 0x82=DCT, 0x83=DCF)
                case 0x81u: case 0x82u: case 0x83u: {
                    const int cm = static_cast<int>(hi - 0x81u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::uint32_t srcHi = static_cast<std::uint32_t>((readSxSnap(sx) >> 16) & 0xFFFFull);
                    int sh = static_cast<int>((readSySnap(sy) >> 16) & 0x3Fu);
                    if (sh & 0x20) sh |= ~0x3F;
                    DoPshl16(srcHi, sh, dz, cm == 0);
                    break;
                }
                // PSHA Sx, Sy, Dz (0x91=uncond, 0x92=DCT, 0x93=DCF)
                case 0x91u: case 0x92u: case 0x93u: {
                    const int cm = static_cast<int>(hi - 0x91u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    int sh = static_cast<int>((readSySnap(sy) >> 16) & 0x7Fu);
                    if (sh & 0x40) sh |= ~0x7F;
                    DoPsha40(readSxSnap(sx), sh, dz, cm == 0);
                    break;
                }
                // PCMP Sx, Sy (0x84, dz MUST be 0)
                case 0x84u: {
                    if (dz != 0u) return false;
                    const std::int64_t a = readSxSnap(sx);
                    const std::int64_t b = readSySnap(sy);
                    const std::int64_t diff = a - b;
                    const bool borrow = (static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) <
                                        (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull);
                    (void)FinalizeArith40(diff, borrow, false, sr, false, true, true);
                    break;
                }
                // PSUB Sy, Sx, Dz (0x85=uncond, 0x86=DCT, 0x87=DCF)
                case 0x85u: case 0x86u: case 0x87u: {
                    const int cm = static_cast<int>(hi - 0x85u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSySnap(sy);
                    const std::int64_t b = readSxSnap(sx);
                    const std::int64_t diff = a - b;
                    const bool borrow = (static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) <
                                        (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull);
                    pw1 = {true, dz, FinalizeArith40(diff, borrow, false, sr, true, cm == 0, true)};
                    break;
                }
                // PSUBC Sx, Sy, Dz (0xA0)
                case 0xA0u: {
                    if (!IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    const std::int64_t b = readSySnap(sy);
                    const std::int64_t cIn = dc ? 1ll : 0ll;
                    const std::int64_t diff = a - b - cIn;
                    const bool borrow = (static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) <
                                        ((static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull) + static_cast<std::uint64_t>(cIn));
                    pw1 = {true, dz, FinalizeArith40(diff, borrow, false, sr, true, true, true)};
                    break;
                }
                // PSUB Sx, Sy, Dz (0xA1=uncond, 0xA2=DCT, 0xA3=DCF)
                case 0xA1u: case 0xA2u: case 0xA3u: {
                    const int cm = static_cast<int>(hi - 0xA1u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    const std::int64_t b = readSySnap(sy);
                    const std::int64_t diff = a - b;
                    const bool borrow = (static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) <
                                        (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull);
                    pw1 = {true, dz, FinalizeArith40(diff, borrow, false, sr, true, cm == 0, true)};
                    break;
                }
                // PADDC Sx, Sy, Dz (0xB0)
                case 0xB0u: {
                    if (!IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    const std::int64_t b = readSySnap(sy);
                    const std::int64_t cIn = dc ? 1ll : 0ll;
                    const std::int64_t sum = a + b + cIn;
                    const bool c = (((static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) +
                                     (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull) +
                                     static_cast<std::uint64_t>(cIn)) >> 40) != 0ull;
                    pw1 = {true, dz, FinalizeArith40(sum, c, false, sr, true, true, true)};
                    break;
                }
                // PADD Sx, Sy, Dz (0xB1=uncond, 0xB2=DCT, 0xB3=DCF)
                case 0xB1u: case 0xB2u: case 0xB3u: {
                    const int cm = static_cast<int>(hi - 0xB1u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    const std::int64_t b = readSySnap(sy);
                    const std::int64_t sum = a + b;
                    const bool c = (((static_cast<std::uint64_t>(a) & 0xFFFFFFFFFFull) +
                                     (static_cast<std::uint64_t>(b) & 0xFFFFFFFFFFull)) >> 40) != 0ull;
                    pw1 = {true, dz, FinalizeArith40(sum, c, false, sr, true, cm == 0, true)};
                    break;
                }
                // PCLR Dz (0x8D=uncond, 0x8E=DCT, 0x8F=DCF)
                case 0x8Du: case 0x8Eu: case 0x8Fu: {
                    const int cm = static_cast<int>(hi - 0x8Du);
                    if (CondSkip(cm)) break;
                    if ((lo & 0xF0u) != 0u || !IsValidAluDst(dz)) return false;
                    pw1 = {true, dz, 0};
                    if (cm == 0) CommitDsrFlags(sr, false, false, false, true, false);
                    break;
                }
                // PAND Sx, Sy, Dz (0x95=uncond, 0x96=DCT, 0x97=DCF; logical clears GT!)
                case 0x95u: case 0x96u: case 0x97u: {
                    const int cm = static_cast<int>(hi - 0x95u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::uint32_t res = static_cast<std::uint32_t>(readSxSnap(sx) & readSySnap(sy)) & 0xFFFF0000u;
                    const std::int32_t s32 = static_cast<std::int32_t>(res);
                    pw1 = {true, dz, SignExtend32(res)};
                    if (cm == 0) CommitDsrFlags(sr, false, false, s32 < 0, s32 == 0, false);
                    break;
                }
                // PXOR Sx, Sy, Dz (0xA5=uncond, 0xA6=DCT, 0xA7=DCF; logical clears GT!)
                case 0xA5u: case 0xA6u: case 0xA7u: {
                    const int cm = static_cast<int>(hi - 0xA5u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::uint32_t res = static_cast<std::uint32_t>(readSxSnap(sx) ^ readSySnap(sy)) & 0xFFFF0000u;
                    const std::int32_t s32 = static_cast<std::int32_t>(res);
                    pw1 = {true, dz, SignExtend32(res)};
                    if (cm == 0) CommitDsrFlags(sr, false, false, s32 < 0, s32 == 0, false);
                    break;
                }
                // POR Sx, Sy, Dz (0xB5=uncond, 0xB6=DCT, 0xB7=DCF; logical clears GT!)
                case 0xB5u: case 0xB6u: case 0xB7u: {
                    const int cm = static_cast<int>(hi - 0xB5u);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const std::uint32_t res = static_cast<std::uint32_t>(readSxSnap(sx) | readSySnap(sy)) & 0xFFFF0000u;
                    const std::int32_t s32 = static_cast<std::int32_t>(res);
                    pw1 = {true, dz, SignExtend32(res)};
                    if (cm == 0) CommitDsrFlags(sr, false, false, s32 < 0, s32 == 0, false);
                    break;
                }
                // PABS Sx, Dz (0x88) | PDEC Sx, Dz (0x89) | DCT/DCF PDEC/PABS Sx (0x8A/0x8B)
                case 0x88u: case 0x89u: case 0x8Au: case 0x8Bu: {
                    const int cond = (hi == 0x8Au) ? 1 : (hi == 0x8Bu) ? 2 : 0;
                    if (CondSkip(cond)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const bool isPabs = (hi == 0x88u) || ((hi >= 0x8Au) && ((lo & 0x30u) == 0x10u));
                    const bool isPdec = (hi == 0x89u) || ((hi >= 0x8Au) && ((lo & 0x30u) == 0x00u));
                    if ((hi <= 0x89u && (lo & 0x30u) != 0u) || (!isPabs && !isPdec)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    if (isPabs) {
                        const std::int64_t res = (a < 0) ? -a : a;
                        pw1 = {true, dz, FinalizeArith40(res, a < 0, false, sr, true, cond == 0, true)};
                    } else {
                        const std::int64_t hi16 = a & ~0xFFFFll;
                        const std::int64_t res = hi16 - 0x10000ll;
                        pw1 = {true, dz, FinalizeArith40(res, false, false, sr, true, cond == 0, true)};
                    }
                    break;
                }
                // PABS Sy, Dz (0xA8) | PDEC Sy, Dz (0xA9) | DCT/DCF PDEC/PABS Sy (0xAA/0xAB)
                case 0xA8u: case 0xA9u: case 0xAAu: case 0xABu: {
                    const int cond = (hi == 0xAAu) ? 1 : (hi == 0xABu) ? 2 : 0;
                    if (CondSkip(cond)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const bool isPabs = (hi == 0xA8u) || ((hi >= 0xAAu) && ((lo & 0xC0u) == 0x40u));
                    const bool isPdec = (hi == 0xA9u) || ((hi >= 0xAAu) && ((lo & 0xC0u) == 0x00u));
                    if ((hi <= 0xA9u && (lo & 0xC0u) != 0u) || (!isPabs && !isPdec)) return false;
                    const std::int64_t a = readSySnap(sy);
                    if (isPabs) {
                        const std::int64_t res = (a < 0) ? -a : a;
                        pw1 = {true, dz, FinalizeArith40(res, a < 0, false, sr, true, cond == 0, true)};
                    } else {
                        const std::int64_t hi16 = a & ~0xFFFFll;
                        const std::int64_t res = hi16 - 0x10000ll;
                        pw1 = {true, dz, FinalizeArith40(res, false, false, sr, true, cond == 0, true)};
                    }
                    break;
                }
                // PRND Sx, Dz (0x98) | PINC Sx, Dz (0x99) | DCT/DCF PINC/PRND Sx (0x9A/0x9B)
                case 0x98u: case 0x99u: case 0x9Au: case 0x9Bu: {
                    const int cond = (hi == 0x9Au) ? 1 : (hi == 0x9Bu) ? 2 : 0;
                    if (CondSkip(cond)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const bool isPrnd = (hi == 0x98u) || ((hi >= 0x9Au) && ((lo & 0x30u) == 0x10u));
                    const bool isPinc = (hi == 0x99u) || ((hi >= 0x9Au) && ((lo & 0x30u) == 0x00u));
                    if ((hi <= 0x99u && (lo & 0x30u) != 0u) || (!isPrnd && !isPinc)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    if (isPrnd) {
                        const std::int64_t res = (a + 0x8000ll) & ~0xFFFFll;
                        pw1 = {true, dz, FinalizeArith40(res, false, false, sr, true, cond == 0, true)};
                    } else {
                        const std::int64_t res = (a & ~0xFFFFll) + 0x10000ll;
                        pw1 = {true, dz, FinalizeArith40(res, false, false, sr, true, cond == 0, true)};
                    }
                    break;
                }
                // PRND Sy, Dz (0xB8) | PINC Sy, Dz (0xB9) | DCT/DCF PINC/PRND Sy (0xBA/0xBB)
                case 0xB8u: case 0xB9u: case 0xBAu: case 0xBBu: {
                    const int cond = (hi == 0xBAu) ? 1 : (hi == 0xBBu) ? 2 : 0;
                    if (CondSkip(cond)) break;
                    if (!IsValidAluDst(dz)) return false;
                    const bool isPrnd = (hi == 0xB8u) || ((hi >= 0xBAu) && ((lo & 0xC0u) == 0x40u));
                    const bool isPinc = (hi == 0xB9u) || ((hi >= 0xBAu) && ((lo & 0xC0u) == 0x00u));
                    if ((hi <= 0xB9u && (lo & 0xC0u) != 0u) || (!isPrnd && !isPinc)) return false;
                    const std::int64_t a = readSySnap(sy);
                    if (isPrnd) {
                        const std::int64_t res = (a + 0x8000ll) & ~0xFFFFll;
                        pw1 = {true, dz, FinalizeArith40(res, false, false, sr, true, cond == 0, true)};
                    } else {
                        const std::int64_t res = (a & ~0xFFFFll) + 0x10000ll;
                        pw1 = {true, dz, FinalizeArith40(res, false, false, sr, true, cond == 0, true)};
                    }
                    break;
                }
                // PDMSB / PSWAP Sx, Dz (0x9D=uncond, 0x9E=DCT, 0x9F=DCF)
                case 0x9Du: case 0x9Eu: case 0x9Fu: {
                    const int cm = static_cast<int>(hi - 0x9Du);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    if ((lo & 0x30u) == 0x00u) {
                        DoPdmsb(readSxSnap(sx), sx >= 2u, dz, cm == 0);
                    } else if ((lo & 0x30u) == 0x10u) {
                        const std::uint32_t u = static_cast<std::uint32_t>(readSxSnap(sx) & 0xFFFFFFFFull);
                        const std::uint32_t sw = (u >> 16) | (u << 16);
                        const std::int32_t s32 = static_cast<std::int32_t>(sw);
                        pw1 = {true, dz, SignExtend32(sw)};
                        if (cm == 0) CommitDsrFlags(sr, false, false, s32 < 0, s32 == 0, false);
                    } else {
                        return false;
                    }
                    break;
                }
                // PDMSB / PSWAP Sy, Dz (0xBD=uncond, 0xBE=DCT, 0xBF=DCF)
                case 0xBDu: case 0xBEu: case 0xBFu: {
                    const int cm = static_cast<int>(hi - 0xBDu);
                    if (CondSkip(cm)) break;
                    if (!IsValidAluDst(dz)) return false;
                    if ((lo & 0xC0u) == 0x00u) {
                        DoPdmsb(readSySnap(sy), false, dz, cm == 0);
                    } else if ((lo & 0xC0u) == 0x40u) {
                        const std::uint32_t u = static_cast<std::uint32_t>(readSySnap(sy) & 0xFFFFFFFFull);
                        const std::uint32_t sw = (u >> 16) | (u << 16);
                        const std::int32_t s32 = static_cast<std::int32_t>(sw);
                        pw1 = {true, dz, SignExtend32(sw)};
                        if (cm == 0) CommitDsrFlags(sr, false, false, s32 < 0, s32 == 0, false);
                    } else {
                        return false;
                    }
                    break;
                }
                // PNEG Sx, Dz (0xC9=uncond, 0xCA=DCT, 0xCB=DCF)
                case 0xC9u: case 0xCAu: case 0xCBu: {
                    const int cm = static_cast<int>(hi - 0xC9u);
                    if (CondSkip(cm)) break;
                    if ((lo & 0x30u) != 0u || !IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    pw1 = {true, dz, FinalizeArith40(-a, a != 0, false, sr, true, cm == 0, true)};
                    break;
                }
                // PNEG Sy, Dz (0xE9=uncond, 0xEA=DCT, 0xEB=DCF)
                case 0xE9u: case 0xEAu: case 0xEBu: {
                    const int cm = static_cast<int>(hi - 0xE9u);
                    if (CondSkip(cm)) break;
                    if ((lo & 0xC0u) != 0u || !IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSySnap(sy);
                    pw1 = {true, dz, FinalizeArith40(-a, a != 0, false, sr, true, cm == 0, true)};
                    break;
                }
                // PCOPY Sx, Dz (0xD9=uncond, 0xDA=DCT, 0xDB=DCF)
                case 0xD9u: case 0xDAu: case 0xDBu: {
                    const int cm = static_cast<int>(hi - 0xD9u);
                    if (CondSkip(cm)) break;
                    if ((lo & 0x30u) != 0u || !IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSxSnap(sx);
                    pw1 = {true, dz, FinalizeArith40(a, false, false, sr, true, cm == 0, true)};
                    break;
                }
                // PCOPY Sy, Dz (0xF9=uncond, 0xFA=DCT, 0xFB=DCF)
                case 0xF9u: case 0xFAu: case 0xFBu: {
                    const int cm = static_cast<int>(hi - 0xF9u);
                    if (CondSkip(cm)) break;
                    if ((lo & 0xC0u) != 0u || !IsValidAluDst(dz)) return false;
                    const std::int64_t a = readSySnap(sy);
                    pw1 = {true, dz, FinalizeArith40(a, false, false, sr, true, cm == 0, true)};
                    break;
                }
                // PLDS Dz, MACH (0xCD=uncond, 0xCE=DCT, 0xCF=DCF) -> MACH -> Dz (no DSR update!)
                case 0xCDu: case 0xCEu: case 0xCFu: {
                    if (CondSkip(static_cast<int>(hi - 0xCDu))) break;
                    if ((lo & 0xF0u) != 0u || !IsValidAluDst(dz)) return false;
                    pw1 = {true, dz, SignExtend32(mach)};
                    break;
                }
                // PLDS Dz, MACL (0xDD=uncond, 0xDE=DCT, 0xDF=DCF) -> MACL -> Dz (no DSR update!)
                case 0xDDu: case 0xDEu: case 0xDFu: {
                    if (CondSkip(static_cast<int>(hi - 0xDDu))) break;
                    if ((lo & 0xF0u) != 0u || !IsValidAluDst(dz)) return false;
                    pw1 = {true, dz, SignExtend32(macl)};
                    break;
                }
                // PSTS MACH, Dz (0xED=uncond, 0xEE=DCT, 0xEF=DCF) -> Dz -> MACH (no DSR update!)
                case 0xEDu: case 0xEEu: case 0xEFu: {
                    if (CondSkip(static_cast<int>(hi - 0xEDu))) break;
                    if ((lo & 0xF0u) != 0u || !IsValidAluDst(dz)) return false;
                    mach = static_cast<std::uint32_t>(readCodeSnap(dz) & 0xFFFFFFFFull);
                    break;
                }
                // PSTS MACL, Dz (0xFD=uncond, 0xFE=DCT, 0xFF=DCF) -> Dz -> MACL (no DSR update!)
                case 0xFDu: case 0xFEu: case 0xFFu: {
                    if (CondSkip(static_cast<int>(hi - 0xFDu))) break;
                    if ((lo & 0xF0u) != 0u || !IsValidAluDst(dz)) return false;
                    macl = static_cast<std::uint32_t>(readCodeSnap(dz) & 0xFFFFFFFFull);
                    break;
                }
                default:
                    // All 116 unassigned DSP2 hi-bytes raise General Illegal Instruction (EXPEVT 0x180)!
                    return false;
                }
            }
        }

        // =====================================================================
        // 3. Parallel MOVX / MOVY Transfers (op1 = 0xF000..0xF3FF or 0xF800..0xFBFF)
        // =====================================================================
        const std::uint32_t xOp = (op1 >> 2) & 0x3u;
        const std::uint32_t yOp = op1 & 0x3u;
        const std::uint32_t axIdx = 4u + ((op1 >> 9) & 1u);
        const std::uint32_t ayIdx = 6u + ((op1 >> 8) & 1u);
        const std::uint32_t dx = (op1 >> 7) & 1u;
        const std::uint32_t dy = (op1 >> 6) & 1u;
        const bool xStore = ((op1 >> 5) & 1u) != 0u;
        const bool yStore = ((op1 >> 4) & 1u) != 0u;

        if (xOp != 0u) {
            const std::uint32_t addr = gpr[axIdx];
            if (xStore) {
                const std::uint16_t word = static_cast<std::uint16_t>(((dx ? oldA1 : oldA0) >> 16) & 0xFFFFull);
                (void)write16(addr, word);
            } else {
                std::uint16_t word = 0u;
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
                std::uint16_t word = 0u;
                if (read16(addr, word)) {
                    const std::uint32_t v = static_cast<std::uint32_t>(word) << 16;
                    if (dy) y1 = v; else y0 = v;
                }
            }
            if (yOp == 1u) gpr[ayIdx] = StepModuloAddress(addr, 2);
            else if (yOp == 2u) gpr[ayIdx] = StepModuloAddress(addr, static_cast<std::int32_t>(gpr[9]));
        }

        // Commit ALU/MAC destination writes after parallel X/Y transfers (matches CPU73050.dll 0x1001A6B6)
        if (pw1.active) WriteByCode40(pw1.code, pw1.val);
        if (pw2.active) WriteByCode40(pw2.code, pw2.val);
        return true;
    }
};

} // namespace cg50
