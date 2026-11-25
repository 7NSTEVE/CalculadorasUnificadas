#include "academic/tec_digital/tda_tds_client.hpp"
#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/vault.hpp"
#include "http_common.hpp"
#include "session_http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <regex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace academic::tec_digital {

namespace {

std::string JoinUrl(std::string_view base_url, std::string_view path) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    std::string p(path);
    if (p.empty() || p.front() != '/') p.insert(p.begin(), '/');
    return b + p;
}

std::string UrlEncode(std::string_view sv) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(sv.size());
    for (unsigned char c : sv) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string NormalizeCommPath(std::string_view comm) {
    std::string s(comm);
    ParsedUrl p = ParseHttpUrl(s);
    if (p.valid) s = p.path;
    while (!s.empty() && s.back() == '/') s.pop_back();
    if (s.empty()) return "/dotlrn";
    if (s.front() != '/') s.insert(s.begin(), '/');
    return s;
}

std::string JsonAsString(const nlohmann::json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return {};
    const auto& v = j[key];
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
    if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
    if (v.is_number_float()) return std::to_string(v.get<double>());
    if (v.is_boolean()) return v.get<bool>() ? "true" : "false";
    return {};
}

std::string FirstStr(const nlohmann::json& j, std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        std::string v = JsonAsString(j, k);
        if (!v.empty()) return v;
    }
    return {};
}

int64_t JsonAsInt64(const nlohmann::json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return 0;
    const auto& v = j[key];
    if (v.is_number_integer()) return v.get<int64_t>();
    if (v.is_number_unsigned()) return static_cast<int64_t>(v.get<uint64_t>());
    if (v.is_number_float()) return static_cast<int64_t>(v.get<double>());
    if (v.is_string()) {
        std::string s = TrimAscii(v.get<std::string>());
        if (!s.empty()) return std::strtoll(s.c_str(), nullptr, 10);
    }
    return 0;
}

double JsonAsDouble(const nlohmann::json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return 0.0;
    const auto& v = j[key];
    if (v.is_number()) return v.get<double>();
    if (v.is_string()) {
        std::string s = TrimAscii(v.get<std::string>());
        if (!s.empty()) return std::strtod(s.c_str(), nullptr);
    }
    return 0.0;
}

std::vector<std::string> ParseIdOrStringArray(const nlohmann::json& j, const char* key) {
    std::vector<std::string> out;
    if (!j.is_object() || !j.contains(key) || !j[key].is_array()) return out;
    for (const auto& elem : j[key]) {
        if (elem.is_string()) {
            out.push_back(elem.get<std::string>());
        } else if (elem.is_number_integer()) {
            out.push_back(std::to_string(elem.get<int64_t>()));
        } else if (elem.is_object()) {
            std::string id = FirstStr(elem, {"id", "id_course", "codigo", "key"});
            if (!id.empty()) out.push_back(std::move(id));
        }
    }
    return out;
}

std::map<std::string, std::string> ParseFlatStringMap(std::string_view json_text) {
    std::map<std::string, std::string> out;
    auto j = nlohmann::json::parse(json_text);
    if (!j.is_object()) return out;
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_string()) {
            out[it.key()] = it.value().get<std::string>();
        } else if (it.value().is_number_integer()) {
            out[it.key()] = std::to_string(it.value().get<int64_t>());
        } else if (it.value().is_number_unsigned()) {
            out[it.key()] = std::to_string(it.value().get<uint64_t>());
        } else if (it.value().is_boolean()) {
            out[it.key()] = it.value().get<bool>() ? "true" : "false";
        }
    }
    return out;
}

std::string StripTags(std::string_view html) {
    std::string out;
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') in_tag = true;
        else if (c == '>') in_tag = false;
        else if (!in_tag) out.push_back(c);
    }
    return TrimAscii(HeadlessCrawler::HtmlUnescape(out));
}

std::string UnwrapCdataAndUnescape(std::string_view raw) {
    std::string s = TrimAscii(raw);
    if (s.rfind("<![CDATA[", 0) == 0) {
        size_t end_cd = s.rfind("]]>");
        if (end_cd != std::string::npos && end_cd >= 9) {
            return TrimAscii(s.substr(9, end_cd - 9));
        }
    }
    return TrimAscii(HeadlessCrawler::HtmlUnescape(s));
}

std::string ExtractXmlTag(std::string_view block, std::string_view tag_name) {
    const std::string tag_low = AsciiLower(tag_name);
    const std::string low = AsciiLower(block);
    const size_t n = block.size();

    auto is_ws = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };

    size_t pos = 0;
    size_t content_start = std::string::npos;
    while (pos < n) {
        if (low.compare(pos, 9, "<![cdata[") == 0) {
            size_t end_cd = low.find("]]>", pos + 9);
            if (end_cd == std::string::npos) return {};
            pos = end_cd + 3;
            continue;
        }
        if (low.compare(pos, 4, "<!--") == 0) {
            size_t end_cm = low.find("-->", pos + 4);
            if (end_cm == std::string::npos) return {};
            pos = end_cm + 3;
            continue;
        }
        if (block[pos] == '<' && pos + 1 + tag_low.size() < n &&
            low.compare(pos + 1, tag_low.size(), tag_low) == 0) {
            char next = block[pos + 1 + tag_low.size()];
            if (next == '>' || next == '/' || is_ws(next)) {
                size_t cur = pos + 1 + tag_low.size();
                char quote = 0;
                while (cur < n) {
                    char ch = block[cur];
                    if (quote != 0) {
                        if (ch == quote) quote = 0;
                    } else if (ch == '"' || ch == '\'') {
                        quote = ch;
                    } else if (ch == '>') {
                        break;
                    }
                    ++cur;
                }
                if (cur >= n) return {};
                if (cur > pos + 1 && block[cur - 1] == '/') {
                    return {};
                }
                content_start = cur + 1;
                break;
            }
        }
        ++pos;
    }
    if (content_start == std::string::npos) return {};

    size_t scan = content_start;
    const std::string close_prefix = "</" + tag_low;
    while (scan < n) {
        if (low.compare(scan, 9, "<![cdata[") == 0) {
            size_t end_cd = low.find("]]>", scan + 9);
            if (end_cd == std::string::npos) return {};
            scan = end_cd + 3;
            continue;
        }
        if (low.compare(scan, 4, "<!--") == 0) {
            size_t end_cm = low.find("-->", scan + 4);
            if (end_cm == std::string::npos) return {};
            scan = end_cm + 3;
            continue;
        }
        if (scan + close_prefix.size() < n &&
            low.compare(scan, close_prefix.size(), close_prefix) == 0) {
            size_t after = scan + close_prefix.size();
            while (after < n && is_ws(block[after])) ++after;
            if (after < n && block[after] == '>') {
                return UnwrapCdataAndUnescape(
                    block.substr(content_start, scan - content_start));
            }
        }
        ++scan;
    }
    return {};
}

std::string ExtractFirstElementInnerByRegex(std::string_view html, const std::regex& open_tag_re) {
    std::string s(html);
    std::smatch m;
    if (!std::regex_search(s, m, open_tag_re) || m.size() < 2) return {};
    std::string tag_name = AsciiLower(m[1].str());
    size_t content_start = static_cast<size_t>(m.position() + m.length());
    int depth = 1;
    size_t i = content_start;
    const size_t n = html.size();
    const std::string low = AsciiLower(html);
    while (i < n) {
        size_t lt = low.find('<', i);
        if (lt == std::string::npos) break;
        if (low.compare(lt, 4, "<!--") == 0) {
            size_t end_cm = low.find("-->", lt + 4);
            if (end_cm == std::string::npos) break;
            i = end_cm + 3;
            continue;
        }
        bool is_close = (lt + 1 < n && low[lt + 1] == '/');
        size_t name_start = lt + (is_close ? 2 : 1);
        if (name_start + tag_name.size() <= n &&
            low.compare(name_start, tag_name.size(), tag_name) == 0) {
            size_t after = name_start + tag_name.size();
            if (after < n && (low[after] == '>' || low[after] == '/' ||
                              std::isspace(static_cast<unsigned char>(low[after])))) {
                size_t gt = low.find('>', after);
                if (gt == std::string::npos) break;
                bool self_closing = (!is_close && gt > lt && low[gt - 1] == '/');
                if (is_close) {
                    --depth;
                    if (depth == 0) {
                        return std::string(html.substr(content_start, lt - content_start));
                    }
                } else if (!self_closing) {
                    ++depth;
                }
                i = gt + 1;
                continue;
            }
        }
        i = lt + 1;
    }
    return {};
}

std::vector<std::string> SplitDelim(std::string_view sv, char delim) {
    std::vector<std::string> out;
    size_t pos = 0;
    while (pos <= sv.size()) {
        size_t d = sv.find(delim, pos);
        if (d == std::string_view::npos) d = sv.size();
        out.emplace_back(sv.substr(pos, d - pos));
        if (d == sv.size()) break;
        pos = d + 1;
    }
    return out;
}

void SanitizeUrlOrText(std::string& s, const Sanitizer& san) {
    if (s.empty()) return;
    if (s.find("://") != std::string::npos) {
        s = san.SanitizeUrl(s);
    } else {
        s = san.SanitizeText(s);
    }
}

void SanitizeSedes(std::vector<SedeInfo>& list, const Sanitizer& san) {
    for (auto& s : list) {
        s.key = san.SanitizeText(s.key);
        s.name = san.SanitizeText(s.name);
    }
}

void SanitizeCarreras(std::vector<CarreraInfo>& list, const Sanitizer& san) {
    for (auto& c : list) {
        c.key = san.SanitizeText(c.key);
        c.name = san.SanitizeText(c.name);
    }
}

void SanitizePlanes(std::vector<PlanInfo>& list, const Sanitizer& san) {
    for (auto& p : list) {
        p.key = san.SanitizeText(p.key);
        p.name = san.SanitizeText(p.name);
    }
}

void SanitizeColorCodes(ColorCodeCatalog& cc_cat, const Sanitizer& san) {
    cc_cat.title = san.SanitizeText(cc_cat.title);
    cc_cat.additional_text = san.SanitizeText(cc_cat.additional_text);
    for (auto& cc : cc_cat.color_codes) {
        cc.label = san.SanitizeText(cc.label);
        cc.css_class = san.SanitizeText(cc.css_class);
    }
}

void SanitizeCurriculumGraph(CurriculumGraph& cg, const Sanitizer& san) {
    cg.dsc_curriculum = san.SanitizeText(cg.dsc_curriculum);
    cg.modality = san.SanitizeText(cg.modality);
    cg.academic_degree = san.SanitizeText(cg.academic_degree);
    for (auto& lv : cg.levels) {
        lv.id = san.SanitizeText(lv.id);
        for (auto& co : lv.courses) {
            co.id_course = san.SanitizeText(co.id_course);
            co.name = san.SanitizeText(co.name);
            co.truncated_name = san.SanitizeText(co.truncated_name);
            co.t_equiv = san.SanitizeText(co.t_equiv);
            for (auto& r : co.requirements) r = san.SanitizeText(r);
            for (auto& r : co.co_requirements) r = san.SanitizeText(r);
            for (auto& r : co.required_by) r = san.SanitizeText(r);
            for (auto& r : co.equivalent) r = san.SanitizeText(r);
        }
    }
}

void SanitizeStudentProfile(StudentCurriculumProfile& prof, const Sanitizer& san) {
    for (auto& g : prof.grades) {
        g.id_course = san.SanitizeText(g.id_course);
        g.best_status = san.SanitizeText(g.best_status);
        for (auto& att : g.grades_record) {
            att.modality = san.SanitizeText(att.modality);
            att.name_equivalent = san.SanitizeText(att.name_equivalent);
            att.id_equivalent = san.SanitizeText(att.id_equivalent);
            for (auto& sch : att.schedule) {
                sch = san.SanitizeText(sch);
            }
        }
    }
}

void SanitizeStudentCareer(StudentCareerRecord& rec, const Sanitizer& san) {
    rec.nombre = san.SanitizeText(rec.nombre);
    rec.sede = san.SanitizeText(rec.sede);
    rec.carrera = san.SanitizeText(rec.carrera);
}

void SanitizeExpediente(ExpedienteSummary& ex, const Sanitizer& san) {
    for (auto& kv : ex.i18n_labels) kv.second = san.SanitizeText(kv.second);
    for (auto& kv : ex.validation_labels) kv.second = san.SanitizeText(kv.second);
    SanitizeSedes(ex.sedes_combo, san);
    SanitizePlanes(ex.periodos_combo, san);
    ex.detalle_notas_html = san.SanitizeHtml(ex.detalle_notas_html);
    ex.detalle_promedio_html = san.SanitizeHtml(ex.detalle_promedio_html);
    ex.tabla_promedio_html = san.SanitizeHtml(ex.tabla_promedio_html);
    ex.detalle_historial_html = san.SanitizeHtml(ex.detalle_historial_html);
    ex.citas_matricula_html = san.SanitizeHtml(ex.citas_matricula_html);
    ex.guia_horario_html = san.SanitizeHtml(ex.guia_horario_html);
    ex.weighted_average_text = san.SanitizeText(ex.weighted_average_text);
    ex.approval_pct_text = san.SanitizeText(ex.approval_pct_text);
    for (auto& ga : ex.structured.graduate_attributes) {
        ga.attribute_name = san.SanitizeText(ga.attribute_name);
        ga.description = san.SanitizeText(ga.description);
        ga.courses = san.SanitizeText(ga.courses);
    }
}

void SanitizePrematricula(PrematriculaSnapshot& pm, const Sanitizer& san) {
    pm.rn_student = san.SanitizeText(pm.rn_student);
    for (auto& c : pm.courses) {
        c.course_code = san.SanitizeText(c.course_code);
        c.course_elective = san.SanitizeText(c.course_elective);
        c.name_elective = san.SanitizeText(c.name_elective);
    }
    for (auto& req : pm.requests) {
        req.course_code = san.SanitizeText(req.course_code);
        req.observation = san.SanitizeText(req.observation);
    }
    for (auto& dc : pm.disabled_courses) {
        dc = san.SanitizeText(dc);
    }
    for (auto& kv : pm.i18n_labels) kv.second = san.SanitizeText(kv.second);
}

