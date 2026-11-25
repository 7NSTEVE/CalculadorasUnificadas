#pragma once
// ============================================================================
// settings_and_exporter.h
// 1) Configuracion de Portapapeles (Solo Imagen, Solo Texto, o Ambas) +
//    Auto-copia al evaluar + Modo Presentacion (Resaltar ultima tecla) +
//    Instantaneas Coherentes de Estado (Undo / Snapshot RAM + Historial).
// 2) Exportador de Guias de Secuencias de Botones:
//    Seleccion manual de rango de pasos (Desde #A hasta #B o individual),
//    notas con renderizado del Subconjunto LaTeX Matematico 2D, botones
//    reales de cada calculadora y captura de pantalla LCD lossless PNG/BMP.
// ============================================================================

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <cstdio>
#include <ctime>
#include <algorithm>
#include "ui_theme.h"
#include "latex_math_renderer.h"

#pragma comment(lib, "comdlg32.lib")

// Config globals are defined in main.cpp right above #include "popup_display.h" so
// both MainWndProc/DrawCalculator and settings_and_exporter.h share them cleanly.

inline std::wstring GetUnifiedBaseDirectory() {
    return GetUnifiedExecutableDirectory();
}

// Instantanea coherente de estado (RAM + Historial + Capturas + Modelo activo)
struct CalculatorSnapshot {
    std::vector<uint8_t> emuState;
    std::vector<KeyLogEntry> keyHistory;
    size_t opCaptureCount = 0;
    int activeCalcIdx = 0;
    bool valid = false;
};
inline CalculatorSnapshot g_savedCalcSnapshots[3];
inline std::vector<uint8_t> g_savedCalcState[3];
inline bool g_hasSavedCalcState[3] = { false, false, false };

// Estado del Exportador de Guias de Secuencias
struct ExporterStepItem {
    int calcIdx = 0;
    int keyId = 0;
    bool selected = true;
    bool isAppCapture = false;
    std::wstring captureFilePath;
    std::wstring captureLabel;
};

inline HWND g_hwndSettings = nullptr;
inline HWND g_hwndExporter = nullptr;
inline HWND g_hwndExpTitleEdit = nullptr;
inline HWND g_hwndExpNotesEdit = nullptr;
inline bool g_expEditingTitle = false;
inline bool g_expEditingNotes = false;
inline HFONT g_hExpEditFont = nullptr;

inline std::vector<ExporterStepItem> g_exporterSteps;
inline int  g_expRangeStart = 1; // 1-based inclusive
inline int  g_expRangeEnd = 1;   // 1-based inclusive
inline int  g_expActiveCalc = 0;
inline bool g_expIncludeStepNumbers = true;
inline bool g_expIncludeLcdResult = true;
inline bool g_expIncludeLatexFormula = true;
inline bool g_expIncludeNotes = true;
inline bool g_expIncludeSummary = true;
inline bool g_expHidePracticeAnswer = false;
inline int  g_expFormatPreset = 0; // 0=Explicada, 1=Rapida, 2=Practica, 3=Presentacion
inline int  g_expAnnotTool = 0;    // 0=Off, 1=Lapiz(mano alzada), 2=Texto/Nota, 3=Flecha, 4=Circulo, 5=Marco, 6=#N

struct GuideLcdAnnotation {
    int kind = 1; // 1=Flecha, 2=Circulo, 3=Marco, 4=Etiqueta #
    float normX = 0.5f;
    float normY = 0.5f;
    int tagNum = 1;
    COLORREF color = RGB(255, 72, 72);
    bool fullSheet = false;
};
inline std::vector<GuideLcdAnnotation> g_expAnnotations;
inline RECT g_expLastLcdDrawRect = { 0, 0, 0, 0 };

struct GuidePointF {
    float x = 0.0f;
    float y = 0.0f;
};

struct GuideFreehandStroke {
    std::vector<GuidePointF> pts; // normalizadas 0..1 sobre la hoja completa
    COLORREF color = RGB(225, 45, 57);
    float strokePx = 3.2f;
};
struct GuideSheetNote {
    float normX = 0.5f; // normalizadas 0..1 sobre la hoja completa
    float normY = 0.5f;
    std::wstring text;
    COLORREF color = RGB(225, 45, 57);
};
enum class ExpUndoKind { Shape, Freehand, Note };
inline std::vector<GuideFreehandStroke> g_expFreehandStrokes;
inline std::vector<GuideSheetNote>      g_expSheetNotes;
inline std::vector<ExpUndoKind>         g_expUndoStack;
inline int  g_expAnnotColorIdx = 0;
inline bool g_expDrawingActive = false;
inline HWND g_hwndExpInlineNoteEdit = nullptr;
inline GuidePointF g_expInlineNoteNorm = { 0.5f, 0.5f };
inline bool g_expInlineNoteActive = false;

inline COLORREF GetExpAnnotInkColor() {
    static constexpr COLORREF kInk[4] = {
        RGB(225, 45, 57),   // Rojo estudio
        RGB(0, 112, 243),   // Azul iPadOS
        RGB(217, 119, 6),   // Ambar
        RGB(22, 163, 74)    // Verde
    };
    return kInk[(g_expAnnotColorIdx >= 0 && g_expAnnotColorIdx < 4) ? g_expAnnotColorIdx : 0];
}

inline std::wstring g_expTitleText = L"Gu\u00EDa Paso a Paso: C\u00E1lculo Integral y Ecuaciones ($\\int_{0}^{3} (x^{2} + 2x)\\,dx = 18$)";
inline std::wstring g_expNotesText =
    L"1) Presione el bot\u00F3n de integral definida $\\int_{a}^{b} f(x)\\,dx$ e ingrese el integrando $f(x) = x^{2} + 2x$.\r\n"
    L"2) Use las flechas del cursor para colocarse en los l\u00EDmites inferior ($a = 0$) y superior ($b = 3$).\r\n"
    L"3) Presione [=] o [EXE] para evaluar el resultado exacto y comparar con $\\frac{b^{3}}{3} + b^{2} = \\frac{27}{3} + 9 = 18$.";
inline std::wstring g_expStatusMsg = L"Listo. Puede ajustar el rango (Desde / Hasta), hacer clic en cualquier bot\u00F3n para activarlo/desactivarlo o exportar la gu\u00EDa.";

inline void ApplyGuideFormatPreset(int preset) {
    g_expFormatPreset = (preset < 0 || preset > 3) ? 0 : preset;
    if (g_expFormatPreset == 0) {
        g_expIncludeNotes = true; g_expIncludeStepNumbers = true; g_expIncludeLcdResult = true;
        g_expIncludeLatexFormula = true; g_expIncludeSummary = true; g_expHidePracticeAnswer = false;
        g_exportSheetLightTheme = true;
        g_expStatusMsg = L"Formato: Gu\u00EDa explicada (notas, f\u00F3rmula, pasos y LCD).";
    } else if (g_expFormatPreset == 1) {
        g_expIncludeNotes = false; g_expIncludeStepNumbers = true; g_expIncludeLcdResult = false;
        g_expIncludeLatexFormula = false; g_expIncludeSummary = true; g_expHidePracticeAnswer = false;
        g_expStatusMsg = L"Formato: Gu\u00EDa r\u00E1pida (secuencia compacta de botones).";
    } else if (g_expFormatPreset == 2) {
        g_expIncludeNotes = true; g_expIncludeStepNumbers = true; g_expIncludeLcdResult = true;
        g_expIncludeLatexFormula = true; g_expIncludeSummary = false; g_expHidePracticeAnswer = true;
        g_exportSheetLightTheme = true;
        g_expStatusMsg = L"Formato: Gu\u00EDa para practicar (oculta resultado LCD y \u00FAltima tecla).";
    } else {
        g_expIncludeNotes = true; g_expIncludeStepNumbers = true; g_expIncludeLcdResult = true;
        g_expIncludeLatexFormula = true; g_expIncludeSummary = true; g_expHidePracticeAnswer = false;
        g_exportSheetLightTheme = false;
        g_expStatusMsg = L"Formato: Material de presentaci\u00F3n (tema oscuro alto contraste).";
    }
}

inline void TrySwitchExporterToEquivalentModel() {
    std::vector<int> curSeq;
    for (const auto& st : g_exporterSteps) if (st.selected) curSeq.push_back(st.keyId);
    const auto& procs = CasioKnowledge::GetCatalog().procedures;
    const CasioKnowledge::ProcedureEntry* matched = nullptr;
    for (const auto& p : procs) {
        if (g_expActiveCalc >= 0 && g_expActiveCalc <= 2 && p.perModel[g_expActiveCalc].supported) {
            if (!curSeq.empty() && p.perModel[g_expActiveCalc].keys == curSeq) {
                matched = &p;
                break;
            }
            if (!g_expTitleText.empty() && p.title.size() > 6 &&
                g_expTitleText.find(p.title.substr(0, (std::min)(size_t{12}, p.title.size()))) != std::wstring::npos) {
                matched = &p;
                break;
            }
        }
    }
    if (!matched) {
        g_expStatusMsg = L"Sin equivalencia directa verificada para esta secuencia manual. Revise Comparador.";
        return;
    }
    for (int step = 1; step <= 2; ++step) {
        int cand = (g_expActiveCalc + step) % 3;
        if (matched->perModel[cand].supported && !matched->perModel[cand].keys.empty()) {
            const wchar_t* mNames[3] = { L"ClassWiz", L"ES PLUS", L"fx-CG50" };
            const auto& vm = matched->perModel[cand];
            g_expActiveCalc = cand;
            g_exporterSteps.clear();
            for (int k : vm.keys) {
                g_exporterSteps.push_back({ cand, k, true });
            }
            g_expRangeStart = 1;
            g_expRangeEnd = (std::max)(1, static_cast<int>(g_exporterSteps.size()));
            g_expNotesText = matched->goalSummary + L"\r\nPasos (" + mNames[cand] + L"): " + vm.stepsSummary + L"\r\nResultado LCD: " + vm.expectedLcd;
            g_expEditingTitle = false;
            g_expEditingNotes = false;
            if (g_hwndExpNotesEdit) {
                SetWindowTextW(g_hwndExpNotesEdit, g_expNotesText.c_str());
                ShowWindow(g_hwndExpNotesEdit, SW_HIDE);
            }
            if (g_hwndExpTitleEdit) ShowWindow(g_hwndExpTitleEdit, SW_HIDE);
            g_expAnnotations.clear();
            g_expFreehandStrokes.clear();
            g_expSheetNotes.clear();
            g_expUndoStack.clear();
            g_expInlineNoteActive = false;
            if (g_hwndExpInlineNoteEdit) ShowWindow(g_hwndExpInlineNoteEdit, SW_HIDE);
            g_expStatusMsg = std::wstring(L"Equivalencia verificada cargada para ") + mNames[cand] + L".";
            return;
        }
    }
    g_expStatusMsg = L"Este procedimiento es exclusivo del modelo actual.";
}

inline void DrawAnnotationVector(HDC hdc, const RECT& targetRect, const GuideLcdAnnotation& a) {
    const int w = targetRect.right - targetRect.left;
    const int h = targetRect.bottom - targetRect.top;
    if (w <= 4 || h <= 4) return;
    const int px = targetRect.left + static_cast<int>(a.normX * w);
    const int py = targetRect.top  + static_cast<int>(a.normY * h);
    if (EnsureGdiplusInitialized()) {
        Gdiplus::Graphics g(hdc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::Color col(255, GetRValue(a.color), GetGValue(a.color), GetBValue(a.color));
        const float lw = a.fullSheet ? 2.6f : 2.2f;
        Gdiplus::Pen pen(col, lw);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        if (a.kind == 1) {
            const float offX = a.fullSheet ? 38.0f : 28.0f;
            const float offY = a.fullSheet ? 30.0f : 22.0f;
            const float sx = (std::min)(static_cast<float>(targetRect.right - 6), px + offX);
            const float sy = (std::min)(static_cast<float>(targetRect.bottom - 6), py + offY);
            g.DrawLine(&pen, sx, sy, static_cast<float>(px), static_cast<float>(py));
            g.DrawLine(&pen, static_cast<float>(px), static_cast<float>(py), px + 10.0f, py + 2.5f);
            g.DrawLine(&pen, static_cast<float>(px), static_cast<float>(py), px + 3.5f, py + 11.0f);
        } else if (a.kind == 2) {
            const int r = a.fullSheet ? 18 : 14;
            g.DrawEllipse(&pen, px - r, py - r, r * 2, r * 2);
        } else if (a.kind == 3) {
            const int rx = a.fullSheet ? 32 : 22;
            const int ry = a.fullSheet ? 16 : 12;
            Gdiplus::GraphicsPath p;
            CasioTheme::AddRoundRectPath(p, static_cast<float>(px - rx), static_cast<float>(py - ry),
                static_cast<float>(rx * 2), static_cast<float>(ry * 2), 6.0f);
            g.DrawPath(&pen, &p);
        } else if (a.kind == 4) {
            const int r = a.fullSheet ? 12 : 10;
            Gdiplus::SolidBrush br(col);
            g.FillEllipse(&br, px - r, py - r, r * 2, r * 2);
            Gdiplus::Pen ringPen(Gdiplus::Color(255, 255, 255, 255), 1.4f);
            g.DrawEllipse(&ringPen, px - r, py - r, r * 2, r * 2);
            HFONT hf = LatexEngine::CreateMathFont(a.fullSheet ? 12 : 11, false, true, false);
            HGDIOBJ of = SelectObject(hdc, hf);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(255, 255, 255));
            wchar_t tbuf[8]{};
            swprintf_s(tbuf, L"%d", a.tagNum);
            RECT rt{ px - r, py - r, px + r, py + r };
            DrawTextW(hdc, tbuf, -1, &rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of);
            DeleteObject(hf);
        }
        return;
    }
    HPEN hp = CreatePen(PS_SOLID, 2, a.color);
    HGDIOBJ op = SelectObject(hdc, hp);
    HGDIOBJ ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    if (a.kind == 1) {
        int sx = (std::min)(static_cast<int>(targetRect.right - 4), px + 28);
        int sy = (std::min)(static_cast<int>(targetRect.bottom - 4), py + 22);
        MoveToEx(hdc, sx, sy, nullptr); LineTo(hdc, px, py);
        MoveToEx(hdc, px, py, nullptr); LineTo(hdc, px + 8, py + 2);
        MoveToEx(hdc, px, py, nullptr); LineTo(hdc, px + 3, py + 9);
    } else if (a.kind == 2) {
        Ellipse(hdc, px - 14, py - 14, px + 14, py + 14);
    } else if (a.kind == 3) {
        RoundRect(hdc, px - 22, py - 12, px + 22, py + 12, 4, 4);
    } else if (a.kind == 4) {
        HBRUSH hb = CreateSolidBrush(a.color);
        SelectObject(hdc, hb);
        Ellipse(hdc, px - 10, py - 10, px + 10, py + 10);
        SelectObject(hdc, GetStockObject(NULL_BRUSH));
        DeleteObject(hb);
        HFONT hf = LatexEngine::CreateMathFont(11, false, true, false);
        HGDIOBJ of = SelectObject(hdc, hf);
        SetTextColor(hdc, RGB(255, 255, 255));
        wchar_t tbuf[8]{};
        swprintf_s(tbuf, L"%d", a.tagNum);
        RECT rt{ px - 10, py - 10, px + 10, py + 10 };
        DrawTextW(hdc, tbuf, -1, &rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, of);
        DeleteObject(hf);
    }
    SelectObject(hdc, op);
    SelectObject(hdc, ob);
    DeleteObject(hp);
}

inline void DrawFreehandStrokesOnSheet(HDC hdc, int w, int h) {
    if (g_expFreehandStrokes.empty() || w <= 4 || h <= 4 || !EnsureGdiplusInitialized()) return;
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    for (const auto& st : g_expFreehandStrokes) {
        if (st.pts.empty()) continue;
        Gdiplus::Color col(240, GetRValue(st.color), GetGValue(st.color), GetBValue(st.color));
        if (st.pts.size() == 1) {
            const float x = st.pts[0].x * w;
            const float y = st.pts[0].y * h;
            const float r = st.strokePx * 0.65f;
            Gdiplus::SolidBrush sb(col);
            g.FillEllipse(&sb, x - r, y - r, r * 2.0f, r * 2.0f);
            continue;
        }
        std::vector<Gdiplus::PointF> gpts;
        gpts.reserve(st.pts.size());
        for (const auto& p : st.pts) {
            gpts.emplace_back(p.x * w, p.y * h);
        }
        Gdiplus::Pen pen(col, st.strokePx);
        pen.SetStartCap(Gdiplus::LineCapRound);
        pen.SetEndCap(Gdiplus::LineCapRound);
        pen.SetLineJoin(Gdiplus::LineJoinRound);
        g.DrawCurve(&pen, gpts.data(), static_cast<INT>(gpts.size()), 0.35f);
    }
}

inline void DrawSheetNotesOverlay(HDC hdc, int w, int h, bool lightTheme) {
    if (g_expSheetNotes.empty() || w <= 4 || h <= 4) return;
    for (const auto& note : g_expSheetNotes) {
        if (note.text.empty()) continue;
        const int maxTxtW = (std::min)(360, (std::max)(180, w - 40));
        const int measuredH = (std::max)(22, MeasureLatexRichText(hdc, note.text, maxTxtW, 13, false));
        const int estW = (std::min)(maxTxtW, (std::max)(88, static_cast<int>(note.text.size()) * 8 + 18));
        const int boxW = estW + 18;
        const int boxH = measuredH + 12;
        int x = static_cast<int>(note.normX * w) - 10;
        int y = static_cast<int>(note.normY * h) - boxH / 2;
        if (x < 8) x = 8;
        if (x + boxW > w - 8) x = (std::max)(8, w - 8 - boxW);
        if (y < 8) y = 8;
        if (y + boxH > h - 8) y = (std::max)(8, h - 8 - boxH);

        RECT rcNote{ x, y, x + boxW, y + boxH };
        const COLORREF fill = lightTheme ? RGB(255, 253, 246) : RGB(30, 36, 48);
        const COLORREF txtCol = lightTheme ? RGB(24, 30, 42) : RGB(236, 240, 248);
        CasioTheme::FillRoundRectAA(hdc, rcNote, 7.0f, fill, note.color, 1.5f);
        CasioTheme::DrawDotIndicator(hdc, rcNote.left + 8, rcNote.top + boxH / 2, 3, note.color);
        RECT rcTxt{ rcNote.left + 16, rcNote.top + 5, rcNote.right - 6, rcNote.bottom - 5 };
        DrawLatexRichText(hdc, note.text, rcTxt, txtCol, note.color, 13, false);
    }
}

