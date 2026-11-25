#pragma once
#include <windowsx.h>
#include <algorithm>
#include <cwchar>
#include "key_capture_rects.h"
#include "operation_capture.h"
#include "ui_theme.h"

// Forward declarations implemented in settings_and_exporter.h / main.cpp / learning_studio.h
static void OpenSettingsWindow();
static void OpenSequenceExporterWindow(bool syncHistoryFirst);
static std::wstring BuildLiveHistoryLatexPreview(int calcIdx);
static HBITMAP CaptureLcdSnapshot(int calcIdx, int zoomScale, int& outW, int& outH);
static void CopyCalculatorResultSmart(int calcIdx, int modeOverride, bool updateStatus);
static void ShowToolbarToast(const std::wstring& msg);
inline void OpenExplorerForKey(int calcIdx, int keyId, bool pinCard, int layer);
inline int GetDockedExplorerHeight(int availClientH);
inline void ClearDockedExplorerHitRects();
inline void DrawDockedExplorerInspector(HDC hdc, const RECT& cardRc);
inline bool HandleDockedExplorerClick(int lx, int ly);
inline void ShowCalculatorContextMenu(HWND hwndOwner, int clientX, int clientY, int calcIdx, int keyId, int indSpriteIdx, bool indLit, bool isLcdBody, int timelineStepIdx);

struct KeyCaptureCacheEntry { HBITMAP bitmap=nullptr; int width=0,height=0; };
static KeyCaptureCacheEntry g_keyCaptures[4][50];
static RECT g_histQuickBtnClear{};
static RECT g_histQuickBtnCfg{};
static RECT g_histQuickBtnExp{};
static RECT g_histQuickBtnClose{};

struct HistoryKeyHitItem {
    RECT rect{};
    int calcIdx = 0;
    int keyId = 0;
    int logIdx = 0;
    bool isAppCapture = false;
    std::wstring captureFilePath;
};
inline std::vector<HistoryKeyHitItem> g_histKeyHitItems;

struct StepTimelineSnapshot {
    int calcIdx = 0;
    int keyId = 0;
    std::wstring caption;
    std::wstring note;
    std::wstring indicatorsSummary;
    std::vector<uint8_t> emuState;
    HBITMAP hLcdBmp = nullptr;
    int lcdW = 0;
    int lcdH = 0;
    RECT hitRect{};
};
inline std::vector<StepTimelineSnapshot> g_timelineSnapshots;
inline int  g_timelineCursor = -1;
inline bool g_timelinePlaying = false;
inline int  g_timelineSpeedIdx = 1; // 0=0.5x(1200ms), 1=1x(700ms), 2=2x(350ms)
inline bool g_timelineSyncCalc = false;

struct PlayerControlRects {
    RECT firstBtn{};
    RECT prevBtn{};
    RECT playBtn{};
    RECT nextBtn{};
    RECT lastBtn{};
    RECT speedBtn{};
    RECT syncBtn{};
    RECT restoreBtn{};
    RECT noteBtn{};
    RECT exportPngBtn{};
};
inline PlayerControlRects g_playerBtns;
inline std::wstring g_playerStatusMsg;

static constexpr UINT_PTR ID_TIMER_TIMELINE_PLAY = 2009;

inline void ClearTimelineSnapshots()
{
    g_timelinePlaying = false;
    if (g_hHistoryWnd) KillTimer(g_hHistoryWnd, ID_TIMER_TIMELINE_PLAY);
    for (auto& snap : g_timelineSnapshots) {
        if (snap.hLcdBmp) {
            DeleteObject(snap.hLcdBmp);
            snap.hLcdBmp = nullptr;
        }
    }
    g_timelineSnapshots.clear();
    g_timelineCursor = -1;
    g_playerStatusMsg.clear();
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
}

void ReleaseKeyCaptures()
{
    for(auto& model:g_keyCaptures)for(auto& key:model){
        if(key.bitmap)DeleteObject(key.bitmap);
        key={};
    }
    ClearTimelineSnapshots();
}

static KeyCaptureCacheEntry& KeyCaptureForHistory(HDC dc,int model,int keyId)
{
    static KeyCaptureCacheEntry kInvalidEntry{};
    if (model < 0 || model >= 4 || keyId < 0 || keyId >= 50) return kInvalidEntry;
    auto& entry=g_keyCaptures[model][keyId];
    if(entry.bitmap)return entry;
    KeyVisual key{};
    if(!FillKeyVisual(model,keyId,key))return entry;
    auto& calculator=g_nativeCalcs[model];
    if (!calculator.hSkinBmp || calculator.skinW <= 0 || calculator.skinH <= 0) return entry;
    auto spec=CalculatorKeyCaptureSpec(model,keyId,key.cx,key.cy,key.w,key.h,
                                     calculator.skinW,calculator.skinH);
    entry.width=spec.bounds.right-spec.bounds.left;
    entry.height=spec.bounds.bottom-spec.bounds.top;
    entry.bitmap=CreateCalculatorKeySnapshot(dc,calculator.hSkinBmp,model,spec);
    if (entry.bitmap && CasioTheme::IsDirectionalKeyId(model, keyId)) {
        CasioTheme::DrawDirectionalKeyHighlightBitmap(
            entry.bitmap, entry.width, entry.height,
            spec.bounds.left, spec.bounds.top,
            model, keyId, key.cx, key.cy, key.w, key.h);
    }
    return entry;
}

inline bool DrawHistoryKeyThumbInRect(HDC hdc, int calcIdx, int keyId, const RECT& rSlot, HDC memKeyDc)
{
    const auto& keyCap = KeyCaptureForHistory(hdc, calcIdx, keyId);
    if (!keyCap.bitmap || keyCap.width <= 0 || keyCap.height <= 0) return false;
    HDC dcUse = memKeyDc ? memKeyDc : CreateCompatibleDC(hdc);
    if (!dcUse) return false;
    const int availW = (std::max)(10, static_cast<int>(rSlot.right - rSlot.left) - 4);
    const int availH = (std::max)(10, static_cast<int>(rSlot.bottom - rSlot.top) - 4);
    const double sc = (std::min)(static_cast<double>(availW) / keyCap.width, static_cast<double>(availH) / keyCap.height);
    const int dw = (std::max)(8, static_cast<int>(keyCap.width * sc));
    const int dh = (std::max)(8, static_cast<int>(keyCap.height * sc));
    const int dx = rSlot.left + (static_cast<int>(rSlot.right - rSlot.left) - dw) / 2;
    const int dy = rSlot.top + (static_cast<int>(rSlot.bottom - rSlot.top) - dh) / 2;
    HGDIOBJ oldKb = SelectObject(dcUse, keyCap.bitmap);
    SetStretchBltMode(hdc, HALFTONE);
    SetBrushOrgEx(hdc, 0, 0, nullptr);
    StretchBlt(hdc, dx, dy, dw, dh, dcUse, 0, 0, keyCap.width, keyCap.height, SRCCOPY);
    SelectObject(dcUse, oldKb);
    if (!memKeyDc) DeleteDC(dcUse);
    return true;
}

static constexpr int HISTORY_HEADER_HEIGHT=136;
static constexpr int HISTORY_CELL_HEIGHT=116;

inline int GetHistoryHeaderHeight()
{
    return BuildLiveHistoryLatexPreview(g_activeCalcIdx).empty() ? 84 : HISTORY_HEADER_HEIGHT;
}

inline float GetHistoryUiScale(int physClientW)
{
    float s = CasioTheme::g_uiScale;
    int desired = CasioTheme::UiToDevice(356, s);
    if (physClientW > 0 && desired > physClientW) {
        s = (std::max)(1.0f, s * (static_cast<float>(physClientW) / static_cast<float>(desired)));
    }
    return s;
}

