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

constexpr UINT WM_APP_TEC_WORK_DONE           = WM_APP + 310;
constexpr UINT WM_APP_TEC_PROGRESS            = WM_APP + 311;
constexpr UINT WM_APP_TEC_HORARIOS_DONE       = WM_APP + 312;
constexpr UINT WM_APP_TEC_ESCUELAS_DONE       = WM_APP + 313;
constexpr UINT WM_APP_TEC_GLOBAL_HORARIOS_DONE = WM_APP + 314;
constexpr UINT WM_APP_TEC_MONITOR_CHANGES     = WM_APP + 315;

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
    IDC_BTN_COURSE_UPLOAD_EVAL = 5316,
    IDC_EDIT_COURSE_DOC_SEARCH = 5317,
    IDC_CMB_COURSE_DOC_SORT = 5318,

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
    IDC_EDIT_HOR_GLOBAL_Q = 5625,
    IDC_CMB_HOR_GLOBAL_SCHOOL = 5626,
    IDC_CMB_HOR_GLOBAL_MOD = 5627,
    IDC_CMB_HOR_GLOBAL_DAY = 5628,
    IDC_BTN_HOR_INDEX_ALL = 5629,
    IDC_HOR_SEARCH_CANVAS = 5630,

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

std::string FormatCountdownToIso(const std::string& dateIso, std::int64_t serverOffsetSec = 0) {
    if (dateIso.size() < 10) return "";
    int y = 0, m = 0, d = 0;
    if (sscanf_s(dateIso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return "";
    const std::time_t nowAdj = std::time(nullptr) + static_cast<std::time_t>(serverOffsetSec);
    std::tm tNow{};
    localtime_s(&tNow, &nowAdj);
    tNow.tm_hour = 12;
    tNow.tm_min = 0;
    tNow.tm_sec = 0;
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

// Automatically matches a course (e.g. "Física general III GR 51") to its official Cátedra space
// (e.g. "Cátedra de Física General III") so Programa, Cronograma, Módulos & Calendario stay unified.
const td::CommunityInfo* FindLinkedCatedraForSpace(
    const DocStudio::TecDigital::TecAcademicSnapshot& snap,
    const td::CommunityInfo& space) {
    if (space.type != "CLASE") return nullptr;
    auto normFold = [](const std::string& raw) -> std::string {
        std::string s;
        for (size_t i = 0; i < raw.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(raw[i]);
            if (c == 0xC3 && i + 1 < raw.size()) {
                unsigned char n = static_cast<unsigned char>(raw[i + 1]);
                if (n == 0xA1 || n == 0x81) { s.push_back('a'); ++i; continue; }
                if (n == 0xA9 || n == 0x89) { s.push_back('e'); ++i; continue; }
                if (n == 0xAD || n == 0x8D) { s.push_back('i'); ++i; continue; }
                if (n == 0xB3 || n == 0x93) { s.push_back('o'); ++i; continue; }
                if (n == 0xBA || n == 0x9A) { s.push_back('u'); ++i; continue; }
                if (n == 0xB1 || n == 0x91) { s.push_back('n'); ++i; continue; }
            }
            if (std::isalnum(c)) s.push_back(static_cast<char>(std::tolower(c)));
            else s.push_back(' ');
        }
        return s;
    };
    const std::string cname = normFold(space.name);
    // Strip trailing " gr NN" from course name
    std::string base = cname;
    if (size_t gp = base.find(" gr "); gp != std::string::npos) base = base.substr(0, gp);
    while (!base.empty() && base.back() == ' ') base.pop_back();
    if (base.size() < 4) return nullptr;

    const td::CommunityInfo* best = nullptr;
    int bestScore = 0;
    for (const auto& cand : snap.communities) {
        if (cand.type != "CATEDRA") continue;
        const std::string kname = normFold(cand.name);
        // Exact trailing Roman numeral guard so Física II never matches Física III!
        auto endsWord = [](const std::string& hay, const std::string& w) {
            size_t p = hay.rfind(w);
            if (p == std::string::npos) return false;
            bool leftOk = (p == 0 || hay[p - 1] == ' ');
            bool rightOk = (p + w.size() == hay.size() || hay[p + w.size()] == ' ');
            return leftOk && rightOk;
        };
        int romanC = endsWord(base, "iii") ? 3 : (endsWord(base, "ii") ? 2 : (endsWord(base, "i") ? 1 : 0));
        int romanK = endsWord(kname, "iii") ? 3 : (endsWord(kname, "ii") ? 2 : (endsWord(kname, "i") ? 1 : 0));
        if (romanC > 0 && romanK > 0 && romanC != romanK) continue;

        int score = 0;
        if (kname.find(base) != std::string::npos) score += 100;
        // Token overlap
        std::istringstream iss(base);
        std::string tok;
        while (iss >> tok) {
            if (tok.size() < 4 || tok == "general" || tok == "curso") continue;
            if (kname.find(tok) != std::string::npos) score += 20;
        }
        if (romanC > 0 && romanC == romanK && score >= 20) score += 40;
        if (score > bestScore) {
            bestScore = score;
            best = &cand;
        }
    }
    return (bestScore >= 40) ? best : nullptr;
}

std::string BestCourseStatusForNode(const td::StudentCurriculumProfile* prof, const std::string& code) {
    if (!prof) return "pendiente";
    std::string target = SafeLowerAscii(code);
    std::string bestFound = "pendiente";
    auto rankSt = [](const std::string& s) -> int {
        if (s == "aprobada") return 4;
        if (s == "en_curso") return 3;
        if (s == "reprobada") return 2;
        return 1;
    };
    for (const auto& g : prof->grades) {
        if (SafeLowerAscii(g.id_course) != target) continue;
        std::string bs = SafeLowerAscii(g.best_status);
        std::string cur = "pendiente";
        if (bs == "1" || bs.find("ganad") != std::string::npos ||
            bs.find("aprob") != std::string::npos || bs.find("reconoc") != std::string::npos) {
            cur = "aprobada";
        } else if (bs.find("curso") != std::string::npos) {
            cur = "en_curso";
        } else if (bs.find("perdid") != std::string::npos || bs.find("reprob") != std::string::npos) {
            cur = "reprobada";
        } else if (bs.find("pendien") != std::string::npos || bs.find("retir") != std::string::npos) {
            cur = "pendiente";
        } else {
            for (const auto& att : g.grades_record) {
                if (att.year > 0 && ((att.grade >= 67.5 && att.grade <= 100.0) || att.grade > 100.0)) {
                    cur = "aprobada";
                    break;
                }
            }
        }
        if (rankSt(cur) > rankSt(bestFound)) bestFound = cur;
    }
    return bestFound;
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
    int initialInnerTab = 0;
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

    // Global All-TEC Schedule Search state (all schools, campuses, careers & modalities)
    int horViewMode = 0; // 0=Buscador global, 1=Planificador sin choques
    int horModeHover = -1;
    std::vector<Views::SegmentedHit> horModeHits;
    DocStudio::TecDigital::GlobalHorariosIndex globalHorariosIdx;
    std::vector<std::string> globalSchoolChoices;
    std::vector<DocStudio::TecDigital::GlobalHorariosGroupEntry> horSearchResults;
    std::size_t horSearchTotalMatches = 0;
    std::vector<std::string> selectedHorCourseCodes;
    int horSearchScrollY = 0;
    int horSearchMaxScroll = 0;
    int horSearchHoverIdx = -1;
    std::vector<std::pair<RECT, std::string>> horAddBtnRects;
    std::vector<std::pair<RECT, std::string>> horSelectedChipRects;
    RECT rcHorClearSel{};
    RECT rcHorIndexCta{};
    bool pendingGlobalIndexOk = false;
    std::string pendingGlobalIndexStatus;

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
    RECT rcInEmail{}, rcInPass{}, rcInWhatIf{}, rcInCourseDocSearch{}, rcInHorFilter{}, rcInHorCarnet{}, rcInHorGlobalQ{}, rcInFts{};
    std::vector<RECT> framedPanelRects;
    std::vector<RECT> framedElevRects;
    std::vector<RECT> framedInputRects;

    // Quick references
    HWND hEmail = nullptr, hPass = nullptr, hChkDpapi = nullptr, hBtnLogin = nullptr, hBtnQr = nullptr, hBtnLogout = nullptr;
    HWND hSummaryEdit = nullptr, hSummaryCanvas = nullptr;

    HWND hCmbPeriod = nullptr, hLstCourses = nullptr, hCmbCourseSub = nullptr, hCourseDetail = nullptr;
    HWND hLstCourseFiles = nullptr;
    HWND hEditCourseDocSearch = nullptr, hCmbCourseDocSort = nullptr;
    HWND hEditWhatIf = nullptr, hBtnWhatIf = nullptr, hBtnCourseRubric = nullptr, hBtnCourseUploadEval = nullptr;
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
    HWND hEditHorGlobalQ = nullptr, hCmbHorGlobalSchool = nullptr, hCmbHorGlobalMod = nullptr;
    HWND hCmbHorGlobalDay = nullptr, hBtnHorIndexAll = nullptr, hHorSearchCanvas = nullptr;
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
    int selectedCustomPageNum = -1; // -1 = Portada, >=1 = dynamic custom tab page_num
    std::string currentCourseFolderId; // empty = root folder in Documentos explorer
    int selectedEvalFlatIdx = 0;
    int courseListScrollY = 0;
    int courseListMaxScroll = 0;
    int courseListHoverIdx = -1;
    int courseDetailScrollY = 0;
    int courseDetailMaxScroll = 0;
    bool autoScrollSelectedEvalOnce = false;
    int courseDetailHoverSeg = -1;
    int courseFileHoverIdx = -1;
    std::vector<Views::SegmentedHit> courseSegHits;
    std::vector<RECT> courseCardRects;
    std::vector<RECT> courseFileRects;
    std::vector<std::pair<RECT, std::string>> courseFolderRects;       // clickable subfolders
    std::vector<std::pair<RECT, std::string>> courseBreadcrumbRects;   // clickable breadcrumbs
    std::vector<std::pair<RECT, std::string>> courseUrlLinkRects;      // clickable folder URL links
    std::vector<std::pair<RECT, int>> courseCustomTabRects;            // clickable ?page_num=N chips
    std::vector<std::pair<RECT, std::string>> courseBannerLinkRects;   // clickable banner / link rows
    std::vector<std::pair<RECT, int>> courseEvalCardRects;             // clickable evaluation cards
    std::vector<std::pair<RECT, std::string>> courseEvalActionRects;   // clickable inspector actions (rubric, links, what-if, sync)
    std::map<std::string, double> whatIfOverridesByAssignId;           // per-item What-If grade overrides (0..100)
    std::optional<td::RubricBreakdown> activeRubric;
    std::string activeRubricAssignId;
    std::vector<std::pair<RECT, std::string>> courseNewsRects;         // clickable news articles
    std::vector<std::pair<RECT, std::string>> courseDocActionRects;    // clickable doc actions (versions, alerts, subpages, images)
    std::vector<std::pair<RECT, int>> summaryCourseJumpRects;
    std::vector<std::pair<RECT, std::string>> summaryNotifRects;       // clickable notifications in Inicio
    std::vector<std::pair<RECT, std::string>> summaryRadarRects;       // clickable change-detection radar items in Inicio

    int agendaScrollY = 0;
    int agendaMaxScroll = 0;
    int calMonthYear = 0;
    int calMonthMon = 0;
    std::string calSelectedDateIso;
    RECT rcCalPrevMonth{};
    RECT rcCalNextMonth{};
    std::vector<std::pair<RECT, std::string>> calMonthDayRects;

    int expStatsScrollY = 0;
    int expStatsMaxScroll = 0;
    int expGradeFilter = 0; // 0=Todas, 1=Aprobadas, 2=En curso, 3=Reprobadas
    int expGradeFilterHover = -1;
    std::vector<Views::SegmentedHit> expGradeFilterHits;
    int expSubTab = 0; // 0=Historial y Promedio, 1=Pendientes y Citas, 2=8 Estados e Info
    int expSubTabHover = -1;
    std::vector<Views::SegmentedHit> expSubTabHits;
    bool expShowAllAttempts = true;
    RECT rcExpToggleAttempts{};

    int ftsScrollY = 0;
    int ftsMaxScroll = 0;
    int selectedFtsIdx = 0;
    int ftsHoverIdx = -1;
    int ftsModeTab = 0; // 0=Todos los archivos (Biblioteca global), 1=Buscar en apuntes, 2=Mis documentos
    int ftsModeHover = -1;
    int globalLibFilter = 0; // 0=Todos, 1=PDF, 2=Docs/PPT, 3=Solo en directorio, 4=Nuevos/Modif., 5=Evaluaciones
    int globalLibSort = 0;   // 0=Recientes, 1=Creación, 2=Nombre, 3=Tamaño
    std::string currentPersonalFolderId;
    std::vector<Views::SegmentedHit> ftsModeHits;
    std::vector<RECT> ftsCardRects;
    std::vector<std::pair<RECT, std::string>> ftsActionRects;
    std::vector<std::pair<RECT, std::string>> personalFolderRects;
    std::vector<std::pair<RECT, std::string>> personalBreadcrumbRects;
    std::vector<td::FileRecord> currentPersonalFiles;
    std::vector<DocStudio::TecDigital::GlobalFileCatalogEntry> globalLibCatalog;
    std::vector<DocStudio::TecDigital::GlobalFileCatalogEntry> globalLibFiltered;

    bool trayIconActive = false;
    std::vector<td::TecChangeEvent> pendingMonitorChanges;
    std::map<std::string, std::string> cachedInfographicFiles;

    int tramitesScrollY = 0;
    int tramitesMaxScroll = 0;
    int tramitesSubTab = 0; // 0=Resumen y DAR, 1=Plagio, 2=Modelo Usuario, 3=Eval. Docente y Ferias, 4=Notificaciones y Cuenta
    int tramitesSubTabHover = -1;
    std::vector<Views::SegmentedHit> tramitesSubTabHits;
    std::vector<std::pair<RECT, std::string>> tramitesActionRects;

    const wchar_t* autoScreenshotPathW = nullptr;

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
    if (id == IDC_BTN_COURSE_INSERT_CANVAS || id == IDC_BTN_COURSE_UPLOAD_EVAL ||
        id == IDC_BTN_CAL_EXPORT_ICS ||
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
        const float itemW = 176.0f * dpiScale;
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
            sfCap.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
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
        if (st->courseSubTab == 1 && st->rcInCourseDocSearch.right > st->rcInCourseDocSearch.left) {
            st->framedInputRects.push_back(st->rcInCourseDocSearch);
        }
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
        if (st->horViewMode == 0) {
            if (st->rcInHorGlobalQ.right > st->rcInHorGlobalQ.left) {
                st->framedInputRects.push_back(st->rcInHorGlobalQ);
            }
        } else {
            if (st->horFiltersExpanded && st->rcInHorFilter.right > st->rcInHorFilter.left) {
                st->framedInputRects.push_back(st->rcInHorFilter);
            }
            int horSrc = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
            if (horSrc == 2 && st->rcInHorCarnet.right > st->rcInHorCarnet.left) {
                st->framedInputRects.push_back(st->rcInHorCarnet);
            }
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

void SwitchSection(TecHubState* st, HubSection sec);
void RefreshSelectedCourseDetail(TecHubState* st);
void RefreshCourseList(TecHubState* st);
void LayoutDialog(TecHubState* st);

std::string TrimDialogUtf8(std::string_view sv);

bool IsValidCalendarEventDialog(const td::CalendarEventEntry& ev) {
    if (ev.date_iso.size() < 10 || ev.date_iso[4] != '-' || ev.date_iso[7] != '-') return false;
    for (int idx : {0, 1, 2, 3, 5, 6, 8, 9}) {
        if (!std::isdigit(static_cast<unsigned char>(ev.date_iso[static_cast<size_t>(idx)]))) return false;
    }
    std::string t = TrimDialogUtf8(ev.title);
    if (t.size() < 3) return false;
    bool allDigits = true;
    for (char ch : t) {
        if (!std::isdigit(static_cast<unsigned char>(ch)) && !std::isspace(static_cast<unsigned char>(ch))) {
            allDigits = false;
            break;
        }
    }
    return !allDigits;
}

std::string TodayIsoLocalDialog() {
    SYSTEMTIME stNow{};
    GetLocalTime(&stNow);
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u", stNow.wYear, stNow.wMonth, stNow.wDay);
    return buf;
}

std::string NormalizeCalEventKeyDialog(const std::string& rawTitle) {
    std::string low = SafeLowerAscii(rawTitle);
    // Collapse whitespace
    std::string t;
    t.reserve(low.size());
    bool prevWs = false;
    for (char c : low) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!prevWs && !t.empty()) t.push_back(' ');
            prevWs = true;
        } else {
            t.push_back(c);
            prevWs = false;
        }
    }
    while (!t.empty() && t.back() == ' ') t.pop_back();
    static const char* kPfxs[] = { "fecha de entrega para ", "entrega: ", "entrega de " };
    for (const char* pfx : kPfxs) {
        const size_t n = std::strlen(pfx);
        if (t.rfind(pfx, 0) == 0) {
            t = t.substr(n);
            while (!t.empty() && t.front() == ' ') t.erase(t.begin());
            break;
        }
    }
    return t;
}

std::string CleanHtmlSnippetDialog(const std::string& raw) {
    if (raw.empty()) return {};
    auto replaceAll = [](std::string& s, const std::string& from, const std::string& to) {
        size_t p = 0;
        while ((p = s.find(from, p)) != std::string::npos) {
            s.replace(p, from.size(), to);
            p += to.size();
        }
    };
    std::string s = raw;
    replaceAll(s, "&lt;", "<");
    replaceAll(s, "&gt;", ">");
    replaceAll(s, "&amp;", "&");
    replaceAll(s, "&quot;", "\"");
    replaceAll(s, "&#39;", "'");
    replaceAll(s, "&nbsp;", " ");
    std::string stripped;
    stripped.reserve(s.size());
    bool inTag = false;
    for (char c : s) {
        if (c == '<') { inTag = true; stripped.push_back(' '); }
        else if (c == '>') { inTag = false; }
        else if (!inTag) { stripped.push_back(c); }
    }
    replaceAll(stripped, "&nbsp;", " ");
    replaceAll(stripped, "&aacute;", "\xC3\xA1");
    replaceAll(stripped, "&eacute;", "\xC3\xA9");
    replaceAll(stripped, "&iacute;", "\xC3\xAD");
    replaceAll(stripped, "&oacute;", "\xC3\xB3");
    replaceAll(stripped, "&uacute;", "\xC3\xBA");
    replaceAll(stripped, "&ntilde;", "\xC3\xB1");
    std::string out;
    out.reserve(stripped.size());
    bool prevWs = false;
    for (char c : stripped) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!prevWs && !out.empty()) out.push_back(' ');
            prevWs = true;
        } else {
            out.push_back(c);
            prevWs = false;
        }
    }
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

std::string FormatNewsDateDialog(const std::string& raw) {
    // Convert "Tue, 24 Nov 2026 03:38:01 GMT" -> "24 Nov 2026"
    size_t comma = raw.find(',');
    if (comma != std::string::npos && comma + 2 < raw.size()) {
        std::string rest = raw.substr(comma + 1);
        while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
        // Keep first 3 space-separated tokens: "24 Nov 2026"
        int spaces = 0;
        for (size_t i = 0; i < rest.size(); ++i) {
            if (rest[i] == ' ') {
                if (++spaces == 3) return rest.substr(0, i);
            }
        }
        return rest;
    }
    return raw;
}

std::string FormatLocalServerClockDialog(std::int64_t serverOffsetSec) {
    const std::time_t nowAdj = std::time(nullptr) + static_cast<std::time_t>(serverOffsetSec);
    std::tm lt{};
    if (localtime_s(&lt, &nowAdj) != 0) return {};
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%02d/%02d/%04d %02d:%02d",
                  lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900, lt.tm_hour, lt.tm_min);
    return buf;
}

std::string StripCopyCommentSuffixDialog(const std::string& raw) {
    std::string out = raw;
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    std::string low = SafeLowerAscii(out);
    const std::string kSuffix = "copiar comentario";
    if (low.size() >= kSuffix.size() &&
        low.compare(low.size() - kSuffix.size(), kSuffix.size(), kSuffix) == 0) {
        out.erase(out.size() - kSuffix.size());
        while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) out.pop_back();
    }
    return out;
}

std::string FormatPlagiarismStatusDialog(const std::string& statusCode, const std::string& statusText) {
    const std::string raw = !statusText.empty() ? statusText : statusCode;
    std::string trimmed = raw;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();
    if (trimmed.empty() || trimmed == "12" || trimmed == "0") return "Sin analizar";
    if (trimmed == "1") return "En proceso";
    if (trimmed == "2" || trimmed == "3") return "Analizado";
    if (trimmed == "4" || trimmed == "-1") return "Error";
    bool allDigits = true;
    for (char c : trimmed) {
        if (!std::isdigit(static_cast<unsigned char>(c))) { allDigits = false; break; }
    }
    if (allDigits) return "Sin analizar";
    return trimmed;
}

void StartAsyncSync(TecHubState* st);
void RefreshAllSections(TecHubState* st);
void SwitchSection(TecHubState* st, HubSection sec);
void RefreshCourseList(TecHubState* st);
void RefreshSelectedCourseDetail(TecHubState* st);
std::string FoldAccentsLowerDialog(std::string_view sv);

std::string FormatFriendlyTimestampDialog(const std::string& raw) {
    if (raw.size() < 10) return {};
    // Accept YYYY-MM-DD[ T]HH:MM[:SS]
    if (std::isdigit(static_cast<unsigned char>(raw[0])) &&
        std::isdigit(static_cast<unsigned char>(raw[1])) &&
        std::isdigit(static_cast<unsigned char>(raw[2])) &&
        std::isdigit(static_cast<unsigned char>(raw[3])) &&
        raw[4] == '-' && raw[7] == '-') {
        std::string out = raw.substr(8, 2) + "/" + raw.substr(5, 2) + "/" + raw.substr(0, 4);
        if (raw.size() >= 16 && (raw[10] == ' ' || raw[10] == 'T')) {
            out += " " + raw.substr(11, 5);
        }
        return out;
    }
    return raw.substr(0, std::min<size_t>(16, raw.size()));
}

std::string FriendlyChangeKindLabel(td::TecChangeEventKind k) {
    switch (k) {
    case td::TecChangeEventKind::NewFile: return "Archivo nuevo";
    case td::TecChangeEventKind::ModifiedFile: return "Archivo modificado";
    case td::TecChangeEventKind::NewFolder: return "Carpeta nueva";
    case td::TecChangeEventKind::GradePublished: return "Nota publicada";
    case td::TecChangeEventKind::GradeChanged: return "Nota actualizada";
    case td::TecChangeEventKind::RubricGraded: return "Rúbrica calificada";
    case td::TecChangeEventKind::NewTeacherComment: return "Comentario docente";
    case td::TecChangeEventKind::NewEvaluation: return "Nueva evaluación";
    case td::TecChangeEventKind::NewNews: return "Aviso / noticia";
    case td::TecChangeEventKind::NewWeeklyContent: return "Módulo semanal";
    }
    return "Novedad";
}

UiGlass::Rgba8 FriendlyChangeKindColor(td::TecChangeEventKind k, const UiGlass::GlassPalette& pal) {
    switch (k) {
    case td::TecChangeEventKind::GradePublished:
    case td::TecChangeEventKind::GradeChanged:
    case td::TecChangeEventKind::RubricGraded:
        return pal.accentGreen;
    case td::TecChangeEventKind::NewFile:
    case td::TecChangeEventKind::ModifiedFile:
    case td::TecChangeEventKind::NewFolder:
        return pal.accentBlue;
    case td::TecChangeEventKind::NewWeeklyContent:
        return pal.accentIndigo;
    case td::TecChangeEventKind::NewEvaluation:
    case td::TecChangeEventKind::NewTeacherComment:
    case td::TecChangeEventKind::NewNews:
        return pal.accentOrange;
    }
    return pal.accentTeal;
}

void ShowWindowsTecToast(TecHubState* st, const std::wstring& title, const std::wstring& body) {
    if (!st || !st->hwnd || st->autoScreenshotPathW != nullptr) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = st->hwnd;
    nid.uID = 4201;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_INFO | NIF_SHOWTIP;
    nid.hIcon = LoadIconW(nullptr, IDI_INFORMATION);
    wcsncpy_s(nid.szTip, L"CuadernU — Monitor TEC Digital", _TRUNCATE);
    wcsncpy_s(nid.szInfoTitle, title.c_str(), _TRUNCATE);
    wcsncpy_s(nid.szInfo, body.c_str(), _TRUNCATE);
    nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    if (!st->trayIconActive) {
        if (Shell_NotifyIconW(NIM_ADD, &nid)) {
            st->trayIconActive = true;
            nid.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &nid);
        }
    } else {
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
}

void RemoveWindowsTecTrayIcon(TecHubState* st) {
    if (!st || !st->hwnd || !st->trayIconActive) return;
    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = st->hwnd;
    nid.uID = 4201;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    st->trayIconActive = false;
}

void StartAutoMonitorBridge(TecHubState* st, int intervalMinutes) {
    if (!st || !st->store) return;
    if (st->autoScreenshotPathW != nullptr) return;
    HWND targetHwnd = st->hwnd;
    st->store->ConfigureAutoMonitor(
        intervalMinutes,
        [st, targetHwnd](const std::vector<td::TecChangeEvent>& changes) {
            if (!targetHwnd || !IsWindow(targetHwnd)) return;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                st->pendingMonitorChanges = changes;
            }
            PostMessageW(targetHwnd, WM_APP_TEC_MONITOR_CHANGES, static_cast<WPARAM>(changes.size()), 0);
        });
}

struct SimplePromptState {
    HWND hEdit = nullptr;
    std::wstring result;
    bool accepted = false;
    bool closed = false;
    HFONT hFont = nullptr;
};

LRESULT CALLBACK SimplePromptWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* ps = reinterpret_cast<SimplePromptState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        ps = reinterpret_cast<SimplePromptState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ps));
        return 0;
    }
    case WM_COMMAND: {
        const int id = LOWORD(wp);
        if (id == IDOK && ps) {
            if (ps->hEdit) ps->result = GetCtrlTextW(ps->hEdit);
            ps->accepted = true;
            ps->closed = true;
            DestroyWindow(hwnd);
            return 0;
        }
        if (id == IDCANCEL && ps) {
            ps->accepted = false;
            ps->closed = true;
            DestroyWindow(hwnd);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        if (ps) { ps->accepted = false; ps->closed = true; }
        DestroyWindow(hwnd);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool ShowSimpleTextPromptDialog(HWND parent,
                                const std::wstring& title,
                                const std::wstring& prompt,
                                const std::wstring& defaultText,
                                std::wstring& outText) {
    static bool reg = false;
    if (!reg) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = SimplePromptWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"CuadernUSimpleTextPrompt";
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassExW(&wc);
        reg = true;
    }
    const UINT dpi = UiGlass::DpiOfWindow(parent);
    const int W = UiGlass::ScaleDpi(440, dpi);
    const int H = UiGlass::ScaleDpi(170, dpi);
    RECT prc{};
    GetWindowRect(parent, &prc);
    const int x = prc.left + ((prc.right - prc.left) - W) / 2;
    const int y = prc.top + ((prc.bottom - prc.top) - H) / 2;

    SimplePromptState ps{};
    ps.hFont = CreateFontW(-UiGlass::ScaleDpi(13, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"CuadernUSimpleTextPrompt", title.c_str(),
                                WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                                x, y, W, H, parent, nullptr, GetModuleHandleW(nullptr), &ps);
    if (!hDlg) { if (ps.hFont) DeleteObject(ps.hFont); return false; }

    RECT crc{};
    GetClientRect(hDlg, &crc);
    const int cw = crc.right - crc.left;
    const int pad = UiGlass::ScaleDpi(16, dpi);
    HWND hLbl = CreateWindowExW(0, L"STATIC", prompt.c_str(), WS_CHILD | WS_VISIBLE | SS_LEFT,
                                pad, pad, cw - pad * 2, UiGlass::ScaleDpi(34, dpi), hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    ps.hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defaultText.c_str(),
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                               pad, pad + UiGlass::ScaleDpi(38, dpi), cw - pad * 2, UiGlass::ScaleDpi(26, dpi),
                               hDlg, nullptr, GetModuleHandleW(nullptr), nullptr);
    const int btnW = UiGlass::ScaleDpi(94, dpi);
    const int btnH = UiGlass::ScaleDpi(28, dpi);
    const int btnY = crc.bottom - pad - btnH;
    HWND hOk = CreateWindowExW(0, L"BUTTON", L"Aceptar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                               cw - pad - btnW * 2 - UiGlass::ScaleDpi(8, dpi), btnY, btnW, btnH,
                               hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDOK)), GetModuleHandleW(nullptr), nullptr);
    HWND hCan = CreateWindowExW(0, L"BUTTON", L"Cancelar", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                cw - pad - btnW, btnY, btnW, btnH,
                                hDlg, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDCANCEL)), GetModuleHandleW(nullptr), nullptr);
    for (HWND c : { hLbl, ps.hEdit, hOk, hCan }) {
        if (c && ps.hFont) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(ps.hFont), TRUE);
    }
    SetFocus(ps.hEdit);
    SendMessageW(ps.hEdit, EM_SETSEL, 0, -1);

    EnableWindow(parent, FALSE);
    MSG m{};
    while (!ps.closed && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (m.message == WM_KEYDOWN && m.wParam == VK_RETURN) {
            PostMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0);
            continue;
        }
        if (m.message == WM_KEYDOWN && m.wParam == VK_ESCAPE) {
            PostMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            continue;
        }
        if (!IsDialogMessageW(hDlg, &m)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }
    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);
    if (ps.hFont) DeleteObject(ps.hFont);
    if (ps.accepted) {
        outText = ps.result;
        return true;
    }
    return false;
}

void RefreshGlobalLibraryFilter(TecHubState* st) {
    if (!st) return;
    const std::string qRaw = st->hEditFts ? WideToUtf8(GetCtrlTextW(st->hEditFts)) : std::string{};
    const std::string qFold = FoldAccentsLowerDialog(qRaw);
    std::vector<std::string> tokens;
    {
        std::istringstream iss(qFold);
        std::string tok;
        while (iss >> tok) tokens.push_back(std::move(tok));
    }

    st->globalLibFiltered.clear();
    st->globalLibFiltered.reserve(st->globalLibCatalog.size());
    auto extEnds = [](const std::string& low, const char* ext) {
        const size_t n = std::strlen(ext);
        return low.size() >= n && low.compare(low.size() - n, n, ext) == 0;
    };
    for (const auto& e : st->globalLibCatalog) {
        if (st->globalLibFilter == 1) {
            if (!extEnds(SafeLowerAscii(e.fileName), ".pdf")) continue;
        } else if (st->globalLibFilter == 2) {
            const std::string lf = SafeLowerAscii(e.fileName);
            if (!(extEnds(lf, ".doc") || extEnds(lf, ".docx") || extEnds(lf, ".ppt") || extEnds(lf, ".pptx") ||
                  extEnds(lf, ".xls") || extEnds(lf, ".xlsx"))) continue;
        } else if (st->globalLibFilter == 3) {
            if (e.isLinkedInPortada || e.isPersonal || e.isEvalAttachment) continue;
        } else if (st->globalLibFilter == 4) {
            if (!e.isRecentlyChanged) continue;
        } else if (st->globalLibFilter == 5) {
            if (!e.isEvalAttachment) continue;
        }
        bool match = true;
        for (const auto& t : tokens) {
            if (e.searchFolded.find(t) == std::string::npos) { match = false; break; }
        }
        if (match) st->globalLibFiltered.push_back(e);
    }

    if (st->globalLibSort == 0) {
        std::stable_sort(st->globalLibFiltered.begin(), st->globalLibFiltered.end(), [](const auto& a, const auto& b) {
            const std::string& da = !a.lastModifiedAnsi.empty() ? a.lastModifiedAnsi : a.creationDateAnsi;
            const std::string& db = !b.lastModifiedAnsi.empty() ? b.lastModifiedAnsi : b.creationDateAnsi;
            if (da != db) return da > db;
            return a.fileName < b.fileName;
        });
    } else if (st->globalLibSort == 1) {
        std::stable_sort(st->globalLibFiltered.begin(), st->globalLibFiltered.end(), [](const auto& a, const auto& b) {
            if (a.creationDateAnsi != b.creationDateAnsi) return a.creationDateAnsi > b.creationDateAnsi;
            return a.fileName < b.fileName;
        });
    } else if (st->globalLibSort == 2) {
        std::stable_sort(st->globalLibFiltered.begin(), st->globalLibFiltered.end(), [](const auto& a, const auto& b) {
            return SafeLowerAscii(a.fileName) < SafeLowerAscii(b.fileName);
        });
    } else if (st->globalLibSort == 3) {
        std::stable_sort(st->globalLibFiltered.begin(), st->globalLibFiltered.end(), [](const auto& a, const auto& b) {
            if (a.contentSize != b.contentSize) return a.contentSize > b.contentSize;
            return a.fileName < b.fileName;
        });
    }
    st->ftsScrollY = 0;
    st->selectedFtsIdx = 0;
    if (st->hFtsCanvas) InvalidateRect(st->hFtsCanvas, nullptr, FALSE);
}

std::vector<const td::CalendarEventEntry*> GetPrioritizedCalendarEvents(const std::vector<td::CalendarEventEntry>& all) {
    const std::string today = TodayIsoLocalDialog();
    // Deduplicate "Fecha de entrega para X" vs "Entrega: X" on the same date, preferring evaluation entries
    std::vector<const td::CalendarEventEntry*> evalsFirst;
    std::vector<const td::CalendarEventEntry*> others;
    for (const auto& ev : all) {
        if (!IsValidCalendarEventDialog(ev)) continue;
        if (ev.source_kind == "evaluation") evalsFirst.push_back(&ev);
        else others.push_back(&ev);
    }
    std::set<std::string> seen;
    std::vector<const td::CalendarEventEntry*> upcoming;
    std::vector<const td::CalendarEventEntry*> past;
    auto accept = [&](const td::CalendarEventEntry* ev) {
        const std::string d10 = ev->date_iso.substr(0, 10);
        const std::string nt = NormalizeCalEventKeyDialog(ev->title);
        const bool evalLike = (ev->source_kind == "evaluation") ||
                              (SafeLowerAscii(ev->title).rfind("fecha de entrega para ", 0) == 0);
        const std::string key = evalLike ? (d10 + "|eval|" + nt) : (d10 + "|" + ev->start_time + "|" + nt);
        if (!seen.insert(key).second) return;
        if (d10 >= today) upcoming.push_back(ev);
        else past.push_back(ev);
    };
    for (const auto* ev : evalsFirst) accept(ev);
    for (const auto* ev : others) accept(ev);

    std::sort(upcoming.begin(), upcoming.end(),
              [](const td::CalendarEventEntry* a, const td::CalendarEventEntry* b) { return a->date_iso < b->date_iso; });
    std::sort(past.begin(), past.end(),
              [](const td::CalendarEventEntry* a, const td::CalendarEventEntry* b) { return a->date_iso > b->date_iso; });
    upcoming.insert(upcoming.end(), past.begin(), past.end());
    return upcoming;
}

void ComputeEffectiveExpedienteData(
    const DocStudio::TecDigital::TecAcademicSnapshot& snap,
    td::AcademicStatsSummary& outStats,
    std::vector<td::GradeRecordRow>& outGrades) {
    outStats = snap.expediente ? snap.expediente->structured.stats : td::AcademicStatsSummary{};
    outGrades.clear();

    // Check whether HTML expediente grades actually contain real course-code rows
    if (snap.expediente) {
        for (const auto& g : snap.expediente->structured.grades) {
            if (!g.course_code.empty() && g.course_code.size() >= 4 &&
                std::isalpha(static_cast<unsigned char>(g.course_code[0]))) {
                outGrades.push_back(g);
            }
        }
    }

    // Join real studentProfile + curriculumGraph whenever HTML expediente summary lacks course-level rows or stats
    if (snap.studentProfile && !snap.studentProfile->grades.empty()) {
        std::unordered_map<std::string, std::pair<std::string, int>> metaByCode;
        int graphTotalCredits = 0;
        int graphTotalCourses = 0;
        if (snap.curriculumGraph) {
            for (const auto& lv : snap.curriculumGraph->levels) {
                for (const auto& c : lv.courses) {
                    metaByCode[SafeLowerAscii(c.id_course)] = { c.name, c.credits };
                    graphTotalCredits += std::max(0, c.credits);
                    ++graphTotalCourses;
                }
            }
        }

        double sumWeightedAll = 0.0;
        int sumCredAll = 0;
        double sumWeightedPassed = 0.0;
        int sumCredPassed = 0;
        int cntAprob = 0, cntCurso = 0, cntReprob = 0;
        int credPassed = 0;
        std::vector<td::GradeRecordRow> derivedRows;

        // Deduplicate TEC studentProfile records by course code (keep entry with richest status/attempts)
        auto statusPriority = [](const std::string& bsRaw) -> int {
            std::string bs = SafeLowerAscii(bsRaw);
            if (bs.find("ganad") != std::string::npos || bs.find("aprob") != std::string::npos || bs.find("reconoc") != std::string::npos) return 5;
            if (bs.find("curso") != std::string::npos) return 4;
            if (bs.find("perdid") != std::string::npos || bs.find("reprob") != std::string::npos) return 3;
            if (bs.find("retir") != std::string::npos) return 2;
            return 1;
        };
        std::map<std::string, const td::StudentCourseRecord*> bestRecByCode;
        for (const auto& rec : snap.studentProfile->grades) {
            if (rec.id_course.empty()) continue;
            const std::string key = SafeLowerAscii(rec.id_course);
            auto it = bestRecByCode.find(key);
            if (it == bestRecByCode.end()) {
                bestRecByCode[key] = &rec;
            } else {
                int pNew = statusPriority(rec.best_status);
                int pOld = statusPriority(it->second->best_status);
                if (pNew > pOld || (pNew == pOld && rec.grades_record.size() > it->second->grades_record.size())) {
                    it->second = &rec;
                }
            }
        }

        for (const auto& [codeLow, recPtr] : bestRecByCode) {
            const auto& rec = *recPtr;
            std::string cname;
            int creds = 0;
            if (auto it = metaByCode.find(codeLow); it != metaByCode.end()) {
                cname = it->second.first;
                creds = it->second.second;
            }
            const std::string stLow = SafeLowerAscii(rec.best_status);
            const bool isPassed = (stLow == "1" || stLow.find("ganad") != std::string::npos ||
                                   stLow.find("aprob") != std::string::npos || stLow.find("reconoc") != std::string::npos);
            const bool isInProgress = !isPassed && (stLow.find("curso") != std::string::npos);
            const bool isFailed = !isPassed && !isInProgress &&
                                  (stLow.find("perdid") != std::string::npos || stLow.find("reprob") != std::string::npos);
            const bool isWithdrawn = !isPassed && !isInProgress && !isFailed && (stLow.find("retir") != std::string::npos);

            if (!isPassed && !isInProgress && !isFailed && !isWithdrawn) continue;

            int latestYear = 0, latestPer = 0;
            std::string latestMod = "S";
            double bestGrade = 0.0;
            bool hasNumericGrade = false;
            bool hasPassNoGrade = false;

            for (const auto& att : rec.grades_record) {
                if (att.year <= 0) continue;
                if (att.year > latestYear || (att.year == latestYear && att.period > latestPer)) {
                    latestYear = att.year;
                    latestPer = att.period;
                    if (!att.modality.empty()) latestMod = att.modality;
                }
                if (att.grade > 100.0) {
                    hasPassNoGrade = true;
                } else if (att.grade > 0.0 && !isInProgress) {
                    if (!hasNumericGrade || att.grade > bestGrade) bestGrade = att.grade;
                    hasNumericGrade = true;
                }
            }

            if (hasNumericGrade && creds > 0) {
                sumWeightedAll += bestGrade * static_cast<double>(creds);
                sumCredAll += creds;
                if (isPassed) {
                    sumWeightedPassed += bestGrade * static_cast<double>(creds);
                    sumCredPassed += creds;
                }
            }

            std::string prettyState = "Retirada";
            if (isPassed) { prettyState = "Aprobada"; ++cntAprob; credPassed += creds; }
            else if (isInProgress) { prettyState = "En curso"; ++cntCurso; }
            else if (isFailed) { prettyState = "Reprobada"; ++cntReprob; }

            td::GradeRecordRow row;
            row.year = latestYear > 0 ? latestYear : 2026;
            row.period = latestPer > 0 ? std::to_string(latestPer) : "2";
            row.modality = latestMod;
            row.state = prettyState;
            row.course_code = rec.id_course;
            row.course_name = cname;
            row.credits = creds;
            row.grade = hasNumericGrade ? bestGrade : 0.0;
            row.grade_text = hasNumericGrade ? FmtDouble(bestGrade, 0) : (hasPassNoGrade ? "AP" : "--");
            derivedRows.push_back(std::move(row));
        }

        std::sort(derivedRows.begin(), derivedRows.end(), [](const td::GradeRecordRow& a, const td::GradeRecordRow& b) {
            if (a.year != b.year) return a.year > b.year;
            if (a.period != b.period) return a.period > b.period;
            return a.course_code < b.course_code;
        });

        if (outGrades.empty() && !derivedRows.empty()) {
            outGrades = std::move(derivedRows);
        }
        if (outStats.weighted_avg_general <= 0.0 && sumCredAll > 0) {
            outStats.weighted_avg_general = sumWeightedAll / static_cast<double>(sumCredAll);
        }
        if (outStats.weighted_avg_passed <= 0.0 && sumCredPassed > 0) {
            outStats.weighted_avg_passed = sumWeightedPassed / static_cast<double>(sumCredPassed);
        }
        if (outStats.count_aprobadas == 0) outStats.count_aprobadas = cntAprob;
        if (outStats.count_en_curso == 0) outStats.count_en_curso = cntCurso;
        if (outStats.count_reprobadas == 0) outStats.count_reprobadas = cntReprob;
        if (outStats.credits_passed == 0) outStats.credits_passed = credPassed;
        if (outStats.credits_total == 0 && graphTotalCredits > 0) outStats.credits_total = graphTotalCredits;
        if (outStats.count_pendientes == 0 && graphTotalCourses > 0) {
            outStats.count_pendientes = std::max(0, graphTotalCourses - cntAprob);
        }
    }
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

    st->summaryCourseJumpRects.clear();

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        const bool noData = st->snap.communities.empty() && st->snap.syncedAtUtc.empty() &&
                            st->snap.unifiedCalendarEvents.empty() && st->snap.currentSchedule.empty();
        if (noData) {
            DrawCleanSurfaceBox(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                                static_cast<float>(st->Sc(12)), pal.contentCard,
                                hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);
            static const wchar_t* kTags[3] = {
                L"Cursos y documentos",
                L"Notas y malla",
                L"Horarios sin choques"
            };
            static const UiGlass::IconId kIcons[3] = {
                UiGlass::IconId::ReadingBook,
                UiGlass::IconId::LayersStack,
                UiGlass::IconId::ClockHistory
            };
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::AcademicCap,
                L"Tu espacio universitario en un solo lugar",
                L"Inicia sesión arriba con tu cuenta del TEC para ver tus cursos, documentos sin conexión, evaluaciones, calendario y malla curricular.",
                kTags, kIcons, 3);
        } else {
            const float gap = 12.0f * dpiScale;
            const float kpiH = 82.0f * dpiScale;
            const float kpiW = std::max(100.0f, (static_cast<float>(w) - gap * 3.0f) / 4.0f);

            int activeCoursesCount = 0;
            int totalClassesCount = 0;
            for (const auto& c : st->snap.communities) {
                if (c.type == "CLASE") {
                    ++totalClassesCount;
                    if (c.is_current_period) ++activeCoursesCount;
                }
            }
            if (activeCoursesCount == 0) activeCoursesCount = totalClassesCount;
            if (activeCoursesCount == 0) activeCoursesCount = static_cast<int>(st->snap.communities.size());

            td::AcademicStatsSummary effStats{};
            std::vector<td::GradeRecordRow> effGradesUnused;
            ComputeEffectiveExpedienteData(st->snap, effStats, effGradesUnused);

            double weightedAvg = 0.0;
            if (st->snap.studentGeneralInfo && st->snap.studentGeneralInfo->weighted_average > 0.0) {
                weightedAvg = st->snap.studentGeneralInfo->weighted_average;
            } else if (effStats.weighted_avg_general > 0.0) {
                weightedAvg = effStats.weighted_avg_general;
            }

            int creditsPassed = effStats.credits_passed;
            int creditsTotal = effStats.credits_total;
            int availCredits = st->snap.studentGeneralInfo ? st->snap.studentGeneralInfo->available_registration_credits : 0;
            int unreadNotifs = st->snap.notifications ? st->snap.notifications->unread_quantity : 0;
            if (unreadNotifs == 0 && st->snap.notifications) {
                for (const auto& n : st->snap.notifications->notifications) {
                    if (n.state == "U") ++unreadNotifs;
                }
            }
            int unreadRadar = 0;
            for (const auto& ev : st->snap.activityFeed) {
                if (ev.unread) ++unreadRadar;
            }
            const int totalUnreadAlerts = unreadNotifs + unreadRadar;

            std::wstring avgVal = (weightedAvg > 0.0) ? Utf8ToWide(FmtDouble(weightedAvg, 2)) : L"--";
            std::wstring avgSub = (st->snap.studentGeneralInfo && !st->snap.studentGeneralInfo->career.empty())
                                      ? Utf8ToWide(st->snap.studentGeneralInfo->career)
                                      : (st->snap.curriculumGraph && !st->snap.curriculumGraph->dsc_curriculum.empty()
                                             ? Utf8ToWide(st->snap.curriculumGraph->dsc_curriculum)
                                             : L"Promedio general");
            std::wstring credVal = (creditsPassed > 0) ? (std::to_wstring(creditsPassed) + L" cr") : L"--";
            std::wstring credSub = (creditsTotal > 0)
                                       ? (L"de " + std::to_wstring(creditsTotal) + L" cr · " + std::to_wstring(effStats.count_aprobadas) + L" aprob.")
                                       : (availCredits > 0 ? (std::to_wstring(availCredits) + L" cr disponibles") : L"Avance de plan");

            Views::DrawKpiTile(g, 0.0f, 0.0f, kpiW, kpiH, L"Cursos activos",
                               std::to_wstring(activeCoursesCount),
                               std::to_wstring(st->snap.communities.size()) + L" espacios totales",
                               UiGlass::IconId::ReadingBook, pal.accentBlue, dpiScale, pal, dark, hc);
            Views::DrawKpiTile(g, (kpiW + gap) * 1.0f, 0.0f, kpiW, kpiH, L"Promedio ponderado",
                               avgVal, avgSub, UiGlass::IconId::AcademicCap, pal.accentIndigo, dpiScale, pal, dark, hc);
            Views::DrawKpiTile(g, (kpiW + gap) * 2.0f, 0.0f, kpiW, kpiH, L"Créditos aprobados",
                               credVal, credSub, UiGlass::IconId::LayersStack, pal.accentGreen, dpiScale, pal, dark, hc);
            Views::DrawKpiTile(g, (kpiW + gap) * 3.0f, 0.0f, kpiW, kpiH, L"Radar y avisos",
                               std::to_wstring(totalUnreadAlerts > 0 ? totalUnreadAlerts : static_cast<int>(st->snap.unifiedCalendarEvents.size())),
                               unreadRadar > 0 ? (std::to_wstring(unreadRadar) + L" cambios detectados")
                                               : (unreadNotifs > 0 ? L"Notificaciones sin leer" : L"Eventos en agenda"),
                               UiGlass::IconId::CalendarGrid, totalUnreadAlerts > 0 ? pal.accentOrange : pal.accentTeal,
                               dpiScale, pal, dark, hc);

            // Two clean balanced cards below KPI strip
            const float bodyY = kpiH + gap;
            const float bodyH = std::max(140.0f, static_cast<float>(h) - bodyY - 2.0f);
            const float leftW = std::max(180.0f, (static_cast<float>(w) - gap) * 0.54f);
            const float rightX = leftW + gap;
            const float rightW = std::max(160.0f, static_cast<float>(w) - rightX - 1.0f);

            Views::DrawSurface(g, 0.0f, bodyY, leftW, bodyH, 11.0f * dpiScale, pal.contentCard,
                               hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);
            Views::DrawSurface(g, rightX, bodyY, rightW, bodyH, 11.0f * dpiScale, pal.contentCard,
                               hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);

            Gdiplus::FontFamily ff(L"Segoe UI");
            Gdiplus::Font fSecHdr(&ff, 13.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fCardTitle(&ff, 12.5f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fRow(&ff, 12.0f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::Font fSmall(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
            Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
            Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
            Gdiplus::StringFormat sfTrim;
            sfTrim.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sfTrim.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

            // LEFT CARD: Tu día + Accesos rápidos a cursos activos (clickeables!)
            const float pad = 16.0f * dpiScale;
            float ly = bodyY + pad;
            const float lInnerW = leftW - pad * 2.0f;

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
            if (!nextClass && !st->snap.currentSchedule.empty()) {
                nextClass = &st->snap.currentSchedule.front();
            }

            // Highlighted Next Class Banner
            const float nextH = 58.0f * dpiScale;
            Views::DrawSurface(g, pad, ly, lInnerW, nextH, 9.0f * dpiScale,
                               Views::SoftTint(pal.accentBlue, dark),
                               hc ? pal.accentBlue : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.2f : 0.0f);
            UiGlass::DrawVectorIconGdiPlus(g, UiGlass::IconId::ClockHistory,
                                           pad + 18.0f * dpiScale, ly + nextH * 0.5f,
                                           16.0f * dpiScale, UiGlass::ToGdip(pal.accentBlue), 1.6f * dpiScale);
            if (nextClass) {
                std::string ncTitle = nextClass->course_name.empty()
                                          ? (nextClass->course_code.empty() ? nextClass->raw_text : nextClass->course_code)
                                          : (nextClass->course_code + " · " + nextClass->course_name);
                std::string ncSub = nextClass->time_range;
                if (!nextClass->room.empty()) ncSub += "  ·  Aula " + nextClass->room;
                if (!nextClass->professor.empty()) ncSub += "  ·  " + nextClass->professor;
                g.DrawString(L"Próxima clase", -1, &fSmall,
                             Gdiplus::RectF(pad + 36.0f * dpiScale, ly + 8.0f * dpiScale, lInnerW - 48.0f * dpiScale, 15.0f * dpiScale),
                             &sfTrim, &brSec);
                g.DrawString(Utf8ToWide(ncTitle).c_str(), -1, &fCardTitle,
                             Gdiplus::RectF(pad + 36.0f * dpiScale, ly + 22.0f * dpiScale, lInnerW - 48.0f * dpiScale, 18.0f * dpiScale),
                             &sfTrim, &brPrim);
                g.DrawString(Utf8ToWide(ncSub).c_str(), -1, &fSmall,
                             Gdiplus::RectF(pad + 36.0f * dpiScale, ly + 39.0f * dpiScale, lInnerW - 48.0f * dpiScale, 15.0f * dpiScale),
                             &sfTrim, &brSec);
            } else {
                g.DrawString(L"Agenda de hoy", -1, &fCardTitle,
                             Gdiplus::RectF(pad + 36.0f * dpiScale, ly + 12.0f * dpiScale, lInnerW - 48.0f * dpiScale, 18.0f * dpiScale),
                             &sfTrim, &brPrim);
                g.DrawString(L"Sin clases registradas para hoy", -1, &fRow,
                             Gdiplus::RectF(pad + 36.0f * dpiScale, ly + 32.0f * dpiScale, lInnerW - 48.0f * dpiScale, 18.0f * dpiScale),
                             &sfTrim, &brSec);
            }
            ly += nextH + 14.0f * dpiScale;

            // Active courses clickable cards
            g.DrawString(L"Tus cursos activos", -1, &fSecHdr,
                         Gdiplus::RectF(pad, ly, lInnerW, 18.0f * dpiScale), &sfTrim, &brPrim);
            ly += 24.0f * dpiScale;

            int shownC = 0;
            const float cardH = 52.0f * dpiScale;
            std::vector<size_t> activeClassIdxs;
            for (size_t ci = 0; ci < st->snap.communities.size(); ++ci) {
                const auto& c = st->snap.communities[ci];
                if (c.type == "CLASE" && c.is_current_period) activeClassIdxs.push_back(ci);
            }
            if (activeClassIdxs.empty()) {
                for (size_t ci = 0; ci < st->snap.communities.size(); ++ci) {
                    if (st->snap.communities[ci].type == "CLASE") activeClassIdxs.push_back(ci);
                }
            }
            for (size_t ci : activeClassIdxs) {
                const auto& c = st->snap.communities[ci];
                if (ly + cardH > bodyY + bodyH - 12.0f * dpiScale || shownC >= 5) break;

                const UiGlass::Rgba8 acc = Views::DeptAccent(c.department, c.type, pal);
                Views::DrawSurface(g, pad, ly, lInnerW, cardH, 9.0f * dpiScale, pal.contentCardElevated,
                                   hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.3f : 0.85f);
                Gdiplus::SolidBrush accBr(UiGlass::ToGdip(acc));
                g.FillRectangle(&accBr, pad + 3.0f, ly + 9.0f * dpiScale, 3.5f * dpiScale, cardH - 18.0f * dpiScale);

                // Optional right pill (base-100 grade from percentage or file count)
                float rightBadgeW = 0.0f;
                auto evIt = st->snap.evaluationsByPath.find(c.url);
                auto fIt = st->snap.foldersByPath.find(c.url);
                const double courseG100 = (evIt != st->snap.evaluationsByPath.end())
                                              ? td::ComputeCourseAccumulatedGrade100(evIt->second)
                                              : -1.0;
                if (courseG100 >= 0.0) {
                    const UiGlass::Rgba8 gCol = (courseG100 >= 70.0) ? pal.accentGreen : pal.accentRed;
                    std::wstring gTxt = Utf8ToWide(FmtDouble(courseG100, 1) + " / 100");
                    rightBadgeW = 78.0f * dpiScale;
                    Views::DrawPill(g, pad + lInnerW - rightBadgeW - 10.0f * dpiScale,
                                    ly + (cardH - 21.0f * dpiScale) * 0.5f, gTxt, dpiScale,
                                    Views::SoftTint(gCol, dark), gCol, hc, 84.0f);
                } else if (fIt != st->snap.foldersByPath.end() && !fIt->second.files.empty()) {
                    std::wstring dTxt = std::to_wstring(fIt->second.files.size()) + L" docs";
                    rightBadgeW = 64.0f * dpiScale;
                    Views::DrawPill(g, pad + lInnerW - rightBadgeW - 10.0f * dpiScale,
                                    ly + (cardH - 21.0f * dpiScale) * 0.5f, dTxt, dpiScale,
                                    Views::SoftTint(acc, dark), acc, hc, 70.0f);
                }

                const float txtW = std::max(60.0f, lInnerW - 26.0f * dpiScale - rightBadgeW);
                g.DrawString(Utf8ToWide(c.name).c_str(), -1, &fCardTitle,
                             Gdiplus::RectF(pad + 14.0f * dpiScale, ly + 9.0f * dpiScale, txtW, 18.0f * dpiScale),
                             &sfTrim, &brPrim);
                std::string meta = c.period.empty() ? c.slug : (c.slug + " · " + c.period);
                g.DrawString(Utf8ToWide(meta).c_str(), -1, &fSmall,
                             Gdiplus::RectF(pad + 14.0f * dpiScale, ly + 29.0f * dpiScale, txtW, 15.0f * dpiScale),
                             &sfTrim, &brTert);

                RECT hitRc{ static_cast<LONG>(pad), static_cast<LONG>(ly),
                            static_cast<LONG>(pad + lInnerW), static_cast<LONG>(ly + cardH) };
                st->summaryCourseJumpRects.push_back({ hitRc, static_cast<int>(ci) });
                ly += cardH + 8.0f * dpiScale;
                ++shownC;
            }
            if (shownC == 0) {
                g.DrawString(L"Presiona Sincronizar para cargar tus cursos actuales.", -1, &fRow,
                             Gdiplus::RectF(pad, ly, lInnerW, 20.0f * dpiScale), &sfTrim, &brTert);
            }

            // RIGHT CARD: Próximas entregas (top half) + Avisos recientes (bottom half)
            float ry = bodyY + pad;
            const float rInnerX = rightX + pad;
            const float rInnerW = rightW - pad * 2.0f;
            const float midSplitY = bodyY + bodyH * 0.52f;

            g.DrawString(L"Próximas entregas y citas", -1, &fSecHdr,
                         Gdiplus::RectF(rInnerX, ry, rInnerW, 18.0f * dpiScale), &sfTrim, &brPrim);
            ry += 24.0f * dpiScale;

            int evShown = 0;
            const float evRowH = 38.0f * dpiScale;
            const auto sortedEvents = GetPrioritizedCalendarEvents(st->snap.unifiedCalendarEvents);
            for (const auto* evPtr : sortedEvents) {
                const auto& ev = *evPtr;
                if (ry + evRowH > midSplitY - 8.0f * dpiScale || evShown >= 4) break;
                const UiGlass::Rgba8 evCol = (ev.source_kind == "matricula")
                                                 ? pal.accentOrange
                                                 : ((ev.source_kind == "evaluation") ? pal.accentBlue : pal.accentTeal);
                Gdiplus::SolidBrush dotBr(UiGlass::ToGdip(evCol));
                g.FillEllipse(&dotBr, rInnerX + 2.0f, ry + 7.0f * dpiScale, 7.0f * dpiScale, 7.0f * dpiScale);

                std::string cd = FormatCountdownToIso(ev.date_iso, st->snap.serverTimeOffsetSec);
                float pillW = 0.0f;
                if (!cd.empty()) {
                    const UiGlass::Rgba8 pCol = (cd == "Hoy" || cd == "Mañana") ? pal.accentRed : evCol;
                    pillW = 74.0f * dpiScale;
                    Views::DrawPill(g, rInnerX + rInnerW - pillW, ry + 5.0f * dpiScale,
                                    Utf8ToWide(cd), dpiScale, Views::SoftTint(pCol, dark), pCol, hc, 78.0f);
                }
                const float tw = std::max(60.0f, rInnerW - 18.0f * dpiScale - pillW - 6.0f * dpiScale);
                g.DrawString(Utf8ToWide(ev.title).c_str(), -1, &fRow,
                             Gdiplus::RectF(rInnerX + 16.0f * dpiScale, ry + 1.0f, tw, 17.0f * dpiScale),
                             &sfTrim, &brPrim);
                std::string sub = ev.date_iso + (ev.calendar_name.empty() ? "" : (" · " + ev.calendar_name));
                g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSmall,
                             Gdiplus::RectF(rInnerX + 16.0f * dpiScale, ry + 18.0f * dpiScale, tw, 15.0f * dpiScale),
                             &sfTrim, &brTert);
                ry += evRowH;
                ++evShown;
            }
            if (evShown == 0) {
                g.DrawString(L"Sin entregas ni citas pendientes", -1, &fRow,
                             Gdiplus::RectF(rInnerX, ry, rInnerW, 18.0f * dpiScale), &sfTrim, &brTert);
            }

            // Separator + Avisos recientes (clickable notifications!)
            Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);
            g.DrawLine(&sepPen, rInnerX, midSplitY, rInnerX + rInnerW, midSplitY);
            ry = midSplitY + 12.0f * dpiScale;

            st->summaryNotifRects.clear();
            st->summaryRadarRects.clear();
            const int unreadCnt = totalUnreadAlerts;
            std::wstring avisosHdr = unreadCnt > 0
                                         ? (L"Radar de cambios y avisos (" + std::to_wstring(unreadCnt) + L" sin leer)")
                                         : L"Radar de cambios y avisos";
            g.DrawString(avisosHdr.c_str(), -1, &fSecHdr,
                         Gdiplus::RectF(rInnerX, ry, rInnerW, 18.0f * dpiScale), &sfTrim, &brPrim);
            ry += 24.0f * dpiScale;

            const bool hasFooter = !st->snap.serverDateTimeIso.empty() || !st->snap.warnings.empty() || st->snap.monitorIntervalMinutes >= 0;
            const float footerReserve = hasFooter ? (30.0f * dpiScale) : (8.0f * dpiScale);
            int nShown = 0;

            // Show detected Change-Radar events first (new/modified files, graded evals, rubrics, weekly modules)
            for (const auto& ce : st->snap.activityFeed) {
                if (ry + 36.0f * dpiScale > bodyY + bodyH - footerReserve || nShown >= 4) break;
                const UiGlass::Rgba8 cCol = FriendlyChangeKindColor(ce.kind, pal);
                Gdiplus::SolidBrush ub(UiGlass::ToGdip(cCol));
                g.FillEllipse(&ub, rInnerX + 2.0f, ry + 6.0f * dpiScale, 7.0f * dpiScale, 7.0f * dpiScale);
                const std::string kindLbl = FriendlyChangeKindLabel(ce.kind);
                const float badgeW = 96.0f * dpiScale;
                Views::DrawPill(g, rInnerX + rInnerW - badgeW, ry + 5.0f * dpiScale, Utf8ToWide(kindLbl),
                                dpiScale, Views::SoftTint(cCol, dark), cCol, hc, 100.0f);
                const float ox = rInnerX + 16.0f * dpiScale;
                const float tw = std::max(60.0f, rInnerW - 22.0f * dpiScale - badgeW);
                g.DrawString(Utf8ToWide(ce.title).c_str(), -1, ce.unread ? &fCardTitle : &fRow,
                             Gdiplus::RectF(ox, ry, tw, 17.0f * dpiScale), &sfTrim, &brPrim);
                std::string subLine = ce.space_name.empty()
                                          ? ce.detail
                                          : ((ce.detail.empty() || ce.detail == ce.space_name)
                                                 ? ce.space_name
                                                 : (ce.detail.rfind(ce.space_name, 0) == 0 ? ce.detail : (ce.space_name + " · " + ce.detail)));
                g.DrawString(Utf8ToWide(subLine).c_str(), -1, &fSmall,
                             Gdiplus::RectF(ox, ry + 17.0f * dpiScale, tw, 15.0f * dpiScale), &sfTrim, &brTert);
                RECT rrc{ static_cast<LONG>(rInnerX), static_cast<LONG>(ry),
                          static_cast<LONG>(rInnerX + rInnerW), static_cast<LONG>(ry + 34.0f * dpiScale) };
                st->summaryRadarRects.push_back({ rrc, ce.id + "|" + ce.space_url });
                ry += 36.0f * dpiScale;
                ++nShown;
            }

            auto drawNoticeRow = [&](bool unread, const std::string& title, const std::string& sub,
                                     const std::string& notifId) {
                if (ry + 36.0f * dpiScale > bodyY + bodyH - footerReserve || nShown >= 4) return false;
                if (unread) {
                    Gdiplus::SolidBrush ub(UiGlass::ToGdip(pal.accentBlue));
                    g.FillEllipse(&ub, rInnerX + 2.0f, ry + 6.0f * dpiScale, 7.0f * dpiScale, 7.0f * dpiScale);
                }
                const float ox = rInnerX + (unread ? 16.0f * dpiScale : 2.0f);
                const float tw = std::max(60.0f, rInnerW - (ox - rInnerX));
                g.DrawString(Utf8ToWide(title).c_str(), -1, unread ? &fCardTitle : &fRow,
                             Gdiplus::RectF(ox, ry, tw, 17.0f * dpiScale), &sfTrim, &brPrim);
                g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSmall,
                             Gdiplus::RectF(ox, ry + 17.0f * dpiScale, tw, 15.0f * dpiScale), &sfTrim, &brTert);
                if (!notifId.empty()) {
                    RECT nrc{ static_cast<LONG>(rInnerX), static_cast<LONG>(ry),
                              static_cast<LONG>(rInnerX + rInnerW), static_cast<LONG>(ry + 34.0f * dpiScale) };
                    st->summaryNotifRects.push_back({ nrc, notifId });
                }
                ry += 36.0f * dpiScale;
                ++nShown;
                return true;
            };

            if (st->snap.notifications) {
                for (const auto& n : st->snap.notifications->notifications) {
                    if (!drawNoticeRow(n.state == "U", n.title, n.text, n.notification_id)) break;
                }
            }
            if (nShown < 4) {
                for (const auto& [path, newsList] : st->snap.newsByPath) {
                    for (const auto& nr : newsList) {
                        if (!drawNoticeRow(false, nr.title, FormatNewsDateDialog(nr.pub_date) + " · Noticia de curso", "")) break;
                    }
                    if (nShown >= 4) break;
                }
            }
            if (hasFooter) {
                std::string foot;
                if (!st->snap.serverDateTimeIso.empty()) {
                    const std::string localClk = FormatLocalServerClockDialog(st->snap.serverTimeOffsetSec);
                    foot = "Reloj TEC: " + (!localClk.empty() ? localClk : st->snap.serverDateTimeIso.substr(0, std::min<size_t>(16, st->snap.serverDateTimeIso.size())));
                }
                if (!foot.empty()) foot += "  ·  ";
                foot += (st->snap.monitorIntervalMinutes > 0)
                            ? ("Monitor: " + std::to_string(st->snap.monitorIntervalMinutes) + " min")
                            : "Monitor: manual";
                if (!st->snap.warnings.empty()) {
                    foot += "  ·  " + std::to_string(st->snap.warnings.size()) + " aviso(s)";
                }
                Gdiplus::Pen fSep(UiGlass::ToGdip(pal.separator, 120), 0.8f);
                g.DrawLine(&fSep, rInnerX, bodyY + bodyH - 26.0f * dpiScale, rInnerX + rInnerW, bodyY + bodyH - 26.0f * dpiScale);
                g.DrawString(Utf8ToWide(foot).c_str(), -1, &fSmall,
                             Gdiplus::RectF(rInnerX, bodyY + bodyH - 21.0f * dpiScale, rInnerW, 16.0f * dpiScale),
                             &sfTrim, &brTert);
            }
            if (nShown == 0) {
                g.DrawString(L"Todo al día · sin avisos nuevos", -1, &fRow,
                             Gdiplus::RectF(rInnerX, ry, rInnerW, 18.0f * dpiScale), &sfTrim, &brTert);
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
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) {
        PaintSummaryCanvas(hwnd, st);
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        for (const auto& [rc, commIdx] : st->summaryCourseJumpRects) {
            if (PtInRect(&rc, pt) && commIdx >= 0 && static_cast<size_t>(commIdx) < st->snap.communities.size()) {
                const std::string targetUrl = st->snap.communities[static_cast<size_t>(commIdx)].url;
                if (st->hCmbPeriod) SendMessageW(st->hCmbPeriod, CB_SETCURSEL, 0, 0);
                RefreshCourseList(st);
                for (size_t i = 0; i < st->filteredCourses.size(); ++i) {
                    if (st->filteredCourses[i].url == targetUrl) {
                        st->selectedCourseIdx = static_cast<int>(i);
                        if (st->hLstCourses) SendMessageW(st->hLstCourses, LB_SETCURSEL, i, 0);
                        break;
                    }
                }
                st->selectedCustomPageNum = -1;
                st->currentCourseFolderId.clear();
                st->courseDetailScrollY = 0;
                SwitchSection(st, HubSection::Cursos);
                RefreshSelectedCourseDetail(st);
                return 0;
            }
        }
        for (const auto& [rc, payload] : st->summaryRadarRects) {
            if (PtInRect(&rc, pt)) {
                const size_t sep = payload.find('|');
                const std::string evId = (sep == std::string::npos) ? payload : payload.substr(0, sep);
                const std::string spUrl = (sep == std::string::npos) ? std::string{} : payload.substr(sep + 1);
                if (st->store && !evId.empty()) {
                    st->store->MarkChangeRead(evId);
                    st->snap = st->store->Snapshot();
                }
                if (!spUrl.empty()) {
                    if (st->hCmbPeriod) SendMessageW(st->hCmbPeriod, CB_SETCURSEL, 0, 0);
                    RefreshCourseList(st);
                    for (size_t i = 0; i < st->filteredCourses.size(); ++i) {
                        if (st->filteredCourses[i].url == spUrl) {
                            st->selectedCourseIdx = static_cast<int>(i);
                            if (st->hLstCourses) SendMessageW(st->hLstCourses, LB_SETCURSEL, i, 0);
                            break;
                        }
                    }
                    SwitchSection(st, HubSection::Cursos);
                    RefreshSelectedCourseDetail(st);
                } else {
                    st->tramitesSubTab = 4;
                    SwitchSection(st, HubSection::TramitesYPerfil);
                }
                return 0;
            }
        }
        for (const auto& [rc, nid] : st->summaryNotifRects) {
            if (PtInRect(&rc, pt) && st->snap.notifications) {
                for (const auto& n : st->snap.notifications->notifications) {
                    if (n.notification_id != nid) continue;
                    std::wstring msgTxt = Utf8ToWide(n.title + "\n\n" + n.text +
                        (n.creation_date.empty() ? "" : ("\nFecha: " + n.creation_date)));
                    if (n.state == "U") {
                        msgTxt += L"\n\n¿Deseas marcar esta notificación como leída en TEC Digital?";
                        int ans = MessageBoxW(st->hwnd, msgTxt.c_str(), L"Notificación de TEC Digital", MB_YESNO | MB_ICONINFORMATION);
                        if (ans == IDYES && st->store) {
                            auto r = st->store->MarkNotificationAsRead(nid, true);
                            if (r.ok && r.value) {
                                st->snap = st->store->Snapshot();
                                SetWindowTextW(st->hProgressBanner, L"Notificación marcada como leída.");
                                InvalidateRect(hwnd, nullptr, FALSE);
                            } else {
                                SetWindowTextW(st->hProgressBanner, Utf8ToWide("No se pudo actualizar: " + r.error_message).c_str());
                            }
                        }
                    } else {
                        MessageBoxW(st->hwnd, msgTxt.c_str(), L"Notificación de TEC Digital", MB_OK | MB_ICONINFORMATION);
                    }
                    if (!n.url.empty() && n.url.find("http") == 0) {
                        OpenSafeUrl(st->hwnd, n.url);
                    }
                    return 0;
                }
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
// Brand-New guías de interfaz Interactive Canvases (Cursos, Agenda, Expediente,
// Biblioteca y Trámites — zero plain-text EDIT dumps, 0% GPU GDI+)
// ============================================================================

void PaintCourseListCanvas(HWND hwnd, TecHubState* st) {
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

    st->courseCardRects.clear();
    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        Views::DrawSurface(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                           10.0f * dpiScale, pal.contentCard,
                           hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);

        const float pad = 10.0f * dpiScale;
        const float cardH = 58.0f * dpiScale;
        const float gap = 7.0f * dpiScale;
        const int totalH = static_cast<int>(pad * 2.0f + static_cast<float>(st->filteredCourses.size()) * (cardH + gap));
        st->courseListMaxScroll = std::max(0, totalH - h);
        st->courseListScrollY = std::clamp(st->courseListScrollY, 0, st->courseListMaxScroll);

        Gdiplus::RectF clipRc(3.0f, 3.0f, static_cast<float>(w) - 6.0f, static_cast<float>(h) - 6.0f);
        g.SetClip(clipRc);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fTitle(&ff, 12.2f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSub(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
        Gdiplus::StringFormat sf;
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        if (st->filteredCourses.empty()) {
            g.DrawString(L"Sin cursos sincronizados", -1, &fSub,
                         Gdiplus::RectF(pad + 6.0f, pad + 14.0f, static_cast<float>(w) - pad * 2.0f, 24.0f * dpiScale),
                         &sf, &brTert);
        } else {
            float y = pad - static_cast<float>(st->courseListScrollY);
            const float cw = std::max(60.0f, static_cast<float>(w) - pad * 2.0f - (st->courseListMaxScroll > 0 ? 6.0f * dpiScale : 0.0f));
            for (size_t i = 0; i < st->filteredCourses.size(); ++i) {
                const auto& c = st->filteredCourses[i];
                const bool sel = (static_cast<int>(i) == st->selectedCourseIdx);
                const bool hov = (static_cast<int>(i) == st->courseListHoverIdx && !sel);
                const UiGlass::Rgba8 acc = Views::DeptAccent(c.department, c.type, pal);

                RECT hitRc{ static_cast<LONG>(pad), static_cast<LONG>(y),
                            static_cast<LONG>(pad + cw), static_cast<LONG>(y + cardH) };
                st->courseCardRects.push_back(hitRc);

                if (y + cardH >= 0.0f && y <= static_cast<float>(h)) {
                    const UiGlass::Rgba8 bg = sel ? Views::SoftTint(pal.accentBlue, dark)
                                                  : (hov ? pal.sidebarHover : pal.contentCardElevated);
                    const UiGlass::Rgba8 bd = sel ? pal.accentBlue : (hc ? pal.borderStrong : pal.borderSubtle);
                    Views::DrawSurface(g, pad, y, cw, cardH, 9.0f * dpiScale, bg, bd, sel ? 1.5f : 0.8f);

                    Gdiplus::SolidBrush ab(UiGlass::ToGdip(acc));
                    g.FillRectangle(&ab, pad + 3.0f, y + 10.0f * dpiScale, 3.2f * dpiScale, cardH - 20.0f * dpiScale);

                    g.DrawString(Utf8ToWide(c.name).c_str(), -1, &fTitle,
                                 Gdiplus::RectF(pad + 13.0f * dpiScale, y + 9.0f * dpiScale, cw - 22.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);

                    std::string sub;
                    if (c.type == "CLASE") {
                        sub = c.period.empty() ? "Curso" : c.period;
                    } else {
                        sub = WideToUtf8(Views::SpaceTypeBadgeLabel(c.type));
                        if (!c.department.empty() && c.department != "Catedra") sub += " · " + c.department;
                    }
                    auto evIt = st->snap.evaluationsByPath.find(c.url);
                    if (evIt != st->snap.evaluationsByPath.end() && evIt->second.unrounded_total > 0.0) {
                        const double g100 = td::ComputeCourseAccumulatedGrade100(evIt->second);
                        if (g100 >= 0.0) sub += "  ·  Nota " + FmtDouble(g100, 1);
                        else sub += "  ·  " + FmtDouble(evIt->second.unrounded_total, 1) + "%";
                    }
                    g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSub,
                                 Gdiplus::RectF(pad + 13.0f * dpiScale, y + 31.0f * dpiScale, cw - 22.0f * dpiScale, 16.0f * dpiScale),
                                 &sf, &brTert);
                }
                y += cardH + gap;
            }
        }
        g.ResetClip();
        Views::DrawScrollbarThumb(g, w, h, st->courseListScrollY, st->courseListMaxScroll, dpiScale, pal);
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

void TriggerOnDemandCourseSync(TecHubState* st, const std::string& courseUrl, const std::string& courseName) {
    if (!st || !st->store || courseUrl.empty() || !st->store->IsAuthenticated()) return;
    if (st->workerBusy.exchange(true)) return;
    if (st->hProgressBanner) {
        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Cargando datos de " + courseName + "...").c_str());
    }
    if (st->workerThread.joinable()) st->workerThread.join();
    st->workerThread = std::thread([st, courseUrl, courseName]() {
        std::string err;
        bool ok = st->store->SyncCourseOnDemand(courseUrl, &err);
        {
            std::lock_guard<std::mutex> lk(st->workResultMutex);
            st->pendingWorkKind = 2;
            st->pendingWorkStatus = ok ? ("Curso actualizado: " + courseName)
                                       : ("No se pudo cargar " + courseName + ": " + err);
        }
        st->workerBusy.store(false);
        PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, ok ? 1 : 0, 0);
    });
}

LRESULT CALLBACK CourseListCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) { PaintCourseListCanvas(hwnd, st); return 0; }
    if (msg == WM_MOUSEWHEEL && st) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->courseListScrollY = std::clamp(st->courseListScrollY - (delta / 2), 0, st->courseListMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        int hov = -1;
        for (size_t i = 0; i < st->courseCardRects.size(); ++i) {
            if (PtInRect(&st->courseCardRects[i], pt)) { hov = static_cast<int>(i); break; }
        }
        if (hov != st->courseListHoverIdx) {
            st->courseListHoverIdx = hov;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        for (size_t i = 0; i < st->courseCardRects.size(); ++i) {
            if (PtInRect(&st->courseCardRects[i], pt)) {
                st->selectedCourseIdx = static_cast<int>(i);
                st->selectedCourseFileIdx = 0;
                st->selectedCustomPageNum = -1;
                st->currentCourseFolderId.clear();
                st->selectedEvalFlatIdx = 0;
                st->whatIfOverridesByAssignId.clear();
                st->activeRubric.reset();
                st->activeRubricAssignId.clear();
                st->courseDetailScrollY = 0;
                if (st->hLstCourses) SendMessageW(st->hLstCourses, LB_SETCURSEL, i, 0);
                RefreshSelectedCourseDetail(st);
                InvalidateRect(hwnd, nullptr, FALSE);
                // Auto on-demand sync for historical courses not yet cached locally
                if (i < st->filteredCourses.size()) {
                    const auto& c = st->filteredCourses[i];
                    const bool missingData = (st->snap.overviewsByPath.find(c.url) == st->snap.overviewsByPath.end()) &&
                                             (st->snap.evaluationsByPath.find(c.url) == st->snap.evaluationsByPath.end());
                    if (missingData) {
                        TriggerOnDemandCourseSync(st, c.url, c.name);
                    }
                }
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintCourseDetailCanvas(HWND hwnd, TecHubState* st) {
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

    st->courseSegHits.clear();
    st->courseFileRects.clear();
    st->courseFolderRects.clear();
    st->courseBreadcrumbRects.clear();
    st->courseUrlLinkRects.clear();
    st->courseCustomTabRects.clear();
    st->courseBannerLinkRects.clear();
    st->courseEvalCardRects.clear();
    st->courseEvalActionRects.clear();
    st->courseNewsRects.clear();
    st->courseDocActionRects.clear();

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        Views::DrawSurface(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                           10.0f * dpiScale, pal.contentCard,
                           hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);

        const float pad = 16.0f * dpiScale;
        const float innerW = std::max(100.0f, static_cast<float>(w) - pad * 2.0f);

        // Top Segmented Bar (always visible & clickable!)
        static const std::vector<std::wstring> kTabs = {
            L"Resumen", L"Documentos", L"Evaluaciones", L"Actividades", L"Novedades"
        };
        const float segH = 32.0f * dpiScale;
        Views::DrawSegmentedBar(g, pad, pad, innerW, segH, kTabs, st->courseSubTab, st->courseDetailHoverSeg,
                                dpiScale, pal, hc, st->courseSegHits);

        const float contentTop = pad + segH + 14.0f * dpiScale;
        const float viewH = std::max(60.0f, static_cast<float>(h) - contentTop - 8.0f * dpiScale);
        g.SetClip(Gdiplus::RectF(4.0f, contentTop, static_cast<float>(w) - 8.0f, viewH));

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fHero(&ff, 16.5f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSec(&ff, 13.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fBody(&ff, 12.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 11.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fBigNum(&ff, 24.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
        Gdiplus::StringFormat sfTrim;
        sfTrim.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfTrim.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::StringFormat sfWrap;
        sfWrap.SetTrimming(Gdiplus::StringTrimmingEllipsisWord);

        if (st->selectedCourseIdx < 0 || static_cast<size_t>(st->selectedCourseIdx) >= st->filteredCourses.size()) {
            DrawEmptyStateCentered(g, w, h, dpiScale, pal, hc, UiGlass::IconId::ReadingBook,
                                   L"Selecciona un curso o espacio",
                                   L"Elige un curso, cátedra o escuela a la izquierda para explorar su portada, documentos, notas y novedades.");
            st->courseDetailMaxScroll = 0;
        } else {
            const auto& course = st->filteredCourses[static_cast<size_t>(st->selectedCourseIdx)];
            auto ovIt = st->snap.overviewsByPath.find(course.url);
            const td::CourseOverviewSnapshot* ov = (ovIt != st->snap.overviewsByPath.end()) ? &ovIt->second : nullptr;
            auto evIt = st->snap.evaluationsByPath.find(course.url);
            const td::DetailedEvaluationsReport* ev = (evIt != st->snap.evaluationsByPath.end()) ? &evIt->second : nullptr;
            const UiGlass::Rgba8 acc = Views::DeptAccent(course.department, course.type, pal);

            const float cw = innerW - 8.0f * dpiScale;
            float y = contentTop - static_cast<float>(st->courseDetailScrollY);

            // Course / Space Header Strip
            g.DrawString(Utf8ToWide(course.name).c_str(), -1, &fHero,
                         Gdiplus::RectF(pad, y, cw, 22.0f * dpiScale), &sfTrim, &brPrim);
            y += 24.0f * dpiScale;
            float px = pad;
            px += Views::DrawPill(g, px, y, Views::SpaceTypeBadgeLabel(course.type), dpiScale,
                                  Views::SoftTint(acc, dark), acc, hc, 110.0f) + 6.0f * dpiScale;
            if (!course.period.empty() && course.period != "Permanente") {
                px += Views::DrawPill(g, px, y, Utf8ToWide(course.period), dpiScale,
                                      pal.sidebarBg, pal.labelSecondary, hc) + 6.0f * dpiScale;
            }
            if (!course.department.empty() && course.department != "Catedra") {
                px += Views::DrawPill(g, px, y, Utf8ToWide(course.department), dpiScale,
                                      pal.sidebarBg, pal.labelSecondary, hc) + 6.0f * dpiScale;
            }
            if (ov && !ov->teacher.name.empty()) {
                g.DrawString(Utf8ToWide(ov->teacher.name).c_str(), -1, &fSmall,
                             Gdiplus::RectF(px + 4.0f, y + 3.0f * dpiScale, std::max(40.0f, cw - (px - pad)), 16.0f * dpiScale),
                             &sfTrim, &brSec);
            }
            y += 30.0f * dpiScale;

            if (st->courseSubTab == 0) { // Resumen (Portada + Banners + Portlets + Dynamic Custom Tabs)
                if (!ov) {
                    Views::DrawSurface(g, pad, y, cw, 66.0f * dpiScale, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                    g.DrawString(L"Sin portada sincronizada aún para este espacio.", -1, &fBody,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 14.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                 &sfTrim, &brPrim);
                    g.DrawString(L"Presiona Sincronizar en la barra lateral para cargar portada, pestañas, banners y avisos.", -1, &fSmall,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 35.0f * dpiScale, cw - 28.0f * dpiScale, 16.0f * dpiScale),
                                 &sfTrim, &brTert);
                    y += 76.0f * dpiScale;
                } else {
                    // Clickable Dynamic Custom Page Selector Pills (Portada + every ?page_num=N custom tab!)
                    std::vector<const td::CourseTabInfo*> customTabs;
                    for (const auto& t : ov->tabs) {
                        const std::string tl = SafeLowerAscii(t.label);
                        const bool isStdLbl = (tl == "portada" || tl == "calendario" || tl == "documentos" ||
                                               tl == "evaluaciones" || tl == "home" || tl == "calendar" ||
                                               tl == "documents" || tl == "evaluations");
                        if (t.is_custom && !isStdLbl) customTabs.push_back(&t);
                    }
                    if (!customTabs.empty()) {
                        float cxp = pad;
                        const bool onPortada = (st->selectedCustomPageNum < 0);
                        float pw0 = Views::DrawPill(g, cxp, y, L"Portada", dpiScale,
                                                    onPortada ? pal.accentBlue : pal.sidebarBg,
                                                    onPortada ? UiGlass::Rgba8{ 255, 255, 255, 255 } : pal.labelSecondary,
                                                    hc, 110.0f);
                        st->courseCustomTabRects.push_back({ { static_cast<LONG>(cxp), static_cast<LONG>(y),
                                                               static_cast<LONG>(cxp + pw0), static_cast<LONG>(y + 22.0f * dpiScale) }, -1 });
                        cxp += pw0 + 6.0f * dpiScale;
                        for (const auto* t : customTabs) {
                            const bool activeTab = (st->selectedCustomPageNum == t->page_num);
                            if (cxp > pad + cw - 120.0f * dpiScale) { cxp = pad; y += 26.0f * dpiScale; }
                            float pw = Views::DrawPill(g, cxp, y, Utf8ToWide(t->label), dpiScale,
                                                       activeTab ? pal.accentIndigo : Views::SoftTint(pal.accentIndigo, dark),
                                                       activeTab ? UiGlass::Rgba8{ 255, 255, 255, 255 } : pal.accentIndigo,
                                                       hc, 195.0f);
                            st->courseCustomTabRects.push_back({ { static_cast<LONG>(cxp), static_cast<LONG>(y),
                                                                   static_cast<LONG>(cxp + pw), static_cast<LONG>(y + 22.0f * dpiScale) }, t->page_num });
                            cxp += pw + 6.0f * dpiScale;
                        }
                        y += 30.0f * dpiScale;
                    }

                    // Viewing a specific custom page (?page_num=N)
                    if (st->selectedCustomPageNum >= 0) {
                        const td::SpacePageSnapshot* activePg = nullptr;
                        if (auto cpit = st->snap.customPagesBySpace.find(course.url); cpit != st->snap.customPagesBySpace.end()) {
                            for (const auto& pg : cpit->second) {
                                if (pg.page_num == st->selectedCustomPageNum) { activePg = &pg; break; }
                            }
                        }
                        if (!activePg) {
                            for (const auto& pg : ov->custom_pages) {
                                if (pg.page_num == st->selectedCustomPageNum) { activePg = &pg; break; }
                            }
                        }
                        if (!activePg) {
                            Views::DrawSurface(g, pad, y, cw, 58.0f * dpiScale, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                            g.DrawString(L"Pestaña sin contenido descargado aún.", -1, &fBody,
                                         Gdiplus::RectF(pad + 14.0f * dpiScale, y + 18.0f * dpiScale, cw - 28.0f * dpiScale, 20.0f * dpiScale),
                                         &sfTrim, &brSec);
                            y += 68.0f * dpiScale;
                        } else {
                            // Subpage header breadcrumb when navigated into a nested weekly module (e.g. S1..S16)
                            bool inTopTabs = false;
                            for (const auto* t : customTabs) {
                                if (t->page_num == activePg->page_num) { inTopTabs = true; break; }
                            }
                            if (!inTopTabs) {
                                const float bwBack = Views::DrawPill(g, pad, y, L"← Volver a Portada", dpiScale,
                                                                     pal.sidebarBg, pal.labelPrimary, hc, 150.0f);
                                st->courseCustomTabRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y),
                                                                       static_cast<LONG>(pad + bwBack), static_cast<LONG>(y + 22.0f * dpiScale) }, -1 });
                                std::wstring modTitle = L"Módulo semanal: " + Utf8ToWide(activePg->tab_label.empty()
                                                            ? ("Sección " + std::to_string(activePg->page_num))
                                                            : activePg->tab_label);
                                g.DrawString(modTitle.c_str(), -1, &fSec,
                                             Gdiplus::RectF(pad + bwBack + 10.0f * dpiScale, y + 2.0f * dpiScale, cw - bwBack - 14.0f * dpiScale, 18.0f * dpiScale),
                                             &sfTrim, &brPrim);
                                y += 30.0f * dpiScale;
                            }
                            if (!activePg->banners.empty()) {
                                g.DrawString(L"Banners y recursos visuales", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                                y += 22.0f * dpiScale;
                                for (const auto& b : activePg->banners) {
                                    const float bh = 36.0f * dpiScale;
                                    Views::DrawSurface(g, pad, y, cw, bh, 8.0f * dpiScale, Views::SoftTint(pal.accentIndigo, dark), pal.borderSubtle, 0.85f);
                                    Views::DrawPill(g, pad + 10.0f * dpiScale, y + 7.5f * dpiScale, L"Banner", dpiScale, pal.contentCardElevated, pal.accentIndigo, hc, 70.0f);
                                    std::string bLbl = !b.alt_text.empty() ? b.alt_text : b.image_url;
                                    g.DrawString(Utf8ToWide(bLbl).c_str(), -1, &fBody,
                                                 Gdiplus::RectF(pad + 86.0f * dpiScale, y + 9.0f * dpiScale, cw - 98.0f * dpiScale, 18.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + bh) },
                                                                          b.link_url.empty() ? b.image_url : b.link_url });
                                    y += bh + 6.0f * dpiScale;
                                }
                                y += 6.0f * dpiScale;
                            }
                            if (!activePg->portlets.empty()) {
                                for (const auto& ptb : activePg->portlets) {
                                    float bodyH = 0.0f;
                                    if (!ptb.body_preview.empty()) {
                                        Gdiplus::RectF bb{};
                                        g.MeasureString(Utf8ToWide(ptb.body_preview).c_str(), -1, &fSmall,
                                                        Gdiplus::RectF(0, 0, std::max(80.0f, cw - 28.0f * dpiScale), 280.0f * dpiScale),
                                                        &sfWrap, &bb);
                                        bodyH = std::clamp(bb.Height + 6.0f * dpiScale, 18.0f * dpiScale, 220.0f * dpiScale);
                                    }
                                    // Pre-calculate subpage pill rows (e.g. S1..S16)
                                    float subpagesH = 0.0f;
                                    if (!ptb.linked_subpages.empty()) {
                                        float sx = 0.0f;
                                        int sRows = 1;
                                        for (const auto& sp : ptb.linked_subpages) {
                                            const float estW = std::clamp(static_cast<float>(sp.label.size()) * 7.5f + 28.0f, 52.0f, 175.0f) * dpiScale;
                                            if (sx > 0.0f && sx + estW > cw - 28.0f * dpiScale) { sx = 0.0f; ++sRows; }
                                            sx += estW + 6.0f * dpiScale;
                                        }
                                        subpagesH = static_cast<float>(sRows) * 26.0f * dpiScale + 8.0f * dpiScale;
                                    }
                                    // Pre-calculate infographic image heights (using already-cached disk PNG/JPG, 0 network in WM_PAINT!)
                                    struct PreparedImg {
                                        std::string imageUrl;
                                        std::string linkUrl;
                                        std::string altText;
                                        std::string localPath;
                                        float drawW = 0.0f;
                                        float drawH = 0.0f;
                                    };
                                    std::vector<PreparedImg> prepImgs;
                                    float imgsTotalH = 0.0f;
                                    const size_t nImgs = std::min<size_t>(ptb.images.size(), 3);
                                    for (size_t ii = 0; ii < nImgs; ++ii) {
                                        const auto& im = ptb.images[ii];
                                        PreparedImg pi{};
                                        pi.imageUrl = im.image_url;
                                        pi.linkUrl = im.link_url;
                                        pi.altText = im.alt_text;
                                        if (st->store && !im.image_url.empty()) {
                                            pi.localPath = st->store->CachedMediaImagePathIfPresent(im.image_url);
                                        }
                                        if (!pi.localPath.empty()) {
                                            Gdiplus::Image imgObj(Utf8ToWide(pi.localPath).c_str());
                                            if (imgObj.GetLastStatus() == Gdiplus::Ok && imgObj.GetWidth() > 4 && imgObj.GetHeight() > 4) {
                                                const float maxW = std::max(120.0f, cw - 28.0f * dpiScale);
                                                const float maxH = 240.0f * dpiScale;
                                                const float scale = std::min(1.0f, std::min(maxW / static_cast<float>(imgObj.GetWidth()),
                                                                                             maxH / static_cast<float>(imgObj.GetHeight())));
                                                pi.drawW = std::max(40.0f, static_cast<float>(imgObj.GetWidth()) * scale);
                                                pi.drawH = std::max(32.0f, static_cast<float>(imgObj.GetHeight()) * scale);
                                                imgsTotalH += pi.drawH + 10.0f * dpiScale;
                                            } else {
                                                pi.localPath.clear();
                                                imgsTotalH += 26.0f * dpiScale;
                                            }
                                        } else {
                                            imgsTotalH += 26.0f * dpiScale;
                                        }
                                        prepImgs.push_back(std::move(pi));
                                    }

                                    const size_t nLinks = std::min<size_t>(ptb.links.size(), 8);
                                    const float linksH = nLinks > 0 ? (static_cast<float>(nLinks) * 24.0f * dpiScale + 6.0f * dpiScale) : 0.0f;
                                    const float ph = std::max(44.0f * dpiScale, 34.0f * dpiScale + bodyH + subpagesH + imgsTotalH + linksH);
                                    Views::DrawSurface(g, pad, y, cw, ph, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);

                                    float titleMaxW = cw - 28.0f * dpiScale;
                                    if (!ptb.module_kind.empty() && ptb.module_kind != "general" && ptb.module_kind != "info") {
                                        std::wstring mkLbl = (ptb.module_kind == "weekly") ? L"Semanal"
                                                             : ((ptb.module_kind == "news" || ptb.module_kind == "notice") ? L"Aviso"
                                                             : ((ptb.module_kind == "exam_notice" || ptb.module_kind == "exams") ? L"Exámenes" : L"Documentos"));
                                        UiGlass::Rgba8 mkCol = (ptb.module_kind == "weekly") ? pal.accentIndigo
                                                               : ((ptb.module_kind == "news" || ptb.module_kind == "notice") ? pal.accentOrange : pal.accentTeal);
                                        Views::DrawPill(g, pad + cw - 92.0f * dpiScale, y + 7.0f * dpiScale, mkLbl, dpiScale,
                                                        Views::SoftTint(mkCol, dark), mkCol, hc, 82.0f);
                                        titleMaxW -= 92.0f * dpiScale;
                                    }
                                    g.DrawString(Utf8ToWide(ptb.title.empty() ? activePg->tab_label : ptb.title).c_str(), -1, &fSec,
                                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 9.0f * dpiScale, titleMaxW, 18.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    float py = y + 30.0f * dpiScale;
                                    if (!ptb.body_preview.empty()) {
                                        g.DrawString(Utf8ToWide(ptb.body_preview).c_str(), -1, &fSmall,
                                                     Gdiplus::RectF(pad + 14.0f * dpiScale, py, cw - 28.0f * dpiScale, bodyH),
                                                     &sfWrap, &brSec);
                                        py += bodyH + 2.0f * dpiScale;
                                    }
                                    if (!ptb.linked_subpages.empty()) {
                                        float sx = pad + 14.0f * dpiScale;
                                        for (const auto& sp : ptb.linked_subpages) {
                                            std::wstring spLbl = Utf8ToWide(sp.label.empty() ? ("Sección " + std::to_string(sp.page_num)) : sp.label);
                                            const float estW = std::clamp(static_cast<float>(spLbl.size()) * 7.5f + 28.0f, 52.0f, 175.0f) * dpiScale;
                                            if (sx > pad + 14.0f * dpiScale && sx + estW > pad + cw - 14.0f * dpiScale) {
                                                sx = pad + 14.0f * dpiScale;
                                                py += 26.0f * dpiScale;
                                            }
                                            const float pw = Views::DrawPill(g, sx, py, spLbl, dpiScale,
                                                                             Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 175.0f);
                                            st->courseCustomTabRects.push_back({ { static_cast<LONG>(sx), static_cast<LONG>(py),
                                                                                   static_cast<LONG>(sx + pw), static_cast<LONG>(py + 22.0f * dpiScale) },
                                                                                 sp.page_num });
                                            sx += pw + 6.0f * dpiScale;
                                        }
                                        py += 28.0f * dpiScale;
                                    }
                                    for (const auto& pi : prepImgs) {
                                        if (!pi.localPath.empty() && pi.drawW > 0.0f && pi.drawH > 0.0f) {
                                            Gdiplus::Image imgObj(Utf8ToWide(pi.localPath).c_str());
                                            if (imgObj.GetLastStatus() == Gdiplus::Ok) {
                                                const auto prevMode = g.GetInterpolationMode();
                                                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                                                g.DrawImage(&imgObj, pad + 14.0f * dpiScale, py, pi.drawW, pi.drawH);
                                                g.SetInterpolationMode(prevMode);
                                                Gdiplus::Pen imgPen(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                                                g.DrawRectangle(&imgPen, pad + 14.0f * dpiScale, py, pi.drawW, pi.drawH);
                                                st->courseDocActionRects.push_back({
                                                    { static_cast<LONG>(pad + 14.0f * dpiScale), static_cast<LONG>(py),
                                                      static_cast<LONG>(pad + 14.0f * dpiScale + pi.drawW), static_cast<LONG>(py + pi.drawH) },
                                                    "img_open:" + pi.localPath });
                                            }
                                            py += pi.drawH + 10.0f * dpiScale;
                                        } else {
                                            std::wstring ilbl = L"Ver infografía: " + Utf8ToWide(pi.altText.empty() ? "Recurso gráfico semanal" : pi.altText);
                                            const float iw = Views::DrawPill(g, pad + 14.0f * dpiScale, py, ilbl, dpiScale,
                                                                             Views::SoftTint(pal.accentTeal, dark), pal.accentTeal, hc,
                                                                             std::max(150.0f, cw - 28.0f * dpiScale));
                                            st->courseDocActionRects.push_back({
                                                { static_cast<LONG>(pad + 14.0f * dpiScale), static_cast<LONG>(py),
                                                  static_cast<LONG>(pad + 14.0f * dpiScale + iw), static_cast<LONG>(py + 22.0f * dpiScale) },
                                                "img_fetch:" + pi.imageUrl });
                                            py += 26.0f * dpiScale;
                                        }
                                    }
                                    for (size_t li = 0; li < nLinks; ++li) {
                                        const auto& lk = ptb.links[li];
                                        const float bw = Views::DrawPill(g, pad + 14.0f * dpiScale, py, Utf8ToWide(lk.label),
                                                                         dpiScale, Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc,
                                                                         std::max(140.0f, cw - 28.0f * dpiScale));
                                        st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad + 14.0f * dpiScale), static_cast<LONG>(py),
                                                                                static_cast<LONG>(pad + 14.0f * dpiScale + bw), static_cast<LONG>(py + 21.0f * dpiScale) },
                                                                              lk.url });
                                        py += 24.0f * dpiScale;
                                    }
                                    y += ph + 8.0f * dpiScale;
                                }
                            } else if (!activePg->plain_excerpt.empty()) {
                                Gdiplus::RectF eb{};
                                g.MeasureString(Utf8ToWide(activePg->plain_excerpt).c_str(), -1, &fSmall,
                                                Gdiplus::RectF(0, 0, std::max(80.0f, cw - 28.0f * dpiScale), 260.0f * dpiScale),
                                                &sfWrap, &eb);
                                const float ph = std::clamp(eb.Height + 22.0f * dpiScale, 60.0f * dpiScale, 240.0f * dpiScale);
                                Views::DrawSurface(g, pad, y, cw, ph, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                                g.DrawString(Utf8ToWide(activePg->plain_excerpt).c_str(), -1, &fSmall,
                                             Gdiplus::RectF(pad + 14.0f * dpiScale, y + 10.0f * dpiScale, cw - 28.0f * dpiScale, ph - 18.0f * dpiScale),
                                             &sfWrap, &brSec);
                                y += ph + 8.0f * dpiScale;
                            }
                            if (!activePg->links.empty()) {
                                g.DrawString(L"Enlaces y materiales de esta sección", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                                y += 22.0f * dpiScale;
                                for (const auto& l : activePg->links) {
                                    const float rh = 36.0f * dpiScale;
                                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                                    Views::DrawPill(g, pad + 10.0f * dpiScale, y + 7.5f * dpiScale, Utf8ToWide(l.category.empty() ? "Enlace" : l.category),
                                                    dpiScale, Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 90.0f);
                                    g.DrawString(Utf8ToWide(l.label).c_str(), -1, &fBody,
                                                 Gdiplus::RectF(pad + 106.0f * dpiScale, y + 9.0f * dpiScale, cw - 118.0f * dpiScale, 18.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) }, l.url });
                                    y += rh + 6.0f * dpiScale;
                                }
                            }
                        }
                    } else {
                        // Portada view: Banners + Teacher + Portlets + Official Docs + Weekly sections
                        if (!ov->banners.empty()) {
                            for (size_t bi = 0; bi < ov->banners.size() && bi < 3; ++bi) {
                                const auto& b = ov->banners[bi];
                                const float bh = 38.0f * dpiScale;
                                Views::DrawSurface(g, pad, y, cw, bh, 9.0f * dpiScale, Views::SoftTint(acc, dark), pal.borderSubtle, 0.85f);
                                Views::DrawPill(g, pad + 10.0f * dpiScale, y + 8.5f * dpiScale, L"Banner", dpiScale, pal.contentCardElevated, acc, hc, 70.0f);
                                std::string bLbl = !b.alt_text.empty() ? b.alt_text : b.image_url;
                                g.DrawString(Utf8ToWide(bLbl).c_str(), -1, &fBody,
                                             Gdiplus::RectF(pad + 86.0f * dpiScale, y + 10.0f * dpiScale, cw - 98.0f * dpiScale, 18.0f * dpiScale),
                                             &sfTrim, &brPrim);
                                st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + bh) },
                                                                      b.link_url.empty() ? b.image_url : b.link_url });
                                y += bh + 6.0f * dpiScale;
                            }
                            y += 6.0f * dpiScale;
                        }

                        // Teacher & consultation card
                        if (!ov->teacher.name.empty() || !ov->teacher.office.empty() || !ov->teacher.consultation_hours.empty()) {
                            const float tcH = 68.0f * dpiScale;
                            Views::DrawSurface(g, pad, y, cw, tcH, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                            g.DrawString(L"Docente y horario de consulta", -1, &fSec,
                                         Gdiplus::RectF(pad + 14.0f * dpiScale, y + 10.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                         &sfTrim, &brPrim);
                            std::string line1 = ov->teacher.name;
                            if (!ov->teacher.office.empty()) line1 += (line1.empty() ? "" : "  ·  ") + ov->teacher.office;
                            g.DrawString(Utf8ToWide(line1.empty() ? "Información docente disponible" : line1).c_str(), -1, &fBody,
                                         Gdiplus::RectF(pad + 14.0f * dpiScale, y + 29.0f * dpiScale, cw - 28.0f * dpiScale, 17.0f * dpiScale),
                                         &sfTrim, &brSec);
                            std::string line2 = ov->teacher.teams_handle;
                            if (!ov->teacher.consultation_hours.empty()) {
                                line2 = ov->teacher.consultation_hours.front();
                                if (ov->teacher.consultation_hours.size() > 1) line2 += " · " + ov->teacher.consultation_hours[1];
                            }
                            if (!line2.empty()) {
                                g.DrawString(Utf8ToWide(line2).c_str(), -1, &fSmall,
                                             Gdiplus::RectF(pad + 14.0f * dpiScale, y + 47.0f * dpiScale, cw - 28.0f * dpiScale, 15.0f * dpiScale),
                                             &sfTrim, &brTert);
                            }
                            y += tcH + 10.0f * dpiScale;
                        }

                        // Dedicated Editorial Cátedra Header: Avisos recientes de Cátedra + Módulos Semanales rápidos
                        if (course.type == "CATEDRA") {
                            std::vector<td::RssNewsItem> catNews;
                            std::set<std::string> seenCn;
                            if (auto nit = st->snap.newsByPath.find(course.url); nit != st->snap.newsByPath.end()) {
                                for (const auto& nr : nit->second) {
                                    if (seenCn.insert(SafeLowerAscii(nr.title)).second) catNews.push_back(nr);
                                }
                            }
                            if (auto ait = st->snap.newsArchiveByPath.find(course.url); ait != st->snap.newsArchiveByPath.end()) {
                                for (const auto& nr : ait->second) {
                                    if (seenCn.insert(SafeLowerAscii(nr.title)).second) catNews.push_back(nr);
                                }
                            }
                            if (!catNews.empty()) {
                                g.DrawString(L"Avisos y comunicados recientes de cátedra", -1, &fSec,
                                             Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 110.0f * dpiScale, 18.0f * dpiScale), &sfTrim, &brPrim);
                                const float allW = Views::DrawPill(g, pad + cw - 92.0f * dpiScale, y, L"Ver todos", dpiScale,
                                                                   Views::SoftTint(pal.accentOrange, dark), pal.accentOrange, hc, 86.0f);
                                st->courseDocActionRects.push_back({
                                    { static_cast<LONG>(pad + cw - 92.0f * dpiScale), static_cast<LONG>(y),
                                      static_cast<LONG>(pad + cw - 92.0f * dpiScale + allW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                    "goto_news:" });
                                y += 26.0f * dpiScale;
                                for (size_t ni = 0; ni < catNews.size() && ni < 3; ++ni) {
                                    const auto& nr = catNews[ni];
                                    const float rh = 44.0f * dpiScale;
                                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, Views::SoftTint(pal.accentOrange, dark), pal.borderSubtle, 0.85f);
                                    g.DrawString(Utf8ToWide(nr.title).c_str(), -1, &fSec,
                                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 6.0f * dpiScale, cw - 24.0f * dpiScale, 17.0f * dpiScale), &sfTrim, &brPrim);
                                    const std::string sn = FormatNewsDateDialog(nr.pub_date) +
                                                           (nr.description.empty() ? "" : ("  ·  " + CleanHtmlSnippetDialog(nr.description)));
                                    g.DrawString(Utf8ToWide(sn).c_str(), -1, &fSmall,
                                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 24.0f * dpiScale, cw - 24.0f * dpiScale, 15.0f * dpiScale), &sfTrim, &brSec);
                                    st->courseDocActionRects.push_back({
                                        { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) },
                                        "news_read:" + nr.link });
                                    y += rh + 6.0f * dpiScale;
                                }
                                y += 6.0f * dpiScale;
                            }
                            // Quick-jump weekly modules discovered across custom pages (e.g. S1..S16)
                            std::vector<td::CourseTabInfo> wkJump;
                            std::set<int> seenWk;
                            if (auto cpit = st->snap.customPagesBySpace.find(course.url); cpit != st->snap.customPagesBySpace.end()) {
                                for (const auto& pg : cpit->second) {
                                    for (const auto& pt : pg.portlets) {
                                        for (const auto& sp : pt.linked_subpages) {
                                            if (sp.page_num > 0 && seenWk.insert(sp.page_num).second) wkJump.push_back(sp);
                                        }
                                    }
                                }
                            }
                            if (!wkJump.empty()) {
                                g.DrawString(L"Módulos semanales de la cátedra", -1, &fSec,
                                             Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                                y += 24.0f * dpiScale;
                                float wx = pad;
                                for (const auto& sp : wkJump) {
                                    std::wstring lbl = Utf8ToWide(sp.label.empty() ? ("S" + std::to_string(sp.page_num)) : sp.label);
                                    const float estW = std::clamp(static_cast<float>(lbl.size()) * 7.5f + 28.0f, 48.0f, 160.0f) * dpiScale;
                                    if (wx > pad && wx + estW > pad + cw) { wx = pad; y += 26.0f * dpiScale; }
                                    const float pw = Views::DrawPill(g, wx, y, lbl, dpiScale,
                                                                     Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 160.0f);
                                    st->courseCustomTabRects.push_back({
                                        { static_cast<LONG>(wx), static_cast<LONG>(y),
                                          static_cast<LONG>(wx + pw), static_cast<LONG>(y + 22.0f * dpiScale) },
                                        sp.page_num });
                                    wx += pw + 6.0f * dpiScale;
                                }
                                y += 32.0f * dpiScale;
                            }
                        }

                        // Linked Cátedra + Programa, Cronograma, Informes y Guías de Estudio (detected automatically from course & linked cátedra)
                        const td::CommunityInfo* linkedCat = FindLinkedCatedraForSpace(st->snap, course);
                        {
                            struct StudyDocHit {
                                int prio = 9;
                                std::string tag;
                                std::string title;
                                std::string spaceUrl;
                                std::string objectId;
                                bool fromCatedra = false;
                            };
                            std::vector<StudyDocHit> studyDocs;
                            std::set<std::string> seenDocIds;
                            auto isImageOrUiIcon = [](const std::string& low) -> bool {
                                auto endsWith = [&](const char* ext) {
                                    const size_t n = std::strlen(ext);
                                    return low.size() >= n && low.compare(low.size() - n, n, ext) == 0;
                                };
                                if (endsWith(".jpg") || endsWith(".jpeg") || endsWith(".png") || endsWith(".gif") ||
                                    endsWith(".bmp") || endsWith(".svg") || endsWith(".ico") || endsWith(".webp")) {
                                    return true;
                                }
                                if (low.find("icono") != std::string::npos || low.find("banner") != std::string::npos ||
                                    low.find("logo") != std::string::npos || low.find("thumb") != std::string::npos) {
                                    return true;
                                }
                                return false;
                            };
                            auto isStudyDocExt = [](const std::string& low) -> bool {
                                auto endsWith = [&](const char* ext) {
                                    const size_t n = std::strlen(ext);
                                    return low.size() >= n && low.compare(low.size() - n, n, ext) == 0;
                                };
                                return endsWith(".pdf") || endsWith(".docx") || endsWith(".doc") ||
                                       endsWith(".pptx") || endsWith(".ppt") || endsWith(".xlsx");
                            };
                            auto collectFromSpace = [&](const std::string& sUrl, bool isCat) {
                                auto fit = st->snap.foldersByPath.find(sUrl);
                                if (fit == st->snap.foldersByPath.end()) return;
                                for (const auto& f : fit->second.files) {
                                    const std::string fn = !f.name.empty() ? f.name : f.title;
                                    const std::string low = SafeLowerAscii(fn);
                                    if (isImageOrUiIcon(low)) continue;
                                    std::string tag;
                                    int prio = 9;
                                    if (low.find("programa") != std::string::npos || low.find("carta") != std::string::npos || low.find("silabo") != std::string::npos) {
                                        tag = "Programa"; prio = 0;
                                    } else if (low.find("cronograma") != std::string::npos || low.find("calendario") != std::string::npos) {
                                        tag = "Cronograma"; prio = 1;
                                    } else if (low.find("folleto") != std::string::npos || low.find("modulo") != std::string::npos || low.find("semana") != std::string::npos) {
                                        tag = "Módulo"; prio = 2;
                                    } else if (low.find("informe") != std::string::npos || low.find("laboratorio") != std::string::npos || low.find("proyecto") != std::string::npos) {
                                        tag = "Informe"; prio = 3;
                                    } else if (low.find("examen") != std::string::npos || low.find("parcial") != std::string::npos || low.find("solucion") != std::string::npos) {
                                        tag = "Examen"; prio = 4;
                                    } else if (low.find("guia") != std::string::npos || low.find("evaluaci") != std::string::npos || low.find("practica") != std::string::npos) {
                                        tag = "Guía"; prio = 5;
                                    } else {
                                        continue;
                                    }
                                    // Prefer actual PDFs/Office docs and course-local docs first
                                    int rank = prio * 10 + (isStudyDocExt(low) ? 0 : 4) + (isCat ? 2 : 0);
                                    if (!seenDocIds.insert(f.object_id + "|" + fn).second) continue;
                                    studyDocs.push_back({ rank, tag, fn, sUrl, f.object_id, isCat });
                                }
                            };
                            collectFromSpace(course.url, false);
                            if (linkedCat) collectFromSpace(linkedCat->url, true);
                            std::stable_sort(studyDocs.begin(), studyDocs.end(), [](const StudyDocHit& a, const StudyDocHit& b) {
                                return a.prio < b.prio;
                            });
                            if (studyDocs.size() > 8) studyDocs.resize(8);

                            if (!studyDocs.empty()) {
                                std::wstring sdHdr = linkedCat
                                                         ? (L"Programa, cronograma y guías (incluye " + Utf8ToWide(linkedCat->name) + L")")
                                                         : L"Programa, cronograma y guías de estudio";
                                g.DrawString(sdHdr.c_str(), -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                                y += 24.0f * dpiScale;
                                for (const auto& sd : studyDocs) {
                                    const float rh = 36.0f * dpiScale;
                                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                                    const UiGlass::Rgba8 tCol = (sd.tag == "Programa") ? pal.accentIndigo
                                                                : ((sd.tag == "Cronograma") ? pal.accentTeal
                                                                : ((sd.tag == "Módulo") ? pal.accentBlue : pal.accentOrange));
                                    Views::DrawPill(g, pad + 10.0f * dpiScale, y + 7.5f * dpiScale, Utf8ToWide(sd.tag),
                                                    dpiScale, Views::SoftTint(tCol, dark), tCol, hc, 86.0f);
                                    float rightW = 10.0f * dpiScale;
                                    if (sd.fromCatedra) {
                                        Views::DrawPill(g, pad + cw - 82.0f * dpiScale, y + 7.5f * dpiScale, L"Cátedra",
                                                        dpiScale, pal.sidebarBg, pal.labelSecondary, hc, 72.0f);
                                        rightW += 78.0f * dpiScale;
                                    }
                                    g.DrawString(Utf8ToWide(sd.title).c_str(), -1, &fBody,
                                                 Gdiplus::RectF(pad + 102.0f * dpiScale, y + 9.0f * dpiScale, cw - 108.0f * dpiScale - rightW, 18.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) },
                                                                          "file_open:" + sd.spaceUrl + "|" + sd.objectId });
                                    y += rh + 5.0f * dpiScale;
                                }
                                y += 8.0f * dpiScale;
                            }
                        }

                        // Rich Portlet blocks from portada (all portlets, dynamically measured, zero truncation!)
                        if (!ov->portlets.empty()) {
                            for (size_t pi = 0; pi < ov->portlets.size() && pi < 16; ++pi) {
                                const auto& ptb = ov->portlets[pi];
                                float bodyH = 0.0f;
                                if (!ptb.body_preview.empty()) {
                                    Gdiplus::RectF bb{};
                                    g.MeasureString(Utf8ToWide(ptb.body_preview).c_str(), -1, &fSmall,
                                                    Gdiplus::RectF(0, 0, std::max(80.0f, cw - 28.0f * dpiScale), 220.0f * dpiScale),
                                                    &sfWrap, &bb);
                                    bodyH = std::clamp(bb.Height + 6.0f * dpiScale, 18.0f * dpiScale, 160.0f * dpiScale);
                                }
                                const float ph = std::max(44.0f * dpiScale, 34.0f * dpiScale + bodyH);
                                Views::DrawSurface(g, pad, y, cw, ph, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                                g.DrawString(Utf8ToWide(ptb.title.empty() ? "Información general" : ptb.title).c_str(), -1, &fSec,
                                             Gdiplus::RectF(pad + 14.0f * dpiScale, y + 8.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                             &sfTrim, &brPrim);
                                if (!ptb.body_preview.empty()) {
                                    g.DrawString(Utf8ToWide(ptb.body_preview).c_str(), -1, &fSmall,
                                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 27.0f * dpiScale, cw - 28.0f * dpiScale, bodyH),
                                                 &sfWrap, &brSec);
                                }
                                y += ph + 8.0f * dpiScale;
                            }
                        }

                        // Custom Tab Resources (Zoom, PDFs, Exam Repositories, SharePoint from customTabResourcesByCourse)
                        {
                            std::vector<td::CourseExternalLink> tabLinks;
                            std::set<std::string> seenU;
                            auto addCtr = [&](const std::string& sUrl) {
                                auto it = st->snap.customTabResourcesByCourse.find(sUrl);
                                if (it == st->snap.customTabResourcesByCourse.end()) return;
                                for (const auto& l : it->second) {
                                    if (tabLinks.size() >= 8) break;
                                    if (!l.url.empty() && seenU.insert(l.url).second) tabLinks.push_back(l);
                                }
                            };
                            addCtr(course.url);
                            if (linkedCat) addCtr(linkedCat->url);
                            if (!tabLinks.empty()) {
                                g.DrawString(L"Recursos y enlaces de pestañas", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                                y += 24.0f * dpiScale;
                                for (const auto& l : tabLinks) {
                                    const float rh = 36.0f * dpiScale;
                                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                                    Views::DrawPill(g, pad + 10.0f * dpiScale, y + 7.5f * dpiScale, Utf8ToWide(l.category.empty() ? "Enlace" : l.category),
                                                    dpiScale, Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 90.0f);
                                    g.DrawString(Utf8ToWide(l.label).c_str(), -1, &fBody,
                                                 Gdiplus::RectF(pad + 106.0f * dpiScale, y + 9.0f * dpiScale, cw - 118.0f * dpiScale, 18.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) }, l.url });
                                    y += rh + 5.0f * dpiScale;
                                }
                                y += 8.0f * dpiScale;
                            }
                        }

                        // Official Docs
                        if (!ov->official_docs.empty()) {
                            g.DrawString(L"Documentos oficiales", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                            y += 24.0f * dpiScale;
                            for (const auto& d : ov->official_docs) {
                                const float rh = 36.0f * dpiScale;
                                Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                                Views::DrawPill(g, pad + 10.0f * dpiScale, y + 7.5f * dpiScale, Utf8ToWide(d.category),
                                                dpiScale, Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 90.0f);
                                g.DrawString(Utf8ToWide(d.label).c_str(), -1, &fBody,
                                             Gdiplus::RectF(pad + 106.0f * dpiScale, y + 9.0f * dpiScale, cw - 118.0f * dpiScale, 18.0f * dpiScale),
                                             &sfTrim, &brPrim);
                                st->courseBannerLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) }, d.url });
                                y += rh + 6.0f * dpiScale;
                            }
                            y += 8.0f * dpiScale;
                        }

                        // Weekly sections
                        if (!ov->weekly_sections.empty()) {
                            g.DrawString(L"Cronograma y temas", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                            y += 24.0f * dpiScale;
                            for (const auto& wtxt : ov->weekly_sections) {
                                const float rh = 34.0f * dpiScale;
                                Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                                Gdiplus::SolidBrush db(UiGlass::ToGdip(acc));
                                g.FillEllipse(&db, pad + 12.0f * dpiScale, y + 13.0f * dpiScale, 7.0f * dpiScale, 7.0f * dpiScale);
                                g.DrawString(Utf8ToWide(wtxt).c_str(), -1, &fBody,
                                             Gdiplus::RectF(pad + 26.0f * dpiScale, y + 8.0f * dpiScale, cw - 36.0f * dpiScale, 18.0f * dpiScale),
                                             &sfTrim, &brSec);
                                y += rh + 6.0f * dpiScale;
                            }
                        }
                    }
                }
            } else if (st->courseSubTab == 1) { // Documentos (Recursive Search + Sort + Timestamps + Versions + Folder Alerts)
                auto fwrIt = st->snap.foldersByPath.find(course.url);
                const td::FolderWalkResult* fwr = (fwrIt != st->snap.foldersByPath.end()) ? &fwrIt->second : nullptr;
                const bool hasTree = fwr && !fwr->folders.empty();

                if (!fwr || (fwr->files.empty() && fwr->folders.empty() && fwr->url_links.empty())) {
                    Views::DrawSurface(g, pad, y, cw, 74.0f * dpiScale, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                    g.DrawString(L"Sin documentos sincronizados en este espacio", -1, &fSec,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 14.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                 &sfTrim, &brPrim);
                    g.DrawString(L"Haz clic en «Sincronizar archivos» abajo para explorar carpetas, presentaciones, prácticas y guías.", -1, &fSmall,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 36.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                 &sfTrim, &brTert);
                    y += 84.0f * dpiScale;
                } else {
                    const std::string rawDocQ = st->hEditCourseDocSearch ? WideToUtf8(GetCtrlTextW(st->hEditCourseDocSearch)) : std::string{};
                    const std::string docQFold = FoldAccentsLowerDialog(rawDocQ);
                    std::vector<std::string> docTokens;
                    {
                        std::istringstream iss(docQFold);
                        std::string tok;
                        while (iss >> tok) docTokens.push_back(std::move(tok));
                    }
                    const bool isSearching = !docTokens.empty();
                    const int sortMode = st->hCmbCourseDocSort ? static_cast<int>(SendMessageW(st->hCmbCourseDocSort, CB_GETCURSEL, 0, 0)) : 0;

                    auto matchesTokens = [&](const std::string& hay) {
                        const std::string f = FoldAccentsLowerDialog(hay);
                        for (const auto& t : docTokens) {
                            if (f.find(t) == std::string::npos) return false;
                        }
                        return true;
                    };

                    std::string activeFid = st->currentCourseFolderId;
                    std::string rootFid = fwr->root_folder_id;
                    if (rootFid.empty() && hasTree) rootFid = fwr->folders.front().folder_id;
                    if (activeFid.empty()) activeFid = rootFid;

                    if (isSearching) {
                        Views::DrawSurface(g, pad, y, cw, 28.0f * dpiScale, 7.0f * dpiScale, Views::SoftTint(pal.accentBlue, dark), pal.borderSubtle, 0.7f);
                        std::wstring sHdr = L"Búsqueda recursiva en todas las carpetas de este espacio: «" + Utf8ToWide(rawDocQ) + L"»";
                        g.DrawString(sHdr.c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 10.0f * dpiScale, y + 6.0f * dpiScale, cw - 20.0f * dpiScale, 16.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        y += 34.0f * dpiScale;
                    } else if (hasTree) {
                        // Breadcrumb navigation bar
                        std::map<std::string, const td::FolderRecord*> byId;
                        for (const auto& d : fwr->folders) byId[d.folder_id] = &d;
                        std::vector<std::pair<std::string, std::string>> crumbs;
                        std::string cur = activeFid;
                        std::set<std::string> visited;
                        while (!cur.empty() && visited.insert(cur).second) {
                            auto it = byId.find(cur);
                            if (it == byId.end()) break;
                            std::string nm = (cur == rootFid) ? "Raíz" : (it->second->name.empty() ? cur : it->second->name);
                            crumbs.push_back({ cur, nm });
                            if (cur == rootFid) break;
                            cur = it->second->parent_id;
                        }
                        std::reverse(crumbs.begin(), crumbs.end());
                        if (crumbs.empty()) crumbs.push_back({ rootFid, "Raíz" });

                        float bx = pad;
                        for (size_t ci = 0; ci < crumbs.size(); ++ci) {
                            const bool isLast = (ci + 1 == crumbs.size());
                            float pw = Views::DrawPill(g, bx, y, Utf8ToWide(crumbs[ci].second), dpiScale,
                                                       isLast ? Views::SoftTint(pal.accentBlue, dark) : pal.sidebarBg,
                                                       isLast ? pal.accentBlue : pal.labelSecondary, hc, 160.0f);
                            st->courseBreadcrumbRects.push_back({ { static_cast<LONG>(bx), static_cast<LONG>(y),
                                                                    static_cast<LONG>(bx + pw), static_cast<LONG>(y + 21.0f * dpiScale) }, crumbs[ci].first });
                            bx += pw + 4.0f * dpiScale;
                            if (!isLast) {
                                g.DrawString(L"›", -1, &fBody, Gdiplus::RectF(bx, y + 1.0f, 10.0f * dpiScale, 20.0f * dpiScale), &sfTrim, &brTert);
                                bx += 12.0f * dpiScale;
                            }
                        }
                        y += 28.0f * dpiScale;
                    }

                    // Collect & sort folders (direct children normally, or ALL matching folders when searching)
                    if (hasTree) {
                        std::vector<const td::FolderRecord*> visFolders;
                        for (const auto& d : fwr->folders) {
                            if (d.folder_id == rootFid) continue;
                            if (isSearching) {
                                if (!matchesTokens(d.name + " " + d.folder_path + " " + d.author_name)) continue;
                            } else {
                                if (d.folder_id == activeFid || d.parent_id != activeFid) continue;
                            }
                            visFolders.push_back(&d);
                        }
                        std::stable_sort(visFolders.begin(), visFolders.end(), [&](const td::FolderRecord* a, const td::FolderRecord* b) {
                            if (sortMode == 0) {
                                const std::string& da = !a->last_modified_ansi.empty() ? a->last_modified_ansi : a->creation_date_ansi;
                                const std::string& db = !b->last_modified_ansi.empty() ? b->last_modified_ansi : b->creation_date_ansi;
                                if (da != db) return da > db;
                            } else if (sortMode == 1) {
                                if (a->creation_date_ansi != b->creation_date_ansi) return a->creation_date_ansi > b->creation_date_ansi;
                            }
                            return SafeLowerAscii(a->name) < SafeLowerAscii(b->name);
                        });
                        for (const auto* dPtr : visFolders) {
                            const auto& d = *dPtr;
                            const float rh = 50.0f * dpiScale;
                            Views::DrawSurface(g, pad, y, cw, rh, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                            const float bSz = 28.0f * dpiScale;
                            Views::DrawSurface(g, pad + 10.0f * dpiScale, y + (rh - bSz) * 0.5f, bSz, bSz, 7.0f * dpiScale,
                                               Views::SoftTint(pal.accentOrange, dark), { 0, 0, 0, 0 }, 0.0f);
                            UiGlass::DrawVectorIconGdiPlus(g, UiGlass::IconId::LayersStack,
                                                           pad + 10.0f * dpiScale + bSz * 0.5f, y + rh * 0.5f,
                                                           13.0f * dpiScale, UiGlass::ToGdip(pal.accentOrange), 1.5f * dpiScale);

                            // Alert toggle pill + element count pill on right
                            const float cntW = 68.0f * dpiScale;
                            const float altW = 74.0f * dpiScale;
                            std::wstring cntLbl = d.child_count > 0 ? (std::to_wstring(d.child_count) + L" elem.") : L"Carpeta";
                            Views::DrawPill(g, pad + cw - cntW - 10.0f * dpiScale, y + 14.0f * dpiScale, cntLbl, dpiScale,
                                            pal.sidebarBg, pal.labelSecondary, hc, 72.0f);
                            const UiGlass::Rgba8 altCol = d.subscription_active ? pal.accentGreen : pal.accentBlue;
                            const float drawnAltW = Views::DrawPill(g, pad + cw - cntW - altW - 16.0f * dpiScale, y + 14.0f * dpiScale,
                                                                    d.subscription_active ? L"Alertas ON" : L"Alertas", dpiScale,
                                                                    Views::SoftTint(altCol, dark), altCol, hc, 76.0f);
                            st->courseDocActionRects.push_back({
                                { static_cast<LONG>(pad + cw - cntW - altW - 16.0f * dpiScale), static_cast<LONG>(y + 14.0f * dpiScale),
                                  static_cast<LONG>(pad + cw - cntW - altW - 16.0f * dpiScale + drawnAltW), static_cast<LONG>(y + 36.0f * dpiScale) },
                                "alert_folder:" + d.folder_id + "|" + (d.subscription_active ? "0" : "1") });

                            const float rightUsed = cntW + altW + 26.0f * dpiScale;
                            g.DrawString(Utf8ToWide(d.name.empty() ? d.folder_id : d.name).c_str(), -1, &fSec,
                                         Gdiplus::RectF(pad + 48.0f * dpiScale, y + 7.0f * dpiScale, cw - 52.0f * dpiScale - rightUsed, 18.0f * dpiScale),
                                         &sfTrim, &brPrim);
                            std::string fSub = isSearching && !d.folder_path.empty() ? d.folder_path : "Carpeta";
                            const std::string fMod = FormatFriendlyTimestampDialog(d.last_modified_ansi);
                            const std::string fCre = FormatFriendlyTimestampDialog(d.creation_date_ansi);
                            if (!fMod.empty()) fSub += "  ·  Mod. " + fMod;
                            else if (!fCre.empty()) fSub += "  ·  Creación " + fCre;
                            if (!d.author_name.empty()) fSub += "  ·  " + d.author_name;
                            g.DrawString(Utf8ToWide(fSub).c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pad + 48.0f * dpiScale, y + 26.0f * dpiScale, cw - 52.0f * dpiScale - rightUsed, 15.0f * dpiScale),
                                         &sfTrim, &brTert);
                            st->courseFolderRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y),
                                                                static_cast<LONG>(pad + cw - rightUsed), static_cast<LONG>(y + rh) }, d.folder_id });
                            y += rh + 6.0f * dpiScale;
                        }
                    }

                    // Collect & sort visible files (in active folder or recursively across space when searching)
                    bool anyFileHasParent = false;
                    for (const auto& f : st->currentCourseFiles) {
                        if (!f.parent_folder_id.empty()) { anyFileHasParent = true; break; }
                    }
                    std::vector<size_t> visFileIndices;
                    for (size_t fi = 0; fi < st->currentCourseFiles.size(); ++fi) {
                        const auto& f = st->currentCourseFiles[fi];
                        if (isSearching) {
                            if (!matchesTokens(f.name + " " + f.title + " " + f.folder_path + " " + f.author_name)) continue;
                        } else {
                            if (hasTree && anyFileHasParent && !activeFid.empty() && f.parent_folder_id != activeFid) continue;
                        }
                        visFileIndices.push_back(fi);
                    }
                    std::stable_sort(visFileIndices.begin(), visFileIndices.end(), [&](size_t ia, size_t ib) {
                        const auto& a = st->currentCourseFiles[ia];
                        const auto& b = st->currentCourseFiles[ib];
                        if (sortMode == 0) {
                            const std::string& da = !a.last_modified_ansi.empty() ? a.last_modified_ansi : a.creation_date_ansi;
                            const std::string& db = !b.last_modified_ansi.empty() ? b.last_modified_ansi : b.creation_date_ansi;
                            if (da != db) return da > db;
                        } else if (sortMode == 1) {
                            if (a.creation_date_ansi != b.creation_date_ansi) return a.creation_date_ansi > b.creation_date_ansi;
                        } else if (sortMode == 3) {
                            if (a.content_size != b.content_size) return a.content_size > b.content_size;
                        }
                        return SafeLowerAscii(a.name) < SafeLowerAscii(b.name);
                    });

                    st->courseFileRects.assign(st->currentCourseFiles.size(), RECT{ 0, 0, 0, 0 });
                    int shownFiles = 0;
                    for (size_t fi : visFileIndices) {
                        const auto& f = st->currentCourseFiles[fi];
                        ++shownFiles;
                        const float rh = 58.0f * dpiScale;
                        const bool sel = (static_cast<int>(fi) == st->selectedCourseFileIdx);
                        const bool hov = (static_cast<int>(fi) == st->courseFileHoverIdx && !sel);
                        const UiGlass::Rgba8 bg = sel ? Views::SoftTint(pal.accentBlue, dark)
                                                      : (hov ? pal.sidebarHover : pal.contentCardElevated);
                        const UiGlass::Rgba8 bd = sel ? pal.accentBlue : pal.borderSubtle;
                        Views::DrawSurface(g, pad, y, cw, rh, 9.0f * dpiScale, bg, bd, sel ? 1.4f : 0.85f);

                        UiGlass::Rgba8 extCol = pal.accentBlue;
                        const std::wstring extLbl = Views::FileExtBadgeLabel(f.name, extCol, pal);
                        const float bSz = 32.0f * dpiScale;
                        Views::DrawSurface(g, pad + 10.0f * dpiScale, y + (rh - bSz) * 0.5f, bSz, bSz, 7.0f * dpiScale,
                                           Views::SoftTint(extCol, dark), { 0, 0, 0, 0 }, 0.0f);
                        Gdiplus::Font fExt(&ff, 10.5f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                        Gdiplus::SolidBrush extBr(UiGlass::ToGdip(extCol));
                        Gdiplus::StringFormat sfC;
                        sfC.SetAlignment(Gdiplus::StringAlignmentCenter);
                        sfC.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                        g.DrawString(extLbl.c_str(), -1, &fExt,
                                     Gdiplus::RectF(pad + 10.0f * dpiScale, y + (rh - bSz) * 0.5f, bSz, bSz), &sfC, &extBr);

                        const bool isLocal = (f.status == td::FileStatus::Downloaded || f.deduplicated);
                        const float pillW = 78.0f * dpiScale;
                        const float verW = 68.0f * dpiScale;
                        const float pillY = y + 8.0f * dpiScale;
                        Views::DrawPill(g, pad + cw - pillW - 10.0f * dpiScale, pillY,
                                        isLocal ? L"Sin conexión" : L"En la nube", dpiScale,
                                        isLocal ? Views::SoftTint(pal.accentGreen, dark) : pal.sidebarBg,
                                        isLocal ? pal.accentGreen : pal.labelSecondary, hc, 84.0f);
                        float rightUsed = pillW + 16.0f * dpiScale;
                        if (!f.object_id.empty()) {
                            const float drawnVerW = Views::DrawPill(g, pad + cw - rightUsed - verW - 4.0f * dpiScale, pillY,
                                                                    L"Detalles", dpiScale,
                                                                    Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 72.0f);
                            st->courseDocActionRects.push_back({
                                { static_cast<LONG>(pad + cw - rightUsed - verW - 4.0f * dpiScale), static_cast<LONG>(pillY),
                                  static_cast<LONG>(pad + cw - rightUsed - verW - 4.0f * dpiScale + drawnVerW), static_cast<LONG>(pillY + 21.0f * dpiScale) },
                                "ver:" + f.object_id + "|" + f.name });
                            rightUsed += verW + 8.0f * dpiScale;
                        }

                        const float tx = pad + 52.0f * dpiScale;
                        const float tw = std::max(60.0f, cw - 58.0f * dpiScale - rightUsed);
                        g.DrawString(Utf8ToWide(f.name).c_str(), -1, &fSec,
                                     Gdiplus::RectF(tx, y + 7.0f * dpiScale, tw, 18.0f * dpiScale), &sfTrim, &brPrim);

                        std::string line2 = (f.folder_path.empty() ? "Raíz" : f.folder_path) +
                                            "  ·  " + std::to_string(std::max<std::uint64_t>(1, f.content_size / 1024)) + " KB";
                        const std::string modStr = FormatFriendlyTimestampDialog(f.last_modified_ansi);
                        const std::string creStr = FormatFriendlyTimestampDialog(f.creation_date_ansi);
                        if (!modStr.empty()) line2 += "  ·  Mod. " + modStr;
                        if (!creStr.empty() && creStr.substr(0, 10) != modStr.substr(0, std::min<size_t>(10, modStr.size()))) {
                            line2 += "  ·  Creación " + creStr.substr(0, 10);
                        }
                        if (!f.live_revision.empty() && f.live_revision != "1") line2 += "  ·  Rev. " + f.live_revision;
                        if (!f.author_name.empty()) line2 += "  ·  " + f.author_name;
                        if (f.plagiarism_percentage >= 0.0) line2 += "  ·  Similitud " + FmtDouble(f.plagiarism_percentage, 1) + "%";
                        g.DrawString(Utf8ToWide(line2).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(tx, y + 29.0f * dpiScale, cw - 66.0f * dpiScale, 16.0f * dpiScale), &sfTrim, &brTert);

                        RECT frc{ static_cast<LONG>(pad), static_cast<LONG>(y),
                                  static_cast<LONG>(pad + cw - rightUsed), static_cast<LONG>(y + rh) };
                        st->courseFileRects[fi] = frc;
                        y += rh + 7.0f * dpiScale;
                    }

                    // External URL links inside current folder
                    for (const auto& u : fwr->url_links) {
                        if (isSearching) {
                            if (!matchesTokens(u.title + " " + u.url)) continue;
                        } else if (hasTree && !u.parent_folder_id.empty() && !activeFid.empty() && u.parent_folder_id != activeFid) {
                            continue;
                        }
                        const float rh = 42.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                        Views::DrawPill(g, pad + 10.0f * dpiScale, y + 10.0f * dpiScale, L"Enlace", dpiScale,
                                        Views::SoftTint(pal.accentTeal, dark), pal.accentTeal, hc, 68.0f);
                        g.DrawString(Utf8ToWide(u.title.empty() ? u.url : u.title).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 86.0f * dpiScale, y + 11.0f * dpiScale, cw - 98.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        st->courseUrlLinkRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y),
                                                             static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) }, u.url });
                        y += rh + 6.0f * dpiScale;
                    }
                    if (shownFiles == 0 && st->courseFolderRects.empty() && st->courseUrlLinkRects.empty()) {
                        g.DrawString(isSearching ? L"Sin carpetas ni archivos que coincidan con la búsqueda." : L"Esta carpeta está vacía.",
                                     -1, &fSmall,
                                     Gdiplus::RectF(pad, y + 6.0f * dpiScale, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                        y += 28.0f * dpiScale;
                    }
                }
            } else if (st->courseSubTab == 2) { // Evaluaciones: Nota base 100 (% -> Nota), Rúbrica visual, Adjuntos y What-If
                st->courseEvalActionRects.clear();
                if (!ev || ev->categories.empty()) {
                    const float emptyH = 82.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, emptyH, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                    g.DrawString(L"Aún no hay evaluaciones descargadas para este espacio.", -1, &fBody,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 14.0f * dpiScale, cw - 28.0f * dpiScale, 20.0f * dpiScale),
                                 &sfTrim, &brSec);
                    const float bw = Views::DrawPill(g, pad + 14.0f * dpiScale, y + 42.0f * dpiScale,
                                                     L"Cargar evaluaciones del curso", dpiScale,
                                                     Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 220.0f);
                    st->courseEvalActionRects.push_back({ { static_cast<LONG>(pad + 14.0f * dpiScale), static_cast<LONG>(y + 42.0f * dpiScale),
                                                            static_cast<LONG>(pad + 14.0f * dpiScale + bw), static_cast<LONG>(y + 66.0f * dpiScale) },
                                                          "sync_course" });
                    y += emptyH + 10.0f * dpiScale;
                } else {
                    double target = 70.0;
                    if (st->hEditWhatIf) {
                        double p = std::atof(WideToUtf8(GetCtrlTextW(st->hEditWhatIf)).c_str());
                        if (p >= 1.0 && p <= 100.0) target = p;
                    }
                    const auto scBase = td::ComputeGradeWhatIf(*ev, target, {});
                    const auto scSim = td::ComputeGradeWhatIf(*ev, target, st->whatIfOverridesByAssignId);
                    const bool hasSim = !st->whatIfOverridesByAssignId.empty();
                    const double grade100Now = td::ComputeCourseAccumulatedGrade100(*ev);
                    const double grade100Sim = (scSim.evaluated_weight_pct > 1e-6)
                                                   ? std::clamp((scSim.projected_final_with_overrides / scSim.evaluated_weight_pct) * 100.0, 0.0, 100.0)
                                                   : grade100Now;
                    const double displayGrade100 = hasSim ? grade100Sim : grade100Now;
                    const UiGlass::Rgba8 heroCol = (displayGrade100 < 0.0)
                                                       ? pal.accentBlue
                                                       : (displayGrade100 >= 70.0 ? pal.accentGreen : pal.accentRed);

                    // Visual Grade Hero Card (Base-100 Grade derived from percentage + % accumulated)
                    const float heroH = 104.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, heroH, 10.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 1.0f);
                    g.DrawString(hasSim ? L"Nota proyectada (base 100)" : L"Nota actual (base 100)", -1, &fSmall,
                                 Gdiplus::RectF(pad + 16.0f * dpiScale, y + 10.0f * dpiScale, 210.0f * dpiScale, 16.0f * dpiScale),
                                 &sfTrim, &brTert);
                    Gdiplus::SolidBrush heroNumBr(UiGlass::ToGdip(heroCol));
                    std::wstring scoreBig = (displayGrade100 >= 0.0)
                                                ? (Utf8ToWide(FmtDouble(displayGrade100, 1)) + L" / 100")
                                                : L"-- / 100";
                    g.DrawString(scoreBig.c_str(), -1, &fBigNum,
                                 Gdiplus::RectF(pad + 16.0f * dpiScale, y + 26.0f * dpiScale, 210.0f * dpiScale, 32.0f * dpiScale),
                                 &sfTrim, &heroNumBr);

                    const auto& sc = hasSim ? scSim : scBase;
                    const UiGlass::Rgba8 wiCol = sc.already_reached
                                                     ? pal.accentGreen
                                                     : (sc.mathematically_possible ? pal.accentBlue : pal.accentRed);
                    std::wstring wiTxt = sc.already_reached
                                             ? (L"Meta " + Utf8ToWide(FmtDouble(target, 0)) + L" alcanzada")
                                             : (sc.mathematically_possible
                                                    ? (L"Necesitas " + Utf8ToWide(FmtDouble(sc.required_avg_on_remaining_100, 1)) + L"/100 en el restante para " + Utf8ToWide(FmtDouble(target, 0)))
                                                    : L"Meta no alcanzable matemáticamente");
                    Views::DrawPill(g, pad + cw - 270.0f * dpiScale, y + 12.0f * dpiScale, wiTxt,
                                    dpiScale, Views::SoftTint(wiCol, dark), wiCol, hc, 256.0f);
                    if (hasSim) {
                        const float clW = Views::DrawPill(g, pad + cw - 144.0f * dpiScale, y + 38.0f * dpiScale,
                                                          L"Limpiar simulación", dpiScale,
                                                          Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 130.0f);
                        st->courseEvalActionRects.push_back({ { static_cast<LONG>(pad + cw - 144.0f * dpiScale), static_cast<LONG>(y + 38.0f * dpiScale),
                                                                static_cast<LONG>(pad + cw - 144.0f * dpiScale + clW), static_cast<LONG>(y + 60.0f * dpiScale) },
                                                              "whatif_clear" });
                    }

                    std::wstring evMeta = L"Puntos ganados: " + Utf8ToWide(FmtDouble(sc.current_earned_pct, 2)) +
                                          L" / " + Utf8ToWide(FmtDouble(sc.evaluated_weight_pct, 1)) + L"% evaluado" +
                                          L"  ·  Restante: " + Utf8ToWide(FmtDouble(sc.remaining_weight_pct, 1)) +
                                          L"%  ·  Máx: " + Utf8ToWide(FmtDouble(sc.max_possible_grade, 1));
                    g.DrawString(evMeta.c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 16.0f * dpiScale, y + 62.0f * dpiScale, cw - 32.0f * dpiScale, 16.0f * dpiScale),
                                 &sfTrim, &brSec);
                    const float barRatio = (displayGrade100 >= 0.0)
                                               ? static_cast<float>(std::clamp(displayGrade100 / 100.0, 0.0, 1.0))
                                               : static_cast<float>(std::clamp(sc.current_earned_pct / 100.0, 0.0, 1.0));
                    Views::DrawProgressBar(g, pad + 16.0f * dpiScale, y + 83.0f * dpiScale, cw - 32.0f * dpiScale, 7.0f * dpiScale,
                                           barRatio, pal.sidebarBg, heroCol);
                    y += heroH + 14.0f * dpiScale;

                    // Categories & Selectable Evaluation Rows (Base-100 Grade + Earned/Weight % + Expandable Inspector)
                    int flatIdx = 0;
                    float selCardContentY = -1.0f;
                    for (const auto& cat : ev->categories) {
                        g.DrawString(Utf8ToWide(cat.name).c_str(), -1, &fSec,
                                     Gdiplus::RectF(pad, y, cw - 120.0f * dpiScale, 18.0f * dpiScale), &sfTrim, &brPrim);
                        std::wstring catPct = Utf8ToWide(FmtDouble(cat.earned_pct, 1) + " / " + FmtDouble(cat.weight_pct, 1) + "%");
                        Views::DrawPill(g, pad + cw - 104.0f * dpiScale, y - 2.0f, catPct, dpiScale,
                                        pal.sidebarBg, pal.labelSecondary, hc, 104.0f);
                        y += 24.0f * dpiScale;

                        for (const auto& it : cat.items) {
                            const bool hasExtra = !it.teacher_comment.empty() || !it.submitted_files.empty();
                            const bool isSel = (flatIdx == st->selectedEvalFlatIdx);
                            if (isSel && selCardContentY < 0.0f) {
                                selCardContentY = y + static_cast<float>(st->courseDetailScrollY);
                            }
                            const float rh = (hasExtra ? 64.0f : 50.0f) * dpiScale;
                            Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale,
                                               isSel ? Views::SoftTint(pal.accentBlue, dark) : pal.contentCardElevated,
                                               isSel ? pal.accentBlue : pal.borderSubtle, isSel ? 1.4f : 0.8f);

                            const double g100 = td::ComputeEvaluationGrade100(it);
                            auto simIt = st->whatIfOverridesByAssignId.find(it.assign_id);
                            const bool isSimmed = (!it.is_graded && simIt != st->whatIfOverridesByAssignId.end());

                            // Base-100 Grade or Status Pill on right (green >= 70, red < 70)
                            UiGlass::Rgba8 gCol = pal.labelSecondary;
                            std::wstring gTxt = L"Pendiente";
                            if (it.is_graded && g100 >= 0.0) {
                                gCol = (g100 >= 70.0) ? pal.accentGreen : pal.accentRed;
                                gTxt = Utf8ToWide(FmtDouble(g100, 1) + " / 100");
                            } else if (isSimmed) {
                                gCol = pal.accentIndigo;
                                gTxt = Utf8ToWide("Sim " + FmtDouble(simIt->second, 0) + "/100");
                            } else if (!it.submitted_files.empty()) {
                                gCol = pal.accentTeal;
                                gTxt = L"Entregada";
                            } else if (it.can_upload) {
                                gCol = pal.accentBlue;
                                gTxt = L"Entregable";
                            }
                            const float gW = 92.0f * dpiScale;
                            Views::DrawPill(g, pad + cw - gW - 10.0f * dpiScale, y + 9.0f * dpiScale, gTxt, dpiScale,
                                            (it.is_graded || isSimmed || it.can_upload || !it.submitted_files.empty())
                                                ? Views::SoftTint(gCol, dark)
                                                : pal.sidebarBg,
                                            gCol, hc, 96.0f);

                            // Urgency countdown pill when pending with valid due_date_iso
                            float rightUsed = gW + 18.0f * dpiScale;
                            if (!it.is_graded && it.due_date_iso.size() >= 10) {
                                std::string cd = FormatCountdownToIso(it.due_date_iso.substr(0, 10), st->snap.serverTimeOffsetSec);
                                if (!cd.empty()) {
                                    const bool overdue = (cd.rfind("Hace ", 0) == 0);
                                    const UiGlass::Rgba8 uCol = (cd == "Hoy" || overdue)
                                                                    ? pal.accentRed
                                                                    : (cd == "Mañana" ? pal.accentOrange : pal.accentIndigo);
                                    std::wstring uTxt = overdue ? (it.allow_late_submission ? L"Tardía" : L"Vencida") : Utf8ToWide(cd);
                                    const float uW = 74.0f * dpiScale;
                                    Views::DrawPill(g, pad + cw - rightUsed - uW - 4.0f * dpiScale, y + 9.0f * dpiScale, uTxt,
                                                    dpiScale, Views::SoftTint(uCol, dark), uCol, hc, 78.0f);
                                    rightUsed += uW + 8.0f * dpiScale;
                                }
                            }

                            g.DrawString(Utf8ToWide(it.name).c_str(), -1, &fBody,
                                         Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - rightUsed - 18.0f * dpiScale, 17.0f * dpiScale),
                                         &sfTrim, &brPrim);
                            std::string sub;
                            if (it.is_graded) {
                                sub = "Aporte: " + FmtDouble(it.earned_pct, 2) + " / " + FmtDouble(it.weight_pct, 1) + "%";
                            } else if (isSimmed) {
                                double simPts = (simIt->second / 100.0) * it.weight_pct;
                                sub = "Simulado: " + FmtDouble(simPts, 2) + " / " + FmtDouble(it.weight_pct, 1) + "%";
                            } else {
                                sub = "Valor: " + FmtDouble(it.weight_pct, 1) + "%";
                            }
                            if (!it.due_date_text.empty() && it.due_date_text.find("no definida") == std::string::npos) {
                                sub += "  ·  Vence: " + it.due_date_text;
                            }
                            if (it.allow_late_submission) sub += "  ·  Tardía permitida";
                            // Auto-lookup synced rubric from local v2 cache (no manual click needed)
                            const td::RubricBreakdown* syncedRb = nullptr;
                            if (it.has_rubric && !it.rubric_id.empty()) {
                                const std::string rk = it.rubric_id + "|" + it.task_item_id + "|" + it.task_id;
                                if (auto rbit = st->snap.rubricsByKey.find(rk);
                                    rbit != st->snap.rubricsByKey.end() && !rbit->second.criteria.empty()) {
                                    syncedRb = &rbit->second;
                                } else if (st->activeRubric.has_value() &&
                                           st->activeRubricAssignId == it.assign_id &&
                                           !st->activeRubric->criteria.empty()) {
                                    syncedRb = &(*st->activeRubric);
                                }
                            }
                            if (it.has_rubric) {
                                if (syncedRb && syncedRb->is_graded) {
                                    sub += "  ·  Rúbrica: " + FmtDouble(syncedRb->total_score, 1) +
                                           " / " + FmtDouble(syncedRb->max_score > 0.0 ? syncedRb->max_score : 100.0, 0);
                                } else if (syncedRb) {
                                    sub += "  ·  Rúbrica sincronizada";
                                } else {
                                    sub += "  ·  Rúbrica";
                                }
                            }
                            g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pad + 12.0f * dpiScale, y + 26.0f * dpiScale, cw - 24.0f * dpiScale, 15.0f * dpiScale),
                                         &sfTrim, &brTert);
                            if (hasExtra) {
                                std::string extraLine;
                                if (!it.submitted_files.empty()) {
                                    extraLine = "Entrega: " + it.submitted_files.front();
                                    if (it.submitted_files.size() > 1) extraLine += " (+" + std::to_string(it.submitted_files.size() - 1) + ")";
                                }
                                if (!it.teacher_comment.empty()) {
                                    if (!extraLine.empty()) extraLine += "  ·  ";
                                    extraLine += "Comentario: " + it.teacher_comment;
                                }
                                g.DrawString(Utf8ToWide(extraLine).c_str(), -1, &fSmall,
                                             Gdiplus::RectF(pad + 12.0f * dpiScale, y + 43.0f * dpiScale, cw - 24.0f * dpiScale, 15.0f * dpiScale),
                                             &sfTrim, &brSec);
                            }
                            st->courseEvalCardRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y),
                                                                  static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) }, flatIdx });
                            y += rh + 6.0f * dpiScale;

                            // Inline guías de interfaz Inspector Card for Selected Evaluation (Description, Links, Per-Item What-If, Full Analytic Rubric Matrix)
                            if (isSel) {
                                const td::RubricBreakdown* rbPtr = syncedRb;
                                const bool showRubricTbl = (rbPtr != nullptr && !rbPtr->criteria.empty());
                                const float ix = pad + 18.0f * dpiScale;
                                const float iw = std::max(260.0f * dpiScale, cw - 36.0f * dpiScale);
                                Gdiplus::Font fRubBold(&ff, 10.6f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                                Gdiplus::Font fRubCell(&ff, 10.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                                Gdiplus::Font fRubCellBold(&ff, 10.2f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

                                // Pre-measure rubric matrix row heights accurately so zero text clips or overlaps
                                size_t numScaleCols = 0;
                                std::vector<float> critRowHeights;
                                std::vector<float> critCommentHeights;
                                float rubricMatrixTotalH = 0.0f;
                                if (showRubricTbl) {
                                    for (const auto& cr : rbPtr->criteria) {
                                        numScaleCols = std::max(numScaleCols, cr.levels.size());
                                    }
                                    numScaleCols = std::clamp<size_t>(numScaleCols, 1, 6);
                                    const float catColW = iw * 0.24f;
                                    const float lvlColW = (iw - catColW) / static_cast<float>(numScaleCols);
                                    const float cellPadX = 6.0f * dpiScale;
                                    const float cellPadY = 6.0f * dpiScale;
                                    rubricMatrixTotalH = 38.0f * dpiScale + 30.0f * dpiScale; // header bar + scale header row
                                    critRowHeights.reserve(rbPtr->criteria.size());
                                    critCommentHeights.reserve(rbPtr->criteria.size());
                                    for (const auto& cr : rbPtr->criteria) {
                                        Gdiplus::RectF bound{};
                                        std::wstring catW = Utf8ToWide(cr.category_name);
                                        g.MeasureString(catW.c_str(), -1, &fRubBold,
                                                        Gdiplus::RectF(0, 0, std::max(40.0f, catColW - cellPadX * 2.0f), 260.0f * dpiScale),
                                                        &sfWrap, &bound);
                                        float rowH = bound.Height + 28.0f * dpiScale; // space for Peso pill
                                        for (size_t li = 0; li < cr.levels.size() && li < numScaleCols; ++li) {
                                            std::string descTxt = cr.levels[li].description;
                                            if (!cr.comment.empty() && descTxt.size() > cr.comment.size()) {
                                                const size_t cp = descTxt.rfind(cr.comment);
                                                if (cp != std::string::npos && cp + cr.comment.size() == descTxt.size()) {
                                                    descTxt.erase(cp);
                                                    while (!descTxt.empty() && std::isspace(static_cast<unsigned char>(descTxt.back()))) descTxt.pop_back();
                                                }
                                            }
                                            std::wstring descW = Utf8ToWide(descTxt);
                                            Gdiplus::RectF lb{};
                                            g.MeasureString(descW.c_str(), -1, cr.levels[li].achieved ? &fRubCellBold : &fRubCell,
                                                            Gdiplus::RectF(0, 0, std::max(40.0f, lvlColW - cellPadX * 2.0f), 320.0f * dpiScale),
                                                            &sfWrap, &lb);
                                            float chH = lb.Height + cellPadY * 2.0f + (cr.levels[li].achieved ? 16.0f * dpiScale : 0.0f);
                                            rowH = std::max(rowH, chH);
                                        }
                                        rowH = std::clamp(rowH, 42.0f * dpiScale, 260.0f * dpiScale);
                                        critRowHeights.push_back(rowH);
                                        rubricMatrixTotalH += rowH;

                                        float cmtH = 0.0f;
                                        if (!cr.comment.empty()) {
                                            std::wstring cmtW = Utf8ToWide("Comentario: " + cr.comment);
                                            Gdiplus::RectF cb{};
                                            g.MeasureString(cmtW.c_str(), -1, &fRubCell,
                                                            Gdiplus::RectF(0, 0, std::max(80.0f, iw - 24.0f * dpiScale), 260.0f * dpiScale),
                                                            &sfWrap, &cb);
                                            cmtH = std::clamp(cb.Height + 14.0f * dpiScale, 26.0f * dpiScale, 200.0f * dpiScale);
                                            rubricMatrixTotalH += cmtH + 4.0f * dpiScale;
                                        }
                                        critCommentHeights.push_back(cmtH);
                                    }
                                    rubricMatrixTotalH += 8.0f * dpiScale;
                                }

                                const bool hasAnyActionPill =
                                    (it.has_rubric && !it.rubric_id.empty()) ||
                                    !it.attachment_url.empty() || !it.solution_url.empty() ||
                                    !it.feedback_url.empty() || (!it.is_graded && !it.assign_id.empty());
                                const bool hasAnyInspContent =
                                    !it.description.empty() || !it.teacher_comment.empty() ||
                                    showRubricTbl || hasAnyActionPill;
                                if (!hasAnyInspContent) {
                                    // Skip empty inspector frame when graded item has no extra metadata/rubric
                                } else {
                                float inspH = (hasAnyActionPill ? 38.0f : 14.0f) * dpiScale;
                                if (!it.description.empty()) inspH += 22.0f * dpiScale;
                                if (!it.teacher_comment.empty()) inspH += 20.0f * dpiScale;
                                if (showRubricTbl) inspH += rubricMatrixTotalH;

                                Views::DrawSurface(g, pad + 8.0f * dpiScale, y, cw - 16.0f * dpiScale, inspH,
                                                   9.0f * dpiScale, pal.contentCard, pal.borderSubtle, 0.9f);
                                float iy = y + 8.0f * dpiScale;
                                if (!it.description.empty()) {
                                    g.DrawString(Utf8ToWide("Descripción: " + it.description).c_str(), -1, &fSmall,
                                                 Gdiplus::RectF(ix, iy, iw, 18.0f * dpiScale), &sfTrim, &brSec);
                                    iy += 20.0f * dpiScale;
                                }
                                if (!it.teacher_comment.empty()) {
                                    g.DrawString(Utf8ToWide("Observación docente: " + it.teacher_comment).c_str(), -1, &fSmall,
                                                 Gdiplus::RectF(ix, iy, iw, 18.0f * dpiScale), &sfTrim, &brPrim);
                                    iy += 20.0f * dpiScale;
                                }

                                // Interactive Action Pills Row
                                float ax = ix;
                                auto addActPill = [&](const std::wstring& lbl, const std::string& tag, UiGlass::Rgba8 col) {
                                    if (ax > ix + iw - 70.0f * dpiScale) return;
                                    const float pw = Views::DrawPill(g, ax, iy, lbl, dpiScale, Views::SoftTint(col, dark), col, hc, 140.0f);
                                    st->courseEvalActionRects.push_back({ { static_cast<LONG>(ax), static_cast<LONG>(iy),
                                                                            static_cast<LONG>(ax + pw), static_cast<LONG>(iy + 22.0f * dpiScale) },
                                                                          tag });
                                    ax += pw + 6.0f * dpiScale;
                                };
                                if (it.has_rubric && !it.rubric_id.empty()) {
                                    addActPill(showRubricTbl ? L"Actualizar rúbrica" : L"Cargar rúbrica",
                                               "rubric:" + it.assign_id, pal.accentIndigo);
                                }
                                if (!it.attachment_url.empty()) {
                                    addActPill(L"Enunciado", "url:" + it.attachment_url, pal.accentBlue);
                                }
                                if (!it.solution_url.empty()) {
                                    addActPill(L"Solución", "url:" + it.solution_url, pal.accentGreen);
                                }
                                if (!it.feedback_url.empty()) {
                                    addActPill(L"Retroalimentación", "url:" + it.feedback_url, pal.accentTeal);
                                }
                                if (!it.is_graded && !it.assign_id.empty()) {
                                    addActPill(L"Simular 70", "sim70:" + it.assign_id, pal.accentOrange);
                                    addActPill(L"Simular 85", "sim85:" + it.assign_id, pal.accentBlue);
                                    addActPill(L"Simular 100", "sim100:" + it.assign_id, pal.accentGreen);
                                }
                                if (!it.assign_id.empty()) {
                                    addActPill(L"Alertas", "eval_alert:" + it.assign_id, pal.accentTeal);
                                }
                                if (hasAnyActionPill) iy += 28.0f * dpiScale;

                                // Full guías de interfaz Analytic Rubric Matrix (synchronized automatically)
                                if (showRubricTbl) {
                                    const auto& rb = *rbPtr;
                                    const float catColW = iw * 0.24f;
                                    const float lvlColW = (iw - catColW) / static_cast<float>(numScaleCols);
                                    const float cellPadX = 6.0f * dpiScale;
                                    const float cellPadY = 5.0f * dpiScale;

                                    // Rubric Header Bar
                                    std::string rbSub = std::to_string(rb.criteria.size()) + " criterios";
                                    if (!rb.evaluated_target.empty()) rbSub = rb.evaluated_target + "  ·  " + rbSub;
                                    const double rbMax = rb.max_score > 0.0 ? rb.max_score : 100.0;
                                    UiGlass::Rgba8 scCol = pal.labelSecondary;
                                    std::wstring scTxt = L"Sin calificar";
                                    if (rb.is_graded) {
                                        const double pct = (rbMax > 0.0) ? (rb.total_score / rbMax) * 100.0 : rb.total_score;
                                        scCol = (pct >= 70.0) ? pal.accentGreen : ((pct >= 60.0) ? pal.accentOrange : pal.accentRed);
                                        scTxt = Utf8ToWide("Puntuación: " + FmtDouble(rb.total_score, 1) + " / " + FmtDouble(rbMax, 0));
                                    }
                                    const float scPillW = 138.0f * dpiScale;
                                    Views::DrawPill(g, ix + iw - scPillW, iy + 2.0f * dpiScale, scTxt, dpiScale,
                                                    rb.is_graded ? Views::SoftTint(scCol, dark) : pal.sidebarBg, scCol, hc, 142.0f);
                                    g.DrawString(Utf8ToWide(rb.title.empty() ? ("Rúbrica " + rb.rubric_id) : rb.title).c_str(),
                                                 -1, &fSec,
                                                 Gdiplus::RectF(ix, iy, iw - scPillW - 10.0f * dpiScale, 17.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    g.DrawString(Utf8ToWide(rbSub).c_str(), -1, &fSmall,
                                                 Gdiplus::RectF(ix, iy + 18.0f * dpiScale, iw - scPillW - 10.0f * dpiScale, 15.0f * dpiScale),
                                                 &sfTrim, &brTert);
                                    iy += 38.0f * dpiScale;

                                    // Scale Column Header Row
                                    const float hdrH = 30.0f * dpiScale;
                                    Gdiplus::Pen gridPen(UiGlass::ToGdip(pal.borderSubtle), 1.0f);
                                    Gdiplus::SolidBrush brHdrBg(UiGlass::ToGdip(pal.sidebarBg));
                                    g.FillRectangle(&brHdrBg, ix, iy, iw, hdrH);
                                    g.DrawRectangle(&gridPen, ix, iy, iw, hdrH);
                                    g.DrawString(L"Categoría / Peso", -1, &fRubBold,
                                                 Gdiplus::RectF(ix + cellPadX, iy + 7.0f * dpiScale, catColW - cellPadX * 2.0f, hdrH - 8.0f * dpiScale),
                                                 &sfTrim, &brPrim);
                                    const auto& hdrLevels = rb.criteria.front().levels;
                                    for (size_t li = 0; li < numScaleCols; ++li) {
                                        const float lx = ix + catColW + static_cast<float>(li) * lvlColW;
                                        g.DrawLine(&gridPen, lx, iy, lx, iy + hdrH);
                                        std::string lTitle = (li < hdrLevels.size()) ? hdrLevels[li].label : ("Nivel " + std::to_string(li + 1));
                                        std::string lPts = (li < hdrLevels.size()) ? (FmtDouble(hdrLevels[li].points, 0) + " pts") : "";
                                        g.DrawString(Utf8ToWide(lTitle).c_str(), -1, &fRubBold,
                                                     Gdiplus::RectF(lx + cellPadX, iy + 2.0f * dpiScale, lvlColW - cellPadX * 2.0f, 14.0f * dpiScale),
                                                     &sfTrim, &brPrim);
                                        if (!lPts.empty()) {
                                            g.DrawString(Utf8ToWide(lPts).c_str(), -1, &fRubCell,
                                                         Gdiplus::RectF(lx + cellPadX, iy + 15.0f * dpiScale, lvlColW - cellPadX * 2.0f, 13.0f * dpiScale),
                                                         &sfTrim, &brTert);
                                        }
                                    }
                                    iy += hdrH;

                                    // Criterion Matrix Rows + Achieved Highlight + Inline Teacher Comment Callout
                                    Gdiplus::SolidBrush brSelFill(UiGlass::ToGdip(Views::SoftTint(pal.accentBlue, dark)));
                                    Gdiplus::Pen penSel(UiGlass::ToGdip(pal.accentBlue), 1.6f);
                                    Gdiplus::SolidBrush brSelText(UiGlass::ToGdip(pal.labelPrimary));
                                    Gdiplus::SolidBrush brAccentBlue(UiGlass::ToGdip(pal.accentBlue));
                                    Gdiplus::SolidBrush brCmtBg(UiGlass::ToGdip(Views::SoftTint(pal.accentIndigo, dark)));
                                    Gdiplus::SolidBrush brCmtBar(UiGlass::ToGdip(pal.accentIndigo));

                                    for (size_t ci = 0; ci < rb.criteria.size(); ++ci) {
                                        const auto& cr = rb.criteria[ci];
                                        const float rowH = critRowHeights[ci];
                                        if (ci % 2 == 1) {
                                            Gdiplus::SolidBrush brAlt(UiGlass::ToGdip(pal.contentCardElevated));
                                            g.FillRectangle(&brAlt, ix, iy, iw, rowH);
                                        }
                                        g.DrawRectangle(&gridPen, ix, iy, iw, rowH);

                                        // Left column: Criterion title + Peso badge
                                        g.DrawString(Utf8ToWide(cr.category_name).c_str(), -1, &fRubBold,
                                                     Gdiplus::RectF(ix + cellPadX, iy + cellPadY,
                                                                    catColW - cellPadX * 2.0f, rowH - 24.0f * dpiScale),
                                                     &sfWrap, &brPrim);
                                        std::string wtLbl;
                                        if (cr.weight_pct > 0.0) {
                                            wtLbl = "Peso " + FmtDouble(cr.weight_pct, 0) + "%";
                                            if (rb.is_graded) wtLbl += " · " + FmtDouble(cr.earned_weight_pct, 1) + "%";
                                        } else {
                                            wtLbl = FmtDouble(cr.earned_points, 1) + " / " + FmtDouble(cr.max_points, 1) + " pts";
                                        }
                                        g.DrawString(Utf8ToWide(wtLbl).c_str(), -1, &fRubCellBold,
                                                     Gdiplus::RectF(ix + cellPadX, iy + rowH - 18.0f * dpiScale,
                                                                    catColW - cellPadX * 2.0f, 15.0f * dpiScale),
                                                     &sfTrim, &brAccentBlue);

                                        // Level descriptor cells
                                        for (size_t li = 0; li < numScaleCols; ++li) {
                                            const float lx = ix + catColW + static_cast<float>(li) * lvlColW;
                                            g.DrawLine(&gridPen, lx, iy, lx, iy + rowH);
                                            if (li >= cr.levels.size()) continue;
                                            const auto& lv = cr.levels[li];
                                            std::string descTxt = lv.description;
                                            if (!cr.comment.empty() && descTxt.size() > cr.comment.size()) {
                                                const size_t cp = descTxt.rfind(cr.comment);
                                                if (cp != std::string::npos && cp + cr.comment.size() == descTxt.size()) {
                                                    descTxt.erase(cp);
                                                    while (!descTxt.empty() && std::isspace(static_cast<unsigned char>(descTxt.back()))) descTxt.pop_back();
                                                }
                                            }
                                            if (lv.achieved) {
                                                g.FillRectangle(&brSelFill, lx + 1.0f, iy + 1.0f, lvlColW - 2.0f, rowH - 2.0f);
                                                g.DrawRectangle(&penSel, lx + 1.0f, iy + 1.0f, lvlColW - 2.0f, rowH - 2.0f);
                                                g.DrawString(L"Logrado", -1, &fRubCellBold,
                                                             Gdiplus::RectF(lx + cellPadX, iy + 3.0f * dpiScale,
                                                                            lvlColW - cellPadX * 2.0f, 13.0f * dpiScale),
                                                             &sfTrim, &brAccentBlue);
                                                g.DrawString(Utf8ToWide(descTxt).c_str(), -1, &fRubCellBold,
                                                             Gdiplus::RectF(lx + cellPadX, iy + 16.0f * dpiScale,
                                                                            lvlColW - cellPadX * 2.0f, rowH - 19.0f * dpiScale),
                                                             &sfWrap, &brSelText);
                                            } else {
                                                g.DrawString(Utf8ToWide(descTxt).c_str(), -1, &fRubCell,
                                                             Gdiplus::RectF(lx + cellPadX, iy + cellPadY,
                                                                            lvlColW - cellPadX * 2.0f, rowH - cellPadY * 2.0f),
                                                             &sfWrap, &brSec);
                                            }
                                        }
                                        iy += rowH;

                                        // Inline per-criterion teacher comment callout
                                        if (!cr.comment.empty() && critCommentHeights[ci] > 0.0f) {
                                            const float cmtH = critCommentHeights[ci];
                                            g.FillRectangle(&brCmtBg, ix, iy, iw, cmtH);
                                            g.FillRectangle(&brCmtBar, ix, iy, 3.0f * dpiScale, cmtH);
                                            g.DrawRectangle(&gridPen, ix, iy, iw, cmtH);
                                            g.DrawString(Utf8ToWide("Comentario: " + StripCopyCommentSuffixDialog(cr.comment)).c_str(), -1, &fRubCell,
                                                         Gdiplus::RectF(ix + 10.0f * dpiScale, iy + 5.0f * dpiScale,
                                                                        iw - 18.0f * dpiScale, cmtH - 8.0f * dpiScale),
                                                         &sfWrap, &brPrim);
                                            iy += cmtH + 4.0f * dpiScale;
                                        }
                                    }
                                }
                                y += inspH + 8.0f * dpiScale;
                                } // end if (hasAnyInspContent)
                            }
                            ++flatIdx;
                        }
                        y += 8.0f * dpiScale;
                    }
                    if (st->autoScrollSelectedEvalOnce && selCardContentY > contentTop) {
                        st->autoScrollSelectedEvalOnce = false;
                        const float evalBottom = y + static_cast<float>(st->courseDetailScrollY) + 16.0f * dpiScale;
                        const int evalMaxScroll = std::max(0, static_cast<int>(evalBottom - static_cast<float>(h)));
                        st->courseDetailScrollY = std::clamp(
                            static_cast<int>(selCardContentY - contentTop - 10.0f * dpiScale),
                            0, evalMaxScroll);
                        InvalidateRect(hwnd, nullptr, FALSE);
                    }
                }
            } else if (st->courseSubTab == 3) { // Actividades, Calendario del Curso y Cátedra, SMS y Foros
                // 1. Calendario propio del curso (+ cátedra vinculada + fechas de entregas)
                const td::CommunityInfo* linkedCatAct = FindLinkedCatedraForSpace(st->snap, course);
                g.DrawString(linkedCatAct ? L"Calendario del curso y cátedra" : L"Calendario del curso",
                             -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                y += 24.0f * dpiScale;
                std::vector<std::pair<td::CalendarEventEntry, bool>> combinedEvs;
                std::set<std::string> seenActKeys;
                auto pushActEv = [&](td::CalendarEventEntry ev, bool fromCat) {
                    if (ev.date_iso.size() < 10 || ev.date_iso[4] != '-' || ev.date_iso[7] != '-') return;
                    const std::string d10 = ev.date_iso.substr(0, 10);
                    const std::string nt = NormalizeCalEventKeyDialog(ev.title);
                    const bool evalLike = (ev.source_kind == "evaluation") ||
                                          (SafeLowerAscii(ev.title).rfind("fecha de entrega para ", 0) == 0);
                    const std::string k = evalLike ? (d10 + "|eval|" + nt) : (d10 + "|" + ev.start_time + "|" + nt);
                    if (seenActKeys.insert(k).second) {
                        combinedEvs.emplace_back(std::move(ev), fromCat);
                    }
                };
                // Push evaluations first so rich rubric/category rows win over bare dotLRN calendar mirrors
                if (auto evRepIt = st->snap.evaluationsByPath.find(course.url); evRepIt != st->snap.evaluationsByPath.end()) {
                    for (const auto& cat : evRepIt->second.categories) {
                        for (const auto& item : cat.items) {
                            if (item.due_date_iso.size() < 10) continue;
                            td::CalendarEventEntry calEv;
                            calEv.date_iso = item.due_date_iso.substr(0, 10);
                            calEv.start_time = (item.due_date_iso.size() >= 16) ? item.due_date_iso.substr(11, 5) : "23:59";
                            calEv.title = "Entrega: " + item.name;
                            calEv.description = cat.name + " · " + item.submission_status;
                            calEv.source_kind = "evaluation";
                            pushActEv(std::move(calEv), false);
                        }
                    }
                }
                if (auto ccIt = st->snap.courseCalendarsByPath.find(course.url); ccIt != st->snap.courseCalendarsByPath.end()) {
                    for (const auto& e : ccIt->second) pushActEv(e, false);
                }
                if (linkedCatAct) {
                    if (auto lcIt = st->snap.courseCalendarsByPath.find(linkedCatAct->url); lcIt != st->snap.courseCalendarsByPath.end()) {
                        for (const auto& e : lcIt->second) pushActEv(e, true);
                    }
                }
                std::stable_sort(combinedEvs.begin(), combinedEvs.end(), [](const auto& a, const auto& b) {
                    return a.first.date_iso > b.first.date_iso;
                });
                if (!combinedEvs.empty()) {
                    for (size_t ci = 0; ci < combinedEvs.size() && ci < 16; ++ci) {
                        const auto& evc = combinedEvs[ci].first;
                        const bool fromCat = combinedEvs[ci].second;
                        const float rh = 44.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                        const UiGlass::Rgba8 pCol = (evc.source_kind == "evaluation") ? pal.accentBlue : (fromCat ? pal.accentIndigo : pal.accentTeal);
                        Views::DrawPill(g, pad + 10.0f * dpiScale, y + 11.0f * dpiScale,
                                        Utf8ToWide(evc.date_iso.empty() ? "Fecha" : evc.date_iso), dpiScale,
                                        Views::SoftTint(pCol, dark), pCol, hc, 90.0f);
                        float rightW = 10.0f * dpiScale;
                        if (fromCat) {
                            Views::DrawPill(g, pad + cw - 82.0f * dpiScale, y + 11.0f * dpiScale, L"Cátedra",
                                            dpiScale, pal.sidebarBg, pal.labelSecondary, hc, 72.0f);
                            rightW += 78.0f * dpiScale;
                        }
                        g.DrawString(Utf8ToWide(evc.title).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 108.0f * dpiScale, y + 6.0f * dpiScale, cw - 114.0f * dpiScale - rightW, 17.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        g.DrawString(Utf8ToWide(evc.start_time.empty() ? evc.description : (evc.start_time + " · " + evc.description)).c_str(),
                                     -1, &fSmall,
                                     Gdiplus::RectF(pad + 108.0f * dpiScale, y + 24.0f * dpiScale, cw - 114.0f * dpiScale - rightW, 15.0f * dpiScale),
                                     &sfTrim, &brTert);
                        y += rh + 6.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin eventos registrados en el calendario individual de este espacio.", -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                    y += 24.0f * dpiScale;
                }
                y += 8.0f * dpiScale;

                // 2. Actividades GAAP / Motor de Juegos (native tda-motor-juegos + course GAAP)
                g.DrawString(L"Actividades interactivas y pruebas en línea (GAAP)", -1, &fSec,
                             Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 150.0f * dpiScale, 18.0f * dpiScale), &sfTrim, &brPrim);
                if (!st->snap.gaapServerUrl.empty()) {
                    const float gw = Views::DrawPill(g, pad + cw - 134.0f * dpiScale, y, L"Abrir motor GAAP", dpiScale,
                                                     Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 130.0f);
                    st->courseDocActionRects.push_back({
                        { static_cast<LONG>(pad + cw - 134.0f * dpiScale), static_cast<LONG>(y),
                          static_cast<LONG>(pad + cw - 134.0f * dpiScale + gw), static_cast<LONG>(y + 22.0f * dpiScale) },
                        "url:" + st->snap.gaapServerUrl });
                }
                y += 26.0f * dpiScale;
                auto gpIt = st->snap.gaapByPath.find(course.url);
                bool anyGaapShown = false;
                if (gpIt != st->snap.gaapByPath.end() && !gpIt->second.empty()) {
                    anyGaapShown = true;
                    for (const auto& ga : gpIt->second) {
                        const float rh = 46.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                        const UiGlass::Rgba8 sCol = (ga.score >= 0.0) ? pal.accentGreen : pal.accentBlue;
                        std::wstring sTxt = (ga.score >= 0.0) ? Utf8ToWide(FmtDouble(ga.score, 1) + " pts") : Utf8ToWide(ga.status.empty() ? "Disponible" : ga.status);
                        Views::DrawPill(g, pad + cw - 92.0f * dpiScale, y + 12.0f * dpiScale, sTxt, dpiScale,
                                        Views::SoftTint(sCol, dark), sCol, hc, 86.0f);
                        g.DrawString(Utf8ToWide(ga.title).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - 112.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        g.DrawString(Utf8ToWide(ga.date_text.empty() ? ga.status : ga.date_text).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 25.0f * dpiScale, cw - 112.0f * dpiScale, 15.0f * dpiScale),
                                     &sfTrim, &brTert);
                        y += rh + 6.0f * dpiScale;
                    }
                }
                if (!st->snap.motorJuegosActivities.empty()) {
                    anyGaapShown = true;
                    for (size_t mi = 0; mi < st->snap.motorJuegosActivities.size() && mi < 12; ++mi) {
                        const auto& ma = st->snap.motorJuegosActivities[mi];
                        const float rh = 46.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                        const UiGlass::Rgba8 sCol = (ma.score >= 0.0) ? pal.accentGreen : pal.accentIndigo;
                        std::wstring sTxt = (ma.score >= 0.0) ? Utf8ToWide(FmtDouble(ma.score, 1) + " pts")
                                                              : Utf8ToWide(ma.status.empty() ? "Motor juegos" : ma.status);
                        Views::DrawPill(g, pad + cw - 96.0f * dpiScale, y + 12.0f * dpiScale, sTxt, dpiScale,
                                        Views::SoftTint(sCol, dark), sCol, hc, 90.0f);
                        g.DrawString(Utf8ToWide(ma.title.empty() ? ("Actividad " + ma.id) : ma.title).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - 116.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        std::string mSub = ma.status.empty() ? "Actividad interactiva" : ma.status;
                        if (!ma.due_date.empty()) mSub += "  ·  " + ma.due_date;
                        g.DrawString(Utf8ToWide(mSub).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 25.0f * dpiScale, cw - 116.0f * dpiScale, 15.0f * dpiScale),
                                     &sfTrim, &brTert);
                        if (!ma.url.empty()) {
                            st->courseDocActionRects.push_back({
                                { static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) },
                                "url:" + ma.url });
                        }
                        y += rh + 6.0f * dpiScale;
                    }
                }
                if (!anyGaapShown) {
                    std::wstring gMsg = !st->snap.gaapServerUrl.empty()
                                            ? L"Motor interactivo GAAP vinculado · sin actividades activas asignadas en este curso."
                                            : L"Sin actividades GAAP asignadas en este curso.";
                    g.DrawString(gMsg.c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                    y += 24.0f * dpiScale;
                }
                y += 8.0f * dpiScale;

                // 3. Mensajes SMS del curso
                g.DrawString(L"Mensajes SMS enviados en el curso", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                y += 24.0f * dpiScale;
                auto smsIt = st->snap.smsByPath.find(course.url);
                if (smsIt != st->snap.smsByPath.end() && !smsIt->second.empty()) {
                    for (size_t si = 0; si < smsIt->second.size() && si < 12; ++si) {
                        const auto& sm = smsIt->second[si];
                        const float rh = 44.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                        g.DrawString(Utf8ToWide(sm.content).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 6.0f * dpiScale, cw - 24.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        std::string smSub = sm.transaction_date + (sm.sent_by.empty() ? "" : ("  ·  " + sm.sent_by));
                        g.DrawString(Utf8ToWide(smSub).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 24.0f * dpiScale, cw - 24.0f * dpiScale, 15.0f * dpiScale),
                                     &sfTrim, &brTert);
                        y += rh + 6.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin mensajes SMS registrados para este curso.", -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                    y += 24.0f * dpiScale;
                }
                y += 8.0f * dpiScale;

                // 4. Foros de discusión
                g.DrawString(L"Foros de discusión", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                y += 24.0f * dpiScale;
                auto frIt = st->snap.forumsByPath.find(course.url);
                if (frIt != st->snap.forumsByPath.end() && !frIt->second.empty()) {
                    for (const auto& th : frIt->second) {
                        const float rh = 36.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                        g.DrawString(Utf8ToWide(th.subject).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 9.0f * dpiScale, cw - 24.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        y += rh + 6.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin hilos de foro activos en este espacio.", -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                    y += 24.0f * dpiScale;
                }
            } else { // Novedades (RSS + Archivo histórico) y Personas
                const td::CommunityInfo* linkedCatNews = FindLinkedCatedraForSpace(st->snap, course);
                g.DrawString(L"Comunicados y archivo de noticias (clic para leer)", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brPrim);
                y += 24.0f * dpiScale;
                std::vector<td::RssNewsItem> mergedNews;
                std::set<std::string> seenNews;
                auto appendNewsFrom = [&](const std::string& u, const std::string& prefix) {
                    if (auto nrIt = st->snap.newsByPath.find(u); nrIt != st->snap.newsByPath.end()) {
                        for (auto n : nrIt->second) {
                            if (seenNews.insert(SafeLowerAscii(n.title)).second) {
                                if (!prefix.empty()) n.title = prefix + n.title;
                                mergedNews.push_back(std::move(n));
                            }
                        }
                    }
                    if (auto naIt = st->snap.newsArchiveByPath.find(u); naIt != st->snap.newsArchiveByPath.end()) {
                        for (auto n : naIt->second) {
                            if (seenNews.insert(SafeLowerAscii(n.title)).second) {
                                if (!prefix.empty()) n.title = prefix + n.title;
                                mergedNews.push_back(std::move(n));
                            }
                        }
                    }
                };
                appendNewsFrom(course.url, "");
                if (linkedCatNews) appendNewsFrom(linkedCatNews->url, "[Cátedra] ");
                if (!mergedNews.empty()) {
                    for (size_t ni = 0; ni < mergedNews.size() && ni < 24; ++ni) {
                        const auto& n = mergedNews[ni];
                        const float rh = 48.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                        g.DrawString(Utf8ToWide(n.title).c_str(), -1, &fSec,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - 24.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        const std::string cleanDesc = CleanHtmlSnippetDialog(n.description);
                        const std::string cleanDate = FormatNewsDateDialog(n.pub_date);
                        std::string sub = cleanDate + (cleanDesc.empty() ? "" : ("  ·  " + cleanDesc));
                        g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 26.0f * dpiScale, cw - 24.0f * dpiScale, 16.0f * dpiScale),
                                     &sfTrim, &brSec);
                        st->courseNewsRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y),
                                                          static_cast<LONG>(pad + cw), static_cast<LONG>(y + rh) }, n.link });
                        y += rh + 6.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin comunicados ni noticias publicadas.", -1, &fSmall, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                    y += 24.0f * dpiScale;
                }
                y += 10.0f * dpiScale;
                g.DrawString(L"Equipo docente e integrantes", -1, &fSec, Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 130.0f * dpiScale, 18.0f * dpiScale), &sfTrim, &brPrim);
                {
                    const float csvW = Views::DrawPill(g, pad + cw - 118.0f * dpiScale, y, L"Exportar CSV", dpiScale,
                                                       Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 114.0f);
                    st->courseNewsRects.push_back({ { static_cast<LONG>(pad + cw - 118.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(pad + cw - 118.0f * dpiScale + csvW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                                    "export_members_csv" });
                }
                y += 24.0f * dpiScale;
                auto mbIt = st->snap.membersByPath.find(course.url);
                if (mbIt != st->snap.membersByPath.end() && !mbIt->second.empty()) {
                    int shownM = 0;
                    for (const auto& m : mbIt->second) {
                        const std::string nm = !m.full_name.empty() ? m.full_name : (m.first_name + " " + m.last_name);
                        const std::string nmLow = SafeLowerAscii(nm);
                        const std::string emLow = SafeLowerAscii(m.email);
                        const std::string rlLow = SafeLowerAscii(m.role);
                        // Skip scraped HTML table header rows ("Nombre" / "Apellidos" / "Correo")
                        if (rlLow == "nombre" || rlLow == "rol" || emLow == "apellidos" || emLow == "correo" ||
                            nmLow == "nombre" || nmLow == "apellidos" || nmLow == "nombre apellidos" ||
                            (nmLow.find_first_not_of(" \t\r\n") == std::string::npos && rlLow == "nombre")) {
                            continue;
                        }
                        if (shownM++ >= 40) break;
                        const float rh = m.email.empty() ? (34.0f * dpiScale) : (46.0f * dpiScale);
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                        g.DrawString(Utf8ToWide(nm).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - 110.0f * dpiScale, 18.0f * dpiScale),
                                     &sfTrim, &brPrim);
                        if (!m.email.empty()) {
                            g.DrawString(Utf8ToWide(m.email).c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pad + 12.0f * dpiScale, y + 25.0f * dpiScale, cw - 110.0f * dpiScale, 15.0f * dpiScale),
                                         &sfTrim, &brTert);
                        }
                        if (!m.role.empty()) {
                            const bool isDoc = rlLow.find("doc") != std::string::npos || rlLow.find("prof") != std::string::npos;
                            Views::DrawPill(g, pad + cw - 92.0f * dpiScale, y + 7.0f * dpiScale, Utf8ToWide(m.role), dpiScale,
                                            isDoc ? Views::SoftTint(pal.accentBlue, dark) : pal.sidebarBg,
                                            isDoc ? pal.accentBlue : pal.labelSecondary, hc, 86.0f);
                        }
                        y += rh + 5.0f * dpiScale;
                    }
                    if (shownM == 0) {
                        g.DrawString(L"Sin integrantes adicionales publicados para este espacio.", -1, &fSmall,
                                     Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                        y += 24.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin lista de integrantes sincronizada.", -1, &fSmall, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sfTrim, &brTert);
                    y += 24.0f * dpiScale;
                }
            }

            const float contentBottom = y + static_cast<float>(st->courseDetailScrollY) + 16.0f * dpiScale;
            st->courseDetailMaxScroll = std::max(0, static_cast<int>(contentBottom - static_cast<float>(h)));
            st->courseDetailScrollY = std::clamp(st->courseDetailScrollY, 0, st->courseDetailMaxScroll);
        }

        g.ResetClip();
        Views::DrawScrollbarThumb(g, w, h, st->courseDetailScrollY, st->courseDetailMaxScroll, dpiScale, pal, contentTop);
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK CourseDetailCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) { PaintCourseDetailCanvas(hwnd, st); return 0; }
    if (msg == WM_MOUSEWHEEL && st) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->courseDetailScrollY = std::clamp(st->courseDetailScrollY - (delta / 2), 0, st->courseDetailMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        int hSeg = -1;
        for (const auto& sh : st->courseSegHits) {
            if (PtInRect(&sh.rc, pt)) { hSeg = sh.index; break; }
        }
        int hFile = -1;
        if (st->courseSubTab == 1) {
            for (size_t i = 0; i < st->courseFileRects.size(); ++i) {
                if (PtInRect(&st->courseFileRects[i], pt)) { hFile = static_cast<int>(i); break; }
            }
        }
        if (hSeg != st->courseDetailHoverSeg || hFile != st->courseFileHoverIdx) {
            st->courseDetailHoverSeg = hSeg;
            st->courseFileHoverIdx = hFile;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        for (const auto& sh : st->courseSegHits) {
            if (PtInRect(&sh.rc, pt)) {
                st->courseSubTab = sh.index;
                st->courseDetailScrollY = 0;
                if (st->hCmbCourseSub) SendMessageW(st->hCmbCourseSub, CB_SETCURSEL, sh.index, 0);
                RefreshSelectedCourseDetail(st);
                LayoutDialog(st);
                InvalidateRect(st->hwnd, nullptr, FALSE);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        // Universal action pills inside CourseDetailCanvas (details/versions, folder alerts, infographics, cátedra news, GAAP URL)
        for (const auto& [rc, act] : st->courseDocActionRects) {
            if (!PtInRect(&rc, pt)) continue;
            if (act == "goto_news:") {
                st->courseSubTab = 4;
                st->courseDetailScrollY = 0;
                RefreshSelectedCourseDetail(st);
                LayoutDialog(st);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (act.rfind("url:", 0) == 0) {
                OpenSafeUrl(st->hwnd, act.substr(4));
                return 0;
            }
            if (act.rfind("news_read:", 0) == 0) {
                const std::string nurl = act.substr(10);
                if (!nurl.empty() && st->store && st->store->IsAuthenticated()) {
                    auto art = st->store->FetchNewsArticle(nurl);
                    if (art.ok) {
                        std::wstring txt = Utf8ToWide(art.value.title +
                            (art.value.pub_date.empty() ? "" : ("\nFecha: " + art.value.pub_date)) +
                            "\n\n" + art.value.body_text);
                        MessageBoxW(st->hwnd, txt.c_str(), L"Comunicado de Cátedra", MB_OK);
                        return 0;
                    }
                }
                if (!nurl.empty()) OpenSafeUrl(st->hwnd, nurl);
                return 0;
            }
            if (act.rfind("img_open:", 0) == 0) {
                const std::string p = act.substr(9);
                if (!p.empty()) ShellExecuteW(st->hwnd, L"open", Utf8ToWide(p).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                return 0;
            }
            if (act.rfind("img_fetch:", 0) == 0 && st->store) {
                const std::string u = act.substr(10);
                SetWindowTextW(st->hProgressBanner, L"Descargando infografía semanal...");
                const std::string local = st->store->EnsureMediaImageCached(u);
                if (!local.empty()) {
                    SetWindowTextW(st->hProgressBanner, L"Infografía semanal cargada en caché local.");
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else {
                    OpenSafeUrl(st->hwnd, u);
                }
                return 0;
            }
            if (act.rfind("alert_folder:", 0) == 0 && st->store) {
                const std::string payload = act.substr(13);
                const size_t sep = payload.find('|');
                const std::string fid = (sep == std::string::npos) ? payload : payload.substr(0, sep);
                const bool subOn = (sep != std::string::npos && payload.substr(sep + 1) == "1");
                auto r = st->store->ToggleFolderNotificationSubscription(fid, subOn);
                if (r.ok && r.value) {
                    st->snap = st->store->Snapshot();
                    SetWindowTextW(st->hProgressBanner, subOn ? L"Alertas de carpeta activadas en TEC Digital." : L"Alertas de carpeta desactivadas.");
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else {
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide("Alertas: " + r.error_message).c_str());
                }
                return 0;
            }
            if (act.rfind("ver:", 0) == 0 && st->store) {
                const std::string payload = act.substr(4);
                const size_t sep = payload.find('|');
                const std::string oid = (sep == std::string::npos) ? payload : payload.substr(0, sep);
                const std::string fname = (sep == std::string::npos) ? oid : payload.substr(sep + 1);
                std::ostringstream oss;
                oss << "Archivo: " << fname << "\n";
                auto det = st->store->FetchFileDetails(oid);
                if (det.ok) {
                    if (!det.value.last_modified_ansi.empty()) oss << "Última modificación: " << FormatFriendlyTimestampDialog(det.value.last_modified_ansi) << "\n";
                    if (!det.value.author_name.empty()) oss << "Autor: " << det.value.author_name << "\n";
                    if (!det.value.mime_type.empty()) oss << "Tipo: " << det.value.mime_type << "\n";
                    if (!det.value.live_revision.empty()) oss << "Revisión vigente: " << det.value.live_revision << "\n";
                    if (det.value.content_size > 0) oss << "Tamaño: " << std::max<std::uint64_t>(1, det.value.content_size / 1024) << " KB\n";
                }
                auto vr = st->store->FetchFileVersions(oid);
                if (vr.ok && !vr.value.empty()) {
                    st->snap = st->store->Snapshot();
                    oss << "\nHistorial de versiones (" << vr.value.size() << "):\n";
                    for (size_t vi = 0; vi < vr.value.size() && vi < 12; ++vi) {
                        const auto& v = vr.value[vi];
                        oss << "• Rev. " << (v.live_revision.empty() ? std::to_string(vi + 1) : v.live_revision);
                        if (!v.last_modified_ansi.empty()) oss << " · " << FormatFriendlyTimestampDialog(v.last_modified_ansi);
                        if (v.content_size > 0) oss << " · " << std::max<std::uint64_t>(1, v.content_size / 1024) << " KB";
                        if (!v.author.empty()) oss << " · " << v.author;
                        oss << "\n";
                    }
                } else {
                    // Fallback to locally stored snapshot metadata
                    for (const auto& f : st->currentCourseFiles) {
                        if (f.object_id == oid) {
                            if (!det.ok) {
                                if (!f.creation_date_ansi.empty()) oss << "Creación: " << FormatFriendlyTimestampDialog(f.creation_date_ansi) << "\n";
                                if (!f.last_modified_ansi.empty()) oss << "Última modificación: " << FormatFriendlyTimestampDialog(f.last_modified_ansi) << "\n";
                                if (!f.author_name.empty()) oss << "Autor: " << f.author_name << "\n";
                                if (!f.live_revision.empty()) oss << "Revisión actual: " << f.live_revision << "\n";
                                oss << "Tamaño: " << std::max<std::uint64_t>(1, f.content_size / 1024) << " KB\n";
                            }
                            break;
                        }
                    }
                }
                MessageBoxW(st->hwnd, Utf8ToWide(oss.str()).c_str(), L"Detalles y versiones del archivo", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
        }
        if (st->courseSubTab == 0) {
            for (const auto& [rc, pnum] : st->courseCustomTabRects) {
                if (PtInRect(&rc, pt)) {
                    st->selectedCustomPageNum = pnum;
                    st->courseDetailScrollY = 0;
                    // On-demand fetch if user clicks an uncached custom page on a live session
                    if (pnum >= 0 && st->store && st->store->IsAuthenticated() &&
                        st->selectedCourseIdx >= 0 && static_cast<size_t>(st->selectedCourseIdx) < st->filteredCourses.size()) {
                        const auto& course = st->filteredCourses[static_cast<size_t>(st->selectedCourseIdx)];
                        bool cached = false;
                        if (auto it = st->snap.customPagesBySpace.find(course.url); it != st->snap.customPagesBySpace.end()) {
                            for (const auto& pg : it->second) if (pg.page_num == pnum) { cached = true; break; }
                        }
                        if (!cached) {
                            std::string tabLbl;
                            if (auto oit = st->snap.overviewsByPath.find(course.url); oit != st->snap.overviewsByPath.end()) {
                                for (const auto& t : oit->second.tabs) if (t.page_num == pnum) { tabLbl = t.label; break; }
                            }
                            auto pr = st->store->FetchSpacePage(course.url, pnum, tabLbl);
                            if (pr.ok) st->snap = st->store->Snapshot();
                        }
                    }
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (const auto& [rc, u] : st->courseBannerLinkRects) {
                if (PtInRect(&rc, pt) && !u.empty()) {
                    if (u.rfind("file_open:", 0) == 0 && st->store) {
                        const std::string payload = u.substr(10);
                        const size_t sep = payload.find('|');
                        if (sep != std::string::npos) {
                            const std::string spUrl = payload.substr(0, sep);
                            const std::string objId = payload.substr(sep + 1);
                            std::string localPath, err;
                            if (st->store->DownloadSingleCourseFile(spUrl, objId, &localPath, &err) && !localPath.empty()) {
                                ShellExecuteW(st->hwnd, L"open", Utf8ToWide(localPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                            } else {
                                OpenSafeUrl(st->hwnd, spUrl + "/file-storage/");
                            }
                        }
                        return 0;
                    }
                    OpenSafeUrl(st->hwnd, u);
                    return 0;
                }
            }
        }
        if (st->courseSubTab == 1) {
            for (const auto& [rc, fid] : st->courseBreadcrumbRects) {
                if (PtInRect(&rc, pt)) {
                    st->currentCourseFolderId = fid;
                    st->courseDetailScrollY = 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (const auto& [rc, fid] : st->courseFolderRects) {
                if (PtInRect(&rc, pt)) {
                    st->currentCourseFolderId = fid;
                    st->courseDetailScrollY = 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (size_t i = 0; i < st->courseFileRects.size(); ++i) {
                if (PtInRect(&st->courseFileRects[i], pt)) {
                    st->selectedCourseFileIdx = static_cast<int>(i);
                    if (st->hLstCourseFiles) SendMessageW(st->hLstCourseFiles, LB_SETCURSEL, i, 0);
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (const auto& [rc, u] : st->courseUrlLinkRects) {
                if (PtInRect(&rc, pt) && !u.empty()) {
                    OpenSafeUrl(st->hwnd, u);
                    return 0;
                }
            }
        }
        if (st->courseSubTab == 2) {
            for (const auto& [rc, tag] : st->courseEvalActionRects) {
                if (!PtInRect(&rc, pt)) continue;
                if (tag == "sync_course") {
                    if (st->selectedCourseIdx >= 0 && static_cast<size_t>(st->selectedCourseIdx) < st->filteredCourses.size()) {
                        const auto& c = st->filteredCourses[static_cast<size_t>(st->selectedCourseIdx)];
                        TriggerOnDemandCourseSync(st, c.url, c.name);
                    }
                    return 0;
                }
                if (tag == "whatif_clear") {
                    st->whatIfOverridesByAssignId.clear();
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (tag.rfind("url:", 0) == 0) {
                    OpenSafeUrl(st->hwnd, tag.substr(4));
                    return 0;
                }
                if (tag.rfind("sim70:", 0) == 0) {
                    st->whatIfOverridesByAssignId[tag.substr(6)] = 70.0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (tag.rfind("sim85:", 0) == 0) {
                    st->whatIfOverridesByAssignId[tag.substr(6)] = 85.0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (tag.rfind("sim100:", 0) == 0) {
                    st->whatIfOverridesByAssignId[tag.substr(7)] = 100.0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
                if (tag.rfind("rubric:", 0) == 0) {
                    SendMessageW(st->hwnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_COURSE_RUBRIC, BN_CLICKED), 0);
                    return 0;
                }
                if (tag.rfind("eval_alert:", 0) == 0 && st->store &&
                    st->selectedCourseIdx >= 0 && static_cast<size_t>(st->selectedCourseIdx) < st->filteredCourses.size()) {
                    const auto& c = st->filteredCourses[static_cast<size_t>(st->selectedCourseIdx)];
                    auto r = st->store->RequestEvaluationNotifications(c.url, tag.substr(11), true);
                    SetWindowTextW(st->hProgressBanner,
                                   r.ok ? L"Alertas de calificación activadas para esta evaluación."
                                        : Utf8ToWide("Alertas evaluación: " + r.error_message).c_str());
                    return 0;
                }
            }
            for (const auto& [rc, eidx] : st->courseEvalCardRects) {
                if (PtInRect(&rc, pt)) {
                    st->selectedEvalFlatIdx = eidx;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
        }
        if (st->courseSubTab == 4) {
            for (const auto& [rc, nurl] : st->courseNewsRects) {
                if (PtInRect(&rc, pt)) {
                    if (nurl == "export_members_csv") {
                        if (st->store && st->selectedCourseIdx >= 0 &&
                            static_cast<size_t>(st->selectedCourseIdx) < st->filteredCourses.size()) {
                            const auto& c = st->filteredCourses[static_cast<size_t>(st->selectedCourseIdx)];
                            auto csvRes = st->store->FetchCommunityMembersCsv(c.url);
                            if (csvRes.ok && !csvRes.value.empty()) {
                                wchar_t pathBuf[MAX_PATH] = L"integrantes_curso.csv";
                                OPENFILENAMEW ofn{};
                                ofn.lStructSize = sizeof(ofn);
                                ofn.hwndOwner = st->hwnd;
                                ofn.lpstrFilter = L"Archivo CSV (*.csv)\0*.csv\0Todos los archivos (*.*)\0*.*\0";
                                ofn.lpstrFile = pathBuf;
                                ofn.nMaxFile = MAX_PATH;
                                ofn.lpstrDefExt = L"csv";
                                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
                                if (GetSaveFileNameW(&ofn)) {
                                    std::ofstream ofs(std::wstring(pathBuf), std::ios::binary | std::ios::trunc);
                                    if (ofs.is_open()) {
                                        ofs.write(csvRes.value.data(), static_cast<std::streamsize>(csvRes.value.size()));
                                        SetWindowTextW(st->hProgressBanner, L"Lista de integrantes exportada en CSV.");
                                    }
                                }
                            } else {
                                SetWindowTextW(st->hProgressBanner, L"No se pudo descargar el CSV de integrantes para este espacio.");
                            }
                        }
                        return 0;
                    }
                    if (!nurl.empty() && st->store && st->store->IsAuthenticated()) {
                        auto art = st->store->FetchNewsArticle(nurl);
                        if (art.ok) {
                            std::wstring txt = Utf8ToWide(art.value.title +
                                (art.value.pub_date.empty() ? "" : ("\nFecha: " + art.value.pub_date)) +
                                "\n\n" + art.value.body_text);
                            MessageBoxW(st->hwnd, txt.c_str(), L"Noticia de TEC Digital", MB_OK);
                            return 0;
                        }
                    }
                    if (!nurl.empty()) OpenSafeUrl(st->hwnd, nurl);
                    return 0;
                }
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintAgendaCanvas(HWND hwnd, TecHubState* st) {
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

        Views::DrawSurface(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                           10.0f * dpiScale, pal.contentCard,
                           hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);

        int filterIdx = st->hCmbCalFilter ? static_cast<int>(SendMessageW(st->hCmbCalFilter, CB_GETCURSEL, 0, 0)) : 0;
        const auto prioritizedAll = GetPrioritizedCalendarEvents(st->snap.unifiedCalendarEvents);
        std::vector<const td::CalendarEventEntry*> items;
        for (const auto* ev : prioritizedAll) {
            if (filterIdx == 1 && ev->source_kind != "evaluation") continue;
            if (filterIdx == 2 && ev->source_kind != "matricula") continue;
            if (filterIdx == 3 && ev->source_kind != "calendar") continue;
            items.push_back(ev);
        }

        if (items.empty()) {
            DrawEmptyStateCentered(g, w, h, dpiScale, pal, hc, UiGlass::IconId::CalendarGrid,
                                   L"Agenda libre",
                                   L"No hay entregas, citas ni eventos para el filtro seleccionado.");
            st->agendaMaxScroll = 0;
        } else {
            const float pad = 16.0f * dpiScale;
            const float cw = std::max(100.0f, static_cast<float>(w) - pad * 2.0f - 8.0f * dpiScale);
            const float rowH = 56.0f * dpiScale;
            const float gap = 8.0f * dpiScale;
            st->agendaMaxScroll = std::max(0, static_cast<int>(pad * 2.0f + items.size() * (rowH + gap)) - h);
            st->agendaScrollY = std::clamp(st->agendaScrollY, 0, st->agendaMaxScroll);

            g.SetClip(Gdiplus::RectF(4.0f, 4.0f, static_cast<float>(w) - 8.0f, static_cast<float>(h) - 8.0f));
            Gdiplus::FontFamily ff(L"Segoe UI");
            Gdiplus::Font fTitle(&ff, 12.6f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fSub(&ff, 11.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
            Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
            Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
            Gdiplus::StringFormat sf;
            sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

            float y = pad - static_cast<float>(st->agendaScrollY);
            for (const auto* ev : items) {
                if (y + rowH >= 0.0f && y <= static_cast<float>(h)) {
                    const UiGlass::Rgba8 col = (ev->source_kind == "matricula")
                                                   ? pal.accentOrange
                                                   : ((ev->source_kind == "evaluation") ? pal.accentBlue : pal.accentTeal);
                    const std::wstring kindLbl = (ev->source_kind == "matricula")
                                                     ? L"Matrícula"
                                                     : ((ev->source_kind == "evaluation") ? L"Entrega" : L"Evento");
                    Views::DrawSurface(g, pad, y, cw, rowH, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                    Gdiplus::SolidBrush sb(UiGlass::ToGdip(col));
                    g.FillRectangle(&sb, pad + 3.0f, y + 9.0f * dpiScale, 3.5f * dpiScale, rowH - 18.0f * dpiScale);

                    // Date badge on left
                    const float dateW = 92.0f * dpiScale;
                    g.DrawString(Utf8ToWide(ev->date_iso).c_str(), -1, &fTitle,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 10.0f * dpiScale, dateW, 18.0f * dpiScale), &sf, &brPrim);
                    g.DrawString(Utf8ToWide(ev->start_time.empty() ? "Todo el día" : ev->start_time).c_str(), -1, &fSub,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 29.0f * dpiScale, dateW, 16.0f * dpiScale), &sf, &brTert);

                    std::string cd = FormatCountdownToIso(ev->date_iso, st->snap.serverTimeOffsetSec);
                    const float rightPillW = 84.0f * dpiScale;
                    Views::DrawPill(g, pad + cw - rightPillW - 10.0f * dpiScale, y + 17.0f * dpiScale,
                                    cd.empty() ? kindLbl : Utf8ToWide(cd), dpiScale,
                                    Views::SoftTint(col, dark), col, hc, 88.0f);

                    const float tx = pad + dateW + 24.0f * dpiScale;
                    const float tw = std::max(60.0f, cw - dateW - rightPillW - 44.0f * dpiScale);
                    g.DrawString(Utf8ToWide(ev->title).c_str(), -1, &fTitle,
                                 Gdiplus::RectF(tx, y + 10.0f * dpiScale, tw, 18.0f * dpiScale), &sf, &brPrim);
                    std::string sub = ev->calendar_name;
                    if (!ev->description.empty()) sub += (sub.empty() ? "" : "  ·  ") + ev->description;
                    g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSub,
                                 Gdiplus::RectF(tx, y + 29.0f * dpiScale, tw, 16.0f * dpiScale), &sf, &brSec);
                }
                y += rowH + gap;
            }
            g.ResetClip();
            Views::DrawScrollbarThumb(g, w, h, st->agendaScrollY, st->agendaMaxScroll, dpiScale, pal);
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK AgendaCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) { PaintAgendaCanvas(hwnd, st); return 0; }
    if (msg == WM_MOUSEWHEEL && st) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->agendaScrollY = std::clamp(st->agendaScrollY - (delta / 2), 0, st->agendaMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintExpStatsCanvas(HWND hwnd, TecHubState* st) {
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

    st->expGradeFilterHits.clear();
    st->expSubTabHits.clear();
    st->rcExpToggleAttempts = {};
    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        if (!st->snap.expediente && !st->snap.studentGeneralInfo && !st->snap.studentProfile) {
            Views::DrawSurface(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                               10.0f * dpiScale, pal.contentCard, pal.borderSubtle, 1.0f);
            DrawEmptyStateCentered(g, w, h, dpiScale, pal, hc, UiGlass::IconId::AcademicCap,
                                   L"Expediente académico",
                                   L"Inicia sesión y sincroniza para ver tu promedio ponderado, evolución semestral, historial de notas y citas de matrícula.");
            st->expStatsMaxScroll = 0;
        } else {
            const float gap = 10.0f * dpiScale;
            const float kpiH = 74.0f * dpiScale;
            const float kpiW = std::max(100.0f, (static_cast<float>(w) - gap * 3.0f) / 4.0f);

            td::AcademicStatsSummary s{};
            std::vector<td::GradeRecordRow> effectiveGrades;
            ComputeEffectiveExpedienteData(st->snap, s, effectiveGrades);
            const double avg = (st->snap.studentGeneralInfo && st->snap.studentGeneralInfo->weighted_average > 0.0)
                                   ? st->snap.studentGeneralInfo->weighted_average
                                   : s.weighted_avg_general;
            const int availCr = st->snap.studentGeneralInfo ? st->snap.studentGeneralInfo->available_registration_credits : 0;
            const int rnCount = st->snap.studentGeneralInfo ? static_cast<int>(st->snap.studentGeneralInfo->rn_courses.size()) : 0;
            const int pendCount = (st->snap.studentGeneralInfo && st->snap.studentGeneralInfo->outstanding_courses > 0)
                                      ? st->snap.studentGeneralInfo->outstanding_courses
                                      : (!st->snap.pendingGraduationCourses.empty()
                                             ? static_cast<int>(st->snap.pendingGraduationCourses.size())
                                             : s.count_pendientes);

            Views::DrawKpiTile(g, 0.0f, 0.0f, kpiW, kpiH, L"Promedio ponderado",
                               avg > 0.0 ? Utf8ToWide(FmtDouble(avg, 2)) : L"--",
                               s.weighted_avg_passed > 0.0 ? (L"Aprobados: " + Utf8ToWide(FmtDouble(s.weighted_avg_passed, 1))) : L"General acumulado",
                               UiGlass::IconId::AcademicCap, pal.accentBlue, dpiScale, pal, dark, hc);
            Views::DrawKpiTile(g, (kpiW + gap) * 1.0f, 0.0f, kpiW, kpiH, L"Cursos aprobados",
                               std::to_wstring(s.count_aprobadas),
                               std::to_wstring(s.count_en_curso) + L" en curso · " + std::to_wstring(s.count_reprobadas) + L" reprob.",
                               UiGlass::IconId::Checklist, pal.accentGreen, dpiScale, pal, dark, hc);
            Views::DrawKpiTile(g, (kpiW + gap) * 2.0f, 0.0f, kpiW, kpiH, L"Créditos",
                               std::to_wstring(s.credits_passed) + (s.credits_total > 0 ? (L"/" + std::to_wstring(s.credits_total)) : L" cr"),
                               availCr > 0 ? (std::to_wstring(availCr) + L" cr matrícula disp.") : (s.hours_taken > 0 ? (std::to_wstring(s.hours_taken) + L" horas cursadas") : L"Créditos cursados"),
                               UiGlass::IconId::LayersStack, pal.accentIndigo, dpiScale, pal, dark, hc);
            Views::DrawKpiTile(g, (kpiW + gap) * 3.0f, 0.0f, kpiW, kpiH, rnCount > 0 ? L"Alertas RN" : L"Pendientes",
                               rnCount > 0 ? std::to_wstring(rnCount) : std::to_wstring(pendCount),
                               rnCount > 0 ? (std::to_wstring(pendCount) + L" pend. graduación") : L"Cursos para graduación",
                               UiGlass::IconId::ClockHistory, rnCount > 0 ? pal.accentOrange : pal.accentTeal,
                               dpiScale, pal, dark, hc);

            // Sub-view segmented bar
            const float segY = kpiH + gap;
            const float segH = 30.0f * dpiScale;
            static const std::vector<std::wstring> kExpSubTabs = {
                L"Historial y Promedio",
                L"Pendientes y Citas",
                L"8 Estados e Información"
            };
            Views::DrawSegmentedBar(g, 0.0f, segY, std::min(520.0f * dpiScale, static_cast<float>(w) - 2.0f), segH,
                                    kExpSubTabs, st->expSubTab, st->expSubTabHover, dpiScale, pal, hc, st->expSubTabHits);

            // Main container card below segmented bar
            const float tblY = segY + segH + gap;
            const float tblH = std::max(120.0f, static_cast<float>(h) - tblY - 2.0f);
            Views::DrawSurface(g, 0.0f, tblY, static_cast<float>(w) - 1.5f, tblH, 10.0f * dpiScale,
                               pal.contentCard, hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);

            const float pad = 14.0f * dpiScale;
            const float cw = std::max(120.0f, static_cast<float>(w) - pad * 2.0f - 10.0f * dpiScale);
            Gdiplus::FontFamily ff(L"Segoe UI");
            Gdiplus::Font fSec(&ff, 13.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fRow(&ff, 12.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::Font fBold(&ff, 12.2f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
            Gdiplus::Font fSmall(&ff, 10.8f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
            Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
            Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
            Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
            Gdiplus::StringFormat sf;
            sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 0.9f);

            g.SetClip(Gdiplus::RectF(4.0f, tblY + 4.0f, static_cast<float>(w) - 8.0f, tblH - 8.0f));
            float y = tblY + pad - static_cast<float>(st->expStatsScrollY);

            if (st->expSubTab == 0) {
                // 1. Vector Sparkline / Chart of Weighted Average by Semester (chronological oldest -> newest)
                if (st->snap.expediente && !st->snap.expediente->structured.weighted_averages.empty()) {
                    auto wavgs = st->snap.expediente->structured.weighted_averages;
                    auto periodRank = [](const auto& r) -> int {
                        const std::string m = SafeLowerAscii(r.modality);
                        const int p = std::atoi(r.period.c_str());
                        if (m.find("centro") != std::string::npos) return p;
                        if (m.find("verano") != std::string::npos) return 90 + p;
                        return 10 + p;
                    };
                    std::stable_sort(wavgs.begin(), wavgs.end(), [&](const auto& a, const auto& b) {
                        if (a.year != b.year) return a.year < b.year;
                        return periodRank(a) < periodRank(b);
                    });
                    const float chartH = 132.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, chartH, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                    g.DrawString(L"Evolución del promedio ponderado por periodo", -1, &fSec,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 8.0f * dpiScale, cw - 150.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    Views::DrawPill(g, pad + cw - 124.0f * dpiScale, y + 6.0f * dpiScale,
                                    std::to_wstring(wavgs.size()) + L" periodos", dpiScale,
                                    Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 112.0f);

                    const float cx0 = pad + 42.0f * dpiScale;
                    const float cy0 = y + 32.0f * dpiScale;
                    const float cW = std::max(80.0f, cw - 62.0f * dpiScale);
                    const float cH = chartH - 56.0f * dpiScale;
                    double minV = 55.0, maxV = 100.0;
                    for (const auto& wrow : wavgs) {
                        if (wrow.weighted_avg > 0.0 && wrow.weighted_avg < minV) minV = std::max(0.0, wrow.weighted_avg - 5.0);
                    }
                    auto valToY = [&](double v) {
                        double t = std::clamp((v - minV) / std::max(1.0, maxV - minV), 0.0, 1.0);
                        return cy0 + cH * static_cast<float>(1.0 - t);
                    };

                    // Reference line at 70
                    const float y70 = valToY(70.0);
                    Gdiplus::Pen refPen(UiGlass::ToGdip(pal.accentGreen, 130), 1.0f);
                    refPen.SetDashStyle(Gdiplus::DashStyleDash);
                    g.DrawLine(&refPen, cx0, y70, cx0 + cW, y70);
                    g.DrawString(L"70", -1, &fSmall, Gdiplus::RectF(pad + 10.0f * dpiScale, y70 - 7.0f * dpiScale, 28.0f * dpiScale, 14.0f * dpiScale), &sf, &brTert);
                    g.DrawString(L"100", -1, &fSmall, Gdiplus::RectF(pad + 8.0f * dpiScale, cy0 - 6.0f * dpiScale, 30.0f * dpiScale, 14.0f * dpiScale), &sf, &brTert);

                    std::vector<Gdiplus::PointF> pts;
                    pts.reserve(wavgs.size());
                    const float stepX = (wavgs.size() > 1) ? (cW / static_cast<float>(wavgs.size() - 1)) : 0.0f;
                    for (size_t i = 0; i < wavgs.size(); ++i) {
                        float px = (wavgs.size() > 1) ? (cx0 + stepX * static_cast<float>(i)) : (cx0 + cW * 0.5f);
                        float py = valToY(wavgs[i].weighted_avg);
                        pts.emplace_back(px, py);
                    }
                    if (pts.size() >= 2) {
                        Gdiplus::Pen linePen(UiGlass::ToGdip(pal.accentBlue), 2.2f * dpiScale);
                        g.DrawLines(&linePen, pts.data(), static_cast<INT>(pts.size()));
                    }
                    Gdiplus::StringFormat sfC;
                    sfC.SetAlignment(Gdiplus::StringAlignmentCenter);
                    sfC.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
                    for (size_t i = 0; i < wavgs.size(); ++i) {
                        const auto& wrow = wavgs[i];
                        const UiGlass::Rgba8 pCol = (wrow.weighted_avg >= 70.0) ? pal.accentGreen : pal.accentRed;
                        Gdiplus::SolidBrush dotBr(UiGlass::ToGdip(pCol));
                        g.FillEllipse(&dotBr, pts[i].X - 3.8f * dpiScale, pts[i].Y - 3.8f * dpiScale, 7.6f * dpiScale, 7.6f * dpiScale);
                        if (wavgs.size() <= 14 || (i % 2 == 0) || i + 1 == wavgs.size()) {
                            std::string lbl = std::to_string(wrow.year % 100) + "-" +
                                              (!wrow.modality.empty() ? std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(wrow.modality[0])))) : "P") +
                                              wrow.period;
                            g.DrawString(Utf8ToWide(lbl).c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pts[i].X - 24.0f * dpiScale, cy0 + cH + 3.0f * dpiScale, 48.0f * dpiScale, 15.0f * dpiScale),
                                         &sfC, &brTert);
                            g.DrawString(Utf8ToWide(FmtDouble(wrow.weighted_avg, 0)).c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pts[i].X - 18.0f * dpiScale, pts[i].Y - 16.0f * dpiScale, 36.0f * dpiScale, 14.0f * dpiScale),
                                         &sfC, &brPrim);
                        }
                    }
                    y += chartH + 12.0f * dpiScale;
                }

                // 2. Grades Table Header + Filter Pills + Toggle (Todos los intentos / Por materia)
                g.DrawString(L"Historial oficial de calificaciones", -1, &fSec,
                             Gdiplus::RectF(pad, y + 4.0f * dpiScale, 240.0f * dpiScale, 20.0f * dpiScale), &sf, &brPrim);
                const bool hasOfficialAttempts = st->snap.expediente && !st->snap.expediente->structured.grades.empty();
                if (hasOfficialAttempts) {
                    const std::wstring togLbl = st->expShowAllAttempts ? L"Vista: Todos los intentos" : L"Vista: Resumen por materia";
                    const float tw = Views::DrawPill(g, pad + 244.0f * dpiScale, y + 2.0f * dpiScale, togLbl, dpiScale,
                                                     Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 180.0f);
                    st->rcExpToggleAttempts = { static_cast<LONG>(pad + 244.0f * dpiScale), static_cast<LONG>(y + 2.0f * dpiScale),
                                                static_cast<LONG>(pad + 244.0f * dpiScale + tw), static_cast<LONG>(y + 26.0f * dpiScale) };
                }
                static const std::vector<std::wstring> kFilters = { L"Todas", L"Aprobadas", L"En curso", L"Reprobadas" };
                const float segW = std::min(320.0f * dpiScale, cw * 0.44f);
                Views::DrawSegmentedBar(g, pad + cw - segW, y, segW, 27.0f * dpiScale, kFilters,
                                        st->expGradeFilter, st->expGradeFilterHover, dpiScale, pal, hc, st->expGradeFilterHits);
                y += 36.0f * dpiScale;

                // Choose source rows based on toggle
                std::vector<td::GradeRecordRow> sourceRows;
                if (st->expShowAllAttempts && hasOfficialAttempts) {
                    sourceRows = st->snap.expediente->structured.grades;
                } else {
                    // Build deduplicated summary from studentProfile when available, else fallback to effectiveGrades
                    if (st->snap.studentProfile && !st->snap.studentProfile->grades.empty()) {
                        DocStudio::TecDigital::TecAcademicSnapshot tmpSnap;
                        tmpSnap.studentProfile = st->snap.studentProfile;
                        tmpSnap.curriculumGraph = st->snap.curriculumGraph;
                        td::AcademicStatsSummary unusedSt{};
                        ComputeEffectiveExpedienteData(tmpSnap, unusedSt, sourceRows);
                    }
                    if (sourceRows.empty()) sourceRows = effectiveGrades;
                }

                // Normalize TEC official status codes (EC, CUR, AC, RPA, RTJ, REC, SUF, CON) + grade display
                struct GradeDisplayInfo {
                    std::string label;
                    std::string gradeTxt;
                    UiGlass::Rgba8 badgeCol;
                    UiGlass::Rgba8 numCol;
                    int bucket = 0; // 1=aprob, 2=en curso, 3=reprob, 4=retiro/other
                };
                auto classifyGrade = [&](const td::GradeRecordRow& gr) -> GradeDisplayInfo {
                    const std::string low = SafeLowerAscii(gr.state);
                    GradeDisplayInfo out;
                    const bool isPassNoGrade = (gr.grade > 100.0);
                    out.gradeTxt = isPassNoGrade ? "AP" : (gr.grade_text.empty() ? "--" : gr.grade_text);
                    if (low == "ec" || low.find("curso") != std::string::npos || low.find("matric") != std::string::npos) {
                        out.label = "En curso";
                        out.badgeCol = pal.accentBlue;
                        out.bucket = 2;
                        if (gr.grade <= 0.0) out.gradeTxt = "--";
                    } else if (low == "rtj" || low == "ret" || low.find("retir") != std::string::npos) {
                        out.label = (low == "rtj") ? "Retiro just." : "Retirada";
                        out.badgeCol = pal.accentOrange;
                        out.bucket = 4;
                        if (gr.grade <= 0.0) out.gradeTxt = "--";
                    } else if (low == "ac" || low.find("aband") != std::string::npos) {
                        out.label = "Abandono";
                        out.badgeCol = pal.accentRed;
                        out.bucket = 3;
                    } else if (low == "rpa") {
                        out.label = "Reprob. asist.";
                        out.badgeCol = pal.accentRed;
                        out.bucket = 3;
                    } else if (low == "rec" || low.find("reconoc") != std::string::npos) {
                        out.label = "Reconocida";
                        out.badgeCol = pal.accentTeal;
                        out.bucket = 1;
                        if (gr.grade <= 0.0) out.gradeTxt = "AP";
                    } else if (low == "suf" || low.find("sufic") != std::string::npos) {
                        out.label = "Suficiencia";
                        out.badgeCol = pal.accentIndigo;
                        out.bucket = 1;
                    } else if (low == "con" || low.find("congel") != std::string::npos) {
                        out.label = "Congelada";
                        out.badgeCol = pal.accentPurple;
                        out.bucket = 4;
                    } else if (low == "cur") {
                        if (gr.grade >= 70.0 || isPassNoGrade) {
                            out.label = "Aprobada";
                            out.badgeCol = pal.accentGreen;
                            out.bucket = 1;
                        } else {
                            out.label = "Reprobada";
                            out.badgeCol = pal.accentRed;
                            out.bucket = 3;
                        }
                    } else if (low.find("aprob") != std::string::npos || low.find("ganad") != std::string::npos) {
                        out.label = "Aprobada";
                        out.badgeCol = pal.accentGreen;
                        out.bucket = 1;
                    } else if (low.find("reprob") != std::string::npos || low.find("perdid") != std::string::npos) {
                        out.label = "Reprobada";
                        out.badgeCol = pal.accentRed;
                        out.bucket = 3;
                    } else {
                        out.label = gr.state.empty() ? "Registrada" : gr.state;
                        out.badgeCol = pal.accentBlue;
                        out.bucket = (gr.grade >= 70.0) ? 1 : ((gr.grade > 0.0) ? 3 : 4);
                    }
                    if (out.gradeTxt == "--") {
                        out.numCol = pal.labelTertiary;
                    } else if (isPassNoGrade || gr.grade >= 70.0 || out.gradeTxt == "AP") {
                        out.numCol = pal.accentGreen;
                    } else if (gr.grade > 0.0 || out.bucket == 3) {
                        out.numCol = pal.accentRed;
                    } else {
                        out.numCol = pal.labelPrimary;
                    }
                    return out;
                };

                std::vector<const td::GradeRecordRow*> rows;
                for (const auto& gr : sourceRows) {
                    const auto di = classifyGrade(gr);
                    if (st->expGradeFilter == 1 && di.bucket != 1) continue;
                    if (st->expGradeFilter == 2 && di.bucket != 2) continue;
                    if (st->expGradeFilter == 3 && di.bucket != 3) continue;
                    rows.push_back(&gr);
                }

                const float rowH = 38.0f * dpiScale;
                if (rows.empty()) {
                    g.DrawString(L"Sin materias registradas para el filtro seleccionado.", -1, &fRow,
                                 Gdiplus::RectF(pad, y + 6.0f * dpiScale, cw, 20.0f * dpiScale), &sf, &brTert);
                    y += 32.0f * dpiScale;
                } else {
                    for (size_t i = 0; i < rows.size(); ++i) {
                        const auto* gr = rows[i];
                        if (y + rowH >= tblY && y <= tblY + tblH) {
                            if (i > 0) g.DrawLine(&sepPen, pad, y, pad + cw, y);
                            const auto di = classifyGrade(*gr);
                            const std::string per = std::to_string(gr->year) + " · " +
                                                    (!gr->modality.empty() ? gr->modality.substr(0, 1) : "P") + gr->period;
                            g.DrawString(Utf8ToWide(per).c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pad, y + 11.0f * dpiScale, 76.0f * dpiScale, 16.0f * dpiScale), &sf, &brTert);

                            std::string title = gr->course_code.empty() ? gr->course_name : (gr->course_code + "  " + gr->course_name);
                            const float rightArea = 210.0f * dpiScale;
                            g.DrawString(Utf8ToWide(title).c_str(), -1, &fRow,
                                         Gdiplus::RectF(pad + 82.0f * dpiScale, y + 10.0f * dpiScale, std::max(60.0f, cw - 90.0f * dpiScale - rightArea), 18.0f * dpiScale),
                                         &sf, &brPrim);

                            Views::DrawPill(g, pad + cw - rightArea + 4.0f * dpiScale, y + 8.5f * dpiScale,
                                            Utf8ToWide(di.label), dpiScale, Views::SoftTint(di.badgeCol, dark), di.badgeCol, hc, 102.0f);

                            g.DrawString((std::to_wstring(gr->credits) + L" cr").c_str(), -1, &fSmall,
                                         Gdiplus::RectF(pad + cw - 94.0f * dpiScale, y + 11.0f * dpiScale, 38.0f * dpiScale, 16.0f * dpiScale),
                                         &sf, &brTert);

                            Gdiplus::StringFormat sfR;
                            sfR.SetAlignment(Gdiplus::StringAlignmentFar);
                            sfR.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
                            Gdiplus::SolidBrush numBr(UiGlass::ToGdip(di.numCol));
                            g.DrawString(Utf8ToWide(di.gradeTxt).c_str(), -1, &fBold,
                                         Gdiplus::RectF(pad + cw - 52.0f * dpiScale, y + 10.0f * dpiScale, 48.0f * dpiScale, 18.0f * dpiScale),
                                         &sfR, &numBr);
                        }
                        y += rowH;
                    }
                }
            } else if (st->expSubTab == 1) {
                // SUBTAB 1: Alertas RN + Citas de Matrícula + Cursos Pendientes de Graduación
                if (st->snap.studentGeneralInfo && !st->snap.studentGeneralInfo->rn_courses.empty()) {
                    g.DrawString(L"Alertas RN registradas", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                    y += 24.0f * dpiScale;
                    for (const auto& rn : st->snap.studentGeneralInfo->rn_courses) {
                        const float rh = 40.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, Views::SoftTint(pal.accentOrange, dark), pal.accentOrange, 0.9f);
                        Views::DrawPill(g, pad + 10.0f * dpiScale, y + 9.0f * dpiScale,
                                        L"RN " + std::to_wstring(rn.rn), dpiScale, pal.contentCard, pal.accentOrange, hc, 64.0f);
                        g.DrawString(Utf8ToWide(rn.course_code + "  ·  " + rn.course_name).c_str(), -1, &fBold,
                                     Gdiplus::RectF(pad + 84.0f * dpiScale, y + 11.0f * dpiScale, cw - 96.0f * dpiScale, 18.0f * dpiScale),
                                     &sf, &brPrim);
                        y += rh + 6.0f * dpiScale;
                    }
                    y += 10.0f * dpiScale;
                }

                // Citas de Matrícula
                g.DrawString(L"Citas oficiales de matrícula", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                y += 24.0f * dpiScale;
                if (st->snap.expediente && !st->snap.expediente->structured.citas_matricula.empty()) {
                    auto citas = st->snap.expediente->structured.citas_matricula;
                    std::stable_sort(citas.begin(), citas.end(), [](const td::MatriculaAppointment& a, const td::MatriculaAppointment& b) {
                        return a.date > b.date;
                    });
                    for (size_t i = 0; i < citas.size() && i < 16; ++i) {
                        const auto& cm = citas[i];
                        const float rh = 38.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                        Views::DrawPill(g, pad + 10.0f * dpiScale, y + 8.0f * dpiScale, Utf8ToWide(cm.date), dpiScale,
                                        Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 96.0f);
                        g.DrawString(Utf8ToWide(cm.time + "  ·  " + cm.modality + "  ·  Periodo " + cm.period).c_str(), -1, &fRow,
                                     Gdiplus::RectF(pad + 116.0f * dpiScale, y + 10.0f * dpiScale, cw - 128.0f * dpiScale, 18.0f * dpiScale),
                                     &sf, &brPrim);
                        y += rh + 5.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin citas de matrícula registradas en la sincronización actual.", -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brTert);
                    y += 24.0f * dpiScale;
                }
                y += 10.0f * dpiScale;

                // Pendientes de Graduación
                int pendCr = 0;
                for (const auto& pc : st->snap.pendingGraduationCourses) pendCr += std::max(0, pc.credits);
                std::wstring pgHdr = L"Cursos pendientes de graduación (" +
                                     std::to_wstring(st->snap.pendingGraduationCourses.size()) + L" cursos · " +
                                     std::to_wstring(pendCr) + L" créditos)";
                g.DrawString(pgHdr.c_str(), -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                y += 24.0f * dpiScale;
                if (!st->snap.pendingGraduationCourses.empty()) {
                    for (const auto& pc : st->snap.pendingGraduationCourses) {
                        const float rh = 34.0f * dpiScale;
                        if (y + rh >= tblY && y <= tblY + tblH) {
                            Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.75f);
                            g.DrawString(Utf8ToWide(pc.code + "  " + pc.name).c_str(), -1, &fRow,
                                         Gdiplus::RectF(pad + 12.0f * dpiScale, y + 8.0f * dpiScale, cw - 82.0f * dpiScale, 18.0f * dpiScale),
                                         &sf, &brPrim);
                            Views::DrawPill(g, pad + cw - 66.0f * dpiScale, y + 6.0f * dpiScale,
                                            std::to_wstring(pc.credits) + L" cr", dpiScale, pal.sidebarBg, pal.labelSecondary, hc, 56.0f);
                        }
                        y += rh + 4.0f * dpiScale;
                    }
                } else {
                    g.DrawString(L"Sin lista oficial de pendientes cargada.", -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brTert);
                    y += 24.0f * dpiScale;
                }
            } else {
                // SUBTAB 2: 8 Estados Oficiales TEC + Información Académica Oficial
                g.DrawString(L"Distribución oficial de los 8 estados académicos TEC", -1, &fSec,
                             Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                y += 26.0f * dpiScale;
                struct StateBar { const wchar_t* label; int count; UiGlass::Rgba8 col; };
                const StateBar bars[8] = {
                    { L"Aprobadas", s.count_aprobadas, pal.accentGreen },
                    { L"En curso / Matriculadas", s.count_en_curso, pal.accentBlue },
                    { L"Reprobadas", s.count_reprobadas, pal.accentRed },
                    { L"Retiradas", s.count_retiradas, pal.accentOrange },
                    { L"Reconocidas", s.count_reconocidas, pal.accentTeal },
                    { L"Suficiencia", s.count_suficiencia, pal.accentIndigo },
                    { L"Pendientes", s.count_pendientes, pal.labelSecondary },
                    { L"Congeladas", s.count_congeladas, pal.accentPurple }
                };
                int maxCnt = 1;
                for (const auto& b : bars) maxCnt = std::max(maxCnt, b.count);
                for (const auto& b : bars) {
                    g.DrawString(b.label, -1, &fRow, Gdiplus::RectF(pad, y + 2.0f * dpiScale, 175.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
                    const float barX = pad + 182.0f * dpiScale;
                    const float barW = std::max(60.0f, cw - 245.0f * dpiScale);
                    Views::DrawProgressBar(g, barX, y + 7.0f * dpiScale, barW, 8.0f * dpiScale,
                                           static_cast<float>(b.count) / static_cast<float>(maxCnt), pal.sidebarBg, b.col);
                    g.DrawString(std::to_wstring(b.count).c_str(), -1, &fBold,
                                 Gdiplus::RectF(barX + barW + 12.0f * dpiScale, y + 2.0f * dpiScale, 45.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    y += 28.0f * dpiScale;
                }

                y += 12.0f * dpiScale;
                g.DrawString(L"Información académica oficial", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                y += 24.0f * dpiScale;
                std::vector<std::pair<std::string, std::string>> infoPairs;
                if (st->snap.studentGeneralInfo && !st->snap.studentGeneralInfo->career.empty()) {
                    infoPairs.emplace_back("Carrera", st->snap.studentGeneralInfo->career);
                } else if (st->snap.studentCareer && !st->snap.studentCareer->carrera.empty()) {
                    infoPairs.emplace_back("Carrera", st->snap.studentCareer->carrera);
                }
                if (st->snap.curriculumGraph && !st->snap.curriculumGraph->dsc_curriculum.empty()) {
                    infoPairs.emplace_back("Plan de estudios", st->snap.curriculumGraph->dsc_curriculum);
                }
                if (st->snap.studentCareer && !st->snap.studentCareer->sede.empty()) {
                    infoPairs.emplace_back("Sede", st->snap.studentCareer->sede);
                }
                if (s.passed_pct > 0.0 || s.failed_pct > 0.0) {
                    infoPairs.emplace_back("Porcentaje aprobación / reprobación",
                                           FmtDouble(s.passed_pct, 1) + "% aprobados · " + FmtDouble(s.failed_pct, 1) + "% reprobados");
                }
                if (s.hours_taken > 0) infoPairs.emplace_back("Total horas cursadas", std::to_string(s.hours_taken) + " h");
                if (st->snap.personalAcademicInfo) {
                    for (const auto& kv : st->snap.personalAcademicInfo->fields) {
                        infoPairs.push_back(kv);
                    }
                }
                for (const auto& kv : infoPairs) {
                    const float rh = 32.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.75f);
                    g.DrawString(Utf8ToWide(kv.first).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 8.0f * dpiScale, 180.0f * dpiScale, 16.0f * dpiScale), &sf, &brTert);
                    g.DrawString(Utf8ToWide(kv.second).c_str(), -1, &fRow,
                                 Gdiplus::RectF(pad + 196.0f * dpiScale, y + 7.0f * dpiScale, cw - 208.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
                    y += rh + 5.0f * dpiScale;
                }
            }

            const float totalH = (y + static_cast<float>(st->expStatsScrollY)) - tblY + 14.0f * dpiScale;
            st->expStatsMaxScroll = std::max(0, static_cast<int>(totalH - tblH));
            st->expStatsScrollY = std::clamp(st->expStatsScrollY, 0, st->expStatsMaxScroll);
            g.ResetClip();
            Views::DrawScrollbarThumb(g, w, h, st->expStatsScrollY, st->expStatsMaxScroll, dpiScale, pal, tblY + 6.0f);
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK ExpStatsCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) { PaintExpStatsCanvas(hwnd, st); return 0; }
    if (msg == WM_MOUSEWHEEL && st) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->expStatsScrollY = std::clamp(st->expStatsScrollY - (delta / 2), 0, st->expStatsMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        int hov = -1;
        for (const auto& h : st->expGradeFilterHits) {
            if (PtInRect(&h.rc, pt)) { hov = h.index; break; }
        }
        int hovSub = -1;
        for (const auto& h : st->expSubTabHits) {
            if (PtInRect(&h.rc, pt)) { hovSub = h.index; break; }
        }
        if (hov != st->expGradeFilterHover || hovSub != st->expSubTabHover) {
            st->expGradeFilterHover = hov;
            st->expSubTabHover = hovSub;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        for (const auto& h : st->expSubTabHits) {
            if (PtInRect(&h.rc, pt)) {
                st->expSubTab = h.index;
                st->expStatsScrollY = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        if (PtInRect(&st->rcExpToggleAttempts, pt)) {
            st->expShowAllAttempts = !st->expShowAllAttempts;
            st->expStatsScrollY = 0;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        for (const auto& h : st->expGradeFilterHits) {
            if (PtInRect(&h.rc, pt)) {
                st->expGradeFilter = h.index;
                st->expStatsScrollY = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintFtsCanvas(HWND hwnd, TecHubState* st) {
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

    st->ftsCardRects.clear();
    st->ftsModeHits.clear();
    st->ftsActionRects.clear();
    st->personalFolderRects.clear();
    st->personalBreadcrumbRects.clear();
    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        Views::DrawSurface(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                           10.0f * dpiScale, pal.contentCard, pal.borderSubtle, 1.0f);

        const float pad = 14.0f * dpiScale;
        const float segH = 30.0f * dpiScale;
        const size_t globalCount = st->globalLibCatalog.size();
        const size_t pdocCount = st->snap.personalDocuments ? st->snap.personalDocuments->files.size() : 0;
        std::vector<std::wstring> modeLabels = {
            globalCount > 0 ? (L"Todos los archivos (" + std::to_wstring(globalCount) + L")") : L"Todos los archivos",
            L"Buscar en apuntes",
            pdocCount > 0 ? (L"Mis documentos (" + std::to_wstring(pdocCount) + L")") : L"Mis documentos"
        };
        const float segW = std::min(560.0f * dpiScale, static_cast<float>(w) - pad * 2.0f);
        Views::DrawSegmentedBar(g, pad, pad, segW, segH, modeLabels, st->ftsModeTab, st->ftsModeHover,
                                dpiScale, pal, hc, st->ftsModeHits);

        const float bodyTop = pad + segH + 12.0f * dpiScale;
        const float bodyH = std::max(60.0f, static_cast<float>(h) - bodyTop - 8.0f * dpiScale);
        const float cw = std::max(100.0f, static_cast<float>(w) - pad * 2.0f - 8.0f * dpiScale);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fTitle(&ff, 12.4f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSnip(&ff, 11.2f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
        Gdiplus::StringFormat sf;
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        if (st->ftsModeTab == 0) {
            // 0: Biblioteca Global Unificada (todos los espacios, carpetas profundas, adjuntos de evaluación y Mis documentos)
            const float chipRowY = bodyTop;
            static const wchar_t* kFilterLbls[6] = {
                L"Todos", L"PDF", L"Docs / PPT", L"Solo en directorio", L"Nuevos / Modif.", L"Evaluaciones"
            };
            float fx = pad;
            for (int fi = 0; fi < 6; ++fi) {
                const bool act = (st->globalLibFilter == fi);
                const float pw = Views::DrawPill(g, fx, chipRowY, kFilterLbls[fi], dpiScale,
                                                 act ? pal.accentBlue : pal.sidebarBg,
                                                 act ? UiGlass::Rgba8{ 255, 255, 255, 255 } : pal.labelSecondary,
                                                 hc, 136.0f);
                st->ftsActionRects.push_back({ { static_cast<LONG>(fx), static_cast<LONG>(chipRowY),
                                                 static_cast<LONG>(fx + pw), static_cast<LONG>(chipRowY + 22.0f * dpiScale) },
                                               "glib_filter:" + std::to_string(fi) });
                fx += pw + 5.0f * dpiScale;
            }
            static const wchar_t* kSortLbls[4] = {
                L"Orden: Recientes", L"Orden: Creación", L"Orden: Nombre", L"Orden: Tamaño"
            };
            const int sIdx = std::clamp(st->globalLibSort, 0, 3);
            const float sortW = 118.0f * dpiScale;
            const float drawnSortW = Views::DrawPill(g, pad + cw - sortW, chipRowY, kSortLbls[sIdx], dpiScale,
                                                     Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 122.0f);
            st->ftsActionRects.push_back({ { static_cast<LONG>(pad + cw - sortW), static_cast<LONG>(chipRowY),
                                             static_cast<LONG>(pad + cw - sortW + drawnSortW), static_cast<LONG>(chipRowY + 22.0f * dpiScale) },
                                           "glib_sort_next" });

            const float listTop = chipRowY + 30.0f * dpiScale;
            const float listH = std::max(60.0f, static_cast<float>(h) - listTop - 8.0f * dpiScale);

            if (st->globalLibFiltered.empty()) {
                DrawEmptyStateCentered(g, w, h, dpiScale, pal, hc, UiGlass::IconId::Document,
                                       st->globalLibCatalog.empty() ? L"Biblioteca global de TEC Digital" : L"Sin archivos para este filtro",
                                       st->globalLibCatalog.empty()
                                           ? L"Presiona Sincronizar en la barra lateral para indexar todos los archivos de tus cursos, cátedras, escuelas y evaluaciones."
                                           : L"Prueba otro término de búsqueda o cambia el filtro superior.");
                st->ftsMaxScroll = 0;
            } else {
                const float rowH = 60.0f * dpiScale;
                const float gap = 7.0f * dpiScale;
                const size_t cap = std::min<size_t>(st->globalLibFiltered.size(), 400);
                st->ftsMaxScroll = std::max(0, static_cast<int>(cap * (rowH + gap) + 12.0f * dpiScale) - static_cast<int>(listH));
                st->ftsScrollY = std::clamp(st->ftsScrollY, 0, st->ftsMaxScroll);

                g.SetClip(Gdiplus::RectF(4.0f, listTop, static_cast<float>(w) - 8.0f, listH));
                float y = listTop - static_cast<float>(st->ftsScrollY);
                for (size_t i = 0; i < cap; ++i) {
                    const auto& e = st->globalLibFiltered[i];
                    const bool sel = (static_cast<int>(i) == st->selectedFtsIdx);
                    const bool hov = (static_cast<int>(i) == st->ftsHoverIdx && !sel);
                    RECT frc{ static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rowH) };
                    st->ftsCardRects.push_back(frc);

                    if (y + rowH >= listTop - rowH && y <= static_cast<float>(h)) {
                        Views::DrawSurface(g, pad, y, cw, rowH, 9.0f * dpiScale,
                                           sel ? Views::SoftTint(pal.accentBlue, dark) : (hov ? pal.sidebarHover : pal.contentCardElevated),
                                           sel ? pal.accentBlue : pal.borderSubtle, sel ? 1.4f : 0.85f);

                        UiGlass::Rgba8 extCol = pal.accentBlue;
                        const std::wstring extLbl = Views::FileExtBadgeLabel(e.fileName, extCol, pal);
                        const float bSz = 32.0f * dpiScale;
                        Views::DrawSurface(g, pad + 10.0f * dpiScale, y + (rowH - bSz) * 0.5f, bSz, bSz, 7.0f * dpiScale,
                                           Views::SoftTint(extCol, dark), { 0, 0, 0, 0 }, 0.0f);
                        Gdiplus::Font fExt(&ff, 10.2f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                        Gdiplus::SolidBrush extBr(UiGlass::ToGdip(extCol));
                        Gdiplus::StringFormat sfC;
                        sfC.SetAlignment(Gdiplus::StringAlignmentCenter);
                        sfC.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                        g.DrawString(extLbl.c_str(), -1, &fExt,
                                     Gdiplus::RectF(pad + 10.0f * dpiScale, y + (rowH - bSz) * 0.5f, bSz, bSz), &sfC, &extBr);

                        float rightUsed = 10.0f * dpiScale;
                        const float locW = 80.0f * dpiScale;
                        Views::DrawPill(g, pad + cw - locW - rightUsed, y + 9.0f * dpiScale,
                                        e.isLocal ? L"Sin conexión" : L"En la nube", dpiScale,
                                        e.isLocal ? Views::SoftTint(pal.accentGreen, dark) : pal.sidebarBg,
                                        e.isLocal ? pal.accentGreen : pal.labelSecondary, hc, 84.0f);
                        rightUsed += locW + 6.0f * dpiScale;

                        if (e.isRecentlyChanged) {
                            const float nw = 58.0f * dpiScale;
                            Views::DrawPill(g, pad + cw - nw - rightUsed, y + 9.0f * dpiScale, L"Nuevo", dpiScale,
                                            Views::SoftTint(pal.accentOrange, dark), pal.accentOrange, hc, 62.0f);
                            rightUsed += nw + 6.0f * dpiScale;
                        } else if (!e.isLinkedInPortada && !e.isPersonal && !e.isEvalAttachment) {
                            const float dw = 108.0f * dpiScale;
                            Views::DrawPill(g, pad + cw - dw - rightUsed, y + 9.0f * dpiScale, L"Solo directorio", dpiScale,
                                            Views::SoftTint(pal.accentTeal, dark), pal.accentTeal, hc, 112.0f);
                            rightUsed += dw + 6.0f * dpiScale;
                        }

                        const float tx = pad + 52.0f * dpiScale;
                        const float tw = std::max(60.0f, cw - 58.0f * dpiScale - rightUsed);
                        g.DrawString(Utf8ToWide(e.fileName).c_str(), -1, &fTitle,
                                     Gdiplus::RectF(tx, y + 8.0f * dpiScale, tw, 18.0f * dpiScale), &sf, &brPrim);

                        std::string sub = e.spaceName;
                        if (!e.folderPath.empty()) sub += " › " + e.folderPath;
                        if (e.contentSize > 0) sub += "  ·  " + std::to_string(std::max<std::uint64_t>(1, e.contentSize / 1024)) + " KB";
                        const std::string modStr = FormatFriendlyTimestampDialog(e.lastModifiedAnsi);
                        const std::string creStr = FormatFriendlyTimestampDialog(e.creationDateAnsi);
                        if (!modStr.empty()) sub += "  ·  Mod. " + modStr;
                        else if (!creStr.empty()) sub += "  ·  Creación " + creStr;
                        if (!e.authorName.empty()) sub += "  ·  " + e.authorName;
                        g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSnip,
                                     Gdiplus::RectF(tx, y + 30.0f * dpiScale, cw - 66.0f * dpiScale, 16.0f * dpiScale), &sf, &brTert);
                    }
                    y += rowH + gap;
                }
                g.ResetClip();
                Views::DrawScrollbarThumb(g, w, h, st->ftsScrollY, st->ftsMaxScroll, dpiScale, pal, listTop);
            }
        } else if (st->ftsModeTab == 2) {
            // 2: Personal "Mis documentos" interactive folder browser + full CRUD (+ Nueva carpeta, Subir, Eliminar)
            if (!st->snap.personalDocuments ||
                (st->snap.personalDocuments->files.empty() && st->snap.personalDocuments->folders.empty())) {
                DrawEmptyStateCentered(g, w, h, dpiScale, pal, hc, UiGlass::IconId::Document,
                                       L"Mis documentos de TEC Digital",
                                       L"Inicia sesión y sincroniza para explorar y administrar tus carpetas y archivos personales de TEC Digital.");
                st->ftsMaxScroll = 0;
            } else {
                const auto& pd = *st->snap.personalDocuments;
                std::string rootFid = pd.root_folder_id;
                if (rootFid.empty() && !pd.folders.empty()) rootFid = pd.folders.front().folder_id;
                std::string activeFid = st->currentPersonalFolderId.empty() ? rootFid : st->currentPersonalFolderId;

                g.SetClip(Gdiplus::RectF(4.0f, bodyTop, static_cast<float>(w) - 8.0f, bodyH));
                float y = bodyTop - static_cast<float>(st->ftsScrollY);

                // Breadcrumbs + Personal CRUD pills (+ Nueva carpeta, Subir archivo)
                std::map<std::string, const td::FolderRecord*> byId;
                for (const auto& d : pd.folders) byId[d.folder_id] = &d;
                std::vector<std::pair<std::string, std::string>> crumbs;
                std::string cur = activeFid;
                std::set<std::string> vis;
                while (!cur.empty() && vis.insert(cur).second) {
                    auto it = byId.find(cur);
                    if (it == byId.end()) break;
                    crumbs.push_back({ cur, cur == rootFid ? "Mis documentos" : it->second->name });
                    if (cur == rootFid) break;
                    cur = it->second->parent_id;
                }
                std::reverse(crumbs.begin(), crumbs.end());
                if (crumbs.empty()) crumbs.push_back({ rootFid, "Mis documentos" });
                float bx = pad;
                for (size_t ci = 0; ci < crumbs.size(); ++ci) {
                    const bool isLast = (ci + 1 == crumbs.size());
                    float pw = Views::DrawPill(g, bx, y, Utf8ToWide(crumbs[ci].second), dpiScale,
                                               isLast ? Views::SoftTint(pal.accentBlue, dark) : pal.sidebarBg,
                                               isLast ? pal.accentBlue : pal.labelSecondary, hc, 160.0f);
                    st->personalBreadcrumbRects.push_back({ { static_cast<LONG>(bx), static_cast<LONG>(y),
                                                              static_cast<LONG>(bx + pw), static_cast<LONG>(y + 21.0f * dpiScale) }, crumbs[ci].first });
                    bx += pw + 4.0f * dpiScale;
                    if (!isLast) {
                        g.DrawString(L"›", -1, &fTitle, Gdiplus::RectF(bx, y + 1.0f, 10.0f * dpiScale, 20.0f * dpiScale), &sf, &brTert);
                        bx += 12.0f * dpiScale;
                    }
                }
                // Right-aligned CRUD pills
                const float upW = 98.0f * dpiScale;
                const float mkW = 114.0f * dpiScale;
                const float drawnUpW = Views::DrawPill(g, pad + cw - upW, y, L"Subir archivo", dpiScale,
                                                       Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, hc, 102.0f);
                st->ftsActionRects.push_back({ { static_cast<LONG>(pad + cw - upW), static_cast<LONG>(y),
                                                 static_cast<LONG>(pad + cw - upW + drawnUpW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                               "pers_upload:" + activeFid });
                const float drawnMkW = Views::DrawPill(g, pad + cw - upW - mkW - 6.0f * dpiScale, y, L"+ Nueva carpeta", dpiScale,
                                                       Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 118.0f);
                st->ftsActionRects.push_back({ { static_cast<LONG>(pad + cw - upW - mkW - 6.0f * dpiScale), static_cast<LONG>(y),
                                                 static_cast<LONG>(pad + cw - upW - mkW - 6.0f * dpiScale + drawnMkW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                               "pers_mkdir:" + activeFid });
                y += 30.0f * dpiScale;

                // Subfolders
                for (const auto& d : pd.folders) {
                    if (d.folder_id == activeFid || d.parent_id != activeFid) continue;
                    const float rh = 46.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                    const float bSz = 28.0f * dpiScale;
                    Views::DrawSurface(g, pad + 10.0f * dpiScale, y + (rh - bSz) * 0.5f, bSz, bSz, 7.0f * dpiScale,
                                       Views::SoftTint(pal.accentOrange, dark), { 0, 0, 0, 0 }, 0.0f);
                    UiGlass::DrawVectorIconGdiPlus(g, UiGlass::IconId::LayersStack,
                                                   pad + 10.0f * dpiScale + bSz * 0.5f, y + rh * 0.5f,
                                                   13.0f * dpiScale, UiGlass::ToGdip(pal.accentOrange), 1.5f * dpiScale);
                    const float delW = 68.0f * dpiScale;
                    const float dDelW = Views::DrawPill(g, pad + cw - delW - 8.0f * dpiScale, y + 12.0f * dpiScale, L"Eliminar", dpiScale,
                                                        Views::SoftTint(pal.accentRed, dark), pal.accentRed, hc, 72.0f);
                    st->ftsActionRects.push_back({ { static_cast<LONG>(pad + cw - delW - 8.0f * dpiScale), static_cast<LONG>(y + 12.0f * dpiScale),
                                                     static_cast<LONG>(pad + cw - delW - 8.0f * dpiScale + dDelW), static_cast<LONG>(y + 34.0f * dpiScale) },
                                                   "pers_del_folder:" + d.folder_id + "|" + d.name });
                    g.DrawString(Utf8ToWide(d.name).c_str(), -1, &fTitle,
                                 Gdiplus::RectF(pad + 48.0f * dpiScale, y + 7.0f * dpiScale, cw - 134.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    std::string fsub = "Carpeta personal";
                    const std::string fmod = FormatFriendlyTimestampDialog(d.last_modified_ansi);
                    if (!fmod.empty()) fsub += "  ·  Mod. " + fmod;
                    g.DrawString(Utf8ToWide(fsub).c_str(), -1, &fSnip,
                                 Gdiplus::RectF(pad + 48.0f * dpiScale, y + 25.0f * dpiScale, cw - 134.0f * dpiScale, 15.0f * dpiScale),
                                 &sf, &brTert);
                    st->personalFolderRects.push_back({ { static_cast<LONG>(pad), static_cast<LONG>(y),
                                                          static_cast<LONG>(pad + cw - delW - 14.0f * dpiScale), static_cast<LONG>(y + rh) }, d.folder_id });
                    y += rh + 6.0f * dpiScale;
                }

                // Files in current personal folder
                st->currentPersonalFiles.clear();
                bool anyHasParent = false;
                for (const auto& f : pd.files) if (!f.parent_folder_id.empty()) { anyHasParent = true; break; }
                for (const auto& f : pd.files) {
                    if (anyHasParent && !activeFid.empty() && f.parent_folder_id != activeFid) continue;
                    const int pIdx = static_cast<int>(st->currentPersonalFiles.size());
                    st->currentPersonalFiles.push_back(f);
                    const float rh = 52.0f * dpiScale;
                    const bool sel = (pIdx == st->selectedFtsIdx);
                    Views::DrawSurface(g, pad, y, cw, rh, 9.0f * dpiScale,
                                       sel ? Views::SoftTint(pal.accentBlue, dark) : pal.contentCardElevated,
                                       sel ? pal.accentBlue : pal.borderSubtle, sel ? 1.4f : 0.85f);
                    UiGlass::Rgba8 extCol = pal.accentBlue;
                    const std::wstring extLbl = Views::FileExtBadgeLabel(f.name, extCol, pal);
                    const float bSz = 30.0f * dpiScale;
                    Views::DrawSurface(g, pad + 10.0f * dpiScale, y + (rh - bSz) * 0.5f, bSz, bSz, 7.0f * dpiScale,
                                       Views::SoftTint(extCol, dark), { 0, 0, 0, 0 }, 0.0f);
                    Gdiplus::Font fExt(&ff, 10.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                    Gdiplus::SolidBrush extBr(UiGlass::ToGdip(extCol));
                    Gdiplus::StringFormat sfC;
                    sfC.SetAlignment(Gdiplus::StringAlignmentCenter);
                    sfC.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                    g.DrawString(extLbl.c_str(), -1, &fExt,
                                 Gdiplus::RectF(pad + 10.0f * dpiScale, y + (rh - bSz) * 0.5f, bSz, bSz), &sfC, &extBr);

                    const float delW = 68.0f * dpiScale;
                    const float dDelW = Views::DrawPill(g, pad + cw - delW - 8.0f * dpiScale, y + 14.0f * dpiScale, L"Eliminar", dpiScale,
                                                        Views::SoftTint(pal.accentRed, dark), pal.accentRed, hc, 72.0f);
                    if (!f.object_id.empty()) {
                        st->ftsActionRects.push_back({ { static_cast<LONG>(pad + cw - delW - 8.0f * dpiScale), static_cast<LONG>(y + 14.0f * dpiScale),
                                                         static_cast<LONG>(pad + cw - delW - 8.0f * dpiScale + dDelW), static_cast<LONG>(y + 36.0f * dpiScale) },
                                                       "pers_del_file:" + f.object_id + "|" + f.name });
                    }
                    const bool isLocal = (f.status == td::FileStatus::Downloaded || f.deduplicated);
                    Views::DrawPill(g, pad + cw - delW - 94.0f * dpiScale, y + 14.0f * dpiScale,
                                    isLocal ? L"Sin conexión" : L"En la nube", dpiScale,
                                    isLocal ? Views::SoftTint(pal.accentGreen, dark) : pal.sidebarBg,
                                    isLocal ? pal.accentGreen : pal.labelSecondary, hc, 82.0f);

                    g.DrawString(Utf8ToWide(f.name).c_str(), -1, &fTitle,
                                 Gdiplus::RectF(pad + 50.0f * dpiScale, y + 7.0f * dpiScale, cw - 220.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    std::string sub = std::to_string(std::max<std::uint64_t>(1, f.content_size / 1024)) + " KB";
                    if (!f.folder_path.empty()) sub = f.folder_path + "  ·  " + sub;
                    const std::string pMod = FormatFriendlyTimestampDialog(f.last_modified_ansi);
                    if (!pMod.empty()) sub += "  ·  Mod. " + pMod;
                    g.DrawString(Utf8ToWide(sub).c_str(), -1, &fSnip,
                                 Gdiplus::RectF(pad + 50.0f * dpiScale, y + 27.0f * dpiScale, cw - 220.0f * dpiScale, 16.0f * dpiScale),
                                 &sf, &brTert);
                    st->ftsCardRects.push_back({ static_cast<LONG>(pad), static_cast<LONG>(y),
                                                 static_cast<LONG>(pad + cw - delW - 14.0f * dpiScale), static_cast<LONG>(y + rh) });
                    y += rh + 6.0f * dpiScale;
                }

                const float bottomY = y + static_cast<float>(st->ftsScrollY) + 12.0f * dpiScale;
                st->ftsMaxScroll = std::max(0, static_cast<int>(bottomY - static_cast<float>(h)));
                st->ftsScrollY = std::clamp(st->ftsScrollY, 0, st->ftsMaxScroll);
                g.ResetClip();
                Views::DrawScrollbarThumb(g, w, h, st->ftsScrollY, st->ftsMaxScroll, dpiScale, pal, bodyTop);
            }
        } else {
            // 1: Buscar en apuntes (offline FTS5 + active notebook)
            const size_t totalItems = st->ftsHits.size() + st->activeDocHits.size();
            if (totalItems == 0) {
                DrawEmptyStateCentered(g, w, h, dpiScale, pal, hc, UiGlass::IconId::Search,
                                       L"Busca en tus materiales y apuntes",
                                       L"Escribe un tema, fórmula o palabra clave arriba para buscar instantáneamente en los PDFs descargados y en tu cuaderno activo.");
                st->ftsMaxScroll = 0;
            } else {
                const float rowH = 66.0f * dpiScale;
                const float gap = 8.0f * dpiScale;
                st->ftsMaxScroll = std::max(0, static_cast<int>(bodyTop + totalItems * (rowH + gap) + 12.0f * dpiScale) - h);
                st->ftsScrollY = std::clamp(st->ftsScrollY, 0, st->ftsMaxScroll);

                g.SetClip(Gdiplus::RectF(4.0f, bodyTop, static_cast<float>(w) - 8.0f, bodyH));
                float y = bodyTop - static_cast<float>(st->ftsScrollY);
                for (size_t i = 0; i < totalItems; ++i) {
                    const bool isMat = (i < st->ftsHits.size());
                    const std::string title = isMat ? st->ftsHits[i].document.name : st->activeDocHits[i - st->ftsHits.size()].titleUtf8;
                    const std::string tag = isMat ? (st->ftsHits[i].document.community.empty() ? "Curso" : st->ftsHits[i].document.community) : "Cuaderno activo";
                    const std::string snip = isMat ? st->ftsHits[i].snippet : st->activeDocHits[i - st->ftsHits.size()].snippetUtf8;
                    const bool sel = (static_cast<int>(i) == st->selectedFtsIdx);
                    const bool hov = (static_cast<int>(i) == st->ftsHoverIdx && !sel);

                    RECT frc{ static_cast<LONG>(pad), static_cast<LONG>(y), static_cast<LONG>(pad + cw), static_cast<LONG>(y + rowH) };
                    st->ftsCardRects.push_back(frc);

                    if (y + rowH >= bodyTop - rowH && y <= static_cast<float>(h)) {
                        Views::DrawSurface(g, pad, y, cw, rowH, 9.0f * dpiScale,
                                           sel ? Views::SoftTint(pal.accentBlue, dark) : (hov ? pal.sidebarHover : pal.contentCardElevated),
                                           sel ? pal.accentBlue : pal.borderSubtle, sel ? 1.4f : 0.85f);
                        Views::DrawPill(g, pad + cw - 110.0f * dpiScale, y + 10.0f * dpiScale, Utf8ToWide(tag), dpiScale,
                                        Views::SoftTint(isMat ? pal.accentBlue : pal.accentPurple, dark),
                                        isMat ? pal.accentBlue : pal.accentPurple, hc, 100.0f);
                        g.DrawString(Utf8ToWide(title).c_str(), -1, &fTitle,
                                     Gdiplus::RectF(pad + 14.0f * dpiScale, y + 10.0f * dpiScale, cw - 134.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
                        g.DrawString(Utf8ToWide(snip).c_str(), -1, &fSnip,
                                     Gdiplus::RectF(pad + 14.0f * dpiScale, y + 32.0f * dpiScale, cw - 28.0f * dpiScale, 26.0f * dpiScale), &sf, &brSec);
                    }
                    y += rowH + gap;
                }
                g.ResetClip();
                Views::DrawScrollbarThumb(g, w, h, st->ftsScrollY, st->ftsMaxScroll, dpiScale, pal, bodyTop);
            }
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK FtsCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) { PaintFtsCanvas(hwnd, st); return 0; }
    if (msg == WM_MOUSEWHEEL && st) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->ftsScrollY = std::clamp(st->ftsScrollY - (delta / 2), 0, st->ftsMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        int hMode = -1;
        for (const auto& mh : st->ftsModeHits) {
            if (PtInRect(&mh.rc, pt)) { hMode = mh.index; break; }
        }
        int hov = -1;
        for (size_t i = 0; i < st->ftsCardRects.size(); ++i) {
            if (PtInRect(&st->ftsCardRects[i], pt)) { hov = static_cast<int>(i); break; }
        }
        if (hMode != st->ftsModeHover || hov != st->ftsHoverIdx) {
            st->ftsModeHover = hMode;
            st->ftsHoverIdx = hov;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        for (const auto& mh : st->ftsModeHits) {
            if (PtInRect(&mh.rc, pt)) {
                st->ftsModeTab = mh.index;
                st->ftsScrollY = 0;
                st->selectedFtsIdx = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        for (const auto& [rc, act] : st->ftsActionRects) {
            if (!PtInRect(&rc, pt)) continue;
            if (act.rfind("glib_filter:", 0) == 0) {
                st->globalLibFilter = std::clamp(std::atoi(act.substr(12).c_str()), 0, 5);
                RefreshGlobalLibraryFilter(st);
                return 0;
            }
            if (act == "glib_sort_next") {
                st->globalLibSort = (st->globalLibSort + 1) % 4;
                RefreshGlobalLibraryFilter(st);
                return 0;
            }
            if (act.rfind("pers_mkdir:", 0) == 0 && st->store) {
                std::wstring folderName;
                if (ShowSimpleTextPromptDialog(st->hwnd, L"Nueva carpeta en Mis documentos",
                                               L"Nombre de la carpeta que deseas crear en TEC Digital:", L"", folderName)) {
                    const std::string nm = WideToUtf8(folderName);
                    if (!nm.empty()) {
                        auto r = st->store->CreatePersonalFolder(act.substr(11), nm);
                        if (r.ok) {
                            st->snap = st->store->Snapshot();
                            st->globalLibCatalog = DocStudio::TecDigital::TecAcademicStore::BuildGlobalFileCatalog(st->snap);
                            RefreshGlobalLibraryFilter(st);
                            SetWindowTextW(st->hProgressBanner, Utf8ToWide("Carpeta creada: " + nm).c_str());
                            InvalidateRect(hwnd, nullptr, FALSE);
                        } else {
                            SetWindowTextW(st->hProgressBanner, Utf8ToWide("Nueva carpeta: " + r.error_message).c_str());
                        }
                    }
                }
                return 0;
            }
            if (act.rfind("pers_upload:", 0) == 0 && st->store) {
                std::wstring inPath;
                if (PickOpenFileW(st->hwnd, L"Todos los archivos (*.*)\0*.*\0PDF (*.pdf)\0*.pdf\0Documentos (*.docx;*.pptx;*.xlsx)\0*.docx;*.pptx;*.xlsx\0", inPath)) {
                    const std::string pfid = act.substr(12);
                    const std::string pathUtf8 = WideToUtf8(inPath);
                    SetWindowTextW(st->hProgressBanner, L"Subiendo archivo a Mis documentos...");
                    auto r = st->store->UploadPersonalFile(pfid, pathUtf8);
                    if (r.ok && r.value) {
                        st->snap = st->store->Snapshot();
                        st->globalLibCatalog = DocStudio::TecDigital::TecAcademicStore::BuildGlobalFileCatalog(st->snap);
                        RefreshGlobalLibraryFilter(st);
                        SetWindowTextW(st->hProgressBanner, L"Archivo subido a Mis documentos en TEC Digital.");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Subir archivo: " + r.error_message).c_str());
                    }
                }
                return 0;
            }
            if ((act.rfind("pers_del_file:", 0) == 0 || act.rfind("pers_del_folder:", 0) == 0) && st->store) {
                const bool isFolder = (act.rfind("pers_del_folder:", 0) == 0);
                const std::string payload = act.substr(isFolder ? 16 : 14);
                const size_t sep = payload.find('|');
                const std::string oid = (sep == std::string::npos) ? payload : payload.substr(0, sep);
                const std::string nm = (sep == std::string::npos) ? oid : payload.substr(sep + 1);
                std::wstring prompt = (isFolder ? L"¿Deseas eliminar la carpeta personal «" : L"¿Deseas eliminar el archivo personal «") +
                                      Utf8ToWide(nm) + L"» de TEC Digital? Esta acción no se puede deshacer.";
                if (MessageBoxW(st->hwnd, prompt.c_str(), L"Eliminar de Mis documentos", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
                    auto r = st->store->DeletePersonalItem(oid, isFolder, true);
                    if (r.ok && r.value) {
                        st->snap = st->store->Snapshot();
                        st->globalLibCatalog = DocStudio::TecDigital::TecAcademicStore::BuildGlobalFileCatalog(st->snap);
                        RefreshGlobalLibraryFilter(st);
                        SetWindowTextW(st->hProgressBanner, L"Elemento eliminado de Mis documentos.");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Eliminar: " + r.error_message).c_str());
                    }
                }
                return 0;
            }
        }
        if (st->ftsModeTab == 2) {
            for (const auto& [rc, fid] : st->personalBreadcrumbRects) {
                if (PtInRect(&rc, pt)) {
                    st->currentPersonalFolderId = fid;
                    st->ftsScrollY = 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
            for (const auto& [rc, fid] : st->personalFolderRects) {
                if (PtInRect(&rc, pt)) {
                    st->currentPersonalFolderId = fid;
                    st->ftsScrollY = 0;
                    InvalidateRect(hwnd, nullptr, FALSE);
                    return 0;
                }
            }
        }
        for (size_t i = 0; i < st->ftsCardRects.size(); ++i) {
            if (PtInRect(&st->ftsCardRects[i], pt)) {
                st->selectedFtsIdx = static_cast<int>(i);
                if (st->hLstFts) SendMessageW(st->hLstFts, LB_SETCURSEL, i, 0);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintTramitesCanvas(HWND hwnd, TecHubState* st) {
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

    st->tramitesSubTabHits.clear();
    st->tramitesActionRects.clear();
    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        static const std::vector<std::wstring> kTramTabs = {
            L"Resumen y DAR",
            L"Control de Plagio",
            L"Modelo de Usuario",
            L"Eval. Docente y Ferias",
            L"Notificaciones y Cuenta"
        };
        const float segH = 30.0f * dpiScale;
        Views::DrawSegmentedBar(g, 0.0f, 0.0f, static_cast<float>(w) - 2.0f, segH, kTramTabs,
                                st->tramitesSubTab, st->tramitesSubTabHover, dpiScale, pal, hc, st->tramitesSubTabHits);

        const float bodyY = segH + 10.0f * dpiScale;
        const float bodyH = std::max(120.0f, static_cast<float>(h) - bodyY - 2.0f);
        Views::DrawSurface(g, 0.0f, bodyY, static_cast<float>(w) - 1.5f, bodyH, 10.0f * dpiScale,
                           pal.contentCard, hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.4f : 1.0f);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fTitle(&ff, 13.5f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSec(&ff, 12.6f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fBody(&ff, 12.0f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 10.8f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
        Gdiplus::StringFormat sf;
        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        const float pad = 16.0f * dpiScale;
        const float cw = std::max(140.0f, static_cast<float>(w) - pad * 2.0f - 10.0f * dpiScale);

        g.SetClip(Gdiplus::RectF(4.0f, bodyY + 4.0f, static_cast<float>(w) - 8.0f, bodyH - 8.0f));
        float y = bodyY + pad - static_cast<float>(st->tramitesScrollY);

        if (st->tramitesSubTab == 0) {
            // 0: Resumen, Inclusiones, Beca, Prematrícula, Inscripciones y DAR Preview
            const float gap = 12.0f * dpiScale;
            const float colW = std::max(160.0f, (cw - gap) * 0.5f);
            const float topCardH = 148.0f * dpiScale;

            auto drawMiniCard = [&](float x, float cy, float mw, float mh, const std::wstring& title,
                                    const std::wstring& badge, UiGlass::Rgba8 acc, UiGlass::IconId icon,
                                    const std::vector<std::wstring>& lines) {
                Views::DrawSurface(g, x, cy, mw, mh, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.9f);
                const float icSz = 26.0f * dpiScale;
                Views::DrawSurface(g, x + 12.0f * dpiScale, cy + 12.0f * dpiScale, icSz, icSz, 7.0f * dpiScale,
                                   Views::SoftTint(acc, dark), { 0, 0, 0, 0 }, 0.0f);
                UiGlass::DrawVectorIconGdiPlus(g, icon, x + 12.0f * dpiScale + icSz * 0.5f, cy + 12.0f * dpiScale + icSz * 0.5f,
                                               13.0f * dpiScale, UiGlass::ToGdip(acc), 1.5f * dpiScale);
                g.DrawString(title.c_str(), -1, &fTitle, Gdiplus::RectF(x + 46.0f * dpiScale, cy + 11.0f * dpiScale, mw - 56.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
                Views::DrawPill(g, x + 46.0f * dpiScale, cy + 32.0f * dpiScale, badge, dpiScale, Views::SoftTint(acc, dark), acc, hc, 150.0f);
                float ly = cy + 60.0f * dpiScale;
                for (const auto& ln : lines) {
                    if (ly + 16.0f * dpiScale > cy + mh - 8.0f * dpiScale) break;
                    g.DrawString(ln.c_str(), -1, &fBody, Gdiplus::RectF(x + 14.0f * dpiScale, ly, mw - 28.0f * dpiScale, 18.0f * dpiScale), &sf, &brSec);
                    ly += 20.0f * dpiScale;
                }
            };

            std::vector<std::wstring> incLines;
            const bool hasCamp = st->snap.inclusionCampaign.has_value();
            if (hasCamp) {
                incLines.push_back(L"Campaña: " + Utf8ToWide(st->snap.inclusionCampaign->name));
                incLines.push_back(L"Fechas: " + Utf8ToWide(st->snap.inclusionCampaign->start_date + " — " + st->snap.inclusionCampaign->end_date));
            } else {
                incLines.push_back(L"Sin periodo de inclusiones abierto actualmente.");
            }
            incLines.push_back(L"Solicitudes registradas: " + std::to_wstring(st->snap.inclusionApplications.size()));
            if (!st->snap.inclusionServiceStatus.empty() && st->snap.inclusionServiceStatus != "unknown") {
                const bool svcActive = (st->snap.inclusionServiceStatus == "active");
                incLines.push_back(svcActive ? L"Servicio de inclusiones: disponible" : L"Servicio de inclusiones: fuera de línea");
            }
            if (st->snap.scholarshipInfo) {
                incLines.push_back(L"Beca: " + Utf8ToWide(st->snap.scholarshipInfo->scholarship_type.empty() ? st->snap.scholarshipInfo->status : st->snap.scholarshipInfo->scholarship_type));
            }
            drawMiniCard(pad, y, colW, topCardH, L"Inclusiones y Beca",
                         hasCamp ? L"Campaña activa" : L"Fuera de periodo",
                         hasCamp ? pal.accentGreen : pal.accentBlue, UiGlass::IconId::Checklist, incLines);

            std::vector<std::wstring> preLines;
            if (st->snap.prematricula) {
                const auto& pm = *st->snap.prematricula;
                std::wstring pmTxt = L"Prematrícula: " + std::to_wstring(pm.courses.size()) + L" cursos";
                if (pm.max_credits > 0) pmTxt += L" · máx. " + std::to_wstring(pm.max_credits) + L" cr";
                if (!pm.requests.empty()) pmTxt += L" · " + std::to_wstring(pm.requests.size()) + L" solic.";
                preLines.push_back(std::move(pmTxt));
            } else {
                preLines.push_back(L"Sin ventana de prematrícula activa.");
            }
            const bool hasIns = !st->snap.gestorInscripcionEventsJson.empty() &&
                                st->snap.gestorInscripcionSelectorHtml.find("No hay inscripciones") == std::string::npos;
            preLines.push_back(hasIns ? L"Gestor de Inscripciones: convocatorias activas." : L"Gestor de Inscripciones: sin inscripciones activas.");
            preLines.push_back(L"Evaluación docente pendiente: " + std::to_wstring(st->snap.pendingPerformanceEvals.size()));
            preLines.push_back(L"Ferias de negocios consultadas: " +
                               std::to_wstring(st->snap.feriaNegocios ? st->snap.feriaNegocios->feria_rows.size() : 0));
            drawMiniCard(pad + colW + gap, y, colW, topCardH, L"Prematrícula e Inscripciones",
                         L"Trámites académicos", pal.accentTeal, UiGlass::IconId::CalendarGrid, preLines);
            y += topCardH + 14.0f * dpiScale;

            // Vista previa DAR + acciones directas (Inclusiones, Prematrícula, DAR)
            g.DrawString(L"Estado de matrícula en Admisión y Registro (DAR)", -1, &fSec,
                         Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 360.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
            {
                float rx = pad + cw;
                const float darBw = Views::DrawPill(g, rx - 148.0f * dpiScale, y, L"Sincronizar DAR", dpiScale,
                                                    Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 148.0f);
                st->tramitesActionRects.push_back({ { static_cast<LONG>(rx - 148.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(rx - 148.0f * dpiScale + darBw), static_cast<LONG>(y + 24.0f * dpiScale) },
                                                    "dar_sync" });
                rx -= 154.0f * dpiScale;
                const float incBw = Views::DrawPill(g, rx - 146.0f * dpiScale, y, L"+ Solicitar inclusión", dpiScale,
                                                    Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, hc, 146.0f);
                st->tramitesActionRects.push_back({ { static_cast<LONG>(rx - 146.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(rx - 146.0f * dpiScale + incBw), static_cast<LONG>(y + 24.0f * dpiScale) },
                                                    "inc_submit" });
                if (st->snap.prematricula && !st->snap.prematricula->courses.empty()) {
                    rx -= 152.0f * dpiScale;
                    const float pmBw = Views::DrawPill(g, rx - 150.0f * dpiScale, y, L"Guardar prematrícula", dpiScale,
                                                       Views::SoftTint(pal.accentTeal, dark), pal.accentTeal, hc, 150.0f);
                    st->tramitesActionRects.push_back({ { static_cast<LONG>(rx - 150.0f * dpiScale), static_cast<LONG>(y),
                                                          static_cast<LONG>(rx - 150.0f * dpiScale + pmBw), static_cast<LONG>(y + 24.0f * dpiScale) },
                                                        "premat_save" });
                }
            }
            y += 28.0f * dpiScale;
            if (!st->snap.inclusionApplications.empty()) {
                for (const auto& inc : st->snap.inclusionApplications) {
                    const float rh = 38.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                    std::string iline = inc.course_code + (inc.course_name.empty() ? "" : (" · " + inc.course_name)) +
                                        (inc.group.empty() ? "" : (" (Gr. " + inc.group + ")")) +
                                        (inc.status.empty() ? "" : (" — " + inc.status));
                    g.DrawString(Utf8ToWide(iline).c_str(), -1, &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 10.0f * dpiScale, cw - 110.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    if (!inc.id.empty()) {
                        const float cwBtn = Views::DrawPill(g, pad + cw - 88.0f * dpiScale, y + 8.0f * dpiScale, L"Cancelar", dpiScale,
                                                            Views::SoftTint(pal.accentRed, dark), pal.accentRed, hc, 80.0f);
                        st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 88.0f * dpiScale), static_cast<LONG>(y + 8.0f * dpiScale),
                                                              static_cast<LONG>(pad + cw - 88.0f * dpiScale + cwBtn), static_cast<LONG>(y + 30.0f * dpiScale) },
                                                            "inc_del:" + inc.id });
                    }
                    y += rh + 5.0f * dpiScale;
                }
                y += 6.0f * dpiScale;
            }
            if (!st->snap.darSyncPreview.empty()) {
                for (const auto& d : st->snap.darSyncPreview) {
                    const float rh = 36.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                    g.DrawString(Utf8ToWide(d.course_name).c_str(), -1, &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 9.0f * dpiScale, cw - 120.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    const bool isMem = SafeLowerAscii(d.status_text).find("miembro") != std::string::npos;
                    Views::DrawPill(g, pad + cw - 98.0f * dpiScale, y + 7.0f * dpiScale,
                                    Utf8ToWide(d.status_text.empty() ? "Sincronizado" : d.status_text), dpiScale,
                                    Views::SoftTint(isMem ? pal.accentGreen : pal.accentOrange, dark),
                                    isMem ? pal.accentGreen : pal.accentOrange, hc, 88.0f);
                    y += rh + 5.0f * dpiScale;
                }
            } else {
                g.DrawString(L"Sin vista previa DAR cargada. Presiona Sincronizar ahora para consultar tus cursos matriculados.", -1, &fSmall,
                             Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brTert);
                y += 24.0f * dpiScale;
            }
        } else if (st->tramitesSubTab == 1) {
            // 1: Control de Plagio (Upload + Analyze + Visibility + Report)
            g.DrawString(L"Control de Plagio · Documentos analizados", -1, &fSec,
                         Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 160.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
            {
                const float upW = Views::DrawPill(g, pad + cw - 144.0f * dpiScale, y, L"Subir a Plagio...", dpiScale,
                                                  Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 140.0f);
                st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 144.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(pad + cw - 144.0f * dpiScale + upW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                                    "plagio_upload" });
            }
            y += 26.0f * dpiScale;
            if (st->snap.plagiarismTree) {
                std::wstring hdr = L"Carpeta personal: " + Utf8ToWide(st->snap.plagiarismTree->my_files_name.empty() ? "Mis Documentos" : st->snap.plagiarismTree->my_files_name) +
                                   L"  ·  Total documentos: " + std::to_wstring(st->snap.plagiarismTree->documents.size());
                g.DrawString(hdr.c_str(), -1, &fSmall, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brSec);
                y += 24.0f * dpiScale;
                if (st->snap.plagiarismTree->documents.empty()) {
                    Views::DrawSurface(g, pad, y, cw, 64.0f * dpiScale, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                    g.DrawString(L"No hay documentos enviados actualmente en Control de Plagio. Usa «Subir a Plagio...» arriba para analizar un archivo.", -1, &fBody,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 22.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale), &sf, &brTert);
                    y += 72.0f * dpiScale;
                } else {
                    for (const auto& d : st->snap.plagiarismTree->documents) {
                        const float rh = 50.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                        const std::string friendlyStatus = FormatPlagiarismStatusDialog(d.status_code, d.status_text);
                        const UiGlass::Rgba8 pCol = (d.similarity_pct < 0.0) ? pal.labelSecondary
                                                    : (d.similarity_pct < 15.0 ? pal.accentGreen : (d.similarity_pct < 35.0 ? pal.accentOrange : pal.accentRed));
                        std::wstring simTxt = (d.similarity_pct >= 0.0) ? Utf8ToWide(FmtDouble(d.similarity_pct, 1) + "% sim.") : Utf8ToWide(friendlyStatus);
                        float rx = pad + cw - 10.0f * dpiScale;
                        const float repW = Views::DrawPill(g, rx - 86.0f * dpiScale, y + 13.0f * dpiScale, L"Ver reporte", dpiScale,
                                                           Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 86.0f);
                        if (!d.doc_uid.empty()) {
                            st->tramitesActionRects.push_back({ { static_cast<LONG>(rx - 86.0f * dpiScale), static_cast<LONG>(y + 13.0f * dpiScale),
                                                                  static_cast<LONG>(rx - 86.0f * dpiScale + repW), static_cast<LONG>(y + 35.0f * dpiScale) },
                                                                "plagio_open:" + d.doc_uid });
                        }
                        rx -= 92.0f * dpiScale;
                        if (!d.doc_uid.empty()) {
                            const float anW = Views::DrawPill(g, rx - 74.0f * dpiScale, y + 13.0f * dpiScale, L"Analizar", dpiScale,
                                                              Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, hc, 74.0f);
                            st->tramitesActionRects.push_back({ { static_cast<LONG>(rx - 74.0f * dpiScale), static_cast<LONG>(y + 13.0f * dpiScale),
                                                                  static_cast<LONG>(rx - 74.0f * dpiScale + anW), static_cast<LONG>(y + 35.0f * dpiScale) },
                                                                "plagio_analyze:" + d.doc_uid + "|" + d.doc_uid });
                            rx -= 80.0f * dpiScale;
                        }
                        Views::DrawPill(g, rx - 90.0f * dpiScale, y + 13.0f * dpiScale, simTxt, dpiScale,
                                        Views::SoftTint(pCol, dark), pCol, hc, 90.0f);
                        rx -= 96.0f * dpiScale;
                        const float titleAvail = std::max(80.0f, rx - (pad + 14.0f * dpiScale));
                        g.DrawString(Utf8ToWide(d.file_name).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, titleAvail, 18.0f * dpiScale), &sf, &brPrim);
                        std::string subTxt = "Carpeta personal";
                        if (!d.visibility.empty() && d.visibility != "12") subTxt += " · " + d.visibility;
                        else if (d.similarity_pct >= 0.0) subTxt += " · " + friendlyStatus;
                        g.DrawString(Utf8ToWide(subTxt).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 27.0f * dpiScale, titleAvail, 15.0f * dpiScale), &sf, &brTert);
                        y += rh + 6.0f * dpiScale;
                    }
                }
            } else {
                g.DrawString(L"Sincroniza tu cuenta para consultar el árbol de Control de Plagio.", -1, &fBody,
                             Gdiplus::RectF(pad, y, cw, 20.0f * dpiScale), &sf, &brTert);
                y += 28.0f * dpiScale;
            }
            if (!st->snap.plagiarismDocsByCourse.empty()) {
                y += 10.0f * dpiScale;
                g.DrawString(L"Documentos por curso (Control de Plagio)", -1, &fSec,
                             Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                y += 24.0f * dpiScale;
                for (const auto& [cpath, docs] : st->snap.plagiarismDocsByCourse) {
                    if (docs.empty()) continue;
                    std::string cLabel = cpath;
                    for (const auto& cm : st->snap.communities) {
                        if (cm.url == cpath) { cLabel = cm.name; break; }
                    }
                    g.DrawString(Utf8ToWide(cLabel).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad, y, cw, 16.0f * dpiScale), &sf, &brSec);
                    y += 18.0f * dpiScale;
                    for (const auto& d : docs) {
                        const float rh = 44.0f * dpiScale;
                        Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                        const UiGlass::Rgba8 pCol = (d.similarity_pct < 0.0) ? pal.labelSecondary
                                                    : (d.similarity_pct < 15.0 ? pal.accentGreen : (d.similarity_pct < 35.0 ? pal.accentOrange : pal.accentRed));
                        std::wstring simTxt = (d.similarity_pct >= 0.0) ? Utf8ToWide(FmtDouble(d.similarity_pct, 1) + "% sim.")
                                                                        : Utf8ToWide(d.status.empty() ? "Revisado" : d.status);
                        Views::DrawPill(g, pad + cw - 196.0f * dpiScale, y + 11.0f * dpiScale, simTxt, dpiScale,
                                        Views::SoftTint(pCol, dark), pCol, hc, 92.0f);
                        const std::string uid = !d.item_id.empty() ? d.item_id : d.doc_id;
                        const float rw = Views::DrawPill(g, pad + cw - 96.0f * dpiScale, y + 11.0f * dpiScale, L"Ver reporte", dpiScale,
                                                         Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 86.0f);
                        if (!uid.empty()) {
                            st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 96.0f * dpiScale), static_cast<LONG>(y + 11.0f * dpiScale),
                                                                  static_cast<LONG>(pad + cw - 96.0f * dpiScale + rw), static_cast<LONG>(y + 33.0f * dpiScale) },
                                                                "plagio_open:" + uid });
                        }
                        g.DrawString(Utf8ToWide(d.file_name).c_str(), -1, &fBody,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 6.0f * dpiScale, cw - 216.0f * dpiScale, 17.0f * dpiScale), &sf, &brPrim);
                        g.DrawString(Utf8ToWide(d.package_from.empty() ? d.status : (d.package_from + " · " + d.status)).c_str(),
                                     -1, &fSmall,
                                     Gdiplus::RectF(pad + 12.0f * dpiScale, y + 24.0f * dpiScale, cw - 216.0f * dpiScale, 15.0f * dpiScale), &sf, &brTert);
                        y += rh + 5.0f * dpiScale;
                    }
                    y += 4.0f * dpiScale;
                }
            }
        } else if (st->tramitesSubTab == 2) {
            // 2: Modelo de Usuario (tda-um HTML sections)
            g.DrawString(L"Modelo de Usuario TEC (Estilos de aprendizaje y perfil psicopedagógico)", -1, &fSec,
                         Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
            y += 26.0f * dpiScale;
            if (st->snap.userModel && !st->snap.userModel->dimensions.empty()) {
                for (const auto& dim : st->snap.userModel->dimensions) {
                    const size_t nRows = std::min<size_t>(dim.metrics.size(), 6);
                    const float ch = (34.0f + static_cast<float>(nRows) * 20.0f) * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, ch, 9.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                    g.DrawString(Utf8ToWide(dim.title.empty() ? dim.dimension_key : dim.title).c_str(), -1, &fSec,
                                 Gdiplus::RectF(pad + 14.0f * dpiScale, y + 10.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                 &sf, &brPrim);
                    float my = y + 30.0f * dpiScale;
                    for (size_t mi = 0; mi < nRows; ++mi) {
                        const auto& kv = dim.metrics[mi];
                        std::string line = kv.first + ": " + kv.second;
                        g.DrawString(Utf8ToWide(line).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(pad + 14.0f * dpiScale, my, cw - 28.0f * dpiScale, 18.0f * dpiScale),
                                     &sf, &brSec);
                        my += 20.0f * dpiScale;
                    }
                    y += ch + 8.0f * dpiScale;
                }
            } else {
                g.DrawString(L"Sin dimensiones del Modelo de Usuario cargadas. Presiona Sincronizar ahora.", -1, &fBody,
                             Gdiplus::RectF(pad, y, cw, 20.0f * dpiScale), &sf, &brTert);
                y += 28.0f * dpiScale;
            }
        } else if (st->tramitesSubTab == 3) {
            // 3: Evaluación Docente y Feria de Negocios
            g.DrawString(L"Evaluación de Desempeño Docente", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
            y += 24.0f * dpiScale;
            if (!st->snap.pendingPerformanceEvals.empty()) {
                for (const auto& pe : st->snap.pendingPerformanceEvals) {
                    const float rh = 44.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.85f);
                    g.DrawString(Utf8ToWide(pe.course_name + "  ·  " + pe.teacher_name).c_str(), -1, &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - 120.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
                    g.DrawString(Utf8ToWide(pe.form_name + (pe.deadline.empty() ? "" : ("  ·  Vence: " + pe.deadline))).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 25.0f * dpiScale, cw - 120.0f * dpiScale, 15.0f * dpiScale), &sf, &brTert);
                    Views::DrawPill(g, pad + cw - 96.0f * dpiScale, y + 11.0f * dpiScale, Utf8ToWide(pe.status.empty() ? "Pendiente" : pe.status),
                                    dpiScale, Views::SoftTint(pal.accentOrange, dark), pal.accentOrange, hc, 86.0f);
                    y += rh + 6.0f * dpiScale;
                }
            } else {
                Views::DrawSurface(g, pad, y, cw, 44.0f * dpiScale, 8.0f * dpiScale, Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, 0.7f);
                g.DrawString(L"Evaluación docente al día · no tienes cuestionarios pendientes en este momento.", -1, &fBody,
                             Gdiplus::RectF(pad + 14.0f * dpiScale, y + 13.0f * dpiScale, cw - 28.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
                y += 52.0f * dpiScale;
            }
            if (!st->snap.performanceEvalForms.empty()) {
                const auto& pf = st->snap.performanceEvalForms.front();
                std::wstring fsum = L"Instrumento oficial: " + Utf8ToWide(pf.name) + L" (" + std::to_wstring(pf.sections.size()) + L" secciones)";
                g.DrawString(fsum.c_str(), -1, &fSmall, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brSec);
                y += 22.0f * dpiScale;
                for (const auto& sec : pf.sections) {
                    std::wstring sl = L"• " + Utf8ToWide(sec.name) + L" (" + std::to_wstring(sec.items.size()) + L" criterios)";
                    g.DrawString(sl.c_str(), -1, &fSmall, Gdiplus::RectF(pad + 10.0f * dpiScale, y, cw - 20.0f * dpiScale, 16.0f * dpiScale), &sf, &brTert);
                    y += 18.0f * dpiScale;
                }
            }
            y += 12.0f * dpiScale;

            // Feria de Negocios
            g.DrawString(L"Feria de Negocios TEC", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
            y += 24.0f * dpiScale;
            if (st->snap.feriaNegocios) {
                if (!st->snap.feriaNegocios->categories.empty()) {
                    float cx = pad;
                    for (const auto& cat : st->snap.feriaNegocios->categories) {
                        if (cx > pad + cw - 90.0f * dpiScale) break;
                        cx += Views::DrawPill(g, cx, y, Utf8ToWide(cat.name), dpiScale,
                                              Views::SoftTint(pal.accentIndigo, dark), pal.accentIndigo, hc, 140.0f) + 6.0f * dpiScale;
                    }
                    y += 28.0f * dpiScale;
                }
                for (const auto& fr : st->snap.feriaNegocios->feria_rows) {
                    const float rh = 42.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                    g.DrawString(Utf8ToWide(fr.name).c_str(), -1, &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 6.0f * dpiScale, cw - 110.0f * dpiScale, 17.0f * dpiScale), &sf, &brPrim);
                    g.DrawString(Utf8ToWide("Inicio: " + fr.start_date + "  ·  Cierre: " + fr.end_date).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 23.0f * dpiScale, cw - 110.0f * dpiScale, 15.0f * dpiScale), &sf, &brTert);
                    if (!fr.bases_url.empty()) {
                        const float bw = Views::DrawPill(g, pad + cw - 86.0f * dpiScale, y + 10.0f * dpiScale, L"Bases", dpiScale,
                                                         Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 76.0f);
                        st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 86.0f * dpiScale), static_cast<LONG>(y + 10.0f * dpiScale),
                                                              static_cast<LONG>(pad + cw - 86.0f * dpiScale + bw), static_cast<LONG>(y + 32.0f * dpiScale) },
                                                            "url:" + fr.bases_url });
                    }
                    {
                        const std::string fIdStr = st->snap.feriaNegocios->ids.pack > 0
                                                       ? std::to_string(st->snap.feriaNegocios->ids.pack)
                                                       : fr.name;
                        const float rgW = Views::DrawPill(g, pad + cw - 224.0f * dpiScale, y + 10.0f * dpiScale, L"Registrar proyecto", dpiScale,
                                                          Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, hc, 130.0f);
                        st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 224.0f * dpiScale), static_cast<LONG>(y + 10.0f * dpiScale),
                                                              static_cast<LONG>(pad + cw - 224.0f * dpiScale + rgW), static_cast<LONG>(y + 32.0f * dpiScale) },
                                                            "fair_register:" + fIdStr });
                    }
                    y += rh + 5.0f * dpiScale;
                }
            } else {
                g.DrawString(L"Sin ferias de negocios cargadas.", -1, &fSmall, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brTert);
                y += 24.0f * dpiScale;
            }
        } else {
            // 4: Radar de Cambios y Monitoreo Automático + Bandeja TEC + Consentimiento + Suscripciones + Escuelas
            g.DrawString(L"Radar de cambios y monitoreo automático (0% uso en reposo)", -1, &fSec,
                         Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 260.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
            {
                const float rdW = Views::DrawPill(g, pad + cw - 118.0f * dpiScale, y, L"Marcar todo leído", dpiScale,
                                                  pal.sidebarBg, pal.labelSecondary, hc, 118.0f);
                st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 118.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(pad + cw - 118.0f * dpiScale + rdW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                                    "radar_read_all" });
                const float scW = Views::DrawPill(g, pad + cw - 252.0f * dpiScale, y, L"Buscar cambios ahora", dpiScale,
                                                  Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 128.0f);
                st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 252.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(pad + cw - 252.0f * dpiScale + scW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                                    "radar_sync" });
            }
            y += 28.0f * dpiScale;
            // Auto-monitor interval selector pills (0 / 5 / 15 / 30 / 60 min)
            {
                static const int kInts[5] = { 0, 5, 15, 30, 60 };
                static const wchar_t* kIntLbls[5] = { L"Manual", L"Cada 5 min", L"Cada 15 min", L"Cada 30 min", L"Cada 60 min" };
                g.DrawString(L"Frecuencia del monitor y avisos de Windows:", -1, &fSmall,
                             Gdiplus::RectF(pad, y + 3.0f * dpiScale, 240.0f * dpiScale, 16.0f * dpiScale), &sf, &brSec);
                float ix = pad + 244.0f * dpiScale;
                for (int ii = 0; ii < 5; ++ii) {
                    const bool active = (st->snap.monitorIntervalMinutes == kInts[ii]);
                    const float pw = Views::DrawPill(g, ix, y, kIntLbls[ii], dpiScale,
                                                     active ? pal.accentBlue : pal.sidebarBg,
                                                     active ? UiGlass::Rgba8{ 255, 255, 255, 255 } : pal.labelSecondary,
                                                     hc, 94.0f);
                    st->tramitesActionRects.push_back({ { static_cast<LONG>(ix), static_cast<LONG>(y),
                                                          static_cast<LONG>(ix + pw), static_cast<LONG>(y + 22.0f * dpiScale) },
                                                        "mon_int:" + std::to_string(kInts[ii]) });
                    ix += pw + 5.0f * dpiScale;
                }
                y += 30.0f * dpiScale;
            }
            if (!st->snap.activityFeed.empty()) {
                for (size_t ci = 0; ci < st->snap.activityFeed.size() && ci < 12; ++ci) {
                    const auto& ev = st->snap.activityFeed[ci];
                    const float rh = 46.0f * dpiScale;
                    const UiGlass::Rgba8 kCol = FriendlyChangeKindColor(ev.kind, pal);
                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale,
                                       ev.unread ? Views::SoftTint(kCol, dark) : pal.contentCardElevated,
                                       ev.unread ? kCol : pal.borderSubtle, 0.85f);
                    Views::DrawPill(g, pad + cw - 118.0f * dpiScale, y + 12.0f * dpiScale,
                                    Utf8ToWide(FriendlyChangeKindLabel(ev.kind)), dpiScale,
                                    Views::SoftTint(kCol, dark), kCol, hc, 110.0f);
                    g.DrawString(Utf8ToWide(ev.title).c_str(), -1, ev.unread ? &fSec : &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 6.0f * dpiScale, cw - 138.0f * dpiScale, 17.0f * dpiScale), &sf, &brPrim);
                    std::string det = ev.space_name.empty()
                                          ? ev.detail
                                          : ((ev.detail.empty() || ev.detail == ev.space_name)
                                                 ? ev.space_name
                                                 : (ev.detail.rfind(ev.space_name, 0) == 0 ? ev.detail : (ev.space_name + "  ·  " + ev.detail)));
                    const std::string ts = FormatFriendlyTimestampDialog(ev.detected_at_utc);
                    if (!ts.empty()) det += "  ·  " + ts;
                    g.DrawString(Utf8ToWide(det).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 25.0f * dpiScale, cw - 138.0f * dpiScale, 15.0f * dpiScale), &sf, &brSec);
                    y += rh + 5.0f * dpiScale;
                }
            } else {
                Views::DrawSurface(g, pad, y, cw, 40.0f * dpiScale, 8.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.8f);
                g.DrawString(L"Sin cambios nuevos desde la última sincronización · CuadernU avisará cuando se publique una nota o archivo.", -1, &fSmall,
                             Gdiplus::RectF(pad + 12.0f * dpiScale, y + 12.0f * dpiScale, cw - 24.0f * dpiScale, 16.0f * dpiScale), &sf, &brTert);
                y += 48.0f * dpiScale;
            }
            y += 10.0f * dpiScale;

            g.DrawString(L"Bandeja de notificaciones de TEC Digital", -1, &fSec,
                         Gdiplus::RectF(pad, y + 2.0f * dpiScale, cw - 130.0f * dpiScale, 18.0f * dpiScale), &sf, &brPrim);
            if (st->snap.notifications && !st->snap.notifications->notifications.empty()) {
                const float daW = Views::DrawPill(g, pad + cw - 112.0f * dpiScale, y, L"Eliminar todas", dpiScale,
                                                  Views::SoftTint(pal.accentRed, dark), pal.accentRed, hc, 108.0f);
                st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 112.0f * dpiScale), static_cast<LONG>(y),
                                                      static_cast<LONG>(pad + cw - 112.0f * dpiScale + daW), static_cast<LONG>(y + 22.0f * dpiScale) },
                                                    "notif_del_all" });
            }
            y += 26.0f * dpiScale;
            if (st->snap.notifications && !st->snap.notifications->notifications.empty()) {
                for (size_t ni = 0; ni < st->snap.notifications->notifications.size() && ni < 15; ++ni) {
                    const auto& n = st->snap.notifications->notifications[ni];
                    const bool unread = (n.state == "U");
                    const float rh = 48.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 8.0f * dpiScale,
                                       unread ? Views::SoftTint(pal.accentBlue, dark) : pal.contentCardElevated,
                                       unread ? pal.accentBlue : pal.borderSubtle, 0.85f);
                    float rightUsed = 10.0f * dpiScale;
                    if (!n.notification_id.empty()) {
                        const float dw = Views::DrawPill(g, pad + cw - 82.0f * dpiScale, y + 13.0f * dpiScale, L"Eliminar", dpiScale,
                                                         Views::SoftTint(pal.accentRed, dark), pal.accentRed, hc, 74.0f);
                        st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 82.0f * dpiScale), static_cast<LONG>(y + 13.0f * dpiScale),
                                                              static_cast<LONG>(pad + cw - 82.0f * dpiScale + dw), static_cast<LONG>(y + 35.0f * dpiScale) },
                                                            "notif_del:" + n.notification_id });
                        rightUsed += 84.0f * dpiScale;
                        if (unread) {
                            const float rw = Views::DrawPill(g, pad + cw - 178.0f * dpiScale, y + 13.0f * dpiScale, L"Marcar leída", dpiScale,
                                                             Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, hc, 90.0f);
                            st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 178.0f * dpiScale), static_cast<LONG>(y + 13.0f * dpiScale),
                                                                  static_cast<LONG>(pad + cw - 178.0f * dpiScale + rw), static_cast<LONG>(y + 35.0f * dpiScale) },
                                                                "notif_read:" + n.notification_id });
                            rightUsed += 96.0f * dpiScale;
                        }
                    }
                    g.DrawString(Utf8ToWide(n.title).c_str(), -1, unread ? &fSec : &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 7.0f * dpiScale, cw - rightUsed - 16.0f * dpiScale, 17.0f * dpiScale), &sf, &brPrim);
                    g.DrawString(Utf8ToWide(n.text + (n.creation_date.empty() ? "" : ("  ·  " + n.creation_date))).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 26.0f * dpiScale, cw - rightUsed - 16.0f * dpiScale, 15.0f * dpiScale), &sf, &brSec);
                    y += rh + 6.0f * dpiScale;
                }
            } else {
                g.DrawString(L"No hay notificaciones pendientes.", -1, &fSmall, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brTert);
                y += 24.0f * dpiScale;
            }
            y += 10.0f * dpiScale;

            // Consentimiento Informado y Reloj Oficial del Servidor
            if (!st->snap.consentStatusByModule.empty() || !st->snap.serverDateTimeIso.empty()) {
                g.DrawString(L"Consentimiento informado y reloj del servidor", -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
                y += 24.0f * dpiScale;
                for (const auto& [modKey, statusTxt] : st->snap.consentStatusByModule) {
                    const float rh = 34.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.75f);
                    std::wstring modLbl = (modKey == "tda-um") ? L"Modelo de Usuario"
                                          : ((modKey == "tda-control-plagio") ? L"Control de Plagio" : Utf8ToWide(modKey));
                    g.DrawString(modLbl.c_str(), -1, &fBody,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 8.0f * dpiScale, cw - 120.0f * dpiScale, 17.0f * dpiScale), &sf, &brPrim);
                    const bool okC = SafeLowerAscii(statusTxt).find("acept") != std::string::npos;
                    Views::DrawPill(g, pad + cw - 102.0f * dpiScale, y + 6.0f * dpiScale, Utf8ToWide(statusTxt), dpiScale,
                                    Views::SoftTint(okC ? pal.accentGreen : pal.accentBlue, dark),
                                    okC ? pal.accentGreen : pal.accentBlue, hc, 92.0f);
                    if (!okC) {
                        const float acW = Views::DrawPill(g, pad + cw - 184.0f * dpiScale, y + 6.0f * dpiScale, L"Aceptar", dpiScale,
                                                          Views::SoftTint(pal.accentGreen, dark), pal.accentGreen, hc, 74.0f);
                        st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 184.0f * dpiScale), static_cast<LONG>(y + 6.0f * dpiScale),
                                                              static_cast<LONG>(pad + cw - 184.0f * dpiScale + acW), static_cast<LONG>(y + 28.0f * dpiScale) },
                                                            "consent_accept:" + modKey });
                    }
                    y += rh + 4.0f * dpiScale;
                }
                if (!st->snap.serverDateTimeIso.empty()) {
                    const float rh = 32.0f * dpiScale;
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.7f);
                    std::wstring clk = L"Última hora oficial TEC (UTC): " + Utf8ToWide(st->snap.serverDateTimeIso) +
                                       L"  ·  Desfase local: " + std::to_wstring(st->snap.serverTimeOffsetSec) + L" s";
                    g.DrawString(clk.c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 8.0f * dpiScale, cw - 24.0f * dpiScale, 16.0f * dpiScale), &sf, &brSec);
                    y += rh + 4.0f * dpiScale;
                }
                y += 10.0f * dpiScale;
            }

            // Suscripciones (/notifications/manage)
            std::wstring subHdr = L"Suscripciones activas de correo y avisos (" +
                                  std::to_wstring(st->snap.notificationSubscriptions.size()) + L")";
            g.DrawString(subHdr.c_str(), -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
            y += 24.0f * dpiScale;
            for (size_t si = 0; si < st->snap.notificationSubscriptions.size() && si < 40; ++si) {
                const auto& sub = st->snap.notificationSubscriptions[si];
                const float rh = 34.0f * dpiScale;
                if (y + rh >= bodyY && y <= bodyY + bodyH) {
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.75f);
                    g.DrawString(Utf8ToWide(sub.community + "  ·  " + sub.notice_type + "  ·  " + sub.element).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 9.0f * dpiScale, cw - 110.0f * dpiScale, 16.0f * dpiScale), &sf, &brPrim);
                    Views::DrawPill(g, pad + cw - 96.0f * dpiScale, y + 6.0f * dpiScale, Utf8ToWide(sub.frequency), dpiScale,
                                    pal.sidebarBg, pal.labelSecondary, hc, 86.0f);
                }
                y += rh + 4.0f * dpiScale;
            }
            y += 10.0f * dpiScale;

            // Catálogo de Escuelas TEC (/dotlrn/classes/)
            std::wstring schHdr = L"Catálogo de escuelas y departamentos TEC (" +
                                  std::to_wstring(st->snap.schoolCatalog.size()) + L")";
            g.DrawString(schHdr.c_str(), -1, &fSec, Gdiplus::RectF(pad, y, cw, 18.0f * dpiScale), &sf, &brPrim);
            y += 24.0f * dpiScale;
            for (size_t sci = 0; sci < st->snap.schoolCatalog.size() && sci < 35; ++sci) {
                const auto& sc = st->snap.schoolCatalog[sci];
                const float rh = 32.0f * dpiScale;
                if (y + rh >= bodyY && y <= bodyY + bodyH) {
                    Views::DrawSurface(g, pad, y, cw, rh, 7.0f * dpiScale, pal.contentCardElevated, pal.borderSubtle, 0.7f);
                    g.DrawString(Utf8ToWide(sc.dept_code + "  —  " + sc.name).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad + 12.0f * dpiScale, y + 8.0f * dpiScale, cw - 90.0f * dpiScale, 16.0f * dpiScale), &sf, &brPrim);
                    if (!sc.url.empty()) {
                        const float bw = Views::DrawPill(g, pad + cw - 74.0f * dpiScale, y + 5.0f * dpiScale, L"Abrir", dpiScale,
                                                         Views::SoftTint(pal.accentBlue, dark), pal.accentBlue, hc, 64.0f);
                        st->tramitesActionRects.push_back({ { static_cast<LONG>(pad + cw - 74.0f * dpiScale), static_cast<LONG>(y + 5.0f * dpiScale),
                                                              static_cast<LONG>(pad + cw - 74.0f * dpiScale + bw), static_cast<LONG>(y + 27.0f * dpiScale) },
                                                            "url:" + sc.url });
                    }
                }
                y += rh + 4.0f * dpiScale;
            }
        }

        const float totalH = (y + static_cast<float>(st->tramitesScrollY)) - bodyY + 14.0f * dpiScale;
        st->tramitesMaxScroll = std::max(0, static_cast<int>(totalH - bodyH));
        st->tramitesScrollY = std::clamp(st->tramitesScrollY, 0, st->tramitesMaxScroll);
        g.ResetClip();
        Views::DrawScrollbarThumb(g, w, h, st->tramitesScrollY, st->tramitesMaxScroll, dpiScale, pal, bodyY + 6.0f);
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK TramitesCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) { PaintTramitesCanvas(hwnd, st); return 0; }
    if (msg == WM_MOUSEWHEEL && st) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->tramitesScrollY = std::clamp(st->tramitesScrollY - (delta / 2), 0, st->tramitesMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        int hov = -1;
        for (const auto& h : st->tramitesSubTabHits) {
            if (PtInRect(&h.rc, pt)) { hov = h.index; break; }
        }
        if (hov != st->tramitesSubTabHover) {
            st->tramitesSubTabHover = hov;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        for (const auto& h : st->tramitesSubTabHits) {
            if (PtInRect(&h.rc, pt)) {
                st->tramitesSubTab = h.index;
                st->tramitesScrollY = 0;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
        for (const auto& [rc, act] : st->tramitesActionRects) {
            if (!PtInRect(&rc, pt)) continue;
            if (act == "dar_sync") {
                if (st->store && !st->workerBusy.exchange(true)) {
                    SetWindowTextW(st->hProgressBanner, L"Actualizando matrícula desde DAR...");
                    if (st->workerThread.joinable()) st->workerThread.join();
                    st->workerThread = std::thread([st]() {
                        auto rr = st->store->SyncExpedienteDarCourses(true);
                        {
                            std::lock_guard<std::mutex> lk(st->workResultMutex);
                            st->pendingWorkKind = 2;
                            st->pendingWorkStatus = rr.ok ? "Lista de cursos actualizada desde DAR."
                                                          : ("DAR: " + rr.error_message);
                        }
                        st->workerBusy.store(false);
                        PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, rr.ok ? 1 : 0, 0);
                    });
                }
                return 0;
            }
            if (act == "radar_sync") {
                StartAsyncSync(st);
                return 0;
            }
            if (act == "radar_read_all" && st->store) {
                st->store->MarkAllChangesRead();
                st->snap = st->store->Snapshot();
                SetWindowTextW(st->hProgressBanner, L"Cambios del radar marcados como leídos.");
                InvalidateRect(hwnd, nullptr, FALSE);
                if (st->hSummaryCanvas) InvalidateRect(st->hSummaryCanvas, nullptr, FALSE);
                return 0;
            }
            if (act.rfind("mon_int:", 0) == 0 && st->store) {
                const int mins = std::atoi(act.substr(8).c_str());
                StartAutoMonitorBridge(st, mins);
                st->snap = st->store->Snapshot();
                SetWindowTextW(st->hProgressBanner,
                               mins > 0 ? Utf8ToWide("Monitor automático activado: revisará cada " + std::to_string(mins) + " minutos.").c_str()
                                        : L"Monitor automático pausado (sincronización manual).");
                InvalidateRect(hwnd, nullptr, FALSE);
                if (st->hSummaryCanvas) InvalidateRect(st->hSummaryCanvas, nullptr, FALSE);
                return 0;
            }
            if (act == "notif_del_all" && st->store) {
                if (MessageBoxW(st->hwnd, L"¿Deseas eliminar TODAS las notificaciones de tu bandeja de TEC Digital?",
                                L"Eliminar todas las notificaciones", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
                    auto r = st->store->DeleteAllUserNotifications(true);
                    if (r.ok && r.value) {
                        st->snap = st->store->Snapshot();
                        SetWindowTextW(st->hProgressBanner, L"Todas las notificaciones fueron eliminadas.");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Notificaciones: " + r.error_message).c_str());
                    }
                }
                return 0;
            }
            if (act.rfind("consent_accept:", 0) == 0 && st->store) {
                auto r = st->store->SubmitConsentAnswer(act.substr(15), true);
                if (r.ok) {
                    st->snap = st->store->Snapshot();
                    SetWindowTextW(st->hProgressBanner, L"Consentimiento informado actualizado en TEC Digital.");
                    InvalidateRect(hwnd, nullptr, FALSE);
                } else {
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide("Consentimiento: " + r.error_message).c_str());
                }
                return 0;
            }
            if (act == "inc_submit" && st->store) {
                std::wstring entry;
                if (ShowSimpleTextPromptDialog(st->hwnd, L"Solicitar inclusión en TEC Digital",
                                               L"Indica Código, Grupo y Motivo separados por | (ej. FI2103|51|Choque de horario):",
                                               L"", entry)) {
                    const std::string s = WideToUtf8(entry);
                    const size_t p1 = s.find('|');
                    const size_t p2 = (p1 == std::string::npos) ? std::string::npos : s.find('|', p1 + 1);
                    const std::string code = (p1 == std::string::npos) ? s : s.substr(0, p1);
                    const std::string grp = (p1 == std::string::npos) ? "1" : (p2 == std::string::npos ? s.substr(p1 + 1) : s.substr(p1 + 1, p2 - p1 - 1));
                    const std::string cmt = (p2 == std::string::npos) ? "" : s.substr(p2 + 1);
                    const std::string campId = st->snap.inclusionCampaign ? st->snap.inclusionCampaign->id : "";
                    auto r = st->store->SubmitInclusionRequest(campId, code, grp, cmt);
                    SetWindowTextW(st->hProgressBanner, r.ok ? L"Solicitud de inclusión enviada a TEC Digital."
                                                             : Utf8ToWide("Inclusión: " + r.error_message).c_str());
                    if (r.ok) { st->snap = st->store->Snapshot(); InvalidateRect(hwnd, nullptr, FALSE); }
                }
                return 0;
            }
            if (act.rfind("inc_del:", 0) == 0 && st->store) {
                if (MessageBoxW(st->hwnd, L"¿Deseas cancelar esta solicitud de inclusión en TEC Digital?",
                                L"Cancelar solicitud de inclusión", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES) {
                    auto r = st->store->DeleteInclusionRequest(act.substr(8), true);
                    if (r.ok) {
                        st->snap = st->store->Snapshot();
                        SetWindowTextW(st->hProgressBanner, L"Solicitud de inclusión cancelada.");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Cancelar inclusión: " + r.error_message).c_str());
                    }
                }
                return 0;
            }
            if (act == "premat_save" && st->store && st->snap.prematricula) {
                std::string codes;
                for (const auto& c : st->snap.prematricula->courses) {
                    if (!c.course_code.empty()) { if (!codes.empty()) codes += ","; codes += c.course_code; }
                }
                const std::string curId = (st->snap.studentCareer && st->snap.studentCareer->plan > 0)
                                              ? std::to_string(st->snap.studentCareer->plan) : "";
                auto r = st->store->SavePrematriculaSelection("active", curId, codes);
                SetWindowTextW(st->hProgressBanner, r.ok ? L"Selección de prematrícula guardada en TEC Digital."
                                                         : Utf8ToWide("Prematrícula: " + r.error_message).c_str());
                return 0;
            }
            if (act == "plagio_upload" && st->store) {
                std::wstring inPath;
                if (PickOpenFileW(st->hwnd, L"Documentos (*.pdf;*.docx;*.doc;*.txt)\0*.pdf;*.docx;*.doc;*.txt\0Todos (*.*)\0*.*\0", inPath)) {
                    const std::string fid = st->snap.plagiarismTree ? st->snap.plagiarismTree->my_files_folder_id : "";
                    SetWindowTextW(st->hProgressBanner, L"Subiendo documento a Control de Plagio...");
                    auto r = st->store->UploadPlagiarismFile(fid, WideToUtf8(inPath));
                    if (r.ok) {
                        st->snap = st->store->Snapshot();
                        SetWindowTextW(st->hProgressBanner, L"Documento subido a Control de Plagio.");
                        InvalidateRect(hwnd, nullptr, FALSE);
                    } else {
                        SetWindowTextW(st->hProgressBanner, Utf8ToWide("Control de Plagio: " + r.error_message).c_str());
                    }
                }
                return 0;
            }
            if (act.rfind("plagio_analyze:", 0) == 0 && st->store) {
                const std::string payload = act.substr(15);
                const size_t sep = payload.find('|');
                const std::string uid = (sep == std::string::npos) ? payload : payload.substr(0, sep);
                const std::string iid = (sep == std::string::npos) ? "" : payload.substr(sep + 1);
                auto r = st->store->SubmitPlagiarismEphorus(uid, iid);
                SetWindowTextW(st->hProgressBanner, r.ok ? L"Análisis de similitud solicitado en Control de Plagio."
                                                         : Utf8ToWide("Analizar plagio: " + r.error_message).c_str());
                if (r.ok) { st->snap = st->store->Snapshot(); InvalidateRect(hwnd, nullptr, FALSE); }
                return 0;
            }
            if (act.rfind("fair_register:", 0) == 0 && st->store) {
                std::wstring projTitle;
                if (ShowSimpleTextPromptDialog(st->hwnd, L"Registrar proyecto en Feria de Negocios",
                                               L"Indica el título del proyecto (o Título|Descripción):", L"", projTitle)) {
                    const std::string s = WideToUtf8(projTitle);
                    const size_t sep = s.find('|');
                    const std::string t = (sep == std::string::npos) ? s : s.substr(0, sep);
                    const std::string desc = (sep == std::string::npos) ? "" : s.substr(sep + 1);
                    const std::string catId = (st->snap.feriaNegocios && !st->snap.feriaNegocios->categories.empty())
                                                  ? std::to_string(st->snap.feriaNegocios->categories.front().id) : "1";
                    auto r = st->store->RegisterFairProject(act.substr(14), t, desc, catId, "CA");
                    SetWindowTextW(st->hProgressBanner, r.ok ? L"Proyecto registrado en Feria de Negocios TEC."
                                                             : Utf8ToWide("Feria de Negocios: " + r.error_message).c_str());
                }
                return 0;
            }
            if (act.rfind("url:", 0) == 0) {
                OpenSafeUrl(st->hwnd, act.substr(4));
                return 0;
            }
            if (act.rfind("plagio_open:", 0) == 0 && st->store) {
                auto rep = st->store->FetchPlagiarismReportUrl(act.substr(12));
                if (rep.ok && DocStudio::IsSafeExternalLinkUrl(rep.value)) OpenSafeUrl(st->hwnd, rep.value);
                else OpenSafeUrl(st->hwnd, "https://tecdigital.tec.ac.cr/tda-control-plagio");
                return 0;
            }
            if (act.rfind("notif_read:", 0) == 0 && st->store) {
                auto r = st->store->MarkNotificationAsRead(act.substr(11), true);
                if (r.ok && r.value) {
                    st->snap = st->store->Snapshot();
                    SetWindowTextW(st->hProgressBanner, L"Notificación marcada como leída.");
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
            if (act.rfind("notif_del:", 0) == 0 && st->store) {
                auto r = st->store->DeleteUserNotification(act.substr(10), true);
                if (r.ok && r.value) {
                    st->snap = st->store->Snapshot();
                    SetWindowTextW(st->hProgressBanner, L"Notificación eliminada.");
                    InvalidateRect(hwnd, nullptr, FALSE);
                }
                return 0;
            }
        }
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

            const bool hasColorLegend = !st->snap.curriculumColors.color_codes.empty();
            const float legendBarH = hasColorLegend ? (36.0f * dpiScale) : 0.0f;
            const float clipBottom = std::max(40.0f, static_cast<float>(h) - legendBarH - 3.0f);
            g.SetClip(Gdiplus::RectF(3.0f, 3.0f, std::max(40.0f, static_cast<float>(w) - 6.0f), clipBottom - 3.0f));

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
                const std::string lowId = SafeLowerAscii(graph.levels[li].id);
                const bool hasPrefix = (lowId.rfind("semestre", 0) == 0 ||
                                        lowId.rfind("nivel", 0) == 0 ||
                                        lowId.rfind("bloque", 0) == 0 ||
                                        lowId.rfind("cuatrim", 0) == 0);
                std::wstring hdr = hasPrefix ? Utf8ToWide(graph.levels[li].id) : (L"Semestre " + Utf8ToWide(graph.levels[li].id));
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
            g.ResetClip();
            // Official TEC Curriculum Color Codes Legend docked in dedicated footer bar (zero overlap with course cards)
            if (hasColorLegend) {
                const float barY = static_cast<float>(h) - legendBarH - 1.5f;
                Gdiplus::SolidBrush barBg(UiGlass::ToGdip(pal.contentCard));
                g.FillRectangle(&barBg, 2.0f, barY, static_cast<float>(w) - 4.0f, legendBarH);
                Gdiplus::Pen topSep(UiGlass::ToGdip(pal.separator), 1.0f);
                g.DrawLine(&topSep, 8.0f, barY, static_cast<float>(w) - 8.0f, barY);

                float lx = 12.0f * dpiScale;
                const float ly = barY + (legendBarH - 22.0f * dpiScale) * 0.5f;
                for (size_t ci = 0; ci < st->snap.curriculumColors.color_codes.size() && ci < 6; ++ci) {
                    const auto& cc = st->snap.curriculumColors.color_codes[ci];
                    if (lx > static_cast<float>(w) - 110.0f * dpiScale) break;
                    const std::string low = SafeLowerAscii(cc.label + " " + cc.css_class);
                    UiGlass::Rgba8 col = pal.labelSecondary;
                    if (low.find("aprob") != std::string::npos || low.find("green") != std::string::npos) col = pal.accentGreen;
                    else if (low.find("curso") != std::string::npos || low.find("blue") != std::string::npos) col = pal.accentBlue;
                    else if (low.find("reprob") != std::string::npos || low.find("red") != std::string::npos) col = pal.accentRed;
                    else if (low.find("req") != std::string::npos || low.find("orange") != std::string::npos) col = pal.accentOrange;
                    lx += Views::DrawPill(g, lx, ly, Utf8ToWide(cc.label), dpiScale,
                                          Views::SoftTint(col, dark), col, hc, 128.0f) + 6.0f * dpiScale;
                }
            }
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

std::string TrimDialogUtf8(std::string_view sv);
std::string FoldAccentsLowerDialog(std::string_view sv);
void SwitchHorariosViewMode(TecHubState* st, int mode);
void StartAsyncGlobalHorariosIndex(TecHubState* st);
void ToggleSelectedHorCourse(TecHubState* st, const std::string& courseCode);
void ClearSelectedHorCourses(TecHubState* st);

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

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fHdr(&ff, 12.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 10.5f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush lblBr(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::SolidBrush tertBr(UiGlass::ToGdip(pal.labelTertiary));
        Gdiplus::Pen hdrSep(UiGlass::ToGdip(pal.separator), 1.0f);

        // Top segmented switcher strip (Buscador global | Planificador sin choques)
        const int modeBarH = st->Sc(44);
        const float segW = std::min(332.0f * dpiScale, std::max(180.0f * dpiScale, static_cast<float>(w) - 28.0f * dpiScale));
        Views::DrawSegmentedBar(
            g, 14.0f * dpiScale, 7.0f * dpiScale, segW, 30.0f * dpiScale,
            { L"Buscador global", L"Planificador sin choques" }, 1, st->horModeHover,
            dpiScale, pal, hc, st->horModeHits);

        if (static_cast<float>(w) > segW + 140.0f * dpiScale) {
            std::wstring metaW;
            if (st->horariosResult.HasSolutions() && st->selectedScheduleIdx < st->horariosResult.schedules.size()) {
                const auto& cur = st->horariosResult.schedules[st->selectedScheduleIdx];
                metaW = L"Opción " + std::to_wstring(st->selectedScheduleIdx + 1) + L"/" +
                        std::to_wstring(st->horariosResult.schedules.size()) + L" · " +
                        std::to_wstring(cur.metrics.totalCredits) + L" cr · " +
                        std::to_wstring(cur.metrics.activeDaysCount) + L" días";
            } else if (!st->selectedHorCourseCodes.empty()) {
                metaW = std::to_wstring(st->selectedHorCourseCodes.size()) + L" cursos seleccionados";
            }
            if (!metaW.empty()) {
                Gdiplus::StringFormat sfMeta;
                sfMeta.SetAlignment(Gdiplus::StringAlignmentFar);
                sfMeta.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                sfMeta.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                sfMeta.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
                const float mx = 14.0f * dpiScale + segW + 12.0f * dpiScale;
                g.DrawString(metaW.c_str(), -1, &fSmall,
                             Gdiplus::RectF(mx, 2.0f, static_cast<float>(w) - mx - 14.0f * dpiScale, static_cast<float>(modeBarH - 2)),
                             &sfMeta, &tertBr);
            }
        }
        g.DrawLine(&hdrSep, 2.0f, static_cast<float>(modeBarH), static_cast<float>(w) - 3.0f, static_cast<float>(modeBarH));

        const int leftAxisW = st->Sc(54);
        const int dayHdrH = st->Sc(28);
        const int topHdrH = modeBarH + dayHdrH;
        const int numDays = 6;
        const float colW = static_cast<float>(std::max(1, w - leftAxisW - st->Sc(8))) / static_cast<float>(numDays);
        g.DrawLine(&hdrSep, 2.0f, static_cast<float>(topHdrH), static_cast<float>(w) - 3.0f, static_cast<float>(topHdrH));

        Gdiplus::StringFormat sfHdr;
        sfHdr.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);

        static const wchar_t* kDays[6] = { L"Lunes", L"Martes", L"Miércoles", L"Jueves", L"Viernes", L"Sábado" };
        for (int d = 0; d < numDays; ++d) {
            float x = static_cast<float>(leftAxisW) + d * colW;
            Gdiplus::RectF hrc(x + 2.0f, static_cast<float>(modeBarH + 1), std::max(10.0f, colW - 4.0f), static_cast<float>(dayHdrH - 2));
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
                   L"\nEscriba los códigos que desea llevar arriba o selecciónelos desde Buscador global.");
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
                L"Selecciona cursos en Buscador global o elige una escuela arriba y presiona Armar horario.");
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
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) {
        PaintHorariosWeeklyGrid(hwnd, st);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int newHov = -1;
        for (const auto& h : st->horModeHits) {
            if (PtInRect(&h.rc, pt)) {
                newHov = h.index;
                break;
            }
        }
        if (newHov != st->horModeHover) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            st->horModeHover = newHov;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_MOUSELEAVE && st) {
        if (st->horModeHover != -1) {
            st->horModeHover = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (const auto& h : st->horModeHits) {
            if (PtInRect(&h.rc, pt)) {
                SwitchHorariosViewMode(st, h.index);
                return 0;
            }
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void PaintHorSearchCanvas(HWND hwnd, TecHubState* st) {
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

    st->horAddBtnRects.clear();
    st->horSelectedChipRects.clear();
    st->rcHorClearSel = {};
    st->rcHorIndexCta = {};

    {
        Gdiplus::Graphics g(memDc);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        const auto& pal = st->theme.pal;
        const bool dark = st->theme.settings.IsDark();
        const bool hc = st->theme.settings.IsHighContrast();
        const float dpiScale = static_cast<float>(st->dpi) / 96.0f;

        Views::DrawSurface(g, 1.0f, 1.0f, static_cast<float>(w) - 2.5f, static_cast<float>(h) - 2.5f,
                           10.0f * dpiScale, pal.contentCard,
                           hc ? pal.borderStrong : pal.borderSubtle, hc ? 1.5f : 1.0f);

        Gdiplus::FontFamily ff(L"Segoe UI");
        Gdiplus::Font fTitle(&ff, 12.8f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fBody(&ff, 11.8f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 10.6f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::Font fSmallBold(&ff, 10.8f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::SolidBrush brTert(UiGlass::ToGdip(pal.labelTertiary));
        Gdiplus::SolidBrush brBlue(UiGlass::ToGdip(pal.accentBlue));
        Gdiplus::SolidBrush brGreen(UiGlass::ToGdip(pal.accentGreen));
        Gdiplus::SolidBrush brRed(UiGlass::ToGdip(pal.accentRed));
        Gdiplus::Pen sepPen(UiGlass::ToGdip(pal.separator), 1.0f);

        Gdiplus::StringFormat sfTrim;
        sfTrim.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfTrim.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfTrim.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
        Gdiplus::StringFormat sfCenter;
        sfCenter.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfCenter.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfCenter.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        // Top segmented switcher strip
        const int modeBarH = st->Sc(44);
        const float segW = std::min(332.0f * dpiScale, std::max(180.0f * dpiScale, static_cast<float>(w) - 28.0f * dpiScale));
        Views::DrawSegmentedBar(
            g, 14.0f * dpiScale, 7.0f * dpiScale, segW, 30.0f * dpiScale,
            { L"Buscador global", L"Planificador sin choques" }, 0, st->horModeHover,
            dpiScale, pal, hc, st->horModeHits);

        if (static_cast<float>(w) > segW + 150.0f * dpiScale) {
            std::wstring statsW;
            if (!st->globalHorariosIdx.entries.empty()) {
                statsW = std::to_wstring(st->globalHorariosIdx.uniqueCoursesCount) + L" cursos · " +
                         std::to_wstring(st->globalHorariosIdx.schoolsWithCourses > 0
                                             ? static_cast<size_t>(st->globalHorariosIdx.schoolsWithCourses)
                                             : st->globalHorariosIdx.schools.size()) +
                         L" escuelas · " +
                         std::to_wstring(st->globalHorariosIdx.campuses.size()) + L" sedes · " +
                         std::to_wstring(st->globalHorariosIdx.year);
            } else {
                statsW = L"Índice global pendiente";
            }
            Gdiplus::StringFormat sfR;
            sfR.SetAlignment(Gdiplus::StringAlignmentFar);
            sfR.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            sfR.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
            sfR.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
            const float mx = 14.0f * dpiScale + segW + 12.0f * dpiScale;
            g.DrawString(statsW.c_str(), -1, &fSmall,
                         Gdiplus::RectF(mx, 2.0f, static_cast<float>(w) - mx - 14.0f * dpiScale, static_cast<float>(modeBarH - 2)),
                         &sfR, &brTert);
        }
        g.DrawLine(&sepPen, 2.0f, static_cast<float>(modeBarH), static_cast<float>(w) - 3.0f, static_cast<float>(modeBarH));

        int listTop = modeBarH;

        // Optional selected-courses strip (for cross-school conflict-free planning)
        if (!st->selectedHorCourseCodes.empty()) {
            const int selBarH = st->Sc(34);
            const float sy = static_cast<float>(listTop);
            Views::DrawSurface(g, 2.0f, sy + 1.0f, static_cast<float>(w) - 4.0f, static_cast<float>(selBarH - 1),
                               0.0f, pal.sidebarBg, { 0, 0, 0, 0 }, 0.0f);
            float cx = 14.0f * dpiScale;
            const float labelW = 114.0f * dpiScale;
            g.DrawString(L"Para armar horario:", -1, &fSmallBold,
                         Gdiplus::RectF(cx, sy, labelW, static_cast<float>(selBarH)), &sfTrim, &brSec);
            cx += labelW + 4.0f * dpiScale;

            const float clearW = 58.0f * dpiScale;
            const float maxChipRight = static_cast<float>(w) - clearW - 18.0f * dpiScale;
            for (const auto& code : st->selectedHorCourseCodes) {
                std::wstring chipTxt = Utf8ToWide(code) + L"  ×";
                Gdiplus::RectF meas{};
                g.MeasureString(chipTxt.c_str(), -1, &fSmallBold, Gdiplus::PointF(0, 0), &meas);
                const float cw = std::min(118.0f * dpiScale, meas.Width + 16.0f * dpiScale);
                if (cx + cw > maxChipRight) break;
                const float cy = sy + (static_cast<float>(selBarH) - 22.0f * dpiScale) * 0.5f;
                Views::DrawSurface(g, cx, cy, cw, 22.0f * dpiScale, 11.0f * dpiScale,
                                   Views::SoftTint(pal.accentBlue, dark),
                                   hc ? pal.accentBlue : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.1f : 0.0f);
                g.DrawString(chipTxt.c_str(), -1, &fSmallBold,
                             Gdiplus::RectF(cx + 4.0f, cy, cw - 8.0f, 22.0f * dpiScale), &sfCenter, &brBlue);
                st->horSelectedChipRects.push_back({
                    RECT{ static_cast<LONG>(cx), static_cast<LONG>(cy),
                          static_cast<LONG>(cx + cw), static_cast<LONG>(cy + 22.0f * dpiScale) },
                    code
                });
                cx += cw + 6.0f * dpiScale;
            }

            const float clearX = static_cast<float>(w) - clearW - 12.0f * dpiScale;
            g.DrawString(L"Limpiar", -1, &fSmallBold,
                         Gdiplus::RectF(clearX, sy, clearW, static_cast<float>(selBarH)), &sfCenter, &brSec);
            st->rcHorClearSel = {
                static_cast<LONG>(clearX), listTop + 2,
                static_cast<LONG>(clearX + clearW), listTop + selBarH - 2
            };
            listTop += selBarH;
            g.DrawLine(&sepPen, 2.0f, static_cast<float>(listTop), static_cast<float>(w) - 3.0f, static_cast<float>(listTop));
        }

        const int listBottom = std::max(listTop + 10, h - st->Sc(4));
        const int viewH = std::max(1, listBottom - listTop);

        if (st->globalHorariosIdx.entries.empty()) {
            st->horSearchMaxScroll = 0;
            st->horSearchScrollY = 0;
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::Search,
                L"Explora horarios de todo el TEC",
                L"Indexa las 90 escuelas, las 5 sedes y todas las carreras en unos segundos para buscar cursos, profesores o aulas sin iniciar sesión.");
            // Clickable primary CTA button centered below empty state
            const float ctaW = 204.0f * dpiScale;
            const float ctaH = 34.0f * dpiScale;
            const float ctaX = (static_cast<float>(w) - ctaW) * 0.5f;
            const float ctaY = std::min(static_cast<float>(h) - ctaH - 18.0f * dpiScale,
                                        std::max(static_cast<float>(listTop) + 178.0f * dpiScale,
                                                 static_cast<float>(h) * 0.44f + 128.0f * dpiScale));
            const UiGlass::Rgba8 primBg = dark ? UiGlass::Rgba8{ 10, 112, 224, 255 } : pal.accentBlue;
            Views::DrawSurface(g, ctaX, ctaY, ctaW, ctaH, 8.0f * dpiScale, primBg,
                               hc ? pal.borderStrong : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.4f : 0.0f);
            Gdiplus::SolidBrush whiteBr(Gdiplus::Color(255, 255, 255, 255));
            g.DrawString(L"Indexar todo el TEC ahora", -1, &fTitle,
                         Gdiplus::RectF(ctaX, ctaY, ctaW, ctaH), &sfCenter, &whiteBr);
            st->rcHorIndexCta = {
                static_cast<LONG>(ctaX), static_cast<LONG>(ctaY),
                static_cast<LONG>(ctaX + ctaW), static_cast<LONG>(ctaY + ctaH)
            };
        } else if (st->horSearchResults.empty()) {
            st->horSearchMaxScroll = 0;
            st->horSearchScrollY = 0;
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::Search,
                L"Sin grupos que coincidan",
                L"Prueba otro término de búsqueda o cambia los filtros de sede, escuela, periodo, modalidad o día.");
        } else {
            const float rowH = 68.0f * dpiScale;
            const int totalContentH = static_cast<int>(std::ceil(static_cast<float>(st->horSearchResults.size()) * rowH)) + st->Sc(8);
            st->horSearchMaxScroll = std::max(0, totalContentH - viewH);
            st->horSearchScrollY = std::clamp(st->horSearchScrollY, 0, st->horSearchMaxScroll);

            g.SetClip(Gdiplus::RectF(2.0f, static_cast<float>(listTop + 1),
                                     static_cast<float>(w - 4), static_cast<float>(viewH - 2)));

            const float sbReserve = st->horSearchMaxScroll > 0 ? 12.0f * dpiScale : 0.0f;
            const float btnW = 86.0f * dpiScale;
            const float btnH = 26.0f * dpiScale;
            const float btnX = static_cast<float>(w) - btnW - 14.0f * dpiScale - sbReserve;

            auto isSelectedCode = [&](const std::string& code) {
                for (const auto& sc : st->selectedHorCourseCodes) {
                    if (sc == code) return true;
                }
                return false;
            };

            for (size_t i = 0; i < st->horSearchResults.size(); ++i) {
                const float ry = static_cast<float>(listTop + st->Sc(4)) + static_cast<float>(i) * rowH - static_cast<float>(st->horSearchScrollY);
                if (ry + rowH < static_cast<float>(listTop)) continue;
                if (ry > static_cast<float>(listBottom)) break;

                const auto& e = st->horSearchResults[i];
                const bool hovered = (static_cast<int>(i) == st->horSearchHoverIdx);
                const bool selected = isSelectedCode(e.courseCode);

                if (hovered) {
                    Views::DrawSurface(g, 8.0f * dpiScale, ry + 2.0f,
                                       static_cast<float>(w) - 16.0f * dpiScale - sbReserve, rowH - 4.0f,
                                       8.0f * dpiScale, pal.sidebarHover, { 0, 0, 0, 0 }, 0.0f);
                }

                // Thin vertical accent bar by school
                const UiGlass::Rgba8 acc = Views::DeptAccent(e.schoolCode, "CLASE", pal);
                Views::DrawSurface(g, 14.0f * dpiScale, ry + 11.0f * dpiScale, 3.2f * dpiScale, rowH - 22.0f * dpiScale,
                                   2.0f * dpiScale, acc, { 0, 0, 0, 0 }, 0.0f);

                // Interactive + Agregar / Agregado button on the right
                const float by = ry + (rowH - btnH) * 0.5f;
                const UiGlass::Rgba8 bFill = selected ? Views::SoftTint(pal.accentBlue, dark) : pal.contentCardElevated;
                const UiGlass::Rgba8 bBord = selected ? pal.accentBlue : (hc ? pal.borderStrong : pal.borderSubtle);
                Views::DrawSurface(g, btnX, by, btnW, btnH, 7.0f * dpiScale, bFill, bBord, selected ? 1.15f : 0.9f);
                g.DrawString(selected ? L"Agregado" : L"+ Agregar", -1, &fSmallBold,
                             Gdiplus::RectF(btnX, by, btnW, btnH), &sfCenter, selected ? &brBlue : &brPrim);
                if (by + btnH >= static_cast<float>(listTop) && by <= static_cast<float>(listBottom)) {
                    st->horAddBtnRects.push_back({
                        RECT{ static_cast<LONG>(btnX), static_cast<LONG>(by),
                              static_cast<LONG>(btnX + btnW), static_cast<LONG>(by + btnH) },
                        e.courseCode
                    });
                }

                // Compact right-aligned metadata pills (Campus, Period, Modality) left of action button
                float pillRight = btnX - 10.0f * dpiScale;
                const float pillY = ry + 8.0f * dpiScale;
                auto drawRightPill = [&](const std::string& txtUtf8, UiGlass::Rgba8 pillAcc, bool neutral) {
                    if (txtUtf8.empty()) return;
                    std::wstring wt = Utf8ToWide(txtUtf8);
                    Gdiplus::RectF meas{};
                    g.MeasureString(wt.c_str(), -1, &fSmallBold, Gdiplus::PointF(0, 0), &meas);
                    const float pw = std::min(108.0f * dpiScale, meas.Width + 14.0f * dpiScale);
                    const float px = pillRight - pw;
                    if (px < 210.0f * dpiScale) return;
                    const UiGlass::Rgba8 pf = neutral ? pal.sidebarBg : Views::SoftTint(pillAcc, dark);
                    const UiGlass::Rgba8 fg = neutral ? pal.labelSecondary : pillAcc;
                    Views::DrawSurface(g, px, pillY, pw, 20.0f * dpiScale, 10.0f * dpiScale, pf,
                                       hc ? fg : UiGlass::Rgba8{ 0, 0, 0, 0 }, hc ? 1.0f : 0.0f);
                    Gdiplus::SolidBrush tbr(UiGlass::ToGdip(fg));
                    g.DrawString(wt.c_str(), -1, &fSmallBold,
                                 Gdiplus::RectF(px + 3.0f, pillY, pw - 6.0f, 20.0f * dpiScale), &sfCenter, &tbr);
                    pillRight = px - 5.0f * dpiScale;
                };

                if (e.seatsAvailable >= 0) {
                    const std::string seatsTxt = e.seatsAvailable > 0
                        ? (std::to_string(e.seatsAvailable) + " cupos")
                        : std::string("Sin cupo");
                    drawRightPill(seatsTxt, e.seatsAvailable > 0 ? pal.accentGreen : pal.accentRed, false);
                }
                if (!e.modality.empty()) {
                    const std::string mLow = FoldAccentsLowerDialog(e.modality);
                    const bool isVirt = (mLow.find("virtual") != std::string::npos);
                    const bool isSemi = (mLow.find("semi") != std::string::npos);
                    drawRightPill(e.modality, isVirt ? pal.accentPurple : (isSemi ? pal.accentOrange : pal.labelSecondary),
                                  !(isVirt || isSemi));
                }
                if (!e.periodCode.empty()) {
                    drawRightPill(e.periodCode, pal.accentIndigo, true);
                }
                if (!e.campusShort.empty()) {
                    drawRightPill(e.campusShort, pal.accentTeal, false);
                }

                // Line 1: Course Code + Group + Course Name
                const float tx = 26.0f * dpiScale;
                const float line1W = std::max(80.0f * dpiScale, pillRight - tx - 6.0f * dpiScale);
                std::wstring headW = Utf8ToWide(e.courseCode + (e.groupId.empty() ? "" : (" · Gr " + e.groupId)));
                if (!e.courseName.empty()) {
                    headW += L"  —  " + Utf8ToWide(e.courseName);
                }
                g.DrawString(headW.c_str(), -1, &fTitle,
                             Gdiplus::RectF(tx, ry + 7.0f * dpiScale, line1W, 20.0f * dpiScale), &sfTrim, &brPrim);

                // Line 2: Schedule summary · Professor · Room · Credits
                const float line2W = std::max(80.0f * dpiScale, btnX - tx - 10.0f * dpiScale);
                std::string meta2 = e.scheduleSummary.empty() ? "Horario por definir" : e.scheduleSummary;
                if (!e.professor.empty()) meta2 += "  ·  " + e.professor;
                if (!e.room.empty()) meta2 += "  ·  Aula " + e.room;
                if (e.credits > 0) meta2 += "  ·  " + std::to_string(e.credits) + " cr";
                g.DrawString(Utf8ToWide(meta2).c_str(), -1, &fBody,
                             Gdiplus::RectF(tx, ry + 28.0f * dpiScale, line2W, 18.0f * dpiScale), &sfTrim, &brSec);

                // Line 3: School · Career (if enriched) · Reserved note
                std::string meta3 = e.schoolName.empty() ? e.schoolCode : e.schoolName;
                if (!e.careerName.empty()) meta3 += "  ·  " + e.careerName;
                if (!e.reservedNote.empty()) meta3 += "  ·  Reserva: " + e.reservedNote;
                g.DrawString(Utf8ToWide(meta3).c_str(), -1, &fSmall,
                             Gdiplus::RectF(tx, ry + 47.0f * dpiScale, line2W, 16.0f * dpiScale), &sfTrim, &brTert);

                if (i + 1 < st->horSearchResults.size()) {
                    g.DrawLine(&sepPen, 16.0f * dpiScale, ry + rowH - 1.0f,
                               static_cast<float>(w) - 16.0f * dpiScale - sbReserve, ry + rowH - 1.0f);
                }
            }

            g.ResetClip();

            // Subtle scroll indicator thumb
            if (st->horSearchMaxScroll > 0) {
                const float trackH = static_cast<float>(viewH - st->Sc(12));
                const float ratio = static_cast<float>(viewH) / static_cast<float>(totalContentH);
                const float thumbH = std::max(26.0f * dpiScale, trackH * ratio);
                const float scrollFrac = static_cast<float>(st->horSearchScrollY) / static_cast<float>(st->horSearchMaxScroll);
                const float thumbY = static_cast<float>(listTop + st->Sc(6)) + scrollFrac * (trackH - thumbH);
                Views::DrawSurface(g, static_cast<float>(w) - 8.5f * dpiScale, thumbY, 4.5f * dpiScale, thumbH,
                                   2.25f * dpiScale, pal.borderStrong, { 0, 0, 0, 0 }, 0.0f);
            }
        }
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK HorSearchCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) {
        PaintHorSearchCanvas(hwnd, st);
        return 0;
    }
    if (msg == WM_MOUSEWHEEL && st) {
        short delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->horSearchScrollY = std::clamp(st->horSearchScrollY - (delta / 120) * st->Sc(58), 0, st->horSearchMaxScroll);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }
    if (msg == WM_MOUSEMOVE && st) {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        int newModeHov = -1;
        for (const auto& h : st->horModeHits) {
            if (PtInRect(&h.rc, pt)) {
                newModeHov = h.index;
                break;
            }
        }
        int newRowHov = -1;
        const int modeBarH = st->Sc(44);
        const int listTop = modeBarH + (st->selectedHorCourseCodes.empty() ? 0 : st->Sc(34));
        if (pt.y > listTop && !st->horSearchResults.empty()) {
            const float rowH = 68.0f * (static_cast<float>(st->dpi) / 96.0f);
            int idx = static_cast<int>((static_cast<float>(pt.y - listTop - st->Sc(4) + st->horSearchScrollY)) / rowH);
            if (idx >= 0 && static_cast<size_t>(idx) < st->horSearchResults.size()) {
                newRowHov = idx;
            }
        }
        if (newModeHov != st->horModeHover || newRowHov != st->horSearchHoverIdx) {
            TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);
            st->horModeHover = newModeHov;
            st->horSearchHoverIdx = newRowHov;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_MOUSELEAVE && st) {
        if (st->horModeHover != -1 || st->horSearchHoverIdx != -1) {
            st->horModeHover = -1;
            st->horSearchHoverIdx = -1;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        for (const auto& h : st->horModeHits) {
            if (PtInRect(&h.rc, pt)) {
                SwitchHorariosViewMode(st, h.index);
                return 0;
            }
        }
        if (st->rcHorIndexCta.right > st->rcHorIndexCta.left && PtInRect(&st->rcHorIndexCta, pt)) {
            StartAsyncGlobalHorariosIndex(st);
            return 0;
        }
        if (st->rcHorClearSel.right > st->rcHorClearSel.left && PtInRect(&st->rcHorClearSel, pt)) {
            ClearSelectedHorCourses(st);
            return 0;
        }
        for (const auto& [rc, code] : st->horSelectedChipRects) {
            if (PtInRect(&rc, pt)) {
                ToggleSelectedHorCourse(st, code);
                return 0;
            }
        }
        for (const auto& [rc, code] : st->horAddBtnRects) {
            if (PtInRect(&rc, pt)) {
                ToggleSelectedHorCourse(st, code);
                return 0;
            }
        }
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

    st->calMonthDayRects.clear();
    st->rcCalPrevMonth = {};
    st->rcCalNextMonth = {};

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
        Gdiplus::Font fHdr(&ff, 12.0f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fBlkTitle(&ff, 11.4f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
        Gdiplus::Font fSmall(&ff, 10.5f * dpiScale, hc ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
        Gdiplus::SolidBrush lblBr(UiGlass::ToGdip(pal.labelSecondary));
        Gdiplus::SolidBrush primBr(UiGlass::ToGdip(pal.labelPrimary));
        Gdiplus::Pen hdrSep(UiGlass::ToGdip(pal.separator), 1.0f);
        Gdiplus::StringFormat sfHdr;
        sfHdr.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetAlignment(Gdiplus::StringAlignmentCenter);
        sfHdr.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        Gdiplus::StringFormat sfTrim;
        sfTrim.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
        sfTrim.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

        const int calMode = st->hCmbCalView ? static_cast<int>(SendMessageW(st->hCmbCalView, CB_GETCURSEL, 0, 0)) : 1;
        if (calMode == 2) {
            // VISTA MENSUAL (guías de interfaz Calendar Grid + Day Inspector)
            SYSTEMTIME nowSt{};
            GetLocalTime(&nowSt);
            if (st->calMonthYear < 1990 || st->calMonthMon < 1 || st->calMonthMon > 12) {
                st->calMonthYear = nowSt.wYear;
                st->calMonthMon = nowSt.wMonth;
            }
            static const wchar_t* kMonths[12] = {
                L"Enero", L"Febrero", L"Marzo", L"Abril", L"Mayo", L"Junio",
                L"Julio", L"Agosto", L"Septiembre", L"Octubre", L"Noviembre", L"Diciembre"
            };
            const float pad = 12.0f * dpiScale;
            const float topBarH = 38.0f * dpiScale;
            const float prevW = Views::DrawPill(g, pad, 8.0f * dpiScale, L"< Anterior", dpiScale, pal.sidebarBg, pal.labelPrimary, hc, 88.0f);
            st->rcCalPrevMonth = { static_cast<LONG>(pad), static_cast<LONG>(8.0f * dpiScale),
                                   static_cast<LONG>(pad + prevW), static_cast<LONG>(32.0f * dpiScale) };
            const float nextX = static_cast<float>(w) - pad - 92.0f * dpiScale;
            const float nextW = Views::DrawPill(g, nextX, 8.0f * dpiScale, L"Siguiente >", dpiScale, pal.sidebarBg, pal.labelPrimary, hc, 92.0f);
            st->rcCalNextMonth = { static_cast<LONG>(nextX), static_cast<LONG>(8.0f * dpiScale),
                                   static_cast<LONG>(nextX + nextW), static_cast<LONG>(32.0f * dpiScale) };

            std::wstring mTitle = std::wstring(kMonths[st->calMonthMon - 1]) + L" " + std::to_wstring(st->calMonthYear);
            g.DrawString(mTitle.c_str(), -1, &fHdr, Gdiplus::RectF(0.0f, 4.0f * dpiScale, static_cast<float>(w), topBarH - 6.0f * dpiScale), &sfHdr, &primBr);
            g.DrawLine(&hdrSep, 2.0f, topBarH, static_cast<float>(w) - 3.0f, topBarH);

            const float dowH = 24.0f * dpiScale;
            const float footerH = 74.0f * dpiScale;
            const float gridTop = topBarH + dowH;
            const float gridH = std::max(120.0f, static_cast<float>(h) - gridTop - footerH - 4.0f);
            const float colW = std::max(24.0f, (static_cast<float>(w) - 8.0f) / 7.0f);
            const float rowH = gridH / 6.0f;

            static const wchar_t* kDow[7] = { L"Lun", L"Mar", L"Mié", L"Jue", L"Vie", L"Sáb", L"Dom" };
            for (int d = 0; d < 7; ++d) {
                g.DrawString(kDow[d], -1, &fSmall, Gdiplus::RectF(4.0f + d * colW, topBarH + 2.0f, colW, dowH - 4.0f), &sfHdr, &lblBr);
            }

            // Compute first day of month (0=Mon..6=Sun) & days in month
            std::tm tFirst{};
            tFirst.tm_year = st->calMonthYear - 1900;
            tFirst.tm_mon = st->calMonthMon - 1;
            tFirst.tm_mday = 1;
            tFirst.tm_hour = 12;
            std::mktime(&tFirst);
            const int firstMon0 = (tFirst.tm_wday == 0) ? 6 : (tFirst.tm_wday - 1);
            std::tm tNext = tFirst;
            tNext.tm_mon += 1;
            tNext.tm_mday = 0;
            std::mktime(&tNext);
            const int daysInMonth = std::clamp(tNext.tm_mday, 28, 31);

            // Filter events matching current filter combo (deduplicated via GetPrioritizedCalendarEvents)
            int filterIdx = st->hCmbCalFilter ? static_cast<int>(SendMessageW(st->hCmbCalFilter, CB_GETCURSEL, 0, 0)) : 0;
            const auto dedupCalEvents = GetPrioritizedCalendarEvents(st->snap.unifiedCalendarEvents);
            std::map<std::string, std::vector<const td::CalendarEventEntry*>> evByDate;
            for (const auto* ev : dedupCalEvents) {
                if (filterIdx == 1 && ev->source_kind != "evaluation") continue;
                if (filterIdx == 2 && ev->source_kind != "matricula") continue;
                if (filterIdx == 3 && ev->source_kind != "calendar") continue;
                if (ev->date_iso.size() >= 10) evByDate[ev->date_iso.substr(0, 10)].push_back(ev);
            }

            char todayIsoBuf[16]{};
            std::snprintf(todayIsoBuf, sizeof(todayIsoBuf), "%04d-%02d-%02d", nowSt.wYear, nowSt.wMonth, nowSt.wDay);
            const std::string todayIso = todayIsoBuf;

            Gdiplus::Pen cellPen(UiGlass::ToGdip(pal.separator, dark ? 110 : 145), 0.9f);
            for (int cellIdx = 0; cellIdx < 42; ++cellIdx) {
                const int r = cellIdx / 7;
                const int c = cellIdx % 7;
                const float x0 = 4.0f + c * colW;
                const float y0 = gridTop + r * rowH;
                g.DrawRectangle(&cellPen, x0, y0, colW, rowH);

                const int dayNum = cellIdx - firstMon0 + 1;
                if (dayNum < 1 || dayNum > daysInMonth) continue;

                char dBuf[16]{};
                std::snprintf(dBuf, sizeof(dBuf), "%04d-%02d-%02d", st->calMonthYear, st->calMonthMon, dayNum);
                const std::string iso = dBuf;
                const bool isToday = (iso == todayIso);
                const bool isSel = (iso == st->calSelectedDateIso);

                if (isSel) {
                    Gdiplus::SolidBrush selBr(UiGlass::ToGdip(Views::SoftTint(pal.accentBlue, dark)));
                    g.FillRectangle(&selBr, x0 + 1.0f, y0 + 1.0f, colW - 2.0f, rowH - 2.0f);
                }
                if (isToday) {
                    Gdiplus::SolidBrush tb(UiGlass::ToGdip(pal.accentBlue));
                    g.FillEllipse(&tb, x0 + 5.0f * dpiScale, y0 + 3.0f * dpiScale, 20.0f * dpiScale, 20.0f * dpiScale);
                    Gdiplus::SolidBrush wb(Gdiplus::Color(255, 255, 255, 255));
                    g.DrawString(std::to_wstring(dayNum).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(x0 + 5.0f * dpiScale, y0 + 4.0f * dpiScale, 20.0f * dpiScale, 18.0f * dpiScale), &sfHdr, &wb);
                } else {
                    g.DrawString(std::to_wstring(dayNum).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(x0 + 6.0f * dpiScale, y0 + 4.0f * dpiScale, 24.0f * dpiScale, 16.0f * dpiScale), &sfTrim, &primBr);
                }

                auto dit = evByDate.find(iso);
                if (dit != evByDate.end()) {
                    float ey = y0 + 23.0f * dpiScale;
                    for (size_t ei = 0; ei < dit->second.size() && ei < 2; ++ei) {
                        if (ey + 14.0f * dpiScale > y0 + rowH - 2.0f) break;
                        const auto* ev = dit->second[ei];
                        const UiGlass::Rgba8 col = (ev->source_kind == "matricula")
                                                       ? pal.accentOrange
                                                       : ((ev->source_kind == "evaluation") ? pal.accentBlue : pal.accentTeal);
                        Gdiplus::SolidBrush eb(UiGlass::ToGdip(Views::SoftTint(col, dark)));
                        g.FillRectangle(&eb, x0 + 4.0f, ey, std::max(8.0f, colW - 8.0f), 14.0f * dpiScale);
                        Gdiplus::SolidBrush tb(UiGlass::ToGdip(col));
                        g.DrawString(Utf8ToWide(ev->title).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(x0 + 6.0f, ey, std::max(8.0f, colW - 10.0f), 14.0f * dpiScale), &sfTrim, &tb);
                        ey += 16.0f * dpiScale;
                    }
                    if (dit->second.size() > 2 && ey + 11.0f * dpiScale <= y0 + rowH) {
                        std::wstring more = L"+" + std::to_wstring(dit->second.size() - 2) + L" más";
                        g.DrawString(more.c_str(), -1, &fSmall, Gdiplus::RectF(x0 + 6.0f, ey, colW - 10.0f, 13.0f * dpiScale), &sfTrim, &lblBr);
                    }
                }
                st->calMonthDayRects.push_back({ { static_cast<LONG>(x0), static_cast<LONG>(y0),
                                                   static_cast<LONG>(x0 + colW), static_cast<LONG>(y0 + rowH) }, iso });
            }

            // Bottom Day Inspector Strip
            const float fy = gridTop + gridH + 4.0f;
            g.DrawLine(&hdrSep, 2.0f, fy, static_cast<float>(w) - 3.0f, fy);
            const std::string selDate = st->calSelectedDateIso.empty() ? todayIso : st->calSelectedDateIso;
            auto sit = evByDate.find(selDate);
            std::wstring fHdrTxt = L"Eventos del " + Utf8ToWide(selDate) + L":";
            g.DrawString(fHdrTxt.c_str(), -1, &fBlkTitle, Gdiplus::RectF(pad, fy + 8.0f * dpiScale, 190.0f * dpiScale, 18.0f * dpiScale), &sfTrim, &primBr);
            if (sit != evByDate.end() && !sit->second.empty()) {
                float ey = fy + 28.0f * dpiScale;
                for (size_t ei = 0; ei < sit->second.size() && ei < 2; ++ei) {
                    const auto* ev = sit->second[ei];
                    std::string line = (ev->start_time.empty() ? "" : (ev->start_time + " · ")) + ev->title +
                                       (ev->calendar_name.empty() ? "" : (" (" + ev->calendar_name + ")"));
                    g.DrawString(Utf8ToWide(line).c_str(), -1, &fSmall,
                                 Gdiplus::RectF(pad, ey, static_cast<float>(w) - pad * 2.0f, 17.0f * dpiScale), &sfTrim, &lblBr);
                    ey += 18.0f * dpiScale;
                }
            } else {
                g.DrawString(L"Sin entregas ni citas en esta fecha. Haz clic en cualquier día con eventos para ver el detalle.", -1, &fSmall,
                             Gdiplus::RectF(pad, fy + 30.0f * dpiScale, static_cast<float>(w) - pad * 2.0f, 18.0f * dpiScale), &sfTrim, &lblBr);
            }
        } else {
            // VISTA SEMANAL (Horario Oficial + Agenda)
            const int leftAxisW = st->Sc(54);
            const int topHdrH = st->Sc(32);
            const bool hasFooterEvents = !st->snap.unifiedCalendarEvents.empty();
            const int eventStripH = hasFooterEvents ? st->Sc(40) : 0;
            const int gridBottom = std::max(topHdrH + st->Sc(100), h - eventStripH - st->Sc(4));
            const int numDays = 6;
            const int startHour = 7;
            const int endHour = 21;
            const float colW = static_cast<float>(std::max(1, w - leftAxisW - st->Sc(8))) / static_cast<float>(numDays);
            const float hourH = static_cast<float>(std::max(1, gridBottom - topHdrH)) / static_cast<float>(endHour - startHour);

            g.DrawLine(&hdrSep, 2.0f, static_cast<float>(topHdrH), static_cast<float>(w) - 3.0f, static_cast<float>(topHdrH));

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
                    g.DrawLine(&gridPen, x, static_cast<float>(topHdrH), x, static_cast<float>(gridBottom));
                }
                for (int hr = startHour; hr <= endHour; ++hr) {
                    float y = static_cast<float>(topHdrH) + static_cast<float>(hr - startHour) * hourH;
                    g.DrawLine(&gridPen, static_cast<float>(leftAxisW), y, static_cast<float>(w - 4), y);
                    if (hr < endHour) {
                        wchar_t tbuf[16]{};
                        swprintf_s(tbuf, L"%02d:00", hr);
                        Gdiplus::RectF trc(4.0f, y + 1.0f, static_cast<float>(leftAxisW - st->Sc(10)), std::max(12.0f * dpiScale, hourH * 0.65f));
                        g.DrawString(tbuf, -1, &fSmall, trc, &sfAxis, &lblBr);
                    }
                }

                for (const auto& cell : st->snap.currentSchedule) {
                    if (cell.day_index_0_mon < 0 || cell.day_index_0_mon >= numDays) continue;
                    if (cell.end_minute <= cell.start_minute) continue;
                    float y0 = static_cast<float>(topHdrH) + (static_cast<float>(cell.start_minute) / 60.0f - startHour) * hourH;
                    float y1 = static_cast<float>(topHdrH) + (static_cast<float>(cell.end_minute) / 60.0f - startHour) * hourH;
                    float x0 = static_cast<float>(leftAxisW) + cell.day_index_0_mon * colW + 4.0f;
                    float bw = std::max(8.0f, colW - 8.0f);
                    float bh = std::max(22.0f * dpiScale, y1 - y0 - 2.0f);

                    std::string dept;
                    for (char ch : cell.course_code) {
                        if (std::isalpha(static_cast<unsigned char>(ch))) dept.push_back(ch);
                        else break;
                    }
                    const UiGlass::Rgba8 acc = Views::DeptAccent(dept, "CLASE", pal);
                    const UiGlass::Rgba8 fill = Views::SoftTint(acc, dark);
                    Views::DrawSurface(g, x0, y0 + 1.0f, bw, bh, 7.0f * dpiScale, fill,
                                       hc ? acc : UiGlass::Rgba8{ acc.r, acc.g, acc.b, static_cast<uint8_t>(dark ? 110 : 75) }, 1.0f);
                    Gdiplus::SolidBrush stripeBr(UiGlass::ToGdip(acc));
                    g.FillRectangle(&stripeBr, x0 + 2.0f, y0 + 5.0f * dpiScale, 3.2f * dpiScale, std::max(8.0f, bh - 8.0f * dpiScale));

                    const std::wstring codeW = Utf8ToWide(cell.course_code.empty() ? cell.raw_text : (cell.course_code + (cell.group.empty() ? "" : (" G" + cell.group))));
                    g.DrawString(codeW.c_str(), -1, &fBlkTitle,
                                 Gdiplus::RectF(x0 + 9.0f * dpiScale, y0 + 4.0f * dpiScale, std::max(10.0f, bw - 13.0f * dpiScale), 16.0f * dpiScale),
                                 &sfTrim, &primBr);
                    if (!cell.room.empty() && bh >= 32.0f * dpiScale) {
                        g.DrawString(Utf8ToWide("Aula " + cell.room).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(x0 + 9.0f * dpiScale, y0 + 20.0f * dpiScale, std::max(10.0f, bw - 13.0f * dpiScale), 14.0f * dpiScale),
                                     &sfTrim, &lblBr);
                    }
                    if (!cell.course_name.empty() && bh >= 48.0f * dpiScale) {
                        g.DrawString(Utf8ToWide(cell.course_name).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(x0 + 9.0f * dpiScale, y0 + 34.0f * dpiScale, std::max(10.0f, bw - 13.0f * dpiScale), 14.0f * dpiScale),
                                     &sfTrim, &lblBr);
                    }
                    if (!cell.professor.empty() && bh >= 62.0f * dpiScale) {
                        g.DrawString(Utf8ToWide(cell.professor).c_str(), -1, &fSmall,
                                     Gdiplus::RectF(x0 + 9.0f * dpiScale, y0 + 48.0f * dpiScale, std::max(10.0f, bw - 13.0f * dpiScale), 14.0f * dpiScale),
                                     &sfTrim, &lblBr);
                    }
                    ++drawn;
                }

                // Dedicated non-clipped bottom strip for upcoming deadlines & matrícula
                const auto sortedFooterEvents = GetPrioritizedCalendarEvents(st->snap.unifiedCalendarEvents);
                if (!sortedFooterEvents.empty()) {
                    const float stripY = static_cast<float>(gridBottom);
                    g.DrawLine(&hdrSep, 2.0f, stripY, static_cast<float>(w) - 3.0f, stripY);
                    Gdiplus::RectF lblRc(4.0f, stripY + 2.0f, static_cast<float>(leftAxisW - st->Sc(10)), static_cast<float>(eventStripH - 4));
                    g.DrawString(L"Agenda", -1, &fSmall, lblRc, &sfAxis, &lblBr);

                    float ex = static_cast<float>(leftAxisW) + 6.0f * dpiScale;
                    const float ey = stripY + (static_cast<float>(eventStripH) - 21.0f * dpiScale) * 0.5f;
                    const float maxRight = static_cast<float>(w) - 10.0f * dpiScale;
                    for (const auto* evPtr : sortedFooterEvents) {
                        const auto& ev = *evPtr;
                        if (ex > maxRight - 90.0f * dpiScale) break;
                        const UiGlass::Rgba8 col = (ev.source_kind == "matricula")
                                                       ? pal.accentOrange
                                                       : ((ev.source_kind == "evaluation") ? pal.accentBlue : pal.accentTeal);
                        std::string chip = (ev.date_iso.size() >= 10 ? (ev.date_iso.substr(5) + " · ") : "") + ev.title;
                        const float availW = (maxRight - ex) / dpiScale;
                        const float pw = Views::DrawPill(g, ex, ey, Utf8ToWide(chip), dpiScale,
                                                         Views::SoftTint(col, dark), col, hc, std::min(196.0f, availW));
                        ex += pw + 6.0f * dpiScale;
                        ++drawn;
                    }
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
    }

    BitBlt(hdc, 0, 0, w, h, memDc, 0, 0, SRCCOPY);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK CalendarCanvasProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<TecHubState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT && st) {
        PaintUnifiedCalendarGrid(hwnd, st);
        return 0;
    }
    if (msg == WM_LBUTTONUP && st) {
        POINT pt{ static_cast<short>(LOWORD(lp)), static_cast<short>(HIWORD(lp)) };
        if (PtInRect(&st->rcCalPrevMonth, pt)) {
            if (--st->calMonthMon < 1) { st->calMonthMon = 12; --st->calMonthYear; }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        if (PtInRect(&st->rcCalNextMonth, pt)) {
            if (++st->calMonthMon > 12) { st->calMonthMon = 1; ++st->calMonthYear; }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        for (const auto& [rc, iso] : st->calMonthDayRects) {
            if (PtInRect(&rc, pt)) {
                st->calSelectedDateIso = iso;
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
        }
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
            const bool hasAccount = !st->snap.communities.empty() || st->snap.sessionAuthenticated;
            DrawEmptyStateCentered(
                g, w, h, dpiScale, pal, hc,
                UiGlass::IconId::AcademicCap,
                hasAccount ? L"Sin atributos publicados para este plan" : L"Atributos de graduación",
                hasAccount
                    ? L"El portal externo de Atributos de Graduación requiere autenticación independiente o aún no registra evaluaciones para tu plan de estudios."
                    : L"Inicie sesión y presione Sincronizar ahora para consultar el avance por atributo en sus cursos evaluados (Inicial, Medio y Avanzado).");
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
                       (st->store->IsAuthenticated() || st->snap.sessionAuthenticated)
                           ? L"Conectado"
                           : L"Sin conexión");
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
            if (periodFilter == "Semestre actual") {
                if (c.type != "CLASE" || !c.is_current_period) continue;
            } else if (periodFilter == "Todos los cursos") {
                if (c.type != "CLASE") continue;
            } else if (periodFilter == "Cátedras") {
                if (c.type != "CATEDRA") continue;
            } else if (periodFilter == "Escuelas y Comunidades") {
                if (c.type == "CLASE" || c.type == "CATEDRA") continue;
            } else if (periodFilter == "Comunidades y Cátedras") {
                if (c.type == "CLASE") continue;
            } else if (c.period != periodFilter) {
                continue;
            }
        }
        st->filteredCourses.push_back(c);
    }
    if (periodFilter.empty()) {
        auto rankSpace = [](const td::CommunityInfo& c) -> int {
            if (c.type == "CLASE" && c.is_current_period) return 0;
            if (c.type == "CATEDRA") return 1;
            if (c.type == "ESCUELA") return 2;
            if (c.type == "SUBGRUPO" || c.type == "PROGRAMA") return 3;
            if (c.type == "CLASE") return 4;
            return 5;
        };
        std::stable_sort(st->filteredCourses.begin(), st->filteredCourses.end(),
                         [&](const td::CommunityInfo& a, const td::CommunityInfo& b) {
                             return rankSpace(a) < rankSpace(b);
                         });
    }
    for (const auto& c : st->filteredCourses) {
        std::string label = c.name;
        if (!c.period.empty()) label += "  [" + c.period + "]";
        SendMessageW(st->hLstCourses, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(label).c_str()));
    }
    if (!st->filteredCourses.empty()) {
        if (st->selectedCourseIdx < 0 || static_cast<size_t>(st->selectedCourseIdx) >= st->filteredCourses.size()) {
            st->selectedCourseIdx = 0;
        }
        SendMessageW(st->hLstCourses, LB_SETCURSEL, static_cast<WPARAM>(st->selectedCourseIdx), 0);
    } else {
        st->selectedCourseIdx = -1;
    }
    st->courseListScrollY = 0;
    if (st->hCourseListCanvas) InvalidateRect(st->hCourseListCanvas, nullptr, FALSE);
}

void RefreshSelectedCourseDetail(TecHubState* st) {
    const int subTab = std::clamp(st->courseSubTab, 0, 4);
    if (st->hCmbCourseSub) {
        SendMessageW(st->hCmbCourseSub, CB_SETCURSEL, static_cast<WPARAM>(subTab), 0);
    }
    if (st->currentSection == HubSection::Cursos) {
        const bool showDocBar = (subTab == 1);
        const bool showWhatIf = (subTab == 2);
        if (st->hEditCourseDocSearch) ShowWindow(st->hEditCourseDocSearch, showDocBar ? SW_SHOW : SW_HIDE);
        if (st->hCmbCourseDocSort) ShowWindow(st->hCmbCourseDocSort, showDocBar ? SW_SHOW : SW_HIDE);
        if (st->hEditWhatIf) ShowWindow(st->hEditWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnWhatIf) ShowWindow(st->hBtnWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseRubric) ShowWindow(st->hBtnCourseRubric, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseUploadEval) ShowWindow(st->hBtnCourseUploadEval, showWhatIf ? SW_SHOW : SW_HIDE);
        HWND hBtnIns = GetDlgItem(st->hwnd, IDC_BTN_COURSE_INSERT_CANVAS);
        if (hBtnIns) ShowWindow(hBtnIns, showWhatIf ? SW_HIDE : SW_SHOW);
        HWND hBtnSyncFiles = GetDlgItem(st->hwnd, IDC_BTN_COURSE_SYNC_FILES);
        if (hBtnSyncFiles) ShowWindow(hBtnSyncFiles, showWhatIf ? SW_HIDE : SW_SHOW);
        if (st->hCourseDetail) ShowWindow(st->hCourseDetail, SW_HIDE);
        if (st->hLstCourseFiles) ShowWindow(st->hLstCourseFiles, SW_HIDE);
        UpdateActiveFramedPanels(st);
        InvalidateRect(st->hwnd, nullptr, FALSE);
    }
    int idx = st->selectedCourseIdx;
    if (idx < 0 && st->hLstCourses) {
        idx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
    }
    if (idx < 0 || static_cast<size_t>(idx) >= st->filteredCourses.size()) {
        st->currentCourseFiles.clear();
        if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
        if (st->hCourseListCanvas) InvalidateRect(st->hCourseListCanvas, nullptr, FALSE);
        return;
    }
    st->selectedCourseIdx = idx;
    const auto& course = st->filteredCourses[static_cast<size_t>(idx)];

    // Always sync currentCourseFiles for selected course so Documentos tab & actions are ready
    st->currentCourseFiles.clear();
    if (st->hLstCourseFiles) SendMessageW(st->hLstCourseFiles, LB_RESETCONTENT, 0, 0);
    if (auto fIt = st->snap.foldersByPath.find(course.url); fIt != st->snap.foldersByPath.end()) {
        for (const auto& f : fIt->second.files) {
            st->currentCourseFiles.push_back(f);
            if (st->hLstCourseFiles) {
                SendMessageW(st->hLstCourseFiles, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(f.name).c_str()));
            }
        }
    }
    if (!st->currentCourseFiles.empty()) {
        st->selectedCourseFileIdx = std::clamp(st->selectedCourseFileIdx, 0, static_cast<int>(st->currentCourseFiles.size()) - 1);
        if (st->hLstCourseFiles) SendMessageW(st->hLstCourseFiles, LB_SETCURSEL, static_cast<WPARAM>(st->selectedCourseFileIdx), 0);
    } else {
        st->selectedCourseFileIdx = -1;
    }

    if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
    if (st->hCourseListCanvas) InvalidateRect(st->hCourseListCanvas, nullptr, FALSE);
}

void RefreshCalendarSection(TecHubState* st) {
    if (st->hAgendaCanvas) InvalidateRect(st->hAgendaCanvas, nullptr, FALSE);
    if (st->hCalCanvas) InvalidateRect(st->hCalCanvas, nullptr, FALSE);
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
    if (st->hExpStatsCanvas) InvalidateRect(st->hExpStatsCanvas, nullptr, FALSE);
    if (st->hMallaCanvas) InvalidateRect(st->hMallaCanvas, nullptr, FALSE);
    if (st->hAttrCanvas) InvalidateRect(st->hAttrCanvas, nullptr, FALSE);
}

void RefreshTramitesSection(TecHubState* st) {
    if (st->hTramitesCanvas) InvalidateRect(st->hTramitesCanvas, nullptr, FALSE);
}

void RefreshCacheUsageLabel(TecHubState* st) {
    if (!st->hLblCache) return;
    double mb = static_cast<double>(st->store->ComputeOfflineCacheBytes()) / (1024.0 * 1024.0);
    double maxMb = static_cast<double>(st->store->OfflineBudgetBytes()) / (1024.0 * 1024.0);
    std::string txt = "Almacenamiento local: " + FmtDouble(mb, 1) + " MB / " + FmtDouble(maxMb, 0) + " MB";
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

void LayoutDialog(TecHubState* st);
void SyncHorariosSourceVisibility(TecHubState* st);

void UpdateHorariosSolveButtonLabel(TecHubState* st) {
    if (!st || !st->hwnd) return;
    HWND hBtn = GetDlgItem(st->hwnd, IDC_BTN_HOR_SOLVE);
    if (!hBtn) return;
    std::wstring label = st->selectedHorCourseCodes.empty()
                             ? L"Armar horario"
                             : (L"Armar horario (" + std::to_wstring(st->selectedHorCourseCodes.size()) + L")");
    SetWindowTextW(hBtn, label.c_str());
}

void PopulateGlobalHorariosSchoolCombo(TecHubState* st) {
    if (!st || !st->hCmbHorGlobalSchool) return;
    std::string prevChoice;
    int prevIdx = static_cast<int>(SendMessageW(st->hCmbHorGlobalSchool, CB_GETCURSEL, 0, 0));
    if (prevIdx >= 0 && static_cast<size_t>(prevIdx) < st->globalSchoolChoices.size()) {
        prevChoice = st->globalSchoolChoices[static_cast<size_t>(prevIdx)];
    }

    st->globalSchoolChoices.clear();
    SendMessageW(st->hCmbHorGlobalSchool, CB_RESETCONTENT, 0, 0);
    st->globalSchoolChoices.push_back("");
    SendMessageW(st->hCmbHorGlobalSchool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todas las escuelas y carreras"));

    int restoreIdx = 0;
    std::set<std::string> seen;
    for (const auto& car : st->globalHorariosIdx.careers) {
        if (car.empty() || !seen.insert("car:" + car).second) continue;
        st->globalSchoolChoices.push_back(car);
        std::wstring lbl = L"Carrera · " + Utf8ToWide(car);
        SendMessageW(st->hCmbHorGlobalSchool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(lbl.c_str()));
        if (!prevChoice.empty() && prevChoice == car) {
            restoreIdx = static_cast<int>(st->globalSchoolChoices.size()) - 1;
        }
    }
    if (!st->globalHorariosIdx.schools.empty()) {
        for (const auto& sch : st->globalHorariosIdx.schools) {
            if (sch.empty() || !seen.insert("sch:" + sch).second) continue;
            st->globalSchoolChoices.push_back(sch);
            SendMessageW(st->hCmbHorGlobalSchool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(sch).c_str()));
            if (!prevChoice.empty() && prevChoice == sch) {
                restoreIdx = static_cast<int>(st->globalSchoolChoices.size()) - 1;
            }
        }
    } else {
        for (const auto& e : st->publicEscuelas) {
            if (e.dsc_depto.empty() || !seen.insert("sch:" + e.dsc_depto).second) continue;
            st->globalSchoolChoices.push_back(e.dsc_depto);
            SendMessageW(st->hCmbHorGlobalSchool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(e.dsc_depto).c_str()));
            if (!prevChoice.empty() && prevChoice == e.dsc_depto) {
                restoreIdx = static_cast<int>(st->globalSchoolChoices.size()) - 1;
            }
        }
    }
    SendMessageW(st->hCmbHorGlobalSchool, CB_SETCURSEL, static_cast<WPARAM>(restoreIdx), 0);
}

DocStudio::TecDigital::GlobalHorariosSearchFilter BuildGlobalHorariosFilterFromUi(TecHubState* st) {
    DocStudio::TecDigital::GlobalHorariosSearchFilter f;
    if (!st) return f;
    if (st->hEditHorGlobalQ) {
        f.queryUtf8 = TrimDialogUtf8(WideToUtf8(GetCtrlTextW(st->hEditHorGlobalQ)));
    }
    static const char* kSedes[6] = { "", "Cartago", "San José", "San Carlos", "Alajuela", "Limón" };
    int isede = st->hCmbHorPubSede ? static_cast<int>(SendMessageW(st->hCmbHorPubSede, CB_GETCURSEL, 0, 0)) : 0;
    if (isede >= 0 && isede < 6) f.campusFilter = kSedes[isede];

    int isch = st->hCmbHorGlobalSchool ? static_cast<int>(SendMessageW(st->hCmbHorGlobalSchool, CB_GETCURSEL, 0, 0)) : 0;
    if (isch >= 0 && static_cast<size_t>(isch) < st->globalSchoolChoices.size()) {
        f.schoolOrCareerFilter = st->globalSchoolChoices[static_cast<size_t>(isch)];
    }

    static const char* kPer[8] = { "", "S-1", "S-2", "V-1", "B", "C", "T", "H" };
    int iper = st->hCmbHorPubPer ? static_cast<int>(SendMessageW(st->hCmbHorPubPer, CB_GETCURSEL, 0, 0)) : 0;
    if (iper >= 0 && iper < 8) f.periodFilter = kPer[iper];

    static const char* kMods[7] = { "", "Regular", "Virtual", "Semipresencial", "Asistida", "Tutoría", "Grupo RN" };
    int imod = st->hCmbHorGlobalMod ? static_cast<int>(SendMessageW(st->hCmbHorGlobalMod, CB_GETCURSEL, 0, 0)) : 0;
    if (imod >= 0 && imod < 7) f.modalityFilter = kMods[imod];

    int iday = st->hCmbHorGlobalDay ? static_cast<int>(SendMessageW(st->hCmbHorGlobalDay, CB_GETCURSEL, 0, 0)) : 0;
    if (iday >= 1 && iday <= 6) f.dayFilter = iday - 1;

    f.excludeNight = st->hChkHorNoNight && (SendMessageW(st->hChkHorNoNight, BM_GETCHECK, 0, 0) == BST_CHECKED);
    f.onlyWithSeats = st->hChkHorSeats && (SendMessageW(st->hChkHorSeats, BM_GETCHECK, 0, 0) == BST_CHECKED);
    f.onlyEligibleByCurriculum = st->hChkHorPrereq && (SendMessageW(st->hChkHorPrereq, BM_GETCHECK, 0, 0) == BST_CHECKED);
    f.maxResults = 400;
    return f;
}

void RefreshGlobalHorariosSearch(TecHubState* st) {
    if (!st || !st->store) return;
    auto filter = BuildGlobalHorariosFilterFromUi(st);
    std::size_t totalMatches = 0;
    st->horSearchResults = st->store->SearchGlobalScheduleIndex(filter, &totalMatches);
    st->horSearchTotalMatches = totalMatches;
    st->horSearchScrollY = 0;
    st->horSearchHoverIdx = -1;
    UpdateHorariosSolveButtonLabel(st);
    if (st->hHorStatus && st->horViewMode == 0) {
        if (st->globalHorariosIdx.entries.empty()) {
            SetWindowTextW(st->hHorStatus, L"Presiona Indexar todo el TEC para consultar todas las escuelas, sedes y carreras.");
        } else {
            std::wstring statusW = L"Mostrando " + std::to_wstring(st->horSearchResults.size()) +
                                   L" de " + std::to_wstring(st->horSearchTotalMatches) + L" grupos";
            if (!st->selectedHorCourseCodes.empty()) {
                statusW += L" · " + std::to_wstring(st->selectedHorCourseCodes.size()) +
                           L" cursos listos para armar horario";
            }
            SetWindowTextW(st->hHorStatus, statusW.c_str());
        }
    }
    if (st->hHorSearchCanvas) InvalidateRect(st->hHorSearchCanvas, nullptr, FALSE);
}

void ToggleSelectedHorCourse(TecHubState* st, const std::string& courseCode) {
    if (!st || courseCode.empty()) return;
    auto it = std::find(st->selectedHorCourseCodes.begin(), st->selectedHorCourseCodes.end(), courseCode);
    if (it != st->selectedHorCourseCodes.end()) {
        st->selectedHorCourseCodes.erase(it);
    } else {
        st->selectedHorCourseCodes.push_back(courseCode);
    }
    UpdateHorariosSolveButtonLabel(st);
    if (st->hHorStatus && st->horViewMode == 0 && !st->globalHorariosIdx.entries.empty()) {
        std::wstring statusW = L"Mostrando " + std::to_wstring(st->horSearchResults.size()) +
                               L" de " + std::to_wstring(st->horSearchTotalMatches) + L" grupos";
        if (!st->selectedHorCourseCodes.empty()) {
            statusW += L" · " + std::to_wstring(st->selectedHorCourseCodes.size()) +
                       L" cursos listos para armar horario";
        }
        SetWindowTextW(st->hHorStatus, statusW.c_str());
    }
    if (st->hHorSearchCanvas) InvalidateRect(st->hHorSearchCanvas, nullptr, FALSE);
    if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
}

void ClearSelectedHorCourses(TecHubState* st) {
    if (!st) return;
    st->selectedHorCourseCodes.clear();
    UpdateHorariosSolveButtonLabel(st);
    if (st->hHorStatus && st->horViewMode == 0 && !st->globalHorariosIdx.entries.empty()) {
        std::wstring statusW = L"Mostrando " + std::to_wstring(st->horSearchResults.size()) +
                               L" de " + std::to_wstring(st->horSearchTotalMatches) + L" grupos";
        SetWindowTextW(st->hHorStatus, statusW.c_str());
    }
    if (st->hHorSearchCanvas) InvalidateRect(st->hHorSearchCanvas, nullptr, FALSE);
}

void RefreshPlannerFooterStatus(TecHubState* st) {
    if (!st || !st->hHorStatus) return;
    if (st->horariosResult.HasSolutions() && st->selectedScheduleIdx < st->horariosResult.schedules.size()) {
        const auto& cur = st->horariosResult.schedules[st->selectedScheduleIdx];
        std::string s = "Opción " + std::to_string(st->selectedScheduleIdx + 1) + "/" +
                        std::to_string(st->horariosResult.schedules.size()) +
                        " · " + std::to_string(cur.metrics.totalCredits) + " cr · " +
                        std::to_string(cur.metrics.activeDaysCount) + " días · " +
                        std::to_string(cur.metrics.totalGapMinutes) + " min huecos";
        SetWindowTextW(st->hHorStatus, Utf8ToWide(s).c_str());
    } else if (!st->horariosCatalog.courses.empty()) {
        std::string s = "Catálogo cargado (" + std::to_string(st->horariosCatalog.courses.size()) +
                        " cursos) · 0 combinaciones sin choques con los filtros actuales";
        SetWindowTextW(st->hHorStatus, Utf8ToWide(s).c_str());
    } else {
        SetWindowTextW(st->hHorStatus, L"0 combinaciones · Agrega cursos en Buscador global");
    }
}

void SwitchHorariosViewMode(TecHubState* st, int mode) {
    if (!st) return;
    st->horViewMode = (mode == 1) ? 1 : 0;
    st->horModeHover = -1;
    SyncHorariosSourceVisibility(st);
    LayoutDialog(st);
    UpdateHorariosSolveButtonLabel(st);
    if (st->horViewMode == 0) {
        RefreshGlobalHorariosSearch(st);
    } else {
        RefreshPlannerFooterStatus(st);
        if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
    }
    UpdateActiveFramedPanels(st);
    InvalidateRect(st->hwnd, nullptr, FALSE);
}

void StartAsyncGlobalHorariosIndex(TecHubState* st) {
    if (!st || !st->store) return;
    if (st->workerBusy.exchange(true)) {
        SetWindowTextW(st->hProgressBanner, L"Hay otra consulta en curso, espere un momento.");
        return;
    }
    int iy = st->hCmbHorYear ? static_cast<int>(SendMessageW(st->hCmbHorYear, CB_GETCURSEL, 0, 0)) : 0;
    int year = 2026;
    if (iy == 1) year = 2025;
    else if (iy == 2) year = 2027;

    SetWindowTextW(st->hProgressBanner, Utf8ToWide("Indexando todas las escuelas y sedes del TEC (" + std::to_string(year) + ")...").c_str());
    if (st->hHorStatus) SetWindowTextW(st->hHorStatus, L"Indexando catálogo global del TEC...");
    if (st->workerThread.joinable()) st->workerThread.join();

    st->workerThread = std::thread([st, year]() {
        std::string err;
        const bool ok = st->store->BuildGlobalScheduleIndex(
            year,
            [st](int pct, const std::string& msg) {
                {
                    std::lock_guard<std::mutex> lk(st->workResultMutex);
                    st->pendingWorkStatus = "[" + std::to_string(pct) + "%] " + msg;
                }
                PostMessageW(st->hwnd, WM_APP_TEC_PROGRESS, 0, 0);
            },
            &st->workerCancel,
            &err,
            8);
        auto idx = st->store->GetGlobalScheduleIndex();
        std::string summary;
        if (ok) {
            summary = "Índice global " + std::to_string(idx.year) + ": " +
                      std::to_string(idx.uniqueCoursesCount) + " cursos, " +
                      std::to_string(idx.entries.size()) + " grupos, " +
                      std::to_string(idx.schoolsWithCourses > 0 ? static_cast<size_t>(idx.schoolsWithCourses) : idx.schools.size()) +
                      " escuelas y " + std::to_string(idx.campuses.size()) + " sedes.";
        } else {
            summary = err.empty() ? "No se pudo indexar los horarios del TEC." : err;
        }
        {
            std::lock_guard<std::mutex> lk(st->workResultMutex);
            st->pendingGlobalIndexOk = ok;
            st->pendingGlobalIndexStatus = summary;
        }
        st->workerBusy.store(false);
        PostMessageW(st->hwnd, WM_APP_TEC_GLOBAL_HORARIOS_DONE, ok ? 1 : 0, 0);
    });
}

void SyncHorariosSourceVisibility(TecHubState* st) {
    if (!st) return;
    const bool isSearch = (st->horViewMode == 0);
    int horSrc = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
    if (horSrc < 0 || horSrc > 3) horSrc = 0;
    const bool exp = st->horFiltersExpanded;
    auto sw = [](HWND h, bool show) {
        if (h) ShowWindow(h, show ? SW_SHOW : SW_HIDE);
    };

    // Mode 0 (Global Search) controls
    sw(st->hEditHorGlobalQ, isSearch);
    sw(st->hBtnHorIndexAll, isSearch);
    sw(st->hCmbHorGlobalSchool, isSearch);
    sw(st->hCmbHorGlobalMod, isSearch);
    sw(st->hCmbHorGlobalDay, isSearch);
    sw(st->hHorSearchCanvas, isSearch);

    // Shared controls (repositioned by LayoutDialog per mode)
    sw(st->hCmbHorYear, isSearch || (horSrc == 1) || (exp && (horSrc == 0 || horSrc == 2)));
    sw(st->hCmbHorPubSede, isSearch || (exp && (horSrc == 0 || horSrc == 1 || horSrc == 2)));
    sw(st->hCmbHorPubPer, isSearch || (exp && (horSrc == 0 || horSrc == 2)));
    sw(GetDlgItem(st->hwnd, IDC_BTN_HOR_SOLVE), true);
    sw(GetDlgItem(st->hwnd, IDC_BTN_HOR_LOAD_JSON), isSearch || exp);
    sw(st->hHorStatus, true);

    // Mode 1 (Weekly Conflict-Free Planner) controls
    sw(st->hCmbHorSource, !isSearch);
    sw(st->hCmbHorEscuela, !isSearch && horSrc == 0);
    sw(st->hEditHorCarnet, !isSearch && horSrc == 2);
    sw(st->hCmbHorCarrera, !isSearch && horSrc == 3);
    sw(st->hCmbHorSede, !isSearch && exp && horSrc == 3);
    sw(st->hCmbHorPeriodo, !isSearch && exp && horSrc == 3);
    sw(st->hChkHorPrereq, !isSearch && exp);
    sw(st->hChkHorNoNight, !isSearch && exp);
    sw(st->hChkHorSeats, !isSearch && exp);
    sw(st->hChkHorBusy, !isSearch && exp);
    sw(st->hCmbHorFreeDay, !isSearch && exp);
    sw(st->hEditHorFilter, !isSearch && exp);
    sw(st->hBtnHorFilters, !isSearch);
    sw(st->hHorCanvas, !isSearch);
    sw(GetDlgItem(st->hwnd, IDC_BTN_HOR_PREV), !isSearch);
    sw(GetDlgItem(st->hwnd, IDC_BTN_HOR_NEXT), !isSearch);
    sw(GetDlgItem(st->hwnd, IDC_BTN_HOR_INSERT), !isSearch);
    sw(GetDlgItem(st->hwnd, IDC_BTN_HOR_EXPORT), !isSearch);

    if (st->hBtnHorFilters) {
        SetWindowTextW(st->hBtnHorFilters, exp ? L"Menos filtros" : L"Filtros");
    }
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

    static const char* kPeriods[8] = { "", "S-1", "S-2", "V-1", "B", "C", "T", "H" };
    int iper = st->hCmbHorPubPer ? static_cast<int>(SendMessageW(st->hCmbHorPubPer, CB_GETCURSEL, 0, 0)) : 0;
    if (outSource != 1 && iper >= 0 && iper < 8) outPubQuery.filter.periodo_filter = kPeriods[iper];

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
                                          : L"Consultando horarios del TEC...");
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
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todos los espacios"));
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Semestre actual"));
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todos los cursos"));
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cátedras"));
        SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Escuelas y Comunidades"));
        std::set<std::string> periods;
        for (const auto& c : st->snap.communities) {
            if (c.type == "CLASE" && !c.period.empty()) periods.insert(c.period);
        }
        for (auto it = periods.rbegin(); it != periods.rend(); ++it) {
            SendMessageW(st->hCmbPeriod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(Utf8ToWide(*it).c_str()));
        }
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
    st->globalHorariosIdx = st->store->GetGlobalScheduleIndex();
    st->globalLibCatalog = DocStudio::TecDigital::TecAcademicStore::BuildGlobalFileCatalog(st->snap);
    RefreshGlobalLibraryFilter(st);
    PopulateComboSelectorsFromSnapshot(st);
    PopulateGlobalHorariosSchoolCombo(st);
    RefreshGlobalHorariosSearch(st);
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
        L"Inicio",
        L"Cursos",
        L"Calendario",
        L"Expediente y Malla",
        L"Horarios",
        L"Biblioteca",
        L"Trámites"
    };
    static const wchar_t* kSubs[7] = {
        L"Resumen de tus cursos activos, próxima clase, entregas y avisos.",
        L"Portada, documentos sin conexión, evaluaciones y simulador de nota.",
        L"Horario semanal, entregas próximas y citas de matrícula.",
        L"Promedio ponderado, historial de notas, malla curricular y atributos.",
        L"Busca cursos de todas las escuelas, sedes y carreras del TEC o arma horarios sin choques.",
        L"Busca al instante en tus materiales descargados y cuaderno activo.",
        L"Inclusiones, becas, control de plagio, prematrícula y servicios TEC."
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

    if (sec == HubSection::Resumen) {
        const bool authedOrCached = st->store &&
            (st->store->IsAuthenticated() || st->snap.sessionAuthenticated || !st->snap.communities.empty());
        const bool showLogin = (!authedOrCached || st->showLoginInline);
        if (st->hEmail) ShowWindow(st->hEmail, showLogin ? SW_SHOW : SW_HIDE);
        if (st->hPass) ShowWindow(st->hPass, showLogin ? SW_SHOW : SW_HIDE);
        if (st->hChkDpapi) ShowWindow(st->hChkDpapi, showLogin ? SW_SHOW : SW_HIDE);
        if (st->hBtnLogin) ShowWindow(st->hBtnLogin, showLogin ? SW_SHOW : SW_HIDE);
        if (st->hBtnQr) ShowWindow(st->hBtnQr, showLogin ? SW_SHOW : SW_HIDE);
        if (st->hBtnLogout) {
            const bool authed = st->store && (st->store->IsAuthenticated() || st->snap.sessionAuthenticated);
            ShowWindow(st->hBtnLogout, authed ? SW_SHOW : SW_HIDE);
        }
        if (st->hSummaryEdit) ShowWindow(st->hSummaryEdit, SW_HIDE);
        if (st->hSummaryCanvas) {
            ShowWindow(st->hSummaryCanvas, SW_SHOW);
            InvalidateRect(st->hSummaryCanvas, nullptr, FALSE);
        }
        LayoutDialog(st);
    }

    if (sec == HubSection::Cursos) {
        const bool showDocBar = (st->courseSubTab == 1);
        const bool showWhatIf = (st->courseSubTab == 2);
        if (st->hLstCourses) ShowWindow(st->hLstCourses, SW_HIDE);
        if (st->hCmbCourseSub) ShowWindow(st->hCmbCourseSub, SW_HIDE);
        if (st->hCourseDetail) ShowWindow(st->hCourseDetail, SW_HIDE);
        if (st->hLstCourseFiles) ShowWindow(st->hLstCourseFiles, SW_HIDE);
        if (st->hEditCourseDocSearch) ShowWindow(st->hEditCourseDocSearch, showDocBar ? SW_SHOW : SW_HIDE);
        if (st->hCmbCourseDocSort) ShowWindow(st->hCmbCourseDocSort, showDocBar ? SW_SHOW : SW_HIDE);
        if (st->hCourseListCanvas) ShowWindow(st->hCourseListCanvas, SW_SHOW);
        if (st->hCourseDetailCanvas) ShowWindow(st->hCourseDetailCanvas, SW_SHOW);
        if (st->hEditWhatIf) ShowWindow(st->hEditWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnWhatIf) ShowWindow(st->hBtnWhatIf, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseRubric) ShowWindow(st->hBtnCourseRubric, showWhatIf ? SW_SHOW : SW_HIDE);
        if (st->hBtnCourseUploadEval) ShowWindow(st->hBtnCourseUploadEval, showWhatIf ? SW_SHOW : SW_HIDE);
        HWND hBtnIns = GetDlgItem(st->hwnd, IDC_BTN_COURSE_INSERT_CANVAS);
        if (hBtnIns) ShowWindow(hBtnIns, showWhatIf ? SW_HIDE : SW_SHOW);
        HWND hBtnSyncFiles = GetDlgItem(st->hwnd, IDC_BTN_COURSE_SYNC_FILES);
        if (hBtnSyncFiles) ShowWindow(hBtnSyncFiles, showWhatIf ? SW_HIDE : SW_SHOW);
        if (st->hBtnCourseOpenWeb) ShowWindow(st->hBtnCourseOpenWeb, SW_HIDE);
        if (st->hBtnCourseGemini) ShowWindow(st->hBtnCourseGemini, SW_HIDE);
        LayoutDialog(st);
        if (st->hCourseListCanvas) InvalidateRect(st->hCourseListCanvas, nullptr, FALSE);
        if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
    }

    if (sec == HubSection::Calendario) {
        int calMode = st->hCmbCalView ? static_cast<int>(SendMessageW(st->hCmbCalView, CB_GETCURSEL, 0, 0)) : 1;
        const bool showGrid = (calMode == 1 || calMode == 2);
        if (st->hCalList) ShowWindow(st->hCalList, SW_HIDE);
        if (st->hAgendaCanvas) {
            ShowWindow(st->hAgendaCanvas, showGrid ? SW_HIDE : SW_SHOW);
            if (!showGrid) InvalidateRect(st->hAgendaCanvas, nullptr, FALSE);
        }
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
        if (st->hCmbGradesFilter) ShowWindow(st->hCmbGradesFilter, SW_HIDE);
        if (st->hExpReport) ShowWindow(st->hExpReport, SW_HIDE);
        if (st->hExpStatsCanvas) ShowWindow(st->hExpStatsCanvas, showStats ? SW_SHOW : SW_HIDE);
        if (st->hBtnMallaPicker) {
            ShowWindow(st->hBtnMallaPicker, showMalla ? SW_SHOW : SW_HIDE);
            SetWindowTextW(st->hBtnMallaPicker, st->mallaPickerExpanded ? L"Ocultar plan" : L"Cambiar plan");
        }
        const bool showPickerRow = showMalla && st->mallaPickerExpanded;
        if (st->hCmbMallaSede) ShowWindow(st->hCmbMallaSede, showPickerRow ? SW_SHOW : SW_HIDE);
        if (st->hCmbMallaCarrera) ShowWindow(st->hCmbMallaCarrera, showPickerRow ? SW_SHOW : SW_HIDE);
        if (st->hCmbMallaPlan) ShowWindow(st->hCmbMallaPlan, showPickerRow ? SW_SHOW : SW_HIDE);
        if (st->hBtnMallaLoad) ShowWindow(st->hBtnMallaLoad, showPickerRow ? SW_SHOW : SW_HIDE);
        if (st->hMallaCanvas) ShowWindow(st->hMallaCanvas, showMalla ? SW_SHOW : SW_HIDE);
        if (st->hBtnMallaInsert) ShowWindow(st->hBtnMallaInsert, showMalla ? SW_SHOW : SW_HIDE);
        if (st->hBtnMallaZIn) ShowWindow(st->hBtnMallaZIn, showMalla ? SW_SHOW : SW_HIDE);
        if (st->hBtnMallaZOut) ShowWindow(st->hBtnMallaZOut, showMalla ? SW_SHOW : SW_HIDE);
        if (st->hAttrCanvas) ShowWindow(st->hAttrCanvas, showAttr ? SW_SHOW : SW_HIDE);
        if (st->hMallaInspector) ShowWindow(st->hMallaInspector, (showMalla || showAttrInsp) ? SW_SHOW : SW_HIDE);
        LayoutDialog(st);
        if (showStats && st->hExpStatsCanvas) {
            InvalidateRect(st->hExpStatsCanvas, nullptr, FALSE);
        } else if (showAttr) {
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
        UpdateHorariosSolveButtonLabel(st);
        if (st->horViewMode == 0) {
            RefreshGlobalHorariosSearch(st);
        } else {
            RefreshPlannerFooterStatus(st);
            if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
        }
    }
    if (sec == HubSection::BibliotecaOffline) {
        if (st->hLstFts) ShowWindow(st->hLstFts, SW_HIDE);
        if (st->hFtsPreview) ShowWindow(st->hFtsPreview, SW_HIDE);
        if (st->hFtsCanvas) {
            ShowWindow(st->hFtsCanvas, SW_SHOW);
            InvalidateRect(st->hFtsCanvas, nullptr, FALSE);
        }
    }
    if (sec == HubSection::TramitesYPerfil) {
        if (st->hCmbTramite) ShowWindow(st->hCmbTramite, SW_HIDE);
        if (st->hTramiteReport) ShowWindow(st->hTramiteReport, SW_HIDE);
        if (st->hBtnSvcMapa) ShowWindow(st->hBtnSvcMapa, SW_HIDE);
        if (st->hBtnSvcGuia) ShowWindow(st->hBtnSvcGuia, SW_HIDE);
        if (st->hBtnSvcSoporte) ShowWindow(st->hBtnSvcSoporte, SW_HIDE);
        if (st->hTramitesCanvas) {
            ShowWindow(st->hTramitesCanvas, SW_SHOW);
            InvalidateRect(st->hTramitesCanvas, nullptr, FALSE);
        }
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

    // Section 0: Inicio & Cuenta
    {
        const bool authedOrCached = st->store &&
            (st->store->IsAuthenticated() || st->snap.sessionAuthenticated || !st->snap.communities.empty());
        const bool compactStrip = authedOrCached && !st->showLoginInline;
        int topSum = cy + st->Sc(88);
        if (compactStrip) {
            st->panelAccountBand = { cx, cy, cx + cw, cy + st->Sc(42) };
            st->rcInEmail = {};
            st->rcInPass = {};
            if (st->hBtnLogout) {
                MoveWindow(st->hBtnLogout, cx + cw - st->Sc(126), cy + st->Sc(7), st->Sc(116), st->Sc(28), TRUE);
            }
            topSum = cy + st->Sc(52);
        } else {
            st->panelAccountBand = { cx, cy, cx + cw, cy + st->Sc(76) };
            const int rowH = st->Sc(30);
            const int rowY = cy + st->Sc(12);
            const int ew = std::min(st->Sc(220), (cw - st->Sc(220)) / 2);
            st->rcInEmail = { cx + st->Sc(12), rowY, cx + st->Sc(12) + ew, rowY + rowH };
            MoveWindow(st->hEmail, st->rcInEmail.left + st->Sc(8), rowY + st->Sc(6), std::max(40, ew - st->Sc(16)), st->Sc(18), TRUE);
            st->rcInPass = { st->rcInEmail.right + st->Sc(8), rowY, st->rcInEmail.right + st->Sc(8) + ew, rowY + rowH };
            MoveWindow(st->hPass, st->rcInPass.left + st->Sc(8), rowY + st->Sc(6), std::max(40, ew - st->Sc(16)), st->Sc(18), TRUE);
            int bx = st->rcInPass.right + st->Sc(10);
            MoveWindow(st->hBtnLogin, bx, rowY, st->Sc(114), rowH, TRUE);
            MoveWindow(st->hBtnQr, bx + st->Sc(120), rowY, st->Sc(54), rowH, TRUE);
            MoveWindow(st->hBtnLogout, cx + cw - st->Sc(126), rowY, st->Sc(116), rowH, TRUE);
            MoveWindow(st->hChkDpapi, cx + st->Sc(12), cy + st->Sc(48), st->Sc(340), st->Sc(20), TRUE);
            topSum = cy + st->Sc(88);
        }
        int sumH = std::max(st->Sc(140), ch - (topSum - cy));
        if (st->hSummaryCanvas) MoveWindow(st->hSummaryCanvas, cx, topSum, cw, sumH, TRUE);
        if (st->hSummaryEdit) MoveWindow(st->hSummaryEdit, cx, topSum, cw, sumH, TRUE);
    }

    // Section 1: Cursos (guías de interfaz Master-Detail Vector Canvases)
    {
        int leftW = std::min(st->Sc(276), cw / 3);
        int rx = cx + leftW + st->Sc(14);
        int rw = cw - leftW - st->Sc(14);
        MoveWindow(st->hCmbPeriod, cx, cy, leftW, st->Sc(220), TRUE);
        if (st->hCourseListCanvas) {
            MoveWindow(st->hCourseListCanvas, cx, cy + st->Sc(36), leftW, std::max(st->Sc(120), ch - st->Sc(36)), TRUE);
        }

        int detailTop = cy;
        int detailH = std::max(st->Sc(140), ch - st->Sc(40));
        if (st->courseSubTab == 1) {
            const int sortW = st->Sc(156);
            const int srchW = std::max(st->Sc(120), rw - sortW - st->Sc(8));
            st->rcInCourseDocSearch = { rx, cy, rx + srchW, cy + st->Sc(30) };
            if (st->hEditCourseDocSearch) {
                MoveWindow(st->hEditCourseDocSearch, st->rcInCourseDocSearch.left + st->Sc(8), cy + st->Sc(6),
                           std::max(40, srchW - st->Sc(16)), st->Sc(18), TRUE);
            }
            if (st->hCmbCourseDocSort) {
                MoveWindow(st->hCmbCourseDocSort, rx + srchW + st->Sc(8), cy, sortW, st->Sc(180), TRUE);
            }
            detailTop = cy + st->Sc(36);
            detailH = std::max(st->Sc(120), ch - st->Sc(76));
        } else {
            st->rcInCourseDocSearch = {};
        }
        if (st->hCourseDetailCanvas) {
            MoveWindow(st->hCourseDetailCanvas, rx, detailTop, rw, detailH, TRUE);
        }

        int btnY = cy + ch - st->Sc(30);
        if (st->courseSubTab == 2) {
            st->rcInWhatIf = { rx, btnY, rx + st->Sc(48), btnY + st->Sc(30) };
            MoveWindow(st->hEditWhatIf, st->rcInWhatIf.left + st->Sc(8), btnY + st->Sc(6), st->Sc(32), st->Sc(18), TRUE);
            if (st->hBtnWhatIf) MoveWindow(st->hBtnWhatIf, rx + st->Sc(54), btnY, st->Sc(96), st->Sc(30), TRUE);
            if (st->hBtnCourseRubric) MoveWindow(st->hBtnCourseRubric, rx + st->Sc(156), btnY, st->Sc(74), st->Sc(30), TRUE);
            HWND hBtnMore = GetDlgItem(st->hwnd, IDC_BTN_COURSE_MORE);
            if (st->hBtnCourseUploadEval) MoveWindow(st->hBtnCourseUploadEval, rx + rw - st->Sc(256), btnY, st->Sc(132), st->Sc(30), TRUE);
            if (hBtnMore) MoveWindow(hBtnMore, rx + rw - st->Sc(118), btnY, st->Sc(118), st->Sc(30), TRUE);
        } else {
            st->rcInWhatIf = {};
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_COURSE_INSERT_CANVAS), rx, btnY, st->Sc(142), st->Sc(30), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_COURSE_SYNC_FILES), rx + st->Sc(150), btnY, st->Sc(152), st->Sc(30), TRUE);
            HWND hBtnMore = GetDlgItem(st->hwnd, IDC_BTN_COURSE_MORE);
            if (hBtnMore) MoveWindow(hBtnMore, rx + rw - st->Sc(128), btnY, st->Sc(128), st->Sc(30), TRUE);
        }
    }

    // Section 2: Calendario
    {
        MoveWindow(st->hCmbCalFilter, cx, cy, st->Sc(200), st->Sc(180), TRUE);
        if (st->hCmbCalView) MoveWindow(st->hCmbCalView, cx + st->Sc(208), cy, st->Sc(156), st->Sc(180), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_CAL_EXPORT_ICS), cx + cw - st->Sc(138), cy, st->Sc(138), st->Sc(28), TRUE);
        if (st->hAgendaCanvas) MoveWindow(st->hAgendaCanvas, cx, cy + st->Sc(38), cw, ch - st->Sc(38), TRUE);
        if (st->hCalCanvas) MoveWindow(st->hCalCanvas, cx, cy + st->Sc(38), cw, ch - st->Sc(38), TRUE);
    }

    // Section 3: Expediente y Malla Curricular
    {
        MoveWindow(st->hCmbExpView, cx, cy, st->Sc(228), st->Sc(180), TRUE);
        if (st->hBtnMallaPicker) MoveWindow(st->hBtnMallaPicker, cx + st->Sc(236), cy, st->Sc(112), st->Sc(28), TRUE);
        MoveWindow(st->hBtnMallaZIn, cx + st->Sc(354), cy, st->Sc(64), st->Sc(28), TRUE);
        MoveWindow(st->hBtnMallaZOut, cx + st->Sc(424), cy, st->Sc(64), st->Sc(28), TRUE);
        MoveWindow(st->hBtnMallaInsert, cx + cw - st->Sc(178), cy, st->Sc(178), st->Sc(28), TRUE);

        int mRow2 = cy + st->Sc(36);
        int mcbW = std::max(st->Sc(110), (cw - st->Sc(122)) / 3);
        if (st->hCmbMallaSede) MoveWindow(st->hCmbMallaSede, cx, mRow2, mcbW, st->Sc(180), TRUE);
        if (st->hCmbMallaCarrera) MoveWindow(st->hCmbMallaCarrera, cx + mcbW + st->Sc(6), mRow2, mcbW, st->Sc(180), TRUE);
        if (st->hCmbMallaPlan) MoveWindow(st->hCmbMallaPlan, cx + 2 * (mcbW + st->Sc(6)), mRow2, mcbW, st->Sc(180), TRUE);
        if (st->hBtnMallaLoad) MoveWindow(st->hBtnMallaLoad, cx + cw - st->Sc(102), mRow2, st->Sc(102), st->Sc(28), TRUE);

        if (st->hExpStatsCanvas) {
            MoveWindow(st->hExpStatsCanvas, cx, cy + st->Sc(38), cw, std::max(st->Sc(160), ch - st->Sc(38)), TRUE);
        }

        int inspH = st->Sc(92);
        int mallaTop = st->mallaPickerExpanded ? (mRow2 + st->Sc(34)) : (cy + st->Sc(38));
        MoveWindow(st->hMallaCanvas, cx, mallaTop, cw, std::max(st->Sc(140), ch - (mallaTop - cy) - inspH - st->Sc(10)), TRUE);
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

    // Section 4: Buscador Global y Planificador Horarios
    {
        const int g = st->Sc(8);
        if (st->horViewMode == 0) {
            st->rcInHorCarnet = {};
            st->rcInHorFilter = {};
            const int idxBtnW = st->Sc(146);
            const int solveBtnW = st->Sc(138);
            const int qW = std::max(st->Sc(160), cw - idxBtnW - solveBtnW - 2 * g);
            st->rcInHorGlobalQ = { cx, cy, cx + qW, cy + st->Sc(30) };
            if (st->hEditHorGlobalQ) {
                MoveWindow(st->hEditHorGlobalQ, st->rcInHorGlobalQ.left + st->Sc(8), cy + st->Sc(6),
                           std::max(40, qW - st->Sc(16)), st->Sc(18), TRUE);
            }
            if (st->hBtnHorIndexAll) MoveWindow(st->hBtnHorIndexAll, cx + qW + g, cy, idxBtnW, st->Sc(30), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_SOLVE), cx + cw - solveBtnW, cy, solveBtnW, st->Sc(30), TRUE);

            // Row 2: 6 clean facet selectors (Sede, Escuela/Carrera, Periodo, Modalidad, Día, Año)
            const int r2 = cy + st->Sc(36);
            const int yrW = st->Sc(74);
            const int dayW = st->Sc(106);
            const int modW = st->Sc(144);
            const int perW = st->Sc(130);
            const int sedeW = st->Sc(122);
            const int schW = std::max(st->Sc(120), cw - sedeW - perW - modW - dayW - yrW - 5 * g);
            int fx = cx;
            if (st->hCmbHorPubSede) { MoveWindow(st->hCmbHorPubSede, fx, r2, sedeW, st->Sc(220), TRUE); fx += sedeW + g; }
            if (st->hCmbHorGlobalSchool) { MoveWindow(st->hCmbHorGlobalSchool, fx, r2, schW, st->Sc(280), TRUE); fx += schW + g; }
            if (st->hCmbHorPubPer) { MoveWindow(st->hCmbHorPubPer, fx, r2, perW, st->Sc(220), TRUE); fx += perW + g; }
            if (st->hCmbHorGlobalMod) { MoveWindow(st->hCmbHorGlobalMod, fx, r2, modW, st->Sc(220), TRUE); fx += modW + g; }
            if (st->hCmbHorGlobalDay) { MoveWindow(st->hCmbHorGlobalDay, fx, r2, dayW, st->Sc(220), TRUE); fx += dayW + g; }
            if (st->hCmbHorYear) { MoveWindow(st->hCmbHorYear, cx + cw - yrW, r2, yrW, st->Sc(200), TRUE); }

            const int canvasTop = r2 + st->Sc(34);
            const int footerH = st->Sc(38);
            if (st->hHorSearchCanvas) {
                MoveWindow(st->hHorSearchCanvas, cx, canvasTop, cw, std::max(st->Sc(140), ch - (canvasTop - cy) - footerH), TRUE);
            }

            const int fy = cy + ch - st->Sc(30);
            const int optW = st->Sc(112);
            MoveWindow(st->hHorStatus, cx, fy + st->Sc(5), std::max(st->Sc(120), cw - optW - g), st->Sc(20), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_LOAD_JSON), cx + cw - optW, fy, optW, st->Sc(28), TRUE);
        } else {
            st->rcInHorGlobalQ = {};
            const int srcW = std::min(cw / 3, std::max(st->Sc(190), (cw * 24) / 100));
            const int filtBtnW = st->Sc(104);
            const int solveBtnW = st->Sc(138);
            if (st->hCmbHorSource) {
                MoveWindow(st->hCmbHorSource, cx, cy, srcW, st->Sc(200), TRUE);
            }
            const int horSrc = st->hCmbHorSource ? static_cast<int>(SendMessageW(st->hCmbHorSource, CB_GETCURSEL, 0, 0)) : 0;
            const int rx0 = cx + srcW + g;
            const int midW = std::max(st->Sc(120), cw - srcW - filtBtnW - solveBtnW - 3 * g);
            st->rcInHorCarnet = {};

            if (horSrc == 0) {
                if (st->hCmbHorEscuela) MoveWindow(st->hCmbHorEscuela, rx0, cy, midW, st->Sc(260), TRUE);
            } else if (horSrc == 1) {
                if (st->hCmbHorYear) MoveWindow(st->hCmbHorYear, rx0, cy, midW, st->Sc(200), TRUE);
            } else if (horSrc == 2) {
                st->rcInHorCarnet = { rx0, cy, rx0 + midW, cy + st->Sc(28) };
                if (st->hEditHorCarnet) {
                    MoveWindow(st->hEditHorCarnet, st->rcInHorCarnet.left + st->Sc(8), cy + st->Sc(5),
                               std::max(40, midW - st->Sc(16)), st->Sc(18), TRUE);
                }
            } else {
                if (st->hCmbHorCarrera) MoveWindow(st->hCmbHorCarrera, rx0, cy, midW, st->Sc(200), TRUE);
            }

            if (st->hBtnHorFilters) MoveWindow(st->hBtnHorFilters, cx + cw - solveBtnW - g - filtBtnW, cy, filtBtnW, st->Sc(28), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_SOLVE), cx + cw - solveBtnW, cy, solveBtnW, st->Sc(28), TRUE);

            int gridTop = cy + st->Sc(38);
            if (st->horFiltersExpanded) {
                int r2 = cy + st->Sc(36);
                const int loadW = st->Sc(106);
                const int remR2 = std::max(st->Sc(120), cw - loadW - g);
                if (horSrc == 3) {
                    const int halfW = (remR2 - g) / 2;
                    if (st->hCmbHorSede) MoveWindow(st->hCmbHorSede, cx, r2, halfW, st->Sc(200), TRUE);
                    if (st->hCmbHorPeriodo) MoveWindow(st->hCmbHorPeriodo, cx + halfW + g, r2, remR2 - halfW - g, st->Sc(200), TRUE);
                } else if (horSrc == 1) {
                    if (st->hCmbHorPubSede) MoveWindow(st->hCmbHorPubSede, cx, r2, remR2, st->Sc(200), TRUE);
                } else {
                    const int w3 = (remR2 - 2 * g) / 3;
                    if (st->hCmbHorYear) MoveWindow(st->hCmbHorYear, cx, r2, w3, st->Sc(200), TRUE);
                    if (st->hCmbHorPubSede) MoveWindow(st->hCmbHorPubSede, cx + w3 + g, r2, w3, st->Sc(200), TRUE);
                    if (st->hCmbHorPubPer) MoveWindow(st->hCmbHorPubPer, cx + 2 * (w3 + g), r2, remR2 - 2 * (w3 + g), st->Sc(200), TRUE);
                }
                MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_LOAD_JSON), cx + cw - loadW, r2, loadW, st->Sc(28), TRUE);

                int r3 = r2 + st->Sc(34);
                MoveWindow(st->hChkHorPrereq, cx, r3, st->Sc(176), st->Sc(22), TRUE);
                MoveWindow(st->hChkHorNoNight, cx + st->Sc(180), r3, st->Sc(110), st->Sc(22), TRUE);
                MoveWindow(st->hChkHorSeats, cx + st->Sc(294), r3, st->Sc(110), st->Sc(22), TRUE);
                if (st->hChkHorBusy) MoveWindow(st->hChkHorBusy, cx + st->Sc(408), r3, st->Sc(146), st->Sc(22), TRUE);

                int r4 = r3 + st->Sc(28);
                if (st->hCmbHorFreeDay) MoveWindow(st->hCmbHorFreeDay, cx, r4, st->Sc(172), st->Sc(180), TRUE);
                st->rcInHorFilter = { cx + st->Sc(180), r4, cx + cw, r4 + st->Sc(28) };
                if (st->hEditHorFilter) {
                    MoveWindow(st->hEditHorFilter, st->rcInHorFilter.left + st->Sc(8), r4 + st->Sc(5),
                               std::max(st->Sc(140), cw - st->Sc(196)), st->Sc(18), TRUE);
                }
                gridTop = r4 + st->Sc(34);
            } else {
                st->rcInHorFilter = {};
            }

            int footerH = st->Sc(38);
            MoveWindow(st->hHorCanvas, cx, gridTop, cw, std::max(st->Sc(140), ch - (gridTop - cy) - footerH), TRUE);

            int fy = cy + ch - st->Sc(30);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_PREV), cx, fy, st->Sc(78), st->Sc(28), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_NEXT), cx + st->Sc(84), fy, st->Sc(82), st->Sc(28), TRUE);
            MoveWindow(st->hHorStatus, cx + st->Sc(174), fy + st->Sc(5), std::max(st->Sc(100), cw - st->Sc(468)), st->Sc(20), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_INSERT), cx + cw - st->Sc(286), fy, st->Sc(144), st->Sc(28), TRUE);
            MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_HOR_EXPORT), cx + cw - st->Sc(134), fy, st->Sc(134), st->Sc(28), TRUE);
        }
    }

    // Section 5: Biblioteca
    {
        st->rcInFts = { cx, cy, cx + cw - st->Sc(118), cy + st->Sc(30) };
        MoveWindow(st->hEditFts, st->rcInFts.left + st->Sc(8), cy + st->Sc(6), std::max(80, cw - st->Sc(134)), st->Sc(18), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_FTS_SEARCH), cx + cw - st->Sc(108), cy, st->Sc(108), st->Sc(30), TRUE);
        if (st->hFtsCanvas) {
            MoveWindow(st->hFtsCanvas, cx, cy + st->Sc(40), cw, std::max(st->Sc(120), ch - st->Sc(80)), TRUE);
        }
        int fy = cy + ch - st->Sc(30);
        MoveWindow(st->hLblCache, cx, fy + st->Sc(6), st->Sc(285), st->Sc(20), TRUE);
        HWND hBtnOpen = GetDlgItem(st->hwnd, IDC_BTN_FTS_OPEN);
        if (hBtnOpen) MoveWindow(hBtnOpen, cx + cw - st->Sc(402), fy, st->Sc(110), st->Sc(30), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_FTS_INSERT), cx + cw - st->Sc(284), fy, st->Sc(144), st->Sc(30), TRUE);
        MoveWindow(GetDlgItem(st->hwnd, IDC_BTN_CACHE_PRUNE), cx + cw - st->Sc(132), fy, st->Sc(132), st->Sc(30), TRUE);
    }

    // Section 6: Trámites y Servicios
    {
        if (st->hTramitesCanvas) {
            MoveWindow(st->hTramitesCanvas, cx, cy, cw, std::max(st->Sc(140), ch - st->Sc(40)), TRUE);
        }
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
    SetWindowTextW(st->hProgressBanner, L"Sincronizando con TEC Digital...");
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
        opts.syncCatedrasAndCommunities = true;
        opts.syncPersonalDocuments = true;
        opts.syncCustomSpacePages = true;
        opts.syncAllSpaceFolders = true;
        opts.syncWeeklySubpages = true;
        opts.syncMotorJuegos = true;
        opts.politeDelayMs = 280;
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
            st->pendingWorkStatus = ok ? "Sincronización completada." : ("Sincronización incompleta: " + err);
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
                    g.DrawString(authed ? L"Conectado" : L"Sin conexión", -1, &fSt,
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

                // Compact connected status bar text inside panelAccountBand on Inicio
                {
                    const bool authedOrCached = st->store &&
                        (st->store->IsAuthenticated() || st->snap.sessionAuthenticated || !st->snap.communities.empty());
                    if (st->currentSection == HubSection::Resumen && authedOrCached && !st->showLoginInline &&
                        st->panelAccountBand.right > st->panelAccountBand.left) {
                        const float ax = static_cast<float>(st->panelAccountBand.left);
                        const float ay = static_cast<float>(st->panelAccountBand.top);
                        const float aw = static_cast<float>(st->panelAccountBand.right - st->panelAccountBand.left);
                        const float ah = static_cast<float>(st->panelAccountBand.bottom - st->panelAccountBand.top);
                        const bool authed = st->store && (st->store->IsAuthenticated() || st->snap.sessionAuthenticated);

                        Gdiplus::SolidBrush dotBr(UiGlass::ToGdip(authed ? pal.accentGreen : pal.accentOrange));
                        const float dotSz = 8.0f * dpiScale;
                        g.FillEllipse(&dotBr, ax + 14.0f * dpiScale, ay + (ah - dotSz) * 0.5f, dotSz, dotSz);

                        Gdiplus::FontFamily ff(L"Segoe UI");
                        Gdiplus::Font fAcc(&ff, 12.6f * dpiScale, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
                        Gdiplus::Font fMeta(&ff, 11.4f * dpiScale, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                        Gdiplus::SolidBrush brPrim(UiGlass::ToGdip(pal.labelPrimary));
                        Gdiplus::SolidBrush brSec(UiGlass::ToGdip(pal.labelSecondary));
                        Gdiplus::StringFormat sf;
                        sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                        sf.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                        sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

                        std::wstring title = (st->snap.studentGeneralInfo && !st->snap.studentGeneralInfo->career.empty())
                                                 ? Utf8ToWide(st->snap.studentGeneralInfo->career)
                                                 : (st->snap.curriculumGraph && !st->snap.curriculumGraph->dsc_curriculum.empty()
                                                        ? Utf8ToWide(st->snap.curriculumGraph->dsc_curriculum)
                                                        : (authed ? L"Cuenta TEC Digital conectada" : L"Datos guardados sin conexión"));
                        std::wstring meta = authed ? L"Sesión activa" : L"Modo sin conexión";
                        if (!st->snap.syncedAtUtc.empty()) {
                            meta += L"  ·  Sincronizado";
                        }
                        const float rightRes = authed ? 136.0f * dpiScale : 16.0f * dpiScale;
                        const float textAvail = std::max(80.0f, aw - 32.0f * dpiScale - rightRes);
                        g.DrawString(title.c_str(), -1, &fAcc,
                                     Gdiplus::RectF(ax + 30.0f * dpiScale, ay, textAvail * 0.64f, ah), &sf, &brPrim);
                        Gdiplus::StringFormat sfR;
                        sfR.SetAlignment(Gdiplus::StringAlignmentFar);
                        sfR.SetLineAlignment(Gdiplus::StringAlignmentCenter);
                        sfR.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
                        sfR.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
                        g.DrawString(meta.c_str(), -1, &fMeta,
                                     Gdiplus::RectF(ax + 30.0f * dpiScale + textAvail * 0.64f, ay, textAvail * 0.36f - 8.0f * dpiScale, ah),
                                     &sfR, &brSec);
                    }
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
            if ((hCtrl == st->hEditWhatIf || hCtrl == st->hEditCourseDocSearch ||
                 hCtrl == st->hEditHorFilter || hCtrl == st->hEditHorCarnet ||
                 hCtrl == st->hEditHorGlobalQ || hCtrl == st->hEditFts) && st->brInput) {
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
            L"Inicio",
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
        st->hSessionBadge = mk(L"STATIC", L"Sin conexión", SS_LEFT, 0, st->fSmall);
        ShowWindow(st->hSessionBadge, SW_HIDE);
        st->hBtnSync = mkBtn(L"Sincronizar", IDC_BTN_SYNC_NOW, st->fBold);
        st->hBtnOpenWeb = mkBtn(L"Abrir TEC Digital", IDC_BTN_OPEN_WEB);

        st->hTitle = mk(L"STATIC", L"Inicio", SS_LEFT, 0, st->fTitle);
        st->hSubtitle = mk(L"STATIC", L"", SS_LEFT, 0, st->fSmall);
        st->hProgressBanner = mk(L"STATIC", L"Listo", SS_LEFT, 0, st->fSmall);

        auto regCanvas = [&](const wchar_t* cls, WNDPROC proc, LPWSTR cursor = IDC_ARROW) {
            WNDCLASSEXW wc{};
            if (!GetClassInfoExW(GetModuleHandleW(nullptr), cls, &wc)) {
                wc.cbSize = sizeof(wc);
                wc.lpfnWndProc = proc;
                wc.hInstance = GetModuleHandleW(nullptr);
                wc.lpszClassName = cls;
                wc.hCursor = LoadCursorW(nullptr, cursor);
                RegisterClassExW(&wc);
            }
        };
        auto mkCanvas = [&](const wchar_t* cls, int id, DWORD style = WS_CHILD) -> HWND {
            HWND c = CreateWindowExW(0, cls, L"", style, 0, 0, 10, 10, hwnd,
                                     reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                     GetModuleHandleW(nullptr), nullptr);
            SetWindowLongPtrW(c, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
            return c;
        };

        // Section 0: Inicio & Cuenta
        st->hEmail = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_EMAIL);
        SendMessageW(st->hEmail, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"usuario@estudiantec.cr"));
        st->hPass = mk(L"EDIT", L"", WS_TABSTOP | ES_PASSWORD | ES_AUTOHSCROLL, IDC_EDIT_PASS);
        SendMessageW(st->hPass, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Contraseña"));
        st->hBtnLogin = mkBtn(L"Iniciar sesión", IDC_BTN_DO_LOGIN, st->fBold);
        st->hBtnQr = mkBtn(L"QR", IDC_BTN_DO_QR);
        st->hBtnLogout = mkBtn(L"Cerrar sesión", IDC_BTN_LOGOUT_LOCAL);
        st->hChkDpapi = mk(L"BUTTON", L"Recordar sesión cifrada localmente", WS_TABSTOP | BS_AUTOCHECKBOX, IDC_CHK_REMEMBER_DPAPI);
        SendMessageW(st->hChkDpapi, BM_SETCHECK, BST_CHECKED, 0);
        st->hSummaryEdit = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_SUMMARY_REPORT);
        ShowWindow(st->hSummaryEdit, SW_HIDE);
        regCanvas(L"CuadernUTecSummaryCanvas", SummaryCanvasProc);
        st->hSummaryCanvas = mkCanvas(L"CuadernUTecSummaryCanvas", IDC_SUMMARY_CANVAS, WS_CHILD | WS_VISIBLE);
        st->secCtrls[0] = { st->hEmail, st->hPass, st->hBtnLogin, st->hBtnQr, st->hBtnLogout, st->hChkDpapi, st->hSummaryCanvas };

        // Section 1: Cursos
        st->hCmbPeriod = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_PERIOD);
        st->hLstCourses = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IDC_LST_COURSES);
        ShowWindow(st->hLstCourses, SW_HIDE);
        st->hCmbCourseSub = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_COURSE_SUBTAB);
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Resumen"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Documentos"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Evaluaciones"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Actividades"));
        SendMessageW(st->hCmbCourseSub, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Personas"));
        SendMessageW(st->hCmbCourseSub, CB_SETCURSEL, 0, 0);
        ShowWindow(st->hCmbCourseSub, SW_HIDE);
        st->hEditCourseDocSearch = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_COURSE_DOC_SEARCH);
        SendMessageW(st->hEditCourseDocSearch, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Buscar carpetas o archivos en este curso o cátedra..."));
        st->hCmbCourseDocSort = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_COURSE_DOC_SORT);
        SendMessageW(st->hCmbCourseDocSort, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Orden: Recientes"));
        SendMessageW(st->hCmbCourseDocSort, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Orden: Creación"));
        SendMessageW(st->hCmbCourseDocSort, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Orden: Nombre A-Z"));
        SendMessageW(st->hCmbCourseDocSort, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Orden: Tamaño"));
        SendMessageW(st->hCmbCourseDocSort, CB_SETCURSEL, 0, 0);
        st->hEditWhatIf = mk(L"EDIT", L"70", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_WHATIF_TARGET);
        st->hBtnWhatIf = mkBtn(L"Simular meta", IDC_BTN_WHATIF_CALC);
        st->hBtnCourseRubric = mkBtn(L"Rúbrica", IDC_BTN_COURSE_RUBRIC);
        st->hBtnCourseUploadEval = mkBtn(L"Subir entrega...", IDC_BTN_COURSE_UPLOAD_EVAL, st->fBold);
        st->hCourseDetail = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_COURSE_DETAIL);
        ShowWindow(st->hCourseDetail, SW_HIDE);
        st->hLstCourseFiles = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IDC_LST_COURSE_FILES);
        ShowWindow(st->hLstCourseFiles, SW_HIDE);
        regCanvas(L"CuadernUTecCourseListCanvas", CourseListCanvasProc, IDC_HAND);
        regCanvas(L"CuadernUTecCourseDetailCanvas", CourseDetailCanvasProc);
        st->hCourseListCanvas = mkCanvas(L"CuadernUTecCourseListCanvas", IDC_COURSE_LIST_CANVAS);
        st->hCourseDetailCanvas = mkCanvas(L"CuadernUTecCourseDetailCanvas", IDC_COURSE_DETAIL_CANVAS);
        HWND bCIns = mkBtn(L"Insertar en lienzo", IDC_BTN_COURSE_INSERT_CANVAS, st->fBold);
        HWND bCSync = mkBtn(L"Sincronizar archivos", IDC_BTN_COURSE_SYNC_FILES);
        st->hBtnCourseOpenWeb = mkBtn(L"Abrir web", IDC_BTN_COURSE_OPEN_WEB);
        ShowWindow(st->hBtnCourseOpenWeb, SW_HIDE);
        HWND bCMore = mkBtn(L"Más acciones...", IDC_BTN_COURSE_MORE);
        st->secCtrls[1] = { st->hCmbPeriod, st->hCourseListCanvas, st->hCourseDetailCanvas,
                            st->hEditCourseDocSearch, st->hCmbCourseDocSort,
                            st->hEditWhatIf, st->hBtnWhatIf, st->hBtnCourseRubric, st->hBtnCourseUploadEval,
                            bCIns, bCSync, bCMore };
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
        SendMessageW(st->hCmbCalView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Agenda"));
        SendMessageW(st->hCmbCalView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Vista Semanal"));
        SendMessageW(st->hCmbCalView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Vista Mensual"));
        SendMessageW(st->hCmbCalView, CB_SETCURSEL, 1, 0);
        HWND bIcs = mkBtn(L"Exportar .ics...", IDC_BTN_CAL_EXPORT_ICS, st->fBold);
        st->hCalList = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_CAL_LIST);
        ShowWindow(st->hCalList, SW_HIDE);
        regCanvas(L"CuadernUTecCalCanvas", CalendarCanvasProc);
        regCanvas(L"CuadernUTecAgendaCanvas", AgendaCanvasProc);
        st->hCalCanvas = mkCanvas(L"CuadernUTecCalCanvas", IDC_CAL_CANVAS);
        st->hAgendaCanvas = mkCanvas(L"CuadernUTecAgendaCanvas", IDC_AGENDA_CANVAS);
        st->secCtrls[2] = { st->hCmbCalFilter, st->hCmbCalView, bIcs, st->hCalCanvas, st->hAgendaCanvas };

        // Section 3: Expediente y Malla Curricular
        st->hCmbExpView = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_EXP_VIEW);
        SendMessageW(st->hCmbExpView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Resumen y Notas"));
        SendMessageW(st->hCmbExpView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Malla Curricular"));
        SendMessageW(st->hCmbExpView, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Atributos de Graduación"));
        SendMessageW(st->hCmbExpView, CB_SETCURSEL, 0, 0);
        st->hCmbGradesFilter = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_GRADES_FILTER);
        ShowWindow(st->hCmbGradesFilter, SW_HIDE);
        st->hExpReport = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_EXP_REPORT);
        ShowWindow(st->hExpReport, SW_HIDE);
        regCanvas(L"CuadernUTecExpStatsCanvas", ExpStatsCanvasProc);
        st->hExpStatsCanvas = mkCanvas(L"CuadernUTecExpStatsCanvas", IDC_EXP_STATS_CANVAS);
        st->hBtnMallaPicker = mkBtn(L"Cambiar plan", IDC_BTN_MALLA_TOGGLE_PICKER);
        st->hBtnMallaZIn = mkBtn(L"Zoom +", IDC_BTN_MALLA_ZOOM_IN);
        st->hBtnMallaZOut = mkBtn(L"Zoom -", IDC_BTN_MALLA_ZOOM_OUT);
        st->hBtnMallaInsert = mkBtn(L"Insertar avance en lienzo", IDC_BTN_MALLA_INSERT, st->fBold);
        st->hCmbMallaSede = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_MALLA_SEDE);
        st->hCmbMallaCarrera = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_MALLA_CARRERA);
        st->hCmbMallaPlan = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_MALLA_PLAN);
        st->hBtnMallaLoad = mkBtn(L"Cargar plan", IDC_BTN_MALLA_LOAD_PLAN);

        regCanvas(L"CuadernUTecMallaCanvas", MallaCanvasProc, IDC_HAND);
        st->hMallaCanvas = mkCanvas(L"CuadernUTecMallaCanvas", IDC_MALLA_CANVAS);
        regCanvas(L"CuadernUTecAttrCanvas", AttrCanvasProc);
        st->hAttrCanvas = mkCanvas(L"CuadernUTecAttrCanvas", IDC_ATTR_CANVAS);
        st->hMallaInspector = mk(L"EDIT", L"Haz clic en una materia para ver requisitos, correquisitos y cursos que desbloquea. Arrastra para mover o usa la rueda para zoom.",
                                 WS_VSCROLL | ES_MULTILINE | ES_READONLY, IDC_MALLA_INSPECTOR);
        padEdit(st->hMallaInspector);
        st->secCtrls[3] = { st->hCmbExpView, st->hExpStatsCanvas, st->hBtnMallaPicker,
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
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Bimestre"));
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Cuatrimestre"));
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Trimestre"));
        SendMessageW(st->hCmbHorPubPer, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Humanísticas"));
        SendMessageW(st->hCmbHorPubPer, CB_SETCURSEL, 0, 0);

        st->hEditHorGlobalQ = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_HOR_GLOBAL_Q);
        SendMessageW(st->hEditHorGlobalQ, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Buscar curso, código, profesor, escuela, carrera o aula..."));

        st->hCmbHorGlobalSchool = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_GLOBAL_SCHOOL);
        SendMessageW(st->hCmbHorGlobalSchool, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todas las escuelas y carreras"));
        SendMessageW(st->hCmbHorGlobalSchool, CB_SETCURSEL, 0, 0);

        st->hCmbHorGlobalMod = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_GLOBAL_MOD);
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todas modalidades"));
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Regular"));
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Virtual"));
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Semipresencial"));
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Asistida"));
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Tutoría"));
        SendMessageW(st->hCmbHorGlobalMod, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Grupo RN"));
        SendMessageW(st->hCmbHorGlobalMod, CB_SETCURSEL, 0, 0);

        st->hCmbHorGlobalDay = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_HOR_GLOBAL_DAY);
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Todos los días"));
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Lunes"));
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Martes"));
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Miércoles"));
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Jueves"));
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Viernes"));
        SendMessageW(st->hCmbHorGlobalDay, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Sábado"));
        SendMessageW(st->hCmbHorGlobalDay, CB_SETCURSEL, 0, 0);

        st->hBtnHorIndexAll = mkBtn(L"Indexar todo el TEC", IDC_BTN_HOR_INDEX_ALL);
        regCanvas(L"CuadernUTecHorSearchCanvas", HorSearchCanvasProc, IDC_HAND);
        st->hHorSearchCanvas = mkCanvas(L"CuadernUTecHorSearchCanvas", IDC_HOR_SEARCH_CANVAS);

        st->hEditHorCarnet = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | ES_NUMBER, IDC_EDIT_HOR_CARNET);
        SendMessageW(st->hEditHorCarnet, EM_SETCUEBANNER, TRUE,
                     reinterpret_cast<LPARAM>(L"Número de carné..."));

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
                     reinterpret_cast<LPARAM>(L"Cursos (ej. EL2113, MA2104), modalidad o profesor..."));
        st->hBtnHorFilters = mkBtn(L"Filtros", IDC_BTN_HOR_TOGGLE_FILTERS);
        HWND bHLoad = mkBtn(L"Opciones...", IDC_BTN_HOR_LOAD_JSON);
        HWND bHSolve = mkBtn(L"Armar horario", IDC_BTN_HOR_SOLVE, st->fBold);
        regCanvas(L"CuadernUTecHorariosCanvas", HorariosCanvasProc, IDC_HAND);
        st->hHorCanvas = mkCanvas(L"CuadernUTecHorariosCanvas", IDC_HOR_CANVAS);
        HWND bHPrev = mkBtn(L"Anterior", IDC_BTN_HOR_PREV);
        HWND bHNext = mkBtn(L"Siguiente", IDC_BTN_HOR_NEXT);
        st->hHorStatus = mk(L"STATIC", L"Busca cursos de todo el TEC o presiona Indexar todo el TEC", SS_LEFT, IDC_HOR_STATUS, st->fSmall);
        HWND bHIns = mkBtn(L"Insertar en lienzo", IDC_BTN_HOR_INSERT);
        HWND bHExp = mkBtn(L"Exportar...", IDC_BTN_HOR_EXPORT);
        st->secCtrls[4] = { st->hCmbHorSource, st->hCmbHorEscuela, st->hCmbHorYear,
                            st->hCmbHorPubSede, st->hCmbHorPubPer, st->hEditHorCarnet,
                            st->hEditHorGlobalQ, st->hCmbHorGlobalSchool, st->hCmbHorGlobalMod,
                            st->hCmbHorGlobalDay, st->hBtnHorIndexAll, st->hHorSearchCanvas,
                            st->hCmbHorSede, st->hCmbHorCarrera, st->hCmbHorPeriodo,
                            st->hChkHorPrereq, st->hChkHorNoNight, st->hChkHorSeats, st->hChkHorBusy,
                            st->hCmbHorFreeDay, st->hEditHorFilter, st->hBtnHorFilters,
                            bHLoad, bHSolve, st->hHorCanvas, bHPrev, bHNext, st->hHorStatus, bHIns, bHExp };

        // Section 5: Biblioteca
        st->hEditFts = mk(L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL, IDC_EDIT_FTS_QUERY);
        SendMessageW(st->hEditFts, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Buscar en todos los archivos, carpetas, apuntes y cuaderno..."));
        HWND bFtsGo = mkBtn(L"Buscar", IDC_BTN_FTS_SEARCH, st->fBold);
        st->hLstFts = mk(L"LISTBOX", L"", WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, IDC_LST_FTS_HITS);
        ShowWindow(st->hLstFts, SW_HIDE);
        st->hFtsPreview = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_FTS_PREVIEW);
        ShowWindow(st->hFtsPreview, SW_HIDE);
        regCanvas(L"CuadernUTecFtsCanvas", FtsCanvasProc, IDC_HAND);
        st->hFtsCanvas = mkCanvas(L"CuadernUTecFtsCanvas", IDC_FTS_CANVAS);
        st->hLblCache = mk(L"STATIC", L"Caché local: 0.0 MB", SS_LEFT, IDC_LBL_CACHE_USAGE, st->fSmall);
        HWND bFtsOpen = mkBtn(L"Abrir archivo", IDC_BTN_FTS_OPEN);
        HWND bFtsIns = mkBtn(L"Insertar en lienzo", IDC_BTN_FTS_INSERT);
        HWND bPrune = mkBtn(L"Limpiar caché", IDC_BTN_CACHE_PRUNE);
        st->secCtrls[5] = { st->hEditFts, bFtsGo, st->hFtsCanvas, st->hLblCache, bFtsOpen, bFtsIns, bPrune };

        // Section 6: Trámites y Servicios
        st->hCmbTramite = mk(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, IDC_CMB_TRAMITE_CAT);
        ShowWindow(st->hCmbTramite, SW_HIDE);
        st->hTramiteReport = mk(L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, IDC_TRAMITE_REPORT);
        ShowWindow(st->hTramiteReport, SW_HIDE);
        regCanvas(L"CuadernUTecTramitesCanvas", TramitesCanvasProc);
        st->hTramitesCanvas = mkCanvas(L"CuadernUTecTramitesCanvas", IDC_TRAMITES_CANVAS);
        st->hBtnSvcMapa = mkBtn(L"Mapa del Campus", IDC_BTN_SVC_MAPA);
        ShowWindow(st->hBtnSvcMapa, SW_HIDE);
        st->hBtnSvcGuia = mkBtn(L"Guía Estudiantes", IDC_BTN_SVC_GUIA);
        ShowWindow(st->hBtnSvcGuia, SW_HIDE);
        st->hBtnSvcSoporte = mkBtn(L"Soporte TEC", IDC_BTN_SVC_SOPORTE);
        ShowWindow(st->hBtnSvcSoporte, SW_HIDE);
        HWND bS4 = mkBtn(L"Control de Plagio", IDC_BTN_SVC_TURNITIN);
        HWND bSMore = mkBtn(L"Servicios TEC...", IDC_BTN_SVC_MORE);
        st->secCtrls[6] = { st->hTramitesCanvas, bS4, bSMore };

        st->theme.ThemeAllChildren(hwnd);
        if (st->currentSection == HubSection::Cursos && st->initialSubView >= 0 && st->initialSubView <= 4) {
            st->courseSubTab = st->initialSubView;
            SendMessageW(st->hCmbCourseSub, CB_SETCURSEL, static_cast<WPARAM>(st->initialSubView), 0);
        } else if (st->currentSection == HubSection::Calendario && st->initialSubView >= 0 && st->initialSubView <= 2) {
            SendMessageW(st->hCmbCalView, CB_SETCURSEL, static_cast<WPARAM>(st->initialSubView), 0);
        } else if (st->currentSection == HubSection::ExpedienteYMalla && st->initialSubView >= 0 && st->initialSubView <= 2) {
            SendMessageW(st->hCmbExpView, CB_SETCURSEL, static_cast<WPARAM>(st->initialSubView), 0);
            if (st->initialSubView == 0 && st->initialInnerTab >= 0 && st->initialInnerTab <= 2) {
                st->expSubTab = st->initialInnerTab;
            }
        } else if (st->currentSection == HubSection::HorariosVivo && (st->initialSubView == 0 || st->initialSubView == 1)) {
            st->horViewMode = st->initialSubView;
        } else if (st->currentSection == HubSection::BibliotecaOffline && st->initialSubView >= 0 && st->initialSubView <= 2) {
            st->ftsModeTab = st->initialSubView;
        } else if (st->currentSection == HubSection::TramitesYPerfil && st->initialSubView >= 0 && st->initialSubView <= 4) {
            st->tramitesSubTab = st->initialSubView;
        }
        RefreshAllSections(st);
        if (st->autoScreenshotPathW != nullptr && st->currentSection == HubSection::Cursos) {
            if (st->initialInnerTab == 1 || st->initialInnerTab == 2) {
                // Focus a course with rich custom pages (e.g. FI2103 GR 51 -> Cronograma weekly modules + linked Cátedra)
                int bestIdx = -1;
                int cronoPage = -1;
                for (size_t ci = 0; ci < st->filteredCourses.size(); ++ci) {
                    const auto& c = st->filteredCourses[ci];
                    if (c.type != "CLASE" || !c.is_current_period) continue;
                    auto cpit = st->snap.customPagesBySpace.find(c.url);
                    if (cpit != st->snap.customPagesBySpace.end() && !cpit->second.empty()) {
                        bestIdx = static_cast<int>(ci);
                        for (const auto& pg : cpit->second) {
                            if (SafeLowerAscii(pg.tab_label).find("cronograma") != std::string::npos) {
                                cronoPage = pg.page_num;
                                break;
                            }
                        }
                        if (cronoPage >= 0) break;
                    }
                }
                if (bestIdx >= 0) {
                    st->selectedCourseIdx = bestIdx;
                    if (st->initialInnerTab == 2 && cronoPage >= 0) {
                        st->selectedCustomPageNum = cronoPage;
                    }
                }
            } else if (st->initialInnerTab == 3 || st->initialInnerTab == 4 || st->initialInnerTab == 5) {
                // 3 = Cátedra Contenidos, 4 = Cátedra Portada Editorial, 5 = Cátedra Weekly Subpage (e.g. S1)
                for (size_t ci = 0; ci < st->filteredCourses.size(); ++ci) {
                    const auto& c = st->filteredCourses[ci];
                    if (c.type != "CATEDRA") continue;
                    auto cpit = st->snap.customPagesBySpace.find(c.url);
                    if (cpit != st->snap.customPagesBySpace.end() && !cpit->second.empty()) {
                        st->selectedCourseIdx = static_cast<int>(ci);
                        if (st->initialInnerTab == 4) {
                            st->selectedCustomPageNum = -1;
                        } else if (st->initialInnerTab == 5) {
                            st->selectedCustomPageNum = cpit->second.front().page_num;
                            for (const auto& pg : cpit->second) {
                                const std::string tl = SafeLowerAscii(pg.tab_label);
                                if (tl.rfind("s1", 0) == 0 || tl.find("semana 1") != std::string::npos ||
                                    tl.rfind("s2", 0) == 0 || tl.find("semana 2") != std::string::npos) {
                                    st->selectedCustomPageNum = pg.page_num;
                                    break;
                                }
                            }
                            if (st->selectedCustomPageNum == cpit->second.front().page_num && cpit->second.size() > 2) {
                                st->selectedCustomPageNum = cpit->second[2].page_num;
                            }
                        } else {
                            st->selectedCustomPageNum = cpit->second.front().page_num;
                        }
                        if (SafeLowerAscii(c.name).find("iii") != std::string::npos) break;
                    }
                }
            } else if (st->courseSubTab == 2) {
                // For automated visual audit of Evaluaciones, focus the richest graded rubric (with criteria & comments)
                int bestScore = -1;
                int bestCourse = -1;
                int bestFlat = -1;
                for (size_t ci = 0; ci < st->filteredCourses.size(); ++ci) {
                    auto evIt = st->snap.evaluationsByPath.find(st->filteredCourses[ci].url);
                    if (evIt == st->snap.evaluationsByPath.end()) continue;
                    int flat = 0;
                    for (const auto& cat : evIt->second.categories) {
                        for (const auto& it : cat.items) {
                            if (it.has_rubric && !it.rubric_id.empty()) {
                                const std::string rk = it.rubric_id + "|" + it.task_item_id + "|" + it.task_id;
                                auto rbit = st->snap.rubricsByKey.find(rk);
                                if (rbit != st->snap.rubricsByKey.end() && !rbit->second.criteria.empty()) {
                                    int cmts = 0;
                                    for (const auto& cr : rbit->second.criteria) {
                                        if (!cr.comment.empty()) ++cmts;
                                    }
                                    int sc = (rbit->second.is_graded ? 100 : 0) + cmts * 10 +
                                             static_cast<int>(rbit->second.criteria.size());
                                    if (sc > bestScore) {
                                        bestScore = sc;
                                        bestCourse = static_cast<int>(ci);
                                        bestFlat = flat;
                                    }
                                }
                            }
                            ++flat;
                        }
                    }
                }
                if (bestCourse >= 0 && bestFlat >= 0) {
                    st->selectedCourseIdx = bestCourse;
                    st->selectedEvalFlatIdx = bestFlat;
                    st->autoScrollSelectedEvalOnce = true;
                }
            }
        }
        LayoutDialog(st);
        SwitchSection(st, st->currentSection);
        if (st->autoScreenshotPathW == nullptr && st->store && st->store->IsAuthenticated() &&
            st->snap.monitorIntervalMinutes > 0) {
            StartAutoMonitorBridge(st, st->snap.monitorIntervalMinutes);
        }
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
                SetWindowTextW(st->hProgressBanner, L"Consulta de horarios completada.");
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

    case WM_APP_TEC_GLOBAL_HORARIOS_DONE:
        if (st) {
            std::string statusMsg;
            bool ok = false;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                ok = st->pendingGlobalIndexOk;
                statusMsg = std::move(st->pendingGlobalIndexStatus);
            }
            if (st->store) {
                st->globalHorariosIdx = st->store->GetGlobalScheduleIndex();
            }
            PopulateGlobalHorariosSchoolCombo(st);
            RefreshGlobalHorariosSearch(st);
            if (!statusMsg.empty()) {
                SetWindowTextW(st->hProgressBanner, Utf8ToWide(statusMsg).c_str());
                if (!ok && st->hHorStatus) {
                    SetWindowTextW(st->hHorStatus, Utf8ToWide(statusMsg).c_str());
                }
            }
        }
        return 0;

    case WM_APP_TEC_MONITOR_CHANGES:
        if (st) {
            std::vector<td::TecChangeEvent> changes;
            {
                std::lock_guard<std::mutex> lk(st->workResultMutex);
                changes = std::move(st->pendingMonitorChanges);
                st->pendingMonitorChanges.clear();
            }
            RefreshAllSections(st);
            if (!changes.empty()) {
                const std::wstring title = L"TEC Digital · " + std::to_wstring(changes.size()) +
                                           (changes.size() == 1 ? L" novedad detectada" : L" novedades detectadas");
                std::string body = changes.front().title;
                if (!changes.front().space_name.empty()) body += " (" + changes.front().space_name + ")";
                if (changes.size() > 1) body += " y " + std::to_string(changes.size() - 1) + " más.";
                SetWindowTextW(st->hProgressBanner, Utf8ToWide("Radar TEC: " + body).c_str());
                ShowWindowsTecToast(st, title, Utf8ToWide(body));
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
        if (id == IDC_EDIT_COURSE_DOC_SEARCH && code == EN_CHANGE) {
            st->courseDetailScrollY = 0;
            if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_CMB_COURSE_DOC_SORT && code == CBN_SELCHANGE) {
            st->courseDetailScrollY = 0;
            if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_EDIT_FTS_QUERY && code == EN_CHANGE) {
            if (st->ftsModeTab == 0) RefreshGlobalLibraryFilter(st);
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
                    st->pendingWorkStatus = res.ok ? "Sesión iniciada correctamente."
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
                    st->pendingWorkStatus = res.ok ? "Sesión QR activa."
                                                   : ("QR: " + res.message);
                }
                st->workerBusy.store(false);
                PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, res.ok ? 1 : 0, 0);
            });
            return 0;
        }
        if (id == IDC_BTN_LOGOUT_LOCAL && code == BN_CLICKED) {
            st->store->LogoutLocalOnly(true);
            st->showLoginInline = true;
            SetWindowTextW(st->hProgressBanner, L"Sesión local cerrada.");
            RefreshAllSections(st);
            SwitchSection(st, st->currentSection);
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
            if (id == IDC_LST_COURSES && st->hLstCourses) {
                st->selectedCourseIdx = static_cast<int>(SendMessageW(st->hLstCourses, LB_GETCURSEL, 0, 0));
            }
            if (id == IDC_CMB_COURSE_SUBTAB && st->hCmbCourseSub) {
                st->courseSubTab = static_cast<int>(SendMessageW(st->hCmbCourseSub, CB_GETCURSEL, 0, 0));
            }
            RefreshSelectedCourseDetail(st);
            return 0;
        }
        if (id == IDC_BTN_COURSE_INSERT_CANVAS && code == BN_CLICKED) {
            int idx = st->selectedCourseIdx;
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
                int subTab = st->courseSubTab;
                if (subTab == 1) {
                    int fIdx = st->selectedCourseFileIdx;
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
            int idx = st->selectedCourseIdx;
            if (idx < 0 || static_cast<size_t>(idx) >= st->filteredCourses.size()) return 0;
            const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
            HMENU hm = CreatePopupMenu();
            AppendMenuW(hm, MF_STRING, 15, L"Actualizar este curso ahora (evaluaciones y portada)");
            AppendMenuW(hm, MF_STRING, 14, L"Insertar resumen del curso en lienzo");
            AppendMenuW(hm, MF_STRING, 1, L"Crear cuaderno del curso (.cnu)...");
            AppendMenuW(hm, MF_STRING, 2, L"Descargar archivo seleccionado");
            AppendMenuW(hm, MF_STRING, 3, L"Generar Flashcards SM-2 para el mazo");
            AppendMenuW(hm, MF_STRING, 4, L"Abrir adjunto / solución de evaluación");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 5, L"Historial de versiones del archivo...");
            AppendMenuW(hm, MF_STRING, 6, L"Historial de entregas y revisión vigente...");
            AppendMenuW(hm, MF_STRING, 7, L"Leer hilo de foro del curso...");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hm, MF_STRING, 10, L"Subir entrega a evaluación seleccionada...");
            AppendMenuW(hm, MF_STRING, 11, L"Eliminar última entrega de evaluación...");
            AppendMenuW(hm, MF_STRING, 12, L"Sincronizar Mis documentos");
            AppendMenuW(hm, MF_STRING, 13, L"Actualizar cursos matriculados desde DAR...");
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
            if (pick == 15) {
                TriggerOnDemandCourseSync(st, c.url, c.name);
            } else if (pick == 14) {
                SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_COURSE_INSERT_CANVAS, BN_CLICKED), 0);
            } else if (pick == 1) {
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
            } else if (pick == 2) {
                int fIdx = st->selectedCourseFileIdx;
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
                int fIdx = st->selectedCourseFileIdx;
                if (fIdx >= 0 && static_cast<size_t>(fIdx) < st->currentCourseFiles.size()) {
                    oid = st->currentCourseFiles[static_cast<size_t>(fIdx)].object_id;
                    fname = st->currentCourseFiles[static_cast<size_t>(fIdx)].name;
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
                        MessageBoxW(hwnd, Utf8ToWide(foss.str()).c_str(), L"Foros del curso", MB_OK);
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
            } else if (pick == 10) {
                SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDC_BTN_COURSE_UPLOAD_EVAL, BN_CLICKED), 0);
            } else if (pick == 11) {
                const td::DetailedEvaluationItem* chosen = nullptr;
                if (evIt != st->snap.evaluationsByPath.end()) {
                    int flat = 0;
                    for (const auto& cat : evIt->second.categories) {
                        for (const auto& it : cat.items) {
                            if (flat == st->selectedEvalFlatIdx && !it.latest_revision_id.empty()) {
                                chosen = &it;
                                break;
                            }
                            ++flat;
                        }
                        if (chosen) break;
                    }
                    if (!chosen) {
                        for (const auto& cat : evIt->second.categories) {
                            for (const auto& it : cat.items) {
                                if (!it.latest_revision_id.empty()) { chosen = &it; break; }
                            }
                            if (chosen) break;
                        }
                    }
                }
                if (!chosen) {
                    MessageBoxW(hwnd, L"No se encontró una entrega registrada con revisión eliminable en este curso.",
                                L"Eliminar entrega", MB_OK | MB_ICONINFORMATION);
                } else {
                    std::wstring prompt = L"¿Desea eliminar la última entrega registrada en TEC Digital?\n\n"
                                          L"Curso: " + Utf8ToWide(c.name) + L"\n"
                                          L"Evaluación: " + Utf8ToWide(chosen->name) + L"\n"
                                          L"Revisión: " + Utf8ToWide(chosen->latest_revision_id) + L"\n\n"
                                          L"Esta acción modifica su entrega en el servidor oficial.";
                    if (MessageBoxW(hwnd, prompt.c_str(), L"Confirmar eliminación de entrega",
                                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES &&
                        !st->workerBusy.exchange(true)) {
                        std::string commUrl = c.url;
                        std::string revId = chosen->latest_revision_id;
                        std::string itemId = chosen->latest_item_id;
                        SetWindowTextW(st->hProgressBanner, L"Eliminando revisión de entrega...");
                        if (st->workerThread.joinable()) st->workerThread.join();
                        st->workerThread = std::thread([st, commUrl, revId, itemId]() {
                            auto dr = st->store->DeleteEvaluationSubmission(commUrl, revId, itemId, true);
                            {
                                std::lock_guard<std::mutex> lk(st->workResultMutex);
                                st->pendingWorkKind = 2;
                                st->pendingWorkStatus = dr.ok ? "Entrega eliminada correctamente en TEC Digital."
                                                              : ("No se pudo eliminar la entrega: " + dr.error_message);
                            }
                            st->workerBusy.store(false);
                            PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, dr.ok ? 1 : 0, 0);
                        });
                    }
                }
            } else if (pick == 12) {
                if (!st->workerBusy.exchange(true)) {
                    SetWindowTextW(st->hProgressBanner, L"Sincronizando Mis documentos...");
                    if (st->workerThread.joinable()) st->workerThread.join();
                    st->workerThread = std::thread([st]() {
                        std::string sum, err;
                        bool ok = st->store->SyncPersonalDocuments(false, &sum, &err);
                        {
                            std::lock_guard<std::mutex> lk(st->workResultMutex);
                            st->pendingWorkKind = 2;
                            st->pendingWorkStatus = ok ? ("Mis documentos sincronizados: " + sum)
                                                       : ("Error en Mis documentos: " + err);
                        }
                        st->workerBusy.store(false);
                        PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, ok ? 1 : 0, 0);
                    });
                }
            } else if (pick == 13) {
                if (MessageBoxW(hwnd,
                                L"¿Desea solicitar al portal TEC Digital la actualización de sus cursos matriculados desde Admisión y Registro (DAR)?",
                                L"Actualizar cursos matriculados",
                                MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES &&
                    !st->workerBusy.exchange(true)) {
                    SetWindowTextW(st->hProgressBanner, L"Actualizando matrícula desde DAR...");
                    if (st->workerThread.joinable()) st->workerThread.join();
                    st->workerThread = std::thread([st]() {
                        auto rr = st->store->RefreshEnrolledCoursesFromDar(true);
                        {
                            std::lock_guard<std::mutex> lk(st->workResultMutex);
                            st->pendingWorkKind = 2;
                            st->pendingWorkStatus = rr.ok ? "Lista de cursos actualizada desde DAR. Presione Sincronizar ahora para recargar."
                                                          : ("No se pudo actualizar cursos desde DAR: " + rr.error_message);
                        }
                        st->workerBusy.store(false);
                        PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, rr.ok ? 1 : 0, 0);
                    });
                }
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_UPLOAD_EVAL && code == BN_CLICKED) {
            int idx = st->selectedCourseIdx;
            if (idx < 0 || static_cast<size_t>(idx) >= st->filteredCourses.size()) return 0;
            const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
            auto evIt = st->snap.evaluationsByPath.find(c.url);
            const td::DetailedEvaluationItem* chosen = nullptr;
            if (evIt != st->snap.evaluationsByPath.end()) {
                int flat = 0;
                for (const auto& cat : evIt->second.categories) {
                    for (const auto& it : cat.items) {
                        if (flat == st->selectedEvalFlatIdx && (it.can_upload || !it.task_id.empty())) {
                            chosen = &it;
                            break;
                        }
                        ++flat;
                    }
                    if (chosen) break;
                }
                if (!chosen) {
                    for (const auto& cat : evIt->second.categories) {
                        for (const auto& it : cat.items) {
                            if (it.can_upload) { chosen = &it; break; }
                        }
                        if (chosen) break;
                    }
                }
            }
            if (!chosen) {
                MessageBoxW(hwnd, L"No hay una evaluación habilitada para entrega seleccionada en este curso.",
                            L"Subir entrega", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (!st->store || !st->store->IsAuthenticated()) {
                MessageBoxW(hwnd, L"Inicie sesión en TEC Digital antes de subir una entrega.",
                            L"Subir entrega", MB_OK | MB_ICONWARNING);
                return 0;
            }
            std::wstring inPath;
            if (!PickOpenFileW(hwnd, L"Todos los archivos (*.*)\0*.*\0Documentos PDF (*.pdf)\0*.pdf\0Comprimidos (*.zip)\0*.zip\0", inPath)) {
                return 0;
            }
            std::ifstream ifs(inPath, std::ios::binary | std::ios::ate);
            if (!ifs) {
                MessageBoxW(hwnd, L"No fue posible leer el archivo seleccionado.", L"Subir entrega", MB_OK | MB_ICONERROR);
                return 0;
            }
            const auto fsize = static_cast<std::uint64_t>(ifs.tellg());
            const int maxMb = chosen->max_file_size_mb > 0 ? chosen->max_file_size_mb : 80;
            if (fsize == 0 || fsize > static_cast<std::uint64_t>(maxMb) * 1024ull * 1024ull) {
                std::wstring errW = L"El archivo está vacío o excede el límite permitido de " + std::to_wstring(maxMb) + L" MB.";
                MessageBoxW(hwnd, errW.c_str(), L"Subir entrega", MB_OK | MB_ICONWARNING);
                return 0;
            }
            ifs.seekg(0, std::ios::beg);
            std::string fileBytes(static_cast<size_t>(fsize), '\0');
            ifs.read(fileBytes.data(), static_cast<std::streamsize>(fsize));

            const std::string fullUtf8 = WideToUtf8(inPath);
            const size_t sl = fullUtf8.find_last_of("\\/");
            const std::string fileName = (sl == std::string::npos) ? fullUtf8 : fullUtf8.substr(sl + 1);
            const double sizeKb = static_cast<double>(fsize) / 1024.0;
            std::ostringstream coss;
            coss << "¿Confirma el envío de esta evaluación a TEC Digital?\n\n"
                 << "Curso: " << c.name << "\n"
                 << "Evaluación: " << chosen->name << "\n";
            if (!chosen->due_date_text.empty()) coss << "Vence: " << chosen->due_date_text << "\n";
            coss << "Archivo: " << fileName << " (" << FmtDouble(sizeKb, 1) << " KB)\n\n"
                 << "El archivo se enviará directamente al servidor de evaluaciones.";
            if (MessageBoxW(hwnd, Utf8ToWide(coss.str()).c_str(), L"Confirmar envío de evaluación",
                            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
                return 0;
            }
            if (st->workerBusy.exchange(true)) {
                SetWindowTextW(st->hProgressBanner, L"Hay otra operación en curso, espere un momento.");
                return 0;
            }
            td::EvaluationUploadRequest req;
            req.community_path = c.url;
            req.task_id = chosen->task_id;
            req.task_item_id = !chosen->task_item_id.empty() ? chosen->task_item_id : chosen->assign_id;
            req.grade_id = chosen->grade_id;
            req.student_id = chosen->student_id;
            req.with_group = chosen->with_group.empty() ? "0" : chosen->with_group;
            req.file_name = fileName;
            req.file_bytes = std::move(fileBytes);

            SetWindowTextW(st->hProgressBanner, Utf8ToWide("Subiendo entrega: " + fileName + "...").c_str());
            if (st->workerThread.joinable()) st->workerThread.join();
            st->workerThread = std::thread([st, req = std::move(req)]() mutable {
                auto ur = st->store->UploadCourseEvaluation(req, true);
                {
                    std::lock_guard<std::mutex> lk(st->workResultMutex);
                    st->pendingWorkKind = 2;
                    st->pendingWorkStatus = ur.ok
                        ? ("Entrega subida correctamente (" + req.file_name + ").")
                        : ("Error al subir entrega: " + ur.error_message);
                }
                st->workerBusy.store(false);
                PostMessageW(st->hwnd, WM_APP_TEC_WORK_DONE, ur.ok ? 1 : 0, 0);
            });
            return 0;
        }
        if (id == IDC_BTN_COURSE_SYNC_FILES && code == BN_CLICKED) {
            int idx = st->selectedCourseIdx;
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
            int idx = st->selectedCourseIdx;
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                OpenSafeUrl(hwnd, st->filteredCourses[static_cast<size_t>(idx)].url);
            }
            return 0;
        }
        if (id == IDC_BTN_COURSE_SEND_GEMINI && code == BN_CLICKED && st->onSendToGemini) {
            int idx = st->selectedCourseIdx;
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
            int idx = st->selectedCourseIdx;
            if (idx >= 0 && static_cast<size_t>(idx) < st->filteredCourses.size()) {
                const auto& c = st->filteredCourses[static_cast<size_t>(idx)];
                auto evIt = st->snap.evaluationsByPath.find(c.url);
                const td::DetailedEvaluationItem* chosen = nullptr;
                if (evIt != st->snap.evaluationsByPath.end()) {
                    int flat = 0;
                    for (const auto& cat : evIt->second.categories) {
                        for (const auto& it : cat.items) {
                            if (flat == st->selectedEvalFlatIdx && it.has_rubric && !it.rubric_id.empty()) {
                                chosen = &it;
                                break;
                            }
                            ++flat;
                        }
                        if (chosen) break;
                    }
                    if (!chosen) {
                        for (const auto& cat : evIt->second.categories) {
                            for (const auto& it : cat.items) {
                                if (it.has_rubric && !it.rubric_id.empty()) { chosen = &it; break; }
                            }
                            if (chosen) break;
                        }
                    }
                }
                if (!chosen) {
                    SetWindowTextW(st->hProgressBanner, L"Esta evaluación no tiene rúbrica analítica asociada.");
                } else {
                    const std::string rKey = chosen->rubric_id + "|" + chosen->task_item_id + "|" + chosen->task_id;
                    auto cachedIt = st->snap.rubricsByKey.find(rKey);
                    if (cachedIt != st->snap.rubricsByKey.end() && !cachedIt->second.criteria.empty()) {
                        st->activeRubric = cachedIt->second;
                        st->activeRubricAssignId = chosen->assign_id;
                        SetWindowTextW(st->hProgressBanner, L"Rúbrica cargada desde caché local.");
                        if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
                    } else {
                        auto rr = st->store->FetchRubric(chosen->rubric_id, chosen->community_id,
                                                         chosen->task_item_id, chosen->task_id, chosen->student_id);
                        if (rr.ok) {
                            st->snap = st->store->Snapshot();
                            st->activeRubric = rr.value;
                            st->activeRubricAssignId = chosen->assign_id;
                            SetWindowTextW(st->hProgressBanner, L"Rúbrica cargada en el inspector de la evaluación.");
                            if (st->hCourseDetailCanvas) InvalidateRect(st->hCourseDetailCanvas, nullptr, FALSE);
                        } else {
                            SetWindowTextW(st->hProgressBanner, Utf8ToWide("Rúbrica: " + rr.error_message).c_str());
                        }
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
        if (id == IDC_BTN_MALLA_TOGGLE_PICKER && code == BN_CLICKED) {
            st->mallaPickerExpanded = !st->mallaPickerExpanded;
            SwitchSection(st, HubSection::ExpedienteYMalla);
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
        if (id == IDC_EDIT_HOR_GLOBAL_Q && code == EN_CHANGE) {
            if (st->horViewMode == 0) RefreshGlobalHorariosSearch(st);
            return 0;
        }
        if ((id == IDC_CMB_HOR_PUB_SEDE || id == IDC_CMB_HOR_GLOBAL_SCHOOL ||
             id == IDC_CMB_HOR_PUB_PER || id == IDC_CMB_HOR_GLOBAL_MOD ||
             id == IDC_CMB_HOR_GLOBAL_DAY) && code == CBN_SELCHANGE) {
            if (st->horViewMode == 0) RefreshGlobalHorariosSearch(st);
            return 0;
        }
        if (id == IDC_CMB_HOR_YEAR && code == CBN_SELCHANGE) {
            if (st->horViewMode == 0 && st->store) {
                int iy = static_cast<int>(SendMessageW(st->hCmbHorYear, CB_GETCURSEL, 0, 0));
                int year = (iy == 1) ? 2025 : ((iy == 2) ? 2027 : 2026);
                std::string loadErr;
                if (st->store->LoadGlobalScheduleIndexFromDisk(year, &loadErr)) {
                    st->globalHorariosIdx = st->store->GetGlobalScheduleIndex();
                } else {
                    st->globalHorariosIdx = DocStudio::TecDigital::GlobalHorariosIndex{};
                    st->globalHorariosIdx.year = year;
                    st->store->SetGlobalScheduleIndexForTesting(st->globalHorariosIdx);
                }
                PopulateGlobalHorariosSchoolCombo(st);
                RefreshGlobalHorariosSearch(st);
            }
            return 0;
        }
        if (id == IDC_BTN_HOR_INDEX_ALL && code == BN_CLICKED) {
            StartAsyncGlobalHorariosIndex(st);
            return 0;
        }
        if (id == IDC_BTN_HOR_TOGGLE_FILTERS && code == BN_CLICKED) {
            st->horFiltersExpanded = !st->horFiltersExpanded;
            SwitchSection(st, HubSection::HorariosVivo);
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
            AppendMenuW(hm, MF_STRING, 6, L"Indexar todo el TEC (todas las escuelas y sedes)");
            const bool canEnrich = st->store && st->store->IsAuthenticated() &&
                                   !st->horSedes.empty() && !st->horCarreras.empty() && !st->horPeriodos.empty();
            AppendMenuW(hm, canEnrich ? MF_STRING : (MF_STRING | MF_GRAYED),
                        7, L"Enriquecer con cupos y aulas reales (Expediente TEC)");
            AppendMenuW(hm, MF_SEPARATOR, 0, nullptr);
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

            if (pick == 6) {
                StartAsyncGlobalHorariosIndex(st);
            } else if (pick == 7 && canEnrich) {
                if (st->workerBusy.exchange(true)) {
                    SetWindowTextW(st->hProgressBanner, L"Hay otra consulta en curso, espere un momento.");
                } else {
                    int is = std::max(0, static_cast<int>(SendMessageW(st->hCmbHorSede, CB_GETCURSEL, 0, 0)));
                    int ic = std::max(0, static_cast<int>(SendMessageW(st->hCmbHorCarrera, CB_GETCURSEL, 0, 0)));
                    int ip = std::max(0, static_cast<int>(SendMessageW(st->hCmbHorPeriodo, CB_GETCURSEL, 0, 0)));
                    std::string sk = st->horSedes[static_cast<size_t>(is)].key;
                    std::string ck = st->horCarreras[static_cast<size_t>(ic)].key;
                    std::string pk = st->horPeriodos[static_cast<size_t>(ip)].key;
                    std::string camp = st->horSedes[static_cast<size_t>(is)].name;
                    std::string carName = st->horCarreras[static_cast<size_t>(ic)].name;
                    SetWindowTextW(st->hProgressBanner, Utf8ToWide("Enriqueciendo cupos y aulas reales para " + carName + "...").c_str());
                    if (st->workerThread.joinable()) st->workerThread.join();
                    st->workerThread = std::thread([st, sk, ck, pk, camp, carName]() {
                        std::string err;
                        const bool ok = st->store->MergeExpedienteCarreraIntoGlobalIndex(sk, ck, pk, camp, carName, &err);
                        {
                            std::lock_guard<std::mutex> lk(st->workResultMutex);
                            st->pendingGlobalIndexOk = ok;
                            st->pendingGlobalIndexStatus = ok
                                ? ("Cupos y aulas reales enriquecidos para " + carName + ".")
                                : (err.empty() ? "No se pudo enriquecer desde Expediente TEC." : err);
                        }
                        st->workerBusy.store(false);
                        PostMessageW(st->hwnd, WM_APP_TEC_GLOBAL_HORARIOS_DONE, ok ? 1 : 0, 0);
                    });
                }
            } else if (pick == 1) {
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
                        if (st->horViewMode == 0) SwitchHorariosViewMode(st, 1);
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
            const bool solveFromGlobal =
                (st->horViewMode == 0) ||
                (!st->selectedHorCourseCodes.empty() && st->horariosCatalogKey.rfind("global:", 0) == 0);
            if (solveFromGlobal) {
                if (st->globalHorariosIdx.entries.empty()) {
                    StartAsyncGlobalHorariosIndex(st);
                    return 0;
                }
                if (st->selectedHorCourseCodes.empty()) {
                    SetWindowTextW(st->hHorStatus, L"Agrega cursos con '+ Agregar' para armar combinaciones sin choques.");
                    return 0;
                }
                ht::SolverPreferences prefs{};
                std::vector<ht::BusySlot> manualBusy;
                bool filterPrereq = false;
                bool includeBusy = false;
                bool hasReqFilter = false;
                BuildHorariosPrefsAndBusyFromUi(st, prefs, manualBusy, filterPrereq, includeBusy, hasReqFilter);
                auto baseFilter = BuildGlobalHorariosFilterFromUi(st);
                baseFilter.queryUtf8.clear();
                baseFilter.schoolOrCareerFilter.clear();
                baseFilter.dayFilter = -1;

                auto res = st->store->SolveSelectedCoursesFromGlobalIndex(
                    st->selectedHorCourseCodes, baseFilter, prefs, filterPrereq, includeBusy, manualBusy);
                if ((!res.ok || !res.solved.HasSolutions()) && !baseFilter.campusFilter.empty()) {
                    // Retry cross-campus automatically if user picked courses across different campuses
                    auto crossCampusFilter = baseFilter;
                    crossCampusFilter.campusFilter.clear();
                    auto res2 = st->store->SolveSelectedCoursesFromGlobalIndex(
                        st->selectedHorCourseCodes, crossCampusFilter, prefs, filterPrereq, includeBusy, manualBusy);
                    if (res2.ok && res2.solved.HasSolutions()) {
                        res = std::move(res2);
                    }
                }
                st->horariosCatalog = std::move(res.catalog);
                st->horariosResult = std::move(res.solved);
                st->horariosCatalogKey = "global:" + std::to_string(st->selectedHorCourseCodes.size());
                st->horariosUsedRequiredFilter = true;
                st->selectedScheduleIdx = 0;
                const std::string s = FormatHorariosSolveSummaryText(
                    st->horariosResult, st->horariosCatalog, true, res.errorUtf8);
                SwitchHorariosViewMode(st, 1);
                SetWindowTextW(st->hHorStatus, Utf8ToWide(s).c_str());
                if (st->hHorCanvas) InvalidateRect(st->hHorCanvas, nullptr, FALSE);
                return 0;
            }

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
            if (st->ftsModeTab == 0) {
                RefreshGlobalLibraryFilter(st);
                return 0;
            }
            std::string q = WideToUtf8(GetCtrlTextW(st->hEditFts));
            st->ftsHits = st->store->SearchOfflineMaterials(q);
            st->activeDocHits.clear();
            if (st->onSearchActiveDoc && !q.empty()) {
                st->activeDocHits = st->onSearchActiveDoc(q);
            }
            st->selectedFtsIdx = 0;
            st->ftsScrollY = 0;
            if (st->hFtsCanvas) InvalidateRect(st->hFtsCanvas, nullptr, FALSE);
            return 0;
        }
        if (id == IDC_LST_FTS_HITS && code == LBN_SELCHANGE) {
            return 0;
        }
        if (id == IDC_BTN_FTS_OPEN && code == BN_CLICKED) {
            int idx = st->selectedFtsIdx;
            if (st->ftsModeTab == 0) {
                if (idx >= 0 && static_cast<size_t>(idx) < st->globalLibFiltered.size()) {
                    const auto& e = st->globalLibFiltered[static_cast<size_t>(idx)];
                    std::string localP = e.localRelPath;
                    if (!localP.empty() && localP.find(':') == std::string::npos) {
                        localP = st->store->OfflineFilesRootUtf8() + "\\" + localP;
                    }
                    if (!localP.empty() && GetFileAttributesW(Utf8ToWide(localP).c_str()) != INVALID_FILE_ATTRIBUTES) {
                        ShellExecuteW(hwnd, L"open", Utf8ToWide(localP).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (!e.isEvalAttachment && !e.spaceUrl.empty() && !e.objectId.empty() && st->store) {
                        std::string dlPath, err;
                        if (st->store->DownloadSingleCourseFile(e.spaceUrl, e.objectId, &dlPath, &err) && !dlPath.empty()) {
                            st->snap = st->store->Snapshot();
                            st->globalLibCatalog = DocStudio::TecDigital::TecAcademicStore::BuildGlobalFileCatalog(st->snap);
                            RefreshGlobalLibraryFilter(st);
                            ShellExecuteW(hwnd, L"open", Utf8ToWide(dlPath).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                        } else if (!e.downloadUrl.empty()) {
                            OpenSafeUrl(hwnd, e.downloadUrl);
                        }
                    } else if (!e.downloadUrl.empty()) {
                        OpenSafeUrl(hwnd, e.downloadUrl);
                    }
                }
                return 0;
            }
            if (st->ftsModeTab == 2) {
                if (idx >= 0 && static_cast<size_t>(idx) < st->currentPersonalFiles.size()) {
                    const auto& pf = st->currentPersonalFiles[static_cast<size_t>(idx)];
                    std::string localP = !pf.logical_rel_path.empty() ? pf.logical_rel_path : pf.blob_rel_path;
                    if (!localP.empty() && localP.find(':') == std::string::npos) {
                        localP = st->store->OfflineFilesRootUtf8() + "\\" + localP;
                    }
                    if (!localP.empty() && GetFileAttributesW(Utf8ToWide(localP).c_str()) != INVALID_FILE_ATTRIBUTES) {
                        ShellExecuteW(hwnd, L"open", Utf8ToWide(localP).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                    } else if (!pf.download_url.empty()) {
                        OpenSafeUrl(hwnd, pf.download_url);
                    }
                }
                return 0;
            }
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
            int idx = st->selectedFtsIdx;
            if (st->ftsModeTab == 0) {
                if (idx >= 0 && static_cast<size_t>(idx) < st->globalLibFiltered.size()) {
                    const auto& e = st->globalLibFiltered[static_cast<size_t>(idx)];
                    std::string localP = e.localRelPath;
                    if (!localP.empty() && localP.find(':') == std::string::npos) {
                        localP = st->store->OfflineFilesRootUtf8() + "\\" + localP;
                    }
                    DocStudio::TecDigital::TecAcademicStore::ImportDownloadedMaterialToCanvas(
                        *st->activeDoc, localP, e.fileName, e.spaceName, e.downloadUrl);
                    st->docMutated = true;
                    if (st->onDocMutated) st->onDocMutated();
                    SetWindowTextW(st->hProgressBanner, L"Archivo de la biblioteca global insertado en el cuaderno.");
                }
                return 0;
            }
            if (st->ftsModeTab == 2) {
                if (idx >= 0 && static_cast<size_t>(idx) < st->currentPersonalFiles.size()) {
                    const auto& pf = st->currentPersonalFiles[static_cast<size_t>(idx)];
                    std::string localP = !pf.logical_rel_path.empty() ? pf.logical_rel_path : pf.blob_rel_path;
                    if (!localP.empty() && localP.find(':') == std::string::npos) {
                        localP = st->store->OfflineFilesRootUtf8() + "\\" + localP;
                    }
                    DocStudio::TecDigital::TecAcademicStore::ImportDownloadedMaterialToCanvas(
                        *st->activeDoc, localP, pf.name, "Mis documentos", pf.download_url);
                    st->docMutated = true;
                    if (st->onDocMutated) st->onDocMutated();
                    SetWindowTextW(st->hProgressBanner, L"Documento personal insertado en el cuaderno.");
                }
                return 0;
            }
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
            if (st->store) st->store->StopAutoMonitor();
            RemoveWindowsTecTrayIcon(st);
            st->JoinWorker();
            st->done = true;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (st) {
            if (st->store) st->store->StopAutoMonitor();
            RemoveWindowsTecTrayIcon(st);
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
    int initialInnerTab) {
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
    st.initialInnerTab = initialInnerTab;
    st.currentSection = initialSection;
    st.autoScreenshotPathW = autoScreenshotPathW;

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
        L"TEC Digital — CuadernU",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE,
        x, y, winW, winH,
        owner, nullptr, GetModuleHandleW(nullptr), &st);
    if (!hDlg) return false;

    if (autoScreenshotPathW && autoScreenshotPathW[0]) {
        RedrawWindow(hDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        MSG pmsg{};
        for (int i = 0; i < 64 && PeekMessageW(&pmsg, nullptr, 0, 0, PM_REMOVE); ++i) {
            TranslateMessage(&pmsg);
            DispatchMessageW(&pmsg);
        }
        RedrawWindow(hDlg, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN);
        Views::SaveWindowScreenshotPngW(hDlg, autoScreenshotPathW);
        DestroyWindow(hDlg);
        st.JoinWorker();
        return false;
    }

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
