#include "academic/tec_digital/cookie_jar.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/http_transport.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <limits>
#include <sstream>

namespace academic::tec_digital {

namespace {

bool IsIpv4Literal(std::string_view s) {
    if (s.empty()) return false;
    int parts = 0;
    size_t i = 0;
    while (i < s.size()) {
        if (parts >= 4) return false;
        size_t dot = s.find('.', i);
        if (dot == std::string_view::npos) dot = s.size();
        std::string_view seg = s.substr(i, dot - i);
        if (seg.empty() || seg.size() > 3) return false;
        int val = 0;
        for (char c : seg) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return false;
            val = val * 10 + (c - '0');
        }
        if (val > 255) return false;
        ++parts;
        if (dot == s.size()) break;
        i = dot + 1;
        if (i == s.size()) return false;
    }
    return parts == 4;
}

bool IsIpAddressLiteral(std::string_view host) {
    if (host.empty()) return false;
    if (host.find(':') != std::string_view::npos) return true;
    return IsIpv4Literal(host);
}

bool DomainMatches(std::string_view req_host, std::string_view cookie_domain, bool host_only) {
    std::string h = AsciiLower(req_host);
    std::string d = AsciiLower(cookie_domain);
    if (d.empty()) return false;
    if (h == d) return true;
    if (host_only || IsIpAddressLiteral(h)) {
        return false;
    }
    if (h.size() <= d.size() + 1) return false;
    const size_t offset = h.size() - d.size();
    return h[offset - 1] == '.' && h.compare(offset, d.size(), d) == 0;
}

bool PathMatches(std::string_view req_path, std::string_view cookie_path) {
    std::string rp = req_path.empty() ? "/" : std::string(req_path);
    std::string cp = cookie_path.empty() ? "/" : std::string(cookie_path);
    if (rp == cp) return true;
    if (rp.size() < cp.size()) return false;
    if (rp.compare(0, cp.size(), cp) != 0) return false;
    if (cp.back() == '/') return true;
    return rp[cp.size()] == '/';
}

std::string DefaultCookiePath(std::string_view req_path) {
    if (req_path.empty() || req_path.front() != '/') return "/";
    size_t last_slash = req_path.rfind('/');
    if (last_slash == 0 || last_slash == std::string_view::npos) return "/";
    return std::string(req_path.substr(0, last_slash));
}

int ParseMonthAbbr(std::string_view m) {
    std::string low = AsciiLower(m);
    if (low == "jan") return 1;
    if (low == "feb") return 2;
    if (low == "mar") return 3;
    if (low == "apr") return 4;
    if (low == "may") return 5;
    if (low == "jun") return 6;
    if (low == "jul") return 7;
    if (low == "aug") return 8;
    if (low == "sep") return 9;
    if (low == "oct") return 10;
    if (low == "nov") return 11;
    if (low == "dec") return 12;
    return 0;
}

int64_t DaysFromCivil(int y, int m, int d) {
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + static_cast<unsigned>(d) - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool ParseIntStrict(std::string_view sv, int& out) {
    if (sv.empty()) return false;
    for (char c : sv) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    out = std::atoi(std::string(sv).c_str());
    return true;
}

}  // namespace

int64_t CookieJar::CurrentUnixEpoch() {
    const auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}

bool CookieJar::ParseHttpDate(std::string_view date_str, int64_t& out_epoch) {
    std::string s = TrimAscii(date_str);
    if (s.empty()) return false;

    // Strip optional weekday ("Fri, " or "Friday, ")
    size_t comma = s.find(',');
    if (comma != std::string::npos) {
        s = TrimAscii(s.substr(comma + 1));
    }

    // Replace hyphens with spaces so both "01-Jan-2035" (OpenACS) and "01 Jan 2035" parse identically.
    for (char& c : s) {
        if (c == '-') c = ' ';
    }

    std::istringstream iss(s);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(tok);
    }

    int day = 0, month = 0, year = 0, hour = 0, min = 0, sec = 0;
    std::string time_str;

    // Expected: DD MMM YYYY HH:MM:SS [GMT] (4 or 5 tokens)
    if (tokens.size() >= 4 && ParseMonthAbbr(tokens[1]) != 0) {
        if (!ParseIntStrict(tokens[0], day)) return false;
        month = ParseMonthAbbr(tokens[1]);
        if (!ParseIntStrict(tokens[2], year)) return false;
        time_str = tokens[3];
    } else if (tokens.size() >= 4 && ParseMonthAbbr(tokens[0]) != 0) {
        // ANSI C asctime: MMM DD HH:MM:SS YYYY
        month = ParseMonthAbbr(tokens[0]);
        if (!ParseIntStrict(tokens[1], day)) return false;
        time_str = tokens[2];
        if (!ParseIntStrict(tokens[3], year)) return false;
    } else {
        return false;
    }

