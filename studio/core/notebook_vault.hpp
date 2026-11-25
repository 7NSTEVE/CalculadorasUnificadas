#pragma once

#include "doc_types.hpp"
#include "document_model.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

// -------- Hierarchical Notebook Vault (Workspace -> Notebook -> Section -> Page) --------

struct VaultPageEntry {
    std::uint64_t id = 0;
    std::string titleUtf8;
    std::string cudocRelPathUtf8;
    std::uint64_t updatedEpochSec = 0;
};

struct VaultSection {
    std::uint64_t id = 0;
    std::string titleUtf8;
    std::uint32_t colorRgba = 0x3B82F6FFu;
    std::vector<VaultPageEntry> pages;
};

struct VaultNotebook {
    std::uint64_t id = 0;
    std::string titleUtf8;
    std::uint32_t coverColorRgba = 0x1E40AFu;
    std::vector<VaultSection> sections;
};

class NotebookVault {
public:
    std::uint64_t CreateNotebook(const std::string& titleUtf8, std::uint32_t coverRgba = 0x1D4ED8FFu);
    std::uint64_t CreateSection(std::uint64_t notebookId, const std::string& titleUtf8, std::uint32_t colorRgba = 0x2563EBFFu);
    std::uint64_t AddPageEntry(std::uint64_t notebookId, std::uint64_t sectionId, const std::string& titleUtf8, const std::string& cudocRelPathUtf8);

    const std::vector<VaultNotebook>& Notebooks() const noexcept { return notebooks_; }
    bool MovePageBetweenSections(std::uint64_t pageEntryId, std::uint64_t dstNotebookId, std::uint64_t dstSectionId);

    bool SaveToFile(const std::wstring& vaultJsonPath, std::string* outErr = nullptr) const;
    bool LoadFromFile(const std::wstring& vaultJsonPath, std::string* outErr = nullptr);

private:
    std::uint64_t nextId_ = 1;
    std::vector<VaultNotebook> notebooks_;
};

// -------- Crash-Safe Write-Ahead Log (.cudoc-wal) --------

struct WalRecord {
    std::string opTag;       // "add_obj", "erase_obj", "modify_obj", "set_title"
    std::string payloadJson; // compact JSON delta
};

class CudocWal {
public:
    static bool AppendRecord(const std::wstring& walPath, const WalRecord& rec, std::string* outErr = nullptr);
    static bool ReadValidRecords(const std::wstring& walPath, std::vector<WalRecord>& outRecords, std::string* outErr = nullptr);
    static bool Truncate(const std::wstring& walPath);

    // Loads base .cudoc (with .bak fallback) and replays any uncommitted WAL frames atomically
    static bool RecoverDocument(
        const std::wstring& cudocPath,
        const std::wstring& walPath,
        DocumentModel& outDoc,
        std::size_t* outReplayedCount = nullptr,
        std::string* outErr = nullptr);
};

// -------- Pure C++17 Unicode Inverted Full-Text Search Index --------

struct FtsPosting {
    std::string docKeyUtf8;
    std::size_t pageIndex = 0;
    std::uint64_t objectId = 0; // 0 = page/OCR text
    std::string sourceKindUtf8; // "pdf", "ocr", "textbox", "math", "calc"
    std::string snippetUtf8;
};

struct FtsHit {
    std::string docKeyUtf8;
    std::size_t pageIndex = 0;
    std::uint64_t objectId = 0;
    std::string sourceKindUtf8;
    std::string snippetUtf8;
    int score = 0;
};

class VaultFullTextIndex {
public:
    void RemoveDocument(const std::string& docKeyUtf8);
    void IndexDocument(const std::string& docKeyUtf8, const DocumentModel& doc);
    std::vector<FtsHit> Query(const std::string& queryUtf8, std::size_t maxResults = 64) const;

    std::size_t IndexedDocumentCount() const noexcept { return indexedDocs_.size(); }
    std::size_t VocabularySize() const noexcept { return postings_.size(); }

