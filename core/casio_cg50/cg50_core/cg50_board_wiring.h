#pragma once

#include "r61524.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cg50 {

inline constexpr std::uint32_t kLcdBusAddress = 0x14000000u;

// Keep existing enum values stable; newly required descriptor ports are appended
// before Count so A/H/M/N/P/R/J indexing remains unchanged.
enum class PfcPort : std::uint8_t { A, H, M, N, P, R, J, U, Count };

struct PfcPin {
    PfcPort port{};
    std::uint8_t bit = 0;
    constexpr bool operator==(const PfcPin& other) const noexcept {
        return port == other.port && bit == other.bit;
    }
    constexpr bool operator!=(const PfcPin& other) const noexcept { return !(*this == other); }
};

struct DiagnosticWindow {
    std::uint32_t base = 0;
    std::uint32_t size = 0;

    constexpr bool Contains(const std::uint32_t address) const noexcept {
        return address >= base &&
               static_cast<std::uint64_t>(address) <
                   static_cast<std::uint64_t>(base) + size;
    }
};

// Exact LogErrorAddr windows declared by cg50_core/EmulatorData/CGM.dlm.
// Their addresses/ranges are evidence; their device semantics are NOT known yet.
inline constexpr std::array<DiagnosticWindow, 4> kDescriptorLogErrorWindows{{
    {0xA4050100u, 0x00000100u},
    {0xFE000000u, 0x00400000u},
    {0xA4080000u, 0x00000100u},
    {0xFFFFFFE0u, 0x00000020u},
}};

constexpr int DescriptorLogErrorWindowIndex(const std::uint32_t address) noexcept
{
    for (std::size_t i = 0; i < kDescriptorLogErrorWindows.size(); ++i) {
        if (kDescriptorLogErrorWindows[i].Contains(address))
            return static_cast<int>(i);
    }
    return -1;
}

// Wiring transcribed from cg50_core/EmulatorData/CGM.dlm.
inline constexpr PfcPin kLcdChipSelectPin      {PfcPort::H, 2}; // PTH2
inline constexpr PfcPin kLcdRegisterSelectPin  {PfcPort::R, 4}; // PTR4
inline constexpr PfcPin kLcdInterfaceMode1Pin  {PfcPort::R, 6}; // PTR6
inline constexpr PfcPin kLcdFrameMarkPin       {PfcPort::R, 5}; // PTR5
inline constexpr PfcPin kLcdResetXPin          {PfcPort::A, 4}; // PTA4
inline constexpr PfcPin kLcdBacklightBrightPin {PfcPort::N, 4}; // PTN4

inline constexpr std::array<PfcPin, 7> kKeyboardKiPins{{
    {PfcPort::P, 0}, {PfcPort::P, 1}, {PfcPort::P, 2}, {PfcPort::P, 3},
    {PfcPort::P, 4}, {PfcPort::P, 5}, {PfcPort::P, 6}
}};

inline constexpr std::array<std::uint8_t, 11> kKeyboardKoNumbers{{
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12
}};

inline constexpr std::array<PfcPin, 11> kKeyboardKoPins{{
    {PfcPort::M, 0}, {PfcPort::M, 1}, {PfcPort::M, 2}, {PfcPort::M, 3},
    {PfcPort::M, 4}, {PfcPort::M, 5},
    {PfcPort::N, 0}, {PfcPort::N, 1}, {PfcPort::N, 2}, {PfcPort::N, 3},
    {PfcPort::N, 5}
}};

// Additional descriptor endpoints. These are raw wiring facts only: no active
// polarity, reset level, PFC register address or timing is inferred here.
inline constexpr PfcPin kDescriptorPowerOnPin         {PfcPort::J, 6}; // PTJ6
inline constexpr PfcPin kDescriptorDdPowerPin         {PfcPort::H, 5}; // PTH5
inline constexpr PfcPin kDescriptorRamChipSelectPin   {PfcPort::R, 2}; // PTR2
inline constexpr PfcPin kDescriptorAppIfChipSelectPin {PfcPort::R, 3}; // PTR3

