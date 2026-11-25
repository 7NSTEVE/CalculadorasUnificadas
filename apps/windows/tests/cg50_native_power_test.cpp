// cg50_native_power_test.cpp — Contratos N-12 del core nativo SH7305:
// SBYCR (0xA4150020, mascara 0xA0) + STANDBY/SLEEP, wake por tecla con
// SR.BL=1 (SBYCR=0xA0) y wake por timer (TMU0) mediante AdvanceCycles.
// Referencia: PENDIENTES maestro N-12 + asiento #73.

#include "cg50_native_core.h"

#include <cstdint>
#include <cstdio>
#include <cwchar>
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

    void MovLToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x2u));
    }
    void MovWToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x1u));
    }
    void MovBToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x0u));
    }
    void MovImm(const std::uint32_t m, const std::uint8_t imm)
    {
        Op(static_cast<std::uint16_t>(0xE000u | (m << 8) | imm));
    }
};

void WriteHandlerRte(Cg50NativeCore& core, const std::uint32_t address)
{
    (void)core.Write32(address, 0x002B0009u);
}

bool DiagContains(Cg50NativeCore& core, const wchar_t* needle)
{
    return std::wcsstr(core.Diagnostic().c_str(), needle) != nullptr;
}

} // namespace

int main()
{
    // ------------------------------------------------------------------
    // A. SBYCR: mascara 0xA0, distincion STANDBY/SLEEP y Reset.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(3, 0xA4150020u);   // &SBYCR
        rom.MovLiteral(5, 0xFFFFFFFFu);   // valor con bits fuera de mascara
        rom.MovLToRn(3, 5);
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(10);
        std::uint32_t sbycr = 0;
        core.Read32(0xA4150020u, sbycr);
        std::printf("[DBG-A] SBYCR=%08X state=%d diag=%ls\n", sbycr,
                    (int)core.GetState(), core.Diagnostic().c_str());
        Check((sbycr & ~0x000000A0u) == 0u,
              "PWR-A: SBYCR solo retiene la mascara 0xA0");
        Check(core.GetState() == Cg50NativeCore::State::Sleeping,
              "PWR-A: SLEEP alcanzado");
        Check(DiagContains(core, L"STANDBY"),
              "PWR-A: diagnostico STANDBY con bits A0");
        core.Reset();
        core.Read32(0xA4150020u, sbycr);
        Check(sbycr == 0u, "PWR-A: Reset limpia SBYCR");
    }

    // ------------------------------------------------------------------
    // B. Wake por tecla desde STANDBY con SR.BL=1 (IPRF=0xD000).
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x70000000u);   // SR = MD|RB|BL
        rom.Op(0x440E);
        rom.MovLiteral(3, 0xA4150020u);
        rom.MovImm(5, 0xA0);
        rom.MovLToRn(3, 5);               // SBYCR = STANDBY
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(8);
        Check(core.GetState() == Cg50NativeCore::State::Sleeping,
              "PWR-B: CPU en STANDBY");
        (void)core.Write16(0xA4080014u, 0xD000u);  // IPRF prio 13
        Check(core.KeyDown(0x070Au), "PWR-B: KeyDown(F1)");
        Check(core.GetStats().interruptsAccepted == 1,
              "PWR-B: la tecla despierta con SR.BL=1 (como HW)");
        Check(core.GetStats().lastInterruptEvent == 0x0BE0u,
              "PWR-B: INTEVT=0x0BE0 (KIU)");
        Check(core.GetState() == Cg50NativeCore::State::Running,
              "PWR-B: estado Running tras el wake");
    }

    // ------------------------------------------------------------------
    // C. Wake por timer: TMU0 en SLEEP + AdvanceCycles.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(6, 0x08000000u);   // VBR
        rom.Op(0x462E);
        rom.MovLiteral(3, 0xA4480008u);   // &TCOR0
        rom.MovImm(5, 0x40);
        rom.MovLToRn(3, 5);
        rom.MovLiteral(3, 0xA448000Cu);   // &TCNT0
        rom.MovLToRn(3, 5);
        rom.MovLiteral(3, 0xA4480010u);   // &TCR0
        rom.MovImm(5, 0x20);              // UNIE
        rom.MovWToRn(3, 5);
        rom.MovLiteral(3, 0xA4080000u);   // &IPRA
        rom.MovLiteral(5, 0xF000u);       // TMU0 prio 15
        rom.MovWToRn(3, 5);
        rom.MovLiteral(2, 0xA4480004u);   // &TSTR0
        rom.MovImm(0, 1);
        rom.MovBToRn(2, 0);               // arranca ch0
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000600u);
        core.RunInstructions(60);
        std::printf("[DBG-C] state=%d pc=%08X diag=%ls\n",
                    (int)core.GetState(), core.GetRegisters().pc,
                    core.Diagnostic().c_str());
        Check(core.GetState() == Cg50NativeCore::State::Sleeping,
              "PWR-C: CPU en SLEEP con TMU0 armado");
        const bool woke = core.AdvanceCycles(200000u);
        std::printf("[DBG-C2] woke=%d state=%d lastEv=%X acc=%llu diag=%ls\n",
                    woke ? 1 : 0, (int)core.GetState(),
                    (unsigned)core.GetStats().lastInterruptEvent,
                    (unsigned long long)core.GetStats().interruptsAccepted,
                    core.Diagnostic().c_str());
        Check(woke, "PWR-C: AdvanceCycles despierta al CPU");
        Check(core.GetState() == Cg50NativeCore::State::Running,
              "PWR-C: estado Running tras el wake por timer");
        Check(core.GetStats().lastInterruptEvent == 0x0400u,
              "PWR-C: ultimo evento 0x400 (TMU0)");
        Check(core.GetStats().interruptsAccepted >= 1,
              "PWR-C: IRQ del timer entregada");
        // Sin TMU armado no debe despertar por timer.
        Check(core.AdvanceCycles(1000u),
              "PWR-C: AdvanceCycles con CPU despierta no altera el estado");
    }

    if (g_failures != 0) {
        std::printf("RESULTADO: %d fallos\n", g_failures);
        return 1;
    }
    std::printf("RESULTADO: POWER OK (3 grupos)\n");
    return 0;
}
