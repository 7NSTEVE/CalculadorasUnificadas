// cg50_native_fpu_test.cpp — Contratos unitarios de la FPU SH-4A del core
// nativo (banco FR/XF, FPUL/FPSCR, grupo 1111, LDS/STS, FPU-disabled).
// Parte del P1 «FPU del SH-4A» del INFORME-REQUERIMIENTOS (asiento #43).
//
// Codificaciones verificadas contra la tabla FXXX del manual SH-4:
//   FADD/FSUB/FMUL/FDIV/FCMP: 1111 nnnn mmmm 0000..0101 (n=destino, m=fuente)
//   FMOV FRm,FRn: 1111 nnnn mmmm 1100
//   Grupo m: FSTS(0x0n5D? no: 1111 nnnn mmmm 1101 con m como selector)
//     FLDI0 FRn = 0xFnFD, FLDI1 FRn = 0xFnED, FABS = 0xFn5D, FNEG = 0xFn4D,
//     FSQRT = 0xFn6D, FSRRA = 0xFn7D, FCNVSD = 0xFn8D, FCNVDS = 0xFn9D
//   FLOAT FPUL,FRn = 0xFn2F, FTRC FRn,FPUL = 0xFn3F, FSCA = 0xFnBF,
//   FTRV XMTRX,FVn = 0xFnEF, FIPR = 1111 nnnn mmmm 1110, FMAC = igual con
//   n,m no multiplos de 4, FRCHG = 0xF3FD, FSCHG = 0xF3ED
//   LDS Rm,FPUL = 0x4m5A, LDS Rm,FPSCR = 0x4m6A, STS FPUL,Rn = 0x0n5A,
//   STS FPSCR,Rn = 0x0n6A, LDC Rm,SR = 0x4m0E.

#include "cg50_native_core.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_failures = 0;

void Check(const bool condition, const char* what)
{
    if (!condition) {
        std::printf("FALLO: %s\n", what);
        ++g_failures;
    } else {
        std::printf("OK: %s\n", what);
    }
}

// Synthetic firmware builder: opcodes at 0xA0000000 + literal pool at +0x1000.
struct TestRom {
    std::vector<std::uint8_t> bytes;
    std::uint32_t pc = 0;          // ROM offset == VA - 0xA0000000
    std::uint32_t poolCursor = 0x200;

    TestRom() : bytes(Cg50NativeCore::kRomSize, 0x00) {}

    void Op(const std::uint16_t opcode)
    {
        bytes[pc] = static_cast<std::uint8_t>(opcode >> 8);
        bytes[pc + 1] = static_cast<std::uint8_t>(opcode);
        pc += 2;
    }

    void Nop() { Op(0x0009); }

    std::uint32_t PlaceLiteral(const std::uint32_t value)
    {
        poolCursor = (poolCursor + 3u) & ~3u;
        bytes[poolCursor] = static_cast<std::uint8_t>(value >> 24);
        bytes[poolCursor + 1] = static_cast<std::uint8_t>(value >> 16);
        bytes[poolCursor + 2] = static_cast<std::uint8_t>(value >> 8);
        bytes[poolCursor + 3] = static_cast<std::uint8_t>(value);
        const std::uint32_t address = 0xA0000000u + poolCursor;
        poolCursor += 4;
        return address;
    }

    void MovLiteral(const std::uint32_t reg, const std::uint32_t value)
    {
        while ((pc & 3u) != 0u) Nop();
        const std::uint32_t lit = PlaceLiteral(value);
        const std::uint32_t disp = (lit - (0xA0000000u + pc + 4u)) / 4u;
        Op(static_cast<std::uint16_t>(0xD000u | (reg << 8) | disp));
    }

    void Sleep() { Op(0x001B); }
};

std::uint32_t FrBits(const Cg50NativeCore& core, const std::uint32_t index)
{
    return core.GetRegisters().fr[index];
}

} // namespace

