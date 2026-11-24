#pragma once

#include "cg50_usb2.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cg50 {

// ============================================================================
// SH7305 INTC2 — External Interrupt Controller (0xA4140000 / phys 0x04140000)
// Exact 7 registers confirmed in Casio CPU73050.dll:
//   ICR0 (0x00), INTPRI00 (0x10), ICR1 (0x1C), INTREQ00 (0x24),
//   INTMSK00 (0x44), INTMSKCLR00 (0x64), NMIFCR (0xC0), plus USERIMASK (0x1000).
// ============================================================================
class Intc2Unit final {
public:
    static constexpr std::uint32_t kEventIrq0 = 0x0600u; // IRQ0..7 = 0x600 + i*0x20
    static constexpr std::uint32_t kEventNmi  = 0x01C0u;

    void Reset() noexcept
    {
        icr0_ = 0x0000u;
        icr1_ = 0x0000u;
        intpri00_ = 0x00000000u;
        intreq00_ = 0x00u;
        intmsk00_ = 0xFFu; // All external IRQ0..7 masked at reset
        nmifcr_ = 0x0000u;
        userImask_ = 0x00000000u;
        raw_.fill(0u);
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return (phys >= 0x04140000u && phys < 0x04140100u) ||
               phys == 0x04141000u;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        if (phys == 0x04140024u) { out = intreq00_; return true; }
        if (phys == 0x04140044u) { out = intmsk00_; return true; }
        if (phys == 0x04140064u) { out = 0u; return true; }
        if (phys >= 0x04140000u && phys < 0x04140100u) {
            out = raw_[phys - 0x04140000u];
            return true;
        }
        return false;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (phys == 0x04140024u) {
            intreq00_ &= val; // W0C
            return true;
        }
        if (phys == 0x04140044u) {
            intmsk00_ |= val;
            return true;
        }
        if (phys == 0x04140064u) {
            intmsk00_ &= static_cast<std::uint8_t>(~val);
            return true;
        }
        if (phys >= 0x04140000u && phys < 0x04140100u) {
            raw_[phys - 0x04140000u] = val;
            return true;
        }
        return false;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (phys == 0x04140000u) { out = icr0_; return true; }
        if (phys == 0x0414001Cu) { out = icr1_; return true; }
        if (phys == 0x041400C0u) { out = nmifcr_; return true; }
        if (phys >= 0x04140000u && phys + 1u < 0x04140100u) {
            const std::size_t off = phys - 0x04140000u;
            out = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(raw_[off]) << 8) | raw_[off + 1u]);
            return true;
        }
        return false;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (phys == 0x04140000u) { icr0_ = val; return true; }
        if (phys == 0x0414001Cu) { icr1_ = val; return true; }
        if (phys == 0x041400C0u) { nmifcr_ = val; return true; }
        if (phys >= 0x04140000u && phys + 1u < 0x04140100u) {
            const std::size_t off = phys - 0x04140000u;
            raw_[off] = static_cast<std::uint8_t>(val >> 8);
            raw_[off + 1u] = static_cast<std::uint8_t>(val & 0xFFu);
            return true;
        }
        return false;
    }

    bool Read32(const std::uint32_t phys, std::uint32_t& out) const noexcept
    {
        if (phys == 0x04140010u) { out = intpri00_; return true; }
        if (phys == 0x04141000u) { out = userImask_; return true; }
        if (phys >= 0x04140000u && phys + 3u < 0x04140100u) {
            const std::size_t off = phys - 0x04140000u;
            out = (static_cast<std::uint32_t>(raw_[off]) << 24) |
                  (static_cast<std::uint32_t>(raw_[off + 1u]) << 16) |
                  (static_cast<std::uint32_t>(raw_[off + 2u]) << 8) |
                  static_cast<std::uint32_t>(raw_[off + 3u]);
            return true;
        }
        return false;
    }

    bool Write32(const std::uint32_t phys, const std::uint32_t val) noexcept
    {
        if (phys == 0x04140010u) { intpri00_ = val; return true; }
        if (phys == 0x04141000u) {
            if ((val & 0xFFFF0000u) == 0xA5000000u) {
                userImask_ = val & 0x000000F0u;
            }
            return true;
        }
        if (phys >= 0x04140000u && phys + 3u < 0x04140100u) {
            const std::size_t off = phys - 0x04140000u;
            raw_[off] = static_cast<std::uint8_t>((val >> 24) & 0xFFu);
            raw_[off + 1u] = static_cast<std::uint8_t>((val >> 16) & 0xFFu);
            raw_[off + 2u] = static_cast<std::uint8_t>((val >> 8) & 0xFFu);
            raw_[off + 3u] = static_cast<std::uint8_t>(val & 0xFFu);
            return true;
        }
        return false;
    }

    void RaiseExternalIrq(const std::size_t irq) noexcept
    {
        if (irq < 8u) intreq00_ |= static_cast<std::uint8_t>(1u << irq);
    }

    void ClearExternalIrq(const std::size_t irq) noexcept
    {
        if (irq < 8u) intreq00_ &= static_cast<std::uint8_t>(~(1u << irq));
    }

    void TriggerNmi() noexcept { nmifcr_ |= 0x0001u; }
    [[nodiscard]] bool AnyPending() const noexcept
    {
        return ((nmifcr_ & 0x0001u) != 0u) || ((intreq00_ & static_cast<std::uint8_t>(~intmsk00_)) != 0u);
    }
    bool ConsumeNmi() noexcept
    {
        if ((nmifcr_ & 0x0001u) == 0u) return false;
        nmifcr_ &= static_cast<std::uint16_t>(~0x0001u);
        return true;
    }

    bool HighestPendingExternalIrq(std::uint32_t& outEvent, std::uint8_t& outPrio, std::size_t& outIrq) const noexcept
    {
        const std::uint8_t active = static_cast<std::uint8_t>(intreq00_ & ~intmsk00_);
        if (active == 0u) return false;
        std::uint8_t bestPrio = 0u;
        std::size_t bestIrq = 0u;
        for (std::size_t i = 0; i < 8u; ++i) {
            if ((active & (1u << i)) == 0u) continue;
            const std::uint8_t p = ExternalIrqPriority(i);
            if (p > bestPrio) {
                bestPrio = p;
                bestIrq = i;
            }
        }
        if (bestPrio == 0u) return false;
        outIrq = bestIrq;
        outPrio = bestPrio;
        outEvent = kEventIrq0 + static_cast<std::uint32_t>(bestIrq) * 0x20u;
        return true;
    }

    std::uint8_t IntMsk00() const noexcept { return intmsk00_; }
    std::uint8_t IntReq00() const noexcept { return intreq00_; }
    std::uint32_t IntPri00() const noexcept { return intpri00_; }
    std::uint32_t UserImask() const noexcept { return userImask_; }
    std::uint8_t ExternalIrqPriority(const std::size_t irq) const noexcept
    {
        if (irq >= 8u) return 0u;
        if ((intmsk00_ & (1u << irq)) != 0u) return 0u;
        return static_cast<std::uint8_t>((intpri00_ >> ((7u - irq) * 4u)) & 0xFu);
    }

