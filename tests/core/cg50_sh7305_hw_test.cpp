#include "cg50_native_core.h"
#include "cg50_core/sh_exception.h"
#include "cg50_core/sh_fpu.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

int gFailures = 0;

void Check(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "[SH7305-HW-TEST] FAIL: %s\n", msg);
        ++gFailures;
    }
}

void PutBe16(std::vector<std::uint8_t>& buf, std::size_t off, std::uint16_t v)
{
    buf[off] = static_cast<std::uint8_t>(v >> 8);
    buf[off + 1] = static_cast<std::uint8_t>(v & 0xFFu);
}

void SetupMinimalCore(Cg50NativeCore& core, const std::vector<std::uint16_t>& code)
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    for (std::size_t i = 0; i < code.size(); ++i) {
        PutBe16(rom, i * 2u, code[i]);
    }
    Check(core.LoadFirmware(rom.data(), rom.size()), "LoadFirmware minimal");
    core.Reset();
}

void TestOnChipMemoriesAndIlramExec()
{
    // Execute 2 instructions directly from ILRAM (0xE5200000):
    //   0xE02A = mov #0x2A, r0
    //   0x0068 = nott (T = !T)
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {0x0009u}); // NOP in ROM

    // Write values into ILRAM, XRAM, YRAM (distinct from RS-RAM 0xFD800000)
    Check(core.Write32(0xE5200000u, 0xE02A0068u), "Write32 ILRAM");
    Check(core.Write32(0xE5007000u, 0x11223344u), "Write32 XRAM");
    Check(core.Write32(0xE5017000u, 0x55667788u), "Write32 YRAM");
    Check(core.Write32(0xFD800000u, 0xAABBCCDDu), "Write32 RS-RAM");

    std::uint32_t vIl = 0, vX = 0, vY = 0, vRs = 0;
    Check(core.Read32(0xE5200000u, vIl) && vIl == 0xE02A0068u, "Read32 ILRAM");
    Check(core.Read32(0x05200000u, vIl) && vIl == 0xE02A0068u, "Read32 ILRAM physical alias");
    Check(core.Read32(0xE5007000u, vX) && vX == 0x11223344u, "Read32 XRAM");
    Check(core.Read32(0xE5017000u, vY) && vY == 0x55667788u, "Read32 YRAM");
    Check(core.Read32(0xFD800000u, vRs) && vRs == 0xAABBCCDDu, "RS-RAM isolated from ILRAM");

    // Execute directly from ILRAM (0xE5200000): mov #0x2A, r0; nott; nott
    Check(core.Write16(0xE5200004u, 0x0068u), "Write16 second NOTT");
    // Jump via JMP @R1 from ROM to 0xE5200000
    SetupMinimalCore(core, {
        0x412Bu, // jmp @r1
        0x0009u, // delay slot nop
    });
    Check(core.Write32(0xE5200000u, 0xE02A0068u), "Write ILRAM code");
    Check(core.Write16(0xE5200004u, 0x0068u), "Write ILRAM NOTT2");
    Check(core.Write32(0xA8000100u, 0xE5200000u), "Stage addr");
    // Use PC-relative load or direct ROM: mov.l @(disp,PC), r1; jmp @r1; nop
    SetupMinimalCore(core, {
        0xD101u, // mov.l @(4,pc) -> 0xA0000008, r1
        0x412Bu, // jmp @r1
        0x0009u, // nop
        0x0009u, // padding
        0xE520u, 0x0000u, // literal 0xE5200000
    });
    Check(core.Write16(0xE5200000u, 0xE02Au), "ILRAM op0 mov #42, r0");
    Check(core.Write16(0xE5200002u, 0x0008u), "ILRAM op1 clrt");
    Check(core.Write16(0xE5200004u, 0x0068u), "ILRAM op2 nott (0->1)");
    Check(core.Write16(0xE5200006u, 0x0068u), "ILRAM op3 nott (1->0)");
    core.RunInstructions(6);
    Check(core.GetRegisters().r[0] == 0x2Au, "Executed mov #42, r0 from ILRAM");
    Check((core.GetRegisters().sr & Cg50NativeCore::kSrT) != 0u, "NOTT flipped T 0->1");
    core.RunInstructions(1);
    Check((core.GetRegisters().sr & Cg50NativeCore::kSrT) == 0u, "NOTT flipped T 1->0");
}

void TestStoreQueuesAndPref()
{
    // Program:
    //   mov.l @lit0, r1 (0xE0001000 -> SQ0 target offset 0x001000)
    //   pref @r1        (0x0183 -> flushes SQ0 to (QACR0<<24)|0x001000 = 0x08001000!)
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {
        0xD101u, // mov.l @(4,pc) -> 0xA0000008, r1
        0x0183u, // pref @r1
        0x0009u, // nop
        0x0009u, // pad
        0xE000u, 0x1000u, // SQ0 address targeting offset 0x1000
    });

    // QACR0 = 0x08 -> area 2 (0x08000000 RAM)
    Check(core.Write32(Cg50NativeCore::kQacr0Register, 0x08u), "Write QACR0");
    std::uint32_t q0 = 0;
    Check(core.Read32(Cg50NativeCore::kQacr0Register, q0) && q0 == 0x08u, "Read QACR0");

    // Fill SQ0 (32 bytes) with deterministic pattern
    for (std::uint32_t i = 0; i < 8u; ++i) {
        Check(core.Write32(0xE0000000u + i * 4u, 0xA5000000u + i), "Write SQ0 word");
    }
    core.RunInstructions(2);

    // Verify 32-byte burst landed in RAM at 0x08001000
    for (std::uint32_t i = 0; i < 8u; ++i) {
        std::uint32_t got = 0;
        Check(core.Read32(0x88001000u + i * 4u, got) && got == (0xA5000000u + i),
              "Store Queue PREF burst landed in RAM");
    }
}

void TestItlbArraysAndInvalidate()
{
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {0x0009u});
    const std::uint32_t addrWord = (0x00300u << 10) | 0x100u | 0x05u; // VPN=0x300, V=1, ASID=5
    const std::uint32_t dataWord = (0x08080u << 10) | 0x100u | 0x060u; // PPN=0x8080, V=1, PR=3
    Check(core.Write32(0xF2000200u, addrWord), "ITLB addr write entry 2");
    Check(core.Write32(0xF3000200u, dataWord), "ITLB data write entry 2");
    std::uint32_t rAddr = 0, rData = 0;
    Check(core.Read32(0xF2000200u, rAddr) && rAddr == addrWord, "ITLB addr readback");
    Check(core.Read32(0xF3000200u, rData) && rData == dataWord, "ITLB data readback");
    // Invalidate via MMUCR.TI (bit 2)
    Check(core.Write32(Cg50NativeCore::kMmuControlRegister, 0x04u), "MMUCR.TI invalidate");
    Check(core.Read32(0xF2000200u, rAddr) && rAddr == 0u, "ITLB cleared by MMUCR.TI");
}

