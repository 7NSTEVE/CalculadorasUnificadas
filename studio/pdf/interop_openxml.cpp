#include "interop_openxml.hpp"
#include "../core/zip_container.hpp"
#include "../math/engineering_cas.hpp"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include "../core/utf_conv.hpp"
#include <filesystem>
#include <fstream>
#endif

namespace DocStudio {

namespace {

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
            // Strip illegal XML 1.0 control chars except \t \n \r
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
        }
        out.push_back(in[i++]);
    }
    return out;
}

std::vector<std::string> SplitParagraphs(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!line.empty()) out.push_back(std::move(line));
    }
    return out;
}

std::string ExtractXmlTagTextParagraphs(const std::string& xml, const std::string& paraCloseTag, const std::string& textTagName) {
    // Extracts text inside <textTagName ...>...</textTagName>, inserting '\n' when paraCloseTag appears
    const std::string openPrefix = "<" + textTagName;
    const std::string closeTag = "</" + textTagName + ">";
    std::string result;
    std::size_t pos = 0;
    while (pos < xml.size()) {
        const std::size_t tPos = xml.find(openPrefix, pos);
        const std::size_t pEndPos = paraCloseTag.empty() ? std::string::npos : xml.find(paraCloseTag, pos);
        if (tPos == std::string::npos) break;
        if (pEndPos != std::string::npos && pEndPos < tPos) {
            if (!result.empty() && result.back() != '\n') result.push_back('\n');
            pos = pEndPos + paraCloseTag.size();
            continue;
        }
        const std::size_t afterName = tPos + openPrefix.size();
        if (afterName >= xml.size() || (xml[afterName] != '>' && xml[afterName] != ' ')) {
            pos = afterName;
            continue;
        }
        const std::size_t gtPos = xml.find('>', afterName);
        if (gtPos == std::string::npos) break;
        if (gtPos > 0 && xml[gtPos - 1] == '/') {
            pos = gtPos + 1;
            continue;
        }
        const std::size_t endPos = xml.find(closeTag, gtPos + 1);
        if (endPos == std::string::npos) break;
        result += XmlUnescape(xml.substr(gtPos + 1, endPos - (gtPos + 1)));
        pos = endPos + closeTag.size();
    }
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

bool WriteAllFile(const std::wstring& path, const std::vector<std::uint8_t>& bytes, std::string* outErr) {
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo crear archivo de exportaci\xC3\xB3n OpenXML.";
        return false;
    }
    std::size_t off = 0;
    while (off < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(bytes.size() - off, 4u * 1024u * 1024u));
        DWORD wr = 0;
        if (!WriteFile(hFile, bytes.data() + off, chunk, &wr, nullptr) || wr != chunk) {
            CloseHandle(hFile);
            if (outErr) *outErr = "Error escribiendo archivo OpenXML.";
            return false;
        }
        off += wr;
    }
    FlushFileBuffers(hFile);
    CloseHandle(hFile);
    return true;
#else
    const std::filesystem::path p(WideToUtf8(path));
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (outErr) *outErr = "No se pudo crear archivo de exportaci\xC3\xB3n OpenXML.";
        return false;
    }
    if (!bytes.empty()) {
        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    ofs.flush();
    if (!ofs.good()) {
        if (outErr) *outErr = "Error escribiendo archivo OpenXML.";
        return false;
    }
    return true;
#endif
}

bool ReadAllFile(const std::wstring& path, std::vector<std::uint8_t>& out, std::string* outErr) {
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo abrir archivo OpenXML.";
        return false;
    }
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(hFile, &li) || li.QuadPart < 0 || li.QuadPart > 256ll * 1024ll * 1024ll) {
        CloseHandle(hFile);
        return false;
    }
    out.resize(static_cast<std::size_t>(li.QuadPart));
    std::size_t off = 0;
    while (off < out.size()) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(out.size() - off, 4u * 1024u * 1024u));
        DWORD rd = 0;
        if (!ReadFile(hFile, out.data() + off, chunk, &rd, nullptr) || rd == 0) {
            CloseHandle(hFile);
            return false;
        }
        off += rd;
    }
    CloseHandle(hFile);
    return true;
#else
    const std::filesystem::path p(WideToUtf8(path));
    std::ifstream ifs(p, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "No se pudo abrir archivo OpenXML.";
        return false;
    }
    const std::streampos sz = ifs.tellg();
    if (sz < 0 || static_cast<std::uint64_t>(sz) > 256ull * 1024ull * 1024ull) {
        return false;
    }
    out.resize(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!out.empty() && !ifs.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()))) {
        return false;
    }
    return true;
#endif
}

std::string MakeDocxDrawingXml(const std::string& relId, std::uint64_t picId, double wPt, double hPt) {
    const std::uint64_t cx = static_cast<std::uint64_t>((std::max)(72.0, (std::min)(468.0, wPt)) * 12700.0);
    const std::uint64_t cy = static_cast<std::uint64_t>((std::max)(48.0, (std::min)(640.0, hPt)) * 12700.0);
    std::ostringstream s;
    s << "<w:p><w:r><w:drawing>"
      << "<wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
      << "<wp:extent cx=\"" << cx << "\" cy=\"" << cy << "\"/>"
      << "<wp:docPr id=\"" << picId << "\" name=\"Imagen " << picId << "\"/>"
      << "<a:graphic xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\">"
      << "<a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
      << "<pic:pic xmlns:pic=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
      << "<pic:nvPicPr><pic:cNvPr id=\"" << picId << "\" name=\"img" << picId << ".png\"/><pic:cNvPicPr/></pic:nvPicPr>"
      << "<pic:blipFill><a:blip r:embed=\"" << relId << "\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
      << "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"" << cx << "\" cy=\"" << cy << "\"/></a:xfrm>"
      << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr>"
      << "</pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r></w:p>\n";
    return s.str();
}

std::string ExtractXmlAttribute(const std::string& tag, const std::string& attrName) {
    const std::string key = attrName + "=\"";
    const std::size_t p = tag.find(key);
    if (p == std::string::npos) return {};
    const std::size_t s = p + key.size();
    const std::size_t e = tag.find('"', s);
    return (e == std::string::npos) ? std::string{} : XmlUnescape(tag.substr(s, e - s));
}

std::vector<std::string> ExtractTagBlocks(const std::string& xml, const std::string& tagName) {
    std::vector<std::string> out;
    const std::string open = "<" + tagName;
    const std::string close = "</" + tagName + ">";
    std::size_t pos = 0;
    while (pos < xml.size()) {
        const std::size_t start = xml.find(open, pos);
        if (start == std::string::npos) break;
        const std::size_t after = start + open.size();
        if (after >= xml.size() || (xml[after] != '>' && xml[after] != ' ' && xml[after] != '/')) {
            pos = after;
            continue;
        }
        const std::size_t gt = xml.find('>', after);
        if (gt == std::string::npos) break;
        if (xml[gt - 1] == '/') {
            pos = gt + 1;
            continue;
        }
        // Handle nested tags of same name
        int depth = 1;
        std::size_t scan = gt + 1;
        while (scan < xml.size() && depth > 0) {
            const std::size_t nextOpen = xml.find(open, scan);
            const std::size_t nextClose = xml.find(close, scan);
            if (nextClose == std::string::npos) break;
            if (nextOpen != std::string::npos && nextOpen < nextClose) {
                const std::size_t na = nextOpen + open.size();
                if (na < xml.size() && (xml[na] == '>' || xml[na] == ' ')) {
                    const std::size_t ngt = xml.find('>', na);
                    if (ngt != std::string::npos && xml[ngt - 1] != '/') ++depth;
                    scan = (ngt == std::string::npos) ? nextOpen + 1 : ngt + 1;
                } else {
                    scan = na;
                }
            } else {
                --depth;
                if (depth == 0) {
                    out.push_back(xml.substr(start, (nextClose + close.size()) - start));
                    pos = nextClose + close.size();
                    break;
                }
                scan = nextClose + close.size();
            }
        }
        if (depth > 0) break;
    }
    return out;
}

