#include "academic/tec_digital/material_indexer.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/vault.hpp"
#include "inflate.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;

namespace academic::tec_digital {

namespace {

uint16_t ReadU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

std::string LowerExtension(std::string_view filename) {
    std::string fn(filename);
    size_t slash = fn.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? fn : fn.substr(slash + 1);
    size_t dot = base.rfind('.');
    if (dot == std::string::npos || dot == 0) return {};
    return AsciiLower(base.substr(dot));
}

bool StartsWithBytes(const uint8_t* data, size_t len, const uint8_t* pat, size_t plen) {
    if (data == nullptr || len < plen) return false;
    return std::memcmp(data, pat, plen) == 0;
}

bool IsSkipExtension(std::string_view ext) {
    static const std::unordered_set<std::string> kSkip = {
        ".mp4", ".m4v", ".mov", ".avi", ".mp3", ".wav", ".m4a",
        ".png", ".jpg", ".jpeg", ".gif", ".webp", ".bmp", ".ico", ".svg",
        ".zip", ".exe", ".dll", ".bin", ".ttf", ".woff", ".woff2", ".eot"
    };
    return kSkip.count(std::string(ext)) > 0;
}

bool IsTextExtension(std::string_view ext) {
    static const std::unordered_set<std::string> kText = {
        ".txt", ".m", ".mp", ".cpp", ".h", ".hpp", ".c", ".cc", ".py",
        ".csv", ".json", ".md", ".tex", ".sql", ".xml", ".html", ".js",
        ".ipynb", ".dat", ".in", ".out", ".log"
    };
    return kText.count(std::string(ext)) > 0;
}

void AppendUtf8Codepoint(uint32_t cp, std::string& out) {
    if (cp == 0 || (cp >= 0xD800u && cp <= 0xDFFFu) || cp > 0x10FFFFu) return;
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
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

bool IsValidUtf8Bytes(const uint8_t* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        uint8_t c = data[i];
        if (c < 0x80u) {
            ++i;
        } else if ((c & 0xE0u) == 0xC0u) {
            if (i + 1 >= len) return false;
            uint8_t c1 = data[i + 1];
            if ((c1 & 0xC0u) != 0x80u || c < 0xC2u) return false;
            i += 2;
        } else if ((c & 0xF0u) == 0xE0u) {
            if (i + 2 >= len) return false;
            uint8_t c1 = data[i + 1];
            uint8_t c2 = data[i + 2];
            if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u) return false;
            uint32_t cp = (static_cast<uint32_t>(c & 0x0Fu) << 12) |
                          (static_cast<uint32_t>(c1 & 0x3Fu) << 6) |
                          static_cast<uint32_t>(c2 & 0x3Fu);
            if (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu)) return false;
            i += 3;
        } else if ((c & 0xF8u) == 0xF0u) {
            if (i + 3 >= len) return false;
            uint8_t c1 = data[i + 1];
            uint8_t c2 = data[i + 2];
            uint8_t c3 = data[i + 3];
            if ((c1 & 0xC0u) != 0x80u || (c2 & 0xC0u) != 0x80u || (c3 & 0xC0u) != 0x80u) return false;
            uint32_t cp = (static_cast<uint32_t>(c & 0x07u) << 18) |
                          (static_cast<uint32_t>(c1 & 0x3Fu) << 12) |
                          (static_cast<uint32_t>(c2 & 0x3Fu) << 6) |
                          static_cast<uint32_t>(c3 & 0x3Fu);
            if (cp < 0x10000u || cp > 0x10FFFFu) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

std::string DecodeXmlEntities(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '&') {
            size_t semi = sv.find(';', i + 1);
            if (semi != std::string_view::npos && semi - i <= 12) {
                std::string_view ent = sv.substr(i + 1, semi - i - 1);
                if (ent == "amp") { out.push_back('&'); i = semi; continue; }
                if (ent == "lt") { out.push_back('<'); i = semi; continue; }
                if (ent == "gt") { out.push_back('>'); i = semi; continue; }
                if (ent == "quot") { out.push_back('"'); i = semi; continue; }
                if (ent == "apos") { out.push_back('\''); i = semi; continue; }
                if (!ent.empty() && ent[0] == '#') {
                    uint32_t cp = 0;
                    bool valid = false;
                    if (ent.size() > 2 && (ent[1] == 'x' || ent[1] == 'X')) {
                        valid = true;
                        for (size_t k = 2; k < ent.size(); ++k) {
                            char c = ent[k];
                            cp <<= 4;
                            if (c >= '0' && c <= '9') cp |= static_cast<uint32_t>(c - '0');
                            else if (c >= 'a' && c <= 'f') cp |= static_cast<uint32_t>(10 + c - 'a');
                            else if (c >= 'A' && c <= 'F') cp |= static_cast<uint32_t>(10 + c - 'A');
                            else { valid = false; break; }
                        }
                    } else if (ent.size() > 1) {
                        valid = true;
                        for (size_t k = 1; k < ent.size(); ++k) {
                            if (ent[k] < '0' || ent[k] > '9') { valid = false; break; }
                            cp = cp * 10u + static_cast<uint32_t>(ent[k] - '0');
                        }
                    }
                    if (valid && cp > 0 && cp <= 0x10FFFFu) {
                        AppendUtf8Codepoint(cp, out);
                        i = semi;
                        continue;
                    }
                }
            }
        }
        out.push_back(sv[i]);
    }
    return out;
}

std::string ExtractXmlText(std::string_view xml) {
    std::vector<std::string> chunks;
    size_t i = 0;
    const size_t n = xml.size();
    while (i < n) {
        size_t lt = xml.find('<', i);
        if (lt == std::string_view::npos) {
            std::string t = TrimAscii(DecodeXmlEntities(xml.substr(i)));
            if (!t.empty()) chunks.push_back(std::move(t));
            break;
        }
        if (lt > i) {
            std::string t = TrimAscii(DecodeXmlEntities(xml.substr(i, lt - i)));
            if (!t.empty()) chunks.push_back(std::move(t));
        }
        if (xml.compare(lt, 9, "<![CDATA[") == 0) {
            size_t end_cd = xml.find("]]>", lt + 9);
            if (end_cd == std::string_view::npos) {
                std::string t = TrimAscii(xml.substr(lt + 9));
                if (!t.empty()) chunks.push_back(std::move(t));
                break;
            }
            std::string t = TrimAscii(xml.substr(lt + 9, end_cd - (lt + 9)));
            if (!t.empty()) chunks.push_back(std::move(t));
            i = end_cd + 3;
        } else if (xml.compare(lt, 4, "<!--") == 0) {
            size_t end_cm = xml.find("-->", lt + 4);
            if (end_cm == std::string_view::npos) break;
            i = end_cm + 3;
        } else if (xml.compare(lt, 2, "<?") == 0) {
            size_t end_pi = xml.find("?>", lt + 2);
            if (end_pi == std::string_view::npos) break;
            i = end_pi + 2;
        } else {
            size_t gt = xml.find('>', lt + 1);
            if (gt == std::string_view::npos) break;
            i = gt + 1;
        }
    }
    std::string joined;
    for (size_t k = 0; k < chunks.size(); ++k) {
        if (k > 0) joined.push_back(' ');
        joined += chunks[k];
    }
    return MaterialExtractor::CleanText(joined);
}

bool IsDigitsOnly(std::string_view sv) {
    if (sv.empty()) return false;
    for (char c : sv) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

enum class OoxmlRole { None, Docx, Pptx, Xlsx };

OoxmlRole ClassifyOoxmlEntry(std::string_view name) {
    if (name.rfind("word/", 0) == 0 && name.size() > 9 &&
        name.compare(name.size() - 4, 4, ".xml") == 0) {
        std::string_view stem = name.substr(5, name.size() - 9);
        if (stem == "document" || stem == "footnotes" || stem == "endnotes") {
            return OoxmlRole::Docx;
        }
        if (stem.rfind("header", 0) == 0 &&
            (stem.size() == 6 || IsDigitsOnly(stem.substr(6)))) {
            return OoxmlRole::Docx;
        }
        if (stem.rfind("footer", 0) == 0 &&
            (stem.size() == 6 || IsDigitsOnly(stem.substr(6)))) {
            return OoxmlRole::Docx;
        }
    }
    if ((name.rfind("ppt/slides/", 0) == 0 || name.rfind("ppt/notesSlides/", 0) == 0) &&
        name.size() > 4 && name.compare(name.size() - 4, 4, ".xml") == 0) {
        size_t slash = name.rfind('/');
        if (slash != std::string_view::npos && slash + 5 <= name.size()) {
            return OoxmlRole::Pptx;
        }
    }
    if (name == "xl/sharedStrings.xml") {
        return OoxmlRole::Xlsx;
    }
    if (name.rfind("xl/worksheets/sheet", 0) == 0 && name.size() > 23 &&
        name.compare(name.size() - 4, 4, ".xml") == 0) {
        std::string_view num = name.substr(19, name.size() - 23);
        if (IsDigitsOnly(num)) {
            return OoxmlRole::Xlsx;
        }
    }
    return OoxmlRole::None;
}

std::string PdfUnescapeLiteral(std::string_view raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ++i) {
        char c = raw[i];
        if (c != '\\') {
            out.push_back(c);
            continue;
        }
        if (i + 1 >= raw.size()) break;
        char n = raw[++i];
        switch (n) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case '(': out.push_back('('); break;
            case ')': out.push_back(')'); break;
            case '\\': out.push_back('\\'); break;
            case '\r':
                if (i + 1 < raw.size() && raw[i + 1] == '\n') ++i;
                break;
            case '\n':
                break;
            default:
                if (n >= '0' && n <= '7') {
                    int val = n - '0';
                    for (int k = 0; k < 2 && i + 1 < raw.size(); ++k) {
                        char oct = raw[i + 1];
                        if (oct >= '0' && oct <= '7') {
                            val = (val << 3) | (oct - '0');
                            ++i;
                        } else {
                            break;
                        }
                    }
                    out.push_back(static_cast<char>(val & 0xFF));
                } else {
                    out.push_back(n);
                }
                break;
        }
    }
    return out;
}