void SanitizeCourseEvaluations(CourseEvaluationsSnapshot& ev, const Sanitizer& san) {
    ev.community_slug = san.SanitizeText(ev.community_slug);
    ev.final_grade_text = san.SanitizeText(ev.final_grade_text);
    for (auto& c : ev.categories) {
        c.category_id = san.SanitizeText(c.category_id);
        c.name = san.SanitizeText(c.name);
        c.weight_text = san.SanitizeText(c.weight_text);
        for (auto& item : c.items) {
            item.assign_id = san.SanitizeText(item.assign_id);
            item.name = san.SanitizeText(item.name);
            item.grade_text = san.SanitizeText(item.grade_text);
        }
    }
    for (auto& pf : ev.performance_forms) {
        pf.name = san.SanitizeText(pf.name);
        pf.instructions = san.SanitizeText(pf.instructions);
        for (auto& sec : pf.sections) {
            sec.name = san.SanitizeText(sec.name);
            sec.description = san.SanitizeText(sec.description);
            for (auto& it : sec.items) {
                it.description = san.SanitizeText(it.description);
                it.type = san.SanitizeText(it.type);
            }
            sec.type = san.SanitizeText(sec.type);
        }
    }
    ev.performance_evaluations_raw_json = san.SanitizeJsonString(ev.performance_evaluations_raw_json);
    for (auto& kv : ev.i18n_labels) kv.second = san.SanitizeText(kv.second);
}

void SanitizeFeriaLocation(FeriaLocation& loc, const Sanitizer& san) {
    loc.id = san.SanitizeText(loc.id);
    loc.name = san.SanitizeText(loc.name);
}

void SanitizeFeriaCategories(std::vector<FeriaCategory>& cats, const Sanitizer& san) {
    for (auto& c : cats) {
        c.name = san.SanitizeText(c.name);
    }
}

void SanitizeFeriaSnapshot(FeriaNegociosSnapshot& fn, const Sanitizer& san) {
    for (auto& kv : fn.i18n_labels) kv.second = san.SanitizeText(kv.second);
    fn.info_estudiante_raw_json = san.SanitizeJsonString(fn.info_estudiante_raw_json);
    SanitizeFeriaLocation(fn.student_location, san);
    fn.active_fairs_raw_json = san.SanitizeJsonString(fn.active_fairs_raw_json);
    SanitizeFeriaCategories(fn.categories, san);
}

void SanitizeNotifications(NotificationsFeed& nf, const Sanitizer& san) {
    for (auto& n : nf.notifications) {
        n.notification_id = san.SanitizeText(n.notification_id);
        n.creation_date = san.SanitizeText(n.creation_date);
        n.expiration_date = san.SanitizeText(n.expiration_date);
        n.title = san.SanitizeText(n.title);
        n.text = san.SanitizeText(n.text);
        n.url = san.SanitizeUrl(n.url);
        n.state = san.SanitizeText(n.state);
    }
}

void SanitizeRssItems(std::vector<RssNewsItem>& items, const Sanitizer& san) {
    for (auto& item : items) {
        item.title = san.SanitizeText(item.title);
        item.link = san.SanitizeUrl(item.link);
        SanitizeUrlOrText(item.guid, san);
        item.description = san.SanitizeText(item.description);
        item.pub_date = san.SanitizeText(item.pub_date);
    }
}

void SanitizeMembers(std::vector<CommunityMember>& members, const Sanitizer& san) {
    for (auto& m : members) {
        m.user_id = san.SanitizeText(m.user_id);
        m.first_name = san.SanitizeText(m.first_name);
        m.last_name = san.SanitizeText(m.last_name);
        m.full_name = san.SanitizeText(m.full_name);
        m.email = san.SanitizeText(m.email);
        m.role = san.SanitizeText(m.role);
    }
}

}  // namespace

TdaTdsClient::TdaTdsClient(PolicyEnforcingTransport& transport,
                           AuthClient& auth,
                           std::string base_url,
                           SanitizeOptions sanitize_opts)
    : transport_(transport),
      auth_(auth),
      base_url_(std::move(base_url)),
      sanitizer_(std::move(sanitize_opts)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
    detail::WarmupRegexLocale();
}

void TdaTdsClient::SetSanitizeOptions(SanitizeOptions sanitize_opts) {
    Sanitizer next(std::move(sanitize_opts));
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    sanitizer_ = std::move(next);
}

Sanitizer TdaTdsClient::SnapshotSanitizer() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return sanitizer_;
}

void TdaTdsClient::ClearCachedCarne() {
    std::lock_guard<std::mutex> lk(carne_mutex_);
    std::fill(cached_carne_.begin(), cached_carne_.end(), '\0');
    cached_carne_.clear();
}

std::string TdaTdsClient::ResolveStudentCarne(std::string_view explicit_carne) {
    std::string trimmed = TrimAscii(explicit_carne);
    if (!trimmed.empty()) {
        std::lock_guard<std::mutex> lk(carne_mutex_);
        cached_carne_ = trimmed;
        return trimmed;
    }
    {
        std::lock_guard<std::mutex> lk(carne_mutex_);
        if (!cached_carne_.empty()) return cached_carne_;
    }

    std::string discovered;
    auto probe_carne_from_text = [](std::string_view text) -> std::string {
        std::string s(text);
        static const std::regex kAttrRe(
            R"((?:name|id)\s*=\s*["'](?:carne|carnet|student_id|carneOficial)["'][^>]*value\s*=\s*["'](\d{6,12})["'])",
            std::regex::ECMAScript | std::regex::icase);
        std::smatch m;
        if (std::regex_search(s, m, kAttrRe)) return m.str(1);
        static const std::regex kJsRe(
            R"((?:carne|carnet|student_id|carneOficial)["']?\s*[:=]\s*["']?(\d{6,12})["']?)",
            std::regex::ECMAScript | std::regex::icase);
        if (std::regex_search(s, m, kJsRe)) return m.str(1);
        return {};
    };

    if (auto r = FetchText("GET", "/tda-expediente-estudiantil/", "", "", "text/html, */*"); r.ok) {
        discovered = probe_carne_from_text(r.value);
    }
    if (discovered.empty()) {
        if (auto r = FetchText("GET", "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante?carneOficial="); r.ok) {
            discovered = probe_carne_from_text(r.value);
        }
    }
    if (discovered.empty()) {
        if (auto r = FetchText("POST", "/tda-expediente-estudiantil/ajax/Consultar_datos_estudiante",
                               "", "application/x-www-form-urlencoded", "text/plain, text/html, */*"); r.ok) {
            discovered = probe_carne_from_text(r.value);
        }
    }
    if (!discovered.empty()) {
        std::lock_guard<std::mutex> lk(carne_mutex_);
        cached_carne_ = discovered;
    }
    return discovered;
}

Result<std::string> TdaTdsClient::FetchText(std::string_view method,
                                            std::string_view path_or_url,
                                            std::string_view body,
                                            std::string_view content_type,
                                            std::string_view accept) {
    std::string url = (path_or_url.rfind("http://", 0) == 0 ||
                       path_or_url.rfind("https://", 0) == 0)
                          ? std::string(path_or_url)
                          : JoinUrl(base_url_, path_or_url);

    HttpRequest req;
    req.method = std::string(method);
    req.url = url;
    req.follow_redirects = true;
    req.max_redirects = 5;
    req.body = std::string(body);
    if (!content_type.empty()) {
        req.headers["Content-Type"] = std::string(content_type);
    }
    if (!accept.empty()) {
        req.headers["Accept"] = std::string(accept);
    }
    req.headers["Referer"] = JoinUrl(base_url_, "/dotlrn/");

    HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
    auto val = auth_.ValidateResponse(resp, url, true);
    if (val == ResponseValidity::SessionExpired) {
        return Result<std::string>::Failure("session_expired",
                                            "session expired on " + SnapshotSanitizer().SanitizeUrl(url));
    }
    if (!resp.transport_ok) {
        return Result<std::string>::Failure(
            resp.error_message.rfind("read-only-request-guard:", 0) == 0 ? "policy_blocked" : "transport_error",
            SnapshotSanitizer().SanitizeText(resp.error_message));
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        return Result<std::string>::Failure(
            "http_" + std::to_string(resp.status_code),
            "HTTP " + std::to_string(resp.status_code) + " on " + SnapshotSanitizer().SanitizeUrl(url));
    }
    return Result<std::string>::Success(resp.BodyAsString());
}

Result<int64_t> TdaTdsClient::GetServerDateMs() {
    auto r = FetchText("GET", "/theme-td/ajax/get_date_server");
    if (!r.ok) return Result<int64_t>::Failure(r.error_code, r.error_message);
    std::string s = TrimAscii(r.value);
    if (s.empty()) {
        return Result<int64_t>::Failure("invalid_response", "empty server date response");
    }
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return Result<int64_t>::Failure("invalid_response", "non-numeric server date");
        }
    }
    return Result<int64_t>::Success(std::strtoll(s.c_str(), nullptr, 10));
}

Result<std::string> TdaTdsClient::GetGaapServer() {
    auto r = FetchText("POST", "/tda-um/ajax/get_gaap_server", "",
                       "application/x-www-form-urlencoded");
    if (!r.ok) return r;
    const Sanitizer san = SnapshotSanitizer();
    std::string s = TrimAscii(r.value);
    if (!s.empty() && s.front() == '{') {
        try {
            auto j = nlohmann::json::parse(s);
            std::string extracted = FirstStr(j, {"gaap_server", "url", "server"});
            if (!extracted.empty()) {
                return Result<std::string>::Success(
                    extracted.find("://") != std::string::npos
                        ? san.SanitizeUrl(extracted)
                        : san.SanitizeText(extracted));
            }
        } catch (...) {
        }
    }
    return Result<std::string>::Success(
        s.find("://") != std::string::npos ? san.SanitizeUrl(s) : san.SanitizeText(s));
}

Result<std::map<std::string, std::string>> TdaTdsClient::GetUmInternationalization() {
    auto r = FetchText("POST", "/tda-um/ajax/internacionalization",
                       "nombre_archivo_js=index", "application/x-www-form-urlencoded");
    if (!r.ok) return Result<std::map<std::string, std::string>>::Failure(r.error_code, r.error_message);
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto m = ParseFlatStringMap(r.value);
        for (auto& kv : m) kv.second = san.SanitizeText(kv.second);
        return Result<std::map<std::string, std::string>>::Success(std::move(m));
    } catch (const std::exception& ex) {
        return Result<std::map<std::string, std::string>>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

Result<std::string> TdaTdsClient::GetUserTermConditionJson(std::string_view module_key) {
    std::string body = module_key.empty() ? "" : ("condition_project_name=" + UrlEncode(module_key));
    auto r = FetchText("POST", "/tda-consentimiento-informado/ajax/client/get_user_term_condition",
                       body, "application/x-www-form-urlencoded");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeJsonString(r.value);
    return r;
}

Result<std::string> TdaTdsClient::GetConsentStatus(std::string_view module_key) {
    return GetUserTermConditionJson(module_key);
}

Result<std::map<std::string, std::string>> TdaTdsClient::GetSmsInternationalization(std::string_view js) {
    auto r = FetchText("GET", "/tda-sms-configuration/ajax/internationalization?js=" + UrlEncode(js));
    if (!r.ok) return Result<std::map<std::string, std::string>>::Failure(r.error_code, r.error_message);
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto m = ParseFlatStringMap(r.value);
        for (auto& kv : m) kv.second = san.SanitizeText(kv.second);
        return Result<std::map<std::string, std::string>>::Success(std::move(m));
    } catch (const std::exception& ex) {
        return Result<std::map<std::string, std::string>>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

Result<std::string> TdaTdsClient::GetTamGaapJson() {
    auto r = FetchText("GET", "/servicios/gaap_oacs/PL/resources/js/json/tam_gaap.json");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeJsonString(r.value);
    return r;
}

Result<std::string> TdaTdsClient::GetNotificationRequestId() {
    auto r = FetchText("POST", "/notifications/ajax/request_id", "",
                       "application/x-www-form-urlencoded");
    if (!r.ok) return r;
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto j = nlohmann::json::parse(r.value);
        std::string rid = FirstStr(j, {"requestID", "request_id", "id"});
        if (!rid.empty()) return Result<std::string>::Success(san.SanitizeText(rid));
    } catch (...) {
    }
    return Result<std::string>::Success(san.SanitizeText(TrimAscii(r.value)));
}

Result<std::string> TdaTdsClient::GetFileStorageFolderChunkHtml(std::string_view folder_id) {
    std::string path = "/dotlrn/file-storage/view/folder-chunk";
    if (!folder_id.empty()) {
        path += "?folder_id=" + UrlEncode(folder_id);
    }
    auto r = FetchText("GET", path, "", "", "text/html, */*");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeHtml(r.value);
    return r;
}

Result<std::map<std::string, std::string>> TdaTdsClient::GetFileStorageInternationalization(
    std::string_view community_path) {
    std::string path = community_path.empty()
                           ? "/dotlrn/file-storage/ajax/internationalization/internationalization?js=file-storage"
                           : (NormalizeCommPath(community_path) +
                              "/file-storage/ajax/internationalization/internationalization?js=file-storage");
    auto r = FetchText("GET", path);
    if (!r.ok) return Result<std::map<std::string, std::string>>::Failure(r.error_code, r.error_message);
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto m = ParseFlatStringMap(r.value);
        for (auto& kv : m) kv.second = san.SanitizeText(kv.second);
        return Result<std::map<std::string, std::string>>::Success(std::move(m));
    } catch (const std::exception& ex) {
        return Result<std::map<std::string, std::string>>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

std::vector<CommunityInfo> TdaTdsClient::ParseCommunitiesHtml(std::string_view html_text,
                                                              std::string_view base_url) {
    detail::WarmupRegexLocale();
    std::vector<CommunityInfo> out;
    std::unordered_set<std::string> seen_urls;

    static const std::regex kAnchorRe(
        R"re(<a\b[^>]*\bhref\s*=\s*(?:"([^"]*)"|'([^']*)')[^>]*>([\s\S]*?)</a>)re",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kClassPathRe(
        R"(^/dotlrn/classes/([^/]+)/([^/]+)/([^/?#]+))",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kCatedraPathRe(
        R"(^/dotlrn/catedras\.tec/([^/?#]+))",
        std::regex::ECMAScript | std::regex::icase);

    std::string html(html_text);
    std::sregex_iterator it(html.begin(), html.end(), kAnchorRe);
    std::sregex_iterator end;
    for (; it != end; ++it) {
        std::string href = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();
        href = TrimAscii(HeadlessCrawler::HtmlUnescape(href));
        if (href.empty()) continue;

        auto canon = UrlCanonicalizer::Canonicalize(href, base_url);
        if (!canon.ok) continue;
        ParsedUrl pu = ParseHttpUrl(canon.value);
        if (!pu.valid) continue;

        std::smatch m;
        if (std::regex_search(pu.path, m, kClassPathRe)) {
            std::string slug = m.str(3);
            std::string comm_url = pu.origin + "/dotlrn/classes/" + m.str(1) + "/" + m.str(2) + "/" + slug;
            if (!seen_urls.insert(comm_url).second) continue;

            CommunityInfo info;
            info.department = m.str(1);
            info.slug = slug;
            info.community_id = slug;
            info.type = "CLASE";
            info.url = std::move(comm_url);
            size_t dot = slug.find('.');
            info.period = (dot != std::string::npos) ? slug.substr(0, dot) : "";
            info.name = StripTags((*it)[3].str());
            if (info.name.empty()) info.name = m.str(2) + " (" + slug + ")";
            out.push_back(std::move(info));
        } else if (std::regex_search(pu.path, m, kCatedraPathRe)) {
            std::string slug = m.str(1);
            if (slug == "one-community-type" || slug == "one-community") continue;
            std::string comm_url = pu.origin + "/dotlrn/catedras.tec/" + slug;
            if (!seen_urls.insert(comm_url).second) continue;

            CommunityInfo info;
            info.department = "Catedra";
            info.slug = slug;
            info.community_id = slug;
            info.type = "CATEDRA";
            info.period = "Permanente";
            info.url = std::move(comm_url);
            info.name = StripTags((*it)[3].str());
            if (info.name.empty()) info.name = slug;
            out.push_back(std::move(info));
        }
    }
    return out;
}

Result<std::vector<CommunityInfo>> TdaTdsClient::FetchCoursesAndCommunities(
    bool include_catalog_pages,
    int max_catalog_pages) {
    // Strictly read-only HTML discovery (NEVER calls /dotlrn/ajax/actualizar_cursos)
    std::vector<std::string> urls = {
        "/dotlrn/index",
        "/dotlrn/my-communities",
        "/dotlrn/classes/",
        "/dotlrn/catedras.tec/"
    };
    if (include_catalog_pages) {
        for (int p = 0; p < max_catalog_pages; ++p) {
            urls.push_back("/dotlrn/classes/one-community-type?page_num=" + std::to_string(p));
            urls.push_back("/dotlrn/catedras.tec/one-community-type?page_num=" + std::to_string(p));
        }
    }

    std::vector<CommunityInfo> all;
    std::unordered_set<std::string> seen;

    for (const auto& u : urls) {
        auto r = FetchText("GET", u, "", "", "text/html,application/xhtml+xml,*/*");
        if (!r.ok) {
            if (r.error_code == "session_expired") {
                return Result<std::vector<CommunityInfo>>::Failure(r.error_code, r.error_message);
            }
            continue;
        }
        auto parsed = ParseCommunitiesHtml(r.value, base_url_ + "/");
        if (include_catalog_pages && u.find("one-community-type") != std::string::npos &&
            parsed.empty()) {
            continue;
        }
        for (auto& c : parsed) {
            if (seen.insert(c.url).second) {
                all.push_back(std::move(c));
            }
        }
    }
    const Sanitizer san = SnapshotSanitizer();
    for (auto& c : all) {
        c.community_id = san.SanitizeText(c.community_id);
        c.slug = san.SanitizeText(c.slug);
        c.name = san.SanitizeText(c.name);
        c.department = san.SanitizeText(c.department);
        c.period = san.SanitizeText(c.period);
        c.url = san.SanitizeUrl(c.url);
        c.root_folder_id = san.SanitizeText(c.root_folder_id);
    }
    return Result<std::vector<CommunityInfo>>::Success(std::move(all));
}

Result<std::vector<SedeInfo>> TdaTdsClient::ParseSedesJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        const nlohmann::json* arr = nullptr;
        if (j.is_array()) arr = &j;
        else if (j.is_object() && j.contains("sedes") && j["sedes"].is_array()) arr = &j["sedes"];
        else return Result<std::vector<SedeInfo>>::Failure("invalid_schema", "missing sedes array");

        std::vector<SedeInfo> out;
        for (const auto& item : *arr) {
            if (!item.is_object()) continue;
            SedeInfo s;
            // Fixes auth_probe.py's (None, None) bug ("key"/"data" primary, legacy fallbacks supported)
            s.key = FirstStr(item, {"key", "id", "codigo", "sede_id", "value"});
            s.name = FirstStr(item, {"data", "nombre", "name", "label"});
            if (!s.key.empty() || !s.name.empty()) {
                out.push_back(std::move(s));
            }
        }
        return Result<std::vector<SedeInfo>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<SedeInfo>>::Failure("invalid_json", ex.what());
    }
}

Result<std::vector<CarreraInfo>> TdaTdsClient::ParseCarrerasJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        const nlohmann::json* arr = nullptr;
        if (j.is_array()) arr = &j;
        else if (j.is_object() && j.contains("carreras") && j["carreras"].is_array()) arr = &j["carreras"];
        else return Result<std::vector<CarreraInfo>>::Failure("invalid_schema", "missing carreras array");

        std::vector<CarreraInfo> out;
        for (const auto& item : *arr) {
            if (!item.is_object()) continue;
            CarreraInfo c;
            c.key = FirstStr(item, {"key", "id", "codigo", "value"});
            c.name = FirstStr(item, {"data", "nombre", "name", "label"});
            if (!c.key.empty() || !c.name.empty()) {
                out.push_back(std::move(c));
            }
        }
        return Result<std::vector<CarreraInfo>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<CarreraInfo>>::Failure("invalid_json", ex.what());
    }
}

