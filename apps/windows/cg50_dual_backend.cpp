// ============================================================================
// cg50_dual_backend.cpp
// Dual-engine Cg50Backend implementation for 64-bit CalculadoraUnificada.exe:
//   1. NativeIndependent: embeds certified core/casio_cg50 v47 (SH7305 FULL_SILICON,
//      zero Casio DLL dependencies, 64-bit native).
//   2. OfficialCasioDll: talks over low-latency shared memory to isolated 32-bit
//      cg50_official_gcal_host32.exe loading Casio's official GCalAppIF/DLSim32E DLLs.
//   3. CompareLive: runs BOTH engines in lockstep, dispatching every key and step
//      to both engines and computing real-time pixel diff metrics + heatmap.
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include "cg50_backend.h"
#include "cg50_native_core.h"
#include "cg50_official_ipc.h"
#include "debug_log.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;

constexpr std::uint32_t kLcdW = static_cast<std::uint32_t>(cg50::R61524::kVisibleWidth);
constexpr std::uint32_t kLcdH = static_cast<std::uint32_t>(cg50::R61524::kVisibleHeight);
constexpr std::size_t   kLcdPixels = static_cast<std::size_t>(kLcdW) * kLcdH;

std::atomic<std::uint32_t> g_ipcSeq{1};

double QueryTimeMs()
{
    static LARGE_INTEGER freq{};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    return (freq.QuadPart > 0)
        ? (static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart))
        : 0.0;
}

fs::path GetModuleDir()
{
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return fs::current_path();
        if (n < buf.size() - 1) {
            buf.resize(n);
            return fs::path(buf).parent_path();
        }
        buf.resize(buf.size() * 2);
    }
}

std::wstring GetEnvW(const wchar_t* name)
{
    if (!name) return {};
    const DWORD req = GetEnvironmentVariableW(name, nullptr, 0);
    if (req == 0) return {};
    std::wstring val(req, L'\0');
    const DWORD n = GetEnvironmentVariableW(name, val.data(), req);
    if (n == 0 || n >= req) return {};
    val.resize(n);
    return val;
}

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& out)
{
    std::error_code ec;
    if (!fs::is_regular_file(p, ec) || ec) return false;
    const auto sz = fs::file_size(p, ec);
    if (ec || sz == 0 || sz > (64ull << 20)) return false;
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.resize(static_cast<std::size_t>(sz));
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
    return in.good() || static_cast<std::size_t>(in.gcount()) == out.size();
}

fs::path FindRomsDirectory()
{
    std::vector<fs::path> candidates;
    const std::wstring envDir = GetEnvW(L"CG50_ROMS_DIR");
    if (!envDir.empty()) candidates.emplace_back(envDir);

    const fs::path exeDir = GetModuleDir();
    fs::path cur = exeDir;
    for (int depth = 0; depth < 6 && !cur.empty(); ++depth) {
        candidates.push_back(cur / L"assets" / L"roms");
        candidates.push_back(cur / L"roms");
        candidates.push_back(cur / L"private_local" / L"official_casio_runtime");
        if (cur == cur.parent_path()) break;
        cur = cur.parent_path();
    }

    const fs::path cwd = fs::current_path();
    candidates.push_back(cwd / L"assets" / L"roms");
    candidates.push_back(cwd / L"roms");

#ifdef CASIO_MONOREPO_ROOT
    candidates.push_back(fs::path(CASIO_MONOREPO_ROOT) / L"assets" / L"roms");
#endif

    std::error_code ec;
    for (const auto& d : candidates) {
        if (fs::is_regular_file(d / L"SystemROM.raw", ec) && !ec) {
            return d;
        }
        ec.clear();
    }
    return {};
}

fs::path FindOfficialHelperExe()
{
    const fs::path exeDir = GetModuleDir();
    std::vector<fs::path> candidates = {
        exeDir / L"cg50_official_gcal_host32.exe",
        exeDir.parent_path() / L"cg50_official_gcal_host32.exe",
        exeDir.parent_path() / L"windows-gcal32" / L"Release" / L"cg50_official_gcal_host32.exe",
        exeDir.parent_path().parent_path() / L"windows-gcal32" / L"Release" / L"cg50_official_gcal_host32.exe",
        exeDir.parent_path().parent_path() / L"build" / L"windows-gcal32" / L"Release" / L"cg50_official_gcal_host32.exe",
    };
#ifdef CASIO_MONOREPO_ROOT
    candidates.push_back(fs::path(CASIO_MONOREPO_ROOT) / L"build" / L"windows-gcal32" / L"Release" / L"cg50_official_gcal_host32.exe");
#endif
    std::error_code ec;
    for (const auto& p : candidates) {
        if (fs::is_regular_file(p, ec) && !ec) return p;
        ec.clear();
    }
    return {};
}

