#pragma once

#include <cstdint>

namespace cg50 {

// ============================================================================
// SH7305 UBC — User Break Controller (0xFF200000..0xFF200620)
// Exact 13 registers verified in Casio CPU73050.dll:
//   0xFF200000 CBR0   Match condition setting 0 (bit 0 CE = channel enable, bits 5:4 ID = 01 fetch / 10 data / 11 both)
//   0xFF200004 CRR0   Match operation setting 0 (bit 0 BIE = break enable)
//   0xFF200008 CAR0   Match address setting 0
//   0xFF20000C CAMR0  Match address mask setting 0 (1 bits masked/ignored)
//   0xFF200020 CBR1   Match condition setting 1
//   0xFF200024 CRR1   Match operation setting 1
//   0xFF200028 CAR1   Match address setting 1
//   0xFF20002C CAMR1  Match address mask setting 1
//   0xFF200030 CDR1   Match data setting 1
//   0xFF200034 CDMR1  Match data mask setting 1
//   0xFF200038 CETR1  Execution count break 1 (bits 11..0)
//   0xFF200600 CCMFR  Channel match flag (bit 0 MF0, bit 1 MF1; write-0 clears)
//   0xFF200620 CBCR   Break control (bit 0 UBDE = user break debug enable)
// ============================================================================
class UbcUnit final {
public:
    void Reset() noexcept
    {
        cbr0_ = crr0_ = car0_ = camr0_ = 0u;
        cbr1_ = crr1_ = car1_ = camr1_ = cdr1_ = cdmr1_ = cetr1_ = 0u;
        ccmfr_ = 0u;
        cbcr_ = 0u;
    }

    static bool MatchesAddress(const std::uint32_t addr) noexcept
    {
        return addr >= 0xFF200000u && addr <= 0xFF200624u;
    }

    bool Read32(const std::uint32_t addr, std::uint32_t& out) const noexcept
    {
        switch (addr) {
        case 0xFF200000u: out = cbr0_; return true;
        case 0xFF200004u: out = crr0_; return true;
        case 0xFF200008u: out = car0_; return true;
        case 0xFF20000Cu: out = camr0_; return true;
        case 0xFF200020u: out = cbr1_; return true;
        case 0xFF200024u: out = crr1_; return true;
        case 0xFF200028u: out = car1_; return true;
        case 0xFF20002Cu: out = camr1_; return true;
        case 0xFF200030u: out = cdr1_; return true;
        case 0xFF200034u: out = cdmr1_; return true;
        case 0xFF200038u: out = cetr1_; return true;
        case 0xFF200600u: out = ccmfr_; return true;
        case 0xFF200620u: out = cbcr_; return true;
        default:          out = 0u; return MatchesAddress(addr);
        }
    }

    bool Write32(const std::uint32_t addr, const std::uint32_t val) noexcept
    {
        switch (addr) {
        case 0xFF200000u: cbr0_ = val; return true;
        case 0xFF200004u: crr0_ = val; return true;
        case 0xFF200008u: car0_ = val; return true;
        case 0xFF20000Cu: camr0_ = val; return true;
        case 0xFF200020u: cbr1_ = val; return true;
        case 0xFF200024u: crr1_ = val; return true;
        case 0xFF200028u: car1_ = val; return true;
        case 0xFF20002Cu: camr1_ = val; return true;
        case 0xFF200030u: cdr1_ = val; return true;
        case 0xFF200034u: cdmr1_ = val; return true;
        case 0xFF200038u: cetr1_ = val & 0x0FFFu; return true;
        case 0xFF200600u: ccmfr_ &= val; return true; // W0C
        case 0xFF200620u: cbcr_ = val & 0x0001u; return true;
        default:          return MatchesAddress(addr);
        }
    }

    bool AnyChannelEnabled() const noexcept
    {
        return ((cbr0_ & 1u) != 0u) || ((cbr1_ & 1u) != 0u);
    }

    // Check instruction fetch breakpoint at pc; returns true if a UBC break exception should fire
    bool CheckFetchMatch(const std::uint32_t pc) noexcept
    {
        bool fire = false;
        if ((cbr0_ & 1u) != 0u) {
            const std::uint32_t id = (cbr0_ >> 4) & 0x3u;
            if ((id == 0u || (id & 1u) != 0u) && ((pc ^ car0_) & ~camr0_) == 0u) {
                ccmfr_ |= 0x0001u;
                if ((crr0_ & 1u) != 0u) fire = true;
            }
        }
        if ((cbr1_ & 1u) != 0u) {
            const std::uint32_t id = (cbr1_ >> 4) & 0x3u;
            if ((id == 0u || (id & 1u) != 0u) && ((pc ^ car1_) & ~camr1_) == 0u) {
                if ((cbr1_ & (1u << 12)) != 0u && cetr1_ > 1u) {
                    --cetr1_;
                } else {
                    ccmfr_ |= 0x0002u;
                    if ((crr1_ & 1u) != 0u) fire = true;
                }
            }
        }
        return fire;
    }

    // Check operand read/write watchpoint
    bool CheckDataMatch(const std::uint32_t addr, const std::uint32_t data, const bool isWrite) noexcept
    {
        bool fire = false;
        const auto checkRw = [&](const std::uint32_t cbr) -> bool {
            const std::uint32_t rw = (cbr >> 1) & 0x3u; // 01=read, 10=write, 11/00=both
            if (rw == 1u && isWrite) return false;
            if (rw == 2u && !isWrite) return false;
            return true;
        };
        if ((cbr0_ & 1u) != 0u && (((cbr0_ >> 4) & 0x2u) != 0u) && checkRw(cbr0_)) {
            if (((addr ^ car0_) & ~camr0_) == 0u) {
                ccmfr_ |= 0x0001u;
                if ((crr0_ & 1u) != 0u) fire = true;
            }
        }
        if ((cbr1_ & 1u) != 0u && (((cbr1_ >> 4) & 0x2u) != 0u) && checkRw(cbr1_)) {
            const bool addrOk = ((addr ^ car1_) & ~camr1_) == 0u;
            const bool dataCheck = (cbr1_ & (1u << 6)) != 0u; // DBE
            const bool dataOk = !dataCheck || (((data ^ cdr1_) & ~cdmr1_) == 0u);
            if (addrOk && dataOk) {
                ccmfr_ |= 0x0002u;
                if ((crr1_ & 1u) != 0u) fire = true;
            }
        }
        return fire;
    }

    std::uint32_t Ccmfr() const noexcept { return ccmfr_; }
    std::uint32_t Cbcr() const noexcept { return cbcr_; }

private:
    std::uint32_t cbr0_ = 0, crr0_ = 0, car0_ = 0, camr0_ = 0;
    std::uint32_t cbr1_ = 0, crr1_ = 0, car1_ = 0, camr1_ = 0;
    std::uint32_t cdr1_ = 0, cdmr1_ = 0, cetr1_ = 0;
    std::uint32_t ccmfr_ = 0;
    std::uint32_t cbcr_ = 0;
};

} // namespace cg50
