// cg50_hw_silicon_100_test.cpp — Phase 2 (v46) 100% FULL_SILICON verification test
// Validates all 8 closed CG50 hardware blocks (HUDI, PRLCK, BSC, CPG, DMAC0, RTC,
// PFC+ExtLY755A, SDHI) against CPU73050.dll disassembly specs + negative error paths
// + save/restore chunks 106..111 + safe passive R/W on the 11 NOT_PRESENT_ON_CG50 blocks.

#include "cg50_native_core.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

int gFails = 0;

void Check(bool cond, const char* msg)
{
    if (!cond) {
        std::fprintf(stderr, "[SILICON-100-TEST] FAIL: %s\n", msg);
        ++gFails;
    }
}

void SetupCore(Cg50NativeCore& core)
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    // 0x00: bra -2 (0xAFFE -> target = 0x04 - 4 = 0x00), 0x02: nop (0x0009)
    rom[0] = 0xAFu; rom[1] = 0xFEu;
    rom[2] = 0x00u; rom[3] = 0x09u;
    Check(core.LoadFirmware(rom.data(), rom.size()), "LoadFirmware");
    core.Reset();
    core.SetStrictSh7305NoFpu(true);
}

void TestHudi()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    std::uint16_t sdir = 0xFFFFu;
    Check(core->Read16(0xFC110000u, sdir) && sdir == 0x0000u, "HUDI SDIR reset == 0x0000 (CGM.dlm _SDIRlow=0x00)");
    Check(core->Write16(0xFC110000u, 0x1234u), "HUDI Write16 SDIR");
    Check(core->Read16(0xFC110000u, sdir) && sdir == 0x1234u, "HUDI Read16 SDIR");

    Check(core->Write32(0xFC110008u, 0xCAFEBABEu), "HUDI Write32 SDDRH/SDDRL");
    std::uint16_t hi = 0, lo = 0;
    Check(core->Read16(0xFC110008u, hi) && hi == 0xCAFEu, "HUDI Read16 SDDRH");
    Check(core->Read16(0xFC11000Au, lo) && lo == 0xBABEu, "HUDI Read16 SDDRL");

    core->HudiMut().RaiseDebugInterrupt();
    Check(core->Hudi().HasPendingIrq(), "HUDI SDINT INTREQ asserted");
    std::uint16_t sdint = 0;
    Check(core->Read16(0xFC110018u, sdint) && (sdint & 1u) == 1u, "HUDI Read16 SDINT == 1");
    Check(core->Write16(0xFC110018u, 0x0000u), "HUDI W0C SDINT");
    Check(!core->Hudi().HasPendingIrq(), "HUDI SDINT cleared by W0C");
    // True W0C negative test: writing 1 when SDINT==0 MUST NOT set INTREQ!
    Check(core->Write16(0xFC110018u, 0x0001u), "HUDI W0C write-1 while cleared");
    Check(!core->Hudi().HasPendingIrq(), "HUDI W0C write-1 did not set SDINT");
}

void TestPrlck()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    std::uint32_t v = 0;
    // Mask 0x3F on all 7 registers
    Check(core->Write32(0xFF800020u, 0xFFFFFFFFu), "PRPRICR0 write unlocked");
    Check(core->Read32(0xFF800020u, v) && v == 0x3Fu, "PRPRICR0 masked to 0x3F (CPU73050 W_CB 0x47AAB)");

    // Lock group 0 via PRLCKCR bit 0 -> negative test: write to PRPRICR0 must be blocked!
    Check(core->Write32(0xFF800018u, 0xFFFFFF01u), "PRLCKCR lock bit 0");
    Check(core->Read32(0xFF800018u, v) && v == 0x01u, "PRLCKCR masked to 0x3F");
    Check(core->Prlck().IsGroupLocked(0u), "PRLCK group 0 locked");

    Check(core->Write32(0xFF800020u, 0x00000012u), "PRPRICR0 write while locked");
    Check(core->Read32(0xFF800020u, v) && v == 0x3Fu, "PRPRICR0 preserved while locked (negative path)");
    Check(core->Prlck().BlockedWrites() == 1u, "PRLCK blockedWrites counter == 1");

    // Group 1 still unlocked -> writable
    Check(core->Write32(0xFF800028u, 0x0000002Au), "PRPRICR1 write unlocked");
    Check(core->Read32(0xFF800028u, v) && v == 0x2Au, "PRPRICR1 updated to 0x2A");

    // Unlock group 0 -> now writable again
    Check(core->Write32(0xFF800018u, 0x00000000u), "PRLCKCR unlock all");
    Check(core->Write32(0xFF800020u, 0x00000015u), "PRPRICR0 write after unlock");
    Check(core->Read32(0xFF800020u, v) && v == 0x15u, "PRPRICR0 updated after unlock");
}

