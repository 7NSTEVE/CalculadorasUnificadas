#pragma once

#include <cstdint>

namespace cg50 {

inline constexpr std::uint32_t kResetVectorAddress = 0xA0000000u;
inline constexpr std::uint32_t kSrBlockBit = 0x10000000u;
inline constexpr std::uint32_t kSrBankBit = 0x20000000u;
inline constexpr std::uint32_t kSrModeBit = 0x40000000u;
inline constexpr std::uint32_t kSrExceptionEntryBits = kSrBlockBit | kSrBankBit | kSrModeBit;
inline constexpr std::uint32_t kGeneralExceptionVectorOffset = 0x100u;
inline constexpr std::uint32_t kTlbMissVectorOffset = 0x400u;
inline constexpr std::uint32_t kInterruptVectorOffset = 0x600u;

// SH-4A event codes. The address-error pair (0x0E0/0x100), the FPU pair
// (0x120/0x140), the illegal/slot-illegal pair (0x180/0x1A0) and the trapa
// code match both the SH-4 manual and the handler grouping that the CG50 OS
// itself registers in ILRAM at 0xFD800FA8 (probe [INT-TABLE] evidence:
// 0x0E0+0x100 -> one handler, 0x120+0x140 -> one handler, 0x180 -> real
// handler, 0x040/0x060/0x080 -> shared TLB handler, 0x0A0/0x0C0 -> shared
// data-TLB handler). SH-4A exposes no separate data-write protection code,
// so 0x0A0 serves both data protection directions (documented deviation).
inline constexpr std::uint32_t kExpevtTrapa = 0x160u;
inline constexpr std::uint32_t kExpevtIllegalInstruction = 0x180u;
inline constexpr std::uint32_t kExpevtSlotIllegalInstruction = 0x1A0u;
inline constexpr std::uint32_t kExpevtItlbMiss = 0x040u;
inline constexpr std::uint32_t kExpevtItlbProtectionViolation = 0x060u;
inline constexpr std::uint32_t kExpevtDataTlbMissRead = 0x080u;
inline constexpr std::uint32_t kExpevtDataTlbProtectionViolation = 0x0A0u;
inline constexpr std::uint32_t kExpevtDataTlbMissWrite = 0x0C0u;
inline constexpr std::uint32_t kExpevtFpuDisabled = 0x120u;
inline constexpr std::uint32_t kExpevtFpuException = 0x140u;
inline constexpr std::uint32_t kExpevtTlbMultiHit = 0x140u;

struct ExceptionRequest {
    std::uint32_t pc = 0;
    std::uint32_t sr = 0;
    std::uint32_t r15 = 0;
    std::uint32_t vbr = 0;
    std::uint32_t eventCode = 0;
    std::uint32_t vectorOffset = kGeneralExceptionVectorOffset;
    bool inDelaySlot = false;
    bool isTrapa = false;
    std::uint8_t trapaImmediate = 0;
};

struct ExceptionEntry {
    enum class Outcome { Vector, Reset };
    Outcome outcome = Outcome::Vector;
    std::uint32_t ssr = 0;
    std::uint32_t spc = 0;
    std::uint32_t sgr = 0;
    std::uint32_t sr = 0;
    std::uint32_t targetPc = 0;
    std::uint32_t expevt = 0;
    std::uint32_t tra = 0;
    bool traValid = false;
};

ExceptionEntry BuildGeneralExceptionEntry(const ExceptionRequest& request) noexcept;

} // namespace cg50
