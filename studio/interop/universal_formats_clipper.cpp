#include "universal_formats_clipper.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/image_processing.hpp"
#include "../core/utf_conv.hpp"
#include "../pdf/pdf_parser.hpp"
#include "../pdf/pdf_writer.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio {

namespace {

std::string TrimStr(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string StripHtmlTags(const std::string& html) {
    std::string out;
    out.reserve(html.size());
    bool inTag = false;
    for (char c : html) {
        if (c == '<') { inTag = true; continue; }
        if (c == '>') { inTag = false; continue; }
        if (!inTag) out.push_back(c);
    }
    return TrimStr(out);
}

std::string ExtractBetweenIcase(const std::string& html, const std::string& openTag, const std::string& closeTag) {
    std::string lowHtml = html;
    std::transform(lowHtml.begin(), lowHtml.end(), lowHtml.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string lowOpen = openTag;
    std::transform(lowOpen.begin(), lowOpen.end(), lowOpen.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::string lowClose = closeTag;
    std::transform(lowClose.begin(), lowClose.end(), lowClose.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    const auto p0 = lowHtml.find(lowOpen);
    if (p0 == std::string::npos) return {};
    const auto s = p0 + lowOpen.size();
    const auto p1 = lowHtml.find(lowClose, s);
    if (p1 == std::string::npos) return {};
    return TrimStr(html.substr(s, p1 - s));
}

std::string EscapeXmlText(const std::string& s) {
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

bool EndsWithIcase(const std::string& str, const std::string& suffix) {
    if (str.size() < suffix.size()) return false;
    const std::size_t off = str.size() - suffix.size();
    for (std::size_t i = 0; i < suffix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(str[off + i])) !=
            std::tolower(static_cast<unsigned char>(suffix[i]))) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> TokenizeBm25(const std::string& textUtf8) {
    static const std::set<std::string> kStopWords = {
        "el", "la", "los", "las", "un", "una", "unos", "unas", "de", "del", "al", "en",
        "con", "por", "para", "como", "cual", "donde", "que", "es", "son", "se", "su",
        "the", "and", "for", "with", "that", "this", "from", "what", "how", "are"
    };
    const std::string folded = FoldUtf8ForSearch(textUtf8, false, true);
    std::vector<std::string> toks;
    std::string cur;
    for (unsigned char c : folded) {
        if (std::isalnum(c)) {
            cur.push_back(static_cast<char>(c));
        } else if (!cur.empty()) {
            if (cur.size() >= 2 && kStopWords.find(cur) == kStopWords.end()) toks.push_back(cur);
            cur.clear();
        }
    }
    if (cur.size() >= 2 && kStopWords.find(cur) == kStopWords.end()) toks.push_back(cur);
    return toks;
}

#ifdef _WIN32
bool WinHttpRequest(
    const std::string& urlUtf8,
    const wchar_t* method,
    const std::string* optionalPostBody,
    const std::string& optionalBearerToken,
    int timeoutMs,
    std::string& outResponseUtf8,
    std::string* outErr)
{
    outResponseUtf8.clear();
    const std::wstring wUrl = Utf8ToWide(urlUtf8);
    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);
    wchar_t hostBuf[512]{};
    wchar_t pathBuf[2048]{};
    uc.lpszHostName = hostBuf;
    uc.dwHostNameLength = static_cast<DWORD>(std::size(hostBuf));
    uc.lpszUrlPath = pathBuf;
    uc.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuf));

    if (!WinHttpCrackUrl(wUrl.c_str(), static_cast<DWORD>(wUrl.size()), 0, &uc)) {
        if (outErr) *outErr = "URL inv\xC3\xA1lida: " + urlUtf8;
        return false;
    }

    HINTERNET hSession = WinHttpOpen(L"CuadernU/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        if (outErr) *outErr = "WinHttpOpen fall\xC3\xB3.";
        return false;
    }
    const int to = (std::max)(1500, timeoutMs);
    WinHttpSetTimeouts(hSession, to, to, to, to);

    HINTERNET hConnect = WinHttpConnect(hSession, hostBuf, uc.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        if (outErr) *outErr = "No se pudo conectar al host: " + WideToUtf8(hostBuf);
        return false;
    }

    const DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hReq = WinHttpOpenRequest(hConnect, method, pathBuf[0] ? pathBuf : L"/", nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hReq) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        if (outErr) *outErr = "WinHttpOpenRequest fall\xC3\xB3.";
        return false;
    }

    std::wstring headers;
    if (optionalPostBody) {
        headers += L"Content-Type: application/json; charset=utf-8\r\n";
    }
    if (!optionalBearerToken.empty()) {
        headers += L"Authorization: Bearer " + Utf8ToWide(optionalBearerToken) + L"\r\n";
    }

    const BOOL sent = WinHttpSendRequest(
        hReq,
        headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
        headers.empty() ? 0 : static_cast<DWORD>(headers.size()),
        optionalPostBody ? const_cast<char*>(optionalPostBody->data()) : WINHTTP_NO_REQUEST_DATA,
        optionalPostBody ? static_cast<DWORD>(optionalPostBody->size()) : 0,
        optionalPostBody ? static_cast<DWORD>(optionalPostBody->size()) : 0,
        0);

    bool ok = false;
    if (sent && WinHttpReceiveResponse(hReq, nullptr)) {
        DWORD statusCode = 0;
        DWORD szStatus = sizeof(statusCode);
        WinHttpQueryHeaders(hReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &szStatus, WINHTTP_NO_HEADER_INDEX);

        constexpr std::size_t kMaxBytes = 4u * 1024u * 1024u;
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hReq, &avail) && avail > 0 && outResponseUtf8.size() < kMaxBytes) {
            const DWORD toRead = (std::min<DWORD>)(avail, static_cast<DWORD>(kMaxBytes - outResponseUtf8.size()));
            std::string chunk(toRead, '\0');
            DWORD got = 0;
            if (!WinHttpReadData(hReq, chunk.data(), toRead, &got) || got == 0) break;
            chunk.resize(got);
            outResponseUtf8 += chunk;
        }
        if (statusCode >= 200 && statusCode < 400) {
            ok = true;
        } else if (outErr) {
            *outErr = "HTTP " + std::to_string(statusCode) + " desde " + urlUtf8;
        }
    } else if (outErr) {
        *outErr = "Sin respuesta HTTP de " + urlUtf8 + " (WinError " + std::to_string(GetLastError()) + ")";
    }

    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}
#endif

} // namespace

// ---------------- UniversalFormatsInterop ----------------

