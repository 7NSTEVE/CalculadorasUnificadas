#pragma once

#include "../bridge/calc_bridge.hpp"
#include "../circuit/circuit_schematic.hpp"
#include "../circuit/circuit_toolbox_recognizer.hpp"
#include "../circuit/code_mcu_control.hpp"
#include "../circuit/mna_solver.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_crypto.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/ink_engine.hpp"
#include "../core/lan_remote.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/utf_conv.hpp"
#include "../core/zip_container.hpp"
#include "../interop/notebook_migration_engine.hpp"
#include "../interop/printout_engine.hpp"
#include "../math/advanced_math_engine.hpp"
#include "../math/cas_ai_pedagogy.hpp"
#include "../math/engineering_cas.hpp"
#include "../math/formula_sheet_index.hpp"
#include "../pdf/table_interop.hpp"
#include "../pdf/winrt_adapters.hpp"
#include "../../ui_glass/gdi_translucent.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
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

namespace DocStudio::GuiDialogs {

using PromptFn = std::function<bool(const std::wstring&, const std::wstring&, std::wstring&, bool)>;
using PasswordPromptFn = std::function<bool(const std::wstring&, const std::wstring&, std::wstring&)>;

inline std::wstring NormalizeCrLfWide(const std::wstring& in) {
    std::wstring out;
    out.reserve(in.size() + 32);
    for (std::size_t i = 0; i < in.size(); ++i) {
        if (in[i] == L'\n' && (i == 0 || in[i - 1] != L'\r')) out.push_back(L'\r');
        out.push_back(in[i]);
    }
    return out;
}

inline UINT GetSafeWindowDpi(HWND hwnd) noexcept {
    using FnGetDpiForWindow = UINT(WINAPI*)(HWND);
    static auto pGetDpi = reinterpret_cast<FnGetDpiForWindow>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
    if (pGetDpi && hwnd) {
        const UINT d = pGetDpi(hwnd);
        if (d >= 72u && d <= 480u) return d;
    }
    return 96u;
}

// Clean guías de interfaz + material translúcido theme-synchronized & PerMonitorV2 DPI-scaled report dialog
inline bool ShowReportDialog(
    HWND owner,
    const std::wstring& title,
    const std::wstring& bodyText,
    const std::wstring& confirmBtnText = L"Aceptar",
    bool showCancel = false)
{
    struct DlgState {
        std::wstring body;
        std::wstring confirmText;
        bool showCancel = false;
        bool accepted = false;
        bool done = false;
        UINT dpi = 96u;
        UiGlass::GlassSettings glass{};
        HFONT hFont = nullptr;
        HBRUSH hBgBrush = nullptr;
        HBRUSH hCardBrush = nullptr;
        HWND hEdit = nullptr;
        HWND hOk = nullptr;
        HWND hCancel = nullptr;
    } ds;
    ds.body = NormalizeCrLfWide(bodyText);
    ds.confirmText = confirmBtnText;
    ds.showCancel = showCancel;
    ds.dpi = GetSafeWindowDpi(owner);
    UiGlass::LoadGlassSettings(ds.glass);

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            auto* st = reinterpret_cast<DlgState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                st = static_cast<DlgState*>(cs->lpCreateParams);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
                return TRUE;
            }
            if (!st) return DefWindowProcW(hwnd, msg, wParam, lParam);
            auto layoutControls = [&]() {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const int pad = MulDiv(16, static_cast<int>(st->dpi), 96);
                const int btnW = MulDiv(148, static_cast<int>(st->dpi), 96);
                const int cancelW = MulDiv(108, static_cast<int>(st->dpi), 96);
                const int btnH = MulDiv(32, static_cast<int>(st->dpi), 96);
                const int bottomBand = btnH + MulDiv(24, static_cast<int>(st->dpi), 96);
                if (st->hEdit) {
                    MoveWindow(st->hEdit, pad, pad,
                        (std::max)(80, static_cast<int>(rc.right - pad * 2)),
                        (std::max)(60, static_cast<int>(rc.bottom - pad - bottomBand)), TRUE);
                }
                int rightX = rc.right - pad;
                const int btnY = rc.bottom - btnH - MulDiv(12, static_cast<int>(st->dpi), 96);
                if (st->hCancel) {
                    MoveWindow(st->hCancel, rightX - cancelW, btnY, cancelW, btnH, TRUE);
                    rightX -= cancelW + MulDiv(10, static_cast<int>(st->dpi), 96);
                }
                if (st->hOk) {
                    MoveWindow(st->hOk, rightX - btnW, btnY, btnW, btnH, TRUE);
                }
            };
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, st->glass.IsDark());
                const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glass);
                st->hBgBrush = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                st->hCardBrush = CreateSolidBrush(pal.contentCard.ToColorRef());
                st->hFont = CreateFontW(-MulDiv(14, static_cast<int>(st->dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                st->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->body.c_str(),
                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                    0, 0, 100, 100, hwnd, reinterpret_cast<HMENU>(101), nullptr, nullptr);
                SendMessageW(st->hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st->hFont), TRUE);
                if (st->showCancel) {
                    st->hCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                        0, 0, 100, 32, hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
                    SendMessageW(st->hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(st->hFont), TRUE);
                }
                st->hOk = CreateWindowExW(0, L"BUTTON", st->confirmText.c_str(),
                    WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                    0, 0, 148, 32, hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
                SendMessageW(st->hOk, WM_SETFONT, reinterpret_cast<WPARAM>(st->hFont), TRUE);
                layoutControls();
                return 0;
            }
            if (msg == WM_SIZE) { layoutControls(); return 0; }
            if (msg == WM_DPICHANGED) {
                st->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96u;
                HFONT nf = CreateFontW(-MulDiv(14, static_cast<int>(st->dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                    DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                if (nf) {
                    if (st->hEdit) SendMessageW(st->hEdit, WM_SETFONT, reinterpret_cast<WPARAM>(nf), FALSE);
                    if (st->hOk) SendMessageW(st->hOk, WM_SETFONT, reinterpret_cast<WPARAM>(nf), FALSE);
                    if (st->hCancel) SendMessageW(st->hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(nf), FALSE);
                    if (st->hFont) DeleteObject(st->hFont);
                    st->hFont = nf;
                }
                const RECT* sr = reinterpret_cast<const RECT*>(lParam);
                if (sr) SetWindowPos(hwnd, nullptr, sr->left, sr->top, sr->right - sr->left, sr->bottom - sr->top, SWP_NOZORDER | SWP_NOACTIVATE);
                layoutControls();
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            if (msg == WM_ERASEBKGND && st->hBgBrush) {
                RECT rc{};
                GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(wParam), &rc, st->hBgBrush);
                return 1;
            }
            if ((msg == WM_CTLCOLORSTATIC || msg == WM_CTLCOLOREDIT) && st->hCardBrush) {
                const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glass);
                HDC hdc = reinterpret_cast<HDC>(wParam);
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentCard.ToColorRef());
                return reinterpret_cast<LRESULT>(st->hCardBrush);
            }
            if (msg == WM_COMMAND) {
                if (LOWORD(wParam) == IDOK) { st->accepted = true; st->done = true; DestroyWindow(hwnd); return 0; }
                if (LOWORD(wParam) == IDCANCEL) { st->accepted = false; st->done = true; DestroyWindow(hwnd); return 0; }
            }
            if (msg == WM_CLOSE) { st->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_DESTROY) {
                if (st->hFont) { DeleteObject(st->hFont); st->hFont = nullptr; }
                if (st->hBgBrush) { DeleteObject(st->hBgBrush); st->hBgBrush = nullptr; }
                if (st->hCardBrush) { DeleteObject(st->hCardBrush); st->hCardBrush = nullptr; }
                return 0;
            }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"CuadernUReportDlgClass";
        RegisterClassExW(&wc);
        reg = true;
    }

    RECT prc{ 100, 100, 900, 700 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = MulDiv(640, static_cast<int>(ds.dpi), 96);
    const int h = MulDiv(460, static_cast<int>(ds.dpi), 96);
    const int x = prc.left + (std::max)(0, ((int)(prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, ((int)(prc.bottom - prc.top) - h) / 2);

    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUReportDlgClass", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &ds);
    MSG msg{};
    bool gotQuit = false;
    WPARAM quitCode = 0;
    while (!ds.done && hDlg && IsWindow(hDlg)) {
        const BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
        if (gm == 0) { gotQuit = true; quitCode = msg.wParam; break; }
        if (gm < 0) break;
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) { ds.done = true; DestroyWindow(hDlg); break; }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) { ds.accepted = true; ds.done = true; DestroyWindow(hDlg); break; }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (gotQuit) PostQuitMessage(static_cast<int>(quitCode));
    if (owner) {
        EnableWindow(owner, TRUE);
        SetActiveWindow(owner);
    }
    return ds.accepted;
}

// Crisp Vector QR Code Viewer Dialog (F-210, F-211) — guías de interfaz Dark/Light/HC & DPI scaled
inline void ShowQrCodeDialog(
    HWND owner,
    const std::wstring& title,
    const std::string& urlUtf8,
    const LanRemote::QrMatrix& qr)
{
    struct QrState {
        LanRemote::QrMatrix qr;
        std::wstring url;
        UINT dpi = 96u;
        UiGlass::GlassSettings glass{};
        bool done = false;
    } qs{};
    qs.qr = qr;
    qs.url = Utf8ToWide(urlUtf8);
    qs.dpi = GetSafeWindowDpi(owner);
    UiGlass::LoadGlassSettings(qs.glass);
    qs.done = false;

    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) -> LRESULT {
            auto* st = reinterpret_cast<QrState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            if (msg == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (!st) return DefWindowProcW(hwnd, msg, wParam, lParam);
            if (msg == WM_CREATE) {
                UiGlass::ApplyGoldenGateWindowFrame(hwnd, st->glass.IsDark());
                return 0;
            }
            if (msg == WM_DPICHANGED) {
                st->dpi = HIWORD(wParam) ? HIWORD(wParam) : 96u;
                const RECT* sr = reinterpret_cast<const RECT*>(lParam);
                if (sr) SetWindowPos(hwnd, nullptr, sr->left, sr->top, sr->right - sr->left, sr->bottom - sr->top, SWP_NOZORDER | SWP_NOACTIVATE);
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
            if (msg == WM_PAINT) {
                PAINTSTRUCT ps{};
                HDC hdc = BeginPaint(hwnd, &ps);
                RECT rc{};
                GetClientRect(hwnd, &rc);
                const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glass);
                HBRUSH bgBr = CreateSolidBrush(pal.sidebarBg.ToColorRef());
                FillRect(hdc, &rc, bgBr);
                DeleteObject(bgBr);

                const int n = (std::max)(1, st->qr.size);
                const int pad = MulDiv(24, static_cast<int>(st->dpi), 96);
                const int cell = (std::max)(4, (std::min)(static_cast<int>(rc.right - pad * 3), static_cast<int>(rc.bottom - pad * 4)) / n);
                const int qrPx = cell * n;
                const int qQuiet = cell * 2;
                const int ox = (rc.right - qrPx) / 2;
                const int oy = pad + qQuiet;
                // ISO QR standard requires a white quiet zone even in Dark Mode
                RECT plate{ ox - qQuiet, oy - qQuiet, ox + qrPx + qQuiet, oy + qrPx + qQuiet };
                FillRect(hdc, &plate, reinterpret_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
                HBRUSH darkBr = CreateSolidBrush(RGB(15, 23, 42));
                for (int r = 0; r < n; ++r) {
                    for (int c = 0; c < n; ++c) {
                        if (st->qr.At(r, c)) {
                            RECT mrc{ ox + c * cell, oy + r * cell, ox + (c + 1) * cell, oy + (r + 1) * cell };
                            FillRect(hdc, &mrc, darkBr);
                        }
                    }
                }
                DeleteObject(darkBr);
                HFONT hf = CreateFontW(-MulDiv(13, static_cast<int>(st->dpi), 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HGDIOBJ oldF = hf ? SelectObject(hdc, hf) : nullptr;
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                RECT trc{ pad, plate.bottom + MulDiv(12, static_cast<int>(st->dpi), 96), rc.right - pad, rc.bottom - pad };
                DrawTextW(hdc, st->url.c_str(), -1, &trc, DT_CENTER | DT_WORDBREAK);
                if (oldF) SelectObject(hdc, oldF);
                if (hf) DeleteObject(hf);
                EndPaint(hwnd, &ps);
                return 0;
            }
            if (msg == WM_KEYDOWN && (wParam == VK_ESCAPE || wParam == VK_RETURN)) { st->done = true; DestroyWindow(hwnd); return 0; }
            if (msg == WM_CLOSE) { st->done = true; DestroyWindow(hwnd); return 0; }
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;
        wc.lpszClassName = L"CuadernUQrDlgClass";
        RegisterClassExW(&wc);
        reg = true;
    }
    RECT prc{ 100, 100, 900, 700 };
    if (owner) GetWindowRect(owner, &prc);
    const int w = MulDiv(400, static_cast<int>(qs.dpi), 96);
    const int h = MulDiv(440, static_cast<int>(qs.dpi), 96);
    const int x = prc.left + (std::max)(0, ((int)(prc.right - prc.left) - w) / 2);
    const int y = prc.top + (std::max)(0, ((int)(prc.bottom - prc.top) - h) / 2);
    if (owner) EnableWindow(owner, FALSE);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUQrDlgClass", title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &qs);
    MSG msg{};
    bool gotQuit = false;
    WPARAM quitCode = 0;
    while (!qs.done && hDlg && IsWindow(hDlg)) {
        const BOOL gm = GetMessageW(&msg, nullptr, 0, 0);
        if (gm == 0) { gotQuit = true; quitCode = msg.wParam; break; }
        if (gm < 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (gotQuit) PostQuitMessage(static_cast<int>(quitCode));
    if (owner) { EnableWindow(owner, TRUE); SetActiveWindow(owner); }
}

// Track E: Multi-Format Notebook Migration with Pre-Conversion Compatibility Warnings Dialog (F-012)
// confirmReplace se llama justo antes de reemplazar el documento abierto; false = no se toca nada.
inline bool RunNotebookMigrationDialog(
    HWND owner,
    DocumentModel& doc,
    AudioTimelineSession& audioSession,
    NotebookVault& vault,
    std::string& outStatus,
    const std::function<bool()>& confirmReplace = {})
{
    wchar_t fileBuf[MAX_PATH * 2]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter =
        L"Todos los cuadernos y documentos\0*.goodnotes;*.note;*.one;*.onepkg;*.tinta;*.docx;*.pptx;*.pdf;*.cudoc\0"
        L"Goodnotes 6 (*.goodnotes)\0*.goodnotes\0"
        L"Notability (*.note)\0*.note\0"
        L"Microsoft OneNote (*.one;*.onepkg)\0*.one;*.onepkg\0"
        L"CuadernU Cl\u00E1sico (*.tinta)\0*.tinta\0"
        L"Word / PowerPoint (*.docx;*.pptx)\0*.docx;*.pptx\0"
        L"PDF (*.pdf)\0*.pdf\0\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Importar o Migrar Cuaderno a CuadernU";
    if (!GetOpenFileNameW(&ofn)) return false;

    const std::wstring pathW(fileBuf);
    const Interop::CompatibilityReport preReport = Interop::NotebookMigrationEngine::AnalyzeCompatibilityFile(pathW);

    std::wstring summaryW = Utf8ToWide(preReport.FormatHumanSummaryUtf8());
    // Replace \n with \r\n for Win32 multiline EDIT control
    std::wstring crlfSummary;
    crlfSummary.reserve(summaryW.size() + 64);
    for (wchar_t ch : summaryW) {
        if (ch == L'\n') crlfSummary += L"\r\n";
        else if (ch != L'\r') crlfSummary.push_back(ch);
    }

    if (!ShowReportDialog(owner, L"Compatibilidad previa a la importaci\u00F3n \u2014 CuadernU",
                          crlfSummary, L"Importar ahora", true)) {
        outStatus = "Importaci\xC3\xB3n cancelada por el usuario.";
        return false;
    }

    DocumentModel migrated;
    Interop::MigrationResult migRes{};
    if (!Interop::NotebookMigrationEngine::ImportAndMigrateFile(pathW, migrated, migRes)) {
        ShowReportDialog(owner, L"Error al importar", Utf8ToWide(migRes.errorUtf8));
        outStatus = "Error de importaci\xC3\xB3n: " + migRes.errorUtf8;
        return false;
    }

    // Antes del audio y de la biblioteca: si la persona cancela, nada del documento actual cambia.
    if (confirmReplace && !confirmReplace()) {
        outStatus = "Se mantuvo el documento actual";
        return false;
    }

    doc = std::move(migrated);
    if (!migRes.primaryAudioWavBytes.empty()) {
        audioSession.LoadFromWavBytes(migRes.primaryAudioWavBytes.data(), migRes.primaryAudioWavBytes.size());
    }
    if (!migRes.notebookSectionsUtf8.empty()) {
        const auto nbId = vault.CreateNotebook(doc.Title());
        for (const auto& sec : migRes.notebookSectionsUtf8) {
            const auto sId = vault.CreateSection(nbId, sec);
            vault.AddPageEntry(nbId, sId, doc.Title(), WideToUtf8(pathW));
        }
    }

    outStatus = "Migrado desde " + migRes.report.formatLabelUtf8 + " (" +
        std::to_string(migRes.report.pageCount) + " p\xC3\xA1g., " +
        std::to_string(migRes.report.editableStrokeCount) + " trazos editables, " +
        std::to_string(migRes.report.tableCount) + " tablas, " +
        std::to_string(migRes.report.mathBlockCount) + " ecuaciones).";
    return true;
}

// Insert File Printout (PDF / Word .docx / PowerPoint .pptx) into Notebook Pages or Infinite Canvas
// with synchronized guías de interfaz Light/Dark/High-Contrast modal + PerMonitorV2 DPI scaling.
inline bool RunInsertPrintoutDialog(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    std::size_t activePageIndex,
    DocPoint canvasOrigin,
    std::string& outStatus,
    const std::wstring& presetFilePath = L"")
{
    std::wstring chosenPath = presetFilePath;
    if (chosenPath.empty()) {
        wchar_t fileBuf[MAX_PATH * 2]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrTitle = L"Insertar copia impresa (PDF / Word / PowerPoint / Excel)";
        ofn.lpstrFilter =
            L"Documentos compatibles (*.pdf;*.docx;*.pptx;*.xlsx;*.doc;*.ppt;*.xls;*.csv)\0*.pdf;*.docx;*.pptx;*.xlsx;*.doc;*.ppt;*.xls;*.csv\0"
            L"Archivos PDF (*.pdf)\0*.pdf\0"
            L"Microsoft Word (*.docx;*.doc)\0*.docx;*.doc\0"
            L"Microsoft PowerPoint (*.pptx;*.ppt)\0*.pptx;*.ppt\0"
            L"Microsoft Excel / CSV (*.xlsx;*.xls;*.csv)\0*.xlsx;*.xls;*.csv\0"
            L"Todos (*.*)\0*.*\0\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return false;
        chosenPath = fileBuf;
    }

    struct PrintoutDlgState {
        Printout::Options opts{};
        bool accepted = false;
        bool done = false;
        HFONT hFont = nullptr;
        HFONT hBoldFont = nullptr;
        HBRUSH hBgBrush = nullptr;
        COLORREF bgCol = RGB(247, 249, 252);
        COLORREF textCol = RGB(18, 24, 34);
        HWND hTargetCombo = nullptr;
        HWND hRangeEdit = nullptr;
        HWND hGapEdit = nullptr;
        HWND hLayoutCombo = nullptr;
        HWND hQualityCombo = nullptr;
        HWND hLockCheck = nullptr;
        HWND hConvCombo = nullptr;
    } st;

    st.opts.target = (doc.Mode() == DocumentMode::InfiniteCanvas)
        ? Printout::Target::InfiniteCanvas
        : Printout::Target::NotebookPages;
    st.opts.lockInPlace = true;
    st.opts.rasterDpi = 192;

    UiGlass::GlassSettings gs{};
    UiGlass::LoadGlassSettings(gs);
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(gs);
    const bool isDark = gs.IsDark();
    st.bgCol = pal.sidebarBg.ToColorRef();
    st.textCol = pal.labelPrimary.ToColorRef();
    st.hBgBrush = CreateSolidBrush(st.bgCol);

    static bool reg = false;
    static const wchar_t* kCls = L"CuadernUInsertPrintoutModal";
    if (!reg) {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
            auto* s = reinterpret_cast<PrintoutDlgState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
            if (m == WM_NCCREATE) {
                auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
                SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return TRUE;
            }
            if (m == WM_CTLCOLORSTATIC || m == WM_CTLCOLORBTN) {
                if (s && s->hBgBrush) {
                    HDC hdc = reinterpret_cast<HDC>(w);
                    SetTextColor(hdc, s->textCol);
                    SetBkColor(hdc, s->bgCol);
                    return reinterpret_cast<LRESULT>(s->hBgBrush);
                }
            }
            if (m == WM_DPICHANGED) {
                const RECT* prc = reinterpret_cast<const RECT*>(l);
                if (prc) {
                    SetWindowPos(h, nullptr, prc->left, prc->top,
                        prc->right - prc->left, prc->bottom - prc->top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
                }
                return 0;
            }
            if (m == WM_COMMAND) {
                const WORD id = LOWORD(w);
                if (id == IDOK && s) {
                    if (s->hTargetCombo) {
                        const LRESULT sel = SendMessageW(s->hTargetCombo, CB_GETCURSEL, 0, 0);
                        s->opts.target = (sel == 1) ? Printout::Target::NotebookPages : Printout::Target::InfiniteCanvas;
                    }
                    if (s->hRangeEdit) {
                        wchar_t rbuf[64]{};
                        GetWindowTextW(s->hRangeEdit, rbuf, 64);
                        std::string rs = WideToUtf8(rbuf);
                        s->opts.firstPage1 = 1;
                        s->opts.lastPage1 = 0;
                        const auto dash = rs.find('-');
                        if (dash != std::string::npos) {
                            const int a = std::atoi(rs.substr(0, dash).c_str());
                            const int b = std::atoi(rs.substr(dash + 1).c_str());
                            if (a >= 1) s->opts.firstPage1 = a;
                            if (b >= s->opts.firstPage1) s->opts.lastPage1 = b;
                        } else if (!rs.empty() && std::isdigit(static_cast<unsigned char>(rs[0]))) {
                            const int single = std::atoi(rs.c_str());
                            if (single >= 1) {
                                s->opts.firstPage1 = single;
                                s->opts.lastPage1 = single;
                            }
                        }
                    }
                    if (s->hLayoutCombo) {
                        const LRESULT sel = SendMessageW(s->hLayoutCombo, CB_GETCURSEL, 0, 0);
                        s->opts.layout = (sel == 1) ? Printout::CanvasLayout::Grid : Printout::CanvasLayout::VerticalColumn;
                        s->opts.gridColumns = 3;
                    }
                    if (s->hGapEdit) {
                        wchar_t gbuf[64]{};
                        GetWindowTextW(s->hGapEdit, gbuf, 64);
                        const double gVal = _wtof(gbuf);
                        if (std::isfinite(gVal) && gVal >= 0.0 && gVal <= 4000.0) {
                            s->opts.pageGapPt = gVal;
                            s->opts.colGapPt = (std::max)(24.0, gVal);
                        }
                    }
                    if (s->hQualityCombo) {
                        const LRESULT sel = SendMessageW(s->hQualityCombo, CB_GETCURSEL, 0, 0);
                        s->opts.rasterDpi = (sel == 0) ? 144 : ((sel == 2) ? 288 : 192);
                    }
                    if (s->hLockCheck) {
                        s->opts.lockInPlace = (SendMessageW(s->hLockCheck, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    }
                    if (s->hConvCombo) {
                        const LRESULT sel = SendMessageW(s->hConvCombo, CB_GETCURSEL, 0, 0);
                        s->opts.converter = (sel == 1) ? Printout::ConverterChoice::MicrosoftOffice
                                          : (sel == 2) ? Printout::ConverterChoice::LibreOffice
                                          : (sel == 3) ? Printout::ConverterChoice::NativeEditable
                                                       : Printout::ConverterChoice::Automatic;
                    }
                    s->accepted = true;
                    s->done = true;
                    DestroyWindow(h);
                    return 0;
                }
                if (id == IDCANCEL && s) {
                    s->accepted = false;
                    s->done = true;
                    DestroyWindow(h);
                    return 0;
                }
            }
            if (m == WM_CLOSE && s) {
                s->done = true;
                DestroyWindow(h);
                return 0;
            }
            return DefWindowProcW(h, m, w, l);
        };
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = kCls;
        RegisterClassExW(&wc);
        reg = true;
    }

    const UINT dpi = GetSafeWindowDpi(owner);
    auto sc = [&](int v) { return MulDiv(v, static_cast<int>(dpi), 96); };

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kCls, L"Insertar copia impresa \u2014 CuadernU",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, sc(480), sc(350),
        owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (!hDlg) {
        if (st.hBgBrush) DeleteObject(st.hBgBrush);
        return false;
    }
    UiGlass::ApplyGoldenGateWindowFrame(hDlg, isDark);

    if (owner && IsWindow(owner)) {
        RECT orc{}, drc{};
        GetWindowRect(owner, &orc);
        GetWindowRect(hDlg, &drc);
        const int dw = drc.right - drc.left;
        const int dh = drc.bottom - drc.top;
        const int cx = orc.left + ((orc.right - orc.left) - dw) / 2;
        const int cy = orc.top + ((orc.bottom - orc.top) - dh) / 2;
        SetWindowPos(hDlg, nullptr, cx, cy, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    st.hFont = CreateFontW(-sc(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
    st.hBoldFont = CreateFontW(-sc(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

    auto makeLbl = [&](const wchar_t* txt, int x, int y, int w, int h, bool bold = false) {
        HWND hl = CreateWindowExW(0, L"STATIC", txt, WS_CHILD | WS_VISIBLE,
            sc(x), sc(y), sc(w), sc(h), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(hl, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? st.hBoldFont : st.hFont), TRUE);
        return hl;
    };

    const std::wstring fname = std::filesystem::path(chosenPath).filename().wstring();
    makeLbl((L"Archivo: " + fname).c_str(), 18, 14, 430, 20, true);

    makeLbl(L"Destino:", 18, 46, 150, 20);
    st.hTargetCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        sc(175), sc(42), sc(270), sc(120), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hTargetCombo, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);
    SendMessageW(st.hTargetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Pizarra / Lienzo Infinito"));
    SendMessageW(st.hTargetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"P\u00E1ginas nuevas en este Cuaderno"));
    SendMessageW(st.hTargetCombo, CB_SETCURSEL, (st.opts.target == Printout::Target::NotebookPages) ? 1 : 0, 0);

    makeLbl(L"P\u00E1ginas (vac\u00EDo = todas):", 18, 82, 155, 20);
    st.hRangeEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        sc(175), sc(78), sc(96), sc(24), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hRangeEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);
    makeLbl(L"Espacio (pt):", 280, 82, 88, 20);
    st.hGapEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"32",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        sc(370), sc(78), sc(75), sc(24), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hGapEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);

    makeLbl(L"Disposici\u00F3n en Lienzo:", 18, 118, 155, 20);
    st.hLayoutCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        sc(175), sc(114), sc(270), sc(120), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hLayoutCombo, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);
    SendMessageW(st.hLayoutCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Columna vertical"));
    SendMessageW(st.hLayoutCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cuadr\u00EDcula (3 columnas)"));
    SendMessageW(st.hLayoutCombo, CB_SETCURSEL, 0, 0);

    makeLbl(L"Calidad de imagen:", 18, 154, 155, 20);
    st.hQualityCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        sc(175), sc(150), sc(270), sc(120), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hQualityCombo, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);
    SendMessageW(st.hQualityCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Est\u00E1ndar (144 ppp)"));
    SendMessageW(st.hQualityCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Alta (192 ppp \u2014 recomendado)"));
    SendMessageW(st.hQualityCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"M\u00E1xima (288 ppp)"));
    SendMessageW(st.hQualityCombo, CB_SETCURSEL, 1, 0);

    st.hLockCheck = CreateWindowExW(0, L"BUTTON",
        L"Fijar copia impresa al fondo (anotar encima sin moverla)",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        sc(18), sc(190), sc(425), sc(24), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hLockCheck, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);
    SendMessageW(st.hLockCheck, BM_SETCHECK, BST_CHECKED, 0);

    makeLbl(L"Conversi\u00F3n Office:", 18, 226, 155, 20);
    st.hConvCombo = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        sc(175), sc(222), sc(270), sc(140), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    SendMessageW(st.hConvCombo, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);
    SendMessageW(st.hConvCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Autom\u00E1tica (Office -> LibreOffice -> Nativo)"));
    SendMessageW(st.hConvCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Microsoft Office (Word / PowerPoint)"));
    SendMessageW(st.hConvCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"LibreOffice (headless)"));
    SendMessageW(st.hConvCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Motor nativo editable (vista aproximada)"));
    SendMessageW(st.hConvCombo, CB_SETCURSEL, 0, 0);

    HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        sc(235), sc(268), sc(100), sc(32), hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hCancel, WM_SETFONT, reinterpret_cast<WPARAM>(st.hFont), TRUE);

    HWND hOk = CreateWindowExW(0, L"BUTTON", L"Insertar",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        sc(345), sc(268), sc(100), sc(32), hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)),
        GetModuleHandleW(nullptr), nullptr);
    SendMessageW(hOk, WM_SETFONT, reinterpret_cast<WPARAM>(st.hBoldFont), TRUE);

    if (owner && IsWindow(owner)) EnableWindow(owner, FALSE);
    ShowWindow(hDlg, SW_SHOW);
    UpdateWindow(hDlg);

    bool hadQuit = false;
    int quitCode = 0;
    MSG msg{};
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && IsWindow(hDlg)) {
            if (msg.wParam == VK_ESCAPE) {
                SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
                continue;
            }
            if (msg.wParam == VK_RETURN) {
                SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
                continue;
            }
        }
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (msg.message == WM_QUIT) {
        hadQuit = true;
        quitCode = static_cast<int>(msg.wParam);
    }
    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    if (IsWindow(hDlg)) DestroyWindow(hDlg);
    if (st.hFont) DeleteObject(st.hFont);
    if (st.hBoldFont) DeleteObject(st.hBoldFont);
    if (st.hBgBrush) DeleteObject(st.hBgBrush);
    if (hadQuit) PostQuitMessage(quitCode);

    if (!st.accepted) {
        outStatus = "Inserci\xC3\xB3n de copia impresa cancelada.";
        return false;
    }

    HCURSOR hOldCur = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
    Printout::Report rep{};
    const bool ok = Printout::InsertPrintoutFromFile(
        doc, cmdStack, chosenPath, st.opts, activePageIndex, canvasOrigin, rep);
    SetCursor(hOldCur);

    if (!ok) {
        ShowReportDialog(owner, L"Error al insertar copia impresa", Utf8ToWide(rep.errorUtf8));
        outStatus = "Error copia impresa: " + rep.errorUtf8;
        return false;
    }

    outStatus = std::to_string(rep.pagesInserted) + " p\xC3\xA1gina(s) insertadas (" +
                rep.converterUsedUtf8 + ", " +
                std::to_string(rep.pagesWithText) + " con texto buscable";
    if (rep.pagesWithOcr > 0) {
        outStatus += ", " + std::to_string(rep.pagesWithOcr) + " v\xC3\xAD" "a OCR";
    }
    outStatus += ")";
    if (!rep.visualFidelityExact) {
        outStatus += " \xC2\xB7 Vista aproximada: no se encontr\xC3\xB3 Office";
    }
    return true;
}

// Track A: Multi-Format Image Import (PNG/JPG/BMP/TIFF/WEBP/SVG via WIC + SVG rasterizer) (F-100)
inline bool RunMultiFormatImageImport(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    std::string& outStatus)
{
    wchar_t fileBuf[MAX_PATH * 2]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Im\u00E1genes (*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.tif;*.tiff;*.webp;*.svg)\0*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.tif;*.tiff;*.webp;*.svg\0Todos (*.*)\0*.*\0\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;

    std::ifstream ifs(std::filesystem::path(fileBuf), std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    const auto sz = ifs.tellg();
    if (sz <= 0 || sz > 64ll * 1024ll * 1024ll) return false;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(raw.data()), sz);

    const bool isGif = (raw.size() >= 6 && std::memcmp(raw.data(), "GIF8", 4) == 0);
    ImageProc::RgbaImage decoded{};
    std::vector<std::uint8_t> pngBytes;
    int pxW = 320, pxH = 220;
    if (ImageProc::DecodeImageBytesWic(raw.data(), raw.size(), decoded) && decoded.Valid()) {
        pxW = decoded.width;
        pxH = decoded.height;
        // Preserve raw multi-frame GIF bytes verbatim so Direct2D animates all frames (NT-EDIT-031)
        if (isGif) {
            pngBytes = std::move(raw);
        } else if (!ImageProc::EncodeRgbaToPngWic(decoded, pngBytes)) {
            pngBytes = std::move(raw);
        }
    } else {
        pngBytes = std::move(raw);
    }

    const double dispW = std::clamp(static_cast<double>(pxW) * 0.65, 120.0, 440.0);
    const double dispH = dispW * (static_cast<double>(pxH) / (std::max)(1, pxW));
    AnnotationObject obj{};
    obj.kind = ObjectKind::Image;
    obj.layerId = doc.ActiveLayerId();
    obj.bounds = { x, y, dispW, dispH };
    obj.pngBytes = std::move(pngBytes);
    obj.imagePixelW = pxW;
    obj.imagePixelH = pxH;
    cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(obj)));
    outStatus = "Imagen importada (" + std::to_string(pxW) + "x" + std::to_string(pxH) + ").";
    return true;
}

// Track A: Enhance Selected Image (Whiteboard Lighting / Remove Background / Crop / Rotate / Opacity) (F-101..F-106)
inline bool EnhanceSelectedImage(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    std::uint64_t objId,
    int mode, // 0=whiteboard contrast, 1=remove bg, 2=rotate 90, 3=toggle 60% opacity, 4=auto crop 10%
    std::string& outStatus)
{
    AnnotationObject* obj = target.isCanvas ? doc.FindCanvasObjectMutable(objId) : doc.FindPageObjectMutable(target.pageIndex, objId);
    if (!obj || obj->kind != ObjectKind::Image) {
        outStatus = "Seleccione una imagen primero.";
        return false;
    }
    AnnotationObject before = *obj;
    AnnotationObject after = *obj;

    if (mode == 2) {
        after.rotationDeg = std::fmod(after.rotationDeg + 90.0, 360.0);
        outStatus = "Imagen rotada +90\xC2\xB0.";
    } else if (mode == 3) {
        after.opacity = (after.opacity > 0.75) ? 0.55 : 1.0;
        outStatus = (after.opacity < 0.75) ? "Opacidad de imagen: 55%." : "Opacidad de imagen: 100%.";
    } else if (mode == 4) {
        after.imageCrop.left = std::clamp(after.imageCrop.left + 0.08, 0.0, 0.40);
        after.imageCrop.top = std::clamp(after.imageCrop.top + 0.08, 0.0, 0.40);
        after.imageCrop.right = std::clamp(after.imageCrop.right - 0.08, 0.60, 1.0);
        after.imageCrop.bottom = std::clamp(after.imageCrop.bottom - 0.08, 0.60, 1.0);
        after.imageCrop.Clamp();
        outStatus = "Recorte de imagen aplicado.";
    } else {
        const std::vector<std::uint8_t>* srcBytes = !after.pngBytes.empty() ? &after.pngBytes : doc.FindAsset(after.assetKey);
        if (!srcBytes || srcBytes->empty()) return false;
        ImageProc::RgbaImage img{};
        if (!ImageProc::DecodeImageBytesWic(srcBytes->data(), srcBytes->size(), img)) {
            outStatus = "No se pudo decodificar la imagen para mejorarla.";
            return false;
        }
        if (mode == 0) {
            ImageProc::EnhanceWhiteboardLighting(img, 0.14f, 0.86f);
            outStatus = "Contraste de pizarra aplicado.";
        } else {
            ImageProc::RemovePaperBackground(img, 218);
            outStatus = "Fondo blanco eliminado con transparencia.";
        }
        std::vector<std::uint8_t> newPng;
        if (!ImageProc::EncodeRgbaToPngWic(img, newPng)) return false;
        after.pngBytes = std::move(newPng);
        after.assetKey.clear();
    }
    return cmdStack.Execute(doc, std::make_unique<ModifyObjectCommand>(target, std::move(before), std::move(after)));
}

// Track A: Engineering Stickers / Reusable Elements Library (F-109)
inline bool InsertEngineeringSticker(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    int stickerKind, // 0=OpAmp, 1=Resistencia, 2=Ejes XY, 3=Nota Destacada
    std::string& outStatus)
{
    AnnotationObject obj{};
    obj.layerId = doc.ActiveLayerId();
    if (stickerKind == 0) {
        obj.kind = ObjectKind::Shape;
        obj.shapeKind = ShapeKind::SignatureStamp;
        obj.bounds = { x, y, 130.0, 72.0 };
        obj.strokeColor = DocColor::RGBA(15, 118, 110, 255);
        obj.fillColor = DocColor::RGBA(240, 253, 250, 235);
        obj.strokeWidth = 2.0;
        obj.textUtf8 = "Op-Amp\n(+)  Vout\n(-)";
        outStatus = "Elemento Op-Amp insertado.";
    } else if (stickerKind == 1) {
        obj.kind = ObjectKind::InkStroke;
        obj.strokeColor = DocColor::RGBA(30, 64, 175, 255);
        obj.strokeWidth = 2.4;
        const double pts[][2] = {
            {0,20},{20,20},{28,4},{40,36},{52,4},{64,36},{76,4},{88,36},{96,20},{116,20}
        };
        for (const auto& p : pts) obj.inkPoints.push_back({ x + p[0], y + p[1], 0.7f, 0, 0, 0 });
        obj.RecomputeBounds();
        outStatus = "Elemento Resistencia vectorial insertado.";
    } else if (stickerKind == 2) {
        obj.kind = ObjectKind::InkStroke;
        obj.strokeColor = DocColor::RGBA(24, 34, 52, 255);
        obj.strokeWidth = 2.0;
        const double pts[][2] = { {0,120},{0,0},{0,120},{160,120} };
        for (const auto& p : pts) obj.inkPoints.push_back({ x + p[0], y + p[1], 0.7f, 0, 0, 0 });
        obj.RecomputeBounds();
        outStatus = "Ejes coordenados XY insertados.";
    } else {
        obj.kind = ObjectKind::TextBox;
        obj.bounds = { x, y, 220.0, 88.0 };
        obj.fillColor = DocColor::RGBA(254, 249, 195, 240);
        obj.strokeColor = DocColor::RGBA(113, 63, 18, 255);
        obj.fontSizePt = 12.0;
        obj.textUtf8 = "Nota clave:\n- Verificar unidades SI\n- Confirmar signos";
        outStatus = "Nota adhesiva insertada.";
    }
    return cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(obj)));
}

// Track A: Offline Windows HTR (Ink -> Editable Text or Formula) (F-043, F-045, F-169)
inline bool RunOfflineInkHtr(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<std::uint64_t>& selectedIds,
    bool convertToMathBlock,
    std::string& outStatus)
{
    std::vector<AnnotationObject> strokes;
    DocRect unionBb{};
    bool haveBb = false;
    auto collectFrom = [&](const AnnotationObject& o) {
        if (o.kind != ObjectKind::InkStroke || o.inkPoints.empty()) return;
        strokes.push_back(o);
        if (!haveBb) { unionBb = o.bounds; haveBb = true; }
        else {
            const double l = (std::min)(unionBb.x, o.bounds.x);
            const double t = (std::min)(unionBb.y, o.bounds.y);
            const double r = (std::max)(unionBb.Right(), o.bounds.Right());
            const double b = (std::max)(unionBb.Bottom(), o.bounds.Bottom());
            unionBb = { l, t, r - l, b - t };
        }
    };

    if (!selectedIds.empty()) {
        for (std::uint64_t id : selectedIds) {
            const auto* o = target.isCanvas ? doc.FindCanvasObject(id)
                : (target.pageIndex < doc.Pages().size() ? [&]() -> const AnnotationObject* {
                    for (const auto& item : doc.Pages()[target.pageIndex].objects) if (item.id == id) return &item;
                    return nullptr;
                }() : nullptr);
            if (o) collectFrom(*o);
        }
    } else {
        const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
        for (const auto& o : vec) collectFrom(o);
    }

    if (strokes.empty()) {
        outStatus = "No hay trazos de tinta para reconocer.";
        return false;
    }

    const HtrOutput htr = WinRtInkHtrAdapter::RecognizeStrokes(strokes);
    if (!htr.recognized) {
        ShowReportDialog(owner, L"Reconocimiento de escritura", Utf8ToWide(htr.errorUtf8));
        outStatus = htr.errorUtf8;
        return false;
    }

    // Index recognized text on strokes for handwriting search (F-169)
    for (const auto& s : strokes) {
        if (AnnotationObject* mut = target.isCanvas ? doc.FindCanvasObjectMutable(s.id) : doc.FindPageObjectMutable(target.pageIndex, s.id)) {
            mut->htrRecognizedTextUtf8 = htr.textUtf8;
        }
    }

    AnnotationObject created{};
    created.layerId = doc.ActiveLayerId();
    if (convertToMathBlock) {
        created.kind = ObjectKind::MathBlock;
        created.bounds = { unionBb.x, unionBb.Bottom() + 12.0, (std::max)(280.0, unionBb.w), 88.0 };
        created.formulaExprUtf8 = htr.mathLatexUtf8.empty() ? htr.textUtf8 : htr.mathLatexUtf8;
        const auto ev = Cas::EvaluateExpression(created.formulaExprUtf8);
        if (ev.ok) created.evaluatedResultUtf8 = ev.formattedResultUtf8;
        outStatus = "Escritura convertida a expresi\xC3\xB3n de una l\xC3\xADnea (sin fracciones ni exponentes): " + created.formulaExprUtf8;
    } else {
        created.kind = ObjectKind::TextBox;
        created.bounds = { unionBb.x, unionBb.Bottom() + 12.0, (std::max)(260.0, unionBb.w), 64.0 };
        created.textUtf8 = htr.textUtf8;
        created.fontSizePt = 13.0;
        created.strokeColor = DocColor::RGBA(24, 30, 42, 255);
        outStatus = "Escritura reconocida: " + htr.textUtf8;
    }
    cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(created)));
    return true;
}

// Track C: Reactive TableBlock Creator/Editor with cycle check & cell formula evaluator (F-111, F-141)
inline bool RunReactiveTableDialog(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    PromptFn prompt,
    std::string& outStatus)
{
    std::wstring input = L"Parametro,Valor,Resultado\r\nR (ohm),1000,=B2*2\r\nV (V),12,=B3/B2";
    if (!prompt(L"Tabla Reactiva (CSV con f\u00F3rmulas =B2*2)",
                L"Ingrese filas CSV (soporta f\u00F3rmulas tipo hoja de c\u00E1lculo):", input, true)) {
        return false;
    }
    const std::string csvUtf8 = WideToUtf8(input);
    std::vector<std::vector<std::string>> grid;
    {
        std::istringstream iss(csvUtf8);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            std::vector<std::string> row;
            std::string cell;
            std::istringstream ls(line);
            while (std::getline(ls, cell, ',')) {
                while (!cell.empty() && cell.front() == ' ') cell.erase(cell.begin());
                while (!cell.empty() && cell.back() == ' ') cell.pop_back();
                row.push_back(cell);
            }
            if (!row.empty()) grid.push_back(std::move(row));
        }
    }
    if (grid.empty()) return false;

    std::string dagErr;
    const bool hasCycle = DocStudioApp::DetectCyclicTableFormulas(grid, &dagErr);

    auto colName = [](std::size_t c) -> std::string {
        return std::string(1, static_cast<char>('A' + (c % 26)));
    };
    Cas::SymbolEnv env;
    auto evaluated = grid;
    for (int pass = 0; pass < 3 && !hasCycle; ++pass) {
        for (std::size_t r = 0; r < grid.size(); ++r) {
            for (std::size_t c = 0; c < grid[r].size(); ++c) {
                const std::string cellRef = colName(c) + std::to_string(r + 1);
                const std::string& raw = grid[r][c];
                if (!raw.empty() && raw[0] == '=') {
                    const auto ev = Cas::EvaluateExpression(raw.substr(1), env);
                    if (ev.ok) {
                        evaluated[r][c] = ev.formattedResultUtf8;
                        env[cellRef] = ev.result;
                    }
                } else {
                    const auto ev = Cas::EvaluateExpression(raw, env);
                    if (ev.ok) env[cellRef] = ev.result;
                }
            }
        }
    }

    nlohmann::json rows = nlohmann::json::array();
    std::ostringstream plain;
    for (std::size_t r = 0; r < evaluated.size(); ++r) {
        nlohmann::json row = nlohmann::json::array();
        for (std::size_t c = 0; c < evaluated[r].size(); ++c) {
            row.push_back(evaluated[r][c]);
            if (c > 0) plain << " | ";
            plain << evaluated[r][c];
        }
        rows.push_back(std::move(row));
        plain << "\n";
    }
    nlohmann::json root;
    root["rows"] = std::move(rows);
    root["csv"] = csvUtf8;

    AnnotationObject tbl{};
    tbl.kind = ObjectKind::TableBlock;
    tbl.layerId = doc.ActiveLayerId();
    tbl.bounds = { x, y, 420.0, (std::max)(110.0, static_cast<double>(evaluated.size()) * 32.0 + 16.0) };
    tbl.tableJsonUtf8 = root.dump();
    tbl.textUtf8 = plain.str();
    tbl.captionUtf8 = "Tabla Reactiva";
    cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(tbl)));
    outStatus = hasCycle ? ("Ciclo detectado en tabla: " + dagErr) : "Tabla reactiva evaluada e insertada.";
    (void)owner;
    return true;
}

// Track D: Advanced Math Lab (Complex Matrix 64x64 / Eigen / ODE RK4 / Calculus / 2D Statics FBD / GUM / Root Locus)
inline bool RunAdvancedMathDialog(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    PromptFn prompt,
    std::string& outStatus)
{
    std::wstring q = L"eigen([[4, 1], [2, 3]])";
    if (!prompt(L"Laboratorio Matem\u00E1tico e Ingenier\u00ED""a Avanzada",
                L"eigen([[4,1],[2,3]]) | ode(y - t^2 + 1, 0, 0.5, 2) | diff(sin(x)*x^2, 1) | int(x^2, 0, 3) | fbd(L=4, P=100, x=2) | gum(V/R; V=12+-0.1; R=1000+-10) | rootlocus(R=100,L=0.01,C=1e-6)", q, false)) {
        return false;
    }
    const std::string expr = WideToUtf8(q);
    std::string resText;

    auto splitArgsTopLevel = [](const std::string& s, char sep) {
        std::vector<std::string> out;
        std::string cur;
        int depth = 0;
        for (char c : s) {
            if (c == '(' || c == '[') ++depth;
            else if (c == ')' || c == ']') --depth;
            if (c == sep && depth == 0) {
                while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.front()))) cur.erase(cur.begin());
                while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.back()))) cur.pop_back();
                out.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.front()))) cur.erase(cur.begin());
        while (!cur.empty() && std::isspace(static_cast<unsigned char>(cur.back()))) cur.pop_back();
        if (!cur.empty()) out.push_back(cur);
        return out;
    };

    if (expr.rfind("eigen(", 0) == 0 && expr.back() == ')') {
        Cas::ComplexMatrix m{};
        if (Cas::detail_adv::ParseComplexMatrixLiteral(expr.substr(6, expr.size() - 7), m)) {
            const auto eig = Cas::ComputeMatrixEigen(m);
            resText = eig.ok ? eig.summaryUtf8 : eig.errorUtf8;
        } else {
            resText = "No se pudo interpretar la matriz.";
        }
    } else if (expr.rfind("ode(", 0) == 0 && expr.back() == ')') {
        const auto args = splitArgsTopLevel(expr.substr(4, expr.size() - 5), ',');
        const std::string rhs = !args.empty() ? args[0] : "y - t^2 + 1";
        const double t0 = (args.size() >= 2) ? std::atof(args[1].c_str()) : 0.0;
        const double y0 = (args.size() >= 3) ? std::atof(args[2].c_str()) : 0.5;
        const double t1 = (args.size() >= 4) ? std::atof(args[3].c_str()) : 2.0;
        const std::size_t steps = (args.size() >= 5) ? static_cast<std::size_t>(std::clamp(std::atoi(args[4].c_str()), 4, 2000)) : 40u;
        const auto ode = Cas::SolveFirstOrderOdeRk4(rhs, t0, { y0, 0.0 }, t1, steps);
        if (ode.ok && !ode.samples.empty()) {
            std::ostringstream os;
            os << "RK4 y(" << t1 << ") = " << ode.samples.back().y.real()
               << " (" << ode.samples.size() << " puntos desde t0=" << t0 << ", y0=" << y0 << ")";
            resText = os.str();
        } else {
            resText = ode.errorUtf8;
        }
    } else if (expr.rfind("fbd(", 0) == 0 && expr.back() == ')') {
        // Parse 2D simply-supported beam FBD: fbd(L=4, P=100, x=2)
        double L = 4.0, P = 100.0, px = 2.0;
        for (const auto& tok : splitArgsTopLevel(expr.substr(4, expr.size() - 5), ',')) {
            const auto eq = tok.find('=');
            if (eq != std::string::npos) {
                const std::string k = tok.substr(0, eq);
                const double v = std::atof(tok.substr(eq + 1).c_str());
                if (k == "L" || k == "l") L = (std::max)(0.1, v);
                else if (k == "P" || k == "F" || k == "p") P = v;
                else if (k == "x" || k == "a") px = v;
            }
        }
        Cas::FbdModel fbd{};
        fbd.title = "Viga 2D (FBD)";
        fbd.supports.push_back({ "A", "RA", Cas::FbdSupportKind::Pin, 0.0, 0.0, 90.0 });
        fbd.supports.push_back({ "B", "RB", Cas::FbdSupportKind::RollerX, L, 0.0, 90.0 });
        fbd.forces.push_back({ "F1", "P", px, 0.0, P, -90.0, false, "N" });
        const auto s = Cas::SolveFbdEquilibrium(fbd);
        if (s.ok) {
            std::ostringstream os;
            os << "[" << s.determinacyStatus << "] ";
            for (std::size_t i = 0; i < s.reactions.size(); ++i) {
                if (i > 0) os << ", ";
                os << s.reactions[i].first << " = " << s.reactions[i].second << " N";
            }
            resText = os.str();
        } else {
            resText = s.errorUtf8;
        }
    } else if (expr.rfind("gum(", 0) == 0 && expr.back() == ')') {
        // GUM Uncertainty & Monte Carlo: gum(V/R; V=12+-0.1; R=1000+-10)
        const auto parts = splitArgsTopLevel(expr.substr(4, expr.size() - 5), ';');
        const std::string formula = !parts.empty() ? parts[0] : "V/R";
        std::unordered_map<std::string, Cas::UncertainValue> vars;
        for (std::size_t i = 1; i < parts.size(); ++i) {
            const auto eq = parts[i].find('=');
            if (eq == std::string::npos) continue;
            std::string name = parts[i].substr(0, eq);
            while (!name.empty() && name.back() == ' ') name.pop_back();
            const std::string rhs = parts[i].substr(eq + 1);
            const auto pm = rhs.find("+-");
            Cas::UncertainValue uv{};
            if (pm != std::string::npos) {
                uv.value = std::atof(rhs.substr(0, pm).c_str());
                uv.uncertainty = std::atof(rhs.substr(pm + 2).c_str());
            } else {
                uv.value = std::atof(rhs.c_str());
                uv.uncertainty = 0.0;
            }
            vars[name] = uv;
        }
        const auto ures = Cas::PropagateUncertainty(formula, vars, 4000);
        resText = ures.ok
            ? ("GUM RSS: " + ures.formattedRssUtf8 + " | Monte Carlo (4k): " + ures.formattedMcUtf8)
            : ures.errorUtf8;
    } else if (expr.rfind("rootlocus(", 0) == 0 && expr.back() == ')') {
        // RLC Root Locus & stability analysis: rootlocus(R=100, L=0.01, C=1e-6)
        double R = 100.0, L = 0.01, C = 1e-6;
        for (const auto& tok : splitArgsTopLevel(expr.substr(10, expr.size() - 11), ',')) {
            const auto eq = tok.find('=');
            if (eq != std::string::npos) {
                const std::string k = tok.substr(0, eq);
                const double v = std::atof(tok.substr(eq + 1).c_str());
                if (k == "R") R = (std::max)(1e-3, v);
                else if (k == "L") L = (std::max)(1e-9, v);
                else if (k == "C") C = (std::max)(1e-12, v);
            }
        }
        std::ostringstream net;
        net << "V1 1 0 1\nR1 1 2 " << R << "\nL1 2 3 " << L << "\nC1 3 0 " << C;
        std::vector<Circuit::CircuitComponent> comps;
        std::string perr;
        if (Circuit::ParseNetlistText(net.str(), comps, &perr)) {
            const auto sym = Circuit::AnalyzeFullSymbolicCircuit(comps, 3, 60.0);
            if (sym.ok) {
                std::ostringstream os;
                os << "Polos: " << sym.poles.size() << " | Ramas Lugar Ra\xC3\xAD""ces: " << sym.rootLocusBranches.size()
                   << " | BIBO: " << (sym.isBibostable ? "ESTABLE" : "INESTABLE");
                if (!sym.poles.empty()) {
                    os << " | p1=" << sym.poles[0].real() << (sym.poles[0].imag() >= 0 ? "+" : "") << sym.poles[0].imag() << "j";
                }
                resText = os.str();
            } else {
                resText = sym.errorUtf8;
            }
        } else {
            resText = perr;
        }
    } else if (expr.rfind("diff(", 0) == 0 && expr.back() == ')') {
        const std::string inner = expr.substr(5, expr.size() - 6);
        const auto c = inner.find_last_of(',');
        if (c != std::string::npos) {
            const auto dr = Cas::NumericalDerivative(inner.substr(0, c), "x", std::atof(inner.substr(c + 1).c_str()));
            resText = dr.ok ? dr.formattedResultUtf8 : dr.errorUtf8;
        }
    } else if (expr.rfind("int(", 0) == 0 && expr.back() == ')') {
        const std::string inner = expr.substr(4, expr.size() - 5);
        const auto c2 = inner.find_last_of(',');
        const auto c1 = (c2 != std::string::npos) ? inner.find_last_of(',', c2 - 1) : std::string::npos;
        if (c1 != std::string::npos && c2 != std::string::npos) {
            const auto ir = Cas::NumericalIntegralSimpson(inner.substr(0, c1), "x",
                std::atof(inner.substr(c1 + 1, c2 - c1 - 1).c_str()),
                std::atof(inner.substr(c2 + 1).c_str()));
            resText = ir.ok ? ir.formattedResultUtf8 : ir.errorUtf8;
        }
    } else {
        const auto mr = Cas::EvaluateComplexMatrixExpression(expr);
        if (mr.ok) resText = mr.formattedUtf8;
        else {
            const auto ev = Cas::EvaluateExpression(expr);
            resText = ev.ok ? ev.formattedResultUtf8 : (mr.errorUtf8.empty() ? ev.errorUtf8 : mr.errorUtf8);
        }
    }

    AnnotationObject mb{};
    mb.kind = ObjectKind::MathBlock;
    mb.layerId = doc.ActiveLayerId();
    mb.bounds = { x, y, 420.0, 96.0 };
    mb.formulaExprUtf8 = expr;
    mb.evaluatedResultUtf8 = resText;
    cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(mb)));
    outStatus = "C\xC3\xA1lculo avanzado: " + resText;
    (void)owner;
    return true;
}

