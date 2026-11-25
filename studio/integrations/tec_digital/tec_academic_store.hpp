#pragma once
// ============================================================================
// doc_studio/integrations/tec_digital/tec_academic_store.hpp
// Decoupled, offline-first, strictly read-only TEC Digital orchestration,
// local cache store (2 GB default budget), HorariosTEC live bridge,
// CuadernU Canvas 1-click builders, and Gemini Notebook batch bridge.
// Zero background polling timers -> 0% GPU and 0% CPU when idle.
// ============================================================================

#include "academic/horarios_tec/horarios_tec.hpp"
#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/service.hpp"
#include "../../core/document_model.hpp"

#if defined(DOC_STUDIO_HAS_GEMINI_NOTEBOOK)
#include "../gemini_notebook/gemini_notebook_types.hpp"
#endif

#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace DocStudio::TecDigital {

constexpr std::uint64_t kDefaultOfflineBudgetBytes = 2ull * 1024ull * 1024ull * 1024ull; // 2 GiB

struct TecAcademicSnapshot {
    std::string syncedAtUtc;
    bool sessionAuthenticated = false;
    std::string inclusionServiceStatus = "unknown";  // "active" | "service_unavailable" | "unknown"

    std::vector<academic::tec_digital::CommunityInfo> communities;
    std::map<std::string, academic::tec_digital::CourseOverviewSnapshot> overviewsByPath;
    std::map<std::string, academic::tec_digital::DetailedEvaluationsReport> evaluationsByPath;
    std::map<std::string, academic::tec_digital::FolderWalkResult> foldersByPath;
    std::map<std::string, std::vector<academic::tec_digital::RssNewsItem>> newsByPath;
    std::map<std::string, std::vector<academic::tec_digital::SmsMessageRecord>> smsByPath;
    std::map<std::string, std::vector<academic::tec_digital::CommunityMember>> membersByPath;
    std::map<std::string, std::vector<academic::tec_digital::ForumThreadSummary>> forumsByPath;
    std::map<std::string, std::vector<academic::tec_digital::GaapActivityRecord>> gaapByPath;

    std::optional<academic::tec_digital::ExpedienteSummary> expediente;
    std::vector<academic::tec_digital::WeeklyScheduleCell> currentSchedule;
    std::optional<academic::tec_digital::CurriculumGraph> curriculumGraph;
    std::optional<academic::tec_digital::StudentCurriculumProfile> studentProfile;
    std::optional<academic::tec_digital::StudentCareerRecord> studentCareer;
    academic::tec_digital::ColorCodeCatalog curriculumColors;
    std::vector<academic::tec_digital::SedeInfo> curriculumSedes;
    std::vector<academic::tec_digital::CarreraInfo> curriculumCarreras;
    std::vector<academic::tec_digital::PlanInfo> curriculumPlanes;

    std::vector<academic::tec_digital::CalendarEventEntry> portalCalendarEvents;
    std::vector<academic::tec_digital::CalendarEventEntry> unifiedCalendarEvents;

    std::optional<academic::tec_digital::NotificationsFeed> notifications;
    std::vector<academic::tec_digital::NotificationSubscriptionItem> notificationSubscriptions;
    std::optional<academic::tec_digital::PlagiarismTreeSnapshot> plagiarismTree;
    std::optional<academic::tec_digital::PrematriculaSnapshot> prematricula;
    std::optional<academic::tec_digital::FeriaNegociosSnapshot> feriaNegocios;
    std::optional<academic::tec_digital::UserModelSnapshot> userModel;
    std::vector<academic::tec_digital::SchoolCatalogEntry> schoolCatalog;
    std::string gestorInscripcionSelectorHtml;
    std::string gestorInscripcionEventsJson;