std::string UniversalFormatsInterop::ExportToJsonCanvas(const DocumentModel& doc) {
    json root;
    json nodes = json::array();
    json edges = json::array();

    const auto& objs = (doc.Mode() == DocumentMode::InfiniteCanvas && !doc.CanvasObjects().empty())
        ? doc.CanvasObjects()
        : (doc.Pages().empty() ? doc.CanvasObjects() : doc.Pages()[0].objects);

    for (const auto& o : objs) {
        if (o.kind == ObjectKind::Shape && o.shapeKind == ShapeKind::Connector && o.connectorFromId != 0 && o.connectorToId != 0) {
            edges.push_back({
                {"id", "edge_" + std::to_string(o.id)},
                {"fromNode", "node_" + std::to_string(o.connectorFromId)},
                {"toNode", "node_" + std::to_string(o.connectorToId)},
                {"label", o.textUtf8}
            });
            continue;
        }
        std::string typeStr = "text";
        std::string textVal = o.textUtf8;
        if (o.kind == ObjectKind::MathBlock && !o.formulaExprUtf8.empty()) {
            textVal = "$$" + o.formulaExprUtf8 + " = " + o.evaluatedResultUtf8 + "$$";
        } else if (o.kind == ObjectKind::PdfCard) {
            typeStr = "file";
            textVal = o.pdfSourcePathUtf8;
        }
        nodes.push_back({
            {"id", "node_" + std::to_string(o.id)},
            {"type", typeStr},
            {"x", static_cast<int>(std::lround(o.bounds.x))},
            {"y", static_cast<int>(std::lround(o.bounds.y))},
            {"width", static_cast<int>(std::lround((std::max)(80.0, o.bounds.w)))},
            {"height", static_cast<int>(std::lround((std::max)(40.0, o.bounds.h)))},
            {"text", textVal}
        });
    }
    root["nodes"] = std::move(nodes);
    root["edges"] = std::move(edges);
    return root.dump(2);
}

