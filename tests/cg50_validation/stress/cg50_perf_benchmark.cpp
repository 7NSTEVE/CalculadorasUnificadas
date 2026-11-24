// cg50_perf_benchmark.cpp — Phase 7 Performance, Scaling (1..16 Threads) & Memory Benchmark (v46)
//
// Measures real Casio fx-CG50 OS 3.80 performance on Cg50NativeCore (zero HLE, strict_no_fpu=true):
//   - Cold boot latency (ms) & warm state save/restore latency (ms)
//   - Single-thread MIPS across 4 workloads: Run-Matrix, Calculus, MicroPython, 2D Graph
//   - Effective LCD frames/sec & keyboard-to-frame response latency (ms)
//   - Empirical multi-thread scaling across 1, 2, 4, 8, 12, 16 threads + Process RSS (MB)
//
// Usage:
//   cg50_perf_benchmark.exe --label before|after --out <perf.json> [--roms <dir>]

#include "cg50_native_core.h"
#include "cg50_core/cg50_smem_fs.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kPixels = 384 * 216;
constexpr std::uint64_t kChunk = 1000000ull;

constexpr std::uint16_t kF1 = 0x070A, kF2 = 0x060A, kF3 = 0x050A, kF4 = 0x040A, kF6 = 0x020A;
constexpr std::uint16_t kOptn = 0x0609, kAlpha = 0x0708, kSD = 0x0606;
constexpr std::uint16_t kXVar = 0x0707, kPow = 0x0508, kCos = 0x0307, kSin = 0x0407;
constexpr std::uint16_t kLParen = 0x0506, kRParen = 0x0406, kRight = 0x0208;
constexpr std::uint16_t kDig0 = 0x0702, kDig1 = 0x0703, kDig3 = 0x0503, kDig5 = 0x0604;
constexpr std::uint16_t kExe = 0x0302, kAcOn = 0x0101;

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& out)
{
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const auto sz = static_cast<std::size_t>(f.tellg());
    f.seekg(0);
    out.resize(sz);
    return static_cast<bool>(f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz)));
}

double CurrentRssMb()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

double PeakRssMb()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<double>(pmc.PeakWorkingSetSize) / (1024.0 * 1024.0);
    }
#endif
    return 0.0;
}

std::uint64_t HashLcd(const Cg50NativeCore& core)
{
    const auto& lcd = core.Peripherals().Lcd();
    if (!lcd.FramebufferAvailable() || lcd.FramebufferPixelCount() < static_cast<std::size_t>(kPixels)) {
        return 0u;
    }
    const std::uint16_t* p = lcd.FramebufferData();
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (int i = 0; i < kPixels; ++i) {
        h ^= static_cast<std::uint64_t>(p[i]);
        h *= 0x00000100000001b3ULL;
    }
    return h;
}

void StepChunks(Cg50NativeCore& core, int n)
{
    for (int i = 0; i < n; ++i) {
        if (core.GetState() == Cg50NativeCore::State::Sleeping) core.AdvanceCycles(kChunk);
        else if (core.GetState() == Cg50NativeCore::State::Running) core.RunInstructions(kChunk);
        else break;
    }
}

void TapKey(Cg50NativeCore& core, std::uint16_t code, int hold = 3, int post = 14)
{
    core.KeyDown(code);
    StepChunks(core, hold);
    core.KeyUp(code);
    StepChunks(core, post);
}

void Stabilize(Cg50NativeCore& core, int streakNeed = 4, int maxChunks = 100)
{
    std::uint64_t prev = HashLcd(core);
    int streak = 0;
    for (int i = 0; i < maxChunks; ++i) {
        StepChunks(core, 1);
        const auto cur = HashLcd(core);
        if (cur == prev && core.GetState() == Cg50NativeCore::State::Sleeping) {
            if (++streak >= streakNeed) return;
        } else {
            streak = 0;
            prev = cur;
        }
    }
}

struct WorkloadMetric {
    std::string name;
    std::uint64_t instructions = 0;
    std::uint64_t bcdOps = 0;
    std::uint64_t fpuOps = 0;
    double elapsedMs = 0.0;
    double mips = 0.0;
    double equivRealSpeedupX = 0.0; // relative to 58 MHz SH7305 (~58 MIPS nominal peak)
};

struct ThreadScalePoint {
    unsigned threads = 1;
    std::uint64_t totalInstructions = 0;
    double wallMs = 0.0;
    double aggregateMips = 0.0;
    double perThreadMips = 0.0;
    double speedupVs1T = 1.0;
    double rssMb = 0.0;
};

} // namespace

