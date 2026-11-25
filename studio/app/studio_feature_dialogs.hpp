#pragma once

// ============================================================================
// studio_feature_dialogs.hpp
// Real interactive guías de interfaz + material translúcido Win32 dialogs for CuadernU.
// Replaces all hollow banners, discarded prompts, hardcoded demos, and
// read-only MessageBox dumps with genuine functional workflows.
// Zero decorative emojis. Neutral language only. PerMonitorV2 DPI scaled.
// ============================================================================

#include "../core/ai_provider_vault.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/structured_database.hpp"
#include "../core/studio_profile_store.hpp"
#include "../core/utf_conv.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../../ui_glass/gdi_translucent.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
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
#include <commdlg.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>

namespace DocStudio::FeatureDialogs {

inline UINT DpiOf(HWND hwnd) noexcept {
    using Fn = UINT(WINAPI*)(HWND);
    static auto p = reinterpret_cast<Fn>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (p && hwnd) {
        const UINT d = p(hwnd);
        if (d >= 72u && d <= 480u) return d;
    }
    return 96u;
}

inline int Sc(int px, UINT dpi) noexcept {
    return MulDiv(px, static_cast<int>(dpi), 96);
}

inline HFONT MakeUiFont(UINT dpi, int ptPx, bool bold = false) {
    return CreateFontW(
        -Sc(ptPx, dpi), 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
}

inline std::wstring GetControlTextW(HWND h) {
    if (!h) return {};
    const int len = GetWindowTextLengthW(h);
    if (len <= 0) return {};
    std::wstring s(static_cast<std::size_t>(len), L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    return s;
}

inline bool PickFolderWin32(HWND owner, const std::wstring& title, std::wstring& outFolder) {
    outFolder.clear();
    Microsoft::WRL::ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || !dlg) {
        return false;
    }
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(title.c_str());
    if (FAILED(dlg->Show(owner))) return false;
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (FAILED(dlg->GetResult(&item)) || !item) return false;
    PWSTR path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
        outFolder = path;
        CoTaskMemFree(path);
        return !outFolder.empty();
    }
    return false;
}

inline bool PickOpenFileWin32(HWND owner, const wchar_t* filter, const wchar_t* title, std::wstring& outPath) {
    wchar_t buf[MAX_PATH * 2]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrTitle = title;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(buf));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    outPath = buf;
    return !outPath.empty();
}

inline bool PickSaveFileWin32(HWND owner, const wchar_t* filter, const wchar_t* defExt, const wchar_t* defName, const wchar_t* title, std::wstring& outPath) {
    wchar_t buf[MAX_PATH * 2]{};
    if (defName) wcsncpy_s(buf, defName, _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrDefExt = defExt;
    ofn.lpstrTitle = title;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(buf));
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    outPath = buf;
    return !outPath.empty();
}

