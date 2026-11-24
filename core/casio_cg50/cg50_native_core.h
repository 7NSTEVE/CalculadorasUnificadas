#pragma once

#include "cg50_core/cg50_flash_cfi.h"
#include "cg50_core/cg50_peripheral_bus.h"
#include "cg50_core/cg50_sh7305_periph.h"
#include "cg50_core/cg50_ubc.h"
#include "cg50_core/sh_cache.h"
#include "cg50_core/sh_dsp.h"
#ifdef CG50_ENABLE_COVERAGE_COUNTERS
#include "cg50_core/cg50_coverage_collector.h"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Independent fx-CG50 execution core.
//
// This class never loads or calls CASIO GCal/DLSim DLLs. It consumes a decrypted
// firmware image and models the CG50/SH7305 address space directly. The implementation
// is intentionally fail-closed: an unknown instruction or unmapped access stops the
// core at the exact PC instead of fabricating calculator behaviour.
class Cg50NativeCore final {
public:
    static constexpr std::uint32_t kRamBase = 0x08000000u;
    // 0x0C000000 (guest 0x8C000000) NO es un alias del banco 8: el motor
    // oficial (GUEST_DUMP 2026-10-26, ramc-res0ekey-preexe.bin) expone ahi un
    // banco PROPIO, inicializado en ceros, mientras el banco 8 tiene 0xFF en
    // las mismas offsets. El OS usa r13=0x8C000000 como memoria de tareas y
    // decide el flujo de la evaluacion con el byte @(28,r13): 0x00 oficial
    // toma el camino del resultado; 0xFF del alias cae a "No Solution".
    static constexpr std::uint32_t kRamAliasBase = 0x0C000000u;
    static constexpr std::size_t   kRamCSize = 0x00800000u;
    static constexpr std::size_t   kRamSize = 0x00800000u;

    static constexpr std::uint32_t kRomBase = 0xA0000000u;
    static constexpr std::uint32_t kRomPhysicalBase = 0x00000000u;
    static constexpr std::size_t   kRomSize = 0x00B60000u;
    static constexpr std::uint32_t kSystemParameterPhysicalBase = 0x00B20000u;

    // DLSim/CPU73050 exponen la NOR de usuario en DOS alias (verificado contra
    // GUEST_DUMP del emulador oficial, 2026-10-18):
    //   [0x00B60000, 0x01000000)        -> storage[0 .. 0x4A0000)
    //   [0x01000000, 0x01000000 + size) -> storage[0 .. size)
    // El alias B arranca en storage[0] (no en storage+0x4A0000): el chequeo de
    // FS del OS lee el superbloque MCS en 0xA1000000. Ver UserRomStorageOffset
    // en cg50_native_core.cpp (helper usado por todos los accesos).
    static constexpr std::uint32_t kUserRomMappedBase = 0xA0B60000u;
    static constexpr std::uint32_t kUserRomBase = 0xA1000000u;
    static constexpr std::uint32_t kUserRomPhysicalBase = 0x00B60000u;
    static constexpr std::uint32_t kUserRomAliasBPhysicalBase = 0x01000000u;
    static constexpr std::size_t   kUserRomDescriptorOffset = 0x004A0000u;
    static constexpr std::size_t   kUserRomSize = 0x014A0000u;
    static constexpr std::size_t   kCombinedRomSize = kRomSize + kUserRomSize;

    // Four consecutive P4 pages observed on the official CPU73050 page table (RS-RAM).
    static constexpr std::uint32_t kOnChipRamBase = 0xFD800000u;
    static constexpr std::size_t   kOnChipRamSize = 0x00004000u;

    // SH7305 dedicated on-chip high-speed memories (ILRAM 16 KB, XRAM 8 KB, YRAM 8 KB)
    static constexpr std::uint32_t kIlramBase = 0xE5200000u;
    static constexpr std::uint32_t kIlramPhysicalAlias = 0x05200000u;
    static constexpr std::size_t   kIlramSize = 0x00004000u;
    static constexpr std::uint32_t kXramBase = 0xE5007000u;
    static constexpr std::uint32_t kXramPhysicalAlias = 0x05007000u;
    static constexpr std::size_t   kXramSize = 0x00002000u;
    static constexpr std::uint32_t kYramBase = 0xE5017000u;
    static constexpr std::uint32_t kYramPhysicalAlias = 0x05017000u;
    static constexpr std::size_t   kYramSize = 0x00002000u;

    // SH-4A Store Queues (SQ0/SQ1, 2x32 B at 0xE0000000..0xE3FFFFFF) + QACR0/QACR1
    static constexpr std::uint32_t kStoreQueueBase = 0xE0000000u;
    static constexpr std::uint32_t kStoreQueueEnd = 0xE4000000u;
    static constexpr std::uint32_t kQacr0Register = 0xFF000038u;
    static constexpr std::uint32_t kQacr1Register = 0xFF00003Cu;

    // SH-4A ITLB MMIO arrays (4 entries)
    static constexpr std::uint32_t kItlbAddressArrayBase = 0xF2000000u;
    static constexpr std::uint32_t kItlbDataArrayBase = 0xF3000000u;
    static constexpr std::uint32_t kItlbDataArrayEnd = 0xF3800000u;
    static constexpr std::uint32_t kItlbArraysEnd = 0xF4000000u;
    static constexpr std::size_t   kItlbEntryCount = 4u;

    static constexpr std::uint32_t kCacheControlRegister = 0xFF00001Cu;
    // Persistent CCR bits: IIX(15), ICE(8), OIX(7), ORA(5), CB(2), WT(1), OCE(0)
    // (OCI bit 3 and ICI bit 11 are self-clearing invalidate commands).
    static constexpr std::uint32_t kCacheControlWritableMask = 0x000081A7u;
    static constexpr std::uint32_t kMmuControlRegister = 0xFF000010u;
    static constexpr std::uint32_t kMmuControlWritableMask = 0xFCFCFD85u;
    static constexpr std::uint32_t kMmuControlTlbInvalidateCommand = 0x00000004u;

    static constexpr std::uint32_t kTraRegister = 0xFF000020u;
    static constexpr std::uint32_t kTraWritableMask = 0x000003FCu;
    static constexpr std::uint32_t kExpevtRegister = 0xFF000024u;
    static constexpr std::uint32_t kExpevtWritableMask = 0x00000FFFu;
    static constexpr std::uint32_t kIntevtRegister = 0xFF000028u;
    static constexpr std::uint32_t kIntevtWritableMask = 0x00003FFFu;
    static constexpr std::uint32_t kPvrRegister = 0xFF000030u;
    static constexpr std::uint32_t kPrrRegister = 0xFF000044u;
    static constexpr std::uint32_t kCpuopmRegister = 0xFF2F0000u;
    static constexpr std::uint32_t kCpuopmReset = 0x00000320u;
    static constexpr std::uint32_t kCpuopmWritableMask = 0x00000008u;
    static constexpr std::uint32_t kCpuopmForcedMask = 0x00000320u;
    static constexpr std::uint32_t kExpmaskRegister = 0xFF2F0004u;
    static constexpr std::uint32_t kExpmaskWritableMask = 0x0000001Fu;

    // SH7305 INTC: IMR0..IMR12 (0xA4080080..0xA40800B0) and IMCR0..IMCR12 (0xA40800C0..0xA40800F0).
    static constexpr std::uint32_t kInterruptMaskRegisterBase = 0xA4080080u;
    static constexpr std::uint32_t kInterruptMaskRegisterPhysicalBase = 0x04080080u;
    static constexpr std::uint32_t kInterruptMaskClearRegisterBase = 0xA40800C0u;
    static constexpr std::uint32_t kInterruptMaskClearRegisterPhysicalBase = 0x040800C0u;
    static constexpr std::size_t   kInterruptMaskRegisterCount = 13u;
    static constexpr std::uint32_t kInterruptMaskRegister5 = 0xA4080094u;
    static constexpr std::uint32_t kInterruptMaskRegister5Physical = 0x04080094u;

    // IPRA..IPRL are twelve 16-bit priority registers spaced four bytes apart.
    static constexpr std::uint32_t kInterruptPriorityRegisterBase = 0xA4080000u;
    static constexpr std::uint32_t kInterruptPriorityRegisterPhysicalBase = 0x04080000u;
    static constexpr std::uint32_t kInterruptPriorityRegisterStride = 4u;
    static constexpr std::size_t kInterruptPriorityRegisterCount = 12u;
    static constexpr std::uint32_t kInterruptPriorityRegisterF =
        kInterruptPriorityRegisterBase + 5u * kInterruptPriorityRegisterStride;

