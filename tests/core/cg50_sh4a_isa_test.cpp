// cg50_sh4a_isa_test.cpp — Unit tests for SH-4A FPU, CPU, MMU multi-hit,
// CLRMAC/LDTLB separation, and real-time RTC epoch sync.

#include "cg50_native_core.h"
#include "cg50_core/sh_exception.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace {

int g_fails = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::fprintf(stderr, "[SH4A-TEST] FAIL (%s:%d): %s\n",            \
                         __FILE__, __LINE__, (msg));                          \
            ++g_fails;                                                        \
        }                                                                     \
    } while (0)

std::uint32_t FloatBits(float f)
{
    std::uint32_t u = 0;
    std::memcpy(&u, &f, 4);
    return u;
}

float BitsFloat(std::uint32_t u)
{
    float f = 0.0f;
    std::memcpy(&f, &u, 4);
    return f;
}

double BitsDouble(std::uint32_t hi, std::uint32_t lo)
{
    std::uint64_t u = (static_cast<std::uint64_t>(hi) << 32) | lo;
    double d = 0.0;
    std::memcpy(&d, &u, 8);
    return d;
}

void PutOp(std::vector<std::uint8_t>& rom, std::size_t off, std::uint16_t op)
{
    rom[off] = static_cast<std::uint8_t>((op >> 8) & 0xFFu);
    rom[off + 1u] = static_cast<std::uint8_t>(op & 0xFFu);
}

void Put32(std::vector<std::uint8_t>& rom, std::size_t off, std::uint32_t v)
{
    rom[off] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
    rom[off + 1u] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    rom[off + 2u] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    rom[off + 3u] = static_cast<std::uint8_t>(v & 0xFFu);
}

