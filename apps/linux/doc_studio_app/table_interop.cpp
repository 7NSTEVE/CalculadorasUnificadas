#include "table_interop.hpp"
#include "../../doc_studio/core/zip_container.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace DocStudioApp {

namespace {

using namespace DocStudio;

bool ReadFileBytes(const std::string& pathUtf8, std::vector<std::uint8_t>& out, std::string* outErr) {
    std::ifstream ifs(std::filesystem::u8path(pathUtf8), std::ios::binary | std::ios::ate);
    if (!ifs) {
        if (outErr) *outErr = "No se pudo abrir el archivo: " + pathUtf8;
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz < 0 || sz > 128ll * 1024ll * 1024ll) {
        if (outErr) *outErr = "Tamano de archivo invalido o demasiado grande.";
        return false;
    }
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty() && !ifs.read(reinterpret_cast<char*>(out.data()), sz)) {
        if (outErr) *outErr = "Error leyendo archivo: " + pathUtf8;
        return false;
    }
    return true;
}

bool WriteFileBytes(const std::string& pathUtf8, const std::vector<std::uint8_t>& bytes, std::string* outErr) {
    std::ofstream ofs(std::filesystem::u8path(pathUtf8), std::ios::binary | std::ios::trunc);
    if (!ofs) {
        if (outErr) *outErr = "No se pudo crear el archivo: " + pathUtf8;
        return false;
    }
    if (!bytes.empty()) {
        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    if (!ofs) {
        if (outErr) *outErr = "Error escribiendo archivo: " + pathUtf8;
        return false;
    }
    return true;
}

std::string CsvEscape(const std::string& field) {
    bool needsQuotes = false;
    for (unsigned char c : field) {
        if (c == ',' || c == '"' || c == '\n' || c == '\r') {
            needsQuotes = true;
            break;
        }
    }
    if (!needsQuotes) return field;
    std::string out = "\"";
    for (char c : field) {
        if (c == '"') out += "\"\"";
        else out.push_back(c);
    }
    out.push_back('"');
    return out;
}

std::vector<std::vector<std::string>> ParseCsv(const std::string& input) {
    std::string data = input;
    if (data.size() >= 3 &&
        static_cast<unsigned char>(data[0]) == 0xEF &&
        static_cast<unsigned char>(data[1]) == 0xBB &&
        static_cast<unsigned char>(data[2]) == 0xBF) {
        data.erase(0, 3);
    }

    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> currentRow;
    std::string field;
    bool inQuotes = false;

    for (std::size_t i = 0; i < data.size(); ++i) {
        const char c = data[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < data.size() && data[i + 1] == '"') {
                    field.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field.push_back(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                currentRow.push_back(std::move(field));
                field.clear();
            } else if (c == '\r') {
                if (i + 1 < data.size() && data[i + 1] == '\n') ++i;
                currentRow.push_back(std::move(field));
                field.clear();
                rows.push_back(std::move(currentRow));
                currentRow.clear();
            } else if (c == '\n') {
                currentRow.push_back(std::move(field));
                field.clear();
                rows.push_back(std::move(currentRow));
                currentRow.clear();
            } else {
                field.push_back(c);
            }
        }
    }

    if (!field.empty() || !currentRow.empty()) {
        currentRow.push_back(std::move(field));
        rows.push_back(std::move(currentRow));
    }
    return rows;
}

struct ExportRow {
    int pageNum = 1;
    std::string source;
    std::string dimensions;
    std::string text;
};

std::vector<ExportRow> CollectDocumentRows(const DocumentModel& doc) {
    std::vector<ExportRow> rows;
    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        for (const auto& obj : doc.CanvasObjects()) {
            if (!obj.textUtf8.empty()) {
                rows.push_back({ 1, "CanvasTextBox", "Canvas", obj.textUtf8 });
            } else if (!obj.captionUtf8.empty()) {
                rows.push_back({ 1, "CanvasCaption", "Canvas", obj.captionUtf8 });
            }
        }
        if (rows.empty()) {
            rows.push_back({ 1, "CanvasSummary", "Canvas",
                doc.Title() + " (" + std::to_string(doc.CanvasObjects().size()) + " objetos)" });
        }
        return rows;
    }

    const auto& pages = doc.Pages();
    for (std::size_t i = 0; i < pages.size(); ++i) {
        const auto& pg = pages[i];
        const int pno = static_cast<int>(i + 1);
        const std::string dims = std::to_string(static_cast<int>(pg.EffectiveWidthPt())) + "x" +
                                 std::to_string(static_cast<int>(pg.EffectiveHeightPt())) + " pt";
        bool pushed = false;
        if (!pg.extractedTextUtf8.empty()) {
            rows.push_back({ pno, "ExtractedText", dims, pg.extractedTextUtf8 });
            pushed = true;
        }
        if (!pg.ocrTextUtf8.empty()) {
            rows.push_back({ pno, "OcrText", dims, pg.ocrTextUtf8 });
            pushed = true;
        }
        for (const auto& obj : pg.objects) {
            if (!obj.textUtf8.empty()) {
                rows.push_back({ pno, "AnnotationText", dims, obj.textUtf8 });
                pushed = true;
            }
        }
        if (!pushed) {
            rows.push_back({ pno, "Page", dims, "" });
        }
    }
    return rows;
}