inline void UndoLastSheetAnnotation() {
    while (!g_expUndoStack.empty()) {
        ExpUndoKind k = g_expUndoStack.back();
        g_expUndoStack.pop_back();
        if (k == ExpUndoKind::Freehand && !g_expFreehandStrokes.empty()) {
            g_expFreehandStrokes.pop_back();
            return;
        }
        if (k == ExpUndoKind::Note && !g_expSheetNotes.empty()) {
            g_expSheetNotes.pop_back();
            return;
        }
        if (k == ExpUndoKind::Shape && !g_expAnnotations.empty()) {
            g_expAnnotations.pop_back();
            return;
        }
    }
    if (!g_expFreehandStrokes.empty()) { g_expFreehandStrokes.pop_back(); return; }
    if (!g_expSheetNotes.empty()) { g_expSheetNotes.pop_back(); return; }
    if (!g_expAnnotations.empty()) { g_expAnnotations.pop_back(); return; }
}

inline void ClearAllSheetAnnotations() {
    g_expAnnotations.clear();
    g_expFreehandStrokes.clear();
    g_expSheetNotes.clear();
    g_expUndoStack.clear();
    g_expInlineNoteActive = false;
    if (g_hwndExpInlineNoteEdit) ShowWindow(g_hwndExpInlineNoteEdit, SW_HIDE);
}

// ----------------------------------------------------------------------------
// Persistencia en config_unificada.ini (exeDir si escribible o %LOCALAPPDATA%)
// ----------------------------------------------------------------------------
inline std::wstring GetConfigIniPath() {
    return ResolveWritableConfigIniPath();
}

static void LoadUnifiedSettingsFromIni() {
    std::wstring writableIni = GetConfigIniPath();
    std::wstring readOnlyExeIni = GetUnifiedExecutableDirectory() + L"\\config_unificada.ini";
    std::wstring ini = PathFileExistsW(writableIni.c_str())
        ? writableIni
        : (PathFileExistsW(readOnlyExeIni.c_str()) ? readOnlyExeIni : writableIni);

    g_clipCopyMode = (int)GetPrivateProfileIntW(L"Clipboard", L"CopyMode", CLIP_COPY_BOTH_IMAGE_AND_TEXT, ini.c_str());
    if (g_clipCopyMode < 0 || g_clipCopyMode > 2) g_clipCopyMode = CLIP_COPY_BOTH_IMAGE_AND_TEXT;
    g_autoCopyOnEvaluate = GetPrivateProfileIntW(L"Clipboard", L"AutoCopyOnEvaluate", 0, ini.c_str()) != 0;
    g_includeLatexInTextCopy = GetPrivateProfileIntW(L"Clipboard", L"IncludeLatexInText", 1, ini.c_str()) != 0;
    g_copyFullBodyInsteadOfLcd = GetPrivateProfileIntW(L"Clipboard", L"CopyFullBody", 0, ini.c_str()) != 0;

    int legacyHighlight = (int)GetPrivateProfileIntW(L"Pedagogy", L"TeacherHighlightMode", 1, ini.c_str());
    int legacyLight = (int)GetPrivateProfileIntW(L"Pedagogy", L"ExportSheetLightTheme", 1, ini.c_str());
    g_teacherHighlightMode = GetPrivateProfileIntW(L"Preferences", L"HighlightLastKey", legacyHighlight, ini.c_str()) != 0;
    g_exportSheetLightTheme = GetPrivateProfileIntW(L"Preferences", L"ExportSheetLightTheme", legacyLight, ini.c_str()) != 0;
    g_shiftAlphaLayerMode = GetPrivateProfileIntW(L"Preferences", L"ShiftAlphaLayerMode", 0, ini.c_str()) != 0;
    g_keyExplorerEnabled = GetPrivateProfileIntW(L"Preferences", L"KeyExplorerEnabled", 1, ini.c_str()) != 0;
    CasioTheme::g_studioDarkTheme = GetPrivateProfileIntW(L"Preferences", L"DarkTheme", 0, ini.c_str()) != 0;
    int uiScaleMode = (int)GetPrivateProfileIntW(L"Preferences", L"UiScaleMode", 0, ini.c_str());
    if (uiScaleMode == 0 || uiScaleMode == 100 || uiScaleMode == 125 || uiScaleMode == 150 || uiScaleMode == 175 || uiScaleMode == 200) {
        CasioTheme::g_uiScaleMode = uiScaleMode;
    } else {
        CasioTheme::g_uiScaleMode = 0;
    }
}

static void SaveUnifiedSettingsToIni() {
    std::wstring ini = GetConfigIniPath();
    wchar_t buf[32];
    swprintf(buf, 32, L"%d", g_clipCopyMode);
    WritePrivateProfileStringW(L"Clipboard", L"CopyMode", buf, ini.c_str());
    WritePrivateProfileStringW(L"Clipboard", L"AutoCopyOnEvaluate", g_autoCopyOnEvaluate ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Clipboard", L"IncludeLatexInText", g_includeLatexInTextCopy ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Clipboard", L"CopyFullBody", g_copyFullBodyInsteadOfLcd ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Preferences", L"HighlightLastKey", g_teacherHighlightMode ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Preferences", L"ExportSheetLightTheme", g_exportSheetLightTheme ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Preferences", L"ShiftAlphaLayerMode", g_shiftAlphaLayerMode ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Preferences", L"KeyExplorerEnabled", g_keyExplorerEnabled ? L"1" : L"0", ini.c_str());
    WritePrivateProfileStringW(L"Preferences", L"DarkTheme", CasioTheme::g_studioDarkTheme ? L"1" : L"0", ini.c_str());
    swprintf(buf, 32, L"%d", CasioTheme::g_uiScaleMode);
    WritePrivateProfileStringW(L"Preferences", L"UiScaleMode", buf, ini.c_str());
}

// ----------------------------------------------------------------------------
// Traductor de Secuencia de Botones -> Texto Lineal + Formula LaTeX 2D
// ----------------------------------------------------------------------------
inline void ConvertStepsToLinearAndLatex(
    const std::vector<ExporterStepItem>& steps,
    int startIdx0,
    int endIdx0,
    std::wstring& outLinearText,
    std::wstring& outLatexFormula,
    std::wstring& outKeyStepsArrow
) {
    outLinearText.clear();
    outLatexFormula.clear();
    outKeyStepsArrow.clear();

    if (steps.empty()) {
        outLinearText = L"sqrt(25) + 3^2 = 14";
        outLatexFormula = L"$\\sqrt{25} + 3^{2} = 14$";
        outKeyStepsArrow = L"[RAIZ] \x2192 [2] \x2192 [5] \x2192 [DERECHA] \x2192 [+] \x2192 [3] \x2192 [x\u00B2] \x2192 [=]";
        return;
    }

    int n = (int)steps.size();
    startIdx0 = (std::max)(0, (std::min)(startIdx0, n - 1));
    endIdx0 = (std::max)(startIdx0, (std::min)(endIdx0, n - 1));

    std::wstring latexBody;
    bool shiftPending = false;
    bool alphaPending = false;
    bool hasExprSinceEq = false;
    int openBraces = 0;

    for (int i = startIdx0; i <= endIdx0; ++i) {
        if (!steps[i].selected) continue;
        int m = steps[i].calcIdx;
        int k = steps[i].keyId;
        if (steps[i].isAppCapture || k < 0) {
            std::wstring lcdLbl = !steps[i].captureLabel.empty() ? steps[i].captureLabel : L"App LCD";
            if (!outKeyStepsArrow.empty()) outKeyStepsArrow += L" \x2192 ";
            outKeyStepsArrow += L"[" + lcdLbl + L"]";
            continue;
        }
        std::wstring cap = KeyHistoryCaption(m, k);
        if (!outKeyStepsArrow.empty()) outKeyStepsArrow += L" \x2192 ";
        outKeyStepsArrow += cap;

        // Detectar SHIFT / ALPHA
        if ((m == 0 && k == 0) || (m == 1 && k == 0) || (m == 2 && k == 6) || (m == 3 && k == 11)) {
            shiftPending = true;
            continue;
        }
        if ((m == 0 && k == 1) || (m == 1 && k == 3) || (m == 2 && k == 14)) {
            alphaPending = true;
            continue;
        }
        const size_t bodyLenBeforeStep = latexBody.size();

        // fx-991CW (m == 3) tiene su propio mapa de indices (cw2_keys_exact.h)
        if (m == 3) {
            if (k == 43) { outLinearText += shiftPending ? L"x" : L"0"; latexBody += shiftPending ? L"x" : L"0"; }
            else if (k >= 38 && k <= 40) {
                const wchar_t* alphaD = (k == 38) ? L"D" : ((k == 39) ? L"E" : L"F");
                if (shiftPending) { outLinearText += alphaD; latexBody += alphaD; }
                else { wchar_t d = (wchar_t)(L'1' + (k - 38)); outLinearText.push_back(d); latexBody.push_back(d); }
            } else if (k >= 33 && k <= 35) {
                const wchar_t* alphaA = (k == 33) ? L"A" : ((k == 34) ? L"B" : L"C");
                if (shiftPending) { outLinearText += alphaA; latexBody += alphaA; }
                else { wchar_t d = (wchar_t)(L'4' + (k - 33)); outLinearText.push_back(d); latexBody.push_back(d); }
            } else if (k >= 28 && k <= 30) {
                if (shiftPending) {
                    const wchar_t* linC = (k == 28) ? L"\x03C0" : ((k == 29) ? L"e" : L"i");
                    const wchar_t* texC = (k == 28) ? L"\\pi " : ((k == 29) ? L"e" : L"i");
                    outLinearText += linC; latexBody += texC;
                } else {
                    wchar_t d = (wchar_t)(L'7' + (k - 28)); outLinearText.push_back(d); latexBody.push_back(d);
                }
            } else if (k == 41) { outLinearText += L" + "; latexBody += L" + "; }
            else if (k == 42) {
                outLinearText += shiftPending ? L"-" : L" - ";
                latexBody += shiftPending ? L"-" : L" - ";
            } else if (k == 36) { outLinearText += L" \u00D7 "; latexBody += L" \\times "; }
            else if (k == 37) { outLinearText += L" \u00F7 "; latexBody += L" \\div "; }
            else if (k == 44) {
                if (shiftPending) { outLinearText += L"y"; latexBody += L"y"; }
                else { outLinearText += L"."; latexBody += L"."; }
            } else if (k == 45) {
                if (shiftPending) { outLinearText += L"z"; latexBody += L"z"; }
                else { outLinearText += L"\u00D710^"; latexBody += L" \\times 10^{"; openBraces++; }
            } else if (k == 22) {
                outLinearText += shiftPending ? L"PreAns" : L"Ans";
                latexBody += shiftPending ? L"\\mathrm{PreAns}" : L"\\mathrm{Ans}";
            } else if (k == 47) {
                while (openBraces > 0) { latexBody += L"}"; openBraces--; }
                if (hasExprSinceEq) {
                    outLinearText += shiftPending ? L" \u2248" : L" =";
                    latexBody += shiftPending ? L" \\approx " : L" = ";
                    hasExprSinceEq = false;
                }
                shiftPending = false;
                alphaPending = false;
                continue;
            } else if (k == 26) { outLinearText += shiftPending ? L"=" : L"("; latexBody += shiftPending ? L"=" : L"("; }
            else if (k == 27) {
                while (openBraces > 0) { latexBody += L"}"; openBraces--; }
                outLinearText += shiftPending ? L"," : L")"; latexBody += shiftPending ? L"," : L")";
            } else if (k == 16) { outLinearText += L"x"; latexBody += L"x"; }
            else if (k == 17) { outLinearText += L"(a/b)"; latexBody += L"\\frac{a}{b}"; }
            else if (k == 18) { outLinearText += L"\x221A("; latexBody += L"\\sqrt{"; openBraces++; }
            else if (k == 19) {
                if (shiftPending) { outLinearText += L"^-1"; latexBody += L"^{-1}"; }
                else { outLinearText += L"^("; latexBody += L"^{"; openBraces++; }
            } else if (k == 20) {
                if (shiftPending) { outLinearText += L"log("; latexBody += L"\\log("; }
                else { outLinearText += L"^2"; latexBody += L"^{2}"; }
            } else if (k == 21) {
                outLinearText += shiftPending ? L"ln(" : L"log(";
                latexBody += shiftPending ? L"\\ln(" : L"\\log(";
            } else if (k == 23) { outLinearText += shiftPending ? L"arcsin(" : L"sin("; latexBody += shiftPending ? L"\\arcsin(" : L"\\sin("; }
            else if (k == 24) { outLinearText += shiftPending ? L"arccos(" : L"cos("; latexBody += shiftPending ? L"\\arccos(" : L"\\cos("; }
            else if (k == 25) { outLinearText += shiftPending ? L"arctan(" : L"tan("; latexBody += shiftPending ? L"\\arctan(" : L"\\tan("; }
            else if (k == 2 || k == 6 || k == 8 || k == 10) {
                if (openBraces > 0) { latexBody += L"}"; openBraces--; }
            } else {
                outLinearText += cap;
            }
            if (latexBody.size() > bodyLenBeforeStep) hasExprSinceEq = true;
            shiftPending = false;
            alphaPending = false;
            continue;
        }

        // Digitos 0-9
        int digit = -1;
        if (k == 45) digit = 0;
        else if (k >= 40 && k <= 42) digit = (k - 40) + 1;
        else if (k >= 35 && k <= 37) digit = (k - 35) + 4;
        else if (k >= 30 && k <= 32) digit = (k - 30) + 7;

        if (digit >= 0 && !shiftPending && !alphaPending) {
            wchar_t dch = (wchar_t)(L'0' + digit);
            outLinearText.push_back(dch);
            latexBody.push_back(dch);
            hasExprSinceEq = true;
            continue;
        }

        // Operaciones comunes en las 3 calculadoras
        if (k == 43) { outLinearText += L" + "; latexBody += L" + "; hasExprSinceEq = true; shiftPending = alphaPending = false; continue; }
        if (k == 44) { outLinearText += L" - "; latexBody += L" - "; hasExprSinceEq = true; shiftPending = alphaPending = false; continue; }
        if (k == 38) { outLinearText += L" \u00D7 "; latexBody += L" \\times "; hasExprSinceEq = true; shiftPending = alphaPending = false; continue; }
        if (k == 39) { outLinearText += L" \u00F7 "; latexBody += L" \\div "; hasExprSinceEq = true; shiftPending = alphaPending = false; continue; }
        if (k == 46) { outLinearText += L"."; latexBody += L"."; hasExprSinceEq = true; shiftPending = alphaPending = false; continue; }
        if (k == 47) {
            if (shiftPending) {
                outLinearText += L"\x03C0";
                latexBody += L"\\pi ";
            } else {
                outLinearText += L"\u00D710^";
                latexBody += L" \\times 10^{";
                openBraces++;
            }
            hasExprSinceEq = true;
            shiftPending = alphaPending = false;
            continue;
        }
        if (k == 48) { outLinearText += L"Ans"; latexBody += L"\\mathrm{Ans}"; hasExprSinceEq = true; shiftPending = alphaPending = false; continue; }
        if (k == 49) {
            while (openBraces > 0) { latexBody += L"}"; openBraces--; }
            if (hasExprSinceEq) {
                outLinearText += L" =";
                latexBody += L" = ";
                hasExprSinceEq = false;
            }
            shiftPending = alphaPending = false;
            continue;
        }
        if (k == 26) { outLinearText += L"("; latexBody += L"("; shiftPending = alphaPending = false; continue; }
        if (k == 27) {
            while (openBraces > 0) { latexBody += L"}"; openBraces--; }
            outLinearText += L")";
            latexBody += L")";
            shiftPending = alphaPending = false;
            continue;
        }

        // Funciones cientificas ClassWiz (m=0) y ES PLUS (m=1)
        if (m == 0 || m == 1) {
            if (k == 12) {
                outLinearText += L"(a/b)";
                latexBody += L"\\frac{a}{b}";
            } else if (k == 13) {
                if (shiftPending) {
                    outLinearText += L"\x221B(";
                    latexBody += L"\\sqrt[3]{";
                } else {
                    outLinearText += L"\x221A(";
                    latexBody += L"\\sqrt{";
                }
                openBraces++;
            } else if (k == 14) {
                if (shiftPending) { outLinearText += L"^3"; latexBody += L"^{3}"; }
                else { outLinearText += L"^2"; latexBody += L"^{2}"; }
            } else if (k == 15) {
                outLinearText += L"^(";
                latexBody += L"^{";
                openBraces++;
            } else if (k == 16) {
                outLinearText += L"log(";
                latexBody += L"\\log(";
            } else if (k == 17) {
                if (shiftPending) { outLinearText += L"e^("; latexBody += L"e^{"; openBraces++; }
                else { outLinearText += L"ln("; latexBody += L"\\ln("; }
            } else if (k == 18) {
                outLinearText += L"-";
                latexBody += L"-";
            } else if (k == 21) {
                outLinearText += shiftPending ? L"arcsin(" : L"sin(";
                latexBody += shiftPending ? L"\\arcsin(" : L"\\sin(";
            } else if (k == 22) {
                outLinearText += shiftPending ? L"arccos(" : L"cos(";
                latexBody += shiftPending ? L"\\arccos(" : L"\\cos(";
            } else if (k == 23) {
                outLinearText += shiftPending ? L"arctan(" : L"tan(";
                latexBody += shiftPending ? L"\\arctan(" : L"\\tan(";
            } else if ((m == 0 && k == 10) || (m == 1 && k == 9)) {
                if (shiftPending) {
                    outLinearText += L"d/dx(";
                    latexBody += L"\\frac{d}{dx}\\left(";
                } else {
                    outLinearText += L"\x222B(";
                    latexBody += L"\\int_{a}^{b} ";
                }
            } else if (m == 0 && k == 11) {
                if (shiftPending) {
                    outLinearText += L"\x2211(";
                    latexBody += L"\\sum_{x=a}^{b} ";
                } else {
                    outLinearText += L"x";
                    latexBody += L"x";
                }
            } else if ((m == 0 && (k == 3 || k == 4 || k == 2 || k == 5)) ||
                       (m == 1 && (k == 5 || k == 6 || k == 2 || k == 7))) {
                // Cursores cierran agrupaciones abiertas en LaTeX
                if (openBraces > 0) {
                    latexBody += L"}";
                    openBraces--;
                }
            } else {
                outLinearText += cap;
            }
        } else if (m == 2) {
            // Graficadora fx-CG50
            if (k == 15) {
                if (shiftPending) { outLinearText += L"\x221A("; latexBody += L"\\sqrt{"; openBraces++; }
                else { outLinearText += L"^2"; latexBody += L"^{2}"; }
            } else if (k == 16) {
                outLinearText += L"^(";
                latexBody += L"^{";
                openBraces++;
            } else if (k == 18) {
                outLinearText += L"x";
                latexBody += L"x";
            } else if (k == 19) {
                outLinearText += L"log(";
                latexBody += L"\\log(";
            } else if (k == 20) {
                outLinearText += L"ln(";
                latexBody += L"\\ln(";
            } else if (k == 21) {
                outLinearText += shiftPending ? L"arcsin(" : L"sin(";
                latexBody += shiftPending ? L"\\arcsin(" : L"\\sin(";
            } else if (k == 22) {
                outLinearText += shiftPending ? L"arccos(" : L"cos(";
                latexBody += shiftPending ? L"\\arccos(" : L"\\cos(";
            } else if (k == 23) {
                outLinearText += shiftPending ? L"arctan(" : L"tan(";
                latexBody += shiftPending ? L"\\arctan(" : L"\\tan(";
            } else if (k == 24) {
                outLinearText += L"(a/b)";
                latexBody += L"\\frac{a}{b}";
            } else if (k >= 10 && k <= 13) {
                if (openBraces > 0) { latexBody += L"}"; openBraces--; }
            } else {
                outLinearText += cap;
            }
        }

        if (latexBody.size() > bodyLenBeforeStep) hasExprSinceEq = true;
        shiftPending = false;
        alphaPending = false;
    }

    while (openBraces > 0) {
        latexBody += L"}";
        openBraces--;
    }

    if (outLinearText.empty()) outLinearText = outKeyStepsArrow;
    if (latexBody.empty()) latexBody = L"\\mathrm{Secuencia\\ de\\ Botones}";
    outLatexFormula = L"$" + latexBody + L"$";
}

