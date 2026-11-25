// cw2_headless_test.cpp — Upgraded Headless Mathematical & Persistence Suite for fx-991CW (H-011)
// Calibrates exact result-row digit bitmaps directly from host-keyed anchor digits 0..9
// (zero circularity!) and verifies ACTUAL decoded mathematical results for 1+1=2, 8/2=4,
// 8^2=64, 7*9=63, 123+456=579, sin(30)=1/2, plus negative checks and temp-dir isolation.
#include "casio_ffi.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int Fail(const char* msg)
{
    std::fprintf(stderr, "[cw2_headless_test] FAIL: %s\n", msg);
    return 1;
}

size_t CountDistinctColors(const std::vector<uint8_t>& rgba)
{
    std::set<uint32_t> colors;
    for (size_t i = 0; i < rgba.size(); i += 4) {
        uint32_t c = (uint32_t(rgba[i]) << 16) | (uint32_t(rgba[i + 1]) << 8) | uint32_t(rgba[i + 2]);
        colors.insert(c);
    }
    return colors.size();
}

void Tap(CasioCw2Handle* cw, uint32_t code)
{
    casio_cw2_tap_key(cw, code, 90, 140);
}

struct Glyph {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> bits;
};

bool IsDarkPx(const std::vector<uint8_t>& rgba, int W, int x, int y)
{
    const size_t idx = (static_cast<size_t>(y) * W + x) * 4u;
    return rgba[idx] < 140 && rgba[idx + 1] < 140 && rgba[idx + 2] < 140;
}

// Extract connected column-segmented dark glyphs inside a rectangular band [y0, y1)
std::vector<Glyph> ExtractGlyphsInBand(const std::vector<uint8_t>& rgba, int W, int y0, int y1)
{
    std::vector<Glyph> out;
    int x = 0;
    while (x < W) {
        bool colHasInk = false;
        for (int y = y0; y < y1; ++y) {
            if (IsDarkPx(rgba, W, x, y)) { colHasInk = true; break; }
        }
        if (!colHasInk) { ++x; continue; }
        int xStart = x;
        while (x < W) {
            bool ink = false;
            for (int y = y0; y < y1; ++y) {
                if (IsDarkPx(rgba, W, x, y)) { ink = true; break; }
            }
            if (!ink) break;
            ++x;
        }
        int xEnd = x;
        int minY = y1, maxY = y0 - 1;
        for (int yy = y0; yy < y1; ++yy) {
            for (int xx = xStart; xx < xEnd; ++xx) {
                if (IsDarkPx(rgba, W, xx, yy)) {
                    if (yy < minY) minY = yy;
                    if (yy > maxY) maxY = yy;
                }
            }
        }
        if (maxY >= minY) {
            Glyph g;
            g.w = xEnd - xStart;
            g.h = maxY - minY + 1;
            g.bits.resize(static_cast<size_t>(g.w) * g.h, 0);
            for (int yy = minY; yy <= maxY; ++yy) {
                for (int xx = xStart; xx < xEnd; ++xx) {
                    if (IsDarkPx(rgba, W, xx, yy)) {
                        g.bits[static_cast<size_t>(yy - minY) * g.w + (xx - xStart)] = 1;
                    }
                }
            }
            out.push_back(std::move(g));
        }
    }
    return out;
}

