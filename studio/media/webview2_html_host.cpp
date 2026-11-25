#include "webview2_html_host.hpp"
#include "native_video_engine.hpp"
#include "../core/studio_profile_store.hpp"
#include "../core/utf_conv.hpp"

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <unordered_map>

#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
#include <objbase.h>
#include <ole2.h>
#include <unknwn.h>
#include <shlwapi.h>
#include <wrl.h>
#include <WebView2.h>
#endif

namespace DocStudio {

#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
namespace {

using Microsoft::WRL::Callback;
using Microsoft::WRL::ComPtr;

bool IsLocalHtmlFilePath(const std::string& s) {
    if (s.size() < 4) return false;
    const std::filesystem::path p(Utf8ToWide(s));
    std::wstring ext = p.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    if (ext != L".html" && ext != L".htm" && ext != L".xhtml" && ext != L".svg") return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
}

std::wstring MakeFileUri(const std::wstring& path) {
    std::wstring uri = L"file:///";
    for (wchar_t ch : path) {
        if (ch == L'\\') uri.push_back(L'/');
        else if (ch == L' ') uri += L"%20";
        else uri.push_back(ch);
    }
    return uri;
}

} // namespace
#endif

struct WebView2HtmlHost::Impl {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    struct HostedBlock {
        HWND parentHwnd = nullptr;
        std::uint64_t objectId = 0;
        RECT bounds{};
        std::string htmlSourceUtf8;
        std::string urlUtf8;
        std::wstring virtualHostFolderW;
        std::string tutorQueryParamsUtf8;
        bool isTutorHost = false;
        bool allowNetwork = false;
        bool ready = false;
        bool suspended = false;
        bool visible = true;
        ComPtr<ICoreWebView2Controller> controller;
        ComPtr<ICoreWebView2> webview;
        EventRegistrationToken netFilterToken{};
        bool hasNetFilter = false;
        EventRegistrationToken newWinToken{};
        bool hasNewWinHandler = false;
        EventRegistrationToken permToken{};
        bool hasPermHandler = false;
        EventRegistrationToken msgToken{};
        bool hasMsgHandler = false;
        std::function<void(const std::string&)> onWebMessageJson;
        std::function<void(std::uint64_t, bool)> onReady;
    };

    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    ComPtr<ICoreWebView2Environment> env;
    bool envCreating = false;
    std::unordered_map<std::uint64_t, HostedBlock> blocks;

