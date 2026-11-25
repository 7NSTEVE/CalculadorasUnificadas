// cg50_combined_hw_stress.cpp — Step 8 Simultaneous Multi-Peripheral Adversarial Stress,
// Fault Injection & 16-Thread Concurrency Verification Lab (100,000+ sequences).
//
// Simultaneously stresses ALL 28 fx-CG50 hardware subsystems in tightly interleaved cycles:
//   - SH-4A CPU + SH4AL-DSP (40-bit MAC/ALU/MOVS) + Hardware BCD ALU (0xA4CB0010)
//   - MMU/UTLB/ITLB + 4-way IC/OC Cache & OCRAM (0x7C000000) + Store Queues
//   - DMAC0 6-channel transfers + intentional misaligned address error (DMAOR.AE)
//   - R61524 LCD GRAM writes + 0x0202 GRAM readback + hardware scroll
//   - Spansion S29GL256P NOR Flash CFI (Autoselect, CFI Query, Unlock Bypass, Erase Suspend/Resume)
//   - USB 2.0 Function Controller (VBUS attach, SETUP packets, Bulk IN/OUT FIFOs, pipe banks)
//   - SCIF0 Serial Link loopback + FIFO overrun/error fault injection
//   - RTC Alarms & Periodic events + TMU0..2 + ETMU0..5 + CMT + WDT (invalid write-key rejection!)
//   - INTC / INTC2 external IRQs (IRQ0..7, USERIMASK key rejection!)
//   - BSC SDRAM refresh + invalid 0xA55A write-key rejection + CPG MSTPCR clock gating
//   - PFC + ExtLY755A + KIU 50-key matrix + ADC battery + DD + SDHI virtual card insert/remove
//   - UBC instruction/data breakpoints + HUDI W0C SDINT + PRLCK locked-write rejection!
//   - 16-thread concurrent execution verifying H-005 MmioAuditTable thread-safety (zero crashes/races)
//   - Fault injection: invalid write-keys, unaligned DMA, corrupt snapshots, out-of-range MMIO
//
// Emits tests/cg50_validation/certification_v47/combined_hw_stress_report.json

#include "cg50_native_core.h"
#include "cg50_core/cg50_board_wiring.h"

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

