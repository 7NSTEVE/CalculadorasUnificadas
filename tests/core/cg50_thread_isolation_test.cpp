#include "cg50_core/cg50_smem_fs.h"
#include "cg50_native_core.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

bool ReadFileExact(const std::string& path, std::vector<std::uint8_t>& out)
{
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;
    const auto sz = ifs.tellg();
    if (sz <= 0) return false;
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    return static_cast<bool>(ifs.read(reinterpret_cast<char*>(out.data()), sz));
}

std::uint64_t Fnv1a64(const void* data, std::size_t len) noexcept
{
    const auto* p = static_cast<const std::uint8_t*>(data);
    std::uint64_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < len; ++i) {
        h ^= static_cast<std::uint64_t>(p[i]);
        h *= 1099511628211ULL;
    }
    return h;
}

struct WorkerResult {
    bool ok = false;
    std::uint64_t fbHash = 0;
    std::uint64_t regHash = 0;
    std::uint64_t instructions = 0;
    std::uint64_t bcdOps = 0;
    std::uint64_t fpuOps = 0;
    std::uint64_t smemRoundtripHash = 0;
    std::string error;
};

void StepCore(Cg50NativeCore& core, std::uint64_t runBudget, std::uint64_t sleepCycles)
{
    if (core.GetState() == Cg50NativeCore::State::Running && runBudget > 0) {
        core.RunInstructions(runBudget);
    }
    if (core.GetState() == Cg50NativeCore::State::Sleeping && sleepCycles > 0) {
        constexpr std::uint64_t kChunk = 2048;
        std::uint64_t rem = sleepCycles;
        while (rem > 0) {
            const std::uint64_t c = rem < kChunk ? rem : kChunk;
            if (core.AdvanceCycles(c)) break;
            rem -= c;
        }
    }
}

void TapKey(Cg50NativeCore& core, std::uint16_t code)
{
    core.KeyDown(code);
    StepCore(core, 600000ULL, 8000ULL);
    StepCore(core, 1500000ULL, 12000ULL);
    StepCore(core, 1500000ULL, 12000ULL);
    core.KeyUp(code);
    StepCore(core, 1500000ULL, 16000ULL);
    StepCore(core, 1500000ULL, 16000ULL);
}

WorkerResult RunDeterministicWorkload(
    int workerIdx,
    const std::vector<std::uint8_t>& sysRom,
    const std::vector<std::uint8_t>& userRom,
    const std::vector<std::uint8_t>& c2Rom,
    const std::vector<std::uint8_t>& ilramSeed,
    const std::vector<std::uint8_t>& warmState)
{
    WorkerResult res;
    auto core = std::make_unique<Cg50NativeCore>();
    core->SetStrictSh7305NoFpu(true);
    if (!ilramSeed.empty()) {
        core->SetIlramSeed(ilramSeed.data(), ilramSeed.size());
    }
    if (!core->LoadFirmwareParts(sysRom.data(), sysRom.size(),
                                 userRom.data(), userRom.size())) {
        res.error = "LoadFirmwareParts failed";
        return res;
    }
    if (!c2Rom.empty()) {
        core->LoadC2Rom(c2Rom.data(), c2Rom.size());
    }
    if (!core->ImportState(warmState.data(), warmState.size())) {
        res.error = "ImportState(warmState) failed";
        return res;
    }

    // Fixed deterministic RTC epoch so wall clock never introduces variance across threads
    core->SetRtcEpochSeconds(1735689600LL); // 2025-01-01 00:00:00 UTC
    StepCore(*core, 200000ULL, 4000ULL);

    // Heterogeneous workload per worker class (all workers of same class must be bit-identical!)
    const int profile = workerIdx % 4;
    if (profile == 0) {
        // Run-Matrix: 12345 + 67890 EXE
        TapKey(*core, 0x0302); // EXE -> Run-Matrix
        TapKey(*core, 0x0703); // 1
        TapKey(*core, 0x0603); // 2
        TapKey(*core, 0x0503); // 3
        TapKey(*core, 0x0704); // 4
        TapKey(*core, 0x0604); // 5
        TapKey(*core, 0x0403); // +
        TapKey(*core, 0x0504); // 6
        TapKey(*core, 0x0705); // 7
        TapKey(*core, 0x0605); // 8
        TapKey(*core, 0x0505); // 9
        TapKey(*core, 0x0702); // 0
        TapKey(*core, 0x0302); // EXE
    } else if (profile == 1) {
        // Run-Matrix trigonometry + S<->D: sin(1) EXE
        TapKey(*core, 0x0302); // EXE
        TapKey(*core, 0x0407); // sin
        TapKey(*core, 0x0703); // 1
        TapKey(*core, 0x0302); // EXE
    } else if (profile == 2) {
        // SMEM Fugue FTL + FAT16 write/read + Save/Restore roundtrip + Catalog QR
        cg50::Cg50SmemFs fs;
        if (!fs.Mount(core->UserRomMut().data(), core->UserRomMut().size(),
                      core->C2RomData(), core->C2RomSize())) {
            res.error = "SMEM Mount failed";
            return res;
        }
        const std::string payload = "print('isolation_check', 7*11*13)\n";
        if (!fs.WriteFile("/iso_test.py",
                          reinterpret_cast<const std::uint8_t*>(payload.data()),
                          payload.size(), true)) {
            res.error = "SMEM WriteFile failed";
            return res;
        }
        std::vector<std::uint8_t> rb;
        if (!fs.ReadFile("/iso_test.py", rb) || rb.size() != payload.size()) {
            res.error = "SMEM ReadFile failed";
            return res;
        }
        res.smemRoundtripHash = Fnv1a64(rb.data(), rb.size());

        std::vector<std::uint8_t> snap;
        if (!core->ExportState(snap) || !core->ImportState(snap.data(), snap.size())) {
            res.error = "Export/Import state roundtrip failed";
            return res;
        }
        TapKey(*core, 0x0302); // EXE -> Run-Matrix
        TapKey(*core, 0x0603); // 2
        TapKey(*core, 0x0404); // x
        TapKey(*core, 0x0503); // 3
        TapKey(*core, 0x0302); // EXE
    } else {
        // SCIF0 external link + USB_DET + Graph entry
        core->SetUsbCableConnected(true);
        core->SetExternalLinkEnabled(true);
        const std::uint8_t rx[4] = {0x05u, '0', '0', '1'};
        core->LinkPushRx(rx, 4u);
        TapKey(*core, 0x0604); // 5 -> Graph
        TapKey(*core, 0x0707); // X,theta,T
        TapKey(*core, 0x0302); // EXE
        TapKey(*core, 0x020A); // F6 DRAW
        core->SetUsbCableConnected(false);
        core->SetExternalLinkEnabled(false);
    }

    // Hash framebuffer + CPU registers deterministically
    const auto& lcd = core->Peripherals().Lcd();
    if (lcd.FramebufferAvailable() && lcd.FramebufferData() != nullptr) {
        constexpr std::size_t kPixels = cg50::R61524::kVisibleWidth * cg50::R61524::kVisibleHeight;
        res.fbHash = Fnv1a64(lcd.FramebufferData(), kPixels * sizeof(std::uint16_t));
    }
    const auto& regs = core->GetRegisters();
    res.regHash = Fnv1a64(&regs, sizeof(regs));
    res.instructions = core->GetStats().instructions;
    res.bcdOps = core->GetAuthenticityStats().bcdAluOpsExecuted;
    res.fpuOps = core->GetAuthenticityStats().fpuOpcodesExecuted;
    res.ok = (res.fbHash != 0 && res.fpuOps == 0);
    if (!res.ok) {
        res.error = (res.fpuOps != 0) ? "Unexpected FPU opcode executed" : "Empty framebuffer hash";
    }
    return res;
}

} // namespace