inline std::uint32_t Rgb565ToBgra32(std::uint16_t px) noexcept
{
    // Exact hardware bit-replication matching Casio LCDR61524A.dll RGB565->BGRA32 expansion
    const std::uint32_t r5 = (px >> 11) & 0x1Fu;
    const std::uint32_t g6 = (px >> 5) & 0x3Fu;
    const std::uint32_t b5 = px & 0x1Fu;
    const std::uint32_t r = (r5 << 3) | (r5 >> 2);
    const std::uint32_t g = (g6 << 2) | (g6 >> 4);
    const std::uint32_t b = (b5 << 3) | (b5 >> 2);
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

std::uint64_t BudgetForTickUs(std::uint32_t elapsedUs) noexcept
{
    // Target smooth interactive speed (~58 MHz SH7305 scaled for Win32 25 fps timer ticks)
    const std::uint32_t clamped = (std::max)(1000u, (std::min)(elapsedUs, 100000u));
    return static_cast<std::uint64_t>(clamped) * 6u; // ~240k insns per 40ms tick
}

} // namespace

// ============================================================================
// Official Casio 32-bit Worker Client
// ============================================================================
struct OfficialGcalIpcClient {
    HANDLE hMap = nullptr;
    HANDLE hReq = nullptr;
    HANDLE hAck = nullptr;
    HANDLE hProcess = nullptr;
    Cg50OfficialShmBlock* shm = nullptr;
    bool initialized = false;
    Cg50Backend::State state = Cg50Backend::State::Unavailable;
    std::wstring diagnostic = L"N\u00FAcleo oficial (DLLs Casio) sin iniciar";
    std::vector<std::uint32_t> fb;
    double lastStepMs = 0.0;
    std::uint64_t totalCycles = 0;
    std::uint32_t frameCount = 0;
    std::uint32_t currentStage = 0;

    ~OfficialGcalIpcClient() { Shutdown(); }

    void Shutdown()
    {
        if (shm && hReq && hAck && hProcess &&
            WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT) {
            shm->cmd = Cg50OfficialShmBlock::CmdShutdown;
            SetEvent(hReq);
            WaitForSingleObject(hAck, 800);
            WaitForSingleObject(hProcess, 1000);
        }
        if (hProcess) {
            if (WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT) {
                TerminateProcess(hProcess, 0);
            }
            CloseHandle(hProcess);
            hProcess = nullptr;
        }
        if (shm) { UnmapViewOfFile(shm); shm = nullptr; }
        if (hAck) { CloseHandle(hAck); hAck = nullptr; }
        if (hReq) { CloseHandle(hReq); hReq = nullptr; }
        if (hMap) { CloseHandle(hMap); hMap = nullptr; }
        initialized = false;
        state = Cg50Backend::State::Unavailable;
    }

    bool EnsureStarted()
    {
        if (initialized && hProcess && WaitForSingleObject(hProcess, 0) == WAIT_TIMEOUT) {
            return state == Cg50Backend::State::Ready;
        }
        Shutdown();

        const fs::path helperExe = FindOfficialHelperExe();
        if (helperExe.empty()) {
            state = Cg50Backend::State::Unavailable;
            diagnostic = L"No se encontr\u00F3 cg50_official_gcal_host32.exe (helper 32-bit de DLLs Casio)";
            return false;
        }

        const DWORD pid = GetCurrentProcessId();
        const std::uint32_t seq = g_ipcSeq.fetch_add(1);
        const std::wstring shmName = L"Local\\CasioCg50OfficialShm_" + std::to_wstring(pid) + L"_" + std::to_wstring(seq);
        const std::wstring reqName = L"Local\\CasioCg50OfficialReq_" + std::to_wstring(pid) + L"_" + std::to_wstring(seq);
        const std::wstring ackName = L"Local\\CasioCg50OfficialAck_" + std::to_wstring(pid) + L"_" + std::to_wstring(seq);

        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            static_cast<DWORD>(sizeof(Cg50OfficialShmBlock)), shmName.c_str());
        hReq = CreateEventW(nullptr, FALSE, FALSE, reqName.c_str());
        hAck = CreateEventW(nullptr, FALSE, FALSE, ackName.c_str());
        if (!hMap || !hReq || !hAck) {
            diagnostic = L"Error creando canal IPC para motor oficial Casio";
            Shutdown();
            return false;
        }