inline void ComputeHistoryHeaderLayout(
    int clientW, RECT& segOuter, RECT& tabKeys, RECT& tabCaps, RECT& tabPlay,
    RECT& btnCfg, RECT& btnExp, RECT& latexBox)
{
    const int avail = (std::max)(180, clientW - 32);
    segOuter = { 16, 44, 16 + avail, 74 };
    const int tw = (avail - 8) / 3;
    tabKeys = { segOuter.left + 2, segOuter.top + 2, segOuter.left + 2 + tw, segOuter.bottom - 2 };
    tabCaps = { tabKeys.right + 2, segOuter.top + 2, tabKeys.right + 2 + tw, segOuter.bottom - 2 };
    tabPlay = { tabCaps.right + 2, segOuter.top + 2, segOuter.right - 2, segOuter.bottom - 2 };

    btnCfg = { clientW - 112, 9, clientW - 46, 35 };   // Centro
    btnExp = { clientW - 40,  9, clientW - 14, 35 };   // Cerrar panel (x)
    latexBox = { 16, 82, 16 + avail, 128 };
}

inline RECT ComputeHistoryClearBtnRect(int clientW)
{
    return RECT{ clientW - 144, 9, clientW - 118, 35 }; // Borrar historial
}

static const wchar_t* KeyHistoryCaption(int calcIdx, int keyId)
{
    KeyVisual kv{};
    if (!FillKeyVisual(calcIdx, keyId, kv) || !kv.name) return L"?";
    const wchar_t* name = kv.name;
    auto isEq = [name](const wchar_t* s) { return wcscmp(name, s) == 0; };
    if (isEq(L"MAS")) return L"+";
    if (isEq(L"MENOS")) return L"−";
    if (isEq(L"POR")) return L"×";
    if (isEq(L"ENTRE")) return L"÷";
    if (isEq(L"IGUAL")) return L"=";
    if (isEq(L"PUNTO")) return L".";
    if (isEq(L"COMA")) return L",";
    if (isEq(L"PAREN_A") || isEq(L"PAREN_ABRE")) return L"(";
    if (isEq(L"PAREN_C") || isEq(L"PAREN_CIERRA")) return L")";
    if (isEq(L"X_THETA")) return L"X, θ, T";
    if (isEq(L"POT") || isEq(L"POTENCIA")) return L"xʸ";
    if (isEq(L"CUADRADO") || isEq(L"X^2")) return L"x²";
    if (isEq(L"FRAC") || isEq(L"FRACCION")) return L"Fracción";
    if (isEq(L"RAIZ")) return L"√";
    if (isEq(L"INVERSO")) return L"x⁻¹";
    if (isEq(L"NEGATIVO") || (calcIdx == 2 && keyId == 48)) return L"(−)";
    if (isEq(L"LOG_B")) return L"log";
    if (isEq(L"MAS_M")) return L"M+";
    if (isEq(L"VARIABLE")) return L"x";
    if (isEq(L"INTEGRAL")) return L"∫";
    if (isEq(L"GRADOS")) return L"° ′ ″";
    if (isEq(L"SD")) return L"S⇔D";
    if (isEq(L"FLECHA")) return L"→";
    if (isEq(L"ARRIBA")) return L"↑ Arriba";
    if (isEq(L"IZQUIERDA")) return L"← Izq.";
    if (isEq(L"DERECHA")) return L"Der. →";
    if (isEq(L"ABAJO")) return L"↓ Abajo";
    if (isEq(L"X_VAR")) return L"x";
    if (isEq(L"ATRAS")) return L"Atr\u00E1s";
    if (isEq(L"RE_PAG")) return L"ReP\u00E1g";
    if (isEq(L"AV_PAG")) return L"AvP\u00E1g";
    if (calcIdx == 3 && isEq(L"EXP")) return L"\u00D710\u207F";
    return name;
}

static const wchar_t* KeyHistoryCaption(const KeyLogEntry& item)
{
    if (item.isAppCapture) {
        return !item.captureLabel.empty() ? item.captureLabel.c_str() : item.keyName.c_str();
    }
    const wchar_t* mapped = KeyHistoryCaption(item.calcIdx, item.keyId);
    if (wcscmp(mapped, L"?") != 0) return mapped;
    return item.keyName.c_str();
}

inline std::wstring BuildLitIndicatorsSummary(int calcIdx)
{
    if (calcIdx == 2) return L"LCD Color fx-CG50";
    if (calcIdx == 3) return L"LCD ClassWiz fx-991CW";
    if (calcIdx < 0 || calcIdx > 1 || !g_nativeCalcs[calcIdx].emu) return L"-";
    const size_t rawSz = calcemu_screen_buffer_size(g_nativeCalcs[calcIdx].emu);
    if (rawSz == 0) return L"-";
    std::vector<unsigned char> rawBuf(rawSz);
    if (calcemu_screen_buffer(g_nativeCalcs[calcIdx].emu, rawBuf.data(), rawBuf.size()) != rawSz) {
        return L"-";
    }
    std::wstring res;
    const int count = CasioLcdIndicators::GetIndicatorCount(calcIdx);
    for (int i = 0; i < count; ++i) {
        const auto* sp = CasioLcdIndicators::GetIndicatorSpec(calcIdx, i);
        if (!sp || static_cast<size_t>(sp->rawByteOffset) >= rawSz) continue;
        if ((rawBuf[sp->rawByteOffset] & sp->bitMask) != 0) {
            const auto* doc = CasioKnowledge::FindIndicatorDoc(calcIdx, i);
            if (!res.empty()) res += L" ";
            res += doc ? doc->symbol : L"*";
        }
    }
    return res.empty() ? L"(ninguno activo)" : res;
}

inline void RecordTimelineStep(int calcIdx, int keyId)
{
    if (calcIdx < 0 || calcIdx > 3 || keyId < 0 || keyId >= 50) return;

    if (g_timelineSnapshots.size() >= 240) {
        if (g_timelineSnapshots.front().hLcdBmp) {
            DeleteObject(g_timelineSnapshots.front().hLcdBmp);
        }
        g_timelineSnapshots.erase(g_timelineSnapshots.begin());
        if (g_timelineCursor > 0) g_timelineCursor--;
    }

    StepTimelineSnapshot snap;
    snap.calcIdx = calcIdx;
    snap.keyId = keyId;
    snap.caption = KeyHistoryCaption(calcIdx, keyId);
    if (const auto* rec = CasioKnowledge::FindKeyRecord(calcIdx, keyId)) {
        snap.note = rec->normal.label + L" - " + rec->normal.summary;
    } else if (calcIdx == 3) {
        snap.note = L"fx-991CW - Tecla " + snap.caption;
    } else {
        snap.note = L"Tecla " + snap.caption;
    }
    snap.indicatorsSummary = BuildLitIndicatorsSummary(calcIdx);

    if (calcIdx < 2 && g_nativeCalcs[calcIdx].emu) {
        size_t sz = calcemu_save_state(g_nativeCalcs[calcIdx].emu, nullptr, 0);
        if (sz > 0) {
            snap.emuState.resize(sz);
            if (calcemu_save_state(g_nativeCalcs[calcIdx].emu, snap.emuState.data(), snap.emuState.size()) != sz) {
                snap.emuState.clear();
            }
        }
    }
    snap.hLcdBmp = CaptureLcdSnapshot(calcIdx, 2, snap.lcdW, snap.lcdH);

    g_timelineSnapshots.push_back(std::move(snap));
    if (!g_timelinePlaying) {
        g_timelineCursor = static_cast<int>(g_timelineSnapshots.size()) - 1;
    }
}

inline bool GetTimelinePlayerActiveKey(int calcIdx, int& outKeyId, int& outStepOneBased)
{
    if ((g_historyViewTab != 2 && !g_timelinePlaying) || g_timelineSnapshots.empty()) return false;
    if (g_timelineCursor < 0 || g_timelineCursor >= static_cast<int>(g_timelineSnapshots.size())) return false;
    const auto& cur = g_timelineSnapshots[g_timelineCursor];
    if (cur.calcIdx != calcIdx) return false;
    outKeyId = cur.keyId;
    outStepOneBased = g_timelineCursor + 1;
    return true;
}

