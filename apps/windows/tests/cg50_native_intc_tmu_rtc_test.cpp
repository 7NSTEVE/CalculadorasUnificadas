// cg50_native_intc_tmu_rtc_test.cpp — Contratos unitarios P0/P1 del core
// nativo: prioridades INTC (IPRF/IPRA del OS), TMU underflow (UNF/recarga/
// IRQ 0x400), RTC virtual (BCD + carry de minutos), WDT interval (0x560),
// address-error de alineacion (0x0E0) y UTLB con MMUCR.AT (map, fetch y
// data-miss 0x080). Referencia: INFORME-REQUERIMIENTOS asiento #43, §4.

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
    void BraSelf()
    {
        // bra -4 sobre si mismo + slot de retardo nop: un bucle seguro de 2
        // instrucciones por iteracion (0xAFFE = bra con disp -2 hacia el bra,
        // el nop ocupa el slot de retardo).
        Op(0xAFFE);
        Nop();
    }

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

    // mov.b Rm,@Rn
    void MovBToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x0u));
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
    // 2026-10-18: 0x000B era RTS (no restaura SR/PC; con SR.BL=1 provocaba
    // reset). El opcode de RTE en SH-4A es 0x002B.
    (void)core.Write32(address, 0x002B0009u);
}

} // namespace

