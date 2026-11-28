#include "collaboration_publish_sync.hpp"
#include "utf8_path.hpp"
#include "utf_conv.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio {

namespace {

std::string MakeSlug(const std::string& titleUtf8) {
    std::string slug;
    slug.reserve(titleUtf8.size());
    for (unsigned char c : titleUtf8) {
        if (std::isalnum(c)) {
            slug.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!slug.empty() && slug.back() != '-') slug.push_back('-');
        }
    }
    while (!slug.empty() && slug.back() == '-') slug.pop_back();
    return slug.empty() ? "nota" : slug;
}

std::string EscapeHtmlSimple(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o.push_back(c); break;
        }
    }
    return o;
}

std::string BytesToHex(const std::uint8_t* data, std::size_t len) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.reserve(len * 2u);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kHex[(data[i] >> 4) & 0x0Fu]);
        out.push_back(kHex[data[i] & 0x0Fu]);
    }
    return out;
}

bool HexToBytes(const std::string& hex, std::vector<std::uint8_t>& out) {
    out.clear();
    if (hex.empty() || (hex.size() % 2u) != 0) return false;
    auto hv = [](char ch) -> int {
        if (ch >= '0' && ch <= '9') return ch - '0';
        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
        return -1;
    };
    out.resize(hex.size() / 2u);
    for (std::size_t i = 0; i < out.size(); ++i) {
        const int hi = hv(hex[2 * i]);
        const int lo = hv(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return true;
}

bool ConstantTimeEquals(const std::uint8_t* a, const std::uint8_t* b, std::size_t n) noexcept {
    std::uint8_t diff = 0;
    for (std::size_t i = 0; i < n; ++i) {
        diff |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return diff == 0;
}

bool ReadFileBytes(const fs::path& p, std::vector<std::uint8_t>& outBytes) {
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    const auto sz = ifs.tellg();
    if (sz < 0) return false;
    outBytes.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (sz > 0 && !ifs.read(reinterpret_cast<char*>(outBytes.data()), sz)) return false;
    return true;
}

bool WriteFileBytes(const fs::path& p, const std::vector<std::uint8_t>& bytes) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;
    if (!bytes.empty()) {
        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    ofs.flush();
    return ofs.good();
}

// Safely back up a file into backupRoot before overwriting or removing it, verifying bytes match.
bool BackupFileVerified(const fs::path& srcFile, const fs::path& backupDst) {
    std::vector<std::uint8_t> orig;
    if (!ReadFileBytes(srcFile, orig)) return false;
    if (!WriteFileBytes(backupDst, orig)) return false;
    std::vector<std::uint8_t> check;
    return ReadFileBytes(backupDst, check) && check == orig;
}

bool SafeRemoveAfterBackup(const fs::path& targetFile, const fs::path& backupDst) {
    if (!fs::exists(targetFile)) return true;
    if (!BackupFileVerified(targetFile, backupDst)) return false;
#ifdef _WIN32
    // Try Windows Recycle Bin first (non-permanent deletion); fall back to relocating if path is outside Recycle Bin scope
    std::wstring wPath = targetFile.wstring();
    wPath.push_back(L'\0');
    wPath.push_back(L'\0');
    SHFILEOPSTRUCTW op{};
    op.wFunc = FO_DELETE;
    op.pFrom = wPath.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    if (SHFileOperationW(&op) == 0 && !op.fAnyOperationsAborted && !fs::exists(targetFile)) {
        return true;
    }
#endif
    // Verified copy already exists at backupDst; unlink original path safely
    std::error_code ec;
    return fs::remove(targetFile, ec);
}

std::string ResolveWikilinksToHtmlAnchors(
    const std::string& htmlBody,
    const std::unordered_map<std::string, std::string>& foldedTitleToFilename)
{
    std::string out = htmlBody;
    std::size_t pos = 0;
    while ((pos = out.find("[[", pos)) != std::string::npos) {
        const auto end = out.find("]]", pos + 2);
        if (end == std::string::npos) break;
        const std::string inner = out.substr(pos + 2, end - (pos + 2));
        std::string target = inner;
        std::string label = inner;
        const auto bar = inner.find('|');
        if (bar != std::string::npos) {
            target = inner.substr(0, bar);
            label = inner.substr(bar + 1);
        }
        const auto hash = target.find('#');
        std::string anchorSuffix;
        if (hash != std::string::npos) {
            anchorSuffix = "#" + MakeSlug(target.substr(hash + 1));
            target = target.substr(0, hash);
        }
        const std::string key = FoldUtf8ForSearch(target, false, true);
        std::string replacement;
        auto it = foldedTitleToFilename.find(key);
        if (it != foldedTitleToFilename.end()) {
            replacement = "<a class=\"wikilink\" href=\"" + it->second + anchorSuffix + "\">" + EscapeHtmlSimple(label) + "</a>";
        } else {
            replacement = "<span class=\"wikilink-private\" title=\"Nota privada o no publicada\">" + EscapeHtmlSimple(label) + "</span>";
        }
        out.replace(pos, (end + 2) - pos, replacement);
        pos += replacement.size();
    }
    return out;
}

} // namespace

// ---------------- AnchoredCommentThreadManager ----------------

std::vector<std::string> AnchoredCommentThreadManager::ExtractAtMentions(const std::string& textUtf8) {
    std::vector<std::string> mentions;
    for (std::size_t i = 0; i < textUtf8.size(); ++i) {
        if (textUtf8[i] == '@' && i + 1 < textUtf8.size() &&
            (std::isalnum(static_cast<unsigned char>(textUtf8[i + 1])) || textUtf8[i + 1] == '_')) {
            std::size_t j = i + 1;
            while (j < textUtf8.size() &&
                   (std::isalnum(static_cast<unsigned char>(textUtf8[j])) || textUtf8[j] == '_' || textUtf8[j] == '.')) {
                ++j;
            }
            mentions.push_back(textUtf8.substr(i + 1, j - (i + 1)));
            i = j - 1;
        }
    }
    return mentions;
}

std::uint64_t AnchoredCommentThreadManager::CreateThread(
    std::size_t pageIndex0,
    std::uint64_t anchorObjectId,
    DocRect anchorRect,
    const std::string& authorUtf8,
    const std::string& commentBodyUtf8,
    std::uint64_t timestampSec)
{
    if (commentBodyUtf8.empty()) return 0;
    AnchoredCommentThread th;
    th.threadId = nextThreadId_++;
    th.pageIndex0 = pageIndex0;
    th.anchorObjectId = anchorObjectId;
    th.anchorRect = anchorRect;

    CommentReply rep;
    rep.replyId = nextReplyId_++;
    rep.authorUtf8 = authorUtf8.empty() ? "Autor" : authorUtf8;
    rep.bodyUtf8 = commentBodyUtf8;
    rep.mentionsUtf8 = ExtractAtMentions(commentBodyUtf8);
    rep.timestampSec = timestampSec;
    th.replies.push_back(std::move(rep));

    auditLog_.push_back("create_thread#" + std::to_string(th.threadId) + " by " + rep.authorUtf8);
    threads_.push_back(std::move(th));
    return threads_.back().threadId;
}

bool AnchoredCommentThreadManager::AddReply(
    std::uint64_t threadId,
    const std::string& authorUtf8,
    const std::string& replyBodyUtf8,
    std::uint64_t timestampSec)
{
    if (replyBodyUtf8.empty()) return false;
    for (auto& th : threads_) {
        if (th.threadId == threadId) {
            CommentReply rep;
            rep.replyId = nextReplyId_++;
            rep.authorUtf8 = authorUtf8.empty() ? "Colaborador" : authorUtf8;
            rep.bodyUtf8 = replyBodyUtf8;
            rep.mentionsUtf8 = ExtractAtMentions(replyBodyUtf8);
            rep.timestampSec = timestampSec;
            th.replies.push_back(std::move(rep));
            auditLog_.push_back("reply_thread#" + std::to_string(threadId) + " by " + rep.authorUtf8);
            return true;
        }
    }
    return false;
}

bool AnchoredCommentThreadManager::ResolveThread(std::uint64_t threadId, const std::string& actorUtf8) {
    for (auto& th : threads_) {
        if (th.threadId == threadId) {
            th.resolved = true;
            th.resolvedByUtf8 = actorUtf8;
            auditLog_.push_back("resolve_thread#" + std::to_string(threadId) + " by " + actorUtf8);
            return true;
        }
    }
    return false;
}

bool AnchoredCommentThreadManager::ReopenThread(std::uint64_t threadId, const std::string& actorUtf8) {
    for (auto& th : threads_) {
        if (th.threadId == threadId) {
            th.resolved = false;
            th.resolvedByUtf8.clear();
            auditLog_.push_back("reopen_thread#" + std::to_string(threadId) + " by " + actorUtf8);
            return true;
        }
    }
    return false;
}

const AnchoredCommentThread* AnchoredCommentThreadManager::FindThread(std::uint64_t threadId) const {
    for (const auto& th : threads_) {
        if (th.threadId == threadId) return &th;
    }
    return nullptr;
}

std::vector<AnchoredCommentThread> AnchoredCommentThreadManager::ActiveThreads() const {
    std::vector<AnchoredCommentThread> out;
    for (const auto& th : threads_) {
        if (!th.resolved) out.push_back(th);
    }
    return out;
}

std::vector<AnchoredCommentThread> AnchoredCommentThreadManager::ResolvedThreads() const {
    std::vector<AnchoredCommentThread> out;
    for (const auto& th : threads_) {
        if (th.resolved) out.push_back(th);
    }
    return out;
}

std::string AnchoredCommentThreadManager::SerializeToJson() const {
    json root;
    root["nextThreadId"] = nextThreadId_;
    root["nextReplyId"] = nextReplyId_;
    root["auditLog"] = auditLog_;
    json arr = json::array();
    for (const auto& th : threads_) {
        json reps = json::array();
        for (const auto& r : th.replies) {
            reps.push_back({
                {"replyId", r.replyId},
                {"author", r.authorUtf8},
                {"body", r.bodyUtf8},
                {"mentions", r.mentionsUtf8},
                {"timestampSec", r.timestampSec}
            });
        }
        arr.push_back({
            {"threadId", th.threadId},
            {"pageIndex0", th.pageIndex0},
            {"anchorObjectId", th.anchorObjectId},
            {"anchorRect", {th.anchorRect.x, th.anchorRect.y, th.anchorRect.w, th.anchorRect.h}},
            {"resolved", th.resolved},
            {"resolvedBy", th.resolvedByUtf8},
            {"replies", std::move(reps)}
        });
    }
    root["threads"] = std::move(arr);
    return root.dump(2);
}

bool AnchoredCommentThreadManager::DeserializeFromJson(const std::string& jsonUtf8) {
    try {
        const json root = json::parse(jsonUtf8);
        if (!root.is_object()) return false;
        threads_.clear();
        auditLog_.clear();
        nextThreadId_ = root.value("nextThreadId", std::uint64_t{ 1 });
        nextReplyId_ = root.value("nextReplyId", std::uint64_t{ 1 });
        if (root.contains("auditLog") && root["auditLog"].is_array()) {
            for (const auto& a : root["auditLog"]) {
                if (a.is_string()) auditLog_.push_back(a.get<std::string>());
            }
        }
        if (root.contains("threads") && root["threads"].is_array()) {
            for (const auto& tj : root["threads"]) {
                AnchoredCommentThread th;
                th.threadId = tj.value("threadId", std::uint64_t{ 0 });
                th.pageIndex0 = tj.value("pageIndex0", std::size_t{ 0 });
                th.anchorObjectId = tj.value("anchorObjectId", std::uint64_t{ 0 });
                if (tj.contains("anchorRect") && tj["anchorRect"].is_array() && tj["anchorRect"].size() >= 4) {
                    th.anchorRect = {
                        tj["anchorRect"][0].get<double>(),
                        tj["anchorRect"][1].get<double>(),
                        tj["anchorRect"][2].get<double>(),
                        tj["anchorRect"][3].get<double>()
                    };
                }
                th.resolved = tj.value("resolved", false);
                th.resolvedByUtf8 = tj.value("resolvedBy", "");
                if (tj.contains("replies") && tj["replies"].is_array()) {
                    for (const auto& rj : tj["replies"]) {
                        CommentReply rep;
                        rep.replyId = rj.value("replyId", std::uint64_t{ 0 });
                        rep.authorUtf8 = rj.value("author", "Autor");
                        rep.bodyUtf8 = rj.value("body", "");
                        rep.timestampSec = rj.value("timestampSec", std::uint64_t{ 0 });
                        if (rj.contains("mentions") && rj["mentions"].is_array()) {
                            for (const auto& m : rj["mentions"]) {
                                if (m.is_string()) rep.mentionsUtf8.push_back(m.get<std::string>());
                            }
                        } else {
                            rep.mentionsUtf8 = ExtractAtMentions(rep.bodyUtf8);
                        }
                        th.replies.push_back(std::move(rep));
                    }
                }
                if (th.threadId >= nextThreadId_) nextThreadId_ = th.threadId + 1;
                threads_.push_back(std::move(th));
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------- CollaborationPermissionManager (Real PBKDF2-HMAC-SHA256 via CNG) ----------------

void CollaborationPermissionManager::SetUserRole(const std::string& userIdUtf8, CollaborationRole role) {
    if (!userIdUtf8.empty()) userRoles_[userIdUtf8] = role;
}

CollaborationRole CollaborationPermissionManager::GetUserRole(const std::string& userIdUtf8) const {
    auto it = userRoles_.find(userIdUtf8);
    return (it != userRoles_.end()) ? it->second : CollaborationRole::Viewer;
}

void CollaborationPermissionManager::SetPagePrivate(std::size_t pageIndex0, bool isPrivate) {
    privatePages_[pageIndex0] = isPrivate;
}

bool CollaborationPermissionManager::IsPagePrivate(std::size_t pageIndex0) const {
    auto it = privatePages_.find(pageIndex0);
    return (it != privatePages_.end()) && it->second;
}

void CollaborationPermissionManager::SetColumnRestrictedToOwner(const std::string& columnIdUtf8, bool restricted) {
    ownerOnlyColumns_[columnIdUtf8] = restricted;
}

bool CollaborationPermissionManager::CanUserEditColumn(const std::string& userIdUtf8, const std::string& columnIdUtf8) const {
    if (!CanUserEditDocument(userIdUtf8)) return false;
    auto it = ownerOnlyColumns_.find(columnIdUtf8);
    if (it != ownerOnlyColumns_.end() && it->second) {
        return GetUserRole(userIdUtf8) == CollaborationRole::Owner;
    }
    return true;
}

bool CollaborationPermissionManager::LockNoteWithPassphrase(const std::string& passphraseUtf8) {
    if (passphraseUtf8.empty()) return false;
    std::uint8_t salt[CudocCrypto::kSaltLen]{};
    std::uint8_t derived[CudocCrypto::kKeyLen]{};
    if (!CudocCrypto::FillRandomBytes(salt, CudocCrypto::kSaltLen)) return false;
    if (!CudocCrypto::DeriveKeyPbkdf2(passphraseUtf8, salt, CudocCrypto::kSaltLen, derived)) return false;

    lockPassphraseHashUtf8_ = "PBKDF2_SHA256$" + std::to_string(CudocCrypto::kPbkdf2Iters) + "$" +
        BytesToHex(salt, CudocCrypto::kSaltLen) + "$" +
        BytesToHex(derived, CudocCrypto::kKeyLen);

    // Zero sensitive derived buffer in memory
    volatile std::uint8_t* p = derived;
    for (std::size_t i = 0; i < CudocCrypto::kKeyLen; ++i) p[i] = 0;

    noteLocked_ = true;
    // Deactivate any existing share links when note is locked
    for (auto& sl : shareLinks_) sl.active = false;
    return true;
}

bool CollaborationPermissionManager::UnlockNoteWithPassphrase(const std::string& passphraseUtf8) {
    if (!noteLocked_) return true;
    if (passphraseUtf8.empty()) return false;

    // Parse PBKDF2_SHA256$<iters>$<saltHex>$<hashHex>
    const std::string prefix = "PBKDF2_SHA256$";
    if (lockPassphraseHashUtf8_.rfind(prefix, 0) != 0) return false;
    const auto p1 = lockPassphraseHashUtf8_.find('$', prefix.size());
    if (p1 == std::string::npos) return false;
    const auto p2 = lockPassphraseHashUtf8_.find('$', p1 + 1);
    if (p2 == std::string::npos) return false;

    const std::string saltHex = lockPassphraseHashUtf8_.substr(p1 + 1, p2 - (p1 + 1));
    const std::string expectedHex = lockPassphraseHashUtf8_.substr(p2 + 1);

    std::vector<std::uint8_t> saltBytes;
    std::vector<std::uint8_t> expectedKey;
    if (!HexToBytes(saltHex, saltBytes) || !HexToBytes(expectedHex, expectedKey)) return false;
    if (saltBytes.size() != CudocCrypto::kSaltLen || expectedKey.size() != CudocCrypto::kKeyLen) return false;

    std::uint8_t candidateKey[CudocCrypto::kKeyLen]{};
    if (!CudocCrypto::DeriveKeyPbkdf2(passphraseUtf8, saltBytes.data(), saltBytes.size(), candidateKey)) {
        return false;
    }

    const bool matched = ConstantTimeEquals(candidateKey, expectedKey.data(), CudocCrypto::kKeyLen);
    volatile std::uint8_t* p = candidateKey;
    for (std::size_t i = 0; i < CudocCrypto::kKeyLen; ++i) p[i] = 0;

    if (matched) {
        noteLocked_ = false;
        return true;
    }
    return false;
}

bool CollaborationPermissionManager::CanUserEditDocument(const std::string& userIdUtf8) const {
    if (planDegradedReadOnly_ || noteLocked_) return false;
    const auto r = GetUserRole(userIdUtf8);
    return r == CollaborationRole::Editor || r == CollaborationRole::Owner;
}

bool CollaborationPermissionManager::CanUserCommentDocument(const std::string& userIdUtf8) const {
    if (noteLocked_) return false;
    const auto r = GetUserRole(userIdUtf8);
    return r == CollaborationRole::Commenter || r == CollaborationRole::Editor || r == CollaborationRole::Owner;
}

ShareLinkToken CollaborationPermissionManager::CreateShareLink(
    const std::string& targetIdUtf8,
    bool isPublic,
    CollaborationRole role)
{
    ShareLinkToken tok;
    std::uint8_t rnd[12]{};
    if (CudocCrypto::FillRandomBytes(rnd, sizeof(rnd))) {
        tok.tokenIdUtf8 = "cuadernu_share_" + BytesToHex(rnd, sizeof(rnd));
    } else {
        tok.tokenIdUtf8 = "cuadernu_share_" + std::to_string(nextTokenSeq_++);
    }
    tok.targetIdUtf8 = targetIdUtf8;
    tok.isPublic = isPublic;
    tok.grantedRole = role;
    tok.active = !noteLocked_;
    shareLinks_.push_back(tok);
    return tok;
}

bool CollaborationPermissionManager::RevokeShareLink(const std::string& tokenIdUtf8) {
    for (auto& s : shareLinks_) {
        if (s.tokenIdUtf8 == tokenIdUtf8) {
            s.active = false;
            return true;
        }
    }
    return false;
}

const ShareLinkToken* CollaborationPermissionManager::FindShareLink(const std::string& tokenIdUtf8) const {
    for (const auto& s : shareLinks_) {
        if (s.tokenIdUtf8 == tokenIdUtf8 && s.active) return &s;
    }
    return nullptr;
}

std::string CollaborationPermissionManager::SerializeToJson() const {
    json root;
    root["planDegradedReadOnly"] = planDegradedReadOnly_;
    root["noteLocked"] = noteLocked_;
    root["lockPassphraseHash"] = lockPassphraseHashUtf8_; // salted PBKDF2 hash ONLY
    root["nextTokenSeq"] = nextTokenSeq_;
    json roles = json::object();
    for (const auto& [uid, r] : userRoles_) roles[uid] = static_cast<int>(r);
    root["userRoles"] = std::move(roles);
    json priv = json::object();
    for (const auto& [pg, isP] : privatePages_) priv[std::to_string(pg)] = isP;
    root["privatePages"] = std::move(priv);
    json cols = json::object();
    for (const auto& [cid, r] : ownerOnlyColumns_) cols[cid] = r;
    root["ownerOnlyColumns"] = std::move(cols);
    json links = json::array();
    for (const auto& sl : shareLinks_) {
        links.push_back({
            {"tokenId", sl.tokenIdUtf8},
            {"targetId", sl.targetIdUtf8},
            {"isPublic", sl.isPublic},
            {"grantedRole", static_cast<int>(sl.grantedRole)},
            {"active", sl.active}
        });
    }
    root["shareLinks"] = std::move(links);
    return root.dump(2);
}

bool CollaborationPermissionManager::DeserializeFromJson(const std::string& jsonUtf8) {
    try {
        const json root = json::parse(jsonUtf8);
        if (!root.is_object()) return false;
        userRoles_.clear();
        privatePages_.clear();
        ownerOnlyColumns_.clear();
        shareLinks_.clear();
        planDegradedReadOnly_ = root.value("planDegradedReadOnly", false);
        noteLocked_ = root.value("noteLocked", false);
        lockPassphraseHashUtf8_ = root.value("lockPassphraseHash", "");
        nextTokenSeq_ = root.value("nextTokenSeq", std::uint64_t{ 1 });
        if (root.contains("userRoles") && root["userRoles"].is_object()) {
            for (auto it = root["userRoles"].begin(); it != root["userRoles"].end(); ++it) {
                userRoles_[it.key()] = static_cast<CollaborationRole>(std::clamp(it.value().get<int>(), 0, 3));
            }
        }
        if (root.contains("privatePages") && root["privatePages"].is_object()) {
            for (auto it = root["privatePages"].begin(); it != root["privatePages"].end(); ++it) {
                try { privatePages_[std::stoull(it.key())] = it.value().get<bool>(); } catch (...) {}
            }
        }
        if (root.contains("ownerOnlyColumns") && root["ownerOnlyColumns"].is_object()) {
            for (auto it = root["ownerOnlyColumns"].begin(); it != root["ownerOnlyColumns"].end(); ++it) {
                ownerOnlyColumns_[it.key()] = it.value().get<bool>();
            }
        }
        if (root.contains("shareLinks") && root["shareLinks"].is_array()) {
            for (const auto& slj : root["shareLinks"]) {
                ShareLinkToken sl;
                sl.tokenIdUtf8 = slj.value("tokenId", "");
                sl.targetIdUtf8 = slj.value("targetId", "");
                sl.isPublic = slj.value("isPublic", false);
                sl.grantedRole = static_cast<CollaborationRole>(std::clamp(slj.value("grantedRole", 0), 0, 3));
                sl.active = slj.value("active", false);
                if (!sl.tokenIdUtf8.empty()) shareLinks_.push_back(std::move(sl));
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------- StaticSitePublisher (Real Markdown -> HTML5 + Wikilink Resolution + Custom CSS) ----------------

StaticPublishReport StaticSitePublisher::PublishVaultToStaticHtml(
    const KnowledgeVault& vault,
    const std::vector<std::string>& allowedNoteIds,
    const std::wstring& outputDir,
    std::string* outErr,
    const std::string& customCssSnippetUtf8)
{
    StaticPublishReport rep;
    if (outputDir.empty()) {
        if (outErr) *outErr = "Directorio de publicacion vacio.";
        return rep;
    }

    const fs::path outRoot(WideToUtf8(outputDir));
    std::error_code ec;
    fs::create_directories(outRoot, ec);

    std::unordered_set<std::string> allowed(allowedNoteIds.begin(), allowedNoteIds.end());

    // First pass: decide which notes are public and map their titles/aliases -> filename for real wikilink resolution
    struct PubEntry {
        const VaultNoteRecord* note = nullptr;
        std::string slug;
        std::string filename;
    };
    std::vector<PubEntry> pubNotes;
    std::unordered_map<std::string, std::string> titleToFile;
    std::unordered_set<std::string> usedSlugs;

    const auto activeNotes = vault.ActiveNotes();
    for (const auto& note : activeNotes) {
        bool publishThis = (allowed.find(note.noteIdUtf8) != allowed.end());
        for (const auto& p : note.properties) {
            if (p.keyUtf8 == "publish" && p.valueUtf8 == "true") publishThis = true;
            if (p.keyUtf8 == "private" && p.valueUtf8 == "true") publishThis = false;
        }
        if (!publishThis) {
            ++rep.skippedPrivateNoteCount;
            continue;
        }
        std::string slug = MakeSlug(note.titleUtf8);
        std::string uniqueSlug = slug;
        int suffix = 2;
        while (usedSlugs.count(uniqueSlug)) {
            uniqueSlug = slug + "-" + std::to_string(suffix++);
        }
        usedSlugs.insert(uniqueSlug);
        const std::string filename = uniqueSlug + ".html";
        titleToFile[FoldUtf8ForSearch(note.titleUtf8, false, true)] = filename;
        for (const auto& al : note.aliasesUtf8) {
            titleToFile[FoldUtf8ForSearch(al, false, true)] = filename;
        }
        pubNotes.push_back({ &note, uniqueSlug, filename });
    }

    const std::string baseStyle =
        "body{font-family:system-ui,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#F6F8FC;color:#181E2A;margin:0;padding:32px;line-height:1.6;}"
        ".shell{max-width:860px;margin:0 auto;background:#FFFFFF;border:1px solid #E2E8F0;border-radius:14px;padding:32px;box-shadow:0 4px 18px rgba(15,23,42,0.05);}"
        "nav{margin-bottom:20px;padding-bottom:12px;border-bottom:1px solid #E2E8F0;font-size:14px;}"
        "a{color:#2563EB;text-decoration:none;}a:hover{text-decoration:underline;}"
        ".wikilink{background:#EFF6FF;padding:1px 6px;border-radius:5px;}"
        ".wikilink-private{color:#64748B;border-bottom:1px dashed #94A3B8;}"
        "pre{background:#0F172A;color:#E2E8F0;padding:14px;border-radius:10px;overflow:auto;}"
        "blockquote,.callout{border-left:3px solid #2563EB;padding:10px 16px;background:#EFF6FF;border-radius:8px;margin:14px 0;}"
        "mark{background:#FEF08A;padding:1px 4px;border-radius:4px;}"
        ".props{display:flex;flex-wrap:wrap;gap:8px;margin-bottom:18px;font-size:12px;}"
        ".prop-tag{background:#F1F5F9;border:1px solid #CBD5E1;border-radius:6px;padding:3px 8px;color:#334155;}"
        + customCssSnippetUtf8;

    std::ostringstream indexHtml;
    indexHtml << "<!DOCTYPE html>\n<html lang=\"es\">\n<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
              << "<title>Sitio Publicado CuadernU</title>\n<style>" << baseStyle << "</style>\n"
              << "</head><body><main class=\"shell\"><h1>Documentaci\xC3\xB3n Publicada</h1>\n"
              << "<p>" << pubNotes.size() << " nota(s) p\xC3\xBA" "blica(s) (" << rep.skippedPrivateNoteCount << " privada(s) omitida(s)).</p><ul>\n";

    json searchIndex = json::array();

    for (const auto& pe : pubNotes) {
        const VaultNoteRecord& note = *pe.note;
        rep.publishedSlugsUtf8.push_back(pe.slug);
        ++rep.publishedNoteCount;

        indexHtml << "<li><a href=\"" << pe.filename << "\">" << EscapeHtmlSimple(note.titleUtf8) << "</a></li>\n";

        // Convert Markdown body into real HTML5 structure via RichBlockDocument
        const RichBlockDocument rbd = RichBlockDocument::FromMarkdown(note.bodyMarkdownUtf8);
        const std::string fullRbdHtml = rbd.ToHtml5("");
        std::string innerBody;
        const auto bOpen = fullRbdHtml.find("<body>");
        const auto bClose = fullRbdHtml.find("</body>");
        if (bOpen != std::string::npos && bClose != std::string::npos && bClose > bOpen + 6) {
            innerBody = fullRbdHtml.substr(bOpen + 6, bClose - (bOpen + 6));
        } else {
            innerBody = "<p>" + EscapeHtmlSimple(note.bodyMarkdownUtf8) + "</p>";
        }
        innerBody = ResolveWikilinksToHtmlAnchors(innerBody, titleToFile);

        std::ostringstream pageHtml;
        pageHtml << "<!DOCTYPE html>\n<html lang=\"es\">\n<head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>"
                 << EscapeHtmlSimple(note.titleUtf8) << "</title>\n"
                 << "<link rel=\"canonical\" href=\"/" << pe.slug << "\">\n"
                 << "<style>" << baseStyle << "</style>\n"
                 << "</head><body><main class=\"shell\">\n<nav><a href=\"index.html\">&larr; Inicio</a></nav>\n"
                 << "<article><h1>" << EscapeHtmlSimple(note.titleUtf8) << "</h1>\n";

        if (!note.properties.empty() || !note.tagsUtf8.empty()) {
            pageHtml << "<div class=\"props\">";
            for (const auto& p : note.properties) {
                if (p.keyUtf8 == "private") continue;
                pageHtml << "<span class=\"prop-tag\"><strong>" << EscapeHtmlSimple(p.keyUtf8) << ":</strong> " << EscapeHtmlSimple(p.valueUtf8) << "</span>";
            }
            for (const auto& t : note.tagsUtf8) {
                pageHtml << "<span class=\"prop-tag\">" << EscapeHtmlSimple(t) << "</span>";
            }
            pageHtml << "</div>\n";
        }
        pageHtml << "<div class=\"content\">\n" << innerBody << "\n</div>\n"
                 << "</article></main></body></html>\n";

        const std::string pageStr = pageHtml.str();
        WriteFileBytes(outRoot / pe.filename, std::vector<std::uint8_t>(pageStr.begin(), pageStr.end()));

        searchIndex.push_back({
            {"slug", pe.slug},
            {"file", pe.filename},
            {"title", note.titleUtf8},
            {"excerpt", rbd.ToReadingPlainText(80).substr(0, 220)}
        });
    }

    indexHtml << "</ul></main></body></html>\n";
    const std::string idxStr = indexHtml.str();
    const fs::path idxPath = outRoot / "index.html";
    const std::string srchStr = searchIndex.dump(2);
    WriteFileBytes(outRoot / "search_index.json", std::vector<std::uint8_t>(srchStr.begin(), srchStr.end()));

    rep.ok = WriteFileBytes(idxPath, std::vector<std::uint8_t>(idxStr.begin(), idxStr.end()));
    rep.indexHtmlPathUtf8 = PathToUtf8(idxPath);
    return rep;
}

// ---------------- LocalFirstSyncEngine (Backup-First Safe Sync) ----------------

namespace {

struct SyncPathLess {
    bool operator()(const fs::path& left, const fs::path& right) const noexcept {
#ifdef _WIN32
        return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_LESS_THAN;
#else
        return left < right;
#endif
    }
};

bool ContainsSyncPath(const fs::path& root, fs::path candidate) {
    const SyncPathLess less;
    for (;;) {
        if (!less(root, candidate) && !less(candidate, root)) return true;
        const auto parent = candidate.parent_path();
        if (parent.empty() || parent == candidate) return false;
        candidate = parent;
    }
}

std::string NewSyncStamp() {
    const auto tick = std::chrono::system_clock::now().time_since_epoch().count();
#ifdef _WIN32
    return std::to_string(tick) + "_" + std::to_string(GetCurrentProcessId());
#else
    return std::to_string(tick);
#endif
}

bool IsInternalSyncPath(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return name.rfind(".cuadernu_sync_backup", 0) == 0 ||
        name.rfind(".cuadernu_sync_tmp_", 0) == 0 ||
        name.find("/.cuadernu_sync_tmp_") != std::string::npos ||
        (name.size() >= 6 && name.compare(name.size() - 6, 6, ".dpapi") == 0);
}

bool SyncFileMatches(const fs::path& path, const std::vector<std::uint8_t>& expected,
                     const std::string& passphrase = {}) {
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytes(path, bytes)) return false;
    if (passphrase.empty()) return bytes == expected;

    std::vector<std::uint8_t> plain;
    return CudocCrypto::DecryptBytes(bytes.data(), bytes.size(), passphrase, plain) && plain == expected;
}

bool WriteSyncFile(const fs::path& destination, const std::vector<std::uint8_t>& bytes,
                   const std::vector<std::uint8_t>* expected = nullptr, const std::string& passphrase = {}) {
    const fs::path stage = destination.parent_path() / (".cuadernu_sync_tmp_" + NewSyncStamp());
    std::error_code ec;
    if (WriteFileBytes(stage, bytes) && (!expected || SyncFileMatches(destination, *expected, passphrase))) {
#ifdef _WIN32
        DWORD flags = MOVEFILE_WRITE_THROUGH;
        if (expected) flags |= MOVEFILE_REPLACE_EXISTING;
        if (MoveFileExW(stage.c_str(), destination.c_str(), flags)) return true;
#else
        if (expected || (!fs::exists(destination, ec) && !ec)) {
            fs::rename(stage, destination, ec);
            if (!ec) return true;
        }
#endif
    }
    fs::remove(stage, ec);
    return false;
}

} // namespace

bool LocalFirstSyncEngine::IsPathAllowedBySelectiveFilter(
    const std::string& relPathUtf8,
    const SelectiveSyncFilter& filter)
{
    if (IsInternalSyncPath(relPathUtf8)) return false;
    for (const auto& pref : filter.excludedFolderPrefixesUtf8) {
        if (!pref.empty() && relPathUtf8.rfind(pref, 0) == 0) return false;
    }
    for (const auto& ext : filter.excludedExtensionsUtf8) {
        if (!ext.empty() && relPathUtf8.size() >= ext.size() &&
            relPathUtf8.compare(relPathUtf8.size() - ext.size(), ext.size(), ext) == 0) {
            return false;
        }
    }
    return true;
}

FolderSyncReport LocalFirstSyncEngine::SynchronizeDirectories(
    const std::wstring& deviceDir,
    const std::wstring& targetSyncDir,
    FolderSyncMode mode,
    const SelectiveSyncFilter& filter)
{
    static std::mutex syncMutex;
    const std::lock_guard<std::mutex> lock(syncMutex);
    FolderSyncReport report;
    const auto fail = [&](const std::string& message, const fs::path& relative = {}) {
        report.diagnosticUtf8 = message;
        if (!relative.empty()) report.diagnosticUtf8 += ": " + PathToGenericUtf8(relative);
        return report;
    };
    if (deviceDir.empty() || targetSyncDir.empty()) return fail("Seleccione ambas carpetas");

    std::error_code ec;
    const fs::path deviceAbsolute = fs::absolute(fs::path(deviceDir), ec);
    if (ec) return fail("No se puede resolver la carpeta del dispositivo");
    fs::path device = fs::weakly_canonical(deviceAbsolute, ec);
    if (ec) return fail("No se puede resolver la carpeta del dispositivo");
    const fs::path targetAbsolute = fs::absolute(fs::path(targetSyncDir), ec);
    if (ec) return fail("No se puede resolver la carpeta de destino");
    fs::path target = fs::weakly_canonical(targetAbsolute, ec);
    if (ec) return fail("No se puede resolver la carpeta de destino");
    if (device.has_relative_path() && !device.has_filename()) device = device.parent_path();
    if (target.has_relative_path() && !target.has_filename()) target = target.parent_path();
    if (ContainsSyncPath(device, target) || ContainsSyncPath(target, device)) {
        return fail("Las carpetas de sincronización deben ser distintas y no contenerse entre sí");
    }

    const bool restore = mode == FolderSyncMode::ReplaceDeviceWithTarget;
    const bool merge = mode == FolderSyncMode::MergeLossless;
    const fs::path& sourceRoot = restore ? target : device;
    const fs::path& destinationRoot = restore ? device : target;
    if (!fs::is_directory(sourceRoot, ec) || ec) return fail("La carpeta de origen no está disponible");
    fs::create_directories(destinationRoot, ec);
    if (ec) return fail("No se puede preparar la carpeta de salida");

    const bool encrypted = !filter.encryptPassphraseUtf8.empty();
    using FileMap = std::map<fs::path, std::vector<std::uint8_t>, SyncPathLess>;
    FileMap localFiles, remoteFiles;
    auto collect = [&](const fs::path& root, bool remote, FileMap& files) {
        fs::recursive_directory_iterator it(root, ec), end;
        for (; !ec && it != end; it.increment(ec)) {
            const fs::path path = it->path();
            fs::path rel = path.lexically_relative(root);
            const std::string name = PathToGenericUtf8(rel);
            if (IsInternalSyncPath(name)) {
                it.disable_recursion_pending();
                continue;
            }
            const auto status = it->symlink_status(ec);
            if (ec) break;
            if (fs::is_symlink(status)) {
                report.diagnosticUtf8 = "Enlace no admitido en la carpeta de sincronización: " + name;
                return false;
            }
            if (!fs::is_regular_file(status)) continue;
            const bool decrypt = remote && encrypted;
            if (decrypt) {
                if (rel.extension() != L".cuenc") {
                    if (!IsPathAllowedBySelectiveFilter(name, filter)) continue;
                    report.diagnosticUtf8 = "El destino cifrado contiene archivos sin cifrar: " + name;
                    return false;
                }
                rel.replace_extension();
            }
            if (!IsPathAllowedBySelectiveFilter(PathToGenericUtf8(rel), filter)) {
                if (merge || remote == restore) ++report.skippedBySelectiveFilter;
                continue;
            }
            std::vector<std::uint8_t> bytes;
            if (!ReadFileBytes(path, bytes)) {
                report.diagnosticUtf8 = "No se pudo leer el archivo: " + name;
                return false;
            }
            if (decrypt) {
                std::vector<std::uint8_t> plain;
                if (!CudocCrypto::DecryptBytes(bytes.data(), bytes.size(), filter.encryptPassphraseUtf8, plain)) {
                    report.diagnosticUtf8 = "No se pudo descifrar el archivo remoto: " + name;
                    return false;
                }
                bytes = std::move(plain);
            }
            if (!files.emplace(rel, std::move(bytes)).second) {
                report.diagnosticUtf8 = "Ruta duplicada en el origen: " + name;
                return false;
            }
        }
        if (ec) report.diagnosticUtf8 = "No se pudo recorrer toda la carpeta: " + ec.message();
        return !ec;
    };
    if (!collect(device, false, localFiles) || !collect(target, true, remoteFiles)) return report;
    if (restore && encrypted && remoteFiles.empty() && !localFiles.empty()) {
        return fail("El origen cifrado no contiene archivos utilizables para restaurar");
    }

    auto remotePath = [&](const fs::path& root, const fs::path& rel) {
        fs::path path = root / rel;
        if (encrypted) path += L".cuenc";
        return path;
    };
    auto writeRemote = [&](const fs::path& rel, const std::vector<std::uint8_t>& plain,
                           const std::vector<std::uint8_t>* expected = nullptr) {
        if (!encrypted) return WriteSyncFile(remotePath(target, rel), plain, expected);
        std::vector<std::uint8_t> cipher;
        return CudocCrypto::EncryptBytes(plain, filter.encryptPassphraseUtf8, cipher) &&
            WriteSyncFile(remotePath(target, rel), cipher, expected, filter.encryptPassphraseUtf8);
    };

    if (!merge) {
        const auto& origin = restore ? remoteFiles : localFiles;
        const auto& destination = restore ? localFiles : remoteFiles;
        const fs::path backupContainer = destinationRoot / ".cuadernu_sync_backup";
        const fs::path resolvedBackup = fs::weakly_canonical(backupContainer, ec);
        const SyncPathLess less;
        if (ec || less(backupContainer, resolvedBackup) || less(resolvedBackup, backupContainer)) {
            return fail("El directorio de respaldos no apunta a la carpeta de sincronización");
        }
        const fs::path backup = backupContainer / NewSyncStamp();
        const std::string destinationPassphrase = restore ? std::string{} : filter.encryptPassphraseUtf8;
        auto destinationPath = [&](const fs::path& root, const fs::path& rel) {
            return restore ? root / rel : remotePath(root, rel);
        };
        for (const auto& [rel, bytes] : origin) {
            const auto existing = destination.find(rel);
            const bool replace = existing != destination.end();
            const fs::path& destName = replace ? existing->first : rel;
            const fs::path path = destinationPath(destinationRoot, destName);
            if (replace) {
                if (existing->second == bytes) continue;
                if (!BackupFileVerified(path, destinationPath(backup / "overwritten", destName))) {
                    return fail("No se pudo respaldar el original; se conserva el archivo de destino", rel);
                }
                report.backupDirUtf8 = PathToUtf8(backup);
                ++report.backedUpBeforeChange;
            }
            const auto* expected = replace ? &existing->second : nullptr;
            const bool written = restore ? WriteSyncFile(path, bytes, expected) : writeRemote(destName, bytes, expected);
            if (!written) return fail("La escritura falló o cambió el archivo de destino; revise y reintente", rel);
            if (restore) ++report.downloadedFiles;
            else ++report.uploadedFiles;
        }
        for (const auto& [rel, bytes] : destination) {
            if (origin.find(rel) != origin.end()) continue;
            const fs::path path = destinationPath(destinationRoot, rel);
            if (!SyncFileMatches(path, bytes, destinationPassphrase)) {
                return fail("El archivo de destino cambió; se conserva para el siguiente intento", rel);
            }
            if (!SafeRemoveAfterBackup(path,
                                       destinationPath(backup / "removed", rel))) {
                return fail("No se pudo retirar el archivo con respaldo verificado", rel);
            }
            report.backupDirUtf8 = PathToUtf8(backup);
            ++report.backedUpBeforeChange;
        }
        report.ok = true;
        report.diagnosticUtf8 = restore ? "Restauración del dispositivo completada." : "Subida completada.";
        return report;
    }

    for (const auto& [rel, bytes] : localFiles) {
        const auto remote = remoteFiles.find(rel);
        if (remote == remoteFiles.end()) {
            if (!writeRemote(rel, bytes)) return fail("No se pudo subir el archivo", rel);
            ++report.uploadedFiles;
            continue;
        }
        if (remote->second == bytes) continue;

        fs::path conflict;
        bool existsLocally = false;
        bool existsRemotely = false;
        for (std::size_t index = 1; ; ++index) {
            const std::wstring suffix = index == 1 ? L"" : L"_" + std::to_wstring(index);
            conflict = rel.parent_path() /
                (rel.stem().wstring() + L"_conflicto_remoto" + suffix + rel.extension().wstring());
            const auto previousLocal = localFiles.find(conflict);
            const auto previousRemote = remoteFiles.find(conflict);
            existsLocally = previousLocal != localFiles.end();
            existsRemotely = previousRemote != remoteFiles.end();
            if ((!existsLocally || previousLocal->second == remote->second) &&
                (!existsRemotely || previousRemote->second == remote->second)) break;
        }
        if ((!existsLocally && !WriteSyncFile(device / conflict, remote->second)) ||
            (!existsRemotely && !writeRemote(conflict, remote->second))) {
            return fail("No se pudo conservar la versión remota del conflicto", rel);
        }
        if (!writeRemote(remote->first, bytes, &remote->second)) {
            return fail("La versión remota cambió o no se pudo escribir; las dos copias se conservan", rel);
        }
        ++report.mergedConflicts;
    }
    for (const auto& [rel, bytes] : remoteFiles) {
        if (localFiles.find(rel) != localFiles.end()) continue;
        const fs::path path = device / rel;
        std::vector<std::uint8_t> alreadyCopied;
        if (ReadFileBytes(path, alreadyCopied) && alreadyCopied == bytes) continue;
        if (!WriteSyncFile(path, bytes)) return fail("No se pudo descargar el archivo", rel);
        ++report.downloadedFiles;
    }
    report.ok = true;
    report.diagnosticUtf8 = "Sincronización bidireccional completada.";
    return report;
}

bool LocalFirstSyncEngine::CreateBackupPackage(
    const std::wstring& sourceDir,
    const std::wstring& backupPackagePath,
    const std::string& optionalPassphraseUtf8,
    std::string* outErr)
{
    const fs::path srcRoot(WideToUtf8(sourceDir));
    if (!fs::exists(srcRoot)) {
        if (outErr) *outErr = "Directorio origen para respaldo no existe.";
        return false;
    }

    ZipArchive zip;
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(srcRoot, ec)) {
        if (!entry.is_regular_file()) continue;
        const std::string rel = PathToGenericUtf8(fs::relative(entry.path(), srcRoot, ec));
        if (rel.rfind(".cuadernu_sync_backup", 0) == 0) continue;
        std::vector<std::uint8_t> bytes;
        if (ReadFileBytes(entry.path(), bytes)) {
            zip.SetEntry(rel, std::move(bytes));
        }
    }

    std::vector<std::uint8_t> payload;
    if (!zip.Serialize(payload, true, outErr)) return false;

    if (!optionalPassphraseUtf8.empty()) {
        std::vector<std::uint8_t> enc;
        if (!CudocCrypto::EncryptBytes(payload, optionalPassphraseUtf8, enc, outErr)) return false;
        payload = std::move(enc);
    }

    return WriteFileBytes(fs::path(WideToUtf8(backupPackagePath)), payload);
}

bool LocalFirstSyncEngine::RestoreFullBackupPackage(
    const std::wstring& backupPackagePath,
    const std::wstring& restoreTargetDir,
    const std::string& optionalPassphraseUtf8,
    std::string* outErr)
{
    std::vector<std::uint8_t> raw;
    if (!ReadFileBytes(fs::path(WideToUtf8(backupPackagePath)), raw)) {
        if (outErr) *outErr = "No se pudo leer archivo de respaldo.";
        return false;
    }

    std::vector<std::uint8_t> zipBytes;
    if (CudocCrypto::IsEncryptedCudocBlob(raw.data(), raw.size())) {
        if (!CudocCrypto::DecryptBytes(raw.data(), raw.size(), optionalPassphraseUtf8, zipBytes, outErr)) return false;
    } else {
        zipBytes = std::move(raw);
    }

    ZipArchive zip;
    if (!zip.Deserialize(zipBytes.data(), zipBytes.size(), outErr)) return false;

    const fs::path dstRoot(WideToUtf8(restoreTargetDir));
    for (const auto& name : zip.ListEntries()) {
        if (const auto* data = zip.FindEntry(name)) {
            if (!WriteFileBytes(dstRoot / name, *data)) return false;
        }
    }
    return true;
}

bool LocalFirstSyncEngine::RestoreSingleNoteFromBackup(
    const std::wstring& backupPackagePath,
    const std::string& noteRelativePathUtf8,
    const std::wstring& outSingleFilePath,
    const std::string& optionalPassphraseUtf8,
    std::string* outErr)
{
    std::vector<std::uint8_t> raw;
    if (!ReadFileBytes(fs::path(WideToUtf8(backupPackagePath)), raw)) return false;

    std::vector<std::uint8_t> zipBytes;
    if (CudocCrypto::IsEncryptedCudocBlob(raw.data(), raw.size())) {
        if (!CudocCrypto::DecryptBytes(raw.data(), raw.size(), optionalPassphraseUtf8, zipBytes, outErr)) return false;
    } else {
        zipBytes = std::move(raw);
    }

    ZipArchive zip;
    if (!zip.Deserialize(zipBytes.data(), zipBytes.size(), outErr)) return false;

    const auto* data = zip.FindEntry(noteRelativePathUtf8);
    if (!data) {
        if (outErr) *outErr = "Nota solicitada no encontrada dentro del respaldo.";
        return false;
    }
    return WriteFileBytes(fs::path(WideToUtf8(outSingleFilePath)), *data);
}

} // namespace DocStudio