void TestDmac6ChannelsAndSizesAndLcdAlias()
{
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {0x0009u});

    // Enable LCD pins (v28sys=1, resetX=1, RS=0 then RS=1)
    core.Peripherals().SetV28Sys(true);
    core.Peripherals().SetPfcPin(cg50::kLcdResetXPin, true);

    // 1. Test DMA CH4 (0xFE008070) with 4-byte transfer size (TS=010 -> bits 4:3 = 2)
    for (std::uint32_t i = 0; i < 4u; ++i) {
        Check(core.Write32(0x88002000u + i * 4u, 0xCAFE0000u + i), "Seed DMA4 src");
    }
    Check(core.Write32(0xFE008070u, 0x88002000u), "DMA4 SAR");
    Check(core.Write32(0xFE008074u, 0x88003000u), "DMA4 DAR");
    Check(core.Write32(0xFE008078u, 4u), "DMA4 TCR=4 (4x4=16B)");
    // SM=1 (inc: 0x1000), DM=1 (inc: 0x4000), TS=010 (4B: 0x0010), DE=1 (0x0001)
    Check(core.Write32(0xFE00807Cu, 0x00005011u), "DMA4 CHCR kick");
    for (std::uint32_t i = 0; i < 4u; ++i) {
        std::uint32_t got = 0;
        Check(core.Read32(0x88003000u + i * 4u, got) && got == (0xCAFE0000u + i),
              "DMA4 4-byte TS transfer exact");
    }

    // 2. Test DMA CH5 (0xFE008080) with 2-byte transfer size (TS=001) to LCD CS5A alias 0xB4009000!
    // Set R61524 cursor (0,0) and index 0x0202
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0200u), "LCD index 0x0200");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
    Check(core.Write16(0xB4000000u, 0x0000u), "LCD X=0");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0201u), "LCD index 0x0201");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
    Check(core.Write16(0xB4000000u, 0x0000u), "LCD Y=0");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0202u), "LCD index 0x0202");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);

    Check(core.Write16(0x88004000u, 0xF800u), "Pixel 0 red");
    Check(core.Write16(0x88004002u, 0x07E0u), "Pixel 1 green");
    Check(core.Write16(0x88004004u, 0x001Fu), "Pixel 2 blue");
    Check(core.Write32(0xFE008080u, 0x88004000u), "DMA5 SAR");
    Check(core.Write32(0xFE008084u, 0xB4009000u), "DMA5 DAR = CS5A alias 0xB4009000");
    Check(core.Write32(0xFE008088u, 3u), "DMA5 TCR=3 (3x2=6B)");
    // SM=1 (0x1000), DM=0 (fixed), TS=001 (2B: 0x0008), DE=1
    Check(core.Write32(0xFE00808Cu, 0x00001009u), "DMA5 CHCR kick to LCD alias");

    // 3. Test R61524 0x0202 GRAM Readback (1 dummy read + 3 RGB565 pixels!)
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0200u), "LCD reset X=0");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
    Check(core.Write16(0xB4000000u, 0x0000u), "LCD X=0");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0201u), "LCD reset Y=0");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
    Check(core.Write16(0xB4000000u, 0x0000u), "LCD Y=0");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0202u), "LCD index 0x0202 for read");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);

    std::uint16_t dummy = 0xFFFFu, p0 = 0, p1 = 0, p2 = 0;
    Check(core.Read16(0xB4000000u, dummy) && dummy == 0x0000u, "R61524 GRAM dummy read == 0");
    Check(core.Read16(0xB4000000u, p0) && p0 == 0xF800u, "R61524 GRAM read pixel 0 red");
    Check(core.Read16(0xB4000000u, p1) && p1 == 0x07E0u, "R61524 GRAM read pixel 1 green");
    Check(core.Read16(0xB4000000u, p2) && p2 == 0x001Fu, "R61524 GRAM read pixel 2 blue");

    // Verify 0x0003 still returns 0 (OS boot contract at PC=0x8004E0A4)
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x0003u), "LCD index 0x0003");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
    std::uint16_t em = 0xFFFFu;
    Check(core.Read16(0xB4000000u, em) && em == 0x0000u, "R61524 index 0x0003 stays 0 for boot");

    // Verify R61524 backlight & vertical scroll register storage
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
    Check(core.Write16(0xB4000000u, 0x05A1u), "LCD index 0x05A1");
    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
    Check(core.Write16(0xB4000000u, 0x00B4u), "LCD backlight duty 0xB4");
    Check(core.Peripherals().Lcd().BacklightDuty() == 0x00B4u, "R61524 BacklightDuty stored");
}

void TestEtmuIntc2AdcTpuScifUsb()
{
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {0x001Bu}); // SLEEP so AdvanceCycles ticks peripherals

    // 1. INTC2 mask set/clear + INTPRI00
    Check(core.Write32(0xA4140010u, 0x12345678u), "INTC2 INTPRI00 write");
    std::uint32_t pri = 0;
    Check(core.Read32(0xA4140010u, pri) && pri == 0x12345678u, "INTC2 INTPRI00 read");
    Check(core.Write8(0xA4140064u, 0x0Fu), "INTC2 INTMSKCLR00 clear low 4 bits");
    std::uint8_t msk = 0;
    Check(core.Read8(0xA4140044u, msk) && msk == 0xF0u, "INTC2 INTMSK00 == 0xF0");
    Check(core.Write8(0xA4140044u, 0x03u), "INTC2 INTMSK00 set bits 0..1");
    Check(core.Read8(0xA4140044u, msk) && msk == 0xF3u, "INTC2 INTMSK00 == 0xF3");

    // 2. ETMU0 countdown, underflow UNF, and reload
    Check(core.Write32(0xA44D0034u, 10u), "ETMU0 TCOR=10");
    Check(core.Write32(0xA44D0038u, 2u), "ETMU0 TCNT=2");
    Check(core.Write8(0xA44D003Cu, 0x01u), "ETMU0 TCR UNIE=1");
    Check(core.Write8(0xA44D0030u, 0x01u), "ETMU0 TSTR start");
    core.RunInstructions(1); // enter SLEEP
    core.AdvanceCycles(4096u); // 4 ticks @ 1024 cycles/tick -> underflows 2->1->0->10->9
    std::uint8_t tcr = 0;
    std::uint32_t tcnt = 0;
    Check(core.Read8(0xA44D003Cu, tcr) && (tcr & 0x02u) != 0u, "ETMU0 UNF set on underflow");
    Check(core.Read32(0xA44D0038u, tcnt) && tcnt == 9u, "ETMU0 reloaded TCOR and continued");

    // 3. ADC healthy battery conversion
    Check(core.Write16(0xA4610008u, 0x0020u), "ADC start conversion ADST=1");
    std::uint16_t adcsr = 0, addra = 0;
    Check(core.Read16(0xA4610008u, adcsr) && (adcsr & 0x0080u) != 0u && (adcsr & 0x0020u) == 0u,
          "ADC ADF=1 ADST=0");
    Check(core.Read16(0xA4610000u, addra) &&
              addra == static_cast<std::uint16_t>(cg50::AdcUnit::kHealthyBatteryRaw10 << 6),
          "ADC ADDRA returns full battery level");

    // 4. TPU start & tick
    Check(core.Write16(0xA44C0000u, 0x0001u), "TPU TSTR ch0 start");
    core.AdvanceCycles(640u);
    std::uint16_t tpuCnt = 0;
    Check(core.Read16(0xA44C0020u, tpuCnt) && tpuCnt >= 10u, "TPU ch0 ticked");

    // 5. SCIF0 loopback TX -> RX FIFO
    Check(core.Write16(0xA4410018u, 0x0007u), "SCIF0 SCFCR reset FIFOs + LOOP=1");
    Check(core.Write16(0xA4410018u, 0x0001u), "SCIF0 SCFCR LOOP=1");
    Check(core.Write8(0xA441000Cu, 0x5Au), "SCIF0 TX 0x5A");
    Check(core.Write8(0xA441000Cu, 0xC3u), "SCIF0 TX 0xC3");
    std::uint16_t scfdr = 0, scfsr = 0;
    Check(core.Read16(0xA441001Cu, scfdr) && (scfdr & 0x1Fu) == 2u, "SCIF0 RX count == 2");
    Check(core.Read16(0xA4410010u, scfsr) && (scfsr & 0x0063u) == 0x0063u, "SCIF0 TEND|TDFE|RDF|DR");
    std::uint8_t rx0 = 0, rx1 = 0;
    Check(core.Read8(0xA4410014u, rx0) && rx0 == 0x5Au, "SCIF0 RX byte 0 == 0x5A");
    Check(core.Read8(0xA4410014u, rx1) && rx1 == 0xC3u, "SCIF0 RX byte 1 == 0xC3");

    // 6. USB2 detached idle status
    std::uint16_t syssts = 0xFFFFu, cfifo = 0;
    Check(core.Read16(0xA4D80004u, syssts) && syssts == 0x0000u, "USB2 SYSSTS0 detached");
    Check(core.Read16(0xA4D80022u, cfifo) && (cfifo & 0x2000u) != 0u, "USB2 CFIFOCTR FRDY");

    // 7. Save/Restore roundtrip preserves all new hardware modules
    std::vector<std::uint8_t> blob;
    Check(core.ExportState(blob) && blob.size() > 1000u, "ExportState with chunks 82..94");
    auto restoredPtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& restored = *restoredPtr;
    SetupMinimalCore(restored, {0x0009u});
    Check(restored.ImportState(blob.data(), blob.size()), "ImportState with chunks 82..94");
    std::uint32_t pri2 = 0;
    Check(restored.Read32(0xA4140010u, pri2) && pri2 == 0x12345678u, "INTC2 restored");
    std::uint32_t tcnt2 = 0;
    Check(restored.Read32(0xA44D0038u, tcnt2) && tcnt2 == 9u, "ETMU0 restored");
}

