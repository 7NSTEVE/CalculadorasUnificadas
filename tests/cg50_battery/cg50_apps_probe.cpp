// cg50_apps_probe.cpp — Interactive CG50 built-in apps & hardware gap probe
// Exercises GRAPH, TABLE, RECURSION, CONIC, SPREADSHEET, eActivity,
// PROGRAM (Casio BASIC + save/restore persistence) and MEMORY/STORAGE on the
// unified monorepo Cg50NativeCore via cg50_api_c.h (zero Casio DLLs).

#include "cg50_api_c.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kW = 384;
constexpr int kH = 216;
constexpr std::uint32_t kChunkUs = 16667u;
constexpr std::size_t kTamSystem = CG50CORE_SYSTEM_ROM_SIZE;
constexpr std::size_t kTamUser   = CG50CORE_USER_ROM_SIZE;
constexpr std::size_t kTamC2     = CG50CORE_C2_ROM_SIZE;

// Exact matrix codes from cg50_keys_exact.h
constexpr std::uint16_t kF1      = 0x070A, kF2 = 0x060A, kF3 = 0x050A;
constexpr std::uint16_t kF4      = 0x040A, kF5 = 0x030A, kF6 = 0x020A;
constexpr std::uint16_t kShift   = 0x0709, kOptn = 0x0609, kVars = 0x0509;
constexpr std::uint16_t kMenu    = 0x0409, kAlpha = 0x0708, kExit = 0x0408;
constexpr std::uint16_t kArriba  = 0x0209, kIzq = 0x0309, kDer = 0x0208, kAba = 0x0308;
constexpr std::uint16_t kXVar    = 0x0707; // X,theta,T (A)
constexpr std::uint16_t kSq      = 0x0608; // x^2
constexpr std::uint16_t kLog     = 0x0607; // log (B)
constexpr std::uint16_t kLn      = 0x0507; // ln (C)
constexpr std::uint16_t kSin     = 0x0407; // sin (D)
constexpr std::uint16_t kCos     = 0x0307; // cos (E)
constexpr std::uint16_t kTan     = 0x0207; // tan (F)
constexpr std::uint16_t kFrac    = 0x0706; // a b/c (G)
constexpr std::uint16_t kSD      = 0x0606; // S<->D (H)
constexpr std::uint16_t kParenA  = 0x0506; // ( (I)
constexpr std::uint16_t kParenC  = 0x0406; // ) (J)
constexpr std::uint16_t kComa    = 0x0306; // , (K)
constexpr std::uint16_t kArrow   = 0x0206; // -> (L)
constexpr std::uint16_t kAcOn    = 0x0101;
constexpr std::uint16_t kExe     = 0x0302;
constexpr std::uint16_t kDig0    = 0x0702, kDig1 = 0x0703, kDig2 = 0x0603, kDig3 = 0x0503;
constexpr std::uint16_t kDig4    = 0x0704, kDig5 = 0x0604, kDig6 = 0x0504;
constexpr std::uint16_t kDig7    = 0x0705, kDig8 = 0x0605, kDig9 = 0x0505;
constexpr std::uint16_t kMas     = 0x0403, kMenos = 0x0303, kPor = 0x0404, kEntre = 0x0304;
constexpr std::uint16_t kPunto   = 0x0602;

bool ReadExactFile(const char* path, std::size_t expected, std::vector<std::uint8_t>& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto sz = static_cast<std::size_t>(f.tellg());
    if (expected != 0 && sz != expected) return false;
    f.seekg(0);
    out.resize(sz);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz)));
}

struct ProbeCtx {
    Cg50Core* core = nullptr;
    std::string outDir;
    std::vector<std::uint32_t> fb;
    ProbeCtx() : fb(kW * kH, 0) {}

    void TickChunks(int n)
    {
        for (int i = 0; i < n; ++i) {
            cg50core_tick(core, kChunkUs, 0);
        }
    }

    bool CopyFb()
    {
        return cg50core_frame_copy(core, fb.data(), fb.size()) == fb.size();
    }

    std::uint64_t HashFb()
    {
        if (!CopyFb()) return 0;
        std::uint64_t h = 14695981039346656037ULL;
        for (std::uint32_t px : fb) {
            h ^= (px & 0xFFFFFFu);
            h *= 1099511628211ULL;
        }
        return h;
    }