    if (year >= 0 && year < 70) year += 2000;
    else if (year >= 70 && year <= 99) year += 1900;

    if (time_str.size() != 8 || time_str[2] != ':' || time_str[5] != ':') return false;
    if (!ParseIntStrict(std::string_view(time_str).substr(0, 2), hour)) return false;
    if (!ParseIntStrict(std::string_view(time_str).substr(3, 2), min)) return false;
    if (!ParseIntStrict(std::string_view(time_str).substr(6, 2), sec)) return false;

    if (month < 1 || month > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 ||
        min < 0 || min > 59 || sec < 0 || sec > 60) {
        return false;
    }
    if (year < 1601 || year > 9999) return false;

    const int64_t days = DaysFromCivil(year, month, day);
    out_epoch = days * 86400LL + static_cast<int64_t>(hour) * 3600LL +
                static_cast<int64_t>(min) * 60LL + static_cast<int64_t>(sec);
    return true;
}

void CookieJar::ApplySetCookieHeaders(const std::vector<std::string>& set_cookie_headers,
                                      std::string_view request_host,
                                      std::string_view request_path,
                                      int64_t now_epoch) {
    const int64_t effective_now = (now_epoch >= 0) ? now_epoch : CurrentUnixEpoch();
    std::lock_guard<std::mutex> lk(mutex_);
    for (const auto& hdr : set_cookie_headers) {
        ApplySetCookieHeaderLocked(hdr, request_host, request_path, effective_now);
    }
}

void CookieJar::ApplySetCookieHeader(std::string_view header_line,
                                     std::string_view request_host,
                                     std::string_view request_path,
                                     int64_t now_epoch) {
    const int64_t effective_now = (now_epoch >= 0) ? now_epoch : CurrentUnixEpoch();
    std::lock_guard<std::mutex> lk(mutex_);
    ApplySetCookieHeaderLocked(header_line, request_host, request_path, effective_now);
}

