#include "table_interop.hpp"
#include "../core/zip_container.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <map>
#include <regex>
#include <set>
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

bool DetectCyclicTableFormulas(
    const std::vector<std::vector<std::string>>& grid,
    std::string* outCycleDiag)
{
    auto cellKey = [&](std::size_t r0, std::size_t c0) -> std::string {
        return ColNameFromIndex0(c0) + std::to_string(r0 + 1);
    };
    static const std::regex kCellRefRe(R"(\b([A-Za-z]{1,3})([1-9][0-9]*)\b)");
    std::map<std::string, std::vector<std::string>> adj;

    for (std::size_t r = 0; r < grid.size(); ++r) {
        for (std::size_t c = 0; c < grid[r].size(); ++c) {
            const std::string& cell = grid[r][c];
            if (cell.empty() || cell[0] != '=') continue;
            const std::string src = cellKey(r, c);
            auto begin = std::sregex_iterator(cell.begin(), cell.end(), kCellRefRe);
            auto end = std::sregex_iterator();
            for (auto it = begin; it != end; ++it) {
                std::string col = (*it)[1].str();
                for (char& ch : col) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                std::string dst = col + (*it)[2].str();
                adj[src].push_back(dst);
                // Also support 1-based data row alias when row 0 is header
                if (r > 0) {
                    const std::string dataSrc = ColNameFromIndex0(c) + std::to_string(r);
                    if (dataSrc != src) adj[dataSrc].push_back(dst);
                }
            }
        }
    }

    // 3-color DFS (0=unvisited, 1=visiting, 2=done)
    std::map<std::string, int> state;
    std::function<bool(const std::string&)> hasCycle = [&](const std::string& u) -> bool {
        state[u] = 1;
        auto it = adj.find(u);
        if (it != adj.end()) {
            for (const auto& v : it->second) {
                const int st = state[v];
                if (st == 1) {
                    if (outCycleDiag) *outCycleDiag = "Cyclic formula reference: " + u + " -> " + v;
                    return true;
                }
                if (st == 0 && hasCycle(v)) return true;
            }
        }
        state[u] = 2;
        return false;
    };

    for (const auto& kv : adj) {
        if (state[kv.first] == 0 && hasCycle(kv.first)) return true;
    }
    return false;
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
    std::string cycleDiag;
    if (DetectCyclicTableFormulas(rows, &cycleDiag)) {
        if (outErr) *outErr = cycleDiag;
        return false;
    }

    // Optional OLS linear regression + R^2 on first two numeric columns
    std::vector<double> xs, ys;
    for (std::size_t r = 1; r < rows.size(); ++r) {
        if (rows[r].size() < 2) continue;
        try {
            std::size_t p1 = 0, p2 = 0;
            const double x = std::stod(rows[r][0], &p1);
            const double y = std::stod(rows[r][1], &p2);
            if (p1 > 0 && p2 > 0 && std::isfinite(x) && std::isfinite(y)) {
                xs.push_back(x);
                ys.push_back(y);
            }
        } catch (...) {}
    }
    if (xs.size() >= 2) {
        const double n = static_cast<double>(xs.size());
        double sx = 0.0, sy = 0.0, sxx = 0.0, sxy = 0.0, syy = 0.0;
        for (std::size_t i = 0; i < xs.size(); ++i) {
            sx += xs[i]; sy += ys[i];
            sxx += xs[i] * xs[i]; sxy += xs[i] * ys[i]; syy += ys[i] * ys[i];
        }
        const double denom = n * sxx - sx * sx;
        if (std::abs(denom) > 1e-12) {
            const double m = (n * sxy - sx * sy) / denom;
            const double b = (sy - m * sx) / n;
            double ssRes = 0.0, ssTot = 0.0;
            const double meanY = sy / n;
            for (std::size_t i = 0; i < xs.size(); ++i) {
                const double e = ys[i] - (m * xs[i] + b);
                ssRes += e * e;
                ssTot += (ys[i] - meanY) * (ys[i] - meanY);
            }
            const double r2 = (ssTot > 1e-12) ? (1.0 - ssRes / ssTot) : 1.0;
            std::ostringstream fit;
            fit << std::fixed << std::setprecision(4)
                << "OLS: y = " << m << "*x + " << b << " (R^2=" << r2 << ")";
            rows.push_back({ "Regresion OLS", fit.str() });
        }
    }

    const auto stemU8 = std::filesystem::u8path(inPathUtf8).stem().u8string();
    const std::string title(reinterpret_cast<const char*>(stemU8.data()), stemU8.size());
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

    const auto stemU8 = std::filesystem::u8path(inPathUtf8).stem().u8string();
    const std::string title(reinterpret_cast<const char*>(stemU8.data()), stemU8.size());
    PopulateModelFromTable(outDoc, title.empty() ? "Excel Importado" : title, parsedSheets);
    return true;
}

