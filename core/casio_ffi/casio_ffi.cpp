#include "casio_ffi.h"

#include "calcemu.h"
#include "casio_cw2.h"
#include "cg50_native_core.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out)
{
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    if (std::fseek(fp, 0, SEEK_END) != 0) {
        std::fclose(fp);
        return false;
    }
    long len = std::ftell(fp);
    if (len <= 0 || std::fseek(fp, 0, SEEK_SET) != 0) {
        std::fclose(fp);
        return false;
    }
    out.resize(static_cast<size_t>(len));
    size_t got = std::fread(out.data(), 1, out.size(), fp);
    std::fclose(fp);
    if (got != out.size()) {
        out.clear();
        return false;
    }
    return true;
}

bool WriteBinaryFileAtomic(const std::string& path, const uint8_t* data, size_t size)
{
    if (path.empty() || !data || size == 0) return false;
    const std::string tmpPath = path + ".tmp";
    FILE* fp = std::fopen(tmpPath.c_str(), "wb");
    if (!fp) return false;
    const size_t written = std::fwrite(data, 1, size, fp);
    const int flushOk = std::fflush(fp);
    const int closeOk = std::fclose(fp);
    if (written != size || flushOk != 0 || closeOk != 0) {
        std::remove(tmpPath.c_str());
        return false;
    }
#if defined(_WIN32)
    std::remove(path.c_str());
#endif
    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        std::remove(tmpPath.c_str());
        return false;
    }
    return true;
}

std::string JoinPath(const std::string& dir, const char* file)
{
    if (dir.empty()) return std::string(file);
    char back = dir.back();
    if (back == '/' || back == '\\') return dir + file;
    return dir + "/" + file;
}

