#include "casio_ffi.h"

#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

static int Fail(const char* msg)
{
    std::fprintf(stderr, "[cw2_headless_test] FAIL: %s\n", msg);
    return 1;
}

static size_t CountDiffPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
{
    size_t n = 0;
    for (size_t i = 0; i < a.size(); i += 4) {
        if (a[i] != b[i] || a[i + 1] != b[i + 1] || a[i + 2] != b[i + 2]) n++;
    }
    return n;
}

static size_t CountDistinctColors(const std::vector<uint8_t>& rgba)
{
    std::set<uint32_t> colors;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        uint32_t c = (uint32_t(rgba[i]) << 16) | (uint32_t(rgba[i + 1]) << 8) | uint32_t(rgba[i + 2]);
        colors.insert(c);
    }
    return colors.size();
}

static void Tap(CasioCw2Handle* cw, uint32_t code)
{
    casio_cw2_tap_key(cw, code, 90, 140);
}

int main()
{
    const std::string romDir = CASIO_ROMS_DIR;
    const std::string romPath = romDir + "/fx991cw_rom.bin";
    const std::string ramPath = romDir + "/fx991cw_default_en.ram";

    CasioCw2Handle* cw = casio_cw2_create_from_files(romPath.c_str(), ramPath.c_str());
    if (!cw) return Fail("casio_cw2_create_from_files returned null");

    const int w = casio_cw2_screen_width(cw);
    const int h = casio_cw2_screen_height(cw);
    if (w != 192 || h != 63) {
        casio_cw2_destroy(cw);
        return Fail("unexpected CW2 LCD dimensions");
    }

    const int fbBytes = w * h * 4;
    std::vector<uint8_t> fb(fbBytes, 0);
    std::vector<uint8_t> prev(fbBytes, 0);
    uint32_t ind = 0;

    // Power on + settle
    casio_cw2_power_on(cw);
    casio_cw2_advance(cw, 400);
    int rc = casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    if (rc != 1) {
        casio_cw2_destroy(cw);
        return Fail("initial render failed");
    }
    // Idle dirty-frame check must return 0
    if (casio_cw2_render_rgba(cw, prev.data(), fbBytes, &ind, 0) != 0) {
        casio_cw2_destroy(cw);
        return Fail("dirty-frame skip failed on idle CW2");
    }

    // 1. HOME menu exercises 4-level grayscale shading (selected icon + unselected gray icons)
    Tap(cw, 0x17); // HOME
    casio_cw2_advance(cw, 200);
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    size_t homeColors = CountDistinctColors(fb);
    std::printf("[cw2_headless_test] HOME distinct gray colors: %zu, indicators=0x%05X\n", homeColors, ind);
    if (homeColors < 3) {
        casio_cw2_destroy(cw);
        return Fail("expected multi-level grayscale (>=3 shades) on CW2 HOME menu");
    }

    // Enter Calculate via OK (0x36)
    Tap(cw, 0x36);
    casio_cw2_advance(cw, 200);

    // 2. SHIFT indicator test (bit 0 = SPR_S)
    Tap(cw, 0x05); // SHIFT
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    std::printf("[cw2_headless_test] after SHIFT indicators=0x%05X\n", ind);
    if ((ind & 1u) == 0u) {
        casio_cw2_destroy(cw);
        return Fail("SHIFT did not activate SPR_S indicator bit 0");
    }
    Tap(cw, 0x05); // cancel SHIFT

    // 3. Test 1+1=2
    Tap(cw, 0x42); // AC
    casio_cw2_render_rgba(cw, prev.data(), fbBytes, &ind, 1);
    Tap(cw, 0x00); // 1
    Tap(cw, 0x30); // +
    Tap(cw, 0x00); // 1
    Tap(cw, 0x60); // EXE
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    size_t d1 = CountDiffPixels(prev, fb);
    std::printf("[cw2_headless_test] 1+1=EXE diff pixels=%zu, shades=%zu\n", d1, CountDistinctColors(fb));
    if (d1 < 30) { casio_cw2_destroy(cw); return Fail("1+1=EXE did not update display"); }

    // 4. Test 8 / 2 = 4
    prev = fb;
    Tap(cw, 0x42); // AC
    Tap(cw, 0x12); // 8
    Tap(cw, 0x41); // ÷
    Tap(cw, 0x10); // 2
    Tap(cw, 0x60); // EXE
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    size_t d2 = CountDiffPixels(prev, fb);
    std::printf("[cw2_headless_test] 8/2=EXE diff pixels=%zu\n", d2);
    if (d2 < 10) { casio_cw2_destroy(cw); return Fail("8/2=EXE did not update display"); }

    // 5. Test sin(30) = 1/2
    prev = fb;
    Tap(cw, 0x42); // AC
    Tap(cw, 0x13); // sin
    Tap(cw, 0x20); // 3
    Tap(cw, 0x64); // 0
    Tap(cw, 0x53); // )
    Tap(cw, 0x60); // EXE
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    size_t d3 = CountDiffPixels(prev, fb);
    std::printf("[cw2_headless_test] sin(30)=EXE diff pixels=%zu\n", d3);
    if (d3 < 20) { casio_cw2_destroy(cw); return Fail("sin(30)=EXE did not update display"); }

    // 6. Test 8^2 = 64
    prev = fb;
    Tap(cw, 0x42); // AC
    Tap(cw, 0x12); // 8
    Tap(cw, 0x44); // x^2
    Tap(cw, 0x60); // EXE
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    size_t d4 = CountDiffPixels(prev, fb);
    std::printf("[cw2_headless_test] 8^2=EXE diff pixels=%zu\n", d4);
    if (d4 < 10) { casio_cw2_destroy(cw); return Fail("8^2=EXE did not update display"); }

    // 7. RAM export/import (24,832 B) positive + negative guard
    const size_t ramSize = casio_cw2_export_ram(cw, nullptr, 0);
    if (ramSize != 24832u) {
        std::fprintf(stderr, "unexpected RAM size: %zu\n", ramSize);
        casio_cw2_destroy(cw);
        return Fail("CW2 RAM size is not 24832 bytes");
    }
    std::vector<uint8_t> ramDump(ramSize, 0);
    if (casio_cw2_export_ram(cw, ramDump.data(), ramDump.size()) != ramSize) {
        casio_cw2_destroy(cw);
        return Fail("casio_cw2_export_ram failed");
    }

    // Negative: wrong size rejected
    if (casio_cw2_import_ram(cw, ramDump.data(), 128) != 0) {
        casio_cw2_destroy(cw);
        return Fail("truncated RAM import was NOT rejected");
    }

    // File roundtrip into fresh instance
    const std::string tmpRam = romDir + "/_test_cw2_ram.ram";
    if (casio_cw2_export_ram_file(cw, tmpRam.c_str()) != 1) {
        casio_cw2_destroy(cw);
        return Fail("casio_cw2_export_ram_file failed");
    }

    CasioCw2Handle* cw2 = casio_cw2_create_from_files(romPath.c_str(), tmpRam.c_str());
    if (!cw2) {
        std::remove(tmpRam.c_str());
        casio_cw2_destroy(cw);
        return Fail("fresh cw2 with exported RAM failed");
    }
    std::vector<uint8_t> ramDump2(ramSize, 0);
    casio_cw2_export_ram(cw2, ramDump2.data(), ramDump2.size());
    // Also verify negative file import with truncated file
    const std::string badRam = romDir + "/_test_cw2_bad.ram";
    if (FILE* f = std::fopen(badRam.c_str(), "wb")) {
        const char x[16] = "BADRAM";
        std::fwrite(x, 1, sizeof(x), f);
        std::fclose(f);
    }
    if (casio_cw2_import_ram_file(cw2, badRam.c_str()) != 0) {
        std::remove(tmpRam.c_str());
        std::remove(badRam.c_str());
        casio_cw2_destroy(cw2);
        casio_cw2_destroy(cw);
        return Fail("bad RAM file was NOT rejected");
    }

    std::remove(tmpRam.c_str());
    std::remove(badRam.c_str());
    casio_cw2_destroy(cw2);
    casio_cw2_destroy(cw);

    std::printf("[cw2_headless_test] ALL PASSED (ram=%zu B, homeShades=%zu)\n", ramSize, homeColors);
    return 0;
}
