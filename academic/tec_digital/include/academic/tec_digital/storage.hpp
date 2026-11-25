#pragma once

#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/canon.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

struct FolderRecord {
    std::string folder_id;
    std::string parent_id;
    std::string name;
    std::string community;
    std::string community_type;
    std::string folder_path;
    int depth = 0;
    size_t child_count = 0;
};

struct FolderUrlLink {
    std::string object_id;
    std::string parent_folder_id;
    std::string title;
    std::string url;
    std::string community;
    std::string folder_path;
};

struct FileDetailedMetadata {
    int64_t object_id = 0;
    int64_t parent_id = 0;
    std::string mime_type;
    std::string name;
    std::string live_revision;
    std::string type;
    std::string pretty_type;
    std::string last_modified_ansi;
    uint64_t content_size = 0;
    std::string url;
    std::string sort_key;
    std::string sort_key_desc;
    std::string file_upload_name;
    std::string title;
    std::string file_url;
    int new_p = 0;
    std::string fs_type;
    int fs_sort_relevance = 0;
    std::string author_id;
    std::string author_name;
    int admin_p = 0;
    int write_p = 0;
    int delete_p = 0;
    int read_p = 0;
};

enum class FileStatus {
    Pending,
    Downloaded,
    Failed,
    Orphan404,
    SkippedLarge,
    SessionExpired
};

struct FileRecord {
    std::string object_id;
    std::string parent_folder_id;
    std::string community;
    std::string community_type = "CLASE";  // "CLASE" or "CATEDRA"
    std::string folder_path;
    std::string name;
    std::string title;
    std::string file_upload_name;
    std::string mime_type;
    uint64_t content_size = 0;
    std::string live_revision;
    std::string creation_date_ansi;
    std::string last_modified_ansi;
    std::string author_name;
    std::string plagiarism_status;
    std::string plagiarism_text;
    double plagiarism_percentage = -1.0;
    std::string plagiarism_doc_uid;
    bool subscription_active = false;
    std::string file_url;
    std::string download_url;
    std::vector<std::string> candidate_urls;

    // Populated after download
    FileStatus status = FileStatus::Pending;
    std::string sha256;
    std::string blob_rel_path;
    std::string logical_rel_path;
    std::string quarantine_rel_path;
    uint64_t bytes_written = 0;
    bool deduplicated = false;
    bool magic_ok = false;
    std::string detected_magic;
    int http_status = 0;
    std::string error_message;
};

struct FolderWalkResult {
    std::string root_folder_id;
    std::vector<FolderRecord> folders;
    std::vector<FileRecord> files;
    std::vector<FolderUrlLink> url_links;
    std::vector<std::pair<std::string, std::string>> breadcrumbs;  // {title, url}
    std::vector<std::string> warnings;
    bool session_expired = false;
};

struct WalkerOptions {
    int max_depth = 10;
    size_t max_folders = 400;
    int request_delay_ms = 0;
};

class FileStorageWalker {
public:
    FileStorageWalker(PolicyEnforcingTransport& transport,
                      AuthClient& auth,
                      std::string base_url = "https://tecdigital.tec.ac.cr",
                      SanitizeOptions sanitize_opts = {});

    static std::vector<std::string> ExtractRootFolderIds(std::string_view html_text);

    Result<std::vector<std::string>> DiscoverRootFolderIds(
        std::string_view community_base_url,
        size_t max_candidates = 5);

    Result<FileDetailedMetadata> FetchFileDetails(std::string_view object_id);
    Result<std::vector<FileVersionRecord>> FetchFileVersions(std::string_view object_id);

    Result<FolderWalkResult> WalkFolderTree(
        std::string_view root_folder_id,
        std::string_view community_slug,
        std::string_view community_type = "CLASE",
        std::string_view community_base_url = "",
        const WalkerOptions& opts = {});

    static Result<FolderWalkResult> ParseFolderJson(
        std::string_view json_text,
        std::string_view expected_folder_id = "");

    static Result<FileDetailedMetadata> ParseFileDetailsJson(std::string_view json_text);

private:
    PolicyEnforcingTransport& transport_;
    AuthClient& auth_;
    std::string base_url_;
    Sanitizer sanitizer_;
};

struct DownloaderOptions {
    std::vector<int> backoff_ms = {1000, 3000, 10000, 30000, 60000};
    int max_retries = 5;
    int min_delay_ms = 400;
    int stall_grace_ms = 60000;
    double stall_min_bps = 1024.0;
    uint64_t max_file_bytes = 512ULL * 1024ULL * 1024ULL;
    bool verify_size_with_head = true;
    int timeout_small_ms = 120000;
    int timeout_medium_ms = 300000;
    int timeout_large_ms = 600000;
    bool create_logical_mirror = true;
};

enum class DownloadOutcome {
    Downloaded,
    Deduplicated,
    Failed,
    Orphan404,
    SkippedLarge,
    SessionExpired
};

struct MagicCheckResult {
    bool valid = false;
    bool is_html_login = false;
    bool is_generic_html = false;
    std::string detected_kind;
    std::string reason;
};