private:
    std::uint16_t icr0_ = 0;
    std::uint16_t icr1_ = 0;
    std::uint32_t intpri00_ = 0;
    std::uint8_t intreq00_ = 0;
    std::uint8_t intmsk00_ = 0xFFu;
    std::uint16_t nmifcr_ = 0;
    std::uint32_t userImask_ = 0;
    std::array<std::uint8_t, 256> raw_{};
};

// ============================================================================
// SH7305 ETMU — 6 Extra 32-bit RTC-Clocked Timers (0xA44D0030..0xA44D00EF)
// Confirmed in CPU73050.dll: RTSTR0..5, RTCOR0..5, RTCNT0..5, RTCR0..5
// ============================================================================
class EtmuUnit final {
public:
    static constexpr std::size_t kChannelCount = 6u;
    static constexpr std::uint32_t kBasePhysical = 0x044D0030u;
    static constexpr std::uint32_t kChannelStride = 0x20u;
    static constexpr std::uint32_t kEndPhysical = kBasePhysical + kChannelCount * kChannelStride;
    static constexpr std::uint64_t kCyclesPerTick = 1024u;

    struct Channel {
        std::uint8_t tstr = 0;
        std::uint8_t tcr = 0;
        std::uint32_t tcor = 0xFFFFFFFFu;
        std::uint32_t tcnt = 0xFFFFFFFFu;
        std::uint64_t prescaler = 0;
    };

    void Reset() noexcept
    {
        for (auto& ch : channels_) ch = Channel{};
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= kBasePhysical && phys < kEndPhysical;
    }

    static constexpr std::uint32_t EventCode(const std::size_t ch) noexcept
    {
        constexpr std::array<std::uint32_t, kChannelCount> kEvents = {
            0x9E0u, 0xC20u, 0xC40u, 0x900u, 0xD00u, 0xFA0u
        };
        return ch < kChannelCount ? kEvents[ch] : 0u;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::size_t ch = (phys - kBasePhysical) / kChannelStride;
        const std::uint32_t off = (phys - kBasePhysical) % kChannelStride;
        if (off == 0x00u) { out = channels_[ch].tstr; return true; }
        if (off == 0x0Cu) { out = channels_[ch].tcr; return true; }
        out = 0u;
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::size_t ch = (phys - kBasePhysical) / kChannelStride;
        const std::uint32_t off = (phys - kBasePhysical) % kChannelStride;
        if (off == 0x00u) {
            channels_[ch].tstr = val & 0x01u;
            if ((val & 0x01u) == 0u) channels_[ch].prescaler = 0u;
            return true;
        }
        if (off == 0x0Cu) {
            const std::uint8_t unf = (channels_[ch].tcr & 0x02u) & (val & 0x02u);
            channels_[ch].tcr = static_cast<std::uint8_t>((val & 0x01u) | unf);
            return true;
        }
        return true;
    }

    bool Read32(const std::uint32_t phys, std::uint32_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::size_t ch = (phys - kBasePhysical) / kChannelStride;
        const std::uint32_t off = (phys - kBasePhysical) % kChannelStride;
        if (off == 0x04u) { out = channels_[ch].tcor; return true; }
        if (off == 0x08u) { out = channels_[ch].tcnt; return true; }
        if (off == 0x00u) { out = channels_[ch].tstr; return true; }
        if (off == 0x0Cu) { out = channels_[ch].tcr; return true; }
        out = 0u;
        return true;
    }

    bool Write32(const std::uint32_t phys, const std::uint32_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::size_t ch = (phys - kBasePhysical) / kChannelStride;
        const std::uint32_t off = (phys - kBasePhysical) % kChannelStride;
        if (off == 0x04u) { channels_[ch].tcor = val; return true; }
        if (off == 0x08u) { channels_[ch].tcnt = val; return true; }
        if (off == 0x00u) return Write8(phys, static_cast<std::uint8_t>(val & 0xFFu));
        if (off == 0x0Cu) return Write8(phys, static_cast<std::uint8_t>(val & 0xFFu));
        return true;
    }