inline uint64_t Fnv1a64(const void* data, size_t len) noexcept
{
    const auto* p = static_cast<const uint8_t*>(data);
    uint64_t h = 14695981039346656037ULL;
    for (size_t i = 0; i < len; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

uint32_t DecodeNxIndicators(int hardwareId, const uint8_t* raw, size_t len)
{
    uint32_t mask = 0;
    if (!raw) return 0;
    if (hardwareId == CALCEMU_HW_CLASSWIZ) {
        if (len < 32) return 0;
        if (raw[0x00] & 0x01u) mask |= (1u << 0);  // S
        if (raw[0x01] & 0x01u) mask |= (1u << 1);  // A
        if (raw[0x02] & 0x01u) mask |= (1u << 2);  // M
        if (raw[0x03] & 0x01u) mask |= (1u << 3);  // STO
        if (raw[0x05] & 0x01u) mask |= (1u << 4);  // Math
        if (raw[0x06] & 0x01u) mask |= (1u << 5);  // D
        if (raw[0x07] & 0x01u) mask |= (1u << 6);  // R
        if (raw[0x08] & 0x01u) mask |= (1u << 7);  // G
        if (raw[0x09] & 0x01u) mask |= (1u << 8);  // FIX
        if (raw[0x0A] & 0x01u) mask |= (1u << 9);  // SCI
        if (raw[0x11] & 0x01u) mask |= (1u << 10); // DOWN
        if (raw[0x12] & 0x01u) mask |= (1u << 11); // UP
    } else if (hardwareId == CALCEMU_HW_ES_PLUS) {
        if (len < 16) return 0;
        if (raw[0x00] & 0x10u) mask |= (1u << 0);  // S
        if (raw[0x00] & 0x04u) mask |= (1u << 1);  // A
        if (raw[0x01] & 0x10u) mask |= (1u << 2);  // M
        if (raw[0x01] & 0x02u) mask |= (1u << 3);  // STO
        if (raw[0x0A] & 0x40u) mask |= (1u << 4);  // Math
        if (raw[0x07] & 0x20u) mask |= (1u << 5);  // D
        if (raw[0x07] & 0x02u) mask |= (1u << 6);  // R
        if (raw[0x08] & 0x10u) mask |= (1u << 7);  // G
        if (raw[0x08] & 0x01u) mask |= (1u << 8);  // FIX
        if (raw[0x09] & 0x20u) mask |= (1u << 9);  // SCI
        if (raw[0x0A] & 0x08u) mask |= (1u << 10); // DOWN
        if (raw[0x0B] & 0x80u) mask |= (1u << 11); // UP
    }
    return mask;
}

inline void WriteRgb565ToRgba(uint16_t px, uint8_t* dst)
{
    uint8_t r5 = static_cast<uint8_t>((px >> 11) & 0x1Fu);
    uint8_t g6 = static_cast<uint8_t>((px >> 5) & 0x3Fu);
    uint8_t b5 = static_cast<uint8_t>(px & 0x1Fu);
    dst[0] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
    dst[1] = static_cast<uint8_t>((g6 << 2) | (g6 >> 4));
    dst[2] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
    dst[3] = 255u;
}

// 4-level grayscale palette for fx-991CW (#E4EBE4 -> #14243A)
constexpr uint8_t kCw2Palette[4][3] = {
    {0xE4, 0xEB, 0xE4}, // Level 0: paper LCD background
    {0x9E, 0xAD, 0xAC}, // Level 1: 1/3 ink
    {0x59, 0x6A, 0x73}, // Level 2: 2/3 ink
    {0x14, 0x24, 0x3A}  // Level 3: full ink
};

} // namespace

struct CasioNxHandle {
    CalcEmu* emu = nullptr;
    int hardwareId = 0;
    uint32_t lastIndicators = 0xFFFFFFFFu;
    int lastMode = -1;
    bool hasRenderedOnce = false;
    std::vector<uint8_t> rawBuf;
    std::vector<uint8_t> pixelBuf;
};

struct CasioCw2Handle {
    CasioCw2Core* core = nullptr;
    uint32_t lastIndicators = 0xFFFFFFFFu;
    uint64_t lastFrameHash = 0;
    bool hasRenderedOnce = false;
    std::vector<uint8_t> grayBuf;
};

struct CasioCg50Handle {
    std::unique_ptr<Cg50NativeCore> core;
    std::vector<uint16_t> pressedKeys;
    uint64_t lastFrameHash = 0;
    bool hasFrameHash = false;
};

extern "C" {

void* casio_alloc(size_t size)
{
    return std::malloc(size == 0 ? 1 : size);
}

void casio_free(void* ptr)
{
    std::free(ptr);
}

// ============================================================================
// nX-U8 / nX-U16
// ============================================================================
CasioNxHandle* casio_nx_create(int hardware_id, const uint8_t* rom_data, size_t rom_size)
{
    if (!rom_data || rom_size == 0) return nullptr;
    CalcEmuModel model{};
    if (hardware_id == CALCEMU_HW_CLASSWIZ) {
        model.hardware_id = CALCEMU_HW_CLASSWIZ;
        model.real_hardware = 0;
        model.csr_mask = 0x0003;
        model.pd_value = 0x00;
    } else if (hardware_id == CALCEMU_HW_ES_PLUS) {
        model.hardware_id = CALCEMU_HW_ES_PLUS;
        model.real_hardware = 0;
        model.csr_mask = 0x0001;
        model.pd_value = 0x08;
    } else {
        return nullptr;
    }

    CalcEmu* emu = calcemu_create(&model, rom_data, rom_size);
    if (!emu) return nullptr;

    calcemu_power_on(emu);
    calcemu_advance(emu, 320);

    auto* h = new CasioNxHandle();
    h->emu = emu;
    h->hardwareId = hardware_id;
    h->rawBuf.resize(calcemu_screen_buffer_size(emu));
    h->pixelBuf.resize(static_cast<size_t>(calcemu_screen_width(emu)) *
                       static_cast<size_t>(calcemu_screen_rows(emu)));
    return h;
}

CasioNxHandle* casio_nx_create_from_file(int hardware_id, const char* rom_path)
{
    if (!rom_path) return nullptr;
    std::vector<uint8_t> rom;
    if (!ReadBinaryFile(rom_path, rom)) return nullptr;
    return casio_nx_create(hardware_id, rom.data(), rom.size());
}

void casio_nx_destroy(CasioNxHandle* handle)
{
    if (!handle) return;
    if (handle->emu) calcemu_destroy(handle->emu);
    delete handle;
}

void casio_nx_power_on(CasioNxHandle* handle)
{
    if (!handle || !handle->emu) return;
    calcemu_power_on(handle->emu);
    calcemu_advance(handle->emu, 120);
    handle->hasRenderedOnce = false;
}

void casio_nx_advance(CasioNxHandle* handle, uint32_t ms)
{
    if (!handle || !handle->emu || ms == 0) return;
    calcemu_advance(handle->emu, ms);
}

void casio_nx_key_down(CasioNxHandle* handle, uint32_t matrix_code)
{
    if (!handle || !handle->emu) return;
    if ((matrix_code & 0xFFu) == 0xFFu) {
        calcemu_power_on(handle->emu);
        calcemu_advance(handle->emu, 80);
        return;
    }
    calcemu_key_down(handle->emu, static_cast<unsigned char>(matrix_code & 0xFFu));
}

void casio_nx_key_up(CasioNxHandle* handle)
{
    if (!handle || !handle->emu) return;
    calcemu_key_up(handle->emu);
}

int casio_nx_tap_key(CasioNxHandle* handle, uint32_t matrix_code, uint32_t hold_ms, uint32_t settle_ms)
{
    if (!handle || !handle->emu) return 0;
    if ((matrix_code & 0xFFu) == 0xFFu) {
        calcemu_power_on(handle->emu);
        if (settle_ms > 0) calcemu_advance(handle->emu, settle_ms);
        return 1;
    }
    return calcemu_tap_key(handle->emu,
                           static_cast<unsigned char>(matrix_code & 0xFFu),
                           hold_ms, settle_ms);
}

int casio_nx_screen_width(const CasioNxHandle* handle)
{
    return (handle && handle->emu) ? calcemu_screen_width(handle->emu) : 0;
}

int casio_nx_screen_height(const CasioNxHandle* handle)
{
    return (handle && handle->emu) ? calcemu_screen_rows(handle->emu) : 0;
}

int casio_nx_screen_mode(const CasioNxHandle* handle)
{
    return (handle && handle->emu) ? calcemu_screen_mode(handle->emu) : 0;
}

int casio_nx_screen_contrast(const CasioNxHandle* handle)
{
    return (handle && handle->emu) ? calcemu_screen_contrast(handle->emu) : 0;
}

int casio_nx_render_rgba(
    CasioNxHandle* handle,
    uint8_t* out_rgba,
    int out_size,
    uint32_t* out_indicators,
    int force_copy)
{
    if (!handle || !handle->emu || !out_rgba) return -1;
    const int w = calcemu_screen_width(handle->emu);
    const int h = calcemu_screen_rows(handle->emu);
    const int needed = w * h * 4;
    if (w <= 0 || h <= 0 || out_size < needed) return -1;

    if (handle->rawBuf.size() < calcemu_screen_buffer_size(handle->emu)) {
        handle->rawBuf.resize(calcemu_screen_buffer_size(handle->emu));
    }
    const size_t rawGot = calcemu_screen_buffer(
        handle->emu, handle->rawBuf.data(), handle->rawBuf.size());
    const uint32_t indMask = DecodeNxIndicators(
        handle->hardwareId, handle->rawBuf.data(), rawGot);
    if (out_indicators) *out_indicators = indMask;

    const int redraw = calcemu_take_redraw_flag(handle->emu);
    const int mode = calcemu_screen_mode(handle->emu);
    const bool changed = !handle->hasRenderedOnce ||
                         force_copy != 0 ||
                         redraw != 0 ||
                         indMask != handle->lastIndicators ||
                         mode != handle->lastMode;
    if (!changed) return 0;

    handle->hasRenderedOnce = true;
    handle->lastIndicators = indMask;
    handle->lastMode = mode;

    const size_t pxCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (handle->pixelBuf.size() < pxCount) handle->pixelBuf.resize(pxCount);
    calcemu_screen_pixels(handle->emu, handle->pixelBuf.data(), handle->pixelBuf.size());

    const uint8_t bgR = (handle->hardwareId == CALCEMU_HW_CLASSWIZ) ? 208 : 205;
    const uint8_t bgG = (handle->hardwareId == CALCEMU_HW_CLASSWIZ) ? 222 : 217;
    const uint8_t bgB = (handle->hardwareId == CALCEMU_HW_CLASSWIZ) ? 200 : 205;
    const uint8_t fgR = (handle->hardwareId == CALCEMU_HW_CLASSWIZ) ? 28 : 25;
    const uint8_t fgG = (handle->hardwareId == CALCEMU_HW_CLASSWIZ) ? 38 : 35;
    const uint8_t fgB = (handle->hardwareId == CALCEMU_HW_CLASSWIZ) ? 56 : 50;

    for (size_t i = 0; i < pxCount; ++i) {
        const bool on = handle->pixelBuf[i] != 0;
        uint8_t* p = out_rgba + i * 4;
        p[0] = on ? fgR : bgR;
        p[1] = on ? fgG : bgG;
        p[2] = on ? fgB : bgB;
        p[3] = 255u;
    }
    return 1;
}

int casio_nx_save_state_file(CasioNxHandle* handle, const char* path)
{
    if (!handle || !handle->emu || !path || !path[0]) return 0;
    const size_t needed = calcemu_save_state(handle->emu, nullptr, 0);
    if (needed == 0) return 0;
    std::vector<uint8_t> blob(needed);
    if (calcemu_save_state(handle->emu, blob.data(), blob.size()) != needed) return 0;
    return WriteBinaryFileAtomic(path, blob.data(), blob.size()) ? 1 : 0;
}

int casio_nx_load_state_file(CasioNxHandle* handle, const char* path)
{
    if (!handle || !handle->emu || !path || !path[0]) return 0;
    std::vector<uint8_t> blob;
    if (!ReadBinaryFile(path, blob)) return 0;
    if (calcemu_load_state(handle->emu, blob.data(), blob.size()) != 1) return 0;
    handle->hasRenderedOnce = false;
    return 1;
}

// ============================================================================
// fx-991CW ClassWiz II
// ============================================================================
CasioCw2Handle* casio_cw2_create(
    const uint8_t* rom_data, size_t rom_size,
    const uint8_t* ram_data, size_t ram_size)
{
    if (!rom_data || rom_size == 0) return nullptr;
    CasioCw2Core* core = casio_cw2_core_create(rom_data, rom_size, ram_data, ram_size);
    if (!core) return nullptr;

    // Boot CW2 cleanly past initial self-test into ready state
    casio_cw2_core_advance(core, 450);

    auto* h = new CasioCw2Handle();
    h->core = core;
    h->grayBuf.resize(static_cast<size_t>(casio_cw2_core_screen_width(core)) *
                      static_cast<size_t>(casio_cw2_core_screen_height(core)));
    return h;
}

CasioCw2Handle* casio_cw2_create_from_files(const char* rom_path, const char* ram_path)
{
    if (!rom_path || !rom_path[0]) return nullptr;
    std::vector<uint8_t> rom;
    if (!ReadBinaryFile(rom_path, rom)) return nullptr;

    std::vector<uint8_t> ram;
    if (ram_path && ram_path[0]) {
        (void)ReadBinaryFile(ram_path, ram);
    }
    return casio_cw2_create(
        rom.data(), rom.size(),
        ram.empty() ? nullptr : ram.data(), ram.size());
}

void casio_cw2_destroy(CasioCw2Handle* handle)
{
    if (!handle) return;
    if (handle->core) casio_cw2_core_destroy(handle->core);
    delete handle;
}

void casio_cw2_power_on(CasioCw2Handle* handle)
{
    if (!handle || !handle->core) return;
    casio_cw2_core_power_on(handle->core);
    handle->hasRenderedOnce = false;
}

void casio_cw2_advance(CasioCw2Handle* handle, uint32_t ms)
{
    if (!handle || !handle->core || ms == 0) return;
    casio_cw2_core_advance(handle->core, ms);
}

void casio_cw2_key_down(CasioCw2Handle* handle, uint32_t matrix_code)
{
    if (!handle || !handle->core) return;
    if ((matrix_code & 0xFFu) == 0xFFu) {
        casio_cw2_core_power_on(handle->core);
        handle->hasRenderedOnce = false;
        return;
    }
    casio_cw2_core_key_down(handle->core, static_cast<uint8_t>(matrix_code & 0xFFu));
}

void casio_cw2_key_up(CasioCw2Handle* handle)
{
    if (!handle || !handle->core) return;
    casio_cw2_core_key_up(handle->core);
}

int casio_cw2_tap_key(CasioCw2Handle* handle, uint32_t matrix_code, uint32_t hold_ms, uint32_t settle_ms)
{
    if (!handle || !handle->core) return 0;
    if ((matrix_code & 0xFFu) == 0xFFu) {
        casio_cw2_core_power_on(handle->core);
        if (settle_ms > 0) casio_cw2_core_advance(handle->core, settle_ms);
        handle->hasRenderedOnce = false;
        return 1;
    }
    return casio_cw2_core_tap_key(
        handle->core, static_cast<uint8_t>(matrix_code & 0xFFu), hold_ms, settle_ms);
}

int casio_cw2_screen_width(const CasioCw2Handle* handle)
{
    return (handle && handle->core) ? casio_cw2_core_screen_width(handle->core) : 0;
}

int casio_cw2_screen_height(const CasioCw2Handle* handle)
{
    return (handle && handle->core) ? casio_cw2_core_screen_height(handle->core) : 0;
}

int casio_cw2_render_rgba(
    CasioCw2Handle* handle,
    uint8_t* out_rgba,
    int out_size,
    uint32_t* out_indicators,
    int force_copy)
{
    if (!handle || !handle->core || !out_rgba) return -1;
    const int w = casio_cw2_core_screen_width(handle->core);
    const int h = casio_cw2_core_screen_height(handle->core);
    const int needed = w * h * 4;
    if (w <= 0 || h <= 0 || out_size < needed) return -1;

    const size_t pxCount = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (handle->grayBuf.size() < pxCount) handle->grayBuf.resize(pxCount);
    if (casio_cw2_core_copy_gray_levels(handle->core, handle->grayBuf.data(), pxCount) != pxCount) {
        return -1;
    }

    (void)casio_cw2_core_take_redraw_flag(handle->core);
    const uint32_t indMask = casio_cw2_core_indicators(handle->core);
    if (out_indicators) *out_indicators = indMask;

    const uint64_t hash = Fnv1a64(handle->grayBuf.data(), pxCount) ^ (static_cast<uint64_t>(indMask) << 32);
    if (handle->hasRenderedOnce && force_copy == 0 && hash == handle->lastFrameHash && indMask == handle->lastIndicators) {
        return 0;
    }

    handle->hasRenderedOnce = true;
    handle->lastFrameHash = hash;
    handle->lastIndicators = indMask;

    for (size_t i = 0; i < pxCount; ++i) {
        const uint8_t lvl = handle->grayBuf[i] & 0x03u;
        uint8_t* p = out_rgba + i * 4u;
        p[0] = kCw2Palette[lvl][0];
        p[1] = kCw2Palette[lvl][1];
        p[2] = kCw2Palette[lvl][2];
        p[3] = 255u;
    }
    return 1;
}

size_t casio_cw2_export_ram(const CasioCw2Handle* handle, uint8_t* out_buf, size_t out_size)
{
    if (!handle || !handle->core) return 0;
    return casio_cw2_core_export_ram(handle->core, out_buf, out_size);
}

int casio_cw2_import_ram(CasioCw2Handle* handle, const uint8_t* buf, size_t size)
{
    if (!handle || !handle->core || !buf || size == 0) return 0;
    const int ok = casio_cw2_core_import_ram(handle->core, buf, size);
    if (ok == 1) {
        handle->hasRenderedOnce = false;
    }
    return ok;
}

int casio_cw2_export_ram_file(const CasioCw2Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    const size_t sz = casio_cw2_core_ram_size(handle->core);
    if (sz == 0) return 0;
    std::vector<uint8_t> buf(sz);
    if (casio_cw2_core_export_ram(handle->core, buf.data(), buf.size()) != sz) return 0;
    return WriteBinaryFileAtomic(path, buf.data(), buf.size()) ? 1 : 0;
}

int casio_cw2_import_ram_file(CasioCw2Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    std::vector<uint8_t> buf;
    if (!ReadBinaryFile(path, buf)) return 0;
    if (buf.size() != casio_cw2_core_ram_size(handle->core)) return 0;
    return casio_cw2_import_ram(handle, buf.data(), buf.size());
}

// ============================================================================
// fx-CG50 PRIZM
// ============================================================================
CasioCg50Handle* casio_cg50_create_from_dir(const char* dir_path)
{
    if (!dir_path) return nullptr;
    const std::string dir(dir_path);

    std::vector<uint8_t> sysRom;
    std::vector<uint8_t> userRom;
    std::vector<uint8_t> c2Rom;
    std::vector<uint8_t> ilramSeed;

    if (!ReadBinaryFile(JoinPath(dir, "SystemROM.raw"), sysRom)) return nullptr;
    (void)ReadBinaryFile(JoinPath(dir, "UserROM.raw"), userRom);
    (void)ReadBinaryFile(JoinPath(dir, "C2ROM.raw"), c2Rom);
    (void)ReadBinaryFile(JoinPath(dir, "official-ILRAM-FD800000-4000.bin"), ilramSeed);

    auto core = std::make_unique<Cg50NativeCore>();
    if (!ilramSeed.empty()) {
        core->SetIlramSeed(ilramSeed.data(), ilramSeed.size());
    }
    if (!core->LoadFirmwareParts(
            sysRom.data(), sysRom.size(),
            userRom.empty() ? nullptr : userRom.data(), userRom.size())) {
        return nullptr;
    }
    if (!c2Rom.empty()) {
        core->LoadC2Rom(c2Rom.data(), c2Rom.size());
    }

    auto* h = new CasioCg50Handle();
    h->core = std::move(core);
    return h;
}

void casio_cg50_destroy(CasioCg50Handle* handle)
{
    delete handle;
}

int casio_cg50_step(CasioCg50Handle* handle, uint64_t run_budget, uint64_t sleep_cycles)
{
    if (!handle || !handle->core) return 0;
    auto& core = *handle->core;

    if (core.GetState() == Cg50NativeCore::State::Running && run_budget > 0) {
        core.RunInstructions(run_budget);
    }
    if (core.GetState() == Cg50NativeCore::State::Sleeping && sleep_cycles > 0) {
        constexpr uint64_t kChunk = 2048;
        uint64_t rem = sleep_cycles;
        while (rem > 0) {
            const uint64_t c = rem < kChunk ? rem : kChunk;
            if (core.AdvanceCycles(c)) break;
            rem -= c;
        }
    }
    return static_cast<int>(core.GetState());
}

int casio_cg50_key_down(CasioCg50Handle* handle, uint16_t matrix_code)
{
    if (!handle || !handle->core) return 0;
    if (!handle->core->KeyDown(matrix_code)) return 0;
    if (std::find(handle->pressedKeys.begin(), handle->pressedKeys.end(), matrix_code) ==
        handle->pressedKeys.end()) {
        handle->pressedKeys.push_back(matrix_code);
    }
    return 1;
}

void casio_cg50_key_up(CasioCg50Handle* handle, uint16_t matrix_code)
{
    if (!handle || !handle->core) return;
    handle->core->KeyUp(matrix_code);
    auto it = std::remove(handle->pressedKeys.begin(), handle->pressedKeys.end(), matrix_code);
    handle->pressedKeys.erase(it, handle->pressedKeys.end());
}

void casio_cg50_release_all_keys(CasioCg50Handle* handle)
{
    if (!handle || !handle->core) return;
    auto copy = handle->pressedKeys;
    for (uint16_t k : copy) {
        handle->core->KeyUp(k);
    }
    handle->pressedKeys.clear();
}

int casio_cg50_get_stats(
    const CasioCg50Handle* handle,
    uint64_t* out_instructions,
    uint64_t* out_cycles,
    uint32_t* out_pc,
    uint64_t* out_lcd_writes)
{
    if (!handle || !handle->core) return -1;
    const auto& s = handle->core->GetStats();
    const auto& r = handle->core->GetRegisters();
    if (out_instructions) *out_instructions = s.instructions;
    if (out_cycles) *out_cycles = s.cycles;
    if (out_pc) *out_pc = r.pc;
    if (out_lcd_writes) *out_lcd_writes = handle->core->Peripherals().Lcd().ObservedWrites().size();
    return static_cast<int>(handle->core->GetState());
}

int casio_cg50_render_rgba_ex(
    CasioCg50Handle* handle,
    uint8_t* out_rgba,
    int out_size,
    int force_copy)
{
    if (!handle || !handle->core || !out_rgba) return -1;
    constexpr int kW = static_cast<int>(cg50::R61524::kVisibleWidth);
    constexpr int kH = static_cast<int>(cg50::R61524::kVisibleHeight);
    constexpr size_t kPixels = static_cast<size_t>(kW) * static_cast<size_t>(kH);
    if (out_size < static_cast<int>(kPixels * 4u)) return -1;

    const auto& lcd = handle->core->Peripherals().Lcd();
    if (lcd.FramebufferAvailable()) {
        const uint16_t* fb = lcd.FramebufferData();
        if (fb != nullptr) {
            const uint64_t hash = Fnv1a64(fb, kPixels * sizeof(uint16_t));
            if (force_copy == 0 && handle->hasFrameHash && hash == handle->lastFrameHash) {
                return 0;
            }
            handle->hasFrameHash = true;
            handle->lastFrameHash = hash;
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    const uint16_t px = fb[static_cast<size_t>(kH - 1 - y) * kW + (kW - 1 - x)];
                    WriteRgb565ToRgba(px, out_rgba + (static_cast<size_t>(y) * kW + x) * 4u);
                }
            }
            return 1;
        }
    }

    // Fallback to DMA scanout VRAM buffer (SAR0 or 0x08000000 / 0x08028800) if active
    uint32_t sar0 = 0;
    uint32_t vramBase = 0x08000000u;
    if (handle->core->Read32(0xFE008020u, sar0) &&
        (sar0 == 0x08000000u || sar0 == 0x08028800u)) {
        vramBase = sar0;
    }
    uint16_t testWord = 0;
    bool hasNonEmpty = false;
    if (handle->core->Read16(vramBase, testWord)) {
        uint64_t h = 14695981039346656037ULL;
        std::vector<uint16_t> tmp(kPixels);
        for (int y = 0; y < kH; ++y) {
            for (int x = 0; x < kW; ++x) {
                uint16_t px = 0xFFFFu;
                (void)handle->core->Read16(
                    vramBase + (static_cast<uint32_t>(kH - 1 - y) * kW + (kW - 1 - x)) * 2u, px);
                if (px != 0u && px != 0xFFFFu) hasNonEmpty = true;
                tmp[static_cast<size_t>(y) * kW + x] = px;
                h ^= static_cast<uint64_t>(px);
                h *= 1099511628211ULL;
            }
        }
        if (!hasNonEmpty) return 0;
        if (force_copy == 0 && handle->hasFrameHash && h == handle->lastFrameHash) {
            return 0;
        }
        handle->hasFrameHash = true;
        handle->lastFrameHash = h;
        for (size_t i = 0; i < kPixels; ++i) {
            WriteRgb565ToRgba(tmp[i], out_rgba + i * 4u);
        }
        return 1;
    }

    std::memset(out_rgba, 0xFF, kPixels * 4u);
    return 0;
}

