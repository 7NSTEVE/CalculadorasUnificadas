#include "cg50_keyboard_matrix.h"

namespace cg50 {

void KeyboardMatrix::Clear() noexcept
{
    for (auto& row : closed_) row.fill(false);
}

bool KeyboardMatrix::IsValidInputLine(const std::uint8_t inputLine) noexcept
{
    return inputLine >= 1 && inputLine <= 7;
}

int KeyboardMatrix::OutputIndex(const std::uint8_t outputLine) noexcept
{
    if (outputLine >= 1 && outputLine <= 10) return static_cast<int>(outputLine - 1);
    if (outputLine == 12) return 10;
    return -1;
}

bool KeyboardMatrix::IsValidOutputLine(const std::uint8_t outputLine) noexcept
{
    return OutputIndex(outputLine) >= 0;
}

bool KeyboardMatrix::SetPressed(const std::uint8_t inputLine,
                                const std::uint8_t outputLine,
                                const bool pressed) noexcept
{
    const int out = OutputIndex(outputLine);
    if (!IsValidInputLine(inputLine) || out < 0) return false;
    closed_[inputLine - 1][static_cast<std::size_t>(out)] = pressed;
    return true;
}

bool KeyboardMatrix::SetMatrixCode(const std::uint16_t code, const bool pressed) noexcept
{
    return SetPressed(static_cast<std::uint8_t>(code >> 8),
                      static_cast<std::uint8_t>(code), pressed);
}

bool KeyboardMatrix::IsPressed(const std::uint8_t inputLine,
                               const std::uint8_t outputLine) const noexcept
{
    const int out = OutputIndex(outputLine);
    if (!IsValidInputLine(inputLine) || out < 0) return false;
    return closed_[inputLine - 1][static_cast<std::size_t>(out)];
}

bool KeyboardMatrix::IsMatrixCodePressed(const std::uint16_t code) const noexcept
{
    return IsPressed(static_cast<std::uint8_t>(code >> 8),
                     static_cast<std::uint8_t>(code));
}

std::size_t KeyboardMatrix::PressedCount() const noexcept
{
    std::size_t count = 0;
    for (const auto& row : closed_)
        for (const bool pressed : row)
            if (pressed) ++count;
    return count;
}

} // namespace cg50
