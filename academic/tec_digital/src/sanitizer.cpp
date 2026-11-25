#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/types.hpp"
#include "http_common.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <unordered_set>

namespace academic::tec_digital {

namespace {

bool IsHexStr(std::string_view s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (!((c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}

bool IsCompleteRedactedShareToken(std::string_view t) {
    static constexpr std::string_view kSharePrefix = "[REDACTED_SHARE_";
    return t.size() == kSharePrefix.size() + 12 + 1 &&
           t.compare(0, kSharePrefix.size(), kSharePrefix) == 0 &&
           t.back() == ']' &&
           IsHexStr(t.substr(kSharePrefix.size(), 12));
}

bool IsCompleteRedactedToken(std::string_view t) {
    if (t == "[REDACTED]" || t == "[REDACTED_EMAIL]" ||
        t == "[REDACTED_PII]" || t == "[REDACTED_JWT]") {
        return true;
    }
    static constexpr std::string_view kShaPrefix = "[REDACTED:sha256=";
    if (t.size() == kShaPrefix.size() + 8 + 1 &&
        t.compare(0, kShaPrefix.size(), kShaPrefix) == 0 &&
        t.back() == ']' &&
        IsHexStr(t.substr(kShaPrefix.size(), 8))) {
        return true;
    }
    return IsCompleteRedactedShareToken(t);
}

bool IsAlreadyRedacted(std::string_view val) {
    std::string trimmed = TrimAscii(val);
    if (trimmed.empty()) return false;
    if (IsCompleteRedactedToken(trimmed)) return true;
    if (trimmed == "[EMPTY]" || trimmed == "[FILTERED]" || trimmed == "REDACTED") return true;
    return false;
}

bool IsHiddenPlaceholder(std::string_view val) {
    std::string t = TrimAscii(val);
    if (t.empty()) return true;
    if (t == "null" || t == "undefined" || t == "REDACTED" || t == "[FILTERED]" || t == "[EMPTY]") {
        return true;
    }
    if (IsCompleteRedactedToken(t)) return true;
    bool all_stars = true;
    for (char c : t) {
        if (c != '*') {
            all_stars = false;
            break;
        }
    }
    if (all_stars) return true;
    if (t.size() >= 4 && t.substr(0, 2) == "{{" && t.substr(t.size() - 2) == "}}") return true;
    if (t.size() >= 3 && t.substr(0, 2) == "${" && t.back() == '}') return true;
    return false;
}

bool IsSensitiveHeaderName(std::string_view name) {
    std::string low = AsciiLower(TrimAscii(name));
    static const std::unordered_set<std::string> kExact = {
        "cookie", "set-cookie", "authorization", "proxy-authorization",
        "x-csrf-token", "x-csrftoken", "csrf-token", "x-csrf-header",
        "x-auth-token", "x-session", "x-secret", "secret", "api-key", "x-api-key"
    };
    if (kExact.count(low) > 0) return true;
    static const std::regex kSecretKeyRe(
        R"((password|passwd|passphrase|authorization|proxy-authorization|cookie|set-cookie|csrf|xsrf|token|session(?:[_-]?id)?|secret|api[-_]?key))",
        std::regex::ECMAScript | std::regex::icase);
    return std::regex_search(low, kSecretKeyRe);
}

bool IsSensitiveParamName(std::string_view name) {
    std::string low = AsciiLower(TrimAscii(name));
    static const std::unordered_set<std::string> kSensitive = {
        "password", "passwd", "pass", "passphrase", "pwd", "token", "retoken",
        "secret", "key", "api_key", "api-key", "session", "sessionid",
        "session_id", "session-id", "sid", "auth", "authorization", "cookie",
        "csrf", "csrf_token", "xsrf", "xsrf_token", "hash_key", "signature",
        "sig", "access_token", "refresh_token", "id_token"
    };
    return kSensitive.count(low) > 0;
}

bool IsSecretJsonKey(std::string_view key) {
    static const std::regex kJsonSecretRe(
        R"((password|passwd|pwd|passphrase|authorization|cookie|set-cookie|csrf|xsrf|token|retoken|session(?:[_-]?id)?|secret|api[-_]?key|userpassword|ad_session_id|ad_user_login))",
        std::regex::ECMAScript | std::regex::icase);
    return std::regex_search(std::string(key), kJsonSecretRe);
}

bool IsPiiJsonKey(std::string_view key) {
    std::string low = AsciiLower(key);
    static const std::unordered_set<std::string> kPiiKeys = {
        "firstname", "lastname", "emailcol", "nombre", "apellidos", "correo",
        "email", "carne", "carneoficial", "studentid", "useremail"
    };
    return kPiiKeys.count(low) > 0;
}

std::string UrlEncodeUpper(std::string_view sv) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
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

std::string UrlEncodeLower(std::string_view sv) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
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

std::string UrlEncodeFormUpper(std::string_view sv) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : sv) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string UrlEncodeFormLower(std::string_view sv) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    for (unsigned char c : sv) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else if (c == ' ') {
            out.push_back('+');
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

std::string AsciiUpperStr(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string JsonEscapeString(std::string_view sv) {
    std::string dumped = nlohmann::json(std::string(sv)).dump();
    if (dumped.size() >= 2 && dumped.front() == '"' && dumped.back() == '"') {
        return dumped.substr(1, dumped.size() - 2);
    }
    return dumped;
}

struct RuntimeNeedles {
    std::vector<std::string> passwords;
    std::vector<std::string> emails;
    std::vector<std::string> piis;
};

RuntimeNeedles BuildRuntimeNeedles(const SanitizeOptions& opts, bool include_email_and_pii) {
    RuntimeNeedles rn;
    auto push_unique = [](std::vector<std::string>& dst, std::string val) {
        if (val.size() < 4) return;
        if (std::find(dst.begin(), dst.end(), val) == dst.end()) {
            dst.push_back(std::move(val));
        }
    };
    auto sort_desc = [](std::vector<std::string>& dst) {
        std::stable_sort(dst.begin(), dst.end(),
                         [](const std::string& a, const std::string& b) {
                             return a.size() > b.size();
                         });
    };

    if (opts.runtime_password.size() >= 4) {
        push_unique(rn.passwords, opts.runtime_password);
        push_unique(rn.passwords, UrlEncodeUpper(opts.runtime_password));
        push_unique(rn.passwords, UrlEncodeLower(opts.runtime_password));
        push_unique(rn.passwords, UrlEncodeFormUpper(opts.runtime_password));
        push_unique(rn.passwords, UrlEncodeFormLower(opts.runtime_password));
        push_unique(rn.passwords, JsonEscapeString(opts.runtime_password));
        sort_desc(rn.passwords);
    }

    if (include_email_and_pii && opts.runtime_email.size() >= 4) {
        std::string low = AsciiLower(opts.runtime_email);
        std::string up = AsciiUpperStr(opts.runtime_email);
        for (const std::string& base : {opts.runtime_email, low, up}) {
            push_unique(rn.emails, base);
            push_unique(rn.emails, UrlEncodeUpper(base));
            push_unique(rn.emails, UrlEncodeLower(base));
            push_unique(rn.emails, UrlEncodeFormUpper(base));
            push_unique(rn.emails, UrlEncodeFormLower(base));
        }
        sort_desc(rn.emails);
    }

    if (include_email_and_pii) {
        for (const auto& pii : opts.pii_needles) {
            push_unique(rn.piis, pii);
        }
        sort_desc(rn.piis);
    }
    return rn;
}

void ReplaceAllExact(std::string& s, std::string_view needle, std::string_view repl) {
    if (needle.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(needle, pos)) != std::string::npos) {
        s.replace(pos, needle.size(), repl);
        pos += repl.size();
    }
}

std::string RedactShareLinksInText(std::string_view input) {
    std::string out(input);
    // SharePoint: https://tenant.sharepoint.com/:w:/g/... -> https://tenant.sharepoint.com/[REDACTED_SHARE_<12>]
    {
        static const std::regex kSharePointRe(
            R"((https?://[a-z0-9.-]+\.sharepoint\.com)/:[a-z]:/[gsr]/[^\s"'<>]+)",
            std::regex::ECMAScript | std::regex::icase);
        std::string rebuilt;
        rebuilt.reserve(out.size());
        std::sregex_iterator it(out.begin(), out.end(), kSharePointRe);
        std::sregex_iterator end;
        size_t last = 0;
        for (; it != end; ++it) {
            const auto& m = *it;
            rebuilt.append(out, last, static_cast<size_t>(m.position()) - last);
            std::string full = m.str(0);
            std::string host = m.str(1);
            std::string h12 = Sha256Hex(full).substr(0, 12);
            rebuilt += host + "/[REDACTED_SHARE_" + h12 + "]";
            last = static_cast<size_t>(m.position() + m.length());
        }
        rebuilt.append(out, last, out.size() - last);
        out = std::move(rebuilt);
    }
    // OneDrive short: https://1drv.ms/... -> https://1drv.ms/[REDACTED_SHARE_<12>]
    {
        static const std::regex kOneDriveRe(
            R"((https?://1drv\.ms)/([^\s"'<>]+))",
            std::regex::ECMAScript | std::regex::icase);
        std::string rebuilt;
        rebuilt.reserve(out.size());
        std::sregex_iterator it(out.begin(), out.end(), kOneDriveRe);
        std::sregex_iterator end;
        size_t last = 0;
        for (; it != end; ++it) {
            const auto& m = *it;
            rebuilt.append(out, last, static_cast<size_t>(m.position()) - last);
            std::string tail = m.str(2);
            if (IsCompleteRedactedShareToken(tail)) {
                rebuilt += m.str(0);
            } else {
                std::string full = m.str(0);
                std::string host = m.str(1);
                std::string h12 = Sha256Hex(full).substr(0, 12);
                rebuilt += host + "/[REDACTED_SHARE_" + h12 + "]";
            }
            last = static_cast<size_t>(m.position() + m.length());
        }
        rebuilt.append(out, last, out.size() - last);
        out = std::move(rebuilt);
    }
    return out;
}

bool IsHtmlSecretIdentity(std::string_view identity) {
    std::string low = AsciiLower(identity);
    static const char* kKeys[] = {
        "password", "passwd", "pwd", "csrf", "xsrf", "token", "retoken",
        "secret", "session", "authorization", "auth_key", "auth-key",
        "api_key", "api-key", "__viewstate", "__viewstategenerator", "__eventvalidation"
    };
    for (const char* k : kKeys) {
        if (low.find(k) != std::string::npos) return true;
    }
    return false;
}

// Extract attribute value from tag text (simple case-insensitive attribute lookup)
bool ExtractAttr(std::string_view tag, std::string_view attr_name,
                 size_t& val_start, size_t& val_len) {
    std::string low_tag = AsciiLower(tag);
    std::string target = AsciiLower(attr_name);
    size_t pos = 0;
    while (pos < low_tag.size()) {
        size_t found = low_tag.find(target, pos);
        if (found == std::string::npos) return false;
        if (found > 0 && (std::isalnum(static_cast<unsigned char>(low_tag[found - 1])) ||
                          low_tag[found - 1] == '-' || low_tag[found - 1] == '_')) {
            pos = found + target.size();
            continue;
        }
        size_t after = found + target.size();
        while (after < low_tag.size() && std::isspace(static_cast<unsigned char>(low_tag[after]))) {
            ++after;
        }
        if (after >= low_tag.size() || low_tag[after] != '=') {
            pos = found + target.size();
            continue;
        }
        ++after;
        while (after < low_tag.size() && std::isspace(static_cast<unsigned char>(low_tag[after]))) {
            ++after;
        }
        if (after >= low_tag.size()) return false;
        if (low_tag[after] == '"' || low_tag[after] == '\'') {
            char q = low_tag[after];
            ++after;
            size_t end_q = tag.find(q, after);
            if (end_q == std::string_view::npos) return false;
            val_start = after;
            val_len = end_q - after;
            return true;
        } else {
            size_t end_u = after;
            while (end_u < tag.size() && !std::isspace(static_cast<unsigned char>(tag[end_u])) &&
                   tag[end_u] != '>') {
                ++end_u;
            }
            val_start = after;
            val_len = end_u - after;
            return true;
        }
    }
    return false;
}

}  // namespace

Sanitizer::Sanitizer(SanitizeOptions opts) : opts_(std::move(opts)) {
    detail::WarmupRegexLocale();
}

std::string Sanitizer::RedactTokenValue(std::string_view val) const {
    if (IsAlreadyRedacted(val)) {
        return std::string(val);
    }
    if (val.empty()) {
        return "[EMPTY]";
    }
    if (opts_.include_sha256_prefix) {
        return "[REDACTED:sha256=" + Sha256Hex(val).substr(0, 8) + "]";
    }
    return "[REDACTED]";
}

std::string Sanitizer::ScrubRuntimeNeedles(std::string_view input) const {
    std::string out(input);
    RuntimeNeedles rn = BuildRuntimeNeedles(opts_, opts_.shareable_mode);
    for (const auto& p : rn.passwords) {
        ReplaceAllExact(out, p, "[REDACTED]");
    }
    for (const auto& e : rn.emails) {
        ReplaceAllExact(out, e, "[REDACTED_EMAIL]");
    }
    for (const auto& pii : rn.piis) {
        ReplaceAllExact(out, pii, "[REDACTED_PII]");
    }
    return out;
}

std::string Sanitizer::SanitizeCookieHeader(std::string_view cookie_header_val) const {
    std::string out;
    size_t pos = 0;
    bool first = true;
    while (pos <= cookie_header_val.size()) {
        size_t semi = cookie_header_val.find(';', pos);
        if (semi == std::string_view::npos) semi = cookie_header_val.size();
        std::string seg = TrimAscii(cookie_header_val.substr(pos, semi - pos));
        if (!seg.empty()) {
            if (!first) out += "; ";
            first = false;
            size_t eq = seg.find('=');
            if (eq == std::string::npos) {
                out += seg;
            } else {
                std::string cname = TrimAscii(seg.substr(0, eq));
                std::string cval = TrimAscii(seg.substr(eq + 1));
                out += cname + "=" + RedactTokenValue(cval);
            }
        }
        if (semi == cookie_header_val.size()) break;
        pos = semi + 1;
    }
    return out;
}

std::string Sanitizer::SanitizeSetCookieHeader(std::string_view set_cookie_val) const {
    std::vector<std::string_view> parts;
    size_t pos = 0;
    while (pos <= set_cookie_val.size()) {
        size_t semi = set_cookie_val.find(';', pos);
        if (semi == std::string_view::npos) {
            parts.push_back(set_cookie_val.substr(pos));
            break;
        }
        parts.push_back(set_cookie_val.substr(pos, semi - pos));
        pos = semi + 1;
    }
    if (parts.empty()) return {};

    std::string out;
    std::string first_seg = TrimAscii(parts[0]);
    size_t eq = first_seg.find('=');
    if (eq == std::string::npos) {
        out = first_seg;
    } else {
        std::string cname = TrimAscii(first_seg.substr(0, eq));
        std::string cval = TrimAscii(first_seg.substr(eq + 1));
        out = cname + "=" + RedactTokenValue(cval);
    }

    for (size_t i = 1; i < parts.size(); ++i) {
        std::string attr = TrimAscii(parts[i]);
        if (attr.empty()) continue;
        size_t aeq = attr.find('=');
        std::string k_low = AsciiLower(TrimAscii(aeq == std::string::npos ? attr : attr.substr(0, aeq)));
        out += "; ";
        if (aeq == std::string::npos) {
            if (k_low == "httponly") out += "HttpOnly";
            else if (k_low == "secure") out += "Secure";
            else if (k_low == "discard") out += "Discard";
            else out += attr;
        } else {
            out += attr;
        }
    }
    return out;
}

std::string Sanitizer::SanitizeHeaderValue(std::string_view header_name,
                                           std::string_view header_value) const {
    std::string low = AsciiLower(TrimAscii(header_name));
    if (low == "cookie") {
        return SanitizeCookieHeader(header_value);
    }
    if (low == "set-cookie") {
        return SanitizeSetCookieHeader(header_value);
    }
    if (low == "authorization" || low == "proxy-authorization") {
        std::string trimmed = TrimAscii(header_value);
        if (trimmed.size() > 7 && AsciiLower(trimmed.substr(0, 7)) == "bearer ") {
            std::string tok = TrimAscii(trimmed.substr(7));
            if (IsAlreadyRedacted(tok)) return trimmed;
            return "Bearer " + RedactTokenValue(tok);
        }
        return RedactTokenValue(trimmed);
    }
    if (IsSensitiveHeaderName(low)) {
        return RedactTokenValue(TrimAscii(header_value));
    }
    return SanitizeText(header_value);
}

std::vector<std::pair<std::string, std::string>> Sanitizer::SanitizeHeaders(
    const std::vector<std::pair<std::string, std::string>>& headers) const {
    std::vector<std::pair<std::string, std::string>> out;
    out.reserve(headers.size());
    for (const auto& kv : headers) {
        out.emplace_back(kv.first, SanitizeHeaderValue(kv.first, kv.second));
    }
    return out;
}

std::string Sanitizer::SanitizeUrl(std::string_view url) const {
    std::string s = RedactShareLinksInText(url);

    auto sanitize_kv_segment = [&](std::string_view query) -> std::string {
        std::string rebuilt_q;
        size_t pos = 0;
        bool first = true;
        while (pos <= query.size()) {
            size_t amp = query.find('&', pos);
            if (amp == std::string_view::npos) amp = query.size();
            std::string item(query.substr(pos, amp - pos));
            if (!first) rebuilt_q.push_back('&');
            first = false;
            size_t eq = item.find('=');
            if (eq == std::string::npos) {
                rebuilt_q += SanitizeText(item);
            } else {
                std::string k = item.substr(0, eq);
                std::string v = item.substr(eq + 1);
                if (IsSensitiveParamName(k)) {
                    if (IsAlreadyRedacted(v)) {
                        rebuilt_q += k + "=" + v;
                    } else {
                        rebuilt_q += k + "=" + RedactTokenValue(v);
                    }
                } else {
                    rebuilt_q += k + "=" + SanitizeText(v);
                }
            }
            if (amp == query.size()) break;
            pos = amp + 1;
        }
        return rebuilt_q;
    };

    // Split fragment
    std::string frag;
    size_t hash_pos = s.find('#');
    if (hash_pos != std::string::npos) {
        frag = s.substr(hash_pos);
        s = s.substr(0, hash_pos);
    }

    size_t q_pos = s.find('?');
    if (q_pos != std::string::npos) {
        std::string prefix = s.substr(0, q_pos + 1);
        std::string query = s.substr(q_pos + 1);
        s = prefix + sanitize_kv_segment(query);
    }

    if (!frag.empty()) {
        // frag starts with '#'
        std::string frag_body = frag.substr(1);
        size_t fq_pos = frag_body.find('?');
        if (fq_pos != std::string::npos) {
            std::string fprefix = SanitizeText(frag_body.substr(0, fq_pos + 1));
            std::string fquery = sanitize_kv_segment(frag_body.substr(fq_pos + 1));
            frag = "#" + fprefix + fquery;
        } else if (frag_body.find('=') != std::string::npos) {
            size_t first_eq = frag_body.find('=');
            size_t slash = frag_body.rfind('/', first_eq);
            if (slash != std::string::npos) {
                std::string fprefix = SanitizeText(frag_body.substr(0, slash + 1));
                std::string fquery = sanitize_kv_segment(frag_body.substr(slash + 1));
                frag = "#" + fprefix + fquery;
            } else {
                frag = "#" + sanitize_kv_segment(frag_body);
            }
        } else {
            frag = "#" + SanitizeText(frag_body);
        }
    }

    s += frag;
    return ScrubRuntimeNeedles(s);
}

std::string Sanitizer::SanitizeText(std::string_view text) const {
    std::string out(text);

    // 0. Scrub exact runtime needles BEFORE regex redactions so passwords containing
    // whitespace, quotes, or punctuation are scrubbed intact without leaking suffixes.
    out = ScrubRuntimeNeedles(out);

    // 1. SharePoint / 1drv.ms links
    out = RedactShareLinksInText(out);

    // 2. Diagnostic HTTP header lines (Cookie:, Set-Cookie:, Authorization:, Proxy-Authorization:)
    {
        static const std::regex kHdrLineRe(
            R"((\b(?:cookie|set-cookie|authorization|proxy-authorization)\s*:\s*)[^\r\n]*)",
            std::regex::ECMAScript | std::regex::icase);
        std::string rebuilt;
        rebuilt.reserve(out.size());
        std::sregex_iterator it(out.begin(), out.end(), kHdrLineRe);
        std::sregex_iterator end;
        size_t last = 0;
        for (; it != end; ++it) {
            const auto& m = *it;
            rebuilt.append(out, last, static_cast<size_t>(m.position()) - last);
            std::string full = m.str(0);
            size_t colon = full.find(':');
            std::string hname = full.substr(0, colon);
            std::string hval = full.substr(colon + 1);
            rebuilt += m.str(1) + SanitizeHeaderValue(hname, hval);
            last = static_cast<size_t>(m.position() + m.length());
        }
        rebuilt.append(out, last, out.size() - last);
        out = std::move(rebuilt);
    }

    // 3. Bearer tokens
    {
        static const std::regex kBearerRe(
            R"((\bBearer\s+)([^\s,;"'<>]+))",
            std::regex::ECMAScript | std::regex::icase);
        std::string rebuilt;
        rebuilt.reserve(out.size());
        std::sregex_iterator it(out.begin(), out.end(), kBearerRe);
        std::sregex_iterator end;
        size_t last = 0;
        for (; it != end; ++it) {
            const auto& m = *it;
            rebuilt.append(out, last, static_cast<size_t>(m.position()) - last);
            std::string tok = m.str(2);
            if (IsAlreadyRedacted(tok)) {
                rebuilt += m.str(0);
            } else {
                rebuilt += m.str(1) + "[REDACTED]";
            }
            last = static_cast<size_t>(m.position() + m.length());
        }
        rebuilt.append(out, last, out.size() - last);
        out = std::move(rebuilt);
    }

    // 4. Standalone JWTs
    {
        static const std::regex kJwtRe(
            R"(\beyJ[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\.[A-Za-z0-9_-]{8,}\b)",
            std::regex::ECMAScript);
        out = std::regex_replace(out, kJwtRe, "[REDACTED_JWT]");
    }

    // 5. Query parameters inside inline URLs (incl. Zoom ?pwd=)
    {
        static const std::regex kQueryParamRe(
            R"(([?&](?:password|passwd|pass|passphrase|pwd|authorization|cookie|csrf|csrf_token|xsrf|xsrf_token|token|retoken|access_token|refresh_token|id_token|session|sessionid|session_id|session-id|sid|auth|secret|key|api_key|api-key|hash_key|signature|sig)=)([^&#\s"'<>]+))",
            std::regex::ECMAScript | std::regex::icase);
        std::string rebuilt;
        rebuilt.reserve(out.size());
        std::sregex_iterator it(out.begin(), out.end(), kQueryParamRe);
        std::sregex_iterator end;
        size_t last = 0;
        for (; it != end; ++it) {
            const auto& m = *it;
            rebuilt.append(out, last, static_cast<size_t>(m.position()) - last);
            std::string val = m.str(2);
            if (IsAlreadyRedacted(val)) {
                rebuilt += m.str(0);
            } else {
                rebuilt += m.str(1) + RedactTokenValue(val);
            }
            last = static_cast<size_t>(m.position() + m.length());
        }
        rebuilt.append(out, last, out.size() - last);
        out = std::move(rebuilt);
    }

    // 6. Inline key=value / key:value secret assignments (quoted values matched before unquoted tokens)
    {
        static const std::regex kInlineSecretRe(
            R"re(((?:["'](?:password|passwd|pwd|passphrase|authorization|cookie|csrf(?:[_-]?token)?|xsrf(?:[_-]?token)?|token|retoken|access_token|refresh_token|id_token|session(?:[_-]?(?:id|token))?|secret|api[-_]?key|userPassword|ad_session_id|ad_user_login)["']|\b(?:password|passwd|pwd|passphrase|authorization|cookie|csrf(?:[_-]?token)?|xsrf(?:[_-]?token)?|token|retoken|access_token|refresh_token|id_token|session(?:[_-]?(?:id|token))?|secret|api[-_]?key|userPassword|ad_session_id|ad_user_login))\s*[:=]\s*)(?:"((?:\\.|[^"\\\r\n])*)"|'((?:\\.|[^'\\\r\n])*)'|`([^`\r\n]*)`|([^\s"'`,;}&<>]+)))re",
            std::regex::ECMAScript | std::regex::icase);

