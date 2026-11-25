#pragma once
// ============================================================================
// doc_studio/integrations/gemini_notebook/gemini_notebook_types.hpp
// Pure data contracts and roadmap interfaces for the decoupled, optional
// Google Gemini Notebook integration (REST API v1 + Streamable HTTP MCP +
// Independent Offline-First Local Sync Store).
// ============================================================================

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace DocStudio::GeminiNotebook {

constexpr std::uint64_t kOfficialMaxUploadBytes = 200ull * 1024ull * 1024ull; // 200 MiB
constexpr std::uint64_t kOfficialMaxWordsPerSource = 500000ull;               // 500,000 words
constexpr std::size_t   kOfficialMaxUploadsPerMinute = 20u;                   // 20 uploads/min/user
constexpr std::size_t   kRecommendedMaxSourcesPerNotebook = 50u;              // Standard tier source cap
constexpr std::uint64_t kSafeChapterBundleMaxWords = 400000ull;               // Safety margin under 500k

enum class SourceInputKind : std::uint8_t {
    LocalFile = 0,
    WebUrl = 1,
    YouTubeUrl = 2,
    InlineTextOrMarkdown = 3
};

enum class SyncSourceState : std::uint8_t {
    LocalOnly = 0,
    Queued = 1,
    Uploading = 2,
    ProcessingRemote = 3,
    ActiveSynced = 4,
    Failed = 5,
    SupersededLocal = 6,   // Local file changed; prior remote copy remains because Public Preview lacks DELETE
    OrphanedRemote = 7     // Exists remotely but missing from local folder
};

inline const char* SyncSourceStateToString(SyncSourceState st) noexcept {
    switch (st) {
    case SyncSourceState::LocalOnly:        return "local_only";
    case SyncSourceState::Queued:           return "queued";
    case SyncSourceState::Uploading:        return "uploading";
    case SyncSourceState::ProcessingRemote: return "processing";
    case SyncSourceState::ActiveSynced:     return "active";
    case SyncSourceState::Failed:           return "failed";
    case SyncSourceState::SupersededLocal:  return "superseded_local";
    case SyncSourceState::OrphanedRemote:   return "orphaned_remote";
    }
    return "local_only";
}

inline SyncSourceState SyncSourceStateFromString(const std::string& s) noexcept {
    if (s == "queued")           return SyncSourceState::Queued;
    if (s == "uploading")        return SyncSourceState::Uploading;
    if (s == "processing")       return SyncSourceState::ProcessingRemote;
    if (s == "active")           return SyncSourceState::ActiveSynced;
    if (s == "failed")           return SyncSourceState::Failed;
    if (s == "superseded_local") return SyncSourceState::SupersededLocal;
    if (s == "orphaned_remote")  return SyncSourceState::OrphanedRemote;
    return SyncSourceState::LocalOnly;
}

struct GeminiNotebookConfig {
    bool enabled = true;
    std::string apiBaseUrlUtf8 = "https://notebook.googleapis.com/v1";
    std::string uploadBaseUrlUtf8 = "https://notebook.googleapis.com/upload/v1";
    std::string mcpEndpointUrlUtf8 = "https://notebook.googleapis.com/mcp";
    std::string oauthTokenEndpointUtf8 = "https://oauth2.googleapis.com/token";
    std::string oauthAuthorizeEndpointUtf8 = "https://accounts.google.com/o/oauth2/v2/auth";
    std::string oauthScopeUtf8 = "https://www.googleapis.com/auth/notebook";
    std::string projectIdUtf8;          // Optional x-goog-user-project header
    std::string oauthClientIdUtf8;      // User-provided Google Cloud Desktop OAuth Client ID
    std::string oauthClientSecretUtf8;  // Stored ONLY inside oauth_tokens.dpapi, never in config.json!
    bool bundleSmallNotesByChapter = false;
    std::uint64_t maxUploadBytes = kOfficialMaxUploadBytes;
    std::uint64_t maxWordsPerSource = kOfficialMaxWordsPerSource;
    std::size_t maxUploadsPerMinute = kOfficialMaxUploadsPerMinute;
    std::size_t recommendedMaxSourcesPerNotebook = kRecommendedMaxSourcesPerNotebook;
    int httpTimeoutMs = 45000;
};

struct GeminiNotebookAuthTokens {
    std::string accessTokenUtf8;
    std::string refreshTokenUtf8;
    std::string oauthClientSecretUtf8;
    std::int64_t expiresAtEpochSec = 0;
    std::string accountEmailUtf8;       // Informational only (e.g. personal@gmail.com)
    bool workspaceRejectionSeen = false;

    bool HasUsableAccessToken(std::int64_t nowEpochSec) const noexcept {
        if (accessTokenUtf8.empty()) return false;
        if (expiresAtEpochSec <= 0) return true; // Manual / ADC bearer token without explicit expiry
        return nowEpochSec + 60 < expiresAtEpochSec;
    }

    bool HasAnyCredential() const noexcept {
        return !accessTokenUtf8.empty() || !refreshTokenUtf8.empty();
    }
};

struct RemoteNotebookRecord {
    std::string resourceNameUtf8; // e.g. "notebooks/abc-123"
    std::string displayNameUtf8;
    std::string createTimeUtf8;
    std::string updateTimeUtf8;
    std::string webUrlUtf8;       // e.g. "https://notebooklm.google.com/notebook/abc-123"
    std::size_t remoteSourceCount = 0;
};

struct RemoteSourceRecord {
    std::string resourceNameUtf8; // e.g. "notebooks/abc-123/sources/src-456"
    std::string displayNameUtf8;
    std::string mimeTypeUtf8;
    std::string stateUtf8;        // "PROCESSING" | "ACTIVE" | "FAILED"
    std::string sourceUrlUtf8;
    std::string createTimeUtf8;
};

struct LocalSyncSourceEntry {
    std::string localIdUtf8;              // Stable local UUID / key
    std::string notebookKeyUtf8;          // Maps to RemoteNotebookRecord::resourceNameUtf8 or local group key
    std::string remoteNotebookNameUtf8;   // "notebooks/..."
    std::string remoteSourceNameUtf8;     // "notebooks/.../sources/..."
    std::string displayNameUtf8;
    SourceInputKind inputKind = SourceInputKind::LocalFile;
    std::string localFilePathUtf8;        // Original local file (never deleted by sync)
    std::string sourceUrlUtf8;            // For WebUrl / YouTubeUrl
    std::string mimeTypeUtf8;
    std::string sha256Hex;                // Content hash for deduplication & change detection
    std::uint64_t sizeBytes = 0;
    std::uint64_t estimatedWords = 0;
    std::string tecCourseUtf8;            // Optional TEC Digital course slug/name
    std::string chapterLabelUtf8;         // Classified chapter/unit label
    std::string materialCategoryUtf8;     // "Teoria" | "Laboratorio" | "Practica" | "Evaluacion" | "Notas"
    SyncSourceState state = SyncSourceState::LocalOnly;
    int versionNumber = 1;
    std::string syncedAtUtc;
    std::string lastErrorUtf8;
};

struct LocalNotebookMapping {
    std::string localKeyUtf8;             // e.g. "tec:FI-1101" or "doc:Calculo"
    std::string displayNameUtf8;
    std::string remoteNotebookNameUtf8;   // "notebooks/..." once created remotely
    std::string webUrlUtf8;
    std::string tecCourseUtf8;
    std::string lastSyncUtc;
};

struct SyncHistoryRecord {
    std::string timestampUtc;
    std::string notebookDisplayNameUtf8;
    std::size_t uploadedCount = 0;
    std::size_t skippedUnchangedCount = 0;
    std::size_t versionedUpdateCount = 0;
    std::size_t failedCount = 0;
    std::string summaryUtf8;
};

struct AcademicMaterialCandidate {
    std::string displayNameUtf8;
    SourceInputKind inputKind = SourceInputKind::LocalFile;
    std::string localFilePathUtf8;
    std::string sourceUrlUtf8;
    std::string inlineMarkdownUtf8;
    std::string inlineMimeTypeUtf8 = "text/markdown";
    std::string tecCourseUtf8;
    std::string folderPathUtf8;
    std::string precomputedSha256Hex;
    std::uint64_t precomputedSizeBytes = 0;
};

struct ClassifiedChapterGroup {
    std::string courseNameUtf8;
    std::string chapterLabelUtf8;
    std::string suggestedNotebookTitleUtf8;
    std::vector<AcademicMaterialCandidate> items;
    bool exceedsRecommendedCap = false;
};

enum class PreFlightSeverity : std::uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2
};