Result<std::vector<PlanInfo>> TdaTdsClient::ParsePlanesJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        const nlohmann::json* arr = nullptr;
        if (j.is_array()) arr = &j;
        else if (j.is_object() && j.contains("planes") && j["planes"].is_array()) arr = &j["planes"];
        else if (j.is_object() && j.contains("periodos") && j["periodos"].is_array()) arr = &j["periodos"];
        else return Result<std::vector<PlanInfo>>::Failure("invalid_schema", "missing planes array");

        std::vector<PlanInfo> out;
        for (const auto& item : *arr) {
            if (!item.is_object()) continue;
            PlanInfo p;
            p.key = FirstStr(item, {"key", "id", "codigo", "value"});
            p.name = FirstStr(item, {"data", "nombre", "name", "label"});
            if (!p.key.empty() || !p.name.empty()) {
                out.push_back(std::move(p));
            }
        }
        return Result<std::vector<PlanInfo>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<PlanInfo>>::Failure("invalid_json", ex.what());
    }
}

Result<ColorCodeCatalog> TdaTdsClient::ParseColorCodesJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<ColorCodeCatalog>::Failure("invalid_schema", "color_codes not an object");
        }
        ColorCodeCatalog cat;
        cat.title = JsonAsString(j, "title");
        cat.additional_text = FirstStr(j, {"aditional_text", "additional_text"});
        if (j.contains("color_codes") && j["color_codes"].is_array()) {
            for (const auto& cc : j["color_codes"]) {
                if (!cc.is_object()) continue;
                ColorCodeInfo info;
                info.label = JsonAsString(cc, "label");
                info.css_class = FirstStr(cc, {"css_class", "class"});
                cat.color_codes.push_back(std::move(info));
            }
        }
        return Result<ColorCodeCatalog>::Success(std::move(cat));
    } catch (const std::exception& ex) {
        return Result<ColorCodeCatalog>::Failure("invalid_json", ex.what());
    }
}

Result<CurriculumGraph> TdaTdsClient::ParseCurriculumGraphJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<CurriculumGraph>::Failure("invalid_schema", "curriculum not an object");
        }
        CurriculumGraph g;
        g.dsc_curriculum = JsonAsString(j, "dsc_curriculum");
        g.id_curriculum = JsonAsInt64(j, "id_curriculum");
        g.modality = JsonAsString(j, "modality");
        g.academic_degree = JsonAsString(j, "academic_degree");
        g.first_level = static_cast<int>(JsonAsInt64(j, "first_level"));

        if (j.contains("levels") && j["levels"].is_array()) {
            for (const auto& lv : j["levels"]) {
                if (!lv.is_object()) continue;
                CurriculumLevel cl;
                cl.id = JsonAsString(lv, "id");
                if (lv.contains("courses") && lv["courses"].is_array()) {
                    for (const auto& c : lv["courses"]) {
                        if (!c.is_object()) continue;
                        CurriculumCourseNode node;
                        node.id_course = JsonAsString(c, "id_course");
                        node.name = JsonAsString(c, "name");
                        node.truncated_name = FirstStr(c, {"trucatedName", "truncatedName"});
                        node.credits = static_cast<int>(JsonAsInt64(c, "credits"));
                        node.hours = static_cast<int>(JsonAsInt64(c, "hours"));
                        node.requirements = ParseIdOrStringArray(c, "requirements");
                        node.co_requirements = ParseIdOrStringArray(c, "co_requirements");
                        node.required_by = ParseIdOrStringArray(c, "required_by");
                        node.equivalent = ParseIdOrStringArray(c, "equivalent");
                        node.t_equiv = FirstStr(c, {"tEquiv", "t_equiv"});
                        cl.courses.push_back(std::move(node));
                    }
                }
                g.levels.push_back(std::move(cl));
            }
        }
        return Result<CurriculumGraph>::Success(std::move(g));
    } catch (const std::exception& ex) {
        return Result<CurriculumGraph>::Failure("invalid_json", ex.what());
    }
}

Result<StudentCurriculumProfile> TdaTdsClient::ParseStudentProfileJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<StudentCurriculumProfile>::Failure("invalid_schema", "profile not an object");
        }
        StudentCurriculumProfile prof;
        if (j.contains("grades") && j["grades"].is_array()) {
            for (const auto& rec : j["grades"]) {
                if (!rec.is_object()) continue;
                StudentCourseRecord sr;
                sr.id_course = JsonAsString(rec, "id_course");
                sr.best_status = JsonAsString(rec, "best_status");
                if (rec.contains("grades_record") && rec["grades_record"].is_array()) {
                    for (const auto& att : rec["grades_record"]) {
                        if (!att.is_object()) continue;
                        CourseGradeAttempt a;
                        a.year = static_cast<int>(JsonAsInt64(att, "year"));
                        a.period = static_cast<int>(JsonAsInt64(att, "period"));
                        a.modality = JsonAsString(att, "modality");
                        a.name_equivalent = JsonAsString(att, "name_equivalent");
                        a.id_equivalent = JsonAsString(att, "id_equivalent");
                        a.state = static_cast<int>(JsonAsInt64(att, "state"));
                        a.grade = JsonAsDouble(att, "grade");
                        a.group = static_cast<int>(JsonAsInt64(att, "group"));
                        if (att.contains("schedule") && att["schedule"].is_array()) {
                            for (const auto& sc : att["schedule"]) {
                                if (sc.is_string()) a.schedule.push_back(sc.get<std::string>());
                                else a.schedule.push_back(sc.dump());
                            }
                        }
                        sr.grades_record.push_back(std::move(a));
                    }
                }
                prof.grades.push_back(std::move(sr));
            }
        }
        return Result<StudentCurriculumProfile>::Success(std::move(prof));
    } catch (const std::exception& ex) {
        return Result<StudentCurriculumProfile>::Failure("invalid_json", ex.what());
    }
}

Result<StudentCareerRecord> TdaTdsClient::ParseStudentCareerJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<StudentCareerRecord>::Failure("invalid_schema", "career not an object");
        }
        StudentCareerRecord rec;
        rec.nombre = JsonAsString(j, "nombre");
        rec.sede = JsonAsString(j, "sede");
        rec.carrera = JsonAsString(j, "carrera");
        rec.plan = JsonAsInt64(j, "plan");
        return Result<StudentCareerRecord>::Success(std::move(rec));
    } catch (const std::exception& ex) {
        return Result<StudentCareerRecord>::Failure("invalid_json", ex.what());
    }
}

Result<NotificationsFeed> TdaTdsClient::ParseNotificationsJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<NotificationsFeed>::Failure("invalid_schema", "notifications not object");
        }
        NotificationsFeed feed;
        if (j.contains("category_sort")) {
            if (j["category_sort"].is_boolean()) feed.category_sort = j["category_sort"].get<bool>();
            else feed.category_sort = (JsonAsInt64(j, "category_sort") != 0);
        }
        if (j.contains("notifications") && j["notifications"].is_array()) {
            for (const auto& n : j["notifications"]) {
                if (!n.is_object()) continue;
                NotificationItem item;
                item.notification_id = JsonAsString(n, "notification_id");
                item.origin = JsonAsInt64(n, "origin");
                item.creation_date = JsonAsString(n, "creation_date");
                item.expiration_date = JsonAsString(n, "expiration_date");
                item.category_id = static_cast<int>(JsonAsInt64(n, "category_id"));
                item.text = JsonAsString(n, "text");
                item.url = JsonAsString(n, "url");
                item.title = JsonAsString(n, "title");
                item.state = JsonAsString(n, "state");
                feed.notifications.push_back(std::move(item));
            }
        }
        return Result<NotificationsFeed>::Success(std::move(feed));
    } catch (const std::exception& ex) {
        return Result<NotificationsFeed>::Failure("invalid_json", ex.what());
    }
}

Result<std::vector<PerformanceForm>> TdaTdsClient::ParsePerformanceFormsJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        const nlohmann::json* arr = nullptr;
        if (j.is_array()) arr = &j;
        else if (j.is_object() && j.contains("forms") && j["forms"].is_array()) arr = &j["forms"];
        else return Result<std::vector<PerformanceForm>>::Failure("invalid_schema", "forms not array");

        std::vector<PerformanceForm> out;
        for (const auto& f : *arr) {
            if (!f.is_object()) continue;
            PerformanceForm pf;
            pf.id = JsonAsInt64(f, "id");
            pf.name = JsonAsString(f, "name");
            pf.instructions = JsonAsString(f, "instructions");
            if (f.contains("sections") && f["sections"].is_array()) {
                for (const auto& s : f["sections"]) {
                    if (!s.is_object()) continue;
                    PerformanceFormSection sec;
                    sec.id = JsonAsInt64(s, "id");
                    sec.name = JsonAsString(s, "name");
                    sec.description = JsonAsString(s, "description");
                    sec.order = static_cast<int>(JsonAsInt64(s, "order"));
                    sec.type = JsonAsString(s, "type");
                    if (s.contains("items") && s["items"].is_array()) {
                        for (const auto& it : s["items"]) {
                            if (!it.is_object()) continue;
                            PerformanceFormItem pfi;
                            pfi.id = JsonAsInt64(it, "id");
                            pfi.description = JsonAsString(it, "description");
                            pfi.order = static_cast<int>(JsonAsInt64(it, "order"));
                            pfi.type = JsonAsString(it, "type");
                            sec.items.push_back(std::move(pfi));
                        }
                    }
                    pf.sections.push_back(std::move(sec));
                }
            }
            out.push_back(std::move(pf));
        }
        return Result<std::vector<PerformanceForm>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<PerformanceForm>>::Failure("invalid_json", ex.what());
    }
}

