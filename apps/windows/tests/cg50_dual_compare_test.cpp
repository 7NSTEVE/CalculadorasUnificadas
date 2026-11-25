// ============================================================================
// cg50_dual_compare_test.cpp
// Verifies all 3 runtime CG50 modes on 64-bit Windows:
//   1. NativeIndependent (core/casio_cg50 v47, zero Casio DLLs)
//   2. OfficialCasioDll  (isolated 32-bit cg50_official_gcal_host32.exe over IPC)
//   3. CompareLive       (lockstep dual stepping + live pixel diff metrics + heatmap)
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <vector>
#include "cg50_backend.h"

int main()
{
    Cg50Backend backend;

    // 1. Native Independent Mode (v47 SH7305 FULL_SILICON, zero Casio DLLs)
    backend.SetEngineMode(Cg50Backend::EngineMode::NativeIndependent);
    if (!backend.Initialize(nullptr, 0)) {
        std::printf("FAIL: NativeIndependent Initialize() failed: %ls\n", backend.Diagnostic().c_str());
        return 1;
    }
    if (backend.GetEngine() != Cg50Backend::Engine::RomNative) {
        std::printf("FAIL: expected Engine::RomNative\n");
        return 2;
    }
    for (int i = 0; i < 6; ++i) backend.Step(40000);
    auto natFb = backend.NativeFramebuffer();
    if (!natFb.pixels || natFb.width != 384 || natFb.height != 216) {
        std::printf("FAIL: Native framebuffer invalid (%zux%zu)\n", natFb.width, natFb.height);
        return 3;
    }
    std::printf("[PASS] Mode 1 NativeIndependent: 384x216 ready (%ls)\n", backend.StatusLabel().c_str());

    // 2. Official Casio DLL Mode (via 32-bit isolated IPC helper)
    if (!backend.IsOfficialHelperAvailable()) {
        std::printf("[FAIL] cg50_official_gcal_host32.exe not found next to test binary or build/windows-gcal32/Release\n");
        return 4;
    }
    const bool offOk = backend.SetEngineMode(Cg50Backend::EngineMode::OfficialCasioDll);
    if (!offOk) {
        std::printf("[FAIL] Mode 2 OfficialCasioDll failed to initialize: %ls\n", backend.Diagnostic().c_str());
        return 5;
    }
    for (int i = 0; i < 6; ++i) backend.Step(40000);
    auto offFb = backend.OfficialFramebuffer();
    if (!offFb.pixels || offFb.width != 384 || offFb.height != 216) {
        std::printf("[FAIL] Mode 2 OfficialCasioDll framebuffer invalid (%zux%zu)\n", offFb.width, offFb.height);
        return 6;
    }
    std::printf("[PASS] Mode 2 OfficialCasioDll (32-bit isolated IPC): 384x216 ready (%ls)\n", backend.StatusLabel().c_str());

    // 3. CompareLive Mode (both engines running simultaneously in lockstep)
    const bool cmpOk = backend.SetEngineMode(Cg50Backend::EngineMode::CompareLive);
    if (!cmpOk) {
        std::printf("[FAIL] Mode 3 CompareLive failed: %ls\n", backend.Diagnostic().c_str());
        return 7;
    }
    // Give official Casio DLSim32E background CPU enough real wall time to finish
    // cold boot onto Casio OS 3.80 Main Menu, then dispatch MENU keystroke (0x0409)
    // in lockstep so BOTH Official Casio DLL and Native SH7305 v47 show Main Menu.
    for (int i = 0; i < 60; ++i) {
        backend.Step(40000);
        Sleep(50);
        auto curOff = backend.OfficialFramebuffer();
        if (curOff.pixels && curOff.width == 384 && curOff.height == 216) {
            int nonUniform = 0;
            const std::uint32_t c0 = curOff.pixels[108 * 384 + 192] & 0xFFFFFFu;
            for (std::size_t k = 0; k < 384u * 216u; k += 17) {
                const std::uint32_t p = curOff.pixels[k] & 0xFFFFFFu;
                if (p != 0xFFFFFFu && p != 0xBEBEBEu && p != 0x000000u) ++nonUniform;
            }
            if (nonUniform > 400 && c0 != 0xFFFFFFu && c0 != 0xBEBEBEu) break;
        }
    }
    backend.KeyStroke(0x0409, 60);
    for (int i = 0; i < 15; ++i) {
        backend.Step(40000);
        Sleep(40);
    }

    std::vector<std::uint32_t> diffHeatmap;
    const auto metrics = backend.ComputeLiveComparison(&diffHeatmap);
    offFb = backend.OfficialFramebuffer();
    natFb = backend.NativeFramebuffer();
    if (!metrics.officialReady || !metrics.nativeReady) {
        std::printf("[FAIL] Mode 3 CompareLive: officialReady=%d nativeReady=%d\n",
            metrics.officialReady ? 1 : 0, metrics.nativeReady ? 1 : 0);
        return 8;
    }
    if (diffHeatmap.size() != 384u * 216u || metrics.syncKeysDispatched < 1) {
        std::printf("[FAIL] Mode 3 CompareLive: invalid heatmap (%zu) or syncKeys (%llu)\n",
            diffHeatmap.size(), static_cast<unsigned long long>(metrics.syncKeysDispatched));
        return 9;
    }
    int natNonWhite = 0, offNonGray = 0;
    for (std::size_t i = 0; i < 384u * 216u; ++i) {
        if (natFb.pixels[i] != 0xFFFFFFFFu) ++natNonWhite;
        if ((offFb.pixels[i] & 0xFFFFFFu) != 0xBEBEBEu) ++offNonGray;
    }
    if (natNonWhite < 1000) {
        std::printf("[FAIL] Native v47 LCD appears blank (nonWhite=%d)\n", natNonWhite);
        return 11;
    }
    if (metrics.matchPercent < 99.0) {
        std::printf("[FAIL] Mode 3 CompareLive match too low: %.2f%% (expected >= 99.0%%)\n", metrics.matchPercent);
        return 12;
    }
    std::printf("[PASS] Mode 3 CompareLive: match=%.2f%% diff=%u/%u maxDelta=%u off=%.2fms nat=%.2fms keys=%llu (natInk=%d offInk=%d offMid=%08X natMid=%08X)\n",
        metrics.matchPercent, metrics.diffPixels, metrics.totalPixels,
        metrics.maxChannelDelta, metrics.officialStepMs, metrics.nativeStepMs,
        static_cast<unsigned long long>(metrics.syncKeysDispatched),
        natNonWhite, offNonGray,
        offFb.pixels[108 * 384 + 192], natFb.pixels[108 * 384 + 192]);

    // 4. Clean return to NativeIndependent mode
    backend.SetEngineMode(Cg50Backend::EngineMode::NativeIndependent);
    if (backend.GetEngineMode() != Cg50Backend::EngineMode::NativeIndependent) {
        std::printf("[FAIL] Failed to switch back to NativeIndependent\n");
        return 10;
    }
    std::printf("ALL CG50 DUAL-ENGINE & LIVE COMPARATOR TESTS PASSED.\n");
    return 0;
}
