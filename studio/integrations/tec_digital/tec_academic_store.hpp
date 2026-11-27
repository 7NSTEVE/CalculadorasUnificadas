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
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
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
    std::map<std::string, std::vector<academic::tec_digital::RssNewsItem>> newsArchiveByPath;
    std::map<std::string, std::vector<academic::tec_digital::CalendarEventEntry>> courseCalendarsByPath;
    std::map<std::string, std::vector<academic::tec_digital::SmsMessageRecord>> smsByPath;
    std::map<std::string, std::vector<academic::tec_digital::CommunityMember>> membersByPath;
    std::map<std::string, std::vector<academic::tec_digital::ForumThreadSummary>> forumsByPath;
    std::map<std::string, std::vector<academic::tec_digital::GaapActivityRecord>> gaapByPath;
    std::map<std::string, academic::tec_digital::RubricBreakdown> rubricsByKey;

    std::optional<academic::tec_digital::ExpedienteSummary> expediente;
    std::optional<academic::tec_digital::PersonalAcademicInfo> personalAcademicInfo;
    std::vector<academic::tec_digital::DarCourseSyncEntry> darSyncPreview;
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
    std::map<std::string, std::vector<academic::tec_digital::PlagiarismMisDocItem>> plagiarismDocsByCourse;
    std::optional<academic::tec_digital::PrematriculaSnapshot> prematricula;
    std::optional<academic::tec_digital::FeriaNegociosSnapshot> feriaNegocios;
    std::optional<academic::tec_digital::UserModelSnapshot> userModel;
    std::vector<academic::tec_digital::SchoolCatalogEntry> schoolCatalog;
    std::string gestorInscripcionSelectorHtml;
    std::string gestorInscripcionEventsJson;
    std::string serverDateTimeIso;
    std::int64_t serverTimeOffsetSec = 0;
    std::map<std::string, std::string> consentStatusByModule;

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
    std::map<std::string, std::vector<academic::tec_digital::SpacePageSnapshot>> customPagesBySpace;
    std::optional<academic::tec_digital::FolderWalkResult> personalDocuments;
    std::string personalDocumentsFolderId;

    // Public Guia de Horarios (https://tec-appsext.itcr.ac.cr/guiahorarios/)
    std::vector<academic::tec_digital::PublicGuiaEscuelaInfo> publicGuiaEscuelas;

    // Change Detection Radar, Native GAAP / Motor de Juegos & Monitor Settings
    std::vector<academic::tec_digital::TecChangeEvent> activityFeed;
    std::optional<academic::tec_digital::MotorJuegosUserInfo> motorJuegosUser;
    std::vector<academic::tec_digital::MotorJuegosActivity> motorJuegosActivities;
    std::string gaapServerUrl;
    int monitorIntervalMinutes = 15; // 0 = off, 5/15/30/60 min (CV wait_for, 0% idle CPU)

    std::vector<std::string> warnings;
};

struct GlobalFileCatalogEntry {
    std::string spaceUrl;
    std::string spaceName;
    std::string spaceType;       // "CLASE" | "CATEDRA" | "ESCUELA" | "PERSONAL" | "EVALUACION" | ...
    std::string folderPath;
    std::string parentFolderId;
    std::string fileName;
    std::string title;
    std::string objectId;
    std::string mimeType;
    std::uint64_t contentSize = 0;
    std::string creationDateAnsi;
    std::string lastModifiedAnsi;
    std::string liveRevision;
    std::string authorName;
    std::string downloadUrl;
    std::string localRelPath;
    bool isLocal = false;
    bool isPersonal = false;
    bool isEvalAttachment = false;
    bool isLinkedInPortada = false;
    bool isRecentlyChanged = false;
    bool subscriptionActive = false;
    std::string searchFolded;
};

