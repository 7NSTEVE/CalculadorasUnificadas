#pragma once
// ============================================================================
// learning_studio.h
// Componente 2: Superposicion de Capas SHIFT/ALPHA, Tarjeta Flotante de
// Explorador de Teclas/Indicadores LCD, Menu Contextual Transversal y
// Centro de Estudio Unificado de 6 Pestanas (100% offline y determinista).
// ============================================================================

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

#include "knowledge_catalog.h"
#include "latex_math_renderer.h"
#include "manual_catalog.h"
#include "ui_theme.h"

// Estado global de Componente 2 (definido en main.cpp antes de settings_and_exporter.h)
// g_shiftAlphaLayerMode y g_keyExplorerEnabled se comparten con INI y barra principal.

// Tarjeta flotante de Explorador de Teclas / Indicadores LCD sobre CanvasWnd
inline int  g_explorerKind        = 0;     // 0 = Oculto, 1 = Tecla, 2 = Indicador LCD
inline int  g_explorerCalcIdx     = 0;
inline int  g_explorerItemId      = 0;     // keyId (0..49) o spriteIndex
inline bool g_explorerIndLit      = false;
inline int  g_explorerLayer       = 0;     // 0 = Normal, 1 = SHIFT, 2 = ALPHA
// g_explorerPinned se declara en main.cpp antes de CanvasWndProc.

struct ExplorerCardRects {
    RECT card{};
    RECT closeBtn{};
    RECT pinBtn{};
    RECT layerTab[3]{};
    RECT btnTry{};
    RECT btnPdf{};
    RECT btnGuide{};
    RECT btnFav{};
    RECT btnStudio{};
    bool visible = false;
};
inline ExplorerCardRects g_explorerRects;

// Favoritos locales y checklist de comprobacion previa
inline std::vector<CasioKnowledge::FavoriteItem> g_studioFavorites;
inline bool g_studioFavoritesLoaded = false;
inline bool g_studioChecklistDone[8] = { false, false, false, false, false, false, false, false };

inline void EnsureFavoritesAndChecklistLoaded() {
    if (!g_studioFavoritesLoaded) {
        g_studioFavorites = CasioKnowledge::LoadFavoritesFromDisk();
        g_studioFavoritesLoaded = true;
    }
}