// Converts Office Math Markup Language (<m:oMath>) XML to natural/LaTeX math expression
std::string OmmlToFormulaExpr(const std::string& ommlXml) {
    auto parseInner = [&](auto&& self, const std::string& frag) -> std::string {
        // Fractions <m:f><m:num>...</m:num><m:den>...</m:den></m:f>
        if (frag.find("<m:f>") != std::string::npos || frag.find("<m:f ") != std::string::npos) {
            std::string work = frag;
            const auto fracs = ExtractTagBlocks(work, "m:f");
            for (const auto& fBlk : fracs) {
                const auto nums = ExtractTagBlocks(fBlk, "m:num");
                const auto dens = ExtractTagBlocks(fBlk, "m:den");
                const std::string nStr = nums.empty() ? "" : self(self, nums[0]);
                const std::string dStr = dens.empty() ? "" : self(self, dens[0]);
                const std::string repl = "(" + nStr + ")/(" + dStr + ")";
                const std::size_t p = work.find(fBlk);
                if (p != std::string::npos) work.replace(p, fBlk.size(), "<m:t>" + repl + "</m:t>");
            }
            return self(self, work);
        }
        // Radicals <m:rad><m:e>...</m:e></m:rad>
        if (frag.find("<m:rad") != std::string::npos) {
            std::string work = frag;
            for (const auto& rBlk : ExtractTagBlocks(work, "m:rad")) {
                const auto es = ExtractTagBlocks(rBlk, "m:e");
                const std::string inner = es.empty() ? "" : self(self, es[0]);
                const std::size_t p = work.find(rBlk);
                if (p != std::string::npos) work.replace(p, rBlk.size(), "<m:t>sqrt(" + inner + ")</m:t>");
            }
            return self(self, work);
        }
        // Superscripts <m:sSup><m:e>base</m:e><m:sup>exp</m:sup></m:sSup>
        if (frag.find("<m:sSup") != std::string::npos) {
            std::string work = frag;
            for (const auto& sBlk : ExtractTagBlocks(work, "m:sSup")) {
                const auto es = ExtractTagBlocks(sBlk, "m:e");
                const auto sups = ExtractTagBlocks(sBlk, "m:sup");
                const std::string b = es.empty() ? "" : self(self, es[0]);
                const std::string e = sups.empty() ? "" : self(self, sups[0]);
                const std::size_t p = work.find(sBlk);
                if (p != std::string::npos) work.replace(p, sBlk.size(), "<m:t>" + b + "^(" + e + ")</m:t>");
            }
            return self(self, work);
        }
        // Simultaneous Subscript + Superscript <m:sSubSup><m:e>b</m:e><m:sub>s</m:sub><m:sup>p</m:sup></m:sSubSup>
        if (frag.find("<m:sSubSup") != std::string::npos) {
            std::string work = frag;
            for (const auto& sBlk : ExtractTagBlocks(work, "m:sSubSup")) {
                const auto es = ExtractTagBlocks(sBlk, "m:e");
                const auto subs = ExtractTagBlocks(sBlk, "m:sub");
                const auto sups = ExtractTagBlocks(sBlk, "m:sup");
                const std::string b = es.empty() ? "" : self(self, es[0]);
                const std::string sb = subs.empty() ? "" : self(self, subs[0]);
                const std::string sp = sups.empty() ? "" : self(self, sups[0]);
                const std::size_t p = work.find(sBlk);
                if (p != std::string::npos) work.replace(p, sBlk.size(), "<m:t>" + b + "_" + sb + "^(" + sp + ")</m:t>");
            }
            return self(self, work);
        }
        // N-ary operators <m:nary> (integrals, summations, products with optional limits)
        if (frag.find("<m:nary") != std::string::npos) {
            std::string work = frag;
            for (const auto& nBlk : ExtractTagBlocks(work, "m:nary")) {
                std::string op = "int";
                const std::string chr = ExtractXmlAttribute(nBlk, "m:val");
                if (chr.find("\xE2\x88\x91") != std::string::npos) op = "sum";
                else if (chr.find("\xE2\x88\x8F") != std::string::npos) op = "prod";
                const auto subs = ExtractTagBlocks(nBlk, "m:sub");
                const auto sups = ExtractTagBlocks(nBlk, "m:sup");
                const auto es = ExtractTagBlocks(nBlk, "m:e");
                const std::string lo = subs.empty() ? "" : self(self, subs[0]);
                const std::string hi = sups.empty() ? "" : self(self, sups[0]);
                const std::string body = es.empty() ? "" : self(self, es[0]);
                std::string repl = op + "(" + body;
                if (!lo.empty() || !hi.empty()) repl += ", " + lo + ", " + hi;
                repl += ")";
                const std::size_t p = work.find(nBlk);
                if (p != std::string::npos) work.replace(p, nBlk.size(), "<m:t>" + repl + "</m:t>");
            }
            return self(self, work);
        }
        // Matrices <m:m><m:mr><m:e>...</m:e></m:mr></m:m> -> [[a,b],[c,d]]
        if (frag.find("<m:m>") != std::string::npos || frag.find("<m:m ") != std::string::npos) {
            std::string work = frag;
            for (const auto& mBlk : ExtractTagBlocks(work, "m:m")) {
                std::string mat = "[";
                bool firstRow = true;
                for (const auto& rBlk : ExtractTagBlocks(mBlk, "m:mr")) {
                    if (!firstRow) mat += ",";
                    firstRow = false;
                    mat += "[";
                    bool firstCell = true;
                    for (const auto& cBlk : ExtractTagBlocks(rBlk, "m:e")) {
                        if (!firstCell) mat += ",";
                        firstCell = false;
                        mat += self(self, cBlk);
                    }
                    mat += "]";
                }
                mat += "]";
                const std::size_t p = work.find(mBlk);
                if (p != std::string::npos) work.replace(p, mBlk.size(), "<m:t>" + mat + "</m:t>");
            }
            return self(self, work);
        }
        // Delimiters <m:d><m:e>...</m:e></m:d> -> (...)
        if (frag.find("<m:d>") != std::string::npos || frag.find("<m:d ") != std::string::npos) {
            std::string work = frag;
            for (const auto& dBlk : ExtractTagBlocks(work, "m:d")) {
                const auto es = ExtractTagBlocks(dBlk, "m:e");
                std::string inner;
                for (std::size_t i = 0; i < es.size(); ++i) {
                    if (i > 0) inner += ",";
                    inner += self(self, es[i]);
                }
                const std::size_t p = work.find(dBlk);
                if (p != std::string::npos) work.replace(p, dBlk.size(), "<m:t>(" + inner + ")</m:t>");
            }
            return self(self, work);
        }
        // Subscripts <m:sSub><m:e>base</m:e><m:sub>sub</m:sub></m:sSub>
        if (frag.find("<m:sSub") != std::string::npos) {
            std::string work = frag;
            for (const auto& sBlk : ExtractTagBlocks(work, "m:sSub")) {
                const auto es = ExtractTagBlocks(sBlk, "m:e");
                const auto subs = ExtractTagBlocks(sBlk, "m:sub");
                const std::string b = es.empty() ? "" : self(self, es[0]);
                const std::string sub = subs.empty() ? "" : self(self, subs[0]);
                const std::size_t p = work.find(sBlk);
                if (p != std::string::npos) work.replace(p, sBlk.size(), "<m:t>" + b + "_" + sub + "</m:t>");
            }
            return self(self, work);
        }
        return ExtractXmlTagTextParagraphs(frag, "", "m:t");
    };
    return parseInner(parseInner, ommlXml);
}

std::string FormulaExprToOmmlXml(const std::string& expr) {
    return "<m:oMathPara xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\"><m:oMath><m:r><m:t>" +
           XmlEscape(expr) + "</m:t></m:r></m:oMath></m:oMathPara>\n";
}

std::map<std::string, std::string> ParseRelationshipsXml(const std::string& relsXml) {
    std::map<std::string, std::string> rels;
    std::size_t pos = 0;
    while (pos < relsXml.size()) {
        const std::size_t p = relsXml.find("<Relationship ", pos);
        if (p == std::string::npos) break;
        const std::size_t gt = relsXml.find('>', p);
        if (gt == std::string::npos) break;
        const std::string tag = relsXml.substr(p, gt - p + 1);
        const std::string id = ExtractXmlAttribute(tag, "Id");
        const std::string target = ExtractXmlAttribute(tag, "Target");
        if (!id.empty() && !target.empty()) rels[id] = target;
        pos = gt + 1;
    }
    return rels;
}

