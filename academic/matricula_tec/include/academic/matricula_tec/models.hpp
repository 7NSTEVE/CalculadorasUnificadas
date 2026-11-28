#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace academic::matricula_tec {

inline constexpr const char* kDefaultBaseUrl = "https://tec-appsext.itcr.ac.cr/Matricula/";
inline constexpr const char* kConfirmationPhrase = "CONFIRMAR MATRICULA REAL";

struct WebFormsTokens {
    std::string view_state;
    std::string view_state_generator;
    std::string event_validation;

    bool IsComplete() const noexcept {
        return !view_state.empty() && !event_validation.empty();
    }
};

struct ConvocatoriaInfo {
    std::string id_convocatoria;
    std::string id_periodo;
    std::string nombre;
    std::string sede;
    std::string modalidad;
    std::string tipo;
};

struct StudentBootstrap {
    std::string carnet;
    std::string nombre;
    std::string carrera_id;
    std::string carrera_nombre;
    std::string plan_id;
    std::string sede_id;
    std::string sede_nombre;
    std::string correo;
    std::string periodo_activo;
    std::string convocatoria_activa;
    std::vector<ConvocatoriaInfo> convocatorias;
};

struct CourseOffer {
    std::string codigo;
    std::string nombre;
    int creditos = 0;
    std::string tipo;
    std::string modalidad;
    std::string estado; // e.g., "M" matriculado, "D" disponible, etc.
    bool matriculado = false;
    int grupo_matriculado = -1;
    std::vector<std::string> correquisitos;
};

struct ScheduleSession {
    int day_index = -1;     // 0=L, 1=K, 2=M, 3=J, 4=V, 5=S, 6=D
    std::string day_code;   // "L","K","M","J","V","S"
    int start_minutes = 0;  // minutes since 00:00
    int end_minutes = 0;
    std::string start_text; // "07:30"
    std::string end_text;   // "09:20"
    std::string edificio;
    std::string aula;
};

struct CourseGroupLive {
    std::string course_code;
    std::string course_name;
    int grupo = 0;
    int cupo_disponible = 0;
    int cupo_maximo = 0;
    int matriculados = 0;
    std::string sede_id;
    std::string sede_nombre;
    std::string modalidad;
    std::string tipo_grupo;
    std::string estado;
    bool matriculado = false;
    std::vector<std::string> profesores;
    std::vector<ScheduleSession> sesiones;
};

struct CitaInfo {
    bool tiene_cita = false;
    std::string fecha;         // e.g. "2026-07-20" or DATIC raw string
    std::string hora_inicio;   // e.g. "08:00"
    std::string hora_fin;
    std::string convocatoria;
    std::string sede;
    std::string raw_text;
    int64_t cita_epoch_utc = 0;
    bool cita_activa_ahora = false;
};

struct MorosidadInfo {
    bool tiene_morosidad = false;
    std::vector<std::string> detalles;
};

struct EnrolledScheduleItem {
    std::string course_code;
    std::string course_name;
    int grupo = 0;
    int creditos = 0;
    std::string sede;
    std::vector<ScheduleSession> sesiones;
};

struct InformeSummary {
    bool ok = false;
    std::string html_informe;
    std::string correo_destino;
    int cursos_count = 0;
    int creditos_total = 0;
    std::string error_message;
};

struct EnrollmentResult {
    bool ok = false;
    bool dry_run = true;
    std::string transport_used; // "api" | "scraper" | "dry_run"
    std::string action;         // "enroll" | "drop" | "send_informe"
    std::string course_code;
    int grupo = 0;
    int status_code = 0;
    std::string datic_code;     // "OK", "SCupo", "CChoque", "SMaterias", "ERROR", etc.
    std::string message;
    bool requires_corequisite = false;
    std::string corequisite_code;
    bool requires_campus_cancel = false;
};

struct ScheduleFilterRule {
    std::vector<int> allowed_groups;
    std::vector<int> excluded_groups;
    std::vector<int> allowed_days;        // 0=L..5=S; empty = any
    std::vector<int> excluded_days;       // 0=L..5=S
    int earliest_start_minutes = 0;       // 00:00
    int latest_end_minutes = 23 * 60 + 59;
    std::vector<std::string> allowed_campuses;
    std::vector<std::string> preferred_professors;
    std::vector<std::string> excluded_professors;
    bool require_seats = true;
};

struct CourseTarget {
    std::string course_code;
    std::string course_name;
    std::vector<int> preferred_groups; // ordered preference; empty = any matching filter
    ScheduleFilterRule filter;
    bool optional_course = false;
    std::string corequisite_code;
    int corequisite_group = -1;
};

struct EnrollmentScenario {
    std::string id;
    std::string name;      // e.g. "Plan A", "Plan B", "Plan C"
    int priority = 1;      // lower = higher priority
    bool enabled = true;
    std::vector<CourseTarget> targets;
};