std::string BytesToLatin1OrUtf8(const std::vector<uint8_t>& bytes) {
    if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        std::string out;
        for (size_t i = 2; i + 1 < bytes.size(); i += 2) {
            uint16_t u = (static_cast<uint16_t>(bytes[i]) << 8) | bytes[i + 1];
            if (u >= 0xD800u && u <= 0xDBFFu && i + 3 < bytes.size()) {
                uint16_t lo = (static_cast<uint16_t>(bytes[i + 2]) << 8) | bytes[i + 3];
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    uint32_t cp = 0x10000u + (((u - 0xD800u) << 10) | (lo - 0xDC00u));
                    AppendUtf8Codepoint(cp, out);
                    i += 2;
                    continue;
                }
            }
            if (u >= 0x20u || u == '\n' || u == '\r' || u == '\t') {
                AppendUtf8Codepoint(u, out);
            }
        }
        return out;
    }
    if (!bytes.empty() && IsValidUtf8Bytes(bytes.data(), bytes.size())) {
        std::string out;
        out.reserve(bytes.size());
        for (uint8_t b : bytes) {
            if (b >= 0x20u || b == '\n' || b == '\r' || b == '\t') {
                out.push_back(static_cast<char>(b));
            }
        }
        return out;
    }
    std::string out;
    out.reserve(bytes.size());
    for (uint8_t b : bytes) {
        if (b >= 0x20 || b == '\n' || b == '\r' || b == '\t') {
            if (b < 0x80) {
                out.push_back(static_cast<char>(b));
            } else {
                AppendUtf8Codepoint(b, out);
            }
        }
    }
    return out;
}

std::string PdfDecodeHex(std::string_view hex_sv) {
    std::string clean;
    clean.reserve(hex_sv.size());
    for (char c : hex_sv) {
        if (std::isxdigit(static_cast<unsigned char>(c))) {
            clean.push_back(c);
        }
    }
    if (clean.empty()) return {};
    if ((clean.size() % 2) != 0) clean.push_back('0');
    auto hval = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
        if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + c - 'a');
        return static_cast<uint8_t>(10 + c - 'A');
    };
    std::vector<uint8_t> bytes;
    bytes.reserve(clean.size() / 2);
    for (size_t i = 0; i + 1 < clean.size(); i += 2) {
        bytes.push_back(static_cast<uint8_t>((hval(clean[i]) << 4) | hval(clean[i + 1])));
    }
    return BytesToLatin1OrUtf8(bytes);
}

bool IsPdfWhitespace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\0';
}

bool IsPdfDelimiter(char c) {
    return IsPdfWhitespace(c) || c == '(' || c == ')' || c == '<' || c == '>' ||
           c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%';
}

std::string ExtractPdfTextOperators(const uint8_t* data, size_t len) {
    std::string_view sv(reinterpret_cast<const char*>(data), len);
    std::vector<std::string> pieces;

    auto read_literal = [&](size_t& pos) -> std::string {
        // pos points at '('
        ++pos;
        size_t start = pos;
        int depth = 1;
        while (pos < sv.size() && depth > 0) {
            char c = sv[pos];
            if (c == '\\') {
                pos += (pos + 1 < sv.size()) ? 2 : 1;
            } else if (c == '(') {
                ++depth;
                ++pos;
            } else if (c == ')') {
                --depth;
                if (depth == 0) {
                    std::string raw_dec = PdfUnescapeLiteral(sv.substr(start, pos - start));
                    std::vector<uint8_t> raw_bytes(raw_dec.begin(), raw_dec.end());
                    std::string dec = BytesToLatin1OrUtf8(raw_bytes);
                    ++pos;
                    return dec;
                }
                ++pos;
            } else {
                ++pos;
            }
        }
        return {};
    };

    auto read_hex = [&](size_t& pos) -> std::string {
        // pos points at '<', caller verified next char != '<'
        ++pos;
        size_t start = pos;
        while (pos < sv.size() && sv[pos] != '>') {
            ++pos;
        }
        std::string dec = PdfDecodeHex(sv.substr(start, pos - start));
        if (pos < sv.size() && sv[pos] == '>') ++pos;
        return dec;
    };

    auto skip_ws_and_comments = [&](size_t& pos) {
        while (pos < sv.size()) {
            if (IsPdfWhitespace(sv[pos])) {
                ++pos;
            } else if (sv[pos] == '%') {
                while (pos < sv.size() && sv[pos] != '\n' && sv[pos] != '\r') ++pos;
            } else {
                break;
            }
        }
    };

    auto peek_operator = [&](size_t pos) -> std::pair<std::string_view, size_t> {
        skip_ws_and_comments(pos);
        if (pos >= sv.size()) return {{}, pos};
        if (sv[pos] == '\'' || sv[pos] == '"') {
            return {sv.substr(pos, 1), pos + 1};
        }
        size_t start = pos;
        while (pos < sv.size() && !IsPdfDelimiter(sv[pos])) ++pos;
        return {sv.substr(start, pos - start), pos};
    };

    size_t i = 0;
    while (i < sv.size()) {
        char c = sv[i];
        if (c == '%') {
            while (i < sv.size() && sv[i] != '\n' && sv[i] != '\r') ++i;
            continue;
        }
        if (c == '(') {
            std::string lit = read_literal(i);
            auto op = peek_operator(i);
            if (op.first == "Tj" || op.first == "'" || op.first == "\"") {
                if (!lit.empty()) pieces.push_back(std::move(lit));
                i = op.second;
            }
            continue;
        }
        if (c == '<') {
            if (i + 1 < sv.size() && sv[i + 1] == '<') {
                i += 2;
                continue;
            }
            std::string hx = read_hex(i);
            auto op = peek_operator(i);
            if (op.first == "Tj" || op.first == "'" || op.first == "\"") {
                if (!hx.empty()) pieces.push_back(std::move(hx));
                i = op.second;
            }
            continue;
        }
        if (c == '[') {
            ++i;
            std::string arr_text;
            while (i < sv.size()) {
                skip_ws_and_comments(i);
                if (i >= sv.size()) break;
                if (sv[i] == ']') {
                    ++i;
                    break;
                }
                if (sv[i] == '(') {
                    arr_text += read_literal(i);
                } else if (sv[i] == '<' && (i + 1 >= sv.size() || sv[i + 1] != '<')) {
                    arr_text += read_hex(i);
                } else {
                    ++i;
                }
            }
            auto op = peek_operator(i);
            if (op.first == "TJ") {
                if (!arr_text.empty()) pieces.push_back(std::move(arr_text));
                i = op.second;
            }
            continue;
        }
        ++i;
    }

    std::string joined;
    for (size_t k = 0; k < pieces.size(); ++k) {
        if (k > 0) joined.push_back(' ');
        joined += pieces[k];
    }
    return MaterialExtractor::CleanText(joined);
}