std::string ParseTableXmlToJson(const std::string& tblXml, const std::string& rowTag, const std::string& cellTag, const std::string& textTag, std::string& outPlainPreview) {
    nlohmann::json rows = nlohmann::json::array();
    std::ostringstream preview;
    for (const auto& rBlk : ExtractTagBlocks(tblXml, rowTag)) {
        nlohmann::json row = nlohmann::json::array();
        bool first = true;
        for (const auto& cBlk : ExtractTagBlocks(rBlk, cellTag)) {
            const std::string cellTxt = ExtractXmlTagTextParagraphs(cBlk, "", textTag);
            row.push_back(cellTxt);
            if (!first) preview << " | ";
            preview << cellTxt;
            first = false;
        }
        if (!row.empty()) {
            rows.push_back(std::move(row));
            preview << "\n";
        }
    }
    outPlainPreview = preview.str();
    nlohmann::json root;
    root["rows"] = std::move(rows);
    return root.dump();
}

} // namespace

bool OpenXmlInterop::ExportToDocxBytes(
    const DocumentModel& doc,
    const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng,
    std::vector<std::uint8_t>& outDocxBytes,
    std::string* outErr)
{
    ZipArchive zip;
    zip.SetTextEntry("[Content_Types].xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Default Extension=\"png\" ContentType=\"image/png\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "</Types>");

    zip.SetTextEntry("_rels/.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>");

    std::ostringstream rels;
    rels << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
         << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n";

    std::ostringstream body;
    body << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
         << "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\" "
         << "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
         << "xmlns:wp=\"http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing\" "
         << "xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\">\n<w:body>\n";

    body << "<w:p><w:r><w:rPr><w:b/><w:sz w:val=\"32\"/></w:rPr><w:t xml:space=\"preserve\">"
         << XmlEscape(doc.Title()) << "</w:t></w:r></w:p>\n";

    std::uint64_t nextImgIdx = 0;
    auto addImage = [&](const std::vector<std::uint8_t>& png, double wPt, double hPt) {
        if (png.empty()) return;
        const std::uint64_t id = ++nextImgIdx;
        const std::string rid = "rId" + std::to_string(id);
        const std::string mediaPath = "media/image" + std::to_string(id) + ".png";
        zip.SetEntry("word/" + mediaPath, png);
        rels << "<Relationship Id=\"" << rid
             << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\""
             << mediaPath << "\"/>\n";
        body << MakeDocxDrawingXml(rid, id, wPt, hPt);
    };

    if (doc.Mode() == DocumentMode::InfiniteCanvas) {
        if (optionalPageSnapshotsPng.size() > 0 && !optionalPageSnapshotsPng[0].empty()) {
            addImage(optionalPageSnapshotsPng[0], 468.0, 320.0);
        }
        for (const auto& obj : doc.CanvasObjects()) {
            if (obj.kind == ObjectKind::TextBox && !obj.textUtf8.empty()) {
                for (const auto& para : SplitParagraphs(obj.textUtf8)) {
                    body << "<w:p><w:r><w:t xml:space=\"preserve\">" << XmlEscape(para) << "</w:t></w:r></w:p>\n";
                }
            } else if (obj.kind == ObjectKind::Image) {
                const auto* b = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
                if (b) addImage(*b, obj.bounds.w, obj.bounds.h);
            }
        }
    } else {
        for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
            if (p > 0) {
                body << "<w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>\n";
            }
            const auto& pg = doc.Pages()[p];
            if (p < optionalPageSnapshotsPng.size() && !optionalPageSnapshotsPng[p].empty()) {
                addImage(optionalPageSnapshotsPng[p], 440.0, 440.0 * (pg.EffectiveHeightPt() / (std::max)(1.0, pg.EffectiveWidthPt())));
            }
            const std::string textBlock = !pg.extractedTextUtf8.empty() ? pg.extractedTextUtf8 : pg.ocrTextUtf8;
            for (const auto& para : SplitParagraphs(textBlock)) {
                body << "<w:p><w:r><w:t xml:space=\"preserve\">" << XmlEscape(para) << "</w:t></w:r></w:p>\n";
            }
            for (const auto& obj : pg.objects) {
                if (obj.kind == ObjectKind::TextBox && !obj.textUtf8.empty()) {
                    for (const auto& para : SplitParagraphs(obj.textUtf8)) {
                        body << "<w:p><w:r><w:t xml:space=\"preserve\">" << XmlEscape(para) << "</w:t></w:r></w:p>\n";
                    }
                } else if (obj.kind == ObjectKind::MathBlock && !obj.formulaExprUtf8.empty()) {
                    body << "<w:p>" << FormulaExprToOmmlXml(obj.formulaExprUtf8) << "</w:p>\n";
                } else if (obj.kind == ObjectKind::TableBlock && !obj.tableJsonUtf8.empty()) {
                    try {
                        const auto tj = nlohmann::json::parse(obj.tableJsonUtf8);
                        if (tj.contains("rows") && tj["rows"].is_array()) {
                            body << "<w:tbl>\n";
                            for (const auto& row : tj["rows"]) {
                                if (!row.is_array()) continue;
                                body << "<w:tr>";
                                for (const auto& cell : row) {
                                    body << "<w:tc><w:p><w:r><w:t>" << XmlEscape(cell.is_string() ? cell.get<std::string>() : cell.dump()) << "</w:t></w:r></w:p></w:tc>";
                                }
                                body << "</w:tr>\n";
                            }
                            body << "</w:tbl>\n";
                        }
                    } catch (...) {}
                } else if (obj.kind == ObjectKind::Image) {
                    const auto* b = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
                    if (b) addImage(*b, obj.bounds.w, obj.bounds.h);
                }
            }
        }
    }

    body << "<w:sectPr><w:pgSz w:w=\"11906\" w:h=\"16838\"/></w:sectPr>\n</w:body>\n</w:document>";
    rels << "</Relationships>";

    zip.SetTextEntry("word/_rels/document.xml.rels", rels.str());
    zip.SetTextEntry("word/document.xml", body.str());
    return zip.Serialize(outDocxBytes, true, outErr);
}

bool OpenXmlInterop::ExportToDocxFile(
    const DocumentModel& doc,
    const std::wstring& outPath,
    const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ExportToDocxBytes(doc, optionalPageSnapshotsPng, bytes, outErr)) return false;
    return WriteAllFile(outPath, bytes, outErr);
}