    bool SaveToFile(const std::wstring& indexPath, std::string* outErr = nullptr) const;
    bool LoadFromFile(const std::wstring& indexPath, std::string* outErr = nullptr);

private:
    std::unordered_map<std::string, std::vector<FtsPosting>> postings_;
    std::unordered_map<std::string, std::size_t> indexedDocs_;
};

// -------- Flashcards / Study Sets (SM-2 Spaced Repetition) --------

struct FlashcardItem {
    std::uint64_t id = 0;
    std::string questionUtf8;
    std::string answerUtf8;
    std::size_t sourcePageIndex = 0;
    DocRect sourceRect{};
    int repetitions = 0;
    int intervalDays = 0;
    double easeFactor = 2.5;
    std::uint64_t dueEpochSec = 0;
};

struct StudyDeck {
    std::string titleUtf8 = "Study Set CASIO";
    std::vector<FlashcardItem> cards;
};

class StudySetManager {
public:
    // SM-2 spaced repetition grade q in [0..5]
    static void ReviewCardSm2(FlashcardItem& card, int quality0to5, std::uint64_t nowEpochSec);

    // Harvests StudyTape cards, MathBlock derivations, CalcSessionCard steps, and Q:/A: text boxes
    static StudyDeck BuildDeckFromDocument(const DocumentModel& doc, const std::string& deckTitle = "");

    static std::size_t CountDueCards(const StudyDeck& deck, std::uint64_t nowEpochSec);
};

// -------- Tag Summary Board, Page Version Snapshots & Conflict Page Merge --------

struct TaggedItemHit {
    bool isCanvas = false;
    std::size_t pageIndex = 0;
    std::uint64_t objectId = 0;
    std::string tagUtf8;
    std::string summaryUtf8;
};

std::vector<TaggedItemHit> CollectTaggedItems(const DocumentModel& doc, const std::string& tagFilterUtf8 = "");

struct PageVersionSnapshot {
    std::uint64_t versionId = 0;
    std::size_t pageIndex = 0;
    std::uint64_t timestampSec = 0;
    std::string authorOrLabelUtf8;
    PageModel snapshotPage;
};

class PageVersionHistory {
public:
    std::uint64_t CaptureSnapshot(const DocumentModel& doc, std::size_t pageIndex, std::uint64_t timestampSec, const std::string& labelUtf8 = "Auto");
    std::vector<PageVersionSnapshot> SnapshotsForPage(std::size_t pageIndex) const;
    bool RestoreSnapshot(DocumentModel& doc, std::uint64_t versionId) const;

private:
    std::uint64_t nextVerId_ = 1;
    std::vector<PageVersionSnapshot> snapshots_;
};

struct ConflictMergeResult {
    bool ok = true;
    int mergedCleanPages = 0;
    int conflictPagesCreated = 0;
    std::vector<std::string> conflictDescriptions;
};

// OneNote-style side-by-side Conflict Page merge (preserves all divergent edits without overwriting)
ConflictMergeResult MergeConcurrentDocuments(
    const DocumentModel& baseDoc,
    const DocumentModel& localDoc,
    const DocumentModel& remoteDoc,
    DocumentModel& outMerged);

// -------- M6: Library Organization, Nested Folders, Tags, Content Views & Smart Folders --------

struct LibraryFolderNode {
    std::uint64_t folderId = 0;
    std::uint64_t parentFolderId = 0; // 0 = root
    std::string nameUtf8;
    int priorityOrder = 0;
};

struct LibraryItemRecord {
    std::uint64_t itemId = 0;
    std::uint64_t folderId = 0;
    std::string titleUtf8;
    std::string kindUtf8 = "notebook"; // "notebook", "pdf", "continuous", "canvas", "quick_note"
    std::uint32_t coverColorRgba = 0x1D4ED8FFu;
    std::string coverArtworkTagUtf8;   // e.g. "custom_sketch", "minimal_grid"
    bool hasCoverInkDrawing = false;
    PaperPattern defaultPaper = PaperPattern::Grid;
    bool favorite = false;
    bool pinned = false;
    bool inTrash = false;
    int priorityRank = 0;
    std::uint64_t updatedEpochSec = 0;
    std::vector<std::string> tagsUtf8; // supports nested tags e.g. "#ingenieria/calculo"
    std::size_t photoCount = 0;
    std::size_t recordingCount = 0;
    std::size_t bookmarkCount = 0;
    std::size_t attachmentCount = 0;
    std::size_t wordCount = 0;
    std::string bodySummaryUtf8;
    std::string filePathUtf8; // Real .cudoc / .pdf path on disk
};

