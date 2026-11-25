// ============================================================================
// CALCULADORAS UNIFICADAS — ESTADO TÉCNICO VERIFICABLE
// C++/Win32 | Emulación nX-U8 para dos modelos científicos
// La fx-CG50 permanece deshabilitada hasta integrar un núcleo SH7305/SH-4A real.
// ============================================================================

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <algorithm>


#include "resource_ids.h"
#include "calcemu.h"
#include "cg50_backend.h"
#include "cg50_live_comparator.h"
#include "fx991cw_host.h"
#include "classwiz_keys_exact.h"
#include "gray_keys_exact.h"
#include "cg50_keys_exact.h"
#include "cg50_selector_geometry.h"
#include "cw2_keys_exact.h"
#include "debug_log.h"
#include "operation_capture.h"
#include "ui_theme.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "msimg32.lib")

// ── Modern Graphite Studio Palette (from ui_theme.h) ────────────────────────
static constexpr COLORREF CLR_WINDOW_BG   = CasioTheme::CanvasBg;
static constexpr COLORREF CLR_SIDEBAR     = CasioTheme::Surface1;
static constexpr COLORREF CLR_PANEL_BG    = CasioTheme::StageBg;
static constexpr COLORREF CLR_CARD_BG     = CasioTheme::Surface2;
static constexpr COLORREF CLR_BORDER      = CasioTheme::BorderSubtle;
static constexpr COLORREF CLR_TEXT_WHITE  = CasioTheme::TextPrimary;
static constexpr COLORREF CLR_TEXT_MUTED  = CasioTheme::TextSecondary;
static constexpr COLORREF CLR_ACCENT_CYAN = CasioTheme::AccentHover;
static constexpr COLORREF CLR_ACCENT_BLUE = CasioTheme::AccentBlue;

// Precise LCD Backgrounds & Foregrounds
static constexpr COLORREF CLR_LCD_CW_BG   = RGB(213, 226, 213); // ClassWiz official olive-tint LCD
static constexpr COLORREF CLR_LCD_CW_FG   = RGB(28, 38, 56);
static constexpr COLORREF CLR_LCD_ES_BG   = RGB(214, 227, 214); // Natural ES official olive-tint LCD
static constexpr COLORREF CLR_LCD_ES_FG   = RGB(25, 35, 50);
static constexpr COLORREF CLR_LCD_CG_BG   = RGB(255, 255, 255); // CG50 Color LCD Canvas
static constexpr COLORREF CLR_LCD_CG_FG   = RGB(15, 20, 30);

static constexpr int SIDEBAR_L_W = 0;   // Left chrome removed in Studio redesign
static constexpr int SIDEBAR_R_W = 356; // Contextual Aprender drawer width (logical px)
static constexpr int MIN_WND_W   = 960;
static constexpr int MIN_WND_H   = 640;
static constexpr int EMULATOR_TOOLBAR_H = 36;
static constexpr int STUDIO_TOOLBAR_H   = 52; // Unified top header height (logical px)

enum CtrlId : UINT_PTR {
    ID_BTN_MODE_ALL    = 1010,
    ID_BTN_MODE_DUAL   = 1011,
    ID_BTN_MODE_WHITE  = 1012,
    ID_BTN_MODE_GRAY   = 1013,
    ID_BTN_MODE_GRAPH  = 1014,
    ID_BTN_ZOOM_IN     = 1015,
    ID_BTN_ZOOM_OUT    = 1016,
    ID_BTN_ZOOM_RESET  = 1017,
    ID_BTN_CLEAR_HIST  = 1021,
    ID_BTN_COPY_LAST   = 1022,
    ID_BTN_OPEN_CAPTURES = 1023,
    ID_BTN_COMPACT_MODE  = 1024,
    ID_BTN_POPUP_DISP    = 1025,
    ID_BTN_SMART_HELP    = 1026,
    ID_CMD_STUDIO_MODE   = 1030,
    ID_CMD_ALWAYS_ON_TOP = 1031,
    ID_CMD_CAPTURE_LCD   = 1032,
    ID_CMD_ZOOM_X1       = 1033,
    ID_CMD_ZOOM_X2       = 1034,
    ID_CMD_ZOOM_X3       = 1035,
    ID_CMD_POPUP_GRID    = 1036,
    ID_CMD_KEYLOG_VIEW   = 1037,
    ID_CMD_CAPTURES_VIEW = 1038,
    ID_CMD_QR_DISPLAY    = 1039,
    ID_CMD_OPEN_MANUALS_FOLDER = 1040,
    ID_CMD_SETTINGS          = 1041,
    ID_CMD_EXPORTER          = 1042,
    ID_CMD_SAVE_RAM_STATE    = 1043,
    ID_CMD_RESTORE_RAM_STATE = 1044,
    ID_CMD_TEACHER_MODE      = 1045,
    ID_CMD_SMART_COPY        = 1046,
    ID_BTN_KEY_EXPLORER      = 1047,
    ID_BTN_SHIFT_ALPHA_LAYERS = 1048,
    ID_BTN_LEARNING_STUDIO   = 1049,
    ID_CMD_COMPARE_MODELS    = 1050,
    ID_CMD_TROUBLESHOOTER    = 1051,
    ID_CMD_CHECKLIST_PRESETS = 1052,
    ID_CMD_PLAYER_TAB        = 1053,
    ID_CMD_STUDIO_MENU       = 1054,
    ID_CMD_UISCALE_AUTO      = 1055,
    ID_CMD_UISCALE_100       = 1056,
    ID_CMD_UISCALE_125       = 1057,
    ID_CMD_UISCALE_150       = 1058,
    ID_CMD_UISCALE_175       = 1059,
    ID_CMD_UISCALE_200       = 1060,
    ID_BTN_MODE_CW2          = 1061,
    ID_CMD_CW2_RESTART       = 1062,
    ID_CMD_CW2_RESET_RAM     = 1063,
    ID_CMD_WORKSPACE_CALC    = 1064,
    ID_CMD_WORKSPACE_COMPARE = 1065,
    ID_CMD_TOGGLE_APRENDER   = 1066,
    ID_CMD_CLOSE_APRENDER    = 1067,
    ID_CMD_TOGGLE_THEME      = 1068,
    ID_CMD_OPEN_DOC_STUDIO   = 1069,
    ID_CMD_SEND_TO_DOC_STUDIO = 1070,
    ID_CMD_APPEAR_LIGHT      = 1071,
    ID_CMD_APPEAR_DARK       = 1072,
    ID_CMD_APPEAR_HC         = 1073,
    ID_CMD_GLASS_DESIGN      = 1074,
    ID_CMD_GLASS_BALANCED    = 1075,
    ID_CMD_GLASS_FROSTED     = 1076,
    ID_CMD_GLASS_REDUCE_TRANS = 1077,
    ID_CMD_CG50_MODE_NATIVE  = 1078,
    ID_CMD_CG50_MODE_OFFICIAL = 1079,
    ID_CMD_CG50_COMPARE_LIVE = 1080,
    ID_CMD_PDF_BASE      = 1100,
    ID_TIMER_TICK      = 2001,
    ID_TIMER_CAPTURE_OP = 2002,
    ID_TIMER_AUTO_SHOT  = 2003
};

static std::wstring g_cliScreenshotOutPath;
static int g_cliScreenshotTargetKind = 0; // 0=Main, 1=Help, 2=Studio, 3=Settings, 4=Exporter, 5=PdfViewer

enum ViewMode {
    VIEW_ALL_THREE = 0,
    VIEW_DUAL_SCIENTIFIC = 1,
    VIEW_ONLY_WHITE = 2,
    VIEW_ONLY_GRAY = 3,
    VIEW_ONLY_GRAPH = 4,
    VIEW_ONLY_CW2 = 5
};

// ── Native KeyLog Data ──────────────────────────────────────────────────────
struct KeyLogEntry {
    int calcIdx;
    int keyId;
    std::wstring keyName;
    bool isAppCapture = false;
    std::wstring captureFilePath;
    std::wstring captureLabel;
};

enum class BackendKind {
    NxU8,
    Cg50
};

// ── Standalone Native Calculator Model ──────────────────────────────────────
struct NativeCalc {
    std::wstring displayName;
    std::wstring subTitle;
    HBITMAP      hSkinBmp = nullptr;
    HBITMAP      hScaledSkinBmp = nullptr;
    int          cachedSkinW = 0;
    int          cachedSkinH = 0;
    std::vector<std::uint8_t> skinPargb;
    int          skinW = 0;
    int          skinH = 0;
    
    // Exact LCD Rect in skin coordinates
    RECT         lcdSkinRect;
    COLORREF     lcdBgColor;
    COLORREF     lcdFgColor;

    // Genuine CPU Emulator instance (ClassWiz & Natural ES)
    CalcEmu*     emu = nullptr;
    std::vector<unsigned char> romData;
    int          lcdW = 0;
    int          lcdH = 0;
    std::vector<unsigned char> lcdPixels;
    std::vector<std::uint32_t> lcdRgb;

    BackendKind  backendKind = BackendKind::NxU8;
    std::wstring backendName;
    std::wstring backendDiagnostic;
    std::unique_ptr<Cg50Backend> cg50;

    int          hoverKey = -1;
    int          pressedKey = -1;
    bool         isVisible = true;

    // Render viewport & pedestal stage bounds
    RECT         viewportRect;
    RECT         stageRect;
};

// ── Globals ─────────────────────────────────────────────────────────────────
static HINSTANCE      g_hInst;
static HWND           g_hMainWnd;
static HWND           g_hCanvasWnd;
static HWND           g_hHistoryWnd;

static HFONT          g_fontHeader;
static HFONT          g_fontNormal;
static HFONT          g_fontBold;
static HFONT          g_fontSmall;
static HFONT          g_fontLcdTop;
static HFONT          g_fontLcdBottom;
static HFONT          g_fontLcdBadge;
static HFONT          g_fontCg50Status;
static HFONT          g_fontCg50Softkey;

static HBRUSH         g_brWindowBg;
static HBRUSH         g_brSidebar;
static HBRUSH         g_brPanelBg;
static HBRUSH         g_brCardBg;
static HBRUSH         g_brBorder;

inline void SyncStudioThemeBrushes()
{
    auto swapBrush = [](HBRUSH& dst, COLORREF col, HWND hClassWnd) {
        HBRUSH nb = CreateSolidBrush(col);
        if (!nb) return;
        HBRUSH old = dst;
        dst = nb;
        if (hClassWnd && IsWindow(hClassWnd)) {
            SetClassLongPtrW(hClassWnd, GCLP_HBRBACKGROUND, reinterpret_cast<LONG_PTR>(nb));
        }
        if (old) DeleteObject(old);
    };
    swapBrush(g_brWindowBg, CasioTheme::StudioCanvasBg(), g_hMainWnd);
    swapBrush(g_brSidebar,  CasioTheme::StudioSurface(),  g_hHistoryWnd);
    swapBrush(g_brPanelBg,  CasioTheme::StudioCanvasBg(), g_hCanvasWnd);
    swapBrush(g_brCardBg,   CasioTheme::StudioSurfaceAlt(), nullptr);
    swapBrush(g_brBorder,   CasioTheme::StudioBorder(), nullptr);
}

static NativeCalc     g_nativeCalcs[4];
static ViewMode       g_viewMode = VIEW_ONLY_CW2;
static std::vector<KeyLogEntry> g_keyHistory;
static int            g_historyScrollY = 0;
static bool           g_historyFollowTail = true;
static int            g_activeCalcIdx = 3;
static double         g_userZoomFactor = 0.0; // 0.0 means Auto-Fit
static ULONGLONG      g_lastTickMs = 0;
static bool           g_cg50InitTried = false;
static bool           g_cg50InitRunning = false;

static int            g_pendingCaptureCalcIdx = -1;
static std::wstring   g_pendingCaptureCalcName;
static std::vector<OperationCaptureItem> g_operationCaptures;
static int            g_historyViewTab = 0; // 0 = Teclas, 1 = Capturas
static bool           g_compactSingleMode = false;
static bool           g_alwaysOnTop = false;
static RECT           g_savedStudioRect{};
static ViewMode       g_savedStudioViewMode = VIEW_ONLY_CW2;
static RECT           g_cw2StageRect{};
static RECT           g_cw2ViewportRect{};
static ULONGLONG      g_lastCw2PopupRefreshMs = 0;
static int            g_soloModelIdx = 3;                // Default protagonist: fx-991CW
static ViewMode       g_compareViewMode = VIEW_ALL_THREE;// Last comparison mode (2 or 4)
static bool           g_learningDrawerOpen = false;      // Contextual 'Aprender' drawer state
static bool           g_savedLearningDrawerOpen = false;

static constexpr int COMPACT_BASE_W = 454;
static constexpr int COMPACT_TAB_W = 20;
static constexpr int COMPACT_DRAWER_W = 264;
static bool          g_compactStepsOpen = false;
static int           g_compactStepsScrollY = 0;
static int           g_compactStepsContentH = 0;
static RECT          g_compactPullTabRc{};
static RECT          g_compactDrawerShareRc{};
static RECT          g_compactDrawerClearRc{};
static RECT          g_compactDrawerScrollAreaRc{};

static constexpr int ID_QUICK_COMPACT_BASE = 1800; // +0..3
static constexpr int ID_QUICK_PIN_BASE     = 1810; // +0..3
static constexpr int ID_COMPACT_PULL_TAB   = 1820;
static constexpr int ID_COMPACT_SHARE      = 1821;
static constexpr int ID_COMPACT_CLEAR      = 1822;
static RECT          g_calcQuickCompactLogRc[4]{};
static RECT          g_calcQuickPinLogRc[4]{};

static HWND          g_hCw2OverlayWnd = nullptr;
static int           g_cw2HoverKey = -1;
static int           g_cw2OverlayLastHover = -2;
static int           g_cw2OverlayLastKeyId = -2;
static int           g_cw2OverlayLastStepCount = -1;
static bool          g_cw2OverlayLastTeacher = false;
static RECT          g_cw2OverlayLastScreenRc{};

struct CompactDrawerStepHit {
    RECT rc;
    int histIdx;
    bool isAppCapture;
    std::wstring captureFilePath;
};
static std::vector<CompactDrawerStepHit> g_compactDrawerStepHits;
static HWND          g_hCompactToastWnd = nullptr;
static std::wstring  g_compactToastLastMsg;
static RECT          g_compactToastLastRect{};
static bool          g_compactToastLastActive = false;

static bool          g_inAppMenuState[4] = { false, false, true, true };
static bool          g_awaitingSubmenuNav[4] = { false, false, false, false };
static ULONGLONG     g_lastPreEntryCapMs[4] = { 0, 0, 0, 0 };
static int           g_cw2SuppressParentNotifyForKey = -1;
static ULONGLONG     g_cw2SuppressParentNotifyUntil = 0;

inline bool IsCompareViewMode(ViewMode vm)
{
    return vm == VIEW_ALL_THREE || vm == VIEW_DUAL_SCIENTIFIC;
}

inline void SetLearningDrawerOpen(bool open);
inline void RelayoutMainPanes(HWND hMain = nullptr);
inline void InvalidateSidebarButtons();
void LayoutCalculators();

inline bool IsCw2Visible()
{
    if (g_compactSingleMode) return g_activeCalcIdx == 3;
    return (g_viewMode == VIEW_ALL_THREE || g_viewMode == VIEW_ONLY_CW2);
}

static RECT  g_studioMenuBtnLogicalRc{};
static int   g_canvasHoverBtnId = 0; // command/action ID hovered in canvas top bar
static bool  g_canvasTrackingMouse = false;
static HWND  g_hCanvasTooltip = nullptr;
static int   g_lastCanvasTipId = -99;

inline int GetSidebarLeftPhysW(int /*mainClientW*/ = -1)
{
    return 0;
}

inline int GetSidebarRightPhysW(int mainClientW = -1)
{
    if (g_compactSingleMode || !g_learningDrawerOpen) return 0;
    int desired = CasioTheme::UiToDevice(SIDEBAR_R_W);
    if (mainClientW > 0) {
        int cap = (std::max)(280, mainClientW / 3);
        return (std::min)(desired, cap);
    }
    return desired;
}

inline float GetSidebarUiScale(int /*mainClientW*/)
{
    return CasioTheme::g_uiScale;
}

inline float GetCanvasUiScale(int physCanvasW)
{
    float s = CasioTheme::g_uiScale;
    constexpr int minLogicalNeed = 700;
    if (physCanvasW > 0 && physCanvasW < CasioTheme::UiToDevice(minLogicalNeed, s)) {
        s = (std::max)(1.0f, static_cast<float>(physCanvasW) / static_cast<float>(minLogicalNeed));
    }
    return s;
}

inline int GetCanvasToolbarH()
{
    if (g_compactSingleMode) return EMULATOR_TOOLBAR_H;
    RECT rcC{};
    int cw = (g_hCanvasWnd && GetClientRect(g_hCanvasWnd, &rcC)) ? rcC.right : 0;
    return CasioTheme::UiToDevice(STUDIO_TOOLBAR_H, GetCanvasUiScale(cw));
}

// ── Forward Declarations ────────────────────────────────────────────────────
LRESULT CALLBACK MainWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK CanvasWndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK HistoryWndProc(HWND, UINT, WPARAM, LPARAM);
void ReleaseKeyCaptures();

void InitNativeCalcs();
void HandleKeyPress(int calcIdx, int keyId);
void AppendKeyHistory(int calcIdx, int keyId);
std::wstring BuildKeySequence(int calcIdx);
void LayoutCalculators();
void DrawCalculator(HDC hdc, int idx, const RECT& dstRect);
void DrawLcdDisplay(HDC hdc, int idx, const RECT& dstLcd);
void DrawCg50Screen(HDC hdc, const RECT& dstLcd);
void DrawCg50UnavailableScreen(HDC hdc, const RECT& dstLcd);
void TryInitCg50Backend();
int  HitTestKey(int calcIdx, int mouseX, int mouseY);
const FullKeyDef* GetKeyDef(int calcIdx, int keyId);
int  GetKeyCount(int calcIdx);
void HandlePhysicalKeyDown(WPARAM vk);
void HandlePhysicalKeyUp(WPARAM vk = 0);
void HandleNativeStateHotkeys(WPARAM vk);

#include "manual_catalog.h"
#include "latex_math_renderer.h"
#include "lcd_indicators.h"
#include "knowledge_catalog.h"

enum ClipboardCopyMode {
    CLIP_COPY_IMAGE_ONLY = 0,
    CLIP_COPY_TEXT_ONLY = 1,
    CLIP_COPY_BOTH_IMAGE_AND_TEXT = 2
};
inline int  g_clipCopyMode = CLIP_COPY_BOTH_IMAGE_AND_TEXT;
inline bool g_autoCopyOnEvaluate = false;
inline bool g_includeLatexInTextCopy = true;
inline bool g_copyFullBodyInsteadOfLcd = false;
inline bool g_teacherHighlightMode = true;
inline bool g_exportSheetLightTheme = true;
inline bool g_shiftAlphaLayerMode = false;
inline bool g_keyExplorerEnabled = true;
inline bool g_explorerPinned = false;
inline std::wstring g_clipStatusText;

inline void ComputeLcdIndAndDotRects(int idx, const RECT& dstLcd, RECT& outIndRect, RECT& outDotRect)
{
    const int dstLcdW = dstLcd.right - dstLcd.left;
    const int dstLcdH = dstLcd.bottom - dstLcd.top;
    const int nativeIndH   = (idx == 1) ? 12 : 8;
    const int nativePadTop = (idx == 1) ? 2  : 1;
    const int nativeGapY   = (idx == 1) ? 3  : 2;
    const int nativeTotalH = (idx == 1) ? 111 : 75;
    const int nativeW      = (idx == 1) ? 288 : 192;
    const int rows         = (idx == 1) ? 31 : 63;

    int padTop = 0, indH = 0, gapY = 0, padBot = 0;
    if (dstLcdW > 0 && dstLcdH > 0 &&
        (dstLcdW % nativeW) == 0 && (dstLcdH % nativeTotalH) == 0 &&
        (dstLcdW / nativeW) == (dstLcdH / nativeTotalH)) {
        const int s = dstLcdH / nativeTotalH;
        padTop = nativePadTop * s;
        indH   = nativeIndH   * s;
        gapY   = nativeGapY   * s;
        padBot = 1 * s;
    } else {
        padTop = MulDiv(nativePadTop, dstLcdH, nativeTotalH);
        indH   = (std::max)(nativeIndH, MulDiv(nativeIndH, dstLcdH, nativeTotalH));
        gapY   = (std::max)(1, MulDiv(nativeGapY, dstLcdH, nativeTotalH));
        padBot = MulDiv(1, dstLcdH, nativeTotalH);
        if (padTop + indH + gapY + rows > dstLcdH) {
            padTop = (std::max)(0, dstLcdH / nativeTotalH);
            indH   = (std::max)(6, MulDiv(nativeIndH, dstLcdH, nativeTotalH));
            gapY   = (std::max)(1, MulDiv(nativeGapY, dstLcdH, nativeTotalH));
            padBot = 0;
        }
    }
    outIndRect = { dstLcd.left, dstLcd.top + padTop, dstLcd.right, dstLcd.top + padTop + indH };
    outDotRect = {
        dstLcd.left,
        outIndRect.bottom + gapY,
        dstLcd.right,
        (std::max)(outIndRect.bottom + gapY + 1, dstLcd.bottom - padBot)
    };
}

static void LoadUnifiedSettingsFromIni();
static void SaveUnifiedSettingsToIni();
static void CopyCalculatorResultSmart(int calcIdx, int modeOverride = -1, bool updateStatus = true);
static bool SaveCalculatorSnapshotState(int calcIdx);
static bool RestoreCalculatorSnapshotState(int calcIdx);
static void LoadCustomSequenceIntoExporter(int calcIdx, const std::vector<int>& keys, const std::wstring& title, const std::wstring& notes);
static void OpenSettingsWindow();
static void OpenSequenceExporterWindow(bool syncHistoryFirst = true);
static void OpenOfficialPdfManual(const std::wstring& pdfFileName, int pageNo = 1);
static void OpenOfficialManualsFolder();
inline bool LaunchDocumentStudio(HWND owner, const std::wstring& args = L"");
static void OpenSmartHelpWindow(int initialCategory = -1);
inline void OpenSmartHelpWindow(const std::wstring& initialQuery);
static HBITMAP CaptureLcdSnapshot(int calcIdx, int zoomScale, int& outW, int& outH);
static void ShowToolbarToast(const std::wstring& msg);
inline void DrawKeyLayersOverlay(HDC hdc, int calcIdx, const RECT& dstRect);
inline void DrawExplorerCardOverlay(HDC hdc, const RECT& canvasRc);
inline bool HandleExplorerOverlayClick(int mx, int my);
inline bool IsPointInsideExplorerCard(int mx, int my);
inline void OpenExplorerForKey(int calcIdx, int keyId, bool pinCard, int layer);
inline void OpenExplorerForIndicator(int calcIdx, int spriteIndex, bool isLit, bool pinCard);
inline void ShowCalculatorContextMenu(HWND hwndOwner, int clientX, int clientY, int calcIdx, int keyId, int indSpriteIdx, bool indLit, bool isLcdBody, int timelineStepIdx);
inline void OpenLearningStudioWindow(int initialTab, int calcIdx, int focusKeyId, const std::wstring& searchQuery);
inline void SyncSecondaryWindowsTheme();
inline HWND ResolveSecondaryScreenshotTarget(int targetKind);
inline void ClearTimelineSnapshots();
inline void RecordTimelineStep(int calcIdx, int keyId);
inline bool GetTimelinePlayerActiveKey(int calcIdx, int& outKeyId, int& outStepOneBased);
int HitTestCw2SkinKey(int skinX, int skinY);
void RecordCw2KeyActivation(int keyId);
static const wchar_t* KeyHistoryCaption(int calcIdx, int keyId);
static std::wstring BuildLiveHistoryLatexPreview(int calcIdx);
inline void SyncExporterStepsFromHistory(int filterCalcIdx = -1);
static void DrawPadlockIconGdi(HDC hdc, const RECT& rc, bool locked, COLORREF color);
static void DrawCompactQuickIconGdi(HDC hdc, const RECT& rc, COLORREF color);
static void ToggleCompactStepsDrawer(bool open);
static void SyncCw2HoverOverlay(bool force = false);
static void PollCw2HoverKey();
static void DestroyCw2HoverOverlay();
static void SyncCompactToastOverlay(bool force = false);
static void DestroyCompactToastOverlay();
static void MaybeCapturePreEntryAppSnapshot(int calcIdx, int keyId);
inline bool DrawHistoryKeyThumbInRect(HDC hdc, int calcIdx, int keyId, const RECT& rSlot, HDC memKeyDc);

#include "popup_display.h"

static void UpdateWindowTitleForMode()
{
    if (!g_hMainWnd) return;
    if (g_compactSingleMode) {
        if (g_activeCalcIdx == 0) SetWindowTextW(g_hMainWnd, L"fx-570/991LA X Emulator");
        else if (g_activeCalcIdx == 1) SetWindowTextW(g_hMainWnd, L"fx-570VN / ES PLUS Emulator");
        else if (g_activeCalcIdx == 2) SetWindowTextW(g_hMainWnd, L"fx-CG50 PRIZM Emulator");
        else SetWindowTextW(g_hMainWnd, L"fx-991CW ClassWiz II Emulator");
    } else {
        SetWindowTextW(g_hMainWnd, L"CASIO \u2014 Calculadoras");
    }
}