struct TecSyncOptions {
    bool syncCommunities = true;
    bool syncCourseOverviews = true;
    bool syncEvaluations = true;
    bool syncRubrics = true;
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
    bool syncCatedrasAndCommunities = true;
    bool syncPersonalDocuments = true;
    bool syncCustomSpacePages = true;
    bool syncPrematricula = true;
    bool syncPlagiarismCourseDocs = true;
    bool syncServerClock = true;
    bool syncConsentStatus = true;
    bool syncCatedraCalendarsAndDocs = true;
    bool syncAllSpaceFolders = true;     // Index file-storage trees across ALL accessible spaces (not just 6)
    bool syncWeeklySubpages = true;      // Recursively follow linked ?page_num=N subpages (e.g. S1..S16)
    bool syncMotorJuegos = true;         // Query native tda-motor-juegos & GAAP server metadata
    int maxCustomTabsPerSpace = 12;
    int maxSubpagesPerSpace = 20;
    int maxActiveCoursesToDeepSync = 6;
    int maxNonClassSpacesToDeepSync = 12;
    int maxAllSpacesFolderWalk = 34;
    int politeDelayMs = 250;       // Respectful rate-limiting between live requests
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

struct GlobalHorariosGroupEntry {
    std::string courseCode;      // e.g. "EL2113"
    std::string courseName;      // e.g. "Circuitos eléctricos en corriente continua"
    int credits = 0;
    int hours = 0;
    std::string groupId;         // e.g. "50"
    std::string schoolCode;      // e.g. "E", "CA", "MA"
    std::string schoolName;      // e.g. "ESCUELA DE INGENIERIA EN ELECTRONICA"
    std::string careerName;      // Optional career label when enriched from Expediente TEC
    std::string campusFull;      // e.g. "CAMPUS TECNOLOGICO CENTRAL CARTAGO"
    std::string campusShort;     // "Cartago" | "San José" | "San Carlos" | "Alajuela" | "Limón"
    std::string periodCode;      // e.g. "S-2", "S-1", "V-1", "B-1", "C-1", "T-1"
    std::string periodLabel;     // e.g. "Semestre 2"
    int year = 2026;
    std::string modality;        // "Regular", "Virtual", "Semipresencial", "Asistida", etc.
    std::string professor;
    std::string room;            // Exact classroom when available (Expediente TEC)
    int seatsAvailable = -1;     // -1 = unknown (public guide), >= 0 = live seats (Expediente)
    std::string reservedNote;    // Non-empty when group has campus/dept/plan reservation
    std::vector<academic::horarios_tec::TimeSlot> sessions;
    std::string scheduleSummary; // e.g. "L 07:30-09:20 · J 07:30-09:20"
    bool hasNightSession = false;
    std::uint8_t daysMask = 0;   // bit 0=L .. bit 6=D
    std::string searchFolded;    // Precomputed accent-folded lowercase text for <1ms search
};

struct GlobalHorariosIndex {
    int year = 2026;
    std::string indexedAtUtc;
    int totalSchoolsFetched = 0;
    int schoolsWithCourses = 0;
    std::size_t uniqueCoursesCount = 0;
    std::vector<std::string> campuses;
    std::vector<std::string> schools;
    std::vector<std::string> careers;
    std::vector<std::string> periods;
    std::vector<std::string> modalities;
    std::vector<GlobalHorariosGroupEntry> entries;
};

struct GlobalHorariosSearchFilter {
    std::string queryUtf8;             // Space-separated AND tokens (code, name, prof, school, career, campus, room)
    std::string campusFilter;          // Empty = all campuses
    std::string schoolOrCareerFilter;  // Empty = all schools/careers
    std::string periodFilter;          // Empty = all, or "S-1", "S-2", "V-1", "B", "C", "T", "H"
    std::string modalityFilter;        // Empty = all, or "Regular", "Virtual", "Semipresencial", ...
    int dayFilter = -1;                // -1 = any day, 0..5 = Mon..Sat
    bool excludeNight = false;
    bool onlyWithSeats = false;
    bool onlyEligibleByCurriculum = false;
    std::size_t maxResults = 500;
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

    bool SyncCourseOnDemand(const std::string& communityUrlOrPath,
                            std::string* outErr = nullptr);

    bool SyncPersonalDocuments(bool downloadFiles = false,
                               std::string* outSummaryUtf8 = nullptr,
                               std::string* outErr = nullptr);

    academic::tec_digital::Result<academic::tec_digital::SpacePageSnapshot>
    FetchSpacePage(const std::string& communityUrlOrPath,
                   int pageNum,
                   const std::string& tabLabel = "");

    academic::tec_digital::Result<academic::tec_digital::NewsArticleDetail>
    FetchNewsArticle(const std::string& newsUrl);

    // Explicit user-confirmed student actions (always blocked unless userConfirmed == true)
    academic::tec_digital::Result<academic::tec_digital::EvaluationUploadResult>
    UploadCourseEvaluation(const academic::tec_digital::EvaluationUploadRequest& req,
                           bool userConfirmed);

    academic::tec_digital::Result<bool>
    DeleteEvaluationSubmission(const std::string& communityUrlOrPath,
                               const std::string& revisionId,
                               const std::string& itemId,
                               bool userConfirmed);

    academic::tec_digital::Result<bool>
    MarkNotificationAsRead(const std::string& notificationId, bool userConfirmed);

    academic::tec_digital::Result<bool>
    DeleteUserNotification(const std::string& notificationId, bool userConfirmed);

    academic::tec_digital::Result<bool>
    RefreshEnrolledCoursesFromDar(bool userConfirmed);

    // Full legitimate student actions & automations (no artificial blocks in live CuadernU UI)
    academic::tec_digital::Result<bool>
    DeleteAllUserNotifications(bool userConfirmed);

    academic::tec_digital::Result<bool>
    ToggleFolderNotificationSubscription(const std::string& objectId, bool subscribe);

