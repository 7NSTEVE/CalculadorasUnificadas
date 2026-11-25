#pragma once

#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/types.hpp"

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

struct CommunityInfo {
    std::string community_id;
    std::string slug;
    std::string name;
    std::string type;        // "CLASE", "CATEDRA", "COMUNIDAD"
    std::string department;  // e.g. "E", "ECE", "CA", or "Catedra"
    std::string period;      // e.g. "S-2-2026" or "Permanente"
    std::string url;
    std::string root_folder_id;
};

struct SedeInfo {
    std::string key;
    std::string name;
};

struct CarreraInfo {
    std::string key;
    std::string name;
};

struct PlanInfo {
    std::string key;  // coerced from JSON number or string
    std::string name;
};

struct ColorCodeInfo {
    std::string label;
    std::string css_class;
};

struct ColorCodeCatalog {
    std::string title;
    std::vector<ColorCodeInfo> color_codes;
    std::string additional_text;
};

struct CurriculumCourseNode {
    std::string id_course;
    std::string name;
    std::string truncated_name;  // parsed from "trucatedName" (fallback "truncatedName")
    int credits = 0;
    int hours = 0;
    std::vector<std::string> requirements;
    std::vector<std::string> co_requirements;
    std::vector<std::string> required_by;
    std::vector<std::string> equivalent;
    std::string t_equiv;
};

struct CurriculumLevel {
    std::string id;
    std::vector<CurriculumCourseNode> courses;
};

struct CurriculumGraph {
    std::string dsc_curriculum;
    int64_t id_curriculum = 0;
    std::string modality;
    std::string academic_degree;
    int first_level = 0;
    std::vector<CurriculumLevel> levels;
};

struct CourseGradeAttempt {
    int year = 0;
    int period = 0;
    std::string modality;
    std::string name_equivalent;
    std::string id_equivalent;
    int state = 0;
    double grade = 0.0;
    int group = 0;
    std::vector<std::string> schedule;
};

struct StudentCourseRecord {
    std::string id_course;
    std::string best_status;
    std::vector<CourseGradeAttempt> grades_record;
};

struct StudentCurriculumProfile {
    std::vector<StudentCourseRecord> grades;
};

struct StudentCareerRecord {
    std::string nombre;
    std::string sede;
    std::string carrera;
    int64_t plan = 0;
};

struct ExpedienteSummary {
    int package_id = 0;
    std::map<std::string, std::string> i18n_labels;
    std::map<std::string, std::string> validation_labels;
    std::vector<SedeInfo> sedes_combo;
    std::vector<CarreraInfo> carreras_combo;
    std::vector<PlanInfo> periodos_combo;
    std::string detalle_notas_html;
    std::string detalle_promedio_html;
    std::string tabla_promedio_html;
    std::string detalle_historial_html;
    std::string citas_matricula_html;
    std::string guia_horario_html;
    std::string weighted_average_text;
    std::string approval_pct_text;
    StructuredExpediente structured;
};

struct PrematriculaCourseEntry {
    std::string course_code;
    int priority = 0;
    std::string course_elective;
    std::string name_elective;
};

struct PrematriculaRequestEntry {
    std::string course_code;
    std::string observation;
};

struct PrematriculaSnapshot {
    int max_credits = 0;
    std::string rn_student;
    std::vector<PrematriculaCourseEntry> courses;
    std::vector<PrematriculaRequestEntry> requests;
    std::vector<std::string> disabled_courses;
    int validated_electives_count = 0;
    std::map<std::string, std::string> i18n_labels;
};

struct PerformanceFormItem {
    int64_t id = 0;
    std::string description;
    int order = 0;
    std::string type;
};

struct PerformanceFormSection {
    int64_t id = 0;
    std::string name;
    std::string description;
    int order = 0;
    std::string type;
    std::vector<PerformanceFormItem> items;
};

struct PerformanceForm {
    int64_t id = 0;
    std::string name;
    std::string instructions;
    std::vector<PerformanceFormSection> sections;
};

struct CourseEvaluationItem {
    std::string assign_id;
    std::string name;
    std::string grade_text;
};

struct CourseEvaluationCategory {
    std::string category_id;
    std::string name;
    std::string weight_text;
    std::vector<CourseEvaluationItem> items;
};

struct CourseEvaluationsSnapshot {
    std::string community_slug;
    std::string final_grade_text;
    std::vector<CourseEvaluationCategory> categories;
    std::vector<PerformanceForm> performance_forms;
    std::string performance_evaluations_raw_json;
    std::map<std::string, std::string> i18n_labels;
};

struct FeriaLocation {
    std::string id;
    std::string name;
};

struct FeriaCategory {
    int64_t id = 0;
    std::string name;
};

