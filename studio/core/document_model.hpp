#pragma once

#include "doc_types.hpp"
#include "page_layout.hpp"
#include "spatial_index.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

// Pure C++17 document state: paginated PDF pages + Infinite Canvas + Quadtree + Asset registry.
class DocumentModel {
public:
    DocumentModel();

    void ResetToBlank(DocumentMode mode = DocumentMode::PaginatedPdf);

    DocumentMode Mode() const noexcept { return mode_; }
    void SetMode(DocumentMode m) noexcept { mode_ = m; ++revision_; }

    const std::string& Title() const noexcept { return titleUtf8_; }
    void SetTitle(std::string t) { titleUtf8_ = std::move(t); ++revision_; }

    // Durable UUID bound to WAL, version history, and sidecar undo (fixes ephemeral ID collision)
    const std::string& DocumentUuid() const noexcept { return documentUuidUtf8_; }
    void SetDocumentUuid(std::string uuid) { if (!uuid.empty()) documentUuidUtf8_ = std::move(uuid); }
    static std::string GenerateDurableUuid();

    std::uint64_t Revision() const noexcept { return revision_; }
    void BumpRevision() noexcept { ++revision_; }
    bool IsDirty() const noexcept { return dirty_; }
    void SetDirty(bool d) noexcept { dirty_ = d; }

    std::uint64_t AllocateObjectId() noexcept { return ++nextObjectId_; }
    std::uint64_t AllocatePageId() noexcept { return ++nextPageId_; }
    std::uint64_t AllocateZOrder() noexcept { return ++nextZOrder_; }

    std::uint64_t NextObjectId() const noexcept { return nextObjectId_; }
    std::uint64_t NextPageId() const noexcept { return nextPageId_; }
    std::uint64_t NextZOrder() const noexcept { return nextZOrder_; }
    std::uint64_t NextAssetSeq() const noexcept { return nextAssetSeq_; }
    void RestoreCounters(
        std::uint64_t nextObj,
        std::uint64_t nextPage,
        std::uint64_t nextZ,
        std::uint64_t nextAsset) noexcept;

    // Assets (embedded PNG/binary blobs shared cleanly across undo/redo and save/load)
    std::string RegisterAsset(std::vector<std::uint8_t> bytes);
    void SetNamedAsset(const std::string& key, std::vector<std::uint8_t> bytes);
    const std::vector<std::uint8_t>* FindAsset(const std::string& key) const;
    const std::unordered_map<std::string, std::vector<std::uint8_t>>& Assets() const noexcept { return assets_; }

    // Paginated Document API
    const std::vector<PageModel>& Pages() const noexcept { return pages_; }
    std::vector<PageModel>& MutablePages() noexcept { return pages_; }
    PageLayout& Layout() noexcept { return layout_; }
    const PageLayout& Layout() const noexcept { return layout_; }
    void RebuildLayout();

    // Infinite Canvas API
    PaperPattern CanvasBackground() const noexcept { return canvasBg_; }
    void SetCanvasBackground(PaperPattern bg) noexcept { canvasBg_ = bg; ++revision_; }
    const std::vector<AnnotationObject>& CanvasObjects() const noexcept { return canvasObjects_; }
    void AddCanvasObject(AnnotationObject obj);
    bool RemoveCanvasObject(std::uint64_t id, AnnotationObject* outRemoved = nullptr);
    AnnotationObject* FindCanvasObjectMutable(std::uint64_t id);
    const AnnotationObject* FindCanvasObject(std::uint64_t id) const;
    std::vector<std::uint64_t> QueryCanvasRect(const DocRect& worldRect) const;
    std::size_t CanvasQuadtreeSize() const noexcept { return canvasIndex_.Size(); }
    void RebuildCanvasIndex();

    // Text Search across pages (extracted PDF text, OCR text, and TextBox annotations)
    std::vector<SearchMatch> SearchText(
        const std::string& queryUtf8,
        bool caseSensitive = false,
        bool accentInsensitive = false) const;

    // Direct page object helpers
    bool AddPageObject(std::size_t pageIdx, AnnotationObject obj);
    bool RemovePageObject(std::size_t pageIdx, std::uint64_t id, AnnotationObject* outRemoved = nullptr);
    AnnotationObject* FindPageObjectMutable(std::size_t pageIdx, std::uint64_t id);

    // Layer management (shared across Windows & Linux, serialized in .cudoc)
    const std::vector<LayerModel>& Layers() const noexcept { return layers_; }
    void SetLayers(std::vector<LayerModel> layers);
    std::uint64_t ActiveLayerId() const noexcept { return activeLayerId_; }
    void SetActiveLayerId(std::uint64_t layerId) noexcept { activeLayerId_ = layerId; ++revision_; }
    std::uint64_t AddLayer(const std::string& nameUtf8);
    bool RemoveLayer(std::uint64_t layerId);
    bool SetLayerProperties(std::uint64_t layerId, bool visible, bool locked, bool includeInExport, double opacity);
    bool MoveLayer(std::size_t fromIdx, std::size_t toIdx);
    const LayerModel* FindLayer(std::uint64_t layerId) const noexcept;
    bool IsLayerVisible(std::uint64_t layerId) const noexcept;
    bool IsLayerLocked(std::uint64_t layerId) const noexcept;
    bool IsLayerExportable(std::uint64_t layerId) const noexcept;