struct PreFlightIssue {
    PreFlightSeverity severity = PreFlightSeverity::Info;
    std::string codeUtf8;
    std::string messageUtf8;
};

struct PreFlightReport {
    bool ok = true;
    std::string detectedMimeTypeUtf8;
    std::string sha256Hex;
    std::uint64_t sizeBytes = 0;
    std::uint64_t estimatedWords = 0;
    bool isYouTube = false;
    std::vector<PreFlightIssue> issues;
};

enum class SyncDiffAction : std::uint8_t {
    CreateNew = 0,
    SkipUnchanged = 1,
    UploadVersionedCopy = 2, // Local hash changed -> never overwrites silently; appends [vN YYYY-MM-DD]
    BlockedByPreFlight = 3
};

struct SyncDiffItem {
    SyncDiffAction action = SyncDiffAction::CreateNew;
    AcademicMaterialCandidate candidate;
    PreFlightReport preflight;
    std::string targetDisplayNameUtf8;
    int nextVersionNumber = 1;
    std::string priorRemoteSourceNameUtf8;
    std::string reasonUtf8;
};

struct SyncDiffPlan {
    std::string targetLocalNotebookKeyUtf8;
    std::string targetNotebookDisplayNameUtf8;
    std::string existingRemoteNotebookNameUtf8;
    std::size_t existingRemoteSourceCount = 0;
    std::vector<SyncDiffItem> items;
    std::size_t countNew = 0;
    std::size_t countUnchanged = 0;
    std::size_t countVersioned = 0;
    std::size_t countBlocked = 0;
    bool quotaWarningExceeds50 = false;
};