Result<std::vector<RssNewsItem>> TdaTdsClient::ParseRssXml(std::string_view xml_text) {
    std::vector<RssNewsItem> out;
    const std::string low = AsciiLower(xml_text);
    const size_t n = xml_text.size();
    auto is_ws = [](char c) {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    };

    size_t pos = 0;
    while (pos < n) {
        size_t content_start = std::string::npos;
        while (pos < n) {
            if (low.compare(pos, 9, "<![cdata[") == 0) {
                size_t end_cd = low.find("]]>", pos + 9);
                if (end_cd == std::string::npos) {
                    pos = n;
                    break;
                }
                pos = end_cd + 3;
                continue;
            }
            if (low.compare(pos, 4, "<!--") == 0) {
                size_t end_cm = low.find("-->", pos + 4);
                if (end_cm == std::string::npos) {
                    pos = n;
                    break;
                }
                pos = end_cm + 3;
                continue;
            }
            if (xml_text[pos] == '<' && pos + 5 < n && low.compare(pos + 1, 4, "item") == 0) {
                char next = xml_text[pos + 5];
                if (next == '>' || next == '/' || is_ws(next)) {
                    size_t cur = pos + 5;
                    char quote = 0;
                    while (cur < n) {
                        char ch = xml_text[cur];
                        if (quote != 0) {
                            if (ch == quote) quote = 0;
                        } else if (ch == '"' || ch == '\'') {
                            quote = ch;
                        } else if (ch == '>') {
                            break;
                        }
                        ++cur;
                    }
                    if (cur >= n) {
                        pos = n;
                        break;
                    }
                    if (cur > pos + 1 && xml_text[cur - 1] == '/') {
                        pos = cur + 1;
                        continue;
                    }
                    content_start = cur + 1;
                    break;
                }
            }
            ++pos;
        }
        if (content_start == std::string::npos) break;

        size_t scan = content_start;
        size_t end = std::string::npos;
        size_t after_end = n;
        while (scan < n) {
            if (low.compare(scan, 9, "<![cdata[") == 0) {
                size_t end_cd = low.find("]]>", scan + 9);
                if (end_cd == std::string::npos) break;
                scan = end_cd + 3;
                continue;
            }
            if (low.compare(scan, 4, "<!--") == 0) {
                size_t end_cm = low.find("-->", scan + 4);
                if (end_cm == std::string::npos) break;
                scan = end_cm + 3;
                continue;
            }
            if (scan + 6 < n && low.compare(scan, 6, "</item") == 0) {
                size_t after = scan + 6;
                while (after < n && is_ws(xml_text[after])) ++after;
                if (after < n && xml_text[after] == '>') {
                    end = scan;
                    after_end = after + 1;
                    break;
                }
            }
            ++scan;
        }
        if (end == std::string::npos) break;

        std::string_view block = xml_text.substr(content_start, end - content_start);
        RssNewsItem item;
        item.title = ExtractXmlTag(block, "title");
        item.link = ExtractXmlTag(block, "link");
        item.guid = ExtractXmlTag(block, "guid");
        item.description = ExtractXmlTag(block, "description");
        item.pub_date = ExtractXmlTag(block, "pubDate");
        if (!item.title.empty() || !item.link.empty() || !item.description.empty()) {
            out.push_back(std::move(item));
        }
        pos = after_end;
    }
    return Result<std::vector<RssNewsItem>>::Success(std::move(out));
}

Result<std::vector<CommunityMember>> TdaTdsClient::ParseMembersJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object() || !j.contains("data") || !j["data"].is_array()) {
            return Result<std::vector<CommunityMember>>::Failure("invalid_schema", "missing data array");
        }
        std::vector<CommunityMember> out;
        for (const auto& m : j["data"]) {
            if (!m.is_object()) continue;
            CommunityMember cm;
            cm.user_id = FirstStr(m, {"user_id", "checkcCol", "id"});
            cm.first_name = FirstStr(m, {"firstName", "first_name"});
            cm.last_name = FirstStr(m, {"lastName", "last_name"});
            cm.full_name = TrimAscii(cm.first_name + " " + cm.last_name);
            cm.email = StripTags(FirstStr(m, {"emailCol", "emailCard", "email"}));
            cm.role = StripTags(FirstStr(m, {"rolCol", "role"}));
            out.push_back(std::move(cm));
        }
        return Result<std::vector<CommunityMember>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<CommunityMember>>::Failure("invalid_json", ex.what());
    }
}

std::vector<CourseEvaluationCategory> TdaTdsClient::ParseEvaluationsHtml(
    std::string_view html_text,
    std::string* out_final_grade) {
    detail::WarmupRegexLocale();
    std::string html(html_text);
    if (out_final_grade != nullptr) {
        static const std::regex kFinalOpenRe(
            R"(<([a-zA-Z0-9]+)\b[^>]*\bid\s*=\s*["']studentFinal["'][^>]*>)",
            std::regex::ECMAScript | std::regex::icase);
        std::string inner = ExtractFirstElementInnerByRegex(html, kFinalOpenRe);
        if (!inner.empty()) {
            *out_final_grade = StripTags(inner);
        }
    }

    std::vector<CourseEvaluationCategory> cats;
    static const std::regex kCatRe(
        R"(<[^>]*\bclass\s*=\s*["'][^"']*\bsub_total\b[^"']*["'][^>]*>([\s\S]*?)</div>)",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kItemWeightRe(
        R"(\bitem_weight\s*=\s*["']([^"']*)["'])",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kAssignRe(
        R"(<[^>]*\bassign_id\s*=\s*["']([^"']*)["'][^>]*>([\s\S]*?)(?=<[^>]*\bassign_id\s*=|$))",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kAssignNameOpenRe(
        R"(<([a-zA-Z0-9]+)\b[^>]*\bclass\s*=\s*["'][^"']*\bassignNameText\b[^"']*["'][^>]*>)",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kGradeOpenRe(
        R"(<([a-zA-Z0-9]+)\b[^>]*\bclass\s*=\s*["'][^"']*\bvt_grade_student\b[^"']*["'][^>]*>)",
        std::regex::ECMAScript | std::regex::icase);

    // Collect category headers and assign items sequentially by document position
    struct CatMarker {
        size_t pos = 0;
        CourseEvaluationCategory cat;
    };
    std::vector<CatMarker> markers;
    {
        std::sregex_iterator it(html.begin(), html.end(), kCatRe);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            CourseEvaluationCategory c;
            c.name = StripTags((*it)[1].str());
            std::string full_match = (*it)[0].str();
            std::smatch wm;
            if (std::regex_search(full_match, wm, kItemWeightRe)) {
                c.weight_text = TrimAscii(wm.str(1));
            }
            markers.push_back({static_cast<size_t>((*it).position()), std::move(c)});
        }
    }

    std::sregex_iterator ait(html.begin(), html.end(), kAssignRe);
    std::sregex_iterator end;
    std::vector<CourseEvaluationItem> orphan_items;
    for (; ait != end; ++ait) {
        CourseEvaluationItem item;
        item.assign_id = TrimAscii((*ait)[1].str());
        std::string block = (*ait)[2].str();
        std::string nm_inner = ExtractFirstElementInnerByRegex(block, kAssignNameOpenRe);
        if (!nm_inner.empty()) {
            item.name = StripTags(nm_inner);
        }
        std::string gr_inner = ExtractFirstElementInnerByRegex(block, kGradeOpenRe);
        if (!gr_inner.empty()) {
            item.grade_text = StripTags(gr_inner);
        }
        size_t apos = static_cast<size_t>((*ait).position());
        CatMarker* owner = nullptr;
        for (auto& m : markers) {
            if (m.pos <= apos) owner = &m;
            else break;
        }
        if (owner != nullptr) owner->cat.items.push_back(std::move(item));
        else orphan_items.push_back(std::move(item));
    }

    for (auto& m : markers) {
        cats.push_back(std::move(m.cat));
    }
    if (!orphan_items.empty()) {
        CourseEvaluationCategory unc;
        unc.name = "Evaluaciones";
        unc.items = std::move(orphan_items);
        cats.push_back(std::move(unc));
    }
    return cats;
}

std::vector<PrematriculaCourseEntry> TdaTdsClient::ParsePrematriculaTipo1(
    std::string_view body,
    int* out_max_credits,
    std::string* out_rn) {
    std::string trimmed = TrimAscii(body);
    std::vector<PrematriculaCourseEntry> out;
    if (trimmed.empty() || trimmed == "-1") return out;

    auto parts = SplitDelim(trimmed, '~');
    if (parts.size() < 2) return out;
    if (out_max_credits != nullptr) *out_max_credits = std::atoi(parts[0].c_str());
    if (out_rn != nullptr) *out_rn = TrimAscii(parts[1]);

    for (size_t i = 2; i + 3 < parts.size(); i += 4) {
        PrematriculaCourseEntry e;
        e.course_code = TrimAscii(parts[i]);
        e.priority = std::atoi(parts[i + 1].c_str());
        e.course_elective = TrimAscii(parts[i + 2]);
        e.name_elective = TrimAscii(parts[i + 3]);
        if (!e.course_code.empty()) {
            out.push_back(std::move(e));
        }
    }
    return out;
}

std::vector<PrematriculaRequestEntry> TdaTdsClient::ParsePrematriculaTipo2(std::string_view body) {
    std::string trimmed = TrimAscii(body);
    std::vector<PrematriculaRequestEntry> out;
    if (trimmed.empty() || trimmed == "-1") return out;

    for (const auto& rec : SplitDelim(trimmed, '~')) {
        std::string r = TrimAscii(rec);
        if (r.empty()) continue;
        size_t bt = r.find('`');
        PrematriculaRequestEntry e;
        if (bt == std::string::npos) {
            e.course_code = r;
        } else {
            e.course_code = TrimAscii(r.substr(0, bt));
            e.observation = TrimAscii(r.substr(bt + 1));
        }
        if (!e.course_code.empty()) out.push_back(std::move(e));
    }
    return out;
}

std::vector<std::string> TdaTdsClient::ParseDisabledCourses(std::string_view body) {
    std::vector<std::string> out;
    std::istringstream iss{std::string(body)};
    std::string tok;
    while (iss >> tok) {
        size_t dot = tok.find('.');
        out.push_back(dot != std::string::npos && dot + 1 < tok.size() ? tok.substr(dot + 1) : tok);
    }
    return out;
}