static std::wstring BuildLiveHistoryLatexPreview(int calcIdx) {
    std::vector<ExporterStepItem> items;
    for (const auto& e : g_keyHistory) {
        if (e.calcIdx == calcIdx && !e.isAppCapture && e.keyId >= 0) {
            ExporterStepItem it;
            it.calcIdx = e.calcIdx;
            it.keyId = e.keyId;
            it.selected = true;
            items.push_back(it);
        }
    }
    if (items.empty()) {
        return L"";
    }
    int start = (int)items.size() > 14 ? (int)items.size() - 14 : 0;
    std::wstring lin, lat, arr;
    ConvertStepsToLinearAndLatex(items, start, (int)items.size() - 1, lin, lat, arr);
    if (lat == L"$\\mathrm{Secuencia\\ de\\ Botones}$") return L"";
    return lat;
}

// ----------------------------------------------------------------------------
// Copia Inteligente al Portapapeles (Solo Imagen / Solo Texto / Ambas)
// ----------------------------------------------------------------------------
inline HBITMAP CreateScaledLcdOrBodyBitmap(int calcIdx, bool fullBody, int& outW, int& outH) {
    outW = 0;
    outH = 0;
    if (calcIdx < 0 || calcIdx > 3) calcIdx = 0;

    if (calcIdx == 3) {
        int rawW = 0, rawH = 0;
        HBITMAP hRaw = Fx991CwHost::CaptureBitmap(!fullBody, rawW, rawH);
        if (!hRaw || rawW <= 0 || rawH <= 0) {
            if (hRaw) DeleteObject(hRaw);
            return nullptr;
        }
        if (!fullBody && rawW < 360) {
            outW = rawW * 2;
            outH = rawH * 2;
            HBITMAP hScaled = ScaleBitmapHalftone(hRaw, rawW, rawH, outW, outH, false);
            DeleteObject(hRaw);
            return hScaled;
        }
        outW = rawW;
        outH = rawH;
        return hRaw;
    }

    HDC hScreen = GetDC(nullptr);
    if (!fullBody) {
        int scale = (calcIdx == 2) ? 2 : 3;
        HBITMAP hBmp = CaptureLcdSnapshot(calcIdx, scale, outW, outH);
        ReleaseDC(nullptr, hScreen);
        return hBmp;
    }

    // Cuerpo completo + LCD renderizado
    auto& c = g_nativeCalcs[calcIdx];
    outW = c.skinW;
    outH = c.skinH;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = outW;
    bmi.bmiHeader.biHeight = -outH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hOut = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC hdcMem = CreateCompatibleDC(hScreen);
    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hOut);

    RECT rcFill = { 0, 0, outW, outH };
    HBRUSH hBr = CreateSolidBrush(RGB(25, 29, 38));
    FillRect(hdcMem, &rcFill, hBr);
    DeleteObject(hBr);

    if (c.hSkinBmp) {
        HDC hdcSkin = CreateCompatibleDC(hScreen);
        HBITMAP hOldSkin = (HBITMAP)SelectObject(hdcSkin, c.hSkinBmp);
        BitBlt(hdcMem, 0, 0, outW, outH, hdcSkin, 0, 0, SRCCOPY);
        SelectObject(hdcSkin, hOldSkin);
        DeleteDC(hdcSkin);
    }

    int lcdW = 0, lcdH = 0;
    HBITMAP hLcd = CaptureLcdSnapshot(calcIdx, 1, lcdW, lcdH);
    if (hLcd) {
        HDC hdcLcd = CreateCompatibleDC(hScreen);
        HBITMAP hOldLcd = (HBITMAP)SelectObject(hdcLcd, hLcd);
        int dstW = c.lcdSkinRect.right - c.lcdSkinRect.left;
        int dstH = c.lcdSkinRect.bottom - c.lcdSkinRect.top;
        SetStretchBltMode(hdcMem, HALFTONE);
        StretchBlt(hdcMem, c.lcdSkinRect.left, c.lcdSkinRect.top, dstW, dstH, hdcLcd, 0, 0, lcdW, lcdH, SRCCOPY);
        SelectObject(hdcLcd, hOldLcd);
        DeleteDC(hdcLcd);
        DeleteObject(hLcd);
    }

    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hScreen);
    return hOut;
}

inline bool PutBitmapAndOrTextToClipboard(HWND hwndOwner, HBITMAP hBmpOwnedByCaller, const std::wstring& textPayload, int copyMode) {
    if (!OpenClipboard(hwndOwner)) return false;
    EmptyClipboard();

    bool ok = false;
    if ((copyMode == CLIP_COPY_IMAGE_ONLY || copyMode == CLIP_COPY_BOTH_IMAGE_AND_TEXT) && hBmpOwnedByCaller) {
        BITMAP bm = {};
        if (GetObject(hBmpOwnedByCaller, sizeof(bm), &bm) > 0) {
            HDC hScreen = GetDC(nullptr);
            HDC hdcSrc = CreateCompatibleDC(hScreen);
            HDC hdcDst = CreateCompatibleDC(hScreen);
            int bw = bm.bmWidth;
            int bh = bm.bmHeight < 0 ? -bm.bmHeight : bm.bmHeight;
            HBITMAP hClipBmp = CreateCompatibleBitmap(hScreen, bw, bh);
            if (hClipBmp) {
                HBITMAP hOldS = (HBITMAP)SelectObject(hdcSrc, hBmpOwnedByCaller);
                HBITMAP hOldD = (HBITMAP)SelectObject(hdcDst, hClipBmp);
                BitBlt(hdcDst, 0, 0, bw, bh, hdcSrc, 0, 0, SRCCOPY);
                SelectObject(hdcSrc, hOldS);
                SelectObject(hdcDst, hOldD);
                if (SetClipboardData(CF_BITMAP, hClipBmp) != nullptr) {
                    ok = true;
                } else {
                    DeleteObject(hClipBmp);
                }
            }
            DeleteDC(hdcSrc);
            DeleteDC(hdcDst);
            ReleaseDC(nullptr, hScreen);
        }
    }

    if ((copyMode == CLIP_COPY_TEXT_ONLY || copyMode == CLIP_COPY_BOTH_IMAGE_AND_TEXT) && !textPayload.empty()) {
        size_t bytes = (textPayload.size() + 1) * sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (hMem) {
            void* ptr = GlobalLock(hMem);
            if (ptr) {
                memcpy(ptr, textPayload.c_str(), bytes);
                GlobalUnlock(hMem);
                if (SetClipboardData(CF_UNICODETEXT, hMem) != nullptr) {
                    ok = true;
                } else {
                    GlobalFree(hMem);
                }
            } else {
                GlobalFree(hMem);
            }
        }
    }

    CloseClipboard();
    return ok;
}

inline std::wstring BuildCalculatorClipboardText(int calcIdx) {
    if (calcIdx < 0 || calcIdx > 3) calcIdx = (g_activeCalcIdx >= 0 && g_activeCalcIdx <= 3) ? g_activeCalcIdx : 0;
    std::vector<ExporterStepItem> recentSteps;
    for (const auto& e : g_keyHistory) {
        if (e.calcIdx == calcIdx) {
            ExporterStepItem it;
            it.calcIdx = e.calcIdx;
            it.keyId = e.keyId;
            it.selected = true;
            recentSteps.push_back(it);
        }
    }
    if (calcIdx == 3 && recentSteps.empty()) {
        return L"=== CASIO ClassWiz II fx-991CW (CasioEmuX) ===\r\n"
               L"Firmware: EY-031A (Emulador integrado)\r\n";
    }

    std::wstring linearExpr, latexExpr, arrowSteps;
    int tailStart = (int)recentSteps.size() > 16 ? (int)recentSteps.size() - 16 : 0;
    ConvertStepsToLinearAndLatex(
        recentSteps, tailStart, (std::max)(0, (int)recentSteps.size() - 1),
        linearExpr, latexExpr, arrowSteps
    );

    const wchar_t* modelTitles[4] = {
        L"CASIO ClassWiz fx-570/991LA X",
        L"CASIO fx-570ES PLUS",
        L"CASIO fx-CG50 PRIZM",
        L"CASIO ClassWiz II fx-991CW"
    };

    std::wstring textPayload = std::wstring(L"=== ") + modelTitles[calcIdx] + L" ===\r\n";
    textPayload += L"Expresi\u00F3n: " + linearExpr + L"\r\n";
    if (g_includeLatexInTextCopy) {
        textPayload += L"LaTeX: " + latexExpr + L"\r\n";
    }
    textPayload += L"Secuencia de Botones: " + arrowSteps + L"\r\n";
    return textPayload;
}

static bool CopyBitmapAndTextByCurrentMode(HWND hwndOwner, int calcIdx, HBITMAP hBmp, bool updateStatus) {
    if (calcIdx < 0 || calcIdx > 3) calcIdx = (g_activeCalcIdx >= 0 && g_activeCalcIdx <= 3) ? g_activeCalcIdx : 0;
    int mode = g_clipCopyMode;
    std::wstring textPayload = BuildCalculatorClipboardText(calcIdx);
    bool ok = PutBitmapAndOrTextToClipboard(hwndOwner ? hwndOwner : g_hMainWnd, hBmp, textPayload, mode);
    if (updateStatus) {
        const wchar_t* modelTitles[4] = {
            L"CASIO ClassWiz fx-570/991LA X",
            L"CASIO fx-570ES PLUS",
            L"CASIO fx-CG50 PRIZM",
            L"CASIO ClassWiz II fx-991CW"
        };
        const wchar_t* modeNames[3] = {
            L"SOLO IMAGEN (Captura LCD)",
            L"SOLO TEXTO (Expresi\u00F3n + LaTeX)",
            L"AMBAS (Imagen LCD + Texto LaTeX)"
        };
        g_clipStatusText = ok
            ? (std::wstring(L"Copiado [") + modeNames[mode] + L"] - " + modelTitles[calcIdx])
            : L"No se pudo abrir el Portapapeles de Windows.";
        ShowToolbarToast(g_clipStatusText);
        if (g_hMainWnd) InvalidateRect(g_hMainWnd, nullptr, FALSE);
    }
    return ok;
}

static void CopyCalculatorResultSmart(int calcIdx, int modeOverride, bool updateStatus) {
    if (calcIdx < 0 || calcIdx > 3) calcIdx = (g_activeCalcIdx >= 0 && g_activeCalcIdx <= 3) ? g_activeCalcIdx : 0;
    int mode = (modeOverride >= 0 && modeOverride <= 2) ? modeOverride : g_clipCopyMode;
    int savedMode = g_clipCopyMode;
    g_clipCopyMode = mode;

    int bmpW = 0, bmpH = 0;
    HBITMAP hBmp = nullptr;
    if (mode == CLIP_COPY_IMAGE_ONLY || mode == CLIP_COPY_BOTH_IMAGE_AND_TEXT) {
        hBmp = CreateScaledLcdOrBodyBitmap(calcIdx, g_copyFullBodyInsteadOfLcd, bmpW, bmpH);
    }

    CopyBitmapAndTextByCurrentMode(g_hMainWnd, calcIdx, hBmp, updateStatus);
    if (hBmp) DeleteObject(hBmp);
    g_clipCopyMode = savedMode;
}

// ----------------------------------------------------------------------------
// Instantanea Coherente de Estado (Save / Restore State: RAM + Historial + Vista)
// ----------------------------------------------------------------------------
static bool SaveCalculatorSnapshotState(int calcIdx) {
    if (calcIdx == 3) {
        g_clipStatusText = L"La fx-991CW guarda su memoria automaticamente en NVRAM (fx991cw.ram).";
        ShowToolbarToast(g_clipStatusText);
        return false;
    }
    if (calcIdx < 0 || calcIdx > 2) return false;
    auto& c = g_nativeCalcs[calcIdx];
    if (!c.emu) {
        g_clipStatusText = L"Instant\u00E1nea de RAM no disponible para " + c.displayName;
        ShowToolbarToast(g_clipStatusText);
        return false;
    }
    size_t sz = calcemu_save_state(c.emu, nullptr, 0);
    if (sz == 0) return false;

    CalculatorSnapshot snap;
    snap.emuState.resize(sz);
    if (calcemu_save_state(c.emu, snap.emuState.data(), sz) != sz) {
        return false;
    }
    snap.keyHistory = g_keyHistory;
    snap.opCaptureCount = g_operationCaptures.size();
    snap.activeCalcIdx = g_activeCalcIdx;
    snap.valid = true;

    g_savedCalcSnapshots[calcIdx] = snap;
    g_savedCalcState[calcIdx] = snap.emuState;
    g_hasSavedCalcState[calcIdx] = true;

    g_clipStatusText = L"Estado RAM guardado (" + c.displayName + L") - Restaura con Ctrl+Z";
    ShowToolbarToast(g_clipStatusText);
    if (g_hMainWnd) InvalidateRect(g_hMainWnd, nullptr, FALSE);
    return true;
}

static bool RestoreCalculatorSnapshotState(int calcIdx) {
    if (calcIdx == 3) {
        ShowToolbarToast(L"Usa Reiniciar NVRAM de fx-991CW en el menu Ver o Estudio para restablecerla.");
        return false;
    }
    if (calcIdx < 0 || calcIdx > 2 || !g_savedCalcSnapshots[calcIdx].valid) {
        ShowToolbarToast(L"Sin snapshot RAM guardado para restaurar (usa Ctrl+S primero)");
        return false;
    }
    auto& c = g_nativeCalcs[calcIdx];
    const auto& snap = g_savedCalcSnapshots[calcIdx];
    if (!c.emu || snap.emuState.empty()) return false;
    if (calcemu_load_state(c.emu, snap.emuState.data(), snap.emuState.size()) != 1) {
        return false;
    }

    g_keyHistory = snap.keyHistory;
    if (g_operationCaptures.size() > snap.opCaptureCount) {
        for (size_t i = snap.opCaptureCount; i < g_operationCaptures.size(); ++i) {
            if (g_operationCaptures[i].hThumb) {
                DeleteObject(g_operationCaptures[i].hThumb);
                g_operationCaptures[i].hThumb = nullptr;
            }
        }
        g_operationCaptures.resize(snap.opCaptureCount);
    }
    g_activeCalcIdx = snap.activeCalcIdx;

    g_clipStatusText = L"Estado coherente restaurado (" + c.displayName + L")";
    ShowToolbarToast(g_clipStatusText);
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    if (g_hMainWnd) InvalidateRect(g_hMainWnd, nullptr, FALSE);
    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
    if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
        InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
    if (g_hwndExporter && IsWindowVisible(g_hwndExporter))
        InvalidateRect(g_hwndExporter, nullptr, FALSE);
    return true;
}

// ----------------------------------------------------------------------------
// Sincronizacion y Renderizado del Exportador de Guias de Secuencias
// ----------------------------------------------------------------------------
inline void SyncExporterStepsFromHistory(int filterCalcIdx) {
    g_exporterSteps.clear();
    for (const auto& e : g_keyHistory) {
        if (filterCalcIdx >= 0 && e.calcIdx != filterCalcIdx) continue;
        ExporterStepItem it;
        it.calcIdx = e.calcIdx;
        it.keyId = e.keyId;
        it.selected = true;
        it.isAppCapture = e.isAppCapture;
        it.captureFilePath = e.captureFilePath;
        it.captureLabel = e.captureLabel;
        g_exporterSteps.push_back(it);
    }

    if (g_exporterSteps.empty()) {
        int m = (filterCalcIdx >= 0 && filterCalcIdx <= 3) ? filterCalcIdx : g_activeCalcIdx;
        g_expActiveCalc = m;
        std::vector<int> demoKeys;
        if (m == 0) {
            demoKeys = { 10, 11, 14, 43, 41, 11, 4, 45, 4, 42, 49 };
        } else if (m == 1) {
            demoKeys = { 13, 41, 36, 6, 43, 42, 14, 49 };
        } else if (m == 3) {
            demoKeys = { 18, 39, 34, 8, 41, 40, 20, 47 };
        } else {
            demoKeys = { 9, 36, 49, 21, 18, 27, 49, 5 };
        }
        for (int k : demoKeys) {
            ExporterStepItem it;
            it.calcIdx = m;
            it.keyId = k;
            it.selected = true;
            g_exporterSteps.push_back(it);
        }
    } else {
        g_expActiveCalc = g_exporterSteps.back().calcIdx;
    }

    g_expRangeStart = 1;
    g_expRangeEnd = (std::max)(1, (int)g_exporterSteps.size());
}

inline void ApplyExporterRangeToSelection() {
    int n = (int)g_exporterSteps.size();
    if (n == 0) return;
    g_expRangeStart = (std::max)(1, (std::min)(g_expRangeStart, n));
    g_expRangeEnd = (std::max)(g_expRangeStart, (std::min)(g_expRangeEnd, n));
    for (int i = 0; i < n; ++i) {
        int step1 = i + 1;
        g_exporterSteps[i].selected = (step1 >= g_expRangeStart && step1 <= g_expRangeEnd);
    }
}

