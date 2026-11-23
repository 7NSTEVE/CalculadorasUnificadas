#include "sh_interrupt.h"

namespace cg50 {

std::uint8_t InterruptMaskLevel(const std::uint32_t sr) noexcept
{
    return static_cast<std::uint8_t>((sr >> 4) & 0x0Fu);
}

bool MaskableInterruptAccepted(const std::uint32_t sr, const std::uint8_t priority, const bool cpuSleeping) noexcept
{
    if (priority == 0 || priority > 15) return false;
    // Renesas SH-4A Architecture: In sleep or standby state, an interrupt is accepted
    // even if the BL bit in SR is set to 1.
    if (!cpuSleeping && (sr & kSrBlockBit) != 0) return false;
    return priority > InterruptMaskLevel(sr);
}

InterruptEntry BuildMaskableInterruptEntry(const InterruptRequest& request) noexcept
{
    InterruptEntry entry{};
    if (!MaskableInterruptAccepted(request.sr, request.priority, request.cpuSleeping)) return entry;
    entry.accepted = true;
    entry.wakesCpu = request.cpuSleeping;
    entry.ssr = request.sr;
    entry.spc = request.pc;
    entry.sgr = request.r15;
    entry.sr = request.sr | kSrExceptionEntryBits;
    entry.targetPc = request.vbr + kInterruptVectorOffset;
    entry.intevt = request.eventCode;
    return entry;
}

} // namespace cg50