Result<std::vector<SedeInfo>> TdaTdsClient::GetCurriculumSedes() {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/carga_sedes_json");
    if (!r.ok) return Result<std::vector<SedeInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParseSedesJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeSedes(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<std::vector<CarreraInfo>> TdaTdsClient::GetCurriculumCarreras(std::string_view id_sede) {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/carga_carreras_json?id_sede=" +
                                  UrlEncode(id_sede));
    if (!r.ok) return Result<std::vector<CarreraInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParseCarrerasJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeCarreras(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<std::vector<PlanInfo>> TdaTdsClient::GetCurriculumPlanes(std::string_view id_sede,
                                                                std::string_view id_depto) {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/carga_planes_json?id_sede=" +
                                  UrlEncode(id_sede) + "&id_depto=" + UrlEncode(id_depto));
    if (!r.ok) return Result<std::vector<PlanInfo>>::Failure(r.error_code, r.error_message);
    auto parsed = ParsePlanesJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizePlanes(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<ColorCodeCatalog> TdaTdsClient::GetCurriculumColorCodes() {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/color_codes_json");
    if (!r.ok) return Result<ColorCodeCatalog>::Failure(r.error_code, r.error_message);
    auto parsed = ParseColorCodesJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeColorCodes(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<CurriculumGraph> TdaTdsClient::GetCurriculumGraph(std::string_view id_plan) {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/json_draw_angular?id_plan=" +
                                  UrlEncode(id_plan));
    if (!r.ok) return Result<CurriculumGraph>::Failure(r.error_code, r.error_message);
    auto parsed = ParseCurriculumGraphJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeCurriculumGraph(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<StudentCurriculumProfile> TdaTdsClient::GetStudentCurriculumProfile(
    std::string_view id_plan,
    std::string_view carne,
    int full_mode) {
    std::string path = "/tds-curriculum-exp/ajax/json_student_profile_angular?id_plan=" +
                       UrlEncode(id_plan) + "&carne=" + UrlEncode(carne) +
                       "&full_mode=" + std::to_string(full_mode);
    auto r = FetchText("GET", path);
    if (!r.ok) return Result<StudentCurriculumProfile>::Failure(r.error_code, r.error_message);
    auto parsed = ParseStudentProfileJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeStudentProfile(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<StudentCareerRecord> TdaTdsClient::GetStudentCareerData(std::string_view carne_oficial) {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/obtener_datos_carrera_estudiante?carneOficial=" +
                                  UrlEncode(carne_oficial));
    if (!r.ok) return Result<StudentCareerRecord>::Failure(r.error_code, r.error_message);
    auto parsed = ParseStudentCareerJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeStudentCareer(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<std::string> TdaTdsClient::GetStudentOfficialName(std::string_view carne_oficial) {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/obtener_nombre_estudiante?carneOficial=" +
                                  UrlEncode(carne_oficial));
    if (!r.ok) return r;
    const Sanitizer san = SnapshotSanitizer();
    std::string s = TrimAscii(r.value);
    if (!s.empty() && s.front() == '{') {
        try {
            auto j = nlohmann::json::parse(s);
            std::string nm = FirstStr(j, {"nombre", "name", "student_name"});
            if (!nm.empty()) return Result<std::string>::Success(san.SanitizeText(nm));
        } catch (...) {
        }
    }
    return Result<std::string>::Success(san.SanitizeText(s));
}

Result<int> TdaTdsClient::GetCurriculumAppCode() {
    auto r = FetchText("GET", "/tds-curriculum-exp/ajax/obtener_codigo_aplicacion.tcl");
    if (!r.ok) return Result<int>::Failure(r.error_code, r.error_message);
    try {
        auto j = nlohmann::json::parse(r.value);
        return Result<int>::Success(static_cast<int>(JsonAsInt64(j, "code")));
    } catch (const std::exception& ex) {
        return Result<int>::Failure("invalid_json", SnapshotSanitizer().SanitizeText(ex.what()));
    }
}

Result<bool> TdaTdsClient::ValidateModificationPermissions(std::string_view carne) {
    std::string path = "/tds-curriculum-exp/ajax/validar_permisos_modificacion?carne=" +
                       (carne.empty() ? std::string("undefined") : UrlEncode(carne));
    auto r = FetchText("GET", path);
    if (!r.ok) return Result<bool>::Failure(r.error_code, r.error_message);
    std::string low = AsciiLower(TrimAscii(r.value));
    if (low == "true" || low == "1") return Result<bool>::Success(true);
    if (low == "false" || low == "0") return Result<bool>::Success(false);
    try {
        auto j = nlohmann::json::parse(low);
        if (j.is_boolean()) return Result<bool>::Success(j.get<bool>());
        if (j.is_object() && j.contains("can_modify")) {
            return Result<bool>::Success(JsonAsInt64(j, "can_modify") != 0 ||
                                         JsonAsString(j, "can_modify") == "true");
        }
    } catch (...) {
    }
    return Result<bool>::Success(false);
}

Result<std::string> TdaTdsClient::GetCurriculumAngularTemplate(std::string_view template_name) {
    auto r = FetchText("GET", "/tds-curriculum-exp/pg_angular/templates/" + std::string(template_name),
                       "", "", "text/html, */*");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeHtml(r.value);
    return r;
}

Result<std::map<std::string, std::string>> TdaTdsClient::GetLegacyCurriculumInternationalization(
    std::string_view js) {
    auto r = FetchText("GET", "/tds-curriculum/ajax/internationalization?js=" + UrlEncode(js));
    if (!r.ok) return Result<std::map<std::string, std::string>>::Failure(r.error_code, r.error_message);
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto m = ParseFlatStringMap(r.value);
        for (auto& kv : m) kv.second = san.SanitizeText(kv.second);
        return Result<std::map<std::string, std::string>>::Success(std::move(m));
    } catch (const std::exception& ex) {
        return Result<std::map<std::string, std::string>>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

Result<std::string> TdaTdsClient::GetLegacyCurriculumDisplayHtml(std::string_view ide_plan,
                                                                 std::string_view depto_id,
                                                                 std::string_view carne) {
    std::string path = "/tds-curriculum/display_3?ide_plan=" + UrlEncode(ide_plan) +
                       "&depto_id=" + UrlEncode(depto_id) + "&carne=" + UrlEncode(carne);
    auto r = FetchText("GET", path, "", "", "text/html, */*");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeHtml(r.value);
    return r;
}

Result<ExpedienteSummary> TdaTdsClient::GetExpedienteSummary(std::string_view carne) {
    ExpedienteSummary sum;
    const std::string resolved_carne = ResolveStudentCarne(carne);

    auto i18n_r = FetchText("GET", "/tda-expediente-estudiantil/ajax/i18n", "", "");
    if (!i18n_r.ok && i18n_r.error_code == "http_405") {
        i18n_r = FetchText("POST", "/tda-expediente-estudiantil/ajax/i18n", "",
                           "application/x-www-form-urlencoded");
    }
    if (!i18n_r.ok && i18n_r.error_code == "session_expired") {
        return Result<ExpedienteSummary>::Failure(i18n_r.error_code, i18n_r.error_message);
    }
    if (i18n_r.ok) {
        try {
            auto j = nlohmann::json::parse(i18n_r.value);
            if (j.is_object()) {
                sum.package_id = static_cast<int>(JsonAsInt64(j, "PackageId"));
                for (auto it = j.begin(); it != j.end(); ++it) {
                    if (it.value().is_string()) sum.i18n_labels[it.key()] = it.value().get<std::string>();
                }
            }
        } catch (...) {
        }
    }

    auto val_r = FetchText("GET", "/tda-expediente-estudiantil/ajax/i18n_validation", "", "");
    if (!val_r.ok && val_r.error_code == "http_405") {
        val_r = FetchText("POST", "/tda-expediente-estudiantil/ajax/i18n_validation", "",
                          "application/x-www-form-urlencoded");
    }
    if (val_r.ok) {
        try {
            sum.validation_labels = ParseFlatStringMap(val_r.value);
        } catch (...) {
        }
    }

    auto post_or_get = [&](std::string_view path, std::string_view post_body,
                           std::string_view get_query, std::string_view accept) {
        auto r = FetchText("POST", path, post_body, "application/x-www-form-urlencoded", accept);
        if (!r.ok && r.error_code != "session_expired" &&
            (r.error_code == "http_405" || r.error_code == "http_404")) {
            r = FetchText("GET", std::string(path) + std::string(get_query), "", "", accept);
        }
        return r;
    };

    std::string q = resolved_carne.empty() ? "" : ("?carne=" + UrlEncode(resolved_carne));
    std::string post_carne = resolved_carne.empty() ? "" : ("carne=" + UrlEncode(resolved_carne));
    if (auto r = FetchText("GET", "/tda-expediente-estudiantil/ajax/detalle_notas" + q,
                           "", "", "text/html, */*"); r.ok) {
        sum.detalle_notas_html = std::move(r.value);
        sum.structured.grades = ParseDetalleNotasHtml(sum.detalle_notas_html);
    }
    if (auto r = FetchText("GET", "/tda-expediente-estudiantil/ajax/detalle_promedio_ponderado" + q,
                           "", "", "text/html, */*"); r.ok) {
        sum.detalle_promedio_html = r.value;
        static const std::regex kNotaRe(
            R"(<[^>]*\bid\s*=\s*["']nota["'][^>]*>([\s\S]*?)</[^>]+>)",
            std::regex::ECMAScript | std::regex::icase);
        static const std::regex kAprobRe(
            R"(<[^>]*\bid\s*=\s*["']porcentaje_aprobado["'][^>]*>([\s\S]*?)</[^>]+>)",
            std::regex::ECMAScript | std::regex::icase);
        std::smatch m;
        if (std::regex_search(sum.detalle_promedio_html, m, kNotaRe)) {
            sum.weighted_average_text = StripTags(m.str(1));
        }
        if (std::regex_search(sum.detalle_promedio_html, m, kAprobRe)) {
            sum.approval_pct_text = StripTags(m.str(1));
        }
    }
    if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/generar_tabla_promedio_ponderado",
                             post_carne, q, "text/html, */*"); r.ok) {
        sum.tabla_promedio_html = std::move(r.value);
        sum.structured.weighted_averages = ParsePromedioPonderadoTableHtml(sum.tabla_promedio_html);
    }
    if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/detalle_historial",
                             post_carne, q, "text/html, */*"); r.ok) {
        sum.detalle_historial_html = std::move(r.value);
        sum.structured.stats = ParseAcademicStatsHtml(sum.detalle_historial_html);
    }
    if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/citas_matricula",
                             post_carne, q, "text/html, */*"); r.ok) {
        sum.citas_matricula_html = std::move(r.value);
        sum.structured.citas_matricula = ParseCitasMatriculaHtml(sum.citas_matricula_html);
    }
    if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/guia_horario",
                             post_carne, q, "text/html, */*"); r.ok) {
        sum.guia_horario_html = std::move(r.value);
    }
    if (auto ga = GetGraduateAttributes(resolved_carne); ga.ok) {
        sum.structured.graduate_attributes = std::move(ga.value);
    }

    if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/combos/carga_sedes_tds_lib",
                             "", "", "text/html,application/json,*/*"); r.ok) {
        auto pairs = ParseExpedienteComboJsonOrHtml(r.value);
        for (auto& kv : pairs) sum.sedes_combo.push_back({std::move(kv.first), std::move(kv.second)});
    }
    if (sum.sedes_combo.empty()) {
        if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/combos/carga_sedes",
                                 "", "", "text/html,application/json,*/*"); r.ok) {
            auto pairs = ParseExpedienteComboJsonOrHtml(r.value);
            for (auto& kv : pairs) sum.sedes_combo.push_back({std::move(kv.first), std::move(kv.second)});
        }
    }

    if (auto r = post_or_get("/tda-expediente-estudiantil/ajax/combos/carga_periodos_tds_lib",
                             "", "", "text/html,application/json,*/*"); r.ok) {
        auto pairs = ParseExpedienteComboJsonOrHtml(r.value);
        for (auto& kv : pairs) sum.periodos_combo.push_back({std::move(kv.first), std::move(kv.second)});
    }

    SanitizeExpediente(sum, SnapshotSanitizer());
    return Result<ExpedienteSummary>::Success(std::move(sum));
}

Result<std::vector<GraduateAttributeRow>> TdaTdsClient::GetGraduateAttributes(
    std::string_view carne) {
    std::string resolved = ResolveStudentCarne(carne);
    std::string body = "student_id=" + UrlEncode(resolved);
    auto r = FetchText("POST", "/tda-expediente-estudiantil/ajax/get_data_attributes",
                       body, "application/x-www-form-urlencoded", "text/html, */*");
    if (!r.ok) {
        return Result<std::vector<GraduateAttributeRow>>::Failure(r.error_code, r.error_message);
    }
    auto rows = ParseGraduateAttributesHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& g : rows) {
        g.attribute_name = san.SanitizeText(g.attribute_name);
        g.description = san.SanitizeText(g.description);
        g.courses = san.SanitizeText(g.courses);
    }
    return Result<std::vector<GraduateAttributeRow>>::Success(std::move(rows));
}

Result<std::vector<WeeklyScheduleCell>> TdaTdsClient::GetCurrentPeriodSchedule(
    std::string_view periodo,
    std::string_view carne) {
    std::string resolved = ResolveStudentCarne(carne);
    std::string exp_period = ConvertPeriodKeyToExpedienteFormat(periodo);
    std::string body = "periodo=" + UrlEncode(exp_period) + "&carne=" + UrlEncode(resolved);
    auto r = FetchText("POST", "/tda-expediente-estudiantil/ajax/tabla_horario",
                       body, "application/x-www-form-urlencoded", "text/html, */*");
    if (!r.ok) return Result<std::vector<WeeklyScheduleCell>>::Failure(r.error_code, r.error_message);
    auto cells = ParseTablaHorarioHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& c : cells) {
        c.course_code = san.SanitizeText(c.course_code);
        c.course_name = san.SanitizeText(c.course_name);
        c.raw_text = san.SanitizeText(c.raw_text);
        c.room = san.SanitizeText(c.room);
    }
    return Result<std::vector<WeeklyScheduleCell>>::Success(std::move(cells));
}

Result<std::vector<CarreraInfo>> TdaTdsClient::GetGuiaHorarioCarreras(std::string_view sede) {
    std::string body = "sede=" + UrlEncode(sede) + "&id_sede=" + UrlEncode(sede);
    auto r = FetchText("POST", "/tda-expediente-estudiantil/ajax/combos/carga_carreras_tds_lib",
                       body, "application/x-www-form-urlencoded", "text/html,application/json,*/*");
    if (!r.ok || TrimAscii(r.value).empty()) {
        auto r2 = FetchText("POST", "/tda-expediente-estudiantil/ajax/combos/carga_carreras",
                            body, "application/x-www-form-urlencoded", "text/html,application/json,*/*");
        if (r2.ok) r = std::move(r2);
    }
    if (!r.ok) return Result<std::vector<CarreraInfo>>::Failure(r.error_code, r.error_message);
    std::vector<CarreraInfo> out;
    for (auto& kv : ParseExpedienteComboJsonOrHtml(r.value)) {
        if (!kv.first.empty() || !kv.second.empty()) {
            out.push_back({std::move(kv.first), std::move(kv.second)});
        }
    }
    SanitizeCarreras(out, SnapshotSanitizer());
    return Result<std::vector<CarreraInfo>>::Success(std::move(out));
}

Result<std::vector<GuiaHorarioRow>> TdaTdsClient::GetGuiaHorarioRows(
    std::string_view sede,
    std::string_view carrera,
    std::string_view periodo) {
    std::string exp_period = ConvertPeriodKeyToExpedienteFormat(periodo);
    std::string body = "sede=" + UrlEncode(sede) + "&carrera=" + UrlEncode(carrera) +
                       "&periodo=" + UrlEncode(exp_period);
    auto r = FetchText("POST", "/tda-expediente-estudiantil/ajax/tabla_guia_horario",
                       body, "application/x-www-form-urlencoded", "text/html, */*");
    if (!r.ok) return Result<std::vector<GuiaHorarioRow>>::Failure(r.error_code, r.error_message);
    auto rows = ParseTablaGuiaHorarioHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& g : rows) {
        g.materia = san.SanitizeText(g.materia);
        g.profesor = san.SanitizeText(g.profesor);
        g.aula = san.SanitizeText(g.aula);
    }
    return Result<std::vector<GuiaHorarioRow>>::Success(std::move(rows));
}

Result<DetailedEvaluationsReport> TdaTdsClient::GetDetailedCourseEvaluations(
    std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    size_t slash = comm.rfind('/');
    std::string slug = (slash != std::string::npos) ? comm.substr(slash + 1) : comm;
    auto r = FetchText("GET", comm + "/evaluation/tda-ce-estudiante/tda-index",
                       "", "", "text/html, */*");
    if (!r.ok) return Result<DetailedEvaluationsReport>::Failure(r.error_code, r.error_message);
    auto rep = ParseDetailedEvaluationsHtml(r.value, slug);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& cat : rep.categories) {
        cat.name = san.SanitizeText(cat.name);
        for (auto& it : cat.items) {
            it.name = san.SanitizeText(it.name);
            it.description = san.SanitizeText(it.description);
            it.teacher_comment = san.SanitizeText(it.teacher_comment);
        }
    }
    return Result<DetailedEvaluationsReport>::Success(std::move(rep));
}

Result<RubricBreakdown> TdaTdsClient::GetRubricBreakdown(std::string_view rubric_id,
                                                         std::string_view community_id,
                                                         std::string_view task_item_id,
                                                         std::string_view task_id) {
    std::string path = "/tda-rubrica/tda_evaluacion_rubrica?rubric_id=" + UrlEncode(rubric_id) + "&accion=ver";
    if (!community_id.empty()) path += "&community_id=" + UrlEncode(community_id);
    if (!task_item_id.empty()) path += "&task_item_id=" + UrlEncode(task_item_id);
    if (!task_id.empty()) path += "&task_id=" + UrlEncode(task_id);
    auto r = FetchText("GET", path, "", "", "text/html, */*");
    if (!r.ok) return Result<RubricBreakdown>::Failure(r.error_code, r.error_message);
    auto rb = ParseRubricHtml(r.value, rubric_id);
    const Sanitizer san = SnapshotSanitizer();
    rb.title = san.SanitizeText(rb.title);
    for (auto& c : rb.criteria) {
        c.category_name = san.SanitizeText(c.category_name);
        for (auto& lv : c.levels) {
            lv.label = san.SanitizeText(lv.label);
            lv.description = san.SanitizeText(lv.description);
        }
    }
    return Result<RubricBreakdown>::Success(std::move(rb));
}

