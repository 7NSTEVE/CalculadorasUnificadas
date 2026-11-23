#include "casio_ffi.h"

#include <cstdio>
#include <string>
#include <vector>

static int Fail(const char* msg)
{
    std::fprintf(stderr, "[nx_regression_test] FAIL: %s\n", msg);
    return 1;
}

static int TestModel(int hwId, const std::string& romPath, int expW, int expH, const char* label)
{
    CasioNxHandle* h = casio_nx_create_from_file(hwId, romPath.c_str());
    if (!h) return Fail("casio_nx_create_from_file returned null");

    if (casio_nx_screen_width(h) != expW || casio_nx_screen_height(h) != expH) {
        casio_nx_destroy(h);
        return Fail("unexpected NX screen dimensions");
    }

    const int fbBytes = expW * expH * 4;
    std::vector<uint8_t> before(fbBytes, 0);
    std::vector<uint8_t> after(fbBytes, 0);
    uint32_t ind = 0;

    // SHIFT activates bit 0
    if (casio_nx_tap_key(h, 0x07, 150, 150) != 1) {
        casio_nx_destroy(h);
        return Fail("SHIFT tap failed");
    }
    casio_nx_render_rgba(h, before.data(), fbBytes, &ind, 1);
    if ((ind & 1u) == 0u) {
        casio_nx_destroy(h);
        return Fail("SHIFT bit 0 not set");
    }

    // Cancel SHIFT + AC
    casio_nx_tap_key(h, 0x07, 150, 150);
    casio_nx_tap_key(h, 0x42, 150, 150);
    casio_nx_render_rgba(h, before.data(), fbBytes, &ind, 1);
    if ((ind & 1u) != 0u) {
        casio_nx_destroy(h);
        return Fail("SHIFT bit 0 did not clear");
    }

    // 1 + 2 =
    casio_nx_tap_key(h, 0x00, 150, 150);
    casio_nx_tap_key(h, 0x30, 150, 150);
    casio_nx_tap_key(h, 0x10, 150, 150);
    casio_nx_tap_key(h, 0x60, 150, 150);
    casio_nx_render_rgba(h, after.data(), fbBytes, &ind, 1);
    if (before == after) {
        casio_nx_destroy(h);
        return Fail("1+2= did not change NX framebuffer");
    }

    // State save/load roundtrip + negative guard
    const std::string statePath = std::string(CASIO_ROMS_DIR) + "/_test_nx_" + std::to_string(hwId) + ".snp";
    if (casio_nx_save_state_file(h, statePath.c_str()) != 1) {
        casio_nx_destroy(h);
        return Fail("casio_nx_save_state_file failed");
    }

    CasioNxHandle* h2 = casio_nx_create_from_file(hwId, romPath.c_str());
    if (!h2) {
        std::remove(statePath.c_str());
        casio_nx_destroy(h);
        return Fail("second create_from_file failed");
    }
    if (casio_nx_load_state_file(h2, statePath.c_str()) != 1) {
        std::remove(statePath.c_str());
        casio_nx_destroy(h2);
        casio_nx_destroy(h);
        return Fail("casio_nx_load_state_file failed");
    }
    std::vector<uint8_t> restored(fbBytes, 0);
    uint32_t ind2 = 0;
    casio_nx_render_rgba(h2, restored.data(), fbBytes, &ind2, 1);
    if (restored != after) {
        std::remove(statePath.c_str());
        casio_nx_destroy(h2);
        casio_nx_destroy(h);
        return Fail("restored NX framebuffer does not match saved state");
    }

    // Negative: corrupt file rejected
    const std::string badPath = std::string(CASIO_ROMS_DIR) + "/_test_nx_bad.snp";
    if (FILE* f = std::fopen(badPath.c_str(), "wb")) {
        const char x[32] = "CORRUPT_NX_SNAPSHOT";
        std::fwrite(x, 1, sizeof(x), f);
        std::fclose(f);
    }
    if (casio_nx_load_state_file(h2, badPath.c_str()) != 0) {
        std::remove(statePath.c_str());
        std::remove(badPath.c_str());
        casio_nx_destroy(h2);
        casio_nx_destroy(h);
        return Fail("corrupt NX state file was NOT rejected");
    }

    std::remove(statePath.c_str());
    std::remove(badPath.c_str());
    casio_nx_destroy(h2);
    casio_nx_destroy(h);
    std::printf("[nx_regression_test] %s PASSED\n", label);
    return 0;
}

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