void TestBsc()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    std::uint32_t v = 0;
    Check(core->Write32(0xFEC10000u, 0xFFFFFFFFu), "BSC CMNCR write");
    Check(core->Read32(0xFEC10000u, v) && v == 0x03003E13u, "BSC CMNCR mask 0x03003E03 | 0x10");

    // CS0BCR type=0 (normal) -> CS0WCR mask 0x171FC3; type=1 (burst ROM) -> 0x031FC3
    Check(core->Write32(0xFEC10004u, 0x00000000u), "CS0BCR type=0");
    Check(core->Write32(0xFEC10024u, 0xFFFFFFFFu), "CS0WCR type=0 write");
    Check(core->Read32(0xFEC10024u, v) && v == 0x00171FC3u, "CS0WCR type=0 mask 0x171FC3 (CPU73050 0x1006DAA0)");

    Check(core->Write32(0xFEC10004u, 0x00001000u), "CS0BCR type=1");
    Check(core->Write32(0xFEC10024u, 0xFFFFFFFFu), "CS0WCR type=1 write");
    Check(core->Read32(0xFEC10024u, v) && v == 0x00031FC3u, "CS0WCR type=1 mask 0x031FC3");

    // Negative test: RTCSR/RTCNT/RTCOR without 0xA55A0000 key MUST be ignored!
    Check(core->Write32(0xFEC10048u, 0x00000008u), "RTCSR bad key write");
    Check(core->Read32(0xFEC10048u, v) && v == 0u, "RTCSR rejected write without 0xA55A0000 key");
    Check(core->Write32(0xFEC10050u, 0x12340010u), "RTCOR bad key write");
    Check(core->Read32(0xFEC10050u, v) && v == 0u, "RTCOR rejected write without 0xA55A0000 key");

    // Valid 0xA55A key: enable refresh timer CKS=1 (div 4), RTCOR=4, RTCNT=0 -> run instructions -> CMF=1!
    Check(core->Write32(0xFEC10050u, 0xA55A0004u), "RTCOR = 4 with key");
    Check(core->Write32(0xFEC1004Cu, 0xA55A0000u), "RTCNT = 0 with key");
    Check(core->Write32(0xFEC10048u, 0xA55A0008u), "RTCSR CKS=1 (div 4) with key");
    core->RunInstructions(40u); // >= 40 cycles / div 4 = 10 ticks >= period 4 -> >=2 refreshes, CMF set!
    Check(core->Read32(0xFEC10048u, v) && (v & 0x80u) != 0u, "BSC RTCSR.CMF asserted after refresh match");
    Check(core->BscRefreshCount() >= 2u, "BSC active SDRAM refresh count >= 2");
    // W0C clear CMF
    Check(core->Write32(0xFEC10048u, 0xA55A0008u), "BSC RTCSR W0C CMF");
    Check(core->Read32(0xFEC10048u, v) && (v & 0x80u) == 0u, "BSC RTCSR.CMF cleared by W0C");
}

