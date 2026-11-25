#include "academic/tec_digital/academic_models.hpp"
#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/types.hpp"
#include "http_common.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_map>

namespace academic::tec_digital {

namespace {

std::string StripHtmlTags(std::string_view html) {
    std::string out;
    out.reserve(html.size());
    bool in_tag = false;
    bool in_script_or_style = false;
    const std::string low = AsciiLower(html);
    for (size_t i = 0; i < html.size(); ++i) {
        if (!in_tag && html[i] == '<') {
            if (low.compare(i, 7, "<script") == 0 || low.compare(i, 6, "<style") == 0) {
                in_script_or_style = true;
            } else if (low.compare(i, 9, "</script>") == 0) {
                in_script_or_style = false;
                i += 8;
                continue;
            } else if (low.compare(i, 8, "</style>") == 0) {
                in_script_or_style = false;
                i += 7;
                continue;
            }
            if (low.compare(i, 3, "<br") == 0 || low.compare(i, 3, "<p ") == 0 ||
                low.compare(i, 3, "<p>") == 0 || low.compare(i, 4, "</p>") == 0 ||
                low.compare(i, 5, "</div>") == 0 || low.compare(i, 5, "</li>") == 0 ||
                low.compare(i, 5, "</tr>") == 0) {
                out.push_back('\n');
            }
            in_tag = true;
        } else if (in_tag && html[i] == '>') {
            in_tag = false;
        } else if (!in_tag && !in_script_or_style) {
            out.push_back(html[i]);
        }
    }
    std::string unesc = HeadlessCrawler::HtmlUnescape(out);
    // Replace non-breaking spaces (UTF-8 C2 A0) with regular space
    std::string clean;
    clean.reserve(unesc.size());
    for (size_t i = 0; i < unesc.size(); ++i) {
        if (i + 1 < unesc.size() &&
            static_cast<unsigned char>(unesc[i]) == 0xC2u &&
            static_cast<unsigned char>(unesc[i + 1]) == 0xA0u) {
            clean.push_back(' ');
            ++i;
        } else {
            clean.push_back(unesc[i]);
        }
    }
    return TrimAscii(clean);
}

std::string CollapseSpaces(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    bool prev_ws = true;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prev_ws) {
                out.push_back(' ');
                prev_ws = true;
            }
        } else {
            out.push_back(c);
            prev_ws = false;
        }
    }
    if (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

bool TryParseDoubleStrict(std::string_view sv, double& out) {
    std::string s = TrimAscii(sv);
    if (s.empty()) return false;
    // Normalize Spanish decimal comma when no dot is present
    if (s.find('.') == std::string::npos && s.find(',') != std::string::npos) {
        std::replace(s.begin(), s.end(), ',', '.');
    }
    char* endp = nullptr;
    double v = std::strtod(s.c_str(), &endp);
    if (endp == s.c_str() || !std::isfinite(v)) return false;
    out = v;
    return true;
}

bool TryExtractFirstDouble(std::string_view sv, double& out) {
    std::string s(sv);
    static const std::regex kNumRe(R"((-?\d+(?:[.,]\d+)?))");
    std::smatch m;
    if (!std::regex_search(s, m, kNumRe)) return false;
    return TryParseDoubleStrict(m.str(1), out);
}

bool TryExtractFractionPair(std::string_view sv, double& out_num, double& out_den) {
    std::string s(sv);
    static const std::regex kPairRe(R"((--|-?\d+(?:[.,]\d+)?)\s*/\s*(\d+(?:[.,]\d+)?))");
    std::smatch m;
    if (!std::regex_search(s, m, kPairRe)) return false;
    if (!TryParseDoubleStrict(m.str(2), out_den)) return false;
    if (m.str(1) == "--") {
        out_num = -1.0;
        return true;
    }
    return TryParseDoubleStrict(m.str(1), out_num);
}

std::string ConvertDmyToIso(std::string_view dmy) {
    std::string s = CollapseSpaces(dmy);
    static const std::regex kDmyRe(R"((\d{1,2})/(\d{1,2})/(\d{4})(?:\s+(\d{1,2}):(\d{2}))?)");
    std::smatch m;
    if (!std::regex_search(s, m, kDmyRe)) return {};
    int dd = std::atoi(m.str(1).c_str());
    int mm = std::atoi(m.str(2).c_str());
    int yyyy = std::atoi(m.str(3).c_str());
    if (dd < 1 || dd > 31 || mm < 1 || mm > 12 || yyyy < 1990 || yyyy > 2100) return {};
    char buf[32]{};
    if (m[4].matched && m[5].matched) {
        int hh = std::atoi(m.str(4).c_str());
        int mi = std::atoi(m.str(5).c_str());
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d", yyyy, mm, dd, hh, mi);
    } else {
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", yyyy, mm, dd);
    }
    return buf;
}

std::vector<std::vector<std::string>> ExtractHtmlTableRows(std::string_view html) {
    std::vector<std::vector<std::string>> rows;
    std::string s(html);
    static const std::regex kTrRe(R"(<tr\b[^>]*>([\s\S]*?)</tr>)", std::regex::icase);
    static const std::regex kCellRe(R"(<t[dh]\b[^>]*>([\s\S]*?)</t[dh]>)", std::regex::icase);
    for (std::sregex_iterator it(s.begin(), s.end(), kTrRe), end; it != end; ++it) {
        std::string row_html = (*it)[1].str();
        std::vector<std::string> cells;
        for (std::sregex_iterator cit(row_html.begin(), row_html.end(), kCellRe); cit != end; ++cit) {
            cells.push_back(CollapseSpaces(StripHtmlTags((*cit)[1].str())));
        }
        if (!cells.empty()) rows.push_back(std::move(cells));
    }
    return rows;
}

std::string ResolveRelativeUrl(std::string_view base_url, std::string_view href) {
    std::string h = TrimAscii(href);
    if (h.empty()) return {};
    if (h.rfind("http://", 0) == 0 || h.rfind("https://", 0) == 0 || h.rfind("mailto:", 0) == 0) {
        return h;
    }
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    if (h.front() == '/') return b + h;
    return b + "/" + h;
}

std::string ClassifyExternalLink(std::string_view label, std::string_view url) {
    std::string low_l = AsciiLower(label);
    std::string low_u = AsciiLower(url);
    if (low_l.find("programa") != std::string::npos || low_l.find("carta al estudiante") != std::string::npos) {
        return "Programa";
    }
    if (low_l.find("cronograma") != std::string::npos || low_l.find("planificaci") != std::string::npos) {
        return "Cronograma";
    }
    if (low_u.find("youtube.com") != std::string::npos || low_u.find("youtu.be") != std::string::npos) {
        return "YouTube";
    }
    if (low_u.find("sharepoint.com") != std::string::npos || low_u.find("onedrive") != std::string::npos) {
        return "SharePoint";
    }
    if (low_u.find("teams.microsoft.com") != std::string::npos) {
        return "Teams";
    }
    if (low_u.find("zoom.us") != std::string::npos) {
        return "Zoom";
    }
    if (low_u.find(".pdf") != std::string::npos) {
        return "PDF";
    }
    return "Enlace";
}

std::string EscapeIcsText(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '\\') out += "\\\\";
        else if (c == ';') out += "\\;";
        else if (c == ',') out += "\\,";
        else if (c == '\r') continue;
        else if (c == '\n') out += "\\n";
        else out.push_back(c);
    }
    return out;
}

std::string DigitsOnly(std::string_view s) {
    std::string out;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

// Parse time token like "7:0", "7:30", "16:5", "07:30", "13:00", "7:30 am", "2:30 pm"
int ParseTimeMinutes(std::string_view token) {
    std::string s = AsciiLower(CollapseSpaces(token));
    static const std::regex kTmRe(R"((\d{1,2}):(\d{1,2})(?:\s*(am|pm))?)");
    std::smatch m;
    if (!std::regex_search(s, m, kTmRe)) return -1;
    int hh = std::atoi(m.str(1).c_str());
    int mm = std::atoi(m.str(2).c_str());
    if (m[3].matched) {
        std::string ap = m.str(3);
        if (ap == "pm" && hh < 12) hh += 12;
        if (ap == "am" && hh == 12) hh = 0;
    }
    if (hh < 0 || hh > 24 || mm < 0 || mm > 59) return -1;
    return hh * 60 + mm;
}

std::string FormatMinutesHHMM(int mins) {
    if (mins < 0) mins = 0;
    int hh = (mins / 60) % 24;
    int mm = mins % 60;
    char buf[8]{};
    std::snprintf(buf, sizeof(buf), "%02d:%02d", hh, mm);
    return buf;
}

// Removes balanced <header ... id="header-td" ...>...</header> and platform_user_panel blocks
// so global portal navigation never pollutes course tabs, links, or folder discovery.
std::string StripGlobalPortalChrome(std::string_view html) {
    std::string out(html);
    auto remove_tag_with_marker = [&](std::string_view tag_name, std::string_view attr_marker) {
        const std::string open_prefix = "<" + std::string(tag_name);
        const std::string close_tag = "</" + std::string(tag_name) + ">";
        size_t pos = 0;
        while (pos < out.size()) {
            std::string low = AsciiLower(out);
            size_t found = low.find(open_prefix, pos);
            if (found == std::string::npos) break;
            size_t gt = out.find('>', found + open_prefix.size());
            if (gt == std::string::npos) break;
            std::string open_tag_low = low.substr(found, gt - found + 1);
            if (open_tag_low.find(attr_marker) == std::string::npos) {
                pos = gt + 1;
                continue;
            }
            int depth = 1;
            size_t scan = gt + 1;
            size_t block_end = out.size();
            while (scan < low.size()) {
                size_t next_open = low.find(open_prefix, scan);
                size_t next_close = low.find(close_tag, scan);
                if (next_close == std::string::npos) {
                    block_end = out.size();
                    break;
                }
                if (next_open != std::string::npos && next_open < next_close) {
                    ++depth;
                    scan = next_open + open_prefix.size();
                } else {
                    --depth;
                    scan = next_close + close_tag.size();
                    if (depth == 0) {
                        block_end = scan;
                        break;
                    }
                }
            }
            out.erase(found, block_end - found);
            pos = found;
        }
    };
    remove_tag_with_marker("header", "header-td");
    remove_tag_with_marker("div", "platform_user_panel");
    return out;
}

bool ExtractAttrValueCI(std::string_view attrs, std::string_view attr_name, std::string& out_val) {
    std::string low = AsciiLower(attrs);
    std::string target = AsciiLower(attr_name);
    size_t pos = 0;
    while (pos < low.size()) {
        size_t found = low.find(target, pos);
        if (found == std::string::npos) return false;
        if (found > 0 && (std::isalnum(static_cast<unsigned char>(low[found - 1])) ||
                          low[found - 1] == '-' || low[found - 1] == '_')) {
            pos = found + target.size();
            continue;
        }
        size_t after = found + target.size();
        while (after < low.size() && std::isspace(static_cast<unsigned char>(low[after]))) ++after;
        if (after >= low.size() || low[after] != '=') {
            pos = found + target.size();
            continue;
        }
        ++after;
        while (after < low.size() && std::isspace(static_cast<unsigned char>(low[after]))) ++after;
        if (after >= low.size()) return false;
        if (low[after] == '"' || low[after] == '\'') {
            char q = low[after];
            ++after;
            size_t end_q = attrs.find(q, after);
            if (end_q == std::string_view::npos) return false;
            out_val = HeadlessCrawler::HtmlUnescape(attrs.substr(after, end_q - after));
            return true;
        }
        size_t end_u = after;
        while (end_u < attrs.size() && !std::isspace(static_cast<unsigned char>(attrs[end_u])) &&
               attrs[end_u] != '>') {
            ++end_u;
        }
        out_val = HeadlessCrawler::HtmlUnescape(attrs.substr(after, end_u - after));
        return true;
    }
    return false;
}

std::string FoldSpanishAccentsAscii(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (c == 0xC3u && i + 1 < sv.size()) {
            unsigned char n = static_cast<unsigned char>(sv[i + 1]);
            switch (n) {
                case 0xA1u: out.push_back('a'); ++i; continue; // á
                case 0x81u: out.push_back('A'); ++i; continue; // Á
                case 0xA9u: out.push_back('e'); ++i; continue; // é
                case 0x89u: out.push_back('E'); ++i; continue; // É
                case 0xADu: out.push_back('i'); ++i; continue; // í
                case 0x8Du: out.push_back('I'); ++i; continue; // Í
                case 0xB3u: out.push_back('o'); ++i; continue; // ó
                case 0x93u: out.push_back('O'); ++i; continue; // Ó
                case 0xBAu: out.push_back('u'); ++i; continue; // ú
                case 0x9Au: out.push_back('U'); ++i; continue; // Ú
                case 0xBCu: out.push_back('u'); ++i; continue; // ü
                case 0x9Cu: out.push_back('U'); ++i; continue; // Ü
                case 0xB1u: out.push_back('n'); ++i; continue; // ñ
                case 0x91u: out.push_back('N'); ++i; continue; // Ñ
                default: break;
            }
        }
        // Normalize UTF-8 en-dash (E2 80 93) and em-dash (E2 80 94) to ASCII '-'
        if (c == 0xE2u && i + 2 < sv.size() &&
            static_cast<unsigned char>(sv[i + 1]) == 0x80u &&
            (static_cast<unsigned char>(sv[i + 2]) == 0x93u ||
             static_cast<unsigned char>(sv[i + 2]) == 0x94u)) {
            out.push_back('-');
            i += 2;
            continue;
        }
        out.push_back(static_cast<char>(c));
    }
    return out;
}