MaterialExtractResult ExtractPdfImpl(const uint8_t* data, size_t len, size_t max_entry_bytes) {
    MaterialExtractResult res;
    res.kind = MaterialKind::Pdf;
    res.kind_detail = "PDF";

    std::string_view sv(reinterpret_cast<const char*>(data), len);
    std::vector<std::string> stream_texts;

    size_t pos = 0;
    while (pos < sv.size()) {
        size_t s_pos = sv.find("stream", pos);
        if (s_pos == std::string_view::npos) break;
        if (s_pos > 0) {
            unsigned char prev = static_cast<unsigned char>(sv[s_pos - 1]);
            if (std::isalnum(prev) || prev == '_') {
                pos = s_pos + 6;
                continue;
            }
        }
        size_t body_start = s_pos + 6;
        if (body_start < sv.size() && sv[body_start] == '\r') {
            ++body_start;
            if (body_start < sv.size() && sv[body_start] == '\n') ++body_start;
        } else if (body_start < sv.size() && sv[body_start] == '\n') {
            ++body_start;
        } else {
            pos = s_pos + 6;
            continue;
        }

        size_t end_pos = sv.find("endstream", body_start);
        if (end_pos == std::string_view::npos) break;

        size_t body_end = end_pos;
        if (body_end > body_start && sv[body_end - 1] == '\n') --body_end;
        if (body_end > body_start && sv[body_end - 1] == '\r') --body_end;

        const uint8_t* payload = data + body_start;
        const size_t plen = body_end - body_start;

        auto inf = InflateCodec::InflateZlib(payload, plen, max_entry_bytes);
        std::string txt;
        if (inf.ok) {
            txt = ExtractPdfTextOperators(inf.value.data(), inf.value.size());
        } else {
            txt = ExtractPdfTextOperators(payload, plen);
        }
        if (!txt.empty()) {
            stream_texts.push_back(std::move(txt));
        }
        pos = end_pos + 9;
    }

    if (stream_texts.empty()) {
        std::string fallback = ExtractPdfTextOperators(data, len);
        if (!fallback.empty()) {
            stream_texts.push_back(std::move(fallback));
        }
    }

    if (!stream_texts.empty()) {
        std::string merged;
        for (size_t k = 0; k < stream_texts.size(); ++k) {
            if (k > 0) merged += "\n\n";
            merged += stream_texts[k];
        }
        res.status = ExtractStatus::Partial;
        res.text = MaterialExtractor::CleanText(merged);
        res.detail = "PDF: operadores de texto tras FlateDecode";
        return res;
    }

    res.status = ExtractStatus::Failed;
    res.detail = "PDF sin texto extraíble (escaneado u ofuscado)";
    return res;
}

MaterialExtractResult ExtractZipFamilyImpl(const uint8_t* data, size_t len, size_t max_entry_bytes) {
    MaterialExtractResult res;
    res.kind = MaterialKind::ZipFamily;
    res.kind_detail = "ZIP";

    auto entries_res = ZipReader::ListEntries(data, len);
    if (!entries_res.ok || entries_res.value.empty()) {
        res.status = ExtractStatus::Failed;
        res.detail = "ZIP corrupto o vacío: " + entries_res.error_message;
        return res;
    }

    const auto& entries = entries_res.value;
    std::vector<const ZipEntryInfo*> docx_targets;
    std::vector<const ZipEntryInfo*> pptx_targets;
    std::vector<const ZipEntryInfo*> xlsx_targets;

    for (const auto& e : entries) {
        switch (ClassifyOoxmlEntry(e.name)) {
            case OoxmlRole::Docx: docx_targets.push_back(&e); break;
            case OoxmlRole::Pptx: pptx_targets.push_back(&e); break;
            case OoxmlRole::Xlsx: xlsx_targets.push_back(&e); break;
            case OoxmlRole::None: break;
        }
    }

    std::vector<const ZipEntryInfo*> targets;
    if (!docx_targets.empty()) {
        res.kind_detail = "DOCX";
        targets = std::move(docx_targets);
    } else if (!pptx_targets.empty()) {
        res.kind_detail = "PPTX";
        targets = std::move(pptx_targets);
    } else if (!xlsx_targets.empty()) {
        res.kind_detail = "XLSX";
        targets = std::move(xlsx_targets);
    }

    if (targets.empty()) {
        std::string listing;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) listing.push_back('\n');
            listing += entries[i].name;
        }
        res.status = ExtractStatus::Partial;
        res.text = std::move(listing);
        res.detail = "zip genérico: " + std::to_string(entries.size()) + " entradas";
        return res;
    }

    std::sort(targets.begin(), targets.end(),
              [](const ZipEntryInfo* a, const ZipEntryInfo* b) { return a->name < b->name; });

    bool had_entry_error = false;
    std::vector<std::string> texts;
    for (const ZipEntryInfo* target : targets) {
        auto bytes_res = ZipReader::ExtractEntry(data, len, *target, max_entry_bytes);
        if (!bytes_res.ok) {
            had_entry_error = true;
            continue;
        }
        std::string_view xml_sv(reinterpret_cast<const char*>(bytes_res.value.data()),
                                bytes_res.value.size());
        std::string t = ExtractXmlText(xml_sv);
        if (!t.empty()) {
            texts.push_back(std::move(t));
        }
    }

    if (texts.empty()) {
        res.status = ExtractStatus::Failed;
        res.detail = res.kind_detail + " sin texto extraíble";
        return res;
    }

    std::string merged;
    for (size_t i = 0; i < texts.size(); ++i) {
        if (i > 0) merged += "\n\n";
        merged += texts[i];
    }
    res.status = had_entry_error ? ExtractStatus::Partial : ExtractStatus::Full;
    res.text = MaterialExtractor::CleanText(merged);
    res.detail = res.kind_detail + ": " + std::to_string(texts.size()) + " partes XML";
    return res;
}

MaterialExtractResult ExtractOleImpl(const uint8_t* data, size_t len) {
    MaterialExtractResult res;
    res.kind = MaterialKind::Ole;
    res.kind_detail = "OLE";

    std::vector<std::string> runs;
    // 1. ASCII runs >= 5
    {
        std::string cur;
        for (size_t i = 0; i < len; ++i) {
            uint8_t b = data[i];
            if (b >= 0x20 && b <= 0x7E) {
                cur.push_back(static_cast<char>(b));
            } else {
                if (cur.size() >= 5) runs.push_back(cur);
                cur.clear();
            }
        }
        if (cur.size() >= 5) runs.push_back(cur);
    }

    // 2. UTF-16-LE printable runs >= 5 (check both parities without overlapping)
    std::vector<bool> covered(len, false);
    for (size_t parity = 0; parity < 2; ++parity) {
        std::string cur;
        size_t run_start = 0;
        size_t unit_count = 0;
        for (size_t i = parity; i + 1 < len; i += 2) {
            uint8_t lo = data[i];
            uint8_t hi = data[i + 1];
            if (!covered[i] && hi == 0x00 && ((lo >= 0x20 && lo <= 0x7E) || lo >= 0xA0)) {
                if (unit_count == 0) run_start = i;
                AppendUtf8Codepoint(lo, cur);
                ++unit_count;
            } else {
                if (unit_count >= 5) {
                    runs.push_back(cur);
                    for (size_t k = run_start; k < i; ++k) covered[k] = true;
                }
                cur.clear();
                unit_count = 0;
            }
        }
        if (unit_count >= 5) {
            runs.push_back(cur);
            for (size_t k = run_start; k < len; ++k) covered[k] = true;
        }
    }

    if (runs.empty()) {
        res.status = ExtractStatus::Failed;
        res.detail = "OLE sin strings legibles";
        return res;
    }

    std::string merged;
    for (size_t i = 0; i < runs.size(); ++i) {
        if (i > 0) merged.push_back('\n');
        merged += runs[i];
    }
    std::string cleaned = MaterialExtractor::CleanText(merged);
    if (cleaned.empty()) {
        res.status = ExtractStatus::Failed;
        res.detail = "OLE sin strings legibles";
        return res;
    }
    res.status = ExtractStatus::Partial;
    res.text = std::move(cleaned);
    res.detail = "OLE binario: corridas de strings (extracción limitada)";
    return res;
}