struct FeriaIds {
    int64_t user = 0;
    int64_t community = 0;
    int64_t pack = 0;
};

struct FeriaNegociosSnapshot {
    std::map<std::string, std::string> i18n_labels;
    std::string info_estudiante_raw_json;
    FeriaLocation student_location;
    std::string active_fairs_raw_json;
    std::vector<FeriaCategory> categories;
    FeriaIds ids;
};

struct NotificationItem {
    std::string notification_id;
    int64_t origin = 0;
    std::string creation_date;
    std::string expiration_date;
    int category_id = 0;
    std::string text;
    std::string url;
    std::string title;
    std::string state;  // "U" unread, "R" read
};

struct NotificationsFeed {
    int unread_quantity = 0;
    bool category_sort = false;
    std::vector<NotificationItem> notifications;
};

struct RssNewsItem {
    std::string title;
    std::string link;
    std::string guid;
    std::string description;
    std::string pub_date;
};

struct CommunityMember {
    std::string user_id;
    std::string first_name;
    std::string last_name;
    std::string full_name;
    std::string email;
    std::string role;
};

struct PerformanceEvaluationStatus {
    std::vector<PerformanceForm> forms;
    std::vector<PerformanceEvalPending> pending_evaluations;
};

struct TdaTdsCatalogOptions {
    bool fetch_curriculum = true;
    bool fetch_expediente = true;
    bool fetch_notifications = true;
    bool fetch_evaluations = true;
    bool fetch_prematricula = false;
    bool fetch_gestor_inscripcion = false;
    bool fetch_inclusion_key = true;
    bool fetch_feria_negocios = false;
    bool fetch_members = false;
    bool fetch_news_rss = true;
    std::string carne;
    std::string id_sede;
    std::string id_depto;
    std::string id_plan;
    std::string pre_enrollment_id;
    std::string curriculum_id_current;
    std::string list_electives;
    std::string convocatory_id;
    std::vector<std::string> community_paths;
};

struct TdaTdsCatalog {
    int64_t server_epoch_ms = 0;
    std::string gaap_server;
    std::string notification_request_id;
    int curriculum_app_code = 0;
    bool can_modify_curriculum = false;
    std::vector<SedeInfo> sedes;
    std::vector<CarreraInfo> carreras;
    std::vector<PlanInfo> planes;
    ColorCodeCatalog color_codes;
    std::optional<StudentCareerRecord> student_career;
    std::optional<CurriculumGraph> curriculum_graph;
    std::optional<StudentCurriculumProfile> student_profile;
    std::optional<ExpedienteSummary> expediente;
    std::optional<PrematriculaSnapshot> prematricula;
    std::optional<NotificationsFeed> notifications;
    std::optional<FeriaNegociosSnapshot> feria_negocios;
    std::string gestor_inscripcion_selector_html;
    std::string gestor_inscripcion_events_json;
    std::string inclusion_transaction_key;
    std::vector<CourseEvaluationsSnapshot> evaluations;
    std::map<std::string, std::vector<RssNewsItem>> news_rss_by_community;
    std::map<std::string, std::vector<CommunityMember>> members_by_community;
    std::vector<std::string> warnings;
};

class TdaTdsClient {
public:
    TdaTdsClient(PolicyEnforcingTransport& transport,
                 AuthClient& auth,
                 std::string base_url = "https://tecdigital.tec.ac.cr",
                 SanitizeOptions sanitize_opts = {});

    void SetSanitizeOptions(SanitizeOptions sanitize_opts);
    Sanitizer SnapshotSanitizer() const;

    // Session-RAM only student identifier resolution (never written to disk or logs)
    std::string ResolveStudentCarne(std::string_view explicit_carne = "");
    void ClearCachedCarne();

    // Theme & Session-adjacent / UM / Consent / SMS / Notifications request_id
    Result<int64_t> GetServerDateMs();
    Result<std::string> GetGaapServer();
    Result<std::map<std::string, std::string>> GetUmInternationalization();
    Result<std::string> GetUserTermConditionJson(std::string_view module_key = "");
    Result<std::string> GetConsentStatus(std::string_view module_key);
    Result<std::map<std::string, std::string>> GetSmsInternationalization(std::string_view js = "spam");
    Result<std::string> GetTamGaapJson();
    Result<std::string> GetNotificationRequestId();

    // File-storage view/i18n helpers
    Result<std::string> GetFileStorageFolderChunkHtml(std::string_view folder_id = "");
    Result<std::map<std::string, std::string>> GetFileStorageInternationalization(
        std::string_view community_path = "");

