#pragma once

#include "academic/tec_digital/types.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace academic::tec_digital {

// ============================================================================
// 1. Detailed Evaluations, Analytic Rubrics & What-If Grade Calculator
// ============================================================================

struct DetailedEvaluationItem {
    std::string assign_id;
    std::string category_name;
    std::string name;
    std::string description;
    std::string attachment_url;
    std::string solution_url;
    std::string feedback_url;
    double weight_pct = 0.0;
    double earned_pct = 0.0;
    double raw_grade_100 = -1.0;  // < 0 means not yet graded
    bool is_graded = false;
    std::string due_date_text;
    std::string due_date_iso;     // YYYY-MM-DDTHH:MM when parseable
    bool allow_late_submission = false;
    int group_size = 1;
    std::vector<std::string> group_members;
    bool has_rubric = false;
    std::string rubric_id;
    std::string task_id;
    std::string task_item_id;
    std::string community_id;
    std::string submission_status;
    std::vector<std::string> submitted_files;
    std::string teacher_comment;
    std::string linked_gaap_id;
    std::string linked_mlc_id;
    // 2026 live submission & upload metadata (populated when assignment accepts submissions)
    bool can_upload = false;
    bool can_upload_url = false;
    std::string grade_id;
    std::string student_id;
    std::string with_group = "0";
    std::string due_date_gmt;
    int max_file_size_mb = 80;
    std::string latest_revision_id;
    std::string latest_item_id;
};

struct EvaluationUploadRequest {
    std::string community_path;
    std::string task_id;
    std::string task_item_id;
    std::string grade_id;
    std::string student_id;
    std::string with_group = "0";
    std::string file_name;        // non-empty when uploading a local file
    std::string file_mime = "application/octet-stream";
    std::string file_bytes;       // binary bytes when uploading a local file
    std::string submission_url;   // non-empty when submitting a URL link
};

struct EvaluationUploadResult {
    bool ok = false;
    int http_status = 0;
    std::string revision_id;
    std::string item_id;
    std::string result_html;
    std::string error_message;
};

struct DetailedEvaluationCategory {
    std::string category_id;
    std::string name;
    double weight_pct = 0.0;
    double earned_pct = 0.0;
    std::vector<DetailedEvaluationItem> items;
};

struct DetailedEvaluationsReport {
    std::string community_slug;
    double unrounded_total = 0.0;
    double final_grade = 0.0;
    bool has_final_grade = false;
    std::vector<DetailedEvaluationCategory> categories;
};

struct RubricLevel {
    std::string label;
    double points = 0.0;
    std::string description;
    bool achieved = false;
};

struct RubricCriterion {
    std::string category_name;
    double earned_points = 0.0;
    double max_points = 0.0;
    std::vector<RubricLevel> levels;
    double weight_pct = 0.0;        // e.g. 15.0 for "Peso 15%"
    double earned_weight_pct = 0.0; // e.g. 10.0 when 2/3 of 15%
    std::string comment;            // per-criterion teacher feedback (tr[comment])
};

struct RubricBreakdown {
    std::string rubric_id;
    std::string title;
    double total_score = 0.0;
    double max_score = 0.0;
    std::vector<RubricCriterion> criteria;
    std::string evaluated_target;   // e.g. "Grupo 5"
    double max_level_points = 0.0;  // e.g. 3.0 from #div_info_rubric[max_weight]
    bool is_graded = false;
};

struct GradeWhatIfScenario {
    double current_earned_pct = 0.0;
    double evaluated_weight_pct = 0.0;
    double remaining_weight_pct = 0.0;
    double total_weight_pct = 0.0;
    double target_grade = 70.0;
    double required_avg_on_remaining_100 = 0.0;
    bool already_reached = false;
    bool mathematically_possible = true;
    double max_possible_grade = 100.0;
    double projected_final_with_overrides = 0.0;
};

// ============================================================================
// 2. Structured Expediente Estudiantil (5 Tabs + Live Guia de Horarios)
// ============================================================================

struct AcademicStatsSummary {
    double weighted_avg_general = 0.0;
    double weighted_avg_passed = 0.0;
    double weighted_avg_failed = 0.0;
    double passed_pct = 0.0;
    double failed_pct = 0.0;
    int hours_taken = 0;
    int credits_passed = 0;
    int credits_failed = 0;
    int credits_total = 0;
    int credits_no_grade = 0;
    int subjects_consulted = 0;
    int subjects_taken = 0;
    // Official 8 TEC course states
    int count_aprobadas = 0;
    int count_en_curso = 0;
    int count_reprobadas = 0;
    int count_retiradas = 0;
    int count_reconocidas = 0;
    int count_suficiencia = 0;
    int count_pendientes = 0;
    int count_congeladas = 0;
};