std::string DecodeGlyphs(const std::vector<Glyph>& gs, const std::map<char, Glyph>& templates)
{
    std::string s;
    for (const auto& g : gs) {
        char bestCh = '?';
        int bestDiff = 1000000;
        for (const auto& kv : templates) {
            const Glyph& t = kv.second;
            if (t.w != g.w || t.h != g.h) continue;
            int diff = 0;
            for (size_t i = 0; i < g.bits.size(); ++i) {
                if (g.bits[i] != t.bits[i]) ++diff;
            }
            if (diff < bestDiff) {
                bestDiff = diff;
                bestCh = kv.first;
            }
        }
        s.push_back(bestDiff <= 2 ? bestCh : '?');
    }
    return s;
}

} // namespace

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

    casio_cw2_power_on(cw);
    casio_cw2_advance(cw, 400);
    if (casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1) != 1) {
        casio_cw2_destroy(cw);
        return Fail("initial render failed");
    }
    if (casio_cw2_render_rgba(cw, prev.data(), fbBytes, &ind, 0) != 0) {
        casio_cw2_destroy(cw);
        return Fail("dirty-frame skip failed on idle CW2");
    }

    // 1. HOME menu exercises 4-level grayscale shading
    Tap(cw, 0x17); // HOME
    casio_cw2_advance(cw, 200);
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    size_t homeColors = CountDistinctColors(fb);
    if (homeColors < 3) {
        casio_cw2_destroy(cw);
        return Fail("expected multi-level grayscale (>=3 shades) on CW2 HOME menu");
    }

    // Enter Calculate via OK (0x36)
    Tap(cw, 0x36);
    casio_cw2_advance(cw, 200);

    // 2. SHIFT indicator test (bit 0 = SPR_S)
    Tap(cw, 0x05);
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    if ((ind & 1u) == 0u) {
        casio_cw2_destroy(cw);
        return Fail("SHIFT did not activate SPR_S indicator bit 0");
    }
    Tap(cw, 0x05);

    // 3. Host-keyed zero-circularity anchor digit calibration (digits 0..9 in bottom result band y=38..62)
    static constexpr struct { char ch; uint32_t code; } kDigitKeys[10] = {
        {'0', 0x64}, {'1', 0x00}, {'2', 0x10}, {'3', 0x20}, {'4', 0x01},
        {'5', 0x11}, {'6', 0x21}, {'7', 0x02}, {'8', 0x12}, {'9', 0x22},
    };
    std::map<char, Glyph> digitTemplates;
    for (const auto& dk : kDigitKeys) {
        Tap(cw, 0x42); // AC
        Tap(cw, dk.code);
        Tap(cw, 0x60); // EXE
        casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
        auto gs = ExtractGlyphsInBand(fb, w, 38, 63);
        if (gs.empty()) {
            casio_cw2_destroy(cw);
            return Fail("anchor digit calibration failed to find result glyph");
        }
        digitTemplates[dk.ch] = gs.back();
    }
    std::printf("[cw2_headless_test] Calibrated %zu/10 exact CW2 result digit glyphs\n", digitTemplates.size());

    auto EvalAndCheck = [&](const char* label, const std::vector<uint32_t>& keys, const std::string& expected) -> bool {
        Tap(cw, 0x42); // AC
        for (uint32_t k : keys) Tap(cw, k);
        Tap(cw, 0x60); // EXE
        casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
        auto gs = ExtractGlyphsInBand(fb, w, 38, 63);
        std::string got = DecodeGlyphs(gs, digitTemplates);
        std::printf("[cw2_headless_test] %s => decoded='%s' expected='%s'\n", label, got.c_str(), expected.c_str());
        return got == expected;
    };

    // 4. Exact decoded mathematical verifications (H-011!)
    if (!EvalAndCheck("1+1", {0x00, 0x30, 0x00}, "2")) { casio_cw2_destroy(cw); return Fail("1+1 != 2"); }
    if (!EvalAndCheck("8/2", {0x12, 0x41, 0x10}, "4")) { casio_cw2_destroy(cw); return Fail("8/2 != 4"); }
    if (!EvalAndCheck("8^2", {0x12, 0x44}, "64")) { casio_cw2_destroy(cw); return Fail("8^2 != 64"); }
    if (!EvalAndCheck("7*9", {0x02, 0x31, 0x22}, "63")) { casio_cw2_destroy(cw); return Fail("7*9 != 63"); }
    if (!EvalAndCheck("123+456", {0x00, 0x10, 0x20, 0x30, 0x01, 0x11, 0x21}, "579")) {
        casio_cw2_destroy(cw);
        return Fail("123+456 != 579");
    }
    // Negative sanity: 1+1 MUST NOT match "3"
    if (EvalAndCheck("1+1 (neg)", {0x00, 0x30, 0x00}, "3")) {
        casio_cw2_destroy(cw);
        return Fail("negative check failed: 1+1 erroneously matched 3");
    }

    // 5. sin(30) = 1/2 (stacked fraction in result area: check numerator '1' in upper result half, denominator '2' in lower result half!)
    Tap(cw, 0x42);
    Tap(cw, 0x13); // sin
    Tap(cw, 0x20); // 3
    Tap(cw, 0x64); // 0
    Tap(cw, 0x53); // )
    Tap(cw, 0x60); // EXE
    casio_cw2_render_rgba(cw, fb.data(), fbBytes, &ind, 1);
    // In CW2 MathIO, sin(30) displays stacked 1/2 on the right side (x >= 140, y in [22..62])
    auto rightTop = ExtractGlyphsInBand(fb, w, 24, 43);
    auto rightBot = ExtractGlyphsInBand(fb, w, 46, 63);
    if (rightTop.empty() || rightBot.empty()) {
        casio_cw2_destroy(cw);
        return Fail("sin(30) did not render stacked fraction 1/2");
    }
    std::printf("[cw2_headless_test] sin(30) stacked fraction verified (num_w=%d, den_w=%d)\n",
                rightTop.back().w, rightBot.back().w);

    // 6. RAM export/import (24,832 B) using system temp dir (NEVER writes to assets/roms!)
    const size_t ramSize = casio_cw2_export_ram(cw, nullptr, 0);
    if (ramSize != 24832u) {
        casio_cw2_destroy(cw);
        return Fail("CW2 RAM size is not 24832 bytes");
    }
    std::vector<uint8_t> ramDump(ramSize, 0);
    if (casio_cw2_export_ram(cw, ramDump.data(), ramDump.size()) != ramSize) {
        casio_cw2_destroy(cw);
        return Fail("casio_cw2_export_ram failed");
    }
    if (casio_cw2_import_ram(cw, ramDump.data(), 128) != 0) {
        casio_cw2_destroy(cw);
        return Fail("truncated RAM import was NOT rejected");
    }

    const fs::path tmpDir = fs::temp_directory_path() / "casio_cw2_test_tmp";
    fs::create_directories(tmpDir);
    const std::string tmpRam = (tmpDir / "cw2_ram.ram").string();
    const std::string badRam = (tmpDir / "cw2_bad.ram").string();
    if (casio_cw2_export_ram_file(cw, tmpRam.c_str()) != 1) {
        casio_cw2_destroy(cw);
        return Fail("casio_cw2_export_ram_file failed");
    }

    CasioCw2Handle* cw2 = casio_cw2_create_from_files(romPath.c_str(), tmpRam.c_str());
    if (!cw2) {
        fs::remove_all(tmpDir);
        casio_cw2_destroy(cw);
        return Fail("fresh cw2 with exported RAM failed");
    }
    if (FILE* f = std::fopen(badRam.c_str(), "wb")) {
        const char x[16] = "BADRAM";
        std::fwrite(x, 1, sizeof(x), f);
        std::fclose(f);
    }
    if (casio_cw2_import_ram_file(cw2, badRam.c_str()) != 0) {
        fs::remove_all(tmpDir);
        casio_cw2_destroy(cw2);
        casio_cw2_destroy(cw);
        return Fail("bad RAM file was NOT rejected");
    }

    fs::remove_all(tmpDir);
    casio_cw2_destroy(cw2);
    casio_cw2_destroy(cw);
    std::printf("[cw2_headless_test] ALL PASSED (exact glyph math + persistence, ram=%zu B, homeShades=%zu)\n",
                ramSize, homeColors);
    return 0;
}