        auto extract_key_lower = [](std::string_view prefix) {
            size_t end = prefix.size();
            while (end > 0 && (prefix[end - 1] == '=' || prefix[end - 1] == ':' ||
                               std::isspace(static_cast<unsigned char>(prefix[end - 1])))) {
                --end;
            }
            std::string_view k = prefix.substr(0, end);
            if (k.size() >= 2 &&
                ((k.front() == '"' && k.back() == '"') ||
                 (k.front() == '\'' && k.back() == '\''))) {
                k = k.substr(1, k.size() - 2);
            }
            return AsciiLower(k);
        };

        auto is_redacted_cookie_assign = [](std::string_view v) {
            size_t eq = v.find('=');
            if (eq == std::string_view::npos || eq == 0) return false;
            return IsAlreadyRedacted(v.substr(eq + 1));
        };

        std::string rebuilt;
        rebuilt.reserve(out.size());
        std::sregex_iterator it(out.begin(), out.end(), kInlineSecretRe);
        std::sregex_iterator end;
        size_t last = 0;
        for (; it != end; ++it) {
            const auto& m = *it;
            rebuilt.append(out, last, static_cast<size_t>(m.position()) - last);
            std::string quote;
            std::string val;
            if (m[2].matched) {
                quote = "\"";
                val = m.str(2);
            } else if (m[3].matched) {
                quote = "'";
                val = m.str(3);
            } else if (m[4].matched) {
                quote = "`";
                val = m.str(4);
            } else {
                val = m.str(5);
            }

            std::string key_low = extract_key_lower(m.str(1));
            bool preserve = IsAlreadyRedacted(val);
            if (!preserve && key_low == "cookie" && is_redacted_cookie_assign(val)) {
                preserve = true;
            }
            if (!preserve && key_low == "authorization" && AsciiLower(val) == "bearer") {
                size_t after_pos = static_cast<size_t>(m.position() + m.length());
                std::string_view tail(out.data() + after_pos, out.size() - after_pos);
                size_t sp = 0;
                while (sp < tail.size() && (tail[sp] == ' ' || tail[sp] == '\t')) ++sp;
                if (sp > 0 && sp < tail.size()) {
                    size_t end_tok = sp;
                    while (end_tok < tail.size()) {
                        char c = tail[end_tok];
                        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' ||
                            c == ',' || c == ';' || c == '"' || c == '\'' ||
                            c == '<' || c == '>' || c == '}') {
                            break;
                        }
                        ++end_tok;
                    }
                    if (IsCompleteRedactedToken(tail.substr(sp, end_tok - sp))) {
                        preserve = true;
                    }
                }
            }