std::string NormalizePlainUtf8(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(len);
    size_t i = 0;
    while (i < len) {
        uint8_t c = data[i];
        if (c < 0x80) {
            if (c != 0x00) out.push_back(static_cast<char>(c));
            ++i;
        } else if ((c & 0xE0u) == 0xC0u && i + 1 < len && (data[i + 1] & 0xC0u) == 0x80u) {
            out.push_back(static_cast<char>(data[i]));
            out.push_back(static_cast<char>(data[i + 1]));
            i += 2;
        } else if ((c & 0xF0u) == 0xE0u && i + 2 < len &&
                   (data[i + 1] & 0xC0u) == 0x80u && (data[i + 2] & 0xC0u) == 0x80u) {
            out.push_back(static_cast<char>(data[i]));
            out.push_back(static_cast<char>(data[i + 1]));
            out.push_back(static_cast<char>(data[i + 2]));
            i += 3;
        } else if ((c & 0xF8u) == 0xF0u && i + 3 < len &&
                   (data[i + 1] & 0xC0u) == 0x80u && (data[i + 2] & 0xC0u) == 0x80u &&
                   (data[i + 3] & 0xC0u) == 0x80u) {
            out.push_back(static_cast<char>(data[i]));
            out.push_back(static_cast<char>(data[i + 1]));
            out.push_back(static_cast<char>(data[i + 2]));
            out.push_back(static_cast<char>(data[i + 3]));
            i += 4;
        } else {
            // Latin-1 fallback to UTF-8
            AppendUtf8Codepoint(c, out);
            ++i;
        }
    }
    return MaterialExtractor::CleanText(out);
}

std::string FoldForSearch(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == 0xC3 && i + 1 < sv.size()) {
            unsigned char n = static_cast<unsigned char>(sv[i + 1]);
            char repl = 0;
            if (n == 0xA1 || n == 0x81) repl = 'a';       // á/Á
            else if (n == 0xA9 || n == 0x89) repl = 'e';  // é/É
            else if (n == 0xAD || n == 0x8D) repl = 'i';  // í/Í
            else if (n == 0xB3 || n == 0x93) repl = 'o';  // ó/Ó
            else if (n == 0xBA || n == 0x9A || n == 0xBC || n == 0x9C) repl = 'u';  // ú/Ú/ü/Ü
            else if (n == 0xB1 || n == 0x91) repl = 'n';  // ñ/Ñ
            if (repl != 0) {
                out.push_back(repl);
                ++i;
            } else {
                out.push_back(static_cast<char>(c));
            }
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::vector<std::string> SplitWhitespaceTokens(std::string_view sv) {
    std::vector<std::string> toks;
    size_t i = 0;
    while (i < sv.size()) {
        while (i < sv.size() && std::isspace(static_cast<unsigned char>(sv[i]))) ++i;
        if (i >= sv.size()) break;
        size_t j = i;
        while (j < sv.size() && !std::isspace(static_cast<unsigned char>(sv[j]))) ++j;
        toks.emplace_back(sv.substr(i, j - i));
        i = j;
    }
    return toks;
}

VoidResult WriteFileAtomicUtf8(const fs::path& target, const std::string& content) {
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    if (ec) {
        return VoidResult::Failure("mkdir_failed", ec.message());
    }
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            return VoidResult::Failure("open_failed", "cannot open temp file");
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        if (!ofs.good()) {
            return VoidResult::Failure("write_failed", "failed writing temp file");
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        std::error_code rm_ec;
        fs::remove(target, rm_ec);
        ec.clear();
        fs::rename(tmp, target, ec);
        if (ec) {
            fs::remove(tmp, rm_ec);
            return VoidResult::Failure("rename_failed", ec.message());
        }
    }
    return VoidResult::Success();
}

bool ReadFileBytes(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return false;
    ifs.seekg(0, std::ios::end);
    std::streamoff sz = ifs.tellg();
    if (sz < 0) return false;
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    if (sz > 0) {
        ifs.read(reinterpret_cast<char*>(out.data()), sz);
        if (!ifs) return false;
    }
    return true;
}

std::string MakeDocKey(const IndexedDocument& d) {
    if (!d.sha256.empty() && !d.object_id.empty()) return d.sha256 + "|" + d.object_id;
    if (!d.sha256.empty()) return d.sha256;
    return d.object_id + "|" + d.name;
}

bool SameLogicalDocument(const IndexedDocument& existing, const IndexedDocument& incoming) {
    if (!incoming.object_id.empty()) {
        return existing.object_id == incoming.object_id;
    }
    if (!incoming.sha256.empty()) {
        return existing.object_id.empty() &&
               (existing.sha256 == incoming.sha256 ||
                (existing.sha256.empty() && existing.name == incoming.name));
    }
    return existing.object_id.empty() && existing.name == incoming.name;
}

void EraseMismatchesForDoc(std::vector<MagicExtMismatch>& mismatches, const IndexedDocument& doc) {
    if (!doc.magic_ext_mismatch) return;
    mismatches.erase(
        std::remove_if(mismatches.begin(), mismatches.end(),
                       [&](const MagicExtMismatch& mm) {
                           if (!doc.object_id.empty()) {
                               return mm.object_id == doc.object_id;
                           }
                           return mm.object_id.empty() &&
                                  mm.name == doc.name &&
                                  mm.ext == doc.ext;
                       }),
        mismatches.end());
}

MaterialKind KindFromString(std::string_view s) {
    if (s == "pdf") return MaterialKind::Pdf;
    if (s == "zip-family") return MaterialKind::ZipFamily;
    if (s == "ole") return MaterialKind::Ole;
    if (s == "binary-media") return MaterialKind::BinaryMedia;
    if (s == "text") return MaterialKind::Text;
    return MaterialKind::Unknown;
}

ExtractStatus StatusFromString(std::string_view s) {
    if (s == "full") return ExtractStatus::Full;
    if (s == "partial") return ExtractStatus::Partial;
    if (s == "skipped") return ExtractStatus::Skipped;
    if (s == "missing") return ExtractStatus::MissingBlob;
    return ExtractStatus::Failed;
}

}  // namespace

Result<std::vector<uint8_t>> InflateCodec::InflateRawDeflate(const uint8_t* data,
                                                             size_t len,
                                                             size_t max_output_bytes) {
    return detail::InflateRawDeflateImpl(data, len, max_output_bytes);
}

Result<std::vector<uint8_t>> InflateCodec::InflateZlib(const uint8_t* data,
                                                       size_t len,
                                                       size_t max_output_bytes) {
    return detail::InflateZlibImpl(data, len, max_output_bytes);
}

Result<std::vector<uint8_t>> InflateCodec::InflateGzip(const uint8_t* data,
                                                       size_t len,
                                                       size_t max_output_bytes) {
    return detail::InflateGzipImpl(data, len, max_output_bytes);
}

