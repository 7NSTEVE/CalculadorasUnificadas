#include "sh_exception.h"

namespace cg50 {

ExceptionEntry BuildGeneralExceptionEntry(const ExceptionRequest& request) noexcept
{
    ExceptionEntry entry{};
    if ((request.sr & kSrBlockBit) != 0) {
        entry.outcome = ExceptionEntry::Outcome::Reset;
        entry.targetPc = kResetVectorAddress;
        entry.sr = request.sr;
        return entry;
    }

    entry.outcome = ExceptionEntry::Outcome::Vector;
    entry.ssr = request.sr;
    entry.sgr = request.r15;
    entry.sr = request.sr | kSrExceptionEntryBits;
    entry.targetPc = request.vbr + request.vectorOffset;
    entry.expevt = request.eventCode;

    if (request.isTrapa) {
        entry.spc = request.pc + 2u;
        entry.tra = static_cast<std::uint32_t>(request.trapaImmediate) << 2;
        entry.traValid = true;
    } else if (request.inDelaySlot) {
        entry.spc = request.pc - 2u;
    } else {
        entry.spc = request.pc;
    }
    return entry;
}

} // namespace cg50
