// cg50_native_state_test.cpp — Contrato P1 de save-state (ExportState/
// ImportState): fidelidad de registros/RAM/storage/teclado/FPU y determinismo
// de la ejecucion continuada tras restaurar.

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
    void BraSelf()
    {
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
    void MovLToRn(const std::uint32_t n, const std::uint32_t m)
    {
        Op(static_cast<std::uint16_t>(0x2000u | (n << 8) | (m << 4) | 0x2u));
    }
};

std::uint32_t RamWord(const Cg50NativeCore& core, const std::uint32_t address)
{
    std::uint32_t value = 0;
    core.Read32(address, value);
    return value;
}

} // namespace

int main()
{
    TestRom rom;
    rom.MovLiteral(4, 0x60000000u);      // SR = MD|RB
    rom.Op(0x440E);                     // ldc r4,sr
    rom.MovLiteral(5, 0x08010000u);      // r5 = destino RAM
    for (std::uint32_t i = 0; i < 8; ++i) {
        rom.MovLiteral(6, 0xC0DE0000u + i);
        rom.MovLToRn(5, 6);             // mov.l r6,@r5
        rom.Op(0x7504);                 // add #4,r5
    }
    rom.BraSelf();

    std::vector<std::uint8_t> userRom(Cg50NativeCore::kUserRomSize, 0x00);
    Cg50NativeCore core;
    Check(core.LoadFirmwareParts(rom.bytes.data(), rom.bytes.size(),
                                 userRom.data(), userRom.size()),
          "STATE: firmware sintetico + UserROM cargados");
    core.RunInstructions(100);
    Check(RamWord(core, 0x08010000u) == 0xC0DE0000u,
          "STATE: patron RAM escrito por el guest");
    Check(core.Write8(0xA0B60100u, 0x5Au),
          "STATE: byte mutado en storage (UserROM 0xA0B60100)");
    Check(core.KeyDown(0x070Au), "STATE: tecla F1 mantenida");
    core.RunInstructions(50);

    std::vector<std::uint8_t> snapshot;
    Check(core.ExportState(snapshot), "STATE: ExportState ok");
    Check(snapshot.size() >
              Cg50NativeCore::kRomSize + Cg50NativeCore::kUserRomSize,
          "STATE: el snapshot incluye ROM + UserROM mutada");

    Cg50NativeCore restored;
    Check(restored.LoadFirmwareParts(rom.bytes.data(), rom.bytes.size(),
                                     userRom.data(), userRom.size()),
          "STATE: firmware recargado en el core restaurado");
    const bool importOk = restored.ImportState(snapshot.data(), snapshot.size());
    if (!importOk) {
        std::printf("[DBG-S] ImportState fallo: snapshot=%zu bytes diag=%ls\n",
                    snapshot.size(), restored.Diagnostic().c_str());
    }
    Check(importOk, "STATE: ImportState ok");

    const auto& a = core.GetRegisters();
    const auto& b = restored.GetRegisters();
    Check(a.pc == b.pc, "STATE: PC identico tras restaurar");
    Check(a.sr == b.sr, "STATE: SR identico");
    Check(a.r == b.r, "STATE: banco GPR identico");
    Check(a.fr == b.fr && a.xf == b.xf && a.fpul == b.fpul,
          "STATE: banco FPU identico");
    Check(RamWord(restored, 0x08010000u) == 0xC0DE0000u,
          "STATE: patron RAM restaurado");
    std::uint8_t storageByte = 0;
    Check(restored.Read8(0xA0B60100u, storageByte) && storageByte == 0x5Au,
          "STATE: byte de storage mutado restaurado");
    Check(restored.IsKeyDown(0x070Au),
          "STATE: tecla preservada en la matriz restaurada");
    Check(restored.GetState() == core.GetState(),
          "STATE: estado de CPU identico");

    // Determinismo: ambos avanzan 500 instrucciones y deben coincidir.
    core.RunInstructions(500);
    restored.RunInstructions(500);
    const auto& a2 = core.GetRegisters();
    const auto& b2 = restored.GetRegisters();
    Check(a2.pc == b2.pc && a2.sr == b2.sr && a2.r == b2.r,
          "STATE: ejecucion continuada DETERMINISTA (PC/SR/GPR identicos)");
    Check(core.GetStats().instructions == restored.GetStats().instructions,
          "STATE: contador de instrucciones continuo exacto");

    if (g_failures != 0) {
        std::printf("RESULTADO: %d fallos\n", g_failures);
        return 1;
    }
    std::printf("RESULTADO: save-state OK (13 checks)\n");
    return 0;
}