Result<std::vector<ZipEntryInfo>> ZipReader::ListEntries(const uint8_t* data, size_t len) {
    if (data == nullptr || len < 22) {
        return Result<std::vector<ZipEntryInfo>>::Failure("invalid_zip", "buffer too small for ZIP");
    }

    // Locate EOCD (PK\x05\x06) searching backwards up to 65557 bytes
    const size_t min_off = (len > 65557) ? (len - 65557) : 0;
    size_t eocd = std::string::npos;
    for (size_t pos = len - 22;; --pos) {
        if (data[pos] == 0x50 && data[pos + 1] == 0x4B &&
            data[pos + 2] == 0x05 && data[pos + 3] == 0x06) {
            const uint16_t comment_len = ReadU16LE(data + pos + 20);
            if (pos + 22 + comment_len <= len) {
                eocd = pos;
                break;
            }
        }
        if (pos == min_off) break;
    }

    std::vector<ZipEntryInfo> entries;
    if (eocd != std::string::npos) {
        const uint16_t total_entries = ReadU16LE(data + eocd + 10);
        const uint32_t cd_size = ReadU32LE(data + eocd + 12);
        const uint32_t cd_offset = ReadU32LE(data + eocd + 16);
        if (static_cast<uint64_t>(cd_offset) + cd_size > len) {
            return Result<std::vector<ZipEntryInfo>>::Failure(
                "invalid_zip_cd", "ZIP central directory out of bounds");
        }

        size_t pos = cd_offset;
        const size_t cd_end = static_cast<size_t>(cd_offset) + cd_size;
        for (uint16_t idx = 0; idx < total_entries && pos + 46 <= cd_end; ++idx) {
            if (ReadU32LE(data + pos) != 0x02014B50u) {
                return Result<std::vector<ZipEntryInfo>>::Failure(
                    "invalid_zip_cd", "missing central directory entry signature");
            }
            ZipEntryInfo info;
            info.flags = ReadU16LE(data + pos + 8);
            info.method = ReadU16LE(data + pos + 10);
            info.crc32 = ReadU32LE(data + pos + 16);
            info.compressed_size = ReadU32LE(data + pos + 20);
            info.uncompressed_size = ReadU32LE(data + pos + 24);
            const uint16_t name_len = ReadU16LE(data + pos + 28);
            const uint16_t extra_len = ReadU16LE(data + pos + 30);
            const uint16_t comment_len = ReadU16LE(data + pos + 32);
            info.local_header_offset = ReadU32LE(data + pos + 42);

            if (pos + 46 + name_len + extra_len + comment_len > cd_end) {
                return Result<std::vector<ZipEntryInfo>>::Failure(
                    "invalid_zip_cd", "ZIP central directory entry truncated");
            }
            info.name.assign(reinterpret_cast<const char*>(data + pos + 46), name_len);
            entries.push_back(std::move(info));
            pos += 46 + name_len + extra_len + comment_len;
        }
        return Result<std::vector<ZipEntryInfo>>::Success(std::move(entries));
    }

    // Fallback: sequential LFH scan (only when bit-3 data descriptor flag is clear)
    size_t pos = 0;
    while (pos + 30 <= len && ReadU32LE(data + pos) == 0x04034B50u) {
        ZipEntryInfo info;
        info.flags = ReadU16LE(data + pos + 6);
        info.method = ReadU16LE(data + pos + 8);
        info.crc32 = ReadU32LE(data + pos + 14);
        info.compressed_size = ReadU32LE(data + pos + 18);
        info.uncompressed_size = ReadU32LE(data + pos + 22);
        const uint16_t name_len = ReadU16LE(data + pos + 26);
        const uint16_t extra_len = ReadU16LE(data + pos + 28);
        info.local_header_offset = static_cast<uint32_t>(pos);
        if ((info.flags & 0x0008u) != 0 ||
            pos + 30 + name_len + extra_len + info.compressed_size > len) {
            break;
        }
        info.name.assign(reinterpret_cast<const char*>(data + pos + 30), name_len);
        entries.push_back(std::move(info));
        pos += 30 + name_len + extra_len + entries.back().compressed_size;
    }

    if (entries.empty()) {
        return Result<std::vector<ZipEntryInfo>>::Failure("invalid_zip", "EOCD record not found");
    }
    return Result<std::vector<ZipEntryInfo>>::Success(std::move(entries));
}

Result<std::vector<uint8_t>> ZipReader::ExtractEntry(const uint8_t* data,
                                                     size_t len,
                                                     const ZipEntryInfo& entry,
                                                     size_t max_entry_bytes) {
    if (data == nullptr || static_cast<uint64_t>(entry.local_header_offset) + 30 > len) {
        return Result<std::vector<uint8_t>>::Failure("invalid_lfh", "local header out of bounds");
    }
    const size_t lfh = entry.local_header_offset;
    if (ReadU32LE(data + lfh) != 0x04034B50u) {
        return Result<std::vector<uint8_t>>::Failure("invalid_lfh", "missing local header signature");
    }
    const uint16_t lfh_name_len = ReadU16LE(data + lfh + 26);
    const uint16_t lfh_extra_len = ReadU16LE(data + lfh + 28);
    const uint64_t data_off = static_cast<uint64_t>(lfh) + 30 + lfh_name_len + lfh_extra_len;
    if (data_off + entry.compressed_size > len) {
        return Result<std::vector<uint8_t>>::Failure("truncated_entry",
                                                    "ZIP entry data exceeds buffer");
    }

    const uint8_t* comp_ptr = data + data_off;
    if (entry.method == 0) {
        if (entry.compressed_size != entry.uncompressed_size ||
            entry.uncompressed_size > max_entry_bytes) {
            return Result<std::vector<uint8_t>>::Failure("invalid_stored_entry",
                                                        "invalid stored entry size");
        }
        return Result<std::vector<uint8_t>>::Success(
            std::vector<uint8_t>(comp_ptr, comp_ptr + entry.compressed_size));
    }
    if (entry.method == 8) {
        if (entry.uncompressed_size > max_entry_bytes) {
            return Result<std::vector<uint8_t>>::Failure("output_limit_exceeded",
                                                        "ZIP entry exceeds output limit");
        }
        return InflateCodec::InflateRawDeflate(comp_ptr, entry.compressed_size, max_entry_bytes);
    }
    return Result<std::vector<uint8_t>>::Failure(
        "unsupported_method", "unsupported ZIP compression method " + std::to_string(entry.method));
}

MaterialKind MaterialExtractor::DetectFormat(std::string_view filename,
                                             const uint8_t* prefix,
                                             size_t prefix_len) {
    static constexpr uint8_t kPdf[4] = {'%', 'P', 'D', 'F'};
    static constexpr uint8_t kPk34[4] = {'P', 'K', 0x03, 0x04};
    static constexpr uint8_t kPk56[4] = {'P', 'K', 0x05, 0x06};
    static constexpr uint8_t kOle[4] = {0xD0, 0xCF, 0x11, 0xE0};

    if (StartsWithBytes(prefix, prefix_len, kPdf, 4)) return MaterialKind::Pdf;
    if (StartsWithBytes(prefix, prefix_len, kPk34, 4) ||
        StartsWithBytes(prefix, prefix_len, kPk56, 4)) {
        return MaterialKind::ZipFamily;
    }
    if (StartsWithBytes(prefix, prefix_len, kOle, 4)) return MaterialKind::Ole;

    std::string ext = LowerExtension(filename);
    if (!ext.empty()) {
        if (IsSkipExtension(ext)) return MaterialKind::BinaryMedia;
        if (IsTextExtension(ext)) return MaterialKind::Text;
    }
    return MaterialKind::Unknown;
}

std::string MaterialExtractor::KindToString(MaterialKind kind) {
    switch (kind) {
        case MaterialKind::Pdf: return "pdf";
        case MaterialKind::ZipFamily: return "zip-family";
        case MaterialKind::Ole: return "ole";
        case MaterialKind::BinaryMedia: return "binary-media";
        case MaterialKind::Text: return "text";
        case MaterialKind::Unknown: return "unknown";
    }
    return "unknown";
}