bool OpenXmlInterop::ImportFromDocxBytes(
    const std::uint8_t* bytes,
    std::size_t size,
    DocumentModel& outDoc,
    std::string* outErr)
{
    ZipArchive zip;
    if (!zip.Deserialize(bytes, size, outErr)) return false;
    const auto* docEntry = zip.FindEntry("word/document.xml");
    if (!docEntry) {
        if (outErr) *outErr = "El archivo .docx no contiene word/document.xml.";
        return false;
    }

    const std::string xml(reinterpret_cast<const char*>(docEntry->data()), docEntry->size());
    std::map<std::string, std::string> rels;
    if (const auto* rEntry = zip.FindEntry("word/_rels/document.xml.rels")) {
        rels = ParseRelationshipsXml(std::string(reinterpret_cast<const char*>(rEntry->data()), rEntry->size()));
    }

    DocumentModel temp;
    temp.ResetToBlank(DocumentMode::PaginatedPdf);
    temp.SetTitle("Importado de Word");
    temp.MutablePages().clear();

    auto startNewPage = [&]() -> std::size_t {
        PageModel pm{};
        pm.pageId = temp.AllocatePageId();
        pm.widthPt = 595.0;
        pm.heightPt = 842.0;
        pm.background = PaperPattern::Plain;
        temp.MutablePages().push_back(std::move(pm));
        return temp.Pages().size() - 1;
    };

    std::size_t curPage = startNewPage();
    double curY = 48.0;
    auto ensureSpace = [&](double neededH) {
        if (curY + neededH > 792.0) {
            curPage = startNewPage();
            curY = 48.0;
        }
    };

    // Walk top-level <w:p> and <w:tbl> blocks in document order
    const std::size_t bodyStart = xml.find("<w:body");
    std::size_t scan = (bodyStart == std::string::npos) ? 0 : xml.find('>', bodyStart);
    if (scan != std::string::npos) ++scan;
    std::set<std::string> importedMedia;

    while (scan < xml.size()) {
        const std::size_t nextP = xml.find("<w:p", scan);
        const std::size_t nextTbl = xml.find("<w:tbl", scan);
        const bool pickTbl = (nextTbl != std::string::npos && (nextP == std::string::npos || nextTbl < nextP));
        const std::size_t nextPos = pickTbl ? nextTbl : nextP;
        if (nextPos == std::string::npos) break;

        const std::string tagName = pickTbl ? "w:tbl" : "w:p";
        const std::size_t after = nextPos + 1 + tagName.size();
        if (after >= xml.size() || (xml[after] != '>' && xml[after] != ' ' && xml[after] != '/')) {
            scan = after;
            continue;
        }
        const auto blocks = ExtractTagBlocks(xml.substr(nextPos, (std::min<std::size_t>)(xml.size() - nextPos, 131072u)), tagName);
        if (blocks.empty()) {
            scan = after;
            continue;
        }
        const std::string& blk = blocks[0];
        scan = nextPos + blk.size();

        if (pickTbl) {
            std::string preview;
            const std::string tblJson = ParseTableXmlToJson(blk, "w:tr", "w:tc", "w:t", preview);
            if (!preview.empty()) {
                ensureSpace(120.0);
                AnnotationObject tblObj{};
                tblObj.kind = ObjectKind::TableBlock;
                tblObj.bounds = { 48.0, curY, 499.0, 110.0 };
                tblObj.tableJsonUtf8 = tblJson;
                tblObj.textUtf8 = preview;
                tblObj.captionUtf8 = "Tabla Word";
                temp.AddPageObject(curPage, std::move(tblObj));
                if (!temp.MutablePages()[curPage].extractedTextUtf8.empty()) temp.MutablePages()[curPage].extractedTextUtf8 += '\n';
                temp.MutablePages()[curPage].extractedTextUtf8 += preview;
                curY += 124.0;
            }
            continue;
        }

        // Page break check
        if (blk.find("w:type=\"page\"") != std::string::npos) {
            curPage = startNewPage();
            curY = 48.0;
        }

        // Check embedded OMML math (<m:oMath>)
        const auto maths = ExtractTagBlocks(blk, "m:oMath");
        for (const auto& mBlk : maths) {
            const std::string expr = OmmlToFormulaExpr(mBlk);
            if (!expr.empty()) {
                ensureSpace(64.0);
                AnnotationObject mb{};
                mb.kind = ObjectKind::MathBlock;
                mb.bounds = { 48.0, curY, 460.0, 56.0 };
                mb.formulaExprUtf8 = expr;
                mb.captionUtf8 = "Ecuaci\xC3\xB3n Word";
                const auto eval = Cas::EvaluateExpression(expr);
                if (eval.ok) mb.evaluatedResultUtf8 = eval.formattedResultUtf8;
                temp.AddPageObject(curPage, std::move(mb));
                curY += 68.0;
            }
        }

        // Check embedded inline drawing (<a:blip r:embed="rId..."/>)
        const std::size_t blipPos = blk.find("r:embed=\"");
        if (blipPos != std::string::npos) {
            const std::size_t s = blipPos + 9;
            const std::size_t e = blk.find('"', s);
            if (e != std::string::npos) {
                const std::string rid = blk.substr(s, e - s);
                auto rit = rels.find(rid);
                if (rit != rels.end()) {
                    const std::string fullPath = (rit->second.rfind("word/", 0) == 0) ? rit->second : ("word/" + rit->second);
                    if (const auto* media = zip.FindEntry(fullPath)) {
                        ensureSpace(190.0);
                        AnnotationObject img{};
                        img.kind = ObjectKind::Image;
                        img.bounds = { 64.0, curY, 320.0, 180.0 };
                        img.pngBytes = *media;
                        temp.AddPageObject(curPage, std::move(img));
                        importedMedia.insert(fullPath);
                        curY += 196.0;
                    }
                }
            }
        }

        // Paragraph text + Heading style / Bold / Italic / List detection
        const std::string pText = ExtractXmlTagTextParagraphs(blk, "", "w:t");
        if (!pText.empty()) {
            const bool isHeading1 = (blk.find("Heading1") != std::string::npos || blk.find("Ttulo1") != std::string::npos || blk.find("Title") != std::string::npos);
            const bool isHeading2 = (blk.find("Heading2") != std::string::npos || blk.find("Heading3") != std::string::npos);
            const bool isBold = isHeading1 || isHeading2 || (blk.find("<w:b/>") != std::string::npos || blk.find("<w:b ") != std::string::npos);
            const bool isItalic = (blk.find("<w:i/>") != std::string::npos || blk.find("<w:i ") != std::string::npos);
            const bool isUnderline = (blk.find("<w:u ") != std::string::npos);
            const bool isList = (blk.find("<w:numPr>") != std::string::npos);

            const double fs = isHeading1 ? 18.0 : (isHeading2 ? 14.5 : 11.5);
            const double estLines = (std::max)(1.0, std::ceil(static_cast<double>(pText.size()) / 76.0));
            const double boxH = (std::max)(26.0, estLines * (fs * 1.45) + 10.0);
            ensureSpace(boxH);

            AnnotationObject tb{};
            tb.kind = ObjectKind::TextBox;
            tb.bounds = { isList ? 64.0 : 48.0, curY, 499.0, boxH };
            tb.textUtf8 = isList ? ("\xE2\x80\xA2 " + pText) : pText;
            tb.strokeColor = DocColor::RGBA(24, 28, 36, 255);
            tb.fontSizePt = fs;
            tb.bold = isBold;
            tb.italic = isItalic;
            tb.underline = isUnderline;
            if (isList) tb.listStyleUtf8 = "bullet";
            temp.AddPageObject(curPage, std::move(tb));

            if (isHeading1 || isHeading2) {
                temp.AddBookmark(pText, curPage, curY, isHeading1 ? 0 : 1);
                if (isHeading1 && temp.Title() == "Importado de Word") temp.SetTitle(pText);
            }
            if (!temp.MutablePages()[curPage].extractedTextUtf8.empty()) temp.MutablePages()[curPage].extractedTextUtf8 += '\n';
            temp.MutablePages()[curPage].extractedTextUtf8 += pText;
            curY += boxH + 8.0;
        }
    }

    // Also attach any remaining unreferenced word/media/* images
    for (const std::string& ePath : zip.ListEntries()) {
        if (ePath.rfind("word/media/", 0) == 0 && importedMedia.find(ePath) == importedMedia.end()) {
            if (const auto* media = zip.FindEntry(ePath)) {
                ensureSpace(190.0);
                AnnotationObject img{};
                img.kind = ObjectKind::Image;
                img.bounds = { 72.0, curY, 300.0, 180.0 };
                img.pngBytes = *media;
                temp.AddPageObject(curPage, std::move(img));
                curY += 196.0;
            }
        }
    }

    temp.RebuildLayout();
    temp.SetDirty(false);
    outDoc = std::move(temp);
    return true;
}

bool OpenXmlInterop::ImportFromDocxFile(
    const std::wstring& path,
    DocumentModel& outDoc,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadAllFile(path, bytes, outErr)) return false;
    return ImportFromDocxBytes(bytes.data(), bytes.size(), outDoc, outErr);
}