struct SmartFolderRule {
    std::string nameUtf8;
    std::string requiredTagPrefixUtf8;
    std::string textQueryUtf8;
    bool onlyFavorites = false;
    bool onlyWithRecordings = false;
    bool onlyWithPhotos = false;
};

class LibraryContentViewsManager {
public:
    std::uint64_t CreateFolder(const std::string& nameUtf8, std::uint64_t parentFolderId = 0, int priorityOrder = 0);
    std::uint64_t AddLibraryItem(LibraryItemRecord item);

    bool RenameItem(std::uint64_t itemId, const std::string& newTitleUtf8);
    bool MoveItemToFolder(std::uint64_t itemId, std::uint64_t targetFolderId);
    bool SetFavorite(std::uint64_t itemId, bool fav);
    bool SetPinned(std::uint64_t itemId, bool pin);
    bool MoveToTrash(std::uint64_t itemId);
    bool RestoreFromTrash(std::uint64_t itemId);
    std::uint64_t DuplicateItem(std::uint64_t itemId);
    std::uint64_t MergeItems(std::uint64_t primaryItemId, std::uint64_t secondaryItemId, const std::string& mergedTitleUtf8);

    // Tag & Nested Tag Management (#parent/child)
    bool AssignTag(std::uint64_t itemId, const std::string& tagUtf8);
    std::size_t RenameTagAcrossLibrary(const std::string& oldTagPrefixUtf8, const std::string& newTagPrefixUtf8);
    std::vector<LibraryItemRecord> QueryByTag(const std::string& tagPrefixUtf8) const;

    // Cross-Notebook Content Views (Noteshelf 3 / Notas del iPad / Goodnotes / Notability / Flexcil)
    std::vector<LibraryItemRecord> QueryPhotosContentView() const;
    std::vector<LibraryItemRecord> QueryRecordingsContentView() const;
    std::vector<LibraryItemRecord> QueryBookmarksContentView() const;
    std::vector<LibraryItemRecord> QueryAttachmentsContentView() const;
    std::vector<LibraryItemRecord> QueryFavoritesAndPinned() const;
    std::vector<LibraryItemRecord> QueryRecentItems(std::size_t limit = 20) const;
    std::vector<LibraryItemRecord> QueryTrashItems() const;
    std::vector<LibraryItemRecord> QueryByKind(const std::string& kindUtf8) const;
    std::vector<LibraryItemRecord> EvaluateSmartFolder(const SmartFolderRule& rule) const;

    const std::vector<LibraryFolderNode>& Folders() const noexcept { return folders_; }
    const std::vector<LibraryItemRecord>& Items() const noexcept { return items_; }
    const LibraryItemRecord* FindItem(std::uint64_t itemId) const;

    // Auto-index any opened/saved DocumentModel with real photo/audio/bookmark/attachment/tag counts
    std::uint64_t UpsertFromDocument(const DocumentModel& doc, const std::string& filePathUtf8);

    // Persistent library storage (JSON)
    std::string SerializeToJson() const;
    bool DeserializeFromJson(const std::string& jsonUtf8);
    bool SaveToFile(const std::wstring& path, std::string* outErr = nullptr) const;
    bool LoadFromFile(const std::wstring& path, std::string* outErr = nullptr);

    void AddSmartFolder(SmartFolderRule rule) { smartFolders_.push_back(std::move(rule)); }
    const std::vector<SmartFolderRule>& SmartFolders() const noexcept { return smartFolders_; }

private:
    std::uint64_t nextFolderId_ = 1;
    std::uint64_t nextItemId_ = 1;
    std::vector<LibraryFolderNode> folders_;
    std::vector<LibraryItemRecord> items_;
    std::vector<SmartFolderRule> smartFolders_;
};

} // namespace DocStudio
