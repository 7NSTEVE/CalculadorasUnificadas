#pragma once

#include "cudoc_crypto.hpp"
#include "cudoc_storage.hpp"
#include "doc_types.hpp"
#include "document_model.hpp"
#include "knowledge_graph.hpp"
#include "notebook_vault.hpp"
#include "zip_container.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

// ---------------- Anchored Comment Threads & @Mentions (COL / EDIT) ----------------

struct CommentReply {
    std::uint64_t replyId = 0;
    std::string authorUtf8;
    std::string bodyUtf8;
    std::vector<std::string> mentionsUtf8;
    std::uint64_t timestampSec = 0;
};

struct AnchoredCommentThread {
    std::uint64_t threadId = 0;
    std::size_t pageIndex0 = 0;
    std::uint64_t anchorObjectId = 0;
    DocRect anchorRect{};
    bool resolved = false;
    std::string resolvedByUtf8;
    std::vector<CommentReply> replies;
};

class AnchoredCommentThreadManager {
public:
    std::uint64_t CreateThread(
        std::size_t pageIndex0,
        std::uint64_t anchorObjectId,
        DocRect anchorRect,
        const std::string& authorUtf8,
        const std::string& commentBodyUtf8,
        std::uint64_t timestampSec = 1000);

    bool AddReply(
        std::uint64_t threadId,
        const std::string& authorUtf8,
        const std::string& replyBodyUtf8,
        std::uint64_t timestampSec = 1005);

    bool ResolveThread(std::uint64_t threadId, const std::string& actorUtf8);
    bool ReopenThread(std::uint64_t threadId, const std::string& actorUtf8);

    const AnchoredCommentThread* FindThread(std::uint64_t threadId) const;
    const std::vector<AnchoredCommentThread>& AllThreads() const noexcept { return threads_; }
    std::vector<AnchoredCommentThread> ActiveThreads() const;
    std::vector<AnchoredCommentThread> ResolvedThreads() const;
    const std::vector<std::string>& AuditLog() const noexcept { return auditLog_; }

    std::string SerializeToJson() const;
    bool DeserializeFromJson(const std::string& jsonUtf8);

    static std::vector<std::string> ExtractAtMentions(const std::string& textUtf8);

private:
    std::uint64_t nextThreadId_ = 1;
    std::uint64_t nextReplyId_ = 1;
    std::vector<AnchoredCommentThread> threads_;
    std::vector<std::string> auditLog_;
};

// ---------------- Granular Permissions, Share Links & Password Lock (COL / SYNC) ----------------

enum class CollaborationRole : std::uint8_t {
    Viewer = 0,
    Commenter = 1,
    Editor = 2,
    Owner = 3
};

struct ShareLinkToken {
    std::string tokenIdUtf8;
    std::string targetIdUtf8;
    bool isPublic = false;
    CollaborationRole grantedRole = CollaborationRole::Viewer;
    bool active = true;
};

class CollaborationPermissionManager {
public:
    void SetUserRole(const std::string& userIdUtf8, CollaborationRole role);
    CollaborationRole GetUserRole(const std::string& userIdUtf8) const;

    void SetReadOnlyPlanDegraded(bool degraded) noexcept { planDegradedReadOnly_ = degraded; }
    bool IsReadOnlyPlanDegraded() const noexcept { return planDegradedReadOnly_; }

    void SetPagePrivate(std::size_t pageIndex0, bool isPrivate);
    bool IsPagePrivate(std::size_t pageIndex0) const;

    void SetColumnRestrictedToOwner(const std::string& columnIdUtf8, bool restricted);
    bool CanUserEditColumn(const std::string& userIdUtf8, const std::string& columnIdUtf8) const;

    bool LockNoteWithPassphrase(const std::string& passphraseUtf8);
    bool UnlockNoteWithPassphrase(const std::string& passphraseUtf8);
    bool IsNoteLocked() const noexcept { return noteLocked_; }
    const std::string& StoredPassphraseHash() const noexcept { return lockPassphraseHashUtf8_; }

    // Locked notes cannot be shared or edited by collaborators (AP-COL-076)
    bool CanUserEditDocument(const std::string& userIdUtf8) const;
    bool CanUserCommentDocument(const std::string& userIdUtf8) const;