            if (preserve) {
                rebuilt += m.str(0);
            } else {
                rebuilt += m.str(1) + quote + "[REDACTED]" + quote;
            }
            last = static_cast<size_t>(m.position() + m.length());
        }
        rebuilt.append(out, last, out.size() - last);
        out = std::move(rebuilt);
    }

    // 7. Email addresses in shareable mode
    if (opts_.shareable_mode) {
        static const std::regex kEmailRe(
            R"(\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b)",
            std::regex::ECMAScript);
        out = std::regex_replace(out, kEmailRe, "[REDACTED_EMAIL]");
    }

    // 8. Runtime needles (second pass)
    out = ScrubRuntimeNeedles(out);
    return out;
}

std::string Sanitizer::SanitizeSample(std::string_view text) const {
    std::string out = SanitizeText(text);
    if (opts_.max_sample_chars > 0 && out.size() > opts_.max_sample_chars) {
        out.resize(opts_.max_sample_chars);
    }
    return out;
}

std::string Sanitizer::SanitizeJsonString(std::string_view json_text) const {
    try {
        auto j = nlohmann::json::parse(json_text);
        std::function<void(nlohmann::json&)> walk = [&](nlohmann::json& node) {
            if (node.is_object()) {
                for (auto it = node.begin(); it != node.end(); ++it) {
                    const std::string& k = it.key();
                    auto& v = it.value();
                    if (IsSecretJsonKey(k)) {
                        if (v.is_string()) {
                            std::string s = v.get<std::string>();
                            if (!IsAlreadyRedacted(s)) {
                                v = RedactTokenValue(s);
                            }
                        } else if (v.is_number() || v.is_boolean()) {
                            v = "[REDACTED]";
                        } else {
                            walk(v);
                        }
                    } else if (opts_.shareable_mode && IsPiiJsonKey(k)) {
                        if (v.is_string()) {
                            std::string s = v.get<std::string>();
                            if (!IsAlreadyRedacted(s)) {
                                v = "[REDACTED_PII]";
                            }
                        } else if (v.is_number() || v.is_boolean()) {
                            v = "[REDACTED_PII]";
                        } else {
                            walk(v);
                        }
                    } else {
                        walk(v);
                    }
                }
            } else if (node.is_array()) {
                for (auto& elem : node) {
                    walk(elem);
                }
            } else if (node.is_string()) {
                std::string s = node.get<std::string>();
                if (!IsAlreadyRedacted(s)) {
                    node = SanitizeText(s);
                }
            }
        };
        walk(j);
        return j.dump();
    } catch (...) {
        return SanitizeText(json_text);
    }
}