    void Stabilize(int minStable = 12, int maxChunks = 80)
    {
        std::uint64_t prev = HashFb();
        int same = 0;
        for (int i = 0; i < maxChunks && same < minStable; ++i) {
            TickChunks(1);
            const std::uint64_t now = HashFb();
            if (now == prev && now != 0) ++same;
            else { prev = now; same = 0; }
        }
    }

    bool Tap(std::uint16_t code, int holdMs = 150, int settleMs = 450)
    {
        const int holdChunks = std::max(1, holdMs * 1000 / static_cast<int>(kChunkUs));
        const int maxWaitChunks = std::max(1, settleMs * 1000 / static_cast<int>(kChunkUs));
        const std::uint64_t before = HashFb();
        if (cg50core_key(core, code, 1) != 1) return false;
        TickChunks(holdChunks);
        cg50core_key(core, code, 0);
        TickChunks(holdChunks);
        std::uint64_t now = HashFb();
        for (int i = 0; i < maxWaitChunks && now == before; ++i) {
            TickChunks(1);
            now = HashFb();
        }
        TickChunks(4);
        return now != before;
    }

    void TapSeq(const std::vector<std::uint16_t>& keys, int holdMs = 150, int settleMs = 500)
    {
        for (std::uint16_t k : keys) {
            Tap(k, holdMs, settleMs);
        }
    }

    struct ScreenMetrics {
        std::uint64_t hash = 0;
        int darkPx = 0;
        int coloredPx = 0;
        int bluePx = 0;
        std::uint64_t inst = 0;
        std::uint64_t excep = 0;
        int cpuState = -1;
        std::uint32_t pc = 0;
        int unsupOp = 0;
        std::uint16_t unsupCode = 0;
        int memFault = 0;
        std::uint32_t memFaultAddr = 0;
    };

    ScreenMetrics CaptureAndSave(const char* name)
    {
        Stabilize(12, 80);
        ScreenMetrics m{};
        m.hash = HashFb();
        Cg50Status st{};
        cg50core_status(core, &st);
        m.inst = st.instructions;
        m.excep = st.exceptionsTaken;
        m.cpuState = st.cpuState;
        m.pc = st.pc;
        m.unsupOp = st.unsupportedOpcodeSeen;
        m.unsupCode = st.unsupportedOpcode;
        m.memFault = st.memoryFaultSeen;
        m.memFaultAddr = st.lastMemoryFaultAddress;

        // Inspect body region y=21..187
        for (int y = 21; y <= 187; ++y) {
            for (int x = 0; x < kW; ++x) {
                const std::uint32_t p = fb[y * kW + x];
                const int r = static_cast<int>((p >> 16) & 0xFF);
                const int g = static_cast<int>((p >> 8) & 0xFF);
                const int b = static_cast<int>(p & 0xFF);
                const int maxc = std::max(r, std::max(g, b));
                const int minc = std::min(r, std::min(g, b));
                if (maxc - minc > 45) {
                    ++m.coloredPx;
                    if (b > r + 60 && b > g + 30) ++m.bluePx;
                }
                const int luma = (r * 299 + g * 587 + b * 114) / 1000;
                if (luma < 140) ++m.darkPx;
            }
        }

        // Write 24-bit BGR BMP (human-oriented: row 0 top -> BMP bottom-up)
        const fs::path p = fs::path(outDir) / (std::string(name) + ".bmp");
        std::FILE* fp = std::fopen(p.string().c_str(), "wb");
        if (fp) {
            const std::uint32_t rowBytes = kW * 3;
            const std::uint32_t imgSize = rowBytes * kH;
            const std::uint32_t fileSize = 54 + imgSize;
            std::uint8_t hdr[54] = {};
            hdr[0] = 'B'; hdr[1] = 'M';
            std::memcpy(hdr + 2, &fileSize, 4);
            const std::uint32_t off = 54;
            std::memcpy(hdr + 10, &off, 4);
            const std::uint32_t bih = 40;
            std::memcpy(hdr + 14, &bih, 4);
            const std::int32_t ww = kW, hh = kH;
            std::memcpy(hdr + 18, &ww, 4);
            std::memcpy(hdr + 22, &hh, 4);
            const std::uint16_t planes = 1, bpp = 24;
            std::memcpy(hdr + 26, &planes, 2);
            std::memcpy(hdr + 28, &bpp, 2);
            std::memcpy(hdr + 34, &imgSize, 4);
            std::fwrite(hdr, 1, 54, fp);
            std::vector<std::uint8_t> row(rowBytes);
            // CG50 R61524 framebuffer is 180-deg rotated; BMP is bottom-up,
            // so iterating y=0..kH-1 and x=kW-1..0 produces an upright image.
            for (int y = 0; y < kH; ++y) {
                for (int x = 0; x < kW; ++x) {
                    const std::uint32_t px = fb[y * kW + (kW - 1 - x)];
                    row[x * 3 + 0] = static_cast<std::uint8_t>(px & 0xFF);
                    row[x * 3 + 1] = static_cast<std::uint8_t>((px >> 8) & 0xFF);
                    row[x * 3 + 2] = static_cast<std::uint8_t>((px >> 16) & 0xFF);
                }
                std::fwrite(row.data(), 1, rowBytes, fp);
            }
            std::fclose(fp);
        }

        std::printf("[APPS] %-24s hash=%016" PRIX64 " dark=%5d color=%5d blue=%5d inst=%" PRIu64 " exc=%" PRIu64 " unsup=%d(0x%04X) mflt=%d(0x%08X) cpu=%d pc=0x%08X\n",
                    name, m.hash, m.darkPx, m.coloredPx, m.bluePx, m.inst, m.excep,
                    m.unsupOp, m.unsupCode, m.memFault, m.memFaultAddr, m.cpuState, m.pc);
        std::fflush(stdout);
        return m;
    }

