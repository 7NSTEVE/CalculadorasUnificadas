#include "casio_ffi.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int Fail(const char* msg)
{
    std::fprintf(stderr, "[cg50_android_smoke] FAIL: %s\n", msg);
    return 1;
}

static void TapCgKey(CasioCg50Handle* cg, uint16_t code)
{
    casio_cg50_key_down(cg, code);
    casio_cg50_step(cg, 600000ULL, 8000ULL);
    casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_key_up(cg, code);
    casio_cg50_step(cg, 1500000ULL, 16000ULL);
    casio_cg50_step(cg, 1500000ULL, 16000ULL);
}

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

    // First render must return 1 (new active frame)
    if (casio_cg50_render_rgba(cg, fb1.data(), kFbSize) != 1) {
        casio_cg50_destroy(cg);
        return Fail("first render did not return 1");
    }
    // Immediate second render without CPU change must return 0 (dirty-frame skip)
    if (casio_cg50_render_rgba(cg, fb2.data(), kFbSize) != 0) {
        casio_cg50_destroy(cg);
        return Fail("dirty-frame skip did not return 0 on identical frame");
    }

    // Enter RUN-MAT (EXE = 0x0302) then type 1 + 2 EXE
    TapCgKey(cg, 0x0302); // EXE enters RUN-MAT from main menu icon 1
    std::vector<uint8_t> beforeCalc(kFbSize, 0);
    casio_cg50_render_rgba_ex(cg, beforeCalc.data(), kFbSize, 1);

    TapCgKey(cg, 0x0703); // 1
    TapCgKey(cg, 0x0403); // +
    TapCgKey(cg, 0x0603); // 2
    TapCgKey(cg, 0x0302); // EXE

    std::vector<uint8_t> afterCalc(kFbSize, 0);
    int changedRc = casio_cg50_render_rgba(cg, afterCalc.data(), kFbSize);
    if (changedRc != 1) {
        casio_cg50_destroy(cg);
        return Fail("render after 1+2=EXE did not detect dirty frame");
    }
    size_t diffPixels = 0;
    for (size_t i = 0; i < kFbSize; i += 4) {
        if (beforeCalc[i] != afterCalc[i] || beforeCalc[i + 1] != afterCalc[i + 1] || beforeCalc[i + 2] != afterCalc[i + 2])
            diffPixels++;
    }
    std::printf("[cg50_android_smoke] RUN-MAT 1+2=EXE changed pixels: %zu\n", diffPixels);
    if (diffPixels < 50) {
        casio_cg50_destroy(cg);
        return Fail("too few pixels changed after 1+2=EXE");
    }

    // Positive file state export/import roundtrip
    const std::string tmpState = romDir + "/_test_cg50_roundtrip.bin";
    if (casio_cg50_export_state_file(cg, tmpState.c_str()) != 1) {
        casio_cg50_destroy(cg);
        return Fail("export_state_file failed");
    }

    uint64_t inst1 = 0, cyc1 = 0, lcd1 = 0;
    uint32_t pc1 = 0;
    casio_cg50_get_stats(cg, &inst1, &cyc1, &pc1, &lcd1);

    CasioCg50Handle* cg2 = casio_cg50_create_from_dir(romDir.c_str());
    if (!cg2) {
        casio_cg50_destroy(cg);
        return Fail("second create_from_dir failed");
    }
    if (casio_cg50_import_state_file(cg2, tmpState.c_str()) != 1) {
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("import_state_file into second instance failed");
    }
    uint64_t inst2 = 0, cyc2 = 0, lcd2 = 0;
    uint32_t pc2 = 0;
    casio_cg50_get_stats(cg2, &inst2, &cyc2, &pc2, &lcd2);
    if (inst1 != inst2 || pc1 != pc2) {
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("state roundtrip stats mismatch");
    }

    // Negative tests: truncated file & corrupted header rejected
    const std::string truncFile = romDir + "/_test_cg50_trunc.bin";
    if (FILE* f = std::fopen(truncFile.c_str(), "wb")) {
        const char bad[32] = "NOT_A_VALID_CG50_STATE_HEADER!!";
        std::fwrite(bad, 1, sizeof(bad), f);
        std::fclose(f);
    }
    if (casio_cg50_import_state_file(cg2, truncFile.c_str()) != 0) {
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("truncated state file was NOT rejected");
    }
    if (casio_cg50_import_state_file(cg2, (romDir + "/does_not_exist.bin").c_str()) != 0) {
        casio_cg50_destroy(cg2);
        casio_cg50_destroy(cg);
        return Fail("missing state file was NOT rejected");
    }

    std::remove(tmpState.c_str());
    std::remove(truncFile.c_str());
    casio_cg50_destroy(cg2);
    casio_cg50_destroy(cg);

    std::printf("[cg50_android_smoke] ALL PASSED\n");
    return 0;
}
