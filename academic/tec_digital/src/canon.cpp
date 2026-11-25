#include "academic/tec_digital/canon.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/http_transport.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

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

bool IsUnreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~';
}

int HexVal(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

std::string UrlDecodeForm(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '+') {
            out.push_back(' ');
        } else if (sv[i] == '%' && i + 2 < sv.size()) {
            int hi = HexVal(sv[i + 1]);
            int lo = HexVal(sv[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
            out.push_back(sv[i]);
        } else {
            out.push_back(sv[i]);
        }
    }
    return out;
}

std::string UrlEncodeForm(std::string_view sv) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(sv.size());
    for (unsigned char c : sv) {
        if (IsUnreserved(c)) {
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

std::string RemoveDotSegmentsRfc3986(std::string_view input) {
    std::vector<std::string_view> stack;
    const bool had_trailing_slash = !input.empty() && input.back() == '/';
    bool ended_with_dot = false;

    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && input[i] == '/') ++i;
        if (i >= input.size()) break;
        size_t j = input.find('/', i);
        if (j == std::string_view::npos) j = input.size();
        std::string_view seg = input.substr(i, j - i);
        ended_with_dot = (seg == "." || seg == "..");
        if (seg == ".") {
            // no-op
        } else if (seg == "..") {
            if (!stack.empty()) stack.pop_back();
        } else {
            stack.push_back(seg);
        }
        i = j;
    }

    std::string out = "/";
    for (size_t idx = 0; idx < stack.size(); ++idx) {
        if (idx > 0) out.push_back('/');
        out.append(stack[idx]);
    }
    if ((had_trailing_slash || ended_with_dot) && out.size() > 1) {
        out.push_back('/');
    }
    return out;
}

bool EndsWithCI(std::string_view str, std::string_view suffix) {
    if (str.size() < suffix.size()) return false;
    return AsciiLower(str.substr(str.size() - suffix.size())) == AsciiLower(suffix);
}

bool MatchesCalendarRoute(std::string_view path,
                          const std::vector<std::pair<std::string, std::string>>& pairs) {
    std::string low = AsciiLower(path);
    while (low.size() > 1 && low.back() == '/') {
        low.pop_back();
    }
    if (EndsWithCI(low, "/calendar/view")) {
        return true;
    }
    if (EndsWithCI(low, "/one-community")) {
        for (const auto& kv : pairs) {
            if (kv.first == "date") return true;
        }
    }
    return false;
}

bool IsDefaultIrrelevantParam(std::string_view key_low) {
    if (key_low == "avoid_cache" || key_low == "fbclid" || key_low == "gclid" ||
        key_low == "mc_cid" || key_low == "mc_eid") {
        return true;
    }
    if (key_low.size() >= 4 && key_low.compare(0, 4, "utm_") == 0) {
        return true;
    }
    return false;
}

}  // namespace

