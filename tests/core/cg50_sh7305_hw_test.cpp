#include "cg50_native_core.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
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
    Cg50NativeCore core;
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
    Cg50NativeCore core;
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
    Cg50NativeCore core;
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
    Cg50NativeCore core;
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
    Cg50NativeCore core;
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
    Cg50NativeCore restored;
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
    Cg50NativeCore core;
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

    if (gFailures == 0) {
        std::printf("[SH7305-HW-TEST] ALL PASS\n");
        return 0;
    }
    std::fprintf(stderr, "[SH7305-HW-TEST] FAILED count=%d\n", gFailures);
    return 1;
}