std::string NormalizeGroupMergeKey(std::string_view raw_group) {
    std::string s = TrimAscii(raw_group);
    if (s.empty()) return "1";
    const bool all_digits = std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    if (all_digits) {
        size_t nz = s.find_first_not_of('0');
        return (nz == std::string::npos) ? "0" : s.substr(nz);
    }
    return AsciiLower(s);
}

}  // namespace

// ============================================================================
// 1. Detailed Evaluations, Rubrics & What-If Calculator
// ============================================================================

DetailedEvaluationsReport ParseDetailedEvaluationsHtml(std::string_view html,
                                                       std::string_view community_slug) {
    detail::WarmupRegexLocale();
    DetailedEvaluationsReport rep;
    rep.community_slug = std::string(community_slug);

    std::string s(html);
    const std::string plain = StripHtmlTags(s);

    // Extract overall unrounded & final grades
    {
        static const std::regex kUnroundedRe(
            R"(Nota\s+Total\s*\(sin\s+redondear\)[\s:]*([0-9]+(?:[.,][0-9]+)?)\s*/\s*100)",
            std::regex::icase);
        std::smatch m;
        if (std::regex_search(plain, m, kUnroundedRe)) {
            TryParseDoubleStrict(m.str(1), rep.unrounded_total);
        }
        static const std::regex kFinalRe(
            R"((?:Nota\s+final|studentFinal[^>]*>)[\s:]*([0-9]+(?:[.,][0-9]+)?)\s*/\s*100)",
            std::regex::icase);
        if (std::regex_search(s, m, kFinalRe) || std::regex_search(plain, m, kFinalRe)) {
            if (TryParseDoubleStrict(m.str(1), rep.final_grade)) {
                rep.has_final_grade = true;
            }
        }
    }

    // Parse categories (.sub_total) and assignment accordions (.container_accor) in document order
    static const std::regex kCatMarkerRe(
        R"(<div[^>]*class\s*=\s*["'][^"']*sub_total[^"']*["'][^>]*>([\s\S]*?)</div>)",
        std::regex::icase);
    static const std::regex kAssignMarkerRe(
        R"(<div[^>]*class\s*=\s*["'][^"']*container_accor[^"']*["'][^>]*>)",
        std::regex::icase);

    struct Marker {
        size_t pos = 0;
        size_t end = 0;
        bool is_cat = false;
        std::string raw_tag;
        std::string inner;
    };
    std::vector<Marker> markers;

    for (std::sregex_iterator it(s.begin(), s.end(), kCatMarkerRe), end; it != end; ++it) {
        Marker mk;
        mk.pos = static_cast<size_t>(it->position());
        mk.end = mk.pos + static_cast<size_t>(it->length());
        mk.is_cat = true;
        mk.raw_tag = it->str(0);
        mk.inner = it->str(1);
        markers.push_back(std::move(mk));
    }
    for (std::sregex_iterator it(s.begin(), s.end(), kAssignMarkerRe), end; it != end; ++it) {
        Marker mk;
        mk.pos = static_cast<size_t>(it->position());
        mk.end = mk.pos + static_cast<size_t>(it->length());
        mk.is_cat = false;
        mk.raw_tag = it->str(0);
        markers.push_back(std::move(mk));
    }
    std::sort(markers.begin(), markers.end(),
              [](const Marker& a, const Marker& b) { return a.pos < b.pos; });

    auto extract_attr = [](const std::string& tag, const std::string& key) -> std::string {
        std::regex re(key + R"(\s*=\s*["']([^"']*)["'])", std::regex::icase);
        std::smatch m;
        if (std::regex_search(tag, m, re)) return m.str(1);
        return {};
    };

    for (size_t i = 0; i < markers.size(); ++i) {
        const size_t slice_end = (i + 1 < markers.size()) ? markers[i + 1].pos : s.size();
        const std::string block = s.substr(markers[i].pos, slice_end - markers[i].pos);

        if (markers[i].is_cat) {
            DetailedEvaluationCategory cat;
            cat.category_id = extract_attr(markers[i].raw_tag, "category_id");
            std::string w_attr = extract_attr(markers[i].raw_tag, "item_weight");
            if (!w_attr.empty()) TryExtractFirstDouble(w_attr, cat.weight_pct);

            std::string cat_txt = CollapseSpaces(StripHtmlTags(markers[i].inner));
            double e_val = -1.0, w_val = 0.0;
            if (TryExtractFractionPair(cat_txt, e_val, w_val)) {
                if (w_val > 0.0) cat.weight_pct = w_val;
                if (e_val >= 0.0) cat.earned_pct = e_val;
            }
            // Strip trailing earned/weight numbers from category title
            cat.name = TrimAscii(std::regex_replace(
                cat_txt, std::regex(R"((?:--|\d+(?:[.,]\d+)?)\s*/\s*\d+(?:[.,]\d+)?.*$)"), ""));
            if (cat.name.empty()) cat.name = "Categoría";
            rep.categories.push_back(std::move(cat));
            continue;
        }

        if (rep.categories.empty()) {
            DetailedEvaluationCategory fallback;
            fallback.name = "Evaluaciones";
            rep.categories.push_back(std::move(fallback));
        }

        DetailedEvaluationItem item;
        item.category_name = rep.categories.back().name;
        item.assign_id = extract_attr(markers[i].raw_tag, "assign_id");

        // Name
        {
            static const std::regex kNameRe(
                R"(class\s*=\s*["'][^"']*assignNameText[^"']*["'][^>]*>([\s\S]*?)</)",
                std::regex::icase);
            std::smatch m;
            if (std::regex_search(block, m, kNameRe)) {
                item.name = CollapseSpaces(StripHtmlTags(m.str(1)));
            }
        }
        // Raw grade (.vt_grade_student or "Nota obtenida: X / 100")
        {
            static const std::regex kGradeSpanRe(
                R"(class\s*=\s*["'][^"']*vt_grade_student[^"']*["'][^>]*>([\s\S]*?)</)",
                std::regex::icase);
            std::smatch m;
            if (std::regex_search(block, m, kGradeSpanRe)) {
                std::string gtxt = CollapseSpaces(StripHtmlTags(m.str(1)));
                double gval = -1.0;
                if (gtxt.find("--") == std::string::npos && TryExtractFirstDouble(gtxt, gval)) {
                    item.raw_grade_100 = gval;
                    item.is_graded = true;
                }
            }
        }

        const std::string bplain = StripHtmlTags(block);

        // Weighted value ("Valor de la Asignación: 3.75")
        {
            static const std::regex kWeightRe(
                R"(Valor\s+de\s+la\s+Asignaci[oó]n\s*:\s*([0-9]+(?:[.,][0-9]+)?))",
                std::regex::icase);
            std::smatch m;
            if (std::regex_search(bplain, m, kWeightRe)) {
                TryParseDoubleStrict(m.str(1), item.weight_pct);
            }
        }
        // Earned / weight fraction from header (e.g. "3.00 / 3.75")
        {
            double num = -1.0, den = 0.0;
            if (TryExtractFractionPair(bplain.substr(0, std::min<size_t>(bplain.size(), 120)), num, den)) {
                if (item.weight_pct <= 0.0 && den > 0.0 && den <= 100.0) item.weight_pct = den;
                if (num >= 0.0) item.earned_pct = num;
            }
        }
        // Raw grade from text fallback ("Nota obtenida: 80.0 / 100")
        if (!item.is_graded) {
            static const std::regex kNotaObtRe(
                R"(Nota\s+obtenida\s*:\s*(--|[0-9]+(?:[.,][0-9]+)?)\s*/\s*100)",
                std::regex::icase);
            std::smatch m;
            if (std::regex_search(bplain, m, kNotaObtRe) && m.str(1) != "--") {
                double gval = -1.0;
                if (TryParseDoubleStrict(m.str(1), gval)) {
                    item.raw_grade_100 = gval;
                    item.is_graded = true;
                }
            }
        }
        if (item.is_graded && item.weight_pct > 0.0 && item.earned_pct <= 0.0 && item.raw_grade_100 > 0.0) {
            item.earned_pct = (item.raw_grade_100 / 100.0) * item.weight_pct;
        }

        // Due date ("Fecha de Entrega : 23/11/2026 23:59")
        {
            static const std::regex kDueRe(
                R"(Fecha\s+de\s+Entrega\s*:\s*([^\n]+))",
                std::regex::icase);
            std::smatch m;
            if (std::regex_search(bplain, m, kDueRe)) {
                item.due_date_text = CollapseSpaces(m.str(1));
                item.due_date_iso = ConvertDmyToIso(item.due_date_text);
            }
        }
        // Group size ("Cantidad de personas por grupo: 2")
        {
            static const std::regex kGrpRe(
                R"(Cantidad\s+de\s+personas\s+por\s+grupo\s*:\s*(\d+))",
                std::regex::icase);
            std::smatch m;
            if (std::regex_search(bplain, m, kGrpRe)) {
                item.group_size = std::max(1, std::atoi(m.str(1).c_str()));
            }
        }
        // Rubric trigger attributes
        {
            static const std::regex kRubRe(R"(<[^>]*class\s*=\s*["'][^"']*seeRubric[^"']*["'][^>]*>)",
                                           std::regex::icase);
            std::smatch m;
            if (std::regex_search(block, m, kRubRe)) {
                std::string rtag = m.str(0);
                item.has_rubric = true;
                item.rubric_id = extract_attr(rtag, "data-rubricid");
                item.task_id = extract_attr(rtag, "task_id");
                item.task_item_id = extract_attr(rtag, "task_item_id");
                item.community_id = extract_attr(rtag, "community_id");
            }
        }
        // Attachment URL (/evaluation/view/...)
        {
            static const std::regex kAttRe(R"(href\s*=\s*["']([^"']*evaluation/view/[^"']*)["'])",
                                           std::regex::icase);
            std::smatch m;
            if (std::regex_search(block, m, kAttRe)) {
                item.attachment_url = HeadlessCrawler::HtmlUnescape(m.str(1));
            }
        }
        // Submission status
        if (bplain.find("No hay entregas de archivo o enlace") != std::string::npos) {
            item.submission_status = "Sin entrega";
        } else if (item.is_graded) {
            item.submission_status = "Calificada";
        } else {
            item.submission_status = "Pendiente";
        }

        if (item.name.empty()) {
            item.name = item.assign_id.empty() ? "Asignación" : ("Asignación " + item.assign_id);
        }
        rep.categories.back().items.push_back(std::move(item));
    }

    // Recompute totals if missing from header
    double sum_earned = 0.0;
    for (auto& cat : rep.categories) {
        double cat_earned = 0.0;
        double cat_weight = 0.0;
        for (const auto& it : cat.items) {
            if (it.is_graded) cat_earned += it.earned_pct;
            cat_weight += it.weight_pct;
        }
        if (cat.weight_pct <= 0.0) cat.weight_pct = cat_weight;
        if (cat.earned_pct <= 0.0) cat.earned_pct = cat_earned;
        sum_earned += cat.earned_pct;
    }
    if (rep.unrounded_total <= 0.0 && sum_earned > 0.0) rep.unrounded_total = sum_earned;
    if (!rep.has_final_grade && rep.unrounded_total > 0.0) {
        rep.final_grade = std::round(rep.unrounded_total);
        rep.has_final_grade = true;
    }
    return rep;
}

RubricBreakdown ParseRubricHtml(std::string_view html, std::string_view rubric_id) {
    RubricBreakdown rb;
    rb.rubric_id = std::string(rubric_id);
    {
        static const std::regex kHeadingRe(R"(<h[1-3][^>]*>([\s\S]*?)</h[1-3]>)", std::regex::icase);
        std::string hs(html);
        std::smatch hm;
        if (std::regex_search(hs, hm, kHeadingRe)) {
            rb.title = CollapseSpaces(StripHtmlTags(hm.str(1)));
        }
    }
    const auto rows = ExtractHtmlTableRows(html);
    if (rows.empty()) return rb;

    std::vector<std::pair<std::string, double>> level_headers;
    for (const auto& row : rows) {
        if (row.size() >= 3 &&
            (row[0].find("Categor") != std::string::npos || row[1].find("Escala") != std::string::npos ||
             row[0].empty())) {
            for (size_t c = 0; c < row.size(); ++c) {
                if (row[c].find("Puntos") != std::string::npos || row[c].find("Punto") != std::string::npos) {
                    double pts = 0.0;
                    TryExtractFirstDouble(row[c], pts);
                    level_headers.emplace_back(row[c], pts);
                } else if (row[c].find("Puntuaci") != std::string::npos && rb.title.empty()) {
                    rb.title = row[c];
                    TryExtractFirstDouble(row[c], rb.total_score);
                }
            }
            continue;
        }
        if (row.size() >= 2 && !row[0].empty()) {
            RubricCriterion crit;
            crit.category_name = row[0];
            for (size_t c = 1; c < row.size(); ++c) {
                RubricLevel lv;
                if (c - 1 < level_headers.size()) {
                    lv.label = level_headers[c - 1].first;
                    lv.points = level_headers[c - 1].second;
                    crit.max_points = std::max(crit.max_points, lv.points);
                } else {
                    lv.label = "Nivel " + std::to_string(c);
                }
                lv.description = row[c];
                crit.levels.push_back(std::move(lv));
            }
            rb.max_score += crit.max_points;
            rb.criteria.push_back(std::move(crit));
        }
    }
    return rb;
}