void TestFlashCfiExtensionsAndFpuDn()
{
    // 1. Spansion Flash CFI Unlock Bypass + Erase Suspend/Resume
    cg50::SpansionNorFlashCfi cfi;
    std::uint16_t mem = 0xFFFFu;
    auto prog = [&](std::uint32_t, std::uint16_t v) noexcept { mem &= v; };
    auto erase = [&](std::uint32_t) noexcept { mem = 0xFFFFu; };

    // Enter Unlock Bypass: AA@AAA, 55@554, 20@AAA
    Check(cfi.HandleWrite16(0x00B60AAAu, 0x00AAu, prog, erase), "UB unlock1");
    Check(cfi.HandleWrite16(0x00B60554u, 0x0055u, prog, erase), "UB unlock2");
    Check(cfi.HandleWrite16(0x00B60AAAu, 0x0020u, prog, erase), "UB enter");
    Check(cfi.CurrentState() == cg50::SpansionNorFlashCfi::State::UnlockBypass, "In UnlockBypass");
    // Fast word program: A0 -> data
    Check(cfi.HandleWrite16(0x00B60000u, 0x00A0u, prog, erase), "UB program cmd");
    Check(cfi.HandleWrite16(0x00B60000u, 0x1234u, prog, erase), "UB program data");
    Check(mem == 0x1234u && cfi.GetStats().unlockBypassPrograms == 1u, "UB programmed 0x1234");
    // Exit Unlock Bypass: 90 -> 00
    Check(cfi.HandleWrite16(0x00B60000u, 0x0090u, prog, erase), "UB exit1");
    Check(cfi.HandleWrite16(0x00B60000u, 0x0000u, prog, erase), "UB exit2");
    Check(cfi.CurrentState() == cg50::SpansionNorFlashCfi::State::ReadArray, "Back to ReadArray");

    // Erase Suspend / Resume
    Check(cfi.HandleWrite16(0x00B60000u, 0x00B0u, prog, erase), "Erase Suspend 0xB0");
    Check(cfi.IsEraseSuspended(), "EraseSuspended == true");
    Check(cfi.HandleWrite16(0x00B60000u, 0x0030u, prog, erase), "Erase Resume 0x30");
    Check(!cfi.IsEraseSuspended(), "EraseSuspended == false");

    // 2. SH-4A FPSCR.DN flush subnormals to zero
    // Code:
    //   lds r0, fpscr (0x406A) -> sets DN=1 (0x00080000)
    //   fmov.s @r1, fr0 (0xF018) -> loads subnormal 0x00000001
    //   fadd fr1, fr0   (0xF010) -> flushes subnormal to +0.0f!
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {
        0xD002u, // mov.l @(8,pc) -> 0xA000000C, r0 = 0x00080000 (DN=1)
        0x406Au, // lds r0, fpscr
        0xD102u, // mov.l @(8,pc) -> 0xA0000010, r1 = 0x88005000
        0xF018u, // fmov.s @r1, fr0
        0xF010u, // fadd fr1, fr0 (fr1=0.0)
        0x0009u, // nop
        0x0008u, 0x0000u, // literal 0x00080000
        0x8800u, 0x5000u, // literal 0x88005000
    });
    // Enable FPU (clear SR.FD bit 15) — Reset leaves SR.FD=0, confirm:
    Check(core.Write32(0x88005000u, 0x00000001u), "Stage subnormal float 1e-45");
    core.RunInstructions(5);
    Check(core.GetRegisters().fr[0] == 0x00000000u, "FPSCR.DN flushed subnormal to +0.0f");

    // 3. Fixed sh_fpu.h RM=01 (TowardZero) IEEE-754 mantissa rounding (NOT integer floor!)
    //    1.75 in RM=01 must stay 1.75 (NOT 1.0 integer floor!); -1.75 must stay -1.75 (NOT -1.0!)
    //    And 1.0 + 1e-7 truncated to float toward zero must be <= exact double and > 1.0!
    cg50::FpuEventReport rep{};
    const double f15 = cg50::FpuRoundToPrecision(1.75, false, cg50::kFpuRoundingZero, rep);
    Check(std::fabs(f15 - 1.75) < 1e-6, "sh_fpu RM=01 1.75 == 1.75 (not integer floor!)");
    const double fNeg = cg50::FpuRoundToPrecision(-1.75, false, cg50::kFpuRoundingZero, rep);
    Check(std::fabs(fNeg - (-1.75)) < 1e-6, "sh_fpu RM=01 -1.75 == -1.75 (not integer ceil!)");
    const double dVal = cg50::FpuRoundToPrecision(1.0 / 3.0, true, cg50::kFpuRoundingZero, rep);
    Check(dVal > 0.333333333333 && dVal <= 0.33333333333333337, "sh_fpu RM=01 double 1/3 accurate");

    // 4. SpansionNorFlashCfi programmable busy-poll window (DQ6 toggle + DQ7 complement)
    cg50::SpansionNorFlashCfi cfiBusy;
    cfiBusy.SetBusyWindowCycles(20u, 20u);
    std::uint16_t flashWord = 0xFFFFu;
    auto progB = [&](std::uint32_t, std::uint16_t v) noexcept { flashWord &= v; };
    auto eraseB = [&](std::uint32_t) noexcept { flashWord = 0xFFFFu; };
    Check(cfiBusy.HandleWrite16(0x00B60AAAu, 0x00AAu, progB, eraseB), "Busy unlock1");
    Check(cfiBusy.HandleWrite16(0x00B60554u, 0x0055u, progB, eraseB), "Busy unlock2");
    Check(cfiBusy.HandleWrite16(0x00B60AAAu, 0x00A0u, progB, eraseB), "Busy word-prog cmd");
    Check(cfiBusy.HandleWrite16(0x00B60000u, 0x0080u, progB, eraseB), "Busy word-prog data 0x0080");
    Check(cfiBusy.IsBusy(), "Flash CFI entered Busy window");
    std::uint16_t poll1 = 0, poll2 = 0;
    Check(cfiBusy.TryRead16(0x00B60000u, poll1), "Busy poll 1 intercepted");
    Check(cfiBusy.TryRead16(0x00B60000u, poll2), "Busy poll 2 intercepted");
    Check(((poll1 ^ poll2) & 0x0040u) != 0u, "Flash CFI DQ6 toggled between consecutive busy polls");
    Check((poll1 & 0x0080u) == 0u, "Flash CFI DQ7 complemented programmed bit 7 (1->0)");
    cfiBusy.TickBusy(20u);
    Check(!cfiBusy.IsBusy() && cfiBusy.IsReadArray(), "Flash CFI exited Busy to ReadArray after cycles");
}