std::string Sanitizer::SanitizeFormUrlEncoded(std::string_view form_body) const {
    std::string out;
    size_t pos = 0;
    bool first = true;
    while (pos <= form_body.size()) {
        size_t amp = form_body.find('&', pos);
        if (amp == std::string_view::npos) amp = form_body.size();
        std::string_view item(form_body.data() + pos, amp - pos);
        if (!first) out.push_back('&');
        first = false;
        size_t eq = item.find('=');
        if (eq == std::string_view::npos) {
            out.append(item);
        } else {
            std::string_view k = item.substr(0, eq);
            std::string_view v = item.substr(eq + 1);
            if (IsSecretJsonKey(k) || IsSensitiveParamName(k)) {
                out.append(k);
                out.push_back('=');
                out += IsAlreadyRedacted(v) ? std::string(v) : RedactTokenValue(v);
            } else {
                out.append(k);
                out.push_back('=');
                out += SanitizeText(v);
            }
        }
        if (amp == form_body.size()) break;
        pos = amp + 1;
    }
    return out;
}

std::string Sanitizer::SanitizeHtml(std::string_view html) const {
    std::string out;
    out.reserve(html.size());
    size_t i = 0;
    const size_t n = html.size();

    while (i < n) {
        size_t lt = html.find('<', i);
        if (lt == std::string_view::npos) {
            out += SanitizeText(html.substr(i));
            break;
        }
        if (lt > i) {
            out += SanitizeText(html.substr(i, lt - i));
        }
        size_t gt = html.find('>', lt);
        if (gt == std::string_view::npos) {
            out += SanitizeText(html.substr(lt));
            break;
        }

        std::string tag(html.substr(lt, gt - lt + 1));
        std::string low_tag = AsciiLower(tag);

        auto is_tag = [&](std::string_view name) {
            if (low_tag.size() < 1 + name.size()) return false;
            if (low_tag.compare(1, name.size(), name) != 0) return false;
            if (low_tag.size() == 1 + name.size()) return true;
            char next = low_tag[1 + name.size()];
            return std::isspace(static_cast<unsigned char>(next)) || next == '>' || next == '/';
        };

        if (is_tag("input") || is_tag("meta")) {
            bool secret_tag = false;
            size_t vs = 0, vl = 0;
            if (ExtractAttr(tag, "type", vs, vl) && AsciiLower(std::string_view(tag).substr(vs, vl)) == "password") {
                secret_tag = true;
            }
            if (!secret_tag && ExtractAttr(tag, "name", vs, vl) &&
                IsHtmlSecretIdentity(std::string_view(tag).substr(vs, vl))) {
                secret_tag = true;
            }
            if (!secret_tag && ExtractAttr(tag, "id", vs, vl) &&
                IsHtmlSecretIdentity(std::string_view(tag).substr(vs, vl))) {
                secret_tag = true;
            }
            if (!secret_tag && ExtractAttr(tag, "http-equiv", vs, vl) &&
                IsHtmlSecretIdentity(std::string_view(tag).substr(vs, vl))) {
                secret_tag = true;
            }

            if (secret_tag) {
                const char* attr_target = is_tag("meta") ? "content" : "value";
                if (ExtractAttr(tag, attr_target, vs, vl)) {
                    std::string_view cur_val(tag.data() + vs, vl);
                    if (!cur_val.empty() && !IsAlreadyRedacted(cur_val)) {
                        tag.replace(vs, vl, "[REDACTED]");
                    }
                }
            }
            // Also sanitize URLs in href/src/action attributes
            for (const char* uattr : {"href", "src", "action"}) {
                if (ExtractAttr(tag, uattr, vs, vl)) {
                    std::string sanitized_u = SanitizeUrl(std::string_view(tag.data() + vs, vl));
                    tag.replace(vs, vl, sanitized_u);
                }
            }
            out += tag;
            i = gt + 1;
        } else if (is_tag("textarea")) {
            bool secret_ta = false;
            size_t vs = 0, vl = 0;
            if (ExtractAttr(tag, "name", vs, vl) &&
                IsHtmlSecretIdentity(std::string_view(tag).substr(vs, vl))) {
                secret_ta = true;
            }
            if (!secret_ta && ExtractAttr(tag, "id", vs, vl) &&
                IsHtmlSecretIdentity(std::string_view(tag).substr(vs, vl))) {
                secret_ta = true;
            }
            out += tag;
            size_t close_pos = AsciiLower(html).find("</textarea", gt + 1);
            if (close_pos == std::string::npos) {
                i = gt + 1;
            } else {
                std::string_view inner = html.substr(gt + 1, close_pos - (gt + 1));
                if (secret_ta && !TrimAscii(inner).empty() && !IsAlreadyRedacted(inner)) {
                    out += "[REDACTED]";
                } else {
                    out += SanitizeText(inner);
                }
                i = close_pos;
            }
        } else if (is_tag("script")) {
            out += tag;
            size_t close_pos = AsciiLower(html).find("</script", gt + 1);
            if (close_pos == std::string::npos) {
                i = gt + 1;
            } else {
                std::string script_body(html.substr(gt + 1, close_pos - (gt + 1)));
                // Scrub document.cookie = "..."
                static const std::regex kDocCookieRe(
                    R"((document\.cookie\s*=\s*)(["'`])([^"'`]*)\2)",
                    std::regex::ECMAScript | std::regex::icase);
                script_body = std::regex_replace(script_body, kDocCookieRe, R"($1$2[REDACTED]$2)");
                static const std::regex kJsSecretAssignRe(
                    R"(((?:password|passwd|csrf[_-]?token|xsrf[_-]?token|access[_-]?token|refresh[_-]?token|retoken|session[_-]?token|api[_-]?key)\s*[:=]\s*)(["'`])([^"'`]*)\2)",
                    std::regex::ECMAScript | std::regex::icase);
                script_body = std::regex_replace(script_body, kJsSecretAssignRe, R"($1$2[REDACTED]$2)");
                out += SanitizeText(script_body);
                i = close_pos;
            }
        } else {
            size_t vs = 0, vl = 0;
            for (const char* uattr : {"href", "src", "action"}) {
                if (ExtractAttr(tag, uattr, vs, vl)) {
                    std::string sanitized_u = SanitizeUrl(std::string_view(tag.data() + vs, vl));
                    tag.replace(vs, vl, sanitized_u);
                }
            }
            out += ScrubRuntimeNeedles(tag);
            i = gt + 1;
        }
    }

    return out;
}