void PopulateModelFromTable(
    DocumentModel& outDoc,
    const std::string& titleUtf8,
    const std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>>& sheets)
{
    outDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    outDoc.SetTitle(titleUtf8);
    outDoc.MutablePages().clear();

    constexpr std::size_t kRowsPerPage = 32;
    for (const auto& sheet : sheets) {
        const std::string& sheetName = sheet.first;
        const auto& rows = sheet.second;
        const std::size_t totalRows = rows.size();
        const std::size_t pageCount = std::max<std::size_t>(1, (totalRows + kRowsPerPage - 1) / kRowsPerPage);

        for (std::size_t p = 0; p < pageCount; ++p) {
            PageModel pm;
            pm.pageId = outDoc.AllocatePageId();
            pm.widthPt = 595.0;
            pm.heightPt = 842.0;
            pm.background = PaperPattern::Grid;

            std::ostringstream blockText;
            blockText << "[" << sheetName << " — Pag. " << (p + 1) << "/" << pageCount << "]\n";

            const std::size_t startRow = p * kRowsPerPage;
            const std::size_t endRow = std::min(totalRows, startRow + kRowsPerPage);
            double y = 48.0;

            AnnotationObject headerObj;
            headerObj.id = outDoc.AllocateObjectId();
            headerObj.zOrder = outDoc.AllocateZOrder();
            headerObj.kind = ObjectKind::TextBox;
            headerObj.bounds = { 36.0, 20.0, 523.0, 24.0 };
            headerObj.strokeColor = DocColor::RGBA(30, 64, 175, 255);
            headerObj.fontSizePt = 12.0;
            headerObj.bold = true;
            headerObj.textUtf8 = sheetName + " (Pag. " + std::to_string(p + 1) + "/" + std::to_string(pageCount) + ")";
            pm.objects.push_back(std::move(headerObj));

            for (std::size_t r = startRow; r < endRow; ++r) {
                std::string line;
                for (std::size_t c = 0; c < rows[r].size(); ++c) {
                    if (c > 0) line += "  |  ";
                    line += rows[r][c];
                }
                if (line.empty()) line = " ";
                blockText << line << '\n';

                AnnotationObject rowObj;
                rowObj.id = outDoc.AllocateObjectId();
                rowObj.zOrder = outDoc.AllocateZOrder();
                rowObj.kind = ObjectKind::TextBox;
                rowObj.bounds = { 36.0, y, 523.0, 22.0 };
                rowObj.strokeColor = (r == 0)
                    ? DocColor::RGBA(15, 23, 42, 255)
                    : DocColor::RGBA(51, 65, 85, 255);
                rowObj.fillColor = (r == 0)
                    ? DocColor::RGBA(219, 234, 254, 255)
                    : ((r % 2 == 1) ? DocColor::RGBA(248, 250, 252, 255) : DocColor::RGBA(255, 255, 255, 0));
                rowObj.fontSizePt = 10.0;
                rowObj.bold = (r == 0);
                rowObj.textUtf8 = line;
                pm.objects.push_back(std::move(rowObj));
                y += 23.0;
            }

            pm.extractedTextUtf8 = blockText.str();
            outDoc.MutablePages().push_back(std::move(pm));
        }
    }

    if (outDoc.Pages().empty()) {
        outDoc.ResetToBlank(DocumentMode::PaginatedPdf);
        outDoc.SetTitle(titleUtf8);
    } else {
        outDoc.RebuildLayout();
        outDoc.SetDirty(false);
    }
}