int main(int argc, char** argv)
{
    fs::path romDir = "D:/Proyectos/CalculadorasUnificadas/assets/roms";
    fs::path outJson = "D:/Proyectos/CalculadorasUnificadas/tests/cg50_validation/reports/perf_v46/perf_snapshot.json";
    std::string label = "baseline";
    bool skipColdBoot = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--roms") == 0 && i + 1 < argc) romDir = argv[++i];
        else if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) outJson = argv[++i];
        else if (std::strcmp(argv[i], "--label") == 0 && i + 1 < argc) label = argv[++i];
        else if (std::strcmp(argv[i], "--skip-cold-boot") == 0) skipColdBoot = true;
    }

    std::vector<std::uint8_t> sys, user, c2, warm;
    if (!ReadFileBytes(romDir / "SystemROM.raw", sys) ||
        !ReadFileBytes(romDir / "UserROM.raw", user) ||
        !ReadFileBytes(romDir / "C2ROM.raw", c2) ||
        !ReadFileBytes(romDir / "cg50_warm_state.bin", warm)) {
        std::fprintf(stderr, "[PERF] FAIL: cannot load ROMs/warm state from %s\n", romDir.string().c_str());
        return 2;
    }

    std::printf("[PERF] Starting benchmark label=%s hw_threads=%u\n", label.c_str(), std::thread::hardware_concurrency());
    std::fflush(stdout);

    // 1. Cold boot timing (from reset to first Main Menu SLEEP)
    double coldBootMs = 0.0;
    std::uint64_t coldBootInst = 0;
    if (!skipColdBoot) {
        auto core = std::make_unique<Cg50NativeCore>();
        core->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
        core->LoadC2Rom(c2.data(), c2.size());
        core->SetStrictSh7305NoFpu(true);
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < 320; ++i) {
            if (core->GetState() == Cg50NativeCore::State::Sleeping) {
                core->AdvanceCycles(kChunk);
                if (core->GetStats().instructions > 150000000ull) break;
            } else if (core->GetState() == Cg50NativeCore::State::Running) {
                core->RunInstructions(kChunk);
            } else {
                break;
            }
        }
        coldBootMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        coldBootInst = core->GetStats().instructions;
        std::printf("[PERF] Cold boot: %.1f ms (%llu inst, %.1f MIPS)\n",
                    coldBootMs, static_cast<unsigned long long>(coldBootInst),
                    coldBootMs > 0.0 ? (coldBootInst / (coldBootMs * 1000.0)) : 0.0);
    }

    // 2. Warm restore & save-state export timings (averaged over 5 runs)
    double warmRestoreMs = 0.0, stateExportMs = 0.0;
    std::size_t stateBytes = 0;
    {
        auto core = std::make_unique<Cg50NativeCore>();
        core->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
        core->LoadC2Rom(c2.data(), c2.size());
        core->SetStrictSh7305NoFpu(true);
        const auto tr0 = std::chrono::steady_clock::now();
        for (int r = 0; r < 5; ++r) core->ImportState(warm.data(), warm.size());
        warmRestoreMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tr0).count() / 5.0;

        std::vector<std::uint8_t> expBuf;
        const auto te0 = std::chrono::steady_clock::now();
        for (int r = 0; r < 5; ++r) core->ExportState(expBuf);
        stateExportMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - te0).count() / 5.0;
        stateBytes = expBuf.size();
        std::printf("[PERF] Warm restore: %.2f ms | Save export: %.2f ms (%zu bytes)\n",
                    warmRestoreMs, stateExportMs, stateBytes);
    }

    // 3. Single-thread real workloads (Run-Matrix, Calculus, MicroPython, 2D Graph) + Keyboard response latency
    std::vector<WorkloadMetric> workloads;
    double keyLatencyMs = 0.0;
    double graphEffectiveFps = 0.0;

    // 3a. Run-Matrix arithmetic + trig + key response latency
    {
        auto core = std::make_unique<Cg50NativeCore>();
        core->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
        core->LoadC2Rom(c2.data(), c2.size());
        core->ImportState(warm.data(), warm.size());
        core->SetStrictSh7305NoFpu(true);
        TapKey(*core, kDig1, 4, 24);
        Stabilize(*core, 4, 60);

        // Key response latency: tap '5' and measure ms until framebuffer hash changes
        const auto h0 = HashLcd(*core);
        const auto tk0 = std::chrono::steady_clock::now();
        core->KeyDown(kDig5);
        for (int s = 0; s < 40; ++s) {
            if (core->GetState() == Cg50NativeCore::State::Sleeping) core->AdvanceCycles(200000ull);
            else core->RunInstructions(200000ull);
            if (HashLcd(*core) != h0) break;
        }
        keyLatencyMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tk0).count();
        core->KeyUp(kDig5);
        TapKey(*core, kAcOn, 3, 12);

        const auto inst0 = core->GetStats().instructions;
        const auto bcd0  = core->GetAuthenticityStats().bcdAluOpsExecuted;
        const auto fpu0  = core->GetAuthenticityStats().fpuOpcodesExecuted;
        const auto tw0 = std::chrono::steady_clock::now();
        for (int iter = 0; iter < 8; ++iter) {
            TapKey(*core, kSin, 2, 10);
            TapKey(*core, kDig1, 2, 10);
            TapKey(*core, kExe, 2, 40);
            Stabilize(*core, 3, 25);
            TapKey(*core, kAcOn, 2, 8);
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tw0).count();
        const auto di = core->GetStats().instructions - inst0;
        const double mips = ms > 0.0 ? (di / (ms * 1000.0)) : 0.0;
        workloads.push_back({"run_matrix_trig", di, core->GetAuthenticityStats().bcdAluOpsExecuted - bcd0,
                             core->GetAuthenticityStats().fpuOpcodesExecuted - fpu0, ms, mips, mips / 58.0});
    }

    // 3b. Calculus: int_0^3 x^3 dx
    {
        auto core = std::make_unique<Cg50NativeCore>();
        core->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
        core->LoadC2Rom(c2.data(), c2.size());
        core->ImportState(warm.data(), warm.size());
        core->SetStrictSh7305NoFpu(true);
        TapKey(*core, kDig1, 4, 24);
        Stabilize(*core, 4, 60);

        const auto inst0 = core->GetStats().instructions;
        const auto bcd0  = core->GetAuthenticityStats().bcdAluOpsExecuted;
        const auto fpu0  = core->GetAuthenticityStats().fpuOpcodesExecuted;
        const auto tw0 = std::chrono::steady_clock::now();
        for (std::uint16_t k : {kOptn, kF4, kF4, kXVar, kPow, kDig3, kRight, kRight, kDig0, kRight, kDig3, kExe}) {
            TapKey(*core, k, 2, 14);
        }
        Stabilize(*core, 4, 150);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tw0).count();
        const auto di = core->GetStats().instructions - inst0;
        const double mips = ms > 0.0 ? (di / (ms * 1000.0)) : 0.0;
        workloads.push_back({"calculus_integral_x3", di, core->GetAuthenticityStats().bcdAluOpsExecuted - bcd0,
                             core->GetAuthenticityStats().fpuOpcodesExecuted - fpu0, ms, mips, mips / 58.0});
    }

    // 3c. MicroPython heavy compute loop (Sieve + 2000 trig iterations)
    {
        auto core = std::make_unique<Cg50NativeCore>();
        core->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
        core->LoadC2Rom(c2.data(), c2.size());
        core->ImportState(warm.data(), warm.size());
        core->SetStrictSh7305NoFpu(true);
        cg50::Cg50SmemFs smem;
        smem.Mount(core->UserRomMut().data(), core->UserRomMut().size(), core->C2RomData(), core->C2RomSize());
        const std::string py =
            "from math import sin, cos\n"
            "s = 0.0\n"
            "for i in range(1500):\n"
            "  s += sin(i*0.01)*cos(i*0.02)\n"
            "print('OK', s)\n";
        smem.WriteFile("/a_perf.py", reinterpret_cast<const std::uint8_t*>(py.data()), py.size(), true);
        core->NotifyFlashStorageMutated();
        TapKey(*core, kAlpha, 3, 12);
        TapKey(*core, kSD, 3, 25);
        Stabilize(*core, 4, 60);

        const auto inst0 = core->GetStats().instructions;
        const auto bcd0  = core->GetAuthenticityStats().bcdAluOpsExecuted;
        const auto fpu0  = core->GetAuthenticityStats().fpuOpcodesExecuted;
        const auto tw0 = std::chrono::steady_clock::now();
        TapKey(*core, kF1, 3, 15);
        Stabilize(*core, 6, 800);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tw0).count();
        const auto di = core->GetStats().instructions - inst0;
        const double mips = ms > 0.0 ? (di / (ms * 1000.0)) : 0.0;
        workloads.push_back({"micropython_compute", di, core->GetAuthenticityStats().bcdAluOpsExecuted - bcd0,
                             core->GetAuthenticityStats().fpuOpcodesExecuted - fpu0, ms, mips, mips / 58.0});
    }

    // 3d. 2D Graph: Polar Rose r1=cos(11*theta) + LCD frame count
    {
        auto core = std::make_unique<Cg50NativeCore>();
        core->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
        core->LoadC2Rom(c2.data(), c2.size());
        core->ImportState(warm.data(), warm.size());
        core->SetStrictSh7305NoFpu(true);

        const auto inst0 = core->GetStats().instructions;
        const auto bcd0  = core->GetAuthenticityStats().bcdAluOpsExecuted;
        const auto fpu0  = core->GetAuthenticityStats().fpuOpcodesExecuted;
        const auto tw0 = std::chrono::steady_clock::now();
        int uniqueFrames = 0;
        std::uint64_t lastH = HashLcd(*core);
        for (std::uint16_t k : {kDig5, kF3, kF2, kCos, kLParen, kDig1, kDig1, kXVar, kRParen, kExe, kF6}) {
            TapKey(*core, k, 3, 18);
            const auto ch = HashLcd(*core);
            if (ch != lastH) { ++uniqueFrames; lastH = ch; }
        }
        for (int c = 0; c < 120; ++c) {
            StepChunks(*core, 1);
            const auto ch = HashLcd(*core);
            if (ch != lastH) { ++uniqueFrames; lastH = ch; }
            if (core->GetState() == Cg50NativeCore::State::Sleeping && c > 25) break;
        }
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tw0).count();
        const auto di = core->GetStats().instructions - inst0;
        const double mips = ms > 0.0 ? (di / (ms * 1000.0)) : 0.0;
        graphEffectiveFps = ms > 0.0 ? (1000.0 * uniqueFrames / ms) : 0.0;
        workloads.push_back({"graph_polar_rose", di, core->GetAuthenticityStats().bcdAluOpsExecuted - bcd0,
                             core->GetAuthenticityStats().fpuOpcodesExecuted - fpu0, ms, mips, mips / 58.0});
    }

    for (const auto& w : workloads) {
        std::printf("[PERF] %-22s | inst=%10llu | %.1f ms | %.2f MIPS (%.2fx 58MHz) | bcd=%llu fpu=%llu\n",
                    w.name.c_str(), static_cast<unsigned long long>(w.instructions),
                    w.elapsedMs, w.mips, w.equivRealSpeedupX,
                    static_cast<unsigned long long>(w.bcdOps),
                    static_cast<unsigned long long>(w.fpuOps));
    }

    // 4. Empirical Thread Scaling: 1, 2, 4, 8, 12, 16 threads (each running identical real Casio OS compute workload!)
    const std::vector<unsigned> threadCounts = {1u, 2u, 4u, 8u, 12u, 16u};
    std::vector<ThreadScalePoint> scaling;
    double baseline1TMips = 0.0;

    for (unsigned tc : threadCounts) {
        // Pre-allocate tc cores cleanly BEFORE timing pure execution loop
        std::vector<std::unique_ptr<Cg50NativeCore>> cores(tc);
        for (unsigned i = 0; i < tc; ++i) {
            cores[i] = std::make_unique<Cg50NativeCore>();
            cores[i]->LoadFirmwareParts(sys.data(), sys.size(), user.data(), user.size());
            cores[i]->LoadC2Rom(c2.data(), c2.size());
            cores[i]->ImportState(warm.data(), warm.size());
            cores[i]->SetStrictSh7305NoFpu(true);
            TapKey(*cores[i], kDig1, 3, 18);
            Stabilize(*cores[i], 3, 40);
        }
        const double rss = CurrentRssMb();
        std::vector<std::uint64_t> instPerThread(tc, 0u);
        const auto t0 = std::chrono::steady_clock::now();
        std::vector<std::thread> pool;
        pool.reserve(tc);
        for (unsigned i = 0; i < tc; ++i) {
            pool.emplace_back([&, i]() {
                auto& c = *cores[i];
                const auto i0 = c.GetStats().instructions;
                for (std::uint16_t k : {kOptn, kF4, kF4, kXVar, kPow, kDig3, kRight, kRight, kDig0, kRight, kDig3, kExe}) {
                    TapKey(c, k, 2, 12);
                }
                Stabilize(c, 4, 150);
                instPerThread[i] = c.GetStats().instructions - i0;
            });
        }
        for (auto& th : pool) th.join();
        const double wallMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::uint64_t sumInst = 0;
        for (auto v : instPerThread) sumInst += v;
        const double aggMips = wallMs > 0.0 ? (sumInst / (wallMs * 1000.0)) : 0.0;
        if (tc == 1u) baseline1TMips = aggMips;
        ThreadScalePoint pt;
        pt.threads = tc;
        pt.totalInstructions = sumInst;
        pt.wallMs = wallMs;
        pt.aggregateMips = aggMips;
        pt.perThreadMips = aggMips / static_cast<double>(tc);
        pt.speedupVs1T = baseline1TMips > 0.0 ? (aggMips / baseline1TMips) : 1.0;
        pt.rssMb = rss;
        scaling.push_back(pt);
        std::printf("[PERF] Scale %2uT | total_inst=%11llu | wall=%7.1f ms | agg=%6.1f MIPS | per_T=%5.1f MIPS | speedup=%.2fx | RSS=%.1f MB\n",
                    tc, static_cast<unsigned long long>(sumInst), wallMs, aggMips, pt.perThreadMips, pt.speedupVs1T, rss);
        std::fflush(stdout);
    }

    // Write structured JSON report
    fs::create_directories(outJson.parent_path());
    std::ofstream jf(outJson, std::ios::binary);
    jf << "{\n"
       << "  \"schema\": \"cg50_perf_benchmark_v46\",\n"
       << "  \"label\": \"" << label << "\",\n"
       << "  \"hardware_concurrency\": " << std::thread::hardware_concurrency() << ",\n"
       << "  \"cold_boot_ms\": " << std::fixed << std::setprecision(2) << coldBootMs << ",\n"
       << "  \"cold_boot_instructions\": " << coldBootInst << ",\n"
       << "  \"warm_restore_ms\": " << std::fixed << std::setprecision(3) << warmRestoreMs << ",\n"
       << "  \"state_export_ms\": " << std::fixed << std::setprecision(3) << stateExportMs << ",\n"
       << "  \"state_export_bytes\": " << stateBytes << ",\n"
       << "  \"keyboard_response_latency_ms\": " << std::fixed << std::setprecision(3) << keyLatencyMs << ",\n"
       << "  \"graph_effective_fps\": " << std::fixed << std::setprecision(2) << graphEffectiveFps << ",\n"
       << "  \"peak_rss_mb\": " << std::fixed << std::setprecision(2) << PeakRssMb() << ",\n"
       << "  \"workloads\": [\n";
    for (std::size_t i = 0; i < workloads.size(); ++i) {
        const auto& w = workloads[i];
        jf << "    {\"name\":\"" << w.name << "\",\"instructions\":" << w.instructions
           << ",\"bcd_ops\":" << w.bcdOps << ",\"fpu_ops\":" << w.fpuOps
           << ",\"elapsed_ms\":" << std::fixed << std::setprecision(2) << w.elapsedMs
           << ",\"mips\":" << std::fixed << std::setprecision(2) << w.mips
           << ",\"speedup_vs_58mhz\":" << std::fixed << std::setprecision(3) << w.equivRealSpeedupX << "}"
           << (i + 1 < workloads.size() ? "," : "") << "\n";
    }
    jf << "  ],\n"
       << "  \"thread_scaling\": [\n";
    for (std::size_t i = 0; i < scaling.size(); ++i) {
        const auto& s = scaling[i];
        jf << "    {\"threads\":" << s.threads << ",\"total_instructions\":" << s.totalInstructions
           << ",\"wall_ms\":" << std::fixed << std::setprecision(2) << s.wallMs
           << ",\"aggregate_mips\":" << std::fixed << std::setprecision(2) << s.aggregateMips
           << ",\"per_thread_mips\":" << std::fixed << std::setprecision(2) << s.perThreadMips
           << ",\"speedup_vs_1t\":" << std::fixed << std::setprecision(3) << s.speedupVs1T
           << ",\"rss_mb\":" << std::fixed << std::setprecision(2) << s.rssMb << "}"
           << (i + 1 < scaling.size() ? "," : "") << "\n";
    }
    jf << "  ]\n}\n";
    jf.close();
    std::printf("[PERF] Wrote %s\n", outJson.string().c_str());
    return 0;
}