GradeWhatIfScenario ComputeGradeWhatIf(
    const DetailedEvaluationsReport& report,
    double target_grade,
    const std::map<std::string, double>& hypothetical_raw_100_by_assign_id) {
    GradeWhatIfScenario sc;
    sc.target_grade = target_grade;

    double earned = 0.0;
    double eval_w = 0.0;
    double rem_w = 0.0;
    double total_w = 0.0;
    double projected = 0.0;

    for (const auto& cat : report.categories) {
        double cat_items_w = 0.0;
        for (const auto& it : cat.items) {
            double w = std::max(0.0, it.weight_pct);
            cat_items_w += w;
            total_w += w;

            auto ov = hypothetical_raw_100_by_assign_id.find(it.assign_id);
            if (ov != hypothetical_raw_100_by_assign_id.end()) {
                double clamped = std::clamp(ov->second, 0.0, 100.0);
                double contrib = (clamped / 100.0) * w;
                projected += contrib;
                earned += contrib;
                eval_w += w;
            } else if (it.is_graded) {
                double contrib = (it.raw_grade_100 >= 0.0 && w > 0.0)
                                     ? ((it.raw_grade_100 / 100.0) * w)
                                     : it.earned_pct;
                earned += contrib;
                projected += contrib;
                eval_w += w;
            } else {
                rem_w += w;
            }
        }
        // Account for unitemized remaining weight inside a category
        if (cat.weight_pct > cat_items_w + 0.01) {
            double unitemized = cat.weight_pct - cat_items_w;
            total_w += unitemized;
            rem_w += unitemized;
        }
    }

    if (total_w < 99.9 && total_w > 0.0) {
        // Keep exact sum of defined course weights
    } else if (total_w <= 0.0) {
        total_w = 100.0;
        rem_w = 100.0;
    }

    sc.current_earned_pct = earned;
    sc.evaluated_weight_pct = eval_w;
    sc.remaining_weight_pct = rem_w;
    sc.total_weight_pct = total_w;
    sc.projected_final_with_overrides = projected;
    sc.max_possible_grade = earned + rem_w;
    sc.already_reached = (earned >= target_grade - 1e-6);

    if (sc.already_reached) {
        sc.required_avg_on_remaining_100 = 0.0;
        sc.mathematically_possible = true;
    } else if (rem_w <= 1e-6) {
        sc.required_avg_on_remaining_100 = 999.0;
        sc.mathematically_possible = false;
    } else {
        double needed_pts = target_grade - earned;
        sc.required_avg_on_remaining_100 = (needed_pts / rem_w) * 100.0;
        sc.mathematically_possible = (sc.required_avg_on_remaining_100 <= 100.0 + 1e-6);
    }
    return sc;
}

// ============================================================================
// 2. Structured Expediente Estudiantil & Live Guia de Horarios Adapter
// ============================================================================

AcademicStatsSummary ParseAcademicStatsHtml(std::string_view html) {
    AcademicStatsSummary st;
    const std::string plain = StripHtmlTags(html);

    auto find_num_after = [&](const char* pattern, double& dst) {
        std::regex re(pattern, std::regex::icase);
        std::smatch m;
        if (std::regex_search(plain, m, re) && m.size() >= 2) {
            TryParseDoubleStrict(m.str(1), dst);
        }
    };
    auto find_int_after = [&](const char* pattern, int& dst) {
        double v = 0.0;
        find_num_after(pattern, v);
        if (v > 0.0) dst = static_cast<int>(std::lround(v));
    };

    find_num_after(R"(Promedio\s+ponderado\s+general[\s:]*([0-9]+(?:[.,][0-9]+)?))", st.weighted_avg_general);
    find_num_after(R"(Promedio\s+ponderado\s+de\s+cursos\s+aprobados[\s:]*([0-9]+(?:[.,][0-9]+)?))", st.weighted_avg_passed);
    find_num_after(R"(Promedio\s+ponderado\s+de\s+cursos\s+reprobados[\s:]*([0-9]+(?:[.,][0-9]+)?))", st.weighted_avg_failed);
    find_num_after(R"(Percentaje\s+de\s+cursos\s+aprobados[\s:]*([0-9]+(?:[.,][0-9]+)?))", st.passed_pct);
    find_num_after(R"(Percentaje\s+de\s+cursos\s+reprobados[\s:]*([0-9]+(?:[.,][0-9]+)?))", st.failed_pct);
    find_int_after(R"(Total\s+de\s+horas\s+cursadas[\s:]*(\d+))", st.hours_taken);
    if (st.hours_taken == 0) find_int_after(R"((\d+)\s+Horas\s+cursadas)", st.hours_taken);

    // Parse "Detalle del curso" table rows (count + state name)
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() == 2) {
            int cnt = std::atoi(r[0].c_str());
            std::string lbl = AsciiLower(r[1]);
            if (lbl.find("aprobadas") != std::string::npos) st.count_aprobadas = cnt;
            else if (lbl.find("en curso") != std::string::npos) st.count_en_curso = cnt;
            else if (lbl.find("reprobadas") != std::string::npos) st.count_reprobadas = cnt;
            else if (lbl.find("retiradas") != std::string::npos) st.count_retiradas = cnt;
            else if (lbl.find("reconocidas") != std::string::npos) st.count_reconocidas = cnt;
            else if (lbl.find("suficiencia") != std::string::npos) st.count_suficiencia = cnt;
            else if (lbl.find("pendientes") != std::string::npos) st.count_pendientes = cnt;
            else if (lbl.find("congeladas") != std::string::npos) st.count_congeladas = cnt;
        } else if (r.size() >= 3) {
            // Summary table: Category | multi-line Descriptions | multi-line Counts
            std::string cat = AsciiLower(r[0]);
            std::string desc = AsciiLower(r[1]);
            std::vector<double> vals;
            std::istringstream iss(r[2]);
            std::string tok;
            while (iss >> tok) {
                double v = 0.0;
                if (TryParseDoubleStrict(tok, v)) vals.push_back(v);
            }
            if (cat.find("materias") != std::string::npos && vals.size() >= 2) {
                st.subjects_consulted = static_cast<int>(std::lround(vals[0]));
                st.subjects_taken = static_cast<int>(std::lround(vals[1]));
            } else if (cat.find("cr") != std::string::npos && vals.size() >= 3) {
                st.credits_passed = static_cast<int>(std::lround(vals[0]));
                st.credits_failed = static_cast<int>(std::lround(vals[1]));
                st.credits_total = static_cast<int>(std::lround(vals[2]));
                if (vals.size() >= 4) st.credits_no_grade = static_cast<int>(std::lround(vals[3]));
            } else if (cat.find("horas") != std::string::npos && !vals.empty()) {
                st.hours_taken = static_cast<int>(std::lround(vals[0]));
            } else if (cat.find("promedio") != std::string::npos && !vals.empty()) {
                st.weighted_avg_general = vals[0];
                if (vals.size() >= 2) st.weighted_avg_passed = vals[1];
                if (vals.size() >= 3) st.weighted_avg_failed = vals[2];
            } else if (cat.find("cursos") != std::string::npos && vals.size() >= 10) {
                st.count_aprobadas = static_cast<int>(std::lround(vals[0]));
                st.count_en_curso = static_cast<int>(std::lround(vals[1]));
                st.count_reprobadas = static_cast<int>(std::lround(vals[2]));
                st.count_retiradas = static_cast<int>(std::lround(vals[3]));
                st.count_reconocidas = static_cast<int>(std::lround(vals[4]));
                st.count_suficiencia = static_cast<int>(std::lround(vals[5]));
                st.count_pendientes = static_cast<int>(std::lround(vals[6]));
                st.count_congeladas = static_cast<int>(std::lround(vals[7]));
                st.passed_pct = vals[8];
                st.failed_pct = vals[9];
            }
            (void)desc;
        }
    }
    return st;
}

std::vector<GradeRecordRow> ParseDetalleNotasHtml(std::string_view html) {
    std::vector<GradeRecordRow> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 7) continue;
        std::string first_low = AsciiLower(r[0]);
        if (first_low.find("a") != std::string::npos && first_low.find("o") != std::string::npos) continue;  // header "Año"
        int yr = std::atoi(r[0].c_str());
        if (yr < 1970 || yr > 2100) continue;

        GradeRecordRow gr;
        gr.year = yr;
        gr.state = r[1];
        gr.grade_text = r[2];
        TryExtractFirstDouble(r[2], gr.grade);

        // Column 3 "Materia" often contains "EL2113 - Circuitos..." or plain name
        std::string mat = r[3];
        static const std::regex kCodeSplitRe(R"(^([A-Z]{2,4}\d{3,4})\s*[-–:]?\s*(.*)$)");
        std::smatch m;
        if (std::regex_match(mat, m, kCodeSplitRe)) {
            gr.course_code = m.str(1);
            gr.course_name = TrimAscii(m.str(2));
        } else {
            gr.course_name = mat;
        }
        gr.modality = r[4];
        gr.period = r[5];
        gr.credits = std::atoi(r[6].c_str());
        out.push_back(std::move(gr));
    }
    return out;
}

std::vector<WeightedAveragePeriodRow> ParsePromedioPonderadoTableHtml(std::string_view html) {
    std::vector<WeightedAveragePeriodRow> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 4) continue;
        int yr = std::atoi(r[0].c_str());
        if (yr < 1970 || yr > 2100) continue;
        WeightedAveragePeriodRow row;
        row.year = yr;
        row.modality = r[1];
        row.period = r[2];
        TryExtractFirstDouble(r[3], row.weighted_avg);
        out.push_back(std::move(row));
    }
    return out;
}

std::vector<MatriculaAppointment> ParseCitasMatriculaHtml(std::string_view html) {
    std::vector<MatriculaAppointment> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 4) continue;
        if (AsciiLower(r[0]) == "fecha") continue;
        if (r[0].size() < 8) continue;
        MatriculaAppointment a;
        a.date = r[0];
        a.period = r[1];
        a.modality = r[2];
        a.time = r[3];
        out.push_back(std::move(a));
    }
    return out;
}

std::vector<WeeklyScheduleCell> ParseTablaHorarioHtml(std::string_view html) {
    std::vector<WeeklyScheduleCell> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 6) continue;
        if (AsciiLower(r[0]).find("horario") != std::string::npos) continue;
        std::string time_range = r[0];
        int start_m = 0, end_m = 0;
        {
            static const std::regex kRangeRe(R"((\d{1,2}:\d{2}(?:\s*[ap]m)?)\s*[-–a]\s*(\d{1,2}:\d{2}(?:\s*[ap]m)?))",
                                             std::regex::icase);
            std::smatch m;
            if (std::regex_search(time_range, m, kRangeRe)) {
                start_m = std::max(0, ParseTimeMinutes(m.str(1)));
                end_m = std::max(start_m + 30, ParseTimeMinutes(m.str(2)));
            }
        }
        for (size_t col = 1; col < r.size() && col <= 7; ++col) {
            if (r[col].empty() || r[col] == "-" || r[col] == "--") continue;
            WeeklyScheduleCell cell;
            cell.day_index_0_mon = static_cast<int>(col - 1);
            cell.time_range = time_range;
            cell.start_minute = start_m;
            cell.end_minute = end_m;
            cell.raw_text = r[col];
            static const std::regex kCodeRe(R"(\b([A-Z]{2,4}\d{3,4})\b)");
            std::smatch cm;
            if (std::regex_search(cell.raw_text, cm, kCodeRe)) {
                cell.course_code = cm.str(1);
            }
            static const std::regex kGrpRe(R"((?:GR|Grupo)\s*\.?\s*(\d+))", std::regex::icase);
            if (std::regex_search(cell.raw_text, cm, kGrpRe)) {
                cell.group = cm.str(1);
            }
            static const std::regex kRoomRe(R"((?:Aula|Lab)\s*:?\s*([A-Za-z0-9-]+))", std::regex::icase);
            if (std::regex_search(cell.raw_text, cm, kRoomRe)) {
                cell.room = cm.str(1);
            }
            cell.course_name = cell.raw_text;
            out.push_back(std::move(cell));
        }
    }
    return out;
}

std::vector<GuiaHorarioRow> ParseTablaGuiaHorarioHtml(std::string_view html) {
    std::vector<GuiaHorarioRow> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 8) continue;
        std::string c0_low = AsciiLower(r[0]);
        if (c0_low.find("c") != std::string::npos && c0_low.find("digo") != std::string::npos) continue;  // header "Código"
        if (c0_low.find("no hay informaci") != std::string::npos) continue;
        if (r[0].empty() || r[1].empty()) continue;

        GuiaHorarioRow g;
        g.codigo = r[0];
        g.materia = r[1];
        g.grupo = r[2];
        g.creditos = std::atoi(r[3].c_str());
        g.horario_raw = r[4];
        g.aula = r[5];
        g.profesor = r[6];
        g.cupo = r[7].empty() ? -1 : std::atoi(r[7].c_str());
        if (r.size() > 8) g.tipo_materia = r[8];
        if (r.size() > 9) g.tipo_grupo = r[9];
        if (r.size() > 10) g.reservados = std::atoi(r[10].c_str());
        out.push_back(std::move(g));
    }
    return out;
}