    void ApplyContent(HostedBlock& blk) {
        if (!blk.webview) return;

        // Configure sandboxed offline network filter (HTTP/HTTPS + WS/WSS) & NewWindowRequested guard
        if (blk.hasNetFilter) {
            blk.webview->remove_WebResourceRequested(blk.netFilterToken);
            blk.hasNetFilter = false;
        }
        if (blk.hasNewWinHandler) {
            blk.webview->remove_NewWindowRequested(blk.newWinToken);
            blk.hasNewWinHandler = false;
        }
        if (blk.hasPermHandler) {
            blk.webview->remove_PermissionRequested(blk.permToken);
            blk.hasPermHandler = false;
        }
        if (blk.hasMsgHandler) {
            blk.webview->remove_WebMessageReceived(blk.msgToken);
            blk.hasMsgHandler = false;
        }

        if (blk.isTutorHost) {
            // Trusted local virtual host for CuadernU Audiovisual Tutor (HTTPS secure context for getUserMedia + AEC3)
            ComPtr<ICoreWebView2_3> wv3;
            if (SUCCEEDED(blk.webview.As(&wv3)) && wv3 && !blk.virtualHostFolderW.empty()) {
                wv3->SetVirtualHostNameToFolderMapping(
                    L"tutor.cuadernu.local",
                    blk.virtualHostFolderW.c_str(),
                    COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
            }
            blk.webview->add_PermissionRequested(
                Callback<ICoreWebView2PermissionRequestedEventHandler>(
                    [](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) -> HRESULT {
                        if (!args) return S_OK;
                        COREWEBVIEW2_PERMISSION_KIND kind{};
                        LPWSTR uri = nullptr;
                        if (SUCCEEDED(args->get_PermissionKind(&kind)) &&
                            (kind == COREWEBVIEW2_PERMISSION_KIND_MICROPHONE ||
                             kind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ) &&
                            SUCCEEDED(args->get_Uri(&uri)) && uri) {
                            const std::wstring u(uri);
                            CoTaskMemFree(uri);
                            if (u.rfind(L"https://tutor.cuadernu.local", 0) == 0 ||
                                u.rfind(L"http://127.0.0.1", 0) == 0 ||
                                u.rfind(L"http://localhost", 0) == 0) {
                                args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
                            }
                        }
                        return S_OK;
                    }).Get(),
                &blk.permToken);
            blk.hasPermHandler = true;

            if (blk.onWebMessageJson) {
                auto cb = blk.onWebMessageJson;
                blk.webview->add_WebMessageReceived(
                    Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                        [cb](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                            if (!args) return S_OK;
                            LPWSTR rawJson = nullptr;
                            if (SUCCEEDED(args->get_WebMessageAsJson(&rawJson)) && rawJson) {
                                const std::string utf8 = WideToUtf8(rawJson);
                                CoTaskMemFree(rawJson);
                                cb(utf8);
                            }
                            return S_OK;
                        }).Get(),
                    &blk.msgToken);
                blk.hasMsgHandler = true;
            }

            std::wstring targetUrl = L"https://tutor.cuadernu.local/index.html";
            if (!blk.tutorQueryParamsUtf8.empty()) {
                targetUrl += L"?" + Utf8ToWide(blk.tutorQueryParamsUtf8);
            }
            blk.webview->Navigate(targetUrl.c_str());
            return;
        }
        const bool allowNet = blk.allowNetwork;
        blk.webview->add_NewWindowRequested(
            Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                [allowNet](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                    if (!args) return S_OK;
                    if (!allowNet) {
                        args->put_Handled(TRUE); // Block window.open escapes in offline sandbox
                        return S_OK;
                    }
                    LPWSTR uri = nullptr;
                    if (SUCCEEDED(args->get_Uri(&uri)) && uri) {
                        const bool safe = IsSafeExternalLinkUrl(WideToUtf8(uri));
                        CoTaskMemFree(uri);
                        if (!safe) args->put_Handled(TRUE);
                    } else {
                        args->put_Handled(TRUE);
                    }
                    return S_OK;
                }).Get(),
            &blk.newWinToken);
        blk.hasNewWinHandler = true;