inline void ApplyTimelineStepToCalculator(int stepIdx, bool syncRam)
{
    if (g_timelineSnapshots.empty()) return;
    stepIdx = (std::max)(0, (std::min)(stepIdx, static_cast<int>(g_timelineSnapshots.size()) - 1));
    g_timelineCursor = stepIdx;
    const auto& snap = g_timelineSnapshots[stepIdx];
    g_activeCalcIdx = snap.calcIdx;

    if (syncRam && !snap.emuState.empty() && snap.calcIdx < 2 && g_nativeCalcs[snap.calcIdx].emu) {
        calcemu_load_state(g_nativeCalcs[snap.calcIdx].emu, snap.emuState.data(), snap.emuState.size());
    }
    OpenExplorerForKey(snap.calcIdx, snap.keyId, false, -1);
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd)) InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
}

inline int TimelineSpeedDelayMs()
{
    if (g_timelineSpeedIdx == 0) return 1200;
    if (g_timelineSpeedIdx == 2) return 350;
    return 700;
}

inline bool PromptStepNoteEdit(HWND owner, std::wstring& ioNote)
{
    struct NoteDlgState { std::wstring text; bool ok = false; bool done = false; HWND hEdit = nullptr; };
    NoteDlgState st{ ioNote, false, false, nullptr };

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = [](HWND h, UINT m, WPARAM w, LPARAM l) -> LRESULT {
        auto* p = reinterpret_cast<NoteDlgState*>(GetWindowLongPtrW(h, GWLP_USERDATA));
        switch (m) {
        case WM_CREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
            p = reinterpret_cast<NoteDlgState*>(cs->lpCreateParams);
            SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(p));
            UiGlass::ApplyGoldenGateWindowFrame(h, CasioTheme::GetActiveGlassSettings().IsDark());
            p->hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", p->text.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 14, 34, 412, 26, h, (HMENU)101, g_hInst, nullptr);
            SendMessageW(p->hEdit, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            HWND bSave = CreateWindowExW(0, L"BUTTON", L"Guardar Nota", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                220, 72, 110, 28, h, (HMENU)IDOK, g_hInst, nullptr);
            HWND bCancel = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                338, 72, 88, 28, h, (HMENU)IDCANCEL, g_hInst, nullptr);
            SendMessageW(bSave, WM_SETFONT, (WPARAM)g_fontBold, TRUE);
            SendMessageW(bCancel, WM_SETFONT, (WPARAM)g_fontNormal, TRUE);
            SetFocus(p->hEdit);
            SendMessageW(p->hEdit, EM_SETSEL, 0, -1);
            return 0;
        }
        case WM_CTLCOLOREDIT:
            return CasioTheme::HandleStudioEditCtlColor(w);
        case WM_DPICHANGED: {
            const RECT* suggested = reinterpret_cast<const RECT*>(l);
            if (suggested) {
                SetWindowPos(h, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left, suggested->bottom - suggested->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
            }
            InvalidateRect(h, nullptr, TRUE);
            return 0;
        }
        case WM_COMMAND:
            if (p && LOWORD(w) == IDOK) {
                wchar_t buf[512]{};
                GetWindowTextW(p->hEdit, buf, 511);
                p->text = buf;
                p->ok = true;
                p->done = true;
                DestroyWindow(h);
                return 0;
            }
            if (p && LOWORD(w) == IDCANCEL) {
                p->done = true;
                DestroyWindow(h);
                return 0;
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(h, &ps);
            RECT r{}; GetClientRect(h, &r);
            HBRUSH hb = CreateSolidBrush(CasioTheme::StudioSurface());
            FillRect(hdc, &r, hb);
            DeleteObject(hb);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, CasioTheme::StudioText());
            SelectObject(hdc, g_fontSmall);
            TextOutW(hdc, 14, 12, L"Nota de este paso (se mostrar\u00E1 en Reproductor y Exportador):", 60);
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CLOSE:
            if (p) p->done = true;
            DestroyWindow(h);
            return 0;
        }
        return DefWindowProcW(h, m, w, l);
    };
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"CasioTimelineNotePrompt";
    RegisterClassExW(&wc);

    RECT wr{ 0, 0, 440, 112 };
    AdjustWindowRectEx(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_DLGMODALFRAME);
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CasioTimelineNotePrompt",
        L"Editar Nota de Paso", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
        owner, nullptr, g_hInst, &st);
    if (!hDlg) return false;
    EnableWindow(owner, FALSE);
    MSG m{};
    while (!st.done && GetMessageW(&m, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (st.ok) ioNote = st.text;
    return st.ok;
}

inline int ExportTimelineSequencePngs()
{
    if (g_timelineSnapshots.empty()) {
        g_playerStatusMsg = L"No hay pasos grabados para exportar.";
        return 0;
    }
    std::wstring outDir = ResolveWritableOutputDir(L"guias\\reproductor_pasos");
    int saved = 0;
    HDC hScr = GetDC(nullptr);
    HDC hdc = CreateCompatibleDC(hScr);
    for (size_t i = 0; i < g_timelineSnapshots.size(); ++i) {
        const auto& snap = g_timelineSnapshots[i];
        const int w = 540, h = 210;
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 24;
        bmi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        HBITMAP hFrame = CreateDIBSection(hScr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hFrame) continue;
        HGDIOBJ oB = SelectObject(hdc, hFrame);
        SetBkMode(hdc, TRANSPARENT);

        RECT rcAll{ 0, 0, w, h };
        HBRUSH hBg = CreateSolidBrush(RGB(18, 22, 32));
        FillRect(hdc, &rcAll, hBg);
        DeleteObject(hBg);

        RECT rcHdr{ 0, 0, w, 34 };
        HBRUSH hHd = CreateSolidBrush(RGB(28, 46, 80));
        FillRect(hdc, &rcHdr, hHd);
        DeleteObject(hHd);

        HFONT hf = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hs = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        SelectObject(hdc, hf);
        SetTextColor(hdc, RGB(240, 248, 255));
        std::wstring title = L"Paso #" + std::to_wstring(i + 1) + L" / " + std::to_wstring(g_timelineSnapshots.size())
            + L"  -  Tecla [" + snap.caption + L"]";
        TextOutW(hdc, 12, 8, title.c_str(), static_cast<int>(title.size()));

        const auto& keyCap = KeyCaptureForHistory(hdc, snap.calcIdx, snap.keyId);
        if (keyCap.bitmap && keyCap.width > 0 && keyCap.height > 0) {
            HDC hk = CreateCompatibleDC(hdc);
            HBITMAP ok = (HBITMAP)SelectObject(hk, keyCap.bitmap);
            double sc = (std::min)(82.0 / keyCap.width, 78.0 / keyCap.height);
            int dw = (std::max)(24, static_cast<int>(keyCap.width * sc));
            int dh = (std::max)(20, static_cast<int>(keyCap.height * sc));
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, 18, 46, dw, dh, hk, 0, 0, keyCap.width, keyCap.height, SRCCOPY);
            SelectObject(hk, ok);
            DeleteDC(hk);
        }
        if (snap.hLcdBmp && snap.lcdW > 0 && snap.lcdH > 0) {
            HDC hl = CreateCompatibleDC(hdc);
            HBITMAP ol = (HBITMAP)SelectObject(hl, snap.hLcdBmp);
            double sc = (std::min)(390.0 / snap.lcdW, 118.0 / snap.lcdH);
            int dw = (std::max)(60, static_cast<int>(snap.lcdW * sc));
            int dh = (std::max)(36, static_cast<int>(snap.lcdH * sc));
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, 125, 44, dw, dh, hl, 0, 0, snap.lcdW, snap.lcdH, SRCCOPY);
            SelectObject(hl, ol);
            DeleteDC(hl);
        }
        RECT rn{ 14, 168, w - 14, 204 };
        DrawLatexRichText(hdc, snap.note + L"   \x00B7   LCD: " + snap.indicatorsSummary,
                          rn, RGB(215, 232, 250), RGB(125, 225, 255), 12, true);

        DeleteObject(hf);
        DeleteObject(hs);
        SelectObject(hdc, oB);

        wchar_t fname[64]{};
        swprintf_s(fname, L"\\paso_%03zu.png", i + 1);
        if (SaveBitmapToPngFile((outDir + fname).c_str(), hFrame)) saved++;
        DeleteObject(hFrame);
    }
    DeleteDC(hdc);
    ReleaseDC(nullptr, hScr);

    g_playerStatusMsg = L"Exportados " + std::to_wstring(saved) + L" PNGs en reproductor_pasos.";
    ShowToolbarToast(g_playerStatusMsg);
    return saved;
}