std::string GuiaHorarioRowsToHorariosTecJson(const std::vector<GuiaHorarioRow>& rows,
                                             std::string_view title,
                                             std::string_view term,
                                             std::string_view campus) {
    struct AggGroup {
        std::string display_id;
        std::string professor;
        std::string modality;
        int cupo = -1;
        int reservados = 0;
        nlohmann::json sessions = nlohmann::json::array();
        std::set<std::string> seen_session_keys;
    };
    struct AggCourse {
        std::string code;
        std::string name;
        int credits = 0;
        std::vector<std::string> group_order;
        std::unordered_map<std::string, AggGroup> groups_by_key;
    };

    auto parse_sessions_into = [&](const std::string& horario_raw,
                                   const std::string& aula,
                                   AggGroup& target_group) {
        // Supports live TEC format ("Lunes - 7:55:9:40", "Jueves - 7:0:8:45", "Miércoles - 12:30:16:5",
        // "Miercoles - 12:30:16:5", "Sábado - 8:0:11:30") AND legacy short formats ("L-M 07:30-09:20").
        const std::string norm_raw = FoldSpanishAccentsAscii(horario_raw);
        static const std::regex kSessRe(
            R"((Lunes|Martes|Miercoles|Jueves|Viernes|Sabado|Domingo|[LKMJVSD](?:\s*[-/,]\s*[LKMJVSD])*)\s*(?:-\s*)?(\d{1,2}:\d{1,2})\s*(?:[-a:]\s*)(\d{1,2}:\d{1,2}))",
            std::regex::icase);
        for (std::sregex_iterator it(norm_raw.begin(), norm_raw.end(), kSessRe), end; it != end; ++it) {
            std::string days_tok = (*it)[1].str();
            int sm = ParseTimeMinutes((*it)[2].str());
            int em = ParseTimeMinutes((*it)[3].str());
            if (sm < 0 || em <= sm) continue;

            std::vector<std::string> day_codes;
            std::string low_d = AsciiLower(days_tok);
            if (low_d.find("lunes") != std::string::npos) day_codes.push_back("L");
            else if (low_d.find("martes") != std::string::npos) day_codes.push_back("K");
            else if (low_d.find("mi") != std::string::npos && low_d.find("rcoles") != std::string::npos) day_codes.push_back("M");
            else if (low_d.find("jueves") != std::string::npos) day_codes.push_back("J");
            else if (low_d.find("viernes") != std::string::npos) day_codes.push_back("V");
            else if (low_d.find("bado") != std::string::npos) day_codes.push_back("S");
            else if (low_d.find("domingo") != std::string::npos) day_codes.push_back("D");
            else {
                for (char ch : days_tok) {
                    char up = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                    if (up == 'L' || up == 'K' || up == 'M' || up == 'J' || up == 'V' || up == 'S' || up == 'D') {
                        day_codes.emplace_back(1, up);
                    }
                }
            }

            std::string start_str = FormatMinutesHHMM(sm);
            std::string end_str = FormatMinutesHHMM(em);
            for (const auto& dc : day_codes) {
                std::string sk = dc + "|" + start_str + "|" + end_str;
                if (!target_group.seen_session_keys.insert(sk).second) continue;
                nlohmann::json s;
                s["day"] = dc;
                s["start"] = start_str;
                s["end"] = end_str;
                if (!aula.empty()) s["room"] = aula;
                if (!campus.empty()) s["campus"] = std::string(campus);
                target_group.sessions.push_back(std::move(s));
            }
        }
    };

    std::vector<std::string> order;
    std::unordered_map<std::string, AggCourse> by_code;
    for (const auto& row : rows) {
        auto it = by_code.find(row.codigo);
        if (it == by_code.end()) {
            order.push_back(row.codigo);
            AggCourse ac;
            ac.code = row.codigo;
            ac.name = row.materia;
            ac.credits = row.creditos;
            it = by_code.emplace(row.codigo, std::move(ac)).first;
        }
        auto& ac = it->second;
        if (ac.credits <= 0 && row.creditos > 0) ac.credits = row.creditos;
        if (ac.name.empty() && !row.materia.empty()) ac.name = row.materia;

        std::string gkey = NormalizeGroupMergeKey(row.grupo);
        auto git = ac.groups_by_key.find(gkey);
        if (git == ac.groups_by_key.end()) {
            ac.group_order.push_back(gkey);
            AggGroup ag;
            ag.display_id = TrimAscii(row.grupo).empty() ? "1" : TrimAscii(row.grupo);
            ag.professor = row.profesor;
            ag.modality = !row.tipo_grupo.empty() ? row.tipo_grupo
                                                   : (!row.tipo_materia.empty() ? row.tipo_materia : "Regular");
            ag.cupo = row.cupo;
            ag.reservados = row.reservados;
            git = ac.groups_by_key.emplace(gkey, std::move(ag)).first;
        } else {
            if (git->second.professor.empty() && !row.profesor.empty()) {
                git->second.professor = row.profesor;
            }
            if ((git->second.modality.empty() || git->second.modality == "Regular") && !row.tipo_grupo.empty()) {
                git->second.modality = row.tipo_grupo;
            }
            if (row.cupo > git->second.cupo) git->second.cupo = row.cupo;
            if (row.reservados > git->second.reservados) git->second.reservados = row.reservados;
        }
        parse_sessions_into(row.horario_raw, row.aula, git->second);
    }

    nlohmann::json root;
    root["title"] = std::string(title);
    root["term"] = std::string(term);
    root["campus"] = std::string(campus);
    root["courses"] = nlohmann::json::array();

    for (const auto& code : order) {
        const auto& ac = by_code[code];
        nlohmann::json cj;
        cj["code"] = ac.code;
        cj["name"] = ac.name;
        cj["credits"] = ac.credits;
        cj["groups"] = nlohmann::json::array();

        for (const auto& gkey : ac.group_order) {
            const auto& gr = ac.groups_by_key.at(gkey);
            nlohmann::json gj;
            gj["id"] = gr.display_id;
            gj["professor"] = gr.professor;
            gj["modality"] = gr.modality.empty() ? "Regular" : gr.modality;
            gj["campus"] = std::string(campus);
            if (gr.cupo >= 0) {
                gj["seatsCapacity"] = gr.cupo;
                gj["seatsAvailable"] = std::max(0, gr.cupo - std::max(0, gr.reservados));
            }
            gj["sessions"] = gr.sessions;
            cj["groups"].push_back(std::move(gj));
        }
        root["courses"].push_back(std::move(cj));
    }

    return root.dump(2);
}

// ============================================================================
// 2b. Public Portal Guia de Horarios (https://tec-appsext.itcr.ac.cr/guiahorarios/)
// ============================================================================

namespace {

bool UnwrapAspNetDArray(std::string_view raw_json,
                        nlohmann::json& out_arr,
                        bool& out_empty_ok,
                        std::string& out_code,
                        std::string& out_msg) {
    out_empty_ok = false;
    auto j = nlohmann::json::parse(raw_json, nullptr, false);
    if (j.is_discarded()) {
        out_code = "invalid_json";
        out_msg = "JSON inválido recibido de Guía de Horarios Pública";
        return false;
    }
    if (j.is_array()) {
        out_arr = std::move(j);
        return true;
    }
    if (!j.is_object() || !j.contains("d")) {
        out_code = "invalid_schema";
        out_msg = "Respuesta sin propiedad ASP.NET 'd'";
        return false;
    }
    const auto& d = j["d"];
    if (d.is_array()) {
        out_arr = d;
        return true;
    }
    if (d.is_null()) {
        out_empty_ok = true;
        return true;
    }
    if (d.is_string()) {
        std::string ds = TrimAscii(d.get<std::string>());
        std::string low = AsciiLower(FoldSpanishAccentsAscii(ds));
        if (ds.empty() || low == "no datos" || low == "estudiante sin horarios") {
            out_empty_ok = true;
            return true;
        }
        if (low == "false") {
            out_code = "server_false";
            out_msg = "Guía de Horarios Pública devolvió 'false'";
            return false;
        }
        out_arr = nlohmann::json::parse(ds, nullptr, false);
        if (out_arr.is_discarded() || !out_arr.is_array()) {
            out_code = "invalid_schema";
            out_msg = "El campo 'd' no contiene un arreglo JSON válido";
            return false;
        }
        return true;
    }
    out_code = "invalid_schema";
    out_msg = "Tipo inesperado en campo 'd'";
    return false;
}

std::string PgJsonStr(const nlohmann::json& obj, const char* key) {
    if (!obj.is_object() || !obj.contains(key)) return {};
    const auto& v = obj[key];
    if (v.is_string()) return TrimAscii(v.get<std::string>());
    if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
    if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    return {};
}

int PgJsonInt(const nlohmann::json& obj, const char* key, int fallback = 0) {
    if (!obj.is_object() || !obj.contains(key)) return fallback;
    const auto& v = obj[key];
    if (v.is_number_integer()) return static_cast<int>(v.get<int64_t>());
    if (v.is_number_unsigned()) return static_cast<int>(v.get<uint64_t>());
    if (v.is_number_float()) return static_cast<int>(v.get<double>());
    if (v.is_string()) {
        std::string s = TrimAscii(v.get<std::string>());
        return s.empty() ? fallback : std::atoi(s.c_str());
    }
    return fallback;
}

std::string PublicGuiaDayToCode(std::string_view nom_dia) {
    std::string d = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(nom_dia)));
    if (d == "l" || d == "lun" || d.find("lunes") != std::string::npos) return "L";
    if (d == "k" || d == "mar" || d.find("martes") != std::string::npos) return "K";
    if (d == "m" || d == "mie" || d.find("miercoles") != std::string::npos) return "M";
    if (d == "j" || d == "jue" || d.find("jueves") != std::string::npos) return "J";
    if (d == "v" || d == "vie" || d.find("viernes") != std::string::npos) return "V";
    if (d == "s" || d == "sab" || d.find("sabado") != std::string::npos) return "S";
    if (d == "d" || d == "dom" || d.find("domingo") != std::string::npos) return "D";
    return {};
}

std::string PublicGuiaCampusShort(std::string_view dsc_sede) {
    std::string s = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(dsc_sede)));
    if (s.find("cartago") != std::string::npos || s.find("central") != std::string::npos) return "CA";
    if (s.find("san jose") != std::string::npos) return "SJ";
    if (s.find("san carlos") != std::string::npos) return "SC";
    if (s.find("alajuela") != std::string::npos) return "AL";
    if (s.find("limon") != std::string::npos) return "LI";
    std::string letters;
    for (char ch : s) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            letters.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
    }
    if (letters.size() > 3) letters.resize(3);
    return letters.empty() ? "TEC" : letters;
}

}  // namespace

Result<std::vector<PublicGuiaEscuelaInfo>> ParsePublicGuiaEscuelasResponse(std::string_view raw_json) {
    nlohmann::json arr;
    bool empty_ok = false;
    std::string code, msg;
    if (!UnwrapAspNetDArray(raw_json, arr, empty_ok, code, msg)) {
        return Result<std::vector<PublicGuiaEscuelaInfo>>::Failure(code, msg);
    }
    std::vector<PublicGuiaEscuelaInfo> out;
    if (empty_ok) return Result<std::vector<PublicGuiaEscuelaInfo>>::Success(std::move(out));
    for (const auto& item : arr) {
        if (!item.is_object()) continue;
        PublicGuiaEscuelaInfo e;
        e.ide_depto = PgJsonStr(item, "IDE_DEPTO");
        e.dsc_depto = PgJsonStr(item, "DSC_DEPTO");
        if (!e.ide_depto.empty() || !e.dsc_depto.empty()) {
            out.push_back(std::move(e));
        }
    }
    return Result<std::vector<PublicGuiaEscuelaInfo>>::Success(std::move(out));
}

Result<std::vector<PublicGuiaModalidadInfo>> ParsePublicGuiaModalidadesResponse(std::string_view raw_json) {
    nlohmann::json arr;
    bool empty_ok = false;
    std::string code, msg;
    if (!UnwrapAspNetDArray(raw_json, arr, empty_ok, code, msg)) {
        return Result<std::vector<PublicGuiaModalidadInfo>>::Failure(code, msg);
    }
    std::vector<PublicGuiaModalidadInfo> out;
    if (empty_ok) return Result<std::vector<PublicGuiaModalidadInfo>>::Success(std::move(out));
    for (const auto& item : arr) {
        if (!item.is_object()) continue;
        PublicGuiaModalidadInfo m;
        m.ide_modalidad = PgJsonStr(item, "IDE_MODALIDAD");
        m.nombre = PgJsonStr(item, "NOMBRE");
        m.cant_periodos = std::max(1, PgJsonInt(item, "CANT_PERIODOS", 1));
        if (!m.ide_modalidad.empty() || !m.nombre.empty()) {
            out.push_back(std::move(m));
        }
    }
    return Result<std::vector<PublicGuiaModalidadInfo>>::Success(std::move(out));
}

Result<std::vector<PublicGuiaSessionRow>> ParsePublicGuiaRowsResponse(std::string_view raw_json) {
    nlohmann::json arr;
    bool empty_ok = false;
    std::string code, msg;
    if (!UnwrapAspNetDArray(raw_json, arr, empty_ok, code, msg)) {
        return Result<std::vector<PublicGuiaSessionRow>>::Failure(code, msg);
    }
    std::vector<PublicGuiaSessionRow> out;
    if (empty_ok) return Result<std::vector<PublicGuiaSessionRow>>::Success(std::move(out));
    out.reserve(arr.size());
    for (const auto& item : arr) {
        if (!item.is_object()) continue;
        PublicGuiaSessionRow r;
        r.dsc_sede = PgJsonStr(item, "DSC_SEDE");
        r.ide_materia = PgJsonStr(item, "IDE_MATERIA");
        r.dsc_materia = PgJsonStr(item, "DSC_MATERIA");
        r.ide_grupo = PgJsonStr(item, "IDE_GRUPO");
        r.dsc_depto = PgJsonStr(item, "DSC_DEPTO");
        r.can_creditos = PgJsonInt(item, "CAN_CREDITOS", 0);
        r.horas = PgJsonInt(item, "HORAS", 0);
        r.nom_dia = PgJsonStr(item, "NOM_DIA");
        r.ide_modalidad = PgJsonStr(item, "IDE_MODALIDAD");
        r.ide_per_mod = PgJsonStr(item, "IDE_PER_MOD");
        r.num_ano = PgJsonInt(item, "NUM_ANO", 0);
        r.dsc_modalidad = PgJsonStr(item, "DSC_MODALIDAD");
        r.tipo_curso = PgJsonStr(item, "TIPO_CURSO");
        r.hinicio = PgJsonStr(item, "HINICIO");
        r.hfin = PgJsonStr(item, "HFIN");
        r.nom_profesor = PgJsonStr(item, "NOM_PROFESOR");
        r.reserva_sede = PgJsonStr(item, "RESERVA_SEDE");
        r.reserva_depto = PgJsonStr(item, "RESERVA_DEPTO");
        r.reserva_plan = PgJsonStr(item, "RESERVA_PLAN");
        r.ide_est_est = PgJsonStr(item, "IDE_EST_EST");
        r.dsc_plan = PgJsonStr(item, "DSC_PLAN");
        r.num_nivel = PgJsonStr(item, "NUM_NIVEL");
        if (!r.ide_materia.empty() || !r.dsc_materia.empty()) {
            out.push_back(std::move(r));
        }
    }
    return Result<std::vector<PublicGuiaSessionRow>>::Success(std::move(out));
}