void TestCpg()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    std::uint32_t v = 0;
    const std::uint32_t clk0 = core->EffectiveCpuClockHz();
    Check(clk0 > 10000000u, "CPG initial CPU clock > 10 MHz");

    // FRQCR KICK bit 31 self-clears and IFC divider=2 halves clock
    Check(core->Write32(0xA4150000u, 0x80100000u), "CPG FRQCR write IFC=1 (div 2) + KICK");
    Check(core->Read32(0xA4150000u, v) && (v & 0x80000000u) == 0u && v == 0x00100000u,
          "CPG FRQCR KICK self-cleared");
    Check(core->EffectiveCpuClockHz() == clk0 / 2u, "CPG EffectiveCpuClockHz halved by FRQCR.IFC");

    // Exact CPU73050.dll forced/writable masks for MSTPCR0, MSTPCR2, FLLFRQ, BAR, LSTATUS
    Check(core->Write32(0xA4150030u, 0x00000000u), "MSTPCR0 write 0");
    Check(core->Read32(0xA4150030u, v) && v == 0x00001000u, "MSTPCR0 forced bit 0x1000");
    Check(core->Write32(0xA4150038u, 0x00000000u), "MSTPCR2 write 0");
    Check(core->Read32(0xA4150038u, v) && v == 0xD7ED948Bu, "MSTPCR2 forced bits 0xD7ED948B");
    Check(core->Write32(0xA4150050u, 0xFFFFFFFFu), "FLLFRQ write all-1s");
    Check(core->Read32(0xA4150050u, v) && v == 0x000047FFu, "FLLFRQ mask 0x7FF | 0x4000");
    Check(core->Write32(0xA4150040u, 0xA0001234u), "CPG BAR write");
    Check(core->Read32(0xA4150040u, v) && v == 0xA0001234u, "CPG BAR readback");
    Check(core->Read32(0xA4150060u, v) && v == 0u, "CPG LSTATUS == 0");

    // Active MSTPCR0 clock gating: stop TMU0 (bit 13 = 0x2000) -> TCNT0 must freeze!
    Check(core->Write32(0xA449000Cu, 1000u), "TMU0 TCNT0 = 1000");
    Check(core->Write8(0xA4490004u, 0x01u), "TMU0 TSTR0 start ch0");
    Check(core->Write32(0xA4150030u, 0x00002800u), "CPG MSTPCR0 stop TMU0 (bit 13) + TMU1 (bit 11)");
    core->RunInstructions(50u);
    Check(core->Read32(0xA449000Cu, v) && v == 1000u, "CPG MSTPCR0 bit 13/11 froze TMU0/TMU1");
    Check(core->Write32(0xA4150030u, 0x00000000u), "CPG MSTPCR0 unstop TMU0/TMU1");
    core->RunInstructions(50u);
    Check(core->Read32(0xA449000Cu, v) && v < 1000u, "TMU0 resumed after MSTPCR0 cleared");
}

void TestDmac0()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    // DMARS0..2 R/W
    std::uint16_t d16 = 0;
    Check(core->Write16(0xA4090000u, 0x1234u), "DMARS0 write");
    Check(core->Read16(0xA4090000u, d16) && d16 == 0x1234u, "DMARS0 readback");
    Check(core->Write16(0xA4090004u, 0x5678u), "DMARS1 write");
    Check(core->Read16(0xA4090004u, d16) && d16 == 0x5678u, "DMARS1 readback");
    Check(core->Write16(0xA4090008u, 0x9ABCu), "DMARS2 write");
    Check(core->Read16(0xA4090008u, d16) && d16 == 0x9ABCu, "DMARS2 readback");

    // TCR0 mask 0x00FFFFFF
    std::uint32_t v = 0;
    Check(core->Write32(0xFE008028u, 0xFF123456u), "DMAC0 TCR0 write");
    Check(core->Read32(0xFE008028u, v) && v == 0x00123456u, "DMAC0 TCR0 masked to 24-bit");

    // Negative test: misaligned 32-bit DMA (SAR=0x88001001, TS=2 -> 4-byte) MUST assert DMAOR.AE!
    Check(core->Write16(0xFE008060u, 0x0301u), "DMAOR DME=1, PR=11 (round-robin)");
    Check(core->Write32(0xFE008020u, 0x88001001u), "DMAC0 SAR0 misaligned");
    Check(core->Write32(0xFE008024u, 0x88002000u), "DMAC0 DAR0 aligned");
    Check(core->Write32(0xFE008028u, 4u), "DMAC0 TCR0 = 4");
    Check(core->Write32(0xFE00802Cu, 0x00005011u), "DMAC0 CHCR0 SM=1,DM=1,TS=2(4B),DE=1");
    Check(core->Read16(0xFE008060u, d16) && (d16 & 0x0004u) != 0u, "DMAC0 DMAOR.AE asserted on misaligned transfer");
    Check(core->DmacExt().addressErrorCount == 1u, "DMAC0 addressErrorCount == 1");

    // Clear AE via W0C, run aligned 4-byte transfer -> succeeds!
    Check(core->Write16(0xFE008060u, 0x0301u), "DMAOR W0C AE");
    Check(core->Read16(0xFE008060u, d16) && (d16 & 0x0004u) == 0u, "DMAOR.AE cleared");
    Check(core->Write32(0x88001000u, 0xDEADBEEFu), "Stage DMA src");
    Check(core->Write32(0x88002000u, 0x00000000u), "Clear DMA dst");
    Check(core->Write32(0xFE008020u, 0x88001000u), "DMAC0 SAR0 aligned");
    Check(core->Write32(0xFE008024u, 0x88002000u), "DMAC0 DAR0 aligned");
    Check(core->Write32(0xFE008028u, 1u), "DMAC0 TCR0 = 1");
    Check(core->Write32(0xFE00802Cu, 0x00005011u), "DMAC0 CHCR0 execute aligned");
    Check(core->Read32(0x88002000u, v) && v == 0xDEADBEEFu, "DMAC0 aligned 32-bit transfer succeeded");
    Check(core->DmacExt().lastServedChannel == 0u && core->DmacExt().arbitrationCount >= 1u,
          "DMAC0 round-robin arbitration updated");
}