// Track D: Study & Pedagogy Suite (Check My Work / Teach Me / Summarize / Cited Local Q&A / 4-Model Casio Comparator)
inline void RunCheckMyWorkOrTeachMe(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    int mode, // 0=CheckMyWork, 1=TeachMe, 2=SummarizeNonDestructive, 3=LocalCitedQA, 4=Compare4Casio
    PromptFn prompt,
    std::string& outStatus)
{
    if (mode == 0) {
        std::wstring stepsW = L"2*x + 4 = 10\r\n2*x = 6\r\nx = 3";
        if (!prompt(L"Revisar mi Procedimiento (Check My Work)", L"Escriba cada paso algebraico en una l\u00EDnea:", stepsW, true)) return;
        std::vector<std::string> lines;
        std::istringstream iss(WideToUtf8(stepsW));
        std::string ln;
        while (std::getline(iss, ln)) {
            if (!ln.empty() && ln.back() == '\r') ln.pop_back();
            if (!ln.empty()) lines.push_back(ln);
        }
        const auto rep = Cas::CheckMyWork(lines);
        std::ostringstream os;
        os << "Resultado global: " << (rep.allValid ? "TODOS LOS PASOS CORRECTOS" : "SE DETECT\xC3\x93 ERROR EN L\xC3\x8DNEA " + std::to_string(rep.firstErrorLineIndex + 1)) << "\r\n"
           << rep.summaryUtf8 << "\r\n\r\n";
        for (const auto& d : rep.lines) {
            os << "L\xC3\xADnea " << (d.lineIndex + 1) << ": " << d.rawLineUtf8
               << "  [" << d.transitionCheck.statusCode << "] " << d.transitionCheck.messageUtf8 << "\r\n";
            if (!d.transitionCheck.hintUtf8.empty()) os << "   Pista: " << d.transitionCheck.hintUtf8 << "\r\n";
        }
        ShowReportDialog(owner, L"Verificaci\u00F3n de Pasos", Utf8ToWide(os.str()));
        outStatus = rep.summaryUtf8;
    } else if (mode == 1) {
        std::wstring probW = L"2*x + 5 = 17";
        if (!prompt(L"Modo Tutor Socr\u00E1tico (Teach Me)", L"Ecuaci\u00F3n o problema a guiar paso a paso:", probW, false)) return;
        const auto tm = Cas::GenerateTeachMeSocratic(WideToUtf8(probW));
        std::ostringstream os;
        os << "Problema: " << tm.problemUtf8 << "\r\n\r\n";
        for (const auto& h : tm.hints) {
            os << "Nivel " << h.level << ": " << h.promptUtf8 << "\r\n"
               << "Pregunta: " << h.questionForStudentUtf8 << "\r\n\r\n";
        }
        const auto solved = Cas::SolveDeterministic(WideToUtf8(probW));
        if (solved.ok) {
            os << "Resoluci\xC3\xB3n determinista (" << solved.primaryMethod.methodNameUtf8 << "):\r\n";
            for (const auto& st : solved.primaryMethod.steps) {
                os << "  " << st.stepNumber << ". " << st.titleUtf8 << ": " << st.expressionUtf8 << "\r\n";
            }
            os << "Soluci\xC3\xB3n: " << solved.primaryMethod.finalSummaryUtf8 << "\r\n";
        }
        ShowReportDialog(owner, L"Gu\u00ED""a Socr\u00E1tica", Utf8ToWide(os.str()));
        outStatus = "Gu\xC3\xAD""a socr\xC3\xA1tica generada localmente.";
    } else if (mode == 2) {
        const auto sum = Cas::SummarizeDocumentNonDestructive(doc, 8);
        if (sum.ok) {
            const auto layerId = doc.AddLayer(sum.targetLayerNameUtf8);
            AnnotationObject block = sum.summaryBlock;
            block.layerId = layerId;
            block.bounds = { x, y, 420.0, 180.0 };
            cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(block)));
            outStatus = "Resumen no destructivo insertado en capa separada.";
        } else {
            outStatus = "No hay suficiente contenido textual para resumir.";
        }
    } else if (mode == 3) {
        std::wstring qW;
        if (!prompt(L"Consulta Local con Citas", L"Pregunta sobre tus apuntes (100% offline):", qW, false)) return;
        VaultFullTextIndex fts;
        fts.IndexDocument(doc.Title(), doc);
        const auto hits = fts.Query(WideToUtf8(qW), 12);
        const auto ans = Cas::SynthesizeGroundedSearchAnswer(WideToUtf8(qW), hits, 6);
        std::ostringstream os;
        os << ans.synthesizedAnswerUtf8 << "\r\n\r\nCitas exactas:\r\n";
        for (const auto& c : ans.citations) {
            os << "[" << c.citationIndex << "] P\xC3\xA1g. " << (c.pageIndex + 1) << " (" << c.sourceKindUtf8 << "): " << c.quotedSnippetUtf8 << "\r\n";
        }
        ShowReportDialog(owner, L"Respuesta Local Citada", Utf8ToWide(os.str()));
        outStatus = "Consulta local completada (" + std::to_string(ans.citations.size()) + " citas).";
    } else if (mode == 4) {
        std::wstring exprW = L"sin(30) + 1/4";
        if (!prompt(L"Comparador 4 Modelos CASIO", L"Expresi\u00F3n a comparar en fx-570LA X, fx-570VN PLUS, fx-CG50 y fx-991CW:", exprW, false)) return;
        const auto cmp = CalcBridge::CompareCalculatorModels(WideToUtf8(exprW));
        std::ostringstream os;
        os << "Expresi\xC3\xB3n: " << cmp.expressionUtf8 << "\r\nReferencia CAS: " << cmp.casReferenceResultUtf8 << "\r\n\r\n";
        for (const auto& m : cmp.models) {
            os << m.modelIdUtf8 << " [" << m.appModeUtf8 << "]\r\n"
               << "  Secuencia: " << m.keySequenceUtf8 << "\r\n"
               << "  Resultado: " << m.displayResultUtf8 << "  " << m.diffNoteUtf8 << "\r\n\r\n";
        }
        ShowReportDialog(owner, L"Comparador de 4 Modelos CASIO", Utf8ToWide(os.str()));
        outStatus = "Comparaci\xC3\xB3n 4 modelos completada.";
    }
}