static void LoadCustomSequenceIntoExporter(int calcIdx, const std::vector<int>& keys, const std::wstring& title, const std::wstring& notes) {
    if (calcIdx < 0 || calcIdx > 3) calcIdx = 0;
    g_expActiveCalc = calcIdx;
    g_exporterSteps.clear();
    for (int k : keys) {
        ExporterStepItem it;
        it.calcIdx = calcIdx;
        it.keyId = k;
        it.selected = true;
        g_exporterSteps.push_back(it);
    }
    g_expRangeStart = 1;
    g_expRangeEnd = (std::max)(1, (int)g_exporterSteps.size());
    if (!title.empty()) g_expTitleText = title;
    if (!notes.empty()) g_expNotesText = notes;

    g_expEditingTitle = false;
    g_expEditingNotes = false;
    if (g_hwndExpTitleEdit) {
        SetWindowTextW(g_hwndExpTitleEdit, g_expTitleText.c_str());
        ShowWindow(g_hwndExpTitleEdit, SW_HIDE);
    }
    if (g_hwndExpNotesEdit) {
        SetWindowTextW(g_hwndExpNotesEdit, g_expNotesText.c_str());
        ShowWindow(g_hwndExpNotesEdit, SW_HIDE);
    }
    if (g_hwndExporter) InvalidateRect(g_hwndExporter, nullptr, FALSE);
}

inline HBITMAP RenderPedagogicalSheetBitmap(
    int& outW,
    int& outH,
    std::vector<RECT>* outStepCardRects = nullptr,
    std::vector<int>* outStepIndices = nullptr
) {
    if (g_expEditingTitle && g_hwndExpTitleEdit) {
        wchar_t tbuf[512] = {};
        GetWindowTextW(g_hwndExpTitleEdit, tbuf, 511);
        g_expTitleText = tbuf;
    }
    if (g_expEditingNotes && g_hwndExpNotesEdit) {
        wchar_t nbuf[2048] = {};
        GetWindowTextW(g_hwndExpNotesEdit, nbuf, 2047);
        g_expNotesText = nbuf;
    }

    HDC hScreen = GetDC(nullptr);
    HDC hdcMeasure = CreateCompatibleDC(hScreen);

    int sheetW = 920;
    bool light = g_exportSheetLightTheme;

    COLORREF bgSheet   = light ? RGB(250, 252, 255) : RGB(18, 22, 31);
    COLORREF bgHeader  = light ? RGB(22, 46, 88)    : RGB(26, 36, 58);
    COLORREF bgNotes   = light ? RGB(238, 244, 252) : RGB(25, 32, 46);
    COLORREF bdNotes   = light ? RGB(180, 202, 232) : RGB(52, 70, 102);
    COLORREF bgFormula = light ? RGB(255, 250, 235) : RGB(34, 32, 24);
    COLORREF bdFormula = light ? RGB(228, 196, 110) : RGB(110, 90, 40);
    COLORREF colText   = light ? RGB(25, 32, 45)    : RGB(230, 236, 245);
    COLORREF colMuted  = light ? RGB(90, 104, 126)  : RGB(148, 163, 184);
    COLORREF colMath   = light ? RGB(15, 82, 165)   : RGB(115, 215, 255);
    COLORREF bgCard    = light ? RGB(255, 255, 255) : RGB(28, 35, 50);
    COLORREF bdCardSel = light ? RGB(38, 115, 225)  : RGB(68, 155, 255);
    COLORREF bdCardOff = light ? RGB(210, 216, 226) : RGB(50, 58, 74);

    // Recopilar pasos dentro del rango [g_expRangeStart .. g_expRangeEnd]
    int nTotal = (int)g_exporterSteps.size();
    int rStart0 = (std::max)(0, (std::min)(g_expRangeStart - 1, (std::max)(0, nTotal - 1)));
    int rEnd0   = (std::max)(rStart0, (std::min)(g_expRangeEnd - 1, (std::max)(0, nTotal - 1)));

    std::wstring linearExpr, latexFormula, arrowSteps;
    ConvertStepsToLinearAndLatex(g_exporterSteps, rStart0, rEnd0, linearExpr, latexFormula, arrowSteps);

    g_expLastLcdDrawRect = { 0, 0, 0, 0 };

    int notesH = g_expIncludeNotes
        ? (std::max)(44, MeasureLatexRichText(hdcMeasure, g_expNotesText, sheetW - 56, 15, false) + 20)
        : 0;

    int formulaH = g_expIncludeLatexFormula ? (MeasureLatexRichText(hdcMeasure, L"F\u00F3rmula:  " + latexFormula, sheetW - 56, 17, false) + 22) : 0;

    // Calcular filas de tarjetas de botones
    int cardW = 86;
    int cardH = 112;
    int arrowW = 24;
    int stepStrideX = cardW + arrowW;
    int availGridW = sheetW - 40 - (g_expIncludeLcdResult ? 290 : 0);
    int colsPerRow = (std::max)(3, availGridW / stepStrideX);

    int visibleCount = (nTotal > 0) ? (rEnd0 - rStart0 + 1) : 0;
    int numRows = (std::max)(1, (visibleCount + colsPerRow - 1) / colsPerRow);
    int gridH = numRows * (cardH + 14) + 24;
    if (g_expIncludeLcdResult) {
        gridH = (std::max)(gridH, 210);
    }

    int footerReserve = g_expIncludeSummary ? 46 : 16;
    int sheetH = 92 + (notesH > 0 ? 12 + notesH : 6) + (formulaH > 0 ? formulaH + 10 : 0) + gridH + footerReserve;
    outW = sheetW;
    outH = sheetH;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = outW;
    bmi.bmiHeader.biHeight = -outH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP hSheetBmp = CreateDIBSection(hScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMeasure, hSheetBmp);
    SetBkMode(hdcMeasure, TRANSPARENT);

    // 1. Fondo general y borde exterior
    RECT rcAll = { 0, 0, outW, outH };
    HBRUSH hBrBg = CreateSolidBrush(bgSheet);
    FillRect(hdcMeasure, &rcAll, hBrBg);
    DeleteObject(hBrBg);

    // 2. Cabecera del Documento
    RECT rcHdr = { 0, 0, outW, 90 };
    HBRUSH hBrHdr = CreateSolidBrush(bgHeader);
    FillRect(hdcMeasure, &rcHdr, hBrHdr);
    DeleteObject(hBrHdr);

    const wchar_t* calcNames[4] = {
        L"CASIO ClassWiz fx-570/991LA X",
        L"CASIO fx-570ES PLUS (Natural-V.P.A.M.)",
        L"CASIO fx-CG50 PRIZM (Graficadora)",
        L"CASIO ClassWiz II fx-991CW"
    };
    int activeM = (g_expActiveCalc >= 0 && g_expActiveCalc <= 3) ? g_expActiveCalc : 0;

    RECT rcTitle = { 20, 8, outW - 240, 62 };
    DrawLatexRichText(hdcMeasure, g_expTitleText, rcTitle, RGB(255, 255, 255), RGB(145, 225, 255), 16, true);

    HFONT hSubFont = LatexEngine::CreateMathFont(13, false, true, false);
    HFONT hOldFont = (HFONT)SelectObject(hdcMeasure, hSubFont);
    SetTextColor(hdcMeasure, RGB(185, 212, 248));
    std::wstring subLine = std::wstring(L"Modelo: ") + calcNames[activeM] +
                           L"   |   Pasos seleccionados: #" + std::to_wstring(rStart0 + 1) +
                           L" a #" + std::to_wstring(rEnd0 + 1);
    TextOutW(hdcMeasure, 20, 65, subLine.c_str(), (int)subLine.size());
    SelectObject(hdcMeasure, hOldFont);
    DeleteObject(hSubFont);

    // Badge derecho en cabecera
    RECT rcBadge = { outW - 225, 22, outW - 18, 62 };
    HFONT hBFont = CasioTheme::CreateUiFont(12, true);
    CasioTheme::DrawModernPill(hdcMeasure, rcBadge, CasioTheme::AccentBlue, CasioTheme::AccentHover,
                               RGB(255, 255, 255), L"GU\u00CDA DE SECUENCIA CASIO", hBFont, 8.0f);
    DeleteObject(hBFont);

    int curY = 102;

    // 3. Caja de Notas / Explicacion (con Subconjunto LaTeX Matematico 2D)
    if (g_expIncludeNotes && notesH > 0) {
        RECT rcNotesBox = { 18, curY, outW - 18, curY + notesH };
        CasioTheme::FillRoundRectAA(hdcMeasure, rcNotesBox, 10.0f, bgNotes, bdNotes, 1.1f);

        RECT rcNotesText = { 30, curY + 10, outW - 30, curY + notesH - 8 };
        DrawLatexRichText(hdcMeasure, g_expNotesText, rcNotesText, colText, colMath, 15, false);
        curY += notesH + 10;
    }

    // 4. Barra de Formula LaTeX 2D generada por la secuencia
    if (g_expIncludeLatexFormula && formulaH > 0) {
        RECT rcFormBox = { 18, curY, outW - 18, curY + formulaH };
        CasioTheme::FillRoundRectAA(hdcMeasure, rcFormBox, 9.0f, bgFormula, bdFormula, 1.1f);

        RECT rcFormTxt = { 30, curY + 8, outW - 30, curY + formulaH - 6 };
        (void)linearExpr;
        DrawLatexRichText(hdcMeasure, latexFormula, rcFormTxt, colText, colMath, 16, true);
        curY += formulaH + 12;
    }

    // 5. Cuadricula de Botones Reales Paso a Paso
    if (outStepCardRects) outStepCardRects->clear();
    if (outStepIndices) outStepIndices->clear();

    int gridLeft = 20;
    int colIdx = 0;
    int rowIdx = 0;
    int activeStepCounter = 1;

    for (int idx = rStart0; idx <= rEnd0 && idx < nTotal; ++idx) {
        const auto& st = g_exporterSteps[idx];
        int cx = gridLeft + colIdx * stepStrideX;
        int cy = curY + rowIdx * (cardH + 14);

        RECT rcCard = { cx, cy, cx + cardW, cy + cardH };
        if (outStepCardRects) outStepCardRects->push_back(rcCard);
        if (outStepIndices) outStepIndices->push_back(idx);

        // Tarjeta del paso con bordes suavizados
        CasioTheme::FillRoundRectAA(hdcMeasure, rcCard, 10.0f, bgCard, st.selected ? bdCardSel : bdCardOff, st.selected ? 1.8f : 1.0f);

        // Encabezado de numero de paso (#1, #2...)
        if (g_expIncludeStepNumbers) {
            RECT rcStepHdr = { rcCard.left + 4, rcCard.top + 4, rcCard.right - 4, rcCard.top + 22 };
            CasioTheme::FillRoundRectAA(hdcMeasure, rcStepHdr, 6.0f, st.selected ? bdCardSel : bdCardOff, CLR_INVALID, 0.0f);

            wchar_t stepLbl[32];
            if (st.selected) {
                swprintf(stepLbl, 32, L"Paso #%d", activeStepCounter++);
            } else {
                swprintf(stepLbl, 32, L"Omitido");
            }
            HFONT hStF = LatexEngine::CreateMathFont(11, false, true, false);
            hOldFont = (HFONT)SelectObject(hdcMeasure, hStF);
            SetTextColor(hdcMeasure, RGB(255, 255, 255));
            DrawTextW(hdcMeasure, stepLbl, -1, &rcStepHdr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMeasure, hOldFont);
            DeleteObject(hStF);
        }

        // Imagen real recortada del boton de la calculadora (o incognita [ ? ] en modo practica para ultimo paso)
        bool maskAsPracticeBlank = g_expHidePracticeAnswer && (idx == rEnd0) && st.selected;
        if (maskAsPracticeBlank) {
            RECT rcQ = { rcCard.left + 14, rcCard.top + 28, rcCard.right - 14, rcCard.top + 84 };
            HBRUSH hBrQ = CreateSolidBrush(light ? RGB(236, 240, 248) : RGB(40, 48, 66));
            HPEN hPenQ = CreatePen(PS_DASH, 1, bdCardSel);
            HGDIOBJ oBq = SelectObject(hdcMeasure, hBrQ);
            HGDIOBJ oPq = SelectObject(hdcMeasure, hPenQ);
            RoundRect(hdcMeasure, rcQ.left, rcQ.top, rcQ.right, rcQ.bottom, 8, 8);
            SelectObject(hdcMeasure, oBq);
            SelectObject(hdcMeasure, oPq);
            DeleteObject(hBrQ);
            DeleteObject(hPenQ);

            HFONT hQF = LatexEngine::CreateMathFont(20, false, true, false);
            hOldFont = (HFONT)SelectObject(hdcMeasure, hQF);
            SetTextColor(hdcMeasure, bdCardSel);
            DrawTextW(hdcMeasure, L"?", -1, &rcQ, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMeasure, hOldFont);
            DeleteObject(hQF);
        } else if (st.isAppCapture || st.keyId < 0) {
            const auto* lcdThumb = FindAppMenuThumb(st.captureFilePath);
            if (lcdThumb && lcdThumb->hBmp && lcdThumb->w > 0 && lcdThumb->h > 0) {
                HDC hdcKey = CreateCompatibleDC(hdcMeasure);
                HBITMAP hOldK = (HBITMAP)SelectObject(hdcKey, lcdThumb->hBmp);
                int availW = cardW - 10;
                int availH = 54;
                double sc = (std::min)((double)availW / lcdThumb->w, (double)availH / lcdThumb->h);
                int dw = (std::max)(24, (int)(lcdThumb->w * sc));
                int dh = (std::max)(16, (int)(lcdThumb->h * sc));
                int dx = rcCard.left + (cardW - dw) / 2;
                int dy = rcCard.top + 28 + (availH - dh) / 2;
                SetStretchBltMode(hdcMeasure, HALFTONE);
                StretchBlt(hdcMeasure, dx, dy, dw, dh, hdcKey, 0, 0, lcdThumb->w, lcdThumb->h, SRCCOPY);
                SelectObject(hdcKey, hOldK);
                DeleteDC(hdcKey);
            }
        } else {
            const auto& capBmp = KeyCaptureForHistory(hdcMeasure, st.calcIdx, st.keyId);
            if (capBmp.bitmap && capBmp.width > 0 && capBmp.height > 0) {
                HDC hdcKey = CreateCompatibleDC(hdcMeasure);
                HBITMAP hOldK = (HBITMAP)SelectObject(hdcKey, capBmp.bitmap);
                int availW = cardW - 12;
                int availH = 58;
                double sc = (std::min)((double)availW / capBmp.width, (double)availH / capBmp.height);
                int dw = (std::max)(24, (int)(capBmp.width * sc));
                int dh = (std::max)(20, (int)(capBmp.height * sc));
                int dx = rcCard.left + (cardW - dw) / 2;
                int dy = rcCard.top + 26 + (availH - dh) / 2;
                SetStretchBltMode(hdcMeasure, HALFTONE);
                StretchBlt(hdcMeasure, dx, dy, dw, dh, hdcKey, 0, 0, capBmp.width, capBmp.height, SRCCOPY);
                SelectObject(hdcKey, hOldK);
                DeleteDC(hdcKey);
            }
        }

        // Etiqueta textual del boton debajo de la foto
        std::wstring keyCap = maskAsPracticeBlank
            ? L"[ Completar ]"
            : ((st.isAppCapture || st.keyId < 0)
                ? (!st.captureLabel.empty() ? st.captureLabel : L"App LCD")
                : std::wstring(KeyHistoryCaption(st.calcIdx, st.keyId)));
        RECT rcCap = { rcCard.left + 2, rcCard.bottom - 22, rcCard.right - 2, rcCard.bottom - 3 };
        DrawLatexRichTextCentered(hdcMeasure, keyCap, rcCap, st.selected ? colText : colMuted, colMath, 11);

        // Flecha direccional (->) hacia el siguiente paso
        if (idx < rEnd0 && colIdx + 1 < colsPerRow) {
            RECT rcArr = { rcCard.right, rcCard.top + 38, rcCard.right + arrowW, rcCard.top + 70 };
            HFONT hArrF = LatexEngine::CreateMathFont(18, false, true, false);
            hOldFont = (HFONT)SelectObject(hdcMeasure, hArrF);
            SetTextColor(hdcMeasure, colMath);
            DrawTextW(hdcMeasure, L"\x2192", -1, &rcArr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdcMeasure, hOldFont);
            DeleteObject(hArrF);
        }

        colIdx++;
        if (colIdx >= colsPerRow) {
            colIdx = 0;
            rowIdx++;
        }
    }

    // 6. Recuadro derecho con Captura de Pantalla LCD del Resultado
    if (g_expIncludeLcdResult) {
        int lcdBoxW = 268;
        int lcdBoxH = 185;
        RECT rcLcdBox = { outW - lcdBoxW - 18, curY, outW - 18, curY + lcdBoxH };
        CasioTheme::FillRoundRectAA(hdcMeasure, rcLcdBox, 10.0f, bgCard, bdCardSel, 1.8f);

        RECT rcLcdTitle = { rcLcdBox.left + 8, rcLcdBox.top + 6, rcLcdBox.right - 8, rcLcdBox.top + 26 };
        HFONT hLTFont = LatexEngine::CreateMathFont(12, false, true, false);
        hOldFont = (HFONT)SelectObject(hdcMeasure, hLTFont);
        SetTextColor(hdcMeasure, colMath);
        DrawTextW(hdcMeasure, g_expHidePracticeAnswer ? L"PANTALLA LCD (PR\u00C1CTICA)" : L"PANTALLA LCD (RESULTADO)",
                  -1, &rcLcdTitle, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdcMeasure, hOldFont);
        DeleteObject(hLTFont);

        int padX = 12;
        RECT rcLcdTarget = { rcLcdBox.left + padX, rcLcdBox.top + 30, rcLcdBox.right - padX, rcLcdBox.top + 30 + (lcdBoxH - 42) };
        if (g_expHidePracticeAnswer) {
            HBRUSH hBrMask = CreateSolidBrush(light ? RGB(234, 240, 249) : RGB(24, 30, 44));
            FillRect(hdcMeasure, &rcLcdTarget, hBrMask);
            DeleteObject(hBrMask);
            HFONT hMF = LatexEngine::CreateMathFont(13, false, true, false);
            hOldFont = (HFONT)SelectObject(hdcMeasure, hMF);
            SetTextColor(hdcMeasure, colMath);
            DrawTextW(hdcMeasure, L"RESPUESTA OCULTA\n\nResuelva en calculadora\ny verifique el valor", -1, &rcLcdTarget, DT_CENTER | DT_VCENTER | DT_WORDBREAK);
            SelectObject(hdcMeasure, hOldFont);
            DeleteObject(hMF);
        } else {
            g_expLastLcdDrawRect = rcLcdTarget;
            int snapW = 0, snapH = 0;
            HBITMAP hSnap = CaptureLcdSnapshot(activeM, 2, snapW, snapH);
            if (hSnap && snapW > 0 && snapH > 0) {
                HDC hdcSnap = CreateCompatibleDC(hdcMeasure);
                HBITMAP hOldSn = (HBITMAP)SelectObject(hdcSnap, hSnap);
                SetStretchBltMode(hdcMeasure, HALFTONE);
                StretchBlt(
                    hdcMeasure,
                    rcLcdTarget.left, rcLcdTarget.top,
                    rcLcdTarget.right - rcLcdTarget.left, rcLcdTarget.bottom - rcLcdTarget.top,
                    hdcSnap, 0, 0, snapW, snapH, SRCCOPY
                );
                SelectObject(hdcSnap, hOldSn);
                DeleteDC(hdcSnap);
                DeleteObject(hSnap);
            }
            for (const auto& ann : g_expAnnotations) {
                if (!ann.fullSheet) DrawAnnotationVector(hdcMeasure, rcLcdTarget, ann);
            }
        }
    }

    // 7. Pie de pagina con resumen lineal de teclas
    if (g_expIncludeSummary) {
        RECT rcFooter = { 20, outH - 32, outW - 20, outH - 8 };
        std::wstring footTxt = L"Resumen de Secuencia:  " + arrowSteps;
        DrawLatexRichText(hdcMeasure, footTxt, rcFooter, colMuted, colMath, 12, true);
    }

    // 8. Capa de dibujo a mano alzada, formas y notas personalizadas sobre toda la ficha
    for (const auto& ann : g_expAnnotations) {
        if (ann.fullSheet) DrawAnnotationVector(hdcMeasure, rcAll, ann);
    }
    DrawFreehandStrokesOnSheet(hdcMeasure, outW, outH);
    DrawSheetNotesOverlay(hdcMeasure, outW, outH, light);

    SelectObject(hdcMeasure, hOldBmp);
    DeleteDC(hdcMeasure);
    ReleaseDC(nullptr, hScreen);
    return hSheetBmp;
}