void TestSh4alDspStrictNoFpuAndClipDiv()
{
    // 1. Strict SH7305 No-FPU mode: 0xF000 raises EXPEVT 0x180 (General Illegal) when SR.DSP=0
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {
        0xD104u, // PC=0x00: mov.l @(lit0: 0x14), r1 = 0xA0001000 (VBR)
        0x412Eu, // PC=0x02: ldc r1, vbr
        0xD104u, // PC=0x04: mov.l @(lit1: 0x18), r1 = 0x40000000 (MD=1, BL=0)
        0x410Eu, // PC=0x06: ldc r1, sr
        0xF19Du, // PC=0x08: FLDI1 FR1 -> must trap EXPEVT 0x180 under strict SH7305!
        0x0009u, // PC=0x0A: nop
        0x0009u, 0x0009u, 0x0009u, 0x0009u, // pad to 0x14
        0xA000u, 0x1000u, // 0x14: VBR = 0xA0001000
        0x4000u, 0x0000u, // 0x18: SR  = 0x40000000
    });
    core.SetStrictSh7305NoFpu(true);
    core.RunInstructions(5);
    Check(core.GetRegisters().expevt == cg50::kExpevtIllegalInstruction,
          "Strict SH7305 No-FPU traps 0xF19D with EXPEVT 0x180");
    Check(core.GetRegisters().pc == 0xA0001100u, "Strict SH7305 No-FPU vectored to VBR+0x100");
    Check(core.GetAuthenticityStats().fpuOpcodesExecuted == 0u, "Zero FPU opcodes executed");

    // 2. SH4AL-DSP: SR.DSP=1, SETRC #3, LDRS, LDRE zero-overhead repeat loop + CLIPS.B/CLIPU.B/DIVS/DIVU
    //    Loop body: add #1, r0 (1 instruction at PC=0x0E) repeated 3 times -> r0 == 3!
    //    PC=0x00: mov.l @(lit0=0x20), r1 = 0x40001000 (MD=1, DSP=1)
    //    PC=0x02: ldc r1, sr
    //    PC=0x04: mov #0, r0
    //    PC=0x06: ldrs @(0x02, pc) -> RS = 0x06+4+4 = 0x0E
    //    PC=0x08: ldre @(0x01, pc) -> RE = 0x08+4+2 = 0x0E
    //    PC=0x0A: setrc #3 (0x8203)
    //    PC=0x0C: nop
    //    PC=0x0E: add #1, r0 (0x7001)  <-- loop body (RS=RE=0x0E, executes 3 times!)
    //    PC=0x10: mov.l @(lit1=0x24), r2 = 0x0000012C (300)
    //    PC=0x12: clips.b r2 (0x4291) -> clamps 300 to +127 (0x7F)
    //    PC=0x14: mov.l @(lit2=0x28), r3 = -5 (0xFFFFFFFB)
    //    PC=0x16: clipu.b r3 (0x4385) -> unsigned 0xFFFFFFFB clamps to 0xFF
    //    PC=0x18: mov #20, r4 (0xE414)
    //    PC=0x1A: mov #3, r5  (0xE503)
    //    PC=0x1C: divs r5, r4 (0x4494 -> r4 = 20/3 = 6)
    SetupMinimalCore(core, {
        0xD108u, // PC=0x00 -> (0x04)+0x20 = 0x24
        0x410Eu, // PC=0x02: ldc r1, sr
        0xE000u, // PC=0x04: mov #0, r0
        0x8C02u, // PC=0x06: ldrs @(2, pc) -> 0x0E
        0x8E01u, // PC=0x08: ldre @(1, pc) -> 0x0E
        0x8203u, // PC=0x0A: setrc #3
        0x0009u, // PC=0x0C: nop
        0x7001u, // PC=0x0E: add #1, r0 (repeated 3x -> r0 == 3!)
        0x6603u, // PC=0x10: mov r0, r6 (preserve repeat loop count!)
        0xD205u, // PC=0x12 -> (0x14)+0x14 = 0x28
        0x4291u, // PC=0x14: clips.b r2
        0xD305u, // PC=0x16 -> (0x18)+0x14 = 0x2C
        0x4381u, // PC=0x18: clipu.b r3
        0xE014u, // PC=0x1A: mov #20, r0
        0xE503u, // PC=0x1C: mov #3, r5
        0x4594u, // PC=0x1E: divs r0, r5 (r0 = 20/3 = 6, r5 = 20%3 = 2)
        0x0009u, // PC=0x20: nop
        0x0009u, // PC=0x22: pad
        0x4000u, 0x1000u, // 0x24: MD=1, DSP=1
        0x0000u, 0x012Cu, // 0x28: 300
        0xFFFFu, 0xFFFBu, // 0x2C: -5
    });
    core.RunInstructions(18);
    Check(core.GetRegisters().r[6] == 3u, "SH4AL-DSP SETRC/LDRS/LDRE repeat loop ran exactly 3 times");
    Check(core.GetRegisters().r[2] == 0x7Fu, "CLIPS.B clamped 300 -> 127");
    Check(core.GetRegisters().r[3] == 0xFFu, "CLIPU.B clamped 0xFFFFFFFB -> 0xFF");
    Check(core.GetRegisters().r[0] == 6u && core.GetRegisters().r[5] == 2u, "DIVS 20/3 == 6 rem 2");
    Check(core.GetAuthenticityStats().dspOpcodesExecuted >= 3u, "DSP telemetry recorded opcodes");

    // 3. Direct SH4AL-DSP parallel ALU (verified exact Renesas SH4AL-DSP / CPU73050.dll encodings)
    cg50::ShDspState dsp;
    dsp.Reset();
    dsp.x0 = 0x00030000u; // 3.0 in Q16 high word
    dsp.y0 = 0x00040000u; // 4.0 in Q16 high word
    std::array<std::uint32_t, 16> gpr{};
    std::uint32_t sr = 0x40001000u, mach = 0u, macl = 0u;
    auto r16 = [](std::uint32_t, std::uint16_t&) { return true; };
    auto w16 = [](std::uint32_t, std::uint16_t) { return true; };
    auto r32 = [](std::uint32_t, std::uint32_t&) { return true; };
    auto w32 = [](std::uint32_t, std::uint32_t) { return true; };
    // Parallel op1 = 0xF800 (no MOVX/MOVY), op2 = 0x4000 (PMULS X0, Y0, M0)
    Check(dsp.ExecuteParallel32(0xF800u, 0x4000u, gpr, sr, mach, macl, r16, w16, r32, w32), "DSP PMULS executed");
    Check(dsp.m0 == 24u, "DSP PMULS signed 16x16*2 == 24");
    dsp.a0 = 100;
    dsp.y0 = 250;
    // Official SH4AL-DSP PADD A0, Y0, A0: hi=0xB1, sx=2(A0)->0x80, sy=0(Y0)->0x00, dz=7(A0)->0x07 => 0xB187
    Check(dsp.ExecuteParallel32(0xF800u, 0xB187u, gpr, sr, mach, macl, r16, w16, r32, w32), "DSP PADD executed");
    Check(dsp.a0 == 350, "DSP PADD A0 + Y0 == 350");

    // 4. CPU73050.dll machine-code proof checks (Phase 1):
    //    - 0x4n5A (LDS Rn, FPUL) raises EXPEVT 0x180 under strictSh7305NoFpu
    //    - 0x4n6A / 0x0n6A access DSR (gated on SR.DSP), 0x4n7A..BA access A0/X0/X1/Y0/Y1
    //    - 0xF000..0xF3FF executes 16-bit DSP MOVX/MOVY when SR.DSP=1
    //    - RAMCR (0xFF000074) masks writes with 0x3E0
    SetupMinimalCore(core, {
        0xD108u, // PC=0x00 -> lit0 @ 0x24 = 0x40001000 (MD=1, DSP=1)
        0x410Eu, // PC=0x02: ldc r1, sr
        0xE255u, // PC=0x04: mov #0x55, r2
        0x426Au, // PC=0x06: lds r2, dsr
        0x086Au, // PC=0x08: sts dsr, r8 (unbanked!)
        0xE277u, // PC=0x0A: mov #0x77, r2
        0x428Au, // PC=0x0C: lds r2, x0
        0x098Au, // PC=0x0E: sts x0, r9 (unbanked!)
        0xF000u, // PC=0x10: 16-bit DSP NOPX/NOPY (valid when SR.DSP=1!)
        0xD105u, // PC=0x12 -> lit1 @ 0x28 = 0xA0001000 (VBR)
        0x412Eu, // PC=0x14: ldc r1, vbr
        0x425Au, // PC=0x16: LDS R2, FPUL -> MUST raise EXPEVT 0x180!
        0x0009u, // PC=0x18: nop
        0x0009u, 0x0009u, 0x0009u, 0x0009u, 0x0009u,
        0x4000u, 0x1000u, // 0x24: MD=1, DSP=1
        0xA000u, 0x1000u, // 0x28: VBR
    });
    core.SetStrictSh7305NoFpu(true);
    core.RunInstructions(12);
    Check(core.GetRegisters().r[8] == 0x55u, "CPU73050 proof: 0x426A/0x086A accesses DSR");
    Check(core.GetRegisters().r[9] == 0x77u, "CPU73050 proof: 0x428A/0x098A accesses X0");
    Check(core.GetRegisters().expevt == cg50::kExpevtIllegalInstruction,
          "CPU73050 proof: 0x425A (FPUL) raises EXPEVT 0x180");
    Check(core.GetRegisters().pc == 0xA0001100u, "CPU73050 proof: 0x425A vectored to VBR+0x100");
    Check(core.Write32(0xFF000074u, 0xFFFFFFFFu), "RAMCR write");
    std::uint32_t ramcrVal = 0;
    Check(core.Read32(0xFF000074u, ramcrVal) && ramcrVal == 0x000003E0u,
          "CPU73050 proof: RAMCR (0xFF000074) masked to 0x3E0");
}

