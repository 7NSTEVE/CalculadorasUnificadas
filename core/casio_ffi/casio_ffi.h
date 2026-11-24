#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) && defined(CASIO_NATIVE_SHARED)
#define CASIO_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define CASIO_EXPORT __attribute__((visibility("default")))
#else
#define CASIO_EXPORT
#endif

CASIO_EXPORT void* casio_alloc(size_t size);
CASIO_EXPORT void casio_free(void* ptr);

// ============================================================================
// nX-U8 / nX-U16 Emulators (fx-570LA X ClassWiz = 4, fx-570VN PLUS = 3)
// ============================================================================
typedef struct CasioNxHandle CasioNxHandle;

CASIO_EXPORT CasioNxHandle* casio_nx_create(int hardware_id, const uint8_t* rom_data, size_t rom_size);
CASIO_EXPORT CasioNxHandle* casio_nx_create_from_file(int hardware_id, const char* rom_path);
CASIO_EXPORT void casio_nx_destroy(CasioNxHandle* handle);

CASIO_EXPORT void casio_nx_power_on(CasioNxHandle* handle);
CASIO_EXPORT void casio_nx_advance(CasioNxHandle* handle, uint32_t ms);
CASIO_EXPORT void casio_nx_key_down(CasioNxHandle* handle, uint32_t matrix_code);
CASIO_EXPORT void casio_nx_key_up(CasioNxHandle* handle);
CASIO_EXPORT int casio_nx_tap_key(CasioNxHandle* handle, uint32_t matrix_code, uint32_t hold_ms, uint32_t settle_ms);

CASIO_EXPORT int casio_nx_screen_width(const CasioNxHandle* handle);
CASIO_EXPORT int casio_nx_screen_height(const CasioNxHandle* handle);
CASIO_EXPORT int casio_nx_screen_mode(const CasioNxHandle* handle);
CASIO_EXPORT int casio_nx_screen_contrast(const CasioNxHandle* handle);

// Renders top-down RGBA8888 dot matrix and outputs row-0 indicator bitmask:
// bit 0:S, 1:A, 2:M, 3:STO, 4:Math, 5:D, 6:R, 7:G, 8:FIX, 9:SCI, 10:DOWN, 11:UP
// Returns >0 if copied, 0 if unchanged, <0 on invalid args.
CASIO_EXPORT int casio_nx_render_rgba(
    CasioNxHandle* handle,
    uint8_t* out_rgba,
    int out_size,
    uint32_t* out_indicators,
    int force_copy);

CASIO_EXPORT int casio_nx_save_state_file(CasioNxHandle* handle, const char* path);
CASIO_EXPORT int casio_nx_load_state_file(CasioNxHandle* handle, const char* path);

// ============================================================================
// fx-991CW ClassWiz II Emulator (nX-U16/100, 192x63 4-level grayscale, 24832B RAM)
// ============================================================================
typedef struct CasioCw2Handle CasioCw2Handle;

CASIO_EXPORT CasioCw2Handle* casio_cw2_create(
    const uint8_t* rom_data, size_t rom_size,
    const uint8_t* ram_data, size_t ram_size);
CASIO_EXPORT CasioCw2Handle* casio_cw2_create_from_files(const char* rom_path, const char* ram_path);
CASIO_EXPORT void casio_cw2_destroy(CasioCw2Handle* handle);

CASIO_EXPORT void casio_cw2_power_on(CasioCw2Handle* handle);
CASIO_EXPORT void casio_cw2_advance(CasioCw2Handle* handle, uint32_t ms);
CASIO_EXPORT void casio_cw2_key_down(CasioCw2Handle* handle, uint32_t matrix_code);
CASIO_EXPORT void casio_cw2_key_up(CasioCw2Handle* handle);
CASIO_EXPORT int casio_cw2_tap_key(CasioCw2Handle* handle, uint32_t matrix_code, uint32_t hold_ms, uint32_t settle_ms);

CASIO_EXPORT int casio_cw2_screen_width(const CasioCw2Handle* handle);
CASIO_EXPORT int casio_cw2_screen_height(const CasioCw2Handle* handle);

// Renders 192x63 4-level grayscale RGBA8888 (#E4EBE4 -> #14243A) and outputs 20-bit indicator mask:
// 0:S, 1:Math, 2:D, 3:R, 4:G, 5:FIX, 6:SCI, 7:f(x), 8:E, 9:i, 10:angle, 11:WDOWN,
// 12:VERIFY, 13:g(x), 14:LEFT, 15:DOWN, 16:UP, 17:RIGHT, 18:PAUSE, 19:SUN.
// Returns >0 if copied, 0 if unchanged, <0 on error.
CASIO_EXPORT int casio_cw2_render_rgba(
    CasioCw2Handle* handle,
    uint8_t* out_rgba,
    int out_size,
    uint32_t* out_indicators,
    int force_copy);