    // Communities & Courses Discovery (/dotlrn/index, /dotlrn/my-communities, catalog)
    Result<std::vector<CommunityInfo>> FetchCoursesAndCommunities(bool include_catalog_pages = false,
                                                                  int max_catalog_pages = 5);

    // Curriculum (tds-curriculum-exp & tds-curriculum)
    Result<std::vector<SedeInfo>> GetCurriculumSedes();
    Result<std::vector<CarreraInfo>> GetCurriculumCarreras(std::string_view id_sede);
    Result<std::vector<PlanInfo>> GetCurriculumPlanes(std::string_view id_sede,
                                                      std::string_view id_depto);
    Result<ColorCodeCatalog> GetCurriculumColorCodes();
    Result<CurriculumGraph> GetCurriculumGraph(std::string_view id_plan);
    Result<StudentCurriculumProfile> GetStudentCurriculumProfile(std::string_view id_plan,
                                                                 std::string_view carne = "",
                                                                 int full_mode = 0);
    Result<StudentCareerRecord> GetStudentCareerData(std::string_view carne_oficial = "");
    Result<std::string> GetStudentOfficialName(std::string_view carne_oficial = "");
    Result<int> GetCurriculumAppCode();
    Result<bool> ValidateModificationPermissions(std::string_view carne = "");
    Result<std::string> GetCurriculumAngularTemplate(std::string_view template_name);
    Result<std::map<std::string, std::string>> GetLegacyCurriculumInternationalization(
        std::string_view js = "index");
    Result<std::string> GetLegacyCurriculumDisplayHtml(std::string_view ide_plan = "",
                                                       std::string_view depto_id = "",
                                                       std::string_view carne = "");

    // Expediente Estudiantil (tda-expediente-estudiantil)
    Result<ExpedienteSummary> GetExpedienteSummary(std::string_view carne = "");
    Result<std::vector<GraduateAttributeRow>> GetGraduateAttributes(std::string_view carne = "");
    Result<std::vector<WeeklyScheduleCell>> GetCurrentPeriodSchedule(std::string_view periodo = "",
                                                                     std::string_view carne = "");
    Result<std::vector<CarreraInfo>> GetGuiaHorarioCarreras(std::string_view sede);
    Result<std::vector<GuiaHorarioRow>> GetGuiaHorarioRows(std::string_view sede,
                                                           std::string_view carrera,
                                                           std::string_view periodo);

    // Rich Evaluations, Analytic Rubrics & Course Portada Overview
    Result<DetailedEvaluationsReport> GetDetailedCourseEvaluations(std::string_view community_path);
    Result<std::vector<EvalSubmissionRevision>> GetEvaluationSubmissionHistory(
        std::string_view community_path,
        std::string_view task_item_id,
        std::string_view student_id = "",
        std::string_view task_name = "",
        std::string_view type = "1",
        std::string_view has_grade = "0");
    Result<std::optional<EvalSubmissionRevision>> GetEvaluationLiveRevision(
        std::string_view community_path,
        std::string_view task_item_id,
        std::string_view student_id = "",
        std::string_view task_name = "",
        std::string_view task_id = "");
    Result<RubricBreakdown> GetRubricBreakdown(std::string_view rubric_id,
                                               std::string_view community_id = "",
                                               std::string_view task_item_id = "",
                                               std::string_view task_id = "");
    Result<CourseOverviewSnapshot> GetCourseOverview(std::string_view community_path);
    Result<std::vector<CourseExternalLink>> CrawlCustomCommunityTabs(
        std::string_view community_path,
        const std::vector<CourseTabInfo>& tabs,
        int max_tabs = 12,
        int polite_delay_ms = 320);

    // Unified Calendar, Control de Plagio, SMS, Subscriptions, Forums, School Catalog, User Model, GAAP
    Result<std::vector<CalendarEventEntry>> GetCalendarEvents(std::string_view date_iso = "",
                                                              std::string_view view = "list",
                                                              std::string_view community_path = "");
    Result<PlagiarismTreeSnapshot> GetPlagiarismTree();
    Result<std::vector<PlagiarismMisDocItem>> GetPlagiarismMisDocs(std::string_view folder_id);
    Result<bool> GetPlagiarismTermsAccepted();
    Result<std::string> GetPlagiarismDocInfo(std::string_view doc_uid);
    Result<std::string> GetPlagiarismReportUrl(std::string_view doc_uid);
    Result<std::vector<GaapActivityRecord>> GetGaapActivityHistory(std::string_view community_path);
    Result<std::vector<SmsMessageRecord>> GetCourseSmsMessages(std::string_view community_path);
    Result<std::vector<NotificationSubscriptionItem>> GetNotificationSubscriptions();
    Result<std::vector<ForumThreadSummary>> GetCommunityForums(std::string_view community_path);
    Result<std::vector<ForumMessagePost>> GetForumThread(std::string_view community_path,
                                                         std::string_view message_id);
    Result<std::vector<RssNewsItem>> GetNewsArchive(std::string_view community_path);
    Result<std::vector<SchoolCatalogEntry>> GetSchoolCatalog();
    Result<UserModelSnapshot> GetUserModelSnapshot();

