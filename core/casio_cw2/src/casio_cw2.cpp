#include "casio_cw2.h"

#include "Chipset/Chipset.hpp"
#include "Data/HardwareId.hpp"
#include "Emulator.hpp"
#include "Peripheral/BatteryBackedRAM.hpp"
#include "Peripheral/Keyboard.hpp"
#include "Peripheral/Screen.hpp"

#include <new>

using namespace casioemux;

struct CasioCw2Core
{
    lua_State lua_stub_state;
    Emulator *emulator = nullptr;
};

extern "C" {

CasioCw2Core *casio_cw2_core_create(
    const uint8_t *rom_data, size_t rom_size,
    const uint8_t *ram_data, size_t ram_size)
{
    if (!rom_data || rom_size == 0)
        return nullptr;

    ModelDefinition def;
    def.model_name = "fx-991CW";
    def.hardware_id = HW_CLASSWIZ_II;
    def.real_hardware = false;
    def.csr_mask = 0x000F;
    def.pd_value = 0x00;

    auto *core = new (std::nothrow) CasioCw2Core();
    if (!core)
        return nullptr;

    casioemux_lua_stub::ScopedBinding binding(&core->lua_stub_state);
    core->emulator = new (std::nothrow) Emulator(def, rom_data, rom_size, ram_data, ram_size);
    if (!core->emulator) {
        delete core;
        return nullptr;
    }
    return core;
}

void casio_cw2_core_destroy(CasioCw2Core *core)
{
    if (!core)
        return;
    delete core->emulator;
    delete core;
}

void casio_cw2_core_power_on(CasioCw2Core *core)
{
    if (!core || !core->emulator || !core->emulator->keyboard)
        return;
    core->emulator->keyboard->PressCode(0xFF);
    core->emulator->Advance(250);
    core->emulator->keyboard->ReleaseAll();
    core->emulator->Advance(250);
}

void casio_cw2_core_advance(CasioCw2Core *core, uint32_t ms)
{
    if (!core || !core->emulator || ms == 0)
        return;
    core->emulator->Advance(ms);
}

void casio_cw2_core_key_down(CasioCw2Core *core, uint8_t matrix_code)
{
    if (!core || !core->emulator || !core->emulator->keyboard)
        return;
    core->emulator->keyboard->PressCode(matrix_code);
}

void casio_cw2_core_key_up(CasioCw2Core *core)
{
    if (!core || !core->emulator || !core->emulator->keyboard)
        return;
    core->emulator->keyboard->ReleaseAll();
}

int casio_cw2_core_tap_key(CasioCw2Core *core, uint8_t matrix_code, uint32_t hold_ms, uint32_t settle_ms)
{
    if (!core || !core->emulator || !core->emulator->keyboard)
        return 0;
    core->emulator->keyboard->PressCode(matrix_code);
    if (hold_ms > 0) core->emulator->Advance(hold_ms);
    core->emulator->keyboard->ReleaseAll();
    if (settle_ms > 0) core->emulator->Advance(settle_ms);
    return 1;
}

int casio_cw2_core_screen_width(const CasioCw2Core *core)
{
    return (core && core->emulator && core->emulator->screen)
        ? core->emulator->screen->Width() : 0;
}

int casio_cw2_core_screen_height(const CasioCw2Core *core)
{
    return (core && core->emulator && core->emulator->screen)
        ? core->emulator->screen->Height() : 0;
}

int casio_cw2_core_screen_mode(const CasioCw2Core *core)
{
    return (core && core->emulator && core->emulator->screen)
        ? core->emulator->screen->Mode() : 0;
}

int casio_cw2_core_take_redraw_flag(CasioCw2Core *core)
{
    if (!core || !core->emulator)
        return 0;
    if (!core->emulator->chipset.GetRequireFrame())
        return 0;
    core->emulator->chipset.Frame();
    return 1;
}

size_t casio_cw2_core_copy_gray_levels(const CasioCw2Core *core, uint8_t *out, size_t out_size)
{
    if (!core || !core->emulator || !core->emulator->screen)
        return 0;
    return core->emulator->screen->CopyGrayLevels(out, out_size);
}

uint32_t casio_cw2_core_indicators(const CasioCw2Core *core)
{
    if (!core || !core->emulator || !core->emulator->screen)
        return 0;
    return core->emulator->screen->IndicatorMask();
}

size_t casio_cw2_core_ram_size(const CasioCw2Core *core)
{
    if (!core || !core->emulator || !core->emulator->ram)
        return 0;
    return core->emulator->ram->Size();
}

size_t casio_cw2_core_export_ram(const CasioCw2Core *core, uint8_t *out, size_t out_size)
{
    if (!core || !core->emulator || !core->emulator->ram)
        return 0;
    return core->emulator->ram->ExportRam(out, out_size);
}

int casio_cw2_core_import_ram(CasioCw2Core *core, const uint8_t *data, size_t size)
{
    if (!core || !core->emulator || !core->emulator->ram)
        return 0;
    return core->emulator->ram->ImportRam(data, size) ? 1 : 0;
}

} // extern "C"
