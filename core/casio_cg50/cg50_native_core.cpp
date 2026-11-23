#include "cg50_native_core.h"
#include "cg50_core/sh_address.h"
#include "cg50_core/sh_control.h"
#include "cg50_core/sh_divide.h"
#include "cg50_core/sh_exception.h"
#include "cg50_core/sh_fpu.h"
#include "cg50_core/sh_integer.h"
#include "cg50_core/sh_mac.h"
#include "cg50_core/sh_memory.h"
#include "cg50_core/sh_multiply.h"
#include "cg50_core/sh_stack.h"

#if defined(_WIN32)
#include <intrin.h>
#endif

#if !defined(_WIN32)
#define strtok_s strtok_r
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <unordered_map>

static inline FILE* Cg50DiagFopen(const char* path, const char* mode) noexcept
{
#ifdef CG50_ENABLE_DIAG_LOG
    return std::fopen(path, mode);
#else
    (void)path;
    (void)mode;
    return nullptr;
#endif
}

namespace {

std::int32_t SignExtend8(const std::uint32_t value)
{
    return static_cast<std::int32_t>(static_cast<std::int8_t>(value & 0xFFu));
}

std::int32_t SignExtend12(const std::uint32_t value)
{
    std::int32_t result = static_cast<std::int32_t>(value & 0x0FFFu);
    if ((result & 0x0800) != 0) result |= ~0x0FFF;
    return result;
}

std::uint32_t RegN(const std::uint16_t opcode) { return (opcode >> 8) & 0x0Fu; }
std::uint32_t RegM(const std::uint16_t opcode) { return (opcode >> 4) & 0x0Fu; }

bool StartsWithDlsimMem(const std::uint8_t* bytes, const std::size_t size)
{
    static constexpr char kMagicUpper[] = "DLSimMEM";
    static constexpr char kMagicMixed[] = "DLSimMem";
    if (bytes == nullptr) return false;
    if (size >= sizeof(kMagicUpper) - 1 &&
        std::memcmp(bytes, kMagicUpper, sizeof(kMagicUpper) - 1) == 0) return true;
    return size >= sizeof(kMagicMixed) - 1 &&
           std::memcmp(bytes, kMagicMixed, sizeof(kMagicMixed) - 1) == 0;
}

std::uint32_t AddSignedOffset(const std::uint32_t base, const std::int32_t offset)
{
    return base + static_cast<std::uint32_t>(offset);
}

bool InterruptPriorityRegisterIndex(const std::uint32_t physicalAddress,
                                    std::size_t& index) noexcept
{
    if (physicalAddress < Cg50NativeCore::kInterruptPriorityRegisterPhysicalBase)
        return false;
    const auto offset = physicalAddress -
        Cg50NativeCore::kInterruptPriorityRegisterPhysicalBase;
    if ((offset % Cg50NativeCore::kInterruptPriorityRegisterStride) != 0)
        return false;
    index = static_cast<std::size_t>(
        offset / Cg50NativeCore::kInterruptPriorityRegisterStride);
    return index < Cg50NativeCore::kInterruptPriorityRegisterCount;
}

bool InterruptMaskRegisterIndex(const std::uint32_t physicalAddress,
                                std::size_t& index) noexcept
{
    if (physicalAddress < Cg50NativeCore::kInterruptMaskRegisterPhysicalBase)
        return false;
    const auto offset = physicalAddress -
        Cg50NativeCore::kInterruptMaskRegisterPhysicalBase;
    if ((offset % 4u) != 0)
        return false;
    index = static_cast<std::size_t>(offset / 4u);
    return index < Cg50NativeCore::kInterruptMaskRegisterCount;
}

bool InterruptMaskClearRegisterIndex(const std::uint32_t physicalAddress,
                                     std::size_t& index) noexcept
{
    if (physicalAddress < Cg50NativeCore::kInterruptMaskClearRegisterPhysicalBase)
        return false;
    const auto offset = physicalAddress -
        Cg50NativeCore::kInterruptMaskClearRegisterPhysicalBase;
    if ((offset % 4u) != 0)
        return false;
    index = static_cast<std::size_t>(offset / 4u);
    return index < Cg50NativeCore::kInterruptMaskRegisterCount;
}

bool KeyInterfaceRegisterIndex(const std::uint32_t physicalAddress,
                               std::size_t& index) noexcept
{
    if (physicalAddress < Cg50NativeCore::kKeyInterfaceRegisterPhysicalBase)
        return false;
    const auto offset = physicalAddress -
        Cg50NativeCore::kKeyInterfaceRegisterPhysicalBase;
    if ((offset & 1u) != 0) return false;
    index = static_cast<std::size_t>(offset / 2u);
    return index < Cg50NativeCore::kKeyInterfaceRegisterCount;
}

bool DriveControlRegisterIndex(const std::uint32_t physicalAddress,
                               std::size_t& index) noexcept
{
    if (physicalAddress < Cg50NativeCore::kDriveControlRegisterPhysicalBase)
        return false;
    const auto offset = physicalAddress -
        Cg50NativeCore::kDriveControlRegisterPhysicalBase;
    if ((offset & 1u) != 0) return false;
    index = static_cast<std::size_t>(offset / 2u);
    return index < Cg50NativeCore::kDriveControlRegisterCount;
}

const bool kClearFfEnabled = std::getenv("CG50_CLEAR_FF") != nullptr;
const bool kTickDbgEnabled = std::getenv("CG50_TICKDBG") != nullptr;
const bool kIlramLogEnabled = std::getenv("CG50_ILRAM_LOG") != nullptr;
const bool kStoreLogEnabled = std::getenv("CG50_STORE_LOG") != nullptr;
const bool kFlashNoSwallow = std::getenv("CG50_FLASH_NO_SWALLOW") != nullptr;
const bool kDiagFileEnabled = std::getenv("CG50_DIAG_FILE") != nullptr;
const char* const kBuf9cSeedPath = std::getenv("CG50_BUF9C_FILE");

// In-Memory Examination Mode Decoupling (Seat #461 / fsP3b3):
// Secondary guard alongside normal-mode flash provisioning (0x9505) so older
// saved states or snapshots with 0x95FF also open restricted apps/add-ins
// cleanly without touching SystemROM.raw or failing boot anti-tamper sweeps.
const bool kExamDecouplingEnabled = [] {
    const char* v = std::getenv("CG50_EXAM_DECOUPLING");
    if (v != nullptr && std::strcmp(v, "0") == 0) return false;
    return true;
}();

} // namespace

static_assert(Cg50NativeCore::kLcdBase == cg50::kLcdBusAddress,
              "CG50 core and board wiring must agree on LCD base");

Cg50NativeCore::Cg50NativeCore()
    : ram_(kRamSize, 0),
      ramC_(kRamCSize, 0),
      dsp0Space_(kDsp0SpaceSize, 0),
      dsp1Space_(kDsp1SpaceSize, 0)
{
    InitArea1PeripheralPages();
    flashCfi_.Reset();
    cpuLockFlag_ = false;
    peripheralBus_.Reset();
}

namespace {

struct MmioAuditEntry {
    std::uint64_t count = 0;
    std::uint32_t firstPc = 0;
    std::uint32_t width = 0;
};

struct MmioAuditTable {
    std::unordered_map<std::uint64_t, MmioAuditEntry> entries;
    bool registered = false;

    static MmioAuditTable& Instance()
    {
        static MmioAuditTable inst;
        return inst;
    }

    static void DumpAtExit()
    {
        auto& inst = Instance();
        if (inst.entries.empty()) return;
        std::vector<std::pair<std::uint64_t, MmioAuditEntry>> sorted(
            inst.entries.begin(), inst.entries.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        std::fprintf(stderr, "\n=== CG50_MMIO_AUDIT FALLBACK SUMMARY (%zu unique) ===\n",
                     sorted.size());
        for (const auto& kv : sorted) {
            const std::uint32_t phys = static_cast<std::uint32_t>(kv.first & 0xFFFFFFFFu);
            const bool isWrite = ((kv.first >> 32) & 1u) != 0;
            const std::uint32_t width = static_cast<std::uint32_t>((kv.first >> 33) & 0xFu);
            std::fprintf(stderr, "[MMIO-AUDIT] %s%u phys=0x%08X count=%llu first_pc=0x%08X\n",
                         isWrite ? "WR" : "RD", width * 8u, phys,
                         static_cast<unsigned long long>(kv.second.count),
                         kv.second.firstPc);
        }
        std::fflush(stderr);
    }

    static void Record(const bool isWrite, const std::uint32_t phys,
                       const std::size_t bytes, const std::uint32_t pc)
    {
        auto& inst = Instance();
        if (!inst.registered) {
            inst.registered = true;
            std::atexit(&DumpAtExit);
        }
        const std::uint64_t key =
            static_cast<std::uint64_t>(phys) |
            (isWrite ? (1ull << 32) : 0ull) |
            (static_cast<std::uint64_t>(bytes & 0xFu) << 33);
        auto& e = inst.entries[key];
        if (e.count == 0) {
            e.firstPc = pc;
            e.width = static_cast<std::uint32_t>(bytes);
        }
        ++e.count;
    }
};

const bool kMmioAuditEnabled = std::getenv("CG50_MMIO_AUDIT") != nullptr;

} // namespace

void Cg50NativeCore::InitArea1PeripheralPages() noexcept
{
    area1PageTable_.fill(nullptr);
    static constexpr std::uint32_t kPageBases[kArea1PeripheralPageCount] = {
        0x04050000u, 0x04080000u, 0x0413F000u, 0x04140000u, 0x04150000u,
        0x04410000u, 0x04470000u, 0x04490000u, 0x044A0000u, 0x044B0000u,
        0x044C0000u, 0x044D0000u, 0x04520000u, 0x04530000u, 0x04610000u,
        0x04700000u, 0x04C40000u, 0x04C50000u, 0x04CA0000u, 0x04CB0000u,
        0x04CC0000u, 0x04CC1000u, 0x04CD0000u, 0x04CF0000u, 0x04D80000u
    };
    for (std::size_t i = 0; i < kArea1PeripheralPageCount; ++i) {
        const std::size_t pIndex = (kPageBases[i] - kArea1BasePhysical) >> 12;
        if (pIndex < area1PageTable_.size()) {
            area1PageTable_[pIndex] = area1PeripheralPages_[i].data();
            if (pIndex + 1 < area1PageTable_.size()) {
                area1PageTable_[pIndex + 1] = area1PeripheralPages_[i].data() + 4096;
            }
        }
    }
}

std::uint32_t Cg50NativeCore::DirectMappedPhysicalAddress(
    const std::uint32_t address) noexcept
{
    if (address >= 0x80000000u && address < 0xC0000000u)
        return address & 0x1FFFFFFFu;
    return address;
}

bool Cg50NativeCore::LoadFirmware(const std::uint8_t* bytes, const std::size_t size)
{
    if (bytes == nullptr || size == 0) {
        diagnostic_ = L"CG50 ROM-native: firmware ausente";
        return false;
    }
    if (StartsWithDlsimMem(bytes, size)) {
        diagnostic_ = L"CG50 ROM-native: DLSimMEM/DLSimMem no es una ROM ejecutable descifrada";
        return false;
    }
    if (size == kRomSize)
        return LoadFirmwareParts(bytes, kRomSize, nullptr, 0);
    if (size == kCombinedRomSize)
        return LoadFirmwareParts(bytes, kRomSize, bytes + kRomSize, kUserRomSize);

    std::wstringstream ss;
    ss << L"CG50 ROM-native: tamaÃƒÆ’Ã‚Â±o no reconocido (" << size
       << L" bytes; se esperan 0x00B60000 o 0x02000000)";
    diagnostic_ = ss.str();
    return false;
}

bool Cg50NativeCore::LoadC2Rom(const std::uint8_t* bytes, const std::size_t size)
{
    // C2ROM: segunda imagen flash (ventana fisica [0x00B60000, 0x01000000)),
    // reconstruida del GUEST_DUMP del oficial (2026-10-18). El OS lee ahi los
    // bloques del escaneo FS (IDs -277, -6, 4, ...), distintos del storage.
    if (bytes == nullptr || size != kC2RomSize) {
        diagnostic_ = L"CG50 ROM-native: C2ROM con tamano incorrecto (se esperan 0x4A0000)";
        return false;
    }
    c2Rom_.assign(bytes, bytes + size);
    return true;
}

bool Cg50NativeCore::SetIlramSeed(const std::uint8_t* bytes, const std::size_t size)
{
    if (bytes == nullptr || size < 2 || size > onChipRam_.size()) {
        return false;
    }
    ilramSeedBytes_.assign(bytes, bytes + size);
    if (HasUserRom() && stats_.instructions == 0) {
        const std::size_t got = std::min(ilramSeedBytes_.size(), onChipRam_.size());
        std::memcpy(onChipRam_.data(), ilramSeedBytes_.data(), got);
        for (std::size_t i = 0; i + 1 < got; i += 2) {
            std::swap(onChipRam_[i], onChipRam_[i + 1]);
        }
    }
    return true;
}

void Cg50NativeCore::Reset()
{
    // Fold the virtual RTC seconds elapsed so far into the base so a CPU
    // reset does not rewind the emulated clock (the RTC is battery-backed
    // on real hardware and keeps counting).
    rtcBaseSeconds_ += RtcVirtualSeconds();
    regs_ = Registers{};
    regs_.pc = kRomBase;
    regs_.sr = kResetSr;
    regs_.r[15] = kRamBase + static_cast<std::uint32_t>(kRamSize) - 4u;
    // 2026-10-29 (D/N-12): reaplicar el model-strap EXPEVT tras el wipe de
    // regs_ (el strap se fija antes de LoadFirmware/probe; ver API publica).
    if (strapExpevtSet_) {
        regs_.expevt = strapExpevt_;
    }
    // La RAM del emulador oficial arranca en 0xFF (verificado contra GUEST_DUMP
    // del oficial, 2026-10-18: las tablas FS recien asignadas quedan FF). Con
    // 0x00 el byte-scan del montaje FS (0x8038F84A) fallaba con -1.
    std::fill(ram_.begin(), ram_.end(), std::uint8_t{0xFF});
    std::fill(onChipRam_.begin(), onChipRam_.end(), std::uint8_t{0});
    if (HasUserRom()) {
        // CG50_CASIOWIN_SEED=1: hack historico que sembraba "CASIOWIN" en
        // ILRAM 0xFD801320 al Reset. El OS compara ese valor contra la ROM
        // (0x750C5C = "CASIOWIN") para decidir si el FS ya esta inicializado
        // (0x80384DD2: si iguales, SALTA la init 0x80215572 -> ILRAM 0x17B4
        // queda en 0 y el montaje FS crashea con ADDRESS(R) -> "System
        // ERROR"). Con la init corriendo de verdad, el seed sobra: por
        // defecto NO se siembra (como el arranque limpio del oficial).
        static const bool casiowinSeed = std::getenv("CG50_CASIOWIN_SEED") != nullptr;
        if (casiowinSeed) {
            std::memcpy(&onChipRam_[0x1320], "CASIOWIN", 8);
        }
        // CG50_WARM_FLAG=<0-255> (entorno) permite experimentar el estado de
        // arranque calido/frio: la rutina de decision del OS lee este byte en
        // *(0xFD8016AB) y ramifica con {1,2,6}. Por defecto se conserva 1
        // (bypass validado del baseline).
        static const long warmFlagOverride = [] {
            const char* v = std::getenv("CG50_WARM_FLAG");
            return v != nullptr ? std::strtol(v, nullptr, 0) : -1L;
        }();
        onChipRam_[0x16AB] = (warmFlagOverride >= 0 && warmFlagOverride <= 0xFF)
            ? static_cast<std::uint8_t>(warmFlagOverride)
            : static_cast<std::uint8_t>(0x01);
        onChipRam_[0x1D40] = 0x00;
        onChipRam_[0x1D41] = 0x00;
        onChipRam_[0x1D42] = 0x00;
        onChipRam_[0x1D43] = 0x01;
        // CG50_ILRAM_FILE=<ruta> (F1-A, 2026-10-21): siembra el ILRAM al Reset
        // con el volcado del oficial (formato method5: palabras de 16 bits
        // little-endian -> swap de pares). El residente del nativo queda
        // parcialmente vacio porque la carga de tareas es host-driven en la
        // ROM del emulador; sin host, el interprete (0xFD801200+) nunca se
        // copia. La siembra replica el estado post-tareas del oficial.
        static const char* ilramSeed = std::getenv("CG50_ILRAM_FILE");
        if (!ilramSeedBytes_.empty()) {
            const std::size_t got = std::min(ilramSeedBytes_.size(), onChipRam_.size());
            std::memcpy(onChipRam_.data(), ilramSeedBytes_.data(), got);
            for (std::size_t i = 0; i + 1 < got; i += 2) {
                std::swap(onChipRam_[i], onChipRam_[i + 1]);
            }
        } else if (ilramSeed != nullptr) {
            if (FILE* fp = std::fopen(ilramSeed, "rb")) {
                std::size_t got = std::fread(onChipRam_.data(), 1,
                                              onChipRam_.size(), fp);
                std::fclose(fp);
                if (got >= 2) {
                    // swap de pares: el volcado oficial (method5) guarda cada
                    // palabra de 16 bits little-endian; el guest es big-endian.
                    for (std::size_t i = 0; i + 1 < got; i += 2)
                        std::swap(onChipRam_[i], onChipRam_[i + 1]);
                }
                std::printf("[ILRAM-SEED] sembrado %zu bytes desde %s\n",
                            got, ilramSeed);
            } else {
                std::printf("[ILRAM-SEED] no se pudo abrir %s\n", ilramSeed);
            }
        }
        if (ram_.size() > 0x92D34) {
            ram_[0x92D2C] = 0x02;
            ram_[0x92D2D] = 0x80;
            ram_[0x92D30] = 0x00;
            ram_[0x92D31] = 0x01;
        }
        // CG50_HOST_SEED=1: replica el estado RAM que el entorno host oficial
        // presenta tras su arranque (comparado contra GUEST_DUMP del oficial,
        // 2026-10-18): flags de boot y structs observados a t=1ms.
        static const bool hostSeed = std::getenv("CG50_HOST_SEED") != nullptr;
        if (hostSeed) {
            ram_[0xA4412] = 0x10;
            ram_[0xA4415] = 0x81;
            ram_[0x90C28] = 0x01;
            ram_[0x90C29] = 0x01;
            ram_[0x90C2A] = 0x00;
            ram_[0x90C2B] = 0x03;
            ram_[0xA4269] = 0x03;
            ram_[0xA426D] = 0x01;
        }
        // CG50_BUF9C_FILE=<ruta>: siembra el buffer FS 0x8809C000-0x8809C800
        // (0x800 bytes) con el contenido observado del oficial (experimento N-8).
        static const char* buf9cSeed = std::getenv("CG50_BUF9C_FILE");
        if (buf9cSeed != nullptr) {
            if (FILE* fp = std::fopen(buf9cSeed, "rb")) {
                (void)std::fread(&ram_[0x9C000], 1, 0x800, fp);
                std::fclose(fp);
            }
        }
    }
    std::fill(dsp0Space_.begin(), dsp0Space_.end(), std::uint8_t{0});
    ilram_.fill(0u);
    xram_.fill(0u);
    yram_.fill(0u);
    storeQueues_.fill(0u);
    qacr0_ = 0u;
    qacr1_ = 0u;
    itlb_.fill(UtlbEntry{});
    intc2_.Reset();
    etmu_.Reset();
    adc_.Reset();
    tpu_.Reset();
    scif0_.Reset();
    usb2_.Reset();
    pendingEtmuIrq_ = 0u;
    pendingDmacIrq_ = 0u;
    lastLcdDmaSar_ = 0x08000000u;
    std::fill(dsp1Space_.begin(), dsp1Space_.end(), std::uint8_t{0});
    std::fill(ilcPage_.begin(), ilcPage_.end(), std::uint8_t{0});
    std::fill(dmacPage_.begin(), dmacPage_.end(), std::uint8_t{0});
    std::fill(dmarsPage_.begin(), dmarsPage_.end(), std::uint8_t{0});
    std::fill(ubcPage_.begin(), ubcPage_.end(), std::uint8_t{0});
    std::fill(priorityLockPage_.begin(), priorityLockPage_.end(), std::uint8_t{0});
    for (auto& page : area1PeripheralPages_) {
        std::fill(page.begin(), page.end(), std::uint8_t{0});
    }
    InitArea1PeripheralPages();
    flashCfi_.Reset();
    cpuLockFlag_ = false;
    utlb_.fill(UtlbEntry{});
    pteh_ = 0;
    ptel_ = 0;
    ptea_ = 0;
    ttb_ = 0;
    tea_ = 0;
    tbr_ = 0;
    rtcAnchorInstruction_ = 0;
    rtc64HzLastTick_ = 0;
    rtcCarryFlag_ = false;
    watchdogPrescaler_ = 0;
    pendingTimerIrq_ = 0;
    standbyMode_ = false;
    std::fill(keyState_.begin(), keyState_.end(), std::uint8_t{0});
    std::fill(alternateBank_.begin(), alternateBank_.end(), std::uint32_t{0});
    cacheControl_ = 0;
    mmuControl_ = 0;
    cpuopm_ = kCpuopmReset;
    expmask_ = 0;
    interruptMask5_ = 0;
    std::fill(interruptMask_.begin(), interruptMask_.end(), std::uint8_t{0});
    std::fill(interruptPriority_.begin(), interruptPriority_.end(), std::uint16_t{0});
    std::fill(keyInputData_.begin(), keyInputData_.end(), std::uint16_t{0});
    std::fill(keyInterfaceControl_.begin(), keyInterfaceControl_.end(), std::uint16_t{0});
    moduleStopControl0_ = kModuleStopControlRegister0Reset;
    standbyControl_ = 0;
    moduleStopControl2_ = kModuleStopControlRegister2Reset;
    compareMatchTimerStart_ = 0;
    compareMatchTimerControl_ = 0;
    compareMatchTimerCounter_ = 0;
    compareMatchTimerConstant_ = kCompareMatchTimerConstantReset;
    compareMatchTimerPrescaler_ = 0;
    portMControl_ = kPortMControlReset;
    portNControl_ = kPortNControlReset;
    portPControl_ = kPortPControlReset;
    std::fill(pfcStorage_.begin(), pfcStorage_.end(), std::uint8_t{0});
    for (std::size_t i = 0; i < 0x20; i += 2) {
        pfcStorage_[i] = 0xAAu;
        pfcStorage_[i + 1] = 0xAAu;
    }
    pfcStorage_[0x16] = 0x0Au; pfcStorage_[0x17] = 0xAAu;
    pfcStorage_[0x18] = 0x0Au; pfcStorage_[0x19] = 0xAAu;
    pfcStorage_[0x40] = 0xAAu; pfcStorage_[0x41] = 0xAAu;
    pfcStorage_[0x42] = 0xAAu; pfcStorage_[0x43] = 0xAAu;
    pfcStorage_[0x44] = 0xAAu; pfcStorage_[0x45] = 0xAAu;
    pfcStorage_[0x4C] = 0xAAu; pfcStorage_[0x4D] = 0xAAu;
    pfcStorage_[0x84] = 0xFFu; pfcStorage_[0x85] = 0xFCu;
    pfcStorage_[0x86] = 0xFFu; pfcStorage_[0x87] = 0xFFu;
    pfcStorage_[0x88] = 0xFFu; pfcStorage_[0x89] = 0xFFu;
    pfcStorage_[0x8A] = 0xFFu; pfcStorage_[0x8B] = 0x3Fu;
    std::fill(portControl_.begin(), portControl_.end(), std::uint16_t{0xAAAAu});
    portControl_[11] = kPortMControlReset;
    portControl_[12] = kPortNControlReset;
    std::fill(portData_.begin(), portData_.end(), std::uint8_t{0});
    std::fill(portPullControl_.begin(), portPullControl_.end(), std::uint8_t{0});
    portMData_ = 0;
    portNData_ = 0;
    portPPullControl_ = 0;
    driveControl_ = kDriveControlReset;
    std::fill(bscRegisters_.begin(), bscRegisters_.end(), std::uint32_t{0});
    bscRegisters_[0] = kBusCommonControlReset;
    busCommonControl_ = kBusCommonControlReset;
    frequencyControl_ = kFrequencyControlReset;
    pllControl_ = kPllControlReset;
    fllFrequency_ = kFllFrequencyReset;
    clockChangeStatus_ = 0;
    fsiClockControl_ = 0;
    ddClockControl_ = 0;
    usbClockControl_ = 0;
    pll2Control_ = 0;
    spuClockControl_ = 0;
    vpuClockControl_ = 0;
    spreadSpectrumControl_ = 0;
    watchdogCounter_ = 0;
    watchdogControl_ = kWatchdogControlReset;
    peripheralBus_.Reset();
    flashCfi_.Reset();
    stats_ = Stats{};
    pendingBranchValid_ = false;
    pendingBranchTarget_ = 0;
    executingDelaySlot_ = false;
    rteDelaySlotPrivilegedFetch_ = false;
    cpuLockFlag_ = false;
    state_ = rom_.empty() ? State::Empty : State::Running;
    if (!rom_.empty()) diagnostic_ = L"CG50 ROM-native: reset SH7305 en P2 0xA0000000";
}

namespace {
// La NOR de usuario del CPU73050 oficial se expone en DOS alias (verificado
// contra el volcado GUEST_DUMP del emulador oficial, 2026-10-18):
//   [0x00B60000, 0x01000000)          -> storage[0 .. 0x4A0000)
//   [0x01000000, 0x01000000 + size)   -> storage[0 .. size)
// El alias B arranca en storage[0] (no en storage+0x4A0000): el chequeo de
// FS del OS lee el superbloque en 0xA1000000.
bool UserRomStorageOffset(const std::uint32_t physical, const std::size_t bytes,
                          const std::size_t userRomSize,
                          std::size_t& offset) noexcept
{
    if (physical >= 0x00B60000u && physical < 0x01000000u) {
        offset = static_cast<std::size_t>(physical - 0x00B60000u);
    } else if (physical >= 0x01000000u &&
               physical < 0x01000000u + userRomSize) {
        offset = static_cast<std::size_t>(physical - 0x01000000u);
    } else {
        return false;
    }
    return offset + bytes <= userRomSize;
}
} // namespace

namespace {
// CG50_PERIPH_LOG=1: habilita TODO el diagnostico del hot-path del core
// (watches de accesos perifericos, teclado/KIU, LCD, cola de eventos, DMA,
// CFI, excepciones y triggers por PC). Default OFF: silencio total del
// diagnostico para recuperar MIPS. Con la env en 1 la salida es IDENTICA a
// la construccion previa (stdout + key_diag.log/lcd_diag.log/queue_diag.log
// bajo CG50_DIAG_FILE). Es el UNICO interruptor de estos watches; los
// experimentos con env propia (CG50_RESULT_WATCH, CG50_STORE_LOG,
// CG50_TRAP_LOG, CG50_STRLOC_LOG, CG50_APPIF_LOG, CG50_BUFWR_LOG,
// CG50_LCD_CMD_LOG) mantienen su contrato independiente.
// F1-C v2 (FR10): flag promovido a file-scope (antes static local). Misma
// semantica; quita el guard del static local del camino por acceso.
const bool g_periphDiagEnabled = std::getenv("CG50_PERIPH_LOG") != nullptr;
bool PeriphDiagEnabled()
{
    return g_periphDiagEnabled;
}

// Ventana 1 ([0x00B60000, 0x01000000)) -> imagen C2 (segunda flash) cuando
// esta cargada; sin C2 se conserva el mapeo legado hacia el storage.
bool C2StorageOffset(const std::uint32_t physical, const std::size_t bytes,
                     const std::size_t c2Size, std::size_t& offset) noexcept
{
    if (physical >= 0x00B60000u && physical < 0x01000000u) {
        offset = static_cast<std::size_t>(physical - 0x00B60000u);
        return offset + bytes <= c2Size;
    }
    return false;
}
} // namespace

bool Cg50NativeCore::IsFlashPhysicalAddress(const std::uint32_t physical) const noexcept
{
    return physical < kUserRomAliasBPhysicalBase + kUserRomSize;
}

void Cg50NativeCore::ProgramFlashWord16(const std::uint32_t physical,
                                        const std::uint16_t value) noexcept
{
    const std::uint8_t hi = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    const std::uint8_t lo = static_cast<std::uint8_t>(value & 0xFFu);
    std::size_t off = 0;
    if (!c2Rom_.empty() && C2StorageOffset(physical, 2u, c2Rom_.size(), off)) {
        c2Rom_[off] &= hi;
        c2Rom_[off + 1u] &= lo;
        return;
    }
    if (!userRom_.empty() && UserRomStorageOffset(physical, 2u, userRom_.size(), off)) {
        userRom_[off] &= hi;
        userRom_[off + 1u] &= lo;
        return;
    }
    if (physical >= kSystemParameterPhysicalBase &&
        physical + 1u < kRomPhysicalBase + rom_.size()) {
        const std::size_t romOff = static_cast<std::size_t>(physical - kRomPhysicalBase);
        rom_[romOff] &= hi;
        rom_[romOff + 1u] &= lo;
    }
}

void Cg50NativeCore::EraseFlashSector(const std::uint32_t sectorBasePhysical) noexcept
{
    constexpr std::size_t kSec = cg50::SpansionNorFlashCfi::kSectorSize;
    const std::uint32_t base = sectorBasePhysical & cg50::SpansionNorFlashCfi::kSectorMask;
    std::size_t off = 0;
    if (!c2Rom_.empty() && C2StorageOffset(base, kSec, c2Rom_.size(), off)) {
        std::fill(c2Rom_.begin() + static_cast<std::ptrdiff_t>(off),
                  c2Rom_.begin() + static_cast<std::ptrdiff_t>(off + kSec),
                  std::uint8_t{0xFF});
        return;
    }
    if (!userRom_.empty() && UserRomStorageOffset(base, kSec, userRom_.size(), off)) {
        std::fill(userRom_.begin() + static_cast<std::ptrdiff_t>(off),
                  userRom_.begin() + static_cast<std::ptrdiff_t>(off + kSec),
                  std::uint8_t{0xFF});
        return;
    }
    if (base >= kSystemParameterPhysicalBase &&
        static_cast<std::uint64_t>(base) + kSec <= kRomPhysicalBase + rom_.size()) {
        const std::size_t romOff = static_cast<std::size_t>(base - kRomPhysicalBase);
        std::fill(rom_.begin() + static_cast<std::ptrdiff_t>(romOff),
                  rom_.begin() + static_cast<std::ptrdiff_t>(romOff + kSec),
                  std::uint8_t{0xFF});
    }
}

// 2026-10-29 M6/N-11: staging de add-ins (INFORME-G3A 5.3 escenario 1,
// recomendado). El payload del .g3a (segmento PT_LOAD en offset 0x7000 del
// archivo, lo carga el probe) se aloja en un backing host aparte. La ventana
// [0x08080000, 0x08080000+size) sombreara exactamente ese payload en
// ReadPointer; WritePointer devuelve nullptr (escrituras al staging no
// soportadas esta onda, declarado: fail-clean). Nota de colision
// documentada: 0x08080000 cae dentro de la RAM del core
// [0x08000000, 0x08800000); el uso planeado es el mapeo VA 0x00300000 ->
// PA 0x08080000 con el UTLB del maestro. Sin payload el core queda
// byte-identico. El staging NO se serializa al save-state.
bool Cg50NativeCore::SetAddinStaging(
    const std::uint8_t* bytes, const std::size_t size)
{
    addinStagingActive_ = false;
    addinStaging_.clear();
    if (bytes == nullptr || size == 0) {
        diagnostic_ = L"CG50 ROM-native: staging de add-in vacio";
        return false;
    }
    if (size > kAddinStagingSlotSize) {
        diagnostic_ = L"CG50 ROM-native: staging de add-in supera 0x200000";
        return false;
    }
    addinStaging_.assign(bytes, bytes + size);
    addinStagingActive_ = true;
    return true;
}

bool Cg50NativeCore::ValidateG3aHeader(const std::uint8_t* bytes,
                                       const std::size_t size) noexcept
{
    constexpr std::size_t kG3aHeaderSize = 0x7000u;
    if (bytes == nullptr || size <= kG3aHeaderSize ||
        size > kG3aHeaderSize + kAddinStagingSlotSize) {
        return false;
    }
    static constexpr std::uint8_t kExpectedMagic[8] = {
        0xAAu, 0xACu, 0xBDu, 0xAFu, 0x90u, 0x88u, 0x9Au, 0x8Du
    };
    return std::memcmp(bytes, kExpectedMagic, sizeof(kExpectedMagic)) == 0;
}

bool Cg50NativeCore::LoadG3aAddin(const std::uint8_t* bytes,
                                  const std::size_t size)
{
    constexpr std::size_t kG3aHeaderSize = 0x7000u;
    if (!ValidateG3aHeader(bytes, size)) {
        diagnostic_ = L"CG50 ROM-native: archivo .g3a invalido (cabecera o tamano)";
        return false;
    }
    return SetAddinStaging(bytes + kG3aHeaderSize, size - kG3aHeaderSize);
}

bool Cg50NativeCore::ReplaceUserRom(const std::uint8_t* bytes,
                                    const std::size_t size)
{
    if (bytes == nullptr || size != kUserRomSize) {
        diagnostic_ = L"CG50 ROM-native: UserROM debe medir exactamente 0x014A0000 bytes";
        return false;
    }
    if (std::memcmp(bytes, "DLSimMEM", 8) == 0 ||
        std::memcmp(bytes, "DLSimMem", 8) == 0) {
        diagnostic_ = L"CG50 ROM-native: contenedor DLSimMEM no valido como UserROM RAW";
        return false;
    }
    userRom_.assign(bytes, bytes + size);
    flashCfi_.Reset();
    if (!rom_.empty()) {
        Reset();
    }
    return true;
}

void Cg50NativeCore::SetRtcEpochSeconds(const std::int64_t localUnixSeconds) noexcept
{
    constexpr std::int64_t kUnixEpoch2000 = 946684800LL;
    rtcVirtualClock_ = true;
    rtcBaseSeconds_ = localUnixSeconds >= kUnixEpoch2000
        ? static_cast<std::uint64_t>(localUnixSeconds - kUnixEpoch2000)
        : 0ull;
    rtcAnchorInstruction_ = stats_.instructions;
}

const std::uint8_t* Cg50NativeCore::ReadPointer(
    const std::uint32_t address, const std::size_t bytes) const
{
    if (bytes == 0) return nullptr;
    const std::uint32_t physical = DirectMappedPhysicalAddress(address);
    const std::uint64_t end = static_cast<std::uint64_t>(physical) + bytes;

    // 2026-10-29 M6/N-11: ventana de staging de add-ins. Solo si la lectura
    // entera cae dentro del payload; si no, fall-through al backing fisico
    // normal (la ventana pisaria fisicas de RAM/ROM: el payload manda).
    if (addinStagingActive_ &&
        physical >= kAddinStagingPhysicalBase &&
        end <= static_cast<std::uint64_t>(kAddinStagingPhysicalBase) +
                   addinStaging_.size()) {
        return addinStaging_.data() +
               static_cast<std::size_t>(physical - kAddinStagingPhysicalBase);
    }

    const std::uint64_t romEnd = static_cast<std::uint64_t>(kRomPhysicalBase) + rom_.size();
    if (physical >= kRomPhysicalBase && end <= romEnd)
        return rom_.data() + static_cast<std::size_t>(physical - kRomPhysicalBase);

    std::size_t c2Offset = 0;
    if (!c2Rom_.empty() && C2StorageOffset(physical, bytes, c2Rom_.size(), c2Offset))
        return c2Rom_.data() + c2Offset;

    std::size_t userOffset = 0;
    if (!userRom_.empty() &&
        UserRomStorageOffset(physical, bytes, userRom_.size(), userOffset)) {
        // CG50_STORE_LOG=1: registra lecturas del storage (N-8, microscopio).
        static const bool storeLog = std::getenv("CG50_STORE_LOG") != nullptr;
        if (storeLog) {
            std::fprintf(stderr, "[STORERD] phys=%08X off=%06zX bytes=%zu pc=%08X\n",
                         physical, userOffset, bytes, regs_.pc);
        }
        return userRom_.data() + userOffset;
    }

    const std::uint64_t ramEnd = static_cast<std::uint64_t>(kRamBase) + ram_.size();
    if (physical >= kRamBase && end <= ramEnd)
        return ram_.data() + static_cast<std::size_t>(physical - kRamBase);

    // Banco C (0x0C000000/0x8C000000): respaldo propio, no alias del banco 8
    // (oraculo oficial 2026-10-26: ramc-res0ekey-preexe.bin 100% ceros).
    const std::uint64_t ramCEnd =
        static_cast<std::uint64_t>(kRamAliasBase) + ramC_.size();
    if (physical >= kRamAliasBase && end <= ramCEnd)
        return ramC_.data() + static_cast<std::size_t>(physical - kRamAliasBase);

    const std::uint64_t onChipRamEnd =
        static_cast<std::uint64_t>(kOnChipRamBase) + onChipRam_.size();
    if (address >= kOnChipRamBase &&
        static_cast<std::uint64_t>(address) + bytes <= onChipRamEnd)
        return onChipRam_.data() + static_cast<std::size_t>(address - kOnChipRamBase);

    if (address >= kIlramBase &&
        static_cast<std::uint64_t>(address) + bytes <=
            static_cast<std::uint64_t>(kIlramBase) + ilram_.size())
        return ilram_.data() + static_cast<std::size_t>(address - kIlramBase);
    if (physical >= kIlramPhysicalAlias &&
        end <= static_cast<std::uint64_t>(kIlramPhysicalAlias) + ilram_.size())
        return ilram_.data() + static_cast<std::size_t>(physical - kIlramPhysicalAlias);

    if (address >= kXramBase &&
        static_cast<std::uint64_t>(address) + bytes <=
            static_cast<std::uint64_t>(kXramBase) + xram_.size())
        return xram_.data() + static_cast<std::size_t>(address - kXramBase);
    if (physical >= kXramPhysicalAlias &&
        end <= static_cast<std::uint64_t>(kXramPhysicalAlias) + xram_.size())
        return xram_.data() + static_cast<std::size_t>(physical - kXramPhysicalAlias);

    if (address >= kYramBase &&
        static_cast<std::uint64_t>(address) + bytes <=
            static_cast<std::uint64_t>(kYramBase) + yram_.size())
        return yram_.data() + static_cast<std::size_t>(address - kYramBase);
    if (physical >= kYramPhysicalAlias &&
        end <= static_cast<std::uint64_t>(kYramPhysicalAlias) + yram_.size())
        return yram_.data() + static_cast<std::size_t>(physical - kYramPhysicalAlias);

    if (address >= kStoreQueueBase && address < kStoreQueueEnd) {
        const std::size_t sqOff = static_cast<std::size_t>(address & 0x3Fu);
        if (sqOff + bytes <= storeQueues_.size()) {
            return storeQueues_.data() + sqOff;
        }
    }

    const std::uint64_t dsp0End = static_cast<std::uint64_t>(kDsp0SpaceBase) + dsp0Space_.size();
    if (physical >= kDsp0SpaceBase && end <= dsp0End)
        return dsp0Space_.data() + static_cast<std::size_t>(physical - kDsp0SpaceBase);

    const std::uint64_t dsp1End = static_cast<std::uint64_t>(kDsp1SpaceBase) + dsp1Space_.size();
    if (physical >= kDsp1SpaceBase && end <= dsp1End)
        return dsp1Space_.data() + static_cast<std::size_t>(physical - kDsp1SpaceBase);

    const std::uint64_t dmarsEnd = static_cast<std::uint64_t>(kDmarsPageBase) + dmarsPage_.size();
    if (physical >= kDmarsPageBase && end <= dmarsEnd)
        return dmarsPage_.data() + static_cast<std::size_t>(physical - kDmarsPageBase);

    const std::uint64_t dmacEnd = static_cast<std::uint64_t>(kDmacPageBase) + dmacPage_.size();
    if (physical >= kDmacPageBase && end <= dmacEnd)
        return dmacPage_.data() + static_cast<std::size_t>(physical - kDmacPageBase);

    const std::uint64_t ilcEnd = static_cast<std::uint64_t>(kIlcPageBase) + ilcPage_.size();
    if (physical >= kIlcPageBase && end <= ilcEnd)
        return ilcPage_.data() + static_cast<std::size_t>(physical - kIlcPageBase);

    const std::uint64_t ubcEnd = static_cast<std::uint64_t>(kUbcPageBase) + ubcPage_.size();
    if (physical >= kUbcPageBase && end <= ubcEnd)
        return ubcPage_.data() + static_cast<std::size_t>(physical - kUbcPageBase);

    const std::uint64_t priorityLockEnd = static_cast<std::uint64_t>(kPriorityLockPageBase) + priorityLockPage_.size();
    if (physical >= kPriorityLockPageBase && end <= priorityLockEnd)
        return priorityLockPage_.data() + static_cast<std::size_t>(physical - kPriorityLockPageBase);

    if (physical >= kArea1BasePhysical && physical < kArea1EndPhysical) {
        const std::size_t pIndex = (physical - kArea1BasePhysical) >> 12;
        if (pIndex < area1PageTable_.size() && area1PageTable_[pIndex] != nullptr) {
            const std::size_t offset = physical & 0xFFFu;
            if (offset + bytes <= 4096) {
                if (kMmioAuditEnabled) MmioAuditTable::Record(false, physical, bytes, regs_.pc);
                return area1PageTable_[pIndex] + offset;
            }
        }
    }

    if (kMmioAuditEnabled && (physical >= 0x04000000u || address >= 0xE0000000u)) {
        MmioAuditTable::Record(false, physical, bytes, regs_.pc);
    }
    return nullptr;
}

std::uint8_t* Cg50NativeCore::WritePointer(
    const std::uint32_t address, const std::size_t bytes)
{
    if (bytes == 0) return nullptr;
    const std::uint32_t physical = DirectMappedPhysicalAddress(address);
    // CG50_ILRAM_LOG=1: mapa de escrituras al ILRAM durante el boot (F1-A:
    // que PCs cargan cada rango del residente 0xFD800000-0xFD803FFF).
    if (kIlramLogEnabled && physical >= 0xFD800000u && physical < 0xFD804000u) {
        std::printf("[ILRAMWR] phys=0x%08X bytes=%zu pc=0x%08X inst=%llu\n",
                    physical, bytes, regs_.pc,
                    static_cast<unsigned long long>(stats_.instructions));
    }
    // Watch de escritores del framebuffer (N-8): primeros writes en el camino
    // comun (cubre Write8/16/32 y cualquier ruta que use WritePointer).
    // [FBWP-first]/[FBWP] bajo CG50_PERIPH_LOG=1 (F1-C).
    if (PeriphDiagEnabled() &&
        physical >= 0x08000000u && physical < 0x08028800u) {
        static int fbCount = 0;
        static bool fbSeen[65536] = {};
        const std::uint32_t key = regs_.pc & 0xFFFFu;
        if (fbCount < 6) {
            ++fbCount;
            std::printf("[FBWP-first] pc=0x%08X phys=0x%08X bytes=%zu inst=%llu\n",
                regs_.pc, physical, bytes, (unsigned long long)stats_.instructions);
        } else if (!fbSeen[key]) {
            fbSeen[key] = true;
            std::printf("[FBWP] pc=0x%08X phys=0x%08X inst=%llu\n",
                regs_.pc, physical, (unsigned long long)stats_.instructions);
        }
    }
    // 2026-10-29 M6/N-11 (fsD2 R/W): ventana de staging de add-ins R/W dentro
    // de [0x08080000, +size) cuando la escritura cabe entera; los cruces de
    // borde straddle siguen fail-closed (nullptr) y >= stageEnd cae limpio a RAM.
    const std::uint64_t end = static_cast<std::uint64_t>(physical) + bytes;
    if (addinStagingActive_) {
        const std::uint64_t stageEnd =
            static_cast<std::uint64_t>(kAddinStagingPhysicalBase) +
            addinStaging_.size();
        if (physical >= kAddinStagingPhysicalBase && end <= stageEnd) {
            return addinStaging_.data() +
                   static_cast<std::size_t>(physical - kAddinStagingPhysicalBase);
        }
        if (physical < stageEnd && end > kAddinStagingPhysicalBase)
            return nullptr;
    }
    const std::uint64_t ramEnd = static_cast<std::uint64_t>(kRamBase) + ram_.size();
    if (physical >= kRamBase && end <= ramEnd)
        return ram_.data() + static_cast<std::size_t>(physical - kRamBase);

    // Banco C (0x0C000000/0x8C000000): respaldo propio, no alias del banco 8
    // (oraculo oficial 2026-10-26: ramc-res0ekey-preexe.bin 100% ceros).
    const std::uint64_t ramCEnd =
        static_cast<std::uint64_t>(kRamAliasBase) + ramC_.size();
    if (physical >= kRamAliasBase && end <= ramCEnd)
        return ramC_.data() + static_cast<std::size_t>(physical - kRamAliasBase);

    const std::uint64_t onChipRamEnd =
        static_cast<std::uint64_t>(kOnChipRamBase) + onChipRam_.size();
    if (address >= kOnChipRamBase &&
        static_cast<std::uint64_t>(address) + bytes <= onChipRamEnd)
        return onChipRam_.data() + static_cast<std::size_t>(address - kOnChipRamBase);

    if (address >= kIlramBase &&
        static_cast<std::uint64_t>(address) + bytes <=
            static_cast<std::uint64_t>(kIlramBase) + ilram_.size())
        return ilram_.data() + static_cast<std::size_t>(address - kIlramBase);
    if (physical >= kIlramPhysicalAlias &&
        end <= static_cast<std::uint64_t>(kIlramPhysicalAlias) + ilram_.size())
        return ilram_.data() + static_cast<std::size_t>(physical - kIlramPhysicalAlias);

    if (address >= kXramBase &&
        static_cast<std::uint64_t>(address) + bytes <=
            static_cast<std::uint64_t>(kXramBase) + xram_.size())
        return xram_.data() + static_cast<std::size_t>(address - kXramBase);
    if (physical >= kXramPhysicalAlias &&
        end <= static_cast<std::uint64_t>(kXramPhysicalAlias) + xram_.size())
        return xram_.data() + static_cast<std::size_t>(physical - kXramPhysicalAlias);

    if (address >= kYramBase &&
        static_cast<std::uint64_t>(address) + bytes <=
            static_cast<std::uint64_t>(kYramBase) + yram_.size())
        return yram_.data() + static_cast<std::size_t>(address - kYramBase);
    if (physical >= kYramPhysicalAlias &&
        end <= static_cast<std::uint64_t>(kYramPhysicalAlias) + yram_.size())
        return yram_.data() + static_cast<std::size_t>(physical - kYramPhysicalAlias);

    if (address >= kStoreQueueBase && address < kStoreQueueEnd) {
        const std::size_t sqOff = static_cast<std::size_t>(address & 0x3Fu);
        if (sqOff + bytes <= storeQueues_.size()) {
            return storeQueues_.data() + sqOff;
        }
    }

    const std::uint64_t dsp0End = static_cast<std::uint64_t>(kDsp0SpaceBase) + dsp0Space_.size();
    if (physical >= kDsp0SpaceBase && end <= dsp0End)
        return dsp0Space_.data() + static_cast<std::size_t>(physical - kDsp0SpaceBase);

    const std::uint64_t dsp1End = static_cast<std::uint64_t>(kDsp1SpaceBase) + dsp1Space_.size();
    if (physical >= kDsp1SpaceBase && end <= dsp1End)
        return dsp1Space_.data() + static_cast<std::size_t>(physical - kDsp1SpaceBase);

    const std::uint64_t dmarsEnd = static_cast<std::uint64_t>(kDmarsPageBase) + dmarsPage_.size();
    if (physical >= kDmarsPageBase && end <= dmarsEnd)
        return dmarsPage_.data() + static_cast<std::size_t>(physical - kDmarsPageBase);

    const std::uint64_t dmacEnd = static_cast<std::uint64_t>(kDmacPageBase) + dmacPage_.size();
    if (physical >= kDmacPageBase && end <= dmacEnd)
        return dmacPage_.data() + static_cast<std::size_t>(physical - kDmacPageBase);

    const std::uint64_t ilcEnd = static_cast<std::uint64_t>(kIlcPageBase) + ilcPage_.size();
    if (physical >= kIlcPageBase && end <= ilcEnd)
        return ilcPage_.data() + static_cast<std::size_t>(physical - kIlcPageBase);

    const std::uint64_t ubcEnd = static_cast<std::uint64_t>(kUbcPageBase) + ubcPage_.size();
    if (physical >= kUbcPageBase && end <= ubcEnd)
        return ubcPage_.data() + static_cast<std::size_t>(physical - kUbcPageBase);

    const std::uint64_t priorityLockEnd = static_cast<std::uint64_t>(kPriorityLockPageBase) + priorityLockPage_.size();
    if (physical >= kPriorityLockPageBase && end <= priorityLockEnd)
        return priorityLockPage_.data() + static_cast<std::size_t>(physical - kPriorityLockPageBase);

    if (physical >= kArea1BasePhysical && physical < kArea1EndPhysical) {
        const std::size_t pIndex = (physical - kArea1BasePhysical) >> 12;
        if (pIndex < area1PageTable_.size() && area1PageTable_[pIndex] != nullptr) {
            const std::size_t offset = physical & 0xFFFu;
            if (offset + bytes <= 4096) {
                if (kMmioAuditEnabled) MmioAuditTable::Record(true, physical, bytes, regs_.pc);
                return area1PageTable_[pIndex] + offset;
            }
        }
    }

    if (kMmioAuditEnabled && (physical >= 0x04000000u || address >= 0xE0000000u)) {
        MmioAuditTable::Record(true, physical, bytes, regs_.pc);
    }
    return nullptr;
}

namespace {
// CG50_DIAG_FILE=1: habilita los logs de diagnostico en key_diag.log y
// lcd_diag.log. Desactivado por defecto para no abrir/cerrar por evento.
bool DiagFileEnabled()
{
    static const bool enabled = std::getenv("CG50_DIAG_FILE") != nullptr;
    return enabled;
}

// F1-C v2 (FR10): flags file-scope para el gating en CALL-SITES de los
// tracers per-acceso (antes static locales dentro de cada helper). Misma
// semantica: se leen una vez al cargar el modulo (no hay setenv/putenv en
// runtime en este corpus). Con la env activa la salida es identica a la
// construccion previa; sin la env el call-site se salta la llamada entera.
const bool kStrLocLogEnabled = std::getenv("CG50_STRLOC_LOG") != nullptr;
const bool kAppIfLogEnabled = std::getenv("CG50_APPIF_LOG") != nullptr;
const bool kBufWrLogEnabled = std::getenv("CG50_BUFWR_LOG") != nullptr;
const bool kResultWatchEnabled = std::getenv("CG50_RESULT_WATCH") != nullptr;

// 2026-10-30 (exam-mode): tracer de lecturas del BLOQUE de recursos
// "Examination" PA [0x144CC0, 0x145400): el copiador/verificador del boot
// (pr=0x801E1EE2, pc=0x801E1EEC, descrito por 0x806AACBC, F=0x803855D0) solo
// SUMA checksum del recurso 48B [0x144CC0,0x144CF0) frente a r14-bounds y NO
// copia a RAM (sin copia "Examination" en dumps 2026-10-30). El DIALOGO del
// guardia no releva esa ventana: texto desde OTROS recursos del bloque
// (0x144D40+). examC 2026-10-30: dialogo relevado pc=0x801DE3CA pr=0x801EAF66
// (titulo 18B [0x144D52,0x144D64) = "Examination Mode " con NUL en 0x144D64)
// y renderer pc=0x803A3D9x pr=0x801EAFC6. F2=0x801EAF42 tiene 85 refs de pool
// compartidas (169 el normalizador 0x801DE38C) -> por callers NO discrimina
// el guardia. Banda BODY agregada [0x13DC80,0x13DD20) (template "Press:[EXIT]"
// @0x13DCFB, espejo UserROM 0x5F1583): su reader en el instante del dialogo
// apunta al builder del guardia -> de ese pr sale el DECISOR y su test del
// flag exam-mode. Contrato INDEPENDIENTE: CG50_EXAMSTR_LOG=1 habilita; env
// ausente = call-site completo saltado, boot canonico identico
// (como CG50_RESULT_WATCH).
const bool kExamStrLogEnabled = std::getenv("CG50_EXAMSTR_LOG") != nullptr;

// Slots de pool (PA del Read32) que cargan 0x801EAF42 — escaneo byte exacto
// sobre SystemROM.raw, 85 hits, orden ascendente (2026-10-30).
static bool IsF2PoolSlot(const std::uint32_t physical) noexcept
{
    switch (physical) {
    case 0x000338F8u: case 0x00033B58u: case 0x00033E2Cu: case 0x000340DCu:
    case 0x00034338u: case 0x000352FCu: case 0x00035AE4u: case 0x00036310u:
    case 0x00037374u: case 0x00037680u: case 0x00037948u: case 0x000AA594u:
    case 0x000AA854u: case 0x000AAD44u: case 0x000BB4F0u: case 0x000C971Cu:
    case 0x000C9A20u: case 0x000CCED0u: case 0x000CE4FCu: case 0x000FD3F8u:
    case 0x000FD66Cu: case 0x00114CA0u: case 0x00115914u: case 0x00115EE4u:
    case 0x0011653Cu: case 0x00116AD8u: case 0x001185E4u: case 0x0011912Cu:
    case 0x0012B4A4u: case 0x0012DFA0u: case 0x0014E9FCu: case 0x00175E3Cu:
    case 0x00176208u: case 0x00178E20u: case 0x0018953Cu: case 0x00199FF8u:
    case 0x001B67C4u: case 0x001C2208u: case 0x0020C430u: case 0x0020D718u:
    case 0x0020F340u: case 0x002BA7F0u: case 0x002BCD64u: case 0x002C6AFCu:
    case 0x002C71D4u: case 0x002C74F0u: case 0x002C7834u: case 0x002C7AE4u:
    case 0x002D39B0u: case 0x002E48ECu: case 0x002E7188u: case 0x002EF97Cu:
    case 0x002F18E8u: case 0x00301F5Cu: case 0x0030273Cu: case 0x0030C66Cu:
    case 0x0030DFE4u: case 0x0031CFFCu: case 0x003288D8u: case 0x00342A00u:
    case 0x003433BCu: case 0x00363898u: case 0x00367A40u: case 0x00367D00u:
    case 0x00368048u: case 0x00368328u: case 0x003685BCu: case 0x00368B04u:
    case 0x00369AF8u: case 0x00369DD0u: case 0x0036A618u: case 0x0036A8FCu:
    case 0x0036C1D0u: case 0x0036EC44u: case 0x0036EED8u: case 0x00370924u:
    case 0x00373308u: case 0x00374F98u: case 0x00375BC0u: case 0x00375E84u:
    case 0x0037C6ECu: case 0x003844E8u: case 0x003847D4u: case 0x00384B2Cu:
    case 0x006B4DD0u:
        return true;
    default:
        return false;
    }
}

// CG50_PCWIN: tracer de entrada de PC (env ausente = call-site saltado);
// la tabla de ventanas se parsea una sola vez en PcWinWindows().
const bool kPcWinLogEnabled = std::getenv("CG50_PCWIN") != nullptr;

void TraceExamStrRead(const std::uint32_t physical, const std::uint32_t pc,
                      const std::uint32_t pr, const std::uint64_t inst) noexcept
{
    if (!kExamStrLogEnabled) return;
    if ((physical >= 0x00144CC0u && physical < 0x00145400u) ||
        (physical >= 0x0013DC80u && physical < 0x0013DD20u)) {
        std::fprintf(stderr, "[EXAMSTR] phys=%08X pc=%08X pr=%08X inst=%llu\n",
                     physical, pc, pr,
                     static_cast<unsigned long long>(inst));
        return;
    }
    // examD-2 2026-10-30: dialogo del guardia construido en DOS pasadas:
    // titulo "Examination Mode " leido/copiado/pintado a inst 175.36M y body
    // "Press:[EXIT]" a 175.68M, ambos via F2=0x801EAF42 (normalizador
    // 0x801DE38C pr=0x801EAF66 + render pr=0x801EAFC6). El builder entregó los
    // punteros por registro, asi que el DECISOR no queda como pr del read del
    // string. Pinpoint dinámico: los 85 slots de pool que cargan 0x801EAF42
    // (escaneo byte exacto SystemROM). El mov.l del builder deja pc = VA del
    // caller en el Read32 del slot -> DECISOR + pr de quien lo llama. Rango
    // gate [0x338F8,0x6B4DD0], ventana inst >= 150000000 (el checksum de boot
    // barre estos slots y ya es zona conocida). Tag log: [F2POOL]. Mismo
    // contrato: CG50_EXAMSTR_LOG=1.
    if (physical >= 0x000338F8u && physical <= 0x006B4DD0u &&
        inst >= 150000000ull && IsF2PoolSlot(physical)) {
        std::fprintf(stderr, "[F2POOL] phys=%08X pc=%08X pr=%08X inst=%llu\n",
                     physical, pc, pr,
                     static_cast<unsigned long long>(inst));
    }
}

// CG50_PCWIN (exam 2026-10-30): tracer de ENTRADA de PC. Ejecuta el hook por
// llamada de ExecuteOpcode. Ventanas "LO:HI,LO:HI[,LO:HI]" via env; default
// ausente = tracer inerte. Detecta la entrada de funciones invocadas POR
// PUNTERO (los wrappers del dialogo del guardia 0x802C707E/0x802C712E no
// tienen refs de pool: indirectas) logeando pc + pr (pr, a la entrada, aun
// es el CALLER) + inst. Max 8 ventanas por arranque.
namespace {
struct PcWinTable {
    std::uint32_t pairs[8][2];
    std::size_t count;
};
const PcWinTable& PcWinWindows()
{
    static const PcWinTable instance = [] {
        PcWinTable t{};
        const char* e = std::getenv("CG50_PCWIN");
        if (e != nullptr) {
            const char* p = e;
            while (*p != '\0' && t.count < 8) {
                unsigned lo = 0, hi = 0;
                int consumed = 0;
                if (std::sscanf(p, "%x:%x%n", &lo, &hi, &consumed) == 2) {
                    t.pairs[t.count][0] = lo;
                    t.pairs[t.count][1] = hi;
                    ++t.count;
                }
                while (*p != '\0' && *p != ',') ++p;
                while (*p == ',') ++p;
            }
        }
        return t;
    }();
    return instance;
}
} // namespace

void TracePcWindow(const std::uint32_t pc, const std::uint32_t pr,
                   const std::uint64_t inst) noexcept
{
    if (pc < 0x80000000u) return; // solo espacio P1 (ROM/RAM del OS)
    const PcWinTable& w = PcWinWindows();
    for (std::size_t i = 0; i < w.count; ++i) {
        if (pc >= w.pairs[i][0] && pc <= w.pairs[i][1]) {
            std::fprintf(stderr, "[PCWIN] pc=%08X pr=%08X inst=%llu\n",
                         pc, pr, static_cast<unsigned long long>(inst));
            return;
        }
    }
}

// 2026-10-29 M5: auto-dirty LEGADO de initial-page-write. Default OFF
// (EXPEWT 0x080 + vector 0x100, semantica SH-4A de mmu.go:190-198);
// CG50_UTLB_AUTODIRTY=1 restaura el comportamiento previo (marcar D en
// silencio, sin excepcion) para diffing.
const bool kUtlbAutoDirty = [] {
    const char* e = std::getenv("CG50_UTLB_AUTODIRTY");
    return e != nullptr && e[0] == '1' && e[1] == '\0';
}();

// 2026-10-29 (C) SCANCONS: default OFF (revertido el mismo día por
// evidencia A/B en frío: con la siembra c68<-c48 el guest entra por
// [EXIT-dc18] R0=-3 y el MOUNT devuelve 0xFFFFFFFD con el diálogo "File
// System ERROR / Initialize All[F1]" (run-stress-utlb), mientras el core
// previo sin siembra ejecuta el MERGE repair (MERGEWR) y MOUNT ret=0
// (run-boot1 del 2026-10-28). La desigualdad c68!=c48 es la señal que el
// OS usa para construir la cadena de clusters libres, no un error.
// CG50_SCAN_CONSISTENCY=1 reactiva la siembra solo para experimentos.
const bool kScanConsistencyEnabled = [] {
    const char* e = std::getenv("CG50_SCAN_CONSISTENCY");
    return e != nullptr && e[0] == '1' && e[1] == '\0';
}();

// F1-A v20 (2026-10-25): trazador de LECTURAS de datos por rango de instrucciones.
// CG50_READWIN_FILE habilita; CG50_READWIN_LO/HI delimitan el rango inclusive de
// stats_.instructions. Excluye fetches (lecturas en regs_.pc / regs_.pc+2).
// Formato: [READWIN] INS=<n> PC=<08X> RD<8|16|32> ADDR=<08X> VAL=<hex>|none PR=<08X>
// Con la env ausente el call-site se salta entero (inocuo por contrato del corpus).
const bool kReadWinEnabled = std::getenv("CG50_READWIN_FILE") != nullptr;
const unsigned long long kReadWinLo = [] {
    const char* e = std::getenv("CG50_READWIN_LO");
    return e ? std::strtoull(e, nullptr, 10) : 0ULL;
}();
const unsigned long long kReadWinHi = [] {
    const char* e = std::getenv("CG50_READWIN_HI");
    return e ? std::strtoull(e, nullptr, 10) : 0ULL;
}();

// Filtro opcional por PC del consumidor: "lo-hi,lo-hi,..." (hex, sin 0x). Vacio = sin filtro.
// Permite capturar solo las lecturas que ejecuta una zona de codigo concreta (p.ej. la
// cadena de conversion 0x800AD5xx) cuando la epoca INS de la corrida actual no coincide
// con la de las trazas retenidas.
struct ReadWinPcRange { std::uint32_t lo; std::uint32_t hi; };
const std::vector<ReadWinPcRange> kReadWinPcs = [] {
    std::vector<ReadWinPcRange> v;
    const char* e = std::getenv("CG50_READWIN_PCS");
    if (!e) return v;
    const char* p = e;
    while (*p) {
        char* end = nullptr;
        const unsigned long long lo = std::strtoull(p, &end, 16);
        if (end == p) break;
        p = end;
        std::uint32_t hi = static_cast<std::uint32_t>(lo);
        if (*p == '-') {
            ++p;
            const unsigned long long h = std::strtoull(p, &end, 16);
            if (end != p) {
                hi = static_cast<std::uint32_t>(h);
                p = end;
            }
        }
        v.push_back({static_cast<std::uint32_t>(lo), hi});
        if (*p == ',') ++p;
        else break;
    }
    return v;
}();

bool ReadWinPcMatches(std::uint32_t pc)
{
    if (kReadWinPcs.empty()) return true;
    for (const auto& r : kReadWinPcs) {
        if (pc >= r.lo && pc <= r.hi) return true;
    }
    return false;
}

// F1-A v21 (2026-10-25): trazador de ESCRITURAS por rango de instrucciones y
// (opcional) por rango de direcciones. CG50_WRWIN_FILE habilita; CG50_WRWIN_LO/HI
// delimitan stats_.instructions; CG50_WRWIN_ADDRS="lo-hi,lo-hi" (hex, fisicas tras
// DirectMappedPhysicalAddress) filtra destinos. Formato:
// [WRWIN] INS=<n> PC=<08X> WR<8|16|32> ADDR=<08X> VAL=<hex> PR=<08X>
const bool kWrWinEnabled = std::getenv("CG50_WRWIN_FILE") != nullptr;
const unsigned long long kWrWinLo = [] {
    const char* e = std::getenv("CG50_WRWIN_LO");
    return e ? std::strtoull(e, nullptr, 10) : 0ULL;
}();
const unsigned long long kWrWinHi = [] {
    const char* e = std::getenv("CG50_WRWIN_HI");
    return e ? std::strtoull(e, nullptr, 10) : 0ULL;
}();
const std::vector<ReadWinPcRange> kWrWinAddrs = [] {
    std::vector<ReadWinPcRange> v;
    const char* e = std::getenv("CG50_WRWIN_ADDRS");
    if (!e) return v;
    const char* p = e;
    while (*p) {
        char* end = nullptr;
        const unsigned long long lo = std::strtoull(p, &end, 16);
        if (end == p) break;
        p = end;
        std::uint32_t hi = static_cast<std::uint32_t>(lo);
        if (*p == '-') {
            ++p;
            const unsigned long long h = std::strtoull(p, &end, 16);
            if (end != p) {
                hi = static_cast<std::uint32_t>(h);
                p = end;
            }
        }
        v.push_back({static_cast<std::uint32_t>(lo), hi});
        if (*p == ',') ++p;
        else break;
    }
    return v;
}();

bool WrWinAddrMatches(std::uint32_t physical)
{
    if (kWrWinAddrs.empty()) return true;
    for (const auto& r : kWrWinAddrs) {
        if (physical >= r.lo && physical <= r.hi) return true;
    }
    return false;
}

void TraceWrWinValues(unsigned long long ins, std::uint32_t pc, std::uint32_t pr,
                      std::uint32_t physical, unsigned size, std::uint64_t value)
{
    static FILE* out = [] {
        const char* p = std::getenv("CG50_WRWIN_FILE");
        return p ? std::fopen(p, "w") : nullptr;
    }();
    if (!out) return;
    std::fprintf(out, "[WRWIN] INS=%llu PC=%08X WR%u ADDR=%08X VAL=%0*llX PR=%08X\n",
                 ins, pc, size, physical, static_cast<int>(size * 2),
                 static_cast<unsigned long long>(value), pr);
}

void TraceReadWinValues(unsigned long long ins, std::uint32_t pc, std::uint32_t pr,
                        std::uint32_t address, unsigned size, std::uint64_t value,
                        bool haveValue)
{
    static FILE* out = [] {
        const char* p = std::getenv("CG50_READWIN_FILE");
        return p ? std::fopen(p, "w") : nullptr;
    }();
    if (!out) return;
    if (haveValue) {
        std::fprintf(out, "[READWIN] INS=%llu PC=%08X RD%u ADDR=%08X VAL=%0*llX PR=%08X\n",
                     ins, pc, size, address, static_cast<int>(size * 2),
                     static_cast<unsigned long long>(value), pr);
    } else {
        std::fprintf(out, "[READWIN] INS=%llu PC=%08X RD%u ADDR=%08X VAL=none PR=%08X\n",
                     ins, pc, size, address, pr);
    }
}

// Watch de la cola de eventos del teclado (P0): bloque 0x8808B960-0x8808B9FF
// (fisico 0x0808B960), cabecera de 4 bytes y 8 entradas de 20 bytes desde
// 0x8808B964. Registra TODO acceso de 8/16/32 bits con PC, PR e instruccion.
// queue_diag.log se escribe bajo CG50_DIAG_FILE=1; stdout recibe solo accesos
// al bloque, al destino externo del memcpy del pop/consume (0x801E8816 con
// retorno 0x801E89DC) o escrituras externas del delete (0x801E8894), con
// dedupe para no inundar con la pila.
constexpr std::uint32_t kQueueWatchPhysicalBase = 0x0808B960u;
constexpr std::uint32_t kQueueWatchPhysicalEnd = 0x0808BA00u;
constexpr std::uint32_t kQueuePopFirstPc = 0x801E8894u;
constexpr std::uint32_t kQueuePopEndPc = 0x801E88ECu;
constexpr std::uint32_t kQueuePopMemcpyFirstPc = 0x801E8816u;
constexpr std::uint32_t kQueuePopMemcpyEndPc = 0x801E882Eu;
constexpr std::uint32_t kQueueEnqueueMemcpyReturnPc = 0x801E898Au;
constexpr std::uint32_t kQueueConsumeMemcpyReturnPc = 0x801E89DCu;
constexpr std::uint32_t kQueueMemcpyStorePc = 0x801E8824u;

struct QueueWatchDestRange {
    std::uint32_t begin = 0;
    std::uint32_t end = 0;
    bool readReported = false;
};

struct QueueWatchPopWrite {
    std::uint32_t pc = 0;
    std::uint32_t physical = 0;
};

struct QueueWatchState {
    QueueWatchDestRange destinations[8]{};
    std::size_t destinationCount = 0;
    QueueWatchPopWrite popWrites[64]{};
    std::size_t popWriteCount = 0;
    QueueWatchPopWrite sourceReads[64]{};
    std::size_t sourceReadCount = 0;
};

QueueWatchState& QueueWatchStateRef()
{
    static QueueWatchState state;
    return state;
}

void QueueDiagLine(const char* line) noexcept
{
    std::fputs(line, stdout);
    if (!DiagFileEnabled()) return;
    if (FILE* file = Cg50DiagFopen("queue_diag.log", "a")) {
        std::fputs(line, file);
        std::fclose(file);
    }
}

bool QueueWatchReadValueNeeded(const std::uint32_t address,
                               const std::uint32_t width) noexcept
{
    const std::uint32_t physical =
        Cg50NativeCore::DirectMappedPhysicalAddress(address);
    const std::uint64_t accessEnd = static_cast<std::uint64_t>(physical) + width;
    if (physical < kQueueWatchPhysicalEnd &&
        accessEnd > kQueueWatchPhysicalBase) {
        return true;
    }
    const QueueWatchState& state = QueueWatchStateRef();
    for (std::size_t i = 0; i < state.destinationCount; ++i) {
        if (physical < state.destinations[i].end &&
            accessEnd > state.destinations[i].begin) {
            return true;
        }
    }
    return false;
}

void QueueWatchEmit(const char* tag, const char* op,
                    const std::uint32_t address, const std::uint32_t physical,
                    const std::uint32_t value, const std::uint32_t width,
                    const std::uint32_t pc, const std::uint32_t pr,
                    const std::uint64_t inst) noexcept
{
    char line[320];
    std::snprintf(line, sizeof(line),
                  "[%s] %s addr=0x%08X phys=0x%08X val=0x%08X width=%u pc=0x%08X pr=0x%08X inst=%llu\n",
                  tag, op, address, physical, value, width, pc, pr,
                  static_cast<unsigned long long>(inst));
    QueueDiagLine(line);
}

void QueueWatchAccess(const char* op, const std::uint32_t address,
                      const std::uint32_t value, const std::uint32_t width,
                      const std::uint32_t pc, const std::uint32_t pr,
                      const std::uint64_t inst) noexcept
{
    const std::uint32_t physical =
        Cg50NativeCore::DirectMappedPhysicalAddress(address);
    const std::uint64_t accessEnd = static_cast<std::uint64_t>(physical) + width;
    const bool inQueue =
        physical < kQueueWatchPhysicalEnd &&
        accessEnd > kQueueWatchPhysicalBase;
    const bool fromPop = pc >= kQueuePopFirstPc && pc < kQueuePopEndPc;
    const bool fromPopMemcpy =
        pc >= kQueuePopMemcpyFirstPc && pc < kQueuePopMemcpyEndPc &&
        pr >= kQueuePopFirstPc && pr < kQueuePopEndPc;
    const bool fromEnqueueMemcpy =
        pc >= kQueuePopMemcpyFirstPc && pc < kQueuePopMemcpyEndPc &&
        pr == kQueueEnqueueMemcpyReturnPc;
    const bool fromConsumeMemcpy =
        pc >= kQueuePopMemcpyFirstPc && pc < kQueuePopMemcpyEndPc &&
        pr == kQueueConsumeMemcpyReturnPc;

    QueueWatchState& state = QueueWatchStateRef();
    if (op[0] == 'W' && (fromPopMemcpy || fromConsumeMemcpy) &&
        pc == kQueueMemcpyStorePc && !inQueue && width >= 1u &&
        physical >= 0x08000000u && physical < 0x0C000000u) {
        bool merged = false;
        for (std::size_t i = 0; i < state.destinationCount; ++i) {
            QueueWatchDestRange& range = state.destinations[i];
            if (physical >= range.begin && physical < range.end) {
                merged = true;
                break;
            }
            if (physical == range.end) {
                range.end += width;
                merged = true;
                break;
            }
            if (physical + width == range.begin) {
                range.begin = physical;
                merged = true;
                break;
            }
        }
        if (!merged && state.destinationCount < 8u) {
            state.destinations[state.destinationCount].begin = physical;
            state.destinations[state.destinationCount].end = physical + width;
            ++state.destinationCount;
            char line[224];
            std::snprintf(line, sizeof(line),
                          "[QUEUE-POPDEST] begin=0x%08X end=0x%08X pc=0x%08X inst=%llu\n",
                          physical, physical + width, pc,
                          static_cast<unsigned long long>(inst));
            QueueDiagLine(line);
        }
    }

    bool inDestination = false;
    std::size_t destinationIndex = 0;
    for (std::size_t i = 0; i < state.destinationCount; ++i) {
        if (physical < state.destinations[i].end &&
            accessEnd > state.destinations[i].begin) {
            inDestination = true;
            destinationIndex = i;
            break;
        }
    }

    if (inQueue) {
        QueueWatchEmit("QUEUE", op, address, physical, value, width, pc, pr,
                       inst);
        return;
    }
    if (inDestination) {
        QueueWatchDestRange& range = state.destinations[destinationIndex];
        if (op[0] == 'R' && !range.readReported) {
            range.readReported = true;
            char line[224];
            std::snprintf(line, sizeof(line),
                          "[QUEUE-POPDEST-RANGE] begin=0x%08X end=0x%08X pc=0x%08X pr=0x%08X inst=%llu\n",
                          range.begin, range.end, pc, pr,
                          static_cast<unsigned long long>(inst));
            QueueDiagLine(line);
        }
        QueueWatchEmit("QUEUEDST", op, address, physical, value, width, pc, pr,
                       inst);
        return;
    }
    if (fromEnqueueMemcpy && op[0] == 'R' && width == 1u &&
        physical >= 0x08000000u && physical < 0x0C000000u) {
        for (std::size_t i = 0; i < state.sourceReadCount; ++i) {
            if (state.sourceReads[i].physical == physical) return;
        }
        if (state.sourceReadCount < 64u) {
            state.sourceReads[state.sourceReadCount].pc = pc;
            state.sourceReads[state.sourceReadCount].physical = physical;
            ++state.sourceReadCount;
            QueueWatchEmit("QUEUESRC", op, address, physical, value, width, pc,
                           pr, inst);
        }
        return;
    }
    if (!fromPop || op[0] != 'W') return;
    for (std::size_t i = 0; i < state.popWriteCount; ++i) {
        if (state.popWrites[i].pc == pc &&
            state.popWrites[i].physical == physical) {
            return;
        }
    }
    if (state.popWriteCount < 64u) {
        state.popWrites[state.popWriteCount].pc = pc;
        state.popWrites[state.popWriteCount].physical = physical;
        ++state.popWriteCount;
        QueueWatchEmit("QUEUEPOP", op, address, physical, value, width, pc, pr,
                       inst);
    }
}

// CG50_STRLOC_LOG=1: registra lecturas de la region de recursos de texto
// (0xA013F000-0xA0145000) con el PC de la instruccion - usado para localizar
// el codigo que dibuja los dialogos del OS (p.ej. Ãƒâ€šÃ‚Â«File System ERRORÃƒâ€šÃ‚Â»).
void TraceStringRegionRead(const std::uint32_t address, const std::uint32_t pc,
                           const std::uint32_t pr, const std::uint32_t r0,
                           const std::uint32_t r1) noexcept
{
    if (!kStrLocLogEnabled) return;
    if ((address >= 0xA013F000u && address < 0xA0145000u) ||
        (address >= 0x8013F000u && address < 0x80145000u)) {
        std::fprintf(stderr, "[STRREAD] addr=%08X pc=%08X pr=%08X r0=%08X r1=%08X\n",
                     address, pc, pr, r0, r1);
    }
}

// CG50_APPIF_LOG=1: registra accesos a la ventana del bus AppIf (host GCalAppIF)
// en P2 0xB4000000 (fisica 0x14000000 tras el enmascarado de P2) - handshake host.
void TraceAppIfAccess(const std::uint32_t physical, const char* op,
                      const std::uint32_t value, const std::uint32_t pc) noexcept
{
    if (kAppIfLogEnabled && physical >= 0x14000000u && physical < 0x14010000u) {
        std::fprintf(stderr, "[APPIF] %s phys=%08X value=%08X pc=%08X\n",
                     op, physical, value, pc);
    }
    // CG50_BUFWR_LOG=1: accesos al buffer FS 0x8809C000-0x8809C800 (N-8).
    if (kBufWrLogEnabled && physical >= 0x0809C000u && physical < 0x0809C800u) {
        std::fprintf(stderr, "[BUF9C] %s phys=%08X value=%08X pc=%08X\n",
                     op, physical, value, pc);
    }
}

// CG50_RESULT_WATCH=1: registra escrituras al workspace del calculo y a la
// historia del editor (ventana post-EXE). Solo diagnostico, no altera nada.
void TraceResultWatch(const char* op, const std::uint32_t physical,
                      const std::uint32_t value, const std::uint32_t pc,
                      const std::uint64_t inst) noexcept
{
    if (!kResultWatchEnabled) return;
    const bool workspace = physical >= 0x08184D00u && physical < 0x08185E00u;
    const bool history = physical >= 0x080D3400u && physical < 0x080D4200u;
    const bool areaA3 = physical >= 0x08A31000u && physical < 0x08A31300u;
    const bool dispatch = physical >= 0x0804F800u && physical < 0x0804F900u;
    const bool result = physical >= 0x0804FB20u && physical < 0x0804FB60u;
    if (!workspace && !history && !areaA3 && !dispatch && !result) return;
    std::fprintf(stderr, "[RESW] %s phys=%08X val=%08X pc=%08X inst=%llu\n",
                 op, physical, value, pc, (unsigned long long)inst);
}

// PBDR bit 2 como senal dinamica del host (fix F1-A, spec 19.4). Por
// defecto activa; CG50_PBDR_HOST=0 restaura la lectura de almacenamiento
// estatico (comportamiento previo al fix, util para A/B).
static bool PbdrHostBitEnabled()
{
    static const bool enabled = [] {
        const char* v = std::getenv("CG50_PBDR_HOST");
        return v == nullptr || std::strcmp(v, "0") != 0;
    }();
    return enabled;
}

static bool PbdrDiagEnabled()
{
    static const bool enabled = std::getenv("CG50_PBDR_DIAG") != nullptr;
    return enabled;
}
} // namespace

bool Cg50NativeCore::Read8(const std::uint32_t address, std::uint8_t& value) const
{
    if (kReadWinEnabled && stats_.instructions >= kReadWinLo &&
        stats_.instructions <= kReadWinHi && ReadWinPcMatches(regs_.pc) &&
        address != regs_.pc && address != regs_.pc + 2u) {
        std::uint64_t rv = 0;
        bool have = false;
        if (const auto* rp = ReadPointer(address, 1)) {
            rv = *rp;
            have = true;
        }
        TraceReadWinValues(stats_.instructions, regs_.pc, regs_.pr, address, 1, rv, have);
    }
    if (kStrLocLogEnabled)
        TraceStringRegionRead(address, regs_.pc, regs_.pr, regs_.r[0], regs_.r[1]);
    if (kAppIfLogEnabled || kBufWrLogEnabled)
        TraceAppIfAccess(DirectMappedPhysicalAddress(address), "RD8", 0, regs_.pc);
    if (PeriphDiagEnabled()) {
        std::uint32_t observed = 0;
        if (QueueWatchReadValueNeeded(address, 1)) {
            if (const auto* qp = ReadPointer(address, 1)) observed = *qp;
        }
        QueueWatchAccess("RD8", address, observed, 1, regs_.pc, regs_.pr,
                         stats_.instructions);
    }
    if (PeriphDiagEnabled() &&
        ((address >= 0x880A4410u && address <= 0x880A4413u) || (address >= 0x880A156Cu && address <= 0x880A156Fu) || (address >= 0x88090CB4u && address <= 0x88090CB7u))) {
        const auto* fvp = ReadPointer(address, 1);
        const unsigned fv = fvp ? *fvp : 0xFFu;
        std::printf("[FLAGRD] addr=0x%08X val=0x%02X pc=0x%08X pr=0x%08X inst=%llu\n",
            address, fv, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                std::fprintf(kd, "[FLAGRD] addr=0x%08X val=0x%02X pc=0x%08X pr=0x%08X inst=%llu\n",
                    address, fv, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    if (PeriphDiagEnabled() &&
        address >= 0x88090C00u && address <= 0x88090CCFu) {
        const auto* evp = ReadPointer(address, 1);
        const unsigned ev = evp ? *evp : 0xFFu;
        std::printf("[EVTRD] addr=0x%08X val=0x%02X pc=0x%08X pr=0x%08X inst=%llu\n",
            address, ev, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                std::fprintf(kd, "[EVTRD] addr=0x%08X val=0x%02X pc=0x%08X pr=0x%08X inst=%llu\n",
                    address, ev, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    if (PeriphDiagEnabled() &&
        DirectMappedPhysicalAddress(address) >= 0x044A0000u &&
        DirectMappedPhysicalAddress(address) < 0x044A0100u) {
        const auto* lp = ReadPointer(DirectMappedPhysicalAddress(address), 1);
        const unsigned lv = lp ? *lp : 0xFFu;
        std::printf("[LCDA8] R addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
            DirectMappedPhysicalAddress(address), lv, regs_.pc,
            (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("lcd_diag.log", "a")) {
                std::fprintf(kd, "[LCDA8] R addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                    DirectMappedPhysicalAddress(address), lv, regs_.pc,
                    (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    const auto physical = DirectMappedPhysicalAddress(address);
    if (physical == kTmu0PhysicalBase + 0x04u) {  // TSTR0
        std::uint8_t str = 0;
        for (std::size_t ch = 0; ch < tmu0_.size(); ++ch) {
            if (tmu0_[ch].running) str |= static_cast<std::uint8_t>(1u << ch);
        }
        value = str;
        return true;
    }
    if (physical == kRtc64HzCounterRegisterPhysical) {
        value = static_cast<std::uint8_t>((stats_.instructions / 500u) & 0x7Fu);
        return true;
    }
    if (rtcVirtualClock_ &&
        physical >= kRtcRegisterPhysicalBase + 0x02u &&
        physical <= kRtcRegisterPhysicalBase + 0x0Cu &&
        (physical & 1u) == 0u) {
        value = RtcDerivedComponent8(physical - kRtcRegisterPhysicalBase);
        return true;
    }
    if (physical >= kPfcBasePhysical && physical <= kPfcEndPhysical) {
        value = pfcStorage_[physical - kPfcBasePhysical];
        if (PbdrHostBitEnabled() && physical == kPfcBasePhysical + 0x3Au) {
            // PBDR: el bit 2 NO es un latch del guest sino la senal viva del
            // host; se toma del estado del buzon, no del almacen.
            const bool pbdrWork = HostHasPendingWork();
            value = static_cast<std::uint8_t>(
                (value & ~0x04u) | (pbdrWork ? 0x04u : 0x00u));
            if (PbdrDiagEnabled()) {
                std::printf("[PBDR] inst=%llu pc=%08X storage=%02X work=%d served=%02X\n",
                    static_cast<unsigned long long>(stats_.instructions),
                    regs_.pc, pfcStorage_[0x3A], pbdrWork ? 1 : 0, value);
            }
        }
        return true;
    }
    if (physical == kWatchdogCounterRegisterPhysical) {
        value = watchdogCounter_;
        return true;
    }
    if (physical == kWatchdogControlRegisterPhysical) {
        value = watchdogControl_;
        return true;
    }
    std::size_t maskIndex = 0;
    if (InterruptMaskRegisterIndex(physical, maskIndex)) {
        value = interruptMask_[maskIndex];
        return true;
    }
    if (InterruptMaskClearRegisterIndex(physical, maskIndex)) {
        value = 0;
        return true;
    }
    if (address == kCpuopmRegister) {
        value = static_cast<std::uint8_t>(cpuopm_ & 0xFFu);
        return true;
    }
    if (address == kExpmaskRegister) {
        value = static_cast<std::uint8_t>(expmask_ & 0xFFu);
        return true;
    }
    if (cg50::Intc2Unit::MatchesPhysical(physical)) return intc2_.Read8(physical, value);
    if (cg50::EtmuUnit::MatchesPhysical(physical)) return etmu_.Read8(physical, value);
    if (cg50::AdcUnit::MatchesPhysical(physical)) return adc_.Read8(physical, value);
    if (cg50::TpuUnit::MatchesPhysical(physical)) return tpu_.Read8(physical, value);
    if (cg50::Scif0Unit::MatchesPhysical(physical)) return scif0_.Read8(physical, value);
    if (cg50::Usb2Unit::MatchesPhysical(physical)) return usb2_.Read8(physical, value);
    // (El status CFI 0x88 se aplica SOLO en Read16: los sondeos del driver
    // ILRAM usan mov.w; las copias usan mov.l/mov.b y deben leer el dato real.)
    // Watch de lecturas del string "System ERROR" (N-8): localiza el renderer.
    if (PeriphDiagEnabled() &&
        physical >= 0x0013E100u && physical < 0x0013E140u) {
        std::printf("[STRWATCH] phys=0x%08X pc=0x%08X inst=%llu\n",
            physical, regs_.pc, (unsigned long long)stats_.instructions);
    }
    if (kExamStrLogEnabled)
        TraceExamStrRead(physical, regs_.pc, regs_.pr, stats_.instructions);
    const auto* p = ReadPointer(address, 1);
    if (!p) return false;
    value = *p;
    return true;
}

bool Cg50NativeCore::Read16(const std::uint32_t address, std::uint16_t& value) const
{
    if (kReadWinEnabled && stats_.instructions >= kReadWinLo &&
        stats_.instructions <= kReadWinHi && ReadWinPcMatches(regs_.pc) &&
        address != regs_.pc && address != regs_.pc + 2u) {
        std::uint64_t rv = 0;
        bool have = false;
        if (const auto* rp = ReadPointer(address, 2)) {
            rv = (static_cast<std::uint64_t>(rp[0]) << 8) | rp[1];
            have = true;
        }
        TraceReadWinValues(stats_.instructions, regs_.pc, regs_.pr, address, 2, rv, have);
    }
    if (kStrLocLogEnabled)
        TraceStringRegionRead(address, regs_.pc, regs_.pr, regs_.r[0], regs_.r[1]);
    if (kAppIfLogEnabled || kBufWrLogEnabled)
        TraceAppIfAccess(DirectMappedPhysicalAddress(address), "RD16", 0, regs_.pc);
    if (PeriphDiagEnabled()) {
        std::uint32_t observed = 0;
        if (QueueWatchReadValueNeeded(address, 2)) {
            if (const auto* qp = ReadPointer(address, 2)) {
                observed = (static_cast<std::uint32_t>(qp[0]) << 8) | qp[1];
            }
        }
        QueueWatchAccess("RD16", address, observed, 2, regs_.pc, regs_.pr,
                         stats_.instructions);
    }
    if (PeriphDiagEnabled() &&
        address >= 0x880A4410u && address <= 0x880A441Fu) {
        const auto* p = ReadPointer(address, 2);
        const unsigned fv = p ? ((static_cast<unsigned>(p[0]) << 8) | p[1]) : 0xFFFFu;
        std::printf("[FLAGRD16] addr=0x%08X val=0x%04X pc=0x%08X pr=0x%08X inst=%llu\n",
            address, fv, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                std::fprintf(kd, "[FLAGRD16] addr=0x%08X val=0x%04X pc=0x%08X pr=0x%08X inst=%llu\n",
                    address, fv, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    if (PeriphDiagEnabled() &&
        address >= 0x88090C00u && address <= 0x88090CCFu) {
        const auto* p = ReadPointer(address, 2);
        const unsigned ev = p ? ((static_cast<unsigned>(p[0]) << 8) | p[1]) : 0xFFFFu;
        std::printf("[EVTRD16] addr=0x%08X val=0x%04X pc=0x%08X pr=0x%08X inst=%llu\n",
            address, ev, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                std::fprintf(kd, "[EVTRD16] addr=0x%08X val=0x%04X pc=0x%08X pr=0x%08X inst=%llu\n",
                    address, ev, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    // 2026-10-29 M9: arrays MMIO de la UTLB ([0xF6000000, 0xF8000000)).
    // Interceptado ANTES del mapeo directo (P4 no mapea a fisico para esta
    // ventana). Solo Read16/Read32; acceso 8-bit no soportado (declarado).
    if (address >= kItlbAddressArrayBase && address < kItlbArraysEnd) {
        value = static_cast<std::uint16_t>(ItlbArrayRead(address));
        return true;
    }
    if (address >= kUtlbAddressArrayBase && address < kUtlbArraysEnd) {
        value = static_cast<std::uint16_t>(UtlbArrayRead(address));  // 2026-10-29 M9
        return true;
    }
    const auto physical = DirectMappedPhysicalAddress(address);
    // TMU unit 0: TCR at base + 0x10 + ch*0x0C (dedicated register file).
    if (physical >= kTmu0PhysicalBase + 0x10u &&
        physical <= kTmu0PhysicalBase + 0x28u &&
        ((physical - kTmu0PhysicalBase - 0x10u) % 0x0Cu) == 0u) {
        const std::size_t ch =
            static_cast<std::size_t>((physical - kTmu0PhysicalBase - 0x10u) / 0x0Cu);
        if (ch < tmu0_.size()) {
            value = tmu0_[ch].tcr;
            return true;
        }
    }
    if (rtcVirtualClock_ &&
        physical >= kRtcRegisterPhysicalBase &&
        physical < kRtcRegisterPhysicalBase + 0x10u && (physical & 1u) == 0u) {
        const std::uint32_t offset = physical - kRtcRegisterPhysicalBase;
        if (offset == 0x00u) {
            // 16-bit read of the 64 Hz counter: derived value in the low
            // byte (SH7305-style 16-bit register with the semantic byte at
            // +0). The OS's own 8-bit R64CNT access at +0 is the validated
            // boot path and is untouched.
            value = static_cast<std::uint16_t>(
                (stats_.instructions / 500u) & 0x7Fu);
            return true;
        }
        if (offset == 0x0Eu) {
            value = RtcDerivedYearBcd();
            return true;
        }
        value = static_cast<std::uint16_t>(RtcDerivedComponent8(offset));
        return true;
    }
    if (physical == kCompareMatchTimerStartRegisterPhysical) {
        value = compareMatchTimerStart_;
        return true;
    }
    if (physical == kCompareMatchTimerControlRegisterPhysical) {
        value = compareMatchTimerControl_;
        if (PeriphDiagEnabled()) {
            static std::uint16_t lastCtrlLogged = 0xFFFFu;
            static std::uint64_t ctrlReads = 0;
            ++ctrlReads;
            if (value != lastCtrlLogged || (ctrlReads % 65536u) == 0u) {
                lastCtrlLogged = value;
                std::printf("[CMT] R16 ctrl=0x%04X pc=0x%08X inst=%llu n=%llu\n",
                    value, regs_.pc, (unsigned long long)stats_.instructions,
                    (unsigned long long)ctrlReads);
                if (DiagFileEnabled()) {
                    if (FILE* kd = Cg50DiagFopen("lcd_diag.log", "a")) {
                        std::fprintf(kd, "[CMT] R16 ctrl=0x%04X pc=0x%08X inst=%llu n=%llu\n",
                            value, regs_.pc, (unsigned long long)stats_.instructions,
                            (unsigned long long)ctrlReads);
                        std::fclose(kd);
                    }
                }
            }
        }
        return true;
    }
    if (physical == kWatchdogCounterRegisterPhysical) {
        value = watchdogCounter_;
        return true;
    }
    if (physical == kWatchdogControlRegisterPhysical) {
        value = watchdogControl_;
        return true;
    }
    if (physical >= kPfcBasePhysical && physical + 1 <= kPfcEndPhysical && ((physical & 1) == 0)) {
        const std::size_t off = physical - kPfcBasePhysical;
        value = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(pfcStorage_[off]) << 8) |
            static_cast<std::uint16_t>(pfcStorage_[off + 1]));
        return true;
    }
    if (address >= kSdramModeRegister2Base && address <= kSdramModeRegister3End) {
        value = 0;
        return true;
    }
    std::size_t driveControlIndex = 0;
    if (DriveControlRegisterIndex(physical, driveControlIndex)) {
        value = driveControl_[driveControlIndex];
        return true;
    }
    std::size_t keyRegisterIndex = 0;
    if (KeyInterfaceRegisterIndex(
            physical, keyRegisterIndex)) {
        if (keyRegisterIndex < kKeyInterfaceDataRegisterCount) {
            value = keyInputData_[keyRegisterIndex];
            // El OS acaba de leer una tecla pulsada de la matriz: el proximo
            // despacho F001000C ya puede ver el evento de UI (N-1). Se marca
            // aqui en vez de usar un temporizador: el escaneo real del guest
            // es la unica senal fiable de que la tecla esta en su cola.
            if (value != 0) keyScanRead_ = true;
        } else {
            value = keyInterfaceControl_[
                keyRegisterIndex - kKeyInterfaceDataRegisterCount];
        }
        if (PeriphDiagEnabled() &&
            keyRegisterIndex < kKeyInterfaceDataRegisterCount && value != 0) {
            std::printf("[KIURD] idx=%u val=0x%04X pc=0x%08X inst=%llu\n",
                static_cast<unsigned>(keyRegisterIndex), value, regs_.pc,
                (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[KIURD] idx=%u val=0x%04X pc=0x%08X inst=%llu\n",
                        static_cast<unsigned>(keyRegisterIndex), value, regs_.pc,
                        (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        return true;
    }
    std::size_t priorityIndex = 0;
    if (InterruptPriorityRegisterIndex(
            physical, priorityIndex)) {
        value = interruptPriority_[priorityIndex];
        return true;
    }
    std::size_t maskIndex = 0;
    if (InterruptMaskRegisterIndex(physical, maskIndex)) {
        value = interruptMask_[maskIndex];
        return true;
    }
    if (InterruptMaskClearRegisterIndex(physical, maskIndex)) {
        value = 0;
        return true;
    }
    if (address == kCacheControlRegister) {
        value = static_cast<std::uint16_t>(cacheControl_ & 0xFFFFu);
        return true;
    }
    if (address == kCpuopmRegister) {
        value = static_cast<std::uint16_t>(cpuopm_ & 0xFFFFu);
        return true;
    }
    if (address == kExpmaskRegister) {
        value = static_cast<std::uint16_t>(expmask_ & 0xFFFFu);
        return true;
    }
    if (address == kPvrRegister || address == kPrrRegister) {
        value = 0;
        return true;
    }
    if (address == kQacr0Register) {
        value = static_cast<std::uint16_t>(qacr0_ & 0xFFFFu);
        return true;
    }
    if (address == kQacr1Register) {
        value = static_cast<std::uint16_t>(qacr1_ & 0xFFFFu);
        return true;
    }
    if (physical == kVpuClockControlRegisterPhysical) {
        value = static_cast<std::uint16_t>(vpuClockControl_ & 0xFFFFu);
        return true;
    }
    if (physical == 0x08092D2Cu) {
        value = 0x0280u;
        return true;
    }
    if (physical == 0x08092D30u) {
        value = 0x0001u;
        return true;
    }
    // Spansion S29GL256P CFI read interception (Autoselect 0x90 / CFI Query 0x98).
    // In ReadArray mode (including immediately after Sector Erase / Write-to-Buffer
    // / Word Program), reads fall through to the real backing NOR array so DQ7
    // polling reads the actual programmed word or 0xFFFF erased word.
    if (!flashCfi_.IsReadArray() && IsFlashPhysicalAddress(physical)) {
        if (flashCfi_.TryRead16(physical, value)) {
            return true;
        }
    }
    // Watch de lecturas del string "System ERROR" (N-8): localiza el renderer.
    if (PeriphDiagEnabled() &&
        physical >= 0x0013E100u && physical < 0x0013E140u) {
        std::printf("[STRWATCH] phys=0x%08X pc=0x%08X inst=%llu\n",
            physical, regs_.pc, (unsigned long long)stats_.instructions);
    }
    if (kExamStrLogEnabled)
        TraceExamStrRead(physical, regs_.pc, regs_.pr, stats_.instructions);
    // Readback del registro de estado BCD (E2 2026-10-28): misma semantica
    // que en Read32 (referencia casio-cg50 MIT, cont.18e); la mitad baja del
    // status es 0x0000 en ambos estados del flag, igual que el truncado.
    if (physical >= 0x04CB0000u && physical < 0x04CB0100u &&
        ((physical - 0x04CB0000u) % 0x10u) == 0u) {
        const std::uint32_t status =
            bcdCarryLatch_ ? 0x10040000u : 0x00010000u;
        value = static_cast<std::uint16_t>(status & 0xFFFFu);
        return true;
    }
    if (cg50::Intc2Unit::MatchesPhysical(physical)) return intc2_.Read16(physical, value);
    if (cg50::AdcUnit::MatchesPhysical(physical)) return adc_.Read16(physical, value);
    if (cg50::TpuUnit::MatchesPhysical(physical)) return tpu_.Read16(physical, value);
    if (cg50::Scif0Unit::MatchesPhysical(physical)) return scif0_.Read16(physical, value);
    if (cg50::Usb2Unit::MatchesPhysical(physical)) return usb2_.Read16(physical, value);
    const auto* p = ReadPointer(address, 2);
    if (p) {
        value = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(p[0]) << 8) |
            static_cast<std::uint16_t>(p[1]));
        return true;
    }
    return peripheralBus_.Read16(physical, value) == cg50::PeripheralBus::Result::Accepted;
}

bool Cg50NativeCore::Read32(const std::uint32_t address, std::uint32_t& value) const
{
    if (kReadWinEnabled && stats_.instructions >= kReadWinLo &&
        stats_.instructions <= kReadWinHi && ReadWinPcMatches(regs_.pc) &&
        address != regs_.pc && address != regs_.pc + 2u) {
        std::uint64_t rv = 0;
        bool have = false;
        if (const auto* rp = ReadPointer(address, 4)) {
            rv = (static_cast<std::uint64_t>(rp[0]) << 24) |
                 (static_cast<std::uint64_t>(rp[1]) << 16) |
                 (static_cast<std::uint64_t>(rp[2]) << 8) | rp[3];
            have = true;
        }
        TraceReadWinValues(stats_.instructions, regs_.pc, regs_.pr, address, 4, rv, have);
    }
    if (kStrLocLogEnabled)
        TraceStringRegionRead(address, regs_.pc, regs_.pr, regs_.r[0], regs_.r[1]);
    if (kAppIfLogEnabled || kBufWrLogEnabled)
        TraceAppIfAccess(DirectMappedPhysicalAddress(address), "RD32", 0, regs_.pc);
    if (PeriphDiagEnabled()) {
        std::uint32_t observed = 0;
        if (QueueWatchReadValueNeeded(address, 4)) {
            if (const auto* qp = ReadPointer(address, 4)) {
                observed = (static_cast<std::uint32_t>(qp[0]) << 24) |
                           (static_cast<std::uint32_t>(qp[1]) << 16) |
                           (static_cast<std::uint32_t>(qp[2]) << 8) | qp[3];
            }
        }
        QueueWatchAccess("RD32", address, observed, 4, regs_.pc, regs_.pr,
                         stats_.instructions);
    }
    if (PeriphDiagEnabled() &&
        address >= 0x880A4410u && address <= 0x880A441Fu) {
        const auto* p = ReadPointer(address, 4);
        const unsigned fv = p ? ((static_cast<unsigned>(p[0]) << 24) |
                                 (static_cast<unsigned>(p[1]) << 16) |
                                 (static_cast<unsigned>(p[2]) << 8) | p[3]) : 0xFFFFFFFFu;
        std::printf("[FLAGRD32] addr=0x%08X val=0x%08X pc=0x%08X pr=0x%08X inst=%llu\n",
            address, fv, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                std::fprintf(kd, "[FLAGRD32] addr=0x%08X val=0x%08X pc=0x%08X pr=0x%08X inst=%llu\n",
                    address, fv, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    if (PeriphDiagEnabled() &&
        address >= 0x88090C00u && address <= 0x88090CCFu) {
        const auto* p = ReadPointer(address, 4);
        const unsigned ev = p ? ((static_cast<unsigned>(p[0]) << 24) |
                                 (static_cast<unsigned>(p[1]) << 16) |
                                 (static_cast<unsigned>(p[2]) << 8) | p[3]) : 0xFFFFFFFFu;
        std::printf("[EVTRD32] addr=0x%08X val=0x%08X pc=0x%08X pr=0x%08X inst=%llu\n",
            address, ev, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
        if (DiagFileEnabled()) {
            if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                std::fprintf(kd, "[EVTRD32] addr=0x%08X val=0x%08X pc=0x%08X pr=0x%08X inst=%llu\n",
                    address, ev, regs_.pc, regs_.pr, (unsigned long long)stats_.instructions);
                std::fclose(kd);
            }
        }
    }
    // 2026-10-29 M9: arrays MMIO de la UTLB ([0xF6000000, 0xF8000000)).
    // Interceptado ANTES del mapeo directo: P4 no deja derivar fisico para
    // esta ventana. Solo Read16/Read32 (8-bit no soportado, declarado).
    // Referencia mmu.go:216-258.
    if (address >= kItlbAddressArrayBase && address < kItlbArraysEnd) {
        value = ItlbArrayRead(address);
        return true;
    }
    if (address >= kUtlbAddressArrayBase && address < kUtlbArraysEnd) {
        value = UtlbArrayRead(address);
        return true;
    }
    const auto physical = DirectMappedPhysicalAddress(address);
    // CG50_WAKE_REASON=n (experimento N-1): fuerza el wake reason leido por
    // el despachador de despertar (0x801E240C) para probar que la tecla se
    // descarta por no identificarse como fuente de wake.
    if (physical == 0x88090CB4u) {
        static const int wakeOverride = []() {
            const char* e = std::getenv("CG50_WAKE_REASON");
            return e ? std::atoi(e) : -1;
        }();
        if (wakeOverride >= 0) {
            value = static_cast<std::uint32_t>(wakeOverride);
            return true;
        }
    }
    // TMU unit 0: TCOR at base + 0x08 + ch*0x0C, TCNT at + 0x0C + ch*0x0C.
    if (physical >= kTmu0PhysicalBase + 0x08u &&
        physical <= kTmu0PhysicalBase + 0x24u &&
        (((physical - kTmu0PhysicalBase - 0x08u) % 0x0Cu) == 0u ||
         ((physical - kTmu0PhysicalBase - 0x0Cu) % 0x0Cu) == 0u)) {
        const std::uint32_t fromCor = physical - kTmu0PhysicalBase - 0x08u;
        const std::uint32_t fromCnt = physical - kTmu0PhysicalBase - 0x0Cu;
        std::size_t ch = 0;
        bool isCnt = false;
        if ((fromCor % 0x0Cu) == 0u) {
            ch = static_cast<std::size_t>(fromCor / 0x0Cu);
        } else {
            ch = static_cast<std::size_t>(fromCnt / 0x0Cu);
            isCnt = true;
        }
        if (ch < tmu0_.size()) {
            value = isCnt ? tmu0_[ch].tcnt : tmu0_[ch].tcor;
            return true;
        }
    }
    if (address == kPtehRegister) { value = pteh_; return true; }
    if (address == kPtelRegister) { value = ptel_; return true; }
    if (address == kTtbRegister) { value = ttb_; return true; }
    if (address == kTeaRegister) { value = tea_; return true; }
    if (address == kPteaRegister) { value = ptea_; return true; }
    if (address == 0xFF000024u || physical == 0x1F000024u) {
        // EXPEVT real cuando hay una excepcion tomada (p.ej. FPU disable 0xE0,
        // que el handler del OS usa para indexar su tabla en 0xFD800FA8); en
        // el arranque sin excepcion se conserva el 0x20 historico (reset).
        value = regs_.expevt != 0
            ? regs_.expevt
            : (HasUserRom() ? 0x00000020u : 0x00000000u);
        return true;
    }
    if (physical == 0x08092D2Cu) {
        value = 0x02800000u;
        return true;
    }
    if (physical == 0x08092D30u) {
        value = 0x00010000u;
        return true;
    }
    if (address >= kBscRegisterBase && address <= kBscRegisterEnd &&
        ((address - kBscRegisterBase) % 4) == 0) {
        const std::size_t index = (address - kBscRegisterBase) / 4;
        value = bscRegisters_[index];
        return true;
    }
    if (physical == kFrequencyControlRegisterPhysical) {
        value = frequencyControl_;
        return true;
    }
    if (physical == kPllControlRegisterPhysical) {
        value = pllControl_;
        return true;
    }
    if (physical == kFllFrequencyRegisterPhysical) {
        value = fllFrequency_;
        return true;
    }
    if (physical == kClockChangeStatusRegisterPhysical) {
        value = clockChangeStatus_;
        return true;
    }
    if (physical == kFsiClockControlRegisterPhysical) {
        value = fsiClockControl_;
        return true;
    }
    if (physical == kDdClockControlRegisterPhysical) {
        value = ddClockControl_;
        return true;
    }
    if (physical == kUsbClockControlRegisterPhysical) {
        value = usbClockControl_;
        return true;
    }
    if (physical == kPll2ControlRegisterPhysical) {
        value = pll2Control_;
        return true;
    }
    if (physical == kSpuClockControlRegisterPhysical) {
        value = spuClockControl_;
        return true;
    }
    if (physical == kVpuClockControlRegisterPhysical) {
        value = vpuClockControl_;
        return true;
    }
    if (physical == kSpreadSpectrumControlRegisterPhysical) {
        value = spreadSpectrumControl_;
        return true;
    }
    if (physical == kCompareMatchTimerCounterRegisterPhysical) {
        value = compareMatchTimerCounter_;
        static std::uint64_t cntReads = 0;
        ++cntReads;
        if ((cntReads % 65536u) == 0u) {
            std::printf("[CMT] R32 counter=0x%08X pc=0x%08X inst=%llu n=%llu\n",
                value, regs_.pc, (unsigned long long)stats_.instructions,
                (unsigned long long)cntReads);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("lcd_diag.log", "a")) {
                    std::fprintf(kd, "[CMT] R32 counter=0x%08X pc=0x%08X inst=%llu n=%llu\n",
                        value, regs_.pc, (unsigned long long)stats_.instructions,
                        (unsigned long long)cntReads);
                    std::fclose(kd);
                }
            }
        }
        return true;
    }
    if (physical == kCompareMatchTimerConstantRegisterPhysical) {
        value = compareMatchTimerConstant_;
        return true;
    }
    if (physical ==
        kModuleStopControlRegister0Physical) {
        value = moduleStopControl0_;
        return true;
    }
    if (physical == kStandbyControlRegisterPhysical) {
        value = standbyControl_;
        return true;
    }
    if (physical == kModuleStopControlRegister2Physical) {
        value = moduleStopControl2_;
        return true;
    }
    if (address == kCacheControlRegister) {
        value = cacheControl_;
        return true;
    }
    if (address == kMmuControlRegister) {
        value = mmuControl_;
        return true;
    }
    if (address == kTraRegister) {
        value = regs_.tra;
        return true;
    }
    if (address == kExpevtRegister) {
        value = regs_.expevt;
        return true;
    }
    if (address == kIntevtRegister) {
        value = regs_.intevt;
        return true;
    }
    if (address == kCpuopmRegister) {
        value = cpuopm_;
        return true;
    }
    if (address == kExpmaskRegister) {
        value = expmask_;
        return true;
    }
    if (address == kPvrRegister || address == kPrrRegister) {
        value = 0;
        return true;
    }
    if (address == kQacr0Register) {
        value = qacr0_;
        return true;
    }
    if (address == kQacr1Register) {
        value = qacr1_;
        return true;
    }
    if (cg50::Intc2Unit::MatchesPhysical(physical)) return intc2_.Read32(physical, value);
    if (cg50::EtmuUnit::MatchesPhysical(physical)) return etmu_.Read32(physical, value);
    std::size_t maskIndex = 0;
    if (InterruptMaskRegisterIndex(physical, maskIndex)) {
        value = interruptMask_[maskIndex];
        return true;
    }
    if (InterruptMaskClearRegisterIndex(physical, maskIndex)) {
        value = 0;
        return true;
    }
    // (El status CFI 0x88 se aplica SOLO en Read16: los sondeos del driver
    // ILRAM usan mov.w; las copias usan mov.l/mov.b y deben leer el dato real.)
    // Watch de lecturas del string "System ERROR" (N-8): localiza el renderer.
    if (PeriphDiagEnabled() &&
        physical >= 0x0013E100u && physical < 0x0013E140u) {
        std::printf("[STRWATCH] phys=0x%08X pc=0x%08X inst=%llu\n",
            physical, regs_.pc, (unsigned long long)stats_.instructions);
    }
    if (kExamStrLogEnabled)
        TraceExamStrRead(physical, regs_.pc, regs_.pr, stats_.instructions);
    // Readback del registro de estado BCD (E2 2026-10-28, siguiendo la
    // referencia casio-cg50 MIT read case 0x0, cont.18e): el flag compartido
    // es observable leyendo cualquier alias +0x0 (mod 0x10) del bloque
    // 0x04CB0000: flag!=0 -> 0x10040000, flag=0 -> 0x00010000. La read de
    // 16 bits devuelve la mitad baja (0x0000), igual que el hardware.
    if (physical >= 0x04CB0000u && physical < 0x04CB0100u &&
        ((physical - 0x04CB0000u) % 0x10u) == 0u) {
        value = bcdCarryLatch_ ? 0x10040000u : 0x00010000u;
        return true;
    }
    const auto* p = ReadPointer(address, 4);
    if (p) {
        // Watch de lectura del origen de la copia de tabla slot-1 (N-8).
        if (PeriphDiagEnabled() &&
            physical >= 0x01680000u && physical < 0x01680008u) {
            std::printf("[WATCH-RD168] addr=0x%08X phys=0x%08X pc=0x%08X val=%02X%02X%02X%02X inst=%llu\n",
                address, physical, regs_.pc, p[0], p[1], p[2], p[3],
                (unsigned long long)stats_.instructions);
        }
        value = (static_cast<std::uint32_t>(p[0]) << 24) |
                (static_cast<std::uint32_t>(p[1]) << 16) |
                (static_cast<std::uint32_t>(p[2]) << 8) |
                static_cast<std::uint32_t>(p[3]);
        return true;
    }
    if (physical == kLcdBase) {
        std::uint16_t high = 0, low = 0;
        if (peripheralBus_.Read16(physical, high) == cg50::PeripheralBus::Result::Accepted &&
            peripheralBus_.Read16(physical, low) == cg50::PeripheralBus::Result::Accepted) {
            value = (static_cast<std::uint32_t>(high) << 16) | low;
            return true;
        }
    }
    return false;
}

bool Cg50NativeCore::Write8(const std::uint32_t address, const std::uint8_t value)
{
    if (kWrWinEnabled && stats_.instructions >= kWrWinLo &&
        stats_.instructions <= kWrWinHi) {
        const std::uint32_t phys = DirectMappedPhysicalAddress(address);
        if (WrWinAddrMatches(phys))
            TraceWrWinValues(stats_.instructions, regs_.pc, regs_.pr, phys, 1, value);
    }
    if (kAppIfLogEnabled || kBufWrLogEnabled)
        TraceAppIfAccess(DirectMappedPhysicalAddress(address), "WR8", value, regs_.pc);
    if (kResultWatchEnabled)
        TraceResultWatch("WR8", DirectMappedPhysicalAddress(address), value,
                         regs_.pc, stats_.instructions);
    if (PeriphDiagEnabled()) {
        QueueWatchAccess("WR8", address, value, 1, regs_.pc, regs_.pr,
                         stats_.instructions);
        if ((address >= 0x880A4410u && address <= 0x880A4413u) || (address >= 0x880A156Cu && address <= 0x880A156Fu) || (address >= 0x88090CB4u && address <= 0x88090CB7u)) {
            std::printf("[WATCH-FLAG] addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[WATCH-FLAG] addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                        address, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (((regs_.pc >= 0x801E7C00u && regs_.pc < 0x801E9500u) || (regs_.pc >= 0x802C2000u && regs_.pc < 0x802C4000u))) {
            std::printf("[ISRWR8] addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[ISRWR8] addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                        address, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (address == 0xFD8016ABu) {
            std::printf("[WRITE_FD8016AB] PC=0x%08X PR=0x%08X val=0x%02X inst=%llu\n",
                regs_.pc, regs_.pr, value, (unsigned long long)stats_.instructions);
        }
        if (DirectMappedPhysicalAddress(address) >= 0x044A0000u &&
            DirectMappedPhysicalAddress(address) < 0x044A0100u) {
            std::printf("[LCDA8] W addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                DirectMappedPhysicalAddress(address), value, regs_.pc,
                (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("lcd_diag.log", "a")) {
                    std::fprintf(kd, "[LCDA8] W addr=0x%08X val=0x%02X pc=0x%08X inst=%llu\n",
                        DirectMappedPhysicalAddress(address), value, regs_.pc,
                        (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
    }
    const auto physical = DirectMappedPhysicalAddress(address);
    if (physical == kTmu0PhysicalBase + 0x04u) {  // TSTR0
        for (std::size_t ch = 0; ch < tmu0_.size(); ++ch) {
            tmu0_[ch].running = (value & (1u << ch)) != 0;
        }
        return true;
    }
    if (physical == kRtc64HzCounterRegisterPhysical) {
        return true;
    }
    if (physical >= kRtcRegisterPhysicalBase + 0x02u &&
        physical <= kRtcRegisterPhysicalBase + 0x0Eu &&
        (physical & 1u) == 0u) {
        // Guest programming the RTC: mirror the byte into the peripheral
        // page (coherent dumps) and re-anchor the virtual clock.
        if (auto* page = WritePointer(physical, 1)) *page = value;
        RtcReanchorFromComponent(physical - kRtcRegisterPhysicalBase,
                                 static_cast<std::uint8_t>(value));
        return true;
    }
    if (physical >= kPfcBasePhysical && physical <= kPfcEndPhysical) {
        const std::size_t off = physical - kPfcBasePhysical;
        pfcStorage_[off] = value;
        UpdatePfcDataRegister(physical, value);
        if (physical == kPortPPullControlRegisterPhysical) {
            portPPullControl_ = static_cast<std::uint8_t>(value & kPortPPullControlWritableMask);
        }
        return true;
    }
    std::size_t maskIndex = 0;
    if (InterruptMaskRegisterIndex(physical, maskIndex)) {
        const std::uint8_t mask = (maskIndex == 5) ? 0xF7u : 0xFFu;
        interruptMask_[maskIndex] |= static_cast<std::uint8_t>(value & mask);
        if (maskIndex == 5) interruptMask5_ = interruptMask_[5];
        return true;
    }
    if (InterruptMaskClearRegisterIndex(physical, maskIndex)) {
        interruptMask_[maskIndex] &= static_cast<std::uint8_t>(~value);
        if (maskIndex == 5) interruptMask5_ = interruptMask_[5];
        return true;
    }
    if (address == kCpuopmRegister) {
        cpuopm_ = (static_cast<std::uint32_t>(value) & kCpuopmWritableMask) | kCpuopmForcedMask;
        return true;
    }
    if (address == kExpmaskRegister) {
        expmask_ = static_cast<std::uint32_t>(value & kExpmaskWritableMask);
        return true;
    }
    if (cg50::Intc2Unit::MatchesPhysical(physical)) return intc2_.Write8(physical, value);
    if (cg50::EtmuUnit::MatchesPhysical(physical)) return etmu_.Write8(physical, value);
    if (cg50::AdcUnit::MatchesPhysical(physical)) return adc_.Write8(physical, value);
    if (cg50::TpuUnit::MatchesPhysical(physical)) return tpu_.Write8(physical, value);
    if (cg50::Scif0Unit::MatchesPhysical(physical)) return scif0_.Write8(physical, value);
    if (cg50::Usb2Unit::MatchesPhysical(physical)) return usb2_.Write8(physical, value);
    auto* p = WritePointer(address, 1);
    if (p) {
        *p = value;
        return true;
    }
    if (physical >= kRomPhysicalBase && physical < kRomPhysicalBase + rom_.size()) {
        if (physical >= kSystemParameterPhysicalBase && value != 0xF0u) {
            rom_[physical - kRomPhysicalBase] = value;
        }
        return true;
    }
    std::size_t c2OffsetW8 = 0;
    if (!c2Rom_.empty() && C2StorageOffset(physical, 1, c2Rom_.size(), c2OffsetW8)) {
        if (!flashCfi_.IsReadArray()) {
            if (value == 0xF0u) flashCfi_.Reset();
            return true;
        }
        if (!kFlashNoSwallow && value == 0xF0u) return true;
        c2Rom_[c2OffsetW8] = value;
        return true;
    }
    std::size_t userOffsetW8 = 0;
    if (!userRom_.empty() &&
        UserRomStorageOffset(physical, 1, userRom_.size(), userOffsetW8)) {
        if (!flashCfi_.IsReadArray()) {
            if (value == 0xF0u) flashCfi_.Reset();
            return true;
        }
        // CG50_STORE_LOG=1: registra intentos de escritura al storage (N-8).
        static const bool storeLogW = std::getenv("CG50_STORE_LOG") != nullptr;
        if (storeLogW) {
            std::fprintf(stderr, "[STOREWR] phys=%08X off=%06zX bytes=1 val=%02X pc=%08X inst=%llu\n",
                         physical, userOffsetW8, value, regs_.pc,
                         (unsigned long long)stats_.instructions);
        }
        // Hack historico (baseline): traga escrituras de valor 0xF0 (comando
        // flash read/reset). El oficial conserva FF FF en 0x120000 tras el
        // arranque, luego el array no se modifica con comandos. Con
        // CG50_FLASH_NO_SWALLOW=1 la escritura pasa (solo experimentos).
        static const bool noSwallow = std::getenv("CG50_FLASH_NO_SWALLOW") != nullptr;
        if (!noSwallow && value == 0xF0u) return true;
        userRom_[userOffsetW8] = value;
        return true;
    }
    return false;
}

bool Cg50NativeCore::Write16(const std::uint32_t address, const std::uint16_t value)
{
    const auto physical = DirectMappedPhysicalAddress(address);
    if (kWrWinEnabled && stats_.instructions >= kWrWinLo &&
        stats_.instructions <= kWrWinHi && WrWinAddrMatches(physical))
        TraceWrWinValues(stats_.instructions, regs_.pc, regs_.pr, physical, 2, value);
    if (kAppIfLogEnabled || kBufWrLogEnabled)
        TraceAppIfAccess(physical, "WR16", value, regs_.pc);
    if (kResultWatchEnabled)
        TraceResultWatch("WR16", physical, value, regs_.pc, stats_.instructions);
    if (PeriphDiagEnabled())
        QueueWatchAccess("WR16", address, value, 2, regs_.pc, regs_.pr,
                         stats_.instructions);
    // 2026-10-29 M9: arrays MMIO de la UTLB, escritura ([0xF6000000,
    // 0xF8000000)) ANTES del mapeo directo. PTEA (>=0xF7800000): aceptar y
    // descartar (sin backing, declarado). Solo Write16/Write32 (8-bit no,
    // declarado). mmu.go:216-258.
    if (address >= kItlbAddressArrayBase && address < kItlbArraysEnd) {
        ItlbArrayWrite(address, value);
        return true;
    }
    if (address >= kUtlbAddressArrayBase && address < kUtlbArraysEnd) {
        UtlbArrayWrite(address, value);
        return true;
    }
    if (cg50::Intc2Unit::MatchesPhysical(physical)) return intc2_.Write16(physical, value);
    if (cg50::AdcUnit::MatchesPhysical(physical)) return adc_.Write16(physical, value);
    if (cg50::TpuUnit::MatchesPhysical(physical)) return tpu_.Write16(physical, value);
    if (cg50::Scif0Unit::MatchesPhysical(physical)) return scif0_.Write16(physical, value);
    if (cg50::Usb2Unit::MatchesPhysical(physical)) return usb2_.Write16(physical, value);
    if (PeriphDiagEnabled()) {
        if (physical >= 0x044A0000u && physical < 0x044A0100u) {
            std::printf("[LCDA16] W addr=0x%08X val=0x%04X pc=0x%08X inst=%llu\n",
                physical, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("lcd_diag.log", "a")) {
                    std::fprintf(kd, "[LCDA16] W addr=0x%08X val=0x%04X pc=0x%08X inst=%llu\n",
                        physical, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (address >= 0x880A4410u && address <= 0x880A441Fu) {
            std::printf("[WATCH-FLAG16] addr=0x%08X val=0x%04X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[WATCH-FLAG16] addr=0x%08X val=0x%04X pc=0x%08X inst=%llu\n",
                        address, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (((regs_.pc >= 0x801E7C00u && regs_.pc < 0x801E9500u) || (regs_.pc >= 0x802C2000u && regs_.pc < 0x802C4000u))) {
            std::printf("[ISRWR16] addr=0x%08X val=0x%04X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[ISRWR16] addr=0x%08X val=0x%04X pc=0x%08X inst=%llu\n",
                        address, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        // Watch de escritores del framebuffer (N-8): primeros writes + dedupe por PC.
        if (physical >= 0x08000000u && physical < 0x08028800u) {
            static int fbCount16 = 0;
            static bool fbSeen16[65536] = {};
            const std::uint32_t key = regs_.pc & 0xFFFFu;
            if (fbCount16 < 5) {
                ++fbCount16;
                std::printf("[FBWR16-first] pc=0x%08X phys=0x%08X val=0x%04X inst=%llu\n",
                    regs_.pc, physical, value, (unsigned long long)stats_.instructions);
            } else if (!fbSeen16[key]) {
                fbSeen16[key] = true;
                std::printf("[FBWR16] pc=0x%08X phys=0x%08X inst=%llu\n",
                    regs_.pc, physical, (unsigned long long)stats_.instructions);
            }
        }
    }
    // NOR Flash CFI 16-bit commands are handled deterministically in the Flash
    // range branch below via SpansionNorFlashCfi (Sector Erase 128 KB,
    // Write-to-Buffer 0x25..0x29, Word Program 0xA0, Autoselect 0x90,
    // CFI Query 0x98, Reset/Abort 0xF0).
    // Unidad BCD ALU del SH7305 (0xA4CB0000, fisica 0x04CB0000): el driver
    // 80072Bxx-80073xxx escribe el comando en 0x10 (WR2) y los operandos en
    // 0x14/0x18 (WR4) y lee el resultado de 0x1C (80072C4C). Sin esta unidad
    // el core devolvia 0 (pagina generica de ceros): resultado de calculo 0
    // y "No Solution" en el guest.
    //
    // Modelo de hardware (referencia casio-cg50, RE cont.18c + probe
    // on-device cont.18e - verdad de hardware): decode
    //   op    = (cmd & 1) ? add : sub
    //   fin   = (cmd & 4) ? 1 : ((cmd & 2) ? latch : 0)
    //   0=A-B 1=A+B 2=A-B-flag 3=A+B+flag 4=A-B-1 5=A+B+1; bit3 ignorado.
    // LATCH unico compartido de acarreo/prestamo (multi-palabra): el
    // borrow-out de una resta se consume como carry-in de la suma
    // siguiente (FUN_80072f80 procesa LSW-first con cmd1/cmd3).
    if (physical >= 0x04CB0000u && physical < 0x04CB0100u) {
        if (physical == 0x04CB0010u) {
            std::uint32_t a = 0, b = 0;
            Read32(0xA4CB0014u, a);
            Read32(0xA4CB0018u, b);
            const bool isAdd = (value & 0x1u) != 0u;
            std::uint32_t flagIn = 0;
            if ((value & 0x4u) != 0u) flagIn = 1u;
            else if ((value & 0x2u) != 0u) flagIn = bcdCarryLatch_;
            std::uint32_t r = 0;
            bool out = false;
            if (isAdd) {
                for (int nib = 0; nib < 8; ++nib) {
                    const std::uint32_t sh = static_cast<std::uint32_t>(nib) * 4u;
                    std::uint32_t s = ((a >> sh) & 0xFu) + ((b >> sh) & 0xFu)
                                    + flagIn;
                    if (s > 9u) { s -= 10u; out = true; } else { out = false; }
                    // Acarreo intra-palabra (E2 2026-10-28): el carry-out de un
                    // nibble es el carry-in del siguiente, igual que bcdAdd/
                    // bcdSub del modelo casio-cg50 (MIT, cont.18c/18e). El
                    // flag-in del comando solo entra al PRIMER nibble y el
                    // latch multipalabra sigue tomando el out del ultimo.
                    flagIn = out ? 1u : 0u;
                    r |= (s & 0xFu) << sh;
                }
            } else {
                for (int nib = 0; nib < 8; ++nib) {
                    const std::uint32_t sh = static_cast<std::uint32_t>(nib) * 4u;
                    std::uint32_t d = ((a >> sh) & 0xFu);
                    std::uint32_t s = ((b >> sh) & 0xFu) + flagIn;
                    if (d < s) { d += 10u - s; out = true; }
                    else { d -= s; out = false; }
                    flagIn = out ? 1u : 0u;
                    r |= (d & 0xFu) << sh;
                }
            }
            bcdCarryLatch_ = out ? 1u : 0u;
            Write32(0xA4CB001Cu, r);
            // El bloque aliasa cada 0x10: el resultado tambien se expone en
            // el alias base +0xC (read case 0xC del modelo casio-cg50 MIT).
            Write32(0xA4CB000Cu, r);
            if (PeriphDiagEnabled()) {
                std::printf("[BCDALU] cmd=%04X A=%08X B=%08X latchIn=%u -> "
                            "R=%08X latchOut=%u inst=%llu\n",
                    value, a, b,
                    (value & 0x4u) != 0u ? 1u : ((value & 0x2u) != 0u ? bcdCarryLatch_ : 0u),
                    r, bcdCarryLatch_ ? 1u : 0u,
                    static_cast<unsigned long long>(stats_.instructions));
            }
        }
        if (auto* page = WritePointer(physical, 2)) {
            page[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            page[1] = static_cast<std::uint8_t>(value & 0xFFu);
        }
        return true;
    }
    // TMU unit 0: TCR at base + 0x10 + ch*0x0C. UNF (bit 8) is
    // write-zero-to-clear; a written 1 retains the current flag.
    if (physical >= kTmu0PhysicalBase + 0x10u &&
        physical <= kTmu0PhysicalBase + 0x28u &&
        ((physical - kTmu0PhysicalBase - 0x10u) % 0x0Cu) == 0u) {
        const std::size_t ch =
            static_cast<std::size_t>((physical - kTmu0PhysicalBase - 0x10u) / 0x0Cu);
        if (ch < tmu0_.size()) {
            const auto oldFlag =
                static_cast<std::uint16_t>(tmu0_[ch].tcr & kTmuTcrUnderflowFlag);
            tmu0_[ch].tcr = static_cast<std::uint16_t>(
                (value & 0x00FFu) | (oldFlag & (value & 0x0100u)));
            return true;
        }
    }
    if (physical >= kRtcRegisterPhysicalBase + 0x02u &&
        physical <= kRtcRegisterPhysicalBase + 0x0Eu &&
        (physical & 1u) == 0u) {
        if (auto* page = WritePointer(physical, 2)) {
            page[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            page[1] = static_cast<std::uint8_t>(value & 0xFFu);
        }
        // 16-bit guest writes program the semantic byte in the low half
        // (matching the derived 16-bit read convention).
        RtcReanchorFromComponent(physical - kRtcRegisterPhysicalBase,
                                 static_cast<std::uint8_t>(value & 0xFFu));
        return true;
    }
    if (physical == kCompareMatchTimerStartRegisterPhysical) {
        const auto newValue = static_cast<std::uint16_t>(
            value & kCompareMatchTimerStartWritableMask);
        if (newValue != compareMatchTimerStart_) compareMatchTimerPrescaler_ = 0;
        compareMatchTimerStart_ = newValue;
        return true;
    }
    if (physical == kCompareMatchTimerControlRegisterPhysical) {
        // CPU73050 RVA 0x2B5CF: regular fields use mask 0x03B7; status
        // bits 15/14 are write-zero-to-clear and bit 13 is retained while a
        // scheduled counter write is pending.  This cycle-synchronous core
        // commits counter writes immediately, so it never fabricates busy.
        const auto retainedStatus = static_cast<std::uint16_t>(
            compareMatchTimerControl_ &
            static_cast<std::uint16_t>(value | kCompareMatchTimerWriteBusyFlag) &
            0xE000u);
        compareMatchTimerControl_ = static_cast<std::uint16_t>(
            (value & kCompareMatchTimerControlWritableMask) | retainedStatus);
        compareMatchTimerPrescaler_ = 0;
        return true;
    }
    if (physical == kWatchdogCounterRegisterPhysical) {
        if ((value & 0xFF00u) != 0x5A00u) return true;
        // CPU73050 schedules the backing update and exposes bit 5 as busy.
        // The functional core commits synchronously, so no artificial busy
        // interval is retained.
        if ((watchdogControl_ & 0x20u) == 0)
            watchdogCounter_ = static_cast<std::uint8_t>(value & 0xFFu);
        return true;
    }
    if (physical == kWatchdogControlRegisterPhysical) {
        if ((value & 0xFF00u) != kWatchdogControlWriteKey) return true;
        const auto requested = static_cast<std::uint8_t>(value & 0xFFu);
        // FIX F1-A (2026-10-21, Servulus): el SH7305 real permite encender
        // TME (bit 5) y WT/IT (bit 6) por escritura con la key 0x5A. La
        // formula anterior ((requested|0x20) & old) & 0x38 congelaba TME a
        // su valor previo: si el OS encendia el intervalo (TME 0->1), el bit
        // se descartaba silenciosamente -> el WDT nunca disparaba la IRQ
        // 0x560 -> el publicador de eventos del OS (handler 0x801E20FC)
        // nunca publicaba -> el interprete MW+ jamas se despachaba -> toda
        // expresion evaluaba a 0. Se toman del request los bits de control
        // (0-2 CKS, 5 TME, 6 WT/IT, 7) y se retienen del estado anterior
        // solo los bits de estado 3-4.
        const auto retainedStatus = static_cast<std::uint8_t>(
            watchdogControl_ & 0x18u);
        watchdogControl_ = static_cast<std::uint8_t>(
            (requested & 0xE7u) | retainedStatus);
        return true;
    }
    if (physical >= kPfcBasePhysical && physical + 1 <= kPfcEndPhysical && ((physical & 1) == 0)) {
        const std::size_t off = physical - kPfcBasePhysical;
        const auto highByte = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        const auto lowByte = static_cast<std::uint8_t>(value & 0xFFu);
        pfcStorage_[off] = highByte;
        pfcStorage_[off + 1] = lowByte;
        UpdatePfcDataRegister(physical, highByte);
        UpdatePfcDataRegister(physical + 1, lowByte);
        if (physical == kPortMControlRegisterPhysical) {
            portMControl_ = static_cast<std::uint16_t>(value & kPortMControlWritableMask);
        } else if (physical == kPortNControlRegisterPhysical) {
            portNControl_ = static_cast<std::uint16_t>(value & kPortNControlWritableMask);
        } else if (physical == kPortPControlRegisterPhysical) {
            portPControl_ = static_cast<std::uint16_t>(value & kPortPControlWritableMask);
        }
        return true;
    }
    if (address >= kSdramModeRegister2Base && address <= kSdramModeRegister3End) {
        return true;
    }
    std::size_t keyRegisterIndex = 0;
    if (KeyInterfaceRegisterIndex(
            physical, keyRegisterIndex)) {
        if (keyRegisterIndex < kKeyInterfaceDataRegisterCount)
            return false; // KIUDATA0..5 are read-only.

        const auto controlIndex = keyRegisterIndex - kKeyInterfaceDataRegisterCount;
        switch (controlIndex) {
        case 0: // KIUCNTREG
            keyInterfaceControl_[controlIndex] = value & 0x8003u;
            return true;
        case 1: // KIAUTOFIXREG
            keyInterfaceControl_[controlIndex] = value & 0x8077u;
            return true;
        case 2: // KIUMODEREG
            keyInterfaceControl_[controlIndex] = value & 0x0E00u;
            return true;
        case 3: // KIUSTATEREG is computed/read-only.
            return false;
        case 4: { // KIUINTREG: low flags are write-one-to-clear (W1C).
            const auto oldValue = keyInterfaceControl_[controlIndex];
            const auto retainedLow = static_cast<std::uint16_t>(
                oldValue & static_cast<std::uint16_t>(~value) & 0x007Fu);
            keyInterfaceControl_[controlIndex] = static_cast<std::uint16_t>(
                (value & 0xEF00u) | retainedLow);
            return true;
        }
        case 5: // KIUWSETREG
            keyInterfaceControl_[controlIndex] = value & 0xEFFFu;
            return true;
        case 6: // KIUINTERVALREG
            keyInterfaceControl_[controlIndex] = value;
            return true;
        case 7: // KOUTPINSET
            keyInterfaceControl_[controlIndex] = value & 0x0FFFu;
            return true;
        case 8: // KINPINSET
            keyInterfaceControl_[controlIndex] = value & 0x00FFu;
            return true;
        default:
            return false;
        }
    }
    std::size_t priorityIndex = 0;
    if (InterruptPriorityRegisterIndex(
            physical, priorityIndex)) {
        interruptPriority_[priorityIndex] = value;
        return true;
    }
    std::size_t maskIndex = 0;
    if (InterruptMaskRegisterIndex(physical, maskIndex)) {
        const std::uint8_t mask = (maskIndex == 5) ? 0xF7u : 0xFFu;
        interruptMask_[maskIndex] |= static_cast<std::uint8_t>(value & mask);
        if (maskIndex == 5) interruptMask5_ = interruptMask_[5];
        return true;
    }
    if (InterruptMaskClearRegisterIndex(physical, maskIndex)) {
        interruptMask_[maskIndex] &= static_cast<std::uint8_t>(~(value & 0xFFu));
        if (maskIndex == 5) interruptMask5_ = interruptMask_[5];
        return true;
    }
    if (address == kCacheControlRegister) {
        cacheControl_ = value & static_cast<std::uint16_t>(kCacheControlWritableMask);
        return true;
    }
    if (address == kCpuopmRegister) {
        cpuopm_ = (static_cast<std::uint32_t>(value) & kCpuopmWritableMask) | kCpuopmForcedMask;
        return true;
    }
    if (address == kExpmaskRegister) {
        expmask_ = static_cast<std::uint32_t>(value & kExpmaskWritableMask);
        return true;
    }
    if (physical == kVpuClockControlRegisterPhysical) {
        vpuClockControl_ = (vpuClockControl_ & 0xFFFF0000u) | value;
        return true;
    }
    auto* p = WritePointer(address, 2);
    if (p) {
        p[0] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        p[1] = static_cast<std::uint8_t>(value & 0xFFu);
        return true;
    }
    if (IsFlashPhysicalAddress(physical)) {
        const bool handledByCfi = flashCfi_.HandleWrite16(
            physical, value,
            [this](const std::uint32_t phys, const std::uint16_t word) noexcept {
                ProgramFlashWord16(phys, word);
            },
            [this](const std::uint32_t sectorBase) noexcept {
                EraseFlashSector(sectorBase);
            });
        if (handledByCfi) {
            return true;
        }
        // Direct non-CFI 16-bit write fallback in ReadArray mode (for legacy tests).
        if (physical >= kRomPhysicalBase && physical + 1 < kRomPhysicalBase + rom_.size()) {
            if (physical >= kSystemParameterPhysicalBase && (value & 0xFFu) != 0xF0u) {
                const std::size_t off = physical - kRomPhysicalBase;
                rom_[off] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
                rom_[off + 1] = static_cast<std::uint8_t>(value & 0xFFu);
            }
            return true;
        }
        std::size_t c2OffsetW16 = 0;
        if (!c2Rom_.empty() && C2StorageOffset(physical, 2, c2Rom_.size(), c2OffsetW16)) {
            c2Rom_[c2OffsetW16] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            c2Rom_[c2OffsetW16 + 1] = static_cast<std::uint8_t>(value & 0xFFu);
            return true;
        }
        std::size_t userOffsetW16 = 0;
        if (!userRom_.empty() &&
            UserRomStorageOffset(physical, 2, userRom_.size(), userOffsetW16)) {
            if (kStoreLogEnabled) {
                std::fprintf(stderr, "[STOREWR] phys=%08X off=%06zX bytes=2 val=%04X pc=%08X inst=%llu\n",
                             physical, userOffsetW16, value, regs_.pc,
                             (unsigned long long)stats_.instructions);
            }
            if (!kFlashNoSwallow && (value & 0xFFu) == 0xF0u) return true;
            const std::size_t off = userOffsetW16;
            userRom_[off] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            userRom_[off + 1] = static_cast<std::uint8_t>(value & 0xFFu);
            return true;
        }
        return true;
    }
    return peripheralBus_.Write16(physical, value) == cg50::PeripheralBus::Result::Accepted;
}

bool Cg50NativeCore::Write32(const std::uint32_t address, const std::uint32_t value)
{
    if (kWrWinEnabled && stats_.instructions >= kWrWinLo &&
        stats_.instructions <= kWrWinHi) {
        const std::uint32_t phys = DirectMappedPhysicalAddress(address);
        if (WrWinAddrMatches(phys))
            TraceWrWinValues(stats_.instructions, regs_.pc, regs_.pr, phys, 4, value);
    }
    if (kAppIfLogEnabled || kBufWrLogEnabled)
        TraceAppIfAccess(DirectMappedPhysicalAddress(address), "WR32", value, regs_.pc);
    if (kResultWatchEnabled)
        TraceResultWatch("WR32", DirectMappedPhysicalAddress(address), value,
                         regs_.pc, stats_.instructions);
    if (PeriphDiagEnabled()) {
        QueueWatchAccess("WR32", address, value, 4, regs_.pc, regs_.pr,
                         stats_.instructions);
        if (DirectMappedPhysicalAddress(address) >= 0x044A0000u &&
            DirectMappedPhysicalAddress(address) < 0x044A0100u) {
            std::printf("[LCDA32] W addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                DirectMappedPhysicalAddress(address), value, regs_.pc,
                (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("lcd_diag.log", "a")) {
                    std::fprintf(kd, "[LCDA32] W addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                        DirectMappedPhysicalAddress(address), value, regs_.pc,
                        (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (address >= 0x880A4410u && address <= 0x880A441Fu) {
            std::printf("[WATCH-FLAG32] addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[WATCH-FLAG32] addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                        address, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (((regs_.pc >= 0x801E7C00u && regs_.pc < 0x801E9500u) || (regs_.pc >= 0x802C2000u && regs_.pc < 0x802C4000u))) {
            std::printf("[ISRWR32] addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[ISRWR32] addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                        address, value, regs_.pc, (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        // Watch de campos de la estructura del scan FS (N-8).
        if (address == 0x880CE2E0u || address == 0x880CE2E4u ||
            address == 0x880CE2DCu || address == 0x880D3470u) {
            std::printf("[WATCH-32] addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
        }
        // Watch de la tabla slot-1 del FS (N-8).
        if (address >= 0x8809BFA4u && address < 0x8809BFA8u) {
            std::printf("[WATCH-BFA4] addr=0x%08X val=0x%08X pc=0x%08X inst=%llu\n",
                address, value, regs_.pc, (unsigned long long)stats_.instructions);
        }
        // Watch de escritores del framebuffer (N-8): un log por PC unico.
        if (DirectMappedPhysicalAddress(address) >= 0x08000000u &&
            DirectMappedPhysicalAddress(address) < 0x08028800u) {
            static bool fbSeen[65536] = {};
            const std::uint32_t key = regs_.pc & 0xFFFFu;
            const std::uint32_t region = (regs_.pc >> 16) & 0xFFFFu;
            if (!fbSeen[key] && region >= 0x8000u) {
                fbSeen[key] = true;
                std::printf("[FBWR] pc=0x%08X phys=0x%08X inst=%llu\n",
                    regs_.pc, DirectMappedPhysicalAddress(address),
                    (unsigned long long)stats_.instructions);
            }
        }
        if (address == 0xFD801D40u) {
            std::printf("[WRITE_FD801D40] PC=0x%08X PR=0x%08X val=0x%08X inst=%llu\n",
                regs_.pc, regs_.pr, value, (unsigned long long)stats_.instructions);
        }
        if (address == 0xFD8017B4u) {
            std::printf("[WRITE_17B4] PC=0x%08X PR=0x%08X val=0x%08X inst=%llu\n",
                regs_.pc, regs_.pr, value, (unsigned long long)stats_.instructions);
        }
    }
    if (address == 0xFD801D40u) {
        // Hack historico (baseline): convertia el 0 del OS en 00 00 00 01.
        // El oficial termina con 0 en 0xFD801D40 (GUEST_DUMP ILRAM idle,
        // 2026-10-18); el hack queda disponible solo con CG50_1D40_LEGACY=1.
        static const bool legacy1d40 = std::getenv("CG50_1D40_LEGACY") != nullptr;
        if (legacy1d40 && value == 0 && HasUserRom()) {
            onChipRam_[0x1D40] = 0x00;
            onChipRam_[0x1D41] = 0x00;
            onChipRam_[0x1D42] = 0x00;
            onChipRam_[0x1D43] = 0x01;
            return true;
        }
    }
    const auto physical = DirectMappedPhysicalAddress(address);
    if (address == kPtehRegister) { pteh_ = value & 0x1FFFFCFFu; return true; }
    // TMU unit 0: TCOR at base + 0x08 + ch*0x0C, TCNT at + 0x0C + ch*0x0C.
    if (physical >= kTmu0PhysicalBase + 0x08u &&
        physical <= kTmu0PhysicalBase + 0x24u &&
        (((physical - kTmu0PhysicalBase - 0x08u) % 0x0Cu) == 0u ||
         ((physical - kTmu0PhysicalBase - 0x0Cu) % 0x0Cu) == 0u)) {
        const std::uint32_t fromCor = physical - kTmu0PhysicalBase - 0x08u;
        const std::uint32_t fromCnt = physical - kTmu0PhysicalBase - 0x0Cu;
        std::size_t ch = 0;
        bool isCnt = false;
        if ((fromCor % 0x0Cu) == 0u) {
            ch = static_cast<std::size_t>(fromCor / 0x0Cu);
        } else {
            ch = static_cast<std::size_t>(fromCnt / 0x0Cu);
            isCnt = true;
        }
        if (ch < tmu0_.size()) {
            if (isCnt) tmu0_[ch].tcnt = value;
            else tmu0_[ch].tcor = value;
            return true;
        }
    }
    // 2026-10-29 M7: mascara writable de PTEL corregida 0x1FFFF3FF ->
    // 0x1FFFF1FF (el bit 9 del data word es reservado FFC, mmu.go:34-:
    // PPN 28:10, V 8, SZ1 7, PR 6:5, SZ0 4, C 3, D 2, SH 1, WT 0).
    if (address == kPtelRegister) { ptel_ = value & 0x1FFFF1FFu; return true; }
    // 2026-10-29 M9: arrays MMIO de la UTLB, escritura ([0xF6000000,
    // 0xF8000000)). PTEA (>=0xF7800000): aceptar y descartar. Solo
    // Write16/Write32 (8-bit no, declarado). mmu.go:216-258.
    if (address >= kItlbAddressArrayBase && address < kItlbArraysEnd) {
        ItlbArrayWrite(address, value);
        return true;
    }
    if (address >= kUtlbAddressArrayBase && address < kUtlbArraysEnd) {
        UtlbArrayWrite(address, value);
        return true;
    }
    if (address == kQacr0Register) { qacr0_ = value & 0x0000001Cu; return true; }
    if (address == kQacr1Register) { qacr1_ = value & 0x0000001Cu; return true; }
    if (cg50::Intc2Unit::MatchesPhysical(physical)) return intc2_.Write32(physical, value);
    if (cg50::EtmuUnit::MatchesPhysical(physical)) return etmu_.Write32(physical, value);
    if (address == kTtbRegister) { ttb_ = value; return true; }
    if (address == kTeaRegister) { tea_ = value; return true; }
    if (address == kPteaRegister) { ptea_ = value & 0x00000007u; return true; }
    if (address >= kBscRegisterBase && address <= kBscRegisterEnd &&
        ((address - kBscRegisterBase) % 4) == 0) {
        const std::size_t index = (address - kBscRegisterBase) / 4;
        if (index == 0) {
            bscRegisters_[0] =
                (value & kBusCommonControlWritableMask) |
                kBusCommonControlForcedMask;
            busCommonControl_ = bscRegisters_[0];
        } else if (index >= 1 && index <= 8) {
            bscRegisters_[index] = value & kBscBusControlWritableMask;
        } else if (index == 17) {
            bscRegisters_[17] = value & kBscSdramControlWritableMask;
        } else {
            bscRegisters_[index] = value;
        }
        return true;
    }
    if (address >= kSdramModeRegister2Base && address <= kSdramModeRegister3End) {
        return true;
    }
    if (physical == kFrequencyControlRegisterPhysical) {
        frequencyControl_ =
            (value & kFrequencyControlWritableMask) &
            ~kFrequencyControlKickMask;
        return true;
    }
    if (physical == kPllControlRegisterPhysical) {
        pllControl_ = value & kPllControlWritableMask;
        return true;
    }
    if (physical == kFllFrequencyRegisterPhysical) {
        fllFrequency_ =
            (value & kFllFrequencyWritableMask) |
            kFllFrequencyForcedMask;
        return true;
    }
    if (physical == kClockChangeStatusRegisterPhysical)
        return false;
    if (physical == kFsiClockControlRegisterPhysical) {
        fsiClockControl_ = value & kFsiClockControlWritableMask;
        return true;
    }
    if (physical == kDdClockControlRegisterPhysical) {
        ddClockControl_ = value & kDdClockControlWritableMask;
        return true;
    }
    if (physical == kUsbClockControlRegisterPhysical) {
        usbClockControl_ = value & kUsbClockControlWritableMask;
        return true;
    }
    if (physical == kPll2ControlRegisterPhysical) {
        pll2Control_ =
            (value & kPll2ControlWritableMask) &
            ~kPll2ControlKickMask;
        return true;
    }
    if (physical == kSpuClockControlRegisterPhysical) {
        spuClockControl_ = value & kSpuClockControlWritableMask;
        return true;
    }
    if (physical == kVpuClockControlRegisterPhysical) {
        vpuClockControl_ = value;
        return true;
    }
    if (physical == kSpreadSpectrumControlRegisterPhysical) {
        spreadSpectrumControl_ = value & kSpreadSpectrumControlWritableMask;
        return true;
    }
    if (physical == kCompareMatchTimerCounterRegisterPhysical) {
        // The official long-access callback writes only DX to the backing
        // dword; retain its high word exactly.
        compareMatchTimerCounter_ =
            (compareMatchTimerCounter_ & 0xFFFF0000u) | (value & 0xFFFFu);
        compareMatchTimerControl_ = static_cast<std::uint16_t>(
            compareMatchTimerControl_ & ~kCompareMatchTimerWriteBusyFlag);
        compareMatchTimerPrescaler_ = 0;
        return true;
    }
    if (physical == kCompareMatchTimerConstantRegisterPhysical) {
        compareMatchTimerConstant_ =
            (compareMatchTimerConstant_ & 0xFFFF0000u) | (value & 0xFFFFu);
        compareMatchTimerPrescaler_ = 0;
        return true;
    }
    if (physical ==
        kModuleStopControlRegister0Physical) {
        moduleStopControl0_ =
            (value & kModuleStopControlRegister0WritableMask) |
            kModuleStopControlRegister0Reset;
        return true;
    }
    if (physical == kStandbyControlRegisterPhysical) {
        standbyControl_ = value & kStandbyControlWritableMask;
        return true;
    }
    if (physical == kModuleStopControlRegister2Physical) {
        moduleStopControl2_ =
            (value & kModuleStopControlRegister2WritableMask) |
            kModuleStopControlRegister2ForcedMask;
        return true;
    }
    if (address == kCacheControlRegister) {
        // Cache tags/timing are not exposed to the functional core yet.  The
        // architectural CCR value is retained so the SH startup sequence can
        // enable caches and observe its write-back exactly.
        cacheControl_ = value & kCacheControlWritableMask;
        return true;
    }
    if (address == kMmuControlRegister) {
        // Bit 2 is the CPU73050 TLB-invalidate command.  The official callback
        // observes it for the invalidation side effect but clears it from the
        // stored register value.  Our functional UTLB array is invalidated
        // as well so translation restarts from a clean table.
        if ((value & kMmuControlTlbInvalidateCommand) != 0) {
            utlb_.fill(UtlbEntry{});
            itlb_.fill(UtlbEntry{});
        }
        mmuControl_ =
            (value & kMmuControlWritableMask) &
            ~kMmuControlTlbInvalidateCommand;
        return true;
    }
    if (address == kTraRegister) {
        regs_.tra = value & kTraWritableMask;
        return true;
    }
    if (address == kExpevtRegister) {
        regs_.expevt = value & kExpevtWritableMask;
        return true;
    }
    if (address == kIntevtRegister) {
        regs_.intevt = value & kIntevtWritableMask;
        return true;
    }
    if (address == kCpuopmRegister) {
        cpuopm_ = (value & kCpuopmWritableMask) | kCpuopmForcedMask;
        return true;
    }
    if (address == kExpmaskRegister) {
        expmask_ = value & kExpmaskWritableMask;
        return true;
    }
    std::size_t maskIndex = 0;
    if (InterruptMaskRegisterIndex(physical, maskIndex)) {
        const std::uint8_t mask = (maskIndex == 5) ? 0xF7u : 0xFFu;
        interruptMask_[maskIndex] |= static_cast<std::uint8_t>(value & mask);
        if (maskIndex == 5) interruptMask5_ = interruptMask_[5];
        return true;
    }
    if (InterruptMaskClearRegisterIndex(physical, maskIndex)) {
        interruptMask_[maskIndex] &= static_cast<std::uint8_t>(~(value & 0xFFu));
        if (maskIndex == 5) interruptMask5_ = interruptMask_[5];
        return true;
    }
    // SH7305 DMAC0: 6 channels
    //   CH0..CH3 at 0xFE008020, 0x30, 0x40, 0x50 (CHCR at +0x0C)
    //   CH4..CH5 at 0xFE008070, 0x80             (CHCR at +0x0C)
    if ((physical == 0xFE00802Cu || physical == 0xFE00803Cu ||
         physical == 0xFE00804Cu || physical == 0xFE00805Cu ||
         physical == 0xFE00807Cu || physical == 0xFE00808Cu)) {
        const std::size_t channelBase = static_cast<std::size_t>(physical - 0xFE008000u - 0xCu);
        std::size_t channelIndex = 0u;
        switch (physical) {
        case 0xFE00802Cu: channelIndex = 0u; break;
        case 0xFE00803Cu: channelIndex = 1u; break;
        case 0xFE00804Cu: channelIndex = 2u; break;
        case 0xFE00805Cu: channelIndex = 3u; break;
        case 0xFE00807Cu: channelIndex = 4u; break;
        case 0xFE00808Cu: channelIndex = 5u; break;
        default: break;
        }
        ExecuteDmacChannel(channelIndex, channelBase, value);
        return true;
    }
    auto* p = WritePointer(address, 4);
    if (p) {
        p[0] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
        p[1] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
        p[2] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        p[3] = static_cast<std::uint8_t>(value & 0xFFu);
        return true;
    }
    if (physical >= kRomPhysicalBase && physical + 3 < kRomPhysicalBase + rom_.size()) {
        if (physical >= kSystemParameterPhysicalBase && (value & 0xFFu) != 0xF0u) {
            const std::size_t off = physical - kRomPhysicalBase;
            rom_[off] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
            rom_[off + 1] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
            rom_[off + 2] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
            rom_[off + 3] = static_cast<std::uint8_t>(value & 0xFFu);
        }
        return true;
    }
    std::size_t c2OffsetW32 = 0;
    if (!c2Rom_.empty() && C2StorageOffset(physical, 4, c2Rom_.size(), c2OffsetW32)) {
        if (!flashCfi_.IsReadArray()) {
            if ((value & 0xFFu) == 0xF0u) flashCfi_.Reset();
            return true;
        }
        c2Rom_[c2OffsetW32] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
        c2Rom_[c2OffsetW32 + 1] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
        c2Rom_[c2OffsetW32 + 2] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        c2Rom_[c2OffsetW32 + 3] = static_cast<std::uint8_t>(value & 0xFFu);
        return true;
    }
    std::size_t userOffsetW32 = 0;
    if (!userRom_.empty() &&
        UserRomStorageOffset(physical, 4, userRom_.size(), userOffsetW32)) {
        if (!flashCfi_.IsReadArray()) {
            if ((value & 0xFFu) == 0xF0u) flashCfi_.Reset();
            return true;
        }
        static const bool storeLogW32 = std::getenv("CG50_STORE_LOG") != nullptr;
        if (storeLogW32) {
            std::fprintf(stderr, "[STOREWR] phys=%08X off=%06zX bytes=4 val=%08X pc=%08X inst=%llu\n",
                         physical, userOffsetW32, value, regs_.pc,
                         (unsigned long long)stats_.instructions);
        }
        static const bool noSwallow32 = std::getenv("CG50_FLASH_NO_SWALLOW") != nullptr;
        if (!noSwallow32 && (value & 0xFFu) == 0xF0u) return true;
        const std::size_t off = userOffsetW32;
        userRom_[off] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
        userRom_[off + 1] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
        userRom_[off + 2] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
        userRom_[off + 3] = static_cast<std::uint8_t>(value & 0xFFu);
        return true;
    }
    if (IsLcdBusPhysical(physical)) {
        const auto high = static_cast<std::uint16_t>((value >> 16) & 0xFFFFu);
        const auto low = static_cast<std::uint16_t>(value & 0xFFFFu);
        return peripheralBus_.Write16(physical, high) == cg50::PeripheralBus::Result::Accepted &&
               peripheralBus_.Write16(physical, low) == cg50::PeripheralBus::Result::Accepted;
    }
    return false;
}

void Cg50NativeCore::DrivePortData(const cg50::PfcPort port,
                                   const std::uint8_t value,
                                   const std::uint8_t implementedMask) noexcept
{
    for (std::uint8_t bit = 0; bit < 8u; ++bit) {
        const auto bitMask = static_cast<std::uint8_t>(1u << bit);
        if ((implementedMask & bitMask) == 0) continue;
        peripheralBus_.SetPfcPin({port, bit}, (value & bitMask) != 0);
    }
}

void Cg50NativeCore::UpdatePfcDataRegister(const std::uint32_t physical, const std::uint8_t value) noexcept
{
    switch (physical) {
    case kPortADataRegisterPhysical:
        DrivePortData(cg50::PfcPort::A, value, 0xFFu);
        break;
    case kPortHDataRegisterPhysical:
        DrivePortData(cg50::PfcPort::H, value, 0xFFu);
        if ((value & 0x20u) != 0) {
            peripheralBus_.SetV28Sys(true);
        }
        break;
    case kPortJDataRegisterPhysical:
        DrivePortData(cg50::PfcPort::J, value, 0xFFu);
        if ((value & 0x40u) != 0) {
            peripheralBus_.SetV28Sys(true);
        }
        break;
    case kPortMDataRegisterPhysical:
        portMData_ = static_cast<std::uint8_t>(value & kPortMDataWritableMask);
        DrivePortData(cg50::PfcPort::M, portMData_, kPortMDataWritableMask);
        break;
    case kPortNDataRegisterPhysical:
        portNData_ = static_cast<std::uint8_t>(value & kPortNDataWritableMask);
        DrivePortData(cg50::PfcPort::N, portNData_, kPortNDataWritableMask);
        break;
    case kPortRDataRegisterPhysical:
        DrivePortData(cg50::PfcPort::R, value, 0xFFu);
        break;
    case kPortUDataRegisterPhysical:
        DrivePortData(cg50::PfcPort::U, value, 0xFFu);
        break;
    case kPortPDataRegisterPhysical:
        DrivePortData(cg50::PfcPort::P, value, 0xFFu);
        break;
    default:
        break;
    }
}

void Cg50NativeCore::TickCompareMatchTimer(const std::uint64_t cycles) noexcept
{
    if (cycles == 0 ||
        (compareMatchTimerStart_ & kCompareMatchTimerStartWritableMask) == 0 ||
        (moduleStopControl0_ & 0x00004000u) != 0)
        return;

    // CPU73050's static CKS table at RVA 0x947B0 is
    // {0,0,0,0,8,32,128,0}.  One functional SH cycle is the core's base tick;
    // clock-domain timing can later refine that ratio without changing the
    // register state machine.
    static constexpr std::uint32_t kClockDivisors[8] = {
        0u, 0u, 0u, 0u, 8u, 32u, 128u, 0u
    };
    const auto divisor = kClockDivisors[compareMatchTimerControl_ & 0x7u];
    if (divisor == 0) return;

    compareMatchTimerPrescaler_ += cycles;
    while (compareMatchTimerPrescaler_ >= divisor &&
           (compareMatchTimerStart_ & kCompareMatchTimerStartWritableMask) != 0) {
        compareMatchTimerPrescaler_ -= divisor;

        const bool sixteenBitMode = (compareMatchTimerControl_ & 0x0200u) != 0;
        std::uint32_t next = compareMatchTimerCounter_ + 1u;
        if (sixteenBitMode) next &= 0xFFFFu;
        if (next == 0) {
            compareMatchTimerControl_ = static_cast<std::uint16_t>(
                compareMatchTimerControl_ | kCompareMatchTimerOverflowFlag);
        }

        std::uint32_t compare = compareMatchTimerConstant_ + 1u;
        if (sixteenBitMode) compare &= 0xFFFFu;
        if (next == compare) {
            compareMatchTimerControl_ = static_cast<std::uint16_t>(
                compareMatchTimerControl_ | kCompareMatchTimerCompareFlag);
            compareMatchTimerCounter_ = 0;
            // Bit 8 selects repeat mode in CPU73050's compare callback.  In
            // one-shot mode the concrete callback clears CMSTR bit 5.
            if ((compareMatchTimerControl_ & 0x0100u) == 0) {
                compareMatchTimerStart_ = static_cast<std::uint16_t>(
                    compareMatchTimerStart_ & ~kCompareMatchTimerStartWritableMask);
            }
        } else {
            compareMatchTimerCounter_ = next;
        }
    }
}

void Cg50NativeCore::TickTimerUnit(const std::uint64_t cycles) noexcept
{
    if (cycles == 0) return;

    // ---- Unit 0 (dedicated register file; see kTmu0PhysicalBase note) ----
    const std::uint32_t delta = static_cast<std::uint32_t>(cycles * 4u);
    for (std::size_t ch = 0; ch < tmu0_.size(); ++ch) {
        auto& channel = tmu0_[ch];
        if (!channel.running) continue;
        if (channel.tcnt >= delta) {
            channel.tcnt -= delta;
        } else {
            // Underflow: set UNF, reload from TCOR and keep the excess
            // counted against the new period.
            if (channel.tcor != 0u) {
                channel.tcnt = channel.tcor -
                               ((delta - channel.tcnt - 1u) % (channel.tcor + 1u));
            } else {
                channel.tcnt = 0u - (delta - channel.tcnt);  // free-run
            }
            channel.tcr |= static_cast<std::uint16_t>(kTmuTcrUnderflowFlag);
            if ((channel.tcr & kTmuTcrUnderflowInterruptEnable) != 0) {
                pendingTimerIrq_ |= static_cast<std::uint32_t>(1u << ch);
            }
        }
    }

    // ---- Unit 1 (page-backed at 0xA4490000, tick only; no IRQ events:
    //      the ch3-5 INTEVT codes are not evidenced locally) ----
    if (std::uint8_t* tmu1 =
            area1PageTable_[(0x04490000u - kArea1BasePhysical) >> 12]) {
        std::uint8_t* tstr = tmu1 + 0x04u;
        // CG50_TICKDBG=1: diagnostico del tick unidad 1 (F1-A).
        if (kTickDbgEnabled) {
            static std::uint64_t tickDbgCount = 0;
            if ((++tickDbgCount % 1000000ull) == 0ull) {
                const std::uint8_t* dbgTcnt = tmu1 + 0x18u;
                std::printf("[TICKDBG] n=%llu cycles=%llu tstr=%p *tstr=0x%02X "
                            "tcnt4=%08X\n",
                            static_cast<unsigned long long>(tickDbgCount),
                            static_cast<unsigned long long>(cycles),
                            static_cast<void*>(tstr),
                            *tstr,
                            (static_cast<std::uint32_t>(dbgTcnt[0]) << 24 |
                             static_cast<std::uint32_t>(dbgTcnt[1]) << 16 |
                             static_cast<std::uint32_t>(dbgTcnt[2]) << 8 |
                             static_cast<std::uint32_t>(dbgTcnt[3])));
            }
        }
        const std::uint8_t str = *tstr;
        if (str != 0u) {
            for (std::size_t ch = 0; ch < 3; ++ch) {
                if ((str & (1u << ch)) == 0) continue;
                const std::size_t chOff = ch * 0x0Cu;
                auto* tcor = tmu1 + 0x08u + chOff;
                auto* tcnt = tmu1 + 0x0Cu + chOff;
                auto* tcr = tmu1 + 0x10u + chOff;
                std::uint32_t cur = (static_cast<std::uint32_t>(tcnt[0]) << 24) |
                                    (static_cast<std::uint32_t>(tcnt[1]) << 16) |
                                    (static_cast<std::uint32_t>(tcnt[2]) << 8) |
                                    static_cast<std::uint32_t>(tcnt[3]);
                std::uint32_t reload = (static_cast<std::uint32_t>(tcor[0]) << 24) |
                                       (static_cast<std::uint32_t>(tcor[1]) << 16) |
                                       (static_cast<std::uint32_t>(tcor[2]) << 8) |
                                       static_cast<std::uint32_t>(tcor[3]);
                if (cur >= delta) {
                    cur -= delta;
                } else {
                    if (reload > 0u) {
                        cur = reload - ((delta - cur - 1u) % (reload + 1u));
                    } else {
                        cur = 0u - (delta - cur);
                    }
                    tcr[0] = static_cast<std::uint8_t>(
                        tcr[0] | static_cast<std::uint8_t>((kTmuTcrUnderflowFlag >> 8) & 0xFFu));
                    tcr[1] = static_cast<std::uint8_t>(
                        tcr[1] | static_cast<std::uint8_t>(kTmuTcrUnderflowFlag & 0xFFu));
                }
                tcnt[0] = static_cast<std::uint8_t>((cur >> 24) & 0xFFu);
                tcnt[1] = static_cast<std::uint8_t>((cur >> 16) & 0xFFu);
                tcnt[2] = static_cast<std::uint8_t>((cur >> 8) & 0xFFu);
                tcnt[3] = static_cast<std::uint8_t>(cur & 0xFFu);
            }
        }
    }
    pendingEtmuIrq_ |= etmu_.Tick(cycles);
    tpu_.Tick(cycles);
}

void Cg50NativeCore::FlushStoreQueue(const std::uint32_t sqAddress)
{
    const bool isSq1 = (sqAddress & 0x20u) != 0u;
    const std::uint8_t* sqData = storeQueues_.data() + (isSq1 ? 32u : 0u);
    std::uint32_t destPhys = 0u;
    if ((mmuControl_ & kMmuControlAtBit) != 0u) {
        std::uint32_t exc = 0u;
        if (!TranslateAddress(sqAddress & 0xFFFFFFE0u, true, false, destPhys, exc, nullptr)) {
            const std::uint32_t qacr = isSq1 ? qacr1_ : qacr0_;
            destPhys = ((qacr & 0x1Cu) << 24) | (sqAddress & 0x03FFFFE0u);
        }
    } else {
        const std::uint32_t qacr = isSq1 ? qacr1_ : qacr0_;
        destPhys = ((qacr & 0x1Cu) << 24) | (sqAddress & 0x03FFFFE0u);
    }
    if (IsLcdBusPhysical(destPhys)) {
        for (std::size_t i = 0; i < 32u; i += 2u) {
            const std::uint16_t word = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(sqData[i]) << 8) | sqData[i + 1u]);
            (void)peripheralBus_.Write16(destPhys, word);
        }
        return;
    }
    if (auto* dst = WritePointer(destPhys, 32u)) {
        std::memcpy(dst, sqData, 32u);
    }
}

void Cg50NativeCore::ExecuteDmacChannel(const std::size_t channelIndex,
                                        const std::size_t channelBase,
                                        const std::uint32_t value)
{
    dmarsPage_[channelBase + 0xC] = static_cast<std::uint8_t>((value >> 24) & 0xFFu);
    dmarsPage_[channelBase + 0xD] = static_cast<std::uint8_t>((value >> 16) & 0xFFu);
    dmarsPage_[channelBase + 0xE] = static_cast<std::uint8_t>((value >> 8) & 0xFFu);
    dmarsPage_[channelBase + 0xF] = static_cast<std::uint8_t>(value & 0xFFu);

    if ((value & 1u) == 0u) return; // DE = 0

    std::uint32_t sar =
        (static_cast<std::uint32_t>(dmarsPage_[channelBase]) << 24) |
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 1]) << 16) |
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 2]) << 8) |
        static_cast<std::uint32_t>(dmarsPage_[channelBase + 3]);
    std::uint32_t dar =
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 4]) << 24) |
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 5]) << 16) |
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 6]) << 8) |
        static_cast<std::uint32_t>(dmarsPage_[channelBase + 7]);
    const std::uint32_t tcr =
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 8]) << 24) |
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 9]) << 16) |
        (static_cast<std::uint32_t>(dmarsPage_[channelBase + 10]) << 8) |
        static_cast<std::uint32_t>(dmarsPage_[channelBase + 11]);

    // SH7305 CHCR TS[2:0]: bit 20 is TS[2], bits 4:3 are TS[1:0]
    static constexpr std::array<std::size_t, 8> kUnitSizes = {
        1u, 2u, 4u, 16u, 32u, 1u, 1u, 1u
    };
    const std::size_t tsCode =
        static_cast<std::size_t>((((value >> 20) & 1u) << 2) | ((value >> 3) & 3u));
    const std::size_t unitBytes = kUnitSizes[tsCode];
    const std::uint32_t smMode = (value >> 12) & 0x3u; // 0=fixed, 1=inc, 2=dec
    const std::uint32_t dmMode = (value >> 14) & 0x3u;
    const std::size_t totalBytes = static_cast<std::size_t>(tcr) * unitBytes;

    const std::uint32_t sarPhys = DirectMappedPhysicalAddress(sar);
    const std::uint32_t darPhys = DirectMappedPhysicalAddress(dar);

    if (PeriphDiagEnabled()) {
        std::printf("[DMA-CH%zu] SAR=0x%08X DAR=0x%08X TCR=%u CHCR=0x%08X sm=%u dm=%u total=%zu inst=%llu\n",
                    channelIndex, sar, dar, tcr, value, smMode, dmMode, totalBytes,
                    static_cast<unsigned long long>(stats_.instructions));
    }

    if (totalBytes > 0u) {
        // Fast path 1: contiguous memory -> CS5A R61524 LCD bus (all aliases in [0x14000000, 0x15000000))
        if (IsLcdBusPhysical(darPhys) && smMode == 1u && dmMode == 0u) {
            if (totalBytes == 165888u) {
                lastLcdDmaSar_ = sarPhys;
            }
            if (const auto* src = ReadPointer(sar, totalBytes)) {
                for (std::size_t i = 0; i + 1u < totalBytes; i += 2u) {
                    const std::uint16_t word = static_cast<std::uint16_t>(
                        (static_cast<std::uint16_t>(src[i]) << 8) | src[i + 1u]);
                    (void)peripheralBus_.Write16(darPhys, word);
                }
                sar += static_cast<std::uint32_t>(totalBytes);
            }
        }
        // Fast path 2: contiguous memory -> contiguous memory
        else if (!IsLcdBusPhysical(sarPhys) && !IsLcdBusPhysical(darPhys) &&
                 smMode == 1u && dmMode == 1u) {
            const auto* src = ReadPointer(sar, totalBytes);
            auto* dst = WritePointer(dar, totalBytes);
            if (src && dst) {
                std::memcpy(dst, src, totalBytes);
                sar += static_cast<std::uint32_t>(totalBytes);
                dar += static_cast<std::uint32_t>(totalBytes);
            }
        }
        // General path: arbitrary SM/DM modes (fixed/inc/dec) & LCD GRAM read/write
        else {
            const std::int32_t sStep = (smMode == 1u) ? static_cast<std::int32_t>(unitBytes)
                                     : (smMode == 2u) ? -static_cast<std::int32_t>(unitBytes) : 0;
            const std::int32_t dStep = (dmMode == 1u) ? static_cast<std::int32_t>(unitBytes)
                                     : (dmMode == 2u) ? -static_cast<std::int32_t>(unitBytes) : 0;
            for (std::uint32_t k = 0; k < tcr; ++k) {
                const std::uint32_t curSarPhys = DirectMappedPhysicalAddress(sar);
                const std::uint32_t curDarPhys = DirectMappedPhysicalAddress(dar);
                if (IsLcdBusPhysical(curDarPhys)) {
                    if (const auto* sp = ReadPointer(sar, unitBytes)) {
                        for (std::size_t i = 0; i + 1u < unitBytes; i += 2u) {
                            const std::uint16_t w = static_cast<std::uint16_t>(
                                (static_cast<std::uint16_t>(sp[i]) << 8) | sp[i + 1u]);
                            (void)peripheralBus_.Write16(curDarPhys, w);
                        }
                    }
                } else if (IsLcdBusPhysical(curSarPhys)) {
                    if (auto* dp = WritePointer(dar, unitBytes)) {
                        for (std::size_t i = 0; i + 1u < unitBytes; i += 2u) {
                            std::uint16_t w = 0u;
                            (void)peripheralBus_.Read16(curSarPhys, w);
                            dp[i] = static_cast<std::uint8_t>(w >> 8);
                            dp[i + 1u] = static_cast<std::uint8_t>(w & 0xFFu);
                        }
                    }
                } else {
                    const auto* sp = ReadPointer(sar, unitBytes);
                    auto* dp = WritePointer(dar, unitBytes);
                    if (sp && dp) {
                        std::memcpy(dp, sp, unitBytes);
                    }
                }
                sar = static_cast<std::uint32_t>(sar + sStep);
                dar = static_cast<std::uint32_t>(dar + dStep);
            }
        }
    }

    // Write back updated SAR, DAR, TCR=0, and CHCR (TE=1, DE=0)
    dmarsPage_[channelBase + 0] = static_cast<std::uint8_t>((sar >> 24) & 0xFFu);
    dmarsPage_[channelBase + 1] = static_cast<std::uint8_t>((sar >> 16) & 0xFFu);
    dmarsPage_[channelBase + 2] = static_cast<std::uint8_t>((sar >> 8) & 0xFFu);
    dmarsPage_[channelBase + 3] = static_cast<std::uint8_t>(sar & 0xFFu);
    dmarsPage_[channelBase + 4] = static_cast<std::uint8_t>((dar >> 24) & 0xFFu);
    dmarsPage_[channelBase + 5] = static_cast<std::uint8_t>((dar >> 16) & 0xFFu);
    dmarsPage_[channelBase + 6] = static_cast<std::uint8_t>((dar >> 8) & 0xFFu);
    dmarsPage_[channelBase + 7] = static_cast<std::uint8_t>(dar & 0xFFu);
    dmarsPage_[channelBase + 8] = 0u;
    dmarsPage_[channelBase + 9] = 0u;
    dmarsPage_[channelBase + 10] = 0u;
    dmarsPage_[channelBase + 11] = 0u;

    const std::uint32_t finalChcr = (value | 0x00000002u) & ~0x00000001u;
    dmarsPage_[channelBase + 0xC] = static_cast<std::uint8_t>((finalChcr >> 24) & 0xFFu);
    dmarsPage_[channelBase + 0xD] = static_cast<std::uint8_t>((finalChcr >> 16) & 0xFFu);
    dmarsPage_[channelBase + 0xE] = static_cast<std::uint8_t>((finalChcr >> 8) & 0xFFu);
    dmarsPage_[channelBase + 0xF] = static_cast<std::uint8_t>(finalChcr & 0xFFu);

    // If CHCR.IE (bit 2) is enabled, queue DMAC Transfer-End IRQ (DEI0..5)
    if ((value & 0x00000004u) != 0u && channelIndex < 6u) {
        pendingDmacIrq_ |= (1u << channelIndex);
    }
}

void Cg50NativeCore::SetT(const bool value) noexcept
{
    if (value) regs_.sr |= kSrT;
    else regs_.sr &= ~kSrT;
}

bool Cg50NativeCore::T() const noexcept { return (regs_.sr & kSrT) != 0; }

void Cg50NativeCore::SetSr(const std::uint32_t value) noexcept
{
    const bool oldRb = (regs_.sr & kSrRb) != 0;
    const std::uint32_t masked = value & kSrWritableMask;
    const bool newRb = (masked & kSrRb) != 0;
    if (oldRb != newRb) {
        for (std::size_t i = 0; i < alternateBank_.size(); ++i)
            std::swap(regs_.r[i], alternateBank_[i]);
    }
    regs_.sr = masked;
}

void Cg50NativeCore::ScheduleBranch(const std::uint32_t target) noexcept
{
    pendingBranchValid_ = true;
    pendingBranchTarget_ = target;
}

void Cg50NativeCore::HaltUnsupported(const std::uint32_t pc, const std::uint16_t opcode)
{
    stats_.unsupportedPc = pc;
    stats_.unsupportedOpcode = opcode;
    state_ = State::Halted;
    std::wstringstream ss;
    ss << L"CG50 ROM-native: opcode SH-4A aÃƒÆ’Ã‚Âºn no implementado 0x"
       << std::hex << std::uppercase << std::setw(4) << std::setfill(L'0') << opcode
       << L" en PC=0x" << std::setw(8) << pc;
    diagnostic_ = ss.str();
}

bool Cg50NativeCore::FailMemory(
    const std::uint32_t pc, const std::uint16_t opcode,
    const std::uint32_t address, const bool write)
{
    stats_.lastMemoryFaultAddress = address;
    stats_.lastMemoryFaultWasWrite = write;
    state_ = State::Halted;
    std::wstringstream ss;
    ss << L"CG50 ROM-native: " << (write ? L"escritura" : L"lectura")
       << L" fuera del mapa/MMIO pendiente en 0x"
       << std::hex << std::uppercase << std::setw(8) << std::setfill(L'0') << address
       << L"; PC=0x" << std::setw(8) << pc
       << L" opcode=0x" << std::setw(4) << opcode;
    diagnostic_ = ss.str();
    return false;
}

bool Cg50NativeCore::EnterGeneralException(
    const std::uint32_t pc, const std::uint32_t eventCode,
    const bool trapa, const std::uint8_t trapaImmediate,
    const std::uint32_t vectorOffset)
{
    cg50::ExceptionRequest request{};
    request.pc = pc;
    request.sr = regs_.sr;
    request.r15 = regs_.r[15];
    request.vbr = regs_.vbr;
    request.eventCode = eventCode;
    request.vectorOffset = vectorOffset;
    request.inDelaySlot = executingDelaySlot_;
    request.isTrapa = trapa;
    request.trapaImmediate = trapaImmediate;

    const auto entry = cg50::BuildGeneralExceptionEntry(request);
    // Traza de excepciones (N-8): evento crudo, outcome, PC, PR y opcode.
    if (PeriphDiagEnabled()) {
        std::uint16_t atPc = 0;
        const bool haveOp = Read16(pc, atPc);
        static const char anchor = 0;
#if defined(_MSC_VER)
        const auto ra = reinterpret_cast<const char*>(_ReturnAddress());
#else
        const auto ra =
            reinterpret_cast<const char*>(__builtin_return_address(0));
#endif
        std::printf("[EXCEPTION] raw=0x%08X outcome=%d expevt=0x%08X pc=0x%08X pr=0x%08X op=0x%04X ok=%d radelta=%lld inst=%llu\n",
            request.eventCode, static_cast<int>(entry.outcome), entry.expevt,
            pc, regs_.pr, atPc, haveOp ? 1 : 0,
            static_cast<long long>(ra - &anchor),
            (unsigned long long)stats_.instructions);
    }
    if (entry.outcome == cg50::ExceptionEntry::Outcome::Reset) {
        const auto previousExceptions = stats_.exceptionsTaken;
        Reset();
        stats_.exceptionsTaken = previousExceptions + 1u;
        stats_.lastExceptionEvent = eventCode;
        diagnostic_ = L"CG50 ROM-native: excepciÃƒÆ’Ã‚Â³n SH con SR.BL activo -> reset";
        stats_.cycles += 5;
        return true;
    }

    regs_.ssr = entry.ssr;
    regs_.spc = entry.spc;
    regs_.sgr = entry.sgr;
    cpuLockFlag_ = false;
    SetSr(entry.sr);
    regs_.pc = entry.targetPc;
    regs_.expevt = entry.expevt;
    if (entry.traValid) regs_.tra = entry.tra;
    pendingBranchValid_ = false;
    pendingBranchTarget_ = 0;
    state_ = State::Running;
    ++stats_.exceptionsTaken;
    stats_.lastExceptionEvent = entry.expevt;
    stats_.cycles += 5;

    std::wstringstream ss;
    ss << L"CG50 ROM-native: excepciÃƒÆ’Ã‚Â³n SH EXPEVT=0x"
       << std::hex << std::uppercase << std::setw(8) << std::setfill(L'0')
       << entry.expevt << L" -> PC=0x" << std::setw(8) << regs_.pc;
    diagnostic_ = ss.str();
    return true;
}

bool Cg50NativeCore::ExecuteOpcode(const std::uint32_t pc, const std::uint16_t op)
{
    // CG50_PCWIN (exam 2026-10-30): tracer de entrada de PC, ver
    // TracePcWindow. kPcWinLogEnabled lo evalua la funcion; aqui solo el
    // if por instructions (barato y default-off).
    if (kPcWinLogEnabled) {
        TracePcWindow(pc, regs_.pr, stats_.instructions);
    }
    const std::uint32_t n = RegN(op);
    const std::uint32_t m = RegM(op);

    const auto requirePrivileged = [&]() -> bool {
        if ((regs_.sr & kSrMd) != 0) return true;
        (void)EnterGeneralException(pc, cg50::kExpevtIllegalInstruction, false, 0);
        return false;
    };

    bool addressExceptionTaken = false;
    bool translationExceptionTaken = false;
    const auto GuestAddressAllowed = [&](const std::uint32_t address,
                                         const bool write) -> bool {
        if (!cg50::DirectSegmentAccessViolatesUserMode(regs_.sr, kSrMd, address))
            return true;
        addressExceptionTaken = true;
        (void)EnterGeneralException(pc, cg50::AddressErrorEvent(write), false, 0);
        return false;
    };
    const auto GuestAlignmentAllowed = [&](const std::uint32_t address,
                                           const std::size_t bytes,
                                           const bool write) -> bool {
        if (bytes == 2 && (address & 1u) != 0) {
            addressExceptionTaken = true;
            (void)EnterGeneralException(pc, cg50::AddressErrorEvent(write), false, 0);
            return false;
        }
        if (bytes >= 4 && (address & 3u) != 0) {
            addressExceptionTaken = true;
            (void)EnterGeneralException(pc, cg50::AddressErrorEvent(write), false, 0);
            return false;
        }
        return true;
    };
    const auto GuestTranslate = [&](const std::uint32_t address,
                                    const bool write,
                                    std::uint32_t& translated) -> bool {
        if ((mmuControl_ & kMmuControlAtBit) == 0) {
            translated = address;
            return true;
        }
        std::uint32_t exceptionCode = 0;
        std::uint32_t vectorOffset = 0;
        if (!TranslateAddress(address, write, false, translated,
                              exceptionCode, &vectorOffset)) {
            // 2026-10-29 M4/M5: el vector lo reporta TranslateAddress (0x400
            // miss/proteccion; 0x100 initial-page-write). TEA/PTEH ya los
            // rellena la propia TranslateAddress (RaiseFault).
            if (PeriphDiagEnabled()) {
                std::printf("[TLB-FAULT] pc=0x%08X addr=0x%08X write=%d code=0x%03X sr=0x%08X inst=%llu\n",
                    pc, address, write ? 1 : 0, exceptionCode, regs_.sr,
                    (unsigned long long)stats_.instructions);
            }
            translationExceptionTaken = true;
            (void)EnterGeneralException(pc, exceptionCode, false, 0,
                                        vectorOffset);
            return false;
        }
        return true;
    };
    const auto Read8 = [&](const std::uint32_t address, std::uint8_t& value) -> bool {
        if (!GuestAddressAllowed(address, false)) return false;
        std::uint32_t physical = 0;
        if (!GuestTranslate(address, false, physical)) return false;
        return this->Read8(physical, value);
    };
    const auto Read16 = [&](const std::uint32_t address, std::uint16_t& value) -> bool {
        if (!GuestAddressAllowed(address, false)) return false;
        if (!GuestAlignmentAllowed(address, 2, false)) return false;
        std::uint32_t physical = 0;
        if (!GuestTranslate(address, false, physical)) return false;
        return this->Read16(physical, value);
    };
    const auto Read32 = [&](const std::uint32_t address, std::uint32_t& value) -> bool {
        if (!GuestAddressAllowed(address, false)) return false;
        if (!GuestAlignmentAllowed(address, 4, false)) return false;
        std::uint32_t physical = 0;
        if (!GuestTranslate(address, false, physical)) return false;
        return this->Read32(physical, value);
    };
    const auto Write8 = [&](const std::uint32_t address, const std::uint8_t value) -> bool {
        if (!GuestAddressAllowed(address, true)) return false;
        std::uint32_t physical = 0;
        if (!GuestTranslate(address, true, physical)) return false;
        return this->Write8(physical, value);
    };
    const auto Write16 = [&](const std::uint32_t address, const std::uint16_t value) -> bool {
        if (!GuestAddressAllowed(address, true)) return false;
        if (!GuestAlignmentAllowed(address, 2, true)) return false;
        std::uint32_t physical = 0;
        if (!GuestTranslate(address, true, physical)) return false;
        return this->Write16(physical, value);
    };
    const auto Write32 = [&](const std::uint32_t address, const std::uint32_t value) -> bool {
        if (!GuestAddressAllowed(address, true)) return false;
        if (!GuestAlignmentAllowed(address, 4, true)) return false;
        std::uint32_t physical = 0;
        if (!GuestTranslate(address, true, physical)) return false;
        return this->Write32(physical, value);
    };
    const auto FailMemory = [&](const std::uint32_t faultPc,
                                const std::uint16_t faultOpcode,
                                const std::uint32_t address,
                                const bool write) -> bool {
        if (addressExceptionTaken || translationExceptionTaken) return true;
        return this->FailMemory(faultPc, faultOpcode, address, write);
    };

    const auto DispatchCommonMemory = [&](bool& execResult) -> bool {
        const auto commonMemory = cg50::ExecuteCommonMemoryOpcode(
            op, regs_.r, regs_.gbr,
            Read8, Read16, Read32, Write8, Write16, Write32);
        if (!commonMemory.handled) return false;
        if (!commonMemory.success) {
            execResult = FailMemory(pc, op, commonMemory.faultAddress, commonMemory.faultWrite);
            return true;
        }
        if (commonMemory.tChanged) SetT(commonMemory.t);
        stats_.cycles += 1;
        execResult = true;
        return true;
    };

    // Primary O(1) top-nibble dispatch. Nibbles 0x1..0xF resolve here and return
    // directly; only nibble 0x0 breaks out into the system/cache fallthrough.
    switch (op >> 12) {
    case 0xFu:
        return ExecuteFpuOpcode(pc, op);
    case 0xEu:
        regs_.r[n] = static_cast<std::uint32_t>(SignExtend8(op));
        stats_.cycles += 1;
        return true;
    case 0xDu: {
        const std::uint32_t address = ((pc + 4u) & ~3u) + ((op & 0xFFu) << 2);
        if (!Read32(address, regs_.r[n])) return FailMemory(pc, op, address, false);
        stats_.cycles += 1;
        return true;
    }
    case 0xCu:
        switch ((op >> 8) & 0x0Fu) {
        case 0x3u:
            if (executingDelaySlot_)
                return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            return EnterGeneralException(pc, cg50::kExpevtTrapa, true,
                                         static_cast<std::uint8_t>(op & 0xFFu));
        case 0x7u:
            regs_.r[0] = ((pc + 4u) & ~3u) + ((op & 0xFFu) << 2);
            stats_.cycles += 1;
            return true;
        case 0x8u:
            SetT((regs_.r[0] & (op & 0xFFu)) == 0);
            stats_.cycles += 1;
            return true;
        case 0x9u:
            regs_.r[0] &= (op & 0xFFu);
            stats_.cycles += 1;
            return true;
        case 0xAu:
            regs_.r[0] ^= (op & 0xFFu);
            stats_.cycles += 1;
            return true;
        case 0xBu:
            regs_.r[0] |= (op & 0xFFu);
            stats_.cycles += 1;
            return true;
        default: {
            bool execResult = false;
            if (DispatchCommonMemory(execResult)) return execResult;
            HaltUnsupported(pc, op);
            return false;
        }
        }
    case 0xBu: {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        const auto disp = SignExtend12(op);
        regs_.pr = pc + 4u;
        ScheduleBranch(AddSignedOffset(pc + 4u, disp * 2));
        stats_.cycles += 2;
        return true;
    }
    case 0xAu: {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        const auto disp = SignExtend12(op);
        ScheduleBranch(AddSignedOffset(pc + 4u, disp * 2));
        stats_.cycles += 2;
        return true;
    }
    case 0x9u: {
        const std::uint32_t address = pc + 4u + ((op & 0xFFu) << 1);
        std::uint16_t value = 0;
        if (!Read16(address, value)) return FailMemory(pc, op, address, false);
        regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value)));
        stats_.cycles += 1;
        return true;
    }
    case 0x8u:
        switch ((op >> 8) & 0x0Fu) {
        case 0x0u:
        case 0x1u:
        case 0x4u:
        case 0x5u: {
            bool execResult = false;
            if (DispatchCommonMemory(execResult)) return execResult;
            HaltUnsupported(pc, op);
            return false;
        }
        case 0x3u: { // JSR/N @@(disp, TBR) - Fast system call without delay slot
            if (executingDelaySlot_)
                return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            const std::uint32_t disp = op & 0x00FFu;
            const std::uint32_t tableAddr = tbr_ + (disp << 2);
            std::uint32_t target = 0;
            if (!Read32(tableAddr, target)) return FailMemory(pc, op, tableAddr, false);
            regs_.pr = pc + 2u;
            regs_.pc = target;
            stats_.cycles += 3;
            return true;
        }
        case 0x8u:
            SetT(regs_.r[0] == static_cast<std::uint32_t>(SignExtend8(op)));
            stats_.cycles += 1;
            return true;
        case 0x9u:
            if (executingDelaySlot_)
                return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            if (T()) {
                const auto disp = SignExtend8(op);
                regs_.pc = AddSignedOffset(pc + 4u, disp * 2);
            }
            stats_.cycles += 1;
            return true;
        case 0xBu:
            if (executingDelaySlot_)
                return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            if (!T()) {
                const auto disp = SignExtend8(op);
                regs_.pc = AddSignedOffset(pc + 4u, disp * 2);
            }
            stats_.cycles += 1;
            return true;
        case 0xDu:
            if (executingDelaySlot_)
                return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            if (T()) {
                const auto disp = SignExtend8(op);
                ScheduleBranch(AddSignedOffset(pc + 4u, disp * 2));
            }
            stats_.cycles += 2;
            return true;
        case 0xFu:
            if (executingDelaySlot_)
                return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            if (!T()) {
                const auto disp = SignExtend8(op);
                ScheduleBranch(AddSignedOffset(pc + 4u, disp * 2));
            }
            stats_.cycles += 2;
            return true;
        default:
            HaltUnsupported(pc, op);
            return false;
        }
    case 0x7u:
        regs_.r[n] += static_cast<std::uint32_t>(SignExtend8(op));
        stats_.cycles += 1;
        return true;
    case 0x6u:
        switch (op & 0x0Fu) {
        case 0x0u: { std::uint8_t value = 0; if (!Read8(regs_.r[m], value)) return FailMemory(pc, op, regs_.r[m], false); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); stats_.cycles += 1; return true; }
        case 0x1u: { std::uint16_t value = 0; if (!Read16(regs_.r[m], value)) return FailMemory(pc, op, regs_.r[m], false); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); stats_.cycles += 1; return true; }
        case 0x2u: { if (!Read32(regs_.r[m], regs_.r[n])) return FailMemory(pc, op, regs_.r[m], false); stats_.cycles += 1; return true; }
        case 0x3u: regs_.r[n] = regs_.r[m]; stats_.cycles += 1; return true;
        case 0x4u: { const std::uint32_t address = regs_.r[m]; std::uint8_t value = 0; if (!Read8(address, value)) return FailMemory(pc, op, address, false); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); if (n != m) regs_.r[m] += 1u; stats_.cycles += 1; return true; }
        case 0x5u: { const std::uint32_t address = regs_.r[m]; std::uint16_t value = 0; if (!Read16(address, value)) return FailMemory(pc, op, address, false); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); if (n != m) regs_.r[m] += 2u; stats_.cycles += 1; return true; }
        case 0x6u: { const std::uint32_t address = regs_.r[m]; std::uint32_t value = 0; if (!Read32(address, value)) return FailMemory(pc, op, address, false); regs_.r[n] = value; if (n != m) regs_.r[m] += 4u; stats_.cycles += 1; return true; }
        case 0x7u: regs_.r[n] = ~regs_.r[m]; stats_.cycles += 1; return true;
        case 0x8u: { const auto x = regs_.r[m]; regs_.r[n] = (x & 0xFFFF0000u) | ((x & 0x00FFu) << 8) | ((x & 0xFF00u) >> 8); stats_.cycles += 1; return true; }
        case 0x9u: { const auto x = regs_.r[m]; regs_.r[n] = (x << 16) | (x >> 16); stats_.cycles += 1; return true; }
        case 0xAu: { const auto result = cg50::NegWithCarry(regs_.r[m], T()); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0xBu: regs_.r[n] = 0u - regs_.r[m]; stats_.cycles += 1; return true;
        case 0xCu: regs_.r[n] = regs_.r[m] & 0xFFu; stats_.cycles += 1; return true;
        case 0xDu: regs_.r[n] = regs_.r[m] & 0xFFFFu; stats_.cycles += 1; return true;
        case 0xEu: regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(regs_.r[m] & 0xFFu))); stats_.cycles += 1; return true;
        case 0xFu: regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(regs_.r[m] & 0xFFFFu))); stats_.cycles += 1; return true;
        }
        HaltUnsupported(pc, op);
        return false;
    case 0x5u: {
        const std::uint32_t address = regs_.r[m] + ((op & 0x0Fu) << 2);
        if (!Read32(address, regs_.r[n])) return FailMemory(pc, op, address, false);
        stats_.cycles += 1;
        return true;
    }
    case 0x4u:
        switch (op & 0xFFu) {
        case 0x0Eu: if (!requirePrivileged()) return true; SetSr(regs_.r[n]); stats_.cycles += 3; return true;
        case 0x1Eu: regs_.gbr = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x2Eu: if (!requirePrivileged()) return true; regs_.vbr = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x3Eu: if (!requirePrivileged()) return true; regs_.ssr = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x4Eu: if (!requirePrivileged()) return true; regs_.spc = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x4Au: if (!requirePrivileged()) return true; tbr_ = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x46u: { // LDC.L @Rm+, TBR
            if (!requirePrivileged()) return true;
            std::uint32_t val = 0;
            if (!Read32(regs_.r[n], val)) return FailMemory(pc, op, regs_.r[n], false);
            regs_.r[n] += 4u;
            tbr_ = val;
            stats_.cycles += 1;
            return true;
        }
        case 0x42u: { // STC.L TBR, @-Rn
            if (!requirePrivileged()) return true;
            const std::uint32_t target = regs_.r[n] - 4u;
            if (!Write32(target, tbr_)) return FailMemory(pc, op, target, true);
            regs_.r[n] = target;
            stats_.cycles += 1;
            return true;
        }
        case 0x0Au: regs_.mach = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x1Au: regs_.macl = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x2Au: regs_.pr = regs_.r[n]; stats_.cycles += 1; return true;
        case 0x5Au:
        case 0x6Au:
            if ((regs_.sr & kSrFd) != 0) {
                std::printf("[FPU-FD-STS] pc=0x%08X op=0x%04X sr=0x%08X inst=%llu\n",
                    pc, op, regs_.sr, (unsigned long long)stats_.instructions);
                (void)EnterGeneralException(pc, cg50::kExpevtFpuDisabled, false, 0);
                return true;
            }
            if ((op & 0xFFu) == 0x5Au) {
                regs_.fpul = regs_.r[n];
            } else {
                regs_.fpscr = regs_.r[n] & kFpscrWritableMask;
            }
            stats_.cycles += 1;
            return true;
        case 0x11u: SetT(static_cast<std::int32_t>(regs_.r[n]) >= 0); stats_.cycles += 1; return true;
        case 0x15u: SetT(static_cast<std::int32_t>(regs_.r[n]) > 0); stats_.cycles += 1; return true;
        case 0x10u: --regs_.r[n]; SetT(regs_.r[n] == 0); stats_.cycles += 1; return true;
        case 0x00u:
        case 0x20u: SetT((regs_.r[n] & 0x80000000u) != 0); regs_.r[n] <<= 1; stats_.cycles += 1; return true;
        case 0x01u: SetT((regs_.r[n] & 1u) != 0); regs_.r[n] >>= 1; stats_.cycles += 1; return true;
        case 0x21u: { const std::uint32_t old = regs_.r[n]; SetT((old & 1u) != 0); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(old) >> 1); stats_.cycles += 1; return true; }
        case 0x04u: { const auto result = cg50::RotateLeft(regs_.r[n]); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0x05u: { const auto result = cg50::RotateRight(regs_.r[n]); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0x24u: { const auto result = cg50::RotateCarryLeft(regs_.r[n], T()); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0x25u: { const auto result = cg50::RotateCarryRight(regs_.r[n], T()); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0x08u: regs_.r[n] <<= 2; stats_.cycles += 1; return true;
        case 0x09u: regs_.r[n] >>= 2; stats_.cycles += 1; return true;
        case 0x18u: regs_.r[n] <<= 8; stats_.cycles += 1; return true;
        case 0x19u: regs_.r[n] >>= 8; stats_.cycles += 1; return true;
        case 0x28u: regs_.r[n] <<= 16; stats_.cycles += 1; return true;
        case 0x29u: regs_.r[n] >>= 16; stats_.cycles += 1; return true;
        case 0x2Bu: if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;
        case 0x0Bu: if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); regs_.pr = pc + 4u; ScheduleBranch(regs_.r[n]); stats_.cycles += 2; return true;
        case 0x4Bu: // JSR/N @Rn (without delay slot)
            if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
            regs_.pr = pc + 2u;
            regs_.pc = regs_.r[n];
            stats_.cycles += 2;
            return true;
        case 0xA9u: // MOVUA.L @Rm, R0 (SH-4A unaligned big-endian 32-bit load)
        case 0xE9u: { // MOVUA.L @Rm+, R0
            const std::uint32_t base = regs_.r[n];
            std::uint8_t b0 = 0, b1 = 0, b2 = 0, b3 = 0;
            if (!Read8(base, b0)) return FailMemory(pc, op, base, false);
            if (!Read8(base + 1u, b1)) return FailMemory(pc, op, base + 1u, false);
            if (!Read8(base + 2u, b2)) return FailMemory(pc, op, base + 2u, false);
            if (!Read8(base + 3u, b3)) return FailMemory(pc, op, base + 3u, false);
            const std::uint32_t loaded =
                (static_cast<std::uint32_t>(b0) << 24) |
                (static_cast<std::uint32_t>(b1) << 16) |
                (static_cast<std::uint32_t>(b2) << 8) |
                static_cast<std::uint32_t>(b3);
            if ((op & 0xFFu) == 0xE9u && n != 0u) {
                regs_.r[n] = base + 4u;
            }
            regs_.r[0] = loaded;
            stats_.cycles += 2;
            return true;
        }
        case 0x1Bu: {
            bool execResult = false;
            if (DispatchCommonMemory(execResult)) return execResult;
            HaltUnsupported(pc, op);
            return false;
        }
        default: {
            const std::uint16_t low4 = op & 0x0Fu;
            if (low4 == 0xCu) {
                regs_.r[n] = cg50::DynamicArithmeticShift(regs_.r[n], regs_.r[m]);
                stats_.cycles += 1;
                return true;
            }
            if (low4 == 0xDu) {
                regs_.r[n] = cg50::DynamicLogicalShift(regs_.r[n], regs_.r[m]);
                stats_.cycles += 1;
                return true;
            }
            if (low4 == 0xFu) {
                const auto macWord = cg50::ExecuteMacWordOpcode(
                    op, regs_.r, regs_.mach, regs_.macl, regs_.sr, Read16);
                if (macWord.handled) {
                    if (!macWord.success)
                        return FailMemory(pc, op, macWord.faultAddress, false);
                    stats_.cycles += 1;
                    return true;
                }
                HaltUnsupported(pc, op);
                return false;
            }
            const auto systemStack = cg50::ExecuteSystemStackOpcode(
                op, regs_.r, (regs_.sr & kSrMd) != 0, regs_.sr,
                regs_.gbr, regs_.vbr, regs_.ssr, regs_.spc,
                regs_.mach, regs_.macl, regs_.pr,
                regs_.fpul, regs_.fpscr, Read32, Write32);
            if (systemStack.handled) {
                if (systemStack.privilegeViolation)
                    return EnterGeneralException(pc, cg50::kExpevtIllegalInstruction, false, 0);
                if (!systemStack.success)
                    return FailMemory(pc, op, systemStack.faultAddress, systemStack.faultWrite);
                if (systemStack.setSr) SetSr(systemStack.srValue);
                stats_.cycles += 1;
                return true;
            }
            const auto bankedControl = cg50::ExecuteBankedControlOpcode(
                op, regs_.r, alternateBank_, (regs_.sr & kSrMd) != 0,
                regs_.sgr, regs_.dbr, Read32, Write32);
            if (bankedControl.handled) {
                if (bankedControl.privilegeViolation)
                    return EnterGeneralException(pc, cg50::kExpevtIllegalInstruction, false, 0);
                if (!bankedControl.success)
                    return FailMemory(pc, op, bankedControl.faultAddress, bankedControl.faultWrite);
                stats_.cycles += 1;
                return true;
            }
            HaltUnsupported(pc, op);
            return false;
        }
        }
    case 0x3u:
        switch (op & 0x0Fu) {
        case 0x0u: SetT(regs_.r[n] == regs_.r[m]); stats_.cycles += 1; return true;
        case 0x2u: SetT(regs_.r[n] >= regs_.r[m]); stats_.cycles += 1; return true;
        case 0x3u: SetT(static_cast<std::int32_t>(regs_.r[n]) >= static_cast<std::int32_t>(regs_.r[m])); stats_.cycles += 1; return true;
        case 0x4u: {
            const auto divide = cg50::ExecuteDivideOpcode(op, regs_.r, regs_.sr);
            if (divide.handled) { SetSr(divide.sr); stats_.cycles += 1; return true; }
            break;
        }
        case 0x5u: { const auto product = cg50::MultiplyUnsignedLong(regs_.r[n], regs_.r[m]); regs_.mach = product.mach; regs_.macl = product.macl; stats_.cycles += 1; return true; }
        case 0x6u: SetT(regs_.r[n] > regs_.r[m]); stats_.cycles += 1; return true;
        case 0x7u: SetT(static_cast<std::int32_t>(regs_.r[n]) > static_cast<std::int32_t>(regs_.r[m])); stats_.cycles += 1; return true;
        case 0x8u: regs_.r[n] -= regs_.r[m]; stats_.cycles += 1; return true;
        case 0xAu: { const auto result = cg50::SubWithCarry(regs_.r[n], regs_.r[m], T()); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0xBu: { const auto result = cg50::SubWithOverflow(regs_.r[n], regs_.r[m]); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0xCu: regs_.r[n] += regs_.r[m]; stats_.cycles += 1; return true;
        case 0xDu: { const auto product = cg50::MultiplySignedLong(regs_.r[n], regs_.r[m]); regs_.mach = product.mach; regs_.macl = product.macl; stats_.cycles += 1; return true; }
        case 0xEu: { const auto result = cg50::AddWithCarry(regs_.r[n], regs_.r[m], T()); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        case 0xFu: { const auto result = cg50::AddWithOverflow(regs_.r[n], regs_.r[m]); regs_.r[n] = result.value; SetT(result.t); stats_.cycles += 1; return true; }
        default: break;
        }
        HaltUnsupported(pc, op);
        return false;
    case 0x2u:
        switch (op & 0x0Fu) {
        case 0x0u: { if (!Write8(regs_.r[n], static_cast<std::uint8_t>(regs_.r[m]))) return FailMemory(pc, op, regs_.r[n], true); stats_.cycles += 1; return true; }
        case 0x1u: { if (!Write16(regs_.r[n], static_cast<std::uint16_t>(regs_.r[m]))) return FailMemory(pc, op, regs_.r[n], true); stats_.cycles += 1; return true; }
        case 0x2u: { if (!Write32(regs_.r[n], regs_.r[m])) return FailMemory(pc, op, regs_.r[n], true); stats_.cycles += 1; return true; }
        case 0x4u: { const std::uint32_t address = regs_.r[n] - 1u; if (!Write8(address, static_cast<std::uint8_t>(regs_.r[m]))) return FailMemory(pc, op, address, true); regs_.r[n] = address; stats_.cycles += 1; return true; }
        case 0x5u: { const std::uint32_t address = regs_.r[n] - 2u; if (!Write16(address, static_cast<std::uint16_t>(regs_.r[m]))) return FailMemory(pc, op, address, true); regs_.r[n] = address; stats_.cycles += 1; return true; }
        case 0x6u: { const std::uint32_t address = regs_.r[n] - 4u; if (!Write32(address, regs_.r[m])) return FailMemory(pc, op, address, true); regs_.r[n] = address; stats_.cycles += 1; return true; }
        case 0x7u: {
            const auto divide = cg50::ExecuteDivideOpcode(op, regs_.r, regs_.sr);
            if (divide.handled) { SetSr(divide.sr); stats_.cycles += 1; return true; }
            break;
        }
        case 0x8u: SetT((regs_.r[n] & regs_.r[m]) == 0); stats_.cycles += 1; return true;
        case 0x9u: regs_.r[n] &= regs_.r[m]; stats_.cycles += 1; return true;
        case 0xAu: regs_.r[n] ^= regs_.r[m]; stats_.cycles += 1; return true;
        case 0xBu: regs_.r[n] |= regs_.r[m]; stats_.cycles += 1; return true;
        case 0xCu: SetT(cg50::CompareStringBytes(regs_.r[n], regs_.r[m])); stats_.cycles += 1; return true;
        case 0xDu: regs_.r[n] = cg50::ExtractMiddle(regs_.r[n], regs_.r[m]); stats_.cycles += 1; return true;
        case 0xEu: regs_.macl = cg50::MultiplyUnsignedWord(regs_.r[n], regs_.r[m]); stats_.cycles += 1; return true;
        case 0xFu: regs_.macl = cg50::MultiplySignedWord(regs_.r[n], regs_.r[m]); stats_.cycles += 1; return true;
        default: break;
        }
        HaltUnsupported(pc, op);
        return false;
    case 0x1u: {
        const std::uint32_t address = regs_.r[n] + ((op & 0x0Fu) << 2);
        if (!Write32(address, regs_.r[m])) return FailMemory(pc, op, address, true);
        stats_.cycles += 1;
        return true;
    }
    case 0x0u:
    default:
        break;
    }

    if (op == 0x0009u) { stats_.cycles += 1; return true; }
    if (op == 0x0008u) { SetT(false); stats_.cycles += 1; return true; }
    if (op == 0x0018u) { SetT(true); stats_.cycles += 1; return true; }
    if (op == 0x0048u) { regs_.sr &= ~kSrS; stats_.cycles += 1; return true; }
    if (op == 0x0058u) { regs_.sr |= kSrS; stats_.cycles += 1; return true; }
    if (op == 0x0019u) { regs_.sr &= ~(kSrM | kSrQ | kSrT); stats_.cycles += 1; return true; }
    // 0x0028 (LDTLB) se maneja mÃƒÆ’Ã‚Â¡s abajo con el UTLB funcional. El manejo
    // previo que lo modelaba como Ãƒâ€šÃ‚Â«borrar MACH/MACLÃƒâ€šÃ‚Â» era un mis-decode
    // inerte (el OS validado nunca ejecuta LDTLB) que bloqueaba el real.

    // ICBI @Rn invalidates one 32-byte instruction-cache line.  This
    // functional core fetches directly from the backing memory and therefore
    // has no cache tags to invalidate, but the logical-address access and the
    // architected 13-cycle cost still have to be represented explicitly.
    if ((op & 0xF0FFu) == 0x00E3u) {
        if (!GuestAddressAllowed(regs_.r[n], false)) return true;
        stats_.cycles += 13;
        return true;
    }

    // NOTT (0x0068): SH-4A invert T bit (T = !T)
    if (op == 0x0068u) { SetT(!T()); stats_.cycles += 1; return true; }

    // SYNCO synchronizes memory operations. In a sequential functional core,
    // all previous reads and writes are already committed.
    if (op == 0x00ABu) { stats_.cycles += 1; return true; }

    // PREF @Rn (0x0n83): triggers 32-byte Store Queue burst flush when @Rn
    // points into the Store Queue window [0xE0000000, 0xE4000000); otherwise
    // acts as a cache prefetch hint.
    if ((op & 0xF0FFu) == 0x0083u) {
        const std::uint32_t addr = regs_.r[n];
        if (addr >= kStoreQueueBase && addr < kStoreQueueEnd) {
            FlushStoreQueue(addr);
        }
        stats_.cycles += 1;
        return true;
    }

    // OCBI @Rn, OCBP @Rn, OCBWB @Rn, PREFI @Rn cache operations.
    if ((op & 0xF0FFu) == 0x0093u ||
        (op & 0xF0FFu) == 0x00A3u || (op & 0xF0FFu) == 0x00B3u ||
        (op & 0xF0FFu) == 0x00D3u) {
        stats_.cycles += 1;
        return true;
    }

    // MOVLI.L @Rm, R0 (SH-4A atomic load-linked)
    if ((op & 0xF0FFu) == 0x0063u) {
        std::uint32_t value = 0;
        if (!Read32(regs_.r[n], value))
            return FailMemory(pc, op, regs_.r[n], false);
        regs_.r[0] = value;
        cpuLockFlag_ = true;
        stats_.cycles += 1;
        return true;
    }

    // MOVCO.L R0, @Rn (SH-4A atomic store-conditional)
    if ((op & 0xF0FFu) == 0x0073u) {
        if (cpuLockFlag_) {
            if (!Write32(regs_.r[n], regs_.r[0]))
                return FailMemory(pc, op, regs_.r[n], true);
            SetT(true);
            cpuLockFlag_ = false;
        } else {
            SetT(false);
        }
        stats_.cycles += 1;
        return true;
    }

    // MOVCA.L R0, @Rn writes R0 to @Rn without cache line fill.
    if ((op & 0xF0FFu) == 0x00C3u) {
        if (!Write32(regs_.r[n], regs_.r[0]))
            return FailMemory(pc, op, regs_.r[n], true);
        stats_.cycles += 1;
        return true;
    }

    // LDTLB commits PTEH/PTEL/PTEA into the functional UTLB (SH-4 opcode
    // 0x0028; the previous 0x0038 decoding was a never-exercised typo ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Â
    // the OS boots with the MMU disabled). The SH-4A associative URB
    // replacement is the MMUCR URC field, exactly like the SH-4A reference
    // (2026-10-29 M2, mmu.go:206-208: entry = entries[mmucr.urc]; the old
    // first-invalid/instruction-counter fallback is GONE).
    // 2026-10-29 M1: decode del data word PTEL corregido via DecodeUtlbData
    // (V bit8 0x100, SZ1 bit7, PR bits6:5, SZ0 bit4, C bit3, D bit2,
    // SH bit1, WT bit0; mmu.go:42-53). vpn/asid siguen viniendo de PTEH.
    // SIN campos nuevos en UtlbEntry (el chunk 61 del save-state queda
    // estable); C/WT no se modelan (declarado).
    // 0x0028 = CLRMAC (SH-4A: clears MACH/MACL, legal in user mode); 0x0038 =
    // canonical LDTLB. Preserve UTLB commit on supervisor 0x0028 as well so
    // existing UTLB unit tests using 0x0028 continue to pass alongside 0x0038.
    // 0x0028 = CLRMAC (SH-4A: clears MACH/MACL in user & supervisor mode;
    // decoupled cleanly from LDTLB so supervisor CLRMAC never overwrites UTLB).
    if (op == 0x0028u) {
        regs_.mach = 0;
        regs_.macl = 0;
        stats_.cycles += 1;
        return true;
    }

    // 0x0038 = LDTLB (SH-4A privileged: commits PTEH/PTEL into UTLB[MMUCR.URC]).
    if (op == 0x0038u) {
        if (!requirePrivileged()) {
            return true;
        }
        const std::size_t victim =
            static_cast<std::size_t>((mmuControl_ >> 10) & 0x3Fu) % utlb_.size();
        UtlbEntry entry = DecodeUtlbData(ptel_);
        entry.virtualPageNumber = (pteh_ >> 10) & 0x001FFFFFu;
        entry.asid = pteh_ & 0xFFu;
        utlb_[victim] = entry;
        stats_.cycles += 1;
        return true;
    }

    if (op == 0x000Bu) {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        ScheduleBranch(regs_.pr); stats_.cycles += 2; return true;
    }

    // RTS/N (SH-4A return without delay slot)
    if (op == 0x006Bu) {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        regs_.pc = regs_.pr;
        stats_.cycles += 2;
        return true;
    }

    // RTV/N Rm (SH-4A return with R0 = Rm, without delay slot)
    if ((op & 0xF0FFu) == 0x007Bu) {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        regs_.r[0] = regs_.r[n];
        regs_.pc = regs_.pr;
        stats_.cycles += 2;
        return true;
    }

    if (op == 0x002Bu) {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        if ((regs_.sr & kSrMd) == 0)
            return EnterGeneralException(pc, cg50::kExpevtIllegalInstruction, false, 0);
        cpuLockFlag_ = false;
        rteDelaySlotPrivilegedFetch_ = true;
        SetSr(regs_.ssr);
        ScheduleBranch(regs_.spc);
        stats_.cycles += 4;
        return true;
    }

    if (op == 0x001Bu) {
        if (executingDelaySlot_)
            return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0);
        if ((regs_.sr & kSrMd) == 0)
            return EnterGeneralException(pc, cg50::kExpevtIllegalInstruction, false, 0);
        // SBYCR bits retained by the model select the deeper standby state
        // (kStandbyControlWritableMask = 0xA0). Both states wake on the
        // first accepted interrupt; standby additionally documents that the
        // clocks (CMT/TMU/WDT/RTC) are the modeled wake sources.
        standbyMode_ = (standbyControl_ & kStandbyControlWritableMask) != 0;
        state_ = State::Sleeping;
        diagnostic_ = standbyMode_
            ? L"CG50 ROM-native: CPU SH7305 en STANDBY (SBYCR); espera interrupciÃƒÆ’Ã‚Â³n"
            : L"CG50 ROM-native: CPU SH7305 en SLEEP; espera interrupciÃƒÆ’Ã‚Â³n";
        stats_.cycles += 3;
        return true;
    }

    // Only 0x0xxx opcodes reach this tail (0x1xxx..0xFxxx already returned above).
    if ((op & 0xF0FFu) == 0x0002u) { if (!requirePrivileged()) return true; regs_.r[n] = regs_.sr; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x0012u) { regs_.r[n] = regs_.gbr; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x0022u) { if (!requirePrivileged()) return true; regs_.r[n] = regs_.vbr; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x0032u) { if (!requirePrivileged()) return true; regs_.r[n] = regs_.ssr; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x0042u) { if (!requirePrivileged()) return true; regs_.r[n] = regs_.spc; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x004Au) { if (!requirePrivileged()) return true; regs_.r[n] = tbr_; stats_.cycles += 1; return true; }

    if ((op & 0xF0FFu) == 0x000Au) { regs_.r[n] = regs_.mach; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x001Au) { regs_.r[n] = regs_.macl; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x002Au) { regs_.r[n] = regs_.pr; stats_.cycles += 1; return true; }
    if ((op & 0xF0FFu) == 0x0029u) { regs_.r[n] = T() ? 1u : 0u; stats_.cycles += 1; return true; }

    if ((op & 0xF0FFu) == 0x005Au || (op & 0xF0FFu) == 0x006Au) {
        if ((regs_.sr & kSrFd) != 0) {
            std::printf("[FPU-FD-STS] pc=0x%08X op=0x%04X sr=0x%08X inst=%llu\n",
                pc, op, regs_.sr, (unsigned long long)stats_.instructions);
            (void)EnterGeneralException(pc, cg50::kExpevtFpuDisabled, false, 0);
            return true;
        }
        if ((op & 0xF0FFu) == 0x005Au) {
            regs_.r[n] = regs_.fpul;                       // STS FPUL,Rn
        } else {
            regs_.r[n] = regs_.fpscr & kFpscrWritableMask; // STS FPSCR,Rn
        }
        stats_.cycles += 1;
        return true;
    }

    const auto bankedControl = cg50::ExecuteBankedControlOpcode(
        op, regs_.r, alternateBank_, (regs_.sr & kSrMd) != 0,
        regs_.sgr, regs_.dbr, Read32, Write32);
    if (bankedControl.handled) {
        if (bankedControl.privilegeViolation)
            return EnterGeneralException(pc, cg50::kExpevtIllegalInstruction, false, 0);
        if (!bankedControl.success)
            return FailMemory(pc, op, bankedControl.faultAddress, bankedControl.faultWrite);
        stats_.cycles += 1;
        return true;
    }

    const auto macLong = cg50::ExecuteMacLongOpcode(
        op, regs_.r, regs_.mach, regs_.macl, regs_.sr, Read32);
    if (macLong.handled) {
        if (!macLong.success)
            return FailMemory(pc, op, macLong.faultAddress, false);
        stats_.cycles += 1;
        return true;
    }

    if ((op & 0xF00Fu) == 0x0007u) {
        regs_.macl = cg50::MultiplyLongLow(regs_.r[n], regs_.r[m]);
        stats_.cycles += 1;
        return true;
    }

    if ((op & 0xF00Fu) == 0x0004u) { const std::uint32_t address = regs_.r[0] + regs_.r[n]; if (!Write8(address, static_cast<std::uint8_t>(regs_.r[m]))) return FailMemory(pc, op, address, true); stats_.cycles += 1; return true; }
    if ((op & 0xF00Fu) == 0x0005u) { const std::uint32_t address = regs_.r[0] + regs_.r[n]; if (!Write16(address, static_cast<std::uint16_t>(regs_.r[m]))) return FailMemory(pc, op, address, true); stats_.cycles += 1; return true; }
    if ((op & 0xF00Fu) == 0x0006u) { const std::uint32_t address = regs_.r[0] + regs_.r[n]; if (!Write32(address, regs_.r[m])) return FailMemory(pc, op, address, true); stats_.cycles += 1; return true; }
    if ((op & 0xF00Fu) == 0x000Cu) { const std::uint32_t address = regs_.r[0] + regs_.r[m]; std::uint8_t value = 0; if (!Read8(address, value)) return FailMemory(pc, op, address, false); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); stats_.cycles += 1; return true; }
    if ((op & 0xF00Fu) == 0x000Du) { const std::uint32_t address = regs_.r[0] + regs_.r[m]; std::uint16_t value = 0; if (!Read16(address, value)) return FailMemory(pc, op, address, false); regs_.r[n] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); stats_.cycles += 1; return true; }
    if ((op & 0xF00Fu) == 0x000Eu) { const std::uint32_t address = regs_.r[0] + regs_.r[m]; if (!Read32(address, regs_.r[n])) return FailMemory(pc, op, address, false); stats_.cycles += 1; return true; }

    if ((op & 0xF0FFu) == 0x0023u) { if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); ScheduleBranch(pc + 4u + regs_.r[n]); stats_.cycles += 2; return true; }
    if ((op & 0xF0FFu) == 0x0003u) { if (executingDelaySlot_) return EnterGeneralException(pc, cg50::kExpevtSlotIllegalInstruction, false, 0); regs_.pr = pc + 4u; ScheduleBranch(pc + 4u + regs_.r[n]); stats_.cycles += 2; return true; }

    HaltUnsupported(pc, op);
    return false;
}

bool Cg50NativeCore::StepInstruction()
{
    if (state_ != State::Running) return false;
    if ((regs_.pc & 1u) != 0) {
        // SH-4A: la busqueda de instruccion en PC impar genera un address
        // error (EXPEVT 0x0E0), no un halt del emulador. El manejador del
        // guest decide (pantalla de error / reinicio).
        const bool handled = EnterGeneralException(
            regs_.pc, cg50::kExpevtAddressErrorRead, false, 0);
        executingDelaySlot_ = false;
        return handled;
    }

    const bool applyOldBranch = pendingBranchValid_;
    const std::uint32_t oldBranchTarget = pendingBranchTarget_;
    const bool rteDelaySlotPrivilegedFetch =
        applyOldBranch && rteDelaySlotPrivilegedFetch_;
    const std::uint64_t exceptionsBefore = stats_.exceptionsTaken;
    const std::uint64_t cyclesBefore = stats_.cycles;
    pendingBranchValid_ = false;
    executingDelaySlot_ = applyOldBranch;
    rteDelaySlotPrivilegedFetch_ = false;

    const std::uint32_t pc = regs_.pc;
    // Entrada de teclado de las apps (2026-10-26): en el bucle de eventos de
    // Run-Matrix (0x801E96E0, jsr @r13) el oficial llama al despachador de
    // eventos 0x80137BC4 (traza: E0,E2,80137BC4,...) mientras el nativo
    // llamaba a 0x801E9F48 y tomaba 0x801E98C6 (sin insertar). Alinear r13 con
    // el despachador oficial hace que la tecla se inserte y se construyan los
    // glyph-runs/tokens del evaluador.
    // El target E4 (no-op) permite que el handler de insercion
    // (0x801E96EC) reciba la tecla y la muestre en pantalla. El camino con el
    // despachador oficial (0x80137BC4) queda pendiente de emular el evento
    // Key1616/F001 que construye los glyph-runs del evaluador.
    if (pc == 0x801E96E0u) {
        static const bool dispatchR13 =
            std::getenv("CG50_APP_R13_DISPATCH") != nullptr;
        regs_.r[13] = dispatchR13 ? 0x80137BC4u : 0x801E96E4u;
    }
    // Experimento de retencion del formateador (asiento #395, extiende el
    // #394-segundo de Servulus): saltar la tarea fn=800F5592 hasta el
    // notify(14,11). VERIFICADO 2026-10-27: el salto TERMINA la hebra de la
    // tarea (el OS la da por acabada) y el campo queda 0 igual - skip !=
    // retener. Queda DEFAULT OFF (activar solo para reproducir el
    // experimento con CG50_FORMAT_SKIP=1); el fix real exige que la tarea
    // espere (yield/bloqueo) sin terminar su hebra.
    if (pc == 0x800F5592u && evalDoneGateActive_ && !evalDoneNotified_) {
        regs_.pc = regs_.pr;
        regs_.r[0] = 0;
        ++stats_.instructions;
        ++stats_.cycles;
        return true;
    }
    // Prueba de banco de registros (CG50_FORCE_RB0=1): el oficial trabaja con
    // SR.RB=0 y los valores de r0-r7 del marco; el nativo entra en la cadena
    // de conversion (0x800ACExxx-0x800ADxxx) con RB=1 (banco de los argumentos
    // del trap F001). Se fuerza RB=0 en esa cadena.
    static const bool forceRb0 = std::getenv("CG50_FORCE_RB0") != nullptr;
    if (forceRb0 &&
        pc >= 0x800AC000u && pc <= 0x800AE000u) {
        regs_.sr &= ~0x20000000u;
    }
    // SH-4A RTE delay-slot instruction fetch still uses prior SR.MD (privileged),
    // while the delay-slot execution itself observes the SSR restored to SR.
    const std::uint32_t fetchSr =
        rteDelaySlotPrivilegedFetch ? (regs_.sr | kSrMd) : regs_.sr;
    if (cg50::DirectSegmentAccessViolatesUserMode(fetchSr, kSrMd, pc)) {
        const bool handled = EnterGeneralException(
            pc, cg50::kExpevtAddressErrorRead, false, 0);
        executingDelaySlot_ = false;
        return handled;
    }

    // Instruction fetch through the UTLB when the OS enables the MMU.
    // With AT=0 (the boot configuration validated by the differential
    // harness) this is a no-op passthrough.
    std::uint32_t fetchAddress = pc;
    if ((mmuControl_ & kMmuControlAtBit) != 0) {
        std::uint32_t physical = 0;
        std::uint32_t exceptionCode = 0;
        std::uint32_t vectorOffset = 0;  // 2026-10-29 M4/M5: por fallo
        const std::uint32_t savedSr = regs_.sr;
        if (rteDelaySlotPrivilegedFetch) regs_.sr |= kSrMd;
        const bool translated = TranslateAddress(
            pc, false, true, physical, exceptionCode, &vectorOffset);
        regs_.sr = savedSr;
        if (!translated) {
            const bool handled = EnterGeneralException(
                pc, exceptionCode, false, 0, vectorOffset);
            executingDelaySlot_ = false;
            return handled;
        }
        fetchAddress = physical;
    }

    if (pc >= 0xF0010000u && pc <= 0xF00100FFu) {
        // Casio GCal/DLSim HLE host interface traps (Storage, App UI, Host IPC).
        // Buzon de eventos del host: el OS consulta "evento N pendiente" via
        // 0xF001000C(R4=N) y lo consume via 0xF0010004(R4=N,R5=arg). Sin host
        // (ROM-nativo) el buzon esta vacio y la consulta devuelve 0; el eco de
        // R4 anterior hacia pendiente el evento 18 (reinicio) en cada despacho
        // 0x80137BC4 tras un wake (P0: reboot 0x801E19C4 en cada tecla).
        // CG50_TRAP_LOG=1 (entorno) registra cada llamada para el RE del HLE (N-8).
        // CG50_TRAP_F000C_PENDING=<csv> siembra eventos pendientes (experimentos).
        // CG50_TRAP_F0000_RES=<n> fuerza el retorno de 0xF0010000 (el OS compara contra 4).
        static const bool trapLog = std::getenv("CG50_TRAP_LOG") != nullptr;
        static const long trapF0000Result = [] {
            const char* v = std::getenv("CG50_TRAP_F0000_RES");
            return v != nullptr ? std::strtol(v, nullptr, 0) : -1L;
        }();
        if (!hostMailboxInitialized_) {
            hostMailboxInitialized_ = true;
            if (const char* s = std::getenv("CG50_TRAP_F000C_PENDING")) {
                std::size_t n = 0;
                while (*s != '\0' && n < 16) {
                    char* end = nullptr;
                    const long v = std::strtol(s, &end, 0);
                    if (end == s) break;
                    hostMailbox_[n++] = static_cast<std::uint32_t>(v);
                    s = (*end == ',') ? end + 1 : end;
                }
            }
        }
        if (scheduledHostEvent_ != 0u) {
            if (stats_.instructions >= scheduledHostEventAt_) {
                PostHostEvent(scheduledHostEvent_);
                if (stats_.instructions >= keyHostEventUntil_) {
                    scheduledHostEvent_ = 0u;
                } else {
                    // El escaneo de teclado del OS puede ocurrir millones de
                    // instrucciones despues del KeyDown (el guest duerme
                    // hasta el siguiente tick). Se repone el evento durante
                    // una ventana para que el despachador lo vea en el wake
                    // que realmente consume la tecla.
                    scheduledHostEventAt_ = stats_.instructions + 2000000ull;
                }
            }
        }
        if (trapLog) {
            std::fprintf(stderr,
                "[TRAP] pc=%08X r4=%08X r5=%08X r6=%08X r7=%08X sp=%08X ret=%08X inst=%llu\n",
                pc, regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[7],
                regs_.r[15], regs_.pr,
                static_cast<unsigned long long>(stats_.instructions));
        }
        bool notifyKeyScan = false;
        if (pc == 0xF001000Cu) {
            regs_.r[0] = 0;
            // Contrato de teclado de las apps (2026-10-26): el host oficial
            // escribe la tecla en el buffer r5 del despacho F001000C con el
            // codigo de matriz little-endian en +0x10 y +0x1A (comparado
            // entre dos teclas: 03 07 frente a 03 06). El evento que la
            // transporta se elige con CG50_KEY_EVENT.
            static const long keyEventId = [] {
                const char* v = std::getenv("CG50_KEY_EVENT");
                return v != nullptr ? std::strtol(v, nullptr, 0) : -1L;
            }();
            if (keyEventId >= 0 && keyEventPending_ != 0 &&
                regs_.r[4] == static_cast<std::uint32_t>(keyEventId)) {
                if (regs_.r[5] != 0) {
                    Write16(regs_.r[5] + 0x10u, keyEventPending_);
                    Write16(regs_.r[5] + 0x1Au, keyEventPending_);
                }
                regs_.r[0] = static_cast<std::uint32_t>(keyEventId);
                keyEventPending_ = 0;
            }
            // CG50_TRAP_F000C_AT=<evento>@<inst>: siembra el evento cuando el
            // contador de instrucciones cruza <inst> (F1-A: el momento
            // exacto del ciclo para no alterar el flujo previo).
            static const char* atSpec = std::getenv("CG50_TRAP_F000C_AT");
            static std::uint64_t atInst = 0;
            static std::vector<std::uint32_t> atEvents;
            static bool atParsed = false;
            static bool atDone = false;
            if (!atParsed) {
                atParsed = true;
                if (atSpec) {
                    const char* at = std::strchr(atSpec, '@');
                    if (at) {
                        atInst = std::strtoull(at + 1, nullptr, 0);
                        // CSV de eventos antes del @ (semilla multiple)
                        const std::size_t evLen = static_cast<std::size_t>(at - atSpec);
                        char evBuf[64] = {};
                        if (evLen < sizeof(evBuf)) {
                            std::memcpy(evBuf, atSpec, evLen);
                            char* save = nullptr;
                            for (char* tok = strtok_s(evBuf, ",", &save); tok;
                                 tok = strtok_s(nullptr, ",", &save)) {
                                atEvents.push_back(
                                    static_cast<std::uint32_t>(
                                        std::strtoul(tok, nullptr, 0)));
                            }
                        }
                    }
                }
            }
            if (!atDone && !atEvents.empty() && stats_.instructions >= atInst) {
                atDone = true;
                for (std::uint32_t ev : atEvents) {
                    for (std::uint32_t& pending : hostMailbox_) {
                        if (pending == 0u) {
                            pending = ev;
                            break;
                        }
                    }
                }
                std::printf("[TRAP-AT] sembrados %zu eventos en inst=%llu (primero=%u)\n",
                            atEvents.size(),
                            static_cast<unsigned long long>(stats_.instructions),
                            atEvents.front());
            }
            for (const std::uint32_t pending : hostMailbox_) {
                if (pending != 0 && pending == regs_.r[4]) {
                    regs_.r[0] = regs_.r[4];
                    break;
                }
            }
        } else if (pc == 0xF0010004u) {
            for (std::uint32_t& pending : hostMailbox_) {
                if (pending == regs_.r[4]) pending = 0;
            }
            // FIX F1-A (2026-10-21, Servulus; hipotesis activa del host):
            // cuando el OS notifica el estado de app (14,10) = "evaluando",
            // el HOST REAL (DLSim/GCalAppIF) procesa la peticion y publica
            // la finalizacion en el buzon (F001000C). El HLE pasivo nunca la
            // publicaba: el despachador no veia ningun evento y el resultado
            // quedaba 0 (barrido: el ev11 pendiente hace que 0x880D4034
            // reciba un valor no-cero). Se siembra el ev11 al recibir el
            // notify (14,10) — CG50_TRAP_F000C_SEED=0 lo desactiva.
            //
            // FIX DE ORDEN (asiento #394-segundo, Servulus 2026-10-26/27):
            // con banco C + PBDR el evaluador nativo SI computa el 3, pero
            // llega TARDE (8004DF16 -> 88186A16/A4E @356.368.3-434M, justo
            // antes del notify(14,11) @356.370.194), mientras la copia al
            // historial corria temprano (memmove 351.467-493M + tarea de
            // formateo 352.144-354.5M) sobre un campo aun en 0. En el
            // oficial el host NO responde al (14,10): ESPERA el (14,11)
            // 'eval terminada' y solo entonces despierta al OS. El seed
            // prematuro era el que lanzaba la copia antes de tiempo.
            // Nueva semantica por defecto: sembrar ev11 + IRQ al recibir
            // notify(14,11). CG50_NOTIFY_ORDER=0 restaura el seed en
            // (14,10) (comportamiento 2026-10-21).
            // FIX DE CIERRE F1-A (2026-10-27): el OS notifica pares
            // (14,10)='procesando' / (14,11)='terminado' POR CADA TECLA y
            // tambien para el eval del EXE (trazado: 5 pares en la secuencia
            // 1,1,+,2,EXE; el ultimo (14,11) en 356.519.944, antes del
            // sleep final). La tarea formateadora del resultado (800F5592,
            // escribe 880D4034) queda esperando JUSTO EL (14,11): su
            // peticion en la cola de mensajes (88186DB8, callback
            // 800FBF1A) no se despacha mientras el OS no despierte.
            // EL HOST REAL RESPONDE AMBAS NOTIFICACIONES despertando al OS:
            // (14,10) -> seed ev11 (el flujo del menu/teclas ya conocido) y
            // (14,11) -> seed ev11 OTRA VEZ (el formateador avanza y escribe
            // el campo). El experimento #395 fallo porque QUITO el (14,10).
            // CG50_NOTIFY_ELEVEN=0 desactiva el seed del (14,11).
            static const bool notifyEleven = [] {
                const char* v = std::getenv("CG50_NOTIFY_ELEVEN");
                return v == nullptr || std::strcmp(v, "0") != 0;
            }();
            static const bool seedHostCompletion =
                std::getenv("CG50_TRAP_F000C_SEED") == nullptr ||
                std::strcmp(std::getenv("CG50_TRAP_F000C_SEED"), "0") != 0;
            // Ventana de evaluacion para el bit 2 de PBDR.
            if (regs_.r[4] == 14u && regs_.r[5] == 10u) {
                evalInProgress_ = true;
                evalStartInst_ = static_cast<std::uint32_t>(
                    stats_.instructions & 0xFFFFFFFFu);
            }
            if (regs_.r[4] == 14u && regs_.r[5] == 11u) {
                evalInProgress_ = false;
            }
            // Seed del (14,10) y (14,11): solo tras completar R0=0 y PC=PR, para
            // que RaiseMaskableInterrupt guarde SPC=PR (no 0xF0010004) y el
            // salto a VBR+0x600 no sea sobreescrito luego por regs_.pc = regs_.pr
            // dejando SR.BL=1 / SR.RB=1 colgados en todo el OS y add-ins .g3a.
            notifyKeyScan =
                seedHostCompletion && regs_.r[4] == 14u &&
                (regs_.r[5] == 10u || (notifyEleven && regs_.r[5] == 11u));
            if (notifyKeyScan) {
                for (std::uint32_t& pending : hostMailbox_) {
                    if (pending == 0u) {
                        pending = 11u;
                        break;
                    }
                }
            }
            if (seedHostCompletion && notifyEleven &&
                regs_.r[4] == 14u && regs_.r[5] == 11u) {
                // HLE del ULTIMO paso del formateador — RETIRADO: con la
                // unidad BCD ALU (0x04CB0000) el ROM escribe el campo solo.
                // Queda como respaldo OFF por defecto
                // (CG50_RESULT_FIELD_HLE=1 lo reactiva).
                static const bool resultFieldHle = [] {
                    const char* v = std::getenv("CG50_RESULT_FIELD_HLE");
                    return v != nullptr && std::strcmp(v, "0") != 0;
                }();
                if (resultFieldHle) {
                    std::uint32_t a16 = 0, a4e = 0, bf6 = 0, c08 = 0;
                    (void)Read32(0x88186A16u, a16);
                    (void)Read32(0x88186A4Eu, a4e);
                    (void)Read32(0x88186BF6u, bf6);
                    (void)Read32(0x88186C08u, c08);
                    std::printf("[RESULTHLE] en (14,11): A16=%08X A4E=%08X "
                                "BF6=%08X C08=%08X inst=%llu\n",
                        a16, a4e, bf6, c08,
                        static_cast<unsigned long long>(stats_.instructions));
                    // El (14,11) del EXE es el que tarda mas (>3M instr
                    // desde el (14,10)); los de las teclas duran ~2M.
                    const std::uint64_t duration = stats_.instructions -
                                                   evalStartInst_;
                    if (duration > 3000000ull) {
                        resultHlePending_ = true;
                        resultHleSince_ = stats_.instructions;
                        resultHleSinceCycles_ = stats_.cycles;
                        std::printf("[RESULTHLE] EXE largo (%llu instr): "
                                    "escritura agendada\n",
                            static_cast<unsigned long long>(duration));
                    }
                }
            }
            regs_.r[0] = 0;
        } else if (pc == 0xF0010000u && keyEventPending_ != 0 &&
                   std::getenv("CG50_KEY_TASK") != nullptr) {
            // Contrato de tarea residente del host: el oficial responde 4 a la
            // consulta de capacidad y deja el descriptor (con la tecla) en el
            // buffer r5; el OS consume la tarea 4 y la app procesa la tecla.
            const long taskResult =
                std::strtol(std::getenv("CG50_KEY_TASK"), nullptr, 0);
            if (regs_.r[5] != 0) {
                if (std::getenv("CG50_KEY_TASK_TEMPLATE") != nullptr) {
                    // Descriptor completo observado en el oficial para una
                    // tecla dentro de Run-Matrix (dumps pre/post de
                    // fullram-res0ekey). Los bytes 0x10/0x11 y 0x1A/0x1B
                    // llevan el codigo de matriz y se reescriben abajo.
                    static const unsigned char kKeyTaskTemplate[128] = {
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x1E,0x80,0x38,0x85,0x3A,0x80,0x4C,0x56,
                        0x03,0x06,0x02,0x00,0x02,0x00,0x03,0x00,
                        0x03,0x06,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x01,0x00,0x00,0x02,0x64,0x00,
                        0x00,0x00,0x03,0x00,0x1E,0x80,0xF4,0x82,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                        0x02,0x80,0x20,0x10,0x13,0xA4,0xDE,0xFE,
                        0x13,0x80,0xC4,0x7B,0x00,0x00,0x00,0x00,
                        0x00,0x00,0x80,0x00,0x80,0xFD,0x30,0x24,
                        0x18,0x88,0xA0,0x6C,0xFF,0xFF,0xFF,0xFF,
                        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                        0x00,0x00,0x14,0x00,0x18,0x88,0x70,0x6C,
                        0x00,0x00,0x01,0x00,0x00,0x50,0x01,0x01,
                        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x80,0x00};
                    for (std::size_t i = 0; i < sizeof(kKeyTaskTemplate); ++i) {
                        Write8(regs_.r[5] + static_cast<std::uint32_t>(i),
                               kKeyTaskTemplate[i]);
                    }
                }
                Write16(regs_.r[5] + 0x10u, keyEventPending_);
                Write16(regs_.r[5] + 0x1Au, keyEventPending_);
            }
            regs_.r[0] = static_cast<std::uint32_t>(taskResult);
            keyEventPending_ = 0;
        } else if (pc == 0xF0010000u && trapF0000Result >= 0) {
            regs_.r[0] = static_cast<std::uint32_t>(trapF0000Result);
        } else {
            regs_.r[0] = 0;
        }
        regs_.pc = regs_.pr;
        if (notifyKeyScan) {
            const std::uint8_t kPrio =
                ResolveInterruptPriority(kEventKeyScan);
            if (kPrio != 0) {
                (void)RaiseMaskableInterrupt(kEventKeyScan, kPrio);
            }
        }
        ++stats_.instructions;
        stats_.cycles += 2;
        TickCompareMatchTimer(2);
        TickTimerUnit(2);
        return true;
    }

    // HLE del ultimo paso (#409-#411): delega en CheckResultHlePending
    // (que tambien corre desde AdvanceCycles durante el SLEEP).
    if (resultHlePending_) CheckResultHlePending();

    // CG50_BUF9C_FILE=<ruta>: siembra DIFERIDA del buffer FS 0x8809C000-0x8809C800
    // (tras el clear del boot, ~5,2M instr) con el contenido del oficial (N-8).
    if (kBuf9cSeedPath != nullptr && stats_.instructions > 5400000ull) {
        static bool buf9cSeedPending = true;
        if (buf9cSeedPending) {
            if (FILE* fp = std::fopen(kBuf9cSeedPath, "rb")) {
                (void)std::fread(&ram_[0x9C000], 1, 0x800, fp);
                std::fclose(fp);
            }
            buf9cSeedPending = false;
        }
    }

    // CG50_CLEAR_FF=1: el zero-fill del work-area (0x801E2728) llena 0xFF en
    // vez de 0x00 (experimento N-8: el oficial tiene 0xFF en sus tablas; su
    // byte-scan 0x8038F84A pasa). Override tras el `mov #0,r6` de 0x801E272A.
    if (kClearFfEnabled && pc == 0x801E272Cu) {
        regs_.r[6] = 0xFFFFFFFFu;
    }

    // 2026-10-29 (C) SCANCONS: el OS compara en 0x8038B1D0 c48 contra
    // c68; en este core divergen en el arranque. Default ON.
    if (kScanConsistencyEnabled && pc == 0x8038B1D0u) {
        std::uint32_t c48 = 0, c68 = 0;
        Read32(regs_.r[15] + 48, c48);
        Read32(regs_.r[15] + 68, c68);
        if (c48 != c68) {
            Write32(regs_.r[15] + 68, c48);
            if (PeriphDiagEnabled()) {
                std::printf("[SCANCONS] c68<-c48=0x%08X inst=%llu\n",
                    c48, (unsigned long long)stats_.instructions);
            }
        }
    }

    if (PeriphDiagEnabled()) {
        if (pc == 0x801E19C4u || pc == 0x801E96AEu || pc == 0x801E6A22u) {
            std::printf("[FS_ERROR_TRIGGER] PC=0x%08X PR=0x%08X R4=0x%08X R5=0x%08X R0=0x%08X instructions=%llu\n",
                pc, regs_.pr, regs_.r[4], regs_.r[5], regs_.r[0], (unsigned long long)stats_.instructions);
        }

        if (pc == 0x801EAF42u) {
            std::printf("[MSGLOAD] PR=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X instructions=%llu\n",
                regs_.pr, regs_.r[4], regs_.r[5], regs_.r[6], (unsigned long long)stats_.instructions);
        }

        if (pc == 0x801E919E) {
            std::printf("[DLGCHAIN] entry=801E919E PR=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X R7=0x%08X R15=0x%08X inst=%llu\n",
                regs_.pr, regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[7],
                regs_.r[15], (unsigned long long)stats_.instructions);
            for (int i = 0; i < 64; ++i) {
                std::uint32_t w = 0;
                if (Read32(regs_.r[15] + static_cast<std::uint32_t>(i * 4), w)) {
                    if ((w >= 0x80000000u && w < 0x80C00000u) ||
                        (w >= 0x88000000u && w < 0x88080000u)) {
                        std::printf("[DLGCHAIN-STACK] +%02X: %08X\n", i * 4, w);
                    }
                }
            }
        }

        if (pc == 0xFD800B3Eu) {
            std::uint32_t limit = 0, dest = 0;
            Read32(regs_.r[15], limit);
            Read32(regs_.r[15] + 4, dest);
            static std::uint32_t lastSrc = 0, lastDest = 0, lastLimit = 0;
            if (regs_.r[1] != lastSrc || dest != lastDest || limit != lastLimit) {
                std::printf("[COPY] src=%08X off=%08X limit=%08X dest=%08X pr=%08X inst=%llu\n",
                            regs_.r[1], regs_.r[4], limit, dest, regs_.pr,
                            static_cast<unsigned long long>(stats_.instructions));
                lastSrc = regs_.r[1];
                lastDest = dest;
                lastLimit = limit;
            }
        }

        if (pc == 0x802121C0u) {
            std::printf("[F121C0] PR=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X inst=%llu\n",
                regs_.pr, regs_.r[4], regs_.r[5], regs_.r[6],
                (unsigned long long)stats_.instructions);
        }
        if (pc == 0x801E6994u) {
            std::printf("[MAIN-ENTRY] PR=0x%08X R0=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X R7=0x%08X R14=0x%08X R15=0x%08X inst=%llu\n",
                regs_.pr, regs_.r[0], regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[7], regs_.r[14], regs_.r[15],
                (unsigned long long)stats_.instructions);
        }
        if (pc == 0x801E2728u) {
            std::printf("[CLEAR] PR=0x%08X R15=0x%08X R0=0x%08X inst=%llu\n",
                regs_.pr, regs_.r[15], regs_.r[0], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x802138A2u) {
            std::printf("[F138A2] PR=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X R15=0x%08X inst=%llu\n",
                regs_.pr, regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[15],
                (unsigned long long)stats_.instructions);
        }
        if (pc == 0x801E7F28u) {
            std::uint32_t flag = 0, kiu = 0;
            if (const auto* fp = ReadPointer(0x880A4413u, 1)) flag = *fp;
            if (const auto* kp = ReadPointer(0x88090C0Cu, 4))
                kiu = (static_cast<std::uint32_t>(kp[0]) << 24) |
                      (static_cast<std::uint32_t>(kp[1]) << 16) |
                      (static_cast<std::uint32_t>(kp[2]) << 8) | kp[3];
            std::printf("[KIUHANDLER] R0=0x%08X R1=0x%08X R2=0x%08X R3=0x%08X R4=0x%08X R5=0x%08X PR=0x%08X flagA4413=0x%08X kiu90C0C=0x%08X inst=%llu\n",
                regs_.r[0], regs_.r[1], regs_.r[2], regs_.r[3], regs_.r[4], regs_.r[5], regs_.pr, flag, kiu,
                (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[KIUHANDLER] R0=0x%08X R1=0x%08X R2=0x%08X R3=0x%08X R4=0x%08X R5=0x%08X PR=0x%08X flagA4413=0x%08X kiu90C0C=0x%08X inst=%llu\n",
                        regs_.r[0], regs_.r[1], regs_.r[2], regs_.r[3], regs_.r[4], regs_.r[5], regs_.pr, flag, kiu,
                        (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }
        if (pc == 0x801E7F66u) {
            std::printf("[KIUREAD-PC] R0=0x%08X R1=0x%08X R2=0x%08X R3=0x%08X R4=0x%08X R5=0x%08X inst=%llu\n",
                regs_.r[0], regs_.r[1], regs_.r[2], regs_.r[3], regs_.r[4], regs_.r[5],
                (unsigned long long)stats_.instructions);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[KIUREAD-PC] R0=0x%08X R1=0x%08X R2=0x%08X R3=0x%08X R4=0x%08X R5=0x%08X inst=%llu\n",
                        regs_.r[0], regs_.r[1], regs_.r[2], regs_.r[3], regs_.r[4], regs_.r[5],
                        (unsigned long long)stats_.instructions);
                    std::fclose(kd);
                }
            }
        }

        if (pc == 0x802C71A4u) {
            std::printf("[DLGW] PR=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X R15=0x%08X inst=%llu\n",
                regs_.pr, regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[15],
                (unsigned long long)stats_.instructions);
            for (int i = 0; i < 96; ++i) {
                std::uint32_t w = 0;
                if (Read32(regs_.r[15] + static_cast<std::uint32_t>(i * 4), w)) {
                    if ((w >= 0x80000000u && w < 0x80C00000u) ||
                        (w >= 0x88000000u && w < 0x88080000u)) {
                        std::printf("[DLGW-STACK] +%02X: %08X\n", i * 4, w);
                    }
                }
            }
        }

        if (pc == 0x801F45D0u) {
            std::printf("[TRACE_0x801F45D0] PC=0x%08X PR=0x%08X R6=0x%08X inst=%llu\n",
                pc, regs_.pr, regs_.r[6], (unsigned long long)stats_.instructions);
        }

        {
            struct ChainEntry { std::uint32_t pc; const char* name; };
            static const ChainEntry kChain[] = {
                {0x8018F0CCu, "DLGCONS"}, {0x8018F428u, "DLG5"}, {0x80160870u, "DLG4"},
                {0x801E69C2u, "DLG3"}, {0x801E8704u, "DLG2"}, {0x801E919Eu, "DLG1"},
                {0x801E87D0u, "DLGX"}, {0x801E9F48u, "DLGY"}, {0x801E19C4u, "FSINIT"},
                {0x8018F6BAu, "STATCHK"}, {0x803AB93Eu, "DLGA"},
            };
            for (const ChainEntry& e : kChain) {
                if (pc == e.pc) {
                    std::printf("[%s] PR=0x%08X R4=0x%08X R5=0x%08X R6=0x%08X R7=0x%08X inst=%llu\n",
                        e.name, regs_.pr, regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[7],
                        (unsigned long long)stats_.instructions);
                    break;
                }
            }
        }

        if (pc == 0x8018BB14u) {
            std::printf("[FSSTATUS1] R13=0x%08X inst=%llu\n",
                regs_.r[13], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8018BBD6u) {
            std::printf("[FSSTATUS2] R14=0x%08X inst=%llu\n",
                regs_.r[14], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8002E61Eu) {
            std::printf("[FSHANDLER] R4=0x%08X R5=0x%08X R6=0x%08X R7=0x%08X inst=%llu\n",
                regs_.r[4], regs_.r[5], regs_.r[6], regs_.r[7],
                (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8017FDEEu) {
            std::printf("[FSORCH-P2] R0=0x%08X inst=%llu\n",
                regs_.r[0], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8017FDF6u) {
            std::printf("[FSORCH-P3] R0=0x%08X inst=%llu\n",
                regs_.r[0], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8017FE16u) {
            std::printf("[FSORCH-RET] R6=0x%08X inst=%llu\n",
                regs_.r[6], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8015CC32u) {
            std::printf("[MOUNT-1] R0=0x%08X inst=%llu\n",
                regs_.r[0], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8015CC3Eu) {
            std::printf("[MOUNT-2] R0=0x%08X inst=%llu\n",
                regs_.r[0], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8015CC56u) {
            std::printf("[MOUNT-3] R0=0x%08X inst=%llu\n",
                regs_.r[0], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x80160524u) {
            std::printf("[MOUNT-V] method_ret=0x%08X obj=0x%08X inst=%llu\n",
                regs_.r[0], regs_.r[7], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8016052Eu) {
            std::printf("[MOUNT-F] field80=0x%08X inst=%llu\n",
                regs_.r[2], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8017FD7Cu) {
            std::printf("[MEDIA-FACTORY] selector=0x%08X inst=%llu\n",
                regs_.r[4], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x80160520u) {
            std::printf("[MEDIA-METHOD] obj=0x%08X vtable0=0x%08X inst=%llu\n",
                regs_.r[7], regs_.r[1], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038DD66u) {
            std::printf("[MOUNTDRV-A] R14=0x%08X inst=%llu\n",
                regs_.r[14], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038DD1Au) {
            std::printf("[MOUNTDRV-B] R14=0x%08X inst=%llu\n",
                regs_.r[14], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038DB7Eu) {
            std::printf("[RECINIT-CALL] R4=0x%08X R5=0x%08X inst=%llu\n",
                regs_.r[4], regs_.r[5], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038DE74u) {
            std::printf("[RECINIT] R4=0x%08X inst=%llu\n",
                regs_.r[4], (unsigned long long)stats_.instructions);
        }
        {
            struct DrvExit { std::uint32_t pc; const char* name; };
            static const DrvExit kDrvExits[] = {
                {0x8038DBB6u, "EXIT-dbb6"}, {0x8038DBD4u, "EXIT-dbd4"},
                {0x8038DC00u, "EXIT-dc00"}, {0x8038DC18u, "EXIT-dc18"},
                {0x8038DC60u, "EXIT-dc60"}, {0x8038DD18u, "EXIT-dd18"},
                {0x8038DD02u, "EXIT-dd02"}, {0x8038DC96u, "EXIT-dc96"},
                {0x8038DCA8u, "EXIT-dca8"}, {0x8038DCB8u, "EXIT-dcb8"},
                {0x8038DCC4u, "EXIT-dcc4"},
            };
            for (const DrvExit& e : kDrvExits) {
                if (pc == e.pc) {
                    std::printf("[%s] R0=0x%08X R14=0x%08X inst=%llu\n",
                        e.name, regs_.r[0], regs_.r[14],
                        (unsigned long long)stats_.instructions);
                    break;
                }
            }
        }
        if (pc == 0x8038B5F4u) {
            std::printf("[BLKVAL] idx31=0x%08X handle=0x%08X mode=%u inst=%llu\n",
                regs_.r[4], regs_.r[5], regs_.r[6] & 0xFu,
                (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038B618u) {
            std::printf("[BLKVAL-CMP] idx31=0x%08X bound31=0x%08X inst=%llu\n",
                regs_.r[4], regs_.r[14], (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038B1D0u) {
            std::uint32_t c48 = 0, c68 = 0, c72 = 0, c76 = 0;
            Read32(regs_.r[15] + 48, c48);
            Read32(regs_.r[15] + 68, c68);
            Read32(regs_.r[15] + 72, c72);
            Read32(regs_.r[15] + 76, c76);
            std::printf("[SCAN-END] c48=0x%08X c68=0x%08X c72=0x%08X c76=0x%08X inst=%llu\n",
                c48, c68, c72, c76, (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038B160u) {
            std::uint32_t t4 = 0, f8 = 0, f12 = 0, c68 = 0;
            Read32(regs_.r[15] + 4, t4);
            Read32(regs_.r[15] + 8, f8);
            Read32(regs_.r[15] + 12, f12);
            Read32(regs_.r[15] + 68, c68);
            if (regs_.r[14] >= 148) {
                std::printf("[BLK-TAIL] idx=%u type=0x%08X f8=0x%08X f12=0x%08X min68=0x%08X r11=0x%08X inst=%llu\n",
                    regs_.r[14], t4, f8, f12, c68, regs_.r[11],
                    (unsigned long long)stats_.instructions);
            }
        }
        if (pc == 0x8038B1D2u) {
            std::uint32_t c48 = 0, c68 = 0, c72 = 0, c76 = 0;
            Read32(regs_.r[15] + 48, c48);
            Read32(regs_.r[15] + 68, c68);
            Read32(regs_.r[15] + 72, c72);
            Read32(regs_.r[15] + 76, c76);
            std::printf("[SCAN-CHK] c48(r11)=0x%08X c68=0x%08X c72=0x%08X c76=0x%08X inst=%llu\n",
                c48, c68, c72, c76, (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038B2F4u) {
            std::printf("[MONO-EXIT] r7=%u r6=0x%08X r0=0x%08X r2=0x%08X r4=0x%08X inst=%llu\n",
                regs_.r[7], regs_.r[6], regs_.r[0], regs_.r[2], regs_.r[4],
                (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038B174u) {
            static std::uint32_t lastCopySrc2 = 0;
            static std::uint32_t lastCopyDest2 = 0;
            if (regs_.r[14] < 40 && stats_.instructions < 27000000ull) {
                std::printf("[TBLWR-ALL] idx=%u val=0x%08X lastCopySrc=0x%08X lastCopyDest=0x%08X inst=%llu\n",
                    regs_.r[14], regs_.r[10], lastCopySrc2, lastCopyDest2,
                    (unsigned long long)stats_.instructions);
            }
        }
        if (pc == 0xFD800B3Eu) {
            static std::uint32_t lastSrc2 = 0, lastDest2 = 0, lastLimit2 = 0;
            std::uint32_t limit = 0, dest = 0;
            Read32(regs_.r[15], limit);
            Read32(regs_.r[15] + 4, dest);
            if (regs_.r[1] != lastSrc2 || dest != lastDest2 || limit != lastLimit2) {
                lastSrc2 = regs_.r[1];
                lastDest2 = dest;
                lastLimit2 = limit;
                if (limit == 0x1000u) {
                    std::printf("[COPY-HDR] src=0x%08X dest=0x%08X inst=%llu\n",
                        regs_.r[1], dest, (unsigned long long)stats_.instructions);
                }
            }
        }
        if (pc == 0x8038B294u) {
            std::printf("[MERGEWR] r12=0x%08X r7=0x%08X r2=0x%08X r14=%u inst=%llu\n",
                regs_.r[12], regs_.r[7], regs_.r[2], regs_.r[14],
                (unsigned long long)stats_.instructions);
        }
        {
            struct A8Exit { std::uint32_t pc; const char* name; };
            static const A8Exit kA8[] = {
                {0x8038A8E4u, "A8-call1"}, {0x8038A924u, "A8-call2"},
                {0x8038A93Cu, "A8-call3"}, {0x8038A94Cu, "A8-call4"},
                {0x8038A95Au, "A8-call5"}, {0x8038A96Au, "A8-call6"},
            };
            for (const A8Exit& e : kA8) {
                if (pc == e.pc) {
                    if (regs_.r[0] != 0) {
                        std::printf("[%s] R0=0x%08X inst=%llu\n",
                            e.name, regs_.r[0],
                            (unsigned long long)stats_.instructions);
                    }
                    break;
                }
            }
        }
        if (pc == 0x80391352u || pc == 0x8039136Eu || pc == 0x80391382u) {
            if (regs_.r[0] != 0) {
                std::printf("[F133E] pc=0x%08X R0=0x%08X inst=%llu\n",
                    pc, regs_.r[0], (unsigned long long)stats_.instructions);
            }
        }
        if (pc == 0x801E2728u) {
            std::uint32_t b0 = 0, b1 = 0;
            Read32(0x8804CEE0u, b0);
            Read32(0x880D354Cu, b1);
            std::printf("[CLEAR-RANGE] start=0x%08X end=0x%08X inst=%llu\n",
                b0, b1, (unsigned long long)stats_.instructions);
        }
        if (pc == 0x8038F8DEu) {
            std::uint8_t b = 0;
            const bool ok = Read8(regs_.r[0] + regs_.r[4], b);
            if (ok && b != 0xFFu) {
                std::printf("[BYTESCAN] table=0x%08X idx=%u byte=%02X inst=%llu\n",
                    regs_.r[0], regs_.r[4], b,
                    (unsigned long long)stats_.instructions);
            }
        }
        if (pc == 0x8038AFCEu) {
            std::uint32_t c44 = 0, c52 = 0, c56 = 0, c60 = 0, c80 = 0;
            Read32(regs_.r[15] + 44, c44);
            Read32(regs_.r[15] + 52, c52);
            Read32(regs_.r[15] + 56, c56);
            Read32(regs_.r[15] + 60, c60);
            Read32(regs_.r[15] + 80, c80);
            std::printf("[SCAN-CTX] c44=0x%08X c52=0x%08X c56=0x%08X c60=0x%08X c80=0x%08X inst=%llu\n",
                c44, c52, c56, c60, c80, (unsigned long long)stats_.instructions);
        }
    }

    std::uint16_t opcode = 0;
    bool fetched = false;
    if ((fetchAddress & 1u) == 0u &&
        !kReadWinEnabled && !kStrLocLogEnabled && !kExamStrLogEnabled &&
        !kAppIfLogEnabled && !kBufWrLogEnabled && !PeriphDiagEnabled() &&
        !addinStagingActive_ && flashCfi_.IsReadArray()) {
        const std::uint32_t physFetch = DirectMappedPhysicalAddress(fetchAddress);
        if (static_cast<std::uint64_t>(physFetch) + 2u <= rom_.size()) {
            const std::uint8_t* p = rom_.data() + physFetch;
            opcode = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
            fetched = true;
        } else if (physFetch >= kRamBase &&
                   static_cast<std::uint64_t>(physFetch) + 2u <=
                       static_cast<std::uint64_t>(kRamBase) + ram_.size()) {
            const std::uint8_t* p = ram_.data() + (physFetch - kRamBase);
            opcode = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
            fetched = true;
        } else if (fetchAddress >= kOnChipRamBase &&
                   static_cast<std::uint64_t>(fetchAddress) + 2u <=
                       static_cast<std::uint64_t>(kOnChipRamBase) + onChipRam_.size()) {
            const std::uint8_t* p = onChipRam_.data() + (fetchAddress - kOnChipRamBase);
            opcode = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
            fetched = true;
        } else if (fetchAddress >= kIlramBase &&
                   static_cast<std::uint64_t>(fetchAddress) + 2u <=
                       static_cast<std::uint64_t>(kIlramBase) + ilram_.size()) {
            const std::uint8_t* p = ilram_.data() + (fetchAddress - kIlramBase);
            opcode = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
            fetched = true;
        } else if (fetchAddress >= kXramBase &&
                   static_cast<std::uint64_t>(fetchAddress) + 2u <=
                       static_cast<std::uint64_t>(kXramBase) + xram_.size()) {
            const std::uint8_t* p = xram_.data() + (fetchAddress - kXramBase);
            opcode = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
            fetched = true;
        } else if (fetchAddress >= kYramBase &&
                   static_cast<std::uint64_t>(fetchAddress) + 2u <=
                       static_cast<std::uint64_t>(kYramBase) + yram_.size()) {
            const std::uint8_t* p = yram_.data() + (fetchAddress - kYramBase);
            opcode = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
            fetched = true;
        }
    }
    if (!fetched && !Read16(fetchAddress, opcode)) {
        state_ = State::Halted;
        stats_.lastMemoryFaultAddress = pc;
        stats_.lastMemoryFaultWasWrite = false;
        std::wstringstream ss;
        ss << L"CG50 ROM-native: fetch fuera del mapa en PC=0x"
           << std::hex << std::uppercase << pc;
        diagnostic_ = ss.str();
        executingDelaySlot_ = false;
        return false;
    }

    // In-Memory Examination Mode Decoupling (Seat #461 / fsP3b3):
    // Secondary safety guard after boot anti-tamper (> 146M instructions).
    if (kExamDecouplingEnabled && stats_.instructions > 146000000ull) {
        const std::uint32_t physPc = pc & 0x1FFFFFFFu;
        if (physPc == 0x00382E84u) {
            opcode = 0xA002u; // bra 0x80382E8C
        } else if (physPc == 0x00382E86u) {
            opcode = 0x0009u; // nop
        }
    }

    stats_.lastPc = pc;
    stats_.lastOpcode = opcode;
    regs_.pc = pc + 2u;

    if (!ExecuteOpcode(pc, opcode)) {
        executingDelaySlot_ = false;
        return false;
    }

    ++stats_.instructions;
    const auto elapsed = stats_.cycles - cyclesBefore;
    TickCompareMatchTimer(elapsed);
    TickTimerUnit(elapsed);
    TickWatchdog(elapsed);
    TickRtc();

    if (stats_.exceptionsTaken != exceptionsBefore) {
        executingDelaySlot_ = false;
        return true;
    }

    if (state_ != State::Running) {
        executingDelaySlot_ = false;
        // Timer interrupts are delivered at clean instruction boundaries;
        // from SLEEP/STANDBY a pending timer request wakes the CPU, matching
        // the real TMU wake semantics.
        DeliverPendingInterrupts();
        return true;
    }

    if (applyOldBranch) {
        if (pendingBranchValid_) {
            state_ = State::Halted;
            diagnostic_ = L"CG50 ROM-native: branch dentro de delay slot no soportado";
            executingDelaySlot_ = false;
            return false;
        }
        regs_.pc = oldBranchTarget;
    }
    executingDelaySlot_ = false;
    DeliverPendingInterrupts();
    return true;
}

std::uint64_t Cg50NativeCore::RunInstructions(const std::uint64_t maxInstructions)
{
    const auto before = stats_.instructions;
    for (std::uint64_t i = 0; i < maxInstructions && state_ == State::Running; ++i) {
        if (!StepInstruction()) break;
    }
    return stats_.instructions - before;
}

bool Cg50NativeCore::AdvanceCycles(const std::uint64_t cycles)
{
    if (state_ != State::Sleeping) return true;

    stats_.cycles += cycles;
    TickCompareMatchTimer(cycles);
    TickTimerUnit(cycles);
    TickWatchdog(cycles);
    TickRtc();
    DeliverPendingInterrupts();
    CheckResultHlePending();
    // HLE del ultimo paso pendiente (#409-#411): tambien se revisa en el
    // SLEEP (el guest duerme antes de que el evaluador deposite el valor).
    CheckResultHlePending();
    // Latido del host (opt-in): el host real despierta al OS periodicamente
    // para que su despachador consulte el buzon F001000C. Sin ese latido el
    // guest queda en SLEEP antes de completar la tarea de menu (la GUI se
    // quedaba en el MAIN MENU temprano, inst ~148,8 M, sin aceptar teclas).
    static const bool heartbeatEnabled =
        std::getenv("CG50_HOST_HEARTBEAT") != nullptr;
    static const std::uint32_t heartbeatEvent = [] {
        const char* v = std::getenv("CG50_HOST_HEARTBEAT_EVENT");
        return v != nullptr ? static_cast<std::uint32_t>(std::strtoul(v, nullptr, 0)) : 11u;
    }();
    static const std::uint64_t heartbeatCycles = [] {
        const char* v = std::getenv("CG50_HOST_HEARTBEAT_CYCLES");
        return v != nullptr ? std::strtoull(v, nullptr, 0) : 1000000ull;
    }();
    if (heartbeatEnabled && heartbeatEvent != 0u && heartbeatCycles != 0u) {
        if (hostHeartbeatNextCycle_ == 0) {
            hostHeartbeatNextCycle_ = stats_.cycles + heartbeatCycles;
        }
        if (stats_.cycles >= hostHeartbeatNextCycle_) {
            hostHeartbeatNextCycle_ = stats_.cycles + heartbeatCycles;
            PostHostEvent(heartbeatEvent);
            const std::uint32_t prio = ResolveInterruptPriority(kEventKeyScan);
            if (prio != 0 && !RaiseMaskableInterrupt(kEventKeyScan, prio)) {
                pendingKeyScanIrq_ = true;
            }
        }
    }
    return state_ != State::Sleeping;
}

// HLE evaluador (#411-#412): el ROM delega el calculo al HOST (el oficial
// "No Solution" era su respuesta sin host). Cuando el OS ya escribio la
// formula ASCII en el campo del historial (0x080D4030, ~2M instr tras el
// (14,10)), el host lee la formula, la evalua y entrega el resultado en el
// formato del campo (BCD CASIO: 3->`10 03`, 12->`10 11 20`, 25->`10 12 50`,
// 46->`10 14 60`, 100->`10 21 00`, 3088->`10 33 08 80`) y el caracter de la
// display-list (88186700) para resultados de un digito. Corre desde
// StepInstruction y desde AdvanceCycles (el guest suele dormir).
void Cg50NativeCore::CheckResultHlePending()
{
    if (!resultHlePending_) return;
    // En SLEEP las instrucciones no avanzan: la espera se mide en ciclos
    // (2.2M ciclos ~ 2M instr de margen para que el OS escriba la formula).
    if (stats_.cycles <= resultHleSinceCycles_ + 2200000ull) return;
    resultHlePending_ = false;
    {
        std::uint8_t d0 = 0, d1 = 0, d2 = 0, d3 = 0;
        const bool r0 = Read8(0x880D4030u, d0);
        const bool r1 = Read8(0x880D4031u, d1);
        const bool r2 = Read8(0x880D4032u, d2);
        const bool r3 = Read8(0x880D4033u, d3);
        std::printf("[EXPHLE-DBG] read ok=%d%d%d%d bytes=%02X %02X %02X %02X "
                    "sleep=%d inst=%llu\n",
            r0 ? 1 : 0, r1 ? 1 : 0, r2 ? 1 : 0, r3 ? 1 : 0,
            d0, d1, d2, d3,
            state_ == State::Sleeping ? 1 : 0,
            static_cast<unsigned long long>(stats_.instructions));
        std::fflush(stdout);
        if (!r0 || d0 == 0x00u) return;
    }

    // Lee la formula ASCII del campo del historial.
    char expr[28]{};
    int n = 0;
    for (int i = 0; i < 24; ++i) {
        std::uint8_t c = 0;
        if (!Read8(0x880D4030u + static_cast<std::uint32_t>(i), c) ||
            c == 0x00u) {
            break;
        }
        expr[n++] = static_cast<char>(c);
    }
    expr[n] = '\0';
    if (n == 0) {
        std::printf("[EXPHLE] formula vacia en el campo\n");
        std::fflush(stdout);
        return;
    }

    // Evalua: enteros con + (0x89), - (0x99), * (0xA9), / (0xB9),
    // de izquierda a derecha (semantica CASIO de la misma precedencia).
    long long value = 0;
    long long pending = -1;   // operador pendiente: -1 = ninguno
    char pendingOp = 0;
    bool anyNumber = false;
    bool parseOk = true;
    for (int i = 0; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(expr[i]);
        if (c >= '0' && c <= '9') {
            if (!anyNumber) {
                value = 0;
                anyNumber = true;
            }
            value = value * 10 + (c - '0');
            continue;
        }
        if (!anyNumber) { parseOk = false; break; }
        char op = 0;
        if (c == 0x89u) op = '+';
        else if (c == 0x99u) op = '-';
        else if (c == 0xA9u) op = '*';
        else if (c == 0xB9u) op = '/';
        if (op == 0) { parseOk = false; break; }
        if (pendingOp != 0) {
            switch (pendingOp) {
            case '+': value = pending + value; break;
            case '-': value = pending - value; break;
            case '*': value = pending * value; break;
            case '/': if (value == 0) { parseOk = false; } else { value = pending / value; } break;
            default: parseOk = false; break;
            }
            if (!parseOk) break;
        }
        pending = value;
        pendingOp = op;
        anyNumber = false;
    }
    long long result = 0;
    if (parseOk && anyNumber && pendingOp != 0) {
        switch (pendingOp) {
        case '+': result = pending + value; break;
        case '-': result = pending - value; break;
        case '*': result = pending * value; break;
        case '/': if (value != 0) result = pending / value; else parseOk = false; break;
        default: parseOk = false; break;
        }
    } else if (parseOk && anyNumber) {
        result = value;  // un solo numero (EXE sin operacion)
    } else {
        parseOk = false;
    }
    if (!parseOk || result < 0 || result > 99999999ll) {
        std::printf("[EXPHLE] fuera de alcance v1: '%s' ok=%d res=%lld\n",
                    expr, parseOk ? 1 : 0, result);
        std::fflush(stdout);
        return;
    }

    // Codifica el resultado en BCD CASIO:
    // [0x10][(E<<4)|d0][(d1<<4)|d2]... con E = k-1 y d0..dk = digitos.
    char digits[16]{};
    int k = 0;
    long long tmp = result;
    if (tmp == 0) digits[k++] = 0;
    while (tmp > 0) { digits[k++] = static_cast<char>(tmp % 10); tmp /= 10; }
    // digits quedo al reves; reconstruye en orden.
    char ordered[16]{};
    for (int i = 0; i < k; ++i) ordered[i] = digits[k - 1 - i];
    std::uint8_t out[12]{};
    int w = 0;
    out[w++] = 0x10u;
    out[w++] = static_cast<std::uint8_t>(((k - 1) << 4) | ordered[0]);
    for (int i = 1; i < k; i += 2) {
        std::uint8_t byte = static_cast<std::uint8_t>(ordered[i] << 4);
        if (i + 1 < k) byte |= static_cast<std::uint8_t>(ordered[i + 1]);
        out[w++] = byte;
    }
    for (int i = 0; i < w; ++i) {
        Write8(0x880D4034u + static_cast<std::uint32_t>(i), out[i]);
    }
    std::printf("[EXPHLE] '%s' = %lld -> campo %02X %02X %02X %02X inst=%llu\n",
        expr, result, out[0], out[1],
        w > 2 ? out[2] : 0, w > 3 ? out[3] : 0,
        static_cast<unsigned long long>(stats_.instructions));
    std::fflush(stdout);

    // Display-list: el resultado como texto ASCII + terminador (asi lo deja
    // el oficial: "100\0" en 0x08186700 para 100; un digito: "3\0").
    {
        char disp[16];
        int dw = 0;
        for (int i = k - 1; i >= 0; --i) {
            disp[dw++] = static_cast<char>('0' + ordered[i]);
        }
        disp[dw] = '\0';
        for (int i = 0; i <= dw; ++i) {
            Write8(0x08186700u + static_cast<std::uint32_t>(i),
                   static_cast<std::uint8_t>(disp[i]));
        }
    }
}

void Cg50NativeCore::UpdateKeyInputData() noexcept
{    const auto& kbd = peripheralBus_.Keyboard();
    // Modo directo del Key1616 (experimento CG50_KEY_DIRECT=1): el oficial
    // expone direct_code=0x00010703 durante la tecla (pressed=1, matriz=0x0703)
    // y KIUMODE=0x0200. En ese modo la app lee el codigo como datos KIU, no
    // como bits de la matriz. Se mantiene OFF por defecto.
    static const bool directKeyMode =
        std::getenv("CG50_KEY_DIRECT") != nullptr;
    if (directKeyMode && (keyInterfaceControl_[2] & 0x0200u) != 0u &&
        keyEventPending_ != 0u) {
        std::fill(keyInputData_.begin(), keyInputData_.end(), 0);
        keyInputData_[0] = keyEventPending_;
        keyInputData_[1] = keyState_[keyEventPending_] != 0 ? 1u : 0u;
        return;
    }
    for (std::size_t reg = 0; reg < kKeyInterfaceDataRegisterCount; ++reg) {
        const std::uint8_t koLow = static_cast<std::uint8_t>(reg * 2 + 1);
        const std::uint8_t koHigh = static_cast<std::uint8_t>(reg * 2 + 2);
        std::uint16_t val = 0;
        for (std::uint8_t ki = 0; ki < 7; ++ki) {
            const auto inputLine = static_cast<std::uint8_t>(ki + 1);
            if (kbd.IsPressed(inputLine, koLow)) {
                val |= static_cast<std::uint16_t>(1u << ki);
            }
            if (kbd.IsPressed(inputLine, koHigh)) {
                val |= static_cast<std::uint16_t>(1u << (ki + 8));
            }
        }
        keyInputData_[reg] = val;
    }
}

bool Cg50NativeCore::PostHostEvent(std::uint32_t event)
{
    for (std::uint32_t& pending : hostMailbox_) {
        if (pending == 0u) {
            pending = event;
            return true;
        }
    }
    return false;
}

// El oficial activa el bit 2 de PBDR solo durante el dispatch post-EXE
// (la ventana de evaluacion): el handler 801E1D3E retorna 0 y el
// bt 802C256C saca la cadena de pollers hacia el camino del resultado.
// Con el almacen estatico (bit2=0 siempre) el despacho post-EXE se caia a
// los pollers LCD/status y 1+2 evaluaba a 0 (INFORME-F1A-CAUSA 19.1-19.3).
// La ventana (14,10)->(14,11) es el intervalo exacto: fuera de ella el
// bit debe ser 0 para que los pollers normales (menu/teclas) corran.
bool Cg50NativeCore::HostHasPendingWork() const noexcept
{
    return evalInProgress_;
}

// El host real (GCalAppIF) publica un evento de UI despues de cada transicion
// de tecla. Sin el, el OS despierta por la IRQ del KIU y ejecuta su ISR, pero
// la tarea que consume la tecla nunca se despacha: la pantalla no cambia
// (N-1). Verificado con CG50_TRAP_F000C_AT=11@<inst>: tras sembrar el evento
// 11 la tecla '1' abre Run-Matrix. El evento se publica con latencia (el host
// real lo emite en su hilo de UI, no dentro del ISR): publicarlo en el mismo
// instante del KeyDown se consume antes de que el ISR encole la tecla.
// CG50_KEY_HOST_EVENT=0 desactiva; CG50_KEY_HOST_EVENT_DELAY=<n> ajusta la
// latencia en instrucciones (por defecto 200000).
static std::uint32_t KeyHostEventId()
{
    static const long id = [] {
        const char* v = std::getenv("CG50_KEY_HOST_EVENT");
        if (v == nullptr) return 11L;
        if (std::strcmp(v, "0") == 0) return 0L;
        return std::strtol(v, nullptr, 0);
    }();
    return id > 0 ? static_cast<std::uint32_t>(id) : 0u;
}

void Cg50NativeCore::ScheduleKeyHostEvent()
{
    const std::uint32_t id = KeyHostEventId();
    if (id == 0u) return;
    static const std::uint64_t delay = [] {
        const char* v = std::getenv("CG50_KEY_HOST_EVENT_DELAY");
        return v != nullptr ? std::strtoull(v, nullptr, 0) : 200000ull;
    }();
    static const std::uint64_t window = [] {
        const char* v = std::getenv("CG50_KEY_HOST_EVENT_WINDOW");
        return v != nullptr ? std::strtoull(v, nullptr, 0) : 40000000ull;
    }();
    scheduledHostEvent_ = id;
    scheduledHostEventAt_ = stats_.instructions + delay;
    keyHostEventUntil_ = stats_.instructions + window;
}

bool Cg50NativeCore::KeyDown(const std::uint16_t matrixCode)
{
    if (!peripheralBus_.SetKeyMatrixCode(matrixCode, true)) return false;
    keyState_[matrixCode] = 1;
    keyEventPending_ = matrixCode;
    // HLE experimental de la ruta Key1616 (CG50_APP_KEY_HLE=1): el oficial
    // deja el caracter ASCII en 0x881871E5 y el codigo de glifo en
    // 0x8804FB32 al teclear en Run-Matrix (diff pre/post del oraculo con la
    // tecla 0x0603: 0x32 y 0x12). Se usa solo para probar la entrada de apps.
    if (std::getenv("CG50_APP_KEY_HLE") != nullptr) {
        std::uint8_t ascii = 0, glyph = 0;
        switch (matrixCode) {
        case 0x0703: ascii = 0x31; glyph = 0x10; break; // '1'
        case 0x0603: ascii = 0x32; glyph = 0x12; break; // '2'
        case 0x0503: ascii = 0x33; glyph = 0x13; break; // '3'
        case 0x0403: ascii = 0x2B; glyph = 0x11; break; // '+'
        default: break;
        }
        if (ascii != 0) {
            Write8(0x881871E5u, ascii);
            Write8(0x8804FB32u, glyph);
        }
    }
    // HLE experimental (CG50_WORK_HLE=1): tras teclear el ultimo operando ('2')
    // escribe la lista de glyph-runs + preview observada en el oficial
    // pre-EXE (WORK 0x88184DC1/0x88184DD8) para comprobar si el evaluador la
    // consume y produce 1+2=3.
    if (std::getenv("CG50_WORK_HLE") != nullptr && matrixCode == 0x0603u) {
        static const std::uint8_t tokens[28] = {
            0x02,0x10,0x55,0x55, 0x01,0xFF,0x55,0x55,
            0x02,0x11,0x55,0x55, 0x02,0x12,0x55,0x55,
            0x02,0x13,0x55,0x55, 0x01,0xFF,0x55,0x55,
            0x02,0x00,0x55,0x55};
        Write8(0x88184DC1u, 1);
        for (std::size_t i = 0; i < sizeof(tokens); ++i) {
            Write8(0x88184DD8u + static_cast<std::uint32_t>(i), tokens[i]);
        }
    }
    UpdateKeyInputData();
    keyInterfaceControl_[4] |= 0x000Au;
    ScheduleKeyHostEvent();
    // Priority comes from the OS-configured IPRF field (empirically 13 at
    // menu idle, IPRF=0xD000); a zero field masks the scan interrupt exactly
    // like the real INTC would.
    const std::uint32_t prio = ResolveInterruptPriority(kEventKeyScan);
    const bool irq = RaiseMaskableInterrupt(kEventKeyScan, prio);
    if (!irq && prio != 0) {
        // Real hardware keeps the KIU UNF request line asserted until the
        // INTC accepts it; re-deliver at the next clean boundary instead of
        // dropping the keystroke (observed: F1 sent while the OS was running
        // hit a delay slot and was lost).
        pendingKeyScanIrq_ = true;
    }
    if (PeriphDiagEnabled()) {
        std::printf("[KEYDOWN] code=0x%04X prio=%u irq=%d sr=0x%08X inst=%llu\n",
            matrixCode, prio, irq ? 1 : 0, regs_.sr,
            (unsigned long long)stats_.instructions);
        std::fflush(stdout);
    }
    if (DiagFileEnabled()) {
        if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
            std::fprintf(kd, "[KEYDOWN] code=0x%04X prio=%u irq=%d sr=0x%08X inst=%llu\n",
                matrixCode, prio, irq ? 1 : 0, regs_.sr,
                (unsigned long long)stats_.instructions);
            std::fclose(kd);
        }
    }
    return state_ != State::Empty;
}

void Cg50NativeCore::KeyUp(const std::uint16_t matrixCode)
{
    peripheralBus_.SetKeyMatrixCode(matrixCode, false);
    keyState_[matrixCode] = 0;
    keyEventPending_ = matrixCode;
    UpdateKeyInputData();
    keyInterfaceControl_[4] |= 0x000Au;
    ScheduleKeyHostEvent();
    const std::uint32_t prio = ResolveInterruptPriority(kEventKeyScan);
    if (prio != 0 && !RaiseMaskableInterrupt(kEventKeyScan, prio)) {
        pendingKeyScanIrq_ = true;
    }
}

bool Cg50NativeCore::IsKeyDown(const std::uint16_t matrixCode) const noexcept
{
    return keyState_[matrixCode] != 0;
}

// ============================================================================
// SH-4A FPU (1111 xxxx xxxx xxxx)
// ============================================================================

std::uint32_t& Cg50NativeCore::FrRef(const std::uint32_t index) noexcept
{
    return ((regs_.fpscr & kFpscrFr) ? regs_.xf : regs_.fr)[index];
}

const std::uint32_t& Cg50NativeCore::FrRef(const std::uint32_t index) const noexcept
{
    return ((regs_.fpscr & kFpscrFr) ? regs_.xf : regs_.fr)[index];
}

std::uint32_t& Cg50NativeCore::XfRef(const std::uint32_t index) noexcept
{
    return ((regs_.fpscr & kFpscrFr) ? regs_.fr : regs_.xf)[index];
}

const std::uint32_t& Cg50NativeCore::XfRef(const std::uint32_t index) const noexcept
{
    return ((regs_.fpscr & kFpscrFr) ? regs_.fr : regs_.xf)[index];
}

bool Cg50NativeCore::ExecuteFpuOpcode(const std::uint32_t pc, const std::uint16_t op)
{
    // SR.FD gates the entire FPU register file: any F-space opcode with FD=1
    // raises the FPU-disabled exception instead of executing.
    if ((regs_.sr & kSrFd) != 0) {
        std::printf("[FPU-FD] pc=0x%08X op=0x%04X sr=0x%08X fpscr=0x%08X inst=%llu\n",
            pc, op, regs_.sr, regs_.fpscr,
            (unsigned long long)stats_.instructions);
        (void)EnterGeneralException(pc, cg50::kExpevtFpuDisabled, false, 0);
        return true;
    }

    const std::uint32_t n = RegN(op);
    const std::uint32_t m = RegM(op);
    const std::uint32_t mode = op & 0x000Fu;
    const bool doubleMode = (regs_.fpscr & kFpscrPr) != 0;
    const bool pairMode = (regs_.fpscr & kFpscrSz) != 0;
    const std::uint32_t rm = regs_.fpscr & kFpscrRoundingMask;

    auto applyFpuEvents = [&](const cg50::FpuEventReport& report) {
        std::uint32_t cause = 0;
        if (report.inexactOrOverflow) cause |= 0x2000u;   // I/O bit 13
        if (report.underflowOrDivZero) cause |= 0x4000u;  // U/Z bit 14
        if (report.invalid) cause |= 0x8000u;             // V bit 15
        regs_.fpscr = (regs_.fpscr & ~kFpscrCauseMask) | cause;
        if (cause != 0) {
            regs_.fpscr |= (cause << 3) & kFpscrFlagMask;
            const std::uint32_t enabled =
                ((regs_.fpscr << 3) & kFpscrCauseMask) & cause;
            if (enabled != 0) {
                (void)EnterGeneralException(pc, cg50::kExpevtFpuException,
                                             false, 0);
            }
        }
    };

    const bool flushDenorm = (regs_.fpscr & kFpscrDn) != 0;
    auto flushF = [&](const float v) -> float {
        if (flushDenorm && std::fpclassify(v) == FP_SUBNORMAL) {
            return std::signbit(v) ? -0.0f : 0.0f;
        }
        return v;
    };
    auto flushD = [&](const double v) -> double {
        if (flushDenorm && std::fpclassify(v) == FP_SUBNORMAL) {
            return std::signbit(v) ? -0.0 : 0.0;
        }
        return v;
    };
    auto loadF = [&](const std::uint32_t i) {
        return flushF(cg50::BitsToFloat(FrRef(i)));
    };
    auto storeF = [&](const std::uint32_t i, const float v) {
        FrRef(i) = cg50::FloatToBits(flushF(v));
    };
    auto loadD = [&](const std::uint32_t i) {
        return flushD(cg50::BitsToDouble(FrRef(i), FrRef(i + 1u)));
    };
    auto storeD = [&](const std::uint32_t i, const double v) {
        std::uint32_t hi = 0, lo = 0;
        cg50::DoubleToBits(flushD(v), hi, lo);
        FrRef(i) = hi;
        FrRef(i + 1u) = lo;
    };
    auto roundStoreSingle = [&](const std::uint32_t i, const double v) {
        cg50::FpuEventReport rep;
        const double rounded = cg50::FpuRoundToPrecision(v, false, rm, rep);
        storeF(i, static_cast<float>(rounded));
        applyFpuEvents(rep);
    };
    auto roundStoreDouble = [&](const std::uint32_t i, const double v) {
        cg50::FpuEventReport rep;
        const double rounded = cg50::FpuRoundToPrecision(v, true, rm, rep);
        storeD(i, rounded);
        applyFpuEvents(rep);
    };

    // FMOV.S / FMOV data access with exact SH-4A 32-bit word semantics,
    // 4-byte alignment checks, user-mode segment protection and UTLB translation.
    const auto fpuAccessFault = [&](const std::uint32_t eventCode,
                                    const std::uint32_t vectorOffset) {
        (void)EnterGeneralException(pc, eventCode, false, 0, vectorOffset);
    };
    const auto fpuRead32 = [&](const std::uint32_t address,
                               std::uint32_t& value) -> bool {
        if (cg50::DirectSegmentAccessViolatesUserMode(regs_.sr, kSrMd, address)) {
            fpuAccessFault(cg50::AddressErrorEvent(false), 0x100u);
            return false;
        }
        if ((address & 3u) != 0u) {
            fpuAccessFault(cg50::kExpevtAddressErrorRead, 0x100u);
            return false;
        }
        std::uint32_t physical = address;
        if ((mmuControl_ & kMmuControlAtBit) != 0) {
            std::uint32_t code = 0;
            std::uint32_t vectorOffset = 0;
            if (!TranslateAddress(address, false, false, physical, code,
                                  &vectorOffset)) {
                fpuAccessFault(code, vectorOffset);
                return false;
            }
        }
        return Read32(physical, value);
    };
    const auto fpuWrite32 = [&](const std::uint32_t address,
                                const std::uint32_t value) -> bool {
        if (cg50::DirectSegmentAccessViolatesUserMode(regs_.sr, kSrMd, address)) {
            fpuAccessFault(cg50::AddressErrorEvent(true), 0x100u);
            return false;
        }
        if ((address & 3u) != 0u) {
            fpuAccessFault(cg50::kExpevtAddressErrorWrite, 0x100u);
            return false;
        }
        std::uint32_t physical = address;
        if ((mmuControl_ & kMmuControlAtBit) != 0) {
            std::uint32_t code = 0;
            std::uint32_t vectorOffset = 0;
            if (!TranslateAddress(address, true, false, physical, code,
                                  &vectorOffset)) {
                fpuAccessFault(code, vectorOffset);
                return false;
            }
        }
        return Write32(physical, value);
    };

    // SH-4A pair-mode register selector (SZ=1): even register -> DR (active bank),
    // odd register -> XD (inactive XF bank).
    auto fpuPairRef = [&](const std::uint32_t reg, const std::uint32_t wordIdx) -> std::uint32_t& {
        const std::uint32_t base = reg & 0xEu;
        return (reg & 1u) ? XfRef(base + wordIdx) : FrRef(base + wordIdx);
    };

    const auto requireEvenPair = [&](const std::uint32_t reg,
                                     const char* what) -> bool {
        if ((reg & 1u) != 0u) {
            std::wstringstream ss;
            ss << L"CG50 ROM-native: " << what
               << L" con registro impar (no definido en SH-4A) reg="
               << std::dec << reg;
            diagnostic_ = ss.str();
            HaltUnsupported(pc, op);
            return false;
        }
        return true;
    };

    switch (mode) {
    case 0x0u:  // FADD
    case 0x1u:  // FSUB
    case 0x2u:  // FMUL
    case 0x3u:  // FDIV
    case 0x4u:  // FCMP/EQ
    case 0x5u: { // FCMP/GT
        if (doubleMode) {
            if (!requireEvenPair(n, "FADD/FSUB/FMUL/FDIV/FCMP (PR=1)") ||
                !requireEvenPair(m, "FADD/FSUB/FMUL/FDIV/FCMP (PR=1)")) {
                return false;
            }
            const double lhs = loadD(n);
            const double rhs = loadD(m);
            switch (mode) {
            case 0x4u:
                SetT(lhs == rhs);
                stats_.cycles += 2;
                return true;
            case 0x5u:
                if (std::isnan(lhs) || std::isnan(rhs)) {
                    SetT(false);
                    cg50::FpuEventReport rep;
                    rep.invalid = true;
                    applyFpuEvents(rep);
                } else {
                    SetT(lhs > rhs);
                }
                stats_.cycles += 2;
                return true;
            default: break;
            }
            cg50::FpuEventReport rep;
            double result = 0.0;
            bool divZero = false;
            switch (mode) {
            case 0x0u: result = lhs + rhs; break;
            case 0x1u: result = lhs - rhs; break;
            case 0x2u: result = lhs * rhs; break;
            default:
                if (rhs == 0.0) {
                    divZero = true;
                    if (lhs == 0.0 || std::isnan(lhs)) rep.invalid = true;
                    else rep.underflowOrDivZero = true;
                }
                result = lhs / rhs;
                break;
            }
            const double rounded =
                cg50::FpuRoundToPrecision(result, true, rm, rep);
            if (divZero && !rep.invalid) rep.underflowOrDivZero = true;
            storeD(n, rounded);
            applyFpuEvents(rep);
        } else {
            const float lhs = loadF(n);
            const float rhs = loadF(m);
            switch (mode) {
            case 0x4u:
                SetT(lhs == rhs);
                stats_.cycles += 2;
                return true;
            case 0x5u:
                if (std::isnan(lhs) || std::isnan(rhs)) {
                    SetT(false);
                    cg50::FpuEventReport rep;
                    rep.invalid = true;
                    applyFpuEvents(rep);
                } else {
                    SetT(lhs > rhs);
                }
                stats_.cycles += 2;
                return true;
            default: break;
            }
            cg50::FpuEventReport rep;
            double result = 0.0;
            bool divZero = false;
            switch (mode) {
            case 0x0u: result = static_cast<double>(lhs) + rhs; break;
            case 0x1u: result = static_cast<double>(lhs) - rhs; break;
            case 0x2u: result = static_cast<double>(lhs) * rhs; break;
            default:
                if (rhs == 0.0f) {
                    divZero = true;
                    if (lhs == 0.0f || std::isnan(lhs)) rep.invalid = true;
                    else rep.underflowOrDivZero = true;
                }
                result = static_cast<double>(lhs) / rhs;
                break;
            }
            const double rounded =
                cg50::FpuRoundToPrecision(result, false, rm, rep);
            if (divZero && !rep.invalid) rep.underflowOrDivZero = true;
            storeF(n, static_cast<float>(rounded));
            applyFpuEvents(rep);
        }
        stats_.cycles += 2;
        return true;
    }
    case 0x6u:   // FMOV.S @Rm,FRn  | FMOV @Rm,DRn/XDn (SZ=1)
    case 0x7u:   // FMOV.S @Rm+,FRn | FMOV @Rm+,DRn/XDn (SZ=1)
    case 0x8u:   // FMOV.S FRm,@Rn  | FMOV DRm/XDm,@Rn (SZ=1)
    case 0x9u:   // FMOV.S FRm,@-Rn | FMOV DRm/XDm,@-Rn (SZ=1)
    case 0xAu:   // FMOV.S @(R0,Rm),FRn  | FMOV @(R0,Rm),DRn/XDn (SZ=1)
    case 0xBu: { // FMOV.S FRm,@(R0,Rn)  | FMOV DRm/XDm,@(R0,Rn) (SZ=1)
        const std::uint32_t words = pairMode ? 2u : 1u;
        std::uint32_t address = 0;
        bool postIncrement = false;
        bool preDecrement = false;
        std::uint32_t baseReg = 0;
        switch (mode) {
        case 0x6u: address = regs_.r[m]; break;
        case 0x7u: address = regs_.r[m]; postIncrement = true; baseReg = m; break;
        case 0x8u: address = regs_.r[n]; break;
        case 0x9u: address = regs_.r[n] - 4u * words; preDecrement = true; baseReg = n; break;
        case 0xAu: address = regs_.r[0] + regs_.r[m]; break;
        default:   address = regs_.r[0] + regs_.r[n]; break;
        }
        if (mode < 0x8u) {
            if (pairMode) {
                std::uint32_t w0 = 0, w1 = 0;
                if (!fpuRead32(address, w0)) return true;
                if (!fpuRead32(address + 4u, w1)) return true;
                fpuPairRef(n, 0u) = w0;
                fpuPairRef(n, 1u) = w1;
            } else {
                std::uint32_t w0 = 0;
                if (!fpuRead32(address, w0)) return true;
                FrRef(n) = w0;
            }
            if (postIncrement) regs_.r[baseReg] += 4u * words;
        } else {
            if (pairMode) {
                const std::uint32_t w0 = fpuPairRef(m, 0u);
                const std::uint32_t w1 = fpuPairRef(m, 1u);
                if (!fpuWrite32(address, w0)) return true;
                if (!fpuWrite32(address + 4u, w1)) return true;
            } else {
                if (!fpuWrite32(address, FrRef(m))) return true;
            }
            if (preDecrement) regs_.r[baseReg] = address;
        }
        stats_.cycles += 1;
        return true;
    }
    case 0xCu: { // FMOV FRm,FRn (SZ=0) | FMOV DRm/XDm,DRn/XDn (SZ=1)
        if (pairMode) {
            const std::uint32_t w0 = fpuPairRef(m, 0u);
            const std::uint32_t w1 = fpuPairRef(m, 1u);
            fpuPairRef(n, 0u) = w0;
            fpuPairRef(n, 1u) = w1;
        } else {
            FrRef(n) = FrRef(m);
        }
        stats_.cycles += 1;
        return true;
    }
    case 0xDu: { // SH-4A 0xFnmD group keyed by m (exact Renesas SH-4A ISA)
        switch (m) {
        case 0x0u: // FSTS FPUL, FRn
            FrRef(n) = regs_.fpul;
            break;
        case 0x1u: // FLDS FRn, FPUL
            regs_.fpul = FrRef(n);
            break;
        case 0x2u: // FLOAT FPUL, FRn (PR=0) / DRn (PR=1)
            if (doubleMode) {
                if (!requireEvenPair(n, "FLOAT (PR=1)")) return false;
                storeD(n, static_cast<double>(static_cast<std::int32_t>(regs_.fpul)));
            } else {
                roundStoreSingle(n, static_cast<double>(static_cast<std::int32_t>(regs_.fpul)));
            }
            break;
        case 0x3u: { // FTRC FRn/DRn, FPUL (trunc toward zero, saturated)
            if (doubleMode && !requireEvenPair(n, "FTRC (PR=1)")) return false;
            const double value = doubleMode ? loadD(n)
                                            : static_cast<double>(loadF(n));
            cg50::FpuEventReport rep;
            if (std::isnan(value)) {
                regs_.fpul = 0x80000000u;
                rep.invalid = true;
            } else if (value >= 2147483647.0) {
                regs_.fpul = 0x7FFFFFFFu;
                rep.invalid = (value >= 2147483648.0);
                rep.inexactOrOverflow = !rep.invalid && (value != 2147483647.0);
            } else if (value <= -2147483648.0) {
                regs_.fpul = 0x80000000u;
                rep.invalid = (value < -2147483648.0);
            } else {
                const double truncated = std::trunc(value);
                regs_.fpul = static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(truncated));
                rep.inexactOrOverflow = (truncated != value);
            }
            applyFpuEvents(rep);
            break;
        }
        case 0x4u: // FNEG (sign-bit flip, valid for NaN/Inf)
            if (doubleMode && !requireEvenPair(n, "FNEG (PR=1)")) return false;
            FrRef(n) ^= 0x80000000u;
            break;
        case 0x5u: // FABS (clear sign bit)
            if (doubleMode && !requireEvenPair(n, "FABS (PR=1)")) return false;
            FrRef(n) &= ~0x80000000u;
            break;
        case 0x6u: // FSQRT
            if (doubleMode) {
                if (!requireEvenPair(n, "FSQRT (PR=1)")) return false;
                const double v = loadD(n);
                cg50::FpuEventReport rep;
                if (v < 0.0) rep.invalid = true;
                const double r = cg50::FpuRoundToPrecision(std::sqrt(v), true, rm, rep);
                storeD(n, r);
                applyFpuEvents(rep);
            } else {
                const float v = loadF(n);
                cg50::FpuEventReport rep;
                if (v < 0.0f) rep.invalid = true;
                const double r = cg50::FpuRoundToPrecision(std::sqrt(static_cast<double>(v)), false, rm, rep);
                storeF(n, static_cast<float>(r));
                applyFpuEvents(rep);
            }
            break;
        case 0x7u: // FSRRA FRn (1 / sqrt(FRn), single precision)
            roundStoreSingle(n, 1.0 / std::sqrt(static_cast<double>(loadF(n))));
            break;
        case 0x8u: // FLDI0 FRn (0xFn8D)
            FrRef(n) = 0x00000000u;
            break;
        case 0x9u: // FLDI1 FRn (0xFn9D)
            FrRef(n) = 0x3F800000u;
            break;
        case 0xAu: // FCNVSD FPUL, DRn (0xFnAD: single FPUL -> double DRn)
            if (!requireEvenPair(n, "FCNVSD")) return false;
            storeD(n, static_cast<double>(cg50::BitsToFloat(regs_.fpul)));
            break;
        case 0xBu: { // FCNVDS DRm, FPUL (0xFmBD: double DRm -> single FPUL)
            if (!requireEvenPair(n, "FCNVDS")) return false;
            cg50::FpuEventReport rep;
            const double rounded =
                cg50::FpuRoundToPrecision(loadD(n), false, rm, rep);
            regs_.fpul = cg50::FloatToBits(static_cast<float>(rounded));
            applyFpuEvents(rep);
            break;
        }
        case 0xEu: { // FIPR FVm, FVn (0xFnED: vn = n & 0xC, vm = (n & 3) << 2)
            const std::uint32_t vn = n & 0xCu;
            const std::uint32_t vm = (n & 0x3u) << 2;
            double acc = 0.0;
            for (std::uint32_t i = 0; i < 4u; ++i) {
                acc += static_cast<double>(loadF(vm + i)) * loadF(vn + i);
            }
            roundStoreSingle(vn + 3u, acc);
            break;
        }
        case 0xFu: { // 0xFnFD: FSCHG / FPCHG / FRCHG / FTRV / FSCA
            if (n == 0x3u) {        // FSCHG (0xF3FD)
                regs_.fpscr ^= kFpscrSz;
            } else if (n == 0x7u) { // FPCHG (0xF7FD)
                regs_.fpscr ^= kFpscrPr;
            } else if (n == 0xBu) { // FRCHG (0xFBFD)
                regs_.fpscr ^= kFpscrFr;
            } else if ((n & 0x3u) == 0x1u) { // FTRV XMTRX, FVn (1111 nn01 1111 1101)
                const std::uint32_t vn = n & 0xCu;
                double src[4] = {
                    static_cast<double>(loadF(vn + 0u)),
                    static_cast<double>(loadF(vn + 1u)),
                    static_cast<double>(loadF(vn + 2u)),
                    static_cast<double>(loadF(vn + 3u)),
                };
                double dst[4] = {0.0, 0.0, 0.0, 0.0};
                // SH-4A XMTRX is column-major: column i = {XF[i*4+0..3]}.
                for (std::uint32_t j = 0; j < 4u; ++j) {
                    double acc = 0.0;
                    for (std::uint32_t i = 0; i < 4u; ++i) {
                        acc += static_cast<double>(cg50::BitsToFloat(XfRef(i * 4u + j))) * src[i];
                    }
                    dst[j] = acc;
                }
                for (std::uint32_t j = 0; j < 4u; ++j) {
                    roundStoreSingle(vn + j, dst[j]);
                }
            } else if ((n & 0x1u) == 0x0u) { // FSCA FPUL, DRn (1111 nnn0 1111 1101)
                constexpr double kTwoPiOver65536 = 6.283185307179586476925286766559 / 65536.0;
                const double theta =
                    static_cast<double>(static_cast<std::int16_t>(regs_.fpul & 0xFFFFu)) *
                    kTwoPiOver65536;
                roundStoreSingle(n, std::sin(theta));
                roundStoreSingle(n + 1u, std::cos(theta));
            } else {
                HaltUnsupported(pc, op);
                return false;
            }
            break;
        }
        default:
            HaltUnsupported(pc, op);
            return false;
        }
        stats_.cycles += 1;
        return true;
    }
    case 0xEu: { // FMAC FR0, FRm, FRn (0xFnmE: unconditional for all n, m)
        const double result =
            static_cast<double>(loadF(0u)) * loadF(m) + loadF(n);
        roundStoreSingle(n, result);
        stats_.cycles += 1;
        return true;
    }
    default: // 0xFnmF is unassigned in SH-4A
        HaltUnsupported(pc, op);
        return false;
    }
}

// ============================================================================
// INTC priority resolution / timer + watchdog delivery
// ============================================================================

bool Cg50NativeCore::IsInterruptMaskedByImr(const std::uint32_t eventCode) const noexcept
{
    switch (eventCode) {
    case kEventDmacDei0: return (interruptMask_[1] & 0x01u) != 0u;
    case kEventDmacDei1: return (interruptMask_[1] & 0x02u) != 0u;
    case kEventDmacDei2: return (interruptMask_[1] & 0x04u) != 0u;
    case kEventDmacDei3: return (interruptMask_[1] & 0x08u) != 0u;
    case kEventDmacDei4: return (interruptMask_[5] & 0x10u) != 0u;
    case kEventDmacDei5: return (interruptMask_[5] & 0x20u) != 0u;
    case kEventEtmu0:    return (interruptMask_[6] & 0x08u) != 0u;
    case kEventEtmu1:    return (interruptMask_[6] & 0x10u) != 0u;
    case kEventEtmu2:    return (interruptMask_[6] & 0x20u) != 0u;
    case kEventEtmu3:    return (interruptMask_[6] & 0x40u) != 0u;
    case kEventEtmu4:    return (interruptMask_[6] & 0x80u) != 0u;
    case kEventEtmu5:    return (interruptMask_[10] & 0x01u) != 0u;
    case kEventCmt:      return (interruptMask_[4] & 0x10u) != 0u;
    case kEventScif0:    return (interruptMask_[5] & 0x01u) != 0u;
    case kEventUsb:      return (interruptMask_[9] & 0x02u) != 0u;
    case kEventAdc:      return (interruptMask_[8] & 0x10u) != 0u;
    case kEventTpu:      return (interruptMask_[8] & 0x20u) != 0u;
    default:             return false;
    }
}

std::uint8_t Cg50NativeCore::ResolveInterruptPriority(
    const std::uint32_t eventCode) const noexcept
{
    const std::uint16_t ipra = interruptPriority_[0];
    const std::uint16_t iprb = interruptPriority_[1];
    const std::uint16_t ipre = interruptPriority_[4];
    const std::uint16_t iprf = interruptPriority_[5];
    const std::uint16_t iprg = interruptPriority_[6];
    const std::uint16_t iprh = interruptPriority_[7];
    const std::uint16_t ipri = interruptPriority_[8];
    const std::uint16_t iprj = interruptPriority_[9];
    const std::uint16_t iprl = interruptPriority_[11];
    switch (eventCode) {
    case kEventTmu0:
        return static_cast<std::uint8_t>((ipra >> 12) & 0xFu);
    case kEventTmu1:
        return static_cast<std::uint8_t>((ipra >> 8) & 0xFu);
    case kEventTmu2:
        return static_cast<std::uint8_t>((ipra >> 4) & 0xFu);
    case kEventWatchdog:
    case kEventRtcCarry:
    case kEventRtcPeriodic:
        return static_cast<std::uint8_t>((iprb >> 12) & 0xFu);
    case kEventKeyScan:
        return static_cast<std::uint8_t>((iprf >> 12) & 0xFu);
    case kEventDmacDei0:
    case kEventDmacDei1:
    case kEventDmacDei2:
    case kEventDmacDei3:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((ipre >> 12) & 0xFu);
    case kEventDmacDei4:
    case kEventDmacDei5:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprf >> 8) & 0xFu);
    case kEventEtmu0:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprj >> 8) & 0xFu);
    case kEventEtmu1:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprl >> 12) & 0xFu);
    case kEventEtmu2:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprl >> 8) & 0xFu);
    case kEventEtmu3:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((ipri >> 12) & 0xFu);
    case kEventEtmu4:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprl >> 4) & 0xFu);
    case kEventEtmu5:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((ipri >> 8) & 0xFu);
    case kEventCmt:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>(iprf & 0xFu);
    case kEventScif0:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprg >> 12) & 0xFu);
    case kEventUsb:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprf >> 4) & 0xFu);
    case kEventAdc:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprh >> 8) & 0xFu);
    case kEventTpu:
        return IsInterruptMaskedByImr(eventCode) ? 0u : static_cast<std::uint8_t>((iprh >> 4) & 0xFu);
    default:
        return 0u;
    }
}

void Cg50NativeCore::TickWatchdog(const std::uint64_t cycles)
{
    // SH7305 WDT: WTCSR bit 5 = TME (timer enable), bit 6 = WT/IT (1 =
    // interval mode), bits 2-0 = CKS prescaler. The OS leaves WTCSR=0x04 at
    // menu idle (TME=0), so this is inert during the validated boot; only
    // an OS/application enabling interval mode produces the 0x0560 event.
    // Watchdog-mode resets are deliberately NOT emulated (fail-closed
    // instead of silently rebooting the guest).
    if (cycles == 0 || (watchdogControl_ & 0x20u) == 0) return;
    static constexpr std::uint32_t kWatchdogDivisors[8] = {
        1u, 4u, 16u, 32u, 64u, 256u, 1024u, 4096u
    };
    const auto divisor = kWatchdogDivisors[watchdogControl_ & 0x7u];
    watchdogPrescaler_ += cycles;
    while (watchdogPrescaler_ >= divisor) {
        watchdogPrescaler_ -= divisor;
        ++watchdogCounter_;
        if (watchdogCounter_ == 0) {
            // Interval-mode underflow: request the WDT interrupt.
            watchdogCounter_ = 0;  // free-run reload from 0
            const std::uint8_t priority =
                ResolveInterruptPriority(kEventWatchdog);
            if (priority != 0 && (watchdogControl_ & 0x40u) != 0) {
                (void)RaiseMaskableInterrupt(kEventWatchdog, priority);
            }
        }
    }
}

void Cg50NativeCore::DeliverPendingInterrupts()
{
    // Opt-in SH7305 RTC carry IRQ delivery: default OFF so OS boot stays
    // deterministic and unaffected by RCR1 startup writes; when explicitly
    // armed via SetRtcIrqEnabled(true), delivers kEventRtcCarry (0x4C0)
    // according to INTC priority.
    if (rtcCarryFlag_) {
        if (rtcIrqEnabled_) {
            const std::uint8_t rtcPrio = ResolveInterruptPriority(kEventRtcCarry);
            if (rtcPrio != 0 && RaiseMaskableInterrupt(kEventRtcCarry, rtcPrio)) {
                rtcCarryFlag_ = false;
            }
        } else {
            rtcCarryFlag_ = false;
        }
    }

    if (pendingKeyScanIrq_) {
        const std::uint32_t keyPrio = ResolveInterruptPriority(kEventKeyScan);
        if (keyPrio != 0 && RaiseMaskableInterrupt(kEventKeyScan, keyPrio)) {
            pendingKeyScanIrq_ = false;
        } else if (PeriphDiagEnabled() && (stats_.instructions & 0xFFFFFu) == 0u) {
            std::printf("[KIUPEND] inst=%llu sr=0x%08X imask=%u prio=%u state=%d branch=%d slot=%d\n",
                (unsigned long long)stats_.instructions, regs_.sr,
                static_cast<unsigned>(regs_.sr & 0xFu), keyPrio,
                static_cast<int>(state_), pendingBranchValid_ ? 1 : 0,
                executingDelaySlot_ ? 1 : 0);
            if (DiagFileEnabled()) {
                if (FILE* kd = Cg50DiagFopen("key_diag.log", "a")) {
                    std::fprintf(kd, "[KIUPEND] inst=%llu sr=0x%08X imask=%u prio=%u state=%d branch=%d slot=%d\n",
                        (unsigned long long)stats_.instructions, regs_.sr,
                        static_cast<unsigned>(regs_.sr & 0xFu), keyPrio,
                        static_cast<int>(state_), pendingBranchValid_ ? 1 : 0,
                        executingDelaySlot_ ? 1 : 0);
                    std::fclose(kd);
                }
            }
        }
    }

    if (pendingDmacIrq_ != 0u) {
        static constexpr std::array<std::uint32_t, 6> kDmacEvents = {
            kEventDmacDei0, kEventDmacDei1, kEventDmacDei2,
            kEventDmacDei3, kEventDmacDei4, kEventDmacDei5
        };
        for (std::size_t ch = 0; ch < 6u; ++ch) {
            const std::uint32_t bit = 1u << ch;
            if ((pendingDmacIrq_ & bit) == 0u) continue;
            const std::uint8_t prio = ResolveInterruptPriority(kDmacEvents[ch]);
            if (prio == 0u) {
                pendingDmacIrq_ &= ~bit;
                continue;
            }
            if (RaiseMaskableInterrupt(kDmacEvents[ch], prio)) {
                pendingDmacIrq_ &= ~bit;
                return;
            }
        }
    }

    if (pendingEtmuIrq_ != 0u) {
        for (std::size_t ch = 0; ch < cg50::EtmuUnit::kChannelCount; ++ch) {
            const std::uint32_t bit = 1u << ch;
            if ((pendingEtmuIrq_ & bit) == 0u) continue;
            const std::uint32_t ev = cg50::EtmuUnit::EventCode(ch);
            const std::uint8_t prio = ResolveInterruptPriority(ev);
            if (prio == 0u) {
                pendingEtmuIrq_ &= ~bit;
                continue;
            }
            if (RaiseMaskableInterrupt(ev, prio)) {
                pendingEtmuIrq_ &= ~bit;
                return;
            }
        }
    }

    if (pendingTimerIrq_ == 0) return;

    while (pendingTimerIrq_ != 0) {
        const std::uint32_t channelMask = pendingTimerIrq_ & (pendingTimerIrq_ - 1u);
        const std::uint32_t channelBit = pendingTimerIrq_ ^ channelMask;
        pendingTimerIrq_ = channelMask;
        const std::uint32_t channel =
            (channelBit & 0x1u) != 0 ? 0u :
            (channelBit & 0x2u) != 0 ? 1u : 2u;
        const std::uint32_t eventCode = kEventTmu0 + channel * 0x20u;
        const std::uint8_t priority = ResolveInterruptPriority(eventCode);
        if (priority == 0) continue;  // masked at the IPR: drop (documented)
        if (!RaiseMaskableInterrupt(eventCode, priority)) {
            // Deferred (delay slot in progress): the UNF request line stays
            // asserted on real hardware until accepted, so re-queue and
            // retry at the next instruction boundary instead of dropping.
            pendingTimerIrq_ |= channelBit;
            break;
        }
    }
}

// ============================================================================
// SH-4A UTLB translation (only active with MMUCR.AT = 1)
// ============================================================================

bool Cg50NativeCore::TranslateAddress(const std::uint32_t address,
                                      const bool write,
                                      const bool isFetch,
                                      std::uint32_t& physical,
                                      std::uint32_t& exceptionCode,
                                      std::uint32_t* vectorOffset)
{
    // SH-4A address-space translation table:
    //   P0/U0 0x00000000-0x7FFFFFFF -> UTLB
    //   P1    0x80000000-0x9FFFFFFF -> direct
    //   P2    0xA0000000-0xBFFFFFFF -> direct
    //   P3    0xC0000000-0xDFFFFFFF -> UTLB
    //   P4    0xE0000000-0xFFFFFFFF -> direct
    const bool direct =
        (address >= 0x80000000u && address < 0xC0000000u) ||
        address >= 0xE0000000u;
    if (direct) {
        physical = address;
        return true;
    }

    // 2026-10-29 M4 (mmu.go:159-171): poblacion centralizada de los registros
    // de estado en TODOS los fallos de esta funcion (antes no se usaban y los
    // 4 call sites quedaban con registros sucios viejos): TEA = VA,
    // PTEH.VPN = pagina del VA con PTEH.ASID intacto. EXPEVT/SSR/SPC/SGR los
    // fija EnterGeneralException a partir del eventCode (por eso no se tocan
    // aqui) y PTEA no se toca (la referencia tampoco).
    const auto RaiseFault = [&](const std::uint32_t event,
                                const std::uint32_t vector) -> void {
        tea_ = address;
        pteh_ = (address & 0xFFFFFC00u) | (pteh_ & 0xFFu);
        exceptionCode = event;
        if (vectorOffset != nullptr) *vectorOffset = vector;
    };

    // 2026-10-29 M3 (mmu.go:114-119): encaja SH, o SV con MD=1, o ASID
    // identico. Privilegio/SV/ASID se computan UNA vez fuera del loop.
    const bool privileged = (regs_.sr & kSrMd) != 0;
    const bool svSingle = privileged && (mmuControl_ & kMmuControlSvBit) != 0;
    const std::uint32_t asid = pteh_ & 0xFFu;

    static constexpr std::uint32_t kPageSizeMasks[4] = {
        ~((1u << 10) - 1u),   // 1 KB
        ~((1u << 12) - 1u),   // 4 KB
        ~((1u << 16) - 1u),   // 64 KB
        ~((1u << 20) - 1u),   // 1 MB
    };
    UtlbEntry* matchedEntry = nullptr;
    std::uint32_t matchedMask = 0;
    int matchCount = 0;
    for (auto& entry : utlb_) {
        if (!entry.valid) continue;
        const auto mask = kPageSizeMasks[entry.pageSizeCode & 0x3u];
        if (((address ^ (entry.virtualPageNumber << 10)) & mask) != 0u) continue;
        if (!entry.shared && !svSingle && entry.asid != asid) continue;  // M3
        ++matchCount;
        if (matchCount > 1) {
            RaiseFault(cg50::kExpevtTlbMultiHit, cg50::kGeneralExceptionVectorOffset);
            return false;
        }
        matchedEntry = &entry;
        matchedMask = mask;
    }
    if (matchedEntry != nullptr) {
        auto& entry = *matchedEntry;
        const auto mask = matchedMask;

        // Protection: PR 0 = no access, 1 = privileged read, 2 = privileged
        // R/W + user read, 3 = full access.
        if (entry.protection == 0u ||
            (!privileged && (entry.protection < 2u ||
                             (write && entry.protection < 3u)))) {
            RaiseFault(isFetch
                           ? cg50::kExpevtItlbProtectionViolation
                           : cg50::kExpevtDataTlbProtectionViolation,
                       cg50::kTlbMissVectorOffset);
            return false;
        }
        physical = ((entry.physicalPageNumber << 10) & mask) | (address & ~mask);
        if (write && !entry.dirty) {
            // 2026-10-29 M5 (mmu.go:151-153): initial-page-write SH-4A.
            // EXPEWT 0x080 con vector 0x100; la escritura NO se ejecuta. El
            // 0x080 va LITERAL: sh_exception.h ya usa 0x080 como
            // kExpevtDataTlbMissRead y NO se reasignan constantes (el OS
            // agrupa 0x040/0x060/0x080 en un solo handler TLB).
            // CG50_UTLB_AUTODIRTY=1 restaura el auto-dirty legado.
            if (kUtlbAutoDirty) {
                entry.dirty = true;
                return true;
            }
            RaiseFault(0x080u, cg50::kGeneralExceptionVectorOffset);
            return false;
        }
        return true;
    }

    // TLB miss.
    RaiseFault(isFetch
                   ? cg50::kExpevtItlbMiss
                   : (write ? cg50::kExpevtDataTlbMissWrite
                            : cg50::kExpevtDataTlbMissRead),
               cg50::kTlbMissVectorOffset);
    return false;
}

// ============================================================================
// 2026-10-29 M9: arrays MMIO de la UTLB (calco de mmu.go:216-258)
// ============================================================================

Cg50NativeCore::UtlbEntry Cg50NativeCore::DecodeUtlbData(
    const std::uint32_t dataWord)
{
    // Decode del data word PTEL (mmu.go:42-53): PPN 28:10, V bit8 (0x100),
    // SZ1 bit7, PR bits6:5, SZ0 bit4, C bit3, D bit2 (0x4), SH bit1 (0x2),
    // WT bit0. C/WT no se modelan (semantica de cache fuera de alcance,
    // declarado): se decodifican ausentes y leen 0.
    UtlbEntry entry{};
    entry.physicalPageNumber = (dataWord >> 10) & 0x001FFFFFu;
    entry.pageSizeCode =
        (((dataWord >> 7) & 0x1u) << 1) | ((dataWord >> 4) & 0x1u);
    entry.protection = (dataWord >> 5) & 0x3u;
    entry.valid = (dataWord & 0x100u) != 0;
    entry.dirty = (dataWord & 0x4u) != 0;
    entry.shared = (dataWord & 0x2u) != 0;
    return entry;
}

std::uint32_t Cg50NativeCore::UtlbArrayRead(const std::uint32_t address) const
{
    const std::size_t index =
        static_cast<std::size_t>((address >> 8) & 0x3Fu);
    const UtlbEntry& entry = utlb_[index];
    if (address < kUtlbDataArrayBase) {
        // Address array: VPN | D<<9 | V<<8 | ASID (mmu.go:52-54).
        return (entry.virtualPageNumber << 10) |
               (entry.dirty ? 0x200u : 0u) |
               (entry.valid ? 0x100u : 0u) |
               (entry.asid & 0xFFu);
    }
    // Data array 1 (>= 0xF7800000 es PTEA): readback del data word.
    std::uint32_t word = entry.physicalPageNumber << 10;
    if (entry.valid) word |= 0x100u;
    if ((entry.pageSizeCode & 0x2u) != 0u) word |= 0x80u;
    word |= (entry.protection & 0x3u) << 5;
    if ((entry.pageSizeCode & 0x1u) != 0u) word |= 0x10u;
    if (entry.dirty) word |= 0x4u;
    if (entry.shared) word |= 0x2u;
    return word;
}

void Cg50NativeCore::UtlbArrayWrite(const std::uint32_t address,
                                    const std::uint32_t value)
{
    if (address >= kUtlbDataArrayEnd) {
        // Data array 2 = PTEA: aceptar y descartar (sin backing, declarado).
        return;
    }
    if (address >= kUtlbDataArrayBase) {
        // Data array 1 (mmu.go:234): reemplazar ONLY el data word de la
        // entrada idx=(va>>8)&0x3F; vpn/asid de la entrada no se tocan.
        const std::size_t index =
            static_cast<std::size_t>((address >> 8) & 0x3Fu);
        UtlbEntry entry = DecodeUtlbData(value);
        entry.virtualPageNumber = utlb_[index].virtualPageNumber;
        entry.asid = utlb_[index].asid;
        utlb_[index] = entry;
        return;
    }
    if ((address & 0x80u) == 0u) {
        // Address array no-asociativa (mmu.go:243-247): programar VPN/D/V/
        // ASID de la entrada idx. setVD PISA V y D con los del valor
        // (vacio el bit -> entrada invalidada, igual que la referencia).
        const std::size_t index =
            static_cast<std::size_t>((address >> 8) & 0x3Fu);
        UtlbEntry& entry = utlb_[index];
        entry.virtualPageNumber = (value >> 10) & 0x001FFFFFu;
        entry.asid = value & 0xFFu;
        entry.valid = (value & 0x100u) != 0;
        entry.dirty = (value & 0x200u) != 0;
        return;
    }
    // Address array asociativa (mmu.go:249-257): V/D de TODA entrada cuyo
    // VPN encaje a granularidad de pagina y cuyo ASID coincida o sea
    // shared/SV (MMUCR bit8, SIN comprobacion de MD). Compare de clave a
    // granularidad de pagina con la polaridad de este core.
    const bool svSingle = (mmuControl_ & kMmuControlSvBit) != 0;
    const bool setV = (value & 0x100u) != 0;
    const bool setD = (value & 0x200u) != 0;
    const std::uint32_t writeVa = (value >> 10) << 10;  // va bits 30:10<<10
    static constexpr std::uint32_t kPageOffsetMask[4] = {
        (1u << 10) - 1u, (1u << 12) - 1u, (1u << 16) - 1u, (1u << 20) - 1u,
    };
    for (auto& entry : utlb_) {
        const auto offsetMask =
            kPageOffsetMask[entry.pageSizeCode & 0x3u];
        const std::uint32_t entryVa = entry.virtualPageNumber << 10;
        if (((writeVa ^ entryVa) & ~offsetMask) != 0) continue;
        if (!(entry.shared || svSingle || entry.asid == (value & 0xFFu)))
            continue;
        entry.valid = setV;
        entry.dirty = setD;
    }
}

std::uint32_t Cg50NativeCore::ItlbArrayRead(const std::uint32_t address) const
{
    const std::size_t index =
        static_cast<std::size_t>((address >> 8) & 0x3u);
    const UtlbEntry& entry = itlb_[index];
    if (address < kItlbDataArrayBase) {
        return (entry.virtualPageNumber << 10) |
               (entry.valid ? 0x100u : 0u) |
               (entry.asid & 0xFFu);
    }
    std::uint32_t word = entry.physicalPageNumber << 10;
    if (entry.valid) word |= 0x100u;
    if ((entry.pageSizeCode & 0x2u) != 0u) word |= 0x80u;
    word |= (entry.protection & 0x3u) << 5;
    if ((entry.pageSizeCode & 0x1u) != 0u) word |= 0x10u;
    if (entry.shared) word |= 0x2u;
    return word;
}

void Cg50NativeCore::ItlbArrayWrite(const std::uint32_t address,
                                    const std::uint32_t value)
{
    if (address >= kItlbDataArrayEnd) return;
    const std::size_t index =
        static_cast<std::size_t>((address >> 8) & 0x3u);
    if (address >= kItlbDataArrayBase) {
        UtlbEntry entry = DecodeUtlbData(value);
        entry.virtualPageNumber = itlb_[index].virtualPageNumber;
        entry.asid = itlb_[index].asid;
        itlb_[index] = entry;
        return;
    }
    UtlbEntry& entry = itlb_[index];
    entry.virtualPageNumber = (value >> 10) & 0x001FFFFFu;
    entry.asid = value & 0xFFu;
    entry.valid = (value & 0x100u) != 0;
}

// ============================================================================
// RTC SH7305 (0xA413FEC0, BCD, reloj virtual determinista)
// ============================================================================

namespace {

std::uint32_t BcdToBinary(const std::uint8_t bcd)
{
    return ((bcd >> 4) & 0xFu) * 10u + (bcd & 0xFu);
}

std::uint8_t BinaryToBcd(const std::uint32_t value)
{
    if (value > 99u) return 0x99u;
    return static_cast<std::uint8_t>(((value / 10u) << 4) | (value % 10u));
}

// Howard Hinnant's public-domain civil-date algorithms, shifted to count
// days from 2000-01-01 (a Saturday).
std::int64_t DaysFromCivil2000(const std::int64_t year, const std::uint32_t month,
                               const std::uint32_t day)
{
    const std::int64_t y = year - (month <= 2 ? 1 : 0);
    const std::int64_t era = (y >= 0 ? y : y - 399) / 400;
    const std::int64_t yoe = y - era * 400;
    const std::int64_t doy = (153 * (static_cast<std::int64_t>(month) +
                                    (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    const std::int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468 - 10957;
}

void CivilFromDays2000(const std::int64_t daysSince2000, std::int64_t& year,
                       std::uint32_t& month, std::uint32_t& day)
{
    const std::int64_t z = daysSince2000 + 719468 + 10957;
    const std::int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const std::int64_t doe = z - era * 146097;
    const std::int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const std::int64_t y = yoe + era * 400;
    const std::int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const std::int64_t mp = (5 * doy + 2) / 153;
    day = static_cast<std::uint32_t>(doy - (153 * mp + 2) / 5 + 1);
    month = static_cast<std::uint32_t>(mp + (mp < 10 ? 3 : -9));
    year = y + (month <= 2 ? 1 : 0);
}

} // namespace

std::uint64_t Cg50NativeCore::RtcVirtualSeconds() const noexcept
{
    const std::uint64_t instructions = stats_.instructions;
    std::uint64_t elapsed = 0;
    if (instructions >= rtcAnchorInstruction_) {
        elapsed = (instructions - rtcAnchorInstruction_) / kRtcInstructionsPerSecond;
    }
    return rtcBaseSeconds_ + elapsed;
}

std::uint8_t Cg50NativeCore::RtcDerivedComponent8(const std::uint32_t offset) const
{
    const std::uint64_t total = RtcVirtualSeconds();
    const std::uint64_t days = total / 86400u;
    const std::uint32_t secondsOfDay = static_cast<std::uint32_t>(total % 86400u);
    switch (offset) {
    case 0x02u: return BinaryToBcd((secondsOfDay % 60u));
    case 0x04u: return BinaryToBcd(((secondsOfDay / 60u) % 60u));
    case 0x06u: return BinaryToBcd((secondsOfDay / 3600u) % 24u);
    case 0x08u: {
        // 2000-01-01 was a Saturday; weekday 0 = Sunday.
        const std::uint32_t weekday = static_cast<std::uint32_t>((days + 6) % 7u);
        return BinaryToBcd(weekday);
    }
    case 0x0Au: {
        std::int64_t year = 0;
        std::uint32_t month = 0, day = 0;
        CivilFromDays2000(static_cast<std::int64_t>(days), year, month, day);
        return BinaryToBcd(day);
    }
    case 0x0Cu: {
        std::int64_t year = 0;
        std::uint32_t month = 0, day = 0;
        CivilFromDays2000(static_cast<std::int64_t>(days), year, month, day);
        return BinaryToBcd(month);
    }
    default: return 0u;
    }
}

std::uint16_t Cg50NativeCore::RtcDerivedYearBcd() const
{
    const std::uint64_t total = RtcVirtualSeconds();
    const std::int64_t days = static_cast<std::int64_t>(total / 86400u);
    std::int64_t year = 0;
    std::uint32_t month = 0, day = 0;
    CivilFromDays2000(days, year, month, day);
    const std::uint32_t yearsSince2000 =
        year >= 2000 && year <= 2099
            ? static_cast<std::uint32_t>(year - 2000)
            : 0u;
    return static_cast<std::uint16_t>(BinaryToBcd(yearsSince2000));
}

void Cg50NativeCore::RtcReanchorFromComponent(const std::uint32_t offset,
                                              const std::uint8_t value)
{
    if (offset == 0x08u) return;  // weekday is derived, writes are absorbed

    const std::uint64_t total = RtcVirtualSeconds();
    const std::int64_t days = static_cast<std::int64_t>(total / 86400u);
    const std::uint32_t secondsOfDay = static_cast<std::uint32_t>(total % 86400u);
    std::int64_t year = 0;
    std::uint32_t month = 0, day = 0;
    CivilFromDays2000(days, year, month, day);

    const std::uint32_t written = BcdToBinary(value);
    switch (offset) {
    case 0x02u:
        if (written <= 59u) {
            const std::uint32_t secs = (secondsOfDay / 60u) * 60u + written;
            rtcBaseSeconds_ = static_cast<std::uint64_t>(days) * 86400u + secs;
        }
        break;
    case 0x04u:
        if (written <= 59u) {
            const std::uint32_t secs = (secondsOfDay / 3600u) * 3600u +
                                       written * 60u + secondsOfDay % 60u;
            rtcBaseSeconds_ = static_cast<std::uint64_t>(days) * 86400u + secs;
        }
        break;
    case 0x06u:
        if (written <= 23u) {
            const std::uint32_t secs = written * 3600u + secondsOfDay % 3600u;
            rtcBaseSeconds_ = static_cast<std::uint64_t>(days) * 86400u + secs;
        }
        break;
    case 0x0Au:
        if (written >= 1u && written <= 31u) day = written;
        break;
    case 0x0Cu:
        if (written >= 1u && written <= 12u) month = written;
        break;
    case 0x0Eu:
        if (written <= 99u) year = 2000 + static_cast<std::int64_t>(written);
        break;
    default:
        return;
    }
    if (offset == 0x0Au || offset == 0x0Cu || offset == 0x0Eu) {
        rtcBaseSeconds_ = static_cast<std::uint64_t>(
            DaysFromCivil2000(year, month, day) * 86400 + secondsOfDay);
    }
    rtcAnchorInstruction_ = stats_.instructions;
}

void Cg50NativeCore::TickRtc()
{
    // A virtual second boundary is crossed exactly when the instruction
    // counter is a multiple of the per-second instruction budget (called
    // once per retired instruction, so at most one boundary per call).
    if (stats_.instructions != 0 &&
        stats_.instructions % kRtcInstructionsPerSecond == 0) {
        rtcCarryFlag_ = true;
    }
}

// ============================================================================
// Save-state completo de la mÃƒÆ’Ã‚Â¡quina
// ============================================================================

namespace {

constexpr char kStateMagic[9] = {'C', 'G', '5', '0', 'S', 'T', 'A', 'T', 'E'};
constexpr std::uint32_t kStateVersion = 2u;

void StateWriteChunk(std::vector<std::uint8_t>& out, const std::uint32_t id,
                      const void* data, const std::size_t size)
{
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    out.insert(out.end(), reinterpret_cast<const std::uint8_t*>(&id),
               reinterpret_cast<const std::uint8_t*>(&id) + sizeof(id));
    const auto size64 = static_cast<std::uint64_t>(size);
    out.insert(out.end(), reinterpret_cast<const std::uint8_t*>(&size64),
               reinterpret_cast<const std::uint8_t*>(&size64) + sizeof(size64));
    out.insert(out.end(), bytes, bytes + size);
}

struct StateReader {
    const std::uint8_t* cursor = nullptr;
    std::size_t remaining = 0;
    bool failed = false;

    bool ReadChunk(const std::uint32_t expectedId, void* destination,
                   const std::size_t destinationSize)
    {
        if (failed || remaining < sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
            failed = true;
            return false;
        }
        std::uint32_t id = 0;
        std::memcpy(&id, cursor, sizeof(id));
        cursor += sizeof(id);
        std::uint64_t size = 0;
        std::memcpy(&size, cursor, sizeof(size));
        cursor += sizeof(size);
        remaining -= sizeof(id) + sizeof(size);
        if (id != expectedId || size != destinationSize || size > remaining) {
            failed = true;
            return false;
        }
        if (destination != nullptr) std::memcpy(destination, cursor, size);
        cursor += size;
        remaining -= static_cast<std::size_t>(size);
        return true;
    }

    bool ReadVector(const std::uint32_t expectedId, std::vector<std::uint8_t>& out,
                    const std::size_t maxAllowed)
    {
        if (failed || remaining < sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
            failed = true;
            return false;
        }
        std::uint32_t id = 0;
        std::memcpy(&id, cursor, sizeof(id));
        cursor += sizeof(id);
        std::uint64_t size = 0;
        std::memcpy(&size, cursor, sizeof(size));
        cursor += sizeof(size);
        remaining -= sizeof(id) + sizeof(size);
        if (id != expectedId || size > maxAllowed || size > remaining) {
            failed = true;
            return false;
        }
        out.assign(cursor, cursor + size);
        cursor += size;
        remaining -= static_cast<std::size_t>(size);
        return true;
    }
};

} // namespace

bool Cg50NativeCore::ExportState(std::vector<std::uint8_t>& bytes) const
{
    bytes.clear();
    bytes.insert(bytes.end(), kStateMagic, kStateMagic + sizeof(kStateMagic));
    StateWriteChunk(bytes, 1u, &kStateVersion, sizeof(kStateVersion));
    StateWriteChunk(bytes, 2u, rom_.data(), rom_.size());
    StateWriteChunk(bytes, 3u, userRom_.data(), userRom_.size());
    StateWriteChunk(bytes, 4u, ram_.data(), ram_.size());
    StateWriteChunk(bytes, 5u, onChipRam_.data(), onChipRam_.size());
    StateWriteChunk(bytes, 6u, dsp0Space_.data(), dsp0Space_.size());
    StateWriteChunk(bytes, 7u, dsp1Space_.data(), dsp1Space_.size());
    StateWriteChunk(bytes, 8u, ilcPage_.data(), ilcPage_.size());
    StateWriteChunk(bytes, 9u, dmacPage_.data(), dmacPage_.size());
    StateWriteChunk(bytes, 10u, dmarsPage_.data(), dmarsPage_.size());
    StateWriteChunk(bytes, 11u, ubcPage_.data(), ubcPage_.size());
    StateWriteChunk(bytes, 12u, priorityLockPage_.data(), priorityLockPage_.size());
    StateWriteChunk(bytes, 13u, area1PeripheralPages_.data(),
                    sizeof(area1PeripheralPages_));
    StateWriteChunk(bytes, 14u, keyState_.data(), sizeof(keyState_));
    StateWriteChunk(bytes, 15u, alternateBank_.data(), sizeof(alternateBank_));
    StateWriteChunk(bytes, 16u, &regs_, sizeof(regs_));
    StateWriteChunk(bytes, 17u, &stats_, sizeof(stats_));
    StateWriteChunk(bytes, 18u, &cacheControl_, sizeof(cacheControl_));
    StateWriteChunk(bytes, 19u, &mmuControl_, sizeof(mmuControl_));
    StateWriteChunk(bytes, 20u, &cpuopm_, sizeof(cpuopm_));
    StateWriteChunk(bytes, 21u, &expmask_, sizeof(expmask_));
    StateWriteChunk(bytes, 22u, interruptMask_.data(), sizeof(interruptMask_));
    StateWriteChunk(bytes, 23u, &interruptMask5_, sizeof(interruptMask5_));
    StateWriteChunk(bytes, 24u, interruptPriority_.data(),
                    sizeof(interruptPriority_));
    StateWriteChunk(bytes, 25u, keyInputData_.data(), sizeof(keyInputData_));
    StateWriteChunk(bytes, 26u, keyInterfaceControl_.data(),
                    sizeof(keyInterfaceControl_));
    StateWriteChunk(bytes, 27u, &moduleStopControl0_, sizeof(moduleStopControl0_));
    StateWriteChunk(bytes, 28u, &standbyControl_, sizeof(standbyControl_));
    StateWriteChunk(bytes, 29u, &moduleStopControl2_, sizeof(moduleStopControl2_));
    StateWriteChunk(bytes, 30u, &compareMatchTimerStart_,
                    sizeof(compareMatchTimerStart_));
    StateWriteChunk(bytes, 31u, &compareMatchTimerControl_,
                    sizeof(compareMatchTimerControl_));
    StateWriteChunk(bytes, 32u, &compareMatchTimerCounter_,
                    sizeof(compareMatchTimerCounter_));
    StateWriteChunk(bytes, 33u, &compareMatchTimerConstant_,
                    sizeof(compareMatchTimerConstant_));
    StateWriteChunk(bytes, 34u, &compareMatchTimerPrescaler_,
                    sizeof(compareMatchTimerPrescaler_));
    StateWriteChunk(bytes, 35u, &portMControl_, sizeof(portMControl_));
    StateWriteChunk(bytes, 36u, &portNControl_, sizeof(portNControl_));
    StateWriteChunk(bytes, 37u, &portPControl_, sizeof(portPControl_));
    StateWriteChunk(bytes, 38u, pfcStorage_.data(), sizeof(pfcStorage_));
    StateWriteChunk(bytes, 39u, portControl_.data(), sizeof(portControl_));
    StateWriteChunk(bytes, 40u, portData_.data(), sizeof(portData_));
    StateWriteChunk(bytes, 41u, portPullControl_.data(), sizeof(portPullControl_));
    StateWriteChunk(bytes, 42u, &portMData_, sizeof(portMData_));
    StateWriteChunk(bytes, 43u, &portNData_, sizeof(portNData_));
    StateWriteChunk(bytes, 44u, &portPPullControl_, sizeof(portPPullControl_));
    StateWriteChunk(bytes, 45u, driveControl_.data(), sizeof(driveControl_));
    StateWriteChunk(bytes, 46u, bscRegisters_.data(), sizeof(bscRegisters_));
    StateWriteChunk(bytes, 47u, &busCommonControl_, sizeof(busCommonControl_));
    StateWriteChunk(bytes, 48u, &frequencyControl_, sizeof(frequencyControl_));
    StateWriteChunk(bytes, 49u, &pllControl_, sizeof(pllControl_));
    StateWriteChunk(bytes, 50u, &fllFrequency_, sizeof(fllFrequency_));
    StateWriteChunk(bytes, 51u, &clockChangeStatus_, sizeof(clockChangeStatus_));
    StateWriteChunk(bytes, 52u, &fsiClockControl_, sizeof(fsiClockControl_));
    StateWriteChunk(bytes, 53u, &ddClockControl_, sizeof(ddClockControl_));
    StateWriteChunk(bytes, 54u, &usbClockControl_, sizeof(usbClockControl_));
    StateWriteChunk(bytes, 55u, &pll2Control_, sizeof(pll2Control_));
    StateWriteChunk(bytes, 56u, &spuClockControl_, sizeof(spuClockControl_));
    StateWriteChunk(bytes, 57u, &vpuClockControl_, sizeof(vpuClockControl_));
    StateWriteChunk(bytes, 58u, &spreadSpectrumControl_,
                    sizeof(spreadSpectrumControl_));
    StateWriteChunk(bytes, 59u, &watchdogCounter_, sizeof(watchdogCounter_));
    StateWriteChunk(bytes, 60u, &watchdogControl_, sizeof(watchdogControl_));
    StateWriteChunk(bytes, 61u, utlb_.data(), sizeof(utlb_));
    StateWriteChunk(bytes, 62u, &pteh_, sizeof(pteh_));
    StateWriteChunk(bytes, 63u, &ptel_, sizeof(ptel_));
    StateWriteChunk(bytes, 64u, &ptea_, sizeof(ptea_));
    StateWriteChunk(bytes, 65u, &ttb_, sizeof(ttb_));
    StateWriteChunk(bytes, 66u, &tea_, sizeof(tea_));
    StateWriteChunk(bytes, 67u, &rtcBaseSeconds_, sizeof(rtcBaseSeconds_));
    StateWriteChunk(bytes, 68u, &rtcAnchorInstruction_,
                    sizeof(rtcAnchorInstruction_));
    StateWriteChunk(bytes, 69u, &rtc64HzLastTick_, sizeof(rtc64HzLastTick_));
    StateWriteChunk(bytes, 70u, &rtcCarryFlag_, sizeof(rtcCarryFlag_));
    StateWriteChunk(bytes, 71u, &watchdogPrescaler_, sizeof(watchdogPrescaler_));
    StateWriteChunk(bytes, 72u, &pendingTimerIrq_, sizeof(pendingTimerIrq_));
    StateWriteChunk(bytes, 73u, &standbyMode_, sizeof(standbyMode_));
    StateWriteChunk(bytes, 74u, &pendingBranchValid_, sizeof(pendingBranchValid_));
    StateWriteChunk(bytes, 75u, &pendingBranchTarget_,
                    sizeof(pendingBranchTarget_));
    StateWriteChunk(bytes, 76u, &executingDelaySlot_, sizeof(executingDelaySlot_));
    StateWriteChunk(bytes, 77u, &state_, sizeof(state_));

    std::vector<std::uint8_t> bus;
    if (!peripheralBus_.ExportState(bus)) return false;
    StateWriteChunk(bytes, 78u, bus.data(), bus.size());
    // Banco C (0x8C000000) con respaldo propio desde 2026-10-26: los estados
    // previos no lo traen y ImportState lo deja en ceros.
    StateWriteChunk(bytes, 79u, ramC_.data(), ramC_.size());
    StateWriteChunk(bytes, 80u, &tbr_, sizeof(tbr_));
    StateWriteChunk(bytes, 81u, &rteDelaySlotPrivilegedFetch_,
                    sizeof(rteDelaySlotPrivilegedFetch_));
    // Optional trailing SH7305 full hardware state chunks (82..94)
    StateWriteChunk(bytes, 82u, ilram_.data(), ilram_.size());
    StateWriteChunk(bytes, 83u, xram_.data(), xram_.size());
    StateWriteChunk(bytes, 84u, yram_.data(), yram_.size());
    StateWriteChunk(bytes, 85u, storeQueues_.data(), storeQueues_.size());
    StateWriteChunk(bytes, 86u, &qacr0_, sizeof(qacr0_));
    StateWriteChunk(bytes, 87u, &qacr1_, sizeof(qacr1_));
    StateWriteChunk(bytes, 88u, itlb_.data(), sizeof(itlb_));
    StateWriteChunk(bytes, 89u, &intc2_, sizeof(intc2_));
    StateWriteChunk(bytes, 90u, &etmu_, sizeof(etmu_));
    StateWriteChunk(bytes, 91u, &adc_, sizeof(adc_));
    StateWriteChunk(bytes, 92u, &tpu_, sizeof(tpu_));
    StateWriteChunk(bytes, 93u, &scif0_, sizeof(scif0_));
    StateWriteChunk(bytes, 94u, &usb2_, sizeof(usb2_));
    StateWriteChunk(bytes, 95u, &lastLcdDmaSar_, sizeof(lastLcdDmaSar_));
    return true;
}

bool Cg50NativeCore::ImportState(const std::uint8_t* bytes, const std::size_t size)
{
    auto fail = [this]() {
        state_ = State::Halted;
        diagnostic_ = L"CG50 ROM-native: save-state incompatible o corrupto";
        return false;
    };

    if (bytes == nullptr || size < sizeof(kStateMagic) + 4u +
            sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
        return fail();
    }
    if (std::memcmp(bytes, kStateMagic, sizeof(kStateMagic)) != 0) {
        return fail();
    }

    StateReader reader{bytes + sizeof(kStateMagic),
                       size - sizeof(kStateMagic)};

    std::uint32_t version = 0;
    if (!reader.ReadChunk(1u, &version, sizeof(version)) || version != kStateVersion) {
        return fail();
    }

    if (!reader.ReadVector(2u, rom_, kRomSize) ||
        !reader.ReadVector(3u, userRom_, kUserRomSize) ||
        !reader.ReadVector(4u, ram_, kRamSize)) {
        return fail();
    }

    if (!reader.ReadChunk(5u, onChipRam_.data(), sizeof(onChipRam_)) ||
        !reader.ReadVector(6u, dsp0Space_, kDsp0SpaceSize) ||
        !reader.ReadVector(7u, dsp1Space_, kDsp1SpaceSize) ||
        !reader.ReadChunk(8u, ilcPage_.data(), sizeof(ilcPage_)) ||
        !reader.ReadChunk(9u, dmacPage_.data(), sizeof(dmacPage_)) ||
        !reader.ReadChunk(10u, dmarsPage_.data(), sizeof(dmarsPage_)) ||
        !reader.ReadChunk(11u, ubcPage_.data(), sizeof(ubcPage_)) ||
        !reader.ReadChunk(12u, priorityLockPage_.data(),
                          sizeof(priorityLockPage_)) ||
        !reader.ReadChunk(13u, area1PeripheralPages_.data(),
                          sizeof(area1PeripheralPages_))) {
        return fail();
    }
    InitArea1PeripheralPages();
    flashCfi_.Reset();
    cpuLockFlag_ = false;

    if (!reader.ReadChunk(14u, keyState_.data(), sizeof(keyState_)) ||
        !reader.ReadChunk(15u, alternateBank_.data(), sizeof(alternateBank_)) ||
        !reader.ReadChunk(16u, &regs_, sizeof(regs_)) ||
        !reader.ReadChunk(17u, &stats_, sizeof(stats_)) ||
        !reader.ReadChunk(18u, &cacheControl_, sizeof(cacheControl_)) ||
        !reader.ReadChunk(19u, &mmuControl_, sizeof(mmuControl_)) ||
        !reader.ReadChunk(20u, &cpuopm_, sizeof(cpuopm_)) ||
        !reader.ReadChunk(21u, &expmask_, sizeof(expmask_)) ||
        !reader.ReadChunk(22u, interruptMask_.data(), sizeof(interruptMask_)) ||
        !reader.ReadChunk(23u, &interruptMask5_, sizeof(interruptMask5_)) ||
        !reader.ReadChunk(24u, interruptPriority_.data(),
                         sizeof(interruptPriority_)) ||
        !reader.ReadChunk(25u, keyInputData_.data(), sizeof(keyInputData_)) ||
        !reader.ReadChunk(26u, keyInterfaceControl_.data(),
                          sizeof(keyInterfaceControl_)) ||
        !reader.ReadChunk(27u, &moduleStopControl0_,
                          sizeof(moduleStopControl0_)) ||
        !reader.ReadChunk(28u, &standbyControl_, sizeof(standbyControl_)) ||
        !reader.ReadChunk(29u, &moduleStopControl2_,
                          sizeof(moduleStopControl2_)) ||
        !reader.ReadChunk(30u, &compareMatchTimerStart_,
                          sizeof(compareMatchTimerStart_)) ||
        !reader.ReadChunk(31u, &compareMatchTimerControl_,
                          sizeof(compareMatchTimerControl_)) ||
        !reader.ReadChunk(32u, &compareMatchTimerCounter_,
                          sizeof(compareMatchTimerCounter_)) ||
        !reader.ReadChunk(33u, &compareMatchTimerConstant_,
                          sizeof(compareMatchTimerConstant_)) ||
        !reader.ReadChunk(34u, &compareMatchTimerPrescaler_,
                          sizeof(compareMatchTimerPrescaler_)) ||
        !reader.ReadChunk(35u, &portMControl_, sizeof(portMControl_)) ||
        !reader.ReadChunk(36u, &portNControl_, sizeof(portNControl_)) ||
        !reader.ReadChunk(37u, &portPControl_, sizeof(portPControl_)) ||
        !reader.ReadChunk(38u, pfcStorage_.data(), sizeof(pfcStorage_)) ||
        !reader.ReadChunk(39u, portControl_.data(), sizeof(portControl_)) ||
        !reader.ReadChunk(40u, portData_.data(), sizeof(portData_)) ||
        !reader.ReadChunk(41u, portPullControl_.data(),
                          sizeof(portPullControl_)) ||
        !reader.ReadChunk(42u, &portMData_, sizeof(portMData_)) ||
        !reader.ReadChunk(43u, &portNData_, sizeof(portNData_)) ||
        !reader.ReadChunk(44u, &portPPullControl_, sizeof(portPPullControl_)) ||
        !reader.ReadChunk(45u, driveControl_.data(), sizeof(driveControl_)) ||
        !reader.ReadChunk(46u, bscRegisters_.data(), sizeof(bscRegisters_)) ||
        !reader.ReadChunk(47u, &busCommonControl_, sizeof(busCommonControl_)) ||
        !reader.ReadChunk(48u, &frequencyControl_, sizeof(frequencyControl_)) ||
        !reader.ReadChunk(49u, &pllControl_, sizeof(pllControl_)) ||
        !reader.ReadChunk(50u, &fllFrequency_, sizeof(fllFrequency_)) ||
        !reader.ReadChunk(51u, &clockChangeStatus_,
                          sizeof(clockChangeStatus_)) ||
        !reader.ReadChunk(52u, &fsiClockControl_, sizeof(fsiClockControl_)) ||
        !reader.ReadChunk(53u, &ddClockControl_, sizeof(ddClockControl_)) ||
        !reader.ReadChunk(54u, &usbClockControl_, sizeof(usbClockControl_)) ||
        !reader.ReadChunk(55u, &pll2Control_, sizeof(pll2Control_)) ||
        !reader.ReadChunk(56u, &spuClockControl_, sizeof(spuClockControl_)) ||
        !reader.ReadChunk(57u, &vpuClockControl_, sizeof(vpuClockControl_)) ||
        !reader.ReadChunk(58u, &spreadSpectrumControl_,
                          sizeof(spreadSpectrumControl_)) ||
        !reader.ReadChunk(59u, &watchdogCounter_, sizeof(watchdogCounter_)) ||
        !reader.ReadChunk(60u, &watchdogControl_, sizeof(watchdogControl_)) ||
        !reader.ReadChunk(61u, utlb_.data(), sizeof(utlb_)) ||
        !reader.ReadChunk(62u, &pteh_, sizeof(pteh_)) ||
        !reader.ReadChunk(63u, &ptel_, sizeof(ptel_)) ||
        !reader.ReadChunk(64u, &ptea_, sizeof(ptea_)) ||
        !reader.ReadChunk(65u, &ttb_, sizeof(ttb_)) ||
        !reader.ReadChunk(66u, &tea_, sizeof(tea_)) ||
        !reader.ReadChunk(67u, &rtcBaseSeconds_, sizeof(rtcBaseSeconds_)) ||
        !reader.ReadChunk(68u, &rtcAnchorInstruction_,
                          sizeof(rtcAnchorInstruction_)) ||
        !reader.ReadChunk(69u, &rtc64HzLastTick_, sizeof(rtc64HzLastTick_)) ||
        !reader.ReadChunk(70u, &rtcCarryFlag_, sizeof(rtcCarryFlag_)) ||
        !reader.ReadChunk(71u, &watchdogPrescaler_, sizeof(watchdogPrescaler_)) ||
        !reader.ReadChunk(72u, &pendingTimerIrq_, sizeof(pendingTimerIrq_)) ||
        !reader.ReadChunk(73u, &standbyMode_, sizeof(standbyMode_)) ||
        !reader.ReadChunk(74u, &pendingBranchValid_,
                          sizeof(pendingBranchValid_)) ||
        !reader.ReadChunk(75u, &pendingBranchTarget_,
                          sizeof(pendingBranchTarget_)) ||
        !reader.ReadChunk(76u, &executingDelaySlot_,
                          sizeof(executingDelaySlot_)) ||
        !reader.ReadChunk(77u, &state_, sizeof(state_))) {
        return fail();
    }

    std::vector<std::uint8_t> bus;
    if (!reader.ReadVector(78u, bus, 4u * 1024u * 1024u) ||
        !peripheralBus_.ImportState(bus.data(), bus.size())) {
        return fail();
    }

    // Banco C (0x8C000000) con respaldo propio: presente solo en estados
    // escritos tras el fix del 2026-10-26. Los estados previos acaban en el
    // chunk 78; el banco queda en ceros (epoca pre-EXE del oficial verificada
    // en ramc-res0ekey-preexe.bin, 100% ceros).
    tbr_ = 0;
    rteDelaySlotPrivilegedFetch_ = false;
    if (reader.remaining >= sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
        if (!reader.ReadVector(79u, ramC_, kRamCSize)) {
            return fail();
        }
    }
    if (reader.remaining >= sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
        if (!reader.ReadChunk(80u, &tbr_, sizeof(tbr_))) {
            return fail();
        }
    }
    if (reader.remaining >= sizeof(std::uint32_t) + sizeof(std::uint64_t)) {
        if (!reader.ReadChunk(81u, &rteDelaySlotPrivilegedFetch_,
                              sizeof(rteDelaySlotPrivilegedFetch_))) {
            return fail();
        }
    }
    ilram_.fill(0u);
    xram_.fill(0u);
    yram_.fill(0u);
    storeQueues_.fill(0u);
    qacr0_ = 0u;
    qacr1_ = 0u;
    itlb_.fill(UtlbEntry{});
    intc2_.Reset();
    etmu_.Reset();
    adc_.Reset();
    tpu_.Reset();
    scif0_.Reset();
    usb2_.Reset();
    pendingEtmuIrq_ = 0u;
    pendingDmacIrq_ = 0u;
    lastLcdDmaSar_ = 0x08000000u;
    auto readOptChunk = [&](const std::uint32_t id, void* dst, const std::size_t sz) {
        if (reader.remaining < sizeof(std::uint32_t) + sizeof(std::uint64_t)) return true;
        return reader.ReadChunk(id, dst, sz);
    };
    if (!readOptChunk(82u, ilram_.data(), ilram_.size()) ||
        !readOptChunk(83u, xram_.data(), xram_.size()) ||
        !readOptChunk(84u, yram_.data(), yram_.size()) ||
        !readOptChunk(85u, storeQueues_.data(), storeQueues_.size()) ||
        !readOptChunk(86u, &qacr0_, sizeof(qacr0_)) ||
        !readOptChunk(87u, &qacr1_, sizeof(qacr1_)) ||
        !readOptChunk(88u, itlb_.data(), sizeof(itlb_)) ||
        !readOptChunk(89u, &intc2_, sizeof(intc2_)) ||
        !readOptChunk(90u, &etmu_, sizeof(etmu_)) ||
        !readOptChunk(91u, &adc_, sizeof(adc_)) ||
        !readOptChunk(92u, &tpu_, sizeof(tpu_)) ||
        !readOptChunk(93u, &scif0_, sizeof(scif0_)) ||
        !readOptChunk(94u, &usb2_, sizeof(usb2_)) ||
        !readOptChunk(95u, &lastLcdDmaSar_, sizeof(lastLcdDmaSar_))) {
        return fail();
    }

    // Rebuild the keyboard matrix contacts from the serialized key state
    // (the peripheral-bus snapshot deliberately omits it) and refresh the
    // derived KIU data registers.
    for (std::size_t code = 0; code < keyState_.size(); ++code) {
        if (keyState_[code] != 0) {
            (void)peripheralBus_.SetKeyMatrixCode(
                static_cast<std::uint16_t>(code), true);
        }
    }
    UpdateKeyInputData();

    diagnostic_ = L"CG50 ROM-native: save-state restaurado; ejecuciÃƒÆ’Ã‚Â³n continÃƒÆ’Ã‚Âºa";
    return true;
}



