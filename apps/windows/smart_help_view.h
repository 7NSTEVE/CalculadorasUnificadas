#pragma once
// ============================================================================
// smart_help_view.h
// Ayuda Inteligente con:
//   1) 14 Manuales Oficiales PDF reales (Hardware y Software, ES/EN) +
//      indice real de 6 columnas (manuales\indice_paginas_pdf.tsv)
//   2) Renderizado visual nativo LaTeX 2D (DrawLatexRichText)
//   3) Tiras visuales de botones reales recortados del skin (KeyCaptureForHistory)
//   4) Botones por tarjeta: [Probar Pasos], [Exportar Gu\u00EDa], [PDF p.N]
// ============================================================================

#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cwctype>
#include "ui_theme.h"
#include "manual_catalog.h"
#include "latex_math_renderer.h"
#include "pdf_viewer.h"

static HWND g_hSmartHelpWnd = nullptr;
static HWND g_hSmartSearchEdit = nullptr;
static int  g_smartHelpCategory = 0; // 0=Todas, 1=Blanca/Negra, 2=Gris, 3=CG50, 4=Software/Hardware
static int  g_smartHelpScrollY = 0;
static std::wstring g_smartSearchQuery;
static POINT g_smartHelpHoverPt = { -1, -1 };
static bool  g_smartHelpTrackingMouse = false;

struct SmartHelpCuratedEntry {
    int            catId;      // 1=Blanca, 2=Gris, 3=CG50, 4=Software/Hardware
    int            calcIdx;    // 0=ClassWiz, 1=Gris ES/VN PLUS, 2=fx-CG50
    const wchar_t* badge;
    const wchar_t* title;
    const wchar_t* keystrokes;
    const wchar_t* explanation;
    std::vector<int> buttonSeq;
    const wchar_t* pdfFile;
    int            pdfPage;
};

static const SmartHelpCuratedEntry g_curatedHelp[] = {
    { 4, 0, L"EMULADOR \x00B7 VENTANA COMPACTA Y POP-UP DISPLAY",
      L"Modo Ventana Rectangular Vertical (1 Calculadora) y Pop-up Display",
      L"Barra superior: Modo Compacto \x00B7 Siempre Visible \x00B7 Pop-up Display \x00B7 Capturar LCD \x00B7 KeyLog",
      L"Muestra una sola calculadora vertical, abre Pop-up Display sincronizado y copia en modos Solo Imagen, Solo Texto o Ambas ($x_1 = \\frac{-b+\\sqrt{\\Delta}}{2a}$).",
      { 13, 41, 36, 4, 43, 42, 14, 49 },
      L"ClassWiz_Emulator_Software_Users_Guide_ES.pdf", 12 },

    { 4, 0, L"EMULADOR \x00B7 PORTAPAPELES, CAPTURA LCD Y EXPORTADOR",
      L"Copiar Imagen + LaTeX, Deshacer RAM ($\\mathrm{Ctrl+Z}$) y Exportar Gu\u00EDas",
      L"Tools -> Configuraci\u00F3n (Ctrl+,) | Exportador de Gu\u00EDas (Ctrl+E) | Guardar/Restaurar RAM (Ctrl+S / Ctrl+Z)",
      L"Permite copiar el LCD o cuerpo al portapapeles como captura, texto LaTeX o ambos simult\u00E1neamente, y seleccionar rangos manuales de botones (#A a #B) con notas LaTeX ($\\int_a^b f(x)\\,dx$).",
      { 10, 11, 14, 4, 45, 4, 42, 49 },
      L"ClassWiz_Emulator_Software_Users_Guide_ES.pdf", 22 },

    { 1, 0, L"BLANCA / NEGRA (fx-570LA X / 991LA X / EX) · HARDWARE Y RESET",
      L"Inicialización Completa, Contraste y LCD Alta Definición $192 \\times 63$",
      L"[SHIFT] [9] (RESET) -> [3] (Inicializar Todo) -> [=] (Sí) | Contraste: [SHIFT] [MENU] [▼] [▼] [4]",
      L"Reinicia modos, unidad angular (D/R/G), formato MathI/MathO y memorias. Indicadores oficiales Casio en la fila superior: $\\mathbf{S}, \\mathbf{A}, \\mathbf{M}, \\mathrm{STO}, \\sqrt{\\Box}/, \\mathrm{D}/\\mathrm{R}/\\mathrm{G}, \\mathrm{FIX}, \\mathrm{SCI}$.",
      { 0, 32, 42, 49 },
      L"fx-570_991LA_X_ES.pdf", 6 },

    { 1, 0, L"BLANCA / NEGRA (fx-570LA X / 991LA X / EX) · ECUACIONES Y SISTEMAS",
      L"Sistemas Lineales ($2\\times 2$ a $4\\times 4$), Polinomios $ax^{2}+bx+c=0$ y SOLVE",
      L"Ecuación: [MENU] [A=(-)] -> [1] Sistema / [2] Polinómica | Newton: [ALPHA] [CALC] (=) -> [SHIFT] [CALC] (SOLVE)",
      L"Resuelve sistemas de 2 a 4 incógnitas y ecuaciones polinómicas de grado 2 a 4 con raíces reales o complejas: $x = \\frac{-b \\pm \\sqrt{b^{2} - 4ac}}{2a}$.",
      { 6, 18, 41, 41, 40, 49 },
      L"fx-570_991LA_X_ES.pdf", 27 },

    { 1, 0, L"BLANCA / NEGRA (fx-570LA X / 991LA X / EX) · CÁLCULO INTEGRAL Y DERIVADAS",
      L"Integrales Definidas $\\int_{a}^{b} f(x)\\,dx$, Derivadas $\\frac{d}{dx}$ y Sumatorias $\\sum$",
      L"Integral: [∫dx] -> f(x) -> [►] lím inf -> [►] lím sup -> [=] | Derivada: [SHIFT] [∫dx] | Suma: [SHIFT] [x]",
      L"Calcula integrales definidas como $\\int_{0}^{3} x^{2}\\,dx = 9$ mediante Gauss-Kronrod y derivadas numéricas por diferencias centrales usando la variable $[x]$.",
      { 10, 11, 14, 4, 45, 4, 42, 49 },
      L"fx-570_991LA_X_ES.pdf", 17 },

    { 1, 0, L"BLANCA / NEGRA (fx-570LA X / 991LA X / EX) · MATRICES Y VECTORES",
      L"Matrices hasta $4\\times 4$, Determinante $\\det(\\mathbf{A})$, Inversa $\\mathbf{A}^{-1}$ y Vectores",
      L"Matrices: [MENU] [4] -> Definir MatA..MatD -> [AC] -> [OPTN] (Det, Trn, Identidad) | Vectores: [MENU] [5]",
      L"Calcula determinantes $\\det(\\mathbf{A})$, inversas $\\mathbf{A}^{-1}$, traspuestas $\\mathbf{A}^{T}$, producto escalar $\\mathbf{u}\\cdot\\mathbf{v}$ y producto vectorial $\\mathbf{u}\\times\\mathbf{v}$.",
      { 6, 35, 8 },
      L"fx-570_991LA_X_ES.pdf", 23 },

    { 1, 0, L"BLANCA / NEGRA (fx-570LA X / 991LA X / EX) · COMPLEJOS $a+bi$",
      L"Números Complejos ($z = a + bi = r\\angle\\theta$), Estadística y Código QR",
      L"Complejos: [MENU] [2] (Unidad $i$ con [ENG], Polar $r\\angle\\theta$ con [OPTN]) | QR: [SHIFT] [OPTN]",
      L"En modo Complejos ([MENU] [2]) opera con números $z \\in \\mathbb{C}$ en forma binomial $a+bi$ y polar $r\\angle\\theta$. En Estadística ([MENU] [6]) obtiene medias $\\bar{x}$, $\\sigma$ y regresiones.",
      { 6, 41, 42, 43, 35, 25, 49 },
      L"fx-570_991LA_X_ES.pdf", 21 },

    { 2, 1, L"GRIS (fx-570VN PLUS / fx-570ES PLUS) · NATURAL-V.P.A.M. Y FRACCIONES",
      L"Fracciones Exactas $\\frac{a}{b}$, Radicales $\\sqrt{x}$ y Alternancia $[S\\Leftrightarrow D]$",
      L"Fracción: [a b/c] -> [▼] -> [►] | Raíz: [√] | Setup: [SHIFT] [MODE] | Reset: [SHIFT] [9] [3] [=]",
      L"La serie Natural-V.P.A.M. ($96\\times 31$ puntos) muestra fracciones $\\frac{1}{2} + \\sqrt{9} = \\frac{7}{2}$ e indicadores superiores oficiales: $\\mathbf{S}, \\mathbf{A}, \\mathbf{M}, \\mathrm{STO}, \\mathrm{RCL}, \\mathrm{STAT}, \\mathrm{CMPLX}, \\mathrm{D}/\\mathrm{R}/\\mathrm{G}$.",
      { 12, 40, 7, 41, 6, 43, 13, 32, 49 },
      L"fx-570_991ES_PLUS_ES.pdf", 5 },

    { 2, 1, L"GRIS (fx-570VN PLUS / fx-570ES PLUS) · ECUACIONES Y DERIVADAS",
      L"Resolución EQN ($aX^{2}+bX+c=0$), Matrices $3\\times 3$, Tablas y $\\frac{d}{dx}$",
      L"Ecuaciones: [MODE] [5] -> [3] $aX^2+bX+c=0$ | Derivada en COMP: [SHIFT] [∫dx] | Matrices: [MODE] [6]",
      L"Incluye modos EQN ([MODE] [5]), MATRIX ([MODE] [6]), TABLE $f(X)$ ([MODE] [7]) y en fx-570VN PLUS además INEQ, RATIO, $\\gcd$ y $\\mathrm{lcm}$.",
      { 4, 36, 42 },
      L"fx-570VN_PLUS_Hardware_Users_Guide.pdf", 25 },

    { 3, 2, L"GRAFICADORA (fx-CG50 PRIZM) · HARDWARE Y RUN-MATRIX",
      L"Pantalla Color $384\\times 216$, Teclas [F1]–[F6] y Plantillas 2D en Run-Matrix",
      L"[MENU] [1] (Run-Matrix) -> Plantillas [F4] (MATH) para $\\int dx, \\frac{d}{dx}, \\sum$ | Catálogo: [SHIFT] [4]",
      L"Ejecuta 14 aplicaciones nativas: Run-Matrix (1), Statistics (2), Graph (5), Table (7), Conic (9), Equation (A), 3D Graph (G), Python (H).",
      { 9, 40, 21, 42, 45, 27, 49 },
      L"fx-CG50_Soft_v370_ES.pdf", 42 },

    { 3, 2, L"GRAFICADORA (fx-CG50 PRIZM) · GRÁFICOS 2D Y G-SOLVE",
      L"Trazado de $Y_{1} = \\sin(x)$, Raíces, Máximos e Integral Bajo la Curva $\\int f(x)\\,dx$",
      L"[MENU] [5] (Graph) -> $Y_1=\\sin(x)$ -> [EXE] -> [F6] (DRAW) | Análisis: [SHIFT] [F5] (G-Solv)",
      L"Permite graficar funciones cartesianas, polares $r(\\theta)$ y paramétricas, sombrear integrales $\\int_{a}^{b} f(x)\\,dx$ y resolver intersecciones $Y_{1}=Y_{2}$.",
      { 9, 36, 49, 21, 18, 27, 49, 5 },
      L"fx-CG50_Soft_v370_ES.pdf", 195 },

    { 3, 2, L"GRAFICADORA (fx-CG50 PRIZM) · EQUATION Y FINANZAS",
      L"Sistemas hasta $6\\times 6$, Polinomios Grado 6 y Solver Numérico Interactivo",
      L"Equation: [MENU] [A] -> [F1] Simultáneas / [F2] Polinomios / [F3] Solver",
      L"Resuelve sistemas lineales $\\mathbf{A}\\mathbf{x} = \\mathbf{b}$ hasta $6\\times 6$ y polinomios $\\sum_{k=0}^{n} a_k x^k = 0$ de grado 2 a 6 con raíces en $\\mathbb{R}$ y $\\mathbb{C}$.",
      { 9, 18, 1 },
      L"fx-CG50_Soft_v370_ES.pdf", 320 }
};

