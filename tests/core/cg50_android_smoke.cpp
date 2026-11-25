// cg50_android_smoke.cpp — Upgraded CG50 FFI Smoke, Exact Result Glyph & Atomic Save Test (H-007 + H-011)
#include "casio_ffi.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int Fail(const char* msg)
{
    std::fprintf(stderr, "[cg50_android_smoke] FAIL: %s\n", msg);
    return 1;
}

void TapCgKey(CasioCg50Handle* cg, uint16_t code)
{
    casio_cg50_key_down(cg, code);
    casio_cg50_step(cg, 600000ULL, 8000ULL);
    casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_key_up(cg, code);
    casio_cg50_step(cg, 1500000ULL, 16000ULL);
    casio_cg50_step(cg, 1500000ULL, 16000ULL);
}

// Extract right-aligned result-row crop (x in [330..378), y in [42..72)) as monochrome ink vector
std::vector<uint8_t> ExtractFirstResultCrop(const std::vector<uint8_t>& rgba)
{
    constexpr int W = 384;
    std::vector<uint8_t> out;
    out.reserve(48 * 30);
    for (int y = 42; y < 72; ++y) {
        for (int x = 330; x < 378; ++x) {
            const size_t i = (static_cast<size_t>(y) * W + x) * 4u;
            const bool dark = (rgba[i] < 90 && rgba[i + 1] < 90 && rgba[i + 2] < 90);
            out.push_back(dark ? 1u : 0u);
        }
    }
    return out;
}

} // namespace

