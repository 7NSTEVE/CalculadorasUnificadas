#include "casio_ffi.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

static void TapKey(CasioCg50Handle* cg, uint16_t code, int hold = 4, int settle = 14)
{
    casio_cg50_key_down(cg, code);
    for (int i = 0; i < hold; ++i) casio_cg50_step(cg, 1500000ULL, 12000ULL);
    casio_cg50_key_up(cg, code);
    for (int i = 0; i < settle; ++i) casio_cg50_step(cg, 1500000ULL, 16000ULL);
}

int main(int argc, char** argv)
{
    const std::string romDir = (argc > 1) ? argv[1] : CASIO_ROMS_DIR;
    const std::string outPath = (argc > 2) ? argv[2] : (romDir + "/cg50_warm_state.bin");

    auto t0 = std::chrono::steady_clock::now();
    CasioCg50Handle* cg = casio_cg50_create_from_dir(romDir.c_str());
    if (!cg) {
        std::fprintf(stderr, "FAIL: casio_cg50_create_from_dir(%s) returned null\n", romDir.c_str());
        return 1;
    }

    constexpr int kFbSize = 384 * 216 * 4;
    std::vector<uint8_t> rgba(kFbSize, 0);

    int sleepStreak = 0;
    bool hasFb = false;
    uint64_t inst = 0, cycles = 0, lcdWrites = 0;
    uint32_t pc = 0;

    for (int iter = 0; iter < 80; ++iter) {
        int st = casio_cg50_step(cg, 4000000ULL, 10000ULL);
        casio_cg50_get_stats(cg, &inst, &cycles, &pc, &lcdWrites);
        int rc = casio_cg50_render_rgba_ex(cg, rgba.data(), kFbSize, 1);
        if (rc > 0) hasFb = true;
        if (st == 2 && hasFb) {
            sleepStreak++;
            if (sleepStreak >= 4) break;
        } else {
            sleepStreak = 0;
        }
    }

    if (!hasFb || sleepStreak < 4) {
        std::fprintf(stderr, "FAIL: CG50 did not reach initial cleared screen\n");
        casio_cg50_destroy(cg);
        return 2;
    }

    // Press MENU (0x0409) to dismiss "MAIN MEMORY CLEARED! Press [MENU] KEY" into full-color MAIN MENU
    TapKey(cg, 0x0409, 4, 16);
    casio_cg50_get_stats(cg, &inst, &cycles, &pc, &lcdWrites);

    auto t1 = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(t1 - t0).count();
    std::printf("[gen_cg50_warm_state] MAIN MENU ready in %.2fs: inst=%llu pc=0x%08X lcdWrites=%llu\n",
                sec, (unsigned long long)inst, pc, (unsigned long long)lcdWrites);

    if (casio_cg50_export_state_file(cg, outPath.c_str()) != 1) {
        std::fprintf(stderr, "FAIL: casio_cg50_export_state_file(%s) failed\n", outPath.c_str());
        casio_cg50_destroy(cg);
        return 3;
    }

    casio_cg50_destroy(cg);
    std::printf("[gen_cg50_warm_state] exported -> %s\n", outPath.c_str());
    return 0;
}