static void SetAlwaysOnTopMode(bool enable)
{
    g_alwaysOnTop = enable;
    g_lastCanvasTipId = -99;
    if (g_hMainWnd) {
        SetWindowPos(g_hMainWnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST,
                     0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    SyncCw2HoverOverlay(true);
}

static HMENU CreateEmulatorMenuBar();

inline void RelayoutMainPanes(HWND hMain)
{
    HWND hTarget = hMain ? hMain : g_hMainWnd;
    if (!hTarget) return;
    RECT rc{}; GetClientRect(hTarget, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) return;
    if (g_compactSingleMode || !g_learningDrawerOpen) {
        if (g_hCanvasWnd) MoveWindow(g_hCanvasWnd, 0, 0, rc.right, rc.bottom, TRUE);
        if (g_hHistoryWnd) {
            if (IsWindowVisible(g_hHistoryWnd)) ShowWindow(g_hHistoryWnd, SW_HIDE);
            MoveWindow(g_hHistoryWnd, rc.right, 0, 0, rc.bottom, FALSE);
        }
    } else {
        const int sbR = GetSidebarRightPhysW(rc.right);
        const int cw  = (std::max)(200, static_cast<int>(rc.right) - sbR);
        if (g_hCanvasWnd)  MoveWindow(g_hCanvasWnd, 0, 0, cw, rc.bottom, TRUE);
        if (g_hHistoryWnd) {
            MoveWindow(g_hHistoryWnd, cw, 0, sbR, rc.bottom, TRUE);
            if (!IsWindowVisible(g_hHistoryWnd)) ShowWindow(g_hHistoryWnd, SW_SHOW);
        }
    }
    LayoutCalculators();
}

inline void SetLearningDrawerOpen(bool open)
{
    if (g_compactSingleMode && open) return;
    if (g_learningDrawerOpen == open) {
        if (g_hHistoryWnd) {
            bool vis = IsWindowVisible(g_hHistoryWnd) != FALSE;
            if (vis != open) RelayoutMainPanes();
        }
        return;
    }
    g_learningDrawerOpen = open;
    RelayoutMainPanes();
    InvalidateSidebarButtons();
    if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
    if (g_hHistoryWnd && open) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
}

inline void RefreshUiScale(HWND hWnd, bool forceRelayout = false)
{
    HWND hTarget = hWnd ? hWnd : g_hMainWnd;
    if (!hTarget) return;
    float nextScale = g_compactSingleMode ? 1.0f : CasioTheme::ComputeRecommendedUiScale(hTarget);
    bool changed = std::fabs(nextScale - CasioTheme::g_uiScale) > 0.001f;
    CasioTheme::g_uiScale = nextScale;
    if (changed || forceRelayout) {
        RelayoutMainPanes(hTarget);
        InvalidateRect(hTarget, nullptr, FALSE);
        if (g_hCanvasWnd)  InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
    }
}

static void SetCompactVerticalMode(bool enable, int targetCalcIdx = -1)
{
    if (!g_hMainWnd) return;

    if (enable) {
        if (!g_compactSingleMode) {
            GetWindowRect(g_hMainWnd, &g_savedStudioRect);
            g_savedStudioViewMode = g_viewMode;
            g_savedLearningDrawerOpen = g_learningDrawerOpen;
        }
        g_compactSingleMode = true;
        g_compactStepsOpen = false;
        g_compactStepsScrollY = 0;
        g_learningDrawerOpen = false;
        if (targetCalcIdx >= 0 && targetCalcIdx <= 3) {
            g_activeCalcIdx = targetCalcIdx;
        }
        if (g_activeCalcIdx < 0 || g_activeCalcIdx > 3) g_activeCalcIdx = 3;

        for (int i = 0; i < 3; ++i) {
            g_nativeCalcs[i].isVisible = (i == g_activeCalcIdx);
        }
        g_viewMode = (g_activeCalcIdx == 0) ? VIEW_ONLY_WHITE
                   : (g_activeCalcIdx == 1) ? VIEW_ONLY_GRAY
                   : (g_activeCalcIdx == 2) ? VIEW_ONLY_GRAPH : VIEW_ONLY_CW2;
        g_userZoomFactor = 0.0;

        if (!GetMenu(g_hMainWnd)) {
            SetMenu(g_hMainWnd, CreateEmulatorMenuBar());
        }
        if (g_hHistoryWnd) ShowWindow(g_hHistoryWnd, SW_HIDE);

        if (IsZoomed(g_hMainWnd)) ShowWindow(g_hMainWnd, SW_RESTORE);

        RECT rcCur{};
        GetWindowRect(g_hMainWnd, &rcCur);
        int compactW = COMPACT_BASE_W;
        int compactH = 815;
        int scrW = GetSystemMetrics(SM_CXSCREEN);
        int scrH = GetSystemMetrics(SM_CYSCREEN);
        int newX = max(20, min((int)rcCur.left, scrW - compactW - 20));
        int newY = max(20, min((int)rcCur.top, scrH - compactH - 40));

        UpdateWindowTitleForMode();
        CasioTheme::ApplyDwmDarkTitleBar(g_hMainWnd, false);
        SetWindowPos(g_hMainWnd, nullptr, newX, newY, compactW, compactH, SWP_NOZORDER | SWP_FRAMECHANGED);

        RefreshUiScale(g_hMainWnd, true);
        SyncCw2HoverOverlay(true);
        SyncCompactToastOverlay(true);
        InvalidateRect(g_hMainWnd, nullptr, TRUE);
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
    } else {
        if (!g_compactSingleMode) return;
        g_compactSingleMode = false;
        g_compactStepsOpen = false;
        g_compactStepsScrollY = 0;

        HMENU hCurMenu = GetMenu(g_hMainWnd);
        if (hCurMenu) {
            SetMenu(g_hMainWnd, nullptr);
            DestroyMenu(hCurMenu);
        }
        g_learningDrawerOpen = g_savedLearningDrawerOpen;
        if (g_hHistoryWnd) ShowWindow(g_hHistoryWnd, g_learningDrawerOpen ? SW_SHOW : SW_HIDE);

        g_viewMode = g_savedStudioViewMode;
        if (IsCompareViewMode(g_viewMode)) g_compareViewMode = g_viewMode;
        else g_soloModelIdx = (g_viewMode == VIEW_ONLY_WHITE) ? 0 : (g_viewMode == VIEW_ONLY_GRAY) ? 1 : (g_viewMode == VIEW_ONLY_GRAPH) ? 2 : 3;
        g_nativeCalcs[0].isVisible = (g_viewMode == VIEW_ALL_THREE || g_viewMode == VIEW_DUAL_SCIENTIFIC || g_viewMode == VIEW_ONLY_WHITE);
        g_nativeCalcs[1].isVisible = (g_viewMode == VIEW_ALL_THREE || g_viewMode == VIEW_DUAL_SCIENTIFIC || g_viewMode == VIEW_ONLY_GRAY);
        g_nativeCalcs[2].isVisible = (g_viewMode == VIEW_ALL_THREE || g_viewMode == VIEW_ONLY_GRAPH);
        if (g_viewMode == VIEW_ONLY_CW2) {
            g_activeCalcIdx = 3;
        } else if (g_activeCalcIdx == 3 && g_viewMode != VIEW_ALL_THREE) {
            g_activeCalcIdx = (g_viewMode == VIEW_ONLY_GRAY) ? 1 : (g_viewMode == VIEW_ONLY_GRAPH ? 2 : 0);
        }

        UpdateWindowTitleForMode();
        CasioTheme::ApplyDwmDarkTitleBar(g_hMainWnd, CasioTheme::g_studioDarkTheme);
        int w = g_savedStudioRect.right - g_savedStudioRect.left;
        int h = g_savedStudioRect.bottom - g_savedStudioRect.top;
        if (w < MIN_WND_W || h < MIN_WND_H) {
            int scrW = GetSystemMetrics(SM_CXSCREEN);
            int scrH = GetSystemMetrics(SM_CYSCREEN);
            w = max(MIN_WND_W, (int)(scrW * 0.92));
            h = max(MIN_WND_H, (int)(scrH * 0.90));
            SetWindowPos(g_hMainWnd, nullptr, (scrW - w) / 2, (scrH - h) / 2, w, h, SWP_NOZORDER | SWP_FRAMECHANGED);
        } else {
            SetWindowPos(g_hMainWnd, nullptr, g_savedStudioRect.left, g_savedStudioRect.top, w, h, SWP_NOZORDER | SWP_FRAMECHANGED);
        }

        RefreshUiScale(g_hMainWnd, true);
        SyncCw2HoverOverlay(true);
        SyncCompactToastOverlay(true);
        InvalidateRect(g_hMainWnd, nullptr, FALSE);
        if (g_hCanvasWnd) {
            InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            UpdateWindow(g_hCanvasWnd);
        }
        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
    }
}

static void ShowSpanishStudioMenuPopup(HWND hWndOwner, POINT screenPt)
{
    HMENU hRoot = CreatePopupMenu();
    if (!hRoot) return;

    auto chkFlag = [](bool cond) -> UINT {
        return MF_STRING | (cond ? MF_CHECKED : MF_UNCHECKED);
    };

    // 1. Ver
    HMENU hView = CreatePopupMenu();
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && !IsCompareViewMode(g_viewMode)), ID_CMD_WORKSPACE_CALC, L"Espacio: Calculadora (1 modelo)");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && IsCompareViewMode(g_viewMode)),  ID_CMD_WORKSPACE_COMPARE, L"Espacio: Comparar modelos");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_learningDrawerOpen),          ID_CMD_TOGGLE_APRENDER, L"Panel lateral: Historial");
    AppendMenuW(hView, chkFlag(CasioTheme::g_studioDarkTheme),                         ID_CMD_TOGGLE_THEME,    L"Tema oscuro");
    {
        const auto& gs = CasioTheme::GetActiveGlassSettings();
        HMENU hAppear = CreatePopupMenu();
        AppendMenuW(hAppear, chkFlag(gs.mode == UiGlass::AppearanceMode::Light),        ID_CMD_APPEAR_LIGHT,       L"Claro");
        AppendMenuW(hAppear, chkFlag(gs.mode == UiGlass::AppearanceMode::Dark),         ID_CMD_APPEAR_DARK,        L"Oscuro");
        AppendMenuW(hAppear, chkFlag(gs.mode == UiGlass::AppearanceMode::HighContrast), ID_CMD_APPEAR_HC,          L"Alto Contraste");
        AppendMenuW(hAppear, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hAppear, chkFlag(!gs.reduceTransparency && gs.userOpacitySlider <= 0.20f), ID_CMD_GLASS_DESIGN,      L"Transparencia: Alta (10%)");
        AppendMenuW(hAppear, chkFlag(!gs.reduceTransparency && gs.userOpacitySlider > 0.20f && gs.userOpacitySlider < 0.70f), ID_CMD_GLASS_BALANCED, L"Transparencia: Equilibrada (36%)");
        AppendMenuW(hAppear, chkFlag(!gs.reduceTransparency && gs.userOpacitySlider >= 0.70f), ID_CMD_GLASS_FROSTED,     L"Transparencia: Esmerilada (85%)");
        AppendMenuW(hAppear, chkFlag(gs.reduceTransparency),                                   ID_CMD_GLASS_REDUCE_TRANS,L"Reducir transparencia");
        AppendMenuW(hView, MF_POPUP, (UINT_PTR)hAppear, L"Apariencia");
    }
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_viewMode == VIEW_ALL_THREE),       ID_BTN_MODE_ALL,   L"Todas las calculadoras (4)\tF6");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_viewMode == VIEW_DUAL_SCIENTIFIC), ID_BTN_MODE_DUAL,  L"Blanca y Gris (2)\tF5");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_viewMode == VIEW_ONLY_WHITE),      ID_BTN_MODE_WHITE, L"Solo Blanca (fx-570LA X)");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_viewMode == VIEW_ONLY_GRAY),       ID_BTN_MODE_GRAY,  L"Solo Gris (fx-570VN PLUS)\tF3");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_viewMode == VIEW_ONLY_GRAPH),      ID_BTN_MODE_GRAPH, L"Solo Graficadora (fx-CG50)\tF4");
    AppendMenuW(hView, chkFlag(!g_compactSingleMode && g_viewMode == VIEW_ONLY_CW2),        ID_BTN_MODE_CW2,   L"Solo fx-991CW (ClassWiz II)\tF7");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, chkFlag(g_compactSingleMode),                                        ID_BTN_COMPACT_MODE,  L"Ventana compacta (1 calculadora)\tCtrl+M");
    AppendMenuW(hView, chkFlag(g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd)),  ID_BTN_POPUP_DISP,    L"Pantalla LCD flotante\tCtrl+P");
    AppendMenuW(hView, chkFlag(g_alwaysOnTop),                                              ID_CMD_ALWAYS_ON_TOP, L"Siempre visible\tCtrl+T");
    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);

    HMENU hUiScale = CreatePopupMenu();
    int effPct = static_cast<int>(std::lround(CasioTheme::g_uiScale * 100.0f));
    std::wstring autoLbl = L"Autom\u00E1tico (actual: " + std::to_wstring(effPct) + L"%)";
    AppendMenuW(hUiScale, chkFlag(CasioTheme::g_uiScaleMode == 0),   ID_CMD_UISCALE_AUTO, autoLbl.c_str());
    AppendMenuW(hUiScale, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hUiScale, chkFlag(CasioTheme::g_uiScaleMode == 100), ID_CMD_UISCALE_100,  L"100% (Compacto)");
    AppendMenuW(hUiScale, chkFlag(CasioTheme::g_uiScaleMode == 125), ID_CMD_UISCALE_125,  L"125%");
    AppendMenuW(hUiScale, chkFlag(CasioTheme::g_uiScaleMode == 150), ID_CMD_UISCALE_150,  L"150%");
    AppendMenuW(hUiScale, chkFlag(CasioTheme::g_uiScaleMode == 175), ID_CMD_UISCALE_175,  L"175%");
    AppendMenuW(hUiScale, chkFlag(CasioTheme::g_uiScaleMode == 200), ID_CMD_UISCALE_200,  L"200% (Pantallas 4K)");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hUiScale, L"Tama\u00F1o de interfaz");

    HMENU hCalcZoom = CreatePopupMenu();
    AppendMenuW(hCalcZoom, MF_STRING, ID_BTN_ZOOM_IN,    L"Aumentar calculadoras");
    AppendMenuW(hCalcZoom, MF_STRING, ID_BTN_ZOOM_OUT,   L"Reducir calculadoras");
    AppendMenuW(hCalcZoom, MF_STRING, ID_BTN_ZOOM_RESET, L"Ajuste autom\u00E1tico");
    AppendMenuW(hView, MF_POPUP, (UINT_PTR)hCalcZoom, L"Tama\u00F1o de calculadoras");

    AppendMenuW(hView, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hView, chkFlag(g_historyViewTab == 0), ID_CMD_KEYLOG_VIEW,   L"Pesta\u00F1a Registro de teclas");
    AppendMenuW(hView, chkFlag(g_historyViewTab == 1), ID_CMD_CAPTURES_VIEW, L"Pesta\u00F1a Galer\u00EDa de capturas");
    AppendMenuW(hView, chkFlag(g_historyViewTab == 2), ID_CMD_PLAYER_TAB,    L"Pesta\u00F1a Reproductor paso a paso\tF2");
    AppendMenuW(hRoot, MF_POPUP, (UINT_PTR)hView, L"&Ver");

    // 2. Capturas y Portapapeles
    HMENU hCap = CreatePopupMenu();
    AppendMenuW(hCap, MF_STRING, ID_CMD_SMART_COPY,  L"Copiar resultado\tCtrl+C");
    AppendMenuW(hCap, MF_STRING, ID_CMD_CAPTURE_LCD, L"Capturar LCD actual (PNG)\tCtrl+L");
    AppendMenuW(hCap, MF_STRING, ID_CMD_EXPORTER,    L"Exportar gu\u00EDa de pasos...\tCtrl+E");
    AppendMenuW(hCap, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hCap, chkFlag(g_captureZoomScale == 1), ID_CMD_ZOOM_X1,    L"Resoluci\u00F3n de captura LCD x1");
    AppendMenuW(hCap, chkFlag(g_captureZoomScale == 2), ID_CMD_ZOOM_X2,    L"Resoluci\u00F3n de captura LCD x2");
    AppendMenuW(hCap, chkFlag(g_captureZoomScale == 3), ID_CMD_ZOOM_X3,    L"Resoluci\u00F3n de captura LCD x3");
    AppendMenuW(hCap, chkFlag(g_popupShowGrid),         ID_CMD_POPUP_GRID, L"Marco en pantalla LCD flotante");
    AppendMenuW(hCap, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hCap, MF_STRING, ID_CMD_QR_DISPLAY,    L"Generar c\u00F3digo QR en Blanca (SHIFT + OPTN)");
    AppendMenuW(hCap, MF_STRING, ID_BTN_OPEN_CAPTURES, L"Abrir carpeta de capturas...");
    AppendMenuW(hCap, MF_STRING, ID_BTN_CLEAR_HIST,    L"Limpiar historial de teclas");
    AppendMenuW(hRoot, MF_POPUP, (UINT_PTR)hCap, L"&Capturas y Portapapeles");

    // 3. Estudio y Referencia
    HMENU hStudy = CreatePopupMenu();
    AppendMenuW(hStudy, chkFlag(g_keyExplorerEnabled),  ID_BTN_KEY_EXPLORER,       L"Inspector de tecla al pasar el cursor\tCtrl+Shift+K");
    AppendMenuW(hStudy, chkFlag(g_shiftAlphaLayerMode), ID_BTN_SHIFT_ALPHA_LAYERS, L"Ver 3 capas (SHIFT / ALPHA) en Inspector\tCtrl+Shift+L");
    AppendMenuW(hStudy, MF_STRING,                      ID_BTN_LEARNING_STUDIO,    L"Centro de estudio CASIO...\tCtrl+Shift+E");
    AppendMenuW(hStudy, MF_STRING,                      ID_CMD_OPEN_DOC_STUDIO,    L"CuadernU (Cuaderno y Editor PDF)...");
    AppendMenuW(hStudy, MF_STRING,                      ID_CMD_SEND_TO_DOC_STUDIO, L"Enviar secuencia actual a CuadernU (Tarjeta CASIO)...");
    AppendMenuW(hStudy, chkFlag(g_teacherHighlightMode), ID_CMD_TEACHER_MODE,      L"Resaltar \u00FAltima tecla pulsada");
    AppendMenuW(hStudy, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hStudy, MF_STRING, ID_CMD_COMPARE_MODELS,    L"Comparador de 3 modelos...");
    AppendMenuW(hStudy, MF_STRING, ID_CMD_TROUBLESHOOTER,    L"Diagn\u00F3stico de pantalla...");
    AppendMenuW(hStudy, MF_STRING, ID_CMD_CHECKLIST_PRESETS, L"Perfiles, verificaci\u00F3n y favoritos...");
    AppendMenuW(hRoot, MF_POPUP, (UINT_PTR)hStudy, L"&Estudio y Referencia");

    // 3b. Motor fx-CG50 (Propio v47 / Oficial DLLs Casio / Comparador en vivo)
    HMENU hCgEngine = CreatePopupMenu();
    const auto cgMode = g_nativeCalcs[2].cg50
        ? g_nativeCalcs[2].cg50->GetEngineMode()
        : Cg50Backend::EngineMode::NativeIndependent;
    AppendMenuW(hCgEngine, chkFlag(cgMode == Cg50Backend::EngineMode::NativeIndependent),
        ID_CMD_CG50_MODE_NATIVE, L"N\u00FAcleo propio independiente (SH7305 v47)");
    AppendMenuW(hCgEngine, chkFlag(cgMode == Cg50Backend::EngineMode::OfficialCasioDll),
        ID_CMD_CG50_MODE_OFFICIAL, L"N\u00FAcleo oficial (DLLs Casio)");
    AppendMenuW(hCgEngine, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hCgEngine, chkFlag(cgMode == Cg50Backend::EngineMode::CompareLive),
        ID_CMD_CG50_COMPARE_LIVE, L"Comparador en vivo: Oficial vs. Propio...");
    AppendMenuW(hRoot, MF_POPUP, (UINT_PTR)hCgEngine, L"Motor &fx-CG50");

    // 4. Sesion y Configuracion
    AppendMenuW(hRoot, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hRoot, MF_STRING, ID_CMD_SAVE_RAM_STATE,    L"Guardar estado actual\tCtrl+S");
    AppendMenuW(hRoot, MF_STRING, ID_CMD_RESTORE_RAM_STATE, L"Restaurar estado guardado\tCtrl+Z");
    AppendMenuW(hRoot, MF_STRING, ID_CMD_CW2_RESTART,       L"Reiniciar fx-991CW");
    AppendMenuW(hRoot, MF_STRING, ID_CMD_CW2_RESET_RAM,     L"Reiniciar NVRAM fx-991CW (Ingl\u00E9s inicial)");
    AppendMenuW(hRoot, MF_STRING, ID_CMD_SETTINGS,          L"Configuraci\u00F3n avanzada...\tCtrl+,");

    // 5. Ayuda y Manuales PDF
    AppendMenuW(hRoot, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hRoot, MF_STRING, ID_BTN_SMART_HELP, L"Ayuda y buscador de funciones...\tF1");
    HMENU hPdfs = CreatePopupMenu();
    constexpr int manualCount = static_cast<int>(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
    for (int i = 0; i < manualCount; ++i) {
        if (i == 7 || i == 10) AppendMenuW(hPdfs, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPdfs, MF_STRING, ID_CMD_PDF_BASE + i, g_officialManuals[i].fullTitle);
    }
    AppendMenuW(hPdfs, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hPdfs, MF_STRING, ID_CMD_OPEN_MANUALS_FOLDER, L"Abrir carpeta de manuales PDF...");
    std::wstring manualsMenuTitle = L"&Manuales oficiales PDF (" + std::to_wstring(manualCount) + L")";
    AppendMenuW(hRoot, MF_POPUP, (UINT_PTR)hPdfs, manualsMenuTitle.c_str());

    SetForegroundWindow(hWndOwner);
    TrackPopupMenuEx(hRoot, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON, screenPt.x, screenPt.y, hWndOwner, nullptr);
    DestroyMenu(hRoot);
}

static HMENU CreateEmulatorMenuBar()
{
    HMENU hBar = CreateMenu();

    // 1. Window menu
    HMENU hWindow = CreatePopupMenu();
    AppendMenuW(hWindow, MF_STRING, ID_BTN_COMPACT_MODE, L"&Modo Ventana Compacta Vertical (1 Calculadora)\tCtrl+M");
    AppendMenuW(hWindow, MF_STRING, ID_CMD_STUDIO_MODE,  L"Modo &Estudio Unificado (Paneles Completos)\tCtrl+U");
    AppendMenuW(hWindow, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hWindow, MF_STRING, ID_BTN_MODE_WHITE,   L"Solo &Blanca (fx-570LA X / 991LA X ClassWiz)");
    AppendMenuW(hWindow, MF_STRING, ID_BTN_MODE_GRAY,    L"Solo &Gris (fx-570VN PLUS / ES PLUS)\tF3");
    AppendMenuW(hWindow, MF_STRING, ID_BTN_MODE_GRAPH,   L"Solo Graficadora (&fx-CG50 PRIZM)\tF4");
    AppendMenuW(hWindow, MF_STRING, ID_BTN_MODE_CW2,     L"Solo &fx-991CW (ClassWiz II)\tF7");
    AppendMenuW(hWindow, MF_STRING, ID_BTN_MODE_DUAL,    L"Vista &Dual (Blanca + Gris)\tF5");
    AppendMenuW(hWindow, MF_STRING, ID_BTN_MODE_ALL,     L"Mostrar las &4 Calculadoras\tF6");
    AppendMenuW(hWindow, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hWindow, MF_STRING, ID_BTN_POPUP_DISP,   L"&Pop-up Display (Pantalla LCD Flotante)\tCtrl+P");
    AppendMenuW(hWindow, MF_STRING, ID_CMD_ALWAYS_ON_TOP, L"&Always on Top (Siempre Visible)\tCtrl+T");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hWindow, L"&Window");

    // 2. Tools menu
    HMENU hTools = CreatePopupMenu();
    AppendMenuW(hTools, MF_STRING, ID_CMD_CAPTURE_LCD,   L"&Capture Screen (Copiar LCD y Guardar)\tCtrl+L");
    AppendMenuW(hTools, MF_STRING, ID_CMD_ZOOM_X1,       L"Capture Setting: &x1 Zoom");
    AppendMenuW(hTools, MF_STRING, ID_CMD_ZOOM_X2,       L"Capture Setting: x&2 Zoom");
    AppendMenuW(hTools, MF_STRING, ID_CMD_ZOOM_X3,       L"Capture Setting: x&3 Zoom");
    AppendMenuW(hTools, MF_STRING, ID_CMD_POPUP_GRID,    L"Marco / Cuadrícula en Pop-up Display");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, ID_CMD_KEYLOG_VIEW,   L"&KeyLog (Registro de Teclas)");
    AppendMenuW(hTools, MF_STRING, ID_CMD_CAPTURES_VIEW, L"Galería de Capturas de Operaciones");
    AppendMenuW(hTools, MF_STRING, ID_CMD_QR_DISPLAY,    L"&QR Code Display (ClassWiz SHIFT+OPTN)");
    AppendMenuW(hTools, MF_STRING, ID_BTN_OPEN_CAPTURES, L"Abrir Carpeta de &Capturas...");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, ID_CMD_SMART_COPY,        L"Copiar Resultado (&Portapapeles Inteligente)\tCtrl+C");
    AppendMenuW(hTools, MF_STRING, ID_CMD_EXPORTER,          L"&Exportador de Gu\u00EDa (Secuencia)...\tCtrl+E");
    AppendMenuW(hTools, MF_STRING, ID_CMD_SETTINGS,          L"&Configuraci\u00F3n de Portapapeles y Presentaci\u00F3n...\tCtrl+,");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, ID_CMD_SAVE_RAM_STATE,    L"Guardar Estado &RAM\tCtrl+S");
    AppendMenuW(hTools, MF_STRING, ID_CMD_RESTORE_RAM_STATE, L"Restaurar Estado Anterior (&Deshacer)\tCtrl+Z");
    AppendMenuW(hTools, MF_STRING, ID_CMD_TEACHER_MODE,      L"&Modo Presentaci\u00F3n (Resaltar \u00DAltima Tecla)");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, ID_BTN_KEY_EXPLORER,       L"&Explorador Interactivo de Teclas\tCtrl+Shift+K");
    AppendMenuW(hTools, MF_STRING, ID_BTN_SHIFT_ALPHA_LAYERS, L"Superponer Capas &SHIFT / ALPHA\tCtrl+Shift+L");
    AppendMenuW(hTools, MF_STRING, ID_BTN_LEARNING_STUDIO,    L"&Centro de Estudio CASIO (6 Pesta\u00F1as)...\tCtrl+Shift+E");
    AppendMenuW(hTools, MF_STRING, ID_CMD_OPEN_DOC_STUDIO,    L"Abrir &CuadernU (Cuaderno y Editor PDF)...");
    AppendMenuW(hTools, MF_STRING, ID_CMD_SEND_TO_DOC_STUDIO, L"Enviar secuencia actual a &CuadernU (Tarjeta CASIO)...");
    AppendMenuW(hTools, MF_STRING, ID_CMD_COMPARE_MODELS,     L"Comparador ClassWiz / ES PLUS / fx-CG50...");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, ID_CMD_CG50_MODE_NATIVE,   L"fx-CG50: Usar N\u00FAcleo propio independiente (SH7305 v47)");
    AppendMenuW(hTools, MF_STRING, ID_CMD_CG50_MODE_OFFICIAL, L"fx-CG50: Usar N\u00FAcleo oficial (DLLs Casio)");
    AppendMenuW(hTools, MF_STRING, ID_CMD_CG50_COMPARE_LIVE,  L"fx-CG50: Comparador en vivo (Oficial vs. Propio)...");
    AppendMenuW(hTools, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTools, MF_STRING, ID_CMD_TROUBLESHOOTER,     L"Diagnosticar Pantalla (\u00BFPor qu\u00E9 muestra esto?)...");
    AppendMenuW(hTools, MF_STRING, ID_CMD_CHECKLIST_PRESETS,  L"Checklist Previo, Perfiles y Favoritos...");
    AppendMenuW(hTools, MF_STRING, ID_CMD_PLAYER_TAB,         L"Reproductor Visual Paso a Paso\tF2");
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hTools, L"&Tools");

    // 3. Help menu
    HMENU hHelp = CreatePopupMenu();
    AppendMenuW(hHelp, MF_STRING, ID_BTN_SMART_HELP, L"&Ayuda Inteligente con Búsqueda y Manuales PDF...\tF1");
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    constexpr int helpManualCount = static_cast<int>(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
    for (int i = 0; i < helpManualCount; ++i) {
        if (i == 7 || i == 10) AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hHelp, MF_STRING, ID_CMD_PDF_BASE + i, g_officialManuals[i].fullTitle);
    }
    AppendMenuW(hHelp, MF_SEPARATOR, 0, nullptr);
    std::wstring helpOpenFolderLbl = L"Abrir &Carpeta con los " + std::to_wstring(helpManualCount) + L" Manuales Oficiales PDF...";
    AppendMenuW(hHelp, MF_STRING, ID_CMD_OPEN_MANUALS_FOLDER, helpOpenFolderLbl.c_str());
    AppendMenuW(hBar, MF_POPUP, (UINT_PTR)hHelp, L"&Help");

    return hBar;
}

// ── Helper to Load Embedded PE Resource ─────────────────────────────────────
static std::vector<unsigned char> LoadResourceBytes(int resId)
{
    HRSRC hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!hRes) return {};
    HGLOBAL hData = LoadResource(g_hInst, hRes);
    if (!hData) return {};
    DWORD size = SizeofResource(g_hInst, hRes);
    void* pMem = LockResource(hData);
    if (!pMem || size == 0) return {};
    std::vector<unsigned char> buffer(size);
    memcpy(buffer.data(), pMem, size);
    return buffer;
}