    // SH7305 key-interface unit recovered from CPU73050's concrete register
    // objects: six read-only data words followed by nine control/status words.
    static constexpr std::uint32_t kKeyInterfaceRegisterBase = 0xA44B0000u;
    static constexpr std::uint32_t kKeyInterfaceRegisterPhysicalBase = 0x044B0000u;
    static constexpr std::size_t kKeyInterfaceDataRegisterCount = 6u;
    static constexpr std::size_t kKeyInterfaceControlRegisterCount = 9u;
    static constexpr std::size_t kKeyInterfaceRegisterCount =
        kKeyInterfaceDataRegisterCount + kKeyInterfaceControlRegisterCount;
    static constexpr std::uint32_t kKeyScanControlRegister = 0xA44B000Cu;
    static constexpr std::uint32_t kKeyAutoFixRegister = 0xA44B000Eu;
    static constexpr std::uint32_t kKeyScanModeRegister = 0xA44B0010u;
    static constexpr std::uint32_t kKeyScanStateRegister = 0xA44B0012u;
    static constexpr std::uint32_t kKeyInterruptRegister = 0xA44B0014u;
    static constexpr std::uint32_t kKeyWaitSetRegister = 0xA44B0016u;
    static constexpr std::uint32_t kKeyIntervalRegister = 0xA44B0018u;
    static constexpr std::uint32_t kKeyOutputPinSetRegister = 0xA44B001Au;
    static constexpr std::uint32_t kKeyInputPinSetRegister = 0xA44B001Cu;

    static constexpr std::uint32_t kModuleStopControlRegister0 = 0xA4150030u;
    static constexpr std::uint32_t kModuleStopControlRegister0Physical = 0x04150030u;
    static constexpr std::uint32_t kModuleStopControlRegister0Reset = 0x00001000u;
    static constexpr std::uint32_t kModuleStopControlRegister0WritableMask = 0xFE7EE0E6u;
    static constexpr std::uint32_t kStandbyControlRegister = 0xA4150020u;
    static constexpr std::uint32_t kStandbyControlRegisterPhysical = 0x04150020u;
    static constexpr std::uint32_t kStandbyControlWritableMask = 0x000000A0u;
    static constexpr std::uint32_t kModuleStopControlRegister2 = 0xA4150038u;
    static constexpr std::uint32_t kModuleStopControlRegister2Physical = 0x04150038u;
    static constexpr std::uint32_t kModuleStopControlRegister2Reset = 0xFFFFFFFFu;
    static constexpr std::uint32_t kModuleStopControlRegister2WritableMask = 0x28126B74u;
    static constexpr std::uint32_t kModuleStopControlRegister2ForcedMask = 0xD7ED948Bu;

    // SH7305 compare-match timer (CMT) register objects recovered from
    // CPU73050.dll.  CMSTR/CMCSR are word-wide; CMCNT/CMCOR are routed through
    // long accesses even though their guest-writable payload is the low word.
    static constexpr std::uint32_t kCompareMatchTimerStartRegister = 0xA44A0000u;
    static constexpr std::uint32_t kCompareMatchTimerStartRegisterPhysical = 0x044A0000u;
    static constexpr std::uint32_t kCompareMatchTimerControlRegister = 0xA44A0060u;
    static constexpr std::uint32_t kCompareMatchTimerControlRegisterPhysical = 0x044A0060u;
    static constexpr std::uint32_t kCompareMatchTimerCounterRegister = 0xA44A0064u;
    static constexpr std::uint32_t kCompareMatchTimerCounterRegisterPhysical = 0x044A0064u;
    static constexpr std::uint32_t kCompareMatchTimerConstantRegister = 0xA44A0068u;
    static constexpr std::uint32_t kCompareMatchTimerConstantRegisterPhysical = 0x044A0068u;
    static constexpr std::uint16_t kCompareMatchTimerStartWritableMask = 0x0020u;
    static constexpr std::uint16_t kCompareMatchTimerControlWritableMask = 0x03B7u;
    static constexpr std::uint16_t kCompareMatchTimerWriteBusyFlag = 0x2000u;
    static constexpr std::uint16_t kCompareMatchTimerOverflowFlag = 0x4000u;
    static constexpr std::uint16_t kCompareMatchTimerCompareFlag = 0x8000u;
    static constexpr std::uint32_t kCompareMatchTimerConstantReset = 0x0000FFFFu;

    // SH7305 port-function controller registers reached by the genuine fx-CG50
    // reset sequence.  Addresses, access widths, reset values and masks come
    // from CPU73050.dll's concrete register descriptors/callbacks, not from a
    // related SuperH part.
    static constexpr std::uint32_t kPortMControlRegister = 0xA4050116u;
    static constexpr std::uint32_t kPortMControlRegisterPhysical = 0x04050116u;
    static constexpr std::uint32_t kPortNControlRegister = 0xA4050118u;
    static constexpr std::uint32_t kPortNControlRegisterPhysical = 0x04050118u;
    static constexpr std::uint32_t kPortPControlRegister = 0xA405014Cu;
    static constexpr std::uint32_t kPortPControlRegisterPhysical = 0x0405014Cu;
    static constexpr std::uint32_t kPortADataRegisterPhysical = 0x04050120u;
    static constexpr std::uint32_t kPortHDataRegisterPhysical = 0x0405012Eu;
    static constexpr std::uint32_t kPortJDataRegisterPhysical = 0x04050130u;
    static constexpr std::uint32_t kPortLDataRegisterPhysical = 0x04050134u;
    static constexpr std::uint32_t kPortMDataRegister = 0xA4050136u;
    static constexpr std::uint32_t kPortMDataRegisterPhysical = 0x04050136u;
    static constexpr std::uint32_t kPortNDataRegister = 0xA4050138u;
    static constexpr std::uint32_t kPortNDataRegisterPhysical = 0x04050138u;
    static constexpr std::uint32_t kPortRDataRegisterPhysical = 0x0405013Cu;
    static constexpr std::uint32_t kPortUDataRegisterPhysical = 0x04050162u;
    static constexpr std::uint32_t kPortPDataRegisterPhysical = 0x0405016Au;
    static constexpr std::uint32_t kPortPPullControlRegister = 0xA40501C6u;
    static constexpr std::uint32_t kPortPPullControlRegisterPhysical = 0x040501C6u;
    // CPU73050 has no concrete descriptor at this PFC byte, but the official
    // emulator's page callback was observed returning zero for the firmware's
    // boot-time read. Keep this narrowly scoped and read-only.
    static constexpr std::uint32_t kPortBootProbeRegister = 0xA405016Cu;
    static constexpr std::uint32_t kPortBootProbeRegisterPhysical = 0x0405016Cu;
    static constexpr std::uint16_t kPortMControlReset = 0x0AAAu;
    static constexpr std::uint16_t kPortNControlReset = 0x0AAAu;
    static constexpr std::uint16_t kPortPControlReset = 0xAAAAu;
    static constexpr std::uint16_t kPortMControlWritableMask = 0x0FFFu;
    static constexpr std::uint16_t kPortNControlWritableMask = 0x0FFFu;
    static constexpr std::uint16_t kPortPControlWritableMask = 0xFFFFu;
    static constexpr std::uint8_t kPortMDataWritableMask = 0x3Fu;
    static constexpr std::uint8_t kPortNDataWritableMask = 0xFFu;
    static constexpr std::uint8_t kPortPPullControlWritableMask = 0xFFu;

    static constexpr std::uint32_t kPfcBasePhysical = 0x04050100u;
    static constexpr std::uint32_t kPfcEndPhysical = 0x040501FFu;
    static constexpr std::size_t   kPfcStorageSize = 256u;

    static constexpr std::uint32_t kPortControlBasePhysical = 0x04050100u;
    static constexpr std::uint32_t kPortControlEndPhysical = 0x0405011Eu;
    static constexpr std::size_t   kPortControlCount = 16u;

    static constexpr std::uint32_t kPortDataBasePhysical = 0x04050120u;
    static constexpr std::uint32_t kPortDataEndPhysical = 0x0405013Eu;
    static constexpr std::size_t   kPortDataCount = 16u;

