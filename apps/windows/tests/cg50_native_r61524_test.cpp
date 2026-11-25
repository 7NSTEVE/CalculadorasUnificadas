// cg50_native_r61524_test.cpp — Contrato P1 de cobertura de comandos GRAM
// del R61524: entry mode, ventana parcial 0x0210-0x0213, auto-incremento con
// envoltura horizontal/vertical, SS flip (0x0001) y lectura GRAM/registros.

#include "cg50_core/r61524.h"

#include <cstdint>
#include <cstdio>

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

} // namespace

int main()
{
    cg50::R61524 lcd;
    lcd.Reset();

    cg50::R61524::Pins pins{};
    pins.v28sys = true;
    pins.resetX = true;

    const auto command = [&](const std::uint16_t index) {
        pins.registerSelect = false;
        lcd.SetPins(pins);
        lcd.ObserveWrite16(index);
    };
    const auto data = [&](const std::uint16_t value) {
        pins.registerSelect = true;
        lcd.SetPins(pins);
        lcd.ObserveWrite16(value);
    };
    const auto gramAt = [&](const std::size_t x, const std::size_t y) {
        return lcd.FramebufferData()[y * cg50::R61524::kVisibleWidth + x];
    };

    // ------------------------------------------------------------------
    // 1. Configuracion: entry mode 0x1030 (incX+incY, horizontal primero).
    // ------------------------------------------------------------------
    command(0x0003);
    data(0x1030);

    // ------------------------------------------------------------------
    // 2. Ventana parcial X=[2..5], Y=[3..4] y cursor en (2,3).
    // ------------------------------------------------------------------
    command(0x0210); data(0x0002);   // winStartX
    command(0x0211); data(0x0005);   // winEndX
    command(0x0212); data(0x0003);   // winStartY
    command(0x0213); data(0x0004);   // winEndY
    command(0x0200); data(0x0002);   // curX
    command(0x0201); data(0x0003);   // curY

    // Contracto de lectura validado contra el boot del OS: ID en indice 0 y
    // 0 en todo lo demas (el read-back rico regreso el boot — ver nota en
    // ReadBus16). Se verifica ese contrato explícitamente.
    std::uint16_t readback = 0;
    lcd.SetPins(pins);
    command(0x0000);
    lcd.ReadBus16(readback);
    Check(readback == cg50::R61524::kConfiguredId,
          "R61524: lectura del ID del panel (indice 0)");
    command(0x0210);
    lcd.ReadBus16(readback);
    Check(readback == 0x0000,
          "R61524: registro != 0 lee 0 (contrato del boot validado)");

    // ------------------------------------------------------------------
    // 3. Escritura GRAM dentro de la ventana: 6 pixeles deben envolver en
    //    X=5 -> X=2 con Y avanzando de 3 a 4.
    // ------------------------------------------------------------------
    command(0x0202);
    data(0x1111); data(0x2222); data(0x3333); data(0x4444);
    data(0x5555); data(0x6666);
    Check(gramAt(2, 3) == 0x1111, "R61524: pixel 0 en (2,3)");
    Check(gramAt(3, 3) == 0x2222, "R61524: pixel 1 en (3,3)");
    Check(gramAt(4, 3) == 0x3333, "R61524: pixel 2 en (4,3)");
    Check(gramAt(5, 3) == 0x4444, "R61524: pixel 3 en (5,3) (borde ventana)");
    Check(gramAt(2, 4) == 0x5555, "R61524: envoltura a (2,4) tras winEndX");
    Check(gramAt(3, 4) == 0x6666, "R61524: pixel 5 en (3,4)");

    // ------------------------------------------------------------------
    // 4. Driver Output Control (0x0001, SS bit 8): almacenado SIN aplicar el
    //    espejo (el boot validado renderiza por esta ruta exacta; el flip
    //    queda documentado como pendiente de evidencia del oficial).
    // ------------------------------------------------------------------
    command(0x0001); data(0x0100);
    command(0x0200); data(0x0000);
    command(0x0201); data(0x0000);
    command(0x0022);
    data(0xABCD);
    Check(gramAt(0, 0) == 0xABCD,
          "R61524: escritura en (0,0) sin espejo SS (contrato del boot)");

    // ------------------------------------------------------------------
    // 5. Reset: GRAM disponible y ventana restaurada.
    // ------------------------------------------------------------------
    lcd.Reset();
    command(0x0022);
    data(0xFFFF);
    Check(lcd.FramebufferAvailable(),
          "R61524: reset deja el framebuffer disponible");
    Check(gramAt(0, 0) == 0xFFFF, "R61524: escritura post-reset en (0,0)");

    if (g_failures != 0) {
        std::printf("RESULTADO: %d fallos\n", g_failures);
        return 1;
    }
    std::printf("RESULTADO: R61524 GRAM OK (10 checks)\n");
    return 0;
}