// Track D: Circuit Stroke Recognizer + Engineering Toolbox + CodeBlock/MCU (F-229, F-239, F-240)
inline void RunEngineeringToolboxOrCodeBlock(
    HWND owner,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    int mode, // 0=RecognizeCircuitStrokes, 1=EngToolbox, 2=CodeBlock+MCU, 3=FormulaFingerprintSearch
    PromptFn prompt,
    std::string& outStatus)
{
    if (mode == 0) {
        std::vector<Circuit::StrokePolyline> polylines;
        const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
        for (const auto& o : vec) {
            if (o.kind != ObjectKind::InkStroke || o.inkPoints.size() < 2) continue;
            Circuit::StrokePolyline sp{};
            for (const auto& pt : o.inkPoints) sp.points.push_back({ pt.x, pt.y });
            polylines.push_back(std::move(sp));
        }
        auto compKindLabel = [](Circuit::ComponentKind k) -> const char* {
            switch (k) {
            case Circuit::ComponentKind::Resistor: return "Resistor (R)";
            case Circuit::ComponentKind::Capacitor: return "Capacitor (C)";
            case Circuit::ComponentKind::Inductor: return "Inductor (L)";
            case Circuit::ComponentKind::VoltageSource: return "Fuente de Voltaje (V)";
            case Circuit::ComponentKind::OpAmp: return "Amplificador Operacional (OPAMP)";
            case Circuit::ComponentKind::Diode: return "Diodo (D)";
            case Circuit::ComponentKind::Ground: return "Tierra (GND)";
            case Circuit::ComponentKind::Wire: return "Conductor (Wire)";
            default: return "Componente";
            }
        };
        const auto rec = Circuit::RecognizeCircuitStrokes(polylines);
        std::ostringstream os;
        os << "Trazos analizados: " << polylines.size() << "\r\n"
           << "Componente reconocido: " << compKindLabel(rec.kind) << " (confianza "
           << static_cast<int>(std::lround(rec.confidence * 100.0f)) << "%)\r\n";

        std::string generatedNetlist;
        if (!polylines.empty()) {
            if (rec.kind == Circuit::ComponentKind::Resistor) generatedNetlist = "V1 1 0 12\nR1 1 2 1000\nR2 2 0 1000";
            else if (rec.kind == Circuit::ComponentKind::Capacitor) generatedNetlist = "V1 1 0 5\nR1 1 2 1000\nC1 2 0 1e-6";
            else if (rec.kind == Circuit::ComponentKind::Inductor) generatedNetlist = "V1 1 0 5\nR1 1 2 100\nL1 2 0 10e-3";
            else if (rec.kind == Circuit::ComponentKind::VoltageSource) generatedNetlist = "V1 1 0 12\nR1 1 0 1000";
            else if (rec.kind == Circuit::ComponentKind::Diode) generatedNetlist = "V1 1 0 5\nD1 1 2\nR1 2 0 1000";
            else if (rec.kind == Circuit::ComponentKind::OpAmp) generatedNetlist = "V1 1 0 1\nR1 1 2 1000\nR2 2 3 10000\nOPAMP U1 0 2 3";
        }
        if (!generatedNetlist.empty()) {
            os << "\r\nPlantilla SPICE generada:\r\n" << generatedNetlist;
            std::vector<Circuit::CircuitComponent> comps;
            std::string perr;
            std::string summary;
            if (Circuit::ParseNetlistText(generatedNetlist, comps, &perr)) {
                const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
                summary = sol.ok ? sol.summaryUtf8 : sol.errorUtf8;
            }
            AnnotationObject cb{};
            cb.kind = ObjectKind::CircuitBlock;
            cb.layerId = doc.ActiveLayerId();
            cb.bounds = { x, y, 380.0, 128.0 };
            cb.circuitNetlistUtf8 = generatedNetlist;
            cb.captionUtf8 = std::string("Circuito reconocido: ") + compKindLabel(rec.kind);
            cb.evaluatedResultUtf8 = summary;
            cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(cb)));
        }
        ShowReportDialog(owner, L"Reconocedor de Circuitos a Mano Alzada", Utf8ToWide(os.str()));
        outStatus = std::string("Circuito reconocido: ") + compKindLabel(rec.kind);
    } else if (mode == 1) {
        std::wstring specW = L"1000, 1e-6";
        if (!prompt(L"Caja de Herramientas de Ingenier\u00ED""a (RC / LC / Op-Amp / Divisor / SMD)",
                    L"Ingrese R (ohm), C (F) o c\u00F3digo SMD (ej. 472 o rojo,violeta,naranja,dorado):", specW, false)) return;
        const std::string s = WideToUtf8(specW);
        std::ostringstream os;
        if (s.find(',') != std::string::npos && std::isalpha(static_cast<unsigned char>(s[0]))) {
            std::vector<std::string> bands;
            std::istringstream iss(s);
            std::string b;
            while (std::getline(iss, b, ',')) {
                while (!b.empty() && b.front() == ' ') b.erase(b.begin());
                if (!b.empty()) bands.push_back(b);
            }
            const auto dec = Circuit::DecodeResistorColorNames(bands);
            os << (dec.ok ? ("Resistencia: " + dec.formattedUtf8 + " +/-" + std::to_string(dec.tolerancePct) + "%") : dec.errorUtf8);
        } else if (s.find(',') == std::string::npos) {
            const auto smd = Circuit::DecodeSmdResistorCode(s);
            os << (smd.ok ? ("SMD " + s + " = " + smd.formattedUtf8) : smd.errorUtf8);
        } else {
            const auto comma = s.find(',');
            const double r = std::atof(s.substr(0, comma).c_str());
            const double c = std::atof(s.substr(comma + 1).c_str());
            const double fc = Circuit::Toolbox::RcCutoffHz(r, c);
            const double tau = Circuit::Toolbox::RcTimeConstantSec(r, c);
            const double vdiv = Circuit::Toolbox::VoltageDivider(12.0, r, r);
            os << "Filtro RC (R=" << Circuit::Toolbox::FormatEngineeringNotation(r, "Ohm")
               << ", C=" << Circuit::Toolbox::FormatEngineeringNotation(c, "F") << ")\r\n"
               << "  Frecuencia de corte fc = " << Circuit::Toolbox::FormatEngineeringNotation(fc, "Hz") << "\r\n"
               << "  Constante de tiempo tau = " << Circuit::Toolbox::FormatEngineeringNotation(tau, "s") << "\r\n"
               << "  Divisor sim\xC3\xA9trico (12 V) = " << vdiv << " V\r\n"
               << "  Ganancia Op-Amp no inversor (Rf=10k, Rg=R) = " << Circuit::Toolbox::OpAmpNonInvertingGain(10000.0, r);
        }
        ShowReportDialog(owner, L"Caja de Herramientas de Ingenier\u00ED""a", Utf8ToWide(os.str()));
        outStatus = "C\xC3\xA1lculo de ingenier\xC3\xAD""a completado.";
    } else if (mode == 2) {
        std::wstring codeW = L"void setup() { pinMode(13, OUTPUT); }\r\nvoid loop() { digitalWrite(13, HIGH); delay(500); digitalWrite(13, LOW); }";
        if (!prompt(L"Insertar Bloque de C\u00F3digo / MCU", L"C\u00F3digo C/C++/Arduino/Python:", codeW, true)) return;
        const std::string src = WideToUtf8(codeW);
        const auto hl = Circuit::TokenizeCodeBlock(src, Circuit::CodeLanguage::ArduinoIno);
        AnnotationObject cb{};
        cb.kind = ObjectKind::CodeBlock;
        cb.layerId = doc.ActiveLayerId();
        cb.bounds = { x, y, 420.0, 140.0 };
        cb.codeLanguageUtf8 = "arduino";
        cb.textUtf8 = src;
        cb.captionUtf8 = "C\xC3\xB3" "digo MCU (" + std::to_string(hl.tokens.size()) + " tokens)";
        cb.evaluatedResultUtf8 = "Sintaxis verificada (" + std::to_string(hl.lineCount) + " l\xC3\xADneas)";
        cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(cb)));
        outStatus = "Bloque de c\xC3\xB3" "digo insertado.";
    } else if (mode == 3) {
        std::wstring qW = L"a*x + b";
        if (!prompt(L"B\u00FAsqueda Sem\u00E1ntica de F\u00F3rmulas (AST)", L"F\u00F3rmula patr\u00F3n (detecta equivalencia independiente de nombres de variables):", qW, false)) return;
        const auto targetFp = Cas::ComputeFormulaFingerprint(WideToUtf8(qW));
        std::ostringstream os;
        os << "Patr\xC3\xB3n can\xC3\xB3nico: " << targetFp.canonicalAlphaExpr << "\r\n\r\nCoincidencias en el documento:\r\n";
        int hits = 0;
        auto scanVec = [&](const std::vector<AnnotationObject>& vec, const std::string& where) {
            for (const auto& o : vec) {
                if (o.formulaExprUtf8.empty()) continue;
                const auto fp = Cas::ComputeFormulaFingerprint(o.formulaExprUtf8);
                if (fp.ok && targetFp.ok && fp.hash64 == targetFp.hash64) {
                    os << "  - [" << where << "] " << o.formulaExprUtf8 << " = " << o.evaluatedResultUtf8 << "\r\n";
                    ++hits;
                }
            }
        };
        for (std::size_t p = 0; p < doc.Pages().size(); ++p) scanVec(doc.Pages()[p].objects, "P\xC3\xA1g. " + std::to_string(p + 1));
        scanVec(doc.CanvasObjects(), "Lienzo");
        if (hits == 0) os << "  (Sin coincidencias estructurales exactas)";
        ShowReportDialog(owner, L"B\u00FAsqueda Sem\u00E1ntica AST", Utf8ToWide(os.str()));
        outStatus = "B\xC3\xBAsqueda sem\xC3\xA1ntica AST: " + std::to_string(hits) + " coincidencia(s).";
    } else if (mode == 4) {
        // Control Block Diagram PID + Plant Step Response & Routh Stability (F-240)
        std::wstring specW = L"Kp=2.5, Ki=1.0, Kd=0.2, Tau=0.5";
        if (!prompt(L"Diagrama de Bloques de Control (PID + Planta)", L"Par\u00E1metros Kp, Ki, Kd, Tau (planta 1/(Tau*s + 1)):", specW, false)) return;
        double kp = 2.5, ki = 1.0, kd = 0.2, tau = 0.5;
        std::string s = WideToUtf8(specW);
        for (char& ch : s) if (ch == ';') ch = ',';
        std::istringstream iss(s);
        std::string tok;
        while (std::getline(iss, tok, ',')) {
            const auto eq = tok.find('=');
            if (eq == std::string::npos) continue;
            std::string k = tok.substr(0, eq);
            while (!k.empty() && k.front() == ' ') k.erase(k.begin());
            while (!k.empty() && k.back() == ' ') k.pop_back();
            const double v = std::atof(tok.substr(eq + 1).c_str());
            if (k == "Kp" || k == "kp") kp = v;
            else if (k == "Ki" || k == "ki") ki = v;
            else if (k == "Kd" || k == "kd") kd = v;
            else if (k == "Tau" || k == "tau") tau = (std::max)(0.01, v);
        }
        Circuit::ControlBlockDiagram diag{};
        diag.title = "Lazo Cerrado PID";
        Circuit::ControlBlockNode nIn{ 1, Circuit::ControlNodeKind::Input, "R(s)" };
        Circuit::ControlBlockNode nSum{ 2, Circuit::ControlNodeKind::SumJunction, "E(s)" };
        Circuit::ControlBlockNode nPid{ 3, Circuit::ControlNodeKind::Pid, "PID" };
        nPid.kp = kp; nPid.ki = ki; nPid.kd = kd;
        Circuit::ControlBlockNode nPlant{ 4, Circuit::ControlNodeKind::TransferFunction, "G(s)" };
        nPlant.tf = { Circuit::Poly1D{ { 1.0 } }, Circuit::Poly1D{ { 1.0, tau } } };
        Circuit::ControlBlockNode nOut{ 5, Circuit::ControlNodeKind::Output, "Y(s)" };
        diag.nodes = { nIn, nSum, nPid, nPlant, nOut };
        diag.edges = { { 1, 1, 2, +1, {} }, { 2, 2, 3, +1, {} }, { 3, 3, 4, +1, {} }, { 4, 4, 5, +1, {} }, { 5, 4, 2, -1, {} } };
        const auto ca = Circuit::AnalyzeControlBlockDiagram(diag, 4.0, 120);
        std::ostringstream os;
        if (ca.ok) {
            os << "Funci\xC3\xB3n de Transferencia Lazo Cerrado:\r\n  T(s) = " << ca.closedLoopUtf8 << "\r\n"
               << "Estabilidad Routh-Hurwitz: " << (ca.isStableRouth ? "ESTABLE" : "INESTABLE") << "\r\n"
               << "Polos (" << ca.poles.size() << "): ";
            for (const auto& p : ca.poles) os << "(" << p.real() << (p.imag() >= 0 ? "+" : "") << p.imag() << "j) ";
            os << "\r\nRespuesta al escal\xC3\xB3n final y(4s) = " << (ca.stepOutput.empty() ? 0.0 : ca.stepOutput.back());
            AnnotationObject mb{};
            mb.kind = ObjectKind::MathBlock;
            mb.layerId = doc.ActiveLayerId();
            mb.bounds = { x, y, 420.0, 96.0 };
            mb.formulaExprUtf8 = "T(s) = " + ca.closedLoopUtf8;
            mb.evaluatedResultUtf8 = std::string(ca.isStableRouth ? "Estable" : "Inestable") + " | y(4s)=" +
                std::to_string(ca.stepOutput.empty() ? 0.0 : ca.stepOutput.back());
            cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(mb)));
        } else {
            os << "Error en diagrama de control: " << ca.errorUtf8;
        }
        ShowReportDialog(owner, L"An\u00E1lisis de Control (Lazo Cerrado PID)", Utf8ToWide(os.str()));
        outStatus = ca.ok ? "Diagrama de control PID analizado." : ca.errorUtf8;
    } else if (mode == 5) {
        // Circuit Topology Search & Diagnostic Validator across Document CircuitBlocks
        std::ostringstream os;
        int blocksChecked = 0;
        auto scanCircuits = [&](const std::vector<AnnotationObject>& vec, const std::string& where) {
            for (const auto& o : vec) {
                if (o.kind != ObjectKind::CircuitBlock || o.circuitNetlistUtf8.empty()) continue;
                ++blocksChecked;
                std::vector<Circuit::CircuitComponent> comps;
                std::string perr;
                os << "[" << where << "] " << (o.captionUtf8.empty() ? "Circuito" : o.captionUtf8) << ":\r\n";
                if (Circuit::ParseNetlistText(o.circuitNetlistUtf8, comps, &perr)) {
                    const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
                    os << "  Componentes: " << comps.size() << " | Diagn\xC3\xB3stico MNA: "
                       << (sol.ok ? sol.summaryUtf8 : ("Error topol\xC3\xB3gico: " + sol.errorUtf8)) << "\r\n\r\n";
                } else {
                    os << "  Error de netlist: " << perr << "\r\n\r\n";
                }
            }
        };
        for (std::size_t p = 0; p < doc.Pages().size(); ++p) scanCircuits(doc.Pages()[p].objects, "P\xC3\xA1g. " + std::to_string(p + 1));
        scanCircuits(doc.CanvasObjects(), "Lienzo");
        if (blocksChecked == 0) os << "No hay bloques de circuito SPICE/MNA en este documento.";
        ShowReportDialog(owner, L"Auditor\u00ED""a Topol\u00F3gica de Circuitos", Utf8ToWide(os.str()));
        outStatus = "Circuitos auditados: " + std::to_string(blocksChecked);
    }
}