    std::uint32_t Tick(const std::uint64_t cycles) noexcept
    {
        if (cycles == 0u) return 0u;
        const std::uint8_t anyRun = static_cast<std::uint8_t>(
            channels_[0].tstr | channels_[1].tstr | channels_[2].tstr |
            channels_[3].tstr | channels_[4].tstr | channels_[5].tstr);
        if ((anyRun & 0x01u) == 0u) return 0u;
        std::uint32_t irqMask = 0u;
        for (std::size_t i = 0; i < kChannelCount; ++i) {
            auto& ch = channels_[i];
            if ((ch.tstr & 0x01u) == 0u) continue;
            ch.prescaler += cycles;
            const std::uint64_t ticks = ch.prescaler / kCyclesPerTick;
            if (ticks == 0u) continue;
            ch.prescaler %= kCyclesPerTick;
            if (ticks > static_cast<std::uint64_t>(ch.tcnt)) {
                ch.tcr |= 0x02u; // UNF
                if ((ch.tcr & 0x01u) != 0u) {
                    irqMask |= (1u << i);
                }
                const std::uint64_t period = static_cast<std::uint64_t>(ch.tcor) + 1ull;
                const std::uint64_t rem = (ticks - static_cast<std::uint64_t>(ch.tcnt) - 1ull) % period;
                ch.tcnt = static_cast<std::uint32_t>(ch.tcor - rem);
            } else {
                ch.tcnt -= static_cast<std::uint32_t>(ticks);
            }
        }
        return irqMask;
    }

    const Channel& GetChannel(const std::size_t idx) const noexcept { return channels_[idx]; }

private:
    std::array<Channel, kChannelCount> channels_{};
};

// ============================================================================
// SH7305 ADC — 10-bit Battery & Power Sensing (0xA4610080..0xA461008E)
// Exact 8 registers verified in Casio CPU73050.dll:
//   0x04610080 ADDRA (AN0 = Touch.TXR)
//   0x04610082 ADDRB (AN1 = MEM.RYBY -> 0x3FF ready)
//   0x04610084 ADDRC (AN2 = Power.CH3 -> battery voltage, 100% = 0x390)
//   0x04610086 ADDRD (AN3 = Touch.TYD)
//   0x04610088 ADCSR (bit 5 ADST, bit 6 ADIE, bit 7 ADF)
//   0x0461008A ADCCSR
//   0x0461008C ADCUST
//   0x0461008E ADPCTL
// Legacy offsets 0x00..0x0A alias to ADDRA..ADCCSR for backward unit tests.
// ============================================================================
class AdcUnit final {
public:
    static constexpr std::uint16_t kHealthyBatteryRaw10 = 0x0390u; // ~6.0V (4xAAA full)

    void Reset() noexcept
    {
        batteryRaw10_ = kHealthyBatteryRaw10;
        RefreshChannels();
        adcsr_ = 0x0000u;
        adccsr_ = 0x0000u;
        adcust_ = 0x0000u;
        adpctl_ = 0x0000u;
        raw_.fill(0u);
    }

    void SetBatteryPercent(const int percent) noexcept
    {
        const int clamped = (percent < 0) ? 0 : (percent > 100 ? 100 : percent);
        // Map 0..100% -> 0x0240 (~3.8V low) .. 0x0390 (~6.0V full)
        batteryRaw10_ = static_cast<std::uint16_t>(0x0240u + (clamped * (0x0390u - 0x0240u)) / 100u);
        RefreshChannels();
    }

    bool HasPendingIrq() const noexcept
    {
        return (adcsr_ & 0x00C0u) == 0x00C0u; // ADF (bit 7) && ADIE (bit 6)
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04610000u && phys < 0x04610100u;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04610000u;
        // Exact CPU73050.dll offsets (0x80..0x8E) + legacy alias (0x00..0x0A)
        if ((off >= 0x80u && off <= 0x86u && (off & 1u) == 0u) ||
            (off <= 0x06u && (off & 1u) == 0u)) {
            out = addr_[(off & 0x06u) / 2u];
            return true;
        }
        if (off == 0x88u || off == 0x08u) { out = adcsr_; return true; }
        if (off == 0x8Au || off == 0x0Au) { out = adccsr_; return true; }
        if (off == 0x8Cu) { out = adcust_; return true; }
        if (off == 0x8Eu) { out = adpctl_; return true; }
        out = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(raw_[off]) << 8) | raw_[off + 1u]);
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04610000u;
        if (off == 0x88u || off == 0x08u) {
            if ((val & 0x0020u) != 0u) {
                RefreshChannels();
                adcsr_ = static_cast<std::uint16_t>((val & ~0x0020u) | 0x0080u);
            } else {
                const std::uint16_t adf = (adcsr_ & 0x0080u) & (val & 0x0080u);
                adcsr_ = static_cast<std::uint16_t>((val & ~0x0080u) | adf);
            }
            return true;
        }
        if (off == 0x8Au || off == 0x0Au) { adccsr_ = val; return true; }
        if (off == 0x8Cu) { adcust_ = val; return true; }
        if (off == 0x8Eu) { adpctl_ = val; return true; }
        raw_[off] = static_cast<std::uint8_t>(val >> 8);
        raw_[off + 1u] = static_cast<std::uint8_t>(val & 0xFFu);
        return true;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        std::uint16_t w = 0;
        if (!Read16(phys & ~1u, w)) return false;
        out = ((phys & 1u) == 0u) ? static_cast<std::uint8_t>(w >> 8)
                                  : static_cast<std::uint8_t>(w & 0xFFu);
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t aligned = phys & ~1u;
        std::uint16_t w = 0;
        (void)Read16(aligned, w);
        if ((phys & 1u) == 0u) {
            w = static_cast<std::uint16_t>((static_cast<std::uint16_t>(val) << 8) | (w & 0xFFu));
        } else {
            w = static_cast<std::uint16_t>((w & 0xFF00u) | val);
        }
        return Write16(aligned, w);
    }

    std::uint16_t Adcsr() const noexcept { return adcsr_; }
    std::uint16_t ChannelResult(const std::size_t ch) const noexcept
    {
        return ch < addr_.size() ? addr_[ch] : 0u;
    }