    static constexpr std::uint32_t kPortPullControlBasePhysical = 0x04050190u;
    static constexpr std::uint32_t kPortPullControlEndPhysical = 0x040501C7u;
    static constexpr std::size_t   kPortPullControlCount = 56u;

    static constexpr std::uint32_t kRtc64HzCounterRegisterPhysical = 0x0413FEC0u;

    static constexpr std::uint32_t kSdramModeRegister2Base = 0xFEC14000u;
    static constexpr std::uint32_t kSdramModeRegister3End = 0xFEC15FFFu;
    static constexpr std::uint32_t kDriveControlRegisterBase = 0xA4050184u;
    static constexpr std::uint32_t kDriveControlRegisterPhysicalBase = 0x04050184u;
    static constexpr std::size_t kDriveControlRegisterCount = 4u;
    // Address order is DRVCRD, DRVCRA, DRVCRB, DRVCRC.
    inline static constexpr std::array<std::uint16_t, kDriveControlRegisterCount>
        kDriveControlReset{{0xFFFCu, 0xFFFFu, 0xFFFFu, 0xFF3Fu}};
    inline static constexpr std::array<std::uint16_t, kDriveControlRegisterCount>
        kDriveControlWritableMask{{0xFFFCu, 0xFFFFu, 0xFFFFu, 0xFF3Fu}};

    // Bus state controller (BSC) registers from CPU73050.dll (CMNCR, CS0BCR..CS6BBCR, CS0WCR..CS6BWCR, SDCR, RTCSR, RTCNT, RTCOR).
    // These are P4 registers (0xFEC10000..0xFEC10050) and therefore are not passed through P0/P1/P2 aliasing.
    static constexpr std::uint32_t kBscRegisterBase = 0xFEC10000u;
    static constexpr std::uint32_t kBscRegisterEnd = 0xFEC10050u;
    static constexpr std::size_t kBscRegisterCount = 21u;
    static constexpr std::uint32_t kBusCommonControlRegister = kBscRegisterBase;
    static constexpr std::uint32_t kBusCommonControlReset = 0x00000010u;
    static constexpr std::uint32_t kBusCommonControlWritableMask = 0x03003E03u;
    static constexpr std::uint32_t kBusCommonControlForcedMask = 0x00000010u;
    static constexpr std::uint32_t kBscBusControlWritableMask = 0x7FFFF600u;
    static constexpr std::uint32_t kBscSdramControlWritableMask = 0x001B2F1Bu;

    // SH7305 P4 peripheral pages recovered from CPU73050.dll page registration table (0x1001e159..0x1001e224).
    static constexpr std::uint32_t kIlcPageBase = 0xFC110000u;
    static constexpr std::size_t   kIlcPageSize = 0x1000u;
    static constexpr std::uint32_t kDmacPageBase = 0xFD000000u;
    static constexpr std::size_t   kDmacPageSize = 0x2000u;
    static constexpr std::uint32_t kDmarsPageBase = 0xFE008000u;
    static constexpr std::size_t   kDmarsPageSize = 0x2000u;
    static constexpr std::uint32_t kDsp0SpaceBase = 0xFE200000u;
    static constexpr std::size_t   kDsp0SpaceSize = 0x00100000u; // 1 MB
    static constexpr std::uint32_t kDsp1SpaceBase = 0xFE300000u;
    static constexpr std::size_t   kDsp1SpaceSize = 0x00100000u; // 1 MB (includes SPU at 0xFE3C0000)
    static constexpr std::uint32_t kUbcPageBase = 0xFF200000u;
    static constexpr std::size_t   kUbcPageSize = 0x1000u;
    static constexpr std::uint32_t kPriorityLockPageBase = 0xFF800000u;
    static constexpr std::size_t   kPriorityLockPageSize = 0x1000u;

    static constexpr std::uint32_t kFrequencyControlRegister = 0xA4150000u;
    static constexpr std::uint32_t kFrequencyControlRegisterPhysical = 0x04150000u;
    static constexpr std::uint32_t kFrequencyControlReset = 0x00000000u;
    static constexpr std::uint32_t kFrequencyControlWritableMask = 0xBFFFFFFFu;
    static constexpr std::uint32_t kFrequencyControlKickMask = 0x80000000u;

    static constexpr std::uint32_t kPllControlRegister = 0xA4150024u;
    static constexpr std::uint32_t kPllControlRegisterPhysical = 0x04150024u;
    static constexpr std::uint32_t kPllControlReset = 0x00005000u;
    static constexpr std::uint32_t kPllControlWritableMask = 0x00005006u;
    static constexpr std::uint32_t kFllFrequencyRegister = 0xA4150050u;
    static constexpr std::uint32_t kFllFrequencyRegisterPhysical = 0x04150050u;
    static constexpr std::uint32_t kFllFrequencyReset = 0x000042FCu;
    static constexpr std::uint32_t kFllFrequencyWritableMask = 0x000007FFu;
    static constexpr std::uint32_t kFllFrequencyForcedMask = 0x00004000u;
    static constexpr std::uint32_t kClockChangeStatusRegister = 0xA4150060u;
    static constexpr std::uint32_t kClockChangeStatusRegisterPhysical = 0x04150060u;

    static constexpr std::uint32_t kFsiClockControlRegisterPhysical = 0x04150008u;
    static constexpr std::uint32_t kFsiClockControlWritableMask = 0x0000FDFFu;

    static constexpr std::uint32_t kDdClockControlRegisterPhysical = 0x04150010u;
    static constexpr std::uint32_t kDdClockControlWritableMask = 0x000001BFu;

    static constexpr std::uint32_t kUsbClockControlRegisterPhysical = 0x04150014u;
    static constexpr std::uint32_t kUsbClockControlWritableMask = 0x00010000u;

    static constexpr std::uint32_t kPll2ControlRegisterPhysical = 0x04150028u;
    static constexpr std::uint32_t kPll2ControlWritableMask = 0xBF000000u;
    static constexpr std::uint32_t kPll2ControlKickMask = 0x80000000u;

    static constexpr std::uint32_t kSpuClockControlRegisterPhysical = 0x0415003Cu;
    static constexpr std::uint32_t kSpuClockControlWritableMask = 0x000001BFu;

    static constexpr std::uint32_t kVpuClockControlRegisterPhysical = 0x04150040u;

    static constexpr std::uint32_t kSpreadSpectrumControlRegisterPhysical = 0x04150044u;
    static constexpr std::uint32_t kSpreadSpectrumControlWritableMask = 0x00000001u;

    static constexpr std::uint32_t kWatchdogCounterRegister = 0xA4520000u;
    static constexpr std::uint32_t kWatchdogCounterRegisterPhysical = 0x04520000u;
    static constexpr std::uint32_t kWatchdogControlRegister = 0xA4520004u;
    static constexpr std::uint32_t kWatchdogControlRegisterPhysical = 0x04520004u;
    static constexpr std::uint8_t kWatchdogControlReset = 0x87u;
    static constexpr std::uint16_t kWatchdogControlWriteKey = 0xA500u;

    // SH7305 physical Area 1 on-chip peripheral modules (0x04000000..0x04FFFFFF)
    // registered by CPU73050.dll with 8KB page buffers (0x800 dwords).
    static constexpr std::uint32_t kArea1BasePhysical = 0x04000000u;
    static constexpr std::uint32_t kArea1EndPhysical = 0x05000000u;
    static constexpr std::size_t   kArea1PageTableEntries = 4096u;
    static constexpr std::size_t   kArea1PeripheralPageCount = 25u;
    static constexpr std::size_t   kArea1PeripheralPageSize = 8192u;

    static constexpr std::uint32_t kLcdBase = 0x14000000u;
    static constexpr std::uint32_t kLcdBusEnd = 0x15000000u;
    static constexpr bool IsLcdBusPhysical(const std::uint32_t physical) noexcept
    {
        return physical >= kLcdBase && physical < kLcdBusEnd;
    }

    static constexpr std::uint32_t kSrT = 0x00000001u;
    static constexpr std::uint32_t kSrS = 0x00000002u;
    static constexpr std::uint32_t kSrImask = 0x000000F0u;
    static constexpr std::uint32_t kSrQ = 0x00000100u;
    static constexpr std::uint32_t kSrM = 0x00000200u;
    static constexpr std::uint32_t kSrDsp = 0x00001000u;
    static constexpr std::uint32_t kSrFd = 0x00008000u;
    static constexpr std::uint32_t kSrRcMask = 0x0FFF0000u;
    static constexpr std::uint32_t kSrBl = 0x10000000u;
    static constexpr std::uint32_t kSrRb = 0x20000000u;
    static constexpr std::uint32_t kSrMd = 0x40000000u;
    static constexpr std::uint32_t kSrWritableMask =
        kSrMd | kSrRb | kSrBl | kSrRcMask | kSrFd | kSrDsp | kSrM | kSrQ | kSrImask | kSrS | kSrT;
    static constexpr std::uint32_t kResetSr = kSrMd | kSrRb | kSrBl | kSrImask;