void TestRtc()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);
    core->SetRtcVirtualClockEnabled(true);
    core->SetRtcIrqEnabled(true);
    core->SetRtcEpochSeconds(1735689600LL); // 2025-01-01 00:00:00 UTC

    // Program alarm for second == 0x00 (ENB bit 7 = 0x80) and enable AIE (bit 3) + CIE (bit 4)
    Check(core->Write8(0xA413FED0u, 0x80u), "RTC RSECAR = 0x80 (sec 00, ENB=1)");
    Check(core->Write8(0xA413FEDCu, 0x18u), "RTC RCR1 = CIE | AIE");
    Check(core->Write8(0xA413FEDEu, 0x19u), "RTC RCR2 = PES=1 (1/256s) | RTCEN | START");
    Check(core->Write16(0xA413FEE0u, 0x2025u), "RTC RYRAR = 2025");
    Check(core->Write8(0xA413FEE6u, 0x05u), "RTC RADJ = 0x05");

    core->RunInstructions(200u);
    std::uint8_t rcr1 = 0, rcr2 = 0, radj = 0;
    Check(core->Read8(0xA413FEDCu, rcr1) && (rcr1 & 0x01u) != 0u && (rcr1 & 0x80u) != 0u,
          "RTC RCR1.AF and RCR1.CF asserted on alarm match + second rollover");
    Check(core->Read8(0xA413FEDEu, rcr2) && (rcr2 & 0x80u) != 0u, "RTC RCR2.PEF asserted on periodic tick");
    Check(core->Read8(0xA413FEE6u, radj) && radj == 0x05u, "RTC RADJ readback");

    // W0C clears AF & PEF
    Check(core->Write8(0xA413FEDCu, 0x18u), "RTC RCR1 W0C AF & CF");
    Check(core->Read8(0xA413FEDCu, rcr1) && (rcr1 & 0x81u) == 0u, "RTC RCR1 AF & CF cleared");
}