    academic::tec_digital::Result<bool>
    RequestEvaluationNotifications(const std::string& communityUrlOrPath,
                                   const std::string& assignId,
                                   bool enabled);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    SyncExpedienteDarCourses(bool userConfirmed);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    UploadPlagiarismFile(const std::string& folderId, const std::string& localFilePathUtf8);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    SubmitPlagiarismEphorus(const std::string& docUid, const std::string& itemId);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    UpdatePlagiarismVisibility(const std::string& docUid, bool visible);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    SavePrematriculaSelection(const std::string& preEnrollId,
                              const std::string& curriculumId,
                              const std::string& courseCodesCsv);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    SubmitInclusionRequest(const std::string& campaignId,
                           const std::string& courseCode,
                           const std::string& group,
                           const std::string& comment);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    DeleteInclusionRequest(const std::string& requestId, bool userConfirmed);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    RegisterFairProject(const std::string& fairId,
                        const std::string& title,
                        const std::string& description,
                        const std::string& categoryId,
                        const std::string& sedeId);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    SaveUserPrivacySettings(const std::string& queryParams);

    academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>
    SubmitConsentAnswer(const std::string& moduleKey, bool accepted);

    academic::tec_digital::Result<std::string>
    CreatePersonalFolder(const std::string& parentFolderId, const std::string& folderName);

    academic::tec_digital::Result<bool>
    UploadPersonalFile(const std::string& parentFolderId, const std::string& localFilePathUtf8);

    academic::tec_digital::Result<bool>
    DeletePersonalItem(const std::string& objectId, bool isFolder, bool userConfirmed);

    academic::tec_digital::Result<academic::tec_digital::FileDetailedMetadata>
    FetchFileDetails(const std::string& objectId);

    // Media cache helper for weekly module infographics (strictly GET, cached locally)
    std::string EnsureMediaImageCached(const std::string& imageUrl);
    std::string CachedMediaImagePathIfPresent(const std::string& imageUrl) const;

    // Global cross-space file catalog & Change Detection Radar
    static std::vector<academic::tec_digital::TecChangeEvent>
    ComputeSnapshotChanges(const TecAcademicSnapshot& prev, const TecAcademicSnapshot& next);

    static std::vector<GlobalFileCatalogEntry>
    BuildGlobalFileCatalog(const TecAcademicSnapshot& snap);

    void MarkAllChangesRead();
    void MarkChangeRead(const std::string& eventId);

    // Low-power waitable CV background monitor (0% idle CPU, zero SetTimer)
    using ChangeNotifyCallback = std::function<void(const std::vector<academic::tec_digital::TecChangeEvent>&)>;
    void ConfigureAutoMonitor(int intervalMinutes, ChangeNotifyCallback onNewChanges = nullptr);
    void StopAutoMonitor();
    int AutoMonitorIntervalMinutes() const noexcept;

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
        const std::string& taskId = "",
        const std::string& studentId = "");

    academic::tec_digital::Result<std::string> FetchPlagiarismReportUrl(const std::string& docUid);
    academic::tec_digital::Result<std::string> FetchCommunityMembersCsv(const std::string& communityUrlOrPath);

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

    // Global All-TEC Schedule Search Index (all schools, campuses, careers & modalities)
    static GlobalHorariosIndex BuildGlobalScheduleIndexFromRows(
        const std::vector<academic::tec_digital::PublicGuiaSessionRow>& rows,
        int year,
        int totalSchoolsFetched = 0,
        int schoolsWithCourses = 0);

    bool BuildGlobalScheduleIndex(
        int year = 2026,
        ProgressCallback onProgress = nullptr,
        std::atomic<bool>* cancelFlag = nullptr,
        std::string* outErr = nullptr,
        int maxConcurrency = 8);

    bool MergeExpedienteCarreraIntoGlobalIndex(
        const std::string& sedeKey,
        const std::string& carreraKey,
        const std::string& periodoKey,
        const std::string& campusLabel,
        const std::string& carreraName,
        std::string* outErr = nullptr);

    bool SaveGlobalScheduleIndexToDisk(const GlobalHorariosIndex& idx, std::string* outErr = nullptr) const;
    bool LoadGlobalScheduleIndexFromDisk(int year = 2026, std::string* outErr = nullptr);

    GlobalHorariosIndex GetGlobalScheduleIndex() const;
    void SetGlobalScheduleIndexForTesting(GlobalHorariosIndex idx);

    std::vector<GlobalHorariosGroupEntry> SearchGlobalScheduleIndex(
        const GlobalHorariosSearchFilter& filter,
        std::size_t* outTotalMatches = nullptr) const;

    LiveScheduleSolveResult SolveSelectedCoursesFromGlobalIndex(
        const std::vector<std::string>& selectedCourseCodes,
        const GlobalHorariosSearchFilter& baseFilter,
        const academic::horarios_tec::SolverPreferences& prefs,
        bool filterEligibleByCurriculum = false,
        bool includeUnifiedCalendarBusySlots = false,
        const std::vector<academic::horarios_tec::BusySlot>& extraBusySlots = {}) const;

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
    mutable std::mutex globalHorariosMutex_;
    GlobalHorariosIndex globalHorariosIndex_;

    // Condition-variable waitable monitor thread (0% idle CPU)
    mutable std::mutex monitorMutex_;
    std::condition_variable monitorCv_;
    std::thread monitorThread_;
    bool monitorStop_ = false;
    int monitorIntervalMin_ = 0;
    ChangeNotifyCallback monitorNotifyCb_;
};

}  // namespace DocStudio::TecDigital