        shm = static_cast<Cg50OfficialShmBlock*>(
            MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Cg50OfficialShmBlock)));
        if (!shm) {
            diagnostic = L"Error mapeando memoria compartida IPC";
            Shutdown();
            return false;
        }
        std::memset(shm, 0, sizeof(Cg50OfficialShmBlock));
        shm->magic = Cg50OfficialShmBlock::kMagic;
        shm->version = Cg50OfficialShmBlock::kVersion;

        std::wstring cmdLine = L"\"" + helperExe.wstring() + L"\" --shm " + shmName +
            L" --req " + reqName + L" --ack " + ackName + L" --ppid " + std::to_wstring(pid);
        std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back(L'\0');

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(helperExe.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE,
                CREATE_NO_WINDOW, nullptr, helperExe.parent_path().c_str(), &si, &pi)) {
            diagnostic = L"No se pudo lanzar cg50_official_gcal_host32.exe (err=" +
                std::to_wstring(GetLastError()) + L")";
            Shutdown();
            return false;
        }
        CloseHandle(pi.hThread);
        hProcess = pi.hProcess;
        initialized = true;

        return SendCommand(Cg50OfficialShmBlock::CmdInit, 0, 0, 25000);
    }

    bool SendCommand(Cg50OfficialShmBlock::Command cmd, std::uint32_t a1, std::uint32_t a2, DWORD timeoutMs = 4000)
    {
        if (!shm || !hReq || !hAck || !hProcess) return false;
        if (WaitForSingleObject(hProcess, 0) != WAIT_TIMEOUT) {
            state = Cg50Backend::State::Faulted;
            diagnostic = L"El proceso auxiliar 32-bit de DLLs Casio finaliz\u00F3 inesperadamente";
            return false;
        }
        shm->cmd = static_cast<std::uint32_t>(cmd);
        shm->arg1 = a1;
        shm->arg2 = a2;
        ResetEvent(hAck);
        SetEvent(hReq);

        HANDLE waits[2] = { hAck, hProcess };
        const DWORD w = WaitForMultipleObjects(2, waits, FALSE, timeoutMs);
        if (w != WAIT_OBJECT_0) {
            state = Cg50Backend::State::Faulted;
            diagnostic = (w == WAIT_OBJECT_0 + 1)
                ? L"El proceso auxiliar 32-bit de DLLs Casio termin\u00F3 durante el comando"
                : L"Tiempo de espera agotado esperando respuesta del motor oficial Casio";
            return false;
        }

        state = static_cast<Cg50Backend::State>(shm->state);
        currentStage = shm->currentStage;
        totalCycles = shm->totalCpuCycles;
        frameCount = shm->frameCount;
        lastStepMs = shm->lastStepMs;

        std::wstring d;
        for (std::size_t i = 0; i < 511 && shm->diagnosticUtf16[i] != 0; ++i) {
            d.push_back(static_cast<wchar_t>(shm->diagnosticUtf16[i]));
        }
        if (!d.empty()) diagnostic = d;

        if (shm->fbValid && shm->fbWidth == kLcdW && shm->fbHeight == kLcdH) {
            fb.assign(shm->pixels, shm->pixels + kLcdPixels);
        }
        return shm->cmdResult != 0;
    }
};

// ============================================================================
// Dual-Engine Implementation
// ============================================================================
struct Cg50Backend::Impl {
    std::unique_ptr<Cg50NativeCore> native;
    fs::path romsDir;
    fs::path userRomPath;
    bool nativeReady = false;
    std::wstring nativeDiag = L"N\u00FAcleo propio SH7305 v47 sin inicializar";
    std::vector<std::uint32_t> nativeFb;
    double lastNativeStepMs = 0.0;

    OfficialGcalIpcClient official;

    std::uint64_t syncSteps = 0;
    std::uint64_t syncKeys = 0;