    // ---- SH-4A FPU (FPSCR) ----
    // Layout per SH-4/SH-4A software manual: RM bits 0-1, enable triplet
    // bits 10-12 (I, U/Z, V), cause triplet 13-15, flag triplet 16-18,
    // DN bit 19, PR bit 20, SZ bit 21, FR bit 22. Reserved bits read 0.
    static constexpr std::uint32_t kFpscrRoundingMask = 0x00000003u;
    static constexpr std::uint32_t kFpscrEnableInexact = 0x00000400u;
    static constexpr std::uint32_t kFpscrEnableUnderflowOrDivZero = 0x00000800u;
    static constexpr std::uint32_t kFpscrEnableInvalid = 0x00001000u;
    static constexpr std::uint32_t kFpscrCauseMask = 0x0000E000u;
    static constexpr std::uint32_t kFpscrFlagMask = 0x00070000u;
    static constexpr std::uint32_t kFpscrDn = 0x00080000u;
    static constexpr std::uint32_t kFpscrPr = 0x00100000u;
    static constexpr std::uint32_t kFpscrSz = 0x00200000u;
    static constexpr std::uint32_t kFpscrFr = 0x00400000u;
    static constexpr std::uint32_t kFpscrWritableMask =
        kFpscrFr | kFpscrSz | kFpscrPr | kFpscrDn | kFpscrFlagMask |
        kFpscrCauseMask | kFpscrEnableInexact |
        kFpscrEnableUnderflowOrDivZero | kFpscrEnableInvalid | kFpscrRoundingMask;

    // ---- SH7305 interrupt event codes (INTEVT) ----
    // TMU0/1/2 event codes are the architectural SH values; the WDT and KIU
    // codes were corroborated against the OS handler table the firmware
    // registers in ILRAM at 0xFD800FA8 (see probe [INT-TABLE] evidence).
    static constexpr std::uint32_t kEventTmu0 = 0x00000400u;
    static constexpr std::uint32_t kEventTmu1 = 0x00000420u;
    static constexpr std::uint32_t kEventTmu2 = 0x00000440u;
    static constexpr std::uint32_t kEventWatchdog = 0x00000560u;
    static constexpr std::uint32_t kEventKeyScan = 0x00000BE0u;
    static constexpr std::uint32_t kEventRtcAlarm = 0x00000480u;
    static constexpr std::uint32_t kEventRtcCarry = 0x000004A0u;
    static constexpr std::uint32_t kEventRtcPeriodic = 0x000004E0u;
    static constexpr std::uint32_t kEventDmacDei0 = 0x00000800u;
    static constexpr std::uint32_t kEventDmacDei1 = 0x00000820u;
    static constexpr std::uint32_t kEventDmacDei2 = 0x00000840u;
    static constexpr std::uint32_t kEventDmacDei3 = 0x00000860u;
    static constexpr std::uint32_t kEventDmacDei4 = 0x00000B80u;
    static constexpr std::uint32_t kEventDmacDei5 = 0x00000BA0u;
    static constexpr std::uint32_t kEventEtmu0 = 0x000009E0u;
    static constexpr std::uint32_t kEventEtmu1 = 0x00000C20u;
    static constexpr std::uint32_t kEventEtmu2 = 0x00000C40u;
    static constexpr std::uint32_t kEventEtmu3 = 0x00000900u;
    static constexpr std::uint32_t kEventEtmu4 = 0x00000D00u;
    static constexpr std::uint32_t kEventEtmu5 = 0x00000FA0u;
    static constexpr std::uint32_t kEventCmt = 0x00000F00u;
    static constexpr std::uint32_t kEventScif0 = 0x00000C00u;
    static constexpr std::uint32_t kEventAdc = 0x00000980u;
    static constexpr std::uint32_t kEventTpu = 0x00000960u;
    static constexpr std::uint32_t kEventUsb = 0x00000A20u;

    // ---- SH7305 SH-4A MMU (P4 control registers) ----
    static constexpr std::uint32_t kPtehRegister = 0xFF000000u;
    static constexpr std::uint32_t kPtelRegister = 0xFF000004u;
    static constexpr std::uint32_t kTtbRegister = 0xFF000008u;
    static constexpr std::uint32_t kTeaRegister = 0xFF00000Cu;
    static constexpr std::uint32_t kPteaRegister = 0xFF000034u;
    static constexpr std::uint32_t kMmuControlAtBit = 0x00000001u;
    // 2026-10-29 M3: bit SV (single virtual mode) de MMUCR. MIT mmu.go:118:
    // el match tambien cubre MD=1 && SV=1 sin exigir ASID igual. La mascara
    // writable de MMUCR ya incluye el bit.
    static constexpr std::uint32_t kMmuControlSvBit = 0x00000100u;
    // 2026-10-29 M9: arrays MMIO de la UTLB (calco de mmu.go:216-258).
    // Address array 0xF6000000-0xF6FFFFFF; data array 1 = 0xF7000000-
    // 0xF77FFFFF; data array 2 (PTEA) = 0xF7800000+. Indice de entrada =
    // (va >> 8) & 0x3F (bits 13:8 de va). Escritura del address array con
    // va bit7 (0x80) = 1 es ASOCIATIVA (V/D en todas las entradas cuyo VPN
    // encaje a granularidad de pagina y cuyo ASID coincida o sea shared/SV);
    // bit7 = 0 escribe la entrada unica.
    static constexpr std::uint32_t kUtlbAddressArrayBase = 0xF6000000u;
    static constexpr std::uint32_t kUtlbDataArrayBase = 0xF7000000u;
    static constexpr std::uint32_t kUtlbDataArrayEnd = 0xF7800000u;
    static constexpr std::uint32_t kUtlbArraysEnd = 0xF8000000u;
    static constexpr std::size_t kUtlbEntryCount = 64u;

    // ---- SH7305 RTC (0xA413FEC0, 16-bit stride) ----
    // Base follows the CPU73050-recovered R64CNT physical (0x0413FEC0).
    // The OS writes observed at menu idle: RCR1=0x0100, RCR2=0x0900.
    static constexpr std::uint32_t kRtcRegisterPhysicalBase = 0x0413FEC0u;
    static constexpr std::uint32_t kRtcRegisterBase = 0xA413FEC0u;
    static constexpr std::size_t kRtcRegisterCount = 16u;
    // Deterministic virtual clock (opt-in). Default OFF: the OS boot is
    // bit-validated against reads of zeros through the peripheral page
    // (official-correlated baseline, 198,617,945 instructions to SLEEP).
    // Enabling it derives BCD time components from the instruction counter
    // (anchored on guest writes) so the clock application ticks; the OS
    // takes a measurably different (longer) boot path when it is on, which
    // is why it never defaults on without differential evidence.
    static constexpr std::uint64_t kRtc64HzInstructionsPerTick = 500u;
    static constexpr std::uint64_t kRtcInstructionsPerSecond =
        kRtc64HzInstructionsPerTick * 64u;

    void SetRtcVirtualClockEnabled(const bool enabled) noexcept
    {
        rtcVirtualClock_ = enabled;
    }

    // ---- SH7305 TMU unit 0 (0xA4480000, register file as dedicated MMIO) ----
    // The CPU73050-recovered page map has NO backing page at 0x04480000
    // (the official handles these registers through callbacks); unit 0 is
    // therefore modeled as a dedicated register file here, mirroring how
    // the CMT and KIU registers are modeled. Unit 1 (0xA4490000) IS a
    // mapped page and keeps the page-backed tick.
    struct Tmu0Channel {
        std::uint32_t tcor = 0;
        std::uint32_t tcnt = 0;
        std::uint16_t tcr = 0;
        bool running = false;
    };
    static constexpr std::uint32_t kTmu0PhysicalBase = 0x04480000u;
    static constexpr std::uint32_t kTmu0VirtualBase = 0xA4480000u;

