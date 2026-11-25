#include "http_common.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <locale>
#include <mutex>
#include <regex>

namespace academic::tec_digital {

namespace {

bool HasSchemePrefix(std::string_view s) {
    size_t sep = s.find("://");
    if (sep == std::string_view::npos || sep == 0) return false;
    size_t delim = s.find_first_of("/?#");
    if (delim != std::string_view::npos && delim < sep) return false;
    if (!std::isalpha(static_cast<unsigned char>(s[0]))) return false;
    for (size_t i = 1; i < sep; ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

bool IsValidPortStr(std::string_view s) {
    if (s.empty() || s.size() > 5) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

std::string RemoveDotSegments(std::string_view input) {
    std::vector<std::string_view> segments;
    const bool trailing_slash = !input.empty() && input.back() == '/';
    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && input[i] == '/') ++i;
        if (i >= input.size()) break;
        size_t j = input.find('/', i);
        if (j == std::string_view::npos) j = input.size();
        std::string_view seg = input.substr(i, j - i);
        if (seg == ".") {
            // skip
        } else if (seg == "..") {
            if (!segments.empty()) segments.pop_back();
        } else {
            segments.push_back(seg);
        }
        i = j;
    }
    std::string out = "/";
    for (size_t idx = 0; idx < segments.size(); ++idx) {
        if (idx > 0) out.push_back('/');
        out.append(segments[idx]);
    }
    if (trailing_slash && out.size() > 1) {
        out.push_back('/');
    }
    return out;
}

}  // namespace

ParsedUrl ParseHttpUrl(std::string_view raw) {
    ParsedUrl res;
    std::string trimmed = TrimAscii(raw);
    if (trimmed.empty()) return res;

    for (unsigned char c : trimmed) {
        if (c < 0x20 || c == 0x7F) return res;
    }

    size_t scheme_end = trimmed.find("://");
    if (scheme_end == std::string::npos || scheme_end == 0) return res;

    std::string scheme = AsciiLower(trimmed.substr(0, scheme_end));
    if (scheme != "http" && scheme != "https") return res;

    size_t auth_start = scheme_end + 3;
    size_t path_start = trimmed.find_first_of("/?#", auth_start);
    std::string authority = (path_start == std::string::npos)
                                ? trimmed.substr(auth_start)
                                : trimmed.substr(auth_start, path_start - auth_start);
    if (authority.empty()) return res;

    // Strip userinfo if present (reject empty host)
    size_t at_pos = authority.rfind('@');
    if (at_pos != std::string::npos) {
        authority = authority.substr(at_pos + 1);
    }
    if (authority.empty()) return res;

    std::string host;
    uint16_t port = (scheme == "https") ? 443 : 80;
    bool has_explicit_port = false;

    if (authority.front() == '[') {
        size_t close_br = authority.find(']');
        if (close_br == std::string::npos) return res;
        host = AsciiLower(authority.substr(1, close_br - 1));
        if (close_br + 1 < authority.size()) {
            if (authority[close_br + 1] != ':') return res;
            std::string port_str = authority.substr(close_br + 2);
            if (!port_str.empty()) {
                if (!IsValidPortStr(port_str)) return res;
                long p = std::strtol(port_str.c_str(), nullptr, 10);
                if (p <= 0 || p > 65535) return res;
                port = static_cast<uint16_t>(p);
                has_explicit_port = true;
            }
        }
    } else {
        size_t colon = authority.rfind(':');
        if (colon != std::string::npos) {
            host = AsciiLower(authority.substr(0, colon));
            std::string port_str = authority.substr(colon + 1);
            if (!port_str.empty()) {
                if (!IsValidPortStr(port_str)) return res;
                long p = std::strtol(port_str.c_str(), nullptr, 10);
                if (p <= 0 || p > 65535) return res;
                port = static_cast<uint16_t>(p);
                has_explicit_port = true;
            }
        } else {
            host = AsciiLower(authority);
        }
    }

    if (host.empty()) return res;

    const bool is_default_port =
        (scheme == "http" && port == 80) || (scheme == "https" && port == 443);
    const bool is_ipv6 = (host.find(':') != std::string::npos);

    std::string host_fmt = is_ipv6 ? ("[" + host + "]") : host;
    std::string host_port = is_default_port ? host_fmt : (host_fmt + ":" + std::to_string(port));
    std::string origin = scheme + "://" + host_port;

    std::string rest = (path_start == std::string::npos) ? "" : trimmed.substr(path_start);
    size_t frag_pos = rest.find('#');
    if (frag_pos != std::string::npos) {
        rest = rest.substr(0, frag_pos);
    }

    std::string path = "/";
    std::string query;
    size_t q_pos = rest.find('?');
    if (q_pos != std::string::npos) {
        std::string raw_path = rest.substr(0, q_pos);
        path = raw_path.empty() ? "/" : raw_path;
        query = rest.substr(q_pos + 1);
    } else if (!rest.empty()) {
        path = rest;
    }

    if (path.front() != '/') {
        path = "/" + path;
    }

    res.valid = true;
    res.scheme = std::move(scheme);
    res.host = std::move(host);
    res.port = port;
    res.has_explicit_port = has_explicit_port && !is_default_port;
    res.host_port = std::move(host_port);
    res.origin = std::move(origin);
    res.path = std::move(path);
    res.query = std::move(query);
    res.target = res.query.empty() ? res.path : (res.path + "?" + res.query);
    return res;
}

bool IsLoopbackHost(std::string_view host) {
    std::string lower = AsciiLower(host);
    if (!lower.empty() && lower.front() == '[' && lower.back() == ']') {
        lower = lower.substr(1, lower.size() - 2);
    }
    return lower == "127.0.0.1" || lower == "::1" || lower == "localhost";
}

bool IsSameOrigin(std::string_view url_a, std::string_view url_b) {
    ParsedUrl a = ParseHttpUrl(url_a);
    ParsedUrl b = ParseHttpUrl(url_b);
    if (!a.valid || !b.valid) return false;
    return a.origin == b.origin;
}

std::string ResolveRedirectLocation(std::string_view base_url, std::string_view location) {
    std::string loc = TrimAscii(location);
    if (loc.empty()) return {};

    // Full scheme URL?
    if (HasSchemePrefix(loc)) {
        return loc;
    }

    ParsedUrl base = ParseHttpUrl(base_url);
    if (!base.valid) return {};

    if (loc.size() >= 2 && loc[0] == '/' && loc[1] == '/') {
        return base.scheme + ":" + loc;
    }

    // Strip fragment from location
    size_t frag = loc.find('#');
    if (frag != std::string::npos) {
        loc = loc.substr(0, frag);
    }

    if (loc.empty()) return base.origin + base.target;

    if (loc.front() == '?') {
        return base.origin + base.path + loc;
    }

    if (loc.front() == '/') {
        size_t q = loc.find('?');
        std::string p = (q == std::string::npos) ? loc : loc.substr(0, q);
        std::string qry = (q == std::string::npos) ? "" : loc.substr(q);
        return base.origin + RemoveDotSegments(p) + qry;
    }

    // Relative path
    size_t q = loc.find('?');
    std::string rel_p = (q == std::string::npos) ? loc : loc.substr(0, q);
    std::string qry = (q == std::string::npos) ? "" : loc.substr(q);
    std::string base_dir = base.path;
    size_t last_slash = base_dir.rfind('/');
    if (last_slash == std::string::npos) {
        base_dir = "/";
    } else {
        base_dir = base_dir.substr(0, last_slash + 1);
    }
    return base.origin + RemoveDotSegments(base_dir + rel_p) + qry;
}

std::string FormatRangeHeader(uint64_t start_byte, uint64_t end_byte) {
    // Never emit "Range: bytes=0-" (TEC server bug)
    if (start_byte == 0 && end_byte == 0) {
        return {};
    }
    if (end_byte > start_byte) {
        return "bytes=" + std::to_string(start_byte) + "-" + std::to_string(end_byte);
    }
    if (start_byte > 0) {
        return "bytes=" + std::to_string(start_byte) + "-";
    }
    return {};
}

namespace detail {

HttpResponse ExecuteWithSameOriginRedirects(
    const HttpRequest& req,
    const std::function<HttpResponse(const HttpRequest&)>& single_exec) {
    if (!req.follow_redirects) {
        return single_exec(req);
    }

    HttpRequest current = req;
    current.follow_redirects = false;
    const int max_hops = (req.max_redirects > 0) ? req.max_redirects : 5;

    for (int hop = 0; hop <= max_hops; ++hop) {
        HttpResponse resp = single_exec(current);
        if (!resp.transport_ok) {
            return resp;
        }
        const int code = resp.status_code;
        const bool is_redirect =
            (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
        if (!is_redirect || resp.location.empty()) {
            return resp;
        }
        std::string next_url = ResolveRedirectLocation(current.url, resp.location);
        if (next_url.empty()) {
            resp.transport_ok = false;
            resp.error_message = "invalid redirect location";
            return resp;
        }
        if (!IsSameOrigin(current.url, next_url)) {
            resp.transport_ok = false;
            resp.error_message = "OffOriginRedirect: blocked cross-origin redirect";
            return resp;
        }
        if (hop == max_hops) {
            resp.transport_ok = false;
            resp.error_message = "TooManyRedirects";
            return resp;
        }
        if (code == 301 || code == 302 || code == 303) {
            if (current.method != "GET" && current.method != "HEAD") {
                current.method = "GET";
                current.body.clear();
            }
        }
        current.url = std::move(next_url);
    }

    HttpResponse err;
    err.transport_ok = false;
    err.error_message = "TooManyRedirects";
    return err;
}

void WarmupRegexLocale() {
    static std::once_flag once;
    std::call_once(once, []() {
        const auto& ct = std::use_facet<std::ctype<char>>(std::locale::classic());
        for (int c = 0; c < 256; ++c) {
            (void)ct.narrow(static_cast<char>(c), '\0');
            (void)ct.widen(static_cast<char>(c));
        }
        static const std::regex kWarmRe("a", std::regex::ECMAScript | std::regex::icase);
        (void)std::regex_search(std::string("a"), kWarmRe);
    });
}

}  // namespace detail

}  // namespace academic::tec_digital