inline int FindFavoriteIndex(const std::wstring& kind, const std::wstring& id) {
    EnsureFavoritesAndChecklistLoaded();
    for (size_t i = 0; i < g_studioFavorites.size(); ++i) {
        if (g_studioFavorites[i].kind == kind && g_studioFavorites[i].id == id) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

inline bool ToggleStudioFavorite(const CasioKnowledge::FavoriteItem& item) {
    EnsureFavoritesAndChecklistLoaded();
    int existing = FindFavoriteIndex(item.kind, item.id);
    bool added = false;
    if (existing >= 0) {
        g_studioFavorites.erase(g_studioFavorites.begin() + existing);
    } else {
        g_studioFavorites.push_back(item);
        added = true;
    }
    CasioKnowledge::SaveFavoritesToDisk(g_studioFavorites);
    return added;
}

// ============================================================================
// Contexto pendiente para menu clic derecho transversal
// ============================================================================
struct PendingContextMenuTarget {
    int calcIdx = 0;
    int keyId = -1;
    int indSpriteIdx = -1;
    bool indLit = false;
    bool isLcdBody = false;
    int timelineStepIdx = -1; // >= 0 cuando viene del Historial / Reproductor
};
inline PendingContextMenuTarget g_ctxTarget;

// ============================================================================
// Etiquetas compactas para superposicion SHIFT / ALPHA sobre el teclado
// ============================================================================
namespace StudioDetail {

inline std::wstring CompactLayerTag(const std::wstring& label, int layer) {
    if (label.empty()) return L"";
    if (layer == 2) {
        if (label.rfind(L"Variable ", 0) == 0 && label.size() >= 10) {
            return label.substr(9, 4);
        }
        if (label.rfind(L"Letra ", 0) == 0 && label.size() >= 7) {
            return label.substr(6, 3);
        }
        if (label.size() == 1) return label;
    }
    struct MapPair { const wchar_t* needle; const wchar_t* tag; };
    static const MapPair kShort[] = {
        { L"SOLVE", L"SOLVE" }, { L"SETUP", L"SETUP" }, { L"RESET", L"RESET" },
        { L"STAT", L"STAT" },   { L"CMPLX", L"CMPLX" }, { L"MATRIX", L"MAT" },
        { L"VECTOR", L"VCT" },  { L"RECALL", L"RCL" },  { L"STO", L"STO" },
        { L"CATALOG", L"CAT" }, { L"CONST", L"CONST" }, { L"CONV", L"CONV" },
        { L"sin^-1", L"sin-1" },{ L"cos^-1", L"cos-1" },{ L"tan^-1", L"tan-1" },
        { L"10^x", L"10^x" },   { L"e^x", L"e^x" },     { L"d/dx", L"d/dx" },
        { L"Ra\u00EDz C\u00FAbica", L"3\u221Ax" }, { L"C\u00FAbica", L"3\u221Ax" },
        { L"Mixta", L"a b/c" }, { L"Porcentaje", L"%" }, { L"Factorial", L"x!" },
        { L"Absoluto", L"Abs" },{ L"Suma", L"\u03A3" }, { L"Pi", L"\u03C0" },
        { L"Polar", L"Pol" },   { L"Rect", L"Rec" },    { L"Permut", L"nPr" },
        { L"Combin", L"nCr" },  { L"INS", L"INS" },     { L"OFF", L"OFF" },
        { L"QR", L"QR" },       { L"Ans", L"Ans" },     { L"Rnd", L"Rnd" },
        { L"Ran#", L"Ran#" },   { L"ENG", L"\u2190" },  { L"M-", L"M-" },
        { L"CALC", L"CALC" },   { L"LIST", L"List" },   { L"Mat", L"Mat" },
        { L"FORMAT", L"FMT" },  { L"CLIP", L"CLIP" },   { L"PASTE", L"PASTE" },
        { L"CAPTURE", L"CAP" }, { L"SPACE", L"SPC" }
    };
    for (const auto& mp : kShort) {
        if (label.find(mp.needle) != std::wstring::npos) return mp.tag;
    }
    if (label.size() <= 7) return label;
    size_t sp = label.find(L' ');
    if (sp != std::wstring::npos && sp > 0 && sp <= 7) {
        return label.substr(0, sp);
    }
    return label.substr(0, 6);
}

inline void DrawBadgePill(HDC hdc, RECT r, COLORREF fill, COLORREF border, COLORREF textCol, const std::wstring& txt, HFONT font) {
    CasioTheme::DrawModernPill(hdc, r, fill, border, textCol, txt, font, 6.0f);
}

inline void AppendKeyToExporter(int calcIdx, int keyId, const std::wstring& titleHint, const std::wstring& notesHint) {
    if (g_exporterSteps.empty() || g_expActiveCalc != calcIdx) {
        LoadCustomSequenceIntoExporter(calcIdx, { keyId }, titleHint, notesHint);
    } else {
        ExporterStepItem it;
        it.calcIdx = calcIdx;
        it.keyId = keyId;
        it.selected = true;
        g_exporterSteps.push_back(it);
        g_expRangeStart = 1;
        g_expRangeEnd = static_cast<int>(g_exporterSteps.size());
    }
    OpenSequenceExporterWindow(false);
}

} // namespace StudioDetail

// Implementacion del resaltado activo desde Reproductor / Explorer en el cuerpo de la calculadora
inline void DrawKeyLayersOverlay(HDC hdc, int calcIdx, const RECT& dstRect) {
    if (calcIdx < 0 || calcIdx > 2) return;
    const auto& calc = g_nativeCalcs[calcIdx];
    if (calc.skinW <= 0 || calc.skinH <= 0) return;

    const int dstW = dstRect.right - dstRect.left;
    const int dstH = dstRect.bottom - dstRect.top;
    if (dstW <= 0 || dstH <= 0) return;

    const int saved = SaveDC(hdc);
    IntersectClipRect(hdc, dstRect.left, dstRect.top, dstRect.right, dstRect.bottom);
    SetBkMode(hdc, TRANSPARENT);

    // 1) Las capas SHIFT / ALPHA se muestran limpiamente en el Inspector lateral (sin tapar teclas).
    // Si esta activo Capas SHIFT/ALPHA y hay una tecla bajo inspeccion, trazamos un halo exterior sutil.
    if (g_shiftAlphaLayerMode && g_explorerKind == 1 && g_explorerCalcIdx == calcIdx && g_explorerItemId >= 0) {
        KeyVisual kv{};
        if (FillKeyVisual(calcIdx, g_explorerItemId, kv)) {
            const auto* rec = CasioKnowledge::FindKeyRecord(calcIdx, g_explorerItemId);
            if (rec) {
                RECT outerRc{};
                outerRc.left   = dstRect.left + MulDiv(kv.cx - kv.w / 2, dstW, calc.skinW) - 4;
                outerRc.top    = dstRect.top  + MulDiv(kv.cy - kv.h / 2, dstH, calc.skinH) - 4;
                outerRc.right  = dstRect.left + MulDiv(kv.cx + kv.w / 2, dstW, calc.skinW) + 4;
                outerRc.bottom = dstRect.top  + MulDiv(kv.cy + kv.h / 2, dstH, calc.skinH) + 4;
                COLORREF layerCol = rec->shift.available ? CasioTheme::AmberWarm
                                  : (rec->alpha.available ? CasioTheme::RoseRed : CasioTheme::AccentBlue);
                CasioTheme::FillRoundRectAA(hdc, outerRc, 7.0f, CLR_INVALID, layerCol, 1.3f, 0, 210);
            }
        }
    }

    // 2) Resaltado sincronizado del Reproductor Visual Paso a Paso
    int playerKeyId = -1;
    int playerStepNo = 0;
    if (GetTimelinePlayerActiveKey(calcIdx, playerKeyId, playerStepNo) && playerKeyId >= 0) {
        KeyVisual kv{};
        if (FillKeyVisual(calcIdx, playerKeyId, kv)) {
            RECT kr{};
            kr.left   = dstRect.left + MulDiv(kv.cx - kv.w / 2, dstW, calc.skinW) - 3;
            kr.top    = dstRect.top  + MulDiv(kv.cy - kv.h / 2, dstH, calc.skinH) - 3;
            kr.right  = dstRect.left + MulDiv(kv.cx + kv.w / 2, dstW, calc.skinW) + 3;
            kr.bottom = dstRect.top  + MulDiv(kv.cy + kv.h / 2, dstH, calc.skinH) + 3;

            CasioTheme::FillRoundRectAA(hdc, kr, 8.0f, CLR_INVALID, CasioTheme::SuccessGreen, 2.5f);

            HFONT hBadgeF = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            const int py1 = (std::min)(static_cast<int>(dstRect.bottom - 2), static_cast<int>(kr.bottom + 14));
            RECT rcStep = { kr.left, py1 - 15, kr.left + 60, py1 };
            CasioTheme::DrawModernPill(hdc, rcStep, RGB(14, 92, 64), CasioTheme::SuccessGreen, RGB(255, 255, 255),
                                       L"Paso #" + std::to_wstring(playerStepNo), hBadgeF, 5.0f);
            DeleteObject(hBadgeF);
        }
    }

    // 3) Borde sutil sobre la tecla fijada actualmente en Explorador
    if (g_keyExplorerEnabled && g_explorerKind == 1 && g_explorerCalcIdx == calcIdx && g_explorerItemId >= 0) {
        KeyVisual kv{};
        if (FillKeyVisual(calcIdx, g_explorerItemId, kv)) {
            RECT kr{};
            kr.left   = dstRect.left + MulDiv(kv.cx - kv.w / 2, dstW, calc.skinW) - 2;
            kr.top    = dstRect.top  + MulDiv(kv.cy - kv.h / 2, dstH, calc.skinH) - 2;
            kr.right  = dstRect.left + MulDiv(kv.cx + kv.w / 2, dstW, calc.skinW) + 2;
            kr.bottom = dstRect.top  + MulDiv(kv.cy + kv.h / 2, dstH, calc.skinH) + 2;
            COLORREF cRing = g_explorerPinned ? CasioTheme::AmberWarm : CasioTheme::AccentHover;
            CasioTheme::FillRoundRectAA(hdc, kr, 6.0f, CLR_INVALID, cRing, g_explorerPinned ? 2.0f : 1.4f);
        }
    }

    RestoreDC(hdc, saved);
}

// ============================================================================
// Funciones publicas del Explorador Flotante sobre CanvasWnd
// ============================================================================
inline void OpenExplorerForKey(int calcIdx, int keyId, bool pinCard = false, int layer = -1) {
    if (calcIdx < 0 || calcIdx > 3 || keyId < 0 || keyId >= 50) return;
    bool changed = false;
    if (g_explorerKind == 1 && g_explorerCalcIdx == calcIdx && g_explorerItemId == keyId) {
        if (pinCard && !g_explorerPinned) { g_explorerPinned = true; changed = true; }
        if (layer >= 0 && layer <= 2 && g_explorerLayer != layer) { g_explorerLayer = layer; changed = true; }
    } else {
        g_explorerKind = 1;
        g_explorerCalcIdx = calcIdx;
        g_explorerItemId = keyId;
        if (pinCard) g_explorerPinned = true;
        if (layer >= 0 && layer <= 2) g_explorerLayer = layer;
        else g_explorerLayer = 0;
        changed = true;
    }
    if (g_explorerIndLit) { g_explorerIndLit = false; changed = true; }
    if (changed) {
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
    }
}

inline void OpenExplorerForIndicator(int calcIdx, int spriteIndex, bool isLit, bool pinCard = true) {
    if (calcIdx < 0 || calcIdx > 1 || spriteIndex < 0) return;
    g_explorerKind = 2;
    g_explorerCalcIdx = calcIdx;
    g_explorerItemId = spriteIndex;
    g_explorerIndLit = isLit;
    g_explorerPinned = pinCard;
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
}

inline void CloseExplorerCard() {
    g_explorerKind = 0;
    g_explorerPinned = false;
    g_explorerRects = {};
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
}

inline void ClearDockedExplorerHitRects() {
    g_explorerRects = {};
}

inline int GetDockedExplorerHeight(int availClientH) {
    if ((!g_keyExplorerEnabled && !g_explorerPinned) || g_explorerKind == 0) {
        return 42;
    }
    int desired = 248;
    if (g_explorerKind == 1 && g_shiftAlphaLayerMode) desired = 308;
    else if (g_explorerKind == 2) desired = 228;
    int maxAllowed = (std::max)(180, availClientH - 260);
    return (std::min)(desired, maxAllowed);
}

inline void DrawDockedExplorerInspector(HDC hdc, const RECT& cardRc) {
    g_explorerRects = {};
    const int cardW = cardRc.right - cardRc.left;
    const int cardH = cardRc.bottom - cardRc.top;
    if (cardW < 180 || cardH < 28) return;

    g_explorerRects.card = cardRc;
    g_explorerRects.visible = true;

    HPEN penTop = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
    HGDIOBJ oldP = SelectObject(hdc, penTop);
    MoveToEx(hdc, cardRc.left, cardRc.top, nullptr);
    LineTo(hdc, cardRc.right, cardRc.top);
    SelectObject(hdc, oldP);
    DeleteObject(penTop);

    HBRUSH brDock = CreateSolidBrush(CasioTheme::StudioSurfaceAlt());
    RECT fillArea = { cardRc.left, cardRc.top + 1, cardRc.right, cardRc.bottom };
    FillRect(hdc, &fillArea, brDock);
    DeleteObject(brDock);

    SetBkMode(hdc, TRANSPARENT);
    HFONT hTitleF = CasioTheme::CreateUiFont(14, true);
    HFONT hSmallF = CasioTheme::CreateUiFont(12, false);
    HFONT hSmallB = CasioTheme::CreateUiFont(12, true);

    if ((!g_keyExplorerEnabled && !g_explorerPinned) || g_explorerKind == 0) {
        HGDIOBJ of = SelectObject(hdc, hSmallB);
        RECT iconRc = { cardRc.left + 16, cardRc.top + 13, cardRc.left + 32, cardRc.bottom - 13 };
        CasioTheme::DrawSidebarVectorGlyph(hdc, iconRc, CasioTheme::GLYPH_INSPECTOR,
            g_keyExplorerEnabled ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextMuted());
        const int swY = (cardRc.top + cardRc.bottom) / 2 - 9;
        const RECT swRc = { cardRc.right - 50, swY, cardRc.right - 14, swY + 18 };
        g_explorerRects.pinBtn = { cardRc.right - 64, cardRc.top + 4, cardRc.right - 10, cardRc.bottom - 4 };
        CasioTheme::DrawToggleSwitch(hdc, swRc, g_keyExplorerEnabled, CasioTheme::StudioAccent(), !CasioTheme::g_studioDarkTheme);

        SetTextColor(hdc, g_keyExplorerEnabled ? CasioTheme::StudioText() : CasioTheme::StudioTextSec());
        RECT msgRc = { cardRc.left + 38, cardRc.top + 2, g_explorerRects.pinBtn.left - 8, cardRc.bottom };
        const wchar_t* idleTxt = g_keyExplorerEnabled
            ? L"Explorar al pasar"
            : L"Inspector desactivado";
        DrawTextW(hdc, idleTxt, -1, &msgRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, of);
        DeleteObject(hTitleF); DeleteObject(hSmallF); DeleteObject(hSmallB);
        return;
    }

    RECT innerCard = { cardRc.left + 6, cardRc.top + 4, cardRc.right - 6, cardRc.bottom - 4 };

    g_explorerRects.closeBtn = { innerCard.right - 30, innerCard.top + 8, innerCard.right - 8, innerCard.top + 30 };
    g_explorerRects.pinBtn   = { innerCard.right - 86, innerCard.top + 8, innerCard.right - 34, innerCard.top + 30 };
    CasioTheme::DrawStudioButton(hdc, g_explorerRects.closeBtn, L"\u00D7", hSmallB, CasioTheme::BTN_TOOLBAR_GHOST, false, false, 6.0f);
    CasioTheme::DrawStudioButton(hdc, g_explorerRects.pinBtn,
        g_explorerPinned ? L"Fijo" : L"Auto", hSmallB,
        g_explorerPinned ? CasioTheme::BTN_AMBER : CasioTheme::BTN_SECONDARY, g_explorerPinned, false, 6.0f);

    const wchar_t* modelShort[4] = { L"ClassWiz", L"ES PLUS", L"fx-CG50", L"fx-991CW" };
    const int innerW = innerCard.right - innerCard.left;

    if (g_explorerKind == 1) {
        const auto* rec = CasioKnowledge::FindKeyRecord(g_explorerCalcIdx, g_explorerItemId);
        if (!rec && g_explorerCalcIdx == 3) {
            const auto& cap = KeyCaptureForHistory(hdc, 3, g_explorerItemId);
            int textLeft = innerCard.left + 12;
            if (cap.bitmap && cap.width > 0 && cap.height > 0) {
                HDC hdcK = CreateCompatibleDC(hdc);
                HBITMAP oK = (HBITMAP)SelectObject(hdcK, cap.bitmap);
                double sc = (std::min)(34.0 / cap.width, 28.0 / cap.height);
                int dw = (std::max)(18, static_cast<int>(cap.width * sc));
                int dh = (std::max)(16, static_cast<int>(cap.height * sc));
                SetStretchBltMode(hdc, HALFTONE);
                StretchBlt(hdc, innerCard.left + 10, innerCard.top + 7, dw, dh, hdcK, 0, 0, cap.width, cap.height, SRCCOPY);
                SelectObject(hdcK, oK);
                DeleteDC(hdcK);
                textLeft = innerCard.left + dw + 18;
            }
            std::wstring lbl = KeyHistoryCaption(3, g_explorerItemId);
            HGDIOBJ oF = SelectObject(hdc, hTitleF);
            RECT rcT = { textLeft, innerCard.top + 4, g_explorerRects.pinBtn.left - 6, innerCard.top + 23 };
            DrawLatexRichText(hdc, L"Tecla " + lbl, rcT, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);
            SelectObject(hdc, hSmallF);
            SetTextColor(hdc, CasioTheme::StudioTextSec());
            std::wstring sub = std::wstring(modelShort[3]) + L"  \u00B7  #" + std::to_wstring(g_explorerItemId + 1);
            RECT rcSub = { textLeft, innerCard.top + 23, g_explorerRects.pinBtn.left - 6, innerCard.top + 38 };
            DrawTextW(hdc, sub.c_str(), -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            const int actionTop = innerCard.bottom - 34;
            const int actionBot = innerCard.bottom - 8;
            RECT rcExp = { innerCard.left + 12, innerCard.top + 48, innerCard.right - 12, actionTop - 8 };
            DrawLatexRichText(hdc, L"Pulsaci\u00F3n registrada en fx-991CW.",
                rcExp, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);
            int wHalf = (innerW - 26) / 2;
            g_explorerRects.btnTry   = { innerCard.left + 10, actionTop, innerCard.left + 10 + wHalf, actionBot };
            g_explorerRects.btnGuide = { innerCard.left + 16 + wHalf, actionTop, innerCard.right - 10, actionBot };
            CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnTry, L"Probar", hSmallB, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
            CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnGuide, L"+ Gu\u00EDa", hSmallB, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
            SelectObject(hdc, oF);
            DeleteObject(hTitleF); DeleteObject(hSmallF); DeleteObject(hSmallB);
            return;
        }
        if (!rec) {
            DeleteObject(hTitleF); DeleteObject(hSmallF); DeleteObject(hSmallB);
            return;
        }
        const auto& cap = KeyCaptureForHistory(hdc, g_explorerCalcIdx, g_explorerItemId);
        int textLeft = innerCard.left + 12;
        if (cap.bitmap && cap.width > 0 && cap.height > 0) {
            HDC hdcK = CreateCompatibleDC(hdc);
            HBITMAP oK = (HBITMAP)SelectObject(hdcK, cap.bitmap);
            double sc = (std::min)(34.0 / cap.width, 28.0 / cap.height);
            int dw = (std::max)(18, static_cast<int>(cap.width * sc));
            int dh = (std::max)(16, static_cast<int>(cap.height * sc));
            SetStretchBltMode(hdc, HALFTONE);
            StretchBlt(hdc, innerCard.left + 10, innerCard.top + 7, dw, dh, hdcK, 0, 0, cap.width, cap.height, SRCCOPY);
            SelectObject(hdcK, oK);
            DeleteDC(hdcK);
            textLeft = innerCard.left + dw + 18;
        }

        const CasioKnowledge::KeyLayerInfo* layers[3] = { &rec->normal, &rec->shift, &rec->alpha };
        if (g_explorerLayer < 0 || g_explorerLayer > 2 || !layers[g_explorerLayer]->available) {
            g_explorerLayer = 0;
        }
        const auto& activeLyr = *layers[g_explorerLayer];

        HGDIOBJ oF = SelectObject(hdc, hTitleF);
        RECT rcT = { textLeft, innerCard.top + 4, g_explorerRects.pinBtn.left - 6, innerCard.top + 23 };
        DrawLatexRichText(hdc, activeLyr.label, rcT, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

        SelectObject(hdc, hSmallF);
        SetTextColor(hdc, CasioTheme::StudioTextSec());
        std::wstring sub = std::wstring(modelShort[g_explorerCalcIdx]) + L"  \u00B7  " + activeLyr.category + L"  \u00B7  #" + std::to_wstring(rec->keyId + 1);
        RECT rcSub = { textLeft, innerCard.top + 23, g_explorerRects.pinBtn.left - 6, innerCard.top + 38 };
        DrawTextW(hdc, sub.c_str(), -1, &rcSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Selector segmentado limpio de capas [Normal | SHIFT | ALPHA]
        const wchar_t* lNames[3] = { L"Normal", L"SHIFT", L"ALPHA" };
        RECT rcSeg = { innerCard.left + 10, innerCard.top + 43, innerCard.right - 10, innerCard.top + 67 };
        CasioTheme::FillRoundRectAA(hdc, rcSeg, 7.0f, CasioTheme::StudioTrack(), CasioTheme::StudioBorder(), 1.0f);
        int tabW = ((rcSeg.right - rcSeg.left) - 10) / 3;
        int tabX = rcSeg.left + 3;
        for (int i = 0; i < 3; ++i) {
            g_explorerRects.layerTab[i] = { tabX, rcSeg.top + 2, tabX + tabW, rcSeg.bottom - 2 };
            bool avail = layers[i]->available;
            bool sel = (g_explorerLayer == i);
            if (sel && avail) {
                COLORREF bCol = (i == 1) ? CasioTheme::StudioAmber() : ((i == 2) ? CasioTheme::StudioRose() : CasioTheme::StudioBorderStrong());
                COLORREF tCol = (i == 1) ? CasioTheme::StudioAmberText() : ((i == 2) ? CasioTheme::StudioRoseText() : CasioTheme::StudioText());
                CasioTheme::FillRoundRectAA(hdc, g_explorerRects.layerTab[i], 5.5f, CasioTheme::StudioSurface(), bCol, 1.0f);
                SetTextColor(hdc, tCol);
                SelectObject(hdc, hSmallB);
                DrawTextW(hdc, lNames[i], -1, &g_explorerRects.layerTab[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                SetTextColor(hdc, !avail ? CasioTheme::StudioTextMuted() : CasioTheme::StudioTextSec());
                SelectObject(hdc, hSmallF);
                DrawTextW(hdc, lNames[i], -1, &g_explorerRects.layerTab[i], DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            tabX += tabW + 2;
        }

        const int actionTop = innerCard.bottom - 34;
        const int actionBot = innerCard.bottom - 8;
        int cy = rcSeg.bottom + 5;

        if (g_shiftAlphaLayerMode) {
            for (int li = 0; li < 3 && cy + 29 < actionTop; ++li) {
                const auto& lyr = *layers[li];
                RECT rowRc = { innerCard.left + 10, cy, innerCard.right - 10, cy + 28 };
                COLORREF tagBg = (li == 1) ? CasioTheme::StudioAmberSoft() : ((li == 2) ? CasioTheme::StudioRoseSoft() : CasioTheme::StudioAccentSoft());
                COLORREF tagBd = (li == 1) ? CasioTheme::StudioAmber() : ((li == 2) ? CasioTheme::StudioRose() : CasioTheme::StudioAccent());
                COLORREF tagFg = (li == 1) ? CasioTheme::StudioAmberText() : ((li == 2) ? CasioTheme::StudioRoseText() : CasioTheme::StudioAccentText());
                if (!lyr.available) { tagBg = CasioTheme::StudioSurfaceAlt(); tagBd = CasioTheme::StudioBorder(); tagFg = CasioTheme::StudioTextMuted(); }
                CasioTheme::FillRoundRectAA(hdc, rowRc, 5.0f, CasioTheme::StudioSurface(),
                    (g_explorerLayer == li) ? tagBd : CasioTheme::StudioBorder(), 1.0f);
                RECT tagRc = { rowRc.left + 4, rowRc.top + 3, rowRc.left + 64, rowRc.bottom - 3 };
                CasioTheme::DrawModernPill(hdc, tagRc, tagBg, tagBd, tagFg, lNames[li], hSmallB, 4.0f);
                RECT txtRc = { tagRc.right + 7, rowRc.top + 1, rowRc.right - 6, rowRc.bottom - 1 };
                std::wstring lineTxt = lyr.available ? (lyr.label + L"  \u2014  " + lyr.summary) : L"Sin asignaci\u00F3n";
                DrawLatexRichText(hdc, lineTxt, txtRc,
                    lyr.available ? CasioTheme::StudioText() : CasioTheme::StudioTextMuted(),
                    CasioTheme::StudioAccentText(), 12, true);
                cy += 30;
            }
        } else {
            const int expBot = (std::min)(actionTop - 78, cy + 32);
            if (expBot > cy + 12) {
                RECT rcExp = { innerCard.left + 12, cy, innerCard.right - 12, expBot };
                DrawLatexRichText(hdc, activeLyr.summary, rcExp, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);
                cy = expBot + 4;
            }
        }

        const int tipReserve = g_shiftAlphaLayerMode ? 6 : 22;
        const int formBot = (std::max)(cy + 34, actionTop - tipReserve);
        if (formBot > cy + 14 && formBot <= actionTop - 4) {
            RECT rcForm = { innerCard.left + 10, cy, innerCard.right - 10, formBot };
            int formMid = (rcForm.top + rcForm.bottom) / 2;
            RECT rcF1 = { rcForm.left + 2, rcForm.top + 2, rcForm.right - 2, formMid };
            RECT rcF2 = { rcForm.left + 2, formMid, rcForm.right - 2, rcForm.bottom - 2 };
            DrawLatexRichText(hdc, activeLyr.latexFormula, rcF1, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);
            DrawLatexRichText(hdc, L"Ej. " + activeLyr.latexExample, rcF2, CasioTheme::StudioTextSec(), CasioTheme::StudioSuccessText(), 13, true);
            cy = formBot + 3;
        }

        if (!g_shiftAlphaLayerMode && cy + 14 <= actionTop - 2) {
            RECT rcTip = { innerCard.left + 12, cy, innerCard.right - 12, actionTop - 2 };
            std::wstring tipTxt = activeLyr.tipOrWarning.empty()
                ? (L"Manual: " + rec->pdfFile + L" (p. " + std::to_wstring(rec->pdfPage) + L")")
                : (L"Nota: " + activeLyr.tipOrWarning);
            DrawLatexRichText(hdc, tipTxt, rcTip, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, false);
        }

        int bwTotal = innerW - 20;
        int gap = 4;
        int bW = (bwTotal - gap * 4) / 5;
        int bx = innerCard.left + 10;
        g_explorerRects.btnTry    = { bx, actionTop, bx + bW, actionBot }; bx += bW + gap;
        g_explorerRects.btnPdf    = { bx, actionTop, bx + bW, actionBot }; bx += bW + gap;
        g_explorerRects.btnGuide  = { bx, actionTop, bx + bW, actionBot }; bx += bW + gap;
        g_explorerRects.btnFav    = { bx, actionTop, bx + bW, actionBot }; bx += bW + gap;
        g_explorerRects.btnStudio = { bx, actionTop, innerCard.right - 10, actionBot };

        std::wstring favKeyId = L"key_" + std::to_wstring(rec->calcIdx) + L"_" + std::to_wstring(rec->keyId);
        bool isFav = FindFavoriteIndex(L"key", favKeyId) >= 0;
        std::wstring pdfLbl = L"PDF p." + std::to_wstring(rec->pdfPage);

        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnTry, L"Probar", hSmallB, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnPdf, pdfLbl, hSmallF, CasioTheme::BTN_TOOLBAR_GHOST, false, false, 6.0f);
        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnGuide, L"+ Gu\u00EDa", hSmallF, CasioTheme::BTN_TOOLBAR_GHOST, false, false, 6.0f);
        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnFav, isFav ? L"En Fav" : L"Favorito", hSmallF,
            isFav ? CasioTheme::BTN_AMBER : CasioTheme::BTN_TOOLBAR_GHOST, isFav, false, 6.0f);
        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnStudio, L"Estudio", hSmallB, CasioTheme::BTN_PRIMARY, false, false, 6.0f);

        SelectObject(hdc, oF);
    } else if (g_explorerKind == 2) {
        const auto* ind = CasioKnowledge::FindIndicatorDoc(g_explorerCalcIdx, g_explorerItemId);
        if (!ind) {
            DeleteObject(hTitleF); DeleteObject(hSmallF); DeleteObject(hSmallB);
            return;
        }
        HGDIOBJ oF = SelectObject(hdc, hTitleF);
        RECT rcSym = { innerCard.left + 10, innerCard.top + 8, innerCard.left + 62, innerCard.top + 34 };
        CasioTheme::FillRoundRectAA(hdc, rcSym, 6.0f,
            g_explorerIndLit ? CasioTheme::StudioSuccessSoft() : CasioTheme::StudioSurface(),
            g_explorerIndLit ? CasioTheme::StudioSuccess() : CasioTheme::StudioBorderStrong(), 1.0f);
        DrawLatexRichTextCentered(hdc, ind->symbol, rcSym,
            g_explorerIndLit ? CasioTheme::StudioSuccessText() : CasioTheme::StudioText(),
            CasioTheme::StudioAccentText(), 13);

        RECT rcT = { innerCard.left + 70, innerCard.top + 4, g_explorerRects.pinBtn.left - 6, innerCard.top + 23 };
        DrawLatexRichText(hdc, ind->title, rcT, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

        SelectObject(hdc, hSmallF);
        SetTextColor(hdc, g_explorerIndLit ? CasioTheme::StudioSuccessText() : CasioTheme::StudioTextSec());
        std::wstring st = std::wstring(modelShort[g_explorerCalcIdx]) + L"  \u00B7  " + (g_explorerIndLit ? L"Activo" : L"Inactivo");
        RECT rcSt = { innerCard.left + 70, innerCard.top + 22, g_explorerRects.pinBtn.left - 6, innerCard.top + 38 };
        DrawTextW(hdc, st.c_str(), -1, &rcSt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        const int actionTop = innerCard.bottom - 34;
        const int actionBot = innerCard.bottom - 8;
        RECT rcM = { innerCard.left + 12, innerCard.top + 44, innerCard.right - 12, innerCard.top + 96 };
        DrawLatexRichText(hdc, ind->meaning, rcM, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);

        RECT rcW = { innerCard.left + 12, innerCard.top + 98, innerCard.right - 12, innerCard.top + 148 };
        DrawLatexRichText(hdc, L"Uso: " + ind->whenAppears, rcW, CasioTheme::StudioAccentText(), CasioTheme::StudioTextSec(), 12, false);

        if (actionTop > innerCard.top + 154) {
            RECT rcK = { innerCard.left + 12, innerCard.top + 150, innerCard.right - 12, actionTop - 4 };
            DrawLatexRichText(hdc, L"Teclas: " + ind->relatedKeys, rcK, CasioTheme::StudioSuccessText(), CasioTheme::StudioAccentText(), 12, false);
        }

        int wHalf = (innerW - 26) / 2;
        g_explorerRects.btnPdf    = { innerCard.left + 10, actionTop, innerCard.left + 10 + wHalf, actionBot };
        g_explorerRects.btnStudio = { innerCard.left + 16 + wHalf, actionTop, innerCard.right - 10, actionBot };
        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnPdf, L"Manual p." + std::to_wstring(ind->pdfPage),
            hSmallF, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
        CasioTheme::DrawStudioButton(hdc, g_explorerRects.btnStudio, L"Diagn\u00F3stico LCD",
            hSmallB, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
        SelectObject(hdc, oF);
    }

    DeleteObject(hTitleF);
    DeleteObject(hSmallF);
    DeleteObject(hSmallB);
}

inline void DrawExplorerCardOverlay(HDC, const RECT&) {
    // El Inspector ahora vive acoplado en el panel derecho para no tapar las calculadoras.
}

// Forward declaration de apertura de Centro de Estudio (definida abajo)
inline void OpenLearningStudioWindow(int initialTab = 0, int calcIdx = -1, int focusKeyId = -1, const std::wstring& searchQuery = L"");

inline bool IsPointInsideExplorerCard(int, int) {
    return false;
}

inline bool HandleExplorerOverlayClick(int, int) {
    return false;
}

inline bool HandleDockedExplorerClick(int lx, int ly) {
    if (!g_explorerRects.visible) return false;
    POINT pt{ lx, ly };
    if (!PtInRect(&g_explorerRects.card, pt)) return false;

    if ((!g_keyExplorerEnabled && !g_explorerPinned) || g_explorerKind == 0) {
        if (PtInRect(&g_explorerRects.pinBtn, pt)) {
            g_keyExplorerEnabled = !g_keyExplorerEnabled;
            SaveUnifiedSettingsToIni();
            InvalidateSidebarButtons();
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            return true;
        }
        return true;
    }

    if (PtInRect(&g_explorerRects.closeBtn, pt)) {
        CloseExplorerCard();
        return true;
    }
    if (PtInRect(&g_explorerRects.pinBtn, pt)) {
        g_explorerPinned = !g_explorerPinned;
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
        return true;
    }

    if (g_explorerKind == 1) {
        const auto* rec = CasioKnowledge::FindKeyRecord(g_explorerCalcIdx, g_explorerItemId);
        if (!rec) {
            if (g_explorerCalcIdx == 3) {
                if (PtInRect(&g_explorerRects.btnTry, pt)) {
                    HandleKeyPress(3, g_explorerItemId);
                    return true;
                }
                if (PtInRect(&g_explorerRects.btnGuide, pt)) {
                    StudioDetail::AppendKeyToExporter(3, g_explorerItemId,
                        L"fx-991CW: " + std::wstring(KeyHistoryCaption(3, g_explorerItemId)),
                        L"Secuencia en CASIO ClassWiz II fx-991CW");
                    OpenSequenceExporterWindow(false);
                    return true;
                }
            }
            return true;
        }
        const CasioKnowledge::KeyLayerInfo* layers[3] = { &rec->normal, &rec->shift, &rec->alpha };
        for (int i = 0; i < 3; ++i) {
            if (PtInRect(&g_explorerRects.layerTab[i], pt) && layers[i]->available) {
                g_explorerLayer = i;
                g_explorerPinned = true;
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
                if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
                return true;
            }
        }
        if (PtInRect(&g_explorerRects.btnTry, pt)) {
            if (!rec->demoSeq.empty()) {
                ExecuteHelpButtonSequenceOnCalc(rec->calcIdx, rec->demoSeq);
            } else {
                SaveCalculatorSnapshotState(rec->calcIdx);
                HandleKeyPress(rec->calcIdx, rec->keyId);
            }
            return true;
        }
        if (PtInRect(&g_explorerRects.btnPdf, pt)) {
            OpenOfficialPdfManual(rec->pdfFile, rec->pdfPage);
            return true;
        }
        if (PtInRect(&g_explorerRects.btnGuide, pt)) {
            const auto& activeLyr = *layers[g_explorerLayer];
            if (!rec->demoSeq.empty()) {
                LoadCustomSequenceIntoExporter(rec->calcIdx, rec->demoSeq,
                    L"Uso de Tecla: " + activeLyr.label,
                    activeLyr.summary + L"\r\nEjemplo: " + activeLyr.latexExample);
                OpenSequenceExporterWindow(false);
            } else {
                StudioDetail::AppendKeyToExporter(rec->calcIdx, rec->keyId,
                    L"Uso de Tecla: " + activeLyr.label, activeLyr.summary);
            }
            return true;
        }
        if (PtInRect(&g_explorerRects.btnFav, pt)) {
            CasioKnowledge::FavoriteItem f;
            f.kind = L"key";
            f.id = L"key_" + std::to_wstring(rec->calcIdx) + L"_" + std::to_wstring(rec->keyId);
            f.title = rec->normal.label;
            f.subtitle = rec->normal.category;
            f.calcIdx = rec->calcIdx;
            f.demoSeq = rec->demoSeq.empty() ? std::vector<int>{ rec->keyId } : rec->demoSeq;
            f.pdfFile = rec->pdfFile;
            f.pdfPage = rec->pdfPage;
            bool added = ToggleStudioFavorite(f);
            ShowToolbarToast(added ? L"Guardado en Favoritos." : L"Quitado de Favoritos.");
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            return true;
        }
        if (PtInRect(&g_explorerRects.btnStudio, pt)) {
            OpenLearningStudioWindow(0, rec->calcIdx, rec->keyId, L"");
            return true;
        }
    } else if (g_explorerKind == 2) {
        const auto* ind = CasioKnowledge::FindIndicatorDoc(g_explorerCalcIdx, g_explorerItemId);
        if (!ind) return true;
        if (PtInRect(&g_explorerRects.btnPdf, pt)) {
            OpenOfficialPdfManual(ind->pdfFile, ind->pdfPage);
            return true;
        }
        if (PtInRect(&g_explorerRects.btnStudio, pt)) {
            OpenLearningStudioWindow(3, ind->calcIdx, -1, ind->symbol);
            return true;
        }
    }
    return true;
}

// ============================================================================
// Menu Contextual Transversal (WM_RBUTTONUP en Tecla / LCD / Popup / Historial)
// ============================================================================
inline void ShowCalculatorContextMenu(
    HWND hwndOwner, int clientX, int clientY,
    int calcIdx, int keyId, int indSpriteIdx, bool indLit, bool isLcdBody,
    int timelineStepIdx = -1
) {
    g_ctxTarget = { calcIdx, keyId, indSpriteIdx, indLit, isLcdBody, timelineStepIdx };
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    if (keyId >= 0) {
        const auto* rec = CasioKnowledge::FindKeyRecord(calcIdx, keyId);
        std::wstring title = rec ? rec->normal.label : L"Tecla seleccionada";
        AppendMenuW(hMenu, MF_STRING, 1200, (L"Explicar tecla (" + title + L") en Explorador").c_str());
        AppendMenuW(hMenu, MF_STRING, 1202, L"Abrir en Centro de Estudio (3 capas)");
        AppendMenuW(hMenu, MF_STRING, 1203, L"Probar ejemplo en calculadora");
        AppendMenuW(hMenu, MF_STRING, 1205, L"Agregar tecla / ejemplo al Exportador de Gu\u00EDa");
        AppendMenuW(hMenu, MF_STRING, 1206, L"Alternar en Favoritos");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        std::wstring pdfLbl = rec ? (L"Abrir Manual PDF (p. " + std::to_wstring(rec->pdfPage) + L")") : L"Abrir Manual PDF";
        AppendMenuW(hMenu, MF_STRING, 1204, pdfLbl.c_str());
    }
    if (indSpriteIdx >= 0) {
        const auto* ind = CasioKnowledge::FindIndicatorDoc(calcIdx, indSpriteIdx);
        std::wstring lbl = ind ? (ind->symbol + L" - " + ind->title) : L"Indicador LCD";
        AppendMenuW(hMenu, MF_STRING, 1200, (L"Explicar indicador: " + lbl).c_str());
        AppendMenuW(hMenu, MF_STRING, 1207, L"Diagnosticar indicador (Por que muestra esto?)");
        AppendMenuW(hMenu, MF_STRING, 1204, ind ? (L"Ver en Manual PDF (p. " + std::to_wstring(ind->pdfPage) + L")").c_str() : L"Ver Manual PDF");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    } else if (isLcdBody) {
        AppendMenuW(hMenu, MF_STRING, 1207, L"Diagnosticar resultado o formato LCD");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    }

    AppendMenuW(hMenu, MF_STRING | (g_shiftAlphaLayerMode ? MF_CHECKED : MF_UNCHECKED), 1201, L"Ver 3 capas (Normal/SHIFT/ALPHA) en Inspector");
    AppendMenuW(hMenu, MF_STRING, 1208, L"Copiar captura LCD al Portapapeles");
    AppendMenuW(hMenu, MF_STRING, 1209, L"Abrir / enfocar Pop-up Display");

    POINT pt{ clientX, clientY };
    ClientToScreen(hwndOwner, &pt);
    UINT cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hwndOwner, nullptr);
    DestroyMenu(hMenu);

    if (cmd == 0) return;
    switch (cmd) {
    case 1200:
        if (g_ctxTarget.indSpriteIdx >= 0) {
            g_keyExplorerEnabled = true;
            SetLearningDrawerOpen(true);
            OpenExplorerForIndicator(g_ctxTarget.calcIdx, g_ctxTarget.indSpriteIdx, g_ctxTarget.indLit, true);
        } else if (g_ctxTarget.keyId >= 0) {
            g_keyExplorerEnabled = true;
            SetLearningDrawerOpen(true);
            OpenExplorerForKey(g_ctxTarget.calcIdx, g_ctxTarget.keyId, true, -1);
        }
        break;
    case 1201:
        g_shiftAlphaLayerMode = !g_shiftAlphaLayerMode;
        SaveUnifiedSettingsToIni();
        InvalidateSidebarButtons();
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
        break;
    case 1202:
        OpenLearningStudioWindow(0, g_ctxTarget.calcIdx, g_ctxTarget.keyId, L"");
        break;
    case 1203:
        if (g_ctxTarget.keyId >= 0) {
            const auto* rec = CasioKnowledge::FindKeyRecord(g_ctxTarget.calcIdx, g_ctxTarget.keyId);
            if (rec && !rec->demoSeq.empty()) {
                ExecuteHelpButtonSequenceOnCalc(rec->calcIdx, rec->demoSeq);
            } else {
                HandleKeyPress(g_ctxTarget.calcIdx, g_ctxTarget.keyId);
            }
        }
        break;
    case 1204:
        if (g_ctxTarget.indSpriteIdx >= 0) {
            if (const auto* ind = CasioKnowledge::FindIndicatorDoc(g_ctxTarget.calcIdx, g_ctxTarget.indSpriteIdx)) {
                OpenOfficialPdfManual(ind->pdfFile, ind->pdfPage);
            }
        } else if (g_ctxTarget.keyId >= 0) {
            if (const auto* rec = CasioKnowledge::FindKeyRecord(g_ctxTarget.calcIdx, g_ctxTarget.keyId)) {
                OpenOfficialPdfManual(rec->pdfFile, rec->pdfPage);
            }
        }
        break;
    case 1205:
        if (g_ctxTarget.keyId >= 0) {
            const auto* rec = CasioKnowledge::FindKeyRecord(g_ctxTarget.calcIdx, g_ctxTarget.keyId);
            std::wstring lbl = rec ? rec->normal.label : L"Tecla CASIO";
            std::wstring sum = rec ? rec->normal.summary : L"Secuencia manual";
            StudioDetail::AppendKeyToExporter(g_ctxTarget.calcIdx, g_ctxTarget.keyId, L"Secuencia: " + lbl, sum);
        }
        break;
    case 1206:
        if (g_ctxTarget.keyId >= 0) {
            if (const auto* rec = CasioKnowledge::FindKeyRecord(g_ctxTarget.calcIdx, g_ctxTarget.keyId)) {
                CasioKnowledge::FavoriteItem f;
                f.kind = L"key";
                f.id = L"key_" + std::to_wstring(rec->calcIdx) + L"_" + std::to_wstring(rec->keyId);
                f.title = rec->normal.label;
                f.subtitle = rec->normal.category;
                f.calcIdx = rec->calcIdx;
                f.demoSeq = rec->demoSeq.empty() ? std::vector<int>{ rec->keyId } : rec->demoSeq;
                f.pdfFile = rec->pdfFile;
                f.pdfPage = rec->pdfPage;
                bool added = ToggleStudioFavorite(f);
                ShowToolbarToast(added ? L"Agregado a Favoritos." : L"Quitado de Favoritos.");
            }
        }
        break;
    case 1207: {
        std::wstring q;
        if (g_ctxTarget.indSpriteIdx >= 0) {
            if (const auto* ind = CasioKnowledge::FindIndicatorDoc(g_ctxTarget.calcIdx, g_ctxTarget.indSpriteIdx)) {
                q = ind->symbol;
            }
        }
        OpenLearningStudioWindow(3, g_ctxTarget.calcIdx, -1, q);
        break;
    }
    case 1208:
        g_activeCalcIdx = g_ctxTarget.calcIdx;
        CopyCalculatorResultSmart(g_ctxTarget.calcIdx, -1, true);
        break;
    case 1209:
        g_activeCalcIdx = g_ctxTarget.calcIdx;
        TogglePopupDisplayWindow();
        break;
    }
}

// ============================================================================
// Centro de Estudio Unificado CASIO (6 Pestanas, Busqueda y Simulacion Directa)
// ============================================================================
inline HWND g_hStudioWnd = nullptr;
inline HWND g_hStudioSearchEdit = nullptr;
inline int  g_studioTab = 0;         // 0..5
inline int  g_studioModelFilter = -1; // -1=Todos, 0=CW, 1=ES, 2=CG50
inline int  g_studioProcCat = 0;      // Filtro de categoria en Tab 1
inline int  g_studioFocusKeyId = -1;
inline int  g_studioScrollY = 0;
inline std::wstring g_studioSearchQuery;

struct StudioHitAction {
    enum Kind {
        ACT_NONE = 0,
        ACT_SET_TAB,
        ACT_SET_MODEL,
        ACT_SET_PROC_CAT,
        ACT_RUN_SEQ,
        ACT_EXPORT_SEQ,
        ACT_OPEN_PDF,
        ACT_TOGGLE_FAV,
        ACT_FOCUS_KEY,
        ACT_TOGGLE_CHECK,
        ACT_OPEN_MANUAL_FOLDER,
        ACT_OPEN_SMART_HELP,
        ACT_OPEN_PLAYER_TAB,
        ACT_CLEAR_SEARCH
    };
    RECT rect{};
    Kind kind = ACT_NONE;
    int  arg1 = 0;
    int  arg2 = 0;
    std::vector<int> seq;
    std::wstring str1;
    std::wstring str2;
    std::wstring str3;
};

inline std::vector<StudioHitAction> g_studioHitActions;
inline std::vector<StudioHitAction> g_studioHeaderActions;
inline RECT g_studioTabRects[6]{};

inline HFONT g_hStudioSearchFont = nullptr;
inline int   g_studioSearchFontPx = 0;

inline float GetStudioUiScale(HWND hWnd) {
    return CasioTheme::ComputeDialogUiScale(hWnd, 1100, 760);
}

inline void SyncStudioSearchEdit(HWND hWnd) {
    if (!hWnd || !IsWindow(hWnd) || !g_hStudioSearchEdit) return;
    const float s = GetStudioUiScale(hWnd);
    const int wantPx = (std::max)(12, CasioTheme::UiToDevice(14, s));
    if (!g_hStudioSearchFont || g_studioSearchFontPx != wantPx) {
        if (g_hStudioSearchFont) { DeleteObject(g_hStudioSearchFont); g_hStudioSearchFont = nullptr; }
        g_hStudioSearchFont = CreateFontW(-wantPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_studioSearchFontPx = wantPx;
        SendMessageW(g_hStudioSearchEdit, WM_SETFONT, (WPARAM)g_hStudioSearchFont, TRUE);
    }
    RECT rcPhys{};
    GetClientRect(hWnd, &rcPhys);
    const int cw = CasioTheme::DeviceToLogical(rcPhys.right, s);
    MoveWindow(g_hStudioSearchEdit,
               CasioTheme::UiToDevice(24, s),
               CasioTheme::UiToDevice(54, s),
               CasioTheme::UiToDevice((std::max)(240, cw - 432), s),
               (std::max)(18, CasioTheme::UiToDevice(20, s)),
               TRUE);
}

inline LRESULT CALLBACK LearningStudioWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static constexpr int STUDIO_HEADER_H = 144;
    static constexpr UINT_PTR ID_STUDIO_SEARCH = 3401;

    switch (msg) {
    case WM_CREATE: {
        CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
        CasioTheme::EnableThemeScrollbar(hWnd, CasioTheme::g_studioDarkTheme);
        EnsurePdfIndexLoaded();
        EnsureFavoritesAndChecklistLoaded();
        g_hStudioSearchEdit = CreateWindowExW(
            0, L"EDIT", g_studioSearchQuery.c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            24, 54, 600, 20,
            hWnd, (HMENU)ID_STUDIO_SEARCH, g_hInst, nullptr);
        SendMessageW(g_hStudioSearchEdit, 0x1501 /*EM_SETCUEBANNER*/, TRUE,
                     (LPARAM)L"Buscar tecla, procedimiento, indicador (FIX, Deg, Rad), error o tema...");
        SyncStudioSearchEdit(hWnd);
        return 0;
    }

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hWnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        SyncStudioSearchEdit(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_CTLCOLOREDIT:
        return CasioTheme::HandleStudioEditCtlColor(wParam);

    case WM_GETMINMAXINFO: {
        const float s = GetStudioUiScale(hWnd);
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize = { CasioTheme::UiToDevice(880, s), CasioTheme::UiToDevice(620, s) };
        return 0;
    }

    case WM_SIZE: {
        SyncStudioSearchEdit(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_STUDIO_SEARCH) {
            const WORD code = HIWORD(wParam);
            if (code == EN_CHANGE) {
                wchar_t buf[256]{};
                GetWindowTextW(g_hStudioSearchEdit, buf, 255);
                g_studioSearchQuery = buf;
                g_studioScrollY = 0;
                InvalidateRect(hWnd, nullptr, FALSE);
            } else if (code == EN_SETFOCUS || code == EN_KILLFOCUS) {
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return 0;

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_VERT, &si);
        int maxS = (std::max)(0, si.nMax - (int)si.nPage + 1);
        g_studioScrollY = (std::max)(0, (std::min)(g_studioScrollY - (delta > 0 ? 96 : -96), maxS));
        SetScrollPos(hWnd, SB_VERT, g_studioScrollY, TRUE);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_VSCROLL: {
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_VERT, &si);
        int pos = si.nPos;
        switch (LOWORD(wParam)) {
        case SB_TOP: pos = 0; break;
        case SB_BOTTOM: pos = si.nMax; break;
        case SB_LINEUP: pos -= 64; break;
        case SB_LINEDOWN: pos += 64; break;
        case SB_PAGEUP: pos -= (int)si.nPage; break;
        case SB_PAGEDOWN: pos += (int)si.nPage; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        }
        int maxS = (std::max)(0, si.nMax - (int)si.nPage + 1);
        g_studioScrollY = (std::max)(0, (std::min)(pos, maxS));
        SetScrollPos(hWnd, SB_VERT, g_studioScrollY, TRUE);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        const float s = GetStudioUiScale(hWnd);
        int mx = CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s);
        int my = CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s);
        POINT pt{ mx, my };

        for (int i = 0; i < 6; ++i) {
            if (PtInRect(&g_studioTabRects[i], pt)) {
                g_studioTab = i;
                g_studioScrollY = 0;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
        if (my < STUDIO_HEADER_H) {
            for (const auto& act : g_studioHeaderActions) {
                if (PtInRect(&act.rect, pt)) {
                    if (act.kind == StudioHitAction::ACT_SET_MODEL) {
                        g_studioModelFilter = act.arg1;
                        g_studioScrollY = 0;
                    } else if (act.kind == StudioHitAction::ACT_CLEAR_SEARCH) {
                        g_studioSearchQuery.clear();
                        if (g_hStudioSearchEdit) SetWindowTextW(g_hStudioSearchEdit, L"");
                        g_studioScrollY = 0;
                    }
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
            }
            return 0;
        }
        for (const auto& act : g_studioHitActions) {
            if (!PtInRect(&act.rect, pt)) continue;
            switch (act.kind) {
            case StudioHitAction::ACT_SET_TAB:
                g_studioTab = act.arg1;
                g_studioScrollY = 0;
                break;
            case StudioHitAction::ACT_SET_MODEL:
                g_studioModelFilter = act.arg1;
                g_studioScrollY = 0;
                break;
            case StudioHitAction::ACT_SET_PROC_CAT:
                g_studioProcCat = act.arg1;
                g_studioScrollY = 0;
                break;
            case StudioHitAction::ACT_RUN_SEQ:
                ExecuteHelpButtonSequenceOnCalc(act.arg1, act.seq);
                if (!act.seq.empty()) {
                    OpenExplorerForKey(act.arg1, act.seq.front(), false, -1);
                }
                break;
            case StudioHitAction::ACT_EXPORT_SEQ:
                LoadCustomSequenceIntoExporter(act.arg1, act.seq, act.str1, act.str2);
                OpenSequenceExporterWindow(false);
                break;
            case StudioHitAction::ACT_OPEN_PDF:
                OpenOfficialPdfManual(act.str1, act.arg1);
                break;
            case StudioHitAction::ACT_TOGGLE_FAV: {
                CasioKnowledge::FavoriteItem f;
                f.kind = act.str1;
                f.id = act.str2;
                f.title = act.str3;
                f.calcIdx = act.arg1;
                f.pdfPage = act.arg2;
                f.demoSeq = act.seq;
                f.pdfFile = (act.arg1 == 1) ? L"fx-570_991ES_PLUS_ES.pdf" : (act.arg1 == 2 ? L"fx-CG50_Soft_v370_ES.pdf" : L"fx-570_991LA_X_ES.pdf");
                bool added = ToggleStudioFavorite(f);
                ShowToolbarToast(added ? L"Agregado a Favoritos." : L"Quitado de Favoritos.");
                break;
            }
            case StudioHitAction::ACT_FOCUS_KEY:
                g_studioFocusKeyId = (g_studioFocusKeyId == act.arg2) ? -1 : act.arg2;
                OpenExplorerForKey(act.arg1, act.arg2, false, -1);
                break;
            case StudioHitAction::ACT_TOGGLE_CHECK:
                if (act.arg1 >= 0 && act.arg1 < 8) {
                    g_studioChecklistDone[act.arg1] = !g_studioChecklistDone[act.arg1];
                }
                break;
            case StudioHitAction::ACT_OPEN_MANUAL_FOLDER:
                OpenOfficialManualsFolder();
                break;
            case StudioHitAction::ACT_OPEN_SMART_HELP:
                OpenSmartHelpWindow(g_studioSearchQuery);
                break;
            case StudioHitAction::ACT_CLEAR_SEARCH:
                g_studioSearchQuery.clear();
                if (g_hStudioSearchEdit) SetWindowTextW(g_hStudioSearchEdit, L"");
                g_studioScrollY = 0;
                break;
            default: break;
            }
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdcWin = BeginPaint(hWnd, &ps);
        RECT rcPhys{};
        GetClientRect(hWnd, &rcPhys);
        if (rcPhys.right <= 0 || rcPhys.bottom <= 0) { EndPaint(hWnd, &ps); return 0; }

        HDC hdc = CreateCompatibleDC(hdcWin);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcWin, rcPhys.right, rcPhys.bottom);
        HGDIOBJ oldBmp = SelectObject(hdc, hBmp);
        SetBkMode(hdc, TRANSPARENT);

        HFONT hTitleF = CasioTheme::CreateUiFont(17, true);
        HFONT hBtnF   = CasioTheme::CreateUiFont(12, true);
        HFONT hSmallF = CasioTheme::CreateUiFont(11, false);

        const float s = GetStudioUiScale(hWnd);
        {
            CasioTheme::UiScaleScope scaleScope(hdc, s);
            RECT rc = {
                0, 0,
                CasioTheme::DeviceToLogical(rcPhys.right, s),
                CasioTheme::DeviceToLogical(rcPhys.bottom, s)
            };

            HBRUSH brBg = CreateSolidBrush(CasioTheme::StudioCanvasBg());
            FillRect(hdc, &rc, brBg);
            DeleteObject(brBg);

            g_studioHitActions.clear();
            g_studioHeaderActions.clear();

        // Zona de contenido desplazable (se pinta primero, luego se fija la cabecera arriba)
        const auto& cat = CasioKnowledge::GetCatalog();
        const int left = 18;
        const int cardW = (std::max)(420, (int)rc.right - 36);
        int y = STUDIO_HEADER_H + 12 - g_studioScrollY;
        int totalBottom = STUDIO_HEADER_H + 12;

        auto pushAct = [&](RECT r, StudioHitAction::Kind k, int a1 = 0, int a2 = 0,
                           const std::vector<int>& seq = {}, const std::wstring& s1 = L"",
                           const std::wstring& s2 = L"", const std::wstring& s3 = L"") {
            if (r.bottom <= STUDIO_HEADER_H) return;
            r.top = (std::max)(static_cast<int>(r.top), STUDIO_HEADER_H);
            StudioHitAction act;
            act.rect = r; act.kind = k; act.arg1 = a1; act.arg2 = a2;
            act.seq = seq; act.str1 = s1; act.str2 = s2; act.str3 = s3;
            g_studioHitActions.push_back(std::move(act));
        };
        const int scrollClipDc = SaveDC(hdc);
        IntersectClipRect(hdc, 0, STUDIO_HEADER_H, rc.right, rc.bottom);

        auto drawBox = [&](RECT r, COLORREF fill, COLORREF bd) {
            if (r.bottom < STUDIO_HEADER_H || r.top > rc.bottom) return false;
            CasioTheme::FillRoundRectAA(hdc, r, 10.0f, fill, bd, 1.0f);
            return true;
        };

        if (g_studioTab == 0) {
            // TAB 0: TECLAS E INDICADORES (3 CAPAS)
            int targetModel = (g_studioModelFilter >= 0 && g_studioModelFilter <= 2)
                ? g_studioModelFilter
                : ((g_activeCalcIdx >= 0 && g_activeCalcIdx <= 2) ? g_activeCalcIdx : 0);

            // Subseccion A: Indicadores LCD del modelo (si aplica a CW / ES)
            if (targetModel == 0 || targetModel == 1) {
                RECT rcIndHdr = { left, y, left + cardW, y + 26 };
                if (drawBox(rcIndHdr, CasioTheme::StudioAccentSoft(), CasioTheme::StudioBorder())) {
                    SelectObject(hdc, hBtnF);
                    SetTextColor(hdc, CasioTheme::StudioAccentText());
                    std::wstring ih = std::wstring(L"Indicadores LCD Oficiales (") + (targetModel == 0 ? L"20 ClassWiz" : L"18 ES PLUS") + L") \u2014 Clic o clic derecho en LCD para inspeccionar";
                    RECT rTxt = { rcIndHdr.left + 10, rcIndHdr.top, rcIndHdr.right - 10, rcIndHdr.bottom };
                    DrawTextW(hdc, ih.c_str(), -1, &rTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                y += 32;

                const int cols = (cardW >= 820) ? 3 : 2;
                const int gapX = 8;
                const int iw = (cardW - (cols - 1) * gapX) / cols;
                int colIdx = 0;
                int rowH = 84;
                bool drewAnyInd = false;
                for (const auto& ind : cat.indicators) {
                    if (ind.calcIdx != targetModel) continue;
                    std::wstring corpus = ind.symbol + L" " + ind.title + L" " + ind.meaning + L" " + ind.whenAppears;
                    if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                    if (colIdx >= cols) { colIdx = 0; y += rowH + 8; }
                    const int ix = left + colIdx * (iw + gapX);
                    RECT rcI = { ix, y, (colIdx == cols - 1) ? (left + cardW) : (ix + iw), y + rowH };
                    drewAnyInd = true;
                    if (drawBox(rcI, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                        RECT rSym = { rcI.left + 8, rcI.top + 8, rcI.left + 64, rcI.top + 30 };
                        CasioTheme::FillRoundRectAA(hdc, rSym, 6.0f, CasioTheme::StudioAccentSoft());
                        DrawLatexRichTextCentered(hdc, ind.symbol, rSym, CasioTheme::StudioAccentText(), CasioTheme::StudioAccentText(), 13);
                        RECT rTit = { rcI.left + 72, rcI.top + 7, rcI.right - 8, rcI.top + 33 };
                        DrawLatexRichText(hdc, ind.title, rTit, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);
                        RECT rM = { rcI.left + 8, rcI.top + 38, rcI.right - 8, rcI.bottom - 6 };
                        DrawLatexRichText(hdc, ind.meaning, rM, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 11, false);
                        pushAct(rcI, StudioHitAction::ACT_OPEN_PDF, ind.pdfPage, 0, {}, ind.pdfFile);
                    }
                    ++colIdx;
                }
                if (drewAnyInd) y += rowH + 14;
            }

            // Subseccion B: 50 Teclas con 3 capas (Normal / SHIFT / ALPHA)
            for (const auto& k : cat.keys) {
                if (k.calcIdx != targetModel) continue;
                std::wstring corpus = k.keyName + L" " + k.normal.label + L" " + k.normal.category + L" " + k.normal.summary
                    + L" " + k.shift.label + L" " + k.shift.summary + L" " + k.alpha.label;
                if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                bool isFocused = (g_studioFocusKeyId == k.keyId);
                int kH = isFocused ? 156 : 114;
                RECT rcK = { left, y, left + cardW, y + kH };
                if (drawBox(rcK, isFocused ? CasioTheme::StudioAccentSoft() : CasioTheme::StudioSurface(),
                            isFocused ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder())) {
                    const auto& cap = KeyCaptureForHistory(hdc, k.calcIdx, k.keyId);
                    if (cap.bitmap && cap.width > 0 && cap.height > 0) {
                        HDC hk = CreateCompatibleDC(hdc);
                        HBITMAP ok = (HBITMAP)SelectObject(hk, cap.bitmap);
                        double sc = (std::min)(54.0 / cap.width, 54.0 / cap.height);
                        int dw = (std::max)(22, static_cast<int>(cap.width * sc));
                        int dh = (std::max)(20, static_cast<int>(cap.height * sc));
                        SetStretchBltMode(hdc, HALFTONE);
                        StretchBlt(hdc, rcK.left + 12, rcK.top + 12, dw, dh, hk, 0, 0, cap.width, cap.height, SRCCOPY);
                        SelectObject(hk, ok);
                        DeleteDC(hk);
                    }

                    int tx = rcK.left + 80;
                    RECT rcKT = { tx, rcK.top + 8, rcK.right - 340, rcK.top + 30 };
                    std::wstring hdr = k.normal.label + L"  [" + k.normal.category + L"]";
                    DrawLatexRichText(hdc, hdr, rcKT, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

                    // Capas inline: Normal | SHIFT | ALPHA
                    RECT rcL0 = { tx, rcK.top + 32, rcK.right - 14, rcK.top + 56 };
                    DrawLatexRichText(hdc, k.normal.summary + L"  (" + k.normal.latexFormula + L")",
                                      rcL0, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

                    std::wstring saLine;
                    if (k.shift.available) saLine += L"SHIFT: " + k.shift.label + L" (" + k.shift.latexFormula + L")   ";
                    if (k.alpha.available) saLine += L"ALPHA: " + k.alpha.label;
                    if (!saLine.empty()) {
                        RECT rcL1 = { tx, rcK.top + 58, rcK.right - 14, rcK.top + 82 };
                        DrawLatexRichText(hdc, saLine, rcL1, CasioTheme::StudioAmberText(), CasioTheme::StudioRoseText(), 12, true);
                    }

                    if (isFocused) {
                        RECT rcEx = { tx, rcK.top + 86, rcK.right - 14, rcK.top + 114 };
                        DrawLatexRichText(hdc, L"Ejemplo: " + k.normal.latexExample +
                            (k.normal.tipOrWarning.empty() ? L"" : L"   |   Nota: " + k.normal.tipOrWarning),
                            rcEx, CasioTheme::StudioSuccessText(), CasioTheme::StudioAccentText(), 13, true);
                    }

                    // Botones de accion derecha-arriba
                    int bx = rcK.right - 332;
                    RECT rTry = { bx, rcK.top + 8, bx + 72, rcK.top + 32 }; bx += 78;
                    RECT rExp = { bx, rcK.top + 8, bx + 78, rcK.top + 32 }; bx += 84;
                    RECT rPdf = { bx, rcK.top + 8, bx + 84, rcK.top + 32 }; bx += 90;
                    RECT rFav = { bx, rcK.top + 8, rcK.right - 10, rcK.top + 32 };

                    std::vector<int> demo = k.demoSeq.empty() ? std::vector<int>{ k.keyId } : k.demoSeq;
                    std::wstring fid = L"key_" + std::to_wstring(k.calcIdx) + L"_" + std::to_wstring(k.keyId);
                    bool fav = FindFavoriteIndex(L"key", fid) >= 0;

                    CasioTheme::DrawStudioButton(hdc, rTry, L"Probar", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                    CasioTheme::DrawStudioButton(hdc, rExp, L"+ Gu\u00EDa", hBtnF, CasioTheme::BTN_VIOLET, false, false, 6.0f);
                    CasioTheme::DrawStudioButton(hdc, rPdf, L"PDF p." + std::to_wstring(k.pdfPage), hBtnF, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
                    CasioTheme::DrawStudioButton(hdc, rFav, fav ? L"En Fav" : L"Fav", hBtnF,
                        fav ? CasioTheme::BTN_AMBER : CasioTheme::BTN_GHOST, fav, false, 6.0f);

                    pushAct(rTry, StudioHitAction::ACT_RUN_SEQ, k.calcIdx, 0, demo);
                    pushAct(rExp, StudioHitAction::ACT_EXPORT_SEQ, k.calcIdx, 0, demo, L"Tecla: " + k.normal.label, k.normal.summary);
                    pushAct(rPdf, StudioHitAction::ACT_OPEN_PDF, k.pdfPage, 0, {}, k.pdfFile);
                    pushAct(rFav, StudioHitAction::ACT_TOGGLE_FAV, k.calcIdx, k.pdfPage, demo, L"key", fid, k.normal.label);
                    pushAct({ rcK.left, rcK.top, rTry.left - 4, rcK.bottom }, StudioHitAction::ACT_FOCUS_KEY, k.calcIdx, k.keyId);
                }
                y += kH + 8;
            }
        } else if (g_studioTab == 1) {
            // TAB 1: PROCEDIMIENTOS PASO A PASO ("COMO HACER EN CASIO")
            const wchar_t* pCats[] = { L"Todas", L"B\u00E1sicas", L"\u00C1lgebra", L"C\u00E1lculo", L"Trigonometr\u00EDa", L"Complejos", L"Matrices", L"Configuraci\u00F3n", L"Graficaci\u00F3n" };
            int cx = left;
            for (int c = 0; c < 9; ++c) {
                int cw = static_cast<int>(wcslen(pCats[c])) * 8 + 20;
                if (cx + cw > left + cardW) { cx = left; y += 30; }
                RECT rcC = { cx, y, cx + cw, y + 25 };
                bool act = (g_studioProcCat == c);
                if (rcC.bottom >= STUDIO_HEADER_H && rcC.top <= rc.bottom) {
                    CasioTheme::FillRoundRectAA(hdc, rcC, 6.0f,
                        act ? CasioTheme::StudioAccentSoft() : CasioTheme::StudioTrack(),
                        act ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder(), 1.0f);
                    SelectObject(hdc, act ? hBtnF : hSmallF);
                    SetTextColor(hdc, act ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextSec());
                    DrawTextW(hdc, pCats[c], -1, &rcC, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    pushAct(rcC, StudioHitAction::ACT_SET_PROC_CAT, c);
                }
                cx += cw + 6;
            }
            y += 34;

            for (const auto& p : cat.procedures) {
                if (g_studioProcCat > 0 && p.category != pCats[g_studioProcCat]) continue;
                std::wstring corpus = p.title + L" " + p.category + L" " + p.goalSummary + L" " + p.tags + L" " + p.commonErrors;
                if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                // Determinar que modelos mostrar para este procedimiento
                std::vector<int> modelsToShow;
                if (g_studioModelFilter >= 0 && g_studioModelFilter <= 2) {
                    if (p.perModel[g_studioModelFilter].supported) modelsToShow.push_back(g_studioModelFilter);
                } else {
                    for (int m = 0; m < 3; ++m) if (p.perModel[m].supported) modelsToShow.push_back(m);
                }
                if (modelsToShow.empty()) continue;

                int cH = 110 + static_cast<int>(modelsToShow.size()) * 116;
                RECT rcP = { left, y, left + cardW, y + cH };
                if (drawBox(rcP, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT rBadge = { rcP.left + 12, rcP.top + 10, rcP.left + 150, rcP.top + 30 };
                    CasioTheme::FillRoundRectAA(hdc, rBadge, 6.0f, CasioTheme::StudioAccentSoft());
                    SelectObject(hdc, hBtnF);
                    SetTextColor(hdc, CasioTheme::StudioAccentText());
                    DrawTextW(hdc, p.category.c_str(), -1, &rBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    RECT rTitle = { rcP.left + 160, rcP.top + 8, rcP.right - 96, rcP.top + 32 };
                    DrawLatexRichText(hdc, p.title, rTitle, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 15, true);

                    bool isFav = FindFavoriteIndex(L"proc", p.id) >= 0;
                    RECT rFav = { rcP.right - 90, rcP.top + 9, rcP.right - 12, rcP.top + 31 };
                    CasioTheme::DrawStudioButton(hdc, rFav, isFav ? L"En Fav" : L"Favorito", hBtnF,
                        isFav ? CasioTheme::BTN_AMBER : CasioTheme::BTN_GHOST, isFav, false, 6.0f);
                    const auto& favSeq = p.perModel[modelsToShow[0]].keys;
                    pushAct(rFav, StudioHitAction::ACT_TOGGLE_FAV, modelsToShow[0], p.perModel[modelsToShow[0]].pdfPage,
                            favSeq, L"proc", p.id, p.title);

                    RECT rGoal = { rcP.left + 14, rcP.top + 36, rcP.right - 14, rcP.top + 78 };
                    DrawLatexRichText(hdc, p.goalSummary + L"   |   F\u00F3rmula: " + p.latexFormula,
                                      rGoal, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, false);

                    RECT rErr = { rcP.left + 14, rcP.top + 80, rcP.right - 14, rcP.top + 102 };
                    DrawLatexRichText(hdc, L"Cuidado: " + p.commonErrors, rErr, CasioTheme::StudioAmberText(), CasioTheme::StudioRoseText(), 12, true);

                    int my = rcP.top + 108;
                    const wchar_t* mNames[3] = { L"ClassWiz", L"ES PLUS", L"fx-CG50" };
                    for (int m : modelsToShow) {
                        const auto& vm = p.perModel[m];
                        RECT rcM = { rcP.left + 12, my, rcP.right - 12, my + 108 };
                        if (drawBox(rcM, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder())) {
                            RECT rMN = { rcM.left + 8, rcM.top + 6, rcM.left + 98, rcM.top + 26 };
                            CasioTheme::FillRoundRectAA(hdc, rMN, 5.0f, CasioTheme::StudioTrack());
                            SelectObject(hdc, hBtnF);
                            SetTextColor(hdc, CasioTheme::StudioText());
                            DrawTextW(hdc, mNames[m], -1, &rMN, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                            RECT rSteps = { rcM.left + 106, rcM.top + 5, rcM.right - 290, rcM.top + 28 };
                            DrawLatexRichText(hdc, vm.stepsSummary + L"   -> LCD: " + vm.expectedLcd,
                                              rSteps, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, true);

                            RECT rTry = { rcM.right - 282, rcM.top + 5, rcM.right - 188, rcM.top + 28 };
                            RECT rExp = { rcM.right - 182, rcM.top + 5, rcM.right - 92, rcM.top + 28 };
                            RECT rPdf = { rcM.right - 86,  rcM.top + 5, rcM.right - 8, rcM.top + 28 };

                            CasioTheme::DrawStudioButton(hdc, rTry, L"Probar", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                            CasioTheme::DrawStudioButton(hdc, rExp, L"Gu\u00EDa PNG", hBtnF, CasioTheme::BTN_VIOLET, false, false, 6.0f);
                            CasioTheme::DrawStudioButton(hdc, rPdf, L"PDF p." + std::to_wstring(vm.pdfPage), hBtnF, CasioTheme::BTN_SECONDARY, false, false, 6.0f);

                            pushAct(rTry, StudioHitAction::ACT_RUN_SEQ, m, 0, vm.keys);
                            pushAct(rExp, StudioHitAction::ACT_EXPORT_SEQ, m, 0, vm.keys, p.title + L" (" + mNames[m] + L")",
                                    p.goalSummary + L"\r\nPasos: " + vm.stepsSummary + L"\r\nResultado LCD: " + vm.expectedLcd);
                            pushAct(rPdf, StudioHitAction::ACT_OPEN_PDF, vm.pdfPage, 0, {}, vm.pdfFile);

                            if (!vm.keys.empty()) {
                                RECT rStrip = { rcM.left + 8, rcM.top + 33, rcM.right - 8, rcM.bottom - 6 };
                                DrawSmartHelpButtonSequenceStrip(hdc, m, vm.keys, rStrip);
                            }
                        }
                        my += 116;
                    }
                }
                y += cH + 10;
            }
        } else if (g_studioTab == 2) {
            // TAB 2: COMPARADOR CW / ES PLUS / FX-CG50
            int c1 = cardW * 18 / 100;
            int c2 = cardW * 24 / 100;
            int c3 = cardW * 24 / 100;
            RECT rH = { left, y, left + cardW, y + 34 };
            if (drawBox(rH, CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorderStrong())) {
                SelectObject(hdc, hBtnF);
                SetTextColor(hdc, CasioTheme::StudioText());
                RECT f0{ left + 8, y, left + c1, y + 34 };
                RECT f1{ left + c1, y, left + c1 + c2, y + 34 };
                RECT f2{ left + c1 + c2, y, left + c1 + c2 + c3, y + 34 };
                RECT f3{ left + c1 + c2 + c3, y, left + cardW - 8, y + 34 };
                DrawTextW(hdc, L"Caracter\u00EDstica", -1, &f0, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextW(hdc, L"Blanca ClassWiz", -1, &f1, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextW(hdc, L"Gris fx-570ES PLUS", -1, &f2, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                DrawTextW(hdc, L"fx-CG50 PRIZM", -1, &f3, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            y += 40;

            for (const auto& row : cat.comparisons) {
                std::wstring corpus = row.feature + L" " + row.category + L" " + row.cwBehavior + L" " + row.esBehavior + L" " + row.cgBehavior + L" " + row.keyDifferenceNote;
                if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                int rHgt = 112;
                RECT rcR = { left, y, left + cardW, y + rHgt };
                if (drawBox(rcR, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT f0{ left + 10, y + 8, left + c1 - 6, y + rHgt - 34 };
                    RECT f1{ left + c1, y + 8, left + c1 + c2 - 8, y + rHgt - 34 };
                    RECT f2{ left + c1 + c2, y + 8, left + c1 + c2 + c3 - 8, y + rHgt - 34 };
                    RECT f3{ left + c1 + c2 + c3, y + 8, left + cardW - 10, y + rHgt - 34 };
                    DrawLatexRichText(hdc, row.feature + L"\n[" + row.category + L"]", f0, CasioTheme::StudioAccentText(), CasioTheme::StudioAmberText(), 13, false);
                    DrawLatexRichText(hdc, row.cwBehavior, f1, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);
                    DrawLatexRichText(hdc, row.esBehavior, f2, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);
                    DrawLatexRichText(hdc, row.cgBehavior, f3, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);

                    RECT rNote = { left + 10, y + rHgt - 28, left + cardW - 210, y + rHgt - 4 };
                    DrawLatexRichText(hdc, L"Nota clave: " + row.keyDifferenceNote, rNote, CasioTheme::StudioAmberText(), CasioTheme::StudioAccentText(), 12, true);

                    if (!row.cwDemo.empty()) {
                        RECT bCW = { left + cardW - 202, y + rHgt - 29, left + cardW - 106, y + rHgt - 5 };
                        CasioTheme::DrawStudioButton(hdc, bCW, L"Probar CW", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                        pushAct(bCW, StudioHitAction::ACT_RUN_SEQ, 0, 0, row.cwDemo);
                    }
                    if (!row.esDemo.empty()) {
                        RECT bES = { left + cardW - 100, y + rHgt - 29, left + cardW - 8, y + rHgt - 5 };
                        CasioTheme::DrawStudioButton(hdc, bES, L"Probar ES", hBtnF, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
                        pushAct(bES, StudioHitAction::ACT_RUN_SEQ, 1, 0, row.esDemo);
                    }
                }
                y += rHgt + 8;
            }
        } else if (g_studioTab == 3) {
            // TAB 3: POR QUE MUESTRA ESTO? + ERRORES OFICIALES
            RECT rSec1 = { left, y, left + cardW, y + 26 };
            if (drawBox(rSec1, CasioTheme::StudioAccentSoft(), CasioTheme::StudioBorder())) {
                SelectObject(hdc, hBtnF);
                SetTextColor(hdc, CasioTheme::StudioAccentText());
                RECT rt = { left + 10, y, left + cardW - 10, y + 26 };
                DrawTextW(hdc, L"Diagn\u00F3stico de Pantalla: \u00BFPor qu\u00E9 muestra esto?", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            y += 32;

            for (const auto& d : cat.diagnostics) {
                std::wstring corpus = d.symptomTitle + L" " + d.category + L" " + d.whyItHappens + L" " + d.howToFix;
                if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                int dH = 160;
                RECT rcD = { left, y, left + cardW, y + dH };
                if (drawBox(rcD, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT rB = { rcD.left + 12, rcD.top + 8, rcD.left + 252, rcD.top + 28 };
                    CasioTheme::FillRoundRectAA(hdc, rB, 5.0f, CasioTheme::StudioAmberSoft());
                    SelectObject(hdc, hBtnF);
                    SetTextColor(hdc, CasioTheme::StudioAmberText());
                    DrawTextW(hdc, d.category.c_str(), -1, &rB, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    RECT rT = { rcD.left + 262, rcD.top + 6, rcD.right - 165, rcD.top + 30 };
                    DrawLatexRichText(hdc, d.symptomTitle, rT, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 14, true);

                    if (d.quickFixCalcIdx >= 0 && !d.quickFixSeq.empty()) {
                        RECT rFix = { rcD.right - 156, rcD.top + 7, rcD.right - 10, rcD.top + 31 };
                        CasioTheme::DrawStudioButton(hdc, rFix, L"Aplicar Soluci\u00F3n", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                        pushAct(rFix, StudioHitAction::ACT_RUN_SEQ, d.quickFixCalcIdx, 0, d.quickFixSeq);
                    }

                    RECT rW = { rcD.left + 14, rcD.top + 34, rcD.right - 14, rcD.top + 96 };
                    DrawLatexRichText(hdc, L"Causa: " + d.whyItHappens + L"\nExpresi\u00F3n: " + d.latexExplanation,
                                      rW, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, false);

                    RECT rH2 = { rcD.left + 14, rcD.top + 98, rcD.right - 14, rcD.top + 136 };
                    DrawLatexRichText(hdc, L"C\u00F3mo solucionarlo: " + d.howToFix,
                                      rH2, CasioTheme::StudioSuccessText(), CasioTheme::StudioAmberText(), 13, false);

                    RECT rRef = { rcD.left + 14, rcD.bottom - 22, rcD.right - 14, rcD.bottom - 4 };
                    SelectObject(hdc, hSmallF);
                    SetTextColor(hdc, CasioTheme::StudioTextMuted());
                    std::wstring refs = L"Referencias: " + d.pdfRefCW + L"  |  " + d.pdfRefES;
                    DrawTextW(hdc, refs.c_str(), -1, &rRef, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                y += dH + 8;
            }

            y += 6;
            RECT rSec2 = { left, y, left + cardW, y + 26 };
            if (drawBox(rSec2, CasioTheme::StudioRoseSoft(), CasioTheme::StudioBorder())) {
                SelectObject(hdc, hBtnF);
                SetTextColor(hdc, CasioTheme::StudioRoseText());
                RECT rt = { left + 10, y, left + cardW - 10, y + 26 };
                DrawTextW(hdc, L"Mensajes de Error Oficiales CASIO", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            y += 32;

            for (const auto& er : cat.errors) {
                std::wstring corpus = er.errorName + L" " + er.appliesTo + L" " + er.cause + L" " + er.resolution;
                if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                RECT rcE = { left, y, left + cardW, y + 92 };
                if (drawBox(rcE, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT rName = { rcE.left + 12, rcE.top + 8, rcE.left + 165, rcE.top + 30 };
                    CasioTheme::FillRoundRectAA(hdc, rName, 5.0f, CasioTheme::StudioRoseSoft());
                    SelectObject(hdc, hBtnF);
                    SetTextColor(hdc, CasioTheme::StudioRoseText());
                    DrawTextW(hdc, er.errorName.c_str(), -1, &rName, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    RECT rApp = { rcE.left + 174, rcE.top + 8, rcE.right - 110, rcE.top + 30 };
                    SelectObject(hdc, hBtnF);
                    SetTextColor(hdc, CasioTheme::StudioTextSec());
                    DrawTextW(hdc, er.appliesTo.c_str(), -1, &rApp, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    RECT rPdf = { rcE.right - 102, rcE.top + 8, rcE.right - 10, rcE.top + 30 };
                    CasioTheme::DrawStudioButton(hdc, rPdf, L"PDF p." + std::to_wstring(er.pdfPage), hBtnF, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
                    pushAct(rPdf, StudioHitAction::ACT_OPEN_PDF, er.pdfPage, 0, {}, er.pdfFile);

                    RECT rBody = { rcE.left + 12, rcE.top + 34, rcE.right - 12, rcE.bottom - 6 };
                    DrawLatexRichText(hdc, L"Causa: " + er.cause + L"\nSoluci\u00F3n: " + er.resolution,
                                      rBody, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, false);
                }
                y += 98;
            }
        } else if (g_studioTab == 4) {
            // TAB 4: TRUCOS, PERFILES, CHECKLIST Y FAVORITOS
            EnsureFavoritesAndChecklistLoaded();

            // Favoritos
            RECT rFavHdr = { left, y, left + cardW, y + 26 };
            if (drawBox(rFavHdr, CasioTheme::StudioAmberSoft(), CasioTheme::StudioBorder())) {
                SelectObject(hdc, hBtnF);
                SetTextColor(hdc, CasioTheme::StudioAmberText());
                RECT rt = { left + 10, y, left + cardW - 10, y + 26 };
                DrawTextW(hdc, (L"Favoritos Guardados (" + std::to_wstring(g_studioFavorites.size()) + L" elementos)").c_str(), -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            y += 32;
            for (const auto& f : g_studioFavorites) {
                RECT rF = { left, y, left + cardW, y + 40 };
                if (drawBox(rF, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT rt = { rF.left + 12, rF.top + 4, rF.right - 265, rF.bottom - 4 };
                    DrawLatexRichText(hdc, f.title + L"  [" + f.subtitle + L"]", rt, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, true);

                    RECT bRun = { rF.right - 256, rF.top + 7, rF.right - 178, rF.bottom - 7 };
                    RECT bPdf = { rF.right - 172, rF.top + 7, rF.right - 92, rF.bottom - 7 };
                    RECT bDel = { rF.right - 86,  rF.top + 7, rF.right - 10, rF.bottom - 7 };
                    CasioTheme::DrawStudioButton(hdc, bRun, L"Probar", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                    CasioTheme::DrawStudioButton(hdc, bPdf, L"PDF p." + std::to_wstring(f.pdfPage), hBtnF, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
                    CasioTheme::DrawStudioButton(hdc, bDel, L"Quitar", hBtnF, CasioTheme::BTN_DANGER, false, false, 6.0f);
                    pushAct(bRun, StudioHitAction::ACT_RUN_SEQ, f.calcIdx, 0, f.demoSeq);
                    pushAct(bPdf, StudioHitAction::ACT_OPEN_PDF, f.pdfPage, 0, {}, f.pdfFile);
                    pushAct(bDel, StudioHitAction::ACT_TOGGLE_FAV, f.calcIdx, f.pdfPage, f.demoSeq, f.kind, f.id, f.title);
                }
                y += 45;
            }
            y += 6;

            // Perfiles recomendados de configuracion
            RECT rProfHdr = { left, y, left + cardW, y + 26 };
            if (drawBox(rProfHdr, CasioTheme::StudioAccentSoft(), CasioTheme::StudioBorder())) {
                SelectObject(hdc, hBtnF);
                SetTextColor(hdc, CasioTheme::StudioAccentText());
                RECT rt = { left + 10, y, left + cardW - 10, y + 26 };
                DrawTextW(hdc, L"Perfiles Recomendados de Configuraci\u00F3n (Aplicar con un clic)", -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            y += 32;
            for (const auto& pr : cat.setupProfiles) {
                RECT rcPr = { left, y, left + cardW, y + 72 };
                if (drawBox(rcPr, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT rt = { rcPr.left + 12, rcPr.top + 6, rcPr.right - 220, rcPr.top + 26 };
                    DrawLatexRichText(hdc, pr.title, rt, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);
                    RECT rd = { rcPr.left + 12, rcPr.top + 28, rcPr.right - 12, rcPr.bottom - 6 };
                    DrawLatexRichText(hdc, pr.description + L"  [" + pr.settingsSummary + L"]", rd, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, false);
                    if (!pr.cwSeq.empty()) {
                        RECT bCW = { rcPr.right - 210, rcPr.top + 7, rcPr.right - 110, rcPr.top + 30 };
                        CasioTheme::DrawStudioButton(hdc, bCW, L"Aplicar CW", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                        pushAct(bCW, StudioHitAction::ACT_RUN_SEQ, 0, 0, pr.cwSeq);
                    }
                    if (!pr.esSeq.empty()) {
                        RECT bES = { rcPr.right - 104, rcPr.top + 7, rcPr.right - 10, rcPr.top + 30 };
                        CasioTheme::DrawStudioButton(hdc, bES, L"Aplicar ES", hBtnF, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
                        pushAct(bES, StudioHitAction::ACT_RUN_SEQ, 1, 0, pr.esSeq);
                    }
                }
                y += 78;
            }
            y += 6;

            // Checklist de Comprobacion Previa (8 items)
            int doneCnt = 0;
            for (bool b : g_studioChecklistDone) if (b) doneCnt++;
            RECT rChkHdr = { left, y, left + cardW, y + 26 };
            if (drawBox(rChkHdr, CasioTheme::StudioSuccessSoft(), CasioTheme::StudioBorder())) {
                SelectObject(hdc, hBtnF);
                SetTextColor(hdc, CasioTheme::StudioSuccessText());
                RECT rt = { left + 10, y, left + cardW - 10, y + 26 };
                std::wstring cl = L"Comprobaci\u00F3n Previa de Calculadora (" + std::to_wstring(doneCnt) + L" / 8 verificados)";
                DrawTextW(hdc, cl.c_str(), -1, &rt, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }
            y += 32;
            for (size_t i = 0; i < cat.checklist.size() && i < 8; ++i) {
                const auto& ch = cat.checklist[i];
                bool ok = g_studioChecklistDone[i];
                RECT rcC = { left, y, left + cardW, y + 38 };
                if (drawBox(rcC, ok ? CasioTheme::StudioSuccessSoft() : CasioTheme::StudioSurface(),
                            ok ? CasioTheme::StudioSuccess() : CasioTheme::StudioBorder())) {
                    RECT chkBox = { rcC.left + 12, rcC.top + 9, rcC.left + 32, rcC.top + 29 };
                    CasioTheme::FillRoundRectAA(hdc, chkBox, 5.0f,
                        ok ? CasioTheme::StudioSuccess() : CasioTheme::StudioInputBg(),
                        ok ? CasioTheme::StudioSuccessText() : CasioTheme::StudioBorderStrong(), 1.2f);
                    if (ok && EnsureGdiplusInitialized()) {
                        Gdiplus::Graphics gc(hdc);
                        gc.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                        Gdiplus::Pen penCk(Gdiplus::Color(255, 255, 255, 255), 2.0f);
                        Gdiplus::PointF pts[3] = {
                            { chkBox.left + 4.5f, chkBox.top + 10.0f },
                            { chkBox.left + 8.5f, chkBox.top + 14.0f },
                            { chkBox.left + 15.5f, chkBox.top + 5.5f }
                        };
                        gc.DrawLines(&penCk, pts, 3);
                    }
                    std::wstring txt = ch.title + L"  \u2014  " + ch.detail;
                    RECT rt = { rcC.left + 42, rcC.top + 2, rcC.right - 144, rcC.bottom - 2 };
                    DrawLatexRichText(hdc, txt, rt, ok ? CasioTheme::StudioSuccessText() : CasioTheme::StudioText(),
                                      CasioTheme::StudioAccentText(), 12, true);
                    RECT ri = { rcC.right - 136, rcC.top + 7, rcC.right - 10, rcC.bottom - 7 };
                    CasioTheme::FillRoundRectAA(hdc, ri, 5.0f, CasioTheme::StudioTrack());
                    DrawLatexRichTextCentered(hdc, ch.indicatorHint, ri, CasioTheme::StudioAccentText(), CasioTheme::StudioAccentText(), 12);
                    pushAct(rcC, StudioHitAction::ACT_TOGGLE_CHECK, static_cast<int>(i));
                }
                y += 42;
            }
            y += 8;

            // Trucos, Advertencias y Cuidados (16 tarjetas)
            for (const auto& tip : cat.tips) {
                std::wstring corpus = tip.kind + L" " + tip.title + L" " + tip.summary + L" " + tip.latexDetail;
                if (!g_studioSearchQuery.empty() && CasioKnowledge::ScoreKnowledgeMatch(corpus, g_studioSearchQuery) == 0) continue;

                RECT rcTip = { left, y, left + cardW, y + 92 };
                const bool isTrick = (tip.kind == L"Truco");
                const bool isWarn = (tip.kind == L"Cuidado");
                const COLORREF tagBg = isTrick ? CasioTheme::StudioSuccessSoft() : (isWarn ? CasioTheme::StudioAmberSoft() : CasioTheme::StudioRoseSoft());
                const COLORREF tagFg = isTrick ? CasioTheme::StudioSuccessText() : (isWarn ? CasioTheme::StudioAmberText() : CasioTheme::StudioRoseText());
                if (drawBox(rcTip, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    RECT rK = { rcTip.left + 12, rcTip.top + 8, rcTip.left + 98, rcTip.top + 28 };
                    CasioTheme::FillRoundRectAA(hdc, rK, 5.0f, tagBg);
                    SelectObject(hdc, hBtnF); SetTextColor(hdc, tagFg);
                    DrawTextW(hdc, tip.kind.c_str(), -1, &rK, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    RECT rT = { rcTip.left + 106, rcTip.top + 8, rcTip.right - 165, rcTip.top + 28 };
                    DrawLatexRichText(hdc, tip.title, rT, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);
                    if (!tip.demoSeq.empty()) {
                        RECT rTry = { rcTip.right - 156, rcTip.top + 7, rcTip.right - 82, rcTip.top + 29 };
                        RECT rExp = { rcTip.right - 76,  rcTip.top + 7, rcTip.right - 10, rcTip.top + 29 };
                        CasioTheme::DrawStudioButton(hdc, rTry, L"Probar", hBtnF, CasioTheme::BTN_SUCCESS, false, false, 6.0f);
                        CasioTheme::DrawStudioButton(hdc, rExp, L"+ Gu\u00EDa", hBtnF, CasioTheme::BTN_VIOLET, false, false, 6.0f);
                        pushAct(rTry, StudioHitAction::ACT_RUN_SEQ, tip.calcIdx, 0, tip.demoSeq);
                        pushAct(rExp, StudioHitAction::ACT_EXPORT_SEQ, tip.calcIdx, 0, tip.demoSeq, tip.title, tip.summary);
                    }
                    RECT rB = { rcTip.left + 12, rcTip.top + 33, rcTip.right - 12, rcTip.bottom - 6 };
                    DrawLatexRichText(hdc, tip.summary + L"   (" + tip.latexDetail + L")", rB, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, false);
                }
                y += 98;
            }
        } else {
            // TAB 5: MANUALES OFICIALES PDF (16) + INDICE REAL EN REJILLA DE 2 COLUMNAS
            RECT rTopBtns = { left, y, left + cardW, y + 40 };
            if (drawBox(rTopBtns, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                RECT b1 = { left + 10, y + 6, left + 236, y + 34 };
                RECT b2 = { left + 246, y + 6, left + 528, y + 34 };
                CasioTheme::DrawStudioButton(hdc, b1, L"Abrir Carpeta manuales/", hBtnF, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
                CasioTheme::DrawStudioButton(hdc, b2, L"Abrir Ayuda Smart PDF (F1)", hBtnF, CasioTheme::BTN_VIOLET, false, false, 6.0f);
                pushAct(b1, StudioHitAction::ACT_OPEN_MANUAL_FOLDER);
                pushAct(b2, StudioHitAction::ACT_OPEN_SMART_HELP);
            }
            y += 48;

            const int cols = (cardW >= 780) ? 2 : 1;
            const int colGap = 10;
            const int colW = (cols == 2) ? (cardW - colGap) / 2 : cardW;
            const int mCardH = 74;
            int colIdx = 0;

            for (const auto& m : g_officialManuals) {
                if (g_studioModelFilter >= 0) {
                    int wantBit = 1 << g_studioModelFilter;
                    if ((m.calcMask & wantBit) == 0) continue;
                }
                int mx = left + colIdx * (colW + colGap);
                RECT rcM = { mx, y, mx + colW, y + mCardH };
                if (drawBox(rcM, CasioTheme::StudioSurface(), CasioTheme::StudioBorder())) {
                    const bool isEs = wcsstr(m.fileName, L"_ES") || wcsstr(m.fullTitle, L"(Espa");
                    RECT rDot = { rcM.left + 12, rcM.top + 15, rcM.left + 19, rcM.top + 22 };
                    CasioTheme::FillRoundRectAA(hdc, rDot, 3.5f, isEs ? CasioTheme::StudioSuccess() : CasioTheme::StudioAccent());
                    RECT rMeta = { rcM.left + 25, rcM.top + 9, rcM.right - 112, rcM.top + 27 };
                    SelectObject(hdc, hSmallF);
                    SetTextColor(hdc, isEs ? CasioTheme::StudioSuccessText() : CasioTheme::StudioAccentText());
                    std::wstring metaLine = std::wstring(isEs ? L"ES \u00B7 " : L"EN \u00B7 ") + m.manualKind;
                    DrawTextW(hdc, metaLine.c_str(), -1, &rMeta, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    RECT rt = { rcM.left + 12, rcM.top + 30, rcM.right - 112, rcM.bottom - 8 };
                    SelectObject(hdc, hBtnF); SetTextColor(hdc, CasioTheme::StudioText());
                    DrawTextW(hdc, m.fullTitle, -1, &rt, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);

                    RECT rO = { rcM.right - 102, rcM.top + 21, rcM.right - 10, rcM.top + 51 };
                    CasioTheme::DrawStudioButton(hdc, rO, L"Abrir PDF", hBtnF, CasioTheme::BTN_PRIMARY, false, false, 6.0f);
                    pushAct(rO, StudioHitAction::ACT_OPEN_PDF, 1, 0, {}, m.fileName);
                }
                if (++colIdx >= cols) {
                    colIdx = 0;
                    y += mCardH + 8;
                }
            }
            if (colIdx > 0) y += mCardH + 8;

            // Coincidencias reales del indice PDF (TSV) cuando hay busqueda activa
            if (!g_studioSearchQuery.empty()) {
                y += 8;
                int shownIdx = 0;
                for (const auto& pg : g_indexedPdfPages) {
                    if (CasioKnowledge::ScoreKnowledgeMatch(pg.heading + L" " + pg.snippet, g_studioSearchQuery) == 0) continue;
                    RECT rcPg = { left, y, left + cardW, y + 56 };
                    if (drawBox(rcPg, CasioTheme::StudioSurface(), CasioTheme::StudioBorderStrong())) {
                        RECT rt = { rcPg.left + 12, rcPg.top + 6, rcPg.right - 120, rcPg.top + 27 };
                        DrawLatexRichText(hdc, pg.fileName + L" (p. " + std::to_wstring(pg.pageNo) + L") \u2014 " + pg.heading,
                                          rt, CasioTheme::StudioAccentText(), CasioTheme::StudioText(), 12, true);
                        RECT rs = { rcPg.left + 12, rcPg.top + 28, rcPg.right - 120, rcPg.bottom - 6 };
                        DrawLatexRichText(hdc, pg.snippet, rs, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 12, true);
                        RECT rO = { rcPg.right - 110, rcPg.top + 12, rcPg.right - 10, rcPg.bottom - 12 };
                        CasioTheme::DrawStudioButton(hdc, rO, L"Ir p." + std::to_wstring(pg.pageNo), hBtnF, CasioTheme::BTN_SECONDARY, false, false, 6.0f);
                        pushAct(rO, StudioHitAction::ACT_OPEN_PDF, pg.pageNo, 0, {}, pg.fileName);
                    }
                    y += 62;
                    if (++shownIdx >= 25) break;
                }
            }
        }

        totalBottom = y + g_studioScrollY + 18;
        RestoreDC(hdc, scrollClipDc);

        // Actualizar Scrollbar
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = (std::max)(0, totalBottom - STUDIO_HEADER_H);
        si.nPage = (std::max)(1, static_cast<int>(rc.bottom) - STUDIO_HEADER_H);
        int maxS = (std::max)(0, si.nMax - (int)si.nPage + 1);
        g_studioScrollY = (std::max)(0, (std::min)(g_studioScrollY, maxS));
        si.nPos = g_studioScrollY;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);

        // CABECERA FIJA SUPERIOR material translúcido 2.0
        RECT rcHdr = { 0, 0, rc.right, STUDIO_HEADER_H };
        CasioTheme::DrawTranslucentHeaderStrip(hdc, rcHdr, true);

        SelectObject(hdc, hTitleF);
        SetTextColor(hdc, CasioTheme::StudioText());
        RECT rcTopT = { 18, 10, 420, 38 };
        DrawTextW(hdc, L"Centro de Estudio CASIO", -1, &rcTopT, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Chrome redondeado alrededor del EDIT nativo de busqueda
        RECT rSearchEditRc = { 24, 54, 24 + (std::max)(240, (int)rc.right - 432), 74 };
        CasioTheme::DrawInputChrome(hdc, rSearchEditRc, (GetFocus() == g_hStudioSearchEdit));

        if (!g_studioSearchQuery.empty()) {
            RECT rClr = { rc.right - 108, 11, rc.right - 18, 37 };
            CasioTheme::DrawStudioButton(hdc, rClr, L"Limpiar", hBtnF, CasioTheme::BTN_DANGER, false, false, 12.0f);
            StudioHitAction act; act.rect = rClr; act.kind = StudioHitAction::ACT_CLEAR_SEARCH;
            g_studioHeaderActions.push_back(act);
        }

        // Selector segmentado flotante material translúcido 2.0 de modelo a la derecha del cuadro de busqueda
        const wchar_t* mLabels[4] = { L"Todos", L"ClassWiz", L"ES PLUS", L"fx-CG50" };
        int mStartX = (std::max)(290, (int)rc.right - 390);
        RECT rSegM = { mStartX - 4, 47, mStartX + 4 * 92 + 2, 81 };
        CasioTheme::DrawFloatingGlassCapsule(hdc, rSegM, s, -1.0f, false);
        for (int i = 0; i < 4; ++i) {
            int mf = i - 1;
            RECT rm = { mStartX + i * 92, 50, mStartX + i * 92 + 88, 78 };
            bool sel = (g_studioModelFilter == mf);
            CasioTheme::DrawStudioSegmentItem(hdc, rm, mLabels[i], sel ? hBtnF : hSmallF, sel, false, sel);
            StudioHitAction act; act.rect = rm; act.kind = StudioHitAction::ACT_SET_MODEL; act.arg1 = mf;
            g_studioHeaderActions.push_back(act);
        }

        // 6 Pestanas principales en capsula flotante material translúcido 2.0
        const std::wstring manualsTabLabel =
            L"Manuales PDF (" + std::to_wstring(sizeof(g_officialManuals) / sizeof(g_officialManuals[0])) + L")";
        const wchar_t* tabLabels[6] = {
            L"Teclas e Indicadores",
            L"C\u00F3mo hacer (Gu\u00EDas)",
            L"Comparador Modelos",
            L"\u00BFPor qu\u00E9 muestra esto?",
            L"Trucos, Config y Favs",
            manualsTabLabel.c_str()
        };
        int tabAvail = (std::max)(600, (int)rc.right - 36);
        RECT rTabTrack = { 15, 91, 21 + tabAvail, 135 };
        CasioTheme::DrawFloatingGlassCapsule(hdc, rTabTrack, s, -1.0f, false);
        int tabW = (tabAvail - 5 * 6) / 6;
        int tx = 18;
        for (int t = 0; t < 6; ++t) {
            g_studioTabRects[t] = { tx, 94, tx + tabW, 132 };
            bool active = (g_studioTab == t);
            CasioTheme::DrawStudioSegmentItem(hdc, g_studioTabRects[t], tabLabels[t], active ? hBtnF : hSmallF, active, false, true);
            tx += tabW + 6;
        }

        DeleteObject(hTitleF);
        DeleteObject(hBtnF);
        DeleteObject(hSmallF);
        } // UiScaleScope

        BitBlt(hdcWin, 0, 0, rcPhys.right, rcPhys.bottom, hdc, 0, 0, SRCCOPY);
        SelectObject(hdc, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        g_hStudioSearchEdit = nullptr;
        g_hStudioWnd = nullptr;
        if (g_hStudioSearchFont) { DeleteObject(g_hStudioSearchFont); g_hStudioSearchFont = nullptr; }
        g_studioSearchFontPx = 0;
        return 0;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

inline void OpenLearningStudioWindow(int initialTab, int calcIdx, int focusKeyId, const std::wstring& searchQuery) {
    static float s_lastStudioScale = 0.0f;
    if (initialTab >= 0 && initialTab < 6) g_studioTab = initialTab;
    if (calcIdx >= -1 && calcIdx <= 2) g_studioModelFilter = calcIdx;
    if (focusKeyId >= 0) g_studioFocusKeyId = focusKeyId;
    if (!searchQuery.empty()) g_studioSearchQuery = searchQuery;
    g_studioScrollY = 0;

    if (!g_hStudioWnd || !IsWindow(g_hStudioWnd)) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = LearningStudioWndProc;
        wc.hInstance = g_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = CasioTheme::GetStudioSurfaceBrush();
        wc.lpszClassName = L"CasioLearningStudioWnd";
        RegisterClassExW(&wc);

        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        mi.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        HMONITOR hMon = (g_hMainWnd && IsWindow(g_hMainWnd))
            ? MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST)
            : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
        if (hMon) GetMonitorInfoW(hMon, &mi);
        const int workW = mi.rcWork.right - mi.rcWork.left;
        const int workH = mi.rcWork.bottom - mi.rcWork.top;
        const float dlgScale = GetStudioUiScale(g_hMainWnd);
        const int w = (std::min)(CasioTheme::UiToDevice(1100, dlgScale), (std::max)(640, workW - 32));
        const int h = (std::min)(CasioTheme::UiToDevice(760, dlgScale), (std::max)(480, workH - 32));
        const int leftX = mi.rcWork.left + (std::max)(16, (workW - w) / 2);
        const int topY  = mi.rcWork.top  + (std::max)(16, (workH - h) / 2);

        g_hStudioWnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            L"CasioLearningStudioWnd",
            L"Centro de Estudio y Manuales CASIO",
            WS_OVERLAPPEDWINDOW | WS_VSCROLL,
            leftX, topY, w, h,
            g_hMainWnd, nullptr, g_hInst, nullptr);
        s_lastStudioScale = GetStudioUiScale(g_hStudioWnd);
        SyncStudioSearchEdit(g_hStudioWnd);
        CasioTheme::ApplyDwmDarkTitleBar(g_hStudioWnd, CasioTheme::g_studioDarkTheme);
        CasioTheme::EnableThemeScrollbar(g_hStudioWnd, CasioTheme::g_studioDarkTheme);
    } else {
        const float activeScale = GetStudioUiScale(g_hStudioWnd);
        if (s_lastStudioScale - activeScale > 0.01f || activeScale - s_lastStudioScale > 0.01f) {
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            mi.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            HMONITOR hMon = MonitorFromWindow(g_hStudioWnd, MONITOR_DEFAULTTONEAREST);
            if (hMon) GetMonitorInfoW(hMon, &mi);
            const int workW = mi.rcWork.right - mi.rcWork.left;
            const int workH = mi.rcWork.bottom - mi.rcWork.top;
            const int nw = (std::min)(CasioTheme::UiToDevice(1100, activeScale), (std::max)(640, workW - 32));
            const int nh = (std::min)(CasioTheme::UiToDevice(760, activeScale), (std::max)(480, workH - 32));
            SetWindowPos(g_hStudioWnd, nullptr, 0, 0, nw, nh, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            s_lastStudioScale = activeScale;
        }
        SyncStudioSearchEdit(g_hStudioWnd);
        CasioTheme::ApplyDwmDarkTitleBar(g_hStudioWnd, CasioTheme::g_studioDarkTheme);
        CasioTheme::EnableThemeScrollbar(g_hStudioWnd, CasioTheme::g_studioDarkTheme);
    }

    if (g_hStudioSearchEdit && !searchQuery.empty()) {
        SetWindowTextW(g_hStudioSearchEdit, g_studioSearchQuery.c_str());
    }
    ShowWindow(g_hStudioWnd, SW_SHOWNORMAL);
    SetForegroundWindow(g_hStudioWnd);
    InvalidateRect(g_hStudioWnd, nullptr, TRUE);
}
