#pragma once
// ============================================================================
// doc_studio/app/studio_gemini_notebook_dialog.hpp
// Native Win32 guías de interfaz Dialogs for the Optional, Decoupled Google Gemini
// Notebook Integration (Account Connect, Sync Hub, TEC Course Dry-Run, and
// Agent MCP Configuration). Zero GPU, Zero idle timers, PerMonitorV2 4K.
// ============================================================================

#ifdef _WIN32

#include "../integrations/gemini_notebook/gemini_notebook_client.hpp"
#include "../integrations/gemini_notebook/gemini_notebook_sync_store.hpp"
#include "../mcp/mcp_server.hpp"
#include "../core/document_model.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/winrt_adapters.hpp"
#include "studio_feature_dialogs.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace DocStudio::GeminiNotebookUi {

inline UINT DpiOf(HWND hwnd) noexcept {
    if (HMODULE hU32 = GetModuleHandleW(L"user32.dll")) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        if (auto fn = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(hU32, "GetDpiForWindow"))) {
            const UINT d = hwnd ? fn(hwnd) : 96u;
            if (d >= 72u && d <= 480u) return d;
        }
    }
    return 96u;
}

inline int Sc(int px, UINT dpi) noexcept {
    return MulDiv(px, static_cast<int>(dpi), 96);
}

inline std::wstring GetEditTextW(HWND hEdit) {
    if (!hEdit) return {};
    const int len = GetWindowTextLengthW(hEdit);
    if (len <= 0) return {};
    std::wstring buf(static_cast<std::size_t>(len) + 1u, L'\0');
    GetWindowTextW(hEdit, buf.data(), len + 1);
    buf.resize(static_cast<std::size_t>(len));
    return buf;
}

inline void CopyUtf8ToClipboard(HWND owner, const std::string& utf8) {
    const std::wstring w = Utf8ToWide(utf8);
    if (!OpenClipboard(owner)) return;
    EmptyClipboard();
    const std::size_t bytes = (w.size() + 1u) * sizeof(wchar_t);
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (hg) {
        void* dst = GlobalLock(hg);
        if (dst) {
            std::memcpy(dst, w.c_str(), bytes);
            GlobalUnlock(hg);
            SetClipboardData(CF_UNICODETEXT, hg);
        } else {
            GlobalFree(hg);
        }
    }
    CloseClipboard();
}

inline bool PickFolderWin32(HWND owner, const wchar_t* title, std::wstring& outFolder) {
    outFolder.clear();
    Microsoft::WRL::ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || !dlg) return false;
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    if (title) dlg->SetTitle(title);
    if (FAILED(dlg->Show(owner))) return false;
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (SUCCEEDED(dlg->GetResult(&item)) && item) {
        PWSTR p = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
            outFolder = p;
            CoTaskMemFree(p);
        }
    }
    return !outFolder.empty();
}

