#pragma once

#include "../core/document_model.hpp"
#include <string>
#include <vector>

namespace DocStudioApp {

// Detects cyclic cell references (=A1+1 <-> =B1+1) in a CSV/table grid.
bool DetectCyclicTableFormulas(
    const std::vector<std::vector<std::string>>& grid,
    std::string* outCycleDiag = nullptr);

// Exports extracted text, OCR text, and TextBox annotations to UTF-8 RFC-4180 CSV.
bool ExportToCsvFile(
    const DocStudio::DocumentModel& doc,
    const std::string& outPathUtf8,
    std::string* outErr = nullptr);

// Imports a UTF-8 CSV file into paginated DocumentModel pages with searchable text,
// reactive formula cycle check, optional OLS linear fit summary, and TextBoxes.
bool ImportFromCsvFile(
    const std::string& inPathUtf8,
    DocStudio::DocumentModel& outDoc,
    std::string* outErr = nullptr);

// Exports document text/annotations to a valid OpenXML SpreadsheetML (.xlsx) workbook via ZipArchive.
bool ExportToXlsxFile(
    const DocStudio::DocumentModel& doc,
    const std::string& outPathUtf8,
    std::string* outErr = nullptr);

// Imports sheets/rows from an OpenXML .xlsx workbook into paginated DocumentModel pages.
bool ImportFromXlsxFile(
    const std::string& inPathUtf8,
    DocStudio::DocumentModel& outDoc,
    std::string* outErr = nullptr);

// Imports Markdown with headings, $$...$$ LaTeX math blocks, and fenced code blocks into DocumentModel.
bool ImportFromMarkdownText(const std::string& markdownUtf8, const std::string& titleUtf8,
                            DocStudio::DocumentModel& outDoc);

bool ImportFromMarkdownFile(
    const std::string& inPathUtf8,
    DocStudio::DocumentModel& outDoc,
    std::string* outErr = nullptr);

// Exports canvas/page vector geometry, ink strokes, and text annotations to AutoCAD ASCII DXF (R12/R2000)
// with architectural/engineering scale ratio (e.g. "1:50" -> scaleFactor = 50.0).
bool ExportToDxfFile(
    const DocStudio::DocumentModel& doc,
    const std::string& outPathUtf8,
    const std::string& scaleRatioUtf8 = "1:1",
    std::string* outErr = nullptr);

} // namespace DocStudioApp
