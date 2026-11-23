#pragma once

#include "sh_exception.h"

#include <cstdint>

namespace cg50 {

struct InterruptRequest {
    std::uint32_t pc = 0;
    std::uint32_t sr = 0;
    std::uint32_t r15 = 0;
    std::uint32_t vbr = 0;
    std::uint32_t eventCode = 0;
    std::uint8_t priority = 0;
    bool cpuSleeping = false;
};

struct InterruptEntry {
    bool accepted = false;
    bool wakesCpu = false;
    std::uint32_t ssr = 0;
    std::uint32_t spc = 0;
    std::uint32_t sgr = 0;
    std::uint32_t sr = 0;
    std::uint32_t targetPc = 0;
    std::uint32_t intevt = 0;
};

std::uint8_t InterruptMaskLevel(std::uint32_t sr) noexcept;
bool MaskableInterruptAccepted(std::uint32_t sr, std::uint8_t priority, bool cpuSleeping = false) noexcept;
InterruptEntry BuildMaskableInterruptEntry(const InterruptRequest& request) noexcept;

} // namespace cg50
