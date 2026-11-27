#include "knowledge_graph.hpp"

#include "../app/studio_command_ids.hpp"
#include "cudoc_storage.hpp"
#include "ink_engine.hpp"
#include "utf_conv.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <unordered_set>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio {

namespace {

std::string Trim(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::string ReplaceAllCopy(std::string s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string EscapeHtml(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char ch : s) {
        if (ch == '&') out += "&amp;";
        else if (ch == '<') out += "&lt;";
        else if (ch == '>') out += "&gt;";
        else if (ch == '"') out += "&quot;";
        else out.push_back(ch);
    }
    return out;
}

std::string SanitizeSlug(const std::string& raw) {
    std::string out;
    for (unsigned char ch : raw) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == ' ') {
            out.push_back(ch == ' ' ? '_' : static_cast<char>(ch));
        } else if (ch >= 0x80u) {
            out.push_back('_');
        }
    }
    return out.empty() ? "nota" : out;
}

bool EqualsFolded(const std::string& a, const std::string& b) {
    return FoldUtf8ForSearch(Trim(a), false, true) == FoldUtf8ForSearch(Trim(b), false, true);
}

bool ContainsFolded(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    return FoldUtf8ForSearch(haystack, false, true).find(FoldUtf8ForSearch(needle, false, true)) != std::string::npos;
}

std::string SanitizeHeadingAnchor(const std::string& heading) {
    std::string low = FoldUtf8ForSearch(Trim(heading), false, true);
    std::string out;
    bool prevDash = false;
    for (unsigned char ch : low) {
        if (std::isalnum(ch)) {
            out.push_back(static_cast<char>(ch));
            prevDash = false;
        } else if (!prevDash && !out.empty()) {
            out.push_back('-');
            prevDash = true;
        }
    }
    if (!out.empty() && out.back() == '-') out.pop_back();
    return out.empty() ? "seccion" : out;
}

std::string LineContextAt(const std::string& text, std::size_t pos, std::size_t& outLineIdx) {
    outLineIdx = 0;
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i < pos && i < text.size(); ++i) {
        if (text[i] == '\n') {
            ++outLineIdx;
            lineStart = i + 1;
        }
    }
    std::size_t lineEnd = text.find('\n', lineStart);
    if (lineEnd == std::string::npos) lineEnd = text.size();
    std::string line = Trim(text.substr(lineStart, lineEnd - lineStart));
    if (line.size() > 140) line = line.substr(0, 137) + "...";
    return line;
}

bool IsWordBoundaryChar(unsigned char ch) {
    return std::isalnum(ch) != 0 || ch == '_' || ch >= 0x80u;
}

bool WriteTextFile(const fs::path& p, const std::string& content, std::string* outErr) {
    try {
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (!ofs) {
            if (outErr) *outErr = "No se pudo abrir archivo para escritura.";
            return false;
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        return static_cast<bool>(ofs);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool ReadTextFile(const fs::path& p, std::string& outContent, std::string* outErr) {
    try {
        std::ifstream ifs(p, std::ios::binary | std::ios::ate);
        if (!ifs) {
            if (outErr) *outErr = "No se pudo abrir archivo para lectura.";
            return false;
        }
        const auto sz = ifs.tellg();
        if (sz < 0) return false;
        outContent.resize(static_cast<std::size_t>(sz));
        ifs.seekg(0, std::ios::beg);
        if (!outContent.empty()) ifs.read(outContent.data(), static_cast<std::streamsize>(outContent.size()));
        return static_cast<bool>(ifs);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

void SplitTargetComponents(
    const std::string& innerExpr,
    std::string& outTitle,
    std::string& outHeading,
    std::string& outBlockId,
    std::string& outAlias)
{
    outTitle.clear();
    outHeading.clear();
    outBlockId.clear();
    outAlias.clear();

    std::string pathPart = innerExpr;
    const auto bar = innerExpr.find('|');
    if (bar != std::string::npos) {
        pathPart = Trim(innerExpr.substr(0, bar));
        outAlias = Trim(innerExpr.substr(bar + 1));
    } else {
        pathPart = Trim(innerExpr);
    }

    const auto hashCaret = pathPart.find("#^");
    if (hashCaret != std::string::npos) {
        outTitle = Trim(pathPart.substr(0, hashCaret));
        outBlockId = Trim(pathPart.substr(hashCaret + 2));
        return;
    }
    const auto caret = pathPart.find('^');
    if (caret != std::string::npos) {
        outTitle = Trim(pathPart.substr(0, caret));
        outBlockId = Trim(pathPart.substr(caret + 1));
        return;
    }
    const auto hash = pathPart.find('#');
    if (hash != std::string::npos) {
        outTitle = Trim(pathPart.substr(0, hash));
        outHeading = Trim(pathPart.substr(hash + 1));
        return;
    }
    outTitle = Trim(pathPart);
}

std::string RebuildWikiInnerExpr(
    const std::string& title,
    const std::string& heading,
    const std::string& blockId,
    const std::string& alias,
    bool hadHashCaret)
{
    std::string s = title;
    if (!blockId.empty()) {
        s += (hadHashCaret ? "#^" : "^") + blockId;
    } else if (!heading.empty()) {
        s += "#" + heading;
    }
    if (!alias.empty()) {
        s += "|" + alias;
    }
    return s;
}

} // namespace

// ---------------- RichBlockDocument & Code Syntax ----------------

RichBlockDocument RichBlockDocument::FromMarkdown(const std::string& mdUtf8) {
    RichBlockDocument doc;
    std::vector<std::string> aliases;
    std::vector<std::string> tags;
    std::string body = mdUtf8;
    if (mdUtf8.rfind("---", 0) == 0) {
        KnowledgeVault::ParseFrontmatterAndBody(mdUtf8, doc.properties, aliases, tags, body);
    }

    std::istringstream iss(body);
    std::string line;
    std::uint64_t nextBid = 1;
    bool inCode = false;
    std::string codeLang;
    std::string codeBuf;
    bool inSync = false;
    std::string syncId;
    std::string syncBuf;
    bool inHtml = false;
    std::string htmlBuf;

    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (inCode) {
            if (Trim(line).rfind("```", 0) == 0) {
                RichBlock b;
                b.blockId = nextBid++;
                b.kind = RichBlockKind::CodeBlock;
                b.extraUtf8 = codeLang;
                b.textUtf8 = codeBuf;
                doc.blocks.push_back(std::move(b));
                inCode = false;
                codeLang.clear();
                codeBuf.clear();
            } else {
                if (!codeBuf.empty()) codeBuf.push_back('\n');
                codeBuf += line;
            }
            continue;
        }

        if (inSync) {
            if (Trim(line) == ":::") {
                RichBlock b;
                b.blockId = nextBid++;
                b.kind = RichBlockKind::SyncedBlock;
                b.extraUtf8 = syncId;
                b.textUtf8 = syncBuf;
                doc.blocks.push_back(std::move(b));
                inSync = false;
                syncId.clear();
                syncBuf.clear();
            } else {
                if (!syncBuf.empty()) syncBuf.push_back('\n');
                syncBuf += line;
            }
            continue;
        }

        if (inHtml) {
            if (Trim(line) == ":::html-end") {
                RichBlock b;
                b.blockId = nextBid++;
                b.kind = RichBlockKind::HtmlSnippet;
                b.textUtf8 = htmlBuf;
                doc.blocks.push_back(std::move(b));
                inHtml = false;
                htmlBuf.clear();
            } else {
                if (!htmlBuf.empty()) htmlBuf.push_back('\n');
                htmlBuf += line;
            }
            continue;
        }

        const std::string tr = Trim(line);
        if (tr.empty()) continue;

        if (tr.rfind("```", 0) == 0) {
            inCode = true;
            codeLang = Trim(tr.substr(3));
            codeBuf.clear();
            continue;
        }
        if (tr.rfind(":::sync[", 0) == 0) {
            const auto rb = tr.find(']');
            syncId = (rb != std::string::npos) ? tr.substr(8, rb - 8) : "sync_default";
            inSync = true;
            syncBuf.clear();
            continue;
        }
        if (tr == ":::html") {
            inHtml = true;
            htmlBuf.clear();
            continue;
        }
        if (tr == "---" || tr == "***") {
            RichBlock b;
            b.blockId = nextBid++;
            b.kind = RichBlockKind::Divider;
            doc.blocks.push_back(std::move(b));
            continue;
        }
        if (tr.rfind("$$", 0) == 0 && tr.size() > 4 && tr.rfind("$$") == tr.size() - 2) {
            RichBlock b;
            b.blockId = nextBid++;
            b.kind = RichBlockKind::MathBlock;
            b.textUtf8 = Trim(tr.substr(2, tr.size() - 4));
            doc.blocks.push_back(std::move(b));
            continue;
        }

        // Count leading spaces/tabs for indent level (2 spaces or 1 tab per level)
        int spaces = 0;
        std::size_t contentStart = 0;
        while (contentStart < line.size() && (line[contentStart] == ' ' || line[contentStart] == '\t')) {
            spaces += (line[contentStart] == '\t') ? 2 : 1;
            ++contentStart;
        }
        const int indent = spaces / 2;
        const std::string rest = line.substr(contentStart);

        RichBlock b;
        b.blockId = nextBid++;
        b.indentLevel = indent;

        // Extract optional trailing ^blockId
        std::string working = rest;
        const auto caretPos = working.rfind(" ^");
        if (caretPos != std::string::npos && caretPos + 2 < working.size()) {
            const std::string cand = Trim(working.substr(caretPos + 2));
            bool okAnchor = !cand.empty();
            for (unsigned char ch : cand) {
                if (!std::isalnum(ch) && ch != '-' && ch != '_') { okAnchor = false; break; }
            }
            if (okAnchor) {
                b.stableAnchorUtf8 = cand;
                working = Trim(working.substr(0, caretPos));
            }
        }

        if (working.rfind("### ", 0) == 0) {
            b.kind = RichBlockKind::Heading3;
            b.textUtf8 = Trim(working.substr(4));
        } else if (working.rfind("## ", 0) == 0) {
            b.kind = RichBlockKind::Heading2;
            b.textUtf8 = Trim(working.substr(3));
        } else if (working.rfind("# ", 0) == 0) {
            b.kind = RichBlockKind::Heading1;
            b.textUtf8 = Trim(working.substr(2));
        } else if (working.rfind("- [ ] ", 0) == 0 || working.rfind("- [x] ", 0) == 0 || working.rfind("- [X] ", 0) == 0) {
            b.kind = RichBlockKind::Checklist;
            b.checked = (working[3] == 'x' || working[3] == 'X');
            b.textUtf8 = Trim(working.substr(6));
        } else if (working.rfind("- ", 0) == 0 || working.rfind("* ", 0) == 0) {
            b.kind = RichBlockKind::BulletList;
            b.textUtf8 = Trim(working.substr(2));
        } else if (working.rfind("> [!toggle] ", 0) == 0 || working.rfind(">+", 0) == 0) {
            b.kind = RichBlockKind::Toggle;
            b.collapsed = true;
            b.textUtf8 = Trim(working.rfind(">+", 0) == 0 ? working.substr(2) : working.substr(12));
        } else if (working.rfind("> [!", 0) == 0) {
            const auto rb = working.find(']');
            b.kind = RichBlockKind::Callout;
            b.extraUtf8 = (rb != std::string::npos) ? working.substr(4, rb - 4) : "note";
            b.textUtf8 = (rb != std::string::npos && rb + 1 < working.size()) ? Trim(working.substr(rb + 1)) : "";
        } else if (working.rfind("> ", 0) == 0) {
            b.kind = RichBlockKind::Quote;
            b.textUtf8 = Trim(working.substr(2));
        } else {
            // Check numbered list e.g. "1. item"
            const auto dotPos = working.find(". ");
            bool isNum = (dotPos != std::string::npos && dotPos > 0 && dotPos <= 4);
            if (isNum) {
                for (std::size_t i = 0; i < dotPos; ++i) {
                    if (!std::isdigit(static_cast<unsigned char>(working[i]))) { isNum = false; break; }
                }
            }
            if (isNum) {
                b.kind = RichBlockKind::NumberedList;
                b.textUtf8 = Trim(working.substr(dotPos + 2));
            } else {
                b.kind = RichBlockKind::Paragraph;
                b.textUtf8 = working;
            }
        }
        doc.blocks.push_back(std::move(b));
    }
    return doc;
}

std::string RichBlockDocument::ToMarkdown() const {
    std::ostringstream oss;
    if (!properties.empty()) {
        oss << KnowledgeVault::SerializeFrontmatterAndBody(properties, {}, {}, "");
    }
    int numCounter = 1;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const auto& b = blocks[i];
        const std::string ind(static_cast<std::size_t>(std::max(0, b.indentLevel)) * 2u, ' ');
        const std::string anchor = b.stableAnchorUtf8.empty() ? "" : (" ^" + b.stableAnchorUtf8);
        if (b.kind != RichBlockKind::NumberedList) numCounter = 1;

        switch (b.kind) {
        case RichBlockKind::Heading1: oss << "# " << b.textUtf8 << anchor << "\n\n"; break;
        case RichBlockKind::Heading2: oss << "## " << b.textUtf8 << anchor << "\n\n"; break;
        case RichBlockKind::Heading3: oss << "### " << b.textUtf8 << anchor << "\n\n"; break;
        case RichBlockKind::BulletList: oss << ind << "- " << b.textUtf8 << anchor << "\n"; break;
        case RichBlockKind::NumberedList: oss << ind << numCounter++ << ". " << b.textUtf8 << anchor << "\n"; break;
        case RichBlockKind::Checklist: oss << ind << "- [" << (b.checked ? 'x' : ' ') << "] " << b.textUtf8 << anchor << "\n"; break;
        case RichBlockKind::CodeBlock:
            oss << "```" << b.extraUtf8 << "\n" << b.textUtf8 << "\n```\n\n";
            break;
        case RichBlockKind::Quote: oss << "> " << b.textUtf8 << anchor << "\n\n"; break;
        case RichBlockKind::Callout: oss << "> [!" << (b.extraUtf8.empty() ? "note" : b.extraUtf8) << "] " << b.textUtf8 << anchor << "\n\n"; break;
        case RichBlockKind::Toggle: oss << "> [!toggle] " << b.textUtf8 << anchor << "\n\n"; break;
        case RichBlockKind::Divider: oss << "---\n\n"; break;
        case RichBlockKind::MathBlock: oss << "$$" << b.textUtf8 << "$$\n\n"; break;
        case RichBlockKind::SyncedBlock:
            oss << ":::sync[" << (b.extraUtf8.empty() ? "sync_1" : b.extraUtf8) << "]\n" << b.textUtf8 << "\n:::\n\n";
            break;
        case RichBlockKind::HtmlSnippet:
            oss << ":::html\n" << b.textUtf8 << "\n:::html-end\n\n";
            break;
        case RichBlockKind::Paragraph:
        default:
            oss << b.textUtf8 << anchor << "\n\n";
            break;
        }
    }
    return Trim(oss.str());
}

std::string RichBlockDocument::ReflowParagraph(const std::string& textUtf8, std::size_t maxCharsPerLine) {
    if (maxCharsPerLine < 8 || textUtf8.size() <= maxCharsPerLine) return textUtf8;
    std::istringstream iss(textUtf8);
    std::string word;
    std::ostringstream out;
    std::size_t curLen = 0;
    while (iss >> word) {
        if (curLen == 0) {
            out << word;
            curLen = word.size();
        } else if (curLen + 1 + word.size() <= maxCharsPerLine) {
            out << ' ' << word;
            curLen += 1 + word.size();
        } else {
            out << '\n' << word;
            curLen = word.size();
        }
    }
    return out.str();
}

std::string RichBlockDocument::ToReadingPlainText(std::size_t wrapCols) const {
    std::ostringstream oss;
    int numCounter = 1;
    for (const auto& b : blocks) {
        const std::string ind(static_cast<std::size_t>(std::max(0, b.indentLevel)) * 2u, ' ');
        if (b.kind != RichBlockKind::NumberedList) numCounter = 1;
        switch (b.kind) {
        case RichBlockKind::Heading1: oss << b.textUtf8 << "\n" << std::string(std::min<std::size_t>(b.textUtf8.size(), 40u), '=') << "\n\n"; break;
        case RichBlockKind::Heading2: oss << b.textUtf8 << "\n" << std::string(std::min<std::size_t>(b.textUtf8.size(), 32u), '-') << "\n\n"; break;
        case RichBlockKind::Heading3: oss << b.textUtf8 << "\n\n"; break;
        case RichBlockKind::BulletList: oss << ind << "• " << b.textUtf8 << "\n"; break;
        case RichBlockKind::NumberedList: oss << ind << numCounter++ << ". " << b.textUtf8 << "\n"; break;
        case RichBlockKind::Checklist: oss << ind << (b.checked ? "☑ " : "☐ ") << b.textUtf8 << "\n"; break;
        case RichBlockKind::CodeBlock: oss << "[Código " << b.extraUtf8 << "]\n" << b.textUtf8 << "\n\n"; break;
        case RichBlockKind::Quote: oss << "│ " << ReflowParagraph(b.textUtf8, wrapCols) << "\n\n"; break;
        case RichBlockKind::Callout: oss << "[" << (b.extraUtf8.empty() ? "Nota" : b.extraUtf8) << "] " << ReflowParagraph(b.textUtf8, wrapCols) << "\n\n"; break;
        case RichBlockKind::Toggle:
            oss << (b.collapsed ? "▸ " : "▾ ") << b.textUtf8 << "\n";
            if (!b.collapsed) {
                for (const auto& ch : b.children) oss << "  " << ch.textUtf8 << "\n";
            }
            oss << "\n";
            break;
        case RichBlockKind::Divider: oss << "────────────────────────\n\n"; break;
        case RichBlockKind::MathBlock: oss << b.textUtf8 << "\n\n"; break;
        case RichBlockKind::SyncedBlock: oss << b.textUtf8 << "\n\n"; break;
        case RichBlockKind::HtmlSnippet: oss << "[Bloque HTML]\n" << b.textUtf8 << "\n\n"; break;
        case RichBlockKind::Paragraph:
        default:
            oss << ReflowParagraph(b.textUtf8, wrapCols) << "\n\n";
            break;
        }
    }
    return Trim(oss.str());
}

std::string RichBlockDocument::ToHtml5(const std::string& cssSnippetUtf8) const {
    std::ostringstream html;
    html << "<!doctype html>\n<html lang=\"es\">\n<head><meta charset=\"utf-8\">"
         << "<title>CuadernU Vista de Lectura</title>\n"
         << "<style>body{font-family:'Segoe UI',sans-serif;max-width:860px;margin:2rem auto;padding:0 1.2rem;color:#111827;background:#FAFBFD;}"
         << "pre{background:#111827;color:#E5E7EB;padding:12px;border-radius:8px;overflow:auto;}"
         << "blockquote,.callout{border-left:3px solid #2563EB;padding:8px 14px;background:#EFF6FF;border-radius:6px;}"
         << "mark{background:#FEF08A;padding:1px 3px;border-radius:3px;}\n"
         << cssSnippetUtf8 << "\n</style></head>\n<body>\n";

    auto renderInline = [](const std::string& raw) -> std::string {
        std::string out = EscapeHtml(raw);
        // Render ==highlight== to <mark>...</mark>
        std::size_t pos = 0;
        while ((pos = out.find("==", pos)) != std::string::npos) {
            const auto end = out.find("==", pos + 2);
            if (end == std::string::npos) break;
            const std::string inner = out.substr(pos + 2, end - (pos + 2));
            out.replace(pos, end - pos + 2, "<mark>" + inner + "</mark>");
            pos += 13 + inner.size();
        }
        return out;
    };

    for (const auto& b : blocks) {
        switch (b.kind) {
        case RichBlockKind::Heading1: html << "<h1 id=\"" << EscapeHtml(SanitizeHeadingAnchor(b.textUtf8)) << "\">" << renderInline(b.textUtf8) << "</h1>\n"; break;
        case RichBlockKind::Heading2: html << "<h2 id=\"" << EscapeHtml(SanitizeHeadingAnchor(b.textUtf8)) << "\">" << renderInline(b.textUtf8) << "</h2>\n"; break;
        case RichBlockKind::Heading3: html << "<h3 id=\"" << EscapeHtml(SanitizeHeadingAnchor(b.textUtf8)) << "\">" << renderInline(b.textUtf8) << "</h3>\n"; break;
        case RichBlockKind::BulletList: html << "<li style=\"margin-left:" << (b.indentLevel * 18) << "px\">" << renderInline(b.textUtf8) << "</li>\n"; break;
        case RichBlockKind::NumberedList: html << "<li style=\"list-style:decimal;margin-left:" << (b.indentLevel * 18) << "px\">" << renderInline(b.textUtf8) << "</li>\n"; break;
        case RichBlockKind::Checklist:
            html << "<div style=\"margin-left:" << (b.indentLevel * 18) << "px\"><input type=\"checkbox\"" << (b.checked ? " checked" : "") << " disabled> " << renderInline(b.textUtf8) << "</div>\n";
            break;
        case RichBlockKind::CodeBlock:
            html << "<pre><code class=\"language-" << EscapeHtml(b.extraUtf8) << "\">" << EscapeHtml(b.textUtf8) << "</code></pre>\n";
            break;
        case RichBlockKind::Quote: html << "<blockquote>" << renderInline(b.textUtf8) << "</blockquote>\n"; break;
        case RichBlockKind::Callout: html << "<div class=\"callout\"><strong>" << EscapeHtml(b.extraUtf8) << ":</strong> " << renderInline(b.textUtf8) << "</div>\n"; break;
        case RichBlockKind::Toggle:
            html << "<details" << (b.collapsed ? "" : " open") << "><summary>" << renderInline(b.textUtf8) << "</summary>";
            for (const auto& ch : b.children) html << "<p>" << renderInline(ch.textUtf8) << "</p>";
            html << "</details>\n";
            break;
        case RichBlockKind::Divider: html << "<hr>\n"; break;
        case RichBlockKind::MathBlock: html << "<div class=\"math-block\">" << EscapeHtml(b.textUtf8) << "</div>\n"; break;
        case RichBlockKind::SyncedBlock: html << "<div class=\"synced-block\" data-sync-id=\"" << EscapeHtml(b.extraUtf8) << "\">" << renderInline(b.textUtf8) << "</div>\n"; break;
        case RichBlockKind::HtmlSnippet: html << "<div class=\"html-snippet-static\">" << b.textUtf8 << "</div>\n"; break;
        case RichBlockKind::Paragraph:
        default: html << "<p>" << renderInline(b.textUtf8) << "</p>\n"; break;
        }
    }
    html << "</body>\n</html>\n";
    return html.str();
}

std::vector<OutlineHeading> RichBlockDocument::ExtractOutline() const {
    std::vector<OutlineHeading> out;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        int lvl = 0;
        if (blocks[i].kind == RichBlockKind::Heading1) lvl = 1;
        else if (blocks[i].kind == RichBlockKind::Heading2) lvl = 2;
        else if (blocks[i].kind == RichBlockKind::Heading3) lvl = 3;
        if (lvl > 0) {
            out.push_back({ lvl, blocks[i].textUtf8, i, SanitizeHeadingAnchor(blocks[i].textUtf8) });
        }
    }
    return out;
}

std::vector<HighlightSpan> RichBlockDocument::ExtractHighlights() const {
    std::vector<HighlightSpan> out;
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        const std::string& t = blocks[i].textUtf8;
        std::size_t pos = 0;
        while ((pos = t.find("==", pos)) != std::string::npos) {
            const auto end = t.find("==", pos + 2);
            if (end == std::string::npos) break;
            if (end > pos + 2) {
                out.push_back({ i, t.substr(pos + 2, end - (pos + 2)) });
            }
            pos = end + 2;
        }
    }
    return out;
}

