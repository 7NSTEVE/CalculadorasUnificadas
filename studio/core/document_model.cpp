#include "document_model.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <random>
#include <sstream>

namespace DocStudio {

namespace {

bool DecodeUtf8Char(const std::string& s, std::size_t& pos, std::uint32_t& outCp) {
    if (pos >= s.size()) return false;
    const std::uint8_t b0 = static_cast<std::uint8_t>(s[pos]);
    if ((b0 & 0x80u) == 0u) {
        outCp = b0;
        pos += 1;
        return true;
    }
    if ((b0 & 0xE0u) == 0xC0u && pos + 1 < s.size()) {
        const std::uint8_t b1 = static_cast<std::uint8_t>(s[pos + 1]);
        if ((b1 & 0xC0u) == 0x80u) {
            outCp = ((b0 & 0x1Fu) << 6) | (b1 & 0x3Fu);
            pos += 2;
            return true;
        }
    }
    if ((b0 & 0xF0u) == 0xE0u && pos + 2 < s.size()) {
        const std::uint8_t b1 = static_cast<std::uint8_t>(s[pos + 1]);
        const std::uint8_t b2 = static_cast<std::uint8_t>(s[pos + 2]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            outCp = ((b0 & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
            pos += 3;
            return true;
        }
    }
    if ((b0 & 0xF8u) == 0xF0u && pos + 3 < s.size()) {
        const std::uint8_t b1 = static_cast<std::uint8_t>(s[pos + 1]);
        const std::uint8_t b2 = static_cast<std::uint8_t>(s[pos + 2]);
        const std::uint8_t b3 = static_cast<std::uint8_t>(s[pos + 3]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            outCp = ((b0 & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) | ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
            pos += 4;
            return true;
        }
    }
    outCp = b0;
    pos += 1;
    return true;
}

std::uint32_t LowerUnicodeCodepoint(std::uint32_t cp) noexcept {
    if (cp >= 'A' && cp <= 'Z') return cp + 32u;
    if (cp >= 0x00C0u && cp <= 0x00DEu && cp != 0x00D7u) return cp + 0x20u;
    if (cp >= 0x0100u && cp <= 0x012Eu && (cp % 2u) == 0u) return cp + 1u;
    if (cp == 0x0130u) return 'i';
    if (cp >= 0x0132u && cp <= 0x0136u && (cp % 2u) == 0u) return cp + 1u;
    if (cp >= 0x0139u && cp <= 0x0147u && (cp % 2u) == 1u) return cp + 1u;
    if (cp >= 0x014Au && cp <= 0x0176u && (cp % 2u) == 0u) return cp + 1u;
    if (cp == 0x0178u) return 0x00FFu;
    if (cp >= 0x0179u && cp <= 0x017Du && (cp % 2u) == 1u) return cp + 1u;
    if (cp == 0x0386u) return 0x03ACu;
    if (cp >= 0x0388u && cp <= 0x038Au) return cp + 0x25u;
    if (cp == 0x038Cu) return 0x03CCu;
    if (cp == 0x038Eu || cp == 0x038Fu) return cp + 0x3Fu;
    if (cp >= 0x0391u && cp <= 0x03ABu && cp != 0x03A2u) return cp + 0x20u;
    if (cp == 0x03C2u) return 0x03C3u;
    if (cp >= 0x0400u && cp <= 0x040Fu) return cp + 0x50u;
    if (cp >= 0x0410u && cp <= 0x042Fu) return cp + 0x20u;
    return cp;
}

std::uint32_t StripLatinDiacritic(std::uint32_t cp) noexcept {
    switch (cp) {
        case 0x00E0u: case 0x00E1u: case 0x00E2u: case 0x00E3u: case 0x00E4u: case 0x00E5u: return 'a';
        case 0x00C0u: case 0x00C1u: case 0x00C2u: case 0x00C3u: case 0x00C4u: case 0x00C5u: return 'A';
        case 0x00E7u: return 'c';
        case 0x00C7u: return 'C';
        case 0x00E8u: case 0x00E9u: case 0x00EAu: case 0x00EBu: return 'e';
        case 0x00C8u: case 0x00C9u: case 0x00CAu: case 0x00CBu: return 'E';
        case 0x00ECu: case 0x00EDu: case 0x00EEu: case 0x00EFu: return 'i';
        case 0x00CCu: case 0x00CDu: case 0x00CEu: case 0x00CFu: return 'I';
        case 0x00F2u: case 0x00F3u: case 0x00F4u: case 0x00F5u: case 0x00F6u: return 'o';
        case 0x00D2u: case 0x00D3u: case 0x00D4u: case 0x00D5u: case 0x00D6u: return 'O';
        case 0x00F9u: case 0x00FAu: case 0x00FBu: case 0x00FCu: return 'u';
        case 0x00D9u: case 0x00DAu: case 0x00DBu: case 0x00DCu: return 'U';
        case 0x00FDu: case 0x00FFu: return 'y';
        case 0x00DDu: case 0x0178u: return 'Y';
        case 0x00F1u: return 'n';
        case 0x00D1u: return 'N';
        default: return cp;
    }
}

struct NormalizedCodepoints {
    std::vector<std::uint32_t> cps;
    std::vector<std::size_t> srcStartOffsets; // utf8 byte start for each cp
    std::vector<std::size_t> srcEndOffsets;   // utf8 byte end for each cp
};

NormalizedCodepoints NormalizeTextForSearch(const std::string& utf8, bool caseSensitive, bool accentInsensitive) {
    NormalizedCodepoints out;
    out.cps.reserve(utf8.size());
    out.srcStartOffsets.reserve(utf8.size());
    out.srcEndOffsets.reserve(utf8.size());

    std::size_t pos = 0;
    while (pos < utf8.size()) {
        const std::size_t cpStart = pos;
        std::uint32_t cp = 0;
        if (!DecodeUtf8Char(utf8, pos, cp)) break;
        const std::size_t cpEnd = pos;
        if (cp >= 0x0300u && cp <= 0x036Fu && accentInsensitive) continue; // combining diacritics

        auto pushMapped = [&](std::uint32_t c) {
            if (!caseSensitive) c = LowerUnicodeCodepoint(c);
            if (accentInsensitive) {
                c = StripLatinDiacritic(c);
                if (!caseSensitive) c = LowerUnicodeCodepoint(c);
            }
            out.cps.push_back(c);
            out.srcStartOffsets.push_back(cpStart);
            out.srcEndOffsets.push_back(cpEnd);
        };

        switch (cp) {
            case 0xFB00u: pushMapped('f'); pushMapped('f'); break;
            case 0xFB01u: pushMapped('f'); pushMapped('i'); break;
            case 0xFB02u: pushMapped('f'); pushMapped('l'); break;
            case 0xFB03u: pushMapped('f'); pushMapped('f'); pushMapped('i'); break;
            case 0xFB04u: pushMapped('f'); pushMapped('f'); pushMapped('l'); break;
            case 0xFB05u: pushMapped('f'); pushMapped('t'); break;
            case 0xFB06u: pushMapped('s'); pushMapped('t'); break;
            default: pushMapped(cp); break;
        }
    }
    return out;
}

std::string SliceUtf8SafeSnippet(const std::string& utf8, std::size_t matchStart, std::size_t matchEnd) {
    std::size_t snipStart = (matchStart > 32u) ? (matchStart - 32u) : 0u;
    while (snipStart < matchStart && snipStart < utf8.size() &&
           (static_cast<std::uint8_t>(utf8[snipStart]) & 0xC0u) == 0x80u) {
        ++snipStart;
    }
    std::size_t snipEnd = (std::min)(utf8.size(), matchEnd + 40u);
    while (snipEnd < utf8.size() &&
           (static_cast<std::uint8_t>(utf8[snipEnd]) & 0xC0u) == 0x80u) {
        ++snipEnd;
    }
    return utf8.substr(snipStart, snipEnd - snipStart);
}

bool ContainsNormalizedMatch(
    const std::string& haystack,
    const NormalizedCodepoints& needleNorm,
    bool caseSensitive,
    bool accentInsensitive);

bool FindMatchingRunBox(
    const std::vector<PdfTextRunBox>& runs,
    const NormalizedCodepoints& needleNorm,
    bool caseSensitive,
    bool accentInsensitive,
    std::size_t& ioRunCursor,
    DocRect& outNormRect)
{
    if (runs.empty() || needleNorm.cps.empty()) return false;
    // 1. Try direct match in runs starting from ioRunCursor, then wrap around
    for (std::size_t pass = 0; pass < 2; ++pass) {
        const std::size_t start = (pass == 0) ? (ioRunCursor % runs.size()) : 0;
        const std::size_t end   = (pass == 0) ? runs.size() : (ioRunCursor % runs.size());
        for (std::size_t r = start; r < end; ++r) {
            if (ContainsNormalizedMatch(runs[r].textUtf8, needleNorm, caseSensitive, accentInsensitive) &&
                runs[r].rectNorm.w > 0.0 && runs[r].rectNorm.h > 0.0) {
                outNormRect = runs[r].rectNorm;
                ioRunCursor = r + 1;
                return true;
            }
        }
    }
    // 2. If needle has multiple words (e.g. "Clave-137 termodinamica"), match first token >= 3 codepoints
    NormalizedCodepoints firstTok;
    for (std::size_t k = 0; k < needleNorm.cps.size(); ++k) {
        const std::uint32_t c = needleNorm.cps[k];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (firstTok.cps.size() >= 2) break;
            firstTok.cps.clear();
            firstTok.srcStartOffsets.clear();
            firstTok.srcEndOffsets.clear();
        } else {
            firstTok.cps.push_back(c);
            firstTok.srcStartOffsets.push_back(0);
            firstTok.srcEndOffsets.push_back(1);
        }
    }
    if (firstTok.cps.size() >= 2 && firstTok.cps.size() < needleNorm.cps.size()) {
        for (std::size_t r = 0; r < runs.size(); ++r) {
            if (ContainsNormalizedMatch(runs[r].textUtf8, firstTok, caseSensitive, accentInsensitive) &&
                runs[r].rectNorm.w > 0.0 && runs[r].rectNorm.h > 0.0) {
                outNormRect = runs[r].rectNorm;
                ioRunCursor = r + 1;
                return true;
            }
        }
    }
    return false;
}

void SearchInText(
    std::size_t pageIdx,
    const std::string& haystack,
    const NormalizedCodepoints& needleNorm,
    bool caseSensitive,
    bool accentInsensitive,
    std::vector<SearchMatch>& out,
    const std::vector<PdfTextRunBox>* runs = nullptr)
{
    if (haystack.empty() || needleNorm.cps.empty()) return;
    const NormalizedCodepoints hayNorm = NormalizeTextForSearch(haystack, caseSensitive, accentInsensitive);
    if (hayNorm.cps.size() < needleNorm.cps.size()) return;

    std::size_t runCursor = 0;
    const std::size_t nLen = needleNorm.cps.size();
    for (std::size_t i = 0; i + nLen <= hayNorm.cps.size(); ) {
        bool match = true;
        for (std::size_t j = 0; j < nLen; ++j) {
            if (hayNorm.cps[i + j] != needleNorm.cps[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            const std::size_t byteStart = hayNorm.srcStartOffsets[i];
            const std::size_t byteEnd = hayNorm.srcEndOffsets[i + nLen - 1];
            SearchMatch m{};
            m.pageIndex = pageIdx;
            m.charOffset = byteStart;
            m.charLength = byteEnd - byteStart;
            m.snippetUtf8 = SliceUtf8SafeSnippet(haystack, byteStart, byteEnd);
            if (runs && FindMatchingRunBox(*runs, needleNorm, caseSensitive, accentInsensitive, runCursor, m.boxNormalized)) {
                m.hasBox = true;
            }
            out.push_back(std::move(m));
            i += (std::max<std::size_t>)(1u, nLen);
        } else {
            ++i;
        }
    }
}

bool ContainsNormalizedMatch(
    const std::string& haystack,
    const NormalizedCodepoints& needleNorm,
    bool caseSensitive,
    bool accentInsensitive)
{
    if (haystack.empty() || needleNorm.cps.empty()) return false;
    const NormalizedCodepoints hayNorm = NormalizeTextForSearch(haystack, caseSensitive, accentInsensitive);
    if (hayNorm.cps.size() < needleNorm.cps.size()) return false;
    const std::size_t nLen = needleNorm.cps.size();
    for (std::size_t i = 0; i + nLen <= hayNorm.cps.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < nLen; ++j) {
            if (hayNorm.cps[i + j] != needleNorm.cps[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }
    return false;
}

} // namespace

std::string DocumentModel::GenerateDurableUuid() {
    static std::atomic<std::uint64_t> sSeq{ 1 };
    const auto nowNs = static_cast<std::uint64_t>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    std::random_device rd;
    const std::uint64_t r1 = (static_cast<std::uint64_t>(rd()) << 32) ^ static_cast<std::uint64_t>(rd()) ^ nowNs;
    const std::uint64_t r2 = (static_cast<std::uint64_t>(rd()) << 32) ^ sSeq.fetch_add(1, std::memory_order_relaxed);
    char buf[48]{};
    std::snprintf(buf, sizeof(buf), "cu-%016llx-%016llx",
        static_cast<unsigned long long>(r1),
        static_cast<unsigned long long>(r2));
    return std::string(buf);
}

DocumentModel::DocumentModel()
    : canvasIndex_({ -1e7, -1e7, 2e7, 2e7 }, 14, 24)
{
    ResetToBlank(DocumentMode::PaginatedPdf);
}

void DocumentModel::ResetToBlank(DocumentMode mode) {
    mode_ = mode;
    titleUtf8_ = "Documento nuevo";
    documentUuidUtf8_ = GenerateDurableUuid();
    dirty_ = false;
    doublePageSpread_ = false;
    nextObjectId_ = 0;
    nextPageId_ = 0;
    nextZOrder_ = 0;
    nextAssetSeq_ = 0;
    pages_.clear();
    canvasObjects_.clear();
    canvasIdToPos_.clear();
    canvasIndex_.Clear();
    assets_.clear();
    inkLinks_.clear();
    collapsedRanges_.clear();
    bookmarks_.clear();
    attachments_.clear();
    extensionPayloads_.clear();
    doublePageSpread_ = false;

    PageModel p;
    p.pageId = AllocatePageId();
    p.widthPt = 595.0;
    p.heightPt = 842.0;
    p.background = PaperPattern::Plain;
    pages_.push_back(std::move(p));
    RebuildLayout();
    BumpRevision();
}

void DocumentModel::RebuildLayout() {
    std::vector<bool> collapsedMask(pages_.size(), false);
    bool anyCollapsed = false;
    for (std::size_t i = 0; i < pages_.size(); ++i) {
        if (IsPageCollapsed(static_cast<int>(i))) {
            collapsedMask[i] = true;
            anyCollapsed = true;
        }
    }
    layout_.Rebuild(pages_, 18.0, 16.0, anyCollapsed ? &collapsedMask : nullptr, doublePageSpread_);
    BumpRevision();
}

void DocumentModel::SetDoublePageSpread(bool enabled) {
    if (doublePageSpread_ == enabled) return;
    doublePageSpread_ = enabled;
    RebuildLayout();
}

std::string DocumentModel::AddAttachment(
    const std::string& filenameUtf8,
    const std::string& mimeTypeUtf8,
    std::vector<std::uint8_t> bytes)
{
    const std::size_t sz = bytes.size();
    const std::string key = RegisterAsset(std::move(bytes));
    EmbeddedAttachment att;
    att.filenameUtf8 = filenameUtf8.empty() ? (key + ".bin") : filenameUtf8;
    att.mimeTypeUtf8 = mimeTypeUtf8.empty() ? "application/octet-stream" : mimeTypeUtf8;
    att.assetKey = key;
    att.sizeBytes = sz;
    attachments_.push_back(std::move(att));
    dirty_ = true;
    BumpRevision();
    return key;
}

bool DocumentModel::RemoveAttachment(const std::string& assetKey) {
    for (std::size_t i = 0; i < attachments_.size(); ++i) {
        if (attachments_[i].assetKey == assetKey) {
            attachments_.erase(attachments_.begin() + static_cast<std::ptrdiff_t>(i));
            dirty_ = true;
            BumpRevision();
            return true;
        }
    }
    return false;
}

void DocumentModel::RestoreCounters(
    std::uint64_t nextObj,
    std::uint64_t nextPage,
    std::uint64_t nextZ,
    std::uint64_t nextAsset) noexcept
{
    if (nextObj > nextObjectId_) nextObjectId_ = nextObj;
    if (nextPage > nextPageId_) nextPageId_ = nextPage;
    if (nextZ > nextZOrder_) nextZOrder_ = nextZ;
    if (nextAsset > nextAssetSeq_) nextAssetSeq_ = nextAsset;
}

std::string DocumentModel::RegisterAsset(std::vector<std::uint8_t> bytes) {
    const std::string key = "asset_" + std::to_string(++nextAssetSeq_);
    assets_[key] = std::move(bytes);
    return key;
}

void DocumentModel::SetNamedAsset(const std::string& key, std::vector<std::uint8_t> bytes) {
    assets_[key] = std::move(bytes);
}

const std::vector<std::uint8_t>* DocumentModel::FindAsset(const std::string& key) const {
    auto it = assets_.find(key);
    return (it == assets_.end()) ? nullptr : &it->second;
}

void DocumentModel::AddCanvasObject(AnnotationObject obj) {
    if (obj.id == 0) obj.id = AllocateObjectId();
    if (obj.zOrder == 0) obj.zOrder = AllocateZOrder();
    obj.RecomputeBounds();

    auto existing = canvasIdToPos_.find(obj.id);
    if (existing != canvasIdToPos_.end()) {
        canvasObjects_[existing->second] = obj;
        canvasIndex_.Update(obj.id, obj.bounds);
    } else {
        const std::size_t idx = canvasObjects_.size();
        canvasIdToPos_[obj.id] = idx;
        canvasObjects_.push_back(obj);
        canvasIndex_.Insert(obj.id, obj.bounds);
    }
    if (obj.id > nextObjectId_) nextObjectId_ = obj.id;
    if (obj.zOrder > nextZOrder_) nextZOrder_ = obj.zOrder;
    dirty_ = true;
    BumpRevision();
}

bool DocumentModel::RemoveCanvasObject(std::uint64_t id, AnnotationObject* outRemoved) {
    auto it = canvasIdToPos_.find(id);
    if (it == canvasIdToPos_.end()) return false;
    const std::size_t idx = it->second;
    if (outRemoved) *outRemoved = canvasObjects_[idx];

    canvasIndex_.Remove(id);
    const std::size_t lastIdx = canvasObjects_.size() - 1;
    if (idx != lastIdx) {
        canvasObjects_[idx] = std::move(canvasObjects_[lastIdx]);
        canvasIdToPos_[canvasObjects_[idx].id] = idx;
    }
    canvasObjects_.pop_back();
    canvasIdToPos_.erase(it);
    dirty_ = true;
    BumpRevision();
    return true;
}

AnnotationObject* DocumentModel::FindCanvasObjectMutable(std::uint64_t id) {
    auto it = canvasIdToPos_.find(id);
    return (it == canvasIdToPos_.end()) ? nullptr : &canvasObjects_[it->second];
}

const AnnotationObject* DocumentModel::FindCanvasObject(std::uint64_t id) const {
    auto it = canvasIdToPos_.find(id);
    return (it == canvasIdToPos_.end()) ? nullptr : &canvasObjects_[it->second];
}

std::vector<std::uint64_t> DocumentModel::QueryCanvasRect(const DocRect& worldRect) const {
    return canvasIndex_.QueryRect(worldRect);
}

void DocumentModel::RebuildCanvasIndex() {
    canvasIndex_.Clear();
    canvasIdToPos_.clear();
    for (std::size_t i = 0; i < canvasObjects_.size(); ++i) {
        canvasObjects_[i].RecomputeBounds();
        canvasIdToPos_[canvasObjects_[i].id] = i;
        canvasIndex_.Insert(canvasObjects_[i].id, canvasObjects_[i].bounds);
        if (canvasObjects_[i].id > nextObjectId_) nextObjectId_ = canvasObjects_[i].id;
        if (canvasObjects_[i].zOrder > nextZOrder_) nextZOrder_ = canvasObjects_[i].zOrder;
    }
}

bool DocumentModel::AddPageObject(std::size_t pageIdx, AnnotationObject obj) {
    if (pageIdx >= pages_.size()) return false;
    if (obj.id == 0) obj.id = AllocateObjectId();
    if (obj.zOrder == 0) obj.zOrder = AllocateZOrder();
    obj.RecomputeBounds();
    if (obj.id > nextObjectId_) nextObjectId_ = obj.id;
    if (obj.zOrder > nextZOrder_) nextZOrder_ = obj.zOrder;
    pages_[pageIdx].objects.push_back(std::move(obj));
    dirty_ = true;
    BumpRevision();
    return true;
}

bool DocumentModel::RemovePageObject(std::size_t pageIdx, std::uint64_t id, AnnotationObject* outRemoved) {
    if (pageIdx >= pages_.size()) return false;
    auto& objs = pages_[pageIdx].objects;
    for (std::size_t i = 0; i < objs.size(); ++i) {
        if (objs[i].id == id) {
            if (outRemoved) *outRemoved = objs[i];
            objs.erase(objs.begin() + static_cast<std::ptrdiff_t>(i));
            dirty_ = true;
            BumpRevision();
            return true;
        }
    }
    return false;
}

AnnotationObject* DocumentModel::FindPageObjectMutable(std::size_t pageIdx, std::uint64_t id) {
    if (pageIdx >= pages_.size()) return nullptr;
    for (auto& o : pages_[pageIdx].objects) {
        if (o.id == id) return &o;
    }
    return nullptr;
}

std::vector<SearchMatch> DocumentModel::SearchText(
    const std::string& queryUtf8,
    bool caseSensitive,
    bool accentInsensitive) const
{
    std::vector<SearchMatch> out;
    if (queryUtf8.empty()) return out;
    const NormalizedCodepoints needleNorm = NormalizeTextForSearch(queryUtf8, caseSensitive, accentInsensitive);
    if (needleNorm.cps.empty()) return out;

    for (std::size_t p = 0; p < pages_.size(); ++p) {
        const auto& pg = pages_[p];
        SearchInText(p, pg.extractedTextUtf8, needleNorm, caseSensitive, accentInsensitive, out, &pg.textBoxes);
        SearchInText(p, pg.ocrTextUtf8, needleNorm, caseSensitive, accentInsensitive, out, &pg.textBoxes);
        for (const auto& obj : pg.objects) {
            std::string searchable;
            if (!obj.textUtf8.empty()) searchable += obj.textUtf8;
            if (!obj.printoutTextUtf8.empty()) {
                if (!searchable.empty()) searchable += ' ';
                searchable += obj.printoutTextUtf8;
            }
            if (!obj.formulaExprUtf8.empty()) {
                if (!searchable.empty()) searchable += ' ';
                searchable += obj.formulaExprUtf8;
            }
            if (!obj.evaluatedResultUtf8.empty()) {
                if (!searchable.empty()) searchable += ' ';
                searchable += obj.evaluatedResultUtf8;
            }
            if (!obj.calcModelIdUtf8.empty()) {
                if (!searchable.empty()) searchable += ' ';
                searchable += obj.calcModelIdUtf8 + " " + obj.calcKeySeqUtf8;
            }
            if (!obj.htrRecognizedTextUtf8.empty()) {
                if (!searchable.empty()) searchable += ' ';
                searchable += obj.htrRecognizedTextUtf8;
            }
            if (!obj.tableJsonUtf8.empty()) {
                if (!searchable.empty()) searchable += ' ';
                searchable += obj.tableJsonUtf8;
            }
            if (!searchable.empty()) {
                if (ContainsNormalizedMatch(searchable, needleNorm, caseSensitive, accentInsensitive)) {
                    SearchMatch m{};
                    m.pageIndex = p;
                    m.objectId = obj.id;
                    m.charOffset = 0;
                    m.charLength = queryUtf8.size();
                    m.snippetUtf8 = searchable;
                    m.hasBox = true;
                    const double pw = (std::max)(1.0, pg.EffectiveWidthPt());
                    const double ph = (std::max)(1.0, pg.EffectiveHeightPt());
                    m.boxNormalized = {
                        obj.bounds.x / pw,
                        obj.bounds.y / ph,
                        obj.bounds.w / pw,
                        obj.bounds.h / ph
                    };
                    out.push_back(std::move(m));
                }
            }
        }
    }

    // Search all Infinite Canvas objects (including PdfCard printouts with embedded text/OCR runs)
    for (const auto& obj : canvasObjects_) {
        if (!obj.printoutTextUtf8.empty()) {
            std::vector<SearchMatch> cardMatches;
            SearchInText(0, obj.printoutTextUtf8, needleNorm, caseSensitive, accentInsensitive, cardMatches, &obj.printoutTextRuns);
            for (auto& cm : cardMatches) {
                cm.onCanvas = true;
                cm.objectId = obj.id;
                if (!cm.hasBox) {
                    cm.hasBox = true;
                    cm.boxNormalized = { 0.04, 0.04, 0.92, 0.10 };
                }
                const double nx = std::clamp(cm.boxNormalized.x, 0.0, 0.98);
                const double ny = std::clamp(cm.boxNormalized.y, 0.0, 0.98);
                const double nw = std::clamp(cm.boxNormalized.w, 0.02, 1.0 - nx);
                const double nh = std::clamp(cm.boxNormalized.h, 0.015, 1.0 - ny);
                cm.worldBox = {
                    obj.bounds.x + nx * obj.bounds.w,
                    obj.bounds.y + ny * obj.bounds.h,
                    (std::max)(16.0, nw * obj.bounds.w),
                    (std::max)(14.0, nh * obj.bounds.h)
                };
                out.push_back(std::move(cm));
            }
        }

        std::string searchable;
        if (!obj.textUtf8.empty()) searchable += obj.textUtf8;
        if (!obj.formulaExprUtf8.empty()) {
            if (!searchable.empty()) searchable += ' ';
            searchable += obj.formulaExprUtf8;
        }
        if (!obj.evaluatedResultUtf8.empty()) {
            if (!searchable.empty()) searchable += ' ';
            searchable += obj.evaluatedResultUtf8;
        }
        if (!obj.calcModelIdUtf8.empty()) {
            if (!searchable.empty()) searchable += ' ';
            searchable += obj.calcModelIdUtf8 + " " + obj.calcKeySeqUtf8;
        }
        if (!obj.htrRecognizedTextUtf8.empty()) {
            if (!searchable.empty()) searchable += ' ';
            searchable += obj.htrRecognizedTextUtf8;
        }
        if (!obj.tableJsonUtf8.empty()) {
            if (!searchable.empty()) searchable += ' ';
            searchable += obj.tableJsonUtf8;
        }
        if (!searchable.empty() && ContainsNormalizedMatch(searchable, needleNorm, caseSensitive, accentInsensitive)) {
            SearchMatch m{};
            m.pageIndex = 0;
            m.onCanvas = true;
            m.objectId = obj.id;
            m.charOffset = 0;
            m.charLength = queryUtf8.size();
            m.snippetUtf8 = (searchable.size() <= 160) ? searchable : SliceUtf8SafeSnippet(searchable, 0, 80);
            m.hasBox = true;
            m.boxNormalized = { 0.0, 0.0, 1.0, 1.0 };
            m.worldBox = obj.bounds;
            out.push_back(std::move(m));
        }
    }
    return out;
}

void DocumentModel::SetLayers(std::vector<LayerModel> layers) {
    if (layers.empty()) {
        layers.push_back(LayerModel{});
    }
    layers_ = std::move(layers);
    nextLayerId_ = 1;
    bool activeFound = false;
    for (const auto& l : layers_) {
        if (l.id > nextLayerId_) nextLayerId_ = l.id;
        if (l.id == activeLayerId_) activeFound = true;
    }
    if (!activeFound) activeLayerId_ = layers_.front().id;
    BumpRevision();
}

std::uint64_t DocumentModel::AddLayer(const std::string& nameUtf8) {
    LayerModel layer;
    layer.id = ++nextLayerId_;
    layer.nameUtf8 = nameUtf8.empty() ? ("Capa " + std::to_string(layer.id)) : nameUtf8;
    layers_.push_back(std::move(layer));
    dirty_ = true;
    BumpRevision();
    return layers_.back().id;
}

bool DocumentModel::RemoveLayer(std::uint64_t layerId) {
    if (layers_.size() <= 1) return false;
    for (std::size_t i = 0; i < layers_.size(); ++i) {
        if (layers_[i].id == layerId) {
            layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(i));
            if (activeLayerId_ == layerId) activeLayerId_ = layers_.front().id;
            dirty_ = true;
            BumpRevision();
            return true;
        }
    }
    return false;
}

bool DocumentModel::SetLayerProperties(
    std::uint64_t layerId,
    bool visible,
    bool locked,
    bool includeInExport,
    double opacity)
{
    for (auto& l : layers_) {
        if (l.id == layerId) {
            l.visible = visible;
            l.locked = locked;
            l.includeInExport = includeInExport;
            l.opacity = std::clamp(opacity, 0.05, 1.0);
            dirty_ = true;
            BumpRevision();
            return true;
        }
    }
    return false;
}

bool DocumentModel::MoveLayer(std::size_t fromIdx, std::size_t toIdx) {
    if (fromIdx >= layers_.size() || toIdx >= layers_.size() || fromIdx == toIdx) return false;
    LayerModel item = std::move(layers_[fromIdx]);
    layers_.erase(layers_.begin() + static_cast<std::ptrdiff_t>(fromIdx));
    layers_.insert(layers_.begin() + static_cast<std::ptrdiff_t>(toIdx), std::move(item));
    dirty_ = true;
    BumpRevision();
    return true;
}

const LayerModel* DocumentModel::FindLayer(std::uint64_t layerId) const noexcept {
    for (const auto& l : layers_) {
        if (l.id == layerId) return &l;
    }
    return nullptr;
}

bool DocumentModel::IsLayerVisible(std::uint64_t layerId) const noexcept {
    const auto* l = FindLayer(layerId);
    return l ? l->visible : true;
}

bool DocumentModel::IsLayerLocked(std::uint64_t layerId) const noexcept {
    const auto* l = FindLayer(layerId);
    return l ? l->locked : false;
}

bool DocumentModel::IsLayerExportable(std::uint64_t layerId) const noexcept {
    const auto* l = FindLayer(layerId);
    return l ? l->includeInExport : true;
}

std::uint64_t DocumentModel::AddInkLink(InkLink link) {
    if (link.id == 0) {
        link.id = ++nextLinkId_;
    } else if (link.id > nextLinkId_) {
        nextLinkId_ = link.id;
    }
    inkLinks_.push_back(std::move(link));
    dirty_ = true;
    BumpRevision();
    return inkLinks_.back().id;
}

bool DocumentModel::RemoveInkLink(std::uint64_t linkId) {
    for (std::size_t i = 0; i < inkLinks_.size(); ++i) {
        if (inkLinks_[i].id == linkId) {
            inkLinks_.erase(inkLinks_.begin() + static_cast<std::ptrdiff_t>(i));
            dirty_ = true;
            BumpRevision();
            return true;
        }
    }
    return false;
}

void DocumentModel::ToggleCollapsedRange(int startPage0, int endPage0) {
    const int a = (std::min)(startPage0, endPage0);
    const int b = (std::max)(startPage0, endPage0);
    for (std::size_t i = 0; i < collapsedRanges_.size(); ++i) {
        if (collapsedRanges_[i].startPage0 == a && collapsedRanges_[i].endPage0 == b) {
            collapsedRanges_[i].collapsed = !collapsedRanges_[i].collapsed;
            if (!collapsedRanges_[i].collapsed) {
                collapsedRanges_.erase(collapsedRanges_.begin() + static_cast<std::ptrdiff_t>(i));
            }
            dirty_ = true;
            RebuildLayout();
            return;
        }
    }
    collapsedRanges_.push_back(CollapsedPageRange{ a, b, true });
    dirty_ = true;
    RebuildLayout();
}

bool DocumentModel::IsPageCollapsed(int pageIndex0) const noexcept {
    for (const auto& r : collapsedRanges_) {
        if (r.collapsed && pageIndex0 >= r.startPage0 && pageIndex0 <= r.endPage0) {
            return true;
        }
    }
    return false;
}

std::uint64_t DocumentModel::AddBookmark(std::string titleUtf8, std::size_t pageIndex, double worldY, int level) {
    for (const auto& bm : bookmarks_) {
        if (bm.id > nextBookmarkId_) nextBookmarkId_ = bm.id;
    }
    DocumentBookmark b{};
    b.id = ++nextBookmarkId_;
    b.titleUtf8 = std::move(titleUtf8);
    b.pageIndex = pageIndex;
    b.worldY = worldY;
    b.level = level;
    bookmarks_.push_back(b);
    dirty_ = true;
    BumpRevision();
    return b.id;
}

bool DocumentModel::RemoveBookmark(std::uint64_t bookmarkId) {
    for (std::size_t i = 0; i < bookmarks_.size(); ++i) {
        if (bookmarks_[i].id == bookmarkId) {
            bookmarks_.erase(bookmarks_.begin() + static_cast<std::ptrdiff_t>(i));
            dirty_ = true;
            BumpRevision();
            return true;
        }
    }
    return false;
}

std::string FoldUtf8ForSearch(const std::string& utf8, bool caseSensitive, bool accentInsensitive) {
    const NormalizedCodepoints norm = NormalizeTextForSearch(utf8, caseSensitive, accentInsensitive);
    std::string out;
    out.reserve(utf8.size());
    for (std::uint32_t cp : norm.cps) {
        if (cp < 0x80u) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}

} // namespace DocStudio
