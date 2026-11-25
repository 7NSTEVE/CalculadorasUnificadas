#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>

namespace cg50 {

// ============================================================================
// SH7305 USB 2.0 Function Controller (0xA4D80000 / phys 0x04D80000..0x04D800FF)
// Exact 57 registers confirmed in Casio CPU73050.dll:
//   0x00 SYSCFG    0x02 BUSWAIT   0x04 SYSSTS    0x08 DVSTCTR   0x0C TESTMODE
//   0x14 CFIFO     0x18 D0FIFO    0x1C D1FIFO
//   0x20 CFIFOSEL  0x22 CFIFOCTR  0x28 D0FIFOSEL 0x2A D0FIFOCTR 0x2C D1FIFOSEL 0x2E D1FIFOCTR
//   0x30 INTENB0   0x32 INTENB1   0x36 BRDYENB   0x38 NRDYENB   0x3A BEMPENB
//   0x40 INTSTS0   0x42 INTSTS1   0x46 BRDYSTS   0x48 NRDYSTS   0x4A BEMPSTS
//   0x4C FRMNUM    0x4E UFRMNUM   0x50 USBADDR   0x54 USBREQ    0x56 USBVAL
//   0x58 USBINDX   0x5A USBLENG   0x5C DCPCFG    0x5E DCPMAXP   0x60 DCPCTR
//   0x64 PIPESEL   0x68 PIPECFG   0x6A PIPEBUF   0x6C PIPEMAXP  0x6E PIPEPERI
//   0x70..0x80 PIPE1CTR..PIPE9CTR
//
// Default reset state is cleanly cable-detached (SYSSTS=0x0000, CFIFOCTR.FRDY=1)
// so Casio OS boot/Link polling never stalls. Virtual host API allows full
// VBUS attach, USB enumeration (SETUP packets), Bulk IN/OUT pipe FIFO transfers
// (Mass Storage BOT/SCSI & Protocol 7.00), and real kEventUsb IRQ generation.
// ============================================================================
class Usb2Unit final {
public:
    static constexpr std::size_t kNumPipes = 10u; // Pipe 0 = DCP, Pipes 1..9 = Bulk/Int

    // INTSTS0 / INTENB0 bits
    static constexpr std::uint16_t kIntVbint = 0x8000u; // VBUS change
    static constexpr std::uint16_t kIntResm  = 0x4000u; // Resume
    static constexpr std::uint16_t kIntSof   = 0x2000u; // Frame refresh
    static constexpr std::uint16_t kIntDvst  = 0x1000u; // Device state transition
    static constexpr std::uint16_t kIntCtrt  = 0x0800u; // Control transfer stage
    static constexpr std::uint16_t kIntBemp  = 0x0400u; // Buffer empty
    static constexpr std::uint16_t kIntNrdy  = 0x0200u; // Buffer not ready
    static constexpr std::uint16_t kIntBrdy  = 0x0100u; // Buffer ready
    static constexpr std::uint16_t kIntValid = 0x0008u; // SETUP packet received

    void Reset() noexcept
    {
        regs_.fill(0u);
        regs_[0x22u / 2u] = 0x2000u; // CFIFOCTR: FRDY=1
        regs_[0x2Au / 2u] = 0x2000u; // D0FIFOCTR: FRDY=1
        regs_[0x2Eu / 2u] = 0x2000u; // D1FIFOCTR: FRDY=1
        regs_[0x5Eu / 2u] = 0x0040u; // DCPMAXP: 64 bytes
        vbusAttached_ = false;
        for (auto& q : pipeRx_) q.clear();
        for (auto& q : pipeTx_) q.clear();
        for (auto& cfg : pipeCfg_) cfg = 0u;
        for (auto& buf : pipeBuf_) buf = 0u;
        for (auto& mp : pipeMaxp_) mp = 64u;
        for (auto& per : pipePeri_) per = 0u;
    }

    static bool MatchesPhysical(const std::uint32_t phys) noexcept
    {
        return phys >= 0x04D80000u && phys < 0x04D80100u;
    }

    // ---- Virtual USB Host API (for tests / Mass Storage / Protocol 7.00) ----
    void SetVbusAttached(const bool attached) noexcept
    {
        if (vbusAttached_ == attached) return;
        vbusAttached_ = attached;
        if (attached) {
            // SYSSTS: LNST=01 (J-state Full-Speed), DVSTCTR: RHST=010 (FS)
            regs_[0x04u / 2u] = 0x0001u;
            regs_[0x08u / 2u] = static_cast<std::uint16_t>((regs_[0x08u / 2u] & ~0x0007u) | 0x0002u);
            regs_[0x40u / 2u] |= (kIntVbint | kIntDvst | 0x0080u); // VBSTS=1 + VBINT + DVST
        } else {
            regs_[0x04u / 2u] = 0x0000u;
            regs_[0x08u / 2u] &= static_cast<std::uint16_t>(~0x0007u);
            regs_[0x40u / 2u] = static_cast<std::uint16_t>((regs_[0x40u / 2u] & ~0x0080u) | kIntVbint | kIntDvst);
        }
    }