    ShareLinkToken CreateShareLink(const std::string& targetIdUtf8, bool isPublic, CollaborationRole role);
    bool RevokeShareLink(const std::string& tokenIdUtf8);
    const ShareLinkToken* FindShareLink(const std::string& tokenIdUtf8) const;
    const std::vector<ShareLinkToken>& ShareLinks() const noexcept { return shareLinks_; }

    std::string SerializeToJson() const;
    bool DeserializeFromJson(const std::string& jsonUtf8);

private:
    std::unordered_map<std::string, CollaborationRole> userRoles_;
    std::unordered_map<std::size_t, bool> privatePages_;
    std::unordered_map<std::string, bool> ownerOnlyColumns_;
    std::vector<ShareLinkToken> shareLinks_;
    bool planDegradedReadOnly_ = false;
    bool noteLocked_ = false;
    std::string lockPassphraseHashUtf8_;
    std::uint64_t nextTokenSeq_ = 1;
};

// ---------------- Static Site Publisher (Obsidian Publish / Public Gallery Local Equivalent) ----------------

struct StaticPublishReport {
    bool ok = false;
    std::size_t publishedNoteCount = 0;
    std::size_t skippedPrivateNoteCount = 0;
    std::vector<std::string> publishedSlugsUtf8;
    std::string indexHtmlPathUtf8;
};

class StaticSitePublisher {
public:
    // Publishes ONLY notes explicitly listed in allowedNoteIds (or having frontmatter publish: true).
    // Renders full Markdown -> HTML5 with resolved [[wikilinks]], optional custom CSS snippets,
    // and guarantees zero leakage of private unselected notes to outputDir.
    static StaticPublishReport PublishVaultToStaticHtml(
        const KnowledgeVault& vault,
        const std::vector<std::string>& allowedNoteIds,
        const std::wstring& outputDir,
        std::string* outErr = nullptr,
        const std::string& customCssSnippetUtf8 = "");
};

// ---------------- Local-First 3-Mode Sync, Selective Sync & Backup/Restore (SYNC) ----------------

enum class FolderSyncMode : std::uint8_t {
    UploadDeviceToTarget = 0,   // Device overwrites target folder (FX-SYNC-073)
    ReplaceDeviceWithTarget = 1,// Target folder overwrites device (FX-SYNC-074)
    MergeLossless = 2           // Union + conflict-page merge on concurrent modifications (FX-SYNC-075, OB-SYNC-081, NO-SYNC-096)
};

struct SelectiveSyncFilter {
    std::vector<std::string> excludedFolderPrefixesUtf8;
    std::vector<std::string> excludedExtensionsUtf8; // e.g. ".tmp", ".mp4"
    bool wifiOnlyPolicy = true;
    bool cellularAllowed = false;
    std::string encryptPassphraseUtf8; // Optional AES-256-GCM E2EE passphrase for synced target files (.cuenc)
};

struct FolderSyncReport {
    bool ok = false;
    std::size_t uploadedFiles = 0;
    std::size_t downloadedFiles = 0;
    std::size_t mergedConflicts = 0;
    std::size_t skippedBySelectiveFilter = 0;
    std::size_t backedUpBeforeChange = 0;
    std::string backupDirUtf8;
    std::string diagnosticUtf8;
};

class LocalFirstSyncEngine {
public:
    static bool IsPathAllowedBySelectiveFilter(
        const std::string& relPathUtf8,
        const SelectiveSyncFilter& filter);

    static FolderSyncReport SynchronizeDirectories(
        const std::wstring& deviceDir,
        const std::wstring& targetSyncDir,
        FolderSyncMode mode,
        const SelectiveSyncFilter& filter = {});

    // Full vault backup archive (.cuadernu-backup ZIP) + full restore + single-note surgical recovery (MS-SYNC-071..073, NS-SYNC-072..073)
    static bool CreateBackupPackage(
        const std::wstring& sourceDir,
        const std::wstring& backupPackagePath,
        const std::string& optionalPassphraseUtf8 = "",
        std::string* outErr = nullptr);

    static bool RestoreFullBackupPackage(
        const std::wstring& backupPackagePath,
        const std::wstring& restoreTargetDir,
        const std::string& optionalPassphraseUtf8 = "",
        std::string* outErr = nullptr);

    static bool RestoreSingleNoteFromBackup(
        const std::wstring& backupPackagePath,
        const std::string& noteRelativePathUtf8,
        const std::wstring& outSingleFilePath,
        const std::string& optionalPassphraseUtf8 = "",
        std::string* outErr = nullptr);
};

} // namespace DocStudio
