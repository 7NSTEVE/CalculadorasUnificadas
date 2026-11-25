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

// Official Renesas SH-4A / SH7305 EXPEVT codes (REJ09B0003-0400 Section 5.4):
//   0x040 = ITLB Miss / Data TLB Miss (Read)           -> VBR + 0x400
//   0x060 = Data TLB Miss (Write)                      -> VBR + 0x400
//   0x080 = Initial Page Write                         -> VBR + 0x100
//   0x0A0 = ITLB / Data TLB Protection Violation (Read)-> VBR + 0x100
//   0x0C0 = Data TLB Protection Violation (Write)      -> VBR + 0x100
//   0x0E0/0x100 = Address Error (Read / Write)         -> VBR + 0x100
//   0x140 = TLB Multi-Hit                              -> VBR + 0x100
//   0x160 = TRAPA, 0x180 = General Illegal, 0x1A0 = Slot Illegal -> VBR + 0x100
inline constexpr std::uint32_t kExpevtTrapa = 0x160u;
inline constexpr std::uint32_t kExpevtIllegalInstruction = 0x180u;
inline constexpr std::uint32_t kExpevtSlotIllegalInstruction = 0x1A0u;
inline constexpr std::uint32_t kExpevtItlbMiss = 0x040u;
inline constexpr std::uint32_t kExpevtDataTlbMissRead = 0x040u;
inline constexpr std::uint32_t kExpevtDataTlbMissWrite = 0x060u;
inline constexpr std::uint32_t kExpevtInitialPageWrite = 0x080u;
inline constexpr std::uint32_t kExpevtItlbProtectionViolation = 0x0A0u;
inline constexpr std::uint32_t kExpevtDataTlbProtectionViolation = 0x0A0u;
inline constexpr std::uint32_t kExpevtDataTlbProtectionWrite = 0x0C0u;
inline constexpr std::uint32_t kExpevtFpuDisabled = 0x800u;
inline constexpr std::uint32_t kExpevtFpuException = 0x120u;
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