void TestFpuEncodingAndFmov32()
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    // Program at 0xA0000000:
    //  0x00: MOV.L @(lit0), R1    ; R1 = 0x88001000 (RAM scratch)
    //  0x02: MOV.L @(lit1), R2    ; R2 = 0x3FC90FDB (float pi/2)
    //  0x04: MOV.L R2, @R1        ; store 32-bit float to RAM
    //  0x06: FMOV.S @R1, FR0      ; 0xF016 — MUST load full 32 bits, NOT 16!
    //  0x08: FLDI1 FR1            ; 0xF19D -> FR1 = 1.0f
    //  0x0A: FLDI0 FR2            ; 0xF28D -> FR2 = 0.0f
    //  0x0C: FMAC FR0, FR1, FR2   ; 0xF21E -> FR2 = FR0*FR1 + FR2 = pi/2
    //  0x0E: FMAC FR0, FR0, FR0   ; 0xF00E (n=0,m=0) -> FR0 = (pi/2)^2 + pi/2 (NOT FIPR!)
    //  0x10: MOV.L @(lit2), R3    ; R3 = 42
    //  0x12: LDS R3, FPUL         ; 0x435A
    //  0x14: FLOAT FPUL, FR4      ; 0xF42D -> FR4 = 42.0f
    //  0x16: FPCHG                ; 0xF7FD -> PR=1 (double mode)
    //  0x18: FLOAT FPUL, DR6      ; 0xF62D -> DR6 = 42.0 (double)
    //  0x1A: FTRC DR6, FPUL       ; 0xF63D -> FPUL = 42
    //  0x1C: FPCHG                ; 0xF7FD -> PR=0 (single mode)
    //  0x1E: FCNVSD FPUL, DR8     ; 0xF8AD (load FPUL=FloatBits(3.5f) first below)
    std::size_t p = 0;
    PutOp(rom, p, 0xD110u); p += 2; // PC=0x00 -> lit at (0x04)+0x40 = 0x44
    PutOp(rom, p, 0xD210u); p += 2; // PC=0x02 -> lit at (0x04)+0x40 = 0x44? Wait ((2+4)&~3)=4 + 0x40 = 0x44; use disp 0x11 -> 0x48!
    // Re-encode with exact PC-relative offsets:
    p = 0;
    PutOp(rom, p, 0xD110u); p += 2; // PC=0x00 -> (0x04&~3)+0x40 = 0x44 (lit0: 0x88001000)
    PutOp(rom, p, 0xD211u); p += 2; // PC=0x02 -> (0x06&~3)+0x44 = 0x48 (lit1: 0x3FC90FDB)
    PutOp(rom, p, 0x2122u); p += 2; // PC=0x04: MOV.L R2, @R1
    PutOp(rom, p, 0xF016u); p += 2; // PC=0x06: FMOV.S @R1, FR0
    PutOp(rom, p, 0xF19Du); p += 2; // PC=0x08: FLDI1 FR1
    PutOp(rom, p, 0xF28Du); p += 2; // PC=0x0A: FLDI0 FR2
    PutOp(rom, p, 0xF21Eu); p += 2; // PC=0x0C: FMAC FR0, FR1, FR2 -> FR2 = pi/2
    PutOp(rom, p, 0xF00Eu); p += 2; // PC=0x0E: FMAC FR0, FR0, FR0 -> FR0 = (pi/2)^2 + pi/2
    PutOp(rom, p, 0xE32Au); p += 2; // PC=0x10: MOV #42, R3
    PutOp(rom, p, 0x435Au); p += 2; // PC=0x12: LDS R3, FPUL
    PutOp(rom, p, 0xF42Du); p += 2; // PC=0x14: FLOAT FPUL, FR4 -> 42.0f
    PutOp(rom, p, 0xF7FDu); p += 2; // PC=0x16: FPCHG -> PR=1
    PutOp(rom, p, 0xF62Du); p += 2; // PC=0x18: FLOAT FPUL, DR6 -> 42.0
    PutOp(rom, p, 0xF63Du); p += 2; // PC=0x1A: FTRC DR6, FPUL -> 42
    PutOp(rom, p, 0xF7FDu); p += 2; // PC=0x1C: FPCHG -> PR=0
    PutOp(rom, p, 0xD30Bu); p += 2; // PC=0x1E -> (0x20)+0x2C = 0x4C (lit2: FloatBits(3.5f))
    PutOp(rom, p, 0x435Au); p += 2; // PC=0x20: LDS R3, FPUL
    PutOp(rom, p, 0xF8ADu); p += 2; // PC=0x22: FCNVSD FPUL, DR8 -> DR8 = 3.5 (double)
    PutOp(rom, p, 0xF8BDu); p += 2; // PC=0x24: FCNVDS DR8, FPUL -> FPUL = FloatBits(3.5f)
    // Test SZ=1 pair FMOV to XD2 (odd reg=3 -> XF[2..3]) and back from RAM
    PutOp(rom, p, 0xF3FDu); p += 2; // PC=0x26: FSCHG -> SZ=1
    PutOp(rom, p, 0xF38Cu); p += 2; // PC=0x28: FMOV DR8, XD2 (n=3 odd -> XF[2..3], m=8 -> FR[8..9])
    PutOp(rom, p, 0xF138u); p += 2; // PC=0x2A: FMOV XD2, @R1 (stores 8 bytes to 0x88001000)
    PutOp(rom, p, 0xFA16u); p += 2; // PC=0x2C: FMOV @R1, DR10 (loads 8 bytes into FR[10..11])
    PutOp(rom, p, 0xF3FDu); p += 2; // PC=0x2E: FSCHG -> SZ=0
    // Test FSCA FPUL, DR12 with FPUL = 16384 (pi/2 -> sin=1, cos=0)
    PutOp(rom, p, 0xD407u); p += 2; // PC=0x30 -> (0x34)+0x1C = 0x50 (lit3: 16384)
    PutOp(rom, p, 0x445Au); p += 2; // PC=0x32: LDS R4, FPUL
    PutOp(rom, p, 0xFCFDu); p += 2; // PC=0x34: FSCA FPUL, DR12 -> FR12=1.0, FR13=0.0
    PutOp(rom, p, 0x0009u); p += 2; // PC=0x36: NOP

    Put32(rom, 0x44u, 0x88001000u);
    Put32(rom, 0x48u, 0x3FC90FDBu); // pi/2
    Put32(rom, 0x4Cu, FloatBits(3.5f));
    Put32(rom, 0x50u, 16384u);

    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    CHECK(core.StrictSh7305NoFpu(), "H-003: SH7305 strict no-FPU mode MUST be enabled by default");
    CHECK(core.GetHardwareProfile() == Cg50NativeCore::HardwareProfile::StrictSilicon,
          "H-004: StrictSilicon hardware profile MUST be default");
    CHECK(!core.ExamDecouplingEnabled(), "H-004: exam opcode patching MUST be OFF by default");
    // Explicitly opt into experimental SH-4A FPU mode solely for legacy sh_fpu.h tests
    core.SetStrictSh7305NoFpu(false);
    CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load FPU test ROM");
    for (int i = 0; i < 27; ++i) {
        CHECK(core.StepInstruction(), "step FPU instruction");
    }

    const auto& regs = core.GetRegisters();
    const float piHalf = BitsFloat(0x3FC90FDBu);
    CHECK(std::fabs(BitsFloat(regs.fr[2]) - piHalf) < 1e-6f, "FMOV.S 32-bit + FLDI1/0 + FMAC");
    CHECK(std::fabs(BitsFloat(regs.fr[0]) - (piHalf * piHalf + piHalf)) < 1e-5f,
          "FMAC 0xF00E (n=0,m=0) works and is not hijacked by FIPR");
    CHECK(std::fabs(BitsFloat(regs.fr[4]) - 42.0f) < 1e-6f, "FLOAT 0xF42D single");
    CHECK(std::fabs(BitsDouble(regs.fr[6], regs.fr[7]) - 42.0) < 1e-12, "FPCHG + FLOAT double");
    CHECK(std::fabs(BitsDouble(regs.fr[8], regs.fr[9]) - 3.5) < 1e-12, "FCNVSD 0xF8AD");
    CHECK(std::fabs(BitsDouble(regs.xf[2], regs.xf[3]) - 3.5) < 1e-12, "FMOV SZ=1 DR8 -> XD2");
    CHECK(std::fabs(BitsDouble(regs.fr[10], regs.fr[11]) - 3.5) < 1e-12,
          "FMOV SZ=1 XD2 -> RAM -> DR10 (full 64-bit)");
    CHECK(std::fabs(BitsFloat(regs.fr[12]) - 1.0f) < 1e-5f &&
          std::fabs(BitsFloat(regs.fr[13])) < 1e-5f, "FSCA 0xFCFD sin/cos(pi/2)");
}

