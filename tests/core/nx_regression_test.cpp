// nx_regression_test.cpp — Upgraded Mathematical & Persistence Suite for fx-570LA X & fx-570VN PLUS (H-011)
// Calibrates exact result-row digit glyphs directly from host-keyed anchors 0..9, verifies
// decoded mathematical results (1+2=3, 7*8=56, 123+456=579), and writes temp snapshots ONLY
// under std::filesystem::temp_directory_path() (never touches assets/roms/).
#include "casio_ffi.h"

#include <cstdio>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int Fail(const char* msg)
{
    std::fprintf(stderr, "[nx_regression_test] FAIL: %s\n", msg);
    return 1;
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

std::vector<Glyph> ExtractGlyphsInBand(const std::vector<uint8_t>& rgba, int W, int y0, int y1)
{
    std::vector<Glyph> out;
    int x = 0;
    while (x < W) {
        bool colInk = false;
        for (int y = y0; y < y1; ++y) {
            if (IsDarkPx(rgba, W, x, y)) { colInk = true; break; }
        }
        if (!colInk) { ++x; continue; }
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

int TestModel(int hwId, const std::string& romPath, int expW, int expH, const char* label)
{
    CasioNxHandle* h = casio_nx_create_from_file(hwId, romPath.c_str());
    if (!h) return Fail("casio_nx_create_from_file returned null");

    if (casio_nx_screen_width(h) != expW || casio_nx_screen_height(h) != expH) {
        casio_nx_destroy(h);
        return Fail("unexpected NX screen dimensions");
    }

    const int fbBytes = expW * expH * 4;
    std::vector<uint8_t> fb(fbBytes, 0);
    uint32_t ind = 0;

    // SHIFT activates bit 0
    if (casio_nx_tap_key(h, 0x07, 150, 150) != 1) {
        casio_nx_destroy(h);
        return Fail("SHIFT tap failed");
    }
    casio_nx_render_rgba(h, fb.data(), fbBytes, &ind, 1);
    if ((ind & 1u) == 0u) {
        casio_nx_destroy(h);
        return Fail("SHIFT bit 0 not set");
    }
    casio_nx_tap_key(h, 0x07, 150, 150);
    casio_nx_tap_key(h, 0x42, 150, 150);
    casio_nx_render_rgba(h, fb.data(), fbBytes, &ind, 1);
    if ((ind & 1u) != 0u) {
        casio_nx_destroy(h);
        return Fail("SHIFT bit 0 did not clear");
    }
    // Ensure calculator is awake after SHIFT/AC sequence (SHIFT+AC powers off on ES/ClassWiz)
    casio_nx_power_on(h);
    casio_nx_advance(h, 250);
    casio_nx_tap_key(h, 0x42, 150, 150);

    // Calibrate result-row digit bitmaps from host-keyed anchor digits 0..9 (zero circularity)
    // Note: calcemu.cpp documents fx-570VN PLUS (ES PLUS) reliable hold window as 350..500 ms.
    const uint32_t kHold = (hwId == 3) ? 380u : 180u;
    const uint32_t kSettle = (hwId == 3) ? 320u : 220u;
    const int resY0 = (expH >= 60) ? 32 : 14;
    static constexpr struct { char ch; uint32_t code; } kDigitKeys[10] = {
        {'0', 0x64}, {'1', 0x00}, {'2', 0x10}, {'3', 0x20}, {'4', 0x01},
        {'5', 0x11}, {'6', 0x21}, {'7', 0x02}, {'8', 0x12}, {'9', 0x22},
    };
    std::map<char, Glyph> digitTemplates;
    for (const auto& dk : kDigitKeys) {
        casio_nx_tap_key(h, 0x42, kHold, kSettle); // AC
        casio_nx_tap_key(h, dk.code, kHold, kSettle);
        casio_nx_tap_key(h, 0x60, kHold, kSettle + 80u); // =
        casio_nx_render_rgba(h, fb.data(), fbBytes, &ind, 1);
        auto gs = ExtractGlyphsInBand(fb, expW, resY0, expH);
        if (gs.empty()) {
            int totalInk = 0;
            for (int y = 0; y < expH; ++y)
                for (int x = 0; x < expW; ++x)
                    if (IsDarkPx(fb, expW, x, y)) ++totalInk;
            std::fprintf(stderr, "[nx_regression_test] diag %s digit='%c' resY0=%d totalInk=%d\n",
                         label, dk.ch, resY0, totalInk);
            casio_nx_destroy(h);
            return Fail("NX anchor digit calibration failed");
        }
        digitTemplates[dk.ch] = gs.back();
    }

    auto EvalCheck = [&](const char* expr, const std::vector<uint32_t>& keys, const std::string& expected) -> bool {
        casio_nx_tap_key(h, 0x42, kHold, kSettle); // AC
        for (uint32_t k : keys) casio_nx_tap_key(h, k, kHold, kSettle);
        casio_nx_tap_key(h, 0x60, kHold, kSettle + 450u); // = (allow full ES PLUS math render)
        casio_nx_render_rgba(h, fb.data(), fbBytes, &ind, 1);
        auto gs = ExtractGlyphsInBand(fb, expW, resY0, expH);
        std::string got = DecodeGlyphs(gs, digitTemplates);
        std::printf("[nx_regression_test] %s | %s => decoded='%s' expected='%s'\n",
                    label, expr, got.c_str(), expected.c_str());
        if (got != expected && std::string(expr).find("neg") == std::string::npos) {
            std::printf("[nx_regression_test] ASCII dump %s (%dx%d) resY0=%d:\n", label, expW, expH, resY0);
            for (int y = 0; y < expH; ++y) {
                std::string row(expW, '.');
                for (int x = 0; x < expW; ++x) if (IsDarkPx(fb, expW, x, y)) row[x] = '#';
                if (row.find('#') != std::string::npos) std::printf("  y=%02d: %s\n", y, row.c_str());
            }
        }
        return got == expected;
    };

    if (!EvalCheck("1+2", {0x00, 0x30, 0x10}, "3")) { casio_nx_destroy(h); return Fail("1+2 != 3"); }
    if (!EvalCheck("7*8", {0x02, 0x31, 0x12}, "56")) { casio_nx_destroy(h); return Fail("7*8 != 56"); }
    if (!EvalCheck("123+456", {0x00, 0x10, 0x20, 0x30, 0x01, 0x11, 0x21}, "579")) {
        casio_nx_destroy(h);
        return Fail("123+456 != 579");
    }
    if (EvalCheck("1+2 (neg)", {0x00, 0x30, 0x10}, "4")) {
        casio_nx_destroy(h);
        return Fail("negative check failed: 1+2 matched 4");
    }

    std::vector<uint8_t> after = fb;

    // State save/load roundtrip using system temp directory (NEVER writes into assets/roms!)
    const fs::path tmpDir = fs::temp_directory_path() / "casio_nx_test_tmp";
    fs::create_directories(tmpDir);
    const std::string statePath = (tmpDir / ("nx_" + std::to_string(hwId) + ".snp")).string();
    const std::string badPath = (tmpDir / ("nx_bad_" + std::to_string(hwId) + ".snp")).string();

    if (casio_nx_save_state_file(h, statePath.c_str()) != 1) {
        fs::remove_all(tmpDir);
        casio_nx_destroy(h);
        return Fail("casio_nx_save_state_file failed");
    }

    CasioNxHandle* h2 = casio_nx_create_from_file(hwId, romPath.c_str());
    if (!h2) {
        fs::remove_all(tmpDir);
        casio_nx_destroy(h);
        return Fail("second create_from_file failed");
    }
    if (casio_nx_load_state_file(h2, statePath.c_str()) != 1) {
        fs::remove_all(tmpDir);
        casio_nx_destroy(h2);
        casio_nx_destroy(h);
        return Fail("casio_nx_load_state_file failed");
    }
    std::vector<uint8_t> restored(fbBytes, 0);
    uint32_t ind2 = 0;
    casio_nx_render_rgba(h2, restored.data(), fbBytes, &ind2, 1);
    if (restored != after) {
        fs::remove_all(tmpDir);
        casio_nx_destroy(h2);
        casio_nx_destroy(h);
        return Fail("restored NX framebuffer does not match saved state");
    }

    // Negative: corrupt file rejected
    if (FILE* f = std::fopen(badPath.c_str(), "wb")) {
        const char x[32] = "CORRUPT_NX_SNAPSHOT";
        std::fwrite(x, 1, sizeof(x), f);
        std::fclose(f);
    }
    if (casio_nx_load_state_file(h2, badPath.c_str()) != 0) {
        fs::remove_all(tmpDir);
        casio_nx_destroy(h2);
        casio_nx_destroy(h);
        return Fail("corrupt NX state file was NOT rejected");
    }

    fs::remove_all(tmpDir);
    casio_nx_destroy(h2);
    casio_nx_destroy(h);
    std::printf("[nx_regression_test] %s PASSED (exact glyph math + isolated temp persistence)\n", label);
    return 0;
}

} // namespace

int main()
{
    const std::string romDir = CASIO_ROMS_DIR;
    if (TestModel(4, romDir + "/fx991lax.rom", 192, 63, "fx-570LA X (ClassWiz)") != 0)
        return 1;
    if (TestModel(3, romDir + "/fx570vnplus.rom", 96, 31, "fx-570VN PLUS (ES PLUS)") != 0)
        return 1;
    std::printf("[nx_regression_test] ALL PASSED\n");
    return 0;
}
