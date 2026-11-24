#include "cg50_native_core.h"

#include "cg50_core/sh_interrupt.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <utility>

namespace {

bool LooksLikeDlsimContainer(const std::uint8_t* bytes, const std::size_t size) noexcept
{
    if (bytes == nullptr || size < 8) return false;
    static constexpr char kUpper[] = "DLSimMEM";
    static constexpr char kMixed[] = "DLSimMem";
    return std::memcmp(bytes, kUpper, 8) == 0 ||
           std::memcmp(bytes, kMixed, 8) == 0;
}

} // namespace

bool Cg50NativeCore::LoadFirmwareParts(const std::uint8_t* systemRom,
                                       const std::size_t systemSize,
                                       const std::uint8_t* userRom,
                                       const std::size_t userSize)
{
    rom_.clear();
    userRom_.clear();
    std::fill(ram_.begin(), ram_.end(), std::uint8_t{0});
    std::fill(onChipRam_.begin(), onChipRam_.end(), std::uint8_t{0});
    std::fill(dsp0Space_.begin(), dsp0Space_.end(), std::uint8_t{0});
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
    regs_ = Registers{};
    tbr_ = 0;
    stats_ = Stats{};
    pendingBranchValid_ = false;
    pendingBranchTarget_ = 0;
    executingDelaySlot_ = false;
    cpuLockFlag_ = false;
    flashCfi_.Reset();
    state_ = State::Empty;

    if (systemRom == nullptr || systemSize == 0) {
        diagnostic_ = L"CG50 ROM-native: SystemROM ausente";
        return false;
    }
    if (LooksLikeDlsimContainer(systemRom, systemSize)) {
        diagnostic_ = L"CG50 ROM-native: SystemROM DLSimMEM/DLSimMem no es firmware RAW ejecutable";
        return false;
    }
    if (systemSize != kRomSize) {
        std::wstringstream ss;
        ss << L"CG50 ROM-native: SystemROM mide " << systemSize
           << L" bytes; se requieren exactamente 0x00B60000";
        diagnostic_ = ss.str();
        return false;
    }

    if ((userRom == nullptr) != (userSize == 0)) {
        diagnostic_ = L"CG50 ROM-native: UserROM debe ser nullptr,0 o un dump RAW completo";
        return false;
    }
    if (userRom != nullptr) {
        if (LooksLikeDlsimContainer(userRom, userSize)) {
            diagnostic_ = L"CG50 ROM-native: UserROM DLSimMEM/DLSimMem no es flash RAW";
            return false;
        }
        if (userSize != kUserRomSize) {
            std::wstringstream ss;
            ss << L"CG50 ROM-native: UserROM mide " << userSize
               << L" bytes; se requieren exactamente 0x014A0000";
            diagnostic_ = ss.str();
            return false;
        }
    }

    rom_.assign(systemRom, systemRom + systemSize);
    if (userRom != nullptr) userRom_.assign(userRom, userRom + userSize);

    // Factory auto-provisioning for uninitialized flash dumps:
    // 1. System parameter journal at 0x00B20000:
    if (rom_.size() >= kSystemParameterPhysicalBase + 64) {
        bool paramsErased = true;
        for (std::size_t i = 0; i < 64; ++i) {
            if (rom_[kSystemParameterPhysicalBase + i] != 0xFFu) {
                paramsErased = false;
                break;
            }
        }
        if (paramsErased) {
            // Commit active 64-byte parameter slot for fx-CG50 normal mode:
            // 0x0FFF (magic header), 0x9505 (normal mode; 0x95FF = Exam Mode active), 0x0003 (hw revision 3)
            rom_[kSystemParameterPhysicalBase + 0] = 0x0Fu;
            rom_[kSystemParameterPhysicalBase + 1] = 0xFFu;
            rom_[kSystemParameterPhysicalBase + 2] = 0x95u;
            rom_[kSystemParameterPhysicalBase + 3] = 0x05u;
            rom_[kSystemParameterPhysicalBase + 4] = 0x00u;
            rom_[kSystemParameterPhysicalBase + 5] = 0x03u;
        }
    }

    // 2. User storage filesystem superblock at 0x00120000:
    if (userRom_.size() >= 0x00120020u) {
        bool fsErased = true;
        for (std::size_t i = 0; i < 28; ++i) {
            if (userRom_[0x00120000u + i] != 0xFFu) {
                fsErased = false;
                break;
            }
        }
        if (fsErased) {
            // Commit default Casio MCS filesystem root block markers
            static constexpr std::uint8_t kDefaultFsSuperblock[28] = {
                0x00, 0x30, 0xFF, 0xD9, 0x00, 0x00, 0x00, 0x26,
                0xFF, 0xFF, 0xFF, 0xD9, 0x00, 0x00, 0x00, 0x26,
                0xFF, 0xFF, 0xFF, 0xD9, 0x00, 0x00, 0x00, 0x26,
                0xFF, 0xFF, 0xFF, 0xFF
            };
            std::memcpy(userRom_.data() + 0x00120000u, kDefaultFsSuperblock, sizeof(kDefaultFsSuperblock));
        }
    }

    Reset();
    diagnostic_ = userRom_.empty()
        ? L"CG50 ROM-native: SystemROM RAW cargada; SH7305 listo para bring-up"
        : L"CG50 ROM-native: SystemROM + UserROM RAW separadas cargadas; SH7305 listo para bring-up";
    return true;
}

