#ifdef _WIN32

#include "studio_tec_digital_dialog.hpp"
#include "studio_tec_digital_views.hpp"

#include "../core/utf_conv.hpp"
#include "../media/native_video_engine.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <shellapi.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

namespace DocStudio::TecDigitalUi {

namespace {

namespace td = academic::tec_digital;
namespace ht = academic::horarios_tec;

constexpr UINT WM_APP_TEC_WORK_DONE      = WM_APP + 310;
constexpr UINT WM_APP_TEC_PROGRESS       = WM_APP + 311;
constexpr UINT WM_APP_TEC_HORARIOS_DONE  = WM_APP + 312;
constexpr UINT WM_APP_TEC_ESCUELAS_DONE  = WM_APP + 313;

// Control IDs
enum CtrlId : int {
    IDC_NAV_BASE = 5100, // 5100..5106 for the 7 sections
    IDC_BTN_SYNC_NOW = 5120,
    IDC_BTN_OPEN_WEB = 5121,
    IDC_BTN_LOGIN_SHOW = 5122,

    // Resumen & Login
    IDC_EDIT_EMAIL = 5201,
    IDC_EDIT_PASS = 5202,
    IDC_CHK_REMEMBER_DPAPI = 5203,
    IDC_BTN_DO_LOGIN = 5204,
    IDC_BTN_DO_QR = 5205,
    IDC_BTN_LOGOUT_LOCAL = 5206,
    IDC_SUMMARY_REPORT = 5207,
    IDC_SUMMARY_CANVAS = 5208,

    // Cursos y Comunidades
    IDC_CMB_PERIOD = 5301,
    IDC_LST_COURSES = 5302,
    IDC_CMB_COURSE_SUBTAB = 5303,
    IDC_COURSE_DETAIL = 5304,
    IDC_BTN_COURSE_INSERT_CANVAS = 5305,
    IDC_BTN_COURSE_SYNC_FILES = 5306,
    IDC_BTN_COURSE_RUBRIC = 5307,
    IDC_EDIT_WHATIF_TARGET = 5308,
    IDC_BTN_WHATIF_CALC = 5309,
    IDC_BTN_COURSE_OPEN_WEB = 5310,
    IDC_BTN_COURSE_SEND_GEMINI = 5311,
    IDC_LST_COURSE_FILES = 5312,
    IDC_BTN_COURSE_MORE = 5313,
    IDC_COURSE_LIST_CANVAS = 5314,
    IDC_COURSE_DETAIL_CANVAS = 5315,

    // Calendario
    IDC_CMB_CAL_FILTER = 5401,
    IDC_CAL_LIST = 5402,
    IDC_BTN_CAL_EXPORT_ICS = 5403,
    IDC_CMB_CAL_VIEW = 5404,
    IDC_CAL_CANVAS = 5405,
    IDC_AGENDA_CANVAS = 5406,

    // Expediente y Malla Curricular
    IDC_CMB_EXP_VIEW = 5501,   // 0=Estadisticas y Notas, 1=Malla Curricular Interactiva, 2=Atributos de Graduacion
    IDC_CMB_GRADES_FILTER = 5502, // Todas / Aprobadas / Reprobadas
    IDC_EXP_REPORT = 5503,
    IDC_MALLA_CANVAS = 5504,
    IDC_MALLA_INSPECTOR = 5505,
    IDC_BTN_MALLA_INSERT = 5506,
    IDC_BTN_MALLA_ZOOM_IN = 5507,
    IDC_BTN_MALLA_ZOOM_OUT = 5508,
    IDC_CMB_MALLA_SEDE = 5509,
    IDC_CMB_MALLA_CARRERA = 5510,
    IDC_CMB_MALLA_PLAN = 5511,
    IDC_BTN_MALLA_LOAD_PLAN = 5512,
    IDC_ATTR_CANVAS = 5513,
    IDC_EXP_STATS_CANVAS = 5514,
    IDC_BTN_MALLA_TOGGLE_PICKER = 5515,

    // HorariosTEC En Vivo
    IDC_CMB_HOR_SEDE = 5601,
    IDC_CMB_HOR_CARRERA = 5602,
    IDC_CMB_HOR_PERIODO = 5603,
    IDC_CHK_HOR_PREREQ = 5604,
    IDC_CHK_HOR_NO_NIGHT = 5605,
    IDC_CHK_HOR_SEATS = 5606,
    IDC_BTN_HOR_LOAD_JSON = 5607,
    IDC_BTN_HOR_SOLVE = 5608,
    IDC_BTN_HOR_PREV = 5609,
    IDC_BTN_HOR_NEXT = 5610,
    IDC_BTN_HOR_INSERT = 5611,
    IDC_BTN_HOR_EXPORT = 5612,
    IDC_HOR_CANVAS = 5613,
    IDC_HOR_STATUS = 5614,
    IDC_CHK_HOR_BUSY = 5615,
    IDC_CMB_HOR_FREEDAY = 5616,
    IDC_EDIT_HOR_FILTER = 5617,
    IDC_CMB_HOR_SOURCE = 5618,
    IDC_CMB_HOR_ESCUELA = 5619,
    IDC_CMB_HOR_YEAR = 5620,
    IDC_CMB_HOR_PUB_SEDE = 5621,
    IDC_EDIT_HOR_CARNET = 5622,
    IDC_CMB_HOR_PUB_PER = 5623,
    IDC_BTN_HOR_TOGGLE_FILTERS = 5624,

    // Biblioteca Offline
    IDC_EDIT_FTS_QUERY = 5701,
    IDC_BTN_FTS_SEARCH = 5702,
    IDC_LST_FTS_HITS = 5703,
    IDC_BTN_FTS_INSERT = 5704,
    IDC_BTN_CACHE_PRUNE = 5705,
    IDC_LBL_CACHE_USAGE = 5706,
    IDC_FTS_PREVIEW = 5707,
    IDC_BTN_FTS_OPEN = 5708,
    IDC_FTS_CANVAS = 5709,

