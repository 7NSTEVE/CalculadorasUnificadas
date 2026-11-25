// cg50_native_exceptions_test.cpp — Contratos N-10 del core nativo SH7305:
// address error de escritura (0x100), fetch con PC impar (0x0E0), violacion
// de proteccion ITLB (0x060), TRAPA (0x160) y slot illegal (0x1A0).
// Referencia: PENDIENTES maestro N-10 + asiento #73.

#include "cg50_native_core.h"

#include <cstdint>
#include <cstdio>
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

struct TestRom {
    std::vector<std::uint8_t> bytes;
    std::uint32_t pc = 0;
    std::uint32_t poolCursor = 0x200;

    TestRom() : bytes(Cg50NativeCore::kRomSize, 0x00) {}

    void Op(const std::uint16_t opcode)
    {
        bytes[pc] = static_cast<std::uint8_t>(opcode >> 8);
        bytes[pc + 1] = static_cast<std::uint8_t>(opcode);
        pc += 2;
    }

    void Nop() { Op(0x0009); }
    void Sleep() { Op(0x001B); }

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

    // mov.l Rm,@Rn (n=base, m=dato)
    void MovLToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x2u));
    }

    // mov.w Rm,@Rn
    void MovWToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x1u));
    }

    // mov #imm8,Rm
    void MovImm(const std::uint32_t m, const std::uint8_t imm)
    {
        Op(static_cast<std::uint16_t>(0xE000u | (m << 8) | imm));
    }
};

void WriteHandlerRte(Cg50NativeCore& core, const std::uint32_t address)
{
    // rte + nop en RAM (manejador minimo que restaura SSR/SPC).
    (void)core.Write32(address, 0x002B0009u);
}

} // namespace

int main()
{
    // ------------------------------------------------------------------
    // A. Address error de ESCRITURA (0x100): mov.w a direccion impar.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);  // SR = MD|RB
        rom.Op(0x440E);                 // ldc r4,sr
        rom.MovLiteral(6, 0x08000000u);  // VBR = RAM
        rom.Op(0x462E);                 // ldc r6,vbr
        rom.MovLiteral(1, 0x08000001u);  // r1 = direccion IMPAR
        rom.MovImm(2, 1);
        rom.MovWToRn(1, 2);             // mov.w r2,@r1 -> 0x100
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000100u);
        core.RunInstructions(10);
        std::printf("[DBG-A] expevt=%X last=%X taken=%llu diag=%ls\n",
                    (unsigned)core.GetRegisters().expevt,
                    (unsigned)core.GetStats().lastExceptionEvent,
                    (unsigned long long)core.GetStats().exceptionsTaken,
                    core.Diagnostic().c_str());
        Check(core.GetStats().lastExceptionEvent == 0x100u,
              "EXC-A: escritura desalineada genera EXPEVT=0x100");
    }

    // ------------------------------------------------------------------
    // B. Fetch con PC impar (0x0E0): jmp @r4 con r4 impar.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(6, 0x08000000u);
        rom.Op(0x462E);
        rom.MovLiteral(4, 0x08000001u);  // destino impar
        rom.Op(0x442B);                 // jmp @r4 (con slot)
        rom.Nop();
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000100u);
        core.RunInstructions(12);
        std::printf("[DBG-B] expevt=%X last=%X pc=%08X diag=%ls\n",
                    (unsigned)core.GetRegisters().expevt,
                    (unsigned)core.GetStats().lastExceptionEvent,
                    core.GetRegisters().pc, core.Diagnostic().c_str());
        Check(core.GetStats().lastExceptionEvent == 0x0E0u,
              "EXC-B: fetch con PC impar genera EXPEVT=0x0E0 (no halt)");
        Check(core.GetState() != Cg50NativeCore::State::Halted,
              "EXC-B: el core no queda Halted");
    }

    // ------------------------------------------------------------------
    // C. Violacion de proteccion ITLB (0x060): fetch a pagina PR=0.
    // Con MMUCR.AT=1 los handlers deben estar en P1 (directo, sin MMU):
    // VBR=0x88000000 (alias P1 de la RAM) y RTE en 0x88000400 (vector TLB).
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(6, 0x88000000u);  // VBR en P1 (directo)
        rom.Op(0x462E);
        rom.MovLiteral(4, 0x10000000u);  // PTEH: VPN 0x10000000
        rom.MovLiteral(7, 0xFF000000u);
        rom.MovLToRn(7, 4);
        rom.MovLiteral(4, 0x08000041u);  // PTEL: PR=0, 4KB, V=1
        rom.MovLiteral(7, 0xFF000004u);
        rom.MovLToRn(7, 4);
        rom.Op(0x0028);                  // ldtlb
        rom.MovLiteral(7, 0xFF000010u);
        rom.MovImm(5, 1);
        rom.MovLToRn(7, 5);              // MMUCR.AT=1
        rom.MovLiteral(4, 0x10000000u);  // VA mapeada con PR=0
        rom.Op(0x442B);                  // jmp @r4 -> fetch viola proteccion
        rom.Nop();
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x88000100u);  // vector general
        WriteHandlerRte(core, 0x88000400u);  // vector TLB
        core.RunInstructions(40);
        std::printf("[DBG-C] expevt=%X last=%X taken=%llu pc=%08X diag=%ls\n",
                    (unsigned)core.GetRegisters().expevt,
                    (unsigned)core.GetStats().lastExceptionEvent,
                    (unsigned long long)core.GetStats().exceptionsTaken,
                    core.GetRegisters().pc, core.Diagnostic().c_str());
        Check(core.GetStats().lastExceptionEvent == 0x060u,
              "EXC-C: fetch a pagina PR=0 genera EXPEVT=0x060 (ITLB)");
    }

    // ------------------------------------------------------------------
    // D. TRAPA (0x160): trap #0x2A.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(6, 0x08000000u);
        rom.Op(0x462E);
        rom.Op(0xC32A);                 // trapa #0x2A
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000100u);
        core.RunInstructions(10);
        std::printf("[DBG-D] expevt=%X last=%X tra=%X diag=%ls\n",
                    (unsigned)core.GetRegisters().expevt,
                    (unsigned)core.GetStats().lastExceptionEvent,
                    (unsigned)core.GetRegisters().tra,
                    core.Diagnostic().c_str());
        Check(core.GetStats().lastExceptionEvent == 0x160u,
              "EXC-D: trapa genera EXPEVT=0x160");
    }

    // ------------------------------------------------------------------
    // E. Slot illegal (0x1A0): jmp dentro del slot de retardo de bra.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(6, 0x08000000u);
        rom.Op(0x462E);
        rom.Op(0xA000);                 // bra +0 (slot siguiente)
        rom.Op(0x442B);                 // jmp @r4 en el delay slot
        rom.Nop();
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000100u);
        core.RunInstructions(12);
        std::printf("[DBG-E] expevt=%X last=%X taken=%llu diag=%ls\n",
                    (unsigned)core.GetRegisters().expevt,
                    (unsigned)core.GetStats().lastExceptionEvent,
                    (unsigned long long)core.GetStats().exceptionsTaken,
                    core.Diagnostic().c_str());
        Check(core.GetStats().lastExceptionEvent == 0x1A0u,
              "EXC-E: branch en delay slot genera EXPEVT=0x1A0");
    }

    if (g_failures != 0) {
        std::printf("RESULTADO: %d fallos\n", g_failures);
        return 1;
    }
    std::printf("RESULTADO: EXCEPCIONES OK (5 grupos)\n");
    return 0;
}