int casio_cg50_render_rgba(
    CasioCg50Handle* handle,
    uint8_t* out_rgba,
    int out_size)
{
    return casio_cg50_render_rgba_ex(handle, out_rgba, out_size, 0);
}

size_t casio_cg50_export_state(const CasioCg50Handle* handle, uint8_t* out_buf, size_t out_size)
{
    if (!handle || !handle->core) return 0;
    std::vector<uint8_t> blob;
    if (!handle->core->ExportState(blob) || blob.empty()) return 0;
    if (!out_buf || out_size < blob.size()) return blob.size();
    std::memcpy(out_buf, blob.data(), blob.size());
    return blob.size();
}

int casio_cg50_import_state(CasioCg50Handle* handle, const uint8_t* buf, size_t size)
{
    if (!handle || !handle->core || !buf || size == 0) return 0;
    const bool ok = handle->core->ImportState(buf, size);
    if (ok) {
        handle->hasFrameHash = false;
        handle->pressedKeys.clear();
    }
    return ok ? 1 : 0;
}

int casio_cg50_export_state_file(const CasioCg50Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    std::vector<uint8_t> blob;
    if (!handle->core->ExportState(blob) || blob.empty()) return 0;
    return WriteBinaryFileAtomic(path, blob.data(), blob.size()) ? 1 : 0;
}