inline std::wstring ExportPedagogicalSheetToPath(const std::wstring& fullPath, bool copyToClip) {
    int w = 0, h = 0;
    HBITMAP hSheet = RenderPedagogicalSheetBitmap(w, h, nullptr, nullptr);
    if (!hSheet) return L"Error al renderizar la ficha de gu\u00EDa.";

    int nTotal = (int)g_exporterSteps.size();
    int rStart0 = (std::max)(0, (std::min)(g_expRangeStart - 1, (std::max)(0, nTotal - 1)));
    int rEnd0   = (std::max)(rStart0, (std::min)(g_expRangeEnd - 1, (std::max)(0, nTotal - 1)));
    std::wstring linearExpr, latexFormula, arrowSteps;
    ConvertStepsToLinearAndLatex(g_exporterSteps, rStart0, rEnd0, linearExpr, latexFormula, arrowSteps);

    std::wstring textSummary =
        g_expTitleText + L"\r\n" +
        g_expNotesText + L"\r\n" +
        L"F\u00F3rmula LaTeX: " + latexFormula + L"\r\n" +
        L"Secuencia de Botones: " + arrowSteps + L"\r\n";

    std::wstring resultMsg;
    if (copyToClip) {
        PutBitmapAndOrTextToClipboard(g_hwndExporter ? g_hwndExporter : g_hMainWnd, hSheet, textSummary, CLIP_COPY_BOTH_IMAGE_AND_TEXT);
        resultMsg = L"Gu\u00EDa copiada al Portapapeles (Imagen + Texto LaTeX). ";
    }

    if (!fullPath.empty()) {
        std::wstring targetPath = fullPath;
        if (SaveBitmapByExtension(targetPath.c_str(), hSheet, w, h)) {
            resultMsg += L"Guardada en: " + targetPath;
        } else {
            size_t dotPos = targetPath.find_last_of(L'.');
            if (dotPos != std::wstring::npos && _wcsicmp(targetPath.c_str() + dotPos, L".png") == 0) {
                targetPath = targetPath.substr(0, dotPos) + L".bmp";
            }
            if (SaveBitmapToFile(targetPath.c_str(), hSheet, w, h)) {
                resultMsg += L"Guardada (BMP respaldo) en: " + targetPath;
            } else {
                resultMsg += L"Error al guardar archivo en: " + fullPath;
            }
        }
    }

    DeleteObject(hSheet);
    g_expStatusMsg = resultMsg;
    if (g_hwndExporter) InvalidateRect(g_hwndExporter, nullptr, FALSE);
    return resultMsg;
}

inline std::wstring ExportPedagogicalSheetToFileAndClipboard(bool copyToClip, bool saveToFile) {
    if (!saveToFile) {
        return ExportPedagogicalSheetToPath(L"", copyToClip);
    }
    std::wstring dir = ResolveWritableOutputDir(L"guias");
    std::time_t now = std::time(nullptr);
    std::tm tmv = {};
    localtime_s(&tmv, &now);
    wchar_t fname[128];
    swprintf(fname, 128, L"\\guia_paso_a_paso_%04d%02d%02d_%02d%02d%02d.png",
        tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
        tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    return ExportPedagogicalSheetToPath(dir + fname, copyToClip);
}

inline std::wstring SavePedagogicalSheetWithSaveAsDialog(HWND owner) {
    std::wstring initDir = ResolveWritableOutputDir(L"guias");
    std::time_t now = std::time(nullptr);
    std::tm tmv = {};
    localtime_s(&tmv, &now);
    wchar_t fileBuf[MAX_PATH] = {};
    swprintf(fileBuf, MAX_PATH, L"%s\\guia_paso_a_paso_%04d%02d%02d_%02d%02d%02d.png",
        initDir.c_str(),
        tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday,
        tmv.tm_hour, tmv.tm_min, tmv.tm_sec);

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = L"Imagen PNG (Lossless)\0*.png\0Mapa de bits BMP\0*.bmp\0\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fileBuf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.lpstrDefExt = L"png";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetSaveFileNameW(&ofn)) {
        return L"Guardado cancelado.";
    }
    return ExportPedagogicalSheetToPath(fileBuf, true);
}

// ----------------------------------------------------------------------------
// Ventana de Configuracion de Portapapeles y Modo Presentacion (CasioSettingsWnd)
// ----------------------------------------------------------------------------
inline POINT g_settingsHoverPt = { -1, -1 };
inline bool  g_settingsTrackingHover = false;

inline void ComputeSettingsLayout(
    int cw,
    RECT (&rcOpt)[3],
    RECT (&rcChk)[5],
    RECT& rcTestBtn,
    RECT& rcExpBtn,
    RECT& rcSaveBtn)
{
    int w = (std::max)(660, cw);
    for (int i = 0; i < 3; ++i) {
        rcOpt[i] = { 24, 92 + i * 56, w - 24, 92 + i * 56 + 50 };
    }
    for (int i = 0; i < 5; ++i) {
        rcChk[i] = { 24, 272 + i * 38, w - 24, 272 + (i + 1) * 38 };
    }
    int gap = 12;
    int btnW = (w - 48 - 2 * gap) / 3;
    rcTestBtn = { 24, 554, 24 + btnW, 594 };
    rcExpBtn  = { 24 + btnW + gap, 554, 24 + 2 * btnW + gap, 594 };
    rcSaveBtn = { 24 + 2 * (btnW + gap), 554, w - 24, 594 };
}

inline float GetSettingsUiScale(HWND hwnd) {
    return CasioTheme::ComputeDialogUiScale(hwnd, 720, 646);
}