int main()
{
    const std::string romDir = CASIO_ROMS_DIR;
    std::vector<std::uint8_t> sysRom, userRom, c2Rom, ilramSeed, warmState;
    if (!ReadFileExact(romDir + "/SystemROM.raw", sysRom) ||
        !ReadFileExact(romDir + "/UserROM.raw", userRom) ||
        !ReadFileExact(romDir + "/C2ROM.raw", c2Rom) ||
        !ReadFileExact(romDir + "/cg50_warm_state.bin", warmState)) {
        std::fprintf(stderr, "[cg50_thread_isolation_test] FAIL: Missing ROMs or cg50_warm_state.bin in %s\n",
                     romDir.c_str());
        return 1;
    }
    (void)ReadFileExact(romDir + "/official-ILRAM-FD800000-4000.bin", ilramSeed);

    // 1. Run 4 reference profiles serially on 1 thread
    WorkerResult serialRef[4];
    for (int p = 0; p < 4; ++p) {
        serialRef[p] = RunDeterministicWorkload(p, sysRom, userRom, c2Rom, ilramSeed, warmState);
        if (!serialRef[p].ok) {
            std::fprintf(stderr, "[cg50_thread_isolation_test] FAIL serial profile %d: %s\n",
                         p, serialRef[p].error.c_str());
            return 1;
        }
        std::printf("[cg50_thread_isolation_test] Serial profile %d: fb=%016llX reg=%016llX inst=%llu bcd=%llu\n",
                    p,
                    static_cast<unsigned long long>(serialRef[p].fbHash),
                    static_cast<unsigned long long>(serialRef[p].regHash),
                    static_cast<unsigned long long>(serialRef[p].instructions),
                    static_cast<unsigned long long>(serialRef[p].bcdOps));
    }

    // 2. Run 16 concurrent threads (4 copies of each profile simultaneously)
    constexpr int kThreads = 16;
    std::vector<WorkerResult> parallelRes(kThreads);
    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        workers.emplace_back([&, i]() {
            parallelRes[i] = RunDeterministicWorkload(i, sysRom, userRom, c2Rom, ilramSeed, warmState);
        });
    }
    for (auto& t : workers) t.join();

    // 3. Assert 100% bit-exact equivalence across all 16 threads vs serial golden reference
    for (int i = 0; i < kThreads; ++i) {
        const auto& r = parallelRes[i];
        const auto& ref = serialRef[i % 4];
        if (!r.ok ||
            r.fbHash != ref.fbHash ||
            r.regHash != ref.regHash ||
            r.instructions != ref.instructions ||
            r.bcdOps != ref.bcdOps ||
            r.fpuOps != 0 ||
            r.smemRoundtripHash != ref.smemRoundtripHash) {
            std::fprintf(stderr,
                         "[cg50_thread_isolation_test] FAIL thread %d (profile %d): "
                         "fb=%016llX vs %016llX, reg=%016llX vs %016llX, inst=%llu vs %llu (%s)\n",
                         i, i % 4,
                         static_cast<unsigned long long>(r.fbHash),
                         static_cast<unsigned long long>(ref.fbHash),
                         static_cast<unsigned long long>(r.regHash),
                         static_cast<unsigned long long>(ref.regHash),
                         static_cast<unsigned long long>(r.instructions),
                         static_cast<unsigned long long>(ref.instructions),
                         r.error.c_str());
            return 1;
        }
    }

    std::printf("[cg50_thread_isolation_test] PASS: 16/16 parallel threads matched 1-thread serial bit-for-bit (0 shared mutable state)\n");
    return 0;
}