    // Phase 2 live & GraphQL fields (sanitized, zero credentials/JWT/carne)
    std::vector<academic::tec_digital::GraduateAttributeRow> graduateAttributes;
    std::vector<academic::tec_digital::PendingGradCourse> pendingGraduationCourses;
    std::optional<academic::tec_digital::InclusionCampaignInfo> inclusionCampaign;
    std::vector<academic::tec_digital::InclusionApplicationRecord> inclusionApplications;
    std::optional<academic::tec_digital::ScholarshipStatusInfo> scholarshipInfo;
    std::optional<academic::tec_digital::StudentGeneralInfo> studentGeneralInfo;
    std::vector<academic::tec_digital::GraphQlActualCourse> actualPeriodCourses;
    std::vector<academic::tec_digital::PerformanceForm> performanceEvalForms;
    std::vector<academic::tec_digital::PerformanceEvalPending> pendingPerformanceEvals;
    std::map<std::string, std::vector<academic::tec_digital::FileVersionRecord>> fileVersionsByObjectId;
    std::map<std::string, std::vector<academic::tec_digital::EvalSubmissionRevision>> evalHistoryByAssignId;
    std::map<std::string, std::vector<academic::tec_digital::CourseExternalLink>> customTabResourcesByCourse;

    // Public Guia de Horarios (https://tec-appsext.itcr.ac.cr/guiahorarios/)
    std::vector<academic::tec_digital::PublicGuiaEscuelaInfo> publicGuiaEscuelas;

    std::vector<std::string> warnings;
};

struct TecSyncOptions {
    bool syncCommunities = true;
    bool syncCourseOverviews = true;
    bool syncEvaluations = true;
    bool syncExpediente = true;
    bool syncCurriculum = true;
    bool syncCalendar = true;
    bool syncNotifications = true;
    bool syncNewsRss = true;
    bool syncPlagiarismTree = true;
    bool syncSms = true;
    bool syncForums = true;
    bool syncMembers = true;
    bool syncGaap = true;
    bool syncSchoolCatalog = true;
    bool syncUserModel = true;
    bool syncInclusionProbe = true;
    bool syncGraduateAttributes = true;
    bool syncPerformanceEval = true;
    bool syncGraphQlInclusiones = true;
    bool syncCustomTabs = true;
    int maxCustomTabsPerSpace = 12;
    int maxActiveCoursesToDeepSync = 6;
    int politeDelayMs = 320;       // Respectful rate-limiting (>= 300 ms) between live requests
    std::string activePeriodHint;  // Optional filter (e.g. "S-2-2026")
};

struct LiveScheduleSolveResult {
    bool ok = false;
    std::string errorUtf8;
    std::vector<academic::tec_digital::GuiaHorarioRow> rawRows;
    academic::horarios_tec::CourseCatalog catalog;
    academic::horarios_tec::ScheduleResult solved;
    std::size_t filteredOutByPrereqsCount = 0;
};

enum class PublicGuiaMode {
    Escuela = 0,
    Verano = 1,
    Carnet = 2
};

struct PublicGuiaQuery {
    PublicGuiaMode mode = PublicGuiaMode::Escuela;
    std::string escuelaCode;
    std::string carnet;
    int year = 2026;
    academic::tec_digital::PublicGuiaFilterOptions filter;
};

struct Sm2FlashcardDraft {
    std::string frontUtf8;
    std::string backUtf8;
    std::string tagUtf8;
};

class TecAcademicStore {
public:
    using ProgressCallback = std::function<void(int pct, const std::string& statusUtf8)>;

    explicit TecAcademicStore(std::string storageRootUtf8 = "",
                              std::unique_ptr<academic::tec_digital::IHttpTransport> transportOverride = nullptr,
                              academic::tec_digital::ServiceConfig serviceConfigOverride = {});
    ~TecAcademicStore();

    TecAcademicStore(const TecAcademicStore&) = delete;
    TecAcademicStore& operator=(const TecAcademicStore&) = delete;

    static std::string DefaultStorageRootUtf8();

    const std::string& StorageRootUtf8() const noexcept { return storageRootUtf8_; }
    const std::string& OfflineFilesRootUtf8() const noexcept { return offlineFilesRootUtf8_; }
    std::uint64_t OfflineBudgetBytes() const noexcept { return offlineBudgetBytes_; }
    void SetOfflineBudgetBytes(std::uint64_t bytes) noexcept { offlineBudgetBytes_ = bytes; }

    academic::tec_digital::TecDigitalService& Service() noexcept { return *service_; }
    const academic::tec_digital::TecDigitalService& Service() const noexcept { return *service_; }