Result<CourseOverviewSnapshot> TdaTdsClient::GetCourseOverview(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/one-community?page_num=0&loc=", "", "", "text/html, */*");
    if (!r.ok) return Result<CourseOverviewSnapshot>::Failure(r.error_code, r.error_message);
    auto ov = ParseCourseOverviewHtml(r.value, comm, base_url_);
    const Sanitizer san = SnapshotSanitizer();
    ov.title = san.SanitizeText(ov.title);
    ov.description = san.SanitizeText(ov.description);
    ov.teacher.name = san.SanitizeText(ov.teacher.name);
    ov.teacher.office = san.SanitizeText(ov.teacher.office);
    return Result<CourseOverviewSnapshot>::Success(std::move(ov));
}

Result<std::vector<EvalSubmissionRevision>> TdaTdsClient::GetEvaluationSubmissionHistory(
    std::string_view community_path,
    std::string_view task_item_id,
    std::string_view student_id,
    std::string_view task_name,
    std::string_view type,
    std::string_view has_grade) {
    std::string comm = NormalizeCommPath(community_path);
    std::string sid = ResolveStudentCarne(student_id);
    std::string body = "task_item_id=" + UrlEncode(task_item_id) +
                       "&student_id=" + UrlEncode(sid) +
                       "&task_name=" + UrlEncode(task_name) +
                       "&type=" + UrlEncode(type) +
                       "&has_grade=" + UrlEncode(has_grade);
    auto r = FetchText("POST", comm + "/evaluation/tda-ce-estudiante/ajax/get_history_files",
                       body, "application/x-www-form-urlencoded", "text/html, */*");
    if (!r.ok) {
        return Result<std::vector<EvalSubmissionRevision>>::Failure(r.error_code, r.error_message);
    }
    auto revs = ParseEvalHistoryHtml(r.value, base_url_);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& rv : revs) {
        rv.file_name = san.SanitizeText(rv.file_name);
        rv.download_url = san.SanitizeUrl(rv.download_url);
    }
    return Result<std::vector<EvalSubmissionRevision>>::Success(std::move(revs));
}

Result<std::optional<EvalSubmissionRevision>> TdaTdsClient::GetEvaluationLiveRevision(
    std::string_view community_path,
    std::string_view task_item_id,
    std::string_view student_id,
    std::string_view task_name,
    std::string_view task_id) {
    std::string comm = NormalizeCommPath(community_path);
    std::string sid = ResolveStudentCarne(student_id);
    std::string body = "task_item_id=" + UrlEncode(task_item_id) +
                       "&student_id=" + UrlEncode(sid) +
                       "&task_name=" + UrlEncode(task_name) +
                       "&task_id=" + UrlEncode(task_id);
    auto r = FetchText("POST", comm + "/evaluation/tda-ce-estudiante/ajax/get_data_liverevision",
                       body, "application/x-www-form-urlencoded", "text/html, */*");
    if (!r.ok) {
        return Result<std::optional<EvalSubmissionRevision>>::Failure(r.error_code, r.error_message);
    }
    auto rev = ParseEvalLiveRevisionHtml(r.value, base_url_);
    if (rev) {
        const Sanitizer san = SnapshotSanitizer();
        rev->file_name = san.SanitizeText(rev->file_name);
        rev->download_url = san.SanitizeUrl(rev->download_url);
    }
    return Result<std::optional<EvalSubmissionRevision>>::Success(std::move(rev));
}

Result<std::vector<CourseExternalLink>> TdaTdsClient::CrawlCustomCommunityTabs(
    std::string_view community_path,
    const std::vector<CourseTabInfo>& tabs,
    int max_tabs,
    int polite_delay_ms) {
    std::string comm = NormalizeCommPath(community_path);
    std::vector<CourseExternalLink> out;
    std::unordered_set<std::string> seen_urls;
    const Sanitizer san = SnapshotSanitizer();

    int crawled = 0;
    for (const auto& t : tabs) {
        if (!t.is_custom && t.page_num < 4) continue;
        if (max_tabs > 0 && crawled >= max_tabs) break;
        ++crawled;
        if (polite_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(polite_delay_ms));
        }
        std::string tab_url = comm + "/one-community?page_num=" + std::to_string(t.page_num) + "&loc=";
        auto r = FetchText("GET", tab_url, "", "", "text/html, */*");
        if (!r.ok) {
            if (r.error_code == "session_expired") {
                return Result<std::vector<CourseExternalLink>>::Failure(r.error_code, r.error_message);
            }
            continue;
        }
        auto links = ExtractCustomTabLinksHtml(r.value, t.label, base_url_);
        for (auto& l : links) {
            if (seen_urls.insert(l.url).second) {
                l.label = san.SanitizeText(l.label);
                l.url = san.SanitizeUrl(l.url);
                out.push_back(std::move(l));
            }
        }
    }
    return Result<std::vector<CourseExternalLink>>::Success(std::move(out));
}

Result<std::vector<CalendarEventEntry>> TdaTdsClient::GetCalendarEvents(
    std::string_view date_iso,
    std::string_view view,
    std::string_view community_path) {
    std::string prefix = community_path.empty() ? "/dotlrn" : NormalizeCommPath(community_path);
    std::string path = prefix + "/calendar/view?view=" + UrlEncode(view.empty() ? "list" : view);
    if (!date_iso.empty()) path += "&date=" + UrlEncode(date_iso);
    auto r = FetchText("GET", path, "", "", "text/html, */*");
    if (!r.ok) return Result<std::vector<CalendarEventEntry>>::Failure(r.error_code, r.error_message);
    auto evs = ParseCalendarListHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& e : evs) {
        e.title = san.SanitizeText(e.title);
        e.description = san.SanitizeText(e.description);
        e.calendar_name = san.SanitizeText(e.calendar_name);
    }
    return Result<std::vector<CalendarEventEntry>>::Success(std::move(evs));
}

Result<PlagiarismTreeSnapshot> TdaTdsClient::GetPlagiarismTree() {
    auto r = FetchText("GET", "/tda-control-plagio/ajax/cargaArbol", "", "", "application/json, text/html, */*");
    if (!r.ok) return Result<PlagiarismTreeSnapshot>::Failure(r.error_code, r.error_message);
    auto tree = ParsePlagiarismTreeJson(r.value);
    if (tree.documents.empty() && !tree.my_files_folder_id.empty()) {
        if (auto mis = GetPlagiarismMisDocs(tree.my_files_folder_id); mis.ok) {
            for (const auto& item : mis.value) {
                PlagiarismDocRecord rec;
                rec.doc_uid = item.item_id.empty() ? item.doc_id : item.item_id;
                rec.file_name = item.file_name;
                rec.status_code = item.status;
                rec.status_text = !item.status.empty() ? item.status : "Revisado";
                rec.similarity_pct = item.similarity_pct;
                rec.visibility = item.visibility;
                rec.source_section = item.package_from;
                tree.documents.push_back(std::move(rec));
            }
        }
    }
    const Sanitizer san = SnapshotSanitizer();
    tree.my_files_name = san.SanitizeText(tree.my_files_name);
    for (auto& d : tree.documents) {
        d.file_name = san.SanitizeText(d.file_name);
        d.status_text = san.SanitizeText(d.status_text);
    }
    return Result<PlagiarismTreeSnapshot>::Success(std::move(tree));
}

Result<std::vector<PlagiarismMisDocItem>> TdaTdsClient::GetPlagiarismMisDocs(
    std::string_view folder_id) {
    if (folder_id.empty()) {
        return Result<std::vector<PlagiarismMisDocItem>>::Failure("invalid_arg", "folder_id required");
    }
    auto r = FetchText("GET",
                       "/tda-control-plagio/ajax/cargaDocumentos/cargaMisDocs?folder_id=" +
                           UrlEncode(folder_id),
                       "", "", "text/html, */*");
    if (!r.ok) {
        return Result<std::vector<PlagiarismMisDocItem>>::Failure(r.error_code, r.error_message);
    }
    auto docs = ParsePlagiarismMisDocsHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& d : docs) {
        d.file_name = san.SanitizeText(d.file_name);
        d.status = san.SanitizeText(d.status);
    }
    return Result<std::vector<PlagiarismMisDocItem>>::Success(std::move(docs));
}

Result<bool> TdaTdsClient::GetPlagiarismTermsAccepted() {
    auto r = FetchText("GET",
                       "/tda-control-plagio/ajax/cargaDocumentos/termsAndConditions?type=1",
                       "", "", "text/plain, application/json, */*");
    if (!r.ok) return Result<bool>::Failure(r.error_code, r.error_message);
    std::string low = AsciiLower(TrimAscii(r.value));
    return Result<bool>::Success(low == "true" || low == "1");
}

Result<std::string> TdaTdsClient::GetPlagiarismDocInfo(std::string_view doc_uid) {
    if (doc_uid.empty()) {
        return Result<std::string>::Failure("invalid_arg", "doc_uid required");
    }
    auto r = FetchText("GET",
                       "/tda-control-plagio/ajax/getDocInfo?doc_uid=" + UrlEncode(doc_uid),
                       "", "", "application/json, text/html, */*");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeText(r.value);
    return r;
}

Result<std::string> TdaTdsClient::GetPlagiarismReportUrl(std::string_view doc_uid) {
    if (doc_uid.empty()) {
        return Result<std::string>::Failure("invalid_arg", "doc_uid required");
    }
    auto r = FetchText("GET",
                       "/tda-control-plagio/ajax/generaReporte?doc_uid=" + UrlEncode(doc_uid),
                       "", "", "application/json, text/plain, */*");
    if (!r.ok) return r;
    std::string body = TrimAscii(r.value);
    try {
        if (!body.empty() && body.front() == '{') {
            auto j = nlohmann::json::parse(body);
            std::string u = FirstStr(j, {"url", "report_url", "link"});
            if (!u.empty()) return Result<std::string>::Success(SnapshotSanitizer().SanitizeText(u));
        }
    } catch (...) {
    }
    return Result<std::string>::Success(SnapshotSanitizer().SanitizeText(body));
}

Result<std::vector<GaapActivityRecord>> TdaTdsClient::GetGaapActivityHistory(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/tda-motor-juegos/verestadisticas", "", "", "text/html, application/json, */*");
    if (!r.ok) {
        if (r.error_code == "http_404") {
            return Result<std::vector<GaapActivityRecord>>::Success({});
        }
        return Result<std::vector<GaapActivityRecord>>::Failure(r.error_code, r.error_message);
    }
    auto items = ParseGaapHistoryHtmlOrJson(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& g : items) {
        g.title = san.SanitizeText(g.title);
        g.status = san.SanitizeText(g.status);
    }
    return Result<std::vector<GaapActivityRecord>>::Success(std::move(items));
}

CurriculumCriticalPath TdaTdsClient::ComputeCurriculumCriticalPath(
    const CurriculumGraph& graph,
    const StudentCurriculumProfile* profile) {
    CurriculumCriticalPath res;

    auto to_low = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto is_approved = [&](const std::string& code) -> bool {
        if (!profile) return false;
        const std::string target = to_low(code);
        for (const auto& g : profile->grades) {
            if (to_low(g.id_course) != target) continue;
            const std::string bs = to_low(g.best_status);
            if (bs == "1" || bs.find("aprob") != std::string::npos ||
                bs.find("reconoc") != std::string::npos || bs.find("sufic") != std::string::npos) {
                return true;
            }
            for (const auto& att : g.grades_record) {
                if (att.state == 1 || att.grade >= 67.5) return true;
            }
        }
        return false;
    };

    std::map<std::string, const CurriculumCourseNode*> by_low;
    std::vector<std::string> all_low;
    std::map<std::string, std::vector<std::string>> dependents;
    std::set<std::string> approved_set;

    for (const auto& lv : graph.levels) {
        for (const auto& c : lv.courses) {
            std::string lk = to_low(c.id_course);
            if (lk.empty()) continue;
            by_low[lk] = &c;
            all_low.push_back(lk);
            ++res.total_courses;
            if (is_approved(c.id_course)) {
                approved_set.insert(lk);
                ++res.approved_courses;
                res.completed_credits += std::max(0, c.credits);
            } else {
                ++res.remaining_courses;
                res.remaining_credits += std::max(0, c.credits);
            }
        }
    }

    for (const auto& lk : all_low) {
        const auto* node = by_low[lk];
        for (const auto& req : node->requirements) {
            std::string rlk = to_low(req);
            if (by_low.count(rlk)) {
                dependents[rlk].push_back(lk);
            }
        }
    }

    // Courses unlocked right now (remaining, all requirements in approved_set)
    for (const auto& lk : all_low) {
        if (approved_set.count(lk)) continue;
        const auto* node = by_low[lk];
        bool ready = true;
        for (const auto& req : node->requirements) {
            std::string rlk = to_low(req);
            if (by_low.count(rlk) && !approved_set.count(rlk)) {
                ready = false;
                break;
            }
        }
        if (ready) res.unlocked_next_codes.push_back(node->id_course);
    }

    // Longest remaining prerequisite chain via memoized DFS (cycle-safe)
    std::map<std::string, int> memo_depth;
    std::map<std::string, std::string> next_hop;
    std::set<std::string> visiting;

    std::function<int(const std::string&)> dfs = [&](const std::string& u) -> int {
        if (approved_set.count(u)) return 0;
        auto mit = memo_depth.find(u);
        if (mit != memo_depth.end()) return mit->second;
        if (!visiting.insert(u).second) return 1; // cycle guard
        int best_child = 0;
        std::string best_next;
        auto dit = dependents.find(u);
        if (dit != dependents.end()) {
            for (const auto& v : dit->second) {
                if (approved_set.count(v)) continue;
                int d = dfs(v);
                if (d > best_child) {
                    best_child = d;
                    best_next = v;
                }
            }
        }
        visiting.erase(u);
        memo_depth[u] = 1 + best_child;
        if (!best_next.empty()) next_hop[u] = best_next;
        return 1 + best_child;
    };

    int best_total = 0;
    std::string best_start;
    for (const auto& lk : all_low) {
        if (approved_set.count(lk)) continue;
        int d = dfs(lk);
        if (d > best_total) {
            best_total = d;
            best_start = lk;
        }
    }
    res.min_semesters_remaining = best_total;
    std::string cur = best_start;
    std::set<std::string> emitted;
    while (!cur.empty() && emitted.insert(cur).second) {
        res.critical_path_codes.push_back(by_low[cur]->id_course);
        auto nit = next_hop.find(cur);
        cur = (nit != next_hop.end()) ? nit->second : "";
    }
    return res;
}

Result<std::vector<SmsMessageRecord>> TdaTdsClient::GetCourseSmsMessages(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/one-community?page_num=0&loc=", "", "", "text/html, */*");
    if (!r.ok) return Result<std::vector<SmsMessageRecord>>::Failure(r.error_code, r.error_message);
    auto msgs = ParseSmsTableHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& m : msgs) {
        m.content = san.SanitizeText(m.content);
        m.community = san.SanitizeText(m.community);
        m.sent_by = san.SanitizeText(m.sent_by);
    }
    return Result<std::vector<SmsMessageRecord>>::Success(std::move(msgs));
}