    void WaitUntilSleeping(int maxChunks)
    {
        int sleepRun = 0;
        for (int i = 0; i < maxChunks; ++i) {
            cg50core_tick(core, kChunkUs, 0);
            Cg50Status st{};
            cg50core_status(core, &st);
            if (st.cpuState == 2) {
                if (++sleepRun >= 8) break;
            } else {
                sleepRun = 0;
            }
        }
    }

    void ReturnToMenu()
    {
        Tap(kExit, 150, 300);
        Tap(kAcOn, 150, 300);
        Tap(kMenu, 200, 1200);
        Stabilize(12, 60);
    }
};

} // namespace

int main(int argc, char** argv)
{
    if (argc < 6) {
        std::fprintf(stderr, "Usage: %s <SystemROM> <UserROM> <C2ROM> <warm_state.bin> <outDir>\n", argv[0]);
        return 2;
    }

    std::vector<std::uint8_t> sys, user, c2, warm;
    if (!ReadExactFile(argv[1], kTamSystem, sys) ||
        !ReadExactFile(argv[2], kTamUser, user) ||
        !ReadExactFile(argv[3], kTamC2, c2) ||
        !ReadExactFile(argv[4], 0, warm)) {
        std::fprintf(stderr, "[APPS] Failed reading input ROMs or warm state\n");
        return 2;
    }

    fs::create_directories(argv[5]);

    ProbeCtx ctx;
    ctx.outDir = argv[5];
    ctx.core = cg50core_create();
    if (!ctx.core) return 3;

    if (cg50core_load_firmware_raw(ctx.core, sys.data(), sys.size(), user.data(), user.size(), c2.data(), c2.size()) != CG50CORE_OK) {
        std::fprintf(stderr, "[APPS] load_firmware_raw failed\n");
        return 3;
    }
    cg50core_set_option(ctx.core, CG50CORE_OPT_SLEEP_ADVANCE, 1);

    if (cg50core_restore(ctx.core, warm.data(), warm.size()) != CG50CORE_OK) {
        std::fprintf(stderr, "[APPS] warm_state restore failed\n");
        return 3;
    }
    std::printf("[APPS] Warm state restored (%zu bytes)\n", warm.size());

    int failures = 0;

    if (argc >= 7 && std::strcmp(argv[6], "page2") == 0) {
        ctx.ReturnToMenu();
        ctx.TapSeq({kAba, kAba, kAba, kAba, kAba, kAba}, 150, 500);
        ctx.CaptureAndSave("20_main_menu_page3");

        // Python [H]
        ctx.ReturnToMenu();
        ctx.Tap(kSD, 200, 2500);
        ctx.CaptureAndSave("21_python_open");
        ctx.Tap(kF2, 200, 2000);
        ctx.CaptureAndSave("21b_python_shell");

        // Distribution [I]
        ctx.ReturnToMenu();
        ctx.Tap(kParenA, 200, 2200);
        ctx.CaptureAndSave("22_distribution");

        // Geometry.g3a [J]
        ctx.ReturnToMenu();
        ctx.Tap(kParenC, 200, 3000);
        ctx.CaptureAndSave("23_geometry_g3a");

        // PictPlot.g3a [K]
        ctx.ReturnToMenu();
        ctx.Tap(kComa, 200, 3000);
        ctx.CaptureAndSave("24_pictplot_g3a");

        // 3DGraph.g3a / Conversion [L]
        ctx.ReturnToMenu();
        ctx.Tap(kArrow, 200, 3000);
        ctx.CaptureAndSave("25_app_l_g3a");

        // eActivity [3]
        ctx.ReturnToMenu();
        ctx.Tap(kDig3, 200, 2500);
        ctx.CaptureAndSave("26_eactivity");

        cg50core_destroy(ctx.core);
        std::printf("[APPS] PAGE2 COMPLETE\n");
        return 0;
    }

    if (argc >= 7 && std::strcmp(argv[6], "g3a") == 0) {
        // 1. Python [H]: from Main Menu row 1 col 1 -> 4 downs = Row 5 Col 1 (Python) -> EXE
        ctx.ReturnToMenu();
        ctx.TapSeq({kAba, kAba, kAba, kAba, kExe}, 180, 2500);
        ctx.CaptureAndSave("30_python_menu");
        ctx.Tap(kF2, 200, 2500); // SHELL
        ctx.CaptureAndSave("31_python_shell");

        // 2. Geometry.g3a [J] with 10s settle
        ctx.ReturnToMenu();
        ctx.Tap(kParenC, 200, 8000);
        ctx.TickChunks(180);
        ctx.CaptureAndSave("32_geometry_g3a");

        // 3. Physium.g3a [M]: 5 downs, 1 right -> EXE
        ctx.ReturnToMenu();
        ctx.TapSeq({kAba, kAba, kAba, kAba, kAba, kDer, kExe}, 180, 6000);
        ctx.TickChunks(120);
        ctx.CaptureAndSave("33_physium_g3a");

        // 4. Prob Sim [N]: 5 downs, 2 rights -> EXE
        ctx.ReturnToMenu();
        ctx.TapSeq({kAba, kAba, kAba, kAba, kAba, kDer, kDer, kExe}, 180, 6000);
        ctx.TickChunks(120);
        ctx.CaptureAndSave("34_probsim_g3a");

        cg50core_destroy(ctx.core);
        std::printf("[APPS] G3A COMPLETE\n");
        return 0;
    }

    if (argc >= 7 && std::strcmp(argv[6], "eact") == 0) {
        ctx.ReturnToMenu();
        ctx.Tap(kDig3, 200, 2500);
        ctx.CaptureAndSave("40_eactivity");
        cg50core_destroy(ctx.core);
        return 0;
    }

    if (argc >= 7 && std::strcmp(argv[6], "cert19") == 0) {
        // 1. eActivity [3]: open cleanly (NO Memory ERROR), create file "A", switch to CALC,
        //    compute 7x8 = 56, SAVE to Spansion NOR Flash (FILE -> SAVE -> "Complete!"),
        //    and re-open eActivity list to prove "A" persisted on SMEM Flash!
        ctx.ReturnToMenu();
        ctx.Tap(kDig3, 200, 2500);
        ctx.CaptureAndSave("41_eact_list");
        ctx.TapSeq({kF2, kXVar, kExe}, 180, 1500); // NEW -> "A" -> EXE
        ctx.CaptureAndSave("42_eact_editor");
        ctx.Tap(kF3, 180, 700); // Toggle TEXT -> CALC line mode
        ctx.TapSeq({kDig7, kPor, kDig8, kExe}, 160, 1200); // 7x8 = 56
        ctx.CaptureAndSave("43_eact_calc");
        // Save A.g3e to Spansion NOR Flash via FILE(F1) -> SAVE(F1) and wait until Complete!
        ctx.Tap(kF1, 200, 1000); // FILE menu
        ctx.Tap(kF1, 200, 1000); // SAVE to SMEM NOR Flash!
        ctx.WaitUntilSleeping(1600); // Wait for full SMEM compaction + Write-to-Buffer + Erase!
        ctx.CaptureAndSave("43b_eact_save_complete");
        ctx.Tap(kExit, 200, 1200); // Dismiss "Complete! Press:[EXIT]"
        ctx.Tap(kExit, 200, 1500); // Exit back to eAct file list (now showing saved file A!)
        ctx.Tap(kExit, 200, 1500);
        ctx.CaptureAndSave("43c_eact_list_persisted");

        // 2. Python [H = kSD]: open Python -> F4 (SHELL) -> compute 2**10 = 1024!
        ctx.ReturnToMenu();
        ctx.Tap(kSD, 200, 2500);
        ctx.CaptureAndSave("44_python_menu");
        ctx.Tap(kF4, 200, 3500); // F4 = SHELL
        ctx.WaitUntilSleeping(200);
        ctx.CaptureAndSave("45_python_shell");
        ctx.TapSeq({kDig2, kPor, kPor, kDig1, kDig0, kExe}, 180, 2500);
        ctx.CaptureAndSave("46_python_2pow10");

        // 3. Spreadsheet [4]: A1=10, A2=20, A3 = "=A1+A2" -> 30
        ctx.ReturnToMenu();
        ctx.Tap(kDig4, 200, 2200);
        ctx.TapSeq({kDig1, kDig0, kExe, kDig2, kDig0, kExe}, 160, 600);
        ctx.TapSeq({kShift, kPunto, kAlpha, kXVar, kDig1, kMas, kAlpha, kXVar, kDig2, kExe}, 160, 1200);
        ctx.CaptureAndSave("47_spreadsheet_sum30");

        // 4. 3DGraph.g3a [L = kArrow]: open -> F3 (TYPE) -> F1 (Template) -> select Sphere (2x DOWN + EXE) -> F6 (SET) -> F6 (DRAW)
        ctx.ReturnToMenu();
        ctx.Tap(kArrow, 200, 4500);
        ctx.WaitUntilSleeping(200);
        ctx.CaptureAndSave("48_3dgraph_open");
        ctx.Tap(kF3, 200, 1500); // F3 = TYPE -> Graph Func
        ctx.Tap(kF1, 200, 1500); // F1 = Template dialog (Line / Plane / Sphere / Cylinder / Cone)
        ctx.TapSeq({kAba, kAba, kExe}, 180, 1800); // Select 3rd template: Sphere (X-a)^2+(Y-b)^2+(Z-c)^2=r^2
        ctx.TapSeq({kDer, kDer, kDer, kDig2, kExe}, 180, 1200); // Set radius r = 2!
        ctx.CaptureAndSave("49_3dgraph_sphere_def");
        ctx.Tap(kF6, 200, 1800); // SET (commits Sphere r=2 into 3D Graph list)
        ctx.Tap(kF6, 200, 5000); // DRAW 3D sphere using fixed SH-4A FPU!
        ctx.WaitUntilSleeping(600);
        ctx.CaptureAndSave("50_3dgraph_sphere_draw");

        // 5. Physium.g3a [ALPHA+7 = M]
        ctx.ReturnToMenu();
        ctx.TapSeq({kAlpha, kDig7}, 200, 5000);
        ctx.WaitUntilSleeping(200);
        ctx.CaptureAndSave("51_physium_g3a");

        // 6. Prob Sim [ALPHA+8 = N]
        ctx.ReturnToMenu();
        ctx.TapSeq({kAlpha, kDig8}, 200, 5000);
        ctx.WaitUntilSleeping(200);
        ctx.CaptureAndSave("52_probsim_g3a");

        // 7. System Manager [G = kFrac] -> F4 (Version)
        ctx.ReturnToMenu();
        ctx.Tap(kFrac, 200, 2000);
        ctx.Tap(kF4, 200, 1500);
        ctx.CaptureAndSave("53_sys_version");

        cg50core_destroy(ctx.core);
        std::printf("[APPS] CERT19 COMPLETE\n");
        return 0;
    }

    // 0. Main Menu baseline + page 2 icon census
    ctx.ReturnToMenu();
    const auto mMenu = ctx.CaptureAndSave("00_main_menu");
    if (mMenu.coloredPx < 15000) {
        std::fprintf(stderr, "[APPS] FAIL: Main Menu colored pixels too low (%d)\n", mMenu.coloredPx);
        ++failures;
    }
    ctx.TapSeq({kAba, kAba, kAba, kAba}, 150, 500);
    ctx.CaptureAndSave("00b_main_menu_page2");
    ctx.TapSeq({kArriba, kArriba, kArriba, kArriba}, 150, 500);

    // 1. GRAPH (5): Y1 = X^2 -> DRAW (F6) -> G-Solv (SHIFT+F5) -> MIN (F3)
    ctx.Tap(kDig5, 200, 1800);
    ctx.CaptureAndSave("01_graph_editor");
    ctx.TapSeq({kXVar, kSq, kExe}, 160, 600);
    ctx.Tap(kF6, 200, 3500);
    const auto mGraph = ctx.CaptureAndSave("02_graph_draw_x2");
    if (mGraph.bluePx < 80) {
        std::fprintf(stderr, "[APPS] WARN/FAIL: GRAPH Y1=X^2 blue curve pixels=%d\n", mGraph.bluePx);
        ++failures;
    }
    ctx.TapSeq({kShift, kF5, kF3}, 180, 1500);
    const auto mGsolv = ctx.CaptureAndSave("03_graph_gsolv_min");
    if (mGsolv.hash == mGraph.hash) {
        std::fprintf(stderr, "[APPS] WARN: G-Solv MIN did not change screen\n");
    }

    // 2. TABLE (7): uses Y1=X^2 -> F6 (TABL)
    ctx.ReturnToMenu();
    ctx.Tap(kDig7, 200, 1800);
    const auto mTabEd = ctx.CaptureAndSave("04_table_editor");
    ctx.Tap(kF6, 200, 2500);
    const auto mTabVal = ctx.CaptureAndSave("05_table_values");
    if (mTabVal.hash == mTabEd.hash) {
        std::fprintf(stderr, "[APPS] FAIL: TABLE F6 did not generate table\n");
        ++failures;
    }

    // 3. RECURSION (8): already on a_{n+1}, enter 2*a_n + 1 -> TABL(F6)
    ctx.ReturnToMenu();
    ctx.Tap(kDig8, 200, 1800);
    ctx.CaptureAndSave("06_recursion_open");
    ctx.TapSeq({kDig2, kPor, kF4, kF2, kMas, kDig1, kExe}, 160, 600);
    ctx.Tap(kF6, 200, 2500);
    ctx.CaptureAndSave("07_recursion_table");

    // 4. CONIC GRAPHS (9): select parabola -> A=1, H=0, K=0 -> DRAW(F6)
    ctx.ReturnToMenu();
    ctx.Tap(kDig9, 200, 1800);
    ctx.CaptureAndSave("08_conic_list");
    ctx.TapSeq({kExe, kDig1, kExe, kDig0, kExe, kDig0, kExe, kF6}, 180, 1800);
    const auto mConic = ctx.CaptureAndSave("09_conic_draw");
    if (mConic.bluePx < 50 && mConic.coloredPx < 50) {
        std::fprintf(stderr, "[APPS] WARN: CONIC DRAW low colored pixels (%d)\n", mConic.coloredPx);
    }

    // 5. SPREADSHEET (4): enter 10 in A1, 20 in A2
    ctx.ReturnToMenu();
    ctx.Tap(kDig4, 200, 2200);
    ctx.CaptureAndSave("10_spreadsheet_open");
    ctx.TapSeq({kDig1, kDig0, kExe, kDig2, kDig0, kExe}, 160, 600);
    ctx.CaptureAndSave("11_spreadsheet_values");

    // 6. eActivity (3)
    ctx.ReturnToMenu();
    ctx.Tap(kDig3, 200, 2200);
    ctx.CaptureAndSave("12_eactivity");

    // 7. PROGRAM / Casio BASIC (B = kLog): NEW(F3), name "T"(kEntre), EXE, "2+3", EXIT, EXE(F1)
    ctx.ReturnToMenu();
    ctx.Tap(kLog, 200, 1800);
    ctx.CaptureAndSave("13_program_list");
    ctx.TapSeq({kF3, kEntre, kExe}, 180, 1000);
    ctx.CaptureAndSave("14_program_editor");
    ctx.TapSeq({kDig2, kMas, kDig3, kExit}, 160, 700);
    ctx.Tap(kF1, 200, 2000);
    const auto mProgRun = ctx.CaptureAndSave("15_program_run");

    // Test Save/Restore Persistence of Program "T" in a fresh core instance!
    const std::size_t stateSz = cg50core_save(ctx.core, nullptr, 0);
    std::vector<std::uint8_t> savedBlob(stateSz);
    const std::size_t wrote = cg50core_save(ctx.core, savedBlob.data(), savedBlob.size());
    const std::size_t urSz = cg50core_read_user_rom(ctx.core, nullptr, 0);
    std::printf("[APPS] Persistence export: state=%zu/%zu B, userRom=%zu B\n", wrote, stateSz, urSz);

    // 8. MEMORY MANAGER (F = kTan): open Main Memory (F1) + Storage Memory (F2)
    ctx.ReturnToMenu();
    ctx.Tap(kTan, 200, 1800);
    ctx.CaptureAndSave("16_memory_manager");
    ctx.Tap(kF1, 200, 1800);
    ctx.CaptureAndSave("16b_main_memory");
    ctx.Tap(kExit, 180, 800);
    ctx.Tap(kF2, 200, 1800);
    ctx.CaptureAndSave("17_storage_memory");

    // 9. SYSTEM MANAGER (G = kFrac), DYNA GRAPH (6), FINANCIAL (C = kLn), APP D (kSin)
    ctx.ReturnToMenu();
    ctx.Tap(kFrac, 200, 1800);
    ctx.CaptureAndSave("17b_system_manager");
    ctx.ReturnToMenu();
    ctx.Tap(kDig6, 200, 1800);
    ctx.CaptureAndSave("17c_dyna_graph");
    ctx.ReturnToMenu();
    ctx.Tap(kLn, 200, 1800);
    ctx.CaptureAndSave("17d_financial");
    ctx.ReturnToMenu();
    ctx.Tap(kSin, 200, 1800);
    ctx.CaptureAndSave("17e_app_d");

    // Verify fresh-instance restore brings back Program "T" cleanly
    if (wrote == stateSz && stateSz > 0) {
        ProbeCtx ctx2;
        ctx2.outDir = ctx.outDir;
        ctx2.core = cg50core_create();
        cg50core_load_firmware_raw(ctx2.core, sys.data(), sys.size(), user.data(), user.size(), c2.data(), c2.size());
        cg50core_set_option(ctx2.core, CG50CORE_OPT_SLEEP_ADVANCE, 1);
        const int rrc = cg50core_restore(ctx2.core, savedBlob.data(), savedBlob.size());
        if (rrc != CG50CORE_OK) {
            std::fprintf(stderr, "[APPS] FAIL: fresh instance restore returned %d\n", rrc);
            ++failures;
        } else {
            const auto mRestored = ctx2.CaptureAndSave("18_restored_program_run");
            if (mRestored.hash != mProgRun.hash) {
                std::fprintf(stderr, "[APPS] FAIL: restored frame hash mismatch (%016" PRIX64 " vs %016" PRIX64 ")\n",
                             mRestored.hash, mProgRun.hash);
                ++failures;
            }
            ctx2.ReturnToMenu();
            ctx2.Tap(kLog, 200, 1800);
            ctx2.CaptureAndSave("19_restored_program_list");
        }
        cg50core_destroy(ctx2.core);
    } else {
        ++failures;
    }

    cg50core_destroy(ctx.core);
    std::printf("[APPS] COMPLETE failures=%d\n", failures);
    return failures == 0 ? 0 : 1;
}