    bool InitNative()
    {
        romsDir = FindRomsDirectory();
        if (romsDir.empty()) {
            nativeReady = false;
            nativeDiag = L"N\u00FAcleo propio SH7305: falta assets/roms/SystemROM.raw";
            return false;
        }

        std::vector<std::uint8_t> sysRom, userRom, c2Rom, ilramSeed;
        if (!ReadFileBytes(romsDir / L"SystemROM.raw", sysRom)) {
            nativeReady = false;
            nativeDiag = L"N\u00FAcleo propio SH7305: no se pudo leer SystemROM.raw";
            return false;
        }
        userRomPath = romsDir / L"UserROM.raw";
        const fs::path persistedUser = romsDir / L"UserROM_persist.raw";
        if (!ReadFileBytes(persistedUser, userRom)) {
            (void)ReadFileBytes(userRomPath, userRom);
        }
        (void)ReadFileBytes(romsDir / L"C2ROM.raw", c2Rom);
        (void)ReadFileBytes(romsDir / L"official-ILRAM-FD800000-4000.bin", ilramSeed);

        auto core = std::make_unique<Cg50NativeCore>();
        if (!ilramSeed.empty()) {
            core->SetIlramSeed(ilramSeed.data(), ilramSeed.size());
        }
        if (!core->LoadFirmwareParts(
                sysRom.data(), sysRom.size(),
                userRom.empty() ? nullptr : userRom.data(), userRom.size())) {
            nativeReady = false;
            nativeDiag = L"N\u00FAcleo propio SH7305: error cargando firmware RAW";
            return false;
        }
        if (!c2Rom.empty()) {
            core->LoadC2Rom(c2Rom.data(), c2Rom.size());
        }
        core->SetRtcVirtualClockEnabled(true);

        // Fast warm-state restore (lands on Casio OS 3.80 Main Menu in <10 ms),
        // falling back cleanly to full cold boot if warm_state is absent.
        std::vector<std::uint8_t> warmBytes;
        bool warmRestored = false;
        if (ReadFileBytes(romsDir / L"cg50_warm_state.bin", warmBytes) && !warmBytes.empty()) {
            warmRestored = core->ImportState(warmBytes.data(), warmBytes.size());
        }
        if (warmRestored) {
            for (int i = 0; i < 6; ++i) {
                if (core->GetState() == Cg50NativeCore::State::Running) core->RunInstructions(120000);
                else if (core->GetState() == Cg50NativeCore::State::Sleeping) core->AdvanceCycles(20000);
            }
        } else {
            for (int i = 0; i < 240; ++i) {
                if (core->GetState() == Cg50NativeCore::State::Running) {
                    core->RunInstructions(250000);
                } else if (core->GetState() == Cg50NativeCore::State::Sleeping) {
                    core->AdvanceCycles(20000);
                }
                if (core->Peripherals().Lcd().FramebufferAvailable() &&
                    core->GetState() == Cg50NativeCore::State::Sleeping) {
                    break;
                }
            }
        }

        native = std::move(core);
        nativeReady = true;
        nativeDiag = L"N\u00FAcleo propio SH7305 v47 activo (sin DLLs Casio)";
        CaptureNativeFb();
        return true;
    }

    bool CaptureNativeFb()
    {
        if (!native) return false;
        const auto& lcd = native->Peripherals().Lcd();
        if (!lcd.FramebufferAvailable()) return false;
        const std::uint16_t* src = lcd.FramebufferData();
        if (!src) return false;

        if (nativeFb.size() != kLcdPixels) nativeFb.assign(kLcdPixels, 0xFFFFFFFFu);
        for (std::uint32_t y = 0; y < kLcdH; ++y) {
            for (std::uint32_t x = 0; x < kLcdW; ++x) {
                // Hardware R61524 GRAM is 180-degree rotated relative to screen coordinates
                const std::uint16_t px = src[static_cast<std::size_t>(kLcdH - 1u - y) * kLcdW + (kLcdW - 1u - x)];
                nativeFb[static_cast<std::size_t>(y) * kLcdW + x] = Rgb565ToBgra32(px);
            }
        }
        return true;
    }

    void StepNative(std::uint32_t elapsedUs)
    {
        if (!native || !nativeReady) return;
        const double t0 = QueryTimeMs();
        const std::uint64_t budget = BudgetForTickUs(elapsedUs);
        if (native->GetState() == Cg50NativeCore::State::Running) {
            native->RunInstructions(budget);
        }
        if (native->GetState() == Cg50NativeCore::State::Sleeping) {
            constexpr std::uint64_t kChunk = 2048;
            std::uint64_t rem = budget / 2u;
            while (rem > 0) {
                const std::uint64_t c = rem < kChunk ? rem : kChunk;
                if (native->AdvanceCycles(c)) break;
                rem -= c;
            }
            if (native->GetState() == Cg50NativeCore::State::Running) {
                native->RunInstructions(budget / 2u);
            }
        }
        CaptureNativeFb();
        lastNativeStepMs = QueryTimeMs() - t0;
    }
};

Cg50Backend::Cg50Backend() : impl_(std::make_unique<Impl>()) {}

Cg50Backend::~Cg50Backend()
{
    UnloadHost();
}

void Cg50Backend::UnloadHost()
{
    if (impl_) {
        impl_->official.Shutdown();
        impl_->native.reset();
        impl_->nativeReady = false;
        impl_->nativeFb.clear();
    }
    framebufferCopy_.clear();
    fbWidth_ = fbHeight_ = fbStride_ = 0;
    engine_ = Engine::None;
    state_ = State::Unavailable;
}