std::vector<PublicGuiaSessionRow> FilterPublicGuiaRows(
    const std::vector<PublicGuiaSessionRow>& rows,
    const PublicGuiaFilterOptions& filter) {
    const std::string sede_needle = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(filter.sede_filter)));
    const std::string mod_needle = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(filter.modalidad_filter)));
    const std::string per_raw = TrimAscii(filter.periodo_filter);

    std::string per_mod_part;
    std::string per_num_part;
    std::string per_yr_part;
    bool per_is_composite = false;
    if (!per_raw.empty()) {
        std::vector<std::string> parts;
        size_t pos = 0;
        while (pos <= per_raw.size()) {
            size_t d = per_raw.find('-', pos);
            if (d == std::string_view::npos) d = per_raw.size();
            parts.emplace_back(per_raw.substr(pos, d - pos));
            if (d == per_raw.size()) break;
            pos = d + 1;
        }
        if (parts.size() >= 2 && !TrimAscii(parts[0]).empty() && !TrimAscii(parts[1]).empty()) {
            per_is_composite = true;
            per_mod_part = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(parts[0])));
            per_num_part = TrimAscii(parts[1]);
            if (parts.size() >= 3) per_yr_part = TrimAscii(parts[2]);
        }
    }

    std::set<std::string> code_set;
    for (const auto& c : filter.course_codes_filter) {
        std::string nc = AsciiLower(TrimAscii(c));
        if (!nc.empty()) code_set.insert(std::move(nc));
    }

    std::vector<PublicGuiaSessionRow> out;
    out.reserve(rows.size());
    for (const auto& r : rows) {
        if (!sede_needle.empty()) {
            std::string rs = AsciiLower(FoldSpanishAccentsAscii(r.dsc_sede));
            if (rs.find(sede_needle) == std::string::npos) continue;
        }
        if (!mod_needle.empty()) {
            std::string im = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(r.ide_modalidad)));
            std::string dm = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(r.dsc_modalidad)));
            if (im != mod_needle && dm.find(mod_needle) == std::string::npos) continue;
        }
        if (!per_raw.empty()) {
            if (per_is_composite) {
                std::string im = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(r.ide_modalidad)));
                if (im != per_mod_part) continue;
                if (TrimAscii(r.ide_per_mod) != per_num_part) continue;
                if (!per_yr_part.empty() && r.num_ano > 0 && std::to_string(r.num_ano) != per_yr_part) {
                    continue;
                }
            } else {
                std::string per_low = AsciiLower(FoldSpanishAccentsAscii(per_raw));
                std::string comb = AsciiLower(FoldSpanishAccentsAscii(r.dsc_modalidad)) + " " +
                                   TrimAscii(r.ide_per_mod);
                if (TrimAscii(r.ide_per_mod) != per_raw && comb.find(per_low) == std::string::npos) {
                    continue;
                }
            }
        }
        if (!code_set.empty()) {
            if (code_set.count(AsciiLower(TrimAscii(r.ide_materia))) == 0) continue;
        }
        if (filter.exclude_reserved) {
            if (!TrimAscii(r.reserva_sede).empty() ||
                !TrimAscii(r.reserva_depto).empty() ||
                !TrimAscii(r.reserva_plan).empty()) {
                continue;
            }
        }
        out.push_back(r);
    }
    return out;
}

std::vector<GuiaHorarioRow> PublicGuiaRowsToGuiaHorarioRows(
    const std::vector<PublicGuiaSessionRow>& rows,
    const PublicGuiaFilterOptions& filter) {
    const auto filtered = FilterPublicGuiaRows(rows, filter);
    std::vector<GuiaHorarioRow> out;
    out.reserve(filtered.size());
    for (const auto& r : filtered) {
        GuiaHorarioRow g;
        g.codigo = r.ide_materia;
        g.materia = r.dsc_materia;
        g.grupo = r.ide_grupo;
        g.creditos = r.can_creditos;
        if (!r.nom_dia.empty() && !r.hinicio.empty() && !r.hfin.empty()) {
            g.horario_raw = r.nom_dia + " - " + r.hinicio + "-" + r.hfin;
        }
        g.profesor = r.nom_profesor;
        g.cupo = -1;
        g.tipo_materia = r.dsc_modalidad;
        g.tipo_grupo = r.tipo_curso;
        g.reservados = (!TrimAscii(r.reserva_sede).empty() ||
                        !TrimAscii(r.reserva_depto).empty() ||
                        !TrimAscii(r.reserva_plan).empty()) ? 1 : 0;
        out.push_back(std::move(g));
    }
    return out;
}

std::string PublicGuiaRowsToHorariosTecJson(
    const std::vector<PublicGuiaSessionRow>& rows,
    const PublicGuiaFilterOptions& filter,
    std::string_view title) {
    const auto filtered = FilterPublicGuiaRows(rows, filter);

    struct IntervalSpan {
        std::string day;
        int start_min = 0;
        int end_min = 0;
    };
    struct AggGroup {
        std::string base_id;
        std::string campus;
        std::string campus_short;
        std::string mod_per_suffix;
        std::string professor;
        std::string modality;
        nlohmann::json sessions = nlohmann::json::array();
        std::vector<IntervalSpan> spans;
    };
    struct AggCourse {
        std::string code;
        std::string name;
        int credits = 0;
        std::vector<std::string> group_keys;
        std::unordered_map<std::string, AggGroup> groups;
    };

    std::vector<std::string> course_order;
    std::unordered_map<std::string, AggCourse> by_code;
    std::set<std::string> unique_campuses;
    std::string inferred_term;

    for (const auto& r : filtered) {
        const std::string code = TrimAscii(r.ide_materia);
        if (code.empty()) continue;

        if (!r.dsc_sede.empty()) unique_campuses.insert(TrimAscii(r.dsc_sede));
        if (inferred_term.empty() && r.num_ano > 0) {
            inferred_term = std::to_string(r.num_ano);
            if (!r.dsc_modalidad.empty()) inferred_term += " " + TrimAscii(r.dsc_modalidad);
            if (!r.ide_per_mod.empty()) inferred_term += " P" + TrimAscii(r.ide_per_mod);
        }

        auto cit = by_code.find(code);
        if (cit == by_code.end()) {
            course_order.push_back(code);
            AggCourse ac;
            ac.code = code;
            ac.name = TrimAscii(r.dsc_materia);
            ac.credits = r.can_creditos;
            cit = by_code.emplace(code, std::move(ac)).first;
        }
        auto& ac = cit->second;
        if (ac.name.empty() && !r.dsc_materia.empty()) ac.name = TrimAscii(r.dsc_materia);
        if (ac.credits <= 0 && r.can_creditos > 0) ac.credits = r.can_creditos;

        const std::string base_grp = TrimAscii(r.ide_grupo).empty() ? "1" : TrimAscii(r.ide_grupo);
        const std::string g_merge = NormalizeGroupMergeKey(base_grp);
        const std::string sede_norm = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(r.dsc_sede)));
        const std::string mod_norm = AsciiLower(TrimAscii(r.ide_modalidad));
        const std::string per_norm = TrimAscii(r.ide_per_mod);
        const std::string tipo_norm = AsciiLower(FoldSpanishAccentsAscii(TrimAscii(r.tipo_curso)));
        const std::string gkey = g_merge + "|" + sede_norm + "|" + mod_norm + "|" + per_norm + "|" + tipo_norm;

        auto git = ac.groups.find(gkey);
        if (git == ac.groups.end()) {
            ac.group_keys.push_back(gkey);
            AggGroup ag;
            ag.base_id = base_grp;
            ag.campus = TrimAscii(r.dsc_sede);
            ag.campus_short = r.dsc_sede.empty() ? "" : PublicGuiaCampusShort(r.dsc_sede);
            if (!r.ide_modalidad.empty() || !r.ide_per_mod.empty()) {
                ag.mod_per_suffix = TrimAscii(r.ide_modalidad) + TrimAscii(r.ide_per_mod);
            }
            ag.professor = TrimAscii(r.nom_profesor);
            ag.modality = !TrimAscii(r.tipo_curso).empty()
                              ? TrimAscii(r.tipo_curso)
                              : (!TrimAscii(r.dsc_modalidad).empty() ? TrimAscii(r.dsc_modalidad) : "Regular");
            git = ac.groups.emplace(gkey, std::move(ag)).first;
        } else {
            if (git->second.professor.empty() && !r.nom_profesor.empty()) {
                git->second.professor = TrimAscii(r.nom_profesor);
            }
        }

        const std::string day_code = PublicGuiaDayToCode(r.nom_dia);
        const int sm = ParseTimeMinutes(r.hinicio);
        const int em = ParseTimeMinutes(r.hfin);
        if (!day_code.empty() && sm >= 0 && em > sm) {
            bool overlaps = false;
            for (const auto& sp : git->second.spans) {
                if (sp.day == day_code && sm < sp.end_min && sp.start_min < em) {
                    overlaps = true;
                    break;
                }
            }
            if (!overlaps) {
                git->second.spans.push_back({day_code, sm, em});
                nlohmann::json s;
                s["day"] = day_code;
                s["start"] = FormatMinutesHHMM(sm);
                s["end"] = FormatMinutesHHMM(em);
                if (!r.dsc_sede.empty()) s["campus"] = TrimAscii(r.dsc_sede);
                git->second.sessions.push_back(std::move(s));
            }
        }
    }

    nlohmann::json root;
    root["title"] = std::string(title);
    root["term"] = !TrimAscii(filter.periodo_filter).empty() ? TrimAscii(filter.periodo_filter) : inferred_term;
    root["campus"] = (unique_campuses.size() == 1) ? *unique_campuses.begin() : std::string{};
    root["courses"] = nlohmann::json::array();

    for (const auto& code : course_order) {
        const auto& ac = by_code[code];
        // Determine whether base_id collides across campuses or modality/periods in this course
        std::unordered_map<std::string, std::set<std::string>> campuses_per_base;
        std::unordered_map<std::string, std::set<std::string>> periods_per_base;
        for (const auto& gk : ac.group_keys) {
            const auto& gr = ac.groups.at(gk);
            std::string b = NormalizeGroupMergeKey(gr.base_id);
            if (!gr.campus_short.empty()) campuses_per_base[b].insert(gr.campus_short);
            if (!gr.mod_per_suffix.empty()) periods_per_base[b].insert(gr.mod_per_suffix);
        }

        nlohmann::json cj;
        cj["code"] = ac.code;
        cj["name"] = ac.name;
        cj["credits"] = ac.credits;
        cj["groups"] = nlohmann::json::array();

        std::set<std::string> used_group_ids;
        for (const auto& gk : ac.group_keys) {
            const auto& gr = ac.groups.at(gk);
            std::string b = NormalizeGroupMergeKey(gr.base_id);
            std::string final_id = gr.base_id;
            if (campuses_per_base[b].size() > 1 && !gr.campus_short.empty()) {
                final_id += "-" + gr.campus_short;
            }
            if (periods_per_base[b].size() > 1 && !gr.mod_per_suffix.empty()) {
                final_id += "-" + gr.mod_per_suffix;
            }
            std::string cand = final_id;
            int seq = 2;
            while (!used_group_ids.insert(AsciiLower(cand)).second) {
                cand = final_id + "#" + std::to_string(seq++);
            }

            nlohmann::json gj;
            gj["id"] = cand;
            gj["professor"] = gr.professor;
            gj["modality"] = gr.modality.empty() ? "Regular" : gr.modality;
            if (!gr.campus.empty()) gj["campus"] = gr.campus;
            gj["sessions"] = gr.sessions;
            cj["groups"].push_back(std::move(gj));
        }
        root["courses"].push_back(std::move(cj));
    }

    return root.dump(2);
}

// ============================================================================
// 3. Course Portada, Unified Calendar, Plagio, SMS, GAAP, User Model, Foros
// ============================================================================