std::string Sanitizer::SanitizeFilename(std::string_view path_or_name) const {
    std::string s = ScrubRuntimeNeedles(path_or_name);
    std::string cleaned;
    cleaned.reserve(s.size());
    for (unsigned char c : s) {
        if (c < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            cleaned.push_back('_');
        } else {
            cleaned.push_back(static_cast<char>(c));
        }
    }
    while (!cleaned.empty() && cleaned.front() == '.') {
        cleaned.erase(cleaned.begin());
    }
    if (cleaned.empty() || cleaned == "." || cleaned == "..") {
        cleaned = "file";
    }
    if (opts_.shareable_mode) {
        // Pseudonymize filename while preserving extension for shareable logs/exports
        size_t dot = cleaned.rfind('.');
        std::string ext = (dot != std::string::npos && dot > 0) ? cleaned.substr(dot) : "";
        return Sha256Hex(path_or_name).substr(0, 12) + ext;
    }
    return cleaned;
}

std::string Sanitizer::SafeDiagnosticPath(std::string_view path) const {
    std::string s(path);
    RuntimeNeedles rn = BuildRuntimeNeedles(opts_, true);
    for (const auto& p : rn.passwords) {
        ReplaceAllExact(s, p, "[REDACTED]");
    }
    for (const auto& e : rn.emails) {
        ReplaceAllExact(s, e, "[REDACTED_EMAIL]");
    }
    for (const auto& pii : rn.piis) {
        ReplaceAllExact(s, pii, "[REDACTED_PII]");
    }
    return s;
}