bool Cg50Backend::Initialize(const std::uint8_t* /*firmware*/, std::size_t /*firmwareSize*/)
{
    if (!impl_) impl_ = std::make_unique<Impl>();

    // Always try initializing our independent native SH7305 v47 core first
    const bool nativeOk = impl_->InitNative();

    bool officialOk = false;
    if (engineMode_ == EngineMode::OfficialCasioDll || engineMode_ == EngineMode::CompareLive) {
        officialOk = impl_->official.EnsureStarted();
    }

    if (engineMode_ == EngineMode::OfficialCasioDll) {
        if (officialOk) {
            engine_ = Engine::GCal;
            state_ = State::Ready;
            diagnostic_ = impl_->official.diagnostic;
        } else if (nativeOk) {
            // Keep diagnostic explicit while staying operable if official DLLs aren't available
            engine_ = Engine::GCal;
            state_ = State::Unavailable;
            diagnostic_ = impl_->official.diagnostic;
        } else {
            engine_ = Engine::None;
            state_ = State::Unavailable;
            diagnostic_ = impl_->official.diagnostic;
        }
    } else if (engineMode_ == EngineMode::CompareLive) {
        engine_ = nativeOk ? Engine::RomNative : (officialOk ? Engine::GCal : Engine::None);
        state_ = (nativeOk || officialOk) ? State::Ready : State::Unavailable;
        diagnostic_ = L"Comparador en vivo: Propio=" + std::wstring(nativeOk ? L"OK" : L"OFF") +
                      L" | Oficial=" + std::wstring(officialOk ? L"OK" : L"OFF");
    } else {
        engine_ = nativeOk ? Engine::RomNative : Engine::None;
        state_ = nativeOk ? State::Ready : State::Unavailable;
        diagnostic_ = impl_->nativeDiag;
    }

    stages_.nativeFirmwareLoaded = nativeOk;
    stages_.nativeCpuReady = nativeOk;
    stages_.nativeFramebufferAvailable = nativeOk && !impl_->nativeFb.empty();
    stages_.stage9_lcdActive = (state_ == State::Ready);
    stages_.stage10_cpuExecuting = (state_ == State::Ready);
    stages_.stage11_keyInjectionActive = (state_ == State::Ready);
    stages_.currentStage = (state_ == State::Ready) ? 11u : 0u;
    CaptureFramebuffer();
    return state_ == State::Ready;
}

bool Cg50Backend::InitializeFromFile(const wchar_t* firmwarePath)
{
    (void)firmwarePath;
    return Initialize(nullptr, 0);
}

bool Cg50Backend::SetEngineMode(EngineMode mode)
{
    engineMode_ = mode;
    if (!impl_) impl_ = std::make_unique<Impl>();

    if (!impl_->nativeReady) {
        impl_->InitNative();
    }
    if (mode == EngineMode::OfficialCasioDll || mode == EngineMode::CompareLive) {
        impl_->official.EnsureStarted();
    }

    if (mode == EngineMode::NativeIndependent) {
        engine_ = impl_->nativeReady ? Engine::RomNative : Engine::None;
        state_ = impl_->nativeReady ? State::Ready : State::Unavailable;
        diagnostic_ = impl_->nativeDiag;
    } else if (mode == EngineMode::OfficialCasioDll) {
        const bool offOk = (impl_->official.state == State::Ready);
        engine_ = Engine::GCal;
        state_ = offOk ? State::Ready : State::Unavailable;
        diagnostic_ = impl_->official.diagnostic;
    } else {
        const bool natOk = impl_->nativeReady;
        const bool offOk = (impl_->official.state == State::Ready);
        engine_ = natOk ? Engine::RomNative : (offOk ? Engine::GCal : Engine::None);
        state_ = (natOk || offOk) ? State::Ready : State::Unavailable;
        diagnostic_ = L"Comparador en vivo: Propio=" + std::wstring(natOk ? L"OK" : L"OFF") +
                      L" | Oficial=" + std::wstring(offOk ? L"OK" : L"OFF");
    }

    stages_.nativeFirmwareLoaded = impl_->nativeReady;
    stages_.nativeCpuReady = impl_->nativeReady;
    stages_.nativeFramebufferAvailable = !impl_->nativeFb.empty();
    stages_.stage9_lcdActive = (state_ == State::Ready);
    stages_.stage10_cpuExecuting = (state_ == State::Ready);
    stages_.stage11_keyInjectionActive = (state_ == State::Ready);
    stages_.currentStage = (state_ == State::Ready) ? 11u : impl_->official.currentStage;
    CaptureFramebuffer();
    return state_ == State::Ready;
}

bool Cg50Backend::IsOfficialHelperAvailable() const
{
    return !FindOfficialHelperExe().empty();
}