// ----------------------------------------------------------------------------
// 1. Interactive Filterable Command Palette Dialog (Ctrl+K)
// ----------------------------------------------------------------------------
inline int ShowInteractiveCommandPaletteDialog(
    HWND owner,
    const CommandPaletteRegistry& registry,
    std::string* outSelectedTitleUtf8 = nullptr)
{
    struct St {
        const CommandPaletteRegistry* reg = nullptr;
        std::vector<CommandPaletteEntry> shown;
        int chosenCmdId = 0;
        std::string chosenTitle;
        int activeCatIdx = 0;
        bool done = false;
        UINT dpi = 96;
        UiGlass::HigDialogTheme theme{};
        HFONT fNorm = nullptr, fBold = nullptr, fSmall = nullptr;
        HWND hEdit = nullptr, hList = nullptr, hRun = nullptr, hCancel = nullptr, hCount = nullptr;
        HWND hCats[8]{};
    } st{};
    st.reg = &registry;
    st.dpi = DpiOf(owner);

    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto matchesCategory = [&](const CommandPaletteEntry& e) -> bool {
                if (s->activeCatIdx == 0) return true;
                const std::string& c = e.categoryUtf8;
                switch (s->activeCatIdx) {
                case 1: return c == "Multimedia" || c == "Bloques" || c == "Elementos";
                case 2: return c == "Tinta" || c == "Papel" || c == "Herramientas" || c == "Capas";
                case 3: return c == "Estudio PDF" || c == "Vista" || c == "Cuaderno" || c == "Biblioteca";
                case 4: return c == "Conocimiento" || c == "Bases de Datos";
                case 5: return c == "STEM" || c == "Acad\xC3\xA9mico";
                case 6: return c == "Estudio" || c == "Asistente" || c == "Automatizaciones" || c == "Audio";
                case 7: return c == "Accesibilidad" || c == "Rendimiento" || c == "Ajustes" || c == "Archivo" ||
                               c == "Seguridad" || c == "Sincronizaci\xC3\xB3n" || c == "Colaboraci\xC3\xB3n" ||
                               c == "Interoperabilidad" || c == "Presentaci\xC3\xB3n";
                default: return true;
                }
            };

            auto refreshList = [&]() {
                if (!s->hList || !s->reg) return;
                const std::string q = WideToUtf8(GetControlTextW(s->hEdit));
                const auto allFound = s->reg->Search(q, 240);
                s->shown.clear();
                for (const auto& e : allFound) {
                    if (matchesCategory(e)) s->shown.push_back(e);
                }
                SendMessageW(s->hList, WM_SETREDRAW, FALSE, 0);
                SendMessageW(s->hList, LB_RESETCONTENT, 0, 0);
                for (const auto& e : s->shown) {
                    std::string line = e.titleUtf8 + "   [" + e.categoryUtf8 + "]   " + e.slashTriggerUtf8;
                    if (!e.shortcutUtf8.empty()) line += " (" + e.shortcutUtf8 + ")";
                    SendMessageW(s->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(line).c_str()));
                }
                if (!s->shown.empty()) SendMessageW(s->hList, LB_SETCURSEL, 0, 0);
                SendMessageW(s->hList, WM_SETREDRAW, TRUE, 0);
                InvalidateRect(s->hList, nullptr, FALSE);
                if (s->hCount) {
                    const std::wstring cTxt = std::to_wstring(s->shown.size()) + L" de " +
                        std::to_wstring(s->reg->All().size()) + L" funciones disponibles";
                    SetWindowTextW(s->hCount, cTxt.c_str());
                }
            };
            auto commitSel = [&]() {
                if (!s->hList) return;
                const LRESULT idx = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && static_cast<std::size_t>(idx) < s->shown.size()) {
                    s->chosenCmdId = s->shown[static_cast<std::size_t>(idx)].commandId;
                    s->chosenTitle = s->shown[static_cast<std::size_t>(idx)].titleUtf8;
                }
                s->done = true;
                DestroyWindow(hwnd);
            };

            if (msg == WM_CREATE) {
                s->theme.Init(hwnd);
                s->fNorm = UiGlass::CreateHigUiFont(Sc(14, s->dpi), FW_NORMAL);
                s->fBold = UiGlass::CreateHigUiFont(Sc(14, s->dpi), FW_SEMIBOLD);
                s->fSmall = UiGlass::CreateHigUiFont(Sc(12, s->dpi), FW_NORMAL);

                RECT rc{}; GetClientRect(hwnd, &rc);
                const int pad = Sc(14, s->dpi);
                const int w = (std::max)(120, static_cast<int>(rc.right - pad * 2));
                const int eh = Sc(30, s->dpi);
                const int bh = Sc(32, s->dpi);

                s->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    pad, pad, w, eh, hwnd, reinterpret_cast<HMENU>(201), nullptr, nullptr);
                SendMessageW(s->hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hEdit, EM_SETCUEBANNER, FALSE,
                    reinterpret_cast<LPARAM>(L"Buscar cualquier funci\u00F3n (ej. enlace, video, html, grafo, pomodoro, ia, ocr)..."));

                const wchar_t* catNames[8] = {
                    L"Todo", L"Multimedia", L"Tinta", L"PDF/Vista", L"B\u00F3veda/BD", L"STEM", L"IA/Estudio", L"A11y/Sistema"
                };
                const int catY = pad + eh + Sc(8, s->dpi);
                const int catH = Sc(26, s->dpi);
                const int catGap = Sc(4, s->dpi);
                const int catW = (w - catGap * 7) / 8;
                for (int ci = 0; ci < 8; ++ci) {
                    s->hCats[ci] = CreateWindowExW(0, L"BUTTON", catNames[ci],
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                        pad + ci * (catW + catGap), catY, catW, catH, hwnd,
                        reinterpret_cast<HMENU>(static_cast<UINT_PTR>(210 + ci)), nullptr, nullptr);
                    SendMessageW(s->hCats[ci], WM_SETFONT, reinterpret_cast<WPARAM>(ci == 0 ? s->fBold : s->fSmall), TRUE);
                }

                const int listY = catY + catH + Sc(8, s->dpi);
                const int listH = (std::max)(160, static_cast<int>(rc.bottom - listY - bh - Sc(22, s->dpi)));
                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    pad, listY, w, listH, hwnd, reinterpret_cast<HMENU>(202), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                const int by = rc.bottom - bh - Sc(12, s->dpi);
                const int bw = Sc(112, s->dpi);
                s->hCount = CreateWindowExW(0, L"STATIC", L"",
                    WS_CHILD | WS_VISIBLE,
                    pad, by + Sc(6, s->dpi), Sc(280, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hCount, WM_SETFONT, reinterpret_cast<WPARAM>(s->fSmall), TRUE);

                s->hCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    rc.right - pad - bw * 2 - Sc(8, s->dpi), by, bw, bh, hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                s->hRun = CreateWindowExW(0, L"BUTTON", L"Ejecutar",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    rc.right - pad - bw, by, bw, bh, hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(s->hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hRun, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                s->theme.ThemeAllChildren(hwnd);
                refreshList();
                SetFocus(s->hEdit);
                return 0;
            }
            LRESULT themeRes = 0;
            if (s->theme.HandleMessage(hwnd, msg, wp, lp, themeRes)) return themeRes;
            if (msg == WM_COMMAND) {
                const WORD cid = LOWORD(wp);
                const WORD code = HIWORD(wp);
                if (cid >= 210 && cid < 218) {
                    s->activeCatIdx = static_cast<int>(cid - 210);
                    for (int ci = 0; ci < 8; ++ci) {
                        if (s->hCats[ci]) SendMessageW(s->hCats[ci], WM_SETFONT,
                            reinterpret_cast<WPARAM>(ci == s->activeCatIdx ? s->fBold : s->fSmall), TRUE);
                    }
                    refreshList();
                    SetFocus(s->hEdit);
                    return 0;
                }
                if (cid == 201 && code == EN_CHANGE) { refreshList(); return 0; }
                if (cid == 202 && code == LBN_DBLCLK) { commitSel(); return 0; }
                if (cid == IDOK) { commitSel(); return 0; }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->fSmall) DeleteObject(s->fSmall);
                s->theme.Destroy();
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUCommandPaletteDlg";
        RegisterClassExW(&wc);
        registered = true;
    }

    RECT prc{ 120, 100, 960, 720 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(740, st.dpi);
    const int h = Sc(496, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 3);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUCommandPaletteDlg", L"Explorador de Funciones y Comandos (Ctrl+K)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);

    MSG msg{};
    bool gotQuit = false;
    WPARAM quitCode = 0;
    while (!st.done && hDlg && IsWindow(hDlg)) {
        const BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
        if (gm == 0) { gotQuit = true; quitCode = msg.wParam; break; }
        if (gm < 0) break;
        if (msg.message == WM_KEYDOWN) {
            if (msg.wParam == VK_ESCAPE) {
                st.done = true;
                DestroyWindow(hDlg);
                break;
            }
            if (msg.wParam == VK_RETURN) {
                SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
                continue;
            }
            if ((msg.wParam == VK_DOWN || msg.wParam == VK_UP) && st.hList && msg.hwnd == st.hEdit) {
                const LRESULT cnt = SendMessageW(st.hList, LB_GETCOUNT, 0, 0);
                LRESULT cur = SendMessageW(st.hList, LB_GETCURSEL, 0, 0);
                if (cnt > 0) {
                    if (msg.wParam == VK_DOWN) cur = (cur + 1 < cnt) ? (cur + 1) : 0;
                    else cur = (cur > 0) ? (cur - 1) : (cnt - 1);
                    SendMessageW(st.hList, LB_SETCURSEL, static_cast<WPARAM>(cur), 0);
                }
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (gotQuit) PostQuitMessage(static_cast<int>(quitCode));
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (outSelectedTitleUtf8) *outSelectedTitleUtf8 = st.chosenTitle;
    return st.chosenCmdId;
}

// ----------------------------------------------------------------------------
// 2. Selective PDF Export Options Dialog
// ----------------------------------------------------------------------------
inline bool ShowSelectivePdfExportDialog(
    HWND owner,
    std::size_t totalPages,
    PdfSelectiveExportOptions& outOpts,
    std::wstring& outSavePath)
{
    struct St {
        std::size_t totalPages = 1;
        PdfSelectiveExportOptions opts{};
        bool accepted = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hRange = nullptr, hChkAnnot = nullptr, hChkEdit = nullptr, hChkOnlyAnn = nullptr;
    } st{};
    st.totalPages = (std::max<std::size_t>)(1u, totalPages);
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 14, true);

                auto lbl = [&](const wchar_t* txt, int x, int y, int w, int h) {
                    HWND hw = CreateWindowExW(0, L"STATIC", txt, WS_CHILD | WS_VISIBLE,
                        Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(h, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                };
                lbl(L"P\u00E1ginas (vac\u00EDo = todas, ej. 1-3, 5):", 18, 16, 250, 22);
                s->hRange = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(275, s->dpi), Sc(14, s->dpi), Sc(155, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hRange, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hChkAnnot = CreateWindowExW(0, L"BUTTON", L"Incluir anotaciones (tinta, notas y f\u00F3rmulas)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(18, s->dpi), Sc(56, s->dpi), Sc(410, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hChkAnnot, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hChkAnnot, BM_SETCHECK, BST_CHECKED, 0);

                s->hChkEdit = CreateWindowExW(0, L"BUTTON", L"Modo editable reversible (desmarcar para PDF aplanado)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(18, s->dpi), Sc(88, s->dpi), Sc(410, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hChkEdit, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hChkEdit, BM_SETCHECK, BST_CHECKED, 0);

                s->hChkOnlyAnn = CreateWindowExW(0, L"BUTTON", L"Exportar solo p\u00E1ginas que contienen anotaciones",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(18, s->dpi), Sc(120, s->dpi), Sc(410, s->dpi), Sc(24, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hChkOnlyAnn, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(220, s->dpi), Sc(164, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Exportar...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(330, s->dpi), Sc(164, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == IDOK) {
                    s->opts.includeAnnotations = (SendMessageW(s->hChkAnnot, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->opts.flattened = (SendMessageW(s->hChkEdit, BM_GETCHECK, 0, 0) != BST_CHECKED);
                    s->opts.onlyAnnotatedPages = (SendMessageW(s->hChkOnlyAnn, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    s->opts.selectedPages0.clear();
                    const std::string rng = WideToUtf8(GetControlTextW(s->hRange));
                    std::istringstream iss(rng);
                    std::string tok;
                    while (std::getline(iss, tok, ',')) {
                        const auto dash = tok.find('-');
                        try {
                            if (dash != std::string::npos) {
                                int a = std::stoi(tok.substr(0, dash));
                                int b = std::stoi(tok.substr(dash + 1));
                                if (a > b) std::swap(a, b);
                                for (int p = a; p <= b; ++p) {
                                    if (p >= 1 && static_cast<std::size_t>(p) <= s->totalPages) {
                                        s->opts.selectedPages0.push_back(static_cast<std::size_t>(p - 1));
                                    }
                                }
                            } else if (!tok.empty()) {
                                int p = std::stoi(tok);
                                if (p >= 1 && static_cast<std::size_t>(p) <= s->totalPages) {
                                    s->opts.selectedPages0.push_back(static_cast<std::size_t>(p - 1));
                                }
                            }
                        } catch (...) {}
                    }
                    s->accepted = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUSelectivePdfDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 820, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(460, st.dpi), h = Sc(248, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUSelectivePdfDlg", L"Exportar PDF Selectivo",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (!st.accepted) return false;
    outOpts = st.opts;
    return PickSaveFileWin32(owner, L"Documento PDF (*.pdf)\0*.pdf\0\0", L"pdf",
        st.opts.flattened ? L"documento_aplanado.pdf" : L"documento_editable.pdf",
        L"Guardar PDF Selectivo", outSavePath);
}

// ----------------------------------------------------------------------------
// 3. Interactive Folder Sync Dialog (3 Modes + Selective Filter + Backup-First)
// ----------------------------------------------------------------------------
inline bool ShowFolderSyncDialog(
    HWND owner,
    const std::wstring& defaultDeviceDir,
    std::wstring& outTargetDir,
    FolderSyncMode& outMode,
    SelectiveSyncFilter& outFilter)
{
    struct St {
        std::wstring targetDir;
        FolderSyncMode mode = FolderSyncMode::MergeLossless;
        SelectiveSyncFilter filter{};
        bool accepted = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hTarget = nullptr, hMode = nullptr, hExclExt = nullptr;
    } st{};
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);
    st.targetDir = (std::filesystem::path(defaultDeviceDir).parent_path() / L"CuadernU_Sync").wstring();

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 14, true);

                auto lbl = [&](const wchar_t* t, int x, int y, int w, int h) {
                    HWND hw = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(h, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                };
                lbl(L"Carpeta destino (OneDrive / Drive / Dropbox / Local):", 18, 14, 440, 20);
                s->hTarget = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", s->targetDir.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(18, s->dpi), Sc(38, s->dpi), Sc(340, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hTarget, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                HWND hBrowse = CreateWindowExW(0, L"BUTTON", L"Explorar...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(366, s->dpi), Sc(37, s->dpi), Sc(92, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(301), nullptr, nullptr);
                SendMessageW(hBrowse, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                lbl(L"Modo de sincronizaci\u00F3n (siempre crea respaldo previo):", 18, 76, 440, 20);
                s->hMode = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(18, s->dpi), Sc(100, s->dpi), Sc(440, s->dpi), Sc(120, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hMode, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Fusionar sin p\u00E9rdida (bidireccional + copias de conflicto)"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Subir biblioteca local hacia destino (respalda destino antes)"));
                SendMessageW(s->hMode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Reemplazar local con copia de destino (respalda local antes)"));
                SendMessageW(s->hMode, CB_SETCURSEL, 0, 0);

                lbl(L"Extensiones excluidas (filtro selectivo, ej. .tmp,.bak):", 18, 138, 440, 20);
                s->hExclExt = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L".tmp,.bak",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(18, s->dpi), Sc(162, s->dpi), Sc(440, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hExclExt, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(248, s->dpi), Sc(204, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Sincronizar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(358, s->dpi), Sc(204, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == 301) {
                    std::wstring f;
                    if (PickFolderWin32(hwnd, L"Seleccionar carpeta de sincronizaci\u00F3n", f)) {
                        SetWindowTextW(s->hTarget, f.c_str());
                    }
                    return 0;
                }
                if (LOWORD(wp) == IDOK) {
                    s->targetDir = GetControlTextW(s->hTarget);
                    const int idx = static_cast<int>(SendMessageW(s->hMode, CB_GETCURSEL, 0, 0));
                    s->mode = (idx == 1) ? FolderSyncMode::UploadDeviceToTarget
                        : ((idx == 2) ? FolderSyncMode::ReplaceDeviceWithTarget : FolderSyncMode::MergeLossless);
                    s->filter.excludedExtensionsUtf8.clear();
                    std::istringstream iss(WideToUtf8(GetControlTextW(s->hExclExt)));
                    std::string ext;
                    while (std::getline(iss, ext, ',')) {
                        while (!ext.empty() && ext.front() == ' ') ext.erase(ext.begin());
                        while (!ext.empty() && ext.back() == ' ') ext.pop_back();
                        if (!ext.empty()) s->filter.excludedExtensionsUtf8.push_back(ext);
                    }
                    s->accepted = !s->targetDir.empty();
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUFolderSyncDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 860, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(492, st.dpi), h = Sc(290, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUFolderSyncDlg", L"Sincronizaci\u00F3n Local-First",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (!st.accepted) return false;
    outTargetDir = st.targetDir;
    outMode = st.mode;
    outFilter = st.filter;
    return true;
}

// ----------------------------------------------------------------------------
// 4. Real Web Clipper Dialog (Live WinHTTP URL Fetch OR Local .html File)
// ----------------------------------------------------------------------------
inline bool ShowWebClipperDialog(
    HWND owner,
    WebClipCaptureResult& outClip,
    std::string& outErrUtf8)
{
    struct St {
        WebClipCaptureResult clip{};
        std::string err;
        bool accepted = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hUrl = nullptr, hTpl = nullptr, hSel = nullptr;
        std::string loadedLocalHtml;
    } st{};
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 14, true);

                auto lbl = [&](const wchar_t* t, int x, int y, int w, int h) {
                    HWND hw = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(h, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                };
                lbl(L"URL (HTTP/HTTPS) o abra un archivo HTML local:", 18, 14, 450, 20);
                s->hUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"https://",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(18, s->dpi), Sc(38, s->dpi), Sc(340, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hUrl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                HWND hFile = CreateWindowExW(0, L"BUTTON", L"HTML local...", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(366, s->dpi), Sc(37, s->dpi), Sc(104, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(401), nullptr, nullptr);
                SendMessageW(hFile, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                lbl(L"Plantilla de extracci\u00F3n:", 18, 76, 200, 20);
                s->hTpl = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(18, s->dpi), Sc(100, s->dpi), Sc(452, s->dpi), Sc(100, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hTpl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hTpl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Art\u00EDculo completo (t\u00EDtulo, URL, autor y contenido)"));
                SendMessageW(s->hTpl, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cita acad\u00E9mica (bloque citado + esquema)"));
                SendMessageW(s->hTpl, CB_SETCURSEL, 0, 0);

                lbl(L"Cita destacada opcional (selecci\u00F3n manual):", 18, 138, 450, 20);
                s->hSel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
                    Sc(18, s->dpi), Sc(162, s->dpi), Sc(452, s->dpi), Sc(72, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hSel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(256, s->dpi), Sc(248, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Capturar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(366, s->dpi), Sc(248, s->dpi), Sc(104, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == 401) {
                    std::wstring fp;
                    if (PickOpenFileWin32(hwnd, L"P\u00E1gina HTML (*.html;*.htm)\0*.html;*.htm\0Todos (*.*)\0*.*\0\0", L"Abrir archivo HTML local", fp)) {
                        std::ifstream ifs(std::filesystem::path(fp), std::ios::binary);
                        if (ifs.is_open()) {
                            std::ostringstream ss; ss << ifs.rdbuf();
                            s->loadedLocalHtml = ss.str();
                            SetWindowTextW(s->hUrl, (L"file://" + fp).c_str());
                        }
                    }
                    return 0;
                }
                if (LOWORD(wp) == IDOK) {
                    const std::string url = WideToUtf8(GetControlTextW(s->hUrl));
                    const std::string sel = WideToUtf8(GetControlTextW(s->hSel));
                    const int tIdx = static_cast<int>(SendMessageW(s->hTpl, CB_GETCURSEL, 0, 0));
                    const std::vector<WebClipperTemplate> builtIns = {
                        { "arxiv", "Articulo Academico", "arxiv.org", "ScholarlyArticle",
                          "# {{title|upper}}\n- Fuente: {{url}}\n- Autor: {{author}}\n- Tipo: {{schema:@type}}\n\n{{selection|blockquote}}\n\n{{content}}" },
                        { "cite", "Cita Academica", "", "Article",
                          "# {{title}}\n- Fuente: {{url}}\n- Tipo: {{schema:@type}}\n\n{{selection|blockquote}}\n\n{{content}}" },
                        { "article", "Articulo", "", "",
                          "# {{title}}\nFuente: {{url}}\nAutor: {{author}}\nFecha: {{date}}\n\n{{content}}" }
                    };

                    std::string html = s->loadedLocalHtml;
                    if (html.empty() && (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) && url.size() > 8) {
                        HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                        const bool okFetch = WebClipperEngine::FetchUrlHtmlWinHttp(url, html, &s->err);
                        SetCursor(oldC);
                        if (!okFetch && sel.empty()) {
                            MessageBoxW(hwnd, Utf8ToWide("No se pudo descargar la URL: " + s->err).c_str(), L"Web Clipper", MB_ICONWARNING);
                            return 0;
                        }
                    }
                    const WebClipperTemplate* autoMatched = WebClipperEngine::MatchTemplateForPage(url, html, builtIns);
                    const WebClipperTemplate& tpl = autoMatched ? *autoMatched : ((tIdx == 1) ? builtIns[1] : builtIns[2]);
                    s->clip = WebClipperEngine::ClipHtmlPage(url, html, sel, tpl);
                    s->accepted = s->clip.ok;
                    if (!s->accepted) {
                        MessageBoxW(hwnd, L"Ingrese una URL v\u00E1lida, seleccione un HTML local o escriba una cita.", L"Web Clipper", MB_ICONINFORMATION);
                        return 0;
                    }
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUWebClipperDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 860, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(504, st.dpi), h = Sc(334, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUWebClipperDlg", L"Web Clipper (URL / HTML)",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outErrUtf8 = st.err;
    if (!st.accepted) return false;
    outClip = std::move(st.clip);
    return true;
}

// ----------------------------------------------------------------------------
// 5. Interactive Multiple-Choice Quiz Runner Dialog
// ----------------------------------------------------------------------------
inline bool ShowInteractiveQuizDialog(
    HWND owner,
    const std::vector<QuizQuestionItem>& quiz,
    QuizGradeReport& outGrade,
    int* outJumpPage0 = nullptr)
{
    if (quiz.empty()) return false;
    struct St {
        const std::vector<QuizQuestionItem>* quiz = nullptr;
        std::size_t idx = 0;
        std::vector<int> answers;
        int jumpPage = -1;
        bool finished = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr;
        HWND hHeader = nullptr, hQuest = nullptr, hOpts[4]{}, hCite = nullptr, hNext = nullptr;
    } st{};
    st.quiz = &quiz;
    st.answers.assign(quiz.size(), -1);
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto loadQ = [&]() {
                const auto& q = (*s->quiz)[s->idx];
                SetWindowTextW(s->hHeader, Utf8ToWide("Pregunta " + std::to_string(s->idx + 1) + " de " + std::to_string(s->quiz->size())).c_str());
                SetWindowTextW(s->hQuest, Utf8ToWide(q.questionUtf8).c_str());
                for (int i = 0; i < 4; ++i) {
                    const std::string opt = (static_cast<std::size_t>(i) < q.optionsUtf8.size()) ? q.optionsUtf8[static_cast<std::size_t>(i)] : "";
                    SetWindowTextW(s->hOpts[i], Utf8ToWide(opt).c_str());
                    SendMessageW(s->hOpts[i], BM_SETCHECK, (s->answers[s->idx] == i) ? BST_CHECKED : BST_UNCHECKED, 0);
                }
                SetWindowTextW(s->hCite, Utf8ToWide("Fuente: P\xC3\xA1g. " + std::to_string(q.sourceCitation.pageIndex0 + 1)).c_str());
                SetWindowTextW(s->hNext, (s->idx + 1 == s->quiz->size()) ? L"Calificar" : L"Siguiente");
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 15, true);

                s->hHeader = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, Sc(18, s->dpi), Sc(14, s->dpi), Sc(460, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hHeader, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                s->hQuest = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, Sc(18, s->dpi), Sc(42, s->dpi), Sc(460, s->dpi), Sc(48, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hQuest, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                for (int i = 0; i < 4; ++i) {
                    s->hOpts[i] = CreateWindowExW(0, L"BUTTON", L"",
                        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0),
                        Sc(22, s->dpi), Sc(98 + i * 34, s->dpi), Sc(456, s->dpi), Sc(28, s->dpi),
                        hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(500 + i)), nullptr, nullptr);
                    SendMessageW(s->hOpts[i], WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                }

                s->hCite = CreateWindowExW(0, L"BUTTON", L"Ir a fuente", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(18, s->dpi), Sc(246, s->dpi), Sc(150, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(510), nullptr, nullptr);
                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cerrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(268, s->dpi), Sc(246, s->dpi), Sc(96, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                s->hNext = CreateWindowExW(0, L"BUTTON", L"Siguiente", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(374, s->dpi), Sc(246, s->dpi), Sc(104, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(s->hCite, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hNext, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                loadQ();
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORSTATIC && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brBg);
            }
            if (msg == WM_COMMAND) {
                const WORD cid = LOWORD(wp);
                if (cid >= 500 && cid <= 503) {
                    s->answers[s->idx] = static_cast<int>(cid - 500);
                    return 0;
                }
                if (cid == 510) {
                    s->jumpPage = static_cast<int>((*s->quiz)[s->idx].sourceCitation.pageIndex0);
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (cid == IDOK) {
                    if (s->idx + 1 < s->quiz->size()) {
                        ++s->idx;
                        loadQ();
                    } else {
                        s->finished = true;
                        s->done = true;
                        DestroyWindow(hwnd);
                    }
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUInteractiveQuizDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 860, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(512, st.dpi), h = Sc(330, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUInteractiveQuizDlg", L"Cuestionario Interactivo de Estudio",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    if (outJumpPage0) *outJumpPage0 = st.jumpPage;
    if (st.finished) {
        outGrade = GroundedStudyAiAssistant::GradeQuizResponses(quiz, st.answers);
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// 6. Interactive Library & Content Views Browser Dialog
// ----------------------------------------------------------------------------
inline bool ShowLibraryContentViewsDialog(
    HWND owner,
    LibraryContentViewsManager& library,
    std::wstring& outOpenPath,
    bool& outMutated)
{
    outOpenPath.clear();
    outMutated = false;

    struct St {
        LibraryContentViewsManager* lib = nullptr;
        std::vector<LibraryItemRecord> shown;
        std::wstring openPath;
        bool mutated = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hFilter = nullptr, hList = nullptr;
    } st{};
    st.lib = &library;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto refresh = [&]() {
                if (!s->hList || !s->lib) return;
                const int idx = static_cast<int>(SendMessageW(s->hFilter, CB_GETCURSEL, 0, 0));
                if (idx == 1) s->shown = s->lib->QueryPhotosContentView();
                else if (idx == 2) s->shown = s->lib->QueryRecordingsContentView();
                else if (idx == 3) s->shown = s->lib->QueryBookmarksContentView();
                else if (idx == 4) s->shown = s->lib->QueryAttachmentsContentView();
                else if (idx == 5) s->shown = s->lib->QueryFavoritesAndPinned();
                else if (idx == 6) s->shown = s->lib->QueryRecentItems(50);
                else if (idx == 7) s->shown = s->lib->QueryTrashItems();
                else {
                    s->shown.clear();
                    for (const auto& it : s->lib->Items()) if (!it.inTrash) s->shown.push_back(it);
                }
                SendMessageW(s->hList, LB_RESETCONTENT, 0, 0);
                for (const auto& it : s->shown) {
                    std::ostringstream os;
                    if (it.favorite) os << "[Fav] ";
                    if (it.inTrash) os << "[Papelera] ";
                    os << it.titleUtf8 << " (" << it.kindUtf8 << ")  |  Fotos:" << it.photoCount
                       << "  Audio:" << it.recordingCount << "  Marcadores:" << it.bookmarkCount
                       << "  Adj:" << it.attachmentCount;
                    SendMessageW(s->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(os.str()).c_str()));
                }
                if (!s->shown.empty()) SendMessageW(s->hList, LB_SETCURSEL, 0, 0);
            };
            auto currentItem = [&]() -> const LibraryItemRecord* {
                const LRESULT idx = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && static_cast<std::size_t>(idx) < s->shown.size()) {
                    return &s->shown[static_cast<std::size_t>(idx)];
                }
                return nullptr;
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 14, true);

                HWND hLbl = CreateWindowExW(0, L"STATIC", L"Vista de contenido:", WS_CHILD | WS_VISIBLE,
                    Sc(16, s->dpi), Sc(16, s->dpi), Sc(140, s->dpi), Sc(22, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(hLbl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                s->hFilter = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                    Sc(160, s->dpi), Sc(12, s->dpi), Sc(460, s->dpi), Sc(200, s->dpi), hwnd, reinterpret_cast<HMENU>(601), nullptr, nullptr);
                SendMessageW(s->hFilter, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                const wchar_t* views[] = {
                    L"Todos los cuadernos activos",
                    L"Content View: Fotos e im\u00E1genes",
                    L"Content View: Grabaciones de audio",
                    L"Content View: Marcadores",
                    L"Content View: Adjuntos incrustados",
                    L"Favoritos y Fijados",
                    L"Recientes",
                    L"Papelera (restaurar)"
                };
                for (const wchar_t* v : views) SendMessageW(s->hFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(v));
                SendMessageW(s->hFilter, CB_SETCURSEL, 0, 0);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(48, s->dpi), Sc(604, s->dpi), Sc(286, s->dpi), hwnd, reinterpret_cast<HMENU>(602), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                auto mkBtn = [&](const wchar_t* t, int x, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(346, s->dpi), Sc(w, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(L"Abrir", 16, 86, 610, true);
                mkBtn(L"Favorito", 108, 92, 611);
                mkBtn(L"Duplicar", 206, 92, 612);
                mkBtn(L"Papelera / Restaurar", 304, 156, 613);
                mkBtn(L"Cerrar", 528, 92, IDCANCEL);
                refresh();
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if ((msg == WM_CTLCOLORLISTBOX || msg == WM_CTLCOLORSTATIC) && s->brCard) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), (msg == WM_CTLCOLORLISTBOX ? pal.contentCard : pal.sidebarBg).ToColorRef());
                return reinterpret_cast<LRESULT>(msg == WM_CTLCOLORLISTBOX ? s->brCard : s->brBg);
            }
            if (msg == WM_COMMAND) {
                const WORD cid = LOWORD(wp);
                if (cid == 601 && HIWORD(wp) == CBN_SELCHANGE) { refresh(); return 0; }
                if (cid == 610 || (cid == 602 && HIWORD(wp) == LBN_DBLCLK)) {
                    if (const auto* it = currentItem()) {
                        if (!it->filePathUtf8.empty()) {
                            s->openPath = Utf8ToWide(it->filePathUtf8);
                            s->done = true;
                            DestroyWindow(hwnd);
                        }
                    }
                    return 0;
                }
                if (cid == 611) {
                    if (const auto* it = currentItem()) {
                        s->lib->SetFavorite(it->itemId, !it->favorite);
                        s->mutated = true;
                        refresh();
                    }
                    return 0;
                }
                if (cid == 612) {
                    if (const auto* it = currentItem()) {
                        s->lib->DuplicateItem(it->itemId);
                        s->mutated = true;
                        refresh();
                    }
                    return 0;
                }
                if (cid == 613) {
                    if (const auto* it = currentItem()) {
                        if (it->inTrash) s->lib->RestoreFromTrash(it->itemId);
                        else s->lib->MoveToTrash(it->itemId);
                        s->mutated = true;
                        refresh();
                    }
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernULibraryViewsDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 100, 100, 960, 700 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(652, st.dpi), h = Sc(432, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernULibraryViewsDlg", L"Biblioteca y Content Views",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outOpenPath = st.openPath;
    outMutated = st.mutated;
    return !outOpenPath.empty() || outMutated;
}

// ----------------------------------------------------------------------------
// 7. Interactive Anchored Comments Manager Dialog
// ----------------------------------------------------------------------------
inline bool ShowCommentsManagerDialog(
    HWND owner,
    AnchoredCommentThreadManager& mgr,
    std::size_t currentPage0,
    std::uint64_t selectedObjId,
    bool& outMutated)
{
    outMutated = false;
    struct St {
        AnchoredCommentThreadManager* mgr = nullptr;
        std::size_t curPage = 0;
        std::uint64_t selObj = 0;
        bool mutated = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hList = nullptr, hInput = nullptr;
    } st{};
    st.mgr = &mgr;
    st.curPage = currentPage0;
    st.selObj = selectedObjId;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);

            auto refresh = [&]() {
                SendMessageW(s->hList, LB_RESETCONTENT, 0, 0);
                for (const auto& th : s->mgr->AllThreads()) {
                    std::ostringstream os;
                    os << "#" << th.threadId << (th.resolved ? " [Resuelto] " : " [Activo] ")
                       << "Pag." << (th.pageIndex0 + 1) << " (" << th.replies.size() << " msg): ";
                    if (!th.replies.empty()) os << th.replies.back().authorUtf8 << ": " << th.replies.back().bodyUtf8;
                    SendMessageW(s->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(os.str()).c_str()));
                }
                if (!s->mgr->AllThreads().empty()) SendMessageW(s->hList, LB_SETCURSEL, 0, 0);
            };
            auto selThreadId = [&]() -> std::uint64_t {
                const LRESULT idx = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                if (idx >= 0 && static_cast<std::size_t>(idx) < s->mgr->AllThreads().size()) {
                    return s->mgr->AllThreads()[static_cast<std::size_t>(idx)].threadId;
                }
                return 0;
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 14, true);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(14, s->dpi), Sc(560, s->dpi), Sc(210, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(234, s->dpi), Sc(560, s->dpi), Sc(28, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hInput, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                auto mkBtn = [&](const wchar_t* t, int x, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(274, s->dpi), Sc(w, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkBtn(L"Nuevo hilo", 16, 108, 701, true);
                mkBtn(L"Responder", 132, 104, 702);
                mkBtn(L"Resolver / Reabrir", 244, 152, 703);
                mkBtn(L"Cerrar", 484, 92, IDCANCEL);
                refresh();
                const std::string titleCnt = "Comentarios Anclados (" +
                    std::to_string(s->mgr->ActiveThreads().size()) + " activos, " +
                    std::to_string(s->mgr->ResolvedThreads().size()) + " resueltos)";
                SetWindowTextW(hwnd, Utf8ToWide(titleCnt).c_str());
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORLISTBOX || msg == WM_CTLCOLOREDIT) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.contentCard.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brCard);
            }
            if (msg == WM_COMMAND) {
                const WORD cid = LOWORD(wp);
                if (cid == 701) {
                    const std::string body = WideToUtf8(GetControlTextW(s->hInput));
                    if (!body.empty()) {
                        s->mgr->CreateThread(s->curPage, s->selObj, { 88.0, 88.0, 160.0, 48.0 }, "Autor", body);
                        SetWindowTextW(s->hInput, L"");
                        s->mutated = true;
                        refresh();
                    }
                    return 0;
                }
                if (cid == 702) {
                    const std::uint64_t tid = selThreadId();
                    const std::string body = WideToUtf8(GetControlTextW(s->hInput));
                    if (tid != 0 && !body.empty()) {
                        s->mgr->AddReply(tid, "Autor", body);
                        SetWindowTextW(s->hInput, L"");
                        s->mutated = true;
                        refresh();
                    }
                    return 0;
                }
                if (cid == 703) {
                    const std::uint64_t tid = selThreadId();
                    if (const auto* th = s->mgr->FindThread(tid)) {
                        if (th->resolved) s->mgr->ReopenThread(tid, "Autor");
                        else s->mgr->ResolveThread(tid, "Autor");
                        s->mutated = true;
                        refresh();
                    }
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUCommentsMgrDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 920, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(608, st.dpi), h = Sc(356, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUCommentsMgrDlg", L"Comentarios Anclados y @Menciones",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    outMutated = st.mutated;
    return st.mutated;
}

// ----------------------------------------------------------------------------
// 8. Multi-Provider Encrypted AI Vault & Whisper ASR Manager Dialog
// ----------------------------------------------------------------------------
inline bool ShowMultiProviderAiVaultDialog(
    HWND owner,
    AiProviderVault& vault,
    StudioUiPreferences& prefs)
{
    struct St {
        AiProviderVault workingVault;
        AiProviderVault* outVault = nullptr;
        StudioUiPreferences* prefs = nullptr;
        int selIdx = 0;
        bool revealKey = false;
        bool accepted = false;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hGlobalEnable = nullptr, hList = nullptr, hFailover = nullptr;
        HWND hUrl = nullptr, hKey = nullptr, hReveal = nullptr, hModel = nullptr;
        HWND hStatus = nullptr, hWhisperExe = nullptr, hWhisperModel = nullptr;
    } st{};
    st.workingVault = vault;
    st.outVault = &vault;
    st.prefs = &prefs;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);
    for (std::size_t i = 0; i < st.workingVault.Providers().size(); ++i) {
        if (st.workingVault.Providers()[i].idUtf8 == st.workingVault.ActiveProviderId()) {
            st.selIdx = static_cast<int>(i);
            break;
        }
    }

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

            auto commitCurrentFields = [&]() {
                auto& provs = s->workingVault.MutableProviders();
                if (s->selIdx < 0 || static_cast<std::size_t>(s->selIdx) >= provs.size()) return;
                auto& p = provs[static_cast<std::size_t>(s->selIdx)];
                p.endpointUrlUtf8 = WideToUtf8(GetControlTextW(s->hUrl));
                p.modelNameUtf8 = WideToUtf8(GetControlTextW(s->hModel));
                const std::string rawKey = WideToUtf8(GetControlTextW(s->hKey));
                p.apiKeyUtf8 = rawKey;
                if (!rawKey.empty()) p.enabled = true;
            };

            auto refreshProviderList = [&]() {
                SendMessageW(s->hList, LB_RESETCONTENT, 0, 0);
                for (const auto& p : s->workingVault.Providers()) {
                    const bool isAct = (p.idUtf8 == s->workingVault.ActiveProviderId());
                    const bool hasKey = !p.apiKeyUtf8.empty() || !p.requiresApiKey;
                    std::string ln = (isAct ? "* " : "  ") + p.displayNameUtf8 +
                        (hasKey ? (p.requiresApiKey ? " [Clave]" : " [Local]") : " [Sin clave]");
                    SendMessageW(s->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(ln).c_str()));
                }
                SendMessageW(s->hList, LB_SETCURSEL, static_cast<WPARAM>(s->selIdx), 0);
            };

            auto loadSelectedFields = [&]() {
                const auto& provs = s->workingVault.Providers();
                if (s->selIdx < 0 || static_cast<std::size_t>(s->selIdx) >= provs.size()) return;
                const auto& p = provs[static_cast<std::size_t>(s->selIdx)];
                SetWindowTextW(s->hUrl, Utf8ToWide(p.endpointUrlUtf8).c_str());
                SetWindowTextW(s->hKey, Utf8ToWide(p.apiKeyUtf8).c_str());
                SendMessageW(s->hModel, CB_RESETCONTENT, 0, 0);
                for (const auto& m : p.suggestedModelsUtf8) {
                    SendMessageW(s->hModel, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(m).c_str()));
                }
                SetWindowTextW(s->hModel, Utf8ToWide(p.modelNameUtf8).c_str());
                const std::string mask = p.requiresApiKey ? AiProviderVault::MaskApiKeyForDisplay(p.apiKeyUtf8) : "Endpoint local (sin clave obligatoria)";
                const std::string stat = p.displayNameUtf8 + "  |  Estado: " + mask +
                    (p.idUtf8 == s->workingVault.ActiveProviderId() ? "  |  ACTIVO" : "");
                SetWindowTextW(s->hStatus, Utf8ToWide(stat).c_str());
            };

            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 13, false);
                s->fBold = MakeUiFont(s->dpi, 13, true);

                auto lbl = [&](const wchar_t* t, int x, int y, int w, int h, bool bold = false) {
                    HWND hw = CreateWindowExW(0, L"STATIC", t, WS_CHILD | WS_VISIBLE, Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(h, s->dpi), hwnd, nullptr, nullptr, nullptr);
                    SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                    return hw;
                };

                s->hGlobalEnable = CreateWindowExW(0, L"BUTTON",
                    L"Activar asistentes de IA (claves protegidas con cifrado local por usuario)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    Sc(16, s->dpi), Sc(12, s->dpi), Sc(660, s->dpi), Sc(24, s->dpi), hwnd, reinterpret_cast<HMENU>(810), nullptr, nullptr);
                SendMessageW(s->hGlobalEnable, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                SendMessageW(s->hGlobalEnable, BM_SETCHECK, s->workingVault.IsEnabled() ? BST_CHECKED : BST_UNCHECKED, 0);

                lbl(L"Proveedores soportados:", 16, 42, 224, 18, true);
                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(64, s->dpi), Sc(232, s->dpi), Sc(256, s->dpi), hwnd, reinterpret_cast<HMENU>(811), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                s->hStatus = lbl(L"", 262, 42, 426, 20, true);

                lbl(L"Endpoint HTTP / HTTPS:", 262, 68, 260, 18);
                s->hUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(262, s->dpi), Sc(88, s->dpi), Sc(426, s->dpi), Sc(26, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hUrl, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                lbl(L"Clave API (oculta por seguridad; autodetecci\u00F3n de prefijo):", 262, 120, 426, 18);
                s->hKey = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
                    Sc(262, s->dpi), Sc(140, s->dpi), Sc(286, s->dpi), Sc(26, s->dpi), hwnd, reinterpret_cast<HMENU>(812), nullptr, nullptr);
                SendMessageW(s->hKey, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                s->hReveal = CreateWindowExW(0, L"BUTTON", L"Mostrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(554, s->dpi), Sc(139, s->dpi), Sc(66, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(813), nullptr, nullptr);
                HWND hClr = CreateWindowExW(0, L"BUTTON", L"Borrar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(624, s->dpi), Sc(139, s->dpi), Sc(64, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(814), nullptr, nullptr);
                SendMessageW(s->hReveal, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hClr, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                lbl(L"Modelo activo:", 262, 174, 220, 18);
                s->hModel = CreateWindowExW(0, L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
                    Sc(262, s->dpi), Sc(194, s->dpi), Sc(286, s->dpi), Sc(180, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hModel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                HWND hDisc = CreateWindowExW(0, L"BUTTON", L"Detectar modelos", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(554, s->dpi), Sc(193, s->dpi), Sc(134, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(815), nullptr, nullptr);
                SendMessageW(hDisc, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                lbl(L"Respaldo autom\u00E1tico (failover si falla el principal):", 262, 230, 340, 18);
                s->hFailover = CreateWindowExW(0, L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                    Sc(262, s->dpi), Sc(250, s->dpi), Sc(286, s->dpi), Sc(200, s->dpi), hwnd, reinterpret_cast<HMENU>(816), nullptr, nullptr);
                SendMessageW(s->hFailover, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(s->hFailover, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"(Ninguno / Solo BM25 offline)"));
                int failSel = 0;
                for (std::size_t i = 0; i < s->workingVault.Providers().size(); ++i) {
                    const auto& p = s->workingVault.Providers()[i];
                    SendMessageW(s->hFailover, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(p.displayNameUtf8).c_str()));
                    if (p.idUtf8 == s->workingVault.FailoverProviderId()) failSel = static_cast<int>(i) + 1;
                }
                SendMessageW(s->hFailover, CB_SETCURSEL, static_cast<WPARAM>(failSel), 0);

                HWND hSetAct = CreateWindowExW(0, L"BUTTON", L"Usar como principal", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(554, s->dpi), Sc(249, s->dpi), Sc(134, s->dpi), Sc(28, s->dpi), hwnd, reinterpret_cast<HMENU>(817), nullptr, nullptr);
                SendMessageW(hSetAct, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);

                auto mkActBtn = [&](const wchar_t* t, int x, int y, int w, int id, bool bold = false) {
                    HWND b = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | WS_TABSTOP | (bold ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                        Sc(x, s->dpi), Sc(y, s->dpi), Sc(w, s->dpi), Sc(30, s->dpi), hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
                    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? s->fBold : s->fNorm), TRUE);
                };
                mkActBtn(L"Importar .env / JSON...", 262, 290, 152, 818);
                mkActBtn(L"Variables entorno",       420, 290, 130, 819);
                mkActBtn(L"Probar conexi\u00F3n",    556, 290, 132, 820);

                lbl(L"ASR local opcional — whisper-cli.exe:", 16, 332, 324, 18);
                s->hWhisperExe = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->prefs->whisperExePathUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(16, s->dpi), Sc(352, s->dpi), Sc(324, s->dpi), Sc(25, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hWhisperExe, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                lbl(L"Modelo ggml-*.bin (Whisper):", 354, 332, 334, 18);
                s->hWhisperModel = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(s->prefs->whisperModelPathUtf8).c_str(),
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                    Sc(354, s->dpi), Sc(352, s->dpi), Sc(334, s->dpi), Sc(25, s->dpi), hwnd, nullptr, nullptr, nullptr);
                SendMessageW(s->hWhisperModel, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);

                mkActBtn(L"Cancelar", 448, 392, 108, IDCANCEL);
                mkActBtn(L"Guardar B\u00F3veda", 564, 392, 124, IDOK, true);

                refreshProviderList();
                loadSelectedFields();
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if ((msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLORLISTBOX) && s->brBg) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), (msg == WM_CTLCOLORLISTBOX ? pal.contentCard : pal.sidebarBg).ToColorRef());
                return reinterpret_cast<LRESULT>(msg == WM_CTLCOLORLISTBOX ? s->brCard : s->brBg);
            }
            if (msg == WM_COMMAND) {
                const WORD cid = LOWORD(wp);
                if (cid == 811 && HIWORD(wp) == LBN_SELCHANGE) {
                    commitCurrentFields();
                    const LRESULT idx = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                    if (idx >= 0 && static_cast<std::size_t>(idx) < s->workingVault.Providers().size()) {
                        s->selIdx = static_cast<int>(idx);
                        loadSelectedFields();
                    }
                    return 0;
                }
                if (cid == 812 && HIWORD(wp) == EN_CHANGE) {
                    const std::string pasted = WideToUtf8(GetControlTextW(s->hKey));
                    if (pasted.size() >= 6) {
                        const AiProviderKind det = AiProviderVault::DetectProviderFromKeyPrefix(pasted);
                        if (det != AiProviderKind::CustomOpenAICompat) {
                            const std::string detId = AiProviderVault::ProviderIdForKind(det);
                            if (const auto* dp = s->workingVault.FindProvider(detId)) {
                                const std::string hint = "Prefijo detectado: " + dp->displayNameUtf8 +
                                    "  |  Clave: " + AiProviderVault::MaskApiKeyForDisplay(pasted);
                                SetWindowTextW(s->hStatus, Utf8ToWide(hint).c_str());
                            }
                        }
                    }
                    return 0;
                }
                if (cid == 813) {
                    s->revealKey = !s->revealKey;
                    SendMessageW(s->hKey, EM_SETPASSWORDCHAR, s->revealKey ? 0 : L'\u2022', 0);
                    SetWindowTextW(s->hReveal, s->revealKey ? L"Ocultar" : L"Mostrar");
                    InvalidateRect(s->hKey, nullptr, TRUE);
                    return 0;
                }
                if (cid == 814) {
                    if (s->selIdx >= 0 && static_cast<std::size_t>(s->selIdx) < s->workingVault.Providers().size()) {
                        s->workingVault.ClearApiKey(s->workingVault.Providers()[static_cast<std::size_t>(s->selIdx)].idUtf8);
                        SetWindowTextW(s->hKey, L"");
                        refreshProviderList();
                        loadSelectedFields();
                    }
                    return 0;
                }
                if (cid == 815) {
                    commitCurrentFields();
                    auto& provs = s->workingVault.MutableProviders();
                    if (s->selIdx >= 0 && static_cast<std::size_t>(s->selIdx) < provs.size()) {
                        auto& p = provs[static_cast<std::size_t>(s->selIdx)];
                        HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                        std::vector<std::string> models;
                        std::string err;
                        const bool ok = AiProviderVault::FetchAvailableModels(p, models, &err);
                        SetCursor(oldC);
                        if (ok && !models.empty()) {
                            p.suggestedModelsUtf8 = models;
                            SendMessageW(s->hModel, CB_RESETCONTENT, 0, 0);
                            for (const auto& m : models) {
                                SendMessageW(s->hModel, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(m).c_str()));
                            }
                            if (p.modelNameUtf8.empty() || std::find(models.begin(), models.end(), p.modelNameUtf8) == models.end()) {
                                p.modelNameUtf8 = models.front();
                            }
                            SetWindowTextW(s->hModel, Utf8ToWide(p.modelNameUtf8).c_str());
                            MessageBoxW(hwnd, Utf8ToWide("Modelos detectados en vivo: " + std::to_string(models.size())).c_str(),
                                L"Descubrimiento de Modelos", MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err.empty() ? "No se pudieron obtener modelos." : err).c_str(),
                                L"Descubrimiento de Modelos", MB_ICONWARNING);
                        }
                    }
                    return 0;
                }
                if (cid == 817) {
                    commitCurrentFields();
                    if (s->selIdx >= 0 && static_cast<std::size_t>(s->selIdx) < s->workingVault.Providers().size()) {
                        s->workingVault.SetActiveProviderId(s->workingVault.Providers()[static_cast<std::size_t>(s->selIdx)].idUtf8);
                        s->workingVault.SetEnabled(true);
                        SendMessageW(s->hGlobalEnable, BM_SETCHECK, BST_CHECKED, 0);
                        refreshProviderList();
                        loadSelectedFields();
                    }
                    return 0;
                }
                if (cid == 818) {
                    commitCurrentFields();
                    std::wstring fp;
                    if (PickOpenFileWin32(hwnd, L"Claves IA (.env; *.json; *.txt)\0*.env;*.json;*.txt\0Todos (*.*)\0*.*\0\0",
                            L"Importar Claves API (.env / JSON / OpenCode auth.json)", fp)) {
                        std::string err;
                        const std::wstring ext = std::filesystem::path(fp).extension().wstring();
                        const bool isJson = (ext == L".json" || ext == L".JSON");
                        const AiKeyImportSummary sum = isJson
                            ? s->workingVault.ImportFromJsonFile(fp, &err)
                            : s->workingVault.ImportFromEnvFile(fp, &err);
                        if (sum.importedCount > 0) {
                            SendMessageW(s->hGlobalEnable, BM_SETCHECK, BST_CHECKED, 0);
                            refreshProviderList();
                            loadSelectedFields();
                            MessageBoxW(hwnd, Utf8ToWide(sum.noteUtf8 + " (almacenadas de forma segura con cifrado local al guardar).").c_str(),
                                L"Importaci\u00F3n de Claves", MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, Utf8ToWide(err.empty() ? "No se detectaron claves reconocidas en el archivo." : err).c_str(),
                                L"Importaci\u00F3n de Claves", MB_ICONWARNING);
                        }
                    }
                    return 0;
                }
                if (cid == 819) {
                    commitCurrentFields();
                    const AiKeyImportSummary sum = s->workingVault.ImportFromProcessEnvironment();
                    if (sum.importedCount > 0) {
                        SendMessageW(s->hGlobalEnable, BM_SETCHECK, BST_CHECKED, 0);
                        refreshProviderList();
                        loadSelectedFields();
                    }
                    MessageBoxW(hwnd, Utf8ToWide(sum.noteUtf8).c_str(), L"Variables de Entorno", MB_ICONINFORMATION);
                    return 0;
                }
                if (cid == 820) {
                    commitCurrentFields();
                    const auto& provs = s->workingVault.Providers();
                    if (s->selIdx >= 0 && static_cast<std::size_t>(s->selIdx) < provs.size()) {
                        AiProviderProfile probe = provs[static_cast<std::size_t>(s->selIdx)];
                        probe.timeoutMs = 15000;
                        HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                        const AiQueryResult qr = AiProviderVault::QuerySingleProvider(probe, "Responde solo: OK.", "Ping");
                        SetCursor(oldC);
                        const std::string msgTxt = qr.ok
                            ? ("Conexi\xC3\xB3n verificada (" + qr.providerNameUsedUtf8 + " / " + qr.modelUsedUtf8 +
                               ", " + std::to_string(qr.latencyMs) + " ms):\n" + qr.replyTextUtf8.substr(0, 140))
                            : ("No disponible (" + probe.displayNameUtf8 + "): " + qr.errorUtf8);
                        MessageBoxW(hwnd, Utf8ToWide(msgTxt).c_str(), L"Prueba de Proveedor IA",
                            qr.ok ? MB_ICONINFORMATION : MB_ICONWARNING);
                    }
                    return 0;
                }
                if (cid == IDOK) {
                    commitCurrentFields();
                    s->workingVault.SetEnabled(SendMessageW(s->hGlobalEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    const LRESULT fIdx = SendMessageW(s->hFailover, CB_GETCURSEL, 0, 0);
                    if (fIdx <= 0) {
                        s->workingVault.SetFailoverProviderId("");
                    } else if (static_cast<std::size_t>(fIdx - 1) < s->workingVault.Providers().size()) {
                        s->workingVault.SetFailoverProviderId(s->workingVault.Providers()[static_cast<std::size_t>(fIdx - 1)].idUtf8);
                    }
                    s->prefs->whisperExePathUtf8 = WideToUtf8(GetControlTextW(s->hWhisperExe));
                    s->prefs->whisperModelPathUtf8 = WideToUtf8(GetControlTextW(s->hWhisperModel));
                    // Keep legacy LocalLlmConfig mirror synchronized with active non-secret metadata
                    if (const auto* act = s->workingVault.FindProvider(s->workingVault.ActiveProviderId())) {
                        s->prefs->llmConfig.enabled = s->workingVault.IsEnabled();
                        s->prefs->llmConfig.endpointUrlUtf8 = act->endpointUrlUtf8;
                        s->prefs->llmConfig.modelNameUtf8 = act->modelNameUtf8;
                    }
                    *s->outVault = std::move(s->workingVault);
                    s->accepted = true;
                    s->done = true;
                    DestroyWindow(hwnd);
                    return 0;
                }
                if (cid == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUMultiProviderAiVaultDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 100, 100, 960, 680 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(720, st.dpi), h = Sc(474, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUMultiProviderAiVaultDlg",
        L"Proveedores de IA y Reconocimiento de Voz",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    return st.accepted;
}

inline bool ShowAiAndAsrSettingsDialog(
    HWND owner,
    StudioUiPreferences& prefs,
    AiProviderVault* vault = nullptr)
{
    if (vault) {
        return ShowMultiProviderAiVaultDialog(owner, *vault, prefs);
    }
    AiProviderVault tmpVault;
    tmpVault.LoadFromDirectory(StudioProfileStore::DefaultProfileRootDir());
    if (ShowMultiProviderAiVaultDialog(owner, tmpVault, prefs)) {
        tmpVault.SaveToDirectory(StudioProfileStore::DefaultProfileRootDir());
        return true;
    }
    return false;
}

// ----------------------------------------------------------------------------
// 9. Reusable Snippet Picker Dialog
// ----------------------------------------------------------------------------
inline std::string ShowSnippetPickerDialog(HWND owner, const ReusableElementLibrary& lib) {
    if (lib.Entries().empty()) return {};
    struct St {
        const ReusableElementLibrary* lib = nullptr;
        std::string chosenId;
        bool done = false;
        UINT dpi = 96;
        UiGlass::GlassSettings glass{};
        HFONT fNorm = nullptr, fBold = nullptr;
        HBRUSH brBg = nullptr, brCard = nullptr;
        HWND hList = nullptr;
    } st{};
    st.lib = &lib;
    st.dpi = DpiOf(owner);
    UiGlass::LoadGlassSettings(st.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) -> LRESULT {
            auto* s = reinterpret_cast<St*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!s) return DefWindowProcW(hwnd, msg, wp, lp);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, s->glass.IsDark());
                const auto pal = UiGlass::GetPalette(s->glass);
                s->brBg = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                s->brCard = CreateSolidBrush(pal.contentCard.ToColorRef());
                s->fNorm = MakeUiFont(s->dpi, 14, false);
                s->fBold = MakeUiFont(s->dpi, 14, true);

                s->hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                    Sc(16, s->dpi), Sc(14, s->dpi), Sc(410, s->dpi), Sc(210, s->dpi), hwnd, reinterpret_cast<HMENU>(901), nullptr, nullptr);
                SendMessageW(s->hList, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                for (const auto& e : s->lib->Entries()) {
                    const std::string line = e.nameUtf8 + "  [" + e.categoryUtf8 + "] (" + std::to_string(e.objects.size()) + " obj)";
                    SendMessageW(s->hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(line).c_str()));
                }
                SendMessageW(s->hList, LB_SETCURSEL, 0, 0);

                HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    Sc(218, s->dpi), Sc(236, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                HWND hOk = CreateWindowExW(0, L"BUTTON", L"Insertar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    Sc(326, s->dpi), Sc(236, s->dpi), Sc(100, s->dpi), Sc(32, s->dpi), hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(hCan, WM_SETFONT, reinterpret_cast<WPARAM>(s->fNorm), TRUE);
                SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(s->fBold), TRUE);
                return 0;
            }
            if (msg == WM_ERASEBKGND && s->brBg) {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wp), &rc, s->brBg);
                return 1;
            }
            if (msg == WM_CTLCOLORLISTBOX && s->brCard) {
                const auto pal = UiGlass::GetPalette(s->glass);
                SetTextColor(reinterpret_cast<HDC>(wp), pal.labelPrimary.ToColorRef());
                SetBkColor(reinterpret_cast<HDC>(wp), pal.contentCard.ToColorRef());
                return reinterpret_cast<LRESULT>(s->brCard);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wp) == IDOK || (LOWORD(wp) == 901 && HIWORD(wp) == LBN_DBLCLK)) {
                    const LRESULT idx = SendMessageW(s->hList, LB_GETCURSEL, 0, 0);
                    if (idx >= 0 && static_cast<std::size_t>(idx) < s->lib->Entries().size()) {
                        s->chosenId = s->lib->Entries()[static_cast<std::size_t>(idx)].idUtf8;
                    }
                    s->done = true; DestroyWindow(hwnd); return 0;
                }
                if (LOWORD(wp) == IDCANCEL) { s->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { s->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (s->fNorm) DeleteObject(s->fNorm);
                if (s->fBold) DeleteObject(s->fBold);
                if (s->brBg) DeleteObject(s->brBg);
                if (s->brCard) DeleteObject(s->brCard);
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wp, lp);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = L"CuadernUSnippetPickerDlg";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 120, 120, 820, 620 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = Sc(458, st.dpi), h = Sc(318, st.dpi);
    const int x = prc.left + (std::max)(0, static_cast<int>((prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, static_cast<int>((prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUSnippetPickerDlg", L"Elementos Reutilizables Guardados",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &st);
    MSG msg{};
    while (!st.done && hDlg && IsWindow(hDlg)) {
        if (GetMessageW(&msg, nullptr, 0, 0) <= 0) break;
        if (!IsDialogMessageW(hDlg, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
    return st.chosenId;
}

} // namespace DocStudio::FeatureDialogs
#endif