    bool VbusAttached() const noexcept { return vbusAttached_; }

    // Inject an 8-byte USB SETUP packet from virtual host -> sets VALID + CTRT
    void InjectSetupPacket(const std::uint16_t usbReq,
                           const std::uint16_t usbVal,
                           const std::uint16_t usbIndx,
                           const std::uint16_t usbLeng) noexcept
    {
        regs_[0x54u / 2u] = usbReq;
        regs_[0x56u / 2u] = usbVal;
        regs_[0x58u / 2u] = usbIndx;
        regs_[0x5Au / 2u] = usbLeng;
        // CTSQ = 001 (Control read data) or 010 (Control write data)
        const std::uint16_t ctsq = ((usbReq & 0x0080u) != 0u) ? 0x0001u : 0x0002u;
        regs_[0x40u / 2u] = static_cast<std::uint16_t>((regs_[0x40u / 2u] & ~0x0007u) | ctsq | kIntCtrt | kIntValid);
    }

    // Virtual host sends OUT packet to device pipe (0..9)
    void HostPushOutPipe(const std::size_t pipe, const std::uint8_t* data, const std::size_t len)
    {
        if (pipe >= kNumPipes || data == nullptr || len == 0u) return;
        for (std::size_t i = 0; i < len; ++i) pipeRx_[pipe].push_back(data[i]);
        regs_[0x46u / 2u] |= static_cast<std::uint16_t>(1u << pipe); // BRDYSTS
        regs_[0x40u / 2u] |= kIntBrdy;
    }

    // Virtual host reads IN packet transmitted by device on pipe (0..9)
    std::vector<std::uint8_t> HostPopInPipe(const std::size_t pipe)
    {
        std::vector<std::uint8_t> out;
        if (pipe >= kNumPipes) return out;
        out.assign(pipeTx_[pipe].begin(), pipeTx_[pipe].end());
        pipeTx_[pipe].clear();
        regs_[0x4Au / 2u] |= static_cast<std::uint16_t>(1u << pipe); // BEMPSTS
        regs_[0x40u / 2u] |= kIntBemp;
        return out;
    }

    std::size_t PipeTxPending(const std::size_t pipe) const noexcept
    {
        return (pipe < kNumPipes) ? pipeTx_[pipe].size() : 0u;
    }

    std::size_t PipeRxPending(const std::size_t pipe) const noexcept
    {
        return (pipe < kNumPipes) ? pipeRx_[pipe].size() : 0u;
    }

    std::uint8_t DeviceAddress() const noexcept
    {
        return static_cast<std::uint8_t>(regs_[0x50u / 2u] & 0x7Fu);
    }

    std::uint16_t PipeControl(const std::size_t pipe) const noexcept
    {
        if (pipe == 0u) return regs_[0x60u / 2u];
        if (pipe < kNumPipes) return regs_[(0x70u + (pipe - 1u) * 2u) / 2u];
        return 0u;
    }

    std::uint16_t PipeCfg(const std::size_t pipe) const noexcept
    {
        return (pipe < kNumPipes) ? pipeCfg_[pipe] : 0u;
    }

    std::uint16_t PipeMaxPacket(const std::size_t pipe) const noexcept
    {
        if (pipe == 0u) return static_cast<std::uint16_t>(regs_[0x5Eu / 2u] & 0x007Fu);
        return (pipe < kNumPipes) ? static_cast<std::uint16_t>(pipeMaxp_[pipe] & 0x07FFu) : 64u;
    }

    bool IsControlStatusComplete() const noexcept
    {
        return (regs_[0x60u / 2u] & 0x0004u) != 0u; // DCPCTR.CCPL
    }

    bool Read32(const std::uint32_t phys, std::uint32_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x14u || off == 0x18u || off == 0x1Cu) {
            std::uint16_t hi = 0, lo = 0;
            (void)Read16(phys, hi);
            (void)Read16(phys, lo);
            out = (static_cast<std::uint32_t>(hi) << 16) | lo;
            return true;
        }
        std::uint16_t hi = 0, lo = 0;
        if (!Read16(phys, hi) || !Read16(phys + 2u, lo)) return false;
        out = (static_cast<std::uint32_t>(hi) << 16) | lo;
        return true;
    }

