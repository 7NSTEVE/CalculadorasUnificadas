#pragma once

#include "cg50_board_wiring.h"
#include "cg50_keyboard_matrix.h"
#include "r61524.h"

#include <cstdint>

namespace cg50 {

class PeripheralBus final {
public:
    enum class Result { Accepted, DeviceRejected, Unmapped };

    void Reset();
    bool SetPfcPin(PfcPin pin, bool level) noexcept;
    void SetV28Sys(bool enabled) noexcept;
    bool SetKeyMatrixCode(std::uint16_t code, bool pressed) noexcept;

    const PfcRawState& Pfc() const noexcept { return pfc_; }
    const KeyboardMatrix& Keyboard() const noexcept { return keyboard_; }
    const R61524& Lcd() const noexcept { return lcd_; }
    bool V28Sys() const noexcept { return v28sys_; }

    Result Write16(std::uint32_t address, std::uint16_t value);
    Result Read16(std::uint32_t address, std::uint16_t& value) const noexcept;

    // Save-state support. The keyboard matrix is NOT serialized here: the
    // owning core replays its serialized keyState_ through
    // SetKeyMatrixCode after restoring, which reconstructs the exact same
    // matrix state.
    bool ExportState(std::vector<std::uint8_t>& out) const;
    bool ImportState(const std::uint8_t* data, std::size_t size);

private:
    void RefreshBoardWiring() noexcept;
    PfcRawState pfc_{};
    KeyboardMatrix keyboard_{};
    R61524 lcd_{};
    bool v28sys_ = false;
};

} // namespace cg50