int main()
{
    const std::string romDir = CASIO_ROMS_DIR;
    const std::string warmPath = romDir + "/cg50_warm_state.bin";

    auto t0 = std::chrono::steady_clock::now();
    CasioCg50Handle* cg = casio_cg50_create_from_dir(romDir.c_str());
    if (!cg) return Fail("casio_cg50_create_from_dir returned null");

    int imported = casio_cg50_import_state_file(cg, warmPath.c_str());
    if (imported != 1) {
        casio_cg50_destroy(cg);
        return Fail("casio_cg50_import_state_file failed (run gen_cg50_warm_state first)");
    }
    casio_cg50_step(cg, 200000ULL, 4000ULL);
    auto t1 = std::chrono::steady_clock::now();
    double warmMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
    std::printf("[cg50_android_smoke] warm boot time: %.1f ms\n", warmMs);
    if (warmMs >= 1000.0) {
        casio_cg50_destroy(cg);
        return Fail("warm boot exceeded 1000 ms on host");
    }

    constexpr int kFbSize = 384 * 216 * 4;
    std::vector<uint8_t> fb1(kFbSize, 0);
    std::vector<uint8_t> fb2(kFbSize, 0);
    if (casio_cg50_render_rgba(cg, fb1.data(), kFbSize) != 1) {
        casio_cg50_destroy(cg);
        return Fail("first render did not return 1");
    }
    if (casio_cg50_render_rgba(cg, fb2.data(), kFbSize) != 0) {
        casio_cg50_destroy(cg);
        return Fail("dirty-frame skip did not return 0 on identical frame");
    }

    // Enter RUN-MAT (EXE = 0x0302), settle into Run-Matrix prompt, and verify EXACT
    // mathematical result of 1+2=3 against independent host-keyed anchor '3 EXE' (and != '4 EXE')!
    auto EnterRunMatClean = [&](CasioCg50Handle* h) {
        TapCgKey(h, 0x0302); // Enter Run-Matrix from Main Menu
        for (int s = 0; s < 8; ++s) {
            casio_cg50_step(h, 2000000ULL, 16000ULL);
        }
    };
    EnterRunMatClean(cg);
    TapCgKey(cg, 0x0703); // 1
    TapCgKey(cg, 0x0403); // +
    TapCgKey(cg, 0x0603); // 2
    TapCgKey(cg, 0x0302); // EXE -> result '3' on line 1
    std::vector<uint8_t> afterCalc(kFbSize, 0);
    if (casio_cg50_render_rgba(cg, afterCalc.data(), kFbSize) != 1) {
        casio_cg50_destroy(cg);
        return Fail("render after 1+2=EXE did not detect dirty frame");
    }
    const auto crop1Plus2 = ExtractFirstResultCrop(afterCalc);

    // Compare against fresh instance typing anchor '3 EXE' and negative anchor '4 EXE'
    auto RunSingleAnchor = [&](uint16_t digitKey) -> std::vector<uint8_t> {
        CasioCg50Handle* ref = casio_cg50_create_from_dir(romDir.c_str());
        casio_cg50_import_state_file(ref, warmPath.c_str());
        casio_cg50_step(ref, 200000ULL, 4000ULL);
        EnterRunMatClean(ref);
        TapCgKey(ref, digitKey);
        TapCgKey(ref, 0x0302);
        std::vector<uint8_t> rfb(kFbSize, 0);
        casio_cg50_render_rgba_ex(ref, rfb.data(), kFbSize, 1);
        casio_cg50_destroy(ref);
        return ExtractFirstResultCrop(rfb);
    };
    const auto cropAnchor3 = RunSingleAnchor(0x0503); // '3'
    const auto cropAnchor4 = RunSingleAnchor(0x0704); // '4'

    size_t darkCnt = 0, darkA3 = 0, darkA4 = 0, diff3 = 0, diff4 = 0;
    for (size_t i = 0; i < crop1Plus2.size(); ++i) {
        darkCnt += crop1Plus2[i];
        darkA3 += cropAnchor3[i];
        darkA4 += cropAnchor4[i];
        if (crop1Plus2[i] != cropAnchor3[i]) ++diff3;
        if (crop1Plus2[i] != cropAnchor4[i]) ++diff4;
    }
    std::printf("[cg50_android_smoke] H-011 crop stats: ink(1+2)=%zu ink('3')=%zu ink('4')=%zu diff('3')=%zu diff('4')=%zu\n",
                darkCnt, darkA3, darkA4, diff3, diff4);
    if (darkCnt < 20) {
        casio_cg50_destroy(cg);
        return Fail("1+2=EXE result crop has no dark ink");
    }
    if (crop1Plus2 != cropAnchor3) {
        casio_cg50_destroy(cg);
        return Fail("H-011: 1+2=EXE result glyph does NOT match exact host-keyed anchor '3'");
    }
    if (crop1Plus2 == cropAnchor4) {
        casio_cg50_destroy(cg);
        return Fail("H-011 negative check: 1+2=EXE matched '4'");
    }
    std::printf("[cg50_android_smoke] H-011 exact result verified: 1+2 == '3' (ink=%zu px, != '4')\n", darkCnt);

    // State export/import in isolated temp dir (NEVER writes into assets/roms!)
    const fs::path tmpDir = fs::temp_directory_path() / "casio_cg50_smoke_tmp";
    fs::create_directories(tmpDir);
    const std::string tmpState = (tmpDir / "cg50_roundtrip.bin").string();
    const std::string truncFile = (tmpDir / "cg50_trunc.bin").string();

    if (casio_cg50_export_state_file(cg, tmpState.c_str()) != 1) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg);
        return Fail("export_state_file failed");
    }
    // H-007 atomic replacement verification: overwriting an existing state file succeeds cleanly
    if (casio_cg50_export_state_file(cg, tmpState.c_str()) != 1) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg);
        return Fail("H-007: atomic overwrite of existing state file failed");
    }

    uint64_t inst1 = 0, cyc1 = 0, lcd1 = 0;
    uint32_t pc1 = 0;
    casio_cg50_get_stats(cg, &inst1, &cyc1, &pc1, &lcd1);

    CasioCg50Handle* cg2 = casio_cg50_create_from_dir(romDir.c_str());
    if (!cg2) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg);
        return Fail("second create_from_dir failed");
    }
    if (casio_cg50_import_state_file(cg2, tmpState.c_str()) != 1) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("import_state_file into second instance failed");
    }
    uint64_t inst2 = 0, cyc2 = 0, lcd2 = 0;
    uint32_t pc2 = 0;
    casio_cg50_get_stats(cg2, &inst2, &cyc2, &pc2, &lcd2);
    if (inst1 != inst2 || pc1 != pc2) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("state roundtrip stats mismatch");
    }

    // Negative tests: truncated file & missing file rejected
    if (FILE* f = std::fopen(truncFile.c_str(), "wb")) {
        const char bad[32] = "NOT_A_VALID_CG50_STATE_HEADER!!";
        std::fwrite(bad, 1, sizeof(bad), f);
        std::fclose(f);
    }
    if (casio_cg50_import_state_file(cg2, truncFile.c_str()) != 0) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("truncated state file was NOT rejected");
    }
    if (casio_cg50_import_state_file(cg2, (tmpDir / "does_not_exist.bin").string().c_str()) != 0) {
        fs::remove_all(tmpDir);
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("missing state file was NOT rejected");
    }

    fs::remove_all(tmpDir);
    casio_cg50_destroy(cg2);
    casio_cg50_destroy(cg);

    std::printf("[cg50_android_smoke] ALL PASSED\n");
    return 0;
}