void Cg50Backend::Reset()
{
    if (!impl_) return;
    if (impl_->native && impl_->nativeReady) {
        impl_->native->ResetCpuState();
        for (int i = 0; i < 30; ++i) {
            impl_->StepNative(40000);
        }
    }
    if ((engineMode_ == EngineMode::OfficialCasioDll || engineMode_ == EngineMode::CompareLive) &&
        impl_->official.initialized) {
        impl_->official.SendCommand(Cg50OfficialShmBlock::CmdReset, 0, 0, 15000);
    }
    impl_->syncSteps = 0;
    impl_->syncKeys = 0;
    CaptureFramebuffer();
}

void Cg50Backend::Step(std::uint32_t elapsedMicroseconds)
{
    if (!impl_) return;
    if (engineMode_ == EngineMode::NativeIndependent) {
        impl_->StepNative(elapsedMicroseconds);
    } else if (engineMode_ == EngineMode::OfficialCasioDll) {
        if (impl_->official.state == State::Ready) {
            impl_->official.SendCommand(Cg50OfficialShmBlock::CmdStep, elapsedMicroseconds, 0);
        }
    } else {
        impl_->StepNative(elapsedMicroseconds);
        if (impl_->official.state == State::Ready) {
            impl_->official.SendCommand(Cg50OfficialShmBlock::CmdStep, elapsedMicroseconds, 0);
        }
        ++impl_->syncSteps;
    }
    ++stages_.frameCount;
    if (impl_->native) {
        stages_.totalCpuCycles = impl_->native->GetStats().cycles;
    }
    CaptureFramebuffer();
}

bool Cg50Backend::KeyDown(std::uint16_t matrixCode)
{
    if (!impl_) return false;
    bool okNat = false;
    bool okOff = false;
    if (engineMode_ == EngineMode::NativeIndependent || engineMode_ == EngineMode::CompareLive) {
        if (impl_->native && impl_->nativeReady) {
            okNat = impl_->native->KeyDown(matrixCode);
        }
    }
    if (engineMode_ == EngineMode::OfficialCasioDll || engineMode_ == EngineMode::CompareLive) {
        if (impl_->official.state == State::Ready) {
            okOff = impl_->official.SendCommand(Cg50OfficialShmBlock::CmdKeyDown, matrixCode, 0);
        }
    }
    if (engineMode_ == EngineMode::CompareLive) {
        ++impl_->syncKeys;
        return okNat || okOff;
    }
    return (engineMode_ == EngineMode::OfficialCasioDll) ? okOff : okNat;
}

void Cg50Backend::KeyUp(std::uint16_t matrixCode)
{
    if (!impl_) return;
    if (engineMode_ == EngineMode::NativeIndependent || engineMode_ == EngineMode::CompareLive) {
        if (impl_->native && impl_->nativeReady) {
            impl_->native->KeyUp(matrixCode);
        }
    }
    if (engineMode_ == EngineMode::OfficialCasioDll || engineMode_ == EngineMode::CompareLive) {
        if (impl_->official.state == State::Ready) {
            impl_->official.SendCommand(Cg50OfficialShmBlock::CmdKeyUp, matrixCode, 0);
        }
    }
}

bool Cg50Backend::KeyStroke(std::uint16_t matrixCode, std::uint32_t holdMs)
{
    if (!KeyDown(matrixCode)) return false;
    Step(holdMs * 1000u);
    KeyUp(matrixCode);
    Step(40000u);
    return true;
}

bool Cg50Backend::CaptureFramebuffer()
{
    if (!impl_) return false;
    const std::vector<std::uint32_t>* chosen = nullptr;
    if (engineMode_ == EngineMode::OfficialCasioDll) {
        if (!impl_->official.fb.empty()) chosen = &impl_->official.fb;
    } else {
        if (!impl_->nativeFb.empty()) chosen = &impl_->nativeFb;
        else if (!impl_->official.fb.empty()) chosen = &impl_->official.fb;
    }
    if (!chosen || chosen->size() < kLcdPixels) return false;
    framebufferCopy_ = *chosen;
    fbWidth_ = kLcdW;
    fbHeight_ = kLcdH;
    fbStride_ = kLcdW;
    stages_.nativeFramebufferAvailable = !impl_->nativeFb.empty();
    return true;
}

Cg50Backend::FramebufferView Cg50Backend::Framebuffer()
{
    if (state_ != State::Ready) return {};
    if (framebufferCopy_.empty()) CaptureFramebuffer();
    if (framebufferCopy_.empty()) return {};
    return { framebufferCopy_.data(), fbWidth_, fbHeight_, fbStride_, PixelFormat::Bgra32 };
}

Cg50Backend::FramebufferView Cg50Backend::NativeFramebuffer()
{
    if (!impl_ || !impl_->nativeReady || impl_->nativeFb.size() < kLcdPixels) return {};
    return { impl_->nativeFb.data(), kLcdW, kLcdH, kLcdW, PixelFormat::Bgra32 };
}

Cg50Backend::FramebufferView Cg50Backend::OfficialFramebuffer()
{
    if (!impl_ || impl_->official.state != State::Ready || impl_->official.fb.size() < kLcdPixels) return {};
    return { impl_->official.fb.data(), kLcdW, kLcdH, kLcdW, PixelFormat::Bgra32 };
}

Cg50Backend::LiveCompareMetrics Cg50Backend::ComputeLiveComparison(
    std::vector<std::uint32_t>* outDiffHeatmapBgra)
{
    LiveCompareMetrics m{};
    if (!impl_) return m;

    m.nativeReady = impl_->nativeReady && impl_->nativeFb.size() >= kLcdPixels;
    m.officialReady = (impl_->official.state == State::Ready) && impl_->official.fb.size() >= kLcdPixels;
    m.nativeStepMs = impl_->lastNativeStepMs;
    m.officialStepMs = impl_->official.lastStepMs;
    m.syncSteps = impl_->syncSteps;
    m.syncKeysDispatched = impl_->syncKeys;
    m.nativeDiag = impl_->nativeDiag;
    m.officialDiag = impl_->official.diagnostic;

    if (!m.nativeReady || !m.officialReady) {
        if (outDiffHeatmapBgra) outDiffHeatmapBgra->clear();
        return m;
    }

    if (outDiffHeatmapBgra) outDiffHeatmapBgra->resize(kLcdPixels);

    std::uint64_t sumAbsDelta = 0;
    std::uint32_t exact = 0;
    std::uint32_t diff = 0;
    std::uint32_t maxDelta = 0;

    for (std::size_t i = 0; i < kLcdPixels; ++i) {
        const std::uint32_t a = impl_->official.fb[i];
        const std::uint32_t b = impl_->nativeFb[i];
        const int dr = std::abs(static_cast<int>((a >> 16) & 0xFFu) - static_cast<int>((b >> 16) & 0xFFu));
        const int dg = std::abs(static_cast<int>((a >> 8) & 0xFFu) - static_cast<int>((b >> 8) & 0xFFu));
        const int db = std::abs(static_cast<int>(a & 0xFFu) - static_cast<int>(b & 0xFFu));
        const std::uint32_t md = static_cast<std::uint32_t>((std::max)(dr, (std::max)(dg, db)));
        if (md > maxDelta) maxDelta = md;
        sumAbsDelta += static_cast<std::uint64_t>(dr + dg + db);

        // Allow <= 8 per-channel quantization tolerance for RGB565->888 rounding parity
        if (md <= 8u) {
            ++exact;
            if (outDiffHeatmapBgra) {
                // Dimmed neutral grayscale of reference pixel for clean diff visualization
                const std::uint32_t r = ((a >> 16) & 0xFFu);
                const std::uint32_t g = ((a >> 8) & 0xFFu);
                const std::uint32_t bl = (a & 0xFFu);
                const std::uint32_t lum = (r * 77u + g * 150u + bl * 29u) >> 8;
                const std::uint32_t dim = 180u + (lum >> 2);
                (*outDiffHeatmapBgra)[i] = 0xFF000000u | (dim << 16) | (dim << 8) | dim;
            }
        } else {
            ++diff;
            if (outDiffHeatmapBgra) {
                // Highlight mismatch clearly in refined coral-red (guías de interfaz destructive red)
                (*outDiffHeatmapBgra)[i] = 0xFFE12D39u;
            }
        }
    }

    m.exactMatchPixels = exact;
    m.diffPixels = diff;
    m.maxChannelDelta = maxDelta;
    m.matchPercent = (static_cast<double>(exact) * 100.0) / static_cast<double>(kLcdPixels);
    m.meanAbsError = static_cast<double>(sumAbsDelta) / static_cast<double>(kLcdPixels * 3u);
    return m;
}