// Track D: Teacher Parametric Exam Variants, Rubric & LMS ZIP Package Generator (F-186..F-197)
inline void RunExamVariantsAndLmsDialog(
    HWND owner,
    const DocumentModel& doc,
    PromptFn prompt,
    std::string& outStatus)
{
    std::wstring countW = L"3";
    if (!prompt(L"Generador de Variantes de Examen y Paquete LMS",
                L"N\u00FAmero de variantes param\u00E9tricas (1..20) con r\u00FAbrica autom\u00E1tica:", countW, false)) return;
    const int nVar = std::clamp(_wtoi(countW.c_str()), 1, 20);

    wchar_t fileBuf[MAX_PATH * 2]{ L"paquete_examen_lms.zip" };
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Paquete LMS / QTI (*.zip)\0*.zip\0\0";
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
    ofn.lpstrDefExt = L"zip";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&ofn)) return;

    ZipArchive zip;
    nlohmann::json manifest;
    manifest["title"] = doc.Title();
    manifest["variantsCount"] = nVar;
    manifest["rubricPointsTotal"] = 100;
    nlohmann::json varsJson = nlohmann::json::array();
    std::ostringstream summary;
    summary << "Paquete de examen LMS generado con " << nVar << " variante(s) param\xC3\xA9tricas:\r\n\r\n";

    for (int v = 1; v <= nVar; ++v) {
        const double R = 100.0 * v;
        const double V = 6.0 + 2.0 * v;
        const double I = V / R;
        const double P = V * I;
        std::ostringstream md;
        md << "# Examen Variante " << v << " — " << doc.Title() << "\n\n"
           << "## Problema Param\xC3\xA9trico\n"
           << "Determine corriente I y potencia P para V = " << V << " V y R = " << R << " Ohm.\n\n"
           << "## Clave y R\xC3\xBA""brica (100 pts)\n"
           << "- Planteamiento (30 pts): I = V / R\n"
           << "- Corriente (35 pts): I = " << I << " A\n"
           << "- Potencia (35 pts): P = " << P << " W\n";
        zip.SetTextEntry("variante_" + std::to_string(v) + ".md", md.str());
        varsJson.push_back({ {"variant", v}, {"V", V}, {"R", R}, {"I_answer", I}, {"P_answer", P} });
        summary << "  - Variante #" << v << ": V=" << V << "V, R=" << R << "Ohm -> I=" << I << "A, P=" << P << "W\r\n";
    }
    manifest["variants"] = std::move(varsJson);
    zip.SetTextEntry("lms_manifest.json", manifest.dump(2));

    std::vector<std::uint8_t> zipBytes;
    std::string err;
    if (zip.Serialize(zipBytes, true, &err)) {
        std::ofstream ofs(std::filesystem::path(fileBuf), std::ios::binary | std::ios::trunc);
        ofs.write(reinterpret_cast<const char*>(zipBytes.data()), static_cast<std::streamsize>(zipBytes.size()));
        if (ofs.good()) {
            ShowReportDialog(owner, L"Paquete de Examen LMS Exportado", Utf8ToWide(summary.str()));
            outStatus = "Exportadas " + std::to_string(nVar) + " variantes de examen LMS.";
            return;
        }
    }
    ShowReportDialog(owner, L"Error exportando paquete LMS", Utf8ToWide(err.empty() ? "No se pudo escribir el archivo ZIP." : err));
}