        if (!blk.allowNetwork) {
            blk.webview->AddWebResourceRequestedFilter(L"http://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
            blk.webview->AddWebResourceRequestedFilter(L"https://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
            blk.webview->AddWebResourceRequestedFilter(L"ws://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
            blk.webview->AddWebResourceRequestedFilter(L"wss://*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
            ComPtr<ICoreWebView2Environment> envRef = env;
            blk.webview->add_WebResourceRequested(
                Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                    [envRef](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                        if (args && envRef) {
                            ComPtr<ICoreWebView2WebResourceResponse> resp;
                            if (SUCCEEDED(envRef->CreateWebResourceResponse(nullptr, 403, L"Offline Sandbox", L"", &resp)) && resp) {
                                args->put_Response(resp.Get());
                            }
                        }
                        return S_OK;
                    }).Get(),
                &blk.netFilterToken);
            blk.hasNetFilter = true;
        }

        if (!blk.htmlSourceUtf8.empty()) {
            const std::wstring wHtml = Utf8ToWide(blk.htmlSourceUtf8);
            blk.webview->NavigateToString(wHtml.c_str());
        } else if (!blk.urlUtf8.empty()) {
            if (blk.allowNetwork && IsSafeExternalLinkUrl(blk.urlUtf8)) {
                blk.webview->Navigate(Utf8ToWide(blk.urlUtf8).c_str());
            } else if (IsLocalHtmlFilePath(blk.urlUtf8)) {
                blk.webview->Navigate(MakeFileUri(Utf8ToWide(blk.urlUtf8)).c_str());
            } else {
                blk.webview->NavigateToString(L"<html><body style='font-family:Segoe UI;color:#64748b;padding:16px'>URL bloqueada por modo offline o esquema no permitido.</body></html>");
            }
        }
    }

    void CreateControllerFor(std::uint64_t objectId) {
        auto it = blocks.find(objectId);
        if (it == blocks.end() || !env) return;
        HWND parent = it->second.parentHwnd;
        if (!parent || !IsWindow(parent)) return;

        auto aliveTok = alive;
        env->CreateCoreWebView2Controller(
            parent,
            Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                [this, aliveTok, objectId](HRESULT hr, ICoreWebView2Controller* ctrl) -> HRESULT {
                    if (!aliveTok || !aliveTok->load()) {
                        if (ctrl) ctrl->Close();
                        return S_OK;
                    }
                    auto bit = blocks.find(objectId);
                    if (bit == blocks.end()) {
                        if (ctrl) ctrl->Close();
                        return S_OK;
                    }
                    if (FAILED(hr) || !ctrl) {
                        if (bit->second.onReady) bit->second.onReady(objectId, false);
                        return S_OK;
                    }
                    bit->second.controller = ctrl;
                    ctrl->get_CoreWebView2(&bit->second.webview);
                    if (bit->second.webview) {
                        ComPtr<ICoreWebView2Settings> st;
                        if (SUCCEEDED(bit->second.webview->get_Settings(&st)) && st) {
                            st->put_IsStatusBarEnabled(FALSE);
                            st->put_AreDefaultScriptDialogsEnabled(FALSE);
                            st->put_IsZoomControlEnabled(FALSE);
                        }
                    }
                    ctrl->put_Bounds(bit->second.bounds);
                    ctrl->put_IsVisible(bit->second.visible ? TRUE : FALSE);
                    bit->second.ready = true;
                    bit->second.suspended = false;
                    ApplyContent(bit->second);
                    if (bit->second.onReady) bit->second.onReady(objectId, true);
                    return S_OK;
                }).Get());
    }

    void SuspendOne(HostedBlock& blk) {
        if (!blk.controller || !blk.webview || blk.suspended) return;
        blk.controller->put_IsVisible(FALSE);
        blk.visible = false;
        ComPtr<ICoreWebView2_3> wv3;
        if (SUCCEEDED(blk.webview.As(&wv3)) && wv3) {
            auto aliveTok = alive;
            wv3->TrySuspend(Callback<ICoreWebView2TrySuspendCompletedHandler>(
                [this, aliveTok, id = blk.objectId](HRESULT hr, BOOL ok) -> HRESULT {
                    if (!aliveTok || !aliveTok->load()) return S_OK;
                    auto it = blocks.find(id);
                    if (it != blocks.end() && SUCCEEDED(hr) && ok) {
                        it->second.suspended = true;
                    }
                    return S_OK;
                }).Get());
        } else {
            blk.suspended = true;
        }
    }
#endif
};

WebView2HtmlHost::WebView2HtmlHost() : impl_(new Impl()) {}
WebView2HtmlHost::~WebView2HtmlHost() {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (impl_ && impl_->alive) impl_->alive->store(false);
#endif
#ifdef _WIN32
    Shutdown();
#endif
    delete impl_;
}

bool WebView2HtmlHost::IsWebView2RuntimeInstalled(std::string* outVersionUtf8) {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    LPWSTR ver = nullptr;
    const HRESULT hr = GetAvailableCoreWebView2BrowserVersionString(nullptr, &ver);
    if (SUCCEEDED(hr) && ver && ver[0] != L'\0') {
        if (outVersionUtf8) *outVersionUtf8 = WideToUtf8(ver);
        CoTaskMemFree(ver);
        return true;
    }
    if (ver) CoTaskMemFree(ver);
    return false;
#else
    (void)outVersionUtf8;
    return false;
#endif
}

bool WebView2HtmlHost::IsEnvironmentInitialized() const noexcept {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    return impl_ && impl_->env != nullptr;
#else
    return false;
#endif
}

bool WebView2HtmlHost::IsBlockActive(std::uint64_t objectId) const noexcept {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return false;
    auto it = impl_->blocks.find(objectId);
    return it != impl_->blocks.end() && !it->second.suspended && it->second.visible;
#else
    (void)objectId;
    return false;
#endif
}

bool WebView2HtmlHost::IsBlockSuspended(std::uint64_t objectId) const noexcept {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return false;
    auto it = impl_->blocks.find(objectId);
    return it != impl_->blocks.end() && it->second.suspended;
#else
    (void)objectId;
    return false;
#endif
}

std::size_t WebView2HtmlHost::ActiveBlockCount() const noexcept {
#if defined(_WIN32) && defined(DOC_STUDIO_HAS_WEBVIEW2)
    return impl_ ? impl_->blocks.size() : 0u;
#else
    return 0u;
#endif
}

#ifdef _WIN32
bool WebView2HtmlHost::ActivateBlock(
    HWND parentHwnd,
    std::uint64_t objectId,
    const RECT& boundsPx,
    const std::string& htmlSourceUtf8,
    const std::string& urlUtf8,
    bool allowNetwork,
    std::function<void(std::uint64_t, bool)> onReady)
{
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_ || !parentHwnd || !IsWindow(parentHwnd)) return false;

