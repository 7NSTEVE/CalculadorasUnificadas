#pragma once

#include "../core/ai_provider_vault.hpp"
#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"
#include "../media/webview2_html_host.hpp"
#include "../tutor/tutor_document_baker.hpp"
#include "../tutor/tutor_sidecar_bridge.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#include <functional>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace DocStudio::Tutor {

// Non-modal guías de interfaz + material translúcido Win32 window hosting the CuadernU
// Audiovisual Whiteboard Tutor (WebView2 secure origin https://tutor.cuadernu.local/
// + decoupled Python Tutor Runtime managed via Win32 JobObject).
class StudioTutorPanel {
public:
    using BakeCallbackFn = std::function<TutorBakeReport(const std::string& beatsJsonArrayUtf8)>;
    using StatusCallbackFn = std::function<void(const std::string& statusUtf8)>;

    StudioTutorPanel();
    ~StudioTutorPanel();

    StudioTutorPanel(const StudioTutorPanel&) = delete;
    StudioTutorPanel& operator=(const StudioTutorPanel&) = delete;

#ifdef _WIN32
    bool OpenOrFocus(
        HWND ownerHwnd,
        const UiGlass::GlassSettings& glassSettings,
        const AiProviderVault* optionalVault,
        BakeCallbackFn onBakeToDocument,
        StatusCallbackFn onStatusUpdate,
        std::string* outErr = nullptr,
        bool backgroundStealth = false);

    void SyncAppearance(const UiGlass::GlassSettings& glassSettings);
    void SendActionToWeb(const std::string& actionTypeUtf8, const std::string& payloadJsonUtf8 = "{}");
    bool CaptureWebViewPng(const std::wstring& outPathW, int timeoutMs = 8000);
    bool IsWebReady() const noexcept;
    void Close();
    bool IsOpen() const noexcept;
    HWND Hwnd() const noexcept;
#endif

private:
    struct Impl;
    Impl* impl_ = nullptr;
};

} // namespace DocStudio::Tutor