std::string XmlEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 16);
    for (unsigned char ch : in) {
        switch (ch) {
        case '&':  out += "&amp;"; break;
        case '<':  out += "&lt;"; break;
        case '>':  out += "&gt;"; break;
        case '"':  out += "&quot;"; break;
        case '\'': out += "&apos;"; break;
        default:
            if (ch >= 0x20 || ch == '\t' || ch == '\n' || ch == '\r') {
                out.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return out;
}

std::string XmlUnescape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (std::size_t i = 0; i < in.size();) {
        if (in[i] == '&') {
            if (in.compare(i, 5, "&amp;") == 0) { out.push_back('&'); i += 5; continue; }
            if (in.compare(i, 4, "&lt;") == 0) { out.push_back('<'); i += 4; continue; }
            if (in.compare(i, 4, "&gt;") == 0) { out.push_back('>'); i += 4; continue; }
            if (in.compare(i, 6, "&quot;") == 0) { out.push_back('"'); i += 6; continue; }
            if (in.compare(i, 6, "&apos;") == 0) { out.push_back('\''); i += 6; continue; }
            if (i + 3 < in.size() && in[i + 1] == '#') {
                const std::size_t semi = in.find(';', i + 2);
                if (semi != std::string::npos && semi - i <= 8) {
                    const std::string num = in.substr(i + 2, semi - (i + 2));
                    int base = 10;
                    std::size_t off = 0;
                    if (!num.empty() && (num[0] == 'x' || num[0] == 'X')) {
                        base = 16;
                        off = 1;
                    }
                    try {
                        const unsigned long cp = std::stoul(num.substr(off), nullptr, base);
                        if (cp > 0 && cp < 0x80ul) {
                            out.push_back(static_cast<char>(cp));
                            i = semi + 1;
                            continue;
                        }
                    } catch (...) {}
                }
            }
        }
        out.push_back(in[i++]);
    }
    return out;
}

std::string ColNameFromIndex0(std::size_t colIdx) {
    std::string s;
    std::size_t n = colIdx + 1;
    while (n > 0) {
        const std::size_t rem = (n - 1) % 26;
        s.push_back(static_cast<char>('A' + rem));
        n = (n - 1) / 26;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

std::size_t ColIndex0FromCellRef(const std::string& cellRef) {
    std::size_t col = 0;
    for (char ch : cellRef) {
        if (ch >= 'A' && ch <= 'Z') {
            col = col * 26 + static_cast<std::size_t>(ch - 'A' + 1);
        } else if (ch >= 'a' && ch <= 'z') {
            col = col * 26 + static_cast<std::size_t>(ch - 'a' + 1);
        } else {
            break;
        }
    }
    return col > 0 ? (col - 1) : 0;
}

std::string ExtractAttr(const std::string& tagAttrs, const std::string& attrName) {
    const std::string key1 = attrName + "=\"";
    std::size_t pos = tagAttrs.find(key1);
    char quote = '"';
    if (pos == std::string::npos) {
        const std::string key2 = attrName + "='";
        pos = tagAttrs.find(key2);
        quote = '\'';
        if (pos == std::string::npos) return {};
        pos += key2.size();
    } else {
        pos += key1.size();
    }
    const std::size_t end = tagAttrs.find(quote, pos);
    return (end == std::string::npos) ? std::string{} : tagAttrs.substr(pos, end - pos);
}

std::string ExtractAllTextTags(const std::string& fragment) {
    std::string out;
    std::size_t pos = 0;
    while (pos < fragment.size()) {
        const std::size_t tStart = fragment.find("<t", pos);
        if (tStart == std::string::npos) break;
        const std::size_t nextCh = tStart + 2;
        if (nextCh >= fragment.size() ||
            (fragment[nextCh] != '>' && fragment[nextCh] != ' ' && fragment[nextCh] != '\t')) {
            pos = nextCh;
            continue;
        }
        const std::size_t gt = fragment.find('>', tStart);
        if (gt == std::string::npos) break;
        if (gt > 0 && fragment[gt - 1] == '/') {
            pos = gt + 1;
            continue;
        }
        const std::size_t close = fragment.find("</t>", gt + 1);
        if (close == std::string::npos) break;
        out += XmlUnescape(fragment.substr(gt + 1, close - (gt + 1)));
        pos = close + 4;
    }
    return out;
}

std::vector<std::string> ParseSharedStrings(const std::string& xml) {
    std::vector<std::string> strings;
    std::size_t pos = 0;
    while (pos < xml.size()) {
        const std::size_t siStart = xml.find("<si", pos);
        if (siStart == std::string::npos) break;
        const std::size_t gt = xml.find('>', siStart);
        if (gt == std::string::npos) break;
        const std::size_t siEnd = xml.find("</si>", gt + 1);
        if (siEnd == std::string::npos) break;
        strings.push_back(ExtractAllTextTags(xml.substr(gt + 1, siEnd - (gt + 1))));
        pos = siEnd + 5;
    }
    return strings;
}

std::vector<std::vector<std::string>> ParseWorksheetXml(
    const std::string& xml,
    const std::vector<std::string>& sharedStrings)
{
    std::vector<std::vector<std::string>> rows;
    std::size_t pos = 0;
    while (pos < xml.size()) {
        const std::size_t rStart = xml.find("<row", pos);
        if (rStart == std::string::npos) break;
        const std::size_t rGt = xml.find('>', rStart);
        if (rGt == std::string::npos) break;
        if (rGt > 0 && xml[rGt - 1] == '/') {
            pos = rGt + 1;
            continue;
        }
        const std::size_t rEnd = xml.find("</row>", rGt + 1);
        if (rEnd == std::string::npos) break;

        const std::string rowBody = xml.substr(rGt + 1, rEnd - (rGt + 1));
        std::vector<std::string> rowCells;
        std::size_t cPos = 0;
        while (cPos < rowBody.size()) {
            const std::size_t cStart = rowBody.find("<c", cPos);
            if (cStart == std::string::npos) break;
            if (cStart + 2 < rowBody.size() &&
                rowBody[cStart + 2] != ' ' &&
                rowBody[cStart + 2] != '>') {
                cPos = cStart + 2;
                continue;
            }
            const std::size_t cGt = rowBody.find('>', cStart);
            if (cGt == std::string::npos) break;
            const std::string cTag = rowBody.substr(cStart, cGt - cStart + 1);
            std::string cellBody;
            if (cGt > 0 && rowBody[cGt - 1] == '/') {
                cPos = cGt + 1;
            } else {
                const std::size_t cEnd = rowBody.find("</c>", cGt + 1);
                if (cEnd == std::string::npos) break;
                cellBody = rowBody.substr(cGt + 1, cEnd - (cGt + 1));
                cPos = cEnd + 4;
            }

            const std::string refAttr = ExtractAttr(cTag, "r");
            const std::string typeAttr = ExtractAttr(cTag, "t");
            const std::size_t colIdx = refAttr.empty() ? rowCells.size() : ColIndex0FromCellRef(refAttr);
            if (rowCells.size() <= colIdx) rowCells.resize(colIdx + 1);

            std::string value;
            if (typeAttr == "inlineStr") {
                value = ExtractAllTextTags(cellBody);
            } else {
                const std::size_t vOpen = cellBody.find("<v>");
                const std::size_t vClose = (vOpen != std::string::npos) ? cellBody.find("</v>", vOpen + 3) : std::string::npos;
                if (vOpen != std::string::npos && vClose != std::string::npos) {
                    const std::string rawV = XmlUnescape(cellBody.substr(vOpen + 3, vClose - (vOpen + 3)));
                    if (typeAttr == "s") {
                        try {
                            const std::size_t idx = static_cast<std::size_t>(std::stoul(rawV));
                            if (idx < sharedStrings.size()) value = sharedStrings[idx];
                        } catch (...) {}
                    } else if (typeAttr == "b") {
                        value = (rawV == "1") ? "TRUE" : "FALSE";
                    } else {
                        value = rawV;
                    }
                }
            }
            rowCells[colIdx] = std::move(value);
        }

        if (!rowCells.empty()) {
            rows.push_back(std::move(rowCells));
        }
        pos = rEnd + 6;
    }
    return rows;
}

} // namespace

bool ExportToCsvFile(
    const DocumentModel& doc,
    const std::string& outPathUtf8,
    std::string* outErr)
{
    const auto rows = CollectDocumentRows(doc);
    std::ostringstream oss;
    oss << "\xEF\xBB\xBF"; // UTF-8 BOM for spreadsheet compatibility
    oss << "Pagina,Origen,Dimensiones,Texto\r\n";
    for (const auto& r : rows) {
        oss << r.pageNum << ','
            << CsvEscape(r.source) << ','
            << CsvEscape(r.dimensions) << ','
            << CsvEscape(r.text) << "\r\n";
    }
    const std::string csvStr = oss.str();
    std::vector<std::uint8_t> bytes(csvStr.begin(), csvStr.end());
    return WriteFileBytes(outPathUtf8, bytes, outErr);
}

bool ImportFromCsvFile(
    const std::string& inPathUtf8,
    DocumentModel& outDoc,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytes(inPathUtf8, bytes, outErr)) return false;
    const std::string content(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    auto rows = ParseCsv(content);
    if (rows.empty()) {
        if (outErr) *outErr = "El archivo CSV esta vacio.";
        return false;
    }
    const std::string title = std::filesystem::u8path(inPathUtf8).stem().u8string();
    PopulateModelFromTable(outDoc, title.empty() ? "CSV Importado" : title, { { "CSV", rows } });
    return true;
}

bool ExportToXlsxFile(
    const DocumentModel& doc,
    const std::string& outPathUtf8,
    std::string* outErr)
{
    const auto rows = CollectDocumentRows(doc);

    ZipArchive zip;
    zip.SetTextEntry("[Content_Types].xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/worksheets/sheet1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>"
        "</Types>");

    zip.SetTextEntry("_rels/.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");

    zip.SetTextEntry("xl/workbook.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets><sheet name=\"DocumentStudio\" sheetId=\"1\" r:id=\"rId1\"/></sheets>"
        "</workbook>");

    zip.SetTextEntry("xl/_rels/workbook.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>");

    zip.SetTextEntry("xl/styles.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<fonts count=\"1\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"1\"><fill><patternFill patternType=\"none\"/></fill></fills>"
        "<borders count=\"1\"><border/></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/></cellXfs>"
        "</styleSheet>");

    std::ostringstream sheet;
    sheet << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
          << "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
          << "<sheetData>";

    auto emitCell = [&](std::size_t row1, std::size_t col0, const std::string& txt) {
        sheet << "<c r=\"" << ColNameFromIndex0(col0) << row1 << "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">"
              << XmlEscape(txt) << "</t></is></c>";
    };

    sheet << "<row r=\"1\">";
    emitCell(1, 0, "Pagina");
    emitCell(1, 1, "Origen");
    emitCell(1, 2, "Dimensiones");
    emitCell(1, 3, "Texto");
    sheet << "</row>";

    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::size_t row1 = i + 2;
        sheet << "<row r=\"" << row1 << "\">";
        sheet << "<c r=\"A" << row1 << "\"><v>" << rows[i].pageNum << "</v></c>";
        emitCell(row1, 1, rows[i].source);
        emitCell(row1, 2, rows[i].dimensions);
        emitCell(row1, 3, rows[i].text);
        sheet << "</row>";
    }
    sheet << "</sheetData></worksheet>";
    zip.SetTextEntry("xl/worksheets/sheet1.xml", sheet.str());

    std::vector<std::uint8_t> xlsxBytes;
    if (!zip.Serialize(xlsxBytes, true, outErr)) return false;
    return WriteFileBytes(outPathUtf8, xlsxBytes, outErr);
}

bool ImportFromXlsxFile(
    const std::string& inPathUtf8,
    DocumentModel& outDoc,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytes(inPathUtf8, bytes, outErr)) return false;

    ZipArchive zip;
    if (!zip.Deserialize(bytes.data(), bytes.size(), outErr)) return false;

    std::vector<std::string> sharedStrings;
    if (const auto* ssEntry = zip.FindEntry("xl/sharedStrings.xml")) {
        const std::string ssXml(reinterpret_cast<const char*>(ssEntry->data()), ssEntry->size());
        sharedStrings = ParseSharedStrings(ssXml);
    }

    std::vector<std::string> sheetPaths;
    for (const auto& name : zip.ListEntries()) {
        if (name.rfind("xl/worksheets/sheet", 0) == 0 && name.size() > 4 &&
            name.substr(name.size() - 4) == ".xml") {
            sheetPaths.push_back(name);
        }
    }
    std::sort(sheetPaths.begin(), sheetPaths.end());

    if (sheetPaths.empty()) {
        if (outErr) *outErr = "No se encontraron hojas de calculo en el archivo .xlsx.";
        return false;
    }

    std::vector<std::pair<std::string, std::vector<std::vector<std::string>>>> parsedSheets;
    for (std::size_t s = 0; s < sheetPaths.size(); ++s) {
        const auto* entry = zip.FindEntry(sheetPaths[s]);
        if (!entry) continue;
        const std::string xml(reinterpret_cast<const char*>(entry->data()), entry->size());
        auto rows = ParseWorksheetXml(xml, sharedStrings);
        if (!rows.empty()) {
            parsedSheets.push_back({ "Hoja " + std::to_string(s + 1), std::move(rows) });
        }
    }

    if (parsedSheets.empty()) {
        if (outErr) *outErr = "El libro .xlsx no contiene filas de texto.";
        return false;
    }

    const std::string title = std::filesystem::u8path(inPathUtf8).stem().u8string();
    PopulateModelFromTable(outDoc, title.empty() ? "Excel Importado" : title, parsedSheets);
    return true;
}

} // namespace DocStudioApp