std::vector<std::string> Sanitizer::ScanReusableTokens(std::string_view text) const {
    constexpr size_t kChunkSize = 1024 * 1024;
    constexpr size_t kOverlap = 4096;

    std::set<std::string> found;

    static const std::regex kBearerRe(
        R"(\bBearer\s+([^\s,;"'<>]{12,}))",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kCookieRe(
        R"(\b(?:ad_session_id|ad_user_login)\s*=\s*["']?([^\s;"'<>&]{8,}))",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kQueryRe(
        R"([?&#/](?:access_token|refresh_token|id_token|csrf|xsrf|session_token|token|password|passwd|pwd|retoken)=([^&#\s"'<>]+))",
        std::regex::ECMAScript | std::regex::icase);
    static const std::regex kJsonRe(
        R"(["'](?:csrf|xsrf|access_token|refresh_token|id_token|session_token|authorization|set-cookie)["']\s*:\s*["']([^"']+)["'])",
        std::regex::ECMAScript | std::regex::icase);

    auto scan_chunk = [&](std::string_view chunk) {
        std::string s(chunk);
        auto check_regex = [&](const std::regex& re, const char* cat) {
            std::sregex_iterator it(s.begin(), s.end(), re);
            std::sregex_iterator end;
            for (; it != end; ++it) {
                std::string val = (*it)[1].str();
                if (!IsHiddenPlaceholder(val)) {
                    found.insert(cat);
                    break;
                }
            }
        };
        check_regex(kBearerRe, "bearer_like_value");
        check_regex(kCookieRe, "session_cookie_like_value");
        check_regex(kQueryRe, "sensitive_query_like_value");
        check_regex(kJsonRe, "secret_json_like_value");

        // Scan <input ...> tags safely without regex backtracking
        size_t pos = 0;
        std::string low = AsciiLower(s);
        while ((pos = low.find("<input", pos)) != std::string::npos) {
            size_t end_gt = s.find('>', pos);
            if (end_gt == std::string::npos || end_gt - pos > 2000) {
                pos += 6;
                continue;
            }
            std::string_view tag(s.data() + pos, end_gt - pos + 1);
            size_t vs = 0, vl = 0;
            bool is_secret_input = false;
            for (const char* attr : {"name", "id"}) {
                if (ExtractAttr(tag, attr, vs, vl)) {
                    std::string id_low = AsciiLower(tag.substr(vs, vl));
                    if (id_low.find("csrf") != std::string::npos ||
                        id_low.find("xsrf") != std::string::npos ||
                        id_low.find("authenticity_token") != std::string::npos ||
                        id_low.find("password") != std::string::npos ||
                        id_low.find("session_token") != std::string::npos ||
                        id_low.find("retoken") != std::string::npos) {
                        is_secret_input = true;
                        break;
                    }
                }
            }
            if (is_secret_input && ExtractAttr(tag, "value", vs, vl)) {
                if (!IsHiddenPlaceholder(tag.substr(vs, vl))) {
                    found.insert("secret_form_value");
                }
            }
            pos = end_gt + 1;
        }
    };

    if (text.size() <= kChunkSize) {
        scan_chunk(text);
    } else {
        size_t offset = 0;
        while (offset < text.size()) {
            size_t len = std::min(kChunkSize, text.size() - offset);
            scan_chunk(text.substr(offset, len));
            if (offset + len >= text.size()) break;
            offset += (kChunkSize - kOverlap);
        }
    }

    return std::vector<std::string>(found.begin(), found.end());
}