void CookieJar::ApplySetCookieHeaderLocked(std::string_view header_line,
                                           std::string_view request_host,
                                           std::string_view request_path,
                                           int64_t effective_now) {
    std::string_view line = header_line;

    // Strip optional "Set-Cookie:" prefix if caller passed full header line
    if (line.size() >= 11 && AsciiLower(line.substr(0, 11)) == "set-cookie:") {
        line.remove_prefix(11);
    }

    // Split ONLY on ';' (never ',' because Expires contains "Fri, 01-Jan-2035 ...")
    std::vector<std::string_view> parts;
    size_t pos = 0;
    while (pos <= line.size()) {
        size_t semi = line.find(';', pos);
        if (semi == std::string_view::npos) {
            parts.push_back(line.substr(pos));
            break;
        }
        parts.push_back(line.substr(pos, semi - pos));
        pos = semi + 1;
    }
    if (parts.empty()) return;

    std::string_view nv = parts[0];
    size_t eq = nv.find('=');
    if (eq == std::string_view::npos) return;

    Cookie c;
    c.name = TrimAscii(nv.substr(0, eq));
    c.value = TrimAscii(nv.substr(eq + 1));
    if (c.name.empty()) return;

    std::string req_host_low = AsciiLower(TrimAscii(request_host));
    c.domain = req_host_low;
    c.host_only = true;
    c.path = DefaultCookiePath(request_path);

    bool has_max_age = false;
    int64_t max_age_expires = 0;

    for (size_t i = 1; i < parts.size(); ++i) {
        std::string attr = TrimAscii(parts[i]);
        if (attr.empty()) continue;

        size_t aeq = attr.find('=');
        std::string key = AsciiLower(TrimAscii(aeq == std::string::npos ? attr : attr.substr(0, aeq)));
        std::string val = (aeq == std::string::npos) ? "" : TrimAscii(attr.substr(aeq + 1));

        if (key == "domain") {
            while (!val.empty() && val.front() == '.') {
                val.erase(val.begin());
            }
            val = AsciiLower(val);
            if (!val.empty()) {
                if (!req_host_low.empty() && !DomainMatches(req_host_low, val, false)) {
                    // Reject cross-domain cookie injection
                    return;
                }
                c.domain = val;
                c.host_only = false;
            }
        } else if (key == "path") {
            if (!val.empty() && val.front() == '/') {
                c.path = val;
            } else {
                c.path = DefaultCookiePath(request_path);
            }
        } else if (key == "expires") {
            int64_t parsed_exp = 0;
            if (ParseHttpDate(val, parsed_exp)) {
                c.has_expires = true;
                c.expires_epoch = parsed_exp;
            }
        } else if (key == "max-age") {
            if (!val.empty()) {
                bool valid_digits = true;
                size_t start_idx = 0;
                if (val[0] == '-' || val[0] == '+') {
                    if (val.size() == 1) valid_digits = false;
                    start_idx = 1;
                }
                for (size_t k = start_idx; k < val.size(); ++k) {
                    if (!std::isdigit(static_cast<unsigned char>(val[k]))) {
                        valid_digits = false;
                        break;
                    }
                }
                if (valid_digits) {
                    errno = 0;
                    char* endptr = nullptr;
                    long long secs = std::strtoll(val.c_str(), &endptr, 10);
                    if (endptr != val.c_str() && *endptr == '\0') {
                        has_max_age = true;
                        if (errno == ERANGE && val[0] == '-') {
                            max_age_expires = 0;
                        } else if (errno == ERANGE ||
                                   secs > (std::numeric_limits<int64_t>::max() - effective_now)) {
                            max_age_expires = std::numeric_limits<int64_t>::max();
                        } else if (secs <= 0) {
                            max_age_expires = 0;
                        } else {
                            max_age_expires = effective_now + static_cast<int64_t>(secs);
                        }
                    }
                }
            }
        } else if (key == "secure") {
            c.secure = true;
        } else if (key == "httponly") {
            c.http_only = true;
        } else if (key == "discard") {
            c.discard = true;
        } else if (key == "samesite") {
            c.same_site = val;
        }
    }

    if (has_max_age) {
        c.has_expires = true;
        c.expires_epoch = max_age_expires;
    }

    const bool expired = (c.has_expires && c.expires_epoch <= effective_now);

    // Remove existing cookie matching (name, domain, path)
    cookies_.erase(
        std::remove_if(cookies_.begin(), cookies_.end(),
                       [&](const Cookie& existing) {
                           return existing.name == c.name &&
                                  AsciiLower(existing.domain) == AsciiLower(c.domain) &&
                                  existing.path == c.path;
                       }),
        cookies_.end());

    if (!expired) {
        cookies_.push_back(std::move(c));
    }
}

std::string CookieJar::BuildCookieHeader(std::string_view url, int64_t now_epoch) const {
    ParsedUrl parsed = ParseHttpUrl(url);
    if (!parsed.valid) return {};

    const int64_t effective_now = (now_epoch >= 0) ? now_epoch : CurrentUnixEpoch();
    const bool is_https = (parsed.scheme == "https");

    std::lock_guard<std::mutex> lk(mutex_);
    struct Match {
        size_t idx;
        const Cookie* cookie;
    };
    std::vector<Match> matches;
    for (size_t i = 0; i < cookies_.size(); ++i) {
        const Cookie& c = cookies_[i];
        if (c.has_expires && c.expires_epoch <= effective_now) continue;
        if (c.secure && !is_https) continue;
        if (!DomainMatches(parsed.host, c.domain, c.host_only)) continue;
        if (!PathMatches(parsed.path, c.path)) continue;
        matches.push_back({i, &c});
    }

    std::stable_sort(matches.begin(), matches.end(),
                     [](const Match& a, const Match& b) {
                         if (a.cookie->path.size() != b.cookie->path.size()) {
                             return a.cookie->path.size() > b.cookie->path.size();
                         }
                         return a.idx < b.idx;
                     });

    std::string out;
    for (size_t i = 0; i < matches.size(); ++i) {
        if (i > 0) out += "; ";
        out += matches[i].cookie->name;
        out += "=";
        out += matches[i].cookie->value;
    }
    return out;
}