CourseOverviewSnapshot ParseCourseOverviewHtml(std::string_view html,
                                               std::string_view community_path,
                                               std::string_view base_url) {
    CourseOverviewSnapshot snap;
    snap.community_path = std::string(community_path);
    std::string raw_s(html);

    snap.has_checkpoint_icon = (raw_s.find("checkpointIcon") != std::string::npos ||
                                raw_s.find("bookmark_check") != std::string::npos);

    // Title (read before chrome strip)
    {
        static const std::regex kTitleRe(R"(<title[^>]*>([\s\S]*?)</title>)", std::regex::icase);
        std::smatch m;
        if (std::regex_search(raw_s, m, kTitleRe)) {
            snap.title = CollapseSpaces(StripHtmlTags(m.str(1)));
        }
    }

    // Strip global #header-td and user panel so portal links never pollute course tabs/resources
    const std::string s = StripGlobalPortalChrome(raw_s);

    // Tabs (links with page_num=N, supporting custom catedra tabs up to page_num=55+)
    {
        static const std::regex kTabRe(
            R"(<a[^>]*href\s*=\s*["']([^"']*page_num=(\d+)[^"']*)["'][^>]*>([\s\S]*?)</a>)",
            std::regex::icase);
        std::set<int> seen_pages;
        for (std::sregex_iterator it(s.begin(), s.end(), kTabRe), end; it != end; ++it) {
            int pnum = std::atoi((*it)[2].str().c_str());
            if (!seen_pages.insert(pnum).second) continue;
            CourseTabInfo ti;
            ti.page_num = pnum;
            ti.url = ResolveRelativeUrl(base_url, HeadlessCrawler::HtmlUnescape((*it)[1].str()));
            ti.label = CollapseSpaces(StripHtmlTags((*it)[3].str()));
            // Remove trailing "bookmark_check" icon text if present
            size_t bm = ti.label.find("bookmark_check");
            if (bm != std::string::npos) ti.label = TrimAscii(ti.label.substr(0, bm));
            std::string lbl_low = AsciiLower(ti.label);
            const bool is_std = (lbl_low == "portada" || lbl_low == "calendario" ||
                                 lbl_low == "documentos" || lbl_low == "evaluaciones" ||
                                 lbl_low == "home" || lbl_low == "calendar" ||
                                 lbl_low == "documents" || lbl_low == "evaluations");
            ti.is_custom = (!is_std && pnum >= 1) || pnum >= 4;
            if (!ti.label.empty()) snap.tabs.push_back(std::move(ti));
        }
    }

    // Portlet titles + detect weekly sections (Week 1 / Semana 01)
    {
        static const std::regex kPortRe(
            R"(class\s*=\s*["'][^"']*portlet-title[^"']*["'][^>]*>([\s\S]*?)</)",
            std::regex::icase);
        for (std::sregex_iterator it(s.begin(), s.end(), kPortRe), end; it != end; ++it) {
            std::string pt = CollapseSpaces(StripHtmlTags((*it)[1].str()));
            if (pt.empty()) continue;
            snap.portlet_titles.push_back(pt);
            std::string low = AsciiLower(pt);
            if (low.rfind("week ", 0) == 0 || low.rfind("semana", 0) == 0) {
                snap.weekly_sections.push_back(pt);
            }
        }
    }

    // External & official document links
    {
        static const std::regex kLinkRe(
            R"(<a[^>]*href\s*=\s*["']([^"']+)["'][^>]*>([\s\S]*?)</a>)",
            std::regex::icase);
        std::set<std::string> seen_urls;
        for (std::sregex_iterator it(s.begin(), s.end(), kLinkRe), end; it != end; ++it) {
            std::string raw_href = HeadlessCrawler::HtmlUnescape((*it)[1].str());
            std::string lbl = CollapseSpaces(StripHtmlTags((*it)[2].str()));
            if (raw_href.empty() || raw_href[0] == '#' || raw_href.rfind("javascript:", 0) == 0) continue;
            if (raw_href.find("mailto:") == 0) {
                if (snap.teacher.email.empty()) snap.teacher.email = raw_href.substr(7);
                continue;
            }
            std::string full_url = ResolveRelativeUrl(base_url, raw_href);
            if (!seen_urls.insert(full_url).second) continue;

            std::string cat = ClassifyExternalLink(lbl, full_url);
            if (cat == "Programa" || cat == "Cronograma") {
                snap.official_docs.push_back({lbl.empty() ? cat : lbl, full_url, cat});
            } else if (cat == "SharePoint" || cat == "YouTube" || cat == "Teams" || cat == "Zoom" ||
                       (cat == "PDF" && full_url.find("/file-storage/view/") != std::string::npos)) {
                snap.external_links.push_back({lbl.empty() ? full_url : lbl, full_url, cat});
            }
        }
    }

    // Extract teacher card clues from plain text (office, consultation hours, Teams)
    const std::string plain = StripHtmlTags(s);
    {
        static const std::regex kOffRe(R"(Oficina\s*:\s*([^\n]+))", std::regex::icase);
        std::smatch m;
        if (std::regex_search(plain, m, kOffRe)) snap.teacher.office = CollapseSpaces(m.str(1));

        static const std::regex kTeamsRe(R"(Teams\s*:\s*([^\n]+))", std::regex::icase);
        if (std::regex_search(plain, m, kTeamsRe)) snap.teacher.teams_handle = CollapseSpaces(m.str(1));

        static const std::regex kConsRe(
            R"((?:Lunes|Martes|Mi[eé]rcoles|Jueves|Viernes)\s+\d{1,2}:\d{2}\s*[-–a]\s*\d{1,2}:\d{2}(?:\s*[ap]m)?)",
            std::regex::icase);
        for (std::sregex_iterator it(plain.begin(), plain.end(), kConsRe), end; it != end; ++it) {
            snap.teacher.consultation_hours.push_back(CollapseSpaces(it->str(0)));
        }
    }

    return snap;
}

std::vector<CalendarEventEntry> ParseCalendarListHtml(std::string_view html) {
    std::vector<CalendarEventEntry> out;
    const auto rows = ExtractHtmlTableRows(html);
    int idx = 0;
    for (const auto& r : rows) {
        if (r.size() < 4) continue;
        if (AsciiLower(r[0]) == "fecha" || r[0].find("Sin eventos") != std::string::npos) continue;
        CalendarEventEntry ev;
        ev.uid = "tec-cal-" + std::to_string(++idx);
        std::string iso = ConvertDmyToIso(r[0]);
        ev.date_iso = iso.empty() ? r[0] : iso.substr(0, 10);
        // Parse time range "08:00 - 10:00" or "08:00"
        {
            static const std::regex kTR(R"((\d{1,2}:\d{2})(?:\s*[-–]\s*(\d{1,2}:\d{2}))?)");
            std::smatch m;
            if (std::regex_search(r[1], m, kTR)) {
                ev.start_time = m.str(1);
                ev.end_time = m[2].matched ? m.str(2) : m.str(1);
            }
        }
        ev.title = r[2];
        ev.calendar_name = r[3];
        if (r.size() > 4) ev.description = r[4];
        ev.source_kind = "calendar";
        if (!ev.title.empty()) out.push_back(std::move(ev));
    }
    return out;
}

std::vector<CalendarEventEntry> BuildUnifiedCalendarEvents(
    const std::vector<CalendarEventEntry>& portal_events,
    const std::vector<DetailedEvaluationsReport>& evals,
    const std::vector<MatriculaAppointment>& citas) {
    std::vector<CalendarEventEntry> out = portal_events;

    int eval_idx = 0;
    for (const auto& rep : evals) {
        for (const auto& cat : rep.categories) {
            for (const auto& it : cat.items) {
                if (it.due_date_iso.size() < 10) continue;
                CalendarEventEntry ev;
                ev.uid = "tec-eval-" + std::to_string(++eval_idx) + "-" + it.assign_id;
                ev.date_iso = it.due_date_iso.substr(0, 10);
                if (it.due_date_iso.size() >= 16) {
                    ev.start_time = it.due_date_iso.substr(11, 5);
                    ev.end_time = ev.start_time;
                } else {
                    ev.start_time = "23:59";
                    ev.end_time = "23:59";
                }
                ev.title = "Entrega: " + it.name;
                ev.calendar_name = rep.community_slug;
                ev.description = "Rubro: " + cat.name + " | Valor: " +
                                 std::to_string(static_cast<int>(std::round(it.weight_pct))) + "% | " +
                                 it.submission_status;
                ev.source_kind = "evaluation";
                out.push_back(std::move(ev));
            }
        }
    }

    int cita_idx = 0;
    for (const auto& c : citas) {
        if (c.date.size() < 10) continue;
        CalendarEventEntry ev;
        ev.uid = "tec-cita-" + std::to_string(++cita_idx);
        ev.date_iso = c.date.substr(0, 10);
        ev.start_time = (c.time.size() >= 5) ? c.time.substr(0, 5) : "08:00";
        ev.end_time = ev.start_time;
        ev.title = "Cita de Matrícula (" + c.modality + ")";
        ev.calendar_name = "Admisión y Registro";
        ev.description = "Periodo " + c.period + " - " + c.modality;
        ev.source_kind = "matricula";
        out.push_back(std::move(ev));
    }

    std::sort(out.begin(), out.end(), [](const CalendarEventEntry& a, const CalendarEventEntry& b) {
        if (a.date_iso != b.date_iso) return a.date_iso < b.date_iso;
        if (a.start_time != b.start_time) return a.start_time < b.start_time;
        return a.title < b.title;
    });
    return out;
}

std::string ExportCalendarEventsToIcs(const std::vector<CalendarEventEntry>& events,
                                      std::string_view prod_id) {
    std::ostringstream oss;
    oss << "BEGIN:VCALENDAR\r\n"
        << "VERSION:2.0\r\n"
        << "PRODID:" << prod_id << "\r\n"
        << "CALSCALE:GREGORIAN\r\n"
        << "METHOD:PUBLISH\r\n";

    int seq = 0;
    for (const auto& ev : events) {
        std::string d8 = DigitsOnly(ev.date_iso);
        if (d8.size() < 8) continue;
        d8.resize(8);

        std::string t_start = DigitsOnly(ev.start_time);
        std::string t_end = DigitsOnly(ev.end_time);
        if (t_start.size() == 3) t_start.insert(t_start.begin(), '0');
        if (t_end.size() == 3) t_end.insert(t_end.begin(), '0');

        oss << "BEGIN:VEVENT\r\n"
            << "UID:" << (ev.uid.empty() ? ("cuadernu-ev-" + std::to_string(++seq)) : ev.uid)
            << "@cuadernu.local\r\n";
        if (t_start.size() >= 4) {
            std::string s4 = t_start.substr(0, 4);
            std::string e4 = (t_end.size() >= 4) ? t_end.substr(0, 4) : s4;
            oss << "DTSTART:" << d8 << "T" << s4 << "00\r\n"
                << "DTEND:" << d8 << "T" << e4 << "00\r\n";
        } else {
            oss << "DTSTART;VALUE=DATE:" << d8 << "\r\n";
        }
        oss << "SUMMARY:" << EscapeIcsText(ev.title) << "\r\n";
        if (!ev.description.empty()) {
            oss << "DESCRIPTION:" << EscapeIcsText(ev.description) << "\r\n";
        }
        if (!ev.calendar_name.empty()) {
            oss << "CATEGORIES:" << EscapeIcsText(ev.calendar_name) << "\r\n";
        }
        oss << "END:VEVENT\r\n";
    }
    oss << "END:VCALENDAR\r\n";
    return oss.str();
}

PlagiarismTreeSnapshot ParsePlagiarismTreeJson(std::string_view json_text) {
    PlagiarismTreeSnapshot snap;
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) return snap;
        if (j.contains("MyFiles") && j["MyFiles"].is_object()) {
            const auto& mf = j["MyFiles"];
            if (mf.contains("name") && mf["name"].is_string()) snap.my_files_name = mf["name"].get<std::string>();
            if (mf.contains("isStudent") && mf["isStudent"].is_number()) {
                snap.is_student = (mf["isStudent"].get<int>() != 0);
            }
            if (mf.contains("folder_id")) {
                snap.my_files_folder_id = mf["folder_id"].is_string()
                                              ? mf["folder_id"].get<std::string>()
                                              : mf["folder_id"].dump();
            }
        }
        if (j.contains("periods") && j["periods"].is_string()) {
            snap.periods_raw = j["periods"].get<std::string>();
        }
        if (j.contains("documents") && j["documents"].is_array()) {
            for (const auto& d : j["documents"]) {
                if (!d.is_object()) continue;
                PlagiarismDocRecord rec;
                if (d.contains("doc_uid") && d["doc_uid"].is_string()) rec.doc_uid = d["doc_uid"].get<std::string>();
                if (d.contains("name") && d["name"].is_string()) rec.file_name = d["name"].get<std::string>();
                if (d.contains("status") && d["status"].is_string()) rec.status_code = d["status"].get<std::string>();
                if (d.contains("text") && d["text"].is_string()) rec.status_text = d["text"].get<std::string>();
                if (d.contains("percentaje") && d["percentaje"].is_number()) {
                    rec.similarity_pct = d["percentaje"].get<double>();
                } else if (d.contains("percentage") && d["percentage"].is_number()) {
                    rec.similarity_pct = d["percentage"].get<double>();
                }
                if (d.contains("visibility") && d["visibility"].is_string()) {
                    rec.visibility = d["visibility"].get<std::string>();
                }
                snap.documents.push_back(std::move(rec));
            }
        }
    } catch (...) {
    }
    return snap;
}

std::vector<SmsMessageRecord> ParseSmsTableHtml(std::string_view html) {
    std::vector<SmsMessageRecord> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 4) continue;
        if (AsciiLower(r[0]) == "contenido") continue;
        if (r[0].empty()) continue;
        out.push_back({r[0], r[1], r[2], r[3]});
    }
    return out;
}

std::vector<NotificationSubscriptionItem> ParseNotificationSubscriptionsHtml(std::string_view html) {
    std::vector<NotificationSubscriptionItem> out;
    const auto rows = ExtractHtmlTableRows(html);
    int idx = 0;
    for (const auto& r : rows) {
        if (r.size() < 4) continue;
        if (AsciiLower(r[0]) == "comunidad") continue;
        NotificationSubscriptionItem item;
        item.request_id = "sub-" + std::to_string(++idx);
        item.community = r[0];
        item.notice_type = r[1];
        item.element = r[2];
        item.frequency = r[3];
        out.push_back(std::move(item));
    }
    return out;
}

