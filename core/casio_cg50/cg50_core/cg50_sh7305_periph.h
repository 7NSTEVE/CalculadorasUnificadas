#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace cg50 {

// ============================================================================
// SH7305 INTC2 — External Interrupt Controller (0xA4140000 / phys 0x04140000)
// ============================================================================
class Intc2Unit final {
public:
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
            // INTREQ00: writing 0 clears request bits; writing 1 leaves unchanged
            intreq00_ &= val;
            return true;
        }
        if (phys == 0x04140044u) {
            // INTMSK00: writing 1 sets mask bits
            intmsk00_ |= val;
            return true;
        }
        if (phys == 0x04140064u) {
            // INTMSKCLR00: writing 1 clears mask bits
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

    std::uint8_t IntMsk00() const noexcept { return intmsk00_; }
    std::uint8_t IntReq00() const noexcept { return intreq00_; }
    std::uint32_t IntPri00() const noexcept { return intpri00_; }
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
// SH7305 ETMU — 6 Extra 32-bit Countdown Timers (0xA44D0030..0xA44D00EF)
// Channel c in [0..5] base = 0x044D0030 + c * 0x20:
//   +0x00 TSTR (8-bit):  bit 0 = STR (running)
//   +0x04 TCOR (32-bit): reload constant
//   +0x08 TCNT (32-bit): countdown counter (32.768 kHz clock)
//   +0x0C TCR  (8-bit):  bit 0 = UNIE (IRQ enable), bit 1 = UNF (underflow)
// ============================================================================
class EtmuUnit final {
public:
    static constexpr std::size_t kChannelCount = 6u;
    static constexpr std::uint32_t kBasePhysical = 0x044D0030u;
    static constexpr std::uint32_t kChannelStride = 0x20u;
    static constexpr std::uint32_t kEndPhysical = kBasePhysical + kChannelCount * kChannelStride;
    static constexpr std::uint64_t kCyclesPerTick = 1024u; // ~32.768 kHz @ ~33 MHz

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
            // bit 0 = UNIE (R/W), bit 1 = UNF (write 0 clears, write 1 keeps)
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

    // Returns bitmask (bits 0..5) of channels that underflowed with UNIE=1
    std::uint32_t Tick(const std::uint64_t cycles) noexcept
    {
        if (cycles == 0u) return 0u;
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
// SH7305 ADC — 10-bit Battery & Power Sensing (0xA4610000 / phys 0x04610000)
// ============================================================================
class AdcUnit final {
public:
    static constexpr std::uint16_t kHealthyBatteryRaw10 = 0x0390u; // ~6.0V (4xAAA full)

    void Reset() noexcept
    {
        addr_.fill(static_cast<std::uint16_t>(kHealthyBatteryRaw10 << 6));
        adcsr_ = 0x0000u;
        adcr_ = 0x0000u;
        ext88_ = 0x0000u;
        ext8A_ = 0x0000u;
        ext8C_ = 0x0000u;
        raw_.fill(0u);
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04610000u && phys < 0x04610100u;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04610000u;
        if (off <= 0x06u && (off & 1u) == 0u) {
            out = addr_[off / 2u];
            return true;
        }
        if (off == 0x08u) { out = adcsr_; return true; }
        if (off == 0x0Au) { out = adcr_; return true; }
        if (off == 0x88u) { out = ext88_; return true; }
        if (off == 0x8Au) { out = ext8A_; return true; }
        if (off == 0x8Cu) { out = ext8C_; return true; }
        out = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(raw_[off]) << 8) | raw_[off + 1u]);
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04610000u;
        if (off == 0x08u) {
            // ADCSR: bit 5 = ADST (start conversion), bit 7 = ADF (complete)
            if ((val & 0x0020u) != 0u) {
                addr_.fill(static_cast<std::uint16_t>(kHealthyBatteryRaw10 << 6));
                adcsr_ = static_cast<std::uint16_t>((val & ~0x0020u) | 0x0080u);
            } else {
                const std::uint16_t adf = (adcsr_ & 0x0080u) & (val & 0x0080u);
                adcsr_ = static_cast<std::uint16_t>((val & ~0x0080u) | adf);
            }
            return true;
        }
        if (off == 0x0Au) { adcr_ = val; return true; }
        if (off == 0x88u) { ext88_ = val; return true; }
        if (off == 0x8Au) {
            // Casio power/ADC control: if start bit written, keep ready state clean
            ext8A_ = val;
            return true;
        }
        if (off == 0x8Cu) { ext8C_ = val; return true; }
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
    std::array<std::uint16_t, 4> addr_{};
    std::uint16_t adcsr_ = 0;
    std::uint16_t adcr_ = 0;
    std::uint16_t ext88_ = 0;
    std::uint16_t ext8A_ = 0;
    std::uint16_t ext8C_ = 0;
    std::array<std::uint8_t, 256> raw_{};
};

// ============================================================================
// SH7305 TPU — Timer Pulse Unit & Backlight PWM (0xA44C0000 / phys 0x044C0000)
// ============================================================================
class TpuUnit final {
public:
    void Reset() noexcept
    {
        tstr_ = 0u;
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
        if (off == 0x00u) { out = static_cast<std::uint8_t>(tstr_ >> 8); return true; }
        if (off == 0x01u) { out = static_cast<std::uint8_t>(tstr_ & 0xFFu); return true; }
        out = raw_[off];
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { tstr_ = static_cast<std::uint16_t>((val << 8) | (tstr_ & 0xFFu)); return true; }
        if (off == 0x01u) { tstr_ = static_cast<std::uint16_t>((tstr_ & 0xFF00u) | val); return true; }
        raw_[off] = val;
        return true;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys) || phys + 1u >= 0x044C0200u) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { out = tstr_; return true; }
        out = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(raw_[off]) << 8) | raw_[off + 1u]);
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys) || phys + 1u >= 0x044C0200u) return false;
        const std::uint32_t off = phys - 0x044C0000u;
        if (off == 0x00u) { tstr_ = val; return true; }
        raw_[off] = static_cast<std::uint8_t>(val >> 8);
        raw_[off + 1u] = static_cast<std::uint8_t>(val & 0xFFu);
        return true;
    }

    void Tick(const std::uint64_t cycles) noexcept
    {
        if (tstr_ == 0u || cycles == 0u) return;
        prescaler_ += cycles;
        const std::uint64_t inc = prescaler_ / 64u;
        if (inc == 0u) return;
        prescaler_ %= 64u;
        // Increment TCNT (+0x10 within each 0x40-stride channel: 0x10+0x10=0x20, etc.)
        static constexpr std::array<std::uint32_t, 4> kTcntOff = {0x20u, 0x60u, 0xA0u, 0xE0u};
        for (std::size_t ch = 0; ch < 4u; ++ch) {
            if ((tstr_ & (1u << ch)) == 0u) continue;
            const std::uint32_t o = kTcntOff[ch];
            std::uint16_t cur = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(raw_[o]) << 8) | raw_[o + 1u]);
            cur = static_cast<std::uint16_t>(cur + static_cast<std::uint16_t>(inc));
            raw_[o] = static_cast<std::uint8_t>(cur >> 8);
            raw_[o + 1u] = static_cast<std::uint8_t>(cur & 0xFFu);
        }
    }

    std::uint16_t Tstr() const noexcept { return tstr_; }