// Measure and paint use the same flow. Offscreen keys do not create bitmaps.
static int HistoryFlow(HDC dc,const RECT& client,int scroll,bool paint)
{
    const int hdrH = GetHistoryHeaderHeight();
    if (paint) g_histKeyHitItems.clear();
    if (g_keyHistory.empty()) {
        const wchar_t* onboardingBody =
            L"Pulsa teclas para registrar la secuencia.";
        const int innerW = max(120, static_cast<int>(client.right - 48));
        RECT rcMeasure{ 0, 0, innerW, 0 };
        if (g_fontSmall) SelectObject(dc, g_fontSmall);
        DrawTextW(dc, onboardingBody, -1, &rcMeasure, DT_LEFT | DT_WORDBREAK | DT_CALCRECT);
        const int blockH = 44 + static_cast<int>(rcMeasure.bottom) + 12;
        if (paint) {
            const int topY = hdrH + 24 - scroll;
            SetBkMode(dc, TRANSPARENT);
            SelectObject(dc, g_fontBold);
            SetTextColor(dc, CasioTheme::StudioTextSec());
            RECT rt{ 20, topY, client.right - 20, topY + 24 };
            DrawTextW(dc, L"Sin teclas", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            SelectObject(dc, g_fontSmall);
            SetTextColor(dc, CasioTheme::StudioTextMuted());
            RECT rb{ 20, topY + 28, client.right - 20, topY + blockH };
            DrawTextW(dc, onboardingBody, -1, &rb, DT_LEFT | DT_WORDBREAK);
        }
        return blockH + 36;
    }
    const int available=max(1,(int)client.right-32);
    const int columns=max(1,available/100);
    const int cellWidth=(available-(columns-1)*8)/columns;
    int y=0,column=0,lastModel=-1;
    HDC imageDC=paint?CreateCompatibleDC(dc):nullptr;
    HGDIOBJ originalBitmap=nullptr;
    for(size_t idx = 0; idx < g_keyHistory.size(); ++idx){
        const auto& item = g_keyHistory[idx];
        if(item.calcIdx!=lastModel){
            if(column){y+=HISTORY_CELL_HEIGHT+8;column=0;}
            if(lastModel!=-1)y+=10;
            lastModel=item.calcIdx;
            const int screenY=hdrH+y-scroll;
            if(paint&&screenY>=hdrH&&screenY+24<=client.bottom){
                SetTextColor(dc,CasioTheme::StudioAccentText());SelectObject(dc,g_fontBold);
                RECT label{16,screenY,client.right-16,screenY+24};
                DrawTextW(dc,g_nativeCalcs[lastModel].displayName.c_str(),-1,&label,
                          DT_LEFT|DT_SINGLELINE|DT_END_ELLIPSIS);
            }
            y+=30;
        }
        const int screenY=hdrH+y-scroll;
        // Keep each complete key and its legends inside the viewport. A partly
        // visible row is skipped instead of displaying a sliced button.
        if(paint&&screenY>=hdrH&&screenY+HISTORY_CELL_HEIGHT<=client.bottom){
            const int x=16+column*(cellWidth+8);
            if (item.isAppCapture) {
                g_histKeyHitItems.push_back({ RECT{ x, screenY, x + cellWidth, screenY + HISTORY_CELL_HEIGHT }, item.calcIdx, -1, static_cast<int>(idx), true, item.captureFilePath });
                RECT cardRc{ x + 1, screenY + 2, x + cellWidth - 1, screenY + 94 };
                CasioTheme::FillRoundRectAA(dc, cardRc, 7.0f, CasioTheme::StudioAccentSoft(), CasioTheme::StudioAccent(), 1.3f);

                RECT badgeRc{ cardRc.left + 4, cardRc.top + 3, cardRc.right - 4, cardRc.top + 17 };
                SelectObject(dc, g_fontSmall);
                SetTextColor(dc, CasioTheme::StudioAccentText());
                DrawTextW(dc, L"App \u00B7 LCD", -1, &badgeRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                const auto* thumb = FindAppMenuThumb(item.captureFilePath);
                if (thumb && thumb->hBmp && thumb->w > 0 && thumb->h > 0) {
                    const int availW = (std::max)(12, static_cast<int>(cardRc.right - cardRc.left) - 10);
                    const int availH = (std::max)(12, static_cast<int>(cardRc.bottom - cardRc.top) - 22);
                    const double scale = (std::min)(static_cast<double>(availW) / thumb->w, static_cast<double>(availH) / thumb->h);
                    const int dw = (std::max)(12, static_cast<int>(thumb->w * scale));
                    const int dh = (std::max)(10, static_cast<int>(thumb->h * scale));
                    const int dx = cardRc.left + (static_cast<int>(cardRc.right - cardRc.left) - dw) / 2;
                    const int dy = cardRc.top + 18 + (availH - dh) / 2;
                    auto prev = SelectObject(imageDC, thumb->hBmp);
                    if (!originalBitmap) originalBitmap = prev;
                    SetStretchBltMode(dc, HALFTONE);
                    StretchBlt(dc, dx, dy, dw, dh, imageDC, 0, 0, thumb->w, thumb->h, SRCCOPY);
                }

                RECT caption{x,screenY+97,x+cellWidth,screenY+HISTORY_CELL_HEIGHT};
                DrawLatexRichTextCentered(dc, KeyHistoryCaption(item), caption, CasioTheme::StudioAccentText(), CasioTheme::StudioAccentText(), 12);
            } else {
                auto& image=KeyCaptureForHistory(dc,item.calcIdx,item.keyId);
                g_histKeyHitItems.push_back({ RECT{ x, screenY, x + cellWidth, screenY + HISTORY_CELL_HEIGHT }, item.calcIdx, item.keyId, static_cast<int>(idx), false, L"" });
                if(image.bitmap&&image.width>0&&image.height>0){
                    const double scale=min((double)cellWidth/image.width,96.0/image.height);
                    const int width=max(1,(int)(image.width*scale));
                    const int height=max(1,(int)(image.height*scale));
                    auto previous=SelectObject(imageDC,image.bitmap);
                    if(!originalBitmap)originalBitmap=previous;
                    StretchBlt(dc,x+(cellWidth-width)/2,screenY,width,height,imageDC,
                               0,0,image.width,image.height,SRCCOPY);
                }
                RECT caption{x,screenY+97,x+cellWidth,screenY+HISTORY_CELL_HEIGHT};
                DrawLatexRichTextCentered(dc, KeyHistoryCaption(item), caption, CasioTheme::StudioTextMuted(), CasioTheme::StudioAccentText(), 12);
            }
        }
        if(++column==columns){column=0;y+=HISTORY_CELL_HEIGHT+8;}
    }
    if(column)y+=HISTORY_CELL_HEIGHT+8;
    if(imageDC){if(originalBitmap)SelectObject(imageDC,originalBitmap);DeleteDC(imageDC);}
    return y+12;
}

// Flow for operation screen captures gallery
static int CapturesFlow(HDC dc, const RECT& client, int scroll, bool paint)
{
    const int hdrH = GetHistoryHeaderHeight();
    const int available = max(1, (int)client.right - 32);
    int y = 0;
    if (g_operationCaptures.empty()) {
        if (paint) {
            SetTextColor(dc, CasioTheme::StudioTextMuted());
            SelectObject(dc, g_fontSmall);
            RECT emptyRc{ 20, hdrH + 28 - scroll, client.right - 20, hdrH + 110 - scroll };
            DrawTextW(dc, L"Sin capturas\n\nPulsa = o EXE para guardar una captura de pantalla.",
                      -1, &emptyRc, DT_CENTER | DT_WORDBREAK);
        }
        return 120;
    }

    HDC imageDC = paint ? CreateCompatibleDC(dc) : nullptr;
    HGDIOBJ originalBitmap = nullptr;

    for (size_t i = 0; i < g_operationCaptures.size(); ++i) {
        auto& item = g_operationCaptures[i];
        const int cardW = available;
        const int cardH = item.thumbH + 56;
        const int screenY = hdrH + y - scroll;

        item.hitRect = { 16, screenY, 16 + cardW, screenY + cardH };

        if (paint && screenY + cardH >= hdrH && screenY <= client.bottom) {
            RECT cardRc{ 16, screenY, 16 + cardW, screenY + cardH };
            CasioTheme::FillRoundRectAA(dc, cardRc, 8.0f, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), 1.0f);

            // Model name & time header
            SetTextColor(dc, CasioTheme::StudioText());
            SelectObject(dc, g_fontBold);
            RECT titleRc{ 24, screenY + 7, 16 + cardW - 80, screenY + 25 };
            DrawTextW(dc, item.calcName.c_str(), -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

            SetTextColor(dc, CasioTheme::StudioTextMuted());
            SelectObject(dc, g_fontSmall);
            RECT timeRc{ 16 + cardW - 76, screenY + 7, 16 + cardW - 10, screenY + 25 };
            DrawTextW(dc, item.timeText.c_str(), -1, &timeRc, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            // Thumbnail
            if (item.hThumb && item.thumbW > 0 && item.thumbH > 0 && imageDC) {
                auto prev = SelectObject(imageDC, item.hThumb);
                if (!originalBitmap) originalBitmap = prev;
                int thumbX = 16 + (cardW - item.thumbW) / 2;
                int thumbY = screenY + 28;
                BitBlt(dc, thumbX, thumbY, item.thumbW, item.thumbH, imageDC, 0, 0, SRCCOPY);
            }

            // Click hint
            SetTextColor(dc, CasioTheme::StudioAccentText());
            SelectObject(dc, g_fontSmall);
            RECT hintRc{ 24, screenY + cardH - 25, 16 + cardW - 10, screenY + cardH - 6 };
            DrawTextW(dc, L"Clic para abrir captura", -1, &hintRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        y += cardH + 12;
    }

    if (imageDC) {
        if (originalBitmap) SelectObject(imageDC, originalBitmap);
        DeleteDC(imageDC);
    }
    return y + 16;
}

static int PlayerFlow(HDC dc, const RECT& client, int scroll, bool paint)
{
    const int hdrH = GetHistoryHeaderHeight();
    const int availW = (std::max)(200, static_cast<int>(client.right) - 32);
    int y = 0;

    if (paint) {
        g_playerBtns = {};
    }

    // Transport Row 1: Vector SkipStart / Prev / Play|Pause / Next / SkipEnd + Speed
    int baseScreenY = hdrH + y - scroll;
    if (paint) {
        int bX = 16;
        int wSmall = 32;
        int wPlay = (std::max)(74, availW - wSmall * 4 - 58 - 20);
        g_playerBtns.firstBtn = { bX, baseScreenY, bX + wSmall, baseScreenY + 28 }; bX += wSmall + 4;
        g_playerBtns.prevBtn  = { bX, baseScreenY, bX + wSmall, baseScreenY + 28 }; bX += wSmall + 4;
        g_playerBtns.playBtn  = { bX, baseScreenY, bX + wPlay,  baseScreenY + 28 }; bX += wPlay + 4;
        g_playerBtns.nextBtn  = { bX, baseScreenY, bX + wSmall, baseScreenY + 28 }; bX += wSmall + 4;
        g_playerBtns.lastBtn  = { bX, baseScreenY, bX + wSmall, baseScreenY + 28 }; bX += wSmall + 4;
        g_playerBtns.speedBtn = { bX, baseScreenY, 16 + availW, baseScreenY + 28 };

        auto drawVecBtn = [&](const RECT& rcBtn, int trKind, COLORREF fill, COLORREF bd, COLORREF fg) {
            CasioTheme::FillRoundRectAA(dc, rcBtn, 6.0f, fill, bd, 1.0f);
            CasioTheme::DrawTransportGlyph(dc, rcBtn, trKind, fg);
        };
        drawVecBtn(g_playerBtns.firstBtn, CasioTheme::TR_SKIP_START, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), CasioTheme::StudioText());
        drawVecBtn(g_playerBtns.prevBtn,  CasioTheme::TR_PREV,       CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), CasioTheme::StudioText());

        CasioTheme::FillRoundRectAA(dc, g_playerBtns.playBtn, 6.0f,
            g_timelinePlaying ? CasioTheme::StudioAmberSoft() : CasioTheme::StudioSuccessSoft(),
            g_timelinePlaying ? CasioTheme::StudioAmber() : CasioTheme::StudioSuccess(), 1.2f);
        RECT rcPlayIcon = { g_playerBtns.playBtn.left + 8, g_playerBtns.playBtn.top + 3,
                            g_playerBtns.playBtn.left + 28, g_playerBtns.playBtn.bottom - 3 };
        CasioTheme::DrawTransportGlyph(dc, rcPlayIcon,
            g_timelinePlaying ? CasioTheme::TR_PAUSE : CasioTheme::TR_PLAY,
            g_timelinePlaying ? CasioTheme::StudioAmberText() : CasioTheme::StudioSuccessText());
        SetTextColor(dc, g_timelinePlaying ? CasioTheme::StudioAmberText() : CasioTheme::StudioSuccessText());
        SelectObject(dc, g_fontBold);
        RECT rcPlayTxt = { g_playerBtns.playBtn.left + 26, g_playerBtns.playBtn.top, g_playerBtns.playBtn.right - 6, g_playerBtns.playBtn.bottom };
        DrawTextW(dc, g_timelinePlaying ? L"Pausa" : L"Reprod.", -1, &rcPlayTxt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        drawVecBtn(g_playerBtns.nextBtn, CasioTheme::TR_NEXT,     CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), CasioTheme::StudioText());
        drawVecBtn(g_playerBtns.lastBtn, CasioTheme::TR_SKIP_END, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), CasioTheme::StudioText());

        const wchar_t* spdTxt = (g_timelineSpeedIdx == 0) ? L"0.5x" : (g_timelineSpeedIdx == 2 ? L"2x" : L"1x");
        CasioTheme::DrawStudioButton(dc, g_playerBtns.speedBtn, spdTxt, g_fontSmall, CasioTheme::BTN_SECONDARY, g_timelineSpeedIdx != 1, false, 6.0f);
    }
    y += 34;

    // Transport Row 2: Sincr RAM | Restaurar | Nota | Export PNGs
    baseScreenY = hdrH + y - scroll;
    if (paint) {
        int bw4 = (availW - 12) / 4;
        int bX = 16;
        g_playerBtns.syncBtn      = { bX, baseScreenY, bX + bw4, baseScreenY + 26 }; bX += bw4 + 4;
        g_playerBtns.restoreBtn   = { bX, baseScreenY, bX + bw4, baseScreenY + 26 }; bX += bw4 + 4;
        g_playerBtns.noteBtn      = { bX, baseScreenY, bX + bw4, baseScreenY + 26 }; bX += bw4 + 4;
        g_playerBtns.exportPngBtn = { bX, baseScreenY, 16 + availW, baseScreenY + 26 };

        CasioTheme::DrawStudioButton(dc, g_playerBtns.syncBtn, g_timelineSyncCalc ? L"Sincr: ON" : L"Sincr: OFF",
            g_fontSmall, CasioTheme::BTN_SECONDARY, g_timelineSyncCalc, false, 6.0f);
        CasioTheme::DrawStudioButton(dc, g_playerBtns.restoreBtn, L"Restaurar",
            g_fontSmall, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
        CasioTheme::DrawStudioButton(dc, g_playerBtns.noteBtn, L"Nota",
            g_fontSmall, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
        CasioTheme::DrawStudioButton(dc, g_playerBtns.exportPngBtn, L"PNGs",
            g_fontSmall, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
    }
    y += 34;

    if (g_timelineSnapshots.empty()) {
        if (paint) {
            SetTextColor(dc, CasioTheme::StudioTextMuted());
            SelectObject(dc, g_fontSmall);
            RECT emptyRc{ 20, hdrH + y + 18 - scroll, client.right - 20, hdrH + y + 100 - scroll };
            DrawTextW(dc, L"Sin pasos\n\nCada tecla pulsada guarda un paso en el reproductor.",
                      -1, &emptyRc, DT_CENTER | DT_WORDBREAK);
        }
        return y + 110;
    }

    int curIdx = (std::max)(0, (std::min)(g_timelineCursor, static_cast<int>(g_timelineSnapshots.size()) - 1));
    const auto& cur = g_timelineSnapshots[curIdx];

    // Inspector de paso activo
    int inspH = 174;
    baseScreenY = hdrH + y - scroll;
    if (paint && baseScreenY + inspH >= hdrH && baseScreenY <= client.bottom) {
        RECT rInsp{ 16, baseScreenY, 16 + availW, baseScreenY + inspH };
        CasioTheme::FillRoundRectAA(dc, rInsp, 8.0f, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioAccent(), 1.2f);

        RECT rPill{ rInsp.left + 10, rInsp.top + 8, rInsp.left + 128, rInsp.top + 28 };
        std::wstring hdr = L"Paso #" + std::to_wstring(curIdx + 1) + L" / " + std::to_wstring(g_timelineSnapshots.size());
        CasioTheme::DrawModernPill(dc, rPill, CasioTheme::StudioAccentSoft(), CasioTheme::StudioAccent(),
            CasioTheme::StudioAccentText(), hdr, g_fontSmall, 5.0f);

        RECT rh{ rPill.right + 8, rInsp.top + 8, rInsp.right - 10, rInsp.top + 28 };
        DrawLatexRichText(dc, cur.caption, rh, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

        const auto& kc = KeyCaptureForHistory(dc, cur.calcIdx, cur.keyId);
        if (kc.bitmap && kc.width > 0 && kc.height > 0) {
            HDC hk = CreateCompatibleDC(dc);
            HBITMAP ok = (HBITMAP)SelectObject(hk, kc.bitmap);
            double sc = (std::min)(54.0 / kc.width, 54.0 / kc.height);
            int dw = (std::max)(20, static_cast<int>(kc.width * sc));
            int dh = (std::max)(18, static_cast<int>(kc.height * sc));
            StretchBlt(dc, rInsp.left + 12, rInsp.top + 36, dw, dh, hk, 0, 0, kc.width, kc.height, SRCCOPY);
            SelectObject(hk, ok);
            DeleteDC(hk);
        }
        RECT rLcdWell{ rInsp.left + 74, rInsp.top + 34, rInsp.right - 10, rInsp.top + 110 };
        CasioTheme::FillRoundRectAA(dc, rLcdWell, 6.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorder(), 1.0f);
        if (cur.hLcdBmp && cur.lcdW > 0 && cur.lcdH > 0) {
            HDC hl = CreateCompatibleDC(dc);
            HBITMAP ol = (HBITMAP)SelectObject(hl, cur.hLcdBmp);
            int maxW = (std::max)(60, (int)(rLcdWell.right - rLcdWell.left - 8));
            double sc = (std::min)(static_cast<double>(maxW) / cur.lcdW, 68.0 / cur.lcdH);
            int dw = (std::max)(40, static_cast<int>(cur.lcdW * sc));
            int dh = (std::max)(24, static_cast<int>(cur.lcdH * sc));
            int lx = rLcdWell.left + ((int)(rLcdWell.right - rLcdWell.left) - dw) / 2;
            int ly = rLcdWell.top + ((int)(rLcdWell.bottom - rLcdWell.top) - dh) / 2;
            StretchBlt(dc, lx, ly, dw, dh, hl, 0, 0, cur.lcdW, cur.lcdH, SRCCOPY);
            SelectObject(hl, ol);
            DeleteDC(hl);
        }

        SelectObject(dc, g_fontSmall);
        SetTextColor(dc, CasioTheme::StudioSuccessText());
        RECT ri{ rInsp.left + 10, rInsp.top + 114, rInsp.right - 10, rInsp.top + 132 };
        DrawTextW(dc, (L"LCD: " + cur.indicatorsSummary).c_str(), -1, &ri, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT rn{ rInsp.left + 10, rInsp.top + 133, rInsp.right - 10, rInsp.bottom - 6 };
        DrawLatexRichText(dc, cur.note, rn, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);
    }
    y += inspH + 10;

    if (!g_playerStatusMsg.empty()) {
        baseScreenY = hdrH + y - scroll;
        if (paint) {
            SelectObject(dc, g_fontSmall);
            SetTextColor(dc, CasioTheme::StudioSuccessText());
            RECT rs{ 16, baseScreenY, 16 + availW, baseScreenY + 20 };
            DrawTextW(dc, g_playerStatusMsg.c_str(), -1, &rs, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        y += 22;
    }

    // Lista de pasos capturados en orden
    for (size_t i = 0; i < g_timelineSnapshots.size(); ++i) {
        auto& snap = g_timelineSnapshots[i];
        int rowH = 54;
        int screenY = hdrH + y - scroll;
        snap.hitRect = { 16, screenY, 16 + availW, screenY + rowH };

        if (paint && screenY + rowH >= hdrH && screenY <= client.bottom) {
            bool active = (static_cast<int>(i) == curIdx);
            CasioTheme::FillRoundRectAA(dc, snap.hitRect, 7.0f,
                active ? CasioTheme::StudioAccentSoft() : CasioTheme::StudioSurfaceAlt(),
                active ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder(),
                active ? 1.3f : 1.0f);
            if (active) {
                RECT rail = { snap.hitRect.left + 3, snap.hitRect.top + 8, snap.hitRect.left + 6, snap.hitRect.bottom - 8 };
                CasioTheme::FillRoundRectAA(dc, rail, 1.5f, CasioTheme::StudioAccent());
            }

            if (snap.hLcdBmp && snap.lcdW > 0 && snap.lcdH > 0) {
                HDC hl = CreateCompatibleDC(dc);
                HBITMAP ol = (HBITMAP)SelectObject(hl, snap.hLcdBmp);
                double sc = (std::min)(80.0 / snap.lcdW, 38.0 / snap.lcdH);
                int dw = (std::max)(24, static_cast<int>(snap.lcdW * sc));
                int dh = (std::max)(14, static_cast<int>(snap.lcdH * sc));
                StretchBlt(dc, snap.hitRect.right - dw - 8, snap.hitRect.top + (rowH - dh) / 2, dw, dh, hl, 0, 0, snap.lcdW, snap.lcdH, SRCCOPY);
                SelectObject(hl, ol);
                DeleteDC(hl);
            }

            SelectObject(dc, active ? g_fontBold : g_fontNormal);
            SetTextColor(dc, CasioTheme::StudioText());
            RECT rt{ snap.hitRect.left + 12, snap.hitRect.top + 6, snap.hitRect.right - 96, snap.hitRect.top + 26 };
            std::wstring lbl = L"#" + std::to_wstring(i + 1) + L"  " + snap.caption;
            DrawTextW(dc, lbl.c_str(), -1, &rt, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

            RECT rn{ snap.hitRect.left + 12, snap.hitRect.top + 27, snap.hitRect.right - 96, snap.hitRect.bottom - 5 };
            DrawLatexRichText(dc, snap.note, rn,
                              active ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextSec(),
                              CasioTheme::StudioAccentText(), 11, true);
        }
        y += rowH + 6;
    }
    return y + 14;
}

LRESULT CALLBACK HistoryWndProc(HWND hWnd,UINT message,WPARAM wParam,LPARAM lParam)
{
    switch(message){
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:InvalidateRect(hWnd,nullptr,FALSE);return 0;
    case WM_TIMER:
        if (wParam == ID_TIMER_TIMELINE_PLAY) {
            if (!g_timelinePlaying || g_timelineSnapshots.empty()) {
                g_timelinePlaying = false;
                KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                return 0;
            }
            if (g_timelineCursor + 1 < static_cast<int>(g_timelineSnapshots.size())) {
                ApplyTimelineStepToCalculator(g_timelineCursor + 1, g_timelineSyncCalc);
            } else {
                g_timelinePlaying = false;
                KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        return 0;
    case WM_RBUTTONUP: {
        int physX = GET_X_LPARAM(lParam);
        int physY = GET_Y_LPARAM(lParam);
        RECT physRc{}; GetClientRect(hWnd, &physRc);
        float scale = GetHistoryUiScale(physRc.right);
        POINT pt{ CasioTheme::DeviceToLogical(physX, scale), CasioTheme::DeviceToLogical(physY, scale) };
        if (g_historyViewTab == 0) {
            for (const auto& h : g_histKeyHitItems) {
                if (PtInRect(&h.rect, pt)) {
                    if (h.isAppCapture) {
                        if (!h.captureFilePath.empty()) {
                            ShellExecuteW(nullptr, L"open", h.captureFilePath.c_str(), nullptr, nullptr, SW_SHOW);
                        }
                        return 0;
                    }
                    ShowCalculatorContextMenu(hWnd, physX, physY, h.calcIdx, h.keyId, -1, false, false, h.logIdx);
                    return 0;
                }
            }
        } else if (g_historyViewTab == 2) {
            for (size_t i = 0; i < g_timelineSnapshots.size(); ++i) {
                if (PtInRect(&g_timelineSnapshots[i].hitRect, pt)) {
                    ApplyTimelineStepToCalculator(static_cast<int>(i), false);
                    ShowCalculatorContextMenu(hWnd, physX, physY, g_timelineSnapshots[i].calcIdx, g_timelineSnapshots[i].keyId, -1, false, false, static_cast<int>(i));
                    return 0;
                }
            }
        }
        return 0;
    }
    case WM_LBUTTONUP:{
        RECT physRc{}; GetClientRect(hWnd, &physRc);
        float scale = GetHistoryUiScale(physRc.right);
        int x = CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), scale);
        int y = CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), scale);
        int logW = CasioTheme::DeviceToLogical(physRc.right, scale);
        RECT segOuter{}, tabKeys{}, tabCaps{}, tabPlay{}, btnCfg{}, btnExp{}, latexBox{};
        ComputeHistoryHeaderLayout(logW, segOuter, tabKeys, tabCaps, tabPlay, btnCfg, btnExp, latexBox);
        const RECT btnClear = ComputeHistoryClearBtnRect(logW);
        if (PtInRect(&btnClear, POINT{ x, y })) {
            SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_CLEAR_HIST, 0);
            return 0;
        }
        if (PtInRect(&btnCfg, POINT{ x, y })) {
            SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_LEARNING_STUDIO, 0);
            return 0;
        }
        if (PtInRect(&btnExp, POINT{ x, y })) {
            SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_CLOSE_APRENDER, 0);
            return 0;
        }
        if (PtInRect(&tabKeys, POINT{ x, y })) {
            if (g_historyViewTab != 0) {
                g_historyViewTab = 0;
                g_historyScrollY = 0;
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            return 0;
        }
        if (PtInRect(&tabCaps, POINT{ x, y })) {
            if (g_historyViewTab != 1) {
                g_historyViewTab = 1;
                g_historyScrollY = 0;
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            return 0;
        }
        if (PtInRect(&tabPlay, POINT{ x, y })) {
            if (g_historyViewTab != 2) {
                g_historyViewTab = 2;
                g_historyScrollY = 0;
                InvalidateRect(hWnd, nullptr, TRUE);
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (g_historyViewTab == 0) {
            if (HandleDockedExplorerClick(x, y)) {
                return 0;
            }
            for (const auto& h : g_histKeyHitItems) {
                if (PtInRect(&h.rect, POINT{ x, y })) {
                    if (h.isAppCapture) {
                        if (!h.captureFilePath.empty()) {
                            ShellExecuteW(nullptr, L"open", h.captureFilePath.c_str(), nullptr, nullptr, SW_SHOW);
                        }
                        return 0;
                    }
                    OpenExplorerForKey(h.calcIdx, h.keyId, true, -1);
                    return 0;
                }
            }
        } else if (g_historyViewTab == 1) {
            for (const auto& cap : g_operationCaptures) {
                if (PtInRect(&cap.hitRect, POINT{ x, y })) {
                    if (!cap.filePath.empty()) {
                        ShellExecuteW(nullptr, L"open", cap.filePath.c_str(), nullptr, nullptr, SW_SHOW);
                    }
                    break;
                }
            }
        } else if (g_historyViewTab == 2) {
            POINT pt{ x, y };
            if (PtInRect(&g_playerBtns.firstBtn, pt)) {
                g_timelinePlaying = false; KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                ApplyTimelineStepToCalculator(0, g_timelineSyncCalc);
                return 0;
            }
            if (PtInRect(&g_playerBtns.prevBtn, pt)) {
                g_timelinePlaying = false; KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                ApplyTimelineStepToCalculator(g_timelineCursor - 1, g_timelineSyncCalc);
                return 0;
            }
            if (PtInRect(&g_playerBtns.nextBtn, pt)) {
                g_timelinePlaying = false; KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                ApplyTimelineStepToCalculator(g_timelineCursor + 1, g_timelineSyncCalc);
                return 0;
            }
            if (PtInRect(&g_playerBtns.lastBtn, pt)) {
                g_timelinePlaying = false; KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                ApplyTimelineStepToCalculator(static_cast<int>(g_timelineSnapshots.size()) - 1, g_timelineSyncCalc);
                return 0;
            }
            if (PtInRect(&g_playerBtns.playBtn, pt)) {
                if (!g_timelineSnapshots.empty()) {
                    g_timelinePlaying = !g_timelinePlaying;
                    if (g_timelinePlaying) {
                        if (g_timelineCursor < 0 || g_timelineCursor >= static_cast<int>(g_timelineSnapshots.size()) - 1) {
                            ApplyTimelineStepToCalculator(0, g_timelineSyncCalc);
                        }
                        SetTimer(hWnd, ID_TIMER_TIMELINE_PLAY, TimelineSpeedDelayMs(), nullptr);
                    } else {
                        KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                    }
                    InvalidateRect(hWnd, nullptr, FALSE);
                }
                return 0;
            }
            if (PtInRect(&g_playerBtns.speedBtn, pt)) {
                g_timelineSpeedIdx = (g_timelineSpeedIdx + 1) % 3;
                if (g_timelinePlaying) SetTimer(hWnd, ID_TIMER_TIMELINE_PLAY, TimelineSpeedDelayMs(), nullptr);
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&g_playerBtns.syncBtn, pt)) {
                g_timelineSyncCalc = !g_timelineSyncCalc;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (PtInRect(&g_playerBtns.restoreBtn, pt)) {
                ApplyTimelineStepToCalculator(g_timelineCursor, true);
                ShowToolbarToast(L"Estado del paso restaurado en calculadora.");
                return 0;
            }
            if (PtInRect(&g_playerBtns.noteBtn, pt)) {
                if (g_timelineCursor >= 0 && g_timelineCursor < static_cast<int>(g_timelineSnapshots.size())) {
                    if (PromptStepNoteEdit(hWnd, g_timelineSnapshots[g_timelineCursor].note)) {
                        InvalidateRect(hWnd, nullptr, FALSE);
                    }
                }
                return 0;
            }
            if (PtInRect(&g_playerBtns.exportPngBtn, pt)) {
                ExportTimelineSequencePngs();
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            for (size_t i = 0; i < g_timelineSnapshots.size(); ++i) {
                if (PtInRect(&g_timelineSnapshots[i].hitRect, pt)) {
                    g_timelinePlaying = false; KillTimer(hWnd, ID_TIMER_TIMELINE_PLAY);
                    ApplyTimelineStepToCalculator(static_cast<int>(i), g_timelineSyncCalc);
                    return 0;
                }
            }
        }
        return 0;
    }
    case WM_PAINT:{
        PAINTSTRUCT ps;HDC windowDC=BeginPaint(hWnd,&ps);
        RECT physClient{};GetClientRect(hWnd,&physClient);
        if(physClient.right<=0||physClient.bottom<=0){EndPaint(hWnd,&ps);return 0;}
        float scale = GetHistoryUiScale(physClient.right);
        RECT client{ 0, 0, CasioTheme::DeviceToLogical(physClient.right, scale), CasioTheme::DeviceToLogical(physClient.bottom, scale) };

        HDC dc=CreateCompatibleDC(windowDC);
        HBITMAP buffer=CreateCompatibleBitmap(windowDC,physClient.right,physClient.bottom);
        HGDIOBJ oldBuffer=SelectObject(dc,buffer);
        FillRect(dc,&physClient,g_brSidebar);

        {
        CasioTheme::UiScaleScope scaleScope(dc, scale);
        SetBkMode(dc,TRANSPARENT);
        SetStretchBltMode(dc,HALFTONE);SetBrushOrgEx(dc,0,0,nullptr);

        const int hdrH = GetHistoryHeaderHeight();
        const int dockH = (g_historyViewTab == 0)
            ? GetDockedExplorerHeight(static_cast<int>(client.bottom) - hdrH)
            : 0;
        const int bodyBottom = (std::max)(hdrH + 40, static_cast<int>(client.bottom) - dockH);
        RECT bodyClient = client;
        bodyClient.bottom = bodyBottom;

        const int content = (g_historyViewTab == 0)
            ? HistoryFlow(dc, bodyClient, 0, false)
            : ((g_historyViewTab == 1) ? CapturesFlow(dc, bodyClient, 0, false) : PlayerFlow(dc, bodyClient, 0, false));

        const int page=max(1, bodyBottom - hdrH);
        const int maximum=max(0,content-page);
        if (g_historyViewTab == 2) g_historyFollowTail = false;
        g_historyScrollY=g_historyFollowTail?maximum:max(0,min(g_historyScrollY,maximum));
        SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
        si.nMin=0;si.nMax=max(0,content-1);si.nPage=page;si.nPos=g_historyScrollY;
        SetScrollInfo(hWnd,SB_VERT,&si,TRUE);

        int saved=SaveDC(dc);
        IntersectClipRect(dc,0,hdrH,client.right,bodyBottom);
        if (g_historyViewTab == 0) {
            HistoryFlow(dc,bodyClient,g_historyScrollY,true);
        } else if (g_historyViewTab == 1) {
            CapturesFlow(dc,bodyClient,g_historyScrollY,true);
        } else {
            PlayerFlow(dc,bodyClient,g_historyScrollY,true);
        }
        RestoreDC(dc,saved);

        if (dockH > 0) {
            RECT dockRc = { 0, bodyBottom, client.right, client.bottom };
            DrawDockedExplorerInspector(dc, dockRc);
        } else {
            ClearDockedExplorerHitRects();
        }

        // Cabecera material translúcido 2.0 del panel contextual Historial (0..hdrH)
        RECT rcHdr = { 0, 0, client.right, hdrH };
        CasioTheme::DrawTranslucentHeaderStrip(dc, rcHdr, true);

        HPEN penL = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
        HGDIOBJ oPen = SelectObject(dc, penL);
        MoveToEx(dc, 0, 0, nullptr);
        LineTo(dc, 0, client.bottom);
        SelectObject(dc, oPen);
        DeleteObject(penL);

        RECT segOuter{}, tabKeys{}, tabCaps{}, tabPlay{}, btnCfg{}, btnExp{}, latexBox{};
        ComputeHistoryHeaderLayout(client.right, segOuter, tabKeys, tabCaps, tabPlay, btnCfg, btnExp, latexBox);
        g_histQuickBtnClear = ComputeHistoryClearBtnRect(client.right);
        g_histQuickBtnCfg = btnCfg;
        g_histQuickBtnClose = btnExp;
        g_histQuickBtnExp = {};

        SetTextColor(dc, CasioTheme::StudioText());
        HFONT hHistTitle = CasioTheme::CreateUiFont(17, true);
        HGDIOBJ oldHistTitle = SelectObject(dc, hHistTitle);
        RECT title{ 16, 8, g_histQuickBtnClear.left - 8, 36 };
        DrawTextW(dc, L"Historial", -1, &title, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_END_ELLIPSIS);
        SelectObject(dc, oldHistTitle);
        DeleteObject(hHistTitle);

        CasioTheme::DrawStudioButton(dc, g_histQuickBtnClear, L"", g_fontSmall,
            CasioTheme::BTN_SECONDARY, false, false, 8.0f);
        CasioTheme::DrawTrashIconGdi(dc, g_histQuickBtnClear, CasioTheme::StudioTextSec());
        CasioTheme::DrawStudioButton(dc, g_histQuickBtnCfg, L"Centro", g_fontSmall,
            CasioTheme::BTN_SECONDARY, false, false, 8.0f);
        CasioTheme::DrawStudioButton(dc, g_histQuickBtnClose, L"\u00D7", g_fontBold,
            CasioTheme::BTN_TOOLBAR_GHOST, false, false, 8.0f);

        // Selector segmentado flotante material translúcido 2.0 de 3 pestanas (Teclas | Capturas | Pasos)
        CasioTheme::DrawFloatingGlassCapsule(dc, segOuter, scale, -1.0f, false);
        const RECT tabRcs[3] = { tabKeys, tabCaps, tabPlay };
        auto fmtTab = [](const wchar_t* base, size_t count) -> std::wstring {
            return count > 0 ? (std::wstring(base) + L" (" + std::to_wstring(count) + L")") : std::wstring(base);
        };
        const std::wstring tabTxt[3] = {
            fmtTab(L"Teclas", g_keyHistory.size()),
            fmtTab(L"Capturas", g_operationCaptures.size()),
            fmtTab(L"Pasos", g_timelineSnapshots.size())
        };
        for (int t = 0; t < 3; ++t) {
            bool act = (g_historyViewTab == t);
            CasioTheme::DrawStudioSegmentItem(dc, tabRcs[t], tabTxt[t], act ? g_fontBold : g_fontSmall, act, false, act);
        }

        // Vista previa de formula en vivo solo cuando hay expresion activa
        std::wstring liveLatex = BuildLiveHistoryLatexPreview(g_activeCalcIdx);
        if (!liveLatex.empty() && hdrH > 90) {
            CasioTheme::FillRoundRectAA(dc, latexBox, 8.0f, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), 1.0f);
            RECT latexInner{ latexBox.left + 10, latexBox.top + 4, latexBox.right - 10, latexBox.bottom - 4 };
            DrawLatexRichText(dc, liveLatex, latexInner, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 14, true);
        }
        }

        BitBlt(windowDC,0,0,physClient.right,physClient.bottom,dc,0,0,SRCCOPY);
        SelectObject(dc,oldBuffer);DeleteObject(buffer);DeleteDC(dc);
        EndPaint(hWnd,&ps);return 0;
    }
    case WM_MOUSEWHEEL:{
        const short delta=GET_WHEEL_DELTA_WPARAM(wParam);
        SendMessageW(hWnd,WM_VSCROLL,delta>0?SB_PAGEUP:SB_PAGEDOWN,0);return 0;
    }
    case WM_VSCROLL:{
        SCROLLINFO si{};si.cbSize=sizeof(si);si.fMask=SIF_ALL;GetScrollInfo(hWnd,SB_VERT,&si);
        int position=si.nPos;
        switch(LOWORD(wParam)){
        case SB_TOP:position=0;break;
        case SB_BOTTOM:position=si.nMax;break;
        case SB_LINEUP:position-=HISTORY_CELL_HEIGHT+8;break;
        case SB_LINEDOWN:position+=HISTORY_CELL_HEIGHT+8;break;
        case SB_PAGEUP:position-=(int)si.nPage;break;
        case SB_PAGEDOWN:position+=(int)si.nPage;break;
        case SB_THUMBTRACK:case SB_THUMBPOSITION:position=si.nTrackPos;break;
        }
        const int maximum=max(0,si.nMax-(int)si.nPage+1);
        g_historyScrollY=max(0,min(position,maximum));
        g_historyFollowTail=g_historyScrollY==maximum;
        SetScrollPos(hWnd,SB_VERT,g_historyScrollY,TRUE);
        InvalidateRect(hWnd,nullptr,FALSE);return 0;
    }
    }
    return DefWindowProcW(hWnd,message,wParam,lParam);
}
