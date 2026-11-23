#include "cg50_peripheral_bus.h"

namespace cg50 {

void PeripheralBus::Reset()
{
    pfc_.Reset();
    keyboard_.Clear();
    v28sys_ = true;
    lcd_.Reset();
    RefreshBoardWiring();
}

bool PeripheralBus::SetPfcPin(const PfcPin pin, const bool level) noexcept
{
    if (!pfc_.Set(pin, level)) return false;
    RefreshBoardWiring();
    return true;
}

void PeripheralBus::SetV28Sys(const bool enabled) noexcept
{
    v28sys_ = enabled;
    RefreshBoardWiring();
}

bool PeripheralBus::SetKeyMatrixCode(const std::uint16_t code, const bool pressed) noexcept
{
    return keyboard_.SetMatrixCode(code, pressed);
}

void PeripheralBus::RefreshBoardWiring() noexcept
{
    lcd_.SetPins(BuildLcdPinsFromBoard(pfc_, v28sys_));
}

PeripheralBus::Result PeripheralBus::Write16(const std::uint32_t address,
                                             const std::uint16_t value)
{
    if (address < kLcdBusAddress || address >= kLcdBusAddress + 0x01000000u)
        return Result::Unmapped;
    return lcd_.ObserveWrite16(value) ? Result::Accepted : Result::DeviceRejected;
}

PeripheralBus::Result PeripheralBus::Read16(const std::uint32_t address,
                                            std::uint16_t& value) const noexcept
{
    if (address < kLcdBusAddress || address >= kLcdBusAddress + 0x01000000u)
        return Result::Unmapped;
    return lcd_.ReadBus16(value) ? Result::Accepted : Result::DeviceRejected;
}

bool PeripheralBus::ExportState(std::vector<std::uint8_t>& out) const
{
    out.clear();
    out.push_back(v28sys_ ? 1u : 0u);
    pfc_.ExportState(out);
    std::vector<std::uint8_t> lcd;
    if (!lcd_.ExportState(lcd)) return false;
    const std::uint32_t lcdBytes = static_cast<std::uint32_t>(lcd.size());
    out.push_back(static_cast<std::uint8_t>(lcdBytes >> 24));
    out.push_back(static_cast<std::uint8_t>(lcdBytes >> 16));
    out.push_back(static_cast<std::uint8_t>(lcdBytes >> 8));
    out.push_back(static_cast<std::uint8_t>(lcdBytes));
    out.insert(out.end(), lcd.begin(), lcd.end());
    return true;
}

bool PeripheralBus::ImportState(const std::uint8_t* data, const std::size_t size)
{
    constexpr std::size_t kPfcBytes =
        static_cast<std::size_t>(PfcPort::Count) * 8u;
    if (data == nullptr || size < 1u + kPfcBytes + 4u) return false;
    std::size_t cursor = 0;
    v28sys_ = data[cursor++] != 0u;
    if (!pfc_.ImportState(data + cursor, size - cursor)) return false;
    cursor += kPfcBytes;
    std::uint32_t lcdBytes = 0;
    lcdBytes |= static_cast<std::uint32_t>(data[cursor++]) << 24;
    lcdBytes |= static_cast<std::uint32_t>(data[cursor++]) << 16;
    lcdBytes |= static_cast<std::uint32_t>(data[cursor++]) << 8;
    lcdBytes |= static_cast<std::uint32_t>(data[cursor++]);
    if (lcdBytes == 0 || lcdBytes > size - cursor) return false;
    if (!lcd_.ImportState(data + cursor, lcdBytes)) return false;
    keyboard_.Clear();
    RefreshBoardWiring();
    return true;
}

} // namespace cg50
