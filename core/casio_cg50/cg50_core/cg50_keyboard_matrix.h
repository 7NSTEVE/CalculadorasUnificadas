#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cg50 {

// Electrical contact topology only. No scan polarity, pull-up or PFC register
// semantics are assumed here.
class KeyboardMatrix final {
public:
    static constexpr std::size_t kInputCount = 7;
    static constexpr std::size_t kOutputCount = 11; // KO1..KO10 and KO12

    void Clear() noexcept;
    static bool IsValidInputLine(std::uint8_t inputLine) noexcept;
    static bool IsValidOutputLine(std::uint8_t outputLine) noexcept;
    bool SetPressed(std::uint8_t inputLine, std::uint8_t outputLine, bool pressed) noexcept;
    bool SetMatrixCode(std::uint16_t code, bool pressed) noexcept;
    bool IsPressed(std::uint8_t inputLine, std::uint8_t outputLine) const noexcept;
    bool IsMatrixCodePressed(std::uint16_t code) const noexcept;
    std::size_t PressedCount() const noexcept;

private:
    static int OutputIndex(std::uint8_t outputLine) noexcept;
    std::array<std::array<bool, kOutputCount>, kInputCount> closed_{};
};

} // namespace cg50