    // DPAPI-backed authentication (strictly read-only policy; LogoutLocalOnly never calls server /logout)
    academic::tec_digital::AuthResult LoginWithPassword(const std::string& emailUtf8,
                                                        const std::string& passwordUtf8,
                                                        bool persistEncryptedVault = true);
    academic::tec_digital::AuthResult TryRestoreVaultSession();
    academic::tec_digital::AuthResult LoginWithQr(const std::string& emailUtf8);
    void LogoutLocalOnly(bool deleteLocalVaultFile = false);
    bool IsAuthenticated() const noexcept { return authenticated_.load(std::memory_order_acquire); }

    // On-demand read-only sync (never runs background polling timers)
    bool SyncAll(const TecSyncOptions& opts = {},
                 ProgressCallback onProgress = nullptr,
                 std::atomic<bool>* cancelFlag = nullptr,
                 std::string* outErr = nullptr);

    bool SyncCourseFiles(const std::string& communityUrlOrPath,
                         bool downloadFiles,
                         std::string* outSummaryUtf8 = nullptr,
                         std::string* outErr = nullptr);

    bool DownloadSingleCourseFile(const std::string& communityUrlOrPath,
                                  const std::string& fileObjectId,
                                  std::string* outLocalPathUtf8 = nullptr,
                                  std::string* outErr = nullptr);

    // Live cascading Curriculum & Guia de Horarios queries
    academic::tec_digital::Result<std::vector<academic::tec_digital::CarreraInfo>>
    FetchCurriculumCarreras(const std::string& sedeKey);
    academic::tec_digital::Result<std::vector<academic::tec_digital::PlanInfo>>
    FetchCurriculumPlanes(const std::string& sedeKey, const std::string& carreraKey);
    bool LoadCurriculumPlan(const std::string& planKey, std::string* outErr = nullptr);
    academic::tec_digital::Result<std::vector<academic::tec_digital::CarreraInfo>>
    FetchGuiaHorarioCarreras(const std::string& sedeKey);

    academic::tec_digital::Result<academic::tec_digital::RubricBreakdown> FetchRubric(
        const std::string& rubricId,
        const std::string& communityId = "",
        const std::string& taskItemId = "",
        const std::string& taskId = "");

    // Phase 2 on-demand helpers
    academic::tec_digital::Result<std::vector<academic::tec_digital::FileVersionRecord>>
    FetchFileVersions(const std::string& objectId);

    academic::tec_digital::Result<std::vector<academic::tec_digital::EvalSubmissionRevision>>
    FetchEvaluationHistory(const std::string& communityUrl,
                           const std::string& assignId,
                           const std::string& taskItemId,
                           const std::string& taskId = "",
                           const std::string& taskName = "");

    academic::tec_digital::Result<std::vector<academic::tec_digital::ForumMessagePost>>
    FetchForumThread(const std::string& communityUrl, const std::string& messageId);

    academic::tec_digital::Result<std::vector<academic::tec_digital::CourseRequisitesCheck>>
    ValidateScheduleCoursesWithGraphQL(const std::vector<std::string>& courseCodes,
                                       const std::string& periodKey = "");

    // Live Guia de Horarios -> Curriculum Prerequisite Cross-check -> HorariosSolver
    LiveScheduleSolveResult FetchAndSolveLiveSchedule(
        const std::string& sedeKey,
        const std::string& carreraKey,
        const std::string& periodoKey,
        const std::string& campusLabel,
        const academic::horarios_tec::SolverPreferences& prefs,
        bool filterEligibleByCurriculum = false,
        bool includeUnifiedCalendarBusySlots = false);

    // Public Guia de Horarios Portal (https://tec-appsext.itcr.ac.cr/guiahorarios/)
    academic::tec_digital::Result<std::vector<academic::tec_digital::PublicGuiaEscuelaInfo>>
    FetchPublicGuiaEscuelas();
    academic::tec_digital::Result<std::vector<academic::tec_digital::PublicGuiaModalidadInfo>>
    FetchPublicGuiaModalidades();
    LiveScheduleSolveResult FetchAndSolvePublicGuiaSchedule(
        const PublicGuiaQuery& query,
        const academic::horarios_tec::SolverPreferences& prefs,
        bool filterEligibleByCurriculum = false,
        bool includeUnifiedCalendarBusySlots = false,
        const academic::horarios_tec::CourseCatalog* mergeIntoCatalog = nullptr);