bool OpenXmlInterop::ExportToPptxBytes(
    const DocumentModel& doc,
    const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng,
    std::vector<std::uint8_t>& outPptxBytes,
    std::string* outErr)
{
    const std::size_t slideCount = (doc.Mode() == DocumentMode::InfiniteCanvas)
        ? 1u
        : (std::max<std::size_t>)(1u, doc.Pages().size());

    ZipArchive zip;
    std::ostringstream ct;
    ct << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
       << "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
       << "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
       << "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
       << "<Default Extension=\"png\" ContentType=\"image/png\"/>"
       << "<Override PartName=\"/ppt/presentation.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml\"/>"
       << "<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml\"/>"
       << "<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml\"/>"
       << "<Override PartName=\"/ppt/theme/theme1.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>";
    for (std::size_t i = 0; i < slideCount; ++i) {
        ct << "<Override PartName=\"/ppt/slides/slide" << (i + 1)
           << ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slide+xml\"/>";
    }
    std::vector<std::size_t> notesSlideIndices1;

    zip.SetTextEntry("_rels/.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"ppt/presentation.xml\"/>"
        "</Relationships>");

    zip.SetTextEntry("ppt/theme/theme1.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<a:theme xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" name=\"DocStudio\">"
        "<a:themeElements>"
        "<a:clrScheme name=\"Default\"><a:dk1><a:srgbClr val=\"111827\"/></a:dk1><a:lt1><a:srgbClr val=\"FFFFFF\"/></a:lt1>"
        "<a:dk2><a:srgbClr val=\"334155\"/></a:dk2><a:lt2><a:srgbClr val=\"F1F5F9\"/></a:lt2>"
        "<a:accent1><a:srgbClr val=\"1D4ED8\"/></a:accent1><a:accent2><a:srgbClr val=\"0EA5E9\"/></a:accent2>"
        "<a:accent3><a:srgbClr val=\"10B981\"/></a:accent3><a:accent4><a:srgbClr val=\"F59E0B\"/></a:accent4>"
        "<a:accent5><a:srgbClr val=\"8B5CF6\"/></a:accent5><a:accent6><a:srgbClr val=\"EC4899\"/></a:accent6>"
        "<a:hlink><a:srgbClr val=\"2563EB\"/></a:hlink><a:folHlink><a:srgbClr val=\"7C3AED\"/></a:folHlink></a:clrScheme>"
        "<a:fontScheme name=\"Default\"><a:majorFont><a:latin typeface=\"Segoe UI\"/></a:majorFont><a:minorFont><a:latin typeface=\"Segoe UI\"/></a:minorFont></a:fontScheme>"
        "<a:fmtScheme name=\"Default\"><a:fillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:fillStyleLst>"
        "<a:lnStyleLst><a:ln w=\"9525\"><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:ln></a:lnStyleLst>"
        "<a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst>"
        "<a:bgFillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:bgFillStyleLst></a:fmtScheme>"
        "</a:themeElements></a:theme>");

    zip.SetTextEntry("ppt/slideMasters/_rels/slideMaster1.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>"
        "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" Target=\"../theme/theme1.xml\"/>"
        "</Relationships>");

    zip.SetTextEntry("ppt/slideMasters/slideMaster1.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<p:sldMaster xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
        "<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/></p:spTree></p:cSld>"
        "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" hlink=\"hlink\" folHlink=\"folHlink\"/>"
        "<p:sldLayoutIdLst><p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/></p:sldLayoutIdLst></p:sldMaster>");

    zip.SetTextEntry("ppt/slideLayouts/_rels/slideLayout1.xml.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" Target=\"../slideMasters/slideMaster1.xml\"/>"
        "</Relationships>");

    zip.SetTextEntry("ppt/slideLayouts/slideLayout1.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
        "<p:sldLayout xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" type=\"blank\">"
        "<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/></p:spTree></p:cSld></p:sldLayout>");

    std::ostringstream presRels;
    presRels << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
             << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
             << "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" Target=\"slideMasters/slideMaster1.xml\"/>\n";

    std::ostringstream presXml;
    presXml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
            << "<p:presentation xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">\n"
            << "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst>\n"
            << "<p:sldIdLst>\n";

    std::size_t globalMediaIdx = 0;

    for (std::size_t i = 0; i < slideCount; ++i) {
        const std::string presRelId = "rId" + std::to_string(i + 2);
        presRels << "<Relationship Id=\"" << presRelId
                 << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide\" Target=\"slides/slide"
                 << (i + 1) << ".xml\"/>\n";
        presXml << "<p:sldId id=\"" << (256 + i) << "\" r:id=\"" << presRelId << "\"/>\n";

        std::ostringstream sRels;
        sRels << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
              << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">\n"
              << "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>\n";

        std::ostringstream slideXml;
        slideXml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                 << "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\">\n"
                 << "<p:cSld><p:spTree>\n"
                 << "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>\n";

        std::size_t shapeId = 2;
        std::size_t sRelCounter = 1;

        auto hexRgb = [](DocColor c) {
            char buf[8]{};
            std::snprintf(buf, sizeof(buf), "%02X%02X%02X", c.r, c.g, c.b);
            return std::string(buf);
        };

        auto appendTextShape = [&](const std::string& text, std::int64_t x, std::int64_t y, std::int64_t cx, std::int64_t cy,
                                   double fontPt = 14.0, DocColor color = DocColor::RGBA(20, 20, 25, 255), bool bold = false) {
            if (text.empty()) return;
            const int szHundredths = (std::max)(800, (std::min)(7200, static_cast<int>(fontPt * 100.0)));
            slideXml << "<p:sp><p:nvSpPr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"Texto\"/><p:cNvSpPr txBox=\"1\"/><p:nvPr/></p:nvSpPr>"
                     << "<p:spPr><a:xfrm><a:off x=\"" << x << "\" y=\"" << y << "\"/><a:ext cx=\"" << (std::max<std::int64_t>)(127000, cx) << "\" cy=\"" << (std::max<std::int64_t>)(127000, cy) << "\"/></a:xfrm>"
                     << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom><a:noFill/></p:spPr><p:txBody><a:bodyPr wrap=\"square\"/><a:lstStyle/>";
            for (const auto& line : SplitParagraphs(text)) {
                slideXml << "<a:p><a:r><a:rPr lang=\"es-ES\" sz=\"" << szHundredths << "\""
                         << (bold ? " b=\"1\"" : "")
                         << "><a:solidFill><a:srgbClr val=\"" << hexRgb(color) << "\"/></a:solidFill></a:rPr><a:t>"
                         << XmlEscape(line) << "</a:t></a:r></a:p>";
            }
            slideXml << "</p:txBody></p:sp>\n";
        };

        std::ostringstream speakerNotes;
        auto appendObjectsToSlide = [&](const std::vector<AnnotationObject>& objs, double srcW, double srcH,
                                        std::int64_t offX, std::int64_t offY, std::int64_t boxW, std::int64_t boxH) {
            const double sx = static_cast<double>(boxW) / (std::max)(72.0, srcW);
            const double sy = static_cast<double>(boxH) / (std::max)(72.0, srcH);
            const double s = (std::min)(sx, sy);
            for (const auto& obj : objs) {
                if (!doc.IsLayerExportable(obj.layerId)) {
                    if (!obj.textUtf8.empty()) {
                        if (speakerNotes.tellp() > 0) speakerNotes << "\n";
                        speakerNotes << obj.textUtf8;
                    } else if (!obj.formulaExprUtf8.empty()) {
                        if (speakerNotes.tellp() > 0) speakerNotes << "\n";
                        speakerNotes << obj.formulaExprUtf8;
                    }
                    continue;
                }
                const std::int64_t ox = offX + static_cast<std::int64_t>(obj.bounds.x * s);
                const std::int64_t oy = offY + static_cast<std::int64_t>(obj.bounds.y * s);
                const std::int64_t ow = (std::max<std::int64_t>)(91440, static_cast<std::int64_t>(obj.bounds.w * s));
                const std::int64_t oh = (std::max<std::int64_t>)(91440, static_cast<std::int64_t>(obj.bounds.h * s));
                if (obj.kind == ObjectKind::TextBox && !obj.textUtf8.empty()) {
                    appendTextShape(obj.textUtf8, ox, oy, ow, oh, obj.fontSizePt, obj.strokeColor, obj.bold);
                } else if (obj.kind == ObjectKind::Shape) {
                    const char* prst = (obj.shapeKind == ShapeKind::Ellipse) ? "ellipse"
                                     : (obj.shapeKind == ShapeKind::Arrow) ? "rightArrow"
                                     : "rect";
                    slideXml << "<p:sp><p:nvSpPr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"Forma\"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>"
                             << "<p:spPr><a:xfrm><a:off x=\"" << ox << "\" y=\"" << oy << "\"/><a:ext cx=\"" << ow << "\" cy=\"" << oh << "\"/></a:xfrm>"
                             << "<a:prstGeom prst=\"" << prst << "\"><a:avLst/></a:prstGeom>";
                    if (obj.fillColor.a > 0) {
                        slideXml << "<a:solidFill><a:srgbClr val=\"" << hexRgb(obj.fillColor) << "\"/></a:solidFill>";
                    } else {
                        slideXml << "<a:noFill/>";
                    }
                    slideXml << "<a:ln w=\"" << static_cast<std::int64_t>((std::max)(1.0, obj.strokeWidth) * 12700.0)
                             << "\"><a:solidFill><a:srgbClr val=\"" << hexRgb(obj.strokeColor) << "\"/></a:solidFill></a:ln>"
                             << "</p:spPr>";
                    if (!obj.textUtf8.empty()) {
                        slideXml << "<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang=\"es-ES\" sz=\"1200\"/><a:t>"
                                 << XmlEscape(obj.textUtf8) << "</a:t></a:r></a:p></p:txBody>";
                    }
                    slideXml << "</p:sp>\n";
                } else if (obj.kind == ObjectKind::InkStroke && obj.inkPoints.size() >= 2) {
                    const double bw = (std::max)(1.0, obj.bounds.w);
                    const double bh = (std::max)(1.0, obj.bounds.h);
                    slideXml << "<p:sp><p:nvSpPr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"Tinta\"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>"
                             << "<p:spPr><a:xfrm><a:off x=\"" << ox << "\" y=\"" << oy << "\"/><a:ext cx=\"" << ow << "\" cy=\"" << oh << "\"/></a:xfrm>"
                             << "<a:custGeom><a:avLst/><a:gdLst/><a:ahLst/><a:cxnLst/><a:rect l=\"0\" t=\"0\" r=\"0\" b=\"0\"/>"
                             << "<a:pathLst><a:path w=\"1000\" h=\"1000\">";
                    for (std::size_t k = 0; k < obj.inkPoints.size(); ++k) {
                        const int px = static_cast<int>(std::clamp((obj.inkPoints[k].x - obj.bounds.x) / bw, 0.0, 1.0) * 1000.0);
                        const int py = static_cast<int>(std::clamp((obj.inkPoints[k].y - obj.bounds.y) / bh, 0.0, 1.0) * 1000.0);
                        slideXml << (k == 0 ? "<a:moveTo><a:pt x=\"" : "<a:lnTo><a:pt x=\"")
                                 << px << "\" y=\"" << py << "\"/>" << (k == 0 ? "</a:moveTo>" : "</a:lnTo>");
                    }
                    slideXml << "</a:path></a:pathLst></a:custGeom><a:noFill/>"
                             << "<a:ln w=\"" << static_cast<std::int64_t>((std::max)(1.0, obj.strokeWidth) * 12700.0)
                             << "\"><a:solidFill><a:srgbClr val=\"" << hexRgb(obj.strokeColor) << "\"/></a:solidFill></a:ln>"
                             << "</p:spPr></p:sp>\n";
                } else if (obj.kind == ObjectKind::MathBlock && !obj.formulaExprUtf8.empty()) {
                    slideXml << "<p:sp><p:nvSpPr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"Ecuacion\"/><p:cNvSpPr txBox=\"1\"/><p:nvPr/></p:nvSpPr>"
                             << "<p:spPr><a:xfrm><a:off x=\"" << ox << "\" y=\"" << oy << "\"/><a:ext cx=\"" << ow << "\" cy=\"" << oh << "\"/></a:xfrm>"
                             << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom><a:noFill/></p:spPr>"
                             << "<p:txBody><a:bodyPr wrap=\"square\"/><a:lstStyle/><a:p>"
                             << "<m:oMath xmlns:m=\"http://schemas.openxmlformats.org/officeDocument/2006/math\">"
                             << "<m:r><m:t>" << XmlEscape(obj.formulaExprUtf8) << "</m:t></m:r></m:oMath>"
                             << "</a:p></p:txBody></p:sp>\n";
                } else if (obj.kind == ObjectKind::TableBlock && !obj.tableJsonUtf8.empty()) {
                    try {
                        const auto j = nlohmann::json::parse(obj.tableJsonUtf8);
                        if (j.is_object() && j.contains("rows") && j["rows"].is_array() && !j["rows"].empty()) {
                            std::size_t maxCols = 1;
                            for (const auto& rj : j["rows"]) {
                                if (rj.is_array() && rj.size() > maxCols) maxCols = rj.size();
                            }
                            const std::int64_t colW = (std::max<std::int64_t>)(457200, ow / static_cast<std::int64_t>(maxCols));
                            slideXml << "<p:graphicFrame><p:nvGraphicFramePr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"Tabla\"/>"
                                     << "<p:cNvGraphicFramePr/><p:nvPr/></p:nvGraphicFramePr>"
                                     << "<p:xfrm><a:off x=\"" << ox << "\" y=\"" << oy << "\"/><a:ext cx=\"" << ow << "\" cy=\"" << oh << "\"/></p:xfrm>"
                                     << "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/table\"><a:tbl><a:tblPr/><a:tblGrid>";
                            for (std::size_t c = 0; c < maxCols; ++c) {
                                slideXml << "<a:gridCol w=\"" << colW << "\"/>";
                            }
                            slideXml << "</a:tblGrid>";
                            for (const auto& rj : j["rows"]) {
                                if (!rj.is_array()) continue;
                                slideXml << "<a:tr h=\"370840\">";
                                for (std::size_t c = 0; c < maxCols; ++c) {
                                    std::string cellTxt;
                                    if (c < rj.size()) {
                                        cellTxt = rj[c].is_string() ? rj[c].get<std::string>() : rj[c].dump();
                                    }
                                    slideXml << "<a:tc><a:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>"
                                             << XmlEscape(cellTxt) << "</a:t></a:r></a:p></a:txBody></a:tc>";
                                }
                                slideXml << "</a:tr>";
                            }
                            slideXml << "</a:tbl></a:graphicData></a:graphic></p:graphicFrame>\n";
                        }
                    } catch (...) {}
                } else if (obj.kind == ObjectKind::Image) {
                    const std::vector<std::uint8_t>* imgPtr = !obj.pngBytes.empty() ? &obj.pngBytes : doc.FindAsset(obj.assetKey);
                    if (imgPtr && !imgPtr->empty()) {
                        const std::string mediaName = "objImg" + std::to_string(++globalMediaIdx) + ".png";
                        const std::string rid = "rId" + std::to_string(++sRelCounter);
                        zip.SetEntry("ppt/media/" + mediaName, *imgPtr);
                        sRels << "<Relationship Id=\"" << rid
                              << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"../media/"
                              << mediaName << "\"/>\n";
                        slideXml << "<p:pic><p:nvPicPr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"" << mediaName << "\"/><p:cNvPicPr/><p:nvPr/></p:nvPicPr>"
                                 << "<p:blipFill><a:blip r:embed=\"" << rid << "\"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>"
                                 << "<p:spPr><a:xfrm><a:off x=\"" << ox << "\" y=\"" << oy << "\"/><a:ext cx=\"" << ow << "\" cy=\"" << oh << "\"/></a:xfrm>"
                                 << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>\n";
                    }
                }
            }
        };

        const bool hasSnap = (i < optionalPageSnapshotsPng.size() && !optionalPageSnapshotsPng[i].empty());
        if (hasSnap) {
            const std::string mediaName = "slideImg" + std::to_string(++globalMediaIdx) + ".png";
            const std::string rid = "rId" + std::to_string(++sRelCounter);
            zip.SetEntry("ppt/media/" + mediaName, optionalPageSnapshotsPng[i]);
            sRels << "<Relationship Id=\"" << rid
                  << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" Target=\"../media/"
                  << mediaName << "\"/>\n";
            slideXml << "<p:pic><p:nvPicPr><p:cNvPr id=\"" << (++shapeId) << "\" name=\"" << mediaName << "\"/><p:cNvPicPr/><p:nvPr/></p:nvPicPr>"
                     << "<p:blipFill><a:blip r:embed=\"" << rid << "\"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>"
                     << "<p:spPr><a:xfrm><a:off x=\"457200\" y=\"365760\"/><a:ext cx=\"4572000\" cy=\"5800000\"/></a:xfrm>"
                     << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></p:spPr></p:pic>\n";
        }

        if (doc.Mode() == DocumentMode::InfiniteCanvas) {
            double maxW = 800.0, maxH = 600.0;
            for (const auto& obj : doc.CanvasObjects()) {
                maxW = (std::max)(maxW, obj.bounds.Right() + 40.0);
                maxH = (std::max)(maxH, obj.bounds.Bottom() + 40.0);
            }
            if (hasSnap) {
                appendObjectsToSlide(doc.CanvasObjects(), maxW, maxH, 5200000, 457200, 6500000, 5800000);
            } else {
                appendObjectsToSlide(doc.CanvasObjects(), maxW, maxH, 457200, 365760, 11200000, 6100000);
            }
            if (!doc.Title().empty()) {
                appendTextShape(doc.Title(), hasSnap ? 5200000 : 457200, 140000, 6500000, 240000, 16.0, DocColor::RGBA(20, 45, 95, 255), true);
            }
        } else if (i < doc.Pages().size()) {
            const auto& pg = doc.Pages()[i];
            // Only emit extracted/OCR background PDF text as a slide text shape;
            // positioned TextBox annotations are emitted individually by appendObjectsToSlide (no duplication).
            const std::string slideText = !pg.extractedTextUtf8.empty() ? pg.extractedTextUtf8 : pg.ocrTextUtf8;
            const std::int64_t tx = hasSnap ? 5200000 : 457200;
            const std::int64_t tw = hasSnap ? 6500000 : 11200000;
            if (!slideText.empty()) {
                appendTextShape(slideText, tx, 457200, tw, 5800000);
            } else if (!hasSnap && pg.objects.empty()) {
                appendTextShape(doc.Title() + " - P\xC3\xA1gina " + std::to_string(i + 1), tx, 457200, tw, 5800000);
            }
            if (!pg.objects.empty()) {
                if (hasSnap) {
                    appendObjectsToSlide(pg.objects, pg.EffectiveWidthPt(), pg.EffectiveHeightPt(), 5200000, 457200, 6500000, 5800000);
                } else {
                    appendObjectsToSlide(pg.objects, pg.EffectiveWidthPt(), pg.EffectiveHeightPt(), 457200, 457200, 11200000, 5800000);
                }
            }
        }

        const std::string notesText = speakerNotes.str();
        if (!notesText.empty()) {
            const std::string notesRelId = "rId" + std::to_string(++sRelCounter);
            const std::size_t sNum = i + 1;
            notesSlideIndices1.push_back(sNum);
            sRels << "<Relationship Id=\"" << notesRelId
                  << "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/notesSlide\" Target=\"../notesSlides/notesSlide"
                  << sNum << ".xml\"/>\n";
            std::ostringstream nRels;
            nRels << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                  << "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
                  << "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide\" Target=\"../slides/slide"
                  << sNum << ".xml\"/></Relationships>";
            std::ostringstream nXml;
            nXml << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                 << "<p:notes xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
                 << "<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr/>"
                 << "<p:sp><p:nvSpPr><p:cNvPr id=\"2\" name=\"Notas del Orador\"/><p:cNvSpPr txBox=\"1\"/><p:nvPr><p:ph type=\"body\" idx=\"1\"/></p:nvPr></p:nvSpPr>"
                 << "<p:spPr/><p:txBody><a:bodyPr/><a:lstStyle/>";
            for (const auto& line : SplitParagraphs(notesText)) {
                nXml << "<a:p><a:r><a:rPr lang=\"es-ES\" sz=\"1200\"/><a:t>" << XmlEscape(line) << "</a:t></a:r></a:p>";
            }
            nXml << "</p:txBody></p:sp></p:spTree></p:cSld></p:notes>";
            zip.SetTextEntry("ppt/notesSlides/_rels/notesSlide" + std::to_string(sNum) + ".xml.rels", nRels.str());
            zip.SetTextEntry("ppt/notesSlides/notesSlide" + std::to_string(sNum) + ".xml", nXml.str());
        }

        slideXml << "</p:spTree></p:cSld></p:sld>";
        sRels << "</Relationships>";
        zip.SetTextEntry("ppt/slides/_rels/slide" + std::to_string(i + 1) + ".xml.rels", sRels.str());
        zip.SetTextEntry("ppt/slides/slide" + std::to_string(i + 1) + ".xml", slideXml.str());
    }

    for (std::size_t nIdx : notesSlideIndices1) {
        ct << "<Override PartName=\"/ppt/notesSlides/notesSlide" << nIdx
           << ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.notesSlide+xml\"/>";
    }
    ct << "</Types>";
    zip.SetTextEntry("[Content_Types].xml", ct.str());

    presXml << "</p:sldIdLst>\n<p:sldSz cx=\"12192000\" cy=\"6858000\"/><p:notesSz cx=\"6858000\" cy=\"9144000\"/></p:presentation>";
    presRels << "</Relationships>";
    zip.SetTextEntry("ppt/_rels/presentation.xml.rels", presRels.str());
    zip.SetTextEntry("ppt/presentation.xml", presXml.str());
    return zip.Serialize(outPptxBytes, true, outErr);
}