    // Bidirectional PDF <-> Canvas InkLinks & Pinch-to-Collapse page ranges
    const std::vector<InkLink>& InkLinks() const noexcept { return inkLinks_; }
    void SetInkLinks(std::vector<InkLink> links) { inkLinks_ = std::move(links); ++revision_; }
    std::uint64_t AddInkLink(InkLink link);
    bool RemoveInkLink(std::uint64_t linkId);

    const std::vector<CollapsedPageRange>& CollapsedRanges() const noexcept { return collapsedRanges_; }
    void SetCollapsedRanges(std::vector<CollapsedPageRange> ranges) { collapsedRanges_ = std::move(ranges); ++revision_; }
    void ToggleCollapsedRange(int startPage0, int endPage0);
    bool IsPageCollapsed(int pageIndex0) const noexcept;

    // Hierarchical Bookmarks / Outlines
    const std::vector<DocumentBookmark>& Bookmarks() const noexcept { return bookmarks_; }
    void SetBookmarks(std::vector<DocumentBookmark> bms) { bookmarks_ = std::move(bms); ++revision_; }
    std::uint64_t AddBookmark(std::string titleUtf8, std::size_t pageIndex, double worldY = 0.0, int level = 0);
    bool RemoveBookmark(std::uint64_t bookmarkId);

    // Embedded file attachments (PDF /EmbeddedFiles, OneNote, Goodnotes, Notability audio/files)
    const std::vector<EmbeddedAttachment>& Attachments() const noexcept { return attachments_; }
    void SetAttachments(std::vector<EmbeddedAttachment> atts) { attachments_ = std::move(atts); ++revision_; }
    std::string AddAttachment(const std::string& filenameUtf8, const std::string& mimeTypeUtf8, std::vector<std::uint8_t> bytes);
    bool RemoveAttachment(const std::string& assetKey);

    // Single vs Double-Page Spread View (F-070)
    bool DoublePageSpread() const noexcept { return doublePageSpread_; }
    void SetDoublePageSpread(bool enabled);

    // Persisted document extension sidecars inside .cudoc (extensions/<name>: properties.json, comments.json, database.json, permissions.json, automations.json, flexcil.json, custom_css.txt)
    void SetExtensionPayload(const std::string& nameUtf8, std::string payloadUtf8) {
        if (!nameUtf8.empty()) {
            extensionPayloads_[nameUtf8] = std::move(payloadUtf8);
            dirty_ = true;
            ++revision_;
        }
    }
    const std::string* FindExtensionPayload(const std::string& nameUtf8) const noexcept {
        auto it = extensionPayloads_.find(nameUtf8);
        return (it != extensionPayloads_.end()) ? &it->second : nullptr;
    }
    const std::unordered_map<std::string, std::string>& ExtensionPayloads() const noexcept {
        return extensionPayloads_;
    }
    void ClearExtensionPayloads() { extensionPayloads_.clear(); }

private:
    DocumentMode mode_ = DocumentMode::PaginatedPdf;
    std::string titleUtf8_ = "Sin t\xC3\xADtulo";
    std::string documentUuidUtf8_;
    std::uint64_t revision_ = 1;
    bool dirty_ = false;
    bool doublePageSpread_ = false;

    std::uint64_t nextObjectId_ = 0;
    std::uint64_t nextPageId_ = 0;
    std::uint64_t nextZOrder_ = 0;
    std::uint64_t nextAssetSeq_ = 0;
    std::uint64_t nextLayerId_ = 1;
    std::uint64_t nextLinkId_ = 0;
    std::uint64_t nextBookmarkId_ = 0;
    std::uint64_t activeLayerId_ = 1;

    std::vector<PageModel> pages_;
    PageLayout layout_;
    std::vector<LayerModel> layers_{ LayerModel{} };
    std::vector<InkLink> inkLinks_;
    std::vector<CollapsedPageRange> collapsedRanges_;
    std::vector<DocumentBookmark> bookmarks_;
    std::vector<EmbeddedAttachment> attachments_;

    PaperPattern canvasBg_ = PaperPattern::Grid;
    std::vector<AnnotationObject> canvasObjects_;
    std::unordered_map<std::uint64_t, std::size_t> canvasIdToPos_;
    QuadtreeIndex canvasIndex_;

    std::unordered_map<std::string, std::vector<std::uint8_t>> assets_;
    std::unordered_map<std::string, std::string> extensionPayloads_;
};

// Shared Unicode case-folding + optional Latin accent stripping for Full-Text Vault indexing
std::string FoldUtf8ForSearch(const std::string& utf8, bool caseSensitive = false, bool accentInsensitive = true);

} // namespace DocStudio