    // Offline cache persistence & budget hygiene
    bool SaveSnapshotToDisk(std::string* outErr = nullptr) const;
    bool LoadSnapshotFromDisk(std::string* outErr = nullptr);
    std::uint64_t ComputeOfflineCacheBytes() const;
    std::size_t PruneOfflineCacheOlderThanBytes(std::uint64_t targetMaxBytes);

    // Offline FTS5 search across downloaded course materials
    std::vector<academic::tec_digital::MaterialSearchHit> SearchOfflineMaterials(
        const std::string& queryUtf8,
        const std::string& courseFilter = "",
        std::size_t maxHits = 40);

    TecAcademicSnapshot Snapshot() const;
    void SetSnapshotForTesting(TecAcademicSnapshot snap);

    // 1-Click CuadernU Canvas & SM-2 Builders
    static std::size_t InsertCourseOverviewToCanvas(
        DocStudio::DocumentModel& doc,
        const academic::tec_digital::CommunityInfo& course,
        const academic::tec_digital::CourseOverviewSnapshot* overview,
        const academic::tec_digital::DetailedEvaluationsReport* evals);

    static std::size_t InsertScheduleToCanvas(
        DocStudio::DocumentModel& doc,
        const academic::horarios_tec::RankedSchedule& schedule,
        const std::string& titleUtf8 = "Horario Semestral TEC");

    static std::string ExportRankedScheduleToIcs(
        const academic::horarios_tec::RankedSchedule& schedule,
        const std::string& titleUtf8 = "Horario Semestral TEC",
        const std::string& mondayAnchorIso = "2026-07-20");

    static bool ExportRankedScheduleToPdfFile(
        const academic::horarios_tec::RankedSchedule& schedule,
        const std::wstring& outPdfPathW,
        const std::string& titleUtf8 = "Horario Semestral TEC",
        std::string* outErr = nullptr);

    static bool SaveCourseNotebookCnu(
        const academic::tec_digital::CommunityInfo& course,
        const academic::tec_digital::CourseOverviewSnapshot* overview,
        const academic::tec_digital::DetailedEvaluationsReport* evals,
        const std::wstring& outCnuPathW,
        std::string* outErr = nullptr);

    static std::size_t InsertCurriculumSummaryToCanvas(
        DocStudio::DocumentModel& doc,
        const academic::tec_digital::CurriculumGraph& graph,
        const academic::tec_digital::StudentCurriculumProfile* profile);

    static bool ImportDownloadedMaterialToCanvas(
        DocStudio::DocumentModel& doc,
        const std::string& localFilePathUtf8,
        const std::string& displayNameUtf8,
        const std::string& courseLabelUtf8,
        const std::string& sourceUrlUtf8 = "");

    static std::vector<Sm2FlashcardDraft> GenerateSm2FlashcardsFromCourse(
        const academic::tec_digital::CommunityInfo& course,
        const academic::tec_digital::CourseOverviewSnapshot* overview,
        const academic::tec_digital::DetailedEvaluationsReport* evals);

#if defined(DOC_STUDIO_HAS_GEMINI_NOTEBOOK)
    static std::vector<DocStudio::GeminiNotebook::AcademicMaterialCandidate>
    BuildGeminiNotebookCandidatesForCourse(
        const academic::tec_digital::CommunityInfo& course,
        const academic::tec_digital::CourseOverviewSnapshot* overview,
        const std::vector<academic::tec_digital::FileRecord>& downloadedFiles,
        const std::string& offlineRootUtf8);
#endif

private:
    std::string storageRootUtf8_;
    std::string offlineFilesRootUtf8_;
    std::string snapshotCachePathUtf8_;
    std::string vaultPathUtf8_;
    std::uint64_t offlineBudgetBytes_ = kDefaultOfflineBudgetBytes;

    std::unique_ptr<academic::tec_digital::TecDigitalService> service_;
    std::atomic<bool> authenticated_{false};

    mutable std::mutex snapMutex_;
    TecAcademicSnapshot snapshot_;
};

}  // namespace DocStudio::TecDigital