void TestCacheOcramItlbAndControlRegs()
{
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    SetupMinimalCore(core, {
        0xD102u, // PC=0x00: mov.l @(lit: 0x0C), r1 = 0x88001000
        0x0193u, // PC=0x02: ocbi @r1
        0x01A3u, // PC=0x04: ocbp @r1
        0x01B3u, // PC=0x06: ocbwb @r1
        0x01E3u, // PC=0x08: icbi @r1
        0x0009u, // PC=0x0A: nop
        0x8800u, 0x1000u, // 0x0C
    });
    // Enable OCE + ICE + ORA (OCRAM 16 KB at 0x7C000000)
    const std::uint32_t ccr = cg50::ShCacheUnit::kCcrOce | cg50::ShCacheUnit::kCcrIce |
                              cg50::ShCacheUnit::kCcrOra;
    Check(core.Write32(Cg50NativeCore::kCacheControlRegister, ccr), "Write CCR OCE|ICE|ORA");
    Check(core.Cache().OcramEnabled(), "OCRAM enabled");
    Check(core.Write32(0x7C000000u, 0xDEADBEEFu), "Write32 OCRAM 0x7C000000");
    Check(core.Write32(0x7C003FFCu, 0x12345678u), "Write32 OCRAM end 0x7C003FFC");
    std::uint32_t v0 = 0, v1 = 0;
    Check(core.Read32(0x7C000000u, v0) && v0 == 0xDEADBEEFu, "Read32 OCRAM start");
    Check(core.Read32(0x7C003FFCu, v1) && v1 == 0x12345678u, "Read32 OCRAM end");

    // MMIO OC address & data array access (0xF4000000 / 0xF5000000)
    Check(core.Write32(0xF4000000u, 0x08001003u), "Write OC address array tag+V+D");
    Check(core.Write32(0xF5000000u, 0xCAFEBABEu), "Write OC data array word 0");
    std::uint32_t ocTag = 0, ocData = 0;
    Check(core.Read32(0xF4000000u, ocTag) && ocTag == 0x08001003u, "Read OC address array");
    Check(core.Read32(0xF5000000u, ocData) && ocData == 0xCAFEBABEu, "Read OC data array");

    // Execute OCBI / OCBP / OCBWB / ICBI
    core.RunInstructions(5);
    Check(core.Cache().GetStats().ocbiOps == 1u &&
          core.Cache().GetStats().ocbpOps == 1u &&
          core.Cache().GetStats().ocbwbOps == 1u &&
          core.Cache().GetStats().icbiOps == 1u,
          "Cache instructions OCBI/OCBP/OCBWB/ICBI tracked in ShCacheUnit");

    // RAMCR / PASCR / IRMCR / XSA..YEA (0xFF000050..0xFF000078)
    Check(core.Write32(0xFF000074u, 0x00000200u), "Write RAMCR");
    Check(core.Write32(0xFF000070u, 0x00000080u), "Write PASCR");
    Check(core.Write32(0xFF000078u, 0x0000000Fu), "Write IRMCR");
    Check(core.Write32(0xFF000050u, 0xE5007000u), "Write XSA");
    std::uint32_t rRamcr = 0, rPascr = 0, rIrmcr = 0, rXsa = 0;
    Check(core.Read32(0xFF000074u, rRamcr) && rRamcr == 0x00000200u, "Read RAMCR");
    Check(core.Read32(0xFF000070u, rPascr) && rPascr == 0x00000080u, "Read PASCR");
    Check(core.Read32(0xFF000078u, rIrmcr) && rIrmcr == 0x0000000Fu, "Read IRMCR");
    Check(core.Read32(0xFF000050u, rXsa) && rXsa == 0xE5007000u, "Read XSA");
}