std::vector<std::string> Sanitizer::ScanReusableTokensFile(const std::string& file_path,
                                                           std::string* out_error) const {
    std::error_code ec;
    std::filesystem::path p(file_path);
    if (!std::filesystem::is_regular_file(p, ec)) {
        if (out_error != nullptr) {
            *out_error = "not a regular file";
        }
        return {};
    }
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) {
        if (out_error != nullptr) {
            *out_error = "failed to open file";
        }
        return {};
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    return ScanReusableTokens(oss.str());
}

NeedleScanResult Sanitizer::ScanStreamForLeaks(
    const std::function<size_t(uint8_t* buf, size_t cap)>& read_chunk) const {
    NeedleScanResult res;
    struct TypedNeedle {
        std::string needle;
        std::string kind;  // "password", "email", "pii"
    };
    std::vector<TypedNeedle> needles;
    auto add_needle = [&](std::string n, const char* kind) {
        if (n.size() >= 4) {
            needles.push_back({std::move(n), kind});
        }
    };

    RuntimeNeedles rn = BuildRuntimeNeedles(opts_, true);
    for (const auto& p : rn.passwords) {
        add_needle(p, "password");
    }
    for (const auto& e : rn.emails) {
        add_needle(e, "email");
    }
    for (const auto& pii : rn.piis) {
        add_needle(pii, "pii");
    }

    if (needles.empty()) return res;

    size_t max_len = 0;
    for (const auto& tn : needles) {
        max_len = std::max(max_len, tn.needle.size());
    }
    const size_t overlap = (max_len > 1) ? (max_len - 1) : 0;

    std::string window;
    std::vector<uint8_t> buf(65536);
    std::set<std::string> cats;

    while (true) {
        size_t got = read_chunk(buf.data(), buf.size());
        if (got == 0) break;
        window.append(reinterpret_cast<const char*>(buf.data()), got);

        for (const auto& tn : needles) {
            if (window.find(tn.needle) != std::string::npos) {
                if (tn.kind == "password") {
                    res.has_password_leak = true;
                    res.passed = false;
                    cats.insert("password");
                } else if (tn.kind == "email") {
                    res.personal_data_detected = true;
                    cats.insert("email");
                    if (!opts_.allow_personal_email) {
                        res.has_email_leak = true;
                        res.passed = false;
                    }
                } else if (tn.kind == "pii") {
                    res.personal_data_detected = true;
                    cats.insert("pii");
                    if (opts_.shareable_mode) {
                        res.has_pii_leak = true;
                        res.passed = false;
                    }
                }
            }
        }

        if (overlap == 0) {
            window.clear();
        } else if (window.size() > overlap) {
            window.erase(0, window.size() - overlap);
        }
    }

    res.finding_categories.assign(cats.begin(), cats.end());
    res.findings_count = cats.size();
    return res;
}