std::string MaterialExtractor::StatusToString(ExtractStatus status) {
    switch (status) {
        case ExtractStatus::Full: return "full";
        case ExtractStatus::Partial: return "partial";
        case ExtractStatus::Skipped: return "skipped";
        case ExtractStatus::Failed: return "failed";
        case ExtractStatus::MissingBlob: return "missing";
    }
    return "failed";
}

std::optional<std::string> MaterialExtractor::ExpectedKindForExtension(std::string_view ext_raw) {
    std::string ext = AsciiLower(ext_raw);
    if (ext == ".pdf") return "pdf";
    if (ext == ".docx" || ext == ".pptx" || ext == ".xlsx" ||
        ext == ".odp" || ext == ".odt" || ext == ".ods" || ext == ".zip") {
        return "zip-family";
    }
    if (ext == ".doc" || ext == ".ppt" || ext == ".xls") return "ole";
    return std::nullopt;
}

std::string MaterialExtractor::CleanText(std::string_view text) {
    std::string collapsed;
    collapsed.reserve(text.size());
    bool in_hspace = false;
    int consecutive_nl = 0;

    for (char ch : text) {
        if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\v') {
            if (consecutive_nl == 0) in_hspace = true;
        } else if (ch == '\n') {
            in_hspace = false;
            if (consecutive_nl < 2) {
                collapsed.push_back('\n');
                ++consecutive_nl;
            }
        } else {
            if (in_hspace && !collapsed.empty() && collapsed.back() != '\n') {
                collapsed.push_back(' ');
            }
            in_hspace = false;
            consecutive_nl = 0;
            collapsed.push_back(ch);
        }
    }
    return TrimAscii(collapsed);
}

MaterialExtractResult MaterialExtractor::Extract(std::string_view filename,
                                                 const uint8_t* data,
                                                 size_t len,
                                                 size_t max_entry_bytes) {
    MaterialKind kind = DetectFormat(filename, data, len);
    switch (kind) {
        case MaterialKind::Pdf:
            return ExtractPdfImpl(data, len, max_entry_bytes);
        case MaterialKind::ZipFamily:
            return ExtractZipFamilyImpl(data, len, max_entry_bytes);
        case MaterialKind::Ole:
            return ExtractOleImpl(data, len);
        case MaterialKind::BinaryMedia: {
            MaterialExtractResult r;
            r.kind = MaterialKind::BinaryMedia;
            r.kind_detail = "media";
            r.status = ExtractStatus::Skipped;
            r.detail = "archivo multimedia/binario omitido";
            return r;
        }
        case MaterialKind::Text: {
            MaterialExtractResult r;
            r.kind = MaterialKind::Text;
            r.kind_detail = "TEXT";
            std::string txt = NormalizePlainUtf8(data, len);
            r.status = ExtractStatus::Full;
            r.text = std::move(txt);
            r.detail = "texto plano";
            return r;
        }
        case MaterialKind::Unknown:
        default: {
            MaterialExtractResult r;
            r.kind = MaterialKind::Unknown;
            r.kind_detail = "UNKNOWN";
            if (data != nullptr && len > 0) {
                size_t printable = 0;
                for (size_t i = 0; i < len; ++i) {
                    uint8_t b = data[i];
                    if ((b >= 0x20 && b <= 0x7E) || b == '\n' || b == '\r' || b == '\t' || b >= 0x80) {
                        ++printable;
                    }
                }
                if (printable * 100 >= len * 85) {
                    std::string txt = NormalizePlainUtf8(data, len);
                    if (!txt.empty()) {
                        r.status = ExtractStatus::Partial;
                        r.text = std::move(txt);
                        r.detail = "texto heurístico";
                        return r;
                    }
                }
            }
            r.status = ExtractStatus::Failed;
            r.detail = "formato no soportado";
            return r;
        }
    }
}

MaterialExtractResult MaterialExtractor::Extract(std::string_view filename,
                                                 const std::vector<uint8_t>& bytes,
                                                 size_t max_entry_bytes) {
    return Extract(filename, bytes.data(), bytes.size(), max_entry_bytes);
}

OfflineMaterialIndex::OfflineMaterialIndex(std::string offline_root,
                                           SanitizeOptions sanitize_opts)
    : offline_root_(std::move(offline_root)), sanitizer_(std::move(sanitize_opts)) {}

void OfflineMaterialIndex::SetOfflineRoot(std::string offline_root) {
    offline_root_ = std::move(offline_root);
}

void OfflineMaterialIndex::SetSanitizeOptions(SanitizeOptions sanitize_opts) {
    Sanitizer next(std::move(sanitize_opts));
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    sanitizer_ = std::move(next);
}

Sanitizer OfflineMaterialIndex::SnapshotSanitizer() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return sanitizer_;
}

VoidResult OfflineMaterialIndex::IndexBlob(const IndexedBlobInput& input) {
    IndexedDocument doc;
    doc.object_id = input.object_id;
    doc.name = input.name;
    doc.ext = LowerExtension(input.name);
    doc.community = input.community;
    doc.course = input.course;
    doc.folder_path = input.folder_path;
    doc.sha256 = AsciiLower(input.sha256);
    doc.blob_rel_path = input.blob_rel_path;
    doc.size_bytes = input.size_bytes;

    std::vector<uint8_t> raw_bytes = input.inline_bytes;
    if (raw_bytes.empty()) {
        fs::path resolved;
        if (!offline_root_.empty() && !input.blob_rel_path.empty()) {
            resolved = fs::u8path(offline_root_) / fs::u8path(input.blob_rel_path);
        } else if (!offline_root_.empty() && doc.sha256.size() >= 3) {
            resolved = fs::u8path(offline_root_) / "downloads" / "_blobs" /
                       doc.sha256.substr(0, 2) / doc.sha256;
            if (!fs::exists(resolved)) {
                resolved = fs::u8path(offline_root_) / "_blobs" /
                           doc.sha256.substr(0, 2) / doc.sha256;
            }
        } else if (!input.blob_rel_path.empty()) {
            resolved = fs::u8path(input.blob_rel_path);
        }

        if (resolved.empty() || !fs::is_regular_file(resolved) || !ReadFileBytes(resolved, raw_bytes)) {
            doc.status = ExtractStatus::MissingBlob;
            doc.detail = "blob no encontrado en disco";
            const std::string key = MakeDocKey(doc);
            auto it = std::find_if(documents_.begin(), documents_.end(),
                                   [&](const IndexedDocument& existing) {
                                       return SameLogicalDocument(existing, doc);
                                   });
            if (it != documents_.end()) {
                const std::string old_key = MakeDocKey(*it);
                if (it->status != ExtractStatus::MissingBlob) {
                    ++missing_blobs_count_;
                }
                EraseMismatchesForDoc(mismatches_, *it);
                text_by_doc_key_.erase(old_key);
                if (key != old_key) {
                    text_by_doc_key_.erase(key);
                }
                *it = std::move(doc);
            } else {
                ++missing_blobs_count_;
                documents_.push_back(std::move(doc));
            }
            return VoidResult::Success();
        }
    }

    if (doc.sha256.empty()) {
        doc.sha256 = Sha256Hex(raw_bytes.data(), raw_bytes.size());
    }
    if (doc.size_bytes == 0) {
        doc.size_bytes = raw_bytes.size();
    }

    MaterialExtractResult ex = MaterialExtractor::Extract(doc.name, raw_bytes);
    doc.magic_kind = ex.kind;
    doc.kind_detail = ex.kind_detail;
    doc.status = ex.status;
    doc.detail = ex.detail;

    std::optional<MagicExtMismatch> new_mismatch;
    if (auto expected = MaterialExtractor::ExpectedKindForExtension(doc.ext)) {
        std::string actual = MaterialExtractor::KindToString(doc.magic_kind);
        if (*expected != actual) {
            doc.magic_ext_mismatch = true;
            new_mismatch = MagicExtMismatch{doc.object_id, doc.name, doc.ext, *expected, actual};
        }
    }

    const std::string key = MakeDocKey(doc);
    if (ex.text.has_value() && !ex.text->empty()) {
        const Sanitizer san = SnapshotSanitizer();
        std::string safe_text = san.SanitizeText(*ex.text);
        doc.chars_extracted = safe_text.size();
        std::string sha16 = doc.sha256.size() >= 16 ? doc.sha256.substr(0, 16) : doc.sha256;
        doc.text_rel_path = "material_index/text/" + sha16 + ".txt";
        text_by_doc_key_[key] = safe_text;

        if (!offline_root_.empty()) {
            fs::path out_txt = fs::u8path(offline_root_) / fs::u8path(doc.text_rel_path);
            auto wr = WriteFileAtomicUtf8(out_txt, safe_text);
            if (!wr.ok) {
                return wr;
            }
        }
    } else {
        text_by_doc_key_.erase(key);
    }

    // Deduplicate existing document entry, missing_blobs_count_, mismatches_, and text_by_doc_key_ on re-index
    auto it = std::find_if(documents_.begin(), documents_.end(),
                           [&](const IndexedDocument& existing) {
                               return SameLogicalDocument(existing, doc);
                           });
    if (it != documents_.end()) {
        const std::string old_key = MakeDocKey(*it);
        if (old_key != key) {
            text_by_doc_key_.erase(old_key);
        }
        if (it->status == ExtractStatus::MissingBlob && missing_blobs_count_ > 0) {
            --missing_blobs_count_;
        }
        EraseMismatchesForDoc(mismatches_, *it);
        *it = std::move(doc);
    } else {
        documents_.push_back(std::move(doc));
    }
    if (new_mismatch.has_value()) {
        mismatches_.push_back(std::move(*new_mismatch));
    }
    return VoidResult::Success();
}

