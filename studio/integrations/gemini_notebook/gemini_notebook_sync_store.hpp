#pragma once
// ============================================================================
// doc_studio/integrations/gemini_notebook/gemini_notebook_sync_store.hpp
// Independent Offline-First Local Sync Store, Pre-Flight Validator,
// Academic TEC Classifier, Chapter Markdown Bundler, .cudoc Exporter,
// DPAPI Token Vault, and Sliding-Window Rate Limiter (<= 20 uploads/min).
// ============================================================================

#include "gemini_notebook_types.hpp"
#include "../../core/document_model.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace DocStudio::GeminiNotebook {

// Cryptographic SHA-256 helpers (Windows BCrypt streaming, zero large RAM spikes)
std::string ComputeSha256HexBytes(const std::uint8_t* data, std::size_t len);
std::string ComputeSha256HexString(const std::string& utf8);
bool ComputeSha256HexFile(const std::wstring& filePath, std::string& outHex, std::uint64_t* outSizeBytes = nullptr);

// UTC timestamp & URL helpers
std::string CurrentUtcIso8601();
std::string CurrentUtcDateYmd();
std::string BuildWebNotebookUrl(const std::string& resourceNameUtf8);
std::string SanitizeSecretFromText(const std::string& textUtf8, const GeminiNotebookAuthTokens& tokens);

// Supported formats & word estimation
std::string DetectSupportedMimeType(const std::wstring& filePath);
bool IsSupportedNotebookFileExtension(const std::wstring& extLower);
std::uint64_t EstimateWordCountFromUtf8(const std::string& textUtf8) noexcept;
std::uint64_t EstimateWordCountForLocalFile(const std::wstring& filePath, const std::string& mimeTypeUtf8, std::uint64_t sizeBytes);

// Strict Pre-Flight Validator (enforces 200 MiB, 500k words, allow-list URLs, supported MIME)
PreFlightReport ValidateSourceForUpload(
    const AcademicMaterialCandidate& candidate,
    const GeminiNotebookConfig& cfg,
    bool computeHash = true);

// Academic Classifier (TEC Digital course / chapter / category grouping)
std::string ExtractChapterLabelUtf8(const std::string& folderPathUtf8, const std::string& fileNameUtf8);
std::string DetectMaterialCategoryUtf8(const std::string& folderPathUtf8, const std::string& fileNameUtf8);
std::vector<ClassifiedChapterGroup> ClassifyAcademicMaterials(
    const std::vector<AcademicMaterialCandidate>& candidates,
    std::size_t recommendedCapPerNotebook = kRecommendedMaxSourcesPerNotebook);

// Combine small notes (.md/.txt/inline) in a chapter into one structured Markdown source (<= 400k words)
struct ChapterBundleResult {
    bool ok = false;
    std::string displayNameUtf8;
    std::string markdownUtf8;
    std::string sha256Hex;
    std::uint64_t estimatedWords = 0;
    std::size_t bundledFileCount = 0;
    std::string errorUtf8;
};

ChapterBundleResult BuildChapterMarkdownBundle(
    const std::string& courseNameUtf8,
    const std::string& chapterLabelUtf8,
    const std::vector<AcademicMaterialCandidate>& candidates,
    std::uint64_t maxWords = kSafeChapterBundleMaxWords);

// Export open CuadernU document (.cudoc) to clean structured Markdown for incremental sync
std::string ExportDocumentToSyncMarkdown(const DocumentModel& doc);

// Sliding-window Token Bucket Rate Limiter (official limit: <= 20 uploads per 60s)
class UploadRateLimiter {
public:
    using ClockMsFn = std::function<std::uint64_t()>;

    explicit UploadRateLimiter(
        std::size_t maxRequestsPerMinute = kOfficialMaxUploadsPerMinute,
        ClockMsFn clockMs = nullptr);

    void SetMaxRequestsPerMinute(std::size_t maxPerMin);
    std::size_t MaxRequestsPerMinute() const noexcept;

