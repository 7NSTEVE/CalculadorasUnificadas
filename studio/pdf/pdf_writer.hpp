#pragma once

#include "../core/doc_types.hpp"
#include "../core/document_model.hpp"
#include "pdf_parser.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

struct CanvasPdfExportOptions {
    bool paginated = true;       // false = single cropped/region page
    double pageWidthPt = 595.0;  // A4 width when paginated
    double pageHeightPt = 842.0; // A4 height when paginated
    bool hasCustomRegion = false;
    DocRect customRegionWorld{};
    std::size_t maxPages = 0;    // 0 = sin tope artificial de 200 paginas
};

class PdfWriter {
public:
    // Exports paginated DocumentModel (both blank/note pages and imported PDF pages + flattened annotations)
    static bool ExportDocumentToBytes(
        const DocumentModel& doc,
        std::vector<std::uint8_t>& outPdfBytes,
        std::string* outErr = nullptr);

    static bool ExportDocumentToFile(
        const DocumentModel& doc,
        const std::wstring& outPath,
        std::string* outErr = nullptr);

    // Exports Infinite Canvas mode (with Quadtree objects, paper pattern, images, PdfCards, shapes, ink)
    static bool ExportInfiniteCanvasToBytes(
        const DocumentModel& doc,
        const CanvasPdfExportOptions& opts,
        std::vector<std::uint8_t>& outPdfBytes,
        std::string* outErr = nullptr);

    static bool ExportInfiniteCanvasToFile(
        const DocumentModel& doc,
        const CanvasPdfExportOptions& opts,
        const std::wstring& outPath,
        std::string* outErr = nullptr);

    // Structural multi-PDF merge (preserving vector content)
    static bool MergePdfFiles(
        const std::vector<std::wstring>& inputPaths,
        const std::wstring& outPath,
        std::string* outErr = nullptr);

    // Structural split / page subset extraction (0-based indices)
    static bool SplitPdfFile(
        const std::wstring& inputPath,
        const std::vector<std::size_t>& zeroBasedPages,
        const std::wstring& outPath,
        std::string* outErr = nullptr);

    // Attempts direct literal string replacement in a decompressed PDF page stream (F5 direct path).
    // Returns false cleanly if literal string isn't safely matched as plain PDF text.
    static bool TryDirectReplaceTextInPdfBytes(
        const std::vector<std::uint8_t>& srcPdfBytes,
        std::size_t pageIndex0,
        const std::string& oldTextUtf8,
        const std::string& newTextUtf8,
        std::vector<std::uint8_t>& outPdfBytes,
        std::string* outErr = nullptr);
};

} // namespace DocStudio