void TestFtrvColumnMajorAndFipr()
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    // Load asymmetric column-major XMTRX into XF[0..15] by setting FR bank, FRCHG, then FTRV!
    // XMTRX columns:
    //   col0 (XF0..3)   = [1, 0, 0, 0]
    //   col1 (XF4..7)   = [2, 1, 0, 0]  <-- M[row=0, col=1] = 2 (asymmetric!)
    //   col2 (XF8..11)  = [3, 0, 1, 0]
    //   col3 (XF12..15) = [4, 0, 0, 1]
    // Vector FV0 = [10, 20, 30, 40]
    // Expected column-major M * v = [1*10 + 2*20 + 3*30 + 4*40, 20, 30, 40] = [300, 20, 30, 40]!
    // (Old transposed row-major bug would have produced [10, 40, 60, 80]!)
    std::size_t p = 0;
    PutOp(rom, p, 0xD110u); p += 2; // PC=0x00 -> lit at 0x44 (0xA0000100: 16 floats XMTRX)
    for (std::uint16_t i = 0; i < 16u; ++i) {
        PutOp(rom, p, static_cast<std::uint16_t>(0xF017u | (i << 8))); // FMOV.S @R1+, FRi
        p += 2;
    }
    PutOp(rom, p, 0xFBFDu); p += 2; // FRCHG: FR becomes XF (XMTRX)!
    for (std::uint16_t i = 0; i < 4u; ++i) {
        PutOp(rom, p, static_cast<std::uint16_t>(0xF017u | (i << 8))); // FMOV.S @R1+, FR0..3 (FV0)
        p += 2;
    }
    PutOp(rom, p, 0xF1FDu); p += 2; // FTRV XMTRX, FV0 (n=1 -> vn=0)
    // Also test FIPR FV0, FV0 (0xF0ED): dot([300,20,30,40], [300,20,30,40]) = 90000+400+900+1600 = 92900 -> FR3
    PutOp(rom, p, 0xF0EDu); p += 2;

    Put32(rom, 0x44u, 0xA0000100u);
    const float xmtrx[16] = {
        1, 0, 0, 0,  // col 0
        2, 1, 0, 0,  // col 1
        3, 0, 1, 0,  // col 2
        4, 0, 0, 1,  // col 3
    };
    const float vec[4] = {10, 20, 30, 40};
    for (int i = 0; i < 16; ++i) Put32(rom, 0x100u + i * 4u, FloatBits(xmtrx[i]));
    for (int i = 0; i < 4; ++i)  Put32(rom, 0x140u + i * 4u, FloatBits(vec[i]));

    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    core.SetStrictSh7305NoFpu(false);
    CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load FTRV/FIPR ROM");
    for (int i = 0; i < 23; ++i) CHECK(core.StepInstruction(), "step FTRV");
    const auto& rAfterFtrv = core.GetRegisters();
    // After FRCHG (FR=1), active FR bank is regs.xf!
    CHECK(std::fabs(BitsFloat(rAfterFtrv.xf[0]) - 300.0f) < 1e-4f, "FTRV col-major out[0]=300");
    CHECK(std::fabs(BitsFloat(rAfterFtrv.xf[1]) - 20.0f) < 1e-4f,  "FTRV col-major out[1]=20");
    CHECK(std::fabs(BitsFloat(rAfterFtrv.xf[2]) - 30.0f) < 1e-4f,  "FTRV col-major out[2]=30");
    CHECK(std::fabs(BitsFloat(rAfterFtrv.xf[3]) - 40.0f) < 1e-4f,  "FTRV col-major out[3]=40");

    CHECK(core.StepInstruction(), "step FIPR");
    CHECK(std::fabs(BitsFloat(core.GetRegisters().xf[3]) - 92900.0f) < 1e-1f, "FIPR 0xF0ED = 92900");
}