    auto& blk = impl_->blocks[objectId];
    blk.parentHwnd = parentHwnd;
    blk.objectId = objectId;
    blk.bounds = boundsPx;
    blk.htmlSourceUtf8 = htmlSourceUtf8;
    blk.urlUtf8 = urlUtf8;
    blk.allowNetwork = allowNetwork;
    blk.visible = true;
    blk.onReady = std::move(onReady);

    if (blk.controller && blk.webview) {
        if (blk.suspended) {
            ComPtr<ICoreWebView2_3> wv3;
            if (SUCCEEDED(blk.webview.As(&wv3)) && wv3) wv3->Resume();
            blk.suspended = false;
        }
        blk.controller->put_Bounds(boundsPx);
        blk.controller->put_IsVisible(TRUE);
        impl_->ApplyContent(blk);
        if (blk.onReady) blk.onReady(objectId, true);
        return true;
    }

    if (impl_->env) {
        impl_->CreateControllerFor(objectId);
        return true;
    }

    if (!impl_->envCreating) {
        impl_->envCreating = true;
        const std::wstring userDataDir = (std::filesystem::path(StudioProfileStore::DefaultProfileRootDir()) / L"WebView2Cache").wstring();
        auto aliveTok = impl_->alive;
        const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            userDataDir.c_str(),
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this, aliveTok](HRESULT envHr, ICoreWebView2Environment* createdEnv) -> HRESULT {
                    if (!aliveTok || !aliveTok->load()) return S_OK;
                    impl_->envCreating = false;
                    if (FAILED(envHr) || !createdEnv) {
                        for (auto& kv : impl_->blocks) {
                            if (kv.second.onReady) kv.second.onReady(kv.first, false);
                        }
                        return S_OK;
                    }
                    impl_->env = createdEnv;
                    for (auto& kv : impl_->blocks) {
                        if (!kv.second.controller) {
                            impl_->CreateControllerFor(kv.first);
                        }
                    }
                    return S_OK;
                }).Get());
        if (FAILED(hr)) {
            impl_->envCreating = false;
            return false;
        }
    }
    return true;
#else
    (void)parentHwnd; (void)objectId; (void)boundsPx; (void)htmlSourceUtf8; (void)urlUtf8; (void)allowNetwork; (void)onReady;
    return false;
#endif
}

