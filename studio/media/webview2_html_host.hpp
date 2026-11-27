#pragma once
// ============================================================================
// doc_studio/media/webview2_html_host.hpp
// Lazy, Shared-Environment Microsoft Edge WebView2 Host for Animated &
// Interactive HTML/CSS/JS Canvas Blocks (ObjectKind::HtmlEmbed).
// - ZERO startup cost (never initializes WebView2 unless user activates HTML).
// - Automatic ICoreWebView2_3::TrySuspend() + PNG CapturePreview when frozen,
//   scrolled off-screen, or window minimized -> ~0% idle CPU/GPU.
// - Strict offline sandbox by default for local HTML/CSS/JS widgets.
// ============================================================================

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace DocStudio {

class WebView2HtmlHost {
public:
    WebView2HtmlHost();
    ~WebView2HtmlHost();

    WebView2HtmlHost(const WebView2HtmlHost&) = delete;
    WebView2HtmlHost& operator=(const WebView2HtmlHost&) = delete;

    // Non-invasive check (does NOT launch browser process)
    static bool IsWebView2RuntimeInstalled(std::string* outVersionUtf8 = nullptr);

    // Enable per-PID WebView2Cache_<pid> directory isolation for background GUI instances
    static void SetUsePidIsolatedCache(bool isolated) noexcept;
    static bool UsePidIsolatedCache() noexcept;

    bool IsEnvironmentInitialized() const noexcept;
    bool IsBlockActive(std::uint64_t objectId) const noexcept;
    bool IsBlockSuspended(std::uint64_t objectId) const noexcept;
    bool IsNavigationCompleted(std::uint64_t objectId) const noexcept;
    std::size_t ActiveBlockCount() const noexcept;

#ifdef _WIN32
    // Lazily creates shared WebView2 environment and hosts live HTML inside parentHwnd at bounds.
    bool ActivateBlock(
        HWND parentHwnd,
        std::uint64_t objectId,
        const RECT& boundsPx,
        const std::string& htmlSourceUtf8,
        const std::string& urlUtf8,
        bool allowNetwork,
        std::function<void(std::uint64_t, bool)> onReady = nullptr);

    // Synchronize screen position/clipping with camera pan/zoom; hides & suspends if offscreen.
    void SyncBlockBounds(std::uint64_t objectId, const RECT& boundsPx, bool visibleInViewport);

    // Captures crisp PNG preview (for zero-CPU frozen rendering & PDF export) and calls TrySuspend().
    void CaptureSnapshotAndSuspend(
        std::uint64_t objectId,
        std::function<void(std::uint64_t, std::vector<std::uint8_t>)> onSnapshotPng);

    // Captures crisp PNG preview WITHOUT suspending the active WebView2 control.
    void CaptureSnapshot(
        std::uint64_t objectId,
        std::function<void(std::uint64_t, std::vector<std::uint8_t>)> onSnapshotPng);

    void ResumeBlock(std::uint64_t objectId, const RECT& boundsPx);
    void ReloadBlock(std::uint64_t objectId, const std::string& htmlSourceUtf8, const std::string& urlUtf8, bool allowNetwork);
    void DeactivateBlock(std::uint64_t objectId);

    // Dedicated trusted virtual-host mode for the CuadernU Audiovisual Tutor Whiteboard:
    // - Maps https://tutor.cuadernu.local/ to local assetsWebDir (HTTPS secure context for getUserMedia + AEC3).
    // - Auto-grants COREWEBVIEW2_PERMISSION_KIND_MICROPHONE strictly to https://tutor.cuadernu.local/.
    // - Allows loopback WebSocket/HTTP strictly to 127.0.0.1 and bridges JSON via PostWebMessageAsJson.
    bool ActivateTutorHost(
        HWND parentHwnd,
        std::uint64_t hostId,
        const RECT& boundsPx,
        const std::wstring& assetsWebDir,
        const std::string& queryParamsUtf8,
        std::function<void(const std::string&)> onWebMessageJson,
        std::function<void(std::uint64_t, bool)> onReady = nullptr);

    bool PostMessageJson(std::uint64_t hostId, const std::string& jsonUtf8);

    // Suspend all active blocks immediately (e.g. on WM_SIZE SIZE_MINIMIZED)
    void SuspendAll();
    void Shutdown();
#endif

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace DocStudio