void TestSh4aIntegerAndControlOpcodes()
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    // Test MOVUA.L @Rm, R0 (0x4mA9) & @Rm+ (0x4mE9) on unaligned addr +1,
    // MOVLI.L (0x0m63) + MOVCO.L (0x0n73) success & fail, PREFI (0x0nD3),
    // JSR/N -> RTV/N (0x0m7B) and RTS/N (0x006B)!
    std::size_t p = 0;
    PutOp(rom, p, 0xD110u); p += 2; // PC=0x00 -> lit 0x44: 0xA0000101 (unaligned +1!)
    PutOp(rom, p, 0x41A9u); p += 2; // PC=0x02: MOVUA.L @R1, R0 -> 0x11223344
    PutOp(rom, p, 0x6203u); p += 2; // PC=0x04: MOV R0, R2
    PutOp(rom, p, 0x41E9u); p += 2; // PC=0x06: MOVUA.L @R1+, R0 -> R1 becomes 0xA0000105
    PutOp(rom, p, 0xD310u); p += 2; // PC=0x08 -> lit 0x4C: 0x88002000 (RAM aligned)
    PutOp(rom, p, 0x03D3u); p += 2; // PC=0x0A: PREFI @R3
    PutOp(rom, p, 0x2322u); p += 2; // PC=0x0C: MOV.L R2, @R3
    PutOp(rom, p, 0x0363u); p += 2; // PC=0x0E: MOVLI.L @R3, R0 (sets lockFlag, R0=0x11223344)
    PutOp(rom, p, 0x7001u); p += 2; // PC=0x10: ADD #1, R0 (0x11223345)
    PutOp(rom, p, 0x0373u); p += 2; // PC=0x12: MOVCO.L R0, @R3 (T=1, stores 0x11223345, clears lock)
    PutOp(rom, p, 0x0429u); p += 2; // PC=0x14: MOVT R4 (expect 1)
    PutOp(rom, p, 0x7001u); p += 2; // PC=0x16: ADD #1, R0 (0x11223346)
    PutOp(rom, p, 0x0373u); p += 2; // PC=0x18: MOVCO.L R0, @R3 WITHOUT MOVLI (T=0, must NOT store!)
    PutOp(rom, p, 0x0529u); p += 2; // PC=0x1A: MOVT R5 (expect 0)
    // Call subroutine at 0xA0000080 via JSR/N (0x464B) which returns R7=0x77 via RTV/N R7 (0x077B)
    PutOp(rom, p, 0xD60Cu); p += 2; // PC=0x1C -> (0x20)+0x30 = 0x50: 0xA0000080
    PutOp(rom, p, 0x464Bu); p += 2; // PC=0x1E: JSR/N @R6 (no delay slot, PR=0xA0000020)
    PutOp(rom, p, 0x6803u); p += 2; // PC=0x20: MOV R0, R8 (expect 0x77)
    // Call subroutine at 0xA0000090 via JSR/N returning via RTS/N (0x006B)
    PutOp(rom, p, 0xD60Bu); p += 2; // PC=0x22 -> (0x24)+0x2C = 0x50? Wait 0x24+0x30 = 0x54 -> disp 0x0C!
    rom[0x22] = 0xD6u; rom[0x23] = 0x0Cu; // PC=0x22 -> 0x24+0x30 = 0x54: 0xA0000090
    PutOp(rom, p, 0x464Bu); p += 2; // PC=0x24: JSR/N @R6
    PutOp(rom, p, 0xE955u); p += 2; // PC=0x26: MOV #0x55, R9

    // Lit pool
    Put32(rom, 0x44u, 0xA0000101u);
    Put32(rom, 0x4Cu, 0x88002000u);
    Put32(rom, 0x50u, 0xA0000080u);
    Put32(rom, 0x54u, 0xA0000090u);
    // Unaligned data at 0x101..0x104 = 0x11223344
    rom[0x100] = 0xAAu; rom[0x101] = 0x11u; rom[0x102] = 0x22u; rom[0x103] = 0x33u; rom[0x104] = 0x44u;
    // Sub 1 at 0x80: MOV #0x77, R7; RTV/N R7
    PutOp(rom, 0x80u, 0xE777u);
    PutOp(rom, 0x82u, 0x077Bu);
    // Sub 2 at 0x90: RTS/N
    PutOp(rom, 0x90u, 0x006Bu);

    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load int SH4A ROM");
    for (int i = 0; i < 23; ++i) CHECK(core.StepInstruction(), "step int SH4A");

    const auto& r = core.GetRegisters();
    CHECK(r.r[2] == 0x11223344u, "MOVUA.L @Rm unaligned +1 read");
    CHECK(r.r[1] == 0xA0000105u, "MOVUA.L @Rm+ post-increment");
    CHECK(r.r[4] == 1u, "MOVLI/MOVCO succeeded (T=1)");
    CHECK(r.r[5] == 0u, "MOVCO without MOVLI failed (T=0)");
    std::uint32_t ramVal = 0;
    CHECK(core.Read32(0x88002000u, ramVal) && ramVal == 0x11223345u,
          "failed MOVCO did not overwrite RAM");
    CHECK(r.r[8] == 0x77u, "RTV/N returned R0=0x77 without delay slot");
    CHECK(r.r[9] == 0x55u, "RTS/N returned cleanly without delay slot");
}