std::vector<CodeSyntaxToken> RichBlockDocument::TokenizeCodeSyntax(const std::string& codeUtf8, const std::string& langUtf8) {
    static const std::unordered_set<std::string> kKeywords{
        "if", "else", "for", "while", "return", "class", "struct", "enum", "def", "import", "from",
        "const", "let", "var", "function", "async", "await", "SELECT", "FROM", "WHERE", "true", "false",
        "nullptr", "null", "public", "private", "namespace", "using", "auto", "virtual", "override"
    };
    static const std::unordered_set<std::string> kTypes{
        "int", "double", "float", "bool", "void", "string", "std", "vector", "size_t", "uint64_t"
    };

    const bool pythonOrSqlComment = (FoldUtf8ForSearch(langUtf8, false, true).find("py") != std::string::npos);
    std::vector<CodeSyntaxToken> tokens;
    std::size_t i = 0;
    const std::size_t n = codeUtf8.size();

    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(codeUtf8[i]);
        if (std::isspace(c)) {
            ++i;
            continue;
        }
        // Comments
        if ((i + 1 < n && codeUtf8[i] == '/' && codeUtf8[i + 1] == '/') ||
            (pythonOrSqlComment && codeUtf8[i] == '#')) {
            const auto nl = codeUtf8.find('\n', i);
            const std::size_t end = (nl == std::string::npos) ? n : nl;
            tokens.push_back({ CodeTokenKind::Comment, i, end - i, codeUtf8.substr(i, end - i) });
            i = end;
            continue;
        }
        // String literal
        if (codeUtf8[i] == '"' || codeUtf8[i] == '\'') {
            const char q = codeUtf8[i];
            std::size_t j = i + 1;
            while (j < n) {
                if (codeUtf8[j] == '\\' && j + 1 < n) { j += 2; continue; }
                if (codeUtf8[j] == q) { ++j; break; }
                ++j;
            }
            tokens.push_back({ CodeTokenKind::StringLit, i, j - i, codeUtf8.substr(i, j - i) });
            i = j;
            continue;
        }
        // Numbers
        if (std::isdigit(c)) {
            std::size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(codeUtf8[j])) || codeUtf8[j] == '.')) ++j;
            tokens.push_back({ CodeTokenKind::Number, i, j - i, codeUtf8.substr(i, j - i) });
            i = j;
            continue;
        }
        // Identifiers / Keywords
        if (std::isalpha(c) || codeUtf8[i] == '_') {
            std::size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(codeUtf8[j])) || codeUtf8[j] == '_')) ++j;
            const std::string word = codeUtf8.substr(i, j - i);
            CodeTokenKind kind = CodeTokenKind::Plain;
            if (kKeywords.count(word)) kind = CodeTokenKind::Keyword;
            else if (kTypes.count(word)) kind = CodeTokenKind::TypeName;
            tokens.push_back({ kind, i, j - i, word });
            i = j;
            continue;
        }
        // Operators / punctuation
        tokens.push_back({ CodeTokenKind::Operator, i, 1, codeUtf8.substr(i, 1) });
        ++i;
    }
    return tokens;
}

// ---------------- ReusableElementLibrary ----------------

std::string ReusableElementLibrary::SaveSnippet(
    const std::string& nameUtf8,
    const std::string& categoryUtf8,
    const std::vector<AnnotationObject>& sourceObjects,
    const std::vector<std::string>& tagsUtf8)
{
    if (sourceObjects.empty()) return {};
    ReusableElementEntry e;
    e.idUtf8 = "snip_" + std::to_string(nextSeq_++);
    e.nameUtf8 = nameUtf8.empty() ? ("Elemento " + e.idUtf8) : nameUtf8;
    e.categoryUtf8 = categoryUtf8.empty() ? "Pegatinas" : categoryUtf8;
    e.tagsUtf8 = tagsUtf8;
    e.objects = sourceObjects;
    entries_.push_back(std::move(e));
    return entries_.back().idUtf8;
}

bool ReusableElementLibrary::UpdateSnippetName(const std::string& idUtf8, const std::string& newNameUtf8) {
    if (newNameUtf8.empty()) return false;
    for (auto& e : entries_) {
        if (e.idUtf8 == idUtf8) {
            e.nameUtf8 = newNameUtf8;
            return true;
        }
    }
    return false;
}

bool ReusableElementLibrary::RemoveSnippet(const std::string& idUtf8) {
    const auto it = std::remove_if(entries_.begin(), entries_.end(), [&](const ReusableElementEntry& e) {
        return e.idUtf8 == idUtf8;
    });
    if (it == entries_.end()) return false;
    entries_.erase(it, entries_.end());
    return true;
}

bool ReusableElementLibrary::InsertSnippetIntoDocument(
    const std::string& idUtf8,
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double targetX,
    double targetY,
    std::vector<std::uint64_t>* outInsertedIds) const
{
    if (outInsertedIds) outInsertedIds->clear();
    const ReusableElementEntry* found = nullptr;
    for (const auto& e : entries_) {
        if (e.idUtf8 == idUtf8) { found = &e; break; }
    }
    if (!found || found->objects.empty()) return false;

    double minX = 1e18, minY = 1e18;
    for (const auto& o : found->objects) {
        minX = (std::min)(minX, o.bounds.x);
        minY = (std::min)(minY, o.bounds.y);
    }
    const double dx = targetX - minX;
    const double dy = targetY - minY;

    std::unordered_map<std::uint64_t, std::uint64_t> oldToNew;
    std::vector<AnnotationObject> copies;
    copies.reserve(found->objects.size());
    for (const auto& orig : found->objects) {
        AnnotationObject clone = orig;
        const std::uint64_t newId = doc.AllocateObjectId();
        oldToNew[orig.id] = newId;
        clone.id = newId;
        clone.zOrder = doc.AllocateZOrder();
        clone.layerId = doc.ActiveLayerId();
        clone.Translate(dx, dy);
        copies.push_back(std::move(clone));
    }
    for (auto& c : copies) {
        if (c.connectorFromId != 0 && oldToNew.count(c.connectorFromId)) c.connectorFromId = oldToNew[c.connectorFromId];
        if (c.connectorToId != 0 && oldToNew.count(c.connectorToId)) c.connectorToId = oldToNew[c.connectorToId];
    }

    for (auto& c : copies) {
        const std::uint64_t cid = c.id;
        if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(c)))) return false;
        if (outInsertedIds) outInsertedIds->push_back(cid);
    }
    return true;
}

std::vector<ReusableElementEntry> ReusableElementLibrary::FilterByCategory(const std::string& categoryUtf8) const {
    if (categoryUtf8.empty()) return entries_;
    std::vector<ReusableElementEntry> out;
    for (const auto& e : entries_) {
        if (EqualsFolded(e.categoryUtf8, categoryUtf8)) out.push_back(e);
    }
    return out;
}