    static constexpr std::uint32_t kTmuTcrUnderflowFlag = 0x0100u;  // TCR bit 8 (UNF)
    static constexpr std::uint32_t kTmuTcrUnderflowInterruptEnable = 0x0020u;  // TCR bit 5 (UNIE)


    enum class State {
        Empty,
        Running,
        Sleeping,
        Halted
    };

    struct Registers {
        std::array<std::uint32_t, 16> r{};
        std::uint32_t pc = kRomBase;
        std::uint32_t pr = 0;
        std::uint32_t sr = kResetSr;
        std::uint32_t gbr = 0;
        std::uint32_t vbr = 0;
        std::uint32_t mach = 0;
        std::uint32_t macl = 0;
        std::uint32_t ssr = 0;
        std::uint32_t spc = 0;
        std::uint32_t sgr = 0;
        std::uint32_t dbr = 0;
        std::uint32_t tra = 0;
        std::uint32_t expevt = 0;
        std::uint32_t intevt = 0;
        // SH-4A FPU state. fr_/xf_ are the two 16-register banks selected by
        // FPSCR.FR; fpul is the integer/float communication register.
        std::array<std::uint32_t, 16> fr{};
        std::array<std::uint32_t, 16> xf{};
        std::uint32_t fpul = 0;
        std::uint32_t fpscr = 0;
    };

    // Diagnostic values that can legitimately be zero carry an independent
    // update counter so zero never doubles as "event absent".
    template <typename T>
    struct EventValue {
        T value{};
        std::uint64_t updates = 0;

        EventValue& operator=(const T next) noexcept {
            value = next;
            ++updates;
            return *this;
        }
        constexpr operator T() const noexcept { return value; }
    };

    struct Stats {
        std::uint64_t instructions = 0;
        std::uint64_t cycles = 0;
        std::uint32_t lastPc = 0;
        std::uint16_t lastOpcode = 0;
        std::uint32_t unsupportedPc = 0;
        EventValue<std::uint16_t> unsupportedOpcode{};
        EventValue<std::uint32_t> lastMemoryFaultAddress{};
        bool lastMemoryFaultWasWrite = false;

        std::uint64_t exceptionsTaken = 0;
        std::uint32_t lastExceptionEvent = 0;
        std::uint64_t interruptsAccepted = 0;
        std::uint32_t lastInterruptEvent = 0;
        std::uint8_t lastInterruptPriority = 0;
    };

    // Separate from Stats so save-state chunk 17 (sizeof(Stats)) stays 100% compatible.
    struct AuthenticityStats {
        std::uint64_t fpuOpcodesExecuted = 0;
        std::uint64_t dspOpcodesExecuted = 0;
        std::uint64_t bcdAluOpsExecuted = 0;
        std::uint64_t appIfTrapsExecuted = 0;
        std::uint64_t hostMathFallbacksExecuted = 0; // Guaranteed 0 without CG50_ENABLE_LEGACY_DEBUG_HLE
        std::uint64_t strictNoHleViolations = 0;
        std::uint64_t watchdogResets = 0;
        std::uint64_t ubcBreaks = 0;
    };

    Cg50NativeCore();
    ~Cg50NativeCore();
#ifdef CG50_ENABLE_COVERAGE_COUNTERS
    bool DumpCoverageJson(const std::string& path) const;
#endif

    bool LoadFirmware(const std::uint8_t* bytes, std::size_t size);
    bool LoadFirmwareParts(const std::uint8_t* systemRom, std::size_t systemSize,
                           const std::uint8_t* userRom, std::size_t userSize);

    // Full machine/power-on-style reset used after firmware loading. It resets
    // CPU state plus current modeled external board state.
    void Reset();

    // CPU-only reset for debugger/front-end use. External RAM, key contacts and
    // peripheral state are preserved because their reset behavior is not assumed
    // without board-level evidence.
    void ResetCpuState();

    // Android/portable in-memory ILRAM seed equivalent to CG50_ILRAM_FILE.
    bool SetIlramSeed(const std::uint8_t* bytes, std::size_t size);

    bool StepInstruction();
    std::uint64_t RunInstructions(std::uint64_t maxInstructions);

    // N-12: avanza el tiempo de los perifericos (CMT/TMU/WDT/RTC) mientras la
    // CPU duerme, sin buscar instrucciones. Entrega los pendientes y devuelve
    // true si la CPU desperto (state != Sleeping) o ya estaba despierta.
    bool AdvanceCycles(std::uint64_t cycles);

    bool Read8(std::uint32_t address, std::uint8_t& value) const;
    bool Read16(std::uint32_t address, std::uint16_t& value) const;
    bool Read32(std::uint32_t address, std::uint32_t& value) const;
    bool Write8(std::uint32_t address, std::uint8_t value);
    bool Write16(std::uint32_t address, std::uint16_t value);
    bool Write32(std::uint32_t address, std::uint32_t value);

    bool KeyDown(std::uint16_t matrixCode);
    void KeyUp(std::uint16_t matrixCode);
    bool IsKeyDown(std::uint16_t matrixCode) const noexcept;

    bool RaiseMaskableInterrupt(std::uint32_t eventCode, std::uint8_t priority);

    // Interrupt priority resolution against the OS-configured SH7305 INTC
    // IPR registers. Event-to-field mapping: TMU0/1/2 -> IPRA bits 15-12 /
    // 11-8 / 7-4; KIU (0x0BE0) -> IPRF bits 15-12 (the OS writes 0xD000 at
    // menu idle, i.e. priority 13); WDT (0x0560) and RTC events -> IPRB bits
    // 15-12 (best-effort; both remain 0 in the observed boot). Returns 0
    // when masked, which RaiseMaskableInterrupt rejects like real hardware.
    std::uint8_t ResolveInterruptPriority(std::uint32_t eventCode) const noexcept;

    // Full machine snapshot. Includes the mutable UserROM storage image, all
    // RAM/peripheral pages and CPU/FPU state, so a restore resumes exactly.
    // ImportState is all-or-nothing per chunk header validation; a failed
    // import leaves the core unloaded (state_ = Halted, diagnostic set).
    bool ExportState(std::vector<std::uint8_t>& bytes) const;
    bool ImportState(const std::uint8_t* bytes, std::size_t size);

    State GetState() const noexcept { return state_; }
    const Registers& GetRegisters() const noexcept { return regs_; }
    const Stats& GetStats() const noexcept { return stats_; }
    const std::wstring& Diagnostic() const noexcept { return diagnostic_; }
    bool HasSystemRom() const noexcept { return rom_.size() == kRomSize; }
    const std::uint8_t* RomData() const noexcept { return rom_.data(); }
    std::size_t RomSize() const noexcept { return rom_.size(); }
    bool HasUserRom() const noexcept { return userRom_.size() == kUserRomSize; }
    const std::uint8_t* UserRomData() const noexcept { return userRom_.data(); }
    std::size_t UserRomSize() const noexcept { return userRom_.size(); }
    // C2ROM: segunda imagen flash (ventana [0x00B60000, 0x01000000)), reconstruida
    // del GUEST_DUMP del oficial (2026-10-18). Sin ella se usa el mapeo legado
    // (ventana 1 -> storage[0..0x4A0000)).
    static constexpr std::size_t kC2RomSize = 0x4A0000u;
    bool LoadC2Rom(const std::uint8_t* bytes, std::size_t size);
    bool HasC2Rom() const noexcept { return c2Rom_.size() == kC2RomSize; }
    const std::uint8_t* C2RomData() const noexcept { return c2Rom_.data(); }
    std::size_t C2RomSize() const noexcept { return c2Rom_.size(); }

    // 2026-10-29 (D/N-12): model-strap inicial de EXPEVT (0xFF000024), igual
    // que la referencia (mmu.go:78-87: el read de EXPEVT devuelve el valor
    // fijado aqui hasta la primera excepcion; 0x0A02 = fx-CG50). Sin este
    // setter el arranque previo se conserva (EXPEVT=0). Aplica inmediato y
    // en cada Reset(); un ImportState posterior introduce su propio regs
    // (regla: si el estado carga, el estado manda).
    void SetStrapExpevt(std::uint32_t value) noexcept
    {
        strapExpevtSet_ = true;
        strapExpevt_ = value & 0xFFFFu;
        regs_.expevt = strapExpevt_;
    }