struct GradeRecordRow {
    int year = 0;
    std::string state;
    double grade = 0.0;
    std::string grade_text;
    std::string course_code;
    std::string course_name;
    std::string modality;
    std::string period;
    int credits = 0;
};

struct WeightedAveragePeriodRow {
    int year = 0;
    std::string modality;
    std::string period;
    double weighted_avg = 0.0;
};

struct MatriculaAppointment {
    std::string date;
    std::string period;
    std::string modality;
    std::string time;
};

struct WeeklyScheduleCell {
    int day_index_0_mon = 0;  // 0=Lun .. 6=Dom
    std::string time_range;
    int start_minute = 0;
    int end_minute = 0;
    std::string course_code;
    std::string course_name;
    std::string group;
    std::string room;
    std::string building;
    std::string professor;
    int credits = 0;
    std::string raw_text;
};

struct PersonalAcademicInfo {
    std::string full_name;
    std::string campus;
    std::string career;
    std::string study_plan;
    std::string academic_status;
    std::string entry_year;
    std::string modality;
    std::string photo_url;
    std::vector<std::pair<std::string, std::string>> fields;
};

struct DarCourseSyncEntry {
    std::string course_name;
    std::string status_text;
    std::string state_code;
};

struct FeriaNegociosRow {
    std::string name;
    std::string start_date;
    std::string end_date;
    std::string bases_url;
};

// Exact 11 columns observed on POST /tda-expediente-estudiantil/ajax/tabla_guia_horario
struct GuiaHorarioRow {
    std::string codigo;
    std::string materia;
    std::string grupo;
    int creditos = 0;
    std::string horario_raw;
    std::string aula;
    std::string profesor;
    int cupo = -1;
    std::string tipo_materia;
    std::string tipo_grupo;
    int reservados = 0;
};

struct GraduateAttributeRow {
    std::string attribute_name;
    std::string description;
    std::string courses;
    double evaluated_pct = 0.0;
    double initial_pct = 0.0;
    double medium_pct = 0.0;
    double advanced_pct = 0.0;
    double total_pct = 0.0;
};

struct StructuredExpediente {
    AcademicStatsSummary stats;
    std::vector<GradeRecordRow> grades;
    std::vector<WeightedAveragePeriodRow> weighted_averages;
    std::vector<MatriculaAppointment> citas_matricula;
    std::vector<WeeklyScheduleCell> current_schedule;
    std::vector<GraduateAttributeRow> graduate_attributes;
};

// ============================================================================
// 3. Course Portada / Overview, Unified Calendar, Plagio, SMS, GAAP, UM, Foros
// ============================================================================

struct CourseTabInfo {
    int page_num = 0;
    std::string label;
    std::string url;
    bool is_custom = false;
};

struct CourseExternalLink {
    std::string label;
    std::string url;
    std::string category;  // "Programa", "Cronograma", "SharePoint", "YouTube", "Teams", "Zoom", "PDF", "Enlace"
};

struct CourseTeacherCard {
    std::string name;
    std::string email;
    std::string office;
    std::string phone;
    std::string teams_handle;
    std::vector<std::string> consultation_hours;
};

struct SpaceBanner {
    std::string image_url;
    std::string alt_text;
    std::string link_url;
};

struct SpacePortletBlock {
    std::string title;
    std::string body_preview;
    std::vector<CourseExternalLink> links;
    std::vector<SpaceBanner> images;           // Infographics & visual cards embedded inside weekly/study portlets
    std::vector<CourseTabInfo> linked_subpages; // Internal ?page_num=N subpages linked inside this portlet (e.g. S1..S16)
    std::string module_kind;                   // "weekly" | "exam_notice" | "documents" | "news" | "info"
};

struct SpacePageSnapshot {
    int page_num = 0;
    std::string tab_label;
    std::string page_url;
    std::vector<SpaceBanner> banners;
    std::vector<SpacePortletBlock> portlets;
    std::vector<CourseExternalLink> links;
    std::string plain_excerpt;
};

struct NewsArticleDetail {
    std::string item_id;
    std::string url;
    std::string title;
    std::string pub_date;
    std::string author;
    std::string body_text;
};