void WebView2HtmlHost::SyncBlockBounds(std::uint64_t objectId, const RECT& boundsPx, bool visibleInViewport) {
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return;
    auto it = impl_->blocks.find(objectId);
    if (it == impl_->blocks.end()) return;
    auto& blk = it->second;
    blk.bounds = boundsPx;
    if (!blk.controller) return;
    if (!visibleInViewport || boundsPx.right - boundsPx.left < 24 || boundsPx.bottom - boundsPx.top < 24) {
        impl_->SuspendOne(blk);
        return;
    }
    if (blk.suspended && blk.webview) {
        ComPtr<ICoreWebView2_3> wv3;
        if (SUCCEEDED(blk.webview.As(&wv3)) && wv3) wv3->Resume();
        blk.suspended = false;
    }
    blk.visible = true;
    blk.controller->put_Bounds(boundsPx);
    blk.controller->put_IsVisible(TRUE);
#else
    (void)objectId; (void)boundsPx; (void)visibleInViewport;
#endif
}

void WebView2HtmlHost::CaptureSnapshotAndSuspend(
    std::uint64_t objectId,
    std::function<void(std::uint64_t, std::vector<std::uint8_t>)> onSnapshotPng)
{
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return;
    auto it = impl_->blocks.find(objectId);
    if (it == impl_->blocks.end() || !it->second.webview) {
        if (onSnapshotPng) onSnapshotPng(objectId, {});
        return;
    }
    ComPtr<IStream> memStream;
    memStream.Attach(SHCreateMemStream(nullptr, 0));
    if (!memStream) {
        impl_->SuspendOne(it->second);
        if (onSnapshotPng) onSnapshotPng(objectId, {});
        return;
    }
    auto aliveTok = impl_->alive;
    it->second.webview->CapturePreview(
        COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG,
        memStream.Get(),
        Callback<ICoreWebView2CapturePreviewCompletedHandler>(
            [this, aliveTok, objectId, memStream, cb = std::move(onSnapshotPng)](HRESULT hr) -> HRESULT {
                if (!aliveTok || !aliveTok->load()) return S_OK;
                std::vector<std::uint8_t> png;
                if (SUCCEEDED(hr) && memStream) {
                    STATSTG stg{};
                    if (SUCCEEDED(memStream->Stat(&stg, STATFLAG_NONAME)) && stg.cbSize.QuadPart > 0 && stg.cbSize.QuadPart < 32ull * 1024ull * 1024ull) {
                        png.resize(static_cast<std::size_t>(stg.cbSize.QuadPart));
                        LARGE_INTEGER zero{};
                        memStream->Seek(zero, STREAM_SEEK_SET, nullptr);
                        ULONG got = 0;
                        memStream->Read(png.data(), static_cast<ULONG>(png.size()), &got);
                        png.resize(got);
                    }
                }
                auto bit = impl_->blocks.find(objectId);
                if (bit != impl_->blocks.end()) {
                    impl_->SuspendOne(bit->second);
                }
                if (cb) cb(objectId, std::move(png));
                return S_OK;
            }).Get());
#else
    (void)objectId;
    if (onSnapshotPng) onSnapshotPng(objectId, {});
#endif
}

void WebView2HtmlHost::ResumeBlock(std::uint64_t objectId, const RECT& boundsPx) {
    SyncBlockBounds(objectId, boundsPx, true);
}

void WebView2HtmlHost::ReloadBlock(std::uint64_t objectId, const std::string& htmlSourceUtf8, const std::string& urlUtf8, bool allowNetwork) {
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return;
    auto it = impl_->blocks.find(objectId);
    if (it == impl_->blocks.end()) return;
    it->second.htmlSourceUtf8 = htmlSourceUtf8;
    it->second.urlUtf8 = urlUtf8;
    it->second.allowNetwork = allowNetwork;
    impl_->ApplyContent(it->second);
#else
    (void)objectId; (void)htmlSourceUtf8; (void)urlUtf8; (void)allowNetwork;
#endif
}

void WebView2HtmlHost::DeactivateBlock(std::uint64_t objectId) {
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return;
    auto it = impl_->blocks.find(objectId);
    if (it == impl_->blocks.end()) return;
    if (it->second.controller) {
        it->second.controller->Close();
        it->second.controller.Reset();
    }
    it->second.webview.Reset();
    impl_->blocks.erase(it);
#else
    (void)objectId;
#endif
}