private:
    std::uint16_t tstr_ = 0;
    std::array<std::uint8_t, 512> raw_{};
    std::uint64_t prescaler_ = 0;
};

// ============================================================================
// SH7305 SCIF0 — 3-pin Link Serial Controller (0xA4410000 / phys 0x04410000)
// Supports 16-byte TX/RX FIFOs and hardware loopback mode (SCFCR.LOOP bit 0).
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
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04410000u && phys < 0x04410040u;
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
                // Hardware loopback mode: feed TX directly into RX FIFO
                PushRx(val);
            } else if (txCount_ < kFifoCap) {
                // External cable detached: byte transmits cleanly (TX stays ready)
                txFifo_[(txHead_ + txCount_) % kFifoCap] = val;
                // Drain immediately so non-loopback TX never blocks Link/E-CON4
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
            // SCFSR: write-0 clears status flags
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

    std::size_t RxCount() const noexcept { return rxCount_; }

private:
    void UpdateStatusFlags() const noexcept
    {
        if (txCount_ == 0u) {
            scfsr_ |= 0x0060u; // TDFE | TEND
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
};

// ============================================================================
// SH7305 USB 2.0 Function Controller (0xA4D80000 / phys 0x04D80000)
// Clean cable-detached hardware register model (no stalls during OS polling).
// ============================================================================
class Usb2Unit final {
public:
    void Reset() noexcept
    {
        regs_.fill(0u);
        regs_[0x22u / 2u] = 0x2000u; // CFIFOCTR: FRDY=1
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04D80000u && phys < 0x04D80100u;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x04u) {
            // SYSSTS0: LNST=00 (SE0 / cable detached)
            out = 0x0000u;
            return true;
        }
        out = regs_[(off / 2u) & 0x7Fu];
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x40u || off == 0x42u || off == 0x46u || off == 0x48u || off == 0x4Au) {
            // INTSTS0/BRDYSTS/NRDYSTS/BEMPSTS: write-0 clears status bits
            regs_[(off / 2u) & 0x7Fu] &= val;
            return true;
        }
        regs_[(off / 2u) & 0x7Fu] = val;
        return true;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        std::uint16_t w = 0;
        if (!Read16(phys & ~1u, w)) return false;
        out = ((phys & 1u) == 0u) ? static_cast<std::uint8_t>(w >> 8)
                                  : static_cast<std::uint8_t>(w & 0xFFu);
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        const std::uint32_t aligned = phys & ~1u;
        std::uint16_t w = 0;
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
};

} // namespace cg50
