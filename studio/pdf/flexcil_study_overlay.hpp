#pragma once

#include "../core/command_stack.hpp"
#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "pdf_parser.hpp"
#include "pdf_writer.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

// ---------------- Flexcil Gesture Recognition (L-Capture, Box-Capture, Text Line) ----------------

enum class FlexcilGestureKind : std::uint8_t {
    None = 0,
    LCapture = 1,        // Vertical down then horizontal right 'L' corner capture (FX-PDF-034)
    SquareCapture = 2,   // Closed 4-corner rectangle / square capture (FX-PDF-035)
    UnderlineText = 3,   // Straight horizontal stroke over PDF text runs (FX-PDF-033 / FX-PDF-036)
    BracketSelect = 4    // C-shaped bracket selection over paragraph
};

struct FlexcilGestureResult {
    FlexcilGestureKind kind = FlexcilGestureKind::None;
    DocRect capturedPageRect{};
    NormalizedCropRect capturedNormRect{};
    std::string extractedTextUtf8;
};

class FlexcilGestureRecognizer {
public:
    static FlexcilGestureResult AnalyzeGestureStroke(
        const PageModel& pdfPage,
        const std::vector<InkPoint>& strokePoints);
};

// ---------------- Annotation Inventory, Outlines & Metadata Index ----------------

struct PdfAnnotationInventoryItem {
    std::size_t pageIndex0 = 0;
    std::uint64_t objectId = 0;
    ObjectKind kind = ObjectKind::InkStroke;
    std::uint64_t layerId = 1;
    std::string summaryUtf8;
    DocRect bounds{};
};

struct PdfSourceMetadata {
    std::string docIdUtf8;
    std::string titleUtf8;
    std::string authorUtf8;
    std::string subjectUtf8;
    std::vector<std::string> keywordsUtf8;
    std::size_t pageCount = 0;
    std::string localPathUtf8;
};

// ---------------- Selective PDF, JPEG, PNG & NTB Export Options ----------------

struct PdfSelectiveExportOptions {
    bool includeAnnotations = true;   // false = export clean background without annotations (MS-PDF-041)
    bool flattened = true;            // false = embed editable CuadernU annotation JSON stream in PDF (GN-PDF-043 / FX-EDIT-053)
    bool onlyAnnotatedPages = false;  // true = filter to pages that have at least one annotation
    std::vector<std::size_t> selectedPages0; // if non-empty, export only these 0-based page indices (MS-PDF-043 / FX-PDF-045)
};

// ---------------- Flexcil Dual Viewer (Main Book + Pop-up Study Note & Tabs) ----------------

enum class PdfScrollDirection : std::uint8_t {
    Vertical = 0,
    Horizontal = 1
};

struct OpenPdfStudyTab {
    std::string tabIdUtf8;
    std::string titleUtf8;
    std::string filePathUtf8;
    DocumentModel doc;
    std::size_t currentPage0 = 0;
};

struct CitationJumpTarget {
    bool valid = false;
    std::string sourceDocPathUtf8;
    std::size_t sourcePage0 = 0;
    NormalizedCropRect sourceRectNorm{};
    std::string labelUtf8;
};

class FlexcilStudyOverlay {
public:
    FlexcilStudyOverlay();

    // Multi-PDF Tab management (FX-PDF-027, FX-PDF-029, FX-PDF-030)
    std::string OpenPdfTabFromBytes(const std::string& titleUtf8, const std::string& virtualPathUtf8, const std::vector<std::uint8_t>& pdfBytes);
    bool ActivatePdfTab(const std::string& tabIdUtf8);
    OpenPdfStudyTab* ActivePdfTabMutable();
    const OpenPdfStudyTab* ActivePdfTab() const;
    const std::vector<OpenPdfStudyTab>& PdfTabs() const noexcept { return pdfTabs_; }

    // Simultaneous Pop-up Study Note & 3-finger slide toggle (FX-PDF-028, FX-PDF-031, FX-PDF-032)
    bool IsPopupNoteVisible() const noexcept { return popupVisible_; }
    void SetPopupNoteVisible(bool vis) noexcept { popupVisible_ = vis; }
    void HandleThreeFingerSwipe(bool swipeLeftToOpen) noexcept { popupVisible_ = swipeLeftToOpen; }
    DocRect PopupOverlayScreenRect() const noexcept { return popupScreenRect_; }
    void SetPopupOverlayScreenRect(DocRect rc) noexcept { popupScreenRect_ = rc; }

    DocumentModel& StudyNoteDocument() noexcept { return studyNoteDoc_; }
    const DocumentModel& StudyNoteDocument() const noexcept { return studyNoteDoc_; }
    CommandStack& StudyNoteCommandStack() noexcept { return studyNoteStack_; }