void TestClrmacVsLdtlbAndUtlbMultiHit()
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    // 1. Set VBR = 0xA0001000, set MMUCR.URC=0, program UTLB[0] via LDTLB (0x0038),
    //    then change PTEH/PTEL and execute CLRMAC (0x0028) in supervisor mode —
    //    verify CLRMAC clears MACH/MACL and does NOT overwrite UTLB[0]!
    // 2. Program UTLB[1] via LDTLB (0x0038) with the SAME VPN (0x00300000) and
    //    enable MMU (MMUCR.AT=1), then read 0x00300000 -> MUST trigger EXPEVT 0x140
    //    at VBR + 0x100 (0xA0001100)!
    std::size_t p = 0;
    PutOp(rom, p, 0xD110u); p += 2; // PC=0x00 -> lit 0x44: 0xA0001000 (VBR)
    PutOp(rom, p, 0x412Eu); p += 2; // PC=0x02: LDC R1, VBR
    // Clear SR.BL (bit 28) so exceptions vector to VBR+0x100 instead of resetting!
    PutOp(rom, p, 0xD110u); p += 2; // PC=0x04 -> lit 0x48: 0x40000000 (MD=1, BL=0)
    PutOp(rom, p, 0x410Eu); p += 2; // PC=0x06: LDC R1, SR
    // Put non-zero MACH/MACL
    PutOp(rom, p, 0xE255u); p += 2; // PC=0x08: MOV #0x55, R2
    PutOp(rom, p, 0x420Au); p += 2; // PC=0x0A: LDS R2, MACH
    PutOp(rom, p, 0x421Au); p += 2; // PC=0x0C: LDS R2, MACL
    // Write PTEH=0x00300000 (0xFF000000), PTEL=0x08000174 (PPN=0x08000, V=1, PR=3, SZ=4KB, D=1)
    PutOp(rom, p, 0xD30Fu); p += 2; // PC=0x0E -> (0x10)+0x3C = 0x4C: 0xFF000000 (PTEH)
    PutOp(rom, p, 0xD40Fu); p += 2; // PC=0x10 -> (0x14)+0x3C = 0x50: 0x00300000
    PutOp(rom, p, 0x2342u); p += 2; // PC=0x12: MOV.L R4, @R3 (PTEH)
    PutOp(rom, p, 0xD50Fu); p += 2; // PC=0x14 -> (0x18)+0x3C = 0x54: 0x08000174 (PTEL)
    PutOp(rom, p, 0x1351u); p += 2; // PC=0x16: MOV.L R5, @(4, R3) (PTEL = 0xFF000004)
    PutOp(rom, p, 0x0038u); p += 2; // PC=0x18: LDTLB -> UTLB[0] = VA 0x00300000
    // Change PTEH to 0x00400000 and run CLRMAC (0x0028) — MUST NOT touch UTLB[0]!
    PutOp(rom, p, 0xD60Fu); p += 2; // PC=0x1A -> (0x1C)+0x3C = 0x58: 0x00400000
    PutOp(rom, p, 0x2362u); p += 2; // PC=0x1C: MOV.L R6, @R3 (PTEH = 0x00400000)
    PutOp(rom, p, 0x0028u); p += 2; // PC=0x1E: CLRMAC (clears MACH/MACL only!)
    // Now restore PTEH=0x00300000, set MMUCR = (1<<10) (URC=1), LDTLB -> duplicate UTLB[1]!
    PutOp(rom, p, 0x2342u); p += 2; // PC=0x20: MOV.L R4, @R3 (PTEH = 0x00300000)
    PutOp(rom, p, 0xD70Eu); p += 2; // PC=0x22 -> (0x24)+0x38 = 0x5C: 0x00000400 (URC=1)
    PutOp(rom, p, 0x1374u); p += 2; // PC=0x24: MOV.L R7, @(16, R3) (MMUCR = 0xFF000010)
    PutOp(rom, p, 0x0038u); p += 2; // PC=0x26: LDTLB -> UTLB[1] = VA 0x00300000 (DUPLICATE!)
    // Enable MMU (MMUCR = 0x00000401)
    PutOp(rom, p, 0x7701u); p += 2; // PC=0x28: ADD #1, R7
    PutOp(rom, p, 0x1374u); p += 2; // PC=0x2A: MOV.L R7, @(16, R3) (MMUCR.AT = 1)
    // Access VA 0x00300000 -> MUST fault with EXPEVT 0x140 at 0xA0001100!
    PutOp(rom, p, 0x6842u); p += 2; // PC=0x2C: MOV.L @R4, R8

    Put32(rom, 0x44u, 0xA0001000u);
    Put32(rom, 0x48u, 0x40000000u);
    Put32(rom, 0x4Cu, 0xFF000000u);
    Put32(rom, 0x50u, 0x00300000u);
    Put32(rom, 0x54u, 0x08000174u);
    Put32(rom, 0x58u, 0x00400000u);
    Put32(rom, 0x5Cu, 0x00000400u);
    // Handler at VBR + 0x100 = 0xA0001100: NOP
    PutOp(rom, 0x1100u, 0x0009u);

    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load MMU test ROM");
    for (int i = 0; i < 16; ++i) CHECK(core.StepInstruction(), "step up to CLRMAC");
    CHECK(core.GetRegisters().mach == 0u && core.GetRegisters().macl == 0u,
          "CLRMAC cleared MACH/MACL");
    // Verify UTLB[0] still has VPN 0x00300000 (not 0x00400000!) via UTLB address array read
    std::uint32_t utlb0 = 0;
    CHECK(core.Read32(0xF6000000u, utlb0) && (utlb0 & 0xFFFFFC00u) == 0x00300000u,
          "supervisor CLRMAC did NOT corrupt UTLB[0]");

    for (int i = 0; i < 7; ++i) CHECK(core.StepInstruction(), "step to UTLB multi-hit");
    CHECK(core.GetRegisters().pc == 0xA0001100u, "UTLB multi-hit vectored to VBR+0x100");
    CHECK(core.GetRegisters().expevt == 0x140u,
          "UTLB multi-hit set EXPEVT=0x140 (literal Renesas REJ09B0003-0400)");
}

