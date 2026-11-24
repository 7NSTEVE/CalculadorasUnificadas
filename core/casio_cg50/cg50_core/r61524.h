#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace cg50 {

class R61524 final {
public:
    struct Pins {
        bool chipSelect = false;
        bool registerSelect = false;
        bool resetX = false;
        bool interfaceMode1 = false;
        bool frameMark = false;
        bool v28sys = false;
        bool backlightOn = true;
        bool backlightBrightness = false;
    };

    struct BusCycle {
        Pins pins{};
        std::uint16_t value = 0;
    };

    static constexpr std::size_t kVisibleWidth = 384;
    static constexpr std::size_t kVisibleHeight = 216;
    static constexpr std::size_t kRegisterSpace = 0x0700;
    static constexpr std::uint16_t kConfiguredId = 0x1234;
    static constexpr std::uint8_t kConfiguredVcm = 0x41;
    static constexpr std::uint8_t kConfiguredVdv = 0x10;

    void Reset();
    void SetPins(const Pins& pins) noexcept { pins_ = pins; }

    const Pins& GetPins() const noexcept { return pins_; }
    bool Powered() const noexcept { return pins_.v28sys; }
    bool ResetReleased() const noexcept { return pins_.resetX; }

    bool ObserveWrite16(std::uint16_t value);
    bool ReadBus16(std::uint16_t& value) const noexcept;

    void AdvanceCursorAfterData() const noexcept;

    const std::vector<BusCycle>& ObservedWrites() const noexcept { return writes_; }
    void ClearTrace() { writes_.clear(); }
    bool FramebufferAvailable() const noexcept { return hasValidFrame_ || !writes_.empty(); }
    const std::uint16_t* FramebufferData() const noexcept { return gram_.data(); }
    std::size_t FramebufferPixelCount() const noexcept { return gram_.size(); }
    std::uint16_t BacklightDuty() const noexcept { return regs_[0x05A1u]; }
    std::uint16_t VerticalScroll() const noexcept { return regs_[0x0404u]; }
    bool VerticalScrollEnabled() const noexcept { return (regs_[0x0401u] & 0x0001u) != 0u; }
    bool HorizontalMirrorEnabled() const noexcept { return (driverOutputControl_ & 0x0100u) != 0u; }
    bool DisplayBlanked() const noexcept
    {
        return (displayControl_ != 0u && (displayControl_ & 0x0003u) == 0u) ||
               ((regs_[0x0100u] & 0x0004u) != 0u); // STB standby bit
    }
    bool FrameMarkActive() const noexcept { return frameMarkPulse_; }
    void TickFrameMark(const std::uint64_t cycles) noexcept
    {
        if (cycles == 0u) return;
        // ~60 Hz TE/FMARK cadence at ~33 MHz (~550,000 cycles/frame, 10% pulse width)
        constexpr std::uint64_t kFrameCycles = 550000ull;
        vsyncCycleAccum_ += cycles;
        if (vsyncCycleAccum_ >= kFrameCycles) {
            vsyncCycleAccum_ %= kFrameCycles;
        }
        frameMarkPulse_ = (vsyncCycleAccum_ < (kFrameCycles / 10ull));
    }
    void RenderDisplayRgb565(std::uint16_t* outPixels, std::size_t count) const noexcept;
    std::uint16_t IndexRegister() const noexcept { return indexRegister_; }
    std::uint16_t EntryMode() const noexcept { return entryMode_; }
    std::uint16_t CurX() const noexcept { return curX_; }
    std::uint16_t CurY() const noexcept { return curY_; }
    std::uint16_t WinStartX() const noexcept { return winStartX_; }
    std::uint16_t WinEndX() const noexcept { return winEndX_; }
    std::uint16_t WinStartY() const noexcept { return winStartY_; }
    std::uint16_t WinEndY() const noexcept { return winEndY_; }
    std::uint16_t RegisterValue(std::uint16_t index) const noexcept
    {
        return index < kRegisterSpace ? regs_[index] : 0u;
    }

    // Save-state support: serializes GRAM + cursor/window/entry-mode state
    // and the driven pins. The write trace (ObservedWrites) is intentionally
    // NOT part of a snapshot; it is diagnostic evidence, not guest state.
    bool ExportState(std::vector<std::uint8_t>& out) const;
    bool ImportState(const std::uint8_t* data, std::size_t size);

private:
    Pins pins_{};
    std::vector<BusCycle> writes_;
    std::vector<std::uint16_t> gram_;
    std::array<std::uint16_t, kRegisterSpace> regs_{};
    std::uint16_t indexRegister_ = 0;
    std::uint16_t entryMode_ = 0x1030;
    std::uint16_t displayControl_ = 0;
    mutable std::uint16_t curX_ = 0;
    mutable std::uint16_t curY_ = 0;
    std::uint16_t winStartX_ = 0;
    std::uint16_t winEndX_ = static_cast<std::uint16_t>(kVisibleWidth - 1);
    std::uint16_t winStartY_ = 0;
    std::uint16_t winEndY_ = static_cast<std::uint16_t>(kVisibleHeight - 1);
    std::uint16_t driverOutputControl_ = 0;
    bool hasValidFrame_ = false;
    mutable bool gramReadDummyPending_ = false;
    std::uint64_t vsyncCycleAccum_ = 0;
    bool frameMarkPulse_ = false;
};

} // namespace cg50