struct SyncExecutionSummary {
    bool ok = false;
    bool workspaceAccountRejected = false;
    bool cancelled = false;
    std::string remoteNotebookNameUtf8;
    std::string remoteWebUrlUtf8;
    std::size_t uploadedCount = 0;
    std::size_t skippedCount = 0;
    std::size_t versionedCount = 0;
    std::size_t failedCount = 0;
    std::vector<std::string> notesUtf8;
    std::string errorUtf8;
};

// Roadmap-ready capability descriptor (updated dynamically from MCP tools/list or REST discovery)
struct GeminiNotebookCapabilities {
    bool createNotebook = true;
    bool listNotebooks = true;
    bool listSources = true;
    bool createWebAndYouTubeSource = true;
    bool createTextAndMarkdownSource = true;
    bool uploadBinaryFileRest = true;
    // Public Preview roadmap features (auto-enabled once exposed by Google API/MCP)
    bool queryNotebookApi = false;
    bool generateArtifactsApi = false;
    bool deleteNotebookOrSourceApi = false;
    std::vector<std::string> discoveredMcpTools;
};

// Extensible interface for future Google Public Preview -> GA roadmap capabilities
class IGeminiNotebookRoadmapCapabilities {
public:
    virtual ~IGeminiNotebookRoadmapCapabilities() = default;
    virtual GeminiNotebookCapabilities CurrentCapabilities() const = 0;
    virtual bool QueryNotebookRemote(
        const std::string& remoteNotebookNameUtf8,
        const std::string& questionUtf8,
        std::string& outAnswerUtf8,
        std::string* outErr = nullptr) = 0;
    virtual bool GenerateArtifactRemote(
        const std::string& remoteNotebookNameUtf8,
        const std::string& artifactKindUtf8,
        std::string& outArtifactJsonUtf8,
        std::string* outErr = nullptr) = 0;
    virtual bool DeleteRemoteSource(
        const std::string& remoteSourceNameUtf8,
        std::string* outErr = nullptr) = 0;
};

struct NotebookHttpResponse {
    bool transportOk = false;
    int httpStatus = 0;
    std::string bodyUtf8;
    std::map<std::string, std::string> headersLower; // Lowercase header names
    std::string errorUtf8;
};

class INotebookHttpTransport {
public:
    virtual ~INotebookHttpTransport() = default;

    virtual NotebookHttpResponse SendJson(
        const std::string& methodUtf8,
        const std::string& urlUtf8,
        const std::vector<std::pair<std::string, std::string>>& headersUtf8,
        const std::string* bodyUtf8,
        int timeoutMs) = 0;

    virtual NotebookHttpResponse UploadFileRaw(
        const std::string& urlUtf8,
        const std::vector<std::pair<std::string, std::string>>& headersUtf8,
        const std::wstring& filePath,
        int timeoutMs,
        std::function<bool()> cancelCheck = nullptr) = 0;
};

} // namespace DocStudio::GeminiNotebook