inline LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CasioTheme::ApplyDwmDarkTitleBar(hwnd, CasioTheme::g_studioDarkTheme);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        const float s = GetSettingsUiScale(hwnd);
        g_settingsHoverPt = {
            CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s),
            CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s)
        };
        if (!g_settingsTrackingHover) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            g_settingsTrackingHover = true;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        RECT rcOpt[3], rcChk[5], r1, r2, r3;
        ComputeSettingsLayout(CasioTheme::DeviceToLogical(rc.right, s), rcOpt, rcChk, r1, r2, r3);
        bool over = PtInRect(&r1, g_settingsHoverPt) || PtInRect(&r2, g_settingsHoverPt) || PtInRect(&r3, g_settingsHoverPt);
        for (int i = 0; i < 3 && !over; ++i) if (PtInRect(&rcOpt[i], g_settingsHoverPt)) over = true;
        for (int i = 0; i < 5 && !over; ++i) if (PtInRect(&rcChk[i], g_settingsHoverPt)) over = true;
        SetCursor(LoadCursor(nullptr, over ? IDC_HAND : IDC_ARROW));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_settingsTrackingHover = false;
        g_settingsHoverPt = { -1, -1 };
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        const float s = GetSettingsUiScale(hwnd);
        POINT pt = {
            CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s),
            CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s)
        };
        RECT rc{};
        GetClientRect(hwnd, &rc);
        RECT rcOpt[3], rcChk[5], rcTestBtn, rcExpBtn, rcSaveBtn;
        ComputeSettingsLayout(CasioTheme::DeviceToLogical(rc.right, s), rcOpt, rcChk, rcTestBtn, rcExpBtn, rcSaveBtn);

        for (int i = 0; i < 3; ++i) {
            if (PtInRect(&rcOpt[i], pt)) {
                g_clipCopyMode = i;
                SaveUnifiedSettingsToIni();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        bool* flags[5] = {
            &g_autoCopyOnEvaluate,
            &g_includeLatexInTextCopy,
            &g_copyFullBodyInsteadOfLcd,
            &g_teacherHighlightMode,
            &g_exportSheetLightTheme
        };
        for (int i = 0; i < 5; ++i) {
            if (PtInRect(&rcChk[i], pt)) {
                *(flags[i]) = !(*(flags[i]));
                SaveUnifiedSettingsToIni();
                InvalidateRect(hwnd, nullptr, FALSE);
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
                if (g_hMainWnd) InvalidateRect(g_hMainWnd, nullptr, FALSE);
                return 0;
            }
        }

        if (PtInRect(&rcTestBtn, pt)) {
            CopyCalculatorResultSmart(g_activeCalcIdx, g_clipCopyMode, true);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&rcExpBtn, pt)) {
            OpenSequenceExporterWindow(true);
            return 0;
        }
        if (PtInRect(&rcSaveBtn, pt)) {
            SaveUnifiedSettingsToIni();
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcPhys{};
        GetClientRect(hwnd, &rcPhys);
        if (rcPhys.right <= 0 || rcPhys.bottom <= 0) {
            EndPaint(hwnd, &ps);
            return 0;
        }

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rcPhys.right, rcPhys.bottom);
        HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);

        HFONT hSecF   = CasioTheme::CreateUiFont(12, true);
        HFONT hTitleF = CasioTheme::CreateUiFont(13, true);
        HFONT hMetaF  = CasioTheme::CreateUiFont(11, true);
        HFONT hBtnF   = CasioTheme::CreateUiFont(13, true);

        const float s = GetSettingsUiScale(hwnd);
        {
            CasioTheme::UiScaleScope scaleScope(memDC, s);
            RECT rc = {
                0, 0,
                CasioTheme::DeviceToLogical(rcPhys.right, s),
                CasioTheme::DeviceToLogical(rcPhys.bottom, s)
            };

            HBRUSH hBg = CreateSolidBrush(CasioTheme::StudioCanvasBg());
            FillRect(memDC, &rc, hBg);
            DeleteObject(hBg);
            SetBkMode(memDC, TRANSPARENT);

        RECT rcOpt[3], rcChk[5], rcTestBtn, rcExpBtn, rcSaveBtn;
        ComputeSettingsLayout(rc.right, rcOpt, rcChk, rcTestBtn, rcExpBtn, rcSaveBtn);

        // Header stage material translúcido 2.0
        RECT rcHdr = { 0, 0, rc.right, 60 };
        CasioTheme::DrawTranslucentHeaderStrip(memDC, rcHdr, true);

        RECT rcTitle = { 24, 10, rc.right - 24, 52 };
        DrawLatexRichText(
            memDC,
            L"Portapapeles y Preferencias",
            rcTitle, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 15, true
        );

        // Section 1: Clipboard Copy Mode
        SelectObject(memDC, hSecF);
        SetTextColor(memDC, CasioTheme::StudioTextSec());
        RECT rcSec1 = { 24, 68, rc.right - 24, 88 };
        DrawTextW(memDC, L"Modo de copia al Portapapeles", -1, &rcSec1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const wchar_t* optTitles[3] = {
            L"Solo captura de pantalla (Imagen LCD n\u00EDtida)",
            L"Solo texto (Expresi\u00F3n, f\u00F3rmula y pasos)",
            L"Ambas simult\u00E1neamente (Imagen LCD + Texto)"
        };
        const wchar_t* optDescs[3] = {
            L"Ideal para pegar directamente la pantalla de la calculadora en Word, Paint o diapositivas.",
            L"Ideal para pegar f\u00F3rmulas en Overleaf/LaTeX, c\u00F3digo o mensajer\u00EDa.",
            L"Permite que cada aplicaci\u00F3n pegue el formato adecuado (imagen o texto LaTeX)."
        };

        for (int i = 0; i < 3; ++i) {
            bool sel = (g_clipCopyMode == i);
            bool hov = PtInRect(&rcOpt[i], g_settingsHoverPt) != FALSE;
            COLORREF cardFill = sel ? CasioTheme::StudioAccentSoft() : (hov ? CasioTheme::StudioSurfaceAlt() : CasioTheme::StudioSurface());
            COLORREF cardBd   = sel ? CasioTheme::StudioAccent() : (hov ? CasioTheme::StudioBorderStrong() : CasioTheme::StudioBorder());
            CasioTheme::FillRoundRectAA(memDC, rcOpt[i], 9.0f, cardFill, cardBd, sel ? 1.5f : 1.0f);

            int cy = (rcOpt[i].top + rcOpt[i].bottom) / 2;
            CasioTheme::DrawRadioCircle(memDC, rcOpt[i].left + 22, cy, 8, sel, CasioTheme::StudioAccent());

            RECT rcT1 = { rcOpt[i].left + 40, rcOpt[i].top + 4, rcOpt[i].right - 16, rcOpt[i].top + 28 };
            DrawLatexRichText(memDC, optTitles[i], rcT1,
                              CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

            RECT rcT2 = { rcOpt[i].left + 40, rcOpt[i].top + 27, rcOpt[i].right - 16, rcOpt[i].bottom - 4 };
            DrawLatexRichText(memDC, optDescs[i], rcT2, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, true);
        }

        // Section 2: Toggle switches grouped cleanly inside one surface card
        RECT rcChkCard = { 24, 272, rc.right - 24, 462 };
        CasioTheme::FillRoundRectAA(memDC, rcChkCard, 10.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorder(), 1.0f);

        struct ChkRow { bool val; const wchar_t* label; } rows[5] = {
            { g_autoCopyOnEvaluate,       L"Auto-copiar resultado al evaluar con [=] o [EXE]" },
            { g_includeLatexInTextCopy,   L"Incluir c\u00F3digo LaTeX ($\\sqrt{x}, \\frac{a}{b}, \\int$) al copiar como texto" },
            { g_copyFullBodyInsteadOfLcd, L"Capturar cuerpo completo de la calculadora en lugar de solo el LCD" },
            { g_teacherHighlightMode,     L"Modo Presentaci\u00F3n: resaltar en el teclado la \u00FAltima tecla presionada" },
            { g_exportSheetLightTheme,    L"Tema claro imprimible (fondo blanco) al exportar gu\u00EDas" }
        };

        for (int i = 0; i < 5; ++i) {
            bool hov = PtInRect(&rcChk[i], g_settingsHoverPt) != FALSE;
            if (hov) {
                RECT rcHovRow = { rcChk[i].left + 4, rcChk[i].top + 2, rcChk[i].right - 4, rcChk[i].bottom - 2 };
                CasioTheme::FillRoundRectAA(memDC, rcHovRow, 6.0f, CasioTheme::StudioSurfaceAlt(), CLR_INVALID, 0.0f);
            }
            if (i < 4) {
                HPEN hDiv = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
                HGDIOBJ op = SelectObject(memDC, hDiv);
                MoveToEx(memDC, rcChk[i].left + 14, rcChk[i].bottom, nullptr);
                LineTo(memDC, rcChk[i].right - 14, rcChk[i].bottom);
                SelectObject(memDC, op);
                DeleteObject(hDiv);
            }
            int rowMidY = (rcChk[i].top + rcChk[i].bottom) / 2;
            RECT rcSwitch = { rcChk[i].right - 56, rowMidY - 10, rcChk[i].right - 16, rowMidY + 10 };
            CasioTheme::DrawToggleSwitch(memDC, rcSwitch, rows[i].val, CasioTheme::StudioAccent(), !CasioTheme::g_studioDarkTheme);

            RECT rcTxt = { rcChk[i].left + 14, rcChk[i].top + 3, rcSwitch.left - 12, rcChk[i].bottom - 3 };
            DrawLatexRichText(memDC, rows[i].label, rcTxt,
                              rows[i].val ? CasioTheme::StudioText() : CasioTheme::StudioTextSec(),
                              CasioTheme::StudioAccentText(), 13, true);
        }

        // Section 3: Live 2D formula preview
        RECT rcDemo = { 24, 474, rc.right - 24, 542 };
        CasioTheme::FillRoundRectAA(memDC, rcDemo, 9.0f, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), 1.0f);

        RECT rcDemoTxt = { 36, 480, rc.right - 36, 536 };
        DrawLatexRichText(
            memDC,
            L"$x = \\frac{-b \\pm \\sqrt{b^{2} - 4ac}}{2a}$    \x00B7    $\\int_{0}^{\\pi} \\sin(\\theta)\\,d\\theta = 2$    \x00B7    $\\sum_{k=1}^{n} k^{2} = \\frac{n(n+1)(2n+1)}{6}$",
            rcDemoTxt, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 14, true
        );

        // Action dock buttons
        CasioTheme::DrawStudioButton(memDC, rcTestBtn, L"Probar Copia Ahora", hBtnF, CasioTheme::BTN_PRIMARY,
                                     false, PtInRect(&rcTestBtn, g_settingsHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, rcExpBtn, L"Abrir Exportador de Gu\u00EDas", hBtnF, CasioTheme::BTN_SUCCESS,
                                     false, PtInRect(&rcExpBtn, g_settingsHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, rcSaveBtn, L"Guardar y Cerrar", hBtnF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&rcSaveBtn, g_settingsHoverPt) != FALSE, 7.0f);

        if (!g_clipStatusText.empty()) {
            RECT rcSt = { 24, 604, rc.right - 24, 632 };
            SelectObject(memDC, hMetaF);
            SetTextColor(memDC, CasioTheme::StudioSuccessText());
            DrawTextW(memDC, g_clipStatusText.c_str(), -1, &rcSt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }
        } // UiScaleScope

        BitBlt(hdc, 0, 0, rcPhys.right, rcPhys.bottom, memDC, 0, 0, SRCCOPY);
        DeleteObject(hSecF);
        DeleteObject(hTitleF);
        DeleteObject(hMetaF);
        DeleteObject(hBtnF);
        SelectObject(memDC, hOld);
        DeleteObject(hBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void OpenSettingsWindow() {
    static float s_lastSettingsScale = 0.0f;
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    const float dlgScale = GetSettingsUiScale(g_hwndSettings);
    if (!g_hwndSettings) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CasioTheme::GetStudioSurfaceBrush();
        wc.lpszClassName = L"CasioSettingsWndClass";
        RegisterClassExW(&wc);

        RECT wr = { 0, 0, CasioTheme::UiToDevice(720, dlgScale), CasioTheme::UiToDevice(646, dlgScale) };
        AdjustWindowRectEx(&wr, style, FALSE, WS_EX_APPWINDOW);
        g_hwndSettings = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"CasioSettingsWndClass",
            L"Configuraci\u00F3n",
            style,
            CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
            g_hMainWnd, nullptr, GetModuleHandleW(nullptr), nullptr
        );
        s_lastSettingsScale = dlgScale;
    } else {
        const float activeScale = GetSettingsUiScale(g_hwndSettings);
        if (s_lastSettingsScale - activeScale > 0.01f || activeScale - s_lastSettingsScale > 0.01f) {
            RECT wr = { 0, 0, CasioTheme::UiToDevice(720, activeScale), CasioTheme::UiToDevice(646, activeScale) };
            AdjustWindowRectEx(&wr, style, FALSE, WS_EX_APPWINDOW);
            SetWindowPos(g_hwndSettings, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            s_lastSettingsScale = activeScale;
        }
    }
    CasioTheme::ApplyDwmDarkTitleBar(g_hwndSettings, CasioTheme::g_studioDarkTheme);
    ShowWindow(g_hwndSettings, SW_SHOW);
    SetForegroundWindow(g_hwndSettings);
    InvalidateRect(g_hwndSettings, nullptr, FALSE);
}

// ----------------------------------------------------------------------------
// Ventana del Exportador de Guias de Secuencias de Botones
// ----------------------------------------------------------------------------
inline std::vector<RECT> g_expLastCardRects;
inline std::vector<int>  g_expLastCardIndices;
inline RECT  g_expPreviewDestRect = { 28, 260, 992, 678 };
inline int   g_expLastSheetW = 920;
inline int   g_expLastSheetH = 420;
inline POINT g_expHoverPt = { -1, -1 };
inline bool  g_expTrackingHover = false;

struct ExporterLayout {
    RECT rcTitleChrome{};
    RECT rcTitleEdit{};
    RECT rcNotesChrome{};
    RECT rcNotesEdit{};

    RECT rcRangeStartMinus{};
    RECT rcRangeStartVal{};
    RECT rcRangeStartPlus{};
    RECT rcRangeEndMinus{};
    RECT rcRangeEndVal{};
    RECT rcRangeEndPlus{};
    RECT rcBtnSyncHist{};
    RECT rcBtnPresetCycle{};
    RECT rcBtnEquivModel{};
    RECT rcThemeSegTrack{};
    RECT rcThemeSeg[2]{};

    RECT rcFmtSegTrack{};
    RECT rcFmtSeg[4]{};
    RECT rcToggleChips[5]{};

    RECT rcPreviewStage{};
    RECT rcPreviewInner{};

    RECT rcDockCard{};
    RECT rcAnnotSegTrack{};
    RECT rcAnnotSeg[7]{};
    RECT rcInkSwatch[4]{};
    RECT rcBtnUndoAnnot{};
    RECT rcBtnClearAnnot{};
    RECT rcBtnCopyClip{};
    RECT rcBtnSavePng{};
    RECT rcBtnSaveAs{};
    RECT rcBtnClose{};
};

inline ExporterLayout ComputeExporterLayout(int cw, int ch) {
    ExporterLayout L{};
    int w = (std::max)(820, cw);
    int h = (std::max)(660, ch);

    // Top form inputs (Title + 3-line Notes with full headroom for \int and \frac)
    L.rcTitleChrome = { 172, 50, w - 28, 76 };
    L.rcTitleEdit   = { 180, 54, w - 36, 72 };
    L.rcNotesChrome = { 172, 82, w - 28, 174 };
    L.rcNotesEdit   = { 180, 86, w - 36, 170 };

    // Row 1 controls (Y = 188..220)
    L.rcRangeStartMinus = { 66, 188, 92, 220 };
    L.rcRangeStartVal   = { 94, 188, 134, 220 };
    L.rcRangeStartPlus  = { 136, 188, 162, 220 };

    L.rcRangeEndMinus   = { 218, 188, 244, 220 };
    L.rcRangeEndVal     = { 246, 188, 286, 220 };
    L.rcRangeEndPlus    = { 288, 188, 314, 220 };

    L.rcBtnSyncHist     = { 326, 188, 446, 220 };
    L.rcBtnPresetCycle  = { 454, 188, 558, 220 };
    L.rcBtnEquivModel   = { 566, 188, 686, 220 };

    L.rcThemeSegTrack   = { w - 208, 188, w - 18, 220 };
    int tw = (L.rcThemeSegTrack.right - L.rcThemeSegTrack.left - 8) / 2;
    L.rcThemeSeg[0]     = { L.rcThemeSegTrack.left + 4, L.rcThemeSegTrack.top + 3,
                            L.rcThemeSegTrack.left + 4 + tw, L.rcThemeSegTrack.bottom - 3 };
    L.rcThemeSeg[1]     = { L.rcThemeSeg[0].right + 2, L.rcThemeSegTrack.top + 3,
                            L.rcThemeSegTrack.right - 4, L.rcThemeSegTrack.bottom - 3 };

    // Row 2 Format Segmented Bar & Toggle Chips (Y = 226..258)
    int fmtRight = (std::min)(436, w * 43 / 100);
    L.rcFmtSegTrack = { 18, 226, fmtRight, 258 };
    int fw = (fmtRight - 18 - 14) / 4;
    for (int i = 0; i < 4; ++i) {
        int fx = 22 + i * (fw + 2);
        L.rcFmtSeg[i] = { fx, 229, (i == 3) ? (fmtRight - 4) : (fx + fw), 255 };
    }

    int chipLeft = fmtRight + 10;
    int chipAvail = (w - 18) - chipLeft;
    int chipGap = 6;
    int chipW = (chipAvail - 4 * chipGap) / 5;
    for (int i = 0; i < 5; ++i) {
        int cx = chipLeft + i * (chipW + chipGap);
        L.rcToggleChips[i] = { cx, 227, (i == 4) ? (w - 18) : (cx + chipW), 257 };
    }

    // Bottom Dock anchor
    int dockBottom = h - 32;
    int dockTop = dockBottom - 62;
    L.rcDockCard = { 18, dockTop, w - 18, dockBottom };

    // Preview Stage sits cleanly between row 2 caption and dock
    L.rcPreviewStage = { 18, 282, w - 18, dockTop - 8 };
    L.rcPreviewInner = { 28, 290, w - 28, dockTop - 16 };

    // Dock Left: 7-tool visual annotator (Off/Lapiz/Texto/Flecha/Circ./Marco/#N) + 4 ink swatches + Undo + Clear
    int annotY1 = dockTop + 14;
    int annotY2 = dockTop + 48;
    int trackLeft = 26;
    int trackRight = (std::min)(352, w - 664);
    L.rcAnnotSegTrack = { trackLeft, annotY1, trackRight, annotY2 };
    int aw = (trackRight - trackLeft - 20) / 7;
    for (int i = 0; i < 7; ++i) {
        int ax = trackLeft + 4 + i * (aw + 2);
        L.rcAnnotSeg[i] = { ax, annotY1 + 3, (i == 6) ? (trackRight - 4) : (ax + aw), annotY2 - 3 };
    }
    int swX = trackRight + 8;
    int swCy = (annotY1 + annotY2) / 2;
    for (int i = 0; i < 4; ++i) {
        L.rcInkSwatch[i] = { swX + i * 20, swCy - 9, swX + i * 20 + 18, swCy + 9 };
    }
    int actLeft = swX + 82;
    L.rcBtnUndoAnnot  = { actLeft, annotY1, actLeft + 66, annotY2 };
    L.rcBtnClearAnnot = { actLeft + 70, annotY1, actLeft + 130, annotY2 };

    // Dock Right: Action cluster
    int btnY1 = dockTop + 12;
    int btnY2 = dockTop + 50;
    L.rcBtnClose    = { w - 100, btnY1, w - 28,  btnY2 };
    L.rcBtnSaveAs   = { w - 230, btnY1, w - 108, btnY2 };
    L.rcBtnSavePng  = { w - 352, btnY1, w - 238, btnY2 };
    L.rcBtnCopyClip = { w - 438, btnY1, w - 360, btnY2 };

    return L;
}

inline void LoadPresetPedagogicalGuide(int presetId) {
    if (presetId == 0) {
        LoadCustomSequenceIntoExporter(
            0,
            { 10, 11, 14, 43, 41, 11, 4, 45, 4, 42, 49 },
            L"Gu\u00EDa #1: Integral Definida en ClassWiz ($\\int_{0}^{3} (x^{2} + 2x)\\,dx = 18$)",
            L"1) Presione el bot\u00F3n de integral $\\int_{a}^{b} f(x)\\,dx$ e ingrese la funci\u00F3n $f(x) = x^{2} + 2x$.\r\n"
            L"2) Desplace el cursor [DERECHA] para definir el l\u00EDmite inferior $a = 0$ y superior $b = 3$.\r\n"
            L"3) Presione [=] para obtener la soluci\u00F3n exacta por cuadratura de Gauss-Kronrod."
        );
    } else if (presetId == 1) {
        LoadCustomSequenceIntoExporter(
            0,
            { 6, 18, 41, 41, 40, 49, 44, 36, 49, 37, 49, 49 },
            L"Gu\u00EDa #2: Ra\u00EDces de Ecuaci\u00F3n Cuadr\u00E1tica ($x = \\frac{-b \\pm \\sqrt{b^{2} - 4ac}}{2a}$)",
            L"1) Entre a [MENU] -> [A] (Ecuaci\u00F3n/Funci\u00F3n) y elija Polin\u00F3mica de grado 2 para resolver $ax^{2} + bx + c = 0$.\r\n"
            L"2) Ingrese los coeficientes de $x^{2} - 5x + 6 = 0$ ($a=1, b=-5, c=6$).\r\n"
            L"3) Presione [=] para obtener las ra\u00EDces $x_{1} = 3$, $x_{2} = 2$ y el v\u00E9rtice de la par\u00E1bola."
        );
    } else if (presetId == 2) {
        LoadCustomSequenceIntoExporter(
            1,
            { 0, 9, 21, 3, 27, 27, 6, 45, 49 },
            L"Gu\u00EDa #3: Derivada Num\u00E9rica en fx-570ES PLUS ($\\frac{d}{dx}[\\sin(x)]_{x=0} = 1$)",
            L"1) Presione [SHIFT] seguido de [INTEGRAL] para activar el operador derivada $\\frac{d}{dx}$.\r\n"
            L"2) Ingrese $\\sin(X)$ usando [ALPHA] + [)] y mu\u00E9vase con [DERECHA] para evaluar en $x = 0$ ($\\lim_{x \\to 0} \\frac{\\sin(x)}{x} = 1$)."
        );
    } else {
        LoadCustomSequenceIntoExporter(
            2,
            { 9, 36, 49, 21, 18, 27, 49, 5 },
            L"Gu\u00EDa #4: Graficaci\u00F3n de $Y_{1} = \\sin(x)$ y An\u00E1lisis G-Solv en fx-CG50",
            L"1) Presione [MENU] y elija el icono [5] (Graph) en la calculadora graficadora fx-CG50.\r\n"
            L"2) Ingrese la funci\u00F3n $Y_{1} = \\sin(x)$ usando el bot\u00F3n de variable $[X,\\theta,T]$ y presione [EXE].\r\n"
            L"3) Presione [F6] (DRAW) para trazar la curva y [F5] (G-Solv) para hallar ra\u00EDces, m\u00E1ximos e $\\int f(x)\\,dx$."
        );
    }
}

inline float GetExporterUiScale(HWND hwnd) {
    return CasioTheme::ComputeDialogUiScale(hwnd, 1020, 792);
}

inline int g_expEditFontPx = 0;

inline void SyncExporterChildEdits(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    const float s = GetExporterUiScale(hwnd);
    const int wantPx = (std::max)(12, CasioTheme::UiToDevice(14, s));
    if (!g_hExpEditFont || g_expEditFontPx != wantPx) {
        if (g_hExpEditFont) { DeleteObject(g_hExpEditFont); g_hExpEditFont = nullptr; }
        g_hExpEditFont = CreateFontW(-wantPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_expEditFontPx = wantPx;
        if (g_hwndExpTitleEdit) SendMessageW(g_hwndExpTitleEdit, WM_SETFONT, (WPARAM)g_hExpEditFont, TRUE);
        if (g_hwndExpNotesEdit) SendMessageW(g_hwndExpNotesEdit, WM_SETFONT, (WPARAM)g_hExpEditFont, TRUE);
        if (g_hwndExpInlineNoteEdit) SendMessageW(g_hwndExpInlineNoteEdit, WM_SETFONT, (WPARAM)g_hExpEditFont, TRUE);
    }
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int logicalW = rc.right ? CasioTheme::DeviceToLogical(rc.right, s) : 1020;
    const int logicalH = rc.bottom ? CasioTheme::DeviceToLogical(rc.bottom, s) : 792;
    ExporterLayout L = ComputeExporterLayout(logicalW, logicalH);
    if (g_hwndExpTitleEdit) {
        MoveWindow(g_hwndExpTitleEdit,
                   CasioTheme::UiToDevice(L.rcTitleEdit.left, s),
                   CasioTheme::UiToDevice(L.rcTitleEdit.top, s),
                   (std::max)(120, CasioTheme::UiToDevice(L.rcTitleEdit.right - L.rcTitleEdit.left, s)),
                   (std::max)(18, CasioTheme::UiToDevice(L.rcTitleEdit.bottom - L.rcTitleEdit.top, s)),
                   TRUE);
        ShowWindow(g_hwndExpTitleEdit, g_expEditingTitle ? SW_SHOW : SW_HIDE);
    }
    if (g_hwndExpNotesEdit) {
        MoveWindow(g_hwndExpNotesEdit,
                   CasioTheme::UiToDevice(L.rcNotesEdit.left, s),
                   CasioTheme::UiToDevice(L.rcNotesEdit.top, s),
                   (std::max)(120, CasioTheme::UiToDevice(L.rcNotesEdit.right - L.rcNotesEdit.left, s)),
                   (std::max)(36, CasioTheme::UiToDevice(L.rcNotesEdit.bottom - L.rcNotesEdit.top, s)),
                   TRUE);
        ShowWindow(g_hwndExpNotesEdit, g_expEditingNotes ? SW_SHOW : SW_HIDE);
    }
}

inline void CommitInlineSheetNote(HWND hwndOwner, bool discard = false) {
    if (!g_expInlineNoteActive) return;
    g_expInlineNoteActive = false;
    if (g_hwndExpInlineNoteEdit && IsWindow(g_hwndExpInlineNoteEdit)) {
        if (!discard) {
            wchar_t buf[256] = {};
            GetWindowTextW(g_hwndExpInlineNoteEdit, buf, 255);
            std::wstring txt = buf;
            while (!txt.empty() && (txt.back() == L' ' || txt.back() == L'\r' || txt.back() == L'\n')) txt.pop_back();
            while (!txt.empty() && (txt.front() == L' ' || txt.front() == L'\r' || txt.front() == L'\n')) txt.erase(txt.begin());
            if (!txt.empty()) {
                GuideSheetNote n;
                n.normX = g_expInlineNoteNorm.x;
                n.normY = g_expInlineNoteNorm.y;
                n.text = std::move(txt);
                n.color = GetExpAnnotInkColor();
                g_expSheetNotes.push_back(std::move(n));
                g_expUndoStack.push_back(ExpUndoKind::Note);
                g_expStatusMsg = L"Nota / indicaci\u00F3n agregada sobre la gu\u00EDa.";
            }
        }
        SetWindowTextW(g_hwndExpInlineNoteEdit, L"");
        ShowWindow(g_hwndExpInlineNoteEdit, SW_HIDE);
    }
    if (hwndOwner && IsWindow(hwndOwner)) InvalidateRect(hwndOwner, nullptr, FALSE);
}

inline LRESULT CALLBACK InlineNoteEditSubclassProc(
    HWND hwndEdit, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    (void)uIdSubclass;
    HWND hwndOwner = reinterpret_cast<HWND>(dwRefData);
    if (msg == WM_GETDLGCODE) {
        return DLGC_WANTALLKEYS;
    }
    if (msg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            CommitInlineSheetNote(hwndOwner, false);
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            CommitInlineSheetNote(hwndOwner, true);
            g_expStatusMsg = L"Edici\u00F3n de nota cancelada.";
            return 0;
        }
    }
    return DefSubclassProc(hwndEdit, msg, wParam, lParam);
}

inline LRESULT CALLBACK SequenceExporterWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CasioTheme::ApplyDwmDarkTitleBar(hwnd, CasioTheme::g_studioDarkTheme);
        if (g_hExpEditFont) { DeleteObject(g_hExpEditFont); g_hExpEditFont = nullptr; }
        g_expEditFontPx = 0;

        g_expEditingTitle = false;
        g_expEditingNotes = false;
        g_expDrawingActive = false;
        g_expInlineNoteActive = false;
        g_hwndExpTitleEdit = CreateWindowExW(
            0, L"EDIT", g_expTitleText.c_str(),
            WS_CHILD | ES_AUTOHSCROLL,
            180, 54, 400, 18,
            hwnd, (HMENU)9201, GetModuleHandleW(nullptr), nullptr
        );

        g_hwndExpNotesEdit = CreateWindowExW(
            0, L"EDIT", g_expNotesText.c_str(),
            WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
            180, 86, 400, 56,
            hwnd, (HMENU)9202, GetModuleHandleW(nullptr), nullptr
        );
        g_hwndExpInlineNoteEdit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            100, 300, 210, 26,
            hwnd, (HMENU)9203, GetModuleHandleW(nullptr), nullptr
        );
        if (g_hwndExpInlineNoteEdit) {
            SetWindowSubclass(g_hwndExpInlineNoteEdit, InlineNoteEditSubclassProc, 1, reinterpret_cast<DWORD_PTR>(hwnd));
        }
        CasioTheme::EnableThemeScrollbar(g_hwndExpNotesEdit, CasioTheme::g_studioDarkTheme);
        SyncExporterChildEdits(hwnd);
        return 0;
    }

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        SyncExporterChildEdits(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_SIZE: {
        SyncExporterChildEdits(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_CTLCOLOREDIT:
        return CasioTheme::HandleStudioEditCtlColor(wParam);

    case WM_ERASEBKGND:
        return 1;

    case WM_DESTROY: {
        if (g_hwndExpInlineNoteEdit) {
            RemoveWindowSubclass(g_hwndExpInlineNoteEdit, InlineNoteEditSubclassProc, 1);
            g_hwndExpInlineNoteEdit = nullptr;
        }
        g_hwndExpTitleEdit = nullptr;
        g_hwndExpNotesEdit = nullptr;
        g_hwndExporter = nullptr;
        g_expDrawingActive = false;
        g_expInlineNoteActive = false;
        if (g_hExpEditFont) { DeleteObject(g_hExpEditFont); g_hExpEditFont = nullptr; }
        g_expEditFontPx = 0;
        return 0;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);
        if (code == EN_CHANGE) {
            if (id == 9201 && g_expEditingTitle && g_hwndExpTitleEdit) {
                wchar_t tbuf[512] = {};
                GetWindowTextW(g_hwndExpTitleEdit, tbuf, 511);
                g_expTitleText = tbuf;
            } else if (id == 9202 && g_expEditingNotes && g_hwndExpNotesEdit) {
                wchar_t nbuf[2048] = {};
                GetWindowTextW(g_hwndExpNotesEdit, nbuf, 2047);
                g_expNotesText = nbuf;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (code == EN_SETFOCUS) {
            InvalidateRect(hwnd, nullptr, FALSE);
        } else if (code == EN_KILLFOCUS) {
            if (id == 9201) {
                if (g_hwndExpTitleEdit) {
                    wchar_t tbuf[512] = {};
                    GetWindowTextW(g_hwndExpTitleEdit, tbuf, 511);
                    g_expTitleText = tbuf;
                    ShowWindow(g_hwndExpTitleEdit, SW_HIDE);
                }
                g_expEditingTitle = false;
            } else if (id == 9202) {
                if (g_hwndExpNotesEdit) {
                    wchar_t nbuf[2048] = {};
                    GetWindowTextW(g_hwndExpNotesEdit, nbuf, 2047);
                    g_expNotesText = nbuf;
                    ShowWindow(g_hwndExpNotesEdit, SW_HIDE);
                }
                g_expEditingNotes = false;
            } else if (id == 9203) {
                CommitInlineSheetNote(hwnd, false);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE: {
        const float s = GetExporterUiScale(hwnd);
        g_expHoverPt = {
            CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s),
            CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s)
        };
        if (g_expDrawingActive && g_expAnnotTool == 1 && !g_expFreehandStrokes.empty()) {
            const int destW = (std::max)(1, static_cast<int>(g_expPreviewDestRect.right - g_expPreviewDestRect.left));
            const int destH = (std::max)(1, static_cast<int>(g_expPreviewDestRect.bottom - g_expPreviewDestRect.top));
            const float cx = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(g_expHoverPt.x - g_expPreviewDestRect.left) / destW));
            const float cy = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(g_expHoverPt.y - g_expPreviewDestRect.top) / destH));
            auto& pts = g_expFreehandStrokes.back().pts;
            if (pts.empty() ||
                std::abs((cx - pts.back().x) * destW) >= 2.0f ||
                std::abs((cy - pts.back().y) * destH) >= 2.0f) {
                pts.push_back(GuidePointF{ cx, cy });
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if (!g_expTrackingHover) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            g_expTrackingHover = true;
        }
        RECT rc{};
        GetClientRect(hwnd, &rc);
        ExporterLayout L = ComputeExporterLayout(
            CasioTheme::DeviceToLogical(rc.right, s),
            CasioTheme::DeviceToLogical(rc.bottom, s));
        bool over =
            PtInRect(&L.rcTitleChrome, g_expHoverPt) || PtInRect(&L.rcNotesChrome, g_expHoverPt) ||
            PtInRect(&L.rcRangeStartMinus, g_expHoverPt) || PtInRect(&L.rcRangeStartPlus, g_expHoverPt) ||
            PtInRect(&L.rcRangeEndMinus, g_expHoverPt)   || PtInRect(&L.rcRangeEndPlus, g_expHoverPt)   ||
            PtInRect(&L.rcBtnSyncHist, g_expHoverPt)     || PtInRect(&L.rcBtnPresetCycle, g_expHoverPt) ||
            PtInRect(&L.rcBtnEquivModel, g_expHoverPt)   || PtInRect(&L.rcBtnUndoAnnot, g_expHoverPt)   ||
            PtInRect(&L.rcBtnClearAnnot, g_expHoverPt)   ||
            PtInRect(&L.rcBtnCopyClip, g_expHoverPt)     || PtInRect(&L.rcBtnSavePng, g_expHoverPt)     ||
            PtInRect(&L.rcBtnSaveAs, g_expHoverPt)       || PtInRect(&L.rcBtnClose, g_expHoverPt);
        for (int i = 0; i < 2 && !over; ++i) if (PtInRect(&L.rcThemeSeg[i], g_expHoverPt)) over = true;
        for (int i = 0; i < 4 && !over; ++i) {
            if (PtInRect(&L.rcFmtSeg[i], g_expHoverPt) || PtInRect(&L.rcInkSwatch[i], g_expHoverPt)) over = true;
        }
        for (int i = 0; i < 5 && !over; ++i) if (PtInRect(&L.rcToggleChips[i], g_expHoverPt)) over = true;
        for (int i = 0; i < 7 && !over; ++i) if (PtInRect(&L.rcAnnotSeg[i], g_expHoverPt)) over = true;
        if (!over && PtInRect(&g_expPreviewDestRect, g_expHoverPt)) over = true;
        SetCursor(LoadCursor(nullptr, (g_expAnnotTool != 0 && PtInRect(&g_expPreviewDestRect, g_expHoverPt))
            ? (g_expAnnotTool == 2 ? IDC_IBEAM : IDC_CROSS)
            : (over ? IDC_HAND : IDC_ARROW)));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
        if (g_expDrawingActive) {
            g_expDrawingActive = false;
            if (GetCapture() == hwnd) ReleaseCapture();
            g_expStatusMsg = L"Trazo a mano alzada agregado sobre la gu\u00EDa.";
            InvalidateRect(hwnd, nullptr, FALSE);
            if (msg == WM_LBUTTONUP) return 0;
        }
        break;

    case WM_MOUSELEAVE:
        g_expTrackingHover = false;
        g_expHoverPt = { -1, -1 };
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        const float s = GetExporterUiScale(hwnd);
        POINT pt = {
            CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s),
            CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s)
        };
        int mx = pt.x, my = pt.y;
        RECT rc{};
        GetClientRect(hwnd, &rc);
        ExporterLayout L = ComputeExporterLayout(
            CasioTheme::DeviceToLogical(rc.right, s),
            CasioTheme::DeviceToLogical(rc.bottom, s));
        int nTotal = (std::max)(1, (int)g_exporterSteps.size());

        auto commitActiveEdits = [&]() {
            bool committed = false;
            if (g_expEditingTitle) {
                if (g_hwndExpTitleEdit) {
                    wchar_t tbuf[512] = {};
                    GetWindowTextW(g_hwndExpTitleEdit, tbuf, 511);
                    g_expTitleText = tbuf;
                    ShowWindow(g_hwndExpTitleEdit, SW_HIDE);
                }
                g_expEditingTitle = false;
                committed = true;
            }
            if (g_expEditingNotes) {
                if (g_hwndExpNotesEdit) {
                    wchar_t nbuf[2048] = {};
                    GetWindowTextW(g_hwndExpNotesEdit, nbuf, 2047);
                    g_expNotesText = nbuf;
                    ShowWindow(g_hwndExpNotesEdit, SW_HIDE);
                }
                g_expEditingNotes = false;
                committed = true;
            }
            return committed;
        };

        if (PtInRect(&L.rcTitleChrome, pt)) {
            commitActiveEdits();
            g_expEditingTitle = true;
            if (g_hwndExpTitleEdit) {
                SetWindowTextW(g_hwndExpTitleEdit, g_expTitleText.c_str());
                SyncExporterChildEdits(hwnd);
                ShowWindow(g_hwndExpTitleEdit, SW_SHOW);
                SetFocus(g_hwndExpTitleEdit);
                SendMessageW(g_hwndExpTitleEdit, EM_SETSEL, 0, -1);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcNotesChrome, pt)) {
            commitActiveEdits();
            g_expEditingNotes = true;
            if (g_hwndExpNotesEdit) {
                SetWindowTextW(g_hwndExpNotesEdit, g_expNotesText.c_str());
                SyncExporterChildEdits(hwnd);
                ShowWindow(g_hwndExpNotesEdit, SW_SHOW);
                SetFocus(g_hwndExpNotesEdit);
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (commitActiveEdits()) {
            SetFocus(hwnd);
        }

        // Range & top actions
        if (PtInRect(&L.rcRangeStartMinus, pt)) {
            g_expRangeStart = (std::max)(1, g_expRangeStart - 1);
            ApplyExporterRangeToSelection();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcRangeStartPlus, pt)) {
            g_expRangeStart = (std::min)(g_expRangeEnd, g_expRangeStart + 1);
            ApplyExporterRangeToSelection();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcRangeEndMinus, pt)) {
            g_expRangeEnd = (std::max)(g_expRangeStart, g_expRangeEnd - 1);
            ApplyExporterRangeToSelection();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcRangeEndPlus, pt)) {
            g_expRangeEnd = (std::min)(nTotal, g_expRangeEnd + 1);
            ApplyExporterRangeToSelection();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcBtnSyncHist, pt)) {
            SyncExporterStepsFromHistory(-1);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcBtnPresetCycle, pt)) {
            static int s_presetCycle = 0;
            s_presetCycle = (s_presetCycle + 1) % 4;
            LoadPresetPedagogicalGuide(s_presetCycle);
            return 0;
        }
        if (PtInRect(&L.rcBtnEquivModel, pt)) {
            TrySwitchExporterToEquivalentModel();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        for (int i = 0; i < 2; ++i) {
            if (PtInRect(&L.rcThemeSeg[i], pt)) {
                g_exportSheetLightTheme = (i == 0);
                SaveUnifiedSettingsToIni();
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Format segmented presets (Explicada / Rapida / Practica / Presentacion)
        for (int i = 0; i < 4; ++i) {
            if (PtInRect(&L.rcFmtSeg[i], pt)) {
                ApplyGuideFormatPreset(i);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Granular element chips
        bool* chipFlags[5] = {
            &g_expIncludeNotes,
            &g_expIncludeStepNumbers,
            &g_expIncludeLatexFormula,
            &g_expIncludeLcdResult,
            &g_expHidePracticeAnswer
        };
        for (int i = 0; i < 5; ++i) {
            if (PtInRect(&L.rcToggleChips[i], pt)) {
                *(chipFlags[i]) = !(*(chipFlags[i]));
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        // Interactive live preview (freehand draw, inline notes, full-sheet callouts or toggle steps)
        if (PtInRect(&g_expPreviewDestRect, pt) && g_expLastSheetW > 0 && g_expLastSheetH > 0) {
            CommitInlineSheetNote(hwnd, false);
            int destW = (std::max)(1, static_cast<int>(g_expPreviewDestRect.right - g_expPreviewDestRect.left));
            int destH = (std::max)(1, static_cast<int>(g_expPreviewDestRect.bottom - g_expPreviewDestRect.top));
            float normPrevX = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(mx - g_expPreviewDestRect.left) / destW));
            float normPrevY = (std::max)(0.0f, (std::min)(1.0f, static_cast<float>(my - g_expPreviewDestRect.top) / destH));
            int sx = (mx - g_expPreviewDestRect.left) * g_expLastSheetW / destW;
            int sy = (my - g_expPreviewDestRect.top) * g_expLastSheetH / destH;

            if (g_expAnnotTool == 1) {
                GuideFreehandStroke st;
                st.color = GetExpAnnotInkColor();
                st.strokePx = 3.2f;
                st.pts.push_back(GuidePointF{ normPrevX, normPrevY });
                g_expFreehandStrokes.push_back(std::move(st));
                g_expUndoStack.push_back(ExpUndoKind::Freehand);
                g_expDrawingActive = true;
                SetCapture(hwnd);
                g_expStatusMsg = L"Dibujando a mano alzada...";
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_expAnnotTool == 2 && g_hwndExpInlineNoteEdit) {
                g_expInlineNoteNorm = GuidePointF{ normPrevX, normPrevY };
                g_expInlineNoteActive = true;
                const int boxW = 216;
                const int boxH = 26;
                int ex = (std::min)(static_cast<int>(g_expPreviewDestRect.right - boxW - 4), (std::max)(static_cast<int>(g_expPreviewDestRect.left + 4), mx - 8));
                int ey = (std::min)(static_cast<int>(g_expPreviewDestRect.bottom - boxH - 4), (std::max)(static_cast<int>(g_expPreviewDestRect.top + 4), my - boxH / 2));
                MoveWindow(g_hwndExpInlineNoteEdit,
                           CasioTheme::UiToDevice(ex, s),
                           CasioTheme::UiToDevice(ey, s),
                           CasioTheme::UiToDevice(boxW, s),
                           CasioTheme::UiToDevice(boxH, s),
                           TRUE);
                SetWindowTextW(g_hwndExpInlineNoteEdit, L"");
                ShowWindow(g_hwndExpInlineNoteEdit, SW_SHOW);
                SetFocus(g_hwndExpInlineNoteEdit);
                g_expStatusMsg = L"Escriba una nota o f\u00F3rmula ($...$) y pulse Enter.";
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (g_expAnnotTool >= 3 && g_expAnnotTool <= 6) {
                GuideLcdAnnotation ann;
                ann.kind = g_expAnnotTool - 2; // 1=Flecha, 2=Circulo, 3=Marco, 4=#N
                ann.color = GetExpAnnotInkColor();
                ann.tagNum = static_cast<int>(g_expAnnotations.size()) + 1;
                if (PtInRect(&g_expLastLcdDrawRect, POINT{ sx, sy })) {
                    int lw = (std::max)(1, static_cast<int>(g_expLastLcdDrawRect.right - g_expLastLcdDrawRect.left));
                    int lh = (std::max)(1, static_cast<int>(g_expLastLcdDrawRect.bottom - g_expLastLcdDrawRect.top));
                    ann.normX = static_cast<float>(sx - g_expLastLcdDrawRect.left) / lw;
                    ann.normY = static_cast<float>(sy - g_expLastLcdDrawRect.top) / lh;
                    ann.fullSheet = false;
                } else {
                    ann.normX = normPrevX;
                    ann.normY = normPrevY;
                    ann.fullSheet = true;
                }
                g_expAnnotations.push_back(ann);
                g_expUndoStack.push_back(ExpUndoKind::Shape);
                g_expStatusMsg = L"Indicaci\u00F3n visual #" + std::to_wstring(ann.tagNum) + L" agregada.";
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            for (size_t i = 0; i < g_expLastCardRects.size() && i < g_expLastCardIndices.size(); ++i) {
                const RECT& r = g_expLastCardRects[i];
                if (sx >= r.left && sx <= r.right && sy >= r.top && sy <= r.bottom) {
                    int idx = g_expLastCardIndices[i];
                    if (idx >= 0 && idx < (int)g_exporterSteps.size()) {
                        g_exporterSteps[idx].selected = !g_exporterSteps[idx].selected;
                        g_expStatusMsg = std::wstring(L"Paso #") + std::to_wstring(idx + 1) +
                                         (g_exporterSteps[idx].selected ? L" incluido en la gu\u00EDa." : L" omitido de la gu\u00EDa.");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                    return 0;
                }
            }
        }

        CommitInlineSheetNote(hwnd, false);

        // Bottom Dock: 7-tool Annotator selection + ink swatches + Undo + Clear
        for (int i = 0; i < 7; ++i) {
            if (PtInRect(&L.rcAnnotSeg[i], pt)) {
                g_expAnnotTool = i;
                const wchar_t* toolHint[7] = {
                    L"Modo seleccionar pasos (clic en tarjetas para incluir/omitir).",
                    L"L\u00E1piz (mano alzada): arrastre sobre la vista previa para dibujar.",
                    L"Texto / Nota: haga clic donde desee agregar una indicaci\u00F3n o f\u00F3rmula ($...$).",
                    L"Flecha: haga clic sobre cualquier parte de la hoja para se\u00F1alar.",
                    L"C\u00EDrculo: haga clic sobre la hoja para rodear un elemento.",
                    L"Marco: haga clic sobre la hoja para encuadrar un paso o zona.",
                    L"Etiqueta #N: haga clic para numerar indicaciones sobre la hoja."
                };
                g_expStatusMsg = toolHint[i];
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        for (int i = 0; i < 4; ++i) {
            if (PtInRect(&L.rcInkSwatch[i], pt)) {
                g_expAnnotColorIdx = i;
                static const wchar_t* kInkNames[4] = { L"Rojo", L"Azul", L"\u00C1mbar", L"Verde" };
                g_expStatusMsg = std::wstring(L"Color de trazo y notas: ") + kInkNames[i] + L".";
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        if (PtInRect(&L.rcBtnUndoAnnot, pt)) {
            UndoLastSheetAnnotation();
            const bool emptyAll = g_expAnnotations.empty() && g_expFreehandStrokes.empty() && g_expSheetNotes.empty();
            g_expStatusMsg = emptyAll
                ? L"No quedan trazos ni notas sobre la hoja."
                : L"Se deshizo la \u00FAltima anotaci\u00F3n.";
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&L.rcBtnClearAnnot, pt)) {
            ClearAllSheetAnnotations();
            g_expStatusMsg = L"Se limpiaron todos los dibujos y notas de la hoja.";
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        // Bottom Dock: Primary export actions
        if (PtInRect(&L.rcBtnCopyClip, pt)) {
            ExportPedagogicalSheetToFileAndClipboard(true, false);
            return 0;
        }
        if (PtInRect(&L.rcBtnSavePng, pt)) {
            ExportPedagogicalSheetToFileAndClipboard(true, true);
            return 0;
        }
        if (PtInRect(&L.rcBtnSaveAs, pt)) {
            SavePedagogicalSheetWithSaveAsDialog(hwnd);
            return 0;
        }
        if (PtInRect(&L.rcBtnClose, pt)) {
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rcPhys{};
        GetClientRect(hwnd, &rcPhys);
        if (rcPhys.right <= 0 || rcPhys.bottom <= 0) {
            EndPaint(hwnd, &ps);
            return 0;
        }

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, rcPhys.right, rcPhys.bottom);
        HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBmp);

        HFONT hSecF   = CasioTheme::CreateUiFont(12, true);
        HFONT hMetaF  = CasioTheme::CreateUiFont(11, true);
        HFONT hSmallF = CasioTheme::CreateUiFont(11, false);
        HFONT hOldF   = (HFONT)SelectObject(memDC, hSecF);

        const float s = GetExporterUiScale(hwnd);
        {
            CasioTheme::UiScaleScope scaleScope(memDC, s);
            RECT rc = {
                0, 0,
                CasioTheme::DeviceToLogical(rcPhys.right, s),
                CasioTheme::DeviceToLogical(rcPhys.bottom, s)
            };

            HBRUSH hBg = CreateSolidBrush(CasioTheme::StudioCanvasBg());
            FillRect(memDC, &rc, hBg);
            DeleteObject(hBg);
            SetBkMode(memDC, TRANSPARENT);

        ExporterLayout L = ComputeExporterLayout(rc.right, rc.bottom);
        LatexValidationResult latexVal = ValidateLatexRichText(g_expTitleText + L"\n" + g_expNotesText);

        // Header stage material translúcido 2.0
        RECT rcHdr = { 0, 0, rc.right, 44 };
        CasioTheme::DrawTranslucentHeaderStrip(memDC, rcHdr, true);

        RECT rcHdrTxt = { 18, 4, rc.right - 220, 40 };
        DrawLatexRichText(
            memDC,
            L"Exportador de Gu\u00EDas y Secuencias",
            rcHdrTxt, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 15, true
        );

        RECT rcLatexPill = { rc.right - 208, 10, rc.right - 18, 34 };
        CasioTheme::DrawModernPill(
            memDC, rcLatexPill,
            latexVal.valid ? CasioTheme::StudioSuccessSoft() : CasioTheme::StudioRoseSoft(),
            latexVal.valid ? CasioTheme::StudioBorder()      : CasioTheme::StudioRose(),
            latexVal.valid ? CasioTheme::StudioSuccessText() : CasioTheme::StudioRoseText(),
            latexVal.valid ? L"F\u00F3rmula v\u00E1lida" : L"Sintaxis con error",
            hMetaF, 7.0f
        );

        // Top Form Surface Panel
        RECT rcFormPanel = { 18, 45, rc.right - 18, 180 };
        CasioTheme::FillRoundRectAA(memDC, rcFormPanel, 9.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorder(), 1.0f);

        DrawLatexRichText(memDC, L"T\u00EDtulo:", { 30, 52, 166, 74 },
                          CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, true);
        DrawLatexRichText(memDC, L"Notas:", { 30, 86, 166, 108 },
                          CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, true);
        DrawLatexRichText(memDC, (g_expEditingTitle || g_expEditingNotes) ? L"Editando texto..." : L"Clic para editar", { 30, 110, 166, 134 },
                          CasioTheme::StudioTextMuted(), CasioTheme::StudioAccentText(), 11, true);

        HWND focusWnd = GetFocus();
        CasioTheme::DrawInputChrome(memDC, L.rcTitleChrome, g_expEditingTitle || focusWnd == g_hwndExpTitleEdit);
        CasioTheme::DrawInputChrome(memDC, L.rcNotesChrome, g_expEditingNotes || focusWnd == g_hwndExpNotesEdit);
        if (!g_expEditingTitle) {
            int savedClip = SaveDC(memDC);
            IntersectClipRect(memDC, L.rcTitleChrome.left + 2, L.rcTitleChrome.top + 1,
                              L.rcTitleChrome.right - 2, L.rcTitleChrome.bottom - 1);
            RECT rcTitleDraw = { L.rcTitleEdit.left, L.rcTitleChrome.top + 1, L.rcTitleEdit.right, L.rcTitleChrome.bottom - 1 };
            DrawLatexRichText(memDC, g_expTitleText, rcTitleDraw,
                              CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);
            RestoreDC(memDC, savedClip);
        }
        if (!g_expEditingNotes) {
            int savedClip = SaveDC(memDC);
            IntersectClipRect(memDC, L.rcNotesChrome.left + 2, L.rcNotesChrome.top + 1,
                              L.rcNotesChrome.right - 2, L.rcNotesChrome.bottom - 1);
            RECT rcNotesDraw = { L.rcNotesEdit.left, L.rcNotesChrome.top + 3, L.rcNotesEdit.right, L.rcNotesChrome.bottom - 2 };
            DrawLatexRichText(memDC, g_expNotesText, rcNotesDraw,
                              CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 11, false);
            RestoreDC(memDC, savedClip);
        }

        // Row 1 controls: Range Steppers + Action Pills + Theme Segmented Switch
        SelectObject(memDC, hMetaF);
        SetTextColor(memDC, CasioTheme::StudioTextSec());
        RECT rcLblDesde = { 18, 188, 64, 220 };
        RECT rcLblHasta = { 170, 188, 216, 220 };
        DrawTextW(memDC, L"Desde:", -1, &rcLblDesde, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        DrawTextW(memDC, L"Hasta:", -1, &rcLblHasta, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        CasioTheme::DrawStudioButton(memDC, L.rcRangeStartMinus, L"-", hSecF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcRangeStartMinus, g_expHoverPt) != FALSE, 7.0f);
        wchar_t numBuf[32];
        swprintf(numBuf, 32, L"#%d", g_expRangeStart);
        CasioTheme::DrawModernPill(memDC, L.rcRangeStartVal, CasioTheme::StudioInputBg(), CasioTheme::StudioBorderStrong(),
                                   CasioTheme::StudioAccentText(), numBuf, hMetaF, 6.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcRangeStartPlus, L"+", hSecF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcRangeStartPlus, g_expHoverPt) != FALSE, 7.0f);

        CasioTheme::DrawStudioButton(memDC, L.rcRangeEndMinus, L"-", hSecF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcRangeEndMinus, g_expHoverPt) != FALSE, 7.0f);
        swprintf(numBuf, 32, L"#%d", g_expRangeEnd);
        CasioTheme::DrawModernPill(memDC, L.rcRangeEndVal, CasioTheme::StudioInputBg(), CasioTheme::StudioBorderStrong(),
                                   CasioTheme::StudioAccentText(), numBuf, hMetaF, 6.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcRangeEndPlus, L"+", hSecF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcRangeEndPlus, g_expHoverPt) != FALSE, 7.0f);

        CasioTheme::DrawStudioButton(memDC, L.rcBtnSyncHist, L"Usar Historial", hMetaF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcBtnSyncHist, g_expHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcBtnPresetCycle, L"Ejemplos", hMetaF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcBtnPresetCycle, g_expHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcBtnEquivModel, L"Equiv. Modelo", hMetaF, CasioTheme::BTN_VIOLET,
                                     false, PtInRect(&L.rcBtnEquivModel, g_expHoverPt) != FALSE, 7.0f);

        // Floating material translúcido 2.0 Segmented Theme Control (Claro | Oscuro)
        CasioTheme::DrawFloatingGlassCapsule(memDC, L.rcThemeSegTrack, s, -1.0f, false);
        const wchar_t* themeLabels[2] = { L"Claro", L"Oscuro" };
        for (int i = 0; i < 2; ++i) {
            bool act = (g_exportSheetLightTheme == (i == 0));
            bool hov = PtInRect(&L.rcThemeSeg[i], g_expHoverPt) != FALSE;
            CasioTheme::DrawStudioSegmentItem(memDC, L.rcThemeSeg[i], themeLabels[i], act ? hMetaF : hSmallF, act, hov, act);
        }

        // Row 2: Floating material translúcido 2.0 4-Segment Format Bar + 5 Granular Toggle Chips
        CasioTheme::DrawFloatingGlassCapsule(memDC, L.rcFmtSegTrack, s, -1.0f, false);
        const wchar_t* fmtLabels[4] = {
            L"Explicada",
            L"R\u00E1pida",
            L"Pr\u00E1ctica",
            L"Presentaci\u00F3n"
        };
        for (int i = 0; i < 4; ++i) {
            bool act = (g_expFormatPreset == i);
            bool hov = PtInRect(&L.rcFmtSeg[i], g_expHoverPt) != FALSE;
            CasioTheme::DrawStudioSegmentItem(memDC, L.rcFmtSeg[i], fmtLabels[i], act ? hMetaF : hSmallF, act, hov, act);
        }

        struct ToggleChipInfo { bool val; const wchar_t* label; COLORREF accent; COLORREF softBg; } chips[5] = {
            { g_expIncludeNotes,        L"Notas",        CasioTheme::StudioAccent(),  CasioTheme::StudioAccentSoft() },
            { g_expIncludeStepNumbers,  L"Pasos #",      CasioTheme::StudioAccent(),  CasioTheme::StudioAccentSoft() },
            { g_expIncludeLatexFormula, L"F\u00F3rmula",      CasioTheme::StudioAccent(),  CasioTheme::StudioAccentSoft() },
            { g_expIncludeLcdResult,    L"Pantalla LCD", CasioTheme::StudioSuccess(), CasioTheme::StudioSuccessSoft() },
            { g_expHidePracticeAnswer,  L"Oculta Resp",  CasioTheme::StudioAmber(),   CasioTheme::StudioAmberSoft() }
        };
        for (int i = 0; i < 5; ++i) {
            bool on = chips[i].val;
            bool hov = PtInRect(&L.rcToggleChips[i], g_expHoverPt) != FALSE;
            COLORREF fill = on ? chips[i].softBg : (hov ? CasioTheme::StudioSurfaceAlt() : CasioTheme::StudioSurface());
            COLORREF bd   = on ? chips[i].accent : (hov ? CasioTheme::StudioBorderStrong() : CasioTheme::StudioBorder());
            CasioTheme::FillRoundRectAA(memDC, L.rcToggleChips[i], 7.0f, fill, bd, 1.0f);

            int dotY = (L.rcToggleChips[i].top + L.rcToggleChips[i].bottom) / 2;
            CasioTheme::DrawDotIndicator(memDC, L.rcToggleChips[i].left + 11, dotY, 4,
                                         on ? chips[i].accent : CasioTheme::StudioTextMuted());

            RECT rcCLbl = { L.rcToggleChips[i].left + 18, L.rcToggleChips[i].top,
                            L.rcToggleChips[i].right - 4, L.rcToggleChips[i].bottom };
            SelectObject(memDC, hMetaF);
            SetTextColor(memDC, on ? CasioTheme::StudioText() : CasioTheme::StudioTextMuted());
            DrawTextW(memDC, chips[i].label, -1, &rcCLbl, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        // Caption above preview
        SelectObject(memDC, hSmallF);
        SetTextColor(memDC, CasioTheme::StudioTextMuted());
        RECT rcPrevCap = { 18, 262, rc.right - 18, 280 };
        DrawTextW(memDC,
                  L"Vista previa interactiva (clic en paso para omitir, o use L\u00E1piz mano alzada, Texto/Nota y formas sobre toda la hoja):",
                  -1, &rcPrevCap, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Live Preview Stage Card
        CasioTheme::FillRoundRectAA(memDC, L.rcPreviewStage, 10.0f, CasioTheme::StudioStageBg(), CasioTheme::StudioBorder(), 1.0f);

        int shW = 0, shH = 0;
        HBITMAP hSheet = RenderPedagogicalSheetBitmap(shW, shH, &g_expLastCardRects, &g_expLastCardIndices);
        g_expLastSheetW = shW;
        g_expLastSheetH = shH;

        if (hSheet && shW > 0 && shH > 0) {
            HDC hdcSheet = CreateCompatibleDC(memDC);
            HBITMAP hOldSh = (HBITMAP)SelectObject(hdcSheet, hSheet);

            int maxPrevW = (std::max)(200, (int)(L.rcPreviewInner.right - L.rcPreviewInner.left));
            int maxPrevH = (std::max)(160, (int)(L.rcPreviewInner.bottom - L.rcPreviewInner.top));
            double scale = (std::min)((double)maxPrevW / shW, (double)maxPrevH / shH);
            int dw = (int)(shW * scale);
            int dh = (int)(shH * scale);
            int dx = L.rcPreviewInner.left + (maxPrevW - dw) / 2;
            int dy = L.rcPreviewInner.top + (maxPrevH - dh) / 2;
            g_expPreviewDestRect = { dx, dy, dx + dw, dy + dh };

            SetStretchBltMode(memDC, HALFTONE);
            StretchBlt(memDC, dx, dy, dw, dh, hdcSheet, 0, 0, shW, shH, SRCCOPY);

            SelectObject(hdcSheet, hOldSh);
            DeleteDC(hdcSheet);
            DeleteObject(hSheet);
        }

        // Bottom Dock Card + Floating material translúcido 2.0 Annotator Bar
        CasioTheme::FillRoundRectAA(memDC, L.rcDockCard, 12.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorder(), 1.0f);

        CasioTheme::DrawFloatingGlassCapsule(memDC, L.rcAnnotSegTrack, s, -1.0f, false);
        const wchar_t* annotShort[7] = { L"Off", L"L\u00E1piz", L"Texto", L"Flecha", L"C\u00EDrc.", L"Marco", L"#N" };
        for (int i = 0; i < 7; ++i) {
            bool act = (g_expAnnotTool == i);
            bool hov = PtInRect(&L.rcAnnotSeg[i], g_expHoverPt) != FALSE;
            if (i == 0 || !act) {
                CasioTheme::DrawStudioSegmentItem(memDC, L.rcAnnotSeg[i], annotShort[i], act ? hMetaF : hSmallF, act, hov, false);
            } else {
                const COLORREF ink = GetExpAnnotInkColor();
                const COLORREF pillFill = CasioTheme::g_studioDarkTheme ? RGB(44, 34, 40) : RGB(248, 240, 242);
                CasioTheme::FillRoundRectAA(memDC, L.rcAnnotSeg[i], 12.0f, pillFill, ink, 1.2f);
                SelectObject(memDC, hMetaF);
                SetTextColor(memDC, CasioTheme::g_studioDarkTheme ? RGB(245, 248, 255) : ink);
                DrawTextW(memDC, annotShort[i], -1, &L.rcAnnotSeg[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // 4 Aqua 2.0 Vitreous Ink Orbs
        {
            static constexpr COLORREF kInkDots[4] = {
                RGB(225, 45, 57), RGB(0, 112, 243), RGB(217, 119, 6), RGB(22, 163, 74)
            };
            Gdiplus::Graphics gSw(memDC);
            for (int i = 0; i < 4; ++i) {
                const RECT& rs = L.rcInkSwatch[i];
                const float cx = (rs.left + rs.right) * 0.5f;
                const float cy = (rs.top + rs.bottom) * 0.5f;
                const bool sel = (g_expAnnotColorIdx == i);
                UiGlass::DrawAquaVitreousOrb(gSw, cx, cy, sel ? 6.5f : 5.5f,
                    CasioTheme::ToGpColor(kInkDots[i], 255), sel, CasioTheme::g_studioDarkTheme);
            }
        }

        CasioTheme::DrawStudioButton(memDC, L.rcBtnUndoAnnot, L"Deshacer", hMetaF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcBtnUndoAnnot, g_expHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcBtnClearAnnot, L"Limpiar", hMetaF, CasioTheme::BTN_GHOST,
                                     false, PtInRect(&L.rcBtnClearAnnot, g_expHoverPt) != FALSE, 7.0f);

        CasioTheme::DrawStudioButton(memDC, L.rcBtnCopyClip, L"Copiar", hSecF, CasioTheme::BTN_SECONDARY,
                                     false, PtInRect(&L.rcBtnCopyClip, g_expHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcBtnSavePng, L"Guardar PNG", hSecF, CasioTheme::BTN_SUCCESS,
                                     false, PtInRect(&L.rcBtnSavePng, g_expHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcBtnSaveAs, L"Guardar Como...", hSecF, CasioTheme::BTN_PRIMARY,
                                     false, PtInRect(&L.rcBtnSaveAs, g_expHoverPt) != FALSE, 7.0f);
        CasioTheme::DrawStudioButton(memDC, L.rcBtnClose, L"Cerrar", hSecF, CasioTheme::BTN_GHOST,
                                     false, PtInRect(&L.rcBtnClose, g_expHoverPt) != FALSE, 7.0f);

        // Status bar at very bottom
        RECT rcStatBar = { 18, L.rcDockCard.bottom + 4, rc.right - 18, rc.bottom - 4 };
        SelectObject(memDC, hMetaF);
        if (!latexVal.valid) {
            SetTextColor(memDC, CasioTheme::StudioRoseText());
            std::wstring err = L"Sintaxis matem\u00E1tica: " + latexVal.diagnostic;
            DrawTextW(memDC, err.c_str(), -1, &rcStatBar, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        } else {
            SetTextColor(memDC, CasioTheme::StudioSuccessText());
            DrawTextW(memDC, g_expStatusMsg.c_str(), -1, &rcStatBar, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        SelectObject(memDC, hOldF);
        } // UiScaleScope
        DeleteObject(hSecF);
        DeleteObject(hMetaF);
        DeleteObject(hSmallF);

        BitBlt(hdc, 0, 0, rcPhys.right, rcPhys.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, hOld);
        DeleteObject(hBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_CLOSE:
        CommitInlineSheetNote(hwnd, false);
        g_expDrawingActive = false;
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

static void OpenSequenceExporterWindow(bool syncHistoryFirst) {
    static float s_lastExporterScale = 0.0f;
    if (syncHistoryFirst && g_exporterSteps.empty()) {
        SyncExporterStepsFromHistory(-1);
    }
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    const float dlgScale = GetExporterUiScale(g_hwndExporter);
    if (!g_hwndExporter) {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SequenceExporterWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CasioTheme::GetStudioSurfaceBrush();
        wc.lpszClassName = L"CasioSequenceExporterWndClass";
        RegisterClassExW(&wc);

        RECT wr = { 0, 0, CasioTheme::UiToDevice(1020, dlgScale), CasioTheme::UiToDevice(792, dlgScale) };
        AdjustWindowRectEx(&wr, style, FALSE, WS_EX_APPWINDOW);
        g_hwndExporter = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"CasioSequenceExporterWndClass",
            L"Exportador de Gu\u00EDas y Secuencias",
            style,
            CW_USEDEFAULT, CW_USEDEFAULT, wr.right - wr.left, wr.bottom - wr.top,
            g_hMainWnd, nullptr, GetModuleHandleW(nullptr), nullptr
        );
        s_lastExporterScale = dlgScale;
    } else {
        const float activeScale = GetExporterUiScale(g_hwndExporter);
        if (s_lastExporterScale - activeScale > 0.01f || activeScale - s_lastExporterScale > 0.01f) {
            RECT wr = { 0, 0, CasioTheme::UiToDevice(1020, activeScale), CasioTheme::UiToDevice(792, activeScale) };
            AdjustWindowRectEx(&wr, style, FALSE, WS_EX_APPWINDOW);
            SetWindowPos(g_hwndExporter, nullptr, 0, 0, wr.right - wr.left, wr.bottom - wr.top,
                         SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            s_lastExporterScale = activeScale;
        }
        g_expEditingTitle = false;
        g_expEditingNotes = false;
        SyncExporterChildEdits(g_hwndExporter);
    }
    CasioTheme::ApplyDwmDarkTitleBar(g_hwndExporter, CasioTheme::g_studioDarkTheme);
    if (g_hwndExpNotesEdit) CasioTheme::EnableThemeScrollbar(g_hwndExpNotesEdit, CasioTheme::g_studioDarkTheme);
    ShowWindow(g_hwndExporter, SW_SHOW);
    SetForegroundWindow(g_hwndExporter);
    InvalidateRect(g_hwndExporter, nullptr, FALSE);
}