void TestRtcRealTimeEpoch()
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    auto corePtr = std::make_unique<Cg50NativeCore>();
    Cg50NativeCore& core = *corePtr;
    CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load RTC test ROM");

    // 2026-11-23 14:35:42 UTC = 1795444542
    // Verify days since 2000-01-01:
    // 2000..2025 = 26 years (leap years: 2000,04,08,12,16,20,24 = 7) => 26*365+7 = 9497 days
    // 2026 Jan..Oct = 31+28+31+30+31+30+31+31+30+31 = 304 days; Nov 23 -> +22 = 326 days
    // Total days = 9823; seconds = 9823*86400 + 14*3600 + 35*60 + 42 = 848759742
    // Unix epoch 2000-01-01 = 946684800 => 946684800 + 848759742 = 1795444542
    core.SetRtcEpochSeconds(1795444542LL);

    std::uint8_t sec = 0, min = 0, hr = 0, day = 0, mon = 0;
    std::uint16_t yr = 0;
    CHECK(core.Read8(0xA413FEC2u, sec) && sec == 0x42u, "RTC RSECCNT == 0x42");
    CHECK(core.Read8(0xA413FEC4u, min) && min == 0x35u, "RTC RMINCNT == 0x35");
    CHECK(core.Read8(0xA413FEC6u, hr)  && hr  == 0x14u, "RTC RHRCNT == 0x14");
    CHECK(core.Read8(0xA413FECAu, day) && day == 0x23u, "RTC RDAYCNT == 0x23");
    CHECK(core.Read8(0xA413FECCu, mon) && mon == 0x11u, "RTC RMONCNT == 0x11");
    CHECK(core.Read16(0xA413FECEu, yr) && yr  == 0x0026u, "RTC RYRCNT == 0x0026");
}

void TestDtShll2AndDelaySlotJmp()
{
    std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
    std::size_t p = 0;
    PutOp(rom, p, 0xD006u); p += 2; // PC=0x00 -> lit 0x1C: 0xA0001000 (VBR)
    PutOp(rom, p, 0x402Eu); p += 2; // PC=0x02: LDC R0, VBR
    PutOp(rom, p, 0xD006u); p += 2; // PC=0x04 -> lit 0x20: 0x40000000 (MD=1, BL=0)
    PutOp(rom, p, 0x400Eu); p += 2; // PC=0x06: LDC R0, SR
    PutOp(rom, p, 0xE801u); p += 2; // PC=0x08: MOV #1, R8 (unbanked)
    PutOp(rom, p, 0x4810u); p += 2; // PC=0x0A: DT R8 -> R8=0, T=1
    PutOp(rom, p, 0x0A29u); p += 2; // PC=0x0C: MOVT R10 -> R10=1
    PutOp(rom, p, 0xE903u); p += 2; // PC=0x0E: MOV #3, R9
    PutOp(rom, p, 0x4908u); p += 2; // PC=0x10: SHLL2 R9 -> R9=12
    PutOp(rom, p, 0xA000u); p += 2; // PC=0x12: BRA +0 (delay slot at 0x14!)
    PutOp(rom, p, 0x432Bu); p += 2; // PC=0x14: JMP @R3 inside delay slot -> EXPEVT 0x1A0!
    PutOp(rom, p, 0x0009u); p += 2; // PC=0x16: NOP
    PutOp(rom, p, 0x0009u); p += 2; // PC=0x18: NOP
    PutOp(rom, p, 0x0009u); p += 2; // PC=0x1A: NOP
    Put32(rom, 0x1Cu, 0xA0001000u);
    Put32(rom, 0x20u, 0x40000000u);

    auto corePtr = std::make_unique<Cg50NativeCore>();
    auto& core = *corePtr;
    CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load DT/SHLL2/delay-slot ROM");
    for (int i = 0; i < 11; ++i) CHECK(core.StepInstruction(), "step DT/SHLL2/delay-slot");
    const auto& r = core.GetRegisters();
    CHECK(r.r[8] == 0u && r.r[10] == 1u, "DT Rn decrements to 0 and sets T=1");
    CHECK(r.r[9] == 12u, "SHLL2 shifts left by exactly 2 bits (3 << 2 == 12)");
    CHECK(r.expevt == 0x1A0u && r.pc == 0xA0001100u, "JMP inside delay slot raises EXPEVT 0x1A0");
}