struct CourseOverviewSnapshot {
    std::string community_path;
    std::string title;
    bool has_checkpoint_icon = false;
    std::string description;
    std::vector<std::string> objectives;
    CourseTeacherCard teacher;
    std::vector<CourseTabInfo> tabs;
    std::vector<std::string> portlet_titles;
    std::vector<CourseExternalLink> official_docs;
    std::vector<CourseExternalLink> external_links;
    std::vector<std::string> weekly_sections;
    std::vector<std::string> subgroups;
    std::vector<SpaceBanner> banners;
    std::vector<SpacePortletBlock> portlets;
    std::vector<SpacePageSnapshot> custom_pages;
};

struct CalendarEventEntry {
    std::string uid;
    std::string date_iso;      // YYYY-MM-DD
    std::string start_time;    // HH:MM
    std::string end_time;      // HH:MM
    std::string title;
    std::string calendar_name;
    std::string description;
    std::string source_kind;   // "calendar", "evaluation", "matricula", "class"
};

struct PlagiarismDocRecord {
    std::string doc_uid;
    std::string file_name;
    std::string status_code;
    std::string status_text;
    double similarity_pct = -1.0;
    std::string visibility;
    std::string source_section;
    std::string report_url;
};

struct PlagiarismTreeSnapshot {
    std::string my_files_name;
    bool is_student = true;
    std::string my_files_folder_id;
    std::string periods_raw;
    std::vector<PlagiarismDocRecord> documents;
};

struct SmsMessageRecord {
    std::string content;
    std::string transaction_date;
    std::string community;
    std::string sent_by;
};

struct NotificationSubscriptionItem {
    std::string request_id;
    std::string community;
    std::string notice_type;
    std::string element;
    std::string frequency;
};

struct GaapActivityRecord {
    std::string activity_id;
    std::string title;
    std::string activity_type;
    std::string date_text;
    double score = -1.0;
    std::string status;
};

struct UserModelDimension {
    std::string dimension_key;
    std::string title;
    std::vector<std::pair<std::string, std::string>> metrics;
};

struct UserModelSnapshot {
    std::vector<UserModelDimension> dimensions;
    std::string consent_status;
};

struct ForumThreadSummary {
    std::string message_id;
    std::string subject;
    std::string author;
    std::string posted_date;
    int replies = 0;
    std::string url;
};

struct SchoolCatalogEntry {
    std::string dept_code;
    std::string name;
    std::string url;
};

struct FileVersionRecord {
    std::string title;
    std::string live_revision;
    std::string author;
    std::string mime_type;
    std::string type;
    std::string pretty_type;
    std::string last_modified_ansi;
    uint64_t content_size = 0;
    std::string description;
    std::string fs_type;
    int fs_sort_relevance = 0;
    bool read_p = true;
};

struct EvalSubmissionRevision {
    std::string revision_id;
    std::string file_name;
    std::string download_url;
    std::string delivered_date;
    std::string delivered_time;
    bool is_live_revision = false;
};

struct PlagiarismMisDocItem {
    std::string item_id;
    std::string doc_id;
    std::string file_name;
    std::string date;
    std::string time;
    std::string status;
    std::string visibility;
    double similarity_pct = -1.0;
    std::string package_from;
    std::string community_id;
};

struct ForumMessagePost {
    std::string message_id;
    std::string subject;
    std::string author;
    std::string posted_date;
    std::string body_text;
};

struct PerformanceEvalPending {
    std::string evaluation_id;
    std::string course_name;
    std::string teacher_name;
    std::string form_name;
    std::string status;
    std::string deadline;
    std::string url;
};

struct CurriculumCriticalPath {
    int total_courses = 0;
    int approved_courses = 0;
    int remaining_courses = 0;
    int completed_credits = 0;
    int remaining_credits = 0;
    int min_semesters_remaining = 0;
    std::vector<std::string> critical_path_codes;
    std::vector<std::string> unlocked_next_codes;
};

// ============================================================================
// Pure Deterministic Parsers & Calculators (100% Offline Testable)
// ============================================================================

std::string ConvertPeriodKeyToExpedienteFormat(std::string_view period);
std::string ConvertExpedientePeriodToDotlrnFormat(std::string_view period);
std::string ConvertPeriodKeyToTablaHorarioFormat(std::string_view period);
std::vector<std::pair<std::string, std::string>> ParseExpedienteComboJsonOrHtml(std::string_view payload);

double ComputeEvaluationGrade100(const DetailedEvaluationItem& item);
double ComputeCourseAccumulatedGrade100(const DetailedEvaluationsReport& report);

