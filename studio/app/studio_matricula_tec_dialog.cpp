#ifdef _WIN32

#include "studio_matricula_tec_dialog.hpp"

#include "academic/matricula_tec/client.hpp"
#include "academic/matricula_tec/models.hpp"
#include "academic/matricula_tec/protocol.hpp"
#include "academic/matricula_tec/scenario_engine.hpp"
#include "academic/matricula_tec/scheduler.hpp"
#include "academic/matricula_tec/vault.hpp"

#include "../core/utf_conv.hpp"
#include "studio_tec_digital_views.hpp"
#include "../../ui_glass/hig_dialog_theme.hpp"
#include "../../ui_glass/translucent_tokens.hpp"

#include <commctrl.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace DocStudio::MatriculaTecUi {

namespace mt = academic::matricula_tec;

namespace {

constexpr UINT WM_APP_MAT_STATUS = WM_APP + 420;
constexpr UINT WM_APP_MAT_LOG    = WM_APP + 421;
constexpr UINT WM_APP_MAT_SHOT   = WM_APP + 422;

enum CtrlId : int {
    IDC_SEG_CONN = 6101,
    IDC_SEG_SCEN = 6102,
    IDC_SEG_CITA = 6103,
    IDC_SEG_SNIP = 6104,

    // Tab 0: Conexion y Cupos
    IDC_ED_USER = 6201,
    IDC_ED_PASS = 6202,
    IDC_CHK_DPAPI = 6203,
    IDC_BTN_LOGIN = 6204,
    IDC_BTN_LOAD_VAULT = 6205,
    IDC_BTN_CLEAR_VAULT = 6206,
    IDC_ED_COURSE = 6207,
    IDC_BTN_QUERY_SEATS = 6208,
    IDC_ED_SEATS_OUT = 6209,

    // Tab 1: Escenarios Plan A/B/C
    IDC_ED_SCEN_JSON = 6301,
    IDC_BTN_SCEN_LOAD = 6302,
    IDC_BTN_SCEN_SAVE = 6303,
    IDC_BTN_SCEN_TEMPLATE = 6304,

    // Tab 2: Cita Autonoma (T-2m)
    IDC_ED_CITA_EPOCH = 6401,
    IDC_CMB_TRANSPORT = 6402,
    IDC_CMB_PIVOT = 6403,
    IDC_CHK_DRYRUN = 6404,
    IDC_CHK_INFORME = 6405,
    IDC_ED_PHRASE = 6406,
    IDC_BTN_ARM = 6407,
    IDC_BTN_START_MISSION = 6408,
    IDC_BTN_STOP_MISSION = 6409,
    IDC_BTN_SEND_INFORME = 6410,

    // Tab 3: Vigilante de Cupos (Seat Sniper)
    IDC_ED_SNIP_COURSES = 6501,
    IDC_ED_SNIP_INTERVAL = 6502,
    IDC_CHK_SNIP_AUTO = 6503,
    IDC_BTN_SNIP_START = 6504,
    IDC_BTN_SNIP_STOP = 6505,

    // Shared status + log
    IDC_LBL_STATUS = 6601,
    IDC_ED_LOG = 6602,
    IDC_BTN_CLOSE = 6603
};

std::wstring GetEditW(HWND h) {
    if (!h) return {};
    const int n = GetWindowTextLengthW(h);
    if (n <= 0) return {};
    std::wstring s(static_cast<size_t>(n) + 1u, L'\0');
    GetWindowTextW(h, s.data(), n + 1);
    s.resize(static_cast<size_t>(n));
    return s;
}

struct DialogState {
    HWND hwnd = nullptr;
    UiGlass::HigDialogTheme theme;
    UINT dpi = 96;
    HFONT hFont = nullptr;
    HFONT hFontBold = nullptr;
    HFONT hFontMono = nullptr;
    int activeTab = 0;
    std::string statusBanner;
    std::wstring autoShotW;
    bool stealth = false;

    std::shared_ptr<mt::MatriculaClient> client;
    std::unique_ptr<mt::AppointmentScheduler> scheduler;
    mt::ScenarioPlan plan;