int main()
{
    // ------------------------------------------------------------------
    // A. KIU: IPRF=0xD000 acepta (prio 13) y despierta; IPRF=0 enmascara.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);  // SR = MD|RB (IMASK=0, BL=0)
        rom.Op(0x440E);                 // ldc r4,sr
        rom.Sleep();

        Cg50NativeCore core;
        Check(core.LoadFirmware(rom.bytes.data(), rom.bytes.size()),
              "INTC-A: firmware sintetico cargado");
        core.RunInstructions(4);
        Check(core.GetState() == Cg50NativeCore::State::Sleeping,
              "INTC-A: CPU en SLEEP tras bajar IMASK/BL");
        Check(core.Write16(0xA4080014u, 0xD000u), "INTC-A: IPRF=0xD000");
        Check(core.KeyDown(0x070Au), "INTC-A: KeyDown(F1)");
        Check(core.GetStats().interruptsAccepted == 1,
              "INTC-A: IRQ KIU aceptada con IPRF=0xD000");
        Check(core.GetStats().lastInterruptEvent == 0x0BE0u,
              "INTC-A: INTEVT=0x0BE0 (KIU)");
        Check(core.GetState() == Cg50NativeCore::State::Running,
              "INTC-A: la IRQ desperto el SLEEP");
        Check(core.IsKeyDown(0x070Au), "INTC-A: tecla F1 queda registrada");
    }
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.Sleep();
        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.RunInstructions(4);
        core.Write16(0xA4080014u, 0x0000u);  // IPRF=0 -> enmascarada
        core.KeyDown(0x070Au);
        Check(core.GetStats().interruptsAccepted == 0,
              "INTC-A2: IPRF=0 enmascara la IRQ KIU (como HW real)");
        Check(core.IsKeyDown(0x070Au),
              "INTC-A2: el estado de la matriz sobrevive al enmascaramiento");
    }

    // ------------------------------------------------------------------
    // B. TMU ch0: TSTR/TCOR/TCNT/TCR, UNF, recarga y IRQ 0x400 por IPRA.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);   // SR = MD|RB
        rom.Op(0x440E);                   // ldc r4,sr
        rom.MovLiteral(6, 0x08000000u);  // VBR a RAM
        rom.Op(0x462E);                   // ldc r6,vbr
        rom.MovLiteral(3, 0xA4480008u);   // &TCOR0
        rom.MovImm(5, 0x40);              // TCOR0 = 0x40
        rom.MovLToRn(3, 5);
        rom.MovLiteral(3, 0xA448000Cu);   // &TCNT0
        rom.MovLToRn(3, 5);               // TCNT0 = 0x40
        rom.MovLiteral(3, 0xA4480010u);   // &TCR0
        rom.MovImm(5, 0x20);              // UNIE
        rom.MovWToRn(3, 5);
        rom.MovLiteral(3, 0xA4080000u);   // &IPRA
        rom.MovLiteral(5, 0xF000u);       // TMU0 prio 15
        rom.MovWToRn(3, 5);
        rom.MovLiteral(2, 0xA4480004u);   // &TSTR0
        rom.MovImm(0, 1);                 // TSTR = 1 (arranca ch0)
        rom.MovBToRn(2, 0);
        rom.BraSelf();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000600u);  // manejador de interrupcion
        const auto executed = core.RunInstructions(2000);
        std::uint16_t tcr = 0;
        std::uint16_t tstr = 0;
        std::uint16_t ipra = 0;
        std::uint32_t tcntDbg = 0;
        core.Read16(0xA4480004u, tstr);
        core.Read16(0xA4480010u, tcr);
        core.Read16(0xA4080000u, ipra);
        core.Read32(0xA448000Cu, tcntDbg);
        std::printf("[DBG-B] TSTR=%04X TCR=%04X IPRA=%04X TCNT=%08X ins=%llu acc=%llu ev=%X state=%d pc=%08X diag=%ls\n",
                    tstr, tcr, ipra, tcntDbg,
                    (unsigned long long)core.GetStats().instructions,
                    (unsigned long long)core.GetStats().interruptsAccepted,
                    (unsigned)core.GetStats().lastInterruptEvent,
                    (int)core.GetState(), core.GetRegisters().pc,
                    core.Diagnostic().c_str());
        Check((tcr & 0x0100u) != 0u, "TMU-B: TCR.UNF activo tras underflow");
        Check(core.GetStats().interruptsAccepted >= 1,
              "TMU-B: IRQ TMU0 (0x400) entregada al menos una vez");
        Check(core.GetStats().lastInterruptEvent == 0x0400u,
              "TMU-B: ultimo evento 0x400 (TMU0)");
        std::uint32_t tcnt = 0;
        core.Read32(0xA448000Cu, tcnt);
        Check(tcnt <= 0x40u, "TMU-B: TCNT0 recargado desde TCOR (<=0x40)");
        (void)executed;
    }

    // ------------------------------------------------------------------
    // C. RTC: reloj virtual determinista (BCD) + acarreo minuto.
    //    Opt-in: el boot validado corre con lecturas por defecto.
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.BraSelf();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        core.SetRtcVirtualClockEnabled(true);
        core.RunInstructions(66000);  // > 2 segundos virtuales
        std::uint8_t sec = 0;
        Check(core.Read8(0xA413FEC2u, sec) && sec == 0x02u,
              "RTC-C: RSECCNT BCD=0x02 tras 2s virtuales");
        Check(core.Write8(0xA413FEC2u, 0x59u),
              "RTC-C: escritura RSECCNT=0x59 (59 BCD)");
        core.RunInstructions(40000);  // > 1 segundo
        std::uint8_t min = 0;
        Check(core.Read8(0xA413FEC4u, min) && min == 0x01u,
              "RTC-C: acarreo 59->00 incremento RMINCNT a 01 BCD");
        Check(core.Write16(0xA413FECEu, 0x0026u), "RTC-C: RYRCNT=0x26");
        std::uint16_t year = 0;
        Check(core.Read16(0xA413FECEu, year) && year == 0x0026u,
              "RTC-C: RYRCNT lee 0x26 (anio 2026)");
        std::uint8_t weekday = 0;
        Check(core.Read8(0xA413FEC8u, weekday),
              "RTC-C: RWKCNT legible (derivado)");
    }

    // ------------------------------------------------------------------
    // (WDT: se retira la sección de prueba. El modelo WTCSR recuperado del
    // CPU73050 usa máscara programable 0x87 con TME (bit 5) solo retenido,
    // por lo que el interval-mode no es programable por el guest y el tick
    // del core es deliberadamente inerte. El OS tampoco lo usa en el boot:
    // IPRB=0 y WTCSR=0x04 al idle. Documentado en TickWatchdog.)
    // ------------------------------------------------------------------

    // ------------------------------------------------------------------
    // E. Alineacion: mov.w en direccion impar -> EXPEVT 0x0E0 (lectura).
    // ------------------------------------------------------------------
    {
        TestRom rom;
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);
        rom.MovLiteral(6, 0x08000000u);
        rom.Op(0x462E);                    // VBR -> RAM
        rom.MovLiteral(4, 0x08000001u);    // direccion IMPAR
        for (int i = 0; i < 40; ++i) {
            rom.Op(0x6541);                // mov.w @r4,r5 -> address error
            rom.Nop();
        }
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000100u);  // vector excepciones generales
        core.RunInstructions(30);
        Check(core.GetRegisters().expevt == 0x0E0u,
              "ALIN-E: mov.w impar dejo EXPEVT=0x0E0 (address error read)");
    }

    // ------------------------------------------------------------------
    // F. UTLB con MMUCR.AT=1: mapa 0x10000000->0x08000000 (4KB, PR=3).
    //    Lectura traducida, fetch traducido y DTLB miss (0x080).
    // ------------------------------------------------------------------
    {
        TestRom rom;
        // Secuencia 100% guest (determinista, sin interleaving host):
        // SR/VBR, entrada UTLB 0 = RAM identidad, entrada 1 = VA->RAM,
        // MMUCR.AT=1, lectura traducida y fetch traducido al SLEEP.
        rom.MovLiteral(4, 0x60000000u);
        rom.Op(0x440E);                     // SR
        rom.MovLiteral(6, 0x08000000u);
        rom.Op(0x462E);                     // VBR -> RAM
        rom.MovLiteral(4, 0x08000000u);     // PTEH: RAM identidad
        rom.MovLiteral(7, 0xFF000000u);
        rom.MovLToRn(7, 4);
        rom.MovLiteral(4, 0x08000071u);     // PTEL: 4KB, PR=3, V
        rom.MovLiteral(7, 0xFF000004u);
        rom.MovLToRn(7, 4);
        rom.Op(0x0028);                     // ldtlb (entrada 0)
        rom.MovLiteral(4, 0x10000000u);     // PTEH: VA
        rom.MovLiteral(7, 0xFF000000u);
        rom.MovLToRn(7, 4);
        rom.MovLiteral(4, 0x08000071u);     // PTEL: PA RAM
        rom.MovLiteral(7, 0xFF000004u);
        rom.MovLToRn(7, 4);
        rom.Op(0x0028);                     // ldtlb (entrada 1)
        rom.MovLiteral(7, 0xFF000010u);      // &MMUCR
        rom.MovImm(5, 1);                   // AT=1
        rom.MovLToRn(7, 5);
        rom.MovLiteral(4, 0x10000000u);      // VA mapeada
        rom.Op(0x6541);                     // mov.w @r4,r5 -> RAM 0x08000000
        rom.MovLiteral(4, 0x10000004u);      // VA del sleep en RAM
        rom.Op(0x442B);                     // jmp @r4 (0x402B era jmp @r0)
        rom.Nop();
        rom.Sleep();

        Cg50NativeCore core;
        core.LoadFirmware(rom.bytes.data(), rom.bytes.size());
        WriteHandlerRte(core, 0x08000400u);  // vector TLB miss
        WriteHandlerRte(core, 0x08000600u);   // vector interrupciones
        WriteHandlerRte(core, 0x08000100u);   // vector excepciones generales
        core.Write32(0x08000000u, 0x12345678u);
        core.Write32(0x08000004u, 0x001B0009u);
        core.RunInstructions(22);   // hasta el 2o ldtlb (PC=0x2C)
        {
            std::uint32_t mmucr = 0, pteh = 0, ptel = 0;
            core.Read32(0xFF000010u, mmucr);
            core.Read32(0xFF000000u, pteh);
            core.Read32(0xFF000004u, ptel);
            std::printf("[DBG-F0] pc=%08X expevt=%X state=%d MMUCR=%08X PTEH=%08X PTEL=%08X diag=%ls\n",
                        core.GetRegisters().pc,
                        (unsigned)core.GetRegisters().expevt,
                        (int)core.GetState(), mmucr, pteh, ptel,
                        core.Diagnostic().c_str());
            // Aislamiento: si el store del guest no commitio, forzar AT=1
            // desde el host y ver si la traduccion funciona.
            core.Write32(0xFF000010u, 1u);
            core.Read32(0xFF000010u, mmucr);
            std::printf("[DBG-F0b] tras write host: MMUCR=%08X\n", mmucr);
        }
        core.RunInstructions(1);    // el read traducido
        std::printf("[DBG-F1] pc=%08X r5=%08X expevt=%X state=%d diag=%ls\n",
                    core.GetRegisters().pc, core.GetRegisters().r[5],
                    (unsigned)core.GetRegisters().expevt,
                    (int)core.GetState(), core.Diagnostic().c_str());
        core.RunInstructions(37);
        std::printf("[DBG-F] r5=%08X state=%d pc=%08X expevt=%X diag=%ls\n",
                    core.GetRegisters().r[5], (int)core.GetState(),
                    core.GetRegisters().pc,
                    (unsigned)core.GetRegisters().expevt,
                    core.Diagnostic().c_str());
        Check(core.GetRegisters().r[5] == 0x1234u,
              "UTLB-F: lectura traducida 0x10000000 -> RAM 0x08000000");
        Check(core.GetState() == Cg50NativeCore::State::Sleeping,
              "UTLB-F: fetch traducido ejecuto el SLEEP mapeado en RAM");

        // DTLB miss de lectura: VA sin mapear -> EXPEVT 0x080 + vector 0x400.
        Cg50NativeCore core2;
        TestRom rom2;
        rom2.MovLiteral(4, 0x60000000u);
        rom2.Op(0x440E);
        rom2.MovLiteral(6, 0x08000000u);
        rom2.Op(0x462E);
        rom2.MovLiteral(4, 0x08000000u);  // PTEH: RAM identidad
        rom2.MovLiteral(7, 0xFF000000u);
        rom2.MovLToRn(7, 4);
        rom2.MovLiteral(4, 0x08000071u);
        rom2.MovLiteral(7, 0xFF000004u);
        rom2.MovLToRn(7, 4);
        rom2.Op(0x0028);                    // ldtlb (entrada 0: RAM)
        rom2.MovLiteral(7, 0xFF000010u);
        rom2.MovImm(5, 1);
        rom2.MovLToRn(7, 5);                // AT=1 (solo RAM mapeada)
        rom2.MovLiteral(4, 0x30000000u);    // VA sin mapear
        for (int i = 0; i < 20; ++i) {
            rom2.Op(0x6541);                 // mov.w @r4,r5 -> DTLB miss 0x080
            rom2.Nop();
        }
        rom2.Sleep();
        core2.LoadFirmware(rom2.bytes.data(), rom2.bytes.size());
        WriteHandlerRte(core2, 0x08000400u);
        WriteHandlerRte(core2, 0x08000100u);
        core2.RunInstructions(60);
        std::printf("[DBG-F2] state=%d pc=%08X expevt=%X diag=%ls\n",
                    (int)core2.GetState(), core2.GetRegisters().pc,
                    (unsigned)core2.GetRegisters().expevt,
                    core2.Diagnostic().c_str());
        Check(core2.GetRegisters().expevt == 0x080u,
              "UTLB-F2: DTLB miss de lectura dejo EXPEVT=0x080");
    }

    if (g_failures != 0) {
        std::printf("RESULTADO: %d fallos\n", g_failures);
        return 1;
    }
    std::printf("RESULTADO: INTC/TMU/RTC/WDT/ALIN/UTLB OK (9 grupos)\n");
    return 0;
}
