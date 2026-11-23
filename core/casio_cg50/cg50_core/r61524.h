#pragma once

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

    void AdvanceCursorAfterData() noexcept;

    const std::vector<BusCycle>& ObservedWrites() const noexcept { return writes_; }
    void ClearTrace() { writes_.clear(); }
    bool FramebufferAvailable() const noexcept { return hasValidFrame_ || !writes_.empty(); }
    const std::uint16_t* FramebufferData() const noexcept { return gram_.data(); }
    std::size_t FramebufferPixelCount() const noexcept { return gram_.size(); }

    // Save-state support: serializes GRAM + cursor/window/entry-mode state
    // and the driven pins. The write trace (ObservedWrites) is intentionally
    // NOT part of a snapshot; it is diagnostic evidence, not guest state.
    bool ExportState(std::vector<std::uint8_t>& out) const;
    bool ImportState(const std::uint8_t* data, std::size_t size);

private:
    Pins pins_{};
    std::vector<BusCycle> writes_;
    std::vector<std::uint16_t> gram_;
    std::uint16_t indexRegister_ = 0;
    std::uint16_t entryMode_ = 0x1030;
    std::uint16_t displayControl_ = 0;
    std::uint16_t curX_ = 0;
    std::uint16_t curY_ = 0;
    std::uint16_t winStartX_ = 0;
    std::uint16_t winEndX_ = static_cast<std::uint16_t>(kVisibleWidth - 1);
    std::uint16_t winStartY_ = 0;
    std::uint16_t winEndY_ = static_cast<std::uint16_t>(kVisibleHeight - 1);
    std::uint16_t driverOutputControl_ = 0;
    bool hasValidFrame_ = false;
};

} // namespace cg50