    // Scroll & Page deployment mode (FX-PDF-039, FX-PDF-044)
    PdfScrollDirection ScrollDirection() const noexcept { return scrollDir_; }
    void SetScrollDirection(PdfScrollDirection dir) noexcept { scrollDir_ = dir; }
    bool IsGestureModeActive() const noexcept { return gestureMode_; }
    void SetGestureModeActive(bool active) noexcept { gestureMode_ = active; }

    // Gesture capture from active PDF tab into Pop-up Study Note with automatic bi-directional InkLink (FX-PDF-033..038, FX-PDF-047)
    bool ApplyGestureStrokeOnActivePdf(
        std::size_t pageIndex0,
        const std::vector<InkPoint>& strokePts,
        FlexcilGestureResult* outGesture = nullptr,
        std::uint64_t* outCreatedNoteObjId = nullptr,
        std::uint64_t* outCreatedInkLinkId = nullptr);

    // Direct gesture capture from an external open DocumentModel (used live by CuadernU viewport)
    bool ApplyGestureStrokeOnDocument(
        DocumentModel& sourceDoc,
        const std::string& sourcePathUtf8,
        std::size_t pageIndex0,
        const std::vector<InkPoint>& strokePts,
        FlexcilGestureResult* outGesture = nullptr,
        std::uint64_t* outCreatedNoteObjId = nullptr,
        std::uint64_t* outCreatedInkLinkId = nullptr);

    CitationJumpTarget JumpFromStudyNoteObjectToSourcePdf(std::uint64_t studyNoteObjectId);

    std::string SerializeStateToJson() const;
    bool DeserializeStateFromJson(const std::string& jsonUtf8);

    // Page management on any DocumentModel (GN-PDF-047, MS-PDF-037..039, FX-EDIT-056..057)
    static bool DuplicatePage(DocumentModel& doc, std::size_t pageIndex0);
    static bool ReorderPage(DocumentModel& doc, std::size_t fromIndex0, std::size_t toIndex0);
    static bool InsertPageAt(DocumentModel& doc, std::size_t insertIndex0, PaperPattern pattern = PaperPattern::Plain);
    static bool DeletePageAt(DocumentModel& doc, std::size_t pageIndex0);

    // Outlines, Bookmarks & Annotation Inventory (FX-PDF-040..042, FX-PDF-046, GN-PDF-040..041)
    static bool RenameBookmark(DocumentModel& doc, std::uint64_t bookmarkId, const std::string& newTitleUtf8, int newLevel = -1);
    static std::vector<PdfAnnotationInventoryItem> ListAllAnnotations(const DocumentModel& doc);
    static std::string ExportAnnotationsMarkdownSummary(const DocumentModel& doc);

    // Form Signing via Ink SignatureStamp (MS-PDF-045, AP-PDF-046)
    static std::uint64_t SignPdfFormWithInk(
        DocumentModel& doc,
        CommandStack& cmdStack,
        std::size_t pageIndex0,
        const std::string& formFieldNameUtf8,
        const std::string& signerNameUtf8,
        const std::vector<InkPoint>& signatureTrajectory);

    // Source Metadata Indexing & Attachments (OB-PDF-067..072, ON-PDF-048, AP-PDF-043..044, AP-PDF-050)
    void IndexPdfSourceMetadata(const PdfSourceMetadata& meta);
    std::vector<PdfSourceMetadata> SearchIndexedPdfMetadata(const std::string& queryUtf8) const;

    // Selective PDF Export + Editable Annotation Roundtrip (GN-PDF-043..044, MS-PDF-040..043, FX-PDF-045, FX-EDIT-053)
    static bool ExportSelectivePdfToBytes(
        const DocumentModel& doc,
        const PdfSelectiveExportOptions& opts,
        std::vector<std::uint8_t>& outPdfBytes,
        std::string* outErr = nullptr);

    static bool ImportEditablePdfAnnotationsFromBytes(
        const std::vector<std::uint8_t>& pdfBytes,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    // Page Image Export (PNG & JPEG via WIC) and Local .ntb Package (NT-PDF-045..048)
    static bool ExportPageToPngOrJpegFile(
        const DocumentModel& doc,
        std::size_t pageIndex0,
        const std::wstring& outPath,
        bool asJpeg,
        int targetWidthPx = 600,
        std::string* outErr = nullptr);

    static bool ExportLocalNotabilityNtbPackage(
        const DocumentModel& doc,
        const std::wstring& outNtbPath,
        std::string* outErr = nullptr);

private:
    std::uint64_t nextTabSeq_ = 1;
    std::vector<OpenPdfStudyTab> pdfTabs_;
    std::string activeTabId_;
    bool popupVisible_ = true;
    bool gestureMode_ = true;
    PdfScrollDirection scrollDir_ = PdfScrollDirection::Vertical;
    DocRect popupScreenRect_{ 520.0, 72.0, 380.0, 520.0 };
    DocumentModel studyNoteDoc_;
    CommandStack studyNoteStack_;
    std::vector<PdfSourceMetadata> metadataIndex_;
};

} // namespace DocStudio