void TestIntc2NmiCmtWdtAdcSdhiScifPeerUsbUbcLcd()
{
    // 1. INTC2 External IRQ0 + NMI + USERIMASK
    auto corePtr = std::make_unique<Cg50NativeCore>();
    auto& core = *corePtr;
    SetupMinimalCore(core, {
        0xD103u, // PC=0x00: mov.l @(lit0: 0x10), r1 = 0xA0001000 (VBR)
        0x412Eu, // PC=0x02: ldc r1, vbr
        0xD103u, // PC=0x04: mov.l @(lit1: 0x14), r1 = 0x40000000 (MD=1, BL=0, IMASK=0)
        0x410Eu, // PC=0x06: ldc r1, sr
        0x0009u, // PC=0x08: nop
        0x0009u, // PC=0x0A: nop
        0x0009u, 0x0009u, // 0x0C..0x0E pad
        0xA000u, 0x1000u, // 0x10: VBR
        0x4000u, 0x0000u, // 0x14: SR
    });
    core.RunInstructions(4); // VBR set, BL=0, IMASK=0
    // Test USERIMASK key check (0xA50000F0 accepted, 0x000000F0 ignored)
    Check(core.Write32(0xA4141000u, 0x00000050u), "Write USERIMASK bad key");
    std::uint32_t uim = 0;
    Check(core.Read32(0xA4141000u, uim) && uim == 0u, "USERIMASK rejected bad key");
    Check(core.Write32(0xA4141000u, 0xA5000070u), "Write USERIMASK valid key");
    Check(core.Read32(0xA4141000u, uim) && uim == 0x70u, "USERIMASK accepted 0x70");

    // Unmask IRQ0, set priority 10 (bits 31..28 of INTPRI00), raise IRQ0
    Check(core.Write8(0xA4140064u, 0x01u), "Unmask IRQ0");
    Check(core.Write32(0xA4140010u, 0xA0000000u), "IRQ0 prio=10");
    core.Intc2Mut().RaiseExternalIrq(0u);
    core.RunInstructions(1);
    Check(core.GetRegisters().intevt == 0x0600u && core.GetRegisters().pc == 0xA0001600u,
          "INTC2 delivered External IRQ0 (0x600) to VBR+0x600");

    // Trigger NMI (clear BL first via new minimal run)
    SetupMinimalCore(core, {
        0xD103u, 0x412Eu, 0xD103u, 0x410Eu, 0x0009u, 0x0009u, 0x0009u, 0x0009u,
        0xA000u, 0x1000u, 0x4000u, 0x00F0u, // BL=0, IMASK=15 (all maskables masked, NMI still fires!)
    });
    core.RunInstructions(4);
    core.Intc2Mut().TriggerNmi();
    core.RunInstructions(1);
    Check(core.GetRegisters().expevt == 0x01C0u && core.GetRegisters().pc == 0xA0001100u,
          "INTC2 NMI bypassed IMASK=15 and vectored to 0x1C0");

    // 2. ADC at exact CPU73050.dll addresses 0xA4610080..0xA461008E + battery scaling + IRQ
    core.SetBatteryPercent(15);
    Check(core.Write16(0xA4610088u, 0x0060u), "ADC ADCSR @ 0xA4610088 ADIE|ADST");
    std::uint16_t adcsr = 0, addrb = 0, addrc = 0;
    Check(core.Read16(0xA4610088u, adcsr) && (adcsr & 0x00C0u) == 0x00C0u, "ADC ADF|ADIE set at 0x88");
    Check(core.Read16(0xA4610082u, addrb) && addrb == static_cast<std::uint16_t>(0x03FFu << 6),
          "ADC ADDRB (MEM.RYBY) == Ready");
    Check(core.Read16(0xA4610084u, addrc) &&
          addrc < static_cast<std::uint16_t>(cg50::AdcUnit::kHealthyBatteryRaw10 << 6),
          "ADC ADDRC reflects 15% low battery");
    Check(core.Adc().HasPendingIrq(), "ADC pending IRQ signaled");

    // 3. SDHI at 0xA4CF001C..22 & DdBus at 0xA44C0000..20
    std::uint16_t sdMask = 0;
    Check(core.Read16(0xA4CF0020u, sdMask) && sdMask == 0xFFFFu, "SDHI SD_INFO1_MASK default 0xFFFF");
    Check(core.Write16(0xA4CF0020u, 0x001Du), "SDHI write SD_INFO1_MASK");
    Check(core.Read16(0xA4CF0020u, sdMask) && sdMask == 0x001Du, "SDHI readback SD_INFO1_MASK");
    Check(core.Write8(0xA44C0006u, 0x05u), "DDCS_CNTR write");
    std::uint8_t ddcs = 0;
    Check(core.Read8(0xA44C0006u, ddcs) && ddcs == 0x05u, "DDCS_CNTR readback");

    // 4. SCIF0 virtual 3-pin peer-to-peer cable between TWO Cg50NativeCore instances
    {
        auto calcAPtr = std::make_unique<Cg50NativeCore>();
        auto calcBPtr = std::make_unique<Cg50NativeCore>();
        auto& calcA = *calcAPtr;
        auto& calcB = *calcBPtr;
        SetupMinimalCore(calcA, {0x001Bu}); // SLEEP
        SetupMinimalCore(calcB, {0x001Bu});
        calcA.Scif0Mut().ConnectPeer(&calcB.Scif0Mut());
        calcB.Scif0Mut().ConnectPeer(&calcA.Scif0Mut());
        Check(calcA.Write8(0xA4410004u, 0x00u), "SCIF0 A BRR=0 (fast baud)");
        Check(calcA.Write8(0xA441000Cu, 0x42u), "SCIF0 A TX 0x42");
        Check(calcA.Write8(0xA441000Cu, 0x99u), "SCIF0 A TX 0x99");
        calcA.RunInstructions(1);
        calcA.AdvanceCycles(256u);
        std::uint8_t b0 = 0, b1 = 0;
        Check(calcB.Scif0().RxCount() == 2u, "SCIF0 B received 2 bytes over virtual 3-pin cable");
        Check(calcB.Read8(0xA4410014u, b0) && b0 == 0x42u, "SCIF0 B byte 0 == 0x42");
        Check(calcB.Read8(0xA4410014u, b1) && b1 == 0x99u, "SCIF0 B byte 1 == 0x99");
    }

    // 5. USB 2.0 virtual host VBUS attach + SETUP + Bulk OUT/IN pipes + IRQ
    core.Usb2Mut().Reset();
    Check(core.Write16(0xA4D80030u, cg50::Usb2Unit::kIntVbint | cg50::Usb2Unit::kIntCtrt |
                                    cg50::Usb2Unit::kIntBrdy), "USB2 INTENB0");
    core.Usb2Mut().SetVbusAttached(true);
    std::uint16_t usbSts = 0;
    Check(core.Read16(0xA4D80004u, usbSts) && usbSts == 0x0001u, "USB2 SYSSTS attached FS");
    Check(core.Usb2().HasPendingIrq(), "USB2 VBUS attach raised IRQ");
    core.Usb2Mut().InjectSetupPacket(0x0680u, 0x0100u, 0x0000u, 0x0012u); // GET_DESCRIPTOR
    std::uint16_t uReq = 0, uLen = 0;
    Check(core.Read16(0xA4D80054u, uReq) && uReq == 0x0680u, "USB2 USBREQ readback");
    Check(core.Read16(0xA4D8005Au, uLen) && uLen == 0x0012u, "USB2 USBLENG readback");
    // Host sends Bulk OUT bytes on pipe 1 -> guest reads CFIFO
    const std::uint8_t bulkOut[4] = {0x55u, 0x53u, 0x42u, 0x43u}; // "USBC" CBW signature!
    core.Usb2Mut().HostPushOutPipe(1u, bulkOut, 4u);
    Check(core.Write16(0xA4D80020u, 0x0401u), "USB2 CFIFOSEL pipe=1 MBW=16");
    std::uint16_t w0 = 0, w1 = 0;
    Check(core.Read16(0xA4D80014u, w0) && w0 == 0x5553u, "USB2 CFIFO Bulk OUT word 0");
    Check(core.Read16(0xA4D80014u, w1) && w1 == 0x4243u, "USB2 CFIFO Bulk OUT word 1");
    // Guest writes Bulk IN bytes on pipe 2 -> host pops
    Check(core.Write16(0xA4D80020u, 0x0402u), "USB2 CFIFOSEL pipe=2 MBW=16");
    Check(core.Write16(0xA4D80014u, 0x5553u), "USB2 CFIFO Bulk IN word 0");
    Check(core.Write16(0xA4D80014u, 0x4253u), "USB2 CFIFO Bulk IN word 1");
    const auto inPkt = core.Usb2Mut().HostPopInPipe(2u);
    Check(inPkt.size() == 4u && inPkt[0] == 0x55u && inPkt[3] == 0x53u, "USB2 Bulk IN CSW popped by host");

    // 6. UBC hardware fetch breakpoint (EXPEVT 0x1E0 + CCMFR.MF0=1)
    SetupMinimalCore(core, {
        0xD103u, 0x412Eu, 0xD103u, 0x410Eu, 0x0009u, 0x0009u, 0x0009u, 0x0009u,
        0xA000u, 0x1000u, 0x4000u, 0x0000u,
    });
    core.RunInstructions(4); // PC is now 0xA0000008, BL=0
    Check(core.Write32(0xFF200008u, 0xA000000Au), "UBC CAR0 = 0xA000000A");
    Check(core.Write32(0xFF20000Cu, 0x00000000u), "UBC CAMR0 = 0 (exact match)");
    Check(core.Write32(0xFF200004u, 0x00000001u), "UBC CRR0 BIE=1");
    Check(core.Write32(0xFF200000u, 0x00000011u), "UBC CBR0 CE=1 ID=01(fetch)");
    core.RunInstructions(1); // executes 0xA0000008 -> PC=0xA000000A
    Check(core.GetRegisters().pc == 0xA000000Au, "Reached breakpoint PC");
    core.RunInstructions(1); // hits UBC breakpoint at 0xA000000A!
    Check(core.GetRegisters().expevt == 0x01E0u && core.GetRegisters().pc == 0xA0001100u,
          "UBC triggered hardware breakpoint EXPEVT 0x1E0");
    Check((core.Ubc().Ccmfr() & 1u) != 0u && core.GetAuthenticityStats().ubcBreaks == 1u,
          "UBC CCMFR.MF0 and telemetry incremented");

    // 7. WDT Watchdog-mode reset (WT/IT=0 + SetWatchdogResetEnabled(true))
    SetupMinimalCore(core, {0x001Bu}); // SLEEP
    core.SetWatchdogResetEnabled(true);
    Check(core.Write16(0xA4520000u, 0x5A00u | 0xFEu), "WTCNT = 0xFE");
    Check(core.Write16(0xA4520004u, 0xA500u | 0x20u), "WTCSR TME=1 WT/IT=0 CKS=0 (div 1)");
    core.RunInstructions(1);
    core.AdvanceCycles(4u);
    Check(core.GetAuthenticityStats().watchdogResets == 1u && core.GetRegisters().pc == 0xA0000000u,
          "WDT watchdog-mode overflow triggered clean CPU reset");

    // 8. R61524 scroll, mirror, CABC brightness, blanking, RenderDisplayRgb565, 60 Hz FMARK
    cg50::R61524 lcd;
    lcd.Reset();
    cg50::R61524::Pins pins{};
    pins.v28sys = true;
    pins.resetX = true;
    pins.chipSelect = false;
    auto lcdReg = [&](std::uint16_t idx, std::uint16_t val) {
        pins.registerSelect = false; lcd.SetPins(pins); lcd.ObserveWrite16(idx);
        pins.registerSelect = true;  lcd.SetPins(pins); lcd.ObserveWrite16(val);
    };
    lcdReg(0x0007u, 0x0173u); // Display ON
    lcdReg(0x0200u, 0x0000u); lcdReg(0x0201u, 0x0000u);
    pins.registerSelect = false; lcd.SetPins(pins); lcd.ObserveWrite16(0x0202u);
    pins.registerSelect = true;  lcd.SetPins(pins); lcd.ObserveWrite16(0xF800u); // Red at (0,0)
    lcdReg(0x0401u, 0x0001u); // VLE=1 enable vertical scroll
    lcdReg(0x0404u, 0x0001u); // Scroll by 1 line
    lcdReg(0x05A1u, 0x0080u); // 50% backlight
    std::vector<std::uint16_t> disp(cg50::R61524::kVisibleWidth * cg50::R61524::kVisibleHeight, 0u);
    lcd.RenderDisplayRgb565(disp.data(), disp.size());
    Check(disp[0] == 0x0000u, "R61524 vertical scroll shifted row 0");
    // Blanking via Power Control 1 STB=1 (0x0100 bit 2) -> all black
    lcdReg(0x0100u, 0x0004u);
    Check(lcd.DisplayBlanked(), "R61524 STB standby blanks display");
    lcd.RenderDisplayRgb565(disp.data(), disp.size());
    Check(disp[0] == 0x0000u, "R61524 display blanking renders black");
    // 60 Hz FMARK (~550,000 cycles/frame, 10% active pulse window)
    lcd.TickFrameMark(550000u);
    Check(lcd.FrameMarkActive(), "R61524 60 Hz FMARK pulsed active at frame boundary");
    lcd.TickFrameMark(100000u);
    Check(!lcd.FrameMarkActive(), "R61524 60 Hz FMARK returned inactive after pulse window");

    // 9. Strict No-HLE compile-time & env audit check
#ifdef CG50_ENABLE_LEGACY_DEBUG_HLE
    Check(false, "CG50_ENABLE_LEGACY_DEBUG_HLE MUST NOT be defined in standard/cert builds!");
#endif
    std::vector<std::string> badEnvs;
    Check(Cg50NativeCore::AuditForbiddenEnvOverrides(badEnvs) == 0u,
          "Zero forbidden HLE env overrides active");

    // 10. Extended Save/Restore roundtrip (chunks 96..105: DSP, UBC, SDHI, OCRAM, RAMCR)
    SetupMinimalCore(core, {0x0009u});
    Check(core.Write32(Cg50NativeCore::kCacheControlRegister, cg50::ShCacheUnit::kCcrOra), "Enable OCRAM");
    Check(core.Write32(0x7C000100u, 0x55AA33CCu), "Write OCRAM pattern");
    Check(core.Write32(0xFF000074u, 0x00000200u), "Write RAMCR pattern");
    Check(core.Write16(0xA4CF0020u, 0x1357u), "Write SDHI mask");
    Check(core.Write32(0xFF200008u, 0x80012340u), "Write UBC CAR0");
    core.DspMut().a0 = 0x123456789ll;
    std::vector<std::uint8_t> snap;
    Check(core.ExportState(snap), "ExportState v44 chunks 96..105");
    auto restoredPtr = std::make_unique<Cg50NativeCore>();
    auto& restored = *restoredPtr;
    SetupMinimalCore(restored, {0x0009u});
    Check(restored.ImportState(snap.data(), snap.size()), "ImportState v44 chunks 96..105");
    std::uint32_t rOcram = 0, rRamcr = 0, rCar0 = 0;
    std::uint16_t rSdhi = 0;
    Check(restored.Read32(0x7C000100u, rOcram) && rOcram == 0x55AA33CCu, "Restored OCRAM");
    Check(restored.Read32(0xFF000074u, rRamcr) && rRamcr == 0x00000200u, "Restored RAMCR");
    Check(restored.Read16(0xA4CF0020u, rSdhi) && rSdhi == 0x1357u, "Restored SDHI");
    Check(restored.Read32(0xFF200008u, rCar0) && rCar0 == 0x80012340u, "Restored UBC CAR0");
    Check(restored.Dsp().a0 == 0x123456789ll, "Restored SH4AL-DSP 40-bit A0");
}