NeedleScanResult Sanitizer::ScanForLeaks(std::string_view data) const {
    size_t offset = 0;
    return ScanStreamForLeaks([&](uint8_t* buf, size_t cap) -> size_t {
        if (offset >= data.size()) return 0;
        size_t take = std::min(cap, data.size() - offset);
        std::copy(data.data() + offset, data.data() + offset + take, reinterpret_cast<char*>(buf));
        offset += take;
        return take;
    });
}

NeedleScanResult Sanitizer::ScanDirectoryForLeaks(const std::string& dir_path) const {
    NeedleScanResult agg;
    std::set<std::string> cats;
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec)) {
        return agg;
    }

    for (std::filesystem::recursive_directory_iterator it(dir_path, ec), end;
         it != end && !ec; it.increment(ec)) {
        // BUG-069: strictly check is_regular_file() because page slugs can be directories ending in ".html"
        if (!it->is_regular_file(ec)) {
            continue;
        }
        std::ifstream ifs(it->path(), std::ios::binary);
        if (!ifs.is_open()) continue;
        NeedleScanResult file_res = ScanStreamForLeaks([&](uint8_t* buf, size_t cap) -> size_t {
            ifs.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(cap));
            return static_cast<size_t>(ifs.gcount());
        });
        if (!file_res.passed) agg.passed = false;
        if (file_res.has_password_leak) agg.has_password_leak = true;
        if (file_res.has_email_leak) agg.has_email_leak = true;
        if (file_res.has_pii_leak) agg.has_pii_leak = true;
        if (file_res.personal_data_detected) agg.personal_data_detected = true;
        for (const auto& c : file_res.finding_categories) {
            cats.insert(c);
        }
    }
    agg.finding_categories.assign(cats.begin(), cats.end());
    agg.findings_count = cats.size();
    return agg;
}

}  // namespace academic::tec_digital