    // 2026-10-29 M6/N-11 (INFORME-G3A 5.3 escenario 1, recomendado): ventana
    // de staging dedicada para add-ins. El UTLB del guest podra mapear la
    // ventana fisica [0x08080000, 0x08080000+0x00200000); el core sirve el
    // payload del .g3a (segmento PT_LOAD en 0x7000 del archivo) desde un
    // backing host aparte, sin tocar SystemROM. PRUDENTE: sin payload
    // cargado el core queda byte-identico (fall-through al backing fisico
    // normal); la ventana SOLO ensombrece el rango exacto del payload.
    // Nota de colision documentada: en el modelo de este core la RAM ocupa
    // [0x08000000, 0x08800000), con lo cual 0x08080000 cae dentro de RAM;
    // el staging host->core se usa con el UTLB del maestro (VA 0x00300000 ->
    // PA 0x08080000) para la deliver del stage-2 y se declara el coste:
    // mientras haya payload, las direcciones de la ventana van al staging.
    // El staging NO se serializa al save-state (los pases validados no lo
    // carga; el sonde lo repasa por corrida). CG50_ADDIN_STAGING=0 apaga la
    // activacion sin reconstruir el binario (default ON con payload).
    bool SetAddinStaging(const std::uint8_t* bytes, std::size_t size);
    bool AddinStagingActive() const noexcept { return addinStagingActive_; }
    static bool ValidateG3aHeader(const std::uint8_t* bytes, std::size_t size) noexcept;
    bool LoadG3aAddin(const std::uint8_t* bytes, std::size_t size);
    bool ReplaceUserRom(const std::uint8_t* bytes, std::size_t size);
    std::uint32_t LastLcdDmaSar() const noexcept { return lastLcdDmaSar_; }

    // Real-time wall-clock RTC synchronization (localUnixSeconds since 1970-01-01)
    // and opt-in RTC carry IRQ delivery (default OFF to keep deterministic boot).
    void SetRtcEpochSeconds(std::int64_t localUnixSeconds) noexcept;
    void SetRtcIrqEnabled(bool enabled) noexcept { rtcIrqEnabled_ = enabled; }
    bool RtcIrqEnabled() const noexcept { return rtcIrqEnabled_; }
    static constexpr std::uint32_t kAddinStagingPhysicalBase = 0x08080000u;
    static constexpr std::size_t kAddinStagingSlotSize = 0x00200000u;

    cg50::PeripheralBus& Peripherals() noexcept { return peripheralBus_; }
    const cg50::PeripheralBus& Peripherals() const noexcept { return peripheralBus_; }

    const cg50::SpansionNorFlashCfi::Stats& FlashCfiStats() const noexcept { return flashCfi_.GetStats(); }
    cg50::SpansionNorFlashCfi& FlashCfiMut() noexcept { return flashCfi_; }
    const cg50::Intc2Unit& Intc2() const noexcept { return intc2_; }
    cg50::Intc2Unit& Intc2Mut() noexcept { return intc2_; }
    const cg50::EtmuUnit& Etmu() const noexcept { return etmu_; }
    const cg50::AdcUnit& Adc() const noexcept { return adc_; }
    cg50::AdcUnit& AdcMut() noexcept { return adc_; }
    const cg50::DdBusUnit& Tpu() const noexcept { return tpu_; }
    const cg50::DdBusUnit& DdBus() const noexcept { return tpu_; }
    const cg50::SdhiUnit& Sdhi() const noexcept { return sdhi_; }
    const cg50::Scif0Unit& Scif0() const noexcept { return scif0_; }
    cg50::Scif0Unit& Scif0Mut() noexcept { return scif0_; }
    const cg50::Usb2Unit& Usb2() const noexcept { return usb2_; }
    cg50::Usb2Unit& Usb2Mut() noexcept { return usb2_; }

    // Virtual USB cable & external SCIF0 serial link controls (v45)
    void SetUsbCableConnected(bool connected) noexcept;
    bool IsUsbCableConnected() const noexcept { return usb2_.VbusAttached(); }
    void SetExternalLinkEnabled(bool enabled) noexcept;
    bool IsExternalLinkEnabled() const noexcept { return externalLinkEnabled_; }
    std::size_t LinkPushRx(const std::uint8_t* data, std::size_t len) noexcept;
    std::size_t LinkPopTx(std::uint8_t* out, std::size_t maxLen) noexcept;

    // Direct storage backing access for verified bit-exact Fugue FTL + FAT16 SMEM manager
    std::vector<std::uint8_t>& UserRomMut() noexcept { return userRom_; }
    std::vector<std::uint8_t>& C2RomMut() noexcept { return c2Rom_; }
    void NotifyFlashStorageMutated() noexcept;
    const cg50::ShDspState& Dsp() const noexcept { return dsp_; }
    cg50::ShDspState& DspMut() noexcept { return dsp_; }
    const cg50::ShCacheUnit& Cache() const noexcept { return cache_; }
    cg50::ShCacheUnit& CacheMut() noexcept { return cache_; }
    const cg50::UbcUnit& Ubc() const noexcept { return ubc_; }
    const AuthenticityStats& GetAuthenticityStats() const noexcept { return authStats_; }
    void SetStrictSh7305NoFpu(bool enabled) noexcept { strictSh7305NoFpu_ = enabled; }
    bool StrictSh7305NoFpu() const noexcept { return strictSh7305NoFpu_; }
    void SetWatchdogResetEnabled(bool enabled) noexcept { watchdogResetEnabled_ = enabled; }
    void SetBatteryPercent(int percent) noexcept { adc_.SetBatteryPercent(percent); }
    static std::size_t AuditForbiddenEnvOverrides(std::vector<std::string>& activeOut);
    std::uint32_t Qacr0() const noexcept { return qacr0_; }
    std::uint32_t Qacr1() const noexcept { return qacr1_; }
    std::uint32_t Ramcr() const noexcept { return ramcr_; }
    std::uint32_t Pascr() const noexcept { return pascr_; }
    std::uint32_t Irmcr() const noexcept { return irmcr_; }
    const cg50::HudiUnit& Hudi() const noexcept { return hudi_; }
    cg50::HudiUnit& HudiMut() noexcept { return hudi_; }
    const cg50::PrlckUnit& Prlck() const noexcept { return prlck_; }
    cg50::SdhiUnit& SdhiMut() noexcept { return sdhi_; }
    const cg50::RtcExtState& RtcExt() const noexcept { return rtcExt_; }
    const cg50::Dmac0ExtState& DmacExt() const noexcept { return dmacExt_; }
    const cg50::ExtLy755aState& ExtLy755a() const noexcept { return extLy755a_; }
    std::uint32_t BscRefreshCount() const noexcept { return bscRefreshCount_; }
    std::uint32_t EffectiveCpuClockHz() const noexcept;
    bool IsCpgModuleStopped(std::uint32_t mstpcrIndex, std::uint32_t bitMask) const noexcept;