void TestDspBcdCacheIntc2CornerCases()
{
    // 1. SH4AL-DSP LDS Rm, A0 (0x427A) with SR.DSP=0 MUST raise EXPEVT 0x180
    {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupMinimalCore(core, {
            0xD104u, // PC=0x00 -> lit 0x14: VBR=0xA0001000
            0x412Eu, // PC=0x02: LDC R1, VBR
            0xD104u, // PC=0x04 -> lit 0x18: SR=0x40000000 (MD=1, BL=0, DSP=0)
            0x410Eu, // PC=0x06: LDC R1, SR
            0xE255u, // PC=0x08: MOV #0x55, R2
            0x427Au, // PC=0x0A: LDS R2, A0 -> MUST trap 0x180 when SR.DSP=0!
            0x0009u, 0x0009u, 0x0009u, 0x0009u,
            0xA000u, 0x1000u, // 0x14: VBR
            0x4000u, 0x0000u, // 0x18: SR (DSP=0)
        });
        core.RunInstructions(6);
        Check(core.GetRegisters().expevt == 0x180u && core.GetRegisters().pc == 0xA0001100u,
              "LDS Rm, A0 traps EXPEVT 0x180 when SR.DSP=0");
    }

    // 2. SH4AL-DSP positive 32-bit saturation (DSR.S=1) & SETRC Rm (0x4n14) multi-bit RC
    {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupMinimalCore(core, {
            0xE204u, // PC=0x00: MOV #4, R2
            0x4214u, // PC=0x02: SETRC R2 -> SR.RC = 4 (tests bits above bit 0!)
            0x0009u,
        });
        core.RunInstructions(2);
        const std::uint32_t rc = (core.GetRegisters().sr & Cg50NativeCore::kSrRcMask) >> 16;
        Check(rc == 4u, "SETRC Rm preserves full 12-bit repeat count (RC == 4)");

        std::uint32_t satSr = 0x40001002u; // SR.MD=1, SR.DSP=1, SR.S=1 (official SH4AL-DSP saturation bit!)
        const std::int64_t satOut = core.DspMut().FinalizeArith40(0x123456789ll, false, false, satSr, true);
        Check(satOut == 0x7FFFFFFFll, "DSP FinalizeArith40 saturates positive overflow to 0x7FFFFFFF when SR.S=1");
    }

    // 3. Hardware BCD ALU exact decimal carry boundary: 5 + 5 = 10 (s == 10 > 9!)
    {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupMinimalCore(core, {0x0009u});
        Check(core.Write32(0xA4CB0014u, 0x00000005u), "BCD FASSRA=5");
        Check(core.Write32(0xA4CB0018u, 0x00000005u), "BCD FASSRB=5");
        Check(core.Write16(0xA4CB0010u, 0x0001u), "BCD ADD 5+5");
        std::uint32_t res = 0;
        Check(core.Read32(0xA4CB001Cu, res) && res == 0x00000010u,
              "BCD ALU exact decimal carry threshold 5+5 == 0x10");
    }

    // 4. SH-4A Cache CCR.OCI invalidates populated OC line
    {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupMinimalCore(core, {0x0009u});
        Check(core.Write32(0xF4000000u, 0x08001001u), "Seed valid OC line via MMIO address array (V=bit0)");
        std::uint32_t tagBefore = 0, tagAfter = 0xFFFFFFFFu;
        Check(core.Read32(0xF4000000u, tagBefore) && (tagBefore & 0x1u) != 0u, "OC line valid before CCR.OCI");
        Check(core.Write32(Cg50NativeCore::kCacheControlRegister, cg50::ShCacheUnit::kCcrOci), "Write CCR.OCI");
        Check(core.Read32(0xF4000000u, tagAfter) && (tagAfter & 0x1u) == 0u, "OC line invalidated by CCR.OCI");
    }

    // 5. INTC2 INTMSK00 suppresses masked external IRQ
    {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupMinimalCore(core, {0x0009u});
        Check(core.Write32(0xA4140010u, 0x0A000000u), "IRQ1 prio=10");
        Check(core.Write8(0xA4140044u, 0x02u), "Mask IRQ1 via INTMSK00");
        core.Intc2Mut().RaiseExternalIrq(1u);
        std::uint32_t ev = 0; std::uint8_t pr = 0; std::size_t idx = 0;
        Check(!core.Intc2().HighestPendingExternalIrq(ev, pr, idx),
              "INTC2 INTMSK00 blocks masked IRQ1 from HighestPendingExternalIrq");
    }

    // 6. Mutation-hardened corner cases (MOD wrap, Cache ICI/OCRAM/OCBP/OCBWB, UBC masks+CH1 data, INTC2 W0C, ETMU dir, SCIF0 ORER, USB2 SETUP)
    {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupMinimalCore(core, {0x0009u});

        // MUT-050: SH4AL-DSP MOD circular buffer wraparound
        core.DspMut().mod = (0x1002u << 16) | 0x1000u; // MS=0x1000, ME=0x1002
        Check(core.Dsp().StepModuloAddress(0x88001002u, 2) == 0x88001000u,
              "SH4AL-DSP MOD circular buffer wraps ME -> MS");

        // MUT-063: Cache CCR.ICI invalidates populated IC line
        Check(core.Write32(0xF0000000u, 0x08002001u), "Seed valid IC line (V=bit0)");
        std::uint32_t icBefore = 0, icAfter = 0xFFFFFFFFu;
        Check(core.Read32(0xF0000000u, icBefore) && (icBefore & 1u) != 0u, "IC line valid before CCR.ICI");
        Check(core.Write32(Cg50NativeCore::kCacheControlRegister, cg50::ShCacheUnit::kCcrIci), "Write CCR.ICI");
        Check(core.Read32(0xF0000000u, icAfter) && (icAfter & 1u) == 0u, "IC line invalidated by CCR.ICI");

        // MUT-064: OCRAM strictly gated by CCR.ORA=1
        Check(core.Write32(Cg50NativeCore::kCacheControlRegister, 0u), "CCR.ORA=0");
        Check(!core.Cache().OcramEnabled(), "OCRAM disabled when CCR.ORA=0");
        Check(core.Write32(Cg50NativeCore::kCacheControlRegister, cg50::ShCacheUnit::kCcrOra), "CCR.ORA=1");
        Check(core.Cache().OcramEnabled(), "OCRAM enabled when CCR.ORA=1");

        // MUT-065 & MUT-066: OCBP dirty writeback + invalidate & OCBWB dirty writeback + clean
        cg50::ShCacheUnit cache;
        cache.Reset();
        cache.ApplyCcrWrite(cg50::ShCacheUnit::kCcrOce | cg50::ShCacheUnit::kCcrCb);
        cache.ObserveOcAccess(0x08001000u, true, true, nullptr); // populates dirty OC line
        const std::uint64_t wb0 = cache.GetStats().ocWritebacks;
        cache.Ocbp(0x08001000u);
        Check(cache.GetStats().ocWritebacks == wb0 + 1u, "OCBP flushes dirty OC line (+1 writeback)");
        cache.ObserveOcAccess(0x08001020u, true, true, nullptr); // populates another dirty OC line
        const std::uint64_t wb1 = cache.GetStats().ocWritebacks;
        cache.Ocbwb(0x08001020u);
        Check(cache.GetStats().ocWritebacks == wb1 + 1u, "OCBWB flushes dirty OC line (+1 writeback)");
        cache.Ocbwb(0x08001020u);
        Check(cache.GetStats().ocWritebacks == wb1 + 1u, "Second OCBWB sees clean line (dirty cleared to false)");

        // MUT-069, MUT-070, MUT-071, MUT-072: UBC CCMFR W0C, CETR1 12-bit mask, CBCR bit-0 mask, CH1 data match
        cg50::UbcUnit ubc;
        ubc.Reset();
        Check(ubc.Write32(0xFF200038u, 0xFFFFFFFFu), "UBC write CETR1=0xFFFFFFFF");
        std::uint32_t ubcVal = 0;
        Check(ubc.Read32(0xFF200038u, ubcVal) && ubcVal == 0x00000FFFu, "UBC CETR1 masked to 12 bits");
        Check(ubc.Write32(0xFF200620u, 0xFFFFFFFFu), "UBC write CBCR=0xFFFFFFFF");
        Check(ubc.Read32(0xFF200620u, ubcVal) && ubcVal == 0x00000001u, "UBC CBCR masked to bit 0");
        // CCMFR W0C: writing 0xFFFFFFFF when 0 must NOT set bits!
        Check(ubc.Write32(0xFF200600u, 0xFFFFFFFFu), "UBC write CCMFR=0xFFFFFFFF");
        Check(ubc.Read32(0xFF200600u, ubcVal) && ubcVal == 0u, "UBC CCMFR W0C cannot set bits to 1");
        // CH1 data match (CBR1: CE=1, RW=10 write, ID=10 operand, DBE=bit6 => 0x65)
        Check(ubc.Write32(0xFF200028u, 0x88002000u), "UBC CAR1");
        Check(ubc.Write32(0xFF20002Cu, 0x00000000u), "UBC CAMR1");
        Check(ubc.Write32(0xFF200030u, 0x12345678u), "UBC CDR1");
        Check(ubc.Write32(0xFF200034u, 0x00000000u), "UBC CDMR1");
        Check(ubc.Write32(0xFF200024u, 0x00000001u), "UBC CRR1 BIE=1");
        Check(ubc.Write32(0xFF200020u, 0x00000065u), "UBC CBR1 CE=1,RW=write,ID=operand,DBE=1");
        Check(!ubc.CheckDataMatch(0x88002000u, 0xDEADBEEFu, true) && (ubc.Ccmfr() & 2u) == 0u,
              "UBC CH1 rejects mismatched data value");
        Check(ubc.CheckDataMatch(0x88002000u, 0x12345678u, true) && (ubc.Ccmfr() & 2u) != 0u,
              "UBC CH1 triggers on matching data value");

        // MUT-074: INTC2 INTREQ00 W0C (writing 0xFF when 0 cannot set bits!)
        Check(core.Write8(0xA4140024u, 0xFFu), "INTC2 write INTREQ00=0xFF");
        std::uint8_t req8 = 0xFFu;
        Check(core.Read8(0xA4140024u, req8) && req8 == 0x00u, "INTC2 INTREQ00 W0C stays 0");

        // MUT-078: ETMU0 strictly decrements TCNT (never increments!)
        {
            auto ePtr = std::make_unique<Cg50NativeCore>();
            auto& eCore = *ePtr;
            SetupMinimalCore(eCore, {0x001Bu}); // SLEEP so AdvanceCycles ticks peripherals
            Check(eCore.Write32(0xA44D0034u, 500u), "ETMU0 TCOR=500");
            Check(eCore.Write32(0xA44D0038u, 500u), "ETMU0 TCNT=500");
            Check(eCore.Write8(0xA44D0030u, 0x01u), "ETMU0 TSTR=1");
            eCore.RunInstructions(1); // enter SLEEP
            eCore.AdvanceCycles(4096u); // 4 ticks @ 1024 cycles/tick -> 500 - 4 == 496
            std::uint32_t etmuCnt = 0;
            Check(eCore.Read32(0xA44D0038u, etmuCnt) && etmuCnt == 496u, "ETMU0 counts strictly down (500 -> 496)");
        }

        // MUT-089: SCIF0 ORER overrun flag on 17th RX byte
        cg50::Scif0Unit scif;
        scif.Reset();
        for (int i = 0; i < 17; ++i) scif.PushRx(static_cast<std::uint8_t>(i));
        std::uint16_t sclsr = 0;
        Check(scif.Read16(0x04410024u, sclsr) && (sclsr & 0x0001u) != 0u,
              "SCIF0 sets SCLSR.ORER on 17th RX byte overrun");

        // MUT-092: USB2 InjectSetupPacket sets INTSTS0 VALID (0x0008) & CTRT (0x0800)
        cg50::Usb2Unit usb;
        usb.Reset();
        usb.InjectSetupPacket(0x0680u, 0x0100u, 0x0000u, 0x0012u);
        std::uint16_t intsts0 = 0;
        Check(usb.Read16(0x04D80040u, intsts0) &&
                  (intsts0 & (cg50::Usb2Unit::kIntValid | cg50::Usb2Unit::kIntCtrt)) ==
                      (cg50::Usb2Unit::kIntValid | cg50::Usb2Unit::kIntCtrt),
              "USB2 InjectSetupPacket latches INTSTS0 VALID|CTRT");
    }
}

} // namespace

int main()
{
    TestOnChipMemoriesAndIlramExec();
    TestStoreQueuesAndPref();
    TestItlbArraysAndInvalidate();
    TestDmac6ChannelsAndSizesAndLcdAlias();
    TestEtmuIntc2AdcTpuScifUsb();
    TestFlashCfiExtensionsAndFpuDn();
    TestSh4alDspStrictNoFpuAndClipDiv();
    TestCacheOcramItlbAndControlRegs();
    TestIntc2NmiCmtWdtAdcSdhiScifPeerUsbUbcLcd();
    TestDspBcdCacheIntc2CornerCases();

    if (gFailures == 0) {
        std::printf("[SH7305-HW-TEST] ALL PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[SH7305-HW-TEST] FAILED count=%d\n", gFailures);
    return 1;
}