inline bool CaptureDialogStealthPng(HWND dlg, const wchar_t* outPathW) {
    if (!dlg || !IsWindow(dlg) || !outPathW || !outPathW[0]) return false;
    UpdateWindow(dlg);
    RedrawWindow(dlg, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    MSG pmsg{};
    for (int i = 0; i < 64 && PeekMessageW(&pmsg, nullptr, 0, 0, PM_REMOVE); ++i) {
        TranslateMessage(&pmsg);
        DispatchMessageW(&pmsg);
    }
    RedrawWindow(dlg, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
    RECT rc{};
    GetWindowRect(dlg, &rc);
    const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
    const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
    HDC hdcScr = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScr);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hbmp = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    bool ok = false;
    if (hbmp && bits) {
        HGDIOBJ old = SelectObject(hdcMem, hbmp);
        // Use software GDI PrintWindow(0) first so cloaked Win32 child controls
        // (STATIC, BUTTON, EDIT, LISTBOX) render immediately without waiting for DWM.
        if (!PrintWindow(dlg, hdcMem, 0)) {
            PrintWindow(dlg, hdcMem, PW_RENDERFULLCONTENT);
        }
        // Explicitly composite visible child controls via WM_PRINT so themed Win32 controls always appear crisp
        POINT dlgOrigin{ rc.left, rc.top };
        struct ChildPrintCtx {
            HDC hdc;
            POINT parentScreenOrigin;
        } cpCtx{ hdcMem, dlgOrigin };
        EnumChildWindows(dlg, [](HWND hChild, LPARAM lp) -> BOOL {
            if (!IsWindowVisible(hChild)) return TRUE;
            auto* c = reinterpret_cast<ChildPrintCtx*>(lp);
            RECT crc{};
            if (!GetWindowRect(hChild, &crc)) return TRUE;
            const int dx = crc.left - c->parentScreenOrigin.x;
            const int dy = crc.top - c->parentScreenOrigin.y;
            const int saved = SaveDC(c->hdc);
            SetViewportOrgEx(c->hdc, dx, dy, nullptr);
            SendMessageW(hChild, WM_PRINT, reinterpret_cast<WPARAM>(c->hdc),
                PRF_NONCLIENT | PRF_CLIENT | PRF_ERASEBKGND | PRF_CHILDREN);
            RestoreDC(c->hdc, saved);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&cpCtx));
        GdiFlush();
        RasterImageBgra32 img;
        img.width = w;
        img.height = h;
        img.pixels.resize(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u);
        std::memcpy(img.pixels.data(), bits, img.pixels.size());
        for (std::size_t i = 3; i < img.pixels.size(); i += 4) img.pixels[i] = 255u;
        std::vector<std::uint8_t> png;
        if (WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, png) && !png.empty()) {
            std::error_code dirEc;
            const std::filesystem::path outP(outPathW);
            if (outP.has_parent_path()) std::filesystem::create_directories(outP.parent_path(), dirEc);
            HANDLE hf = CreateFileW(outPathW, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (hf != INVALID_HANDLE_VALUE) {
                DWORD wr = 0;
                ok = WriteFile(hf, png.data(), static_cast<DWORD>(png.size()), &wr, nullptr) && wr == png.size();
                CloseHandle(hf);
            }
        }
        SelectObject(hdcMem, old);
        DeleteObject(hbmp);
    }
    if (hdcMem) DeleteDC(hdcMem);
    if (hdcScr) ReleaseDC(nullptr, hdcScr);
    return ok;
}

inline void ApplyStealthCloakToDialog(HWND dlg) {
    if (!dlg) return;
    if (HMODULE hDwm = LoadLibraryW(L"dwmapi.dll")) {
        using DwmSetAttrFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
        if (auto fn = reinterpret_cast<DwmSetAttrFn>(GetProcAddress(hDwm, "DwmSetWindowAttribute"))) {
            BOOL cloak = TRUE;
            fn(dlg, 13 /*DWMWA_CLOAK*/, &cloak, sizeof(cloak));
        }
        FreeLibrary(hDwm);
    }
}

// ============================================================================
// 1. Agent MCP Configuration & Roadmap Status Dialog
// ============================================================================
inline void ShowGeminiNotebookMcpConfigDialog(
    HWND owner,
    const GeminiNotebook::GeminiNotebookConfig& cfg,
    const GeminiNotebook::GeminiNotebookCapabilities* caps = nullptr,
    const wchar_t* autoScreenshotPathW = nullptr,
    bool backgroundStealth = false,
    const UiGlass::GlassSettings* overrideGlass = nullptr)
{
    wchar_t exeBuf[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exeBuf, MAX_PATH);
    const std::string snippet = Mcp::McpServer::GenerateAgentMcpConfigJson(exeBuf, cfg.mcpEndpointUrlUtf8);

    std::string editSnippet;
    editSnippet.reserve(snippet.size() + 32u);
    for (std::size_t i = 0; i < snippet.size(); ++i) {
        if (snippet[i] == '\n' && (i == 0 || snippet[i - 1] != '\r')) editSnippet.push_back('\r');
        editSnippet.push_back(snippet[i]);
    }

    struct St {
        std::string jsonSnippet;
        std::string editSnippet;
        GeminiNotebook::GeminiNotebookCapabilities capInfo{};
        bool done = false;
        UINT dpi = 96;
        const UiGlass::GlassSettings* overrideGlass = nullptr;
        UiGlass::HigDialogTheme theme{};
        HFONT fNorm = nullptr, fBold = nullptr, fMono = nullptr;
        HWND hCode = nullptr;
    } st{};
    st.jsonSnippet = snippet;
    st.editSnippet = std::move(editSnippet);
    if (caps) st.capInfo = *caps;
    st.dpi = DpiOf(owner);
    st.overrideGlass = overrideGlass;

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            if (msg == WM_CREATE) {
                s->theme.Init(hwnd, s->overrideGlass);
                s->fNorm = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_NORMAL);
                s->fBold = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_SEMIBOLD);
                s->fMono = CreateFontW(-Sc(12, s->dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
                const int pad = Sc(16, s->dpi);
                const int fw = Sc(560, s->dpi);
                int y = Sc(14, s->dpi);

                HWND hT = CreateWindowExW(0, L"STATIC",
                    L"Conexi\u00F3n para agentes de IA (Antigravity, Codex u otros clientes compatibles):",
                    WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(20, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hT, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                y += Sc(24, s->dpi);

                HWND hSub = CreateWindowExW(0, L"STATIC",
                    L"Incluye el servidor completo de CuadernU (cuadernos, dibujo, capturas en segundo plano, TEC Digital y estudio) y el servidor oficial de Gemini Notebook.",
                    WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(40, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hSub, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(46, s->dpi);

                s->hCode = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->editSnippet).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                    pad, y, fw, Sc(210, s->dpi), hwnd, reinterpret_cast<HMENU>(801), nullptr, nullptr);
                SendMessageW(s->hCode, WM_SETFONT, reinterpret_cast<WPARAM>(s->fMono ? s->fMono : s->fNorm), TRUE);
                y += Sc(220, s->dpi);

                const std::wstring capTxt =
                    L"Capacidades oficiales hoy: crear/listar cuadernos, listar fuentes, enlaces web/YouTube y carga de archivos.\r\n"
                    L"En hoja de ruta de Google: consultas remotas (" +
                    std::wstring(s->capInfo.queryNotebookApi ? L"disponible" : L"pendiente") +
                    L"), artefactos (" +
                    std::wstring(s->capInfo.generateArtifactsApi ? L"disponible" : L"pendiente") +
                    L") y borrado remoto (" +
                    std::wstring(s->capInfo.deleteNotebookOrSourceApi ? L"disponible" : L"pendiente") + L").";
                HWND hCap = CreateWindowExW(0, L"STATIC", capTxt.c_str(),
                    WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(78, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hCap, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(86, s->dpi);

                HWND bCopy = CreateWindowExW(0, L"BUTTON", L"Copiar configuraci\u00F3n",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    pad, y, Sc(168, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(802), nullptr, nullptr);
                HWND bClose = CreateWindowExW(0, L"BUTTON", L"Cerrar",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    pad + fw - Sc(96, s->dpi), y, Sc(96, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                SendMessageW(bCopy, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bClose, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                s->theme.ThemeAllChildren(hwnd);
                return 0;
            }
            {
                LRESULT tr = 0;
                if (s->theme.HandleMessage(hwnd, msg, wp, lp, tr)) return tr;
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == 802) {
                    CopyUtf8ToClipboard(hwnd, s->jsonSnippet);
                    MessageBoxW(hwnd, L"Configuraci\u00F3n copiada al portapapeles.", L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL || LOWORD(wp) == IDOK) {
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->fMono) DeleteObject(s->fMono);
                s->theme.Destroy();
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernU_GnMcpCfgDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    const int w = Sc(608, st.dpi);
    const int h = Sc(494, st.dpi);
    RECT orc{};
    GetWindowRect(owner, &orc);
    const int x = (orc.left + orc.right - w) / 2;
    const int y = (orc.top + orc.bottom - h) / 2;
    if (owner && !backgroundStealth) EnableWindow(owner, FALSE);
    const DWORD exSt = backgroundStealth ? (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE) : WS_EX_DLGMODALFRAME;
    const DWORD wSt = backgroundStealth ? (WS_POPUP | WS_CAPTION | WS_SYSMENU) : (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
    HWND dlg = CreateWindowExW(exSt, L"CuadernU_GnMcpCfgDlg",
        L"Puente de Agentes — CuadernU y Gemini Notebook", wSt,
        x, y, w, h, backgroundStealth ? nullptr : owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (backgroundStealth && dlg) {
        ApplyStealthCloakToDialog(dlg);
        ShowWindow(dlg, SW_SHOWNOACTIVATE);
    }
    if (autoScreenshotPathW && autoScreenshotPathW[0] && dlg) {
        CaptureDialogStealthPng(dlg, autoScreenshotPathW);
        DestroyWindow(dlg);
        if (owner && !backgroundStealth) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        return;
    }
    MSG msg{};
    while (!st.done && IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessageW(dlg, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner && !backgroundStealth) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}

// ============================================================================
// 2. Connect Personal Google Account Dialog (OAuth Browser + Token/ADC)
// ============================================================================
inline bool ShowGeminiNotebookConnectDialog(
    HWND owner,
    GeminiNotebook::GeminiNotebookSyncStore& store,
    const wchar_t* autoScreenshotPathW = nullptr,
    bool backgroundStealth = false,
    const UiGlass::GlassSettings* overrideGlass = nullptr) {
    struct St {
        GeminiNotebook::GeminiNotebookSyncStore* store = nullptr;
        bool changed = false, done = false;
        UINT dpi = 96;
        const UiGlass::GlassSettings* overrideGlass = nullptr;
        UiGlass::HigDialogTheme theme{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HWND hProj = nullptr, hClientId = nullptr, hClientSec = nullptr, hToken = nullptr, hEmail = nullptr;
    } st{};
    st.store = &store;
    st.dpi = DpiOf(owner);
    st.overrideGlass = overrideGlass;

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            if (msg == WM_CREATE) {
                s->theme.Init(hwnd, s->overrideGlass);
                s->fNorm = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_NORMAL);
                s->fBold = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_SEMIBOLD);
                const int pad = Sc(16, s->dpi);
                const int fw = Sc(500, s->dpi);
                int y = Sc(14, s->dpi);

                auto addLbl = [&](const wchar_t* t, bool bold = true) {
                    HWND h = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                    y += Sc(20, s->dpi);
                };

                HWND hNote = CreateWindowExW(0, L"STATIC",
                    L"Usa una cuenta personal de Google. Durante la versi\u00F3n preliminar las cuentas institucionales Workspace (como la del TEC) no est\u00E1n admitidas por Google.",
                    WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(34, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hNote, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(40, s->dpi);

                addLbl(L"ID de proyecto de Google Cloud (opcional):");
                s->hProj = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->store->Config().projectIdUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(821), nullptr, nullptr);
                SendMessageW(s->hProj, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"ID de cliente de escritorio (para iniciar sesi\u00F3n por navegador):");
                s->hClientId = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->store->Config().oauthClientIdUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(822), nullptr, nullptr);
                SendMessageW(s->hClientId, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"Secreto de cliente (opcional, guardado cifrado localmente):");
                s->hClientSec = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->store->Config().oauthClientSecretUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(823), nullptr, nullptr);
                SendMessageW(s->hClientSec, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(36, s->dpi);

                HWND bBrowser = CreateWindowExW(0, L"BUTTON", L"Iniciar sesi\u00F3n con Google en el navegador",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    pad, y, Sc(290, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(826), nullptr, nullptr);
                HWND bGcloud = CreateWindowExW(0, L"BUTTON", L"Importar sesi\u00F3n gcloud",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    pad + Sc(300, s->dpi), y, Sc(200, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(827), nullptr, nullptr);
                SendMessageW(bBrowser, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bGcloud, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(42, s->dpi);

                addLbl(L"O pegar token de acceso directo (guardado cifrado localmente):");
                s->hToken = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(824), nullptr, nullptr);
                SendMessageW(s->hToken, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(34, s->dpi);

                addLbl(L"Etiqueta de cuenta personal (ej. micorreo@gmail.com, opcional):", false);
                s->hEmail = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->store->Tokens().accountEmailUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, pad, y, fw, Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(825), nullptr, nullptr);
                SendMessageW(s->hEmail, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(40, s->dpi);

                HWND bClear = CreateWindowExW(0, L"BUTTON", L"Cerrar sesi\u00F3n",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP, pad, y, Sc(122, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(828), nullptr, nullptr);
                HWND bSave = CreateWindowExW(0, L"BUTTON", L"Guardar",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP, pad + fw - Sc(204, s->dpi), y, Sc(96, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                HWND bCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP, pad + fw - Sc(98, s->dpi), y, Sc(98, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                SendMessageW(bClear, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bSave, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(bCancel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                s->theme.ThemeAllChildren(hwnd);
                return 0;
            }
            {
                LRESULT tr = 0;
                if (s->theme.HandleMessage(hwnd, msg, wp, lp, tr)) return tr;
            }
            if (msg == WM_COMMAND) {
                const WORD id = LOWORD(wp);
                auto syncInputs = [&]() {
                    s->store->MutableConfig().projectIdUtf8 = WideToUtf8(GetEditTextW(s->hProj));
                    s->store->MutableConfig().oauthClientIdUtf8 = WideToUtf8(GetEditTextW(s->hClientId));
                    s->store->MutableConfig().oauthClientSecretUtf8 = WideToUtf8(GetEditTextW(s->hClientSec));
                    s->store->MutableTokens().oauthClientSecretUtf8 = s->store->Config().oauthClientSecretUtf8;
                    const std::string em = WideToUtf8(GetEditTextW(s->hEmail));
                    if (!em.empty()) s->store->MutableTokens().accountEmailUtf8 = em;
                };
                if (id == 826) {
                    syncInputs();
                    GeminiNotebook::WinHttpNotebookTransport tr;
                    GeminiNotebook::GeminiNotebookAuthTokens tok = s->store->Tokens();
                    std::string err;
                    if (GeminiNotebook::GoogleOAuthPkceHelper::RunLoopbackAuthorization(s->store->Config(), tr, tok, 120, &err)) {
                        s->store->MutableTokens() = std::move(tok);
                        s->store->SaveConfig(nullptr);
                        s->store->SaveTokensDpapi(nullptr);
                        s->changed = true;
                        MessageBoxW(hwnd, L"Cuenta personal conectada y credenciales protegidas localmente.", L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                        s->done = true;
                        DestroyWindow(hwnd);
                    } else {
                        MessageBoxW(hwnd, Utf8ToWide(err.empty() ? "No se pudo completar el inicio de sesion." : err).c_str(),
                            L"Gemini Notebook", MB_OK | MB_ICONWARNING);
                    }
                    return 0;
                }
                if (id == 827) {
                    std::string adcTok, err;
                    if (GeminiNotebook::GoogleOAuthPkceHelper::TryImportGcloudAdcToken(adcTok, &err)) {
                        SetWindowTextW(s->hToken, Utf8ToWide(adcTok).c_str());
                        MessageBoxW(hwnd, L"Token importado desde la sesi\u00F3n local de gcloud. Pulsa Guardar para aplicarlo.", L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Gemini Notebook", MB_OK | MB_ICONWARNING);
                    }
                    return 0;
                }
                if (id == 828) {
                    s->store->ClearTokens(nullptr);
                    s->changed = true;
                    SetWindowTextW(s->hToken, L"");
                    MessageBoxW(hwnd, L"Sesi\u00F3n de Gemini Notebook cerrada localmente (tus archivos locales no se eliminaron).", L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                    return 0;
                }
                if (id == IDOK) {
                    syncInputs();
                    const std::string manualTok = WideToUtf8(GetEditTextW(s->hToken));
                    if (!manualTok.empty()) {
                        s->store->MutableTokens().accessTokenUtf8 = manualTok;
                        s->store->MutableTokens().expiresAtEpochSec = 0;
                        s->store->MutableTokens().workspaceRejectionSeen = false;
                    }
                    s->store->SaveConfig(nullptr);
                    if (s->store->Tokens().HasAnyCredential() || !s->store->Config().oauthClientSecretUtf8.empty()) {
                        s->store->SaveTokensDpapi(nullptr);
                    }
                    s->changed = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (id == IDCANCEL) {
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                s->theme.Destroy();
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernU_GnConnectDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    const int w = Sc(548, st.dpi);
    const int h = Sc(462, st.dpi);
    RECT orc{};
    GetWindowRect(owner, &orc);
    const int x = (orc.left + orc.right - w) / 2;
    const int y = (orc.top + orc.bottom - h) / 2;
    if (owner && !backgroundStealth) EnableWindow(owner, FALSE);
    const DWORD exSt = backgroundStealth ? (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE) : WS_EX_DLGMODALFRAME;
    const DWORD wSt = backgroundStealth ? (WS_POPUP | WS_CAPTION | WS_SYSMENU) : (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
    HWND dlg = CreateWindowExW(exSt, L"CuadernU_GnConnectDlg",
        L"Conectar cuenta personal — Gemini Notebook", wSt,
        x, y, w, h, backgroundStealth ? nullptr : owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (backgroundStealth && dlg) {
        ApplyStealthCloakToDialog(dlg);
        ShowWindow(dlg, SW_SHOWNOACTIVATE);
    }
    if (autoScreenshotPathW && autoScreenshotPathW[0] && dlg) {
        CaptureDialogStealthPng(dlg, autoScreenshotPathW);
        DestroyWindow(dlg);
        if (owner && !backgroundStealth) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        return false;
    }
    MSG msg{};
    while (!st.done && IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessageW(dlg, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner && !backgroundStealth) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    return st.changed;
}

// ============================================================================
// 3. Main Gemini Notebook Sync Hub Dialog (Offline-First + TEC Course Pipeline)
// ============================================================================
inline void ShowGeminiNotebookHubDialog(
    HWND owner,
    GeminiNotebook::GeminiNotebookSyncStore& store,
    const DocumentModel* openDoc,
    std::string& outStatusBannerUtf8,
    bool* outRequestAiStudy = nullptr,
    const wchar_t* autoScreenshotPathW = nullptr,
    bool backgroundStealth = false,
    const UiGlass::GlassSettings* overrideGlass = nullptr)
{
    if (outRequestAiStudy) *outRequestAiStudy = false;
    store.LoadAll(nullptr);

    struct St {
        GeminiNotebook::GeminiNotebookSyncStore* store = nullptr;
        const DocumentModel* openDoc = nullptr;
        std::string* statusOut = nullptr;
        bool* reqAiStudy = nullptr;
        GeminiNotebook::GeminiNotebookCapabilities caps{};
        bool done = false;
        UINT dpi = 96;
        const UiGlass::GlassSettings* overrideGlass = nullptr;
        UiGlass::HigDialogTheme theme{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HWND hBanner = nullptr, hNbList = nullptr, hSrcList = nullptr, hBundleChk = nullptr, hUrlEdit = nullptr;
    } st{};
    st.store = &store;
    st.openDoc = openDoc;
    st.statusOut = &outStatusBannerUtf8;
    st.reqAiStudy = outRequestAiStudy;
    st.dpi = DpiOf(owner);
    st.overrideGlass = overrideGlass;

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto refreshUi = [&]() {
                if (!s->hBanner || !s->hNbList || !s->hSrcList) return;
                const auto& tok = s->store->Tokens();
                std::wstring banner;
                if (tok.workspaceRejectionSeen) {
                    banner = L"Aviso: cuenta institucional Workspace no admitida en vista previa — usa una cuenta personal de Google.";
                } else if (tok.HasAnyCredential()) {
                    banner = L"Cuenta conectada" +
                        (tok.accountEmailUtf8.empty() ? std::wstring(L" (personal)") : (L": " + Utf8ToWide(tok.accountEmailUtf8))) +
                        L"  \u00B7  Cuadernos: " + std::to_wstring(s->store->NotebookMappings().size()) +
                        L"  \u00B7  Fuentes: " + std::to_wstring(s->store->SyncedSources().size());
                } else {
                    banner = L"Modo local independiente  \u00B7  Conecta tu cuenta personal de Google para sincronizar con la nube.";
                }
                SetWindowTextW(s->hBanner, banner.c_str());

                const int prevSel = static_cast<int>(SendMessageW(s->hNbList, LB_GETCURSEL, 0, 0));
                SendMessageW(s->hNbList, LB_RESETCONTENT, 0, 0);
                for (const auto& nb : s->store->NotebookMappings()) {
                    const auto srcs = s->store->SourcesForNotebook(nb.localKeyUtf8);
                    const std::wstring line = Utf8ToWide(nb.displayNameUtf8) + L"  (" +
                        std::to_wstring(srcs.size()) + L"/" + std::to_wstring(s->store->Config().recommendedMaxSourcesPerNotebook) + L")";
                    SendMessageW(s->hNbList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
                }
                if (!s->store->NotebookMappings().empty()) {
                    const int sel = (prevSel >= 0 && prevSel < static_cast<int>(s->store->NotebookMappings().size())) ? prevSel : 0;
                    SendMessageW(s->hNbList, LB_SETCURSEL, static_cast<WPARAM>(sel), 0);
                }

                SendMessageW(s->hSrcList, LB_RESETCONTENT, 0, 0);
                const int selNb = static_cast<int>(SendMessageW(s->hNbList, LB_GETCURSEL, 0, 0));
                if (selNb >= 0 && selNb < static_cast<int>(s->store->NotebookMappings().size())) {
                    const auto& nb = s->store->NotebookMappings()[static_cast<std::size_t>(selNb)];
                    const auto srcs = s->store->SourcesForNotebook(nb.localKeyUtf8);
                    for (const auto& src : srcs) {
                        const std::wstring stateTxt =
                            (src.state == GeminiNotebook::SyncSourceState::ActiveSynced) ? L"Sincronizado" :
                            (src.state == GeminiNotebook::SyncSourceState::SupersededLocal) ? L"Reemplazado localmente" :
                            (src.state == GeminiNotebook::SyncSourceState::ProcessingRemote) ? L"Procesando" :
                            (src.state == GeminiNotebook::SyncSourceState::Failed) ? L"Error" : L"Local";
                        const std::wstring row = Utf8ToWide(src.displayNameUtf8) + L"  [" + stateTxt + L" \u00B7 v" +
                            std::to_wstring(src.versionNumber) + L"]";
                        SendMessageW(s->hSrcList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
                    }
                }
            };

            if (msg == WM_CREATE) {
                s->theme.Init(hwnd, s->overrideGlass);
                s->fNorm = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_NORMAL);
                s->fBold = UiGlass::CreateHigUiFont(Sc(13, s->dpi), FW_SEMIBOLD);
                const int pad = Sc(16, s->dpi);
                const int fw = Sc(708, s->dpi);
                int y = Sc(14, s->dpi);

                s->hBanner = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                    pad, y, fw - Sc(168, s->dpi), Sc(44, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hBanner, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                HWND bConn = CreateWindowExW(0, L"BUTTON", L"Conectar cuenta...",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    pad + fw - Sc(158, s->dpi), y, Sc(158, s->dpi), Sc(28, s->dpi),
                    hwnd, reinterpret_cast<HMENU>(851), nullptr, nullptr);
                SendMessageW(bConn, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                y += Sc(48, s->dpi);

                const int colL = Sc(250, s->dpi);
                const int colR = fw - colL - Sc(12, s->dpi);
                HWND lNb = CreateWindowExW(0, L"STATIC", L"Cuadernos sincronizados:", WS_CHILD | WS_VISIBLE,
                    pad, y, colL, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                HWND lSrc = CreateWindowExW(0, L"STATIC", L"Fuentes e inventario local (sin duplicados):", WS_CHILD | WS_VISIBLE,
                    pad + colL + Sc(12, s->dpi), y, colR, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(lNb, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(lSrc, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                y += Sc(22, s->dpi);

                const int listH = Sc(220, s->dpi);
                s->hNbList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    pad, y, colL, listH, hwnd, reinterpret_cast<HMENU>(852), nullptr, nullptr);
                s->hSrcList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | LBS_NOINTEGRALHEIGHT,
                    pad + colL + Sc(12, s->dpi), y, colR, listH, hwnd, reinterpret_cast<HMENU>(853), nullptr, nullptr);
                SendMessageW(s->hNbList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hSrcList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += listH + Sc(12, s->dpi);

                // Quick URL / YouTube source bar
                HWND lUrl = CreateWindowExW(0, L"STATIC", L"A\u00F1adir enlace web o video de YouTube al cuaderno seleccionado:",
                    WS_CHILD | WS_VISIBLE, pad, y, fw, Sc(18, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(lUrl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(20, s->dpi);

                s->hUrlEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    pad, y, fw - Sc(262, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(854), nullptr, nullptr);
                SendMessageW(s->hUrlEdit, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                HWND bAddUrl = CreateWindowExW(0, L"BUTTON", L"+ A\u00F1adir enlace",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP, pad + fw - Sc(254, s->dpi), y - Sc(1, s->dpi), Sc(122, s->dpi), Sc(28, s->dpi),
                    hwnd, reinterpret_cast<HMENU>(855), nullptr, nullptr);
                HWND bAddFile = CreateWindowExW(0, L"BUTTON", L"+ Archivo local...",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP, pad + fw - Sc(124, s->dpi), y - Sc(1, s->dpi), Sc(124, s->dpi), Sc(28, s->dpi),
                    hwnd, reinterpret_cast<HMENU>(856), nullptr, nullptr);
                SendMessageW(bAddUrl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(bAddFile, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                y += Sc(36, s->dpi);

                s->hBundleChk = CreateWindowExW(0, L"BUTTON",
                    L"Agrupar notas cortas (.md/.txt) por cap\u00EDtulo para optimizar el cupo de 50 fuentes",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    pad, y, fw, Sc(22, s->dpi), hwnd, reinterpret_cast<HMENU>(857), nullptr, nullptr);
                SendMessageW(s->hBundleChk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                if (s->store->Config().bundleSmallNotesByChapter) {
                    SendMessageW(s->hBundleChk, BM_SETCHECK, BST_CHECKED, 0);
                }
                y += Sc(32, s->dpi);

                // Row 1: Primary sync actions
                const int bw = Sc(168, s->dpi);
                const int gap = Sc(12, s->dpi);
                auto mkBtn = [&](int col, int rowY, const wchar_t* txt, UINT_PTR cid, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                        pad + col * (bw + gap), rowY, bw, Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(cid), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(0, y, L"Sincronizar TEC / Carpeta", 858, true);
                mkBtn(1, y, L"Enviar cuaderno actual",    859);
                mkBtn(2, y, L"Actualizar remotos",        860);
                mkBtn(3, y, L"Abrir en la web",           861);
                y += Sc(38, s->dpi);

                mkBtn(0, y, L"Estudiar ahora con IA",     862);
                mkBtn(1, y, L"Puente para agentes...",    863);
                mkBtn(2, y, L"Nuevo cuaderno...",         864);
                mkBtn(3, y, L"Cerrar",                    IDCANCEL);

                s->theme.ThemeAllChildren(hwnd);
                refreshUi();
                return 0;
            }
            {
                LRESULT tr = 0;
                if (s->theme.HandleMessage(hwnd, msg, wp, lp, tr)) return tr;
            }
            if (msg == WM_COMMAND) {
                const WORD id = LOWORD(wp);
                if (id == 852 && HIWORD(wp) == LBN_SELCHANGE) {
                    refreshUi();
                    return 0;
                }
                if (id == 857) {
                    s->store->MutableConfig().bundleSmallNotesByChapter = (SendMessageW(s->hBundleChk, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->store->SaveConfig(nullptr);
                    return 0;
                }
                if (id == 851) {
                    if (ShowGeminiNotebookConnectDialog(hwnd, *s->store)) refreshUi();
                    return 0;
                }
                if (id == 863) {
                    ShowGeminiNotebookMcpConfigDialog(hwnd, s->store->Config(), &s->caps);
                    return 0;
                }
                if (id == 862) {
                    if (s->reqAiStudy) *s->reqAiStudy = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (id == 861) {
                    const int sel = static_cast<int>(SendMessageW(s->hNbList, LB_GETCURSEL, 0, 0));
                    std::string webUrl = "https://notebooklm.google.com/";
                    if (sel >= 0 && sel < static_cast<int>(s->store->NotebookMappings().size())) {
                        const auto& nb = s->store->NotebookMappings()[static_cast<std::size_t>(sel)];
                        if (!nb.webUrlUtf8.empty()) webUrl = nb.webUrlUtf8;
                    }
                    if (IsSafeExternalLinkUrl(webUrl)) {
                        ShellExecuteW(hwnd, L"open", Utf8ToWide(webUrl).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    }
                    return 0;
                }
                if (id == 864) {
                    // Create local/remote notebook entry
                    const std::string title = "Cuaderno Estudio " + GeminiNotebook::CurrentUtcDateYmd();
                    GeminiNotebook::LocalNotebookMapping m{};
                    m.localKeyUtf8 = "local:" + GeminiNotebook::ComputeSha256HexString(title + GeminiNotebook::CurrentUtcIso8601()).substr(0, 12);
                    m.displayNameUtf8 = title;
                    m.webUrlUtf8 = "https://notebooklm.google.com/";
                    if (s->store->Tokens().HasAnyCredential()) {
                        GeminiNotebook::GeminiNotebookRestClient client(*s->store);
                        GeminiNotebook::RemoteNotebookRecord created{};
                        std::string err;
                        bool wsRej = false;
                        if (client.CreateNotebook(title, created, &err, &wsRej)) {
                            m.remoteNotebookNameUtf8 = created.resourceNameUtf8;
                            m.webUrlUtf8 = created.webUrlUtf8;
                        } else if (!err.empty()) {
                            MessageBoxW(hwnd, Utf8ToWide(err).c_str(), L"Gemini Notebook", MB_OK | MB_ICONWARNING);
                        }
                    }
                    s->store->UpsertNotebookMapping(m);
                    s->store->SaveManifest(nullptr);
                    refreshUi();
                    return 0;
                }
                if (id == 860) {
                    GeminiNotebook::GeminiNotebookRestClient client(*s->store);
                    std::vector<GeminiNotebook::RemoteNotebookRecord> nbs;
                    std::string err;
                    bool wsRej = false;
                    if (client.ListNotebooks(nbs, &err, &wsRej)) {
                        for (const auto& r : nbs) {
                            GeminiNotebook::LocalNotebookMapping m{};
                            m.localKeyUtf8 = r.resourceNameUtf8;
                            m.displayNameUtf8 = r.displayNameUtf8.empty() ? r.resourceNameUtf8 : r.displayNameUtf8;
                            m.remoteNotebookNameUtf8 = r.resourceNameUtf8;
                            m.webUrlUtf8 = r.webUrlUtf8;
                            m.lastSyncUtc = GeminiNotebook::CurrentUtcIso8601();
                            s->store->UpsertNotebookMapping(m);
                        }
                        s->store->SaveManifest(nullptr);
                        // Also refresh MCP capabilities opportunistically
                        GeminiNotebook::GeminiNotebookMcpClient mcp(*s->store);
                        if (mcp.Initialize(nullptr, nullptr)) {
                            mcp.DiscoverToolsAndCapabilities(s->caps, nullptr);
                        }
                        refreshUi();
                    } else {
                        MessageBoxW(hwnd, Utf8ToWide(err.empty() ? "No se pudo consultar el inventario remoto." : err).c_str(),
                            L"Gemini Notebook", MB_OK | MB_ICONWARNING);
                        refreshUi();
                    }
                    return 0;
                }
                if (id == 855 || id == 856) {
                    std::string nbKey = "cuaderno:default";
                    std::string nbTitle = "Cuaderno CuadernU";
                    const int sel = static_cast<int>(SendMessageW(s->hNbList, LB_GETCURSEL, 0, 0));
                    if (sel >= 0 && sel < static_cast<int>(s->store->NotebookMappings().size())) {
                        nbKey = s->store->NotebookMappings()[static_cast<std::size_t>(sel)].localKeyUtf8;
                        nbTitle = s->store->NotebookMappings()[static_cast<std::size_t>(sel)].displayNameUtf8;
                    }
                    GeminiNotebook::AcademicMaterialCandidate c{};
                    if (id == 855) {
                        c.sourceUrlUtf8 = WideToUtf8(GetEditTextW(s->hUrlEdit));
                        c.inputKind = IsYouTubeUrlUtf8(c.sourceUrlUtf8)
                            ? GeminiNotebook::SourceInputKind::YouTubeUrl : GeminiNotebook::SourceInputKind::WebUrl;
                        c.displayNameUtf8 = ExtractUrlDomainUtf8(c.sourceUrlUtf8);
                    } else {
                        std::wstring fPath;
                        if (!FeatureDialogs::PickOpenFileWin32(hwnd,
                                L"Fuentes compatibles (*.pdf;*.docx;*.pptx;*.md;*.txt;*.epub;*.mp3;*.wav;*.png;*.jpg)\0*.pdf;*.docx;*.pptx;*.md;*.txt;*.csv;*.epub;*.mp3;*.wav;*.m4a;*.png;*.jpg;*.jpeg;*.webp\0\0",
                                L"Seleccionar archivo para Gemini Notebook", fPath)) {
                            return 0;
                        }
                        c.inputKind = GeminiNotebook::SourceInputKind::LocalFile;
                        c.localFilePathUtf8 = WideToUtf8(fPath);
                        c.displayNameUtf8 = WideToUtf8(std::filesystem::path(fPath).filename().wstring());
                    }
                    const auto pre = GeminiNotebook::ValidateSourceForUpload(c, s->store->Config(), true);
                    if (!pre.ok) {
                        const std::string msgErr = pre.issues.empty() ? "Validacion fallida." : pre.issues.front().messageUtf8;
                        MessageBoxW(hwnd, Utf8ToWide(msgErr).c_str(), L"Validaci\u00F3n previa", MB_OK | MB_ICONWARNING);
                        return 0;
                    }
                    const auto plan = s->store->ComputeIncrementalSyncPlan(nbKey, nbTitle, { c });
                    if (s->store->Tokens().HasAnyCredential()) {
                        GeminiNotebook::GeminiNotebookRestClient client(*s->store);
                        const auto sum = client.ExecuteSyncPlan(plan);
                        if (s->statusOut) *s->statusOut = "Gemini Notebook: " + std::to_string(sum.uploadedCount) + " nueva(s), " + std::to_string(sum.skippedCount) + " sin cambios";
                        if (!sum.ok && !sum.errorUtf8.empty()) {
                            MessageBoxW(hwnd, Utf8ToWide(sum.errorUtf8).c_str(), L"Gemini Notebook", MB_OK | MB_ICONWARNING);
                        }
                    } else {
                        // Track locally offline-first
                        GeminiNotebook::LocalNotebookMapping nm{};
                        nm.localKeyUtf8 = nbKey; nm.displayNameUtf8 = nbTitle; nm.webUrlUtf8 = "https://notebooklm.google.com/";
                        s->store->UpsertNotebookMapping(nm);
                        GeminiNotebook::LocalSyncSourceEntry e{};
                        e.localIdUtf8 = nbKey + ":" + pre.sha256Hex.substr(0, 16);
                        e.notebookKeyUtf8 = nbKey;
                        e.displayNameUtf8 = c.displayNameUtf8;
                        e.inputKind = c.inputKind;
                        e.localFilePathUtf8 = c.localFilePathUtf8;
                        e.sourceUrlUtf8 = c.sourceUrlUtf8;
                        e.mimeTypeUtf8 = pre.detectedMimeTypeUtf8;
                        e.sha256Hex = pre.sha256Hex;
                        e.sizeBytes = pre.sizeBytes;
                        e.estimatedWords = pre.estimatedWords;
                        e.state = GeminiNotebook::SyncSourceState::LocalOnly;
                        s->store->UpsertSourceEntry(e);
                        s->store->SaveManifest(nullptr);
                        MessageBoxW(hwnd, L"Fuente validada y guardada en tu inventario local. Conecta tu cuenta personal para subirla a la nube.",
                            L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                    }
                    SetWindowTextW(s->hUrlEdit, L"");
                    refreshUi();
                    return 0;
                }
                if (id == 858) {
                    std::wstring folder;
                    if (!PickFolderWin32(hwnd, L"Seleccionar carpeta de curso (TEC Digital o materiales locales)", folder)) return 0;
                    const std::string courseName = WideToUtf8(std::filesystem::path(folder).filename().wstring());
                    std::vector<GeminiNotebook::AcademicMaterialCandidate> cands;
                    std::error_code ec;
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path(folder), ec)) {
                        if (!entry.is_regular_file(ec)) continue;
                        if (GeminiNotebook::DetectSupportedMimeType(entry.path().wstring()).empty()) continue;
                        GeminiNotebook::AcademicMaterialCandidate c{};
                        c.inputKind = GeminiNotebook::SourceInputKind::LocalFile;
                        c.localFilePathUtf8 = WideToUtf8(entry.path().wstring());
                        c.displayNameUtf8 = WideToUtf8(entry.path().filename().wstring());
                        c.tecCourseUtf8 = courseName;
                        c.folderPathUtf8 = WideToUtf8(std::filesystem::relative(entry.path().parent_path(), std::filesystem::path(folder), ec).wstring());
                        cands.push_back(std::move(c));
                    }
                    if (cands.empty()) {
                        MessageBoxW(hwnd, L"No se encontraron archivos compatibles en la carpeta seleccionada.", L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                        return 0;
                    }
                    const auto groups = GeminiNotebook::ClassifyAcademicMaterials(cands, s->store->Config().recommendedMaxSourcesPerNotebook);
                    const auto plan = s->store->ComputeIncrementalSyncPlan("tec:" + courseName, courseName, cands);
                    std::ostringstream preview;
                    preview << "Curso: " << courseName << "\n"
                            << "Capitulos detectados: " << groups.size() << "\n"
                            << "Fuentes nuevas: " << plan.countNew << "\n"
                            << "Sin cambios (omitidas): " << plan.countUnchanged << "\n"
                            << "Modificadas (version v2): " << plan.countVersioned << "\n"
                            << "Bloqueadas por limite: " << plan.countBlocked << "\n";
                    if (plan.quotaWarningExceeds50) {
                        preview << "\nAviso: el total proyectado supera 50 fuentes. Puedes activar el empaquetado por capitulo.";
                    }
                    preview << "\n\nDeseas ejecutar esta sincronizacion ahora?";
                    if (MessageBoxW(hwnd, Utf8ToWide(preview.str()).c_str(), L"Vista previa de sincronizaci\u00F3n", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                        if (s->store->Tokens().HasAnyCredential()) {
                            GeminiNotebook::GeminiNotebookRestClient client(*s->store);
                            const auto sum = client.ExecuteSyncPlan(plan);
                            if (s->statusOut) *s->statusOut = "TEC -> Gemini Notebook: " + std::to_string(sum.uploadedCount) + " subidas, " + std::to_string(sum.skippedCount) + " sin cambios";
                            MessageBoxW(hwnd, Utf8ToWide("Sincronizacion completada: " + std::to_string(sum.uploadedCount) +
                                " nuevas, " + std::to_string(sum.versionedCount) + " versionadas, " +
                                std::to_string(sum.skippedCount) + " omitidas, " + std::to_string(sum.failedCount) + " fallos.").c_str(),
                                L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                        } else {
                            GeminiNotebook::LocalNotebookMapping nm{};
                            nm.localKeyUtf8 = plan.targetLocalNotebookKeyUtf8;
                            nm.displayNameUtf8 = plan.targetNotebookDisplayNameUtf8;
                            nm.tecCourseUtf8 = courseName;
                            nm.webUrlUtf8 = "https://notebooklm.google.com/";
                            s->store->UpsertNotebookMapping(nm);
                            for (const auto& item : plan.items) {
                                if (item.action == GeminiNotebook::SyncDiffAction::BlockedByPreFlight) continue;
                                GeminiNotebook::LocalSyncSourceEntry e{};
                                e.localIdUtf8 = nm.localKeyUtf8 + ":" + item.preflight.sha256Hex.substr(0, 16);
                                e.notebookKeyUtf8 = nm.localKeyUtf8;
                                e.displayNameUtf8 = item.targetDisplayNameUtf8;
                                e.inputKind = item.candidate.inputKind;
                                e.localFilePathUtf8 = item.candidate.localFilePathUtf8;
                                e.mimeTypeUtf8 = item.preflight.detectedMimeTypeUtf8;
                                e.sha256Hex = item.preflight.sha256Hex;
                                e.sizeBytes = item.preflight.sizeBytes;
                                e.estimatedWords = item.preflight.estimatedWords;
                                e.tecCourseUtf8 = courseName;
                                e.state = GeminiNotebook::SyncSourceState::LocalOnly;
                                s->store->UpsertSourceEntry(e);
                            }
                            s->store->SaveManifest(nullptr);
                            MessageBoxW(hwnd, L"Plan clasificado y guardado en tu inventario local. Conecta tu cuenta personal para subirlo.",
                                L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                        }
                        refreshUi();
                    }
                    return 0;
                }
                if (id == 859 && s->openDoc) {
                    const std::string md = GeminiNotebook::ExportDocumentToSyncMarkdown(*s->openDoc);
                    GeminiNotebook::AcademicMaterialCandidate c{};
                    c.inputKind = GeminiNotebook::SourceInputKind::InlineTextOrMarkdown;
                    c.displayNameUtf8 = (s->openDoc->Title().empty() ? "Cuaderno" : s->openDoc->Title()) + ".md";
                    c.inlineMarkdownUtf8 = md;
                    c.inlineMimeTypeUtf8 = "text/markdown";
                    const std::string nbKey = "doc:" + (s->openDoc->Title().empty() ? "actual" : s->openDoc->Title());
                    const auto plan = s->store->ComputeIncrementalSyncPlan(nbKey, s->openDoc->Title().empty() ? "Cuaderno CuadernU" : s->openDoc->Title(), { c });
                    if (s->store->Tokens().HasAnyCredential()) {
                        GeminiNotebook::GeminiNotebookRestClient client(*s->store);
                        const auto sum = client.ExecuteSyncPlan(plan);
                        if (s->statusOut) *s->statusOut = "Cuaderno enviado a Gemini Notebook (" + std::to_string(sum.uploadedCount + sum.versionedCount) + " fuente)";
                        MessageBoxW(hwnd, Utf8ToWide(sum.ok ? "Cuaderno sincronizado correctamente." : ("Resultado: " + sum.errorUtf8)).c_str(),
                            L"Gemini Notebook", MB_OK | (sum.ok ? MB_ICONINFORMATION : MB_ICONWARNING));
                    } else {
                        MessageBoxW(hwnd, L"Cuaderno exportado y validado localmente. Conecta primero tu cuenta personal de Google para subirlo.",
                            L"Gemini Notebook", MB_OK | MB_ICONINFORMATION);
                    }
                    refreshUi();
                    return 0;
                }
                if (id == IDCANCEL || id == IDOK) {
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                s->theme.Destroy();
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernU_GnHubDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    const int w = Sc(756, st.dpi);
    const int h = Sc(518, st.dpi);
    RECT orc{};
    GetWindowRect(owner, &orc);
    const int x = (orc.left + orc.right - w) / 2;
    const int y = (orc.top + orc.bottom - h) / 2;
    if (owner && !backgroundStealth) EnableWindow(owner, FALSE);
    const DWORD exSt = backgroundStealth ? (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE) : WS_EX_DLGMODALFRAME;
    const DWORD wSt = backgroundStealth ? (WS_POPUP | WS_CAPTION | WS_SYSMENU) : (WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE);
    HWND dlg = CreateWindowExW(exSt, L"CuadernU_GnHubDlg",
        L"Gemini Notebook — Centro de Estudio y Sincronizaci\u00F3n", wSt,
        x, y, w, h, backgroundStealth ? nullptr : owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (backgroundStealth && dlg) {
        ApplyStealthCloakToDialog(dlg);
        ShowWindow(dlg, SW_SHOWNOACTIVATE);
    }
    if (autoScreenshotPathW && autoScreenshotPathW[0] && dlg) {
        CaptureDialogStealthPng(dlg, autoScreenshotPathW);
        DestroyWindow(dlg);
        if (owner && !backgroundStealth) {
            EnableWindow(owner, TRUE);
            SetForegroundWindow(owner);
        }
        return;
    }
    MSG msg{};
    while (!st.done && IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessageW(dlg, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner && !backgroundStealth) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
}

} // namespace DocStudio::GeminiNotebookUi

#endif // _WIN32