Result<OfflineMaterialStats> OfflineMaterialIndex::IndexDirectory(const std::string& offline_root) {
    if (!offline_root.empty()) {
        offline_root_ = offline_root;
    }
    if (offline_root_.empty()) {
        return Result<OfflineMaterialStats>::Failure("empty_root", "offline_root is empty");
    }

    documents_.clear();
    text_by_doc_key_.clear();
    missing_blobs_count_ = 0;
    mismatches_.clear();

    const fs::path root = fs::u8path(offline_root_);
    std::set<std::string> seen_shas;

    // 1. Check files_manifest.json (root or downloads/)
    fs::path manifest_path = root / "files_manifest.json";
    if (!fs::is_regular_file(manifest_path)) {
        manifest_path = root / "downloads" / "files_manifest.json";
    }
    if (fs::is_regular_file(manifest_path)) {
        std::vector<uint8_t> mbytes;
        if (ReadFileBytes(manifest_path, mbytes)) {
            try {
                auto j = nlohmann::json::parse(mbytes.begin(), mbytes.end());
                nlohmann::json arr = nlohmann::json::array();
                if (j.is_array()) arr = j;
                else if (j.is_object() && j.contains("files") && j["files"].is_array()) {
                    arr = j["files"];
                }
                for (const auto& item : arr) {
                    if (!item.is_object()) continue;
                    IndexedBlobInput in;
                    in.object_id = item.value("object_id", "");
                    in.name = item.value("name", item.value("filename", ""));
                    in.community = item.value("community", "");
                    in.course = item.value("course", "");
                    in.folder_path = item.value("folder_path", "");
                    in.sha256 = item.value("sha256", "");
                    in.blob_rel_path = item.value("blob_rel_path", "");
                    in.logical_rel_path = item.value("logical_rel_path", "");
                    in.size_bytes = item.value("size_bytes", uint64_t{0});
                    if (in.blob_rel_path.empty() && in.sha256.size() >= 3) {
                        in.blob_rel_path = "downloads/_blobs/" + in.sha256.substr(0, 2) + "/" + in.sha256;
                    }
                    if (!in.sha256.empty()) seen_shas.insert(AsciiLower(in.sha256));
                    auto r = IndexBlob(in);
                    if (!r.ok) {
                        return Result<OfflineMaterialStats>::Failure(r.error_code, r.error_message);
                    }
                }
            } catch (const std::exception& ex) {
                return Result<OfflineMaterialStats>::Failure("invalid_manifest", ex.what());
            }
        }
    }

    // 2. Scan downloads/_blobs/ (or _blobs/) for any unmanifested blobs
    fs::path blobs_dir = root / "downloads" / "_blobs";
    if (!fs::is_directory(blobs_dir)) {
        blobs_dir = root / "_blobs";
    }
    if (fs::is_directory(blobs_dir)) {
        std::error_code ec;
        for (fs::recursive_directory_iterator it(blobs_dir, ec), end; it != end && !ec;
             it.increment(ec)) {
            if (!it->is_regular_file(ec)) continue;
            std::string fname = it->path().filename().u8string();
            if (fname.size() >= 5 && fname.compare(fname.size() - 5, 5, ".part") == 0) continue;
            if (fname.size() >= 4 && fname.compare(fname.size() - 4, 4, ".bad") == 0) continue;
            if (seen_shas.count(AsciiLower(fname)) > 0) continue;

            IndexedBlobInput in;
            in.object_id = "blob:" + fname.substr(0, std::min<size_t>(fname.size(), 16));
            in.name = fname;
            in.sha256 = fname;
            in.blob_rel_path = fs::relative(it->path(), root, ec).generic_u8string();
            auto r = IndexBlob(in);
            if (!r.ok) {
                return Result<OfflineMaterialStats>::Failure(r.error_code, r.error_message);
            }
        }
    }

    auto save_res = SaveToDisk(offline_root_);
    if (!save_res.ok) {
        return Result<OfflineMaterialStats>::Failure(save_res.error_code, save_res.error_message);
    }
    return Result<OfflineMaterialStats>::Success(GetOfflineStats());
}

VoidResult OfflineMaterialIndex::SaveToDisk(const std::string& offline_root) const {
    std::string target_root = offline_root.empty() ? offline_root_ : offline_root;
    if (target_root.empty()) {
        return VoidResult::Failure("empty_root", "offline_root is empty");
    }

    const Sanitizer san = SnapshotSanitizer();
    const fs::path idx_dir = fs::u8path(target_root) / "material_index";
    nlohmann::json docs_j = nlohmann::json::array();
    for (const auto& d : documents_) {
        docs_j.push_back({
            {"object_id", d.object_id},
            {"name", san.SanitizeText(d.name)},
            {"ext", d.ext},
            {"community", san.SanitizeText(d.community)},
            {"course", san.SanitizeText(d.course)},
            {"folder_path", san.SanitizeText(d.folder_path)},
            {"sha256", d.sha256},
            {"blob_rel_path", d.blob_rel_path},
            {"text_rel_path", d.text_rel_path},
            {"magic_kind", MaterialExtractor::KindToString(d.magic_kind)},
            {"kind_detail", d.kind_detail},
            {"status", MaterialExtractor::StatusToString(d.status)},
            {"detail", san.SanitizeText(d.detail)},
            {"chars_extracted", d.chars_extracted},
            {"size_bytes", d.size_bytes},
            {"magic_ext_mismatch", d.magic_ext_mismatch}
        });
    }

    auto wr_docs = WriteFileAtomicUtf8(idx_dir / "documents.json", docs_j.dump(2));
    if (!wr_docs.ok) return wr_docs;

    OfflineMaterialStats st = GetOfflineStats();
    nlohmann::json stats_j = {
        {"generated_at", st.generated_at_utc},
        {"total", st.total},
        {"missing_blobs", st.missing_blobs},
        {"by_status", st.by_status},
        {"by_ext", st.by_ext},
        {"by_magic_kind", st.by_magic_kind},
        {"chars_extracted_total", st.chars_extracted_total},
        {"extracted_chars", st.chars_extracted_total},
        {"magic_vs_ext_mismatches", nlohmann::json::array()}
    };
    for (const auto& mm : st.magic_vs_ext_mismatches) {
        stats_j["magic_vs_ext_mismatches"].push_back({
            {"object_id", mm.object_id},
            {"name", san.SanitizeText(mm.name)},
            {"ext", mm.ext},
            {"expected_kind", mm.expected_kind},
            {"actual_kind", mm.actual_kind}
        });
    }
    return WriteFileAtomicUtf8(idx_dir / "stats.json", stats_j.dump(2));
}

