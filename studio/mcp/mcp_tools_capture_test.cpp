#include "mcp_server.hpp"

#include "../core/cudoc_storage.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/winrt_adapters.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
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
#include <psapi.h>
#include <wincodec.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::string ToLowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool DecodePngBytesToBgra32(
    const std::vector<std::uint8_t>& pngBytes,
    RasterImageBgra32& outImg,
    std::string* outErr = nullptr)
{
    outImg = {};
    if (pngBytes.empty()) {
        if (outErr) *outErr = "PNG vacio.";
        return false;
    }
#ifdef _WIN32
    using Microsoft::WRL::ComPtr;
    bool localComInit = false;
    struct ComUninitGuard {
        bool& flag;
        ~ComUninitGuard() { if (flag) CoUninitialize(); }
    } comGuard{ localComInit };
    ComPtr<IWICImagingFactory> factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (hr == CO_E_NOTINITIALIZED) {
        const HRESULT hrInit = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (hrInit == S_OK || hrInit == S_FALSE) localComInit = true;
        hr = CoCreateInstance(
            CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    }
    if (FAILED(hr) || !factory) {
        if (outErr) *outErr = "No se pudo inicializar WICImagingFactory.";
        return false;
    }
    ComPtr<IWICStream> stream;
    hr = factory->CreateStream(&stream);
    if (FAILED(hr) || !stream) {
        if (outErr) *outErr = "No se pudo crear IWICStream.";
        return false;
    }
    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(reinterpret_cast<const BYTE*>(pngBytes.data())),
        static_cast<DWORD>(pngBytes.size()));
    if (FAILED(hr)) {
        if (outErr) *outErr = "InitializeFromMemory fallo en WIC.";
        return false;
    }
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(
        stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr) || !decoder) {
        if (outErr) *outErr = "No se pudo decodificar imagen PNG.";
        return false;
    }
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr) || !frame) {
        if (outErr) *outErr = "No se pudo leer el frame 0 del PNG.";
        return false;
    }
    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);
    if (w == 0 || h == 0 || w > 16384 || h > 16384) {
        if (outErr) *outErr = "Dimensiones de PNG invalidas.";
        return false;
    }
    ComPtr<IWICFormatConverter> conv;
    hr = factory->CreateFormatConverter(&conv);
    if (FAILED(hr) || !conv) {
        if (outErr) *outErr = "No se pudo crear convertidor WIC.";
        return false;
    }
    hr = conv->Initialize(
        frame.Get(), GUID_WICPixelFormat32bppBGRA,
        WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        if (outErr) *outErr = "No se pudo convertir PNG a BGRA32.";
        return false;
    }
    outImg.width = static_cast<int>(w);
    outImg.height = static_cast<int>(h);
    outImg.pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
    const UINT stride = w * 4u;
    hr = conv->CopyPixels(nullptr, stride, static_cast<UINT>(outImg.pixels.size()), outImg.pixels.data());
    if (FAILED(hr)) {
        outImg = {};
        if (outErr) *outErr = "CopyPixels WIC fallo.";
        return false;
    }
    return true;
#else
    if (outErr) *outErr = "Decodificador PNG solo disponible en Windows.";
    return false;
#endif
}

std::wstring ResolveToolExe(const std::wstring& preferredAbs, const std::wstring& fallbackCmd) {
    std::error_code ec;
    if (!preferredAbs.empty() && fs::exists(fs::path(preferredAbs), ec)) {
        return preferredAbs;
    }
    return fallbackCmd;
}

