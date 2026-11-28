#pragma once
// ============================================================================
// studio/app/studio_matricula_tec_dialog.hpp
// Native Win32 guías de interfaz + material translúcido "Matricula TEC Autonoma" Workspace
// (C++23 + C23 ABI, PerMonitorV2 4K, Porcelain Light / Graphite Dark /
// High Contrast synchronized, Dry-Run safe default, DPAPI shared with MCP).
// ============================================================================

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace UiGlass { struct GlassSettings; }

namespace DocStudio::MatriculaTecUi {

// Opens the modal guías de interfaz "Matricula TEC Autonoma" dialog.
// Supports optional stealth PNG capture for automated visual QA.
bool RunMatriculaTecDialog(
    HWND owner,
    std::string& outStatusBannerUtf8,
    const UiGlass::GlassSettings* glassSettings = nullptr,
    const wchar_t* autoScreenshotPathW = nullptr,
    bool backgroundStealth = false);

} // namespace DocStudio::MatriculaTecUi

#endif // _WIN32