    static std::uint32_t DirectMappedPhysicalAddress(std::uint32_t address) noexcept;

private:
    std::vector<std::uint8_t> rom_;
    std::vector<std::uint8_t> userRom_;
    std::vector<std::uint8_t> c2Rom_;
    // Maquina de estados NOR Flash CFI Spansion S29GL256P (128 KB sectores,
    // Sector Erase 0x30, Write-to-Buffer 0x25..0x29, Word Program 0xA0,
    // Autoselect 0x90, CFI Query 0x98, Reset/Abort 0xF0).
    cg50::SpansionNorFlashCfi flashCfi_{};
    bool IsFlashPhysicalAddress(std::uint32_t physical) const noexcept;
    void ProgramFlashWord16(std::uint32_t physical, std::uint16_t value) noexcept;
    void EraseFlashSector(std::uint32_t sectorBasePhysical) noexcept;
    std::vector<std::uint8_t> ram_;
    // Banco C separado (guest 0x8C000000 / fisico 0x0C000000): el oraculo
    // oficial lo expone en ceros, independiente del banco 8.
    std::vector<std::uint8_t> ramC_;
    std::array<std::uint8_t, kOnChipRamSize> onChipRam_{};
    std::array<std::uint8_t, kIlramSize> ilram_{};
    std::array<std::uint8_t, kXramSize> xram_{};
    std::array<std::uint8_t, kYramSize> yram_{};
    std::array<std::uint8_t, 64> storeQueues_{};
    std::uint32_t qacr0_ = 0;
    std::uint32_t qacr1_ = 0;
    cg50::Intc2Unit intc2_{};
    cg50::EtmuUnit etmu_{};
    cg50::AdcUnit adc_{};
    cg50::DdBusUnit tpu_{};
    cg50::SdhiUnit sdhi_{};
    cg50::HudiUnit hudi_{};
    cg50::PrlckUnit prlck_{};
    cg50::RtcExtState rtcExt_{};
    cg50::Dmac0ExtState dmacExt_{};
    cg50::ExtLy755aState extLy755a_{};
    std::uint64_t bscRefreshPrescaler_ = 0;
    std::uint32_t bscRefreshCount_ = 0;
    cg50::Scif0Unit scif0_{};
    cg50::Scif0Unit externalLinkSink_{};
    bool externalLinkEnabled_ = false;
    cg50::Usb2Unit usb2_{};
    cg50::ShDspState dsp_{};
    cg50::ShCacheUnit cache_{};
    cg50::UbcUnit ubc_{};
    std::uint32_t pendingEtmuIrq_ = 0;
    std::uint32_t pendingDmacIrq_ = 0;
    bool pendingCmtIrq_ = false;
    std::uint32_t ramcr_ = 0;
    std::uint32_t pascr_ = 0;
    std::uint32_t irmcr_ = 0;
    std::array<std::uint32_t, 8> xyDspCtrl_{}; // XSA, YSA, XDA, YDA, XPR, YPR, XEA, YEA
    std::size_t itlbNextVictim_ = 0;
    bool strictSh7305NoFpu_ = false;
    bool watchdogResetEnabled_ = false;
    AuthenticityStats authStats_{};
    std::vector<std::uint8_t> dsp0Space_;
    std::vector<std::uint8_t> dsp1Space_;
    std::array<std::uint8_t, kIlcPageSize> ilcPage_{};
    std::array<std::uint8_t, kDmacPageSize> dmacPage_{};
    std::array<std::uint8_t, kDmarsPageSize> dmarsPage_{};
    std::array<std::uint8_t, kUbcPageSize> ubcPage_{};
    std::array<std::uint8_t, kPriorityLockPageSize> priorityLockPage_{};
    std::array<std::array<std::uint8_t, kArea1PeripheralPageSize>, kArea1PeripheralPageCount>
        area1PeripheralPages_{};
    std::array<std::uint8_t*, kArea1PageTableEntries> area1PageTable_{};
    std::array<std::uint8_t, 65536> keyState_{};
    std::array<std::uint32_t, 8> alternateBank_{};
    std::uint32_t cacheControl_ = 0;
    std::uint32_t mmuControl_ = 0;
    std::uint32_t cpuopm_ = kCpuopmReset;
    std::uint32_t expmask_ = 0;
    std::array<std::uint8_t, kInterruptMaskRegisterCount> interruptMask_{};
    std::uint8_t interruptMask5_ = 0;
    std::array<std::uint16_t, kInterruptPriorityRegisterCount> interruptPriority_{};
    std::array<std::uint16_t, kKeyInterfaceDataRegisterCount> keyInputData_{};
    std::array<std::uint16_t, kKeyInterfaceControlRegisterCount> keyInterfaceControl_{};
    std::uint32_t moduleStopControl0_ = kModuleStopControlRegister0Reset;
    std::uint32_t standbyControl_ = 0;
    std::uint32_t moduleStopControl2_ = kModuleStopControlRegister2Reset;
    std::uint16_t compareMatchTimerStart_ = 0;
    std::uint16_t compareMatchTimerControl_ = 0;
    std::uint32_t compareMatchTimerCounter_ = 0;
    std::uint32_t compareMatchTimerConstant_ = kCompareMatchTimerConstantReset;
    std::uint64_t compareMatchTimerPrescaler_ = 0;
    std::uint16_t portMControl_ = kPortMControlReset;
    std::uint16_t portNControl_ = kPortNControlReset;
    std::uint16_t portPControl_ = kPortPControlReset;
    std::array<std::uint8_t, kPfcStorageSize> pfcStorage_{};
    std::array<std::uint16_t, kPortControlCount> portControl_{};
    std::array<std::uint8_t, kPortDataCount> portData_{};
    std::array<std::uint8_t, kPortPullControlCount> portPullControl_{};
    std::uint8_t portMData_ = 0;
    std::uint8_t portNData_ = 0;
    std::uint8_t portPPullControl_ = 0;
    std::array<std::uint16_t, kDriveControlRegisterCount> driveControl_ =
        kDriveControlReset;
    std::array<std::uint32_t, kBscRegisterCount> bscRegisters_{};
    std::uint32_t busCommonControl_ = kBusCommonControlReset;
    std::uint32_t frequencyControl_ = kFrequencyControlReset;
    std::uint32_t pllControl_ = kPllControlReset;
    std::uint32_t fllFrequency_ = kFllFrequencyReset;
    std::uint32_t clockChangeStatus_ = 0;
    std::uint32_t fsiClockControl_ = 0;
    std::uint32_t ddClockControl_ = 0;
    std::uint32_t usbClockControl_ = 0;
    std::uint32_t pll2Control_ = 0;
    std::uint32_t spuClockControl_ = 0;
    std::uint32_t vpuClockControl_ = 0;
    std::uint32_t spreadSpectrumControl_ = 0;
    std::uint8_t watchdogCounter_ = 0;
    std::uint8_t watchdogControl_ = kWatchdogControlReset;
    cg50::PeripheralBus peripheralBus_{};
    Registers regs_{};
    Stats stats_{};
    State state_ = State::Empty;
    std::wstring diagnostic_ = L"CG50 ROM-native: no firmware loaded";

    bool pendingBranchValid_ = false;
    std::uint32_t pendingBranchTarget_ = 0;
    bool executingDelaySlot_ = false;
    bool rteDelaySlotPrivilegedFetch_ = false;
    bool cpuLockFlag_ = false; // SH-4A MOVLI.L / MOVCO.L atomic reservation flag

    // ---- SH-4A MMU (UTLB) ----
    struct UtlbEntry {
        std::uint32_t virtualPageNumber = 0;  // PTEH bits 28-10
        std::uint32_t physicalPageNumber = 0;  // PTEL bits 28-10
        std::uint32_t asid = 0;                // PTEH bits 7-0
        // 2026-10-29 M1: comentarios del data word SH-4A corregidos
        // (MIT mmu.go:42-53: "PPN 28:10, V 8, SZ1 7, PR 6:5, SZ0 4, C 3,
        // D 2, SH 1, WT 0"). Los "PTEL bits 7-6 / 5-4 / 2 / 1 / 0" previos
        // eran el decode erroneo que el fix M1 corrige en el cpp. SIN
        // campos nuevos (el chunk 61 del save-state queda estable).
        std::uint32_t pageSizeCode = 0;        // SZ = (PTEL bit7 << 1) | PTEL bit4
        std::uint32_t protection = 0;          // PR = PTEL bits 6:5
        bool valid = false;                     // PTEL bit 8 (0x100)
        bool shared = false;                    // PTEL bit 1 (0x2) = SH
        bool dirty = false;                     // PTEL bit 2 (0x4) = D
    };
    std::array<UtlbEntry, kUtlbEntryCount> utlb_{};
    std::array<UtlbEntry, kItlbEntryCount> itlb_{};
    std::uint32_t pteh_ = 0;
    std::uint32_t ptel_ = 0;
    std::uint32_t ptea_ = 0;
    std::uint32_t ttb_ = 0;
    std::uint32_t tea_ = 0;
    std::uint32_t tbr_ = 0;

    // 2026-10-29 M6/N-11: payload residente del staging de add-ins (backing
    // host aparte de la RAM/ROM reales) + interruptor. NO se serializa al
    // save-state: los pases validados no lo cargan y el probe lo repasa
    // por corrida. CG50_ADDIN_STAGING=0 apaga la activacion.
    std::vector<std::uint8_t> addinStaging_;
    bool addinStagingActive_ = false;
    std::vector<std::uint8_t> ilramSeedBytes_;
    // 2026-10-29 (D/N-12): model-strap EXPEVT (ver API publica).
    bool strapExpevtSet_ = false;
    std::uint32_t strapExpevt_ = 0;

    // ---- RTC virtual clock (deterministic, replay-safe, opt-in) ----
    // rtcBaseSeconds_ is the virtual seconds count at instruction
    // rtcAnchorInstruction_. Guest writes to the time registers re-anchor
    // both so the clock keeps counting from the value the OS programs.
    bool rtcVirtualClock_ = false;
    std::uint64_t rtcBaseSeconds_ = 0;
    std::uint64_t rtcAnchorInstruction_ = 0;
    std::uint8_t rtc64HzLastTick_ = 0;
    bool rtcCarryFlag_ = false;
    bool rtcIrqEnabled_ = false;