    std::vector<HWND> tabCtrls[4];
    HWND hSeg[4]{};
    HWND hLblStatus = nullptr;
    HWND hEdLog = nullptr;
    HWND hBtnClose = nullptr;

    // Key controls
    HWND hEdUser = nullptr, hEdPass = nullptr, hChkDpapi = nullptr;
    HWND hEdCourse = nullptr, hEdSeatsOut = nullptr;
    HWND hEdScenJson = nullptr;
    HWND hEdCitaEpoch = nullptr, hCmbTransport = nullptr, hCmbPivot = nullptr;
    HWND hChkDryRun = nullptr, hChkInforme = nullptr, hEdPhrase = nullptr;
    HWND hEdSnipCourses = nullptr, hEdSnipInterval = nullptr, hChkSnipAuto = nullptr;

    int Sc(int px) const noexcept { return UiGlass::ScaleDpi(px, dpi); }

    void AppendLog(const std::string& utf8) {
        if (!hEdLog) return;
        std::wstring cur = GetEditW(hEdLog);
        if (!cur.empty()) cur += L"\r\n";
        cur += Utf8ToWide(utf8);
        if (cur.size() > 24000) cur = cur.substr(cur.size() - 24000);
        SetWindowTextW(hEdLog, cur.c_str());
        const int len = GetWindowTextLengthW(hEdLog);
        SendMessageW(hEdLog, EM_SETSEL, len, len);
        SendMessageW(hEdLog, EM_SCROLLCARET, 0, 0);
    }

    void SetStatus(const std::string& utf8) {
        statusBanner = utf8;
        if (hLblStatus) SetWindowTextW(hLblStatus, Utf8ToWide(utf8).c_str());
    }
};

void SwitchTab(DialogState* st, int idx) {
    st->activeTab = std::clamp(idx, 0, 3);
    for (int t = 0; t < 4; ++t) {
        const int show = (t == st->activeTab) ? SW_SHOW : SW_HIDE;
        for (HWND h : st->tabCtrls[t]) {
            if (h) ShowWindow(h, show);
        }
    }
    InvalidateRect(st->hwnd, nullptr, TRUE);
}

mt::ScenarioPlan BuildDefaultTemplatePlan() {
    mt::ScenarioPlan p{};
    p.convocatoria_id = "1";
    p.periodo_id = "2026-2";
    p.enable_plan_z_rescue = true;
    p.pivot_policy = mt::PivotPolicy::StrictlyConservative;
    p.min_enrolled_courses_safety_floor = 1;
    p.auto_send_informe_on_success = true;

    mt::EnrollmentScenario a{};
    a.id = "plan_a"; a.name = "Plan A (Prioridad 1)"; a.priority = 1; a.enabled = true;
    mt::CourseTarget ta{};
    ta.course_code = "FI1204";
    ta.course_name = "Fisica General III";
    ta.preferred_groups = { 1, 2 };
    ta.filter.earliest_start_minutes = 7 * 60;
    ta.filter.latest_end_minutes = 18 * 60;
    a.targets.push_back(std::move(ta));

    mt::EnrollmentScenario b{};
    b.id = "plan_b"; b.name = "Plan B (Respaldo)"; b.priority = 2; b.enabled = true;
    mt::CourseTarget tb{};
    tb.course_code = "EL3307";
    tb.course_name = "Diseno Electrico";
    tb.preferred_groups = { 1 };
    b.targets.push_back(std::move(tb));

    p.scenarios = { std::move(a), std::move(b) };
    return p;
}

HWND AddCtrl(DialogState* st, int tab, const wchar_t* cls, const wchar_t* text,
             DWORD style, DWORD exStyle, int x, int y, int w, int h, int id, HFONT font = nullptr) {
    HWND hw = CreateWindowExW(exStyle, cls, text, style | WS_CHILD | WS_VISIBLE,
                              st->Sc(x), st->Sc(y), st->Sc(w), st->Sc(h),
                              st->hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                              GetModuleHandleW(nullptr), nullptr);
    if (hw) {
        SendMessageW(hw, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : st->hFont), TRUE);
        if (tab >= 0 && tab < 4) st->tabCtrls[tab].push_back(hw);
    }
    return hw;
}