std::vector<ForumThreadSummary> ParseForumsHtml(std::string_view html, std::string_view base_url) {
    std::vector<ForumThreadSummary> out;
    std::string s(html);
    static const std::regex kMsgRe(
        R"(<a[^>]*href\s*=\s*["']([^"']*message-view\?message_id=(\d+)[^"']*)["'][^>]*>([\s\S]*?)</a>)",
        std::regex::icase);
    std::set<std::string> seen;
    for (std::sregex_iterator it(s.begin(), s.end(), kMsgRe), end; it != end; ++it) {
        std::string mid = (*it)[2].str();
        if (!seen.insert(mid).second) continue;
        ForumThreadSummary th;
        th.message_id = mid;
        th.url = ResolveRelativeUrl(base_url, HeadlessCrawler::HtmlUnescape((*it)[1].str()));
        th.subject = CollapseSpaces(StripHtmlTags((*it)[3].str()));
        if (!th.subject.empty()) out.push_back(std::move(th));
    }
    return out;
}

std::vector<SchoolCatalogEntry> ParseSchoolCatalogHtml(std::string_view html, std::string_view base_url) {
    std::vector<SchoolCatalogEntry> out;
    std::string s(html);
    static const std::regex kSchRe(
        R"(<a[^>]*href\s*=\s*["'](/dotlrn/classes/([A-Za-z0-9_.-]+)/)["'][^>]*>([\s\S]*?)</a>)",
        std::regex::icase);
    std::set<std::string> seen;
    for (std::sregex_iterator it(s.begin(), s.end(), kSchRe), end; it != end; ++it) {
        std::string code = (*it)[2].str();
        if (!seen.insert(code).second) continue;
        SchoolCatalogEntry e;
        e.dept_code = code;
        e.url = ResolveRelativeUrl(base_url, (*it)[1].str());
        e.name = CollapseSpaces(StripHtmlTags((*it)[3].str()));
        if (!e.name.empty()) out.push_back(std::move(e));
    }
    return out;
}

std::vector<GaapActivityRecord> ParseGaapHistoryHtmlOrJson(std::string_view payload) {
    std::vector<GaapActivityRecord> out;
    std::string trimmed = TrimAscii(payload);
    if (trimmed.empty()) return out;

    if (trimmed.front() == '[' || trimmed.front() == '{') {
        try {
            auto j = nlohmann::json::parse(trimmed);
            const nlohmann::json* arr = j.is_array() ? &j
                                                     : (j.is_object() && j.contains("data") && j["data"].is_array()
                                                            ? &j["data"]
                                                            : nullptr);
            if (arr) {
                for (const auto& item : *arr) {
                    if (!item.is_object()) continue;
                    GaapActivityRecord r;
                    if (item.contains("id")) r.activity_id = item["id"].is_string() ? item["id"].get<std::string>() : item["id"].dump();
                    if (item.contains("title") && item["title"].is_string()) r.title = item["title"].get<std::string>();
                    else if (item.contains("nombre") && item["nombre"].is_string()) r.title = item["nombre"].get<std::string>();
                    if (item.contains("type") && item["type"].is_string()) r.activity_type = item["type"].get<std::string>();
                    if (item.contains("date") && item["date"].is_string()) r.date_text = item["date"].get<std::string>();
                    if (item.contains("score") && item["score"].is_number()) r.score = item["score"].get<double>();
                    if (item.contains("status") && item["status"].is_string()) r.status = item["status"].get<std::string>();
                    if (!r.title.empty() || !r.activity_id.empty()) out.push_back(std::move(r));
                }
                return out;
            }
        } catch (...) {
        }
    }

    const auto rows = ExtractHtmlTableRows(trimmed);
    int idx = 0;
    for (const auto& r : rows) {
        if (r.size() < 3) continue;
        if (AsciiLower(r[0]).find("actividad") != std::string::npos || AsciiLower(r[0]) == "nombre") continue;
        GaapActivityRecord g;
        g.activity_id = "gaap-" + std::to_string(++idx);
        g.title = r[0];
        g.date_text = r[1];
        TryExtractFirstDouble(r[2], g.score);
        if (r.size() > 3) g.status = r[3];
        out.push_back(std::move(g));
    }
    return out;
}

UserModelSnapshot ParseUserModelInfoJson(std::string_view json_text) {
    UserModelSnapshot snap;
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) return snap;
        for (auto it = j.begin(); it != j.end(); ++it) {
            UserModelDimension dim;
            dim.dimension_key = it.key();
            dim.title = it.key();
            if (it.value().is_object()) {
                for (auto jt = it.value().begin(); jt != it.value().end(); ++jt) {
                    if (jt.value().is_string()) dim.metrics.emplace_back(jt.key(), jt.value().get<std::string>());
                    else if (jt.value().is_number() || jt.value().is_boolean()) {
                        dim.metrics.emplace_back(jt.key(), jt.value().dump());
                    }
                }
            } else if (it.value().is_string()) {
                dim.metrics.emplace_back(it.key(), it.value().get<std::string>());
            } else if (it.value().is_number() || it.value().is_boolean()) {
                dim.metrics.emplace_back(it.key(), it.value().dump());
            }
            if (!dim.metrics.empty()) snap.dimensions.push_back(std::move(dim));
        }
    } catch (...) {
    }
    return snap;
}

// ============================================================================
// 4. Phase 2 Pure Parsers & Protocol Helpers
// ============================================================================

std::string ConvertPeriodKeyToExpedienteFormat(std::string_view period) {
    std::string p = TrimAscii(period);
    if (p.empty()) return {};
    static const std::regex kDotlrnPerRe(R"(^([A-Za-z]+)-(\d+)-(\d{4})$)");
    std::smatch m;
    if (std::regex_match(p, m, kDotlrnPerRe)) {
        std::string mod = m.str(1);
        for (char& c : mod) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return m.str(3) + "_" + mod + "_" + m.str(2);
    }
    static const std::regex kExpPerRe(R"(^(\d{4})_([A-Za-z]+)_(\d+)$)");
    if (std::regex_match(p, m, kExpPerRe)) {
        std::string mod = m.str(2);
        for (char& c : mod) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return m.str(1) + "_" + mod + "_" + m.str(3);
    }
    return p;
}

std::string ConvertExpedientePeriodToDotlrnFormat(std::string_view period) {
    std::string p = TrimAscii(period);
    if (p.empty()) return {};
    static const std::regex kExpPerRe(R"(^(\d{4})_([A-Za-z]+)_(\d+)$)");
    std::smatch m;
    if (std::regex_match(p, m, kExpPerRe)) {
        std::string mod = m.str(2);
        for (char& c : mod) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return mod + "-" + m.str(3) + "-" + m.str(1);
    }
    return p;
}

std::vector<std::pair<std::string, std::string>> ParseExpedienteComboJsonOrHtml(std::string_view payload) {
    std::vector<std::pair<std::string, std::string>> out;
    std::string trimmed = TrimAscii(payload);
    if (trimmed.empty()) return out;

    if (trimmed.front() == '[' || trimmed.front() == '{') {
        auto j = nlohmann::json::parse(trimmed, nullptr, false);
        if (!j.is_discarded()) {
            const nlohmann::json* arr = nullptr;
            if (j.is_array()) arr = &j;
            else if (j.is_object()) {
                for (const char* k : {"periodos", "sedes", "carreras", "planes", "data"}) {
                    if (j.contains(k) && j[k].is_array()) {
                        arr = &j[k];
                        break;
                    }
                }
            }
            if (arr != nullptr) {
                for (const auto& item : *arr) {
                    if (!item.is_object()) continue;
                    std::string k, v;
                    for (const char* kk : {"key", "id", "codigo", "sede_id", "value"}) {
                        if (item.contains(kk)) {
                            if (item[kk].is_string()) { k = item[kk].get<std::string>(); break; }
                            if (item[kk].is_number_integer()) { k = std::to_string(item[kk].get<int64_t>()); break; }
                        }
                    }
                    for (const char* vk : {"data", "nombre", "name", "label"}) {
                        if (item.contains(vk) && item[vk].is_string()) {
                            v = CollapseSpaces(item[vk].get<std::string>());
                            break;
                        }
                    }
                    if (!k.empty() || !v.empty()) {
                        out.emplace_back(std::move(k), std::move(v));
                    }
                }
                return out;
            }
        }
    }

    static const std::regex kSpanOptRe(
        R"(<(?:span|option)\b([^>]*)>([\s\S]*?)</(?:span|option)>)",
        std::regex::ECMAScript | std::regex::icase);
    for (std::sregex_iterator it(trimmed.begin(), trimmed.end(), kSpanOptRe), end; it != end; ++it) {
        std::string key;
        if (!ExtractAttrValueCI((*it)[1].str(), "value", key)) continue;
        std::string label = CollapseSpaces(StripHtmlTags((*it)[2].str()));
        if (!key.empty() || !label.empty()) {
            out.emplace_back(TrimAscii(key), std::move(label));
        }
    }
    return out;
}

std::vector<GraduateAttributeRow> ParseGraduateAttributesHtml(std::string_view html) {
    std::vector<GraduateAttributeRow> out;
    const auto rows = ExtractHtmlTableRows(html);
    for (const auto& r : rows) {
        if (r.size() < 8) continue;
        std::string c0 = AsciiLower(r[0]);
        if (c0 == "atributo" || c0 == "attribute" || c0.find("no hay") != std::string::npos) continue;
        GraduateAttributeRow g;
        g.attribute_name = r[0];
        g.description = r[1];
        g.courses = r[2];
        TryExtractFirstDouble(r[3], g.evaluated_pct);
        TryExtractFirstDouble(r[4], g.initial_pct);
        TryExtractFirstDouble(r[5], g.medium_pct);
        TryExtractFirstDouble(r[6], g.advanced_pct);
        TryExtractFirstDouble(r[7], g.total_pct);
        if (!g.attribute_name.empty()) {
            out.push_back(std::move(g));
        }
    }
    return out;
}

std::vector<FileVersionRecord> ParseFileVersionsJson(std::string_view json_text) {
    std::vector<FileVersionRecord> out;
    auto j = nlohmann::json::parse(json_text, nullptr, false);
    if (j.is_discarded()) return out;

    const nlohmann::json* arr = nullptr;
    if (j.is_array()) arr = &j;
    else if (j.is_object()) {
        for (const char* k : {"versions", "elements", "data"}) {
            if (j.contains(k) && j[k].is_array()) {
                arr = &j[k];
                break;
            }
        }
    }
    if (arr == nullptr) return out;

    auto str_field = [](const nlohmann::json& obj, const char* key) -> std::string {
        if (!obj.contains(key)) return {};
        const auto& v = obj[key];
        if (v.is_string()) return v.get<std::string>();
        if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
        if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
        return {};
    };

    for (const auto& item : *arr) {
        if (!item.is_object()) continue;
        FileVersionRecord rec;
        rec.title = str_field(item, "title");
        rec.live_revision = str_field(item, "live_revision");
        if (item.contains("author")) {
            if (item["author"].is_object()) rec.author = str_field(item["author"], "name");
            else if (item["author"].is_string()) rec.author = item["author"].get<std::string>();
        }
        rec.mime_type = str_field(item, "mime_type");
        rec.type = str_field(item, "type");
        rec.pretty_type = str_field(item, "pretty_type");
        rec.last_modified_ansi = str_field(item, "last_modified_ansi");
        if (item.contains("content_size")) {
            if (item["content_size"].is_number_unsigned()) {
                rec.content_size = item["content_size"].get<uint64_t>();
            } else if (item["content_size"].is_number_integer()) {
                rec.content_size = static_cast<uint64_t>(std::max<int64_t>(0, item["content_size"].get<int64_t>()));
            } else if (item["content_size"].is_string()) {
                rec.content_size = std::strtoull(item["content_size"].get<std::string>().c_str(), nullptr, 10);
            }
        }
        rec.description = str_field(item, "description");
        rec.fs_type = str_field(item, "fs_type");
        if (item.contains("fs_sort_relevance") && item["fs_sort_relevance"].is_number_integer()) {
            rec.fs_sort_relevance = item["fs_sort_relevance"].get<int>();
        }
        if (item.contains("privileges") && item["privileges"].is_object() &&
            item["privileges"].contains("read_p")) {
            const auto& rp = item["privileges"]["read_p"];
            rec.read_p = rp.is_boolean() ? rp.get<bool>() : (rp.is_number_integer() ? rp.get<int>() != 0 : true);
        }
        if (!rec.title.empty() || !rec.live_revision.empty() || !rec.last_modified_ansi.empty()) {
            out.push_back(std::move(rec));
        }
    }
    return out;
}