// ── Main Entry ──────────────────────────────────────────────────────────────

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR lpCmdLine, int nCmdShow)
{
    const std::wstring startupArgs = lpCmdLine ? lpCmdLine : L"";
    const auto first = startupArgs.find_first_not_of(L" \t");
    if (first != std::wstring::npos) {
        const auto end = startupArgs.find_first_of(L" \t", first);
        if (startupArgs.substr(first, end == std::wstring::npos ? end : end - first) == L"--cuadernu") {
            const auto forwardedStart = end == std::wstring::npos
                ? std::wstring::npos : startupArgs.find_first_not_of(L" \t", end);
            const auto forwarded = forwardedStart == std::wstring::npos
                ? std::wstring{} : startupArgs.substr(forwardedStart);
            return LaunchDocumentStudio(nullptr, forwarded) ? 0 : 1;
        }
    }

    (void)nCmdShow;
    g_hInst = hInst;
    // A GUI launch must not depend on a live parent console. A failed CONOUT$
    // reopen closes the CRT stream and a subsequent printf can fast-fail.
    wchar_t consoleOption[2]{};
    const bool consoleRequested = GetEnvironmentVariableW(L"CALCULADORA_CONSOLA", consoleOption, 2) == 1
        && consoleOption[0] == L'1';
    const bool consoleAttached = consoleRequested && AttachConsole(ATTACH_PARENT_PROCESS);
    const char* consoleTarget = consoleAttached ? "CONOUT$" : "NUL";
    FILE* fp = nullptr;
    if (freopen_s(&fp, consoleTarget, "w", stdout) != 0)
        freopen_s(&fp, "NUL", "w", stdout);
    if (freopen_s(&fp, consoleTarget, "w", stderr) != 0)
        freopen_s(&fp, "NUL", "w", stderr);
    std::printf("[MAIN] wWinMain starting (hInst=0x%p)...\n", (void*)hInst);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    g_fontHeader      = CreateFontW(-24, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontNormal      = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontBold        = CreateFontW(-16, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontSmall       = CreateFontW(-14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontLcdTop      = CreateFontW(-20, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Lucida Console");
    g_fontLcdBottom   = CreateFontW(-28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Lucida Console");
    g_fontLcdBadge    = CreateFontW(-11, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontCg50Status  = CreateFontW(-11, 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    g_fontCg50Softkey = CreateFontW(-11, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");

    g_brWindowBg = CreateSolidBrush(CasioTheme::StudioCanvasBg());
    g_brSidebar  = CreateSolidBrush(CasioTheme::StudioSurface());
    g_brPanelBg  = CreateSolidBrush(CasioTheme::StudioCanvasBg());
    g_brCardBg   = CreateSolidBrush(CasioTheme::StudioSurfaceAlt());
    g_brBorder   = CreateSolidBrush(CasioTheme::StudioBorder());



    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance      = hInst;
    wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground  = g_brWindowBg;
    wc.lpszClassName  = L"CasioNativeMain";
    RegisterClassExW(&wc);

    WNDCLASSEXW wcc{};
    wcc.cbSize        = sizeof(wcc);
    wcc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcc.lpfnWndProc   = CanvasWndProc;
    wcc.hInstance      = hInst;
    wcc.hCursor        = LoadCursor(nullptr, IDC_HAND);
    wcc.hbrBackground  = g_brPanelBg;
    wcc.lpszClassName  = L"CasioCanvasArea";
    RegisterClassExW(&wcc);

    WNDCLASSEXW wch{};
    wch.cbSize        = sizeof(wch);
    wch.style         = CS_HREDRAW | CS_VREDRAW;
    wch.lpfnWndProc   = HistoryWndProc;
    wch.hInstance      = hInst;
    wch.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wch.hbrBackground  = g_brSidebar;
    wch.lpszClassName  = L"CasioHistoryArea";
    RegisterClassExW(&wch);

    int scrW = GetSystemMetrics(SM_CXSCREEN);
    int scrH = GetSystemMetrics(SM_CYSCREEN);
    if (scrW == 0 || scrH == 0) { scrW = 1920; scrH = 1080; }

    int wndW = (int)(scrW * 0.95);
    int wndH = (int)(scrH * 0.95);
    if (wndW < MIN_WND_W) wndW = MIN_WND_W;
    if (wndH < MIN_WND_H) wndH = MIN_WND_H;


    g_hMainWnd = CreateWindowExW(
        0, L"CasioNativeMain", L"CASIO \u2014 Calculadoras",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        (scrW - wndW) / 2, (scrH - wndH) / 2, wndW, wndH,
        nullptr, nullptr, hInst, nullptr);

    if (!g_hMainWnd) return 0;

    CasioTheme::ApplyDwmDarkTitleBar(g_hMainWnd, CasioTheme::g_studioDarkTheme);
    ShowWindow(g_hMainWnd, SW_SHOW);
    UpdateWindow(g_hMainWnd);

    std::wstring cmdArgs = lpCmdLine ? lpCmdLine : L"";
    auto extractCliVal = [&](const std::wstring& prefix) -> std::wstring {
        const size_t p = cmdArgs.find(prefix);
        if (p == std::wstring::npos) return L"";
        size_t s = p + prefix.size();
        if (s >= cmdArgs.size()) return L"";
        if (cmdArgs[s] == L'"') {
            const size_t e = cmdArgs.find(L'"', s + 1);
            return (e == std::wstring::npos) ? cmdArgs.substr(s + 1) : cmdArgs.substr(s + 1, e - s - 1);
        }
        if (p > 0 && cmdArgs[p - 1] == L'"') {
            const size_t e = cmdArgs.find(L'"', s);
            return (e == std::wstring::npos) ? cmdArgs.substr(s) : cmdArgs.substr(s, e - s);
        }
        const size_t e = cmdArgs.find(L' ', s);
        return (e == std::wstring::npos) ? cmdArgs.substr(s) : cmdArgs.substr(s, e - s);
    };

    const std::wstring cliTheme = extractCliVal(L"--theme=");
    if (cliTheme == L"dark") {
        SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_APPEAR_DARK, 0);
    } else if (cliTheme == L"hc" || cliTheme == L"high-contrast") {
        SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_APPEAR_HC, 0);
    } else if (cliTheme == L"light") {
        SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_APPEAR_LIGHT, 0);
    }

    const std::wstring cliView = extractCliVal(L"--view=");
    if (cliView == L"all") SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_ALL, 0);
    else if (cliView == L"dual") SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_DUAL, 0);
    else if (cliView == L"white") SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_WHITE, 0);
    else if (cliView == L"gray") SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_GRAY, 0);
    else if (cliView == L"cg50") SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_GRAPH, 0);
    else if (cliView == L"cw2") SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_CW2, 0);

    if (cmdArgs.find(L"--open-history") != std::wstring::npos) {
        SetLearningDrawerOpen(true);
    }
    if (cmdArgs.find(L"--seed-demo-history") != std::wstring::npos) {
        const int demoCalc = (g_activeCalcIdx >= 0 && g_activeCalcIdx <= 1) ? g_activeCalcIdx : 0;
        const wchar_t* demoKeys[5] = { L"SHIFT", L"x\x00B2", L"4", L"9", L"=" };
        for (int k = 0; k < 5; ++k) {
            KeyLogEntry e{};
            e.calcIdx = demoCalc;
            e.keyId = (k < GetKeyCount(demoCalc)) ? k : 0;
            e.keyName = demoKeys[k];
            g_keyHistory.push_back(std::move(e));
        }
        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
    }
    if (cmdArgs.find(L"--compact-gray") != std::wstring::npos) {
        SetCompactVerticalMode(true, 1);
    } else if (cmdArgs.find(L"--compact-cg50") != std::wstring::npos) {
        SetCompactVerticalMode(true, 2);
    } else if (cmdArgs.find(L"--compact-cw") != std::wstring::npos) {
        SetCompactVerticalMode(true, 3);
    } else if (cmdArgs.find(L"--compact") != std::wstring::npos) {
        SetCompactVerticalMode(true, 0);
    }
    if (cmdArgs.find(L"--open-popup") != std::wstring::npos) {
        TogglePopupDisplayWindow();
        g_cliScreenshotTargetKind = 6;
    } else if (cmdArgs.find(L"--popup") != std::wstring::npos) {
        TogglePopupDisplayWindow();
    }
    size_t hsPos = cmdArgs.find(L"--help-search");
    if (hsPos != std::wstring::npos) {
        size_t eqPos = cmdArgs.find(L'=', hsPos);
        if (eqPos != std::wstring::npos && eqPos + 1 < cmdArgs.size()) {
            size_t endPos = cmdArgs.find(L' ', eqPos + 1);
            std::wstring q = (endPos == std::wstring::npos)
                ? cmdArgs.substr(eqPos + 1)
                : cmdArgs.substr(eqPos + 1, endPos - eqPos - 1);
            if (q.size() >= 2 && q.front() == L'"' && q.back() == L'"') {
                q = q.substr(1, q.size() - 2);
            }
            OpenSmartHelpWindow(q);
        } else {
            OpenSmartHelpWindow(0);
        }
    }
    if (cmdArgs.find(L"--open-help") != std::wstring::npos) {
        OpenSmartHelpWindow(0);
        g_cliScreenshotTargetKind = 1;
    } else if (cmdArgs.find(L"--open-studio") != std::wstring::npos) {
        OpenLearningStudioWindow(0, -1, -1, L"");
        g_cliScreenshotTargetKind = 2;
    } else if (cmdArgs.find(L"--open-settings") != std::wstring::npos) {
        OpenSettingsWindow();
        g_cliScreenshotTargetKind = 3;
    } else if (cmdArgs.find(L"--open-exporter") != std::wstring::npos) {
        OpenSequenceExporterWindow(true);
        g_cliScreenshotTargetKind = 4;
    } else if (cmdArgs.find(L"--open-pdf") != std::wstring::npos) {
        OpenOfficialPdfManual(g_officialManuals[0].fileName, 1);
        g_cliScreenshotTargetKind = 5;
    } else if (cmdArgs.find(L"--open-compare-live") != std::wstring::npos) {
        TryInitCg50Backend();
        if (g_nativeCalcs[2].cg50) {
            OpenOrToggleCg50LiveComparator(g_hMainWnd, g_hInst, g_nativeCalcs[2].cg50.get());
            g_cliScreenshotTargetKind = 7;
        }
    }

    const std::wstring cliShot = extractCliVal(L"--screenshot-out=");
    if (!cliShot.empty()) {
        g_cliScreenshotOutPath = cliShot;
        const std::wstring cliDelay = extractCliVal(L"--screenshot-delay=");
        UINT delayMs = 1400;
        if (!cliDelay.empty()) {
            const int parsed = _wtoi(cliDelay.c_str());
            if (parsed >= 300 && parsed <= 15000) delayMs = static_cast<UINT>(parsed);
        }
        SetTimer(g_hMainWnd, ID_TIMER_AUTO_SHOT, delayMs, nullptr);
    }

    MSG msg = {0};
    while (GetMessage(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

// ── Native Calculators Initialization (100% Embedded from Memory) ───────────

void TryInitCg50Backend();

static HBITMAP LoadCw2SkinBodyBitmap()
{
    const std::wstring exeDir = GetUnifiedExecutableDirectory();
    const std::wstring rel = L"\\runtime\\fx991cw\\models\\fx991cw_emu\\interface.png";
    const std::wstring candidates[] = {
        exeDir + rel,
        exeDir + L"\\.." + rel,
        exeDir + L"\\..\\.." + rel,
        L"runtime\\fx991cw\\models\\fx991cw_emu\\interface.png"
    };
    std::wstring foundPath;
    for (const auto& cand : candidates) {
        const DWORD attr = GetFileAttributesW(cand.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            foundPath = cand;
            break;
        }
    }
    if (foundPath.empty() || !EnsureGdiplusInitialized()) return nullptr;

    Gdiplus::Bitmap srcBmp(foundPath.c_str());
    if (srcBmp.GetLastStatus() != Gdiplus::Ok) return nullptr;

    const int targetW = 992;
    const int targetH = 2082;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = targetW;
    bmi.bmiHeader.biHeight = -targetH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hOut = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hOut) {
        if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
        return nullptr;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HGDIOBJ oldBmp = SelectObject(hdcMem, hOut);
    HBRUSH brBg = CreateSolidBrush(CasioTheme::StageBg);
    RECT rcFill{ 0, 0, targetW, targetH };
    FillRect(hdcMem, &rcFill, brBg);
    DeleteObject(brBg);

    {
        Gdiplus::Graphics g(hdcMem);
        g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        const int srcW = static_cast<int>(srcBmp.GetWidth());
        const int srcH = static_cast<int>(srcBmp.GetHeight());
        const int cropW = (srcW >= targetW) ? targetW : srcW;
        const int cropH = (srcH >= targetH) ? targetH : srcH;
        g.DrawImage(&srcBmp,
            Gdiplus::Rect(0, 0, targetW, targetH),
            0, 0, cropW, cropH, Gdiplus::UnitPixel);
    }

    SelectObject(hdcMem, oldBmp);
    DeleteDC(hdcMem);
    if (hdcScreen) ReleaseDC(nullptr, hdcScreen);
    return hOut;
}

void InitNativeCalcs()
{
    std::printf("[INIT] InitNativeCalcs starting...\n");
    {
        auto& c = g_nativeCalcs[0];
        c.displayName = L"fx-570LA X";
        c.subTitle    = L"ClassWiz";
        c.hSkinBmp    = LoadBitmapW(g_hInst, MAKEINTRESOURCEW(IDB_SKIN_CW));
        c.skinW = 644; c.skinH = 1250;
        c.lcdSkinRect = { 130, 235, 514, 385 };
        c.lcdBgColor  = CLR_LCD_CW_BG;
        c.lcdFgColor  = CLR_LCD_CW_FG;
        c.backendKind = BackendKind::NxU8;
        c.backendName = L"Emulador nativo";
        c.backendDiagnostic = L"No se pudo iniciar CalcEmuCore";

        // Initialize genuine OKI nX-U8 emulator from embedded binary ROM resource
        c.romData = LoadResourceBytes(IDR_ROM_CW);
        if (!c.romData.empty()) {
            CalcEmuModel model{};
            model.hardware_id = 4;
            model.real_hardware = 0;
            model.csr_mask = 0x0003;
            model.pd_value = 0x00;
            c.emu = calcemu_create(&model, c.romData.data(), c.romData.size());
            if (c.emu) {
                calcemu_power_on(c.emu);
                calcemu_advance(c.emu, 2000);
                c.lcdW = calcemu_screen_width(c.emu);
                c.lcdH = calcemu_screen_rows(c.emu);
                c.lcdPixels.resize(static_cast<std::size_t>(c.lcdW) * c.lcdH);
                c.lcdRgb.resize(static_cast<std::size_t>(c.lcdW) * c.lcdH);
                c.backendDiagnostic = L"CPU, teclado matricial y LCD activos";
            }
        }
    }

    // 1: Natural fx-570VN PLUS
    {
        auto& c = g_nativeCalcs[1];
        c.displayName = L"fx-570VN PLUS";
        c.subTitle    = L"Natural V.P.A.M.";
        c.hSkinBmp    = LoadBitmapW(g_hInst, MAKEINTRESOURCEW(IDB_SKIN_ES));
        c.skinW = 405; c.skinH = 816;
        c.lcdSkinRect = { 61, 132, 349, 243 };
        c.lcdBgColor  = CLR_LCD_ES_BG;
        c.lcdFgColor  = CLR_LCD_ES_FG;
        c.backendKind = BackendKind::NxU8;
        c.backendName = L"Emulador nativo";
        c.backendDiagnostic = L"No se pudo iniciar CalcEmuCore";

        // Initialize genuine OKI nX-U8 emulator from embedded binary ROM resource
        c.romData = LoadResourceBytes(IDR_ROM_ES);
        if (!c.romData.empty()) {
            CalcEmuModel model{};
            model.hardware_id = 3;
            model.real_hardware = 0;
            model.csr_mask = 0x0001;
            model.pd_value = 0x08;
            c.emu = calcemu_create(&model, c.romData.data(), c.romData.size());
            if (c.emu) {
                calcemu_power_on(c.emu);
                calcemu_advance(c.emu, 2000);
                c.lcdW = calcemu_screen_width(c.emu);
                c.lcdH = calcemu_screen_rows(c.emu);
                c.lcdPixels.resize(static_cast<std::size_t>(c.lcdW) * c.lcdH);
                c.lcdRgb.resize(static_cast<std::size_t>(c.lcdW) * c.lcdH);
                c.backendDiagnostic = L"CPU, teclado matricial y LCD activos";
            }
        }
    }

    // 2: Color Graph fx-CG50
    {
        auto& c = g_nativeCalcs[2];
        c.displayName = L"fx-CG50";
        c.subTitle    = L"Color Graph";
        c.hSkinBmp    = LoadBitmapW(g_hInst, MAKEINTRESOURCEW(IDB_SKIN_CG));
        c.skinW = 664; c.skinH = 1364;
        c.lcdSkinRect = { 92, 153, 572, 403 };
        c.lcdBgColor  = CLR_LCD_CG_BG;
        c.lcdFgColor  = CLR_LCD_CG_FG;
        c.backendKind = BackendKind::Cg50;
        c.backendName = L"GCal / SH7305 (proceso local)";
        c.cg50 = std::make_unique<Cg50Backend>();
        c.backendDiagnostic = L"CG50: inicializando GCal...";
    }

    // 3: ClassWiz II fx-991CW (CasioEmuX external process + official 992x2082 faceplate bitmap)
    {
        auto& c = g_nativeCalcs[3];
        c.displayName = L"fx-991CW";
        c.subTitle    = L"ClassWiz II";
        c.hSkinBmp    = LoadCw2SkinBodyBitmap();
        c.skinW = 992; c.skinH = 2082;
        c.lcdSkinRect = { 107, 315, 885, 620 };
        c.lcdBgColor  = RGB(207, 230, 219);
        c.lcdFgColor  = RGB(20, 36, 58);
        c.backendKind = BackendKind::Cg50; // Isolated external backend process
        c.backendName = L"CasioEmuX (proceso aislado)";
        c.backendDiagnostic = L"Emulador CasioEmuX ClassWiz II";
    }

    g_nativeCalcs[0].isVisible = false;
    g_nativeCalcs[1].isVisible = false;
    g_nativeCalcs[2].isVisible = false;
    g_nativeCalcs[3].isVisible = true;
    g_activeCalcIdx = 3;

    for (int i = 0; i < 3; ++i) {
        CasioTheme::BuildSkinPargbBuffer(
            g_nativeCalcs[i].hSkinBmp,
            g_nativeCalcs[i].skinW,
            g_nativeCalcs[i].skinH,
            g_nativeCalcs[i].skinPargb);
    }
}

// ── Key Lookup Helpers ──────────────────────────────────────────────────────

struct KeyVisual {
    int cx = 0, cy = 0, w = 0, h = 0;
    std::uint16_t matrixCode = 0;
    const wchar_t* name = L"";
};

bool FillKeyVisual(int calcIdx, int keyId, KeyVisual& out)
{
    if (calcIdx == 0) {
        if (keyId < 0 || keyId >= NUM_CW_KEYS) return false;
        const auto& k = g_classwizExactKeys[keyId];
        out = { k.cx, k.cy, k.w, k.h, k.matrixCode, k.name };
        return true;
    }
    if (calcIdx == 1) {
        if (keyId < 0 || keyId >= NUM_GRAY_KEYS) return false;
        const auto& k = g_grayExactKeys[keyId];
        out = { k.cx, k.cy, k.w, k.h, k.matrixCode, k.name };
        return true;
    }
    if (calcIdx == 2) {
        if (keyId < 0 || keyId >= NUM_CG50_KEYS) return false;
        const auto& k = g_cg50ExactKeys[keyId];
        out = { k.cx, k.cy, k.w, k.h, k.matrixCode, k.name };
        return true;
    }
    if (calcIdx == 3) {
        if (keyId < 0 || keyId >= NUM_CW2_KEYS) return false;
        const auto& k = g_cw2ExactKeys[keyId];
        out = { k.cx, k.cy, k.w, k.h, k.matrixCode, k.name };
        return true;
    }
    return false;
}

const FullKeyDef* GetKeyDef(int calcIdx, int keyId)
{
    if (calcIdx == 0) {
        if (keyId >= 0 && keyId < NUM_CW_KEYS) return &g_classwizExactKeys[keyId];
    } else if (calcIdx == 1) {
        if (keyId >= 0 && keyId < NUM_GRAY_KEYS) return &g_grayExactKeys[keyId];
    } else if (calcIdx == 3) {
        if (keyId >= 0 && keyId < NUM_CW2_KEYS) return &g_cw2ExactKeys[keyId];
    }
    return nullptr;
}

int GetKeyCount(int calcIdx)
{
    if (calcIdx == 0) return NUM_CW_KEYS;
    if (calcIdx == 1) return NUM_GRAY_KEYS;
    if (calcIdx == 2) return NUM_CG50_KEYS;
    if (calcIdx == 3) return NUM_CW2_KEYS;
    return 0;
}

void TryInitCg50Backend()
{
    if (g_cg50InitTried || g_cg50InitRunning) return;
    g_cg50InitRunning = true;
    auto& c = g_nativeCalcs[2];
    if (!c.cg50) c.cg50 = std::make_unique<Cg50Backend>();
    log_msg("CG50: Initialize() begin");
    c.cg50->Initialize(nullptr, 0);
    c.backendDiagnostic = c.cg50->Diagnostic();
    {
        char buf[512]{};
        const std::wstring& d = c.backendDiagnostic;
        int n = WideCharToMultiByte(CP_UTF8, 0, d.c_str(), (int)d.size(), buf, 510, nullptr, nullptr);
        buf[n > 0 ? n : 0] = '\0';
        char line[600]{};
        snprintf(line, 600, "CG50: state=%d diag=%s",
                 (int)c.cg50->GetState(), buf);
        log_msg(line);
    }
    const bool isNative = c.cg50 && c.cg50->GetEngine() == Cg50Backend::Engine::RomNative;
    switch (c.cg50->GetState()) {
    case Cg50Backend::State::Ready:
        c.backendName = isNative ? L"SH7305 / R61524 nativo" : L"GCal listo";
        log_msg("CG50: READY");
        break;
    case Cg50Backend::State::Faulted:
        c.backendName = isNative ? L"SH7305 nativo con error" : L"GCal con error";
        log_msg("CG50: FAULTED");
        break;
    default:
        c.backendName = isNative ? L"SH7305 nativo no disponible" : L"GCal no disponible";
        log_msg("CG50: UNAVAILABLE");
        break;
    }
    g_cg50InitTried = true;
    g_cg50InitRunning = false;
}

// ── Main Window Procedure ───────────────────────────────────────────────────

inline bool IsSidebarItemActive(UINT_PTR cid)
{
    const bool popupOpen = (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd));
    switch (cid) {
    case ID_BTN_MODE_ALL:           return !g_compactSingleMode && g_viewMode == VIEW_ALL_THREE;
    case ID_BTN_MODE_DUAL:          return !g_compactSingleMode && g_viewMode == VIEW_DUAL_SCIENTIFIC;
    case ID_BTN_MODE_WHITE:         return !g_compactSingleMode && g_viewMode == VIEW_ONLY_WHITE;
    case ID_BTN_MODE_GRAY:          return !g_compactSingleMode && g_viewMode == VIEW_ONLY_GRAY;
    case ID_BTN_MODE_GRAPH:         return !g_compactSingleMode && g_viewMode == VIEW_ONLY_GRAPH;
    case ID_BTN_MODE_CW2:           return !g_compactSingleMode && g_viewMode == VIEW_ONLY_CW2;
    case ID_BTN_COMPACT_MODE:       return g_compactSingleMode;
    case ID_BTN_POPUP_DISP:         return popupOpen;
    case ID_BTN_KEY_EXPLORER:       return g_keyExplorerEnabled;
    case ID_BTN_SHIFT_ALPHA_LAYERS: return g_shiftAlphaLayerMode;
    case ID_CMD_TEACHER_MODE:       return g_teacherHighlightMode;
    case ID_CMD_ALWAYS_ON_TOP:      return g_alwaysOnTop;
    case ID_CMD_PLAYER_TAB:         return !g_compactSingleMode && g_learningDrawerOpen && g_historyViewTab == 2;
    case ID_CMD_TOGGLE_APRENDER:    return !g_compactSingleMode && g_learningDrawerOpen;
    default:                        return false;
    }
}

inline bool IsSidebarButtonActive(UINT_PTR cid)
{
    return IsSidebarItemActive(cid);
}

inline void InvalidateSidebarButtons()
{
    if (!g_hMainWnd || g_compactSingleMode) return;
    if (g_hCanvasWnd) {
        RECT rcC{};
        GetClientRect(g_hCanvasWnd, &rcC);
        const float cScale = (rcC.right > 0) ? (std::min)(CasioTheme::g_uiScale, (std::max)(1.0f, static_cast<float>(rcC.right) / 880.0f)) : CasioTheme::g_uiScale;
        RECT rTop = { 0, 0, rcC.right, CasioTheme::UiToDevice(STUDIO_TOOLBAR_H + 44, cScale) };
        InvalidateRect(g_hCanvasWnd, &rTop, FALSE);
    }
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        g_hMainWnd = hWnd;
        InitNativeCalcs();

        g_hCanvasWnd = CreateWindowExW(
            0, L"CasioCanvasArea", nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
            0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);

        g_hHistoryWnd = CreateWindowExW(
            0, L"CasioHistoryArea", nullptr,
            WS_CHILD | WS_CLIPCHILDREN | WS_VSCROLL,
            0, 0, 0, 0, hWnd, nullptr, g_hInst, nullptr);

        LoadUnifiedSettingsFromIni();
        UiGlass::LoadGlassSettings(CasioTheme::g_glassSettings);
        CasioTheme::g_studioDarkTheme = CasioTheme::g_glassSettings.IsDark();
        SyncStudioThemeBrushes();
        CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
        RefreshUiScale(hWnd, true);
        g_lastTickMs = GetTickCount64();
        SetTimer(hWnd, ID_TIMER_TICK, 40, nullptr); // 25 fps UI update
        return 0;
    }
    case WM_SIZE: {
        RefreshUiScale(hWnd, true);
        SyncCw2HoverOverlay(true);
        SyncCompactToastOverlay(true);
        return 0;
    }
    case WM_MOVE: {
        SyncCw2HoverOverlay(true);
        SyncCompactToastOverlay(true);
        return 0;
    }
    case WM_DPICHANGED: {
        const RECT* prcNew = reinterpret_cast<const RECT*>(lParam);
        if (prcNew) {
            SetWindowPos(hWnd, nullptr, prcNew->left, prcNew->top,
                prcNew->right - prcNew->left, prcNew->bottom - prcNew->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        RefreshUiScale(hWnd, true);
        SyncCw2HoverOverlay(true);
        SyncCompactToastOverlay(true);
        return 0;
    }
    case WM_ACTIVATE: {
        InvalidateSidebarButtons();
        SyncCw2HoverOverlay(true);
        SyncCompactToastOverlay(true);
        break;
    }
    case WM_SYSCOMMAND: {
        if (!g_compactSingleMode && (wParam & 0xFFF0) == SC_KEYMENU && lParam == 0) {
            SendMessageW(hWnd, WM_COMMAND, ID_CMD_STUDIO_MENU, 0);
            return 0;
        }
        break;
    }
    case WM_TIMER: {
        if (wParam == ID_TIMER_AUTO_SHOT) {
            KillTimer(hWnd, ID_TIMER_AUTO_SHOT);
            HWND targetHwnd = ResolveSecondaryScreenshotTarget(g_cliScreenshotTargetKind);
            if (!targetHwnd || !IsWindow(targetHwnd)) targetHwnd = hWnd;

            if (!g_cliScreenshotOutPath.empty() && targetHwnd && IsWindow(targetHwnd)) {
                RedrawWindow(targetHwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
                RECT rc{};
                GetClientRect(targetHwnd, &rc);
                const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
                const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
                HDC hdcW = GetDC(targetHwnd);
                HDC hdcM = CreateCompatibleDC(hdcW);
                HBITMAP hBmp = CreateCompatibleBitmap(hdcW, w, h);
                HGDIOBJ oldB = SelectObject(hdcM, hBmp);
                if (!PrintWindow(targetHwnd, hdcM, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) {
                    BitBlt(hdcM, 0, 0, w, h, hdcW, 0, 0, SRCCOPY);
                }
                GdiFlush();
                SaveBitmapToPngFile(g_cliScreenshotOutPath.c_str(), hBmp);
                SelectObject(hdcM, oldB);
                DeleteObject(hBmp);
                DeleteDC(hdcM);
                ReleaseDC(targetHwnd, hdcW);
            }
            PostMessageW(hWnd, WM_CLOSE, 0, 0);
            return 0;
        }
        if (wParam == ID_TIMER_CAPTURE_OP) {
            KillTimer(hWnd, ID_TIMER_CAPTURE_OP);
            if (g_pendingCaptureCalcIdx >= 0 && g_hCanvasWnd) {
                InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
                UpdateWindow(g_hCanvasWnd);
                OperationCaptureItem capItem{};
                if (CaptureWindowSnapshot(g_hCanvasWnd, g_pendingCaptureCalcIdx, g_pendingCaptureCalcName, capItem)) {
                    g_operationCaptures.push_back(capItem);
                    g_historyFollowTail = true;
                    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
                }
                if (g_autoCopyOnEvaluate) {
                    CopyCalculatorResultSmart(g_pendingCaptureCalcIdx, g_clipCopyMode, true);
                }
                g_pendingCaptureCalcIdx = -1;
            }
            return 0;
        }
        if (wParam == ID_TIMER_TICK) {
            const ULONGLONG now = GetTickCount64();
            DWORD elapsedMs = static_cast<DWORD>(now - g_lastTickMs);
            g_lastTickMs = now;
            if (elapsedMs > 100) elapsedMs = 100;

            bool advanced = false;
            if (!g_cg50InitTried && !g_cg50InitRunning) {
                TryInitCg50Backend();
                if (!IsWindow(hWnd)) return 0;
                InvalidateRect(hWnd, nullptr, FALSE);
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            }
            const bool popupOpen = (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd));
            for (int i = 0; i < 2; ++i) {
                if ((g_nativeCalcs[i].isVisible || (popupOpen && g_activeCalcIdx == i)) &&
                    g_nativeCalcs[i].emu && !IsIconic(hWnd)) {
                    calcemu_advance(g_nativeCalcs[i].emu, elapsedMs);
                    advanced = true;
                }
            }
            auto& cg = g_nativeCalcs[2];
            const bool compareOpen = (g_hCg50CompareWnd && IsWindowVisible(g_hCg50CompareWnd));
            if (!g_cg50InitRunning &&
                (cg.isVisible || compareOpen || (popupOpen && g_activeCalcIdx == 2)) && cg.cg50 &&
                cg.cg50->GetState() == Cg50Backend::State::Ready &&
                !IsIconic(hWnd)) {
                cg.cg50->Step(elapsedMs * 1000);
                cg.backendDiagnostic = cg.cg50->Diagnostic();
                if (compareOpen) RefreshCg50LiveComparator();
                advanced = true;
                // Diagnóstico N-8: estado del core nativo cada ~2 s.
                static int cgDiagTick = 0;
                if (++cgDiagTick % 50 == 0) {
                    const auto rs = cg.cg50->GetRuntimeStatus();
                    char line[256]{};
                    snprintf(line, 256,
                             "CG50 native: inst=%llu cyc=%llu state=%d fb=%d pc=%08X exc=%llu",
                             (unsigned long long)rs.instructions,
                             (unsigned long long)rs.cycles,
                             (int)rs.cpuState, rs.framebufferAvailable ? 1 : 0,
                             rs.pc, (unsigned long long)rs.exceptionsTaken);
                    log_msg(line);
                }
            }
            const Fx991CwState cwPrev = Fx991CwHost::GetState();
            Fx991CwHost::PollAttach(g_hCanvasWnd, g_cw2ViewportRect, IsCw2Visible());
            if (Fx991CwHost::GetState() != cwPrev) {
                InvalidateSidebarButtons();
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            }
            if (GetForegroundWindow() == g_hMainWnd && g_activeCalcIdx == 3 && IsCw2Visible() &&
                (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0 &&
                Fx991CwHost::IsChildFocused() && g_hCanvasWnd && IsWindowVisible(g_hCanvasWnd)) {
                SetFocus(g_hCanvasWnd);
            }
            PollCw2HoverKey();
            SyncCompactToastOverlay(false);
            if (popupOpen && g_activeCalcIdx == 3 && Fx991CwHost::GetState() == Fx991CwState::Ready &&
                (now - g_lastCw2PopupRefreshMs) >= 180) {
                g_lastCw2PopupRefreshMs = now;
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
            }
            if (advanced && g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            if (advanced && g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd)) {
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
            }
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        HBRUSH brBg = CreateSolidBrush(CasioTheme::StudioCanvasBg());
        FillRect(hdc, &rc, brBg);
        DeleteObject(brBg);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_SETCURSOR:
        break;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = g_compactSingleMode ? 320 : MIN_WND_W;
        mmi->ptMinTrackSize.y = g_compactSingleMode ? 540 : MIN_WND_H;
        return 0;
    }
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        if (id == ID_CMD_WORKSPACE_CALC) {
            if (g_compactSingleMode) {
                g_savedStudioViewMode = (g_soloModelIdx == 0) ? VIEW_ONLY_WHITE
                                      : (g_soloModelIdx == 1) ? VIEW_ONLY_GRAY
                                      : (g_soloModelIdx == 2) ? VIEW_ONLY_GRAPH : VIEW_ONLY_CW2;
                SetCompactVerticalMode(false);
                InvalidateSidebarButtons();
                return 0;
            }
            const WPARAM modeCmd = (g_soloModelIdx == 0) ? ID_BTN_MODE_WHITE
                                 : (g_soloModelIdx == 1) ? ID_BTN_MODE_GRAY
                                 : (g_soloModelIdx == 2) ? ID_BTN_MODE_GRAPH : ID_BTN_MODE_CW2;
            return SendMessageW(hWnd, WM_COMMAND, modeCmd, 0);
        }
        if (id == ID_CMD_WORKSPACE_COMPARE) {
            if (!IsCompareViewMode(g_viewMode) && g_activeCalcIdx >= 0 && g_activeCalcIdx <= 3) {
                g_soloModelIdx = g_activeCalcIdx;
            }
            const WPARAM cmpCmd = (g_compareViewMode == VIEW_DUAL_SCIENTIFIC) ? ID_BTN_MODE_DUAL : ID_BTN_MODE_ALL;
            return SendMessageW(hWnd, WM_COMMAND, cmpCmd, 0);
        }
        if (id == ID_CMD_TOGGLE_APRENDER) {
            if (g_compactSingleMode) {
                SetCompactVerticalMode(false);
                SetLearningDrawerOpen(true);
            } else {
                SetLearningDrawerOpen(!g_learningDrawerOpen);
            }
            return 0;
        }
        if (id == ID_CMD_CLOSE_APRENDER) {
            SetLearningDrawerOpen(false);
            return 0;
        }
        if (id == ID_CMD_TOGGLE_THEME) {
            CasioTheme::SetStudioAppearanceMode(
                CasioTheme::g_studioDarkTheme ? UiGlass::AppearanceMode::Light : UiGlass::AppearanceMode::Dark);
            SyncStudioThemeBrushes();
            SaveUnifiedSettingsToIni();
            if (!g_compactSingleMode) {
                CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
            SyncSecondaryWindowsTheme();
            return 0;
        }
        if (id >= ID_CMD_APPEAR_LIGHT && id <= ID_CMD_GLASS_REDUCE_TRANS) {
            if (id == ID_CMD_APPEAR_LIGHT) CasioTheme::SetStudioAppearanceMode(UiGlass::AppearanceMode::Light);
            else if (id == ID_CMD_APPEAR_DARK) CasioTheme::SetStudioAppearanceMode(UiGlass::AppearanceMode::Dark);
            else if (id == ID_CMD_APPEAR_HC) CasioTheme::SetStudioAppearanceMode(UiGlass::AppearanceMode::HighContrast);
            else if (id == ID_CMD_GLASS_DESIGN) CasioTheme::SetStudioGlassOpacity(0.10f, false);
            else if (id == ID_CMD_GLASS_BALANCED) CasioTheme::SetStudioGlassOpacity(0.36f, false);
            else if (id == ID_CMD_GLASS_FROSTED) CasioTheme::SetStudioGlassOpacity(0.85f, false);
            else if (id == ID_CMD_GLASS_REDUCE_TRANS) {
                CasioTheme::SetStudioGlassOpacity(CasioTheme::g_glassSettings.userOpacitySlider,
                    !CasioTheme::g_glassSettings.reduceTransparency);
            }
            SyncStudioThemeBrushes();
            SaveUnifiedSettingsToIni();
            if (!g_compactSingleMode) {
                CasioTheme::ApplyDwmDarkTitleBar(hWnd, CasioTheme::g_studioDarkTheme);
            }
            InvalidateRect(hWnd, nullptr, TRUE);
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
            SyncSecondaryWindowsTheme();
            return 0;
        }
        if (id == ID_CMD_STUDIO_MENU) {
            POINT ptScreen{};
            if (g_hCanvasWnd && g_studioMenuBtnLogicalRc.right > g_studioMenuBtnLogicalRc.left) {
                RECT rcC{}; GetClientRect(g_hCanvasWnd, &rcC);
                const float cScale = GetCanvasUiScale(rcC.right);
                ptScreen.x = CasioTheme::UiToDevice(g_studioMenuBtnLogicalRc.left, cScale);
                ptScreen.y = CasioTheme::UiToDevice(g_studioMenuBtnLogicalRc.bottom, cScale);
                ClientToScreen(g_hCanvasWnd, &ptScreen);
            } else {
                ptScreen.x = 8;
                ptScreen.y = GetCanvasToolbarH();
                ClientToScreen(hWnd, &ptScreen);
            }
            ShowSpanishStudioMenuPopup(hWnd, ptScreen);
            return 0;
        }
        if (id >= ID_CMD_UISCALE_AUTO && id <= ID_CMD_UISCALE_200) {
            static const int kScaleModes[6] = { 0, 100, 125, 150, 175, 200 };
            const int newMode = kScaleModes[id - ID_CMD_UISCALE_AUTO];
            CasioTheme::g_uiScaleMode = newMode;
            SaveUnifiedSettingsToIni();
            RefreshUiScale(hWnd, true);
            const int effPct = static_cast<int>(std::lround(CasioTheme::g_uiScale * 100.0f));
            const std::wstring scaleToast = (newMode == 0)
                ? (L"Tama\u00F1o de interfaz: Autom\u00E1tico (" + std::to_wstring(effPct) + L"%)")
                : (L"Tama\u00F1o de interfaz fijado en " + std::to_wstring(newMode) + L"%");
            ShowToolbarToast(scaleToast);
            return 0;
        }
        constexpr int manualCount = static_cast<int>(sizeof(g_officialManuals) / sizeof(g_officialManuals[0]));
        if (id >= ID_CMD_PDF_BASE && id < ID_CMD_PDF_BASE + manualCount) {
            OpenOfficialPdfManual(g_officialManuals[id - ID_CMD_PDF_BASE].fileName, 1);
            return 0;
        }
        if (id == ID_CMD_OPEN_MANUALS_FOLDER) {
            OpenOfficialManualsFolder();
            return 0;
        }
        if (id == ID_CMD_SETTINGS) {
            OpenSettingsWindow();
            return 0;
        }
        if (id == ID_CMD_EXPORTER) {
            OpenSequenceExporterWindow(true);
            return 0;
        }
        if (id == ID_CMD_SAVE_RAM_STATE) {
            SaveCalculatorSnapshotState(g_activeCalcIdx);
            return 0;
        }
        if (id == ID_CMD_RESTORE_RAM_STATE) {
            RestoreCalculatorSnapshotState(g_activeCalcIdx);
            return 0;
        }
        if (id == ID_CMD_TEACHER_MODE) {
            g_teacherHighlightMode = !g_teacherHighlightMode;
            SaveUnifiedSettingsToIni();
            ShowToolbarToast(g_teacherHighlightMode
                ? L"Resaltado de \u00FAltima tecla: ACTIVADO"
                : L"Resaltado de \u00FAltima tecla: DESACTIVADO");
            InvalidateSidebarButtons();
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            return 0;
        }
        if (id == ID_CMD_SMART_COPY) {
            CopyCalculatorResultSmart(g_activeCalcIdx, g_clipCopyMode, true);
            return 0;
        }
        if (id == ID_BTN_KEY_EXPLORER) {
            g_keyExplorerEnabled = !g_keyExplorerEnabled;
            SaveUnifiedSettingsToIni();
            if (g_keyExplorerEnabled && !g_compactSingleMode && !g_learningDrawerOpen) {
                SetLearningDrawerOpen(true);
            }
            ShowToolbarToast(g_keyExplorerEnabled
                ? L"Inspector activado en Historial"
                : L"Inspector al pasar desactivado");
            InvalidateSidebarButtons();
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            return 0;
        }
        if (id == ID_BTN_SHIFT_ALPHA_LAYERS) {
            g_shiftAlphaLayerMode = !g_shiftAlphaLayerMode;
            SaveUnifiedSettingsToIni();
            ShowToolbarToast(g_shiftAlphaLayerMode
                ? L"Capas SHIFT / ALPHA expandidas en el Inspector"
                : L"Capas SHIFT / ALPHA en vista normal");
            InvalidateSidebarButtons();
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            return 0;
        }
        if (id == ID_BTN_LEARNING_STUDIO) {
            OpenLearningStudioWindow(0, g_activeCalcIdx, -1, L"");
            return 0;
        }
        if (id == ID_CMD_OPEN_DOC_STUDIO) {
            LaunchDocumentStudio(g_hMainWnd);
            return 0;
        }
        if (id == ID_CMD_SEND_TO_DOC_STUDIO) {
            const wchar_t* modelW = (g_activeCalcIdx == 0) ? L"fx-570LA X"
                : (g_activeCalcIdx == 1) ? L"fx-570VN PLUS"
                : (g_activeCalcIdx == 2) ? L"fx-CG50"
                : L"fx-991CW";
            std::wstring livePreview = BuildLiveHistoryLatexPreview(g_activeCalcIdx);
            for (wchar_t& ch : livePreview) {
                if (ch == L'"' || ch == L'|' || ch == L'\r' || ch == L'\n') ch = L' ';
            }
            if (livePreview.empty()) livePreview = L"2 + 3";

            // Live zero-network IPC push to an already-open CuadernU window (F-246)
            auto wideToUtf8Local = [](const std::wstring& w) -> std::string {
                if (w.empty()) return {};
                int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
                if (n <= 0) return {};
                std::string s(static_cast<std::size_t>(n), '\0');
                WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
                return s;
            };
            auto jsonEscLocal = [](const std::string& in) -> std::string {
                std::string o;
                for (char c : in) {
                    if (c == '\\' || c == '"') o.push_back('\\');
                    o.push_back(c);
                }
                return o;
            };
            HWND hLiveCuadernu = FindWindowW(L"CalculadoraUnificadaDocStudioWnd", nullptr);
            if (hLiveCuadernu && IsWindow(hLiveCuadernu)) {
                wchar_t localApp[MAX_PATH]{};
                if (GetEnvironmentVariableW(L"LOCALAPPDATA", localApp, MAX_PATH) > 0) {
                    std::wstring ipcDir = std::wstring(localApp) + L"\\CuadernU\\ipc_inbox";
                    CreateDirectoryW((std::wstring(localApp) + L"\\CuadernU").c_str(), nullptr);
                    CreateDirectoryW(ipcDir.c_str(), nullptr);
                    const ULONGLONG ts = GetTickCount64();
                    std::wstring msgFile = ipcDir + L"\\calc_" + std::to_wstring(ts) + L".json";
                    const std::string modelU8 = wideToUtf8Local(modelW);
                    const std::string exprU8 = wideToUtf8Local(livePreview);
                    std::string payload = "{\"model\":\"" + jsonEscLocal(modelU8) +
                        "\",\"keySeq\":\"" + jsonEscLocal(exprU8) +
                        "\",\"expression\":\"" + jsonEscLocal(exprU8) +
                        "\",\"ts\":" + std::to_string(ts) + "}";
                    HANDLE hf = CreateFileW((msgFile + L".tmp").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    if (hf != INVALID_HANDLE_VALUE) {
                        DWORD wr = 0;
                        WriteFile(hf, payload.data(), static_cast<DWORD>(payload.size()), &wr, nullptr);
                        CloseHandle(hf);
                        MoveFileExW((msgFile + L".tmp").c_str(), msgFile.c_str(), MOVEFILE_REPLACE_EXISTING);
                        ShowToolbarToast(L"Secuencia enviada en vivo a CuadernU");
                        SetForegroundWindow(hLiveCuadernu);
                        return 0;
                    }
                }
            }

            const std::wstring args = L"--mode canvas --calc-card \"" + std::wstring(modelW) + L"|" + livePreview + L"|" + livePreview + L"\"";
            LaunchDocumentStudio(g_hMainWnd, args);
            return 0;
        }
        if (id == ID_CMD_COMPARE_MODELS) {
            OpenLearningStudioWindow(2, -1, -1, L"");
            return 0;
        }
        if (id == ID_CMD_CG50_MODE_NATIVE || id == ID_CMD_CG50_MODE_OFFICIAL) {
            auto& cg = g_nativeCalcs[2];
            if (!cg.cg50) TryInitCg50Backend();
            if (cg.cg50) {
                if (g_hCg50CompareWnd && IsWindow(g_hCg50CompareWnd)) {
                    DestroyWindow(g_hCg50CompareWnd);
                }
                const auto targetMode = (id == ID_CMD_CG50_MODE_OFFICIAL)
                    ? Cg50Backend::EngineMode::OfficialCasioDll
                    : Cg50Backend::EngineMode::NativeIndependent;
                const bool ok = cg.cg50->SetEngineMode(targetMode);
                cg.backendDiagnostic = cg.cg50->Diagnostic();
                cg.backendName = cg.cg50->StatusLabel();
                ShowToolbarToast(ok
                    ? (targetMode == Cg50Backend::EngineMode::OfficialCasioDll
                        ? L"fx-CG50: N\u00FAcleo oficial Casio (DLLs) activo"
                        : L"fx-CG50: N\u00FAcleo propio independiente (SH7305 v47) activo")
                    : (L"fx-CG50: " + cg.backendDiagnostic));
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (id == ID_CMD_CG50_COMPARE_LIVE) {
            auto& cg = g_nativeCalcs[2];
            if (!cg.cg50) TryInitCg50Backend();
            if (cg.cg50) {
                OpenOrToggleCg50LiveComparator(g_hMainWnd, g_hInst, cg.cg50.get());
                cg.backendDiagnostic = cg.cg50->Diagnostic();
                cg.backendName = cg.cg50->StatusLabel();
                if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            }
            return 0;
        }
        if (id == ID_CMD_TROUBLESHOOTER) {
            OpenLearningStudioWindow(3, g_activeCalcIdx, -1, L"");
            return 0;
        }
        if (id == ID_CMD_CHECKLIST_PRESETS) {
            OpenLearningStudioWindow(4, g_activeCalcIdx, -1, L"");
            return 0;
        }
        if (id == ID_CMD_PLAYER_TAB) {
            g_historyViewTab = 2;
            if (g_compactSingleMode) SetCompactVerticalMode(false);
            SetLearningDrawerOpen(true);
            InvalidateSidebarButtons();
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            return 0;
        }
        if (id == ID_BTN_COMPACT_MODE) {
            SetCompactVerticalMode(!g_compactSingleMode, g_activeCalcIdx);
            return 0;
        }
        if (id == ID_CMD_STUDIO_MODE) {
            SetCompactVerticalMode(false);
            return 0;
        }
        if (id == ID_BTN_POPUP_DISP) {
            TogglePopupDisplayWindow();
            InvalidateSidebarButtons();
            return 0;
        }
        if (id == ID_BTN_SMART_HELP) {
            OpenSmartHelpWindow();
            return 0;
        }
        if (id == ID_CMD_ALWAYS_ON_TOP) {
            SetAlwaysOnTopMode(!g_alwaysOnTop);
            InvalidateSidebarButtons();
            ShowToolbarToast(g_alwaysOnTop ? L"Siempre visible (Always on Top): ACTIVADO" : L"Siempre visible (Always on Top): DESACTIVADO");
            return 0;
        }
        if (id >= ID_QUICK_COMPACT_BASE && id < ID_QUICK_COMPACT_BASE + 4) {
            const int targetIdx = id - ID_QUICK_COMPACT_BASE;
            SetCompactVerticalMode(!g_compactSingleMode, targetIdx);
            InvalidateSidebarButtons();
            return 0;
        }
        if (id >= ID_QUICK_PIN_BASE && id < ID_QUICK_PIN_BASE + 4) {
            const int targetIdx = id - ID_QUICK_PIN_BASE;
            if (targetIdx >= 0 && targetIdx <= 3) g_activeCalcIdx = targetIdx;
            SetAlwaysOnTopMode(!g_alwaysOnTop);
            InvalidateSidebarButtons();
            ShowToolbarToast(g_alwaysOnTop ? L"Siempre visible (Always on Top): ACTIVADO" : L"Siempre visible (Always on Top): DESACTIVADO");
            return 0;
        }
        if (id == ID_COMPACT_PULL_TAB) {
            ToggleCompactStepsDrawer(!g_compactStepsOpen);
            return 0;
        }
        if (id == ID_COMPACT_SHARE) {
            OpenSequenceExporterWindow(true);
            return 0;
        }
        if (id == ID_COMPACT_CLEAR) {
            SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_CLEAR_HIST, 0);
            return 0;
        }
        if (id == ID_CMD_CAPTURE_LCD) {
            CopyAndSaveActiveLcd(g_activeCalcIdx, g_captureZoomScale);
            return 0;
        }
        if (id == ID_CMD_ZOOM_X1 || id == ID_CMD_ZOOM_X2 || id == ID_CMD_ZOOM_X3) {
            g_captureZoomScale = (id == ID_CMD_ZOOM_X1) ? 1 : (id == ID_CMD_ZOOM_X2 ? 2 : 3);
            ShowToolbarToast(L"Escala de captura LCD configurada en Zoom x" + std::to_wstring(g_captureZoomScale));
            if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
            return 0;
        }
        if (id == ID_CMD_POPUP_GRID) {
            g_popupShowGrid = !g_popupShowGrid;
            if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
            return 0;
        }
        if (id == ID_CMD_KEYLOG_VIEW) {
            g_historyViewTab = 0;
            if (g_compactSingleMode) SetCompactVerticalMode(false);
            SetLearningDrawerOpen(true);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
            return 0;
        }
        if (id == ID_CMD_CAPTURES_VIEW) {
            g_historyViewTab = 1;
            if (g_compactSingleMode) SetCompactVerticalMode(false);
            SetLearningDrawerOpen(true);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
            return 0;
        }
        if (id == ID_CMD_QR_DISPLAY) {
            // Send SHIFT + OPTN to ClassWiz if active, and show toast
            if (g_activeCalcIdx == 0 && g_nativeCalcs[0].emu) {
                HandleKeyPress(0, 0); // SHIFT
                HandleKeyPress(0, 6); // OPTN (QR)
                ShowToolbarToast(L"Comando QR (SHIFT + OPTN) enviado a fx-570LA X / 991LA X");
            } else {
                ShowToolbarToast(L"Seleccione la calculadora Blanca (ClassWiz) para generar código QR en pantalla");
            }
            return 0;
        }
        if (id == ID_BTN_MODE_ALL) {
            g_compareViewMode = VIEW_ALL_THREE;
            if (g_compactSingleMode) {
                g_savedStudioViewMode = VIEW_ALL_THREE;
                SetCompactVerticalMode(false);
                InvalidateSidebarButtons();
                return 0;
            }
            g_viewMode = VIEW_ALL_THREE;
            g_nativeCalcs[0].isVisible = true;
            g_nativeCalcs[1].isVisible = true;
            g_nativeCalcs[2].isVisible = true;
            UpdateWindowTitleForMode();
            LayoutCalculators();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            InvalidateSidebarButtons();
        }
        else if (id == ID_BTN_MODE_DUAL) {
            g_compareViewMode = VIEW_DUAL_SCIENTIFIC;
            if (g_compactSingleMode) {
                g_savedStudioViewMode = VIEW_DUAL_SCIENTIFIC;
                SetCompactVerticalMode(false);
                InvalidateSidebarButtons();
                return 0;
            }
            g_viewMode = VIEW_DUAL_SCIENTIFIC;
            if (g_activeCalcIdx >= 2) g_activeCalcIdx = 0;
            g_nativeCalcs[0].isVisible = true;
            g_nativeCalcs[1].isVisible = true;
            g_nativeCalcs[2].isVisible = false;
            UpdateWindowTitleForMode();
            LayoutCalculators();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            InvalidateSidebarButtons();
        }
        else if (id == ID_BTN_MODE_WHITE) {
            g_soloModelIdx = 0;
            g_viewMode = VIEW_ONLY_WHITE;
            g_activeCalcIdx = 0;
            g_nativeCalcs[0].isVisible = true;
            g_nativeCalcs[1].isVisible = false;
            g_nativeCalcs[2].isVisible = false;
            UpdateWindowTitleForMode();
            LayoutCalculators();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            InvalidateSidebarButtons();
            if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
        }
        else if (id == ID_BTN_MODE_GRAY) {
            g_soloModelIdx = 1;
            g_viewMode = VIEW_ONLY_GRAY;
            g_activeCalcIdx = 1;
            g_nativeCalcs[0].isVisible = false;
            g_nativeCalcs[1].isVisible = true;
            g_nativeCalcs[2].isVisible = false;
            UpdateWindowTitleForMode();
            LayoutCalculators();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            InvalidateSidebarButtons();
            if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
        }
        else if (id == ID_BTN_MODE_GRAPH) {
            g_soloModelIdx = 2;
            g_viewMode = VIEW_ONLY_GRAPH;
            g_activeCalcIdx = 2;
            g_nativeCalcs[0].isVisible = false;
            g_nativeCalcs[1].isVisible = false;
            g_nativeCalcs[2].isVisible = true;
            UpdateWindowTitleForMode();
            LayoutCalculators();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            InvalidateSidebarButtons();
            if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
        }
        else if (id == ID_BTN_MODE_CW2) {
            g_soloModelIdx = 3;
            g_viewMode = VIEW_ONLY_CW2;
            g_activeCalcIdx = 3;
            g_nativeCalcs[0].isVisible = false;
            g_nativeCalcs[1].isVisible = false;
            g_nativeCalcs[2].isVisible = false;
            UpdateWindowTitleForMode();
            LayoutCalculators();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
            InvalidateSidebarButtons();
            if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
        }
        else if (id == ID_CMD_CW2_RESTART || id == ID_CMD_CW2_RESET_RAM) {
            const bool resetRam = (id == ID_CMD_CW2_RESET_RAM);
            Fx991CwHost::Restart(g_hCanvasWnd, g_cw2ViewportRect, IsCw2Visible(), resetRam);
            g_inAppMenuState[3] = true;
            g_awaitingSubmenuNav[3] = false;
            ShowToolbarToast(resetRam
                ? L"fx-991CW reiniciada con memoria inicial en ingl\u00E9s."
                : L"Emulador fx-991CW reiniciado.");
            InvalidateSidebarButtons();
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
        }
        else if (id == ID_BTN_ZOOM_IN) {
            if (g_userZoomFactor <= 0.0) g_userZoomFactor = 1.0;
            g_userZoomFactor += 0.15;
            if (g_userZoomFactor > 2.5) g_userZoomFactor = 2.5;
            LayoutCalculators();
            InvalidateSidebarButtons();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
        }
        else if (id == ID_BTN_ZOOM_OUT) {
            if (g_userZoomFactor <= 0.0) g_userZoomFactor = 1.0;
            g_userZoomFactor -= 0.15;
            if (g_userZoomFactor < 0.5) g_userZoomFactor = 0.5;
            LayoutCalculators();
            InvalidateSidebarButtons();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
        }
        else if (id == ID_BTN_ZOOM_RESET) {
            g_userZoomFactor = 0.0; // Auto-Fit
            LayoutCalculators();
            InvalidateSidebarButtons();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
        }
        else if (id == ID_BTN_CLEAR_HIST) {
            g_keyHistory.clear();
            ClearTimelineSnapshots();
            ClearAppMenuThumbCache();
            g_compactDrawerStepHits.clear();
            if (g_historyViewTab == 1) {
                for (auto& cap : g_operationCaptures) {
                    if (cap.hThumb) {
                        DeleteObject(cap.hThumb);
                        cap.hThumb = nullptr;
                    }
                }
                g_operationCaptures.clear();
            }
            g_historyFollowTail = true;
            g_compactStepsScrollY = 0;
            InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
            if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
            SyncCw2HoverOverlay(true);
        }
        else if (id == ID_BTN_OPEN_CAPTURES) {
            OpenCapturesFolder();
        }
        else if (id == ID_BTN_COPY_LAST) {
            CopyCalculatorResultSmart(g_activeCalcIdx, g_clipCopyMode, true);
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
        if (g_compactSingleMode && g_compactStepsOpen && g_hCanvasWnd && g_compactStepsContentH > 0) {
            POINT ptWheel{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(g_hCanvasWnd, &ptWheel);
            if (PtInRect(&g_compactDrawerScrollAreaRc, ptWheel)) {
                const int viewH = (std::max)(1, static_cast<int>(g_compactDrawerScrollAreaRc.bottom - g_compactDrawerScrollAreaRc.top));
                const int maxScroll = (std::max)(0, g_compactStepsContentH - viewH);
                g_compactStepsScrollY = (std::max)(0, (std::min)(maxScroll, g_compactStepsScrollY - (delta / WHEEL_DELTA) * 32));
                InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
                return 0;
            }
        }
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (g_userZoomFactor <= 0.0) g_userZoomFactor = 1.0;
            if (delta > 0) g_userZoomFactor += 0.10;
            else g_userZoomFactor -= 0.10;
            if (g_userZoomFactor < 0.5) g_userZoomFactor = 0.5;
            if (g_userZoomFactor > 2.5) g_userZoomFactor = 2.5;
            LayoutCalculators();
            InvalidateSidebarButtons();
            InvalidateRect(g_hCanvasWnd, nullptr, TRUE);
            return 0;
        }
        break;
    }
    case WM_KEYDOWN: {
        HandleNativeStateHotkeys(wParam);
        HandlePhysicalKeyDown(wParam);
        return 0;
    }
    case WM_KEYUP: {
        HandlePhysicalKeyUp(wParam);
        return 0;
    }
    case WM_CLOSE:
        Fx991CwHost::Shutdown();
        DestroyWindow(hWnd);
        return 0;
    case WM_DESTROY:
        DestroyCompactToastOverlay();
        DestroyCw2HoverOverlay();
        Fx991CwHost::Shutdown();
        KillTimer(hWnd, ID_TIMER_TICK);
        KillTimer(hWnd, ID_TIMER_CAPTURE_OP);
        ReleaseKeyCaptures();
        CasioLcdIndicators::GlobalRenderer().Release();
        cg50_selector::Release();
        for (auto& cap : g_operationCaptures) {
            if (cap.hThumb) {
                DeleteObject(cap.hThumb);
                cap.hThumb = nullptr;
            }
        }
        g_operationCaptures.clear();
        for (int i = 0; i < 4; ++i) {
            if (g_nativeCalcs[i].emu) {
                calcemu_destroy(g_nativeCalcs[i].emu);
                g_nativeCalcs[i].emu = nullptr;
            }
            if (g_nativeCalcs[i].hSkinBmp) {
                DeleteObject(g_nativeCalcs[i].hSkinBmp);
                g_nativeCalcs[i].hSkinBmp = nullptr;
            }
            if (g_nativeCalcs[i].hScaledSkinBmp) {
                DeleteObject(g_nativeCalcs[i].hScaledSkinBmp);
                g_nativeCalcs[i].hScaledSkinBmp = nullptr;
            }
        }
        DeleteObject(g_fontHeader);
        DeleteObject(g_fontNormal);
        DeleteObject(g_fontBold);
        DeleteObject(g_fontSmall);
        DeleteObject(g_fontLcdTop);
        DeleteObject(g_fontLcdBottom);
        DeleteObject(g_fontLcdBadge);
        DeleteObject(g_fontCg50Status);
        DeleteObject(g_fontCg50Softkey);
        DeleteObject(g_brWindowBg);
        DeleteObject(g_brSidebar);
        DeleteObject(g_brPanelBg);
        DeleteObject(g_brCardBg);
        DeleteObject(g_brBorder);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── Layout Calculators Dynamically with Auto-Fit and Zoom ───────────────────

void LayoutCalculators()
{
    if (!g_hCanvasWnd) return;
    RECT rc; GetClientRect(g_hCanvasWnd, &rc);
    const int totalW = rc.right;
    const int barH   = GetCanvasToolbarH();
    const int availH = rc.bottom - barH;
    if (totalW <= 0 || availH <= 0) return;

    const bool cw2Visible = IsCw2Visible();
    if (!cw2Visible) {
        g_cw2StageRect = {};
        g_cw2ViewportRect = {};
    }

    std::vector<int> visible;
    for (int i = 0; i < 3; ++i) {
        if (g_nativeCalcs[i].isVisible) visible.push_back(i);
    }
    if (cw2Visible) {
        visible.push_back(3);
    }
    if (visible.empty()) {
        Fx991CwHost::SyncLayout(g_hCanvasWnd, g_cw2ViewportRect, false);
        return;
    }

    auto getSkinW = [](int idx) -> int {
        return (idx == 3) ? 992 : g_nativeCalcs[idx].skinW;
    };
    auto getSkinH = [](int idx) -> int {
        return (idx == 3) ? 2082 : g_nativeCalcs[idx].skinH;
    };

    int n = (int)visible.size();
    int slotW = totalW / n;

    if (g_compactSingleMode) {
        const int calcAreaW = g_compactStepsOpen
            ? (std::max)(240, totalW - COMPACT_DRAWER_W - COMPACT_TAB_W)
            : (std::max)(240, totalW - COMPACT_TAB_W);
        slotW = calcAreaW / n;
        int fitH = availH - 14;
        for (int idx : visible) {
            int maxWForSlot = slotW - 12;
            int maxHFromW = (maxWForSlot * getSkinH(idx)) / getSkinW(idx);
            if (maxHFromW < fitH) fitH = maxHFromW;
        }
        if (fitH < 260) fitH = 260;

        int targetH = (g_userZoomFactor > 0.05) ? (int)(fitH * g_userZoomFactor) : fitH;
        for (int k = 0; k < n; ++k) {
            int idx = visible[k];
            int targetW = (getSkinW(idx) * targetH) / getSkinH(idx);
            int x = k * slotW + (slotW - targetW) / 2;
            int y = barH + (availH - targetH) / 2;
            if (y < barH + 4) y = barH + 4;
            RECT vp = { x, y, x + targetW, y + targetH };
            RECT st = { 0, barH, calcAreaW, rc.bottom };
            if (idx == 3) {
                g_cw2ViewportRect = vp;
                g_cw2StageRect    = st;
            } else {
                g_nativeCalcs[idx].viewportRect = vp;
                g_nativeCalcs[idx].stageRect    = st;
            }
        }
        Fx991CwHost::SyncLayout(g_hCanvasWnd, g_cw2ViewportRect, cw2Visible);
        SyncCw2HoverOverlay(true);
        return;
    }

    const float cScale = GetCanvasUiScale(totalW);
    CasioTheme::g_canvasUiScale = cScale;
    const bool compare = IsCompareViewMode(g_viewMode);

    const int topPad   = barH + CasioTheme::UiToDevice(compare ? 88 : 16, cScale);
    const int botPad   = CasioTheme::UiToDevice(compare ? 18 : 58, cScale);
    const int availBodyH = (std::max)(CasioTheme::UiToDevice(180, cScale), static_cast<int>(rc.bottom) - topPad - botPad);

    const int outerPadX  = CasioTheme::UiToDevice((n >= 4) ? 20 : 40, cScale);
    const int minGap     = CasioTheme::UiToDevice((n >= 4) ? 16 : 36, cScale);
    const int maxPairGap = CasioTheme::UiToDevice((n == 2) ? 96 : 44, cScale);
    const int maxAllowedW = (std::max)(CasioTheme::UiToDevice(80, cScale),
        (totalW - outerPadX * 2 - minGap * (std::max)(0, n - 1)) / n);

    int fitH = availBodyH;
    for (int idx : visible) {
        const int maxHFromW = (maxAllowedW * getSkinH(idx)) / getSkinW(idx);
        if (maxHFromW < fitH) fitH = maxHFromW;
    }
    if (fitH < 220) fitH = 220;

    const int targetH = (g_userZoomFactor > 0.05) ? static_cast<int>(fitH * g_userZoomFactor) : fitH;
    std::vector<int> targetW(n, 0);
    int sumW = 0;
    for (int k = 0; k < n; ++k) {
        const int idx = visible[k];
        targetW[k] = (getSkinW(idx) * targetH) / getSkinH(idx);
        sumW += targetW[k];
    }

    int gap = minGap;
    if (n > 1 && (totalW - outerPadX * 2) > (sumW + minGap * (n - 1))) {
        const int rawGap = (totalW - outerPadX * 2 - sumW) / (n - 1);
        gap = (std::min)(maxPairGap, rawGap);
    }
    const int groupW = sumW + gap * (std::max)(0, n - 1);
    const bool useTightGroup = (groupW <= totalW - 8);
    int curX = useTightGroup ? ((totalW - groupW) / 2) : 0;
    const int y = (std::max)(topPad, topPad + (availBodyH - targetH) / 2);

    for (int k = 0; k < n; ++k) {
        const int idx = visible[k];
        const int w = targetW[k];
        const int x = useTightGroup ? curX : (k * slotW + (slotW - w) / 2);
        if (useTightGroup) curX += w + gap;

        RECT vp = { x, y, x + w, y + targetH };
        RECT st{};
        if (n == 1) {
            st = { 0, barH, totalW, static_cast<int>(rc.bottom) - botPad };
        } else if (useTightGroup) {
            const int halfGap = gap / 2;
            st = {
                (std::max)(0, x - halfGap),
                topPad - CasioTheme::UiToDevice(28, cScale),
                (std::min)(totalW, x + w + halfGap),
                static_cast<int>(rc.bottom) - botPad
            };
        } else {
            st = {
                k * slotW,
                topPad - CasioTheme::UiToDevice(28, cScale),
                (k + 1) * slotW,
                static_cast<int>(rc.bottom) - botPad
            };
        }
        if (idx == 3) {
            g_cw2StageRect    = st;
            g_cw2ViewportRect = vp;
        } else {
            g_nativeCalcs[idx].stageRect    = st;
            g_nativeCalcs[idx].viewportRect = vp;
        }
    }
    Fx991CwHost::SyncLayout(g_hCanvasWnd, g_cw2ViewportRect, cw2Visible);
    SyncCw2HoverOverlay(true);
}

// ── Physical Keyboard Mapping ───────────────────────────────────────────────

inline WPARAM TranslateCw2ForwardVk(WPARAM vk)
{
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) {
        return static_cast<WPARAM>('0' + (vk - VK_NUMPAD0));
    }
    switch (vk) {
    case VK_DECIMAL:  return VK_OEM_PERIOD;
    case VK_SUBTRACT: return VK_OEM_MINUS;
    case VK_ADD:      return VK_OEM_PLUS;
    case VK_DIVIDE:   return VK_OEM_2;
    case VK_ESCAPE:
    case VK_DELETE:   return VK_SPACE;
    default:          return vk;
    }
}

int MapVkToKeyId(int calcIdx, WPARAM vk)
{
    if (calcIdx == 3) {
        switch (vk) {
        case '0': case VK_NUMPAD0: return 43;
        case '1': case VK_NUMPAD1: return 38;
        case '2': case VK_NUMPAD2: return 39;
        case '3': case VK_NUMPAD3: return 40;
        case '4': case VK_NUMPAD4: return 33;
        case '5': case VK_NUMPAD5: return 34;
        case '6': case VK_NUMPAD6: return 35;
        case '7': case VK_NUMPAD7: return 28;
        case '8': case VK_NUMPAD8: return 29;
        case '9': case VK_NUMPAD9: return 30;
        case VK_ADD: case VK_OEM_PLUS: return 41;
        case VK_SUBTRACT: case VK_OEM_MINUS: return 42;
        case VK_MULTIPLY: return 36;
        case VK_DIVIDE: case VK_OEM_2: return 37;
        case VK_RETURN:   return 47;
        case VK_DECIMAL: case VK_OEM_PERIOD: return 44;
        case VK_BACK:     return 31;
        case VK_DELETE: case VK_ESCAPE: case VK_SPACE: return 32;
        case VK_F3: case VK_HOME: return 1;
        case VK_F4: case VK_UP: return 2;
        case VK_PRIOR:    return 3;
        case VK_F5:       return 11;
        case VK_F6:       return 12;
        case VK_F8:       return 15;
        case VK_LEFT:     return 6;
        case VK_RIGHT:    return 8;
        case VK_NEXT:     return 9;
        case VK_DOWN:     return 10;
        case 'E':         return 45;
        case 'X':         return 16;
        case 'S':         return 23;
        case 'C':         return 24;
        case 'T':         return 25;
        case 'L':         return 21;
        case 'A':         return 22;
        case VK_OEM_4:    return 26;
        case VK_OEM_6:    return 27;
        default:          return -1;
        }
    }
    switch (vk) {
    case '0': case VK_NUMPAD0: return 45;
    case '1': case VK_NUMPAD1: return 40;
    case '2': case VK_NUMPAD2: return 41;
    case '3': case VK_NUMPAD3: return 42;
    case '4': case VK_NUMPAD4: return 35;
    case '5': case VK_NUMPAD5: return 36;
    case '6': case VK_NUMPAD6: return 37;
    case '7': case VK_NUMPAD7: return 30;
    case '8': case VK_NUMPAD8: return 31;
    case '9': case VK_NUMPAD9: return 32;
    case VK_ADD: case VK_OEM_PLUS: return 43;
    case VK_SUBTRACT: case VK_OEM_MINUS: return 44;
    case VK_MULTIPLY: return 38;
    case VK_DIVIDE: case VK_OEM_2: return 39;
    case VK_RETURN:   return 49;
    case VK_DECIMAL: case VK_OEM_PERIOD: return 46;
    case VK_BACK:     return 33;
    case VK_DELETE: case VK_ESCAPE: return 34;
    case VK_F1:    return 0;
    case VK_F2:    return 1;
    case VK_F3:    return 2;
    case VK_F4:    return 3;
    case VK_F5:    return 4;
    case VK_F6:    return 5;
    case VK_UP:    return calcIdx == 2 ? 10 : 2;
    case VK_LEFT:  return calcIdx == 2 ? 11 : (calcIdx == 1 ? 5 : 3);
    case VK_RIGHT: return calcIdx == 2 ? 12 : (calcIdx == 1 ? 6 : 4);
    case VK_DOWN:  return calcIdx == 2 ? 13 : (calcIdx == 1 ? 7 : 5);
    case VK_SHIFT:   return calcIdx == 2 ? 6 : 0;
    case VK_CAPITAL: return calcIdx == 2 ? 14 : (calcIdx == 1 ? 3 : 1);
    default: break;
    }
    // CG50-only secondary keys: modifiers and menu navigation. The nX-U8
    // calculators have no equivalent for these physical keys.
    if (calcIdx == 2) {
        switch (vk) {
        case VK_SHIFT:   return 6;   // SHIFT
        case VK_CAPITAL: return 14;  // ALPHA
        case VK_HOME:    return 9;   // MENU
        case VK_INSERT:  return 7;   // OPTN
        case VK_PRIOR:   return 8;   // VARS (RePag)
        case VK_END:     return 17;  // EXIT
        case VK_NEXT:    return 29;  // FLECHA (AvPag)
        case 'C':        return 22;  // COS
        case 'S':        return 21;  // SIN
        case 'T':        return 23;  // TAN
        case 'L':        return 20;  // LN
        case 'G':        return 19;  // LOG
        case 'X':        return 18;  // X_THETA
        case 0x88: case VK_OEM_4: return 26;  // PAREN_A '('
        case 0x89: case VK_OEM_5: case VK_OEM_6: return 27;  // PAREN_C ')'
        case 0x8A:       return 16;  // POT '^'
        case 0x8B:       return 24;  // FRAC
        case 0x8C:       return 25;  // SD
        case 0x8D: case VK_OEM_COMMA: return 28;  // COMA ','
        case 0x8E:       return 47;  // EXP (x10^x)
        case 0x8F:       return 48;  // ANS
        case 0x92:       return 15;  // X^2
        default: break;
        }
    }
    return -1;
}

// Shared physical-key handlers: both the main window and the canvas
// can hold keyboard focus, so both WM_KEYDOWN entry points converge
// here (cableado unificado GUI->matriz).

static bool IsFocusedControlEdit()
{
    HWND hFocus = GetFocus();
    if (!hFocus) return false;
    wchar_t cls[32]{};
    GetClassNameW(hFocus, cls, 32);
    return (_wcsicmp(cls, L"Edit") == 0);
}

void HandlePhysicalKeyDown(WPARAM vk)
{
    if (IsFocusedControlEdit()) return;
    const bool ctrlDown  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool altDown   = (GetKeyState(VK_MENU) & 0x8000) != 0;
    const bool shiftDown = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (ctrlDown && !altDown && shiftDown) {
        if (vk == 'K') { SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_KEY_EXPLORER, 0); return; }
        if (vk == 'L') { SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_SHIFT_ALPHA_LAYERS, 0); return; }
        if (vk == 'E') { SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_LEARNING_STUDIO, 0); return; }
    }
    if (ctrlDown && !altDown && !shiftDown) {
        if (vk == 'M') { SetCompactVerticalMode(!g_compactSingleMode, g_activeCalcIdx); return; }
        if (vk == 'U') { SetCompactVerticalMode(false); return; }
        if (vk == 'P') { TogglePopupDisplayWindow(); InvalidateSidebarButtons(); return; }
        if (vk == 'T') { SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_ALWAYS_ON_TOP, 0); return; }
        if (vk == 'L') { CopyAndSaveActiveLcd(g_activeCalcIdx, g_captureZoomScale); return; }
        if (vk == 'C') { CopyCalculatorResultSmart(g_activeCalcIdx, g_clipCopyMode, true); return; }
        if (vk == 'E') { OpenSequenceExporterWindow(true); return; }
        if (vk == 'S') { SaveCalculatorSnapshotState(g_activeCalcIdx); return; }
        if (vk == 'Z') { RestoreCalculatorSnapshotState(g_activeCalcIdx); return; }
        if (vk == VK_OEM_COMMA) { OpenSettingsWindow(); return; }
    }
    if (!ctrlDown && !altDown && !shiftDown && vk == VK_F7) {
        SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_MODE_CW2, 0);
        return;
    }
    if (vk == VK_F1 && g_activeCalcIdx != 2) {
        OpenSmartHelpWindow();
        return;
    }
    if (vk == VK_F2) {
        SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_PLAYER_TAB, 0);
        return;
    }

    if (g_activeCalcIdx == 3) {
        const int cw2Key = MapVkToKeyId(3, vk);
        const WPARAM fwdVk = TranslateCw2ForwardVk(vk);
        const bool sdlBound =
            (fwdVk >= '0' && fwdVk <= '9') ||
            fwdVk == VK_RETURN || fwdVk == VK_BACK || fwdVk == VK_SPACE ||
            fwdVk == VK_OEM_MINUS || fwdVk == VK_OEM_PLUS ||
            fwdVk == VK_OEM_2 || fwdVk == VK_OEM_PERIOD ||
            fwdVk == 'E' || fwdVk == VK_F3 || fwdVk == VK_F4 ||
            fwdVk == VK_F5 || fwdVk == VK_F6 || fwdVk == VK_F8;
        if (cw2Key >= 0) {
            MaybeCapturePreEntryAppSnapshot(3, cw2Key);
            g_cw2SuppressParentNotifyForKey = cw2Key;
            g_cw2SuppressParentNotifyUntil = GetTickCount64() + 300;
        }
        if (sdlBound) {
            Fx991CwHost::ForwardKey(fwdVk, true);
        } else if (cw2Key >= 0) {
            KeyVisual kv{};
            if (FillKeyVisual(3, cw2Key, kv)) {
                Fx991CwHost::ClickNormalized(kv.cx / 992.0, kv.cy / 2082.0);
            }
        }
        if (cw2Key >= 0) {
            RecordCw2KeyActivation(cw2Key);
        }
        return;
    }

    const int k = MapVkToKeyId(g_activeCalcIdx, vk);
    if (k >= 0) {
        auto& c = g_nativeCalcs[g_activeCalcIdx];
        c.pressedKey = k;
        HandleKeyPress(g_activeCalcIdx, k);
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
            InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
    }
}

void HandlePhysicalKeyUp(WPARAM vk)
{
    if (g_activeCalcIdx == 3) {
        if (vk != 0) {
            const WPARAM fwdVk = TranslateCw2ForwardVk(vk);
            const bool sdlBound =
                (fwdVk >= '0' && fwdVk <= '9') ||
                fwdVk == VK_RETURN || fwdVk == VK_BACK || fwdVk == VK_SPACE ||
                fwdVk == VK_OEM_MINUS || fwdVk == VK_OEM_PLUS ||
                fwdVk == VK_OEM_2 || fwdVk == VK_OEM_PERIOD ||
                fwdVk == 'E' || fwdVk == VK_F3 || fwdVk == VK_F4 ||
                fwdVk == VK_F5 || fwdVk == VK_F6 || fwdVk == VK_F8;
            if (sdlBound) {
                Fx991CwHost::ForwardKey(fwdVk, false);
            }
        }
        return;
    }
    auto& c = g_nativeCalcs[g_activeCalcIdx];
    if (c.pressedKey >= 0) {
        KeyVisual kv{};
        if (FillKeyVisual(g_activeCalcIdx, c.pressedKey, kv)) {
            if (c.emu) {
                calcemu_key_up(c.emu);
            }
            if (g_activeCalcIdx == 2 && c.cg50 &&
                c.cg50->GetState() == Cg50Backend::State::Ready) {
                c.cg50->KeyUp(kv.matrixCode);
            }
        }
        c.pressedKey = -1;
        if (g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    }
}

// Native save-state / storage hotkeys (solo motor ROM-native):
// Ctrl+Alt+S guarda, Ctrl+Alt+L restaura, Ctrl+Alt+P persiste storage.
void HandleNativeStateHotkeys(WPARAM vk)
{
    auto& cg = g_nativeCalcs[2];
    if (!cg.cg50 || cg.cg50->GetEngine() != Cg50Backend::Engine::RomNative ||
        cg.cg50->GetState() != Cg50Backend::State::Ready) {
        return;
    }
    if (!(GetKeyState(VK_CONTROL) & 0x8000) ||
        !(GetKeyState(VK_MENU) & 0x8000)) {
        return;
    }
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring dir(exePath);
    const std::size_t slash = dir.find_last_of(L'\\');
    if (slash != std::wstring::npos) dir.resize(slash);
    const std::wstring file = dir + L"\\cg50-state.bin";
    if (vk == 'S') {
        cg.cg50->SaveStateToFile(file.c_str());
        cg.backendDiagnostic = L"Save-state guardado: cg50-state.bin";
    } else if (vk == 'L') {
        if (cg.cg50->LoadStateFromFile(file.c_str())) {
            cg.backendDiagnostic = L"Save-state restaurado: cg50-state.bin";
        } else {
            cg.backendDiagnostic = L"Save-state incompatible o ausente";
        }
    } else if (vk == 'P') {
        if (cg.cg50->PersistUserRom()) {
            cg.backendDiagnostic = L"Storage persistido: UserROM_persist.raw";
        } else {
            cg.backendDiagnostic = L"Storage: nada que persistir o sin UserROM";
        }
    }
}

// ── Canvas Toolbar Layout & LCD Hit-Test Helpers ───────────────────────────

struct StudioToolbarLayout {
    bool compact = false;
    bool dense   = false;
    bool ultraNarrow = false;
    RECT rB[5]{};              // Compact only: 0:Compact, 1:OnTop, 2:Popup, 3:Capture, 4:Grid
    RECT rPills[4]{};          // Compact only: Blanca, Gris, CG50, 991CW
    RECT rBrand{};             // Studio left: CASIO wordmark
    RECT rWorkspaceGrp{};      // Studio left: [ Calculadora | Comparar ]
    RECT rWorkspacePills[2]{}; // 0:Calculadora, 1:Comparar
    RECT rAprender{};          // Studio left: Aprender drawer toggle
    RECT rSubCompareGrp{};     // Comparar sub-bar: [ Blanca + Gris (2) | Todas (4) ]
    RECT rSubComparePills[2]{};// 0:Blanca+Gris, 1:Todas(4)
    RECT rSubFocusBadge{};     // Comparar sub-bar: Teclado en <modelo>
    RECT rDockOuter{};         // Calculadora bottom model switcher dock
    RECT rDockPills[4]{};      // 0:fx-991CW, 1:fx-570LA X, 2:fx-570VN+, 3:fx-CG50
    RECT rZoomReset{};         // Studio right (when manual zoom active)
    RECT rGuia{};              // Studio right: Exportar guia
    RECT rHelp{};              // Both: Ayuda / ? PDF
    RECT rMenu{};              // Studio right: Menu completo
};

static constexpr UINT_PTR kDockCmds[4] = {
    ID_BTN_MODE_CW2,
    ID_BTN_MODE_WHITE,
    ID_BTN_MODE_GRAY,
    ID_BTN_MODE_GRAPH
};
static constexpr int kDockCalcIdx[4] = { 3, 0, 1, 2 };

inline StudioToolbarLayout ComputeStudioToolbarLayout(
    int widthCoord, int heightCoord, bool compact, bool compareMode, bool hasCustomZoom)
{
    StudioToolbarLayout tl{};
    tl.compact = compact;
    if (compact) {
        tl.rB[0] = { 6,   5, 30,  31 };
        tl.rB[1] = { 33,  5, 57,  31 };
        tl.rB[2] = { 60,  5, 84,  31 };
        tl.rB[3] = { 87,  5, 111, 31 };
        tl.rB[4] = { 114, 5, 138, 31 };
        tl.rPills[0] = { 143, 6, 203, 30 };
        tl.rPills[1] = { 206, 6, 250, 30 };
        tl.rPills[2] = { 253, 6, 303, 30 };
        tl.rPills[3] = { 306, 6, 366, 30 };
        tl.rHelp     = { 371, 6, 426, 30 };
        return tl;
    }

    const bool wide = (widthCoord >= 860);
    tl.dense = !wide;
    tl.ultraNarrow = (widthCoord < 660);

    int minLeft = 12;
    if (!tl.ultraNarrow) {
        tl.rBrand = { 16, 12, 96, 40 };
        minLeft = tl.rBrand.right + 12;
    }

    const int rgap = wide ? 6 : 4;
    int rx = widthCoord - (wide ? 16 : 10);
    auto placeRight = [&](int w) -> RECT {
        RECT r = { rx - w, 11, rx, 41 };
        rx -= (w + rgap);
        return r;
    };

    tl.rMenu = placeRight(wide ? 38 : 34);
    tl.rHelp = placeRight(wide ? 62 : 52);
    if (!tl.ultraNarrow) {
        tl.rGuia = placeRight(wide ? 78 : 66);
    }
    tl.rAprender = placeRight(wide ? 82 : (tl.ultraNarrow ? 68 : 72));

    const int ws0W = wide ? 106 : (tl.ultraNarrow ? 82 : 92);
    const int ws1W = wide ? 94  : (tl.ultraNarrow ? 74 : 82);
    const int grpW = 3 + ws0W + 2 + ws1W + 3;
    const int maxWsRight = rx - 10;
    int wsX = (widthCoord - grpW) / 2;
    if (wsX + grpW > maxWsRight) wsX = maxWsRight - grpW;
    if (wsX < minLeft) wsX = minLeft;

    tl.rWorkspacePills[0] = { wsX + 3, 12, wsX + 3 + ws0W, 40 };
    tl.rWorkspacePills[1] = { wsX + 3 + ws0W + 2, 12, wsX + 3 + ws0W + 2 + ws1W, 40 };
    tl.rWorkspaceGrp      = { wsX, 9, tl.rWorkspacePills[1].right + 3, 43 };

    if (hasCustomZoom) {
        const int zw = wide ? 76 : 64;
        if (rx - zw >= tl.rWorkspaceGrp.right + 10) {
            tl.rZoomReset = placeRight(zw);
        }
    }

    if (compareMode) {
        const int p0W = wide ? 124 : 106;
        const int p1W = wide ? 78  : 68;
        const int subGrpW = 3 + p0W + 2 + p1W + 3;
        const int focusW = (widthCoord >= 680) ? 148 : 0;
        const int unitW = (focusW > 0) ? (subGrpW + 14 + focusW) : subGrpW;
        const int subX = (std::max)(12, (widthCoord - unitW) / 2);
        const int subTop = STUDIO_TOOLBAR_H + 8;
        const int subBot = STUDIO_TOOLBAR_H + 38;
        tl.rSubCompareGrp = { subX, subTop, subX + subGrpW, subBot };
        tl.rSubComparePills[0] = { subX + 3, subTop + 3, subX + 3 + p0W, subBot - 3 };
        tl.rSubComparePills[1] = { subX + 3 + p0W + 2, subTop + 3, subX + 3 + p0W + 2 + p1W, subBot - 3 };
        if (focusW > 0) {
            tl.rSubFocusBadge = { tl.rSubCompareGrp.right + 14, subTop + 2, tl.rSubCompareGrp.right + 14 + focusW, subBot - 2 };
        }
    } else if (heightCoord > STUDIO_TOOLBAR_H + 90) {
        const int dockBot = heightCoord - 12;
        const int dockTop = dockBot - 34;
        const int cellGap = 3;
        const int innerPad = 3;
        const int cellW = (widthCoord >= 520) ? 94 : (std::max)(64, (widthCoord - 24 - innerPad * 2 - cellGap * 3) / 4);
        const int dockW = innerPad * 2 + cellW * 4 + cellGap * 3;
        const int dockX = (std::max)(8, (widthCoord - dockW) / 2);
        tl.rDockOuter = { dockX, dockTop, dockX + dockW, dockBot };
        int px = dockX + innerPad;
        for (int i = 0; i < 4; ++i) {
            tl.rDockPills[i] = { px, dockTop + 3, px + cellW, dockBot - 3 };
            px += cellW + cellGap;
        }
    }
    return tl;
}

inline int HitTestCanvasStudioChrome(const StudioToolbarLayout& tl, int lx, int ly)
{
    if (tl.compact) return 0;
    const POINT pt{ lx, ly };
    if (ly >= 0 && ly < STUDIO_TOOLBAR_H) {
        if (PtInRect(&tl.rWorkspacePills[0], pt)) return ID_CMD_WORKSPACE_CALC;
        if (PtInRect(&tl.rWorkspacePills[1], pt)) return ID_CMD_WORKSPACE_COMPARE;
        if (PtInRect(&tl.rAprender, pt)) return ID_CMD_TOGGLE_APRENDER;
        if (tl.rZoomReset.right > tl.rZoomReset.left && PtInRect(&tl.rZoomReset, pt)) return ID_BTN_ZOOM_RESET;
        if (tl.rGuia.right > tl.rGuia.left && PtInRect(&tl.rGuia, pt)) return ID_CMD_EXPORTER;
        if (PtInRect(&tl.rHelp, pt)) return ID_BTN_SMART_HELP;
        if (PtInRect(&tl.rMenu, pt)) return ID_CMD_STUDIO_MENU;
        return 0;
    }
    if (tl.rSubComparePills[0].right > tl.rSubComparePills[0].left && PtInRect(&tl.rSubComparePills[0], pt)) return ID_BTN_MODE_DUAL;
    if (tl.rSubComparePills[1].right > tl.rSubComparePills[1].left && PtInRect(&tl.rSubComparePills[1], pt)) return ID_BTN_MODE_ALL;
    for (int i = 0; i < 4; ++i) {
        if (tl.rDockPills[i].right > tl.rDockPills[i].left && PtInRect(&tl.rDockPills[i], pt)) {
            return static_cast<int>(kDockCmds[i]);
        }
    }
    for (int i = 0; i < 4; ++i) {
        if (g_calcQuickPinLogRc[i].right > g_calcQuickPinLogRc[i].left && PtInRect(&g_calcQuickPinLogRc[i], pt)) {
            return ID_QUICK_PIN_BASE + i;
        }
        if (g_calcQuickCompactLogRc[i].right > g_calcQuickCompactLogRc[i].left && PtInRect(&g_calcQuickCompactLogRc[i], pt)) {
            return ID_QUICK_COMPACT_BASE + i;
        }
    }
    return 0;
}

inline void InitCanvasTooltip(HWND hWnd)
{
    if (g_hCanvasTooltip && IsWindow(g_hCanvasTooltip)) return;
    g_hCanvasTooltip = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hWnd, nullptr, g_hInst, nullptr);
    if (!g_hCanvasTooltip) return;
    SendMessageW(g_hCanvasTooltip, TTM_SETMAXTIPWIDTH, 0, 320);
    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = hWnd;
    ti.uId = reinterpret_cast<UINT_PTR>(hWnd);
    ti.lpszText = const_cast<wchar_t*>(L"");
    SendMessageW(g_hCanvasTooltip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
    SendMessageW(g_hCanvasTooltip, TTM_ACTIVATE, FALSE, 0);
}

inline const wchar_t* GetCanvasToolbarTip(int cmdId)
{
    switch (cmdId) {
    case ID_CMD_WORKSPACE_CALC:    return L"Una calculadora a tama\u00F1o completo";
    case ID_CMD_WORKSPACE_COMPARE: return L"Comparar modelos lado a lado (F5 / F6)";
    case ID_CMD_TOGGLE_APRENDER:   return L"Mostrar u ocultar el panel de historial";
    case ID_BTN_MODE_ALL:          return L"Comparar Blanca, Gris, fx-CG50 y fx-991CW (F6)";
    case ID_BTN_MODE_DUAL:         return L"Comparar Blanca ClassWiz y Gris Natural ES (F5)";
    case ID_BTN_MODE_WHITE:        return L"fx-570LA X / 991LA X (Blanca)";
    case ID_BTN_MODE_GRAY:         return L"fx-570VN PLUS (Gris, F3)";
    case ID_BTN_MODE_GRAPH:        return L"fx-CG50 Graficadora (F4)";
    case ID_BTN_MODE_CW2:          return L"fx-991CW ClassWiz II (F7)";
    case ID_BTN_ZOOM_RESET:        return L"Ajuste autom\u00E1tico de tama\u00F1o";
    case ID_CMD_EXPORTER:          return L"Exportar gu\u00EDa (Ctrl+E)";
    case ID_BTN_SMART_HELP:        return L"Ayuda y manuales PDF (F1)";
    case ID_CMD_STUDIO_MENU:       return L"Opciones de ventana, tema y estudio (Alt)";
    case ID_COMPACT_PULL_TAB:      return g_compactStepsOpen ? L"Ocultar secuencia de teclas" : L"Mostrar secuencia de teclas";
    case ID_COMPACT_SHARE:         return L"Editar, dibujar a mano o anotar secuencia para compartir";
    case ID_COMPACT_CLEAR:         return L"Limpiar secuencia registrada";
    default:
        if (cmdId >= ID_QUICK_COMPACT_BASE && cmdId < ID_QUICK_COMPACT_BASE + 4) {
            return L"Abrir esta calculadora en modo ventana compacta";
        }
        if (cmdId >= ID_QUICK_PIN_BASE && cmdId < ID_QUICK_PIN_BASE + 4) {
            return g_alwaysOnTop
                ? L"Desbloquear siempre en primer plano (candado)"
                : L"Fijar siempre en primer plano (candado)";
        }
        return L"";
    }
}

inline void UpdateCanvasTooltipText(HWND hWnd, int hoverCmdId)
{
    if (!g_hCanvasTooltip) return;
    if (hoverCmdId == g_lastCanvasTipId) return;
    g_lastCanvasTipId = hoverCmdId;

    SendMessageW(g_hCanvasTooltip, TTM_POP, 0, 0);
    const wchar_t* txt = GetCanvasToolbarTip(hoverCmdId);
    TOOLINFOW ti{};
    ti.cbSize = sizeof(ti);
    ti.hwnd = hWnd;
    ti.uId = reinterpret_cast<UINT_PTR>(hWnd);
    ti.lpszText = const_cast<wchar_t*>(txt ? txt : L"");
    SendMessageW(g_hCanvasTooltip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
    SendMessageW(g_hCanvasTooltip, TTM_ACTIVATE, (txt && txt[0] != L'\0') ? TRUE : FALSE, 0);
}

inline bool HitTestCanvasLcd(int calcIdx, int mouseX, int mouseY, int& outIndSprite, bool& outIndLit, bool& outLcdBody)
{
    outIndSprite = -1;
    outIndLit = false;
    outLcdBody = false;
    if (calcIdx < 0 || calcIdx > 2) return false;
    const auto& c = g_nativeCalcs[calcIdx];
    if (!c.isVisible || c.skinW <= 0 || c.skinH <= 0) return false;
    const int dstW = c.viewportRect.right - c.viewportRect.left;
    const int dstH = c.viewportRect.bottom - c.viewportRect.top;
    if (dstW <= 0 || dstH <= 0) return false;

    double scaleX = static_cast<double>(dstW) / c.skinW;
    double scaleY = static_cast<double>(dstH) / c.skinH;
    RECT dstLcd = {
        c.viewportRect.left + static_cast<int>(c.lcdSkinRect.left   * scaleX),
        c.viewportRect.top  + static_cast<int>(c.lcdSkinRect.top    * scaleY),
        c.viewportRect.left + static_cast<int>(c.lcdSkinRect.right  * scaleX),
        c.viewportRect.top  + static_cast<int>(c.lcdSkinRect.bottom * scaleY)
    };
    if (!PtInRect(&dstLcd, POINT{ mouseX, mouseY })) return false;
    outLcdBody = true;

    if (calcIdx < 2 && c.emu) {
        RECT indRect{}, dotRect{};
        ComputeLcdIndAndDotRects(calcIdx, dstLcd, indRect, dotRect);
        const size_t rawSz = calcemu_screen_buffer_size(c.emu);
        std::vector<unsigned char> rawBuf(rawSz);
        bool gotRaw = (rawSz > 0 && calcemu_screen_buffer(c.emu, rawBuf.data(), rawBuf.size()) == rawSz);
        CasioKnowledge::HitTestLcdIndicator(calcIdx, mouseX, mouseY, indRect,
            gotRaw ? rawBuf.data() : nullptr, rawBuf.size(), outIndSprite, outIndLit);
    }
    return true;
}

// ── Compact Step Drawer, Per-Card Studio Icons, cw2 Hover Overlay & App Menu Capture ──

static void DrawPadlockIconGdi(HDC hdc, const RECT& rc, bool locked, COLORREF color)
{
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int s = (std::min)(w, h);
    if (s < 6) return;

    Gdiplus::Graphics g(hdc);
    CasioTheme::SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    const float u = static_cast<float>(s) / 16.0f;
    const float cx = (rc.left + rc.right) * 0.5f;
    const float cy = (rc.top + rc.bottom) * 0.5f;
    const Gdiplus::Color col(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(col, (std::max)(1.25f, 1.45f * u));
    pen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);

    const float bodyW = 9.2f * u;
    const float bodyH = 6.2f * u;
    const float bodyL = cx - bodyW * 0.5f;
    const float bodyT = cy - 0.2f * u;
    const float rad = (std::max)(1.0f, 1.6f * u);
    const float d = rad * 2.0f;

    Gdiplus::GraphicsPath bodyPath;
    bodyPath.AddArc(bodyL, bodyT, d, d, 180.0f, 90.0f);
    bodyPath.AddArc(bodyL + bodyW - d, bodyT, d, d, 270.0f, 90.0f);
    bodyPath.AddArc(bodyL + bodyW - d, bodyT + bodyH - d, d, d, 0.0f, 90.0f);
    bodyPath.AddArc(bodyL, bodyT + bodyH - d, d, d, 90.0f, 90.0f);
    bodyPath.CloseFigure();

    if (locked) {
        Gdiplus::SolidBrush fillBr(Gdiplus::Color(40, GetRValue(color), GetGValue(color), GetBValue(color)));
        g.FillPath(&fillBr, &bodyPath);
    }
    g.DrawPath(&pen, &bodyPath);

    const float shW = 5.4f * u;
    const float shH = 5.2f * u;
    if (locked) {
        Gdiplus::GraphicsPath shPath;
        shPath.AddLine(cx - shW * 0.5f, bodyT + 0.3f * u, cx - shW * 0.5f, bodyT - 2.3f * u);
        shPath.AddArc(cx - shW * 0.5f, bodyT - shH, shW, shH, 180.0f, 180.0f);
        shPath.AddLine(cx + shW * 0.5f, bodyT - 2.3f * u, cx + shW * 0.5f, bodyT + 0.3f * u);
        g.DrawPath(&pen, &shPath);
    } else {
        const float lift = 1.6f * u;
        Gdiplus::GraphicsPath shPath;
        shPath.AddArc(cx - shW * 0.5f, bodyT - shH - lift, shW, shH, 200.0f, 160.0f);
        shPath.AddLine(cx + shW * 0.5f, bodyT - 2.3f * u - lift, cx + shW * 0.5f, bodyT + 0.3f * u);
        g.DrawPath(&pen, &shPath);
    }

    Gdiplus::SolidBrush dotBr(col);
    const float kr = (std::max)(1.0f, 1.15f * u);
    g.FillEllipse(&dotBr, cx - kr, bodyT + bodyH * 0.5f - kr, kr * 2.0f, kr * 2.0f);
}

static void DrawCompactQuickIconGdi(HDC hdc, const RECT& rc, COLORREF color)
{
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    const int s = (std::min)(w, h);
    if (s < 6) return;

    Gdiplus::Graphics g(hdc);
    CasioTheme::SyncGraphicsTransform(hdc, g);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);

    const float u = static_cast<float>(s) / 16.0f;
    const float cx = (rc.left + rc.right) * 0.5f;
    const float cy = (rc.top + rc.bottom) * 0.5f;
    const Gdiplus::Color col(255, GetRValue(color), GetGValue(color), GetBValue(color));
    Gdiplus::Pen pen(col, (std::max)(1.2f, 1.35f * u));
    pen.SetLineJoin(Gdiplus::LineJoinRound);

    const float cardW = 8.6f * u;
    const float cardH = 11.8f * u;
    const float cardL = cx - cardW * 0.5f;
    const float cardT = cy - cardH * 0.5f;
    const float rad = (std::max)(1.2f, 1.8f * u);
    const float d = rad * 2.0f;

    Gdiplus::GraphicsPath cardPath;
    cardPath.AddArc(cardL, cardT, d, d, 180.0f, 90.0f);
    cardPath.AddArc(cardL + cardW - d, cardT, d, d, 270.0f, 90.0f);
    cardPath.AddArc(cardL + cardW - d, cardT + cardH - d, d, d, 0.0f, 90.0f);
    cardPath.AddArc(cardL, cardT + cardH - d, d, d, 90.0f, 90.0f);
    cardPath.CloseFigure();
    g.DrawPath(&pen, &cardPath);

    Gdiplus::SolidBrush softBr(Gdiplus::Color(65, GetRValue(color), GetGValue(color), GetBValue(color)));
    g.FillRectangle(&softBr, cardL + 1.8f * u, cardT + 2.0f * u, cardW - 3.6f * u, 2.7f * u);
    Gdiplus::Pen lcdPen(col, (std::max)(1.0f, 0.9f * u));
    g.DrawRectangle(&lcdPen, cardL + 1.8f * u, cardT + 2.0f * u, cardW - 3.6f * u, 2.7f * u);

    Gdiplus::SolidBrush dotBr(col);
    const float dr = (std::max)(0.85f, 0.85f * u);
    for (int r = 0; r < 2; ++r) {
        const float py = cardT + (6.6f + r * 2.4f) * u;
        for (int c = 0; c < 3; ++c) {
            const float px = cx + (c - 1) * 2.15f * u;
            g.FillEllipse(&dotBr, px - dr, py - dr, dr * 2.0f, dr * 2.0f);
        }
    }
}

static void ToggleCompactStepsDrawer(bool open)
{
    if (g_compactStepsOpen == open) return;
    g_compactStepsOpen = open;
    g_compactStepsScrollY = 0;

    if (g_compactSingleMode && g_hMainWnd && IsWindow(g_hMainWnd) && !IsZoomed(g_hMainWnd)) {
        RECT rcWin{};
        GetWindowRect(g_hMainWnd, &rcWin);
        RECT wa{};
        HMONITOR hMon = MonitorFromWindow(g_hMainWnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (GetMonitorInfoW(hMon, &mi)) {
            wa = mi.rcWork;
        } else {
            SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        }
        const int curW = rcWin.right - rcWin.left;
        const int curH = rcWin.bottom - rcWin.top;
        int newW = open ? (curW + COMPACT_DRAWER_W) : (std::max)(COMPACT_BASE_W, curW - COMPACT_DRAWER_W);
        if (open && newW < COMPACT_BASE_W + COMPACT_DRAWER_W) newW = COMPACT_BASE_W + COMPACT_DRAWER_W;
        if (!open && curW >= COMPACT_BASE_W + COMPACT_DRAWER_W) newW = (std::max)(COMPACT_BASE_W, curW - COMPACT_DRAWER_W);
        const int maxAvailW = (std::max)(360, static_cast<int>(wa.right - wa.left) - 16);
        if (newW > maxAvailW) newW = maxAvailW;
        int newX = rcWin.left;
        if (newX + newW > wa.right - 8) newX = (std::max)(static_cast<int>(wa.left) + 8, static_cast<int>(wa.right) - 8 - newW);
        if (newX < wa.left + 8) newX = static_cast<int>(wa.left) + 8;
        SetWindowPos(g_hMainWnd, nullptr, newX, rcWin.top, newW, curH, SWP_NOZORDER | SWP_NOACTIVATE);
        RelayoutMainPanes(g_hMainWnd);
    }
    SyncCw2HoverOverlay(true);
    SyncCompactToastOverlay(true);
    if (g_hCanvasWnd) {
        InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
        UpdateWindow(g_hCanvasWnd);
    }
}

static LRESULT CALLBACK Cw2OverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_NCHITTEST) return HTTRANSPARENT;
    if (msg == WM_ERASEBKGND) return 1;
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

static void EnsureCw2OverlayWindow()
{
    if (g_hCw2OverlayWnd && IsWindow(g_hCw2OverlayWnd)) return;
    static bool s_registered = false;
    static const wchar_t* kOverlayClass = L"CasioCw2HoverOverlayClass";
    if (!s_registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = Cw2OverlayWndProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = kOverlayClass;
        RegisterClassExW(&wc);
        s_registered = true;
    }
    g_hCw2OverlayWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kOverlayClass, L"", WS_POPUP,
        0, 0, 1, 1, g_hMainWnd, nullptr, g_hInst, nullptr);
}

static void DestroyCw2HoverOverlay()
{
    if (g_hCw2OverlayWnd && IsWindow(g_hCw2OverlayWnd)) {
        DestroyWindow(g_hCw2OverlayWnd);
    }
    g_hCw2OverlayWnd = nullptr;
    g_cw2HoverKey = -1;
    g_cw2OverlayLastHover = -2;
    g_cw2OverlayLastKeyId = -2;
    g_cw2OverlayLastStepCount = -1;
    g_cw2OverlayLastTeacher = false;
    SetRectEmpty(&g_cw2OverlayLastScreenRc);
}

static void EnsureCompactToastOverlayWindow()
{
    if (g_hCompactToastWnd && IsWindow(g_hCompactToastWnd)) return;
    static bool s_toastReg = false;
    static const wchar_t* kToastClass = L"CasioCompactToastOverlayClass";
    if (!s_toastReg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = Cw2OverlayWndProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = kToastClass;
        RegisterClassExW(&wc);
        s_toastReg = true;
    }
    g_hCompactToastWnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        kToastClass, L"", WS_POPUP,
        0, 0, 1, 1, g_hMainWnd, nullptr, g_hInst, nullptr);
}

static void DestroyCompactToastOverlay()
{
    if (g_hCompactToastWnd && IsWindow(g_hCompactToastWnd)) {
        DestroyWindow(g_hCompactToastWnd);
    }
    g_hCompactToastWnd = nullptr;
    g_compactToastLastMsg.clear();
    g_compactToastLastActive = false;
    SetRectEmpty(&g_compactToastLastRect);
}

static void SyncCompactToastOverlay(bool force)
{
    const bool toastActive =
        g_compactSingleMode &&
        !g_lastToolbarToast.empty() &&
        (GetTickCount64() - g_lastToolbarToastMs) < 3500 &&
        g_hMainWnd && IsWindow(g_hMainWnd) && IsWindowVisible(g_hMainWnd) && !IsIconic(g_hMainWnd) &&
        g_hCanvasWnd && IsWindowVisible(g_hCanvasWnd);

    if (!toastActive) {
        if (g_hCompactToastWnd && IsWindow(g_hCompactToastWnd) && IsWindowVisible(g_hCompactToastWnd)) {
            ShowWindow(g_hCompactToastWnd, SW_HIDE);
        }
        g_compactToastLastActive = false;
        g_compactToastLastMsg.clear();
        SetRectEmpty(&g_compactToastLastRect);
        return;
    }

    RECT rcC{};
    GetClientRect(g_hCanvasWnd, &rcC);
    const int barH = GetCanvasToolbarH();
    const int rightReserved = COMPACT_TAB_W + (g_compactStepsOpen ? COMPACT_DRAWER_W : 0);
    const int calcPaneW = (std::max)(160, static_cast<int>(rcC.right) - rightReserved);
    RECT rcLocal{ 10, barH + 6, (std::max)(150, calcPaneW - 10), barH + 38 };
    RECT rcScr = rcLocal;
    MapWindowPoints(g_hCanvasWnd, nullptr, reinterpret_cast<LPPOINT>(&rcScr), 2);
    const int tw = rcScr.right - rcScr.left;
    const int th = rcScr.bottom - rcScr.top;
    if (tw <= 24 || th <= 16) {
        if (g_hCompactToastWnd && IsWindowVisible(g_hCompactToastWnd)) ShowWindow(g_hCompactToastWnd, SW_HIDE);
        return;
    }

    if (!force && g_compactToastLastActive &&
        EqualRect(&rcScr, &g_compactToastLastRect) &&
        g_lastToolbarToast == g_compactToastLastMsg &&
        g_hCompactToastWnd && IsWindowVisible(g_hCompactToastWnd)) {
        return;
    }

    EnsureCompactToastOverlayWindow();
    if (!g_hCompactToastWnd || !IsWindow(g_hCompactToastWnd)) return;

    g_compactToastLastActive = true;
    g_compactToastLastMsg = g_lastToolbarToast;
    g_compactToastLastRect = rcScr;

    EnsureGdiplusInitialized();
    Gdiplus::Bitmap bmpToast(tw, th, PixelFormat32bppPARGB);
    Gdiplus::Graphics g(&bmpToast);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    const float x = 1.0f, y = 1.0f, w = static_cast<float>(tw - 2), h = static_cast<float>(th - 2), r = 8.0f;
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, r * 2.0f, r * 2.0f, 180.0f, 90.0f);
    path.AddArc(x + w - r * 2.0f, y, r * 2.0f, r * 2.0f, 270.0f, 90.0f);
    path.AddArc(x + w - r * 2.0f, y + h - r * 2.0f, r * 2.0f, r * 2.0f, 0.0f, 90.0f);
    path.AddArc(x, y + h - r * 2.0f, r * 2.0f, r * 2.0f, 90.0f, 90.0f);
    path.CloseFigure();

    Gdiplus::SolidBrush fillBr(Gdiplus::Color(242, 20, 36, 66));
    g.FillPath(&fillBr, &path);
    Gdiplus::Pen borderPen(CasioTheme::ToGpColor(CasioTheme::AccentBlue, 245), 1.4f);
    g.DrawPath(&borderPen, &path);

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fnt(&ff, 11.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush txtBr(Gdiplus::Color(255, 255, 255, 255));
    Gdiplus::StringFormat sf(Gdiplus::StringFormatFlagsNoWrap);
    sf.SetAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    g.DrawString(g_lastToolbarToast.c_str(), -1, &fnt,
        Gdiplus::RectF(x + 8.0f, y, (std::max)(8.0f, w - 16.0f), h), &sf, &txtBr);

    HBITMAP hBmpToast = nullptr;
    if (bmpToast.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBmpToast) == Gdiplus::Ok && hBmpToast) {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HGDIOBJ oldBmp = SelectObject(hdcMem, hBmpToast);
        POINT ptDst{ rcScr.left, rcScr.top };
        SIZE szDst{ tw, th };
        POINT ptSrc{ 0, 0 };
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(g_hCompactToastWnd, hdcScreen, &ptDst, &szDst, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
        SelectObject(hdcMem, oldBmp);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        DeleteObject(hBmpToast);
    }

    if (!IsWindowVisible(g_hCompactToastWnd)) {
        ShowWindow(g_hCompactToastWnd, SW_SHOWNA);
    }
    SetWindowPos(g_hCompactToastWnd, g_alwaysOnTop ? HWND_TOPMOST : HWND_TOP,
        rcScr.left, rcScr.top, tw, th,
        SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOOWNERZORDER);
}

static bool IsSecondaryFloatingPopupBlockingCw2Point(POINT ptScr)
{
    if (FindWindowW(L"#32768", nullptr) != nullptr) return true;
    struct EnumCtx {
        POINT pt;
        bool blocked;
    } ctx{ ptScr, false };
    EnumThreadWindows(GetCurrentThreadId(), [](HWND hWnd, LPARAM lParam) -> BOOL {
        auto* c = reinterpret_cast<EnumCtx*>(lParam);
        if (!hWnd || hWnd == g_hMainWnd || hWnd == g_hCw2OverlayWnd || hWnd == g_hCanvasTooltip) return TRUE;
        if (!IsWindowVisible(hWnd)) return TRUE;
        if (GetWindow(hWnd, GW_OWNER) == g_hMainWnd) {
            RECT r{};
            GetWindowRect(hWnd, &r);
            if (PtInRect(&r, c->pt)) {
                c->blocked = true;
                return FALSE;
            }
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));
    return ctx.blocked;
}

static void SyncCw2HoverOverlay(bool force)
{
    HWND hChild = Fx991CwHost::GetChildHwnd();
    const bool hostVisible = IsCw2Visible() && hChild && IsWindow(hChild) && IsWindowVisible(hChild) &&
        g_hMainWnd && IsWindow(g_hMainWnd) && IsWindowVisible(g_hMainWnd) && !IsIconic(g_hMainWnd) &&
        g_hCanvasWnd && IsWindowVisible(g_hCanvasWnd) &&
        (g_cw2ViewportRect.right - g_cw2ViewportRect.left > 24) &&
        (g_cw2ViewportRect.bottom - g_cw2ViewportRect.top > 24);

    int lastKeyId = -1;
    int stepCount = 0;
    if (g_teacherHighlightMode) {
        for (const auto& hk : g_keyHistory) {
            if (hk.calcIdx == 3 && !hk.isAppCapture && hk.keyId >= 0) {
                lastKeyId = hk.keyId;
                stepCount++;
            }
        }
    }

    if (!hostVisible || (g_cw2HoverKey < 0 && (!g_teacherHighlightMode || lastKeyId < 0))) {
        if (g_hCw2OverlayWnd && IsWindow(g_hCw2OverlayWnd) && IsWindowVisible(g_hCw2OverlayWnd)) {
            ShowWindow(g_hCw2OverlayWnd, SW_HIDE);
        }
        g_cw2OverlayLastHover = -2;
        g_cw2OverlayLastKeyId = -2;
        g_cw2OverlayLastStepCount = -1;
        g_cw2OverlayLastTeacher = false;
        SetRectEmpty(&g_cw2OverlayLastScreenRc);
        return;
    }

    RECT rcScr = g_cw2ViewportRect;
    MapWindowPoints(g_hCanvasWnd, nullptr, reinterpret_cast<LPPOINT>(&rcScr), 2);
    const int ovW = rcScr.right - rcScr.left;
    const int ovH = rcScr.bottom - rcScr.top;
    if (ovW <= 24 || ovH <= 24) {
        if (g_hCw2OverlayWnd && IsWindowVisible(g_hCw2OverlayWnd)) ShowWindow(g_hCw2OverlayWnd, SW_HIDE);
        return;
    }

    const bool sameRect = EqualRect(&rcScr, &g_cw2OverlayLastScreenRc) != FALSE;
    if (!force && sameRect &&
        g_cw2HoverKey == g_cw2OverlayLastHover &&
        lastKeyId == g_cw2OverlayLastKeyId &&
        stepCount == g_cw2OverlayLastStepCount &&
        g_teacherHighlightMode == g_cw2OverlayLastTeacher &&
        g_hCw2OverlayWnd && IsWindowVisible(g_hCw2OverlayWnd)) {
        return;
    }

    EnsureCw2OverlayWindow();
    if (!g_hCw2OverlayWnd || !IsWindow(g_hCw2OverlayWnd)) return;

    g_cw2OverlayLastHover = g_cw2HoverKey;
    g_cw2OverlayLastKeyId = lastKeyId;
    g_cw2OverlayLastStepCount = stepCount;
    g_cw2OverlayLastTeacher = g_teacherHighlightMode;
    g_cw2OverlayLastScreenRc = rcScr;

    const double sx = static_cast<double>(ovW) / 992.0;
    const double sy = static_cast<double>(ovH) / 2082.0;

    Gdiplus::Bitmap bmpLayer(ovW, ovH, PixelFormat32bppPARGB);
    Gdiplus::Graphics g(&bmpLayer);
    g.Clear(Gdiplus::Color(0, 0, 0, 0));
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);

    auto buildKeyPath = [&](const KeyVisual& kv, float pad, Gdiplus::GraphicsPath& outPath, RECT& outRc) {
        const float x0 = static_cast<float>((kv.cx - kv.w * 0.5) * sx) - pad;
        const float y0 = static_cast<float>((kv.cy - kv.h * 0.5) * sy) - pad;
        const float kw = (std::max)(8.0f, static_cast<float>(kv.w * sx) + pad * 2.0f);
        const float kh = (std::max)(8.0f, static_cast<float>(kv.h * sy) + pad * 2.0f);
        outRc = { static_cast<LONG>(x0), static_cast<LONG>(y0),
                  static_cast<LONG>(x0 + kw), static_cast<LONG>(y0 + kh) };
        const float r = (std::min)(kw, kh) * 0.45f;
        const float d = r * 2.0f;
        outPath.Reset();
        outPath.AddArc(x0, y0, d, d, 180.0f, 90.0f);
        outPath.AddArc(x0 + kw - d, y0, d, d, 270.0f, 90.0f);
        outPath.AddArc(x0 + kw - d, y0 + kh - d, d, d, 0.0f, 90.0f);
        outPath.AddArc(x0, y0 + kh - d, d, d, 90.0f, 90.0f);
        outPath.CloseFigure();
    };

    if (g_teacherHighlightMode && lastKeyId >= 0) {
        KeyVisual kvLast{};
        if (FillKeyVisual(3, lastKeyId, kvLast)) {
            Gdiplus::GraphicsPath pLast;
            RECT rLast{};
            buildKeyPath(kvLast, 1.0f, pLast, rLast);
            Gdiplus::SolidBrush fillT(CasioTheme::ToGpColor(CasioTheme::AmberWarm, 30));
            g.FillPath(&fillT, &pLast);
            Gdiplus::Pen penT(CasioTheme::ToGpColor(CasioTheme::AmberWarm, 245), 2.2f);
            g.DrawPath(&penT, &pLast);

            if (stepCount > 0 && (rLast.right - rLast.left) >= 24 && (rLast.bottom - rLast.top) >= 16) {
                const float bx = static_cast<float>((std::max)(rLast.left, rLast.right - 25));
                const float by = static_cast<float>((std::max)(rLast.top, rLast.bottom - 14));
                const float bw = static_cast<float>(rLast.right) - bx;
                const float bh = static_cast<float>(rLast.bottom) - by;
                Gdiplus::GraphicsPath bp;
                const float br = 4.0f;
                bp.AddArc(bx, by, br * 2, br * 2, 180.0f, 90.0f);
                bp.AddArc(bx + bw - br * 2, by, br * 2, br * 2, 270.0f, 90.0f);
                bp.AddArc(bx + bw - br * 2, by + bh - br * 2, br * 2, br * 2, 0.0f, 90.0f);
                bp.AddArc(bx, by + bh - br * 2, br * 2, br * 2, 90.0f, 90.0f);
                bp.CloseFigure();
                Gdiplus::SolidBrush brBadge(CasioTheme::ToGpColor(CasioTheme::AmberWarm, 250));
                g.FillPath(&brBadge, &bp);
                Gdiplus::Pen penBadge(Gdiplus::Color(240, 254, 243, 199), 1.0f);
                g.DrawPath(&penBadge, &bp);

                wchar_t badgeNum[16]{};
                wsprintfW(badgeNum, L"#%d", stepCount);
                Gdiplus::FontFamily ff(L"Segoe UI");
                Gdiplus::Font fnt(&ff, 9.5f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                Gdiplus::SolidBrush txtBr(Gdiplus::Color(255, 18, 22, 30));
                Gdiplus::StringFormat sf;
                sf.SetAlignment(Gdiplus::StringAlignmentCenter);
                sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                g.DrawString(badgeNum, -1, &fnt, Gdiplus::RectF(bx, by, bw, bh), &sf, &txtBr);
            }
        }
    }

    if (g_cw2HoverKey >= 0) {
        KeyVisual kvHov{};
        if (FillKeyVisual(3, g_cw2HoverKey, kvHov)) {
            Gdiplus::GraphicsPath pHov;
            RECT rHov{};
            buildKeyPath(kvHov, 1.0f, pHov, rHov);
            Gdiplus::SolidBrush hovFill(Gdiplus::Color(42, 95, 175, 255));
            g.FillPath(&hovFill, &pHov);
            Gdiplus::Pen hovHalo(Gdiplus::Color(115, 70, 155, 255), 4.0f);
            g.DrawPath(&hovHalo, &pHov);
            Gdiplus::Pen hovRing(Gdiplus::Color(248, 230, 242, 255), 1.9f);
            g.DrawPath(&hovRing, &pHov);
        }
    }

    HBITMAP hBmpLayer = nullptr;
    if (bmpLayer.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hBmpLayer) == Gdiplus::Ok && hBmpLayer) {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HGDIOBJ oldBmp = SelectObject(hdcMem, hBmpLayer);
        POINT ptDst{ rcScr.left, rcScr.top };
        SIZE szDst{ ovW, ovH };
        POINT ptSrc{ 0, 0 };
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        UpdateLayeredWindow(g_hCw2OverlayWnd, hdcScreen, &ptDst, &szDst, hdcMem, &ptSrc, 0, &bf, ULW_ALPHA);
        SelectObject(hdcMem, oldBmp);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
        DeleteObject(hBmpLayer);
    }

    if (!IsWindowVisible(g_hCw2OverlayWnd)) {
        ShowWindow(g_hCw2OverlayWnd, SW_SHOWNA);
    }
    SetWindowPos(g_hCw2OverlayWnd, g_alwaysOnTop ? HWND_TOPMOST : HWND_TOP,
        rcScr.left, rcScr.top, ovW, ovH,
        SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOOWNERZORDER);
}

static void PollCw2HoverKey()
{
    HWND hChild = Fx991CwHost::GetChildHwnd();
    if (!IsCw2Visible() || !hChild || !IsWindow(hChild) || !IsWindowVisible(hChild) ||
        !g_hMainWnd || !IsWindowVisible(g_hMainWnd) || IsIconic(g_hMainWnd)) {
        if (g_cw2HoverKey != -1 || (g_hCw2OverlayWnd && IsWindowVisible(g_hCw2OverlayWnd))) {
            g_cw2HoverKey = -1;
            SyncCw2HoverOverlay(true);
        }
        return;
    }

    POINT ptScr{};
    GetCursorPos(&ptScr);
    RECT rcScr = g_cw2ViewportRect;
    MapWindowPoints(g_hCanvasWnd, nullptr, reinterpret_cast<LPPOINT>(&rcScr), 2);

    int nextHover = -1;
    if (PtInRect(&rcScr, ptScr) && !IsSecondaryFloatingPopupBlockingCw2Point(ptScr)) {
        HWND hAt = WindowFromPoint(ptScr);
        if (hAt == hChild || IsChild(hChild, hAt) || hAt == g_hCw2OverlayWnd) {
            RECT rcChild{};
            GetClientRect(hChild, &rcChild);
            const int childW = rcChild.right - rcChild.left;
            const int childH = rcChild.bottom - rcChild.top;
            if (childW > 0 && childH > 0) {
                POINT ptC = ptScr;
                ScreenToClient(hChild, &ptC);
                const int skinX = MulDiv(ptC.x, 992, childW);
                const int skinY = MulDiv(ptC.y, 2082, childH);
                nextHover = HitTestCw2SkinKey(skinX, skinY);
            }
        }
    }

    if (nextHover != g_cw2HoverKey) {
        g_cw2HoverKey = nextHover;
        if (nextHover >= 0 && g_keyExplorerEnabled && !g_explorerPinned) {
            OpenExplorerForKey(3, nextHover, false, -1);
        }
        SyncCw2HoverOverlay(true);
    } else {
        SyncCw2HoverOverlay(false);
    }
}

static void MaybeCapturePreEntryAppSnapshot(int calcIdx, int keyId)
{
    if (calcIdx < 0 || calcIdx > 3 || keyId < 0) return;

    bool isOpenKey = false;
    bool isArrowOnly = false;
    bool isNavKey = false;
    bool isCancelKey = false;
    bool isEnterKey = false;

    if (calcIdx == 0) {
        isOpenKey = (keyId == 6);
        isCancelKey = (keyId == 7 || keyId == 34);
        isArrowOnly = (keyId >= 2 && keyId <= 5);
        isNavKey = (keyId >= 0 && keyId <= 6);
        isEnterKey = (keyId == 49) || (keyId >= 18 && keyId <= 23) ||
                     (keyId >= 30 && keyId <= 32) || (keyId >= 35 && keyId <= 37) ||
                     (keyId >= 40 && keyId <= 42) || (keyId == 45);
    } else if (calcIdx == 1) {
        isOpenKey = (keyId == 4);
        isCancelKey = (keyId == 1 || keyId == 34);
        isArrowOnly = (keyId == 2 || keyId == 5 || keyId == 6 || keyId == 7);
        isNavKey = (keyId == 0 || keyId == 2 || keyId == 3 || keyId == 4 || keyId == 5 || keyId == 6 || keyId == 7);
        isEnterKey = (keyId == 49 || keyId == 40 || keyId == 41 || keyId == 42 ||
                      keyId == 35 || keyId == 36 || keyId == 37 || keyId == 30 || keyId == 31);
    } else if (calcIdx == 2) {
        isOpenKey = (keyId == 9);
        isCancelKey = false;
        isArrowOnly = (keyId >= 10 && keyId <= 13);
        isNavKey = (keyId == 6 || keyId == 9 || (keyId >= 10 && keyId <= 14) || keyId == 17 || keyId == 34);
        isEnterKey = (keyId == 49) || (keyId >= 18 && keyId <= 29) ||
                     (keyId >= 30 && keyId <= 32) || (keyId >= 35 && keyId <= 37) ||
                     (keyId >= 40 && keyId <= 42) || (keyId == 45);
    } else {
        isOpenKey = (keyId == 1 || keyId == 4 || keyId == 12 || keyId == 13 || keyId == 14 || keyId == 15);
        isCancelKey = (keyId == 0 || keyId == 5 || keyId == 32);
        isArrowOnly = (keyId == 2 || keyId == 3 || keyId == 6 || keyId == 8 || keyId == 9 || keyId == 10);
        isNavKey = isOpenKey || isArrowOnly || (keyId == 11);
        isEnterKey = (keyId == 7 || keyId == 47);
    }

    if (!g_inAppMenuState[calcIdx] && g_awaitingSubmenuNav[calcIdx]) {
        if (isArrowOnly || (calcIdx >= 2 && keyId == 7)) {
            g_inAppMenuState[calcIdx] = true;
            g_awaitingSubmenuNav[calcIdx] = false;
        } else if (!isOpenKey) {
            g_awaitingSubmenuNav[calcIdx] = false;
        }
    }

    if (g_inAppMenuState[calcIdx]) {
        if (isEnterKey) {
            const ULONGLONG now = GetTickCount64();
            if (now - g_lastPreEntryCapMs[calcIdx] >= 150) {
                HBITMAP hCapBmp = nullptr;
                int capW = 0;
                int capH = 0;
                if (calcIdx == 3) {
                    hCapBmp = Fx991CwHost::CaptureBitmap(false, capW, capH);
                } else {
                    auto& c = g_nativeCalcs[calcIdx];
                    if (c.hSkinBmp && c.skinW > 0 && c.skinH > 0) {
                        capW = 420;
                        capH = (std::max)(1, MulDiv(capW, c.skinH, c.skinW));
                        HDC hdcScreen = GetDC(nullptr);
                        HDC hdcOff = CreateCompatibleDC(hdcScreen);
                        hCapBmp = CreateCompatibleBitmap(hdcScreen, capW, capH);
                        if (hdcOff && hCapBmp) {
                            HGDIOBJ oldB = SelectObject(hdcOff, hCapBmp);
                            const int savedHov = c.hoverKey;
                            const int savedPrs = c.pressedKey;
                            c.hoverKey = -1;
                            c.pressedKey = -1;
                            RECT rOff{ 0, 0, capW, capH };
                            HBRUSH brBg = CreateSolidBrush(CasioTheme::StudioSurface());
                            FillRect(hdcOff, &rOff, brBg);
                            DeleteObject(brBg);
                            DrawCalculator(hdcOff, calcIdx, rOff);
                            c.hoverKey = savedHov;
                            c.pressedKey = savedPrs;
                            SelectObject(hdcOff, oldB);
                        }
                        if (hdcOff) DeleteDC(hdcOff);
                        ReleaseDC(nullptr, hdcScreen);
                    }
                }

                if (hCapBmp && capW > 0 && capH > 0) {
                    OperationCaptureItem capItem{};
                    const std::wstring keyLbl = KeyHistoryCaption(calcIdx, keyId);
                    if (SaveBitmapAsOperationCapture(hCapBmp, capW, capH, calcIdx,
                            g_nativeCalcs[calcIdx].displayName, keyId, keyLbl, L"appmenu", capItem)) {
                        int lcdW = 0, lcdH = 0;
                        HBITMAP hLcdBmp = CaptureLcdSnapshot(calcIdx, 2, lcdW, lcdH);
                        if (hLcdBmp && lcdW > 0 && lcdH > 0) {
                            StoreAppMenuThumb(capItem.filePath, hLcdBmp, lcdW, lcdH);
                            DeleteObject(hLcdBmp);
                        } else {
                            if (hLcdBmp) DeleteObject(hLcdBmp);
                            StoreAppMenuThumb(capItem.filePath, hCapBmp, capW, capH);
                        }

                        g_operationCaptures.push_back(capItem);
                        if (g_operationCaptures.size() > 60) {
                            if (g_operationCaptures.front().hThumb) DeleteObject(g_operationCaptures.front().hThumb);
                            g_operationCaptures.erase(g_operationCaptures.begin());
                        }

                        KeyLogEntry lcdEntry;
                        lcdEntry.calcIdx = calcIdx;
                        lcdEntry.keyId = -1;
                        lcdEntry.isAppCapture = true;
                        lcdEntry.captureFilePath = capItem.filePath;
                        lcdEntry.captureLabel = L"Pre-" + keyLbl;
                        lcdEntry.keyName = lcdEntry.captureLabel;
                        g_keyHistory.push_back(std::move(lcdEntry));

                        g_lastPreEntryCapMs[calcIdx] = now;
                        ShowToolbarToast(std::wstring(L"Captura del men\u00FA antes de entrar: ") + g_nativeCalcs[calcIdx].displayName);
                        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
                        if (g_compactSingleMode && g_compactStepsOpen && g_hCanvasWnd) InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
                    }
                    DeleteObject(hCapBmp);
                } else if (hCapBmp) {
                    DeleteObject(hCapBmp);
                }
            }
            g_inAppMenuState[calcIdx] = false;
            g_awaitingSubmenuNav[calcIdx] = true;
        } else if (isCancelKey || !isNavKey) {
            g_inAppMenuState[calcIdx] = false;
            g_awaitingSubmenuNav[calcIdx] = false;
        }
    }

    if (isOpenKey) {
        g_inAppMenuState[calcIdx] = true;
        g_awaitingSubmenuNav[calcIdx] = false;
    }
}

// ── Interactive Canvas Procedure ────────────────────────────────────────────

LRESULT CALLBACK CanvasWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        InitCanvasTooltip(hWnd);
        return 0;
    }
    case WM_PARENTNOTIFY: {
        if (LOWORD(wParam) == WM_LBUTTONDOWN && IsCw2Visible()) {
            if (g_activeCalcIdx != 3) {
                g_activeCalcIdx = 3;
                InvalidateRect(hWnd, nullptr, FALSE);
                if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                    InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
            }
            HWND hChild = Fx991CwHost::GetChildHwnd();
            if (hChild && IsWindowVisible(hChild)) {
                RECT rcChild{};
                GetClientRect(hChild, &rcChild);
                const int childW = rcChild.right - rcChild.left;
                const int childH = rcChild.bottom - rcChild.top;
                if (childW > 0 && childH > 0) {
                    POINT ptChild{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                    ClientToScreen(hWnd, &ptChild);
                    ScreenToClient(hChild, &ptChild);
                    if (ptChild.x >= 0 && ptChild.x < childW && ptChild.y >= 0 && ptChild.y < childH) {
                        const int skinX = MulDiv(ptChild.x, 992, childW);
                        const int skinY = MulDiv(ptChild.y, 2082, childH);
                        const int k = HitTestCw2SkinKey(skinX, skinY);
                        if (k >= 0) {
                            const ULONGLONG now = GetTickCount64();
                            const bool suppressed = (k == g_cw2SuppressParentNotifyForKey && now <= g_cw2SuppressParentNotifyUntil);
                            if (!suppressed) {
                                MaybeCapturePreEntryAppSnapshot(3, k);
                                RecordCw2KeyActivation(k);
                            }
                        }
                    }
                }
            }
        }
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_PRINTCLIENT:
    case WM_PAINT: {
        PAINTSTRUCT ps{};
        const bool isPrintClient = (msg == WM_PRINTCLIENT);
        HDC hdc = isPrintClient ? reinterpret_cast<HDC>(wParam) : BeginPaint(hWnd, &ps);
        RECT rc{};
        GetClientRect(hWnd, &rc);
        if (!hdc || rc.right <= 0 || rc.bottom <= 0) {
            if (!isPrintClient) EndPaint(hWnd, &ps);
            return 0;
        }

        for (int i = 0; i < 4; ++i) {
            SetRectEmpty(&g_calcQuickCompactLogRc[i]);
            SetRectEmpty(&g_calcQuickPinLogRc[i]);
        }
        SetRectEmpty(&g_compactPullTabRc);
        SetRectEmpty(&g_compactDrawerShareRc);
        SetRectEmpty(&g_compactDrawerClearRc);
        SetRectEmpty(&g_compactDrawerScrollAreaRc);
        g_compactDrawerStepHits.clear();

        // Double buffer in exact physical pixels
        HDC hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hBmpMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBmpMem);

        CasioTheme::DrawStudioAtmosphericBackdrop(hdcMem, rc);

        const float cScale = g_compactSingleMode ? 1.0f : GetCanvasUiScale(rc.right);
        if (!g_compactSingleMode) CasioTheme::g_canvasUiScale = cScale;
        const RECT rcLogical = CasioTheme::DeviceRectToLogical(rc, cScale);
        const bool compareMode = !g_compactSingleMode && IsCompareViewMode(g_viewMode);
        const StudioToolbarLayout tl = ComputeStudioToolbarLayout(
            g_compactSingleMode ? static_cast<int>(rc.right) : static_cast<int>(rcLogical.right),
            g_compactSingleMode ? static_cast<int>(rc.bottom) : static_cast<int>(rcLogical.bottom),
            g_compactSingleMode,
            compareMode,
            g_userZoomFactor > 0.05);
        const bool popupOpen = (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd));

        if (g_compactSingleMode) {
            // ── Compact Vertical Mode Top Toolbar (guías de interfaz material translúcido 2.0, Light/Dark/HC) ──
            RECT rcBar = { 0, 0, rc.right, EMULATOR_TOOLBAR_H };
            CasioTheme::DrawTranslucentHeaderStrip(hdcMem, rcBar, true);
            RECT rcPillsCap = { tl.rPills[0].left - 3, 3, tl.rPills[3].right + 3, EMULATOR_TOOLBAR_H - 3 };
            CasioTheme::DrawFloatingGlassCapsule(hdcMem, rcPillsCap, 1.0f, -1.0f, false);

            auto drawActiveBtnHighlight = [&](const RECT& rBtn, bool active) {
                if (!active) return;
                CasioTheme::FillRoundRectAA(hdcMem, rBtn, 6.0f,
                    CasioTheme::StudioAccentSoft(), CasioTheme::StudioAccent(), 1.0f);
            };

            drawActiveBtnHighlight(tl.rB[0], g_compactSingleMode);
            drawActiveBtnHighlight(tl.rB[1], g_alwaysOnTop);
            drawActiveBtnHighlight(tl.rB[2], popupOpen);
            drawActiveBtnHighlight(tl.rB[4], g_popupShowGrid);

            const COLORREF icIdle = CasioTheme::StudioTextSec();
            const COLORREF icAct  = CasioTheme::StudioAccentText();
            DrawCompactViewIconGdi(hdcMem, tl.rB[0], icAct);
            DrawPadlockIconGdi(hdcMem,     tl.rB[1], g_alwaysOnTop, g_alwaysOnTop ? icAct : icIdle);
            DrawPopupIconGdi(hdcMem,       tl.rB[2], popupOpen ? icAct : icIdle);
            DrawCameraIconGdi(hdcMem,      tl.rB[3], icIdle);
            DrawGridIconGdi(hdcMem,        tl.rB[4], g_popupShowGrid ? icAct : icIdle);

            SetBkMode(hdcMem, TRANSPARENT);
            const wchar_t* cNames[4] = { L"Blanca", L"Gris", L"CG50", L"991CW" };
            for (int i = 0; i < 4; ++i) {
                const bool vis = (i == 3) ? IsCw2Visible() : g_nativeCalcs[i].isVisible;
                bool act = (g_activeCalcIdx == i && (!g_compactSingleMode || vis));
                CasioTheme::DrawStudioSegmentItem(hdcMem, tl.rPills[i], cNames[i],
                    g_fontSmall, act, false, true);
            }

            CasioTheme::DrawStudioButton(hdcMem, tl.rHelp, L"? PDF", g_fontSmall,
                CasioTheme::BTN_SUCCESS, true, false, 6.0f);
        } else {
            // ── Restrained guías de interfaz Studio Header & Spatial Stage ──────────
            g_studioMenuBtnLogicalRc = tl.rMenu;

            auto getModelReady = [](int idx) -> bool {
                if (idx == 3) return Fx991CwHost::GetState() == Fx991CwState::Ready;
                if (idx == 2) return g_nativeCalcs[2].cg50 && g_nativeCalcs[2].cg50->GetState() == Cg50Backend::State::Ready;
                return g_nativeCalcs[idx].emu != nullptr;
            };
            auto getModelStarting = [&](int idx) -> bool {
                if (idx == 3) return Fx991CwHost::GetState() == Fx991CwState::Launching;
                if (idx == 2) return !getModelReady(2) && g_cg50InitRunning;
                return false;
            };
            static const wchar_t* kModelShort[4] = {
                L"fx-570LA X", L"fx-570VN+", L"fx-CG50", L"fx-991CW"
            };
            static const wchar_t* kModelSub[4] = {
                L"ClassWiz",
                L"Natural-V.P.A.M.",
                L"Graficadora PRIZM",
                L"ClassWiz II"
            };

            // 1. Hardware pedestals / grounded shadows in exact 1:1 physical coordinates
            {
                const float haloR = 14.0f * cScale;
                for (int i = 0; i < 3; ++i) {
                    if (g_nativeCalcs[i].isVisible) {
                        CasioTheme::DrawCalculatorAmbientHalo(hdcMem, g_nativeCalcs[i].viewportRect,
                            compareMode && (g_activeCalcIdx == i), haloR, compareMode, cScale);
                    }
                }
                if (IsCw2Visible()) {
                    CasioTheme::DrawCalculatorAmbientHalo(hdcMem, g_cw2ViewportRect,
                        compareMode && (g_activeCalcIdx == 3), haloR, compareMode, cScale);
                }
            }

            // 2. Top bar, floating material translúcido 2.0 capsules, and stage chrome in logical UI coordinates
            CasioTheme::UiScaleScope scaleScope(hdcMem, cScale);
            SetBkMode(hdcMem, TRANSPARENT);

            // Soft atmospheric material translúcido 2.0 header band (ScrollEdgeEffectStyle.soft, no heavy opaque slab)
            RECT rcBar = { 0, 0, rcLogical.right, STUDIO_TOOLBAR_H };
            CasioTheme::DrawTranslucentHeaderStrip(hdcMem, rcBar, false);

            if (tl.rBrand.right > tl.rBrand.left) {
                {
                    Gdiplus::Graphics gOrb(hdcMem);
                    const auto palB = CasioTheme::ActiveGlassPalette();
                    UiGlass::DrawAquaVitreousOrb(gOrb,
                        static_cast<float>(tl.rBrand.left) + 7.0f,
                        static_cast<float>(tl.rBrand.top + tl.rBrand.bottom) * 0.5f,
                        5.2f,
                        UiGlass::ToGdip(palB.accentBlue, 245),
                        false,
                        CasioTheme::GetActiveGlassSettings().IsDark());
                }
                HFONT hBrandF = CasioTheme::CreateUiFont(15, true);
                HGDIOBJ oldBF = SelectObject(hdcMem, hBrandF);
                SetTextColor(hdcMem, CasioTheme::StudioText());
                RECT rBrandTxt = { tl.rBrand.left + 18, tl.rBrand.top, tl.rBrand.right, tl.rBrand.bottom };
                DrawTextW(hdcMem, L"CASIO", -1, &rBrandTxt,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                SelectObject(hdcMem, oldBF);
                DeleteObject(hBrandF);
            }

            // Floating material translúcido 2.0 Segmented Workspace Switcher: [ Calculadora | Comparar ]
            CasioTheme::DrawFloatingGlassCapsule(hdcMem, tl.rWorkspaceGrp, cScale, -1.0f, false);
            CasioTheme::DrawStudioSegmentItem(hdcMem, tl.rWorkspacePills[0],
                tl.ultraNarrow ? L"Calc." : L"Calculadora",
                !compareMode ? g_fontBold : g_fontSmall,
                !compareMode, (g_canvasHoverBtnId == ID_CMD_WORKSPACE_CALC), true);
            CasioTheme::DrawStudioSegmentItem(hdcMem, tl.rWorkspacePills[1],
                L"Comparar",
                compareMode ? g_fontBold : g_fontSmall,
                compareMode, (g_canvasHoverBtnId == ID_CMD_WORKSPACE_COMPARE), true);

            // Floating material translúcido 2.0 Right Utility Cluster: [ (% zoom) | Historial | Exportar | Ayuda | ... ]
            {
                LONG utilLeft = tl.rAprender.left;
                if (tl.rZoomReset.right > tl.rZoomReset.left) {
                    utilLeft = (std::min)(utilLeft, tl.rZoomReset.left);
                }
                RECT rUtilCapsule = { utilLeft - 4, 9, tl.rMenu.right + 4, 43 };
                CasioTheme::DrawFloatingGlassCapsule(hdcMem, rUtilCapsule, cScale, -1.0f, false);
            }
            if (tl.rZoomReset.right > tl.rZoomReset.left) {
                const int zPct = static_cast<int>(std::lround(g_userZoomFactor * 100.0));
                const std::wstring zLbl = std::to_wstring(zPct) + L"% \u00D7";
                CasioTheme::DrawStudioButton(hdcMem, tl.rZoomReset, zLbl, g_fontSmall,
                    CasioTheme::BTN_TOOLBAR_GHOST, false, (g_canvasHoverBtnId == ID_BTN_ZOOM_RESET), 14.0f);
            }
            CasioTheme::DrawStudioButton(hdcMem, tl.rAprender, L"Historial",
                g_learningDrawerOpen ? g_fontBold : g_fontSmall,
                CasioTheme::BTN_TOOLBAR_GHOST, g_learningDrawerOpen,
                (g_canvasHoverBtnId == ID_CMD_TOGGLE_APRENDER), 14.0f);
            if (tl.rGuia.right > tl.rGuia.left) {
                CasioTheme::DrawStudioButton(hdcMem, tl.rGuia, L"Exportar", g_fontSmall,
                    CasioTheme::BTN_TOOLBAR_GHOST, false, (g_canvasHoverBtnId == ID_CMD_EXPORTER), 14.0f);
            }
            CasioTheme::DrawStudioButton(hdcMem, tl.rHelp, L"Ayuda", g_fontSmall,
                CasioTheme::BTN_TOOLBAR_GHOST, false, (g_canvasHoverBtnId == ID_BTN_SMART_HELP), 14.0f);
            CasioTheme::DrawStudioButton(hdcMem, tl.rMenu, L"\u2022\u2022\u2022", g_fontBold,
                CasioTheme::BTN_TOOLBAR_GHOST, false, (g_canvasHoverBtnId == ID_CMD_STUDIO_MENU), 14.0f);

            auto drawQuickPair = [&](int idx, const RECT& rComp, const RECT& rPin) {
                if (idx < 0 || idx > 3) return;
                g_calcQuickCompactLogRc[idx] = rComp;
                g_calcQuickPinLogRc[idx]     = rPin;

                const bool compHov = (g_canvasHoverBtnId == ID_QUICK_COMPACT_BASE + idx);
                const bool pinHov  = (g_canvasHoverBtnId == ID_QUICK_PIN_BASE + idx);

                CasioTheme::FillRoundRectAA(hdcMem, rComp, 6.0f,
                    compHov ? CasioTheme::StudioTrack() : CasioTheme::StudioSurface(),
                    compHov ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder(), 1.0f);
                DrawCompactQuickIconGdi(hdcMem, rComp,
                    compHov ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextSec());

                CasioTheme::FillRoundRectAA(hdcMem, rPin, 6.0f,
                    g_alwaysOnTop ? CasioTheme::StudioAccentSoft()
                        : (pinHov ? CasioTheme::StudioTrack() : CasioTheme::StudioSurface()),
                    (g_alwaysOnTop || pinHov) ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder(), 1.0f);
                DrawPadlockIconGdi(hdcMem, rPin, g_alwaysOnTop,
                    g_alwaysOnTop ? CasioTheme::StudioAccentText()
                        : (pinHov ? CasioTheme::StudioText() : CasioTheme::StudioTextSec()));
            };

            if (compareMode) {
                // Floating material translúcido 2.0 Compare Sub-Mode Capsule
                CasioTheme::DrawFloatingGlassCapsule(hdcMem, tl.rSubCompareGrp, cScale, -1.0f, false);
                CasioTheme::DrawStudioSegmentItem(hdcMem, tl.rSubComparePills[0],
                    L"Blanca + Gris",
                    g_viewMode == VIEW_DUAL_SCIENTIFIC ? g_fontBold : g_fontSmall,
                    g_viewMode == VIEW_DUAL_SCIENTIFIC, (g_canvasHoverBtnId == ID_BTN_MODE_DUAL), false);
                CasioTheme::DrawStudioSegmentItem(hdcMem, tl.rSubComparePills[1],
                    L"Todas",
                    g_viewMode == VIEW_ALL_THREE ? g_fontBold : g_fontSmall,
                    g_viewMode == VIEW_ALL_THREE, (g_canvasHoverBtnId == ID_BTN_MODE_ALL), false);

                if (tl.rSubFocusBadge.right > tl.rSubFocusBadge.left) {
                    const int actIdx = (g_activeCalcIdx >= 0 && g_activeCalcIdx <= 3) ? g_activeCalcIdx : 0;
                    CasioTheme::DrawDotIndicator(hdcMem, tl.rSubFocusBadge.left + 8,
                        (tl.rSubFocusBadge.top + tl.rSubFocusBadge.bottom) / 2, 3, CasioTheme::StudioAccent());
                    RECT rFoc = { tl.rSubFocusBadge.left + 17, tl.rSubFocusBadge.top, tl.rSubFocusBadge.right, tl.rSubFocusBadge.bottom };
                    SelectObject(hdcMem, g_fontSmall);
                    SetTextColor(hdcMem, CasioTheme::StudioTextSec());
                    std::wstring focTxt = std::wstring(L"Teclado: ") + kModelShort[actIdx];
                    DrawTextW(hdcMem, focTxt.c_str(), -1, &rFoc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }

                // Clean header inside top of each pedestal card (no redundant 'Lista' tags or subtitles)
                auto drawFloatingHeader = [&](int idx, const RECT& vpPhys) {
                    const RECT vp = CasioTheme::DeviceRectToLogical(vpPhys, cScale);
                    const int w = vp.right - vp.left;
                    if (w < 60) return;
                    const bool act = (idx == g_activeCalcIdx);
                    const bool ready = getModelReady(idx);
                    const bool starting = getModelStarting(idx);
                    const int lblTop = vp.top - 26;
                    const int lblBot = vp.top - 5;
                    const COLORREF dotCol = ready ? CasioTheme::StudioSuccess()
                                        : (starting ? CasioTheme::StudioAmber() : CasioTheme::StudioRose());

                    RECT rRow{ vp.left - 3, lblTop, vp.right + 3, lblBot };
                    CasioTheme::DrawDotIndicator(hdcMem, rRow.left + 6, (lblTop + lblBot) / 2, 3, dotCol);

                    const int qW = 20;
                    const int qH = 18;
                    const int qGap = 3;
                    const int qTop = (lblTop + lblBot - qH) / 2;
                    RECT rPin{ rRow.right - qW, qTop, rRow.right, qTop + qH };
                    RECT rComp{ rPin.left - qGap - qW, qTop, rPin.left - qGap, qTop + qH };
                    drawQuickPair(idx, rComp, rPin);

                    const int badgeW = act ? 50 : 0;
                    RECT rBadge{ rComp.left - 4 - badgeW, lblTop + 2, rComp.left - 4, lblBot - 2 };
                    RECT rName{ rRow.left + 15, lblTop, (act ? rBadge.left : rComp.left) - 4, lblBot };
                    SelectObject(hdcMem, act ? g_fontBold : g_fontSmall);
                    SetTextColor(hdcMem, act ? CasioTheme::StudioText() : CasioTheme::StudioTextSec());
                    DrawTextW(hdcMem, kModelShort[idx], -1, &rName, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

                    if (act) {
                        CasioTheme::DrawModernPill(hdcMem, rBadge, CasioTheme::StudioAccentSoft(),
                            CasioTheme::StudioBorderStrong(), CasioTheme::StudioAccentText(), L"Activa", g_fontSmall, 8.0f);
                    }
                };
                for (int i = 0; i < 3; ++i) {
                    if (g_nativeCalcs[i].isVisible) drawFloatingHeader(i, g_nativeCalcs[i].viewportRect);
                }
                if (IsCw2Visible()) {
                    drawFloatingHeader(3, g_cw2ViewportRect);
                }
            } else {
                // Solo Calculadora: clean understated model caption top-left + floating material translúcido bottom dock
                int soloIdx = IsCw2Visible() ? 3 : (g_nativeCalcs[2].isVisible ? 2 : (g_nativeCalcs[1].isVisible ? 1 : 0));
                const RECT vpPhys = (soloIdx == 3) ? g_cw2ViewportRect : g_nativeCalcs[soloIdx].viewportRect;
                const RECT vpLog = CasioTheme::DeviceRectToLogical(vpPhys, cScale);
                if (vpLog.left >= 180) {
                    const bool ready = getModelReady(soloIdx);
                    const bool starting = getModelStarting(soloIdx);
                    const int infoLeft = 28;
                    const int infoRight = vpLog.left - 24;
                    const int infoTop = STUDIO_TOOLBAR_H + 22;

                    const COLORREF dotCol = ready ? CasioTheme::StudioSuccess()
                                        : (starting ? CasioTheme::StudioAmber() : CasioTheme::StudioRose());
                    CasioTheme::DrawDotIndicator(hdcMem, infoLeft + 4, infoTop + 12, 3, dotCol);

                    HFONT hHero = CasioTheme::CreateUiFont(17, true);
                    HGDIOBJ oHero = SelectObject(hdcMem, hHero);
                    SetTextColor(hdcMem, CasioTheme::StudioText());
                    RECT rHero = { infoLeft + 14, infoTop, infoRight, infoTop + 24 };
                    DrawTextW(hdcMem, kModelShort[soloIdx], -1, &rHero, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    SelectObject(hdcMem, oHero);
                    DeleteObject(hHero);

                    SelectObject(hdcMem, g_fontSmall);
                    SetTextColor(hdcMem, CasioTheme::StudioTextSec());
                    RECT rSub = { infoLeft + 14, infoTop + 24, infoRight, infoTop + 44 };
                    DrawTextW(hdcMem, kModelSub[soloIdx], -1, &rSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }

                {
                    const int qW = 24;
                    const int qH = 22;
                    const int qGap = 5;
                    int qx = vpLog.right + 12;
                    int qy = (std::max)(STUDIO_TOOLBAR_H + 8, static_cast<int>(vpLog.top) + 4);
                    if (qx + qW * 2 + qGap > rcLogical.right - 8) {
                        qx = (std::max)(8, static_cast<int>(rcLogical.right) - 8 - (qW * 2 + qGap));
                        qy = STUDIO_TOOLBAR_H + 8;
                    }
                    RECT rComp{ qx, qy, qx + qW, qy + qH };
                    RECT rPin{ rComp.right + qGap, qy, rComp.right + qGap + qW, qy + qH };
                    drawQuickPair(soloIdx, rComp, rPin);
                }

                if (tl.rDockOuter.right > tl.rDockOuter.left) {
                    CasioTheme::DrawFloatingGlassCapsule(hdcMem, tl.rDockOuter, cScale, -1.0f, false);
                    static const wchar_t* kDockWide[4]  = { L"fx-991CW", L"fx-570LA X", L"fx-570VN+", L"fx-CG50" };
                    static const wchar_t* kDockShort[4] = { L"991CW", L"Blanca", L"Gris", L"CG50" };
                    const wchar_t** dockLbls = (rcLogical.right < 520) ? kDockShort : kDockWide;
                    for (int i = 0; i < 4; ++i) {
                        const bool act = (soloIdx == kDockCalcIdx[i]);
                        const bool hov = (g_canvasHoverBtnId == static_cast<int>(kDockCmds[i]));
                        CasioTheme::DrawStudioSegmentItem(hdcMem, tl.rDockPills[i], dockLbls[i],
                            act ? g_fontBold : g_fontSmall, act, hov, false);
                    }
                }
            }
        }

        for (int i = 0; i < 3; ++i) {
            if (g_nativeCalcs[i].isVisible) {
                DrawCalculator(hdcMem, i, g_nativeCalcs[i].viewportRect);
            }
        }
        if (IsCw2Visible() && !Fx991CwHost::GetChildHwnd()) {
            const Fx991CwState cwState = Fx991CwHost::GetState();
            RECT vp = g_cw2ViewportRect;
            if (vp.right > vp.left && vp.bottom > vp.top) {
                if (g_nativeCalcs[3].hSkinBmp) {
                    DrawCalculator(hdcMem, 3, vp);
                } else {
                    HBRUSH brBody = CreateSolidBrush(RGB(28, 33, 42));
                    FillRect(hdcMem, &vp, brBody);
                    DeleteObject(brBody);
                }
                RECT rMsg = { vp.left + 18, vp.top + ((vp.bottom - vp.top) * 16) / 100, vp.right - 18, vp.top + ((vp.bottom - vp.top) * 26) / 100 };
                if (rMsg.bottom - rMsg.top < 36) { rMsg = vp; InflateRect(&rMsg, -12, -12); }
                std::wstring cwMsg = (cwState == Fx991CwState::Failed || cwState == Fx991CwState::NotFound)
                    ? (Fx991CwHost::GetLastError().empty() ? L"fx-991CW no disponible" : Fx991CwHost::GetLastError())
                    : L"Iniciando fx-991CW...";
                CasioTheme::DrawModernPill(hdcMem, rMsg, CasioTheme::Surface1, CasioTheme::BorderSubtle,
                    CasioTheme::TextSecondary, cwMsg, g_fontSmall, 9.0f);
            }
        }

        if (g_compactSingleMode) {
            const int topY = EMULATOR_TOOLBAR_H;
            const int tabLeft = (std::max)(240, static_cast<int>(rc.right) - COMPACT_TAB_W);
            g_compactPullTabRc = { tabLeft, topY, rc.right, rc.bottom };

            std::vector<size_t> stepIndices;
            stepIndices.reserve(g_keyHistory.size());
            for (size_t k = 0; k < g_keyHistory.size(); ++k) {
                if (g_keyHistory[k].calcIdx == g_activeCalcIdx) {
                    stepIndices.push_back(k);
                }
            }
            const int stepCount = static_cast<int>(stepIndices.size());

            if (g_compactStepsOpen) {
                const int drawerRight = tabLeft;
                const int drawerLeft = (std::max)(180, drawerRight - COMPACT_DRAWER_W);
                if (drawerRight - drawerLeft >= 140) {
                    RECT rDrawer{ drawerLeft, topY, drawerRight, rc.bottom };
                    HBRUSH brDrw = CreateSolidBrush(CasioTheme::StudioSurface());
                    FillRect(hdcMem, &rDrawer, brDrw);
                    DeleteObject(brDrw);

                    HPEN penEdge = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
                    HGDIOBJ oldEdge = SelectObject(hdcMem, penEdge);
                    MoveToEx(hdcMem, drawerLeft, topY, nullptr);
                    LineTo(hdcMem, drawerLeft, rc.bottom);
                    SelectObject(hdcMem, oldEdge);
                    DeleteObject(penEdge);

                    int keyStepCount = 0;
                    for (size_t kIdx : stepIndices) {
                        if (!g_keyHistory[kIdx].isAppCapture) keyStepCount++;
                    }
                    const std::wstring cntStr = std::to_wstring(keyStepCount) + (keyStepCount == 1 ? L" paso" : L" pasos");
                    const int trashW = 28;
                    const int pillW = 78;
                    g_compactDrawerClearRc = { drawerRight - 10 - trashW, topY + 8, drawerRight - 10, topY + 32 };
                    RECT rCntPill{ g_compactDrawerClearRc.left - 6 - pillW, topY + 9, g_compactDrawerClearRc.left - 6, topY + 31 };

                    SetBkMode(hdcMem, TRANSPARENT);
                    RECT rHead{ drawerLeft + 12, topY + 8, rCntPill.left - 6, topY + 32 };
                    SelectObject(hdcMem, g_fontBold);
                    SetTextColor(hdcMem, CasioTheme::StudioText());
                    DrawTextW(hdcMem, L"Secuencia", -1, &rHead, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                    CasioTheme::DrawModernPill(hdcMem, rCntPill,
                        keyStepCount > 0 ? CasioTheme::StudioAccentSoft() : CasioTheme::StudioSurfaceAlt(),
                        CasioTheme::StudioBorder(),
                        keyStepCount > 0 ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextSec(),
                        cntStr, g_fontSmall, 6.0f);

                    const bool clrHov = (g_canvasHoverBtnId == ID_COMPACT_CLEAR);
                    CasioTheme::FillRoundRectAA(hdcMem, g_compactDrawerClearRc, 6.0f,
                        CasioTheme::StudioSurfaceAlt(),
                        clrHov ? CasioTheme::StudioBorderStrong() : CasioTheme::StudioBorder(), 1.0f);
                    CasioTheme::DrawTrashIconGdi(hdcMem, g_compactDrawerClearRc,
                        clrHov ? CasioTheme::StudioRose() : CasioTheme::StudioTextSec());

                    g_compactDrawerShareRc = { drawerLeft + 12, topY + 38, drawerRight - 12, topY + 66 };
                    CasioTheme::DrawStudioButton(hdcMem, g_compactDrawerShareRc, L"Anotar / Compartir",
                        g_fontBold, CasioTheme::BTN_PRIMARY, false, (g_canvasHoverBtnId == ID_COMPACT_SHARE), 7.0f);

                    std::wstring liveLatex = BuildLiveHistoryLatexPreview(g_activeCalcIdx);
                    int scrollTopY = topY + 74;
                    if (!liveLatex.empty()) {
                        RECT rLatexCard{ drawerLeft + 12, topY + 74, drawerRight - 12, topY + 122 };
                        CasioTheme::FillRoundRectAA(hdcMem, rLatexCard, 7.0f,
                            CasioTheme::StudioSurfaceAlt(), CasioTheme::StudioBorder(), 1.0f);

                        RECT rLatexMath{ drawerLeft + 20, topY + 80, drawerRight - 20, topY + 116 };
                        DrawLatexRichText(hdcMem, liveLatex, rLatexMath,
                            CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 14, true);
                        scrollTopY = topY + 130;
                    }

                    g_compactDrawerScrollAreaRc = { drawerLeft + 10, scrollTopY, drawerRight - 8, rc.bottom - 10 };
                    const int areaW = g_compactDrawerScrollAreaRc.right - g_compactDrawerScrollAreaRc.left;
                    const int areaH = (std::max)(1, static_cast<int>(g_compactDrawerScrollAreaRc.bottom - g_compactDrawerScrollAreaRc.top));
                    const int rowH = 50;
                    const int rowStride = 54;
                    g_compactStepsContentH = stepCount * rowStride;
                    const int maxScroll = (std::max)(0, g_compactStepsContentH - areaH);
                    if (g_compactStepsScrollY > maxScroll) g_compactStepsScrollY = maxScroll;
                    if (g_compactStepsScrollY < 0) g_compactStepsScrollY = 0;

                    if (stepCount == 0) {
                        RECT rEmpty = g_compactDrawerScrollAreaRc;
                        InflateRect(&rEmpty, -10, -16);
                        SelectObject(hdcMem, g_fontSmall);
                        SetTextColor(hdcMem, CasioTheme::StudioTextMuted());
                        DrawTextW(hdcMem, L"Presiona teclas en la calculadora para ver y compartir la secuencia.",
                            -1, &rEmpty, DT_CENTER | DT_WORDBREAK);
                    } else {
                        const int savedDc = SaveDC(hdcMem);
                        IntersectClipRect(hdcMem, g_compactDrawerScrollAreaRc.left, g_compactDrawerScrollAreaRc.top,
                            g_compactDrawerScrollAreaRc.right, g_compactDrawerScrollAreaRc.bottom);
                        const bool hasScroll = (g_compactStepsContentH > areaH);
                        const int itemRight = g_compactDrawerScrollAreaRc.right - (hasScroll ? 9 : 2);
                        HDC memKeyDc = CreateCompatibleDC(hdcMem);
                        int curKeyNum = 0;
                        for (int sIdx = 0; sIdx < stepCount; ++sIdx) {
                            const auto& hk = g_keyHistory[stepIndices[sIdx]];
                            const bool isAppCap = hk.isAppCapture;
                            if (!isAppCap) curKeyNum++;

                            const int ry = g_compactDrawerScrollAreaRc.top + sIdx * rowStride - g_compactStepsScrollY;
                            if (ry + rowH < g_compactDrawerScrollAreaRc.top || ry > g_compactDrawerScrollAreaRc.bottom) continue;
                            const bool isLatest = (sIdx + 1 == stepCount);
                            RECT rRow{ g_compactDrawerScrollAreaRc.left + 2, ry, itemRight, ry + rowH };
                            g_compactDrawerStepHits.push_back({ rRow, static_cast<int>(stepIndices[sIdx]), isAppCap, hk.captureFilePath });

                            CasioTheme::FillRoundRectAA(hdcMem, rRow, 6.0f,
                                (isAppCap || isLatest) ? CasioTheme::StudioAccentSoft() : CasioTheme::StudioSurfaceAlt(),
                                (isAppCap || isLatest) ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder(),
                                isAppCap ? 1.2f : 1.0f);

                            RECT rNum{ rRow.left + 6, rRow.top + 14, rRow.left + 38, rRow.bottom - 14 };
                            const std::wstring nStr = isAppCap ? L"LCD" : (L"#" + std::to_wstring(curKeyNum));
                            CasioTheme::DrawModernPill(hdcMem, rNum,
                                (isAppCap || isLatest) ? CasioTheme::StudioAccent() : CasioTheme::StudioSurface(),
                                CasioTheme::StudioBorder(),
                                (isAppCap || isLatest) ? RGB(255, 255, 255) : CasioTheme::StudioTextSec(),
                                nStr, g_fontSmall, 4.5f);

                            if (isAppCap) {
                                RECT rLcdSlot{ rNum.right + 7, rRow.top + 4, (std::min)(rRow.right - 68, rNum.right + 101), rRow.bottom - 4 };
                                CasioTheme::FillRoundRectAA(hdcMem, rLcdSlot, 4.0f,
                                    RGB(20, 26, 36), CasioTheme::StudioAccent(), 1.0f);
                                const auto* lcdThumb = FindAppMenuThumb(hk.captureFilePath);
                                if (lcdThumb && lcdThumb->hBmp && lcdThumb->w > 0 && lcdThumb->h > 0 && memKeyDc) {
                                    const int availW = (std::max)(12, static_cast<int>(rLcdSlot.right - rLcdSlot.left) - 4);
                                    const int availH = (std::max)(10, static_cast<int>(rLcdSlot.bottom - rLcdSlot.top) - 4);
                                    const double sc = (std::min)(static_cast<double>(availW) / lcdThumb->w, static_cast<double>(availH) / lcdThumb->h);
                                    const int dw = (std::max)(10, static_cast<int>(lcdThumb->w * sc));
                                    const int dh = (std::max)(8, static_cast<int>(lcdThumb->h * sc));
                                    const int dx = rLcdSlot.left + (static_cast<int>(rLcdSlot.right - rLcdSlot.left) - dw) / 2;
                                    const int dy = rLcdSlot.top + (static_cast<int>(rLcdSlot.bottom - rLcdSlot.top) - dh) / 2;
                                    HGDIOBJ oldKb = SelectObject(memKeyDc, lcdThumb->hBmp);
                                    SetStretchBltMode(hdcMem, HALFTONE);
                                    SetBrushOrgEx(hdcMem, 0, 0, nullptr);
                                    StretchBlt(hdcMem, dx, dy, dw, dh, memKeyDc, 0, 0, lcdThumb->w, lcdThumb->h, SRCCOPY);
                                    SelectObject(memKeyDc, oldKb);
                                }
                                RECT rCapTop{ rLcdSlot.right + 6, rRow.top + 6, rRow.right - 6, rRow.top + 23 };
                                SelectObject(hdcMem, g_fontSmall);
                                SetTextColor(hdcMem, CasioTheme::StudioAccentText());
                                DrawTextW(hdcMem, L"App \u00B7 LCD", -1, &rCapTop, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                                RECT rCapBot{ rLcdSlot.right + 6, rRow.top + 23, rRow.right - 6, rRow.bottom - 5 };
                                std::wstring lbl = !hk.captureLabel.empty() ? hk.captureLabel : hk.keyName;
                                DrawLatexRichText(hdcMem, lbl, rCapBot,
                                    CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, true);
                            } else {
                                RECT rKeySlot{ rNum.right + 7, rRow.top + 4, rNum.right + 51, rRow.bottom - 4 };
                                CasioTheme::FillRoundRectAA(hdcMem, rKeySlot, 4.0f,
                                    RGB(22, 26, 34), CasioTheme::StudioBorder(), 1.0f);
                                DrawHistoryKeyThumbInRect(hdcMem, hk.calcIdx, hk.keyId, rKeySlot, memKeyDc);
                                std::wstring cap = KeyHistoryCaption(g_activeCalcIdx, hk.keyId);
                                if (cap.empty()) cap = hk.keyName;
                                RECT rCap{ rKeySlot.right + 8, rRow.top + 4, rRow.right - 6, rRow.bottom - 4 };
                                DrawLatexRichText(hdcMem, cap, rCap,
                                    CasioTheme::StudioText(), CasioTheme::StudioAccentText(), 12, true);
                            }
                        }
                        if (memKeyDc) DeleteDC(memKeyDc);
                        RestoreDC(hdcMem, savedDc);

                        if (g_compactStepsContentH > areaH && areaW > 24) {
                            const int trackX = g_compactDrawerScrollAreaRc.right - 5;
                            const int thumbH = (std::max)(24, MulDiv(areaH, areaH, g_compactStepsContentH));
                            const int thumbY = g_compactDrawerScrollAreaRc.top +
                                ((maxScroll > 0) ? MulDiv(g_compactStepsScrollY, areaH - thumbH, maxScroll) : 0);
                            RECT rThumb{ trackX, thumbY, trackX + 4, thumbY + thumbH };
                            CasioTheme::FillRoundRectAA(hdcMem, rThumb, 2.0f, CasioTheme::StudioBorderStrong(), CLR_INVALID, 0.0f);
                        }
                    }
                }
            }

            const bool tabHov = (g_canvasHoverBtnId == ID_COMPACT_PULL_TAB);
            HBRUSH brTab = CreateSolidBrush(tabHov ? CasioTheme::StudioTrack() : CasioTheme::StudioSurfaceAlt());
            FillRect(hdcMem, &g_compactPullTabRc, brTab);
            DeleteObject(brTab);

            HPEN penTab = CreatePen(PS_SOLID, 1, CasioTheme::StudioBorder());
            HGDIOBJ oldTabPen = SelectObject(hdcMem, penTab);
            MoveToEx(hdcMem, tabLeft, topY, nullptr);
            LineTo(hdcMem, tabLeft, rc.bottom);
            SelectObject(hdcMem, oldTabPen);
            DeleteObject(penTab);

            const int tabMidY = (topY + rc.bottom) / 2;
            const int gripH = (stepCount > 0) ? 74 : 52;
            RECT rGrip{ tabLeft + 2, tabMidY - gripH / 2, rc.right - 2, tabMidY + gripH / 2 };
            CasioTheme::FillRoundRectAA(hdcMem, rGrip, 6.0f,
                (tabHov || g_compactStepsOpen) ? CasioTheme::StudioAccentSoft() : CasioTheme::StudioSurface(),
                tabHov ? CasioTheme::StudioAccent() : CasioTheme::StudioBorder(), 1.0f);

            {
                Gdiplus::Graphics gTab(hdcMem);
                gTab.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                const COLORREF chevCol = (tabHov || g_compactStepsOpen) ? CasioTheme::StudioAccentText() : CasioTheme::StudioTextSec();
                Gdiplus::Pen pChev(Gdiplus::Color(255, GetRValue(chevCol), GetGValue(chevCol), GetBValue(chevCol)), 1.6f);
                pChev.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
                pChev.SetLineJoin(Gdiplus::LineJoinRound);
                const float gcx = (rGrip.left + rGrip.right) * 0.5f;
                const float gcy = (stepCount > 0) ? (rGrip.top + 18.0f) : ((rGrip.top + rGrip.bottom) * 0.5f);
                const float dir = g_compactStepsOpen ? 1.0f : -1.0f; // open: '>' collapses right; closed: '<' expands left
                Gdiplus::PointF pts[3] = {
                    { gcx - dir * 2.3f, gcy - 4.2f },
                    { gcx + dir * 2.3f, gcy },
                    { gcx - dir * 2.3f, gcy + 4.2f }
                };
                gTab.DrawLines(&pChev, pts, 3);
            }
            if (stepCount > 0) {
                RECT rBadgeCnt{ rGrip.left, rGrip.top + 32, rGrip.right, rGrip.bottom - 4 };
                SetBkMode(hdcMem, TRANSPARENT);
                SelectObject(hdcMem, g_fontSmall);
                SetTextColor(hdcMem, CasioTheme::StudioAccentText());
                std::wstring cntShort = (stepCount > 99) ? L"99+" : std::to_wstring(stepCount);
                DrawTextW(hdcMem, cntShort.c_str(), -1, &rBadgeCnt, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }

        {
            CasioTheme::UiScaleScope overlayScope(hdcMem, cScale);

            // Transient toast notification
            if (!g_lastToolbarToast.empty() && (GetTickCount64() - g_lastToolbarToastMs) < 3500) {
                if (g_compactSingleMode) {
                    // Rendered via layered g_hCompactToastWnd above heavyweight child HWNDs (SDL_app)
                } else if (compareMode) {
                    const int tw = (std::min)(static_cast<int>(rcLogical.right) - 32, 560);
                    const int tx = (static_cast<int>(rcLogical.right) - tw) / 2;
                    RECT rToast = { tx, STUDIO_TOOLBAR_H + 10, tx + tw, STUDIO_TOOLBAR_H + 38 };
                    CasioTheme::DrawSoftShadow(hdcMem, rToast, 8.0f, 2);
                    CasioTheme::DrawModernPill(hdcMem, rToast, CasioTheme::StudioSurface(),
                        CasioTheme::StudioAccent(), CasioTheme::StudioText(), g_lastToolbarToast, g_fontSmall, 7.0f);
                } else {
                    // In Solo view place toast in bottom-left away from centered calculator & quick buttons
                    RECT rToast{};
                    if (tl.rDockOuter.right > tl.rDockOuter.left && tl.rDockOuter.left >= 220) {
                        const int maxTw = tl.rDockOuter.left - 36;
                        const int tw = (std::min)(maxTw, 420);
                        const int tyTop = tl.rDockOuter.top + 3;
                        const int tyBot = tl.rDockOuter.bottom - 3;
                        rToast = { 24, tyTop, 24 + tw, tyBot };
                    } else {
                        const int tw = (std::min)(static_cast<int>(rcLogical.right) - 32, 460);
                        const int tx = (static_cast<int>(rcLogical.right) - tw) / 2;
                        const int botBase = (tl.rDockOuter.top > STUDIO_TOOLBAR_H + 60)
                            ? (tl.rDockOuter.top - 8) : (static_cast<int>(rcLogical.bottom) - 12);
                        rToast = { tx, botBase - 28, tx + tw, botBase };
                    }
                    CasioTheme::DrawSoftShadow(hdcMem, rToast, 8.0f, 2);
                    CasioTheme::DrawModernPill(hdcMem, rToast, CasioTheme::StudioSurface(),
                        CasioTheme::StudioAccent(), CasioTheme::StudioText(), g_lastToolbarToast, g_fontSmall, 7.0f);
                }
            }
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBmpMem);
        DeleteDC(hdcMem);

        if (!isPrintClient) EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!g_canvasTrackingMouse) {
            TRACKMOUSEEVENT tme{};
            tme.cbSize = sizeof(tme);
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            g_canvasTrackingMouse = true;
        }
        const int x = GET_X_LPARAM(lParam);
        const int y = GET_Y_LPARAM(lParam);
        bool changed = false;

        const int barH = GetCanvasToolbarH();
        int nextChromeHover = 0;
        if (!g_compactSingleMode) {
            RECT rcC{}; GetClientRect(hWnd, &rcC);
            const float cScale = GetCanvasUiScale(rcC.right);
            const RECT rcLog = CasioTheme::DeviceRectToLogical(rcC, cScale);
            const int lx = CasioTheme::DeviceToLogical(x, cScale);
            const int ly = CasioTheme::DeviceToLogical(y, cScale);
            const StudioToolbarLayout tl = ComputeStudioToolbarLayout(
                rcLog.right, rcLog.bottom, false, IsCompareViewMode(g_viewMode), g_userZoomFactor > 0.05);
            nextChromeHover = HitTestCanvasStudioChrome(tl, lx, ly);
            UpdateCanvasTooltipText(hWnd, nextChromeHover);
            if (nextChromeHover != g_canvasHoverBtnId) {
                g_canvasHoverBtnId = nextChromeHover;
                changed = true;
            }
        } else {
            POINT ptC{ x, y };
            if (g_compactPullTabRc.right > g_compactPullTabRc.left && PtInRect(&g_compactPullTabRc, ptC)) {
                nextChromeHover = ID_COMPACT_PULL_TAB;
            } else if (g_compactStepsOpen && g_compactDrawerShareRc.right > g_compactDrawerShareRc.left && PtInRect(&g_compactDrawerShareRc, ptC)) {
                nextChromeHover = ID_COMPACT_SHARE;
            } else if (g_compactStepsOpen && g_compactDrawerClearRc.right > g_compactDrawerClearRc.left && PtInRect(&g_compactDrawerClearRc, ptC)) {
                nextChromeHover = ID_COMPACT_CLEAR;
            }
            UpdateCanvasTooltipText(hWnd, nextChromeHover);
            if (nextChromeHover != g_canvasHoverBtnId) {
                g_canvasHoverBtnId = nextChromeHover;
                changed = true;
            }
        }

        int hoveredCalc = -1;
        int hoveredKey = -1;
        for (int i = 0; i < 3; ++i) {
            if (!g_nativeCalcs[i].isVisible) continue;
            int k = (y >= barH && nextChromeHover == 0) ? HitTestKey(i, x, y) : -1;
            if (k >= 0) {
                hoveredCalc = i;
                hoveredKey = k;
            }
            if (k != g_nativeCalcs[i].hoverKey) {
                g_nativeCalcs[i].hoverKey = k;
                changed = true;
            }
        }
        if (g_keyExplorerEnabled && !g_explorerPinned && hoveredCalc >= 0 && hoveredKey >= 0) {
            OpenExplorerForKey(hoveredCalc, hoveredKey, false, -1);
        }

        if (changed) InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSELEAVE: {
        g_canvasTrackingMouse = false;
        UpdateCanvasTooltipText(hWnd, 0);
        bool changed = (g_canvasHoverBtnId != 0);
        g_canvasHoverBtnId = 0;
        for (int i = 0; i < 3; ++i) {
            if (g_nativeCalcs[i].hoverKey != -1) {
                g_nativeCalcs[i].hoverKey = -1;
                changed = true;
            }
        }
        if (changed) InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT && g_canvasHoverBtnId != 0) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_RBUTTONUP: {
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        if (y < GetCanvasToolbarH()) return 0;
        for (int i = 0; i < 3; ++i) {
            if (!g_nativeCalcs[i].isVisible) continue;
            int k = HitTestKey(i, x, y);
            if (k >= 0) {
                g_activeCalcIdx = i;
                ShowCalculatorContextMenu(hWnd, x, y, i, k, -1, false, false, -1);
                return 0;
            }
            int spriteIdx = -1; bool indLit = false; bool isLcd = false;
            if (HitTestCanvasLcd(i, x, y, spriteIdx, indLit, isLcd)) {
                g_activeCalcIdx = i;
                ShowCalculatorContextMenu(hWnd, x, y, i, -1, spriteIdx, indLit, isLcd, -1);
                return 0;
            }
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        SetFocus(hWnd);
        int x = GET_X_LPARAM(lParam);
        int y = GET_Y_LPARAM(lParam);
        const int barH = GetCanvasToolbarH();

        if (g_compactSingleMode) {
            if (y < barH) {
                RECT rcBtnClient{};
                GetClientRect(hWnd, &rcBtnClient);
                POINT pt{ x, y };
                const StudioToolbarLayout tl = ComputeStudioToolbarLayout(rcBtnClient.right, rcBtnClient.bottom, true, false, false);
                if (PtInRect(&tl.rB[0], pt)) { SetCompactVerticalMode(!g_compactSingleMode, g_activeCalcIdx); InvalidateSidebarButtons(); return 0; }
                if (PtInRect(&tl.rB[1], pt)) { SendMessageW(g_hMainWnd, WM_COMMAND, ID_CMD_ALWAYS_ON_TOP, 0); return 0; }
                if (PtInRect(&tl.rB[2], pt)) { TogglePopupDisplayWindow(); InvalidateSidebarButtons(); return 0; }
                if (PtInRect(&tl.rB[3], pt)) { CopyAndSaveActiveLcd(g_activeCalcIdx, g_captureZoomScale); return 0; }
                if (PtInRect(&tl.rB[4], pt)) {
                    g_popupShowGrid = !g_popupShowGrid;
                    ShowToolbarToast(g_popupShowGrid ? L"Marco de captura LCD activado" : L"Marco de captura LCD desactivado");
                    if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                        InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
                    InvalidateRect(hWnd, nullptr, FALSE);
                    return 0;
                }
                for (int i = 0; i < 4; ++i) {
                    if (PtInRect(&tl.rPills[i], pt)) {
                        const UINT_PTR modeCmd[4] = { ID_BTN_MODE_WHITE, ID_BTN_MODE_GRAY, ID_BTN_MODE_GRAPH, ID_BTN_MODE_CW2 };
                        SendMessageW(g_hMainWnd, WM_COMMAND, modeCmd[i], 0);
                        if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                            InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
                        return 0;
                    }
                }
                if (PtInRect(&tl.rHelp, pt)) { OpenSmartHelpWindow(); return 0; }
                return 0;
            }
            POINT ptCompact{ x, y };
            if (g_compactPullTabRc.right > g_compactPullTabRc.left && PtInRect(&g_compactPullTabRc, ptCompact)) {
                ToggleCompactStepsDrawer(!g_compactStepsOpen);
                return 0;
            }
            if (g_compactStepsOpen) {
                if (g_compactDrawerShareRc.right > g_compactDrawerShareRc.left && PtInRect(&g_compactDrawerShareRc, ptCompact)) {
                    OpenSequenceExporterWindow(true);
                    return 0;
                }
                if (g_compactDrawerClearRc.right > g_compactDrawerClearRc.left && PtInRect(&g_compactDrawerClearRc, ptCompact)) {
                    SendMessageW(g_hMainWnd, WM_COMMAND, ID_BTN_CLEAR_HIST, 0);
                    return 0;
                }
                if (g_compactDrawerScrollAreaRc.right > g_compactDrawerScrollAreaRc.left && PtInRect(&g_compactDrawerScrollAreaRc, ptCompact)) {
                    for (const auto& hit : g_compactDrawerStepHits) {
                        if (PtInRect(&hit.rc, ptCompact)) {
                            if (hit.isAppCapture) {
                                if (!hit.captureFilePath.empty()) {
                                    ShellExecuteW(nullptr, L"open", hit.captureFilePath.c_str(), nullptr, nullptr, SW_SHOW);
                                }
                            } else if (hit.histIdx >= 0 && hit.histIdx < static_cast<int>(g_keyHistory.size()) && g_keyExplorerEnabled) {
                                const auto& hk = g_keyHistory[hit.histIdx];
                                OpenExplorerForKey(hk.calcIdx, hk.keyId, false, -1);
                            }
                            break;
                        }
                    }
                    return 0;
                }
            }
        } else {
            RECT rcBtnClient{};
            GetClientRect(hWnd, &rcBtnClient);
            const float cScale = GetCanvasUiScale(rcBtnClient.right);
            const RECT rcLog = CasioTheme::DeviceRectToLogical(rcBtnClient, cScale);
            const int lx = CasioTheme::DeviceToLogical(x, cScale);
            const int ly = CasioTheme::DeviceToLogical(y, cScale);
            const StudioToolbarLayout tl = ComputeStudioToolbarLayout(
                rcLog.right, rcLog.bottom, false, IsCompareViewMode(g_viewMode), g_userZoomFactor > 0.05);
            g_studioMenuBtnLogicalRc = tl.rMenu;
            const int cmdId = HitTestCanvasStudioChrome(tl, lx, ly);
            if (cmdId != 0) {
                SendMessageW(g_hMainWnd, WM_COMMAND, static_cast<WPARAM>(cmdId), 0);
                return 0;
            }
            if (y < barH) {
                return 0;
            }
        }

        for (int i = 0; i < 3; ++i) {
            if (!g_nativeCalcs[i].isVisible) continue;
            int spriteIdx = -1; bool indLit = false; bool isLcd = false;
            if (HitTestCanvasLcd(i, x, y, spriteIdx, indLit, isLcd) && spriteIdx >= 0) {
                g_activeCalcIdx = i;
                OpenExplorerForIndicator(i, spriteIdx, indLit, true);
                return 0;
            }
        }

        SetCapture(hWnd);
        bool hitKey = false;
        for (int i = 0; i < 3; ++i) {
            if (!g_nativeCalcs[i].isVisible) continue;
            int k = HitTestKey(i, x, y);
            if (k >= 0) {
                hitKey = true;
                g_activeCalcIdx = i;
                g_nativeCalcs[i].pressedKey = k;
                HandleKeyPress(i, k);
                if (g_keyExplorerEnabled && !g_explorerPinned) {
                    OpenExplorerForKey(i, k, false, -1);
                }
                InvalidateRect(hWnd, nullptr, FALSE);
                if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
                if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                    InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
                break;
            }
        }
        if (!hitKey && !g_compactSingleMode) {
            POINT ptStage{ x, y };
            bool matchedStage = false;
            for (int i = 0; i < 3; ++i) {
                if (g_nativeCalcs[i].isVisible && PtInRect(&g_nativeCalcs[i].stageRect, ptStage)) {
                    if (g_activeCalcIdx != i) {
                        g_activeCalcIdx = i;
                        InvalidateRect(hWnd, nullptr, FALSE);
                        if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
                        if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd))
                            InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
                    }
                    matchedStage = true;
                    break;
                }
            }
            if (!matchedStage && IsCw2Visible() && PtInRect(&g_cw2StageRect, ptStage)) {
                if (g_activeCalcIdx != 3) {
                    g_activeCalcIdx = 3;
                    InvalidateRect(hWnd, nullptr, FALSE);
                    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
                }
            }
        }
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED: {
        ReleaseCapture();
        for (int i = 0; i < 3; ++i) {
            if (g_nativeCalcs[i].pressedKey >= 0) {
                KeyVisual kv{};
                if (FillKeyVisual(i, g_nativeCalcs[i].pressedKey, kv)) {
                    if (g_nativeCalcs[i].emu) {
                        calcemu_key_up(g_nativeCalcs[i].emu);
                    }
                    if (i == 2 && g_nativeCalcs[i].cg50 &&
                        g_nativeCalcs[i].cg50->GetState() == Cg50Backend::State::Ready) {
                        g_nativeCalcs[i].cg50->KeyUp(kv.matrixCode);
                    }
                }
                g_nativeCalcs[i].pressedKey = -1;
                InvalidateRect(hWnd, nullptr, FALSE);
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (g_compactSingleMode && g_compactStepsOpen && g_compactStepsContentH > 0) {
            POINT ptW{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            ScreenToClient(hWnd, &ptW);
            if (PtInRect(&g_compactDrawerScrollAreaRc, ptW)) {
                const short delta = GET_WHEEL_DELTA_WPARAM(wParam);
                const int viewH = (std::max)(1, static_cast<int>(g_compactDrawerScrollAreaRc.bottom - g_compactDrawerScrollAreaRc.top));
                const int maxScroll = (std::max)(0, g_compactStepsContentH - viewH);
                g_compactStepsScrollY = (std::max)(0, (std::min)(maxScroll, g_compactStepsScrollY - (delta / WHEEL_DELTA) * 32));
                InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
        return SendMessageW(g_hMainWnd, msg, wParam, lParam);
    }
    case WM_KEYDOWN: {
        HandlePhysicalKeyDown(wParam);
        return 0;
    }
    case WM_KEYUP: {
        HandlePhysicalKeyUp(wParam);
        return 0;
    }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// ── Calculator Rendering & Drawing ──────────────────────────────────────────

void DrawCalculator(HDC hdc, int idx, const RECT& dstRect)
{
    auto& c = g_nativeCalcs[idx];
    if (!c.hSkinBmp) return;

    int dstW = dstRect.right - dstRect.left;
    int dstH = dstRect.bottom - dstRect.top;
    if (dstW <= 0 || dstH <= 0) return;

    // Draw high-resolution faceplate skin with transparent outer chassis corners
    HBITMAP hScaled = CasioTheme::EnsureScaledSkinBitmap(
        c.skinW, c.skinH, c.skinPargb, dstW, dstH,
        c.hScaledSkinBmp, c.cachedSkinW, c.cachedSkinH);
    HDC hdcSkin = CreateCompatibleDC(hdc);
    if (hScaled) {
        HGDIOBJ oldS = SelectObject(hdcSkin, hScaled);
        BLENDFUNCTION bf{};
        bf.BlendOp = AC_SRC_OVER;
        bf.SourceConstantAlpha = 255;
        bf.AlphaFormat = AC_SRC_ALPHA;
        AlphaBlend(hdc, dstRect.left, dstRect.top, dstW, dstH, hdcSkin, 0, 0, dstW, dstH, bf);
        SelectObject(hdcSkin, oldS);
    } else {
        HGDIOBJ oldS = SelectObject(hdcSkin, c.hSkinBmp);
        SetStretchBltMode(hdc, HALFTONE);
        StretchBlt(hdc, dstRect.left, dstRect.top, dstW, dstH, hdcSkin, 0, 0, c.skinW, c.skinH, SRCCOPY);
        SelectObject(hdcSkin, oldS);
    }
    DeleteDC(hdcSkin);

    // Map and Draw LCD Screen in exact skin coordinates
    double scaleX = (double)dstW / c.skinW;
    double scaleY = (double)dstH / c.skinH;

    RECT dstLcd;
    dstLcd.left   = dstRect.left + (int)(c.lcdSkinRect.left * scaleX);
    dstLcd.top    = dstRect.top  + (int)(c.lcdSkinRect.top * scaleY);
    dstLcd.right  = dstRect.left + (int)(c.lcdSkinRect.right * scaleX);
    dstLcd.bottom = dstRect.top  + (int)(c.lcdSkinRect.bottom * scaleY);

    if (idx == 2) {
        DrawCg50Screen(hdc, dstLcd);
    } else if (idx < 2) {
        DrawLcdDisplay(hdc, idx, dstLcd);
    }

    if (c.hoverKey >= 0 || c.pressedKey >= 0) {
        int keyId = (c.pressedKey >= 0) ? c.pressedKey : c.hoverKey;
        KeyVisual kv{};
        if (idx == 2 && cg50_selector::Ensure(c.hSkinBmp)) {
            HRGN region = cg50_selector::AtScale(keyId, dstRect, c.skinW, c.skinH);
            if (region) {
                HBRUSH brush = CreateSolidBrush(c.pressedKey >= 0 ? CasioTheme::AccentHover : RGB(240, 245, 255));
                FrameRgn(hdc, region, brush, 1, 1);
                DeleteObject(brush);
                DeleteObject(region);
            }
        } else if (FillKeyVisual(idx, keyId, kv)) {
            int x0 = kv.cx - kv.w / 2;
            int y0 = kv.cy - kv.h / 2;

            RECT rK;
            rK.left   = dstRect.left + (int)(x0 * scaleX);
            rK.top    = dstRect.top  + (int)(y0 * scaleY);
            rK.right  = dstRect.left + (int)((x0 + kv.w) * scaleX);
            rK.bottom = dstRect.top  + (int)((y0 + kv.h) * scaleY);

            CasioTheme::FillRoundRectAA(hdc, rK, 5.0f, CLR_INVALID,
                c.pressedKey >= 0 ? CasioTheme::AccentHover : RGB(240, 245, 255), 1.6f, 0, 230);
        }
    }

    // Presentation Highlight Mode: antialiased amber spotlight + bottom-right step badge #N
    if (g_teacherHighlightMode && c.pressedKey < 0) {
        int lastKeyId = -1;
        int stepCount = 0;
        for (const auto& hk : g_keyHistory) {
            if (hk.calcIdx == idx) {
                lastKeyId = hk.keyId;
                stepCount++;
            }
        }
        if (lastKeyId >= 0) {
            KeyVisual kvLast{};
            if (idx == 2 && cg50_selector::Ensure(c.hSkinBmp)) {
                HRGN rgn = cg50_selector::AtScale(lastKeyId, dstRect, c.skinW, c.skinH);
                if (rgn) {
                    HBRUSH brTeacher = CreateSolidBrush(CasioTheme::AmberWarm);
                    FrameRgn(hdc, rgn, brTeacher, 2, 2);
                    DeleteObject(brTeacher);
                    DeleteObject(rgn);
                }
            } else if (FillKeyVisual(idx, lastKeyId, kvLast)) {
                int x0 = kvLast.cx - kvLast.w / 2;
                int y0 = kvLast.cy - kvLast.h / 2;
                RECT rLast{
                    dstRect.left + (int)(x0 * scaleX),
                    dstRect.top  + (int)(y0 * scaleY),
                    dstRect.left + (int)((x0 + kvLast.w) * scaleX),
                    dstRect.top  + (int)((y0 + kvLast.h) * scaleY)
                };
                CasioTheme::FillRoundRectAA(hdc, rLast, 5.0f, CLR_INVALID, CasioTheme::AmberWarm, 2.2f);

                if (stepCount > 0 && (rLast.right - rLast.left) >= 24 && (rLast.bottom - rLast.top) >= 16) {
                    RECT rBadge{
                        (std::max)(rLast.left, rLast.right - 25),
                        (std::max)(rLast.top, rLast.bottom - 14),
                        rLast.right,
                        rLast.bottom
                    };
                    wchar_t badgeNum[16]{};
                    wsprintfW(badgeNum, L"#%d", stepCount);
                    CasioTheme::FillRoundRectAA(hdc, rBadge, 4.0f, CasioTheme::AmberWarm, RGB(254, 243, 199), 1.0f);
                    HFONT hTiny = CasioTheme::CreateUiFont(10, true);
                    HGDIOBJ oldBF = SelectObject(hdc, hTiny);
                    SetBkMode(hdc, TRANSPARENT);
                    SetTextColor(hdc, RGB(18, 22, 30));
                    DrawTextW(hdc, badgeNum, -1, &rBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                    SelectObject(hdc, oldBF);
                    DeleteObject(hTiny);
                }
            }
        }
    }

    DrawKeyLayersOverlay(hdc, idx, dstRect);
}

// ── Draw Genuine Dot-Matrix LCD for ClassWiz & Natural ES ───────────────────

void DrawLcdDisplay(HDC hdc, int idx, const RECT& dstLcd)
{
    auto& c = g_nativeCalcs[idx];
    int dstLcdW = dstLcd.right - dstLcd.left;
    int dstLcdH = dstLcd.bottom - dstLcd.top;
    if (dstLcdW <= 0 || dstLcdH <= 0) return;

    // Draw authentic LCD background (matched to official Casio olive bezel)
    HBRUSH hBrLcd = CreateSolidBrush(c.lcdBgColor);
    FillRect(hdc, &dstLcd, hBrLcd);
    DeleteObject(hBrLcd);

    // If emulated CPU is running, render the genuine dot-matrix pixels + official sprite indicators
    if (c.emu && c.lcdW > 0 && c.lcdH > 0) {
        const int screenMode = calcemu_screen_mode(c.emu);
        if (screenMode != 4 && screenMode != 5 && screenMode != 6) {
            return; // LCD powered off
        }

        RECT indRect{};
        RECT dotRect{};
        ComputeLcdIndAndDotRects(idx, dstLcd, indRect, dotRect);
        int dotH = dotRect.bottom - dotRect.top;

        if (screenMode == 4) {
            return; // Blank active LCD
        }

        const size_t rawBufSize = calcemu_screen_buffer_size(c.emu);
        std::vector<unsigned char> rawBuf(rawBufSize);
        const bool gotRaw = (rawBufSize > 0 && calcemu_screen_buffer(c.emu, rawBuf.data(), rawBuf.size()) == rawBufSize);

        if (screenMode == 6) {
            HBRUSH brAllOn = CreateSolidBrush(c.lcdFgColor);
            FillRect(hdc, &dotRect, brAllOn);
            DeleteObject(brAllOn);
            if (gotRaw) {
                std::vector<unsigned char> allOn(rawBufSize, 0xFF);
                CasioLcdIndicators::GlobalRenderer().Draw(
                    hdc, g_hInst, idx, allOn.data(), allOn.size(),
                    indRect, c.lcdBgColor, c.lcdFgColor, 128);
            }
            return;
        }

        if (gotRaw) {
            CasioLcdIndicators::GlobalRenderer().Draw(
                hdc, g_hInst, idx, rawBuf.data(), rawBuf.size(),
                indRect, c.lcdBgColor, c.lcdFgColor, 255);
        }

        const std::size_t pixelCount = static_cast<std::size_t>(c.lcdW) * c.lcdH;
        if (c.lcdPixels.size() != pixelCount) c.lcdPixels.resize(pixelCount);
        if (c.lcdRgb.size() != pixelCount) c.lcdRgb.resize(pixelCount);
        if (calcemu_screen_pixels(c.emu, c.lcdPixels.data(), c.lcdPixels.size()) == c.lcdPixels.size()) {
            uint32_t bgRgb = ((GetRValue(c.lcdBgColor)) << 16) | ((GetGValue(c.lcdBgColor)) << 8) | (GetBValue(c.lcdBgColor));
            uint32_t fgRgb = ((GetRValue(c.lcdFgColor)) << 16) | ((GetGValue(c.lcdFgColor)) << 8) | (GetBValue(c.lcdFgColor));

            for (int y = 0; y < c.lcdH; ++y) {
                int srcY = c.lcdH - 1 - y;
                for (int x = 0; x < c.lcdW; ++x) {
                    unsigned char p = c.lcdPixels[srcY * c.lcdW + x];
                    c.lcdRgb[y * c.lcdW + x] = (p != 0) ? fgRgb : bgRgb;
                }
            }

            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = c.lcdW;
            bmi.bmiHeader.biHeight = c.lcdH;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            SetStretchBltMode(hdc, COLORONCOLOR);
            StretchDIBits(hdc,
                dotRect.left, dotRect.top, dstLcdW, dotH,
                0, 0, c.lcdW, c.lcdH,
                c.lcdRgb.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
            return;
        }
    }

    // Fail closed: never replace a missing emulator with synthetic math.
    SetBkMode(hdc, TRANSPARENT);
    SelectObject(hdc, g_fontSmall);
    SetTextColor(hdc, RGB(120, 45, 45));
    RECT errorRect = { dstLcd.left + 6, dstLcd.top + 4, dstLcd.right - 6, dstLcd.bottom - 4 };
    DrawTextW(hdc, c.backendDiagnostic.c_str(), -1, &errorRect,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK);
}

// ── Honest fail-closed state for the not-yet-implemented fx-CG50 ─────────────

void DrawCg50Screen(HDC hdc, const RECT& dstLcd)
{
    auto& c = g_nativeCalcs[2];
    if (!c.cg50 || c.cg50->GetState() != Cg50Backend::State::Ready) {
        DrawCg50UnavailableScreen(hdc, dstLcd);
        return;
    }

    auto fb = c.cg50->Framebuffer();
    if (!fb.pixels || fb.width == 0 || fb.height == 0 ||
        fb.format != Cg50Backend::PixelFormat::Bgra32) {
        DrawCg50UnavailableScreen(hdc, dstLcd);
        return;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(fb.stridePixels);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(fb.height);
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    const int dstLcdW = dstLcd.right - dstLcd.left;
    const int dstLcdH = dstLcd.bottom - dstLcd.top;
    SetStretchBltMode(hdc, COLORONCOLOR);
    StretchDIBits(hdc,
        dstLcd.left, dstLcd.top, dstLcdW, dstLcdH,
        0, 0, static_cast<int>(fb.width), static_cast<int>(fb.height),
        fb.pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void DrawCg50UnavailableScreen(HDC hdc, const RECT& dstLcd)
{
    auto& c = g_nativeCalcs[2];
    HBRUSH background = CreateSolidBrush(RGB(24, 18, 20));
    FillRect(hdc, &dstLcd, background);
    DeleteObject(background);

    HBRUSH border = CreateSolidBrush(RGB(220, 72, 65));
    FrameRect(hdc, &dstLcd, border);
    DeleteObject(border);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255, 118, 105));
    SelectObject(hdc, g_fontBold);
    RECT titleRect = { dstLcd.left + 8, dstLcd.top + 8, dstLcd.right - 8, dstLcd.top + 34 };
    const bool isNative = c.cg50 && c.cg50->GetEngine() == Cg50Backend::Engine::RomNative;
    const wchar_t* title = isNative ? L"CG50 — NATIVO NO LISTO" : L"CG50 — GCAL NO LISTO";
    if (c.cg50 && c.cg50->GetState() == Cg50Backend::State::Faulted) {
        title = isNative ? L"CG50 — ERROR NATIVO" : L"CG50 — ERROR DE ARRANQUE";
    }
    DrawTextW(hdc, title, -1, &titleRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SetTextColor(hdc, RGB(235, 225, 225));
    SelectObject(hdc, g_fontSmall);
    RECT detailRect = { dstLcd.left + 12, dstLcd.top + 38, dstLcd.right - 12, dstLcd.bottom - 8 };
    DrawTextW(hdc, c.backendDiagnostic.c_str(), -1, &detailRect,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK);
}

static HBITMAP CaptureLcdSnapshot(int calcIdx, int zoomScale, int& outW, int& outH)
{
    if (calcIdx < 0 || calcIdx > 3) calcIdx = 0;
    zoomScale = (std::max)(1, (std::min)(zoomScale, 4));
    if (calcIdx == 3) {
        int rawW = 0, rawH = 0;
        HBITMAP hRaw = Fx991CwHost::CaptureBitmap(true, rawW, rawH);
        if (!hRaw || rawW <= 0 || rawH <= 0) {
            if (hRaw) DeleteObject(hRaw);
            outW = 0;
            outH = 0;
            return nullptr;
        }
        if (zoomScale == 1) {
            outW = rawW;
            outH = rawH;
            return hRaw;
        }
        outW = rawW * zoomScale;
        outH = rawH * zoomScale;
        HBITMAP hScaled = ScaleBitmapHalftone(hRaw, rawW, rawH, outW, outH, false);
        DeleteObject(hRaw);
        return hScaled;
    }
    if (calcIdx == 2) {
        outW = 384 * zoomScale;
        outH = 216 * zoomScale;
    } else if (calcIdx == 0) {
        outW = 192 * zoomScale;
        outH = 75 * zoomScale;
    } else {
        outW = 288 * zoomScale;
        outH = 111 * zoomScale;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = outW;
    bmi.bmiHeader.biHeight = -outH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC hdcScreen = GetDC(nullptr);
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp) {
        ReleaseDC(nullptr, hdcScreen);
        outW = 0;
        outH = 0;
        return nullptr;
    }

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HGDIOBJ hOld = SelectObject(hdcMem, hBmp);
    RECT rc{ 0, 0, outW, outH };
    if (calcIdx == 2) {
        DrawCg50Screen(hdcMem, rc);
    } else {
        DrawLcdDisplay(hdcMem, calcIdx, rc);
    }
    SelectObject(hdcMem, hOld);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    return hBmp;
}

// ── Hit Testing for Keypad with Authoritative Geometry ──────────────────────

int HitTestKey(int calcIdx, int mouseX, int mouseY)
{
    auto& c = g_nativeCalcs[calcIdx];
    const RECT& vr = c.viewportRect;
    if (!PtInRect(&vr, POINT{ mouseX, mouseY })) return -1;

    int dstW = vr.right - vr.left;
    int dstH = vr.bottom - vr.top;
    if (dstW <= 0 || dstH <= 0) return -1;

    double scaleX = (double)dstW / c.skinW;
    double scaleY = (double)dstH / c.skinH;

    int localX = (int)((mouseX - vr.left) / scaleX);
    int localY = (int)((mouseY - vr.top) / scaleY);

    if (calcIdx == 2 && cg50_selector::Ensure(c.hSkinBmp))
        return cg50_selector::KeyAt(localX, localY);

    int totalKeys = GetKeyCount(calcIdx);
    for (int i = 0; i < totalKeys; ++i) {
        KeyVisual kv{};
        if (!FillKeyVisual(calcIdx, i, kv)) continue;

        int x0 = kv.cx - kv.w / 2;
        int y0 = kv.cy - kv.h / 2;
        if (localX >= x0 && localX <= x0 + kv.w &&
            localY >= y0 && localY <= y0 + kv.h) {
            return i;
        }
    }
    return -1;
}

// ── Key press handler: only forwards physical matrix codes to a real backend ─

int HitTestCw2SkinKey(int skinX, int skinY)
{
    const int totalKeys = GetKeyCount(3);
    for (int i = 0; i < totalKeys; ++i) {
        KeyVisual kv{};
        if (!FillKeyVisual(3, i, kv)) continue;
        const int x0 = kv.cx - kv.w / 2;
        const int y0 = kv.cy - kv.h / 2;
        if (skinX >= x0 && skinX <= x0 + kv.w &&
            skinY >= y0 && skinY <= y0 + kv.h) {
            return i;
        }
    }
    return -1;
}

void RecordCw2KeyActivation(int keyId)
{
    KeyVisual kv{};
    if (!FillKeyVisual(3, keyId, kv)) return;
    g_activeCalcIdx = 3;
    AppendKeyHistory(3, keyId);
    RecordTimelineStep(3, keyId);
    if (g_keyExplorerEnabled && !g_explorerPinned) {
        OpenExplorerForKey(3, keyId, false, -1);
    }
    if (g_hHistoryWnd) InvalidateRect(g_hHistoryWnd, nullptr, FALSE);
    if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd)) {
        InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
    }
    SyncCw2HoverOverlay(true);
    if (wcscmp(kv.name, L"EXE") == 0) {
        g_pendingCaptureCalcIdx = 3;
        g_pendingCaptureCalcName = g_nativeCalcs[3].displayName;
        if (g_hMainWnd) {
            SetTimer(g_hMainWnd, ID_TIMER_CAPTURE_OP, 240, nullptr);
        }
    }
}

void HandleKeyPress(int calcIdx, int keyId)
{
    if (calcIdx < 0 || calcIdx > 3) return;
    auto& c = g_nativeCalcs[calcIdx];
    KeyVisual kv{};
    if (!FillKeyVisual(calcIdx, keyId, kv)) return;

    MaybeCapturePreEntryAppSnapshot(calcIdx, keyId);

    if (calcIdx == 3) {
        g_cw2SuppressParentNotifyForKey = keyId;
        g_cw2SuppressParentNotifyUntil = GetTickCount64() + 300;
        Fx991CwHost::ClickNormalized(kv.cx / 992.0, kv.cy / 2082.0);
        RecordCw2KeyActivation(keyId);
        return;
    }

    if (calcIdx == 2) {
        if (c.cg50 && c.cg50->GetState() == Cg50Backend::State::Ready) {
            c.cg50->KeyDown(kv.matrixCode);
            AppendKeyHistory(calcIdx, keyId);
            RecordTimelineStep(calcIdx, keyId);
        }
    } else if (c.emu) {
        if (kv.matrixCode == 0xFF) {
            calcemu_power_on(c.emu);
        } else {
            // A host click is one complete ROM transaction, independent of mouse hold.
            calcemu_tap_key(c.emu, static_cast<unsigned char>(kv.matrixCode), 400, 300);
        }
        AppendKeyHistory(calcIdx, keyId);
        RecordTimelineStep(calcIdx, keyId);
    }
    if (g_hPopupDisplayWnd && IsWindowVisible(g_hPopupDisplayWnd)) {
        InvalidateRect(g_hPopupDisplayWnd, nullptr, FALSE);
    }

    // Automatically trigger operation capture on EXE or =
    if (wcscmp(kv.name, L"EXE") == 0 || wcscmp(kv.name, L"=") == 0 || wcscmp(kv.name, L"IGUAL") == 0) {
        g_pendingCaptureCalcIdx = calcIdx;
        g_pendingCaptureCalcName = c.displayName;
        if (g_hMainWnd) {
            SetTimer(g_hMainWnd, ID_TIMER_CAPTURE_OP, 160, nullptr);
        }
    }
}

// ── Text-only record of matrix keys sent to each backend ────────────────────

void AppendKeyHistory(int calcIdx, int keyId)
{
    KeyVisual kv{};
    std::wstring name = FillKeyVisual(calcIdx, keyId, kv) ? kv.name : L"KEY";

    g_keyHistory.push_back({ calcIdx, keyId, name });
    g_historyFollowTail = true;
    InvalidateRect(g_hHistoryWnd, nullptr, TRUE);
    if (g_compactSingleMode && g_compactStepsOpen && g_hCanvasWnd) {
        InvalidateRect(g_hCanvasWnd, nullptr, FALSE);
    }
}

std::wstring BuildKeySequence(int calcIdx)
{
    std::wstring sequence;
    for (const auto& item : g_keyHistory) {
        if (item.calcIdx != calcIdx) continue;
        if (!sequence.empty()) sequence += L" ";
        sequence += item.keyName;
    }
    return sequence;
}

// History uses original key captures, including SHIFT / ALPHA legends.
#include "key_history_view.h"
#include "settings_and_exporter.h"
#include "smart_help_view.h"
#include "learning_studio.h"

inline void SyncSecondaryWindowsTheme()
{
    if (g_hwndSettings && IsWindow(g_hwndSettings)) {
        SetClassLongPtrW(g_hwndSettings, GCLP_HBRBACKGROUND,
            reinterpret_cast<LONG_PTR>(CasioTheme::GetStudioSurfaceBrush()));
        CasioTheme::ApplyDwmDarkTitleBar(g_hwndSettings, CasioTheme::g_studioDarkTheme);
        InvalidateRect(g_hwndSettings, nullptr, TRUE);
    }
    if (g_hwndExporter && IsWindow(g_hwndExporter)) {
        SetClassLongPtrW(g_hwndExporter, GCLP_HBRBACKGROUND,
            reinterpret_cast<LONG_PTR>(CasioTheme::GetStudioSurfaceBrush()));
        CasioTheme::ApplyDwmDarkTitleBar(g_hwndExporter, CasioTheme::g_studioDarkTheme);
        if (g_hwndExpNotesEdit && IsWindow(g_hwndExpNotesEdit)) {
            CasioTheme::EnableThemeScrollbar(g_hwndExpNotesEdit, CasioTheme::g_studioDarkTheme);
        }
        InvalidateRect(g_hwndExporter, nullptr, TRUE);
    }
    if (g_hSmartHelpWnd && IsWindow(g_hSmartHelpWnd)) {
        SetClassLongPtrW(g_hSmartHelpWnd, GCLP_HBRBACKGROUND,
            reinterpret_cast<LONG_PTR>(CasioTheme::GetStudioSurfaceBrush()));
        CasioTheme::ApplyDwmDarkTitleBar(g_hSmartHelpWnd, CasioTheme::g_studioDarkTheme);
        CasioTheme::EnableThemeScrollbar(g_hSmartHelpWnd, CasioTheme::g_studioDarkTheme);
        InvalidateRect(g_hSmartHelpWnd, nullptr, TRUE);
    }
    if (g_hStudioWnd && IsWindow(g_hStudioWnd)) {
        SetClassLongPtrW(g_hStudioWnd, GCLP_HBRBACKGROUND,
            reinterpret_cast<LONG_PTR>(CasioTheme::GetStudioSurfaceBrush()));
        CasioTheme::ApplyDwmDarkTitleBar(g_hStudioWnd, CasioTheme::g_studioDarkTheme);
        CasioTheme::EnableThemeScrollbar(g_hStudioWnd, CasioTheme::g_studioDarkTheme);
        InvalidateRect(g_hStudioWnd, nullptr, TRUE);
    }
    if (g_hPopupDisplayWnd && IsWindow(g_hPopupDisplayWnd)) {
        CasioTheme::ApplyDwmDarkTitleBar(g_hPopupDisplayWnd, CasioTheme::g_studioDarkTheme);
        InvalidateRect(g_hPopupDisplayWnd, nullptr, TRUE);
    }
    if (g_hCg50CompareWnd && IsWindow(g_hCg50CompareWnd)) {
        CasioTheme::ApplyDwmDarkTitleBar(g_hCg50CompareWnd, CasioTheme::g_studioDarkTheme);
        InvalidateRect(g_hCg50CompareWnd, nullptr, TRUE);
    }
    SyncPdfViewerTheme();
}

inline HWND ResolveSecondaryScreenshotTarget(int targetKind)
{
    if (targetKind == 1 && g_hSmartHelpWnd && IsWindow(g_hSmartHelpWnd)) return g_hSmartHelpWnd;
    if (targetKind == 2 && g_hStudioWnd && IsWindow(g_hStudioWnd)) return g_hStudioWnd;
    if (targetKind == 3 && g_hwndSettings && IsWindow(g_hwndSettings)) return g_hwndSettings;
    if (targetKind == 4 && g_hwndExporter && IsWindow(g_hwndExporter)) return g_hwndExporter;
    if (targetKind == 5 && PdfViewerUi::g_hPdfViewerWnd && IsWindow(PdfViewerUi::g_hPdfViewerWnd)) return PdfViewerUi::g_hPdfViewerWnd;
    if (targetKind == 6 && g_hPopupDisplayWnd && IsWindow(g_hPopupDisplayWnd)) return g_hPopupDisplayWnd;
    if (targetKind == 7 && g_hCg50CompareWnd && IsWindow(g_hCg50CompareWnd)) return g_hCg50CompareWnd;
    return nullptr;
}