Result<std::vector<NotificationSubscriptionItem>> TdaTdsClient::GetNotificationSubscriptions() {
    auto r = FetchText("GET", "/notifications/manage", "", "", "text/html, */*");
    if (!r.ok) return Result<std::vector<NotificationSubscriptionItem>>::Failure(r.error_code, r.error_message);
    auto subs = ParseNotificationSubscriptionsHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& s : subs) {
        s.community = san.SanitizeText(s.community);
        s.element = san.SanitizeText(s.element);
    }
    return Result<std::vector<NotificationSubscriptionItem>>::Success(std::move(subs));
}

Result<std::vector<ForumThreadSummary>> TdaTdsClient::GetCommunityForums(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/forums/", "", "", "text/html, */*");
    if (!r.ok) {
        if (r.error_code == "http_404") {
            return Result<std::vector<ForumThreadSummary>>::Success({});
        }
        return Result<std::vector<ForumThreadSummary>>::Failure(r.error_code, r.error_message);
    }
    auto threads = ParseForumsHtml(r.value, base_url_);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& th : threads) {
        th.subject = san.SanitizeText(th.subject);
        th.author = san.SanitizeText(th.author);
    }
    return Result<std::vector<ForumThreadSummary>>::Success(std::move(threads));
}

Result<std::vector<ForumMessagePost>> TdaTdsClient::GetForumThread(
    std::string_view community_path,
    std::string_view message_id) {
    if (message_id.empty()) {
        return Result<std::vector<ForumMessagePost>>::Failure("invalid_arg", "message_id required");
    }
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/forums/message-view?message_id=" + UrlEncode(message_id),
                       "", "", "text/html, */*");
    if (!r.ok) return Result<std::vector<ForumMessagePost>>::Failure(r.error_code, r.error_message);
    auto posts = ParseForumThreadMessagesHtml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& p : posts) {
        p.subject = san.SanitizeText(p.subject);
        p.author = san.SanitizeText(p.author);
        p.body_text = san.SanitizeText(p.body_text);
    }
    return Result<std::vector<ForumMessagePost>>::Success(std::move(posts));
}

Result<std::vector<RssNewsItem>> TdaTdsClient::GetNewsArchive(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/news/?view=archive", "", "", "text/html, */*");
    if (!r.ok) {
        if (r.error_code == "http_404") {
            return Result<std::vector<RssNewsItem>>::Success({});
        }
        return Result<std::vector<RssNewsItem>>::Failure(r.error_code, r.error_message);
    }
    std::vector<RssNewsItem> items;
    const Sanitizer san = SnapshotSanitizer();
    static const std::regex kTrRe(R"(<tr\b[^>]*>([\s\S]*?)</tr>)", std::regex::icase);
    static const std::regex kTdRe(R"(<t[dh]\b[^>]*>([\s\S]*?)</t[dh]>)", std::regex::icase);
    std::string body = r.value;
    for (std::sregex_iterator it(body.begin(), body.end(), kTrRe), end; it != end; ++it) {
        std::string row_html = (*it)[1].str();
        std::vector<std::string> cells;
        for (std::sregex_iterator cit(row_html.begin(), row_html.end(), kTdRe); cit != end; ++cit) {
            cells.push_back(StripTags((*cit)[1].str()));
        }
        if (cells.size() >= 2 && AsciiLower(cells[0]).find("fecha") == std::string::npos) {
            RssNewsItem ni;
            ni.pub_date = san.SanitizeText(cells[0]);
            ni.title = san.SanitizeText(cells[1]);
            if (cells.size() >= 3) ni.description = san.SanitizeText(cells[2]);
            if (!ni.title.empty()) items.push_back(std::move(ni));
        }
    }
    return Result<std::vector<RssNewsItem>>::Success(std::move(items));
}

Result<std::vector<SchoolCatalogEntry>> TdaTdsClient::GetSchoolCatalog() {
    auto r = FetchText("GET", "/dotlrn/classes/", "", "", "text/html, */*");
    if (!r.ok) return Result<std::vector<SchoolCatalogEntry>>::Failure(r.error_code, r.error_message);
    auto schools = ParseSchoolCatalogHtml(r.value, base_url_);
    const Sanitizer san = SnapshotSanitizer();
    for (auto& sc : schools) sc.name = san.SanitizeText(sc.name);
    return Result<std::vector<SchoolCatalogEntry>>::Success(std::move(schools));
}

Result<UserModelSnapshot> TdaTdsClient::GetUserModelSnapshot() {
    UserModelSnapshot snap;
    if (auto c = GetConsentStatus("tda-um"); c.ok && !TrimAscii(c.value).empty()) {
        snap.consent_status = TrimAscii(c.value);
    } else if (auto c0 = GetUserTermConditionJson(); c0.ok) {
        snap.consent_status = TrimAscii(c0.value);
    }

    static const char* kSelectors[] = {
        "#personal_info_container",
        "#learning_style_info",
        "#learning_needs_info",
        "#socioacademic_info",
        "#achievement_info",
        "#temperament_info",
        "#entrepreneur_info",
        "#comunication_info"
    };
    std::unordered_set<std::string> seen_dims;
    for (const char* sel : kSelectors) {
        auto r = FetchText("POST", "/tda-um/ajax/get_info",
                           "section_type=" + UrlEncode(sel),
                           "application/x-www-form-urlencoded");
        if (!r.ok) {
            if (r.error_code == "session_expired") {
                return Result<UserModelSnapshot>::Failure(r.error_code, r.error_message);
            }
            continue;
        }
        auto parsed = ParseUserModelInfoJson(r.value);
        for (auto& dim : parsed.dimensions) {
            if (seen_dims.insert(dim.dimension_key).second) {
                snap.dimensions.push_back(std::move(dim));
            }
        }
    }

    if (snap.dimensions.empty()) {
        auto r = FetchText("POST", "/tda-um/ajax/get_info", "", "application/x-www-form-urlencoded");
        if (!r.ok && r.error_code != "session_expired") {
            r = FetchText("GET", "/tda-um/ajax/get_info", "", "");
        }
        if (r.ok) {
            auto parsed = ParseUserModelInfoJson(r.value);
            snap.dimensions = std::move(parsed.dimensions);
        } else if (r.error_code == "session_expired") {
            return Result<UserModelSnapshot>::Failure(r.error_code, r.error_message);
        }
    }
    return Result<UserModelSnapshot>::Success(std::move(snap));
}

Result<PrematriculaSnapshot> TdaTdsClient::GetPrematriculaSnapshot(
    std::string_view pre_enrollment_id,
    std::string_view curriculum_id,
    std::string_view curriculum_id_current,
    std::string_view list_electives) {
    PrematriculaSnapshot snap;

    // 1. tipo=1 (enrolled courses + max credits)
    std::string body1 = "tipo=1&pre_enrollment_id=" + UrlEncode(pre_enrollment_id) +
                        "&curriculum_id=" + UrlEncode(curriculum_id);
    if (!curriculum_id_current.empty()) {
        body1 += "&curriculum_id_current=" + UrlEncode(curriculum_id_current);
    }
    auto r1 = FetchText("POST", "/tda-prematricula/ajax/prematricula_est", body1,
                        "application/x-www-form-urlencoded");
    if (!r1.ok) return Result<PrematriculaSnapshot>::Failure(r1.error_code, r1.error_message);
    snap.courses = ParsePrematriculaTipo1(r1.value, &snap.max_credits, &snap.rn_student);

    // 2. tipo=2 (special requests)
    std::string body2 = "tipo=2&pre_enrollment_id=" + UrlEncode(pre_enrollment_id) +
                        "&curriculum_id=" + UrlEncode(curriculum_id);
    auto r2 = FetchText("POST", "/tda-prematricula/ajax/prematricula_est", body2,
                        "application/x-www-form-urlencoded");
    if (r2.ok) {
        snap.requests = ParsePrematriculaTipo2(r2.value);
    }

    // 3. cursos_deshabilitados
    auto r3 = FetchText("GET", "/tda-prematricula/ajax/cursos_deshabilitados?pre_enrollment_id=" +
                                   UrlEncode(pre_enrollment_id) + "&curriculum_id=" +
                                   UrlEncode(curriculum_id));
    if (r3.ok) {
        snap.disabled_courses = ParseDisabledCourses(r3.value);
    }

    // 4. validate_elective
    if (!list_electives.empty()) {
        auto r4 = FetchText("GET", "/tda-prematricula/ajax/validate_elective?curriculum_id=" +
                                       UrlEncode(curriculum_id) + "&listElectives=" +
                                       UrlEncode(list_electives));
        if (r4.ok) {
            snap.validated_electives_count = std::atoi(TrimAscii(r4.value).c_str());
        }
    }

    // 5. i18n
    auto r5 = FetchText("GET", "/tda-prematricula/ajax/Internationalization?js=principal");
    if (r5.ok) {
        try {
            snap.i18n_labels = ParseFlatStringMap(r5.value);
        } catch (...) {
        }
    }

    SanitizePrematricula(snap, SnapshotSanitizer());
    return Result<PrematriculaSnapshot>::Success(std::move(snap));
}

Result<std::string> TdaTdsClient::GetGestorInscripcionSelectorHtml() {
    auto r = FetchText("GET", "/tda-gestor-inscripcion/estudiante/ajax/refresca_selector",
                       "", "", "text/html, */*");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeHtml(r.value);
    return r;
}

Result<std::string> TdaTdsClient::GetGestorInscripcionEventsJson(std::string_view convocatory_id) {
    std::string path = "/tda-gestor-inscripcion/estudiante/ajax/carga_inscripciones";
    if (!convocatory_id.empty()) {
        path += "?convocatory_id=" + UrlEncode(convocatory_id);
    }
    auto r = FetchText("GET", path);
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeJsonString(r.value);
    return r;
}