private:
    void RefreshChannels() noexcept
    {
        addr_[0] = static_cast<std::uint16_t>(batteryRaw10_ << 6); // AN0 (Touch.TXR / fallback)
        addr_[1] = static_cast<std::uint16_t>(0x03FFu << 6);       // AN1 (MEM.RYBY = Ready)
        addr_[2] = static_cast<std::uint16_t>(batteryRaw10_ << 6); // AN2 (Power.CH3 battery)
        addr_[3] = static_cast<std::uint16_t>(batteryRaw10_ << 6); // AN3 (Touch.TYD)
    }

    std::array<std::uint16_t, 4> addr_{};
    std::uint16_t batteryRaw10_ = kHealthyBatteryRaw10;
    std::uint16_t adcsr_ = 0;
    std::uint16_t adccsr_ = 0;
    std::uint16_t adcust_ = 0;
    std::uint16_t adpctl_ = 0;
    std::array<std::uint8_t, 256> raw_{};
};

// ============================================================================
// SH7305 DD Bus / External Clock Controller (0xA44C0000 / phys 0x044C0000)
// Exact 6 registers confirmed in Casio CPU73050.dll:
//   0x044C0000 DDCLKR0 (16b)   0x044C0002 DDCLKR1 (16b)   0x044C0004 DDCLKR2 (16b)
//   0x044C0006 DDCS_CNTR (8b)  0x044C0008 HIZ_CNTR (8b)   0x044C0020 DDCK_CNTR (8b)
// Keeps backward-compatible tick counters in raw_ for legacy TpuUnit tests.
// ============================================================================
class DdBusUnit final {
public:
    void Reset() noexcept
    {
        ddclkr0_ = ddclkr1_ = ddclkr2_ = 0u;
        ddcsCntr_ = hizCntr_ = ddckCntr_ = 0u;
        raw_.fill(0u);
        prescaler_ = 0u;
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x044C0000u && phys < 0x044C0200u;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { out = static_cast<std::uint8_t>(ddclkr0_ >> 8); return true; }
        if (off == 0x01u) { out = static_cast<std::uint8_t>(ddclkr0_ & 0xFFu); return true; }
        if (off == 0x06u) { out = ddcsCntr_; return true; }
        if (off == 0x08u) { out = hizCntr_; return true; }
        if (off == 0x20u) { out = ddckCntr_; return true; }
        out = raw_[off];
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { ddclkr0_ = static_cast<std::uint16_t>((val << 8) | (ddclkr0_ & 0xFFu)); return true; }
        if (off == 0x01u) { ddclkr0_ = static_cast<std::uint16_t>((ddclkr0_ & 0xFF00u) | val); return true; }
        if (off == 0x06u) { ddcsCntr_ = val; return true; }
        if (off == 0x08u) { hizCntr_ = val; return true; }
        if (off == 0x20u) { ddckCntr_ = val; return true; }
        raw_[off] = val;
        return true;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys) || phys + 1u >= 0x044C0200u) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { out = ddclkr0_; return true; }
        if (off == 0x02u) { out = ddclkr1_; return true; }
        if (off == 0x04u) { out = ddclkr2_; return true; }
        out = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(raw_[off]) << 8) | raw_[off + 1u]);
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys) || phys + 1u >= 0x044C0200u) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { ddclkr0_ = val; return true; }
        if (off == 0x02u) { ddclkr1_ = val; return true; }
        if (off == 0x04u) { ddclkr2_ = val; return true; }
        raw_[off] = static_cast<std::uint8_t>(val >> 8);
        raw_[off + 1u] = static_cast<std::uint8_t>(val & 0xFFu);
        return true;
    }

    void Tick(const std::uint64_t cycles) noexcept
    {
        if (ddclkr0_ == 0u || cycles == 0u) return;
        prescaler_ += cycles;
        const std::uint64_t inc = prescaler_ / 64u;
        if (inc == 0u) return;
        prescaler_ %= 64u;
        static constexpr std::array<std::uint32_t, 4> kTcntOff = {0x20u, 0x60u, 0xA0u, 0xE0u};
        for (std::size_t ch = 0; ch < 4u; ++ch) {
            if ((ddclkr0_ & (1u << ch)) == 0u) continue;
            const std::uint32_t o = kTcntOff[ch];
            std::uint16_t cur = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(raw_[o]) << 8) | raw_[o + 1u]);
            cur = static_cast<std::uint16_t>(cur + static_cast<std::uint16_t>(inc));
            raw_[o] = static_cast<std::uint8_t>(cur >> 8);
            raw_[o + 1u] = static_cast<std::uint8_t>(cur & 0xFFu);
        }
    }

    std::uint16_t Tstr() const noexcept { return ddclkr0_; }
    std::uint16_t Ddclkr0() const noexcept { return ddclkr0_; }
    std::uint8_t DdcsCntr() const noexcept { return ddcsCntr_; }
    std::uint8_t DdckCntr() const noexcept { return ddckCntr_; }

private:
    std::uint16_t ddclkr0_ = 0;
    std::uint16_t ddclkr1_ = 0;
    std::uint16_t ddclkr2_ = 0;
    std::uint8_t ddcsCntr_ = 0;
    std::uint8_t hizCntr_ = 0;
    std::uint8_t ddckCntr_ = 0;
    std::array<std::uint8_t, 512> raw_{};
    std::uint64_t prescaler_ = 0;
};

using TpuUnit = DdBusUnit;

// ============================================================================
// SH7305 SDHI — SD Host Interface (0xA4CF0000 / phys 0x04CF0000..0x04CF00FF)
// Observed live in OS 3.80 boot and 24-app cert2900 (0x04CF001C SD_INFO1,
// 0x04CF0020 SD_INFO1_MASK, 0x04CF0022 SD_INFO2_MASK). Supports optional
// virtual SD card insertion (disabled by default on fx-CG50 hardware).
// ============================================================================
class SdhiUnit final {
public:
    static constexpr std::uint16_t kInfo1RespEnd = 0x0001u;
    static constexpr std::uint16_t kInfo1CardRem = 0x0008u;
    static constexpr std::uint16_t kInfo1CardIns = 0x0010u;
    static constexpr std::uint16_t kInfo1CardDet = 0x0020u;
    static constexpr std::uint16_t kInfo2SclkDivEn = 0x2000u;