bool ImportFromMarkdownFile(
    const std::string& inPathUtf8,
    DocumentModel& outDoc,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBytes(inPathUtf8, bytes, outErr)) return false;
    const std::string md(bytes.begin(), bytes.end());
    const auto stemU8 = std::filesystem::u8path(inPathUtf8).stem().u8string();
    const std::string stemTitle(reinterpret_cast<const char*>(stemU8.data()), stemU8.size());
    return ImportFromMarkdownText(md, stemTitle, outDoc);
}

bool ImportFromMarkdownText(const std::string& md, const std::string& title, DocumentModel& outDoc) {
    outDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
    outDoc.SetTitle(title.empty() ? "Markdown Importado" : title);

    std::istringstream iss(md);
    std::string line;
    double curY = 48.0;
    bool inCode = false;
    std::string codeBuf;
    std::string codeLang;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.rfind("```", 0) == 0) {
            if (!inCode) {
                inCode = true;
                codeLang = line.substr(3);
                codeBuf.clear();
            } else {
                inCode = false;
                AnnotationObject obj;
                obj.kind = ObjectKind::TextBox;
                obj.bounds = { 48.0, curY, 520.0, 90.0 };
                obj.textUtf8 = "[" + (codeLang.empty() ? "code" : codeLang) + "]\n" + codeBuf;
                obj.strokeColor = DocColor::RGBA(15, 23, 42, 255);
                obj.fillColor = DocColor::RGBA(241, 245, 249, 255);
                obj.fontSizePt = 11.0;
                outDoc.AddCanvasObject(std::move(obj));
                curY += 108.0;
            }
            continue;
        }
        if (inCode) {
            if (!codeBuf.empty()) codeBuf += "\n";
            codeBuf += line;
            continue;
        }
        if (line.empty()) continue;

        if (line.size() >= 4 && line.rfind("$$", 0) == 0 && line.substr(line.size() - 2) == "$$") {
            AnnotationObject mathObj;
            mathObj.kind = ObjectKind::MathBlock;
            mathObj.bounds = { 48.0, curY, 440.0, 86.0 };
            mathObj.formulaExprUtf8 = line.substr(2, line.size() - 4);
            mathObj.evaluatedResultUtf8 = mathObj.formulaExprUtf8;
            mathObj.textUtf8 = mathObj.formulaExprUtf8;
            outDoc.AddCanvasObject(std::move(mathObj));
            curY += 102.0;
            continue;
        }

        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        const bool isHeading = (!line.empty() && line[0] == '#');
        std::string txt = line;
        while (!txt.empty() && (txt[0] == '#' || txt[0] == ' ')) {
            if (txt[0] == '#') txt.erase(txt.begin());
            else if (isHeading) { txt.erase(txt.begin()); break; }
            else break;
        }
        tb.bounds = { 48.0, curY, 520.0, isHeading ? 36.0 : 28.0 };
        tb.textUtf8 = txt;
        tb.bold = isHeading;
        tb.fontSizePt = isHeading ? 18.0 : 12.0;
        tb.strokeColor = DocColor::RGBA(15, 23, 42, 255);
        outDoc.AddCanvasObject(std::move(tb));
        curY += isHeading ? 44.0 : 34.0;
    }
    return true;
}