std::vector<EvalSubmissionRevision> ParseEvalHistoryHtml(std::string_view html,
                                                         std::string_view base_url) {
    std::vector<EvalSubmissionRevision> out;
    std::string s(html);

    auto parse_block = [&](const std::string& block, bool is_live) {
        EvalSubmissionRevision rev;
        rev.is_live_revision = is_live;
        static const std::regex kHrefRe(
            R"(<a[^>]*\bhref\s*=\s*["']([^"']+)["'][^>]*>([\s\S]*?)</a>)",
            std::regex::ECMAScript | std::regex::icase);
        std::smatch am;
        if (std::regex_search(block, am, kHrefRe)) {
            std::string raw_href = HeadlessCrawler::HtmlUnescape(am.str(1));
            rev.download_url = ResolveRelativeUrl(base_url, raw_href);
            rev.file_name = CollapseSpaces(StripHtmlTags(am.str(2)));
            static const std::regex kRevIdRe(R"(revision_id=(\d+))", std::regex::icase);
            std::smatch rm;
            if (std::regex_search(raw_href, rm, kRevIdRe)) {
                rev.revision_id = rm.str(1);
            }
        }
        std::string plain = CollapseSpaces(StripHtmlTags(block));
        static const std::regex kDateRe(R"((\d{1,2}/\d{1,2}/\d{4}|\d{4}-\d{2}-\d{2}))");
        static const std::regex kTimeRe(R"((\d{1,2}:\d{2}(?:\s*[ap]m)?))", std::regex::icase);
        std::smatch dm;
        if (std::regex_search(plain, dm, kDateRe)) rev.delivered_date = dm.str(1);
        if (std::regex_search(plain, dm, kTimeRe)) rev.delivered_time = dm.str(1);
        if (rev.file_name.empty() && !plain.empty()) {
            rev.file_name = plain;
        }
        if (!rev.download_url.empty() || !rev.revision_id.empty() || !rev.delivered_date.empty()) {
            out.push_back(std::move(rev));
        }
    };

    static const std::regex kLineRe(
        R"(<[^>]*\bclass\s*=\s*["'][^"']*\blineFiles\b[^"']*["'][^>]*>([\s\S]*?)</div>)",
        std::regex::ECMAScript | std::regex::icase);
    for (std::sregex_iterator it(s.begin(), s.end(), kLineRe), end; it != end; ++it) {
        parse_block((*it)[1].str(), false);
    }

    if (out.empty()) {
        static const std::regex kTrRe(R"(<tr\b[^>]*>([\s\S]*?)</tr>)", std::regex::icase);
        for (std::sregex_iterator it(s.begin(), s.end(), kTrRe), end; it != end; ++it) {
            std::string row = (*it)[1].str();
            if (AsciiLower(row).find("revision_id") != std::string::npos ||
                AsciiLower(row).find("view/") != std::string::npos) {
                parse_block(row, false);
            }
        }
    }
    return out;
}

std::optional<EvalSubmissionRevision> ParseEvalLiveRevisionHtml(std::string_view html,
                                                                std::string_view base_url) {
    std::string s(html);
    std::string plain = StripHtmlTags(s);
    std::string low = AsciiLower(plain);
    if (TrimAscii(plain).empty() || low.find("sin entrega") != std::string::npos) {
        return std::nullopt;
    }
    EvalSubmissionRevision rev;
    rev.is_live_revision = true;
    static const std::regex kHrefRe(
        R"(<a[^>]*\bhref\s*=\s*["']([^"']+)["'][^>]*>([\s\S]*?)</a>)",
        std::regex::ECMAScript | std::regex::icase);
    std::smatch am;
    if (std::regex_search(s, am, kHrefRe)) {
        std::string raw_href = HeadlessCrawler::HtmlUnescape(am.str(1));
        rev.download_url = ResolveRelativeUrl(base_url, raw_href);
        rev.file_name = CollapseSpaces(StripHtmlTags(am.str(2)));
        static const std::regex kRevIdRe(R"(revision_id=(\d+))", std::regex::icase);
        std::smatch rm;
        if (std::regex_search(raw_href, rm, kRevIdRe)) {
            rev.revision_id = rm.str(1);
        }
    }
    const std::string plain_folded = FoldSpanishAccentsAscii(plain);
    {
        static const std::regex kDayRe(R"(Dia de entrega\s*:?\s*([^\n]+))", std::regex::icase);
        std::smatch m;
        if (std::regex_search(plain_folded, m, kDayRe)) {
            rev.delivered_date = CollapseSpaces(m.str(1));
        } else {
            static const std::regex kDateRe(R"((\d{1,2}/\d{1,2}/\d{4}|\d{4}-\d{2}-\d{2}))");
            if (std::regex_search(plain_folded, m, kDateRe)) rev.delivered_date = m.str(1);
        }
    }
    {
        static const std::regex kHrRe(R"(Hora de entrega\s*:?\s*([^\n]+))", std::regex::icase);
        std::smatch m;
        if (std::regex_search(plain_folded, m, kHrRe)) {
            rev.delivered_time = CollapseSpaces(m.str(1));
        } else {
            static const std::regex kTimeRe(R"((\d{1,2}:\d{2}(?:\s*[ap]m)?))", std::regex::icase);
            if (std::regex_search(plain_folded, m, kTimeRe)) rev.delivered_time = m.str(1);
        }
    }
    if (rev.file_name.empty() && rev.download_url.empty() && rev.delivered_date.empty()) {
        return std::nullopt;
    }
    return rev;
}

std::vector<PlagiarismMisDocItem> ParsePlagiarismMisDocsHtml(std::string_view html) {
    std::vector<PlagiarismMisDocItem> out;
    std::string s(html);
    static const std::regex kSpanDocRe(
        R"(<span\b([^>]*\bdocumento\b[^>]*)>([\s\S]*?)</span>)",
        std::regex::ECMAScript | std::regex::icase);
    for (std::sregex_iterator it(s.begin(), s.end(), kSpanDocRe), end; it != end; ++it) {
        std::string attrs = (*it)[1].str();
        PlagiarismMisDocItem d;
        ExtractAttrValueCI(attrs, "item_id", d.item_id);
        ExtractAttrValueCI(attrs, "id", d.doc_id);
        ExtractAttrValueCI(attrs, "fecha", d.date);
        ExtractAttrValueCI(attrs, "hora", d.time);
        ExtractAttrValueCI(attrs, "status", d.status);
        ExtractAttrValueCI(attrs, "visibilidad", d.visibility);
        std::string pct;
        if (ExtractAttrValueCI(attrs, "porcentaje", pct) && !pct.empty()) {
            double v = -1.0;
            if (TryExtractFirstDouble(pct, v)) d.similarity_pct = v;
        }
        ExtractAttrValueCI(attrs, "package_from", d.package_from);
        ExtractAttrValueCI(attrs, "community_id", d.community_id);
        d.file_name = CollapseSpaces(StripHtmlTags((*it)[2].str()));
        if (!d.item_id.empty() || !d.file_name.empty()) {
            out.push_back(std::move(d));
        }
    }
    return out;
}

std::vector<ForumMessagePost> ParseForumThreadMessagesHtml(std::string_view html) {
    std::vector<ForumMessagePost> out;
    std::string s = StripGlobalPortalChrome(html);

    std::vector<size_t> header_positions;
    static const std::regex kHdrRe(
        R"(<div\b[^>]*\bclass\s*=\s*["'][^"']*\bmessage_header\b[^"']*["'][^>]*>)",
        std::regex::ECMAScript | std::regex::icase);
    for (std::sregex_iterator it(s.begin(), s.end(), kHdrRe), end; it != end; ++it) {
        header_positions.push_back(static_cast<size_t>((*it).position()));
    }

    auto extract_block = [&](const std::string& block) {
        ForumMessagePost post;
        static const std::regex kMidRe(R"(message_id=(\d+))", std::regex::icase);
        std::smatch mm;
        if (std::regex_search(block, mm, kMidRe)) post.message_id = mm.str(1);

        static const std::regex kSubjRe(
            R"(<(?:h[1-6]|div|span)[^>]*\bclass\s*=\s*["'][^"']*(?:message_subject|forum_subject|subject)[^"']*["'][^>]*>([\s\S]*?)</(?:h[1-6]|div|span)>)",
            std::regex::icase);
        if (std::regex_search(block, mm, kSubjRe)) {
            post.subject = CollapseSpaces(StripHtmlTags(mm.str(1)));
        }

        static const std::regex kAuthRe(
            R"(<[^>]*\bclass\s*=\s*["'][^"']*\bforum_user_name\b[^"']*["'][^>]*>([\s\S]*?)</[^>]+>)",
            std::regex::icase);
        if (std::regex_search(block, mm, kAuthRe)) {
            post.author = CollapseSpaces(StripHtmlTags(mm.str(1)));
        }

        static const std::regex kDateRe(R"((\d{1,2}/\d{1,2}/\d{4}(?:\s+\d{1,2}:\d{2})?|\d{4}-\d{2}-\d{2}(?:\s+\d{2}:\d{2})?))");
        if (std::regex_search(block, mm, kDateRe)) {
            post.posted_date = CollapseSpaces(mm.str(1));
        }

        static const std::regex kBodyRe(
            R"(<div\b[^>]*\bclass\s*=\s*["'][^"']*(?:content\s+forum_message|forum_message|richtext_forums)[^"']*["'][^>]*>([\s\S]*?)</div>)",
            std::regex::icase);
        if (std::regex_search(block, mm, kBodyRe)) {
            post.body_text = TrimAscii(StripHtmlTags(mm.str(1)));
        }

        if (!post.body_text.empty() || !post.author.empty() || !post.subject.empty()) {
            out.push_back(std::move(post));
        }
    };

    if (!header_positions.empty()) {
        for (size_t i = 0; i < header_positions.size(); ++i) {
            size_t start = header_positions[i];
            size_t stop = (i + 1 < header_positions.size()) ? header_positions[i + 1] : s.size();
            extract_block(s.substr(start, stop - start));
        }
    } else {
        extract_block(s);
    }
    return out;
}

std::vector<PerformanceEvalPending> ParsePerformanceEvaluationsJson(std::string_view json_text) {
    std::vector<PerformanceEvalPending> out;
    auto j = nlohmann::json::parse(json_text, nullptr, false);
    if (j.is_discarded()) return out;

    const nlohmann::json* arr = nullptr;
    if (j.is_array()) arr = &j;
    else if (j.is_object()) {
        for (const char* k : {"evaluations", "data", "results"}) {
            if (j.contains(k) && j[k].is_array()) {
                arr = &j[k];
                break;
            }
        }
    }
    if (arr == nullptr) return out;

    auto first_str = [](const nlohmann::json& obj, std::initializer_list<const char*> keys) -> std::string {
        for (const char* k : keys) {
            if (!obj.contains(k)) continue;
            const auto& v = obj[k];
            if (v.is_string()) return v.get<std::string>();
            if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
            if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
        }
        return {};
    };

    for (const auto& item : *arr) {
        if (!item.is_object()) continue;
        PerformanceEvalPending ev;
        ev.evaluation_id = first_str(item, {"id", "evaluation_id", "eval_id"});
        ev.course_name = first_str(item, {"course_name", "course", "materia", "community_name"});
        ev.teacher_name = first_str(item, {"teacher_name", "professor", "profesor", "teacher"});
        ev.form_name = first_str(item, {"form_name", "form", "title", "name"});
        ev.status = first_str(item, {"status", "estado", "state"});
        ev.deadline = first_str(item, {"deadline", "end_date", "fecha_fin", "due_date"});
        ev.url = first_str(item, {"url", "link"});
        if (!ev.evaluation_id.empty() || !ev.course_name.empty() || !ev.teacher_name.empty() || !ev.form_name.empty()) {
            out.push_back(std::move(ev));
        }
    }
    return out;
}

std::vector<CourseExternalLink> ExtractCustomTabLinksHtml(std::string_view html,
                                                          std::string_view tab_label,
                                                          std::string_view base_url) {
    std::vector<CourseExternalLink> out;
    const std::string s = StripGlobalPortalChrome(html);
    static const std::regex kLinkRe(
        R"(<a[^>]*href\s*=\s*["']([^"']+)["'][^>]*>([\s\S]*?)</a>)",
        std::regex::ECMAScript | std::regex::icase);
    std::set<std::string> seen;
    for (std::sregex_iterator it(s.begin(), s.end(), kLinkRe), end; it != end; ++it) {
        std::string raw_href = HeadlessCrawler::HtmlUnescape((*it)[1].str());
        if (raw_href.empty() || raw_href[0] == '#' ||
            raw_href.rfind("javascript:", 0) == 0 || raw_href.rfind("mailto:", 0) == 0) {
            continue;
        }
        std::string full_url = ResolveRelativeUrl(base_url, raw_href);
        std::string low_u = AsciiLower(full_url);
        if (low_u.find("page_num=") != std::string::npos ||
            low_u.find("/register/") != std::string::npos) {
            continue;
        }
        const bool is_doc_or_ext =
            low_u.find("/file-storage/view/") != std::string::npos ||
            low_u.find("/file-storage/download") != std::string::npos ||
            low_u.find("/evaluation/view/") != std::string::npos ||
            low_u.find(".pdf") != std::string::npos ||
            low_u.find("sharepoint.com") != std::string::npos ||
            low_u.find("1drv.ms") != std::string::npos ||
            low_u.find("youtube.com") != std::string::npos ||
            low_u.find("youtu.be") != std::string::npos ||
            low_u.find("teams.microsoft.com") != std::string::npos ||
            low_u.find("zoom.us") != std::string::npos;
        if (!is_doc_or_ext) continue;
        if (!seen.insert(full_url).second) continue;

        std::string lbl = CollapseSpaces(StripHtmlTags((*it)[2].str()));
        std::string cat = ClassifyExternalLink(lbl, full_url);
        if (cat == "Enlace" && !tab_label.empty()) {
            cat = std::string(tab_label);
        }
        CourseExternalLink lnk;
        lnk.label = lbl.empty() ? (tab_label.empty() ? full_url : std::string(tab_label)) : lbl;
        if (!tab_label.empty() && AsciiLower(lnk.label).find(AsciiLower(tab_label)) == std::string::npos) {
            lnk.label = std::string(tab_label) + " — " + lnk.label;
        }
        lnk.url = std::move(full_url);
        lnk.category = std::move(cat);
        out.push_back(std::move(lnk));
    }
    return out;
}

}  // namespace academic::tec_digital