    void Reset() noexcept
    {
        regs_.fill(0u);
        virtualCardPresent_ = false;
        // No SD card inserted on fx-CG50 by default (CD=0, masks default 0xFFFF)
        regs_[0x1Eu / 2u] = kInfo2SclkDivEn;
        regs_[0x20u / 2u] = 0xFFFFu;
        regs_[0x22u / 2u] = 0xFFFFu;
        regs_[0x26u / 2u] = 512u; // SD_SIZE = 512 bytes
        regs_[0xE0u / 2u] = 0x0007u; // SD_SOFT_RST normal
    }

    void SetVirtualCardInserted(const bool inserted) noexcept
    {
        if (inserted == virtualCardPresent_) return;
        virtualCardPresent_ = inserted;
        if (inserted) {
            regs_[0x1Cu / 2u] |= static_cast<std::uint16_t>(kInfo1CardDet | kInfo1CardIns);
            regs_[0x1Cu / 2u] &= static_cast<std::uint16_t>(~kInfo1CardRem);
        } else {
            regs_[0x1Cu / 2u] &= static_cast<std::uint16_t>(~kInfo1CardDet);
            regs_[0x1Cu / 2u] |= kInfo1CardRem;
        }
    }

    bool VirtualCardInserted() const noexcept { return virtualCardPresent_; }

    bool HasPendingIrq() const noexcept
    {
        const std::uint16_t s1 = static_cast<std::uint16_t>(regs_[0x1Cu / 2u] & ~regs_[0x20u / 2u]);
        const std::uint16_t s2 = static_cast<std::uint16_t>((regs_[0x1Eu / 2u] & ~kInfo2SclkDivEn) & ~regs_[0x22u / 2u]);
        return (s1 != 0u) || (s2 != 0u);
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04CF0000u && phys < 0x04CF0100u;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04CF0000u;
        out = regs_[(off / 2u) & 0x7Fu];
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04CF0000u;
        if (off == 0x00u) {
            // SD_CMD: execute basic SD command response when virtual card is present
            regs_[0] = val;
            if (virtualCardPresent_) {
                regs_[0x1Cu / 2u] |= kInfo1RespEnd;
                const std::uint16_t cmdIdx = val & 0x003Fu;
                regs_[0x0Cu / 2u] = (cmdIdx == 8u) ? 0x01AAu : 0x0900u; // SD_RSP0
            } else {
                regs_[0x1Eu / 2u] |= 0x0040u; // RSP_TO response timeout
            }
            return true;
        }
        if (off == 0x1Cu) {
            // SD_INFO1: write-0 clears event bits; bit 5 (CD) tracks physical card presence
            const std::uint16_t cd = virtualCardPresent_ ? kInfo1CardDet : 0u;
            regs_[0x1Cu / 2u] = static_cast<std::uint16_t>((regs_[0x1Cu / 2u] & val & ~kInfo1CardDet) | cd);
            return true;
        }
        if (off == 0x1Eu) {
            // SD_INFO2: write-0 clears error bits; SCLKDIVEN stays ready
            regs_[0x1Eu / 2u] = static_cast<std::uint16_t>((regs_[0x1Eu / 2u] & val) | kInfo2SclkDivEn);
            return true;
        }
        if (off == 0xE0u) {
            // SD_SOFT_RST: bit 0 = 0 triggers internal soft reset
            regs_[0xE0u / 2u] = val;
            if ((val & 0x0001u) == 0u) {
                const bool card = virtualCardPresent_;
                Reset();
                if (card) SetVirtualCardInserted(true);
            }
            return true;
        }
        regs_[(off / 2u) & 0x7Fu] = val;
        return true;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        std::uint16_t w = 0u;
        if (!Read16(phys & ~1u, w)) return false;
        out = ((phys & 1u) == 0u) ? static_cast<std::uint8_t>(w >> 8)
                                  : static_cast<std::uint8_t>(w & 0xFFu);
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        const std::uint32_t aligned = phys & ~1u;
        std::uint16_t w = 0u;
        if (!Read16(aligned, w)) return false;
        if ((phys & 1u) == 0u) {
            w = static_cast<std::uint16_t>((static_cast<std::uint16_t>(val) << 8) | (w & 0xFFu));
        } else {
            w = static_cast<std::uint16_t>((w & 0xFF00u) | val);
        }
        return Write16(aligned, w);
    }

private:
    std::array<std::uint16_t, 128> regs_{};
    bool virtualCardPresent_ = false;
};

// ============================================================================
// SH7305 H-UDI — User Debugging Interface (0xFC110000..0xFC1100FF)
// Exact 4 registers verified in Casio CPU73050.dll & CGM.dlm (_SDIRlow=0x00):
//   0xFC110000 SDIR  (16b) Debug instruction register
//   0xFC110008 SDDRH (16b) Data register high (W_CB 0x28A09)
//   0xFC11000A SDDRL (16b) Data register low
//   0xFC110018 SDINT (16b) Interrupt source register (bit 0 INTREQ W0C)
// ============================================================================
class HudiUnit final {
public:
    void Reset() noexcept
    {
        sdir_ = 0x0000u; // CGM.dlm _SDIRlow=0x00
        sddrh_ = 0x0000u;
        sddrl_ = 0x0000u;
        sdint_ = 0x0000u;
    }

    static bool MatchesAddress(const std::uint32_t addr) noexcept
    {
        return addr >= 0xFC110000u && addr < 0xFC110100u;
    }