bool ReusableElementLibrary::SaveToFile(const std::wstring& jsonPath, std::string* outErr) const {
    try {
        json root;
        root["version"] = 1;
        root["nextSeq"] = nextSeq_;
        json arr = json::array();
        for (const auto& e : entries_) {
            DocumentModel wrap;
            wrap.ResetToBlank(DocumentMode::InfiniteCanvas);
            for (const auto& obj : e.objects) wrap.AddCanvasObject(obj);
            std::vector<std::uint8_t> cudocBytes;
            CudocStorage::SerializeToBytes(wrap, cudocBytes);
            // Encode compact hex of binary .cudoc snippet payload
            static constexpr char kHex[] = "0123456789abcdef";
            std::string hex;
            hex.reserve(cudocBytes.size() * 2u);
            for (std::uint8_t b : cudocBytes) {
                hex.push_back(kHex[(b >> 4) & 0x0Fu]);
                hex.push_back(kHex[b & 0x0Fu]);
            }
            arr.push_back({
                {"id", e.idUtf8},
                {"name", e.nameUtf8},
                {"category", e.categoryUtf8},
                {"tags", e.tagsUtf8},
                {"cudocHex", hex}
            });
        }
        root["entries"] = std::move(arr);
        return WriteTextFile(fs::path(WideToUtf8(jsonPath)), root.dump(2), outErr);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool ReusableElementLibrary::LoadFromFile(const std::wstring& jsonPath, std::string* outErr) {
    std::string raw;
    if (!ReadTextFile(fs::path(WideToUtf8(jsonPath)), raw, outErr)) return false;
    try {
        const json root = json::parse(raw);
        entries_.clear();
        nextSeq_ = root.value("nextSeq", std::uint64_t{ 1 });
        if (root.contains("entries") && root["entries"].is_array()) {
            for (const auto& ej : root["entries"]) {
                ReusableElementEntry e;
                e.idUtf8 = ej.value("id", "");
                e.nameUtf8 = ej.value("name", "");
                e.categoryUtf8 = ej.value("category", "Pegatinas");
                if (ej.contains("tags") && ej["tags"].is_array()) {
                    for (const auto& t : ej["tags"]) e.tagsUtf8.push_back(t.get<std::string>());
                }
                const std::string hex = ej.value("cudocHex", "");
                if (hex.size() >= 2 && (hex.size() % 2 == 0)) {
                    std::vector<std::uint8_t> bytes(hex.size() / 2u);
                    auto hv = [](char ch) -> int {
                        if (ch >= '0' && ch <= '9') return ch - '0';
                        if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                        if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                        return 0;
                    };
                    for (std::size_t i = 0; i < bytes.size(); ++i) {
                        bytes[i] = static_cast<std::uint8_t>((hv(hex[2 * i]) << 4) | hv(hex[2 * i + 1]));
                    }
                    DocumentModel wrap;
                    if (CudocStorage::DeserializeFromBytes(bytes.data(), bytes.size(), wrap)) {
                        e.objects = wrap.CanvasObjects();
                    }
                }
                entries_.push_back(std::move(e));
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

// ---------------- CommandPaletteRegistry ----------------

void CommandPaletteRegistry::RegisterDefaultCommands() {
    using namespace CmdIds;
    entries_.clear();
    entries_ = {
        { CMD_OPEN,                    "/abrir",               "Abrir documento o PDF...",                              "Archivo",           "Ctrl+O",       "open pdf cuaderno cudoc" },
        { CMD_SAVE_CUDOC,              "/guardar",             "Guardar cuaderno (.cudoc)",                             "Archivo",           "Ctrl+S",       "save persist" },
        { CMD_OPEN_DRAFTS,             "/borradores",          "Recuperar borradores (lo que no se guard\xC3\xB3)...",  "Archivo",           "",             "recuperar borrador autoguardado sin guardar copia seguridad recovery draft" },
        { CMD_EXPORT_PDF,              "/exportar-pdf",        "Exportar a PDF...",                                     "Archivo",           "Ctrl+Shift+E", "pdf print" },
        { CMD_PRINT,                   "/imprimir",            "Imprimir documento...",                                 "Archivo",           "Ctrl+P",       "print" },
        { CMD_TOOL_TEXT,               "/texto",               "Herramienta: Caja de texto",                            "Herramientas",      "T",            "textbox nota" },
        { CMD_ADD_FORMULA,             "/formula",             "Insertar bloque matem\xC3\xA1tico 2D",                  "STEM",              "Ctrl+M",       "math latex cas" },
        { CMD_ADD_TABLE,               "/tabla",               "Insertar tabla o bloque estructurado",                  "Bloques",           "",             "table grid database" },
        { CMD_ADD_CODE_BLOCK,          "/codigo",              "Insertar bloque de c\xC3\xB3""digo",                    "Bloques",           "",             "code cpp python syntax" },
        { CMD_INSERT_PRINTOUT,         "/copia-impresa",       "Insertar copia impresa (PDF/Word/PPTX/Excel)...",       "Archivo",           "",             "printout word pptx excel" },
        { CMD_OPEN_VAULT_LIBRARY,      "/boveda",              "Biblioteca y B\xC3\xB3veda Global",                     "Conocimiento",      "",             "vault search folders" },
        { CMD_OPEN_DAILY_NOTE,         "/diaria",              "Abrir o crear Nota Diaria (hoy)",                       "Conocimiento",      "",             "daily note zettelkasten" },
        { CMD_TOGGLE_GRAPH_VIEW,       "/grafo",               "Mostrar / Ocultar Vista de Grafo",                      "Conocimiento",      "",             "graph backlinks network" },
        { CMD_VIEW_BACKLINKS,          "/backlinks",           "Ver Backlinks y Menciones sin enlazar",                 "Conocimiento",      "",             "links references" },
        { CMD_NEW_TEMPLATE_NOTE,       "/plantilla",           "Crear nota desde plantilla...",                         "Conocimiento",      "",             "template variables" },
        { CMD_INSERT_SYNCED_BLOCK,     "/sincronizado",        "Insertar bloque sincronizado...",                       "Bloques",           "",             "synced block transclusion" },
        { CMD_SAVE_REUSABLE_SNIPPET,   "/elemento-guardar",    "Guardar selecci\xC3\xB3n como elemento reutilizable",   "Elementos",         "",             "reusable sticker snippet" },
        { CMD_INSERT_REUSABLE_SNIPPET, "/elemento-usar",       "Insertar elemento / pegatina guardada...",              "Elementos",         "",             "sticker stamp insert" },
        { CMD_EXPORT_STICKY_NOTES,     "/exportar-notas",      "Exportar notas adhesivas...",                           "Bloques",           "",             "sticky notes export" },
        { CMD_EDIT_PROPERTIES,         "/propiedades",         "Editar propiedades (frontmatter YAML)...",              "Conocimiento",      "",             "yaml metadata properties" },
        { CMD_TOGGLE_READING_VIEW,     "/lectura",             "Alternar Vista de Lectura / Edici\xC3\xB3n",            "Vista",             "",             "reading markdown preview" },
        { CMD_CUSTOM_SHORTCUTS,        "/atajos",              "Configurar atajos personalizados...",                   "Ajustes",           "",             "keymap shortcuts" },
        { CMD_THEME_SELECTOR,          "/tema",                "Seleccionar tema visual...",                            "Vista",             "",             "theme claro oscuro porcelana" },
        { CMD_TOOLBAR_ORIENTATION,     "/orientacion-barra",   "Cambiar orientaci\xC3\xB3n de barra de herramientas",   "Vista",             "",             "toolbar orientation vertical horizontal" },
        { CMD_EDIT_CUSTOM_CSS,         "/css-personalizado",   "Editar fragmento CSS personalizado...",                 "Vista",             "",             "css snippet style publish" },
        { CMD_INSERT_STRUCTURED_DB,    "/base",                "Insertar base de datos estructurada (.base)",           "Bases de Datos",    "",             "database table notion obsidian" },
        { CMD_TOGGLE_DB_PANEL,         "/panel-base",          "Mostrar / Ocultar panel interactivo de Base de Datos",  "Bases de Datos",    "",             "database panel kanban calendar" },
        { CMD_CYCLE_DB_VIEW,           "/kanban",              "Cambiar vista de base (Tabla/Kanban/Calendario/Galer\xC3\xAD""a)", "Bases de Datos", "",   "view board calendar timeline gallery" },
        { CMD_EXPORT_BASE_FILE,        "/exportar-base",       "Exportar archivo .base a disco...",                     "Bases de Datos",    "",             "obsidian base export" },
        { CMD_IMPORT_BASE_FILE,        "/importar-base",       "Importar archivo .base desde disco...",                 "Bases de Datos",    "",             "obsidian base import" },
        { CMD_AUTOMATION_RULES,        "/automatizaciones",    "Configurar reglas de automatizaci\xC3\xB3n de base...", "Bases de Datos",    "",             "automation trigger action rule" },
        { CMD_FLEXCIL_POPUP_OVERLAY,   "/visor-emergente",     "Abrir / Ocultar Visor Emergente de Estudio",            "Estudio PDF",       "",             "flexcil popup overlay note" },
        { CMD_FLEXCIL_GESTURE_MODE,    "/modo-gestos",         "Alternar Modo Gestos / Modo L\xC3\xA1piz",              "Estudio PDF",       "",             "flexcil gesture L capture" },
        { CMD_SELECTIVE_PDF_EXPORT,    "/exportar-selectivo",  "Exportar PDF Selectivo (Editable/Aplanado/Rango)...",   "Estudio PDF",       "",             "selective pdf export clean flattened" },
        { CMD_HANDWRITING_REFLOW,      "/reflujo-tinta",       "Reflujo responsivo de escritura manuscrita",            "Tinta",             "",             "reflow handwriting words margin" },
        { CMD_PASTE_AS_HANDWRITING,    "/pegar-manuscrito",    "Pegar texto transformado a escritura manuscrita...",    "Tinta",             "",             "paste text as handwriting smart script" },
        { CMD_HANDWRITING_SPELLING,    "/ortografia-tinta",    "Revisar ortograf\xC3\xAD""a manuscrita", "Tinta",            "",             "spelling handwriting check" },
        { CMD_ZOOM_WRITING_BOX,        "/zoom-escritura",      "Alternar Caja de Zoom para Escritura (auto-avance)",    "Tinta",             "",             "zoom writing box auto advance" },
        { CMD_REACTIVE_MATH_EVAL,      "/matematicas-reactivas","Evaluar ecuaciones reactivas (=) y variables",         "STEM",              "",             "math notes reactive equals variables" },
        { CMD_CHEMISTRY_BALANCER,      "/quimica",             "Reconocer y balancear ecuaci\xC3\xB3n qu\xC3\xADmica 2D...", "STEM",         "",             "chemistry molar mass stoichiometry" },
        { CMD_CONTENT_VIEWS,           "/content-views",       "Biblioteca y Content Views (Fotos/Audio/Smart Folders)", "Biblioteca",       "",             "photos recordings bookmarks smart folders" },
        { CMD_AUDIO_DIARIZE,           "/diarizar-audio",      "Transcribir y diarizar audio multi-hablante",           "Audio",             "",             "speaker diarization transcript" },
        { CMD_AUDIO_NOISE_REDUCTION,   "/reducir-ruido",       "Aplicar filtro de reducci\xC3\xB3n de ruido al audio",  "Audio",             "",             "noise reduction audio filter" },
        { CMD_AUDIO_SPEED_CYCLE,       "/velocidad-audio",     "Cambiar velocidad de reproducci\xC3\xB3n de audio",     "Audio",             "",             "audio speed playback" },
        { CMD_MEETING_MINUTES_ICS,     "/acta-ics",            "Generar acta de reuni\xC3\xB3n desde archivo .ics...",  "Audio",             "",             "meeting notes calendar ics" },
        { CMD_ANCHORED_COMMENTS,       "/comentarios",         "Agregar comentario anclado con @menciones...",          "Colaboraci\xC3\xB3n","",            "anchored comment thread mention" },
        { CMD_TOGGLE_COMMENTS_PANEL,   "/panel-comentarios",   "Ver / Resolver hilos de comentarios anclados",          "Colaboraci\xC3\xB3n","",            "comments resolve thread" },
        { CMD_LOCK_NOTE_PASSPHRASE,    "/bloquear-nota",       "Bloquear / Desbloquear nota con contrase\xC3\xB1""a (PBKDF2)", "Seguridad", "",            "lock password pbkdf2 security" },
        { CMD_TOGGLE_LAN_COLLAB_SERVER,"/servidor-lan",        "Iniciar / Detener servidor colaborativo LAN local",     "Colaboraci\xC3\xB3n","",            "lan collab server share qr" },
        { CMD_STATIC_SITE_PUBLISH,     "/publicar-sitio",      "Publicar notas como sitio web est\xC3\xA1tico HTML5...", "Colaboraci\xC3\xB3n","",           "static site publish html slug" },
        { CMD_FOLDER_SYNC_3MODES,      "/sincronizar-carpeta", "Sincronizar biblioteca (Subir/Reemplazar/Fusionar)...", "Sincronizaci\xC3\xB3n","",          "sync upload replace merge backup" },
        { CMD_WEB_CLIPPER_CAPTURE,     "/web-clipper",         "Capturar URL o p\xC3\xA1gina HTML con Web Clipper...",  "Interoperabilidad", "",             "web clipper url http template" },
        { CMD_JSON_CANVAS_EXPORT,      "/exportar-canvas",     "Exportar JSON Canvas 1.0 (.canvas)...",                 "Interoperabilidad", "",             "json canvas obsidian export" },
        { CMD_IMPORT_JSON_CANVAS,      "/importar-canvas",     "Importar JSON Canvas 1.0 (.canvas)...",                 "Interoperabilidad", "",             "json canvas obsidian import" },
        { CMD_EXPORT_SVG,              "/exportar-svg",        "Exportar p\xC3\xA1gina actual a SVG vectorial...",      "Interoperabilidad", "",             "svg vector export" },
        { CMD_EXPORT_PAGE_IMAGE,       "/exportar-imagen",     "Exportar p\xC3\xA1gina actual a PNG o JPEG...",         "Interoperabilidad", "",             "png jpeg image export" },
        { CMD_EXPORT_ANKI_DECK,        "/exportar-anki",       "Exportar mazo de estudio a CSV / TSV / Anki...",        "Estudio",           "",             "anki csv tsv flashcards" },
        { CMD_EXPORT_ONENOTE_HTML,     "/exportar-onenote",    "Exportar p\xC3\xA1gina a HTML compatible OneNote...",   "Interoperabilidad", "",             "onenote graph html" },
        { CMD_IMPORT_UNIVERSAL_PKG,    "/importar-paquete",    "Importar paquete universal (.cuadernu-pkg/.goodnotes/.note)...", "Interoperabilidad", "",  "import goodnotes nebo ntb nsa" },
        { CMD_EXPORT_UNIVERSAL_PKG,    "/exportar-paquete",    "Exportar paquete portable (.cuadernu-pkg)...",          "Interoperabilidad", "",             "export package portable" },
        { CMD_GROUNDED_AI_CITATIONS,   "/ia-citas",            "Consultar al cuaderno con citas fundamentadas...",      "Asistente",         "",             "grounded ai citations qa ollama" },
        { CMD_AUTO_QUIZ_GENERATOR,     "/cuestionario-ia",     "Iniciar cuestionario interactivo de estudio...",        "Asistente",         "",             "quiz flashcards study progress" },
        { CMD_TRANSLATE_SELECTION,      "/traducir",            "Traducir selecci\xC3\xB3n de texto...",                 "Asistente",         "",             "translate bilingual language" },
        { CMD_AI_SETTINGS,              "/ajustes-ia",          "Configurar IA local (Ollama/OpenAI) y ASR Whisper...",  "Ajustes",           "",             "ollama llm whisper speech config" },
        { CMD_BACKUP_RESTORE_MGR,       "/respaldo",            "Respaldar o restaurar biblioteca / nota individual...", "Sincronizaci\xC3\xB3n","",          "backup restore single note zip" },
        { CMD_NOTE_COVER_PAPER,         "/portada-papel",       "Cambiar portada y estilo de papel de la nota...",       "Cuaderno",          "",             "cover paper style template color" },
        { CMD_SHARE_LINKS_MGR,          "/permisos-enlaces",    "Gestionar enlaces compartidos y permisos de nota...",   "Colaboraci\xC3\xB3n","",            "share links revoke role permissions" },
        { CMD_GRAPH_LOCAL_FILTER,       "/grafo-local",         "Grafo local y filtros por etiqueta / carpeta...",       "Conocimiento",      "",             "local graph filter tag folder" },
        { CMD_VAULT_SEARCH_RENAME,      "/buscar-boveda",       "Buscar en b\xC3\xB3veda, filtrar propiedades y renombrar...", "Conocimiento", "",             "vault search filter rename wikilink" },
        { CMD_DOCUMENT_OUTLINE,         "/esquema",             "Esquema de encabezados y contador de palabras...",      "Conocimiento",      "",             "outline headings word count" },
        { CMD_DB_EDITOR_MGR,            "/editar-base",         "Editor completo de Base de Datos (columnas/f\xC3\xB3rmulas/vistas)...", "Bases de Datos", "", "database crud formula rollup filter sort template" },
        { CMD_DB_SUBMIT_FORM,           "/formulario-base",     "Abrir formulario conectado a Base de Datos...",         "Bases de Datos",    "",             "form submit database record" },
        { CMD_DB_ACTION_BUTTON,         "/boton-accion",        "Ejecutar bot\xC3\xB3n de acci\xC3\xB3n (automatizaci\xC3\xB3n)...", "Bases de Datos", "", "action button trigger automation" },
        { CMD_AUDIO_PAUSE_RESUME,       "/pausar-audio",        "Pausar / Reanudar grabaci\xC3\xB3n de audio",           "Audio",             "",             "pause resume audio recording" },
        { CMD_AUDIO_SEARCH_SEEK,        "/buscar-audio",        "Buscar palabra en transcripci\xC3\xB3n y saltar...",    "Audio",             "",             "search audio transcript seek timestamp" },
        { CMD_AUDIO_EXPORT_WAV,         "/exportar-wav",        "Exportar grabaci\xC3\xB3n de audio (.wav)...",          "Audio",             "",             "export wav audio file" },
        { CMD_AUDIO_MERGE_WAV,          "/combinar-audio",      "Combinar pista de audio externa (.wav)...",             "Audio",             "",             "merge audio track wav" },
        { CMD_STUDY_PROGRESS_TIMER,     "/progreso-estudio",    "Cron\xC3\xB3metro y progreso de estudio SM-2...",         "Estudio",           "",             "study timer progress minutes mastered" },
        { CMD_IMPORT_STUDY_DECK,        "/importar-tarjetas",   "Importar tarjetas de estudio desde CSV / TSV...",       "Estudio",           "",             "import flashcards csv tsv anki" },
        { CMD_LAYERS_MANAGER,           "/gestor-capas",        "Gestor de capas (reordenar, eliminar, opacidad)...",    "Capas",             "",             "layers reorder move delete opacity" },
        { CMD_PDF_ANNOTATIONS_MGR,      "/anotaciones-pdf",     "Gestor de anotaciones, marcadores, metadatos y adjuntos PDF...", "Estudio PDF", "", "annotations bookmarks metadata attachments sign" },
        { CMD_PDF_SCROLL_DIRECTION,     "/direccion-pdf",       "Alternar desplazamiento PDF vertical / horizontal",     "Estudio PDF",       "",             "scroll direction horizontal vertical pdf" },
        { CMD_STYLUS_DOUBLE_TAP_ERASER, "/doble-toque-borrador","Alternar l\xC3\xA1piz / borrador (doble toque)",        "Tinta",             "",             "double tap eraser stylus toggle" },
        { CMD_INK_WORD_SPACE_GESTURE,   "/espacio-tinta",       "Insertar espacio o unir palabras manuscritas",          "Tinta",             "",             "join split space gesture ink" },
        { CMD_IMPORT_XLSX,              "/importar-excel",      "Importar hoja de c\xC3\xA1lculo Excel (.xlsx)...",      "Interoperabilidad", "",             "import xlsx excel spreadsheet" },
        { CMD_EXPORT_XLSX,              "/exportar-excel",      "Exportar documento a Excel (.xlsx)...",                 "Interoperabilidad", "",             "export xlsx excel spreadsheet" },
        { CMD_EXPORT_CSV,               "/exportar-csv",        "Exportar tablas/datos a CSV...",                        "Interoperabilidad", "",             "export csv table" },
        { CMD_IMPORT_MARKDOWN,          "/importar-markdown",   "Importar documento Markdown (.md)...",                  "Interoperabilidad", "",             "import markdown md" },
        { CMD_EXPORT_MARKDOWN,          "/exportar-markdown",   "Exportar documento y resaltados a Markdown (.md)...",   "Interoperabilidad", "",             "export markdown highlights" },
        { CMD_IMPORT_ONENOTE_HTML,      "/importar-html",       "Importar p\xC3\xA1gina HTML / OneNote HTML...",         "Interoperabilidad", "",             "import html onenote" },
        { CMD_EXPORT_DXF,               "/exportar-dxf",        "Exportar trazos y geometr\xC3\xAD""a a CAD DXF...",     "Interoperabilidad", "",             "export dxf cad autocad" },
        { CMD_TOGGLE_SPLIT_VERTICAL,    "/vista-vertical",      "Alternar Vista Dividida Vertical (arriba / abajo)",     "Vista",             "",             "split view vertical multi note" },
        { CMD_SCAN_RECTIFY_TO_PDF,      "/escanear-rectificar", "Rectificar perspectiva de foto escaneada + OCR...",     "Estudio PDF",       "",             "scan rectify perspective ocr whiteboard" },
        { CMD_GLOBAL_SCREEN_CAPTURE,    "/captura-pantalla",    "Capturar recorte de pantalla del escritorio...",        "Herramientas",      "",             "screen capture screenshot snip" },
        { CMD_AUTO_SYNC_WATCHER,        "/auto-sincronizar",    "Configurar auto-sincronizaci\xC3\xB3n cifrada por carpeta...", "Sincronizaci\xC3\xB3n", "", "auto sync watcher encrypted aes" },
        { CMD_EXPORT_KEYMAP_LOAD_THEME, "/perfil-tema-atajos",  "Exportar atajos o cargar perfil de tema (.json)...",    "Ajustes",           "",             "export keymap load theme file" },
        { CMD_INSERT_VIDEO_CLIP,        "/insertar-video",      "Insertar clip de video reproducible...",                "Bloques",           "",             "video clip mp4 wmv inline player" },
        { CMD_AI_PROVIDERS_VAULT,       "/proveedores-ia",      "Proveedores de IA y B\xC3\xB3veda de Claves API...",   "Ajustes",           "",             "gemini nvidia openai anthropic opencode api key vault" },
        { CMD_A11Y_CENTER,              "/accesibilidad",       "Centro de Accesibilidad Universal...",                  "Accesibilidad",     "",             "accessibility tts dislexia daltonismo regla lectura" },
        { CMD_A11Y_TTS_SPEAK,           "/leer-voz-alta",       "Leer p\xC3\xA1gina o selecci\xC3\xB3n en voz alta",       "Accesibilidad",     "",             "tts speak audio voice math speech" },
        { CMD_A11Y_TTS_STOP,            "/detener-voz",         "Detener lectura en voz alta",                           "Accesibilidad",     "",             "tts stop mute voice" },
        { CMD_A11Y_READING_RULER,       "/regla-lectura",       "Alternar Regla de Lectura Enfocada",                    "Accesibilidad",     "",             "reading ruler focus band guide" },
        { CMD_A11Y_DYSLEXIA_MODE,       "/modo-dislexia",       "Alternar Modo Lectura Amigable (Dislexia)",             "Accesibilidad",     "",             "dyslexia font spacing readability" },
        { CMD_A11Y_CYCLE_TINT,          "/tinte-papel",         "Rotar Tinte de Papel Anti-Fatiga (Sepia/Menta/Lavanda)", "Accesibilidad",   "",             "paper tint sepia mint lavender night eye strain" },
        { CMD_A11Y_CYCLE_COLORBLIND,    "/daltonismo",          "Rotar Paleta Segura para Daltonismo (Okabe-Ito)",       "Accesibilidad",     "",             "colorblind protanopia deuteranopia tritanopia okabe ito" },
        { CMD_STUDY_TUTOR_STEP,         "/tutor",               "Tutor Paso a Paso (Pistas Socr\xC3\xA1ticas / IA)...",  "Estudio",           "",             "tutor socratic step hint study ai" },
        { CMD_STUDY_GEN_FLASHCARDS_AI,  "/generar-tarjetas",    "Generar Tarjetas SM-2 y Pr\xC3\xA1""ctica con Cinta",   "Estudio",           "",             "flashcards sm2 studytape practice generate ai" },
        { CMD_STUDY_CONCEPT_MAP,        "/mapa-conceptual",     "Generar Mapa Conceptual Jer\xC3\xA1rquico en Lienzo...", "Estudio",          "",             "concept mind map connectors canvas" },
        { CMD_STUDY_FORMULA_GLOSSARY,   "/hoja-formulas",       "Compilar Hoja de F\xC3\xB3rmulas y Glosario Autom\xC3\xA1tico", "Estudio",   "",             "formula sheet glossary summary review" },
        { CMD_STUDY_POMODORO,           "/pomodoro",            "Temporizador Pomodoro Inteligente (25/5, 50/10)...",    "Estudio",           "",             "pomodoro timer focus break study" },
        { CMD_AUTO_EXTRACT_TASKS_ICS,   "/extraer-tareas",      "Extraer Tareas y Entregas a Base de Datos / .ics...",   "Automatizaciones",  "",             "tasks deadlines todo ics calendar database" },
        { CMD_AUTO_POST_LECTURE,        "/flujo-post-audio",    "Flujo Post-Audio (Transcribir + Resumir + Tareas)",     "Automatizaciones",  "",             "post audio lecture pipeline summary tasks" },
        { CMD_STUDY_BIBLIOGRAPHY_CITE,  "/citas-bibliografia",  "Generador de Citas Bibliogr\xC3\xA1""ficas (APA 7 / IEEE / BibTeX)...", "Estudio", "", "citation apa ieee bibtex bibliography" },
        // Rich Multimedia (Links, Animated GIF, Native Video, Lazy WebView2 HTML) & Performance
        { CMD_INSERT_LINK_CARD,         "/enlace",              "Insertar tarjeta de enlace web o salto interno...",     "Multimedia",        "",             "link url hyperlink bookmark tarjeta enlace" },
        { CMD_INSERT_GIF,               "/gif",                 "Insertar GIF animado en el lienzo...",                  "Multimedia",        "",             "gif animado image animation" },
        { CMD_INSERT_HTML_EMBED,        "/html",                "Insertar bloque HTML / CSS / JS animado...",            "Multimedia",        "",             "html css js svg canvas embed widget" },
        { CMD_MEDIA_PLAY_PAUSE,         "/reproducir-media",    "Reproducir / Pausar video o GIF seleccionado",          "Multimedia",        "",             "play pause video gif media" },
        { CMD_HTML_FREEZE_RESUME,       "/congelar-html",       "Activar / Congelar bloque HTML (0% GPU)",               "Multimedia",        "",             "freeze suspend resume html" },
        { CMD_MEDIA_CAPTURE_FRAME,      "/capturar-fotograma",  "Capturar fotograma actual de video al lienzo",          "Multimedia",        "",             "capture video frame snapshot annotate" },
        { CMD_TOGGLE_GPU_ACCEL,         "/modo-gpu",            "Alternar modo Ultra Eficiente (0% GPU) / Acelerado",    "Rendimiento",       "",             "gpu cpu performance software battery" },
        { CMD_MEDIA_TRIM_MEMORY,        "/liberar-ram",         "Liberar cach\xC3\xA9s y compactar RAM ahora",           "Rendimiento",       "",             "ram memory trim cache optimize" },
        // Complete STEM, Ink, Paper, PDF, Audio, Layers & Academic Discoverability
        { CMD_ADV_MATH,                 "/lab-matematico",      "Laboratorio Matem\xC3\xA1tico (Eigen / EDO / C\xC3\xA1lculo)...", "STEM", "", "eigen ode integral derivative math" },
        { CMD_ADD_CIRCUIT,              "/circuito",            "Resolver circuito MNA (DC / AC / Diodos)...",           "STEM",              "",             "circuit spice mna resistor capacitor" },
        { CMD_RECOGNIZE_CIRCUIT,        "/reconocer-circuito",  "Reconocer circuito a mano alzada...",                   "STEM",              "",             "handwritten circuit schematic" },
        { CMD_CIRCUIT_TOPO_AUDIT,       "/auditar-circuito",    "Auditor\xC3\xAD""a topol\xC3\xB3gica de circuitos...",  "STEM",              "",             "topology audit floating node" },
        { CMD_CONTROL_BLOCK_LAB,        "/control-pid",         "Diagrama de bloques de control (PID / Routh)...",       "STEM",              "",             "control pid routh transfer function" },
        { CMD_ENG_TOOLBOX,              "/caja-ingenieria",     "Caja de herramientas de ingenier\xC3\xAD""a (RC/Op-Amp/SMD)...", "STEM", "", "rc opamp smd filter engineering" },
        { CMD_ADD_TRANSIENT_LAPLACE,    "/osciloscopio-laplace","Simulaci\xC3\xB3n transitoria + Laplace (s)...",        "STEM",              "",             "transient oscilloscope laplace" },
        { CMD_ADD_KMAP,                 "/karnaugh",            "Simplificar Mapa de Karnaugh...",                       "STEM",              "",             "kmap boolean logic karnaugh" },
        { CMD_ADD_PLOT,                 "/grafica-2d",          "Insertar gr\xC3\xA1""fico 2D / Regresi\xC3\xB3n CSV...", "STEM",             "",             "plot 2d chart regression csv" },
        { CMD_ADD_PLOT_3D,              "/superficie-3d",       "Insertar superficie 3D z = f(x,y) rotable...",          "STEM",              "",             "surface 3d mesh z fxy" },
        { CMD_ADD_PHASOR_DIAG,          "/fasores",             "Diagrama fasorial vectorial (Rect/Polar/Exp)...",       "STEM",              "",             "phasor ac complex polar" },
        { CMD_ADD_BODE_PLOT,            "/bode",                "Diagrama de Bode semilogar\xC3\xADtmico |H(jw)|...",    "STEM",              "",             "bode frequency response db" },
        { CMD_ADD_MATRIX_SEL,           "/matriz-sel",          "Resolver matriz / sistema lineal (SEL)...",             "STEM",              "",             "matrix linear system gauss" },
        { CMD_PARAM_SLIDER,             "/slider-reactivo",     "Insertar deslizador de par\xC3\xA1metro reactivo...",   "STEM",              "",             "parameter slider interactive math" },
        { CMD_CHECK_STEP,               "/verificar-paso",      "Verificar paso algebraico...",                          "STEM",              "",             "check algebra step equivalence" },
        { CMD_OCR_CONFIRM_FORMULA,      "/ocr-formula",         "OCR a f\xC3\xB3rmula 2D con confirmaci\xC3\xB3n...",    "STEM",              "",             "ocr formula confirm math" },
        { CMD_FORMULA_AST_SEARCH,       "/buscar-formula",      "B\xC3\xBAsqueda sem\xC3\xA1ntica de f\xC3\xB3rmulas (AST)...", "STEM",     "",             "ast semantic formula search" },
        { CMD_VAR_INSPECTOR,            "/variables-si",        "Inspector de variables y dimensiones SI...",            "STEM",              "",             "units dimensions variables inspector" },
        { CMD_ADD_CALC_CARD,            "/tarjeta-casio",       "Insertar tarjeta interactiva CASIO...",                 "STEM",              "",             "casio card calculator" },
        { CMD_CASIO_REVERSE_KEYS,       "/teclas-casio",        "Traducir ecuaci\xC3\xB3n a teclas CASIO...",            "STEM",              "",             "casio keys sequence translate" },
        { CMD_COMPARE_4_CASIO,          "/comparar-casio",      "Comparador de 4 modelos CASIO...",                      "STEM",              "",             "compare 4 casio models" },
        { CMD_EXPORT_LAB_HTML,          "/informe-lab-html",    "Exportar memoria de c\xC3\xA1lculo / informe Lab (.html)...", "STEM",      "",             "lab report export html" },
        { CMD_CYCLE_PEN_STYLE,          "/estilo-lapiz",        "Cambiar estilo de l\xC3\xA1piz (Bol\xC3\xADgrafo/Pluma/Pincel/L\xC3\xA1piz)", "Tinta", "", "pen style fountain brush pencil" },
        { CMD_HTR_TO_TEXT,              "/tinta-a-texto",       "Convertir escritura manuscrita a texto editable (HTR)", "Tinta",             "",             "htr handwriting to text" },
        { CMD_HTR_TO_MATH,              "/tinta-a-ecuacion",    "Convertir escritura manuscrita a ecuaci\xC3\xB3n 2D",   "Tinta",             "",             "htr handwriting to math equation" },
        { CMD_PAPER_ISOMETRIC,          "/papel-isometrico",    "Papel isom\xC3\xA9trico 30\xC2\xB0",                    "Papel",             "",             "paper isometric 3d grid" },
        { CMD_PAPER_LOGLOG,             "/papel-loglog",        "Papel logar\xC3\xADtmico Log-Log",                      "Papel",             "",             "paper log log engineering" },
        { CMD_PAPER_POLAR,              "/papel-polar",         "Papel de coordenadas polares",                          "Papel",             "",             "paper polar radar" },
        { CMD_PAPER_SMITH,              "/carta-smith",         "Plantilla Carta de Smith (RF)",                         "Papel",             "",             "smith chart rf paper" },
        { CMD_PAPER_CORNELL,            "/papel-cornell",       "Plantilla de apuntes Cornell",                          "Papel",             "",             "cornell notes study template" },
        { CMD_PAPER_MUSIC,              "/papel-pentagrama",    "Plantilla pentagrama musical",                          "Papel",             "",             "music staff sheet paper" },
        { CMD_TOGGLE_RULER,             "/regla",               "Mostrar / Ocultar regla graduada",                      "Herramientas",      "",             "ruler protractor compass geometry" },
        { CMD_TOGGLE_MINIMAP,           "/minimapa",            "Mostrar / Ocultar minimapa espacial",                   "Vista",             "",             "minimap overview spatial" },
        { CMD_FOCUS_MODE,               "/concentracion",       "Alternar Modo Concentraci\xC3\xB3n (sin distracciones)","Vista",             "",             "focus distraction free zen" },
        { CMD_TOGGLE_SPLIT_VIEW,        "/vista-dividida",      "Alternar Vista Dividida Horizontal (lado a lado)",      "Vista",             "",             "split view side by side" },
        { CMD_TOGGLE_SPREAD,            "/doble-pagina",        "Alternar Vista Doble P\xC3\xA1gina (libro)",            "Vista",             "",             "double page spread book" },
        { CMD_MERGE_PDF,                "/combinar-pdf",        "Combinar varios archivos PDF...",                       "Estudio PDF",       "",             "merge combine pdf" },
        { CMD_SPLIT_PDF,                "/dividir-pdf",         "Dividir o extraer p\xC3\xA1ginas de PDF...",            "Estudio PDF",       "",             "split extract pages pdf" },
        { CMD_SIGN_PDF,                 "/firmar-pdf",          "Firmar PDF digitalmente (PKCS#7)...",                   "Estudio PDF",       "",             "sign digital signature pkcs7" },
        { CMD_RUN_OCR,                  "/ocr-pdf",             "Ejecutar OCR offline sobre el documento",               "Estudio PDF",       "",             "ocr text recognition pdf" },
        { CMD_AUDIO_REC,                "/grabar-audio",        "Grabar / Detener audio sincronizado (Mic)",             "Audio",             "",             "record audio mic timeline" },
        { CMD_REPLAY_TOGGLE,            "/ink-replay",          "Reproducir / Detener Ink Replay + Audio",               "Audio",             "",             "ink replay playback audio" },
        { CMD_AUDIO_TRANSCRIBE,         "/transcribir-audio",   "Transcribir audio grabado",                             "Audio",             "",             "transcribe speech audio voice" },
        { CMD_ADD_TAPE,                 "/cinta-estudio",       "Insertar Cinta de Estudio (Ocultar/Revelar)...",        "Estudio",           "",             "study tape hide reveal active recall" },
        { CMD_ADD_INKLINK,              "/inklink",             "Vincular p\xC3\xA1gina PDF con lienzo (InkLink)",       "Estudio PDF",       "",             "inklink bidireccional pdf canvas" },
        { CMD_STUDY_DECK,               "/repasar-sm2",         "Repasar tarjetas de estudio (SM-2)...",                 "Estudio",           "",             "flashcards sm2 spaced repetition" },
        { CMD_OPEN_PROJECTOR,           "/proyector",           "Abrir ventana de proyector (segundo monitor)",          "Presentaci\xC3\xB3n","",            "projector second monitor class" },
        { CMD_PRESENTER_MODE,           "/presentador",         "Alternar Modo Presentador + Puntero L\xC3\xA1ser",      "Presentaci\xC3\xB3n","",            "presenter laser pointer" },
        { CMD_EXAM_PIN_LOCK,            "/modo-examen",         "Activar / Desbloquear Modo Examen con PIN...",          "Seguridad",         "",             "exam mode pin lock" },
        { CMD_MIGRATE_NOTEBOOK,         "/migrar-cuaderno",     "Importar / Migrar cuaderno (Goodnotes/Notability/OneNote/.tinta)...", "Archivo", "", "migrate goodnotes notability onenote tinta" },
        { CMD_SAVE_ENCRYPTED,           "/guardar-cifrado",     "Guardar cuaderno cifrado (AES-256-GCM)...",             "Seguridad",         "",             "encrypt aes256 save secure" },
        { CMD_OPEN_ENCRYPTED,           "/abrir-cifrado",       "Abrir cuaderno cifrado (AES-256-GCM)...",               "Seguridad",         "",             "decrypt open encrypted aes" },
        { CMD_VERSION_HISTORY,          "/historial-versiones", "Guardar / Ver historial de versiones...",               "Archivo",           "",             "version history snapshot restore" },
        { CMD_ACADEMIC_SYNC_TEC,        "/tec-digital",         "Abrir Centro Acad\xC3\xA9mico TEC Digital (solo lectura)...", "Acad\xC3\xA9mico", "Ctrl+Shift+T", "tec digital academic courses portal" },
        { CMD_ACADEMIC_SOLVE_SCHEDULE,  "/horarios-tec",        "Planificador de Horarios en vivo (HorariosTEC)...",     "Acad\xC3\xA9mico",  "Ctrl+Shift+H", "schedule optimizer horariostec guia horarios" },
        { CMD_ACADEMIC_SEARCH_MATERIALS,"/buscar-tec",          "Biblioteca Offline y b\xC3\xBAsqueda en materiales de cursos...", "Acad\xC3\xA9mico",  "",             "search materials fts offline tec pdf" },
        { CMD_ACADEMIC_MALLA_TEC,       "/malla-tec",           "Malla Curricular interactiva y avance de carrera...",   "Acad\xC3\xA9mico",  "",             "malla curricular plan estudios requisitos tec" },
        { CMD_ACADEMIC_NOTAS_TEC,       "/notas-tec",           "Expediente, estad\xC3\xADsticas y registro de notas...", "Acad\xC3\xA9mico",  "",             "expediente notas promedio ponderado citas matricula" },
        { CMD_ACADEMIC_CALENDAR_TEC,    "/calendario-tec",      "Calendario unificado de entregas, clases y citas...",   "Acad\xC3\xA9mico",  "",             "calendario entregas examenes ics tec" },
        { CMD_ACADEMIC_EVALS_TEC,       "/evaluaciones-tec",    "Evaluaciones, r\xC3\xBA" "bricas y simulador de nota...", "Acad\xC3\xA9mico",  "",             "evaluaciones rubricas simulador que nota necesito" },
        { CMD_ACADEMIC_PLAGIO_TEC,      "/plagio-tec",          "Tr\xC3\xA1mites, control de plagio y servicios TEC...",   "Acad\xC3\xA9mico",  "",             "plagio turnitin prematricula tramites servicios tec" },
        { CMD_INSERT_IMAGE,             "/insertar-imagen",     "Insertar imagen (PNG/JPG/GIF/SVG/WEBP/TIFF)...",        "Multimedia",        "",             "insert image picture photo" },
        { CMD_NEW_TAB,                  "/nueva-pestana",       "Abrir nueva pesta\xC3\xB1""a de cuaderno",              "Archivo",           "Ctrl+T",       "new tab notebook" },
        { CMD_QUICK_NOTE,               "/nota-rapida",         "Crear nota r\xC3\xA1pida instant\xC3\xA1nea",           "Archivo",           "Ctrl+Shift+N", "quick note instant" },
        // Google Gemini Notebook (Opcional y Desacoplado) + Enlaces Inline en Texto
        { CMD_GEMINI_NOTEBOOK_HUB,      "/gemini-notebook",     "Abrir centro de sincronizaci\xC3\xB3n Gemini Notebook...", "Estudio",          "",             "gemini notebook notebooklm sync fuentes cuadernos" },
        { CMD_GEMINI_NOTEBOOK_CONNECT,  "/conectar-notebook",   "Conectar cuenta personal de Google para Gemini Notebook...", "Ajustes",       "",             "google oauth cuenta personal gemini notebook" },
        { CMD_GEMINI_NOTEBOOK_SYNC_TEC, "/tec-a-notebook",      "Sincronizar curso o carpeta de TEC Digital a Gemini Notebook...", "Acad\xC3\xA9mico", "", "tec digital cursos capitulos sincronizar gemini notebook" },
        { CMD_GEMINI_NOTEBOOK_SEND_DOC, "/enviar-cuaderno",     "Enviar cuaderno actual a Gemini Notebook...",           "Estudio",           "",             "publicar exportar cuaderno markdown gemini notebook" },
        { CMD_GEMINI_NOTEBOOK_SEND_URL, "/enviar-enlace",       "Enviar enlace web o YouTube seleccionado a Gemini Notebook...", "Multimedia",   "",             "url youtube fuente enlace gemini notebook" },
        { CMD_GEMINI_NOTEBOOK_MCP_CONFIG,"/agentes-notebook",   "Configurar conexi\xC3\xB3n para agentes de IA (MCP)...", "Ajustes",          "",             "mcp agentes antigravity codex claude configuracion" },
        { CMD_GEMINI_NOTEBOOK_OPEN_WEB, "/abrir-notebook-web",  "Abrir cuaderno de Gemini Notebook en el navegador",     "Estudio",           "",             "abrir web notebooklm audio overview" },
        { CMD_TEXTBOX_URL_TO_LINKCARD,  "/convertir-enlace",    "Convertir enlace de texto en tarjeta interactiva",      "Multimedia",        "",             "textbox url link card convertir enlace" }
    };
}

void CommandPaletteRegistry::Register(CommandPaletteEntry entry) {
    for (auto& existing : entries_) {
        if (existing.commandId == entry.commandId && entry.commandId != 0) {
            existing = std::move(entry);
            return;
        }
    }
    entries_.push_back(std::move(entry));
}

std::vector<CommandPaletteEntry> CommandPaletteRegistry::Search(const std::string& queryUtf8, std::size_t maxResults) const {
    const std::string q = Trim(queryUtf8);
    if (q.empty() || q == "/") {
        std::vector<CommandPaletteEntry> res = entries_;
        if (res.size() > maxResults) res.resize(maxResults);
        return res;
    }

    const bool slashMode = (q.front() == '/');
    const std::string needle = FoldUtf8ForSearch(q, false, true);
    const std::string needleNoSlash = FoldUtf8ForSearch(slashMode ? q.substr(1) : q, false, true);

    struct Scored {
        int score = 0;
        CommandPaletteEntry entry;
    };
    std::vector<Scored> scored;

    for (const auto& e : entries_) {
        const std::string fSlash = FoldUtf8ForSearch(e.slashTriggerUtf8, false, true);
        const std::string fTitle = FoldUtf8ForSearch(e.titleUtf8, false, true);
        const std::string fCat = FoldUtf8ForSearch(e.categoryUtf8, false, true);
        const std::string fKw = FoldUtf8ForSearch(e.keywordsUtf8, false, true);

        int s = 0;
        if (fSlash == needle) s = 200;
        else if (!fSlash.empty() && fSlash.find(needle) == 0) s = 140;
        else if (!needleNoSlash.empty() && !fSlash.empty() && fSlash.find(needleNoSlash) != std::string::npos) s = 110;
        else if (!slashMode && fTitle.find(needle) == 0) s = 95;
        else if (fTitle.find(needleNoSlash) != std::string::npos) s = 80;
        else if (fCat.find(needleNoSlash) != std::string::npos) s = 55;
        else if (fKw.find(needleNoSlash) != std::string::npos) s = 40;

        if (s > 0) scored.push_back({ s, e });
    }

    std::sort(scored.begin(), scored.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.entry.titleUtf8 < b.entry.titleUtf8;
    });

    std::vector<CommandPaletteEntry> out;
    out.reserve((std::min)(maxResults, scored.size()));
    for (std::size_t i = 0; i < scored.size() && i < maxResults; ++i) {
        out.push_back(std::move(scored[i].entry));
    }
    return out;
}

// ---------------- ThemeProfileRegistry ----------------

std::vector<ThemeProfile> ThemeProfileRegistry::BuiltInProfiles() {
    return {
        { "porcelana-claro", "Porcelana Claro", false, false,
          DocColor::RGBA(246, 248, 252, 255), DocColor::RGBA(255, 255, 255, 255),
          DocColor::RGBA(37, 99, 235, 255), DocColor::RGBA(24, 30, 42, 255),
          "body{background:#F6F8FC;color:#181E2A;}" },
        { "pizarra-oscura", "Pizarra Oscura", true, false,
          DocColor::RGBA(22, 26, 36, 255), DocColor::RGBA(30, 36, 48, 255),
          DocColor::RGBA(96, 165, 250, 255), DocColor::RGBA(238, 242, 250, 255),
          "body{background:#161A24;color:#EEF2FA;}" },
        { "alto-contraste", "Alto Contraste", false, true,
          DocColor::RGBA(255, 255, 255, 255), DocColor::RGBA(255, 255, 255, 255),
          DocColor::RGBA(0, 64, 221, 255), DocColor::RGBA(0, 0, 0, 255),
          "body{background:#FFFFFF;color:#000000;}" },
        { "papel-calido", "Papel Acad\xC3\xA9mico C\xC3\xA1lido", false, false,
          DocColor::RGBA(250, 246, 236, 255), DocColor::RGBA(255, 253, 248, 255),
          DocColor::RGBA(180, 83, 9, 255), DocColor::RGBA(41, 37, 36, 255),
          "body{background:#FAF6EC;color:#292524;}" }
    };
}

bool ThemeProfileRegistry::SaveProfileToFile(const ThemeProfile& prof, const std::wstring& path, std::string* outErr) {
    json j = {
        {"id", prof.idUtf8},
        {"displayName", prof.displayNameUtf8},
        {"dark", prof.dark},
        {"highContrast", prof.highContrast},
        {"canvasBg", prof.canvasBg.PackRgba()},
        {"cardBg", prof.cardBg.PackRgba()},
        {"accent", prof.accent.PackRgba()},
        {"inkDefault", prof.inkDefault.PackRgba()},
        {"cssSnippet", prof.cssSnippetUtf8}
    };
    return WriteTextFile(fs::path(WideToUtf8(path)), j.dump(2), outErr);
}

bool ThemeProfileRegistry::LoadProfileFromFile(const std::wstring& path, ThemeProfile& outProf, std::string* outErr) {
    std::string raw;
    if (!ReadTextFile(fs::path(WideToUtf8(path)), raw, outErr)) return false;
    try {
        const json j = json::parse(raw);
        outProf.idUtf8 = j.value("id", "custom");
        outProf.displayNameUtf8 = j.value("displayName", "Tema Personalizado");
        outProf.dark = j.value("dark", false);
        outProf.highContrast = j.value("highContrast", false);
        outProf.canvasBg = DocColor::UnpackRgba(j.value("canvasBg", 0xF6F8FCFFu));
        outProf.cardBg = DocColor::UnpackRgba(j.value("cardBg", 0xFFFFFFFFu));
        outProf.accent = DocColor::UnpackRgba(j.value("accent", 0x2563EBFFu));
        outProf.inkDefault = DocColor::UnpackRgba(j.value("inkDefault", 0x181E2AFFu));
        outProf.cssSnippetUtf8 = j.value("cssSnippet", "");
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

// ---------------- KnowledgeVault ----------------

std::string KnowledgeVault::CreateNote(
    const std::string& titleUtf8,
    const std::string& folderPathUtf8,
    const std::string& bodyMarkdownUtf8,
    const std::vector<NoteProperty>& properties,
    PaperPattern paper,
    std::uint32_t coverRgba)
{
    VaultNoteRecord rec;
    rec.noteIdUtf8 = "note_" + std::to_string(nextSeq_++);
    rec.titleUtf8 = titleUtf8.empty() ? ("Nota " + rec.noteIdUtf8) : titleUtf8;
    rec.folderPathUtf8 = folderPathUtf8;
    rec.bodyMarkdownUtf8 = bodyMarkdownUtf8;
    rec.properties = properties;
    if (bodyMarkdownUtf8.rfind("---", 0) == 0) {
        std::vector<NoteProperty> fmProps;
        std::vector<std::string> fmAliases, fmTags;
        std::string strippedBody;
        if (ParseFrontmatterAndBody(bodyMarkdownUtf8, fmProps, fmAliases, fmTags, strippedBody)) {
            rec.bodyMarkdownUtf8 = std::move(strippedBody);
            for (auto& fp : fmProps) rec.properties.push_back(std::move(fp));
            for (auto& fa : fmAliases) rec.aliasesUtf8.push_back(std::move(fa));
            for (auto& ft : fmTags) rec.tagsUtf8.push_back(std::move(ft));
        }
    }
    rec.paperPattern = paper;
    rec.coverColorRgba = coverRgba;
    rec.updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));

    for (const auto& p : rec.properties) {
        if (EqualsFolded(p.keyUtf8, "tags") || EqualsFolded(p.keyUtf8, "etiquetas")) {
            for (const auto& tv : p.listValuesUtf8) rec.tagsUtf8.push_back(tv);
        } else if (EqualsFolded(p.keyUtf8, "aliases")) {
            for (const auto& av : p.listValuesUtf8) rec.aliasesUtf8.push_back(av);
        }
    }
    notes_.push_back(std::move(rec));
    return notes_.back().noteIdUtf8;
}

VaultNoteRecord* KnowledgeVault::FindNoteMutableById(const std::string& noteIdUtf8) {
    for (auto& n : notes_) {
        if (n.noteIdUtf8 == noteIdUtf8) return &n;
    }
    return nullptr;
}

const VaultNoteRecord* KnowledgeVault::FindNoteById(const std::string& noteIdUtf8) const {
    for (const auto& n : notes_) {
        if (n.noteIdUtf8 == noteIdUtf8) return &n;
    }
    return nullptr;
}

const VaultNoteRecord* KnowledgeVault::FindNoteByTitleOrAlias(const std::string& titleOrAliasUtf8) const {
    if (Trim(titleOrAliasUtf8).empty()) return nullptr;
    for (const auto& n : notes_) {
        if (n.inTrash) continue;
        if (EqualsFolded(n.titleUtf8, titleOrAliasUtf8)) return &n;
        for (const auto& al : n.aliasesUtf8) {
            if (EqualsFolded(al, titleOrAliasUtf8)) return &n;
        }
    }
    return nullptr;
}

std::vector<VaultNoteRecord> KnowledgeVault::ActiveNotes() const {
    std::vector<VaultNoteRecord> out;
    for (const auto& n : notes_) {
        if (!n.inTrash) out.push_back(n);
    }
    return out;
}

std::size_t KnowledgeVault::TotalNoteCount(bool includeTrash) const {
    if (includeTrash) return notes_.size();
    std::size_t c = 0;
    for (const auto& n : notes_) if (!n.inTrash) ++c;
    return c;
}

bool KnowledgeVault::MoveNote(const std::string& noteIdUtf8, const std::string& dstFolderPathUtf8) {
    VaultNoteRecord* n = FindNoteMutableById(noteIdUtf8);
    if (!n) return false;
    n->folderPathUtf8 = dstFolderPathUtf8;
    n->updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));
    return true;
}

bool KnowledgeVault::MoveToTrash(const std::string& noteIdUtf8) {
    VaultNoteRecord* n = FindNoteMutableById(noteIdUtf8);
    if (!n || n->inTrash) return false;
    n->inTrash = true;
    return true;
}

bool KnowledgeVault::RestoreFromTrash(const std::string& noteIdUtf8) {
    VaultNoteRecord* n = FindNoteMutableById(noteIdUtf8);
    if (!n || !n->inTrash) return false;
    n->inTrash = false;
    return true;
}

bool KnowledgeVault::SetNoteProperty(const std::string& noteIdUtf8, const NoteProperty& prop) {
    VaultNoteRecord* n = FindNoteMutableById(noteIdUtf8);
    if (!n || prop.keyUtf8.empty()) return false;
    for (auto& p : n->properties) {
        if (EqualsFolded(p.keyUtf8, prop.keyUtf8)) {
            p = prop;
            n->updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));
            return true;
        }
    }
    n->properties.push_back(prop);
    n->updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));
    return true;
}