bool WebView2HtmlHost::ActivateTutorHost(
    HWND parentHwnd,
    std::uint64_t hostId,
    const RECT& boundsPx,
    const std::wstring& assetsWebDir,
    const std::string& queryParamsUtf8,
    std::function<void(const std::string&)> onWebMessageJson,
    std::function<void(std::uint64_t, bool)> onReady)
{
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_ || !parentHwnd || !IsWindow(parentHwnd)) return false;

    auto& blk = impl_->blocks[hostId];
    blk.parentHwnd = parentHwnd;
    blk.objectId = hostId;
    blk.bounds = boundsPx;
    blk.isTutorHost = true;
    blk.allowNetwork = true;
    blk.virtualHostFolderW = assetsWebDir;
    blk.tutorQueryParamsUtf8 = queryParamsUtf8;
    blk.onWebMessageJson = std::move(onWebMessageJson);
    blk.onReady = std::move(onReady);
    blk.visible = true;

    if (blk.controller && blk.webview) {
        if (blk.suspended) {
            ComPtr<ICoreWebView2_3> wv3;
            if (SUCCEEDED(blk.webview.As(&wv3)) && wv3) wv3->Resume();
            blk.suspended = false;
        }
        blk.controller->put_Bounds(boundsPx);
        blk.controller->put_IsVisible(TRUE);
        impl_->ApplyContent(blk);
        if (blk.onReady) blk.onReady(hostId, true);
        return true;
    }

    if (impl_->env) {
        impl_->CreateControllerFor(hostId);
        return true;
    }

    if (!impl_->envCreating) {
        impl_->envCreating = true;
        const std::wstring userDataDir = (std::filesystem::path(StudioProfileStore::DefaultProfileRootDir()) / L"WebView2Cache").wstring();
        auto aliveTok = impl_->alive;
        const HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr,
            userDataDir.c_str(),
            nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [this, aliveTok](HRESULT envHr, ICoreWebView2Environment* createdEnv) -> HRESULT {
                    if (!aliveTok || !aliveTok->load()) return S_OK;
                    impl_->envCreating = false;
                    if (FAILED(envHr) || !createdEnv) {
                        for (auto& kv : impl_->blocks) {
                            if (kv.second.onReady) kv.second.onReady(kv.first, false);
                        }
                        return S_OK;
                    }
                    impl_->env = createdEnv;
                    for (auto& kv : impl_->blocks) {
                        if (!kv.second.controller) {
                            impl_->CreateControllerFor(kv.first);
                        }
                    }
                    return S_OK;
                }).Get());
        if (FAILED(hr)) {
            impl_->envCreating = false;
            return false;
        }
    }
    return true;
#else
    (void)parentHwnd; (void)hostId; (void)boundsPx; (void)assetsWebDir;
    (void)queryParamsUtf8; (void)onWebMessageJson; (void)onReady;
    return false;
#endif
}

bool WebView2HtmlHost::PostMessageJson(std::uint64_t hostId, const std::string& jsonUtf8) {
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return false;
    auto it = impl_->blocks.find(hostId);
    if (it == impl_->blocks.end() || !it->second.webview) return false;
    const std::wstring wJson = Utf8ToWide(jsonUtf8);
    return SUCCEEDED(it->second.webview->PostWebMessageAsJson(wJson.c_str()));
#else
    (void)hostId; (void)jsonUtf8;
    return false;
#endif
}

void WebView2HtmlHost::SuspendAll() {
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return;
    for (auto& kv : impl_->blocks) {
        impl_->SuspendOne(kv.second);
    }
#endif
}

void WebView2HtmlHost::Shutdown() {
#if defined(DOC_STUDIO_HAS_WEBVIEW2)
    if (!impl_) return;
    for (auto& kv : impl_->blocks) {
        if (kv.second.controller) {
            kv.second.controller->Close();
            kv.second.controller.Reset();
        }
        kv.second.webview.Reset();
    }
    impl_->blocks.clear();
    impl_->env.Reset();
    impl_->envCreating = false;
#endif
}
#endif

} // namespace DocStudio