CASIO_EXPORT size_t casio_cw2_export_ram(const CasioCw2Handle* handle, uint8_t* out_buf, size_t out_size);
CASIO_EXPORT int casio_cw2_import_ram(CasioCw2Handle* handle, const uint8_t* buf, size_t size);
CASIO_EXPORT int casio_cw2_export_ram_file(const CasioCw2Handle* handle, const char* path);
CASIO_EXPORT int casio_cw2_import_ram_file(CasioCw2Handle* handle, const char* path);

// ============================================================================
// fx-CG50 PRIZM Emulator (SH7305 / SH-4A + R61524 384x216 RGB565)
// ============================================================================
typedef struct CasioCg50Handle CasioCg50Handle;

CASIO_EXPORT CasioCg50Handle* casio_cg50_create_from_dir(const char* dir_path);
CASIO_EXPORT void casio_cg50_destroy(CasioCg50Handle* handle);

// Returns current state: 0=Empty, 1=Running, 2=Sleeping, 3=Halted.
CASIO_EXPORT int casio_cg50_step(CasioCg50Handle* handle, uint64_t run_budget, uint64_t sleep_cycles);

CASIO_EXPORT int casio_cg50_key_down(CasioCg50Handle* handle, uint16_t matrix_code);
CASIO_EXPORT void casio_cg50_key_up(CasioCg50Handle* handle, uint16_t matrix_code);
CASIO_EXPORT void casio_cg50_release_all_keys(CasioCg50Handle* handle);

CASIO_EXPORT int casio_cg50_get_stats(
    const CasioCg50Handle* handle,
    uint64_t* out_instructions,
    uint64_t* out_cycles,
    uint32_t* out_pc,
    uint64_t* out_lcd_writes);

// Renders 384x216 top-down RGBA8888 framebuffer (out_size >= 331776) with FNV-1a dirty-frame skip.
// Returns 1 if active framebuffer changed and was copied, 0 if unchanged or blank, <0 on error.
CASIO_EXPORT int casio_cg50_render_rgba(
    CasioCg50Handle* handle,
    uint8_t* out_rgba,
    int out_size);

// Same as casio_cg50_render_rgba but allows force_copy != 0 to bypass dirty-frame skip.
CASIO_EXPORT int casio_cg50_render_rgba_ex(
    CasioCg50Handle* handle,
    uint8_t* out_rgba,
    int out_size,
    int force_copy);

CASIO_EXPORT size_t casio_cg50_export_state(const CasioCg50Handle* handle, uint8_t* out_buf, size_t out_size);
CASIO_EXPORT int casio_cg50_import_state(CasioCg50Handle* handle, const uint8_t* buf, size_t size);
CASIO_EXPORT int casio_cg50_export_state_file(const CasioCg50Handle* handle, const char* path);
CASIO_EXPORT int casio_cg50_import_state_file(CasioCg50Handle* handle, const char* path);

// Real-time RTC wall-clock sync, UserROM backup/restore, and .g3a add-in staging
CASIO_EXPORT int casio_cg50_set_rtc_epoch(CasioCg50Handle* handle, int64_t local_unix_seconds);
CASIO_EXPORT int casio_cg50_export_user_rom_file(const CasioCg50Handle* handle, const char* path);
CASIO_EXPORT int casio_cg50_import_user_rom_file(CasioCg50Handle* handle, const char* path);
CASIO_EXPORT int casio_cg50_load_addin_file(CasioCg50Handle* handle, const char* path);

// Direct SMEM (Fugue FTL + FAT16/LFN) file exchange
CASIO_EXPORT int casio_cg50_smem_list_json(CasioCg50Handle* handle, const char* path, char* out_json, int out_size);
CASIO_EXPORT int64_t casio_cg50_smem_read_file(CasioCg50Handle* handle, const char* path, uint8_t* out_buf, size_t out_size);
CASIO_EXPORT int casio_cg50_smem_write_file(CasioCg50Handle* handle, const char* path, const uint8_t* data, size_t size, int overwrite);
CASIO_EXPORT int casio_cg50_smem_delete(CasioCg50Handle* handle, const char* path, int recursive);
CASIO_EXPORT int casio_cg50_smem_mkdir(CasioCg50Handle* handle, const char* path);
CASIO_EXPORT int casio_cg50_smem_remount_os(CasioCg50Handle* handle);

// Virtual USB 2.0 Host & External SCIF0 3-pin Link
CASIO_EXPORT int casio_cg50_usb_set_connected(CasioCg50Handle* handle, int connected);
CASIO_EXPORT int casio_cg50_usb_get_status_json(CasioCg50Handle* handle, char* out_json, int out_size);
CASIO_EXPORT int casio_cg50_link_set_enabled(CasioCg50Handle* handle, int enabled);
CASIO_EXPORT int casio_cg50_link_push_rx(CasioCg50Handle* handle, const uint8_t* data, size_t len);
CASIO_EXPORT int casio_cg50_link_pop_tx(CasioCg50Handle* handle, uint8_t* out_buf, size_t max_len);

// Native ISO-18004 QR decoder on current 384x216 LCD frame
CASIO_EXPORT int casio_cg50_decode_lcd_qr(CasioCg50Handle* handle, char* out_text, int out_size);

#ifdef __cplusplus
}
#endif