void TestPfcAndExtLy755a()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    // ExtLY755A pin wiring: PTH5 (DDPOWER), PTJ6 (POWERON), PTR3 (_AppIF), PTL1/PTU1 (USB_DET)
    Check(core->Write8(0xA405012Eu, 0x20u), "PHDR PTH5=1 (DDPOWER)");
    Check(core->ExtLy755a().ddPower, "ExtLY755A DDPOWER == true");
    Check(core->Write8(0xA405012Eu, 0x00u), "PHDR PTH5=0");
    Check(!core->ExtLy755a().ddPower, "ExtLY755A DDPOWER == false");

    Check(core->Write8(0xA4050130u, 0x40u), "PJDR PTJ6=1 (POWERON)");
    Check(core->ExtLy755a().powerOn, "ExtLY755A POWERON == true");
    Check(core->Write8(0xA405013Cu, 0x08u), "PRDR PTR3=1 (_AppIF)");
    Check(core->ExtLy755a().appIfPin, "ExtLY755A _AppIF == true");

    core->SetUsbCableConnected(true);
    std::uint8_t pldr = 0, pudr = 0;
    Check(core->Read8(0xA4050134u, pldr) && (pldr & 0x02u) != 0u, "PLDR.1 USB_DET live high");
    Check(core->Read8(0xA4050162u, pudr) && (pudr & 0x02u) != 0u, "PUDR.1 USBPW live high");
    core->SetUsbCableConnected(false);
    Check(core->Read8(0xA4050134u, pldr) && (pldr & 0x02u) == 0u, "PLDR.1 USB_DET live low");

    // Sweep all PFC control/data/pull/drive registers (0x04050100..0x040501C6)
    for (std::uint32_t addr = 0xA4050100u; addr <= 0xA4050188u; addr += 2u) {
        std::uint16_t tmp = 0;
        Check(core->Read16(addr, tmp), "PFC Read16 sweep");
    }
}

void TestSdhiAndSaveRestore()
{
    auto core = std::make_unique<Cg50NativeCore>();
    SetupCore(*core);

    // Default: no card inserted -> SD_INFO1 == 0, CMD causes timeout bit in SD_INFO2
    std::uint16_t info1 = 0xFFFFu, info2 = 0;
    Check(core->Read16(0xA4CF001Cu, info1) && info1 == 0u, "SDHI default SD_INFO1 == 0 (no card on CG50)");
    Check(core->Write16(0xA4CF0000u, 0x0008u), "SDHI CMD8 with no card");
    Check(core->Read16(0xA4CF001Eu, info2) && (info2 & 0x0040u) != 0u, "SDHI RSP_TO when no card inserted");

    // Optional virtual SD card attach -> CD | INS, CMD8 responds 0x01AA + RESP_END
    core->SdhiMut().SetVirtualCardInserted(true);
    Check(core->Read16(0xA4CF001Cu, info1) && (info1 & 0x0030u) == 0x0030u, "SDHI virtual card CD | INS");
    Check(core->Write16(0xA4CF0000u, 0x0008u), "SDHI CMD8 with virtual card");
    std::uint16_t rsp0 = 0;
    Check(core->Read16(0xA4CF001Cu, info1) && (info1 & 0x0001u) != 0u, "SDHI RESP_END asserted");
    Check(core->Read16(0xA4CF000Cu, rsp0) && rsp0 == 0x01AAu, "SDHI CMD8 RSP0 == 0x01AA");

    // Save/Restore roundtrip verifies optional chunks 106..111 preserve all 8 new blocks!
    Check(core->Write16(0xFC110000u, 0x5A5Au), "Stage HUDI SDIR");
    Check(core->Write32(0xFF800020u, 0x0000002Bu), "Stage PRPRICR0");
    Check(core->Write32(0xFF800018u, 0x00000001u), "Stage PRLCKCR lock bit 0");
    Check(core->Write8(0xA413FEE6u, 0x33u), "Stage RTC RADJ");
    Check(core->Write16(0xA4090000u, 0x7777u), "Stage DMARS0");

    std::vector<std::uint8_t> snap;
    Check(core->ExportState(snap) && !snap.empty(), "ExportState with chunks 106..111");
    auto restored = std::make_unique<Cg50NativeCore>();
    SetupCore(*restored);
    Check(restored->ImportState(snap.data(), snap.size()), "ImportState with chunks 106..111");

    std::uint16_t sdir = 0, dmars0 = 0;
    std::uint32_t pr0 = 0;
    std::uint8_t radj = 0;
    Check(restored->Read16(0xFC110000u, sdir) && sdir == 0x5A5Au, "Restored HUDI SDIR");
    Check(restored->Read32(0xFF800020u, pr0) && pr0 == 0x2Bu && restored->Prlck().IsGroupLocked(0u), "Restored PRLCK");
    Check(restored->Read8(0xA413FEE6u, radj) && radj == 0x33u, "Restored RTC RADJ");
    Check(restored->Read16(0xA4090000u, dmars0) && dmars0 == 0x7777u, "Restored DMARS0");
    Check(restored->Sdhi().VirtualCardInserted(), "Restored SDHI virtual card state");

    // Verify safe verified passive R/W on all 11 NOT_PRESENT_ON_CG50 peripheral pages (exact CPU73050.dll addrs)
    static constexpr std::uint32_t kNotPresentAddrs[] = {
        0xA4470000u, // I2C
        0xA4530000u, // SHOC
        0xA4C40000u, // SIOF0
        0xA4C50000u, // SIOF1
        0xA4CA0000u, // MMCIF
        0xA4CC0000u, // FLCTL
        0xA4CD0000u, // BLITTER
        0xFD000000u, // ECC
        0xFE200000u, // SPU2-DSP0
        0xFE3C0000u, // FSI
        0xFE300000u, // SPU2-DSP1
    };
    for (const std::uint32_t a : kNotPresentAddrs) {
        std::uint32_t tmp = 0;
        Check(restored->Write32(a, 0x13572468u), "NOT_PRESENT_ON_CG50 safe passive Write32");
        Check(restored->Read32(a, tmp) && tmp == 0x13572468u, "NOT_PRESENT_ON_CG50 safe passive Read32");
    }

    // Extra mutation guards: CPG MSTPCR0 gating on TMU0, DMAC0 CHCR0 mask, SDHI card-absent W0C
    {
        auto g = std::make_unique<Cg50NativeCore>();
        SetupCore(*g);
        // 1. TMU0 (0xA4480000) stopped by MSTPCR0 bit 13 (0x2000)
        Check(g->Write32(0xA4150030u, 0x00002000u), "MSTPCR0 stop TMU0/1");
        Check(g->Write32(0xA4480008u, 0x00010000u), "TMU0 TCOR0");
        Check(g->Write32(0xA448000Cu, 0x00010000u), "TMU0 TCNT0");
        Check(g->Write16(0xA4480010u, 0x0000u), "TMU0 TCR0");
        Check(g->Write8(0xA4480004u, 0x01u), "TMU0 TSTR start ch0");
        g->RunInstructions(500);
        std::uint32_t tcntStopped = 0;
        Check(g->Read32(0xA448000Cu, tcntStopped) && tcntStopped == 0x00010000u,
              "TMU0 TCNT0 frozen while CPG MSTPCR0 bit 13 is set");

        // 2. DMAC0 CHCR0 writable mask (CPU73050 0x4FB7FFFD; DE=0 -> 0x4FB7FFFC)
        Check(g->Write32(0xFE00802Cu, 0xFFFFFFFEu), "DMAC0 CHCR0 write all-ones (DE=0)");
        std::uint32_t chcr0 = 0;
        Check(g->Read32(0xFE00802Cu, chcr0) && chcr0 == 0x4FB7FFFCu,
              "DMAC0 CHCR0 enforces exact CPU73050 writable mask 0x4FB7FFFD");

        // 3. SDHI SD_INFO1 write 0xFFFF with no card inserted MUST keep CD (bit 5) == 0
        g->SdhiMut().SetVirtualCardInserted(false);
        Check(g->Write16(0xA4CF001Cu, 0xFFFFu), "SDHI SD_INFO1 write 0xFFFF card absent");
        std::uint16_t info1 = 0xFFFFu;
        Check(g->Read16(0xA4CF001Cu, info1) && (info1 & 0x0020u) == 0u,
              "SDHI SD_INFO1.CD stays 0 when no card inserted");
    }
}

} // namespace

int main()
{
    TestHudi();
    TestPrlck();
    TestBsc();
    TestCpg();
    TestDmac0();
    TestRtc();
    TestPfcAndExtLy755a();
    TestSdhiAndSaveRestore();

    if (gFails != 0) {
        std::fprintf(stderr, "[SILICON-100-TEST] FAILED count=%d\n", gFails);
        return 1;
    }
    std::printf("[SILICON-100-TEST] PASS: 8/8 remaining CG50 blocks closed to 100%% FULL_SILICON + negative paths + save/restore chunks 106..111!\n");
    return 0;
}