bool KnowledgeVault::SetNoteCoverAndPaper(const std::string& noteIdUtf8, std::uint32_t coverRgba, PaperPattern paper) {
    VaultNoteRecord* n = FindNoteMutableById(noteIdUtf8);
    if (!n) return false;
    n->coverColorRgba = coverRgba;
    n->paperPattern = paper;
    n->updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));
    return true;
}

void KnowledgeVault::RegisterTemplate(NoteTemplate tpl) {
    for (auto& t : templates_) {
        if (EqualsFolded(t.nameUtf8, tpl.nameUtf8)) {
            t = std::move(tpl);
            return;
        }
    }
    templates_.push_back(std::move(tpl));
}

std::string KnowledgeVault::ExpandTemplateVariables(
    const std::string& rawTemplateUtf8,
    const std::string& titleUtf8,
    const std::string& dateIsoUtf8,
    const std::string& timeHhMmUtf8,
    const std::string& uuidUtf8)
{
    std::string out = rawTemplateUtf8;
    out = ReplaceAllCopy(std::move(out), "{{title}}", titleUtf8);
    out = ReplaceAllCopy(std::move(out), "{{date}}", dateIsoUtf8);
    out = ReplaceAllCopy(std::move(out), "{{time}}", timeHhMmUtf8);
    out = ReplaceAllCopy(std::move(out), "{{uuid}}", uuidUtf8);
    return out;
}