bool OpenXmlInterop::ExportToPptxFile(
    const DocumentModel& doc,
    const std::wstring& outPath,
    const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ExportToPptxBytes(doc, optionalPageSnapshotsPng, bytes, outErr)) return false;
    return WriteAllFile(outPath, bytes, outErr);
}

bool OpenXmlInterop::ImportFromPptxBytes(
    const std::uint8_t* bytes,
    std::size_t size,
    DocumentModel& outDoc,
    std::string* outErr)
{
    ZipArchive zip;
    if (!zip.Deserialize(bytes, size, outErr)) return false;

    std::vector<std::pair<int, std::string>> slidePaths;
    for (const std::string& p : zip.ListEntries()) {
        constexpr const char* kPrefix = "ppt/slides/slide";
        if (p.rfind(kPrefix, 0) == 0 && p.size() > 20 && p.compare(p.size() - 4, 4, ".xml") == 0) {
            const int num = std::atoi(p.substr(16, p.size() - 20).c_str());
            if (num > 0) slidePaths.push_back({ num, p });
        }
    }
    if (slidePaths.empty()) {
        if (outErr) *outErr = "El archivo .pptx no contiene diapositivas.";
        return false;
    }
    std::sort(slidePaths.begin(), slidePaths.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    // Read presentation slide size in EMUs (default 16:9 = 12192000 x 6858000 -> 960 x 540 pt)
    double sldCxEmu = 12192000.0;
    double sldCyEmu = 6858000.0;
    if (const auto* pres = zip.FindEntry("ppt/presentation.xml")) {
        const std::string pXml(reinterpret_cast<const char*>(pres->data()), pres->size());
        const std::size_t szPos = pXml.find("<p:sldSz ");
        if (szPos != std::string::npos) {
            const std::size_t gt = pXml.find('>', szPos);
            if (gt != std::string::npos) {
                const std::string tag = pXml.substr(szPos, gt - szPos + 1);
                const std::string cxStr = ExtractXmlAttribute(tag, "cx");
                const std::string cyStr = ExtractXmlAttribute(tag, "cy");
                if (!cxStr.empty()) sldCxEmu = (std::max)(1000000.0, std::atof(cxStr.c_str()));
                if (!cyStr.empty()) sldCyEmu = (std::max)(1000000.0, std::atof(cyStr.c_str()));
            }
        }
    }

    constexpr double kSlidePtW = 960.0;
    const double kSlidePtH = std::clamp(kSlidePtW * (sldCyEmu / sldCxEmu), 360.0, 960.0);
    const double emuToPtX = kSlidePtW / sldCxEmu;
    const double emuToPtY = kSlidePtH / sldCyEmu;

    auto extractXfrmBounds = [&](const std::string& blk, DocRect fallback) -> DocRect {
        const std::size_t offPos = blk.find("<a:off ");
        const std::size_t extPos = blk.find("<a:ext ");
        if (offPos == std::string::npos || extPos == std::string::npos) return fallback;
        const std::size_t offGt = blk.find('>', offPos);
        const std::size_t extGt = blk.find('>', extPos);
        if (offGt == std::string::npos || extGt == std::string::npos) return fallback;
        const std::string offTag = blk.substr(offPos, offGt - offPos + 1);
        const std::string extTag = blk.substr(extPos, extGt - extPos + 1);
        const double x = std::atof(ExtractXmlAttribute(offTag, "x").c_str()) * emuToPtX;
        const double y = std::atof(ExtractXmlAttribute(offTag, "y").c_str()) * emuToPtY;
        const double w = std::atof(ExtractXmlAttribute(extTag, "cx").c_str()) * emuToPtX;
        const double h = std::atof(ExtractXmlAttribute(extTag, "cy").c_str()) * emuToPtY;
        if (w < 4.0 || h < 4.0) return fallback;
        return { x, y, w, h };
    };

    DocumentModel temp;
    temp.ResetToBlank(DocumentMode::PaginatedPdf);
    temp.SetTitle("Importado de PowerPoint");
    temp.MutablePages().clear();

    for (std::size_t si = 0; si < slidePaths.size(); ++si) {
        const auto& sp = slidePaths[si];
        const auto* entry = zip.FindEntry(sp.second);
        if (!entry) continue;
        const std::string xml(reinterpret_cast<const char*>(entry->data()), entry->size());

        std::map<std::string, std::string> slideRels;
        const std::string relPath = "ppt/slides/_rels/slide" + std::to_string(sp.first) + ".xml.rels";
        if (const auto* re = zip.FindEntry(relPath)) {
            slideRels = ParseRelationshipsXml(std::string(reinterpret_cast<const char*>(re->data()), re->size()));
        }

        PageModel pm;
        pm.pageId = temp.AllocatePageId();
        pm.widthPt = kSlidePtW;
        pm.heightPt = kSlidePtH;
        const std::string fullSlideText = ExtractXmlTagTextParagraphs(xml, "</a:p>", "a:t");
        pm.extractedTextUtf8 = fullSlideText;
        const std::size_t pageIdx = temp.Pages().size();
        temp.MutablePages().push_back(std::move(pm));
        temp.AddBookmark("Diapositiva " + std::to_string(si + 1), pageIdx, 0.0, 0);

        bool importedAnyObj = false;

        // 1. Positioned shapes, textboxes, OMML equations, and custom vector ink paths (<p:sp>)
        for (const auto& spBlk : ExtractTagBlocks(xml, "p:sp")) {
            const DocRect r = extractXfrmBounds(spBlk, { 48.0, 48.0, 420.0, 120.0 });

            // Check vector ink <a:custGeom>
            if (spBlk.find("<a:custGeom") != std::string::npos) {
                const auto paths = ExtractTagBlocks(spBlk, "a:path");
                for (const auto& pBlk : paths) {
                    const std::size_t pGt = pBlk.find('>');
                    const std::string pTag = (pGt != std::string::npos) ? pBlk.substr(0, pGt + 1) : "";
                    const double pathW = (std::max)(1.0, std::atof(ExtractXmlAttribute(pTag, "w").c_str()));
                    const double pathH = (std::max)(1.0, std::atof(ExtractXmlAttribute(pTag, "h").c_str()));
                    AnnotationObject ink{};
                    ink.kind = ObjectKind::InkStroke;
                    ink.strokeColor = DocColor::RGBA(24, 38, 66, 255);
                    ink.strokeWidth = 2.4;
                    std::size_t ptScan = 0;
                    while ((ptScan = pBlk.find("<a:pt ", ptScan)) != std::string::npos) {
                        const std::size_t gt = pBlk.find('>', ptScan);
                        if (gt == std::string::npos) break;
                        const std::string ptTag = pBlk.substr(ptScan, gt - ptScan + 1);
                        const double px = std::atof(ExtractXmlAttribute(ptTag, "x").c_str());
                        const double py = std::atof(ExtractXmlAttribute(ptTag, "y").c_str());
                        InkPoint ipt{};
                        ipt.x = r.x + (px / pathW) * r.w;
                        ipt.y = r.y + (py / pathH) * r.h;
                        ipt.pressure = 0.6f;
                        ink.inkPoints.push_back(ipt);
                        ptScan = gt + 1;
                    }
                    if (ink.inkPoints.size() >= 2) {
                        ink.RecomputeBounds();
                        temp.AddPageObject(pageIdx, std::move(ink));
                        importedAnyObj = true;
                    }
                }
                continue;
            }

            // Check OMML math inside slide shape
            for (const auto& mBlk : ExtractTagBlocks(spBlk, "m:oMath")) {
                const std::string expr = OmmlToFormulaExpr(mBlk);
                if (!expr.empty()) {
                    AnnotationObject mb{};
                    mb.kind = ObjectKind::MathBlock;
                    mb.bounds = r;
                    mb.formulaExprUtf8 = expr;
                    mb.captionUtf8 = "Ecuaci\xC3\xB3n PowerPoint";
                    const auto ev = Cas::EvaluateExpression(expr);
                    if (ev.ok) mb.evaluatedResultUtf8 = ev.formattedResultUtf8;
                    temp.AddPageObject(pageIdx, std::move(mb));
                    importedAnyObj = true;
                }
            }

            const std::string shapeText = ExtractXmlTagTextParagraphs(spBlk, "</a:p>", "a:t");
            const bool isTxBox = (spBlk.find("txBox=\"1\"") != std::string::npos) ||
                                 (spBlk.find("<a:noFill/>") != std::string::npos && !shapeText.empty());
            if (isTxBox && !shapeText.empty()) {
                AnnotationObject tb{};
                tb.kind = ObjectKind::TextBox;
                tb.bounds = r;
                tb.textUtf8 = shapeText;
                tb.fontSizePt = (spBlk.find("b=\"1\"") != std::string::npos) ? 18.0 : 14.0;
                tb.bold = (spBlk.find("b=\"1\"") != std::string::npos);
                tb.strokeColor = DocColor::RGBA(20, 24, 34, 255);
                temp.AddPageObject(pageIdx, std::move(tb));
                importedAnyObj = true;
            } else if (spBlk.find("<a:prstGeom") != std::string::npos) {
                AnnotationObject sh{};
                sh.kind = ObjectKind::Shape;
                sh.bounds = r;
                sh.shapeStart = { r.x, r.y + r.h * 0.5 };
                sh.shapeEnd = { r.Right(), r.y + r.h * 0.5 };
                if (spBlk.find("prst=\"ellipse\"") != std::string::npos) sh.shapeKind = ShapeKind::Ellipse;
                else if (spBlk.find("Arrow") != std::string::npos) sh.shapeKind = ShapeKind::Arrow;
                else if (spBlk.find("prst=\"line\"") != std::string::npos) sh.shapeKind = ShapeKind::Line;
                else sh.shapeKind = ShapeKind::Rectangle;
                sh.strokeColor = DocColor::RGBA(37, 99, 235, 255);
                sh.strokeWidth = 2.0;
                sh.textUtf8 = shapeText;
                temp.AddPageObject(pageIdx, std::move(sh));
                importedAnyObj = true;
            }
        }

        // 2. Positioned Pictures (<p:pic>) via slide relationships
        for (const auto& picBlk : ExtractTagBlocks(xml, "p:pic")) {
            const std::size_t embPos = picBlk.find("r:embed=\"");
            if (embPos == std::string::npos) continue;
            const std::size_t s = embPos + 9;
            const std::size_t e = picBlk.find('"', s);
            if (e == std::string::npos) continue;
            const std::string rid = picBlk.substr(s, e - s);
            auto rit = slideRels.find(rid);
            if (rit == slideRels.end()) continue;
            std::string target = rit->second;
            if (target.rfind("../", 0) == 0) target = "ppt/" + target.substr(3);
            else if (target.rfind("ppt/", 0) != 0) target = "ppt/slides/" + target;
            if (const auto* m = zip.FindEntry(target)) {
                AnnotationObject img{};
                img.kind = ObjectKind::Image;
                img.bounds = extractXfrmBounds(picBlk, { 48.0, 120.0, 320.0, 220.0 });
                img.pngBytes = *m;
                temp.AddPageObject(pageIdx, std::move(img));
                importedAnyObj = true;
            }
        }

        // 3. Tables (<a:tbl>) inside slide graphic frames
        for (const auto& tblBlk : ExtractTagBlocks(xml, "a:tbl")) {
            std::string preview;
            const std::string tblJson = ParseTableXmlToJson(tblBlk, "a:tr", "a:tc", "a:t", preview);
            if (!preview.empty()) {
                AnnotationObject tblObj{};
                tblObj.kind = ObjectKind::TableBlock;
                tblObj.bounds = { 60.0, 140.0, 820.0, 220.0 };
                tblObj.tableJsonUtf8 = tblJson;
                tblObj.textUtf8 = preview;
                tblObj.captionUtf8 = "Tabla PowerPoint";
                temp.AddPageObject(pageIdx, std::move(tblObj));
                importedAnyObj = true;
            }
        }

        if (!importedAnyObj && !fullSlideText.empty()) {
            AnnotationObject tb{};
            tb.kind = ObjectKind::TextBox;
            tb.bounds = { 48.0, 48.0, 864.0, 420.0 };
            tb.textUtf8 = fullSlideText;
            tb.fontSizePt = 16.0;
            tb.strokeColor = DocColor::RGBA(20, 24, 34, 255);
            temp.AddPageObject(pageIdx, std::move(tb));
        }
    }

    if (temp.Pages().empty()) {
        PageModel pm;
        pm.pageId = temp.AllocatePageId();
        temp.MutablePages().push_back(std::move(pm));
    }

    temp.RebuildLayout();
    temp.SetDirty(false);
    outDoc = std::move(temp);
    return true;
}

bool OpenXmlInterop::ImportFromPptxFile(
    const std::wstring& path,
    DocumentModel& outDoc,
    std::string* outErr)
{
    std::vector<std::uint8_t> bytes;
    if (!ReadAllFile(path, bytes, outErr)) return false;
    return ImportFromPptxBytes(bytes.data(), bytes.size(), outDoc, outErr);
}

} // namespace DocStudio