VoidResult OfflineMaterialIndex::LoadFromDisk(const std::string& offline_root) {
    std::string target_root = offline_root.empty() ? offline_root_ : offline_root;
    if (target_root.empty()) {
        return VoidResult::Failure("empty_root", "offline_root is empty");
    }
    offline_root_ = target_root;

    const fs::path root = fs::u8path(target_root);
    const fs::path docs_path = root / "material_index" / "documents.json";
    if (!fs::is_regular_file(docs_path)) {
        return VoidResult::Failure("not_found", "material_index/documents.json not found");
    }

    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(docs_path, bytes)) {
        return VoidResult::Failure("read_failed", "failed reading documents.json");
    }

    try {
        auto j = nlohmann::json::parse(bytes.begin(), bytes.end());
        if (!j.is_array()) {
            return VoidResult::Failure("invalid_json", "documents.json is not an array");
        }
        documents_.clear();
        text_by_doc_key_.clear();
        missing_blobs_count_ = 0;
        mismatches_.clear();

        for (const auto& item : j) {
            if (!item.is_object()) continue;
            IndexedDocument d;
            d.object_id = item.value("object_id", "");
            d.name = item.value("name", "");
            d.ext = item.value("ext", "");
            d.community = item.value("community", "");
            d.course = item.value("course", "");
            d.folder_path = item.value("folder_path", "");
            d.sha256 = item.value("sha256", "");
            d.blob_rel_path = item.value("blob_rel_path", "");
            d.text_rel_path = item.value("text_rel_path", "");
            d.magic_kind = KindFromString(item.value("magic_kind", "unknown"));
            d.kind_detail = item.value("kind_detail", "");
            d.status = StatusFromString(item.value("status", "failed"));
            d.detail = item.value("detail", "");
            d.chars_extracted = item.value("chars_extracted", size_t{0});
            d.size_bytes = item.value("size_bytes", uint64_t{0});
            d.magic_ext_mismatch = item.value("magic_ext_mismatch", false);
            if (d.status == ExtractStatus::MissingBlob) {
                ++missing_blobs_count_;
            }
            if (d.magic_ext_mismatch) {
                if (auto exp = MaterialExtractor::ExpectedKindForExtension(d.ext)) {
                    mismatches_.push_back({
                        d.object_id, d.name, d.ext, *exp,
                        MaterialExtractor::KindToString(d.magic_kind)
                    });
                }
            }
            if (!d.text_rel_path.empty()) {
                std::vector<uint8_t> tbytes;
                if (ReadFileBytes(root / fs::u8path(d.text_rel_path), tbytes)) {
                    text_by_doc_key_[MakeDocKey(d)] =
                        std::string(reinterpret_cast<const char*>(tbytes.data()), tbytes.size());
                }
            }
            documents_.push_back(std::move(d));
        }
        return VoidResult::Success();
    } catch (const std::exception& ex) {
        return VoidResult::Failure("invalid_json", ex.what());
    }
}

std::vector<MaterialSearchHit> OfflineMaterialIndex::SearchOfflineMaterials(
    std::string_view query,
    const MaterialSearchFilters& filters) const {
    const std::string q_folded = FoldForSearch(query);
    const std::vector<std::string> tokens = SplitWhitespaceTokens(q_folded);

    const std::string f_comm = AsciiLower(filters.community);
    const std::string f_course = AsciiLower(filters.course);
    const std::string f_ext = AsciiLower(filters.ext);

    std::vector<MaterialSearchHit> hits;
    for (const auto& doc : documents_) {
        if (!f_comm.empty() && AsciiLower(doc.community).find(f_comm) == std::string::npos) continue;
        if (!f_course.empty() && AsciiLower(doc.course).find(f_course) == std::string::npos) continue;
        if (!f_ext.empty() && AsciiLower(doc.ext) != f_ext) continue;
        if (filters.kind.has_value() && doc.magic_kind != *filters.kind) continue;
        if (filters.status.has_value() && doc.status != *filters.status) continue;

        const std::string meta_hay =
            FoldForSearch(doc.name + " " + doc.folder_path + " " + doc.community + " " + doc.course);

        std::string body_raw;
        auto tit = text_by_doc_key_.find(MakeDocKey(doc));
        if (tit != text_by_doc_key_.end()) {
            body_raw = tit->second;
        } else if (!offline_root_.empty() && !doc.text_rel_path.empty()) {
            std::vector<uint8_t> tb;
            if (ReadFileBytes(fs::u8path(offline_root_) / fs::u8path(doc.text_rel_path), tb)) {
                body_raw.assign(reinterpret_cast<const char*>(tb.data()), tb.size());
            }
        }
        const std::string body_folded = FoldForSearch(body_raw);

        int score = 0;
        bool all_match = true;
        size_t first_body_pos = std::string::npos;

        if (tokens.empty()) {
            score = 1;
        } else {
            for (const auto& tok : tokens) {
                bool in_meta = (meta_hay.find(tok) != std::string::npos);
                size_t bpos = body_folded.find(tok);
                bool in_body = (bpos != std::string::npos);
                if (!in_meta && !in_body) {
                    all_match = false;
                    break;
                }
                if (in_meta) score += 10;
                if (in_body) {
                    score += 3;
                    if (first_body_pos == std::string::npos || bpos < first_body_pos) {
                        first_body_pos = bpos;
                    }
                    // Count up to 5 extra occurrences
                    size_t scan = bpos + tok.size();
                    for (int extra = 0; extra < 5; ++extra) {
                        size_t nxt = body_folded.find(tok, scan);
                        if (nxt == std::string::npos) break;
                        ++score;
                        scan = nxt + tok.size();
                    }
                }
            }
        }

        if (!all_match) continue;

        MaterialSearchHit hit;
        hit.document = doc;
        hit.score = score;
        if (!body_raw.empty()) {
            size_t center = (first_body_pos != std::string::npos && first_body_pos < body_raw.size())
                                ? first_body_pos
                                : 0;
            size_t start = (center > 60) ? (center - 60) : 0;
            // Advance start past partial UTF-8 continuation bytes
            while (start < body_raw.size() &&
                   (static_cast<unsigned char>(body_raw[start]) & 0xC0u) == 0x80u) {
                ++start;
            }
            size_t end = std::min(body_raw.size(), start + 160);
            while (end < body_raw.size() &&
                   (static_cast<unsigned char>(body_raw[end]) & 0xC0u) == 0x80u) {
                ++end;
            }
            std::string snip = body_raw.substr(start, end - start);
            if (start > 0) snip = "..." + snip;
            if (end < body_raw.size()) snip += "...";
            hit.snippet = SnapshotSanitizer().SanitizeText(snip);
        }
        hits.push_back(std::move(hit));
    }

    std::sort(hits.begin(), hits.end(),
              [](const MaterialSearchHit& a, const MaterialSearchHit& b) {
                  if (a.score != b.score) return a.score > b.score;
                  return a.document.name < b.document.name;
              });

    if (filters.max_results > 0 && hits.size() > filters.max_results) {
        hits.resize(filters.max_results);
    }
    return hits;
}

OfflineMaterialStats OfflineMaterialIndex::GetOfflineStats() const {
    OfflineMaterialStats st;
    st.generated_at_utc = CredentialVault::CurrentUtcIso8601();
    st.total = documents_.size();
    st.missing_blobs = missing_blobs_count_;
    st.magic_vs_ext_mismatches = mismatches_;
    for (const auto& d : documents_) {
        ++st.by_status[MaterialExtractor::StatusToString(d.status)];
        std::string ext_key = d.ext.empty() ? "(none)" : d.ext;
        ++st.by_ext[ext_key];
        ++st.by_magic_kind[MaterialExtractor::KindToString(d.magic_kind)];
        st.chars_extracted_total += d.chars_extracted;
    }
    return st;
}

}  // namespace academic::tec_digital