int casio_cg50_import_state_file(CasioCg50Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    std::vector<uint8_t> blob;
    if (!ReadBinaryFile(path, blob) || blob.size() < 64) return 0;
    return casio_cg50_import_state(handle, blob.data(), blob.size());
}

int casio_cg50_set_rtc_epoch(CasioCg50Handle* handle, int64_t local_unix_seconds)
{
    if (!handle || !handle->core) return 0;
    handle->core->SetRtcEpochSeconds(local_unix_seconds);
    return 1;
}

int casio_cg50_export_user_rom_file(const CasioCg50Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    if (!handle->core->HasUserRom() || handle->core->UserRomData() == nullptr) return 0;
    return WriteBinaryFileAtomic(
               path, handle->core->UserRomData(), handle->core->UserRomSize())
               ? 1
               : 0;
}

int casio_cg50_import_user_rom_file(CasioCg50Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    std::vector<uint8_t> buf;
    if (!ReadBinaryFile(path, buf)) return 0;
    if (buf.size() != Cg50NativeCore::kUserRomSize) return 0;
    const bool ok = handle->core->ReplaceUserRom(buf.data(), buf.size());
    if (ok) {
        handle->hasFrameHash = false;
        handle->pressedKeys.clear();
    }
    return ok ? 1 : 0;
}

int casio_cg50_load_addin_file(CasioCg50Handle* handle, const char* path)
{
    if (!handle || !handle->core || !path || !path[0]) return 0;
    std::vector<uint8_t> buf;
    if (!ReadBinaryFile(path, buf)) return 0;
    return handle->core->LoadG3aAddin(buf.data(), buf.size()) ? 1 : 0;
}

} // extern "C"