struct IndexedPdfPageEntry {
    std::wstring fileName;
    std::wstring modelCat;
    std::wstring docType;
    int          pageNo = 1;
    std::wstring heading;
    std::wstring snippet;
    std::wstring searchNormalized;
    int          catId = 0; // 1=Blanca, 2=Gris, 3=CG50
    int          calcIdx = 0;
};

static std::vector<IndexedPdfPageEntry> g_indexedPdfPages;
static bool g_pdfIndexLoaded = false;

static std::wstring NormalizeForSearch(const std::wstring& s)
{
    std::wstring out;
    out.reserve(s.size());
    for (wchar_t ch : s) {
        wchar_t c = towlower(ch);
        switch (c) {
        case L'á': case L'à': case L'ä': case L'â': c = L'a'; break;
        case L'é': case L'è': case L'ë': case L'ê': c = L'e'; break;
        case L'í': case L'ì': case L'ï': case L'î': c = L'i'; break;
        case L'ó': case L'ò': case L'ö': case L'ô': c = L'o'; break;
        case L'ú': case L'ù': case L'ü': case L'û': c = L'u'; break;
        case L'ñ': c = L'n'; break;
        }
        out.push_back(c);
    }
    return out;
}

static std::wstring ResolveManualsDirectory()
{
    wchar_t exeDir[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exeDir, MAX_PATH);
    PathRemoveFileSpecW(exeDir);

    std::wstring cand1 = std::wstring(exeDir) + L"\\manuales";
    if (PathFileExistsW(cand1.c_str())) return cand1;

    wchar_t parent1[MAX_PATH]{};
    wcscpy_s(parent1, exeDir);
    PathRemoveFileSpecW(parent1);
    std::wstring cand2 = std::wstring(parent1) + L"\\manuales";
    if (PathFileExistsW(cand2.c_str())) return cand2;

    wchar_t parent2[MAX_PATH]{};
    wcscpy_s(parent2, parent1);
    PathRemoveFileSpecW(parent2);
    std::wstring cand3 = std::wstring(parent2) + L"\\manuales";
    if (PathFileExistsW(cand3.c_str())) return cand3;

    return cand1;
}

static void OpenOfficialPdfManual(const std::wstring& pdfFileName, int pageNo)
{
    std::wstring dir = ResolveManualsDirectory();
    std::wstring fullPath = dir + L"\\" + pdfFileName;
    if (!PathFileExistsW(fullPath.c_str())) {
        std::wstring msg = L"No se encontró el archivo PDF en:\n" + fullPath;
        MessageBoxW(g_hSmartHelpWnd ? g_hSmartHelpWnd : g_hMainWnd, msg.c_str(), L"Manual PDF", MB_ICONWARNING | MB_OK);
        return;
    }

    OpenNativePdfViewer(
        fullPath,
        (std::max)(1, pageNo),
        g_hSmartHelpWnd ? g_hSmartHelpWnd : g_hMainWnd,
        dir);
}