    bool Read16(const std::uint32_t addr, std::uint16_t& out) const noexcept
    {
        if (!MatchesAddress(addr)) return false;
        switch (addr & 0xFFu) {
        case 0x00u: out = sdir_; return true;
        case 0x08u: out = sddrh_; return true;
        case 0x0Au: out = sddrl_; return true;
        case 0x18u: out = sdint_; return true;
        default:    out = 0u; return true;
        }
    }

    bool Write16(const std::uint32_t addr, const std::uint16_t val) noexcept
    {
        if (!MatchesAddress(addr)) return false;
        switch (addr & 0xFFu) {
        case 0x00u: sdir_ = val; return true;
        case 0x08u: sddrh_ = val; return true;
        case 0x0Au: sddrl_ = val; return true;
        case 0x18u: sdint_ &= static_cast<std::uint16_t>(val & 0x0001u); return true; // W0C INTREQ
        default:    return true;
        }
    }

    bool Read32(const std::uint32_t addr, std::uint32_t& out) const noexcept
    {
        if (!MatchesAddress(addr)) return false;
        if ((addr & 0xFFu) == 0x08u) {
            out = (static_cast<std::uint32_t>(sddrh_) << 16) | sddrl_;
            return true;
        }
        std::uint16_t hi = 0u;
        (void)Read16(addr, hi);
        out = hi;
        return true;
    }

    bool Write32(const std::uint32_t addr, const std::uint32_t val) noexcept
    {
        if (!MatchesAddress(addr)) return false;
        if ((addr & 0xFFu) == 0x08u) {
            sddrh_ = static_cast<std::uint16_t>(val >> 16);
            sddrl_ = static_cast<std::uint16_t>(val & 0xFFFFu);
            return true;
        }
        return Write16(addr, static_cast<std::uint16_t>(val & 0xFFFFu));
    }

    bool Read8(const std::uint32_t addr, std::uint8_t& out) const noexcept
    {
        std::uint16_t w = 0u;
        if (!Read16(addr & ~1u, w)) return false;
        out = ((addr & 1u) == 0u) ? static_cast<std::uint8_t>(w >> 8)
                                  : static_cast<std::uint8_t>(w & 0xFFu);
        return true;
    }

    bool Write8(const std::uint32_t addr, const std::uint8_t val) noexcept
    {
        const std::uint32_t aligned = addr & ~1u;
        std::uint16_t w = 0u;
        if (!Read16(aligned, w)) return false;
        if ((addr & 1u) == 0u) {
            w = static_cast<std::uint16_t>((static_cast<std::uint16_t>(val) << 8) | (w & 0xFFu));
        } else {
            w = static_cast<std::uint16_t>((w & 0xFF00u) | val);
        }
        return Write16(aligned, w);
    }

    void RaiseDebugInterrupt() noexcept { sdint_ |= 0x0001u; }
    bool HasPendingIrq() const noexcept { return (sdint_ & 0x0001u) != 0u; }
    std::uint16_t Sdir() const noexcept { return sdir_; }

private:
    std::uint16_t sdir_ = 0u;
    std::uint16_t sddrh_ = 0u;
    std::uint16_t sddrl_ = 0u;
    std::uint16_t sdint_ = 0u;
};

// ============================================================================
// SH7305 PRLCK — Bus Priority Lock Controller (0xFF800018..0xFF800048)
// Exact 7 registers verified in Casio CPU73050.dll (W_CB 0x47A73..0x47B3B):
//   0xFF800018 PRLCKCR  (32b, mask 0x3F: bit i locks PRPRICR[i])
//   0xFF800020..0xFF800048 (step 8) PRPRICR0..5 (32b, mask 0x3F)
// ============================================================================
class PrlckUnit final {
public:
    static constexpr std::uint32_t kMask = 0x0000003Fu;

    void Reset() noexcept
    {
        prlckcr_ = 0u;
        prpricr_.fill(0u);
        blockedWrites_ = 0u;
    }

    static bool MatchesAddress(const std::uint32_t addr) noexcept
    {
        return addr >= 0xFF800000u && addr < 0xFF800100u;
    }

    bool Read32(const std::uint32_t addr, std::uint32_t& out) const noexcept
    {
        if (!MatchesAddress(addr)) return false;
        const std::uint32_t off = addr & 0xFFu;
        if (off == 0x18u) { out = prlckcr_; return true; }
        if (off >= 0x20u && off <= 0x48u && ((off - 0x20u) % 8u) == 0u) {
            out = prpricr_[(off - 0x20u) / 8u];
            return true;
        }
        out = 0u;
        return true;
    }

    bool Write32(const std::uint32_t addr, const std::uint32_t val) noexcept
    {
        if (!MatchesAddress(addr)) return false;
        const std::uint32_t off = addr & 0xFFu;
        if (off == 0x18u) {
            prlckcr_ = val & kMask;
            return true;
        }
        if (off >= 0x20u && off <= 0x48u && ((off - 0x20u) % 8u) == 0u) {
            const std::size_t idx = (off - 0x20u) / 8u;
            if ((prlckcr_ & (1u << idx)) != 0u) {
                ++blockedWrites_;
                return true; // Locked by PRLCKCR bit idx
            }
            prpricr_[idx] = val & kMask;
            return true;
        }
        return true;
    }

    bool IsGroupLocked(const std::size_t idx) const noexcept
    {
        return idx < 6u && ((prlckcr_ & (1u << idx)) != 0u);
    }

    std::uint32_t Priority(const std::size_t idx) const noexcept
    {
        return idx < 6u ? prpricr_[idx] : 0u;
    }

    std::uint32_t BlockedWrites() const noexcept { return blockedWrites_; }

private:
    std::uint32_t prlckcr_ = 0u;
    std::array<std::uint32_t, 6> prpricr_{};
    std::uint32_t blockedWrites_ = 0u;
};

