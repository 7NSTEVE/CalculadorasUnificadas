#include "calcemu.h"

#include "Chipset/Chipset.hpp"
#include "Data/HardwareId.hpp"
#include "Emulator.hpp"
#include "Peripheral/BatteryBackedRAM.hpp"
#include "Peripheral/Keyboard.hpp"
#include "Peripheral/Screen.hpp"

#include <cstring>
#include <new>

using namespace casioemu;

struct CalcEmu
{
    lua_State lua_stub_state;
    Emulator *emulator;
};

namespace
{
    const Screen *ScreenOf(const CalcEmu *emu)
    {
        return emu && emu->emulator ? emu->emulator->screen : nullptr;
    }
}

extern "C" {

CalcEmu *calcemu_create(const CalcEmuModel *model,
                        const unsigned char *rom, size_t rom_size)
{
    if (!model || !rom || rom_size == 0)
        return nullptr;
    if (model->hardware_id != CALCEMU_HW_ES_PLUS && model->hardware_id != CALCEMU_HW_CLASSWIZ)
        return nullptr;

    ModelDefinition definition;
    definition.model_name = "calcemu";
    definition.hardware_id = (HardwareId)model->hardware_id;
    definition.real_hardware = model->real_hardware != 0;
    definition.csr_mask = model->csr_mask;
    definition.pd_value = model->pd_value;

    CalcEmu *emu = new (std::nothrow) CalcEmu();
    if (!emu)
        return nullptr;
    // CPU y MMU conservan llamadas de registro a Lua durante sus constructores.
    // No hay interprete en iOS, pero necesitan memoria temporal valida. Se liga
    // el stub propio de esta instancia durante toda la construccion; el binding
    // es thread_local y ScopedBinding restaura incluso ante una excepcion.
    calcemu_lua_stub::ScopedBinding lua_binding(&emu->lua_stub_state);
    emu->emulator = new (std::nothrow) Emulator(definition, rom, rom_size);
    if (!emu->emulator)
    {
        delete emu;
        return nullptr;
    }
    return emu;
}

void calcemu_destroy(CalcEmu *emu)
{
    if (!emu)
        return;
    delete emu->emulator;
    delete emu;
}

void calcemu_power_on(CalcEmu *emu)
{
    if (!emu || !emu->emulator || !emu->emulator->keyboard)
        return;
    // El firmware consulta la matriz del teclado mientras la CPU avanza. Pulsar
    // y soltar ON sin ejecutar ningun ciclo entre ambos eventos hace que algunas
    // ROM (en particular fx-570VN PLUS) no lleguen a observar la tecla. Una
    // pulsacion estable mantiene ON durante 400 ms emulados. El banco de
    // regresion usa el mismo intervalo: por debajo de ~350 ms algunas ROM
    // pueden perder la tecla y por encima de ~600 ms aparece autorrepeticion.
    emu->emulator->keyboard->PressCode(0xFF);
    emu->emulator->Advance(400);
    emu->emulator->keyboard->ReleaseAll();
}

void calcemu_advance(CalcEmu *emu, unsigned int milliseconds)
{
    if (emu && emu->emulator)
        emu->emulator->Advance(milliseconds);
}

void calcemu_key_down(CalcEmu *emu, unsigned char matrix_code)
{
    if (emu && emu->emulator && emu->emulator->keyboard)
        emu->emulator->keyboard->PressCode(matrix_code);
}

void calcemu_key_up(CalcEmu *emu)
{
    if (emu && emu->emulator && emu->emulator->keyboard)
        emu->emulator->keyboard->ReleaseAll();
}

int calcemu_tap_key(CalcEmu *emu, unsigned char matrix_code,
                    unsigned int hold_milliseconds,
                    unsigned int settle_milliseconds)
{
    if (!emu || !emu->emulator || !emu->emulator->keyboard)
        return 0;

    emu->emulator->keyboard->PressCode(matrix_code);
    emu->emulator->Advance(hold_milliseconds);
    emu->emulator->keyboard->ReleaseAll();
    emu->emulator->Advance(settle_milliseconds);
    return 1;
}

int calcemu_screen_rows(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->RowCount() : 0;
}

int calcemu_screen_width(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->PixelWidth() : 0;
}

int calcemu_screen_row_size(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->RowSize() : 0;
}

int calcemu_screen_row_offset(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->RowOffset() : 0;
}

size_t calcemu_screen_buffer_size(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->BufferSize() : 0;
}

size_t calcemu_screen_buffer(const CalcEmu *emu, unsigned char *out, size_t out_size)
{
    const Screen *screen = ScreenOf(emu);
    if (!screen || !screen->Buffer() || !out)
        return 0;
    size_t size = screen->BufferSize();
    if (out_size < size)
        return 0;
    std::memcpy(out, screen->Buffer(), size);
    return size;
}

size_t calcemu_screen_pixels(const CalcEmu *emu, unsigned char *out, size_t out_size)
{
    const Screen *screen = ScreenOf(emu);
    if (!screen || !screen->Buffer() || !out)
        return 0;

    const int rows = screen->RowCount();
    const int width = screen->PixelWidth();
    const size_t needed = (size_t)rows * width;
    if (out_size < needed)
        return 0;

    const unsigned char *buffer = screen->Buffer();
    const int row_size = screen->RowSize();
    const int row_offset = screen->RowOffset();
    const int visible = screen->VisibleBytesPerRow();

    for (int y = 0; y != rows; ++y)
    {
        for (int xb = 0; xb != visible; ++xb)
        {
            unsigned char byte = buffer[y * row_size + row_offset + xb];
            for (int bit = 0; bit != 8; ++bit)
                out[(size_t)y * width + xb * 8 + bit] = (byte & (0x80 >> bit)) ? 1 : 0;
        }
    }
    return needed;
}

int calcemu_screen_mode(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->Mode() : 0;
}

int calcemu_screen_contrast(const CalcEmu *emu)
{
    const Screen *screen = ScreenOf(emu);
    return screen ? screen->Contrast() : 0;
}

int calcemu_take_redraw_flag(CalcEmu *emu)
{
    if (!emu || !emu->emulator)
        return 0;
    if (!emu->emulator->chipset.GetRequireFrame())
        return 0;
    // Frame() recorre los perifericos limpiando su aviso, igual que hacia el bucle
    // de eventos del emulador de escritorio.
    emu->emulator->chipset.Frame();
    return 1;
}

size_t calcemu_save_state(CalcEmu *emu, unsigned char *out, size_t out_size)
{
    if (!emu || !emu->emulator)
        return 0;

    std::vector<unsigned char> blob;
    emu->emulator->SaveState(blob);
    if (blob.empty())
        return 0;
    // Con out = NULL solo se consulta el tamano; con un buffer corto tampoco se
    // escribe nada, para no dejar medio volcado en manos del anfitrion.
    if (!out || out_size < blob.size())
        return blob.size();

    std::memcpy(out, blob.data(), blob.size());
    return blob.size();
}

int calcemu_load_state(CalcEmu *emu, const unsigned char *data, size_t size)
{
    if (!emu || !emu->emulator || !data || size == 0)
        return 0;
    return emu->emulator->LoadState(data, size) ? 1 : 0;
}

size_t calcemu_ram_size(const CalcEmu *emu)
{
    if (!emu || !emu->emulator || !emu->emulator->ram)
        return 0;
    return emu->emulator->ram->Size();
}

size_t calcemu_read_ram(const CalcEmu *emu, unsigned char *out, size_t out_size)
{
    if (!emu || !emu->emulator || !emu->emulator->ram || !out)
        return 0;
    size_t size = emu->emulator->ram->Size();
    if (out_size < size)
        return 0;
    std::memcpy(out, emu->emulator->ram->Buffer(), size);
    return size;
}

} // extern "C"