    // Returns 0 if a slot is available now and reserves it; otherwise returns ms to wait (does NOT reserve).
    std::uint64_t TryAcquireOrWaitMs();

    // Records server-mandated cooldown from HTTP 429 Retry-After header.
    void NoteRetryAfterMs(std::uint64_t cooldownMs);

    // Current active timestamps inside the 60s window.
    std::size_t UsedInCurrentWindow() const;

private:
    std::uint64_t NowMs() const;

    mutable std::mutex mtx_;
    std::size_t maxPerMin_ = kOfficialMaxUploadsPerMinute;
    ClockMsFn clockMs_;
    std::deque<std::uint64_t> timestampsMs_;
    std::uint64_t retryBlockedUntilMs_ = 0;
};

// Independent Offline-First Local Store (Manifest + Config + DPAPI Token Vault)
class GeminiNotebookSyncStore {
public:
    explicit GeminiNotebookSyncStore(std::wstring storageRootDir = L"");

    static std::wstring DefaultStorageRootDir();
    const std::wstring& StorageRootDir() const noexcept { return rootDir_; }

    bool LoadAll(std::string* outErr = nullptr);
    bool SaveConfig(std::string* outErr = nullptr) const;
    bool SaveManifest(std::string* outErr = nullptr) const;
    bool SaveTokensDpapi(std::string* outErr = nullptr) const;
    bool LoadTokensDpapi(std::string* outErr = nullptr);
    bool ClearTokens(std::string* outErr = nullptr);

    const GeminiNotebookConfig& Config() const noexcept { return config_; }
    GeminiNotebookConfig& MutableConfig() noexcept { return config_; }

    const GeminiNotebookAuthTokens& Tokens() const noexcept { return tokens_; }
    GeminiNotebookAuthTokens& MutableTokens() noexcept { return tokens_; }

    const std::vector<LocalNotebookMapping>& NotebookMappings() const noexcept { return notebooks_; }
    const std::vector<LocalSyncSourceEntry>& SyncedSources() const noexcept { return sources_; }
    const std::vector<SyncHistoryRecord>& History() const noexcept { return history_; }

    const LocalNotebookMapping* FindNotebookByLocalKey(const std::string& localKeyUtf8) const;
    const LocalNotebookMapping* FindNotebookByRemoteName(const std::string& remoteNameUtf8) const;
    void UpsertNotebookMapping(const LocalNotebookMapping& mapping);

    const LocalSyncSourceEntry* FindSourceBySha256(
        const std::string& notebookKeyUtf8,
        const std::string& sha256Hex) const;
    const LocalSyncSourceEntry* FindLatestSourceByOrigin(
        const std::string& notebookKeyUtf8,
        const std::string& localPathOrUrlUtf8) const;
    std::vector<LocalSyncSourceEntry> SourcesForNotebook(const std::string& notebookKeyUtf8) const;

    void UpsertSourceEntry(const LocalSyncSourceEntry& entry);
    void MarkPriorSourcesSuperseded(const std::string& notebookKeyUtf8, const std::string& localPathOrUrlUtf8, const std::string& exceptSha256Hex);
    void AppendHistory(const SyncHistoryRecord& rec);

    // Dry-run incremental sync planner (SHA-256 deduplication + safe versioned updates)
    SyncDiffPlan ComputeIncrementalSyncPlan(
        const std::string& targetLocalNotebookKeyUtf8,
        const std::string& targetNotebookDisplayNameUtf8,
        const std::vector<AcademicMaterialCandidate>& candidates,
        const std::string& dateTagYmdOverride = "") const;

private:
    std::wstring rootDir_;
    GeminiNotebookConfig config_{};
    GeminiNotebookAuthTokens tokens_{};
    std::vector<LocalNotebookMapping> notebooks_;
    std::vector<LocalSyncSourceEntry> sources_;
    std::vector<SyncHistoryRecord> history_;
};

} // namespace DocStudio::GeminiNotebook