std::string KnowledgeVault::CreateNoteFromTemplate(
    const std::string& templateNameUtf8,
    const std::string& titleUtf8,
    const std::string& folderPathUtf8,
    const std::string& dateIsoOverrideUtf8,
    const std::string& timeOverrideUtf8)
{
    const NoteTemplate* tpl = nullptr;
    for (const auto& t : templates_) {
        if (EqualsFolded(t.nameUtf8, templateNameUtf8)) { tpl = &t; break; }
    }
    std::string dateIso = dateIsoOverrideUtf8;
    std::string timeStr = timeOverrideUtf8;
    if (dateIso.empty() || timeStr.empty()) {
        std::time_t now = std::time(nullptr);
        std::tm tmNow{};
#ifdef _WIN32
        localtime_s(&tmNow, &now);
#else
        localtime_r(&now, &tmNow);
#endif
        char dBuf[16]{};
        char tBuf[16]{};
        std::strftime(dBuf, sizeof(dBuf), "%Y-%m-%d", &tmNow);
        std::strftime(tBuf, sizeof(tBuf), "%H:%M", &tmNow);
        if (dateIso.empty()) dateIso = dBuf;
        if (timeStr.empty()) timeStr = tBuf;
    }

    const std::string uuid = DocumentModel::GenerateDurableUuid();
    const std::string body = tpl
        ? ExpandTemplateVariables(tpl->contentTemplateUtf8, titleUtf8, dateIso, timeStr, uuid)
        : ("# " + titleUtf8 + "\n");
    const PaperPattern paper = tpl ? tpl->paperPattern : PaperPattern::Plain;
    const std::uint32_t cover = tpl ? tpl->coverColorRgba : 0x1D4ED8FFu;
    std::vector<NoteProperty> props = tpl ? tpl->defaultProps : std::vector<NoteProperty>{};
    props.push_back({ "fecha", NotePropType::Date, dateIso, {} });

    return CreateNote(titleUtf8, folderPathUtf8, body, std::move(props), paper, cover);
}