    bool Write32(const std::uint32_t phys, const std::uint32_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x14u || off == 0x18u || off == 0x1Cu) {
            const std::size_t selOff = (off == 0x14u) ? 0x20u : ((off == 0x18u) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            if (pipe < kNumPipes) {
                pipeTx_[pipe].push_back(static_cast<std::uint8_t>((val >> 24) & 0xFFu));
                pipeTx_[pipe].push_back(static_cast<std::uint8_t>((val >> 16) & 0xFFu));
                pipeTx_[pipe].push_back(static_cast<std::uint8_t>((val >> 8) & 0xFFu));
                pipeTx_[pipe].push_back(static_cast<std::uint8_t>(val & 0xFFu));
            }
            return true;
        }
        return Write16(phys, static_cast<std::uint16_t>(val >> 16)) &&
               Write16(phys + 2u, static_cast<std::uint16_t>(val & 0xFFFFu));
    }

    bool HasPendingIrq() const noexcept
    {
        const std::uint16_t en0 = regs_[0x30u / 2u];
        const std::uint16_t st0 = regs_[0x40u / 2u];
        if ((en0 & st0 & 0xFF00u) != 0u) return true;
        if (((en0 & kIntBrdy) != 0u) && ((regs_[0x36u / 2u] & regs_[0x46u / 2u]) != 0u)) return true;
        if (((en0 & kIntNrdy) != 0u) && ((regs_[0x38u / 2u] & regs_[0x48u / 2u]) != 0u)) return true;
        if (((en0 & kIntBemp) != 0u) && ((regs_[0x3Au / 2u] & regs_[0x4Au / 2u]) != 0u)) return true;
        return false;
    }

