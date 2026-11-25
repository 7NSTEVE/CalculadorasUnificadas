#pragma once

#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/types.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

enum class MaterialKind {
    Pdf,
    ZipFamily,
    Ole,
    BinaryMedia,
    Text,
    Unknown
};

enum class ExtractStatus {
    Full,
    Partial,
    Skipped,
    Failed,
    MissingBlob
};

struct MaterialExtractResult {
    MaterialKind kind = MaterialKind::Unknown;
    std::string kind_detail;
    ExtractStatus status = ExtractStatus::Failed;
    std::optional<std::string> text;
    std::string detail;
};

struct ZipEntryInfo {
    std::string name;
    uint16_t method = 0;
    uint16_t flags = 0;
    uint32_t crc32 = 0;
    uint32_t compressed_size = 0;
    uint32_t uncompressed_size = 0;
    uint32_t local_header_offset = 0;
};

class InflateCodec {
public:
    static Result<std::vector<uint8_t>> InflateRawDeflate(
        const uint8_t* data,
        size_t len,
        size_t max_output_bytes = 64ULL * 1024ULL * 1024ULL);

    static Result<std::vector<uint8_t>> InflateZlib(
        const uint8_t* data,
        size_t len,
        size_t max_output_bytes = 64ULL * 1024ULL * 1024ULL);

    static Result<std::vector<uint8_t>> InflateGzip(
        const uint8_t* data,
        size_t len,
        size_t max_output_bytes = 64ULL * 1024ULL * 1024ULL);
};

class ZipReader {
public:
    static Result<std::vector<ZipEntryInfo>> ListEntries(const uint8_t* data, size_t len);
    static Result<std::vector<uint8_t>> ExtractEntry(
        const uint8_t* data,
        size_t len,
        const ZipEntryInfo& entry,
        size_t max_entry_bytes = 64ULL * 1024ULL * 1024ULL);
};

class MaterialExtractor {
public:
    static MaterialKind DetectFormat(std::string_view filename,
                                     const uint8_t* prefix,
                                     size_t prefix_len);
    static std::string KindToString(MaterialKind kind);
    static std::string StatusToString(ExtractStatus status);
    static std::optional<std::string> ExpectedKindForExtension(std::string_view ext);
    static std::string CleanText(std::string_view text);

    static MaterialExtractResult Extract(
        std::string_view filename,
        const uint8_t* data,
        size_t len,
        size_t max_entry_bytes = 64ULL * 1024ULL * 1024ULL);

    static MaterialExtractResult Extract(
        std::string_view filename,
        const std::vector<uint8_t>& bytes,
        size_t max_entry_bytes = 64ULL * 1024ULL * 1024ULL);
};

struct IndexedBlobInput {
    std::string object_id;
    std::string name;
    std::string community;
    std::string course;
    std::string folder_path;
    std::string sha256;
    std::string blob_rel_path;
    std::string logical_rel_path;
    std::string source_url;
    uint64_t size_bytes = 0;
    std::vector<uint8_t> inline_bytes;
};

struct IndexedDocument {
    std::string object_id;
    std::string name;
    std::string ext;
    std::string community;
    std::string course;
    std::string folder_path;
    std::string sha256;
    std::string blob_rel_path;
    std::string text_rel_path;
    MaterialKind magic_kind = MaterialKind::Unknown;
    std::string kind_detail;
    ExtractStatus status = ExtractStatus::Failed;
    std::string detail;
    size_t chars_extracted = 0;
    uint64_t size_bytes = 0;
    bool magic_ext_mismatch = false;
};

struct MagicExtMismatch {
    std::string object_id;
    std::string name;
    std::string ext;
    std::string expected_kind;
    std::string actual_kind;
};

struct OfflineMaterialStats {
    std::string generated_at_utc;
    size_t total = 0;
    size_t missing_blobs = 0;
    std::map<std::string, size_t> by_status;
    std::map<std::string, size_t> by_ext;
    std::map<std::string, size_t> by_magic_kind;
    uint64_t chars_extracted_total = 0;
    std::vector<MagicExtMismatch> magic_vs_ext_mismatches;
};

struct MaterialSearchFilters {
    std::string community;
    std::string course;
    std::string ext;
    std::optional<MaterialKind> kind;
    std::optional<ExtractStatus> status;
    size_t max_results = 50;
};

struct MaterialSearchHit {
    IndexedDocument document;
    int score = 0;
    std::string snippet;
};

class OfflineMaterialIndex {
public:
    explicit OfflineMaterialIndex(std::string offline_root = "",
                                  SanitizeOptions sanitize_opts = {});

    void SetOfflineRoot(std::string offline_root);
    const std::string& OfflineRoot() const { return offline_root_; }
    void SetSanitizeOptions(SanitizeOptions sanitize_opts);
    Sanitizer SnapshotSanitizer() const;

    VoidResult IndexBlob(const IndexedBlobInput& input);
    Result<OfflineMaterialStats> IndexDirectory(const std::string& offline_root = "");

    VoidResult LoadFromDisk(const std::string& offline_root = "");
    VoidResult SaveToDisk(const std::string& offline_root = "") const;

    std::vector<MaterialSearchHit> SearchOfflineMaterials(
        std::string_view query,
        const MaterialSearchFilters& filters = {}) const;

    OfflineMaterialStats GetOfflineStats() const;
    const std::vector<IndexedDocument>& Documents() const { return documents_; }

private:
    std::string offline_root_;
    mutable std::mutex sanitizer_mutex_;
    Sanitizer sanitizer_;
    std::vector<IndexedDocument> documents_;
    std::map<std::string, std::string> text_by_doc_key_;
    size_t missing_blobs_count_ = 0;
    std::vector<MagicExtMismatch> mismatches_;
};

}  // namespace academic::tec_digital