std::string KnowledgeVault::OpenOrCreateDailyNote(
    const std::string& dateIsoUtf8,
    const std::string& templateNameUtf8)
{
    for (const auto& n : notes_) {
        if (!n.inTrash && (n.dailyDateIsoUtf8 == dateIsoUtf8 || EqualsFolded(n.titleUtf8, dateIsoUtf8))) {
            return n.noteIdUtf8;
        }
    }

    // Find latest existing daily note for automatic chronological prev link
    std::string prevDailyTitle;
    for (auto it = notes_.rbegin(); it != notes_.rend(); ++it) {
        if (!it->inTrash && it->isDailyNote) {
            prevDailyTitle = it->titleUtf8;
            break;
        }
    }

    std::string id;
    if (!templateNameUtf8.empty()) {
        id = CreateNoteFromTemplate(templateNameUtf8, dateIsoUtf8, "Diarias", dateIsoUtf8, "09:00");
    } else {
        std::ostringstream oss;
        oss << "# Nota Diaria " << dateIsoUtf8 << "\n\n";
        if (!prevDailyTitle.empty()) oss << "Anterior: [[" << prevDailyTitle << "]]\n\n";
        oss << "## Tareas del d\xC3\xAD""a\n- [ ] Revisi\xC3\xB3n\n";
        id = CreateNote(dateIsoUtf8, "Diarias", oss.str(), { { "tipo", NotePropType::Text, "diaria", {} } }, PaperPattern::Ruled, 0x0F766EFFu);
    }
    if (VaultNoteRecord* rec = FindNoteMutableById(id)) {
        rec->isDailyNote = true;
        rec->dailyDateIsoUtf8 = dateIsoUtf8;
        if (!prevDailyTitle.empty() && rec->bodyMarkdownUtf8.find("[[" + prevDailyTitle + "]]") == std::string::npos) {
            rec->bodyMarkdownUtf8 += "\n\nAnterior: [[" + prevDailyTitle + "]]";
        }
    }
    return id;
}

bool KnowledgeVault::UpdateSyncedBlock(
    const std::string& syncIdUtf8,
    const std::string& newInnerTextUtf8,
    std::size_t* outNotesTouched)
{
    if (outNotesTouched) *outNotesTouched = 0;
    if (syncIdUtf8.empty()) return false;
    const std::string openFence = ":::sync[" + syncIdUtf8 + "]";
    std::size_t touched = 0;

    for (auto& n : notes_) {
        std::size_t pos = 0;
        bool changed = false;
        while ((pos = n.bodyMarkdownUtf8.find(openFence, pos)) != std::string::npos) {
            const std::size_t lineEnd = n.bodyMarkdownUtf8.find('\n', pos);
            if (lineEnd == std::string::npos) break;
            const std::size_t closePos = n.bodyMarkdownUtf8.find("\n:::", lineEnd);
            if (closePos == std::string::npos) break;
            const std::string replacement = "\n" + newInnerTextUtf8;
            n.bodyMarkdownUtf8.replace(lineEnd, closePos - lineEnd, replacement);
            pos = lineEnd + replacement.size() + 4;
            changed = true;
        }
        if (changed) {
            ++touched;
            n.updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));
        }
    }
    if (outNotesTouched) *outNotesTouched = touched;
    return touched > 0;
}

std::string KnowledgeVault::ResolveTransclusionMarkdown(const std::string& targetExprUtf8, int maxDepth) const {
    if (maxDepth <= 0) return "[Transclusi\xC3\xB3n c\xC3\xAD""clica detenida]";
    std::string title, heading, blockId, alias;
    SplitTargetComponents(targetExprUtf8, title, heading, blockId, alias);
    const VaultNoteRecord* target = FindNoteByTitleOrAlias(title);
    if (!target) return "[Nota no encontrada: " + title + "]";

    std::string fragment;
    if (!blockId.empty()) {
        std::istringstream iss(target->bodyMarkdownUtf8);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.find("^" + blockId) != std::string::npos) {
                fragment = ReplaceAllCopy(line, "^" + blockId, "");
                fragment = Trim(fragment);
                break;
            }
        }
    } else if (!heading.empty()) {
        std::istringstream iss(target->bodyMarkdownUtf8);
        std::string line;
        bool capturing = false;
        int capturedLevel = 6;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const std::string tr = Trim(line);
            int hl = 0;
            while (hl < static_cast<int>(tr.size()) && hl < 6 && tr[hl] == '#') ++hl;
            const bool isH = (hl > 0 && hl < static_cast<int>(tr.size()) && tr[hl] == ' ');
            if (isH) {
                const std::string hTitle = Trim(tr.substr(hl + 1));
                if (!capturing && EqualsFolded(hTitle, heading)) {
                    capturing = true;
                    capturedLevel = hl;
                    fragment = tr;
                    continue;
                }
                if (capturing && hl <= capturedLevel) break;
            }
            if (capturing) {
                if (!fragment.empty()) fragment.push_back('\n');
                fragment += line;
            }
        }
    } else {
        fragment = target->bodyMarkdownUtf8;
    }

    // Expand nested transclusions recursively up to maxDepth - 1
    std::size_t pos = 0;
    while ((pos = fragment.find("![[", pos)) != std::string::npos) {
        const auto end = fragment.find("]]", pos + 3);
        if (end == std::string::npos) break;
        const std::string nestedExpr = fragment.substr(pos + 3, end - (pos + 3));
        const std::string expanded = ResolveTransclusionMarkdown(nestedExpr, maxDepth - 1);
        fragment.replace(pos, (end + 2) - pos, expanded);
        pos += expanded.size();
    }
    return Trim(fragment);
}

std::vector<VaultNoteRecord> KnowledgeVault::FilterNotes(const NoteFilterCriteria& criteria) const {
    std::vector<VaultNoteRecord> out;
    for (const auto& n : notes_) {
        if (!criteria.includeTrash && n.inTrash) continue;
        if (!criteria.folderPrefixUtf8.empty() && n.folderPathUtf8.find(criteria.folderPrefixUtf8) != 0) continue;
        if (!criteria.requiredTagUtf8.empty()) {
            bool hasTag = false;
            for (const auto& t : n.tagsUtf8) {
                if (EqualsFolded(t, criteria.requiredTagUtf8)) { hasTag = true; break; }
            }
            if (!hasTag) continue;
        }
        if (!criteria.propKeyUtf8.empty()) {
            bool propOk = false;
            for (const auto& p : n.properties) {
                if (!EqualsFolded(p.keyUtf8, criteria.propKeyUtf8)) continue;
                if (criteria.propOpUtf8 == "contains") {
                    propOk = ContainsFolded(p.valueUtf8, criteria.propValueUtf8);
                    for (const auto& lv : p.listValuesUtf8) {
                        if (ContainsFolded(lv, criteria.propValueUtf8)) propOk = true;
                    }
                } else if (criteria.propOpUtf8 == ">=" || criteria.propOpUtf8 == "<=") {
                    try {
                        const double pv = std::stod(p.valueUtf8);
                        const double cv = std::stod(criteria.propValueUtf8);
                        propOk = (criteria.propOpUtf8 == ">=") ? (pv >= cv) : (pv <= cv);
                    } catch (...) {
                        propOk = (criteria.propOpUtf8 == ">=")
                            ? (p.valueUtf8 >= criteria.propValueUtf8)
                            : (p.valueUtf8 <= criteria.propValueUtf8);
                    }
                } else {
                    propOk = EqualsFolded(p.valueUtf8, criteria.propValueUtf8);
                }
                break;
            }
            if (!propOk) continue;
        }
        out.push_back(n);
    }
    return out;
}

std::vector<KnowledgeSearchHit> KnowledgeVault::SearchWorkspace(const std::string& queryUtf8, std::size_t maxResults) const {
    const std::string q = FoldUtf8ForSearch(Trim(queryUtf8), false, true);
    if (q.empty()) return {};
    std::vector<KnowledgeSearchHit> hits;

    for (const auto& n : notes_) {
        if (n.inTrash) continue;
        int score = 0;
        if (ContainsFolded(n.titleUtf8, q)) score += 20;
        for (const auto& al : n.aliasesUtf8) if (ContainsFolded(al, q)) score += 12;
        for (const auto& t : n.tagsUtf8) if (ContainsFolded(t, q)) score += 10;
        for (const auto& p : n.properties) {
            if (ContainsFolded(p.keyUtf8, q) || ContainsFolded(p.valueUtf8, q)) score += 8;
        }
        const std::string foldedBody = FoldUtf8ForSearch(n.bodyMarkdownUtf8, false, true);
        const auto pos = foldedBody.find(q);
        std::string snippet;
        if (pos != std::string::npos) {
            score += 10;
            const std::size_t start = (pos > 36) ? (pos - 36) : 0;
            const std::size_t len = (std::min<std::size_t>)(120, n.bodyMarkdownUtf8.size() > start ? (n.bodyMarkdownUtf8.size() - start) : 0);
            snippet = ReplaceAllCopy(n.bodyMarkdownUtf8.substr(start, len), "\n", " ");
        } else if (score > 0) {
            snippet = ReplaceAllCopy(n.bodyMarkdownUtf8.substr(0, (std::min<std::size_t>)(120, n.bodyMarkdownUtf8.size())), "\n", " ");
        }
        if (score > 0) {
            hits.push_back({ n.noteIdUtf8, n.titleUtf8, n.folderPathUtf8, snippet, score });
        }
    }

    std::sort(hits.begin(), hits.end(), [](const KnowledgeSearchHit& a, const KnowledgeSearchHit& b) {
        if (a.score != b.score) return a.score > b.score;
        return a.titleUtf8 < b.titleUtf8;
    });
    if (hits.size() > maxResults) hits.resize(maxResults);
    return hits;
}

std::vector<KnowledgeLink> KnowledgeVault::ExtractLinksFromNote(const VaultNoteRecord& note) const {
    std::vector<KnowledgeLink> links;
    const std::string& txt = note.bodyMarkdownUtf8;
    std::size_t i = 0;
    const std::size_t n = txt.size();

    while (i < n) {
        // WikiLink or Transclusion ![[...]] / [[...]]
        const bool isTrans = (txt[i] == '!' && i + 2 < n && txt[i + 1] == '[' && txt[i + 2] == '[');
        const bool isWiki = (!isTrans && i + 1 < n && txt[i] == '[' && txt[i + 1] == '[');
        if (isTrans || isWiki) {
            const std::size_t openOffset = i;
            const std::size_t contentStart = i + (isTrans ? 3u : 2u);
            const std::size_t closePos = txt.find("]]", contentStart);
            if (closePos != std::string::npos && txt.find('\n', contentStart) >= closePos) {
                const std::string inner = txt.substr(contentStart, closePos - contentStart);
                KnowledgeLink lk;
                lk.sourceNoteIdUtf8 = note.noteIdUtf8;
                lk.sourceNoteTitleUtf8 = note.titleUtf8;
                lk.targetRawUtf8 = inner;
                SplitTargetComponents(inner, lk.targetNoteTitleUtf8, lk.targetHeadingUtf8, lk.targetBlockIdUtf8, lk.aliasUtf8);
                if (lk.targetNoteTitleUtf8.empty()) lk.targetNoteTitleUtf8 = note.titleUtf8;
                lk.lineIndex0 = 0;
                lk.contextSnippetUtf8 = LineContextAt(txt, openOffset, lk.lineIndex0);
                lk.byteOffset = openOffset;
                lk.byteLength = (closePos + 2) - openOffset;
                if (isTrans) lk.kind = KnowledgeLinkKind::Transclusion;
                else if (!lk.targetBlockIdUtf8.empty()) lk.kind = KnowledgeLinkKind::WikiBlock;
                else if (!lk.targetHeadingUtf8.empty()) lk.kind = KnowledgeLinkKind::WikiHeading;
                else lk.kind = KnowledgeLinkKind::WikiNote;
                links.push_back(std::move(lk));
                i = closePos + 2;
                continue;
            }
        }

        // Markdown link [label](url-or-scheme)
        if (txt[i] == '[' && !(i + 1 < n && txt[i + 1] == '[') && !(i > 0 && txt[i - 1] == '[')) {
            const std::size_t rb = txt.find("](", i + 1);
            if (rb != std::string::npos && txt.find('\n', i) > rb) {
                const std::size_t rp = txt.find(')', rb + 2);
                if (rp != std::string::npos && txt.find('\n', rb) > rp) {
                    const std::string lbl = txt.substr(i + 1, rb - (i + 1));
                    const std::string url = Trim(txt.substr(rb + 2, rp - (rb + 2)));
                    KnowledgeLink lk;
                    lk.sourceNoteIdUtf8 = note.noteIdUtf8;
                    lk.sourceNoteTitleUtf8 = note.titleUtf8;
                    lk.targetRawUtf8 = url;
                    lk.aliasUtf8 = lbl;
                    lk.byteOffset = i;
                    lk.byteLength = (rp + 1) - i;
                    lk.contextSnippetUtf8 = LineContextAt(txt, i, lk.lineIndex0);

                    if (url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0) {
                        lk.kind = KnowledgeLinkKind::ExternalUrl;
                        lk.targetNoteTitleUtf8 = url;
                    } else if (url.rfind("cudoc://page/", 0) == 0) {
                        lk.kind = KnowledgeLinkKind::InternalPage;
                        lk.targetNoteTitleUtf8 = url;
                    } else if (url.rfind("cudoc://section/", 0) == 0) {
                        lk.kind = KnowledgeLinkKind::InternalSection;
                        lk.targetNoteTitleUtf8 = url.substr(16);
                    } else if (url.rfind("office://", 0) == 0 || url.rfind("file://", 0) == 0 ||
                               url.find(".docx") != std::string::npos || url.find(".pptx") != std::string::npos ||
                               url.find(".xlsx") != std::string::npos) {
                        lk.kind = KnowledgeLinkKind::OfficeFileRef;
                        lk.targetNoteTitleUtf8 = url;
                    } else {
                        std::string dummyAlias;
                        SplitTargetComponents(url, lk.targetNoteTitleUtf8, lk.targetHeadingUtf8, lk.targetBlockIdUtf8, dummyAlias);
                        lk.kind = KnowledgeLinkKind::WikiNote;
                    }
                    links.push_back(std::move(lk));
                    i = rp + 1;
                    continue;
                }
            }
        }
        ++i;
    }
    return links;
}

std::vector<KnowledgeLink> KnowledgeVault::CollectAllLinks() const {
    std::vector<KnowledgeLink> all;
    for (const auto& n : notes_) {
        if (n.inTrash) continue;
        auto lks = ExtractLinksFromNote(n);
        all.insert(all.end(), std::make_move_iterator(lks.begin()), std::make_move_iterator(lks.end()));
    }
    return all;
}

std::vector<KnowledgeLink> KnowledgeVault::OutgoingLinks(const std::string& noteIdOrTitleUtf8) const {
    const VaultNoteRecord* src = FindNoteById(noteIdOrTitleUtf8);
    if (!src) src = FindNoteByTitleOrAlias(noteIdOrTitleUtf8);
    if (!src || src->inTrash) return {};
    return ExtractLinksFromNote(*src);
}

std::vector<KnowledgeLink> KnowledgeVault::Backlinks(const std::string& targetTitleOrAliasUtf8) const {
    const VaultNoteRecord* target = FindNoteByTitleOrAlias(targetTitleOrAliasUtf8);
    std::vector<std::string> names{ targetTitleOrAliasUtf8 };
    if (target) {
        names = { target->titleUtf8 };
        for (const auto& al : target->aliasesUtf8) names.push_back(al);
    }

    std::vector<KnowledgeLink> out;
    for (const auto& lk : CollectAllLinks()) {
        if (lk.kind == KnowledgeLinkKind::ExternalUrl || lk.kind == KnowledgeLinkKind::OfficeFileRef) continue;
        for (const auto& nm : names) {
            if (EqualsFolded(lk.targetNoteTitleUtf8, nm)) {
                out.push_back(lk);
                break;
            }
        }
    }
    return out;
}