bool RunSubprocessCapture(
    const std::wstring& cmdLineW,
    const std::wstring& workDirW,
    int timeoutMs,
    const std::vector<std::pair<std::wstring, std::wstring>>& extraEnv,
    int& outExitCode,
    std::string& outCombinedOutput,
    std::string* outErr = nullptr)
{
    outExitCode = -1;
    outCombinedOutput.clear();
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0)) {
        if (outErr) *outErr = "CreatePipe fallo.";
        return false;
    }
    SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    std::vector<wchar_t> envBlock;
    LPVOID envPtr = nullptr;
    if (!extraEnv.empty()) {
        // Clone current environment and override/add extraEnv entries
        std::map<std::wstring, std::wstring> envMap;
        if (LPWCH raw = GetEnvironmentStringsW()) {
            const wchar_t* cur = raw;
            while (*cur) {
                std::wstring entry(cur);
                cur += entry.size() + 1;
                const auto eq = entry.find(L'=');
                if (eq != std::wstring::npos && eq > 0) {
                    envMap[entry.substr(0, eq)] = entry.substr(eq + 1);
                }
            }
            FreeEnvironmentStringsW(raw);
        }
        for (const auto& kv : extraEnv) {
            // Erase case-insensitive matches first
            std::wstring keyLower = kv.first;
            for (wchar_t& ch : keyLower) ch = static_cast<wchar_t>(towlower(ch));
            for (auto it = envMap.begin(); it != envMap.end(); ) {
                std::wstring existingLower = it->first;
                for (wchar_t& ch : existingLower) ch = static_cast<wchar_t>(towlower(ch));
                if (existingLower == keyLower) it = envMap.erase(it);
                else ++it;
            }
            envMap[kv.first] = kv.second;
        }
        for (const auto& kv : envMap) {
            const std::wstring line = kv.first + L"=" + kv.second;
            envBlock.insert(envBlock.end(), line.begin(), line.end());
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0');
        envPtr = envBlock.data();
    }

    HANDLE hNullIn = CreateFileW(
        L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.hStdInput = (hNullIn && hNullIn != INVALID_HANDLE_VALUE) ? hNullIn : INVALID_HANDLE_VALUE;

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = cmdLineW;
    const wchar_t* cwdPtr = workDirW.empty() ? nullptr : workDirW.c_str();
    const DWORD flags = CREATE_NO_WINDOW | (envPtr ? CREATE_UNICODE_ENVIRONMENT : 0u);
    const BOOL ok = CreateProcessW(
        nullptr, mutableCmd.data(), nullptr, nullptr, TRUE, flags, envPtr, cwdPtr, &si, &pi);
    CloseHandle(hWrite);
    if (hNullIn && hNullIn != INVALID_HANDLE_VALUE) CloseHandle(hNullIn);

    if (!ok) {
        CloseHandle(hRead);
        if (outErr) *outErr = "CreateProcessW fallo (error=" + std::to_string(GetLastError()) + ").";
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((std::max)(1000, timeoutMs));
    bool timedOut = false;
    char buf[4096];
    for (;;) {
        DWORD avail = 0;
        if (PeekNamedPipe(hRead, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            DWORD got = 0;
            if (ReadFile(hRead, buf, (std::min)(static_cast<DWORD>(sizeof(buf)), avail), &got, nullptr) && got > 0) {
                if (outCombinedOutput.size() < 2u * 1024u * 1024u) {
                    outCombinedOutput.append(buf, got);
                }
            }
            continue;
        }
        const DWORD waitRes = WaitForSingleObject(pi.hProcess, 25);
        if (waitRes == WAIT_OBJECT_0) {
            // Drain any remaining buffered pipe data
            for (;;) {
                DWORD rem = 0;
                if (!PeekNamedPipe(hRead, nullptr, 0, nullptr, &rem, nullptr) || rem == 0) break;
                DWORD got = 0;
                if (!ReadFile(hRead, buf, (std::min)(static_cast<DWORD>(sizeof(buf)), rem), &got, nullptr) || got == 0) break;
                if (outCombinedOutput.size() < 2u * 1024u * 1024u) {
                    outCombinedOutput.append(buf, got);
                }
            }
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            timedOut = true;
            TerminateProcess(pi.hProcess, 124);
            WaitForSingleObject(pi.hProcess, 1500);
            break;
        }
    }

    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    outExitCode = timedOut ? 124 : static_cast<int>(code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    CloseHandle(hRead);
    if (timedOut) {
        if (outErr) *outErr = "Subproceso agoto el tiempo limite (" + std::to_string(timeoutMs) + " ms).";
        return false;
    }
    return true;
#else
    if (outErr) *outErr = "Ejecucion de subproceso solo soportada en Windows.";
    return false;
#endif
}

bool EnsureGuiReadyForCapture(
    McpServerContext& ctx,
    const json& args,
    std::string& outInstanceId,
    bool& outAutoSpawned,
    std::string& outErr)
{
    outAutoSpawned = false;
    outInstanceId = args.value("instance_id", "");
    if (!outInstanceId.empty() || ctx.guiManager.HasActiveConnection()) {
        return true;
    }
    if (!args.value("auto_spawn", true)) {
        outErr = "No hay ninguna instancia GUI activa ni adjunta. Llama a cuadernu_gui(action=\"spawn_background\") o \"attach_live\" primero.";
        return false;
    }
    LiveGuiInstanceInfo info{};
    if (!ctx.guiManager.SpawnBackgroundGui(
            ctx.Options().cuadernuExePathW,
            ctx.EffectiveProfileRootW(),
            args.value("width", 1440),
            args.value("height", 900),
            args.value("theme", "porcelain"),
            args.value("mode", "paginated"),
            true,
            info,
            &outErr))
    {
        return false;
    }
    outInstanceId = info.instanceId;
    outAutoSpawned = true;
    return true;
}

bool SyncActiveSessionToGuiIfRequested(
    McpServerContext& ctx,
    const json& args,
    const std::string& instanceId,
    std::string& outErr)
{
    if (!args.value("sync_session", false)) return true;
    auto sess = ctx.FindSession(args.value("session_id", ""));
    if (!sess) return true;
    ctx.PackExtensions(*sess);
    std::vector<std::uint8_t> bytes;
    if (!CudocStorage::SerializeToBytes(sess->doc, bytes, &outErr)) {
        return false;
    }
    json req = {
        { "action", "sync_session_to_gui" },
        { "title", sess->doc.Title() },
        { "active_page_index", sess->activePageIndex },
        { "cudoc_base64", Base64Encode(bytes) }
    };
    json resp;
    if (!ctx.guiManager.SendRequest(req, resp, instanceId, 20000, &outErr)) {
        return false;
    }
    if (!resp.value("ok", false)) {
        outErr = resp.value("error", "sync_session_to_gui fallo en GUI.");
        return false;
    }
    return true;
}

int ParseTecSectionIndex(const json& args) {
    if (args.contains("section") && args["section"].is_number_integer()) {
        return std::clamp(args["section"].get<int>(), 0, 6);
    }
    const std::string s = ToLowerAscii(args.value("section", "resumen"));
    if (s == "cursos" || s == "courses") return 1;
    if (s == "calendario" || s == "calendar") return 2;
    if (s == "expediente" || s == "malla" || s == "curriculum") return 3;
    if (s == "horarios" || s == "schedule") return 4;
    if (s == "biblioteca" || s == "library") return 5;
    if (s == "tramites" || s == "perfil" || s == "profile") return 6;
    return 0;
}

json SerializeInstanceInfo(const LiveGuiInstanceInfo& info) {
    return json{
        { "pid", info.pid },
        { "instance_id", info.instanceId },
        { "pipe_name", info.pipeName },
        { "mode", info.mode },
        { "title", info.titleUtf8 },
        { "profile_dir", info.profileDirUtf8 },
        { "managed_by_this_server", info.managedByThisServer },
        { "alive", info.alive }
    };
}

} // namespace

void RegisterCaptureGuiAndTestTools(McpProtocolRouter& router, McpServerContext& ctx) {
    // =========================================================================
    // 33. cuadernu_capture — Plane 1 offscreen PNG + Plane 2 stealth/live GUI captures
    // =========================================================================
    router.RegisterTool({
        "cuadernu_capture",
        "Captura visual en segundo plano sin robar foco (0 HWND / 0% GPU offscreen para paginas y lienzos, "
        "o ventana completa / TEC Digital / Gemini / Proyector / WebView2 / Tutor en segundo plano DWMWA_CLOAK). "
        "Guarda PNG en disco y devuelve bloque nativo MCP image/png.",
        MakeToolSchema(
            { "page_png", "canvas_region_png", "viewport_png", "all_pages_png",
              "gui_window_png", "gui_viewport_png", "tec_dialog_png", "gemini_dialog_png",
              "projector_png", "html_embed_png", "tutor_panel_png" },
            {
                { "session_id", { { "type", "string" } } },
                { "instance_id", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "max_pages", { { "type", "integer" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } },
                { "w", { { "type", "number" } } },
                { "h", { { "type", "number" } } },
                { "width_px", { { "type", "integer" } } },
                { "height_px", { { "type", "integer" } } },
                { "presenter_mode", { { "type", "boolean" } } },
                { "dark_theme", { { "type", "boolean" } } },
                { "out_path", { { "type", "string" } } },
                { "out_dir", { { "type", "string" } } },
                { "include_image_block", { { "type", "boolean" } } },
                { "auto_spawn", { { "type", "boolean" } } },
                { "sync_session", { { "type", "boolean" } } },
                { "section", { { "type", "string" }, { "description", "Seccion TEC Digital (0..6 o resumen/cursos/calendario/expediente/horarios/biblioteca/tramites)" } } },
                { "subview", { { "type", "integer" } } },
                { "inner_tab", { { "type", "integer" } } },
                { "dialog_kind", { { "type", "string" } } },
                { "frozen", { { "type", "boolean" } } },
                { "object_id", { { "type", "integer" } } },
                { "wait_ready_ms", { { "type", "integer" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            const bool includeImg = args.value("include_image_block", true);

            // ---------------- Plane 1: Pure offscreen software D2D (0 HWND, 0% GPU) ----------------
            if (action == "page_png" || action == "canvas_region_png" || action == "viewport_png") {
                auto sess = ctx.GetOrCreateSession(args);
                std::vector<std::uint8_t> png;
                std::string err;
                const int widthPx = args.value("width_px", 1280);
                const int heightPx = args.value("height_px", 900);
                bool ok = false;

                if (action == "page_png") {
                    const std::size_t pgIdx = args.contains("page_index")
                        ? static_cast<std::size_t>((std::max)(0, args.value("page_index", 0)))
                        : sess->activePageIndex;
                    ok = ctx.RenderPagePng(*sess, pgIdx, widthPx, png, &err);
                } else if (action == "canvas_region_png") {
                    DocRect region{
                        args.value("x", 0.0),
                        args.value("y", 0.0),
                        args.value("w", 960.0),
                        args.value("h", 720.0)
                    };
                    ok = ctx.RenderCanvasRegionPng(*sess, region, widthPx, heightPx, png, &err);
                } else {
                    ok = ctx.RenderViewportPng(
                        *sess, widthPx, heightPx,
                        args.value("presenter_mode", false),
                        args.value("dark_theme", false),
                        png, &err);
                }
                if (!ok || png.empty()) {
                    return McpToolResult::Err(err.empty() ? "Render offscreen fallo." : err);
                }

                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("out_path", ""), action, ".png");
                if (!ctx.WriteBytesToFile(outW, png, &err)) {
                    return McpToolResult::Err(err);
                }

                RasterImageBgra32 decoded;
                DecodePngBytesToBgra32(png, decoded);
                json payload = {
                    { "ok", true },
                    { "plane", "plane1_offscreen_cpu" },
                    { "action", action },
                    { "session_id", sess->sessionId },
                    { "path", WideToUtf8(outW) },
                    { "bytes", png.size() },
                    { "width_px", decoded.width },
                    { "height_px", decoded.height }
                };
                if (includeImg) {
                    return McpToolResult::OkWithImage(std::move(payload), std::move(png), "image/png");
                }
                return McpToolResult::Ok(std::move(payload));
            }

            if (action == "all_pages_png") {
                auto sess = ctx.GetOrCreateSession(args);
                const int widthPx = args.value("width_px", 1024);
                const std::size_t totalPages = sess->doc.Mode() == DocumentMode::InfiniteCanvas ? 1u : sess->doc.Pages().size();
                const std::size_t limit = (std::min)(totalPages, static_cast<std::size_t>(std::clamp(args.value("max_pages", 20), 1, 100)));
                const std::wstring outDirW = args.value("out_dir", "").empty()
                    ? ctx.EffectiveArtifactsDirW()
                    : Utf8ToWide(args.value("out_dir", ""));
                std::error_code ec;
                fs::create_directories(fs::path(outDirW), ec);

                json pagesArr = json::array();
                std::vector<std::uint8_t> firstPng;
                for (std::size_t i = 0; i < limit; ++i) {
                    std::vector<std::uint8_t> png;
                    std::string err;
                    if (!ctx.RenderPagePng(*sess, i, widthPx, png, &err)) {
                        return McpToolResult::Err("Error renderizando pagina " + std::to_string(i) + ": " + err);
                    }
                    const std::wstring fW = (fs::path(outDirW) / (L"page_" + std::to_wstring(i + 1) + L".png")).wstring();
                    if (!ctx.WriteBytesToFile(fW, png, &err)) {
                        return McpToolResult::Err(err);
                    }
                    pagesArr.push_back({
                        { "page_index", i },
                        { "path", WideToUtf8(fW) },
                        { "bytes", png.size() }
                    });
                    if (i == 0 && includeImg) firstPng = std::move(png);
                }
                json payload = {
                    { "ok", true },
                    { "plane", "plane1_offscreen_cpu" },
                    { "session_id", sess->sessionId },
                    { "total_pages", totalPages },
                    { "rendered_pages", pagesArr }
                };
                if (includeImg && !firstPng.empty()) {
                    return McpToolResult::OkWithImage(std::move(payload), std::move(firstPng), "image/png");
                }
                return McpToolResult::Ok(std::move(payload));
            }

            // ---------------- Plane 2: Stealth Background or Live Attached GUI ----------------
            if (action == "gui_window_png" || action == "gui_viewport_png" ||
                action == "tec_dialog_png" || action == "gemini_dialog_png" ||
                action == "projector_png" || action == "html_embed_png" ||
                action == "tutor_panel_png")
            {
                std::string instanceId;
                bool autoSpawned = false;
                std::string err;
                if (!EnsureGuiReadyForCapture(ctx, args, instanceId, autoSpawned, err)) {
                    return McpToolResult::Err(err);
                }

                // Push Plane 1 document into the GUI ONLY when explicitly requested via sync_session=true,
                // or when session_id is explicitly provided for a managed stealth background instance
                // (never silently overwrite a live_desktop user document or direct GUI strokes!).
                const auto activeInfo = ctx.guiManager.ActiveInstanceInfo();
                const bool isLiveDesktop = (activeInfo.mode == "live_desktop");
                const bool explicitSession = args.contains("session_id") && args["session_id"].is_string() && !args["session_id"].get<std::string>().empty();
                const bool defaultSync = explicitSession && !isLiveDesktop &&
                    (action == "gui_window_png" || action == "gui_viewport_png" || action == "projector_png" || action == "html_embed_png");
                if (args.value("sync_session", defaultSync)) {
                    json syncArgs = args;
                    syncArgs["sync_session"] = true;
                    if (!SyncActiveSessionToGuiIfRequested(ctx, syncArgs, instanceId, err)) {
                        return McpToolResult::Err(err);
                    }
                }

                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("out_path", ""), action, ".png");
                json ipcReq = {
                    { "action", action },
                    { "out_path", WideToUtf8(outW) }
                };
                if (action == "tec_dialog_png") {
                    ipcReq["section"] = ParseTecSectionIndex(args);
                    ipcReq["subview"] = args.value("subview", 0);
                    ipcReq["inner_tab"] = args.value("inner_tab", 0);
                } else if (action == "gemini_dialog_png") {
                    ipcReq["dialog_kind"] = args.value("dialog_kind", "hub");
                } else if (action == "projector_png") {
                    ipcReq["frozen"] = args.value("frozen", false);
                } else if (action == "html_embed_png") {
                    ipcReq["object_id"] = args.value("object_id", std::uint64_t{ 0 });
                } else if (action == "tutor_panel_png") {
                    ipcReq["wait_ready_ms"] = args.value("wait_ready_ms", 45000);
                }

                json ipcResp;
                const int waitReady = ipcReq.value("wait_ready_ms", 0);
                const int timeoutMs = (action == "tutor_panel_png")
                    ? std::clamp(waitReady + 30000, 65000, 125000)
                    : ((action == "tec_dialog_png") ? 35000 : 25000);
                if (!ctx.guiManager.SendRequest(ipcReq, ipcResp, instanceId, timeoutMs, &err)) {
                    return McpToolResult::Err(err);
                }
                if (!ipcResp.value("ok", false)) {
                    return McpToolResult::Err(ipcResp.value("error", "Captura GUI fallo."), ipcResp);
                }

                std::vector<std::uint8_t> png;
                if (!ctx.ReadBytesFromFile(outW, png, &err) || png.empty()) {
                    return McpToolResult::Err("La GUI reporto ok pero no escribio PNG legible: " + err);
                }
                RasterImageBgra32 decoded;
                DecodePngBytesToBgra32(png, decoded);

                json payload = {
                    { "ok", true },
                    { "plane", "plane2_gui_ipc" },
                    { "action", action },
                    { "instance_id", instanceId },
                    { "auto_spawned_background_gui", autoSpawned },
                    { "path", WideToUtf8(outW) },
                    { "bytes", png.size() },
                    { "width_px", decoded.width },
                    { "height_px", decoded.height },
                    { "gui_details", ipcResp }
                };
                if (includeImg) {
                    return McpToolResult::OkWithImage(std::move(payload), std::move(png), "image/png");
                }
                return McpToolResult::Ok(std::move(payload));
            }

            return McpToolResult::Err("Accion desconocida en cuadernu_capture: " + action);
        }
    });

    // =========================================================================
    // 34. cuadernu_gui — Plane 2A Stealth Background GUI & Plane 2B Live Desktop Control
    // =========================================================================
    router.RegisterTool({
        "cuadernu_gui",
        "Control completo de instancias GUI de CuadernU en segundo plano (DWMWA_CLOAK, sin robar foco, JobObject) "
        "o conectadas a una ventana interactiva abierta: camara, herramienta, tema, paneles, pestanas, paleta de comandos, "
        "inyeccion de trazos/puntero/teclado, sincronizacion de sesiones y apertura de dialogos.",
        MakeToolSchema(
            { "list_instances", "spawn_background", "close_background", "attach_live", "detach",
              "get_state", "set_camera", "set_tool", "set_theme", "set_panels", "manage_tabs",
              "execute_command", "list_commands", "search_commands",
              "inject_stroke", "inject_pointer", "inject_key",
              "sync_session_to_gui", "sync_gui_to_session", "open_file_in_gui",
              "open_tec_dialog", "open_tutor_panel", "open_projector", "run_dpi_cycle_live" },
            {
                { "instance_id", { { "type", "string" } } },
                { "session_id", { { "type", "string" } } },
                { "pid", { { "type", "integer" } } },
                { "all", { { "type", "boolean" } } },
                { "width", { { "type", "integer" } } },
                { "height", { { "type", "integer" } } },
                { "theme", { { "type", "string" } } },
                { "mode", { { "type", "string" } } },
                { "no_profile_write", { { "type", "boolean" } } },
                { "zoom", { { "type", "number" } } },
                { "pan_x", { { "type", "number" } } },
                { "pan_y", { { "type", "number" } } },
                { "fit_page", { { "type", "boolean" } } },
                { "tool", { { "type", "string" } } },
                { "stroke_width", { { "type", "number" } } },
                { "color", { { "type", "string" } } },
                { "show_sidebar", { { "type", "boolean" } } },
                { "show_inspector", { { "type", "boolean" } } },
                { "inspector_tab", { { "type", "integer" } } },
                { "focus_mode", { { "type", "boolean" } } },
                { "presenter_mode", { { "type", "boolean" } } },
                { "tab_action", { { "type", "string" } } },
                { "tab_index", { { "type", "integer" } } },
                { "command_id", { { "type", "integer" } } },
                { "allow_modal", { { "type", "boolean" } } },
                { "query", { { "type", "string" } } },
                { "max_results", { { "type", "integer" } } },
                { "points", { { "type", "array" } } },
                { "pen_style", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "is_canvas", { { "type", "boolean" } } },
                { "event", { { "type", "string" } } },
                { "pressed", { { "type", "boolean" } } },
                { "x", { { "type", "number" } } },
                { "y", { { "type", "number" } } },
                { "vk", { { "type", "integer" } } },
                { "text", { { "type", "string" } } },
                { "ctrl", { { "type", "boolean" } } },
                { "shift", { { "type", "boolean" } } },
                { "alt", { { "type", "boolean" } } },
                { "path", { { "type", "string" } } },
                { "profile_dir", { { "type", "string" } } },
                { "section", { { "type", "string" } } },
                { "subview", { { "type", "integer" } } },
                { "inner_tab", { { "type", "integer" } } },
                { "out_path", { { "type", "string" } } },
                { "frozen", { { "type", "boolean" } } },
                { "close", { { "type", "boolean" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");

            if (action == "list_instances") {
                const std::string pDir = args.value("profile_dir", "");
                const std::wstring rootW = pDir.empty() ? ctx.EffectiveProfileRootW() : Utf8ToWide(pDir);
                const auto list = ctx.guiManager.ListInstances(rootW);
                json arr = json::array();
                for (const auto& info : list) arr.push_back(SerializeInstanceInfo(info));
                return McpToolResult::Ok({
                    { "ok", true },
                    { "has_active_connection", ctx.guiManager.HasActiveConnection() },
                    { "active_instance", ctx.guiManager.HasActiveConnection() ? SerializeInstanceInfo(ctx.guiManager.ActiveInstanceInfo()) : json(nullptr) },
                    { "instances", arr }
                });
            }

            if (action == "list_commands" || action == "search_commands") {
                const auto& cmds = (action == "search_commands")
                    ? ctx.commandPalette.Search(args.value("query", ""), static_cast<std::size_t>(std::clamp(args.value("max_results", 24), 1, 200)))
                    : ctx.commandPalette.All();
                json arr = json::array();
                for (const auto& c : cmds) {
                    arr.push_back({
                        { "command_id", c.commandId },
                        { "slash_trigger", c.slashTriggerUtf8 },
                        { "title", c.titleUtf8 },
                        { "category", c.categoryUtf8 },
                        { "shortcut", c.shortcutUtf8 },
                        { "keywords", c.keywordsUtf8 }
                    });
                }
                return McpToolResult::Ok({
                    { "ok", true },
                    { "count", arr.size() },
                    { "commands", arr }
                });
            }

            if (action == "spawn_background") {
                LiveGuiInstanceInfo info{};
                std::string err;
                if (!ctx.guiManager.SpawnBackgroundGui(
                        ctx.Options().cuadernuExePathW,
                        ctx.EffectiveProfileRootW(),
                        args.value("width", 1440),
                        args.value("height", 900),
                        args.value("theme", "porcelain"),
                        args.value("mode", "paginated"),
                        args.value("no_profile_write", true),
                        info,
                        &err))
                {
                    return McpToolResult::Err(err);
                }
                return McpToolResult::Ok({
                    { "ok", true },
                    { "instance", SerializeInstanceInfo(info) }
                });
            }

            if (action == "close_background") {
                if (args.value("all", false)) {
                    ctx.guiManager.CloseAllBackgroundGuis();
                    return McpToolResult::Ok({ { "ok", true }, { "closed_all", true } });
                }
                std::string err;
                if (!ctx.guiManager.CloseBackgroundGui(args.value("instance_id", ""), &err)) {
                    return McpToolResult::Err(err);
                }
                return McpToolResult::Ok({ { "ok", true } });
            }

            if (action == "attach_live") {
                LiveGuiInstanceInfo info{};
                std::string err;
                const std::string pDir = args.value("profile_dir", "");
                const std::wstring rootW = pDir.empty() ? ctx.EffectiveProfileRootW() : Utf8ToWide(pDir);
                if (!ctx.guiManager.AttachLiveInstance(
                        rootW,
                        args.value("pid", std::uint32_t{ 0 }),
                        info,
                        &err))
                {
                    return McpToolResult::Err(err);
                }
                return McpToolResult::Ok({
                    { "ok", true },
                    { "instance", SerializeInstanceInfo(info) }
                });
            }

            if (action == "detach") {
                ctx.guiManager.DetachCurrent();
                return McpToolResult::Ok({ { "ok", true }, { "detached", true } });
            }

            const std::string instanceId = args.value("instance_id", "");

            if (action == "sync_session_to_gui") {
                auto sess = ctx.GetOrCreateSession(args);
                ctx.PackExtensions(*sess);
                std::vector<std::uint8_t> bytes;
                std::string err;
                if (!CudocStorage::SerializeToBytes(sess->doc, bytes, &err)) {
                    return McpToolResult::Err(err);
                }
                json req = {
                    { "action", "sync_session_to_gui" },
                    { "title", sess->doc.Title() },
                    { "active_page_index", sess->activePageIndex },
                    { "cudoc_base64", Base64Encode(bytes) }
                };
                json resp;
                if (!ctx.guiManager.SendRequest(req, resp, instanceId, 20000, &err)) {
                    return McpToolResult::Err(err);
                }
                if (!resp.value("ok", false)) {
                    return McpToolResult::Err(resp.value("error", "sync_session_to_gui fallo."), resp);
                }
                resp["session_id"] = sess->sessionId;
                return McpToolResult::Ok(std::move(resp));
            }

            if (action == "sync_gui_to_session") {
                json resp;
                std::string err;
                if (!ctx.guiManager.SendRequest({ { "action", "sync_gui_to_session" } }, resp, instanceId, 20000, &err)) {
                    return McpToolResult::Err(err);
                }
                if (!resp.value("ok", false)) {
                    return McpToolResult::Err(resp.value("error", "sync_gui_to_session fallo."), resp);
                }
                const std::string b64 = resp.value("cudoc_base64", "");
                const auto bytes = Base64Decode(b64);
                if (bytes.empty()) {
                    return McpToolResult::Err("sync_gui_to_session devolvio cudoc vacio.");
                }
                auto sess = ctx.GetOrCreateSession(args);
                if (!CudocStorage::DeserializeFromBytes(bytes.data(), bytes.size(), sess->doc, &err)) {
                    return McpToolResult::Err(err);
                }
                ctx.UnpackExtensions(*sess);
                sess->doc.RebuildLayout();
                return McpToolResult::Ok({
                    { "ok", true },
                    { "session_id", sess->sessionId },
                    { "title", sess->doc.Title() },
                    { "page_count", sess->doc.Pages().size() },
                    { "canvas_object_count", sess->doc.CanvasObjects().size() }
                });
            }

            // All remaining GUI control commands forward directly over Named Pipe -> WM_APP_MCP_DISPATCH
            json ipcReq = args;
            if (action == "open_tec_dialog") {
                ipcReq["section"] = ParseTecSectionIndex(args);
            }
            json resp;
            std::string err;
            const int timeoutMs = (action == "open_tec_dialog" || action == "open_tutor_panel" || action == "run_dpi_cycle_live") ? 35000 : 25000;
            if (!ctx.guiManager.SendRequest(ipcReq, resp, instanceId, timeoutMs, &err)) {
                return McpToolResult::Err(err);
            }
            if (!resp.value("ok", false)) {
                return McpToolResult::Err(resp.value("error", "Accion GUI fallo: " + action), resp);
            }
            return McpToolResult::Ok(std::move(resp));
        }
    });

    // =========================================================================
    // 35. cuadernu_test — Automated Testing, Visual Diff, OCR QA, Benchmarks & Hygiene
    // =========================================================================
    router.RegisterTool({
        "cuadernu_test",
        "Suite de pruebas automatizadas y auditoria visual/QA de CuadernU: CTest (con MUTATION_MASK=0 forzado), "
        "smoke test, ciclo DPI, simulacion GPU reset, arbol de accesibilidad, layout bounds, diff visual pixel a pixel "
        "con mapa de calor PNG, asercion no-en-blanco, verificacion OCR offline, benchmark de render/RAM, higiene y sandbox reset.",
        MakeToolSchema(
            { "list_ctest_suites", "run_ctest", "run_smoke_test", "run_dpi_cycle", "run_gpu_reset_drill",
              "dump_a11y_tree", "dump_layout_bounds", "image_diff", "assert_visual_nonblank",
              "ocr_verify_capture", "benchmark_render", "run_hygiene_check", "sandbox_reset" },
            {
                { "filter_regex", { { "type", "string" } } },
                { "build_dir", { { "type", "string" } } },
                { "timeout_ms", { { "type", "integer" } } },
                { "instance_id", { { "type", "string" } } },
                { "session_id", { { "type", "string" } } },
                { "image_a", { { "type", "string" } } },
                { "image_b", { { "type", "string" } } },
                { "image_path", { { "type", "string" } } },
                { "heatmap_out_path", { { "type", "string" } } },
                { "include_image_block", { { "type", "boolean" } } },
                { "tolerance", { { "type", "integer" } } },
                { "min_nonwhite_ratio", { { "type", "number" } } },
                { "expected_substrings", { { "type", "array" }, { "items", { { "type", "string" } } } } },
                { "language_tag", { { "type", "string" } } },
                { "iterations", { { "type", "integer" } } },
                { "width_px", { { "type", "integer" } } },
                { "page_index", { { "type", "integer" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");

            if (action == "sandbox_reset") {
                std::string err;
                if (!ctx.ResetSandbox(&err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "ok", true }, { "sandbox_root", WideToUtf8(ctx.EffectiveProfileRootW()) } });
            }

            if (action == "list_ctest_suites" || action == "run_ctest") {
                const std::wstring buildDirW = args.value("build_dir", "").empty()
                    ? ctx.Options().buildDirW
                    : Utf8ToWide(args.value("build_dir", ""));
                if (buildDirW.empty() || buildDirW.find(L'"') != std::wstring::npos ||
                    buildDirW.find(L'\r') != std::wstring::npos || buildDirW.find(L'\n') != std::wstring::npos) {
                    return McpToolResult::Err("No se encontro build_dir valido con CTestTestfile.cmake.");
                }
                const std::wstring ctestExe = ResolveToolExe(L"D:\\DevTools\\CMake\\bin\\ctest.exe", L"ctest.exe");
                std::wstring cmd = L"\"" + ctestExe + L"\" --test-dir \"" + buildDirW + L"\" -C Release";
                if (action == "list_ctest_suites") {
                    cmd += L" -N";
                } else {
                    cmd += L" --output-on-failure";
                    std::string rx = args.value("filter_regex", "");
                    rx.erase(std::remove_if(rx.begin(), rx.end(), [](unsigned char ch) {
                        return ch == '"' || ch == '\r' || ch == '\n' || ch == '\0';
                    }), rx.end());
                    if (!rx.empty()) {
                        cmd += L" -R \"" + Utf8ToWide(rx) + L"\"";
                    }
                }
                int exitCode = -1;
                std::string output, err;
                const int timeoutMs = std::clamp(args.value("timeout_ms", 180000), 5000, 600000);
                // Always force MUTATION_MASK=0 for CTest runs!
                const bool ran = RunSubprocessCapture(cmd, buildDirW, timeoutMs, { { L"MUTATION_MASK", L"0" } }, exitCode, output, &err);
                if (!ran && exitCode != 0 && output.empty()) {
                    return McpToolResult::Err(err);
                }
                json res = {
                    { "ok", exitCode == 0 },
                    { "exit_code", exitCode },
                    { "build_dir", WideToUtf8(buildDirW) },
                    { "mutation_mask_enforced", "0" },
                    { "output", output }
                };
                if (action == "list_ctest_suites") {
                    json suites = json::array();
                    std::istringstream iss(output);
                    std::string line;
                    while (std::getline(iss, line)) {
                        const auto pos = line.find("Test #");
                        if (pos != std::string::npos) {
                            const auto colon = line.find(':', pos);
                            if (colon != std::string::npos) {
                                std::string name = line.substr(colon + 1);
                                while (!name.empty() && (name.front() == ' ' || name.front() == '\t')) name.erase(name.begin());
                                while (!name.empty() && (name.back() == '\r' || name.back() == '\n' || name.back() == ' ')) name.pop_back();
                                if (!name.empty()) suites.push_back(name);
                            }
                        }
                    }
                    res["suites"] = suites;
                    res["suite_count"] = suites.size();
                }
                return McpToolResult::Ok(std::move(res));
            }

            if (action == "run_smoke_test" || action == "run_dpi_cycle" || action == "run_gpu_reset_drill") {
                const std::wstring exeW = ctx.Options().cuadernuExePathW;
                if (exeW.empty()) return McpToolResult::Err("No se conoce la ruta de CuadernU.exe.");
                std::wstring cmd = L"\"" + exeW + L"\" --profile-dir \"" + ctx.EffectiveProfileRootW() + L"\" --no-profile-write";
                if (action == "run_smoke_test") cmd += L" --smoke-test";
                else if (action == "run_dpi_cycle") cmd += L" --test-dpi-cycle";
                else cmd += L" --simulate-gpu-reset --smoke-test";

                int exitCode = -1;
                std::string output, err;
                const int timeoutMs = std::clamp(args.value("timeout_ms", 60000), 5000, 300000);
                RunSubprocessCapture(cmd, L"", timeoutMs, { { L"MUTATION_MASK", L"0" } }, exitCode, output, &err);
                return McpToolResult::Ok({
                    { "ok", exitCode == 0 },
                    { "action", action },
                    { "exit_code", exitCode },
                    { "output", output },
                    { "error", err }
                });
            }

            if (action == "dump_a11y_tree" || action == "dump_layout_bounds") {
                std::string instanceId;
                bool autoSpawned = false;
                std::string err;
                if (!EnsureGuiReadyForCapture(ctx, args, instanceId, autoSpawned, err)) {
                    return McpToolResult::Err(err);
                }
                json resp;
                if (!ctx.guiManager.SendRequest({ { "action", action } }, resp, instanceId, 20000, &err)) {
                    return McpToolResult::Err(err);
                }
                if (!resp.value("ok", false)) {
                    return McpToolResult::Err(resp.value("error", action + " fallo en GUI."), resp);
                }
                resp["instance_id"] = instanceId;
                resp["auto_spawned_background_gui"] = autoSpawned;
                return McpToolResult::Ok(std::move(resp));
            }

            if (action == "image_diff") {
                const std::string pathA = args.value("image_a", "");
                const std::string pathB = args.value("image_b", "");
                if (pathA.empty() || pathB.empty()) {
                    return McpToolResult::Err("Se requieren image_a e image_b.");
                }
                std::vector<std::uint8_t> bytesA, bytesB;
                std::string err;
                if (!ctx.ReadBytesFromFile(Utf8ToWide(pathA), bytesA, &err)) return McpToolResult::Err("image_a: " + err);
                if (!ctx.ReadBytesFromFile(Utf8ToWide(pathB), bytesB, &err)) return McpToolResult::Err("image_b: " + err);

                RasterImageBgra32 imgA, imgB;
                if (!DecodePngBytesToBgra32(bytesA, imgA, &err)) return McpToolResult::Err("image_a decode: " + err);
                if (!DecodePngBytesToBgra32(bytesB, imgB, &err)) return McpToolResult::Err("image_b decode: " + err);

                const bool sameDims = (imgA.width == imgB.width && imgA.height == imgB.height);
                const int cmpW = (std::min)(imgA.width, imgB.width);
                const int cmpH = (std::min)(imgA.height, imgB.height);
                const int tol = std::clamp(args.value("tolerance", 4), 0, 255);

                RasterImageBgra32 heatmap;
                heatmap.width = cmpW;
                heatmap.height = cmpH;
                heatmap.pixels.resize(static_cast<std::size_t>(cmpW) * static_cast<std::size_t>(cmpH) * 4u, 255u);

                std::uint64_t mismatchPixels = 0;
                double sumAbs = 0.0;
                double sumSq = 0.0;
                const std::uint64_t totalCompared = static_cast<std::uint64_t>(cmpW) * static_cast<std::uint64_t>(cmpH);

                for (int y = 0; y < cmpH; ++y) {
                    for (int x = 0; x < cmpW; ++x) {
                        const std::size_t idxA = (static_cast<std::size_t>(y) * imgA.width + x) * 4u;
                        const std::size_t idxB = (static_cast<std::size_t>(y) * imgB.width + x) * 4u;
                        const std::size_t idxH = (static_cast<std::size_t>(y) * cmpW + x) * 4u;

                        const int db = std::abs(static_cast<int>(imgA.pixels[idxA + 0]) - static_cast<int>(imgB.pixels[idxB + 0]));
                        const int dg = std::abs(static_cast<int>(imgA.pixels[idxA + 1]) - static_cast<int>(imgB.pixels[idxB + 1]));
                        const int dr = std::abs(static_cast<int>(imgA.pixels[idxA + 2]) - static_cast<int>(imgB.pixels[idxB + 2]));
                        const int maxCh = (std::max)(dr, (std::max)(dg, db));
                        const double meanCh = (dr + dg + db) / 3.0;
                        sumAbs += meanCh;
                        sumSq += (dr * dr + dg * dg + db * db) / 3.0;

                        if (maxCh > tol) {
                            ++mismatchPixels;
                            // Bright red highlight scaled by difference magnitude
                            heatmap.pixels[idxH + 0] = 32u;
                            heatmap.pixels[idxH + 1] = static_cast<std::uint8_t>((std::max)(20, 120 - maxCh / 2));
                            heatmap.pixels[idxH + 2] = 245u;
                            heatmap.pixels[idxH + 3] = 255u;
                        } else {
                            // Subtle dimmed grayscale context from image B
                            const std::uint8_t lum = static_cast<std::uint8_t>(
                                (imgB.pixels[idxB + 2] * 30 + imgB.pixels[idxB + 1] * 59 + imgB.pixels[idxB + 0] * 11) / 100);
                            const std::uint8_t muted = static_cast<std::uint8_t>(180 + (lum / 4));
                            heatmap.pixels[idxH + 0] = muted;
                            heatmap.pixels[idxH + 1] = muted;
                            heatmap.pixels[idxH + 2] = muted;
                            heatmap.pixels[idxH + 3] = 255u;
                        }
                    }
                }

                const double mismatchPct = (totalCompared > 0)
                    ? (100.0 * static_cast<double>(mismatchPixels) / static_cast<double>(totalCompared))
                    : 0.0;
                const double mae = (totalCompared > 0) ? (sumAbs / static_cast<double>(totalCompared)) : 0.0;
                const double rmse = (totalCompared > 0) ? std::sqrt(sumSq / static_cast<double>(totalCompared)) : 0.0;

                std::vector<std::uint8_t> heatPng;
                std::wstring heatPathW;
                if (WinRtPdfRenderer::EncodeBgra32ToPngBytes(heatmap, heatPng) && !heatPng.empty()) {
                    heatPathW = ctx.ResolveArtifactPathW(args.value("heatmap_out_path", ""), "image_diff_heatmap", ".png");
                    ctx.WriteBytesToFile(heatPathW, heatPng, nullptr);
                }

                json payload = {
                    { "ok", true },
                    { "identical", sameDims && mismatchPixels == 0 },
                    { "same_dimensions", sameDims },
                    { "dims_a", { imgA.width, imgA.height } },
                    { "dims_b", { imgB.width, imgB.height } },
                    { "tolerance", tol },
                    { "total_pixels_compared", totalCompared },
                    { "mismatch_pixels", mismatchPixels },
                    { "mismatch_percent", mismatchPct },
                    { "mae", mae },
                    { "rmse", rmse },
                    { "heatmap_path", WideToUtf8(heatPathW) }
                };
                if (args.value("include_image_block", false) && !heatPng.empty()) {
                    return McpToolResult::OkWithImage(std::move(payload), std::move(heatPng), "image/png");
                }
                return McpToolResult::Ok(std::move(payload));
            }

            if (action == "assert_visual_nonblank") {
                const std::string imgPath = args.value("image_path", "");
                if (imgPath.empty()) return McpToolResult::Err("Se requiere image_path.");
                std::vector<std::uint8_t> bytes;
                std::string err;
                if (!ctx.ReadBytesFromFile(Utf8ToWide(imgPath), bytes, &err)) return McpToolResult::Err(err);
                RasterImageBgra32 img;
                if (!DecodePngBytesToBgra32(bytes, img, &err)) return McpToolResult::Err(err);

                const std::uint64_t total = static_cast<std::uint64_t>(img.width) * static_cast<std::uint64_t>(img.height);
                std::uint64_t nonWhite = 0;
                std::uint64_t nonBlack = 0;
                double sumLum = 0.0;
                double sumLumSq = 0.0;
                std::set<std::uint16_t> quantizedColors;

                int minX = img.width, minY = img.height, maxX = -1, maxY = -1;
                // Sample corner pixel as dominant background heuristic
                const std::uint8_t bgB = img.pixels[0], bgG = img.pixels[1], bgR = img.pixels[2];
                std::uint64_t diffFromCorner = 0;

                for (int y = 0; y < img.height; ++y) {
                    for (int x = 0; x < img.width; ++x) {
                        const std::size_t idx = (static_cast<std::size_t>(y) * img.width + x) * 4u;
                        const std::uint8_t b = img.pixels[idx + 0];
                        const std::uint8_t g = img.pixels[idx + 1];
                        const std::uint8_t r = img.pixels[idx + 2];
                        if (r < 246 || g < 246 || b < 246) ++nonWhite;
                        if (r > 10 || g > 10 || b > 10) ++nonBlack;

                        const int dCorner = (std::max)(std::abs(int(r) - int(bgR)),
                            (std::max)(std::abs(int(g) - int(bgG)), std::abs(int(b) - int(bgB))));
                        if (dCorner > 12) {
                            ++diffFromCorner;
                            minX = (std::min)(minX, x);
                            minY = (std::min)(minY, y);
                            maxX = (std::max)(maxX, x);
                            maxY = (std::max)(maxY, y);
                        }

                        const double lum = 0.299 * r + 0.587 * g + 0.114 * b;
                        sumLum += lum;
                        sumLumSq += lum * lum;
                        if ((x & 3) == 0 && (y & 3) == 0) {
                            const std::uint16_t q = static_cast<std::uint16_t>(((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4));
                            quantizedColors.insert(q);
                        }
                    }
                }

                const double meanLum = total ? (sumLum / static_cast<double>(total)) : 0.0;
                const double varLum = total ? (std::max)(0.0, (sumLumSq / static_cast<double>(total)) - meanLum * meanLum) : 0.0;
                const double stdDevLum = std::sqrt(varLum);
                const double nonWhiteRatio = total ? (static_cast<double>(nonWhite) / static_cast<double>(total)) : 0.0;
                const double nonBlackRatio = total ? (static_cast<double>(nonBlack) / static_cast<double>(total)) : 0.0;
                const double contentRatio = total ? (static_cast<double>(diffFromCorner) / static_cast<double>(total)) : 0.0;
                const double minRatio = args.value("min_nonwhite_ratio", 0.002);
                const bool nonblank = (contentRatio >= minRatio) && (quantizedColors.size() >= 2u) && (stdDevLum > 1.5);

                return McpToolResult::Ok({
                    { "ok", true },
                    { "nonblank", nonblank },
                    { "width_px", img.width },
                    { "height_px", img.height },
                    { "nonwhite_ratio", nonWhiteRatio },
                    { "nonblack_ratio", nonBlackRatio },
                    { "content_diff_ratio", contentRatio },
                    { "luminance_mean", meanLum },
                    { "luminance_stddev", stdDevLum },
                    { "unique_quantized_colors", quantizedColors.size() },
                    { "content_bbox", maxX >= minX ? json({ { "x", minX }, { "y", minY }, { "w", maxX - minX + 1 }, { "h", maxY - minY + 1 } }) : json(nullptr) }
                });
            }

            if (action == "ocr_verify_capture") {
                const std::string imgPath = args.value("image_path", "");
                if (imgPath.empty()) return McpToolResult::Err("Se requiere image_path.");
                std::vector<std::uint8_t> bytes;
                std::string err;
                if (!ctx.ReadBytesFromFile(Utf8ToWide(imgPath), bytes, &err)) return McpToolResult::Err(err);
                RasterImageBgra32 img;
                if (!DecodePngBytesToBgra32(bytes, img, &err)) return McpToolResult::Err(err);

                const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(
                    img.width, img.height, img.pixels.data(), args.value("language_tag", ""));
                const std::string lowerText = ToLowerAscii(ocr.fullTextUtf8);

                json matched = json::array();
                json missing = json::array();
                if (args.contains("expected_substrings") && args["expected_substrings"].is_array()) {
                    for (const auto& item : args["expected_substrings"]) {
                        if (!item.is_string()) continue;
                        const std::string sub = item.get<std::string>();
                        if (lowerText.find(ToLowerAscii(sub)) != std::string::npos) {
                            matched.push_back(sub);
                        } else {
                            missing.push_back(sub);
                        }
                    }
                }
                const bool passed = ocr.engineAvailable && missing.empty();
                return McpToolResult::Ok({
                    { "ok", true },
                    { "passed", passed },
                    { "engine_available", ocr.engineAvailable },
                    { "language_tag", ocr.languageTagUtf8 },
                    { "full_text", ocr.fullTextUtf8 },
                    { "line_count", ocr.linesUtf8.size() },
                    { "word_count", ocr.words.size() },
                    { "matched_substrings", matched },
                    { "missing_substrings", missing },
                    { "ocr_error", ocr.errorUtf8 }
                });
            }

            if (action == "benchmark_render") {
                auto sess = ctx.GetOrCreateSession(args);
                const int iters = std::clamp(args.value("iterations", 5), 1, 50);
                const int widthPx = std::clamp(args.value("width_px", 1280), 256, 3840);
                const std::size_t pgIdx = args.contains("page_index")
                    ? static_cast<std::size_t>((std::max)(0, args.value("page_index", 0)))
                    : sess->activePageIndex;

                double sumMs = 0.0, minMs = 1e9, maxMs = 0.0;
                std::size_t lastPngBytes = 0;
                for (int i = 0; i < iters; ++i) {
                    const auto t0 = std::chrono::steady_clock::now();
                    std::vector<std::uint8_t> png;
                    std::string err;
                    if (!ctx.RenderPagePng(*sess, pgIdx, widthPx, png, &err)) {
                        return McpToolResult::Err(err);
                    }
                    const auto t1 = std::chrono::steady_clock::now();
                    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                    sumMs += ms;
                    minMs = (std::min)(minMs, ms);
                    maxMs = (std::max)(maxMs, ms);
                    lastPngBytes = png.size();
                }

                double ramMb = 0.0;
#ifdef _WIN32
                PROCESS_MEMORY_COUNTERS pmc{};
                if (K32GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                    ramMb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
                }
#endif
                return McpToolResult::Ok({
                    { "ok", true },
                    { "session_id", sess->sessionId },
                    { "iterations", iters },
                    { "width_px", widthPx },
                    { "avg_ms", sumMs / static_cast<double>(iters) },
                    { "min_ms", minMs },
                    { "max_ms", maxMs },
                    { "last_png_bytes", lastPngBytes },
                    { "process_working_set_mb", ramMb }
                });
            }

            if (action == "run_hygiene_check") {
                const std::wstring repoW = ctx.Options().repoRootW;
                if (repoW.empty()) return McpToolResult::Err("No se detecto la raiz del repositorio.");
                const fs::path script = fs::path(repoW) / L"tools" / L"check_workspace_hygiene.py";
                std::error_code ec;
                if (!fs::exists(script, ec)) {
                    return McpToolResult::Err("No se encontro tools/check_workspace_hygiene.py.");
                }
                const std::wstring pyExe = ResolveToolExe(L"D:\\DevTools\\Python311\\python.exe", L"python.exe");
                const std::wstring cmd = L"\"" + pyExe + L"\" \"" + script.wstring() + L"\"";
                int exitCode = -1;
                std::string output, err;
                RunSubprocessCapture(cmd, repoW, 30000, {}, exitCode, output, &err);
                return McpToolResult::Ok({
                    { "ok", exitCode == 0 },
                    { "exit_code", exitCode },
                    { "output", output },
                    { "error", err }
                });
            }

            return McpToolResult::Err("Accion desconocida en cuadernu_test: " + action);
        }
    });
}

} // namespace DocStudio::Mcp