int main()
{
    // ------------------------------------------------------------------
    // 1. FLDI0/FLDI1 + FADD + FMUL + FDIV + FCMP/EQ (simple).
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.Op(0xF5FD);   // fldi0 fr5
        rom.Op(0xF7ED);   // fldi1 fr7
        rom.Op(0xF570);   // fadd fr7,fr5 -> fr5 = 1.0f
        rom.Op(0xF572);   // fmul fr7,fr5 -> fr5 = 1.0f
        rom.Op(0xF573);   // fdiv fr7,fr5 -> fr5 = 1.0f
        rom.Op(0xF574);   // fcmp/eq fr7,fr5 -> T=1
        rom.Sleep();

        Cg50NativeCore core;
        Check(core.LoadFirmware(rom.bytes.data(), rom.bytes.size()),
              "FPU-1: firmware sintetico cargado");
        core.RunInstructions(20);
        Check(FrBits(core, 5) == 0x3F800000u, "FPU-1: fr5 == 1.0f exacto");
        Check((core.GetRegisters().sr & 1u) != 0u, "FPU-1: FCMP/EQ dejo T=1");
    }

    // ------------------------------------------------------------------
    // 2. LDS/STS FPUL + FLOAT + FRCHG/FSCHG.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 100u);  // int32 100
        rom.Op(0x445A);           // lds r4,fpul
        rom.Op(0xF42F);           // float fpul,fr4 -> 100.0f
        rom.Op(0xF3FD);           // frchg
        rom.Op(0xF3ED);           // fschg
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(20);
        Check(FrBits(core, 4) == 0x42C80000u,
              "FPU-2: FLOAT FPUL,FR4 == 100.0f");
        Check((core.GetRegisters().fpscr & Cg50NativeCore::kFpscrFr) != 0u,
              "FPU-2: FRCHG dejo FPSCR.FR=1");
        Check((core.GetRegisters().fpscr & Cg50NativeCore::kFpscrSz) != 0u,
              "FPU-2: FSCHG dejo FPSCR.SZ=1");
    }

    // ------------------------------------------------------------------
    // 3. Doble precision (PR=1): FCNVDS 1.0f -> dr4 y FADD dr0,dr4.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x3F800000u);   // 1.0f
        rom.Op(0x445A);                   // lds r4,fpul
        rom.MovLiteral(6, Cg50NativeCore::kFpscrPr);  // PR=1
        rom.Op(0x466A);                   // lds r6,fpscr
        rom.Op(0xF49D);                   // fcnvds fpul,dr4 -> dr4 = 1.0
        rom.Op(0xF400);                   // fadd dr0,dr4 (n=4,m=0) -> 1.0+0
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(20);
        const auto& regs = core.GetRegisters();
        Check(regs.fr[4] == 0x3FF00000u && regs.fr[5] == 0x00000000u,
              "FPU-3: FCNVDS + FADD doble (PR=1) dr4 == 1.0 exacto");
        Check((regs.fpscr & Cg50NativeCore::kFpscrPr) != 0u,
              "FPU-3: PR persiste en FPSCR");
    }

    // ------------------------------------------------------------------
    // 4. FTRC (trunc hacia cero) + STS FPUL + FLOAT.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x422B3333u);  // bits de 42.7f
        rom.Op(0x445A);                 // lds r4,fpul
        rom.Op(0xF40D);                 // fsts fpul,fr4 (copia RAW de bits)
        rom.Op(0xF43F);                 // ftrc fr4,fpul -> 42
        rom.Op(0x055A);                 // sts fpul,r5
        rom.Op(0xF42F);                 // float fpul,fr4 -> 42.0f
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(20);
        const auto& regs = core.GetRegisters();
        Check(regs.fpul == 42u, "FPU-4: FTRC 42.7 -> FPUL=42 (trunc)");
        Check(regs.r[5] == 42u, "FPU-4: STS FPUL,R5 == 42");
        Check(regs.fr[4] == 0x42280000u, "FPU-4: FLOAT 42 -> 42.0f exacto");
    }

    // ------------------------------------------------------------------
    // 5. FSCA(0): sin/cos, y FSQRT.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.Op(0xF4FD);   // fldi0 fr4 (fpul=0.0f por reset)
        rom.Op(0x445A);   // lds r4,fpul (0.0f)
        rom.Op(0xF4BF);   // fsca dr4 -> fr4=sin(0)=+0, fr5=cos(0)=1.0f
        rom.Op(0xF66D);   // fsqrt fr6 (0.0 -> 0.0)
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(20);
        Check(FrBits(core, 4) == 0x00000000u, "FPU-5: FSCA sin(0)=+0");
        Check(FrBits(core, 5) == 0x3F800000u, "FPU-5: FSCA cos(0)=1.0f");
        Check(FrBits(core, 6) == 0x00000000u, "FPU-5: FSQRT(0)=0");
    }

    // ------------------------------------------------------------------
    // 6. FIPR fv4·fv8 (resultado en FR7).
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.Op(0xF4ED);   // fldi1 fr4
        rom.Op(0xF5ED);   // fldi1 fr5
        rom.Op(0xF6ED);   // fldi1 fr6
        rom.Op(0xF7ED);   // fldi1 fr7
        // fv8 (fr8..fr11) queda en 0.0 -> producto punto 0 -> FR7 = 0.
        rom.Op(0xF48E);   // fipr fv4,fv8 (n=4, m=8)
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(20);
        Check(FrBits(core, 7) == 0x00000000u,
              "FPU-6: FIPR fv4.fv8 = 0 (fv8 en cero)");
    }

    // ------------------------------------------------------------------
    // 7. FDIV por cero: +Inf + causa/bandera U/Z visibles por STS FPSCR.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.Op(0xF5ED);   // fldi1 fr5 (dividendo 1.0f)
        rom.Op(0xF7FD);   // fldi0 fr7 (divisor 0.0f)
        rom.Op(0xF573);   // fdiv fr7,fr5 -> 1/0 = +Inf
        rom.Op(0x056A);   // sts fpscr,r5
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(20);
        const auto& regs = core.GetRegisters();
        Check(regs.fr[5] == 0x7F800000u, "FPU-7: 1/0 -> +Inf (0x7F800000)");
        Check((regs.r[5] & 0x00004000u) != 0u,
              "FPU-7: causa U/Z en FPSCR tras STS");
        Check((regs.r[5] & 0x00020000u) != 0u,
              "FPU-7: bandera U/Z acumulada en FPSCR");
    }

    // ------------------------------------------------------------------
    // 8. SR.FD=1: cualquier opcode F dispara EXPEVT 0x120.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x40008000u);  // SR = MD|FD
        rom.Op(0x440E);   // ldc r4,sr
        rom.Op(0xF5FD);   // fldi0 fr5 -> excepcion FPU-disabled
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(4);
        Check(core.GetRegisters().expevt == 0x120u,
              "FPU-8: EXPEVT=0x120 (FPU disabled) con SR.FD=1");
        Check(core.GetRegisters().spc != 0,
              "FPU-8: SPC capturado por la excepcion");
    }

    if (g_failures != 0) {
        std::printf("RESULTADO: %d fallos\n", g_failures);
        return 1;
    }
    std::printf("RESULTADO: FPU OK (8 grupos)\n");
    return 0;
}