void Cg50NativeCore::ResetCpuState()
{
    // Keep externally modelled state intact. Without board-level evidence, a
    // CPU reset must not silently erase external SDRAM, release physical keys,
    // or power-cycle the LCD/peripheral bus.
    auto savedRam = std::move(ram_);
    auto savedDsp0 = std::move(dsp0Space_);
    auto savedDsp1 = std::move(dsp1Space_);
    const auto savedArea1 = area1PeripheralPages_;
    const auto savedOnChipRam = onChipRam_;
    const auto savedKeys = keyState_;
    const auto savedPeripherals = peripheralBus_;

    Reset();

    ram_ = std::move(savedRam);
    dsp0Space_ = std::move(savedDsp0);
    dsp1Space_ = std::move(savedDsp1);
    area1PeripheralPages_ = savedArea1;
    InitArea1PeripheralPages();
    onChipRam_ = savedOnChipRam;
    keyState_ = savedKeys;
    peripheralBus_ = savedPeripherals;

    diagnostic_ = rom_.empty()
        ? L"CG50 ROM-native: reset CPU solicitado sin SystemROM"
        : L"CG50 ROM-native: reset arquitectónico SH7305; RAM interna/externa y placa conservadas";
}

bool Cg50NativeCore::RaiseMaskableInterrupt(const std::uint32_t eventCode,
                                            const std::uint8_t priority)
{
    const bool sleeping = state_ == State::Sleeping;
    if ((state_ != State::Running && !sleeping) || rom_.empty()) return false;

    if (pendingBranchValid_ || executingDelaySlot_) {
        diagnostic_ = L"CG50 ROM-native: interrupción aplazada; hay un delay slot SH-4A en curso";
        return false;
    }

    cg50::InterruptRequest request{};
    request.pc = regs_.pc;
    request.sr = regs_.sr;
    request.r15 = regs_.r[15];
    request.vbr = regs_.vbr;
    request.eventCode = eventCode;
    request.priority = priority;
    request.cpuSleeping = sleeping;

    const auto entry = cg50::BuildMaskableInterruptEntry(request);
    if (!entry.accepted) return false;

    regs_.ssr = entry.ssr;
    regs_.spc = entry.spc;
    regs_.sgr = entry.sgr;
    SetSr(entry.sr);
    regs_.pc = entry.targetPc;
    regs_.intevt = entry.intevt;

    pendingBranchValid_ = false;
    pendingBranchTarget_ = 0;
    executingDelaySlot_ = false;
    cpuLockFlag_ = false;
    state_ = State::Running;
    // Any accepted interrupt leaves SLEEP/STANDBY behind.
    standbyMode_ = false;

    ++stats_.interruptsAccepted;
    stats_.lastInterruptEvent = eventCode;
    stats_.lastInterruptPriority = priority;
#ifdef CG50_ENABLE_COVERAGE_COUNTERS
    if (cov_) {
        ++cov_->intevtHits[eventCode];
        cov_->RecordCpuState(static_cast<std::uint8_t>(state_));
        if (entry.wakesCpu) cov_->RecordBoundary("irq_wake_from_sleep");
    }
#endif

    std::wstringstream ss;
    ss << L"CG50 ROM-native: interrupción SH aceptada INTEVT=0x"
       << std::hex << std::uppercase << std::setw(8) << std::setfill(L'0') << eventCode
       << L" prioridad=" << std::dec << static_cast<unsigned>(priority)
       << (entry.wakesCpu ? L" (wake desde SLEEP)" : L"")
       << L" -> PC=0x" << std::hex << std::setw(8) << regs_.pc;
    diagnostic_ = ss.str();
    return true;
}