// RS232 wiring transcribed literally from CGM.dlm. Pin identity is evidence;
// SCI/SCIF block identity, register map, polarity, baud/timing and IRQ behavior
// remain intentionally unknown until exact SH7305 evidence or a real boot demands it.
inline constexpr PfcPin kDescriptorRs232TxPin       {PfcPort::J, 2}; // PTJ2
inline constexpr PfcPin kDescriptorRs232RxPin       {PfcPort::J, 3}; // PTJ3
inline constexpr PfcPin kDescriptorRs232TxEnablePin {PfcPort::U, 2}; // PTU2
inline constexpr PfcPin kDescriptorRs232RxEnablePin {PfcPort::U, 3}; // PTU3

inline constexpr std::array<std::string_view, 4> kDescriptorRs232Assignments{{
    "_PTJ3=_SerialInter:RS232.RXD",
    "_TXD=_Processor:PFC.PTJ2",
    "_TXD_ENE=_Processor:PFC.PTU2",
    "_RXD_ENE=_Processor:PFC.PTU3",
}};

enum class DescriptorBoardSignal : std::uint8_t {
    ResetP,
    PowerOn,
    DdPower,
    RamChipSelect,
    RomUserRomChipSelect,
    AppIfChipSelect,
};

struct DescriptorBoardConnection {
    DescriptorBoardSignal signal{};
    std::string_view endpoint{};
};

// Preserve the exact CGM.dlm endpoint spelling as a machine-checkable evidence
// ledger. BSC.CS0 and Power.BLD3 intentionally remain raw endpoint strings rather
// than being reinterpreted as PFC signals.
inline constexpr std::array<DescriptorBoardConnection, 6> kDescriptorBoardConnections{{
    {DescriptorBoardSignal::ResetP, "_PowerInter:Power.BLD3"},
    {DescriptorBoardSignal::PowerOn, "_Processor:PFC.PTJ6"},
    {DescriptorBoardSignal::DdPower, "_Processor:PFC.PTH5"},
    {DescriptorBoardSignal::RamChipSelect, "_Processor:PFC.PTR2"},
    {DescriptorBoardSignal::RomUserRomChipSelect, "_Processor:BSC.CS0"},
    {DescriptorBoardSignal::AppIfChipSelect, "_Processor:PFC.PTR3"},
}};

constexpr std::string_view DescriptorEndpoint(const DescriptorBoardSignal signal) noexcept
{
    for (const auto& connection : kDescriptorBoardConnections) {
        if (connection.signal == signal) return connection.endpoint;
    }
    return {};
}

class PfcRawState final {
public:
    void Reset() noexcept { for (auto& port : levels_) port.fill(false); }
    bool Set(const PfcPin pin, const bool level) noexcept {
        const auto port = static_cast<std::size_t>(pin.port);
        if (port >= levels_.size() || pin.bit >= 8) return false;
        levels_[port][pin.bit] = level;
        return true;
    }
    bool Get(const PfcPin pin) const noexcept {
        const auto port = static_cast<std::size_t>(pin.port);
        if (port >= levels_.size() || pin.bit >= 8) return false;
        return levels_[port][pin.bit];
    }
    // Save-state support: raw 9x8 pin-level bitmap.
    void ExportState(std::vector<std::uint8_t>& out) const {
        for (const auto& port : levels_)
            for (const bool level : port) out.push_back(level ? 1u : 0u);
    }
    bool ImportState(const std::uint8_t* data, std::size_t size) {
        constexpr std::size_t kBytes =
            static_cast<std::size_t>(PfcPort::Count) * 8u;
        if (data == nullptr || size < kBytes) return false;
        std::size_t cursor = 0;
        for (auto& port : levels_) {
            for (auto& level : port) level = data[cursor++] != 0u;
        }
        return true;
    }
private:
    std::array<std::array<bool, 8>, static_cast<std::size_t>(PfcPort::Count)> levels_{};
};

inline R61524::Pins BuildLcdPinsFromBoard(const PfcRawState& pfc, const bool v28sys) noexcept
{
    R61524::Pins pins{};
    pins.chipSelect = pfc.Get(kLcdChipSelectPin);
    pins.registerSelect = pfc.Get(kLcdRegisterSelectPin);
    pins.interfaceMode1 = pfc.Get(kLcdInterfaceMode1Pin);
    pins.frameMark = pfc.Get(kLcdFrameMarkPin);
    pins.resetX = pfc.Get(kLcdResetXPin);
    pins.v28sys = v28sys;
    pins.backlightOn = true; // CGM.dlm: _BLON=1!1
    pins.backlightBrightness = pfc.Get(kLcdBacklightBrightPin);
    return pins;
}

} // namespace cg50