void BuildControls(DialogState* st) {
    // Segmented top navigation (guías de interfaz, zero emojis, clean typography)
    const wchar_t* segLabels[4] = {
        L"1. Conexión y Cupos",
        L"2. Escenarios (Plan A/B/C)",
        L"3. Cita Autónoma (T-2m)",
        L"4. Vigilante e Informe"
    };
    for (int i = 0; i < 4; ++i) {
        st->hSeg[i] = AddCtrl(st, -1, L"BUTTON", segLabels[i], BS_PUSHBUTTON, 0,
                              18 + i * 204, 14, 196, 30, IDC_SEG_CONN + i, st->hFontBold);
    }

    // ---- Tab 0: Conexion y Cupos ----
    AddCtrl(st, 0, L"STATIC", L"Correo / Usuario TEC:", 0, 0, 20, 62, 160, 20, 0);
    st->hEdUser = AddCtrl(st, 0, L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 0, 184, 58, 250, 24, IDC_ED_USER);
    AddCtrl(st, 0, L"STATIC", L"Contraseña:", 0, 0, 450, 62, 85, 20, 0);
    st->hEdPass = AddCtrl(st, 0, L"EDIT", L"", ES_PASSWORD | ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP, 0, 540, 58, 200, 24, IDC_ED_PASS);
    st->hChkDpapi = AddCtrl(st, 0, L"BUTTON", L"Guardar cifrado en bóveda Windows DPAPI (compartida con MCP)",
                            BS_AUTOCHECKBOX | WS_TABSTOP, 0, 20, 92, 430, 22, IDC_CHK_DPAPI);
    AddCtrl(st, 0, L"BUTTON", L"Iniciar sesión (Solo lectura)", BS_PUSHBUTTON, 0, 20, 122, 190, 28, IDC_BTN_LOGIN, st->hFontBold);
    AddCtrl(st, 0, L"BUTTON", L"Cargar bóveda DPAPI", BS_PUSHBUTTON, 0, 220, 122, 165, 28, IDC_BTN_LOAD_VAULT);
    AddCtrl(st, 0, L"BUTTON", L"Borrar bóveda", BS_PUSHBUTTON, 0, 395, 122, 125, 28, IDC_BTN_CLEAR_VAULT);

    AddCtrl(st, 0, L"STATIC", L"Consultar cupos en vivo (código de curso, ej. FI1204):", 0, 0, 20, 164, 340, 20, 0);
    st->hEdCourse = AddCtrl(st, 0, L"EDIT", L"FI1204", ES_AUTOHSCROLL | WS_BORDER, 0, 365, 160, 110, 24, IDC_ED_COURSE);
    AddCtrl(st, 0, L"BUTTON", L"Consultar cupos", BS_PUSHBUTTON, 0, 485, 159, 130, 26, IDC_BTN_QUERY_SEATS);
    st->hEdSeatsOut = AddCtrl(st, 0, L"EDIT", L"Sin consulta activa. Inicie sesión o cargue la bóveda DPAPI.",
                              ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER, 0, 20, 196, 804, 152, IDC_ED_SEATS_OUT, st->hFontMono);

    // ---- Tab 1: Escenarios Plan A/B/C ----
    AddCtrl(st, 1, L"STATIC", L"Definición JSON de escenarios jerárquicos (compartida con agente IA en matricula_scenarios.json):",
            0, 0, 20, 60, 620, 20, 0);
    AddCtrl(st, 1, L"BUTTON", L"Cargar JSON", BS_PUSHBUTTON, 0, 20, 84, 120, 26, IDC_BTN_SCEN_LOAD);
    AddCtrl(st, 1, L"BUTTON", L"Guardar JSON", BS_PUSHBUTTON, 0, 148, 84, 120, 26, IDC_BTN_SCEN_SAVE, st->hFontBold);
    AddCtrl(st, 1, L"BUTTON", L"Plantilla Plan A/B", BS_PUSHBUTTON, 0, 276, 84, 145, 26, IDC_BTN_SCEN_TEMPLATE);
    st->hEdScenJson = AddCtrl(st, 1, L"EDIT", L"", ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_HSCROLL | WS_BORDER,
                              0, 20, 118, 804, 230, IDC_ED_SCEN_JSON, st->hFontMono);

    // ---- Tab 2: Cita Autonoma (T-2m) ----
    AddCtrl(st, 2, L"STATIC", L"Hora de cita (epoch UTC; 0 = inmediato en simulación):", 0, 0, 20, 62, 340, 20, 0);
    st->hEdCitaEpoch = AddCtrl(st, 2, L"EDIT", L"0", ES_AUTOHSCROLL | WS_BORDER, 0, 365, 58, 160, 24, IDC_ED_CITA_EPOCH);

    AddCtrl(st, 2, L"STATIC", L"Transporte:", 0, 0, 20, 96, 90, 20, 0);
    st->hCmbTransport = AddCtrl(st, 2, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 0, 115, 92, 260, 120, IDC_CMB_TRANSPORT);
    SendMessageW(st->hCmbTransport, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"API rápida + Fallback Scraping Playwright"));
    SendMessageW(st->hCmbTransport, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solo API JSON"));
    SendMessageW(st->hCmbTransport, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Solo Scraping DOM Playwright"));
    SendMessageW(st->hCmbTransport, CB_SETCURSEL, 0, 0);

    AddCtrl(st, 2, L"STATIC", L"Política de pivote:", 0, 0, 395, 96, 120, 20, 0);
    st->hCmbPivot = AddCtrl(st, 2, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 0, 520, 92, 304, 120, IDC_CMB_PIVOT);
    SendMessageW(st->hCmbPivot, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Estrictamente conservadora (nunca retira)"));
    SendMessageW(st->hCmbPivot, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Reemplazo prioritario (piso SMaterias >= 1)"));
    SendMessageW(st->hCmbPivot, CB_SETCURSEL, 0, 0);

    st->hChkDryRun = AddCtrl(st, 2, L"BUTTON", L"Modo simulación segura (Dry-Run: cero mutaciones reales en el TEC)",
                             BS_AUTOCHECKBOX | WS_TABSTOP, 0, 20, 130, 520, 22, IDC_CHK_DRYRUN);
    SendMessageW(st->hChkDryRun, BM_SETCHECK, BST_CHECKED, 0);

    st->hChkInforme = AddCtrl(st, 2, L"BUTTON", L"Enviar comprobante oficial automáticamente (Mi Informe -> Enviar)",
                              BS_AUTOCHECKBOX | WS_TABSTOP, 0, 20, 158, 520, 22, IDC_CHK_INFORME);
    SendMessageW(st->hChkInforme, BM_SETCHECK, BST_CHECKED, 0);

    AddCtrl(st, 2, L"STATIC", L"Frase de confirmación real (CONFIRMAR MATRICULA REAL):", 0, 0, 20, 194, 370, 20, 0);
    st->hEdPhrase = AddCtrl(st, 2, L"EDIT", L"", ES_AUTOHSCROLL | WS_BORDER, 0, 395, 190, 250, 24, IDC_ED_PHRASE);
    AddCtrl(st, 2, L"BUTTON", L"Armar candado", BS_PUSHBUTTON, 0, 655, 189, 169, 26, IDC_BTN_ARM);

    AddCtrl(st, 2, L"BUTTON", L"Iniciar misión autónoma (T-2:00)", BS_PUSHBUTTON, 0, 20, 232, 240, 32, IDC_BTN_START_MISSION, st->hFontBold);
    AddCtrl(st, 2, L"BUTTON", L"Detener misión", BS_PUSHBUTTON, 0, 270, 232, 140, 32, IDC_BTN_STOP_MISSION);
    AddCtrl(st, 2, L"BUTTON", L"Enviar Mi Informe ahora", BS_PUSHBUTTON, 0, 420, 232, 190, 32, IDC_BTN_SEND_INFORME);

    // ---- Tab 3: Vigilante de Cupos (Seat Sniper) ----
    AddCtrl(st, 3, L"STATIC", L"Cursos a vigilar (separados por coma, ej. FI1204,EL3307):", 0, 0, 20, 62, 370, 20, 0);
    st->hEdSnipCourses = AddCtrl(st, 3, L"EDIT", L"FI1204", ES_AUTOHSCROLL | WS_BORDER, 0, 395, 58, 220, 24, IDC_ED_SNIP_COURSES);
    AddCtrl(st, 3, L"STATIC", L"Intervalo (seg):", 0, 0, 630, 62, 95, 20, 0);
    st->hEdSnipInterval = AddCtrl(st, 3, L"EDIT", L"20", ES_AUTOHSCROLL | WS_BORDER, 0, 730, 58, 94, 24, IDC_ED_SNIP_INTERVAL);
    st->hChkSnipAuto = AddCtrl(st, 3, L"BUTTON", L"Auto-matricular + Enviar Mi Informe al detectar cupo (requiere candado armado si Dry-Run está desactivado)",
                               BS_AUTOCHECKBOX | WS_TABSTOP, 0, 20, 96, 790, 22, IDC_CHK_SNIP_AUTO);
    AddCtrl(st, 3, L"BUTTON", L"Iniciar Vigilante de Cupos", BS_PUSHBUTTON, 0, 20, 130, 210, 30, IDC_BTN_SNIP_START, st->hFontBold);
    AddCtrl(st, 3, L"BUTTON", L"Detener Vigilante", BS_PUSHBUTTON, 0, 240, 130, 150, 30, IDC_BTN_SNIP_STOP);

    // ---- Shared Status + Audit Log ----
    st->hLblStatus = AddCtrl(st, -1, L"STATIC", L"Listo (Modo seguro Dry-Run activado · C23/C++23).", 0, 0, 20, 360, 680, 22, IDC_LBL_STATUS, st->hFontBold);
    st->hBtnClose = AddCtrl(st, -1, L"BUTTON", L"Cerrar", BS_PUSHBUTTON, 0, 724, 356, 100, 28, IDC_BTN_CLOSE);
    st->hEdLog = AddCtrl(st, -1, L"EDIT", L"Registro sanitizado (nunca imprime contraseñas ni cookies).",
                         ES_MULTILINE | ES_READONLY | WS_VSCROLL | WS_BORDER, 0, 20, 390, 804, 150, IDC_ED_LOG, st->hFontMono);

    // Load saved scenario JSON or default template
    std::string lerr;
    if (!mt::LoadScenarioPlanFromFile(mt::DefaultScenariosPathUtf8(), st->plan, &lerr)) {
        st->plan = BuildDefaultTemplatePlan();
    }
    SetWindowTextW(st->hEdScenJson, Utf8ToWide(mt::ScenarioPlanToJson(st->plan).dump(2)).c_str());

    // Indicate DPAPI vault status
    if (mt::ProtectedCredentialVault::DpapiFileExists(mt::DefaultVaultPathUtf8())) {
        st->AppendLog("Boveda Windows DPAPI detectada en " + mt::DefaultVaultPathUtf8());
    }

    SwitchTab(st, 0);
}

LRESULT CALLBACK MatriculaDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (st) {
        LRESULT themeRes = 0;
        if (st->theme.HandleMessage(hwnd, msg, wp, lp, themeRes)) {
            return themeRes;
        }
    }

    switch (msg) {
        case WM_COMMAND: {
            if (!st) break;
            const int id = LOWORD(wp);
            if (id >= IDC_SEG_CONN && id <= IDC_SEG_SNIP) {
                SwitchTab(st, id - IDC_SEG_CONN);
                return 0;
            }
            if (id == IDC_BTN_CLOSE || id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (id == IDC_BTN_LOAD_VAULT) {
                std::string err;
                if (st->scheduler->LoadCredentialsFromDpapi("", &err)) {
                    st->SetStatus("Credenciales cargadas desde boveda DPAPI en memoria protegida.");
                    st->AppendLog("DPAPI cargado correctamente (sin exponer contrasena).");
                } else {
                    st->SetStatus("Error DPAPI: " + err);
                    st->AppendLog("Error DPAPI: " + err);
                }
                return 0;
            }
            if (id == IDC_BTN_CLEAR_VAULT) {
                std::string err;
                st->scheduler->ClearCredentials();
                mt::ProtectedCredentialVault::DeleteDpapiFile(mt::DefaultVaultPathUtf8(), &err);
                st->SetStatus("Boveda DPAPI y memoria RAM limpiadas.");
                st->AppendLog("Credenciales eliminadas de memoria y disco.");
                return 0;
            }
            if (id == IDC_BTN_LOGIN) {
                const std::string user = WideToUtf8(GetEditW(st->hEdUser));
                const std::string pass = WideToUtf8(GetEditW(st->hEdPass));
                const bool save_dpapi = (SendMessageW(st->hChkDpapi, BM_GETCHECK, 0, 0) == BST_CHECKED);
                std::string err;
                if (!user.empty() && !pass.empty()) {
                    st->scheduler->SetCredentials(user, pass, &err);
                    if (save_dpapi) {
                        mt::ProtectedCredentialVault::SaveToDpapiFile(mt::DefaultVaultPathUtf8(), user, pass, &err);
                    }
                    SetWindowTextW(st->hEdPass, L"");
                }
                if (!st->scheduler->HasCredentials()) {
                    st->SetStatus("Ingrese credenciales o cargue la boveda DPAPI primero.");
                    return 0;
                }
                st->SetStatus("Conectando al portal oficial de Matricula TEC...");
                std::thread([st, hwnd]() {
                    std::string lerr;
                    mt::ProtectedCredentials creds;
                    // Run read-only clock sync + login via client
                    const bool ok = st->scheduler->Client()->SyncServerClock(nullptr, &lerr);
                    auto* msgStr = new std::string(ok
                        ? ("Reloj TEC sincronizado (offset=" + std::to_string(st->scheduler->Client()->ServerClockOffsetMs()) + " ms). Listo para login/consulta.")
                        : ("Error de red/TLS: " + lerr));
                    PostMessageW(hwnd, WM_APP_MAT_LOG, 0, reinterpret_cast<LPARAM>(msgStr));
                }).detach();
                return 0;
            }
            if (id == IDC_BTN_QUERY_SEATS) {
                const std::string code = WideToUtf8(GetEditW(st->hEdCourse));
                if (!st->client->IsAuthenticated()) {
                    st->SetStatus("Inicie sesion en el portal TEC para consultar cupos en vivo.");
                    return 0;
                }
                std::vector<mt::CourseGroupLive> groups;
                std::string err;
                if (st->client->FetchLiveGroups(code, groups, &err)) {
                    std::ostringstream oss;
                    for (const auto& g : groups) {
                        oss << g.course_code << " G" << g.grupo
                            << " | Cupos: " << g.cupo_disponible << "/" << g.cupo_maximo
                            << " | Sede: " << g.sede_nombre << "\r\n";
                    }
                    const std::string out = oss.str().empty() ? "Sin grupos encontrados." : oss.str();
                    SetWindowTextW(st->hEdSeatsOut, Utf8ToWide(out).c_str());
                    st->SetStatus("Consulta en vivo completada para " + code);
                } else {
                    st->SetStatus("Error consultando cupos: " + err);
                }
                return 0;
            }
            if (id == IDC_BTN_SCEN_TEMPLATE) {
                st->plan = BuildDefaultTemplatePlan();
                SetWindowTextW(st->hEdScenJson, Utf8ToWide(mt::ScenarioPlanToJson(st->plan).dump(2)).c_str());
                st->SetStatus("Plantilla Plan A / Plan B cargada.");
                return 0;
            }
            if (id == IDC_BTN_SCEN_LOAD) {
                std::string err;
                if (mt::LoadScenarioPlanFromFile(mt::DefaultScenariosPathUtf8(), st->plan, &err)) {
                    SetWindowTextW(st->hEdScenJson, Utf8ToWide(mt::ScenarioPlanToJson(st->plan).dump(2)).c_str());
                    st->SetStatus("Escenarios cargados desde matricula_scenarios.json.");
                } else {
                    st->SetStatus("No se pudo cargar escenarios: " + err);
                }
                return 0;
            }
            if (id == IDC_BTN_SCEN_SAVE) {
                try {
                    const std::string raw = WideToUtf8(GetEditW(st->hEdScenJson));
                    st->plan = mt::ScenarioPlanFromJson(nlohmann::json::parse(raw));
                    std::string err;
                    if (mt::SaveScenarioPlanToFile(st->plan, mt::DefaultScenariosPathUtf8(), &err)) {
                        st->SetStatus("Escenarios guardados y sincronizados con agente IA MCP.");
                    } else {
                        st->SetStatus("Error guardando JSON: " + err);
                    }
                } catch (const std::exception& ex) {
                    st->SetStatus(std::string("JSON invalido: ") + ex.what());
                }
                return 0;
            }
            if (id == IDC_BTN_ARM) {
                const std::string phrase = WideToUtf8(GetEditW(st->hEdPhrase));
                std::string err;
                if (st->scheduler->ArmMission(true, phrase, &err)) {
                    st->SetStatus("CANDADO ARMADO para matricula real (se desarmara automaticamente al finalizar).");
                    st->AppendLog("Candado armado con frase oficial.");
                } else {
                    st->SetStatus("Candado bloqueado: " + err);
                }
                return 0;
            }
            if (id == IDC_BTN_START_MISSION) {
                mt::AppointmentMissionConfig cfg{};
                cfg.plan = st->plan;
                cfg.dry_run = (SendMessageW(st->hChkDryRun, BM_GETCHECK, 0, 0) == BST_CHECKED);
                cfg.send_informe_after_enroll = (SendMessageW(st->hChkInforme, BM_GETCHECK, 0, 0) == BST_CHECKED);
                try { cfg.cita_epoch_utc = std::stoll(WideToUtf8(GetEditW(st->hEdCitaEpoch))); } catch (...) { cfg.cita_epoch_utc = 0; }
                const int tr = static_cast<int>(SendMessageW(st->hCmbTransport, CB_GETCURSEL, 0, 0));
                cfg.transport_mode = (tr == 1) ? mt::TransportMode::ApiOnly
                                   : (tr == 2) ? mt::TransportMode::ScrapingOnly
                                               : mt::TransportMode::ApiWithScrapingFallback;
                const int pv = static_cast<int>(SendMessageW(st->hCmbPivot, CB_GETCURSEL, 0, 0));
                cfg.plan.pivot_policy = (pv == 1) ? mt::PivotPolicy::AllowPriorityScenarioReplace
                                                  : mt::PivotPolicy::StrictlyConservative;
                std::string err;
                if (st->scheduler->StartMission(cfg, [hwnd](const mt::AppointmentStatus& s) {
                        auto* copy = new std::string(s.state_label + " | " + s.last_message);
                        PostMessageW(hwnd, WM_APP_MAT_STATUS, 0, reinterpret_cast<LPARAM>(copy));
                    }, &err)) {
                    st->SetStatus("Mision autonoma iniciada.");
                } else {
                    st->SetStatus("No se pudo iniciar mision: " + err);
                }
                return 0;
            }
            if (id == IDC_BTN_STOP_MISSION) {
                st->scheduler->StopMission();
                st->scheduler->Disarm();
                st->SetStatus("Mision detenida y candado desarmado.");
                return 0;
            }
            if (id == IDC_BTN_SEND_INFORME) {
                const bool dry_run = (SendMessageW(st->hChkDryRun, BM_GETCHECK, 0, 0) == BST_CHECKED);
                const auto res = st->client->SendInformeEmail("", dry_run);
                st->SetStatus(res.message);
                st->AppendLog("Mi Informe -> Enviar: " + res.message);
                return 0;
            }
            if (id == IDC_BTN_SNIP_START) {
                mt::SniperConfig sc{};
                sc.dry_run = (SendMessageW(st->hChkDryRun, BM_GETCHECK, 0, 0) == BST_CHECKED);
                sc.auto_enroll = (SendMessageW(st->hChkSnipAuto, BM_GETCHECK, 0, 0) == BST_CHECKED);
                try { sc.poll_interval_seconds = std::max(5, std::stoi(WideToUtf8(GetEditW(st->hEdSnipInterval)))); } catch (...) { sc.poll_interval_seconds = 20; }
                std::stringstream ss(WideToUtf8(GetEditW(st->hEdSnipCourses)));
                std::string item;
                while (std::getline(ss, item, ',')) {
                    while (!item.empty() && item.front() == ' ') item.erase(item.begin());
                    while (!item.empty() && item.back() == ' ') item.pop_back();
                    if (!item.empty()) {
                        mt::SniperTarget t{};
                        t.course_code = item;
                        sc.targets.push_back(std::move(t));
                    }
                }
                std::string err;
                if (st->scheduler->StartSniper(sc, [hwnd](const mt::SniperStatus& s) {
                        auto* copy = new std::string("Sniper #" + std::to_string(s.poll_count) + " | " + s.last_message);
                        PostMessageW(hwnd, WM_APP_MAT_STATUS, 0, reinterpret_cast<LPARAM>(copy));
                    }, &err)) {
                    st->SetStatus("Vigilante de cupos iniciado.");
                } else {
                    st->SetStatus("Error vigilante: " + err);
                }
                return 0;
            }
            if (id == IDC_BTN_SNIP_STOP) {
                st->scheduler->StopSniper();
                st->SetStatus("Vigilante de cupos detenido.");
                return 0;
            }
            break;
        }
        case WM_APP_MAT_STATUS: {
            std::unique_ptr<std::string> s(reinterpret_cast<std::string*>(lp));
            if (st && s) {
                st->SetStatus(*s);
                st->AppendLog(*s);
            }
            return 0;
        }
        case WM_APP_MAT_LOG: {
            std::unique_ptr<std::string> s(reinterpret_cast<std::string*>(lp));
            if (st && s) {
                st->SetStatus(*s);
                st->AppendLog(*s);
            }
            return 0;
        }
        case WM_APP_MAT_SHOT: {
            if (st && !st->autoShotW.empty()) {
                DocStudio::TecDigitalUi::Views::SaveWindowScreenshotPngW(hwnd, st->autoShotW.c_str());
                DestroyWindow(hwnd);
            }
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            if (st && st->scheduler) {
                st->scheduler->StopMission();
                st->scheduler->StopSniper();
            }
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

bool RunMatriculaTecDialog(
    HWND owner,
    std::string& outStatusBannerUtf8,
    const UiGlass::GlassSettings* glassSettings,
    const wchar_t* autoScreenshotPathW,
    bool backgroundStealth)
{
    static bool clsRegistered = false;
    const wchar_t* kCls = L"CuadernU_MatriculaTec_HIG_Wnd";
    if (!clsRegistered) {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = MatriculaDlgProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kCls;
        RegisterClassExW(&wc);
        clsRegistered = true;
    }

    DialogState st{};
    st.dpi = UiGlass::DpiOfWindow(owner);
    st.client = std::make_shared<mt::MatriculaClient>();
    st.scheduler = std::make_unique<mt::AppointmentScheduler>(st.client);
    st.stealth = backgroundStealth;
    if (autoScreenshotPathW && autoScreenshotPathW[0]) {
        st.autoShotW = autoScreenshotPathW;
    }

    const int w = st.Sc(860);
    const int h = st.Sc(600);
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    const int x = backgroundStealth ? -20000 : CW_USEDEFAULT;
    const int y = backgroundStealth ? -20000 : CW_USEDEFAULT;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kCls,
        L"Matrícula TEC Autónoma — CuadernU",
        style,
        x, y, w, h,
        owner,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (!hwnd) return false;

    st.hwnd = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&st));
    st.theme.Init(hwnd, glassSettings);
    st.hFont = CreateFontW(-st.Sc(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    st.hFontBold = CreateFontW(-st.Sc(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    st.hFontMono = CreateFontW(-st.Sc(12), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               FIXED_PITCH | FF_MODERN, L"Consolas");

    BuildControls(&st);
    st.theme.ThemeAllChildren(hwnd);

    if (owner && IsWindow(owner) && !backgroundStealth) {
        EnableWindow(owner, FALSE);
    }
    ShowWindow(hwnd, backgroundStealth ? SW_SHOWNOACTIVATE : SW_SHOW);
    UpdateWindow(hwnd);

    if (!st.autoShotW.empty()) {
        PostMessageW(hwnd, WM_APP_MAT_SHOT, 0, 0);
    }

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner && IsWindow(owner) && !backgroundStealth) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    if (st.hFont) DeleteObject(st.hFont);
    if (st.hFontBold) DeleteObject(st.hFontBold);
    if (st.hFontMono) DeleteObject(st.hFontMono);

    outStatusBannerUtf8 = st.statusBanner;
    return true;
}

} // namespace DocStudio::MatriculaTecUi

#endif // _WIN32