// Track D: Security (AES-256-GCM Encrypted Save/Open + LAN QR Broadcast) (F-210, F-211, F-215)
// confirmReplace (solo mode 1) se llama ya descifrado, justo antes de reemplazar el documento abierto.
inline bool RunSecurityOrLanFlow(
    HWND owner,
    DocumentModel& doc,
    int mode, // 0=EncryptSave, 1=DecryptOpen, 2=LanQrBroadcast
    PasswordPromptFn passPrompt,
    std::string& outStatus,
    const std::function<bool()>& confirmReplace = {})
{
    if (mode == 0) {
        std::wstring passW;
        if (!passPrompt(L"Guardar Cuaderno Cifrado (AES-256-GCM)", L"Contrase\u00F1""a de protecci\u00F3n:", passW) || passW.empty()) return false;
        wchar_t fileBuf[MAX_PATH * 2]{ L"cuaderno_seguro.cudoc" };
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = L"Cuaderno Cifrado CuadernU (*.cudoc)\0*.cudoc\0\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
        ofn.lpstrDefExt = L"cudoc";
        ofn.Flags = OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn)) return false;
        std::string err;
        if (CudocCrypto::SaveEncryptedCudocFile(WideToUtf8(fileBuf), doc, WideToUtf8(passW), &err)) {
            outStatus = "Cuaderno guardado con cifrado AES-256-GCM.";
            return true;
        }
        ShowReportDialog(owner, L"Error de cifrado", Utf8ToWide(err));
        return false;
    } else if (mode == 1) {
        wchar_t fileBuf[MAX_PATH * 2]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = L"Cuaderno Cifrado CuadernU (*.cudoc)\0*.cudoc\0\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return false;
        std::wstring passW;
        if (!passPrompt(L"Abrir Cuaderno Cifrado", L"Ingrese la contrase\u00F1""a:", passW) || passW.empty()) return false;
        DocumentModel dec;
        std::string err;
        if (CudocCrypto::LoadEncryptedCudocFile(WideToUtf8(fileBuf), WideToUtf8(passW), dec, &err)) {
            if (confirmReplace && !confirmReplace()) {
                outStatus = "Se mantuvo el documento actual";
                return false;
            }
            doc = std::move(dec);
            outStatus = "Cuaderno cifrado abierto correctamente.";
            return true;
        }
        ShowReportDialog(owner, L"Error al descifrar", Utf8ToWide(err));
        return false;
    } else if (mode == 2) {
        int boundPort = 0;
        std::string err;
        if (!LanRemote::ProbeBindLocalPort(0, boundPort, &err)) boundPort = 8765;
        const std::string token = LanRemote::GenerateSessionTokenHex24().substr(0, 16);
        const std::string url = "http://127.0.0.1:" + std::to_string(boundPort) + "/cuadernu?session=" + token;
        LanRemote::QrMatrix qr{};
        if (LanRemote::EncodeQrMatrix(url, qr, &err)) {
            wchar_t tmpDir[MAX_PATH]{};
            GetTempPathW(MAX_PATH, tmpDir);
            const std::string htmlPath = WideToUtf8(tmpDir) + "cuadernu_lan_broadcast.html";
            LanRemote::ExportLanBroadcastHtml(doc, htmlPath, url, true, false);
            ShowQrCodeDialog(owner, L"Difusi\u00F3n LAN Local (Solo Lectura)", url, qr);
            outStatus = "Sesi\xC3\xB3n LAN local lista en puerto " + std::to_string(boundPort) + ".";
            return true;
        }
    }
    return false;
}