static void OpenOfficialManualsFolder()
{
    std::wstring dir = ResolveManualsDirectory();
    ShellExecuteW(g_hMainWnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static std::vector<int> ExtractKeyIdsFromPdfText(const std::wstring& normText, int calcIdx)
{
    std::vector<int> seq;
    auto addUnique = [&](int k) {
        if (seq.size() < 7 && (seq.empty() || seq.back() != k)) seq.push_back(k);
    };
    if (normText.find(L"shift") != std::wstring::npos) addUnique(calcIdx == 2 ? 6 : 0);
    if (normText.find(L"alpha") != std::wstring::npos) addUnique(calcIdx == 0 ? 1 : (calcIdx == 1 ? 3 : 14));
    if (normText.find(L"menu") != std::wstring::npos || normText.find(L"setup") != std::wstring::npos || normText.find(L"mode") != std::wstring::npos) {
        addUnique(calcIdx == 0 ? 6 : (calcIdx == 1 ? 4 : 9));
    }
    if (normText.find(L"optn") != std::wstring::npos && calcIdx != 1) addUnique(calcIdx == 0 ? 8 : 7);
    if (normText.find(L"calc") != std::wstring::npos || normText.find(L"solve") != std::wstring::npos) {
        if (calcIdx == 0) addUnique(9);
        else if (calcIdx == 1) addUnique(8);
    }
    if (normText.find(L"integral") != std::wstring::npos || normText.find(L"deriv") != std::wstring::npos) {
        if (calcIdx == 0) addUnique(10);
        else if (calcIdx == 1) addUnique(9);
    }
    if (normText.find(L"frac") != std::wstring::npos) addUnique(calcIdx == 2 ? 24 : 12);
    if (normText.find(L"raiz") != std::wstring::npos || normText.find(L"root") != std::wstring::npos || normText.find(L"sqrt") != std::wstring::npos) {
        addUnique(calcIdx == 2 ? 15 : 13);
    }
    if (normText.find(L"sin") != std::wstring::npos || normText.find(L"sen") != std::wstring::npos || normText.find(L"trig") != std::wstring::npos) addUnique(21);
    if (normText.find(L"log") != std::wstring::npos) addUnique(calcIdx == 2 ? 19 : 16);
    if (normText.find(L"ln") != std::wstring::npos) addUnique(calcIdx == 2 ? 20 : 17);
    if (seq.empty()) {
        if (calcIdx == 0) seq = { 6, 40, 49 };
        else if (calcIdx == 1) seq = { 4, 40, 49 };
        else seq = { 9, 40, 49 };
    } else {
        addUnique(49);
    }
    return seq;
}

static void EnsurePdfIndexLoaded()
{
    if (g_pdfIndexLoaded) return;
    g_pdfIndexLoaded = true;

    std::wstring tsvPath = ResolveManualsDirectory() + L"\\indice_paginas_pdf.tsv";
    std::ifstream in(tsvPath, std::ios::binary);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        int wlen = MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.size(), nullptr, 0);
        if (wlen <= 0) continue;
        std::wstring wline(wlen, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, line.c_str(), (int)line.size(), &wline[0], wlen);

        std::vector<std::wstring> cols;
        size_t start = 0;
        while (start <= wline.size()) {
            size_t tab = wline.find(L'\t', start);
            if (tab == std::wstring::npos) {
                cols.push_back(wline.substr(start));
                break;
            }
            cols.push_back(wline.substr(start, tab - start));
            start = tab + 1;
        }
        if (cols.size() < 6) continue;

        IndexedPdfPageEntry e{};
        e.fileName = cols[0];
        e.modelCat = cols[1];
        e.docType  = cols[2];
        e.pageNo   = _wtoi(cols[3].c_str());
        e.heading  = cols[4];
        e.snippet  = cols[5];
        if (e.modelCat.find(L"Blanca") != std::wstring::npos) { e.catId = 1; e.calcIdx = 0; }
        else if (e.modelCat.find(L"Gris") != std::wstring::npos) { e.catId = 2; e.calcIdx = 1; }
        else if (e.modelCat.find(L"Graficadora") != std::wstring::npos) { e.catId = 3; e.calcIdx = 2; }
        e.searchNormalized = NormalizeForSearch(e.modelCat + L" " + e.docType + L" " + e.heading + L" " + e.snippet);
        g_indexedPdfPages.push_back(std::move(e));
    }
}

struct HelpHitCard {
    RECT             rcCard{};
    RECT             rcOpenPdf{};
    RECT             rcExportGuide{};
    RECT             rcRunOnCalc{};
    std::wstring     pdfFile;
    int              pageNo = 1;
    int              calcIdx = 0;
    std::vector<int> buttonSeq;
    std::wstring     title;
    std::wstring     explanation;
};

static std::vector<HelpHitCard> g_helpHitCards;
static RECT g_helpCatTabRects[5]{};
static RECT g_helpOpenFolderBtnRect{};

static bool MatchesQueryTokens(const std::wstring& normalizedText, const std::vector<std::wstring>& tokens)
{
    for (const auto& tok : tokens) {
        if (!tok.empty() && normalizedText.find(tok) == std::wstring::npos)
            return false;
    }
    return true;
}

static void ExecuteHelpButtonSequenceOnCalc(int calcIdx, const std::vector<int>& buttonSeq)
{
    if (calcIdx < 0 || calcIdx > 2 || buttonSeq.empty()) return;
    SaveCalculatorSnapshotState(calcIdx);
    g_activeCalcIdx = calcIdx;
    for (int keyId : buttonSeq) {
        HandleKeyPress(calcIdx, keyId);
    }
    ShowToolbarToast(L"Secuencia enviada a " + g_nativeCalcs[calcIdx].displayName + L" (Ctrl+Z para deshacer)");
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
        InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
}