    bool Read16(const std::uint32_t phys, std::uint16_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x04u) {
            out = vbusAttached_ ? regs_[0x04u / 2u] : 0x0000u;
            return true;
        }
        if (off == 0x14u || off == 0x18u || off == 0x1Cu) {
            const std::size_t selOff = (off == 0x14u) ? 0x20u : ((off == 0x18u) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            out = PopFifoWord(pipe);
            return true;
        }
        if (off == 0x22u || off == 0x2Au || off == 0x2Eu) {
            const std::size_t selOff = (off == 0x22u) ? 0x20u : ((off == 0x2Au) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            const std::uint16_t dtln = (pipe < kNumPipes)
                ? static_cast<std::uint16_t>(pipeRx_[pipe].size() & 0x0FFFu)
                : 0u;
            out = static_cast<std::uint16_t>(0x2000u | dtln); // FRDY=1 | DTLN
            return true;
        }
        if (off == 0x68u) { out = pipeCfg_[SelectedPipe()]; return true; }
        if (off == 0x6Au) { out = pipeBuf_[SelectedPipe()]; return true; }
        if (off == 0x6Cu) { out = pipeMaxp_[SelectedPipe()]; return true; }
        if (off == 0x6Eu) { out = pipePeri_[SelectedPipe()]; return true; }
        out = regs_[(off / 2u) & 0x7Fu];
        return true;
    }

    bool Write16(const std::uint32_t phys, const std::uint16_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x14u || off == 0x18u || off == 0x1Cu) {
            const std::size_t selOff = (off == 0x14u) ? 0x20u : ((off == 0x18u) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            const bool mbw16 = (regs_[selOff / 2u] & 0x0400u) != 0u;
            PushFifoWord(pipe, val, mbw16);
            return true;
        }
        if (off == 0x22u || off == 0x2Au || off == 0x2Eu) {
            const std::size_t selOff = (off == 0x22u) ? 0x20u : ((off == 0x2Au) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            if ((val & 0x4000u) != 0u && pipe < kNumPipes) { // BCLR
                pipeRx_[pipe].clear();
            }
            if ((val & 0x8000u) != 0u && pipe < kNumPipes) { // BVAL (commit IN packet)
                regs_[0x4Au / 2u] |= static_cast<std::uint16_t>(1u << pipe);
            }
            regs_[(off / 2u) & 0x7Fu] = static_cast<std::uint16_t>((val & 0x1FFFu) | 0x2000u);
            return true;
        }
        if (off == 0x40u || off == 0x42u || off == 0x46u || off == 0x48u || off == 0x4Au) {
            // INTSTS0/1, BRDYSTS, NRDYSTS, BEMPSTS: write-0 clears status bits
            regs_[(off / 2u) & 0x7Fu] &= val;
            return true;
        }
        if (off == 0x68u) { pipeCfg_[SelectedPipe()] = val; return true; }
        if (off == 0x6Au) { pipeBuf_[SelectedPipe()] = val; return true; }
        if (off == 0x6Cu) { pipeMaxp_[SelectedPipe()] = val; return true; }
        if (off == 0x6Eu) { pipePeri_[SelectedPipe()] = val; return true; }
        regs_[(off / 2u) & 0x7Fu] = val;
        return true;
    }

    bool Read8(const std::uint32_t phys, std::uint8_t& out) const noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x14u || off == 0x15u || off == 0x18u || off == 0x19u || off == 0x1Cu || off == 0x1Du) {
            const std::size_t selOff = (off < 0x18u) ? 0x20u : ((off < 0x1Cu) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            if (pipe < kNumPipes && !pipeRx_[pipe].empty()) {
                out = pipeRx_[pipe].front();
                pipeRx_[pipe].pop_front();
                return true;
            }
            out = 0u;
            return true;
        }
        std::uint16_t w = 0;
        if (!Read16(phys & ~1u, w)) return false;
        out = ((phys & 1u) == 0u) ? static_cast<std::uint8_t>(w >> 8)
                                  : static_cast<std::uint8_t>(w & 0xFFu);
        return true;
    }

    bool Write8(const std::uint32_t phys, const std::uint8_t val) noexcept
    {
        if (!MatchesPhysical(phys)) return false;
        const std::uint32_t off = phys - 0x04D80000u;
        if (off == 0x14u || off == 0x15u || off == 0x18u || off == 0x19u || off == 0x1Cu || off == 0x1Du) {
            const std::size_t selOff = (off < 0x18u) ? 0x20u : ((off < 0x1Cu) ? 0x28u : 0x2Cu);
            const std::size_t pipe = regs_[selOff / 2u] & 0x0Fu;
            if (pipe < kNumPipes) pipeTx_[pipe].push_back(val);
            return true;
        }
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

    // POD register snapshot for save-state compatibility (256 bytes = 128 x uint16_t)
    const std::array<std::uint16_t, 128>& RawRegs() const noexcept { return regs_; }
    std::array<std::uint16_t, 128>& RawRegs() noexcept { return regs_; }

    // Extended POD snapshot for pipe-banked registers (PIPECFG/PIPEBUF/PIPEMAXP/PIPEPERI) + VBUS
    struct PipeBanksSnapshot {
        std::array<std::uint16_t, kNumPipes> pipeCfg{};
        std::array<std::uint16_t, kNumPipes> pipeBuf{};
        std::array<std::uint16_t, kNumPipes> pipeMaxp{};
        std::array<std::uint16_t, kNumPipes> pipePeri{};
        std::uint8_t vbusAttached = 0;
        std::uint8_t reserved[3]{};
    };
    PipeBanksSnapshot ExportPipeBanks() const noexcept
    {
        PipeBanksSnapshot s{};
        s.pipeCfg = pipeCfg_;
        s.pipeBuf = pipeBuf_;
        s.pipeMaxp = pipeMaxp_;
        s.pipePeri = pipePeri_;
        s.vbusAttached = vbusAttached_ ? 1u : 0u;
        return s;
    }
    void ImportPipeBanks(const PipeBanksSnapshot& s) noexcept
    {
        pipeCfg_ = s.pipeCfg;
        pipeBuf_ = s.pipeBuf;
        pipeMaxp_ = s.pipeMaxp;
        pipePeri_ = s.pipePeri;
        vbusAttached_ = (s.vbusAttached != 0u);
    }

private:
    std::size_t SelectedPipe() const noexcept
    {
        const std::size_t p = regs_[0x64u / 2u] & 0x0Fu;
        return (p < kNumPipes) ? p : 0u;
    }

    std::uint16_t PopFifoWord(const std::size_t pipe) const noexcept
    {
        if (pipe >= kNumPipes || pipeRx_[pipe].empty()) return 0u;
        const std::uint8_t b0 = pipeRx_[pipe].front();
        pipeRx_[pipe].pop_front();
        std::uint8_t b1 = 0u;
        if (!pipeRx_[pipe].empty()) {
            b1 = pipeRx_[pipe].front();
            pipeRx_[pipe].pop_front();
        }
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(b0) << 8) | b1);
    }

    void PushFifoWord(const std::size_t pipe, const std::uint16_t val, const bool mbw16) noexcept
    {
        if (pipe >= kNumPipes) return;
        if (mbw16) {
            pipeTx_[pipe].push_back(static_cast<std::uint8_t>(val >> 8));
            pipeTx_[pipe].push_back(static_cast<std::uint8_t>(val & 0xFFu));
        } else {
            pipeTx_[pipe].push_back(static_cast<std::uint8_t>(val & 0xFFu));
        }
    }

    std::array<std::uint16_t, 128> regs_{};
    bool vbusAttached_ = false;
    std::array<std::uint16_t, kNumPipes> pipeCfg_{};
    std::array<std::uint16_t, kNumPipes> pipeBuf_{};
    std::array<std::uint16_t, kNumPipes> pipeMaxp_{};
    std::array<std::uint16_t, kNumPipes> pipePeri_{};
    mutable std::array<std::deque<std::uint8_t>, kNumPipes> pipeRx_{};
    std::array<std::deque<std::uint8_t>, kNumPipes> pipeTx_{};
};

} // namespace cg50
