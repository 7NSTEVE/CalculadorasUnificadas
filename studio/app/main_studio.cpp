#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <objidl.h>
#include <gdiplus.h>
#include <roapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <psapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "../../ui_glass/translucent_tokens.hpp"
#include "../../ui_glass/translucent_math.hpp"
#include "../../ui_glass/vector_icons.hpp"
#include "../../ui_glass/gdi_translucent.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"
#include "canvas_manipulation.hpp"
#include "projector_window.hpp"
#include "stem_gui_dialogs.hpp"
#include "studio_actions.hpp"
#include "studio_cli_dispatch.hpp"
#include "../interop/notion_markdown.hpp"
#include "studio_command_ids.hpp"
#include "studio_feature_dialogs.hpp"
#include "studio_closure_dialogs.hpp"
#include "studio_tutor_panel.hpp"
#include "../pdf/table_interop.hpp"
#include "../core/accessibility_engine.hpp"
#include "../core/ai_provider_vault.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/study_assistant_tools.hpp"
#include "../core/camera.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "../core/image_processing.hpp"
#include "../core/inline_url_parser.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/lan_remote.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/smart_script_reflow.hpp"
#include "../core/structured_database.hpp"
#include "../core/studio_profile_store.hpp"
#include "../core/utf_conv.hpp"
#include "../interop/notebook_migration_engine.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../math/reactive_math_chemistry.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../pdf/interop_openxml.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_signer.hpp"
#include "../pdf/pdf_writer.hpp"
#include "../pdf/winrt_adapters.hpp"
#include "../media/native_video_engine.hpp"
#include "../media/webview2_html_host.hpp"
#include "../render/d2d_renderer.hpp"
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
#include "studio_gemini_notebook_dialog.hpp"
#endif
#ifdef DOC_STUDIO_HAS_ACADEMIC
#include "studio_tec_digital_dialog.hpp"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace {

using namespace DocStudio;

constexpr const wchar_t* kMainClassName = L"CalculadoraUnificadaDocStudioWnd";
constexpr const wchar_t* kViewportClassName = L"CalculadoraUnificadaDocStudioViewport";
constexpr const wchar_t* kSidebarClassName = L"CalculadoraUnificadaDocStudioSidebar";
constexpr const wchar_t* kInspectorClassName = L"CalculadoraUnificadaDocStudioInspector";
constexpr const wchar_t* kPromptClassName = L"CalculadoraUnificadaDocStudioPrompt";

constexpr int kTopChromeH = 54;
constexpr int kBottomStatusH = 30;
constexpr int kSidebarWidth = 228;
constexpr int kInspectorWidth = 296;

constexpr UINT WM_APP_ASYNC_PAGE_READY = WM_APP + 101;
constexpr UINT WM_APP_TAKE_SCREENSHOT = WM_APP + 102;
constexpr UINT WM_APP_CASIO_INBOX_READY = WM_APP + 103;
constexpr UINT WM_APP_HTML_SNAPSHOT_READY = WM_APP + 104;
constexpr UINT WM_APP_DRAFT_RECOVERY = WM_APP + 105;

struct HtmlSnapshotPayload {
    std::uint64_t objectId = 0;
    bool isCanvas = false;
    std::size_t pageIndex = 0;
    std::vector<std::uint8_t> pngBytes;
};

// Dynamic on-demand timer IDs (0 active timers when idle => 0.0% CPU / 0.0% GPU!)
constexpr UINT_PTR kTimerIdReplay   = 701;
constexpr UINT_PTR kTimerIdGif      = 703;
constexpr UINT_PTR kTimerIdPomodoro = 704;
constexpr UINT_PTR kTimerIdVideo    = 705;
constexpr UINT_PTR kTimerIdAutoSync = 706;
constexpr UINT_PTR kTimerIdDraftAutosave = 707; // solo mientras haya cambios sin guardar

// Borradores: copia de seguridad de lo que todavía no se guardó.
constexpr UINT kDraftAutosaveMs = 60u * 1000u;
constexpr int kDraftKeepDays = 30;

inline int DpiSc(int px96, UINT dpi) noexcept {
    return MulDiv(px96, static_cast<int>((dpi >= 72u && dpi <= 480u) ? dpi : 96u), 96);
}

enum class ToolKind {
    Hand = 0,
    Pen,
    Highlighter,
    Eraser,
    Lasso,
    Laser,
    ShapeRect,
    ShapeArrow,
    TextBox,
    Crop
};

using namespace DocStudio::CmdIds;

enum class ToolbarZone {
    Leading = 0,
    CenterTools,
    CenterInk,
    CenterHub,
    TrailingUtility,
    TrailingProminent,
    StatusZoom
};

struct ToolbarBtn {
    WORD id = 0;
    const wchar_t* label = nullptr;
    int width = 32;
    ToolbarZone zone = ToolbarZone::CenterTools;
    UiGlass::IconId icon = UiGlass::IconId::Document;
    bool hasIcon = true;
    bool hasLabel = false;
    bool accent = false;
    bool isSwatch = false;
    COLORREF swatchColor = 0;
    bool visible = true;
};

struct GlassCapsuleGroup {
    RECT rc{};
    bool prominent = false;
    bool visible = false;
};

struct InspectorHitBtn {
    RECT rc{};
    WORD cmdId = 0;
    int tabIdx = -1; // >= 0 switches inspector tab
};

struct StudioAppState {
    HWND hwndMain = nullptr;
    HWND hwndViewport = nullptr;
    HWND hwndSidebar = nullptr;
    HWND hwndInspector = nullptr;
    HWND hwndSearchEdit = nullptr;
    HFONT hUiFont = nullptr;
    HFONT hUiBoldFont = nullptr;
    HFONT hUiSmallFont = nullptr;
    HFONT hUiTitleFont = nullptr;
    HBRUSH hSearchBgBrush = nullptr;
    HDC hTopBufDc = nullptr;
    HBITMAP hTopBufBmp = nullptr;
    int topBufW = 0;
    int topBufH = 0;
    HDC hBotBufDc = nullptr;
    HBITMAP hBotBufBmp = nullptr;
    int botBufW = 0;
    int botBufH = 0;

    // material translúcido 2.0 & guías de interfaz appearance state
    UiGlass::GlassSettings glassSettings{};
    bool sidebarVisible = true;
    bool inspectorVisible = false;
    int inspectorTab = 0; // 0 = Tinta/Obj, 1 = Bóveda/BD, 2 = STEM, 3 = IA/Estudio, 4 = A11y/Rend, 5 = Capas/TEC
    bool searchVisible = false;
    bool appearancePopoverOpen = false;
    bool draggingOpacitySlider = false;
    RECT appearancePopoverRc{};
    RECT opacityTrackRc{};
    std::vector<GlassCapsuleGroup> toolbarCapsules;
    std::vector<InspectorHitBtn> inspectorHits;
    std::vector<InspectorHitBtn> sidebarHits;
    std::vector<InspectorHitBtn> statusHits;
    std::vector<RECT> sidebarNavRects;
    int sidebarContentScrollY = 0;
    int sidebarMaxScrollY = 0;
    int sidebarHoverIdx = -1;
    int inspectorScrollY = 0;
    int inspectorMaxScrollY = 0;
    int inspectorHoverIdx = -1;
    std::string academicStatusUtf8 = "Conecta tu cuenta para ver cursos y entregas";
    std::string scheduleSummaryUtf8 = "Listo para planificar tu semestre";
    int cliTecPreviewSection = -1;
    int cliTecPreviewSubView = 0;
    int cliTecPreviewInnerTab = 0;

    DocumentModel doc;
    CommandStack cmdStack;
    Camera2D camera;
    PageRenderCache pageCache{ 64ull * 1024ull * 1024ull }; // 64 MiB lean RAM budget (trimmed on minimize)
    D2DRenderer renderer;
    NativeVideoPlayer videoPlayer;
    WebView2HtmlHost htmlHost;
    Tutor::StudioTutorPanel tutorPanel;
    std::uint64_t activeHtmlObjectId = 0;
    ObjectTarget activeHtmlTarget{ true, 0 };
    HANDLE hCasioWatchStop = nullptr;
    std::thread casioWatchThread;
    bool timerGifActive = false;
    bool timerPomoActive = false;
    bool timerVideoActive = false;
    bool timerSyncActive = false;
    UINT currentGifTimerMs = 0;
    bool isMinimized = false;
    bool showcaseMediaCli = false;
    std::uint64_t asyncGen = 1;
    std::vector<AnnotationObject> objectClipboard;
    DocPoint lastContextWorldPt{ 80.0, 80.0 };
    bool hasLastContextWorldPt = false;
    std::string lastContextInlineUrlUtf8;
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
    GeminiNotebook::GeminiNotebookSyncStore gnStore;
    bool gnStoreLoaded = false;
#endif
#ifdef DOC_STUDIO_HAS_ACADEMIC
    std::unique_ptr<DocStudio::TecDigital::TecAcademicStore> tecStore;
#endif
    DocRect lastSnipRectWorld{};
    std::vector<DocPoint> lastSnipPolyWorld;
    bool hasLastSnipRegion = false;

    std::wstring currentDocPath;
    std::string activePdfSourcePathUtf8;
    std::string statusBannerUtf8 = "CuadernU listo";

    ToolKind activeTool = ToolKind::Pen;
    DocColor penColor = DocColor::RGBA(26, 115, 232, 255);
    double penWidth = 2.6;
    InkPenStyle penStyle = InkPenStyle::Fountain;
    EraserActionMode eraserActionMode = EraserActionMode::WholeStroke;
    bool snapToGrid = false;
    double penOpacity = 1.0;

    // Interactive gesture & direct canvas manipulation state
    bool panning = false;
    POINT lastPanPt{};
    POINT panStartPt{};
    bool panMoved = false;
    bool rbuttonMoved = false;
    POINT rbuttonDownPt{};
    bool drawing = false;
    bool draftIsCanvas = false;
    std::size_t draftPageIdx = 0;
    AnnotationObject draftObj{};
    DocPoint draftAnchorWorld{};
    bool hasDraftCrop = false;
    DocRect draftCropPageLocal{};

    CanvasManip::ActiveTransformDrag transformDrag{};
    bool pdfSelectingText = false;
    std::size_t pdfSelPage = 0;
    DocPoint pdfSelAnchorLocal{};
    DocRect pdfSelRectLocal{};
    std::string lastSelectedPdfTextUtf8;

    // Stylus hardware features (F-036 palm rejection, F-037 inverted eraser, F-038 barrel button)
    ULONGLONG lastPenInputTickMs = 0;
    bool barrelTempLasso = false;
    bool invertedTempEraser = false;
    ToolKind savedToolBeforeStylusMod = ToolKind::Pen;

    // Inline TextBox editing overlay
    HWND hwndInlineEdit = nullptr;
    std::uint64_t inlineEditObjId = 0;
    ObjectTarget inlineEditTarget{};

    // Multi-tab, split view, vault, versions, projector, exam lock
    struct OpenDocTab {
        std::wstring path;
        std::vector<std::uint8_t> serializedCudoc;
        Camera2D cam{};
        std::string title;
    };
    std::vector<OpenDocTab> tabs;
    std::size_t activeTabIdx = 0;
    bool splitViewActive = false;
    bool splitViewVertical = false;
    bool splitPanning = false;
    Camera2D splitCamera{};
    bool graphLocalMode = false;
    GraphFilterOptions graphFilter{};
    bool hasHoverPreview = false;
    DocPoint hoverPreviewScreenPt{};
    std::string hoverPreviewTitleUtf8;
    std::string hoverPreviewExcerptUtf8;
    bool studyTimerRunning = false;
    ULONGLONG lastStudyClockMs = 0;
    ULONGLONG lastPointerTapTickMs = 0;
    DocPoint lastPointerTapScreenPt{};
    std::uint32_t animationTick = 0;
    std::uint64_t autoSyncTickCounter = 0;
    std::string autoSyncPassphraseUtf8;
    FloatingInkToolbarState floatingInkToolbar{};
    NotebookVault vault;
    KnowledgeVault knowledgeVault;
    StructuredDatabase activeDatabase{ "db_estudio", "Proyectos y Tareas" };
    int activeDbViewCycle = 0;
    FlexcilStudyOverlay flexcilOverlay;
    ZoomWritingBoxState zoomWritingBox{};
    LibraryContentViewsManager contentViews;
    AnchoredCommentThreadManager commentThreads;
    CollaborationPermissionManager permissions;
    AutomationRuleEngine automations;
    StudioUiPreferences uiPrefs{};
    AiProviderVault aiVault;
    SpeechSynthesizerWin32 speechSynth;
    PomodoroTimerState pomodoro{};
    double readingRulerCursorY = 260.0;
    KeymapProfile keymapProfile = KeymapProfile::Default();
    LanRemote::LanCollabHttpServer lanServer;
    CommandPaletteRegistry commandPalette;
    ReusableElementLibrary snippetLibrary;
    GraphViewData activeGraphView;
    bool graphOverlayVisible = false;
    bool readingViewMode = false;
    bool dbPanelVisible = false;
    bool commentsPanelVisible = false;
    std::uint64_t selectedCommentThreadId = 0;
    bool audioTimelineVisible = false;
    bool drawingInZoomBox = false;
    std::string docYamlFrontmatterUtf8;
    std::vector<NoteProperty> docProperties;
    std::vector<std::string> docAliasesUtf8;
    std::vector<std::string> docTagsUtf8;
    std::string cachedReadingTextUtf8;
    std::vector<RECT> docTabRects;
    PageVersionHistory versionHistory;
    SecondaryProjectorWindow projectorWnd;
    bool examModeLocked = false;
    std::string examPinUtf8;
    bool aiOfflineOnly = true;

    std::uint64_t selectedObjectId = 0;
    bool selectedIsCanvas = false;
    std::size_t selectedPageIndex = 0;
    std::vector<std::uint64_t> lassoSelectedIds;
    std::vector<SearchMatch> searchMatches;
    std::size_t activeSearchIdx = 0;
    std::string lastSearchQueryUtf8;
    std::size_t activePageIndex = 0;
    int sidebarScrollPos = 0;

    // P1-P3 Ink Physics, Layers, Study & STEM interactive state
    bool snapShapes = true;
    double penStabilization = 0.65;
    bool presenterMode = false;
    bool replayActive = false;
    std::uint64_t replayCursorMs = 0;
    std::uint64_t replayMaxMs = 1;
    LaserPointerState laserPointer{};
    std::uint64_t strokeOriginEpochMs = 0;
    AudioTimelineSession audioSession;
    VaultFullTextIndex ftsIndex;
    StudyDeck activeDeck;
    std::size_t activeDeckCursor = 0;

    // Extended Goodnotes 6 / OneNote / Calcpad state
    bool minimapVisible = false;
    bool focusMode = false;
    bool hasPredictedTip = false;
    RulerState rulerState{};
    EraserFilterMode eraserFilterMode = EraserFilterMode::All;
    bool showcaseStemCli = false;

    std::vector<RECT> buttonRects;
    std::vector<ToolbarBtn> buttons;
    int hoveredButton = -1;

    // Borradores (copia de seguridad del documento con cambios sin guardar)
    std::wstring sessionDraftPath;
    bool timerDraftActive = false;

    // CLI automation state
    bool smokeTestMode = false;
    bool testDpiCycleCli = false;
    UINT currentDpi = 96;
    int dpiCycleTransitions = 0;
    std::wstring screenshotOutPath;
    int initialPage1Based = 1;
    std::string cliCalcCardSpec;
};

// ---------------- Small Win32 Helpers ----------------

std::wstring GetFileExtLower(const std::wstring& path) {
    const auto dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return {};
    std::wstring ext = path.substr(dot);
    for (wchar_t& c : ext) c = static_cast<wchar_t>(towlower(c));
    return ext;
}

bool ShowOpenDialog(HWND owner, const wchar_t* title, const COMDLG_FILTERSPEC* specs, UINT specCount, std::vector<std::wstring>& outPaths, bool multiSelect,
                    const wchar_t* initialFolder = nullptr) {
    outPaths.clear();
    Microsoft::WRL::ComPtr<IFileOpenDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || !dlg) return false;
    DWORD opts = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST;
    if (multiSelect) opts |= FOS_ALLOWMULTISELECT;
    dlg->SetOptions(opts);
    if (title) dlg->SetTitle(title);
    if (specs && specCount > 0) dlg->SetFileTypes(specCount, specs);
    if (initialFolder && *initialFolder) {
        Microsoft::WRL::ComPtr<IShellItem> folder;
        if (SUCCEEDED(SHCreateItemFromParsingName(initialFolder, nullptr, IID_PPV_ARGS(&folder))) && folder) {
            dlg->SetFolder(folder.Get());
        }
    }
    if (FAILED(dlg->Show(owner))) return false;

    Microsoft::WRL::ComPtr<IShellItemArray> arr;
    if (SUCCEEDED(dlg->GetResults(&arr)) && arr) {
        DWORD cnt = 0;
        arr->GetCount(&cnt);
        for (DWORD i = 0; i < cnt; ++i) {
            Microsoft::WRL::ComPtr<IShellItem> item;
            if (SUCCEEDED(arr->GetItemAt(i, &item)) && item) {
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
                    outPaths.emplace_back(p);
                    CoTaskMemFree(p);
                }
            }
        }
    }
    return !outPaths.empty();
}

bool ShowSaveDialog(HWND owner, const wchar_t* title, const COMDLG_FILTERSPEC* specs, UINT specCount, const wchar_t* defExt, const wchar_t* defName, std::wstring& outPath) {
    outPath.clear();
    Microsoft::WRL::ComPtr<IFileSaveDialog> dlg;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))) || !dlg) return false;
    dlg->SetOptions(FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);
    if (title) dlg->SetTitle(title);
    if (specs && specCount > 0) dlg->SetFileTypes(specCount, specs);
    if (defExt) dlg->SetDefaultExtension(defExt);
    if (defName) dlg->SetFileName(defName);
    if (FAILED(dlg->Show(owner))) return false;
    Microsoft::WRL::ComPtr<IShellItem> item;
    if (SUCCEEDED(dlg->GetResult(&item)) && item) {
        PWSTR p = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p)) && p) {
            outPath = p;
            CoTaskMemFree(p);
        }
    }
    return !outPath.empty();
}

// Dynamic modal text prompt (no .rc dependency, guías de interfaz + material translúcido 2.0 synchronized)
struct PromptState {
    std::wstring label1;
    std::wstring val1;
    std::wstring label2;
    std::wstring val2;
    bool twoFields = false;
    bool multiline = false;
    bool passwordMask = false;
    bool accepted = false;
    UiGlass::GlassSettings glassSettings{};
    HWND hwndEdit1 = nullptr;
    HWND hwndEdit2 = nullptr;
    HFONT hFont = nullptr;
    HFONT hBoldFont = nullptr;
    HBRUSH hBgBrush = nullptr;
    HBRUSH hCardBrush = nullptr;
};

LRESULT CALLBACK PromptWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PromptState* ps = reinterpret_cast<PromptState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        ps = reinterpret_cast<PromptState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ps));
        UiGlass::ApplyGoldenGateWindowFrame(hwnd, ps->glassSettings.IsDark(),
            ps->glassSettings.reduceTransparency || ps->glassSettings.IsHighContrast());
        ps->hFont = UiGlass::CreateHigUiFont(13, FW_NORMAL);
        ps->hBoldFont = UiGlass::CreateHigUiFont(13, FW_SEMIBOLD);
        const UiGlass::GlassPalette pal = UiGlass::GetPalette(ps->glassSettings);
        ps->hBgBrush = CreateSolidBrush(pal.sidebarBg.ToColorRef());
        ps->hCardBrush = CreateSolidBrush(pal.contentCard.ToColorRef());
        HFONT hf = ps->hFont ? ps->hFont : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));

        HWND l1 = CreateWindowW(L"STATIC", ps->label1.c_str(), WS_CHILD | WS_VISIBLE, 14, 12, 390, 20, hwnd, nullptr, nullptr, nullptr);
        SendMessageW(l1, WM_SETFONT, reinterpret_cast<WPARAM>(hf), TRUE);
        DWORD ed1Style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
        int ed1H = 24;
        if (ps->multiline) {
            ed1Style |= ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL;
            ed1H = 104;
        } else {
            ed1Style |= ES_AUTOHSCROLL;
            if (ps->passwordMask) ed1Style |= ES_PASSWORD;
        }
        ps->hwndEdit1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ps->val1.c_str(),
            ed1Style, 14, 34, 390, ed1H, hwnd, reinterpret_cast<HMENU>(101), nullptr, nullptr);
        SendMessageW(ps->hwndEdit1, WM_SETFONT, reinterpret_cast<WPARAM>(hf), TRUE);

        int btnY = ps->multiline ? 150 : 72;
        if (ps->twoFields) {
            HWND l2 = CreateWindowW(L"STATIC", ps->label2.c_str(), WS_CHILD | WS_VISIBLE, 14, 68, 390, 20, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(l2, WM_SETFONT, reinterpret_cast<WPARAM>(hf), TRUE);
            ps->hwndEdit2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", ps->val2.c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | WS_TABSTOP, 14, 90, 390, 24, hwnd, reinterpret_cast<HMENU>(102), nullptr, nullptr);
            SendMessageW(ps->hwndEdit2, WM_SETFONT, reinterpret_cast<WPARAM>(hf), TRUE);
            btnY = 128;
        }

        HWND bOk = CreateWindowW(L"BUTTON", L"Aceptar", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP,
            234, btnY, 82, 28, hwnd, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
        HWND bCancel = CreateWindowW(L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            322, btnY, 82, 28, hwnd, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
        SendMessageW(bOk, WM_SETFONT, reinterpret_cast<WPARAM>(ps->hBoldFont ? ps->hBoldFont : hf), TRUE);
        SendMessageW(bCancel, WM_SETFONT, reinterpret_cast<WPARAM>(hf), TRUE);
        UiGlass::ApplyControlDarkTheme(ps->hwndEdit1, ps->glassSettings.IsDark());
        if (ps->hwndEdit2) UiGlass::ApplyControlDarkTheme(ps->hwndEdit2, ps->glassSettings.IsDark());
        UiGlass::ApplyControlDarkTheme(bOk, ps->glassSettings.IsDark());
        UiGlass::ApplyControlDarkTheme(bCancel, ps->glassSettings.IsDark());
        SetFocus(ps->hwndEdit1);
        return 0;
    }
    case WM_ERASEBKGND: {
        if (ps && ps->hBgBrush) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            FillRect(reinterpret_cast<HDC>(wp), &rc, ps->hBgBrush);
            return 1;
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        if (ps && ps->hBgBrush) {
            const UiGlass::GlassPalette pal = UiGlass::GetPalette(ps->glassSettings);
            HDC hdcS = reinterpret_cast<HDC>(wp);
            SetTextColor(hdcS, pal.labelPrimary.ToColorRef());
            SetBkColor(hdcS, pal.sidebarBg.ToColorRef());
            return reinterpret_cast<LRESULT>(ps->hBgBrush);
        }
        break;
    }
    case WM_CTLCOLOREDIT: {
        if (ps && ps->hCardBrush) {
            const UiGlass::GlassPalette pal = UiGlass::GetPalette(ps->glassSettings);
            HDC hdcE = reinterpret_cast<HDC>(wp);
            SetTextColor(hdcE, pal.labelPrimary.ToColorRef());
            SetBkColor(hdcE, pal.contentCard.ToColorRef());
            return reinterpret_cast<LRESULT>(ps->hCardBrush);
        }
        break;
    }
    case WM_DPICHANGED: {
        const RECT* suggested = reinterpret_cast<const RECT*>(lp);
        if (suggested) {
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                suggested->right - suggested->left, suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        InvalidateRect(hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (!ps) break;
        if (LOWORD(wp) == IDOK) {
            wchar_t buf[2048]{};
            GetWindowTextW(ps->hwndEdit1, buf, 2048);
            ps->val1 = buf;
            if (ps->twoFields && ps->hwndEdit2) {
                GetWindowTextW(ps->hwndEdit2, buf, 2048);
                ps->val2 = buf;
            }
            ps->accepted = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (LOWORD(wp) == IDCANCEL) {
            ps->accepted = false;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    case WM_CLOSE:
        if (ps) ps->accepted = false;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (ps) {
            if (ps->hFont) { DeleteObject(ps->hFont); ps->hFont = nullptr; }
            if (ps->hBoldFont) { DeleteObject(ps->hBoldFont); ps->hBoldFont = nullptr; }
            if (ps->hBgBrush) { DeleteObject(ps->hBgBrush); ps->hBgBrush = nullptr; }
            if (ps->hCardBrush) { DeleteObject(ps->hCardBrush); ps->hCardBrush = nullptr; }
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool ShowTextPrompt(
    HWND owner,
    const wchar_t* title,
    const wchar_t* label1,
    std::wstring& val1,
    const wchar_t* label2 = nullptr,
    std::wstring* val2 = nullptr,
    bool multiline = false,
    bool passwordMask = false)
{
    PromptState ps;
    ps.label1 = label1 ? label1 : L"";
    ps.val1 = val1;
    ps.twoFields = (label2 != nullptr && val2 != nullptr);
    ps.multiline = multiline && !ps.twoFields;
    ps.passwordMask = passwordMask && !ps.multiline;
    if (ps.twoFields) {
        ps.label2 = label2;
        ps.val2 = *val2;
    }
    if (owner) {
        if (const auto* ownerSt = reinterpret_cast<const StudioAppState*>(GetWindowLongPtrW(owner, GWLP_USERDATA))) {
            ps.glassSettings = ownerSt->glassSettings;
        }
    }

    RECT ownerRc{};
    GetWindowRect(owner, &ownerRc);
    const int w = 434;
    const int h = ps.multiline ? 232 : (ps.twoFields ? 204 : 150);
    const int x = (ownerRc.left + ownerRc.right - w) / 2;
    const int y = (ownerRc.top + ownerRc.bottom - h) / 2;

    EnableWindow(owner, FALSE);
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, kPromptClassName, title,
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, w, h, owner, nullptr, GetModuleHandleW(nullptr), &ps);

    MSG msg{};
    while (IsWindow(dlg) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN &&
            (!ps.multiline || (GetKeyState(VK_CONTROL) & 0x8000) != 0)) {
            SendMessageW(dlg, WM_COMMAND, IDOK, 0);
            continue;
        }
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            SendMessageW(dlg, WM_COMMAND, IDCANCEL, 0);
            continue;
        }
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    if (ps.accepted) {
        val1 = ps.val1;
        if (ps.twoFields && val2) *val2 = ps.val2;
    }
    return ps.accepted;
}

bool CaptureWindowToPng(HWND hwnd, const std::wstring& outPath) {
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
    const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));

    HDC hdcWin = GetDC(hwnd);
    HDC hdcMem = CreateCompatibleDC(hdcWin);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcWin, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp || !bits) {
        if (hBmp) DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(hwnd, hdcWin);
        return false;
    }

    HGDIOBJ old = SelectObject(hdcMem, hBmp);
    if (!PrintWindow(hwnd, hdcMem, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) {
        BitBlt(hdcMem, 0, 0, w, h, hdcWin, 0, 0, SRCCOPY);
    }

    RasterImageBgra32 img;
    img.width = w;
    img.height = h;
    img.pixels.resize(static_cast<std::size_t>(w) * h * 4u);
    std::memcpy(img.pixels.data(), bits, img.pixels.size());
    // Ensure opaque alpha on GDI chrome pixels
    for (std::size_t i = 3; i < img.pixels.size(); i += 4) img.pixels[i] = 255;

    SelectObject(hdcMem, old);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(hwnd, hdcWin);

    std::vector<std::uint8_t> png;
    if (!WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, png)) return false;

    HANDLE hf = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    const BOOL ok = WriteFile(hf, png.data(), static_cast<DWORD>(png.size()), &wr, nullptr);
    CloseHandle(hf);
    return ok && wr == png.size();
}

void PaintAppearancePopoverOnHdc(StudioAppState* st, HDC hdcTarget, int clientW, int clientH);
void PopulateViewportOverlay(const StudioAppState* st, ViewportOverlayState& ov);

// Composite Direct2D viewport onto GDI capture if Direct2D Present isn't copied by PrintWindow on some GPUs
bool CaptureStudioCombinedToPng(StudioAppState* st, const std::wstring& outPath) {
    RECT mainRc{}, vpRc{};
    GetClientRect(st->hwndMain, &mainRc);
    GetWindowRect(st->hwndViewport, &vpRc);
    MapWindowPoints(HWND_DESKTOP, st->hwndMain, reinterpret_cast<LPPOINT>(&vpRc), 2);

    const int mw = (std::max)(1, static_cast<int>(mainRc.right - mainRc.left));
    const int mh = (std::max)(1, static_cast<int>(mainRc.bottom - mainRc.top));
    HDC hdcWin = GetDC(st->hwndMain);
    HDC hdcMem = CreateCompatibleDC(hdcWin);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = mw;
    bmi.bmiHeader.biHeight = -mh;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(hdcWin, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hBmp || !bits) {
        if (hBmp) DeleteObject(hBmp);
        DeleteDC(hdcMem);
        ReleaseDC(st->hwndMain, hdcWin);
        return false;
    }

    HGDIOBJ old = SelectObject(hdcMem, hBmp);
    PrintWindow(st->hwndMain, hdcMem, PW_CLIENTONLY | PW_RENDERFULLCONTENT);
    GdiFlush();

    RasterImageBgra32 composite;
    composite.width = mw;
    composite.height = mh;
    composite.pixels.resize(static_cast<std::size_t>(mw) * mh * 4u);
    std::memcpy(composite.pixels.data(), bits, composite.pixels.size());
    for (std::size_t i = 3; i < composite.pixels.size(); i += 4) composite.pixels[i] = 255;

    // Blit crisp synchronous offscreen Direct2D render straight into viewport region so screenshot is 100% deterministic
    const int vw = (std::max)(1, static_cast<int>(vpRc.right - vpRc.left));
    const int vh = (std::max)(1, static_cast<int>(vpRc.bottom - vpRc.top));
    RasterImageBgra32 vpImg;
    ViewportOverlayState ov{};
    PopulateViewportOverlay(st, ov);
    if (st->renderer.RenderHwndViewportToOffscreen(st->doc, st->camera, vw, vh, &st->pageCache, vpImg, &ov)) {
        for (int y = 0; y < vh; ++y) {
            const int dy = vpRc.top + y;
            if (dy < 0 || dy >= mh) continue;
            for (int x = 0; x < vw; ++x) {
                const int dx = vpRc.left + x;
                if (dx < 0 || dx >= mw) continue;
                const std::uint8_t* s = &vpImg.pixels[(static_cast<std::size_t>(y) * vw + x) * 4u];
                std::uint8_t* d = &composite.pixels[(static_cast<std::size_t>(dy) * mw + dx) * 4u];
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = 255;
            }
        }
    }

    // Composite floating material translúcido 2.0 Appearance Popover over live D2D content when open
    if (st->appearancePopoverOpen) {
        std::memcpy(bits, composite.pixels.data(), composite.pixels.size());
        PaintAppearancePopoverOnHdc(st, hdcMem, mw, mh);
        GdiFlush();
        std::memcpy(composite.pixels.data(), bits, composite.pixels.size());
        for (std::size_t i = 3; i < composite.pixels.size(); i += 4) composite.pixels[i] = 255;
    }

    SelectObject(hdcMem, old);
    DeleteObject(hBmp);
    DeleteDC(hdcMem);
    ReleaseDC(st->hwndMain, hdcWin);

    std::vector<std::uint8_t> png;
    if (!WinRtPdfRenderer::EncodeBgra32ToPngBytes(composite, png)) return false;
    HANDLE hf = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hf == INVALID_HANDLE_VALUE) return false;
    DWORD wr = 0;
    const BOOL ok = WriteFile(hf, png.data(), static_cast<DWORD>(png.size()), &wr, nullptr);
    CloseHandle(hf);
    return ok && wr == png.size();
}

// ---------------- App Helpers ----------------

void RefreshActivePageFromCamera(StudioAppState* st) {
    if (st->doc.Mode() != DocumentMode::PaginatedPdf || st->doc.Pages().empty()) return;
    RECT rc{};
    GetClientRect(st->hwndViewport, &rc);
    const double midY = st->camera.ScreenToWorld(0.0, (rc.bottom - rc.top) * 0.4).y;
    std::size_t f = 0, l = 0;
    if (st->doc.Layout().ComputeVisibleRange(midY, midY + 10.0, f, l, 0)) {
        st->activePageIndex = f;
    }
}

void CenterCameraOnPage(StudioAppState* st, std::size_t pageIdx) {
    if (st->doc.Pages().empty()) return;
    const std::size_t idx = (std::min)(pageIdx, st->doc.Pages().size() - 1);
    const auto* pl = st->doc.Layout().PlacementAt(idx);
    if (!pl) return;
    RECT rc{};
    GetClientRect(st->hwndViewport, &rc);
    const double vw = (std::max)(200.0, static_cast<double>(rc.right - rc.left));
    st->camera.zoom = std::clamp((vw - 64.0) / (std::max)(120.0, pl->widthWorld), 0.25, 2.2);
    st->camera.panX = -vw / (2.0 * st->camera.zoom);
    st->camera.panY = pl->topWorld - 16.0;
    st->activePageIndex = idx;
}

void CenterCameraOnWorldRect(StudioAppState* st, const DocRect& worldBox) {
    if (!st || worldBox.Empty()) return;
    RECT rc{};
    GetClientRect(st->hwndViewport, &rc);
    const double vw = (std::max)(200.0, static_cast<double>(rc.right - rc.left));
    const double vh = (std::max)(200.0, static_cast<double>(rc.bottom - rc.top));
    // Keep current zoom if comfortable, otherwise fit target box nicely
    const double fitZoom = std::clamp((vw * 0.72) / (std::max)(160.0, worldBox.w), 0.45, 1.45);
    if (st->camera.zoom < 0.35 || st->camera.zoom > 2.4) {
        st->camera.zoom = fitZoom;
    }
    const double cx = worldBox.x + worldBox.w * 0.5;
    const double cy = worldBox.y + worldBox.h * 0.5;
    st->camera.panX = cx - (vw / (2.0 * st->camera.zoom));
    st->camera.panY = cy - (vh / (2.0 * st->camera.zoom));
}

void UpdateWindowTitle(StudioAppState* st) {
    const std::wstring dirtyMark = st->doc.IsDirty() ? L"*" : L"";
    const std::wstring name = st->currentDocPath.empty()
        ? Utf8ToWide(st->doc.Title())
        : std::filesystem::path(st->currentDocPath).filename().wstring();
    std::wstring extra;
    if (st->tabs.size() > 1) {
        extra += L"  [" + std::to_wstring(st->activeTabIdx + 1) + L"/" + std::to_wstring(st->tabs.size()) + L"]";
    }
    if (st->splitViewActive) extra += L"  [Vista Dividida]";
    if (st->examModeLocked) extra += L"  [Modo Examen]";
    const std::wstring title = L"CuadernU \u2014 " + name + extra + dirtyMark;
    SetWindowTextW(st->hwndMain, title.c_str());
}

ObjectTarget CurrentTarget(const StudioAppState* st) noexcept {
    if (!st) return { true, 0 };
    const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
    const std::size_t pg = st->doc.Pages().empty() ? 0 : (std::min)(st->activePageIndex, st->doc.Pages().size() - 1);
    return { isCanvas, isCanvas ? 0u : pg };
}

void NextCardPlacement(const StudioAppState* st, double& outX, double& outY) noexcept {
    const ObjectTarget t = CurrentTarget(st);
    const std::size_t count = t.isCanvas
        ? st->doc.CanvasObjects().size()
        : (t.pageIndex < st->doc.Pages().size() ? st->doc.Pages()[t.pageIndex].objects.size() : 0);
    const std::size_t col = count / 4;
    const std::size_t row = count % 4;
    outX = 80.0 + static_cast<double>(col) * 390.0;
    outY = 80.0 + static_cast<double>(row) * 145.0;
}

std::uint64_t CurrentStrokeRelMs(StudioAppState* st) noexcept {
    const auto nowMs = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    if (st->strokeOriginEpochMs == 0) st->strokeOriginEpochMs = nowMs;
    return (nowMs >= st->strokeOriginEpochMs) ? (nowMs - st->strokeOriginEpochMs) : 0ull;
}

void InvalidateViews(StudioAppState* st);

void RebuildScaledUiFonts(StudioAppState* st) {
    if (!st) return;
    const double dpiScale = static_cast<double>(std::clamp(st->currentDpi, 72u, 384u)) / 96.0;
    const double sc = dpiScale * (std::clamp(st->uiPrefs.accessibility.uiFontScalePct, 100, 150) / 100.0);
    auto mkSz = [&](int basePx) -> int {
        return (std::max)(10, static_cast<int>(std::lround(basePx * sc)));
    };
    HFONT newUiFont      = UiGlass::CreateHigUiFont(mkSz(13), FW_NORMAL);
    HFONT newUiBoldFont  = UiGlass::CreateHigUiFont(mkSz(13), FW_SEMIBOLD);
    HFONT newUiSmallFont = UiGlass::CreateHigUiFont(mkSz(12), FW_NORMAL);
    HFONT newUiTitleFont = UiGlass::CreateHigUiFont(mkSz(15), FW_SEMIBOLD);
    if (st->hwndSearchEdit && newUiFont) {
        SendMessageW(st->hwndSearchEdit, WM_SETFONT, reinterpret_cast<WPARAM>(newUiFont), FALSE);
    }
    if (st->hwndInlineEdit && newUiFont) {
        SendMessageW(st->hwndInlineEdit, WM_SETFONT, reinterpret_cast<WPARAM>(newUiFont), FALSE);
    }
    if (st->hUiFont)      DeleteObject(st->hUiFont);
    if (st->hUiBoldFont)  DeleteObject(st->hUiBoldFont);
    if (st->hUiSmallFont) DeleteObject(st->hUiSmallFont);
    if (st->hUiTitleFont) DeleteObject(st->hUiTitleFont);
    st->hUiFont      = newUiFont;
    st->hUiBoldFont  = newUiBoldFont;
    st->hUiSmallFont = newUiSmallFont;
    st->hUiTitleFont = newUiTitleFont;
}

DocumentExtensionBundle BuildExtensionBundle(StudioAppState* st) {
    DocumentExtensionBundle b{};
    b.yamlFrontmatterUtf8 = st->docYamlFrontmatterUtf8;
    b.properties = st->docProperties;
    b.aliasesUtf8 = st->docAliasesUtf8;
    b.tagsUtf8 = st->docTagsUtf8;
    b.comments = &st->commentThreads;
    b.database = &st->activeDatabase;
    b.permissions = &st->permissions;
    b.automations = &st->automations;
    b.flexcil = &st->flexcilOverlay;
    b.studyDeck = &st->activeDeck;
    b.customCssUtf8 = st->uiPrefs.customCssSnippetUtf8;
    b.activeDbViewKind = st->activeDbViewCycle;
    return b;
}

void PersistStudioProfileNow(StudioAppState* st) {
    if (!st) return;
    const std::wstring rootDir = StudioProfileStore::DefaultProfileRootDir();
    st->audioSession.SetWhisperCliConfig(st->uiPrefs.whisperExePathUtf8, st->uiPrefs.whisperModelPathUtf8);
    StudioProfileStore::SaveAllProfileState(
        rootDir,
        st->contentViews,
        st->snippetLibrary,
        st->keymapProfile,
        st->knowledgeVault,
        st->uiPrefs);
    st->aiVault.SaveToDirectory(rootDir);
}

void LoadStudioProfileAtStartup(StudioAppState* st) {
    if (!st) return;
    if (st->commandPalette.All().empty()) st->commandPalette.RegisterDefaultCommands();
    const std::wstring rootDir = StudioProfileStore::DefaultProfileRootDir();
    StudioProfileStore::LoadAllProfileState(
        rootDir,
        st->contentViews,
        st->snippetLibrary,
        st->keymapProfile,
        st->knowledgeVault,
        st->uiPrefs);
    st->aiVault.LoadFromDirectory(rootDir);
    st->speechSynth.SetRate(st->uiPrefs.accessibility.ttsRate);
    st->speechSynth.SetVolume(st->uiPrefs.accessibility.ttsVolume);
    if (!st->uiPrefs.accessibility.ttsVoiceNameUtf8.empty()) {
        st->speechSynth.SelectVoiceByName(st->uiPrefs.accessibility.ttsVoiceNameUtf8);
    }
    st->audioSession.SetWhisperCliConfig(st->uiPrefs.whisperExePathUtf8, st->uiPrefs.whisperModelPathUtf8);
    st->renderer.SetHardwareAcceleration(st->uiPrefs.hardwareGpuAcceleration);
}

void RebuildReadingViewText(StudioAppState* st) {
    if (!st) return;
    std::ostringstream oss;
    if (!st->docProperties.empty()) {
        for (const auto& p : st->docProperties) {
            oss << p.keyUtf8 << ": " << p.valueUtf8 << "   ";
        }
        oss << "\n\n";
    }
    auto emitObj = [&](const AnnotationObject& o) {
        if (!o.textUtf8.empty()) {
            // Expand Obsidian-style ![[Note]] / ![[Note#Heading]] / ![[Note#^block]] transclusions (OB-EDIT-031)
            std::string expanded = o.textUtf8;
            std::size_t pos = 0;
            while ((pos = expanded.find("![[", pos)) != std::string::npos) {
                const auto end = expanded.find("]]", pos + 3);
                if (end == std::string::npos) break;
                const std::string expr = expanded.substr(pos + 3, end - (pos + 3));
                const std::string resolved = st->knowledgeVault.ResolveTransclusionMarkdown(expr);
                if (!resolved.empty()) {
                    expanded.replace(pos, (end + 2) - pos, resolved);
                    pos += resolved.size();
                } else {
                    pos = end + 2;
                }
            }
            oss << expanded << "\n\n";
        } else if (!o.formulaExprUtf8.empty()) {
            oss << o.formulaExprUtf8;
            if (!o.evaluatedResultUtf8.empty()) oss << "  =  " << o.evaluatedResultUtf8;
            oss << "\n\n";
        }
    };
    if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& o : st->doc.CanvasObjects()) emitObj(o);
    } else {
        for (std::size_t i = 0; i < st->doc.Pages().size(); ++i) {
            const auto& pg = st->doc.Pages()[i];
            if (st->doc.Pages().size() > 1) oss << "P\xC3\xA1gina " << (i + 1) << "\n";
            if (!pg.ocrTextUtf8.empty()) oss << pg.ocrTextUtf8 << "\n\n";
            for (const auto& o : pg.objects) emitObj(o);
        }
    }
    st->cachedReadingTextUtf8 = oss.str();
}

void PropagateSyncedBlocksInDocAndVault(StudioAppState* st, const std::string& editedTextUtf8) {
    if (!st) return;
    const auto openPos = editedTextUtf8.find(":::sync[");
    if (openPos == std::string::npos) return;
    const auto closeBr = editedTextUtf8.find(']', openPos + 8);
    if (closeBr == std::string::npos) return;
    const std::string syncId = editedTextUtf8.substr(openPos + 8, closeBr - (openPos + 8));
    if (syncId.empty()) return;
    const std::string tagPrefix = ":::sync[" + syncId + "]";
    std::vector<std::uint64_t> canvasIds;
    for (const auto& o : st->doc.CanvasObjects()) {
        if (o.kind == ObjectKind::TextBox && o.textUtf8.find(tagPrefix) != std::string::npos && o.textUtf8 != editedTextUtf8) {
            canvasIds.push_back(o.id);
        }
    }
    for (std::uint64_t cid : canvasIds) {
        if (AnnotationObject* mo = CanvasManip::FindObjectMut(st->doc, ObjectTarget{ true, 0 }, cid)) {
            mo->textUtf8 = editedTextUtf8;
        }
    }
    for (auto& pg : st->doc.MutablePages()) {
        for (auto& o : pg.objects) {
            if (o.kind == ObjectKind::TextBox && o.textUtf8.find(tagPrefix) != std::string::npos && o.textUtf8 != editedTextUtf8) {
                o.textUtf8 = editedTextUtf8;
            }
        }
    }
    st->doc.BumpRevision();
    std::string innerBody = editedTextUtf8.substr(closeBr + 1);
    const auto endMark = innerBody.rfind(":::");
    if (endMark != std::string::npos) innerBody.resize(endMark);
    while (!innerBody.empty() && (innerBody.front() == '\n' || innerBody.front() == '\r')) innerBody.erase(innerBody.begin());
    while (!innerBody.empty() && (innerBody.back() == '\n' || innerBody.back() == '\r')) innerBody.pop_back();
    st->knowledgeVault.UpdateSyncedBlock(syncId, innerBody);
}

bool IsDocumentLockedForEdit(StudioAppState* st) {
    if (!st || !st->permissions.IsNoteLocked()) return false;
    st->statusBannerUtf8 = "Nota bloqueada (solo lectura) \xE2\x80\x94 desbloquee con /bloquear-nota";
    InvalidateViews(st);
    return true;
}

void PopulateViewportOverlay(const StudioAppState* st, ViewportOverlayState& ov) {
    ov.draftObject = st->drawing ? &st->draftObj : nullptr;
    ov.draftPageIndex = st->draftPageIdx;
    ov.selectedObjectId = st->selectedObjectId;
    ov.lassoSelectedIds = st->lassoSelectedIds;
    ov.hasCropPreview = st->hasDraftCrop;
    ov.cropPageIndex = st->draftPageIdx;
    ov.cropPreviewPageLocal = st->draftCropPageLocal;
    ov.searchMatches = st->searchMatches.empty() ? nullptr : &st->searchMatches;
    ov.activeSearchMatchIndex = st->activeSearchIdx;
    ov.replayActive = st->replayActive;
    ov.replayCursorMs = st->replayCursorMs;
    ov.laserPointer = st->laserPointer;
    ov.presenterMode = st->presenterMode;
    ov.minimapVisible = st->minimapVisible;
    ov.rulerState = st->rulerState;
    ov.focusMode = st->focusMode;
    ov.darkTheme = st->glassSettings.IsDark();
    ov.highContrast = st->glassSettings.IsHighContrast();
    ov.hasPdfTextSel = st->pdfSelectingText && !st->pdfSelRectLocal.Empty();
    ov.pdfTextSelPage = st->pdfSelPage;
    ov.pdfTextSelRectLocal = st->pdfSelRectLocal;
    ov.showSelectionHandles = !st->presenterMode;
    ov.graphOverlayVisible = st->graphOverlayVisible;
    ov.graphOverlay = st->graphOverlayVisible ? &st->activeGraphView : nullptr;
    ov.flexcilPopupVisible = st->flexcilOverlay.IsPopupNoteVisible();
    ov.flexcilGestureMode = st->flexcilOverlay.IsGestureModeActive();
    ov.flexcilOverlay = &st->flexcilOverlay;
    ov.zoomWritingBox = st->zoomWritingBox.active ? &st->zoomWritingBox : nullptr;
    ov.zoomWritingPageIndex = st->activePageIndex;
    ov.dbPanelVisible = st->dbPanelVisible;
    ov.dbViewKind = st->activeDbViewCycle;
    ov.activeDatabase = st->dbPanelVisible ? &st->activeDatabase : nullptr;
    ov.commentThreads = st->commentThreads.AllThreads().empty() ? nullptr : &st->commentThreads;
    ov.commentsPanelVisible = st->commentsPanelVisible;
    ov.selectedCommentThreadId = st->selectedCommentThreadId;
    ov.readingViewMode = st->readingViewMode;
    ov.readingModeTitleUtf8 = st->doc.Title();
    ov.readingModeTextUtf8 = st->cachedReadingTextUtf8;
    ov.audioTimelineVisible = st->audioTimelineVisible;
    ov.audioRecording = st->audioSession.IsContinuousRecording();
    ov.audioPlaying = st->audioSession.IsPlaying();
    ov.audioCursorMs = st->replayActive ? st->replayCursorMs : st->audioSession.CursorMs();
    ov.audioDurationMs = st->audioSession.DurationMs();
    ov.audioSpeed = st->audioSession.PlaybackSpeed();
    if (ov.audioTimelineVisible || ov.audioRecording || ov.audioPlaying) {
        ov.audioEnvelopeBins = st->audioSession.ComputeEnvelopeBins(64);
        for (const auto& bm : st->audioSession.Bookmarks()) ov.audioBookmarkMs.push_back(bm.timestampMs);
    }
    ov.noteLocked = st->permissions.IsNoteLocked();
    ov.graphLocalMode = st->graphLocalMode;
    ov.graphFilterLabelUtf8 = !st->graphFilter.requiredTagUtf8.empty() ? st->graphFilter.requiredTagUtf8 : st->graphFilter.folderPrefixUtf8;
    ov.hasHoverPreview = st->hasHoverPreview;
    ov.hoverPreviewScreenPt = st->hoverPreviewScreenPt;
    ov.hoverPreviewTitleUtf8 = st->hoverPreviewTitleUtf8;
    ov.hoverPreviewExcerptUtf8 = st->hoverPreviewExcerptUtf8;
    ov.animationTick = st->animationTick;
    ov.readingRulerEnabled = st->uiPrefs.accessibility.readingRulerEnabled;
    ov.readingRulerCursorY = st->readingRulerCursorY;
    ov.readingRulerHeightPx = st->uiPrefs.accessibility.readingRulerHeightPx;
    ov.readingRulerMaskOpacity = st->uiPrefs.accessibility.readingRulerMaskOpacity;
    ov.dyslexiaFriendlyMode = st->uiPrefs.accessibility.dyslexiaFriendlyMode;
    ov.paperTintPreset = static_cast<int>(st->uiPrefs.accessibility.paperTint);
    ov.colorBlindPaletteMode = static_cast<int>(st->uiPrefs.accessibility.colorBlindMode);
    ov.enhancedFocusRing = st->uiPrefs.accessibility.enhancedFocusRing;
    ov.pomodoroVisible = st->pomodoro.running;
    ov.pomodoroBreakPhase = st->pomodoro.inBreakPhase;
    if (st->pomodoro.running) ov.pomodoroBadgeUtf8 = st->pomodoro.FormatHudBadgeUtf8();
    ov.animationClockMs = GetTickCount64();
    ov.activeVideoObjectId = st->videoPlayer.ActiveObjectId();
    ov.activeVideoPlaying = st->videoPlayer.IsPlaying();
    ov.activeVideoPosMs = st->videoPlayer.CurrentPositionMs();
    ov.activeVideoFrameBgra = (st->videoPlayer.IsPlaying() && st->videoPlayer.CurrentFrameBgra().Valid())
        ? &st->videoPlayer.CurrentFrameBgra() : nullptr;
    ov.activeHtmlObjectId = st->activeHtmlObjectId;
}

void ApplyCliCalcCardSpec(StudioAppState* st) {
    if (!st || st->cliCalcCardSpec.empty()) return;
    CalcBridge::CalcStepRecord rec{};
    const auto b1 = st->cliCalcCardSpec.find('|');
    if (b1 == std::string::npos) {
        rec.modelIdUtf8 = "fx-991CW";
        rec.expressionUtf8 = st->cliCalcCardSpec;
    } else {
        rec.modelIdUtf8 = b1 > 0 ? st->cliCalcCardSpec.substr(0, b1) : "fx-991CW";
        const auto b2 = st->cliCalcCardSpec.find('|', b1 + 1);
        if (b2 == std::string::npos) {
            rec.keySeqUtf8 = st->cliCalcCardSpec.substr(b1 + 1);
            rec.expressionUtf8 = rec.keySeqUtf8;
        } else {
            rec.keySeqUtf8 = st->cliCalcCardSpec.substr(b1 + 1, b2 - (b1 + 1));
            rec.expressionUtf8 = st->cliCalcCardSpec.substr(b2 + 1);
        }
    }
    double x = 80.0, y = 80.0;
    NextCardPlacement(st, x, y);
    const ObjectTarget tgt = CurrentTarget(st);
    const std::uint64_t newObjId = AppActions::AddCalcCardAction(st->doc, st->cmdStack, tgt, x, y, rec, st->statusBannerUtf8);
    if (newObjId != 0) {
        st->selectedObjectId = newObjId;
        st->selectedIsCanvas = tgt.isCanvas;
        st->selectedPageIndex = tgt.pageIndex;
    }
}

void RebuildToolbarButtons(StudioAppState* st) {
    st->buttons = {
        // Leading Capsule: Sidebar + File menu + Mode switcher (PDF | Lienzo)
        { CMD_TOGGLE_SIDEBAR,  L"",              30, ToolbarZone::Leading,           UiGlass::IconId::SidebarLeft,     true,  false, false, false, 0, true },
        { CMD_FILE_MENU,       L"Archivo",       80, ToolbarZone::Leading,           UiGlass::IconId::FolderOpen,      true,  true,  false, false, 0, true },
        { CMD_MODE_PDF,        L"PDF",           60, ToolbarZone::Leading,           UiGlass::IconId::PdfPages,        true,  true,  false, false, 0, true },
        { CMD_MODE_CANVAS,     L"Lienzo",        78, ToolbarZone::Leading,           UiGlass::IconId::InfiniteCanvas,  true,  true,  false, false, 0, true },

        // Center Capsule 1: Icon-only vector tool dock (strict HIG: never mix icons + verbose text)
        { CMD_TOOL_HAND,       L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::HandPan,         true,  false, false, false, 0, true },
        { CMD_TOOL_PEN,        L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::PenTool,         true,  false, false, false, 0, true },
        { CMD_TOOL_HL,         L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::HighlighterTool, true,  false, false, false, 0, true },
        { CMD_TOOL_ERASER,     L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::EraserTool,      true,  false, false, false, 0, true },
        { CMD_TOOL_LASSO,      L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::LassoTool,       true,  false, false, false, 0, true },
        { CMD_TOOL_RECT,       L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::ShapeRect,       true,  false, false, false, 0, true },
        { CMD_TOOL_ARROW,      L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::ShapeArrow,      true,  false, false, false, 0, true },
        { CMD_TOOL_TEXT,       L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::TextMath,        true,  false, false, false, 0, true },
        { CMD_TOOL_LASER,      L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::LaserTool,       true,  false, false, false, 0, true },
        { CMD_TOOL_CROP,       L"",              27, ToolbarZone::CenterTools,       UiGlass::IconId::CropTool,        true,  false, false, false, 0, true },

        // Center Capsule 2: Aqua 2.0 Ink Wells + Snap + Undo/Redo
        { CMD_COL_BLACK,       L"",              20, ToolbarZone::CenterInk,         UiGlass::IconId::PenTool,         false, false, false, true,  RGB(22, 27, 38),   true },
        { CMD_COL_BLUE,        L"",              20, ToolbarZone::CenterInk,         UiGlass::IconId::PenTool,         false, false, false, true,  RGB(0, 122, 255),  true },
        { CMD_COL_RED,         L"",              20, ToolbarZone::CenterInk,         UiGlass::IconId::PenTool,         false, false, false, true,  RGB(255, 59, 48),  true },
        { CMD_COL_GREEN,       L"",              20, ToolbarZone::CenterInk,         UiGlass::IconId::PenTool,         false, false, false, true,  RGB(40, 167, 69),  true },
        { CMD_COL_YELLOW,      L"",              20, ToolbarZone::CenterInk,         UiGlass::IconId::PenTool,         false, false, false, true,  RGB(245, 184, 10), true },
        { CMD_TOGGLE_SNAP,     L"",              25, ToolbarZone::CenterInk,         UiGlass::IconId::SnapMagnet,      true,  false, false, false, 0, true },
        { CMD_UNDO,            L"",              25, ToolbarZone::CenterInk,         UiGlass::IconId::Undo,            true,  false, false, false, 0, true },
        { CMD_REDO,            L"",              25, ToolbarZone::CenterInk,         UiGlass::IconId::Redo,            true,  false, false, false, 0, true },

        // Center Capsule 3: Engineering STEM, Study SM-2, Academic TEC Digital / HorariosTEC
        { CMD_STEM_MENU,       L"STEM",          68, ToolbarZone::CenterHub,         UiGlass::IconId::StemAtom,        true,  true,  false, false, 0, true },
        { CMD_STUDY_MENU,      L"Estudio",       76, ToolbarZone::CenterHub,         UiGlass::IconId::StudySparkles,   true,  true,  false, false, 0, true },
        { CMD_TOGGLE_ACADEMIC, L"Acad\u00E9mico",104, ToolbarZone::CenterHub,        UiGlass::IconId::AcademicCap,     true,  true,  false, false, 0, true },

        // Trailing Utility Capsule: Command Explorer (Ctrl+K) + Search + Appearance + Inspector + Overflow
        { CMD_COMMAND_PALETTE,   L"Explorar",    84, ToolbarZone::TrailingUtility,   UiGlass::IconId::Search,          true,  true,  false, false, 0, true },
        { CMD_TOGGLE_SEARCH,     L"",            28, ToolbarZone::TrailingUtility,   UiGlass::IconId::CompareGrid,     true,  false, false, false, 0, true },
        { CMD_TOGGLE_APPEARANCE, L"",            28, ToolbarZone::TrailingUtility,   UiGlass::IconId::SlidersGlass,    true,  false, false, false, 0, true },
        { CMD_TOGGLE_INSPECTOR,  L"",            28, ToolbarZone::TrailingUtility,   UiGlass::IconId::InspectorRight,  true,  false, false, false, 0, true },
        { CMD_MORE_OVERFLOW,     L"",            28, ToolbarZone::TrailingUtility,   UiGlass::IconId::MoreEllipsis,    true,  false, false, false, 0, true },

        // Trailing Prominent CTA Capsule (HIG: single prominent tinted material translúcido action)
        { CMD_EXPORT_PDF,      L"Exportar",      92, ToolbarZone::TrailingProminent, UiGlass::IconId::ShareExport,     true,  true,  true,  false, 0, true },

        // Bottom Status Zoom Controls
        { CMD_ZOOM_OUT,        L"-",             24, ToolbarZone::StatusZoom,        UiGlass::IconId::ZoomOut,         true,  false, false, false, 0, true },
        { CMD_ZOOM_RESET,      L"100%",          46, ToolbarZone::StatusZoom,        UiGlass::IconId::Search,          false, true,  false, false, 0, true },
        { CMD_ZOOM_IN,         L"+",             24, ToolbarZone::StatusZoom,        UiGlass::IconId::ZoomIn,          true,  false, false, false, 0, true }
    };
}

bool IsToolbarButtonChecked(const StudioAppState* st, WORD id) {
    switch (id) {
    case CMD_TOGGLE_SIDEBAR:    return st->sidebarVisible && !st->focusMode;
    case CMD_TOGGLE_INSPECTOR:  return st->inspectorVisible && !st->focusMode;
    case CMD_TOGGLE_ACADEMIC:   return st->inspectorVisible && st->inspectorTab == 5 && !st->focusMode;
    case CMD_TOGGLE_SEARCH:     return st->searchVisible;
    case CMD_TOGGLE_APPEARANCE: return st->appearancePopoverOpen;
    case CMD_MODE_PDF:          return st->doc.Mode() == DocumentMode::PaginatedPdf;
    case CMD_MODE_CANVAS:       return st->doc.Mode() == DocumentMode::InfiniteCanvas;
    case CMD_TOOL_HAND:         return st->activeTool == ToolKind::Hand;
    case CMD_TOOL_PEN:          return st->activeTool == ToolKind::Pen;
    case CMD_TOOL_HL:           return st->activeTool == ToolKind::Highlighter;
    case CMD_TOOL_ERASER:       return st->activeTool == ToolKind::Eraser;
    case CMD_TOOL_LASSO:        return st->activeTool == ToolKind::Lasso;
    case CMD_TOOL_LASER:        return st->activeTool == ToolKind::Laser;
    case CMD_TOOL_RECT:         return st->activeTool == ToolKind::ShapeRect;
    case CMD_TOOL_ARROW:        return st->activeTool == ToolKind::ShapeArrow;
    case CMD_TOOL_TEXT:         return st->activeTool == ToolKind::TextBox;
    case CMD_TOOL_CROP:         return st->activeTool == ToolKind::Crop;
    case CMD_TOGGLE_SNAP:       return st->snapShapes;
    case CMD_COL_BLACK:         return st->penColor.r <= 30 && st->penColor.g <= 35;
    case CMD_COL_BLUE:          return st->penColor.b >= 220 && st->penColor.r <= 40;
    case CMD_COL_RED:           return st->penColor.r >= 210 && st->penColor.g <= 70;
    case CMD_COL_GREEN:         return st->penColor.g >= 120 && st->penColor.r <= 60;
    case CMD_COL_YELLOW:        return st->penColor.r >= 230 && st->penColor.g >= 170;
    default: return false;
    }
}

void SyncActiveHtmlBounds(StudioAppState* st);
void SyncDynamicTimers(StudioAppState* st);

void LayoutMainChildren(StudioAppState* st) {
    if (!st || !st->hwndMain) return;
    RECT rc{};
    GetClientRect(st->hwndMain, &rc);
    const int w = (std::max)(360, static_cast<int>(rc.right - rc.left));
    const int h = (std::max)(260, static_cast<int>(rc.bottom - rc.top));
    const UINT dpi = (st->currentDpi >= 72u && st->currentDpi <= 480u) ? st->currentDpi : 96u;
    auto S = [&](int px) -> int { return DpiSc(px, dpi); };

    const int topH = S(kTopChromeH);
    const int botH = S(kBottomStatusH);
    const int sideWBase = S(kSidebarWidth);
    const int inspWBase = S(kInspectorWidth);

    // Responsive DPI-aware breakpoints: guarantee ZERO clipping/overlap from 600 DIPs up to 4K 200% DPI
    const bool showInkCapsule = (w >= S(1240));
    const bool showHubCapsule = (w >= S(980));
    const bool compactTools   = (w < S(860));
    const bool ultraCompact   = (w < S(760));
    const bool allowSidebars  = (w >= S(620)) && !st->focusMode;

    // Dynamically collapse "Explorar" text pill to icon-only on narrow windows so zones never collide
    for (auto& b : st->buttons) {
        if (b.id == CMD_COMMAND_PALETTE) {
            b.hasLabel = !ultraCompact;
            b.width = ultraCompact ? 28 : 84;
        }
    }

    const int sideW = (allowSidebars && st->sidebarVisible) ? sideWBase : 0;
    const int inspW = (allowSidebars && st->inspectorVisible) ? inspWBase : 0;

    st->buttonRects.assign(st->buttons.size(), RECT{ 0, 0, 0, 0 });
    st->toolbarCapsules.clear();

    const int capTop = S(9);
    const int capH   = S(36);
    const int padX   = S(5);
    const int gapBtn = S(2);
    const int itemY0 = capTop + S(3);
    const int itemY1 = capTop + capH - S(3);

    auto layoutZoneGroup = [&](ToolbarZone z, int startX, bool visible, bool prominent, int& outEndX, int maxRight = 100000) {
        if (!visible || startX + padX * 2 + S(24) > maxRight) {
            for (auto& b : st->buttons) {
                if (b.zone == z) b.visible = false;
            }
            outEndX = startX;
            return;
        }
        int x = startX + padX;
        int placed = 0;
        for (std::size_t i = 0; i < st->buttons.size(); ++i) {
            auto& b = st->buttons[i];
            if (b.zone != z) continue;
            if (compactTools && z == ToolbarZone::CenterTools &&
                (b.id == CMD_TOOL_LASSO || b.id == CMD_TOOL_ARROW || b.id == CMD_TOOL_LASER || b.id == CMD_TOOL_CROP)) {
                b.visible = false;
                continue;
            }
            const int bw = S(b.width);
            if (x + bw + padX > maxRight) {
                b.visible = false;
                continue;
            }
            b.visible = true;
            st->buttonRects[i] = { x, itemY0, x + bw, itemY1 };
            x += bw + gapBtn;
            placed++;
        }
        if (placed > 0) {
            x = x - gapBtn + padX;
            st->toolbarCapsules.push_back({ RECT{ startX, capTop, x, capTop + capH }, prominent, true });
            outEndX = x;
        } else {
            outEndX = startX;
        }
    };

    auto measureZoneWidth = [&](ToolbarZone z, bool visible) -> int {
        if (!visible) return 0;
        int sum = padX * 2;
        int cnt = 0;
        for (const auto& b : st->buttons) {
            if (b.zone != z) continue;
            if (compactTools && z == ToolbarZone::CenterTools &&
                (b.id == CMD_TOOL_LASSO || b.id == CMD_TOOL_ARROW || b.id == CMD_TOOL_LASER || b.id == CMD_TOOL_CROP)) {
                continue;
            }
            sum += S(b.width) + gapBtn;
            cnt++;
        }
        return cnt > 0 ? (sum - gapBtn) : 0;
    };

    // 1. Leading Zone
    int leadEnd = S(12);
    layoutZoneGroup(ToolbarZone::Leading, S(12), true, false, leadEnd);

    // 2. Trailing Zone
    const int promW = measureZoneWidth(ToolbarZone::TrailingProminent, true);
    const int utilW = measureZoneWidth(ToolbarZone::TrailingUtility, true);
    const int promX = w - S(12) - promW;
    const int utilX = promX - S(7) - utilW;
    int dummyEnd = 0;
    layoutZoneGroup(ToolbarZone::TrailingUtility, utilX, true, false, dummyEnd);
    layoutZoneGroup(ToolbarZone::TrailingProminent, promX, true, true, dummyEnd);

    // 3. Center Zone (strictly bounded within [availLeft, availRight] — mathematically zero overlap!)
    const int toolsW = measureZoneWidth(ToolbarZone::CenterTools, true);
    const int inkW   = measureZoneWidth(ToolbarZone::CenterInk, showInkCapsule);
    const int hubW   = measureZoneWidth(ToolbarZone::CenterHub, showHubCapsule);
    int centerTotal = toolsW + (inkW > 0 ? (S(6) + inkW) : 0) + (hubW > 0 ? (S(6) + hubW) : 0);
    const int availLeft = leadEnd + S(8);
    const int availRight = utilX - S(8);
    const int availCenterW = (std::max)(0, availRight - availLeft);

    bool effInk = showInkCapsule;
    bool effHub = showHubCapsule;
    if (centerTotal > availCenterW && effInk) {
        effInk = false;
        centerTotal = toolsW + (effHub ? (S(6) + hubW) : 0);
    }
    if (centerTotal > availCenterW && effHub) {
        effHub = false;
        centerTotal = toolsW;
    }

    int cx = availLeft + (std::max)(0, (availCenterW - centerTotal) / 2);
    int cEnd = cx;
    layoutZoneGroup(ToolbarZone::CenterTools, cx, true, false, cEnd, availRight);
    if (effInk) {
        cx = cEnd + S(6);
        layoutZoneGroup(ToolbarZone::CenterInk, cx, true, false, cEnd, availRight);
    } else {
        layoutZoneGroup(ToolbarZone::CenterInk, 0, false, false, dummyEnd);
    }
    if (effHub) {
        cx = cEnd + S(6);
        layoutZoneGroup(ToolbarZone::CenterHub, cx, true, false, cEnd, availRight);
    } else {
        layoutZoneGroup(ToolbarZone::CenterHub, 0, false, false, dummyEnd);
    }

    // 4. Bottom Status Zoom Stepper
    int zx = w - S(112);
    const int zy0 = h - botH + S(4);
    const int zy1 = h - S(4);
    for (std::size_t i = 0; i < st->buttons.size(); ++i) {
        if (st->buttons[i].zone != ToolbarZone::StatusZoom) continue;
        st->buttons[i].visible = true;
        const int bw = S(st->buttons[i].width);
        st->buttonRects[i] = { zx, zy0, zx + bw, zy1 };
        zx += bw + S(2);
    }

    // 5. Optional Document Tabs Strip + Search bar
    st->docTabRects.clear();
    const int tabStripH = (st->tabs.size() > 1) ? S(28) : 0;
    if (tabStripH > 0) {
        int tx = sideW + S(10);
        const int maxTabW = (std::max)(S(80), (std::min)(S(180), (w - sideW - inspW - S(24)) / static_cast<int>(st->tabs.size())));
        for (std::size_t ti = 0; ti < st->tabs.size(); ++ti) {
            st->docTabRects.push_back(RECT{ tx, topH + S(3), tx + maxTabW - S(4), topH + tabStripH - S(2) });
            tx += maxTabW;
        }
    }

    const int searchBandH = st->searchVisible ? S(38) : 0;
    if (st->hwndSearchEdit) {
        if (st->searchVisible) {
            const int sx = (std::max)(sideW + S(22), w - inspW - S(340));
            const int sw = (std::max)(S(140), (w - inspW - S(122)) - sx);
            MoveWindow(st->hwndSearchEdit, sx, topH + tabStripH + S(10), sw, S(18), TRUE);
            ShowWindow(st->hwndSearchEdit, SW_SHOW);
        } else {
            ShowWindow(st->hwndSearchEdit, SW_HIDE);
        }
    }

    const int contentTop = topH + tabStripH + searchBandH;
    const int contentH = (std::max)(100, h - contentTop - botH);

    // Optional Vertical Tool Rail
    const int railW = (st->uiPrefs.toolbarOrientation == 1 || st->uiPrefs.toolbarOrientation == 2) ? S(44) : 0;
    const int vpLeft = sideW + ((st->uiPrefs.toolbarOrientation == 1) ? railW : 0);
    const int vpRightEdge = w - inspW - ((st->uiPrefs.toolbarOrientation == 2) ? railW : 0);
    if (railW > 0) {
        const int railX = (st->uiPrefs.toolbarOrientation == 1) ? (sideW + S(6)) : (vpRightEdge + S(6));
        int ry = contentTop + S(12);
        for (std::size_t i = 0; i < st->buttons.size(); ++i) {
            if (st->buttons[i].zone != ToolbarZone::CenterTools) continue;
            st->buttons[i].visible = true;
            st->buttonRects[i] = { railX, ry, railX + S(32), ry + S(30) };
            ry += S(34);
        }
    }

    if (st->hwndSidebar) {
        if (sideW > 0) {
            ShowWindow(st->hwndSidebar, SW_SHOW);
            MoveWindow(st->hwndSidebar, 0, topH, sideW, (std::max)(100, h - topH - botH), TRUE);
        } else {
            ShowWindow(st->hwndSidebar, SW_HIDE);
        }
    }
    if (st->hwndInspector) {
        if (inspW > 0) {
            ShowWindow(st->hwndInspector, SW_SHOW);
            MoveWindow(st->hwndInspector, w - inspW, topH, inspW, (std::max)(100, h - topH - botH), TRUE);
        } else {
            ShowWindow(st->hwndInspector, SW_HIDE);
        }
    }
    if (st->hwndViewport) {
        const int vw = (std::max)(120, vpRightEdge - vpLeft);
        MoveWindow(st->hwndViewport, vpLeft, contentTop, vw, contentH, TRUE);
        st->renderer.Resize(vw, contentH);
    }

    const int popW = S(304);
    const int popH = S(248);
    const int vpRight = w - inspW;
    const int popX = (std::max)(sideW + S(12), vpRight - popW - S(16));
    const int popY = contentTop + S(8);
    st->appearancePopoverRc = { popX, popY, popX + popW, popY + popH };
    st->opacityTrackRc = { popX + S(20), popY + S(124), popX + popW - S(20), popY + S(144) };
    SyncActiveHtmlBounds(st);
}

void InvalidateRibbonOnly(StudioAppState* st) {
    if (!st || !st->hwndMain) return;
    RECT rc{};
    GetClientRect(st->hwndMain, &rc);
    const int topH = DpiSc(kTopChromeH, st->currentDpi) + (st->tabs.size() > 1 ? DpiSc(28, st->currentDpi) : 0) + (st->searchVisible ? DpiSc(40, st->currentDpi) : 0);
    RECT topRc{ 0, 0, rc.right, topH };
    InvalidateRect(st->hwndMain, &topRc, FALSE);
}

void InvalidateStatusOnly(StudioAppState* st) {
    if (!st || !st->hwndMain) return;
    RECT rc{};
    GetClientRect(st->hwndMain, &rc);
    const int botH = DpiSc(kBottomStatusH, st->currentDpi);
    RECT botRc{ 0, rc.bottom - botH, rc.right, rc.bottom };
    InvalidateRect(st->hwndMain, &botRc, FALSE);
}

void InvalidateViewportOnly(StudioAppState* st) {
    if (!st) return;
    if (st->hwndViewport) InvalidateRect(st->hwndViewport, nullptr, FALSE);
    SyncActiveHtmlBounds(st);
}

void InvalidateViews(StudioAppState* st) {
    if (!st) return;
    if (st->hwndViewport) InvalidateRect(st->hwndViewport, nullptr, FALSE);
    if (st->hwndSidebar && IsWindowVisible(st->hwndSidebar)) InvalidateRect(st->hwndSidebar, nullptr, FALSE);
    if (st->hwndInspector && IsWindowVisible(st->hwndInspector)) InvalidateRect(st->hwndInspector, nullptr, FALSE);
    if (st->projectorWnd.IsOpen()) {
        ViewportOverlayState pv{};
        PopulateViewportOverlay(st, pv);
        st->projectorWnd.SyncFromMain(&st->doc, &st->camera, &st->pageCache, pv);
    }
    InvalidateRibbonOnly(st);
    InvalidateStatusOnly(st);
    SyncActiveHtmlBounds(st);
}

bool IsAnimatedGifObject(const DocumentModel& doc, const AnnotationObject& obj) {
    if (obj.kind != ObjectKind::Image) return false;
    const std::vector<std::uint8_t>* bytes = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
    return bytes && bytes->size() >= 6 && std::memcmp(bytes->data(), "GIF8", 4) == 0;
}

void SyncActiveHtmlBounds(StudioAppState* st) {
    if (!st || st->activeHtmlObjectId == 0 || !st->hwndViewport) return;
    const ObjectTarget tgt = st->activeHtmlTarget;
    const AnnotationObject* obj = CanvasManip::FindObjectConst(st->doc, tgt, st->activeHtmlObjectId);
    if (!obj || obj->kind != ObjectKind::HtmlEmbed) {
        st->htmlHost.DeactivateBlock(st->activeHtmlObjectId);
        st->activeHtmlObjectId = 0;
        return;
    }
    const bool modeMatches = (tgt.isCanvas == (st->doc.Mode() == DocumentMode::InfiniteCanvas));
    if (!modeMatches) {
        RECT emptyRc{ 0, 0, 0, 0 };
        st->htmlHost.SyncBlockBounds(st->activeHtmlObjectId, emptyRc, false);
        return;
    }
    RECT vpRc{};
    GetClientRect(st->hwndViewport, &vpRc);
    DocRect wr = obj->bounds;
    if (!tgt.isCanvas) {
        if (const auto* pl = st->doc.Layout().PlacementAt(tgt.pageIndex)) {
            wr.x += pl->leftWorld;
            wr.y += pl->topWorld;
        }
    }
    const DocPoint p0 = st->camera.WorldToScreen(wr.x, wr.y);
    const DocPoint p1 = st->camera.WorldToScreen(wr.Right(), wr.Bottom());
    const int hdrH = (std::min)(26, static_cast<int>((p1.y - p0.y) * 0.22));
    RECT bnd{
        static_cast<LONG>(std::lround(p0.x)) + 2,
        static_cast<LONG>(std::lround(p0.y)) + hdrH + 1,
        static_cast<LONG>(std::lround(p1.x)) - 2,
        static_cast<LONG>(std::lround(p1.y)) - 2
    };
    const bool visible = !st->isMinimized &&
        bnd.right > 0 && bnd.bottom > 0 && bnd.left < vpRc.right && bnd.top < vpRc.bottom &&
        (bnd.right - bnd.left) >= 32 && (bnd.bottom - bnd.top) >= 28;
    st->htmlHost.SyncBlockBounds(st->activeHtmlObjectId, bnd, visible);
}

// Corridas automáticas (pruebas, capturas, vistas previas por línea de comandos): nunca
// preguntan ni escriben borradores, para que nada se quede esperando a una persona.
bool IsAutomationRun(const StudioAppState* st) noexcept {
    return st && (st->smokeTestMode || st->testDpiCycleCli || !st->screenshotOutPath.empty() ||
                  st->cliTecPreviewSection >= 0);
}

void SyncDynamicTimers(StudioAppState* st) {
    if (!st || !st->hwndMain) return;
    // 1. GIF timer: active ONLY when a visible non-paused animated GIF exists & not minimized
    const bool needGif = !st->isMinimized && st->renderer.LastFrameHadVisibleAnimatedGif();
    const UINT wantGifMs = st->renderer.LastFrameMinGifDelayMs();
    if (needGif) {
        if (!st->timerGifActive || st->currentGifTimerMs != wantGifMs) {
            SetTimer(st->hwndMain, kTimerIdGif, wantGifMs, nullptr);
            st->timerGifActive = true;
            st->currentGifTimerMs = wantGifMs;
        }
    } else if (st->timerGifActive) {
        KillTimer(st->hwndMain, kTimerIdGif);
        st->timerGifActive = false;
        st->currentGifTimerMs = 0;
    }

    // 2. Pomodoro / Study timer: 1 Hz ONLY when running & not minimized (0% minimized CPU)
    const bool needPomo = !st->isMinimized && (st->pomodoro.running || st->studyTimerRunning);
    if (needPomo && !st->timerPomoActive) {
        SetTimer(st->hwndMain, kTimerIdPomodoro, 1000, nullptr);
        st->timerPomoActive = true;
    } else if (!needPomo && st->timerPomoActive) {
        KillTimer(st->hwndMain, kTimerIdPomodoro);
        st->timerPomoActive = false;
    }

    // 3. Native Video timer: 30 fps ONLY while actively playing & not minimized
    const bool needVideo = !st->isMinimized && st->videoPlayer.IsPlaying();
    if (needVideo && !st->timerVideoActive) {
        SetTimer(st->hwndMain, kTimerIdVideo, 33, nullptr);
        st->timerVideoActive = true;
    } else if (!needVideo && st->timerVideoActive) {
        KillTimer(st->hwndMain, kTimerIdVideo);
        st->timerVideoActive = false;
    }

    // 4. Auto-sync timer: 30 s ONLY when explicitly enabled & not minimized
    const bool waitingForSyncPhrase = st->uiPrefs.autoSyncEncrypted && st->autoSyncPassphraseUtf8.empty();
    const bool needSync = !st->isMinimized && st->uiPrefs.autoSyncEnabled && !st->uiPrefs.autoSyncFolderUtf8.empty() &&
        !waitingForSyncPhrase;
    if (needSync && !st->timerSyncActive) {
        SetTimer(st->hwndMain, kTimerIdAutoSync, 30000, nullptr);
        st->timerSyncActive = true;
    } else if (!needSync && st->timerSyncActive) {
        KillTimer(st->hwndMain, kTimerIdAutoSync);
        st->timerSyncActive = false;
    }

    // 5. Borrador: cada 60 s SOLO mientras haya cambios sin guardar (también minimizado:
    //    es justo cuando más fácil se olvida). En pruebas automáticas no corre.
    const bool needDraft = !IsAutomationRun(st) && st->doc.IsDirty();
    if (needDraft && !st->timerDraftActive) {
        SetTimer(st->hwndMain, kTimerIdDraftAutosave, kDraftAutosaveMs, nullptr);
        st->timerDraftActive = true;
    } else if (!needDraft && st->timerDraftActive) {
        KillTimer(st->hwndMain, kTimerIdDraftAutosave);
        st->timerDraftActive = false;
    }
}

void TrimApplicationMemoryNow(StudioAppState* st, bool aggressive) {
    if (!st) return;
    st->renderer.TrimCaches(aggressive);
    if (aggressive) {
        st->pageCache.SetMaxBytes(16ull * 1024ull * 1024ull);
        st->pageCache.SetMaxBytes(64ull * 1024ull * 1024ull);
        WinRtPdfRenderer::ClearOpenDocumentCache();
        SetProcessWorkingSetSizeEx(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1), 0);
        EmptyWorkingSet(GetCurrentProcess());
    }
}

std::wstring GetCuadernUDataDir();

void StartCasioInboxWatcher(StudioAppState* st) {
    if (!st || !st->hwndMain || st->hCasioWatchStop) return;
    std::filesystem::path inbox = std::filesystem::path(GetCuadernUDataDir()) / L"ipc_inbox";
    std::error_code ec;
    std::filesystem::create_directories(inbox, ec);
    st->hCasioWatchStop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!st->hCasioWatchStop) return;
    HWND targetHwnd = st->hwndMain;
    HANDLE stopEvt = st->hCasioWatchStop;
    const std::wstring watchDir = inbox.wstring();
    st->casioWatchThread = std::thread([targetHwnd, stopEvt, watchDir]() {
        HANDLE hChange = FindFirstChangeNotificationW(
            watchDir.c_str(), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE);
        if (hChange == INVALID_HANDLE_VALUE) return;
        HANDLE waits[2] = { stopEvt, hChange };
        for (;;) {
            const DWORD res = WaitForMultipleObjects(2, waits, FALSE, INFINITE);
            if (res == WAIT_OBJECT_0) break; // clean shutdown
            if (res == WAIT_OBJECT_0 + 1) {
                if (IsWindow(targetHwnd)) PostMessageW(targetHwnd, WM_APP_CASIO_INBOX_READY, 0, 0);
                if (!FindNextChangeNotification(hChange)) break;
            } else {
                break;
            }
        }
        FindCloseChangeNotification(hChange);
    });
}

void StopCasioInboxWatcher(StudioAppState* st) {
    if (!st) return;
    if (st->hCasioWatchStop) {
        SetEvent(st->hCasioWatchStop);
    }
    if (st->casioWatchThread.joinable()) {
        st->casioWatchThread.join();
    }
    if (st->hCasioWatchStop) {
        CloseHandle(st->hCasioWatchStop);
        st->hCasioWatchStop = nullptr;
    }
}

std::wstring ResolveVideoPathOnDisk(const StudioAppState* st, const AnnotationObject& obj) {
    if (!obj.videoSourcePathUtf8.empty()) {
        const std::wstring p = Utf8ToWide(obj.videoSourcePathUtf8);
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(p), ec)) return p;
    }
    if (st && !obj.videoAssetKey.empty()) {
        if (const auto* bytes = st->doc.FindAsset(obj.videoAssetKey); bytes && !bytes->empty()) {
            wchar_t tmp[MAX_PATH]{};
            GetTempPathW(MAX_PATH, tmp);
            std::filesystem::path dir = std::filesystem::path(tmp) / L"CuadernU" / L"video_cache";
            std::error_code ec;
            std::filesystem::create_directories(dir, ec);
            std::filesystem::path out = dir / (L"vid_" + std::to_wstring(obj.id) + L".mp4");
            if (!std::filesystem::exists(out, ec) || std::filesystem::file_size(out, ec) != bytes->size()) {
                std::ofstream ofs(out, std::ios::binary | std::ios::trunc);
                if (ofs.is_open()) {
                    ofs.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
                }
            }
            return out.wstring();
        }
    }
    return {};
}

std::wstring GetCuadernUDataDir() {
    return StudioProfileStore::DefaultProfileRootDir();
}

std::wstring GetPersistentUndoPath(const StudioAppState* st) {
    std::filesystem::path dir = std::filesystem::path(GetCuadernUDataDir()) / L"history";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    std::string uuid = st ? st->doc.DocumentUuid() : "default";
    for (char& ch : uuid) {
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_') ch = '_';
    }
    return (dir / (Utf8ToWide(uuid) + L".undo.json")).wstring();
}

void SavePersistentUndoAndTruncateWal(StudioAppState* st) {
    if (!st) return;
    if (!st->currentDocPath.empty()) {
        CudocWal::Truncate(st->currentDocPath + L"-wal");
    }
    st->cmdStack.SaveSessionHistoryToFile(GetPersistentUndoPath(st), st->doc);
}

void TryLoadPersistentUndo(StudioAppState* st) {
    if (!st) return;
    if (st->cmdStack.LoadSessionHistoryFromFile(GetPersistentUndoPath(st), st->doc)) {
        st->statusBannerUtf8 += " (historial Undo restaurado)";
    }
}

void AppendWalMutation(StudioAppState* st, const std::string& opTag) {
    if (!st || st->currentDocPath.empty()) return;
    WalRecord rec{};
    std::vector<std::uint8_t> bytes;
    if (CudocStorage::SerializeToBytes(st->doc, bytes, nullptr) && !bytes.empty() && bytes.size() <= 2u * 1024u * 1024u) {
        static constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string b64;
        b64.reserve(((bytes.size() + 2u) / 3u) * 4u);
        for (std::size_t i = 0; i < bytes.size(); i += 3u) {
            const std::uint32_t b0 = bytes[i];
            const std::uint32_t b1 = (i + 1u < bytes.size()) ? bytes[i + 1u] : 0u;
            const std::uint32_t b2 = (i + 2u < bytes.size()) ? bytes[i + 2u] : 0u;
            const std::uint32_t t = (b0 << 16) | (b1 << 8) | b2;
            b64.push_back(kB64[(t >> 18) & 0x3Fu]);
            b64.push_back(kB64[(t >> 12) & 0x3Fu]);
            b64.push_back((i + 1u < bytes.size()) ? kB64[(t >> 6) & 0x3Fu] : '=');
            b64.push_back((i + 2u < bytes.size()) ? kB64[t & 0x3Fu] : '=');
        }
        rec.opTag = "snapshot";
        rec.payloadJson = nlohmann::json{ {"op", opTag}, {"rev", st->doc.Revision()}, {"cudocB64", std::move(b64)} }.dump();
    } else {
        rec.opTag = opTag;
        rec.payloadJson = "{\"rev\":" + std::to_string(st->doc.Revision()) + "}";
    }
    CudocWal::AppendRecord(st->currentDocPath + L"-wal", rec);
}

const char* PaperPatternLabelUtf8(PaperPattern pat) noexcept {
    switch (pat) {
    case PaperPattern::Plain:      return "Blanco";
    case PaperPattern::Ruled:      return "Rayado";
    case PaperPattern::Grid:       return "Cuadr\xC3\xAD" "cula";
    case PaperPattern::Dots:       return "Puntos";
    case PaperPattern::Millimeter: return "Milimetrado";
    case PaperPattern::Isometric:  return "Isom\xC3\xA9trico 30\xC2\xB0";
    case PaperPattern::LogLog:     return "Log-Log";
    case PaperPattern::Polar:      return "Polar";
    case PaperPattern::SmithChart: return "Carta de Smith";
    case PaperPattern::Cornell:    return "Cornell";
    case PaperPattern::MusicStaff: return "Pentagrama";
    }
    return "Blanco";
}

GuiDialogs::PromptFn MakePromptAdapter(StudioAppState* st) {
    return [st](const std::wstring& title, const std::wstring& label, std::wstring& value, bool multiline) -> bool {
        return ShowTextPrompt(st->hwndMain, title.c_str(), label.c_str(), value, nullptr, nullptr, multiline, false);
    };
}

GuiDialogs::PasswordPromptFn MakePasswordPromptAdapter(StudioAppState* st) {
    return [st](const std::wstring& title, const std::wstring& label, std::wstring& value) -> bool {
        return ShowTextPrompt(st->hwndMain, title.c_str(), label.c_str(), value, nullptr, nullptr, false, true);
    };
}

void ExecuteToolbarCommand(StudioAppState* st, WORD id);

void CommitInlineTextEdit(StudioAppState* st, bool save) {
    if (!st || !st->hwndInlineEdit) return;
    HWND hEdit = st->hwndInlineEdit;
    const std::uint64_t objId = st->inlineEditObjId;
    const ObjectTarget tgt = st->inlineEditTarget;
    st->hwndInlineEdit = nullptr;
    st->inlineEditObjId = 0;
    WORD slashDispatchedCmd = 0;

    if (save && objId != 0) {
        const int len = GetWindowTextLengthW(hEdit);
        std::wstring buf(static_cast<std::size_t>((std::max)(0, len)) + 1u, L'\0');
        GetWindowTextW(hEdit, buf.data(), static_cast<int>(buf.size()));
        buf.resize(static_cast<std::size_t>((std::max)(0, len)));
        if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, tgt, objId)) {
            AnnotationObject before = *obj;
            AnnotationObject after = *obj;
            after.textUtf8 = WideToUtf8(buf);
            if (!after.textUtf8.empty() && after.textUtf8.front() == '/') {
                if (st->commandPalette.All().empty()) st->commandPalette.RegisterDefaultCommands();
                if (after.textUtf8 == "/") {
                    slashDispatchedCmd = CMD_COMMAND_PALETTE;
                } else {
                    const auto matches = st->commandPalette.Search(after.textUtf8, 1);
                    if (!matches.empty() && matches.front().commandId > 0) {
                        slashDispatchedCmd = static_cast<WORD>(matches.front().commandId);
                    }
                }
            }
            if (after.textUtf8 != before.textUtf8) {
                const std::string editedText = after.textUtf8;
                st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)));
                PropagateSyncedBlocksInDocAndVault(st, editedText);
                // Live reactive math variable cascade when editing "var = number" (AP-MATH-056)
                const auto eqPos = editedText.find('=');
                if (eqPos != std::string::npos && eqPos > 0 && editedText.find('\n') == std::string::npos) {
                    std::string lhs = editedText.substr(0, eqPos);
                    std::string rhs = editedText.substr(eqPos + 1);
                    while (!lhs.empty() && lhs.front() == ' ') lhs.erase(lhs.begin());
                    while (!lhs.empty() && lhs.back() == ' ') lhs.pop_back();
                    while (!rhs.empty() && rhs.front() == ' ') rhs.erase(rhs.begin());
                    char* endPtr = nullptr;
                    const double numVal = std::strtod(rhs.c_str(), &endPtr);
                    if (!lhs.empty() && std::isalpha(static_cast<unsigned char>(lhs.front())) && endPtr && endPtr != rhs.c_str()) {
                        MathChem::ReactiveMathChemistryEngine::UpdateDeclaredVariableAndRecompute(st->doc, st->cmdStack, tgt, lhs, numVal);
                    }
                }
                AppendWalMutation(st, "edit_textbox");
                st->statusBannerUtf8 = "Texto actualizado";
            }
        }
    }
    if (IsWindow(hEdit)) DestroyWindow(hEdit);
    if (slashDispatchedCmd != 0) {
        ExecuteToolbarCommand(st, slashDispatchedCmd);
    }
    UpdateWindowTitle(st);
    InvalidateViews(st);
}

void BeginInlineTextEdit(StudioAppState* st, ObjectTarget tgt, const AnnotationObject& obj) {
    if (!st || !st->hwndViewport) return;
    CommitInlineTextEdit(st, true);
    double ox = 0.0, oy = 0.0;
    if (!tgt.isCanvas) {
        if (const auto* pl = st->doc.Layout().PlacementAt(tgt.pageIndex)) {
            const DocRect wr = pl->WorldRect();
            ox = wr.x; oy = wr.y;
        }
    }
    const DocRect scr = st->camera.WorldRectToScreen({ ox + obj.bounds.x, oy + obj.bounds.y, obj.bounds.w, obj.bounds.h });
    const int x = static_cast<int>(std::lround(scr.x));
    const int y = static_cast<int>(std::lround(scr.y));
    const int w = (std::max)(160, static_cast<int>(std::lround(scr.w)));
    const int h = (std::max)(44, static_cast<int>(std::lround(scr.h)));

    st->hwndInlineEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", Utf8ToWide(obj.textUtf8).c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        x, y, w, h, st->hwndViewport, reinterpret_cast<HMENU>(CMD_INLINE_EDIT_CTRL),
        GetModuleHandleW(nullptr), nullptr);
    if (st->hwndInlineEdit) {
        st->inlineEditObjId = obj.id;
        st->inlineEditTarget = tgt;
        SendMessageW(st->hwndInlineEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st->hUiFont), TRUE);
        SetFocus(st->hwndInlineEdit);
        SendMessageW(st->hwndInlineEdit, EM_SETSEL, 0, -1);
    }
}

void SaveActiveTabSnapshot(StudioAppState* st) {
    if (!st) return;
    if (st->tabs.empty()) {
        st->tabs.push_back({});
        st->activeTabIdx = 0;
    }
    if (st->activeTabIdx >= st->tabs.size()) st->activeTabIdx = st->tabs.size() - 1;
    auto& t = st->tabs[st->activeTabIdx];
    t.path = st->currentDocPath;
    t.cam = st->camera;
    t.title = st->doc.Title();
    t.serializedCudoc.clear();
    CudocStorage::SerializeToBytes(st->doc, t.serializedCudoc);
}

void SwitchToDocTab(StudioAppState* st, std::size_t newIdx) {
    if (!st || st->tabs.empty()) return;
    SaveActiveTabSnapshot(st);
    newIdx %= st->tabs.size();
    st->activeTabIdx = newIdx;
    auto& t = st->tabs[newIdx];
    if (!t.serializedCudoc.empty()) {
        DocumentModel loaded;
        if (CudocStorage::DeserializeFromBytes(t.serializedCudoc.data(), t.serializedCudoc.size(), loaded)) {
            st->doc = std::move(loaded);
        }
    }
    st->currentDocPath = t.path;
    st->camera = t.cam;
    st->selectedObjectId = 0;
    st->lassoSelectedIds.clear();
    st->cmdStack.Clear();
    st->pageCache.InvalidateAll(++st->asyncGen);
    st->statusBannerUtf8 = "Pesta\xC3\xB1""a " + std::to_string(newIdx + 1) + "/" + std::to_string(st->tabs.size()) + ": " + st->doc.Title();
    LayoutMainChildren(st);
    UpdateWindowTitle(st);
    InvalidateViews(st);
}

void AddNewDocTab(StudioAppState* st, bool quickNote) {
    if (!st) return;
    SaveActiveTabSnapshot(st);
    StudioAppState::OpenDocTab nt{};
    DocumentModel fresh;
    fresh.ResetToBlank(quickNote ? DocumentMode::InfiniteCanvas : DocumentMode::PaginatedPdf);
    if (quickNote) {
        fresh.SetCanvasBackground(PaperPattern::Dots);
        fresh.SetTitle("Nota R\xC3\xA1pida");
    } else {
        fresh.SetTitle("Cuaderno " + std::to_string(st->tabs.size() + 1));
    }
    CudocStorage::SerializeToBytes(fresh, nt.serializedCudoc);
    nt.title = fresh.Title();
    nt.cam.zoom = 1.0;
    nt.cam.panX = -48.0;
    nt.cam.panY = -48.0;
    st->tabs.push_back(std::move(nt));
    SwitchToDocTab(st, st->tabs.size() - 1);
}

void PollCasioIpcInbox(StudioAppState* st) {
    if (!st) return;
    std::filesystem::path inbox = std::filesystem::path(GetCuadernUDataDir()) / L"ipc_inbox";
    std::error_code ec;
    if (!std::filesystem::exists(inbox, ec)) return;
    bool anyAdded = false;
    for (const auto& ent : std::filesystem::directory_iterator(inbox, ec)) {
        if (!ent.is_regular_file()) continue;
        if (ent.path().extension() != L".json") continue;
        std::ifstream ifs(ent.path(), std::ios::binary);
        if (!ifs.is_open()) continue;
        std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        ifs.close();
        try {
            auto j = nlohmann::json::parse(raw);
            CalcBridge::CalcStepRecord rec{};
            rec.modelIdUtf8 = j.value("model", "fx-991CW");
            rec.keySeqUtf8 = j.value("keySeq", "[CALC]");
            rec.expressionUtf8 = j.value("expression", "");
            if (!rec.expressionUtf8.empty() || !rec.keySeqUtf8.empty()) {
                double x = 80.0, y = 80.0;
                NextCardPlacement(st, x, y);
                const ObjectTarget tgt = CurrentTarget(st);
                const auto newId = AppActions::AddCalcCardAction(st->doc, st->cmdStack, tgt, x, y, rec, st->statusBannerUtf8);
                if (newId != 0) {
                    st->selectedObjectId = newId;
                    st->selectedIsCanvas = tgt.isCanvas;
                    st->selectedPageIndex = tgt.pageIndex;
                    st->statusBannerUtf8 = "Tarjeta CASIO recibida en vivo (" + rec.modelIdUtf8 + ")";
                    anyAdded = true;
                }
            }
        } catch (...) {}
        std::filesystem::remove(ent.path(), ec);
    }
    if (anyAdded) {
        UpdateWindowTitle(st);
        InvalidateViews(st);
    }
}

bool SnipWorldRegionAsImage(
    StudioAppState* st,
    const DocRect& worldRect,
    const std::vector<DocPoint>* optionalPolyWorld = nullptr)
{
    if (!st || worldRect.w < 12.0 || worldRect.h < 12.0) return false;
    const double maxDim = (std::max)(worldRect.w, worldRect.h);
    const double scale = std::clamp(1100.0 / maxDim, 1.5, 2.75);
    const int pxW = std::clamp(static_cast<int>(std::lround(worldRect.w * scale)), 24, 2800);
    const int pxH = std::clamp(static_cast<int>(std::lround(worldRect.h * scale)), 24, 2800);

    Camera2D snipCam{};
    snipCam.panX = worldRect.x;
    snipCam.panY = worldRect.y;
    snipCam.zoom = static_cast<double>(pxW) / worldRect.w;

    ViewportOverlayState snipOv{};
    snipOv.showSelectionHandles = false;
    snipOv.darkTheme = false;
    snipOv.highContrast = false;

    RasterImageBgra32 bgra;
    if (!st->renderer.RenderHwndViewportToOffscreen(st->doc, snipCam, pxW, pxH, &st->pageCache, bgra, &snipOv) ||
        !bgra.Valid()) {
        return false;
    }

    const bool hasPolyMask = (optionalPolyWorld && optionalPolyWorld->size() >= 3);
    if (hasPolyMask) {
        CanvasManip::ApplyPolygonMaskToBgra32(bgra, worldRect, *optionalPolyWorld);
    }

    std::vector<std::uint8_t> pngBytes;
    if (!WinRtPdfRenderer::EncodeBgra32ToPngBytes(bgra, pngBytes) || pngBytes.empty()) {
        return false;
    }

    AnnotationObject snipObj{};
    snipObj.kind = ObjectKind::Image;
    snipObj.layerId = st->doc.ActiveLayerId();
    snipObj.bounds = worldRect;
    snipObj.pngBytes = std::move(pngBytes);
    snipObj.imagePixelW = bgra.width;
    snipObj.imagePixelH = bgra.height;
    snipObj.captionUtf8 = hasPolyMask ? "Recorte Lazo/Circular" : "Recorte Rectangular";

    st->objectClipboard = { snipObj };
    CanvasManip::CopyBgra32ToClipboardWin32(st->hwndMain, bgra);
    st->statusBannerUtf8 = std::string(hasPolyMask ? "Recorte circular/lazo" : "Recorte rectangular") +
        " copiado como imagen (" + std::to_string(bgra.width) + "x" + std::to_string(bgra.height) +
        " px) \xE2\x80\x94 Presione Ctrl+V o clic derecho -> Pegar en cualquier hoja o lienzo";
    return true;
}

// ---------------------------------------------------------------------------
// Borradores y pregunta al cerrar (auditoría de concentración, P0): olvidarse de guardar no debe costar
// el trabajo. Mientras haya cambios sin guardar se escribe cada 60 s una copia en
// <perfil>\borradores; al cerrar o al abrir otro documento se pregunta Guardar / No guardar /
// Cancelar, y «No guardar» deja igual una copia por kDraftKeepDays días.
// ---------------------------------------------------------------------------
std::filesystem::path DraftsDirectory() {
    std::filesystem::path dir = std::filesystem::path(StudioProfileStore::DefaultProfileRootDir()) / L"borradores";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir;
}

std::wstring DocumentDisplayName(const StudioAppState* st) {
    if (!st->currentDocPath.empty()) return std::filesystem::path(st->currentDocPath).filename().wstring();
    const std::wstring title = Utf8ToWide(st->doc.Title());
    return title.empty() ? std::wstring(L"Documento nuevo") : title;
}

// "<prefijo> 2026-11-26 17-45-03 - <nombre> [<pid>].cudoc" (el pid distingue ventanas abiertas a la vez)
std::wstring MakeDraftFileName(const wchar_t* prefix, const std::wstring& title) {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    wchar_t stamp[40]{};
    swprintf_s(stamp, L"%04u-%02u-%02u %02u-%02u-%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    std::wstring clean;
    for (wchar_t c : title) clean.push_back((c < 32 || wcschr(L"\\/:*?\"<>|[]", c)) ? L'-' : c);
    if (clean.size() > 60) clean.resize(60);
    while (!clean.empty() && (clean.back() == L' ' || clean.back() == L'.')) clean.pop_back();
    if (clean.empty()) clean = L"Documento";
    return std::wstring(prefix) + L" " + stamp + L" - " + clean + L" [" + std::to_wstring(GetCurrentProcessId()) + L"].cudoc";
}

// Copia del documento activo: no toca el archivo de la persona ni lo marca como guardado.
bool WriteDocumentCopy(StudioAppState* st, const std::wstring& target) {
    std::string err;
    return CudocStorage::SaveToFile(target, st->doc, &err);
}

void DiscardSessionDraft(StudioAppState* st) {
    if (!st || st->sessionDraftPath.empty()) return;
    std::error_code ec;
    std::filesystem::remove(st->sessionDraftPath, ec);
    std::filesystem::remove(st->sessionDraftPath + L".bak", ec);
    st->sessionDraftPath.clear();
}

void AutosaveSessionDraft(StudioAppState* st) {
    if (!st || IsAutomationRun(st) || !st->doc.IsDirty()) return;
    if (st->sessionDraftPath.empty()) {
        st->sessionDraftPath = (DraftsDirectory() / MakeDraftFileName(L"Autoguardado", DocumentDisplayName(st))).wstring();
    }
    WriteDocumentCopy(st, st->sessionDraftPath);
}

// «No guardar»: queda una copia con nombre propio y se borra la copia automática de esta sesión.
void KeepDiscardedCopy(StudioAppState* st) {
    if (!st) return;
    WriteDocumentCopy(st, (DraftsDirectory() / MakeDraftFileName(L"Sin guardar", DocumentDisplayName(st))).wstring());
    DiscardSessionDraft(st);
}

// Las otras pestañas viven en memoria; al cerrar se dejan como borrador para no perderlas.
void KeepInactiveTabsAsDrafts(StudioAppState* st) {
    if (!st || IsAutomationRun(st) || st->tabs.size() < 2) return;
    for (std::size_t i = 0; i < st->tabs.size(); ++i) {
        if (i == st->activeTabIdx) continue;
        const auto& tab = st->tabs[i];
        if (tab.serializedCudoc.empty()) continue;
        const std::wstring name = tab.path.empty() ? Utf8ToWide(tab.title)
                                                   : std::filesystem::path(tab.path).filename().wstring();
        std::ofstream out(DraftsDirectory() / MakeDraftFileName(L"Pestaña", name), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(tab.serializedCudoc.data()),
                  static_cast<std::streamsize>(tab.serializedCudoc.size()));
    }
}

// Pregunta qué hacer con los cambios sin guardar antes de cerrar o de reemplazar el documento.
// true = se puede seguir (no había cambios, se guardó, o se eligió «No guardar» con copia).
bool ConfirmUnsavedChanges(StudioAppState* st, const wchar_t* beforeWhat) {
    if (!st) return true;
    CommitInlineTextEdit(st, true);
    if (IsAutomationRun(st) || !st->doc.IsDirty()) return true;
    // Cerrar desde la barra de tareas con la ventana minimizada: sin esto la pregunta queda escondida.
    if (st->hwndMain && IsIconic(st->hwndMain)) ShowWindow(st->hwndMain, SW_RESTORE);

    const std::wstring question = L"¿Guardar los cambios de «" + DocumentDisplayName(st) +
                                  L"» antes de " + beforeWhat + L"?";
    const std::wstring detail = L"Si elige «No guardar», queda igual una copia de seguridad durante " +
                                std::to_wstring(kDraftKeepDays) + L" días en Archivo → Recuperar borradores.";
    enum : int { kBtnSave = 1001, kBtnDontSave = 1002 };
    const TASKDIALOG_BUTTON buttons[] = {
        { kBtnSave, L"Guardar" },
        { kBtnDontSave, L"No guardar" },
        { IDCANCEL, L"Cancelar" },
    };
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = st->hwndMain;
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = L"CuadernU";
    cfg.pszMainInstruction = question.c_str();
    cfg.pszContent = detail.c_str();
    cfg.cButtons = ARRAYSIZE(buttons);
    cfg.pButtons = buttons;
    cfg.nDefaultButton = kBtnSave;
    int choice = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&cfg, &choice, nullptr, nullptr))) {
        KeepDiscardedCopy(st); // sin diálogo no se puede preguntar: al menos no se pierde nada
        return true;
    }
    if (choice == kBtnSave) {
        ExecuteToolbarCommand(st, CMD_SAVE_CUDOC);
        return !st->doc.IsDirty(); // si canceló «Guardar como» o falló, no se sigue
    }
    if (choice == kBtnDontSave) {
        KeepDiscardedCopy(st);
        return true;
    }
    return false;
}

void PruneOldDrafts() {
    std::error_code ec;
    const auto limit = std::filesystem::file_time_type::clock::now() - std::chrono::hours(24 * kDraftKeepDays);
    for (const auto& entry : std::filesystem::directory_iterator(DraftsDirectory(), ec)) {
        std::error_code e2;
        if (!entry.is_regular_file(e2)) continue;
        const auto when = entry.last_write_time(e2);
        if (!e2 && when < limit) std::filesystem::remove(entry.path(), e2);
    }
}

bool IsCuadernUProcessAlive(DWORD pid) {
    if (pid == 0 || pid == GetCurrentProcessId()) return pid != 0;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return false;
    wchar_t image[MAX_PATH]{};
    DWORD len = MAX_PATH;
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE &&
                       QueryFullProcessImageNameW(h, 0, image, &len);
    CloseHandle(h);
    if (!alive) return false;
    std::wstring name = std::filesystem::path(image).filename().wstring();
    for (wchar_t& c : name) c = static_cast<wchar_t>(towlower(c));
    return name == L"cuadernu.exe" || name == L"documentstudio.exe";
}

// Abre un borrador como documento sin guardar: «Guardar» pedirá dónde dejarlo (nunca dentro de
// la carpeta de borradores) y cerrar volverá a preguntar.
bool OpenDocumentFile(StudioAppState* st, const std::wstring& path, std::string* outErr = nullptr);
bool OpenDraftAsUnsaved(StudioAppState* st, const std::wstring& draftPath) {
    std::string err;
    if (!OpenDocumentFile(st, draftPath, &err)) {
        if (!err.empty()) MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"No se pudo abrir el borrador", MB_ICONERROR);
        return false;
    }
    st->currentDocPath.clear();
    st->doc.SetDirty(true);
    st->statusBannerUtf8 = "Borrador recuperado: use Guardar para elegir d\xC3\xB3nde dejarlo";
    UpdateWindowTitle(st);
    SyncDynamicTimers(st);
    InvalidateViews(st);
    return true;
}

// Al arrancar: copias «Autoguardado» de una sesión que ya no corre = cierre inesperado.
void OfferDraftsFromPreviousSession(StudioAppState* st) {
    if (!st || IsAutomationRun(st)) return;
    struct Leftover { std::filesystem::path path; std::filesystem::file_time_type when; };
    std::vector<Leftover> leftovers;
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(DraftsDirectory(), ec)) {
        std::error_code e2;
        if (!entry.is_regular_file(e2) || entry.path().extension() != L".cudoc") continue;
        const std::wstring name = entry.path().filename().wstring();
        if (name.rfind(L"Autoguardado ", 0) != 0) continue;
        DWORD pid = 0;
        const auto open = name.rfind(L'[');
        const auto close = name.rfind(L']');
        if (open != std::wstring::npos && close != std::wstring::npos && close > open + 1) {
            pid = static_cast<DWORD>(wcstoul(name.substr(open + 1, close - open - 1).c_str(), nullptr, 10));
        }
        if (IsCuadernUProcessAlive(pid)) continue; // es de otra ventana de CuadernU abierta ahora
        leftovers.push_back({ entry.path(), entry.last_write_time(e2) });
    }
    if (leftovers.empty()) return;
    std::sort(leftovers.begin(), leftovers.end(), [](const Leftover& a, const Leftover& b) { return a.when > b.when; });

    // Se renombran a «Recuperado …» para no volver a preguntar; siguen en Recuperar borradores.
    std::vector<std::filesystem::path> renamed;
    for (const auto& lo : leftovers) {
        std::wstring name = lo.path.filename().wstring();
        name.replace(0, wcslen(L"Autoguardado"), L"Recuperado");
        std::filesystem::path target = lo.path.parent_path() / name;
        std::error_code e3;
        std::filesystem::rename(lo.path, target, e3);
        renamed.push_back(e3 ? lo.path : target);
    }

    std::wstring shown = renamed.front().stem().wstring();
    if (const auto dash = shown.find(L" - "); dash != std::wstring::npos) shown = shown.substr(dash + 3);
    if (const auto bracket = shown.rfind(L" ["); bracket != std::wstring::npos) shown.resize(bracket);
    std::wstring detail = L"«" + shown + L"» quedó sin guardar cuando CuadernU se cerró de golpe.";
    if (renamed.size() > 1) {
        detail += L" Hay " + std::to_wstring(renamed.size() - 1) + L" copia(s) más en Archivo → Recuperar borradores.";
    }
    detail += L" Si elige «Ahora no», queda ahí durante " + std::to_wstring(kDraftKeepDays) + L" días.";
    enum : int { kBtnOpen = 1001 };
    const TASKDIALOG_BUTTON buttons[] = { { kBtnOpen, L"Abrirlo ahora" }, { IDCANCEL, L"Ahora no" } };
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = st->hwndMain;
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = L"CuadernU";
    cfg.pszMainInstruction = L"Hay un trabajo que no se guardó la última vez";
    cfg.pszContent = detail.c_str();
    cfg.cButtons = ARRAYSIZE(buttons);
    cfg.pButtons = buttons;
    cfg.nDefaultButton = kBtnOpen;
    int choice = IDCANCEL;
    if (SUCCEEDED(TaskDialogIndirect(&cfg, &choice, nullptr, nullptr)) && choice == kBtnOpen) {
        OpenDraftAsUnsaved(st, renamed.front().wstring());
    }
}

bool OpenDocumentFile(StudioAppState* st, const std::wstring& path, std::string* outErr) {
    const std::wstring ext = GetFileExtLower(path);
    // Estos formatos reemplazan el documento abierto (imágenes y videos se insertan en él).
    const bool replacesDocument = ext == L".cudoc" || ext == L".pdf" || ext == L".docx" || ext == L".pptx" ||
                                  ext == L".goodnotes" || ext == L".note" || ext == L".one" || ext == L".onepkg" ||
                                  ext == L".tinta";
    if (replacesDocument && !ConfirmUnsavedChanges(st, L"abrir otro documento")) {
        if (outErr) outErr->clear();
        st->statusBannerUtf8 = "Se mantuvo el documento actual";
        InvalidateViews(st);
        return false;
    }
    const std::string pathUtf8 = WideToUtf8(path);

    if (ext == L".goodnotes" || ext == L".note" || ext == L".one" || ext == L".onepkg" || ext == L".tinta") {
        DocumentModel migrated;
        Interop::MigrationResult migRes{};
        if (!Interop::NotebookMigrationEngine::ImportAndMigrateFile(path, migrated, migRes)) {
            if (outErr) *outErr = migRes.errorUtf8;
            st->statusBannerUtf8 = "Error de migraci\xC3\xB3n: " + migRes.errorUtf8;
            return false;
        }
        st->doc = std::move(migrated);
        if (!migRes.primaryAudioWavBytes.empty()) {
            st->audioSession.LoadFromWavBytes(migRes.primaryAudioWavBytes.data(), migRes.primaryAudioWavBytes.size());
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPath.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "Migrado desde " + migRes.report.formatLabelUtf8 + " (" +
            std::to_string(migRes.report.pageCount) + " p\xC3\xA1g., " +
            std::to_string(migRes.report.editableStrokeCount) + " trazos editables)";
    } else if (ext == L".pdf") {
        PdfDocument pdf;
        std::string err;
        if (!pdf.LoadFromFile(path, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error PDF: " + err;
            return false;
        }
        if (!pdf.PopulateDocumentModel(st->doc, pathUtf8, &err)) {
            if (outErr) *outErr = err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPath.clear();
        st->activePdfSourcePathUtf8 = pathUtf8;
        st->doc.SetTitle(WideToUtf8(std::filesystem::path(path).stem().wstring()));
        CenterCameraOnPage(st, static_cast<std::size_t>((std::max)(0, st->initialPage1Based - 1)));
        st->initialPage1Based = 1;
        st->statusBannerUtf8 = "PDF abierto: " + std::to_string(st->doc.Pages().size()) + " p\xC3\xA1ginas";
    } else if (ext == L".cudoc") {
        bool usedBak = false;
        std::string err;
        std::size_t replayed = 0;
        const std::wstring walPath = path + L"-wal";
        bool loadedOk = false;
        if (std::filesystem::exists(walPath)) {
            loadedOk = CudocWal::RecoverDocument(path, walPath, st->doc, &replayed, &err);
        }
        if (!loadedOk) {
            loadedOk = CudocStorage::LoadFromFileWithRecovery(path, st->doc, &usedBak, &err);
        }
        if (!loadedOk) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .cudoc: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPath = path;
        {
            DocumentExtensionBundle extB = BuildExtensionBundle(st);
            StudioProfileStore::UnpackExtensionsFromDocument(st->doc, extB);
            st->docYamlFrontmatterUtf8 = extB.yamlFrontmatterUtf8;
            st->docProperties = extB.properties;
            st->docAliasesUtf8 = extB.aliasesUtf8;
            st->docTagsUtf8 = extB.tagsUtf8;
            if (!extB.customCssUtf8.empty()) st->uiPrefs.customCssSnippetUtf8 = extB.customCssUtf8;
            st->activeDbViewCycle = std::clamp(extB.activeDbViewKind, 0, 8);
        }
        st->contentViews.UpsertFromDocument(st->doc, pathUtf8);
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = (replayed > 0)
            ? ("Recuperado con WAL (" + std::to_string(replayed) + " deltas)")
            : (usedBak ? "Recuperado desde respaldo .bak" : ".cudoc cargado");
        TryLoadPersistentUndo(st);
    } else if (ext == L".docx") {
        std::string err;
        if (!OpenXmlInterop::ImportFromDocxFile(path, st->doc, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .docx: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPath.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "Word .docx importado";
    } else if (ext == L".pptx") {
        std::string err;
        if (!OpenXmlInterop::ImportFromPptxFile(path, st->doc, &err)) {
            if (outErr) *outErr = err;
            st->statusBannerUtf8 = "Error .pptx: " + err;
            return false;
        }
        st->cmdStack.Clear();
        st->pageCache.InvalidateAll(++st->asyncGen);
        st->currentDocPath.clear();
        CenterCameraOnPage(st, 0);
        st->statusBannerUtf8 = "PowerPoint .pptx importado";
    } else if (ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp" ||
               ext == L".tif" || ext == L".tiff" || ext == L".webp" || ext == L".svg" || ext == L".gif") {
        HANDLE hf = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hf == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER li{};
        GetFileSizeEx(hf, &li);
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>((std::min<LONGLONG>)(li.QuadPart, 64ll * 1024ll * 1024ll)));
        DWORD rd = 0;
        ReadFile(hf, bytes.data(), static_cast<DWORD>(bytes.size()), &rd, nullptr);
        CloseHandle(hf);
        bytes.resize(rd);

        ImageProc::RgbaImage dec{};
        std::vector<std::uint8_t> pngOut;
        int pxW = 480, pxH = 320;
        if (ImageProc::DecodeImageBytesWic(bytes.data(), bytes.size(), dec) && dec.Valid()) {
            pxW = dec.width;
            pxH = dec.height;
            // Preserve raw multi-frame GIF bytes so WIC animates all frames live in Direct2D (NT-EDIT-031)
            if (ext == L".gif") pngOut = std::move(bytes);
            else if (!ImageProc::EncodeRgbaToPngWic(dec, pngOut)) pngOut = std::move(bytes);
        } else {
            pngOut = std::move(bytes);
        }

        AnnotationObject img;
        img.kind = ObjectKind::Image;
        img.layerId = st->doc.ActiveLayerId();
        if (ext == L".gif") {
            int fCnt = 0;
            std::vector<int> delays;
            if (D2DRenderer::InspectGifMetadataWic(pngOut, fCnt, delays) && fCnt > 1) {
                img.gifFrameCount = fCnt;
                int sumD = 0;
                for (int d : delays) sumD += d;
                img.gifAvgDelayMs = delays.empty() ? 100 : (sumD / static_cast<int>(delays.size()));
            }
        }
        img.pngBytes = std::move(pngOut);
        img.imagePixelW = pxW;
        img.imagePixelH = pxH;
        const double dispW = std::clamp(static_cast<double>(pxW) * 0.65, 120.0, 480.0);
        const double dispH = dispW * (static_cast<double>(pxH) / (std::max)(1, pxW));
        img.bounds = { 48.0, 48.0, dispW, dispH };
        if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ true, 0 }, std::move(img)));
        } else {
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, st->activePageIndex }, std::move(img)));
        }
        st->statusBannerUtf8 = (ext == L".gif" && img.gifFrameCount > 1)
            ? ("GIF animado insertado (" + std::to_string(img.gifFrameCount) + " cuadros, " + std::to_string(pxW) + "x" + std::to_string(pxH) + ")")
            : ("Imagen insertada (" + std::to_string(pxW) + "x" + std::to_string(pxH) + ")");
    } else if (ext == L".mp4" || ext == L".wmv" || ext == L".mkv" || ext == L".avi" || ext == L".webm") {
        RasterImageBgra32 posterBgra;
        std::vector<std::uint8_t> posterPng;
        std::uint64_t durMs = 0;
        int vw = 640, vh = 360;
        std::string vErr;
        ExtractVideoPosterAndMetadata(path, posterBgra, posterPng, durMs, vw, vh, &vErr);
        std::vector<std::uint8_t> vbytes;
        std::error_code ec;
        const auto fsz = std::filesystem::file_size(std::filesystem::path(path), ec);
        std::string vKey;
        if (!ec && fsz > 0 && fsz <= 16ull * 1024ull * 1024ull) {
            std::ifstream ifs(path, std::ios::binary);
            if (ifs.is_open()) {
                vbytes.resize(static_cast<std::size_t>(fsz));
                ifs.read(reinterpret_cast<char*>(vbytes.data()), static_cast<std::streamsize>(fsz));
                vKey = st->doc.RegisterAsset(vbytes);
            }
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const double aspect = (vw > 0 && vh > 0) ? (static_cast<double>(vh) / static_cast<double>(vw)) : 0.5625;
        const double dw = 360.0;
        const double dh = std::clamp(dw * aspect, 160.0, 320.0);
        AnnotationObject vid{};
        vid.kind = ObjectKind::VideoClip;
        vid.layerId = st->doc.ActiveLayerId();
        vid.bounds = { x, y, dw, dh };
        vid.captionUtf8 = std::filesystem::path(path).filename().u8string();
        vid.videoSourcePathUtf8 = pathUtf8;
        vid.videoAssetKey = vKey;
        vid.videoDurationMs = durMs;
        vid.videoPosterPngBytes = std::move(posterPng);
        vid.imagePixelW = vw;
        vid.imagePixelH = vh;
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(vid)));
        st->statusBannerUtf8 = "Video insertado (" + std::to_string(durMs / 1000) + " s, doble clic para reproducir)";
    } else {
        if (outErr) *outErr = "Extensi\xC3\xB3n de archivo no soportada.";
        return false;
    }

    if (replacesDocument) {
        // Recién abierto = nada que guardar todavía (un PDF recién abierto salía como modificado).
        // El .cudoc ya llega limpio, salvo si el registro WAL recuperó cambios: esos se respetan.
        if (ext != L".cudoc") st->cmdStack.MarkClean(st->doc);
        DiscardSessionDraft(st);
    }

    LayoutMainChildren(st);
    UpdateWindowTitle(st);
    InvalidateViews(st);
    SyncDynamicTimers(st);
    return true;
}

void ExecuteToolbarCommand(StudioAppState* st, WORD id) {
    if (!st) return;
    switch (id) {
    case CMD_OPEN: {
        static constexpr COMDLG_FILTERSPEC kFilters[] = {
            { L"Todos compatibles (*.cudoc;*.pdf;*.goodnotes;*.note;*.one;*.onepkg;*.tinta;*.docx;*.pptx;*.png;*.gif;*.svg)",
              L"*.cudoc;*.pdf;*.goodnotes;*.note;*.one;*.onepkg;*.tinta;*.docx;*.pptx;*.png;*.jpg;*.jpeg;*.gif;*.bmp;*.tif;*.tiff;*.webp;*.svg" },
            { L"Cuadernos CuadernU (*.cudoc;*.tinta)", L"*.cudoc;*.tinta" },
            { L"Goodnotes / Notability / OneNote (*.goodnotes;*.note;*.one;*.onepkg)", L"*.goodnotes;*.note;*.one;*.onepkg" },
            { L"Documentos PDF / Office (*.pdf;*.docx;*.pptx)", L"*.pdf;*.docx;*.pptx" }
        };
        std::vector<std::wstring> paths;
        if (ShowOpenDialog(st->hwndMain, L"Abrir en CuadernU", kFilters, 4, paths, false)) {
            std::string err;
            if (!OpenDocumentFile(st, paths[0], &err) && !err.empty()) { // vacío = la persona eligió «Cancelar»
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error al abrir", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_OPEN_DRAFTS: {
        static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"Borradores de CuadernU (*.cudoc)", L"*.cudoc" } };
        std::vector<std::wstring> paths;
        const std::wstring draftsDir = DraftsDirectory().wstring();
        if (ShowOpenDialog(st->hwndMain, L"Recuperar un borrador", kFilters, 1, paths, false, draftsDir.c_str())) {
            OpenDraftAsUnsaved(st, paths[0]);
        }
        break;
    }
    case CMD_SAVE_CUDOC: {
        std::wstring savePath = st->currentDocPath;
        if (savePath.empty()) {
            static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"Cuaderno CuadernU (*.cudoc)", L"*.cudoc" } };
            if (!ShowSaveDialog(st->hwndMain, L"Guardar cuaderno CuadernU (.cudoc)", kFilters, 1, L"cudoc", L"cuaderno.cudoc", savePath)) break;
        }
        StudioProfileStore::PackExtensionsIntoDocument(st->doc, BuildExtensionBundle(st));
        std::string err;
        if (CudocStorage::SaveToFile(savePath, st->doc, &err)) {
            st->currentDocPath = savePath;
            st->cmdStack.MarkClean(st->doc);
            DiscardSessionDraft(st); // ya está a salvo en su archivo
            SavePersistentUndoAndTruncateWal(st);
            st->contentViews.UpsertFromDocument(st->doc, WideToUtf8(savePath));
            PersistStudioProfileNow(st);
            st->statusBannerUtf8 = "Guardado at\xC3\xB3micamente (.cudoc + extensiones + perfil)";
            UpdateWindowTitle(st);
        } else {
            MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error al guardar", MB_ICONERROR);
        }
        break;
    }
    case CMD_EXPORT_PDF: {
        static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"Archivo PDF (*.pdf)", L"*.pdf" } };
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, L"Exportar PDF", kFilters, 1, L"pdf", L"exportado.pdf", outPath)) break;
        std::string err;
        bool ok = false;
        if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
            CanvasPdfExportOptions opts;
            opts.paginated = true;
            ok = PdfWriter::ExportInfiniteCanvasToFile(st->doc, opts, outPath, &err);
        } else {
            ok = PdfWriter::ExportDocumentToFile(st->doc, outPath, &err);
        }
        st->statusBannerUtf8 = ok ? "PDF exportado correctamente" : ("Error PDF: " + err);
        if (!ok) MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando PDF", MB_ICONERROR);
        break;
    }
    case CMD_MERGE_PDF: {
        static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"Archivos PDF (*.pdf)", L"*.pdf" } };
        std::vector<std::wstring> inputs;
        if (!ShowOpenDialog(st->hwndMain, L"Seleccionar PDFs para combinar", kFilters, 1, inputs, true) || inputs.size() < 2) break;
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, L"Guardar PDF combinado", kFilters, 1, L"pdf", L"combinado.pdf", outPath)) break;
        std::string err;
        if (PdfWriter::MergePdfFiles(inputs, outPath, &err)) {
            OpenDocumentFile(st, outPath);
            st->statusBannerUtf8 = "PDFs combinados (" + std::to_string(inputs.size()) + " archivos)";
        } else {
            MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error combinando PDF", MB_ICONERROR);
        }
        break;
    }
    case CMD_SPLIT_PDF: {
        std::wstring rangeStr = L"1-" + std::to_wstring((std::max<std::size_t>)(1u, st->doc.Pages().size()));
        if (!ShowTextPrompt(st->hwndMain, L"Dividir / Extraer P\u00E1ginas", L"P\u00E1ginas (ej. 1-3, 5):", rangeStr)) break;
        std::vector<std::size_t> picked;
        std::wstringstream wss(rangeStr);
        std::wstring part;
        while (std::getline(wss, part, L',')) {
            const auto dash = part.find(L'-');
            if (dash != std::wstring::npos) {
                const int a = _wtoi(part.substr(0, dash).c_str());
                const int b = _wtoi(part.substr(dash + 1).c_str());
                for (int p = (std::max)(1, a); p <= b && p <= static_cast<int>(st->doc.Pages().size()); ++p) {
                    picked.push_back(static_cast<std::size_t>(p - 1));
                }
            } else {
                const int p = _wtoi(part.c_str());
                if (p >= 1 && p <= static_cast<int>(st->doc.Pages().size())) picked.push_back(static_cast<std::size_t>(p - 1));
            }
        }
        if (picked.empty()) break;
        static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"Archivo PDF (*.pdf)", L"*.pdf" } };
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, L"Guardar PDF dividido", kFilters, 1, L"pdf", L"extraccion.pdf", outPath)) break;
        DocumentModel subset;
        subset.ResetToBlank(DocumentMode::PaginatedPdf);
        subset.MutablePages().clear();
        for (std::size_t idx : picked) subset.MutablePages().push_back(st->doc.Pages()[idx]);
        subset.RebuildLayout();
        std::string err;
        if (PdfWriter::ExportDocumentToFile(subset, outPath, &err)) {
            st->statusBannerUtf8 = "Extra\xC3\xAD" "das " + std::to_string(picked.size()) + " p\xC3\xA1ginas";
        } else {
            MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error dividiendo PDF", MB_ICONERROR);
        }
        break;
    }
    case CMD_SIGN_PDF: {
        std::wstring signer = L"Usuario Calculadora Unificada";
        std::wstring reason = L"Revisado y aprobado";
        if (!ShowTextPrompt(st->hwndMain, L"Firma Digital PKCS#7 (SHA-256)", L"Firmante:", signer, L"Motivo:", &reason)) break;
        static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"PDF Firmado (*.pdf)", L"*.pdf" } };
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, L"Guardar PDF firmado", kFilters, 1, L"pdf", L"firmado.pdf", outPath)) break;
        PdfSignatureOptions sopts;
        sopts.signerNameUtf8 = WideToUtf8(signer);
        sopts.reasonUtf8 = WideToUtf8(reason);
        sopts.addVisualStamp = true;
        sopts.visualPageIndex0 = st->activePageIndex;
        std::string err;
        if (PdfSigner::ExportAndSignDocument(st->doc, sopts, outPath, L"", L"", &err)) {
            st->statusBannerUtf8 = "PDF firmado criptogr\xC3\xA1" "ficamente (PKCS#7 SHA-256)";
            MessageBoxW(st->hwndMain, L"Documento firmado con PKCS#7 detached SHA-256.\nPuede verificarlo con el bot\u00F3n 'Verificar'.", L"Firma exitosa", MB_ICONINFORMATION);
        } else {
            MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error firmando PDF", MB_ICONERROR);
        }
        break;
    }
    case CMD_VERIFY_SIG: {
        static constexpr COMDLG_FILTERSPEC kFilters[] = { { L"Archivo PDF (*.pdf)", L"*.pdf" } };
        std::vector<std::wstring> paths;
        if (!ShowOpenDialog(st->hwndMain, L"Seleccionar PDF a verificar", kFilters, 1, paths, false)) break;
        const auto vres = PdfSigner::VerifyPdfFile(paths[0]);
        const UINT icon = (vres.hasSignature && vres.byteRangeValid && vres.cryptographicValid) ? MB_ICONINFORMATION : MB_ICONWARNING;
        std::wstring msg = L"Firmante: " + Utf8ToWide(vres.signerSubjectUtf8.empty() ? "-" : vres.signerSubjectUtf8) +
            L"\nByteRange \u00EDntegro: " + (vres.byteRangeValid ? L"S\u00CD" : L"NO") +
            L"\nFirma criptogr\u00E1fica: " + (vres.cryptographicValid ? L"V\u00C1LIDA" : L"INV\u00C1LIDA") +
            L"\nCadena de sistema: " + (vres.trustedBySystemRoot ? L"De confianza" : L"Autofirmada / no ra\u00EDz") +
            L"\n\n" + Utf8ToWide(vres.diagnosticUtf8);
        MessageBoxW(st->hwndMain, msg.c_str(), L"Verificaci\u00F3n PKCS#7", icon);
        break;
    }
    case CMD_RUN_OCR: {
        if (st->doc.Pages().empty()) break;
        const std::size_t idx = (std::min)(st->activePageIndex, st->doc.Pages().size() - 1);
        RasterImageBgra32 img;
        if (!st->renderer.RenderPageOffscreen(st->doc, idx, 1400, &st->pageCache, img)) {
            st->statusBannerUtf8 = "No se pudo rasterizar p\xC3\xA1gina para OCR";
            break;
        }
        const OcrOutput ocr = WinRtOcrAdapter::RecognizeBgra32(img.width, img.height, img.pixels.data());
        if (ocr.engineAvailable && !ocr.fullTextUtf8.empty()) {
            st->doc.MutablePages()[idx].ocrTextUtf8 = ocr.fullTextUtf8;
            st->doc.BumpRevision();
            st->statusBannerUtf8 = "OCR completado (" + ocr.languageTagUtf8 + "): " + std::to_string(ocr.words.size()) + " palabras";
            MessageBoxW(st->hwndMain, Utf8ToWide(ocr.fullTextUtf8.substr(0, 1200)).c_str(), L"Texto reconocido (OCR offline)", MB_ICONINFORMATION);
        } else {
            const std::string msg = ocr.errorUtf8.empty() ? "Sin texto detectado en esta p\xC3\xA1gina." : ocr.errorUtf8;
            st->statusBannerUtf8 = msg;
            MessageBoxW(st->hwndMain, Utf8ToWide(msg).c_str(), L"Resultado OCR", MB_ICONINFORMATION);
        }
        break;
    }
    case CMD_INTEROP_MENU: {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, CMD_EXPORT_DOCX, L"Exportar a Word (.docx)...");
        AppendMenuW(menu, MF_STRING, CMD_EXPORT_PPTX, L"Exportar a PowerPoint (.pptx)...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_IMPORT_DOCX, L"Importar Word (.docx)...");
        AppendMenuW(menu, MF_STRING, CMD_IMPORT_PPTX, L"Importar PowerPoint (.pptx)...");
        POINT pt{ 540, 34 };
        for (std::size_t i = 0; i < st->buttons.size(); ++i) {
            if (st->buttons[i].id == CMD_INTEROP_MENU) {
                pt = { st->buttonRects[i].left, st->buttonRects[i].bottom };
                break;
            }
        }
        ClientToScreen(st->hwndMain, &pt);
        const UINT cmd = UiGlass::TrackTranslucentPopupMenu(menu, pt, st->hwndMain, st->glassSettings, false);
        DestroyMenu(menu);
        if (cmd != 0) ExecuteToolbarCommand(st, static_cast<WORD>(cmd));
        break;
    }
    case CMD_EXPORT_DOCX:
    case CMD_EXPORT_PPTX: {
        const bool isDocx = (id == CMD_EXPORT_DOCX);
        const COMDLG_FILTERSPEC spec = isDocx
            ? COMDLG_FILTERSPEC{ L"Word (*.docx)", L"*.docx" }
            : COMDLG_FILTERSPEC{ L"PowerPoint (*.pptx)", L"*.pptx" };
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, isDocx ? L"Exportar Word .docx" : L"Exportar PowerPoint .pptx", &spec, 1,
            isDocx ? L"docx" : L"pptx", isDocx ? L"documento.docx" : L"presentacion.pptx", outPath)) break;

        std::vector<std::vector<std::uint8_t>> snaps;
        const std::size_t maxSnapPages = (std::min<std::size_t>)(st->doc.Pages().size(), 12u);
        for (std::size_t p = 0; p < maxSnapPages; ++p) {
            RasterImageBgra32 img;
            std::vector<std::uint8_t> png;
            if (st->renderer.RenderPageOffscreen(st->doc, p, 900, &st->pageCache, img) &&
                WinRtPdfRenderer::EncodeBgra32ToPngBytes(img, png)) {
                snaps.push_back(std::move(png));
            } else {
                snaps.push_back({});
            }
        }
        std::string err;
        const bool ok = isDocx
            ? OpenXmlInterop::ExportToDocxFile(st->doc, outPath, snaps, &err)
            : OpenXmlInterop::ExportToPptxFile(st->doc, outPath, snaps, &err);
        st->statusBannerUtf8 = ok ? "Exportaci\xC3\xB3n OpenXML completada" : err;
        if (!ok) MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error OpenXML", MB_ICONERROR);
        break;
    }
    case CMD_IMPORT_DOCX:
    case CMD_IMPORT_PPTX: {
        const bool isDocx = (id == CMD_IMPORT_DOCX);
        const COMDLG_FILTERSPEC spec = isDocx
            ? COMDLG_FILTERSPEC{ L"Word (*.docx)", L"*.docx" }
            : COMDLG_FILTERSPEC{ L"PowerPoint (*.pptx)", L"*.pptx" };
        std::vector<std::wstring> paths;
        if (ShowOpenDialog(st->hwndMain, L"Importar Office", &spec, 1, paths, false)) {
            OpenDocumentFile(st, paths[0]);
        }
        break;
    }
    case CMD_PRINT: {
        PRINTDLGW pd{};
        pd.lStructSize = sizeof(pd);
        pd.hwndOwner = st->hwndMain;
        pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOSELECTION;
        pd.nMinPage = 1;
        pd.nMaxPage = static_cast<WORD>((std::max<std::size_t>)(1u, st->doc.Pages().size()));
        pd.nFromPage = 1;
        pd.nToPage = pd.nMaxPage;
        if (!PrintDlgW(&pd) || !pd.hDC) break;

        DOCINFOW di{};
        di.cbSize = sizeof(di);
        di.lpszDocName = L"CuadernU";
        if (StartDocW(pd.hDC, &di) > 0) {
            const int dpiW = GetDeviceCaps(pd.hDC, HORZRES);
            const int dpiH = GetDeviceCaps(pd.hDC, VERTRES);
            const std::size_t pStart = (pd.Flags & PD_PAGENUMS) ? (pd.nFromPage - 1) : 0;
            const std::size_t pEnd = (pd.Flags & PD_PAGENUMS) ? (std::min<std::size_t>)(pd.nToPage, st->doc.Pages().size()) : st->doc.Pages().size();
            for (std::size_t p = pStart; p < pEnd; ++p) {
                RasterImageBgra32 img;
                if (!st->renderer.RenderPageOffscreen(st->doc, p, 1600, &st->pageCache, img)) continue;
                if (StartPage(pd.hDC) > 0) {
                    BITMAPINFO bmi{};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = img.width;
                    bmi.bmiHeader.biHeight = -img.height;
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    const double scale = (std::min)(static_cast<double>(dpiW) / img.width, static_cast<double>(dpiH) / img.height);
                    const int dstW = static_cast<int>(img.width * scale);
                    const int dstH = static_cast<int>(img.height * scale);
                    const int dstX = (dpiW - dstW) / 2;
                    const int dstY = (dpiH - dstH) / 2;
                    StretchDIBits(pd.hDC, dstX, dstY, dstW, dstH, 0, 0, img.width, img.height,
                        img.pixels.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
                    EndPage(pd.hDC);
                }
            }
            EndDoc(pd.hDC);
        }
        DeleteDC(pd.hDC);
        st->statusBannerUtf8 = "Trabajo de impresi\xC3\xB3n enviado";
        break;
    }
    case CMD_MODE_PDF:
        st->doc.SetMode(DocumentMode::PaginatedPdf);
        st->doc.RebuildLayout();
        CenterCameraOnPage(st, st->activePageIndex);
        LayoutMainChildren(st);
        break;
    case CMD_MODE_CANVAS:
        st->doc.SetMode(DocumentMode::InfiniteCanvas);
        st->camera.panX = -50.0;
        st->camera.panY = -50.0;
        st->camera.zoom = 1.0;
        LayoutMainChildren(st);
        break;
    case CMD_TOOL_HAND:   st->activeTool = ToolKind::Hand; break;
    case CMD_TOOL_PEN:    st->activeTool = ToolKind::Pen; break;
    case CMD_TOOL_HL:     st->activeTool = ToolKind::Highlighter; break;
    case CMD_TOOL_ERASER: st->activeTool = ToolKind::Eraser; break;
    case CMD_TOOL_LASSO:  st->activeTool = ToolKind::Lasso; st->statusBannerUtf8 = "Lazo activo: dibuje un bucle para seleccionar objetos"; break;
    case CMD_TOOL_LASER:  st->activeTool = ToolKind::Laser; st->statusBannerUtf8 = "Puntero l\xC3\xA1ser activo (arrastre para se\xC3\xB1" "alar en vivo)"; break;
    case CMD_TOOL_RECT:   st->activeTool = ToolKind::ShapeRect; break;
    case CMD_TOOL_ARROW:  st->activeTool = ToolKind::ShapeArrow; break;
    case CMD_TOOL_TEXT:   st->activeTool = ToolKind::TextBox; break;
    case CMD_TOOL_CROP:   st->activeTool = ToolKind::Crop; break;
    case CMD_TOGGLE_SNAP:
        st->snapShapes = !st->snapShapes;
        st->statusBannerUtf8 = st->snapShapes
            ? "Hold-to-Snap activado (trazos rectos/flechas/figuras se ajustan)"
            : "Hold-to-Snap desactivado (trazo libre)";
        break;
    case CMD_STEM_MENU: {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, CMD_ADD_FORMULA,           L"Insertar F\u00F3rmula #varsub / Unidades...");
        AppendMenuW(menu, MF_STRING, CMD_ADV_MATH,              L"Laboratorio Matem\u00E1tico Avanzado (Eigen / EDO / C\u00E1lculo)...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_TABLE,             L"Insertar Tabla Reactiva (CSV con F\u00F3rmulas)...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_CODE_BLOCK,        L"Insertar Bloque de C\u00F3digo / MCU...");
        AppendMenuW(menu, MF_STRING, CMD_CYCLE_MATH_SUBMODE,    L"Alternar Modo de Sustituci\u00F3n (#varsub / #nosub / #novar)");
        AppendMenuW(menu, MF_STRING, CMD_PARAM_SLIDER,          L"Slider de Par\u00E1metro Reactivo...");
        AppendMenuW(menu, MF_STRING, CMD_CHECK_STEP,            L"Verificar Paso Algebraico...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_MATRIX_SEL,        L"Matriz / Sistema de Ecuaciones Lineales (SEL)...");
        AppendMenuW(menu, MF_STRING, CMD_OCR_CONFIRM_FORMULA,   L"OCR -> F\u00F3rmula con Tarjeta de Confirmaci\u00F3n...");
        AppendMenuW(menu, MF_STRING, CMD_FORMULA_AST_SEARCH,    L"B\u00FAsqueda Sem\u00E1ntica de F\u00F3rmulas (AST)...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_ADD_CIRCUIT,           L"Resolver Circuito MNA (DC / AC / Diodos)...");
        AppendMenuW(menu, MF_STRING, CMD_RECOGNIZE_CIRCUIT,     L"Reconocer Circuito a Mano Alzada...");
        AppendMenuW(menu, MF_STRING, CMD_CIRCUIT_TOPO_AUDIT,    L"Auditor\u00ED""a Topol\u00F3gica de Circuitos...");
        AppendMenuW(menu, MF_STRING, CMD_CONTROL_BLOCK_LAB,     L"Diagrama de Bloques de Control (PID / Routh)...");
        AppendMenuW(menu, MF_STRING, CMD_ENG_TOOLBOX,           L"Caja de Herramientas de Ingenier\u00ED""a (RC / Op-Amp / SMD)...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_TRANSIENT_LAPLACE, L"Simulaci\u00F3n Transitoria Osciloscopio + Laplace (s)...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_KMAP,              L"Simplificar Mapa de Karnaugh...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_PLOT,              L"Gr\u00E1fico 2D / Regresi\u00F3n CSV...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_PLOT_3D,           L"Superficie 3D z = f(x,y) [Rotable con Rat\u00F3n]...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_PHASOR_DIAG,       L"Diagrama Fasorial Vectorial (Rect / Polar / Exp)...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_BODE_PLOT,         L"Diagrama de Bode Semilogar\u00EDtmico |H(jw)| dB...");
        AppendMenuW(menu, MF_STRING, CMD_ADD_CALC_CARD,         L"Insertar Tarjeta CASIO Interactiva...");
        AppendMenuW(menu, MF_STRING, CMD_CASIO_REVERSE_KEYS,    L"Traducir Ecuaci\u00F3n/Matriz/Fasor -> Teclas CASIO...");
        AppendMenuW(menu, MF_STRING, CMD_COMPARE_4_CASIO,       L"Comparador de 4 Modelos CASIO...");
        AppendMenuW(menu, MF_STRING, CMD_VAR_INSPECTOR,         L"Inspector de Variables y Dimensiones F\u00EDsicas...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_EXPORT_CIRCUIT,        L"Exportar Circuito (SVG / DXF / KiCad)...");
        AppendMenuW(menu, MF_STRING, CMD_EXPORT_LAB_HTML,       L"Exportar Memoria de C\u00E1lculo / Informe Lab (.html)...");
        POINT pt{ 600, 34 };
        for (std::size_t i = 0; i < st->buttons.size() && i < st->buttonRects.size(); ++i) {
            if (st->buttons[i].id == CMD_STEM_MENU) {
                pt = { st->buttonRects[i].left, st->buttonRects[i].bottom };
                break;
            }
        }
        ClientToScreen(st->hwndMain, &pt);
        const UINT cmd = UiGlass::TrackTranslucentPopupMenu(menu, pt, st->hwndMain, st->glassSettings, false);
        DestroyMenu(menu);
        if (cmd != 0) ExecuteToolbarCommand(st, static_cast<WORD>(cmd));
        break;
    }
    case CMD_STUDY_MENU: {
        HMENU menu = CreatePopupMenu();
        const UINT examGate = st->examModeLocked ? (MF_STRING | MF_GRAYED) : MF_STRING;
        AppendMenuW(menu, examGate,  CMD_TUTOR_WHITEBOARD_OPEN, L"Pizarra Tutor Audiovisual (Voz + Lazo + PDF)...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, examGate,  CMD_CHECK_MY_WORK,       L"Revisar mi Procedimiento (Check My Work)...");
        AppendMenuW(menu, examGate,  CMD_TEACH_ME,            L"Gu\u00ED""a Socr\u00E1tica Paso a Paso (Teach Me)...");
        AppendMenuW(menu, examGate,  CMD_SUMMARIZE_NOTES,     L"Generar Resumen No Destructivo (Capa Separada)");
        AppendMenuW(menu, examGate,  CMD_LOCAL_QA,            L"Consulta Local con Citas sobre Apuntes...");
        AppendMenuW(menu, examGate,  CMD_GROUNDED_AI_CITATIONS, L"Consulta con Citas Fundamentadas (IA / BM25)...");
        AppendMenuW(menu, examGate,  CMD_STUDY_TUTOR_STEP,      L"Tutor Paso a Paso (Gu\u00ED""a Socr\u00E1tica)...");
        AppendMenuW(menu, examGate,  CMD_AUTO_QUIZ_GENERATOR,   L"Cuestionario Autocalificable Interactivo...");
        AppendMenuW(menu, examGate,  CMD_TRANSLATE_SELECTION,   L"Traducir Texto Seleccionado (ES / EN)...");
        AppendMenuW(menu, MF_STRING, CMD_AI_PROVIDERS_VAULT,    L"Proveedores de IA y Claves API (Gemini / NVIDIA / OpenAI / Anthropic / OpenCode)...");
        HMENU hSupportSub = CreatePopupMenu();
        AppendMenuW(hSupportSub, examGate,  CMD_STUDY_GEN_FLASHCARDS_AI, L"Generar Tarjetas SM-2 y Pr\u00E1""ctica con Cinta (IA / Local)");
        AppendMenuW(hSupportSub, examGate,  CMD_STUDY_CONCEPT_MAP,       L"Generar Mapa Conceptual Jer\u00E1rquico en Lienzo...");
        AppendMenuW(hSupportSub, MF_STRING, CMD_STUDY_FORMULA_GLOSSARY,  L"Compilar Hoja de F\u00F3rmulas y Glosario Autom\u00E1tico");
        AppendMenuW(hSupportSub, MF_STRING, CMD_STUDY_POMODORO,          L"Temporizador Pomodoro (25/5, 50/10)...");
        AppendMenuW(hSupportSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hSupportSub, MF_STRING, CMD_AUTO_EXTRACT_TASKS_ICS,  L"Extraer Tareas y Entregas a Base de Datos / .ics...");
        AppendMenuW(hSupportSub, MF_STRING, CMD_AUTO_POST_LECTURE,       L"Flujo Post-Audio (Transcribir + Resumir + Tareas)");
        AppendMenuW(hSupportSub, MF_STRING, CMD_STUDY_BIBLIOGRAPHY_CITE, L"Generador de Citas Bibliogr\u00E1""ficas (APA 7 / IEEE / BibTeX)...");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(hSupportSub), L"Apoyo al Estudio y Automatizaciones");
        HMENU hA11ySub = CreatePopupMenu();
        AppendMenuW(hA11ySub, MF_STRING, CMD_A11Y_CENTER,           L"Centro de Accesibilidad Universal...");
        AppendMenuW(hA11ySub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hA11ySub, MF_STRING, CMD_A11Y_TTS_SPEAK,        L"Leer P\u00E1gina o Selecci\u00F3n en Voz Alta");
        AppendMenuW(hA11ySub, MF_STRING, CMD_A11Y_TTS_STOP,         L"Detener Lectura en Voz Alta");
        AppendMenuW(hA11ySub, MF_STRING | (st->uiPrefs.accessibility.readingRulerEnabled ? MF_CHECKED : 0), CMD_A11Y_READING_RULER, L"Regla de Lectura Enfocada");
        AppendMenuW(hA11ySub, MF_STRING | (st->uiPrefs.accessibility.dyslexiaFriendlyMode ? MF_CHECKED : 0), CMD_A11Y_DYSLEXIA_MODE, L"Modo Lectura Amigable (Dislexia)");
        AppendMenuW(hA11ySub, MF_STRING, CMD_A11Y_CYCLE_TINT,       L"Rotar Tinte de Papel Anti-Fatiga (Sepia / Menta / Lavanda)");
        AppendMenuW(hA11ySub, MF_STRING, CMD_A11Y_CYCLE_COLORBLIND, L"Rotar Paleta para Daltonismo (Okabe-Ito)");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(hA11ySub), L"Accesibilidad Universal");
        AppendMenuW(menu, MF_STRING, CMD_STUDY_DECK,            L"Repasar Flashcards (SM-2)...");
        AppendMenuW(menu, MF_STRING, CMD_STUDY_PROGRESS_TIMER,  L"Cron\u00F3metro y Progreso de Estudio SM-2...");
        AppendMenuW(menu, MF_STRING, CMD_IMPORT_STUDY_DECK,     L"Importar Tarjetas de Estudio (CSV / TSV)...");
        AppendMenuW(menu, MF_STRING, CMD_FTS_SEARCH,            L"Buscar en \u00CDndice Unicode (FTS Vault)...");
        AppendMenuW(menu, MF_STRING, CMD_CONTENT_VIEWS,         L"Biblioteca y Vistas de Contenido (Content Views)...");
        AppendMenuW(menu, MF_STRING, CMD_EXAM_VARIANTS_LMS,     L"Generar Variantes de Examen y Paquete LMS...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        HMENU hAudioSub = CreatePopupMenu();
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_REC,             st->audioSession.IsContinuousRecording() ? L"Detener Grabaci\u00F3n Continua (Mic)" : L"Grabar Audio Continuo en Segundo Plano (Mic)");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_PAUSE_RESUME,    st->audioSession.IsPaused() ? L"Reanudar Grabaci\u00F3n de Audio" : L"Pausar Grabaci\u00F3n de Audio");
        AppendMenuW(hAudioSub, MF_STRING, CMD_REPLAY_TOGGLE,         st->replayActive ? L"Detener Ink Replay + Audio" : L"Reproducir Ink Replay + Altavoces");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_SPEED_CYCLE,     L"Cambiar velocidad de reproducci\u00F3n (0.75x - 2.5x)");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_SKIP_BACK,       L"Retroceder audio -10 s");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_SKIP_FWD,        L"Adelantar audio +10 s");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_NOISE_REDUCTION, L"Aplicar reducci\u00F3n de ruido");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_BOOKMARK,        L"Insertar Marcador en L\u00EDnea de Audio...");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_IMPORT,          L"Importar Archivo de Audio (.wav)...");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_EXPORT_WAV,      L"Exportar Grabaci\u00F3n (.wav)...");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_MERGE_WAV,       L"Combinar Pista de Audio Externa (.wav)...");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_TRANSCRIBE,      L"Transcribir Audio (Whisper / SAPI)");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_DIARIZE,         L"Transcripci\u00F3n Multi-Hablante (Diarizaci\u00F3n)");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_SEARCH_SEEK,     L"Buscar en Transcripci\u00F3n y Saltar...");
        AppendMenuW(hAudioSub, MF_STRING, CMD_MEETING_MINUTES_ICS,   L"Generar Acta de Reuni\u00F3n desde Calendario (.ics)...");
        AppendMenuW(hAudioSub, MF_STRING, CMD_AUDIO_EXPORT_HTML5,    L"Exportar Tutorial Sincronizado HTML5...");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(hAudioSub), L"Audio Sincronizado e Ink Replay");
        HMENU hLayerSub = CreatePopupMenu();
        AppendMenuW(hLayerSub, MF_STRING, CMD_LAYERS_MANAGER,        L"Gestor de Capas (Orden / Eliminar / Exportar)...");
        AppendMenuW(hLayerSub, MF_STRING, CMD_LAYER_ADD,             L"Nueva capa...");
        AppendMenuW(hLayerSub, MF_STRING, CMD_LAYER_NEXT,            L"Cambiar capa activa");
        AppendMenuW(hLayerSub, MF_STRING, CMD_LAYER_TOGGLE_VIS,      L"Mostrar / Ocultar capa activa");
        AppendMenuW(hLayerSub, MF_STRING, CMD_LAYER_TOGGLE_LOCK,     L"Bloquear / Desbloquear capa activa");
        AppendMenuW(hLayerSub, MF_STRING, CMD_TEACHER_PRIVATE_LAYER, L"Crear Capa Privada (Oculta en Proyecci\u00F3n/PDF)");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(hLayerSub), L"Capas del Documento");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_ADD_TAPE,            L"Insertar Study Tape (Ocultar/Revelar)...");
        AppendMenuW(menu, MF_STRING, CMD_ANCHOR_INK,          L"Anclar tinta seleccionada a objeto");
        AppendMenuW(menu, MF_STRING, CMD_ADD_INKLINK,         L"Vincular p\u00E1gina PDF \u2194 Lienzo (InkLink)");
        AppendMenuW(menu, MF_STRING, CMD_TOGGLE_COLLAPSE,     L"Colapsar / Expandir rango PDF actual");
        AppendMenuW(menu, MF_STRING, CMD_TAG_OBJECT_BOARD,    L"Etiquetar Objeto (#examen / #duda) y Ver Tablero...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, CMD_OPEN_PROJECTOR,      L"Abrir Ventana de Proyector (Segundo Monitor)");
        AppendMenuW(menu, MF_STRING, CMD_FREEZE_PROJECTOR,    st->projectorWnd.IsFrozen() ? L"Descongelar Proyector" : L"Congelar Proyector");
        AppendMenuW(menu, MF_STRING, CMD_PRESENTER_MODE,      L"Modo Presentador + Puntero L\u00E1ser");
        AppendMenuW(menu, MF_STRING, CMD_LAN_QR,              L"Difusi\u00F3n LAN Local con C\u00F3digo QR...");
        AppendMenuW(menu, MF_STRING, CMD_EXAM_PIN_LOCK,       st->examModeLocked ? L"Desbloquear Modo Examen (PIN)..." : L"Activar Modo Examen con PIN...");
        AppendMenuW(menu, MF_STRING, CMD_FOCUS_MODE,          L"Modo Concentraci\u00F3n (Sin Distracciones)");
        POINT pt{ 670, 34 };
        for (std::size_t i = 0; i < st->buttons.size() && i < st->buttonRects.size(); ++i) {
            if (st->buttons[i].id == CMD_STUDY_MENU) {
                pt = { st->buttonRects[i].left, st->buttonRects[i].bottom };
                break;
            }
        }
        ClientToScreen(st->hwndMain, &pt);
        const UINT cmd = UiGlass::TrackTranslucentPopupMenu(menu, pt, st->hwndMain, st->glassSettings, false);
        DestroyMenu(menu);
        if (cmd != 0) ExecuteToolbarCommand(st, static_cast<WORD>(cmd));
        break;
    }
    case CMD_INSERT_IMAGE: {
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        GuiDialogs::RunMultiFormatImageImport(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, st->statusBannerUtf8);
        AppendWalMutation(st, "add_image");
        break;
    }
    case CMD_COL_BLACK:  st->penColor = DocColor::RGBA(17, 24, 39, 255); break;
    case CMD_COL_BLUE:   st->penColor = DocColor::RGBA(26, 115, 232, 255); break;
    case CMD_COL_RED:    st->penColor = DocColor::RGBA(220, 38, 38, 255); break;
    case CMD_COL_GREEN:  st->penColor = DocColor::RGBA(21, 128, 61, 255); break;
    case CMD_COL_YELLOW: st->penColor = DocColor::RGBA(250, 204, 21, 255); st->activeTool = ToolKind::Highlighter; break;
    case CMD_PAPER_NEXT: {
        const PaperPattern cur = (st->doc.Mode() == DocumentMode::InfiniteCanvas)
            ? st->doc.CanvasBackground()
            : (st->doc.Pages().empty() ? PaperPattern::Plain : st->doc.Pages()[st->activePageIndex].background);
        const PaperPattern next = static_cast<PaperPattern>((static_cast<int>(cur) + 1) % 11);
        st->cmdStack.Execute(st->doc, std::make_unique<SetBackgroundCommand>(
            ObjectTarget{ st->doc.Mode() == DocumentMode::InfiniteCanvas, st->activePageIndex }, next));
        st->statusBannerUtf8 = std::string("Plantilla de papel: ") + PaperPatternLabelUtf8(next);
        break;
    }
    case CMD_ADD_PAGE: {
        PageModel blank;
        blank.background = PaperPattern::Grid;
        st->cmdStack.Execute(st->doc, std::make_unique<InsertPageCommand>(st->activePageIndex + 1, std::move(blank)));
        CenterCameraOnPage(st, st->activePageIndex + 1);
        break;
    }
    case CMD_ROTATE_PAGE:
        st->cmdStack.Execute(st->doc, std::make_unique<RotatePageCommand>(st->activePageIndex, 90));
        break;
    case CMD_DEL_PAGE:
        if (!st->cmdStack.Execute(st->doc, std::make_unique<DeletePageCommand>(st->activePageIndex))) {
            st->statusBannerUtf8 = "Debe permanecer al menos 1 p\xC3\xA1gina";
        } else if (st->activePageIndex >= st->doc.Pages().size()) {
            st->activePageIndex = st->doc.Pages().size() - 1;
        }
        break;
    case CMD_PAGE_UP:
        if (st->activePageIndex > 0) {
            if (st->cmdStack.Execute(st->doc, std::make_unique<ReorderPageCommand>(st->activePageIndex, st->activePageIndex - 1))) {
                --st->activePageIndex;
            }
        }
        break;
    case CMD_PAGE_DOWN:
        if (st->activePageIndex + 1 < st->doc.Pages().size()) {
            if (st->cmdStack.Execute(st->doc, std::make_unique<ReorderPageCommand>(st->activePageIndex, st->activePageIndex + 1))) {
                ++st->activePageIndex;
            }
        }
        break;
    case CMD_UNDO: st->cmdStack.Undo(st->doc); break;
    case CMD_REDO: st->cmdStack.Redo(st->doc); break;
    case CMD_ZOOM_OUT: {
        RECT rc{};
        GetClientRect(st->hwndViewport, &rc);
        st->camera.ZoomByFactorAtAnchor((rc.right - rc.left) * 0.5, (rc.bottom - rc.top) * 0.5, 0.85);
        break;
    }
    case CMD_ZOOM_IN: {
        RECT rc{};
        GetClientRect(st->hwndViewport, &rc);
        st->camera.ZoomByFactorAtAnchor((rc.right - rc.left) * 0.5, (rc.bottom - rc.top) * 0.5, 1.18);
        break;
    }
    case CMD_ZOOM_RESET:
        CenterCameraOnPage(st, st->activePageIndex);
        break;
    case CMD_SEARCH_NEXT: {
        wchar_t qbuf[256]{};
        if (st->hwndSearchEdit) GetWindowTextW(st->hwndSearchEdit, qbuf, 256);
        const std::string qUtf8 = WideToUtf8(qbuf);
        if (qUtf8.empty()) {
            st->searchMatches.clear();
            st->lastSearchQueryUtf8.clear();
            break;
        }
        const bool isNewQuery = (qUtf8 != st->lastSearchQueryUtf8 || st->searchMatches.empty());
        if (isNewQuery) {
            st->searchMatches = st->doc.SearchText(qUtf8, false, true);
            st->lastSearchQueryUtf8 = qUtf8;
            st->activeSearchIdx = 0;
        }
        if (st->searchMatches.empty()) {
            st->statusBannerUtf8 = "Sin resultados para: " + qUtf8;
        } else {
            if (!isNewQuery) {
                st->activeSearchIdx = (st->activeSearchIdx + 1) % st->searchMatches.size();
            }
            const auto& m = st->searchMatches[st->activeSearchIdx];
            if (m.onCanvas) {
                if (st->doc.Mode() != DocumentMode::InfiniteCanvas) {
                    st->doc.SetMode(DocumentMode::InfiniteCanvas);
                    LayoutMainChildren(st);
                }
                if (m.objectId != 0) {
                    st->selectedObjectId = m.objectId;
                    st->selectedIsCanvas = true;
                }
                CenterCameraOnWorldRect(st, m.worldBox);
            } else {
                if (st->doc.Mode() != DocumentMode::PaginatedPdf) {
                    st->doc.SetMode(DocumentMode::PaginatedPdf);
                    LayoutMainChildren(st);
                }
                if (m.objectId != 0) {
                    st->selectedObjectId = m.objectId;
                    st->selectedIsCanvas = false;
                    st->selectedPageIndex = m.pageIndex;
                }
                CenterCameraOnPage(st, m.pageIndex);
            }
            st->statusBannerUtf8 = "Coincidencia " + std::to_string(st->activeSearchIdx + 1) + "/" +
                std::to_string(st->searchMatches.size()) + " (" +
                (m.onCanvas ? "Lienzo Infinito" : ("P\xC3\xA1g. " + std::to_string(m.pageIndex + 1))) + ")";
        }
        break;
    }
    case CMD_REPLACE_TEXT: {
        wchar_t qbuf[256]{};
        if (st->hwndSearchEdit) GetWindowTextW(st->hwndSearchEdit, qbuf, 256);
        std::wstring oldW = qbuf;
        std::wstring newW;
        if (!ShowTextPrompt(st->hwndMain, L"Reemplazar / Redactar texto", L"Texto actual:", oldW, L"Nuevo texto:", &newW)) break;
        const std::string oldUtf8 = WideToUtf8(oldW);
        const std::string newUtf8 = WideToUtf8(newW);
        std::string report = "Modo cobertura/redactado + TextBox preparado.";
        if (!st->activePdfSourcePathUtf8.empty()) {
            PdfDocument pdf;
            if (pdf.LoadFromFile(Utf8ToWide(st->activePdfSourcePathUtf8))) {
                const auto cap = pdf.InspectTextReplacement(st->activePageIndex, oldUtf8);
                report = cap.diagnosticUtf8;
            }
        }
        // Always place clean opaque white background TextBox at active page center as visible redaction overlay
        AnnotationObject redactBox;
        redactBox.kind = ObjectKind::TextBox;
        redactBox.bounds = { 80.0, 120.0, 320.0, 34.0 };
        redactBox.fillColor = DocColor::RGBA(255, 255, 255, 255);
        redactBox.strokeColor = DocColor::RGBA(17, 24, 39, 255);
        redactBox.fontSizePt = 13.0;
        redactBox.textUtf8 = newUtf8;
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, st->activePageIndex }, std::move(redactBox)));
        st->statusBannerUtf8 = report;
        MessageBoxW(st->hwndMain, Utf8ToWide(report).c_str(), L"Reemplazo sem\u00E1ntico PDF", MB_ICONINFORMATION);
        break;
    }
    case CMD_ADD_FORMULA: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring expr = L"V_out = 4.7 kOhm * 2 mA | V";
        if (!ShowTextPrompt(st->hwndMain, L"F\u00F3rmula #varsub / Unidades", L"Expresi\u00F3n (soporta unidades, |, fasores r\u2220\u03B8\u00B0):", expr) || expr.empty()) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddFormulaNodeAction(st->doc, st->cmdStack, tgt, x, y, WideToUtf8(expr), "", st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_CHECK_STEP: {
        std::wstring prevW = L"(x + 2)^2";
        std::wstring nextW = L"x^2 + 4*x + 4";
        if (!ShowTextPrompt(st->hwndMain, L"Verificar Paso Algebraico", L"Paso anterior:", prevW, L"Paso actual:", &nextW)) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddFormulaNodeAction(st->doc, st->cmdStack, tgt, x, y, WideToUtf8(nextW), WideToUtf8(prevW), st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_CIRCUIT: {
        std::wstring netW = L"V1 1 0 12; R1 1 2 1000; R2 2 0 2000";
        std::wstring freqW = L"0";
        if (!ShowTextPrompt(st->hwndMain, L"Resolver Circuito MNA", L"Netlist (separar l\u00EDneas con ';', R C L V I OPAMP):", netW, L"Frecuencia Hz (0 = DC):", &freqW)) break;
        std::string netUtf8 = WideToUtf8(netW);
        for (char& ch : netUtf8) { if (ch == ';') ch = '\n'; }
        const double freqHz = _wtof(freqW.c_str());
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddCircuitNodeAction(st->doc, st->cmdStack, tgt, x, y, netUtf8, freqHz, st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_KMAP: {
        std::wstring varsW = L"4";
        std::wstring minsW = L"0,1,4,5,12,13";
        if (!ShowTextPrompt(st->hwndMain, L"Mapa de Karnaugh (Quine-McCluskey)", L"N\u00FAmero de variables (2..4):", varsW, L"Minit\u00E9rminos (ej. 0,1,4,5):", &minsW)) break;
        const int nv = std::clamp(_wtoi(varsW.c_str()), 2, 4);
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddKMapNodeAction(st->doc, st->cmdStack, tgt, x, y, nv, WideToUtf8(minsW), st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_PLOT: {
        std::wstring exprW = L"sin(x) + 0.25*x";
        std::wstring rngW = L"-6.28, 6.28";
        if (!ShowTextPrompt(st->hwndMain, L"Gr\u00E1fico 2D / Regresi\u00F3n CSV", L"Expresi\u00F3n f(x) o puntos x,y separados por ';':", exprW, L"Rango xMin, xMax:", &rngW)) break;
        std::string exprUtf8 = WideToUtf8(exprW);
        for (char& ch : exprUtf8) { if (ch == ';') ch = '\n'; }
        double xMin = -6.28, xMax = 6.28;
        const std::string rngUtf8 = WideToUtf8(rngW);
        if (const auto comma = rngUtf8.find(','); comma != std::string::npos) {
            try {
                xMin = std::stod(rngUtf8.substr(0, comma));
                xMax = std::stod(rngUtf8.substr(comma + 1));
            } catch (...) {}
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddPlotNodeAction(st->doc, st->cmdStack, tgt, x, y, exprUtf8, xMin, xMax, st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_CALC_CARD: {
        std::wstring modelW = L"fx-991CW";
        std::wstring exprW = L"4.7 kOhm * 2 mA | V";
        if (!ShowTextPrompt(st->hwndMain, L"Tarjeta CASIO Interactiva", L"Modelo CASIO:", modelW, L"Teclas|Expresi\u00F3n o C\u00E1lculo:", &exprW)) break;
        CalcBridge::CalcStepRecord rec{};
        rec.modelIdUtf8 = WideToUtf8(modelW);
        const std::string raw = WideToUtf8(exprW);
        if (const auto bar = raw.find('|'); bar != std::string::npos && raw.find('=') == std::string::npos && raw.find(" | ") == std::string::npos) {
            rec.keySeqUtf8 = raw.substr(0, bar);
            rec.expressionUtf8 = raw.substr(bar + 1);
        } else {
            rec.keySeqUtf8 = "[CALC]";
            rec.expressionUtf8 = raw;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddCalcCardAction(st->doc, st->cmdStack, tgt, x, y, rec, st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_EXPORT_CIRCUIT: {
        const ObjectTarget tgt = CurrentTarget(st);
        const AnnotationObject* cObj = nullptr;
        if (st->selectedObjectId != 0) {
            cObj = tgt.isCanvas ? st->doc.FindCanvasObject(st->selectedObjectId) : st->doc.FindPageObjectMutable(tgt.pageIndex, st->selectedObjectId);
            if (cObj && cObj->kind != ObjectKind::CircuitBlock) cObj = nullptr;
        }
        if (!cObj && (tgt.isCanvas || tgt.pageIndex < st->doc.Pages().size())) {
            const auto& vec = tgt.isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[tgt.pageIndex].objects;
            for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                if (it->kind == ObjectKind::CircuitBlock) { cObj = &(*it); break; }
            }
        }
        if (!cObj) {
            st->statusBannerUtf8 = "Inserte o seleccione primero un Circuito MNA.";
            break;
        }
        std::vector<Circuit::CircuitComponent> comps;
        std::string parseErr;
        if (!Circuit::ParseNetlistText(cObj->circuitNetlistUtf8, comps, &parseErr)) {
            st->statusBannerUtf8 = parseErr;
            break;
        }
        const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
        static constexpr COMDLG_FILTERSPEC kFilters[] = {
            { L"Esquema SVG (*.svg)", L"*.svg" },
            { L"CAD DXF (*.dxf)", L"*.dxf" },
            { L"Netlist KiCad (*.net)", L"*.net" }
        };
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, L"Exportar Circuito CAD", kFilters, 3, L"svg", L"circuito.svg", outPath)) break;
        const std::wstring ext = GetFileExtLower(outPath);
        std::string payload;
        if (ext == L".dxf") payload = Circuit::ExportCircuitToDxf(comps, sol.ok ? &sol : nullptr);
        else if (ext == L".net" || ext == L".cir") payload = Circuit::ExportCircuitToKiCadNetlist(comps, st->doc.Title());
        else payload = Circuit::ExportCircuitToSvg(comps, sol.ok ? &sol : nullptr);
        std::ofstream ofs(std::filesystem::path(outPath), std::ios::binary | std::ios::trunc);
        if (ofs) {
            ofs.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            st->statusBannerUtf8 = "Circuito exportado (" + std::to_string(payload.size()) + " bytes)";
        } else {
            st->statusBannerUtf8 = "Error escribiendo archivo de circuito";
        }
        break;
    }
    case CMD_LAYER_ADD: {
        std::wstring nameW = L"Capa de Notas";
        if (!ShowTextPrompt(st->hwndMain, L"Nueva Capa", L"Nombre de la capa:", nameW) || nameW.empty()) break;
        const std::uint64_t lid = st->doc.AddLayer(WideToUtf8(nameW));
        st->doc.SetActiveLayerId(lid);
        st->statusBannerUtf8 = "Capa activa: " + WideToUtf8(nameW);
        break;
    }
    case CMD_LAYER_NEXT: {
        const auto& layers = st->doc.Layers();
        if (layers.empty()) break;
        std::size_t idx = 0;
        for (std::size_t i = 0; i < layers.size(); ++i) {
            if (layers[i].id == st->doc.ActiveLayerId()) { idx = (i + 1) % layers.size(); break; }
        }
        st->doc.SetActiveLayerId(layers[idx].id);
        st->statusBannerUtf8 = "Capa activa: " + layers[idx].nameUtf8 +
            (layers[idx].visible ? " [Vis]" : " [Oculta]") + (layers[idx].locked ? " [Bloq]" : "");
        break;
    }
    case CMD_LAYER_TOGGLE_VIS: {
        const LayerModel* cur = st->doc.FindLayer(st->doc.ActiveLayerId());
        if (!cur) break;
        const bool nextVis = !cur->visible;
        st->doc.SetLayerProperties(cur->id, nextVis, cur->locked, cur->includeInExport, cur->opacity);
        st->statusBannerUtf8 = "Capa '" + cur->nameUtf8 + "' ahora " + (nextVis ? "visible" : "oculta");
        break;
    }
    case CMD_LAYER_TOGGLE_LOCK: {
        const LayerModel* cur = st->doc.FindLayer(st->doc.ActiveLayerId());
        if (!cur) break;
        const bool nextLock = !cur->locked;
        st->doc.SetLayerProperties(cur->id, cur->visible, nextLock, cur->includeInExport, cur->opacity);
        st->statusBannerUtf8 = "Capa '" + cur->nameUtf8 + "' ahora " + (nextLock ? "bloqueada" : "desbloqueada");
        break;
    }
    case CMD_ADD_TAPE: {
        std::wstring capW = L"F\u00F3rmula Impedancia RLC";
        std::wstring ansW = L"Z = R + j*(w*L - 1/(w*C))";
        if (!ShowTextPrompt(st->hwndMain, L"Insertar Study Tape", L"Etiqueta visible:", capW, L"Contenido oculto:", &ansW)) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddStudyTapeAction(st->doc, st->cmdStack, tgt, x, y, WideToUtf8(capW), WideToUtf8(ansW), st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ANCHOR_INK: {
        const ObjectTarget tgt = CurrentTarget(st);
        if (!tgt.isCanvas && tgt.pageIndex >= st->doc.Pages().size()) break;
        const auto& vec = tgt.isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[tgt.pageIndex].objects;
        std::uint64_t parentId = 0;
        for (const auto& o : vec) {
            if (o.kind != ObjectKind::InkStroke && o.id != st->selectedObjectId) parentId = o.id;
        }
        if (parentId == 0) {
            st->statusBannerUtf8 = "Se requiere un objeto destino (tarjeta/imagen/PDF) para anclar tinta";
            break;
        }
        int anchoredCount = 0;
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        for (std::uint64_t oid : ids) {
            if (oid == parentId) continue;
            AnnotationObject* obj = tgt.isCanvas ? st->doc.FindCanvasObjectMutable(oid) : st->doc.FindPageObjectMutable(tgt.pageIndex, oid);
            if (!obj) continue;
            AnnotationObject before = *obj;
            AnnotationObject after = before;
            after.anchorObjectId = parentId;
            if (st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)))) {
                ++anchoredCount;
            }
        }
        st->statusBannerUtf8 = anchoredCount > 0
            ? (std::to_string(anchoredCount) + " trazo(s) anclado(s) al objeto #" + std::to_string(parentId))
            : "Seleccione o encierre con Lazo un trazo para anclar";
        break;
    }
    case CMD_ADD_INKLINK: {
        InkLink link{};
        link.sourceDocPathUtf8 = st->activePdfSourcePathUtf8;
        link.sourcePageIndex0 = static_cast<int>(st->activePageIndex);
        link.sourceRectNorm = { 0.1, 0.1, 0.9, 0.4 };
        link.targetIsCanvas = true;
        link.targetPageIndex0 = 0;
        link.targetObjectId = st->selectedObjectId;
        link.labelUtf8 = "PDF P\xC3\xA1g " + std::to_string(st->activePageIndex + 1) + " <-> Lienzo";
        const std::uint64_t lid = st->doc.AddInkLink(std::move(link));
        st->statusBannerUtf8 = "InkLink #" + std::to_string(lid) + " creado (" + std::to_string(st->doc.InkLinks().size()) + " enlaces)";
        break;
    }
    case CMD_TOGGLE_COLLAPSE: {
        if (st->doc.Pages().size() < 2) {
            st->statusBannerUtf8 = "Colapso de rango requiere al menos 2 p\xC3\xA1ginas";
            break;
        }
        const int s0 = static_cast<int>(st->activePageIndex);
        const int e0 = (std::min)(static_cast<int>(st->doc.Pages().size()) - 1, s0 + 1);
        st->doc.ToggleCollapsedRange(s0, e0);
        st->statusBannerUtf8 = st->doc.IsPageCollapsed(s0)
            ? ("Rango P\xC3\xA1g " + std::to_string(s0 + 1) + "-" + std::to_string(e0 + 1) + " colapsado")
            : ("Rango P\xC3\xA1g " + std::to_string(s0 + 1) + "-" + std::to_string(e0 + 1) + " expandido");
        break;
    }
    case CMD_STUDY_DECK: {
        // Merge freshly harvested cards WITHOUT wiping existing SM-2 repetition/interval/ease history!
        const StudyDeck harvested = StudySetManager::BuildDeckFromDocument(st->doc, st->doc.Title());
        if (st->activeDeck.titleUtf8.empty()) st->activeDeck.titleUtf8 = harvested.titleUtf8;
        for (const auto& hc : harvested.cards) {
            bool exists = false;
            for (const auto& ex : st->activeDeck.cards) {
                if (ex.questionUtf8 == hc.questionUtf8 && ex.answerUtf8 == hc.answerUtf8) { exists = true; break; }
            }
            if (!exists) st->activeDeck.cards.push_back(hc);
        }
        if (st->activeDeck.cards.empty()) {
            st->statusBannerUtf8 = "No hay tarjetas StudyTape/Math/CASIO en este documento";
            MessageBoxW(st->hwndMain, L"Agregue una Study Tape, F\u00F3rmula #varsub o Tarjeta CASIO (o importe CSV/TSV) para repasar Flashcards SM-2.", L"Study Set SM-2", MB_ICONINFORMATION);
            break;
        }
        if (st->activeDeckCursor >= st->activeDeck.cards.size()) st->activeDeckCursor = 0;
        auto& card = st->activeDeck.cards[st->activeDeckCursor];
        std::wstring qScore = L"5";
        const std::uint64_t nowSec = static_cast<std::uint64_t>(std::time(nullptr));
        const std::size_t dueCnt = StudySetManager::CountDueCards(st->activeDeck, nowSec);
        const std::wstring promptTxt = Utf8ToWide(
            "Pendientes hoy: " + std::to_string(dueCnt) + "/" + std::to_string(st->activeDeck.cards.size()) +
            "\nPregunta: " + card.questionUtf8 + "\nRespuesta: " + card.answerUtf8 +
            "\nCalificaci\xC3\xB3n SM-2 (0=Olvid\xC3\xB3 .. 5=Perfecto):");
        if (ShowTextPrompt(st->hwndMain, L"Flashcards SM-2 (Historial Persistente)", promptTxt.c_str(), qScore)) {
            const int q = std::clamp(_wtoi(qScore.c_str()), 0, 5);
            StudySetManager::ReviewCardSm2(card, q, nowSec);
            st->doc.BumpRevision();
            st->statusBannerUtf8 = "SM-2 tarjeta " + std::to_string(st->activeDeckCursor + 1) + "/" +
                std::to_string(st->activeDeck.cards.size()) + ": rep #" + std::to_string(card.repetitions) +
                ", pr\xC3\xB3ximo en " + std::to_string(card.intervalDays) + " d\xC3\xAD" "a(s)";
            st->activeDeckCursor = (st->activeDeckCursor + 1) % st->activeDeck.cards.size();
        }
        break;
    }
    case CMD_FTS_SEARCH: {
        st->ftsIndex.IndexDocument("active", st->doc);
        std::wstring qW = L"ohm";
        if (!ShowTextPrompt(st->hwndMain, L"B\u00FAsqueda Unicode Vault FTS", L"T\u00E9rmino a buscar:", qW) || qW.empty()) break;
        const auto hits = st->ftsIndex.Query(WideToUtf8(qW), 24);
        if (hits.empty()) {
            st->statusBannerUtf8 = "FTS (" + std::to_string(st->ftsIndex.VocabularySize()) + " t\xC3\xA9rminos): 0 coincidencias";
        } else {
            if (hits.front().objectId != 0) st->selectedObjectId = hits.front().objectId;
            if (const auto* cObj = (hits.front().objectId != 0) ? st->doc.FindCanvasObject(hits.front().objectId) : nullptr) {
                if (st->doc.Mode() != DocumentMode::InfiniteCanvas) {
                    st->doc.SetMode(DocumentMode::InfiniteCanvas);
                    LayoutMainChildren(st);
                }
                st->selectedIsCanvas = true;
                CenterCameraOnWorldRect(st, cObj->bounds);
            } else if (st->doc.Mode() == DocumentMode::PaginatedPdf && hits.front().pageIndex < st->doc.Pages().size()) {
                st->selectedIsCanvas = false;
                st->selectedPageIndex = hits.front().pageIndex;
                CenterCameraOnPage(st, hits.front().pageIndex);
            }
            st->statusBannerUtf8 = "FTS " + std::to_string(hits.size()) + " resultado(s): " + hits.front().snippetUtf8;
        }
        break;
    }
    case CMD_REPLAY_TOGGLE: {
        if (st->replayActive) {
            st->replayActive = false;
            KillTimer(st->hwndMain, 701);
            st->audioSession.StopAudioPlayback();
            st->statusBannerUtf8 = "Ink Replay + Audio detenido";
        } else {
            const auto range = InkEngine::ComputeReplayTimelineRange(st->doc, CurrentTarget(st));
            const std::uint64_t audioDur = st->audioSession.DurationMs();
            st->replayMaxMs = (std::max)(range.second, audioDur);
            if (st->replayMaxMs < 100) st->replayMaxMs = 1500;
            st->replayCursorMs = range.first;
            st->replayActive = true;
            st->audioSession.SeekMs(st->replayCursorMs);
            if (audioDur > 0) {
                std::string playErr;
                st->audioSession.StartAudioPlayback(st->replayCursorMs, &playErr);
            } else {
                st->audioSession.SetPlaying(true);
            }
            SetTimer(st->hwndMain, 701, 45, nullptr);
            st->statusBannerUtf8 = "Ink Replay reproduciendo (0 / " + std::to_string(st->replayMaxMs) + " ms)";
        }
        break;
    }
    case CMD_AUDIO_REC: {
        if (st->audioSession.IsContinuousRecording()) {
            const auto rep = st->audioSession.StopContinuousMicRecording();
            std::vector<std::uint8_t> wav;
            if (st->audioSession.ExportWavBytes(wav) && !wav.empty()) {
                // Embed playable AudioClip widget on current canvas/page
                double x = 80.0, y = 80.0;
                NextCardPlacement(st, x, y);
                AnnotationObject clip{};
                clip.kind = ObjectKind::AudioClip;
                clip.layerId = st->doc.ActiveLayerId();
                clip.bounds = { x, y, 270.0, 56.0 };
                clip.captionUtf8 = "Audio (" + std::to_string(st->audioSession.DurationMs() / 1000) + " s)";
                clip.textUtf8 = rep.statusNoteUtf8;
                clip.inkStartTimeMs = 0;
                clip.inkEndTimeMs = st->audioSession.DurationMs();
                const std::string audioKey = st->doc.RegisterAsset(wav);
                clip.assetKey = audioKey;
                clip.audioAssetKey = audioKey;
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(clip)));
                st->statusBannerUtf8 = rep.statusNoteUtf8 + " — clic en tarjeta o Replay para reproducir";
            } else {
                st->statusBannerUtf8 = rep.statusNoteUtf8;
            }
        } else {
            st->strokeOriginEpochMs = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()).count());
            std::string recErr;
            if (st->audioSession.StartContinuousMicRecording(16000, &recErr)) {
                st->statusBannerUtf8 = "Grabando audio continuo (Win32 waveIn) — escribe mientras hablas; pulsa de nuevo para detener";
            } else {
                st->statusBannerUtf8 = "No se pudo iniciar micr\xC3\xB3" "fono: " + recErr;
            }
        }
        break;
    }
    case CMD_TOGGLE_STAB:
        if (st->penStabilization < 0.25) st->penStabilization = 0.45;
        else if (st->penStabilization < 0.70) st->penStabilization = 0.85;
        else st->penStabilization = 0.15;
        st->statusBannerUtf8 = "Estabilizaci\xC3\xB3n de tinta: " + std::to_string(static_cast<int>(std::lround(st->penStabilization * 100.0))) + "%";
        break;
    case CMD_PRESENTER_MODE:
        st->presenterMode = !st->presenterMode;
        if (st->presenterMode) st->activeTool = ToolKind::Laser;
        st->statusBannerUtf8 = st->presenterMode
            ? "Modo Presentador + Puntero L\xC3\xA1ser ACTIVADO"
            : "Modo Presentador desactivado";
        break;
    case CMD_ADD_PLOT_3D: {
        std::wstring exprW = L"sin(x^2 + y^2)";
        std::wstring domW = L"-2.5, 2.5";
        if (!ShowTextPrompt(st->hwndMain, L"Superficie 3D z = f(x, y) [CVD]", L"Expresi\u00F3n z = f(x, y):", exprW, L"Dominio min, max (ej. -2.5, 2.5):", &domW) || exprW.empty()) break;
        double dMin = -2.5, dMax = 2.5;
        const std::string domUtf8 = WideToUtf8(domW);
        if (const auto comma = domUtf8.find(','); comma != std::string::npos) {
            try {
                dMin = std::stod(domUtf8.substr(0, comma));
                dMax = std::stod(domUtf8.substr(comma + 1));
            } catch (...) {}
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddSurface3DPlotNodeAction(
            st->doc, st->cmdStack, tgt, x, y, WideToUtf8(exprW), dMin, dMax, dMin, dMax, st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_MATRIX_SEL: {
        std::wstring inW = L"2*x + y = 7; x - y = 2";
        if (!ShowTextPrompt(st->hwndMain, L"Matriz o Sistema Lineal (SEL)", L"Ej: 2*x + y = 7; x - y = 2  \u00F3  det([[2,1],[1,3]]):", inW) || inW.empty()) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddMatrixOrLinearSystemAction(
            st->doc, st->cmdStack, tgt, x, y, WideToUtf8(inW), st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_TRANSIENT_LAPLACE: {
        std::wstring netW = L"V1 1 0 10; D1 1 2; R1 2 0 1k; C1 2 0 2.2u";
        std::wstring cfgW = L"35, 60, 2";
        if (!ShowTextPrompt(st->hwndMain, L"Osciloscopio MNA Transitorio + Laplace (s)",
            L"Netlist (R, C, L, V, I, OPAMP, D):", netW, L"Duraci\u00F3n_ms, Frecuencia_Hz, Nodo_sonda:", &cfgW) || netW.empty()) break;
        std::string netUtf8 = WideToUtf8(netW);
        for (char& ch : netUtf8) { if (ch == ';') ch = '\n'; }
        double durMs = 35.0, fHz = 60.0;
        int probeN = 2;
        {
            std::string cUtf8 = WideToUtf8(cfgW);
            for (char& ch : cUtf8) { if (ch == ',') ch = ' '; }
            std::istringstream iss(cUtf8);
            iss >> durMs >> fHz >> probeN;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddTransientLaplaceCircuitAction(
            st->doc, st->cmdStack, tgt, x, y, netUtf8, durMs, fHz, probeN, st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_CYCLE_MATH_SUBMODE:
        AppActions::CycleSelectedMathSubModeAction(
            st->doc, st->cmdStack, CurrentTarget(st), st->selectedObjectId, st->statusBannerUtf8);
        break;
    case CMD_PARAM_SLIDER: {
        std::wstring varW = L"k";
        std::wstring valW = L"2.0, 0.2, 5.0";
        if (!ShowTextPrompt(st->hwndMain, L"Slider de Par\u00E1metro Reactivo",
            L"Variable (ej. k, R_load, F):", varW, L"ValorActual, M\u00EDn, M\u00E1x [Unidad]:", &valW) || varW.empty()) break;
        double curV = 2.0, minV = 0.2, maxV = 5.0;
        std::string unitSuf;
        {
            std::string s = WideToUtf8(valW);
            for (char& ch : s) { if (ch == ',') ch = ' '; }
            std::istringstream iss(s);
            iss >> curV >> minV >> maxV >> unitSuf;
        }
        AppActions::UpdateParameterSliderAction(
            st->doc, st->cmdStack, CurrentTarget(st), WideToUtf8(varW), curV, minV, maxV, unitSuf, st->statusBannerUtf8);
        break;
    }
    case CMD_OCR_CONFIRM_FORMULA: {
        // Confirmation Card workflow before converting OCR/handwriting to executable formula (Superando p.12 / F-046)
        std::string seedCandidate = "P_tot = 3 bar + 100 psi";
        if (st->doc.Mode() == DocumentMode::PaginatedPdf && st->activePageIndex < st->doc.Pages().size()) {
            const auto& ocr = st->doc.Pages()[st->activePageIndex].ocrTextUtf8;
            if (!ocr.empty()) {
                const auto nl = ocr.find('\n');
                seedCandidate = ocr.substr(0, nl == std::string::npos ? 80 : (std::min<std::size_t>)(80, nl));
            }
        }
        std::wstring candW = Utf8ToWide(seedCandidate);
        if (!ShowTextPrompt(st->hwndMain, L"Tarjeta de Confirmaci\u00F3n OCR -> F\u00F3rmula",
            L"Verifique o edite la expresi\u00F3n reconocida antes de ejecutar:", candW) || candW.empty()) {
            st->statusBannerUtf8 = "Conversi\xC3\xB3n OCR cancelada (trazo original intacto)";
            break;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddFormulaNodeAction(
            st->doc, st->cmdStack, tgt, x, y, WideToUtf8(candW), "", st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_EXPORT_LAB_HTML: {
        static constexpr COMDLG_FILTERSPEC kFilters[] = {
            { L"Memoria de C\u00E1lculo HTML5 (*.html)", L"*.html" },
            { L"Informe Markdown (*.md)", L"*.md" }
        };
        std::wstring outPath;
        if (!ShowSaveDialog(st->hwndMain, L"Exportar Memoria de C\u00E1lculo / Informe de Laboratorio",
            kFilters, 2, L"html", L"memoria_calculo_stem.html", outPath)) break;
        const std::wstring ext = GetFileExtLower(outPath);
        const std::string payload = (ext == L".md")
            ? Bridge::ExportCalculationReportMarkdown(st->doc)
            : Bridge::ExportCalculationReportHtml(st->doc);
        std::ofstream ofs(std::filesystem::path(outPath), std::ios::binary | std::ios::trunc);
        if (ofs) {
            ofs.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            st->statusBannerUtf8 = "Memoria de c\xC3\xA1" "lculo exportada (" + std::to_string(payload.size()) + " bytes)";
        } else {
            st->statusBannerUtf8 = "No se pudo guardar el informe";
        }
        break;
    }
    case CMD_TOGGLE_MINIMAP:
        st->minimapVisible = !st->minimapVisible;
        st->statusBannerUtf8 = st->minimapVisible ? "Minimapa activado" : "Minimapa oculto";
        break;
    case CMD_TOGGLE_RULER:
        if (!st->rulerState.visible) {
            RECT rc{};
            GetClientRect(st->hwndViewport, &rc);
            const DocPoint cWorld = st->camera.ScreenToWorld((rc.right - rc.left) * 0.5, (rc.bottom - rc.top) * 0.55);
            st->rulerState.centerX = cWorld.x;
            st->rulerState.centerY = cWorld.y;
            st->rulerState.angleDeg = 0.0;
            st->rulerState.visible = true;
        } else if (st->rulerState.angleDeg < 165.0) {
            st->rulerState.angleDeg += 15.0;
        } else {
            st->rulerState.visible = false;
            st->rulerState.angleDeg = 0.0;
        }
        st->statusBannerUtf8 = st->rulerState.visible
            ? ("Regla graduada activa (" + std::to_string(static_cast<int>(st->rulerState.angleDeg)) + "\xC2\xB0)")
            : "Regla graduada oculta";
        break;
    case CMD_CYCLE_ERASER_FILTER:
        if (st->eraserFilterMode == EraserFilterMode::All) {
            st->eraserFilterMode = EraserFilterMode::HighlighterOnly;
            st->statusBannerUtf8 = "Borrador Selectivo: SOLO RESALTADOR (protege tinta y f\xC3\xB3rmulas)";
        } else if (st->eraserFilterMode == EraserFilterMode::HighlighterOnly) {
            st->eraserFilterMode = EraserFilterMode::InkOnly;
            st->statusBannerUtf8 = "Borrador Selectivo: SOLO TINTA";
        } else {
            st->eraserFilterMode = EraserFilterMode::All;
            st->statusBannerUtf8 = "Borrador Selectivo: TODO";
        }
        st->activeTool = ToolKind::Eraser;
        break;
    case CMD_CLEAR_HIGHLIGHTS:
        AppActions::ClearItemsByFilterAction(
            st->doc, st->cmdStack, CurrentTarget(st), EraserFilterMode::HighlighterOnly, st->statusBannerUtf8);
        break;
    case CMD_TAG_OBJECT_BOARD: {
        if (st->selectedObjectId != 0) {
            std::wstring tagW = L"examen";
            if (ShowTextPrompt(st->hwndMain, L"Etiquetar Objeto (#tag)", L"Etiqueta (ej. examen, importante, duda):", tagW) && !tagW.empty()) {
                AppActions::AssignTagToObjectAction(
                    st->doc, st->cmdStack, CurrentTarget(st), st->selectedObjectId, WideToUtf8(tagW), st->statusBannerUtf8);
            }
        }
        const std::string board = AppActions::FormatTagSummaryBoardUtf8(st->doc);
        MessageBoxW(st->hwndMain, Utf8ToWide(board).c_str(), L"Tablero Resumen de Etiquetas (#tags)", MB_ICONINFORMATION);
        break;
    }
    case CMD_FOCUS_MODE:
        st->focusMode = !st->focusMode;
        LayoutMainChildren(st);
        st->statusBannerUtf8 = st->focusMode
            ? "Modo Concentraci\xC3\xB3n activado (panel lateral oculto)"
            : "Modo Concentraci\xC3\xB3n desactivado";
        break;
    case CMD_CASIO_REVERSE_KEYS: {
        std::wstring exprW = L"2*x + y = 7; x - y = 2";
        std::wstring modW = L"fx-991CW";
        if (!ShowTextPrompt(st->hwndMain, L"Puente Lienzo -> Teclas CASIO",
            L"Ecuaci\u00F3n, Matriz [[...]], SEL o Fasor:", exprW, L"Modelo (fx-991CW / fx-991LAX):", &modW) || exprW.empty()) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddCasioReverseBridgeAction(
            st->doc, st->cmdStack, tgt, x, y, WideToUtf8(exprW), WideToUtf8(modW), st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_PHASOR_DIAG: {
        std::wstring phW = L"V_s = 120 @ 0 V; I_s = 40 @ -36.87 A; V_L = 96 @ 53.13 V";
        if (!ShowTextPrompt(st->hwndMain, L"Generador de Diagrama Fasorial",
            L"Fasores separados por ';' (ej. V = 120 @ 0 V; I = 40 @ -36.87 A):", phW) || phW.empty()) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddPhasorDiagramNodeAction(
            st->doc, st->cmdStack, tgt, x, y, WideToUtf8(phW), st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_ADD_BODE_PLOT: {
        std::wstring netW = L"V1 1 0 1; R1 1 2 1k; C1 2 0 1u";
        std::wstring rngW = L"2, 10, 100000";
        if (!ShowTextPrompt(st->hwndMain, L"Diagrama de Bode Semilogar\u00EDtmico",
            L"Netlist AC (separado por ';'):", netW, L"NodoSalida, fMin_Hz, fMax_Hz:", &rngW) || netW.empty()) break;
        std::string netUtf8 = WideToUtf8(netW);
        for (char& ch : netUtf8) { if (ch == ';') ch = '\n'; }
        int outN = 2;
        double f0 = 10.0, f1 = 100000.0;
        {
            std::string r = WideToUtf8(rngW);
            for (char& ch : r) { if (ch == ',') ch = ' '; }
            std::istringstream iss(r);
            iss >> outN >> f0 >> f1;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const ObjectTarget tgt = CurrentTarget(st);
        const std::uint64_t newObjId = AppActions::AddBodePlotNodeAction(
            st->doc, st->cmdStack, tgt, x, y, netUtf8, outN, f0, f1, st->statusBannerUtf8);
        if (newObjId != 0) { st->selectedObjectId = newObjId; st->selectedIsCanvas = tgt.isCanvas; st->selectedPageIndex = tgt.pageIndex; }
        break;
    }
    case CMD_VAR_INSPECTOR: {
        const std::string rep = AppActions::FormatVariableInspectorUtf8(st->doc);
        MessageBoxW(st->hwndMain, Utf8ToWide(rep).c_str(), L"Inspector F\u00EDsico de Variables y Unidades SI", MB_ICONINFORMATION);
        break;
    }
    case CMD_TEACHER_PRIVATE_LAYER:
        AppActions::AddPrivateTeacherLayerAction(st->doc, st->statusBannerUtf8);
        break;
    case CMD_FILE_MENU: {
        HMENU hm = CreatePopupMenu();
        AppendMenuW(hm, MF_STRING, CMD_OPEN,                 L"Abrir documento... (Ctrl+O)");
        AppendMenuW(hm, MF_STRING, CMD_INSERT_PRINTOUT,      L"Insertar copia impresa (PDF / Word / PowerPoint)...");
        AppendMenuW(hm, MF_STRING, CMD_MIGRATE_NOTEBOOK,     L"Importar / Migrar Cuaderno (Goodnotes / Notability / OneNote / Office / .tinta)...");
        AppendMenuW(hm, MF_STRING, CMD_OPEN_VAULT_LIBRARY,   L"Biblioteca de Cuadernos y B\u00FAsqueda Global...");
        AppendMenuW(hm, MF_STRING, CMD_SAVE_CUDOC,           L"Guardar cuaderno .cudoc (Ctrl+S)");
        AppendMenuW(hm, MF_STRING, CMD_OPEN_DRAFTS,          L"Recuperar borradores...");
        AppendMenuW(hm, MF_STRING, CMD_EXPORT_PDF,           L"Exportar PDF...");
        AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
        HMENU hTabSub = CreatePopupMenu();
        AppendMenuW(hTabSub, MF_STRING, CMD_NEW_TAB,          L"Nueva pesta\u00F1""a de cuaderno (Ctrl+T)");
        AppendMenuW(hTabSub, MF_STRING, CMD_QUICK_NOTE,       L"Nota r\u00E1pida instant\u00E1nea (Ctrl+Shift+N)");
        AppendMenuW(hTabSub, MF_STRING, CMD_NEXT_TAB,         L"Cambiar a siguiente pesta\u00F1""a (Ctrl+Tab)");
        AppendMenuW(hTabSub, MF_STRING, CMD_TOGGLE_SPLIT_VIEW,     (st->splitViewActive && !st->splitViewVertical) ? L"Cerrar Vista Dividida Horizontal" : L"Vista Dividida Horizontal (Lado a Lado)");
        AppendMenuW(hTabSub, MF_STRING, CMD_TOGGLE_SPLIT_VERTICAL, (st->splitViewActive && st->splitViewVertical) ? L"Cerrar Vista Dividida Vertical" : L"Vista Dividida Vertical (Arriba / Abajo)");
        AppendMenuW(hTabSub, MF_STRING, CMD_TOGGLE_SPREAD,    st->doc.DoublePageSpread() ? L"Vista de P\u00E1gina \u00DAnica" : L"Vista Doble P\u00E1gina (Libro)");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hTabSub), L"Pesta\u00F1""as y Vista");
        HMENU hPdfSub = CreatePopupMenu();
        AppendMenuW(hPdfSub, MF_STRING, CMD_MERGE_PDF,            L"Combinar PDFs...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_SPLIT_PDF,            L"Dividir / Extraer p\u00E1ginas...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_SELECTIVE_PDF_EXPORT, L"Exportar PDF Selectivo (Editable / Aplanado)...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_PDF_ANNOTATIONS_MGR,  L"Gestor de Anotaciones, Marcadores y Adjuntos PDF...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_PDF_SCROLL_DIRECTION, st->flexcilOverlay.ScrollDirection() == PdfScrollDirection::Horizontal ? L"Desplazamiento PDF: Cambiar a Vertical" : L"Desplazamiento PDF: Cambiar a Horizontal");
        AppendMenuW(hPdfSub, MF_STRING, CMD_FLEXCIL_POPUP_OVERLAY, st->flexcilOverlay.IsPopupNoteVisible() ? L"Ocultar Nota de Estudio Flotante" : L"Mostrar Nota de Estudio Flotante");
        AppendMenuW(hPdfSub, MF_STRING, CMD_FLEXCIL_GESTURE_MODE,  st->flexcilOverlay.IsGestureModeActive() ? L"Desactivar Modo Gestos PDF" : L"Activar Modo Gestos PDF (L / Recuadro / Subrayado)");
        AppendMenuW(hPdfSub, MF_STRING, CMD_SCAN_RECTIFY_TO_PDF,  L"Rectificar Foto Escaneada + OCR...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_VIEW_ATTACHMENTS,     L"Ver adjuntos incrustados en PDF...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_SIGN_PDF,             L"Firmar PDF (PKCS#7)...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_VERIFY_SIG,           L"Verificar firma digital...");
        AppendMenuW(hPdfSub, MF_STRING, CMD_RUN_OCR,              L"Reconocer texto (OCR offline)");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hPdfSub), L"Herramientas PDF");
        HMENU hOffSub = CreatePopupMenu();
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_DOCX,          L"Exportar Word (.docx)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_PPTX,          L"Exportar PowerPoint (.pptx)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_DOCX,          L"Importar Word (.docx)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_PPTX,          L"Importar PowerPoint (.pptx)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_XLSX,          L"Importar Excel (.xlsx)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_XLSX,          L"Exportar Excel (.xlsx)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_CSV,           L"Exportar Tablas a CSV...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_MARKDOWN,      L"Importar Markdown (.md)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_NOTION_PAGE,   L"Guardar p\u00E1gina de Notion...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_MARKDOWN,      L"Exportar Markdown y Resaltados (.md)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_ONENOTE_HTML,  L"Importar HTML / OneNote HTML...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_DXF,           L"Exportar Geometr\u00EDa CAD (.dxf)...");
        AppendMenuW(hOffSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_SVG,           L"Exportar p\u00E1gina a SVG vectorial...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_PAGE_IMAGE,    L"Exportar p\u00E1gina como imagen (PNG / JPEG)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_ONENOTE_HTML,  L"Exportar HTML compatible OneNote...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_ANKI_DECK,     L"Exportar tarjetas a Anki (.tsv)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_JSON_CANVAS_EXPORT,   L"Exportar JSON Canvas 1.0 (.canvas)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_JSON_CANVAS,   L"Importar JSON Canvas 1.0 (.canvas)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_EXPORT_UNIVERSAL_PKG, L"Exportar paquete portable (.cuadernu-pkg)...");
        AppendMenuW(hOffSub, MF_STRING, CMD_IMPORT_UNIVERSAL_PKG, L"Importar paquete (.cuadernu-pkg / .goodnotes / .note / .nebo)...");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hOffSub), L"Exportar e Interoperabilidad");
        HMENU hSecSub = CreatePopupMenu();
        AppendMenuW(hSecSub, MF_STRING, CMD_LOCK_NOTE_PASSPHRASE, st->permissions.IsNoteLocked() ? L"Desbloquear Nota (PBKDF2)..." : L"Bloquear Nota con Contrase\u00F1a (PBKDF2)...");
        AppendMenuW(hSecSub, MF_STRING, CMD_SHARE_LINKS_MGR,      L"Permisos y Enlaces Compartidos...");
        AppendMenuW(hSecSub, MF_STRING, CMD_SAVE_ENCRYPTED,   L"Guardar Cuaderno Cifrado (AES-256-GCM)...");
        AppendMenuW(hSecSub, MF_STRING, CMD_OPEN_ENCRYPTED,   L"Abrir Cuaderno Cifrado...");
        AppendMenuW(hSecSub, MF_STRING, CMD_BACKUP_RESTORE_MGR,   L"Respaldo Completo y Restauraci\u00F3n Individual...");
        AppendMenuW(hSecSub, MF_STRING, CMD_VERSION_HISTORY,  L"Guardar / Ver Historial de Versiones...");
        AppendMenuW(hSecSub, MF_STRING, CMD_CONFLICT_MERGE,   L"Fusionar Cuaderno Concurrente sin P\u00E9rdida...");
        AppendMenuW(hSecSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hSecSub, MF_STRING, CMD_FOLDER_SYNC_3MODES,  L"Sincronizaci\u00F3n de Carpetas (3 Modos + Respaldo)...");
        const wchar_t* syncMenuLabel = L"Configurar sincronizaci\u00F3n autom\u00E1tica...";
        if (st->uiPrefs.autoSyncEnabled) {
            syncMenuLabel = st->uiPrefs.autoSyncEncrypted && st->autoSyncPassphraseUtf8.empty()
                ? L"Reanudar sincronizaci\u00F3n cifrada..."
                : L"Desactivar sincronizaci\u00F3n autom\u00E1tica";
        }
        AppendMenuW(hSecSub, MF_STRING, CMD_AUTO_SYNC_WATCHER, syncMenuLabel);
        AppendMenuW(hSecSub, MF_STRING, CMD_STATIC_SITE_PUBLISH, L"Publicar B\u00F3veda como Sitio Web HTML5...");
        AppendMenuW(hSecSub, MF_STRING, CMD_TOGGLE_LAN_COLLAB_SERVER, st->lanServer.IsRunning() ? L"Detener Servidor Colaborativo LAN" : L"Iniciar Servidor Colaborativo LAN...");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hSecSub), L"Seguridad, Sincronizaci\u00F3n y Publicaci\u00F3n");
        AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
        HMENU hMediaSub = CreatePopupMenu();
        AppendMenuW(hMediaSub, MF_STRING, CMD_INSERT_LINK_CARD,      L"Insertar enlace interactivo (Web / P\u00E1gina)...");
        AppendMenuW(hMediaSub, MF_STRING, CMD_INSERT_GIF,            L"Insertar GIF animado...");
        AppendMenuW(hMediaSub, MF_STRING, CMD_INSERT_VIDEO_CLIP,     L"Insertar video reproducible...");
        AppendMenuW(hMediaSub, MF_STRING, CMD_INSERT_HTML_EMBED,     L"Insertar bloque HTML / CSS / JS animado...");
        AppendMenuW(hMediaSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMediaSub, MF_STRING, CMD_INSERT_IMAGE,          L"Insertar imagen (PNG/JPG/GIF/SVG/WEBP/TIFF)...");
        AppendMenuW(hMediaSub, MF_STRING, CMD_GLOBAL_SCREEN_CAPTURE, L"Capturar recorte de pantalla del escritorio");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hMediaSub), L"Insertar Multimedia (Enlace / Video / GIF / HTML)");
        AppendMenuW(hm, MF_STRING, CMD_PRINT,        L"Imprimir (Ctrl+P)");
        POINT pt{ DpiSc(54, st->currentDpi), DpiSc(kTopChromeH, st->currentDpi) };
        ClientToScreen(st->hwndMain, &pt);
        const UINT chosen = UiGlass::TrackTranslucentPopupMenu(hm, pt, st->hwndMain, st->glassSettings, false);
        DestroyMenu(hm);
        if (chosen != 0) { ExecuteToolbarCommand(st, static_cast<WORD>(chosen)); return; }
        break;
    }
    case CMD_TOGGLE_SIDEBAR:
        st->focusMode = false;
        st->sidebarVisible = !st->sidebarVisible;
        LayoutMainChildren(st);
        break;
    case CMD_TOGGLE_INSPECTOR:
        st->focusMode = false;
        st->inspectorVisible = !st->inspectorVisible;
        LayoutMainChildren(st);
        break;
    case CMD_TOGGLE_ACADEMIC:
        st->focusMode = false;
        if (st->inspectorVisible && st->inspectorTab == 5) {
            st->inspectorVisible = false;
        } else {
            st->inspectorTab = 5;
            st->inspectorScrollY = 0;
            st->inspectorVisible = true;
        }
        LayoutMainChildren(st);
        break;
    case CMD_TOGGLE_SEARCH:
        st->searchVisible = !st->searchVisible;
        LayoutMainChildren(st);
        if (st->searchVisible && st->hwndSearchEdit) SetFocus(st->hwndSearchEdit);
        break;
    case CMD_TOGGLE_APPEARANCE:
        st->appearancePopoverOpen = !st->appearancePopoverOpen;
        break;
    case CMD_MORE_OVERFLOW: {
        HMENU hm = CreatePopupMenu();
        HMENU hInkSub = CreatePopupMenu();
        AppendMenuW(hInkSub, MF_STRING, CMD_CYCLE_PEN_STYLE,       L"Cambiar estilo de l\u00E1piz (Bol\u00EDgrafo / Pluma / Pincel / L\u00E1piz)");
        HMENU hWidthSub = CreatePopupMenu();
        AppendMenuW(hWidthSub, MF_STRING | (st->penWidth < 2.0 ? MF_CHECKED : 0),  CMD_PEN_WIDTH_FINE,   L"Fino (1.5 pt)");
        AppendMenuW(hWidthSub, MF_STRING | (st->penWidth >= 2.0 && st->penWidth < 3.8 ? MF_CHECKED : 0), CMD_PEN_WIDTH_MED, L"Medio (2.6 pt)");
        AppendMenuW(hWidthSub, MF_STRING | (st->penWidth >= 3.8 && st->penWidth < 6.5 ? MF_CHECKED : 0), CMD_PEN_WIDTH_BOLD, L"Grueso (4.8 pt)");
        AppendMenuW(hWidthSub, MF_STRING | (st->penWidth >= 6.5 ? MF_CHECKED : 0), CMD_PEN_WIDTH_MARKER, L"Marcador (8.0 pt)");
        AppendMenuW(hInkSub, MF_POPUP, reinterpret_cast<UINT_PTR>(hWidthSub), L"Grosor de trazo");
        HMENU hOpSub = CreatePopupMenu();
        AppendMenuW(hOpSub, MF_STRING | (st->penOpacity > 0.90 ? MF_CHECKED : 0), CMD_PEN_OPACITY_100, L"100% (Opaco)");
        AppendMenuW(hOpSub, MF_STRING | (st->penOpacity <= 0.90 && st->penOpacity > 0.62 ? MF_CHECKED : 0), CMD_PEN_OPACITY_75, L"75%");
        AppendMenuW(hOpSub, MF_STRING | (st->penOpacity <= 0.62 && st->penOpacity > 0.40 ? MF_CHECKED : 0), CMD_PEN_OPACITY_50, L"50%");
        AppendMenuW(hOpSub, MF_STRING | (st->penOpacity <= 0.40 ? MF_CHECKED : 0), CMD_PEN_OPACITY_30, L"30% (Transl\u00FAcido)");
        AppendMenuW(hInkSub, MF_POPUP, reinterpret_cast<UINT_PTR>(hOpSub), L"Opacidad de tinta");
        AppendMenuW(hInkSub, MF_STRING, CMD_CUSTOM_INK_COLOR,      L"Color personalizado de tinta...");
        AppendMenuW(hInkSub, MF_STRING, CMD_TOGGLE_PARTIAL_ERASER, st->eraserActionMode == EraserActionMode::PartialPixel ? L"Borrador: Cambiar a Trazo Completo" : L"Borrador: Cambiar a Precisi\u00F3n / P\u00EDxel");
        AppendMenuW(hInkSub, MF_STRING, CMD_CYCLE_ERASER_FILTER,   L"Filtro de borrador (Todo / Solo Resaltador / Solo Tinta)");
        AppendMenuW(hInkSub, MF_STRING, CMD_CLEAR_HIGHLIGHTS,      L"Limpiar todos los resaltados");
        AppendMenuW(hInkSub, MF_STRING, CMD_TOGGLE_STAB,           L"Estabilizaci\u00F3n de trazo");
        AppendMenuW(hInkSub, MF_STRING, CMD_TOGGLE_SNAP_GRID,      st->snapToGrid ? L"Desactivar ajuste a cuadr\u00EDcula" : L"Activar ajuste a cuadr\u00EDcula");
        AppendMenuW(hInkSub, MF_STRING, CMD_CYCLE_GEOMETRY_TOOL,   L"Herramienta de geometr\u00ED""a (Regla / Transportador / Comp\u00E1s)");
        AppendMenuW(hInkSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hInkSub, MF_STRING, CMD_COPY_STROKE_STYLE,     L"Copiar estilo del trazo seleccionado");
        AppendMenuW(hInkSub, MF_STRING, CMD_RESTYLE_INK,           L"Enderezar y estilizar tinta seleccionada");
        AppendMenuW(hInkSub, MF_STRING, CMD_HTR_TO_TEXT,           L"Convertir escritura a texto editable (InkAnalyzer / HTR)");
        AppendMenuW(hInkSub, MF_STRING, CMD_HTR_TO_MATH,           L"Convertir escritura a ecuaci\u00F3n 2D (HTR)");
        AppendMenuW(hInkSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hInkSub, MF_STRING, CMD_ZOOM_WRITING_BOX,      st->zoomWritingBox.active ? L"Cerrar Caja de Zoom para Escritura" : L"Caja de Zoom para Escritura (Auto-avance)");
        AppendMenuW(hInkSub, MF_STRING, CMD_HANDWRITING_REFLOW,    L"Reflujo continuo de escritura manuscrita");
        AppendMenuW(hInkSub, MF_STRING, CMD_PASTE_AS_HANDWRITING,  L"Pegar texto como caligraf\u00EDa vectorial...");
        AppendMenuW(hInkSub, MF_STRING, CMD_HANDWRITING_SPELLING,     L"Revisar ortograf\u00EDa manuscrita");
        AppendMenuW(hInkSub, MF_STRING, CMD_STYLUS_DOUBLE_TAP_ERASER, L"Alternar l\u00E1piz / borrador (doble toque)");
        AppendMenuW(hInkSub, MF_STRING, CMD_INK_WORD_SPACE_GESTURE,   L"Insertar espacio / unir palabras manuscritas");
        AppendMenuW(hInkSub, MF_STRING, CMD_REACTIVE_MATH_EVAL,       L"Evaluar matem\u00E1ticas reactivas (=)");
        AppendMenuW(hInkSub, MF_STRING, CMD_CHEMISTRY_BALANCER,    L"Balanceador Qu\u00EDmico 2D (118 elementos)...");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hInkSub), L"Tinta y Escritura");

        HMENU hObjSub = CreatePopupMenu();
        AppendMenuW(hObjSub, MF_STRING, CMD_CLIPBOARD_PASTE,        L"Pegar imagen o texto (Ctrl+V)");
        AppendMenuW(hObjSub, MF_STRING, CMD_GROUP_OBJECTS,          L"Agrupar selecci\u00F3n (Ctrl+G)");
        AppendMenuW(hObjSub, MF_STRING, CMD_UNGROUP_OBJECTS,        L"Desagrupar selecci\u00F3n (Ctrl+Shift+G)");
        AppendMenuW(hObjSub, MF_STRING, CMD_DUPLICATE_OBJECTS,      L"Duplicar selecci\u00F3n (Ctrl+D)");
        AppendMenuW(hObjSub, MF_STRING, CMD_ADD_CONNECTOR,          L"Insertar conector magn\u00E9tico");
        AppendMenuW(hObjSub, MF_STRING, CMD_ADD_REDACTION,          L"Insertar censura PDF permanente");
        AppendMenuW(hObjSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hObjSub, MF_STRING, CMD_TEXT_BOLD,              L"Texto: Negrita (Ctrl+B)");
        AppendMenuW(hObjSub, MF_STRING, CMD_TEXT_ITALIC,            L"Texto: Cursiva (Ctrl+I)");
        AppendMenuW(hObjSub, MF_STRING, CMD_TEXT_UNDERLINE,         L"Texto: Subrayado (Ctrl+U)");
        AppendMenuW(hObjSub, MF_STRING, CMD_TEXT_BULLET_LIST,       L"Texto: Alternar vi\u00F1""etas (\u2022)");
        AppendMenuW(hObjSub, MF_STRING, CMD_TEXT_CHECKBOX_TOGGLE,   L"Texto: Alternar casilla (\u2610 / \u2611)");
        AppendMenuW(hObjSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hObjSub, MF_STRING, CMD_IMG_ENHANCE_WHITEBOARD, L"Imagen: Mejorar contraste de pizarra");
        AppendMenuW(hObjSub, MF_STRING, CMD_IMG_REMOVE_BG,          L"Imagen: Quitar fondo blanco");
        AppendMenuW(hObjSub, MF_STRING, CMD_IMG_ROTATE_90,          L"Imagen: Rotar +90\u00B0");
        AppendMenuW(hObjSub, MF_STRING, CMD_IMG_TOGGLE_OPACITY,     L"Imagen: Alternar opacidad 55% / 100%");
        AppendMenuW(hObjSub, MF_STRING, CMD_IMG_CROP,               L"Imagen: Recortar m\u00E1rgenes");
        AppendMenuW(hObjSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hObjSub, MF_STRING, CMD_STICKER_OPAMP,          L"Elemento: Amplificador Op-Amp");
        AppendMenuW(hObjSub, MF_STRING, CMD_STICKER_RESISTOR,       L"Elemento: Resistencia vectorial");
        AppendMenuW(hObjSub, MF_STRING, CMD_STICKER_AXES,           L"Elemento: Ejes XY");
        AppendMenuW(hObjSub, MF_STRING, CMD_STICKER_NOTE,           L"Elemento: Nota adhesiva");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hObjSub), L"Objetos, Texto e Im\u00E1genes");

        HMENU hPaperSub = CreatePopupMenu();
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_NEXT,       L"Siguiente plantilla de papel");
        AppendMenuW(hPaperSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_ISOMETRIC,  L"Papel Isom\u00E9trico 30\u00B0");
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_LOGLOG,     L"Papel Log-Log");
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_POLAR,      L"Papel Polar");
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_SMITH,      L"Carta de Smith");
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_CORNELL,    L"Plantilla Cornell");
        AppendMenuW(hPaperSub, MF_STRING, CMD_PAPER_MUSIC,      L"Pentagrama musical");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hPaperSub), L"Plantillas de Papel");

        AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hm, MF_STRING, CMD_STEM_MENU,       L"Ingenier\u00EDa STEM (+F\u00F3rmula / Circuito / Gr\u00E1fica)...");
        AppendMenuW(hm, MF_STRING, CMD_STUDY_MENU,      L"Estudio SM-2 / Capas / Audio...");
        AppendMenuW(hm, MF_STRING, CMD_A11Y_CENTER,     L"Centro de Accesibilidad Universal...");
        AppendMenuW(hm, MF_STRING, CMD_AI_PROVIDERS_VAULT, L"Proveedores de IA y Claves API...");
        AppendMenuW(hm, MF_STRING, CMD_TOGGLE_ACADEMIC, L"Panel Acad\u00E9mico (TEC Digital y HorariosTEC)");
        AppendMenuW(hm, MF_STRING, CMD_ACADEMIC_SYNC_TEC, L"Centro Acad\u00E9mico TEC Digital... (Ctrl+Shift+T)");
        AppendMenuW(hm, MF_STRING, CMD_COMMAND_PALETTE, L"Paleta de comandos... (Ctrl+K)");

        HMENU hKnowSub = CreatePopupMenu();
        AppendMenuW(hKnowSub, MF_STRING, CMD_OPEN_DAILY_NOTE,         L"Abrir o crear Nota Diaria (hoy)");
        AppendMenuW(hKnowSub, MF_STRING, CMD_TOGGLE_GRAPH_VIEW,       L"Mostrar / Ocultar Vista de Grafo");
        AppendMenuW(hKnowSub, MF_STRING, CMD_GRAPH_LOCAL_FILTER,      L"Grafo Local y Filtros de Nodos...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_VIEW_BACKLINKS,          L"Panel de Backlinks, Salientes, Menciones y Enlaces Rotos...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_VAULT_SEARCH_RENAME,     L"Buscar en B\u00F3veda, Filtros y Renombrado Propagado...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_DOCUMENT_OUTLINE,        L"Esquema de Encabezados y Contador de Palabras...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_NOTE_COVER_PAPER,        L"Cambiar Portada y Papel del Cuaderno...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_NEW_TEMPLATE_NOTE,       L"Crear nota desde plantilla...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_EDIT_PROPERTIES,         L"Propiedades de nota (Frontmatter)...");
        AppendMenuW(hKnowSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hKnowSub, MF_STRING, CMD_INSERT_SYNCED_BLOCK,     L"Insertar bloque sincronizado...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_SAVE_REUSABLE_SNIPPET,   L"Guardar selecci\u00F3n en biblioteca de elementos...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_INSERT_REUSABLE_SNIPPET, L"Gestor de elementos y pegatinas guardadas...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_EXPORT_STICKY_NOTES,     L"Exportar notas adhesivas...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_TOGGLE_READING_VIEW,     L"Alternar Vista de Lectura / Edici\u00F3n");
        AppendMenuW(hKnowSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hKnowSub, MF_STRING, CMD_INSERT_STRUCTURED_DB,    L"Insertar Base de Datos Estructurada (.base)");
        AppendMenuW(hKnowSub, MF_STRING, CMD_DB_EDITOR_MGR,           L"Editor de Base de Datos (Columnas / F\u00F3rmulas / Vistas)...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_DB_SUBMIT_FORM,          L"Formulario Conectado a Base de Datos...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_DB_ACTION_BUTTON,        L"Ejecutar Bot\u00F3n de Acci\u00F3n (Automatizaci\u00F3n)");
        AppendMenuW(hKnowSub, MF_STRING, CMD_TOGGLE_DB_PANEL,         st->dbPanelVisible ? L"Ocultar Panel Visual de Base de Datos" : L"Mostrar Panel Visual de Base de Datos (9 vistas)");
        AppendMenuW(hKnowSub, MF_STRING, CMD_CYCLE_DB_VIEW,           L"Siguiente Vista de Base (Tabla / Lista / Galer\u00EDa / Kanban / Calendario / Mapa)");
        AppendMenuW(hKnowSub, MF_STRING, CMD_AUTOMATION_RULES,        L"Reglas de Automatizaci\u00F3n de Base...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_EXPORT_BASE_FILE,        L"Exportar archivo .base...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_IMPORT_BASE_FILE,        L"Importar archivo .base...");
        AppendMenuW(hKnowSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hKnowSub, MF_STRING, CMD_EXPORT_KEYMAP_LOAD_THEME, L"Exportar Atajos / Cargar Perfil de Tema (.json)...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_WEB_CLIPPER_CAPTURE,     L"Capturar P\u00E1gina Web (Web Clipper)...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_ANCHORED_COMMENTS,       L"Comentarios Anclados y @Menciones...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_TOGGLE_COMMENTS_PANEL,   st->commentsPanelVisible ? L"Ocultar Panel de Comentarios" : L"Mostrar Panel de Comentarios");
        AppendMenuW(hKnowSub, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hKnowSub, MF_STRING, CMD_CUSTOM_SHORTCUTS,        L"Personalizar Atajos de Teclado...");
        AppendMenuW(hKnowSub, MF_STRING, CMD_TOOLBAR_ORIENTATION,     L"Cambiar orientaci\u00F3n de barra (Superior / Izquierda / Derecha)");
        AppendMenuW(hKnowSub, MF_STRING, CMD_EDIT_CUSTOM_CSS,         L"Editar Fragmentos CSS Personalizados...");
        AppendMenuW(hm, MF_POPUP, reinterpret_cast<UINT_PTR>(hKnowSub), L"Conocimiento, Grafo y Plantillas");
        AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hm, MF_STRING, CMD_ADD_PAGE,        L"A\u00F1adir p\u00E1gina");
        AppendMenuW(hm, MF_STRING, CMD_ROTATE_PAGE,     L"Rotar p\u00E1gina 90\u00B0");
        AppendMenuW(hm, MF_STRING, CMD_DEL_PAGE,        L"Eliminar p\u00E1gina actual");
        AppendMenuW(hm, MF_STRING, CMD_UNDO,            L"Deshacer (Ctrl+Z)");
        AppendMenuW(hm, MF_STRING, CMD_REDO,            L"Rehacer (Ctrl+Y)");
        AppendMenuW(hm, MF_STRING, CMD_TOGGLE_RULER,    L"Regla graduada");
        AppendMenuW(hm, MF_STRING, CMD_TOGGLE_MINIMAP,  L"Minimapa de lienzo");
        AppendMenuW(hm, MF_STRING, CMD_FOCUS_MODE,      L"Modo Concentraci\u00F3n");
        AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hm, MF_STRING | (!st->renderer.IsHardwareAccelerationEnabled() ? MF_CHECKED : 0),
            CMD_TOGGLE_GPU_ACCEL,
            st->renderer.IsHardwareAccelerationEnabled()
                ? L"Cambiar a Modo Ultra Eficiente (0% GPU)"
                : L"Modo Ultra Eficiente activo (0% GPU) \u2014 Activar GPU");
        AppendMenuW(hm, MF_STRING, CMD_MEDIA_TRIM_MEMORY, L"Liberar cach\u00E9 y RAM ahora");
        RECT rc{};
        GetClientRect(st->hwndMain, &rc);
        POINT pt{ rc.right - DpiSc(16, st->currentDpi), DpiSc(kTopChromeH, st->currentDpi) };
        ClientToScreen(st->hwndMain, &pt);
        const UINT chosen = UiGlass::TrackTranslucentPopupMenu(hm, pt, st->hwndMain, st->glassSettings, true);
        DestroyMenu(hm);
        if (chosen != 0) { ExecuteToolbarCommand(st, static_cast<WORD>(chosen)); return; }
        break;
    }
    case CMD_ACADEMIC_SYNC_TEC:
    case CMD_ACADEMIC_SOLVE_SCHEDULE:
    case CMD_ACADEMIC_SEARCH_MATERIALS:
    case CMD_ACADEMIC_MALLA_TEC:
    case CMD_ACADEMIC_NOTAS_TEC:
    case CMD_ACADEMIC_CALENDAR_TEC:
    case CMD_ACADEMIC_EVALS_TEC:
    case CMD_ACADEMIC_PLAGIO_TEC: {
#if defined(DOC_STUDIO_HAS_ACADEMIC)
        if (!st->tecStore) {
            st->tecStore = std::make_unique<DocStudio::TecDigital::TecAcademicStore>();
            st->tecStore->LoadSnapshotFromDisk(nullptr);
            st->tecStore->TryRestoreVaultSession();
        }
        auto section = DocStudio::TecDigitalUi::HubSection::Resumen;
        int subView = 0;
        if (id == CMD_ACADEMIC_SOLVE_SCHEDULE) {
            section = DocStudio::TecDigitalUi::HubSection::HorariosVivo;
        } else if (id == CMD_ACADEMIC_SEARCH_MATERIALS) {
            section = DocStudio::TecDigitalUi::HubSection::BibliotecaOffline;
        } else if (id == CMD_ACADEMIC_MALLA_TEC) {
            section = DocStudio::TecDigitalUi::HubSection::ExpedienteYMalla;
            subView = 1;
        } else if (id == CMD_ACADEMIC_NOTAS_TEC) {
            section = DocStudio::TecDigitalUi::HubSection::ExpedienteYMalla;
            subView = 0;
        } else if (id == CMD_ACADEMIC_CALENDAR_TEC) {
            section = DocStudio::TecDigitalUi::HubSection::Calendario;
        } else if (id == CMD_ACADEMIC_EVALS_TEC) {
            section = DocStudio::TecDigitalUi::HubSection::Cursos;
            subView = 2;
        } else if (id == CMD_ACADEMIC_PLAGIO_TEC) {
            section = DocStudio::TecDigitalUi::HubSection::TramitesYPerfil;
            subView = 0;
        }

        DocStudio::TecDigitalUi::SendCourseToGeminiFn sendGeminiFn = nullptr;
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        sendGeminiFn = [st](const academic::tec_digital::CommunityInfo& course,
                            const academic::tec_digital::CourseOverviewSnapshot* overview,
                            const std::vector<academic::tec_digital::FileRecord>& downloadedFiles,
                            std::string& outMsg) -> bool {
            if (!st->gnStoreLoaded) {
                st->gnStore.LoadAll(nullptr);
                st->gnStoreLoaded = true;
            }
            const auto cands = DocStudio::TecDigital::TecAcademicStore::BuildGeminiNotebookCandidatesForCourse(
                course, overview, downloadedFiles, st->tecStore->OfflineFilesRootUtf8());
            if (cands.empty()) {
                outMsg = "Sincronice archivos o portada del curso antes de enviar a Gemini Notebook.";
                return false;
            }
            const std::string nbKey = "tec:" + (course.slug.empty() ? course.name : course.slug);
            const std::string nbTitle = "TEC · " + (course.name.empty() ? course.slug : course.name);
            const auto plan = st->gnStore.ComputeIncrementalSyncPlan(nbKey, nbTitle, cands);
            if (st->gnStore.Tokens().HasAnyCredential()) {
                GeminiNotebook::GeminiNotebookRestClient client(st->gnStore);
                const auto sum = client.ExecuteSyncPlan(plan);
                outMsg = sum.ok
                    ? ("Gemini Notebook: " + std::to_string(sum.uploadedCount + sum.versionedCount) +
                       " fuente(s) enviada(s), " + std::to_string(sum.skippedCount) + " sin cambios.")
                    : ("Gemini Notebook: " + sum.errorUtf8);
                return sum.ok;
            }
            outMsg = "Plan preparado (" + std::to_string(cands.size()) +
                     " fuentes). Conecte su cuenta en Gemini Notebook para sincronizar.";
            GeminiNotebookUi::ShowGeminiNotebookHubDialog(st->hwndMain, st->gnStore, &st->doc, st->statusBannerUtf8);
            return true;
        };
#endif

        auto addSm2Fn = [st](const std::vector<DocStudio::TecDigital::Sm2FlashcardDraft>& drafts) -> std::size_t {
            std::size_t added = 0;
            for (const auto& d : drafts) {
                FlashcardItem item{};
                item.id = static_cast<std::uint64_t>(st->activeDeck.cards.size() + 1);
                item.questionUtf8 = d.frontUtf8;
                item.answerUtf8 = d.backUtf8 + (d.tagUtf8.empty() ? "" : (" [" + d.tagUtf8 + "]"));
                item.easeFactor = 2.5;
                st->activeDeck.cards.push_back(std::move(item));
                ++added;
            }
            return added;
        };

        auto searchActiveDocFn = [st](const std::string& q) -> std::vector<DocStudio::TecDigitalUi::ActiveDocSearchHit> {
            st->ftsIndex.IndexDocument("active", st->doc);
            const auto hits = st->ftsIndex.Query(q);
            std::vector<DocStudio::TecDigitalUi::ActiveDocSearchHit> out;
            for (const auto& h : hits) {
                DocStudio::TecDigitalUi::ActiveDocSearchHit a{};
                a.titleUtf8 = st->doc.Title().empty() ? "Cuaderno activo" : st->doc.Title();
                a.snippetUtf8 = h.snippetUtf8;
                a.pageIndex = h.pageIndex;
                out.push_back(std::move(a));
            }
            return out;
        };

        const bool mutated = DocStudio::TecDigitalUi::RunTecDigitalCenterDialog(
            st->hwndMain,
            st->doc,
            *st->tecStore,
            section,
            st->statusBannerUtf8,
            [st]() {
                st->pageCache.InvalidateAll(++st->asyncGen);
                InvalidateViews(st);
            },
            subView,
            std::move(sendGeminiFn),
            std::move(addSm2Fn),
            std::move(searchActiveDocFn),
            &st->glassSettings);

        const auto snap = st->tecStore->Snapshot();
        st->academicStatusUtf8 = snap.sessionAuthenticated
            ? (std::to_string(snap.communities.size()) + " cursos y comunidades sincronizados")
            : (snap.communities.empty()
                   ? "Conecta tu cuenta para ver cursos y entregas"
                   : (std::to_string(snap.communities.size()) + " cursos guardados sin conexi\xC3\xB3n"));
        if (st->statusBannerUtf8.empty()) {
            st->statusBannerUtf8 = st->academicStatusUtf8;
        }
        if (mutated) {
            st->pageCache.InvalidateAll(++st->asyncGen);
            InvalidateViews(st);
        }
#else
        st->academicStatusUtf8 = "Módulo académico TEC Digital no habilitado en esta compilación";
        st->statusBannerUtf8 = st->academicStatusUtf8;
#endif
        break;
    }
    case CMD_MIGRATE_NOTEBOOK: {
        if (GuiDialogs::RunNotebookMigrationDialog(st->hwndMain, st->doc, st->audioSession, st->vault, st->statusBannerUtf8)) {
            st->cmdStack.Clear();
            st->pageCache.InvalidateAll(++st->asyncGen);
            st->currentDocPath.clear();
            CenterCameraOnPage(st, 0);
            LayoutMainChildren(st);
        }
        break;
    }
    case CMD_OPEN_VAULT_LIBRARY:
        GuiDialogs::RunVaultLibraryManager(st->hwndMain, st->doc, st->vault, st->ftsIndex, st->currentDocPath, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    case CMD_SAVE_ENCRYPTED:
        GuiDialogs::RunSecurityOrLanFlow(st->hwndMain, st->doc, 0, MakePasswordPromptAdapter(st), st->statusBannerUtf8);
        break;
    case CMD_OPEN_ENCRYPTED:
        if (GuiDialogs::RunSecurityOrLanFlow(st->hwndMain, st->doc, 1, MakePasswordPromptAdapter(st), st->statusBannerUtf8)) {
            st->cmdStack.Clear();
            st->pageCache.InvalidateAll(++st->asyncGen);
            CenterCameraOnPage(st, 0);
            LayoutMainChildren(st);
        }
        break;
    case CMD_VERSION_HISTORY:
        GuiDialogs::RunVersionHistoryOrMerge(st->hwndMain, st->doc, st->versionHistory, st->activePageIndex, false, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    case CMD_CONFLICT_MERGE:
        GuiDialogs::RunVersionHistoryOrMerge(st->hwndMain, st->doc, st->versionHistory, st->activePageIndex, true, MakePromptAdapter(st), st->statusBannerUtf8);
        st->pageCache.InvalidateAll(++st->asyncGen);
        break;
    case CMD_VIEW_ATTACHMENTS: {
        std::ostringstream os;
        os << "Adjuntos incrustados en el documento (" << st->doc.Attachments().size() << "):\r\n\r\n";
        for (const auto& a : st->doc.Attachments()) {
            os << "  - " << a.filenameUtf8 << " (" << a.mimeTypeUtf8 << ", " << a.sizeBytes << " bytes)\r\n";
        }
        if (st->doc.Attachments().empty()) os << "  (Este documento no contiene archivos adjuntos incrustados.)";
        GuiDialogs::ShowReportDialog(st->hwndMain, L"Adjuntos del Documento", Utf8ToWide(os.str()));
        break;
    }
    case CMD_QUICK_NOTE:
        AddNewDocTab(st, true);
        break;
    case CMD_NEW_TAB:
        AddNewDocTab(st, false);
        break;
    case CMD_NEXT_TAB:
        if (st->tabs.size() > 1) SwitchToDocTab(st, st->activeTabIdx + 1);
        else st->statusBannerUtf8 = "Abra otra pesta\xC3\xB1""a con Ctrl+T o Nota R\xC3\xA1pida";
        break;
    case CMD_TOGGLE_SPREAD:
        st->doc.SetDoublePageSpread(!st->doc.DoublePageSpread());
        st->statusBannerUtf8 = st->doc.DoublePageSpread() ? "Vista Doble P\xC3\xA1gina (Libro) activa" : "Vista P\xC3\xA1gina \xC3\x9Anica activa";
        break;
    case CMD_TOGGLE_SPLIT_VIEW:
        if (st->splitViewActive && !st->splitViewVertical) {
            st->splitViewActive = false;
        } else {
            st->splitViewActive = true;
            st->splitViewVertical = false;
            st->splitCamera = st->camera;
            st->splitCamera.panY += 420.0;
        }
        st->statusBannerUtf8 = st->splitViewActive ? "Vista Dividida Horizontal (lado a lado) activa" : "Vista Dividida cerrada";
        break;
    case CMD_TOGGLE_SPLIT_VERTICAL:
        if (st->splitViewActive && st->splitViewVertical) {
            st->splitViewActive = false;
            st->splitViewVertical = false;
        } else {
            st->splitViewActive = true;
            st->splitViewVertical = true;
            st->splitCamera = st->camera;
            st->splitCamera.panY += 420.0;
        }
        st->statusBannerUtf8 = st->splitViewActive ? "Vista Dividida Vertical (arriba / abajo) activa" : "Vista Dividida cerrada";
        break;
    case CMD_OPEN_PROJECTOR: {
        ViewportOverlayState ov{};
        PopulateViewportOverlay(st, ov);
        if (st->projectorWnd.Open(GetModuleHandleW(nullptr), st->hwndMain, &st->doc, &st->camera, &st->pageCache, &ov)) {
            st->statusBannerUtf8 = "Ventana de Proyector activa (capas privadas ocultas; tecla F para congelar)";
        }
        break;
    }
    case CMD_FREEZE_PROJECTOR:
        if (st->projectorWnd.IsOpen()) {
            st->projectorWnd.SetFrozen(!st->projectorWnd.IsFrozen());
            st->statusBannerUtf8 = st->projectorWnd.IsFrozen() ? "Proyector CONGELADO (puedes editar en privado)" : "Proyector descongelado en vivo";
        } else {
            st->statusBannerUtf8 = "Abra primero la Ventana de Proyector";
        }
        break;
    case CMD_INSERT_PRINTOUT:
    case CMD_IMPORT_PDF_TO_CANVAS: {
        if (IsDocumentLockedForEdit(st)) break;
        double cx = 60.0, cy = 60.0;
        if (st->hasLastContextWorldPt && st->doc.Mode() == DocumentMode::InfiniteCanvas) {
            cx = st->lastContextWorldPt.x;
            cy = st->lastContextWorldPt.y;
            st->hasLastContextWorldPt = false;
            // Avoid overlapping an existing column directly if user right-clicked on top of an existing sheet
            for (const auto& o : st->doc.CanvasObjects()) {
                if (o.kind == ObjectKind::PdfCard && o.bounds.Contains(cx, cy, 4.0)) {
                    cx = o.bounds.Right() + 48.0;
                    cy = o.bounds.y;
                    break;
                }
            }
        } else {
            double maxRight = 0.0;
            bool anyCards = false;
            for (const auto& o : st->doc.CanvasObjects()) {
                if (o.kind == ObjectKind::PdfCard) {
                    maxRight = (std::max)(maxRight, o.bounds.Right());
                    anyCards = true;
                }
            }
            if (anyCards) {
                // Place new document in the next column to the right at the current camera Y
                cx = maxRight + 64.0;
                cy = (std::max)(60.0, st->camera.panY + 40.0);
            }
        }
        if (GuiDialogs::RunInsertPrintoutDialog(
                st->hwndMain, st->doc, st->cmdStack, st->activePageIndex, { cx, cy }, st->statusBannerUtf8)) {
            st->pageCache.InvalidateAll(++st->asyncGen);
            LayoutMainChildren(st);
            UpdateWindowTitle(st);
            InvalidateViews(st);
            AppendWalMutation(st, "insert_printout");
        }
        break;
    }
    case CMD_TOGGLE_LOCK_IN_PLACE: {
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        if (!ids.empty()) {
            const ObjectTarget tgt{ st->selectedIsCanvas, st->selectedPageIndex };
            std::vector<AnnotationObject> beforeVec, afterVec;
            bool newLockState = false;
            for (std::uint64_t oid : ids) {
                if (const auto* cur = CanvasManip::FindObjectConst(st->doc, tgt, oid)) {
                    beforeVec.push_back(*cur);
                    AnnotationObject after = *cur;
                    after.lockedInPlace = !cur->lockedInPlace;
                    newLockState = after.lockedInPlace;
                    afterVec.push_back(std::move(after));
                }
            }
            if (!beforeVec.empty() &&
                st->cmdStack.Execute(st->doc, std::make_unique<BatchModifyObjectsCommand>(
                    tgt, std::move(beforeVec), std::move(afterVec), "Fijar/Desfijar copia impresa"))) {
                st->statusBannerUtf8 = newLockState
                    ? "Copia impresa fijada al fondo"
                    : "Copia impresa desfijada (lista para seleccionar, arrastrar, mover o editar)";
                AppendWalMutation(st, "lock_in_place");
            }
        }
        break;
    }
    case CMD_ADJUST_SHEET_SPACING: {
        std::wstring vGapW = L"96";
        std::wstring hGapW = L"48";
        if (!ShowTextPrompt(st->hwndMain, L"Espacio personalizado entre hojas",
                L"Espacio vertical entre hoja y hoja (pt):", vGapW,
                L"Espacio horizontal entre columnas (pt):", &hGapW)) {
            break;
        }
        const double rowGap = std::clamp(_wtof(vGapW.c_str()), 0.0, 4000.0);
        const double colGap = std::clamp(_wtof(hGapW.c_str()), 0.0, 4000.0);

        // Collect target PdfCard sheets (if a specific document's card is selected, re-space that document's cards; otherwise all PdfCards)
        std::string filterSource;
        if (st->selectedObjectId != 0) {
            if (const auto* sel = st->doc.FindCanvasObject(st->selectedObjectId)) {
                if (sel->kind == ObjectKind::PdfCard && !sel->printoutSourceNameUtf8.empty()) {
                    filterSource = sel->printoutSourceNameUtf8;
                }
            }
        }
        std::vector<const AnnotationObject*> cards;
        for (const auto& o : st->doc.CanvasObjects()) {
            if (o.kind == ObjectKind::PdfCard) {
                if (filterSource.empty() || o.printoutSourceNameUtf8 == filterSource) {
                    cards.push_back(&o);
                }
            }
        }
        if (cards.size() < 2) {
            st->statusBannerUtf8 = "Se necesitan al menos 2 hojas en el Lienzo Infinito para ajustar el espacio";
            break;
        }
        // Preserve reading order (sort by original row Y, then X, or page index within same source)
        std::sort(cards.begin(), cards.end(), [](const AnnotationObject* a, const AnnotationObject* b) {
            if (std::abs(a->bounds.y - b->bounds.y) > 20.0) return a->bounds.y < b->bounds.y;
            return a->bounds.x < b->bounds.x;
        });

        // Detect number of columns from initial row
        int cols = 1;
        const double firstRowY = cards[0]->bounds.y;
        for (std::size_t i = 1; i < cards.size(); ++i) {
            if (std::abs(cards[i]->bounds.y - firstRowY) <= 20.0) ++cols;
            else break;
        }
        const double originX = cards[0]->bounds.x;
        const double originY = cards[0]->bounds.y;

        std::vector<AnnotationObject> beforeVec;
        std::vector<AnnotationObject> afterVec;
        double curX = originX;
        double curY = originY;
        double rowMaxH = 0.0;
        int colIdx = 0;

        for (const auto* c : cards) {
            const double dx = curX - c->bounds.x;
            const double dy = curY - c->bounds.y;
            if (std::abs(dx) > 1e-3 || std::abs(dy) > 1e-3) {
                beforeVec.push_back(*c);
                AnnotationObject movedCard = *c;
                movedCard.Translate(dx, dy);
                afterVec.push_back(std::move(movedCard));

                // Also shift any ink strokes or notes drawn on or anchored to this sheet
                for (const auto& o : st->doc.CanvasObjects()) {
                    if (o.id == c->id || o.kind == ObjectKind::PdfCard) continue;
                    const DocPoint center{ o.bounds.x + o.bounds.w * 0.5, o.bounds.y + o.bounds.h * 0.5 };
                    if (o.anchorObjectId == c->id || c->bounds.Contains(center.x, center.y, 4.0)) {
                        const bool alreadyAdded = std::any_of(beforeVec.begin(), beforeVec.end(),
                            [&](const AnnotationObject& b) { return b.id == o.id; });
                        if (!alreadyAdded) {
                            beforeVec.push_back(o);
                            AnnotationObject movedAnn = o;
                            movedAnn.Translate(dx, dy);
                            afterVec.push_back(std::move(movedAnn));
                        }
                    }
                }
            }
            rowMaxH = (std::max)(rowMaxH, c->bounds.h);
            ++colIdx;
            if (colIdx >= cols) {
                colIdx = 0;
                curX = originX;
                curY += rowMaxH + rowGap;
                rowMaxH = 0.0;
            } else {
                curX += c->bounds.w + colGap;
            }
        }
        if (!beforeVec.empty()) {
            st->cmdStack.Execute(st->doc, std::make_unique<BatchModifyObjectsCommand>(
                ObjectTarget{ true, 0 }, std::move(beforeVec), std::move(afterVec), "Ajustar espacio entre hojas"));
            AppendWalMutation(st, "adjust_sheet_spacing");
            st->statusBannerUtf8 = "Espacio entre hojas actualizado a " + std::to_string(static_cast<int>(rowGap)) +
                " pt vertical / " + std::to_string(static_cast<int>(colGap)) + " pt horizontal (" +
                std::to_string(cards.size()) + " hojas)";
        }
        break;
    }
    case CMD_INSERT_VERTICAL_SPACE: {
        std::wstring deltaW = L"160";
        if (!ShowTextPrompt(st->hwndMain, L"Insertar / Quitar espacio vertical",
                L"Desplazamiento vertical en puntos (ej. 160 o -80):", deltaW) || deltaW.empty()) {
            break;
        }
        const double dy = std::clamp(_wtof(deltaW.c_str()), -10000.0, 10000.0);
        if (std::abs(dy) < 0.5) break;
        double splitY = st->hasLastContextWorldPt ? st->lastContextWorldPt.y : (st->camera.panY + 120.0);
        st->hasLastContextWorldPt = false;
        if (st->selectedObjectId != 0) {
            if (const auto* sel = st->doc.FindCanvasObject(st->selectedObjectId)) {
                splitY = sel->bounds.Bottom() + 2.0;
            }
        }
        std::vector<AnnotationObject> beforeVec, afterVec;
        for (const auto& o : st->doc.CanvasObjects()) {
            if (o.bounds.y >= splitY) {
                beforeVec.push_back(o);
                AnnotationObject after = o;
                after.Translate(0.0, dy);
                afterVec.push_back(std::move(after));
            }
        }
        if (!beforeVec.empty()) {
            st->cmdStack.Execute(st->doc, std::make_unique<BatchModifyObjectsCommand>(
                ObjectTarget{ true, 0 }, std::move(beforeVec), std::move(afterVec), "Insertar espacio vertical"));
            AppendWalMutation(st, "insert_vertical_space");
            st->statusBannerUtf8 = "Espacio vertical de " + std::to_string(static_cast<int>(dy)) +
                " pt aplicado a " + std::to_string(beforeVec.size()) + " objeto(s)";
        }
        break;
    }
    case CMD_EDIT_SHEET_CONTENT: {
        const ObjectTarget tgt = CurrentTarget(st);
        const AnnotationObject* obj = CanvasManip::FindObjectConst(st->doc, tgt, st->selectedObjectId);
        if (!obj) break;
        if (obj->kind == ObjectKind::TextBox) {
            BeginInlineTextEdit(st, tgt, *obj);
        } else if (obj->kind == ObjectKind::TableBlock) {
            ExecuteToolbarCommand(st, CMD_ADD_TABLE);
        } else if (obj->kind == ObjectKind::MathBlock) {
            std::wstring exprW = Utf8ToWide(obj->formulaExprUtf8);
            if (ShowTextPrompt(st->hwndMain, L"Editar F\u00F3rmula 2D", L"Expresi\u00F3n:", exprW) && !exprW.empty()) {
                AnnotationObject before = *obj;
                AnnotationObject after = *obj;
                after.formulaExprUtf8 = WideToUtf8(exprW);
                const auto ev = Cas::EvaluateExpression(after.formulaExprUtf8);
                if (ev.ok) after.evaluatedResultUtf8 = ev.formattedResultUtf8;
                st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)));
                st->statusBannerUtf8 = "F\xC3\xB3rmula recalculada";
            }
        } else if (obj->kind == ObjectKind::PdfCard) {
            // Extract editable text block right next to the printed sheet so user can edit the slide/page text directly
            AnnotationObject tb{};
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { obj->bounds.Right() + 24.0, obj->bounds.y, 420.0, (std::min)(520.0, obj->bounds.h) };
            tb.textUtf8 = !obj->printoutTextUtf8.empty() ? obj->printoutTextUtf8 : obj->captionUtf8;
            tb.fontSizePt = 12.0;
            tb.strokeColor = DocColor::RGBA(20, 26, 36, 255);
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(tb)));
            const std::uint64_t newId = AppActions::LastInsertedId(st->doc, tgt);
            if (newId != 0) {
                st->selectedObjectId = newId;
                st->selectedIsCanvas = tgt.isCanvas;
                st->selectedPageIndex = tgt.pageIndex;
                if (const auto* created = CanvasManip::FindObjectConst(st->doc, tgt, newId)) {
                    BeginInlineTextEdit(st, tgt, *created);
                }
            }
            st->statusBannerUtf8 = "Texto de la hoja extra\xC3\xAD" "do como bloque editable junto a la p\xC3\xA1gina";
        }
        break;
    }
    case CMD_SNIP_REGION_AS_IMAGE: {
        DocRect worldRect = st->lastSnipRectWorld;
        const std::vector<DocPoint>* polyPtr = (st->lastSnipPolyWorld.size() >= 3) ? &st->lastSnipPolyWorld : nullptr;
        if (!st->hasLastSnipRegion || worldRect.w < 12.0 || worldRect.h < 12.0) {
            // Fall back to currently selected object(s) bounding box in world coordinates
            std::vector<std::uint64_t> ids = st->lassoSelectedIds;
            if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
            const ObjectTarget tgt = CurrentTarget(st);
            double ox = 0.0, oy = 0.0;
            if (!tgt.isCanvas) {
                if (const auto* pl = st->doc.Layout().PlacementAt(tgt.pageIndex)) {
                    ox = pl->WorldRect().x;
                    oy = pl->WorldRect().y;
                }
            }
            bool any = false;
            for (std::uint64_t oid : ids) {
                if (const auto* o = CanvasManip::FindObjectConst(st->doc, tgt, oid)) {
                    const DocRect wb{ ox + o->bounds.x, oy + o->bounds.y, o->bounds.w, o->bounds.h };
                    worldRect = any ? DocRect::FromMinMax(
                        (std::min)(worldRect.x, wb.x), (std::min)(worldRect.y, wb.y),
                        (std::max)(worldRect.Right(), wb.Right()), (std::max)(worldRect.Bottom(), wb.Bottom())) : wb;
                    any = true;
                }
            }
            polyPtr = nullptr;
        }
        if (!SnipWorldRegionAsImage(st, worldRect, polyPtr)) {
            st->statusBannerUtf8 = "Encierre primero una regi\xC3\xB3n con Lazo (c\xC3\xADrculo) o Recorte (rect\xC3\xA1ngulo) para copiarla como imagen";
        }
        break;
    }
    case CMD_SEARCH_IN_THIS_SHEET: {
        std::wstring qW;
        if (!ShowTextPrompt(st->hwndMain, L"Buscar en esta hoja", L"Palabra o frase:", qW) || qW.empty()) break;
        if (!st->searchVisible) {
            st->searchVisible = true;
            LayoutMainChildren(st);
        }
        if (st->hwndSearchEdit) {
            SetWindowTextW(st->hwndSearchEdit, qW.c_str());
        }
        st->lastSearchQueryUtf8.clear();
        ExecuteToolbarCommand(st, CMD_SEARCH_NEXT);
        break;
    }
    case CMD_TOGGLE_PARTIAL_ERASER:
        st->eraserActionMode = (st->eraserActionMode == EraserActionMode::WholeStroke)
            ? EraserActionMode::PartialPixel : EraserActionMode::WholeStroke;
        st->activeTool = ToolKind::Eraser;
        st->statusBannerUtf8 = (st->eraserActionMode == EraserActionMode::PartialPixel)
            ? "Borrador de Precisi\xC3\xB3n / P\xC3\xADxel activo (divide trazos)"
            : "Borrador de Trazo Completo activo";
        break;
    case CMD_CYCLE_PEN_STYLE: {
        const int next = (static_cast<int>(st->penStyle) + 1) % 4;
        st->penStyle = static_cast<InkPenStyle>(next);
        static const char* kStyleNames[4] = { "Bol\xC3\xADgrafo", "Pluma Fuente", "Pincel Caligr\xC3\xA1""fico", "L\xC3\xA1piz Grafito" };
        st->activeTool = ToolKind::Pen;
        st->statusBannerUtf8 = std::string("Estilo de l\xC3\xA1piz: ") + kStyleNames[next];
        break;
    }
    case CMD_CUSTOM_INK_COLOR: {
        static COLORREF customPalette[16] = {
            RGB(26,115,232), RGB(220,38,38), RGB(21,128,61), RGB(245,158,11),
            RGB(124,58,237), RGB(14,116,144), RGB(190,24,93), RGB(17,24,39),
            RGB(71,85,105), RGB(234,88,12), RGB(37,99,235), RGB(5,150,105),
            RGB(168,85,247), RGB(217,119,6), RGB(225,29,72), RGB(15,23,42)
        };
        CHOOSECOLORW cc{};
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = st->hwndMain;
        cc.rgbResult = RGB(st->penColor.r, st->penColor.g, st->penColor.b);
        cc.lpCustColors = customPalette;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (ChooseColorW(&cc)) {
            st->penColor = DocColor::RGBA(GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult), 255);
            st->statusBannerUtf8 = "Color de tinta personalizado aplicado";
        }
        break;
    }
    case CMD_TOGGLE_SNAP_GRID:
        st->snapToGrid = !st->snapToGrid;
        st->statusBannerUtf8 = st->snapToGrid ? "Ajuste a cuadr\xC3\xAD" "cula ACTIVADO" : "Ajuste a cuadr\xC3\xAD" "cula desactivado";
        break;
    case CMD_CYCLE_GEOMETRY_TOOL: {
        if (!st->rulerState.visible) {
            RECT rc{};
            GetClientRect(st->hwndViewport, &rc);
            const DocPoint cW = st->camera.ScreenToWorld((rc.right - rc.left) * 0.5, (rc.bottom - rc.top) * 0.55);
            st->rulerState.centerX = cW.x;
            st->rulerState.centerY = cW.y;
            st->rulerState.mode = RulerMode::Straightedge;
            st->rulerState.visible = true;
            st->statusBannerUtf8 = "Regla Recta activa";
        } else if (st->rulerState.mode == RulerMode::Straightedge) {
            st->rulerState.mode = RulerMode::Protractor;
            st->statusBannerUtf8 = "Transportador 180\xC2\xB0 activo (traza arcos precisos)";
        } else if (st->rulerState.mode == RulerMode::Protractor) {
            st->rulerState.mode = RulerMode::Compass;
            st->rulerState.compassRadiusPt = 110.0;
            st->statusBannerUtf8 = "Comp\xC3\xA1s geom\xC3\xA9trico activo (radio 110 pt)";
        } else {
            st->rulerState.visible = false;
            st->rulerState.mode = RulerMode::Straightedge;
            st->statusBannerUtf8 = "Herramienta geom\xC3\xA9trica oculta";
        }
        break;
    }
    case CMD_COPY_STROKE_STYLE: {
        if (const auto* o = CanvasManip::FindObjectConst(st->doc, CurrentTarget(st), st->selectedObjectId)) {
            st->penColor = o->strokeColor;
            st->penWidth = std::clamp(o->strokeWidth, 0.8, 24.0);
            st->penStyle = o->penStyle;
            st->statusBannerUtf8 = "Estilo del objeto copiado al l\xC3\xA1piz activo";
        } else {
            st->statusBannerUtf8 = "Seleccione un trazo u objeto para copiar su estilo";
        }
        break;
    }
    case CMD_RESTYLE_INK: {
        if (IsDocumentLockedForEdit(st)) break;
        const ObjectTarget tgt = CurrentTarget(st);
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        std::vector<AnnotationObject> beforeSnaps;
        for (std::uint64_t oid : ids) {
            if (const auto* o = CanvasManip::FindObjectConst(st->doc, tgt, oid)) {
                if (o->kind == ObjectKind::InkStroke) beforeSnaps.push_back(*o);
            }
        }
        const bool ok = !beforeSnaps.empty() && InkEngine::NeatenAndRestyleStrokes(st->doc, tgt, ids);
        if (ok) {
            std::vector<AnnotationObject> afterSnaps;
            for (const auto& b : beforeSnaps) {
                if (AnnotationObject* m = CanvasManip::FindObjectMut(st->doc, tgt, b.id)) {
                    afterSnaps.push_back(*m);
                    *m = b;
                }
            }
            if (afterSnaps.size() == beforeSnaps.size()) {
                st->cmdStack.Execute(st->doc, std::make_unique<BatchModifyObjectsCommand>(
                    tgt, std::move(beforeSnaps), std::move(afterSnaps), "Estilizar tinta"));
                AppendWalMutation(st, "restyle_ink");
            }
        }
        st->statusBannerUtf8 = ok
            ? ("Estilizados y enderezados " + std::to_string(ids.size()) + " trazo(s)")
            : "Seleccione o encierre con Lazo los trazos a estilizar";
        break;
    }
    case CMD_HTR_TO_TEXT:
    case CMD_HTR_TO_MATH: {
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        GuiDialogs::RunOfflineInkHtr(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), ids, (id == CMD_HTR_TO_MATH), st->statusBannerUtf8);
        break;
    }
    case CMD_GROUP_OBJECTS:
    case CMD_UNGROUP_OBJECTS: {
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        if (CanvasManip::GroupSelectedObjects(st->doc, st->cmdStack, CurrentTarget(st), ids, id == CMD_UNGROUP_OBJECTS)) {
            st->statusBannerUtf8 = (id == CMD_UNGROUP_OBJECTS) ? "Objetos desagrupados" : ("Agrupados " + std::to_string(ids.size()) + " objetos");
        } else {
            st->statusBannerUtf8 = "Seleccione uno o m\xC3\xA1s objetos primero";
        }
        break;
    }
    case CMD_DUPLICATE_OBJECTS: {
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        const auto dup = CanvasManip::DuplicateSelectedObjects(st->doc, st->cmdStack, CurrentTarget(st), ids);
        if (!dup.empty()) {
            st->lassoSelectedIds = dup;
            st->selectedObjectId = dup.back();
            st->statusBannerUtf8 = "Duplicado(s) " + std::to_string(dup.size()) + " objeto(s)";
        }
        break;
    }
    case CMD_CLIPBOARD_COPY:
    case CMD_CLIPBOARD_CUT: {
        std::vector<std::uint64_t> ids = st->lassoSelectedIds;
        if (ids.empty() && st->selectedObjectId != 0) ids.push_back(st->selectedObjectId);
        const ObjectTarget tgt = CurrentTarget(st);
        if (!ids.empty()) {
            st->objectClipboard.clear();
            std::string combinedTxt;
            for (std::uint64_t oid : ids) {
                if (const auto* o = CanvasManip::FindObjectConst(st->doc, tgt, oid)) {
                    st->objectClipboard.push_back(*o);
                    const std::string t = !o->textUtf8.empty() ? o->textUtf8
                        : (!o->formulaExprUtf8.empty() ? o->formulaExprUtf8
                        : (!o->printoutTextUtf8.empty() ? o->printoutTextUtf8 : o->evaluatedResultUtf8));
                    if (!t.empty()) {
                        if (!combinedTxt.empty()) combinedTxt += "\r\n";
                        combinedTxt += t;
                    }
                }
            }
            if (!combinedTxt.empty()) {
                CanvasManip::CopyTextToClipboardWin32(st->hwndMain, combinedTxt);
            }
            if (id == CMD_CLIPBOARD_CUT) {
                if (ids.size() > 1) {
                    st->cmdStack.Execute(st->doc, std::make_unique<BatchEraseObjectsCommand>(tgt, ids));
                } else {
                    st->cmdStack.Execute(st->doc, std::make_unique<RemoveObjectCommand>(tgt, ids[0]));
                }
                st->lassoSelectedIds.clear();
                st->selectedObjectId = 0;
                st->statusBannerUtf8 = "Cortado(s) " + std::to_string(st->objectClipboard.size()) + " objeto(s) al portapapeles";
                AppendWalMutation(st, "clipboard_cut");
            } else {
                st->statusBannerUtf8 = "Copiado(s) " + std::to_string(st->objectClipboard.size()) + " objeto(s) al portapapeles";
            }
        } else if (!st->lastSelectedPdfTextUtf8.empty()) {
            CanvasManip::CopyTextToClipboardWin32(st->hwndMain, st->lastSelectedPdfTextUtf8);
            st->statusBannerUtf8 = "Texto PDF copiado";
        }
        break;
    }
    case CMD_CLIPBOARD_PASTE: {
        double x = 80.0, y = 80.0;
        if (st->hasLastContextWorldPt && st->doc.Mode() == DocumentMode::InfiniteCanvas) {
            x = st->lastContextWorldPt.x;
            y = st->lastContextWorldPt.y;
            st->hasLastContextWorldPt = false;
        } else {
            NextCardPlacement(st, x, y);
        }
        const ObjectTarget tgt = CurrentTarget(st);
        if (!st->objectClipboard.empty()) {
            DocRect unionBox = st->objectClipboard[0].bounds;
            for (std::size_t i = 1; i < st->objectClipboard.size(); ++i) {
                const auto& b = st->objectClipboard[i].bounds;
                unionBox = DocRect::FromMinMax(
                    (std::min)(unionBox.x, b.x),
                    (std::min)(unionBox.y, b.y),
                    (std::max)(unionBox.Right(), b.Right()),
                    (std::max)(unionBox.Bottom(), b.Bottom()));
            }
            const double dx = x - unionBox.x;
            const double dy = y - unionBox.y;
            std::vector<std::uint64_t> newIds;
            for (const auto& src : st->objectClipboard) {
                AnnotationObject clone = src;
                clone.id = 0;
                clone.zOrder = 0;
                clone.Translate(dx, dy);
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(clone)));
                const std::uint64_t nid = AppActions::LastInsertedId(st->doc, tgt);
                if (nid != 0) newIds.push_back(nid);
            }
            if (!newIds.empty()) {
                st->selectedIsCanvas = tgt.isCanvas;
                st->selectedPageIndex = tgt.pageIndex;
                st->selectedObjectId = newIds.back();
                st->lassoSelectedIds = (newIds.size() > 1) ? newIds : std::vector<std::uint64_t>{};
                st->statusBannerUtf8 = "Pegado(s) " + std::to_string(newIds.size()) + " objeto(s)";
                AppendWalMutation(st, "clipboard_paste_objs");
            }
            break;
        }
        if (!CanvasManip::PasteClipboardIntoDocument(st->hwndMain, st->doc, st->cmdStack, tgt, x, y, st->statusBannerUtf8)) {
            st->statusBannerUtf8 = "No hay objeto, imagen ni texto compatible en el portapapeles";
        }
        break;
    }
    case CMD_ADD_REDACTION: {
        double x = 90.0, y = 120.0;
        NextCardPlacement(st, x, y);
        AnnotationObject red{};
        red.kind = ObjectKind::Shape;
        red.shapeKind = ShapeKind::Rectangle;
        red.layerId = st->doc.ActiveLayerId();
        red.bounds = { x, y, 220.0, 32.0 };
        red.isRedaction = true;
        red.fillColor = DocColor::RGBA(0, 0, 0, 255);
        red.strokeColor = DocColor::RGBA(0, 0, 0, 255);
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(red)));
        st->statusBannerUtf8 = "Censura PDF permanente insertada (quema caja negra al exportar)";
        break;
    }
    case CMD_ADD_CONNECTOR: {
        const ObjectTarget tgt = CurrentTarget(st);
        const auto& vec = tgt.isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[tgt.pageIndex].objects;
        AnnotationObject conn{};
        conn.kind = ObjectKind::Shape;
        conn.shapeKind = ShapeKind::Connector;
        conn.layerId = st->doc.ActiveLayerId();
        conn.strokeColor = st->penColor;
        conn.strokeWidth = 2.0;
        if (vec.size() >= 2) {
            const auto& a = vec[vec.size() - 2];
            const auto& b = vec[vec.size() - 1];
            conn.connectorFromId = a.id;
            conn.connectorToId = b.id;
            conn.shapeStart = { a.bounds.x + a.bounds.w * 0.5, a.bounds.y + a.bounds.h * 0.5 };
            conn.shapeEnd = { b.bounds.x + b.bounds.w * 0.5, b.bounds.y + b.bounds.h * 0.5 };
        } else {
            conn.shapeStart = { 120.0, 140.0 };
            conn.shapeEnd = { 320.0, 210.0 };
        }
        conn.RecomputeBounds();
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(conn)));
        st->statusBannerUtf8 = "Conector magn\xC3\xA9tico enlazado";
        break;
    }
    case CMD_IMPORT_IMAGE_MULTI: {
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        GuiDialogs::RunMultiFormatImageImport(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, st->statusBannerUtf8);
        break;
    }
    case CMD_IMG_ENHANCE_WHITEBOARD:
    case CMD_IMG_REMOVE_BG:
    case CMD_IMG_ROTATE_90:
    case CMD_IMG_TOGGLE_OPACITY:
    case CMD_IMG_CROP: {
        const int m = (id == CMD_IMG_ENHANCE_WHITEBOARD) ? 0
            : (id == CMD_IMG_REMOVE_BG) ? 1
            : (id == CMD_IMG_ROTATE_90) ? 2
            : (id == CMD_IMG_TOGGLE_OPACITY) ? 3 : 4;
        GuiDialogs::EnhanceSelectedImage(st->doc, st->cmdStack, CurrentTarget(st), st->selectedObjectId, m, st->statusBannerUtf8);
        break;
    }
    case CMD_STICKER_OPAMP:
    case CMD_STICKER_RESISTOR:
    case CMD_STICKER_AXES:
    case CMD_STICKER_NOTE: {
        double x = 90.0, y = 90.0;
        NextCardPlacement(st, x, y);
        const int sk = (id == CMD_STICKER_OPAMP) ? 0 : (id == CMD_STICKER_RESISTOR) ? 1 : (id == CMD_STICKER_AXES) ? 2 : 3;
        GuiDialogs::InsertEngineeringSticker(st->doc, st->cmdStack, CurrentTarget(st), x, y, sk, st->statusBannerUtf8);
        break;
    }
    case CMD_PAPER_ISOMETRIC:
    case CMD_PAPER_LOGLOG:
    case CMD_PAPER_POLAR:
    case CMD_PAPER_SMITH:
    case CMD_PAPER_CORNELL:
    case CMD_PAPER_MUSIC: {
        const PaperPattern pat = (id == CMD_PAPER_ISOMETRIC) ? PaperPattern::Isometric
            : (id == CMD_PAPER_LOGLOG) ? PaperPattern::LogLog
            : (id == CMD_PAPER_POLAR) ? PaperPattern::Polar
            : (id == CMD_PAPER_SMITH) ? PaperPattern::SmithChart
            : (id == CMD_PAPER_CORNELL) ? PaperPattern::Cornell : PaperPattern::MusicStaff;
        st->cmdStack.Execute(st->doc, std::make_unique<SetBackgroundCommand>(
            ObjectTarget{ st->doc.Mode() == DocumentMode::InfiniteCanvas, st->activePageIndex }, pat));
        st->statusBannerUtf8 = std::string("Plantilla aplicada: ") + PaperPatternLabelUtf8(pat);
        break;
    }
    case CMD_AUDIO_BOOKMARK: {
        std::wstring lbl = L"Explicaci\u00F3n clave";
        if (ShowTextPrompt(st->hwndMain, L"Marcador de Audio", L"Etiqueta del marcador temporal:", lbl) && !lbl.empty()) {
            const std::uint64_t tMs = st->replayActive ? st->replayCursorMs : st->audioSession.DurationMs();
            st->audioSession.AddBookmark(WideToUtf8(lbl), tMs);
            st->statusBannerUtf8 = "Marcador de audio guardado en " + std::to_string(tMs / 1000) + " s";
        }
        break;
    }
    case CMD_AUDIO_IMPORT: {
        static constexpr COMDLG_FILTERSPEC kW[] = { { L"Audio WAV PCM (*.wav)", L"*.wav" } };
        std::vector<std::wstring> paths;
        if (ShowOpenDialog(st->hwndMain, L"Importar Audio WAV", kW, 1, paths, false)) {
            std::ifstream ifs(std::filesystem::path(paths[0]), std::ios::binary | std::ios::ate);
            if (ifs.is_open()) {
                const auto sz = ifs.tellg();
                if (sz >= 44 && static_cast<std::uint64_t>(sz) <= 128ull * 1024ull * 1024ull) {
                    std::vector<std::uint8_t> buf(static_cast<std::size_t>(sz));
                    ifs.seekg(0, std::ios::beg);
                    if (ifs.read(reinterpret_cast<char*>(buf.data()), sz) &&
                        st->audioSession.LoadFromWavBytes(buf.data(), buf.size())) {
                        double x = 80.0, y = 80.0;
                        NextCardPlacement(st, x, y);
                        AnnotationObject clip{};
                        clip.kind = ObjectKind::AudioClip;
                        clip.layerId = st->doc.ActiveLayerId();
                        clip.bounds = { x, y, 270.0, 56.0 };
                        clip.captionUtf8 = "Audio WAV (" + std::to_string(st->audioSession.DurationMs() / 1000) + " s)";
                        clip.inkStartTimeMs = 0;
                        clip.inkEndTimeMs = st->audioSession.DurationMs();
                        const std::string key = st->doc.RegisterAsset(buf);
                        clip.assetKey = key;
                        clip.audioAssetKey = key;
                        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(clip)));
                        st->statusBannerUtf8 = "Audio WAV importado (" + std::to_string(st->audioSession.DurationMs() / 1000) + " s)";
                    } else {
                        st->statusBannerUtf8 = "Archivo WAV inv\xC3\xA1lido o formato PCM no soportado";
                    }
                } else {
                    st->statusBannerUtf8 = "Archivo WAV vac\xC3\xADo o superior al l\xC3\xADmite de 128 MB";
                }
            }
        }
        break;
    }
    case CMD_AUDIO_TRANSCRIBE: {
        const auto segs = st->audioSession.TranscribeOffline();
        std::ostringstream os;
        os << "Segmentos de voz detectados offline (" << segs.size() << "):\r\n\r\n";
        for (const auto& s : segs) {
            os << "  [" << (s.startMs / 1000.0) << "s - " << (s.endMs / 1000.0) << "s] " << s.textUtf8 << "\r\n";
        }
        if (segs.empty()) os << "  (Grabe o importe audio WAV primero para segmentar voz offline.)";
        GuiDialogs::ShowReportDialog(st->hwndMain, L"Transcripci\u00F3n y Segmentos de Audio Offline", Utf8ToWide(os.str()));
        st->statusBannerUtf8 = "Transcripci\xC3\xB3n offline: " + std::to_string(segs.size()) + " segmento(s)";
        break;
    }
    case CMD_AUDIO_EXPORT_HTML5: {
        static constexpr COMDLG_FILTERSPEC kH[] = { { L"Tutorial Interactivo HTML5 (*.html)", L"*.html" } };
        std::wstring outPath;
        if (ShowSaveDialog(st->hwndMain, L"Exportar Tutorial HTML5 Sincronizado", kH, 1, L"html", L"tutorial_cuadernu.html", outPath)) {
            std::string err;
            if (st->audioSession.ExportSyncedTutorialHtml5(st->doc, WideToUtf8(outPath), &err)) {
                st->statusBannerUtf8 = "Tutorial HTML5 sincronizado exportado";
            } else {
                st->statusBannerUtf8 = "Error HTML5: " + err;
            }
        }
        break;
    }
    case CMD_ADD_TABLE: {
        if (IsDocumentLockedForEdit(st)) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        GuiDialogs::RunReactiveTableDialog(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    }
    case CMD_ADD_CODE_BLOCK: {
        if (IsDocumentLockedForEdit(st)) break;
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        GuiDialogs::RunEngineeringToolboxOrCodeBlock(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, 2, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    }
    case CMD_ADV_MATH: {
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        GuiDialogs::RunAdvancedMathDialog(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    }
    case CMD_CHECK_MY_WORK:
    case CMD_TEACH_ME:
    case CMD_SUMMARIZE_NOTES:
    case CMD_LOCAL_QA:
    case CMD_COMPARE_4_CASIO: {
        if (st->examModeLocked && id != CMD_COMPARE_4_CASIO) {
            st->statusBannerUtf8 = "Modo Examen activo: funciones asistidas bloqueadas (desbloquee con PIN)";
            break;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const int m = (id == CMD_CHECK_MY_WORK) ? 0
            : (id == CMD_TEACH_ME) ? 1
            : (id == CMD_SUMMARIZE_NOTES) ? 2
            : (id == CMD_LOCAL_QA) ? 3 : 4;
        GuiDialogs::RunCheckMyWorkOrTeachMe(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, m, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    }
    case CMD_RECOGNIZE_CIRCUIT:
    case CMD_ENG_TOOLBOX:
    case CMD_FORMULA_AST_SEARCH:
    case CMD_CONTROL_BLOCK_LAB:
    case CMD_CIRCUIT_TOPO_AUDIT: {
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const int m = (id == CMD_RECOGNIZE_CIRCUIT) ? 0
            : (id == CMD_ENG_TOOLBOX) ? 1
            : (id == CMD_FORMULA_AST_SEARCH) ? 3
            : (id == CMD_CONTROL_BLOCK_LAB) ? 4 : 5;
        GuiDialogs::RunEngineeringToolboxOrCodeBlock(st->hwndMain, st->doc, st->cmdStack, CurrentTarget(st), x, y, m, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    }
    case CMD_EXAM_VARIANTS_LMS:
        GuiDialogs::RunExamVariantsAndLmsDialog(st->hwndMain, st->doc, MakePromptAdapter(st), st->statusBannerUtf8);
        break;
    case CMD_LAN_QR:
        GuiDialogs::RunSecurityOrLanFlow(st->hwndMain, st->doc, 2, MakePasswordPromptAdapter(st), st->statusBannerUtf8);
        break;
    case CMD_EXAM_PIN_LOCK: {
        std::wstring pinW;
        if (!st->examModeLocked) {
            if (ShowTextPrompt(st->hwndMain, L"Activar Modo Examen", L"Establezca un PIN de 4 d\u00EDgitos para bloquear funciones asistidas:", pinW, nullptr, nullptr, false, true) && !pinW.empty()) {
                st->examPinUtf8 = WideToUtf8(pinW);
                st->examModeLocked = true;
                st->statusBannerUtf8 = "Modo Examen ACTIVADO (bloqueado por PIN)";
            }
        } else {
            if (ShowTextPrompt(st->hwndMain, L"Desbloquear Modo Examen", L"Ingrese el PIN para desactivar el Modo Examen:", pinW, nullptr, nullptr, false, true)) {
                if (WideToUtf8(pinW) == st->examPinUtf8) {
                    st->examModeLocked = false;
                    st->examPinUtf8.clear();
                    st->statusBannerUtf8 = "Modo Examen desactivado";
                } else {
                    st->statusBannerUtf8 = "PIN incorrecto";
                }
            }
        }
        break;
    }
    case CMD_PEN_WIDTH_FINE:   st->penWidth = 1.5; st->activeTool = ToolKind::Pen; st->statusBannerUtf8 = "Grosor de tinta: Fino (1.5 pt)"; break;
    case CMD_PEN_WIDTH_MED:    st->penWidth = 2.6; st->activeTool = ToolKind::Pen; st->statusBannerUtf8 = "Grosor de tinta: Medio (2.6 pt)"; break;
    case CMD_PEN_WIDTH_BOLD:   st->penWidth = 4.8; st->activeTool = ToolKind::Pen; st->statusBannerUtf8 = "Grosor de tinta: Grueso (4.8 pt)"; break;
    case CMD_PEN_WIDTH_MARKER: st->penWidth = 8.0; st->activeTool = ToolKind::Pen; st->statusBannerUtf8 = "Grosor de tinta: Marcador (8.0 pt)"; break;
    case CMD_PEN_OPACITY_100:  st->penOpacity = 1.00; st->statusBannerUtf8 = "Opacidad de tinta: 100%"; break;
    case CMD_PEN_OPACITY_75:   st->penOpacity = 0.75; st->statusBannerUtf8 = "Opacidad de tinta: 75%"; break;
    case CMD_PEN_OPACITY_50:   st->penOpacity = 0.50; st->statusBannerUtf8 = "Opacidad de tinta: 50%"; break;
    case CMD_PEN_OPACITY_30:   st->penOpacity = 0.30; st->statusBannerUtf8 = "Opacidad de tinta: 30%"; break;
    case CMD_TEXT_BOLD:
    case CMD_TEXT_ITALIC:
    case CMD_TEXT_UNDERLINE:
    case CMD_TEXT_BULLET_LIST:
    case CMD_TEXT_CHECKBOX_TOGGLE: {
        const ObjectTarget tgt = CurrentTarget(st);
        if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, tgt, st->selectedObjectId)) {
            if (obj->kind == ObjectKind::TextBox) {
                AnnotationObject before = *obj;
                AnnotationObject after = *obj;
                if (id == CMD_TEXT_BOLD) after.bold = !after.bold;
                else if (id == CMD_TEXT_ITALIC) after.italic = !after.italic;
                else if (id == CMD_TEXT_UNDERLINE) after.underline = !after.underline;
                else if (id == CMD_TEXT_BULLET_LIST) {
                    std::istringstream iss(after.textUtf8);
                    std::ostringstream oss;
                    std::string ln;
                    bool first = true;
                    while (std::getline(iss, ln)) {
                        if (!first) oss << '\n';
                        first = false;
                        if (ln.rfind("\xE2\x80\xA2 ", 0) == 0) oss << ln.substr(4);
                        else oss << "\xE2\x80\xA2 " << ln;
                    }
                    after.textUtf8 = oss.str();
                } else if (id == CMD_TEXT_CHECKBOX_TOGGLE) {
                    std::istringstream iss(after.textUtf8);
                    std::ostringstream oss;
                    std::string ln;
                    bool first = true;
                    while (std::getline(iss, ln)) {
                        if (!first) oss << '\n';
                        first = false;
                        if (ln.rfind("\xE2\x98\x90 ", 0) == 0) oss << "\xE2\x98\x91 " << ln.substr(4);
                        else if (ln.rfind("\xE2\x98\x91 ", 0) == 0) oss << "\xE2\x98\x90 " << ln.substr(4);
                        else oss << "\xE2\x98\x90 " << ln;
                    }
                    after.textUtf8 = oss.str();
                }
                st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)));
                st->statusBannerUtf8 = "Formato de texto actualizado";
            }
        }
        break;
    }
    case CMD_COMMAND_PALETTE: {
        if (st->commandPalette.All().empty()) st->commandPalette.RegisterDefaultCommands();
        std::string selTitle;
        const int chosenCmd = FeatureDialogs::ShowInteractiveCommandPaletteDialog(st->hwndMain, st->commandPalette, &selTitle);
        if (chosenCmd > 0 && chosenCmd != CMD_COMMAND_PALETTE) {
            st->statusBannerUtf8 = "Paleta ejecut\xC3\xB3: " + selTitle;
            ExecuteToolbarCommand(st, static_cast<WORD>(chosenCmd));
            return;
        }
        break;
    }
    case CMD_OPEN_DAILY_NOTE: {
        std::time_t now = std::time(nullptr);
        std::tm tmNow{};
        localtime_s(&tmNow, &now);
        char dBuf[16]{};
        std::strftime(dBuf, sizeof(dBuf), "%Y-%m-%d", &tmNow);
        const std::string nid = st->knowledgeVault.OpenOrCreateDailyNote(dBuf);
        if (const auto* n = st->knowledgeVault.FindNoteById(nid)) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { x, y, 360.0, 140.0 };
            tb.textUtf8 = n->bodyMarkdownUtf8;
            tb.captionUtf8 = "Nota Diaria " + n->dailyDateIsoUtf8;
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
            st->statusBannerUtf8 = "Nota Diaria abierta: " + n->titleUtf8;
        }
        break;
    }
    case CMD_TOGGLE_GRAPH_VIEW: {
        if (st->knowledgeVault.TotalNoteCount() == 0) {
            // Seed active document links & sections into the knowledge graph view
            const std::string rootId = st->knowledgeVault.CreateNote(st->doc.Title(), "Cuaderno",
                "Nota principal vinculada a [[Resumen]] y [[Formulario]].");
            st->knowledgeVault.CreateNote("Resumen", "Cuaderno", "Conectado con [[" + st->doc.Title() + "]].");
            st->knowledgeVault.CreateNote("Formulario", "Cuaderno", "Ver [[" + st->doc.Title() + "]] y [[Resumen]].");
            (void)rootId;
        }
        st->activeGraphView = st->knowledgeVault.BuildGlobalGraph();
        KnowledgeVault::ComputeGraphLayout2D(st->activeGraphView, 60, 800.0, 560.0);
        st->graphOverlayVisible = !st->graphOverlayVisible;
        st->statusBannerUtf8 = st->graphOverlayVisible
            ? ("Vista de Grafo activa (" + std::to_string(st->activeGraphView.nodes.size()) + " nodos)")
            : "Vista de Grafo oculta";
        break;
    }
    case CMD_VIEW_BACKLINKS: {
        std::string openTitle;
        bool mutated = false;
        if (ClosureDialogs::ShowKnowledgeLinksDialog(st->hwndMain, st->knowledgeVault, st->doc.Title(), openTitle, mutated)) {
            if (const auto* n = st->knowledgeVault.FindNoteByTitleOrAlias(openTitle)) {
                st->cachedReadingTextUtf8 = n->bodyMarkdownUtf8;
                st->readingViewMode = true;
                st->statusBannerUtf8 = "Enlace abierto en Vista de Lectura: " + n->titleUtf8;
            }
        }
        if (mutated) PersistStudioProfileNow(st);
        break;
    }
    case CMD_NEW_TEMPLATE_NOTE: {
        std::wstring tplW = L"# {{title}}\nFecha: {{date}} {{time}}\nID: {{uuid}}\n\n- [ ] Punto 1";
        if (ShowTextPrompt(st->hwndMain, L"Crear Nota desde Plantilla",
                L"Plantilla con variables {{title}}, {{date}}, {{time}}, {{uuid}}:", tplW, nullptr, nullptr, true, false)) {
            NoteTemplate nt;
            nt.nameUtf8 = "Estudio";
            nt.contentTemplateUtf8 = WideToUtf8(tplW);
            st->knowledgeVault.RegisterTemplate(nt);
            const std::string nid = st->knowledgeVault.CreateNoteFromTemplate("Estudio", st->doc.Title());
            if (const auto* n = st->knowledgeVault.FindNoteById(nid)) {
                double x = 80.0, y = 80.0;
                NextCardPlacement(st, x, y);
                AnnotationObject tb;
                tb.kind = ObjectKind::TextBox;
                tb.layerId = st->doc.ActiveLayerId();
                tb.bounds = { x, y, 360.0, 130.0 };
                tb.textUtf8 = n->bodyMarkdownUtf8;
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
                st->statusBannerUtf8 = "Plantilla aplicada con variables expandidas";
            }
        }
        break;
    }
    case CMD_INSERT_SYNCED_BLOCK: {
        std::wstring syncW = L":::sync[teorema_base]\nTeorema fundamental compartido entre notas\n:::";
        if (ShowTextPrompt(st->hwndMain, L"Insertar Bloque Sincronizado",
                L"Contenido del bloque sincronizado (:::sync[id] ... :::):", syncW, nullptr, nullptr, true, false)) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { x, y, 360.0, 110.0 };
            tb.textUtf8 = WideToUtf8(syncW);
            tb.captionUtf8 = "Bloque Sincronizado";
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
            st->statusBannerUtf8 = "Bloque sincronizado insertado";
        }
        break;
    }
    case CMD_SAVE_REUSABLE_SNIPPET: {
        std::vector<AnnotationObject> selObjs;
        const ObjectTarget tgt = CurrentTarget(st);
        const auto& vec = tgt.isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[tgt.pageIndex].objects;
        for (const auto& o : vec) {
            if (o.id == st->selectedObjectId ||
                std::find(st->lassoSelectedIds.begin(), st->lassoSelectedIds.end(), o.id) != st->lassoSelectedIds.end()) {
                selObjs.push_back(o);
            }
        }
        if (selObjs.empty() && !vec.empty()) selObjs.push_back(vec.back());
        if (!selObjs.empty()) {
            std::wstring nameW = L"Fragmento " + std::to_wstring(st->snippetLibrary.Entries().size() + 1);
            if (ShowTextPrompt(st->hwndMain, L"Guardar Elemento Reutilizable", L"Nombre del fragmento:", nameW) && !nameW.empty()) {
                const std::string sid = st->snippetLibrary.SaveSnippet(WideToUtf8(nameW), "Fragmentos", selObjs);
                PersistStudioProfileNow(st);
                st->statusBannerUtf8 = "Elemento guardado y persistido (" + sid + ")";
            }
        } else {
            st->statusBannerUtf8 = "Seleccione al menos un objeto para guardar en la biblioteca";
        }
        break;
    }
    case CMD_INSERT_REUSABLE_SNIPPET: {
        if (IsDocumentLockedForEdit(st)) break;
        if (st->snippetLibrary.Entries().empty()) {
            st->statusBannerUtf8 = "La biblioteca de fragmentos est\xC3\xA1 vac\xC3\xAD""a";
            break;
        }
        bool mutated = false;
        const std::string pickedId = ClosureDialogs::ShowSnippetManagerDialog(st->hwndMain, st->snippetLibrary, mutated);
        if (mutated) PersistStudioProfileNow(st);
        if (!pickedId.empty()) {
            double x = 96.0, y = 96.0;
            NextCardPlacement(st, x, y);
            if (st->snippetLibrary.InsertSnippetIntoDocument(pickedId, st->doc, st->cmdStack, CurrentTarget(st), x, y)) {
                st->statusBannerUtf8 = "Elemento reutilizable insertado (" + pickedId + ")";
            }
        }
        break;
    }
    case CMD_EXPORT_STICKY_NOTES: {
        const auto exp = ExportStickyNotesFromDocument(st->doc);
        std::wstring savePath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain,
                L"Markdown (*.md)\0*.md\0JSON (*.json)\0*.json\0\0", L"md", L"notas_adhesivas.md",
                L"Exportar Notas Adhesivas", savePath)) {
            const bool asJson = (GetFileExtLower(savePath) == L".json");
            std::ofstream ofs(savePath, std::ios::binary | std::ios::trunc);
            const std::string& payload = asJson ? exp.jsonUtf8 : exp.markdownUtf8;
            ofs.write(payload.data(), static_cast<std::streamsize>(payload.size()));
            CanvasManip::CopyTextToClipboardWin32(st->hwndMain, payload);
            st->statusBannerUtf8 = "Notas adhesivas exportadas (" + std::to_string(exp.count) + ") y copiadas al portapapeles";
        }
        break;
    }
    case CMD_EDIT_PROPERTIES: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring propW = st->docYamlFrontmatterUtf8.empty()
            ? L"---\nestado: En progreso\nprioridad: 1\npublish: true\ntags:\n  - estudio\n---"
            : Utf8ToWide(st->docYamlFrontmatterUtf8);
        if (ShowTextPrompt(st->hwndMain, L"Propiedades de la Nota (Frontmatter YAML)",
                L"Propiedades tipadas (texto, n\u00FAmero, fecha, casilla, lista):", propW, nullptr, nullptr, true, false)) {
            const std::string raw = WideToUtf8(propW);
            std::vector<NoteProperty> parsedProps;
            std::vector<std::string> parsedAliases, parsedTags;
            std::string bodyRemainder;
            KnowledgeVault::ParseFrontmatterAndBody(raw, parsedProps, parsedAliases, parsedTags, bodyRemainder);
            st->docYamlFrontmatterUtf8 = raw;
            st->docProperties = parsedProps;
            st->docAliasesUtf8 = parsedAliases;
            st->docTagsUtf8 = parsedTags;
            // Sync into KnowledgeVault so Graph / Publish / Search immediately reflect updated frontmatter
            const std::string docTitle = st->doc.Title().empty() ? "Cuaderno" : st->doc.Title();
            if (const auto* existing = st->knowledgeVault.FindNoteByTitleOrAlias(docTitle)) {
                if (auto* mutNote = st->knowledgeVault.FindNoteMutableById(existing->noteIdUtf8)) {
                    mutNote->properties = parsedProps;
                    mutNote->aliasesUtf8 = parsedAliases;
                    mutNote->tagsUtf8 = parsedTags;
                }
            } else {
                st->knowledgeVault.CreateNote(docTitle, "Cuaderno", raw + "\n", parsedProps);
            }
            st->doc.BumpRevision();
            PersistStudioProfileNow(st);
            st->statusBannerUtf8 = "Propiedades YAML guardadas (" + std::to_string(parsedProps.size()) + " prop., " + std::to_string(parsedTags.size()) + " tags)";
        }
        break;
    }
    case CMD_TOGGLE_READING_VIEW: {
        st->readingViewMode = !st->readingViewMode;
        if (st->readingViewMode) RebuildReadingViewText(st);
        st->statusBannerUtf8 = st->readingViewMode ? "Vista de Lectura activada" : "Modo Edici\xC3\xB3n activo";
        break;
    }
    case CMD_CUSTOM_SHORTCUTS: {
        std::wstring jsonW = Utf8ToWide(st->keymapProfile.ToJson().dump(2));
        if (ShowTextPrompt(st->hwndMain, L"Editor de Atajos Personalizados (JSON)",
                L"Edite las asignaciones en 'bindings' (ej. \"view.command_palette\": \"Ctrl+K\"):", jsonW, nullptr, nullptr, true, false)) {
            try {
                const auto parsed = nlohmann::json::parse(WideToUtf8(jsonW));
                st->keymapProfile = KeymapProfile::FromJson(parsed);
                PersistStudioProfileNow(st);
                st->statusBannerUtf8 = "Atajos personalizados guardados (" + std::to_string(st->keymapProfile.actionToShortcut.size()) + " atajos, " +
                    std::to_string(st->keymapProfile.warnings.size()) + " advertencias)";
            } catch (const std::exception& ex) {
                MessageBoxW(st->hwndMain, Utf8ToWide(std::string("JSON inv\xC3\xA1lido: ") + ex.what()).c_str(), L"Error en Atajos", MB_ICONWARNING);
            }
        }
        break;
    }
    case CMD_THEME_SELECTOR: {
        ExecuteToolbarCommand(st, CMD_TOGGLE_APPEARANCE);
        return;
    }
    case CMD_INSERT_STRUCTURED_DB: {
        if (IsDocumentLockedForEdit(st)) break;
        if (st->activeDatabase.Columns().empty()) {
            st->activeDatabase.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "En curso", "Completado" } });
            st->activeDatabase.AddColumn({ "prioridad", "Prioridad", DbColumnType::Number });
            st->activeDatabase.AddColumn({ "fecha", "Fecha", DbColumnType::Date });
            st->activeDatabase.AddColumn({ "lat", "Latitud", DbColumnType::Number });
            st->activeDatabase.AddColumn({ "lon", "Longitud", DbColumnType::Number });
            DbRecord r1; r1.titleUtf8 = "Laboratorio 1";
            const std::string id1 = st->activeDatabase.AddRecord(r1);
            st->activeDatabase.SetCellText(id1, "estado", "En curso");
            st->activeDatabase.SetCellNumber(id1, "prioridad", 1.0);
            st->activeDatabase.SetCellText(id1, "fecha", "2026-11-20");
            st->activeDatabase.SetCellNumber(id1, "lat", 9.856);
            st->activeDatabase.SetCellNumber(id1, "lon", -83.912);
            DbRecord r2; r2.titleUtf8 = "Entrega Proyecto";
            const std::string id2 = st->activeDatabase.AddRecord(r2);
            st->activeDatabase.SetCellText(id2, "estado", "Pendiente");
            st->activeDatabase.SetCellNumber(id2, "prioridad", 2.0);
            st->activeDatabase.SetCellText(id2, "fecha", "2026-11-24");
            st->activeDatabase.SetCellNumber(id2, "lat", 9.935);
            st->activeDatabase.SetCellNumber(id2, "lon", -84.091);
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        st->activeDatabase.SyncToDocumentTableObject(st->doc, st->cmdStack, CurrentTarget(st), x, y, true);
        st->dbPanelVisible = true;
        st->statusBannerUtf8 = "Base de datos estructurada (.base) insertada y panel visual abierto";
        break;
    }
    case CMD_CYCLE_DB_VIEW: {
        static const char* kViewNames[] = {
            "Tabla", "Lista", "Galer\xC3\xAD""a", "Kanban", "Calendario", "Cronograma", "Gr\xC3\xA1""fico", "Panel", "Mapa"
        };
        if (st->activeDatabase.Columns().empty()) {
            st->activeDatabase.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "En curso", "Completado" } });
            st->activeDatabase.AddColumn({ "prioridad", "Prioridad", DbColumnType::Number });
            st->activeDatabase.AddColumn({ "fecha", "Fecha", DbColumnType::Date });
            DbRecord r1; r1.titleUtf8 = "Registro inicial";
            const std::string id1 = st->activeDatabase.AddRecord(r1);
            st->activeDatabase.SetCellText(id1, "estado", "En curso");
            st->activeDatabase.SetCellNumber(id1, "prioridad", 1.0);
            st->activeDatabase.SetCellText(id1, "fecha", "2026-11-20");
        }
        st->activeDbViewCycle = (st->activeDbViewCycle + 1) % 9;
        st->dbPanelVisible = true;
        st->statusBannerUtf8 = std::string("Vista de Base de Datos activa: ") + kViewNames[st->activeDbViewCycle];
        break;
    }
    case CMD_TOGGLE_DB_PANEL: {
        st->dbPanelVisible = !st->dbPanelVisible;
        if (st->dbPanelVisible && st->activeDatabase.Columns().empty()) {
            st->activeDatabase.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Pendiente", "En curso", "Completado" } });
            st->activeDatabase.AddColumn({ "prioridad", "Prioridad", DbColumnType::Number });
            st->activeDatabase.AddColumn({ "fecha", "Fecha", DbColumnType::Date });
            DbRecord r1; r1.titleUtf8 = "Tarea 1";
            const std::string id1 = st->activeDatabase.AddRecord(r1);
            st->activeDatabase.SetCellText(id1, "estado", "Pendiente");
            st->activeDatabase.SetCellNumber(id1, "prioridad", 1.0);
            st->activeDatabase.SetCellText(id1, "fecha", "2026-11-20");
        }
        st->statusBannerUtf8 = st->dbPanelVisible ? "Panel de Base de Datos visible (clic en pesta\xC3\xB1""as para cambiar vista)" : "Panel de Base de Datos oculto";
        break;
    }
    case CMD_EXPORT_BASE_FILE: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain,
                L"Base de Datos (*.base)\0*.base\0JSON (*.json)\0*.json\0\0", L"base", L"datos.base",
                L"Exportar Base de Datos (.base)", outPath)) {
            std::string err;
            if (st->activeDatabase.SaveToBaseFile(outPath, &err)) {
                st->statusBannerUtf8 = "Base de datos exportada en disco (.base)";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando .base", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_IMPORT_BASE_FILE: {
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"Base de Datos (*.base;*.json)\0*.base;*.json\0\0", L"Importar Base de Datos (.base)", inPath)) {
            std::string err;
            if (st->activeDatabase.LoadFromBaseFile(inPath, &err)) {
                st->dbPanelVisible = true;
                st->statusBannerUtf8 = "Base .base importada (" + std::to_string(st->activeDatabase.Records().size()) + " registros)";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error importando .base", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_AUTOMATION_RULES: {
        std::wstring ruleW = L"estado=Completado -> set_prop:revisado=Si";
        if (ShowTextPrompt(st->hwndMain, L"Regla de Automatizaci\u00F3n de Base de Datos",
                L"Formato: columna=valor -> set_prop:col=val  (o add_tag:etiqueta)", ruleW)) {
            const std::string s = WideToUtf8(ruleW);
            const auto arrow = s.find("->");
            const auto eq = s.find('=');
            if (arrow != std::string::npos && eq != std::string::npos && eq < arrow) {
                auto trimS = [](std::string v) {
                    while (!v.empty() && v.front() == ' ') v.erase(v.begin());
                    while (!v.empty() && v.back() == ' ') v.pop_back();
                    return v;
                };
                AutomationRule r{};
                r.ruleIdUtf8 = "rule_" + std::to_string(st->automations.Rules().size() + 1);
                r.nameUtf8 = s;
                r.trigger = AutomationTriggerKind::OnPropertyChanged;
                r.watchColumnIdUtf8 = trimS(s.substr(0, eq));
                r.watchValueUtf8 = trimS(s.substr(eq + 1, arrow - eq - 1));
                const std::string act = trimS(s.substr(arrow + 2));
                AutomationActionStep step{};
                if (act.rfind("add_tag:", 0) == 0) {
                    step.actionKindUtf8 = "add_tag";
                    step.valueUtf8 = trimS(act.substr(8));
                } else {
                    step.actionKindUtf8 = "set_property";
                    const std::string body = (act.rfind("set_prop:", 0) == 0) ? trimS(act.substr(9)) : act;
                    const auto eq2 = body.find('=');
                    if (eq2 != std::string::npos) {
                        step.targetKeyUtf8 = trimS(body.substr(0, eq2));
                        step.valueUtf8 = trimS(body.substr(eq2 + 1));
                    }
                }
                r.steps.push_back(std::move(step));
                st->automations.RegisterRule(r);
                int firedTotal = 0;
                for (const auto& rec : st->activeDatabase.Records()) {
                    auto it = rec.cells.find(r.watchColumnIdUtf8);
                    if (it != rec.cells.end() && it->second.textUtf8 == r.watchValueUtf8) {
                        firedTotal += static_cast<int>(st->automations.TriggerEvent(
                            AutomationTriggerKind::OnPropertyChanged,
                            st->activeDatabase, rec.recordIdUtf8, r.watchColumnIdUtf8, r.watchValueUtf8));
                    }
                }
                st->statusBannerUtf8 = "Regla de automatizaci\xC3\xB3n registrada (" + std::to_string(firedTotal) + " ejecuciones inmediatas)";
            }
        }
        break;
    }
    case CMD_FLEXCIL_POPUP_OVERLAY: {
        const bool nowVisible = !st->flexcilOverlay.IsPopupNoteVisible();
        st->flexcilOverlay.SetPopupNoteVisible(nowVisible);
        st->statusBannerUtf8 = nowVisible
            ? "Nota de estudio flotante activa (clic en citas para saltar al origen)"
            : "Nota de estudio flotante oculta";
        break;
    }
    case CMD_FLEXCIL_GESTURE_MODE: {
        const bool nextGesture = !st->flexcilOverlay.IsGestureModeActive();
        st->flexcilOverlay.SetGestureModeActive(nextGesture);
        if (nextGesture) st->flexcilOverlay.SetPopupNoteVisible(true);
        st->statusBannerUtf8 = nextGesture
            ? "Modo Gestos activo (trace L, rect\xC3\xA1ngulo o subrayado sobre el documento)"
            : "Modo L\xC3\xA1piz normal activo";
        break;
    }
    case CMD_SELECTIVE_PDF_EXPORT: {
        PdfSelectiveExportOptions opts{};
        std::wstring outPath;
        if (FeatureDialogs::ShowSelectivePdfExportDialog(st->hwndMain, (std::max<std::size_t>)(1u, st->doc.Pages().size()), opts, outPath)) {
            std::vector<std::uint8_t> pdfBytes;
            std::string err;
            if (FlexcilStudyOverlay::ExportSelectivePdfToBytes(st->doc, opts, pdfBytes, &err)) {
                std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
                ofs.write(reinterpret_cast<const char*>(pdfBytes.data()), static_cast<std::streamsize>(pdfBytes.size()));
                st->statusBannerUtf8 = opts.flattened
                    ? "PDF selectivo exportado (aplanado)"
                    : "PDF selectivo exportado con carga editable reversible";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error al exportar PDF", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_HANDWRITING_REFLOW: {
        if (IsDocumentLockedForEdit(st)) break;
        const ObjectTarget tgt = CurrentTarget(st);
        const std::size_t moved = SmartScriptReflowEngine::ReflowHandwrittenWordsToWidth(st->doc, st->cmdStack, tgt, 48.0, 48.0, 460.0);
        st->statusBannerUtf8 = "Reflujo manuscrito aplicado (" + std::to_string(moved) + " palabra(s) reposicionada(s))";
        break;
    }
    case CMD_PASTE_AS_HANDWRITING: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring txtW = L"Apunte transformado a caligraf\u00EDa vectorial";
        if (ShowTextPrompt(st->hwndMain, L"Pegar Texto como Escritura Manuscrita",
                L"Texto a sintetizar como trazos de tinta vectorial legible:", txtW)) {
            double x = 72.0, y = 96.0;
            NextCardPlacement(st, x, y);
            const auto ids = SmartScriptReflowEngine::PasteTextAsHandwriting(
                st->doc, st->cmdStack, CurrentTarget(st), WideToUtf8(txtW), x, y);
            st->statusBannerUtf8 = "Texto pegado como escritura manuscrita (" + std::to_string(ids.size()) + " trazo(s))";
        }
        break;
    }
    case CMD_HANDWRITING_SPELLING: {
        if (IsDocumentLockedForEdit(st)) break;
        const auto issues = SmartScriptReflowEngine::CheckHandwritingSpelling(st->doc, CurrentTarget(st));
        if (issues.empty()) {
            st->statusBannerUtf8 = "Ortograf\xC3\xAD""a manuscrita verificada: 0 errores detectados";
        } else {
            SmartScriptReflowEngine::ApplyHandwritingSpellingCorrection(
                st->doc, st->cmdStack, CurrentTarget(st), issues.front());
            st->statusBannerUtf8 = "Correcci\xC3\xB3n ortogr\xC3\xA1""fica aplicada: " +
                issues.front().misspelledWordUtf8 + " -> " + issues.front().correctedWordUtf8;
        }
        break;
    }
    case CMD_ZOOM_WRITING_BOX: {
        st->zoomWritingBox.active = !st->zoomWritingBox.active;
        st->statusBannerUtf8 = st->zoomWritingBox.active
            ? "Caja de Zoom activa: escriba en el panel inferior (auto-avance al llegar a la zona azul)"
            : "Caja de Zoom desactivada";
        break;
    }
    case CMD_REACTIVE_MATH_EVAL: {
        if (IsDocumentLockedForEdit(st)) break;
        const auto lines = MathChem::ReactiveMathChemistryEngine::EvaluateReactiveEqualsOnDocument(
            st->doc, CurrentTarget(st));
        st->statusBannerUtf8 = "Matem\xC3\xA1ticas reactivas (=) actualizadas: " + std::to_string(lines.size()) + " expresi\xC3\xB3n(es) resuelta(s)";
        break;
    }
    case CMD_CHEMISTRY_BALANCER: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring chemW = L"KMnO4 + HCl -> KCl + MnCl2 + Cl2 + H2O";
        if (ShowTextPrompt(st->hwndMain, L"Reconocimiento y Balanceo Qu\u00EDmico 2D (118 elementos)",
                L"F\u00F3rmula o ecuaci\u00F3n qu\u00EDmica:", chemW)) {
            const auto res = MathChem::ReactiveMathChemistryEngine::AnalyzeAndBalanceChemistry(WideToUtf8(chemW));
            if (res.valid) {
                double x = 80.0, y = 80.0;
                NextCardPlacement(st, x, y);
                AnnotationObject node;
                node.kind = ObjectKind::MathBlock;
                node.layerId = st->doc.ActiveLayerId();
                node.bounds = { x, y, 380.0, 84.0 };
                node.formulaExprUtf8 = res.balancedEquationUtf8;
                node.evaluatedResultUtf8 = res.latexUtf8;
                node.textUtf8 = res.natural2DUtf8;
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(node)));
                st->statusBannerUtf8 = "Qu\xC3\xADmica 2D: " + res.natural2DUtf8;
            } else {
                st->statusBannerUtf8 = "Ecuaci\xC3\xB3n qu\xC3\xADmica no reconocida o sin balance entero";
            }
        }
        break;
    }
    case CMD_CONTENT_VIEWS: {
        st->contentViews.UpsertFromDocument(st->doc, WideToUtf8(st->currentDocPath));
        std::wstring openPath;
        bool mutated = false;
        const bool wantOpen = ClosureDialogs::ShowFullLibraryManagerDialog(st->hwndMain, st->contentViews, openPath, mutated);
        if (mutated) PersistStudioProfileNow(st);
        if (wantOpen && !openPath.empty()) {
            OpenDocumentFile(st, openPath);
        } else {
            st->statusBannerUtf8 = "Biblioteca y Vistas de Contenido actualizadas (" + std::to_string(st->contentViews.Items().size()) + " elementos)";
        }
        break;
    }
    case CMD_AUDIO_DIARIZE: {
        st->audioTimelineVisible = true;
        st->audioSession.TranscribeOffline();
        const auto segs = st->audioSession.DiarizeMultiSpeakerTranscript();
        if (!segs.empty()) {
            std::ostringstream oss;
            oss << "Transcripci\xC3\xB3n Multi-Hablante (" << segs.size() << " segmentos)\n";
            for (const auto& s : segs) {
                oss << "[" << (s.startMs / 1000) << "s] " << s.speakerIdUtf8 << ": " << s.textUtf8 << "\n";
            }
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { x, y, 420.0, 160.0 };
            tb.textUtf8 = oss.str();
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
        }
        st->statusBannerUtf8 = "Transcripci\xC3\xB3n y diarizaci\xC3\xB3n: " + std::to_string(segs.size()) + " segmento(s)";
        break;
    }
    case CMD_MEETING_MINUTES_ICS: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring icsPath;
        if (!FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"Calendario iCalendar (*.ics)\0*.ics\0Todos (*.*)\0*.*\0\0",
                L"Seleccionar evento de calendario (.ics)", icsPath)) {
            break;
        }
        std::ifstream ifs(icsPath, std::ios::binary);
        if (!ifs.is_open()) {
            st->statusBannerUtf8 = "No se pudo leer el archivo .ics seleccionado";
            break;
        }
        const std::string icsContent((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        const auto segs = st->audioSession.DiarizeMultiSpeakerTranscript();
        const auto rep = AudioTimelineSession::GenerateCalendarMeetingNotesFromIcs(icsContent, segs, st->doc.Title());
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.layerId = st->doc.ActiveLayerId();
        tb.bounds = { x, y, 440.0, 200.0 };
        tb.textUtf8 = rep.combinedMarkdownUtf8;
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
        st->statusBannerUtf8 = "Acta generada desde archivo .ics: " + rep.eventTitleUtf8;
        break;
    }
    case CMD_ANCHORED_COMMENTS: {
        bool mutated = false;
        FeatureDialogs::ShowCommentsManagerDialog(st->hwndMain, st->commentThreads, st->activePageIndex, st->selectedObjectId, mutated);
        st->commentsPanelVisible = !st->commentThreads.AllThreads().empty();
        if (mutated) st->doc.BumpRevision();
        st->statusBannerUtf8 = "Comentarios anclados: " + std::to_string(st->commentThreads.ActiveThreads().size()) + " activo(s)";
        break;
    }
    case CMD_TOGGLE_COMMENTS_PANEL: {
        st->commentsPanelVisible = !st->commentsPanelVisible;
        st->statusBannerUtf8 = st->commentsPanelVisible ? "Panel de comentarios visible" : "Panel de comentarios oculto";
        break;
    }
    case CMD_STATIC_SITE_PUBLISH: {
        std::wstring outDir;
        if (!FeatureDialogs::PickFolderWin32(st->hwndMain, L"Seleccionar carpeta para publicar sitio HTML5", outDir)) break;
        if (st->knowledgeVault.TotalNoteCount() == 0) {
            RebuildReadingViewText(st);
            const std::string title = st->doc.Title().empty() ? "Cuaderno" : st->doc.Title();
            st->knowledgeVault.CreateNote(title, "Publico", "---\npublish: true\n---\n# " + title + "\n\n" + st->cachedReadingTextUtf8);
        }
        const auto rep = StaticSitePublisher::PublishVaultToStaticHtml(
            st->knowledgeVault, {}, outDir, nullptr, st->uiPrefs.customCssSnippetUtf8);
        if (rep.ok) {
            st->statusBannerUtf8 = "Sitio HTML5 publicado (" + std::to_string(rep.publishedNoteCount) + " notas p\xC3\xBA" "blicas, " +
                std::to_string(rep.skippedPrivateNoteCount) + " privadas omitidas)";
            const std::wstring msg = L"Sitio publicado en:\n" + Utf8ToWide(rep.indexHtmlPathUtf8) +
                L"\n\nNotas p\u00FAblicas: " + std::to_wstring(rep.publishedNoteCount) +
                L"\nPrivadas omitidas: " + std::to_wstring(rep.skippedPrivateNoteCount) +
                L"\n\n\u00BFDesea abrir index.html en el navegador?";
            if (MessageBoxW(st->hwndMain, msg.c_str(), L"Sitio Web Est\u00E1tico Publicado", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
                ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(rep.indexHtmlPathUtf8).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else {
            st->statusBannerUtf8 = "No se pudo publicar el sitio (verifique que existan notas con publish: true)";
        }
        break;
    }
    case CMD_FOLDER_SYNC_3MODES: {
        const std::wstring localDir = StudioProfileStore::DefaultProfileRootDir();
        PersistStudioProfileNow(st);
        std::wstring targetDir;
        FolderSyncMode mode = FolderSyncMode::MergeLossless;
        SelectiveSyncFilter filter{};
        if (FeatureDialogs::ShowFolderSyncDialog(st->hwndMain, localDir, targetDir, mode, filter)) {
            const auto rep = LocalFirstSyncEngine::SynchronizeDirectories(localDir, targetDir, mode, filter);
            std::ostringstream oss;
            oss << rep.diagnosticUtf8 << "\n\n"
                << "  - Subidos: " << rep.uploadedFiles << "\n"
                << "  - Descargados: " << rep.downloadedFiles << "\n"
                << "  - Conflictos preservados: " << rep.mergedConflicts << "\n"
                << "  - Respaldados antes de sobrescribir: " << rep.backedUpBeforeChange << "\n"
                << "  - Omitidos por filtro: " << rep.skippedBySelectiveFilter << "\n";
            if (!rep.backupDirUtf8.empty()) oss << "  - Respaldo: " << rep.backupDirUtf8 << "\n";
            MessageBoxW(st->hwndMain, Utf8ToWide(oss.str()).c_str(), L"Resultado de sincronizaci\u00F3n",
                MB_OK | (rep.ok ? MB_ICONINFORMATION : MB_ICONWARNING));
            st->statusBannerUtf8 = rep.diagnosticUtf8;
        }
        break;
    }
    case CMD_WEB_CLIPPER_CAPTURE: {
        if (IsDocumentLockedForEdit(st)) break;
        WebClipCaptureResult clipped{};
        std::string err;
        if (FeatureDialogs::ShowWebClipperDialog(st->hwndMain, clipped, err) && clipped.ok) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { x, y, 420.0, 180.0 };
            tb.textUtf8 = clipped.markdownOutputUtf8;
            tb.captionUtf8 = clipped.extractedTitleUtf8;
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
            st->knowledgeVault.CreateNote(clipped.extractedTitleUtf8.empty() ? "Captura Web" : clipped.extractedTitleUtf8,
                "WebClipper", clipped.markdownOutputUtf8);
            PersistStudioProfileNow(st);
            st->statusBannerUtf8 = "Captura Web Clipper insertada: " + clipped.extractedTitleUtf8;
        }
        break;
    }
    case CMD_JSON_CANVAS_EXPORT: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain,
                L"JSON Canvas 1.0 (*.canvas)\0*.canvas\0JSON (*.json)\0*.json\0\0", L"canvas", L"lienzo.canvas",
                L"Exportar JSON Canvas 1.0 (.canvas)", outPath)) {
            const std::string jCanvas = UniversalFormatsInterop::ExportToJsonCanvas(st->doc);
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            ofs.write(jCanvas.data(), static_cast<std::streamsize>(jCanvas.size()));
            st->statusBannerUtf8 = "Exportado a JSON Canvas 1.0 (.canvas)";
        }
        break;
    }
    case CMD_IMPORT_JSON_CANVAS: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"JSON Canvas 1.0 (*.canvas;*.json)\0*.canvas;*.json\0\0", L"Importar JSON Canvas 1.0", inPath)) {
            std::ifstream ifs(inPath, std::ios::binary);
            const std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            std::string err;
            if (UniversalFormatsInterop::ImportFromJsonCanvas(raw, st->doc, &err)) {
                st->statusBannerUtf8 = "JSON Canvas 1.0 importado al lienzo";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error importando .canvas", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_GROUNDED_AI_CITATIONS: {
        const auto segs = st->audioSession.DiarizeMultiSpeakerTranscript();
        const LocalLlmConfig* llmPtr = st->uiPrefs.llmConfig.enabled ? &st->uiPrefs.llmConfig : nullptr;
        std::string insertMd;
        int jumpPage0 = -1;
        if (ClosureDialogs::ShowGroundedAiStudioDialog(st->hwndMain, st->doc, segs, llmPtr, insertMd, &jumpPage0, &st->aiVault, &st->speechSynth)) {
            PersistStudioProfileNow(st);
            if (!insertMd.empty() && !IsDocumentLockedForEdit(st)) {
                double x = 80.0, y = 80.0;
                NextCardPlacement(st, x, y);
                AnnotationObject tb;
                tb.kind = ObjectKind::TextBox;
                tb.layerId = st->doc.ActiveLayerId();
                tb.bounds = { x, y, 420.0, 180.0 };
                tb.textUtf8 = insertMd;
                tb.captionUtf8 = "Respuesta Asistente";
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
                st->statusBannerUtf8 = "Respuesta del asistente insertada en el documento";
            }
            if (jumpPage0 >= 0 && static_cast<std::size_t>(jumpPage0) < st->doc.Pages().size()) {
                st->doc.SetMode(DocumentMode::PaginatedPdf);
                CenterCameraOnPage(st, static_cast<std::size_t>(jumpPage0));
                st->statusBannerUtf8 = "Saltado a cita en p\xC3\xA1gina " + std::to_string(jumpPage0 + 1);
            }
        }
        break;
    }
    case CMD_AUTO_QUIZ_GENERATOR: {
        const auto quiz = GroundedStudyAiAssistant::GenerateMultipleChoiceQuiz(st->doc, 5);
        if (quiz.empty()) {
            st->statusBannerUtf8 = "Agregue apuntes o f\xC3\xB3rmulas al documento para generar un cuestionario";
            break;
        }
        QuizGradeReport grade{};
        int jumpPage0 = -1;
        if (FeatureDialogs::ShowInteractiveQuizDialog(st->hwndMain, quiz, grade, &jumpPage0)) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Cuestionario: %zu/%zu correctas (%.0f%%)",
                grade.correctCount, grade.totalQuestions, grade.scorePercent);
            st->statusBannerUtf8 = buf;
            MessageBoxW(st->hwndMain, Utf8ToWide(std::string(buf)).c_str(), L"Resultado del Cuestionario", MB_ICONINFORMATION);
        }
        if (jumpPage0 >= 0 && static_cast<std::size_t>(jumpPage0) < st->doc.Pages().size()) {
            st->doc.SetMode(DocumentMode::PaginatedPdf);
            CenterCameraOnPage(st, static_cast<std::size_t>(jumpPage0));
        }
        break;
    }
    case CMD_LOCK_NOTE_PASSPHRASE: {
        std::wstring passW;
        if (!st->permissions.IsNoteLocked()) {
            if (ShowTextPrompt(st->hwndMain, L"Bloquear Nota con Contrase\u00F1a (PBKDF2-SHA256)",
                    L"Establezca una contrase\u00F1a (almacenada con hash seguro CNG):", passW, nullptr, nullptr, false, true) && !passW.empty()) {
                if (st->permissions.LockNoteWithPassphrase(WideToUtf8(passW))) {
                    st->doc.BumpRevision();
                    st->statusBannerUtf8 = "Nota bloqueada con PBKDF2-HMAC-SHA256 (modo solo lectura)";
                }
            }
        } else {
            if (ShowTextPrompt(st->hwndMain, L"Desbloquear Nota",
                    L"Ingrese la contrase\u00F1a para habilitar edici\u00F3n:", passW, nullptr, nullptr, false, true)) {
                if (st->permissions.UnlockNoteWithPassphrase(WideToUtf8(passW))) {
                    st->doc.BumpRevision();
                    st->statusBannerUtf8 = "Nota desbloqueada correctamente";
                } else {
                    MessageBoxW(st->hwndMain, L"Contrase\u00F1a incorrecta.", L"Bloqueo de Nota", MB_ICONWARNING);
                    st->statusBannerUtf8 = "Contrase\u00F1a incorrecta";
                }
            }
        }
        break;
    }
    case CMD_EXPORT_SVG: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Gr\u00E1fico Vectorial SVG (*.svg)\0*.svg\0\0", L"svg", L"pagina.svg",
                L"Exportar P\u00E1gina a SVG", outPath)) {
            const std::string svg = UniversalFormatsInterop::ExportPageToSvg(st->doc, st->activePageIndex);
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            ofs.write(svg.data(), static_cast<std::streamsize>(svg.size()));
            st->statusBannerUtf8 = "P\xC3\xA1gina exportada a SVG vectorial";
        }
        break;
    }
    case CMD_EXPORT_PAGE_IMAGE: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain,
                L"Imagen PNG (*.png)\0*.png\0Imagen JPEG (*.jpg)\0*.jpg\0\0", L"png", L"pagina.png",
                L"Exportar P\u00E1gina como Imagen", outPath)) {
            const bool asJpeg = (GetFileExtLower(outPath) == L".jpg" || GetFileExtLower(outPath) == L".jpeg");
            std::string err;
            if (FlexcilStudyOverlay::ExportPageToPngOrJpegFile(st->doc, st->activePageIndex, outPath, asJpeg, 1600, &err)) {
                st->statusBannerUtf8 = asJpeg ? "P\xC3\xA1gina exportada como JPEG" : "P\xC3\xA1gina exportada como PNG";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando imagen", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_TOGGLE_LAN_COLLAB_SERVER: {
        if (st->lanServer.IsRunning()) {
            st->lanServer.Stop();
            st->statusBannerUtf8 = "Servidor colaborativo LAN detenido";
        } else {
            std::string err;
            const bool ok = st->lanServer.Start(
                0,
                false,
                [st]() -> std::string {
                    nlohmann::json j = {
                        {"title", st->doc.Title()},
                        {"pages", st->doc.Pages().size()},
                        {"locked", st->permissions.IsNoteLocked()},
                        {"comments", nlohmann::json::parse(st->commentThreads.SerializeToJson(), nullptr, false)}
                    };
                    return j.dump();
                },
                [st](const std::string& author, const std::string& body, std::size_t pg) -> bool {
                    if (st->permissions.IsNoteLocked()) return false;
                    st->commentThreads.CreateThread(pg, 0, { 72.0, 72.0, 160.0, 44.0 }, author, body);
                    return true;
                },
                &err);
            if (ok) {
                const std::string url = st->lanServer.BaseUrl();
                CanvasManip::CopyTextToClipboardWin32(st->hwndMain, url);
                st->statusBannerUtf8 = "Servidor LAN activo en puerto " + std::to_string(st->lanServer.BoundPort());
                MessageBoxW(st->hwndMain,
                    Utf8ToWide("Servidor colaborativo LAN iniciado (sin nube).\n\nURL con token (copiada al portapapeles):\n" + url).c_str(),
                    L"Colaboraci\u00F3n LAN Local", MB_ICONINFORMATION);
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error Servidor LAN", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_AI_SETTINGS:
    case CMD_AI_PROVIDERS_VAULT: {
        if (FeatureDialogs::ShowAiAndAsrSettingsDialog(st->hwndMain, st->uiPrefs, &st->aiVault)) {
            PersistStudioProfileNow(st);
            const auto* act = st->aiVault.FindProvider(st->aiVault.ActiveProviderId());
            st->statusBannerUtf8 = (st->aiVault.IsEnabled() && act)
                ? ("IA activa: " + act->displayNameUtf8 + " (" + act->modelNameUtf8 + ")")
                : "IA en modo local determinista (BM25)";
        }
        break;
    }
    case CMD_TOOLBAR_ORIENTATION: {
        st->uiPrefs.toolbarOrientation = (st->uiPrefs.toolbarOrientation + 1) % 3;
        PersistStudioProfileNow(st);
        static const char* kOrient[3] = { "Superior", "Izquierda", "Derecha" };
        st->statusBannerUtf8 = std::string("Orientaci\xC3\xB3n de barra de herramientas: ") + kOrient[st->uiPrefs.toolbarOrientation];
        LayoutMainChildren(st);
        break;
    }
    case CMD_EDIT_CUSTOM_CSS: {
        std::wstring cssW = Utf8ToWide(st->uiPrefs.customCssSnippetUtf8.empty()
            ? "/* CSS personalizado para Sitio Publicado y Exportaci\xC3\xB3n HTML */\nbody { font-size: 16px; }\n"
            : st->uiPrefs.customCssSnippetUtf8);
        if (ShowTextPrompt(st->hwndMain, L"Fragmentos CSS Personalizados",
                L"Reglas CSS aplicadas a publicaci\u00F3n HTML5 y exportaci\u00F3n:", cssW, nullptr, nullptr, true, false)) {
            st->uiPrefs.customCssSnippetUtf8 = WideToUtf8(cssW);
            PersistStudioProfileNow(st);
            st->statusBannerUtf8 = "CSS personalizado guardado";
        }
        break;
    }
    case CMD_AUDIO_NOISE_REDUCTION: {
        st->audioTimelineVisible = true;
        if (!st->audioSession.Samples().empty()) {
            st->audioSession.ApplyNoiseReductionHighPass();
            st->statusBannerUtf8 = "Reducci\xC3\xB3n de ruido (filtro paso-alto) aplicada al audio";
        } else {
            st->statusBannerUtf8 = "Grabe o importe audio antes de aplicar reducci\xC3\xB3n de ruido";
        }
        break;
    }
    case CMD_AUDIO_SPEED_CYCLE: {
        st->audioTimelineVisible = true;
        static constexpr double kSpeeds[] = { 0.75, 1.0, 1.25, 1.5, 2.0, 2.5 };
        double cur = st->audioSession.PlaybackSpeed();
        double next = 1.0;
        for (std::size_t i = 0; i < std::size(kSpeeds); ++i) {
            if (cur < kSpeeds[i] - 0.05) { next = kSpeeds[i]; break; }
            if (i + 1 == std::size(kSpeeds)) next = kSpeeds[0];
        }
        st->audioSession.SetPlaybackSpeed(next);
        char sb[64];
        std::snprintf(sb, sizeof(sb), "Velocidad de reproducci\xC3\xB3n: %.2fx", next);
        st->statusBannerUtf8 = sb;
        break;
    }
    case CMD_AUDIO_SKIP_BACK: {
        st->audioTimelineVisible = true;
        const std::uint64_t cur = st->replayActive ? st->replayCursorMs : st->audioSession.CursorMs();
        const std::uint64_t target = (cur > 10000u) ? (cur - 10000u) : 0u;
        st->audioSession.SeekMs(target);
        st->replayCursorMs = target;
        st->statusBannerUtf8 = "Audio retrocedido a " + std::to_string(target / 1000) + " s";
        break;
    }
    case CMD_AUDIO_SKIP_FWD: {
        st->audioTimelineVisible = true;
        const std::uint64_t cur = st->replayActive ? st->replayCursorMs : st->audioSession.CursorMs();
        const std::uint64_t dur = st->audioSession.DurationMs();
        const std::uint64_t target = (dur > 0) ? (std::min)(dur, cur + 10000u) : (cur + 10000u);
        st->audioSession.SeekMs(target);
        st->replayCursorMs = target;
        st->statusBannerUtf8 = "Audio adelantado a " + std::to_string(target / 1000) + " s";
        break;
    }
    case CMD_EXPORT_ANKI_DECK: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Mazo Anki TSV (*.tsv)\0*.tsv\0\0", L"tsv", L"tarjetas_anki.tsv",
                L"Exportar Tarjetas a Anki (.tsv)", outPath)) {
            const std::string tsv = UniversalFormatsInterop::ExportStudyDeckToDelimited(st->activeDeck, '\t');
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            ofs.write(tsv.data(), static_cast<std::streamsize>(tsv.size()));
            st->statusBannerUtf8 = "Mazo exportado a Anki (.tsv)";
        }
        break;
    }
    case CMD_EXPORT_ONENOTE_HTML: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"HTML OneNote (*.html)\0*.html\0\0", L"html", L"cuaderno_onenote.html",
                L"Exportar HTML Compatible OneNote", outPath)) {
            const std::string html = UniversalFormatsInterop::ExportPageToOneNoteGraphHtml(st->doc, st->activePageIndex);
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            ofs.write(html.data(), static_cast<std::streamsize>(html.size()));
            st->statusBannerUtf8 = "Documento exportado a HTML compatible OneNote";
        }
        break;
    }
    case CMD_IMPORT_UNIVERSAL_PKG: {
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"Paquetes de Notas (*.cuadernu-pkg;*.goodnotes;*.note;*.nebo;*.ntb;*.nsa)\0*.cuadernu-pkg;*.goodnotes;*.note;*.nebo;*.ntb;*.nsa\0\0",
                L"Importar Paquete de Notas Universal", inPath)) {
            DocumentModel imported;
            std::string detectedFmt, err;
            if (UniversalFormatsInterop::ImportUniversalNotePackage(inPath, imported, &detectedFmt, &err)) {
                st->doc = std::move(imported);
                st->cmdStack.Clear();
                st->pageCache.InvalidateAll(++st->asyncGen);
                CenterCameraOnPage(st, 0);
                st->statusBannerUtf8 = "Paquete importado (" + detectedFmt + ")";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Importaci\u00F3n de Paquete", MB_ICONWARNING);
            }
        }
        break;
    }
    case CMD_EXPORT_UNIVERSAL_PKG: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain,
                L"Paquete Portable CuadernU (*.cuadernu-pkg)\0*.cuadernu-pkg\0\0", L"cuadernu-pkg", L"cuaderno.cuadernu-pkg",
                L"Exportar Paquete Portable", outPath)) {
            std::string err;
            if (UniversalFormatsInterop::ExportUniversalNotePackage(st->doc, outPath, "cuadernu-pkg", &err)) {
                st->statusBannerUtf8 = "Paquete portable .cuadernu-pkg exportado";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando paquete", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_TRANSLATE_SELECTION: {
        std::wstring srcW = L"";
        if (st->selectedObjectId != 0) {
            if (const auto* o = CanvasManip::FindObjectConst(st->doc, CurrentTarget(st), st->selectedObjectId)) {
                srcW = Utf8ToWide(o->textUtf8);
            }
        }
        std::wstring langW = L"en";
        if (ShowTextPrompt(st->hwndMain, L"Traducir Texto (ES <-> EN)",
                L"Texto a traducir:", srcW, L"Idioma destino (en / es):", &langW) && !srcW.empty()) {
            const LocalLlmConfig* llmPtr = st->uiPrefs.llmConfig.enabled ? &st->uiPrefs.llmConfig : nullptr;
            const AiProviderVault* vaultPtr = st->aiVault.IsEnabled() ? &st->aiVault : nullptr;
            const std::string tr = GroundedStudyAiAssistant::TranslateSelectionOffline(WideToUtf8(srcW), WideToUtf8(langW), llmPtr, vaultPtr);
            CanvasManip::CopyTextToClipboardWin32(st->hwndMain, tr);
            MessageBoxW(st->hwndMain, Utf8ToWide(tr + "\n\n(Copiado al portapapeles)").c_str(), L"Traducci\u00F3n", MB_ICONINFORMATION);
            st->statusBannerUtf8 = "Traducci\xC3\xB3n completada y copiada al portapapeles";
        }
        break;
    }
    case CMD_BACKUP_RESTORE_MGR: {
        PersistStudioProfileNow(st);
        ClosureDialogs::ShowBackupRestoreDialog(st->hwndMain, StudioProfileStore::DefaultProfileRootDir(), st->statusBannerUtf8);
        break;
    }
    case CMD_NOTE_COVER_PAPER: {
        std::wstring colW = L"1D4ED8", papW = L"grid";
        if (ShowTextPrompt(st->hwndMain, L"Portada y Papel de la Nota",
                L"Color de portada HEX (ej. 1D4ED8, 059669, DC2626):", colW,
                L"Papel (plain / ruled / grid / dotted / cornell / isometric):", &papW)) {
            const std::uint32_t rgb = static_cast<std::uint32_t>(std::wcstoul(colW.c_str(), nullptr, 16));
            const std::uint32_t rgba = (rgb << 8) | 0xFFu;
            const std::string ps = WideToUtf8(papW);
            PaperPattern pp = PaperPattern::Grid;
            if (ps == "plain") pp = PaperPattern::Plain;
            else if (ps == "ruled") pp = PaperPattern::Ruled;
            else if (ps == "dotted") pp = PaperPattern::Dots;
            else if (ps == "cornell") pp = PaperPattern::Cornell;
            else if (ps == "isometric") pp = PaperPattern::Isometric;
            if (st->activePageIndex < st->doc.Pages().size()) {
                st->doc.MutablePages()[st->activePageIndex].background = pp;
            }
            const std::string docTitle = st->doc.Title().empty() ? "Cuaderno" : st->doc.Title();
            if (const auto* existing = st->knowledgeVault.FindNoteByTitleOrAlias(docTitle)) {
                st->knowledgeVault.SetNoteCoverAndPaper(existing->noteIdUtf8, rgba, pp);
            } else {
                const std::string nid = st->knowledgeVault.CreateNote(docTitle, "Cuaderno", "", {}, pp, rgba);
                st->knowledgeVault.SetNoteCoverAndPaper(nid, rgba, pp);
            }
            st->doc.BumpRevision();
            PersistStudioProfileNow(st);
            st->statusBannerUtf8 = "Portada y estilo de papel actualizados";
        }
        break;
    }
    case CMD_SHARE_LINKS_MGR: {
        if (ClosureDialogs::ShowShareLinksAndPermissionsDialog(
                st->hwndMain, st->permissions, st->doc.Title(), st->activePageIndex, st->statusBannerUtf8)) {
            st->doc.BumpRevision();
        }
        break;
    }
    case CMD_GRAPH_LOCAL_FILTER: {
        std::wstring tagW = Utf8ToWide(st->graphFilter.requiredTagUtf8);
        std::wstring foldW = Utf8ToWide(st->graphFilter.folderPrefixUtf8);
        if (ShowTextPrompt(st->hwndMain, L"Grafo Local y Filtros de Nodos",
                L"Filtrar por etiqueta (vac\u00EDo = todas):", tagW,
                L"Filtrar por carpeta (o 'local' para alternar Grafo Local):", &foldW)) {
            st->graphFilter.requiredTagUtf8 = WideToUtf8(tagW);
            const std::string fw = WideToUtf8(foldW);
            if (fw == "local") {
                st->graphLocalMode = !st->graphLocalMode;
            } else {
                st->graphFilter.folderPrefixUtf8 = fw;
                st->graphLocalMode = true;
            }
            if (st->knowledgeVault.TotalNoteCount() == 0) {
                st->knowledgeVault.CreateNote(st->doc.Title(), "Cuaderno", "Conectado con [[Resumen]].");
                st->knowledgeVault.CreateNote("Resumen", "Cuaderno", "Ver [[" + st->doc.Title() + "]].");
            }
            st->activeGraphView = st->graphLocalMode
                ? st->knowledgeVault.BuildLocalGraph(st->doc.Title(), 2, st->graphFilter)
                : st->knowledgeVault.BuildGlobalGraph(st->graphFilter);
            KnowledgeVault::ComputeGraphLayout2D(st->activeGraphView, 60, 800.0, 560.0);
            st->graphOverlayVisible = true;
            st->statusBannerUtf8 = (st->graphLocalMode ? "Grafo Local (" : "Grafo Global (") +
                std::to_string(st->activeGraphView.nodes.size()) + " nodos)";
        }
        break;
    }
    case CMD_VAULT_SEARCH_RENAME: {
        std::string picked;
        bool mutated = false;
        if (ClosureDialogs::ShowVaultSearchRenameDialog(st->hwndMain, st->knowledgeVault, picked, mutated, st->statusBannerUtf8)) {
            if (const auto* n = st->knowledgeVault.FindNoteByTitleOrAlias(picked)) {
                st->cachedReadingTextUtf8 = n->bodyMarkdownUtf8;
                st->readingViewMode = true;
                st->statusBannerUtf8 = "Nota abierta desde B\xC3\xB3veda: " + n->titleUtf8;
            }
        }
        if (mutated) PersistStudioProfileNow(st);
        break;
    }
    case CMD_DOCUMENT_OUTLINE: {
        RebuildReadingViewText(st);
        const RichBlockDocument rbd = RichBlockDocument::FromMarkdown(st->cachedReadingTextUtf8);
        const auto outline = rbd.ExtractOutline();
        const auto hls = rbd.ExtractHighlights();
        std::size_t words = 0;
        {
            std::istringstream ws(st->cachedReadingTextUtf8);
            std::string tok;
            while (ws >> tok) ++words;
        }
        std::ostringstream oss;
        oss << "Documento: " << st->doc.Title() << "\r\n"
            << "Palabras: " << words << "  |  Caracteres: " << st->cachedReadingTextUtf8.size()
            << "  |  Resaltados: " << hls.size() << "\r\n\r\n"
            << "Esquema de encabezados (" << outline.size() << "):\r\n";
        for (const auto& h : outline) {
            oss << std::string(static_cast<std::size_t>((std::max)(1, h.level)) * 2u, ' ')
                << "- " << h.titleUtf8 << " (L" << (h.lineIndex0 + 1) << ")\r\n";
        }
        if (outline.empty()) oss << "  (Sin encabezados Markdown # en el documento)\r\n";
        GuiDialogs::ShowReportDialog(st->hwndMain, L"Esquema y Contador de Palabras", Utf8ToWide(oss.str()));
        st->statusBannerUtf8 = "Esquema: " + std::to_string(outline.size()) + " encabezados, " + std::to_string(words) + " palabras";
        break;
    }
    case CMD_DB_EDITOR_MGR: {
        if (IsDocumentLockedForEdit(st)) break;
        if (ClosureDialogs::ShowDatabaseEditorDialog(st->hwndMain, st->activeDatabase, st->knowledgeVault, st->automations, st->statusBannerUtf8)) {
            st->dbPanelVisible = true;
            st->doc.BumpRevision();
            PersistStudioProfileNow(st);
        }
        break;
    }
    case CMD_DB_SUBMIT_FORM: {
        if (IsDocumentLockedForEdit(st)) break;
        if (st->activeDatabase.Columns().empty()) ExecuteToolbarCommand(st, CMD_INSERT_STRUCTURED_DB);
        if (ClosureDialogs::ShowDatabaseFormDialog(st->hwndMain, st->activeDatabase, st->statusBannerUtf8)) {
            st->dbPanelVisible = true;
            st->doc.BumpRevision();
        }
        break;
    }
    case CMD_DB_ACTION_BUTTON: {
        if (IsDocumentLockedForEdit(st)) break;
        if (st->activeDatabase.Records().empty()) {
            st->statusBannerUtf8 = "No hay registros en la Base de Datos para ejecutar el bot\xC3\xB3n de acci\xC3\xB3n";
            break;
        }
        if (st->automations.Rules().empty()) {
            AutomationRule btnRule{};
            btnRule.ruleIdUtf8 = "action_btn_1";
            btnRule.nameUtf8 = "Marcar revisado";
            btnRule.trigger = AutomationTriggerKind::OnActionButtonClicked;
            btnRule.steps.push_back({ "set_property", "estado", "Completado" });
            st->automations.RegisterRule(std::move(btnRule));
        }
        const std::size_t fired = st->automations.TriggerEvent(
            AutomationTriggerKind::OnActionButtonClicked,
            st->activeDatabase,
            st->activeDatabase.Records().front().recordIdUtf8);
        st->activeDatabase.RecomputeFormulasAndRollups(&st->knowledgeVault);
        st->doc.BumpRevision();
        st->statusBannerUtf8 = "Bot\xC3\xB3n de acci\xC3\xB3n ejecutado (" + std::to_string(fired) + " regla(s) disparada(s))";
        break;
    }
    case CMD_AUDIO_PAUSE_RESUME: {
        st->audioTimelineVisible = true;
        if (st->audioSession.IsPaused()) {
            st->audioSession.ResumeRecording();
            st->statusBannerUtf8 = "Grabaci\xC3\xB3n de audio reanudada";
        } else {
            st->audioSession.PauseRecording();
            st->statusBannerUtf8 = "Grabaci\xC3\xB3n de audio en pausa";
        }
        break;
    }
    case CMD_AUDIO_SEARCH_SEEK: {
        st->audioTimelineVisible = true;
        std::wstring qW = L"";
        if (ShowTextPrompt(st->hwndMain, L"Buscar en Transcripci\u00F3n de Audio", L"Palabra o frase:", qW) && !qW.empty()) {
            const auto segs = st->audioSession.DiarizeMultiSpeakerTranscript();
            std::uint64_t matchedMs = 0;
            if (st->audioSession.SearchTranscriptAndSeek(WideToUtf8(qW), segs, &matchedMs)) {
                st->replayCursorMs = matchedMs;
                st->statusBannerUtf8 = "Audio posicionado en coincidencia (" + std::to_string(matchedMs / 1000) + " s)";
            } else {
                st->statusBannerUtf8 = "Sin coincidencias en la transcripci\xC3\xB3n actual";
            }
        }
        break;
    }
    case CMD_AUDIO_EXPORT_WAV: {
        std::wstring outWav;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Audio WAV (*.wav)\0*.wav\0\0", L"wav", L"grabacion.wav", L"Exportar Audio WAV", outWav)) {
            std::string err;
            if (st->audioSession.ExportWavFile(WideToUtf8(outWav), &err)) {
                st->statusBannerUtf8 = "Audio exportado a archivo WAV";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando WAV", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_AUDIO_MERGE_WAV: {
        std::wstring inWav;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain, L"Audio WAV (*.wav)\0*.wav\0\0", L"Combinar Pista WAV", inWav)) {
            AudioTimelineSession other;
            std::string err;
            if (other.LoadFromWavFile(WideToUtf8(inWav), &err) && st->audioSession.MergeWithTrack(other)) {
                st->audioTimelineVisible = true;
                st->statusBannerUtf8 = "Pista WAV combinada (" + std::to_string(st->audioSession.DurationMs() / 1000) + " s totales)";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err.empty() ? "No se pudo combinar la pista." : err).c_str(), L"Combinar Audio", MB_ICONWARNING);
            }
        }
        break;
    }
    case CMD_STUDY_PROGRESS_TIMER: {
        const ULONGLONG nowTick = GetTickCount64();
        if (st->studyTimerRunning && st->lastStudyClockMs > 0 && nowTick > st->lastStudyClockMs) {
            st->uiPrefs.totalStudySeconds += (nowTick - st->lastStudyClockMs) / 1000ull;
        }
        st->studyTimerRunning = !st->studyTimerRunning;
        st->lastStudyClockMs = st->studyTimerRunning ? nowTick : 0ull;
        const std::uint64_t nowSec = static_cast<std::uint64_t>(std::time(nullptr));
        const auto prog = GroundedStudyAiAssistant::ComputeStudyProgress(st->uiPrefs.totalStudySeconds / 60ull, st->activeDeck);
        const std::size_t dueCnt = StudySetManager::CountDueCards(st->activeDeck, nowSec);
        PersistStudioProfileNow(st);
        std::ostringstream oss;
        oss << "Cron\xC3\xB3metro de estudio: " << (st->studyTimerRunning ? "ACTIVO" : "PAUSADO") << "\n"
            << "Tiempo acumulado: " << prog.totalStudyMinutes << " min (" << (st->uiPrefs.totalStudySeconds % 60ull) << " s)\n"
            << "Tarjetas dominadas: " << prog.masteredCards << " / " << prog.totalCards
            << " (" << static_cast<int>(prog.completionRatio * 100.0) << "%)\n"
            << "Pendientes hoy (SM-2): " << dueCnt;
        MessageBoxW(st->hwndMain, Utf8ToWide(oss.str()).c_str(), L"Progreso de Estudio y Cron\u00F3metro", MB_ICONINFORMATION);
        st->statusBannerUtf8 = std::string("Cron\xC3\xB3metro ") + (st->studyTimerRunning ? "activo" : "pausado") +
            " (" + std::to_string(prog.totalStudyMinutes) + " min, " + std::to_string(prog.masteredCards) + "/" + std::to_string(prog.totalCards) + " dominadas)";
        SyncDynamicTimers(st);
        break;
    }
    case CMD_IMPORT_STUDY_DECK: {
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"Tarjetas CSV / TSV (*.csv;*.tsv)\0*.csv;*.tsv\0\0", L"Importar Tarjetas de Estudio", inPath)) {
            std::ifstream ifs(inPath, std::ios::binary);
            const std::string raw((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            const char delim = (GetFileExtLower(inPath) == L".tsv") ? '\t' : ',';
            const StudyDeck imp = UniversalFormatsInterop::ImportStudyDeckFromDelimited(raw, delim, st->doc.Title());
            for (const auto& c : imp.cards) st->activeDeck.cards.push_back(c);
            st->doc.BumpRevision();
            st->statusBannerUtf8 = "Tarjetas importadas: " + std::to_string(imp.cards.size()) + " (total mazo: " + std::to_string(st->activeDeck.cards.size()) + ")";
        }
        break;
    }
    case CMD_LAYERS_MANAGER: {
        if (IsDocumentLockedForEdit(st)) break;
        if (ClosureDialogs::ShowLayersManagerDialog(st->hwndMain, st->doc, st->statusBannerUtf8)) {
            st->doc.BumpRevision();
        }
        break;
    }
    case CMD_PDF_ANNOTATIONS_MGR: {
        int jumpPage0 = -1;
        if (ClosureDialogs::ShowPdfAnnotationsAndBookmarksDialog(
                st->hwndMain, st->doc, st->cmdStack, st->flexcilOverlay, st->activePageIndex, jumpPage0, st->statusBannerUtf8)) {
            st->doc.BumpRevision();
        }
        if (jumpPage0 >= 0 && static_cast<std::size_t>(jumpPage0) < st->doc.Pages().size()) {
            st->doc.SetMode(DocumentMode::PaginatedPdf);
            CenterCameraOnPage(st, static_cast<std::size_t>(jumpPage0));
        }
        break;
    }
    case CMD_PDF_SCROLL_DIRECTION: {
        const auto nextDir = (st->flexcilOverlay.ScrollDirection() == PdfScrollDirection::Vertical)
            ? PdfScrollDirection::Horizontal
            : PdfScrollDirection::Vertical;
        st->flexcilOverlay.SetScrollDirection(nextDir);
        st->statusBannerUtf8 = (nextDir == PdfScrollDirection::Horizontal)
            ? "Desplazamiento PDF: Horizontal"
            : "Desplazamiento PDF: Vertical";
        break;
    }
    case CMD_STYLUS_DOUBLE_TAP_ERASER: {
        st->floatingInkToolbar.eraserActive = (st->activeTool == ToolKind::Eraser);
        if (st->activeTool == ToolKind::Highlighter) {
            st->floatingInkToolbar.activePen = InkPenStyle::Highlighter;
        } else if (st->activeTool == ToolKind::Pen) {
            st->floatingInkToolbar.activePen = st->penStyle;
        }
        SmartScriptReflowEngine::HandleStylusDoubleTapToggle(st->floatingInkToolbar);
        if (st->floatingInkToolbar.eraserActive) {
            st->activeTool = ToolKind::Eraser;
        } else {
            st->activeTool = (st->floatingInkToolbar.activePen == InkPenStyle::Highlighter)
                ? ToolKind::Highlighter : ToolKind::Pen;
        }
        st->statusBannerUtf8 = st->floatingInkToolbar.eraserActive
            ? "Doble toque: Borrador activo"
            : (st->activeTool == ToolKind::Highlighter ? "Doble toque: Resaltador activo" : "Doble toque: L\xC3\xA1piz activo");
        break;
    }
    case CMD_INK_WORD_SPACE_GESTURE: {
        if (IsDocumentLockedForEdit(st)) break;
        const std::vector<InkPoint> splitStroke = {
            { 160.0, 60.0, 0.7f, 0, 0, 1 }, { 160.0, 220.0, 0.7f, 0, 0, 2 }
        };
        const bool ok = SmartScriptReflowEngine::ApplyWordJoinOrSplitSpaceGesture(st->doc, st->cmdStack, CurrentTarget(st), splitStroke);
        st->statusBannerUtf8 = ok
            ? "Espaciado entre palabras manuscritas ajustado"
            : "Escriba al menos dos trazos manuscritos para ajustar espaciado";
        break;
    }
    case CMD_IMPORT_XLSX: {
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain, L"Libro Excel (*.xlsx)\0*.xlsx\0\0", L"Importar Hoja de C\u00E1lculo Excel", inPath)) {
            std::string err;
            if (DocStudioApp::ImportFromXlsxFile(WideToUtf8(inPath), st->doc, &err)) {
                st->cmdStack.Clear();
                st->pageCache.InvalidateAll(++st->asyncGen);
                CenterCameraOnPage(st, 0);
                st->statusBannerUtf8 = "Hoja Excel (.xlsx) importada (" + std::to_string(st->doc.Pages().size()) + " p\xC3\xA1gina(s))";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error importando Excel", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_EXPORT_XLSX: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Libro Excel (*.xlsx)\0*.xlsx\0\0", L"xlsx", L"cuaderno.xlsx", L"Exportar a Excel (.xlsx)", outPath)) {
            std::string err;
            if (DocStudioApp::ExportToXlsxFile(st->doc, WideToUtf8(outPath), &err)) {
                st->statusBannerUtf8 = "Documento exportado a Excel (.xlsx)";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando Excel", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_EXPORT_CSV: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"CSV (*.csv)\0*.csv\0\0", L"csv", L"cuaderno.csv", L"Exportar a CSV", outPath)) {
            std::string err;
            if (DocStudioApp::ExportToCsvFile(st->doc, WideToUtf8(outPath), &err)) {
                st->statusBannerUtf8 = "Tablas y texto exportados a CSV";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando CSV", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_IMPORT_NOTION_PAGE: {
        std::wstring reference;
        if (!ShowTextPrompt(st->hwndMain, L"Guardar p\u00E1gina de Notion", L"Enlace o ID de p\u00E1gina",
                reference)) break;
        std::wstring savePath;
        if (!FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"CuadernU (*.cudoc)\0*.cudoc\0\0",
                L"cudoc", L"Notion.cudoc", L"Guardar p\u00E1gina como cuaderno", savePath)) break;
        std::wstring token;
        if (!ShowTextPrompt(st->hwndMain, L"Conexi\u00F3n con Notion", L"Clave de la conexi\u00F3n de lectura",
                token, nullptr, nullptr, false, true)) break;
        std::string markdown, error;
        const bool fetched = NotionMarkdown::FetchPage(WideToUtf8(reference), token, markdown, error);
        SecureZeroMemory(token.data(), token.size() * sizeof(wchar_t));
        if (!fetched) {
            MessageBoxW(st->hwndMain, Utf8ToWide(error).c_str(), L"Lectura de Notion", MB_OK | MB_ICONWARNING);
            break;
        }
        DocumentModel imported;
        if (DocStudioApp::ImportFromMarkdownText(markdown, "Notion", imported) &&
            CudocStorage::SaveToFile(savePath, imported, &error)) {
            st->statusBannerUtf8 = "P\xC3\xA1gina de Notion guardada como cuaderno";
        } else {
            MessageBoxW(st->hwndMain, Utf8ToWide(error).c_str(), L"Guardar p\u00E1gina de Notion",
                MB_OK | MB_ICONWARNING);
        }
        break;
    }
    case CMD_IMPORT_MARKDOWN: {
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain, L"Markdown (*.md)\0*.md\0\0", L"Importar Markdown (.md)", inPath)) {
            std::string err;
            if (DocStudioApp::ImportFromMarkdownFile(WideToUtf8(inPath), st->doc, &err)) {
                st->cmdStack.Clear();
                st->pageCache.InvalidateAll(++st->asyncGen);
                CenterCameraOnPage(st, 0);
                st->statusBannerUtf8 = "Documento Markdown (.md) importado";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error importando Markdown", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_EXPORT_MARKDOWN: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Markdown (*.md)\0*.md\0\0", L"md", L"documento.md", L"Exportar a Markdown (.md)", outPath)) {
            RebuildReadingViewText(st);
            const RichBlockDocument rbd = RichBlockDocument::FromMarkdown(st->cachedReadingTextUtf8);
            std::string md = rbd.ToMarkdown();
            const auto hls = rbd.ExtractHighlights();
            if (!hls.empty()) {
                md += "\n\n## Resaltados Extraidos\n";
                for (const auto& h : hls) md += "- " + h.highlightedTextUtf8 + "\n";
            }
            std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
            ofs.write(md.data(), static_cast<std::streamsize>(md.size()));
            st->statusBannerUtf8 = "Exportado a Markdown (" + std::to_string(hls.size()) + " resaltados)";
        }
        break;
    }
    case CMD_IMPORT_ONENOTE_HTML: {
        std::wstring inPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain, L"HTML (*.html;*.htm)\0*.html;*.htm\0\0", L"Importar HTML / OneNote HTML", inPath)) {
            std::ifstream ifs(inPath, std::ios::binary);
            const std::string html((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            std::string err;
            if (UniversalFormatsInterop::ImportPageFromOneNoteGraphHtml(html, st->doc, &err)) {
                st->cmdStack.Clear();
                st->pageCache.InvalidateAll(++st->asyncGen);
                CenterCameraOnPage(st, 0);
                st->statusBannerUtf8 = "P\xC3\xA1gina HTML / OneNote importada";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error importando HTML", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_EXPORT_DXF: {
        std::wstring outPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"AutoCAD DXF (*.dxf)\0*.dxf\0\0", L"dxf", L"plano.dxf", L"Exportar Geometr\u00EDa CAD (.dxf)", outPath)) {
            std::string err;
            if (DocStudioApp::ExportToDxfFile(st->doc, WideToUtf8(outPath), "1:1", &err)) {
                st->statusBannerUtf8 = "Geometr\xC3\xAD""a exportada a AutoCAD DXF";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Error exportando DXF", MB_ICONERROR);
            }
        }
        break;
    }
    case CMD_SCAN_RECTIFY_TO_PDF: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring inImg;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"Imagen de Escaneo (*.png;*.jpg;*.jpeg;*.bmp)\0*.png;*.jpg;*.jpeg;*.bmp\0\0",
                L"Seleccionar Fotografia de Documento", inImg)) {
            std::vector<std::uint8_t> png;
            int w = 0, h = 0;
            std::string err;
            if (ClosureDialogs::RectifyScanFileToPngBytes(inImg, png, w, h, &err)) {
                AnnotationObject img;
                img.kind = ObjectKind::Image;
                img.layerId = st->doc.ActiveLayerId();
                img.pngBytes = std::move(png);
                img.imagePixelW = w;
                img.imagePixelH = h;
                img.bounds = { 48.0, 48.0, 460.0, 460.0 * static_cast<double>(h) / (std::max)(1, w) };
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(img)));
                st->statusBannerUtf8 = "Escaneo rectificado con homograf\xC3\xAD""a y mejora de contraste (" + std::to_string(w) + "x" + std::to_string(h) + ")";
            } else {
                MessageBoxW(st->hwndMain, Utf8ToWide(err).c_str(), L"Rectificar Escaneo", MB_ICONWARNING);
            }
        }
        break;
    }
    case CMD_GLOBAL_SCREEN_CAPTURE: {
        if (IsDocumentLockedForEdit(st)) break;
        ShowWindow(st->hwndMain, SW_MINIMIZE);
        Sleep(220);
        std::vector<std::uint8_t> png;
        int w = 0, h = 0;
        const bool ok = ClosureDialogs::CaptureDesktopScreenPngBytes(png, w, h);
        ShowWindow(st->hwndMain, SW_RESTORE);
        SetForegroundWindow(st->hwndMain);
        if (ok && !png.empty()) {
            double x = 60.0, y = 60.0;
            NextCardPlacement(st, x, y);
            AnnotationObject img;
            img.kind = ObjectKind::Image;
            img.layerId = st->doc.ActiveLayerId();
            img.pngBytes = std::move(png);
            img.imagePixelW = w;
            img.imagePixelH = h;
            const double dw = 480.0;
            const double dh = dw * static_cast<double>(h) / (std::max)(1, w);
            img.bounds = { x, y, dw, dh };
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(img)));
            st->statusBannerUtf8 = "Captura de pantalla insertada (" + std::to_string(w) + "x" + std::to_string(h) + ")";
        } else {
            st->statusBannerUtf8 = "No se pudo capturar la pantalla del escritorio";
        }
        break;
    }
    case CMD_AUTO_SYNC_WATCHER: {
        if (st->uiPrefs.autoSyncEnabled && st->uiPrefs.autoSyncEncrypted && st->autoSyncPassphraseUtf8.empty()) {
            std::wstring passW;
            if (ShowTextPrompt(st->hwndMain, L"Reanudar sincronizaci\u00F3n cifrada",
                    L"Introduzca la frase de cifrado", passW, nullptr, nullptr, false, true) && !passW.empty()) {
                SelectiveSyncFilter filter{};
                filter.encryptPassphraseUtf8 = WideToUtf8(passW);
                const auto rep = LocalFirstSyncEngine::SynchronizeDirectories(
                    StudioProfileStore::DefaultProfileRootDir(),
                    Utf8ToWide(st->uiPrefs.autoSyncFolderUtf8), FolderSyncMode::MergeLossless, filter);
                st->statusBannerUtf8 = rep.diagnosticUtf8;
                if (rep.ok) st->autoSyncPassphraseUtf8 = std::move(filter.encryptPassphraseUtf8);
            }
        } else if (st->uiPrefs.autoSyncEnabled) {
            st->uiPrefs.autoSyncEnabled = false;
            st->autoSyncPassphraseUtf8.clear();
            PersistStudioProfileNow(st);
            st->statusBannerUtf8 = "Auto-sincronizaci\xC3\xB3n por carpeta desactivada";
        } else {
            std::wstring syncDir;
            if (FeatureDialogs::PickFolderWin32(st->hwndMain, L"Seleccionar Carpeta de Auto-Sincronizaci\u00F3n", syncDir)) {
                std::wstring passW;
                if (!ShowTextPrompt(st->hwndMain, L"Cifrado de carpeta sincronizada",
                        L"Frase de cifrado opcional", passW, nullptr, nullptr, false, true)) break;
                PersistStudioProfileNow(st);
                SelectiveSyncFilter flt{};
                flt.encryptPassphraseUtf8 = WideToUtf8(passW);
                const auto rep = LocalFirstSyncEngine::SynchronizeDirectories(
                    StudioProfileStore::DefaultProfileRootDir(), syncDir, FolderSyncMode::MergeLossless, flt);
                st->statusBannerUtf8 = rep.diagnosticUtf8;
                if (rep.ok) {
                    st->uiPrefs.autoSyncFolderUtf8 = WideToUtf8(syncDir);
                    st->uiPrefs.autoSyncEncrypted = !passW.empty();
                    st->autoSyncPassphraseUtf8 = std::move(flt.encryptPassphraseUtf8);
                    st->uiPrefs.autoSyncEnabled = true;
                    PersistStudioProfileNow(st);
                }
            }
        }
        SyncDynamicTimers(st);
        break;
    }
    case CMD_EXPORT_KEYMAP_LOAD_THEME: {
        std::wstring outKm;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Atajos JSON (*.json)\0*.json\0\0", L"json", L"atajos_cuadernu.json", L"Exportar Atajos (Cancelar = Cargar Tema)", outKm)) {
            if (st->keymapProfile.ExportToFile(WideToUtf8(outKm))) {
                st->statusBannerUtf8 = "Perfil de atajos exportado a JSON";
            }
        } else {
            std::wstring inTheme;
            if (FeatureDialogs::PickOpenFileWin32(st->hwndMain, L"Perfil de Tema (*.json)\0*.json\0\0", L"Cargar Perfil de Tema (.json)", inTheme)) {
                ThemeProfile prof{};
                std::string err;
                if (ThemeProfileRegistry::LoadProfileFromFile(inTheme, prof, &err)) {
                    st->uiPrefs.themeProfileIdUtf8 = prof.idUtf8;
                    if (!prof.cssSnippetUtf8.empty()) st->uiPrefs.customCssSnippetUtf8 = prof.cssSnippetUtf8;
                    PersistStudioProfileNow(st);
                    st->statusBannerUtf8 = "Perfil de tema cargado: " + prof.displayNameUtf8;
                }
            }
        }
        break;
    }
    case CMD_INSERT_VIDEO_CLIP: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring inVid;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"Video (*.mp4;*.mkv;*.wmv;*.avi;*.webm)\0*.mp4;*.mkv;*.wmv;*.avi;*.webm\0\0",
                L"Insertar Video Reproducible", inVid)) {
            RasterImageBgra32 posterBgra;
            std::vector<std::uint8_t> posterPng;
            std::uint64_t durMs = 0;
            int vw = 640, vh = 360;
            std::string vErr;
            const bool extracted = ExtractVideoPosterAndMetadata(inVid, posterBgra, posterPng, durMs, vw, vh, &vErr);

            std::string vKey;
            std::error_code ec;
            const auto fsz = std::filesystem::file_size(std::filesystem::path(inVid), ec);
            if (!ec && fsz > 0 && fsz <= 16ull * 1024ull * 1024ull) {
                std::ifstream ifs(inVid, std::ios::binary);
                if (ifs.is_open()) {
                    std::vector<std::uint8_t> vbytes(static_cast<std::size_t>(fsz));
                    ifs.read(reinterpret_cast<char*>(vbytes.data()), static_cast<std::streamsize>(fsz));
                    vKey = st->doc.RegisterAsset(vbytes);
                }
            }
            const std::string fname = std::filesystem::path(inVid).filename().u8string();
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            const double aspect = (vw > 0 && vh > 0) ? (static_cast<double>(vh) / static_cast<double>(vw)) : 0.5625;
            const double dw = 360.0;
            const double dh = std::clamp(dw * aspect, 168.0, 320.0);

            AnnotationObject card{};
            card.kind = ObjectKind::VideoClip;
            card.layerId = st->doc.ActiveLayerId();
            card.bounds = { x, y, dw, dh };
            card.captionUtf8 = fname;
            card.videoSourcePathUtf8 = WideToUtf8(inVid);
            card.videoAssetKey = vKey;
            card.videoDurationMs = durMs;
            card.videoPosterPngBytes = std::move(posterPng);
            card.imagePixelW = vw;
            card.imagePixelH = vh;
            const ObjectTarget tgt = CurrentTarget(st);
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(card)));
            const std::uint64_t newId = AppActions::LastInsertedId(st->doc, tgt);
            if (newId != 0) {
                st->selectedObjectId = newId;
                st->selectedIsCanvas = tgt.isCanvas;
                st->selectedPageIndex = tgt.pageIndex;
            }
            AppendWalMutation(st, "add_video_clip");
            st->statusBannerUtf8 = extracted
                ? ("Video insertado: " + fname + " (" + std::to_string(durMs / 1000) + " s" + (vKey.empty() ? ", vinculado" : ", incrustado") + ") \xE2\x80\x94 doble clic para reproducir")
                : ("Video insertado: " + fname + " (sin p\xC3\xB3ster: " + vErr + ")");
        }
        break;
    }
    case CMD_A11Y_CENTER: {
        if (ClosureDialogs::ShowAccessibilityCenterDialog(st->hwndMain, st->uiPrefs.accessibility, st->speechSynth)) {
            RebuildScaledUiFonts(st);
            PersistStudioProfileNow(st);
            LayoutMainChildren(st);
            st->statusBannerUtf8 = std::string("Accesibilidad actualizada — Papel: ") +
                AccessibilityEngine::PaperTintLabelUtf8(st->uiPrefs.accessibility.paperTint);
        }
        break;
    }
    case CMD_A11Y_TTS_SPEAK: {
        const std::string textToSpeak = AccessibilityEngine::ExtractReadableSpeechText(st->doc, st->activePageIndex, st->selectedObjectId);
        if (textToSpeak.empty()) {
            st->statusBannerUtf8 = "No hay texto ni f\xC3\xB3rmulas en la vista actual para leer en voz alta";
        } else {
            st->speechSynth.SetRate(st->uiPrefs.accessibility.ttsRate);
            st->speechSynth.SetVolume(st->uiPrefs.accessibility.ttsVolume);
            std::string ttsErr;
            if (st->speechSynth.SpeakAsync(textToSpeak, &ttsErr)) {
                st->statusBannerUtf8 = "Leyendo en voz alta...";
            } else {
                st->statusBannerUtf8 = ttsErr;
            }
        }
        break;
    }
    case CMD_A11Y_TTS_STOP: {
        st->speechSynth.Stop();
        st->statusBannerUtf8 = "Lectura en voz alta detenida";
        break;
    }
    case CMD_A11Y_READING_RULER: {
        st->uiPrefs.accessibility.readingRulerEnabled = !st->uiPrefs.accessibility.readingRulerEnabled;
        PersistStudioProfileNow(st);
        st->statusBannerUtf8 = st->uiPrefs.accessibility.readingRulerEnabled
            ? "Regla de lectura enfocada ACTIVADA (mueva el cursor verticalmente)"
            : "Regla de lectura enfocada desactivada";
        break;
    }
    case CMD_A11Y_DYSLEXIA_MODE: {
        st->uiPrefs.accessibility.dyslexiaFriendlyMode = !st->uiPrefs.accessibility.dyslexiaFriendlyMode;
        PersistStudioProfileNow(st);
        st->statusBannerUtf8 = st->uiPrefs.accessibility.dyslexiaFriendlyMode
            ? "Modo lectura amigable para dislexia ACTIVADO (Verdana 1.48x)"
            : "Modo lectura amigable desactivado";
        break;
    }
    case CMD_A11Y_CYCLE_TINT: {
        const int nextTint = (static_cast<int>(st->uiPrefs.accessibility.paperTint) + 1) % 5;
        st->uiPrefs.accessibility.paperTint = static_cast<PaperTintPreset>(nextTint);
        PersistStudioProfileNow(st);
        st->statusBannerUtf8 = std::string("Tinte de papel: ") +
            AccessibilityEngine::PaperTintLabelUtf8(st->uiPrefs.accessibility.paperTint);
        break;
    }
    case CMD_A11Y_CYCLE_COLORBLIND: {
        const int nextCb = (static_cast<int>(st->uiPrefs.accessibility.colorBlindMode) + 1) % 4;
        st->uiPrefs.accessibility.colorBlindMode = static_cast<ColorBlindPaletteMode>(nextCb);
        PersistStudioProfileNow(st);
        st->statusBannerUtf8 = std::string("Paleta de color: ") +
            AccessibilityEngine::ColorBlindModeLabelUtf8(st->uiPrefs.accessibility.colorBlindMode);
        break;
    }
    case CMD_TUTOR_WHITEBOARD_OPEN: {
        if (st->examModeLocked) {
            st->statusBannerUtf8 = "Bloqueado en Modo Examen";
            break;
        }
        std::string tutorErr;
        const AiProviderVault* vPtr = st->aiVault.IsEnabled() ? &st->aiVault : nullptr;
        const bool ok = st->tutorPanel.OpenOrFocus(
            st->hwndMain,
            st->glassSettings,
            vPtr,
            [st](const std::string& beatsJsonArrayUtf8) -> Tutor::TutorBakeReport {
                Tutor::TutorBakeReport rep;
                if (IsDocumentLockedForEdit(st)) {
                    rep.summaryUtf8 = "El documento est\xC3\xA1 bloqueado para edici\xC3\xB3n.";
                    return rep;
                }
                Tutor::TutorSessionStateMachine sm;
                sm.Reset("baked_from_panel");
                try {
                    const auto arr = nlohmann::json::parse(beatsJsonArrayUtf8);
                    if (arr.is_array()) {
                        for (const auto& item : arr) {
                            Tutor::TutorBeat b;
                            Tutor::TutorValidationResult vr;
                            if (Tutor::ParseAndValidateTutorBeat(item.dump(), b, vr) && vr.ok) {
                                sm.ApplyBeat(b);
                            }
                        }
                    }
                } catch (...) {}
                double bx = 80.0, by = 80.0;
                NextCardPlacement(st, bx, by);
                rep = Tutor::BakeTutorSessionToDocument(st->doc, st->cmdStack, CurrentTarget(st), sm, bx, by);
                if (rep.ok) {
                    if (st->hwndViewport && IsWindow(st->hwndViewport)) {
                        InvalidateRect(st->hwndViewport, nullptr, FALSE);
                    }
                    if (st->hwndMain && IsWindow(st->hwndMain)) {
                        InvalidateRect(st->hwndMain, nullptr, FALSE);
                    }
                }
                return rep;
            },
            [st](const std::string& statusUtf8) {
                if (!statusUtf8.empty()) {
                    st->statusBannerUtf8 = statusUtf8;
                    if (st->hwndMain && IsWindow(st->hwndMain)) {
                        InvalidateRect(st->hwndMain, nullptr, FALSE);
                    }
                }
            },
            &tutorErr);
        if (!ok) {
            st->statusBannerUtf8 = tutorErr.empty()
                ? "No se pudo abrir la Pizarra Tutor Audiovisual"
                : tutorErr;
            MessageBoxW(
                st->hwndMain,
                Utf8ToWide(st->statusBannerUtf8).c_str(),
                L"CuadernU \u00B7 Pizarra Tutor Audiovisual",
                MB_ICONWARNING | MB_OK);
        }
        break;
    }
    case CMD_STUDY_TUTOR_STEP: {
        std::string insertMd;
        const AiProviderVault* vPtr = st->aiVault.IsEnabled() ? &st->aiVault : nullptr;
        if (ClosureDialogs::ShowGuidedTutorDialog(st->hwndMain, st->doc, vPtr, &st->speechSynth, insertMd) &&
            !insertMd.empty() && !IsDocumentLockedForEdit(st)) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { x, y, 440.0, 210.0 };
            tb.captionUtf8 = "Tutor Paso a Paso";
            tb.textUtf8 = insertMd;
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
            st->statusBannerUtf8 = "Gu\xC3\xAD""a del Tutor insertada en el cuaderno";
        }
        break;
    }
    case CMD_STUDY_GEN_FLASHCARDS_AI: {
        HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        bool usedAi = false;
        const AiProviderVault* vPtr = st->aiVault.IsEnabled() ? &st->aiVault : nullptr;
        const auto generated = StudyAssistantTools::GenerateSmartFlashcards(st->doc, 8, vPtr, &usedAi);
        SetCursor(oldC);
        if (generated.empty()) {
            st->statusBannerUtf8 = "Agregue apuntes, definiciones o f\xC3\xB3rmulas para generar tarjetas SM-2";
            break;
        }
        std::size_t addedToDeck = 0;
        for (const auto& fc : generated) {
            bool dup = false;
            for (const auto& ex : st->activeDeck.cards) {
                if (ex.questionUtf8 == fc.questionUtf8) { dup = true; break; }
            }
            if (!dup) {
                st->activeDeck.cards.push_back(fc);
                ++addedToDeck;
            }
        }
        if (!IsDocumentLockedForEdit(st)) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            StudyAssistantTools::InsertPracticeStudyTapes(st->doc, st->cmdStack, CurrentTarget(st), generated, x, y);
        }
        st->doc.BumpRevision();
        st->statusBannerUtf8 = std::to_string(generated.size()) + " tarjeta(s) de pr\xC3\xA1""ctica generadas (" +
            std::to_string(addedToDeck) + " nuevas en SM-2" + (usedAi ? ", IA)" : ", Local)");
        break;
    }
    case CMD_STUDY_CONCEPT_MAP: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring topicW = Utf8ToWide(st->doc.Title().empty() ? "Tema Central" : st->doc.Title());
        if (!ShowTextPrompt(st->hwndMain, L"Generador de Mapa Conceptual", L"Tema central del esquema:", topicW) || topicW.empty()) break;
        if (st->doc.Mode() != DocumentMode::InfiniteCanvas) {
            st->doc.SetMode(DocumentMode::InfiniteCanvas);
            LayoutMainChildren(st);
        }
        HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        const AiProviderVault* vPtr = st->aiVault.IsEnabled() ? &st->aiVault : nullptr;
        const auto mapRes = StudyAssistantTools::GenerateConceptMapInDocument(
            st->doc, st->cmdStack, ObjectTarget{ true, 0 }, WideToUtf8(topicW), vPtr, 120.0, 100.0);
        SetCursor(oldC);
        if (mapRes.ok) {
            CenterCameraOnWorldRect(st, { 100.0, 80.0, 780.0, 420.0 });
            st->statusBannerUtf8 = mapRes.summaryUtf8;
        }
        break;
    }
    case CMD_STUDY_FORMULA_GLOSSARY: {
        if (IsDocumentLockedForEdit(st)) break;
        const auto sheet = StudyAssistantTools::BuildFormulaSheetAndGlossary(st->doc, &st->knowledgeVault);
        if (StudyAssistantTools::AppendFormulaSheetPage(st->doc, st->cmdStack, sheet)) {
            if (st->doc.Mode() == DocumentMode::PaginatedPdf && !st->doc.Pages().empty()) {
                st->doc.RebuildLayout();
                CenterCameraOnPage(st, st->doc.Pages().size() - 1);
            }
            st->statusBannerUtf8 = "Hoja de F\xC3\xB3rmulas (" + std::to_string(sheet.formulaCount) +
                ") y Glosario (" + std::to_string(sheet.glossaryCount) + ") compilados";
        }
        break;
    }
    case CMD_STUDY_POMODORO: {
        if (ClosureDialogs::ShowPomodoroDialog(st->hwndMain, st->pomodoro)) {
            if (st->pomodoro.running && !st->pomodoro.paused && st->pomodoro.autoEnableFocusMode && !st->pomodoro.inBreakPhase) {
                st->focusMode = true;
                LayoutMainChildren(st);
            }
            st->statusBannerUtf8 = "Pomodoro: " + st->pomodoro.FormatHudBadgeUtf8();
            SyncDynamicTimers(st);
        }
        break;
    }
    case CMD_AUTO_EXTRACT_TASKS_ICS: {
        const auto rep = StudyAssistantTools::ExtractTasksAndDeadlines(st->doc, &st->knowledgeVault, &st->activeDatabase);
        if (rep.extracted.empty()) {
            st->statusBannerUtf8 = "No se detectaron tareas (- [ ], TODO:, Entrega:, Examen:) en el cuaderno";
            break;
        }
        st->dbPanelVisible = true;
        st->doc.BumpRevision();
        st->statusBannerUtf8 = "Tareas extra\xC3\xAD""das: " + std::to_string(rep.extracted.size()) +
            " (" + std::to_string(rep.insertedIntoDatabaseCount) + " nuevas en BD)";
        std::wstring icsPath;
        if (FeatureDialogs::PickSaveFileWin32(st->hwndMain, L"Calendario iCalendar (*.ics)\0*.ics\0\0", L"ics",
                L"entregas_cuadernu.ics", L"Exportar Tareas y Entregas a Calendario (.ics) [Cancelar = Solo BD]", icsPath)) {
            std::ofstream ofs(icsPath, std::ios::binary | std::ios::trunc);
            ofs.write(rep.icsCalendarUtf8.data(), static_cast<std::streamsize>(rep.icsCalendarUtf8.size()));
            st->statusBannerUtf8 += " + .ics exportado";
        }
        break;
    }
    case CMD_AUTO_POST_LECTURE: {
        if (IsDocumentLockedForEdit(st)) break;
        HCURSOR oldC = SetCursor(LoadCursorW(nullptr, IDC_WAIT));
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        const AiProviderVault* vPtr = st->aiVault.IsEnabled() ? &st->aiVault : nullptr;
        const auto rep = StudyAssistantTools::RunPostLectureAutomation(
            st->doc, st->cmdStack, CurrentTarget(st), st->audioSession, st->activeDatabase, vPtr, x, y);
        SetCursor(oldC);
        st->statusBannerUtf8 = rep.statusNoteUtf8;
        break;
    }
    case CMD_STUDY_BIBLIOGRAPHY_CITE: {
        std::string citeText;
        if (ClosureDialogs::ShowBibliographyCitationDialog(st->hwndMain, st->doc, &st->flexcilOverlay, citeText) &&
            !citeText.empty() && !IsDocumentLockedForEdit(st)) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.layerId = st->doc.ActiveLayerId();
            tb.bounds = { x, y, 420.0, 110.0 };
            tb.captionUtf8 = "Referencia Bibliogr\xC3\xA1""fica";
            tb.textUtf8 = citeText;
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(tb)));
            st->statusBannerUtf8 = "Referencia bibliogr\xC3\xA1""fica insertada";
        }
        break;
    }
    case CMD_INSERT_LINK_CARD: {
        if (IsDocumentLockedForEdit(st)) break;
        ClosureDialogs::LinkCardInput inp{};
        inp.titleUtf8 = "Calculadora Desmos / Referencia";
        inp.urlUtf8 = "https://www.desmos.com/calculator";
        inp.descriptionUtf8 = "Recurso interactivo de apoyo para el curso";
        if (ClosureDialogs::ShowInsertLinkCardDialog(st->hwndMain, st->doc.Pages().size(), inp)) {
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject card{};
            card.kind = ObjectKind::LinkCard;
            card.layerId = st->doc.ActiveLayerId();
            card.bounds = { x, y, 310.0, 82.0 };
            card.linkTitleUtf8 = inp.titleUtf8;
            card.linkUrlUtf8 = inp.urlUtf8;
            card.linkDescriptionUtf8 = inp.descriptionUtf8;
            card.linkTargetPageIndex = inp.targetPageIndex0;
            const ObjectTarget tgt = CurrentTarget(st);
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(card)));
            const std::uint64_t newId = AppActions::LastInsertedId(st->doc, tgt);
            if (newId != 0) {
                st->selectedObjectId = newId;
                st->selectedIsCanvas = tgt.isCanvas;
                st->selectedPageIndex = tgt.pageIndex;
            }
            AppendWalMutation(st, "add_link_card");
            st->statusBannerUtf8 = (inp.targetPageIndex0 >= 0)
                ? ("Enlace interno a p\xC3\xA1gina " + std::to_string(inp.targetPageIndex0 + 1) + " insertado")
                : ("Enlace web insertado: " + ExtractUrlDomainUtf8(inp.urlUtf8));
        }
        break;
    }
    case CMD_INSERT_GIF: {
        if (IsDocumentLockedForEdit(st)) break;
        std::wstring gifPath;
        if (FeatureDialogs::PickOpenFileWin32(st->hwndMain,
                L"GIF animado (*.gif)\0*.gif\0Todas las im\u00E1genes (*.gif;*.png;*.webp)\0*.gif;*.png;*.webp\0\0",
                L"Insertar GIF Animado", gifPath)) {
            OpenDocumentFile(st, gifPath);
            AppendWalMutation(st, "add_gif");
        }
        break;
    }
    case CMD_INSERT_HTML_EMBED: {
        if (IsDocumentLockedForEdit(st)) break;
        ClosureDialogs::HtmlEmbedInput inp{};
        if (ClosureDialogs::ShowInsertHtmlEmbedDialog(st->hwndMain, inp)) {
            std::string htmlSrc = inp.htmlSourceUtf8;
            std::string htmlUrl = inp.urlUtf8;
            // If user picked a local .html file, load its UTF-8 content inline so .cudoc stays self-contained
            if (!htmlUrl.empty() && !IsSafeExternalLinkUrl(htmlUrl)) {
                std::ifstream ifs(Utf8ToWide(htmlUrl), std::ios::binary);
                if (ifs.is_open()) {
                    htmlSrc.assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                    htmlUrl.clear();
                }
            }
            double x = 80.0, y = 80.0;
            NextCardPlacement(st, x, y);
            AnnotationObject blk{};
            blk.kind = ObjectKind::HtmlEmbed;
            blk.layerId = st->doc.ActiveLayerId();
            blk.bounds = { x, y, 420.0, 260.0 };
            blk.htmlTitleUtf8 = inp.titleUtf8;
            blk.htmlSourceUtf8 = std::move(htmlSrc);
            blk.htmlUrlUtf8 = std::move(htmlUrl);
            blk.htmlAllowNetwork = inp.allowNetwork;
            blk.htmlInteractive = false;
            const ObjectTarget tgt = CurrentTarget(st);
            st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(blk)));
            const std::uint64_t newId = AppActions::LastInsertedId(st->doc, tgt);
            if (newId != 0) {
                st->selectedObjectId = newId;
                st->selectedIsCanvas = tgt.isCanvas;
                st->selectedPageIndex = tgt.pageIndex;
            }
            AppendWalMutation(st, "add_html_embed");
            st->statusBannerUtf8 = "Bloque HTML insertado: " + inp.titleUtf8 + " (doble clic para interactuar en vivo)";
        }
        break;
    }
    case CMD_TOGGLE_GPU_ACCEL: {
        const bool nextHw = !st->renderer.IsHardwareAccelerationEnabled();
        st->uiPrefs.hardwareGpuAcceleration = nextHw;
        st->renderer.SetHardwareAcceleration(nextHw);
        PersistStudioProfileNow(st);
        st->statusBannerUtf8 = nextHw
            ? "Modo de gr\xC3\xA1""ficos: Acelerado por GPU (opcional)"
            : "Modo de gr\xC3\xA1""ficos: Ultra Eficiente (0% GPU activo)";
        break;
    }
    case CMD_MEDIA_PLAY_PAUSE: {
        const ObjectTarget tgt{ st->selectedIsCanvas, st->selectedPageIndex };
        AnnotationObject* obj = (st->selectedObjectId != 0)
            ? CanvasManip::FindObjectMut(st->doc, tgt, st->selectedObjectId) : nullptr;
        if (!obj) {
            st->statusBannerUtf8 = "Seleccione primero un video o GIF animado";
            break;
        }
        if (obj->kind == ObjectKind::VideoClip) {
            if (st->videoPlayer.IsPlaying() && st->videoPlayer.ActiveObjectId() == obj->id) {
                obj->videoPositionMs = st->videoPlayer.CurrentPositionMs();
                st->videoPlayer.Pause();
                st->statusBannerUtf8 = "Video en pausa (decodificador liberado \xC2\xB7 0% CPU/GPU)";
            } else {
                const std::wstring vPath = ResolveVideoPathOnDisk(st, *obj);
                std::string vErr;
                if (!vPath.empty() && st->videoPlayer.Start(
                        obj->id, vPath, obj->videoPositionMs, obj->videoPlaybackRate, obj->videoMuted, obj->videoLoop, &vErr)) {
                    if (obj->videoDurationMs == 0) obj->videoDurationMs = st->videoPlayer.DurationMs();
                    st->statusBannerUtf8 = "Reproduciendo video: " + obj->captionUtf8;
                } else {
                    st->statusBannerUtf8 = "No se pudo reproducir el video: " + (vErr.empty() ? "archivo no encontrado" : vErr);
                }
            }
            SyncDynamicTimers(st);
        } else if (IsAnimatedGifObject(st->doc, *obj)) {
            obj->gifPaused = !obj->gifPaused;
            st->statusBannerUtf8 = obj->gifPaused ? "GIF animado pausado (0% CPU)" : "GIF animado reanudado";
            SyncDynamicTimers(st);
        } else if (obj->kind == ObjectKind::AudioClip) {
            ExecuteToolbarCommand(st, CMD_REPLAY_TOGGLE);
            return;
        }
        break;
    }
    case CMD_HTML_FREEZE_RESUME: {
        const ObjectTarget tgt{ st->selectedIsCanvas, st->selectedPageIndex };
        AnnotationObject* obj = (st->selectedObjectId != 0)
            ? CanvasManip::FindObjectMut(st->doc, tgt, st->selectedObjectId) : nullptr;
        if (!obj || obj->kind != ObjectKind::HtmlEmbed) {
            st->statusBannerUtf8 = "Seleccione primero un bloque HTML animado";
            break;
        }
        if (st->activeHtmlObjectId == obj->id) {
            // Freeze block: capture crisp PNG snapshot and suspend host!
            const std::uint64_t freezeId = obj->id;
            const bool isCanvas = tgt.isCanvas;
            const std::size_t pgIdx = tgt.pageIndex;
            HWND mainH = st->hwndMain;
            obj->htmlInteractive = false;
            st->activeHtmlObjectId = 0;
            st->htmlHost.CaptureSnapshotAndSuspend(freezeId, [mainH, freezeId, isCanvas, pgIdx](std::uint64_t, std::vector<std::uint8_t> png) {
                if (png.empty() || !IsWindow(mainH)) return;
                auto* payload = new HtmlSnapshotPayload{ freezeId, isCanvas, pgIdx, std::move(png) };
                if (!PostMessageW(mainH, WM_APP_HTML_SNAPSHOT_READY, 0, reinterpret_cast<LPARAM>(payload))) {
                    delete payload;
                }
            });
            st->statusBannerUtf8 = "HTML congelado en miniatura PNG (suspendido \xC2\xB7 ~0% CPU)";
        } else {
            if (st->activeHtmlObjectId != 0) {
                st->htmlHost.DeactivateBlock(st->activeHtmlObjectId);
                st->activeHtmlObjectId = 0;
            }
            if (!WebView2HtmlHost::IsWebView2RuntimeInstalled()) {
                st->statusBannerUtf8 = "Motor de vista web del sistema no detectado";
                break;
            }
            DocRect wr = obj->bounds;
            if (!tgt.isCanvas) {
                if (const auto* pl = st->doc.Layout().PlacementAt(tgt.pageIndex)) {
                    wr.x += pl->leftWorld;
                    wr.y += pl->topWorld;
                }
            }
            const DocPoint p0 = st->camera.WorldToScreen(wr.x, wr.y);
            const DocPoint p1 = st->camera.WorldToScreen(wr.Right(), wr.Bottom());
            const int hdrH = (std::min)(26, static_cast<int>((p1.y - p0.y) * 0.22));
            RECT bnd{
                static_cast<LONG>(std::lround(p0.x)) + 2,
                static_cast<LONG>(std::lround(p0.y)) + hdrH + 1,
                static_cast<LONG>(std::lround(p1.x)) - 2,
                static_cast<LONG>(std::lround(p1.y)) - 2
            };
            HWND mainH = st->hwndMain;
            const bool ok = st->htmlHost.ActivateBlock(
                st->hwndViewport, obj->id, bnd, obj->htmlSourceUtf8, obj->htmlUrlUtf8, obj->htmlAllowNetwork,
                [mainH](std::uint64_t, bool ready) {
                    if (ready && IsWindow(mainH)) PostMessageW(mainH, WM_APP_ASYNC_PAGE_READY, 0, 0);
                });
            if (ok) {
                st->activeHtmlObjectId = obj->id;
                st->activeHtmlTarget = tgt;
                obj->htmlInteractive = true;
                st->statusBannerUtf8 = "HTML interactivo activo (clic en barra superior o Ctrl+K -> Congelar HTML para suspender)";
            } else {
                st->statusBannerUtf8 = "No se pudo iniciar la vista HTML interactiva";
            }
        }
        break;
    }
    case CMD_MEDIA_CAPTURE_FRAME: {
        if (IsDocumentLockedForEdit(st)) break;
        std::vector<std::uint8_t> png;
        int cw = 360, ch = 220;
        if (st->videoPlayer.IsPlaying() && st->videoPlayer.CaptureCurrentFramePng(png) && !png.empty()) {
            cw = st->videoPlayer.CurrentFrameBgra().width;
            ch = st->videoPlayer.CurrentFrameBgra().height;
        } else if (st->selectedObjectId != 0) {
            if (const auto* o = CanvasManip::FindObjectConst(st->doc, ObjectTarget{ st->selectedIsCanvas, st->selectedPageIndex }, st->selectedObjectId)) {
                if (o->kind == ObjectKind::VideoClip && !o->videoPosterPngBytes.empty()) {
                    png = o->videoPosterPngBytes;
                    cw = (o->imagePixelW > 0) ? o->imagePixelW : 360;
                    ch = (o->imagePixelH > 0) ? o->imagePixelH : 220;
                } else if (o->kind == ObjectKind::HtmlEmbed && !o->htmlSnapshotPngBytes.empty()) {
                    png = o->htmlSnapshotPngBytes;
                }
            }
        }
        if (png.empty()) {
            st->statusBannerUtf8 = "No hay fotograma activo disponible para capturar";
            break;
        }
        double x = 80.0, y = 80.0;
        NextCardPlacement(st, x, y);
        AnnotationObject snap{};
        snap.kind = ObjectKind::Image;
        snap.layerId = st->doc.ActiveLayerId();
        snap.pngBytes = std::move(png);
        snap.imagePixelW = cw;
        snap.imagePixelH = ch;
        snap.captionUtf8 = "Fotograma capturado";
        const double dw = 340.0;
        const double dh = dw * static_cast<double>((std::max)(1, ch)) / static_cast<double>((std::max)(1, cw));
        snap.bounds = { x, y, dw, dh };
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(CurrentTarget(st), std::move(snap)));
        AppendWalMutation(st, "capture_media_frame");
        st->statusBannerUtf8 = "Fotograma capturado como imagen anotable (" + std::to_string(cw) + "x" + std::to_string(ch) + ")";
        break;
    }
    case CMD_MEDIA_TRIM_MEMORY: {
        st->videoPlayer.Pause();
        if (st->activeHtmlObjectId != 0) {
            st->htmlHost.SuspendAll();
            st->activeHtmlObjectId = 0;
        }
        TrimApplicationMemoryNow(st, true);
        SyncDynamicTimers(st);
        PROCESS_MEMORY_COUNTERS pmc{};
        std::string ramTxt;
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
            const double mb = static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
            char mbBuf[48]{};
            std::snprintf(mbBuf, sizeof(mbBuf), " (RAM activa: %.1f MB)", mb);
            ramTxt = mbBuf;
        }
        st->statusBannerUtf8 = "Cach\xC3\xA9 y RAM liberados correctamente" + ramTxt;
        break;
    }
    case CMD_GEMINI_NOTEBOOK_HUB: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        if (!st->gnStoreLoaded) {
            st->gnStore.LoadAll(nullptr);
            st->gnStoreLoaded = true;
        }
        bool reqAiStudy = false;
        GeminiNotebookUi::ShowGeminiNotebookHubDialog(
            st->hwndMain, st->gnStore, &st->doc, st->statusBannerUtf8, &reqAiStudy);
        if (reqAiStudy) {
            ExecuteToolbarCommand(st, CMD_GROUNDED_AI_CITATIONS);
            return;
        }
#else
        st->statusBannerUtf8 = "El modulo opcional Gemini Notebook no esta habilitado en esta compilacion";
#endif
        break;
    }
    case CMD_GEMINI_NOTEBOOK_CONNECT: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        if (!st->gnStoreLoaded) {
            st->gnStore.LoadAll(nullptr);
            st->gnStoreLoaded = true;
        }
        if (GeminiNotebookUi::ShowGeminiNotebookConnectDialog(st->hwndMain, st->gnStore)) {
            st->statusBannerUtf8 = st->gnStore.Tokens().HasAnyCredential()
                ? "Cuenta personal de Google configurada para Gemini Notebook"
                : "Sesion de Gemini Notebook actualizada";
        }
#endif
        break;
    }
    case CMD_GEMINI_NOTEBOOK_SYNC_TEC: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        ExecuteToolbarCommand(st, CMD_GEMINI_NOTEBOOK_HUB);
        return;
#endif
        break;
    }
    case CMD_GEMINI_NOTEBOOK_SEND_DOC: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        if (!st->gnStoreLoaded) {
            st->gnStore.LoadAll(nullptr);
            st->gnStoreLoaded = true;
        }
        const std::string md = GeminiNotebook::ExportDocumentToSyncMarkdown(st->doc);
        GeminiNotebook::AcademicMaterialCandidate cand{};
        cand.inputKind = GeminiNotebook::SourceInputKind::InlineTextOrMarkdown;
        cand.displayNameUtf8 = (st->doc.Title().empty() ? "Cuaderno" : st->doc.Title()) + ".md";
        cand.inlineMarkdownUtf8 = md;
        cand.inlineMimeTypeUtf8 = "text/markdown";
        const std::string nbKey = "doc:" + (st->doc.Title().empty() ? "actual" : st->doc.Title());
        const auto plan = st->gnStore.ComputeIncrementalSyncPlan(
            nbKey, st->doc.Title().empty() ? "Cuaderno CuadernU" : st->doc.Title(), { cand });
        if (st->gnStore.Tokens().HasAnyCredential()) {
            GeminiNotebook::GeminiNotebookRestClient client(st->gnStore);
            const auto sum = client.ExecuteSyncPlan(plan);
            st->statusBannerUtf8 = sum.ok
                ? ("Cuaderno sincronizado con Gemini Notebook (" + std::to_string(sum.uploadedCount + sum.versionedCount) + " nueva, " + std::to_string(sum.skippedCount) + " sin cambios)")
                : ("Gemini Notebook: " + sum.errorUtf8);
        } else {
            GeminiNotebookUi::ShowGeminiNotebookHubDialog(st->hwndMain, st->gnStore, &st->doc, st->statusBannerUtf8);
        }
#endif
        break;
    }
    case CMD_GEMINI_NOTEBOOK_SEND_URL: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        if (!st->gnStoreLoaded) {
            st->gnStore.LoadAll(nullptr);
            st->gnStoreLoaded = true;
        }
        std::string urlToSend = st->lastContextInlineUrlUtf8;
        if (urlToSend.empty() && st->selectedObjectId != 0) {
            const ObjectTarget selTgt{ st->selectedIsCanvas, st->selectedPageIndex };
            if (const auto* selObj = CanvasManip::FindObjectConst(st->doc, selTgt, st->selectedObjectId)) {
                if (selObj->kind == ObjectKind::LinkCard) {
                    urlToSend = selObj->linkUrlUtf8;
                } else if (selObj->kind == ObjectKind::TextBox) {
                    const auto urls = ExtractSafeInlineUrls(selObj->textUtf8);
                    if (!urls.empty()) urlToSend = urls.front().normalizedUrlUtf8;
                }
            }
        }
        if (!IsSafeExternalLinkUrl(urlToSend)) {
            st->statusBannerUtf8 = "Selecciona primero una tarjeta de enlace o un texto con enlace valido";
            break;
        }
        GeminiNotebook::AcademicMaterialCandidate cand{};
        cand.sourceUrlUtf8 = urlToSend;
        cand.inputKind = IsYouTubeUrlUtf8(urlToSend)
            ? GeminiNotebook::SourceInputKind::YouTubeUrl : GeminiNotebook::SourceInputKind::WebUrl;
        cand.displayNameUtf8 = ExtractUrlDomainUtf8(urlToSend);
        const std::string nbKey = "doc:" + (st->doc.Title().empty() ? "actual" : st->doc.Title());
        const auto plan = st->gnStore.ComputeIncrementalSyncPlan(
            nbKey, st->doc.Title().empty() ? "Cuaderno CuadernU" : st->doc.Title(), { cand });
        if (st->gnStore.Tokens().HasAnyCredential()) {
            GeminiNotebook::GeminiNotebookRestClient client(st->gnStore);
            const auto sum = client.ExecuteSyncPlan(plan);
            st->statusBannerUtf8 = sum.ok
                ? ("Enlace enviado a Gemini Notebook: " + cand.displayNameUtf8)
                : ("Gemini Notebook: " + sum.errorUtf8);
        } else {
            GeminiNotebook::LocalNotebookMapping nm{};
            nm.localKeyUtf8 = nbKey;
            nm.displayNameUtf8 = st->doc.Title().empty() ? "Cuaderno CuadernU" : st->doc.Title();
            nm.webUrlUtf8 = "https://notebooklm.google.com/";
            st->gnStore.UpsertNotebookMapping(nm);
            const auto pre = GeminiNotebook::ValidateSourceForUpload(cand, st->gnStore.Config(), true);
            if (pre.ok) {
                GeminiNotebook::LocalSyncSourceEntry e{};
                e.localIdUtf8 = nbKey + ":" + pre.sha256Hex.substr(0, 16);
                e.notebookKeyUtf8 = nbKey;
                e.displayNameUtf8 = cand.displayNameUtf8;
                e.inputKind = cand.inputKind;
                e.sourceUrlUtf8 = cand.sourceUrlUtf8;
                e.mimeTypeUtf8 = pre.detectedMimeTypeUtf8;
                e.sha256Hex = pre.sha256Hex;
                e.state = GeminiNotebook::SyncSourceState::LocalOnly;
                st->gnStore.UpsertSourceEntry(e);
                st->gnStore.SaveManifest(nullptr);
            }
            st->statusBannerUtf8 = "Enlace guardado en inventario local de Gemini Notebook (" + cand.displayNameUtf8 + ")";
        }
#endif
        break;
    }
    case CMD_GEMINI_NOTEBOOK_MCP_CONFIG: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        if (!st->gnStoreLoaded) {
            st->gnStore.LoadAll(nullptr);
            st->gnStoreLoaded = true;
        }
        GeminiNotebookUi::ShowGeminiNotebookMcpConfigDialog(st->hwndMain, st->gnStore.Config());
#endif
        break;
    }
    case CMD_GEMINI_NOTEBOOK_OPEN_WEB: {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
        if (!st->gnStoreLoaded) {
            st->gnStore.LoadAll(nullptr);
            st->gnStoreLoaded = true;
        }
        std::string url = "https://notebooklm.google.com/";
        const std::string nbKey = "doc:" + (st->doc.Title().empty() ? "actual" : st->doc.Title());
        if (const auto* nb = st->gnStore.FindNotebookByLocalKey(nbKey)) {
            if (!nb->webUrlUtf8.empty()) url = nb->webUrlUtf8;
        } else if (!st->gnStore.NotebookMappings().empty() && !st->gnStore.NotebookMappings().front().webUrlUtf8.empty()) {
            url = st->gnStore.NotebookMappings().front().webUrlUtf8;
        }
        if (IsSafeExternalLinkUrl(url)) {
            ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            st->statusBannerUtf8 = "Abriendo Gemini Notebook en el navegador...";
        }
#endif
        break;
    }
    case CMD_TEXTBOX_URL_TO_LINKCARD: {
        if (IsDocumentLockedForEdit(st)) break;
        std::string url = st->lastContextInlineUrlUtf8;
        DocRect anchor{ 80.0, 80.0, 320.0, 86.0 };
        if (st->selectedObjectId != 0) {
            const ObjectTarget selTgt{ st->selectedIsCanvas, st->selectedPageIndex };
            if (const auto* selObj = CanvasManip::FindObjectConst(st->doc, selTgt, st->selectedObjectId)) {
                anchor = { selObj->bounds.x, selObj->bounds.Bottom() + 14.0, 320.0, 86.0 };
                if (url.empty() && selObj->kind == ObjectKind::TextBox) {
                    const auto spans = ExtractSafeInlineUrls(selObj->textUtf8);
                    if (!spans.empty()) url = spans.front().normalizedUrlUtf8;
                }
            }
        }
        if (!IsSafeExternalLinkUrl(url)) {
            st->statusBannerUtf8 = "No se encontro un enlace seguro en el bloque de texto seleccionado";
            break;
        }
        AnnotationObject lk{};
        lk.kind = ObjectKind::LinkCard;
        lk.layerId = st->doc.ActiveLayerId();
        lk.bounds = anchor;
        lk.linkTitleUtf8 = ExtractUrlDomainUtf8(url);
        lk.linkUrlUtf8 = url;
        lk.linkDescriptionUtf8 = IsYouTubeUrlUtf8(url) ? "Video de YouTube enlazado desde texto" : "Enlace convertido desde cuadro de texto";
        lk.linkTargetPageIndex = -1;
        const ObjectTarget tgt = CurrentTarget(st);
        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(tgt, std::move(lk)));
        AppendWalMutation(st, "textbox_url_to_linkcard");
        st->statusBannerUtf8 = "Enlace convertido en tarjeta interactiva: " + ExtractUrlDomainUtf8(url);
        break;
    }
    }
    UpdateWindowTitle(st);
    InvalidateViews(st);
}

// ---------------- Hit-testing for Eraser & Selection ----------------

bool PointHitsAnnotation(const AnnotationObject& obj, double lx, double ly, double tolWorld) {
    if (!obj.bounds.Contains(lx, ly, tolWorld)) return false;
    if (obj.kind == ObjectKind::InkStroke && !obj.inkPoints.empty()) {
        for (const auto& pt : obj.inkPoints) {
            if (std::hypot(pt.x - lx, pt.y - ly) <= (tolWorld + obj.strokeWidth)) return true;
        }
        return false;
    }
    return true;
}

std::vector<std::uint64_t> CollectHits(
    StudioAppState* st,
    bool isCanvas,
    std::size_t pageIdx,
    double lx,
    double ly,
    double tol,
    bool includeLockedInPlace = false)
{
    std::vector<std::uint64_t> hits;
    if (isCanvas) {
        const auto candidates = st->doc.QueryCanvasRect({ lx - tol, ly - tol, tol * 2.0, tol * 2.0 });
        for (std::uint64_t id : candidates) {
            if (const auto* o = st->doc.FindCanvasObject(id)) {
                if (o->lockedInPlace && !includeLockedInPlace) continue;
                if (PointHitsAnnotation(*o, lx, ly, tol)) hits.push_back(id);
            }
        }
    } else if (pageIdx < st->doc.Pages().size()) {
        for (const auto& o : st->doc.Pages()[pageIdx].objects) {
            if (o.lockedInPlace && !includeLockedInPlace) continue;
            if (PointHitsAnnotation(o, lx, ly, tol)) hits.push_back(o.id);
        }
    }
    return hits;
}

void UpdateAppearanceFromPoint(StudioAppState* st, POINT pt, bool isDrag);

// ---------------- Viewport WndProc ----------------

LRESULT CALLBACK ViewportWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StudioAppState* st = reinterpret_cast<StudioAppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    auto extractPointerWorld = [&](LPARAM lParam, float& outPressure, DocPoint& outScreen) {
        outPressure = 0.55f;
        outScreen = { static_cast<double>(GET_X_LPARAM(lParam)), static_cast<double>(GET_Y_LPARAM(lParam)) };
    };

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<StudioAppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_SIZE:
        if (st) st->renderer.Resize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcVp = BeginPaint(hwnd, &ps);
        if (st) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int vw = (std::max)(1, static_cast<int>(rc.right - rc.left));
            const int vh = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
            ViewportOverlayState ov{};
            PopulateViewportOverlay(st, ov);

            // Live material translúcido 2.0 floating Appearance Popover composited INSIDE Direct2D
            // (refracts real D2D scene pixels with zero GDI-over-swapchain flicker!)
            RasterImageBgra32 popPatch;
            if (st->appearancePopoverOpen && hdcVp) {
                RECT vpInMain{};
                GetWindowRect(hwnd, &vpInMain);
                MapWindowPoints(HWND_DESKTOP, st->hwndMain, reinterpret_cast<LPPOINT>(&vpInMain), 2);
                RECT popLocal = st->appearancePopoverRc;
                OffsetRect(&popLocal, -vpInMain.left, -vpInMain.top);
                RECT trkLocal = st->opacityTrackRc;
                OffsetRect(&trkLocal, -vpInMain.left, -vpInMain.top);

                const int padX = 12;
                const int padY = 14;
                const int capL = (std::max)(0, static_cast<int>(popLocal.left) - padX);
                const int capT = (std::max)(0, static_cast<int>(popLocal.top) - padY);
                const int capR = (std::min)(vw, static_cast<int>(popLocal.right) + padX);
                const int capB = (std::min)(vh, static_cast<int>(popLocal.bottom) + padY);
                const int pw = capR - capL;
                const int ph = capB - capT;
                if (pw > 16 && ph > 16) {
                    RasterImageBgra32 sceneBg;
                    if (st->renderer.RenderHwndViewportToOffscreen(st->doc, st->camera, vw, vh, &st->pageCache, sceneBg, &ov) &&
                        sceneBg.Valid()) {
                        BITMAPINFO bmi{};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = pw;
                        bmi.bmiHeader.biHeight = -ph;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;
                        void* bits = nullptr;
                        HDC memDc = CreateCompatibleDC(hdcVp);
                        HBITMAP hDib = memDc ? CreateDIBSection(hdcVp, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0) : nullptr;
                        if (memDc && hDib && bits) {
                            HGDIOBJ oldB = SelectObject(memDc, hDib);
                            auto* dst = static_cast<std::uint8_t*>(bits);
                            for (int y = 0; y < ph; ++y) {
                                const std::uint8_t* sRow = sceneBg.pixels.data() +
                                    (static_cast<std::size_t>(capT + y) * static_cast<std::size_t>(vw) + static_cast<std::size_t>(capL)) * 4u;
                                std::memcpy(dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(pw) * 4u, sRow, static_cast<std::size_t>(pw) * 4u);
                            }
                            const RECT savedPop = st->appearancePopoverRc;
                            const RECT savedTrk = st->opacityTrackRc;
                            st->appearancePopoverRc = { popLocal.left - capL, popLocal.top - capT, popLocal.right - capL, popLocal.bottom - capT };
                            st->opacityTrackRc      = { trkLocal.left - capL, trkLocal.top - capT, trkLocal.right - capL, trkLocal.bottom - capT };
                            PaintAppearancePopoverOnHdc(st, memDc, pw, ph);
                            GdiFlush();
                            st->appearancePopoverRc = savedPop;
                            st->opacityTrackRc = savedTrk;

                            popPatch.width = pw;
                            popPatch.height = ph;
                            popPatch.pixels.resize(static_cast<std::size_t>(pw) * static_cast<std::size_t>(ph) * 4u);
                            std::memcpy(popPatch.pixels.data(), bits, popPatch.pixels.size());
                            for (std::size_t k = 3; k < popPatch.pixels.size(); k += 4) popPatch.pixels[k] = 255;

                            ov.floatingOverlayBgra = &popPatch;
                            ov.floatingOverlayScreenRect = {
                                static_cast<double>(capL), static_cast<double>(capT),
                                static_cast<double>(pw), static_cast<double>(ph)
                            };
                            SelectObject(memDc, oldB);
                        }
                        if (hDib) DeleteObject(hDib);
                        if (memDc) DeleteDC(memDc);
                    }
                }
            }

            // Split View (Horizontal side-by-side or Vertical top/bottom) composited smoothly via Direct2D offscreen pass (F-066, F-067, GN-UI-092)
            RasterImageBgra32 splitPatch;
            if (st->splitViewActive && !st->appearancePopoverOpen && vw > 240 && vh > 120) {
                ViewportOverlayState sov = ov;
                sov.floatingOverlayBgra = nullptr;
                sov.showSelectionHandles = false;
                if (st->splitViewVertical) {
                    const int halfH = vh / 2;
                    const int botH = vh - halfH;
                    if (st->renderer.RenderHwndViewportToOffscreen(st->doc, st->splitCamera, vw, botH, &st->pageCache, splitPatch, &sov) &&
                        splitPatch.Valid()) {
                        // Subtle 2px horizontal splitter line on top edge of bottom split pane
                        for (int y = 0; y < 2 && y < botH; ++y) {
                            for (int x = 0; x < vw; ++x) {
                                std::uint8_t* p = &splitPatch.pixels[(static_cast<std::size_t>(y) * vw + x) * 4u];
                                p[0] = 225; p[1] = 175; p[2] = 130; p[3] = 255;
                            }
                        }
                        ov.floatingOverlayBgra = &splitPatch;
                        ov.floatingOverlayScreenRect = { 0.0, static_cast<double>(halfH), static_cast<double>(vw), static_cast<double>(botH) };
                    }
                } else {
                    const int halfW = vw / 2;
                    const int rightW = vw - halfW;
                    if (st->renderer.RenderHwndViewportToOffscreen(st->doc, st->splitCamera, rightW, vh, &st->pageCache, splitPatch, &sov) &&
                        splitPatch.Valid()) {
                        // Subtle 2px vertical splitter line on left edge of right split pane
                        for (int y = 0; y < vh; ++y) {
                            for (int x = 0; x < 2 && x < rightW; ++x) {
                                std::uint8_t* p = &splitPatch.pixels[(static_cast<std::size_t>(y) * rightW + x) * 4u];
                                p[0] = 225; p[1] = 175; p[2] = 130; p[3] = 255;
                            }
                        }
                        ov.floatingOverlayBgra = &splitPatch;
                        ov.floatingOverlayScreenRect = { static_cast<double>(halfW), 0.0, static_cast<double>(rightW), static_cast<double>(vh) };
                    }
                }
            }

            HWND mainH = st->hwndMain;
            st->renderer.RenderViewport(
                st->doc, st->camera,
                vw, vh,
                &st->pageCache,
                st->asyncGen,
                [mainH]() { if (IsWindow(mainH)) PostMessageW(mainH, WM_APP_ASYNC_PAGE_READY, 0, 0); },
                &ov);
            SyncDynamicTimers(st);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_COMMAND: {
        if (st && LOWORD(wp) == CMD_INLINE_EDIT_CTRL && HIWORD(wp) == EN_KILLFOCUS) {
            CommitInlineTextEdit(st, true);
            return 0;
        }
        break;
    }
    case WM_LBUTTONDBLCLK: {
        if (!st) break;
        if (IsDocumentLockedForEdit(st)) return 0;
        CommitInlineTextEdit(st, true);
        const DocPoint wpWorld = st->camera.ScreenToWorld(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
        std::size_t pgIdx = st->activePageIndex;
        double lx = wpWorld.x, ly = wpWorld.y;
        if (!isCanvas && !st->doc.Layout().HitTestWorldPoint(wpWorld.x, wpWorld.y, pgIdx, lx, ly)) return 0;
        const double tol = std::clamp(8.0 / (std::max)(0.25, st->camera.zoom), 3.0, 18.0);
        const auto hits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol);
        if (hits.empty()) return 0;
        const ObjectTarget tgt{ isCanvas, pgIdx };
        const AnnotationObject* obj = CanvasManip::FindObjectConst(st->doc, tgt, hits.back());
        if (!obj) return 0;
        st->selectedObjectId = obj->id;
        st->selectedIsCanvas = isCanvas;
        st->selectedPageIndex = pgIdx;
        if (obj->kind == ObjectKind::TextBox) {
            BeginInlineTextEdit(st, tgt, *obj);
        } else if (obj->kind == ObjectKind::MathBlock) {
            std::wstring exprW = Utf8ToWide(obj->formulaExprUtf8);
            if (ShowTextPrompt(st->hwndMain, L"Editar F\u00F3rmula 2D", L"Expresi\u00F3n:", exprW) && !exprW.empty()) {
                AnnotationObject before = *obj;
                AnnotationObject after = *obj;
                after.formulaExprUtf8 = WideToUtf8(exprW);
                const auto ev = Cas::EvaluateExpression(after.formulaExprUtf8);
                if (ev.ok) after.evaluatedResultUtf8 = ev.formattedResultUtf8;
                const std::string exprAfter = after.formulaExprUtf8;
                st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)));
                // Live reactive math variable cascade when editing "var = number" (AP-MATH-056)
                const auto eqPos = exprAfter.find('=');
                if (eqPos != std::string::npos && eqPos > 0) {
                    std::string lhs = exprAfter.substr(0, eqPos);
                    std::string rhs = exprAfter.substr(eqPos + 1);
                    while (!lhs.empty() && lhs.front() == ' ') lhs.erase(lhs.begin());
                    while (!lhs.empty() && lhs.back() == ' ') lhs.pop_back();
                    while (!rhs.empty() && rhs.front() == ' ') rhs.erase(rhs.begin());
                    char* endPtr = nullptr;
                    const double numVal = std::strtod(rhs.c_str(), &endPtr);
                    if (!lhs.empty() && std::isalpha(static_cast<unsigned char>(lhs.front())) && endPtr && endPtr != rhs.c_str()) {
                        MathChem::ReactiveMathChemistryEngine::UpdateDeclaredVariableAndRecompute(st->doc, st->cmdStack, tgt, lhs, numVal);
                    }
                }
                st->statusBannerUtf8 = "F\xC3\xB3rmula recalculada";
                UpdateWindowTitle(st);
                InvalidateViews(st);
            }
        } else if (obj->kind == ObjectKind::InkStroke) {
            const std::size_t converted = SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(st->doc, st->cmdStack, tgt);
            if (converted > 0) {
                st->statusBannerUtf8 = "Doble toque: " + std::to_string(converted) + " trazo(s)/diagrama(s) convertidos";
                UpdateWindowTitle(st);
                InvalidateViews(st);
            }
        } else if (obj->kind == ObjectKind::TableBlock) {
            ExecuteToolbarCommand(st, CMD_ADD_TABLE);
        } else if (obj->kind == ObjectKind::CircuitBlock) {
            std::wstring netW = Utf8ToWide(obj->circuitNetlistUtf8);
            for (wchar_t& ch : netW) if (ch == L'\n') ch = L';';
            if (ShowTextPrompt(st->hwndMain, L"Editar Circuito SPICE/MNA", L"Netlist (separado por ';'):", netW) && !netW.empty()) {
                std::string net = WideToUtf8(netW);
                for (char& ch : net) if (ch == ';') ch = '\n';
                std::vector<Circuit::CircuitComponent> comps;
                std::string perr;
                AnnotationObject before = *obj;
                AnnotationObject after = *obj;
                after.circuitNetlistUtf8 = net;
                if (Circuit::ParseNetlistText(net, comps, &perr)) {
                    const auto sol = Circuit::SolveMnaCircuit(comps, 0.0);
                    after.evaluatedResultUtf8 = sol.ok ? sol.summaryUtf8 : sol.errorUtf8;
                }
                st->cmdStack.Execute(st->doc, std::make_unique<ModifyObjectCommand>(tgt, std::move(before), std::move(after)));
                st->statusBannerUtf8 = "Circuito recalculado";
                UpdateWindowTitle(st);
                InvalidateViews(st);
            }
            } else if (obj->kind == ObjectKind::AudioClip) {
                const std::string audioKey = !obj->audioAssetKey.empty() ? obj->audioAssetKey : obj->assetKey;
                if (!audioKey.empty()) {
                    if (const auto* bytes = st->doc.FindAsset(audioKey)) {
                        st->audioSession.LoadFromWavBytes(bytes->data(), bytes->size());
                    }
                }
                ExecuteToolbarCommand(st, CMD_REPLAY_TOGGLE);
            } else if (obj->kind == ObjectKind::PdfCard) {
                ExecuteToolbarCommand(st, CMD_EDIT_SHEET_CONTENT);
            } else if (obj->kind == ObjectKind::LinkCard) {
                if (obj->linkTargetPageIndex >= 0 && static_cast<std::size_t>(obj->linkTargetPageIndex) < st->doc.Pages().size()) {
                    st->doc.SetMode(DocumentMode::PaginatedPdf);
                    CenterCameraOnPage(st, static_cast<std::size_t>(obj->linkTargetPageIndex));
                    st->statusBannerUtf8 = "Navegado a p\xC3\xA1gina " + std::to_string(obj->linkTargetPageIndex + 1);
                    LayoutMainChildren(st);
                    InvalidateViews(st);
                } else if (IsSafeExternalLinkUrl(obj->linkUrlUtf8)) {
                    ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(obj->linkUrlUtf8).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    st->statusBannerUtf8 = "Enlace abierto: " + ExtractUrlDomainUtf8(obj->linkUrlUtf8);
                    InvalidateStatusOnly(st);
                } else {
                    st->statusBannerUtf8 = "Enlace bloqueado por seguridad (solo http/https/mailto)";
                    InvalidateStatusOnly(st);
                }
            } else if (obj->kind == ObjectKind::VideoClip) {
                ExecuteToolbarCommand(st, CMD_MEDIA_PLAY_PAUSE);
            } else if (obj->kind == ObjectKind::HtmlEmbed) {
                ExecuteToolbarCommand(st, CMD_HTML_FREEZE_RESUME);
            } else if (IsAnimatedGifObject(st->doc, *obj)) {
                ExecuteToolbarCommand(st, CMD_MEDIA_PLAY_PAUSE);
            }
        return 0;
    }
    case WM_KEYDOWN:
    case WM_KEYUP: {
        if (st && st->hwndMain) {
            return SendMessageW(st->hwndMain, msg, wp, lp);
        }
        break;
    }
    case WM_POINTERCAPTURECHANGED:
    case WM_POINTERLEAVE: {
        if (st && (st->invertedTempEraser || st->barrelTempLasso)) {
            st->activeTool = st->savedToolBeforeStylusMod;
            st->invertedTempEraser = false;
            st->barrelTempLasso = false;
        }
        return 0;
    }
    case WM_CAPTURECHANGED: {
        if (st) {
            // Roll back any active interactive transform drag cleanly so lost focus never leaves partial mutations
            if (st->transformDrag.active && !st->transformDrag.beforeSnapshots.empty()) {
                for (std::size_t i = 0; i < st->transformDrag.objectIds.size() && i < st->transformDrag.beforeSnapshots.size(); ++i) {
                    if (AnnotationObject* m = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->transformDrag.objectIds[i])) {
                        *m = st->transformDrag.beforeSnapshots[i];
                    }
                }
                st->doc.BumpRevision();
                InvalidateViews(st);
            }
            if (st->invertedTempEraser || st->barrelTempLasso) {
                st->activeTool = st->savedToolBeforeStylusMod;
                st->invertedTempEraser = false;
                st->barrelTempLasso = false;
            }
            st->draggingOpacitySlider = false;
            st->panning = false;
            st->splitPanning = false;
            st->drawing = false;
            st->transformDrag.active = false;
            st->pdfSelectingText = false;
            st->laserPointer.active = false;
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (!st) break;
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || st->doc.Mode() == DocumentMode::InfiniteCanvas;
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(hwnd, &pt);
        RECT rc{};
        GetClientRect(hwnd, &rc);
        const bool onSplitSecondary = st->splitViewActive && (st->splitViewVertical ? (pt.y > (rc.bottom / 2)) : (pt.x > (rc.right / 2)));
        Camera2D& activeCam = onSplitSecondary ? st->splitCamera : st->camera;
        if (ctrl) {
            const double factor = (delta > 0) ? 1.12 : 0.89;
            const double anchorX = (onSplitSecondary && !st->splitViewVertical) ? (pt.x - rc.right / 2) : pt.x;
            const double anchorY = (onSplitSecondary && st->splitViewVertical) ? (pt.y - rc.bottom / 2) : pt.y;
            activeCam.ZoomByFactorAtAnchor(anchorX, anchorY, factor);
        } else {
            activeCam.panY -= (static_cast<double>(delta) / WHEEL_DELTA) * (72.0 / (std::max)(0.2, activeCam.zoom));
            if (!onSplitSecondary) RefreshActivePageFromCamera(st);
        }
        InvalidateViews(st);
        return 0;
    }
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        if (!st) break;
        CommitInlineTextEdit(st, true);
        RECT vrc{};
        GetClientRect(hwnd, &vrc);
        st->panning = true;
        st->splitPanning = (st->splitViewActive && (st->splitViewVertical ? (GET_Y_LPARAM(lp) > (vrc.bottom / 2)) : (GET_X_LPARAM(lp) > (vrc.right / 2))));
        st->rbuttonMoved = false;
        st->rbuttonDownPt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        st->lastPanPt = st->rbuttonDownPt;
        SetCapture(hwnd);
        return 0;
    }
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        if (st && st->panning) {
            st->panning = false;
            st->splitPanning = false;
            ReleaseCapture();
            if (msg == WM_RBUTTONUP && !st->rbuttonMoved) {
                // Auto-select object under right-click cursor so context menu immediately exposes object actions
                const DocPoint wpWorld = st->camera.ScreenToWorld(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
                st->lastContextWorldPt = wpWorld;
                st->hasLastContextWorldPt = true;
                const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
                std::size_t pgIdx = st->activePageIndex;
                double lx = wpWorld.x, ly = wpWorld.y;
                const bool hitPage = isCanvas || st->doc.Layout().HitTestWorldPoint(wpWorld.x, wpWorld.y, pgIdx, lx, ly);
                if (hitPage) {
                    const double tol = std::clamp(8.0 / (std::max)(0.25, st->camera.zoom), 3.0, 18.0);
                    auto hits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol, false);
                    if (hits.empty()) {
                        hits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol, true);
                    }
                    if (!hits.empty()) {
                        st->selectedObjectId = hits.back();
                        st->selectedIsCanvas = isCanvas;
                        st->selectedPageIndex = pgIdx;
                        InvalidateViews(st);
                    }
                }
                HMENU cm = CreatePopupMenu();
                if (st->selectedObjectId != 0 || !st->lassoSelectedIds.empty()) {
                    AppendMenuW(cm, MF_STRING, CMD_CLIPBOARD_CUT,   L"Cortar (Ctrl+X)");
                    AppendMenuW(cm, MF_STRING, CMD_CLIPBOARD_COPY,  L"Copiar (Ctrl+C)");
                }
                if (st->hasLastSnipRegion || st->selectedObjectId != 0 || !st->lassoSelectedIds.empty()) {
                    AppendMenuW(cm, MF_STRING, CMD_SNIP_REGION_AS_IMAGE, L"Copiar recorte de regi\u00F3n como imagen (Ctrl+Shift+C)");
                }
                AppendMenuW(cm, MF_STRING, CMD_CLIPBOARD_PASTE, L"Pegar (Ctrl+V)");
                AppendMenuW(cm, MF_STRING, CMD_INSERT_PRINTOUT, L"Insertar copia impresa (PDF / Word / PowerPoint / Excel)...");
                HMENU hInsMedia = CreatePopupMenu();
                AppendMenuW(hInsMedia, MF_STRING, CMD_INSERT_LINK_CARD,  L"Insertar enlace interactivo...");
                AppendMenuW(hInsMedia, MF_STRING, CMD_INSERT_GIF,        L"Insertar GIF animado...");
                AppendMenuW(hInsMedia, MF_STRING, CMD_INSERT_VIDEO_CLIP, L"Insertar video reproducible...");
                AppendMenuW(hInsMedia, MF_STRING, CMD_INSERT_HTML_EMBED, L"Insertar bloque HTML animado...");
                AppendMenuW(cm, MF_POPUP, reinterpret_cast<UINT_PTR>(hInsMedia), L"Insertar multimedia");
                if (isCanvas) {
                    AppendMenuW(cm, MF_STRING, CMD_ADJUST_SHEET_SPACING,  L"Espacio personalizado entre hojas...");
                    AppendMenuW(cm, MF_STRING, CMD_INSERT_VERTICAL_SPACE, L"Insertar / quitar espacio vertical desde aqu\u00ED...");
                }
                if (st->selectedObjectId != 0 || !st->lassoSelectedIds.empty()) {
                    const ObjectTarget selTgt{ st->selectedIsCanvas, st->selectedPageIndex };
                    const auto* selObj = CanvasManip::FindObjectConst(st->doc, selTgt, st->selectedObjectId);
                    const bool isLocked = selObj && selObj->lockedInPlace;
                    if (selObj && selObj->kind == ObjectKind::VideoClip) {
                        AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                        const bool isPlayingThis = st->videoPlayer.IsPlaying() && st->videoPlayer.ActiveObjectId() == selObj->id;
                        AppendMenuW(cm, MF_STRING, CMD_MEDIA_PLAY_PAUSE, isPlayingThis ? L"Pausar video" : L"Reproducir video");
                        AppendMenuW(cm, MF_STRING, CMD_MEDIA_CAPTURE_FRAME, L"Capturar fotograma como imagen anotable");
                    } else if (selObj && selObj->kind == ObjectKind::HtmlEmbed) {
                        AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                        const bool isLive = (st->activeHtmlObjectId == selObj->id);
                        AppendMenuW(cm, MF_STRING, CMD_HTML_FREEZE_RESUME, isLive ? L"Congelar HTML (suspender 0% GPU)" : L"Activar HTML interactivo en vivo");
                        AppendMenuW(cm, MF_STRING, CMD_MEDIA_CAPTURE_FRAME, L"Capturar vista HTML como imagen");
                    } else if (selObj && IsAnimatedGifObject(st->doc, *selObj)) {
                        AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                        AppendMenuW(cm, MF_STRING, CMD_MEDIA_PLAY_PAUSE, selObj->gifPaused ? L"Reanudar GIF animado" : L"Pausar GIF animado (0% CPU)");
                    } else if (selObj && selObj->kind == ObjectKind::LinkCard && IsSafeExternalLinkUrl(selObj->linkUrlUtf8)) {
                        st->lastContextInlineUrlUtf8 = selObj->linkUrlUtf8;
                        AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                        AppendMenuW(cm, MF_STRING, CMD_GEMINI_NOTEBOOK_SEND_URL, L"Enviar enlace a Gemini Notebook...");
                    } else if (selObj && selObj->kind == ObjectKind::TextBox) {
                        InlineUrlSpan hitUrl{};
                        bool foundUrl = st->renderer.HitTestTextBoxInlineUrl(
                            *selObj, lx, ly, st->camera.zoom, st->uiPrefs.accessibility.dyslexiaFriendlyMode, hitUrl);
                        if (!foundUrl) {
                            const auto allUrls = ExtractSafeInlineUrls(selObj->textUtf8);
                            if (!allUrls.empty()) { hitUrl = allUrls.front(); foundUrl = true; }
                        }
                        if (foundUrl && IsSafeExternalLinkUrl(hitUrl.normalizedUrlUtf8)) {
                            st->lastContextInlineUrlUtf8 = hitUrl.normalizedUrlUtf8;
                            AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                            const std::wstring openLbl = L"Abrir enlace (" + Utf8ToWide(ExtractUrlDomainUtf8(hitUrl.normalizedUrlUtf8)) + L")";
                            AppendMenuW(cm, MF_STRING, 61999, openLbl.c_str());
                            AppendMenuW(cm, MF_STRING, CMD_TEXTBOX_URL_TO_LINKCARD, L"Convertir enlace en tarjeta interactiva");
                            AppendMenuW(cm, MF_STRING, CMD_GEMINI_NOTEBOOK_SEND_URL, L"Enviar enlace a Gemini Notebook...");
                        }
                    }
                    AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(cm, MF_STRING, CMD_TOGGLE_LOCK_IN_PLACE,
                        isLocked ? L"Desfijar copia impresa del fondo (para arrastrar/mover)" : L"Fijar copia impresa al fondo");
                    AppendMenuW(cm, MF_STRING, CMD_EDIT_SHEET_CONTENT,  L"Editar hoja / Extraer texto editable...");
                    AppendMenuW(cm, MF_STRING, CMD_SEARCH_IN_THIS_SHEET, L"Buscar en esta hoja...");
                    AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(cm, MF_STRING, CMD_DUPLICATE_OBJECTS, L"Duplicar (Ctrl+D)");
                    AppendMenuW(cm, MF_STRING, CMD_GROUP_OBJECTS,     L"Agrupar (Ctrl+G)");
                    AppendMenuW(cm, MF_STRING, CMD_UNGROUP_OBJECTS,   L"Desagrupar");
                    AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(cm, MF_STRING, CMD_COPY_STROKE_STYLE, L"Copiar estilo al l\u00E1piz");
                    AppendMenuW(cm, MF_STRING, CMD_RESTYLE_INK,       L"Enderezar / Estilizar tinta");
                    AppendMenuW(cm, MF_STRING, CMD_HTR_TO_TEXT,       L"Escritura -> Texto editable");
                    AppendMenuW(cm, MF_STRING, CMD_HTR_TO_MATH,       L"Escritura -> Ecuaci\u00F3n 2D");
                    AppendMenuW(cm, MF_SEPARATOR, 0, nullptr);
                    AppendMenuW(cm, MF_STRING, CMD_IMG_ENHANCE_WHITEBOARD, L"Imagen: Mejorar pizarra");
                    AppendMenuW(cm, MF_STRING, CMD_IMG_ROTATE_90,          L"Imagen: Rotar +90\u00B0");
                    AppendMenuW(cm, MF_STRING, CMD_TEXT_BOLD,              L"Texto: Negrita (Ctrl+B)");
                    AppendMenuW(cm, MF_STRING, CMD_TEXT_ITALIC,            L"Texto: Cursiva (Ctrl+I)");
                    AppendMenuW(cm, MF_STRING, CMD_TEXT_UNDERLINE,         L"Texto: Subrayado (Ctrl+U)");
                    AppendMenuW(cm, MF_STRING, CMD_TEXT_BULLET_LIST,       L"Texto: Vi\u00F1""etas (\u2022)");
                    AppendMenuW(cm, MF_STRING, CMD_TEXT_CHECKBOX_TOGGLE,   L"Texto: Casilla (\u2610 / \u2611)");
                }
                POINT spt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ClientToScreen(hwnd, &spt);
                const UINT cmd = UiGlass::TrackTranslucentPopupMenu(cm, spt, st->hwndMain, st->glassSettings, false);
                DestroyMenu(cm);
                if (cmd == 61999 && IsSafeExternalLinkUrl(st->lastContextInlineUrlUtf8)) {
                    ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(st->lastContextInlineUrlUtf8).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    st->statusBannerUtf8 = "Enlace abierto: " + ExtractUrlDomainUtf8(st->lastContextInlineUrlUtf8);
                    InvalidateStatusOnly(st);
                } else if (cmd != 0) {
                    ExecuteToolbarCommand(st, static_cast<WORD>(cmd));
                }
            }
        }
        return 0;
    }
    case WM_SETCURSOR: {
        if (st && LOWORD(lp) == HTCLIENT) {
            const bool ctrlDown = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrlDown || st->readingViewMode) {
                POINT pt{};
                if (GetCursorPos(&pt) && ScreenToClient(hwnd, &pt)) {
                    const DocPoint wpWorld = st->camera.ScreenToWorld(pt.x, pt.y);
                    const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
                    std::size_t pgIdx = st->activePageIndex;
                    double lx = wpWorld.x, ly = wpWorld.y;
                    if (isCanvas || st->doc.Layout().HitTestWorldPoint(wpWorld.x, wpWorld.y, pgIdx, lx, ly)) {
                        const auto hits = CollectHits(st, isCanvas, pgIdx, lx, ly, 4.0, true);
                        for (auto it = hits.rbegin(); it != hits.rend(); ++it) {
                            const auto* o = CanvasManip::FindObjectConst(st->doc, ObjectTarget{ isCanvas, pgIdx }, *it);
                            if (!o) continue;
                            if (o->kind == ObjectKind::LinkCard) {
                                SetCursor(LoadCursorW(nullptr, IDC_HAND));
                                return TRUE;
                            }
                            if (o->kind == ObjectKind::TextBox) {
                                InlineUrlSpan sp{};
                                if (st->renderer.HitTestTextBoxInlineUrl(*o, lx, ly, st->camera.zoom,
                                        st->uiPrefs.accessibility.dyslexiaFriendlyMode, sp)) {
                                    SetCursor(LoadCursorW(nullptr, IDC_HAND));
                                    return TRUE;
                                }
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    case WM_POINTERDOWN:
    case WM_LBUTTONDOWN: {
        if (!st) break;
        CommitInlineTextEdit(st, true);
        if (st->appearancePopoverOpen) {
            POINT ptMain{};
            if (msg == WM_POINTERDOWN) {
                POINTER_INFO piPop{};
                if (GetPointerInfo(GET_POINTERID_WPARAM(wp), &piPop)) {
                    ptMain = piPop.ptPixelLocation;
                    ScreenToClient(st->hwndMain, &ptMain);
                }
            } else {
                ptMain = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                MapWindowPoints(hwnd, st->hwndMain, &ptMain, 1);
            }
            if (PtInRect(&st->appearancePopoverRc, ptMain)) {
                if (ptMain.x >= st->opacityTrackRc.left - 8 && ptMain.x <= st->opacityTrackRc.right + 8 &&
                    ptMain.y >= st->opacityTrackRc.top - 6 && ptMain.y <= st->opacityTrackRc.bottom + 6) {
                    st->draggingOpacitySlider = true;
                    SetCapture(hwnd);
                }
                UpdateAppearanceFromPoint(st, ptMain, false);
                return 0;
            }
            st->appearancePopoverOpen = false;
            InvalidateViews(st);
        }
        SetFocus(hwnd);
        float pressure = 0.55f;
        float tiltX = 0.0f;
        float tiltY = 0.0f;
        DocPoint sp{};
        if (msg == WM_POINTERDOWN) {
            const UINT32 pid = GET_POINTERID_WPARAM(wp);
            POINTER_INFO pi{};
            if (GetPointerInfo(pid, &pi)) {
                // Strict Palm Rejection (F-036): ignore touch while pen is active/recent
                if (pi.pointerType == PT_TOUCH && (GetTickCount64() - st->lastPenInputTickMs) < 1200ull) {
                    return 0;
                }
                POINT pt = pi.ptPixelLocation;
                ScreenToClient(hwnd, &pt);
                sp = { static_cast<double>(pt.x), static_cast<double>(pt.y) };
                POINTER_PEN_INFO pen{};
                if (pi.pointerType == PT_PEN && GetPointerPenInfo(pid, &pen)) {
                    st->lastPenInputTickMs = GetTickCount64();
                    pressure = std::clamp(static_cast<float>(pen.pressure) / 1024.0f, 0.1f, 1.0f);
                    tiltX = static_cast<float>(pen.tiltX);
                    tiltY = static_cast<float>(pen.tiltY);
                    // Inverted stylus eraser (F-037) & Barrel button temporary Lasso (F-038)
                    if ((pen.penFlags & PEN_FLAG_INVERTED) || (pen.penFlags & PEN_FLAG_ERASER)) {
                        if (!st->invertedTempEraser) {
                            st->savedToolBeforeStylusMod = st->activeTool;
                            st->activeTool = ToolKind::Eraser;
                            st->invertedTempEraser = true;
                        }
                    } else if (pen.penFlags & PEN_FLAG_BARREL) {
                        if (!st->barrelTempLasso) {
                            st->savedToolBeforeStylusMod = st->activeTool;
                            st->activeTool = ToolKind::Lasso;
                            st->barrelTempLasso = true;
                        }
                    }
                }
            }
        } else {
            extractPointerWorld(lp, pressure, sp);
        }

        if (st->uiPrefs.accessibility.readingRulerEnabled && std::abs(st->readingRulerCursorY - sp.y) >= 1.5) {
            st->readingRulerCursorY = sp.y;
            if (!st->drawing && !st->panning && !st->transformDrag.active) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        // Native Stylus / Touch Double-Tap over InkStroke -> Convert Handwriting & Diagrams (MS-INK-020)
        if (msg == WM_POINTERDOWN && !st->graphOverlayVisible && !st->dbPanelVisible && !st->readingViewMode && !IsDocumentLockedForEdit(st)) {
            const ULONGLONG nowTap = GetTickCount64();
            const ULONGLONG dblMs = (std::max<ULONGLONG>)(250ull, static_cast<ULONGLONG>(GetDoubleClickTime()));
            const bool isDoubleTap = (st->lastPointerTapTickMs > 0) &&
                ((nowTap - st->lastPointerTapTickMs) <= dblMs) &&
                (std::hypot(sp.x - st->lastPointerTapScreenPt.x, sp.y - st->lastPointerTapScreenPt.y) <= 16.0);
            st->lastPointerTapTickMs = nowTap;
            st->lastPointerTapScreenPt = sp;
            if (isDoubleTap) {
                const DocPoint tapWorld = st->camera.ScreenToWorld(sp.x, sp.y);
                const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
                std::size_t pgIdx = st->activePageIndex;
                double tlx = tapWorld.x, tly = tapWorld.y;
                if (isCanvas || st->doc.Layout().HitTestWorldPoint(tapWorld.x, tapWorld.y, pgIdx, tlx, tly)) {
                    const double tol = std::clamp(10.0 / (std::max)(0.25, st->camera.zoom), 4.0, 20.0);
                    const auto tapHits = CollectHits(st, isCanvas, pgIdx, tlx, tly, tol);
                    if (!tapHits.empty()) {
                        const ObjectTarget tgt{ isCanvas, pgIdx };
                        const AnnotationObject* hitObj = CanvasManip::FindObjectConst(st->doc, tgt, tapHits.back());
                        if (hitObj && hitObj->kind == ObjectKind::InkStroke) {
                            const std::size_t converted = SmartScriptReflowEngine::ConvertHandwritingAndDiagramsOnDoubleTap(st->doc, st->cmdStack, tgt);
                            if (converted > 0) {
                                st->lastPointerTapTickMs = 0;
                                st->statusBannerUtf8 = "Doble toque de l\xC3\xA1piz: " + std::to_string(converted) + " trazo(s)/diagrama(s) convertidos";
                                UpdateWindowTitle(st);
                                InvalidateViews(st);
                                return 0;
                            }
                        }
                    }
                }
            }
        }

        // Interactive Direct2D Overlay Hit-Testing (Graph, Database 9-Views, Flexcil Citations, Audio Scrubber, Comment Pins, Zoom Box)
        RECT vpRc{};
        GetClientRect(hwnd, &vpRc);
        const int vw = static_cast<int>(vpRc.right - vpRc.left);
        const int vh = static_cast<int>(vpRc.bottom - vpRc.top);

        // A. Clickable Knowledge Graph Overlay
        if (st->graphOverlayVisible) {
            const DocRect gr = OverlayGeometry::GraphPanelRect(vw, vh);
            if (gr.Contains(sp.x, sp.y)) {
                // Header buttons: [Local/Global] and [Filtrar]
                if (sp.y >= gr.y + 8.0 && sp.y <= gr.y + 30.0) {
                    if (sp.x >= gr.Right() - 160.0 && sp.x <= gr.Right() - 88.0) {
                        st->graphLocalMode = !st->graphLocalMode;
                        std::string centerTitle;
                        for (const auto& nd : st->activeGraphView.nodes) {
                            if (nd.isCenter && !nd.labelUtf8.empty()) { centerTitle = nd.labelUtf8; break; }
                        }
                        if (centerTitle.empty()) {
                            const auto activeNotes = st->knowledgeVault.ActiveNotes();
                            if (!activeNotes.empty()) centerTitle = activeNotes.front().titleUtf8;
                        }
                        st->activeGraphView = (st->graphLocalMode && !centerTitle.empty())
                            ? st->knowledgeVault.BuildLocalGraph(centerTitle, 1, st->graphFilter)
                            : st->knowledgeVault.BuildGlobalGraph(st->graphFilter);
                        KnowledgeVault::ComputeGraphLayout2D(st->activeGraphView, 60, 800.0, 560.0);
                        st->statusBannerUtf8 = st->graphLocalMode ? "Grafo Local activado" : "Grafo Global activado";
                        InvalidateViews(st);
                        return 0;
                    }
                    if (sp.x >= gr.Right() - 82.0 && sp.x <= gr.Right() - 14.0) {
                        ExecuteToolbarCommand(st, CMD_GRAPH_LOCAL_FILTER);
                        return 0;
                    }
                }
                const double padX = 36.0, padTop = 44.0, padBot = 28.0;
                const double sx = (gr.w - padX * 2.0) / 800.0;
                const double sy = (gr.h - padTop - padBot) / 560.0;
                for (auto& nd : st->activeGraphView.nodes) {
                    const double nx = gr.x + padX + nd.x * sx;
                    const double ny = gr.y + padTop + nd.y * sy;
                    if (std::hypot(sp.x - nx, sp.y - ny) <= 14.0) {
                        const std::string clickedId = nd.idUtf8;
                        const std::string clickedLabel = nd.labelUtf8;
                        const int clickedDeg = nd.degree;
                        for (auto& n2 : st->activeGraphView.nodes) n2.isCenter = (n2.idUtf8 == clickedId);
                        if (st->graphLocalMode && !clickedLabel.empty()) {
                            st->activeGraphView = st->knowledgeVault.BuildLocalGraph(clickedLabel, 1, st->graphFilter);
                            KnowledgeVault::ComputeGraphLayout2D(st->activeGraphView, 60, 800.0, 560.0);
                        }
                        if (const auto* note = st->knowledgeVault.FindNoteById(clickedId)) {
                            st->cachedReadingTextUtf8 = note->bodyMarkdownUtf8;
                            st->statusBannerUtf8 = "Nodo de grafo seleccionado: " + note->titleUtf8 + " (" + std::to_string(clickedDeg) + " enlaces)";
                        } else {
                            st->statusBannerUtf8 = "Nodo seleccionado: " + clickedLabel;
                        }
                        InvalidateViews(st);
                        return 0;
                    }
                }
                return 0;
            }
            st->graphOverlayVisible = false;
            InvalidateViews(st);
            return 0;
        }

        // B. Clickable Structured Database 9-View Panel
        if (st->dbPanelVisible) {
            const DocRect dr = OverlayGeometry::DatabasePanelRect(vw, vh);
            if (dr.Contains(sp.x, sp.y)) {
                // Header action buttons: [Editar BD] [Formulario] [Accion]
                if (sp.y >= dr.y + 10.0 && sp.y <= dr.y + 32.0) {
                    for (int ai = 0; ai < 3; ++ai) {
                        const double bx0 = dr.Right() - 246.0 + ai * 78.0;
                        if (sp.x >= bx0 && sp.x <= bx0 + 72.0) {
                            const WORD actCmd = (ai == 0) ? CMD_DB_EDITOR_MGR
                                              : ((ai == 1) ? CMD_DB_SUBMIT_FORM : CMD_DB_ACTION_BUTTON);
                            ExecuteToolbarCommand(st, actCmd);
                            return 0;
                        }
                    }
                }
                const double tabY0 = dr.y + 38.0, tabY1 = dr.y + 64.0;
                if (sp.y >= tabY0 && sp.y <= tabY1 && sp.x >= dr.x + 18.0 && sp.x <= dr.Right() - 18.0) {
                    const double tabW = (dr.w - 36.0) / 9.0;
                    const int ti = std::clamp(static_cast<int>((sp.x - (dr.x + 18.0)) / (std::max)(1.0, tabW)), 0, 8);
                    st->activeDbViewCycle = ti;
                    st->statusBannerUtf8 = "Vista de Base de Datos cambiada (#" + std::to_string(ti + 1) + ")";
                    InvalidateViews(st);
                    return 0;
                }
                // Kanban click: advance first matching card status & trigger AutomationRuleEngine
                if (st->activeDbViewCycle == 3 && !st->activeDatabase.Records().empty()) {
                    std::string statusCol;
                    for (const auto& c : st->activeDatabase.Columns()) {
                        if (c.type == DbColumnType::Status) { statusCol = c.idUtf8; break; }
                    }
                    if (!statusCol.empty()) {
                        const auto& rec = st->activeDatabase.Records().front();
                        std::string curVal = "Pendiente";
                        if (auto it = rec.cells.find(statusCol); it != rec.cells.end()) curVal = it->second.textUtf8;
                        const std::string nextVal = (curVal == "Pendiente") ? "En curso" : ((curVal == "En curso") ? "Completado" : "Pendiente");
                        st->activeDatabase.MoveKanbanCard(rec.recordIdUtf8, statusCol, nextVal);
                        const std::size_t fired = st->automations.TriggerEvent(
                            AutomationTriggerKind::OnPropertyChanged, st->activeDatabase, rec.recordIdUtf8, statusCol, nextVal);
                        st->statusBannerUtf8 = "Kanban: '" + rec.titleUtf8 + "' -> " + nextVal +
                            (fired == 0 ? "" : (" (" + std::to_string(fired) + " regla(s) ejecutada(s))"));
                        InvalidateViews(st);
                        return 0;
                    }
                }
                return 0;
            }
            st->dbPanelVisible = false;
            InvalidateViews(st);
            return 0;
        }

        // C. Clickable Flexcil Pop-up Study Note (Gesture pill & Citation jump cards)
        if (st->flexcilOverlay.IsPopupNoteVisible()) {
            const DocRect pr = OverlayGeometry::FlexcilPopupPanelRect(vw, vh);
            if (pr.Contains(sp.x, sp.y)) {
                if (sp.x >= pr.Right() - 116.0 && sp.y <= pr.y + 36.0) {
                    ExecuteToolbarCommand(st, CMD_FLEXCIL_GESTURE_MODE);
                    return 0;
                }
                const auto& studyDoc = st->flexcilOverlay.StudyNoteDocument();
                if (!studyDoc.Pages().empty()) {
                    const int cardIdx = static_cast<int>((sp.y - (pr.y + 50.0)) / 66.0);
                    if (cardIdx >= 0 && static_cast<std::size_t>(cardIdx) < studyDoc.Pages()[0].objects.size()) {
                        const auto objId = studyDoc.Pages()[0].objects[static_cast<std::size_t>(cardIdx)].id;
                        const auto target = st->flexcilOverlay.JumpFromStudyNoteObjectToSourcePdf(objId);
                        if (target.valid && target.sourcePage0 < st->doc.Pages().size()) {
                            st->doc.SetMode(DocumentMode::PaginatedPdf);
                            CenterCameraOnPage(st, target.sourcePage0);
                            st->statusBannerUtf8 = "Saltado a cita de estudio en p\xC3\xA1gina " + std::to_string(target.sourcePage0 + 1);
                            InvalidateViews(st);
                        }
                    }
                }
                return 0;
            }
        }

        // D. Clickable Audio Timeline Scrubber Bar
        if ((st->audioTimelineVisible || st->audioSession.IsContinuousRecording() || st->audioSession.IsPlaying()) && !st->zoomWritingBox.active) {
            const DocRect ab = OverlayGeometry::AudioTimelineBarRect(vw, vh);
            if (ab.Contains(sp.x, sp.y)) {
                const double wfL = ab.x + 172.0, wfR = ab.Right() - 16.0;
                if (sp.x >= wfL && sp.x <= wfR && st->audioSession.DurationMs() > 0) {
                    const double ratio = std::clamp((sp.x - wfL) / (std::max)(1.0, wfR - wfL), 0.0, 1.0);
                    const std::uint64_t seekMs = static_cast<std::uint64_t>(ratio * static_cast<double>(st->audioSession.DurationMs()));
                    st->audioSession.SeekMs(seekMs);
                    st->replayCursorMs = seekMs;
                    st->statusBannerUtf8 = "L\xC3\xADnea de audio posicionada en " + std::to_string(seekMs / 1000) + " s";
                    InvalidateViews(st);
                }
                return 0;
            }
        }

        // E. Clickable Anchored Comment Pins
        if (!st->commentThreads.AllThreads().empty() && st->doc.Mode() == DocumentMode::PaginatedPdf) {
            for (const auto& th : st->commentThreads.AllThreads()) {
                if (th.resolved || th.pageIndex0 >= st->doc.Pages().size()) continue;
                if (const auto* pl = st->doc.Layout().PlacementAt(th.pageIndex0)) {
                    const DocRect pw = pl->WorldRect();
                    const DocPoint pScr = st->camera.WorldToScreen(
                        pw.x + std::clamp(th.anchorRect.x, 12.0, (std::max)(24.0, pw.w - 16.0)),
                        pw.y + std::clamp(th.anchorRect.y, 12.0, (std::max)(24.0, pw.h - 16.0)));
                    if (std::hypot(sp.x - pScr.x, sp.y - pScr.y) <= 13.0) {
                        st->selectedCommentThreadId = th.threadId;
                        st->commentsPanelVisible = true;
                        st->statusBannerUtf8 = "Hilo de comentario #" + std::to_string(th.threadId) + " seleccionado";
                        InvalidateViews(st);
                        return 0;
                    }
                }
            }
        }

        // F. Zoom Writing Box Magnified Bottom Dock Input (GN-INK-024 / NT-INK-019)
        if (st->zoomWritingBox.active && st->doc.Mode() == DocumentMode::PaginatedPdf && !st->doc.Pages().empty()) {
            const DocRect dock = OverlayGeometry::ZoomBoxDockRect(vw, vh);
            if (dock.Contains(sp.x, sp.y)) {
                if (IsDocumentLockedForEdit(st)) return 0;
                const double waL = dock.x + 12.0, waT = dock.y + 26.0;
                const double waW = (std::max)(1.0, dock.w - 24.0), waH = (std::max)(1.0, dock.h - 36.0);
                const double rx = std::clamp((sp.x - waL) / waW, 0.0, 1.0);
                const double ry = std::clamp((sp.y - waT) / waH, 0.0, 1.0);
                const double plx = st->zoomWritingBox.pageTargetRect.x + rx * st->zoomWritingBox.pageTargetRect.w;
                const double ply = st->zoomWritingBox.pageTargetRect.y + ry * st->zoomWritingBox.pageTargetRect.h;

                st->drawingInZoomBox = true;
                st->drawing = true;
                st->draftIsCanvas = false;
                st->draftPageIdx = (std::min)(st->activePageIndex, st->doc.Pages().size() - 1);
                st->draftObj = AnnotationObject{};
                st->draftObj.kind = ObjectKind::InkStroke;
                st->draftObj.layerId = st->doc.ActiveLayerId();
                st->draftObj.strokeColor = st->penColor;
                st->draftObj.strokeWidth = st->penWidth;
                st->draftObj.penStyle = st->penStyle;
                st->draftObj.opacity = st->penOpacity;
                const std::uint64_t relMs = CurrentStrokeRelMs(st);
                st->draftObj.inkStartTimeMs = relMs;
                st->draftObj.inkEndTimeMs = relMs;
                st->draftObj.inkPoints.push_back({ plx, ply, pressure, tiltX, tiltY, relMs });
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        const DocPoint wpWorld = st->camera.ScreenToWorld(sp.x, sp.y);
        bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
        std::size_t pgIdx = st->activePageIndex;
        double lx = wpWorld.x, ly = wpWorld.y;
        bool insidePage = isCanvas;
        if (!isCanvas) {
            // Check collapsed page header banner click first (F-055)
            for (std::size_t pi = 0; pi < st->doc.Layout().PageCount(); ++pi) {
                const auto* pl = st->doc.Layout().PlacementAt(pi);
                if (pl && pl->collapsedHeader && pl->WorldRect().Contains(wpWorld.x, wpWorld.y)) {
                    st->doc.ToggleCollapsedRange(static_cast<int>(pl->pageIndex), static_cast<int>(pl->collapsedEndPage0));
                    st->statusBannerUtf8 = "Rango de p\xC3\xA1ginas expandido";
                    UpdateWindowTitle(st);
                    InvalidateViews(st);
                    return 0;
                }
            }
            insidePage = st->doc.Layout().HitTestWorldPoint(wpWorld.x, wpWorld.y, pgIdx, lx, ly);
            if (insidePage) st->activePageIndex = pgIdx;
        }

        const bool spacePan = (GetKeyState(VK_SPACE) & 0x8000) != 0;
        if (st->activeTool == ToolKind::Hand && !spacePan && insidePage) {
            const ObjectTarget tgt{ isCanvas, pgIdx };
            double originX = 0.0, originY = 0.0;
            if (!isCanvas) {
                if (const auto* pl = st->doc.Layout().PlacementAt(pgIdx)) {
                    const DocRect wr = pl->WorldRect();
                    originX = wr.x; originY = wr.y;
                    // 1. Clickable InkLink badge navigation (F-132)
                    if (const InkLink* lk = CanvasManip::HitTestInkLink(st->doc, pgIdx, { lx, ly }, wr.w, wr.h)) {
                        if (lk->targetIsCanvas) {
                            st->doc.SetMode(DocumentMode::InfiniteCanvas);
                            if (const auto* tObj = st->doc.FindCanvasObject(lk->targetObjectId)) {
                                st->selectedObjectId = tObj->id;
                                st->camera.panX = tObj->bounds.x - 100.0;
                                st->camera.panY = tObj->bounds.y - 80.0;
                            }
                        } else if (lk->targetPageIndex0 >= 0 && static_cast<std::size_t>(lk->targetPageIndex0) < st->doc.Pages().size()) {
                            st->doc.SetMode(DocumentMode::PaginatedPdf);
                            CenterCameraOnPage(st, static_cast<std::size_t>(lk->targetPageIndex0));
                        }
                        st->statusBannerUtf8 = "Navegado a enlace: " + lk->labelUtf8;
                        LayoutMainChildren(st);
                        InvalidateViews(st);
                        return 0;
                    }
                    // 2. Clickable PDF hyperlink (F-082) — strict allow-list security guard
                    if (const PdfLinkRect* plk = CanvasManip::HitTestPdfLink(st->doc.Pages()[pgIdx], { lx, ly }, wr.w, wr.h)) {
                        if (!plk->uriUtf8.empty()) {
                            if (IsSafeExternalLinkUrl(plk->uriUtf8)) {
                                ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(plk->uriUtf8).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                                st->statusBannerUtf8 = "Enlace abierto: " + ExtractUrlDomainUtf8(plk->uriUtf8);
                            } else {
                                st->statusBannerUtf8 = "Enlace PDF bloqueado por seguridad (solo http/https/mailto)";
                            }
                        } else if (plk->targetPage0 >= 0 && static_cast<std::size_t>(plk->targetPage0) < st->doc.Pages().size()) {
                            CenterCameraOnPage(st, static_cast<std::size_t>(plk->targetPage0));
                            st->statusBannerUtf8 = "Ir a p\xC3\xA1gina " + std::to_string(plk->targetPage0 + 1);
                        }
                        InvalidateViews(st);
                        return 0;
                    }
                    // 3. Interactive AcroForm field (F-084)
                    const int ffIdx = CanvasManip::HitTestFormField(st->doc.Pages()[pgIdx], { lx, ly }, wr.w, wr.h);
                    if (ffIdx >= 0) {
                        auto& ff = st->doc.MutablePages()[pgIdx].formFields[static_cast<std::size_t>(ffIdx)];
                        if (!ff.readOnly) {
                            if (ff.fieldTypeUtf8 == "Btn") {
                                ff.checked = !ff.checked;
                                st->doc.BumpRevision();
                                st->statusBannerUtf8 = "Casilla '" + ff.nameUtf8 + "' actualizada";
                            } else {
                                std::wstring vW = Utf8ToWide(ff.valueUtf8);
                                if (ShowTextPrompt(st->hwndMain, L"Campo de Formulario PDF", Utf8ToWide(ff.nameUtf8).c_str(), vW)) {
                                    ff.valueUtf8 = WideToUtf8(vW);
                                    st->doc.BumpRevision();
                                    st->statusBannerUtf8 = "Campo '" + ff.nameUtf8 + "' guardado";
                                }
                            }
                            InvalidateViews(st);
                            return 0;
                        }
                    }
                }
            }

            // 4. Hit-test 8 resize handles + rotation stalk on currently selected object (F-026, F-103)
            if (st->selectedObjectId != 0 && st->selectedIsCanvas == isCanvas && (isCanvas || st->selectedPageIndex == pgIdx)) {
                if (const auto* selObj = CanvasManip::FindObjectConst(st->doc, tgt, st->selectedObjectId)) {
                    if (!selObj->lockedInPlace) {
                    const auto hKind = CanvasManip::HitTestSelectionHandles(
                        selObj->bounds, originX, originY, st->camera,
                        static_cast<int>(std::lround(sp.x)), static_cast<int>(std::lround(sp.y)));
                    if (hKind != CanvasManip::HandleKind::None && hKind != CanvasManip::HandleKind::Body) {
                        if (IsDocumentLockedForEdit(st)) return 0;
                        st->transformDrag = CanvasManip::ActiveTransformDrag{};
                        st->transformDrag.active = true;
                        st->transformDrag.handle = hKind;
                        st->transformDrag.target = tgt;
                        st->transformDrag.objectIds = { selObj->id };
                        st->transformDrag.beforeSnapshots = { *selObj };
                        st->transformDrag.startWorld = { lx, ly };
                        st->transformDrag.initialBounds = selObj->bounds;
                        const double cx = selObj->bounds.x + selObj->bounds.w * 0.5;
                        const double cy = selObj->bounds.y + selObj->bounds.h * 0.5;
                        st->transformDrag.initialAngleRad = std::atan2(ly - cy, lx - cx);
                        st->transformDrag.initialRotDeg = selObj->rotationDeg;
                        st->panMoved = false;
                        st->panStartPt = { static_cast<LONG>(sp.x), static_cast<LONG>(sp.y) };
                        SetCapture(hwnd);
                        return 0;
                    }
                    }
                }
            }

            // 5. Direct object hit -> Drag-to-move, Multi-lasso drag, 3D surface rotation (F-237), or Audio Seek (F-156)
            // Holding Alt with Hand tool (or clicking an already-selected PdfCard) allows dragging even a pinned sheet directly
            const double tol = std::clamp(8.0 / (std::max)(0.25, st->camera.zoom), 3.0, 18.0);
            const bool altHeld = (GetKeyState(VK_MENU) & 0x8000) != 0;
            auto hits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol, altHeld);
            if (hits.empty() && st->selectedObjectId != 0) {
                // If user already selected a sheet via right-click and now drags it with Hand tool
                auto lockedHits = CollectHits(st, isCanvas, pgIdx, lx, ly, tol, true);
                if (!lockedHits.empty() && lockedHits.back() == st->selectedObjectId) {
                    const auto* selO = CanvasManip::FindObjectConst(st->doc, tgt, st->selectedObjectId);
                    if (selO && (!selO->lockedInPlace || altHeld)) {
                        hits = std::move(lockedHits);
                    }
                }
            }
            const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (!hits.empty()) {
                const std::uint64_t hitId = hits.back();
                const auto* hitObj = CanvasManip::FindObjectConst(st->doc, tgt, hitId);
                if (hitObj && shiftHeld && (hitObj->kind == ObjectKind::AudioClip || (hitObj->kind == ObjectKind::InkStroke && hitObj->inkStartTimeMs > 0))) {
                    st->audioSession.SeekMs(hitObj->inkStartTimeMs);
                    if (st->audioSession.DurationMs() > 0) {
                        st->audioSession.StartAudioPlayback(hitObj->inkStartTimeMs);
                    }
                    st->replayCursorMs = hitObj->inkStartTimeMs;
                    st->statusBannerUtf8 = "Audio posicionado en " + std::to_string(hitObj->inkStartTimeMs / 1000) + " s";
                    InvalidateViews(st);
                    return 0;
                }

                // Expand selection with group or existing multi-lasso selection
                std::vector<std::uint64_t> dragIds;
                const bool inLasso = std::find(st->lassoSelectedIds.begin(), st->lassoSelectedIds.end(), hitId) != st->lassoSelectedIds.end();
                if (inLasso && st->lassoSelectedIds.size() > 1) {
                    dragIds = st->lassoSelectedIds;
                } else {
                    dragIds = { hitId };
                    CanvasManip::ExpandSelectionWithGroupIds(st->doc, tgt, dragIds);
                    // When dragging a printed PdfCard sheet, automatically carry along any ink strokes / annotations drawn on top of it
                    if (hitObj && hitObj->kind == ObjectKind::PdfCard && tgt.isCanvas) {
                        for (const auto& o : st->doc.CanvasObjects()) {
                            if (o.id == hitId || o.kind == ObjectKind::PdfCard) continue;
                            const DocPoint c{ o.bounds.x + o.bounds.w * 0.5, o.bounds.y + o.bounds.h * 0.5 };
                            if (o.anchorObjectId == hitId || hitObj->bounds.Contains(c.x, c.y, 4.0)) {
                                if (std::find(dragIds.begin(), dragIds.end(), o.id) == dragIds.end()) {
                                    dragIds.push_back(o.id);
                                }
                            }
                        }
                    }
                    st->lassoSelectedIds = (dragIds.size() > 1) ? dragIds : std::vector<std::uint64_t>{};
                }
                st->selectedObjectId = hitId;
                st->selectedIsCanvas = isCanvas;
                st->selectedPageIndex = pgIdx;
                if (IsDocumentLockedForEdit(st)) {
                    InvalidateViews(st);
                    return 0;
                }

                st->transformDrag = CanvasManip::ActiveTransformDrag{};
                st->transformDrag.active = true;
                st->transformDrag.handle = CanvasManip::HandleKind::Body;
                st->transformDrag.target = tgt;
                st->transformDrag.objectIds = dragIds;
                st->transformDrag.startWorld = { lx, ly };
                for (std::uint64_t id : dragIds) {
                    if (const auto* o = CanvasManip::FindObjectConst(st->doc, tgt, id)) {
                        st->transformDrag.beforeSnapshots.push_back(*o);
                    }
                }
                // Also capture any magnetic connectors attached to moved objects so Ctrl+Z restores them cleanly
                const auto& scopeObjs = tgt.isCanvas ? st->doc.CanvasObjects() : st->doc.Pages()[tgt.pageIndex].objects;
                for (const auto& o : scopeObjs) {
                    if (o.kind == ObjectKind::Shape && o.shapeKind == ShapeKind::Connector) {
                        const bool attached =
                            std::find(dragIds.begin(), dragIds.end(), o.connectorFromId) != dragIds.end() ||
                            std::find(dragIds.begin(), dragIds.end(), o.connectorToId) != dragIds.end();
                        if (attached && std::find(st->transformDrag.objectIds.begin(), st->transformDrag.objectIds.end(), o.id) == st->transformDrag.objectIds.end()) {
                            st->transformDrag.objectIds.push_back(o.id);
                            st->transformDrag.beforeSnapshots.push_back(o);
                        }
                    }
                }
                if (hitObj && CanvasManip::IsPlot3DObject(*hitObj) && (shiftHeld || dragIds.size() == 1)) {
                    // Drag inside center of 3D surface rotates yaw/pitch interactively; edge moves box
                    const bool nearEdge = (lx - hitObj->bounds.x < 22.0) || (hitObj->bounds.Right() - lx < 22.0) ||
                                          (ly - hitObj->bounds.y < 28.0) || (hitObj->bounds.Bottom() - ly < 22.0);
                    if (shiftHeld || !nearEdge) {
                        st->transformDrag.is3DRotateDrag = true;
                        try {
                            auto j = nlohmann::json::parse(hitObj->plotConfigUtf8);
                            st->transformDrag.startYawDeg = j.value("yawDeg", 42.0);
                            st->transformDrag.startPitchDeg = j.value("pitchDeg", 28.0);
                        } catch (...) {}
                    }
                }
                st->panMoved = false;
                st->panStartPt = { static_cast<LONG>(sp.x), static_cast<LONG>(sp.y) };
                SetCapture(hwnd);
                InvalidateViews(st);
                return 0;
            }

            // 6. Shift-drag on PDF page selects PDF text (F-075, F-076)
            if (!isCanvas && shiftHeld) {
                st->pdfSelectingText = true;
                st->pdfSelPage = pgIdx;
                st->pdfSelAnchorLocal = { lx, ly };
                st->pdfSelRectLocal = { lx, ly, 1.0, 1.0 };
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }

        if (st->activeTool == ToolKind::Hand || spacePan) {
            RECT vrc{};
            GetClientRect(hwnd, &vrc);
            st->panning = true;
            st->splitPanning = (st->splitViewActive && (st->splitViewVertical ? (sp.y > static_cast<double>(vrc.bottom / 2)) : (sp.x > static_cast<double>(vrc.right / 2))));
            st->panMoved = false;
            st->panStartPt = { static_cast<LONG>(sp.x), static_cast<LONG>(sp.y) };
            st->lastPanPt = st->panStartPt;
            SetCapture(hwnd);
            return 0;
        }

        if (st->activeTool == ToolKind::Laser) {
            st->drawing = true;
            st->laserPointer.active = true;
            st->laserPointer.worldX = wpWorld.x;
            st->laserPointer.worldY = wpWorld.y;
            st->laserPointer.trail.clear();
            st->laserPointer.trail.push_back(wpWorld);
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (!insidePage) return 0;
        if (IsDocumentLockedForEdit(st)) return 0;

        if (st->activeTool == ToolKind::Eraser) {
            const ObjectTarget erTarget{ isCanvas, pgIdx };
            const double erRad = 11.0 / (std::max)(0.4, st->camera.zoom);
            const auto rawHits = CollectHits(st, isCanvas, pgIdx, lx, ly, erRad);
            const auto hits = InkEngine::FilterHitsByEraserMode(st->doc, erTarget, rawHits, st->eraserFilterMode, true);
            if (!hits.empty()) {
                if (st->eraserActionMode == EraserActionMode::PartialPixel) {
                    bool anySplit = false;
                    for (std::uint64_t hid : hits) {
                        const auto* o = CanvasManip::FindObjectConst(st->doc, erTarget, hid);
                        if (!o) continue;
                        if (o->kind == ObjectKind::InkStroke) {
                            std::vector<AnnotationObject> splits;
                            if (InkEngine::EraseStrokePartial(*o, { lx, ly }, erRad, splits)) {
                                st->cmdStack.Execute(st->doc, std::make_unique<ReplaceStrokeWithSplitsCommand>(erTarget, *o, std::move(splits)));
                                anySplit = true;
                            }
                        } else {
                            st->cmdStack.Execute(st->doc, std::make_unique<RemoveObjectCommand>(erTarget, hid));
                            anySplit = true;
                        }
                    }
                    if (anySplit) {
                        st->statusBannerUtf8 = "Borrado parcial aplicado";
                        AppendWalMutation(st, "erase_partial");
                    }
                } else {
                    st->cmdStack.Execute(st->doc, std::make_unique<BatchEraseObjectsCommand>(erTarget, hits));
                    AppendWalMutation(st, "erase_whole");
                }
                UpdateWindowTitle(st);
                InvalidateViews(st);
            }
            st->drawing = true;
            st->draftIsCanvas = isCanvas;
            st->draftPageIdx = pgIdx;
            SetCapture(hwnd);
            return 0;
        }

        if (st->activeTool == ToolKind::TextBox) {
            std::wstring text = L"";
            if (ShowTextPrompt(st->hwndMain, L"Insertar Cuadro de Texto", L"Contenido:", text) && !text.empty()) {
                AnnotationObject tb;
                tb.kind = ObjectKind::TextBox;
                tb.layerId = st->doc.ActiveLayerId();
                tb.bounds = { lx, ly, 240.0, 80.0 };
                tb.textUtf8 = WideToUtf8(text);
                tb.strokeColor = st->penColor;
                tb.fontSizePt = 14.0;
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ isCanvas, pgIdx }, std::move(tb)));
                AppendWalMutation(st, "add_textbox");
                UpdateWindowTitle(st);
                InvalidateViews(st);
            }
            return 0;
        }

        if (st->snapToGrid && st->activeTool != ToolKind::Lasso && st->activeTool != ToolKind::Crop) {
            const PaperPattern pat = isCanvas ? st->doc.CanvasBackground() : st->doc.Pages()[pgIdx].background;
            const DocPoint sn = InkEngine::SnapPointToGrid({ lx, ly }, pat, 24.0);
            lx = sn.x; ly = sn.y;
        }

        st->drawing = true;
        st->draftIsCanvas = isCanvas;
        st->draftPageIdx = pgIdx;
        st->draftAnchorWorld = { lx, ly };
        st->hasDraftCrop = false;
        st->draftObj = AnnotationObject{};
        st->draftObj.layerId = st->doc.ActiveLayerId();
        st->draftObj.strokeColor = st->penColor;
        st->draftObj.strokeWidth = (st->activeTool == ToolKind::Highlighter) ? 14.0 : st->penWidth;
        st->draftObj.isHighlighter = (st->activeTool == ToolKind::Highlighter);
        st->draftObj.penStyle = st->penStyle;
        st->draftObj.opacity = st->penOpacity;

        if (st->activeTool == ToolKind::Pen || st->activeTool == ToolKind::Highlighter || st->activeTool == ToolKind::Lasso) {
            st->draftObj.kind = ObjectKind::InkStroke;
            st->hasPredictedTip = false;
            if (st->activeTool == ToolKind::Lasso) {
                st->draftObj.strokeColor = DocColor::RGBA(37, 99, 235, 220);
                st->draftObj.strokeWidth = 2.0;
            } else if (st->rulerState.visible) {
                const DocPoint snapped = InkEngine::ProjectPointOntoRuler({ lx, ly }, st->rulerState);
                lx = snapped.x;
                ly = snapped.y;
            }
            const std::uint64_t relMs = CurrentStrokeRelMs(st);
            st->draftObj.inkStartTimeMs = relMs;
            st->draftObj.inkEndTimeMs = relMs;
            st->draftObj.inkPoints.push_back({ lx, ly, pressure, tiltX, tiltY, relMs });
        } else if (st->activeTool == ToolKind::ShapeRect) {
            st->draftObj.kind = ObjectKind::Shape;
            st->draftObj.shapeKind = ShapeKind::Rectangle;
            st->draftObj.bounds = { lx, ly, 1.0, 1.0 };
        } else if (st->activeTool == ToolKind::ShapeArrow) {
            st->draftObj.kind = ObjectKind::Shape;
            st->draftObj.shapeKind = ShapeKind::Arrow;
            st->draftObj.shapeStart = { lx, ly };
            st->draftObj.shapeEnd = { lx, ly };
        } else if (st->activeTool == ToolKind::Crop) {
            st->hasDraftCrop = true;
            st->draftCropPageLocal = { lx, ly, 1.0, 1.0 };
        }

        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_POINTERUPDATE:
    case WM_MOUSEMOVE: {
        if (!st) break;
        if (msg == WM_MOUSEMOVE && st->draggingOpacitySlider && st->appearancePopoverOpen) {
            POINT ptMain{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            MapWindowPoints(hwnd, st->hwndMain, &ptMain, 1);
            UpdateAppearanceFromPoint(st, ptMain, true);
            return 0;
        }
        float pressure = 0.55f;
        float tiltX = 0.0f;
        float tiltY = 0.0f;
        DocPoint sp{};
        if (msg == WM_POINTERUPDATE) {
            const UINT32 pid = GET_POINTERID_WPARAM(wp);
            POINTER_INFO pi{};
            if (GetPointerInfo(pid, &pi)) {
                if (pi.pointerType == PT_TOUCH && (GetTickCount64() - st->lastPenInputTickMs) < 1200ull) {
                    return 0;
                }
                POINT pt = pi.ptPixelLocation;
                ScreenToClient(hwnd, &pt);
                sp = { static_cast<double>(pt.x), static_cast<double>(pt.y) };
                POINTER_PEN_INFO pen{};
                if (pi.pointerType == PT_PEN && GetPointerPenInfo(pid, &pen)) {
                    st->lastPenInputTickMs = GetTickCount64();
                    pressure = std::clamp(static_cast<float>(pen.pressure) / 1024.0f, 0.1f, 1.0f);
                    tiltX = static_cast<float>(pen.tiltX);
                    tiltY = static_cast<float>(pen.tiltY);
                }
            }
        } else {
            extractPointerWorld(lp, pressure, sp);
        }

        if (st->uiPrefs.accessibility.readingRulerEnabled && std::abs(st->readingRulerCursorY - sp.y) >= 1.5) {
            st->readingRulerCursorY = sp.y;
            if (!st->drawing && !st->panning && !st->transformDrag.active) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
        }

        if (st->transformDrag.active) {
            if (std::hypot(sp.x - st->panStartPt.x, sp.y - st->panStartPt.y) > 3.0) st->panMoved = true;
        const DocPoint wpWorld = st->camera.ScreenToWorld(sp.x, sp.y);
            double lx = wpWorld.x, ly = wpWorld.y;
            if (!st->transformDrag.target.isCanvas) {
                if (const auto* pl = st->doc.Layout().PlacementAt(st->transformDrag.target.pageIndex)) {
                    const DocRect wr = pl->WorldRect();
                    lx -= wr.x; ly -= wr.y;
                }
            }
            const double dx = lx - st->transformDrag.startWorld.x;
            const double dy = ly - st->transformDrag.startWorld.y;

            if (st->transformDrag.is3DRotateDrag && !st->transformDrag.objectIds.empty()) {
                if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->transformDrag.objectIds[0])) {
                    CanvasManip::UpdatePlot3DRotation(*obj, st->transformDrag.startYawDeg + dx * 0.75, st->transformDrag.startPitchDeg - dy * 0.55);
                    st->doc.BumpRevision();
                }
            } else if (st->transformDrag.handle == CanvasManip::HandleKind::Body) {
                for (std::size_t i = 0; i < st->transformDrag.objectIds.size() && i < st->transformDrag.beforeSnapshots.size(); ++i) {
                    const auto& snap = st->transformDrag.beforeSnapshots[i];
                    if (snap.kind == ObjectKind::Shape && snap.shapeKind == ShapeKind::Connector) continue;
                    if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->transformDrag.objectIds[i])) {
                        *obj = snap;
                        CanvasManip::TranslateAnnotationObject(*obj, dx, dy);
                        InkEngine::UpdateMagneticConnectors(st->doc, st->transformDrag.target, obj->id);
                    }
                }
                st->doc.BumpRevision();
            } else if (st->transformDrag.handle == CanvasManip::HandleKind::Rotate && !st->transformDrag.objectIds.empty()) {
                if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->transformDrag.objectIds[0])) {
                    const double cx = st->transformDrag.initialBounds.x + st->transformDrag.initialBounds.w * 0.5;
                    const double cy = st->transformDrag.initialBounds.y + st->transformDrag.initialBounds.h * 0.5;
                    const double ang = std::atan2(ly - cy, lx - cx);
                    const double deltaDeg = (ang - st->transformDrag.initialAngleRad) * (180.0 / 3.14159265358979323846);
                    obj->rotationDeg = std::fmod(st->transformDrag.initialRotDeg + deltaDeg + 3600.0, 360.0);
                    st->doc.BumpRevision();
                }
            } else if (!st->transformDrag.objectIds.empty() && !st->transformDrag.beforeSnapshots.empty()) {
                if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->transformDrag.objectIds[0])) {
                    const DocRect nb = CanvasManip::ComputeResizedBounds(st->transformDrag.handle, st->transformDrag.initialBounds, dx, dy);
                    CanvasManip::ResizeAnnotationObject(*obj, st->transformDrag.beforeSnapshots[0], nb);
                    InkEngine::UpdateMagneticConnectors(st->doc, st->transformDrag.target, obj->id);
                    st->doc.BumpRevision();
                }
            }
            InvalidateViews(st);
            return 0;
        }

        if (st->pdfSelectingText) {
        const DocPoint wpWorld = st->camera.ScreenToWorld(sp.x, sp.y);
            if (const auto* pl = st->doc.Layout().PlacementAt(st->pdfSelPage)) {
                const DocRect wr = pl->WorldRect();
                const double lx = wpWorld.x - wr.x;
                const double ly = wpWorld.y - wr.y;
                st->pdfSelRectLocal = DocRect::FromMinMax(
                    (std::min)(st->pdfSelAnchorLocal.x, lx), (std::min)(st->pdfSelAnchorLocal.y, ly),
                    (std::max)(st->pdfSelAnchorLocal.x, lx), (std::max)(st->pdfSelAnchorLocal.y, ly));
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }

        if (st->panning) {
            if (std::hypot(sp.x - st->panStartPt.x, sp.y - st->panStartPt.y) > 3.5) {
                st->panMoved = true;
            }
            if (std::hypot(sp.x - st->rbuttonDownPt.x, sp.y - st->rbuttonDownPt.y) > 4.0) {
                st->rbuttonMoved = true;
            }
            const double dx = sp.x - st->lastPanPt.x;
            const double dy = sp.y - st->lastPanPt.y;
            Camera2D& panCam = st->splitPanning ? st->splitCamera : st->camera;
            panCam.panX -= dx / panCam.zoom;
            panCam.panY -= dy / panCam.zoom;
            st->lastPanPt = { static_cast<LONG>(sp.x), static_cast<LONG>(sp.y) };
            if (!st->splitPanning) RefreshActivePageFromCamera(st);
            InvalidateViews(st);
            return 0;
        }

        if (!st->drawing) {
            // Hover Link Preview Card (OB-LINK-045) over Graph nodes or TextBoxes containing [[wikilinks]]
            bool foundHover = false;
            std::string hTitle, hExcerpt;
            if (st->graphOverlayVisible) {
                RECT vpRc{};
                GetClientRect(hwnd, &vpRc);
                const DocRect gr = OverlayGeometry::GraphPanelRect(
                    static_cast<int>(vpRc.right - vpRc.left), static_cast<int>(vpRc.bottom - vpRc.top));
                if (gr.Contains(sp.x, sp.y)) {
                    const double padX = 36.0, padTop = 44.0, padBot = 28.0;
                    const double sx = (gr.w - padX * 2.0) / 800.0;
                    const double sy = (gr.h - padTop - padBot) / 560.0;
                    for (const auto& nd : st->activeGraphView.nodes) {
                        const double nx = gr.x + padX + nd.x * sx;
                        const double ny = gr.y + padTop + nd.y * sy;
                        if (std::hypot(sp.x - nx, sp.y - ny) <= 13.0) {
                            const auto prev = st->knowledgeVault.ResolveLinkPreview(nd.labelUtf8);
                            if (prev.found) {
                                foundHover = true;
                                hTitle = prev.targetTitleUtf8;
                                hExcerpt = prev.previewExcerptUtf8;
                            }
                            break;
                        }
                    }
                }
            } else if (!st->dbPanelVisible && !st->readingViewMode && st->knowledgeVault.TotalNoteCount(false) > 0) {
                const DocPoint wpHover = st->camera.ScreenToWorld(sp.x, sp.y);
                const bool isCanvas = (st->doc.Mode() == DocumentMode::InfiniteCanvas);
                std::size_t hPg = st->activePageIndex;
                double hlx = wpHover.x, hly = wpHover.y;
                const bool onPg = isCanvas || st->doc.Layout().HitTestWorldPoint(wpHover.x, wpHover.y, hPg, hlx, hly);
                if (onPg) {
                    const auto hits = CollectHits(st, isCanvas, hPg, hlx, hly, 6.0);
                    if (!hits.empty()) {
                        if (const auto* ho = CanvasManip::FindObjectConst(st->doc, ObjectTarget{ isCanvas, hPg }, hits.back())) {
                            if (ho->kind == ObjectKind::TextBox) {
                                const auto p0 = ho->textUtf8.find("[[");
                                const auto p1 = (p0 != std::string::npos) ? ho->textUtf8.find("]]", p0 + 2) : std::string::npos;
                                if (p0 != std::string::npos && p1 != std::string::npos && p1 > p0 + 2) {
                                    const std::string targetLink = ho->textUtf8.substr(p0 + 2, p1 - (p0 + 2));
                                    const auto prev = st->knowledgeVault.ResolveLinkPreview(targetLink);
                                    if (prev.found) {
                                        foundHover = true;
                                        hTitle = prev.targetTitleUtf8;
                                        hExcerpt = prev.previewExcerptUtf8;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            if (foundHover != st->hasHoverPreview || (foundHover && (hTitle != st->hoverPreviewTitleUtf8 ||
                    std::hypot(sp.x - st->hoverPreviewScreenPt.x, sp.y - st->hoverPreviewScreenPt.y) > 12.0))) {
                st->hasHoverPreview = foundHover;
                st->hoverPreviewScreenPt = sp;
                st->hoverPreviewTitleUtf8 = std::move(hTitle);
                st->hoverPreviewExcerptUtf8 = std::move(hExcerpt);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }

        if (st->drawingInZoomBox && st->zoomWritingBox.active) {
            RECT vpRc{};
            GetClientRect(hwnd, &vpRc);
            const DocRect dock = OverlayGeometry::ZoomBoxDockRect(
                static_cast<int>(vpRc.right - vpRc.left), static_cast<int>(vpRc.bottom - vpRc.top));
            const double waL = dock.x + 12.0, waT = dock.y + 26.0;
            const double waW = (std::max)(1.0, dock.w - 24.0), waH = (std::max)(1.0, dock.h - 36.0);
            const double rx = std::clamp((sp.x - waL) / waW, 0.0, 1.0);
            const double ry = std::clamp((sp.y - waT) / waH, 0.0, 1.0);
            const double plx = st->zoomWritingBox.pageTargetRect.x + rx * st->zoomWritingBox.pageTargetRect.w;
            const double ply = st->zoomWritingBox.pageTargetRect.y + ry * st->zoomWritingBox.pageTargetRect.h;
            if (st->draftObj.inkPoints.empty() ||
                std::hypot(plx - st->draftObj.inkPoints.back().x, ply - st->draftObj.inkPoints.back().y) >= 0.6) {
                const std::uint64_t relMs = CurrentStrokeRelMs(st);
                st->draftObj.inkEndTimeMs = relMs;
                st->draftObj.inkPoints.push_back({ plx, ply, pressure, tiltX, tiltY, relMs });
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        const DocPoint wpWorld = st->camera.ScreenToWorld(sp.x, sp.y);
        if (st->activeTool == ToolKind::Laser) {
            st->laserPointer.active = true;
            st->laserPointer.worldX = wpWorld.x;
            st->laserPointer.worldY = wpWorld.y;
            st->laserPointer.trail.push_back(wpWorld);
            if (st->laserPointer.trail.size() > 28) {
                st->laserPointer.trail.erase(st->laserPointer.trail.begin());
            }
            InvalidateViews(st);
            return 0;
        }

        double lx = wpWorld.x, ly = wpWorld.y;
        if (!st->draftIsCanvas) {
            if (const auto* pl = st->doc.Layout().PlacementAt(st->draftPageIdx)) {
                const DocRect wr = pl->WorldRect();
                lx = wpWorld.x - wr.x;
                ly = wpWorld.y - wr.y;
            }
        }

        // Continuous erasing on drag (both WholeStroke & PartialPixel modes)
        if (st->activeTool == ToolKind::Eraser) {
            const ObjectTarget erTarget{ st->draftIsCanvas, st->draftPageIdx };
            const double erRad = 11.0 / (std::max)(0.4, st->camera.zoom);
            const auto rawHits = CollectHits(st, st->draftIsCanvas, st->draftPageIdx, lx, ly, erRad);
            const auto hits = InkEngine::FilterHitsByEraserMode(st->doc, erTarget, rawHits, st->eraserFilterMode, true);
            if (!hits.empty()) {
                if (st->eraserActionMode == EraserActionMode::PartialPixel) {
                    for (std::uint64_t hid : hits) {
                        const auto* o = CanvasManip::FindObjectConst(st->doc, erTarget, hid);
                        if (!o) continue;
                        if (o->kind == ObjectKind::InkStroke) {
                            std::vector<AnnotationObject> splits;
                            if (InkEngine::EraseStrokePartial(*o, { lx, ly }, erRad, splits)) {
                                st->cmdStack.Execute(st->doc, std::make_unique<ReplaceStrokeWithSplitsCommand>(erTarget, *o, std::move(splits)));
                            }
                        } else {
                            st->cmdStack.Execute(st->doc, std::make_unique<RemoveObjectCommand>(erTarget, hid));
                        }
                    }
                } else {
                    st->cmdStack.Execute(st->doc, std::make_unique<BatchEraseObjectsCommand>(erTarget, hits));
                }
                InvalidateViews(st);
            }
            return 0;
        }

        if (st->draftObj.kind == ObjectKind::InkStroke) {
            if (st->hasPredictedTip && st->draftObj.inkPoints.size() >= 2) {
                st->draftObj.inkPoints.pop_back();
                st->hasPredictedTip = false;
            }

            auto appendLocalSample = [&](double sxLocal, double syLocal, float pr, float tx, float ty) {
                if (st->activeTool != ToolKind::Lasso) {
                    if (st->rulerState.visible) {
                        const DocPoint snapped = InkEngine::ProjectPointOntoRuler({ sxLocal, syLocal }, st->rulerState);
                        sxLocal = snapped.x;
                        syLocal = snapped.y;
                    } else if (st->snapToGrid) {
                        const PaperPattern pat = st->draftIsCanvas ? st->doc.CanvasBackground() : st->doc.Pages()[st->draftPageIdx].background;
                        const DocPoint sn = InkEngine::SnapPointToGrid({ sxLocal, syLocal }, pat, 24.0);
                        sxLocal = sn.x;
                        syLocal = sn.y;
                    }
                }
                if (!st->draftObj.inkPoints.empty()) {
                    const auto& prev = st->draftObj.inkPoints.back();
                    if (std::hypot(sxLocal - prev.x, syLocal - prev.y) >= 1.1) {
                        const std::uint64_t relMs = CurrentStrokeRelMs(st);
                        st->draftObj.inkEndTimeMs = relMs;
                        st->draftObj.inkPoints.push_back({ sxLocal, syLocal, pr, tx, ty, relMs });
                    }
                }
            };

            bool ingestedCoalesced = false;
            if (msg == WM_POINTERUPDATE) {
                const UINT32 pid = GET_POINTERID_WPARAM(wp);
                POINTER_INFO pi{};
                if (GetPointerInfo(pid, &pi) && pi.historyCount > 1) {
                    UINT32 count = (std::min<UINT32>)(pi.historyCount, 64u);
                    if (pi.pointerType == PT_PEN) {
                        std::vector<POINTER_PEN_INFO> penHist(count);
                        if (GetPointerPenInfoHistory(pid, &count, penHist.data()) && count > 0) {
                            for (int hi = static_cast<int>(count) - 1; hi >= 0; --hi) {
                                POINT hpt = penHist[static_cast<std::size_t>(hi)].pointerInfo.ptPixelLocation;
                                ScreenToClient(hwnd, &hpt);
                                const DocPoint hw = st->camera.ScreenToWorld(hpt.x, hpt.y);
                                double hlx = hw.x, hly = hw.y;
                                if (!st->draftIsCanvas) {
                                    if (const auto* pl = st->doc.Layout().PlacementAt(st->draftPageIdx)) {
                                        const DocRect wr = pl->WorldRect();
                                        hlx -= wr.x; hly -= wr.y;
                                    }
                                }
                                const float hPr = std::clamp(static_cast<float>(penHist[static_cast<std::size_t>(hi)].pressure) / 1024.0f, 0.1f, 1.0f);
                                appendLocalSample(hlx, hly, hPr,
                                    static_cast<float>(penHist[static_cast<std::size_t>(hi)].tiltX),
                                    static_cast<float>(penHist[static_cast<std::size_t>(hi)].tiltY));
                            }
                            ingestedCoalesced = true;
                        }
                    } else {
                        std::vector<POINTER_INFO> ptHist(count);
                        if (GetPointerInfoHistory(pid, &count, ptHist.data()) && count > 0) {
                            for (int hi = static_cast<int>(count) - 1; hi >= 0; --hi) {
                                POINT hpt = ptHist[static_cast<std::size_t>(hi)].ptPixelLocation;
                                ScreenToClient(hwnd, &hpt);
                                const DocPoint hw = st->camera.ScreenToWorld(hpt.x, hpt.y);
                                double hlx = hw.x, hly = hw.y;
                                if (!st->draftIsCanvas) {
                                    if (const auto* pl = st->doc.Layout().PlacementAt(st->draftPageIdx)) {
                                        const DocRect wr = pl->WorldRect();
                                        hlx -= wr.x; hly -= wr.y;
                                    }
                                }
                                appendLocalSample(hlx, hly, pressure, 0.0f, 0.0f);
                            }
                            ingestedCoalesced = true;
                        }
                    }
                }
            }
            if (!ingestedCoalesced) {
                appendLocalSample(lx, ly, pressure, tiltX, tiltY);
            }

            if (st->activeTool != ToolKind::Lasso && !st->rulerState.visible && !st->snapToGrid && st->draftObj.inkPoints.size() >= 3) {
                InkPoint pred{};
                if (InkEngine::PredictNextInkPoint(st->draftObj.inkPoints, pred, 12.0) &&
                    std::hypot(pred.x - st->draftObj.inkPoints.back().x, pred.y - st->draftObj.inkPoints.back().y) >= 0.4) {
                    st->draftObj.inkPoints.push_back(pred);
                    st->hasPredictedTip = true;
                }
            }
        } else if (st->hasDraftCrop) {
            st->draftCropPageLocal = DocRect::FromMinMax(
                (std::min)(st->draftAnchorWorld.x, lx), (std::min)(st->draftAnchorWorld.y, ly),
                (std::max)(st->draftAnchorWorld.x, lx), (std::max)(st->draftAnchorWorld.y, ly));
        } else if (st->draftObj.shapeKind == ShapeKind::Rectangle) {
            st->draftObj.bounds = DocRect::FromMinMax(
                (std::min)(st->draftAnchorWorld.x, lx), (std::min)(st->draftAnchorWorld.y, ly),
                (std::max)(st->draftAnchorWorld.x, lx), (std::max)(st->draftAnchorWorld.y, ly));
        } else if (st->draftObj.shapeKind == ShapeKind::Arrow) {
            st->draftObj.shapeEnd = { lx, ly };
        }
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_POINTERUP:
    case WM_LBUTTONUP: {
        if (!st) break;
        auto restoreStylusTempTool = [&]() {
            if (st->invertedTempEraser || st->barrelTempLasso) {
                st->activeTool = st->savedToolBeforeStylusMod;
                st->invertedTempEraser = false;
                st->barrelTempLasso = false;
            }
        };
        if (msg == WM_LBUTTONUP && st->draggingOpacitySlider) {
            st->draggingOpacitySlider = false;
            ReleaseCapture();
            return 0;
        }
        if (st->transformDrag.active) {
            st->transformDrag.active = false;
            ReleaseCapture();
            if (st->panMoved && !st->transformDrag.beforeSnapshots.empty()) {
                // Commit undoable batch transform snapshot!
                std::vector<AnnotationObject> afterSnaps;
                for (std::uint64_t id : st->transformDrag.objectIds) {
                    if (const auto* cur = CanvasManip::FindObjectConst(st->doc, st->transformDrag.target, id)) {
                        afterSnaps.push_back(*cur);
                    }
                }
                if (afterSnaps.size() == st->transformDrag.beforeSnapshots.size()) {
                    // Restore pre-drag state cleanly so CommandStack::Execute records proper before->after
                    for (std::size_t i = 0; i < st->transformDrag.objectIds.size(); ++i) {
                        if (AnnotationObject* m = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->transformDrag.objectIds[i])) {
                            *m = st->transformDrag.beforeSnapshots[i];
                        }
                    }
                    st->cmdStack.Execute(st->doc, std::make_unique<BatchModifyObjectsCommand>(
                        st->transformDrag.target,
                        std::move(st->transformDrag.beforeSnapshots),
                        std::move(afterSnaps),
                        st->transformDrag.is3DRotateDrag ? "Rotar 3D" : "Transformar objeto"));
                    AppendWalMutation(st, "transform_obj");
                }
            } else if (!st->panMoved && st->selectedObjectId != 0) {
                if (auto* selObj = CanvasManip::FindObjectMut(st->doc, st->transformDrag.target, st->selectedObjectId)) {
                    const double clickLx = st->transformDrag.startWorld.x;
                    const double clickLy = st->transformDrag.startWorld.y;
                    if (selObj->kind == ObjectKind::VideoClip) {
                        const double barTop = selObj->bounds.Bottom() - (std::min)(28.0, selObj->bounds.h * 0.25);
                        const double trackPadL = 11.0 / (std::max)(0.25, st->camera.zoom);
                        const double trackPadR = 93.0 / (std::max)(0.25, st->camera.zoom);
                        const double trackX0 = selObj->bounds.x + trackPadL;
                        const double trackX1 = (std::max)(trackX0 + 8.0, selObj->bounds.Right() - trackPadR);
                        const std::uint64_t durMs = (st->videoPlayer.ActiveObjectId() == selObj->id && st->videoPlayer.DurationMs() > 0)
                            ? st->videoPlayer.DurationMs() : selObj->videoDurationMs;
                        if (clickLy >= barTop && durMs > 0) {
                            const double frac = std::clamp((clickLx - trackX0) / (trackX1 - trackX0), 0.0, 1.0);
                            const std::uint64_t seekMs = static_cast<std::uint64_t>(frac * static_cast<double>(durMs));
                            selObj->videoPositionMs = seekMs;
                            if (st->videoPlayer.ActiveObjectId() == selObj->id) {
                                st->videoPlayer.SeekMs(seekMs);
                            }
                        } else if ((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                            ExecuteToolbarCommand(st, CMD_MEDIA_PLAY_PAUSE);
                        }
                    } else if (selObj->kind == ObjectKind::HtmlEmbed && st->activeHtmlObjectId == selObj->id) {
                        const double hdrBot = selObj->bounds.y + (std::min)(28.0, selObj->bounds.h * 0.24);
                        if (clickLy <= hdrBot) {
                            ExecuteToolbarCommand(st, CMD_HTML_FREEZE_RESUME);
                        }
                    } else if (selObj->kind == ObjectKind::LinkCard && ((GetKeyState(VK_CONTROL) & 0x8000) != 0 || st->readingViewMode)) {
                        if (selObj->linkTargetPageIndex >= 0 && static_cast<std::size_t>(selObj->linkTargetPageIndex) < st->doc.Pages().size()) {
                            st->doc.SetMode(DocumentMode::PaginatedPdf);
                            CenterCameraOnPage(st, static_cast<std::size_t>(selObj->linkTargetPageIndex));
                            LayoutMainChildren(st);
                        } else if (IsSafeExternalLinkUrl(selObj->linkUrlUtf8)) {
                            ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(selObj->linkUrlUtf8).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                            st->statusBannerUtf8 = "Enlace abierto: " + ExtractUrlDomainUtf8(selObj->linkUrlUtf8);
                        }
                    } else if (selObj->kind == ObjectKind::TextBox && ((GetKeyState(VK_CONTROL) & 0x8000) != 0 || st->readingViewMode)) {
                        InlineUrlSpan hitUrl{};
                        if (st->renderer.HitTestTextBoxInlineUrl(*selObj, clickLx, clickLy, st->camera.zoom,
                                st->uiPrefs.accessibility.dyslexiaFriendlyMode, hitUrl) &&
                            IsSafeExternalLinkUrl(hitUrl.normalizedUrlUtf8)) {
                            ShellExecuteW(st->hwndMain, L"open", Utf8ToWide(hitUrl.normalizedUrlUtf8).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                            st->statusBannerUtf8 = "Enlace abierto desde texto: " + ExtractUrlDomainUtf8(hitUrl.normalizedUrlUtf8);
                        }
                    } else {
                        AppActions::ToggleStudyTapeIfHit(
                            st->doc, st->cmdStack, st->transformDrag.target,
                            st->selectedObjectId, st->statusBannerUtf8);
                    }
                }
            }
            UpdateWindowTitle(st);
            InvalidateViews(st);
            restoreStylusTempTool();
            return 0;
        }
        if (st->pdfSelectingText) {
            st->pdfSelectingText = false;
            ReleaseCapture();
            if (st->pdfSelPage < st->doc.Pages().size() && !st->pdfSelRectLocal.Empty()) {
                const auto& pg = st->doc.Pages()[st->pdfSelPage];
                std::vector<DocRect> quads;
                const std::string extracted = CanvasManip::ExtractPdfTextInRect(
                    pg, st->pdfSelRectLocal, pg.EffectiveWidthPt(), pg.EffectiveHeightPt(), &quads);
                if (!extracted.empty()) {
                    st->lastSelectedPdfTextUtf8 = extracted;
                    CanvasManip::CopyTextToClipboardWin32(st->hwndMain, extracted);
                    // Also record semantic QuadPoints highlight when selection covers >= 12pt
                    if (st->pdfSelRectLocal.w >= 12.0 && !quads.empty()) {
                        AnnotationObject hl{};
                        hl.kind = ObjectKind::Shape;
                        hl.shapeKind = ShapeKind::Rectangle;
                        hl.layerId = st->doc.ActiveLayerId();
                        hl.bounds = st->pdfSelRectLocal;
                        hl.fillColor = DocColor::RGBA(253, 224, 71, 95);
                        hl.strokeColor = DocColor::RGBA(234, 179, 8, 120);
                        hl.strokeWidth = 0.8;
                        hl.isHighlighter = true;
                        hl.textUtf8 = extracted;
                        hl.pdfQuadPointsNorm = quads;
                        st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(ObjectTarget{ false, st->pdfSelPage }, std::move(hl)));
                    }
                    st->statusBannerUtf8 = "Texto PDF copiado y resaltado (QuadPoints): \"" + extracted.substr(0, 48) + "\"";
                }
            }
            st->pdfSelRectLocal = {};
            InvalidateViews(st);
            restoreStylusTempTool();
            return 0;
        }
        if (st->panning) {
            st->panning = false;
            st->splitPanning = false;
            ReleaseCapture();
            if (!st->panMoved && st->activeTool == ToolKind::Hand) {
                st->selectedObjectId = 0;
                st->lassoSelectedIds.clear();
                UpdateWindowTitle(st);
                InvalidateViews(st);
            }
            restoreStylusTempTool();
            return 0;
        }
        if (!st->drawing) { restoreStylusTempTool(); return 0; }
        st->drawing = false;
        const bool wasZoomBoxStroke = st->drawingInZoomBox;
        st->drawingInZoomBox = false;
        ReleaseCapture();

        if (st->activeTool == ToolKind::Eraser) {
            UpdateWindowTitle(st);
            InvalidateViews(st);
            restoreStylusTempTool();
            return 0;
        }

        if (st->activeTool == ToolKind::Laser) {
            st->laserPointer.active = false;
            InvalidateViews(st);
            restoreStylusTempTool();
            return 0;
        }

        if (st->hasDraftCrop) {
            const bool shiftHeld = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            if (!st->draftIsCanvas && shiftHeld && st->draftPageIdx < st->doc.Pages().size()) {
                // Shift + Crop on paginated page: legacy full-page margin crop
                const auto& pg = st->doc.Pages()[st->draftPageIdx];
                const double pw = (std::max)(1.0, pg.EffectiveWidthPt());
                const double ph = (std::max)(1.0, pg.EffectiveHeightPt());
                if (st->draftCropPageLocal.w >= 24.0 && st->draftCropPageLocal.h >= 24.0) {
                    NormalizedCropRect nc{
                        pg.crop.left + (st->draftCropPageLocal.x / pw) * (pg.crop.right - pg.crop.left),
                        pg.crop.top + (st->draftCropPageLocal.y / ph) * (pg.crop.bottom - pg.crop.top),
                        pg.crop.left + (st->draftCropPageLocal.Right() / pw) * (pg.crop.right - pg.crop.left),
                        pg.crop.top + (st->draftCropPageLocal.Bottom() / ph) * (pg.crop.bottom - pg.crop.top)
                    };
                    nc.Clamp();
                    st->cmdStack.Execute(st->doc, std::make_unique<CropPageCommand>(st->draftPageIdx, nc));
                }
            } else if (st->draftCropPageLocal.w >= 14.0 && st->draftCropPageLocal.h >= 14.0) {
                double ox = 0.0, oy = 0.0;
                if (!st->draftIsCanvas) {
                    if (const auto* pl = st->doc.Layout().PlacementAt(st->draftPageIdx)) {
                        ox = pl->WorldRect().x;
                        oy = pl->WorldRect().y;
                    }
                }
                const DocRect worldRect{
                    ox + st->draftCropPageLocal.x,
                    oy + st->draftCropPageLocal.y,
                    st->draftCropPageLocal.w,
                    st->draftCropPageLocal.h
                };
                st->lastSnipRectWorld = worldRect;
                st->lastSnipPolyWorld.clear();
                st->hasLastSnipRegion = true;
                SnipWorldRegionAsImage(st, worldRect, nullptr);
            }
            st->hasDraftCrop = false;
        } else if (st->draftObj.kind == ObjectKind::InkStroke) {
            if (st->hasPredictedTip && st->draftObj.inkPoints.size() >= 2) {
                st->draftObj.inkPoints.pop_back();
                st->hasPredictedTip = false;
            }
            const bool isLasso = (st->activeTool == ToolKind::Lasso);
            // Flexcil Live Gesture Mode interception (L-capture / Box-capture / Underline / Bracket)
            if (!wasZoomBoxStroke && !isLasso && !st->draftIsCanvas &&
                st->flexcilOverlay.IsGestureModeActive() && st->draftObj.inkPoints.size() >= 4) {
                FlexcilGestureResult gres{};
                std::uint64_t noteObjId = 0, linkId = 0;
                if (st->flexcilOverlay.ApplyGestureStrokeOnDocument(
                        st->doc, st->activePdfSourcePathUtf8, st->draftPageIdx,
                        st->draftObj.inkPoints, &gres, &noteObjId, &linkId)) {
                    st->flexcilOverlay.SetPopupNoteVisible(true);
                    st->statusBannerUtf8 = "Gesto Flexcil capturado y citado en Nota de estudio (p\xC3\xA1g. " +
                        std::to_string(st->draftPageIdx + 1) + ")";
                    restoreStylusTempTool();
                    UpdateWindowTitle(st);
                    InvalidateViews(st);
                    return 0;
                }
            }
            std::vector<DocPoint> lassoPolyWorld;
            DocRect lassoWorldRect{};
            if (isLasso && st->draftObj.inkPoints.size() >= 3) {
                double ox = 0.0, oy = 0.0;
                if (!st->draftIsCanvas) {
                    if (const auto* pl = st->doc.Layout().PlacementAt(st->draftPageIdx)) {
                        ox = pl->WorldRect().x;
                        oy = pl->WorldRect().y;
                    }
                }
                double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
                lassoPolyWorld.reserve(st->draftObj.inkPoints.size());
                for (const auto& pt : st->draftObj.inkPoints) {
                    const double wx = ox + pt.x;
                    const double wy = oy + pt.y;
                    lassoPolyWorld.push_back({ wx, wy });
                    minX = (std::min)(minX, wx);
                    minY = (std::min)(minY, wy);
                    maxX = (std::max)(maxX, wx);
                    maxY = (std::max)(maxY, wy);
                }
                lassoWorldRect = DocRect::FromMinMax(minX, minY, maxX, maxY);
            }
            AppActions::HandleStrokeCompletedAction(
                st->doc,
                st->cmdStack,
                ObjectTarget{ st->draftIsCanvas, st->draftPageIdx },
                st->draftObj,
                isLasso,
                st->snapShapes,
                st->penStabilization,
                st->lassoSelectedIds,
                st->selectedObjectId,
                st->statusBannerUtf8);
            st->selectedIsCanvas = st->draftIsCanvas;
            st->selectedPageIndex = st->draftPageIdx;
            if (wasZoomBoxStroke && st->zoomWritingBox.active && !st->draftObj.inkPoints.empty()) {
                if (SmartScriptReflowEngine::UpdateZoomWritingBoxOnStroke(st->zoomWritingBox, st->draftObj.inkPoints)) {
                    st->statusBannerUtf8 = "Caja de Zoom: auto-avance aplicado (avances=" +
                        std::to_string(st->zoomWritingBox.autoAdvanceCount) + ", saltos=" +
                        std::to_string(st->zoomWritingBox.lineWrapCount) + ")";
                }
            }
            if (isLasso && lassoWorldRect.w >= 14.0 && lassoWorldRect.h >= 14.0) {
                st->lastSnipRectWorld = lassoWorldRect;
                st->lastSnipPolyWorld = std::move(lassoPolyWorld);
                st->hasLastSnipRegion = true;
                // When user circles part of a printed document where no separate unlocked objects were enclosed
                // (or holds Shift while lassoing), automatically copy the circular/freehand cutout as an image!
                if (st->lassoSelectedIds.empty() || ((GetKeyState(VK_SHIFT) & 0x8000) != 0)) {
                    SnipWorldRegionAsImage(st, st->lastSnipRectWorld, &st->lastSnipPolyWorld);
                }
            }
            AppendWalMutation(st, "add_ink");
        } else {
            st->draftObj.RecomputeBounds();
            if (!st->draftObj.bounds.Empty()) {
                st->cmdStack.Execute(st->doc, std::make_unique<AddObjectCommand>(
                    ObjectTarget{ st->draftIsCanvas, st->draftPageIdx }, std::move(st->draftObj)));
                st->selectedIsCanvas = st->draftIsCanvas;
                st->selectedPageIndex = st->draftPageIdx;
                st->selectedObjectId = AppActions::LastInsertedId(st->doc, ObjectTarget{ st->draftIsCanvas, st->draftPageIdx });
                AppendWalMutation(st, "add_shape");
            }
        }
        restoreStylusTempTool();
        UpdateWindowTitle(st);
        InvalidateViews(st);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------- Flat Zero-GPU guías de interfaz Appearance & Performance Popover ----------------

void UpdateAppearanceFromPoint(StudioAppState* st, POINT pt, bool isDrag) {
    if (!st || !st->appearancePopoverOpen || isDrag) return;
    const UINT dpi = st->currentDpi;
    auto S = [&](int px) { return DpiSc(px, dpi); };
    const RECT& pr = st->appearancePopoverRc;

    // 1. Theme segmented buttons: Claro | Oscuro | Contraste
    const int segY0 = pr.top + S(44);
    const int segY1 = pr.top + S(74);
    if (pt.y >= segY0 && pt.y <= segY1 && pt.x >= pr.left + S(16) && pt.x <= pr.right - S(16)) {
        const int segW = (pr.right - pr.left - S(32)) / 3;
        const int idx = std::clamp(static_cast<int>((pt.x - (pr.left + S(16))) / (std::max)(1, segW)), 0, 2);
        st->glassSettings.mode = static_cast<UiGlass::AppearanceMode>(idx);
        const bool isHcMode = (st->glassSettings.mode == UiGlass::AppearanceMode::HighContrast);
        st->glassSettings.increaseContrast = isHcMode;
        st->glassSettings.reduceTransparency = isHcMode;
        UiGlass::ApplyWindowsNativeMicaBackdrop(st->hwndMain, st->glassSettings.IsDark(), st->glassSettings.reduceTransparency);
        UiGlass::SaveGlassSettings(st->glassSettings);
        InvalidateViews(st);
        return;
    }

    if (pt.x >= pr.left + S(16) && pt.x <= pr.right - S(16)) {
        // 2. Zero-GPU software render toggle row
        if (pt.y >= pr.top + S(88) && pt.y <= pr.top + S(114)) {
            ExecuteToolbarCommand(st, CMD_TOGGLE_GPU_ACCEL);
            return;
        }
        // 3. Reduce Transparency (Windows Mica toggle)
        if (pt.y >= pr.top + S(120) && pt.y <= pr.top + S(146)) {
            st->glassSettings.reduceTransparency = !st->glassSettings.reduceTransparency;
            UiGlass::ApplyWindowsNativeMicaBackdrop(st->hwndMain, st->glassSettings.IsDark(), st->glassSettings.reduceTransparency);
            UiGlass::SaveGlassSettings(st->glassSettings);
            InvalidateViews(st);
            return;
        }
        // 4. Increase Contrast
        if (pt.y >= pr.top + S(152) && pt.y <= pr.top + S(178)) {
            st->glassSettings.increaseContrast = !st->glassSettings.increaseContrast;
            UiGlass::SaveGlassSettings(st->glassSettings);
            InvalidateViews(st);
            return;
        }
        // 5. Trim RAM / Cache action button
        if (pt.y >= pr.top + S(194) && pt.y <= pr.top + S(226)) {
            ExecuteToolbarCommand(st, CMD_MEDIA_TRIM_MEMORY);
            return;
        }
    }
}

void PaintAppearancePopoverOnHdc(StudioAppState* st, HDC hdcTarget, int clientW, int clientH) {
    if (!st || !st->appearancePopoverOpen) return;
    (void)clientW;
    (void)clientH;
    const UINT dpi = st->currentDpi;
    auto S = [&](int px) { return DpiSc(px, dpi); };
    const RECT pr = st->appearancePopoverRc;
    const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glassSettings);

    Gdiplus::Graphics g(hdcTarget);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    SetBkMode(hdcTarget, TRANSPARENT);

    // Clean flat zero-GPU guías de interfaz floating surface (zero Dual Kawase blur, zero GPU shaders)
    {
        Gdiplus::GraphicsPath cardP;
        UiGlass::AddRoundPath(cardP, static_cast<float>(pr.left), static_cast<float>(pr.top),
            static_cast<float>(pr.right - pr.left), static_cast<float>(pr.bottom - pr.top), static_cast<float>(S(14)));
        Gdiplus::SolidBrush fillBr(UiGlass::ToGdip(pal.contentCard));
        g.FillPath(&fillBr, &cardP);
        Gdiplus::Pen borderPen(UiGlass::ToGdip(st->glassSettings.IsHighContrast() ? pal.borderStrong : pal.borderSubtle), 1.0f);
        g.DrawPath(&borderPen, &cardP);
    }

    HGDIOBJ oldF = SelectObject(hdcTarget, st->hUiBoldFont ? st->hUiBoldFont : st->hUiFont);
    SetTextColor(hdcTarget, pal.labelPrimary.ToColorRef());
    RECT titleRc{ pr.left + S(16), pr.top + S(12), pr.right - S(16), pr.top + S(34) };
    DrawTextW(hdcTarget, L"Apariencia y Rendimiento", -1, &titleRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Theme Segmented Control: Claro | Oscuro | Contraste
    RECT segOuter{ pr.left + S(16), pr.top + S(44), pr.right - S(16), pr.top + S(74) };
    Gdiplus::GraphicsPath segTrack;
    UiGlass::AddRoundPath(segTrack, static_cast<float>(segOuter.left), static_cast<float>(segOuter.top),
        static_cast<float>(segOuter.right - segOuter.left), static_cast<float>(segOuter.bottom - segOuter.top), static_cast<float>(S(9)));
    Gdiplus::SolidBrush trBr(st->glassSettings.IsDark() ? Gdiplus::Color(255, 28, 32, 42) : Gdiplus::Color(255, 232, 236, 244));
    g.FillPath(&trBr, &segTrack);

    const wchar_t* themeNames[3] = { L"Claro", L"Oscuro", L"Contraste" };
    const int sw = (segOuter.right - segOuter.left - S(6)) / 3;
    SelectObject(hdcTarget, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
    for (int i = 0; i < 3; ++i) {
        const bool sel = (static_cast<int>(st->glassSettings.mode) == i);
        RECT sr{ segOuter.left + S(3) + i * sw, segOuter.top + S(3), segOuter.left + S(3) + (i + 1) * sw, segOuter.bottom - S(3) };
        if (sel) {
            Gdiplus::GraphicsPath sp;
            UiGlass::AddRoundPath(sp, static_cast<float>(sr.left), static_cast<float>(sr.top),
                static_cast<float>(sr.right - sr.left), static_cast<float>(sr.bottom - sr.top), static_cast<float>(S(7)));
            Gdiplus::SolidBrush sbr(UiGlass::ToGdip(pal.contentCardElevated));
            g.FillPath(&sbr, &sp);
            Gdiplus::Pen spen(UiGlass::ToGdip(pal.accentBlue, 190), 1.0f);
            g.DrawPath(&spen, &sp);
        }
        SetTextColor(hdcTarget, sel ? pal.accentBlue.ToColorRef() : pal.labelSecondary.ToColorRef());
        DrawTextW(hdcTarget, themeNames[i], -1, &sr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    auto drawToggleRow = [&](int y0, const wchar_t* text, bool on) {
        RECT txt{ pr.left + S(16), y0, pr.right - S(62), y0 + S(24) };
        SetTextColor(hdcTarget, pal.labelPrimary.ToColorRef());
        DrawTextW(hdcTarget, text, -1, &txt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const float tx = static_cast<float>(pr.right - S(52));
        const float ty0 = static_cast<float>(y0 + S(3));
        const float tw = static_cast<float>(S(36));
        const float th = static_cast<float>(S(18));
        Gdiplus::GraphicsPath tp;
        UiGlass::AddRoundPath(tp, tx, ty0, tw, th, th * 0.5f);
        Gdiplus::SolidBrush tb(on ? UiGlass::ToGdip(pal.accentBlue) : Gdiplus::Color(255, 168, 176, 190));
        g.FillPath(&tb, &tp);
        const float knobD = static_cast<float>(S(14));
        const float kx = on ? (tx + tw - knobD - static_cast<float>(S(2))) : (tx + static_cast<float>(S(2)));
        Gdiplus::SolidBrush kb(Gdiplus::Color(255, 255, 255, 255));
        g.FillEllipse(&kb, kx, ty0 + static_cast<float>(S(2)), knobD, knobD);
    };
    drawToggleRow(pr.top + S(88),  L"Modo 0% GPU (Direct2D SW)", !st->renderer.IsHardwareAccelerationEnabled());
    drawToggleRow(pr.top + S(120), L"Reducir transparencia (Mica)", st->glassSettings.reduceTransparency);
    drawToggleRow(pr.top + S(152), L"Aumentar contraste", st->glassSettings.increaseContrast);

    // Trim RAM / Cache button
    RECT trimBtn{ pr.left + S(16), pr.top + S(194), pr.right - S(16), pr.top + S(226) };
    {
        Gdiplus::GraphicsPath bp;
        UiGlass::AddRoundPath(bp, static_cast<float>(trimBtn.left), static_cast<float>(trimBtn.top),
            static_cast<float>(trimBtn.right - trimBtn.left), static_cast<float>(trimBtn.bottom - trimBtn.top), static_cast<float>(S(8)));
        Gdiplus::SolidBrush bb(UiGlass::ToGdip(pal.contentCardElevated));
        g.FillPath(&bb, &bp);
        Gdiplus::Pen bpen(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
        g.DrawPath(&bpen, &bp);
        SelectObject(hdcTarget, st->hUiBoldFont ? st->hUiBoldFont : st->hUiFont);
        SetTextColor(hdcTarget, pal.accentBlue.ToColorRef());
        DrawTextW(hdcTarget, L"Liberar cach\u00E9 y RAM ahora", -1, &trimBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (oldF) SelectObject(hdcTarget, oldF);
}

// ---------------- Borderless Edge-to-Edge guías de interfaz Sidebar WndProc ----------------

LRESULT CALLBACK SidebarWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StudioAppState* st = reinterpret_cast<StudioAppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<StudioAppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEWHEEL: {
        if (!st) break;
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->sidebarContentScrollY = std::clamp(st->sidebarContentScrollY - (delta / 3), 0, (std::max)(0, st->sidebarMaxScrollY));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!st) break;
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = -1;
        for (std::size_t i = 0; i < st->sidebarHits.size(); ++i) {
            if (PtInRect(&st->sidebarHits[i].rc, pt)) { hit = static_cast<int>(i); break; }
        }
        if (hit != st->sidebarHoverIdx) {
            st->sidebarHoverIdx = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (st && st->sidebarHoverIdx != -1) {
            st->sidebarHoverIdx = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!st) break;
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (const auto& hit : st->sidebarHits) {
            if (PtInRect(&hit.rc, pt)) {
                if (hit.tabIdx >= 0 && hit.tabIdx < 100) {
                    // Inspector domain tab switch
                    st->inspectorTab = hit.tabIdx;
                    st->inspectorScrollY = 0;
                    st->inspectorVisible = true;
                    LayoutMainChildren(st);
                    InvalidateViews(st);
                } else if (hit.tabIdx >= 1000) {
                    // PDF page index jump (1000 + pageIdx)
                    const std::size_t pIdx = static_cast<std::size_t>(hit.tabIdx - 1000);
                    if (pIdx < st->doc.Pages().size()) {
                        st->activePageIndex = pIdx;
                        CenterCameraOnPage(st, pIdx);
                        InvalidateViews(st);
                    }
                } else if (hit.cmdId != 0) {
                    ExecuteToolbarCommand(st, hit.cmdId);
                }
                return 0;
            }
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (st) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
            const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
            const UINT dpi = st->currentDpi;
            auto S = [&](int px) { return DpiSc(px, dpi); };

            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ old = SelectObject(mem, bmp);

            const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glassSettings);
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            Gdiplus::SolidBrush bgBr(UiGlass::ToGdip(pal.sidebarBg));
            g.FillRectangle(&bgBr, 0, 0, w, h);
            SetBkMode(mem, TRANSPARENT);

            st->sidebarHits.clear();
            int y = S(8) - st->sidebarContentScrollY;

            auto drawSectionHeader = [&](const wchar_t* title) {
                RECT sr{ S(14), y, w - S(12), y + S(16) };
                if (sr.bottom >= 0 && sr.top <= h) {
                    SelectObject(mem, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
                    SetTextColor(mem, pal.labelTertiary.ToColorRef());
                    DrawTextW(mem, title, -1, &sr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                }
                y += S(18);
            };

            auto drawHairline = [&]() {
                if (y >= 0 && y <= h) {
                    Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);
                    g.DrawLine(&sepPen, static_cast<float>(S(14)), static_cast<float>(y), static_cast<float>(w - S(14)), static_cast<float>(y));
                }
                y += S(6);
            };

            // Pure guías de interfaz borderless row (ZERO boxed cards!)
            auto drawRow = [&](const wchar_t* title, UiGlass::IconId icon, UiGlass::Rgba8 accent, bool active, WORD cmdId, int tabIdx = -1) {
                const int rowH = S(25);
                RECT r{ S(8), y, w - S(8), y + rowH };
                const int idx = static_cast<int>(st->sidebarHits.size());
                st->sidebarHits.push_back({ r, cmdId, tabIdx });
                if (r.bottom >= 0 && r.top <= h) {
                    const bool hov = (idx == st->sidebarHoverIdx);
                    if (active || hov) {
                        Gdiplus::GraphicsPath rp;
                        UiGlass::AddRoundPath(rp, static_cast<float>(r.left), static_cast<float>(r.top),
                            static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top), static_cast<float>(S(6)));
                        Gdiplus::SolidBrush rb(UiGlass::ToGdip(active ? pal.sidebarSelected : pal.glassPillHover));
                        g.FillPath(&rb, &rp);
                    }
                    UiGlass::DrawVectorIconGdiPlus(g, icon,
                        static_cast<float>(r.left + S(14)), static_cast<float>(r.top + rowH / 2),
                        static_cast<float>(S(13)), UiGlass::ToGdip(accent), 1.55f);
                    SelectObject(mem, active && st->hUiBoldFont ? st->hUiBoldFont : st->hUiFont);
                    SetTextColor(mem, active ? pal.labelPrimary.ToColorRef() : pal.labelSecondary.ToColorRef());
                    RECT tr{ r.left + S(28), r.top, r.right - S(8), r.bottom };
                    DrawTextW(mem, title, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                y += rowH + S(1);
            };

            // 1. ESPACIOS
            drawSectionHeader(L"ESPACIOS");
            drawRow(L"Biblioteca",       UiGlass::IconId::LibraryBooks,   pal.accentBlue,   false, CMD_CONTENT_VIEWS);
            drawRow(L"P\u00E1ginas PDF", UiGlass::IconId::PdfPages,       pal.accentBlue,   st->doc.Mode() == DocumentMode::PaginatedPdf, CMD_MODE_PDF);
            drawRow(L"Lienzo Infinito",  UiGlass::IconId::InfiniteCanvas, pal.accentIndigo, st->doc.Mode() == DocumentMode::InfiniteCanvas, CMD_MODE_CANVAS);
            drawRow(L"B\u00F3veda y Grafo", UiGlass::IconId::GraphNetwork, pal.accentPurple, st->graphOverlayVisible, CMD_TOGGLE_GRAPH_VIEW);
            drawRow(L"Bases de Datos",   UiGlass::IconId::DatabaseTable,  pal.accentTeal,   st->dbPanelVisible, CMD_TOGGLE_DB_PANEL);
            drawRow(L"Modo Lectura",     UiGlass::IconId::ReadingBook,    pal.accentOrange, st->readingViewMode, CMD_TOGGLE_READING_VIEW);

            y += S(4); drawHairline();

            // 2. DOMINIOS (opens right Contextual Inspector on each semantic tab)
            drawSectionHeader(L"DOMINIOS");
            drawRow(L"Tinta y Objetos",        UiGlass::IconId::PenTool,             pal.accentBlue,   st->inspectorVisible && st->inspectorTab == 0, 0, 0);
            drawRow(L"Ingenier\u00EDa STEM",   UiGlass::IconId::StemAtom,            pal.accentGreen,  st->inspectorVisible && st->inspectorTab == 2, 0, 2);
            drawRow(L"IA y Estudio",           UiGlass::IconId::StudySparkles,       pal.accentPurple, st->inspectorVisible && st->inspectorTab == 3, 0, 3);
            drawRow(L"Accesibilidad y 0% GPU", UiGlass::IconId::AccessibilityPerson, pal.accentTeal,   st->inspectorVisible && st->inspectorTab == 4, 0, 4);
            drawRow(L"Capas, Audio y TEC",     UiGlass::IconId::AcademicCap,         pal.accentOrange, st->inspectorVisible && st->inspectorTab == 5, 0, 5);

            y += S(4); drawHairline();

            // 3. INSERTAR MULTIMEDIA Y BLOQUES
            drawSectionHeader(L"INSERTAR");
            drawRow(L"Enlace interactivo",  UiGlass::IconId::LinkChain,  pal.accentBlue,   false, CMD_INSERT_LINK_CARD);
            drawRow(L"Video reproducible",  UiGlass::IconId::VideoPlay,  pal.accentRed,    false, CMD_INSERT_VIDEO_CLIP);
            drawRow(L"GIF animado",         UiGlass::IconId::CompareGrid,pal.accentPurple, false, CMD_INSERT_GIF);
            drawRow(L"Bloque HTML animado", UiGlass::IconId::HtmlCode,   pal.accentTeal,   false, CMD_INSERT_HTML_EMBED);
            drawRow(L"F\u00F3rmula 2D / CAS",UiGlass::IconId::TextMath,  pal.accentGreen,  false, CMD_ADD_FORMULA);
            drawRow(L"Copia impresa",       UiGlass::IconId::Document,   pal.accentOrange, false, CMD_INSERT_PRINTOUT);

            y += S(4); drawHairline();

            // 4. CONTEXTUAL: PDF Pages/TOC OR Infinite Canvas Tools
            if (st->doc.Mode() == DocumentMode::PaginatedPdf) {
                if (!st->doc.Bookmarks().empty()) {
                    drawSectionHeader(L"\u00CDNDICE");
                    const std::size_t bmShow = (std::min<std::size_t>)(st->doc.Bookmarks().size(), 6u);
                    for (std::size_t b = 0; b < bmShow; ++b) {
                        const auto& bm = st->doc.Bookmarks()[b];
                        const std::wstring bTxt = Utf8ToWide(bm.titleUtf8);
                        drawRow(bTxt.c_str(), UiGlass::IconId::ChevronRight, pal.accentBlue,
                            bm.pageIndex == st->activePageIndex, 0, 1000 + static_cast<int>(bm.pageIndex));
                    }
                    y += S(4); drawHairline();
                }
                const std::wstring pgHdr = L"P\u00C1GINAS (" + std::to_wstring(st->doc.Pages().size()) + L")";
                drawSectionHeader(pgHdr.c_str());
                const int itemH = S(78);
                for (std::size_t i = 0; i < st->doc.Pages().size(); ++i) {
                    RECT r{ S(10), y, w - S(10), y + itemH - S(6) };
                    const int idx = static_cast<int>(st->sidebarHits.size());
                    st->sidebarHits.push_back({ r, 0, 1000 + static_cast<int>(i) });
                    if (r.bottom >= 0 && r.top <= h) {
                        const bool active = (i == st->activePageIndex);
                        const bool hov = (idx == st->sidebarHoverIdx);
                        if (active || hov) {
                            Gdiplus::GraphicsPath rp;
                            UiGlass::AddRoundPath(rp, static_cast<float>(r.left), static_cast<float>(r.top),
                                static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top), static_cast<float>(S(8)));
                            Gdiplus::SolidBrush rb(UiGlass::ToGdip(active ? pal.sidebarSelected : pal.glassPillHover));
                            g.FillPath(&rb, &rp);
                        }
                        const auto& pg = st->doc.Pages()[i];
                        RECT thumb{ r.left + S(6), r.top + S(6), r.left + S(52), r.bottom - S(6) };
                        Gdiplus::SolidBrush tBg(Gdiplus::Color(255, 255, 255, 255));
                        g.FillRectangle(&tBg, thumb.left, thumb.top, thumb.right - thumb.left, thumb.bottom - thumb.top);
                        if (pg.hasPdfSource) {
                            const PageCacheKey tkey = PageRenderCache::MakeKey(pg, 160);
                            RasterImageBgra32 img;
                            if (st->pageCache.TryGet(tkey, img) && img.Valid()) {
                                BITMAPINFO bmi{};
                                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                                bmi.bmiHeader.biWidth = img.width;
                                bmi.bmiHeader.biHeight = -img.height;
                                bmi.bmiHeader.biPlanes = 1;
                                bmi.bmiHeader.biBitCount = 32;
                                bmi.bmiHeader.biCompression = BI_RGB;
                                SetStretchBltMode(mem, HALFTONE);
                                StretchDIBits(mem, thumb.left + 1, thumb.top + 1, (thumb.right - thumb.left) - 2, (thumb.bottom - thumb.top) - 2,
                                    0, 0, img.width, img.height, img.pixels.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
                            } else {
                                HWND mainH = st->hwndMain;
                                st->pageCache.RequestAsync(tkey, st->asyncGen, [mainH]() {
                                    if (IsWindow(mainH)) PostMessageW(mainH, WM_APP_ASYNC_PAGE_READY, 0, 0);
                                });
                            }
                        }
                        Gdiplus::Pen tPen(UiGlass::ToGdip(active ? pal.accentBlue : pal.borderSubtle), active ? 1.5f : 1.0f);
                        g.DrawRectangle(&tPen, thumb.left, thumb.top, thumb.right - thumb.left, thumb.bottom - thumb.top);

                        SelectObject(mem, active && st->hUiBoldFont ? st->hUiBoldFont : st->hUiFont);
                        SetTextColor(mem, pal.labelPrimary.ToColorRef());
                        const std::wstring lbl = L"P\u00E1g. " + std::to_wstring(i + 1);
                        RECT txtRc{ thumb.right + S(10), r.top + S(10), r.right - S(8), r.top + S(30) };
                        DrawTextW(mem, lbl.c_str(), -1, &txtRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                        SelectObject(mem, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
                        SetTextColor(mem, pal.labelTertiary.ToColorRef());
                        const std::wstring dimLbl = std::to_wstring(static_cast<int>(pg.EffectiveWidthPt())) + L" \u00D7 " +
                            std::to_wstring(static_cast<int>(pg.EffectiveHeightPt())) + L" pt";
                        RECT dimRc{ thumb.right + S(10), r.top + S(32), r.right - S(8), r.top + S(50) };
                        DrawTextW(mem, dimLbl.c_str(), -1, &dimRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                    }
                    y += itemH;
                }
            } else {
                drawSectionHeader(L"HERRAMIENTAS");
                drawRow(L"Plantilla de papel",   UiGlass::IconId::PaperGrid,      pal.accentBlue,   false, CMD_PAPER_NEXT);
                drawRow(L"Regla graduada",       UiGlass::IconId::RulerGraduated, pal.accentTeal,   st->rulerState.visible, CMD_TOGGLE_RULER);
                drawRow(L"Minimapa espacial",    UiGlass::IconId::CompareGrid,    pal.accentIndigo, st->minimapVisible, CMD_TOGGLE_MINIMAP);
                drawRow(L"Caja de zoom",         UiGlass::IconId::ZoomBox,        pal.accentBlue,   st->zoomWritingBox.active, CMD_ZOOM_WRITING_BOX);
                drawRow(st->audioSession.IsContinuousRecording() ? L"Detener audio (Mic)" : L"Audio sincronizado",
                                                 UiGlass::IconId::AudioWave,      pal.accentRed,    st->audioSession.IsContinuousRecording(), CMD_AUDIO_REC);
                drawRow(L"Comentarios anclados", UiGlass::IconId::CommentBubble,  pal.accentOrange, st->commentsPanelVisible, CMD_ANCHORED_COMMENTS);
                drawRow(L"Modo Concentraci\u00F3n", UiGlass::IconId::ContrastCircle, pal.accentPurple, st->focusMode, CMD_FOCUS_MODE);
            }

            const int totalContentH = y + st->sidebarContentScrollY + S(14);
            st->sidebarMaxScrollY = (std::max)(0, totalContentH - h);
            if (st->sidebarContentScrollY > st->sidebarMaxScrollY) st->sidebarContentScrollY = st->sidebarMaxScrollY;

            // Subtle iPadOS-style scroll thumb when scrollable
            if (st->sidebarMaxScrollY > 0) {
                const float barH = (std::max)(24.0f, static_cast<float>(h * h) / static_cast<float>((std::max)(1, totalContentH)));
                const float barY = (static_cast<float>(st->sidebarContentScrollY) / static_cast<float>(st->sidebarMaxScrollY)) * (static_cast<float>(h) - barH - 8.0f) + 4.0f;
                Gdiplus::GraphicsPath sp;
                UiGlass::AddRoundPath(sp, static_cast<float>(w - 5), barY, 3.0f, barH, 1.5f);
                Gdiplus::SolidBrush sb(UiGlass::ToGdip(pal.labelTertiary, 110));
                g.FillPath(&sb, &sp);
            }

            // Crisp right separator hairline
            Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);
            g.DrawLine(&sepPen, static_cast<float>(w - 1), 0.0f, static_cast<float>(w - 1), static_cast<float>(h));

            BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------- Right Collapsible Contextual 6-Tab Inspector (guías de interfaz) ----------------

LRESULT CALLBACK InspectorWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StudioAppState* st = reinterpret_cast<StudioAppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<StudioAppState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEWHEEL: {
        if (!st) break;
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->inspectorScrollY = std::clamp(st->inspectorScrollY - (delta / 3), 0, (std::max)(0, st->inspectorMaxScrollY));
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!st) break;
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int hit = -1;
        for (std::size_t i = 0; i < st->inspectorHits.size(); ++i) {
            if (PtInRect(&st->inspectorHits[i].rc, pt)) { hit = static_cast<int>(i); break; }
        }
        if (hit != st->inspectorHoverIdx) {
            st->inspectorHoverIdx = hit;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE: {
        if (st && st->inspectorHoverIdx != -1) {
            st->inspectorHoverIdx = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!st) break;
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (const auto& hit : st->inspectorHits) {
            if (PtInRect(&hit.rc, pt)) {
                if (hit.tabIdx >= 0) {
                    st->inspectorTab = hit.tabIdx;
                    st->inspectorScrollY = 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    if (st->hwndSidebar && IsWindowVisible(st->hwndSidebar)) InvalidateRect(st->hwndSidebar, nullptr, FALSE);
                } else if (hit.cmdId != 0) {
                    ExecuteToolbarCommand(st, hit.cmdId);
                }
                return 0;
            }
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (st) {
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
            const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
            const UINT dpi = st->currentDpi;
            auto S = [&](int px) { return DpiSc(px, dpi); };

            HDC mem = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
            HGDIOBJ old = SelectObject(mem, bmp);

            const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glassSettings);
            Gdiplus::Graphics g(mem);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            st->inspectorHits.clear();

            Gdiplus::SolidBrush bgBr(UiGlass::ToGdip(pal.inspectorBg));
            g.FillRectangle(&bgBr, 0, 0, w, h);
            Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);
            g.DrawLine(&sepPen, 0.0f, 0.0f, 0.0f, static_cast<float>(h));
            SetBkMode(mem, TRANSPARENT);

            // Clean 6-tab segmented grid (2 rows x 3 cols) at top of Inspector
            const int tabAreaTop = S(10);
            const int tabRowH = S(26);
            const int tabAreaH = tabRowH * 2 + S(8);
            RECT segOuter{ S(12), tabAreaTop, w - S(12), tabAreaTop + tabAreaH };
            {
                Gdiplus::GraphicsPath segP;
                UiGlass::AddRoundPath(segP, static_cast<float>(segOuter.left), static_cast<float>(segOuter.top),
                    static_cast<float>(segOuter.right - segOuter.left), static_cast<float>(segOuter.bottom - segOuter.top), static_cast<float>(S(9)));
                Gdiplus::SolidBrush segTr(st->glassSettings.IsHighContrast()
                    ? Gdiplus::Color(255, 240, 243, 248)
                    : (st->glassSettings.IsDark() ? Gdiplus::Color(255, 16, 19, 25) : Gdiplus::Color(255, 232, 236, 244)));
                g.FillPath(&segTr, &segP);
                if (st->glassSettings.IsHighContrast()) {
                    Gdiplus::Pen segB(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                    g.DrawPath(&segB, &segP);
                }
            }

            const wchar_t* tabLabels[6] = { L"Tinta", L"B\u00F3veda", L"STEM", L"IA", L"A11y", L"TEC" };
            const int tw = (segOuter.right - segOuter.left - S(6)) / 3;
            for (int i = 0; i < 6; ++i) {
                const int row = i / 3;
                const int col = i % 3;
                RECT tr{
                    segOuter.left + S(3) + col * tw,
                    segOuter.top + S(3) + row * (tabRowH + S(2)),
                    segOuter.left + S(3) + (col + 1) * tw,
                    segOuter.top + S(3) + row * (tabRowH + S(2)) + tabRowH
                };
                st->inspectorHits.push_back({ tr, 0, i });
                const bool sel = (st->inspectorTab == i);
                if (sel) {
                    Gdiplus::GraphicsPath tp;
                    UiGlass::AddRoundPath(tp, static_cast<float>(tr.left), static_cast<float>(tr.top),
                        static_cast<float>(tr.right - tr.left), static_cast<float>(tr.bottom - tr.top), static_cast<float>(S(6)));
                    const UiGlass::Rgba8 selFill = st->glassSettings.IsHighContrast()
                        ? pal.sidebarSelected
                        : (st->glassSettings.IsDark() ? pal.contentCardElevated : pal.contentCard);
                    Gdiplus::SolidBrush tb(UiGlass::ToGdip(selFill));
                    g.FillPath(&tb, &tp);
                    if (st->glassSettings.IsHighContrast() || st->glassSettings.IsDark()) {
                        Gdiplus::Pen tpen(UiGlass::ToGdip(st->glassSettings.IsHighContrast() ? pal.borderStrong : pal.borderStrong),
                            st->glassSettings.IsHighContrast() ? 1.5f : 1.0f);
                        g.DrawPath(&tpen, &tp);
                    }
                }
                SelectObject(mem, sel && st->hUiBoldFont ? st->hUiBoldFont : (st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont));
                const COLORREF tabFg = sel
                    ? (st->glassSettings.IsDark() ? pal.labelPrimary.ToColorRef() : pal.accentBlue.ToColorRef())
                    : pal.labelSecondary.ToColorRef();
                SetTextColor(mem, tabFg);
                DrawTextW(mem, tabLabels[i], -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            const int contentTop = segOuter.bottom + S(10);
            g.DrawLine(&sepPen, static_cast<float>(S(12)), static_cast<float>(contentTop - S(4)),
                static_cast<float>(w - S(12)), static_cast<float>(contentTop - S(4)));

            // Clip scrollable body strictly below tab bar
            HRGN bodyClip = CreateRectRgn(1, contentTop, w, h);
            SelectClipRgn(mem, bodyClip);
            g.SetClip(Gdiplus::Rect(1, contentTop, (std::max)(1, w - 1), (std::max)(1, h - contentTop)));

            int y = contentTop + S(4) - st->inspectorScrollY;
            const int padX = S(14);
            const int bodyW = (std::max)(60, w - padX * 2);

            auto drawSecHdr = [&](const wchar_t* title, UiGlass::IconId icon, UiGlass::Rgba8 accent) {
                RECT hr{ padX, y, w - padX, y + S(22) };
                if (hr.bottom >= contentTop && hr.top <= h) {
                    UiGlass::DrawVectorIconGdiPlus(g, icon, static_cast<float>(padX + S(8)), static_cast<float>(y + S(11)),
                        static_cast<float>(S(13)), UiGlass::ToGdip(accent), 1.55f);
                    SelectObject(mem, st->hUiBoldFont ? st->hUiBoldFont : st->hUiFont);
                    SetTextColor(mem, pal.labelPrimary.ToColorRef());
                    RECT tr{ padX + S(22), y, w - padX, y + S(22) };
                    DrawTextW(mem, title, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                y += S(25);
            };

            auto drawInfoText = [&](const std::wstring& txt, int lines = 2) {
                const int th = S(16) * lines + S(4);
                RECT ir{ padX, y, w - padX, y + th };
                if (ir.bottom >= contentTop && ir.top <= h) {
                    SelectObject(mem, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
                    SetTextColor(mem, pal.labelSecondary.ToColorRef());
                    DrawTextW(mem, txt.c_str(), -1, &ir, DT_LEFT | DT_WORDBREAK);
                }
                y += th + S(4);
            };

            auto drawBadge = [&](const std::wstring& txt, UiGlass::Rgba8 accent) {
                RECT bdg{ padX, y, w - padX, y + S(22) };
                if (bdg.bottom >= contentTop && bdg.top <= h) {
                    Gdiplus::GraphicsPath bp;
                    UiGlass::AddRoundPath(bp, static_cast<float>(bdg.left), static_cast<float>(bdg.top),
                        static_cast<float>(bdg.right - bdg.left), static_cast<float>(bdg.bottom - bdg.top), static_cast<float>(S(6)));
                    const std::uint8_t tintA = st->glassSettings.IsHighContrast() ? 16u : (st->glassSettings.IsDark() ? 46u : 22u);
                    Gdiplus::SolidBrush bbr(UiGlass::ToGdip(accent, tintA));
                    g.FillPath(&bbr, &bp);
                    if (st->glassSettings.IsHighContrast()) {
                        Gdiplus::Pen bpen(UiGlass::ToGdip(accent, 220), 1.0f);
                        g.DrawPath(&bpen, &bp);
                    }
                    SelectObject(mem, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
                    SetTextColor(mem, accent.ToColorRef());
                    DrawTextW(mem, txt.c_str(), -1, &bdg, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                y += S(28);
            };

            auto drawDivider = [&]() {
                if (y >= contentTop && y <= h) {
                    g.DrawLine(&sepPen, static_cast<float>(padX), static_cast<float>(y),
                        static_cast<float>(w - padX), static_cast<float>(y));
                }
                y += S(10);
            };

            auto drawBtn = [&](const wchar_t* lbl, WORD cmd, bool primary = false) {
                const int bh = S(28);
                RECT br{ padX, y, w - padX, y + bh };
                const int hitIdx = static_cast<int>(st->inspectorHits.size());
                if (br.bottom >= contentTop && br.top <= h) {
                    st->inspectorHits.push_back({ br, cmd, -1 });
                    const bool hov = (hitIdx == st->inspectorHoverIdx);
                    Gdiplus::GraphicsPath bp;
                    UiGlass::AddRoundPath(bp, static_cast<float>(br.left), static_cast<float>(br.top),
                        static_cast<float>(br.right - br.left), static_cast<float>(br.bottom - br.top), static_cast<float>(S(7)));
                    Gdiplus::SolidBrush bb(primary
                        ? UiGlass::ToGdip(pal.accentBlue)
                        : UiGlass::ToGdip(hov ? pal.glassPillHover : pal.contentCard));
                    g.FillPath(&bb, &bp);
                    if (!primary) {
                        Gdiplus::Pen bpen(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                        g.DrawPath(&bpen, &bp);
                    }
                    SelectObject(mem, primary && st->hUiBoldFont ? st->hUiBoldFont : (st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont));
                    SetTextColor(mem, primary ? RGB(255, 255, 255) : pal.labelPrimary.ToColorRef());
                    RECT tx{ br.left + S(8), br.top, br.right - S(8), br.bottom };
                    DrawTextW(mem, lbl, -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                }
                y += bh + S(6);
            };

            auto drawBtnPair = [&](const wchar_t* l1, WORD c1, const wchar_t* l2, WORD c2) {
                const int bh = S(26);
                const int gap = S(6);
                const int bw = (bodyW - gap) / 2;
                RECT b1{ padX, y, padX + bw, y + bh };
                RECT b2{ padX + bw + gap, y, w - padX, y + bh };
                auto paintOne = [&](const RECT& br, const wchar_t* lbl, WORD cmd) {
                    const int hitIdx = static_cast<int>(st->inspectorHits.size());
                    if (br.bottom >= contentTop && br.top <= h) {
                        st->inspectorHits.push_back({ br, cmd, -1 });
                        const bool hov = (hitIdx == st->inspectorHoverIdx);
                        Gdiplus::GraphicsPath bp;
                        UiGlass::AddRoundPath(bp, static_cast<float>(br.left), static_cast<float>(br.top),
                            static_cast<float>(br.right - br.left), static_cast<float>(br.bottom - br.top), static_cast<float>(S(6)));
                        Gdiplus::SolidBrush bb(UiGlass::ToGdip(hov ? pal.glassPillHover : pal.contentCard));
                        g.FillPath(&bb, &bp);
                        Gdiplus::Pen bpen(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                        g.DrawPath(&bpen, &bp);
                        SelectObject(mem, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
                        SetTextColor(mem, pal.labelPrimary.ToColorRef());
                        RECT tx{ br.left + S(4), br.top, br.right - S(4), br.bottom };
                        DrawTextW(mem, lbl, -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                    }
                };
                paintOne(b1, l1, c1);
                paintOne(b2, l2, c2);
                y += bh + S(6);
            };

            if (st->inspectorTab == 0) {
                // TAB 0: Tinta, Objetos y Multimedia Contextual
                const AnnotationObject* selObj = (st->selectedObjectId != 0)
                    ? CanvasManip::FindObjectConst(st->doc, ObjectTarget{ st->selectedIsCanvas, st->selectedPageIndex }, st->selectedObjectId)
                    : nullptr;
                if (selObj) {
                    drawSecHdr(L"Objeto Seleccionado", UiGlass::IconId::SlidersGlass, pal.accentBlue);
                    if (selObj->kind == ObjectKind::VideoClip) {
                        const bool playing = st->videoPlayer.IsPlaying() && st->videoPlayer.ActiveObjectId() == selObj->id;
                        drawBadge(playing ? L"Video reproduciendo" : L"Video pausado (0% CPU/GPU)", pal.accentRed);
                        drawBtn(playing ? L"Pausar Video" : L"Reproducir Video", CMD_MEDIA_PLAY_PAUSE, true);
                        drawBtn(L"Capturar Fotograma como Imagen", CMD_MEDIA_CAPTURE_FRAME);
                    } else if (selObj->kind == ObjectKind::HtmlEmbed) {
                        const bool live = (st->activeHtmlObjectId == selObj->id);
                        drawBadge(live ? L"HTML interactivo activo" : L"HTML congelado (~0% CPU)", pal.accentTeal);
                        drawBtn(live ? L"Congelar HTML (0% GPU)" : L"Activar HTML Interactivo", CMD_HTML_FREEZE_RESUME, true);
                        drawBtn(L"Capturar Vista como Imagen", CMD_MEDIA_CAPTURE_FRAME);
                    } else if (selObj->kind == ObjectKind::LinkCard) {
                        drawBadge(selObj->linkTargetPageIndex >= 0
                            ? (L"Enlace interno -> P\u00E1g. " + std::to_wstring(selObj->linkTargetPageIndex + 1))
                            : Utf8ToWide(ExtractUrlDomainUtf8(selObj->linkUrlUtf8)), pal.accentBlue);
                    } else if (IsAnimatedGifObject(st->doc, *selObj)) {
                        drawBtn(selObj->gifPaused ? L"Reanudar GIF Animado" : L"Pausar GIF Animado (0% CPU)", CMD_MEDIA_PLAY_PAUSE, true);
                    }
                    drawBtnPair(L"Duplicar", CMD_DUPLICATE_OBJECTS, L"Fijar/Desfijar", CMD_TOGGLE_LOCK_IN_PLACE);
                    drawDivider();
                }

                drawSecHdr(L"Multimedia en Lienzo", UiGlass::IconId::VideoPlay, pal.accentRed);
                drawBtnPair(L"+ Enlace", CMD_INSERT_LINK_CARD, L"+ Video", CMD_INSERT_VIDEO_CLIP);
                drawBtnPair(L"+ GIF", CMD_INSERT_GIF, L"+ HTML", CMD_INSERT_HTML_EMBED);
                drawDivider();

                drawSecHdr(L"Tinta, Escritura y HTR", UiGlass::IconId::PenTool, pal.accentBlue);
                drawBtn(L"Cambiar Estilo de L\u00E1piz", CMD_CYCLE_PEN_STYLE, true);
                drawBtnPair(L"Fino 1.5", CMD_PEN_WIDTH_FINE, L"Medio 2.6", CMD_PEN_WIDTH_MED);
                drawBtnPair(L"Grueso 4.8", CMD_PEN_WIDTH_BOLD, L"Marcador 8", CMD_PEN_WIDTH_MARKER);
                drawBtnPair(L"HTR -> Texto", CMD_HTR_TO_TEXT, L"HTR -> Math 2D", CMD_HTR_TO_MATH);
                drawBtnPair(L"Enderezar", CMD_RESTYLE_INK, L"Caja de Zoom", CMD_ZOOM_WRITING_BOX);
                drawBtnPair(L"Regla / Comp\u00E1s", CMD_CYCLE_GEOMETRY_TOOL, L"Estabilizador", CMD_TOGGLE_STAB);
                drawDivider();

                drawSecHdr(L"Texto e Im\u00E1genes", UiGlass::IconId::TextMath, pal.accentIndigo);
                drawBtnPair(L"Negrita", CMD_TEXT_BOLD, L"Cursiva", CMD_TEXT_ITALIC);
                drawBtnPair(L"Vi\u00F1""etas", CMD_TEXT_BULLET_LIST, L"Casilla", CMD_TEXT_CHECKBOX_TOGGLE);
                drawBtnPair(L"Pizarra", CMD_IMG_ENHANCE_WHITEBOARD, L"Rotar +90\u00B0", CMD_IMG_ROTATE_90);
                drawBtnPair(L"Agrupar", CMD_GROUP_OBJECTS, L"Conector", CMD_ADD_CONNECTOR);
            } else if (st->inspectorTab == 1) {
                // TAB 1: Bóveda de Conocimiento, Grafo y Bases de Datos (.base)
                drawSecHdr(L"B\u00F3veda y Grafo", UiGlass::IconId::GraphNetwork, pal.accentPurple);
                drawBtn(st->graphOverlayVisible ? L"Ocultar Grafo Visual" : L"Mostrar Vista de Grafo", CMD_TOGGLE_GRAPH_VIEW, true);
                drawBtnPair(L"Grafo Local", CMD_GRAPH_LOCAL_FILTER, L"Backlinks", CMD_VIEW_BACKLINKS);
                drawBtnPair(L"Nota Diaria", CMD_OPEN_DAILY_NOTE, L"Buscar B\u00F3veda", CMD_VAULT_SEARCH_RENAME);
                drawBtnPair(L"Propiedades", CMD_EDIT_PROPERTIES, L"Esquema", CMD_DOCUMENT_OUTLINE);
                drawBtnPair(L"Plantilla", CMD_NEW_TEMPLATE_NOTE, L"Bloque Sync", CMD_INSERT_SYNCED_BLOCK);
                drawBtn(L"Capturar Web (Web Clipper)", CMD_WEB_CLIPPER_CAPTURE);
                drawDivider();

                drawSecHdr(L"Bases de Datos (9 Vistas)", UiGlass::IconId::DatabaseTable, pal.accentTeal);
                drawBtn(st->dbPanelVisible ? L"Ocultar Panel de Base" : L"Abrir Base de Datos Visual", CMD_TOGGLE_DB_PANEL, true);
                drawBtnPair(L"Editor BD", CMD_DB_EDITOR_MGR, L"Rotar Vista", CMD_CYCLE_DB_VIEW);
                drawBtnPair(L"Formulario", CMD_DB_SUBMIT_FORM, L"Automatizar", CMD_AUTOMATION_RULES);
                drawBtnPair(L"Exportar .base", CMD_EXPORT_BASE_FILE, L"Importar .base", CMD_IMPORT_BASE_FILE);
            } else if (st->inspectorTab == 2) {
                // TAB 2: Ingeniería STEM, CAS y Puente CASIO
                drawSecHdr(L"Motor STEM 2D y CAS", UiGlass::IconId::StemAtom, pal.accentGreen);
                drawBtn(L"+ F\u00F3rmula Natural 2D / CAS", CMD_ADD_FORMULA, true);
                drawBtnPair(L"Lab EDO/Eigen", CMD_ADV_MATH, L"Matriz / SEL", CMD_ADD_MATRIX_SEL);
                drawBtnPair(L"Tabla CSV", CMD_ADD_TABLE, L"C\u00F3digo / MCU", CMD_ADD_CODE_BLOCK);
                drawBtnPair(L"Qu\u00EDmica 2D", CMD_CHEMISTRY_BALANCER, L"Variables SI", CMD_VAR_INSPECTOR);
                drawBtnPair(L"Slider Param", CMD_PARAM_SLIDER, L"Verificar Paso", CMD_CHECK_STEP);
                drawDivider();

                drawSecHdr(L"Circuitos y Gr\u00E1ficas", UiGlass::IconId::CompareGrid, pal.accentBlue);
                drawBtn(L"+ Circuito MNA (DC / AC)", CMD_ADD_CIRCUIT, true);
                drawBtnPair(L"Osciloscopio", CMD_ADD_TRANSIENT_LAPLACE, L"Bode |H(jw)|", CMD_ADD_BODE_PLOT);
                drawBtnPair(L"Fasores", CMD_ADD_PHASOR_DIAG, L"Control PID", CMD_CONTROL_BLOCK_LAB);
                drawBtnPair(L"Gr\u00E1fica 2D", CMD_ADD_PLOT, L"Superficie 3D", CMD_ADD_PLOT_3D);
                drawBtnPair(L"Mapa Karnaugh", CMD_ADD_KMAP, L"Caja Ing.", CMD_ENG_TOOLBOX);
                drawDivider();

                drawSecHdr(L"Puente CASIO Gr\u00E1fico", UiGlass::IconId::Calculator, pal.accentOrange);
                drawBtnPair(L"Tarjeta CASIO", CMD_ADD_CALC_CARD, L"Ecuac -> Teclas", CMD_CASIO_REVERSE_KEYS);
                drawBtnPair(L"Comparar 4", CMD_COMPARE_4_CASIO, L"Memoria HTML", CMD_EXPORT_LAB_HTML);
            } else if (st->inspectorTab == 3) {
                // TAB 3: IA Multi-Proveedor y Apoyo al Estudio
                drawSecHdr(L"Proveedores de IA", UiGlass::IconId::KeyVault, pal.accentPurple);
                const auto* actP = st->aiVault.FindProvider(st->aiVault.ActiveProviderId());
                const std::wstring provBadge = (st->aiVault.IsEnabled() && actP)
                    ? Utf8ToWide("Activo: " + actP->displayNameUtf8)
                    : L"Modo Local Determinista (BM25)";
                drawBadge(provBadge, st->aiVault.IsEnabled() ? pal.accentGreen : pal.accentBlue);
                drawBtn(L"Pizarra Tutor Audiovisual...", CMD_TUTOR_WHITEBOARD_OPEN, true);
                drawBtn(L"Configurar Claves API de IA...", CMD_AI_PROVIDERS_VAULT);
                drawBtn(L"Consulta IA con Citas", CMD_GROUNDED_AI_CITATIONS);
                drawBtnPair(L"Tutor Socr\u00E1tico", CMD_STUDY_TUTOR_STEP, L"Revisar Pasos", CMD_CHECK_MY_WORK);
                drawBtnPair(L"Resumir Notas", CMD_SUMMARIZE_NOTES, L"Traducir ES/EN", CMD_TRANSLATE_SELECTION);
                drawDivider();

                drawSecHdr(L"Estudio y Automatizaciones", UiGlass::IconId::Flashcards, pal.accentBlue);
                drawBtn(L"Generar Flashcards + Cinta", CMD_STUDY_GEN_FLASHCARDS_AI, true);
                drawBtnPair(L"Repasar SM-2", CMD_STUDY_DECK, L"Cuestionario", CMD_AUTO_QUIZ_GENERATOR);
                drawBtnPair(L"Mapa Concept.", CMD_STUDY_CONCEPT_MAP, L"Hoja F\u00F3rmulas", CMD_STUDY_FORMULA_GLOSSARY);
                drawBtnPair(L"Pomodoro", CMD_STUDY_POMODORO, L"Tareas -> .ics", CMD_AUTO_EXTRACT_TASKS_ICS);
                drawBtnPair(L"Post-Audio", CMD_AUTO_POST_LECTURE, L"Citas APA/IEEE", CMD_STUDY_BIBLIOGRAPHY_CITE);
            } else if (st->inspectorTab == 4) {
                // TAB 4: Accesibilidad Universal, Rendimiento 0% GPU y Seguridad
                drawSecHdr(L"Rendimiento y GPU", UiGlass::IconId::SettingsGear, pal.accentGreen);
                const bool isSw = !st->renderer.IsHardwareAccelerationEnabled();
                drawBadge(isSw ? L"Modo Ultra Eficiente \u00B7 0% GPU activo" : L"Aceleraci\u00F3n GPU (opcional)", isSw ? pal.accentGreen : pal.accentOrange);
                PROCESS_MEMORY_COUNTERS pmc{};
                std::wstring ramW = L"Cach\u00E9s acotadas (64 MB)";
                if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
                    char mbBuf[64]{};
                    std::snprintf(mbBuf, sizeof(mbBuf), "Memoria activa: %.1f MB",
                        static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0));
                    ramW = Utf8ToWide(mbBuf);
                }
                drawInfoText(ramW, 1);
                drawBtnPair(isSw ? L"Activar GPU" : L"Modo 0% GPU", CMD_TOGGLE_GPU_ACCEL, L"Liberar RAM", CMD_MEDIA_TRIM_MEMORY);
                drawDivider();

                drawSecHdr(L"Accesibilidad Universal", UiGlass::IconId::AccessibilityPerson, pal.accentTeal);
                drawBtn(L"Centro de Accesibilidad...", CMD_A11Y_CENTER, true);
                drawBtnPair(L"Leer en Voz Alta", CMD_A11Y_TTS_SPEAK, L"Detener Voz", CMD_A11Y_TTS_STOP);
                drawBtnPair(L"Regla Lectura", CMD_A11Y_READING_RULER, L"Modo Dislexia", CMD_A11Y_DYSLEXIA_MODE);
                drawBtnPair(L"Tinte Papel", CMD_A11Y_CYCLE_TINT, L"Daltonismo", CMD_A11Y_CYCLE_COLORBLIND);
                drawDivider();

                drawSecHdr(L"Seguridad y Sincronizaci\u00F3n", UiGlass::IconId::SyncCloud, pal.accentBlue);
                drawBtnPair(L"Bloquear Nota", CMD_LOCK_NOTE_PASSPHRASE, L"Cifrar AES-256", CMD_SAVE_ENCRYPTED);
                drawBtnPair(L"Sync 3 Modos", CMD_FOLDER_SYNC_3MODES, L"Colab LAN", CMD_TOGGLE_LAN_COLLAB_SERVER);
                drawBtn(L"Explorar Comandos (Ctrl+K)", CMD_COMMAND_PALETTE);
            } else {
                // TAB 5: TEC Digital, Capas y Audio Sincronizado
                drawSecHdr(L"TEC Digital", UiGlass::IconId::AcademicCap, pal.accentBlue);
#if defined(DOC_STUDIO_HAS_ACADEMIC)
                const bool tecAuthed = st->tecStore && st->tecStore->IsAuthenticated();
#else
                const bool tecAuthed = false;
#endif
                drawBadge(tecAuthed ? L"Conectado" : L"Sin conectar", tecAuthed ? pal.accentGreen : pal.accentBlue);
                drawInfoText(Utf8ToWide(st->academicStatusUtf8), 2);
                drawBtn(L"Abrir TEC Digital", CMD_ACADEMIC_SYNC_TEC, true);
                drawBtnPair(L"Mis Cursos", CMD_ACADEMIC_EVALS_TEC, L"Horario", CMD_ACADEMIC_SOLVE_SCHEDULE);
                drawBtnPair(L"Malla Curricular", CMD_ACADEMIC_MALLA_TEC, L"Notas", CMD_ACADEMIC_NOTAS_TEC);
                drawBtnPair(L"Calendario", CMD_ACADEMIC_CALENDAR_TEC, L"Biblioteca", CMD_ACADEMIC_SEARCH_MATERIALS);
                drawDivider();

                drawSecHdr(L"Gemini Notebook", UiGlass::IconId::SyncCloud, pal.accentBlue);
                drawBtn(L"Abrir Gemini Notebook...", CMD_GEMINI_NOTEBOOK_HUB, true);
                drawBtnPair(L"Enviar Cuaderno", CMD_GEMINI_NOTEBOOK_SEND_DOC, L"Agentes IA", CMD_GEMINI_NOTEBOOK_MCP_CONFIG);
                drawDivider();

                drawSecHdr(L"Capas del Documento", UiGlass::IconId::LayersStack, pal.accentIndigo);
                drawInfoText(L"Capas: " + std::to_wstring(st->doc.Layers().size()) + L"  |  Activa #" + std::to_wstring(st->doc.ActiveLayerId()), 1);
                drawBtn(L"Gestor de Capas...", CMD_LAYERS_MANAGER);
                drawBtnPair(L"+ Nueva Capa", CMD_LAYER_ADD, L"Capa Docente", CMD_TEACHER_PRIVATE_LAYER);
                drawDivider();

                drawSecHdr(L"Audio e Ink Replay", UiGlass::IconId::MicAudio, pal.accentRed);
                drawBtn(st->audioSession.IsContinuousRecording() ? L"Detener Grabaci\u00F3n (Mic)" : L"Grabar Audio Continuo", CMD_AUDIO_REC, true);
                drawBtnPair(L"Ink Replay", CMD_REPLAY_TOGGLE, L"Transcribir", CMD_AUDIO_TRANSCRIBE);
                drawBtnPair(L"Diarizar", CMD_AUDIO_DIARIZE, L"Acta .ics", CMD_MEETING_MINUTES_ICS);
                drawBtnPair(L"Anotac. PDF", CMD_PDF_ANNOTATIONS_MGR, L"Firmar PKCS#7", CMD_SIGN_PDF);
            }

            const int totalH = (y + st->inspectorScrollY) - contentTop + S(16);
            const int visH = (std::max)(1, h - contentTop);
            st->inspectorMaxScrollY = (std::max)(0, totalH - visH);
            if (st->inspectorScrollY > st->inspectorMaxScrollY) st->inspectorScrollY = st->inspectorMaxScrollY;

            SelectClipRgn(mem, nullptr);
            DeleteObject(bodyClip);
            g.ResetClip();

            if (st->inspectorMaxScrollY > 0) {
                const float barH = (std::max)(24.0f, static_cast<float>(visH * visH) / static_cast<float>((std::max)(1, totalH)));
                const float barY = static_cast<float>(contentTop) + (static_cast<float>(st->inspectorScrollY) / static_cast<float>(st->inspectorMaxScrollY)) * (static_cast<float>(visH) - barH - 8.0f) + 4.0f;
                Gdiplus::GraphicsPath sp;
                UiGlass::AddRoundPath(sp, static_cast<float>(w - 5), barY, 3.0f, barH, 1.5f);
                Gdiplus::SolidBrush sb(UiGlass::ToGdip(pal.labelTertiary, 110));
                g.FillPath(&sb, &sp);
            }

            BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old);
            DeleteObject(bmp);
            DeleteDC(mem);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---------------- Flat Zero-GPU guías de interfaz Chrome & Interactive Status Renderer ----------------

void PaintRibbonAndStatus(StudioAppState* st, HDC hdc) {
    RECT rc{};
    GetClientRect(st->hwndMain, &rc);
    const int w = (std::max)(1, static_cast<int>(rc.right - rc.left));
    const int h = (std::max)(1, static_cast<int>(rc.bottom - rc.top));
    const UINT dpi = st->currentDpi;
    auto S = [&](int px) { return DpiSc(px, dpi); };

    const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glassSettings);
    const int topChromeH = S(kTopChromeH);
    const int botStatusH = S(kBottomStatusH);
    const int tabStripH = (st->tabs.size() > 1) ? S(28) : 0;
    const int topTotalH = topChromeH + tabStripH + (st->searchVisible ? S(38) : 0);
    const bool sideRail = (st->uiPrefs.toolbarOrientation == 1 || st->uiPrefs.toolbarOrientation == 2);
    const int wantTopH = sideRail ? h : (std::max)(1, topTotalH);

    // Persistent strip double-buffers (ZERO CreateCompatibleBitmap allocations on hover/paint!)
    if (!st->hTopBufDc) st->hTopBufDc = CreateCompatibleDC(hdc);
    if (!st->hTopBufBmp || st->topBufW != w || st->topBufH != wantTopH) {
        if (st->hTopBufBmp) DeleteObject(st->hTopBufBmp);
        st->hTopBufBmp = CreateCompatibleBitmap(hdc, w, wantTopH);
        st->topBufW = w;
        st->topBufH = wantTopH;
        SelectObject(st->hTopBufDc, st->hTopBufBmp);
    }
    if (!st->hBotBufDc) st->hBotBufDc = CreateCompatibleDC(hdc);
    if (!st->hBotBufBmp || st->botBufW != w || st->botBufH != botStatusH) {
        if (st->hBotBufBmp) DeleteObject(st->hBotBufBmp);
        st->hBotBufBmp = CreateCompatibleBitmap(hdc, w, botStatusH);
        st->botBufW = w;
        st->botBufH = botStatusH;
        SelectObject(st->hBotBufDc, st->hBotBufBmp);
    }
    HDC mem = st->hTopBufDc;
    HDC memBot = st->hBotBufDc;

    {
        Gdiplus::Graphics g(mem);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        SetBkMode(mem, TRANSPARENT);

        // Clean flat guías de interfaz top chrome surface (ZERO Dual Kawase blur, ZERO GPU shaders)
        Gdiplus::SolidBrush topBg(UiGlass::ToGdip(pal.windowCanvasTop));
        g.FillRectangle(&topBg, 0, 0, w, topTotalH);
        Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);
        g.DrawLine(&sepPen, 0.0f, static_cast<float>(topTotalH - 1), static_cast<float>(w), static_cast<float>(topTotalH - 1));

        // Clean flat guías de interfaz rounded toolbar group containers (single prominent blue CTA)
        const float capR = static_cast<float>(S(10));
        for (const auto& cap : st->toolbarCapsules) {
            if (!cap.visible) continue;
            Gdiplus::GraphicsPath cp;
            UiGlass::AddRoundPath(cp,
                static_cast<float>(cap.rc.left), static_cast<float>(cap.rc.top),
                static_cast<float>(cap.rc.right - cap.rc.left), static_cast<float>(cap.rc.bottom - cap.rc.top),
                capR);
            if (cap.prominent) {
                Gdiplus::SolidBrush pbr(UiGlass::ToGdip(pal.accentBlue));
                g.FillPath(&pbr, &cp);
            } else {
                Gdiplus::SolidBrush fbr(UiGlass::ToGdip(pal.contentCard));
                g.FillPath(&fbr, &cp);
                Gdiplus::Pen bpen(UiGlass::ToGdip(st->glassSettings.IsHighContrast() ? pal.borderStrong : pal.borderSubtle), 1.0f);
                g.DrawPath(&bpen, &cp);
            }
        }

        // Crisp 100% vector SF-Symbol-style icons, ink swatches, and labels
        for (std::size_t i = 0; i < st->buttons.size() && i < st->buttonRects.size(); ++i) {
            const auto& b = st->buttons[i];
            if (!b.visible || b.zone == ToolbarZone::StatusZoom) continue;
            const RECT& br = st->buttonRects[i];
            if (br.right <= br.left || br.bottom <= br.top) continue;
            const bool checked = IsToolbarButtonChecked(st, b.id);
            const bool hovered = (static_cast<int>(i) == st->hoveredButton);

            if (b.isSwatch) {
                const float cx = static_cast<float>(br.left + br.right) * 0.5f;
                const float cy = static_cast<float>(br.top + br.bottom) * 0.5f;
                const float rad = static_cast<float>(S(6));
                Gdiplus::SolidBrush swBr(Gdiplus::Color(255, GetRValue(b.swatchColor), GetGValue(b.swatchColor), GetBValue(b.swatchColor)));
                g.FillEllipse(&swBr, cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
                if (checked) {
                    Gdiplus::Pen ringP(UiGlass::ToGdip(pal.accentBlue), 1.8f);
                    g.DrawEllipse(&ringP, cx - rad - 2.2f, cy - rad - 2.2f, (rad + 2.2f) * 2.0f, (rad + 2.2f) * 2.0f);
                } else {
                    Gdiplus::Pen ringP(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                    g.DrawEllipse(&ringP, cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
                }
                continue;
            }

            if (!b.accent && (checked || hovered)) {
                Gdiplus::GraphicsPath ip;
                UiGlass::AddRoundPath(ip, static_cast<float>(br.left), static_cast<float>(br.top),
                    static_cast<float>(br.right - br.left), static_cast<float>(br.bottom - br.top), static_cast<float>(S(7)));
                Gdiplus::SolidBrush ib(UiGlass::ToGdip(checked ? pal.glassPillSelected : pal.glassPillHover));
                g.FillPath(&ib, &ip);
                if (checked) {
                    Gdiplus::Pen ipen(UiGlass::ToGdip(pal.accentBlue, st->glassSettings.IsHighContrast() ? 240 : 125), 1.0f);
                    g.DrawPath(&ipen, &ip);
                }
            }

            const UiGlass::Rgba8 fgRgba = b.accent
                ? UiGlass::Rgba8{ 255, 255, 255, 255 }
                : (checked ? pal.accentBlue : (hovered ? pal.labelPrimary : pal.labelSecondary));

            const float cy = static_cast<float>(br.top + br.bottom) * 0.5f;
            if (b.hasIcon && !b.hasLabel) {
                const float cx = static_cast<float>(br.left + br.right) * 0.5f;
                UiGlass::DrawVectorIconGdiPlus(g, b.icon, cx, cy, static_cast<float>(S(15)), UiGlass::ToGdip(fgRgba), 1.55f);
            } else if (b.hasIcon && b.hasLabel) {
                UiGlass::DrawVectorIconGdiPlus(g, b.icon, static_cast<float>(br.left + S(13)), cy, static_cast<float>(S(13)), UiGlass::ToGdip(fgRgba), 1.5f);
                SelectObject(mem, (b.accent || checked) && st->hUiBoldFont ? st->hUiBoldFont : (st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont));
                SetTextColor(mem, fgRgba.ToColorRef());
                RECT tr{ br.left + S(23), br.top, br.right - S(5), br.bottom };
                DrawTextW(mem, b.label, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            } else if (b.hasLabel) {
                SelectObject(mem, (b.accent || checked) && st->hUiBoldFont ? st->hUiBoldFont : st->hUiFont);
                SetTextColor(mem, fgRgba.ToColorRef());
                RECT tr = br;
                DrawTextW(mem, b.label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // Optional Document Tabs Strip
        if (tabStripH > 0 && !st->docTabRects.empty()) {
            for (std::size_t ti = 0; ti < st->docTabRects.size() && ti < st->tabs.size(); ++ti) {
                const RECT& tr = st->docTabRects[ti];
                const bool isAct = (ti == st->activeTabIdx);
                Gdiplus::GraphicsPath tp;
                UiGlass::AddRoundPath(tp, static_cast<float>(tr.left), static_cast<float>(tr.top),
                    static_cast<float>(tr.right - tr.left), static_cast<float>(tr.bottom - tr.top), static_cast<float>(S(6)));
                Gdiplus::SolidBrush tbr(UiGlass::ToGdip(isAct ? pal.accentBlue : pal.contentCard));
                g.FillPath(&tbr, &tp);
                Gdiplus::Pen tpn(UiGlass::ToGdip(isAct ? pal.accentBlue : pal.borderSubtle), 1.0f);
                g.DrawPath(&tpn, &tp);
                SelectObject(mem, isAct && st->hUiBoldFont ? st->hUiBoldFont : (st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont));
                SetTextColor(mem, isAct ? RGB(255, 255, 255) : pal.labelPrimary.ToColorRef());
                RECT textRc{ tr.left + S(8), tr.top, tr.right - S(8), tr.bottom };
                const std::wstring tabTitle = Utf8ToWide(st->tabs[ti].title.empty() ? ("Cuaderno " + std::to_string(ti + 1)) : st->tabs[ti].title);
                DrawTextW(mem, tabTitle.c_str(), -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            }
        }

        // Optional Search Action Buttons + HIG rounded input chrome
        if (st->searchVisible && st->hwndSearchEdit) {
            RECT seRc{};
            GetWindowRect(st->hwndSearchEdit, &seRc);
            MapWindowPoints(HWND_DESKTOP, st->hwndMain, reinterpret_cast<LPPOINT>(&seRc), 2);
            RECT frameRc{ seRc.left - S(6), seRc.top - S(3), seRc.right + S(6), seRc.bottom + S(3) };
            {
                Gdiplus::GraphicsPath sp;
                UiGlass::AddRoundPath(sp, static_cast<float>(frameRc.left), static_cast<float>(frameRc.top),
                    static_cast<float>(frameRc.right - frameRc.left), static_cast<float>(frameRc.bottom - frameRc.top), static_cast<float>(S(6)));
                Gdiplus::SolidBrush sbr(UiGlass::ToGdip(pal.contentCard));
                g.FillPath(&sbr, &sp);
                Gdiplus::Pen spn(UiGlass::ToGdip(pal.borderStrong), 1.0f);
                g.DrawPath(&spn, &sp);
            }
            RECT bFind{ seRc.right + S(10), topChromeH + tabStripH + S(7), seRc.right + S(52), topChromeH + tabStripH + S(31) };
            RECT bRepl{ seRc.right + S(58), topChromeH + tabStripH + S(7), seRc.right + S(108), topChromeH + tabStripH + S(31) };
            auto drawMini = [&](const RECT& r, const wchar_t* t) {
                Gdiplus::GraphicsPath p;
                UiGlass::AddRoundPath(p, static_cast<float>(r.left), static_cast<float>(r.top),
                    static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top), static_cast<float>(S(6)));
                Gdiplus::SolidBrush br(UiGlass::ToGdip(pal.contentCard));
                g.FillPath(&br, &p);
                Gdiplus::Pen pn(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                g.DrawPath(&pn, &p);
                SelectObject(mem, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
                SetTextColor(mem, pal.labelPrimary.ToColorRef());
                RECT tx = r;
                DrawTextW(mem, t, -1, &tx, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            };
            drawMini(bFind, L"Ir");
            drawMini(bRepl, L"Reemp.");
        }
    }

    // Bottom Minimal Interactive Status Strip (dedicated persistent strip buffer)
    const int botYOff = h - botStatusH;
    {
        Gdiplus::Graphics gb(memBot);
        gb.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        SetBkMode(memBot, TRANSPARENT);
        Gdiplus::Pen sepPenB(UiGlass::ToGdip(pal.separator), 1.0f);

        Gdiplus::SolidBrush stBr(UiGlass::ToGdip(pal.sidebarBg));
        gb.FillRectangle(&stBr, 0, 0, w, botStatusH);
        gb.DrawLine(&sepPenB, 0.0f, 0.0f, static_cast<float>(w), 0.0f);

        // Interactive status micro-pills on the right (before zoom stepper)
        st->statusHits.clear();
        int rightCursor = w - S(120);
        const int pillY0 = S(4);
        const int pillY1 = botStatusH - S(4);
        SelectObject(memBot, st->hUiSmallFont ? st->hUiSmallFont : st->hUiFont);
        const std::uint8_t pillTintA = st->glassSettings.IsHighContrast() ? 16u : (st->glassSettings.IsDark() ? 46u : 20u);

        auto drawStatusPill = [&](const std::wstring& txt, WORD cmd, UiGlass::Rgba8 accent, bool tinted) {
            SIZE tsz{};
            GetTextExtentPoint32W(memBot, txt.c_str(), static_cast<int>(txt.size()), &tsz);
            const int pw = tsz.cx + S(16);
            if (rightCursor - pw < S(260)) return;
            RECT prLocal{ rightCursor - pw, pillY0, rightCursor, pillY1 };
            RECT prClient{ prLocal.left, prLocal.top + botYOff, prLocal.right, prLocal.bottom + botYOff };
            st->statusHits.push_back({ prClient, cmd, -1 });
            Gdiplus::GraphicsPath pp;
            UiGlass::AddRoundPath(pp, static_cast<float>(prLocal.left), static_cast<float>(prLocal.top),
                static_cast<float>(prLocal.right - prLocal.left), static_cast<float>(prLocal.bottom - prLocal.top), static_cast<float>(S(6)));
            Gdiplus::SolidBrush pb(tinted
                ? UiGlass::ToGdip(accent, pillTintA)
                : UiGlass::ToGdip(pal.contentCard));
            gb.FillPath(&pb, &pp);
            Gdiplus::Pen pn(UiGlass::ToGdip(tinted ? accent : pal.borderSubtle,
                tinted ? (st->glassSettings.IsHighContrast() ? 225 : 135) : 255), 1.0f);
            gb.DrawPath(&pn, &pp);
            SetTextColor(memBot, tinted ? accent.ToColorRef() : pal.labelSecondary.ToColorRef());
            DrawTextW(memBot, txt.c_str(), -1, &prLocal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            rightCursor = prLocal.left - S(6);
        };

        drawStatusPill(L"Ctrl+K", CMD_COMMAND_PALETTE, pal.accentBlue, false);
        const bool isSwGpu = !st->renderer.IsHardwareAccelerationEnabled();
        drawStatusPill(isSwGpu ? L"GPU 0%" : L"GPU HW", CMD_TOGGLE_GPU_ACCEL, isSwGpu ? pal.accentGreen : pal.accentOrange, true);
        {
            const auto* actP = st->aiVault.FindProvider(st->aiVault.ActiveProviderId());
            const std::wstring aiTxt = (st->aiVault.IsEnabled() && actP)
                ? Utf8ToWide("IA: " + actP->displayNameUtf8.substr(0, 14))
                : L"IA: Local";
            drawStatusPill(aiTxt, CMD_AI_PROVIDERS_VAULT, pal.accentPurple, st->aiVault.IsEnabled());
        }
        {
            const std::wstring layTxt = L"Capa #" + std::to_wstring(st->doc.ActiveLayerId());
            drawStatusPill(layTxt, CMD_LAYERS_MANAGER, pal.accentIndigo, false);
        }

        std::wostringstream status;
        status << (st->doc.Mode() == DocumentMode::InfiniteCanvas ? L"Lienzo Infinito" : L"PDF Paginado");
        if (st->doc.Mode() == DocumentMode::InfiniteCanvas) {
            status << L"  \u00B7  " << st->doc.CanvasQuadtreeSize() << L" objetos";
        } else {
            status << L"  \u00B7  P\u00E1g. " << (st->activePageIndex + 1) << L"/" << (std::max<std::size_t>)(1u, st->doc.Pages().size());
        }
        status << L"  \u00B7  " << Utf8ToWide(st->statusBannerUtf8);

        SetTextColor(memBot, pal.labelSecondary.ToColorRef());
        RECT stTxt{ S(14), S(2), (std::max)(S(120), rightCursor - S(8)), botStatusH - S(2) };
        const std::wstring sStr = status.str();
        DrawTextW(memBot, sStr.c_str(), static_cast<int>(sStr.size()), &stTxt, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

        // Bottom-right zoom stepper pill (- 100% +)
        const std::wstring zStr = std::to_wstring(static_cast<int>(std::lround(st->camera.zoom * 100.0))) + L"%";
        for (std::size_t i = 0; i < st->buttons.size() && i < st->buttonRects.size(); ++i) {
            const auto& b = st->buttons[i];
            if (b.zone != ToolbarZone::StatusZoom) continue;
            RECT br = st->buttonRects[i];
            br.top -= botYOff;
            br.bottom -= botYOff;
            const bool hov = (static_cast<int>(i) == st->hoveredButton);
            Gdiplus::GraphicsPath zp;
            UiGlass::AddRoundPath(zp, static_cast<float>(br.left), static_cast<float>(br.top),
                static_cast<float>(br.right - br.left), static_cast<float>(br.bottom - br.top), static_cast<float>(S(6)));
            Gdiplus::SolidBrush zb(UiGlass::ToGdip(hov ? pal.glassPillHover : pal.contentCard));
            gb.FillPath(&zb, &zp);
            Gdiplus::Pen zpen(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
            gb.DrawPath(&zpen, &zp);
            if (b.id == CMD_ZOOM_RESET) {
                SetTextColor(memBot, pal.labelPrimary.ToColorRef());
                RECT tr = br;
                DrawTextW(memBot, zStr.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            } else {
                UiGlass::DrawVectorIconGdiPlus(gb, b.icon,
                    static_cast<float>(br.left + br.right) * 0.5f,
                    static_cast<float>(br.top + br.bottom) * 0.5f,
                    static_cast<float>(S(11)), UiGlass::ToGdip(pal.labelPrimary), 1.5f);
            }
        }
    }

    BitBlt(hdc, 0, 0, w, topTotalH, mem, 0, 0, SRCCOPY);
    BitBlt(hdc, 0, botYOff, w, botStatusH, memBot, 0, 0, SRCCOPY);
    if (sideRail) {
        const bool allowSidebars = (w >= S(840)) && !st->focusMode;
        const int sideW = (allowSidebars && st->sidebarVisible) ? S(kSidebarWidth) : 0;
        const int inspW = (allowSidebars && st->inspectorVisible) ? S(kInspectorWidth) : 0;
        const int railW = S(44);
        const int railX = (st->uiPrefs.toolbarOrientation == 1) ? sideW : (w - inspW - railW);
        BitBlt(hdc, railX, topTotalH, railW, (std::max)(1, h - topTotalH - botStatusH), mem, railX, topTotalH, SRCCOPY);
    }
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    StudioAppState* st = reinterpret_cast<StudioAppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<StudioAppState*>(cs->lpCreateParams);
        st->hwndMain = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
            using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
            if (auto pGetDpi = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(hUser32, "GetDpiForWindow"))) {
                const UINT wndDpi = pGetDpi(hwnd);
                if (wndDpi >= 72) st->currentDpi = wndDpi;
            }
        }
        RebuildScaledUiFonts(st);
        UiGlass::ApplyWindowsNativeMicaBackdrop(hwnd, st->glassSettings.IsDark(), st->glassSettings.reduceTransparency);
        RebuildToolbarButtons(st);

        st->hwndSearchEdit = CreateWindowExW(0, L"EDIT", L"",
            WS_CHILD | ES_AUTOHSCROLL, 740, kTopChromeH + 10, 180, 18, hwnd,
            reinterpret_cast<HMENU>(CMD_SEARCH_EDIT), cs->hInstance, nullptr);
        SendMessageW(st->hwndSearchEdit, WM_SETFONT, reinterpret_cast<WPARAM>(st->hUiFont), TRUE);
        SendMessageW(st->hwndSearchEdit, EM_SETCUEBANNER, FALSE, reinterpret_cast<LPARAM>(L"Buscar texto... (Ctrl+F)"));

        st->hwndSidebar = CreateWindowW(kSidebarClassName, L"", WS_CHILD | WS_VISIBLE,
            0, kTopChromeH, kSidebarWidth, 500, hwnd, nullptr, cs->hInstance, st);
        st->hwndInspector = CreateWindowW(kInspectorClassName, L"", WS_CHILD,
            900, kTopChromeH, kInspectorWidth, 500, hwnd, nullptr, cs->hInstance, st);
        st->hwndViewport = CreateWindowW(kViewportClassName, L"", WS_CHILD | WS_VISIBLE,
            kSidebarWidth, kTopChromeH, 800, 500, hwnd, nullptr, cs->hInstance, st);

        st->renderer.AttachHwnd(st->hwndViewport, 800, 500);
        LoadStudioProfileAtStartup(st);
        RebuildScaledUiFonts(st);
        DragAcceptFiles(hwnd, TRUE);
        StartCasioInboxWatcher(st); // Sleeping Win32 directory notification watcher (0.0% idle CPU; zero polling!)
        SyncDynamicTimers(st);
        LayoutMainChildren(st);
        CenterCameraOnPage(st, 0);
        UpdateWindowTitle(st);
        if (!IsAutomationRun(st)) {
            PruneOldDrafts();
            PostMessageW(hwnd, WM_APP_DRAFT_RECOVERY, 0, 0); // ya con la ventana a la vista
        }
        return 0;
    }
    case WM_APP_DRAFT_RECOVERY:
        if (st) OfferDraftsFromPreviousSession(st);
        return 0;
    case WM_CLOSE:
        // Antes la ventana se cerraba sin preguntar y un documento nuevo se perdía entero.
        if (st && !ConfirmUnsavedChanges(st, L"cerrar CuadernU")) return 0;
        if (st) {
            KeepInactiveTabsAsDrafts(st);
            DiscardSessionDraft(st);
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_QUERYENDSESSION:
        // Windows se apaga o reinicia: no se puede preguntar. Queda una copia que CuadernU
        // ofrece abrir la próxima vez.
        if (st && !IsAutomationRun(st)) {
            CommitInlineTextEdit(st, true);
            AutosaveSessionDraft(st);
            KeepInactiveTabsAsDrafts(st);
        }
        return TRUE;
    case WM_SIZE:
        if (st) {
            if (wp == SIZE_MINIMIZED) {
                st->isMinimized = true;
                st->videoPlayer.Pause();
                if (st->replayActive) {
                    st->replayActive = false;
                    KillTimer(hwnd, kTimerIdReplay);
                    st->audioSession.StopAudioPlayback();
                }
                if (st->activeHtmlObjectId != 0) {
                    st->htmlHost.SuspendAll();
                    st->activeHtmlObjectId = 0;
                }
                SyncDynamicTimers(st);
                TrimApplicationMemoryNow(st, true);
                return 0;
            }
            if (st->isMinimized) {
                st->isMinimized = false;
                SyncDynamicTimers(st);
            }
            LayoutMainChildren(st);
        }
        return 0;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        if (st) {
            UiGlass::ApplyWindowsNativeMicaBackdrop(hwnd, st->glassSettings.IsDark(), st->glassSettings.reduceTransparency);
            InvalidateViews(st);
        }
        return 0;
    case WM_DPICHANGED: {
        if (!st) break;
        const UINT newDpi = HIWORD(wp) ? HIWORD(wp) : (LOWORD(wp) ? LOWORD(wp) : 96u);
        if (newDpi >= 72) {
            st->currentDpi = newDpi;
            st->dpiCycleTransitions++;
            RebuildScaledUiFonts(st);
        }
        const RECT* suggested = reinterpret_cast<const RECT*>(lp);
        if (suggested) {
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                suggested->right - suggested->left, suggested->bottom - suggested->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
        }
        LayoutMainChildren(st);
        InvalidateViews(st);
        return 0;
    }
    case WM_DROPFILES: {
        if (!st) break;
        HDROP hDrop = reinterpret_cast<HDROP>(wp);
        wchar_t path[4096]{};
        if (DragQueryFileW(hDrop, 0, path, 4096) > 0) {
            const std::wstring ext = GetFileExtLower(path);
            const bool isPrintoutExt = (ext == L".pdf" || ext == L".docx" || ext == L".pptx" ||
                                        ext == L".xlsx" || ext == L".doc" || ext == L".ppt" ||
                                        ext == L".xls" || ext == L".csv");
            if (isPrintoutExt && st->doc.Mode() == DocumentMode::InfiniteCanvas) {
                POINT dpt{ 120, 120 };
                DragQueryPoint(hDrop, &dpt);
                const DocPoint wpt = st->camera.ScreenToWorld(dpt.x, dpt.y);
                DragFinish(hDrop);
                if (GuiDialogs::RunInsertPrintoutDialog(
                        st->hwndMain, st->doc, st->cmdStack, st->activePageIndex, wpt, st->statusBannerUtf8, path)) {
                    st->pageCache.InvalidateAll(++st->asyncGen);
                    LayoutMainChildren(st);
                    UpdateWindowTitle(st);
                    InvalidateViews(st);
                }
                return 0;
            }
            OpenDocumentFile(st, path);
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!st) break;
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (st->draggingOpacitySlider && st->appearancePopoverOpen) {
            UpdateAppearanceFromPoint(st, pt, true);
            return 0;
        }
        int hit = -1;
        for (std::size_t i = 0; i < st->buttonRects.size() && i < st->buttons.size(); ++i) {
            if (st->buttons[i].visible && PtInRect(&st->buttonRects[i], pt)) { hit = static_cast<int>(i); break; }
        }
        if (hit != st->hoveredButton) {
            st->hoveredButton = hit;
            InvalidateRibbonOnly(st);
            InvalidateStatusOnly(st);
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        if (st && reinterpret_cast<HWND>(lp) == st->hwndSearchEdit) {
            const UiGlass::GlassPalette pal = UiGlass::GetPalette(st->glassSettings);
            HDC hdcE = reinterpret_cast<HDC>(wp);
            SetTextColor(hdcE, pal.labelPrimary.ToColorRef());
            SetBkColor(hdcE, pal.contentCard.ToColorRef());
            if (st->hSearchBgBrush) DeleteObject(st->hSearchBgBrush);
            st->hSearchBgBrush = CreateSolidBrush(pal.contentCard.ToColorRef());
            return reinterpret_cast<LRESULT>(st->hSearchBgBrush);
        }
        break;
    }
    case WM_CAPTURECHANGED: {
        if (st) {
            st->draggingOpacitySlider = false;
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (st && st->draggingOpacitySlider) {
            st->draggingOpacitySlider = false;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        if (!st) break;
        const POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        if (st->appearancePopoverOpen && PtInRect(&st->appearancePopoverRc, pt)) {
            if (pt.x >= st->opacityTrackRc.left - 8 && pt.x <= st->opacityTrackRc.right + 8 &&
                pt.y >= st->opacityTrackRc.top - 6 && pt.y <= st->opacityTrackRc.bottom + 6) {
                st->draggingOpacitySlider = true;
                SetCapture(hwnd);
            }
            UpdateAppearanceFromPoint(st, pt, false);
            return 0;
        }
        for (std::size_t ti = 0; ti < st->docTabRects.size() && ti < st->tabs.size(); ++ti) {
            if (PtInRect(&st->docTabRects[ti], pt)) {
                SwitchToDocTab(st, ti);
                return 0;
            }
        }
        for (std::size_t i = 0; i < st->buttonRects.size() && i < st->buttons.size(); ++i) {
            if (st->buttons[i].visible && PtInRect(&st->buttonRects[i], pt)) {
                if (st->buttons[i].id != CMD_TOGGLE_APPEARANCE) st->appearancePopoverOpen = false;
                ExecuteToolbarCommand(st, st->buttons[i].id);
                return 0;
            }
        }
        for (const auto& sh : st->statusHits) {
            if (sh.cmdId != 0 && PtInRect(&sh.rc, pt)) {
                st->appearancePopoverOpen = false;
                ExecuteToolbarCommand(st, sh.cmdId);
                return 0;
            }
        }
        if (st->appearancePopoverOpen) {
            st->appearancePopoverOpen = false;
            InvalidateViews(st);
        }
        if (st->searchVisible && st->hwndSearchEdit) {
            RECT seRc{};
            GetWindowRect(st->hwndSearchEdit, &seRc);
            MapWindowPoints(HWND_DESKTOP, hwnd, reinterpret_cast<LPPOINT>(&seRc), 2);
            const int topC = DpiSc(kTopChromeH, st->currentDpi) + (st->tabs.size() > 1 ? DpiSc(28, st->currentDpi) : 0);
            RECT bFind{ seRc.right + DpiSc(10, st->currentDpi), topC + DpiSc(7, st->currentDpi),
                        seRc.right + DpiSc(52, st->currentDpi), topC + DpiSc(31, st->currentDpi) };
            RECT bRepl{ seRc.right + DpiSc(58, st->currentDpi), topC + DpiSc(7, st->currentDpi),
                        seRc.right + DpiSc(108, st->currentDpi), topC + DpiSc(31, st->currentDpi) };
            if (PtInRect(&bFind, pt)) { ExecuteToolbarCommand(st, CMD_SEARCH_NEXT); return 0; }
            if (PtInRect(&bRepl, pt)) { ExecuteToolbarCommand(st, CMD_REPLACE_TEXT); return 0; }
        }
        return 0;
    }
    case WM_KEYDOWN: {
        if (!st) break;
        const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool inSearchEdit = (st->hwndSearchEdit && GetFocus() == st->hwndSearchEdit);
        const bool inInlineEdit = (st->hwndInlineEdit && GetFocus() == st->hwndInlineEdit);
        if (wp == VK_ESCAPE && inInlineEdit) { CommitInlineTextEdit(st, false); return 0; }
        if (wp == VK_ESCAPE && (st->appearancePopoverOpen || st->graphOverlayVisible || st->dbPanelVisible || st->commentsPanelVisible || st->readingViewMode)) {
            st->appearancePopoverOpen = false;
            st->graphOverlayVisible = false;
            st->dbPanelVisible = false;
            st->commentsPanelVisible = false;
            st->readingViewMode = false;
            InvalidateViews(st);
            return 0;
        }
        // Custom user-configured KeymapProfile shortcut dispatch (F-257)
        if (!inSearchEdit && !inInlineEdit) {
            std::string pressed;
            if (ctrl) pressed += "Ctrl+";
            if (shift) pressed += "Shift+";
            if ((GetKeyState(VK_MENU) & 0x8000) != 0) pressed += "Alt+";
            if (wp >= 'A' && wp <= 'Z') pressed.push_back(static_cast<char>(wp));
            else if (wp >= '0' && wp <= '9') pressed.push_back(static_cast<char>(wp));
            else if (wp == VK_F11) pressed += "F11";
            else if (wp == VK_OEM_PLUS) pressed += "=";
            else if (wp == VK_OEM_MINUS) pressed += "-";
            if (!pressed.empty()) {
                for (const auto& kv : st->keymapProfile.actionToShortcut) {
                    if (kv.second != pressed) continue;
                    WORD mapped = 0;
                    if (kv.first == "view.command_palette") mapped = CMD_COMMAND_PALETTE;
                    else if (kv.first == "view.focus_mode") mapped = CMD_FOCUS_MODE;
                    else if (kv.first == "tool.hand") mapped = CMD_TOOL_HAND;
                    else if (kv.first == "tool.pen") mapped = CMD_TOOL_PEN;
                    else if (kv.first == "tool.highlighter") mapped = CMD_TOOL_HL;
                    else if (kv.first == "tool.eraser") mapped = CMD_TOOL_ERASER;
                    else if (kv.first == "tool.lasso") mapped = CMD_TOOL_LASSO;
                    else if (kv.first == "tool.laser") mapped = CMD_TOOL_LASER;
                    else if (kv.first == "tool.textbox") mapped = CMD_TOOL_TEXT;
                    else if (kv.first == "tool.shape_rect") mapped = CMD_TOOL_RECT;
                    else if (kv.first == "tool.shape_arrow") mapped = CMD_TOOL_ARROW;
                    else if (kv.first == "stem.math_block") mapped = CMD_ADD_FORMULA;
                    else if (kv.first == "stem.circuit_block") mapped = CMD_ADD_CIRCUIT;
                    else if (kv.first == "stem.plot_block") mapped = CMD_ADD_PLOT;
                    else if (kv.first == "study.flashcards") mapped = CMD_STUDY_DECK;
                    else if (kv.first == "study.audio_record") mapped = CMD_AUDIO_REC;
                    else if (kv.first == "view.zoom_in") mapped = CMD_ZOOM_IN;
                    else if (kv.first == "view.zoom_out") mapped = CMD_ZOOM_OUT;
                    else if (kv.first == "view.zoom_fit") mapped = CMD_ZOOM_RESET;
                    if (mapped != 0) {
                        ExecuteToolbarCommand(st, mapped);
                        return 0;
                    }
                }
            }
        }
        if (ctrl && wp == 'S') { ExecuteToolbarCommand(st, CMD_SAVE_CUDOC); return 0; }
        if (ctrl && wp == 'O') { ExecuteToolbarCommand(st, CMD_OPEN); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'Z') { ExecuteToolbarCommand(st, CMD_UNDO); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'Y') { ExecuteToolbarCommand(st, CMD_REDO); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'V') { ExecuteToolbarCommand(st, CMD_CLIPBOARD_PASTE); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'X') { ExecuteToolbarCommand(st, CMD_CLIPBOARD_CUT); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'C') {
            if (shift || (st->selectedObjectId == 0 && st->lassoSelectedIds.empty() && st->hasLastSnipRegion)) {
                ExecuteToolbarCommand(st, CMD_SNIP_REGION_AS_IMAGE);
            } else {
                ExecuteToolbarCommand(st, CMD_CLIPBOARD_COPY);
            }
            return 0;
        }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'G') { ExecuteToolbarCommand(st, shift ? CMD_UNGROUP_OBJECTS : CMD_GROUP_OBJECTS); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'D') { ExecuteToolbarCommand(st, CMD_DUPLICATE_OBJECTS); return 0; }
        if (ctrl && shift && wp == 'T') { ExecuteToolbarCommand(st, CMD_ACADEMIC_SYNC_TEC); return 0; }
        if (ctrl && shift && wp == 'H') { ExecuteToolbarCommand(st, CMD_ACADEMIC_SOLVE_SCHEDULE); return 0; }
        if (ctrl && wp == 'T') { ExecuteToolbarCommand(st, CMD_NEW_TAB); return 0; }
        if (ctrl && wp == VK_TAB) { ExecuteToolbarCommand(st, CMD_NEXT_TAB); return 0; }
        if (ctrl && shift && wp == 'N') { ExecuteToolbarCommand(st, CMD_QUICK_NOTE); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'B') { ExecuteToolbarCommand(st, CMD_TEXT_BOLD); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'I') { ExecuteToolbarCommand(st, CMD_TEXT_ITALIC); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'U') { ExecuteToolbarCommand(st, CMD_TEXT_UNDERLINE); return 0; }
        if (ctrl && wp == 'F') {
            if (!st->searchVisible) ExecuteToolbarCommand(st, CMD_TOGGLE_SEARCH);
            else if (st->hwndSearchEdit) SetFocus(st->hwndSearchEdit);
            return 0;
        }
        if (ctrl && wp == 'P') { ExecuteToolbarCommand(st, CMD_PRINT); return 0; }
        if (ctrl && !inSearchEdit && !inInlineEdit && wp == 'K') { ExecuteToolbarCommand(st, CMD_COMMAND_PALETTE); return 0; }
        if (wp == VK_F3) { ExecuteToolbarCommand(st, CMD_SEARCH_NEXT); return 0; }
        if (!inSearchEdit && !inInlineEdit && (wp == VK_DELETE || wp == VK_BACK)) {
            if (!st->lassoSelectedIds.empty()) {
                st->cmdStack.Execute(st->doc, std::make_unique<BatchEraseObjectsCommand>(
                    ObjectTarget{ st->selectedIsCanvas, st->selectedPageIndex }, st->lassoSelectedIds));
                st->lassoSelectedIds.clear();
                st->selectedObjectId = 0;
                st->statusBannerUtf8 = "Selecci\xC3\xB3n Lazo eliminada";
                UpdateWindowTitle(st);
                InvalidateViews(st);
                return 0;
            }
            if (st->selectedObjectId != 0) {
                st->cmdStack.Execute(st->doc, std::make_unique<RemoveObjectCommand>(
                    ObjectTarget{ st->selectedIsCanvas, st->selectedPageIndex }, st->selectedObjectId));
                st->selectedObjectId = 0;
                st->statusBannerUtf8 = "Objeto eliminado";
                UpdateWindowTitle(st);
                InvalidateViews(st);
                return 0;
            }
        }
        break;
    }
    case WM_TIMER: {
        if (!st) break;
        if (wp == kTimerIdDraftAutosave) {
            AutosaveSessionDraft(st);
            return 0;
        }
        if (wp == kTimerIdGif) {
            st->animationTick++;
            InvalidateViewportOnly(st);
            return 0;
        }
        if (wp == kTimerIdPomodoro) {
            if (st->studyTimerRunning) {
                const ULONGLONG nowMs = GetTickCount64();
                if (st->lastStudyClockMs == 0) {
                    st->lastStudyClockMs = nowMs;
                } else if (nowMs >= st->lastStudyClockMs + 1000ull) {
                    const ULONGLONG elapsedSec = (nowMs - st->lastStudyClockMs) / 1000ull;
                    st->uiPrefs.totalStudySeconds += elapsedSec;
                    st->lastStudyClockMs += elapsedSec * 1000ull;
                }
            }
            if (st->pomodoro.running) {
                std::uint64_t addedPomoSec = 0;
                bool phaseChanged = false;
                if (st->pomodoro.Tick(GetTickCount64(), addedPomoSec, &phaseChanged)) {
                    st->uiPrefs.totalStudySeconds += addedPomoSec;
                    if (phaseChanged) {
                        st->statusBannerUtf8 = st->pomodoro.inBreakPhase
                            ? ("Pomodoro completado (#" + std::to_string(st->pomodoro.completedCycles) + ") \xE2\x80\x94 tiempo de pausa")
                            : "Pausa finalizada \xE2\x80\x94 nuevo bloque de foco";
                        if (st->pomodoro.inBreakPhase && st->focusMode) {
                            st->focusMode = false;
                            LayoutMainChildren(st);
                        }
                    }
                    InvalidateViewportOnly(st);
                    InvalidateStatusOnly(st);
                }
            }
            SyncDynamicTimers(st);
            return 0;
        }
        if (wp == kTimerIdVideo) {
            if (st->videoPlayer.IsPlaying()) {
                if (st->videoPlayer.TickAndDecodeFrame()) {
                    InvalidateViewportOnly(st);
                }
                if (!st->videoPlayer.IsPlaying()) {
                    st->statusBannerUtf8 = "Reproducci\xC3\xB3n de video finalizada (0% CPU/GPU)";
                    InvalidateViewportOnly(st);
                    InvalidateStatusOnly(st);
                    SyncDynamicTimers(st);
                }
            } else {
                SyncDynamicTimers(st);
            }
            return 0;
        }
        if (wp == kTimerIdAutoSync) {
            if (st->uiPrefs.autoSyncEnabled && !st->uiPrefs.autoSyncFolderUtf8.empty()) {
                if (st->uiPrefs.autoSyncEncrypted && st->autoSyncPassphraseUtf8.empty()) {
                    KillTimer(hwnd, kTimerIdAutoSync);
                    st->timerSyncActive = false;
                    st->statusBannerUtf8 = "Reanude la sincronizaci\xC3\xB3n cifrada con su frase de cifrado";
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                SelectiveSyncFilter flt{};
                flt.encryptPassphraseUtf8 = st->autoSyncPassphraseUtf8;
                const auto rep = LocalFirstSyncEngine::SynchronizeDirectories(
                    StudioProfileStore::DefaultProfileRootDir(),
                    Utf8ToWide(st->uiPrefs.autoSyncFolderUtf8),
                    FolderSyncMode::MergeLossless, flt);
                if (!rep.ok) {
                    st->statusBannerUtf8 = rep.diagnosticUtf8;
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
            } else {
                SyncDynamicTimers(st);
            }
            return 0;
        }
        if (wp == kTimerIdReplay && st->replayActive) {
            if (st->audioSession.IsPlaying() && st->audioSession.DurationMs() > 0) {
                st->replayCursorMs = st->audioSession.GetHardwarePlaybackCursorMs();
            } else {
                st->replayCursorMs += 45;
                st->audioSession.AdvancePlaybackMs(45);
            }
            if (st->replayCursorMs >= st->replayMaxMs) {
                st->replayActive = false;
                KillTimer(hwnd, kTimerIdReplay);
                st->audioSession.StopAudioPlayback();
                st->statusBannerUtf8 = "Ink Replay completado";
            } else {
                st->statusBannerUtf8 = "Ink Replay: " + std::to_string(st->replayCursorMs) + " / " + std::to_string(st->replayMaxMs) + " ms";
            }
            InvalidateViewportOnly(st);
            InvalidateStatusOnly(st);
            return 0;
        }
        break;
    }
    case WM_APP_CASIO_INBOX_READY:
        if (st) PollCasioIpcInbox(st);
        return 0;
    case WM_APP_HTML_SNAPSHOT_READY: {
        std::unique_ptr<HtmlSnapshotPayload> payload(reinterpret_cast<HtmlSnapshotPayload*>(lp));
        if (st && payload && !payload->pngBytes.empty()) {
            const ObjectTarget tgt{ payload->isCanvas, payload->pageIndex };
            if (AnnotationObject* obj = CanvasManip::FindObjectMut(st->doc, tgt, payload->objectId)) {
                if (obj->kind == ObjectKind::HtmlEmbed) {
                    obj->htmlSnapshotPngBytes = std::move(payload->pngBytes);
                    st->doc.BumpRevision();
                    InvalidateViewportOnly(st);
                }
            }
        }
        return 0;
    }
    case WM_APP_ASYNC_PAGE_READY:
        if (st) InvalidateViews(st);
        return 0;
    case WM_APP_TAKE_SCREENSHOT: {
        if (st && st->testDpiCycleCli) {
            RECT origRc{};
            GetWindowRect(hwnd, &origRc);
            const int origW = (std::max)(800, static_cast<int>(origRc.right - origRc.left));
            const int origH = (std::max)(600, static_cast<int>(origRc.bottom - origRc.top));
            struct DpiStep { UINT dpi; int w; int h; };
            const DpiStep steps[3] = {
                { 144u, origW + 120, origH + 80 },
                { 192u, origW + 240, origH + 140 },
                { 96u,  origW,       origH }
            };
            bool ok = true;
            for (const auto& step : steps) {
                RECT sug{ origRc.left, origRc.top, origRc.left + step.w, origRc.top + step.h };
                SendMessageW(hwnd, WM_DPICHANGED, MAKEWPARAM(step.dpi, step.dpi), reinterpret_cast<LPARAM>(&sug));
                if (st->currentDpi != step.dpi) ok = false;
                RedrawWindow(hwnd, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                if (st->hwndSidebar && IsWindowVisible(st->hwndSidebar)) RedrawWindow(st->hwndSidebar, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                if (st->hwndInspector && IsWindowVisible(st->hwndInspector)) RedrawWindow(st->hwndInspector, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
                if (st->hwndViewport && IsWindowVisible(st->hwndViewport)) RedrawWindow(st->hwndViewport, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            }
            if (!ok || st->dpiCycleTransitions < 3 || st->currentDpi != 96u || !st->hUiFont || !st->hUiBoldFont) {
                DestroyWindow(hwnd);
                PostQuitMessage(2);
                return 0;
            }
            if (st->showcaseMediaCli && st->hwndViewport) {
                RECT vpRc{};
                GetClientRect(st->hwndViewport, &vpRc);
                const double zx = (static_cast<double>((std::max)(360L, vpRc.right - vpRc.left)) - 44.0) / 832.0;
                const double zy = (static_cast<double>((std::max)(280L, vpRc.bottom - vpRc.top)) - 40.0) / 530.0;
                st->camera.zoom = std::clamp((std::min)(zx, zy), 0.65, 1.35);
            }
        }
        if (st && !st->screenshotOutPath.empty()) {
            auto ensureThumb = [&](std::size_t p) {
                if (p >= st->doc.Pages().size()) return;
                const auto& pg = st->doc.Pages()[p];
                if (!pg.hasPdfSource || pg.sourcePdfPathUtf8.empty()) return;
                const PageCacheKey tkey = PageRenderCache::MakeKey(pg, 160);
                RasterImageBgra32 img;
                if (st->pageCache.TryGet(tkey, img)) return;
                NormalizedCropRect c = pg.crop;
                c.Clamp();
                if (WinRtPdfRenderer::RenderPageToBgra32(Utf8ToWide(pg.sourcePdfPathUtf8), pg.sourcePageIndex0,
                        tkey.widthBucketPx, tkey.rotationDeg, c, img, nullptr, nullptr)) {
                    st->pageCache.PutDirect(tkey, img);
                }
            };
            const std::size_t topCount = (std::min<std::size_t>)(st->doc.Pages().size(), 7u);
            for (std::size_t p = 0; p < topCount; ++p) ensureThumb(p);
            const std::size_t firstThumb = st->activePageIndex > 2 ? st->activePageIndex - 2 : 0;
            const std::size_t endThumb = (std::min)(st->doc.Pages().size(), firstThumb + 6);
            for (std::size_t p = firstThumb; p < endThumb; ++p) ensureThumb(p);
            if (st->hwndSidebar && IsWindowVisible(st->hwndSidebar)) RedrawWindow(st->hwndSidebar, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            if (st->hwndInspector && IsWindowVisible(st->hwndInspector)) RedrawWindow(st->hwndInspector, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
            CaptureStudioCombinedToPng(st, st->screenshotOutPath);
            if (st->smokeTestMode || !st->screenshotOutPath.empty()) PostMessageW(hwnd, WM_CLOSE, 0, 0);
        } else if (st && st->smokeTestMode) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (st) PaintRibbonAndStatus(st, hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        KillTimer(hwnd, kTimerIdReplay);
        KillTimer(hwnd, kTimerIdGif);
        KillTimer(hwnd, kTimerIdPomodoro);
        KillTimer(hwnd, kTimerIdVideo);
        KillTimer(hwnd, kTimerIdAutoSync);
        KillTimer(hwnd, kTimerIdDraftAutosave);
        if (st) {
            StopCasioInboxWatcher(st);
            st->videoPlayer.Stop();
            st->tutorPanel.Close();
            st->htmlHost.Shutdown();
            CommitInlineTextEdit(st, true);
            st->speechSynth.Shutdown();
            st->lanServer.Stop();
            PersistStudioProfileNow(st);
            if (st->audioSession.IsContinuousRecording()) st->audioSession.StopContinuousMicRecording();
            st->audioSession.StopAudioPlayback();
            st->projectorWnd.Close();
            st->pageCache.Shutdown();
            WinRtPdfRenderer::ClearOpenDocumentCache();
            st->renderer.DiscardDeviceResources();
            if (st->hUiFont)      { DeleteObject(st->hUiFont);      st->hUiFont = nullptr; }
            if (st->hUiBoldFont)  { DeleteObject(st->hUiBoldFont);  st->hUiBoldFont = nullptr; }
            if (st->hUiSmallFont) { DeleteObject(st->hUiSmallFont); st->hUiSmallFont = nullptr; }
            if (st->hUiTitleFont) { DeleteObject(st->hUiTitleFont); st->hUiTitleFont = nullptr; }
            if (st->hSearchBgBrush) { DeleteObject(st->hSearchBgBrush); st->hSearchBgBrush = nullptr; }
            if (st->hTopBufBmp) { DeleteObject(st->hTopBufBmp); st->hTopBufBmp = nullptr; }
            if (st->hTopBufDc)  { DeleteDC(st->hTopBufDc);      st->hTopBufDc = nullptr; }
            if (st->hBotBufBmp) { DeleteObject(st->hBotBufBmp); st->hBotBufBmp = nullptr; }
            if (st->hBotBufDc)  { DeleteDC(st->hBotBufDc);      st->hBotBufDc = nullptr; }
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ParseCliArgs(StudioAppState* st, std::wstring& outOpenPath, std::string& outMode) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        if (arg == L"--open" && i + 1 < argc) {
            outOpenPath = argv[++i];
        } else if (arg == L"--profile-dir" && i + 1 < argc) {
            ++i;
        } else if (arg == L"--page" && i + 1 < argc) {
            st->initialPage1Based = (std::max)(1, _wtoi(argv[++i]));
        } else if (arg == L"--mode" && i + 1 < argc) {
            const std::wstring m = argv[++i];
            if (m == L"canvas") outMode = "canvas";
            else if (m == L"pdf") outMode = "pdf";
        } else if (arg == L"--calc-card" && i + 1 < argc) {
            st->cliCalcCardSpec = WideToUtf8(argv[++i]);
        } else if (arg == L"--showcase-stem") {
            st->showcaseStemCli = true;
            outMode = "canvas";
        } else if (arg == L"--showcase-media") {
            st->showcaseMediaCli = true;
            outMode = "canvas";
        } else if (arg == L"--screenshot-out" && i + 1 < argc) {
            st->screenshotOutPath = argv[++i];
        } else if (arg == L"--smoke-test") {
            st->smokeTestMode = true;
        } else if (arg == L"--test-dpi-cycle") {
            st->testDpiCycleCli = true;
            st->smokeTestMode = true;
        } else if (arg == L"--theme" && i + 1 < argc) {
            const std::wstring t = argv[++i];
            if (t == L"dark") {
                st->glassSettings.mode = UiGlass::AppearanceMode::Dark;
                st->glassSettings.increaseContrast = false;
                st->glassSettings.reduceTransparency = false;
            } else if (t == L"high-contrast" || t == L"hc") {
                st->glassSettings.mode = UiGlass::AppearanceMode::HighContrast;
                st->glassSettings.increaseContrast = true;
                st->glassSettings.reduceTransparency = true;
            } else {
                st->glassSettings.mode = UiGlass::AppearanceMode::Light;
                st->glassSettings.increaseContrast = false;
                st->glassSettings.reduceTransparency = false;
            }
        } else if (arg == L"--glass-opacity" && i + 1 < argc) {
            st->glassSettings.userOpacitySlider = std::clamp(static_cast<float>(_wtof(argv[++i])), 0.0f, 1.0f);
            st->glassSettings.reduceTransparency = false;
        } else if (arg == L"--inspector" && i + 1 < argc) {
            const std::wstring tab = argv[++i];
            if (tab == L"ink") { st->inspectorVisible = true; st->inspectorTab = 0; }
            else if (tab == L"vault") { st->inspectorVisible = true; st->inspectorTab = 1; }
            else if (tab == L"stem") { st->inspectorVisible = true; st->inspectorTab = 2; }
            else if (tab == L"ai") { st->inspectorVisible = true; st->inspectorTab = 3; }
            else if (tab == L"a11y") { st->inspectorVisible = true; st->inspectorTab = 4; }
            else if (tab == L"layers" || tab == L"academic") { st->inspectorVisible = true; st->inspectorTab = 5; }
            else if (tab == L"off") { st->inspectorVisible = false; }
        } else if (arg == L"--appearance-popover") {
            st->appearancePopoverOpen = true;
        } else if (arg == L"--search-open") {
            st->searchVisible = true;
        } else if (arg == L"--tec-digital-preview" && i + 1 < argc) {
            st->cliTecPreviewSection = std::clamp(_wtoi(argv[++i]), 0, 6);
        } else if (arg == L"--tec-subview" && i + 1 < argc) {
            st->cliTecPreviewSubView = std::clamp(_wtoi(argv[++i]), 0, 4);
        } else if (arg == L"--tec-inner-tab" && i + 1 < argc) {
            st->cliTecPreviewInnerTab = std::clamp(_wtoi(argv[++i]), 0, 9);
        }
    }
    LocalFree(argv);
}

std::vector<std::uint8_t> BuildShowcaseAnimatedGifBytes() {
    std::vector<std::uint8_t> out;
    Microsoft::WRL::ComPtr<IWICImagingFactory> fac;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac)))) return out;
    Microsoft::WRL::ComPtr<IStream> memStream;
    if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &memStream))) return out;
    Microsoft::WRL::ComPtr<IWICBitmapEncoder> enc;
    if (FAILED(fac->CreateEncoder(GUID_ContainerFormatGif, nullptr, &enc))) return out;
    if (FAILED(enc->Initialize(memStream.Get(), WICBitmapEncoderNoCache))) return out;

    const UINT gw = 160, gh = 110;
    for (int f = 0; f < 2; ++f) {
        std::vector<std::uint8_t> px(static_cast<std::size_t>(gw) * gh * 4u);
        for (UINT y = 0; y < gh; ++y) {
            for (UINT x = 0; x < gw; ++x) {
                const double wave = std::sin(static_cast<double>(x) * 0.08 + f * 1.4) * 28.0 + gh * 0.5;
                const bool onWave = std::abs(static_cast<double>(y) - wave) < 5.0;
                const std::size_t idx = (static_cast<std::size_t>(y) * gw + x) * 4u;
                px[idx + 0] = onWave ? (f == 0 ? 250 : 120) : 42;
                px[idx + 1] = onWave ? (f == 0 ? 185 : 235) : 28;
                px[idx + 2] = onWave ? (f == 0 ? 56  : 90)  : 20;
                px[idx + 3] = 255;
            }
        }
        Microsoft::WRL::ComPtr<IWICBitmap> srcBmp;
        if (FAILED(fac->CreateBitmapFromMemory(gw, gh, GUID_WICPixelFormat32bppBGRA, gw * 4u, static_cast<UINT>(px.size()), px.data(), &srcBmp))) return out;
        Microsoft::WRL::ComPtr<IWICPalette> pal;
        if (FAILED(fac->CreatePalette(&pal))) return out;
        if (FAILED(pal->InitializeFromBitmap(srcBmp.Get(), 256, FALSE))) return out;
        Microsoft::WRL::ComPtr<IWICFormatConverter> conv;
        if (FAILED(fac->CreateFormatConverter(&conv))) return out;
        if (FAILED(conv->Initialize(srcBmp.Get(), GUID_WICPixelFormat8bppIndexed, WICBitmapDitherTypeNone, pal.Get(), 0.0, WICBitmapPaletteTypeCustom))) return out;

        Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frame;
        if (FAILED(enc->CreateNewFrame(&frame, nullptr))) return out;
        if (FAILED(frame->Initialize(nullptr))) return out;
        frame->SetSize(gw, gh);
        WICPixelFormatGUID pf = GUID_WICPixelFormat8bppIndexed;
        frame->SetPixelFormat(&pf);
        frame->SetPalette(pal.Get());
        Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> mw;
        if (SUCCEEDED(frame->GetMetadataQueryWriter(&mw)) && mw) {
            PROPVARIANT pv{};
            PropVariantInit(&pv);
            pv.vt = VT_UI2;
            pv.uiVal = 20; // 200 ms real GIF frame delay
            mw->SetMetadataByName(L"/grctlext/Delay", &pv);
            PropVariantClear(&pv);
        }
        if (FAILED(frame->WriteSource(conv.Get(), nullptr))) return out;
        if (FAILED(frame->Commit())) return out;
    }
    if (FAILED(enc->Commit())) return out;
    HGLOBAL hMem = nullptr;
    if (SUCCEEDED(GetHGlobalFromStream(memStream.Get(), &hMem)) && hMem) {
        const SIZE_T sz = GlobalSize(hMem);
        if (const void* ptr = GlobalLock(hMem)) {
            out.assign(static_cast<const std::uint8_t*>(ptr), static_cast<const std::uint8_t*>(ptr) + sz);
            GlobalUnlock(hMem);
        }
    }
    return out;
}

void PopulateMediaShowcaseScene(StudioAppState* st) {
    if (!st) return;
    st->doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    st->doc.SetCanvasBackground(PaperPattern::Dots);
    st->doc.SetTitle("Multimedia Enriquecida \xC2\xB7 CuadernU");

    // 1. Interactive LinkCard (top-left)
    {
        AnnotationObject lk{};
        lk.kind = ObjectKind::LinkCard;
        lk.layerId = st->doc.ActiveLayerId();
        lk.bounds = { 36.0, 32.0, 360.0, 92.0 };
        lk.linkTitleUtf8 = "Calculadora Gr\xC3\xA1""fica Desmos (Interactiva)";
        lk.linkUrlUtf8 = "https://www.desmos.com/calculator";
        lk.linkDescriptionUtf8 = "Abrir simulador de funciones 2D/3D (esquema https:// verificado)";
        lk.linkTargetPageIndex = -1;
        st->doc.AddCanvasObject(std::move(lk));
    }

    // 2. Real multi-frame animated GIF89a (bottom-left)
    {
        std::vector<std::uint8_t> gifBytes = BuildShowcaseAnimatedGifBytes();
        AnnotationObject gif{};
        gif.kind = ObjectKind::Image;
        gif.layerId = st->doc.ActiveLayerId();
        gif.bounds = { 36.0, 148.0, 360.0, 232.0 };
        gif.captionUtf8 = "Onda Arm\xC3\xB3nica (GIF89a \xC2\xB7 Delay 200ms)";
        gif.gifFrameCount = 2;
        gif.gifAvgDelayMs = 200;
        gif.imagePixelW = 160;
        gif.imagePixelH = 110;
        gif.pngBytes = std::move(gifBytes);
        st->doc.AddCanvasObject(std::move(gif));
    }

    // 3. Real Media Foundation VideoClip with synthesized MP4 & extracted poster (top-right)
    {
        wchar_t tmp[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tmp);
        const std::wstring mp4Path = (std::filesystem::path(tmp) / L"CuadernU_showcase_demo.mp4").wstring();
        std::string vErr;
        WriteSyntheticTestVideoFile(mp4Path, 320, 180, 15, 22, &vErr);
        RasterImageBgra32 posterBgra;
        std::vector<std::uint8_t> posterPng;
        std::uint64_t durMs = 1500;
        int vw = 320, vh = 180;
        ExtractVideoPosterAndMetadata(mp4Path, posterBgra, posterPng, durMs, vw, vh, &vErr);

        AnnotationObject vid{};
        vid.kind = ObjectKind::VideoClip;
        vid.layerId = st->doc.ActiveLayerId();
        vid.bounds = { 424.0, 32.0, 380.0, 214.0 };
        vid.captionUtf8 = "Experimento Osciloscopio RC";
        vid.videoSourcePathUtf8 = WideToUtf8(mp4Path);
        vid.videoDurationMs = durMs;
        vid.videoPosterPngBytes = std::move(posterPng);
        vid.imagePixelW = vw;
        vid.imagePixelH = vh;
        st->doc.AddCanvasObject(std::move(vid));
    }

    // 4. Real HtmlEmbed block (bottom-right, offline sandboxed + pre-rendered frozen PNG preview)
    {
        std::vector<std::uint8_t> htmlSnapPng;
        {
            const UINT sw = 360, sh = 196;
            std::vector<std::uint8_t> bgra(static_cast<std::size_t>(sw) * sh * 4u);
            for (UINT y = 0; y < sh; ++y) {
                for (UINT x = 0; x < sw; ++x) {
                    const bool grid = ((x % 36u == 0u) || (y % 32u == 0u));
                    const bool axis = (y == sh / 2u) || (x == 28u);
                    const double t = static_cast<double>(x - 28) / 28.0;
                    const double w1 = sh * 0.5 - std::sin(t * 1.15) * 56.0;
                    const double w2 = sh * 0.5 - std::cos(t * 0.65) * 34.0;
                    const bool on1 = (x >= 28u && x < sw - 14u && std::abs(static_cast<double>(y) - w1) < 2.6);
                    const bool on2 = (x >= 28u && x < sw - 14u && std::abs(static_cast<double>(y) - w2) < 2.0);
                    const std::size_t idx = (static_cast<std::size_t>(y) * sw + x) * 4u;
                    if (on1) { bgra[idx + 0] = 246; bgra[idx + 1] = 178; bgra[idx + 2] = 46; }       // Cyan-blue curve
                    else if (on2) { bgra[idx + 0] = 148; bgra[idx + 1] = 224; bgra[idx + 2] = 74; } // Emerald curve
                    else if (axis) { bgra[idx + 0] = 108; bgra[idx + 1] = 122; bgra[idx + 2] = 148; }
                    else if (grid) { bgra[idx + 0] = 44; bgra[idx + 1] = 50; bgra[idx + 2] = 64; }
                    else { bgra[idx + 0] = 26; bgra[idx + 1] = 30; bgra[idx + 2] = 40; }
                    bgra[idx + 3] = 255;
                }
            }
            Microsoft::WRL::ComPtr<IWICImagingFactory> fac;
            Microsoft::WRL::ComPtr<IStream> ms;
            Microsoft::WRL::ComPtr<IWICBitmapEncoder> enc;
            Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> frm;
            if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&fac))) &&
                SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &ms)) &&
                SUCCEEDED(fac->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) &&
                SUCCEEDED(enc->Initialize(ms.Get(), WICBitmapEncoderNoCache)) &&
                SUCCEEDED(enc->CreateNewFrame(&frm, nullptr)) &&
                SUCCEEDED(frm->Initialize(nullptr)) &&
                SUCCEEDED(frm->SetSize(sw, sh))) {
                WICPixelFormatGUID pf = GUID_WICPixelFormat32bppBGRA;
                if (SUCCEEDED(frm->SetPixelFormat(&pf)) &&
                    SUCCEEDED(frm->WritePixels(sh, sw * 4u, static_cast<UINT>(bgra.size()), bgra.data())) &&
                    SUCCEEDED(frm->Commit()) && SUCCEEDED(enc->Commit())) {
                    STATSTG stg{};
                    if (SUCCEEDED(ms->Stat(&stg, STATFLAG_NONAME)) && stg.cbSize.QuadPart > 0) {
                        htmlSnapPng.resize(static_cast<std::size_t>(stg.cbSize.QuadPart));
                        LARGE_INTEGER z{}; ms->Seek(z, STREAM_SEEK_SET, nullptr);
                        ULONG got = 0; ms->Read(htmlSnapPng.data(), static_cast<ULONG>(htmlSnapPng.size()), &got);
                        htmlSnapPng.resize(got);
                    }
                }
            }
        }
        AnnotationObject html{};
        html.kind = ObjectKind::HtmlEmbed;
        html.layerId = st->doc.ActiveLayerId();
        html.bounds = { 424.0, 268.0, 380.0, 226.0 };
        html.htmlTitleUtf8 = "Simulaci\xC3\xB3n HTML5 Canvas \xC2\xB7 Onda Senoidal";
        html.htmlSourceUtf8 = ClosureDialogs::DefaultAnimatedWaveHtmlTemplate();
        html.htmlSnapshotPngBytes = std::move(htmlSnapPng);
        html.htmlAllowNetwork = false;
        html.htmlInteractive = false;
        st->doc.AddCanvasObject(std::move(html));
    }

    // 5. TextBox with safe clickable inline URLs (underlined WCAG AA/AAA blue + Ctrl+Click)
    {
        AnnotationObject note{};
        note.kind = ObjectKind::TextBox;
        note.layerId = st->doc.ActiveLayerId();
        note.bounds = { 36.0, 396.0, 360.0, 96.0 };
        note.textUtf8 = "Enlaces clicables en texto (Ctrl+Clic):\nGuia: https://www.desmos.com/calculator\nClase: https://www.youtube.com/watch?v=fNk_zzaMoSs";
        note.strokeColor = DocColor::RGBA(24, 30, 42, 255);
        note.fillColor = DocColor::RGBA(248, 250, 254, 245);
        note.fontSizePt = 11.5;
        st->doc.AddCanvasObject(std::move(note));
    }

    RECT vpRc{};
    if (st->hwndViewport) GetClientRect(st->hwndViewport, &vpRc);
    const double vpW = static_cast<double>((std::max)(400L, vpRc.right - vpRc.left));
    const double vpH = static_cast<double>((std::max)(300L, vpRc.bottom - vpRc.top));
    const double zx = (vpW - 44.0) / 832.0;
    const double zy = (vpH - 40.0) / 530.0;
    st->camera.zoom = std::clamp((std::min)(zx, zy), 0.65, 2.0);
    st->camera.panX = -16.0;
    st->camera.panY = -12.0;
    st->statusBannerUtf8 = "Multimedia en Lienzo: Enlace + GIF animado + Video nativo + HTML5 interactivo (0% GPU)";
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nCmdShow) {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
    {
        int preArgc = 0;
        LPWSTR* preArgv = CommandLineToArgvW(GetCommandLineW(), &preArgc);
        bool runMcpStdio = false;
        bool runGnStatus = false;
        std::wstring overrideProf;
        if (preArgv) {
            for (int i = 1; i < preArgc; ++i) {
                const std::wstring a = preArgv[i];
                if (a == L"--mcp-stdio") runMcpStdio = true;
                else if (a == L"--gn-status") runGnStatus = true;
                else if (a == L"--profile-dir" && i + 1 < preArgc) overrideProf = preArgv[++i];
            }
            LocalFree(preArgv);
        }
        if (runMcpStdio || runGnStatus) {
            if (!overrideProf.empty()) {
                StudioProfileStore::SetProfileRootOverride(overrideProf);
            }
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            GeminiNotebook::GeminiNotebookSyncStore cliStore;
            cliStore.LoadAll(nullptr);
            int rc = 0;
            if (runMcpStdio) {
                rc = GeminiNotebook::CuadernUMcpBridge::RunStdioServer(cliStore);
            } else {
                const std::string statusResp = GeminiNotebook::CuadernUMcpBridge::HandleJsonRpcRequestLine(
                    R"({"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"cuadernu_notebook_status","arguments":{}}})",
                    cliStore, nullptr);
                std::cout << statusResp << "\n" << std::flush;
            }
            CoUninitialize();
            return rc;
        }
    }
#endif
    if (HMODULE hUser32 = GetModuleHandleW(L"user32.dll")) {
        using SetDpiCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        auto pSetDpiCtx = reinterpret_cast<SetDpiCtxFn>(GetProcAddress(hUser32, "SetProcessDpiAwarenessContext"));
        if (!pSetDpiCtx || !pSetDpiCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
            SetProcessDPIAware();
        }
    } else {
        SetProcessDPIAware();
    }
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    RoInitialize(RO_INIT_SINGLETHREADED);

    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gdiplusIn{};
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusIn, nullptr);

    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    auto registerCls = [&](const wchar_t* name, WNDPROC proc, HCURSOR cur, bool withDblClk = false, bool withIcon = false) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = withDblClk ? CS_DBLCLKS : 0;
        wc.lpfnWndProc = proc;
        wc.hInstance = hInst;
        wc.hCursor = cur;
        if (withIcon) {
            wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));
            wc.hIconSm = static_cast<HICON>(LoadImageW(hInst, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
        }
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wc.lpszClassName = name;
        RegisterClassExW(&wc);
    };
    registerCls(kMainClassName, MainWndProc, LoadCursorW(nullptr, IDC_ARROW), false, true);
    registerCls(kViewportClassName, ViewportWndProc, LoadCursorW(nullptr, IDC_CROSS), true, false);
    registerCls(kSidebarClassName, SidebarWndProc, LoadCursorW(nullptr, IDC_ARROW));
    registerCls(kInspectorClassName, InspectorWndProc, LoadCursorW(nullptr, IDC_ARROW));
    registerCls(kPromptClassName, PromptWndProc, LoadCursorW(nullptr, IDC_ARROW));

    {
        auto st = std::make_unique<StudioAppState>();
        st->flexcilOverlay.SetPopupNoteVisible(false);
        st->flexcilOverlay.SetGestureModeActive(false);
        // Collect UTF-8 CLI args for unified cross-platform workflow dispatch
        std::vector<std::string> cliArgsUtf8;
        {
            int argcW = 0;
            LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argcW);
            if (argvW) {
                for (int i = 1; i < argcW; ++i) {
                    cliArgsUtf8.push_back(WideToUtf8(argvW[i]));
                }
                LocalFree(argvW);
            }
        }
        const CliDispatch::StudioCliOptions cliOpts = CliDispatch::ParseArgsUtf8(cliArgsUtf8);
        if (cliOpts.invalidProfileDir ||
            (!cliOpts.profileDirUtf8.empty() &&
             !StudioProfileStore::SetProfileRootOverride(Utf8ToWide(cliOpts.profileDirUtf8)))) {
            OutputDebugStringW(L"CuadernU: --profile-dir requiere una ruta absoluta de directorio accesible.\n");
            st.reset();
            if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
            RoUninitialize();
            CoUninitialize();
            return 2;
        }
        if (!cliOpts.profileDirUtf8.empty()) {
            UiGlass::glassSettingsIniOverride =
                (std::filesystem::path(StudioProfileStore::DefaultProfileRootDir()) / L"translucent_settings.ini").wstring();
        }
        UiGlass::LoadGlassSettings(st->glassSettings);

        std::wstring initialOpen;
        std::string initialMode;
        ParseCliArgs(st.get(), initialOpen, initialMode);

        // Execute extended batch/workflow/academic/audio/crypto/LAN CLI commands first
        if (cliOpts.HasAnyExtendedWorkflow() ||
            (!cliOpts.openPathUtf8.empty() && !cliOpts.decryptPassUtf8.empty())) {
            const auto disp = CliDispatch::ExecuteWorkflow(cliOpts, st->doc, st->audioSession);
            if (!disp.statusBannerUtf8.empty()) {
                st->statusBannerUtf8 = disp.statusBannerUtf8;
            }
            if (disp.exitImmediately) {
                st.reset();
                if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
                RoUninitialize();
                CoUninitialize();
                return disp.exitCode;
            }
        }

        if (initialMode == "canvas" && st->doc.Mode() != DocumentMode::InfiniteCanvas) {
            st->doc.ResetToBlank(DocumentMode::InfiniteCanvas);
            st->doc.SetCanvasBackground(PaperPattern::Grid);
            st->doc.SetTitle("Lienzo Infinito");
        }

        UINT sysDpi = 96u;
        if (HMODULE hU32 = GetModuleHandleW(L"user32.dll")) {
            using GetDpiSysFn = UINT(WINAPI*)();
            if (auto fn = reinterpret_cast<GetDpiSysFn>(GetProcAddress(hU32, "GetDpiForSystem"))) {
                const UINT d = fn();
                if (d >= 72u && d <= 480u) sysDpi = d;
            }
        }
        RECT workArea{ 0, 0, 1920, 1080 };
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        const int maxW = (std::max)(800, static_cast<int>((workArea.right - workArea.left) * 94 / 100));
        const int maxH = (std::max)(600, static_cast<int>((workArea.bottom - workArea.top) * 92 / 100));
        const int defW = (std::min)(maxW, DpiSc(1380, sysDpi));
        const int defH = (std::min)(maxH, DpiSc(860, sysDpi));
        const int winW = cliOpts.hasExplicitWindowSize ? cliOpts.windowW : defW;
        const int winH = cliOpts.hasExplicitWindowSize ? cliOpts.windowH : defH;
        HWND hwnd = CreateWindowExW(0, kMainClassName, L"CuadernU \u2014 Calculadora Unificada",
            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, winW, winH,
            nullptr, nullptr, hInst, st.get());
        if (!hwnd) {
            st.reset();
            if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
            RoUninitialize();
            CoUninitialize();
            return 1;
        }

        if (cliOpts.simulateGpuReset) {
            st->renderer.DiscardDeviceResources();
            st->renderer.AttachHwnd(st->hwndViewport, (std::max)(200, winW - kSidebarWidth), (std::max)(200, winH - kTopChromeH - kBottomStatusH));
        }

        if (!initialOpen.empty() && !cliOpts.HasAnyExtendedWorkflow()) {
            const std::wstring openExt = GetFileExtLower(initialOpen);
            if (openExt != L".png" && openExt != L".jpg" && openExt != L".jpeg" && cliOpts.decryptPassUtf8.empty()) {
                OpenDocumentFile(st.get(), initialOpen);
            }
        }
        if (initialMode == "canvas" && cliOpts.insertPrintoutPathUtf8.empty()) {
            if (st->doc.Mode() != DocumentMode::InfiniteCanvas) {
                ExecuteToolbarCommand(st.get(), CMD_MODE_CANVAS);
            }
            st->camera.zoom = 1.0;
            st->camera.panX = -48.0;
            st->camera.panY = -48.0;
        }

        if (st->showcaseMediaCli) {
            PopulateMediaShowcaseScene(st.get());
        } else if (st->showcaseStemCli) {
            AppActions::PopulateStemShowcaseScene(st->doc, st->cmdStack);
            st->minimapVisible = true;
            st->rulerState.visible = true;
            st->rulerState.centerX = 610.0;
            st->rulerState.centerY = 294.0;
            st->rulerState.angleDeg = -8.0;
            st->rulerState.lengthPt = 250.0;
            RECT vpRc{};
            if (st->hwndViewport) GetClientRect(st->hwndViewport, &vpRc);
            const double availVw = static_cast<double>((std::max)(480L, vpRc.right - vpRc.left));
            const double availVh = static_cast<double>((std::max)(360L, vpRc.bottom - vpRc.top));
            const double zx = (availVw - 40.0) / 920.0;
            const double zy = (availVh - 40.0) / 620.0;
            st->camera.zoom = std::clamp((std::min)(zx, zy), 0.65, 2.0);
            st->camera.panX = -16.0;
            st->camera.panY = -12.0;
        } else if (!st->cliCalcCardSpec.empty() && !cliOpts.calcBridgeSync) {
            ApplyCliCalcCardSpec(st.get());
        } else if (st->doc.Mode() == DocumentMode::InfiniteCanvas && st->doc.CanvasObjects().empty() && initialOpen.empty()) {
            // Seed crisp sample ink & geometry in Infinite Canvas smoke/launch when empty
            AnnotationObject box;
            box.kind = ObjectKind::Shape;
            box.shapeKind = ShapeKind::Rectangle;
            box.bounds = { 80.0, 80.0, 260.0, 120.0 };
            box.strokeColor = DocColor::RGBA(0, 122, 255, 255);
            box.fillColor = DocColor::RGBA(232, 242, 255, 255);
            st->doc.AddCanvasObject(std::move(box));

            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.bounds = { 96.0, 102.0, 228.0, 80.0 };
            tb.textUtf8 = "Lienzo Infinito listo\nZoom y pan ilimitados";
            tb.strokeColor = DocColor::RGBA(20, 26, 36, 255);
            tb.fontSizePt = 15.0;
            st->doc.AddCanvasObject(std::move(tb));
        }

        LayoutMainChildren(st.get());
        if (!cliOpts.searchQueryUtf8.empty()) {
            st->searchVisible = true;
            LayoutMainChildren(st.get());
            if (st->hwndSearchEdit) {
                SetWindowTextW(st->hwndSearchEdit, Utf8ToWide(cliOpts.searchQueryUtf8).c_str());
            }
            st->lastSearchQueryUtf8.clear();
            ExecuteToolbarCommand(st.get(), CMD_SEARCH_NEXT);
            if (cliOpts.searchMatchIndex1Based > 1 && !st->searchMatches.empty()) {
                const std::size_t targetIdx = (std::min)(
                    static_cast<std::size_t>(cliOpts.searchMatchIndex1Based - 1),
                    st->searchMatches.size() - 1);
                while (st->activeSearchIdx != targetIdx) {
                    ExecuteToolbarCommand(st.get(), CMD_SEARCH_NEXT);
                }
            }
            // If we have an active canvas match and screenshotOutPath is requested, add a live annotation over the sheet
            if (!st->screenshotOutPath.empty() && !st->searchMatches.empty() && st->searchMatches[st->activeSearchIdx].onCanvas) {
                const auto& m = st->searchMatches[st->activeSearchIdx];
                // Translucent yellow highlighter stroke right under the matched region
                AnnotationObject hl{};
                hl.kind = ObjectKind::InkStroke;
                hl.layerId = st->doc.ActiveLayerId();
                hl.isHighlighter = true;
                hl.penStyle = InkPenStyle::Highlighter;
                hl.strokeColor = DocColor::RGBA(250, 204, 21, 110);
                hl.strokeWidth = 14.0;
                hl.inkPoints = {
                    { m.worldBox.x, m.worldBox.Bottom() + 18.0, 0.7f, 0, 0, 10 },
                    { m.worldBox.x + 190.0, m.worldBox.Bottom() + 18.0, 0.7f, 0, 0, 30 }
                };
                hl.RecomputeBounds();
                st->doc.AddCanvasObject(std::move(hl));

                // Red ink underline + arrow annotation pointing to the searched concept
                AnnotationObject ink{};
                ink.kind = ObjectKind::InkStroke;
                ink.layerId = st->doc.ActiveLayerId();
                ink.strokeColor = DocColor::RGBA(220, 38, 38, 245);
                ink.strokeWidth = 2.6;
                ink.inkPoints = {
                    { m.worldBox.x - 4.0, m.worldBox.Bottom() + 3.0, 0.75f, 0, 0, 40 },
                    { m.worldBox.x + (std::min)(220.0, m.worldBox.w + 20.0), m.worldBox.Bottom() + 3.0, 0.85f, 0, 0, 75 }
                };
                ink.RecomputeBounds();
                st->doc.AddCanvasObject(std::move(ink));

                // 2D Natural Math margin note + live circular/lasso cutout snippet pasted next to the printed page
                if (const auto* card = st->doc.FindCanvasObject(m.objectId)) {
                    AppActions::AddFormulaNodeAction(
                        st->doc, st->cmdStack, ObjectTarget{ true, 0 },
                        card->bounds.Right() + 14.0, m.worldBox.y - 36.0,
                        "dy_dt = 45 * (3 - 2)^8 / (2*3 + 1)^4", "", st->statusBannerUtf8);

                    const DocRect snipRect{
                        m.worldBox.x - 12.0,
                        m.worldBox.y - 6.0,
                        236.0,
                        78.0
                    };
                    std::vector<DocPoint> circlePoly;
                    circlePoly.reserve(36);
                    const double cx = snipRect.x + snipRect.w * 0.5;
                    const double cy = snipRect.y + snipRect.h * 0.5;
                    const double rx = snipRect.w * 0.48;
                    const double ry = snipRect.h * 0.47;
                    for (int k = 0; k < 36; ++k) {
                        const double a = (2.0 * 3.14159265358979323846 * k) / 36.0;
                        circlePoly.push_back({ cx + rx * std::cos(a), cy + ry * std::sin(a) });
                    }
                    if (SnipWorldRegionAsImage(st.get(), snipRect, &circlePoly)) {
                        st->lastContextWorldPt = { card->bounds.Right() + 14.0, m.worldBox.y + 72.0 };
                        st->hasLastContextWorldPt = true;
                        ExecuteToolbarCommand(st.get(), CMD_CLIPBOARD_PASTE);
                    }

                    st->statusBannerUtf8 = "Coincidencia " + std::to_string(st->activeSearchIdx + 1) + "/" +
                        std::to_string(st->searchMatches.size()) +
                        " en Lienzo Infinito \xC2\xB7 \"" + cliOpts.searchQueryUtf8 + "\" + Recorte circular pegado";
                }
            }
        }
        ShowWindow(hwnd, (st->smokeTestMode || st->cliTecPreviewSection >= 0) ? SW_SHOWNOACTIVATE : nCmdShow);
        UpdateWindow(hwnd);

#if defined(DOC_STUDIO_HAS_ACADEMIC)
        if (st->cliTecPreviewSection >= 0) {
            if (!st->tecStore) {
                st->tecStore = std::make_unique<DocStudio::TecDigital::TecAcademicStore>();
                st->tecStore->LoadSnapshotFromDisk(nullptr);
                st->tecStore->TryRestoreVaultSession();
            }
            const auto sec = static_cast<DocStudio::TecDigitalUi::HubSection>(st->cliTecPreviewSection);
            const wchar_t* shotW = st->screenshotOutPath.empty() ? nullptr : st->screenshotOutPath.c_str();
            DocStudio::TecDigitalUi::RunTecDigitalCenterDialog(
                hwnd,
                st->doc,
                *st->tecStore,
                sec,
                st->statusBannerUtf8,
                nullptr,
                st->cliTecPreviewSubView,
                nullptr,
                nullptr,
                nullptr,
                &st->glassSettings,
                shotW,
                st->cliTecPreviewInnerTab);
            if (shotW != nullptr) {
                DestroyWindow(hwnd);
                st.reset();
                if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
                RoUninitialize();
                CoUninitialize();
                return 0;
            }
        }
#endif

        if (!st->screenshotOutPath.empty() || st->smokeTestMode) {
            PostMessageW(hwnd, WM_APP_TAKE_SCREENSHOT, 0, 0);
        }

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        const int exitCode = (msg.message == WM_QUIT) ? static_cast<int>(msg.wParam) : 0;
        st.reset();
        if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
        RoUninitialize();
        CoUninitialize();
        return exitCode;
    }

    if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
    RoUninitialize();
    CoUninitialize();
    return 0;
}