bool ExportToDxfFile(
    const DocumentModel& doc,
    const std::string& outPathUtf8,
    const std::string& scaleRatioUtf8,
    std::string* outErr)
{
    double scaleFactor = 1.0;
    const auto colon = scaleRatioUtf8.find(':');
    if (colon != std::string::npos) {
        try {
            const double num = std::stod(scaleRatioUtf8.substr(0, colon));
            const double den = std::stod(scaleRatioUtf8.substr(colon + 1));
            if (num > 0.0 && den > 0.0) scaleFactor = den / num;
        } catch (...) {}
    }

    std::ostringstream dxf;
    dxf << "0\nSECTION\n2\nHEADER\n"
        << "9\n$ACADVER\n1\nAC1015\n"
        << "9\n$INSUNITS\n70\n4\n" // millimeters
        << "9\n$DIMLFAC\n40\n" << scaleFactor << "\n"
        << "0\nENDSEC\n"
        << "0\nSECTION\n2\nTABLES\n0\nENDSEC\n"
        << "0\nSECTION\n2\nENTITIES\n";

    auto emitLine = [&](double x0, double y0, double x1, double y1, const char* layer) {
        dxf << "0\nLINE\n8\n" << layer << "\n"
            << "10\n" << (x0 * scaleFactor) << "\n20\n" << (-y0 * scaleFactor) << "\n30\n0.0\n"
            << "11\n" << (x1 * scaleFactor) << "\n21\n" << (-y1 * scaleFactor) << "\n31\n0.0\n";
    };
    auto emitText = [&](double x, double y, double h, const std::string& text, const char* layer) {
        std::string safe = text;
        for (char& c : safe) if (c == '\r' || c == '\n') c = ' ';
        dxf << "0\nTEXT\n8\n" << layer << "\n"
            << "10\n" << (x * scaleFactor) << "\n20\n" << (-y * scaleFactor) << "\n30\n0.0\n"
            << "40\n" << (h * scaleFactor) << "\n1\n" << safe << "\n";
    };

    auto emitObjList = [&](const std::vector<AnnotationObject>& objs) {
        for (const auto& o : objs) {
            if (o.kind == ObjectKind::InkStroke && o.inkPoints.size() >= 2) {
                for (std::size_t i = 1; i < o.inkPoints.size(); ++i) {
                    emitLine(o.inkPoints[i - 1].x, o.inkPoints[i - 1].y, o.inkPoints[i].x, o.inkPoints[i].y, "INK");
                }
            } else if (o.kind == ObjectKind::Shape) {
                if (o.shapeKind == ShapeKind::Line || o.shapeKind == ShapeKind::Arrow) {
                    emitLine(o.shapeStart.x, o.shapeStart.y, o.shapeEnd.x, o.shapeEnd.y, "GEOMETRY");
                } else {
                    const auto& b = o.bounds;
                    emitLine(b.x, b.y, b.x + b.w, b.y, "GEOMETRY");
                    emitLine(b.x + b.w, b.y, b.x + b.w, b.y + b.h, "GEOMETRY");
                    emitLine(b.x + b.w, b.y + b.h, b.x, b.y + b.h, "GEOMETRY");
                    emitLine(b.x, b.y + b.h, b.x, b.y, "GEOMETRY");
                }
            } else if (!o.textUtf8.empty() || !o.formulaExprUtf8.empty()) {
                emitText(o.bounds.x, o.bounds.y, 12.0,
                    !o.formulaExprUtf8.empty() ? o.formulaExprUtf8 : o.textUtf8, "ANNOTATIONS");
            }
        }
    };

    emitObjList(doc.CanvasObjects());
    for (const auto& pg : doc.Pages()) emitObjList(pg.objects);
    // Emit scale title block reference line so even an empty canvas produces a valid scaled CAD frame
    emitLine(0.0, 0.0, 297.0, 0.0, "FRAME");
    emitLine(297.0, 0.0, 297.0, 210.0, "FRAME");
    emitLine(297.0, 210.0, 0.0, 210.0, "FRAME");
    emitLine(0.0, 210.0, 0.0, 0.0, "FRAME");
    emitText(10.0, 195.0, 5.0, "Scale " + scaleRatioUtf8 + " - " + doc.Title(), "FRAME");

    dxf << "0\nENDSEC\n0\nEOF\n";
    const std::string s = dxf.str();
    std::vector<std::uint8_t> out(s.begin(), s.end());
    return WriteFileBytes(outPathUtf8, out, outErr);
}

} // namespace DocStudioApp