    // Tramites, Plagio y Servicios
    IDC_CMB_TRAMITE_CAT = 5801,
    IDC_TRAMITE_REPORT = 5802,
    IDC_BTN_SVC_MAPA = 5803,
    IDC_BTN_SVC_GUIA = 5804,
    IDC_BTN_SVC_SOPORTE = 5805,
    IDC_BTN_SVC_TURNITIN = 5806,
    IDC_BTN_SVC_MORE = 5807,
    IDC_TRAMITES_CANVAS = 5808
};

std::wstring GetCtrlTextW(HWND h) {
    if (!h) return {};
    int len = GetWindowTextLengthW(h);
    if (len <= 0) return {};
    std::wstring buf(static_cast<size_t>(len) + 1u, L'\0');
    GetWindowTextW(h, buf.data(), len + 1);
    buf.resize(static_cast<size_t>(len));
    return buf;
}

void OpenSafeUrl(HWND owner, const std::string& urlUtf8) {
    if (!DocStudio::IsSafeExternalLinkUrl(urlUtf8)) return;
    std::wstring w = Utf8ToWide(urlUtf8);
    ShellExecuteW(owner, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool PickSaveFileW(HWND owner, const wchar_t* filter, const wchar_t* defExt,
                   const wchar_t* defaultName, std::wstring& outPath) {
    wchar_t buf[MAX_PATH]{};
    if (defaultName) wcsncpy_s(buf, defaultName, _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defExt;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) return false;
    outPath = buf;
    return true;
}

bool PickOpenFileW(HWND owner, const wchar_t* filter, std::wstring& outPath) {
    wchar_t buf[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&ofn)) return false;
    outPath = buf;
    return true;
}

std::string FmtDouble(double v, int dec = 1) {
    char buf[48]{};
    if (dec == 0) std::snprintf(buf, sizeof(buf), "%.0f", v);
    else if (dec == 2) std::snprintf(buf, sizeof(buf), "%.2f", v);
    else std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

std::string SafeLowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool TryParseHexRgb(const std::string& hex, BYTE& r, BYTE& g, BYTE& b) {
    std::string s = hex;
    if (!s.empty() && s[0] == '#') s.erase(s.begin());
    if (s.size() != 6) return false;
    unsigned int val = 0;
    if (sscanf_s(s.c_str(), "%x", &val) != 1) return false;
    r = static_cast<BYTE>((val >> 16) & 0xFF);
    g = static_cast<BYTE>((val >> 8) & 0xFF);
    b = static_cast<BYTE>(val & 0xFF);
    return true;
}

std::string FormatCountdownToIso(const std::string& dateIso) {
    if (dateIso.size() < 10) return "";
    int y = 0, m = 0, d = 0;
    if (sscanf_s(dateIso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return "";
    SYSTEMTIME stNow{};
    GetLocalTime(&stNow);
    std::tm tNow{};
    tNow.tm_year = stNow.wYear - 1900;
    tNow.tm_mon = stNow.wMonth - 1;
    tNow.tm_mday = stNow.wDay;
    tNow.tm_hour = 12;
    std::tm tTarget{};
    tTarget.tm_year = y - 1900;
    tTarget.tm_mon = m - 1;
    tTarget.tm_mday = d;
    tTarget.tm_hour = 12;
    const std::time_t a = std::mktime(&tNow);
    const std::time_t b = std::mktime(&tTarget);
    if (a == static_cast<std::time_t>(-1) || b == static_cast<std::time_t>(-1)) return "";
    const int days = static_cast<int>(std::llround(std::difftime(b, a) / 86400.0));
    if (days == 0) return "Hoy";
    if (days == 1) return "Mañana";
    if (days > 1) return "En " + std::to_string(days) + " días";
    return "Hace " + std::to_string(-days) + " días";
}

std::string BestCourseStatusForNode(const td::StudentCurriculumProfile* prof, const std::string& code) {
    if (!prof) return "pendiente";
    std::string target = SafeLowerAscii(code);
    for (const auto& g : prof->grades) {
        if (SafeLowerAscii(g.id_course) != target) continue;
        std::string bs = SafeLowerAscii(g.best_status);
        if (bs == "1" || bs.find("aprob") != std::string::npos || bs.find("reconoc") != std::string::npos) {
            return "aprobada";
        }
        if (bs.find("curso") != std::string::npos) return "en_curso";
        if (bs.find("reprob") != std::string::npos) return "reprobada";
        for (const auto& att : g.grades_record) {
            if (att.state == 1 || att.grade >= 67.5) return "aprobada";
        }
        if (!g.grades_record.empty()) return "reprobada";
    }
    return "pendiente";
}

struct MallaNodeBox {
    std::string code;
    std::string name;
    int credits = 0;
    int levelIdx = 0;
    RECT rc{};
    std::string status; // "aprobada", "en_curso", "reprobada", "pendiente"
    std::vector<std::string> reqs;
    std::vector<std::string> coreqs;
    std::vector<std::string> reqBy;
};

struct TecHubState {
    HWND hwnd = nullptr;
    HWND owner = nullptr;
    DocStudio::DocumentModel* activeDoc = nullptr;
    DocStudio::TecDigital::TecAcademicStore* store = nullptr;
    std::string* outStatusBanner = nullptr;
    std::function<void()> onDocMutated;
    SendCourseToGeminiFn onSendToGemini;
    AddSm2FlashcardsFn onAddSm2;
    SearchActiveDocFn onSearchActiveDoc;
    int initialSubView = 0;
    bool docMutated = false;
    bool done = false;

    UINT dpi = 96;
    const UiGlass::GlassSettings* overrideGlass = nullptr;
    UiGlass::HigDialogTheme theme{};
    HBRUSH brSidebar = nullptr;
    HBRUSH brPaper = nullptr;
    HBRUSH brCard = nullptr;
    HBRUSH brInput = nullptr;
    HWND hoveredBtn = nullptr;
    HFONT fNorm = nullptr;
    HFONT fBold = nullptr;
    HFONT fTitle = nullptr;
    HFONT fSmall = nullptr;

    HubSection currentSection = HubSection::Resumen;
    std::atomic<bool> workerBusy{false};
    std::atomic<bool> workerCancel{false};
    std::thread workerThread;
    std::mutex workResultMutex;
    std::string pendingWorkStatus;
    std::string pendingError;
    int pendingWorkKind = 0; // 1=login, 2=sync, 3=files, 4=solve
    DocStudio::TecDigital::LiveScheduleSolveResult pendingHorariosResult;
    std::string pendingHorariosCatalogKey;
    std::string pendingHorariosStatus;
    bool pendingHorariosUsedFilter = false;
    std::vector<td::PublicGuiaEscuelaInfo> pendingPublicEscuelas;

    DocStudio::TecDigital::TecAcademicSnapshot snap;
    std::vector<td::CommunityInfo> filteredCourses;
    std::vector<td::FileRecord> currentCourseFiles;
    std::vector<td::MaterialSearchHit> ftsHits;
    std::vector<ActiveDocSearchHit> activeDocHits;

    // Interactive Malla Curricular state (0% GPU software canvas)
    float mallaZoom = 1.0f;
    int mallaPanX = 16;
    int mallaPanY = 16;
    bool mallaDragging = false;
    POINT mallaDragStart{};
    int mallaPanStartX = 0;
    int mallaPanStartY = 0;
    std::string selectedMallaCode;
    std::vector<MallaNodeBox> mallaBoxes;
    std::vector<td::SedeInfo> mallaSedes;
    std::vector<td::CarreraInfo> mallaCarreras;
    std::vector<td::PlanInfo> mallaPlanes;

    // Live HorariosTEC state (0% GPU software canvas)
    ht::CourseCatalog horariosCatalog;
    ht::ScheduleResult horariosResult;
    std::size_t selectedScheduleIdx = 0;
    std::string horariosCatalogKey;
    bool horariosUsedRequiredFilter = false;
    std::vector<td::PublicGuiaEscuelaInfo> publicEscuelas;
    std::vector<td::SedeInfo> horSedes;
    std::vector<td::CarreraInfo> horCarreras;
    std::vector<td::PlanInfo> horPeriodos;

    // Persistent controls
    HWND navBtns[7]{};
    HWND hTitle = nullptr;
    HWND hSubtitle = nullptr;
    HWND hSessionBadge = nullptr;
    HWND hBtnSync = nullptr;
    HWND hBtnOpenWeb = nullptr;
    HWND hProgressBanner = nullptr;

    // Section containers / groups of HWNDs for clean show/hide
    std::vector<HWND> secCtrls[7];

    // Panel geometry & active rounded frames painted in WM_PAINT
    RECT rcSidebarSep{};
    RECT panelAccountBand{};
    RECT panelCourseList{}, panelCourseDetail{}, panelCalList{}, panelExpReport{}, panelMallaInspector{};
    RECT panelFtsList{}, panelFtsPreview{}, panelTramiteReport{};
    RECT rcInEmail{}, rcInPass{}, rcInWhatIf{}, rcInHorFilter{}, rcInHorCarnet{}, rcInFts{};
    std::vector<RECT> framedPanelRects;
    std::vector<RECT> framedElevRects;
    std::vector<RECT> framedInputRects;

    // Quick references
    HWND hEmail = nullptr, hPass = nullptr, hChkDpapi = nullptr, hBtnLogin = nullptr, hBtnQr = nullptr, hBtnLogout = nullptr;
    HWND hSummaryEdit = nullptr, hSummaryCanvas = nullptr;

    HWND hCmbPeriod = nullptr, hLstCourses = nullptr, hCmbCourseSub = nullptr, hCourseDetail = nullptr;
    HWND hLstCourseFiles = nullptr;
    HWND hEditWhatIf = nullptr, hBtnWhatIf = nullptr, hBtnCourseRubric = nullptr;
    HWND hBtnCourseOpenWeb = nullptr, hBtnCourseGemini = nullptr;

    HWND hCmbCalFilter = nullptr, hCmbCalView = nullptr, hCalList = nullptr, hCalCanvas = nullptr;

    HWND hCmbExpView = nullptr, hCmbGradesFilter = nullptr, hExpReport = nullptr;
    HWND hCmbMallaSede = nullptr, hCmbMallaCarrera = nullptr, hCmbMallaPlan = nullptr, hBtnMallaLoad = nullptr;
    HWND hMallaCanvas = nullptr, hAttrCanvas = nullptr, hMallaInspector = nullptr;
    HWND hBtnMallaInsert = nullptr, hBtnMallaZIn = nullptr, hBtnMallaZOut = nullptr;

    HWND hCmbHorSource = nullptr, hCmbHorEscuela = nullptr, hCmbHorYear = nullptr;
    HWND hCmbHorPubSede = nullptr, hCmbHorPubPer = nullptr, hEditHorCarnet = nullptr;
    HWND hCmbHorSede = nullptr, hCmbHorCarrera = nullptr, hCmbHorPeriodo = nullptr;
    HWND hChkHorPrereq = nullptr, hChkHorNoNight = nullptr, hChkHorSeats = nullptr, hChkHorBusy = nullptr;
    HWND hCmbHorFreeDay = nullptr, hEditHorFilter = nullptr;
    HWND hHorCanvas = nullptr, hHorStatus = nullptr;

    HWND hEditFts = nullptr, hLstFts = nullptr, hFtsPreview = nullptr, hLblCache = nullptr;
    HWND hCmbTramite = nullptr, hTramiteReport = nullptr;
    HWND hBtnSvcMapa = nullptr, hBtnSvcGuia = nullptr, hBtnSvcSoporte = nullptr;

    // Redesigned guías de interfaz vector canvases & progressive-disclosure state
    HWND hCourseListCanvas = nullptr;
    HWND hCourseDetailCanvas = nullptr;
    HWND hAgendaCanvas = nullptr;
    HWND hExpStatsCanvas = nullptr;
    HWND hFtsCanvas = nullptr;
    HWND hTramitesCanvas = nullptr;
    HWND hBtnMallaPicker = nullptr;
    HWND hBtnHorFilters = nullptr;

    bool showLoginInline = false;
    bool mallaPickerExpanded = false;
    bool horFiltersExpanded = false;

    int selectedCourseIdx = 0;
    int courseSubTab = 0;
    int selectedCourseFileIdx = 0;
    int courseListScrollY = 0;
    int courseListMaxScroll = 0;
    int courseListHoverIdx = -1;
    int courseDetailScrollY = 0;
    int courseDetailMaxScroll = 0;
    int courseDetailHoverSeg = -1;
    int courseFileHoverIdx = -1;
    std::vector<Views::SegmentedHit> courseSegHits;
    std::vector<RECT> courseCardRects;
    std::vector<RECT> courseFileRects;
    std::vector<std::pair<RECT, int>> summaryCourseJumpRects;

    int agendaScrollY = 0;
    int agendaMaxScroll = 0;

    int expStatsScrollY = 0;
    int expStatsMaxScroll = 0;
    int expGradeFilter = 0; // 0=Todas, 1=Aprobadas, 2=En curso, 3=Reprobadas
    int expGradeFilterHover = -1;
    std::vector<Views::SegmentedHit> expGradeFilterHits;

    int ftsScrollY = 0;
    int ftsMaxScroll = 0;
    int selectedFtsIdx = 0;
    int ftsHoverIdx = -1;
    std::vector<RECT> ftsCardRects;

    int tramitesScrollY = 0;
    int tramitesMaxScroll = 0;

    const wchar_t* autoScreenshotPathW = nullptr;
    bool visualAuditDemoMode = false;

    int Sc(int px) const noexcept { return UiGlass::ScaleDpi(px, dpi); }

    void JoinWorker() {
        workerCancel.store(true, std::memory_order_release);
        if (workerThread.joinable()) workerThread.join();
        workerCancel.store(false, std::memory_order_release);
        workerBusy.store(false, std::memory_order_release);
    }
};

// ============================================================================
// guías de interfaz + material translúcido 2.0 Software Surface & Control Helpers
// ============================================================================

enum class HigBtnTier { Nav = 0, Primary = 1, Secondary = 2, Quiet = 3 };

struct ButtonVisualSpec {
    HigBtnTier tier = HigBtnTier::Secondary;
    bool inSidebar = false;
    bool inAccountBand = false;
    bool hasIcon = false;
    UiGlass::IconId icon = UiGlass::IconId::Document;
    UiGlass::Rgba8 iconColor{};
    int navIndex = -1;
};

ButtonVisualSpec ClassifyHigButton(int id, const UiGlass::GlassPalette& pal) noexcept {
    ButtonVisualSpec s{};
    if (id >= IDC_NAV_BASE && id < IDC_NAV_BASE + 7) {
        static const UiGlass::IconId kIcons[7] = {
            UiGlass::IconId::AcademicCap,
            UiGlass::IconId::ReadingBook,
            UiGlass::IconId::CalendarGrid,
            UiGlass::IconId::LayersStack,
            UiGlass::IconId::ClockHistory,
            UiGlass::IconId::Search,
            UiGlass::IconId::Checklist
        };
        const UiGlass::Rgba8 colors[7] = {
            pal.accentBlue, pal.accentIndigo, pal.accentRed,
            pal.accentPurple, pal.accentTeal, pal.accentOrange, pal.accentGreen
        };
        const int ni = id - IDC_NAV_BASE;
        s.tier = HigBtnTier::Nav;
        s.inSidebar = true;
        s.hasIcon = true;
        s.icon = kIcons[ni];
        s.iconColor = colors[ni];
        s.navIndex = ni;
        return s;
    }
    if (id == IDC_BTN_SYNC_NOW) {
        s.tier = HigBtnTier::Primary;
        s.inSidebar = true;
        s.hasIcon = true;
        s.icon = UiGlass::IconId::SyncCloud;
        return s;
    }
    if (id == IDC_BTN_OPEN_WEB) {
        s.tier = HigBtnTier::Secondary;
        s.inSidebar = true;
        s.hasIcon = true;
        s.icon = UiGlass::IconId::LinkChain;
        s.iconColor = pal.labelSecondary;
        return s;
    }
    if (id == IDC_BTN_DO_LOGIN) {
        s.tier = HigBtnTier::Primary;
        s.inAccountBand = true;
        return s;
    }
    if (id == IDC_BTN_DO_QR || id == IDC_BTN_LOGOUT_LOCAL) {
        s.tier = HigBtnTier::Quiet;
        s.inAccountBand = true;
        return s;
    }
    if (id == IDC_BTN_COURSE_INSERT_CANVAS || id == IDC_BTN_CAL_EXPORT_ICS ||
        id == IDC_BTN_MALLA_INSERT || id == IDC_BTN_HOR_SOLVE || id == IDC_BTN_FTS_SEARCH) {
        s.tier = HigBtnTier::Primary;
        return s;
    }
    if (id == IDC_BTN_MALLA_ZOOM_IN || id == IDC_BTN_MALLA_ZOOM_OUT ||
        id == IDC_BTN_HOR_PREV || id == IDC_BTN_HOR_NEXT || id == IDC_BTN_CACHE_PRUNE ||
        id == IDC_BTN_MALLA_TOGGLE_PICKER || id == IDC_BTN_HOR_TOGGLE_FILTERS) {
        s.tier = HigBtnTier::Quiet;
        return s;
    }
    s.tier = HigBtnTier::Secondary;
    return s;
}

void FillSolidDcRect(HDC hdc, int w, int h, UiGlass::Rgba8 c) noexcept {
    if (!hdc || w <= 0 || h <= 0) return;
    RECT rc{0, 0, w, h};
    HBRUSH br = CreateSolidBrush(RGB(c.r, c.g, c.b));
    if (br) {
        FillRect(hdc, &rc, br);
        DeleteObject(br);
    }
}

void DrawCleanSurfaceBox(Gdiplus::Graphics& g, float x, float y, float w, float h, float r,
                         UiGlass::Rgba8 fill, UiGlass::Rgba8 border, float borderW) {
    if (w <= 2.0f || h <= 2.0f) return;
    Gdiplus::GraphicsPath path;
    UiGlass::AddRoundPath(path, x, y, w, h, r);
    Gdiplus::SolidBrush br(UiGlass::ToGdip(fill));
    g.FillPath(&br, &path);
    if (borderW > 0.0f && border.a > 0) {
        Gdiplus::Pen pen(UiGlass::ToGdip(border), borderW);
        g.DrawPath(&pen, &path);
    }
}

void DrawEmptyStateCentered(
    Gdiplus::Graphics& g,
    int w, int h, float dpiScale,
    const UiGlass::GlassPalette& pal, bool hc,
    UiGlass::IconId icon,
    const wchar_t* title,
    const wchar_t* subtitle,
    const wchar_t* const* capLabels = nullptr,
    const UiGlass::IconId* capIcons = nullptr,
    int capCount = 0)
{
    const float medSz = 50.0f * dpiScale;
    const float cx = static_cast<float>(w) * 0.5f;
    const float blockH = (capCount > 0 ? 156.0f : 122.0f) * dpiScale;
    const float topY = std::max(22.0f * dpiScale, (static_cast<float>(h) - blockH) * 0.44f);

    // Soft circular icon medallion
    const float mx = cx - medSz * 0.5f;
    Gdiplus::SolidBrush medBr(UiGlass::ToGdip(pal.accentBlueSoft));
    g.FillEllipse(&medBr, mx, topY, medSz, medSz);
    if (hc) {
        Gdiplus::Pen mp(UiGlass::ToGdip(pal.accentBlue), 1.5f);
        g.DrawEllipse(&mp, mx, topY, medSz, medSz);
    }
    UiGlass::DrawVectorIconGdiPlus(
        g, icon, cx, topY + medSz * 0.5f, 22.0f * dpiScale,
        UiGlass::ToGdip(pal.accentBlue), 1.75f * dpiScale);

    Gdiplus::FontFamily ff(L"Segoe UI");
    Gdiplus::Font fHdr(&ff, 15.5f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
    Gdiplus::Font fSub(&ff, 12.5f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::Font fCap(&ff, 11.5f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brPrimary(UiGlass::ToGdip(pal.labelPrimary));
    Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
    Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));

    Gdiplus::StringFormat sfCenter;
    sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
    sfCenter.SetTrimming(Gdiplus::StringTrimmingWord);

    const float textW = std::clamp(static_cast<float>(w) - 64.0f * dpiScale, 180.0f, 460.0f * dpiScale);
    const float textX = cx - textW * 0.5f;
    Gdiplus::RectF tRc(textX, topY + medSz + 14.0f * dpiScale, textW, 24.0f * dpiScale);
    g.DrawString(title, -1, &fHdr, tRc, &sfCenter, &brPrimary);

    Gdiplus::RectF sRc(textX, tRc.GetBottom() + 4.0f * dpiScale, textW, 44.0f * dpiScale);
    g.DrawString(subtitle, -1, &fSub, sRc, &sfCenter, &brSec);

    if (capCount > 0 && capLabels && capIcons && w > static_cast<int>(420.0f * dpiScale)) {
        const float itemW = 160.0f * dpiScale;
        const float totalW = itemW * static_cast<float>(capCount);
        float ix = cx - totalW * 0.5f;
        const float iy = sRc.GetBottom() + 14.0f * dpiScale;
        Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);
        for (int i = 0; i < capCount; ++i) {
            if (i > 0) {
                g.DrawLine(&sepPen, ix, iy + 2.0f * dpiScale, ix, iy + 18.0f * dpiScale);
            }
            UiGlass::DrawVectorIconGdiPlus(
                g, capIcons[i], ix + 20.0f * dpiScale, iy + 10.0f * dpiScale,
                13.0f * dpiScale, UiGlass::ToGdip(pal.accentBlue), 1.45f * dpiScale);
            Gdiplus::StringFormat sfCap;
            sfCap.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfCap.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            Gdiplus::RectF cRc(ix + 32.0f * dpiScale, iy, itemW - 36.0f * dpiScale, 20.0f * dpiScale);
            g.DrawString(capLabels[i], -1, &fCap, cRc, &sfCap, &brTert);
            ix += itemW;
        }
    }
}

void UpdateActiveFramedPanels(TecHubState* st) {
    if (!st) return;
    st->framedPanelRects.clear();
    st->framedElevRects.clear();
    st->framedInputRects.clear();

    const bool authedOrCached = st->store &&
        (st->store->IsAuthenticated() || st->snap.sessionAuthenticated || !st->snap.communities.empty());

    switch (st->currentSection) {
    case HubSection::Resumen:
        if (!authedOrCached || st->showLoginInline) {
            st->framedElevRects.push_back(st->panelAccountBand);
            st->framedInputRects.push_back(st->rcInEmail);
            st->framedInputRects.push_back(st->rcInPass);
        } else if (st->panelAccountBand.right > st->panelAccountBand.left) {
            st->framedElevRects.push_back(st->panelAccountBand);
        }
        break;
    case HubSection::Cursos: {
        if (st->courseSubTab == 2 && st->rcInWhatIf.right > st->rcInWhatIf.left) {
            st->framedInputRects.push_back(st->rcInWhatIf);
        }
        break;
    }
    case HubSection::Calendario:
        break;
    case HubSection::ExpedienteYMalla: {
        int expMode = st->hCmbExpView ? static_cast<int>(SendMessageW(st->hCmbExpView, CB_GETCURSEL, 0, 0)) : 0;
        if (expMode == 1 || (expMode == 2 && !st->snap.graduateAttributes.empty())) {
            st->framedPanelRects.push_back(st->panelMallaInspector);
        }
        break;
    }
    case HubSection::HorariosVivo: {
        if (st->horFiltersExpanded && st->rcInHorFilter.right > st->rcInHorFilter.left) {
            st->framedInputRects.push_back(st->rcInHorFilter);
        }
        int horSrc = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
        if (horSrc == 2 && st->rcInHorCarnet.right > st->rcInHorCarnet.left) {
            st->framedInputRects.push_back(st->rcInHorCarnet);
        }
        break;
    }
    case HubSection::BibliotecaOffline:
        st->framedInputRects.push_back(st->rcInFts);
        break;
    case HubSection::TramitesYPerfil:
        break;
    }
}

LRESULT CALLBACK HigBtnSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
    auto* st = reinterpret_cast<TecHubState*>(dwRefData);
    if (msg == WM_MOUSEMOVE && st) {
        if (st->hoveredBtn != hwnd) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            HWND prev = st->hoveredBtn;
            st->hoveredBtn = hwnd;
            if (prev && IsWindow(prev)) InvalidateRect(prev, nullptr, FALSE);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    } else if (msg == WM_MOUSELEAVE && st) {
        if (st->hoveredBtn == hwnd) {
            st->hoveredBtn = nullptr;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
    } else if (msg == WM_NCDESTROY) {
        if (st && st->hoveredBtn == hwnd) st->hoveredBtn = nullptr;
        RemoveWindowSubclass(hwnd, HigBtnSubclassProc, uIdSubclass);
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

void DrawHigButton(const DRAWITEMSTRUCT* dis, TecHubState* st) {
    if (!dis || !st) return;
    const int w = std::max<int>(1, dis->rcItem.right - dis->rcItem.left);
    const int h = std::max<int>(1, dis->rcItem.bottom - dis->rcItem.top);
    const auto& pal = st->theme.pal;
    const bool dark = st->theme.settings.IsDark();
    const bool hc = st->theme.settings.IsHighContrast();
    const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

    const ButtonVisualSpec spec = ClassifyHigButton(static_cast<int>(dis->CtlID), pal);
    const bool disabled = (dis->itemState & ODS_DISABLED) != 0;
    const bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    const bool focused = ((dis->itemState & ODS_FOCUS) != 0) && ((dis->itemState & ODS_NOFOCUSRECT) == 0);
    const bool hovered = (st->hoveredBtn == dis->hwndItem);
    const bool activeNav = (spec.tier == HigBtnTier::Nav && spec.navIndex == static_cast<int>(st->currentSection));

    HDC memDc = CreateCompatibleDC(dis->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(dis->hDC, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);

    wchar_t text[128]{};
    GetWindowTextW(dis->hwndItem, text, 128);

    const UiGlass::Rgba8 parentBg = spec.inSidebar ? pal.sidebarBg
                                                   : (spec.inAccountBand ? pal.contentCardElevated : pal.contentPaper);
    FillSolidDcRect(memDc, w, h, parentBg);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const float rx = 1.0f;
        const float ry = 1.0f;
        const float rw = static_cast<float>(w) - 2.5f;
        const float rh = static_cast<float>(h) - 2.5f;
        const float radius = (spec.tier == HigBtnTier::Secondary || spec.tier == HigBtnTier::Quiet)
                                 ? static_cast<float>(st->Sc(7))
                                 : static_cast<float>(st->Sc(8));

        UiGlass::Rgba8 fill = parentBg;
        UiGlass::Rgba8 border{ 0, 0, 0, 0 };
        float borderW = 0.0f;
        UiGlass::Rgba8 txtCol = pal.labelPrimary;
        bool useBoldText = false;

        if (spec.tier == HigBtnTier::Nav) {
            if (activeNav) {
                fill = pal.sidebarSelected;
                border = hc ? pal.accentBlue : pal.borderSubtle;
                borderW = hc ? 1.5f : 0.75f;
                txtCol = pal.labelPrimary;
                useBoldText = true;
            } else if (pressed) {
                fill = pal.sidebarSelected;
            } else if (hovered) {
                fill = pal.sidebarHover;
            }
            if (activeNav || pressed || hovered) {
                DrawCleanSurfaceBox(g, rx, ry, rw, rh, radius, fill, border, borderW);
            } else if (hc) {
                DrawCleanSurfaceBox(g, rx, ry, rw, rh, radius, parentBg, { 0, 0, 0, 0 }, 0.0f);
            }
            if (spec.hasIcon) {
                UiGlass::DrawVectorIconGdiPlus(
                    g, spec.icon, static_cast<float>(st->Sc(18)), static_cast<float>(h) * 0.5f,
                    static_cast<float>(st->Sc(15)), UiGlass::ToGdip(spec.iconColor), 1.55f * dpiScale);
            }
        } else if (spec.tier == HigBtnTier::Primary) {
            // Ensure WCAG AA+ white text contrast in Dark Mode too
            UiGlass::Rgba8 primNorm = hc ? pal.accentBlue : (dark ? UiGlass::Rgba8{ 10, 112, 224, 255 } : pal.accentBlue);
            UiGlass::Rgba8 primHov  = hc ? pal.accentBlueHover : (dark ? UiGlass::Rgba8{ 26, 130, 244, 255 } : pal.accentBlueHover);
            UiGlass::Rgba8 primPres = dark ? UiGlass::Rgba8{ 8, 92, 188, 255 } : UiGlass::Rgba8{ 0, 74, 174, 255 };
            fill = disabled ? pal.borderSubtle : (pressed ? primPres : (hovered ? primHov : primNorm));
            border = hc ? pal.borderStrong : UiGlass::Rgba8{ 0, 0, 0, 0 };
            borderW = hc ? 1.5f : 0.0f;
            txtCol = disabled ? pal.labelTertiary : UiGlass::Rgba8{ 255, 255, 255, 255 };
            useBoldText = true;
            DrawCleanSurfaceBox(g, rx, ry, rw, rh, radius, fill, border, borderW);
        } else if (spec.tier == HigBtnTier::Secondary) {
            fill = pressed ? pal.sidebarSelected : (hovered ? pal.sidebarHover : pal.contentCardElevated);
            border = hc ? pal.borderStrong : pal.borderSubtle;
            borderW = hc ? 1.5f : 1.0f;
            txtCol = disabled ? pal.labelTertiary : pal.labelPrimary;
            useBoldText = hc;
            DrawCleanSurfaceBox(g, rx, ry, rw, rh, radius, fill, border, borderW);
        } else { // Quiet
            fill = pressed ? pal.sidebarSelected : (hovered ? pal.sidebarHover : parentBg);
            border = hc ? pal.borderSubtle : (hovered ? pal.borderSubtle : UiGlass::Rgba8{ 0, 0, 0, 0 });
            borderW = hc ? 1.25f : (hovered ? 0.8f : 0.0f);
            txtCol = disabled ? pal.labelTertiary : (hc ? pal.labelPrimary : pal.labelSecondary);
            useBoldText = hc;
            if (pressed || hovered || hc) {
                DrawCleanSurfaceBox(g, rx, ry, rw, rh, radius, fill, border, borderW);
            }
        }

        // Optional icon on Primary / Secondary (e.g., Sync / Open web)
        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font font(&ff, 12.8f * dpiScale,
                           useBoldText ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular,
                           Gdiplus::UnitPixel);
        Gdiplus::SolidBrush tBr(UiGlass::ToGdip(txtCol));
        Gdiplus::StringFormat sf;
        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        if (spec.tier == HigBtnTier::Nav) {
            Gdiplus::RectF trc(static_cast<float>(st->Sc(34)), 0.0f,
                               std::max(10.0f, static_cast<float>(w - st->Sc(42))), static_cast<float>(h));
            g.DrawString(text, -1, &font, trc, &sf, &tBr);
        } else if (spec.hasIcon) {
            const float icoSz = static_cast<float>(st->Sc(13));
            const float icoX = static_cast<float>(st->Sc(18));
            UiGlass::Rgba8 iCol = (spec.tier == HigBtnTier::Primary)
                                      ? UiGlass::Rgba8{ 255, 255, 255, 255 }
                                      : pal.labelSecondary;
            UiGlass::DrawVectorIconGdiPlus(g, spec.icon, icoX, static_cast<float>(h) * 0.5f,
                                           icoSz, UiGlass::ToGdip(iCol), 1.5f * dpiScale);
            sf.SetAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF trc(static_cast<float>(st->Sc(26)), 0.0f,
                               std::max(10.0f, static_cast<float>(w - st->Sc(32))), static_cast<float>(h));
            g.DrawString(text, -1, &font, trc, &sf, &tBr);
        } else {
            sf.SetAlignment(Gdiplus::StringAlignmentCenter);
            Gdiplus::RectF trc(static_cast<float>(st->Sc(6)), 0.0f,
                               std::max(10.0f, static_cast<float>(w - st->Sc(12))), static_cast<float>(h));
            g.DrawString(text, -1, &font, trc, &sf, &tBr);
        }

        if (focused) {
            Gdiplus::GraphicsPath fPath;
            UiGlass::AddRoundPath(fPath, rx + 1.5f, ry + 1.5f, rw - 3.0f, rh - 3.0f, std::max(2.0f, radius - 1.5f));
            Gdiplus::Pen fPen(UiGlass::ToGdip(pal.accentBlue), 1.5f);
            g.DrawPath(&fPen, &fPath);
        }
    }

    BitBlt(dis->hDC, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
}

void PaintSummaryCanvas(HWND hwnd, TecHubState* st) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    FillSolidDcRect(memDc, w, h, st->theme.pal.contentPaper);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        DrawCleanSurfaceBox(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                            static_cast<float>(st->Sc(10)), pal.contentCard,
                            hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);

        const bool noData = st->snap.communities.empty() && st->snap.syncedAtUtc.empty() &&
                            st->snap.unifiedCalendarEvents.empty() && st->snap.currentSchedule.empty();
        if (noData) {
            static const wchar_t* kTags[3] = {
                L"Agenda y entregas",
                L"Malla y ruta crítica",
                L"Horarios sin choques"
            };
            static const UiGlass::IconId kIcons[3] = {
                UiGlass::IconId::CalendarGrid,
                UiGlass::IconId::LayersStack,
                UiGlass::IconId::ClockHistory
            };
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::AcademicCap,
                L"Conecte su cuenta de TEC Digital",
                L"Consulte sus cursos, agenda, malla curricular, atributos de graduación y materiales sin conexión en modo solo lectura.",
                kTags, kIcons, 3);
        } else {
            const float padX = static_cast<float>(st->Sc(18));
            const float topY = static_cast<float>(st->Sc(14));
            const float innerW = std::max(120.0f, static_cast<float>(w) - padX * 2.0f);

            Gdiplus::FontFamily ff(L"Segoe UI");
            Gdiplus::Font fNum(&ff, 18.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fSecHdr(&ff, 12.5f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fRow(&ff, 12.2f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::Font fLbl(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
            Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
            Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
            Gdiplus::SolidBrush brBlue(UiGlass::ToGdip(pal.accentBlue));
            Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);

            Gdiplus::StringFormat sfTrim;
            sfTrim.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sfTrim.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

            // Top editorial metric strip (3 columns separated by subtle vertical hairlines)
            const float colW3 = innerW / 3.0f;
            const std::wstring mVals[3] = {
                std::to_wstring(st->snap.communities.size()),
                std::to_wstring(st->snap.unifiedCalendarEvents.size()),
                Utf8ToWide(st->snap.syncedAtUtc.empty() ? "Pendiente" : st->snap.syncedAtUtc.substr(0, 10))
            };
            const wchar_t* mTitles[3] = {
                L"Cursos y comunidades",
                L"Entregas y citas",
                L"Última sincronización"
            };
            for (int c = 0; c < 3; ++c) {
                float mx = padX + static_cast<float>(c) * colW3;
                if (c > 0) g.DrawLine(&sepPen, mx, topY + 2.0f, mx, topY + static_cast<float>(st->Sc(42)));
                float ox = (c > 0) ? (mx + static_cast<float>(st->Sc(14))) : mx;
                g.DrawString(mTitles[c], -1, &fLbl, Gdiplus::RectF(ox, topY, colW3 - 18.0f, 16.0f * dpiScale), &sfTrim, &brTert);
                g.DrawString(mVals[c].c_str(), -1, &fNum, Gdiplus::RectF(ox, topY + 17.0f * dpiScale, colW3 - 18.0f, 26.0f * dpiScale), &sfTrim, &brPrim);
            }

            float curY = topY + static_cast<float>(st->Sc(52));
            g.DrawLine(&sepPen, padX, curY, padX + innerW, curY);
            curY += static_cast<float>(st->Sc(12));

            // Agenda de hoy y matrícula
            SYSTEMTIME stNow{};
            GetLocalTime(&stNow);
            const int todayMon0 = (stNow.wDayOfWeek == 0) ? 6 : (stNow.wDayOfWeek - 1);
            const int nowMin = stNow.wHour * 60 + stNow.wMinute;
            const td::WeeklyScheduleCell* nextClass = nullptr;
            for (const auto& cell : st->snap.currentSchedule) {
                if (cell.day_index_0_mon == todayMon0 && cell.end_minute >= nowMin) {
                    if (!nextClass || cell.start_minute < nextClass->start_minute) nextClass = &cell;
                }
            }
            UiGlass::DrawVectorIconGdiPlus(g, UiGlass::IconId::ClockHistory,
                                           padX + 7.0f * dpiScale, curY + 8.0f * dpiScale,
                                           13.0f * dpiScale, UiGlass::ToGdip(pal.accentTeal), 1.5f * dpiScale);
            g.DrawString(L"Agenda de hoy y matrícula", -1, &fSecHdr,
                         Gdiplus::RectF(padX + 20.0f * dpiScale, curY, innerW - 20.0f, 18.0f * dpiScale), &sfTrim, &brPrim);
            curY += static_cast<float>(st->Sc(22));

            std::string agendaLine = nextClass
                ? ("Próxima clase hoy: " + (nextClass->course_code.empty() ? nextClass->raw_text : nextClass->course_code) +
                   " (" + nextClass->time_range + ")" + (nextClass->room.empty() ? "" : (" · Aula " + nextClass->room)))
                : "Sin clases pendientes registradas para hoy";
            g.DrawString(Utf8ToWide(agendaLine).c_str(), -1, &fRow,
                         Gdiplus::RectF(padX + 4.0f, curY, innerW - 8.0f, 18.0f * dpiScale), &sfTrim, &brSec);
            curY += static_cast<float>(st->Sc(20));

            if (st->snap.expediente && !st->snap.expediente->structured.citas_matricula.empty()) {
                const auto& cm = st->snap.expediente->structured.citas_matricula.front();
                std::string cd = FormatCountdownToIso(cm.date);
                std::string line = "Próxima cita de matrícula: " + cm.date + " " + cm.time + " (" + cm.modality + ")";
                if (!cd.empty()) line += " · " + cd;
                g.DrawString(Utf8ToWide(line).c_str(), -1, &fRow,
                             Gdiplus::RectF(padX + 4.0f, curY, innerW - 8.0f, 18.0f * dpiScale), &sfTrim, &brBlue);
                curY += static_cast<float>(st->Sc(20));
            }
            if (!st->snap.pendingPerformanceEvals.empty()) {
                std::string peLine = "Evaluación de desempeño docente pendiente: " +
                                     std::to_string(st->snap.pendingPerformanceEvals.size()) +
                                     " curso(s) — complete en TEC Digital";
                g.DrawString(Utf8ToWide(peLine).c_str(), -1, &fRow,
                             Gdiplus::RectF(padX + 4.0f, curY, innerW - 8.0f, 18.0f * dpiScale), &sfTrim, &brSec);
                curY += static_cast<float>(st->Sc(20));
            }

            curY += static_cast<float>(st->Sc(6));
            g.DrawLine(&sepPen, padX, curY, padX + innerW, curY);
            curY += static_cast<float>(st->Sc(12));

            // Two calm columns below: Entregas próximas (left) | Avisos recientes (right)
            const float halfW = (innerW - static_cast<float>(st->Sc(24))) * 0.5f;
            const float rightX = padX + halfW + static_cast<float>(st->Sc(24));
            g.DrawLine(&sepPen, padX + halfW + static_cast<float>(st->Sc(12)), curY,
                       padX + halfW + static_cast<float>(st->Sc(12)), static_cast<float>(h - st->Sc(14)));

            g.DrawString(L"Próximas entregas y eventos", -1, &fSecHdr,
                         Gdiplus::RectF(padX, curY, halfW, 18.0f * dpiScale), &sfTrim, &brPrim);
            g.DrawString(L"Avisos recientes", -1, &fSecHdr,
                         Gdiplus::RectF(rightX, curY, halfW, 18.0f * dpiScale), &sfTrim, &brPrim);

            float ly = curY + static_cast<float>(st->Sc(24));
            const float rowStep = static_cast<float>(st->Sc(22));
            int evShown = 0;
            for (const auto& ev : st->snap.unifiedCalendarEvents) {
                if (ly + rowStep > static_cast<float>(h - st->Sc(10)) || evShown >= 8) break;
                std::string cd = FormatCountdownToIso(ev.date_iso);
                std::string row = ev.date_iso + "  " + ev.title + (cd.empty() ? "" : (" (" + cd + ")"));
                g.DrawString(Utf8ToWide(row).c_str(), -1, &fRow,
                             Gdiplus::RectF(padX, ly, halfW, rowStep), &sfTrim, &brSec);
                ly += rowStep;
                ++evShown;
            }
            if (evShown == 0) {
                g.DrawString(L"Sin entregas o eventos registrados", -1, &fRow,
                             Gdiplus::RectF(padX, ly, halfW, rowStep), &sfTrim, &brTert);
            }

            float ry = curY + static_cast<float>(st->Sc(24));
            int nShown = 0;
            auto drawNotice = [&](const std::string& txt) {
                if (ry + rowStep > static_cast<float>(h - st->Sc(10)) || nShown >= 8) return false;
                g.DrawString(Utf8ToWide(txt).c_str(), -1, &fRow,
                             Gdiplus::RectF(rightX, ry, halfW, rowStep), &sfTrim, &brSec);
                ry += rowStep;
                ++nShown;
                return true;
            };
            if (st->snap.notifications) {
                for (const auto& n : st->snap.notifications->notifications) {
                    if (!drawNotice((n.state == "U" ? "[Nuevo] " : "") + n.title + " — " + n.text)) break;
                }
            }
            for (const auto& [path, newsList] : st->snap.newsByPath) {
                for (const auto& nr : newsList) {
                    if (!drawNotice("[Noticia] " + nr.pub_date + " — " + nr.title)) break;
                }
            }
            for (const auto& [path, smsList] : st->snap.smsByPath) {
                for (const auto& m : smsList) {
                    if (!drawNotice("[SMS] " + m.transaction_date + " — " + m.content)) break;
                }
            }
            if (nShown == 0) {
                g.DrawString(L"Sin avisos recientes", -1, &fRow,
                             Gdiplus::RectF(rightX, ry, halfW, rowStep), &sfTrim, &brTert);
            }
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK SummaryCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_PAINT && st) {
        PaintSummaryCanvas(hwnd, st);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
// Software Vector Renderers (GDI+ offscreen DIB -> strictly 0.0% GPU!)
// ============================================================================

void PaintMallaCurricularCanvas(HWND hwnd, TecHubState* st) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    FillSolidDcRect(memDc, w, h, st->theme.pal.contentPaper);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        DrawCleanSurfaceBox(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                            static_cast<float>(st->Sc(10)), pal.contentCard,
                            hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);

        st->mallaBoxes.clear();
        if (!st->snap.curriculumGraph || st->snap.curriculumGraph->levels.empty()) {
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::LayersStack,
                L"Malla curricular interactiva",
                L"Inicie sesión y presione Sincronizar ahora, o seleccione una sede, carrera y plan para explorar requisitos, correquisitos y ruta crítica.");
        } else {
            const auto& graph = *st->snap.curriculumGraph;
            const auto* prof = st->snap.studentProfile ? &(*st->snap.studentProfile) : nullptr;
            const float z = std::clamp(st->mallaZoom, 0.55f, 1.8f) * dpiScale;
            const int colW = static_cast<int>(160.0f * z);
            const int colGap = static_cast<int>(20.0f * z);
            const int nodeH = static_cast<int>(54.0f * z);
            const int nodeGap = static_cast<int>(10.0f * z);
            const int hdrH = static_cast<int>(28.0f * z);

            Gdiplus::FontFamily ff(L"Segoe UI");
            Gdiplus::Font fHdr(&ff, std::max(10.0f, 12.0f * z), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fCode(&ff, std::max(9.5f, 11.5f * z), Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fName(&ff, std::max(9.0f, 10.5f * z), Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

            std::unordered_map<std::string, POINT> centerByCode;
            std::set<std::string> hlReqs, hlCoreqs, hlUnlocked;

            for (size_t li = 0; li < graph.levels.size(); ++li) {
                const auto& lv = graph.levels[li];
                int x = st->mallaPanX + static_cast<int>(li) * (colW + colGap);
                for (size_t ci = 0; ci < lv.courses.size(); ++ci) {
                    const auto& c = lv.courses[ci];
                    int y = st->mallaPanY + hdrH + static_cast<int>(ci) * (nodeH + nodeGap);
                    MallaNodeBox nb;
                    nb.code = c.id_course;
                    nb.name = c.name;
                    nb.credits = c.credits;
                    nb.levelIdx = static_cast<int>(li);
                    nb.rc = { x, y, x + colW, y + nodeH };
                    nb.status = BestCourseStatusForNode(prof, c.id_course);
                    nb.reqs = c.requirements;
                    nb.coreqs = c.co_requirements;
                    nb.reqBy = c.required_by;
                    centerByCode[SafeLowerAscii(nb.code)] = { (nb.rc.left + nb.rc.right) / 2,
                                                              (nb.rc.top + nb.rc.bottom) / 2 };
                    if (!st->selectedMallaCode.empty() &&
                        SafeLowerAscii(nb.code) == SafeLowerAscii(st->selectedMallaCode)) {
                        for (const auto& r : nb.reqs) hlReqs.insert(SafeLowerAscii(r));
                        for (const auto& r : nb.coreqs) hlCoreqs.insert(SafeLowerAscii(r));
                        for (const auto& r : nb.reqBy) hlUnlocked.insert(SafeLowerAscii(r));
                    }
                    st->mallaBoxes.push_back(std::move(nb));
                }
            }

            if (!st->selectedMallaCode.empty()) {
                auto sit = centerByCode.find(SafeLowerAscii(st->selectedMallaCode));
                if (sit != centerByCode.end()) {
                    auto drawBezierEdge = [&](const Gdiplus::Pen& pen, POINT a, POINT b) {
                        const float dx = static_cast<float>(b.x - a.x) * 0.45f;
                        g.DrawBezier(&pen,
                                     static_cast<float>(a.x), static_cast<float>(a.y),
                                     static_cast<float>(a.x) + dx, static_cast<float>(a.y),
                                     static_cast<float>(b.x) - dx, static_cast<float>(b.y),
                                     static_cast<float>(b.x), static_cast<float>(b.y));
                    };
                    Gdiplus::Pen penReq(UiGlass::ToGdip(pal.accentOrange, 220), 2.0f);
                    Gdiplus::Pen penCoreq(UiGlass::ToGdip(pal.accentBlue, 220), 2.0f);
                    Gdiplus::Pen penUnl(UiGlass::ToGdip(pal.accentGreen, 220), 2.0f);
                    for (const auto& rcCode : hlReqs) {
                        auto it = centerByCode.find(rcCode);
                        if (it != centerByCode.end()) drawBezierEdge(penReq, it->second, sit->second);
                    }
                    for (const auto& ccCode : hlCoreqs) {
                        auto it = centerByCode.find(ccCode);
                        if (it != centerByCode.end()) drawBezierEdge(penCoreq, it->second, sit->second);
                    }
                    for (const auto& uCode : hlUnlocked) {
                        auto it = centerByCode.find(uCode);
                        if (it != centerByCode.end()) drawBezierEdge(penUnl, sit->second, it->second);
                    }
                }
            }

            Gdiplus::SolidBrush hdrTxt(UiGlass::ToGdip(pal.labelSecondary));
            for (size_t li = 0; li < graph.levels.size(); ++li) {
                int x = st->mallaPanX + static_cast<int>(li) * (colW + colGap);
                std::wstring hdr = L"Semestre " + Utf8ToWide(graph.levels[li].id);
                g.DrawString(hdr.c_str(), -1, &fHdr,
                             Gdiplus::PointF(static_cast<float>(x + 4), static_cast<float>(st->mallaPanY + 4)),
                             &hdrTxt);
            }

            for (const auto& nb : st->mallaBoxes) {
                const std::string low = SafeLowerAscii(nb.code);
                const bool isSel = (!st->selectedMallaCode.empty() && low == SafeLowerAscii(st->selectedMallaCode));
                const bool isReq = hlReqs.count(low) > 0;
                const bool isCoreq = hlCoreqs.count(low) > 0;
                const bool isUnl = hlUnlocked.count(low) > 0;

                UiGlass::Rgba8 fill = pal.contentCardElevated;
                UiGlass::Rgba8 border = hc ? pal.borderStrong : pal.borderSubtle;
                UiGlass::Rgba8 stripe = pal.borderSubtle;
                if (nb.status == "aprobada") {
                    fill = hc ? pal.contentCard : (dark ? UiGlass::Rgba8{ 22, 54, 40, 255 } : UiGlass::Rgba8{ 234, 248, 240, 255 });
                    border = pal.accentGreen;
                    stripe = pal.accentGreen;
                } else if (nb.status == "en_curso") {
                    fill = hc ? pal.contentCard : (dark ? UiGlass::Rgba8{ 24, 46, 76, 255 } : pal.accentBlueSoft);
                    border = pal.accentBlue;
                    stripe = pal.accentBlue;
                } else if (nb.status == "reprobada") {
                    fill = hc ? pal.contentCard : (dark ? UiGlass::Rgba8{ 68, 28, 34, 255 } : UiGlass::Rgba8{ 254, 238, 238, 255 });
                    border = pal.accentRed;
                    stripe = pal.accentRed;
                }
                if (isReq) border = pal.accentOrange;
                if (isCoreq) border = pal.accentBlue;
                if (isUnl) border = pal.accentGreen;
                if (isSel) border = pal.labelPrimary;

                const float rx = static_cast<float>(nb.rc.left);
                const float ry = static_cast<float>(nb.rc.top);
                const float rw = static_cast<float>(nb.rc.right - nb.rc.left);
                const float rh = static_cast<float>(nb.rc.bottom - nb.rc.top);
                DrawCleanSurfaceBox(g, rx, ry, rw, rh, 8.0f * z, fill, border,
                                    (isSel || isReq || isCoreq || isUnl) ? 2.0f : (hc ? 1.5f : 1.0f));

                Gdiplus::SolidBrush stripeBr(UiGlass::ToGdip(stripe));
                g.FillRectangle(&stripeBr, rx + 3.0f, ry + 8.0f * z, 3.0f, std::max(8.0f, rh - 16.0f * z));

                Gdiplus::SolidBrush tPrimary(UiGlass::ToGdip(pal.labelPrimary));
                Gdiplus::SolidBrush tSec(UiGlass::ToGdip(pal.labelSecondary));
                std::wstring codeW = Utf8ToWide(nb.code + " · " + std::to_string(nb.credits) + " cr");
                std::wstring nameW = Utf8ToWide(nb.name);
                Gdiplus::RectF rcCode(rx + 10.0f, ry + 5.0f, rw - 16.0f, 16.0f * z);
                Gdiplus::RectF rcName(rx + 10.0f, ry + 21.0f * z, rw - 16.0f, rh - 24.0f * z);
                Gdiplus::StringFormat sf;
                sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                g.DrawString(codeW.c_str(), -1, &fCode, rcCode, &sf, &tPrimary);
                g.DrawString(nameW.c_str(), -1, &fName, rcName, &sf, &tSec);
            }
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

void PaintHorariosWeeklyGrid(HWND hwnd, TecHubState* st) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    FillSolidDcRect(memDc, w, h, st->theme.pal.contentPaper);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        DrawCleanSurfaceBox(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                            static_cast<float>(st->Sc(10)), pal.contentCard,
                            hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);

        const int leftAxisW = st->Sc(54);
        const int topHdrH = st->Sc(32);
        const int numDays = 6;
        const float colW = static_cast<float>(std::max(1, w - leftAxisW - st->Sc(8))) / static_cast<float>(numDays);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fHdr(&ff, 12.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 10.5f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush lblBr(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::Pen hdrSep(UiGlass::ToGdip(pal.separator), 1.0f);
        g.DrawLine(&hdrSep, 2.0f, static_cast<float>(topHdrH), static_cast<float>(w) - 3.0f, static_cast<float>(topHdrH));

        Gdiplus::StringFormat sfHdr;
        sfHdr.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        static const wchar_t* kDays[6] = { L"Lunes", L"Martes", L"Miércoles", L"Jueves", L"Viernes", L"Sábado" };
        for (int d = 0; d < numDays; ++d) {
            float x = static_cast<float>(leftAxisW) + d * colW;
            Gdiplus::RectF hrc(x + 2.0f, 1.0f, std::max(10.0f, colW - 4.0f), static_cast<float>(topHdrH - 2));
            g.DrawString(kDays[d], -1, &fHdr, hrc, &sfHdr, &lblBr);
        }

        if (st->horariosResult.HasSolutions() && st->selectedScheduleIdx < st->horariosResult.schedules.size()) {
            const int startHour = 7;
            const int endHour = 21;
            const float hourH = static_cast<float>(std::max(1, h - topHdrH - st->Sc(8))) / static_cast<float>(endHour - startHour);
            Gdiplus::Pen gridPen(UiGlass::ToGdip(pal.separator, hc ? 255 : (dark ? 115 : 150)), hc ? 1.25f : 1.0f);
            Gdiplus::StringFormat sfAxis;
            sfAxis.SetAlignment(Gdiplus::StringAlignmentFar);
            sfAxis.SetLineAlignment(Gdiplus::StringAlignmentCenter);

            for (int d = 0; d < numDays; ++d) {
                float x = static_cast<float>(leftAxisW) + d * colW;
                g.DrawLine(&gridPen, x, static_cast<float>(topHdrH), x, static_cast<float>(h - 4));
            }
            for (int hr = startHour; hr <= endHour; ++hr) {
                float y = static_cast<float>(topHdrH) + static_cast<float>(hr - startHour) * hourH;
                g.DrawLine(&gridPen, static_cast<float>(leftAxisW), y, static_cast<float>(w - 4), y);
                if (hr < endHour) {
                    wchar_t tbuf[16]{};
                    swprintf_s(tbuf, L"%02d:00", hr);
                    Gdiplus::RectF trc(4.0f, y + 1.0f, static_cast<float>(leftAxisW - st->Sc(10)), std::max(12.0f * dpiScale, hourH * 0.55f));
                    g.DrawString(tbuf, -1, &fSmall, trc, &sfAxis, &lblBr);
                }
            }

            const auto& sched = st->horariosResult.schedules[st->selectedScheduleIdx];
            const UiGlass::Rgba8 kAccents[6] = {
                pal.accentBlue, pal.accentGreen, pal.accentOrange, pal.accentPurple, pal.accentRed, pal.accentTeal
            };
            Gdiplus::SolidBrush whiteBr(Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::StringFormat sf;
            sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

            for (size_t i = 0; i < sched.selections.size(); ++i) {
                const auto& sel = sched.selections[i];
                const auto& acc = kAccents[i % 6];
                UiGlass::Rgba8 blkFill = { acc.r, acc.g, acc.b, 232 };
                for (const auto& s : sel.group.sessions) {
                    int d = static_cast<int>(s.day);
                    if (d < 0 || d >= numDays) continue;
                    float y0 = static_cast<float>(topHdrH) + (static_cast<float>(s.startMinute) / 60.0f - startHour) * hourH;
                    float y1 = static_cast<float>(topHdrH) + (static_cast<float>(s.endMinute) / 60.0f - startHour) * hourH;
                    float x0 = static_cast<float>(leftAxisW) + d * colW + 3.0f;
                    float bw = std::max(8.0f, colW - 6.0f);
                    float bh = std::max(16.0f, y1 - y0 - 2.0f);
                    DrawCleanSurfaceBox(g, x0, y0 + 1.0f, bw, bh, 6.0f * dpiScale, blkFill, { 0, 0, 0, 0 }, 0.0f);
                    std::wstring txt = Utf8ToWide(sel.courseCodeUtf8 + " Gr " + sel.group.idUtf8 +
                                                  (s.roomUtf8.empty() ? "" : ("\n" + s.roomUtf8)));
                    Gdiplus::RectF tr(x0 + 6.0f, y0 + 3.0f, bw - 10.0f, bh - 4.0f);
                    g.DrawString(txt.c_str(), -1, &fSmall, tr, &sf, &whiteBr);
                }
            }
        } else if (!st->horariosCatalog.courses.empty()) {
            size_t totalGroups = 0;
            std::string sampleCodes;
            for (size_t i = 0; i < st->horariosCatalog.courses.size(); ++i) {
                totalGroups += st->horariosCatalog.courses[i].groups.size();
                if (i < 6) {
                    if (!sampleCodes.empty()) sampleCodes += ", ";
                    sampleCodes += st->horariosCatalog.courses[i].codeUtf8;
                }
            }
            if (st->horariosCatalog.courses.size() > 6) sampleCodes += "...";
            std::wstring titleW = L"Catálogo cargado · " + std::to_wstring(st->horariosCatalog.courses.size()) +
                                  L" cursos (" + std::to_wstring(totalGroups) + L" grupos)";
            std::wstring subW = st->horariosUsedRequiredFilter
                ? L"Ninguna combinación de los cursos solicitados está libre de choques con los filtros actuales. Ajuste los códigos o restricciones."
                : (L"Ejemplos disponibles: " + Utf8ToWide(sampleCodes) +
                   L"\nEscriba los códigos que desea llevar arriba y presione Resolver Horarios.");
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::ClockHistory,
                titleW.c_str(),
                subW.c_str());
        } else {
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::ClockHistory,
                L"Planificador de horarios sin choques",
                L"Consulte la Guía Pública del TEC por escuela, verano o carné, el Expediente TEC o un catálogo JSON y presione Resolver Horarios.");
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK MallaCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_PAINT && st) {
        PaintMallaCurricularCanvas(hwnd, st);
        return 0;
    }
    if (msg == WM_LBUTTONDOWN && st) {
        SetCapture(hwnd);
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        bool hitNode = false;
        for (const auto& nb : st->mallaBoxes) {
            if (PtInRect(&nb.rc, pt)) {
                st->selectedMallaCode = nb.code;
                hitNode = true;
                // Update inspector panel text
                std::ostringstream oss;
                oss << nb.code << " — " << nb.name << "\r\n"
                    << "Créditos: " << nb.credits << "   ·   Estado: " << nb.status << "\r\n";
                if (!nb.reqs.empty()) {
                    oss << "Requisitos: ";
                    for (size_t i = 0; i < nb.reqs.size(); ++i) oss << (i ? ", " : "") << nb.reqs[i];
                    oss << "\r\n";
                }
                if (!nb.coreqs.empty()) {
                    oss << "Correquisitos: ";
                    for (size_t i = 0; i < nb.coreqs.size(); ++i) oss << (i ? ", " : "") << nb.coreqs[i];
                    oss << "\r\n";
                }
                if (!nb.reqBy.empty()) {
                    oss << "Desbloquea: ";
                    for (size_t i = 0; i < nb.reqBy.size(); ++i) oss << (i ? ", " : "") << nb.reqBy[i];
                    oss << "\r\n";
                }
                if (st->snap.studentProfile) {
                    const std::string targetLow = SafeLowerAscii(nb.code);
                    for (const auto& gr : st->snap.studentProfile->grades) {
                        if (SafeLowerAscii(gr.id_course) != targetLow || gr.grades_record.empty()) continue;
                        oss << "Intentos registrados: ";
                        for (size_t ai = 0; ai < gr.grades_record.size(); ++ai) {
                            const auto& a = gr.grades_record[ai];
                            oss << (ai ? " | " : "") << a.year << " P" << a.period
                                << " (" << FmtDouble(a.grade, 1) << ")";
                        }
                        oss << "\r\n";
                        break;
                    }
                }
                if (st->snap.curriculumGraph) {
                    const auto cp = td::TdaTdsClient::ComputeCurriculumCriticalPath(
                        *st->snap.curriculumGraph,
                        st->snap.studentProfile ? &(*st->snap.studentProfile) : nullptr);
                    oss << "Ruta crítica a graduación: mín. " << cp.min_semesters_remaining
                        << " semestre(s) · Créditos restantes: " << cp.remaining_credits;
                    if (!cp.critical_path_codes.empty()) {
                        oss << " · Cadena: ";
                        for (size_t ci = 0; ci < cp.critical_path_codes.size() && ci < 6; ++ci) {
                            oss << (ci ? " → " : "") << cp.critical_path_codes[ci];
                        }
                    }
                    oss << "\r\n";
                }
                if (st->hMallaInspector) {
                    SetWindowTextW(st->hMallaInspector, Utf8ToWide(oss.str()).c_str());
                }
                InvalidateRect(hwnd, nullptr, FALSE);
                break;
            }
        }
        if (!hitNode) {
            st->mallaDragging = true;
            st->mallaDragStart = pt;
            st->mallaPanStartX = st->mallaPanX;
            st->mallaPanStartY = st->mallaPanY;
        }
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st && st->mallaDragging) {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        st->mallaPanX = st->mallaPanStartX + (pt.x - st->mallaDragStart.x);
        st->mallaPanY = st->mallaPanStartY + (pt.y - st->mallaDragStart.y);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        st->mallaDragging = false;
        ReleaseCapture();
        return 0;
    }
    if (msg == WM_MOUSEWHEEL && st) {
        short delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->mallaZoom = std::clamp(st->mallaZoom + (delta > 0 ? 0.12f : -0.12f), 0.55f, 1.8f);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT CALLBACK HorariosCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_PAINT && st) {
        PaintHorariosWeeklyGrid(hwnd, st);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintUnifiedCalendarGrid(HWND hwnd, TecHubState* st) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    FillSolidDcRect(memDc, w, h, st->theme.pal.contentPaper);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        DrawCleanSurfaceBox(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                            static_cast<float>(st->Sc(10)), pal.contentCard,
                            hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);

        const int leftAxisW = st->Sc(54);
        const int topHdrH = st->Sc(32);
        const int numDays = 6;
        const int startHour = 7;
        const int endHour = 21;
        const float colW = static_cast<float>(std::max(1, w - leftAxisW - st->Sc(8))) / static_cast<float>(numDays);
        const float hourH = static_cast<float>(std::max(1, h - topHdrH - st->Sc(8))) / static_cast<float>(endHour - startHour);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fHdr(&ff, 12.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 10.5f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush lblBr(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::Pen hdrSep(UiGlass::ToGdip(pal.separator), 1.0f);
        g.DrawLine(&hdrSep, 2.0f, static_cast<float>(topHdrH), static_cast<float>(w) - 3.0f, static_cast<float>(topHdrH));

        Gdiplus::StringFormat sfHdr;
        sfHdr.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        Gdiplus::StringFormat sfTrim;
        sfTrim.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        static const wchar_t* kDays[6] = { L"Lunes", L"Martes", L"Miércoles", L"Jueves", L"Viernes", L"Sábado" };
        for (int d = 0; d < numDays; ++d) {
            float x = static_cast<float>(leftAxisW) + d * colW;
            Gdiplus::RectF hrc(x + 2.0f, 1.0f, std::max(10.0f, colW - 4.0f), static_cast<float>(topHdrH - 2));
            g.DrawString(kDays[d], -1, &fHdr, hrc, &sfHdr, &lblBr);
        }

        const bool hasAnyData = !st->snap.currentSchedule.empty() || !st->snap.unifiedCalendarEvents.empty();
        int drawn = 0;
        if (hasAnyData) {
            Gdiplus::Pen gridPen(UiGlass::ToGdip(pal.separator, hc ? 255 : (dark ? 115 : 150)), hc ? 1.25f : 1.0f);
            Gdiplus::StringFormat sfAxis;
            sfAxis.SetAlignment(Gdiplus::StringAlignmentFar);
            sfAxis.SetLineAlignment(Gdiplus::StringAlignmentCenter);

            for (int d = 0; d < numDays; ++d) {
                float x = static_cast<float>(leftAxisW) + d * colW;
                g.DrawLine(&gridPen, x, static_cast<float>(topHdrH), x, static_cast<float>(h - 4));
            }
            for (int hr = startHour; hr <= endHour; ++hr) {
                float y = static_cast<float>(topHdrH) + static_cast<float>(hr - startHour) * hourH;
                g.DrawLine(&gridPen, static_cast<float>(leftAxisW), y, static_cast<float>(w - 4), y);
                if (hr < endHour) {
                    wchar_t tbuf[16]{};
                    swprintf_s(tbuf, L"%02d:00", hr);
                    Gdiplus::RectF trc(4.0f, y + 1.0f, static_cast<float>(leftAxisW - st->Sc(10)), std::max(12.0f * dpiScale, hourH * 0.55f));
                    g.DrawString(tbuf, -1, &fSmall, trc, &sfAxis, &lblBr);
                }
            }

            UiGlass::Rgba8 classFill{ pal.accentBlue.r, pal.accentBlue.g, pal.accentBlue.b, 232 };
            UiGlass::Rgba8 evalFill{ pal.accentOrange.r, pal.accentOrange.g, pal.accentOrange.b, 232 };
            Gdiplus::SolidBrush whiteBr(Gdiplus::Color(255, 255, 255, 255));

            for (const auto& cell : st->snap.currentSchedule) {
                if (cell.day_index_0_mon < 0 || cell.day_index_0_mon >= numDays) continue;
                if (cell.end_minute <= cell.start_minute) continue;
                float y0 = static_cast<float>(topHdrH) + (static_cast<float>(cell.start_minute) / 60.0f - startHour) * hourH;
                float y1 = static_cast<float>(topHdrH) + (static_cast<float>(cell.end_minute) / 60.0f - startHour) * hourH;
                float x0 = static_cast<float>(leftAxisW) + cell.day_index_0_mon * colW + 3.0f;
                float bw = std::max(8.0f, colW - 6.0f);
                float bh = std::max(16.0f * dpiScale, y1 - y0 - 2.0f);
                DrawCleanSurfaceBox(g, x0, y0 + 1.0f, bw, bh, 6.0f * dpiScale, classFill, { 0, 0, 0, 0 }, 0.0f);
                std::wstring label = Utf8ToWide((cell.course_code.empty() ? cell.raw_text : cell.course_code) +
                                                (cell.room.empty() ? "" : ("\n" + cell.room)));
                Gdiplus::RectF tr(x0 + 6.0f, y0 + 3.0f, bw - 10.0f, bh - 4.0f);
                g.DrawString(label.c_str(), -1, &fSmall, tr, &sfTrim, &whiteBr);
                ++drawn;
            }
            int slotCount[6]{};
            for (const auto& ev : st->snap.unifiedCalendarEvents) {
                int y = 0, m = 0, d = 0;
                if (sscanf_s(ev.date_iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) continue;
                std::tm tmVal{};
                tmVal.tm_year = y - 1900; tmVal.tm_mon = m - 1; tmVal.tm_mday = d; tmVal.tm_hour = 12;
                std::mktime(&tmVal);
                int wd = (tmVal.tm_wday == 0) ? 6 : (tmVal.tm_wday - 1);
                if (wd < 0 || wd >= numDays || slotCount[wd] >= 3) continue;
                float y0 = static_cast<float>(h - st->Sc(26) * (slotCount[wd] + 1) - st->Sc(6));
                float x0 = static_cast<float>(leftAxisW) + wd * colW + 3.0f;
                float bw = std::max(8.0f, colW - 6.0f);
                float bh = static_cast<float>(st->Sc(22));
                DrawCleanSurfaceBox(g, x0, y0, bw, bh, 5.0f * dpiScale, evalFill, { 0, 0, 0, 0 }, 0.0f);
                std::wstring txt = Utf8ToWide(ev.title);
                Gdiplus::RectF tr(x0 + 6.0f, y0 + 2.0f, bw - 10.0f, bh - 4.0f);
                g.DrawString(txt.c_str(), -1, &fSmall, tr, &sfTrim, &whiteBr);
                ++slotCount[wd];
                ++drawn;
            }
        }
        if (drawn == 0) {
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::CalendarGrid,
                L"Calendario semanal unificado",
                L"Sin clases ni eventos sincronizados todavía. Inicie sesión y presione Sincronizar ahora.");
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK CalendarCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_PAINT && st) {
        PaintUnifiedCalendarGrid(hwnd, st);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintGraduateAttributesCanvas(HWND hwnd, TecHubState* st) {
    PAINTSTRUCT ps{};
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc{};
    GetClientRect(hwnd, &rc);
    const int w = std::max<int>(1, rc.right - rc.left);
    const int h = std::max<int>(1, rc.bottom - rc.top);

    HDC memDc = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
    HGDIOBJ oldBmp = SelectObject(memDc, bmp);
    FillSolidDcRect(memDc, w, h, st->theme.pal.contentPaper);

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        DrawCleanSurfaceBox(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                            static_cast<float>(st->Sc(10)), pal.contentCard,
                            hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fHdr(&ff, 13.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fBody(&ff, 12.0f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

        Gdiplus::SolidBrush txtPrimary(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::SolidBrush txtSec(UiGlass::ToGdip(pal.labelSecondary));

        if (st->snap.graduateAttributes.empty()) {
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::AcademicCap,
                L"Atributos de graduación",
                L"Inicie sesión y presione Sincronizar ahora para consultar el avance por atributo en sus cursos evaluados (Inicial, Medio y Avanzado).");
        } else {
            const float padX = 18.0f * dpiScale;
            const float topY = 14.0f * dpiScale;
            const float legendH = 28.0f * dpiScale;

            Gdiplus::SolidBrush brInit(UiGlass::ToGdip(pal.accentOrange));
            Gdiplus::SolidBrush brMed(UiGlass::ToGdip(pal.accentBlue));
            Gdiplus::SolidBrush brAdv(UiGlass::ToGdip(pal.accentGreen));
            UiGlass::Rgba8 trackCol = hc ? UiGlass::Rgba8{ 226, 232, 240, 255 }
                                        : (dark ? UiGlass::Rgba8{ 42, 47, 58, 255 } : UiGlass::Rgba8{ 234, 239, 247, 255 });
            Gdiplus::Pen rowSepPen(UiGlass::ToGdip(pal.separator, 120), 1.0f);

            g.DrawString(L"Avance por Atributo de Graduación", -1, &fHdr,
                         Gdiplus::PointF(padX, topY), &txtPrimary);
            const float lxStart = static_cast<float>(w) - 260.0f * dpiScale;
            if (lxStart > 260.0f * dpiScale) {
                const float dotSz = 8.0f * dpiScale;
                const float dy = topY + 5.0f * dpiScale;
                g.FillEllipse(&brInit, lxStart, dy, dotSz, dotSz);
                g.DrawString(L"Inicial", -1, &fSmall, Gdiplus::PointF(lxStart + 12.0f * dpiScale, topY + 1.0f * dpiScale), &txtSec);
                g.FillEllipse(&brMed, lxStart + 68.0f * dpiScale, dy, dotSz, dotSz);
                g.DrawString(L"Medio", -1, &fSmall, Gdiplus::PointF(lxStart + 80.0f * dpiScale, topY + 1.0f * dpiScale), &txtSec);
                g.FillEllipse(&brAdv, lxStart + 138.0f * dpiScale, dy, dotSz, dotSz);
                g.DrawString(L"Avanzado", -1, &fSmall, Gdiplus::PointF(lxStart + 150.0f * dpiScale, topY + 1.0f * dpiScale), &txtSec);
            }

            const size_t n = st->snap.graduateAttributes.size();
            const float availH = std::max(80.0f, static_cast<float>(h) - (topY + legendH + 12.0f * dpiScale));
            const float rowH = std::clamp(availH / static_cast<float>(std::max<size_t>(1, n)),
                                          26.0f * dpiScale, 40.0f * dpiScale);
            const float nameW = std::min(320.0f * dpiScale, static_cast<float>(w) * 0.42f);
            const float pctW = 86.0f * dpiScale;
            const float barX = padX + nameW + 12.0f * dpiScale;
            const float barW = std::max(80.0f, static_cast<float>(w) - barX - pctW - padX);
            const float barH = std::min(12.0f * dpiScale, rowH * 0.40f);

            Gdiplus::StringFormat sfRow;
            sfRow.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfRow.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sfRow.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            Gdiplus::StringFormat sfPct;
            sfPct.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfPct.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sfPct.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            sfPct.SetAlignment(Gdiplus::StringAlignmentFar);

            for (size_t i = 0; i < n; ++i) {
                const float ry = topY + legendH + 4.0f * dpiScale + static_cast<float>(i) * rowH;
                if (ry + rowH > static_cast<float>(h) + 2.0f) break;
                if (i > 0) g.DrawLine(&rowSepPen, padX, ry, static_cast<float>(w) - padX, ry);
                const auto& ga = st->snap.graduateAttributes[i];

                std::wstring title = Utf8ToWide(ga.attribute_name);
                if (!ga.courses.empty()) {
                    title += L" · " + Utf8ToWide(ga.courses);
                }
                Gdiplus::RectF nRc(padX, ry, nameW, rowH);
                g.DrawString(title.c_str(), -1, &fBody, nRc, &sfRow, &txtPrimary);

                const float by = ry + (rowH - barH) * 0.5f;
                DrawCleanSurfaceBox(g, barX, by, barW, barH, barH * 0.5f, trackCol,
                                    hc ? pal.borderStrong : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.0f : 0.0f);

                double sum = ga.initial_pct + ga.medium_pct + ga.advanced_pct;
                double denom = (sum > 100.0) ? sum : 100.0;
                float wInit = static_cast<float>(std::clamp(ga.initial_pct, 0.0, denom) / denom) * barW;
                float wMed = static_cast<float>(std::clamp(ga.medium_pct, 0.0, denom) / denom) * barW;
                float wAdv = static_cast<float>(std::clamp(ga.advanced_pct, 0.0, denom) / denom) * barW;

                Gdiplus::GraphicsPath trackClip;
                UiGlass::AddRoundPath(trackClip, barX, by, barW, barH, barH * 0.5f);
                Gdiplus::GraphicsState stClip = g.Save();
                g.SetClip(&trackClip);
                float curX = barX;
                if (wInit > 0.5f) { g.FillRectangle(&brInit, curX, by, wInit, barH); curX += wInit; }
                if (wMed > 0.5f)  { g.FillRectangle(&brMed, curX, by, wMed, barH);  curX += wMed; }
                if (wAdv > 0.5f)  { g.FillRectangle(&brAdv, curX, by, wAdv, barH); }
                g.Restore(stClip);

                std::string pStr = FmtDouble(ga.evaluated_pct, 0) + "% eval.";
                Gdiplus::RectF pRc(barX + barW + 8.0f * dpiScale, ry, pctW - 8.0f * dpiScale, rowH);
                g.DrawString(Utf8ToWide(pStr).c_str(), -1, &fSmall, pRc, &sfPct, &txtSec);
            }
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK AttrCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_PAINT && st) {
        PaintGraduateAttributesCanvas(hwnd, st);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
// Data -> UI Refresh Helpers
// ============================================================================

void RefreshSummarySection(TecHubState* st) {
    if (!st->hSummaryEdit) return;
    std::ostringstream oss;
    oss << "Estado: " << (st->store->IsAuthenticated() ? "Conectado (Solo Lectura)" : "Modo sin conexión / caché local")
        << "\r\nÚltima sincronización: " << (st->snap.syncedAtUtc.empty() ? "Sin sincronizar todavía" : st->snap.syncedAtUtc)
        << "\r\nCursos y comunidades detectados: " << st->snap.communities.size() << "\r\n\r\n";

    // Próxima clase del día (desde tabla_horario)
    {
        SYSTEMTIME stNow{};
        GetLocalTime(&stNow);
        const int todayMon0 = (stNow.wDayOfWeek == 0) ? 6 : (stNow.wDayOfWeek - 1);
        const int nowMin = stNow.wHour * 60 + stNow.wMinute;
        const td::WeeklyScheduleCell* nextClass = nullptr;
        for (const auto& cell : st->snap.currentSchedule) {
            if (cell.day_index_0_mon == todayMon0 && cell.end_minute >= nowMin) {
                if (!nextClass || cell.start_minute < nextClass->start_minute) nextClass = &cell;
            }
        }
        oss << "AGENDA DE HOY Y MATRÍCULA\r\n";
        if (nextClass) {
            oss << "  • Próxima clase hoy: " << (nextClass->course_code.empty() ? nextClass->raw_text : nextClass->course_code)
                << " (" << nextClass->time_range << ")"
                << (nextClass->room.empty() ? "" : (" · Aula " + nextClass->room)) << "\r\n";
        } else {
            oss << "  • Próxima clase hoy: Sin clases pendientes registradas para hoy\r\n";
        }
        if (st->snap.expediente && !st->snap.expediente->structured.citas_matricula.empty()) {
            const auto& cm = st->snap.expediente->structured.citas_matricula.front();
            std::string cd = FormatCountdownToIso(cm.date);
            oss << "  • Próxima cita de matrícula: " << cm.date << " " << cm.time
                << " (" << cm.modality << ")" << (cd.empty() ? "" : (" · " + cd)) << "\r\n";
        }
        if (!st->snap.pendingPerformanceEvals.empty()) {
            oss << "  • Evaluación de desempeño docente pendiente: "
                << st->snap.pendingPerformanceEvals.size()
                << " curso(s) — complete en TEC Digital\r\n";
        }
        oss << "\r\n";
    }

    oss << "PRÓXIMAS ENTREGAS Y EVENTOS\r\n";
    int shown = 0;
    for (const auto& ev : st->snap.unifiedCalendarEvents) {
        if (shown++ >= 8) break;
        std::string cd = FormatCountdownToIso(ev.date_iso);
        oss << "  • " << ev.date_iso << " " << ev.start_time
            << (cd.empty() ? "" : (" [" + cd + "]"))
            << "  ·  " << ev.title << " (" << ev.calendar_name << ")\r\n";
    }
    if (shown == 0) oss << "  (Sin eventos o entregas registradas todavía)\r\n";

    oss << "\r\nAVISOS RECIENTES (NOTIFICACIONES, NOTICIAS Y SMS)\r\n";
    int nShown = 0;
    if (st->snap.notifications) {
        for (const auto& n : st->snap.notifications->notifications) {
            if (nShown++ >= 6) break;
            oss << "  • [" << (n.state == "U" ? "Nuevo" : "Leído") << "] " << n.title
                << " — " << n.text << "\r\n";
        }
    }
    for (const auto& [path, newsList] : st->snap.newsByPath) {
        for (const auto& nr : newsList) {
            if (nShown++ >= 9) break;
            oss << "  • [Noticia] " << nr.pub_date << " — " << nr.title << "\r\n";
        }
    }
    for (const auto& [path, smsList] : st->snap.smsByPath) {
        for (const auto& m : smsList) {
            if (nShown++ >= 12) break;
            oss << "  • [SMS] " << m.transaction_date << " (" << m.community << "): " << m.content << "\r\n";
        }
    }
    if (nShown == 0) oss << "  (Sin avisos recientes)\r\n";

    if (st->hSummaryEdit) SetWindowTextW(st->hSummaryEdit, Utf8ToWide(oss.str()).c_str());
    if (st->hSummaryCanvas) InvalidateRect(st->hSummaryCanvas, nullptr, FALSE);
    if (st->hBtnLogout && st->currentSection == HubSection::Resumen) {
        const bool authed = st->store->IsAuthenticated() || st->snap.sessionAuthenticated;
        ShowWindow(st->hBtnLogout, authed ? SW_SHOW : SW_HIDE);
    }
    if (st->hSessionBadge) {
        SetWindowTextW(st->hSessionBadge,
                       st->store->IsAuthenticated()
                           ? L"Sesión activa · Solo lectura"
                           : L"Sin sesión · Caché offline");
    }
    if (st->hwnd) InvalidateRect(st->hwnd, nullptr, FALSE);
}

void RefreshCourseList(TecHubState* st) {
    if (!st->hLstCourses || !st->hCmbPeriod) return;
    int selPer = static_cast<int>(SendMessageW(st->hCmbPeriod, CB_GETCURSEL, 0, 0));
    std::string periodFilter;
    if (selPer > 0) {
        wchar_t buf[128]{};
        SendMessageW(st->hCmbPeriod, CB_GETLBTEXT, selPer, reinterpret_cast<LPARAM>(buf));
        periodFilter = WideToUtf8(buf);
    }

    st->filteredCourses.clear();
    SendMessageW(st->hLstCourses, LB_RESETCONTENT, 0, 0);
    for (const auto& c : st->snap.communities) {
        if (!periodFilter.empty()) {
            if (periodFilter == "Comunidades y Cátedras") {
                if (c.type == "CLASE") continue;
            } else if (c.period != periodFilter) {
                continue;
            }
        }
        st->filteredCourses.push_back(c);
        std::string label = c.name;
        if (!c.period.empty()) label += "  [" + c.period + "]";
        SendMessageW(st->hLstCourses, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(label).c_str()));
    }
    if (!st->filteredCourses.empty()) {
        SendMessageW(st->hLstCourses, LB_SETCURSEL, 0, 0);
    }
}

void RefreshSelectedCourseDetail(TecHubState* st) {
    const int subTab = st->hCmbCourseSub ? static_cast<int>(SendMessageW(st->hCmbCourseSub, CB_GETCURSEL, 0, 0)) : 0;
    if (st->currentSection == HubSection::Cursos) {
        const bool showWhatIf = (subTab == 2);
        if (st->hEditWhatIf) ShowWindow(st->hEditWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnWhatIf) ShowWindow(st->hBtnWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseRubric) ShowWindow(st->hBtnCourseRubric, showWhatIf ? SW_SHOW : SW_HIDE);
        UpdateActiveFramedPanels(st);
        InvalidateRect(st->hwnd, nullptr, FALSE);
    }
    if (!st->hCourseDetail || !st->hLstCourses) return;
    int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
    if (idx < 0 || static_cast<size_t>(idx) >= st->filteredCourses.size()) {
        if (subTab == 1 && st->hLstCourseFiles) {
            st->currentCourseFiles.clear();
            SendMessageW(st->hLstCourseFiles, LB_RESETCONTENT, 0, 0);
            SendMessageW(st->hLstCourseFiles, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(L"Seleccione un curso o presione 'Sincronizar archivos' para explorar documentos."));
            if (st->currentSection == HubSection::Cursos) {
                ShowWindow(st->hCourseDetail, SW_HIDE);
                ShowWindow(st->hLstCourseFiles, SW_SHOW);
            }
            return;
        }
        if (st->currentSection == HubSection::Cursos) {
            if (st->hLstCourseFiles) ShowWindow(st->hLstCourseFiles, SW_HIDE);
            ShowWindow(st->hCourseDetail, SW_SHOW);
        }
        SetWindowTextW(st->hCourseDetail, L"Seleccione un curso o comunidad de la lista para ver sus detalles.");
        return;
    }
    const auto& course = st->filteredCourses[static_cast<size_t>(idx)];

    auto ovIt = st->snap.overviewsByPath.find(course.url);
    const td::CourseOverviewSnapshot* ov = (ovIt != st->snap.overviewsByPath.end()) ? &ovIt->second : nullptr;
    auto evIt = st->snap.evaluationsByPath.find(course.url);
    const td::DetailedEvaluationsReport* ev = (evIt != st->snap.evaluationsByPath.end()) ? &evIt->second : nullptr;

    std::ostringstream oss;
    oss << course.name << "\r\nPeriodo: " << course.period << "   ·   Tipo: " << course.type << "\r\n\r\n";

    if (subTab == 0) { // Portada y Cronograma
        if (!ov) {
            oss << "Presione Sincronizar para cargar portada, docente, programa y enlaces del curso.\r\n";
        } else {
            if (!ov->teacher.office.empty() || !ov->teacher.teams_handle.empty() || !ov->teacher.consultation_hours.empty()) {
                oss << "Docente y consulta\r\n";
                if (!ov->teacher.office.empty()) oss << "  Oficina: " << ov->teacher.office << "\r\n";
                if (!ov->teacher.teams_handle.empty()) oss << "  Teams: " << ov->teacher.teams_handle << "\r\n";
                for (const auto& h : ov->teacher.consultation_hours) oss << "  Horario de consulta: " << h << "\r\n";
                oss << "\r\n";
            }
            if (!ov->official_docs.empty()) {
                oss << "Documentos oficiales\r\n";
                for (const auto& d : ov->official_docs) {
                    oss << "  • [" << d.category << "] " << d.label << "  →  " << d.url << "\r\n";
                }
                oss << "\r\n";
            }
            if (!ov->weekly_sections.empty()) {
                oss << "Semanas / Cronograma\r\n";
                for (const auto& w : ov->weekly_sections) oss << "  • " << w << "\r\n";
                oss << "\r\n";
            }
            if (!ov->external_links.empty()) {
                oss << "Enlaces y recursos externos\r\n";
                for (const auto& l : ov->external_links) {
                    oss << "  • [" << l.category << "] " << l.label << "\r\n";
                }
                oss << "\r\n";
            }
            std::vector<const td::CourseTabInfo*> customTabs;
            for (const auto& t : ov->tabs) {
                if (t.is_custom) customTabs.push_back(&t);
            }
            auto ctIt = st->snap.customTabResourcesByCourse.find(course.url);
            if (!customTabs.empty() || (ctIt != st->snap.customTabResourcesByCourse.end() && !ctIt->second.empty())) {
                oss << "Pestañas y recursos adicionales\r\n";
                for (const auto* t : customTabs) {
                    oss << "  • Pestaña: " << t->label << "\r\n";
                }
                if (ctIt != st->snap.customTabResourcesByCourse.end()) {
                    for (const auto& rl : ctIt->second) {
                        oss << "  • [" << rl.category << "] " << rl.label << "  →  " << rl.url << "\r\n";
                    }
                }
            }
        }
    } else if (subTab == 1) { // Documentos (árbol interactivo en hLstCourseFiles)
        st->currentCourseFiles.clear();
        if (st->hLstCourseFiles) SendMessageW(st->hLstCourseFiles, LB_RESETCONTENT, 0, 0);
        auto fIt = st->snap.foldersByPath.find(course.url);
        if (fIt != st->snap.foldersByPath.end() && !fIt->second.files.empty()) {
            for (const auto& f : fIt->second.files) {
                st->currentCourseFiles.push_back(f);
                const bool isLocal = (f.status == td::FileStatus::Downloaded || f.deduplicated);
                std::string row = (isLocal ? "[Local] " : "[Nube]  ");
                if (!f.folder_path.empty()) row += f.folder_path + " / ";
                row += f.name + "  (" + std::to_string(f.content_size / 1024) + " KB)";
                if (f.plagiarism_percentage >= 0.0) {
                    row += "  ·  Similitud: " + FmtDouble(f.plagiarism_percentage, 1) + "%";
                }
                if (st->hLstCourseFiles) {
                    SendMessageW(st->hLstCourseFiles, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(row).c_str()));
                }
            }
            if (st->hLstCourseFiles) SendMessageW(st->hLstCourseFiles, LB_SETCURSEL, 0, 0);
        } else if (st->hLstCourseFiles) {
            SendMessageW(st->hLstCourseFiles, LB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(L"Presione 'Sincronizar archivos' para explorar y descargar los materiales de este curso."));
        }
        if (st->currentSection == HubSection::Cursos) {
            if (st->hCourseDetail) ShowWindow(st->hCourseDetail, SW_HIDE);
            if (st->hLstCourseFiles) ShowWindow(st->hLstCourseFiles, SW_SHOW);
        }
        return;
    } else if (subTab == 2) { // Evaluaciones y Simulador
        if (!ev || ev->categories.empty()) {
            oss << "No hay evaluaciones cargadas para este espacio.\r\n";
        } else {
            double target = 70.0;
            std::map<std::string, double> hypOverrides;
            if (st->hEditWhatIf) {
                std::string tStr = WideToUtf8(GetCtrlTextW(st->hEditWhatIf));
                size_t comma = tStr.find(',');
                double parsed = std::atof(tStr.c_str());
                if (parsed >= 1.0 && parsed <= 100.0) target = parsed;
                if (comma != std::string::npos) {
                    double hyp = std::atof(tStr.substr(comma + 1).c_str());
                    if (hyp >= 0.0 && hyp <= 100.0) {
                        for (const auto& cat : ev->categories) {
                            for (const auto& it : cat.items) {
                                if (!it.is_graded && !it.assign_id.empty()) {
                                    hypOverrides[it.assign_id] = hyp;
                                    break;
                                }
                            }
                            if (!hypOverrides.empty()) break;
                        }
                    }
                }
            }
            const auto sc = td::ComputeGradeWhatIf(*ev, target, hypOverrides);
            oss << "Resumen de nota y simulador (Meta: " << FmtDouble(target, 1)
                << " · Use '70,85' para simular 85 en la próxima entrega)\r\n"
                << "  Acumulado actual: " << FmtDouble(sc.current_earned_pct, 2) << " / " << FmtDouble(sc.total_weight_pct, 1) << "%\r\n"
                << "  Evaluado: " << FmtDouble(sc.evaluated_weight_pct, 1) << "%   ·   Restante: " << FmtDouble(sc.remaining_weight_pct, 1) << "%\r\n"
                << "  Nota máxima alcanzable: " << FmtDouble(sc.max_possible_grade, 2) << "\r\n";
            if (!hypOverrides.empty()) {
                oss << "  Proyección con escenario simulado: " << FmtDouble(sc.projected_final_with_overrides, 2) << "\r\n";
            }
            oss << "  Requerido en evaluaciones restantes: "
                << (sc.already_reached ? "Meta ya alcanzada"
                                       : (sc.mathematically_possible ? (FmtDouble(sc.required_avg_on_remaining_100, 1) + " / 100")
                                                                     : "No alcanzable matemáticamente (> 100)"))
                << "\r\n\r\nDesglose por rubro\r\n";
            for (const auto& cat : ev->categories) {
                oss << "\r\n[" << cat.name << "]  " << FmtDouble(cat.earned_pct, 2) << " / " << FmtDouble(cat.weight_pct, 1) << "%\r\n";
                for (const auto& it : cat.items) {
                    oss << "  • " << it.name << " (" << FmtDouble(it.weight_pct, 2) << "%) — ";
                    if (it.is_graded) oss << "Nota: " << FmtDouble(it.raw_grade_100, 1) << "/100";
                    else oss << it.submission_status;
                    if (!it.due_date_text.empty() && it.due_date_text.find("no definida") == std::string::npos) {
                        oss << "  ·  Entrega: " << it.due_date_text;
                    }
                    if (it.has_rubric) oss << "  [Rúbrica]";
                    auto ehIt = st->snap.evalHistoryByAssignId.find(it.assign_id.empty() ? it.task_item_id : it.assign_id);
                    if (ehIt != st->snap.evalHistoryByAssignId.end() && !ehIt->second.empty()) {
                        oss << "  [" << ehIt->second.size() << " entrega(s)]";
                    }
                    if (!it.teacher_comment.empty()) oss << "\r\n      Comentario: " << it.teacher_comment;
                    oss << "\r\n";
                }
            }
        }
    } else if (subTab == 3) { // Actividades (GAAP) y Foros
        oss << "Actividades de aprendizaje (GAAP)\r\n";
        auto gpIt = st->snap.gaapByPath.find(course.url);
        if (gpIt != st->snap.gaapByPath.end() && !gpIt->second.empty()) {
            for (const auto& g : gpIt->second) {
                oss << "  • " << g.title;
                if (!g.date_text.empty()) oss << "  (" << g.date_text << ")";
                if (g.score >= 0.0) oss << "  —  Puntaje: " << FmtDouble(g.score, 1);
                if (!g.status.empty()) oss << "  [" << g.status << "]";
                oss << "\r\n";
            }
        } else {
            oss << "  (Sin historial GAAP registrado para este curso)\r\n";
        }
        oss << "\r\nForos del curso (Solo lectura)\r\n";
        auto frIt = st->snap.forumsByPath.find(course.url);
        if (frIt != st->snap.forumsByPath.end() && !frIt->second.empty()) {
            for (const auto& th : frIt->second) {
                oss << "  • " << th.subject << "\r\n";
            }
        } else {
            oss << "  (Sin hilos de foro activos)\r\n";
        }
    } else { // Personas y Avisos
        oss << "Equipo docente e integrantes\r\n";
        auto mbIt = st->snap.membersByPath.find(course.url);
        if (mbIt != st->snap.membersByPath.end() && !mbIt->second.empty()) {
            int shownM = 0;
            for (const auto& m : mbIt->second) {
                if (shownM++ >= 40) {
                    oss << "  ... y " << (mbIt->second.size() - 40) << " integrante(s) más\r\n";
                    break;
                }
                const std::string displayM = !m.full_name.empty() ? m.full_name : (m.first_name + " " + m.last_name);
                oss << "  • " << displayM;
                if (!m.role.empty()) oss << "  [" << m.role << "]";
                oss << "\r\n";
            }
        } else {
            oss << "  (Sincronice para cargar la lista de participantes)\r\n";
        }
        oss << "\r\nNoticias y mensajes del curso\r\n";
        auto nrIt = st->snap.newsByPath.find(course.url);
        if (nrIt != st->snap.newsByPath.end() && !nrIt->second.empty()) {
            for (const auto& n : nrIt->second) {
                oss << "  • [" << n.pub_date << "] " << n.title << "\r\n";
            }
        } else {
            oss << "  (Sin noticias publicadas)\r\n";
        }
        auto smIt = st->snap.smsByPath.find(course.url);
        if (smIt != st->snap.smsByPath.end() && !smIt->second.empty()) {
            oss << "\r\nMensajes urgentes (SMS):\r\n";
            for (const auto& m : smIt->second) {
                oss << "  • [" << m.transaction_date << "] " << m.sent_by << ": " << m.content << "\r\n";
            }
        }
    }

    if (st->currentSection == HubSection::Cursos) {
        if (st->hLstCourseFiles) ShowWindow(st->hLstCourseFiles, SW_HIDE);
        if (st->hCourseDetail) ShowWindow(st->hCourseDetail, SW_SHOW);
    }
    SetWindowTextW(st->hCourseDetail, Utf8ToWide(oss.str()).c_str());
}

void RefreshCalendarSection(TecHubState* st) {
    if (!st->hCalList) return;
    int filterIdx = st->hCmbCalFilter ? static_cast<int>(SendMessageW(st->hCmbCalFilter, CB_GETCURSEL, 0, 0)) : 0;
    std::ostringstream oss;
    oss << "Calendario académico unificado (" << st->snap.unifiedCalendarEvents.size() << " registros)\r\n\r\n";
    int count = 0;
    for (const auto& ev : st->snap.unifiedCalendarEvents) {
        if (filterIdx == 1 && ev.source_kind != "evaluation") continue;
        if (filterIdx == 2 && ev.source_kind != "matricula") continue;
        if (filterIdx == 3 && ev.source_kind != "calendar") continue;
        oss << ev.date_iso << "  " << (ev.start_time.empty() ? "--:--" : ev.start_time)
            << "   [" << ev.source_kind << "]   " << ev.title;
        if (!ev.calendar_name.empty()) oss << "  ·  " << ev.calendar_name;
        if (!ev.description.empty()) oss << "\r\n    " << ev.description;
        oss << "\r\n\r\n";
        ++count;
    }
    if (count == 0) oss << "No hay eventos para el filtro seleccionado.\r\n";
    SetWindowTextW(st->hCalList, Utf8ToWide(oss.str()).c_str());
}

std::string FormatGraduateAttributesInspectorText(const TecHubState* st) {
    if (!st || st->snap.graduateAttributes.empty()) {
        return "Atributos de Graduación: inicie sesión y presione Sincronizar ahora para consultar el avance por atributo (Inicial, Medio y Avanzado).";
    }
    const auto& attrs = st->snap.graduateAttributes;
    double avgEval = 0.0;
    double avgAdv = 0.0;
    for (const auto& a : attrs) {
        avgEval += a.evaluated_pct;
        avgAdv += a.advanced_pct;
    }
    avgEval /= static_cast<double>(attrs.size());
    avgAdv /= static_cast<double>(attrs.size());

    std::ostringstream oss;
    oss << "Atributos de Graduación (" << attrs.size() << " registrados)"
        << " · Promedio evaluado: " << FmtDouble(avgEval, 1) << "%"
        << " · Nivel avanzado medio: " << FmtDouble(avgAdv, 1) << "%\r\n";
    for (size_t i = 0; i < attrs.size() && i < 4; ++i) {
        const auto& a = attrs[i];
        oss << "  • " << a.attribute_name << ": "
            << FmtDouble(a.evaluated_pct, 0) << "% evaluado (Inicial "
            << FmtDouble(a.initial_pct, 0) << "% · Medio "
            << FmtDouble(a.medium_pct, 0) << "% · Avanzado "
            << FmtDouble(a.advanced_pct, 0) << "%)";
        if (!a.courses.empty()) oss << " · Cursos: " << a.courses;
        oss << "\r\n";
    }
    if (attrs.size() > 4) {
        oss << "  ... y " << (attrs.size() - 4) << " atributo(s) más mostrados en el gráfico superior.";
    }
    return oss.str();
}

void RefreshExpedienteSection(TecHubState* st) {
    if (!st->hExpReport) return;
    int gradeFilter = st->hCmbGradesFilter ? static_cast<int>(SendMessageW(st->hCmbGradesFilter, CB_GETCURSEL, 0, 0)) : 0;
    std::ostringstream oss;
    if (!st->snap.expediente) {
        oss << "Expediente aún no sincronizado. Inicie sesión y presione Sincronizar.\r\n";
    } else {
        const auto& ex = st->snap.expediente->structured;
        const auto& s = ex.stats;
        oss << "Estadísticas académicas\r\n"
            << "  Promedio ponderado general: " << FmtDouble(s.weighted_avg_general, 2)
            << "   ·   Aprobados: " << FmtDouble(s.weighted_avg_passed, 2)
            << "   ·   Reprobados: " << FmtDouble(s.weighted_avg_failed, 2)
            << "   ·   Horas: " << s.hours_taken << "\r\n"
            << "  Créditos aprobados: " << s.credits_passed
            << "   ·   Reprobados: " << s.credits_failed
            << "   ·   Sin nota: " << s.credits_no_grade
            << "   ·   Total: " << s.credits_total << "\r\n"
            << "  Desglose 8 estados: Aprobadas=" << s.count_aprobadas
            << " | En curso=" << s.count_en_curso
            << " | Reprobadas=" << s.count_reprobadas
            << " | Retiradas=" << s.count_retiradas
            << " | Reconocidas=" << s.count_reconocidas
            << " | Suficiencia=" << s.count_suficiencia
            << " | Pendientes=" << s.count_pendientes
            << " | Congeladas=" << s.count_congeladas << "\r\n\r\n";

        if (!ex.weighted_averages.empty()) {
            oss << "Promedio ponderado por periodo\r\n";
            for (const auto& w : ex.weighted_averages) {
                oss << "  • " << w.year << " P" << w.period << " (" << w.modality << "): " << FmtDouble(w.weighted_avg, 2) << "\r\n";
            }
            oss << "\r\n";
        }

        if (!ex.grades.empty()) {
            oss << "Registro de notas\r\n";
            for (const auto& g : ex.grades) {
                std::string lowSt = SafeLowerAscii(g.state);
                if (gradeFilter == 1 && lowSt.find("aprob") == std::string::npos) continue;
                if (gradeFilter == 2 && lowSt.find("reprob") == std::string::npos) continue;
                oss << "  • " << g.year << " P" << g.period << "  [" << g.state << "]  "
                    << g.grade_text << "  —  " << (g.course_code.empty() ? "" : (g.course_code + " "))
                    << g.course_name << " (" << g.credits << " cr)\r\n";
            }
            oss << "\r\n";
        }

        if (!ex.citas_matricula.empty()) {
            oss << "Citas de matrícula\r\n";
            for (const auto& c : ex.citas_matricula) {
                oss << "  • " << c.date << " " << c.time << "  —  Periodo " << c.period
                    << " (" << c.modality << ")\r\n";
            }
        }
    }
    SetWindowTextW(st->hExpReport, Utf8ToWide(oss.str()).c_str());

    int expMode = st->hCmbExpView ? static_cast<int>(SendMessageW(st->hCmbExpView, CB_GETCURSEL, 0, 0)) : 0;
    if (st->hMallaInspector) {
        if (expMode == 2) {
            SetWindowTextW(st->hMallaInspector, Utf8ToWide(FormatGraduateAttributesInspectorText(st)).c_str());
        } else if (st->snap.curriculumGraph && st->selectedMallaCode.empty()) {
            const auto cp = td::TdaTdsClient::ComputeCurriculumCriticalPath(
                *st->snap.curriculumGraph,
                st->snap.studentProfile ? &(*st->snap.studentProfile) : nullptr);
            std::ostringstream cpOss;
            cpOss << "Plan: " << st->snap.curriculumGraph->dsc_curriculum
                  << " · Avance: " << cp.approved_courses << "/" << cp.total_courses
                  << " cursos (" << cp.completed_credits << " cr aprobados, " << cp.remaining_credits << " cr restantes)\r\n"
                  << "Ruta crítica estimada: mín. " << cp.min_semesters_remaining << " semestre(s)";
            if (!cp.critical_path_codes.empty()) {
                cpOss << " · Cadena: ";
                for (size_t i = 0; i < cp.critical_path_codes.size() && i < 6; ++i) {
                    cpOss << (i ? " → " : "") << cp.critical_path_codes[i];
                }
            }
            if (!cp.unlocked_next_codes.empty()) {
                cpOss << "\r\nCursos desbloqueados disponibles (" << cp.unlocked_next_codes.size() << "): ";
                for (size_t i = 0; i < cp.unlocked_next_codes.size() && i < 8; ++i) {
                    cpOss << (i ? ", " : "") << cp.unlocked_next_codes[i];
                }
            }
            if (!st->snap.pendingGraduationCourses.empty()) {
                int pendCred = 0;
                for (const auto& pc : st->snap.pendingGraduationCourses) pendCred += std::max(0, pc.credits);
                cpOss << "\r\nPendientes oficiales de graduación (" << st->snap.pendingGraduationCourses.size()
                      << " cursos · " << pendCred << " cr): ";
                for (size_t i = 0; i < st->snap.pendingGraduationCourses.size() && i < 8; ++i) {
                    const auto& pc = st->snap.pendingGraduationCourses[i];
                    cpOss << (i ? ", " : "") << (!pc.code.empty() ? pc.code : pc.name);
                }
            }
            SetWindowTextW(st->hMallaInspector, Utf8ToWide(cpOss.str()).c_str());
        } else if (!st->snap.curriculumGraph && !st->snap.pendingGraduationCourses.empty()) {
            std::ostringstream pgOss;
            pgOss << "Cursos pendientes oficiales de graduación (" << st->snap.pendingGraduationCourses.size() << "): ";
            for (size_t i = 0; i < st->snap.pendingGraduationCourses.size() && i < 10; ++i) {
                const auto& pc = st->snap.pendingGraduationCourses[i];
                pgOss << (i ? ", " : "") << pc.code << (pc.name.empty() ? "" : (" " + pc.name));
            }
            SetWindowTextW(st->hMallaInspector, Utf8ToWide(pgOss.str()).c_str());
        } else if (expMode == 1 && st->selectedMallaCode.empty()) {
            SetWindowTextW(st->hMallaInspector,
                           L"Haga clic en una materia para resaltar requisitos, correquisitos y cursos que desbloquea. Arrastre para desplazar o use la rueda para zoom.");
        }
    }
    if (st->hMallaCanvas) InvalidateRect(st->hMallaCanvas, nullptr, FALSE);
    if (st->hAttrCanvas) InvalidateRect(st->hAttrCanvas, nullptr, FALSE);
}

void RefreshTramitesSection(TecHubState* st) {
    if (!st->hTramiteReport) return;
    int cat = st->hCmbTramite ? static_cast<int>(SendMessageW(st->hCmbTramite, CB_GETCURSEL, 0, 0)) : 0;
    std::ostringstream oss;
    if (cat == 0) {
        oss << "Control de plagio (Solo lectura)\r\n\r\n";
        if (st->snap.plagiarismTree) {
            oss << "Carpeta personal: " << st->snap.plagiarismTree->my_files_name
                << " (ID: " << st->snap.plagiarismTree->my_files_folder_id << ")\r\n"
                << "Documentos analizados: " << st->snap.plagiarismTree->documents.size() << "\r\n";
            for (const auto& d : st->snap.plagiarismTree->documents) {
                oss << "  • " << d.file_name << " — " << d.status_text;
                if (d.similarity_pct >= 0.0) oss << " (" << FmtDouble(d.similarity_pct, 1) << "%)";
                oss << "\r\n";
            }
        } else {
            oss << "Sincronice para consultar el estado de revisión de documentos.\r\n";
        }
    } else if (cat == 1) {
        oss << "Prematrícula, inscripciones e inclusiones\r\n\r\n";
        oss << "Gestor de inclusiones\r\n"
            << "  Estado del servicio: "
            << (st->snap.inclusionServiceStatus == "active"
                    ? "Activo"
                    : "Inactivo o fuera de periodo de inclusiones")
            << "\r\n";
        if (st->snap.inclusionCampaign) {
            const auto& ic = *st->snap.inclusionCampaign;
            oss << "  Campaña: " << (ic.name.empty() ? ic.id : ic.name);
            if (!ic.period.empty()) oss << "  ·  Periodo: " << ic.period;
            if (!ic.student_entity.empty()) oss << "  ·  Escuela: " << ic.student_entity;
            oss << "\r\n";
            if (!ic.start_date.empty() || !ic.end_date.empty()) {
                oss << "  Ventana estudiantil: " << ic.start_date << " — " << ic.end_date << "\r\n";
            }
        }
        if (st->snap.scholarshipInfo) {
            const auto& sc = *st->snap.scholarshipInfo;
            oss << "  Estado de beca: " << (sc.status.empty() ? "Consultado" : sc.status);
            if (!sc.scholarship_type.empty()) oss << " (" << sc.scholarship_type << ")";
            if (!sc.detail.empty()) oss << " — " << sc.detail;
            oss << "\r\n";
        }
        if (!st->snap.inclusionApplications.empty()) {
            oss << "  Solicitudes registradas (" << st->snap.inclusionApplications.size() << "):\r\n";
            for (const auto& a : st->snap.inclusionApplications) {
                oss << "    • " << a.course_code;
                if (!a.course_name.empty()) oss << " — " << a.course_name;
                if (!a.group.empty()) oss << " [Gr " << a.group << "]";
                if (!a.status.empty()) oss << " · Estado: " << a.status;
                if (!a.comment.empty()) oss << " (" << a.comment << ")";
                oss << "\r\n";
            }
        }
        oss << "\r\nEvaluación de desempeño docente\r\n";
        if (!st->snap.pendingPerformanceEvals.empty()) {
            oss << "  Evaluaciones pendientes (" << st->snap.pendingPerformanceEvals.size() << "):\r\n";
            for (const auto& pe : st->snap.pendingPerformanceEvals) {
                oss << "    • " << (!pe.course_name.empty() ? pe.course_name : pe.form_name);
                if (!pe.teacher_name.empty()) oss << " — " << pe.teacher_name;
                if (!pe.status.empty()) oss << " [" << pe.status << "]";
                if (!pe.deadline.empty()) oss << " · Límite: " << pe.deadline;
                oss << "\r\n";
            }
            oss << "  Para completar el formulario oficial, seleccione 'Servicios TEC...' → Evaluación de Desempeño Docente.\r\n";
        } else {
            oss << "  Sin evaluaciones de desempeño docente pendientes en este momento.\r\n";
        }
        oss << "\r\nPrematrícula\r\n";
        if (st->snap.prematricula) {
            oss << "  Máximo de créditos: " << st->snap.prematricula->max_credits
                << " · Cursos registrados: " << st->snap.prematricula->courses.size() << "\r\n";
            for (const auto& c : st->snap.prematricula->courses) {
                oss << "  • " << c.course_code << " (Prioridad " << c.priority << ")\r\n";
            }
        } else {
            oss << "  Consulte en el portal cuando exista periodo abierto.\r\n";
        }
        oss << "\r\nGestor de inscripción\r\n  "
            << (st->snap.gestorInscripcionEventsJson.empty() ? "Sin convocatorias activas." : "Convocatorias cargadas en caché.")
            << "\r\n";
        if (st->snap.feriaNegocios) {
            oss << "\r\nFeria de Ideas de Negocios\r\n  "
                << st->snap.feriaNegocios->categories.size() << " categorías disponibles";
            if (!st->snap.feriaNegocios->student_location.name.empty()) {
                oss << " · Sede: " << st->snap.feriaNegocios->student_location.name;
            }
            oss << "\r\n";
        }
    } else if (cat == 2) {
        oss << "Suscripciones y perfil de aprendizaje\r\n\r\n";
        if (!st->snap.notificationSubscriptions.empty()) {
            oss << "Suscripciones activas (" << st->snap.notificationSubscriptions.size() << "):\r\n";
            for (const auto& s : st->snap.notificationSubscriptions) {
                oss << "  • " << s.community << " — " << s.notice_type << " (" << s.frequency << ")\r\n";
            }
            oss << "\r\n";
        }
        if (st->snap.userModel && !st->snap.userModel->dimensions.empty()) {
            oss << "Perfil de aprendizaje y accesibilidad (local y privado):\r\n";
            for (const auto& d : st->snap.userModel->dimensions) {
                oss << "  • " << d.title << ": ";
                for (size_t i = 0; i < d.metrics.size() && i < 5; ++i) {
                    oss << (i ? " · " : "") << d.metrics[i].first << "=" << d.metrics[i].second;
                }
                oss << "\r\n";
            }
        }
        if (!st->snap.schoolCatalog.empty()) {
            oss << "\r\nEscuelas y departamentos TEC (" << st->snap.schoolCatalog.size() << "):\r\n";
            for (size_t i = 0; i < st->snap.schoolCatalog.size() && i < 15; ++i) {
                oss << "  • " << st->snap.schoolCatalog[i].name << "\r\n";
            }
        }
    } else {
        oss << "Directorio de servicios institucionales TEC\r\n\r\n"
            << "  • Soporte TEC Digital: +506 2550-2069 · L-V 7:30 am – 4:30 pm\r\n"
            << "  • Mapa del Campus Central Cartago (PDF oficial)\r\n"
            << "  • Guía para Estudiantes y Guía para Profesores\r\n"
            << "  • Admisión y Registro (DAR) · KapiBOT\r\n"
            << "  • Biblioteca · Becas · PAySE · PSED-NE (Apoyos Educativos)\r\n\r\n"
            << "Seleccione 'Servicios TEC...' para abrir cualquier recurso en su navegador.\r\n";
    }
    SetWindowTextW(st->hTramiteReport, Utf8ToWide(oss.str()).c_str());
}

void RefreshCacheUsageLabel(TecHubState* st) {
    if (!st->hLblCache) return;
    double mb = static_cast<double>(st->store->ComputeOfflineCacheBytes()) / (1024.0 * 1024.0);
    double maxMb = static_cast<double>(st->store->OfflineBudgetBytes()) / (1024.0 * 1024.0);
    std::string txt = "Caché offline: " + FmtDouble(mb, 1) + " MB / " + FmtDouble(maxMb, 0) + " MB";
    SetWindowTextW(st->hLblCache, Utf8ToWide(txt).c_str());
}

void SeedDefaultPublicEscuelas(TecHubState* st) {
    if (!st->publicEscuelas.empty()) return;
    st->publicEscuelas = {
        { "CA",  "Ingeniería en Computación" },
        { "MA",  "Escuela de Matemática" },
        { "FI",  "Escuela de Física" },
        { "QU",  "Escuela de Química" },
        { "E",   "Ingeniería en Electrónica" },
        { "CES", "Ingeniería en Computadores" },
        { "MI",  "Ingeniería Electromecánica" },
        { "PI",  "Ingeniería en Producción Industrial" },
        { "CO",  "Ingeniería en Construcción" },
        { "EMT", "Ingeniería en Mecatrónica" },
        { "ME",  "Ciencia e Ing. de los Materiales" },
        { "BI",  "Escuela de Biología" },
        { "AE",  "Administración de Empresas" },
        { "AIT", "Administración de Tecnologías (ATI)" },
        { "DI",  "Diseño Industrial" },
        { "IA",  "Ingeniería Agrícola" },
        { "AA",  "Ing. Agropecuaria Administrativa" },
        { "FO",  "Ingeniería Forestal" },
        { "SHO", "Seguridad Laboral e Higiene Ambiental" },
        { "CI",  "Ciencias del Lenguaje" },
        { "FH",  "Formación Humanística" },
        { "AMB", "Ingeniería Ambiental" }
    };
}

void RefreshPublicEscuelasCombo(TecHubState* st) {
    if (!st || !st->hCmbHorEscuela) return;
    std::string prevCode;
    int prevIdx = static_cast<int>(SendMessageW(st->hCmbHorEscuela, CB_GETCURSEL, 0, 0));
    if (prevIdx >= 0 && static_cast<size_t>(prevIdx) < st->publicEscuelas.size()) {
        prevCode = st->publicEscuelas[static_cast<size_t>(prevIdx)].ide_depto;
    }
    if (!st->snap.publicGuiaEscuelas.empty()) {
        st->publicEscuelas = st->snap.publicGuiaEscuelas;
    }
    SeedDefaultPublicEscuelas(st);
    SendMessageW(st->hCmbHorEscuela, CB_RESETCONTENT, 0, 0);
    int restoreIdx = 0;
    for (size_t i = 0; i < st->publicEscuelas.size(); ++i) {
        const auto& e = st->publicEscuelas[i];
        std::string label = e.ide_depto.empty() ? e.dsc_depto : (e.ide_depto + " — " + e.dsc_depto);
        SendMessageW(st->hCmbHorEscuela, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(label).c_str()));
        if (!prevCode.empty() && e.ide_depto == prevCode) {
            restoreIdx = static_cast<int>(i);
        }
    }
    if (!st->publicEscuelas.empty()) {
        SendMessageW(st->hCmbHorEscuela, CB_SETCURSEL, static_cast<WPARAM>(restoreIdx), 0);
    }
}

void SyncHorariosSourceVisibility(TecHubState* st) {
    if (!st) return;
    int horSrc = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
    if (horSrc < 0 || horSrc > 3) horSrc = 0;
    auto sw = [](HWND h, bool show) {
        if (h) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
    };
    sw(st->hCmbHorSource, true);
    sw(st->hCmbHorEscuela, horSrc == 0);
    sw(st->hEditHorCarnet, horSrc == 2);
    sw(st->hCmbHorYear, horSrc == 0 || horSrc == 1 || horSrc == 2);
    sw(st->hCmbHorPubSede, horSrc == 0 || horSrc == 1 || horSrc == 2);
    sw(st->hCmbHorPubPer, horSrc == 0 || horSrc == 2);
    sw(st->hCmbHorSede, horSrc == 3);
    sw(st->hCmbHorCarrera, horSrc == 3);
    sw(st->hCmbHorPeriodo, horSrc == 3);
}

std::string TrimDialogUtf8(std::string_view sv) {
    size_t a = 0;
    while (a < sv.size() && std::isspace(static_cast<unsigned char>(sv[a]))) ++a;
    size_t b = sv.size();
    while (b > a && std::isspace(static_cast<unsigned char>(sv[b - 1]))) --b;
    return std::string(sv.substr(a, b - a));
}

std::string FoldAccentsLowerDialog(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        const auto c = static_cast<unsigned char>(sv[i]);
        if (c == 0xC3u && i + 1 < sv.size()) {
            const auto n = static_cast<unsigned char>(sv[i + 1]);
            switch (n) {
                case 0xA1u: case 0x81u: out.push_back('a'); ++i; continue;
                case 0xA9u: case 0x89u: out.push_back('e'); ++i; continue;
                case 0xADu: case 0x8Du: out.push_back('i'); ++i; continue;
                case 0xB3u: case 0x93u: out.push_back('o'); ++i; continue;
                case 0xBAu: case 0x9Au: case 0xBCu: case 0x9Cu: out.push_back('u'); ++i; continue;
                case 0xB1u: case 0x91u: out.push_back('n'); ++i; continue;
                default: break;
            }
        }
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

bool IsTecCourseCodeToken(std::string_view tok) {
    if (tok.size() < 5 || tok.size() > 8) return false;
    size_t alpha = 0;
    while (alpha < tok.size() && std::isalpha(static_cast<unsigned char>(tok[alpha]))) ++alpha;
    size_t digits = 0;
    while (alpha + digits < tok.size() && std::isdigit(static_cast<unsigned char>(tok[alpha + digits]))) ++digits;
    return (alpha + digits == tok.size()) && alpha >= 2 && alpha <= 4 && digits >= 3 && digits <= 4;
}

void BuildHorariosPrefsAndBusyFromUi(
    TecHubState* st,
    ht::SolverPreferences& prefs,
    std::vector<ht::BusySlot>& busy,
    bool& filterPrereq,
    bool& includeBusy,
    bool& hasReqFilter) {
    prefs = ht::SolverPreferences{};
    prefs.maxResults = 30;
    prefs.filters.excludeNightClasses = st->hChkHorNoNight && (SendMessageW(st->hChkHorNoNight, BM_GETCHECK, 0, 0) == BST_CHECKED);
    prefs.filters.requireAvailableSeats = st->hChkHorSeats && (SendMessageW(st->hChkHorSeats, BM_GETCHECK, 0, 0) == BST_CHECKED);
    filterPrereq = st->hChkHorPrereq && (SendMessageW(st->hChkHorPrereq, BM_GETCHECK, 0, 0) == BST_CHECKED);
    includeBusy = st->hChkHorBusy && (SendMessageW(st->hChkHorBusy, BM_GETCHECK, 0, 0) == BST_CHECKED);

    if (st->hEditHorFilter) {
        std::string ftxt = TrimDialogUtf8(WideToUtf8(GetCtrlTextW(st->hEditHorFilter)));
        if (!ftxt.empty()) {
            std::vector<std::string> chunks;
            size_t pos = 0;
            while (pos <= ftxt.size()) {
                size_t sep = ftxt.find_first_of(",;", pos);
                if (sep == std::string::npos) sep = ftxt.size();
                std::string chunk = TrimDialogUtf8(std::string_view(ftxt).substr(pos, sep - pos));
                if (!chunk.empty()) chunks.push_back(std::move(chunk));
                if (sep == ftxt.size()) break;
                pos = sep + 1;
            }
            for (const auto& chunk : chunks) {
                std::istringstream iss(chunk);
                std::vector<std::string> words;
                std::string w;
                while (iss >> w) words.push_back(std::move(w));

                bool allCodes = !words.empty();
                for (const auto& word : words) {
                    if (!IsTecCourseCodeToken(word)) {
                        allCodes = false;
                        break;
                    }
                }
                if (allCodes) {
                    for (auto word : words) {
                        for (char& ch : word) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                        prefs.filters.requiredCourseCodes.push_back(std::move(word));
                    }
                    continue;
                }

                std::string low = FoldAccentsLowerDialog(chunk);
                if (low == "virtual") prefs.filters.allowedModalities.push_back("Virtual");
                else if (low == "regular" || low == "presencial") prefs.filters.allowedModalities.push_back("Regular");
                else if (low == "semipresencial") prefs.filters.allowedModalities.push_back("Semipresencial");
                else if (low == "asistida" || low == "asistido") prefs.filters.allowedModalities.push_back("Asistida");
                else if (low == "tutoria") prefs.filters.allowedModalities.push_back("Tutoría");
                else if (low == "grupo rn") prefs.filters.allowedModalities.push_back("Grupo RN");
                else prefs.filters.excludedProfessors.push_back(chunk);
            }
        }
    }
    hasReqFilter = !prefs.filters.requiredCourseCodes.empty();

    busy.clear();
    int freeDaySel = st->hCmbHorFreeDay ? static_cast<int>(SendMessageW(st->hCmbHorFreeDay, CB_GETCURSEL, 0, 0)) : 0;
    if (freeDaySel >= 1 && freeDaySel <= 6) {
        ht::BusySlot bs{};
        bs.day = static_cast<ht::DayOfWeek>(freeDaySel - 1);
        bs.startMinute = 0;
        bs.endMinute = 24 * 60;
        bs.labelUtf8 = "Día libre solicitado";
        bs.sourceUtf8 = "user";
        busy.push_back(std::move(bs));
    }
    if (includeBusy) {
        for (const auto& cell : st->snap.currentSchedule) {
            if (cell.start_minute < cell.end_minute && cell.day_index_0_mon >= 0 && cell.day_index_0_mon <= 6) {
                ht::BusySlot bs{};
                bs.day = static_cast<ht::DayOfWeek>(cell.day_index_0_mon);
                bs.startMinute = cell.start_minute;
                bs.endMinute = cell.end_minute;
                bs.labelUtf8 = cell.course_code.empty() ? cell.raw_text : cell.course_code;
                bs.sourceUtf8 = "enrolled";
                busy.push_back(std::move(bs));
            }
        }
    }
}

std::string FormatHorariosSolveSummaryText(const ht::ScheduleResult& res,
                                           const ht::CourseCatalog& cat,
                                           bool hasReqFilter,
                                           const std::string& extraErr = "") {
    if (!extraErr.empty() && cat.courses.empty()) {
        return extraErr;
    }
    if (res.HasSolutions()) {
        const auto& cur = res.schedules[0];
        return "Opción 1/" + std::to_string(res.schedules.size()) +
               " · " + std::to_string(cur.metrics.totalCredits) + " cr · " +
               std::to_string(cur.metrics.activeDaysCount) + " días · " +
               std::to_string(cur.metrics.totalGapMinutes) + " min huecos";
    }
    if (!cat.courses.empty()) {
        if (hasReqFilter) {
            return "0 combinaciones válidas sin choques para los códigos y filtros indicados.";
        }
        return "Catálogo cargado (" + std::to_string(cat.courses.size()) +
               " cursos). Indique códigos arriba y presione Resolver Horarios.";
    }
    return "0 combinaciones válidas sin choques para los filtros actuales.";
}

std::string BuildCurrentHorariosQueryKey(
    TecHubState* st,
    DocStudio::TecDigital::PublicGuiaQuery& outPubQuery,
    int& outSource) {
    outSource = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
    if (outSource < 0 || outSource > 3) outSource = 0;

    if (outSource == 3) {
        int is = st->hCmbHorSede ? std::max(0, static_cast<int>(SendMessageW(st->hCmbHorSede, CB_GETCURSEL, 0, 0))) : 0;
        int ic = st->hCmbHorCarrera ? std::max(0, static_cast<int>(SendMessageW(st->hCmbHorCarrera, CB_GETCURSEL, 0, 0))) : 0;
        int ip = st->hCmbHorPeriodo ? std::max(0, static_cast<int>(SendMessageW(st->hCmbHorPeriodo, CB_GETCURSEL, 0, 0))) : 0;
        std::string sk = (!st->horSedes.empty() && static_cast<size_t>(is) < st->horSedes.size()) ? st->horSedes[static_cast<size_t>(is)].key : "";
        std::string ck = (!st->horCarreras.empty() && static_cast<size_t>(ic) < st->horCarreras.size()) ? st->horCarreras[static_cast<size_t>(ic)].key : "";
        std::string pk = (!st->horPeriodos.empty() && static_cast<size_t>(ip) < st->horPeriodos.size()) ? st->horPeriodos[static_cast<size_t>(ip)].key : "";
        return "exp:" + sk + "|" + ck + "|" + pk;
    }

    outPubQuery = DocStudio::TecDigital::PublicGuiaQuery{};
    int iy = st->hCmbHorYear ? static_cast<int>(SendMessageW(st->hCmbHorYear, CB_GETCURSEL, 0, 0)) : 0;
    if (iy == 1) outPubQuery.year = 2025;
    else if (iy == 2) outPubQuery.year = 2027;
    else if (iy == 3) outPubQuery.year = 0;
    else outPubQuery.year = 2026;

    static const char* kSedes[6] = { "", "Cartago", "San Jose", "San Carlos", "Alajuela", "Limon" };
    int isede = st->hCmbHorPubSede ? static_cast<int>(SendMessageW(st->hCmbHorPubSede, CB_GETCURSEL, 0, 0)) : 0;
    if (isede >= 0 && isede < 6) outPubQuery.filter.sede_filter = kSedes[isede];

    static const char* kPeriods[4] = { "", "S-1", "S-2", "V-1" };
    int iper = st->hCmbHorPubPer ? static_cast<int>(SendMessageW(st->hCmbHorPubPer, CB_GETCURSEL, 0, 0)) : 0;
    if (outSource != 1 && iper >= 0 && iper < 4) outPubQuery.filter.periodo_filter = kPeriods[iper];

    if (outSource == 0) {
        outPubQuery.mode = DocStudio::TecDigital::PublicGuiaMode::Escuela;
        if (outPubQuery.year <= 0) outPubQuery.year = 2026;
        int ie = st->hCmbHorEscuela ? std::max(0, static_cast<int>(SendMessageW(st->hCmbHorEscuela, CB_GETCURSEL, 0, 0))) : 0;
        if (static_cast<size_t>(ie) < st->publicEscuelas.size()) {
            outPubQuery.escuelaCode = st->publicEscuelas[static_cast<size_t>(ie)].ide_depto;
        }
        return "pub:esc:" + outPubQuery.escuelaCode + "|" + std::to_string(outPubQuery.year) + "|" +
               outPubQuery.filter.sede_filter + "|" + outPubQuery.filter.periodo_filter;
    }
    if (outSource == 1) {
        outPubQuery.mode = DocStudio::TecDigital::PublicGuiaMode::Verano;
        if (outPubQuery.year <= 0) outPubQuery.year = 2026;
        return "pub:ver:" + std::to_string(outPubQuery.year) + "|" + outPubQuery.filter.sede_filter;
    }

    outPubQuery.mode = DocStudio::TecDigital::PublicGuiaMode::Carnet;
    outPubQuery.carnet = TrimDialogUtf8(WideToUtf8(GetCtrlTextW(st->hEditHorCarnet)));
    return "pub:car:" + std::to_string(outPubQuery.carnet.size()) + ":" +
           std::to_string(outPubQuery.year) + "|" + outPubQuery.filter.sede_filter + "|" +
           outPubQuery.filter.periodo_filter + "|" +
           std::to_string(std::hash<std::string>{}(outPubQuery.carnet));
}

void LaunchAsyncHorariosFetch(TecHubState* st, bool mergeIntoCurrent) {
    if (!st) return;
    if (st->workerBusy.exchange(true)) {
        SetWindowTextW(st->hProgressBanner, L"Hay otra consulta en curso, espere un momento.");
        return;
    }

    ht::SolverPreferences prefs{};
    std::vector<ht::BusySlot> manualBusy;
    bool filterPrereq = false;
    bool includeBusy = false;
    bool hasReqFilter = false;
    BuildHorariosPrefsAndBusyFromUi(st, prefs, manualBusy, filterPrereq, includeBusy, hasReqFilter);

    DocStudio::TecDigital::PublicGuiaQuery pubQuery{};
    int source = 0;
    std::string qKey = BuildCurrentHorariosQueryKey(st, pubQuery, source);
    if (mergeIntoCurrent && source == 3) {
        st->workerBusy.store(false);
        SetWindowTextW(st->hHorStatus, L"Cambie la fuente a Guía Pública (Escuelas, Verano o Carné) para combinar catálogos.");
        return;
    }

    std::optional<ht::CourseCatalog> baseCatalog;
    if (mergeIntoCurrent && !st->horariosCatalog.courses.empty()) {
        baseCatalog = st->horariosCatalog;
    }

    std::string expSedeKey, expCarKey, expPerKey, expCampusName;
    if (source == 3) {
        if (st->horSedes.empty() || st->horCarreras.empty() || st->horPeriodos.empty()) {
            st->workerBusy.store(false);
            SetWindowTextW(st->hHorStatus, L"Inicie sesión y sincronice para consultar Expediente TEC, o use Guía Pública TEC.");
            return;
        }
        int is = std::max(0, static_cast<int>(SendMessageW(st->hCmbHorSede, CB_GETCURSEL, 0, 0)));
        int ic = std::max(0, static_cast<int>(SendMessageW(st->hCmbHorCarrera, CB_GETCURSEL, 0, 0)));
        int ip = std::max(0, static_cast<int>(SendMessageW(st->hCmbHorPeriodo, CB_GETCURSEL, 0, 0)));
        expSedeKey = st->horSedes[static_cast<size_t>(is)].key;
        expCarKey = st->horCarreras[static_cast<size_t>(ic)].key;
        expPerKey = st->horPeriodos[static_cast<size_t>(ip)].key;
        expCampusName = st->horSedes[static_cast<size_t>(is)].name;
    }

    SetWindowTextW(st->hProgressBanner, mergeIntoCurrent
                                          ? L"Combinando horarios desde Guía Pública TEC..."
                                          : L"Consultando horarios en modo solo lectura...");
    SetWindowTextW(st->hHorStatus, L"Consultando horarios...");
    if (st->workerThread.joinable()) st->workerThread.join();

    st->workerThread = std::thread([st, source, pubQuery = std::move(pubQuery),
                                    expSedeKey, expCarKey, expPerKey, expCampusName,
                                    prefs, manualBusy = std::move(manualBusy),
                                    filterPrereq, includeBusy, hasReqFilter,
                                    qKey, mergeIntoCurrent, baseCatalog = std::move(baseCatalog)]() mutable {
        DocStudio::TecDigital::LiveScheduleSolveResult res;
        if (source == 3) {
            res = st->store->FetchAndSolveLiveSchedule(
                expSedeKey, expCarKey, expPerKey, expCampusName,
                prefs, filterPrereq, includeBusy);
        } else {
            const ht::CourseCatalog* mergePtr = baseCatalog.has_value() ? &(*baseCatalog) : nullptr;
            res = st->store->FetchAndSolvePublicGuiaSchedule(
                pubQuery, prefs, filterPrereq, includeBusy, mergePtr);
        }
        if (res.ok && !manualBusy.empty()) {
            res.solved = ht::HorariosSolver::Solve(res.catalog, manualBusy, prefs);
        }

        const bool isOk = res.ok;
        const std::string nextKey = mergeIntoCurrent ? ("merged:" + qKey) : qKey;
        const std::string statusMsg = isOk
            ? FormatHorariosSolveSummaryText(res.solved, res.catalog, hasReqFilter)
            : res.errorUtf8;
        {
            std::lock_guard<std::mutex> lk(st->workResultMutex);
            st->pendingHorariosResult = std::move(res);
            st->pendingHorariosCatalogKey = nextKey;
            st->pendingHorariosStatus = statusMsg;
            st->pendingHorariosUsedFilter = hasReqFilter;
        }
        st->workerBusy.store(false);
        PostMessageW(st->hwnd, WM_APP_TEC_HORARIOS_DONE, isOk ? 1 : 0, mergeIntoCurrent ? 1 : 0);
    });
}

void PopulateComboSelectorsFromSnapshot(TecHubState* st) {
    if (st->hCmbPeriod) {
        SendMessageW(st->hCmbPeriod, CB_RESETCONTENT, 0, 0);
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todos los periodos"));
        std::set<std::string> periods;
        for (const auto& c : st->snap.communities) {
            if (c.type == "CLASE" && !c.period.empty()) periods.insert(c.period);
        }
        for (auto it = periods.rbegin(); it != periods.rend(); ++it) {
            SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(*it).c_str()));
        }
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Comunidades y Cátedras"));
        SendMessageW(st->hCmbPeriod, CB_SETCURSEL, 0, 0);
    }

    auto fillOrPlaceholder = [](HWND hCmb, const std::vector<std::string>& items, const wchar_t* placeholder) {
        if (!hCmb) return;
        SendMessageW(hCmb, CB_RESETCONTENT, 0, 0);
        if (items.empty()) {
            SendMessageW(hCmb, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(placeholder));
            SendMessageW(hCmb, CB_SETCURSEL, 0, 0);
        } else {
            for (const auto& name : items) {
                SendMessageW(hCmb, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(name).c_str()));
            }
            SendMessageW(hCmb, CB_SETCURSEL, 0, 0);
        }
    };
    auto extractNames = [](const auto& vec) {
        std::vector<std::string> names;
        names.reserve(vec.size());
        for (const auto& x : vec) names.push_back(x.name);
        return names;
    };

    st->horSedes = st->snap.expediente ? st->snap.expediente->sedes_combo : std::vector<td::SedeInfo>{};
    if (st->horSedes.empty()) st->horSedes = st->snap.curriculumSedes;
    fillOrPlaceholder(st->hCmbHorSede, extractNames(st->horSedes), L"Sede...");

    st->horCarreras = st->snap.curriculumCarreras;
    fillOrPlaceholder(st->hCmbHorCarrera, extractNames(st->horCarreras), L"Carrera...");

    st->horPeriodos = st->snap.expediente ? st->snap.expediente->periodos_combo : std::vector<td::PlanInfo>{};
    fillOrPlaceholder(st->hCmbHorPeriodo, extractNames(st->horPeriodos), L"Periodo...");

    st->mallaSedes = st->snap.curriculumSedes;
    fillOrPlaceholder(st->hCmbMallaSede, extractNames(st->mallaSedes), L"Sede...");

    st->mallaCarreras = st->snap.curriculumCarreras;
    fillOrPlaceholder(st->hCmbMallaCarrera, extractNames(st->mallaCarreras), L"Carrera...");

    st->mallaPlanes = st->snap.curriculumPlanes;
    fillOrPlaceholder(st->hCmbMallaPlan, extractNames(st->mallaPlanes), L"Plan de estudios...");

    RefreshPublicEscuelasCombo(st);
}

void RefreshAllSections(TecHubState* st) {
    st->snap = st->store->Snapshot();
    PopulateComboSelectorsFromSnapshot(st);
    RefreshSummarySection(st);
    RefreshCourseList(st);
    RefreshSelectedCourseDetail(st);
    RefreshCalendarSection(st);
    RefreshExpedienteSection(st);
    RefreshTramitesSection(st);
    RefreshCacheUsageLabel(st);
}

void LayoutDialog(TecHubState* st);

void SwitchSection(TecHubState* st, HubSection sec) {
    st->currentSection = sec;
    static const wchar_t* kTitles[7] = {
        L"Resumen Académico",
        L"Cursos y Comunidades",
        L"Calendario Unificado",
        L"Expediente y Plan de Estudios",
        L"Planificador de Horarios",
        L"Biblioteca Offline",
        L"Trámites y Servicios"
    };
    static const wchar_t* kSubs[7] = {
        L"Acceso solo lectura a su agenda, entregas próximas y avisos institucionales.",
        L"Portadas, documentos descargables, evaluaciones, rúbricas y simulador de nota.",
        L"Entregas, citas de matrícula y eventos integrados con exportación estándar .ics.",
        L"Estadísticas de avance, registro de notas, malla curricular y atributos de graduación.",
        L"Combinaciones óptimas sin choques desde la Guía de Horarios o archivo local.",
        L"Búsqueda instantánea de texto completo dentro de los materiales descargados.",
        L"Control de plagio, prematrícula, inclusiones, evaluación docente y servicios TEC."
    };

    const int idx = static_cast<int>(sec);
    if (st->hTitle) SetWindowTextW(st->hTitle, kTitles[idx]);
    if (st->hSubtitle) SetWindowTextW(st->hSubtitle, kSubs[idx]);

    for (int i = 0; i < 7; ++i) {
        const int cmd = (i == idx) ? SW_SHOW : SW_HIDE;
        for (HWND c : st->secCtrls[i]) {
            if (c) ShowWindow(c, cmd);
        }
        if (st->navBtns[i]) InvalidateRect(st->navBtns[i], nullptr, FALSE);
    }

    if (sec == HubSection::Resumen && st->hBtnLogout) {
        const bool authed = st->store->IsAuthenticated() || st->snap.sessionAuthenticated;
        ShowWindow(st->hBtnLogout, authed ? SW_SHOW : SW_HIDE);
    }

    if (sec == HubSection::Cursos) {
        int subTab = st->hCmbCourseSub ? static_cast<int>(SendMessageW(st->hCmbCourseSub, CB_GETCURSEL, 0, 0)) : 0;
        const bool showDocs = (subTab == 1);
        const bool showWhatIf = (subTab == 2);
        if (st->hCourseDetail) ShowWindow(st->hCourseDetail, showDocs ? SW_HIDE : SW_SHOW);
        if (st->hLstCourseFiles) ShowWindow(st->hLstCourseFiles, showDocs ? SW_SHOW : SW_HIDE);
        if (st->hEditWhatIf) ShowWindow(st->hEditWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnWhatIf) ShowWindow(st->hBtnWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseRubric) ShowWindow(st->hBtnCourseRubric, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseOpenWeb) ShowWindow(st->hBtnCourseOpenWeb, SW_HIDE);
        if (st->hBtnCourseGemini) ShowWindow(st->hBtnCourseGemini, SW_HIDE);
    }

    if (sec == HubSection::Calendario) {
        int calMode = st->hCmbCalView ? static_cast<int>(SendMessageW(st->hCmbCalView, CB_GETCURSEL, 0, 0)) : 0;
        const bool showGrid = (calMode == 1);
        if (st->hCalList) ShowWindow(st->hCalList, showGrid ? SW_HIDE : SW_SHOW);
        if (st->hCalCanvas) {
            ShowWindow(st->hCalCanvas, showGrid ? SW_SHOW : SW_HIDE);
            if (showGrid) InvalidateRect(st->hCalCanvas, nullptr, FALSE);
        }
    }

    if (sec == HubSection::ExpedienteYMalla) {
        int expMode = st->hCmbExpView ? static_cast<int>(SendMessageW(st->hCmbExpView, CB_GETCURSEL, 0, 0)) : 0;
        const bool showStats = (expMode == 0);
        const bool showMalla = (expMode == 1);
        const bool showAttr  = (expMode == 2);
        const bool showAttrInsp = showAttr && !st->snap.graduateAttributes.empty();
        ShowWindow(st->hCmbGradesFilter, showStats ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hExpReport, showStats ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hCmbMallaSede, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hCmbMallaCarrera, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hCmbMallaPlan, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hBtnMallaLoad, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hMallaCanvas, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hBtnMallaInsert, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hBtnMallaZIn, showMalla ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hBtnMallaZOut, showMalla ? SW_SHOW : SW_HIDE);
        if (st->hAttrCanvas) ShowWindow(st->hAttrCanvas, showAttr ? SW_SHOW : SW_HIDE);
        ShowWindow(st->hMallaInspector, (showMalla || showAttrInsp) ? SW_SHOW : SW_HIDE);
        LayoutDialog(st);
        if (showAttr) {
            if (st->hMallaInspector && showAttrInsp) {
                SetWindowTextW(st->hMallaInspector, Utf8ToWide(FormatGraduateAttributesInspectorText(st)).c_str());
            }
            if (st->hAttrCanvas) InvalidateRect(st->hAttrCanvas, nullptr, FALSE);
        } else if (showMalla) {
            RefreshExpedienteSection(st);
            if (st->hMallaCanvas) InvalidateRect(st->hMallaCanvas, nullptr, FALSE);
        }
    }
    if (sec == HubSection::HorariosVivo) {
        SyncHorariosSourceVisibility(st);
        LayoutDialog(st);
        if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
    }
    if (sec == HubSection::TramitesYPerfil) {
        if (st->hBtnSvcMapa) ShowWindow(st->hBtnSvcMapa, SW_HIDE);
        if (st->hBtnSvcGuia) ShowWindow(st->hBtnSvcGuia, SW_HIDE);
        if (st->hBtnSvcSoporte) ShowWindow(st->hBtnSvcSoporte, SW_HIDE);
    }
    UpdateActiveFramedPanels(st);
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

void LayoutDialog(TecHubState* st) {
    RECT rc{};
    GetClientRect(st->hwnd, &rc);
    const int W = rc.right - rc.left;
    const int H = rc.bottom - rc.top;
    const int railW = st->Sc(216);
    const int pad = st->Sc(22);
    const int hdrH = st->Sc(68);
    const int cx = railW + pad;
    const int cw = std::max(st->Sc(320), W - cx - pad);
    const int cy = hdrH + st->Sc(12);
    const int ch = std::max(st->Sc(220), H - cy - st->Sc(42));

    st->rcSidebarSep = { railW, 0, railW + 1, H };

    // Left rail navigation buttons (below sidebar identity header)
    int ny = st->Sc(72);
    for (int i = 0; i < 7; ++i) {
        if (st->navBtns[i]) {
            MoveWindow(st->navBtns[i], st->Sc(12), ny, railW - st->Sc(24), st->Sc(34), TRUE);
        }
        ny += st->Sc(38);
    }
    if (st->hSessionBadge) MoveWindow(st->hSessionBadge, st->Sc(12), H - st->Sc(114), railW - st->Sc(24), st->Sc(20), TRUE);
    MoveWindow(st->hBtnSync, st->Sc(12), H - st->Sc(86), railW - st->Sc(24), st->Sc(32), TRUE);
    MoveWindow(st->hBtnOpenWeb, st->Sc(12), H - st->Sc(46), railW - st->Sc(24), st->Sc(30), TRUE);

    // Main header & status bar
    MoveWindow(st->hTitle, cx, st->Sc(18), cw, st->Sc(28), TRUE);
    MoveWindow(st->hSubtitle, cx, st->Sc(48), cw, st->Sc(20), TRUE);
    MoveWindow(st->hProgressBanner, cx + st->Sc(18), H - st->Sc(26), std::max(st->Sc(120), cw - st->Sc(18)), st->Sc(18), TRUE);

    // Section 0: Resumen + Cuenta
    {
        st->panelAccountBand = { cx, cy, cx + cw, cy + st->Sc(76) };
        const int rowH = st->Sc(30);
        const int rowY = cy + st->Sc(12);
        const int ew = std::min(st->Sc(200), (cw - st->Sc(310)) / 2);
        st->rcInEmail = { cx + st->Sc(12), rowY, cx + st->Sc(12) + ew, rowY + rowH };
        MoveWindow(st->hEmail, st->rcInEmail.left + st->Sc(8), rowY + st->Sc(6), std::max(40, ew - st->Sc(16)), st->Sc(18), TRUE);
        st->rcInPass = { st->rcInEmail.right + st->Sc(8), rowY, st->rcInEmail.right + st->Sc(8) + ew, rowY + rowH };
        MoveWindow(st->hPass, st->rcInPass.left + st->Sc(8), rowY + st->Sc(6), std::max(40, ew - st->Sc(16)), st->Sc(18), TRUE);
        int bx = st->rcInPass.right + st->Sc(10);
        MoveWindow(st->hBtnLogin, bx, rowY, st->Sc(114), rowH, TRUE);
        MoveWindow(st->hBtnQr, bx + st->Sc(120), rowY, st->Sc(56), rowH, TRUE);
        MoveWindow(st->hBtnLogout, bx + st->Sc(182), rowY, st->Sc(134), rowH, TRUE);
        MoveWindow(st->hChkDpapi, cx + st->Sc(12), cy + st->Sc(48), st->Sc(340), st->Sc(20), TRUE);

        int topSum = cy + st->Sc(88);
        int sumH = std::max(st->Sc(140), ch - (topSum - cy));
        if (st->hSummaryCanvas) MoveWindow(st->hSummaryCanvas, cx, topSum, cw, sumH, TRUE);
        if (st->hSummaryEdit) MoveWindow(st->hSummaryEdit, cx, topSum, cw, sumH, TRUE);
    }

    // Section 1: Cursos
    {
        int leftW = std::min(st->Sc(284), cw / 3);
        int rx = cx + leftW + st->Sc(16);
        int rw = cw - leftW - st->Sc(16);
        MoveWindow(st->hCmbPeriod, cx, cy, leftW, st->Sc(220), TRUE);
        st->panelCourseList = { cx, cy + st->Sc(38), cx + leftW, cy + ch };
        MoveWindow(st->hLstCourses, st->panelCourseList.left + st->Sc(4), st->panelCourseList.top + st->Sc(6),
                   std::max(60, leftW - st->Sc(8)), std::max(80, ch - st->Sc(50)), TRUE);

        MoveWindow(st->hCmbCourseSub, rx, cy, st->Sc(218), st->Sc(220), TRUE);
        st->rcInWhatIf = { rx + st->Sc(226), cy, rx + st->Sc(292), cy + st->Sc(28) };
        MoveWindow(st->hEditWhatIf, st->rcInWhatIf.left + st->Sc(8), cy + st->Sc(5), st->Sc(50), st->Sc(18), TRUE);
        if (st->hBtnWhatIf) MoveWindow(st->hBtnWhatIf, rx + st->Sc(298), cy, st->Sc(98), st->Sc(28), TRUE);
        MoveWindow(st->hBtnCourseRubric, rx + st->Sc(402), cy, st->Sc(98), st->Sc(28), TRUE);

        int detailH = std::max(st->Sc(100), ch - st->Sc(80));
        st->panelCourseDetail = { rx, cy + st->Sc(38), rx + rw, cy + st->Sc(38) + detailH };
        MoveWindow(st->hCourseDetail, rx + st->Sc(4), cy + st->Sc(44), std::max(80, rw - st->Sc(8)), detailH - st->Sc(12), TRUE);
        if (st->hLstCourseFiles) {
            MoveWindow(st->hLstCourseFiles, rx + st->Sc(4), cy + st->Sc(44), std::max(80, rw - st->Sc(8)), detailH - st->Sc(12), TRUE);
        }
        int btnY = cy + ch - st->Sc(30);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_COURSE_INSERT_CANVAS), rx, btnY, st->Sc(142), st->Sc(30), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_COURSE_SYNC_FILES), rx + st->Sc(150), btnY, st->Sc(152), st->Sc(30), TRUE);
        HWND hBtnMore = GetDlgItem(st->hwnd, IDC_BTN_COURSE_MORE);
        if (hBtnMore) MoveWindow(hBtnMore, rx + st->Sc(310), btnY, st->Sc(134), st->Sc(30), TRUE);
    }

    // Section 2: Calendario
    {
        MoveWindow(st->hCmbCalFilter, cx, cy, st->Sc(200), st->Sc(180), TRUE);
        if (st->hCmbCalView) MoveWindow(st->hCmbCalView, cx + st->Sc(208), cy, st->Sc(184), st->Sc(180), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_CAL_EXPORT_ICS), cx + cw - st->Sc(144), cy, st->Sc(144), st->Sc(28), TRUE);
        st->panelCalList = { cx, cy + st->Sc(38), cx + cw, cy + ch };
        MoveWindow(st->hCalList, cx + st->Sc(4), cy + st->Sc(44), cw - st->Sc(8), ch - st->Sc(50), TRUE);
        if (st->hCalCanvas) MoveWindow(st->hCalCanvas, cx, cy + st->Sc(38), cw, ch - st->Sc(38), TRUE);
    }

    // Section 3: Expediente y Malla Curricular
    {
        MoveWindow(st->hCmbExpView, cx, cy, st->Sc(232), st->Sc(180), TRUE);
        MoveWindow(st->hCmbGradesFilter, cx + st->Sc(240), cy, st->Sc(180), st->Sc(180), TRUE);
        MoveWindow(st->hBtnMallaZIn, cx + st->Sc(240), cy, st->Sc(68), st->Sc(28), TRUE);
        MoveWindow(st->hBtnMallaZOut, cx + st->Sc(314), cy, st->Sc(68), st->Sc(28), TRUE);
        MoveWindow(st->hBtnMallaInsert, cx + cw - st->Sc(182), cy, st->Sc(182), st->Sc(28), TRUE);

        int mRow2 = cy + st->Sc(36);
        int mcbW = std::max(st->Sc(110), (cw - st->Sc(122)) / 3);
        if (st->hCmbMallaSede) MoveWindow(st->hCmbMallaSede, cx, mRow2, mcbW, st->Sc(180), TRUE);
        if (st->hCmbMallaCarrera) MoveWindow(st->hCmbMallaCarrera, cx + mcbW + st->Sc(6), mRow2, mcbW, st->Sc(180), TRUE);
        if (st->hCmbMallaPlan) MoveWindow(st->hCmbMallaPlan, cx + 2 * (mcbW + st->Sc(6)), mRow2, mcbW, st->Sc(180), TRUE);
        if (st->hBtnMallaLoad) MoveWindow(st->hBtnMallaLoad, cx + cw - st->Sc(102), mRow2, st->Sc(102), st->Sc(28), TRUE);

        st->panelExpReport = { cx, cy + st->Sc(38), cx + cw, cy + ch };
        MoveWindow(st->hExpReport, cx + st->Sc(4), cy + st->Sc(44), cw - st->Sc(8), ch - st->Sc(50), TRUE);

        int inspH = st->Sc(96);
        int mallaTop = mRow2 + st->Sc(34);
        MoveWindow(st->hMallaCanvas, cx, mallaTop, cw, std::max(st->Sc(130), ch - (mallaTop - cy) - inspH - st->Sc(10)), TRUE);
        if (st->hAttrCanvas) {
            const bool showAttrInsp = !st->snap.graduateAttributes.empty();
            const int attrH = showAttrInsp
                                  ? std::max(st->Sc(160), ch - st->Sc(38) - inspH - st->Sc(10))
                                  : std::max(st->Sc(180), ch - st->Sc(38));
            MoveWindow(st->hAttrCanvas, cx, cy + st->Sc(38), cw, attrH, TRUE);
        }
        st->panelMallaInspector = { cx, cy + ch - inspH, cx + cw, cy + ch };
        MoveWindow(st->hMallaInspector, cx + st->Sc(4), cy + ch - inspH + st->Sc(6), cw - st->Sc(8), inspH - st->Sc(12), TRUE);
    }

    // Section 4: Planificador Horarios
    {
        const int g = st->Sc(8);
        const int srcW = std::min(cw / 3, std::max(st->Sc(194), (cw * 25) / 100));
        if (st->hCmbHorSource) {
            MoveWindow(st->hCmbHorSource, cx, cy, srcW, st->Sc(200), TRUE);
        }
        const int horSrc = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
        const int rx0 = cx + srcW + g;
        const int remW = std::max(st->Sc(120), cw - srcW - g);
        st->rcInHorCarnet = {};

        if (horSrc == 0) {
            const int remInner = std::max(st->Sc(80), remW - 3 * g);
            const int escW = std::max(st->Sc(150), (remInner * 42) / 100);
            const int yrW  = std::max(st->Sc(68),  (remInner * 14) / 100);
            const int sdW  = std::max(st->Sc(96),  (remInner * 22) / 100);
            const int prW  = std::max(st->Sc(90),  remInner - escW - yrW - sdW);
            if (st->hCmbHorEscuela) MoveWindow(st->hCmbHorEscuela, rx0, cy, escW, st->Sc(260), TRUE);
            if (st->hCmbHorYear) MoveWindow(st->hCmbHorYear, rx0 + escW + g, cy, yrW, st->Sc(200), TRUE);
            if (st->hCmbHorPubSede) MoveWindow(st->hCmbHorPubSede, rx0 + escW + g + yrW + g, cy, sdW, st->Sc(200), TRUE);
            if (st->hCmbHorPubPer) MoveWindow(st->hCmbHorPubPer, rx0 + escW + g + yrW + g + sdW + g, cy, prW, st->Sc(200), TRUE);
        } else if (horSrc == 1) {
            const int remInner = std::max(st->Sc(80), remW - g);
            const int yrW = std::max(st->Sc(90), (remInner * 24) / 100);
            const int sdW = std::max(st->Sc(140), remInner - yrW);
            if (st->hCmbHorYear) MoveWindow(st->hCmbHorYear, rx0, cy, yrW, st->Sc(200), TRUE);
            if (st->hCmbHorPubSede) MoveWindow(st->hCmbHorPubSede, rx0 + yrW + g, cy, sdW, st->Sc(200), TRUE);
        } else if (horSrc == 2) {
            const int remInner = std::max(st->Sc(80), remW - 3 * g);
            const int carW = std::max(st->Sc(140), (remInner * 38) / 100);
            const int yrW  = std::max(st->Sc(74),  (remInner * 16) / 100);
            const int sdW  = std::max(st->Sc(96),  (remInner * 23) / 100);
            const int prW  = std::max(st->Sc(90),  remInner - carW - yrW - sdW);
            st->rcInHorCarnet = { rx0, cy, rx0 + carW, cy + st->Sc(26) };
            if (st->hEditHorCarnet) {
                MoveWindow(st->hEditHorCarnet, st->rcInHorCarnet.left + st->Sc(8), cy + st->Sc(4),
                           std::max(40, carW - st->Sc(16)), st->Sc(18), TRUE);
            }
            if (st->hCmbHorYear) MoveWindow(st->hCmbHorYear, rx0 + carW + g, cy, yrW, st->Sc(200), TRUE);
            if (st->hCmbHorPubSede) MoveWindow(st->hCmbHorPubSede, rx0 + carW + g + yrW + g, cy, sdW, st->Sc(200), TRUE);
            if (st->hCmbHorPubPer) MoveWindow(st->hCmbHorPubPer, rx0 + carW + g + yrW + g + sdW + g, cy, prW, st->Sc(200), TRUE);
        } else {
            const int cbW = std::max(st->Sc(80), (remW - 2 * g) / 3);
            MoveWindow(st->hCmbHorSede, rx0, cy, cbW, st->Sc(200), TRUE);
            MoveWindow(st->hCmbHorCarrera, rx0 + cbW + g, cy, cbW, st->Sc(200), TRUE);
            MoveWindow(st->hCmbHorPeriodo, rx0 + 2 * (cbW + g), cy, std::max(st->Sc(80), remW - 2 * (cbW + g)), st->Sc(200), TRUE);
        }

        int r2 = cy + st->Sc(36);
        MoveWindow(st->hChkHorPrereq, cx, r2 + st->Sc(3), st->Sc(172), st->Sc(22), TRUE);
        MoveWindow(st->hChkHorNoNight, cx + st->Sc(176), r2 + st->Sc(3), st->Sc(106), st->Sc(22), TRUE);
        MoveWindow(st->hChkHorSeats, cx + st->Sc(286), r2 + st->Sc(3), st->Sc(106), st->Sc(22), TRUE);
        if (st->hChkHorBusy) MoveWindow(st->hChkHorBusy, cx + st->Sc(396), r2 + st->Sc(3), st->Sc(144), st->Sc(22), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_LOAD_JSON), cx + cw - st->Sc(252), r2, st->Sc(110), st->Sc(28), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_SOLVE), cx + cw - st->Sc(134), r2, st->Sc(134), st->Sc(28), TRUE);

        int r3 = r2 + st->Sc(32);
        if (st->hCmbHorFreeDay) MoveWindow(st->hCmbHorFreeDay, cx, r3, st->Sc(172), st->Sc(180), TRUE);
        st->rcInHorFilter = { cx + st->Sc(180), r3, cx + cw, r3 + st->Sc(28) };
        if (st->hEditHorFilter) {
            MoveWindow(st->hEditHorFilter, st->rcInHorFilter.left + st->Sc(8), r3 + st->Sc(5),
                       std::max(st->Sc(140), cw - st->Sc(196)), st->Sc(18), TRUE);
        }

        int gridTop = r3 + st->Sc(34);
        int footerH = st->Sc(40);
        MoveWindow(st->hHorCanvas, cx, gridTop, cw, std::max(st->Sc(140), ch - (gridTop - cy) - footerH), TRUE);

        int fy = cy + ch - st->Sc(30);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_PREV), cx, fy, st->Sc(78), st->Sc(28), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_NEXT), cx + st->Sc(84), fy, st->Sc(82), st->Sc(28), TRUE);
        MoveWindow(st->hHorStatus, cx + st->Sc(174), fy + st->Sc(5), std::max(st->Sc(100), cw - st->Sc(468)), st->Sc(20), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_INSERT), cx + cw - st->Sc(286), fy, st->Sc(144), st->Sc(28), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_EXPORT), cx + cw - st->Sc(134), fy, st->Sc(134), st->Sc(28), TRUE);
    }

    // Section 5: Biblioteca Offline
    {
        st->rcInFts = { cx, cy, cx + cw - st->Sc(118), cy + st->Sc(30) };
        MoveWindow(st->hEditFts, st->rcInFts.left + st->Sc(8), cy + st->Sc(6), std::max(80, cw - st->Sc(134)), st->Sc(18), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_FTS_SEARCH), cx + cw - st->Sc(108), cy, st->Sc(108), st->Sc(30), TRUE);
        int prevH = st->Sc(88);
        int lstH = std::max(st->Sc(110), ch - st->Sc(92) - prevH);
        st->panelFtsList = { cx, cy + st->Sc(40), cx + cw, cy + st->Sc(40) + lstH };
        MoveWindow(st->hLstFts, cx + st->Sc(4), cy + st->Sc(46), cw - st->Sc(8), lstH - st->Sc(12), TRUE);
        st->panelFtsPreview = { cx, cy + st->Sc(48) + lstH, cx + cw, cy + st->Sc(48) + lstH + prevH };
        if (st->hFtsPreview) {
            MoveWindow(st->hFtsPreview, cx + st->Sc(4), st->panelFtsPreview.top + st->Sc(6), cw - st->Sc(8), prevH - st->Sc(12), TRUE);
        }
        int fy = cy + ch - st->Sc(30);
        MoveWindow(st->hLblCache, cx, fy + st->Sc(6), st->Sc(210), st->Sc(20), TRUE);
        HWND hBtnOpen = GetDlgItem(st->hwnd, IDC_BTN_FTS_OPEN);
        if (hBtnOpen) MoveWindow(hBtnOpen, cx + cw - st->Sc(402), fy, st->Sc(110), st->Sc(30), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_FTS_INSERT), cx + cw - st->Sc(284), fy, st->Sc(144), st->Sc(30), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_CACHE_PRUNE), cx + cw - st->Sc(132), fy, st->Sc(132), st->Sc(30), TRUE);
    }

    // Section 6: Tramites y Servicios
    {
        MoveWindow(st->hCmbTramite, cx, cy, st->Sc(340), st->Sc(180), TRUE);
        st->panelTramiteReport = { cx, cy + st->Sc(38), cx + cw, cy + ch - st->Sc(40) };
        MoveWindow(st->hTramiteReport, cx + st->Sc(4), cy + st->Sc(44), cw - st->Sc(8), std::max(80, ch - st->Sc(90)), TRUE);
        int fy = cy + ch - st->Sc(30);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_SVC_TURNITIN), cx, fy, st->Sc(146), st->Sc(30), TRUE);
        HWND hBtnSvcMore = GetDlgItem(st->hwnd, IDC_BTN_SVC_MORE);
        if (hBtnSvcMore) MoveWindow(hBtnSvcMore, cx + st->Sc(154), fy, st->Sc(156), st->Sc(30), TRUE);
    }

    UpdateActiveFramedPanels(st);
}

void StartAsyncSync(TecHubState* st) {
    if (st->workerBusy.exchange(true)) return;
    st->workerCancel.store(false);
    SetWindowTextW(st->hProgressBanner, L"Sincronizando en modo solo lectura...");
    if (st->workerThread.joinable()) st->workerThread.join();
    st->workerThread = std::thread([st]() {
        std::string err;
        DocStudio::TecDigital::TecSyncOptions opts;
        opts.syncUserModel = true;
        opts.syncSchoolCatalog = true;
        opts.syncForums = true;
        opts.syncMembers = true;
        opts.syncGaap = true;
        opts.syncGraduateAttributes = true;
        opts.syncPerformanceEval = true;
        opts.syncGraphQlInclusiones = true;
        opts.syncCustomTabs = true;
        opts.maxCustomTabsPerSpace = 12;
        opts.politeDelayMs = 320;
        bool ok = st->store->SyncAll(
            opts,
            [st](int pct, const std::string& msg) {
                {
                    std::lock_guard<std::mutex> lk(st->workResultMutex);
                    st->pendingWorkStatus = std::to_string(pct) + "% — " + msg;
                }
                PostMessageW(st->hwnd, WM_APP_TEC_PROGRESS, 0, 0);
            },
            &st->workerCancel,
            &err);
        {
            std::lock_guard<std::mutex> lk(st->workResultMutex);
            st->pendingWorkKind = 2;
            st->pendingWorkStatus = ok ? "Sincronización completada (0 mutaciones)." : ("Sincronización incompleta: " + err);
            st->pendingError = ok ? "" : err;
        }
        st->workerBusy.store(false);
        PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, ok ? 1 : 0, 0);
    });
}

LRESULT CALLBACK TecHubWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (st) {
        if (msg == WM_ERASEBKGND) {
            return 1;
        }
        if (msg == WM_PAINT) {
            PAINTSTRUCT ps{};
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc{};
            GetClientRect(hwnd, &rc);
            const int W = std::max<int>(1, rc.right - rc.left);
            const int H = std::max<int>(1, rc.bottom - rc.top);
            HDC memDc = CreateCompatibleDC(hdc);
            HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
            HGDIOBJ oldBmp = SelectObject(memDc, bmp);

            {
                const int preRailW = st->rcSidebarSep.right > 0 ? st->rcSidebarSep.left : st->Sc(216);
                const int clampedRailW = std::max(0, std::min(W, preRailW));
                FillSolidDcRect(memDc, clampedRailW, H, st->theme.pal.sidebarBg);
                if (W > clampedRailW && H > 0) {
                    RECT cpRc{clampedRailW, 0, W, H};
                    HBRUSH cpHbr = CreateSolidBrush(RGB(st->theme.pal.contentPaper.r,
                                                        st->theme.pal.contentPaper.g,
                                                        st->theme.pal.contentPaper.b));
                    if (cpHbr) {
                        FillRect(memDc, &cpRc, cpHbr);
                        DeleteObject(cpHbr);
                    }
                }
            }

            {
                Gdiplus::Graphics g(memDc);
                g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

                const auto& pal = st->theme.pal;
                const bool hc = st->theme.settings.IsHighContrast();
                const float dpiScale = static_cast<float>(st->dpi) / 96.0f;
                const int railW = st->rcSidebarSep.right > 0 ? st->rcSidebarSep.left : st->Sc(216);

                // Vertical hairline separator
                Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), hc ? 1.5f : 1.0f);
                g.DrawLine(&sepPen, static_cast<float>(railW), 0.0f, static_cast<float>(railW), static_cast<float>(H));

                // Sidebar identity header (squircle emblem + "TEC Digital" + live status dot)
                {
                    const float bx = static_cast<float>(st->Sc(14));
                    const float by = static_cast<float>(st->Sc(18));
                    const float bSz = static_cast<float>(st->Sc(28));
                    DrawCleanSurfaceBox(g, bx, by, bSz, bSz, static_cast<float>(st->Sc(7)),
                                        pal.accentBlueSoft, hc ? pal.accentBlue : UiGlass::Rgba8{ 0, 0, 0, 0 },
                                        hc ? 1.25f : 0.0f);
                    UiGlass::DrawVectorIconGdiPlus(g, UiGlass::IconId::AcademicCap,
                                                   bx + bSz * 0.5f, by + bSz * 0.5f,
                                                   static_cast<float>(st->Sc(15)), UiGlass::ToGdip(pal.accentBlue),
                                                   1.55f * dpiScale);

                    Gdiplus::FontFamily ff(L"Segoe UI");
                    Gdiplus::Font fId(&ff, 14.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                    Gdiplus::Font fSt(&ff, 11.2f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                    Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
                    Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
                    Gdiplus::StringFormat sf;
                    sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                    sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
                    const float tx = bx + bSz + static_cast<float>(st->Sc(9));
                    const float tw = std::max(40.0f, static_cast<float>(railW) - tx - static_cast<float>(st->Sc(10)));
                    g.DrawString(L"TEC Digital", -1, &fId, Gdiplus::RectF(tx, by - 1.0f, tw, 17.0f * dpiScale), &sf, &brPrim);

                    const bool authed = st->store && (st->store->IsAuthenticated() || st->snap.sessionAuthenticated);
                    Gdiplus::SolidBrush dotBr(UiGlass::ToGdip(authed ? pal.accentGreen : pal.labelTertiary));
                    const float dotSz = 6.5f * dpiScale;
                    g.FillEllipse(&dotBr, tx, by + 18.0f * dpiScale, dotSz, dotSz);
                    g.DrawString(authed ? L"Conectado · Solo lectura" : L"Sin conexión · Caché", -1, &fSt,
                                 Gdiplus::RectF(tx + 10.0f * dpiScale, by + 14.5f * dpiScale, tw - 10.0f * dpiScale, 15.0f * dpiScale),
                                 &sf, &brSec);
                }

                // Active rounded containers (one surface tier only, never nested cards)
                const float panelR = static_cast<float>(st->Sc(10));
                const float inputR = static_cast<float>(st->Sc(7));
                const UiGlass::Rgba8 panelBorder = hc ? pal.borderStrong : pal.borderSubtle;
                const float panelBorderW = hc ? 1.5f : 1.0f;

                for (const RECT& pr : st->framedElevRects) {
                    DrawCleanSurfaceBox(g, static_cast<float>(pr.left), static_cast<float>(pr.top),
                                        static_cast<float>(pr.right - pr.left), static_cast<float>(pr.bottom - pr.top),
                                        panelR, pal.contentCardElevated, panelBorder, panelBorderW);
                }
                for (const RECT& pr : st->framedPanelRects) {
                    DrawCleanSurfaceBox(g, static_cast<float>(pr.left), static_cast<float>(pr.top),
                                        static_cast<float>(pr.right - pr.left), static_cast<float>(pr.bottom - pr.top),
                                        panelR, pal.contentCard, panelBorder, panelBorderW);
                }
                for (const RECT& ir : st->framedInputRects) {
                    const UiGlass::Rgba8 inFill = (st->currentSection == HubSection::Resumen)
                                                      ? pal.contentCard
                                                      : pal.contentCardElevated;
                    DrawCleanSurfaceBox(g, static_cast<float>(ir.left), static_cast<float>(ir.top),
                                        static_cast<float>(ir.right - ir.left), static_cast<float>(ir.bottom - ir.top),
                                        inputR, inFill, panelBorder, panelBorderW);
                }

                // Footer hairline & quiet security glyph
                const int cx = railW + st->Sc(22);
                const float footY = static_cast<float>(H - st->Sc(34));
                g.DrawLine(&sepPen, static_cast<float>(cx), footY, static_cast<float>(W - st->Sc(22)), footY);
                UiGlass::DrawVectorIconGdiPlus(g, UiGlass::IconId::KeyVault,
                                               static_cast<float>(cx + st->Sc(6)), static_cast<float>(H - st->Sc(18)),
                                               static_cast<float>(st->Sc(11)), UiGlass::ToGdip(pal.labelTertiary),
                                               1.35f * dpiScale);
            }

            BitBlt(hdc, 0, 0, W, H, memDc, 0, 0, SRCCOPY);
            SelectObject(memDc, oldBmp);
            DeleteObject(bmp);
            DeleteDC(memDc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        if (msg == WM_DRAWITEM && wp != 0) {
            DrawHigButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lp), st);
            return TRUE;
        }
        if (msg == WM_CTLCOLORSTATIC) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            HWND hCtrl = reinterpret_cast<HWND>(lp);
            const auto& pal = st->theme.pal;
            if (hCtrl == st->hChkDpapi && st->brInput) {
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentCardElevated.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brInput);
            }
            if (hCtrl == st->hSessionBadge && st->brSidebar) {
                SetTextColor(hdc, pal.labelTertiary.ToColorRef());
                SetBkColor(hdc, pal.sidebarBg.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brSidebar);
            }
            if ((hCtrl == st->hCourseDetail || hCtrl == st->hCalList || hCtrl == st->hExpReport ||
                 hCtrl == st->hMallaInspector || hCtrl == st->hFtsPreview || hCtrl == st->hTramiteReport ||
                 hCtrl == st->hSummaryEdit) && st->brCard) {
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentCard.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brCard);
            }
            if (hCtrl == st->hSubtitle && st->brPaper) {
                SetTextColor(hdc, pal.labelSecondary.ToColorRef());
                SetBkColor(hdc, pal.contentPaper.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brPaper);
            }
            if ((hCtrl == st->hProgressBanner || hCtrl == st->hHorStatus || hCtrl == st->hLblCache) && st->brPaper) {
                SetTextColor(hdc, pal.labelTertiary.ToColorRef());
                SetBkColor(hdc, pal.contentPaper.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brPaper);
            }
            if (st->brPaper) {
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentPaper.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brPaper);
            }
        }
        if (msg == WM_CTLCOLOREDIT) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            HWND hCtrl = reinterpret_cast<HWND>(lp);
            const auto& pal = st->theme.pal;
            if ((hCtrl == st->hEditWhatIf || hCtrl == st->hEditHorFilter || hCtrl == st->hEditHorCarnet || hCtrl == st->hEditFts) && st->brInput) {
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentCardElevated.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brInput);
            }
            if (st->brCard) {
                SetTextColor(hdc, pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, pal.contentCard.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brCard);
            }
        }
        if (msg == WM_CTLCOLORLISTBOX && st->brCard) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            SetTextColor(hdc, st->theme.pal.labelPrimary.ToColorRef());
            SetBkColor(hdc, st->theme.pal.contentCard.ToColorRef());
            return reinterpret_cast<LRESULT>(st->brCard);
        }
        if (msg == WM_CTLCOLORBTN && st->brPaper) {
            HDC hdc = reinterpret_cast<HDC>(wp);
            HWND hCtrl = reinterpret_cast<HWND>(lp);
            if (hCtrl == st->hChkDpapi && st->brInput) {
                SetTextColor(hdc, st->theme.pal.labelPrimary.ToColorRef());
                SetBkColor(hdc, st->theme.pal.contentCardElevated.ToColorRef());
                return reinterpret_cast<LRESULT>(st->brInput);
            }
            SetTextColor(hdc, st->theme.pal.labelPrimary.ToColorRef());
            SetBkColor(hdc, st->theme.pal.contentPaper.ToColorRef());
            return reinterpret_cast<LRESULT>(st->brPaper);
        }
        LRESULT themeRes = 0;
        if (st->theme.HandleMessage(hwnd, msg, wp, lp, themeRes)) return themeRes;
    }

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        st = reinterpret_cast<TecHubState*>(cs->lpCreateParams);
        st->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->dpi = UiGlass::DpiOfWindow(hwnd);
        st->theme.Init(hwnd, st->overrideGlass);
        st->brSidebar = CreateSolidBrush(st->theme.pal.sidebarBg.ToColorRef());
        st->brPaper   = CreateSolidBrush(st->theme.pal.contentPaper.ToColorRef());
        st->brCard    = CreateSolidBrush(st->theme.pal.contentCard.ToColorRef());
        st->brInput   = CreateSolidBrush(st->theme.pal.contentCardElevated.ToColorRef());

        st->fNorm = CreateFontW(-st->Sc(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        st->fBold = CreateFontW(-st->Sc(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        st->fTitle = CreateFontW(-st->Sc(21), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        st->fSmall = CreateFontW(-st->Sc(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Segoe UI");

        auto mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD style, int id, HFONT font = nullptr, DWORD ex = 0) -> HWND {
            HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | WS_VISIBLE | style,
                                     0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     GetModuleHandleW(nullptr), nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : st->fNorm), TRUE);
            return c;
        };
        auto mkBtn = [&](const wchar_t* txt, int id, HFONT font = nullptr) -> HWND {
            HWND c = CreateWindowExW(0, L"BUTTON", txt, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                     0, 0, 10, 10, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     GetModuleHandleW(nullptr), nullptr);
            SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : st->fNorm), TRUE);
            SetWindowSubclass(c, HigBtnSubclassProc, 1, reinterpret_cast<DWORD_PTR>(st));
            return c;
        };
        auto padEdit = [&](HWND hEd) {
            if (hEd) SendMessageW(hEd, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(st->Sc(10), st->Sc(10)));
        };

        static const wchar_t* kNavLabels[7] = {
            L"Resumen",
            L"Cursos",
            L"Calendario",
            L"Expediente y Malla",
            L"Horarios",
            L"Biblioteca",
            L"Trámites"
        };
        for (int i = 0; i < 7; ++i) {
            st->navBtns[i] = mkBtn(kNavLabels[i], IDC_NAV_BASE + i, st->fBold);
        }
        st->hSessionBadge = mk(L"STATIC", L"Sin sesión · Caché offline", SS_LEFT, 0, st->fSmall);
        ShowWindow(st->hSessionBadge, SW_HIDE);
        st->hBtnSync = mkBtn(L"Sincronizar ahora", IDC_BTN_SYNC_NOW, st->fBold);
        st->hBtnOpenWeb = mkBtn(L"Abrir en TEC Digital", IDC_BTN_OPEN_WEB);

        st->hTitle = mk(L"STATIC", L"Resumen Académico", SS_LEFT, 0, st->fTitle);
        st->hSubtitle = mk(L"STATIC", L"", SS_LEFT, 0, st->fSmall);
        st->hProgressBanner = mk(L"STATIC", L"Modo Solo Lectura activo · Sin mutaciones en la plataforma", SS_LEFT, 0, st->fSmall);

        // Section 0: Resumen & Cuenta
        st->hEmail = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_EMAIL);
        SendMessageW(st->hEmail, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"usuario@estudiantec.cr"));
        st->hPass = mk(L"EDIT", L"", WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL, IDC_EDIT_PASS);
        SendMessageW(st->hPass, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Contraseña"));
        st->hBtnLogin = mkBtn(L"Iniciar sesión", IDC_BTN_DO_LOGIN, st->fBold);
        st->hBtnQr = mkBtn(L"QR", IDC_BTN_DO_QR);
        st->hBtnLogout = mkBtn(L"Cerrar sesión local", IDC_BTN_LOGOUT_LOCAL);
        st->hChkDpapi = mk(L"BUTTON", L"Recordar sesión cifrada localmente", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_CHK_REMEMBER_DPAPI);
        SendMessageW(st->hChkDpapi, BM_SETCHECK, BST_CHECKED, 0);
        st->hSummaryEdit = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_SUMMARY_REPORT);
        ShowWindow(st->hSummaryEdit, SW_HIDE);
        static bool regSum = false;
        if (!regSum) {
            WNDCLASSEXW sc{};
            sc.cbSize = sizeof(sc);
            sc.lpfnWndProc = SummaryCanvasProc;
            sc.hInstance = GetModuleHandleW(nullptr);
            sc.lpszClassName = L"CuadernUTecSummaryCanvas";
            sc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            RegisterClassExW(&sc);
            regSum = true;
        }
        st->hSummaryCanvas = CreateWindowExW(0, L"CuadernUTecSummaryCanvas", L"",
                                             WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                                             reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SUMMARY_CANVAS)),
                                             GetModuleHandleW(nullptr), nullptr);
        SetWindowLongPtrW(st->hSummaryCanvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->secCtrls[0] = { st->hEmail, st->hPass, st->hBtnLogin, st->hBtnQr, st->hBtnLogout, st->hChkDpapi, st->hSummaryCanvas };

        // Section 1: Cursos y Comunidades
        st->hCmbPeriod = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_PERIOD);
        st->hLstCourses = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IDC_LST_COURSES);
        st->hCmbCourseSub = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_COURSE_SUBTAB);
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Portada y Cronograma"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Documentos"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Evaluaciones y Simulador"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Actividades y Foros"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Personas y Avisos"));
        SendMessageW(st->hCmbCourseSub, CB_SETCURSEL, 0, 0);
        st->hEditWhatIf = mk(L"EDIT", L"70", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_WHATIF_TARGET);
        st->hBtnWhatIf = mkBtn(L"Simular meta", IDC_BTN_WHATIF_CALC);
        st->hBtnCourseRubric = mkBtn(L"Ver rúbrica", IDC_BTN_COURSE_RUBRIC);
        st->hCourseDetail = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_COURSE_DETAIL);
        padEdit(st->hCourseDetail);
        st->hLstCourseFiles = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IDC_LST_COURSE_FILES);
        HWND bCIns = mkBtn(L"Insertar en lienzo", IDC_BTN_COURSE_INSERT_CANVAS, st->fBold);
        HWND bCSync = mkBtn(L"Sincronizar archivos", IDC_BTN_COURSE_SYNC_FILES);
        st->hBtnCourseOpenWeb = mkBtn(L"Abrir web", IDC_BTN_COURSE_OPEN_WEB);
        ShowWindow(st->hBtnCourseOpenWeb, SW_HIDE);
        HWND bCMore = mkBtn(L"Más acciones...", IDC_BTN_COURSE_MORE);
        st->secCtrls[1] = { st->hCmbPeriod, st->hLstCourses, st->hCmbCourseSub, st->hEditWhatIf, st->hBtnWhatIf,
                            st->hBtnCourseRubric, st->hCourseDetail, st->hLstCourseFiles, bCIns, bCSync, bCMore };
        if (st->onSendToGemini) {
            st->hBtnCourseGemini = mkBtn(L"Gemini Notebook", IDC_BTN_COURSE_SEND_GEMINI);
            ShowWindow(st->hBtnCourseGemini, SW_HIDE);
        }

        // Section 2: Calendario
        st->hCmbCalFilter = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_CAL_FILTER);
        SendMessageW(st->hCmbCalFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todos los eventos"));
        SendMessageW(st->hCmbCalFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Entregas de cursos"));
        SendMessageW(st->hCmbCalFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Citas de matrícula"));
        SendMessageW(st->hCmbCalFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Eventos de calendario"));
        SendMessageW(st->hCmbCalFilter, CB_SETCURSEL, 0, 0);
        st->hCmbCalView = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_CAL_VIEW);
        SendMessageW(st->hCmbCalView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Vista de Lista"));
        SendMessageW(st->hCmbCalView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Vista Semanal Visual"));
        SendMessageW(st->hCmbCalView, CB_SETCURSEL, 0, 0);
        HWND bIcs = mkBtn(L"Exportar .ics...", IDC_BTN_CAL_EXPORT_ICS, st->fBold);
        st->hCalList = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_CAL_LIST);
        padEdit(st->hCalList);
        static bool regCal = false;
        if (!regCal) {
            WNDCLASSEXW cc{};
            cc.cbSize = sizeof(cc);
            cc.lpfnWndProc = CalendarCanvasProc;
            cc.hInstance = GetModuleHandleW(nullptr);
            cc.lpszClassName = L"CuadernUTecCalCanvas";
            cc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            RegisterClassExW(&cc);
            regCal = true;
        }
        st->hCalCanvas = CreateWindowExW(0, L"CuadernUTecCalCanvas", L"",
                                         WS_CHILD, 0, 0, 10, 10, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_CAL_CANVAS)),
                                         GetModuleHandleW(nullptr), nullptr);
        SetWindowLongPtrW(st->hCalCanvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->secCtrls[2] = { st->hCmbCalFilter, st->hCmbCalView, bIcs, st->hCalList, st->hCalCanvas };

        // Section 3: Expediente y Malla Curricular
        st->hCmbExpView = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_EXP_VIEW);
        SendMessageW(st->hCmbExpView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Estadísticas, Notas y Citas"));
        SendMessageW(st->hCmbExpView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Malla Curricular Interactiva"));
        SendMessageW(st->hCmbExpView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Atributos de Graduación"));
        SendMessageW(st->hCmbExpView, CB_SETCURSEL, 0, 0);
        st->hCmbGradesFilter = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_GRADES_FILTER);
        SendMessageW(st->hCmbGradesFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todas las materias"));
        SendMessageW(st->hCmbGradesFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solo aprobadas"));
        SendMessageW(st->hCmbGradesFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solo reprobadas"));
        SendMessageW(st->hCmbGradesFilter, CB_SETCURSEL, 0, 0);
        st->hExpReport = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_EXP_REPORT);
        padEdit(st->hExpReport);
        st->hBtnMallaZIn = mkBtn(L"Zoom +", IDC_BTN_MALLA_ZOOM_IN);
        st->hBtnMallaZOut = mkBtn(L"Zoom -", IDC_BTN_MALLA_ZOOM_OUT);
        st->hBtnMallaInsert = mkBtn(L"Insertar avance en lienzo", IDC_BTN_MALLA_INSERT, st->fBold);
        st->hCmbMallaSede = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_MALLA_SEDE);
        st->hCmbMallaCarrera = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_MALLA_CARRERA);
        st->hCmbMallaPlan = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_MALLA_PLAN);
        st->hBtnMallaLoad = mkBtn(L"Cargar plan", IDC_BTN_MALLA_LOAD_PLAN);

        static bool regMalla = false;
        if (!regMalla) {
            WNDCLASSEXW mc{};
            mc.cbSize = sizeof(mc);
            mc.lpfnWndProc = MallaCanvasProc;
            mc.hInstance = GetModuleHandleW(nullptr);
            mc.lpszClassName = L"CuadernUTecMallaCanvas";
            mc.hCursor = LoadCursorW(nullptr, IDC_HAND);
            RegisterClassExW(&mc);
            regMalla = true;
        }
        st->hMallaCanvas = CreateWindowExW(0, L"CuadernUTecMallaCanvas", L"",
                                           WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MALLA_CANVAS)),
                                           GetModuleHandleW(nullptr), nullptr);
        SetWindowLongPtrW(st->hMallaCanvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        static bool regAttr = false;
        if (!regAttr) {
            WNDCLASSEXW ac{};
            ac.cbSize = sizeof(ac);
            ac.lpfnWndProc = AttrCanvasProc;
            ac.hInstance = GetModuleHandleW(nullptr);
            ac.lpszClassName = L"CuadernUTecAttrCanvas";
            ac.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            RegisterClassExW(&ac);
            regAttr = true;
        }
        st->hAttrCanvas = CreateWindowExW(0, L"CuadernUTecAttrCanvas", L"",
                                          WS_CHILD, 0, 0, 10, 10, hwnd,
                                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_ATTR_CANVAS)),
                                          GetModuleHandleW(nullptr), nullptr);
        SetWindowLongPtrW(st->hAttrCanvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hMallaInspector = mk(L"EDIT", L"Haga clic en una materia para resaltar requisitos, correquisitos y cursos que desbloquea. Arrastre para desplazar o use la rueda para zoom.",
                                 WS_VSCROLL | ES_MULTILINE | ES_READONLY, IDC_MALLA_INSPECTOR);
        padEdit(st->hMallaInspector);
        st->secCtrls[3] = { st->hCmbExpView, st->hCmbGradesFilter, st->hExpReport,
                            st->hCmbMallaSede, st->hCmbMallaCarrera, st->hCmbMallaPlan, st->hBtnMallaLoad,
                            st->hBtnMallaZIn, st->hBtnMallaZOut, st->hBtnMallaInsert,
                            st->hMallaCanvas, st->hAttrCanvas, st->hMallaInspector };

        // Section 4: Planificador Horarios
        st->hCmbHorSource = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_SOURCE);
        SendMessageW(st->hCmbHorSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Guía Pública · Escuelas"));
        SendMessageW(st->hCmbHorSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Guía Pública · Verano"));
        SendMessageW(st->hCmbHorSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Guía Pública · Carné"));
        SendMessageW(st->hCmbHorSource, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Expediente TEC"));
        SendMessageW(st->hCmbHorSource, CB_SETCURSEL, 0, 0);

        st->hCmbHorEscuela = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_ESCUELA);
        SeedDefaultPublicEscuelas(st);
        RefreshPublicEscuelasCombo(st);

        st->hCmbHorYear = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_YEAR);
        SendMessageW(st->hCmbHorYear, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2026"));
        SendMessageW(st->hCmbHorYear, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2025"));
        SendMessageW(st->hCmbHorYear, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"2027"));
        SendMessageW(st->hCmbHorYear, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Último (Carné)"));
        SendMessageW(st->hCmbHorYear, CB_SETCURSEL, 0, 0);

        st->hCmbHorPubSede = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_PUB_SEDE);
        SendMessageW(st->hCmbHorPubSede, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todas las sedes"));
        SendMessageW(st->hCmbHorPubSede, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cartago"));
        SendMessageW(st->hCmbHorPubSede, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"San José"));
        SendMessageW(st->hCmbHorPubSede, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"San Carlos"));
        SendMessageW(st->hCmbHorPubSede, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Alajuela"));
        SendMessageW(st->hCmbHorPubSede, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Limón"));
        SendMessageW(st->hCmbHorPubSede, CB_SETCURSEL, 0, 0);

        st->hCmbHorPubPer = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_PUB_PER);
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todos los periodos"));
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Semestre 1"));
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Semestre 2"));
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Verano"));
        SendMessageW(st->hCmbHorPubPer, CB_SETCURSEL, 0, 0);

        st->hEditHorCarnet = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, IDC_EDIT_HOR_CARNET);
        SendMessageW(st->hEditHorCarnet, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Número de carné (opcional si inició sesión)..."));

        st->hCmbHorSede = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_SEDE);
        st->hCmbHorCarrera = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_CARRERA);
        st->hCmbHorPeriodo = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_PERIODO);
        st->hChkHorPrereq = mk(L"BUTTON", L"Filtrar elegibles por malla", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_CHK_HOR_PREREQ);
        st->hChkHorNoNight = mk(L"BUTTON", L"Sin nocturnas", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_CHK_HOR_NO_NIGHT);
        st->hChkHorSeats = mk(L"BUTTON", L"Solo con cupo", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_CHK_HOR_SEATS);
        st->hChkHorBusy = mk(L"BUTTON", L"Respetar ocupados", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_CHK_HOR_BUSY);
        st->hCmbHorFreeDay = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_FREEDAY);
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sin día libre fijo"));
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Día libre: Lunes"));
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Día libre: Martes"));
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Día libre: Miércoles"));
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Día libre: Jueves"));
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Día libre: Viernes"));
        SendMessageW(st->hCmbHorFreeDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Día libre: Sábado"));
        SendMessageW(st->hCmbHorFreeDay, CB_SETCURSEL, 0, 0);
        st->hEditHorFilter = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_HOR_FILTER);
        SendMessageW(st->hEditHorFilter, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Cursos (ej. IC1802, MA1103), modalidad (Virtual) o excluir profesor..."));
        HWND bHLoad = mkBtn(L"Cargar...", IDC_BTN_HOR_LOAD_JSON);
        HWND bHSolve = mkBtn(L"Resolver Horarios", IDC_BTN_HOR_SOLVE, st->fBold);
        static bool regHor = false;
        if (!regHor) {
            WNDCLASSEXW hc{};
            hc.cbSize = sizeof(hc);
            hc.lpfnWndProc = HorariosCanvasProc;
            hc.hInstance = GetModuleHandleW(nullptr);
            hc.lpszClassName = L"CuadernUTecHorariosCanvas";
            hc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
            RegisterClassExW(&hc);
            regHor = true;
        }
        st->hHorCanvas = CreateWindowExW(0, L"CuadernUTecHorariosCanvas", L"",
                                         WS_CHILD | WS_VISIBLE, 0, 0, 10, 10, hwnd,
                                         reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_HOR_CANVAS)),
                                         GetModuleHandleW(nullptr), nullptr);
        SetWindowLongPtrW(st->hHorCanvas, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        HWND bHPrev = mkBtn(L"Anterior", IDC_BTN_HOR_PREV);
        HWND bHNext = mkBtn(L"Siguiente", IDC_BTN_HOR_NEXT);
        st->hHorStatus = mk(L"STATIC", L"Sin combinaciones calculadas", SS_LEFT, IDC_HOR_STATUS, st->fSmall);
        HWND bHIns = mkBtn(L"Insertar en lienzo", IDC_BTN_HOR_INSERT);
        HWND bHExp = mkBtn(L"Exportar...", IDC_BTN_HOR_EXPORT);
        st->secCtrls[4] = { st->hCmbHorSource, st->hCmbHorEscuela, st->hCmbHorYear,
                            st->hCmbHorPubSede, st->hCmbHorPubPer, st->hEditHorCarnet,
                            st->hCmbHorSede, st->hCmbHorCarrera, st->hCmbHorPeriodo,
                            st->hChkHorPrereq, st->hChkHorNoNight, st->hChkHorSeats, st->hChkHorBusy,
                            st->hCmbHorFreeDay, st->hEditHorFilter,
                            bHLoad, bHSolve, st->hHorCanvas, bHPrev, bHNext, st->hHorStatus, bHIns, bHExp };

        // Section 5: Biblioteca Offline
        st->hEditFts = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_FTS_QUERY);
        SendMessageW(st->hEditFts, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Buscar en materiales descargados y en el cuaderno activo..."));
        HWND bFtsGo = mkBtn(L"Buscar", IDC_BTN_FTS_SEARCH, st->fBold);
        st->hLstFts = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IDC_LST_FTS_HITS);
        st->hFtsPreview = mk(L"EDIT", L"Seleccione una coincidencia para previsualizar el fragmento y origen.",
                             WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_FTS_PREVIEW);
        padEdit(st->hFtsPreview);
        st->hLblCache = mk(L"STATIC", L"Caché offline: 0.0 MB", SS_LEFT, IDC_LBL_CACHE_USAGE, st->fSmall);
        HWND bFtsOpen = mkBtn(L"Abrir archivo", IDC_BTN_FTS_OPEN);
        HWND bFtsIns = mkBtn(L"Insertar en lienzo", IDC_BTN_FTS_INSERT);
        HWND bPrune = mkBtn(L"Limpiar caché", IDC_BTN_CACHE_PRUNE);
        st->secCtrls[5] = { st->hEditFts, bFtsGo, st->hLstFts, st->hFtsPreview, st->hLblCache, bFtsOpen, bFtsIns, bPrune };

        // Section 6: Tramites y Servicios
        st->hCmbTramite = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_TRAMITE_CAT);
        SendMessageW(st->hCmbTramite, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Control de Plagio (Solo Lectura)"));
        SendMessageW(st->hCmbTramite, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Prematrícula, Inscripción, Inclusiones y Feria"));
        SendMessageW(st->hCmbTramite, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Suscripciones, Escuelas y Perfil de Aprendizaje"));
        SendMessageW(st->hCmbTramite, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Servicios Estudiantiles TEC"));
        SendMessageW(st->hCmbTramite, CB_SETCURSEL, 0, 0);
        st->hTramiteReport = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_TRAMITE_REPORT);
        padEdit(st->hTramiteReport);
        st->hBtnSvcMapa = mkBtn(L"Mapa del Campus", IDC_BTN_SVC_MAPA);
        ShowWindow(st->hBtnSvcMapa, SW_HIDE);
        st->hBtnSvcGuia = mkBtn(L"Guía Estudiantes", IDC_BTN_SVC_GUIA);
        ShowWindow(st->hBtnSvcGuia, SW_HIDE);
        st->hBtnSvcSoporte = mkBtn(L"Soporte TEC", IDC_BTN_SVC_SOPORTE);
        ShowWindow(st->hBtnSvcSoporte, SW_HIDE);
        HWND bS4 = mkBtn(L"Control de Plagio", IDC_BTN_SVC_TURNITIN);
        HWND bSMore = mkBtn(L"Servicios TEC...", IDC_BTN_SVC_MORE);
        st->secCtrls[6] = { st->hCmbTramite, st->hTramiteReport, bS4, bSMore };

        st->theme.ThemeAllChildren(hwnd);
        if (st->currentSection == HubSection::Cursos && st->initialSubView >= 0 && st->initialSubView <= 4) {
            SendMessageW(st->hCmbCourseSub, CB_SETCURSEL, static_cast<WPARAM>(st->initialSubView), 0);
        } else if (st->currentSection == HubSection::ExpedienteYMalla && st->initialSubView >= 0 && st->initialSubView <= 2) {
            SendMessageW(st->hCmbExpView, CB_SETCURSEL, static_cast<WPARAM>(st->initialSubView), 0);
        } else if (st->currentSection == HubSection::TramitesYPerfil && st->initialSubView >= 0 && st->initialSubView <= 3) {
            SendMessageW(st->hCmbTramite, CB_SETCURSEL, static_cast<WPARAM>(st->initialSubView), 0);
        }
        RefreshAllSections(st);
        LayoutDialog(st);
        SwitchSection(st, st->currentSection);
        return 0;
    }

    case WM_SIZE:
        if (st) {
            LayoutDialog(st);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_APP_TEC_PROGRESS:
        if (st && st->hProgressBanner) {
            std::string s;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                s = st->pendingWorkStatus;
            }
            SetWindowTextW(st->hProgressBanner, Utf8ToWide(s).c_str());
        }
        return 0;

    case WM_APP_TEC_WORK_DONE:
        if (st) {
            std::string s;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                s = st->pendingWorkStatus;
            }
            SetWindowTextW(st->hProgressBanner, Utf8ToWide(s).c_str());
            if (st->outStatusBanner) *st->outStatusBanner = s;
            RefreshAllSections(st);
            SwitchSection(st, st->currentSection);
        }
        return 0;

    case WM_APP_TEC_HORARIOS_DONE:
        if (st) {
            DocStudio::TecDigital::LiveScheduleSolveResult res;
            std::string key, statusMsg;
            bool usedReqFilter = false;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                res = std::move(st->pendingHorariosResult);
                key = std::move(st->pendingHorariosCatalogKey);
                statusMsg = std::move(st->pendingHorariosStatus);
                usedReqFilter = st->pendingHorariosUsedFilter;
            }
            const bool isOk = (wp != 0);
            const bool wasMerge = (lp != 0);
            if (isOk) {
                st->horariosCatalog = std::move(res.catalog);
                st->horariosResult = std::move(res.solved);
                st->horariosCatalogKey = std::move(key);
                st->horariosUsedRequiredFilter = usedReqFilter;
                st->selectedScheduleIdx = 0;
                SetWindowTextW(st->hProgressBanner, L"Consulta de horarios completada (0 mutaciones).");
            } else {
                if (!wasMerge) {
                    st->horariosCatalog = ht::CourseCatalog{};
                    st->horariosResult = ht::ScheduleResult{};
                    st->horariosCatalogKey.clear();
                    st->horariosUsedRequiredFilter = usedReqFilter;
                }
                SetWindowTextW(st->hProgressBanner, Utf8ToWide(statusMsg).c_str());
            }
            SetWindowTextW(st->hHorStatus, Utf8ToWide(statusMsg).c_str());
            if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
        }
        return 0;

    case WM_APP_TEC_ESCUELAS_DONE:
        if (st) {
            std::vector<td::PublicGuiaEscuelaInfo> esc;
            std::string statusMsg;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                esc = std::move(st->pendingPublicEscuelas);
                statusMsg = std::move(st->pendingWorkStatus);
            }
            if (!esc.empty()) {
                st->publicEscuelas = std::move(esc);
                RefreshPublicEscuelasCombo(st);
            }
            if (!statusMsg.empty()) {
                SetWindowTextW(st->hProgressBanner, Utf8ToWide(statusMsg).c_str());
            }
        }
        return 0;

    case WM_COMMAND: {
        if (!st) break;
        const int id = LOWORD(wp);
        const int code = HIWORD(wp);

        if (id >= IDC_NAV_BASE && id < IDC_NAV_BASE + 7 && code == BN_CLICKED) {
            SwitchSection(st, static_cast<HubSection>(id - IDC_NAV_BASE));
            return 0;
        }
        if (id == IDC_BTN_SYNC_NOW && code == BN_CLICKED) {
            StartAsyncSync(st);
            return 0;
        }
        if (id == IDC_BTN_OPEN_WEB && code == BN_CLICKED) {
            OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/dotlrn/");
            return 0;
        }
        if (id == IDC_BTN_DO_LOGIN && code == BN_CLICKED) {
            if (st->workerBusy.exchange(true)) return 0;
            std::string email = WideToUtf8(GetCtrlTextW(st->hEmail));
            std::string pass = WideToUtf8(GetCtrlTextW(st->hPass));
            bool persist = (SendMessageW(st->hChkDpapi, BM_GETCHECK, 0, 0) == BST_CHECKED);
            SetWindowTextW(st->hPass, L""); // Immediately clear password field from UI memory
            SetWindowTextW(st->hProgressBanner, L"Autenticando con TEC Digital...");
            if (st->workerThread.joinable()) st->workerThread.join();
            st->workerThread = std::thread([st, email, pass, persist]() mutable {
                auto res = st->store->LoginWithPassword(email, pass, persist);
                std::fill(pass.begin(), pass.end(), '\0');
                {
                    std::lock_guard<std::mutex> lk(st->workResultMutex);
                    st->pendingWorkKind = 1;
                    st->pendingWorkStatus = res.ok ? "Sesión iniciada correctamente (Solo Lectura)."
                                                   : ("Error de autenticación: " + res.message);
                }
                st->workerBusy.store(false);
                PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, res.ok ? 1 : 0, 0);
            });
            return 0;
        }
        if (id == IDC_BTN_DO_QR && code == BN_CLICKED) {
            if (st->workerBusy.exchange(true)) return 0;
            std::string email = WideToUtf8(GetCtrlTextW(st->hEmail));
            SetWindowTextW(st->hProgressBanner, L"Verificando acceso mediante QR...");
            if (st->workerThread.joinable()) st->workerThread.join();
            st->workerThread = std::thread([st, email]() {
                auto res = st->store->LoginWithQr(email);
                {
                    std::lock_guard<std::mutex> lk(st->workResultMutex);
                    st->pendingWorkStatus = res.ok ? "Sesión QR activa (Solo Lectura)."
                                                   : ("QR: " + res.message);
                }
                st->workerBusy.store(false);
                PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, res.ok ? 1 : 0, 0);
            });
            return 0;
        }
        if (id == IDC_BTN_LOGOUT_LOCAL && code == BN_CLICKED) {
            st->store->LogoutLocalOnly(true);
            SetWindowTextW(st->hProgressBanner, L"Sesión local cerrada y bóveda local eliminada (sin afectar navegadores).");
            RefreshAllSections(st);
            return 0;
        }
        if ((id == IDC_CMB_PERIOD && code == CBN_SELCHANGE)) {
            RefreshCourseList(st);
            RefreshSelectedCourseDetail(st);
            return 0;
        }
        if ((id == IDC_LST_COURSES && code == LBN_SELCHANGE) ||
            (id == IDC_CMB_COURSE_SUBTAB && code == CBN_SELCHANGE) ||
            (id == IDC_BTN_WHATIF_CALC && code == BN_CLICKED)) {
            RefreshSelectedCourseDetail(st);
            return 0;
        }
        if (id == IDC_BTN_COURSE_INSERT_CANVAS && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
                int subTab = st->hCmbCourseSub ? static_cast<int>(SendMessageW(st->hCmbCourseSub, CB_GETCURSEL, 0, 0)) : 0;
                if (subTab == 1 && st->hLstCourseFiles) {
                    int fIdx = static_cast<int>(SendMessageW(st->hLstCourseFiles, LB_GETCURSEL, 0, 0));
                    if (fIdx >= 0 && static_cast<size_t>(fIdx) < st->currentCourseFiles.size()) {
                        const auto& fr = st->currentCourseFiles[static_cast<size_t>(fIdx)];
                        std::string rel = !fr.logical_rel_path.empty() ? fr.logical_rel_path : fr.blob_rel_path;
                        std::string fullPath = rel.empty() ? "" : (st->store->OfflineFilesRootUtf8() + "\\" + rel);
                        DocStudio::TecDigital::TecAcademicStore::ImportDownloadedMaterialToCanvas(
                            *st->activeDoc, fullPath, fr.name, c.name, fr.download_url);
                        st->docMutated = true;
                        if (st->onDocMutated) st->onDocMutated();
                        SetWindowTextW(st->hProgressBanner, L"Archivo del curso insertado en el cuaderno activo.");
                        return 0;
                    }
                }
                auto ovIt = st->snap.overviewsByPath.find(c.url);
                auto evIt = st->snap.evaluationsByPath.find(c.url);
                DocStudio::TecDigital::TecAcademicStore::InsertCourseOverviewToCanvas(
                    *st->activeDoc, c,
                    ovIt != st->snap.overviewsByPath.end() ? &ovIt->second : nullptr,
                    evIt != st->snap.evaluationsByPath.end() ? &evIt->second : nullptr);
                st->docMutated = true;
                if (st->onDocMutated) st->onDocMutated();
                SetWindowTextW(st->hProgressBanner, L"Resumen del curso insertado en el cuaderno activo.");
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_MORE && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            if (idx < 0 || static_cast<size_t>(idx) >= st->filteredCourses.size()) return 0;
            const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
            HMENU hm = CreatePopupMenu();
            AppendMenuW(hm, MF_STRING, 1, L"Crear cuaderno del curso (.cnu)...");
            AppendMenuW(hm, MF_STRING, 2, L"Descargar archivo seleccionado");
            AppendMenuW(hm, MF_STRING, 3, L"Generar Flashcards SM-2 para el mazo");
            AppendMenuW(hm, MF_STRING, 4, L"Abrir adjunto / solución de evaluación");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 5, L"Historial de versiones del archivo...");
            AppendMenuW(hm, MF_STRING, 6, L"Historial de entregas y revisión vigente...");
            AppendMenuW(hm, MF_STRING, 7, L"Leer hilo de foro del curso...");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 8, L"Abrir curso en TEC Digital");
            if (st->onSendToGemini) {
                AppendMenuW(hm, MF_STRING, 9, L"Enviar curso a Gemini Notebook");
            }
            RECT brc{};
            GetWindowRect(GetDlgItem(hwnd, IDC_BTN_COURSE_MORE), &brc);
            POINT pt{ brc.left, brc.bottom + st->Sc(4) };
            UINT pick = UiGlass::TrackTranslucentPopupMenu(hm, pt, hwnd, st->theme.settings, false);
            DestroyMenu(hm);
            auto ovIt = st->snap.overviewsByPath.find(c.url);
            auto evIt = st->snap.evaluationsByPath.find(c.url);
            if (pick == 1) {
                std::wstring outPath;
                std::wstring defName = Utf8ToWide((c.slug.empty() ? "curso_tec" : c.slug) + ".cnu");
                if (PickSaveFileW(hwnd, L"Cuaderno CuadernU (*.cnu)\0*.cnu\0", L"cnu", defName.c_str(), outPath)) {
                    std::string err;
                    if (DocStudio::TecDigital::TecAcademicStore::SaveCourseNotebookCnu(
                            c,
                            ovIt != st->snap.overviewsByPath.end() ? &ovIt->second : nullptr,
                            evIt != st->snap.evaluationsByPath.end() ? &evIt->second : nullptr,
                            outPath, &err)) {
                        SetWindowTextW(st->hProgressBanner, L"Cuaderno del curso (.cnu) creado correctamente.");
                    } else {
                        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Error al guardar cuaderno: " + err).c_str());
                    }
                }
            } else if (pick == 2 && st->hLstCourseFiles) {
                int fIdx = static_cast<int>(SendMessageW(st->hLstCourseFiles, LB_GETCURSEL, 0, 0));
                if (fIdx >= 0 && static_cast<size_t>(fIdx) < st->currentCourseFiles.size() && !st->workerBusy.exchange(true)) {
                    std::string commUrl = c.url;
                    std::string oid = st->currentCourseFiles[static_cast<size_t>(fIdx)].object_id;
                    SetWindowTextW(st->hProgressBanner, L"Descargando archivo seleccionado...");
                    if (st->workerThread.joinable()) st->workerThread.join();
                    st->workerThread = std::thread([st, commUrl, oid]() {
                        std::string localP, err;
                        bool ok = st->store->DownloadSingleCourseFile(commUrl, oid, &localP, &err);
                        {
                            std::lock_guard<std::mutex> lk(st->workResultMutex);
                            st->pendingWorkStatus = ok ? "Archivo descargado e indexado localmente." : ("Descarga: " + err);
                        }
                        st->workerBusy.store(false);
                        PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, ok ? 1 : 0, 0);
                    });
                }
            } else if (pick == 3) {
                auto drafts = DocStudio::TecDigital::TecAcademicStore::GenerateSm2FlashcardsFromCourse(
                    c,
                    ovIt != st->snap.overviewsByPath.end() ? &ovIt->second : nullptr,
                    evIt != st->snap.evaluationsByPath.end() ? &evIt->second : nullptr);
                if (drafts.empty()) {
                    SetWindowTextW(st->hProgressBanner, L"Sincronice portada o evaluaciones del curso para generar tarjetas SM-2.");
                } else if (st->onAddSm2) {
                    const std::size_t added = st->onAddSm2(drafts);
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide(std::to_string(added) + " tarjeta(s) SM-2 agregadas al mazo de estudio.").c_str());
                }
            } else if (pick == 4 && evIt != st->snap.evaluationsByPath.end()) {
                std::string targetUrl;
                for (const auto& cat : evIt->second.categories) {
                    for (const auto& it : cat.items) {
                        if (!it.attachment_url.empty()) { targetUrl = it.attachment_url; break; }
                        if (!it.solution_url.empty()) { targetUrl = it.solution_url; break; }
                    }
                    if (!targetUrl.empty()) break;
                }
                if (!targetUrl.empty()) OpenSafeUrl(hwnd, targetUrl);
                else SetWindowTextW(st->hProgressBanner, L"No se encontraron adjuntos o soluciones en las evaluaciones cargadas.");
            } else if (pick == 5) {
                std::string oid, fname;
                if (st->hLstCourseFiles) {
                    int fIdx = static_cast<int>(SendMessageW(st->hLstCourseFiles, LB_GETCURSEL, 0, 0));
                    if (fIdx >= 0 && static_cast<size_t>(fIdx) < st->currentCourseFiles.size()) {
                        oid = st->currentCourseFiles[static_cast<size_t>(fIdx)].object_id;
                        fname = st->currentCourseFiles[static_cast<size_t>(fIdx)].name;
                    }
                }
                if (oid.empty()) {
                    auto fIt = st->snap.foldersByPath.find(c.url);
                    if (fIt != st->snap.foldersByPath.end() && !fIt->second.files.empty()) {
                        oid = fIt->second.files.front().object_id;
                        fname = fIt->second.files.front().name;
                    }
                }
                if (oid.empty()) {
                    MessageBoxW(hwnd, L"Seleccione un documento o sincronice los archivos del curso primero.",
                                L"Historial de versiones", MB_OK | MB_ICONINFORMATION);
                } else {
                    auto vr = st->store->FetchFileVersions(oid);
                    std::vector<td::FileVersionRecord> versions;
                    if (vr.ok && !vr.value.empty()) {
                        st->snap = st->store->Snapshot();
                        versions = std::move(vr.value);
                    } else if (auto cit = st->snap.fileVersionsByObjectId.find(oid);
                               cit != st->snap.fileVersionsByObjectId.end() && !cit->second.empty()) {
                        versions = cit->second;
                    }
                    if (!versions.empty()) {
                        std::ostringstream voss;
                        voss << "Archivo: " << fname << "\nVersiones registradas: " << versions.size() << "\n\n";
                        for (size_t vi = 0; vi < versions.size() && vi < 12; ++vi) {
                            const auto& v = versions[vi];
                            voss << "• Rev. " << (v.live_revision.empty() ? std::to_string(vi + 1) : v.live_revision)
                                 << " — " << (v.title.empty() ? fname : v.title);
                            if (!v.last_modified_ansi.empty()) voss << " (" << v.last_modified_ansi << ")";
                            if (v.content_size > 0) voss << " · " << (v.content_size / 1024) << " KB";
                            if (!v.author.empty()) voss << " · " << v.author;
                            if (!v.description.empty()) voss << "\n    " << v.description;
                            voss << "\n";
                        }
                        MessageBoxW(hwnd, Utf8ToWide(voss.str()).c_str(), L"Historial de versiones", MB_OK);
                    } else {
                        std::string msgErr = vr.ok ? "No se registran versiones adicionales para este archivo."
                                                   : ("No fue posible consultar el historial: " + vr.error_message);
                        MessageBoxW(hwnd, Utf8ToWide(msgErr).c_str(), L"Historial de versiones", MB_OK | MB_ICONINFORMATION);
                    }
                }
            } else if (pick == 6) {
                std::string assignId, taskItemId, taskId, taskName;
                if (evIt != st->snap.evaluationsByPath.end()) {
                    for (const auto& cat : evIt->second.categories) {
                        for (const auto& it : cat.items) {
                            if (!it.task_item_id.empty() || !it.assign_id.empty()) {
                                assignId = it.assign_id;
                                taskItemId = !it.task_item_id.empty() ? it.task_item_id : it.assign_id;
                                taskId = it.task_id;
                                taskName = it.name;
                                break;
                            }
                        }
                        if (!taskItemId.empty()) break;
                    }
                }
                if (taskItemId.empty()) {
                    MessageBoxW(hwnd, L"No se encontraron evaluaciones con entregas cargadas para este curso.",
                                L"Historial de entregas", MB_OK | MB_ICONINFORMATION);
                } else {
                    auto hr = st->store->FetchEvaluationHistory(c.url, assignId, taskItemId, taskId, taskName);
                    std::vector<td::EvalSubmissionRevision> revs;
                    const std::string histKey = assignId.empty() ? taskItemId : assignId;
                    if (hr.ok && !hr.value.empty()) {
                        st->snap = st->store->Snapshot();
                        RefreshSelectedCourseDetail(st);
                        revs = std::move(hr.value);
                    } else if (auto cit = st->snap.evalHistoryByAssignId.find(histKey);
                               cit != st->snap.evalHistoryByAssignId.end() && !cit->second.empty()) {
                        revs = cit->second;
                    }
                    if (!revs.empty()) {
                        std::ostringstream hoss;
                        hoss << "Evaluación: " << taskName << "\nRevisiones registradas: " << revs.size() << "\n\n";
                        for (size_t hi = 0; hi < revs.size() && hi < 12; ++hi) {
                            const auto& rv = revs[hi];
                            hoss << "• " << (rv.is_live_revision ? "[Vigente] " : "[Histórico] ")
                                 << (rv.file_name.empty() ? "Entrega" : rv.file_name);
                            if (!rv.delivered_date.empty()) hoss << " · " << rv.delivered_date;
                            if (!rv.delivered_time.empty()) hoss << " " << rv.delivered_time;
                            hoss << "\n";
                        }
                        MessageBoxW(hwnd, Utf8ToWide(hoss.str()).c_str(), L"Historial de entregas y revisión vigente", MB_OK);
                    } else {
                        std::string msgErr = hr.ok ? "No hay archivos entregados registrados para esta evaluación."
                                                   : ("No fue posible consultar el historial de entregas: " + hr.error_message);
                        MessageBoxW(hwnd, Utf8ToWide(msgErr).c_str(), L"Historial de entregas", MB_OK | MB_ICONINFORMATION);
                    }
                }
            } else if (pick == 7) {
                auto frIt = st->snap.forumsByPath.find(c.url);
                if (frIt == st->snap.forumsByPath.end() || frIt->second.empty()) {
                    MessageBoxW(hwnd, L"No hay hilos de foro sincronizados para este curso.",
                                L"Foros del curso", MB_OK | MB_ICONINFORMATION);
                } else {
                    const auto& th = frIt->second.front();
                    auto tr = st->store->FetchForumThread(c.url, th.message_id);
                    if (tr.ok && !tr.value.empty()) {
                        std::ostringstream foss;
                        foss << "Hilo: " << th.subject << "\nMensajes (" << tr.value.size() << "):\n\n";
                        for (size_t mi = 0; mi < tr.value.size() && mi < 10; ++mi) {
                            const auto& p = tr.value[mi];
                            foss << "• " << (p.author.empty() ? "Participante" : p.author);
                            if (!p.posted_date.empty()) foss << " (" << p.posted_date << ")";
                            foss << "\n  " << p.body_text << "\n\n";
                        }
                        MessageBoxW(hwnd, Utf8ToWide(foss.str()).c_str(), L"Lectura de foro (Solo lectura)", MB_OK);
                    } else {
                        std::string msgErr = tr.ok ? "El hilo seleccionado no contiene mensajes visibles."
                                                   : ("No fue posible leer el hilo de foro: " + tr.error_message);
                        MessageBoxW(hwnd, Utf8ToWide(msgErr).c_str(), L"Foros del curso", MB_OK | MB_ICONINFORMATION);
                    }
                }
            } else if (pick == 8) {
                OpenSafeUrl(hwnd, c.url);
            } else if (pick == 9 && st->onSendToGemini) {
                SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_COURSE_SEND_GEMINI, BN_CLICKED), 0);
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_SYNC_FILES && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size() && !st->workerBusy.exchange(true)) {
                std::string url = st->filteredCourses[static_cast<size_t>(idx)].url;
                SetWindowTextW(st->hProgressBanner, L"Sincronizando documentos del curso...");
                if (st->workerThread.joinable()) st->workerThread.join();
                st->workerThread = std::thread([st, url]() {
                    std::string sum, err;
                    bool ok = st->store->SyncCourseFiles(url, true, &sum, &err);
                    {
                        std::lock_guard<std::mutex> lk(st->workResultMutex);
                        st->pendingWorkStatus = ok ? ("Documentos sincronizados: " + sum) : ("Error: " + err);
                    }
                    st->workerBusy.store(false);
                    PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, ok ? 1 : 0, 0);
                });
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_OPEN_WEB && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                OpenSafeUrl(hwnd, st->filteredCourses[static_cast<size_t>(idx)].url);
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_SEND_GEMINI && code == BN_CLICKED && st->onSendToGemini) {
            int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
                auto ovIt = st->snap.overviewsByPath.find(c.url);
                std::vector<td::FileRecord> files;
                auto fIt = st->snap.foldersByPath.find(c.url);
                if (fIt != st->snap.foldersByPath.end()) files = fIt->second.files;
                std::string msgOut;
                st->onSendToGemini(c, ovIt != st->snap.overviewsByPath.end() ? &ovIt->second : nullptr, files, msgOut);
                if (!msgOut.empty()) {
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide(msgOut).c_str());
                }
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_RUBRIC && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
                auto evIt = st->snap.evaluationsByPath.find(c.url);
                std::string rid, cid, tid, tiid;
                if (evIt != st->snap.evaluationsByPath.end()) {
                    for (const auto& cat : evIt->second.categories) {
                        for (const auto& it : cat.items) {
                            if (it.has_rubric && !it.rubric_id.empty()) {
                                rid = it.rubric_id; cid = it.community_id; tiid = it.task_item_id; tid = it.task_id;
                                break;
                            }
                        }
                        if (!rid.empty()) break;
                    }
                }
                if (rid.empty()) {
                    MessageBoxW(hwnd, L"No se encontró una rúbrica asociada a las evaluaciones de este curso.", L"Rúbrica", MB_OK | MB_ICONINFORMATION);
                } else {
                    auto rr = st->store->FetchRubric(rid, cid, tiid, tid);
                    if (rr.ok) {
                        std::ostringstream oss;
                        oss << rr.value.title << "\nPuntaje máximo: " << FmtDouble(rr.value.max_score, 1) << "\n\n";
                        for (const auto& cr : rr.value.criteria) {
                            oss << "• " << cr.category_name << " (máx " << FmtDouble(cr.max_points, 1) << " pts)\n";
                        }
                        MessageBoxW(hwnd, Utf8ToWide(oss.str()).c_str(), L"Desglose de Rúbrica", MB_OK);
                    } else {
                        MessageBoxW(hwnd, Utf8ToWide("No fue posible consultar la rúbrica: " + rr.error_message).c_str(), L"Rúbrica", MB_OK | MB_ICONWARNING);
                    }
                }
            }
            return 0;
        }
        if (id == IDC_CMB_CAL_FILTER && code == CBN_SELCHANGE) {
            RefreshCalendarSection(st);
            if (st->hCalCanvas) InvalidateRect(st->hCalCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_CMB_CAL_VIEW && code == CBN_SELCHANGE) {
            SwitchSection(st, HubSection::Calendario);
            return 0;
        }
        if (id == IDC_BTN_CAL_EXPORT_ICS && code == BN_CLICKED) {
            std::wstring outPath;
            if (PickSaveFileW(hwnd, L"Calendario iCalendar (*.ics)\0*.ics\0", L"ics", L"calendario_tec.ics", outPath)) {
                const std::string ics = td::ExportCalendarEventsToIcs(st->snap.unifiedCalendarEvents);
                std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
                ofs.write(ics.data(), static_cast<std::streamsize>(ics.size()));
                SetWindowTextW(st->hProgressBanner, L"Calendario exportado en formato .ics estándar.");
            }
            return 0;
        }
        if (id == IDC_CMB_EXP_VIEW && code == CBN_SELCHANGE) {
            SwitchSection(st, HubSection::ExpedienteYMalla);
            return 0;
        }
        if (id == IDC_CMB_MALLA_SEDE && code == CBN_SELCHANGE) {
            int is = static_cast<int>(SendMessageW(st->hCmbMallaSede, CB_GETCURSEL, 0, 0));
            if (is >= 0 && static_cast<size_t>(is) < st->mallaSedes.size()) {
                auto cr = st->store->FetchCurriculumCarreras(st->mallaSedes[static_cast<size_t>(is)].key);
                if (cr.ok) {
                    st->mallaCarreras = std::move(cr.value);
                    SendMessageW(st->hCmbMallaCarrera, CB_RESETCONTENT, 0, 0);
                    for (const auto& c : st->mallaCarreras) {
                        SendMessageW(st->hCmbMallaCarrera, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(c.name).c_str()));
                    }
                    if (!st->mallaCarreras.empty()) SendMessageW(st->hCmbMallaCarrera, CB_SETCURSEL, 0, 0);
                }
            }
            return 0;
        }
        if (id == IDC_CMB_MALLA_CARRERA && code == CBN_SELCHANGE) {
            int is = static_cast<int>(SendMessageW(st->hCmbMallaSede, CB_GETCURSEL, 0, 0));
            int ic = static_cast<int>(SendMessageW(st->hCmbMallaCarrera, CB_GETCURSEL, 0, 0));
            if (is >= 0 && static_cast<size_t>(is) < st->mallaSedes.size() &&
                ic >= 0 && static_cast<size_t>(ic) < st->mallaCarreras.size()) {
                auto pr = st->store->FetchCurriculumPlanes(
                    st->mallaSedes[static_cast<size_t>(is)].key,
                    st->mallaCarreras[static_cast<size_t>(ic)].key);
                if (pr.ok) {
                    st->mallaPlanes = std::move(pr.value);
                    SendMessageW(st->hCmbMallaPlan, CB_RESETCONTENT, 0, 0);
                    for (const auto& p : st->mallaPlanes) {
                        SendMessageW(st->hCmbMallaPlan, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(p.name).c_str()));
                    }
                    if (!st->mallaPlanes.empty()) SendMessageW(st->hCmbMallaPlan, CB_SETCURSEL, 0, 0);
                }
            }
            return 0;
        }
        if (id == IDC_BTN_MALLA_LOAD_PLAN && code == BN_CLICKED) {
            int ip = static_cast<int>(SendMessageW(st->hCmbMallaPlan, CB_GETCURSEL, 0, 0));
            if (ip >= 0 && static_cast<size_t>(ip) < st->mallaPlanes.size()) {
                std::string err;
                if (st->store->LoadCurriculumPlan(st->mallaPlanes[static_cast<size_t>(ip)].key, &err)) {
                    st->selectedMallaCode.clear();
                    st->snap = st->store->Snapshot();
                    RefreshExpedienteSection(st);
                    SetWindowTextW(st->hProgressBanner, L"Plan de estudios cargado correctamente.");
                } else {
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide("Plan de estudios: " + err).c_str());
                }
            }
            return 0;
        }
        if (id == IDC_CMB_GRADES_FILTER && code == CBN_SELCHANGE) {
            RefreshExpedienteSection(st);
            return 0;
        }
        if (id == IDC_BTN_MALLA_ZOOM_IN && code == BN_CLICKED) {
            st->mallaZoom = std::min(1.8f, st->mallaZoom + 0.15f);
            InvalidateRect(st->hMallaCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_BTN_MALLA_ZOOM_OUT && code == BN_CLICKED) {
            st->mallaZoom = std::max(0.55f, st->mallaZoom - 0.15f);
            InvalidateRect(st->hMallaCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_BTN_MALLA_INSERT && code == BN_CLICKED) {
            if (st->snap.curriculumGraph) {
                DocStudio::TecDigital::TecAcademicStore::InsertCurriculumSummaryToCanvas(
                    *st->activeDoc, *st->snap.curriculumGraph,
                    st->snap.studentProfile ? &(*st->snap.studentProfile) : nullptr);
                st->docMutated = true;
                if (st->onDocMutated) st->onDocMutated();
                SetWindowTextW(st->hProgressBanner, L"Resumen del plan de estudios insertado en el cuaderno.");
            }
            return 0;
        }
        if (id == IDC_CMB_HOR_SOURCE && code == CBN_SELCHANGE) {
            SyncHorariosSourceVisibility(st);
            LayoutDialog(st);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_BTN_HOR_LOAD_JSON && code == BN_CLICKED) {
            HMENU hm = CreatePopupMenu();
            AppendMenuW(hm, MF_STRING, 1, L"Cargar archivo JSON local...");
            AppendMenuW(hm, MF_STRING, 2, L"Actualizar lista de escuelas (Guía Pública TEC)");
            AppendMenuW(hm, MF_STRING, 3, L"Agregar escuela o consulta al catálogo actual");
            AppendMenuW(hm, st->horariosCatalog.courses.empty() ? (MF_STRING | MF_GRAYED) : MF_STRING,
                        4, L"Limpiar catálogo cargado");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 5, L"Abrir Guía de Horarios Pública del TEC");
            RECT brc{};
            GetWindowRect(GetDlgItem(hwnd, IDC_BTN_HOR_LOAD_JSON), &brc);
            POINT pt{ brc.left, brc.bottom + st->Sc(4) };
            UINT pick = UiGlass::TrackTranslucentPopupMenu(hm, pt, hwnd, st->theme.settings, false);
            DestroyMenu(hm);

            if (pick == 1) {
                std::wstring inPath;
                if (PickOpenFileW(hwnd, L"Catálogo HorariosTEC (*.json)\0*.json\0", inPath)) {
                    std::ifstream ifs(inPath, std::ios::binary);
                    std::ostringstream oss;
                    oss << ifs.rdbuf();
                    auto parsed = ht::ParseCatalogJson(oss.str());
                    if (parsed.ok) {
                        st->horariosCatalog = std::move(parsed.catalog);
                        st->horariosResult = ht::ScheduleResult{};
                        st->horariosCatalogKey = "json:" + WideToUtf8(inPath);
                        st->horariosUsedRequiredFilter = false;
                        SetWindowTextW(st->hHorStatus, Utf8ToWide("Catálogo JSON cargado: " + std::to_string(st->horariosCatalog.courses.size()) + " cursos").c_str());
                        if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
                    } else {
                        SetWindowTextW(st->hHorStatus, Utf8ToWide("Error JSON: " + parsed.errorUtf8).c_str());
                    }
                }
            } else if (pick == 2) {
                if (st->workerBusy.exchange(true)) {
                    SetWindowTextW(st->hProgressBanner, L"Hay otra consulta en curso, espere un momento.");
                } else {
                    SetWindowTextW(st->hProgressBanner, L"Actualizando lista de escuelas desde Guía Pública TEC...");
                    if (st->workerThread.joinable()) st->workerThread.join();
                    st->workerThread = std::thread([st]() {
                        auto er = st->store->FetchPublicGuiaEscuelas();
                        {
                            std::lock_guard<std::mutex> lk(st->workResultMutex);
                            if (er.ok) {
                                st->pendingPublicEscuelas = std::move(er.value);
                                st->pendingWorkStatus = "Lista actualizada: " + std::to_string(st->pendingPublicEscuelas.size()) + " escuelas TEC.";
                            } else {
                                st->pendingPublicEscuelas.clear();
                                st->pendingWorkStatus = "No se pudo actualizar escuelas: " + er.error_message;
                            }
                        }
                        st->workerBusy.store(false);
                        PostMessageW(st->hwnd, WM_APP_TEC_ESCUELAS_DONE, er.ok ? 1 : 0, 0);
                    });
                }
            } else if (pick == 3) {
                LaunchAsyncHorariosFetch(st, true);
            } else if (pick == 4) {
                st->horariosCatalog = ht::CourseCatalog{};
                st->horariosResult = ht::ScheduleResult{};
                st->horariosCatalogKey.clear();
                st->horariosUsedRequiredFilter = false;
                st->selectedScheduleIdx = 0;
                SetWindowTextW(st->hHorStatus, L"Sin combinaciones calculadas");
                if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
            } else if (pick == 5) {
                OpenSafeUrl(hwnd, "https://tec-appsext.itcr.ac.cr/guiahorarios/");
            }
            return 0;
        }
        if (id == IDC_CMB_HOR_SEDE && code == CBN_SELCHANGE) {
            int is = static_cast<int>(SendMessageW(st->hCmbHorSede, CB_GETCURSEL, 0, 0));
            if (is >= 0 && static_cast<size_t>(is) < st->horSedes.size()) {
                auto cr = st->store->FetchGuiaHorarioCarreras(st->horSedes[static_cast<size_t>(is)].key);
                if (!cr.ok || cr.value.empty()) {
                    cr = st->store->FetchCurriculumCarreras(st->horSedes[static_cast<size_t>(is)].key);
                }
                if (cr.ok) {
                    st->horCarreras = std::move(cr.value);
                    SendMessageW(st->hCmbHorCarrera, CB_RESETCONTENT, 0, 0);
                    for (const auto& c : st->horCarreras) {
                        SendMessageW(st->hCmbHorCarrera, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(c.name).c_str()));
                    }
                    if (!st->horCarreras.empty()) SendMessageW(st->hCmbHorCarrera, CB_SETCURSEL, 0, 0);
                }
            }
            return 0;
        }
        if (id == IDC_BTN_HOR_SOLVE && code == BN_CLICKED) {
            DocStudio::TecDigital::PublicGuiaQuery pubQuery{};
            int source = 0;
            const std::string qKey = BuildCurrentHorariosQueryKey(st, pubQuery, source);
            const bool isReusableInMemory = !st->horariosCatalog.courses.empty() &&
                (st->horariosCatalogKey == qKey ||
                 st->horariosCatalogKey.rfind("json:", 0) == 0 ||
                 st->horariosCatalogKey.rfind("merged:", 0) == 0);

            if (isReusableInMemory) {
                ht::SolverPreferences prefs{};
                std::vector<ht::BusySlot> manualBusy;
                bool filterPrereq = false;
                bool includeBusy = false;
                bool hasReqFilter = false;
                BuildHorariosPrefsAndBusyFromUi(st, prefs, manualBusy, filterPrereq, includeBusy, hasReqFilter);
                st->horariosResult = ht::HorariosSolver::Solve(st->horariosCatalog, manualBusy, prefs);
                st->horariosUsedRequiredFilter = hasReqFilter;
                st->selectedScheduleIdx = 0;
                const std::string s = FormatHorariosSolveSummaryText(st->horariosResult, st->horariosCatalog, hasReqFilter);
                SetWindowTextW(st->hHorStatus, Utf8ToWide(s).c_str());
                if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
            } else {
                LaunchAsyncHorariosFetch(st, false);
            }
            return 0;
        }
        if ((id == IDC_BTN_HOR_PREV || id == IDC_BTN_HOR_NEXT) && code == BN_CLICKED &&
            st->horariosResult.HasSolutions()) {
            const size_t n = st->horariosResult.schedules.size();
            if (id == IDC_BTN_HOR_PREV) st->selectedScheduleIdx = (st->selectedScheduleIdx + n - 1) % n;
            else st->selectedScheduleIdx = (st->selectedScheduleIdx + 1) % n;
            const auto& cur = st->horariosResult.schedules[st->selectedScheduleIdx];
            std::string s = "Opción " + std::to_string(st->selectedScheduleIdx + 1) + "/" + std::to_string(n) +
                            " · " + std::to_string(cur.metrics.totalCredits) + " cr · " +
                            std::to_string(cur.metrics.activeDaysCount) + " días · " +
                            std::to_string(cur.metrics.totalGapMinutes) + " min huecos";
            SetWindowTextW(st->hHorStatus, Utf8ToWide(s).c_str());
            InvalidateRect(st->hHorCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_BTN_HOR_INSERT && code == BN_CLICKED &&
            st->horariosResult.HasSolutions() && st->selectedScheduleIdx < st->horariosResult.schedules.size()) {
            DocStudio::TecDigital::TecAcademicStore::InsertScheduleToCanvas(
                *st->activeDoc, st->horariosResult.schedules[st->selectedScheduleIdx]);
            st->docMutated = true;
            if (st->onDocMutated) st->onDocMutated();
            SetWindowTextW(st->hProgressBanner, L"Horario óptimo insertado en el cuaderno activo.");
            return 0;
        }
        if (id == IDC_BTN_HOR_EXPORT && code == BN_CLICKED) {
            const bool hasSol = st->horariosResult.HasSolutions();
            std::vector<std::string> checkCodes;
            if (hasSol && st->selectedScheduleIdx < st->horariosResult.schedules.size()) {
                for (const auto& sel : st->horariosResult.schedules[st->selectedScheduleIdx].selections) {
                    if (!sel.courseCodeUtf8.empty()) checkCodes.push_back(sel.courseCodeUtf8);
                }
            } else {
                for (const auto& c : st->horariosCatalog.courses) {
                    if (!c.codeUtf8.empty()) checkCodes.push_back(c.codeUtf8);
                }
            }
            if (!hasSol && checkCodes.empty()) {
                SetWindowTextW(st->hProgressBanner, L"Cargue o resuelva un horario antes de exportar o validar requisitos.");
                return 0;
            }
            HMENU hm = CreatePopupMenu();
            const UINT solFlags = hasSol ? MF_STRING : (MF_STRING | MF_GRAYED);
            AppendMenuW(hm, solFlags, 1, L"Exportar Tabla CSV (*.csv)...");
            AppendMenuW(hm, solFlags, 2, L"Exportar Calendario iCalendar (*.ics)...");
            AppendMenuW(hm, solFlags, 3, L"Exportar Horario en PDF (*.pdf)...");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, checkCodes.empty() ? (MF_STRING | MF_GRAYED) : MF_STRING, 4, L"Validar requisitos en TEC...");
            RECT brc{};
            GetWindowRect(GetDlgItem(hwnd, IDC_BTN_HOR_EXPORT), &brc);
            POINT pt{ brc.left, brc.bottom + st->Sc(4) };
            UINT pick = UiGlass::TrackTranslucentPopupMenu(hm, pt, hwnd, st->theme.settings, false);
            DestroyMenu(hm);
            std::wstring outPath;
            if (pick == 1 && hasSol && PickSaveFileW(hwnd, L"Tabla CSV (*.csv)\0*.csv\0", L"csv", L"horarios_tec.csv", outPath)) {
                std::string csv = ht::ExportCsv(st->horariosResult);
                std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
                ofs.write(csv.data(), static_cast<std::streamsize>(csv.size()));
                SetWindowTextW(st->hProgressBanner, L"Combinaciones de horario exportadas a CSV.");
            } else if (pick == 2 && hasSol && PickSaveFileW(hwnd, L"Calendario iCalendar (*.ics)\0*.ics\0", L"ics", L"horario_tec.ics", outPath)) {
                const auto& curSched = st->horariosResult.schedules[std::min(st->selectedScheduleIdx, st->horariosResult.schedules.size() - 1)];
                std::string ics = DocStudio::TecDigital::TecAcademicStore::ExportRankedScheduleToIcs(curSched);
                std::ofstream ofs(outPath, std::ios::binary | std::ios::trunc);
                ofs.write(ics.data(), static_cast<std::streamsize>(ics.size()));
                SetWindowTextW(st->hProgressBanner, L"Horario óptimo exportado a formato .ics.");
            } else if (pick == 3 && hasSol && PickSaveFileW(hwnd, L"Documento PDF (*.pdf)\0*.pdf\0", L"pdf", L"horario_tec.pdf", outPath)) {
                const auto& curSched = st->horariosResult.schedules[std::min(st->selectedScheduleIdx, st->horariosResult.schedules.size() - 1)];
                std::string err;
                if (DocStudio::TecDigital::TecAcademicStore::ExportRankedScheduleToPdfFile(curSched, outPath, "Horario Semestral TEC", &err)) {
                    SetWindowTextW(st->hProgressBanner, L"Horario óptimo exportado a PDF.");
                } else {
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide("Error PDF: " + err).c_str());
                }
            } else if (pick == 4 && !checkCodes.empty()) {
                std::string periodKey;
                int ip = st->hCmbHorPeriodo ? static_cast<int>(SendMessageW(st->hCmbHorPeriodo, CB_GETCURSEL, 0, 0)) : -1;
                if (ip >= 0 && static_cast<size_t>(ip) < st->horPeriodos.size()) {
                    periodKey = st->horPeriodos[static_cast<size_t>(ip)].key;
                }
                auto vr = st->store->ValidateScheduleCoursesWithGraphQL(checkCodes, periodKey);
                if (vr.ok) {
                    std::ostringstream voss;
                    voss << "Validación oficial de requisitos y choques (" << vr.value.size() << " cursos):\n\n";
                    for (const auto& chk : vr.value) {
                        voss << "• " << chk.course_code << ": "
                             << (chk.all_requirements_met ? "Requisitos cumplidos" : "Faltan requisitos")
                             << " · " << (chk.all_corequirements_met ? "Correquisitos ok" : "Faltan correquisitos")
                             << (chk.has_overlap ? " · Choque horario detectado" : " · Sin choques");
                        if (!chk.missing_requirements.empty()) {
                            voss << "\n    Requisitos pendientes: ";
                            for (size_t i = 0; i < chk.missing_requirements.size(); ++i) {
                                voss << (i ? ", " : "") << chk.missing_requirements[i];
                            }
                        }
                        if (!chk.missing_corequirements.empty()) {
                            voss << "\n    Correquisitos pendientes: ";
                            for (size_t i = 0; i < chk.missing_corequirements.size(); ++i) {
                                voss << (i ? ", " : "") << chk.missing_corequirements[i];
                            }
                        }
                        if (!chk.message.empty()) voss << "\n    Nota: " << chk.message;
                        voss << "\n";
                    }
                    SetWindowTextW(st->hProgressBanner, L"Validación de requisitos en TEC completada.");
                    MessageBoxW(hwnd, Utf8ToWide(voss.str()).c_str(), L"Validación de Requisitos TEC", MB_OK);
                } else {
                    MessageBoxW(hwnd, Utf8ToWide("No fue posible validar requisitos en TEC: " + vr.error_message).c_str(),
                                L"Validación de Requisitos TEC", MB_OK | MB_ICONWARNING);
                }
            }
            return 0;
        }
        if (id == IDC_BTN_FTS_SEARCH && code == BN_CLICKED) {
            std::string q = WideToUtf8(GetCtrlTextW(st->hEditFts));
            st->ftsHits = st->store->SearchOfflineMaterials(q);
            st->activeDocHits.clear();
            if (st->onSearchActiveDoc && !q.empty()) {
                st->activeDocHits = st->onSearchActiveDoc(q);
            }
            SendMessageW(st->hLstFts, LB_RESETCONTENT, 0, 0);
            for (const auto& h : st->ftsHits) {
                std::string row = "[TEC · " + h.document.community + "] " + h.document.name + " — " + h.snippet;
                SendMessageW(st->hLstFts, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(row).c_str()));
            }
            for (const auto& ah : st->activeDocHits) {
                std::string row = "[Cuaderno · Pág. " + std::to_string(ah.pageIndex + 1) + "] " + ah.titleUtf8 + " — " + ah.snippetUtf8;
                SendMessageW(st->hLstFts, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(row).c_str()));
            }
            if (st->ftsHits.empty() && st->activeDocHits.empty()) {
                SendMessageW(st->hLstFts, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sin coincidencias en materiales descargados ni en el cuaderno activo."));
                if (st->hFtsPreview) SetWindowTextW(st->hFtsPreview, L"Sin vista previa disponible.");
            } else {
                SendMessageW(st->hLstFts, LB_SETCURSEL, 0, 0);
                PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_LST_FTS_HITS, LBN_SELCHANGE), reinterpret_cast<LPARAM>(st->hLstFts));
            }
            return 0;
        }
        if (id == IDC_LST_FTS_HITS && code == LBN_SELCHANGE && st->hFtsPreview) {
            int idx = static_cast<int>(SendMessageW(st->hLstFts, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->ftsHits.size()) {
                const auto& h = st->ftsHits[static_cast<size_t>(idx)];
                std::string prev = "Archivo: " + h.document.name + "  ·  Curso: " + h.document.community +
                                   "\r\nRuta local: " + h.document.blob_rel_path +
                                   "\r\nFragmento: " + h.snippet;
                SetWindowTextW(st->hFtsPreview, Utf8ToWide(prev).c_str());
            } else if (idx >= 0 && static_cast<size_t>(idx) < st->ftsHits.size() + st->activeDocHits.size()) {
                const auto& ah = st->activeDocHits[static_cast<size_t>(idx) - st->ftsHits.size()];
                std::string prev = "Origen: " + ah.titleUtf8 + " (Página " + std::to_string(ah.pageIndex + 1) + ")" +
                                   "\r\nFragmento: " + ah.snippetUtf8;
                SetWindowTextW(st->hFtsPreview, Utf8ToWide(prev).c_str());
            }
            return 0;
        }
        if (id == IDC_BTN_FTS_OPEN && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstFts, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->ftsHits.size()) {
                const auto& h = st->ftsHits[static_cast<size_t>(idx)];
                std::string fullPath = h.document.blob_rel_path;
                if (!fullPath.empty() && fullPath.find(':') == std::string::npos) {
                    fullPath = st->store->OfflineFilesRootUtf8() + "\\" + fullPath;
                }
                if (!fullPath.empty()) {
                    ShellExecuteW(hwnd, L"open", Utf8ToWide(fullPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            return 0;
        }
        if (id == IDC_BTN_FTS_INSERT && code == BN_CLICKED) {
            int idx = static_cast<int>(SendMessageW(st->hLstFts, LB_GETCURSEL, 0, 0));
            if (idx >= 0 && static_cast<size_t>(idx) < st->ftsHits.size()) {
                const auto& h = st->ftsHits[static_cast<size_t>(idx)];
                std::string fullPath = h.document.blob_rel_path;
                if (!fullPath.empty() && fullPath.find(':') == std::string::npos) {
                    fullPath = st->store->OfflineFilesRootUtf8() + "\\" + fullPath;
                }
                DocStudio::TecDigital::TecAcademicStore::ImportDownloadedMaterialToCanvas(
                    *st->activeDoc, fullPath, h.document.name, h.document.community);
                st->docMutated = true;
                if (st->onDocMutated) st->onDocMutated();
                SetWindowTextW(st->hProgressBanner, L"Referencia del material insertada en el cuaderno.");
            }
            return 0;
        }
        if (id == IDC_BTN_CACHE_PRUNE && code == BN_CLICKED) {
            std::size_t rem = st->store->PruneOfflineCacheOlderThanBytes(st->store->OfflineBudgetBytes() / 2);
            RefreshCacheUsageLabel(st);
            SetWindowTextW(st->hProgressBanner, Utf8ToWide("Caché optimizada: " + std::to_string(rem) + " archivos antiguos liberados.").c_str());
            return 0;
        }
        if (id == IDC_CMB_TRAMITE_CAT && code == CBN_SELCHANGE) {
            RefreshTramitesSection(st);
            return 0;
        }
        if (id == IDC_BTN_SVC_MAPA && code == BN_CLICKED) {
            OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/servicios/Mapa_Campus_TEC_CA.pdf");
            return 0;
        }
        if (id == IDC_BTN_SVC_GUIA && code == BN_CLICKED) {
            OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/servicios/capacitacion/guia_estudiantes/");
            return 0;
        }
        if (id == IDC_BTN_SVC_SOPORTE && code == BN_CLICKED) {
            OpenSafeUrl(hwnd, "https://tecdigital.zendesk.com/hc/es/requests/new");
            return 0;
        }
        if (id == IDC_BTN_SVC_TURNITIN && code == BN_CLICKED) {
            OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/tda-control-plagio");
            return 0;
        }
        if (id == IDC_BTN_SVC_MORE && code == BN_CLICKED) {
            HMENU hm = CreatePopupMenu();
            AppendMenuW(hm, MF_STRING, 9, L"Mapa del Campus Central (PDF)");
            AppendMenuW(hm, MF_STRING, 10, L"Guía para Estudiantes TEC Digital");
            AppendMenuW(hm, MF_STRING, 11, L"Soporte Técnico TEC Digital");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 1, L"Biblioteca TEC (SIBITEC)");
            AppendMenuW(hm, MF_STRING, 2, L"Becas y Gestión Social");
            AppendMenuW(hm, MF_STRING, 3, L"Admisión y Registro (DAR)");
            AppendMenuW(hm, MF_STRING, 4, L"PAySE (Asesoría y Servicios Estudiantiles)");
            AppendMenuW(hm, MF_STRING, 5, L"PSED-NE (Apoyos Educativos)");
            AppendMenuW(hm, MF_STRING, 6, L"KapiBOT / Asistencia TEC");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 7, L"Consultar reporte de plagio (primer documento)");
            AppendMenuW(hm, MF_STRING, 8, L"Evaluación de Desempeño Docente (TEC Digital)");
            AppendMenuW(hm, MF_STRING, 12, L"Guía de Horarios Pública del TEC");
            RECT brc{};
            GetWindowRect(GetDlgItem(hwnd, IDC_BTN_SVC_MORE), &brc);
            POINT pt{ brc.left, brc.bottom + st->Sc(4) };
            UINT pick = UiGlass::TrackTranslucentPopupMenu(hm, pt, hwnd, st->theme.settings, false);
            DestroyMenu(hm);
            if (pick == 1) OpenSafeUrl(hwnd, "https://www.tec.ac.cr/biblioteca-jose-figueres-ferrer");
            else if (pick == 2) OpenSafeUrl(hwnd, "https://www.tec.ac.cr/becas");
            else if (pick == 3) OpenSafeUrl(hwnd, "https://www.tec.ac.cr/unidades/departamento-admision-registro");
            else if (pick == 4) OpenSafeUrl(hwnd, "https://www.tec.ac.cr/unidades/departamento-orientacion-psicologia");
            else if (pick == 5) OpenSafeUrl(hwnd, "https://www.tec.ac.cr/servicios-apoyo-educativo");
            else if (pick == 6) OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/");
            else if (pick == 7) {
                if (st->snap.plagiarismTree && !st->snap.plagiarismTree->documents.empty()) {
                    const auto& d = st->snap.plagiarismTree->documents.front();
                    auto rep = st->store->Service().TdaClient().GetPlagiarismReportUrl(d.doc_uid);
                    if (rep.ok && DocStudio::IsSafeExternalLinkUrl(rep.value)) {
                        OpenSafeUrl(hwnd, rep.value);
                    } else {
                        OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/tda-control-plagio");
                    }
                } else {
                    OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/tda-control-plagio");
                }
            } else if (pick == 8) {
                OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/tda-performance-evaluation/");
            } else if (pick == 9) {
                OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/servicios/Mapa_Campus_TEC_CA.pdf");
            } else if (pick == 10) {
                OpenSafeUrl(hwnd, "https://tecdigital.tec.ac.cr/servicios/capacitacion/guia_estudiantes/");
            } else if (pick == 11) {
                OpenSafeUrl(hwnd, "https://tecdigital.zendesk.com/hc/es/requests/new");
            } else if (pick == 12) {
                OpenSafeUrl(hwnd, "https://tec-appsext.itcr.ac.cr/guiahorarios/");
            }
            return 0;
        }
        break;
    }

    case WM_CLOSE:
        if (st) {
            st->JoinWorker();
            st->done = true;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (st) {
            if (st->fNorm) DeleteObject(st->fNorm);
            if (st->fBold) DeleteObject(st->fBold);
            if (st->fTitle) DeleteObject(st->fTitle);
            if (st->fSmall) DeleteObject(st->fSmall);
            if (st->brSidebar) { DeleteObject(st->brSidebar); st->brSidebar = nullptr; }
            if (st->brPaper)   { DeleteObject(st->brPaper);   st->brPaper = nullptr; }
            if (st->brCard)    { DeleteObject(st->brCard);    st->brCard = nullptr; }
            if (st->brInput)   { DeleteObject(st->brInput);   st->brInput = nullptr; }
            st->theme.Destroy();
            st->done = true;
        }
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool RunTecDigitalCenterDialog(
    HWND owner,
    DocStudio::DocumentModel& activeDoc,
    DocStudio::TecDigital::TecAcademicStore& store,
    HubSection initialSection,
    std::string& outStatusBannerUtf8,
    std::function<void()> onDocumentMutated,
    int initialSubView,
    SendCourseToGeminiFn onSendToGeminiNotebook,
    AddSm2FlashcardsFn onAddSm2Flashcards,
    SearchActiveDocFn onSearchActiveDoc,
    const UiGlass::GlassSettings* glassSettings,
    const wchar_t* autoScreenshotPathW,
    bool seedVisualAuditDemoData) {
    (void)autoScreenshotPathW;
    (void)seedVisualAuditDemoData;
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = TecHubWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CuadernUCentroAcademicoTecWnd";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassExW(&wc);
        registered = true;
    }

    TecHubState st{};
    st.owner = owner;
    st.overrideGlass = glassSettings;
    st.activeDoc = &activeDoc;
    st.store = &store;
    st.outStatusBanner = &outStatusBannerUtf8;
    st.onDocMutated = std::move(onDocumentMutated);
    st.onSendToGemini = std::move(onSendToGeminiNotebook);
    st.onAddSm2 = std::move(onAddSm2Flashcards);
    st.onSearchActiveDoc = std::move(onSearchActiveDoc);
    st.initialSubView = initialSubView;
    st.currentSection = initialSection;

    const UINT dpi = UiGlass::DpiOfWindow(owner);
    int winW = UiGlass::ScaleDpi(1100, dpi);
    int winH = UiGlass::ScaleDpi(700, dpi);
    RECT workRc{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
    HMONITOR hMon = MonitorFromWindow(owner ? owner : GetDesktopWindow(), MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (hMon && GetMonitorInfoW(hMon, &mi)) {
        workRc = mi.rcWork;
    }
    const int maxW = std::max(640, static_cast<int>(workRc.right - workRc.left) - UiGlass::ScaleDpi(40, dpi));
    const int maxH = std::max(480, static_cast<int>(workRc.bottom - workRc.top) - UiGlass::ScaleDpi(40, dpi));
    winW = std::min(winW, maxW);
    winH = std::min(winH, maxH);
    int x = workRc.left + std::max(0, (static_cast<int>(workRc.right - workRc.left) - winW) / 2);
    int y = workRc.top + std::max(0, (static_cast<int>(workRc.bottom - workRc.top) - winH) / 2);
    if (owner && IsWindow(owner)) {
        RECT orc{};
        GetWindowRect(owner, &orc);
        x = std::clamp<int>(static_cast<int>(orc.left) + (static_cast<int>(orc.right - orc.left) - winW) / 2,
                            static_cast<int>(workRc.left),
                            std::max<int>(static_cast<int>(workRc.left), static_cast<int>(workRc.right) - winW));
        y = std::clamp<int>(static_cast<int>(orc.top) + (static_cast<int>(orc.bottom - orc.top) - winH) / 2,
                            static_cast<int>(workRc.top),
                            std::max<int>(static_cast<int>(workRc.top), static_cast<int>(workRc.bottom) - winH));
    }

    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        L"CuadernUCentroAcademicoTecWnd",
        L"Centro Académico TEC — CuadernU",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        x, y, winW, winH,
        owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (!hDlg) return false;

    if (owner) EnableWindow(owner, FALSE);
    MSG msg{};
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
            PostMessageW(hDlg, WM_CLOSE, 0, 0);
            continue;
        }
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (owner) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }
    st.JoinWorker();
    return st.docMutated;
}

}  // namespace DocStudio::TecDigitalUi

#endif  // _WIN32