    static CurriculumCriticalPath ComputeCurriculumCriticalPath(
        const CurriculumGraph& graph,
        const StudentCurriculumProfile* profile);

    // Prematricula (strictly read-only tipo=1,2)
    Result<PrematriculaSnapshot> GetPrematriculaSnapshot(std::string_view pre_enrollment_id,
                                                         std::string_view curriculum_id,
                                                         std::string_view curriculum_id_current = "",
                                                         std::string_view list_electives = "");

    // Gestor Inscripcion (read-only selector & schedule)
    Result<std::string> GetGestorInscripcionSelectorHtml();
    Result<std::string> GetGestorInscripcionEventsJson(std::string_view convocatory_id = "");

    // Inclusion Manager
    Result<std::string> GetInclusionTransactionKey();

    // Evaluations & Performance Evaluation
    Result<CourseEvaluationsSnapshot> GetCourseEvaluations(std::string_view community_path);
    Result<PerformanceEvaluationStatus> GetPerformanceEvaluationStatus();

    // Feria de Negocios (read-only metadata)
    Result<FeriaNegociosSnapshot> GetFeriaNegociosSnapshot(std::string_view location_id = "");
    Result<FeriaLocation> GetFeriaStudentLocation();
    Result<std::vector<FeriaCategory>> GetFeriaCategories();
    Result<FeriaIds> GetFeriaIds();

    // Notifications (read-only)
    Result<int> GetUnreadNotificationCount();
    Result<NotificationsFeed> GetUserNotifications();

    // News RSS & Members
    Result<std::vector<RssNewsItem>> GetCommunityNewsRss(std::string_view community_path);
    Result<std::vector<CommunityMember>> GetCommunityMembers(std::string_view community_path,
                                                             std::string_view community_id = "",
                                                             int length = 1000);
    Result<std::string> GetCommunityMembersCsv(std::string_view community_path);

    // Aggregated catalog fetcher
    Result<TdaTdsCatalog> FetchCatalog(const TdaTdsCatalogOptions& opts = {});

    // Pure parsers (exposed for deterministic offline testing)
    static Result<std::vector<SedeInfo>> ParseSedesJson(std::string_view json_text);
    static Result<std::vector<CarreraInfo>> ParseCarrerasJson(std::string_view json_text);
    static Result<std::vector<PlanInfo>> ParsePlanesJson(std::string_view json_text);
    static Result<ColorCodeCatalog> ParseColorCodesJson(std::string_view json_text);
    static Result<CurriculumGraph> ParseCurriculumGraphJson(std::string_view json_text);
    static Result<StudentCurriculumProfile> ParseStudentProfileJson(std::string_view json_text);
    static Result<StudentCareerRecord> ParseStudentCareerJson(std::string_view json_text);
    static Result<NotificationsFeed> ParseNotificationsJson(std::string_view json_text);
    static Result<std::vector<PerformanceForm>> ParsePerformanceFormsJson(std::string_view json_text);
    static Result<std::vector<RssNewsItem>> ParseRssXml(std::string_view xml_text);
    static Result<std::vector<CommunityMember>> ParseMembersJson(std::string_view json_text);
    static std::vector<CommunityInfo> ParseCommunitiesHtml(std::string_view html_text,
                                                           std::string_view base_url);
    static std::vector<CourseEvaluationCategory> ParseEvaluationsHtml(
        std::string_view html_text,
        std::string* out_final_grade = nullptr);
    static std::vector<PrematriculaCourseEntry> ParsePrematriculaTipo1(
        std::string_view body,
        int* out_max_credits = nullptr,
        std::string* out_rn = nullptr);
    static std::vector<PrematriculaRequestEntry> ParsePrematriculaTipo2(std::string_view body);
    static std::vector<std::string> ParseDisabledCourses(std::string_view body);

private:
    Result<std::string> FetchText(std::string_view method,
                                  std::string_view path_or_url,
                                  std::string_view body = "",
                                  std::string_view content_type = "",
                                  std::string_view accept = "application/json, text/plain, */*");

    PolicyEnforcingTransport& transport_;
    AuthClient& auth_;
    std::string base_url_;
    mutable std::mutex sanitizer_mutex_;
    Sanitizer sanitizer_;
    mutable std::mutex carne_mutex_;
    std::string cached_carne_;
};

}  // namespace academic::tec_digital
