#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CasioCw2Core CasioCw2Core;

CasioCw2Core *casio_cw2_core_create(
    const uint8_t *rom_data, size_t rom_size,
    const uint8_t *ram_data, size_t ram_size);
void casio_cw2_core_destroy(CasioCw2Core *core);

void casio_cw2_core_power_on(CasioCw2Core *core);
void casio_cw2_core_advance(CasioCw2Core *core, uint32_t ms);
void casio_cw2_core_key_down(CasioCw2Core *core, uint8_t matrix_code);
void casio_cw2_core_key_up(CasioCw2Core *core);
int casio_cw2_core_tap_key(CasioCw2Core *core, uint8_t matrix_code, uint32_t hold_ms, uint32_t settle_ms);

int casio_cw2_core_screen_width(const CasioCw2Core *core);
int casio_cw2_core_screen_height(const CasioCw2Core *core);
int casio_cw2_core_screen_mode(const CasioCw2Core *core);
int casio_cw2_core_take_redraw_flag(CasioCw2Core *core);
size_t casio_cw2_core_copy_gray_levels(const CasioCw2Core *core, uint8_t *out, size_t out_size);
uint32_t casio_cw2_core_indicators(const CasioCw2Core *core);

size_t casio_cw2_core_ram_size(const CasioCw2Core *core);
size_t casio_cw2_core_export_ram(const CasioCw2Core *core, uint8_t *out, size_t out_size);
int casio_cw2_core_import_ram(CasioCw2Core *core, const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif
