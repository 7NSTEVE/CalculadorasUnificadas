// ============================================================================
// cg50_official_gcal_host32.cpp
// Isolated 32-bit (Win32 x86) worker process that loads Casio's official
// 32-bit GCalAppIF.dll / DLSim32E.dll runtime and exposes the 384x216 BGRA32
// framebuffer + key injection over named shared memory to 64-bit
// CalculadoraUnificada.exe (for Official mode and Live Official-vs-Native Compare).
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
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <string>
#include <vector>

#include "cg50_backend.h"
#include "cg50_official_ipc.h"

namespace {

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

void CopyDiagnostic(Cg50OfficialShmBlock* shm, const std::wstring& diag)
{
    if (!shm) return;
    std::memset(shm->diagnosticUtf16, 0, sizeof(shm->diagnosticUtf16));
    const std::size_t n = (std::min)(diag.size(), static_cast<std::size_t>(511));
    for (std::size_t i = 0; i < n; ++i) {
        shm->diagnosticUtf16[i] = static_cast<std::uint16_t>(diag[i]);
    }
}

void PublishSnapshot(Cg50OfficialShmBlock* shm, Cg50Backend& backend, double stepMs)
{
    if (!shm) return;
    shm->state = static_cast<std::int32_t>(backend.GetState());
    const auto st = backend.GetStages();
    shm->currentStage = st.currentStage;
    shm->totalCpuCycles = st.totalCpuCycles;
    shm->frameCount = st.frameCount;
    shm->lastStepMs = stepMs;
    CopyDiagnostic(shm, backend.Diagnostic());

    auto fb = backend.Framebuffer();
    if (fb.pixels && fb.width >= Cg50OfficialShmBlock::kMaxW && fb.height >= Cg50OfficialShmBlock::kMaxH) {
        // Casio DLSim32E/LCDR61524A exposes 396x224; the active 384x216 Casio OS
        // viewport sits at exact hardware offset (xOff=6, yOff=1), verified 100.00%
        // pixel-identical (0/82,944 diff) against independent core/casio_cg50 v47.
        const std::size_t dstW = Cg50OfficialShmBlock::kMaxW;
        const std::size_t dstH = Cg50OfficialShmBlock::kMaxH;
        const std::size_t xOff = (fb.width >= dstW + 6u) ? 6u : (fb.width - dstW) / 2u;
        const std::size_t yOff = (fb.height > dstH) ? 1u : 0u;
        shm->fbWidth = static_cast<std::uint32_t>(dstW);
        shm->fbHeight = static_cast<std::uint32_t>(dstH);
        shm->fbStride = static_cast<std::uint32_t>(dstW);
        for (std::size_t y = 0; y < dstH; ++y) {
            const std::uint32_t* srcRow = fb.pixels + (y + yOff) * fb.stridePixels + xOff;
            std::uint32_t* dstRow = shm->pixels + y * dstW;
            for (std::size_t x = 0; x < dstW; ++x) {
                dstRow[x] = srcRow[x] | 0xFF000000u;
            }
        }
        shm->fbValid = 1u;
    } else if (fb.pixels && fb.width > 0 && fb.height > 0 &&
               fb.width <= Cg50OfficialShmBlock::kMaxW &&
               fb.height <= Cg50OfficialShmBlock::kMaxH) {
        shm->fbWidth = static_cast<std::uint32_t>(fb.width);
        shm->fbHeight = static_cast<std::uint32_t>(fb.height);
        shm->fbStride = static_cast<std::uint32_t>(fb.width);
        for (std::size_t y = 0; y < fb.height; ++y) {
            const std::uint32_t* srcRow = fb.pixels + y * fb.stridePixels;
            std::uint32_t* dstRow = shm->pixels + y * fb.width;
            for (std::size_t x = 0; x < fb.width; ++x) {
                dstRow[x] = srcRow[x] | 0xFF000000u;
            }
        }
        shm->fbValid = 1u;
    } else {
        shm->fbValid = 0u;
    }
}

bool ParseArg(int argc, wchar_t** argv, const wchar_t* flag, std::wstring& out)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::wcscmp(argv[i], flag) == 0) {
            out = argv[i + 1];
            return true;
        }
    }
    return false;
}

bool HasFlag(int argc, wchar_t** argv, const wchar_t* flag)
{
    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], flag) == 0) return true;
    }
    return false;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (HasFlag(argc, argv, L"--self-test")) {
        Cg50Backend backend;
        const bool ok = backend.Initialize(nullptr, 0);
        if (ok) {
            for (int i = 0; i < 8; ++i) backend.Step(40000);
        }
        const auto fb = backend.Framebuffer();
        std::wprintf(L"GCAL32_SELFTEST ok=%d state=%d stage=%u fb=%zux%zu diag=%ls\n",
            ok ? 1 : 0,
            static_cast<int>(backend.GetState()),
            backend.GetStages().currentStage,
            fb.width,
            fb.height,
            backend.Diagnostic().c_str());
        return ok ? 0 : 2;
    }

    std::wstring shmName, reqName, ackName, ppidStr;
    if (!ParseArg(argc, argv, L"--shm", shmName) ||
        !ParseArg(argc, argv, L"--req", reqName) ||
        !ParseArg(argc, argv, L"--ack", ackName)) {
        return 10;
    }
    ParseArg(argc, argv, L"--ppid", ppidStr);

    HANDLE hMap = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, shmName.c_str());
    HANDLE hReq = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, reqName.c_str());
    HANDLE hAck = OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, ackName.c_str());
    if (!hMap || !hReq || !hAck) {
        if (hAck) CloseHandle(hAck);
        if (hReq) CloseHandle(hReq);
        if (hMap) CloseHandle(hMap);
        return 11;
    }

    auto* shm = static_cast<Cg50OfficialShmBlock*>(
        MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Cg50OfficialShmBlock)));
    if (!shm || shm->magic != Cg50OfficialShmBlock::kMagic ||
        shm->version != Cg50OfficialShmBlock::kVersion) {
        if (shm) UnmapViewOfFile(shm);
        CloseHandle(hAck);
        CloseHandle(hReq);
        CloseHandle(hMap);
        return 12;
    }

    HANDLE hParent = nullptr;
    if (!ppidStr.empty()) {
        const DWORD ppid = static_cast<DWORD>(std::wcstoul(ppidStr.c_str(), nullptr, 10));
        if (ppid != 0) {
            hParent = OpenProcess(SYNCHRONIZE, FALSE, ppid);
        }
    }

    Cg50Backend backend;
    bool running = true;

    while (running) {
        HANDLE waits[2] = { hReq, hParent };
        const DWORD waitCount = hParent ? 2u : 1u;
        const DWORD w = MsgWaitForMultipleObjects(waitCount, waits, FALSE, 250, QS_ALLINPUT);

        if (w == WAIT_OBJECT_0 + waitCount) {
            MSG msg{};
            while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
            continue;
        }
        if (hParent && w == WAIT_OBJECT_0 + 1) {
            // Parent exited unexpectedly — shut down cleanly so no orphan remains.
            break;
        }
        if (w != WAIT_OBJECT_0) {
            continue;
        }

        const double t0 = QueryTimeMs();
        int32_t res = 1;
        switch (shm->cmd) {
        case Cg50OfficialShmBlock::CmdInit:
            res = backend.Initialize(nullptr, 0) ? 1 : 0;
            PublishSnapshot(shm, backend, QueryTimeMs() - t0);
            break;
        case Cg50OfficialShmBlock::CmdStep:
            backend.Step(shm->arg1);
            PublishSnapshot(shm, backend, QueryTimeMs() - t0);
            break;
        case Cg50OfficialShmBlock::CmdKeyDown:
            res = backend.KeyDown(static_cast<std::uint16_t>(shm->arg1 & 0xFFFFu)) ? 1 : 0;
            PublishSnapshot(shm, backend, QueryTimeMs() - t0);
            break;
        case Cg50OfficialShmBlock::CmdKeyUp:
            backend.KeyUp(static_cast<std::uint16_t>(shm->arg1 & 0xFFFFu));
            PublishSnapshot(shm, backend, QueryTimeMs() - t0);
            break;
        case Cg50OfficialShmBlock::CmdKeyStroke:
            res = backend.KeyStroke(
                static_cast<std::uint16_t>(shm->arg1 & 0xFFFFu),
                shm->arg2 ? shm->arg2 : 60u) ? 1 : 0;
            PublishSnapshot(shm, backend, QueryTimeMs() - t0);
            break;
        case Cg50OfficialShmBlock::CmdReset:
            backend.Reset();
            PublishSnapshot(shm, backend, QueryTimeMs() - t0);
            break;
        case Cg50OfficialShmBlock::CmdShutdown:
            running = false;
            res = 1;
            break;
        default:
            res = 0;
            break;
        }
        shm->cmdResult = res;
        SetEvent(hAck);
    }

    if (hParent) CloseHandle(hParent);
    UnmapViewOfFile(shm);
    CloseHandle(hAck);
    CloseHandle(hReq);
    CloseHandle(hMap);
    return 0;
}