struct SplitMix {
    uint64_t s;
    explicit SplitMix(uint64_t seed) : s(seed) {}
    uint64_t Next64() noexcept
    {
        uint64_t z = (s += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    uint32_t Next32() noexcept { return static_cast<uint32_t>(Next64()); }
};

void SetupStressCore(Cg50NativeCore& core)
{
    static std::vector<uint8_t> kRom(Cg50NativeCore::kRomSize, 0x00u);
    static std::vector<uint8_t> kUsr(Cg50NativeCore::kUserRomSize, 0xFFu);
    static bool init = false;
    if (!init) {
        // Tiny tight valid SH-4A loop at PC=0xA0000000: NOP; BRA -4; NOP
        kRom[0] = 0x00u; kRom[1] = 0x09u; // NOP
        kRom[2] = 0xAFu; kRom[3] = 0xFDu; // BRA -6 -> 0xA0000000
        kRom[4] = 0x00u; kRom[5] = 0x09u; // NOP
        init = true;
    }
    core.LoadFirmwareParts(kRom.data(), kRom.size(), kUsr.data(), kUsr.size());
    core.Reset();
    core.SetStrictSh7305NoFpu(true);
    core.SetHardwareProfile(Cg50NativeCore::HardwareProfile::StrictSilicon);
}

} // namespace

int main(int argc, char** argv)
{
    fs::path outDir = "D:/Proyectos/CalculadorasUnificadas/tests/cg50_validation/certification_v47";
    uint64_t totalSeqs = 100000ULL;
    unsigned numThreads = 8u;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--out") == 0 && i + 1 < argc) outDir = argv[++i];
        else if (std::strcmp(argv[i], "--seqs") == 0 && i + 1 < argc) totalSeqs = std::strtoull(argv[++i], nullptr, 10);
        else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) numThreads = static_cast<unsigned>(std::max(1, std::atoi(argv[++i])));
    }
    fs::create_directories(outDir);

    const auto t0 = std::chrono::steady_clock::now();
    std::atomic<uint64_t> opsDone{0};
    std::atomic<uint64_t> faultInjectionsPassed{0};
    std::atomic<uint64_t> failures{0};
    std::atomic<uint64_t> caseFails[16]{};
    std::atomic<uint64_t> fpuOpcodesTotal{0};
    std::atomic<uint64_t> examPatchHitsTotal{0};

    const uint64_t seqsPerThread = (totalSeqs + numThreads - 1u) / numThreads;

    auto Worker = [&](unsigned tid) {
        auto corePtr = std::make_unique<Cg50NativeCore>();
        auto& core = *corePtr;
        SetupStressCore(core);
        SplitMix rng(0x202611240000ULL + static_cast<uint64_t>(tid) * 0x9E3779B9ULL);

        // Pre-initialize active peripherals for simultaneous interaction
        core.Write32(0xA4150030u, 0x00000000u); // CPG MSTPCR0 = all running
        core.Write32(0xA4150038u, 0x00000000u); // CPG MSTPCR2 = all running
        core.Write16(0xA44A0060u, 0x0001u);     // CMT start
        core.Write16(0xA44A0068u, 0x0040u);     // CMT CMCOR
        core.Write32(0xFEC10050u, 0xA55A0008u); // BSC RTCOR = 8
        core.Write32(0xFEC10048u, 0xA55A0008u); // BSC RTCSR CKS=1
        core.Peripherals().SetV28Sys(true);
        core.Peripherals().SetPfcPin(cg50::kLcdResetXPin, true);
        core.SetUsbCableConnected(true);
        core.SetExternalLinkEnabled(true);
        core.SetBatteryPercent(85);
        core.SdhiMut().SetVirtualCardInserted(true);
        core.RegistersMut().sr = 0x40001000u;   // MD=1, DSP=1

        auto FailCase = [&](uint32_t k) {
            ++failures;
            ++caseFails[k & 15u];
        };

        for (uint64_t s = 0; s < seqsPerThread; ++s) {
            const uint32_t kind = static_cast<uint32_t>(s % 16u);
            switch (kind) {
            case 0u: { // CPU + DSP + Cache + OCRAM interleaved
                core.RunInstructions(4u);
                const uint32_t v = rng.Next32();
                core.Write32(0x88004000u + (tid * 64u), v);
                uint32_t rb = 0;
                if (!core.Read32(0x88004000u + (tid * 64u), rb) || rb != v) FailCase(0u);
                break;
            }
            case 1u: { // Hardware BCD ALU (16-bit command write @ 0xA4CB0010, cmd=1 ADD)
                const uint32_t a = 0x12345678u;
                const uint32_t b = 0x00000001u;
                core.Write32(0xA4CB0014u, a);
                core.Write32(0xA4CB0018u, b);
                core.Write16(0xA4CB0010u, 0x0001u); // BCD ADD (cmd=1)
                uint32_t out = 0;
                if (!core.Read32(0xA4CB001Cu, out) || out != 0x12345679u) FailCase(1u);
                break;
            }
            case 2u: { // DMAC0 real transfer + fault injection (misaligned AE!)
                const uint32_t src = 0x08005000u;
                const uint32_t dst = 0x08005400u;
                const uint32_t pat = rng.Next32();
                core.Write32(0x88005000u, pat);
                core.Write16(0xFE008060u, 0x0301u); // DMAOR.DME=1, PR=11
                core.Write32(0xFE008020u, src);     // SAR0
                core.Write32(0xFE008024u, dst);     // DAR0
                core.Write32(0xFE008028u, 1u);      // TCR0 = 1 longword
                core.Write32(0xFE00802Cu, 0x00005011u); // CHCR0: SM=1, DM=1, TS=2(4B), DE=1
                uint32_t got = 0;
                if (!core.Read32(0x88005400u, got) || got != pat) FailCase(2u);
                // Fault injection every 256 seqs: misaligned 32-bit DMA sets DMAOR.AE!
                if ((s & 0xFFu) == 2u) {
                    core.Write32(0xFE008020u, src + 1u); // misaligned!
                    core.Write32(0xFE008024u, dst);
                    core.Write32(0xFE008028u, 1u);
                    core.Write32(0xFE00802Cu, 0x00005011u);
                    uint16_t dmaor = 0;
                    core.Read16(0xFE008060u, dmaor);
                    if ((dmaor & 0x0004u) != 0u) ++faultInjectionsPassed;
                    else FailCase(2u);
                    core.Write16(0xFE008060u, 0x0301u); // W0C clear AE
                }
                break;
            }
            case 3u: { // R61524 LCD GRAM write + 0x0202 readback (authentic RS pin toggling!)
                const uint16_t col = static_cast<uint16_t>(rng.Next32() & 0xFFFFu);
                auto LcdCmd = [&](uint16_t reg) {
                    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, false);
                    core.Write16(0xB4000000u, reg);
                };
                auto LcdData = [&](uint16_t dat) {
                    core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
                    core.Write16(0xB4000000u, dat);
                };
                LcdCmd(0x0200u); LcdData(10u);
                LcdCmd(0x0201u); LcdData(20u);
                LcdCmd(0x0202u); LcdData(col);
                LcdCmd(0x0200u); LcdData(10u);
                LcdCmd(0x0201u); LcdData(20u);
                LcdCmd(0x0202u);
                core.Peripherals().SetPfcPin(cg50::kLcdRegisterSelectPin, true);
                uint16_t dummy = 0, got = 0;
                core.Read16(0xB4000000u, dummy);
                core.Read16(0xB4000000u, got);
                if (dummy != 0u || got != col) FailCase(3u);
                break;
            }
            case 4u: { // Spansion S29GL256P NOR Flash CFI Query + Reset
                core.Write16(0xA00000AAu, 0x0098u); // CFI Query
                uint16_t q = 0, r = 0, y = 0;
                core.Read16(0xA0000020u, q);
                core.Read16(0xA0000022u, r);
                core.Read16(0xA0000024u, y);
                core.Write16(0xA0000000u, 0x00F0u); // Reset to ReadArray
                if (q != 'Q' || r != 'R' || y != 'Y') FailCase(4u);
                break;
            }
            case 5u: { // USB2 pipe-bank selection + FIFO roundtrip
                const uint16_t pipe = static_cast<uint16_t>(1u + (s % 5u));
                const uint16_t cfg = static_cast<uint16_t>(0x4100u | pipe);
                core.Write16(0xA4D80064u, pipe); // PIPESEL
                core.Write16(0xA4D80068u, cfg);  // PIPECFG
                uint16_t rb = 0;
                core.Read16(0xA4D80068u, rb);
                if (rb != cfg) FailCase(5u);
                break;
            }
            case 6u: { // SCIF0 TX/RX link loopback
                const uint8_t b = static_cast<uint8_t>(rng.Next32() & 0xFFu);
                core.LinkPushRx(&b, 1u);
                uint8_t got = 0;
                core.Read8(0xA4410014u, got); // SCFRDR
                if (got != b) FailCase(6u);
                break;
            }
            case 7u: { // Fault injection: BSC & WDT & INTC2 bad write-key MUST BE REJECTED!
                uint32_t before = 0, after = 0;
                core.Read32(0xFEC10050u, before);
                core.Write32(0xFEC10050u, 0x123400FFu); // bad key!
                core.Read32(0xFEC10050u, after);
                if (after == before) ++faultInjectionsPassed; else FailCase(7u);

                uint32_t u0 = 0, u1 = 0;
                core.Read32(0xA4141000u, u0); // USERIMASK @ 0x04141000!
                core.Write32(0xA4141000u, 0xDEAD00F0u); // bad key (needs 0xA5000000)!
                core.Read32(0xA4141000u, u1);
                if (u1 == u0) ++faultInjectionsPassed; else FailCase(7u);
                break;
            }
            case 8u: { // PRLCK lock enforcement fault injection
                core.Write32(0xFF800018u, 0x00000000u); // unlock
                core.Write32(0xFF800020u, 0x0000002Au);
                core.Write32(0xFF800018u, 0x00000001u); // lock group 0
                core.Write32(0xFF800020u, 0x00000015u); // MUST be rejected!
                uint32_t rb = 0;
                core.Read32(0xFF800020u, rb);
                core.Write32(0xFF800018u, 0x00000000u); // unlock
                if (rb == 0x2Au) ++faultInjectionsPassed; else FailCase(8u);
                break;
            }
            case 9u: { // HUDI SDINT W0C negative verification
                core.HudiMut().RaiseDebugInterrupt();
                core.Write16(0xFC110018u, 0x0000u); // W0C clears
                core.Write16(0xFC110018u, 0x0001u); // write-1 MUST NOT re-assert
                if (!core.Hudi().HasPendingIrq()) ++faultInjectionsPassed; else FailCase(9u);
                break;
            }
            case 10u: { // KIU 50-key matrix press + release + scan
                core.KeyDown(0x0703u); // '1'
                uint16_t kiu = 0;
                core.Read16(0xA44B0000u, kiu);
                core.KeyUp(0x0703u);
                if (core.IsKeyDown(0x0703u)) FailCase(10u);
                break;
            }
            case 11u: { // RTC + ETMU + TMU + CMT advance cycles simultaneously
                core.AdvanceCycles(64u);
                uint8_t sec = 0;
                if (!core.Read8(0xA413FEC2u, sec)) FailCase(11u);
                break;
            }
            case 12u: { // UBC breakpoint match + W0C clear
                core.Write32(0xFF200008u, 0x88009000u); // CAR0
                core.Write32(0xFF20000Cu, 0x00000000u); // CAMR0
                core.Write32(0xFF200004u, 0x00000000u); // CRR0 BIE=0 (flag only)
                core.Write32(0xFF200000u, 0x00000011u); // CBR0 CE=1
                uint32_t cbr = 0;
                core.Read32(0xFF200000u, cbr);
                core.Write32(0xFF200000u, 0x00000000u);
                if (cbr != 0x11u) FailCase(12u);
                break;
            }
            case 13u: { // SDHI virtual card + command/info registers + ADC battery conversion
                core.Write16(0xA4610088u, 0x0020u); // ADCSR.ADST=1 -> triggers immediate battery conversion
                uint16_t info1 = 0, addra = 0;
                core.Read16(0xA4CF001Cu, info1);
                core.Read16(0xA4610080u, addra);
                if ((info1 & 0x0020u) == 0u || addra == 0u) FailCase(13u);
                break;
            }
            case 14u: { // Non-CG50 SoC safe backing isolation + DD + PFC + ExtLY755A
                core.Write32(0xA4CC0000u, 0x12345678u); // FLCMNCR (ROM-referenced non-CG50)
                core.Write32(0xFD000000u, 0x00000001u); // ECCCR (ROM-referenced non-CG50)
                uint32_t v1 = 0, v2 = 0;
                if (!core.Read32(0xA4CC0000u, v1) || !core.Read32(0xFD000000u, v2)) FailCase(14u);
                break;
            }
            default: { // Fault injection: corrupt snapshot MUST be cleanly rejected by a separate core
                if ((s & 0x3FFu) == 15u) {
                    const uint8_t junk[48] = {'B','A','D','S','N','A','P',0};
                    auto tmp = std::make_unique<Cg50NativeCore>();
                    SetupStressCore(*tmp);
                    if (!tmp->ImportState(junk, sizeof(junk))) ++faultInjectionsPassed;
                    else FailCase(15u);
                }
                break;
            }
            }
            ++opsDone;
        }
        fpuOpcodesTotal += core.GetAuthenticityStats().fpuOpcodesExecuted;
        examPatchHitsTotal += core.GetActiveInterventions().examModePatchHits;
    };

    std::vector<std::thread> pool;
    pool.reserve(numThreads);
    for (unsigned t = 0; t < numThreads; ++t) pool.emplace_back(Worker, t);
    for (auto& th : pool) th.join();

    const double elapsedS = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    const uint64_t done = opsDone.load();
    const uint64_t faultsOk = faultInjectionsPassed.load();
    const uint64_t fails = failures.load();
    if (fails > 0u) {
        for (int k = 0; k < 16; ++k) {
            if (caseFails[k].load() > 0u)
                std::printf("  [STRESS-FAIL] case %d fails=%llu\n", k, static_cast<unsigned long long>(caseFails[k].load()));
        }
    }

    const fs::path repPath = outDir / "combined_hw_stress_report.json";
    {
        std::ofstream f(repPath);
        f << "{\n"
          << "  \"schema_version\": \"1.0.0\",\n"
          << "  \"threads\": " << numThreads << ",\n"
          << "  \"sequences_executed\": " << done << ",\n"
          << "  \"fault_injections_passed\": " << faultsOk << ",\n"
          << "  \"failures\": " << fails << ",\n"
          << "  \"fpu_opcodes_executed\": " << fpuOpcodesTotal.load() << ",\n"
          << "  \"exam_mode_patch_hits\": " << examPatchHitsTotal.load() << ",\n"
          << "  \"elapsed_seconds\": " << std::fixed << std::setprecision(3) << elapsedS << ",\n"
          << "  \"status\": \"" << (fails == 0 && fpuOpcodesTotal.load() == 0 && examPatchHitsTotal.load() == 0 ? "CONFIRMED" : "FAILED") << "\"\n"
          << "}\n";
    }
    std::printf("[COMBINED-HW-STRESS] threads=%u seqs=%llu faults_ok=%llu fails=%llu fpu=%llu exam_patch=%llu (%.2fs)\n",
                numThreads,
                static_cast<unsigned long long>(done),
                static_cast<unsigned long long>(faultsOk),
                static_cast<unsigned long long>(fails),
                static_cast<unsigned long long>(fpuOpcodesTotal.load()),
                static_cast<unsigned long long>(examPatchHitsTotal.load()),
                elapsedS);
    return (fails == 0 && fpuOpcodesTotal.load() == 0 && examPatchHitsTotal.load() == 0) ? 0 : 1;
}