bool UniversalFormatsInterop::ImportFromJsonCanvas(
    const std::string& jsonCanvasUtf8,
    DocumentModel& outDoc,
    std::string* outErr)
{
    try {
        const json root = json::parse(jsonCanvasUtf8);
        if (!root.is_object() || !root.contains("nodes") || !root["nodes"].is_array()) {
            if (outErr) *outErr = "JSON Canvas invalido: falta arreglo 'nodes'.";
            return false;
        }
        outDoc.ResetToBlank(DocumentMode::InfiniteCanvas);
        outDoc.SetTitle("JSON Canvas");

        std::unordered_map<std::string, std::uint64_t> strToId;
        for (const auto& n : root["nodes"]) {
            AnnotationObject obj;
            obj.kind = ObjectKind::TextBox;
            obj.bounds.x = n.value("x", 0.0);
            obj.bounds.y = n.value("y", 0.0);
            obj.bounds.w = (std::max)(60.0, n.value("width", 180.0));
            obj.bounds.h = (std::max)(36.0, n.value("height", 80.0));
            obj.textUtf8 = n.value("text", n.value("file", ""));
            outDoc.AddCanvasObject(std::move(obj));
            const std::uint64_t assignedId = outDoc.CanvasObjects().back().id;
            strToId[n.value("id", "")] = assignedId;
        }

        if (root.contains("edges") && root["edges"].is_array()) {
            for (const auto& e : root["edges"]) {
                const std::string fromStr = e.value("fromNode", "");
                const std::string toStr = e.value("toNode", "");
                if (strToId.count(fromStr) && strToId.count(toStr)) {
                    AnnotationObject conn;
                    conn.kind = ObjectKind::Shape;
                    conn.shapeKind = ShapeKind::Connector;
                    conn.connectorFromId = strToId[fromStr];
                    conn.connectorToId = strToId[toStr];
                    conn.textUtf8 = e.value("label", "");
                    if (const auto* a = outDoc.FindCanvasObject(conn.connectorFromId)) {
                        conn.shapeStart = { a->bounds.Right(), a->bounds.y + a->bounds.h * 0.5 };
                    }
                    if (const auto* b = outDoc.FindCanvasObject(conn.connectorToId)) {
                        conn.shapeEnd = { b->bounds.x, b->bounds.y + b->bounds.h * 0.5 };
                    }
                    conn.RecomputeBounds();
                    outDoc.AddCanvasObject(std::move(conn));
                }
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

std::string UniversalFormatsInterop::ExportPageToSvg(const DocumentModel& doc, std::size_t pageIndex0) {
    if (pageIndex0 >= doc.Pages().size()) return {};
    const auto& pg = doc.Pages()[pageIndex0];
    std::ostringstream svg;
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << pg.widthPt << "\" height=\"" << pg.heightPt
        << "\" viewBox=\"0 0 " << pg.widthPt << " " << pg.heightPt << "\">\n"
        << "  <rect x=\"0\" y=\"0\" width=\"" << pg.widthPt << "\" height=\"" << pg.heightPt << "\" fill=\"#FFFFFF\"/>\n";

    for (const auto& o : pg.objects) {
        if (!doc.IsLayerVisible(o.layerId)) continue;
        char colHex[16]{};
        std::snprintf(colHex, sizeof(colHex), "#%02X%02X%02X", o.strokeColor.r, o.strokeColor.g, o.strokeColor.b);

        if (o.kind == ObjectKind::InkStroke && o.inkPoints.size() >= 2) {
            svg << "  <polyline fill=\"none\" stroke=\"" << colHex << "\" stroke-width=\"" << o.strokeWidth
                << "\" stroke-linecap=\"round\" stroke-linejoin=\"round\" points=\"";
            for (std::size_t i = 0; i < o.inkPoints.size(); ++i) {
                if (i > 0) svg << " ";
                svg << o.inkPoints[i].x << "," << o.inkPoints[i].y;
            }
            svg << "\"/>\n";
        } else if (o.kind == ObjectKind::Shape) {
            if (o.shapeKind == ShapeKind::Line || o.shapeKind == ShapeKind::Arrow || o.shapeKind == ShapeKind::Connector) {
                svg << "  <line x1=\"" << o.shapeStart.x << "\" y1=\"" << o.shapeStart.y
                    << "\" x2=\"" << o.shapeEnd.x << "\" y2=\"" << o.shapeEnd.y
                    << "\" stroke=\"" << colHex << "\" stroke-width=\"" << o.strokeWidth << "\"/>\n";
            } else if (o.shapeKind == ShapeKind::Ellipse) {
                svg << "  <ellipse cx=\"" << (o.bounds.x + o.bounds.w * 0.5) << "\" cy=\"" << (o.bounds.y + o.bounds.h * 0.5)
                    << "\" rx=\"" << (o.bounds.w * 0.5) << "\" ry=\"" << (o.bounds.h * 0.5)
                    << "\" fill=\"none\" stroke=\"" << colHex << "\" stroke-width=\"" << o.strokeWidth << "\"/>\n";
            } else {
                svg << "  <rect x=\"" << o.bounds.x << "\" y=\"" << o.bounds.y
                    << "\" width=\"" << o.bounds.w << "\" height=\"" << o.bounds.h
                    << "\" fill=\"none\" stroke=\"" << colHex << "\" stroke-width=\"" << o.strokeWidth << "\"/>\n";
            }
        } else if (!o.textUtf8.empty() || !o.formulaExprUtf8.empty()) {
            const std::string txt = !o.formulaExprUtf8.empty()
                ? (o.formulaExprUtf8 + " = " + o.evaluatedResultUtf8)
                : o.textUtf8;
            svg << "  <text x=\"" << o.bounds.x << "\" y=\"" << (o.bounds.y + 16.0)
                << "\" fill=\"" << colHex << "\" font-family=\"Segoe UI\" font-size=\"" << o.fontSizePt << "\">"
                << EscapeXmlText(txt) << "</text>\n";
        }
    }
    svg << "</svg>\n";
    return svg.str();
}

std::string UniversalFormatsInterop::ExportStudyDeckToDelimited(const StudyDeck& deck, char delimiter) {
    std::ostringstream oss;
    for (const auto& c : deck.cards) {
        std::string q = c.questionUtf8;
        std::string a = c.answerUtf8;
        std::replace(q.begin(), q.end(), '\n', ' ');
        std::replace(a.begin(), a.end(), '\n', ' ');
        oss << q << delimiter << a << "\n";
    }
    return oss.str();
}

StudyDeck UniversalFormatsInterop::ImportStudyDeckFromDelimited(
    const std::string& contentUtf8,
    char delimiter,
    const std::string& deckTitleUtf8)
{
    StudyDeck deck;
    deck.titleUtf8 = deckTitleUtf8;
    std::istringstream iss(contentUtf8);
    std::string line;
    std::uint64_t nextId = 1;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (TrimStr(line).empty()) continue;
        const auto pos = line.find(delimiter);
        if (pos == std::string::npos) continue;
        FlashcardItem item;
        item.id = nextId++;
        item.questionUtf8 = TrimStr(line.substr(0, pos));
        item.answerUtf8 = TrimStr(line.substr(pos + 1));
        if (!item.questionUtf8.empty()) {
            deck.cards.push_back(std::move(item));
        }
    }
    return deck;
}

std::string UniversalFormatsInterop::ExportPageToOneNoteGraphHtml(const DocumentModel& doc, std::size_t pageIndex0) {
    if (pageIndex0 >= doc.Pages().size()) return {};
    const auto& pg = doc.Pages()[pageIndex0];
    std::ostringstream html;
    html << "<!DOCTYPE html>\n<html>\n<head>\n<title>" << EscapeXmlText(doc.Title()) << "</title>\n"
         << "<meta name=\"created\" content=\"2026-11-20T12:00:00Z\" />\n</head>\n"
         << "<body data-absolute-enabled=\"true\" style=\"font-family:Calibri;font-size:11pt\">\n";
    for (const auto& o : pg.objects) {
        if (o.textUtf8.empty() && o.formulaExprUtf8.empty()) continue;
        const std::string txt = !o.formulaExprUtf8.empty()
            ? (o.formulaExprUtf8 + " = " + o.evaluatedResultUtf8)
            : o.textUtf8;
        html << "  <div style=\"position:absolute;left:" << static_cast<int>(o.bounds.x)
             << "px;top:" << static_cast<int>(o.bounds.y) << "px;width:" << static_cast<int>((std::max)(120.0, o.bounds.w)) << "px\">\n"
             << "    <p>" << EscapeXmlText(txt) << "</p>\n"
             << "  </div>\n";
    }
    html << "</body>\n</html>\n";
    return html.str();
}

bool UniversalFormatsInterop::ImportPageFromOneNoteGraphHtml(
    const std::string& htmlUtf8,
    DocumentModel& outDoc,
    std::string* outErr)
{
    if (htmlUtf8.find("<html") == std::string::npos && htmlUtf8.find("<body") == std::string::npos) {
        if (outErr) *outErr = "HTML de OneNote/Graph invalido.";
        return false;
    }
    outDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    const std::string title = ExtractBetweenIcase(htmlUtf8, "<title>", "</title>");
    outDoc.SetTitle(title.empty() ? "Pagina OneNote Graph" : title);

    std::size_t pos = 0;
    double curY = 64.0;
    while ((pos = htmlUtf8.find("<p>", pos)) != std::string::npos) {
        const auto endP = htmlUtf8.find("</p>", pos + 3);
        if (endP == std::string::npos) break;
        const std::string pTxt = StripHtmlTags(htmlUtf8.substr(pos + 3, endP - (pos + 3)));
        if (!pTxt.empty()) {
            AnnotationObject tb;
            tb.kind = ObjectKind::TextBox;
            tb.bounds = { 48.0, curY, 440.0, 36.0 };
            tb.textUtf8 = pTxt;
            outDoc.AddPageObject(0, std::move(tb));
            curY += 48.0;
        }
        pos = endP + 4;
    }
    return true;
}

bool UniversalFormatsInterop::ExportUniversalNotePackage(
    const DocumentModel& doc,
    const std::wstring& outFilePath,
    const std::string& formatExtensionUtf8,
    std::string* outErr)
{
    if (outFilePath.empty()) {
        if (outErr) *outErr = "Ruta de paquete vacia.";
        return false;
    }
    std::vector<std::uint8_t> cudocBytes;
    if (!CudocStorage::SerializeToBytes(doc, cudocBytes, outErr)) return false;

    std::vector<std::uint8_t> pdfBytes;
    PdfWriter::ExportDocumentToBytes(doc, pdfBytes, nullptr);

    ZipArchive zip;
    json manifest = {
        {"format", formatExtensionUtf8},
        {"containerKind", "cuadernu_portable_package_v1"},
        {"title", doc.Title()},
        {"pages", doc.Pages().size()},
        {"generator", "CuadernU Universal Interop"}
    };
    zip.SetTextEntry("package_manifest.json", manifest.dump(2));
    zip.SetEntry("payload.cudoc", std::move(cudocBytes));
    if (!pdfBytes.empty()) zip.SetEntry("document.pdf", std::move(pdfBytes));

    std::vector<std::uint8_t> zipBytes;
    if (!zip.Serialize(zipBytes, true, outErr)) return false;

    const fs::path p(WideToUtf8(outFilePath));
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(zipBytes.data()), static_cast<std::streamsize>(zipBytes.size()));
    ofs.flush();
    return ofs.good();
}

bool UniversalFormatsInterop::ImportUniversalNotePackage(
    const std::wstring& inFilePath,
    DocumentModel& outDoc,
    std::string* outDetectedFormatUtf8,
    std::string* outErr)
{
    std::ifstream ifs(fs::path(WideToUtf8(inFilePath)), std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "Archivo de paquete no encontrado.";
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz <= 0) return false;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!ifs.read(reinterpret_cast<char*>(raw.data()), sz)) return false;

    ZipArchive zip;
    if (!zip.Deserialize(raw.data(), raw.size(), outErr)) return false;

    // 1. CuadernU portable package (native roundtrip)
    if (const auto* mf = zip.FindEntry("package_manifest.json")) {
        try {
            const json j = json::parse(std::string(mf->begin(), mf->end()));
            if (outDetectedFormatUtf8) *outDetectedFormatUtf8 = j.value("format", "cuadernu-pkg");
        } catch (...) {}
    }
    if (const auto* cudoc = zip.FindEntry("payload.cudoc")) {
        return CudocStorage::DeserializeFromBytes(cudoc->data(), cudoc->size(), outDoc, outErr);
    }
    if (const auto* cudoc2 = zip.FindEntry("nb_payload.cudoc")) {
        return CudocStorage::DeserializeFromBytes(cudoc2->data(), cudoc2->size(), outDoc, outErr);
    }

    // 2. Real third-party ZIP archive inspection (.goodnotes, .note, .nebo, .ntb, .nsa):
    //    Extract real embedded PDFs, PNG/JPG attachments, and readable text/Markdown/JSON notes!
    const auto entries = zip.ListEntries();
    bool importedAnything = false;
    bool looksLikeGoodnotes = false;
    for (const auto& name : entries) {
        if (name.find("attachments/") == 0 || name.find("notes/") == 0 || name == "index.notes.pb") {
            looksLikeGoodnotes = true;
        }
    }

    outDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    outDoc.SetTitle(fs::path(WideToUtf8(inFilePath)).stem().u8string());
    outDoc.MutablePages().clear();

    for (const auto& name : entries) {
        const auto* bytes = zip.FindEntry(name);
        if (!bytes || bytes->empty()) continue;

        // Real embedded PDF inside Goodnotes/Notability/Noteshelf archive
        if ((EndsWithIcase(name, ".pdf") ||
             (bytes->size() >= 5 && std::memcmp(bytes->data(), "%PDF-", 5) == 0))) {
            PdfDocument pdf;
            if (pdf.LoadFromBytes(*bytes)) {
                const std::string assetKey = outDoc.RegisterAsset(*bytes);
                for (std::size_t p = 0; p < pdf.PageCount(); ++p) {
                    const auto& pi = pdf.PageAt(p);
                    PageModel pm;
                    pm.pageId = outDoc.AllocatePageId();
                    pm.widthPt = pi.mediaBox.w > 0 ? pi.mediaBox.w : 595.0;
                    pm.heightPt = pi.mediaBox.h > 0 ? pi.mediaBox.h : 842.0;
                    pm.hasPdfSource = true;
                    pm.sourcePdfPathUtf8 = name;
                    pm.sourcePdfAssetKey = assetKey;
                    pm.sourcePageIndex0 = static_cast<int>(p);
                    pm.extractedTextUtf8 = pi.extractedTextUtf8;
                    pm.textBoxes = pi.textBoxes;
                    outDoc.MutablePages().push_back(std::move(pm));
                    importedAnything = true;
                }
            }
        } else if (EndsWithIcase(name, ".png") || EndsWithIcase(name, ".jpg") || EndsWithIcase(name, ".jpeg") ||
                   (bytes->size() >= 8 && bytes->at(0) == 0x89 && bytes->at(1) == 'P' && bytes->at(2) == 'N' && bytes->at(3) == 'G')) {
            PageModel pm;
            pm.pageId = outDoc.AllocatePageId();
            AnnotationObject img;
            img.id = outDoc.AllocateObjectId();
            img.zOrder = outDoc.AllocateZOrder();
            img.kind = ObjectKind::Image;
            img.bounds = { 36.0, 36.0, 520.0, 380.0 };
            img.assetKey = outDoc.RegisterAsset(*bytes);
            img.captionUtf8 = name;
            pm.objects.push_back(std::move(img));
            outDoc.MutablePages().push_back(std::move(pm));
            importedAnything = true;
        } else if (EndsWithIcase(name, ".txt") || EndsWithIcase(name, ".md")) {
            std::string txt(bytes->begin(), bytes->end());
            if (!TrimStr(txt).empty()) {
                PageModel pm;
                pm.pageId = outDoc.AllocatePageId();
                AnnotationObject tb;
                tb.id = outDoc.AllocateObjectId();
                tb.zOrder = outDoc.AllocateZOrder();
                tb.kind = ObjectKind::TextBox;
                tb.bounds = { 48.0, 48.0, 495.0, 720.0 };
                tb.textUtf8 = txt;
                tb.captionUtf8 = name;
                pm.objects.push_back(std::move(tb));
                outDoc.MutablePages().push_back(std::move(pm));
                importedAnything = true;
            }
        }
    }

    if (!importedAnything) {
        if (outErr) {
            *outErr = "El archivo ZIP no contiene payload .cudoc ni PDF/im\xC3\xA1genes/texto est\xC3\xA1ndar extra\xC3\xAD""bles (los datos binarios propietarios cerrados de terceros deben exportarse primero a PDF desde la app origen).";
        }
        return false;
    }

    if (outDetectedFormatUtf8) {
        *outDetectedFormatUtf8 = looksLikeGoodnotes ? "goodnotes_zip_extracted" : "third_party_zip_extracted";
    }
    outDoc.RebuildLayout();
    return true;
}

// ---------------- WebClipperEngine ----------------

bool WebClipperEngine::FetchUrlHtmlWinHttp(
    const std::string& urlUtf8,
    std::string& outHtmlUtf8,
    std::string* outErr,
    int timeoutMs)
{
#ifdef _WIN32
    return WinHttpRequest(urlUtf8, L"GET", nullptr, "", timeoutMs, outHtmlUtf8, outErr);
#else
    (void)urlUtf8; (void)outHtmlUtf8; (void)timeoutMs;
    if (outErr) *outErr = "WinHTTP solo disponible en Windows.";
    return false;
#endif
}

const WebClipperTemplate* WebClipperEngine::MatchTemplateForPage(
    const std::string& urlUtf8,
    const std::string& rawHtmlUtf8,
    const std::vector<WebClipperTemplate>& templates)
{
    for (const auto& t : templates) {
        const bool domainOk = t.domainPatternUtf8.empty() || (urlUtf8.find(t.domainPatternUtf8) != std::string::npos);
        const bool schemaOk = t.requiredSchemaTypeUtf8.empty() || (rawHtmlUtf8.find(t.requiredSchemaTypeUtf8) != std::string::npos);
        if (domainOk && schemaOk) return &t;
    }
    return templates.empty() ? nullptr : &templates.front();
}

WebClipCaptureResult WebClipperEngine::ClipHtmlPage(
    const std::string& urlUtf8,
    const std::string& rawHtmlUtf8,
    const std::string& userSelectionUtf8,
    const WebClipperTemplate& tpl,
    const std::string& captureDateIsoUtf8)
{
    WebClipCaptureResult res;
    if (rawHtmlUtf8.empty() && userSelectionUtf8.empty()) return res;

    res.ok = true;
    res.matchedTemplateNameUtf8 = tpl.nameUtf8;
    res.extractedTitleUtf8 = StripHtmlTags(ExtractBetweenIcase(rawHtmlUtf8, "<title>", "</title>"));
    if (res.extractedTitleUtf8.empty()) {
        res.extractedTitleUtf8 = StripHtmlTags(ExtractBetweenIcase(rawHtmlUtf8, "<h1>", "</h1>"));
    }
    if (res.extractedTitleUtf8.empty()) res.extractedTitleUtf8 = "Captura Web";

    res.extractedAuthorUtf8 = "Autor Web";
    const auto authPos = rawHtmlUtf8.find("name=\"author\" content=\"");
    if (authPos != std::string::npos) {
        const auto s = authPos + 23;
        const auto e = rawHtmlUtf8.find('"', s);
        if (e != std::string::npos) res.extractedAuthorUtf8 = rawHtmlUtf8.substr(s, e - s);
    }

    const auto typePos = rawHtmlUtf8.find("\"@type\"");
    if (typePos != std::string::npos) {
        const auto col = rawHtmlUtf8.find(':', typePos);
        const auto q1 = (col != std::string::npos) ? rawHtmlUtf8.find('"', col) : std::string::npos;
        const auto q2 = (q1 != std::string::npos) ? rawHtmlUtf8.find('"', q1 + 1) : std::string::npos;
        if (q1 != std::string::npos && q2 != std::string::npos) {
            res.extractedSchemaTypeUtf8 = rawHtmlUtf8.substr(q1 + 1, q2 - q1 - 1);
        }
    }

    std::size_t mPos = 0;
    while ((mPos = rawHtmlUtf8.find("<mark>", mPos)) != std::string::npos) {
        const auto mEnd = rawHtmlUtf8.find("</mark>", mPos + 6);
        if (mEnd == std::string::npos) break;
        res.highlightsUtf8.push_back(StripHtmlTags(rawHtmlUtf8.substr(mPos + 6, mEnd - (mPos + 6))));
        mPos = mEnd + 7;
    }

    std::string bodyContent = ExtractBetweenIcase(rawHtmlUtf8, "<article>", "</article>");
    if (bodyContent.empty()) bodyContent = ExtractBetweenIcase(rawHtmlUtf8, "<main>", "</main>");
    if (bodyContent.empty()) bodyContent = ExtractBetweenIcase(rawHtmlUtf8, "<body>", "</body>");
    if (bodyContent.empty()) bodyContent = rawHtmlUtf8;
    const std::string cleanContent = StripHtmlTags(bodyContent);

    std::unordered_map<std::string, std::string> vars = {
        {"title", res.extractedTitleUtf8},
        {"url", urlUtf8},
        {"author", res.extractedAuthorUtf8},
        {"date", captureDateIsoUtf8},
        {"content", cleanContent},
        {"selection", userSelectionUtf8},
        {"schema:@type", res.extractedSchemaTypeUtf8}
    };

    auto applyFilter = [](std::string val, const std::string& filterName) -> std::string {
        const std::string f = TrimStr(filterName);
        if (f == "upper") {
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        } else if (f == "lower") {
            std::transform(val.begin(), val.end(), val.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        } else if (f == "strip_tags") {
            val = StripHtmlTags(val);
        } else if (f == "blockquote") {
            val = "> " + val;
        }
        return val;
    };

    std::string out = tpl.markdownTemplateUtf8.empty()
        ? "# {{title}}\nFuente: {{url}}\nAutor: {{author}}\nFecha: {{date}}\n\n{{content}}"
        : tpl.markdownTemplateUtf8;

    std::size_t pos = 0;
    while ((pos = out.find("{{", pos)) != std::string::npos) {
        const auto close = out.find("}}", pos + 2);
        if (close == std::string::npos) break;
        const std::string expr = TrimStr(out.substr(pos + 2, close - (pos + 2)));
        std::string varKey = expr;
        std::string filterStr;
        const auto pipe = expr.find('|');
        if (pipe != std::string::npos) {
            varKey = TrimStr(expr.substr(0, pipe));
            filterStr = TrimStr(expr.substr(pipe + 1));
        }
        std::string val = vars.count(varKey) ? vars[varKey] : "";
        if (!filterStr.empty()) val = applyFilter(std::move(val), filterStr);
        out.replace(pos, close + 2 - pos, val);
        pos += val.size();
    }

    res.markdownOutputUtf8 = std::move(out);
    return res;
}

// ---------------- AutomationRuleEngine ----------------

void AutomationRuleEngine::RegisterRule(AutomationRule rule) {
    for (auto& ex : rules_) {
        if (!rule.ruleIdUtf8.empty() && ex.ruleIdUtf8 == rule.ruleIdUtf8) {
            ex = std::move(rule);
            return;
        }
    }
    rules_.push_back(std::move(rule));
}

std::size_t AutomationRuleEngine::TriggerEvent(
    AutomationTriggerKind trigger,
    StructuredDatabase& db,
    const std::string& recordIdUtf8,
    const std::string& changedColumnIdUtf8,
    const std::string& newValueUtf8) const
{
    std::size_t actionsExecuted = 0;
    for (const auto& r : rules_) {
        if (r.trigger != trigger) continue;
        if (trigger == AutomationTriggerKind::OnPropertyChanged) {
            if (!r.watchColumnIdUtf8.empty() && r.watchColumnIdUtf8 != changedColumnIdUtf8) continue;
            if (!r.watchValueUtf8.empty() && r.watchValueUtf8 != newValueUtf8) continue;
        }
        for (const auto& step : r.steps) {
            if (step.actionKindUtf8 == "set_property") {
                if (db.SetCellText(recordIdUtf8, step.targetKeyUtf8, step.valueUtf8)) {
                    ++actionsExecuted;
                }
            } else if (step.actionKindUtf8 == "append_text") {
                if (const auto* rec = db.FindRecord(recordIdUtf8)) {
                    const std::string cur = rec->cells.count(step.targetKeyUtf8) ? rec->cells.at(step.targetKeyUtf8).textUtf8 : "";
                    if (db.SetCellText(recordIdUtf8, step.targetKeyUtf8, cur + step.valueUtf8)) {
                        ++actionsExecuted;
                    }
                }
            } else if (step.actionKindUtf8 == "add_tag") {
                if (const auto* rec = db.FindRecord(recordIdUtf8)) {
                    std::vector<std::string> list = rec->cells.count(step.targetKeyUtf8) ? rec->cells.at(step.targetKeyUtf8).listUtf8 : std::vector<std::string>{};
                    if (std::find(list.begin(), list.end(), step.valueUtf8) == list.end()) {
                        list.push_back(step.valueUtf8);
                    }
                    if (db.SetCellList(recordIdUtf8, step.targetKeyUtf8, std::move(list))) {
                        ++actionsExecuted;
                    }
                }
            }
        }
    }
    return actionsExecuted;
}

std::string AutomationRuleEngine::SerializeToJson() const {
    json arr = json::array();
    for (const auto& r : rules_) {
        json steps = json::array();
        for (const auto& s : r.steps) {
            steps.push_back({
                {"actionKind", s.actionKindUtf8},
                {"targetKey", s.targetKeyUtf8},
                {"value", s.valueUtf8}
            });
        }
        arr.push_back({
            {"ruleId", r.ruleIdUtf8},
            {"name", r.nameUtf8},
            {"trigger", static_cast<int>(r.trigger)},
            {"watchColumnId", r.watchColumnIdUtf8},
            {"watchValue", r.watchValueUtf8},
            {"steps", std::move(steps)}
        });
    }
    return json({ {"rules", std::move(arr)} }).dump(2);
}

bool AutomationRuleEngine::DeserializeFromJson(const std::string& jsonUtf8) {
    try {
        const json root = json::parse(jsonUtf8);
        if (!root.is_object() || !root.contains("rules") || !root["rules"].is_array()) return false;
        rules_.clear();
        for (const auto& rj : root["rules"]) {
            AutomationRule r;
            r.ruleIdUtf8 = rj.value("ruleId", "");
            r.nameUtf8 = rj.value("name", "");
            r.trigger = static_cast<AutomationTriggerKind>(std::clamp(rj.value("trigger", 2), 0, 2));
            r.watchColumnIdUtf8 = rj.value("watchColumnId", "");
            r.watchValueUtf8 = rj.value("watchValue", "");
            if (rj.contains("steps") && rj["steps"].is_array()) {
                for (const auto& sj : rj["steps"]) {
                    r.steps.push_back({
                        sj.value("actionKind", "set_property"),
                        sj.value("targetKey", ""),
                        sj.value("value", "")
                    });
                }
            }
            rules_.push_back(std::move(r));
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ---------------- GroundedStudyAiAssistant (BM25 + Live Local Ollama/OpenAI via WinHTTP) ----------------

bool GroundedStudyAiAssistant::QueryLocalLlmViaWinHttp(
    const LocalLlmConfig& cfg,
    const std::string& systemPromptUtf8,
    const std::string& userPromptUtf8,
    std::string& outReplyUtf8,
    std::string* outErr)
{
    outReplyUtf8.clear();
    if (!cfg.enabled || cfg.endpointUrlUtf8.empty() || cfg.modelNameUtf8.empty()) {
        if (outErr) *outErr = "Endpoint LLM local no configurado o deshabilitado.";
        return false;
    }
#ifdef _WIN32
    const bool isOllama = (cfg.endpointUrlUtf8.find("/api/chat") != std::string::npos);
    json payload = {
        {"model", cfg.modelNameUtf8},
        {"stream", false},
        {"messages", {
            {{"role", "system"}, {"content", systemPromptUtf8}},
            {{"role", "user"}, {"content", userPromptUtf8}}
        }}
    };
    const std::string bodyStr = payload.dump();
    std::string respRaw;
    if (!WinHttpRequest(cfg.endpointUrlUtf8, L"POST", &bodyStr, cfg.apiKeyUtf8, cfg.timeoutMs, respRaw, outErr)) {
        return false;
    }
    try {
        const json rj = json::parse(respRaw);
        if (isOllama && rj.contains("message") && rj["message"].is_object()) {
            outReplyUtf8 = rj["message"].value("content", "");
        } else if (rj.contains("choices") && rj["choices"].is_array() && !rj["choices"].empty()) {
            outReplyUtf8 = rj["choices"][0]["message"].value("content", "");
        }
        return !outReplyUtf8.empty();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = std::string("JSON LLM inv\xC3\xA1lido: ") + ex.what();
        return false;
    }
#else
    (void)systemPromptUtf8; (void)userPromptUtf8;
    if (outErr) *outErr = "WinHTTP solo disponible en Windows.";
    return false;
#endif
}

GroundedAiAnswer GroundedStudyAiAssistant::AnswerQuestionWithCitations(
    const DocumentModel& doc,
    const std::vector<SpeakerTranscriptSegment>& audioSegments,
    const std::string& questionUtf8,
    const LocalLlmConfig* optionalLlm,
    const AiProviderVault* optionalVault)
{
    GroundedAiAnswer ans;
    const auto qTokens = TokenizeBm25(questionUtf8);
    if (qTokens.empty()) return ans;

    struct Passage {
        GroundedCitation cite;
        std::string fullText;
        std::vector<std::string> tokens;
        double score = 0.0;
    };
    std::vector<Passage> corpus;

    auto addPassage = [&](std::size_t pgIdx, std::uint64_t objId, std::uint64_t audioMs,
                          const std::string& kind, const std::string& txt) {
        if (TrimStr(txt).empty()) return;
        Passage p;
        p.cite = { doc.Title(), pgIdx, objId, audioMs, kind, txt.substr(0, 160) };
        p.fullText = txt;
        p.tokens = TokenizeBm25(txt);
        if (!p.tokens.empty()) corpus.push_back(std::move(p));
    };

    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        const auto& pg = doc.Pages()[p];
        if (!pg.extractedTextUtf8.empty()) addPassage(p, 0, 0, "page_text", pg.extractedTextUtf8);
        if (!pg.ocrTextUtf8.empty()) addPassage(p, 0, 0, "ocr_text", pg.ocrTextUtf8);
        for (const auto& o : pg.objects) {
            const std::string txt = !o.textUtf8.empty() ? o.textUtf8
                : (!o.htrRecognizedTextUtf8.empty() ? o.htrRecognizedTextUtf8
                : (!o.printoutTextUtf8.empty() ? o.printoutTextUtf8
                : (!o.formulaExprUtf8.empty() ? (o.formulaExprUtf8 + " = " + o.evaluatedResultUtf8) : "")));
            const std::string kind = !o.htrRecognizedTextUtf8.empty() ? "handwriting"
                : (!o.formulaExprUtf8.empty() ? "math" : "textbox");
            addPassage(p, o.id, o.inkStartTimeMs, kind, txt);
        }
    }
    for (const auto& o : doc.CanvasObjects()) {
        const std::string txt = !o.textUtf8.empty() ? o.textUtf8
            : (!o.htrRecognizedTextUtf8.empty() ? o.htrRecognizedTextUtf8
            : (!o.printoutTextUtf8.empty() ? o.printoutTextUtf8
            : (!o.formulaExprUtf8.empty() ? (o.formulaExprUtf8 + " = " + o.evaluatedResultUtf8) : "")));
        addPassage(0, o.id, o.inkStartTimeMs, !o.formulaExprUtf8.empty() ? "math" : "canvas_object", txt);
    }
    for (const auto& seg : audioSegments) {
        addPassage(0, 0, seg.startMs, "audio_transcript",
            "[" + seg.speakerIdUtf8 + " @ " + std::to_string(seg.startMs) + "ms] " + seg.textUtf8);
    }
    if (corpus.empty()) return ans;

    // BM25 scoring across all retrieved passages
    double avgLen = 0.0;
    for (const auto& d : corpus) avgLen += static_cast<double>(d.tokens.size());
    avgLen /= static_cast<double>(corpus.size());
    if (avgLen < 1.0) avgLen = 1.0;

    std::unordered_map<std::string, std::size_t> df;
    for (const auto& d : corpus) {
        std::set<std::string> uniq(d.tokens.begin(), d.tokens.end());
        for (const auto& t : uniq) ++df[t];
    }

    constexpr double k1 = 1.5;
    constexpr double b = 0.75;
    const double N = static_cast<double>(corpus.size());

    for (auto& d : corpus) {
        std::unordered_map<std::string, int> tf;
        for (const auto& t : d.tokens) ++tf[t];
        const double dl = static_cast<double>(d.tokens.size());
        double s = 0.0;
        for (const auto& qt : qTokens) {
            int freq = tf.count(qt) ? tf[qt] : 0;
            if (freq == 0) {
                // Prefix match for Spanish inflections (e.g. integral/integrales)
                for (const auto& [tok, cnt] : tf) {
                    if (qt.size() >= 4 && tok.rfind(qt, 0) == 0) { freq += cnt; break; }
                }
            }
            if (freq <= 0) continue;
            const double docFreq = static_cast<double>(df.count(qt) ? df[qt] : 1u);
            const double idf = std::log((N - docFreq + 0.5) / (docFreq + 0.5) + 1.0);
            s += idf * ((static_cast<double>(freq) * (k1 + 1.0)) /
                        (static_cast<double>(freq) + k1 * (1.0 - b + b * (dl / avgLen))));
        }
        d.score = s;
    }

    std::sort(corpus.begin(), corpus.end(), [](const Passage& x, const Passage& y) {
        return x.score > y.score;
    });

    std::ostringstream contextForLlm;
    for (const auto& p : corpus) {
        if (p.score <= 0.0) break;
        ans.citations.push_back(p.cite);
        contextForLlm << "[" << ans.citations.size() << "] (" << p.cite.sourceKindUtf8
                      << ", Pag. " << (p.cite.pageIndex0 + 1) << "): " << p.fullText << "\n";
        if (ans.citations.size() >= 6) break;
    }
    if (ans.citations.empty()) return ans;

    ans.answered = true;
    const std::string sys = "Eres un asistente de estudio preciso. Responde en espa\xC3\xB1ol con claridad y rigor solo con base en las fuentes proporcionadas citando [1], [2], etc.";
    const std::string usr = "Fuentes del cuaderno:\n" + contextForLlm.str() + "\nPregunta: " + questionUtf8;
    // 1. Try Multi-Provider DPAPI Vault with automatic failover
    if (optionalVault && optionalVault->HasConfiguredCloudOrLocalProvider()) {
        const AiQueryResult qr = optionalVault->QueryWithFailover(sys, usr);
        if (qr.ok && !qr.replyTextUtf8.empty()) {
            ans.usedLiveLlm = true;
            ans.providerUsedUtf8 = qr.providerNameUsedUtf8 + " (" + qr.modelUsedUtf8 + ")";
            ans.latencyMs = qr.latencyMs;
            ans.answerTextUtf8 = qr.replyTextUtf8 + "\n\n[" + ans.providerUsedUtf8 + " \xC2\xB7 " + std::to_string(qr.latencyMs) + " ms]\nFuentes citadas:\n" + contextForLlm.str();
            return ans;
        }
    }
    // 2. Fallback to legacy single LocalLlmConfig if enabled
    if (optionalLlm && optionalLlm->enabled) {
        std::string llmReply;
        if (QueryLocalLlmViaWinHttp(*optionalLlm, sys, usr, llmReply) && !llmReply.empty()) {
            ans.usedLiveLlm = true;
            ans.providerUsedUtf8 = optionalLlm->modelNameUtf8;
            ans.answerTextUtf8 = llmReply + "\n\nFuentes citadas:\n" + contextForLlm.str();
            return ans;
        }
    }

    std::ostringstream oss;
    oss << "Respuesta fundamentada (BM25 local) en " << ans.citations.size() << " fuente(s) del cuaderno:\n";
    for (std::size_t i = 0; i < ans.citations.size(); ++i) {
        const auto& c = ans.citations[i];
        oss << "[" << (i + 1) << "] (" << c.sourceKindUtf8 << ", Pag. " << (c.pageIndex0 + 1);
        if (c.objectId != 0) oss << ", Obj #" << c.objectId;
        if (c.audioTimestampMs != 0) oss << ", Audio " << c.audioTimestampMs << "ms";
        oss << "): " << c.quoteSnippetUtf8 << "\n";
    }
    ans.answerTextUtf8 = oss.str();
    return ans;
}

std::string GroundedStudyAiAssistant::GenerateExecutiveSummary(
    const DocumentModel& doc,
    const std::vector<SpeakerTranscriptSegment>& audioSegments,
    const LocalLlmConfig* optionalLlm,
    const AiProviderVault* optionalVault)
{
    std::ostringstream rawContext;
    std::size_t count = 0;
    for (std::size_t p = 0; p < doc.Pages().size() && count < 14; ++p) {
        for (const auto& o : doc.Pages()[p].objects) {
            if (!o.textUtf8.empty()) {
                rawContext << "- [Pag. " << (p + 1) << "] " << o.textUtf8 << "\n";
                ++count;
            } else if (!o.formulaExprUtf8.empty()) {
                rawContext << "- [Formula Pag. " << (p + 1) << "] " << o.formulaExprUtf8 << " = " << o.evaluatedResultUtf8 << "\n";
                ++count;
            }
        }
    }
    for (const auto& s : audioSegments) {
        rawContext << "- [Audio " << s.startMs << "ms - " << s.speakerIdUtf8 << "] " << s.textUtf8 << "\n";
    }

    const std::string sys = "Genera un resumen ejecutivo estructurado en Markdown en espa\xC3\xB1ol usando solo los apuntes entregados.";
    const std::string usr = "Titulo: " + doc.Title() + "\nApuntes:\n" + rawContext.str();
    if (optionalVault && optionalVault->HasConfiguredCloudOrLocalProvider() && !rawContext.str().empty()) {
        const AiQueryResult qr = optionalVault->QueryWithFailover(sys, usr);
        if (qr.ok && !qr.replyTextUtf8.empty()) {
            return qr.replyTextUtf8;
        }
    }
    if (optionalLlm && optionalLlm->enabled && !rawContext.str().empty()) {
        std::string llmReply;
        if (QueryLocalLlmViaWinHttp(*optionalLlm, sys, usr, llmReply) && !llmReply.empty()) {
            return llmReply;
        }
    }

    std::ostringstream oss;
    oss << "# Resumen Ejecutivo: " << doc.Title() << "\n";
    oss << "- Paginas analizadas: " << doc.Pages().size() << "\n";
    oss << "- Segmentos de audio transcritos: " << audioSegments.size() << "\n\n## Puntos Clave\n"
        << rawContext.str();
    return oss.str();
}

std::vector<QuizQuestionItem> GroundedStudyAiAssistant::GenerateMultipleChoiceQuiz(
    const DocumentModel& doc,
    std::size_t maxQuestions)
{
    std::vector<QuizQuestionItem> quiz;
    StudyDeck deck = StudySetManager::BuildDeckFromDocument(doc);
    // Supplement deck with factual prose notes when document contains plain TextBox paragraphs
    if (deck.cards.size() < maxQuestions) {
        for (std::size_t p = 0; p < doc.Pages().size() && deck.cards.size() < maxQuestions * 2; ++p) {
            for (const auto& o : doc.Pages()[p].objects) {
                if (o.kind != ObjectKind::TextBox || o.textUtf8.size() < 24) continue;
                const std::string& txt = o.textUtf8;
                std::string topic;
                std::string ans = txt;
                const auto colon = txt.find(':');
                if (colon != std::string::npos && colon >= 3 && colon <= 48) {
                    topic = TrimStr(txt.substr(0, colon));
                    ans = TrimStr(txt.substr(colon + 1));
                } else {
                    // Use first 4 words as concept prompt
                    std::istringstream iss(txt);
                    std::string w;
                    int wc = 0;
                    while (iss >> w && wc < 5) {
                        if (!topic.empty()) topic += ' ';
                        topic += w;
                        ++wc;
                    }
                }
                if (topic.empty() || ans.empty()) continue;
                decltype(deck.cards)::value_type sc{};
                sc.id = o.id;
                sc.sourcePageIndex = p;
                sc.questionUtf8 = "\xC2\xBFQu\xC3\xA9 establece o describe \"" + topic + "\" en los apuntes?";
                sc.answerUtf8 = (ans.size() > 120) ? (ans.substr(0, 117) + "...") : ans;
                deck.cards.push_back(std::move(sc));
            }
        }
    }
    if (deck.cards.empty()) return quiz;

    // Collect pool of real answers from the document itself to build realistic non-hardcoded distractors
    std::vector<std::string> answerPool;
    for (const auto& c : deck.cards) {
        if (!c.answerUtf8.empty() &&
            std::find(answerPool.begin(), answerPool.end(), c.answerUtf8) == answerPool.end()) {
            answerPool.push_back(c.answerUtf8);
        }
    }
    // Supplement pool with distinct expressions/results from document pages if deck has < 4 cards
    for (const auto& pg : doc.Pages()) {
        for (const auto& o : pg.objects) {
            const std::string cand = !o.evaluatedResultUtf8.empty() ? o.evaluatedResultUtf8
                : (!o.textUtf8.empty() && o.textUtf8.size() <= 72 ? o.textUtf8 : "");
            if (!cand.empty() && std::find(answerPool.begin(), answerPool.end(), cand) == answerPool.end()) {
                answerPool.push_back(cand);
            }
        }
    }

    for (std::size_t i = 0; i < deck.cards.size() && quiz.size() < maxQuestions; ++i) {
        const auto& c = deck.cards[i];
        QuizQuestionItem q;
        q.questionUtf8 = c.questionUtf8;
        q.sourceCitation = { doc.Title(), c.sourcePageIndex, c.id, 0, "study_card", c.questionUtf8 };

        std::vector<std::string> distractors;
        for (const auto& poolItem : answerPool) {
            if (poolItem != c.answerUtf8 && distractors.size() < 3) {
                distractors.push_back(poolItem);
            }
        }
        // If numeric answer still needs distractors, perturb numerically instead of hardcoded placeholder text
        if (distractors.size() < 3) {
            try {
                const double v = std::stod(c.answerUtf8);
                const double offsets[3] = { (std::abs(v) > 1e-6 ? v * 0.5 : 1.5), (std::abs(v) > 1e-6 ? v * 2.0 : -2.0), v + 1.0 };
                for (double ov : offsets) {
                    if (distractors.size() >= 3) break;
                    char buf[32]{};
                    std::snprintf(buf, sizeof(buf), "%.4g", ov);
                    std::string s = buf;
                    if (s != c.answerUtf8 && std::find(distractors.begin(), distractors.end(), s) == distractors.end()) {
                        distractors.push_back(s);
                    }
                }
            } catch (...) {}
        }
        while (distractors.size() < 3) {
            distractors.push_back("Concepto diferente al enunciado (" + std::to_string(distractors.size() + 1) + ")");
        }

        // Deterministic varied placement of correct option (not always index 0!)
        const int correctPos = static_cast<int>((i * 3u + c.questionUtf8.size()) % 4u);
        q.optionsUtf8.resize(4);
        q.optionsUtf8[static_cast<std::size_t>(correctPos)] = c.answerUtf8;
        std::size_t dIdx = 0;
        for (int slot = 0; slot < 4; ++slot) {
            if (slot != correctPos) q.optionsUtf8[static_cast<std::size_t>(slot)] = distractors[dIdx++];
        }
        q.correctOptionIndex = correctPos;
        quiz.push_back(std::move(q));
    }
    return quiz;
}

QuizGradeReport GroundedStudyAiAssistant::GradeQuizResponses(
    const std::vector<QuizQuestionItem>& quiz,
    const std::vector<int>& selectedOptionIndices)
{
    QuizGradeReport rep;
    rep.totalQuestions = quiz.size();
    if (quiz.empty()) return rep;
    for (std::size_t i = 0; i < quiz.size() && i < selectedOptionIndices.size(); ++i) {
        if (selectedOptionIndices[i] == quiz[i].correctOptionIndex) {
            ++rep.correctCount;
        }
    }
    rep.scorePercent = (static_cast<double>(rep.correctCount) * 100.0) / static_cast<double>(rep.totalQuestions);
    return rep;
}

std::string GroundedStudyAiAssistant::TranslateSelectionOffline(
    const std::string& textUtf8,
    const std::string& targetLangCodeUtf8,
    const LocalLlmConfig* optionalLlm,
    const AiProviderVault* optionalVault)
{
    const std::string sys = "Traduce con precisi\xC3\xB3n acad\xC3\xA9mica al idioma '" + targetLangCodeUtf8 + "'. Devuelve solo la traducci\xC3\xB3n.";
    if (optionalVault && optionalVault->HasConfiguredCloudOrLocalProvider()) {
        const AiQueryResult qr = optionalVault->QueryWithFailover(sys, textUtf8);
        if (qr.ok && !qr.replyTextUtf8.empty()) {
            return qr.replyTextUtf8;
        }
    }
    if (optionalLlm && optionalLlm->enabled) {
        std::string llmReply;
        if (QueryLocalLlmViaWinHttp(*optionalLlm, sys, textUtf8, llmReply) && !llmReply.empty()) {
            return llmReply;
        }
    }

    // Expanded offline STEM phrase & term glossary (ES <-> EN)
    static const std::vector<std::pair<std::string, std::string>> kEsToEn = {
        {"teorema fundamental del calculo", "fundamental theorem of calculus"},
        {"ecuacion diferencial", "differential equation"},
        {"derivada parcial", "partial derivative"},
        {"transformada de laplace", "laplace transform"},
        {"transformada de fourier", "fourier transform"},
        {"convergencia uniforme", "uniform convergence"},
        {"algebra lineal", "linear algebra"},
        {"calculo integral", "integral calculus"},
        {"espacio vectorial", "vector space"},
        {"valor propio", "eigenvalue"},
        {"ley de ohm", "ohm's law"},
        {"ley de kirchhoff", "kirchhoff's law"},
        {"corriente alterna", "alternating current"},
        {"corriente continua", "direct current"},
        {"masa molar", "molar mass"},
        {"reaccion quimica", "chemical reaction"},
        {"energia cinetica", "kinetic energy"},
        {"energia potencial", "potential energy"},
        {"campo magnetico", "magnetic field"},
        {"campo electrico", "electric field"}
    };

    const std::string low = FoldUtf8ForSearch(textUtf8, false, true);
    if (targetLangCodeUtf8 == "en") {
        for (const auto& [es, en] : kEsToEn) {
            if (low.find(es) != std::string::npos) return "[EN] " + en;
        }
    } else if (targetLangCodeUtf8 == "es") {
        for (const auto& [es, en] : kEsToEn) {
            if (low.find(en) != std::string::npos) return "[ES] " + es;
        }
    }
    return "[" + targetLangCodeUtf8 + " - configure LLM local en Ajustes IA para traducci\xC3\xB3n libre] " + textUtf8;
}

StudyProgressMetrics GroundedStudyAiAssistant::ComputeStudyProgress(
    std::uint64_t accumulatedMinutes,
    const StudyDeck& deck)
{
    StudyProgressMetrics m;
    m.totalStudyMinutes = accumulatedMinutes;
    m.totalCards = deck.cards.size();
    for (const auto& c : deck.cards) {
        if (c.repetitions >= 2 && c.intervalDays >= 6) ++m.masteredCards;
    }
    m.completionRatio = (m.totalCards > 0)
        ? (static_cast<double>(m.masteredCards) / static_cast<double>(m.totalCards))
        : 0.0;
    return m;
}

} // namespace DocStudio