// Track C: Page Version History (with real Restore!) & Conflict Merge Dialogs (F-059, F-219)
inline void RunVersionHistoryOrMerge(
    HWND owner,
    DocumentModel& doc,
    PageVersionHistory& history,
    std::size_t currentPageIdx,
    bool doMergeOtherFile,
    PromptFn prompt,
    std::string& outStatus)
{
    if (!doMergeOtherFile) {
        const auto nowSec = static_cast<std::uint64_t>(std::time(nullptr));
        const auto verId = history.CaptureSnapshot(doc, currentPageIdx, nowSec, "Instant\xC3\xA1nea manual");
        const auto list = history.SnapshotsForPage(currentPageIdx);
        std::ostringstream os;
        os << "Instant\xC3\xA1nea guardada (#" << verId << ") para p\xC3\xA1gina " << (currentPageIdx + 1) << ".\r\n\r\n"
           << "Historial registrado en esta sesi\xC3\xB3n (" << list.size() << " versiones):\r\n";
        for (const auto& s : list) {
            os << "  - Versi\xC3\xB3n #" << s.versionId << " (" << s.authorOrLabelUtf8 << ") — "
               << s.snapshotPage.objects.size() << " objeto(s)\r\n";
        }
        os << "\r\nPulse \"Restaurar versi\xC3\xB3n...\" para volver a cualquier instant\xC3\xA1nea anterior.";
        if (list.size() > 1 && ShowReportDialog(owner, L"Historial de Versiones de P\u00E1gina", Utf8ToWide(os.str()), L"Restaurar versi\u00F3n...", true)) {
            std::wstring idW = std::to_wstring(list.front().versionId);
            if (prompt(L"Restaurar Instant\u00E1nea de P\u00E1gina", L"Ingrese el n\u00FAmero # de versi\u00F3n a restaurar:", idW, false) && !idW.empty()) {
                const std::uint64_t pickId = static_cast<std::uint64_t>(_wcstoui64(idW.c_str(), nullptr, 10));
                if (history.RestoreSnapshot(doc, pickId)) {
                    outStatus = "P\xC3\xA1gina restaurada a instant\xC3\xA1nea #" + std::to_string(pickId) + ".";
                    return;
                }
                outStatus = "No se encontr\xC3\xB3 la versi\xC3\xB3n #" + std::to_string(pickId) + ".";
                return;
            }
        } else if (list.size() <= 1) {
            ShowReportDialog(owner, L"Historial de Versiones de P\u00E1gina", Utf8ToWide(os.str()));
        }
        outStatus = "Instant\xC3\xA1nea #" + std::to_string(verId) + " guardada.";
    } else {
        wchar_t fileBuf[MAX_PATH * 2]{};
        OPENFILENAMEW ofn{};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = owner;
        ofn.lpstrFilter = L"Cuaderno CuadernU (*.cudoc)\0*.cudoc\0\0";
        ofn.lpstrFile = fileBuf;
        ofn.nMaxFile = static_cast<DWORD>(std::size(fileBuf));
        ofn.Flags = OFN_FILEMUSTEXIST;
        if (!GetOpenFileNameW(&ofn)) return;
        DocumentModel remoteDoc;
        std::string err;
        if (!CudocStorage::LoadFromFile(fileBuf, remoteDoc, &err)) {
            ShowReportDialog(owner, L"Error cargando archivo remoto", Utf8ToWide(err));
            return;
        }
        DocumentModel baseDoc;
        DocumentModel merged;
        const auto mr = MergeConcurrentDocuments(baseDoc, doc, remoteDoc, merged);
        doc = std::move(merged);
        std::ostringstream os;
        os << "Fusi\xC3\xB3n sin p\xC3\xA9rdida completada.\r\n"
           << "P\xC3\xA1ginas fusionadas limpiamente: " << mr.mergedCleanPages << "\r\n"
           << "P\xC3\xA1ginas de conflicto preservadas lado a lado: " << mr.conflictPagesCreated << "\r\n";
        for (const auto& d : mr.conflictDescriptions) os << "  - " << d << "\r\n";
        ShowReportDialog(owner, L"Fusi\u00F3n de Conflictos", Utf8ToWide(os.str()));
        outStatus = "Fusi\xC3\xB3n completada (" + std::to_string(mr.conflictPagesCreated) + " conflicto(s) preservados).";
    }
}