enum class PivotPolicy {
    StrictlyConservative = 0,       // NEVER auto-drop already enrolled courses
    AllowPriorityScenarioReplace = 1 // Opt-in: only if >=2 enabled scenarios, verified target seats > 0, and >= safety floor
};

enum class TransportMode {
    ApiWithScrapingFallback = 0,
    ApiOnly = 1,
    ScrapingOnly = 2
};

struct ScenarioPlan {
    std::string convocatoria_id;
    std::string periodo_id;
    std::string sede_id;
    std::vector<EnrollmentScenario> scenarios;
    bool enable_plan_z_rescue = true;
    PivotPolicy pivot_policy = PivotPolicy::StrictlyConservative;
    int min_enrolled_courses_safety_floor = 1; // NEVER drop to 0 courses (prevents SMaterias)
    bool auto_send_informe_on_success = true;
};

struct SelectedGroupChoice {
    std::string course_code;
    int grupo = 0;
    int cupo_disponible = 0;
    int preference_rank = 0;
    std::string corequisite_code;
    int corequisite_group = -1;
    CourseGroupLive group_data;
};

struct ScenarioEvaluation {
    bool feasible = false;
    std::string scenario_id;
    std::string scenario_name;
    bool is_plan_z = false;
    std::vector<SelectedGroupChoice> choices;      // Ordered by MRV (tightest seat count first)
    std::vector<std::string> missing_courses;
    std::vector<std::string> conflict_notes;
    std::vector<std::string> drop_courses_required; // Always empty under StrictlyConservative
};

struct AppointmentMissionConfig {
    std::string base_url = kDefaultBaseUrl;
    int64_t cita_epoch_utc = 0;          // Target enrollment timestamp (UTC epoch seconds)
    int pre_login_lead_seconds = 120;    // T - 2:00 default
    int keep_alive_interval_seconds = 25;
    int re_login_before_seconds = 5;     // Refresh/re-login near T - 0:00
    bool dry_run = true;                 // SAFE DEFAULT
    TransportMode transport_mode = TransportMode::ApiWithScrapingFallback;
    bool send_informe_after_enroll = true;
    ScenarioPlan plan;
};

enum class SchedulerState {
    Idle = 0,
    WaitingForPreLogin = 1,
    PreLoggingIn = 2,
    PreLoggedInStandby = 3,
    ZeroHourRefreshing = 4,
    ExecutingPlan = 5,
    SendingInforme = 6,
    Completed = 7,
    Failed = 8,
    Cancelled = 9
};

struct AppointmentStatus {
    SchedulerState state = SchedulerState::Idle;
    std::string state_label;
    int64_t server_clock_offset_ms = 0;
    int64_t seconds_to_cita = 0;
    bool authenticated = false;
    bool armed = false;
    bool dry_run = true;
    bool fallback_scraper_active = false;
    std::string active_scenario;
    int enrolled_count = 0;
    bool informe_sent = false;
    std::string last_message;
    std::vector<EnrollmentResult> results;
    std::vector<std::string> recent_logs;
};

struct SniperTarget {
    std::string course_code;
    std::vector<int> allowed_groups;
    ScheduleFilterRule filter;
};

struct SniperConfig {
    std::string base_url = kDefaultBaseUrl;
    std::string convocatoria_id;
    std::string periodo_id;
    std::vector<SniperTarget> targets;
    int poll_interval_seconds = 20;
    int jitter_seconds = 5;
    bool auto_enroll = false; // SAFE DEFAULT: alert-only unless armed + auto_enroll=true
    bool dry_run = true;
    bool send_informe_on_snipe = true;
    TransportMode transport_mode = TransportMode::ApiWithScrapingFallback;
};

struct SniperStatus {
    bool running = false;
    bool armed = false;
    bool auto_enroll = false;
    bool dry_run = true;
    int poll_count = 0;
    int seats_detected_count = 0;
    int enrolled_count = 0;
    bool informe_sent = false;
    std::string last_poll_utc;
    std::string last_message;
    std::vector<EnrollmentResult> sniped_results;
};

// JSON serialization helpers (interoperable with %LOCALAPPDATA%\CuadernU\matricula_scenarios.json)
nlohmann::json ScheduleFilterToJson(const ScheduleFilterRule& f);
ScheduleFilterRule ScheduleFilterFromJson(const nlohmann::json& j);
nlohmann::json ScenarioPlanToJson(const ScenarioPlan& plan);
ScenarioPlan ScenarioPlanFromJson(const nlohmann::json& j);
bool SaveScenarioPlanToFile(const ScenarioPlan& plan, const std::string& path_utf8, std::string* out_err = nullptr);
bool LoadScenarioPlanFromFile(const std::string& path_utf8, ScenarioPlan& out_plan, std::string* out_err = nullptr);
std::string DefaultCuadernuDataDirUtf8();
std::string DefaultVaultPathUtf8();
std::string DefaultScenariosPathUtf8();

} // namespace academic::matricula_tec