static void DrawSmartHelpButtonSequenceStrip(HDC hdc, int calcIdx, const std::vector<int>& buttonSeq, RECT rcStrip)
{
    if (buttonSeq.empty()) return;
    if (calcIdx < 0 || calcIdx > 2) calcIdx = 0;

    CasioTheme::FillRoundRectAA(hdc, rcStrip, 8.0f, CasioTheme::StudioTrack(), CasioTheme::StudioBorder(), 1.0f);

    HFONT hLblF = CasioTheme::CreateUiFont(10, true);
    HFONT hCapF = CasioTheme::CreateUiFont(11, true);
    HFONT hArrF = CasioTheme::CreateUiFont(13, true);
    HFONT hOldF = (HFONT)SelectObject(hdc, hLblF);

    const int n = (int)buttonSeq.size();
    const int stripW = (std::max)(80, (int)(rcStrip.right - rcStrip.left));
    const int stripH = (std::max)(44, (int)(rcStrip.bottom - rcStrip.top));
    const int availW = stripW - 16;
    const int arrW = 12;

    // Fit all keys cleanly on 1 row if possible, shrinking boxW smoothly down to 40px before wrapping
    int singleRowBoxW = (n > 0) ? (availW - (n - 1) * arrW - 4) / n : 54;
    bool twoRows = (singleRowBoxW < 40 && n > 1);
    int perRow = twoRows ? (n + 1) / 2 : n;
    int boxW = twoRows
        ? (std::max)(38, (std::min)(60, (availW - (perRow - 1) * arrW) / (std::max)(1, perRow)))
        : (std::max)(40, (std::min)(60, singleRowBoxW));
    int rowH = twoRows ? (stripH - 10) / 2 : (stripH - 8);
    int curX = rcStrip.left + 8;
    int curY = rcStrip.top + (twoRows ? 4 : 4);
    int rowIdx = 0;

    for (int i = 0; i < n; ++i) {
        if (twoRows && i == perRow && rowIdx == 0) {
            rowIdx = 1;
            curX = rcStrip.left + 8;
            curY = rcStrip.top + 6 + rowH;
        }
        int keyId = buttonSeq[i];
        RECT rcKeyCard = { curX, curY, curX + boxW, curY + rowH };
        if (rcKeyCard.right > rcStrip.right - 6) {
            if (!twoRows) break;
        }

        CasioTheme::FillRoundRectAA(hdc, rcKeyCard, 6.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorderStrong(), 1.0f);

        wchar_t sBadge[16];
        swprintf(sBadge, 16, L"#%d", i + 1);
        SelectObject(hdc, hLblF);
        SetTextColor(hdc, CasioTheme::StudioAccentText());
        RECT rcNum = { rcKeyCard.left + 3, rcKeyCard.top + 1, rcKeyCard.right - 3, rcKeyCard.top + 13 };
        DrawTextW(hdc, sBadge, -1, &rcNum, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        const auto& cap = KeyCaptureForHistory(hdc, calcIdx, keyId);
        if (cap.bitmap && cap.width > 0 && cap.height > 0) {
            HDC hdcKey = CreateCompatibleDC(hdc);
            HBITMAP hOldK = (HBITMAP)SelectObject(hdcKey, cap.bitmap);
            int maxW = (std::max)(14, boxW - 10);
            int maxH = (std::max)(12, rowH - 27);
            double sc = (std::min)((double)maxW / cap.width, (double)maxH / cap.height);
            int dw = (std::max)(14, (int)(cap.width * sc));
            int dh = (std::max)(12, (int)(cap.height * sc));
            int dx = rcKeyCard.left + (boxW - dw) / 2;
            int dy = rcKeyCard.top + 13 + (maxH - dh) / 2;
            SetStretchBltMode(hdc, HALFTONE);
            SetBrushOrgEx(hdc, 0, 0, nullptr);
            StretchBlt(hdc, dx, dy, dw, dh, hdcKey, 0, 0, cap.width, cap.height, SRCCOPY);
            SelectObject(hdcKey, hOldK);
            DeleteDC(hdcKey);
        }

        std::wstring capTxt = KeyHistoryCaption(calcIdx, keyId);
        RECT rcCap = { rcKeyCard.left + 2, rcKeyCard.bottom - 15, rcKeyCard.right - 2, rcKeyCard.bottom - 1 };
        DrawLatexRichTextCentered(hdc, capTxt, rcCap, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 11);

        curX += boxW;
        bool hasNextInRow = (i + 1 < n) && !(twoRows && i + 1 == perRow);
        if (hasNextInRow && curX + arrW + 30 <= rcStrip.right - 6) {
            RECT rcArr = { curX, rcKeyCard.top, curX + arrW, rcKeyCard.bottom };
            SelectObject(hdc, hArrF);
            SetTextColor(hdc, CasioTheme::StudioAmberText());
            DrawTextW(hdc, L"\x2192", -1, &rcArr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            curX += arrW;
        } else {
            curX += 4;
        }
    }

    SelectObject(hdc, hOldF);
    DeleteObject(hLblF);
    DeleteObject(hCapF);
    DeleteObject(hArrF);
}

static void ComputeSmartHelpTopLayout(
    int w, RECT& rcTitle, RECT& rcFolderBtn,
    RECT& rcSearchOuter, RECT& rcSearchEdit, RECT& rcStatPill,
    RECT& rcSegTrack, RECT (&tabRects)[5])
{
    int cw = (std::max)(600, w);
    rcFolderBtn   = { cw - 200, 14, cw - 24, 46 };
    rcTitle       = { 24, 10, cw - 216, 52 };
    rcSearchOuter = { 24, 58, cw - 216, 92 };
    rcSearchEdit  = { 34, 65, cw - 226, 85 };
    rcStatPill    = { cw - 200, 60, cw - 24, 90 };
    rcSegTrack    = { 24, 102, cw - 24, 138 };

    int innerW = (rcSegTrack.right - rcSegTrack.left) - 8;
    int gap = 4;
    int tabW = (innerW - 4 * gap) / 5;
    for (int i = 0; i < 5; ++i) {
        int tx = rcSegTrack.left + 4 + i * (tabW + gap);
        int txRight = (i == 4) ? (rcSegTrack.right - 4) : (tx + tabW);
        tabRects[i] = { tx, rcSegTrack.top + 4, txRight, rcSegTrack.bottom - 4 };
    }
}

static HFONT g_hSmartSearchFont = nullptr;
static int   g_smartSearchFontPx = 0;

static float GetSmartHelpUiScale(HWND hWnd)
{
    return CasioTheme::ComputeDialogUiScale(hWnd, 1140, 780);
}

static void SyncSmartHelpSearchEdit(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd) || !g_hSmartSearchEdit) return;
    const float s = GetSmartHelpUiScale(hWnd);
    const int wantPx = (std::max)(12, CasioTheme::UiToDevice(14, s));
    if (!g_hSmartSearchFont || g_smartSearchFontPx != wantPx) {
        if (g_hSmartSearchFont) { DeleteObject(g_hSmartSearchFont); g_hSmartSearchFont = nullptr; }
        g_hSmartSearchFont = CreateFontW(-wantPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
            DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        g_smartSearchFontPx = wantPx;
        SendMessageW(g_hSmartSearchEdit, WM_SETFONT, (WPARAM)g_hSmartSearchFont, TRUE);
    }
    RECT rcPhys{};
    GetClientRect(hWnd, &rcPhys);
    const int logicalW = CasioTheme::DeviceToLogical(rcPhys.right, s);
    if (logicalW > 280) {
        RECT rT, rF, rSO, rSE, rSt, rTrack, tabs[5];
        ComputeSmartHelpTopLayout(logicalW, rT, rF, rSO, rSE, rSt, rTrack, tabs);
        MoveWindow(g_hSmartSearchEdit,
                   CasioTheme::UiToDevice(rSE.left, s),
                   CasioTheme::UiToDevice(rSE.top, s),
                   (std::max)(120, CasioTheme::UiToDevice(rSE.right - rSE.left, s)),
                   (std::max)(18, CasioTheme::UiToDevice(rSE.bottom - rSE.top, s)),
                   TRUE);
    }
}

static LRESULT CALLBACK SmartHelpWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static constexpr int HELP_TOP_FIXED_H = 148;
    static constexpr UINT_PTR ID_EDIT_SEARCH = 3101;

    switch (msg) {
    case WM_CREATE: {
        CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
        CasioTheme::EnableThemeScrollbar(hWnd, CasioTheme::g_studioDarkTheme);
        EnsurePdfIndexLoaded();
        g_hSmartSearchEdit = CreateWindowExW(
            0, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            34, 65, 680, 20,
            hWnd, (HMENU)ID_EDIT_SEARCH, g_hInst, nullptr);
        SendMessageW(g_hSmartSearchEdit, 0x1501 /*EM_SETCUEBANNER*/, TRUE,
                     (LPARAM)L"Buscar en los 16 manuales PDF y gu\u00EDas (ej: ecuaciones, matriz, integral, 3D, contraste, reset, pop-up, QR)...");
        if (!g_smartSearchQuery.empty()) {
            SetWindowTextW(g_hSmartSearchEdit, g_smartSearchQuery.c_str());
        }
        SyncSmartHelpSearchEdit(hWnd);
        return 0;
    }

    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        if (suggested) {
            SetWindowPos(hWnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
        }
        SyncSmartHelpSearchEdit(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_CTLCOLOREDIT:
        return CasioTheme::HandleStudioEditCtlColor(wParam);

    case WM_ERASEBKGND:
        return 1;

    case WM_SIZE: {
        SyncSmartHelpSearchEdit(hWnd);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_COMMAND: {
        if (LOWORD(wParam) == ID_EDIT_SEARCH) {
            if (HIWORD(wParam) == EN_CHANGE) {
                wchar_t buf[256]{};
                GetWindowTextW(g_hSmartSearchEdit, buf, 255);
                g_smartSearchQuery = buf;
                g_smartHelpScrollY = 0;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
            if (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == EN_KILLFOCUS) {
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
        break;
    }

    case WM_MOUSEMOVE: {
        const float s = GetSmartHelpUiScale(hWnd);
        g_smartHelpHoverPt.x = CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s);
        g_smartHelpHoverPt.y = CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s);
        if (!g_smartHelpTrackingMouse) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
            TrackMouseEvent(&tme);
            g_smartHelpTrackingMouse = true;
        }
        bool overHit = PtInRect(&g_helpOpenFolderBtnRect, g_smartHelpHoverPt) != FALSE;
        for (int i = 0; i < 5 && !overHit; ++i) {
            if (PtInRect(&g_helpCatTabRects[i], g_smartHelpHoverPt)) overHit = true;
        }
        if (!overHit && g_smartHelpHoverPt.y >= HELP_TOP_FIXED_H) {
            constexpr int manualCount = (int)(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
            for (int i = 0; i < manualCount; ++i) {
                if (PtInRect(&g_officialManuals[i].btnRect, g_smartHelpHoverPt)) {
                    overHit = true;
                    break;
                }
            }
            for (const auto& card : g_helpHitCards) {
                if (PtInRect(&card.rcRunOnCalc, g_smartHelpHoverPt) ||
                    PtInRect(&card.rcExportGuide, g_smartHelpHoverPt) ||
                    PtInRect(&card.rcOpenPdf, g_smartHelpHoverPt) ||
                    PtInRect(&card.rcCard, g_smartHelpHoverPt)) {
                    overHit = true;
                    break;
                }
            }
        }
        SetCursor(LoadCursor(nullptr, overHit ? IDC_HAND : IDC_ARROW));
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_MOUSELEAVE:
        g_smartHelpTrackingMouse = false;
        g_smartHelpHoverPt = { -1, -1 };
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        const float s = GetSmartHelpUiScale(hWnd);
        int mx = CasioTheme::DeviceToLogical(GET_X_LPARAM(lParam), s);
        int my = CasioTheme::DeviceToLogical(GET_Y_LPARAM(lParam), s);
        POINT pt{ mx, my };

        if (PtInRect(&g_helpOpenFolderBtnRect, pt)) {
            OpenOfficialManualsFolder();
            return 0;
        }

        for (int i = 0; i < 5; ++i) {
            if (PtInRect(&g_helpCatTabRects[i], pt)) {
                g_smartHelpCategory = i;
                g_smartHelpScrollY = 0;
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }

        if (my >= HELP_TOP_FIXED_H) {
            constexpr int manualCount = (int)(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
            for (int i = 0; i < manualCount; ++i) {
                if (PtInRect(&g_officialManuals[i].btnRect, pt)) {
                    OpenOfficialPdfManual(g_officialManuals[i].fileName, 1);
                    return 0;
                }
            }

            for (const auto& card : g_helpHitCards) {
                if (PtInRect(&card.rcRunOnCalc, pt)) {
                    ExecuteHelpButtonSequenceOnCalc(card.calcIdx, card.buttonSeq);
                    return 0;
                }
                if (PtInRect(&card.rcExportGuide, pt)) {
                    OpenSequenceExporterWindow(false);
                    LoadCustomSequenceIntoExporter(card.calcIdx, card.buttonSeq, card.title, card.explanation);
                    return 0;
                }
                if (PtInRect(&card.rcOpenPdf, pt) || PtInRect(&card.rcCard, pt)) {
                    OpenOfficialPdfManual(card.pdfFile, card.pageNo);
                    return 0;
                }
            }
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        GetScrollInfo(hWnd, SB_VERT, &si);
        int maxScroll = (std::max)(0, si.nMax - (int)si.nPage + 1);
        g_smartHelpScrollY = (std::max)(0, (std::min)(g_smartHelpScrollY - (delta > 0 ? 90 : -90), maxScroll));
        SetScrollPos(hWnd, SB_VERT, g_smartHelpScrollY, TRUE);
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
        case SB_LINEUP: pos -= 60; break;
        case SB_LINEDOWN: pos += 60; break;
        case SB_PAGEUP: pos -= (int)si.nPage; break;
        case SB_PAGEDOWN: pos += (int)si.nPage; break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: pos = si.nTrackPos; break;
        }
        int maxScroll = (std::max)(0, si.nMax - (int)si.nPage + 1);
        g_smartHelpScrollY = (std::max)(0, (std::min)(pos, maxScroll));
        SetScrollPos(hWnd, SB_VERT, g_smartHelpScrollY, TRUE);
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps{};
        HDC hdcWin = BeginPaint(hWnd, &ps);
        RECT rcPhys{};
        GetClientRect(hWnd, &rcPhys);
        if (rcPhys.right <= 0 || rcPhys.bottom <= 0) {
            EndPaint(hWnd, &ps);
            return 0;
        }

        HDC hdc = CreateCompatibleDC(hdcWin);
        HBITMAP hBmp = CreateCompatibleBitmap(hdcWin, rcPhys.right, rcPhys.bottom);
        HGDIOBJ oldBmp = SelectObject(hdc, hBmp);

        HFONT hUiSecF    = CasioTheme::CreateUiFont(12, true);
        HFONT hUiCardTF  = CasioTheme::CreateUiFont(14, true);
        HFONT hUiMetaF   = CasioTheme::CreateUiFont(11, true);
        HFONT hUiSmallF  = CasioTheme::CreateUiFont(12, false);

        const float s = GetSmartHelpUiScale(hWnd);
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

        RECT rcHeader = { 0, 0, rc.right, HELP_TOP_FIXED_H };
        CasioTheme::DrawTranslucentHeaderStrip(hdc, rcHeader, true);

        SetBkMode(hdc, TRANSPARENT);

        RECT rcTitle{}, rcFolderBtn{}, rcSearchOuter{}, rcSearchEdit{}, rcStatPill{}, rcSegTrack{};
        ComputeSmartHelpTopLayout(rc.right, rcTitle, rcFolderBtn, rcSearchOuter, rcSearchEdit,
                                  rcStatPill, rcSegTrack, g_helpCatTabRects);
        g_helpOpenFolderBtnRect = rcFolderBtn;

        DrawLatexRichText(
            hdc,
            L"Ayuda y Manuales",
            rcTitle, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 15, true
        );

        constexpr int manualCount = (int)(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
        bool folderHover = PtInRect(&g_helpOpenFolderBtnRect, g_smartHelpHoverPt) != FALSE;
        std::wstring folderLbl = L"Abrir Carpeta PDF (" + std::to_wstring(manualCount) + L")";
        CasioTheme::DrawStudioButton(hdc, g_helpOpenFolderBtnRect, folderLbl,
                                     hUiSecF, CasioTheme::BTN_SECONDARY, false, folderHover, 7.0f);

        // Dark Search Input Chrome
        bool searchFocused = (GetFocus() == g_hSmartSearchEdit);
        CasioTheme::DrawInputChrome(hdc, rcSearchOuter, searchFocused);

        // Tokenize search query & filter data BEFORE painting sticky counter badge
        std::wstring normQ = NormalizeForSearch(g_smartSearchQuery);
        std::vector<std::wstring> tokens;
        {
            size_t pos = 0;
            while (pos < normQ.size()) {
                while (pos < normQ.size() && normQ[pos] == L' ') ++pos;
                size_t end = normQ.find(L' ', pos);
                if (end == std::wstring::npos) {
                    if (pos < normQ.size()) tokens.push_back(normQ.substr(pos));
                    break;
                }
                tokens.push_back(normQ.substr(pos, end - pos));
                pos = end + 1;
            }
        }

        for (int i = 0; i < manualCount; ++i) g_officialManuals[i].btnRect = { 0, 0, 0, 0 };

        std::vector<int> matchedManuals;
        for (int i = 0; i < manualCount; ++i) {
            const auto& m = g_officialManuals[i];
            if (g_smartHelpCategory != 0 && m.catId != g_smartHelpCategory) continue;
            if (!tokens.empty()) {
                std::wstring blob = NormalizeForSearch(
                    std::wstring(m.fullTitle) + L" " + m.modelCategory + L" " + m.manualKind + L" " + m.fileName);
                if (!MatchesQueryTokens(blob, tokens)) continue;
            }
            matchedManuals.push_back(i);
        }

        std::vector<const SmartHelpCuratedEntry*> matchedCurated;
        for (const auto& item : g_curatedHelp) {
            if (g_smartHelpCategory != 0 && item.catId != g_smartHelpCategory) continue;
            if (!tokens.empty()) {
                std::wstring blob = NormalizeForSearch(
                    std::wstring(item.badge) + L" " + item.title + L" " + item.keystrokes + L" " + item.explanation);
                if (!MatchesQueryTokens(blob, tokens)) continue;
            }
            matchedCurated.push_back(&item);
        }

        std::vector<const IndexedPdfPageEntry*> matchedPdfPages;
        for (const auto& page : g_indexedPdfPages) {
            if (g_smartHelpCategory != 0) {
                if (g_smartHelpCategory == 4) {
                    if (page.docType.find(L"Software") == std::wstring::npos &&
                        page.docType.find(L"Emulador") == std::wstring::npos &&
                        page.docType.find(L"Activaci\u00F3n") == std::wstring::npos) continue;
                } else if (page.catId != g_smartHelpCategory) {
                    continue;
                }
            }
            if (!tokens.empty()) {
                if (!MatchesQueryTokens(page.searchNormalized, tokens)) continue;
            }
            matchedPdfPages.push_back(&page);
            if (matchedPdfPages.size() >= 60) break;
        }

        std::wstring statPillTxt = std::to_wstring(matchedCurated.size()) + L" gu\u00EDas \x00B7 "
                                 + std::to_wstring(matchedPdfPages.size())
                                 + (matchedPdfPages.size() >= 60 ? L"+ p\u00E1g." : L" p\u00E1g.");
        SelectObject(hdc, hUiMetaF);
        SetTextColor(hdc, CasioTheme::StudioTextSec());
        DrawTextW(hdc, statPillTxt.c_str(), -1, &rcStatPill, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

        // Floating material translúcido 2.0 Segmented Category Bar
        CasioTheme::DrawFloatingGlassCapsule(hdc, rcSegTrack, s, -1.0f, false);
        const wchar_t* catLabels[5] = {
            L"Todas (4 Calc + Soft)",
            L"Negra / Blanca (991CW \x00B7 LA X)",
            L"Gris (ES / VN PLUS)",
            L"Graficadora (fx-CG50)",
            L"Emulador y Licencias"
        };
        for (int i = 0; i < 5; ++i) {
            bool active = (g_smartHelpCategory == i);
            bool hovered = PtInRect(&g_helpCatTabRects[i], g_smartHelpHoverPt) != FALSE;
            CasioTheme::DrawStudioSegmentItem(hdc, g_helpCatTabRects[i], catLabels[i], active ? hUiSecF : hUiSmallF, active, hovered, active);
        }

        HPEN penDiv = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
        HGDIOBJ oldPen = SelectObject(hdc, penDiv);
        MoveToEx(hdc, 0, HELP_TOP_FIXED_H - 1, nullptr);
        LineTo(hdc, rc.right, HELP_TOP_FIXED_H - 1);
        SelectObject(hdc, oldPen);
        DeleteObject(penDiv);

        int savedDC = SaveDC(hdc);
        IntersectClipRect(hdc, 0, HELP_TOP_FIXED_H, rc.right, rc.bottom);

        g_helpHitCards.clear();
        int y = 14;
        const int cardLeft = 24;
        const int cardRight = (std::max)(cardLeft + 520, (int)rc.right - 24);
        const int availW = cardRight - cardLeft;

        auto drawSectionHeader = [&](const std::wstring& title, const std::wstring& pillTxt, COLORREF accent) {
            (void)accent;
            int sy = HELP_TOP_FIXED_H + y - g_smartHelpScrollY;
            if (sy + 28 >= HELP_TOP_FIXED_H && sy <= rc.bottom) {
                RECT rcSecLbl = { cardLeft, sy, cardRight - 160, sy + 24 };
                SelectObject(hdc, hUiSecF);
                SetTextColor(hdc, CasioTheme::StudioText());
                DrawTextW(hdc, title.c_str(), -1, &rcSecLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                RECT rcSecPill = { cardRight - 160, sy, cardRight, sy + 24 };
                SelectObject(hdc, hUiMetaF);
                SetTextColor(hdc, CasioTheme::StudioTextMuted());
                DrawTextW(hdc, pillTxt.c_str(), -1, &rcSecPill, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

                HPEN hLinePen = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
                HGDIOBJ oldP = SelectObject(hdc, hLinePen);
                MoveToEx(hdc, cardLeft, sy + 25, nullptr);
                LineTo(hdc, cardRight, sy + 25);
                SelectObject(hdc, oldP);
                DeleteObject(hLinePen);
            }
            y += 32;
        };

        auto renderManualCardsSection = [&]() {
            if (matchedManuals.empty()) return;
            drawSectionHeader(
                L"Manuales Oficiales PDF",
                std::to_wstring(matchedManuals.size()) + L" documentos PDF",
                CasioTheme::StudioAccent());

            const int cols = 2;
            const int gapX = 12;
            const int gapY = 10;
            const int colW = (availW - gapX) / cols;
            const int cardH = 78;

            for (size_t idx = 0; idx < matchedManuals.size(); ++idx) {
                int mIdx = matchedManuals[idx];
                auto& m = g_officialManuals[mIdx];
                int r = (int)idx / cols;
                int c = (int)idx % cols;
                int cx = cardLeft + c * (colW + gapX);
                int screenY = HELP_TOP_FIXED_H + y + r * (cardH + gapY) - g_smartHelpScrollY;
                RECT cardRc = { cx, screenY, cx + colW, screenY + cardH };

                if (screenY + cardH >= HELP_TOP_FIXED_H && screenY <= rc.bottom) {
                    m.btnRect = cardRc;
                    bool hovered = PtInRect(&cardRc, g_smartHelpHoverPt) != FALSE;

                    COLORREF accentCol = CasioTheme::StudioAccent();
                    if (m.catId == 2) {
                        accentCol = CasioTheme::StudioTextSec();
                    } else if (m.catId == 3) {
                        accentCol = CasioTheme::StudioViolet();
                    } else if (m.catId == 4) {
                        accentCol = CasioTheme::StudioSuccess();
                    }

                    CasioTheme::FillRoundRectAA(hdc, cardRc, 9.0f,
                                                hovered ? CasioTheme::StudioSurfaceAlt() : CasioTheme::StudioSurface(),
                                                hovered ? accentCol : CasioTheme::StudioBorder(), 1.0f);

                    std::wstring kindStr = m.manualKind ? m.manualKind : L"";
                    bool isEs = (kindStr.find(L"(ES)") != std::wstring::npos ||
                                 std::wstring(m.fileName).find(L"_ES") != std::wstring::npos);
                    std::wstring langTxt = isEs ? L"ES" : L"EN";

                    CasioTheme::DrawDotIndicator(hdc, cardRc.left + 16, cardRc.top + 17, 4, accentCol);
                    std::wstring metaLine = std::wstring(m.modelCategory) + L" \x00B7 " + langTxt + L" \x00B7 " + kindStr;
                    RECT rcKindTxt = { cardRc.left + 26, cardRc.top + 8, cardRc.right - 88, cardRc.top + 26 };
                    SelectObject(hdc, hUiMetaF);
                    SetTextColor(hdc, CasioTheme::StudioTextSec());
                    DrawTextW(hdc, metaLine.c_str(), -1, &rcKindTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                    RECT rcOpenCue = { cardRc.right - 84, cardRc.top + 8, cardRc.right - 12, cardRc.top + 26 };
                    SetTextColor(hdc, hovered ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextMuted());
                    DrawTextW(hdc, L"Abrir PDF \x2192", -1, &rcOpenCue, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                    // Full non-truncated manual title wrapped neatly
                    RECT rcFullTitle = { cardRc.left + 14, cardRc.top + 30, cardRc.right - 12, cardRc.bottom - 6 };
                    SelectObject(hdc, hUiCardTF);
                    SetTextColor(hdc, CasioTheme::StudioText());
                    DrawTextW(hdc, m.fullTitle, -1, &rcFullTitle, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
                }
            }

            int totalRows = ((int)matchedManuals.size() + cols - 1) / cols;
            y += totalRows * (cardH + gapY) + 8;
        };

        auto renderCuratedGuidesSection = [&]() {
            if (matchedCurated.empty()) return;
            drawSectionHeader(
                L"Gu\u00EDas Interactivas",
                std::to_wstring(matchedCurated.size()) + L" gu\u00EDas",
                CasioTheme::SuccessGreen);

            for (const auto* item : matchedCurated) {
                const int nKeys = (int)item->buttonSeq.size();
                const int singleRowBoxW = (nKeys > 0) ? (availW - 44 - (nKeys - 1) * 12) / nKeys : 54;
                const bool stripTwoRows = (singleRowBoxW < 40 && nKeys > 1);
                const int cardH = stripTwoRows ? 260 : 212;
                int screenY = HELP_TOP_FIXED_H + y - g_smartHelpScrollY;
                RECT cardRc = { cardLeft, screenY, cardRight, screenY + cardH };

                RECT rcRunBtn = { cardRight - 346, screenY + 10, cardRight - 234, screenY + 38 };
                RECT rcExpBtn = { cardRight - 226, screenY + 10, cardRight - 114, screenY + 38 };
                RECT rcPdfBtn = { cardRight - 106, screenY + 10, cardRight - 12,  screenY + 38 };

                HelpHitCard hit{};
                hit.rcCard = cardRc;
                hit.rcRunOnCalc = rcRunBtn;
                hit.rcExportGuide = rcExpBtn;
                hit.rcOpenPdf = rcPdfBtn;
                hit.pdfFile = item->pdfFile;
                hit.pageNo = item->pdfPage;
                hit.calcIdx = item->calcIdx;
                hit.buttonSeq = item->buttonSeq;
                hit.title = item->title;
                hit.explanation = item->explanation;
                if (screenY + cardH >= HELP_TOP_FIXED_H && screenY <= rc.bottom) {
                    g_helpHitCards.push_back(hit);
                }

                if (screenY + cardH >= HELP_TOP_FIXED_H && screenY <= rc.bottom) {
                    CasioTheme::FillRoundRectAA(hdc, cardRc, 10.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorderStrong(), 1.0f);

                    CasioTheme::DrawDotIndicator(hdc, cardLeft + 16, screenY + 20, 4, CasioTheme::StudioSuccess());
                    SelectObject(hdc, hUiMetaF);
                    SetTextColor(hdc, CasioTheme::StudioSuccessText());
                    RECT rVerifyLbl = { cardLeft + 25, screenY + 10, cardLeft + 158, screenY + 30 };
                    DrawTextW(hdc, L"Secuencia verificada", -1, &rVerifyLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                    SetTextColor(hdc, CasioTheme::StudioTextMuted());
                    RECT rBadge = { cardLeft + 162, screenY + 10, cardRight - 356, screenY + 30 };
                    DrawTextW(hdc, item->badge, -1, &rBadge, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

                    CasioTheme::DrawStudioButton(hdc, rcRunBtn, L"Probar Pasos", hUiMetaF, CasioTheme::BTN_SUCCESS,
                                                 false, PtInRect(&rcRunBtn, g_smartHelpHoverPt) != FALSE, 7.0f);
                    CasioTheme::DrawStudioButton(hdc, rcExpBtn, L"Exportar Gu\u00EDa", hUiMetaF, CasioTheme::BTN_VIOLET,
                                                 false, PtInRect(&rcExpBtn, g_smartHelpHoverPt) != FALSE, 7.0f);
                    std::wstring pdfLbl = L"PDF p." + std::to_wstring(item->pdfPage);
                    CasioTheme::DrawStudioButton(hdc, rcPdfBtn, pdfLbl, hUiMetaF, CasioTheme::BTN_PRIMARY,
                                                 false, PtInRect(&rcPdfBtn, g_smartHelpHoverPt) != FALSE, 7.0f);

                    RECT rTitle = { cardLeft + 14, screenY + 34, cardRight - 356, screenY + 66 };
                    DrawLatexRichText(hdc, item->title, rTitle, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 14, true);

                    RECT rKeys = { cardLeft + 14, screenY + 68, cardRight - 14, screenY + 86 };
                    DrawLatexRichText(hdc, std::wstring(L"Ruta: ") + item->keystrokes, rKeys,
                                      CasioTheme::StudioAmberText(), CasioTheme::StudioAccentText(), 12, true);

                    RECT rExp = { cardLeft + 14, screenY + 86, cardRight - 14, screenY + 132 };
                    DrawLatexRichText(hdc, item->explanation, rExp, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 13, false);

                    RECT rcStrip = { cardLeft + 14, screenY + 134, cardRight - 14, screenY + cardH - 8 };
                    DrawSmartHelpButtonSequenceStrip(hdc, item->calcIdx, item->buttonSeq, rcStrip);
                }
                y += cardH + 10;
            }
            y += 6;
        };

        auto renderPdfPagesSection = [&]() {
            if (matchedPdfPages.empty()) return;
            drawSectionHeader(
                L"\u00CDndice Textual de P\u00E1ginas en Manuales PDF",
                std::to_wstring(matchedPdfPages.size()) + (matchedPdfPages.size() >= 60 ? L"+ p\u00E1ginas" : L" p\u00E1ginas"),
                CasioTheme::AmberWarm);

            for (const auto* page : matchedPdfPages) {
                std::vector<int> autoSeq = ExtractKeyIdsFromPdfText(page->searchNormalized, page->calcIdx);
                const int nKeys = (int)autoSeq.size();
                const int singleRowBoxW = (nKeys > 0) ? (availW - 44 - (nKeys - 1) * 12) / nKeys : 54;
                const bool stripTwoRows = (singleRowBoxW < 40 && nKeys > 1);
                const int cardH = stripTwoRows ? 202 : 154;
                int screenY = HELP_TOP_FIXED_H + y - g_smartHelpScrollY;
                RECT cardRc = { cardLeft, screenY, cardRight, screenY + cardH };

                RECT rcExpBtn = { cardRight - 226, screenY + 10, cardRight - 114, screenY + 36 };
                RECT rcPdfBtn = { cardRight - 106, screenY + 10, cardRight - 12,  screenY + 36 };

                HelpHitCard hit{};
                hit.rcCard = cardRc;
                hit.rcExportGuide = rcExpBtn;
                hit.rcOpenPdf = rcPdfBtn;
                hit.pdfFile = page->fileName;
                hit.pageNo = page->pageNo;
                hit.calcIdx = page->calcIdx;
                hit.buttonSeq = autoSeq;
                hit.title = page->modelCat + L" - " + page->heading;
                hit.explanation = page->snippet;
                if (screenY + cardH >= HELP_TOP_FIXED_H && screenY <= rc.bottom) {
                    g_helpHitCards.push_back(hit);
                }

                if (screenY + cardH >= HELP_TOP_FIXED_H && screenY <= rc.bottom) {
                    CasioTheme::FillRoundRectAA(hdc, cardRc, 9.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorder(), 1.0f);

                    CasioTheme::DrawDotIndicator(hdc, cardLeft + 16, screenY + 20, 4, CasioTheme::StudioAmber());
                    RECT rIdxLbl = { cardLeft + 25, screenY + 10, cardLeft + 110, screenY + 29 };
                    SelectObject(hdc, hUiMetaF);
                    SetTextColor(hdc, CasioTheme::StudioAmberText());
                    DrawTextW(hdc, L"\u00CDndice PDF", -1, &rIdxLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                    std::wstring headLeft = page->modelCat + L" \x00B7 " + page->docType + L" \x2014 " + page->heading;
                    RECT rH = { cardLeft + 114, screenY + 8, cardRight - 236, screenY + 32 };
                    DrawLatexRichText(hdc, headLeft, rH, CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 13, true);

                    CasioTheme::DrawStudioButton(hdc, rcExpBtn, L"Exportar Gu\u00EDa", hUiMetaF, CasioTheme::BTN_VIOLET,
                                                 false, PtInRect(&rcExpBtn, g_smartHelpHoverPt) != FALSE, 7.0f);
                    std::wstring pdfLbl = L"PDF P\u00E1g. " + std::to_wstring(page->pageNo);
                    CasioTheme::DrawStudioButton(hdc, rcPdfBtn, pdfLbl, hUiMetaF, CasioTheme::BTN_PRIMARY,
                                                 false, PtInRect(&rcPdfBtn, g_smartHelpHoverPt) != FALSE, 7.0f);

                    RECT rSnip = { cardLeft + 14, screenY + 36, cardRight - 14, screenY + 72 };
                    DrawLatexRichText(hdc, page->snippet, rSnip, CasioTheme::StudioTextSec(), CasioTheme::StudioAccentText(), 13, false);

                    RECT rcStrip = { cardLeft + 14, screenY + 74, cardRight - 14, screenY + cardH - 8 };
                    DrawSmartHelpButtonSequenceStrip(hdc, page->calcIdx, autoSeq, rcStrip);
                }
                y += cardH + 8;
            }
        };

        if (tokens.empty()) {
            renderManualCardsSection();
            renderCuratedGuidesSection();
            renderPdfPagesSection();
        } else {
            renderCuratedGuidesSection();
            renderPdfPagesSection();
            renderManualCardsSection();
        }

        if (matchedManuals.empty() && matchedCurated.empty() && matchedPdfPages.empty()) {
            RECT rcEmpty = { cardLeft, HELP_TOP_FIXED_H + 36, cardRight, HELP_TOP_FIXED_H + 100 };
            CasioTheme::FillRoundRectAA(hdc, rcEmpty, 10.0f, CasioTheme::StudioSurface(), CasioTheme::StudioBorder(), 1.0f);
            SelectObject(hdc, hUiCardTF);
            SetTextColor(hdc, CasioTheme::StudioTextSec());
            DrawTextW(hdc, L"No se encontraron coincidencias para la b\u00FAsqueda actual.", -1, &rcEmpty,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            y += 120;
        }

        RestoreDC(hdc, savedDC);

        int viewH = (std::max)(1, (int)rc.bottom - HELP_TOP_FIXED_H);
        SCROLLINFO si{};
        si.cbSize = sizeof(si);
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = (std::max)(0, y);
        si.nPage = viewH;
        si.nPos = g_smartHelpScrollY;
        SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
        } // UiScaleScope

        BitBlt(hdcWin, 0, 0, rcPhys.right, rcPhys.bottom, hdc, 0, 0, SRCCOPY);
        DeleteObject(hUiSecF);
        DeleteObject(hUiCardTF);
        DeleteObject(hUiMetaF);
        DeleteObject(hUiSmallF);
        SelectObject(hdc, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(hdc);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_DESTROY:
        g_hSmartSearchEdit = nullptr;
        g_hSmartHelpWnd = nullptr;
        if (g_hSmartSearchFont) { DeleteObject(g_hSmartSearchFont); g_hSmartSearchFont = nullptr; }
        g_smartSearchFontPx = 0;
        return 0;

    case WM_CLOSE:
        ShowWindow(hWnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void EnsureSmartHelpRegistered()
{
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SmartHelpWndProc;
    wc.hInstance = g_hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CasioTheme::GetStudioSurfaceBrush();
    wc.lpszClassName = L"CasioSmartHelpWnd";
    RegisterClassExW(&wc);
    registered = true;
}

static void OpenSmartHelpWindow(int initialCategory)
{
    static float s_lastSmartHelpScale = 0.0f;
    EnsureSmartHelpRegistered();
    if (initialCategory >= 0 && initialCategory <= 4) {
        g_smartHelpCategory = initialCategory;
    }
    if (g_hSmartHelpWnd && IsWindow(g_hSmartHelpWnd)) {
        const float activeScale = GetSmartHelpUiScale(g_hSmartHelpWnd);
        if (s_lastSmartHelpScale - activeScale > 0.01f || activeScale - s_lastSmartHelpScale > 0.01f) {
            MONITORINFO mi{};
            mi.cbSize = sizeof(mi);
            mi.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
            HMONITOR hMon = MonitorFromWindow(g_hSmartHelpWnd, MONITOR_DEFAULTTONEAREST);
            if (hMon) GetMonitorInfoW(hMon, &mi);
            const int workW = mi.rcWork.right - mi.rcWork.left;
            const int workH = mi.rcWork.bottom - mi.rcWork.top;
            const int nw = (std::min)(CasioTheme::UiToDevice(1140, activeScale), (std::max)(640, workW - 32));
            const int nh = (std::min)(CasioTheme::UiToDevice(780, activeScale), (std::max)(480, workH - 32));
            SetWindowPos(g_hSmartHelpWnd, nullptr, 0, 0, nw, nh, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            s_lastSmartHelpScale = activeScale;
        }
        SyncSmartHelpSearchEdit(g_hSmartHelpWnd);
        CasioTheme::ApplyDwmDarkTitleBar(g_hSmartHelpWnd, CasioTheme::g_studioDarkTheme);
        ShowWindow(g_hSmartHelpWnd, SW_SHOWNORMAL);
        SetForegroundWindow(g_hSmartHelpWnd);
        InvalidateRect(g_hSmartHelpWnd, nullptr, TRUE);
        if (g_hSmartSearchEdit) SetFocus(g_hSmartSearchEdit);
        return;
    }

    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    mi.rcWork = { 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    HMONITOR hMon = (g_hMainWnd && IsWindow(g_hMainWnd))
        ? MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST)
        : MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    if (hMon) GetMonitorInfoW(hMon, &mi);
    const int scrW = mi.rcWork.right - mi.rcWork.left;
    const int scrH = mi.rcWork.bottom - mi.rcWork.top;
    const float dlgScale = GetSmartHelpUiScale(g_hMainWnd);
    int w = (std::min)(CasioTheme::UiToDevice(1140, dlgScale), (std::max)(640, scrW - 32));
    int h = (std::min)(CasioTheme::UiToDevice(780, dlgScale), (std::max)(480, scrH - 32));
    const int leftX = mi.rcWork.left + (std::max)(16, (scrW - w) / 2);
    const int topY  = mi.rcWork.top  + (std::max)(16, (scrH - h) / 2);

    g_hSmartHelpWnd = CreateWindowExW(
        0,
        L"CasioSmartHelpWnd",
        L"Ayuda y Manuales CASIO",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VSCROLL | WS_VISIBLE,
        leftX, topY, w, h,
        g_hMainWnd, nullptr, g_hInst, nullptr);
    s_lastSmartHelpScale = GetSmartHelpUiScale(g_hSmartHelpWnd);
    SyncSmartHelpSearchEdit(g_hSmartHelpWnd);

    CasioTheme::ApplyDwmDarkTitleBar(g_hSmartHelpWnd, CasioTheme::g_studioDarkTheme);
    ShowWindow(g_hSmartHelpWnd, SW_SHOW);
    UpdateWindow(g_hSmartHelpWnd);
    if (g_hSmartSearchEdit) SetFocus(g_hSmartSearchEdit);
}

inline void OpenSmartHelpWindow(const std::wstring& initialQuery)
{
    g_smartSearchQuery = initialQuery;
    OpenSmartHelpWindow(-1);
    if (g_hSmartSearchEdit && !initialQuery.empty()) {
        SetWindowTextW(g_hSmartSearchEdit, initialQuery.c_str());
    }
}