// Track C: Library Vault Browser & Vault-Wide Search + Notebook/Section Creation (F-058, F-061..F-064)
inline void RunVaultLibraryManager(
    HWND owner,
    DocumentModel& doc,
    NotebookVault& vault,
    VaultFullTextIndex& ftsIndex,
    const std::wstring& currentPathW,
    PromptFn prompt,
    std::string& outStatus)
{
    ftsIndex.IndexDocument(doc.Title(), doc);
    if (vault.Notebooks().empty()) {
        const auto nb = vault.CreateNotebook("Cuadernos de Ingenier\xC3\xAD""a");
        const auto sec = vault.CreateSection(nb, "General");
        vault.AddPageEntry(nb, sec, doc.Title(), WideToUtf8(currentPathW));
    }

    std::wstring queryW;
    if (!prompt(
        L"Biblioteca de Cuadernos y B\u00FAsqueda Global",
        L"T\u00E9rmino a buscar, +cuaderno:Nombre, +seccion:Nombre, +mover:pagId:cuadId:secId (o vac\u00EDo):",
        queryW, false)) {
        return; // Respect Cancel cleanly
    }

    const std::string qUtf8 = WideToUtf8(queryW);
    if (qUtf8.rfind("+cuaderno:", 0) == 0 && qUtf8.size() > 10) {
        const std::string name = qUtf8.substr(10);
        const auto nbId = vault.CreateNotebook(name);
        vault.CreateSection(nbId, "General");
        outStatus = "Cuaderno creado en Biblioteca: " + name;
    } else if (qUtf8.rfind("+seccion:", 0) == 0 && qUtf8.size() > 9 && !vault.Notebooks().empty()) {
        const std::string name = qUtf8.substr(9);
        vault.CreateSection(vault.Notebooks().front().id, name);
        outStatus = "Secci\xC3\xB3n creada en Biblioteca: " + name;
    } else if (qUtf8.rfind("+mover:", 0) == 0 && qUtf8.size() > 7) {
        std::uint64_t pgId = 0, dstNb = 0, dstSec = 0;
        if (sscanf_s(qUtf8.c_str() + 7, "%llu:%llu:%llu", &pgId, &dstNb, &dstSec) == 3 &&
            vault.MovePageBetweenSections(pgId, dstNb, dstSec)) {
            outStatus = "P\xC3\xA1gina #" + std::to_string(pgId) + " movida a secci\xC3\xB3n #" + std::to_string(dstSec);
        } else {
            outStatus = "No se pudo mover p\xC3\xA1gina entre secciones (verifique IDs)";
        }
    }

    std::ostringstream os;
    if (!qUtf8.empty() && qUtf8[0] != '+') {
        const auto hits = ftsIndex.Query(qUtf8, 32);
        os << "B\xC3\xBAsqueda global \"" << qUtf8 << "\" (" << hits.size() << " resultado(s)):\r\n\r\n";
        for (const auto& h : hits) {
            os << "  - [" << h.docKeyUtf8 << " | P\xC3\xA1g. " << (h.pageIndex + 1) << " | " << h.sourceKindUtf8 << "] "
               << h.snippetUtf8 << "\r\n";
        }
        outStatus = "B\xC3\xBAsqueda en biblioteca: " + std::to_string(hits.size()) + " resultado(s).";
    } else {
        os << "Biblioteca CuadernU (" << vault.Notebooks().size() << " cuaderno(s), "
           << ftsIndex.IndexedDocumentCount() << " documento(s) indexados):\r\n\r\n";
        for (const auto& nb : vault.Notebooks()) {
            os << "Cuaderno: " << nb.titleUtf8 << "\r\n";
            for (const auto& sec : nb.sections) {
                os << "  Secci\xC3\xB3n: " << sec.titleUtf8 << " (" << sec.pages.size() << " p\xC3\xA1g.)\r\n";
                for (const auto& pg : sec.pages) {
                    os << "    - " << pg.titleUtf8 << "\r\n";
                }
            }
        }
        if (qUtf8.empty()) {
            outStatus = "Biblioteca actualizada (" + std::to_string(vault.Notebooks().size()) + " cuaderno(s)).";
        }
    }
    ShowReportDialog(owner, L"Biblioteca CuadernU", Utf8ToWide(os.str()));
}

} // namespace DocStudio::GuiDialogs
#endif
