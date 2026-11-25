#pragma once

#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace DocStudio::Printout {

enum class ConverterChoice : std::uint8_t {
    Automatic = 0,
    MicrosoftOffice = 1,
    LibreOffice = 2,
    NativeEditable = 3
};

enum class Target : std::uint8_t {
    NotebookPages = 0,
    InfiniteCanvas = 1
};

enum class CanvasLayout : std::uint8_t {
    VerticalColumn = 0,
    Grid = 1
};

struct Options {
    Target target = Target::InfiniteCanvas;
    int firstPage1 = 1;
    int lastPage1 = 0; // 0 = all pages up to the end (no 6-page cap)
    CanvasLayout layout = CanvasLayout::VerticalColumn;
    int gridColumns = 3;
    double pageGapPt = 32.0; // Custom vertical spacing between sheets on Infinite Canvas (pt)
    double colGapPt = 36.0;  // Custom horizontal spacing between grid columns on Infinite Canvas (pt)
    int rasterDpi = 192; // 144 (Standard), 192 (High), 288 (Max)
    bool lockInPlace = true;
    bool ocrWhenNoText = true;
    ConverterChoice converter = ConverterChoice::Automatic;
    unsigned officeTimeoutMs = 90000;
};

struct PageResult {
    int sourcePageIndex0 = 0;
    double widthPt = 595.0;
    double heightPt = 842.0;
    int pixelW = 0;
    int pixelH = 0;
    std::vector<std::uint8_t> png;
    std::string text;
    std::vector<PdfTextRunBox> runs;
    bool fromOcr = false;
};

struct Report {
    std::string sourceNameUtf8;
    std::string sourceKindUtf8;      // "pdf" | "docx" | "pptx"
    std::string converterUsedUtf8;   // "PDF" | "Microsoft Word" | "Microsoft PowerPoint" | "LibreOffice" | "Nativo"
    bool visualFidelityExact = true; // false only when falling back to NativeEditable
    bool ocrEngineAvailable = false;
    std::size_t totalSourcePages = 0;
    std::size_t pagesInserted = 0;
    std::size_t pagesWithText = 0;
    std::size_t pagesWithOcr = 0;
    std::string sourceSha256Before;
    std::string sourceSha256After;
    std::vector<std::string> warningsUtf8;
    std::string errorUtf8;
};

// Computes lowercase 64-character hex SHA-256 of a file on disk without modifying it.
std::string ComputeFileSha256Hex(const std::wstring& path);

// Converts a Word (.docx/.doc) or PowerPoint (.pptx/.ppt) file into a temporary working PDF.
// Never modifies the original source file (operates on an isolated read-only temp copy).
bool ConvertOfficeToPdf(
    const std::wstring& srcPath,
    ConverterChoice choice,
    unsigned timeoutMs,
    std::wstring& outPdfPath,
    Report& rep);

// Extracts text + bounding boxes, rasterizes pages as needed, and runs Windows.Media.Ocr
// on image-only/scanned pages. Does not mutate any DocumentModel.
bool BuildPages(
    const std::wstring& pdfPath,
    const Options& opts,
    std::vector<PageResult>& outPages,
    Report& rep,
    const std::atomic<bool>* cancel = nullptr,
    const std::function<void(std::size_t, std::size_t)>& progress = {});

// Inserts built pages into the target DocumentModel (either as paginated NotebookPages after
// activePageIndex, or as pinned PdfCard objects on the InfiniteCanvas) as a single Undo step.
bool InsertIntoDocument(
    DocumentModel& doc,
    CommandStack& cmdStack,
    const std::wstring& workingPdfPath,
    const std::vector<PageResult>& pages,
    const Options& opts,
    std::size_t activePageIndex,
    DocPoint canvasOrigin,
    Report& rep);

// Unified end-to-end entry point for UI and CLI.
bool InsertPrintoutFromFile(
    DocumentModel& doc,
    CommandStack& cmdStack,
    const std::wstring& srcPath,
    const Options& opts,
    std::size_t activePageIndex,
    DocPoint canvasOrigin,
    Report& rep,
    const std::atomic<bool>* cancel = nullptr,
    const std::function<void(std::size_t, std::size_t)>& progress = {});

} // namespace DocStudio::Printout