    // ---- WDT interval-mode shadow ----
    std::uint64_t watchdogPrescaler_ = 0;

    // ---- Pending timer interrupts awaiting an instruction boundary ----
    std::uint32_t pendingTimerIrq_ = 0;  // bit0..2 = TMU0..2 of unit 0

    // ---- KIU scan interrupt awaiting a clean boundary (UNF line) ----
    bool pendingKeyScanIrq_ = false;

    // ---- Host mailbox (F001000C consulta / F0010004 consume) ----
    std::array<std::uint32_t, 16> hostMailbox_{};
    bool hostMailboxInitialized_ = false;
    std::uint32_t scheduledHostEvent_ = 0;
    std::uint64_t scheduledHostEventAt_ = 0;
    std::uint64_t keyHostEventUntil_ = 0;
    std::uint64_t hostHeartbeatNextCycle_ = 0;
    mutable bool keyScanRead_ = false;
    std::uint16_t keyEventPending_ = 0;
    bool PostHostEvent(std::uint32_t event);
    void ScheduleKeyHostEvent();
    // PBDR (0xA405013A) bit 2: senal dinamica del host que SOLO se activa
    // durante la ventana de evaluacion (notify(14,10) 'evaluando' hasta
    // notify(14,11) 'terminada') - el dispatch post-EXE del oficial
    // (INFORME-F1A-CAUSA 19.2/19.4). Con la senal pegada a "buzon con
    // trabajo" (rev anterior) el bit quedaba a 1 durante TODO tecleo y la
    // cadena de pollers salia temprano en cada despacho: el menu dejaba de
    // aceptar teclas y la secuencia aritmetica reiniciaba al guest.
    // CG50_PBDR_HOST=0 la desactiva.
    bool HostHasPendingWork() const noexcept;

    // Ventana de evaluacion (14,10)->(14,11): activa el bit 2 de PBDR.
    bool evalInProgress_ = false;

    // BCD ALU (0xA4CB0000): latch unico compartido de acarreo/prestamo
    // (modelo de hardware casio-cg50 cont.18c/18e).
    std::uint32_t bcdCarryLatch_ = 0;

#ifdef CG50_ENABLE_LEGACY_DEBUG_HLE
    bool resultHlePending_ = false;
    std::uint64_t resultHleSince_ = 0;
    std::uint64_t resultHleSinceCycles_ = 0;
    void CheckResultHlePending();
#endif
    std::uint32_t evalStartInst_ = 0;

    // Gate de retencion del formateador (#395): salta la tarea 800F5592
    // hasta el notify(14,11). VERIFICADO: el salto TERMINA la hebra de la
    // tarea (skip != retener) y el campo queda 0 - DEFAULT OFF
    // (CG50_FORMAT_SKIP=1 solo para reproducir el experimento).
    bool evalDoneNotified_ = false;
    bool evalDoneGateActive_ = false;

    // ---- SH7305 TMU unit 0 register file (dedicated MMIO, ver arriba) ----
    std::array<Tmu0Channel, 3> tmu0_{};

    // ---- Standby (SBYCR) state reached through SLEEP ----
    bool standbyMode_ = false;
    // Cached next instruction boundary for TickRtc() (eliminates 64-bit % 32000 per instruction).
    // Not serialized; recomputed deterministically in Reset() and ImportState().
    std::uint64_t rtcCarryNextInst_ = kRtcInstructionsPerSecond;
    // Start physical address of the most recent full-frame LCD DMAC transfer
    // (0x08000000 primary VRAM or 0x08028800 double buffer). Stored separately
    // because authentic SH7305 DMAC increments SAR to SAR+totalBytes on finish.
    std::uint32_t lastLcdDmaSar_ = 0x08000000u;

    const std::uint8_t* ReadPointer(std::uint32_t address, std::size_t bytes) const;
    std::uint8_t* WritePointer(std::uint32_t address, std::size_t bytes);
    bool ExecuteOpcode(std::uint32_t pc, std::uint16_t opcode);
    bool ExecuteFpuOpcode(std::uint32_t pc, std::uint16_t opcode);
    bool EnterGeneralException(std::uint32_t pc, std::uint32_t eventCode,
                               bool trapa, std::uint8_t trapaImmediate,
                               std::uint32_t vectorOffset = 0x100u);
    void ScheduleBranch(std::uint32_t target) noexcept;
    void HaltUnsupported(std::uint32_t pc, std::uint16_t opcode);
    bool HandleAppIfTrap(std::uint32_t pc);
    void LogStepDiagnostics(std::uint32_t pc);
    bool HaltFetchOutOfMap(std::uint32_t pc);
    bool FailMemory(std::uint32_t pc, std::uint16_t opcode,
                    std::uint32_t address, bool write);
    void TickCompareMatchTimer(std::uint64_t cycles) noexcept;
    void TickTimerUnit(std::uint64_t cycles) noexcept;
    void TickBscRefresh(std::uint64_t cycles) noexcept;
    void TickRtcAlarmsAndPeriodic(std::uint64_t cycles) noexcept;
    void TickWatchdog(std::uint64_t cycles);
    [[nodiscard]] bool HasAnyPendingInterrupt() const noexcept
    {
        return pendingTimerIrq_ != 0u ||
               pendingKeyScanIrq_ ||
               pendingCmtIrq_ ||
               pendingDmacIrq_ != 0u ||
               pendingEtmuIrq_ != 0u ||
               rtcCarryFlag_ ||
               rtcExt_.pendingIrqMask != 0u ||
               intc2_.AnyPending() ||
               scif0_.HasPendingIrq() ||
               usb2_.HasPendingIrq() ||
               adc_.HasPendingIrq();
    }
    void DeliverPendingInterrupts();
    bool TranslateAddress(std::uint32_t address, bool write, bool isFetch,
                          std::uint32_t& physical, std::uint32_t& exceptionCode,
                          std::uint32_t* vectorOffset = nullptr);
    // 2026-10-29 M9: ventana MMIO de los arrays de la UTLB ([0xF6000000,
    // 0xF8000000)). Lectura = readback del entry; escritura = programacion
    // (no-asociativa bit7=0, asociativa bit7=1). PTEA >= 0xF7800000:
    // aceptar y descartar (sin campo backing, declarado).
    std::uint32_t UtlbArrayRead(std::uint32_t address) const;
    void UtlbArrayWrite(std::uint32_t address, std::uint32_t value);
    std::uint32_t ItlbArrayRead(std::uint32_t address) const;
    void ItlbArrayWrite(std::uint32_t address, std::uint32_t value);
    void FlushStoreQueue(std::uint32_t sqAddress);
    void ExecuteDmacChannel(std::size_t channelIndex, std::size_t channelBase, std::uint32_t chcrValue);
    bool IsInterruptMaskedByImr(std::uint32_t eventCode) const noexcept;
    // 2026-10-29: decode comun data word PTEL -> UtlbEntry para el data
    // array MMIO y para el registro PTEL (helper compartido, estatico:
    // no toca miembros).
    static UtlbEntry DecodeUtlbData(std::uint32_t dataWord);
    std::uint64_t RtcVirtualSeconds() const noexcept;
    std::uint8_t RtcDerivedComponent8(std::uint32_t offset) const;
    std::uint16_t RtcDerivedYearBcd() const;
    void RtcReanchorFromComponent(std::uint32_t offset, std::uint8_t value);
    void TickRtc();
    std::uint32_t& FrRef(std::uint32_t index) noexcept;
    const std::uint32_t& FrRef(std::uint32_t index) const noexcept;
    std::uint32_t& XfRef(std::uint32_t index) noexcept;
    const std::uint32_t& XfRef(std::uint32_t index) const noexcept;
    void DrivePortData(cg50::PfcPort port, std::uint8_t value,
                       std::uint8_t implementedMask) noexcept;
    void UpdatePfcDataRegister(std::uint32_t physical, std::uint8_t value) noexcept;
    void UpdateKeyInputData() noexcept;
    void SetT(bool value) noexcept;
    bool T() const noexcept;
    void SetSr(std::uint32_t value) noexcept;
    void InitArea1PeripheralPages() noexcept;
#ifdef CG50_ENABLE_COVERAGE_COUNTERS
    mutable std::unique_ptr<cg50::CoverageCollector> cov_;
#endif
};