DetailedEvaluationsReport ParseDetailedEvaluationsHtml(std::string_view html,
                                                       std::string_view community_slug = "");

RubricBreakdown ParseRubricHtml(std::string_view html, std::string_view rubric_id = "");

GradeWhatIfScenario ComputeGradeWhatIf(
    const DetailedEvaluationsReport& report,
    double target_grade = 70.0,
    const std::map<std::string, double>& hypothetical_raw_100_by_assign_id = {});

AcademicStatsSummary ParseAcademicStatsHtml(std::string_view html);
std::vector<GradeRecordRow> ParseDetalleNotasHtml(std::string_view html);
std::vector<WeightedAveragePeriodRow> ParsePromedioPonderadoTableHtml(std::string_view html);
std::vector<MatriculaAppointment> ParseCitasMatriculaHtml(std::string_view html);
std::vector<WeeklyScheduleCell> ParseTablaHorarioHtml(std::string_view html);
std::vector<GuiaHorarioRow> ParseTablaGuiaHorarioHtml(std::string_view html);
std::vector<GraduateAttributeRow> ParseGraduateAttributesHtml(std::string_view html);

// Converts live Guia de Horarios rows (11 TEC columns) into canonical JSON accepted
// directly by academic::horarios_tec::ParseCatalogJson().
std::string GuiaHorarioRowsToHorariosTecJson(const std::vector<GuiaHorarioRow>& rows,
                                             std::string_view title = "Guía de Horarios TEC",
                                             std::string_view term = "",
                                             std::string_view campus = "");

// ============================================================================
// 2b. Public Portal Guia de Horarios (https://tec-appsext.itcr.ac.cr/guiahorarios/)
// ============================================================================

struct PublicGuiaEscuelaInfo {
    std::string ide_depto;
    std::string dsc_depto;
};

struct PublicGuiaModalidadInfo {
    std::string ide_modalidad;
    std::string nombre;
    int cant_periodos = 1;
};

struct PublicGuiaSessionRow {
    std::string dsc_sede;
    std::string ide_materia;
    std::string dsc_materia;
    std::string ide_grupo;
    std::string dsc_depto;
    int can_creditos = 0;
    int horas = 0;
    std::string nom_dia;
    std::string ide_modalidad;
    std::string ide_per_mod;
    int num_ano = 0;
    std::string dsc_modalidad;
    std::string tipo_curso;
    std::string hinicio;
    std::string hfin;
    std::string nom_profesor;
    std::string reserva_sede;
    std::string reserva_depto;
    std::string reserva_plan;
    std::string ide_est_est;
    std::string dsc_plan;
    std::string num_nivel;
};

struct PublicGuiaFilterOptions {
    std::string sede_filter;
    std::string modalidad_filter;
    std::string periodo_filter;
    std::vector<std::string> course_codes_filter;
    bool exclude_reserved = false;
};

// ============================================================================
// 5. Change Detection Radar, Motor de Juegos / GAAP & Student Action Results
// ============================================================================

enum class TecChangeEventKind {
    NewFile = 0,
    ModifiedFile = 1,
    NewFolder = 2,
    GradePublished = 3,
    GradeChanged = 4,
    RubricGraded = 5,
    NewTeacherComment = 6,
    NewEvaluation = 7,
    NewNews = 8,
    NewWeeklyContent = 9
};

struct TecChangeEvent {
    std::string id;                // Deterministic dedup key (e.g. "file_mod:<oid>:<rev/ts>")
    TecChangeEventKind kind = TecChangeEventKind::NewFile;
    std::string detected_at_utc;   // YYYY-MM-DDTHH:MM:SSZ
    std::string space_url;
    std::string space_name;
    std::string title;             // Concise headline (e.g. "Evaluación calificada: II Fase (65.0/100)")
    std::string detail;            // Folder path, timestamp delta, rubric score or news excerpt
    std::string target_id;         // object_id, assign_id, news_url or page_num
    bool unread = true;
};

struct MotorJuegosUserInfo {
    int64_t user_id = 0;
    std::string first_names;
    std::string last_names;
    bool is_professor = false;
};

struct MotorJuegosActivity {
    std::string id;
    std::string title;
    std::string community_id;
    std::string status;
    std::string due_date;
    double score = -1.0;
    bool grade_published = false;
    std::string url;
};

struct StudentMutationResult {
    bool ok = false;
    int http_status = 0;
    std::string message;
    std::string raw_response;
    std::string created_id;
};