// ============================================================================
// Casio ExtLY755A Board Power / Reset / Battery Signal State (CGM.dlm)
// Wires PTJ6 (POWERON), PTH5 (DDPOWER), PTL1/PTU1 (USBPW/USB_DET),
// PTJ2/PTU2/PTU3 (RS232), PTR3 (_AppIF), and V3SYS battery sense.
// ============================================================================
struct ExtLy755aState {
    bool powerOn = true;   // PTJ6
    bool ddPower = true;   // PTH5
    bool usbPower = false; // PTL1 / PTU1
    bool appIfPin = false; // PTR3 (_AppIF)
    bool batteryLowV3Sys = false;

    void Reset() noexcept
    {
        powerOn = true;
        ddPower = true;
        usbPower = false;
        appIfPin = false;
        batteryLowV3Sys = false;
    }
};

// ============================================================================
// SH7305 RTC Extended Alarm / Control / Adjust Registers (0x0413FED0..0xFEE6)
// Full 18+1 register silicon state matching CPU73050.dll.
// ============================================================================
struct RtcExtState {
    std::uint8_t rsecar = 0u; // 0x0413FED0
    std::uint8_t rminar = 0u; // 0x0413FED2
    std::uint8_t rhrar  = 0u; // 0x0413FED4
    std::uint8_t rwkar  = 0u; // 0x0413FED6
    std::uint8_t rdayar = 0u; // 0x0413FED8
    std::uint8_t rmonar = 0u; // 0x0413FEDA
    std::uint8_t rcr1   = 0u; // 0x0413FEDC (CF=0x80, CIE=0x10, AIE=0x08, AF=0x01)
    std::uint8_t rcr2   = 0x09u; // 0x0413FEDE (PEF=0x80, PES=0x70, RTCEN=0x08, ADJ=0x04, RESET=0x02, START=0x01)
    std::uint16_t ryrar = 0u; // 0x0413FEE0
    std::uint8_t rcr3   = 0u; // 0x0413FEE4 (YAENB=0x80)
    std::uint8_t radj   = 0u; // 0x0413FEE6
    std::uint8_t pendingIrqMask = 0u; // bit0=Alarm(0x480), bit1=Periodic(0x4A0), bit2=Carry(0x4C0)
    std::uint64_t periodicAccum = 0u;
    std::int64_t lastAlarmSecond = -1;

    void Reset() noexcept
    {
        rsecar = rminar = rhrar = rwkar = rdayar = rmonar = 0u;
        rcr1 = 0u;
        rcr2 = 0x09u; // RTCEN=1, START=1
        ryrar = 0u;
        rcr3 = 0u;
        radj = 0u;
        pendingIrqMask = 0u;
        periodicAccum = 0u;
        lastAlarmSecond = -1;
    }
};

// ============================================================================
// SH7305 DMAC0 Extended Silicon State (DMAOR + DMARS0..2 + Round-Robin + AE)
// ============================================================================
struct Dmac0ExtState {
    std::uint16_t dmaor = 0x0001u; // DME=1 at boot-ready default; PR[1:0] bits 9:8, AE=bit2, NMIF=bit1
    std::array<std::uint16_t, 3> dmars{}; // DMARS0..2 @ 0x04090000, 0x04090004, 0x04090008
    std::uint8_t lastServedChannel = 5u;
    std::uint32_t addressErrorCount = 0u;
    std::uint32_t arbitrationCount = 0u;

    void Reset() noexcept
    {
        dmaor = 0x0001u;
        dmars.fill(0u);
        lastServedChannel = 5u;
        addressErrorCount = 0u;
        arbitrationCount = 0u;
    }
};

// ============================================================================
// SH7305 SCIF0 — 3-pin Link Serial Controller (0xA4410000 / phys 0x04410000)
// Supports 16-byte TX/RX FIFOs, hardware loopback (SCFCR.LOOP), baud pacing,
// RXI/TXI/ERI/BRI interrupt requests (kEventScif0), and peer-to-peer virtual
// 3-pin link between two Cg50NativeCore instances or an E-CON4 probe emulator.
// ============================================================================
class Scif0Unit final {
public:
    static constexpr std::size_t kFifoCap = 16u;

    void Reset() noexcept
    {
        scsmr_ = 0x0000u;
        scbrr_ = 0xFFu;
        scscr_ = 0x0000u;
        scfsr_ = 0x0060u; // TDFE | TEND set on reset
        scfcr_ = 0x0000u;
        sclsr_ = 0x0000u;
        txHead_ = txCount_ = 0u;
        rxHead_ = rxCount_ = 0u;
        txFifo_.fill(0u);
        rxFifo_.fill(0u);
        baudAccum_ = 0u;
        // Preserve peer_ connection across Reset unless explicitly detached
    }