Result<std::string> TdaTdsClient::GetInclusionTransactionKey() {
    auto r = FetchText("GET", "/tda-inclusion-manager/api/user-transaction");
    if (!r.ok) {
        if (r.error_code == "transport_error" || r.error_code == "http_404" ||
            r.error_code == "http_502" || r.error_code == "http_503") {
            return Result<std::string>::Failure(
                "service_unavailable",
                "Gestor de Inclusiones no disponible fuera de periodo habilitado");
        }
        return r;
    }
    if (TrimAscii(r.value).empty()) {
        return Result<std::string>::Failure(
            "service_unavailable",
            "Gestor de Inclusiones devolvió respuesta vacía (fuera de periodo)");
    }
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto j = nlohmann::json::parse(r.value);
        return Result<std::string>::Success(san.SanitizeText(JsonAsString(j, "transaction_key")));
    } catch (const std::exception& ex) {
        return Result<std::string>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

Result<CourseEvaluationsSnapshot> TdaTdsClient::GetCourseEvaluations(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    CourseEvaluationsSnapshot snap;
    size_t slash = comm.rfind('/');
    snap.community_slug = (slash != std::string::npos) ? comm.substr(slash + 1) : comm;

    auto r_idx = FetchText("GET", comm + "/evaluation/tda-ce-estudiante/tda-index",
                           "", "", "text/html, */*");
    if (!r_idx.ok && r_idx.error_code == "session_expired") {
        return Result<CourseEvaluationsSnapshot>::Failure(r_idx.error_code, r_idx.error_message);
    }
    if (r_idx.ok) {
        snap.categories = ParseEvaluationsHtml(r_idx.value, &snap.final_grade_text);
    }

    auto r_i18n = FetchText("GET", comm + "/evaluation/tda-ce-estudiante/ajax/Internationalization?js=index");
    if (r_i18n.ok) {
        try {
            snap.i18n_labels = ParseFlatStringMap(r_i18n.value);
        } catch (...) {
        }
    }

    auto r_forms = FetchText("GET", comm + "/tda-performance-evaluation/api/forms/");
    if (r_forms.ok) {
        auto pf = ParsePerformanceFormsJson(r_forms.value);
        if (pf.ok) snap.performance_forms = std::move(pf.value);
    }

    auto r_evals = FetchText("GET", comm + "/tda-performance-evaluation/api/evaluations/");
    if (r_evals.ok) {
        snap.performance_evaluations_raw_json = TrimAscii(r_evals.value);
    }

    SanitizeCourseEvaluations(snap, SnapshotSanitizer());
    return Result<CourseEvaluationsSnapshot>::Success(std::move(snap));
}

Result<PerformanceEvaluationStatus> TdaTdsClient::GetPerformanceEvaluationStatus() {
    PerformanceEvaluationStatus status;
    auto r_forms = FetchText("GET", "/tda-performance-evaluation/api/forms/");
    if (!r_forms.ok && r_forms.error_code == "session_expired") {
        return Result<PerformanceEvaluationStatus>::Failure(r_forms.error_code, r_forms.error_message);
    }
    if (r_forms.ok) {
        auto pf = ParsePerformanceFormsJson(r_forms.value);
        if (pf.ok) status.forms = std::move(pf.value);
    }
    auto r_evals = FetchText("GET", "/tda-performance-evaluation/api/evaluations/");
    if (!r_evals.ok && r_evals.error_code == "session_expired") {
        return Result<PerformanceEvaluationStatus>::Failure(r_evals.error_code, r_evals.error_message);
    }
    if (r_evals.ok) {
        status.pending_evaluations = ParsePerformanceEvaluationsJson(r_evals.value);
    }
    const Sanitizer san = SnapshotSanitizer();
    for (auto& pf : status.forms) {
        pf.name = san.SanitizeText(pf.name);
        pf.instructions = san.SanitizeText(pf.instructions);
        for (auto& sec : pf.sections) {
            sec.name = san.SanitizeText(sec.name);
            sec.description = san.SanitizeText(sec.description);
            for (auto& it : sec.items) it.description = san.SanitizeText(it.description);
        }
    }
    for (auto& ev : status.pending_evaluations) {
        ev.course_name = san.SanitizeText(ev.course_name);
        ev.teacher_name = san.SanitizeText(ev.teacher_name);
        ev.form_name = san.SanitizeText(ev.form_name);
        ev.status = san.SanitizeText(ev.status);
        ev.url = san.SanitizeUrl(ev.url);
    }
    return Result<PerformanceEvaluationStatus>::Success(std::move(status));
}

Result<FeriaLocation> TdaTdsClient::GetFeriaStudentLocation() {
    auto r = FetchText("GET", "/tda-feria-negocios/ajax/registrar_proyectos/getStudentLocation");
    if (!r.ok) return Result<FeriaLocation>::Failure(r.error_code, r.error_message);
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto j = nlohmann::json::parse(TrimAscii(r.value));
        FeriaLocation loc;
        loc.id = FirstStr(j, {"identificador", "id", "location_id"});
        loc.name = FirstStr(j, {"nombre", "name"});
        SanitizeFeriaLocation(loc, san);
        return Result<FeriaLocation>::Success(std::move(loc));
    } catch (const std::exception& ex) {
        return Result<FeriaLocation>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

Result<std::vector<FeriaCategory>> TdaTdsClient::GetFeriaCategories() {
    auto r = FetchText("GET", "/tda-feria-negocios/ajax/categorias/load_categorias");
    if (!r.ok) return Result<std::vector<FeriaCategory>>::Failure(r.error_code, r.error_message);
    const Sanitizer san = SnapshotSanitizer();
    try {
        auto j = nlohmann::json::parse(TrimAscii(r.value));
        const nlohmann::json* arr = j.is_array() ? &j
                                                 : (j.is_object() && j.contains("categorias") && j["categorias"].is_array()
                                                        ? &j["categorias"]
                                                        : nullptr);
        if (arr == nullptr) {
            return Result<std::vector<FeriaCategory>>::Failure("invalid_schema", "categories not array");
        }
        std::vector<FeriaCategory> out;
        for (const auto& item : *arr) {
            if (!item.is_object()) continue;
            FeriaCategory c;
            c.id = JsonAsInt64(item, "idOp");
            if (c.id == 0) c.id = JsonAsInt64(item, "id");
            c.name = FirstStr(item, {"nameOp", "name", "nombre"});
            out.push_back(std::move(c));
        }
        SanitizeFeriaCategories(out, san);
        return Result<std::vector<FeriaCategory>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<FeriaCategory>>::Failure("invalid_json", san.SanitizeText(ex.what()));
    }
}

Result<FeriaIds> TdaTdsClient::GetFeriaIds() {
    auto r = FetchText("GET", "/tda-feria-negocios/ajax/ferias/get_ids");
    if (!r.ok) return Result<FeriaIds>::Failure(r.error_code, r.error_message);
    try {
        auto j = nlohmann::json::parse(TrimAscii(r.value));
        FeriaIds ids;
        ids.user = JsonAsInt64(j, "user");
        ids.community = JsonAsInt64(j, "community");
        ids.pack = JsonAsInt64(j, "pack");
        return Result<FeriaIds>::Success(ids);
    } catch (const std::exception& ex) {
        return Result<FeriaIds>::Failure("invalid_json", SnapshotSanitizer().SanitizeText(ex.what()));
    }
}

Result<FeriaNegociosSnapshot> TdaTdsClient::GetFeriaNegociosSnapshot(std::string_view location_id) {
    FeriaNegociosSnapshot snap;
    auto ri = FetchText("POST", "/tda-feria-negocios/ajax/internationalization", "",
                        "application/x-www-form-urlencoded");
    if (!ri.ok && ri.error_code == "session_expired") {
        return Result<FeriaNegociosSnapshot>::Failure(ri.error_code, ri.error_message);
    }
    if (ri.ok) {
        try {
            snap.i18n_labels = ParseFlatStringMap(ri.value);
        } catch (...) {
        }
    }
    if (auto r = FetchText("GET", "/tda-feria-negocios/ajax/registrar_proyectos/getInfoEstudiante"); r.ok) {
        snap.info_estudiante_raw_json = TrimAscii(r.value);
    }
    if (auto r = GetFeriaStudentLocation(); r.ok) {
        snap.student_location = std::move(r.value);
    }
    std::string loc = location_id.empty() ? snap.student_location.id : std::string(location_id);
    if (!loc.empty()) {
        if (auto r = FetchText("GET", "/tda-feria-negocios/ajax/registrar_proyectos/getActiveFairsByLocation?location_id=" +
                                          UrlEncode(loc)); r.ok) {
            snap.active_fairs_raw_json = TrimAscii(r.value);
        }
    }
    if (auto r = GetFeriaCategories(); r.ok) {
        snap.categories = std::move(r.value);
    }
    if (auto r = GetFeriaIds(); r.ok) {
        snap.ids = r.value;
    }
    SanitizeFeriaSnapshot(snap, SnapshotSanitizer());
    return Result<FeriaNegociosSnapshot>::Success(std::move(snap));
}

Result<int> TdaTdsClient::GetUnreadNotificationCount() {
    auto r = FetchText("GET", "/tda-notifications/ajax/has_unread_notifications");
    if (!r.ok) return Result<int>::Failure(r.error_code, r.error_message);
    try {
        auto j = nlohmann::json::parse(r.value);
        return Result<int>::Success(static_cast<int>(JsonAsInt64(j, "quantity")));
    } catch (const std::exception& ex) {
        return Result<int>::Failure("invalid_json", SnapshotSanitizer().SanitizeText(ex.what()));
    }
}

Result<NotificationsFeed> TdaTdsClient::GetUserNotifications() {
    auto r = FetchText("GET", "/tda-notifications/ajax/get_user_notifications");
    if (!r.ok) return Result<NotificationsFeed>::Failure(r.error_code, r.error_message);
    auto parsed = ParseNotificationsJson(r.value);
    if (!parsed.ok) {
        parsed.error_message = SnapshotSanitizer().SanitizeText(parsed.error_message);
        return parsed;
    }
    auto cnt = GetUnreadNotificationCount();
    if (cnt.ok) parsed.value.unread_quantity = cnt.value;
    SanitizeNotifications(parsed.value, SnapshotSanitizer());
    return parsed;
}

Result<std::vector<RssNewsItem>> TdaTdsClient::GetCommunityNewsRss(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/news/rss/rss.xml", "", "", "application/xml, text/xml, */*");
    if (!r.ok) return Result<std::vector<RssNewsItem>>::Failure(r.error_code, r.error_message);
    auto parsed = ParseRssXml(r.value);
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeRssItems(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<std::vector<CommunityMember>> TdaTdsClient::GetCommunityMembers(
    std::string_view community_path,
    std::string_view community_id,
    int length) {
    std::string comm = NormalizeCommPath(community_path);
    std::string body = "draw=1&start=0&length=" + std::to_string(length) +
                       "&search=&community_id=" + UrlEncode(community_id) +
                       "&recordsTotal=0&orderCol=2&orderType=asc";
    auto r = FetchText("POST", comm + "/ajax/load_members", body,
                       "application/x-www-form-urlencoded");
    Result<std::vector<CommunityMember>> parsed =
        r.ok ? ParseMembersJson(r.value)
             : Result<std::vector<CommunityMember>>::Failure(r.error_code, r.error_message);
    if (!parsed.ok || parsed.value.empty()) {
        auto r_html = FetchText("GET", comm + "/members", "", "", "text/html, */*");
        if (r_html.ok) {
            std::vector<CommunityMember> fallback_list;
            static const std::regex kTrRe(R"(<tr\b[^>]*>([\s\S]*?)</tr>)", std::regex::icase);
            static const std::regex kTdRe(R"(<t[dh]\b[^>]*>([\s\S]*?)</t[dh]>)", std::regex::icase);
            std::string h = r_html.value;
            for (std::sregex_iterator it(h.begin(), h.end(), kTrRe), end; it != end; ++it) {
                std::string row_html = (*it)[1].str();
                std::vector<std::string> cells;
                for (std::sregex_iterator cit(row_html.begin(), row_html.end(), kTdRe); cit != end; ++cit) {
                    cells.push_back(StripTags((*cit)[1].str()));
                }
                if (cells.size() >= 3) {
                    std::string c0 = AsciiLower(cells[0]);
                    if (c0 == "apellidos" || c0 == "last name" || c0.find("no hay") != std::string::npos) continue;
                    CommunityMember cm;
                    cm.last_name = TrimAscii(cells[0]);
                    cm.first_name = TrimAscii(cells[1]);
                    cm.full_name = TrimAscii(cm.first_name + " " + cm.last_name);
                    if (cells.size() >= 3) cm.email = TrimAscii(cells[2]);
                    if (cells.size() >= 4) cm.role = TrimAscii(cells[3]);
                    if (!cm.full_name.empty() || !cm.email.empty()) {
                        fallback_list.push_back(std::move(cm));
                    }
                }
            }
            if (!fallback_list.empty()) {
                parsed = Result<std::vector<CommunityMember>>::Success(std::move(fallback_list));
            }
        }
    }
    const Sanitizer san = SnapshotSanitizer();
    if (parsed.ok) SanitizeMembers(parsed.value, san);
    else parsed.error_message = san.SanitizeText(parsed.error_message);
    return parsed;
}

Result<std::string> TdaTdsClient::GetCommunityMembersCsv(std::string_view community_path) {
    std::string comm = NormalizeCommPath(community_path);
    auto r = FetchText("GET", comm + "/members?csv=yes", "", "", "text/csv, text/plain, */*");
    if (!r.ok) return r;
    r.value = SnapshotSanitizer().SanitizeText(r.value);
    return r;
}

Result<TdaTdsCatalog> TdaTdsClient::FetchCatalog(const TdaTdsCatalogOptions& opts) {
    TdaTdsCatalog cat;
    std::string plan = opts.id_plan;

    if (auto r = GetServerDateMs(); r.ok) {
        cat.server_epoch_ms = r.value;
    } else if (r.error_code == "session_expired") {
        return Result<TdaTdsCatalog>::Failure(r.error_code, r.error_message);
    } else {
        cat.warnings.push_back(SnapshotSanitizer().SanitizeText("get_date_server: " + r.error_message));
    }

    if (auto r = GetGaapServer(); r.ok) {
        cat.gaap_server = std::move(r.value);
    }

    if (opts.fetch_curriculum) {
        if (auto r = GetCurriculumSedes(); r.ok) {
            cat.sedes = std::move(r.value);
        } else if (r.error_code == "session_expired") {
            return Result<TdaTdsCatalog>::Failure(r.error_code, r.error_message);
        } else {
            cat.warnings.push_back(SnapshotSanitizer().SanitizeText("carga_sedes_json: " + r.error_message));
        }

        if (auto r = GetCurriculumColorCodes(); r.ok) {
            cat.color_codes = std::move(r.value);
        }
        if (auto r = GetCurriculumAppCode(); r.ok) {
            cat.curriculum_app_code = r.value;
        }
        if (auto r = ValidateModificationPermissions(opts.carne); r.ok) {
            cat.can_modify_curriculum = r.value;
        }
        if (auto r = GetStudentCareerData(opts.carne); r.ok) {
            cat.student_career = std::move(r.value);
        }

        std::string sede = opts.id_sede;
        if (sede.empty() && !cat.sedes.empty()) sede = cat.sedes.front().key;
        if (!sede.empty()) {
            if (auto r = GetCurriculumCarreras(sede); r.ok) {
                cat.carreras = std::move(r.value);
            }
        }
        std::string depto = opts.id_depto;
        if (depto.empty() && !cat.carreras.empty()) depto = cat.carreras.front().key;
        if (!sede.empty() && !depto.empty()) {
            if (auto r = GetCurriculumPlanes(sede, depto); r.ok) {
                cat.planes = std::move(r.value);
            }
        }

        if (plan.empty() && cat.student_career.has_value() && cat.student_career->plan > 0) {
            plan = std::to_string(cat.student_career->plan);
        }
        if (plan.empty() && !cat.planes.empty()) {
            plan = cat.planes.front().key;
        }
        if (!plan.empty()) {
            if (auto r = GetCurriculumGraph(plan); r.ok) {
                cat.curriculum_graph = std::move(r.value);
            }
            if (auto r = GetStudentCurriculumProfile(plan, opts.carne, 0); r.ok) {
                cat.student_profile = std::move(r.value);
            }
        }
    }

    if (opts.fetch_expediente) {
        if (auto r = GetExpedienteSummary(opts.carne); r.ok) {
            cat.expediente = std::move(r.value);
        } else if (r.error_code == "session_expired") {
            return Result<TdaTdsCatalog>::Failure(r.error_code, r.error_message);
        } else {
            cat.warnings.push_back(SnapshotSanitizer().SanitizeText("expediente: " + r.error_message));
        }
    }

    if (opts.fetch_notifications) {
        if (auto r = GetUserNotifications(); r.ok) {
            cat.notifications = std::move(r.value);
        } else if (r.error_code == "session_expired") {
            return Result<TdaTdsCatalog>::Failure(r.error_code, r.error_message);
        } else {
            cat.warnings.push_back(SnapshotSanitizer().SanitizeText("notifications: " + r.error_message));
        }
    }

    if (opts.fetch_inclusion_key) {
        if (auto r = GetInclusionTransactionKey(); r.ok) {
            cat.inclusion_transaction_key = std::move(r.value);
        }
    }

    if (opts.fetch_prematricula && !opts.pre_enrollment_id.empty() && !plan.empty()) {
        if (auto r = GetPrematriculaSnapshot(opts.pre_enrollment_id, plan,
                                             opts.curriculum_id_current, opts.list_electives); r.ok) {
            cat.prematricula = std::move(r.value);
        }
    }

    if (opts.fetch_gestor_inscripcion) {
        if (auto r = GetGestorInscripcionSelectorHtml(); r.ok) {
            cat.gestor_inscripcion_selector_html = std::move(r.value);
        }
        if (auto r = GetGestorInscripcionEventsJson(opts.convocatory_id); r.ok) {
            cat.gestor_inscripcion_events_json = std::move(r.value);
        }
    }

    if (opts.fetch_feria_negocios) {
        if (auto r = GetFeriaNegociosSnapshot(); r.ok) {
            cat.feria_negocios = std::move(r.value);
        }
    }

    for (const auto& comm_path : opts.community_paths) {
        std::string norm = NormalizeCommPath(comm_path);
        if (opts.fetch_evaluations) {
            if (auto r = GetCourseEvaluations(norm); r.ok) {
                cat.evaluations.push_back(std::move(r.value));
            } else if (r.error_code == "session_expired") {
                return Result<TdaTdsCatalog>::Failure(r.error_code, r.error_message);
            }
        }
        if (opts.fetch_news_rss) {
            if (auto r = GetCommunityNewsRss(norm); r.ok) {
                cat.news_rss_by_community[norm] = std::move(r.value);
            }
        }
        if (opts.fetch_members) {
            if (auto r = GetCommunityMembers(norm); r.ok) {
                cat.members_by_community[norm] = std::move(r.value);
            }
        }
    }

    return Result<TdaTdsCatalog>::Success(std::move(cat));
}

}  // namespace academic::tec_digital