std::vector<UnlinkedMention> KnowledgeVault::FindUnlinkedMentions(const std::string& targetTitleUtf8) const {
    std::vector<UnlinkedMention> out;
    const std::string targetTrim = Trim(targetTitleUtf8);
    if (targetTrim.size() < 2) return out;
    const VaultNoteRecord* targetNote = FindNoteByTitleOrAlias(targetTrim);

    for (const auto& src : notes_) {
        if (src.inTrash) continue;
        if (EqualsFolded(src.titleUtf8, targetTrim)) continue;
        if (targetNote && src.noteIdUtf8 == targetNote->noteIdUtf8) continue;

        const auto linked = ExtractLinksFromNote(src);
        auto isInsideExistingLink = [&](std::size_t off, std::size_t len) -> bool {
            const std::size_t endOff = off + len;
            for (const auto& lk : linked) {
                const std::size_t lkEnd = lk.byteOffset + lk.byteLength;
                if (!(endOff <= lk.byteOffset || off >= lkEnd)) return true;
            }
            return false;
        };

        std::vector<std::string> terms{ targetTrim };
        if (targetNote) {
            for (const auto& al : targetNote->aliasesUtf8) {
                if (al.size() >= 2) terms.push_back(al);
            }
        }

        const std::string& raw = src.bodyMarkdownUtf8;
        const std::string lowRaw = FoldUtf8ForSearch(raw, false, false);
        for (const auto& term : terms) {
            const std::string lowTerm = FoldUtf8ForSearch(term, false, false);
            if (lowTerm.empty() || lowRaw.size() != raw.size()) continue;
            std::size_t pos = 0;
            while ((pos = lowRaw.find(lowTerm, pos)) != std::string::npos) {
                const std::size_t endPos = pos + lowTerm.size();
                const bool leftOk = (pos == 0) || !IsWordBoundaryChar(static_cast<unsigned char>(lowRaw[pos - 1]));
                const bool rightOk = (endPos >= lowRaw.size()) || !IsWordBoundaryChar(static_cast<unsigned char>(lowRaw[endPos]));
                if (leftOk && rightOk && !isInsideExistingLink(pos, lowTerm.size())) {
                    std::size_t ln = 0;
                    out.push_back({
                        src.noteIdUtf8,
                        src.titleUtf8,
                        targetTrim,
                        raw.substr(pos, lowTerm.size()),
                        pos,
                        lowTerm.size(),
                        LineContextAt(raw, pos, ln)
                    });
                }
                pos = endPos;
            }
        }
    }
    return out;
}

bool KnowledgeVault::ConvertMentionToWikiLink(
    const std::string& sourceNoteIdUtf8,
    std::size_t byteOffset,
    std::size_t byteLength,
    const std::string& targetTitleUtf8)
{
    VaultNoteRecord* src = FindNoteMutableById(sourceNoteIdUtf8);
    if (!src || byteOffset + byteLength > src->bodyMarkdownUtf8.size()) return false;
    const std::string original = src->bodyMarkdownUtf8.substr(byteOffset, byteLength);
    const std::string replacement = EqualsFolded(original, targetTitleUtf8)
        ? ("[[" + targetTitleUtf8 + "]]")
        : ("[[" + targetTitleUtf8 + "|" + original + "]]");
    src->bodyMarkdownUtf8.replace(byteOffset, byteLength, replacement);
    src->updatedEpochSec = static_cast<std::uint64_t>(std::time(nullptr));
    return true;
}

LinkPreviewResult KnowledgeVault::ResolveLinkPreview(const std::string& targetExprUtf8) const {
    LinkPreviewResult res;
    std::string title, heading, blockId, alias;
    SplitTargetComponents(targetExprUtf8, title, heading, blockId, alias);
    const VaultNoteRecord* note = FindNoteByTitleOrAlias(title);
    if (!note) return res;

    res.found = true;
    res.targetTitleUtf8 = note->titleUtf8;
    res.resolvedSectionUtf8 = !blockId.empty() ? ("^" + blockId) : (!heading.empty() ? ("#" + heading) : "Nota completa");
    res.resolvedPathUtf8 = note->folderPathUtf8.empty() ? note->titleUtf8 : (note->folderPathUtf8 + "/" + note->titleUtf8);
    const std::string content = (!heading.empty() || !blockId.empty())
        ? ResolveTransclusionMarkdown(title + (!blockId.empty() ? ("^" + blockId) : ("#" + heading)), 1)
        : note->bodyMarkdownUtf8;
    res.previewExcerptUtf8 = (content.size() <= 240) ? content : (content.substr(0, 237) + "...");
    return res;
}

RenameImpactReport KnowledgeVault::RenameNote(const std::string& oldTitleUtf8, const std::string& newTitleUtf8) {
    RenameImpactReport rep;
    const std::string oldT = Trim(oldTitleUtf8);
    const std::string newT = Trim(newTitleUtf8);
    if (oldT.empty() || newT.empty()) return rep;

    for (auto& n : notes_) {
        if (EqualsFolded(n.titleUtf8, oldT)) {
            n.titleUtf8 = newT;
        }
    }

    for (auto& n : notes_) {
        std::string& txt = n.bodyMarkdownUtf8;
        bool modified = false;
        std::size_t i = 0;
        while (i < txt.size()) {
            const bool isTrans = (txt[i] == '!' && i + 2 < txt.size() && txt[i + 1] == '[' && txt[i + 2] == '[');
            const bool isWiki = (!isTrans && i + 1 < txt.size() && txt[i] == '[' && txt[i + 1] == '[');
            if (isTrans || isWiki) {
                const std::size_t cs = i + (isTrans ? 3u : 2u);
                const std::size_t ce = txt.find("]]", cs);
                if (ce != std::string::npos && txt.find('\n', cs) >= ce) {
                    const std::string inner = txt.substr(cs, ce - cs);
                    std::string t, h, b, al;
                    SplitTargetComponents(inner, t, h, b, al);
                    if (EqualsFolded(t, oldT)) {
                        const bool hadHashCaret = (inner.find("#^") != std::string::npos);
                        const std::string rebuilt = RebuildWikiInnerExpr(newT, h, b, al, hadHashCaret);
                        txt.replace(cs, ce - cs, rebuilt);
                        modified = true;
                        ++rep.referencesUpdated;
                        i = cs + rebuilt.size() + 2;
                        continue;
                    }
                }
            }
            ++i;
        }
        if (modified) ++rep.notesModified;
    }

    rep.brokenLinksRemaining = FindBrokenWikiLinks().size();
    return rep;
}

RenameImpactReport KnowledgeVault::RenameHeadingInNote(
    const std::string& noteTitleUtf8,
    const std::string& oldHeadingUtf8,
    const std::string& newHeadingUtf8)
{
    RenameImpactReport rep;
    const std::string oldH = Trim(oldHeadingUtf8);
    const std::string newH = Trim(newHeadingUtf8);
    if (oldH.empty() || newH.empty()) return rep;

    for (auto& n : notes_) {
        if (EqualsFolded(n.titleUtf8, noteTitleUtf8)) {
            std::istringstream iss(n.bodyMarkdownUtf8);
            std::ostringstream oss;
            std::string line;
            bool changedHeading = false;
            while (std::getline(iss, line)) {
                const std::string tr = Trim(line);
                int hl = 0;
                while (hl < static_cast<int>(tr.size()) && hl < 6 && tr[hl] == '#') ++hl;
                if (hl > 0 && hl < static_cast<int>(tr.size()) && tr[hl] == ' ') {
                    const std::string ht = Trim(tr.substr(hl + 1));
                    if (EqualsFolded(ht, oldH)) {
                        line = std::string(static_cast<std::size_t>(hl), '#') + " " + newH;
                        changedHeading = true;
                    }
                }
                oss << line << '\n';
            }
            if (changedHeading) {
                n.bodyMarkdownUtf8 = Trim(oss.str());
                ++rep.notesModified;
            }
        }
    }

    for (auto& n : notes_) {
        std::string& txt = n.bodyMarkdownUtf8;
        bool mod = false;
        std::size_t i = 0;
        while (i < txt.size()) {
            const bool isTrans = (txt[i] == '!' && i + 2 < txt.size() && txt[i + 1] == '[' && txt[i + 2] == '[');
            const bool isWiki = (!isTrans && i + 1 < txt.size() && txt[i] == '[' && txt[i + 1] == '[');
            if (isTrans || isWiki) {
                const std::size_t cs = i + (isTrans ? 3u : 2u);
                const std::size_t ce = txt.find("]]", cs);
                if (ce != std::string::npos && txt.find('\n', cs) >= ce) {
                    const std::string inner = txt.substr(cs, ce - cs);
                    std::string t, h, b, al;
                    SplitTargetComponents(inner, t, h, b, al);
                    const std::string effT = t.empty() ? n.titleUtf8 : t;
                    if (EqualsFolded(effT, noteTitleUtf8) && EqualsFolded(h, oldH)) {
                        const std::string rebuilt = RebuildWikiInnerExpr(t, newH, b, al, false);
                        txt.replace(cs, ce - cs, rebuilt);
                        mod = true;
                        ++rep.referencesUpdated;
                        i = cs + rebuilt.size() + 2;
                        continue;
                    }
                }
            }
            ++i;
        }
        if (mod) ++rep.notesModified;
    }

    rep.brokenLinksRemaining = FindBrokenWikiLinks().size();
    return rep;
}

RenameImpactReport KnowledgeVault::RenameBlockIdInNote(
    const std::string& noteTitleUtf8,
    const std::string& oldBlockIdUtf8,
    const std::string& newBlockIdUtf8)
{
    RenameImpactReport rep;
    const std::string oldB = Trim(oldBlockIdUtf8);
    const std::string newB = Trim(newBlockIdUtf8);
    if (oldB.empty() || newB.empty()) return rep;

    for (auto& n : notes_) {
        if (EqualsFolded(n.titleUtf8, noteTitleUtf8)) {
            const std::string updated = ReplaceAllCopy(n.bodyMarkdownUtf8, "^" + oldB, "^" + newB);
            if (updated != n.bodyMarkdownUtf8) {
                n.bodyMarkdownUtf8 = updated;
                ++rep.notesModified;
            }
        }
    }

    for (auto& n : notes_) {
        std::string& txt = n.bodyMarkdownUtf8;
        bool mod = false;
        std::size_t i = 0;
        while (i < txt.size()) {
            const bool isTrans = (txt[i] == '!' && i + 2 < txt.size() && txt[i + 1] == '[' && txt[i + 2] == '[');
            const bool isWiki = (!isTrans && i + 1 < txt.size() && txt[i] == '[' && txt[i + 1] == '[');
            if (isTrans || isWiki) {
                const std::size_t cs = i + (isTrans ? 3u : 2u);
                const std::size_t ce = txt.find("]]", cs);
                if (ce != std::string::npos && txt.find('\n', cs) >= ce) {
                    const std::string inner = txt.substr(cs, ce - cs);
                    std::string t, h, b, al;
                    SplitTargetComponents(inner, t, h, b, al);
                    const std::string effT = t.empty() ? n.titleUtf8 : t;
                    if (EqualsFolded(effT, noteTitleUtf8) && b == oldB) {
                        const bool hadHC = (inner.find("#^") != std::string::npos);
                        const std::string rebuilt = RebuildWikiInnerExpr(t, h, newB, al, hadHC);
                        txt.replace(cs, ce - cs, rebuilt);
                        mod = true;
                        ++rep.referencesUpdated;
                        i = cs + rebuilt.size() + 2;
                        continue;
                    }
                }
            }
            ++i;
        }
        if (mod) ++rep.notesModified;
    }

    rep.brokenLinksRemaining = FindBrokenWikiLinks().size();
    return rep;
}

std::vector<KnowledgeLink> KnowledgeVault::FindBrokenWikiLinks() const {
    std::vector<KnowledgeLink> broken;
    for (const auto& lk : CollectAllLinks()) {
        if (lk.kind == KnowledgeLinkKind::ExternalUrl ||
            lk.kind == KnowledgeLinkKind::InternalPage ||
            lk.kind == KnowledgeLinkKind::InternalSection ||
            lk.kind == KnowledgeLinkKind::OfficeFileRef) {
            continue;
        }
        const VaultNoteRecord* dst = FindNoteByTitleOrAlias(lk.targetNoteTitleUtf8);
        if (!dst) {
            broken.push_back(lk);
            continue;
        }
        if (!lk.targetBlockIdUtf8.empty()) {
            if (dst->bodyMarkdownUtf8.find("^" + lk.targetBlockIdUtf8) == std::string::npos) {
                broken.push_back(lk);
            }
        } else if (!lk.targetHeadingUtf8.empty()) {
            const auto outline = RichBlockDocument::FromMarkdown(dst->bodyMarkdownUtf8).ExtractOutline();
            bool hasH = false;
            for (const auto& oh : outline) {
                if (EqualsFolded(oh.titleUtf8, lk.targetHeadingUtf8)) { hasH = true; break; }
            }
            if (!hasH) broken.push_back(lk);
        }
    }
    return broken;
}

GraphViewData KnowledgeVault::BuildGlobalGraph(const GraphFilterOptions& filter) const {
    GraphViewData g;
    std::unordered_map<std::string, std::size_t> titleToIdx;

    auto passesNoteFilter = [&](const VaultNoteRecord& n) -> bool {
        if (n.inTrash) return false;
        if (!filter.folderPrefixUtf8.empty() && n.folderPathUtf8.find(filter.folderPrefixUtf8) != 0) return false;
        if (!filter.requiredTagUtf8.empty()) {
            bool ok = false;
            for (const auto& t : n.tagsUtf8) if (EqualsFolded(t, filter.requiredTagUtf8)) { ok = true; break; }
            if (!ok) return false;
        }
        if (!filter.propKeyUtf8.empty()) {
            bool ok = false;
            for (const auto& p : n.properties) {
                if (EqualsFolded(p.keyUtf8, filter.propKeyUtf8) && EqualsFolded(p.valueUtf8, filter.propValueUtf8)) {
                    ok = true; break;
                }
            }
            if (!ok) return false;
        }
        return true;
    };

    for (const auto& n : notes_) {
        if (!passesNoteFilter(n)) continue;
        GraphNode2D nd;
        nd.idUtf8 = n.noteIdUtf8;
        nd.labelUtf8 = n.titleUtf8;
        nd.folderUtf8 = n.folderPathUtf8;
        nd.tagsUtf8 = n.tagsUtf8;
        nd.resolved = true;
        titleToIdx[FoldUtf8ForSearch(n.titleUtf8, false, true)] = g.nodes.size();
        g.nodes.push_back(std::move(nd));
    }

    for (const auto& lk : CollectAllLinks()) {
        if (lk.kind == KnowledgeLinkKind::ExternalUrl ||
            lk.kind == KnowledgeLinkKind::InternalPage ||
            lk.kind == KnowledgeLinkKind::InternalSection ||
            lk.kind == KnowledgeLinkKind::OfficeFileRef) {
            continue;
        }
        const std::string srcKey = FoldUtf8ForSearch(lk.sourceNoteTitleUtf8, false, true);
        if (!titleToIdx.count(srcKey)) continue;

        const VaultNoteRecord* dstNote = FindNoteByTitleOrAlias(lk.targetNoteTitleUtf8);
        std::string dstKey = dstNote ? FoldUtf8ForSearch(dstNote->titleUtf8, false, true) : FoldUtf8ForSearch(lk.targetNoteTitleUtf8, false, true);
        if (!titleToIdx.count(dstKey)) {
            if (dstNote != nullptr || !filter.includeUnresolved) continue;
            GraphNode2D un;
            un.idUtf8 = "unresolved:" + dstKey;
            un.labelUtf8 = lk.targetNoteTitleUtf8;
            un.resolved = false;
            titleToIdx[dstKey] = g.nodes.size();
            g.nodes.push_back(std::move(un));
        }
        auto& sNode = g.nodes[titleToIdx[srcKey]];
        auto& dNode = g.nodes[titleToIdx[dstKey]];
        ++sNode.degree;
        ++dNode.degree;
        g.edges.push_back({ sNode.idUtf8, dNode.idUtf8, lk.kind, dNode.resolved });
    }

    if (!filter.includeOrphans) {
        std::unordered_set<std::string> keptIds;
        g.nodes.erase(std::remove_if(g.nodes.begin(), g.nodes.end(), [](const GraphNode2D& n) { return n.degree == 0; }), g.nodes.end());
        for (const auto& n : g.nodes) keptIds.insert(n.idUtf8);
        g.edges.erase(std::remove_if(g.edges.begin(), g.edges.end(), [&](const GraphEdge2D& e) {
            return !keptIds.count(e.fromIdUtf8) || !keptIds.count(e.toIdUtf8);
        }), g.edges.end());
    }
    return g;
}

GraphViewData KnowledgeVault::BuildLocalGraph(
    const std::string& centerTitleUtf8,
    int maxDepth,
    const GraphFilterOptions& filter) const
{
    GraphFilterOptions localF = filter;
    localF.includeOrphans = true;
    GraphViewData global = BuildGlobalGraph(localF);
    const std::string centerKey = FoldUtf8ForSearch(Trim(centerTitleUtf8), false, true);
    std::string centerId;
    for (const auto& n : global.nodes) {
        if (FoldUtf8ForSearch(n.labelUtf8, false, true) == centerKey) {
            centerId = n.idUtf8;
            break;
        }
    }
    if (centerId.empty()) return {};

    std::unordered_map<std::string, int> dist;
    std::queue<std::string> q;
    dist[centerId] = 0;
    q.push(centerId);
    while (!q.empty()) {
        const std::string cur = q.front();
        q.pop();
        if (dist[cur] >= std::max(1, maxDepth)) continue;
        for (const auto& e : global.edges) {
            if (e.fromIdUtf8 == cur && !dist.count(e.toIdUtf8)) {
                dist[e.toIdUtf8] = dist[cur] + 1;
                q.push(e.toIdUtf8);
            }
            if (e.toIdUtf8 == cur && !dist.count(e.fromIdUtf8)) {
                dist[e.fromIdUtf8] = dist[cur] + 1;
                q.push(e.fromIdUtf8);
            }
        }
    }

    GraphViewData local;
    for (auto n : global.nodes) {
        if (!dist.count(n.idUtf8)) continue;
        n.isCenter = (n.idUtf8 == centerId);
        local.nodes.push_back(std::move(n));
    }
    for (const auto& e : global.edges) {
        if (dist.count(e.fromIdUtf8) && dist.count(e.toIdUtf8)) {
            local.edges.push_back(e);
        }
    }
    return local;
}