void TestSh4aIntegerAluMulDivAndJsrSlot()
{
    auto RunSeq = [](const std::vector<std::uint16_t>& ops,
                     const std::uint32_t initR1,
                     const std::uint32_t initR2,
                     const bool initT = false,
                     const bool setDsp = false) {
        std::vector<std::uint8_t> rom(Cg50NativeCore::kRomSize, 0x00u);
        for (std::size_t i = 0; i < ops.size(); ++i) PutOp(rom, i * 2u, ops[i]);
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        CHECK(core.LoadFirmwareParts(rom.data(), rom.size(), nullptr, 0), "load ALU/Mul/Div ROM");
        core.RegistersMut().vbr = 0xA0001000u;
        core.RegistersMut().sr = 0x40000000u | (initT ? 1u : 0u) | (setDsp ? 0x1000u : 0u);
        core.RegistersMut().r[1] = initR1;
        core.RegistersMut().r[2] = initR2;
        for (std::size_t i = 0; i < ops.size(); ++i) {
            (void)core.StepInstruction();
        }
        return core.GetRegisters();
    };

    // 1. JSR @R1 inside BRA delay slot -> EXPEVT 0x1A0 (MUT-011)
    {
        auto r = RunSeq({0xA000u, 0x410Bu}, 0xA0000200u, 0u);
        CHECK(r.expevt == 0x1A0u && r.pc == 0xA0001100u, "JSR inside delay slot raises EXPEVT 0x1A0");
    }
    // 2. CLIPS.B negative saturation (-200 -> -128 = 0xFFFFFF80, T=1) (MUT-014)
    {
        auto r = RunSeq({0x4191u}, static_cast<std::uint32_t>(-200), 0u, false, true);
        CHECK(r.r[1] == 0xFFFFFF80u && (r.sr & 1u) == 1u, "CLIPS.B -200 clamps to -128 (0xFFFFFF80)");
    }
    // 3. ADDV overflow & non-overflow (MUT-019)
    {
        auto rOvf = RunSeq({0x312Fu}, 0x7FFFFFFFu, 1u);
        CHECK(rOvf.r[1] == 0x80000000u && (rOvf.sr & 1u) == 1u, "ADDV 0x7FFFFFFF+1 sets T=1");
        auto rOk = RunSeq({0x312Fu}, 1u, 1u);
        CHECK(rOk.r[1] == 2u && (rOk.sr & 1u) == 0u, "ADDV 1+1 keeps T=0");
    }
    // 4. SUBV overflow & non-overflow (MUT-020)
    {
        auto rOvf = RunSeq({0x312Bu}, 0x80000000u, 1u);
        CHECK(rOvf.r[1] == 0x7FFFFFFFu && (rOvf.sr & 1u) == 1u, "SUBV 0x80000000-1 sets T=1");
        auto rOk = RunSeq({0x312Bu}, 5u, 3u);
        CHECK(rOk.r[1] == 2u && (rOk.sr & 1u) == 0u, "SUBV 5-3 keeps T=0");
    }
    // 5. SUBC equal boundary 5-5 with T=0 -> R1=0, T=0 (MUT-022)
    {
        auto r = RunSeq({0x312Au}, 5u, 5u, false);
        CHECK(r.r[1] == 0u && (r.sr & 1u) == 0u, "SUBC 5-5 (T=0) yields 0 with borrow T=0");
    }
    // 6. NEGC non-zero & zero (MUT-023)
    {
        auto r1 = RunSeq({0x612Au}, 0u, 5u, false);
        CHECK(r1.r[1] == static_cast<std::uint32_t>(-5) && (r1.sr & 1u) == 1u, "NEGC 5 sets T=1");
        auto r0 = RunSeq({0x612Au}, 0u, 0u, false);
        CHECK(r0.r[1] == 0u && (r0.sr & 1u) == 0u, "NEGC 0 keeps T=0");
    }
    // 7. SHAD -32 negative saturation & SHLD -32 zero (MUT-024, MUT-025)
    {
        auto rShad = RunSeq({0x412Cu}, 0x80000000u, static_cast<std::uint32_t>(-32));
        CHECK(rShad.r[1] == 0xFFFFFFFFu, "SHAD 0x80000000 by -32 yields 0xFFFFFFFF");
        auto rShld = RunSeq({0x412Du}, 0x80000000u, static_cast<std::uint32_t>(-32));
        CHECK(rShld.r[1] == 0x00000000u, "SHLD 0x80000000 by -32 yields 0x00000000");
    }
    // 8. ROTL, ROTR, ROTCL, ROTCR (MUT-026..029)
    {
        auto rL = RunSeq({0x4104u}, 0x80000000u, 0u);
        CHECK(rL.r[1] == 0x00000001u && (rL.sr & 1u) == 1u, "ROTL wraps bit 31 to bit 0");
        auto rR = RunSeq({0x4105u}, 0x00000001u, 0u);
        CHECK(rR.r[1] == 0x80000000u && (rR.sr & 1u) == 1u, "ROTR wraps bit 0 to bit 31");
        auto rCL = RunSeq({0x4124u}, 0x00000000u, 0u, true);
        CHECK(rCL.r[1] == 0x00000001u && (rCL.sr & 1u) == 0u, "ROTCL shifts in T=1 to bit 0");
        auto rCR = RunSeq({0x4125u}, 0x00000000u, 0u, true);
        CHECK(rCR.r[1] == 0x80000000u && (rCR.sr & 1u) == 0u, "ROTCR shifts in T=1 to bit 31");
    }
    // 9. CMP/STR MSB byte match & XTRCT (MUT-030, MUT-031)
    {
        auto rCmp = RunSeq({0x212Cu}, 0xAA112233u, 0xAA445566u);
        CHECK((rCmp.sr & 1u) == 1u, "CMP/STR detects matching MSB byte");
        auto rX = RunSeq({0x212Du}, 0x12345678u, 0x9ABCDEF0u);
        CHECK(rX.r[1] == 0xDEF01234u, "XTRCT combines Rm[15:0]<<16 | Rn[31:16]");
    }
    // 10. DIV0S + DIV1 (MUT-032..035)
    {
        // DIV0S R2, R1 with R1=-10 (Q=1), R2=+3 (M=0) -> Q=1, M=0, T=1
        auto rD0 = RunSeq({0x2127u}, static_cast<std::uint32_t>(-10), 3u);
        CHECK(((rD0.sr >> 8) & 1u) == 1u && ((rD0.sr >> 9) & 1u) == 0u && (rD0.sr & 1u) == 1u,
              "DIV0S sets Q=1, M=0, T=(Q!=M)=1");
        // DIV1 R2, R1 starting with DIV0U (Q=0,M=0,T=0) -> SETT (T=1) -> R1=0x40000000, R2=0x40000000:
        // shift-in T=1 -> 0x80000001; sub 0x40000000 -> 0x40000001 (no borrow -> Q=0 -> T=(Q==M)=1)
        auto rD1 = RunSeq({0x0019u, 0x0018u, 0x3124u}, 0x40000000u, 0x40000000u);
        CHECK(rD1.r[1] == 0x40000001u && (rD1.sr & 1u) == 1u && ((rD1.sr >> 8) & 1u) == 0u,
              "DIV1 shifts in oldT=1 and produces exact remainder + T=1");
    }
    // 11. MUL.L, MULU.W, MULS.W, DMULU.L (MUT-036..039)
    {
        auto rMul = RunSeq({0x0127u}, 7u, 9u);
        CHECK(rMul.macl == 63u, "MUL.L 7*9 == 63");
        auto rMulu = RunSeq({0x212Eu}, 0x0200u, 3u);
        CHECK(rMulu.macl == 0x0600u, "MULU.W preserves upper byte of 16-bit lhs");
        auto rMuls = RunSeq({0x212Fu}, 0xFFFEu, 3u);
        CHECK(rMuls.macl == 0xFFFFFFFAu, "MULS.W sign-extends 16-bit lhs (-2 * 3 == -6)");
        auto rDmulu = RunSeq({0x3125u}, 0x10000000u, 0x100u);
        CHECK(rDmulu.mach == 0x10u && rDmulu.macl == 0u, "DMULU.L computes 64-bit MACH:MACL");
    }
}

} // namespace

int main()
{
    TestFpuEncodingAndFmov32();
    TestFtrvColumnMajorAndFipr();
    TestSh4aIntegerAndControlOpcodes();
    TestClrmacVsLdtlbAndUtlbMultiHit();
    TestRtcRealTimeEpoch();
    TestDtShll2AndDelaySlotJmp();
    TestSh4aIntegerAluMulDivAndJsrSlot();

    std::printf("[SH4A-TEST] finished failures=%d\n", g_fails);
    return g_fails == 0 ? 0 : 1;
}