Result<std::vector<PublicGuiaEscuelaInfo>> ParsePublicGuiaEscuelasResponse(std::string_view raw_json);
Result<std::vector<PublicGuiaModalidadInfo>> ParsePublicGuiaModalidadesResponse(std::string_view raw_json);
Result<std::vector<PublicGuiaSessionRow>> ParsePublicGuiaRowsResponse(std::string_view raw_json);

std::vector<PublicGuiaSessionRow> FilterPublicGuiaRows(
    const std::vector<PublicGuiaSessionRow>& rows,
    const PublicGuiaFilterOptions& filter);

std::vector<GuiaHorarioRow> PublicGuiaRowsToGuiaHorarioRows(
    const std::vector<PublicGuiaSessionRow>& rows,
    const PublicGuiaFilterOptions& filter = {});

std::string PublicGuiaRowsToHorariosTecJson(
    const std::vector<PublicGuiaSessionRow>& rows,
    const PublicGuiaFilterOptions& filter = {},
    std::string_view title = "Guía de Horarios Pública TEC");

CourseOverviewSnapshot ParseCourseOverviewHtml(std::string_view html,
                                               std::string_view community_path = "",
                                               std::string_view base_url = "https://tecdigital.tec.ac.cr");
SpacePageSnapshot ParseSpacePageHtml(std::string_view html,
                                     int page_num,
                                     std::string_view tab_label = "",
                                     std::string_view page_url = "",
                                     std::string_view base_url = "https://tecdigital.tec.ac.cr");
NewsArticleDetail ParseNewsArticleHtml(std::string_view html,
                                       std::string_view url = "");
std::vector<CourseExternalLink> ExtractCustomTabLinksHtml(
    std::string_view html,
    std::string_view tab_label = "",
    std::string_view base_url = "https://tecdigital.tec.ac.cr");

std::vector<CalendarEventEntry> ParseCalendarListHtml(std::string_view html);

std::vector<CalendarEventEntry> BuildUnifiedCalendarEvents(
    const std::vector<CalendarEventEntry>& portal_events,
    const std::vector<DetailedEvaluationsReport>& evals,
    const std::vector<MatriculaAppointment>& citas);

std::string ExportCalendarEventsToIcs(const std::vector<CalendarEventEntry>& events,
                                      std::string_view prod_id = "-//CuadernU//TEC Digital Calendar//ES");

PlagiarismTreeSnapshot ParsePlagiarismTreeJson(std::string_view json_text);
std::vector<PlagiarismMisDocItem> ParsePlagiarismMisDocsHtml(std::string_view html);
std::vector<FileVersionRecord> ParseFileVersionsJson(std::string_view json_text);
std::vector<EvalSubmissionRevision> ParseEvalHistoryHtml(
    std::string_view html,
    std::string_view base_url = "https://tecdigital.tec.ac.cr");
std::optional<EvalSubmissionRevision> ParseEvalLiveRevisionHtml(
    std::string_view html,
    std::string_view base_url = "https://tecdigital.tec.ac.cr");
std::vector<SmsMessageRecord> ParseSmsTableHtml(std::string_view html);
std::vector<NotificationSubscriptionItem> ParseNotificationSubscriptionsHtml(std::string_view html);
std::vector<ForumThreadSummary> ParseForumsHtml(std::string_view html,
                                                std::string_view base_url = "https://tecdigital.tec.ac.cr");
std::vector<ForumMessagePost> ParseForumThreadMessagesHtml(std::string_view html);
std::vector<PerformanceEvalPending> ParsePerformanceEvaluationsJson(std::string_view json_text);
std::vector<SchoolCatalogEntry> ParseSchoolCatalogHtml(std::string_view html,
                                                       std::string_view base_url = "https://tecdigital.tec.ac.cr");
std::vector<GaapActivityRecord> ParseGaapHistoryHtmlOrJson(std::string_view payload);
UserModelSnapshot ParseUserModelInfoJson(std::string_view json_text);
UserModelDimension ParseUserModelSectionHtml(std::string_view dimension_key,
                                             std::string_view title,
                                             std::string_view html);
PersonalAcademicInfo ParsePersonalAcademicInfoHtml(std::string_view html);
std::vector<DarCourseSyncEntry> ParseDarActualizarCursosHtml(std::string_view html);
std::vector<FeriaNegociosRow> ParseFeriaTableHtml(
    std::string_view html,
    std::string_view base_url = "https://tecdigital.tec.ac.cr");

}  // namespace academic::tec_digital