    void ConnectPeer(Scif0Unit* peer) noexcept { peer_ = peer; }
    Scif0Unit* Peer() const noexcept { return peer_; }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04410000u && phys < 0x04410040u;
    }

    bool HasPendingIrq() const noexcept
    {
        if ((scscr_ & 0x00C8u) == 0u) return false;
        UpdateStatusFlags();
        // SCSCR: bit 7 TIE, bit 6 RIE, bit 3 REIE
        const bool rxIrq = ((scscr_ & 0x0040u) != 0u) && ((scfsr_ & 0x0003u) != 0u);
        const bool txIrq = ((scscr_ & 0x0080u) != 0u) && ((scfsr_ & 0x0020u) != 0u);
        const bool errIrq = ((scscr_ & 0x0008u) != 0u) && (((scfsr_ & 0x009Cu) != 0u) || ((sclsr_ & 0x0001u) != 0u));
        return rxIrq || txIrq || errIrq;
    }

    // Advance baud-rate transmitter pacing when connected to a peer
    void Tick(const std::uint64_t cycles) noexcept
    {
        if (txCount_ == 0u || cycles == 0u) return;
        const std::uint64_t cyclesPerByte = static_cast<std::uint64_t>(scbrr_ + 1u) * 32u;
        baudAccum_ += cycles;
        while (txCount_ > 0u && baudAccum_ >= cyclesPerByte) {
            baudAccum_ -= cyclesPerByte;
            const std::uint8_t byte = txFifo_[txHead_];
            txHead_ = (txHead_ + 1u) % kFifoCap;
            --txCount_;
            if (peer_ != nullptr) {
                peer_->PushRx(byte);
            }
        }
        UpdateStatusFlags();
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04410000u;
        switch (off) {
        case 0x04u: out = scbrr_; return true;
        case 0x14u: // SCFRDR (RX FIFO pop)
            if (rxCount_ > 0u) {
                out = rxFifo_[rxHead_];
                rxHead_ = (rxHead_ + 1u) % kFifoCap;
                --rxCount_;
                UpdateStatusFlags();
            } else {
                out = 0u;
            }
            return true;
        default: {
            std::uint16_t w = 0u;
            if (!Read16(phys & ~1u, w)) return false;
            out = ((phys & 1u) == 0u) ? static_cast<std::uint8_t>(w >> 8)
                                      : static_cast<std::uint8_t>(w & 0xFFu);
            return true;
        }
        }
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04410000u;
        switch (off) {
        case 0x04u: scbrr_ = val; return true;
        case 0x0Cu: // SCFTDR (TX FIFO push)
            if ((scfcr_ & 0x0001u) != 0u) {
                PushRx(val);
            } else if (peer_ != nullptr) {
                if (txCount_ < kFifoCap) {
                    txFifo_[(txHead_ + txCount_) % kFifoCap] = val;
                    ++txCount_;
                }
            } else {
                // Detached cable: drain cleanly so Link/E-CON4 never stall
                txHead_ = 0u;
                txCount_ = 0u;
            }
            UpdateStatusFlags();
            return true;
        default: {
            const std::uint32_t aligned = phys & ~1u;
            std::uint16_t w = 0u;
            (void)Read16(aligned, w);
            if ((phys & 1u) == 0u) {
                w = static_cast<std::uint16_t>((static_cast<std::uint16_t>(val) << 8) | (w & 0xFFu));
            } else {
                w = static_cast<std::uint16_t>((w & 0xFF00u) | val);
            }
            return Write16(aligned, w);
        }
        }
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04410000u;
        switch (off) {
        case 0x00u: out = scsmr_; return true;
        case 0x04u: out = scbrr_; return true;
        case 0x08u: out = scscr_; return true;
        case 0x10u: UpdateStatusFlags(); out = scfsr_; return true;
        case 0x18u: out = scfcr_; return true;
        case 0x1Cu: out = static_cast<std::uint16_t>((txCount_ << 8) | rxCount_); return true;
        case 0x24u: out = sclsr_; return true;
        default:    out = 0u; return true;
        }
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04410000u;
        switch (off) {
        case 0x00u: scsmr_ = val; return true;
        case 0x04u: scbrr_ = static_cast<std::uint8_t>(val & 0xFFu); return true;
        case 0x08u: scscr_ = val; return true;
        case 0x10u:
            scfsr_ &= val;
            UpdateStatusFlags();
            return true;
        case 0x18u:
            scfcr_ = val;
            if ((val & 0x0002u) != 0u) { rxHead_ = rxCount_ = 0u; } // RFRST
            if ((val & 0x0004u) != 0u) { txHead_ = txCount_ = 0u; } // TFRST
            UpdateStatusFlags();
            return true;
        case 0x24u: sclsr_ &= val; return true;
        default:    return true;
        }
    }

    void PushRx(const std::uint8_t byte) noexcept
    {
        if (rxCount_ < kFifoCap) {
            rxFifo_[(rxHead_ + rxCount_) % kFifoCap] = byte;
            ++rxCount_;
        } else {
            sclsr_ |= 0x0001u; // ORER overrun
        }
        UpdateStatusFlags();
    }

    void InjectFramingError() noexcept { scfsr_ |= 0x0008u; } // FER
    void InjectParityError() noexcept  { scfsr_ |= 0x0004u; } // PER
    void InjectBreak() noexcept        { scfsr_ |= 0x0010u; } // BRK

    std::size_t RxCount() const noexcept { return rxCount_; }
    std::size_t TxCount() const noexcept { return txCount_; }
    bool HasRxData() const noexcept { return rxCount_ > 0u; }

    bool PopRxByte(std::uint8_t& out) noexcept
    {
        if (rxCount_ == 0u) return false;
        out = rxFifo_[rxHead_];
        rxHead_ = (rxHead_ + 1u) % kFifoCap;
        --rxCount_;
        UpdateStatusFlags();
        return true;
    }

private:
    void UpdateStatusFlags() const noexcept
    {
        if (txCount_ == 0u) {
            scfsr_ |= 0x0060u; // TDFE | TEND
        } else {
            scfsr_ &= static_cast<std::uint16_t>(~0x0040u); // Clear TEND while transmitting
        }
        if (rxCount_ > 0u) {
            scfsr_ |= 0x0003u; // RDF | DR
        } else {
            scfsr_ &= static_cast<std::uint16_t>(~0x0003u);
        }
    }

    std::uint16_t scsmr_ = 0;
    std::uint8_t scbrr_ = 0xFFu;
    std::uint16_t scscr_ = 0;
    mutable std::uint16_t scfsr_ = 0x0060u;
    std::uint16_t scfcr_ = 0;
    std::uint16_t sclsr_ = 0;
    std::array<std::uint8_t, kFifoCap> txFifo_{};
    mutable std::array<std::uint8_t, kFifoCap> rxFifo_{};
    std::size_t txHead_ = 0;
    std::size_t txCount_ = 0;
    mutable std::size_t rxHead_ = 0;
    mutable std::size_t rxCount_ = 0;
    std::uint64_t baudAccum_ = 0;
    Scif0Unit* peer_ = nullptr;
};

} // namespace cg50