std::optional<Cookie> CookieJar::GetCookie(std::string_view name,
                                           std::string_view host,
                                           std::string_view path,
                                           bool is_https,
                                           int64_t now_epoch) const {
    const int64_t effective_now = (now_epoch >= 0) ? now_epoch : CurrentUnixEpoch();
    std::lock_guard<std::mutex> lk(mutex_);
    const Cookie* best = nullptr;
    for (const auto& c : cookies_) {
        if (c.name != name) continue;
        if (c.has_expires && c.expires_epoch <= effective_now) continue;
        if (c.secure && !is_https) continue;
        if (!host.empty() && !DomainMatches(host, c.domain, c.host_only)) continue;
        if (!path.empty() && !PathMatches(path, c.path)) continue;
        if (best == nullptr || c.path.size() > best->path.size()) {
            best = &c;
        }
    }
    if (best == nullptr) return std::nullopt;
    return *best;
}

bool CookieJar::HasCookie(std::string_view name,
                          std::string_view host,
                          std::string_view path,
                          bool is_https,
                          int64_t now_epoch) const {
    auto found = GetCookie(name, host, path, is_https, now_epoch);
    return found.has_value() && !found->value.empty();
}

void CookieJar::SetCookies(std::vector<Cookie> cookies) {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& c : cookies_) {
        SecureZeroString(c.value);
    }
    cookies_.clear();
    cookies_ = std::move(cookies);
}

void CookieJar::Clear() {
    std::lock_guard<std::mutex> lk(mutex_);
    for (auto& c : cookies_) {
        SecureZeroString(c.value);
    }
    cookies_.clear();
}

std::vector<CookieMeta> CookieJar::ExportSanitizedMetadata(int64_t now_epoch) const {
    const int64_t effective_now = (now_epoch >= 0) ? now_epoch : CurrentUnixEpoch();
    std::lock_guard<std::mutex> lk(mutex_);
    std::vector<CookieMeta> out;
    for (const auto& c : cookies_) {
        if (c.has_expires && c.expires_epoch <= effective_now) continue;
        CookieMeta m;
        m.name = c.name;
        m.domain = c.domain;
        m.path = c.path;
        m.expires_epoch = c.expires_epoch;
        m.has_expires = c.has_expires;
        m.host_only = c.host_only;
        m.secure = c.secure;
        m.http_only = c.http_only;
        m.discard = c.discard;
        m.same_site = c.same_site;
        m.sha256_prefix = Sha256Hex(c.value).substr(0, 8);
        out.push_back(std::move(m));
    }
    return out;
}

std::string CookieJar::ExportSanitizedMetadataJson(std::string_view saved_at_utc,
                                                   int64_t now_epoch) const {
    auto metas = ExportSanitizedMetadata(now_epoch);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& m : metas) {
        arr.push_back({
            {"name", m.name},
            {"domain", m.domain},
            {"path", m.path},
            {"has_expires", m.has_expires},
            {"expires_epoch", m.expires_epoch},
            {"host_only", m.host_only},
            {"secure", m.secure},
            {"http_only", m.http_only},
            {"discard", m.discard},
            {"same_site", m.same_site},
            {"sha256_prefix", m.sha256_prefix}
        });
    }
    nlohmann::json root = {
        {"saved_at", std::string(saved_at_utc)},
        {"cookies", std::move(arr)}
    };
    return root.dump(2);
}

std::string CookieJar::SerializeToJson() const {
    std::lock_guard<std::mutex> lk(mutex_);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& c : cookies_) {
        arr.push_back({
            {"name", c.name},
            {"value", c.value},
            {"domain", c.domain},
            {"path", c.path},
            {"expires_epoch", c.expires_epoch},
            {"has_expires", c.has_expires},
            {"host_only", c.host_only},
            {"secure", c.secure},
            {"http_only", c.http_only},
            {"discard", c.discard},
            {"same_site", c.same_site}
        });
    }
    return arr.dump();
}

bool CookieJar::DeserializeFromJson(std::string_view json_str) {
    try {
        auto arr = nlohmann::json::parse(json_str);
        if (!arr.is_array()) return false;
        std::vector<Cookie> parsed;
        for (const auto& item : arr) {
            if (!item.is_object()) return false;
            Cookie c;
            c.name = item.value("name", "");
            c.value = item.value("value", "");
            c.domain = item.value("domain", "");
            c.path = item.value("path", "/");
            c.expires_epoch = item.value("expires_epoch", int64_t{0});
            c.has_expires = item.value("has_expires", false);
            c.host_only = item.value("host_only", true);
            c.secure = item.value("secure", false);
            c.http_only = item.value("http_only", false);
            c.discard = item.value("discard", false);
            c.same_site = item.value("same_site", "");
            if (!c.name.empty()) {
                parsed.push_back(std::move(c));
            }
        }
        SetCookies(std::move(parsed));
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace academic::tec_digital