Result<std::string> UrlCanonicalizer::Canonicalize(std::string_view raw_url,
                                                   std::string_view base_url,
                                                   const CanonOptions& opts) {
    std::string raw = TrimAscii(raw_url);
    if (raw.empty()) {
        return Result<std::string>::Failure("empty_url", "URL is empty");
    }
    for (unsigned char c : raw) {
        if (c < 0x20 || c == 0x7F) {
            return Result<std::string>::Failure("control_char", "URL contains control character");
        }
    }

    // Reject non-HTTP(S) explicit schemes (javascript:, mailto:, data:, tel:, blob:)
    {
        size_t colon = raw.find(':');
        size_t slash = raw.find('/');
        size_t qmark = raw.find('?');
        size_t hash = raw.find('#');
        size_t first_delim = std::min({slash, qmark, hash});
        if (colon != std::string::npos && colon < first_delim) {
            std::string scheme = AsciiLower(raw.substr(0, colon));
            if (scheme != "http" && scheme != "https") {
                return Result<std::string>::Failure("unsupported_scheme",
                                                    "non-HTTP(S) scheme: " + scheme);
            }
        }
    }

    std::string resolved;
    if (HasSchemePrefix(raw)) {
        resolved = raw;
    } else if (raw.size() >= 2 && raw[0] == '/' && raw[1] == '/') {
        if (!base_url.empty()) {
            ParsedUrl bp = ParseHttpUrl(base_url);
            resolved = (bp.valid ? bp.scheme : std::string("https")) + ":" + raw;
        } else {
            resolved = "https:" + raw;
        }
    } else if (!base_url.empty()) {
        resolved = ResolveRedirectLocation(base_url, raw);
        if (resolved.empty()) {
            return Result<std::string>::Failure("invalid_base_url", "cannot resolve relative URL");
        }
    } else {
        return Result<std::string>::Failure("relative_without_base",
                                            "relative URL provided without base_url");
    }

    ParsedUrl parsed = ParseHttpUrl(resolved);
    if (!parsed.valid) {
        return Result<std::string>::Failure("invalid_url", "unable to parse HTTP(S) URL");
    }

    std::string norm_path = RemoveDotSegmentsRfc3986(parsed.path);
    if (opts.strip_trailing_slash && norm_path.size() > 1 && norm_path.back() == '/') {
        norm_path.pop_back();
    }

    std::unordered_set<std::string> extra_set;
    for (const auto& p : opts.extra_irrelevant_params) {
        extra_set.insert(AsciiLower(p));
    }

    std::vector<std::pair<std::string, std::string>> pairs;
    if (!parsed.query.empty()) {
        size_t pos = 0;
        while (pos <= parsed.query.size()) {
            size_t amp = parsed.query.find('&', pos);
            if (amp == std::string::npos) amp = parsed.query.size();
            std::string_view item(parsed.query.data() + pos, amp - pos);
            if (!item.empty()) {
                size_t eq = item.find('=');
                std::string k = UrlDecodeForm(eq == std::string_view::npos ? item : item.substr(0, eq));
                std::string v = (eq == std::string_view::npos)
                                    ? ""
                                    : UrlDecodeForm(item.substr(eq + 1));
                std::string k_low = AsciiLower(k);
                bool drop = false;
                if (opts.strip_tracking_and_cache_busters && IsDefaultIrrelevantParam(k_low)) {
                    drop = true;
                }
                if (extra_set.count(k_low) > 0) {
                    drop = true;
                }
                if (!drop) {
                    pairs.emplace_back(std::move(k), std::move(v));
                }
            }
            if (amp == parsed.query.size()) break;
            pos = amp + 1;
        }
    }

    if (opts.collapse_bug059_calendar && MatchesCalendarRoute(norm_path, pairs)) {
        std::vector<std::pair<std::string, std::string>> collapsed;
        collapsed.reserve(pairs.size());
        for (auto& kv : pairs) {
            if (kv.first == "period_days") {
                continue;
            }
            if (kv.first == "date" && kv.second.size() >= 7) {
                kv.second = kv.second.substr(0, 7);
            }
            collapsed.push_back(std::move(kv));
        }
        pairs = std::move(collapsed);
    }

    std::sort(pairs.begin(), pairs.end(),
              [](const auto& a, const auto& b) {
                  if (a.first != b.first) return a.first < b.first;
                  return a.second < b.second;
              });

    std::string out = parsed.origin + norm_path;
    if (!pairs.empty()) {
        out.push_back('?');
        for (size_t i = 0; i < pairs.size(); ++i) {
            if (i > 0) out.push_back('&');
            out += UrlEncodeForm(pairs[i].first);
            out.push_back('=');
            out += UrlEncodeForm(pairs[i].second);
        }
    }
    return Result<std::string>::Success(std::move(out));
}

std::string UrlCanonicalizer::PageKeySha1(std::string_view canonical_url) {
    std::string full = Sha1Hex(canonical_url);
    return full.size() >= 12 ? full.substr(0, 12) : full;
}

std::string UrlCanonicalizer::PageKeySha256(std::string_view canonical_url, size_t prefix_len) {
    std::string full = Sha256Hex(canonical_url);
    if (prefix_len == 0 || prefix_len >= full.size()) return full;
    return full.substr(0, prefix_len);
}

std::string UrlCanonicalizer::UrlObjectId(std::string_view canonical_url) {
    std::string full = Sha256Hex(canonical_url);
    return "url:" + (full.size() >= 32 ? full.substr(0, 32) : full);
}

}  // namespace academic::tec_digital