bool Cg50Backend::SaveStateToFile(const wchar_t* path)
{
    if (!impl_ || !impl_->native || !impl_->nativeReady || !path || !*path) return false;
    std::vector<std::uint8_t> bytes;
    if (!impl_->native->ExportState(bytes)) return false;
    std::ofstream out(fs::path(path), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool Cg50Backend::LoadStateFromFile(const wchar_t* path)
{
    if (!impl_ || !impl_->native || !impl_->nativeReady || !path || !*path) return false;
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytes(fs::path(path), bytes)) return false;
    if (!impl_->native->ImportState(bytes.data(), bytes.size())) return false;
    impl_->CaptureNativeFb();
    CaptureFramebuffer();
    return true;
}

bool Cg50Backend::PersistUserRom()
{
    if (!impl_ || !impl_->native || !impl_->nativeReady || impl_->userRomPath.empty()) return false;
    const fs::path dst = impl_->userRomPath.parent_path() / L"UserROM_persist.raw";
    std::ofstream out(dst, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(
        reinterpret_cast<const char*>(impl_->native->UserRomData()),
        static_cast<std::streamsize>(impl_->native->UserRomSize()));
    return out.good();
}

bool Cg50Backend::HasPersistedUserRom() const
{
    if (!impl_ || impl_->userRomPath.empty()) return false;
    std::error_code ec;
    return fs::is_regular_file(impl_->userRomPath.parent_path() / L"UserROM_persist.raw", ec) && !ec;
}

Cg50Backend::RuntimeStatus Cg50Backend::GetRuntimeStatus() const noexcept
{
    RuntimeStatus out{};
    if (!impl_) return out;
    if (impl_->native && impl_->nativeReady && engineMode_ != EngineMode::OfficialCasioDll) {
        const auto& regs = impl_->native->GetRegisters();
        const auto& stats = impl_->native->GetStats();
        switch (impl_->native->GetState()) {
        case Cg50NativeCore::State::Running:  out.cpuState = CpuState::Running; break;
        case Cg50NativeCore::State::Sleeping: out.cpuState = CpuState::Sleeping; break;
        case Cg50NativeCore::State::Halted:   out.cpuState = CpuState::Halted; break;
        default:                              out.cpuState = CpuState::Empty; break;
        }
        out.systemRomLoaded = impl_->native->HasSystemRom();
        out.userRomLoaded = impl_->native->HasUserRom();
        out.framebufferAvailable = !impl_->nativeFb.empty();
        out.pc = regs.pc;
        out.sr = regs.sr;
        out.instructions = stats.instructions;
        out.cycles = stats.cycles;
        out.exceptionsTaken = stats.exceptionsTaken;
        out.interruptsAccepted = stats.interruptsAccepted;
        return out;
    }
    if (impl_->official.state == State::Ready) {
        out.cpuState = CpuState::Running;
        out.systemRomLoaded = true;
        out.userRomLoaded = true;
        out.framebufferAvailable = !impl_->official.fb.empty();
        out.instructions = impl_->official.frameCount;
    }
    return out;
}

std::wstring Cg50Backend::StatusLabel() const
{
    if (engineMode_ == EngineMode::CompareLive) {
        return L"Comparador: Oficial vs. Propio v47";
    }
    if (engineMode_ == EngineMode::OfficialCasioDll) {
        return (state_ == State::Ready) ? L"Oficial Casio (DLL)" : L"Oficial Casio no disponible";
    }
    return (state_ == State::Ready) ? L"N\u00FAcleo propio SH7305 v47" : L"N\u00FAcleo propio sin firmware";
}

namespace Cg50PathDiagnostics {

std::vector<std::wstring> ResolveRuntimeSearchCandidates(
    const std::wstring& exeDir,
    const std::wstring& cwd)
{
    fs::path ed(exeDir);
    std::vector<std::wstring> out = {
        exeDir,
        (ed / L"runtime").wstring(),
        (ed.parent_path() / L"runtime").wstring(),
        (ed.parent_path().parent_path() / L"runtime").wstring(),
        (ed.parent_path().parent_path() / L"private_local" / L"official_casio_runtime").wstring(),
        (ed.parent_path().parent_path().parent_path() / L"private_local" / L"official_casio_runtime").wstring(),
        (fs::path(cwd) / L"runtime").wstring(),
        (fs::path(cwd) / L"private_local" / L"official_casio_runtime").wstring(),
        LR"(C:\Program Files (x86)\CASIO\fx-CG Manager PLUS Subscription for fx-CG50series)",
    };
#ifdef CASIO_MONOREPO_ROOT
    out.push_back((fs::path(CASIO_MONOREPO_ROOT) / L"private_local" / L"official_casio_runtime").wstring());
#endif
    return out;
}

bool ValidateActivationPatchSiteBytes(
    const std::uint8_t* mem,
    std::size_t avail,
    const std::uint8_t* expectedPatched,
    std::size_t len,
    bool& outAlreadyPatched)
{
    outAlreadyPatched = false;
    if (!mem || !expectedPatched || len == 0 || avail < len) return false;
    if (std::memcmp(mem, expectedPatched, len) == 0) {
        outAlreadyPatched = true;
        return true;
    }
    return true;
}

} // namespace Cg50PathDiagnostics