struct DownloadResult {
    DownloadOutcome outcome = DownloadOutcome::Failed;
    int http_status = 0;
    uint64_t bytes_written = 0;
    std::string sha256;
    std::string blob_rel_path;
    std::string logical_rel_path;
    std::string quarantine_rel_path;
    bool deduplicated = false;
    bool magic_ok = false;
    bool saw_range_header_omitted_on_zero = true;
    std::string error_message;
};

class StorageDownloader {
public:
    StorageDownloader(PolicyEnforcingTransport& transport,
                      AuthClient& auth,
                      std::string base_url = "https://tecdigital.tec.ac.cr",
                      DownloaderOptions opts = {},
                      SanitizeOptions sanitize_opts = {});

    static std::vector<std::string> BuildCandidateUrls(
        const FileRecord& file,
        std::string_view base_url,
        std::string_view community_base_url = "");

    static std::string BuildBulkZipUrl(std::string_view community_base_url,
                                         std::string_view root_folder_id);
    static std::string BuildArchiveZipUrl(std::string_view community_base_url,
                                            std::string_view folder_object_id);

    static MagicCheckResult ValidateMagicAndSafety(
        const uint8_t* data,
        size_t len,
        std::string_view filename,
        std::string_view content_type = "");

    static std::string SafeFilename(std::string_view name);
    static std::string SafeRelPath(std::string_view rel_path);

    DownloadResult DownloadFile(FileRecord& file,
                                const std::string& offline_root,
                                std::string_view community_base_url = "");

private:
    PolicyEnforcingTransport& transport_;
    AuthClient& auth_;
    std::string base_url_;
    DownloaderOptions opts_;
    Sanitizer sanitizer_;
    bool has_last_start_ = false;
    int64_t last_start_steady_ms_ = 0;
};

struct FormFieldMeta {
    std::string tag;
    std::string name;
    std::string type;
    bool required = false;
    bool disabled = false;
    bool hidden = false;
    std::string autocomplete;
};

struct DiscoveredFormMeta {
    std::string page_url;
    std::string action;
    std::string method = "GET";
    std::string enctype;
    std::vector<FormFieldMeta> fields;
};

struct CrawledPageSummary {
    std::string url;
    std::string page_key;
    int depth = 0;
    int http_status = 0;
    std::string content_type;
    std::string title;
    size_t links_found = 0;
    size_t forms_found = 0;
};

struct CrawlerOptions {
    std::string base_url = "https://tecdigital.tec.ac.cr";
    size_t max_pages = 150;
    int max_depth = 6;
    size_t max_folders = 400;
    int delay_ms = 350;
    int saturation_rounds = 3;
    int consec_5xx_break = 5;
    int calendar_date_cap = 8;
    int cal_item_new_cap = 3;
    int one_comm_date_cap = 2;
    int member_profile_cap = 40;
    bool download_discovered_files = true;
    std::vector<std::string> seed_urls;
    std::vector<std::string> seed_folder_ids;
};

struct CrawlSummary {
    size_t pages_visited = 0;
    size_t pages_failed = 0;
    size_t pages_skipped_policy = 0;
    size_t pages_skipped_cap = 0;
    bool session_expired = false;
    bool circuit_broken = false;
    bool saturated = false;
    std::vector<CrawledPageSummary> pages;
    std::vector<FolderRecord> folders;
    std::vector<FileRecord> files;
    std::vector<FolderUrlLink> url_links;
    std::vector<DiscoveredFormMeta> forms;
    std::vector<std::string> discovered_folder_ids;
    std::vector<std::string> discovered_file_api_ids;
    std::vector<std::string> warnings;
};

class HeadlessCrawler {
public:
    HeadlessCrawler(PolicyEnforcingTransport& transport,
                    AuthClient& auth,
                    CrawlerOptions opts = {},
                    SanitizeOptions sanitize_opts = {});

    static std::vector<std::string> DefaultSeeds(std::string_view base_url);
    static std::string HtmlUnescape(std::string_view sv);
    static bool IsDownloadLink(std::string_view canonical_url);
    static bool ShouldSkipPrefix(std::string_view path);

    Result<CrawlSummary> Run(const std::string& offline_root = "",
                             StorageDownloader* optional_downloader = nullptr);

private:
    PolicyEnforcingTransport& transport_;
    AuthClient& auth_;
    CrawlerOptions opts_;
    SanitizeOptions sanitize_opts_;
    Sanitizer sanitizer_;
};

class MirrorManifestStore {
public:
    explicit MirrorManifestStore(SanitizeOptions sanitize_opts = {});

    VoidResult Save(const std::string& offline_root,
                    const std::vector<FileRecord>& files,
                    const std::vector<FolderRecord>& folders,
                    const std::vector<DiscoveredFormMeta>& forms,
                    const std::vector<FolderUrlLink>& url_links = {}) const;

    Result<std::vector<FileRecord>> LoadFilesManifest(const std::string& offline_root) const;

private:
    Sanitizer sanitizer_;
};

}  // namespace academic::tec_digital