void KnowledgeVault::ComputeGraphLayout2D(GraphViewData& graph, int iterations, double width, double height) {
    const std::size_t N = graph.nodes.size();
    if (N == 0) return;

    const double cx = width * 0.5;
    const double cy = height * 0.5;
    const double radius = (std::min)(width, height) * 0.36;
    for (std::size_t i = 0; i < N; ++i) {
        if (graph.nodes[i].isCenter) {
            graph.nodes[i].x = cx;
            graph.nodes[i].y = cy;
        } else {
            const double ang = (6.283185307179586 * static_cast<double>(i)) / static_cast<double>(N);
            graph.nodes[i].x = cx + radius * std::cos(ang);
            graph.nodes[i].y = cy + radius * std::sin(ang);
        }
        graph.nodes[i].vx = 0.0;
        graph.nodes[i].vy = 0.0;
    }

    std::unordered_map<std::string, std::size_t> idIdx;
    for (std::size_t i = 0; i < N; ++i) idIdx[graph.nodes[i].idUtf8] = i;

    const double kArea = (width * height) / static_cast<double>(N + 1u);
    const double kSpring = std::sqrt(kArea) * 0.72;
    for (int it = 0; it < iterations; ++it) {
        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t j = i + 1; j < N; ++j) {
                double dx = graph.nodes[i].x - graph.nodes[j].x;
                double dy = graph.nodes[i].y - graph.nodes[j].y;
                double d2 = dx * dx + dy * dy + 16.0;
                double dist = std::sqrt(d2);
                double rep = (kSpring * kSpring) / dist;
                double fx = (dx / dist) * rep * 0.04;
                double fy = (dy / dist) * rep * 0.04;
                graph.nodes[i].vx += fx; graph.nodes[i].vy += fy;
                graph.nodes[j].vx -= fx; graph.nodes[j].vy -= fy;
            }
        }
        for (const auto& e : graph.edges) {
            if (!idIdx.count(e.fromIdUtf8) || !idIdx.count(e.toIdUtf8)) continue;
            auto& u = graph.nodes[idIdx[e.fromIdUtf8]];
            auto& v = graph.nodes[idIdx[e.toIdUtf8]];
            double dx = v.x - u.x;
            double dy = v.y - u.y;
            double dist = (std::max)(1.0, std::hypot(dx, dy));
            double att = (dist * dist) / (std::max)(1.0, kSpring);
            double fx = (dx / dist) * att * 0.03;
            double fy = (dy / dist) * att * 0.03;
            u.vx += fx; u.vy += fy;
            v.vx -= fx; v.vy -= fy;
        }
        for (std::size_t i = 0; i < N; ++i) {
            if (graph.nodes[i].isCenter) continue;
            graph.nodes[i].vx += (cx - graph.nodes[i].x) * 0.015;
            graph.nodes[i].vy += (cy - graph.nodes[i].y) * 0.015;
            graph.nodes[i].x = std::clamp(graph.nodes[i].x + std::clamp(graph.nodes[i].vx, -24.0, 24.0), 42.0, width - 42.0);
            graph.nodes[i].y = std::clamp(graph.nodes[i].y + std::clamp(graph.nodes[i].vy, -24.0, 24.0), 42.0, height - 42.0);
            graph.nodes[i].vx *= 0.65;
            graph.nodes[i].vy *= 0.65;
        }
    }
}

bool KnowledgeVault::ParseFrontmatterAndBody(
    const std::string& rawMarkdownUtf8,
    std::vector<NoteProperty>& outProps,
    std::vector<std::string>& outAliases,
    std::vector<std::string>& outTags,
    std::string& outBodyUtf8)
{
    outProps.clear();
    outAliases.clear();
    outTags.clear();
    outBodyUtf8 = rawMarkdownUtf8;
    if (rawMarkdownUtf8.rfind("---\n", 0) != 0 && rawMarkdownUtf8.rfind("---\r\n", 0) != 0) return false;

    const std::size_t startYaml = rawMarkdownUtf8.find('\n') + 1;
    const std::size_t endMarker = rawMarkdownUtf8.find("\n---", startYaml);
    if (endMarker == std::string::npos) return false;

    const std::string yamlBlock = rawMarkdownUtf8.substr(startYaml, endMarker - startYaml);
    std::size_t afterEnd = rawMarkdownUtf8.find('\n', endMarker + 4);
    outBodyUtf8 = (afterEnd == std::string::npos) ? "" : Trim(rawMarkdownUtf8.substr(afterEnd + 1));

    std::istringstream iss(yamlBlock);
    std::string line;
    NoteProperty* activeListProp = nullptr;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string tr = Trim(line);
        if (tr.empty()) continue;

        if (tr.rfind("- ", 0) == 0 && activeListProp) {
            const std::string item = Trim(tr.substr(2));
            activeListProp->type = NotePropType::List;
            activeListProp->listValuesUtf8.push_back(item);
            if (EqualsFolded(activeListProp->keyUtf8, "tags") || EqualsFolded(activeListProp->keyUtf8, "etiquetas")) outTags.push_back(item);
            else if (EqualsFolded(activeListProp->keyUtf8, "aliases")) outAliases.push_back(item);
            continue;
        }

        activeListProp = nullptr;
        const auto colon = tr.find(':');
        if (colon == std::string::npos) continue;
        const std::string k = Trim(tr.substr(0, colon));
        std::string v = Trim(tr.substr(colon + 1));

        NoteProperty prop;
        prop.keyUtf8 = k;
        if (v.empty()) {
            prop.type = NotePropType::List;
            outProps.push_back(std::move(prop));
            activeListProp = &outProps.back();
            continue;
        }
        if (v.size() >= 2 && v.front() == '[' && v.back() == ']') {
            prop.type = NotePropType::List;
            std::istringstream ls(v.substr(1, v.size() - 2));
            std::string it;
            while (std::getline(ls, it, ',')) {
                const std::string ti = Trim(it);
                if (!ti.empty()) {
                    prop.listValuesUtf8.push_back(ti);
                    if (EqualsFolded(k, "tags") || EqualsFolded(k, "etiquetas")) outTags.push_back(ti);
                    else if (EqualsFolded(k, "aliases")) outAliases.push_back(ti);
                }
            }
            prop.valueUtf8 = v;
        } else if (v == "true" || v == "false") {
            prop.type = NotePropType::Checkbox;
            prop.valueUtf8 = v;
        } else {
            // Heuristic number/date detection
            bool isDate = (v.size() == 10 && v[4] == '-' && v[7] == '-');
            bool isNum = !v.empty();
            for (std::size_t idx = 0; idx < v.size(); ++idx) {
                char c = v[idx];
                if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || (idx == 0 && c == '-'))) { isNum = false; break; }
            }
            prop.type = isDate ? NotePropType::Date : (isNum ? NotePropType::Number : NotePropType::Text);
            prop.valueUtf8 = v;
        }
        outProps.push_back(std::move(prop));
    }
    return true;
}

std::string KnowledgeVault::SerializeFrontmatterAndBody(
    const std::vector<NoteProperty>& props,
    const std::vector<std::string>& aliases,
    const std::vector<std::string>& tags,
    const std::string& bodyUtf8)
{
    std::ostringstream oss;
    if (!props.empty() || !aliases.empty() || !tags.empty()) {
        oss << "---\n";
        for (const auto& p : props) {
            if (!aliases.empty() && EqualsFolded(p.keyUtf8, "aliases")) continue;
            if (!tags.empty() && EqualsFolded(p.keyUtf8, "tags")) continue;
            if (p.type == NotePropType::List && !p.listValuesUtf8.empty()) {
                oss << p.keyUtf8 << ":\n";
                for (const auto& lv : p.listValuesUtf8) oss << "  - " << lv << "\n";
            } else {
                oss << p.keyUtf8 << ": " << p.valueUtf8 << "\n";
            }
        }
        if (!aliases.empty()) {
            oss << "aliases:\n";
            for (const auto& a : aliases) oss << "  - " << a << "\n";
        }
        if (!tags.empty()) {
            oss << "tags:\n";
            for (const auto& t : tags) oss << "  - " << t << "\n";
        }
        oss << "---\n";
    }
    if (!bodyUtf8.empty()) oss << bodyUtf8;
    return oss.str();
}

bool KnowledgeVault::SaveVaultToDirectory(const std::wstring& rootDirPath, std::string* outErr) const {
    try {
        const fs::path root(WideToUtf8(rootDirPath));
        fs::create_directories(root);

        json meta;
        meta["version"] = 1;
        meta["nextSeq"] = nextSeq_;
        json arr = json::array();

        for (const auto& n : notes_) {
            const std::string fileName = n.noteIdUtf8 + "_" + SanitizeSlug(n.titleUtf8) + ".md";
            const fs::path subDir = n.folderPathUtf8.empty() ? root : (root / fs::path(n.folderPathUtf8));
            const fs::path mdPath = subDir / fileName;
            const std::string fullMd = SerializeFrontmatterAndBody(n.properties, n.aliasesUtf8, n.tagsUtf8, n.bodyMarkdownUtf8);
            if (!WriteTextFile(mdPath, fullMd, outErr)) return false;

            const fs::path rel = fs::relative(mdPath, root);
            arr.push_back({
                {"noteId", n.noteIdUtf8},
                {"title", n.titleUtf8},
                {"folder", n.folderPathUtf8},
                {"relMdPath", rel.generic_string()},
                {"isDaily", n.isDailyNote},
                {"dailyDate", n.dailyDateIsoUtf8},
                {"inTrash", n.inTrash},
                {"cover", n.coverColorRgba},
                {"paper", static_cast<int>(n.paperPattern)},
                {"updatedEpochSec", n.updatedEpochSec}
            });
        }
        meta["notes"] = std::move(arr);
        return WriteTextFile(root / "cuadernu_vault_meta.json", meta.dump(2), outErr);
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool KnowledgeVault::LoadVaultFromDirectory(const std::wstring& rootDirPath, std::string* outErr) {
    try {
        const fs::path root(WideToUtf8(rootDirPath));
        std::string metaRaw;
        if (!ReadTextFile(root / "cuadernu_vault_meta.json", metaRaw, outErr)) return false;

        const json meta = json::parse(metaRaw);
        notes_.clear();
        nextSeq_ = meta.value("nextSeq", std::uint64_t{ 1 });
        if (meta.contains("notes") && meta["notes"].is_array()) {
            for (const auto& nj : meta["notes"]) {
                VaultNoteRecord rec;
                rec.noteIdUtf8 = nj.value("noteId", "");
                rec.titleUtf8 = nj.value("title", "");
                rec.folderPathUtf8 = nj.value("folder", "");
                rec.isDailyNote = nj.value("isDaily", false);
                rec.dailyDateIsoUtf8 = nj.value("dailyDate", "");
                rec.inTrash = nj.value("inTrash", false);
                rec.coverColorRgba = nj.value("cover", 0x1D4ED8FFu);
                rec.paperPattern = static_cast<PaperPattern>(nj.value("paper", 0));
                rec.updatedEpochSec = nj.value("updatedEpochSec", std::uint64_t{ 0 });

                const std::string relPath = nj.value("relMdPath", "");
                std::string rawMd;
                if (!relPath.empty() && ReadTextFile(root / fs::path(relPath), rawMd, outErr)) {
                    ParseFrontmatterAndBody(rawMd, rec.properties, rec.aliasesUtf8, rec.tagsUtf8, rec.bodyMarkdownUtf8);
                }
                notes_.push_back(std::move(rec));
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

// ---------------- Sticky Notes & Diagram Helpers ----------------

std::uint64_t CreateStickyNoteObject(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& textUtf8,
    DocColor paperFill)
{
    AnnotationObject sticky;
    sticky.kind = ObjectKind::TextBox;
    sticky.layerId = doc.ActiveLayerId();
    sticky.bounds = { x, y, 220.0, 96.0 };
    sticky.fillColor = paperFill;
    sticky.strokeColor = DocColor::RGBA(92, 54, 14, 255);
    sticky.fontSizePt = 12.5;
    sticky.textUtf8 = textUtf8.empty() ? "Nota adhesiva" : textUtf8;
    sticky.captionUtf8 = "Nota adhesiva";
    sticky.tags.push_back("sticky");
    if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(sticky)))) return 0;
    const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
    return vec.empty() ? 0ull : vec.back().id;
}

StickyNoteExportResult ExportStickyNotesFromDocument(const DocumentModel& doc) {
    StickyNoteExportResult res;
    std::ostringstream md;
    md << "# Notas Adhesivas Exportadas\n\n";
    json arr = json::array();

    auto visit = [&](const char* location, const AnnotationObject& o) {
        const bool isSticky = (o.captionUtf8 == "Nota adhesiva") ||
            (std::find(o.tags.begin(), o.tags.end(), "sticky") != o.tags.end());
        if (!isSticky) return;
        ++res.count;
        md << "- [" << location << "] " << o.textUtf8 << "\n";
        arr.push_back({
            {"id", o.id},
            {"location", location},
            {"text", o.textUtf8},
            {"fillRgba", o.fillColor.PackRgba()}
        });
    };

    for (std::size_t p = 0; p < doc.Pages().size(); ++p) {
        const std::string loc = "Pagina " + std::to_string(p + 1);
        for (const auto& o : doc.Pages()[p].objects) visit(loc.c_str(), o);
    }
    for (const auto& o : doc.CanvasObjects()) visit("Lienzo", o);

    res.markdownUtf8 = md.str();
    res.jsonUtf8 = json({ {"count", res.count}, {"stickyNotes", arr} }).dump(2);
    return res;
}

bool CreateDiagramBlockWithConnectors(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<DiagramNodeSpec>& nodes,
    const std::vector<std::pair<std::size_t, std::size_t>>& edges,
    std::vector<std::uint64_t>* outNodeIds,
    std::vector<std::uint64_t>* outConnectorIds)
{
    if (outNodeIds) outNodeIds->clear();
    if (outConnectorIds) outConnectorIds->clear();
    if (nodes.empty()) return false;

    std::vector<std::uint64_t> nodeIds;
    std::vector<AnnotationObject> createdNodes;
    nodeIds.reserve(nodes.size());

    for (const auto& spec : nodes) {
        AnnotationObject box;
        box.kind = ObjectKind::Shape;
        box.shapeKind = spec.shape;
        box.layerId = doc.ActiveLayerId();
        box.bounds = { spec.x, spec.y, spec.w, spec.h };
        box.fillColor = spec.fill;
        box.strokeColor = spec.stroke;
        box.strokeWidth = 1.8;
        box.textUtf8 = spec.labelUtf8;
        box.captionUtf8 = spec.labelUtf8;
        if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, box))) return false;
        const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
        if (vec.empty()) return false;
        nodeIds.push_back(vec.back().id);
        createdNodes.push_back(vec.back());

        // Add companion centered TextBox label inside diagram node so PDF & screen always render crisp label
        if (!spec.labelUtf8.empty()) {
            AnnotationObject lbl;
            lbl.kind = ObjectKind::TextBox;
            lbl.layerId = doc.ActiveLayerId();
            lbl.anchorObjectId = nodeIds.back();
            lbl.bounds = { spec.x + 8.0, spec.y + 10.0, (std::max)(24.0, spec.w - 16.0), (std::max)(18.0, spec.h - 20.0) };
            lbl.textUtf8 = spec.labelUtf8;
            lbl.strokeColor = DocColor::RGBA(24, 30, 42, 255);
            lbl.fontSizePt = 11.5;
            cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(lbl)));
        }
    }

    std::vector<std::uint64_t> connIds;
    for (const auto& e : edges) {
        if (e.first >= createdNodes.size() || e.second >= createdNodes.size()) continue;
        const auto& src = createdNodes[e.first];
        const auto& dst = createdNodes[e.second];
        AnnotationObject conn;
        conn.kind = ObjectKind::Shape;
        conn.shapeKind = ShapeKind::Connector;
        conn.layerId = doc.ActiveLayerId();
        conn.strokeColor = DocColor::RGBA(51, 65, 85, 255);
        conn.strokeWidth = 1.8;
        conn.connectorFromId = nodeIds[e.first];
        conn.connectorToId = nodeIds[e.second];
        conn.shapeStart = { src.bounds.x + src.bounds.w * 0.5, src.bounds.y + src.bounds.h * 0.5 };
        conn.shapeEnd = { dst.bounds.x + dst.bounds.w * 0.5, dst.bounds.y + dst.bounds.h * 0.5 };
        conn.RecomputeBounds();
        if (!cmdStack.Execute(doc, std::make_unique<AddObjectCommand>(target, std::move(conn)))) return false;
        const auto& vec = target.isCanvas ? doc.CanvasObjects() : doc.Pages()[target.pageIndex].objects;
        if (!vec.empty()) connIds.push_back(vec.back().id);
    }

    if (outNodeIds) *outNodeIds = std::move(nodeIds);
    if (outConnectorIds) *outConnectorIds = std::move(connIds);
    return true;
}

} // namespace DocStudio
