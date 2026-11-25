#pragma once
// ============================================================================
// doc_studio/core/inline_url_parser.hpp
// Deterministic, security-hardened inline URL extractor for free-form TextBox
// paragraphs and note text. Enforces strict allow-list (http://, https://,
// mailto:, and boundary-delimited www.* normalized to https://) with exact
// UTF-8 and UTF-16 code-unit spans for DirectWrite layout & HitTestPoint.
// ============================================================================

#include "../media/native_video_engine.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

struct InlineUrlSpan {
    std::size_t utf8Start = 0;
    std::size_t utf8Len = 0;
    std::size_t utf16Start = 0;
    std::size_t utf16Len = 0;
    std::string rawTextUtf8;
    std::string normalizedUrlUtf8;
    bool isYouTube = false;
};

inline bool IsYouTubeUrlUtf8(const std::string& urlUtf8) noexcept {
    std::string low;
    low.reserve(urlUtf8.size());
    for (unsigned char c : urlUtf8) {
        low.push_back(static_cast<char>(std::tolower(c)));
    }
    if (low.rfind("https://", 0) != 0 && low.rfind("http://", 0) != 0) return false;
    const std::size_t schemeEnd = low.find("://");
    if (schemeEnd == std::string::npos) return false;
    const std::string rest = low.substr(schemeEnd + 3);
    return rest.rfind("youtube.com/watch", 0) == 0 ||
           rest.rfind("www.youtube.com/watch", 0) == 0 ||
           rest.rfind("m.youtube.com/watch", 0) == 0 ||
           rest.rfind("youtu.be/", 0) == 0 ||
           rest.rfind("www.youtu.be/", 0) == 0 ||
           rest.rfind("youtube.com/shorts/", 0) == 0 ||
           rest.rfind("www.youtube.com/shorts/", 0) == 0;
}

inline std::vector<InlineUrlSpan> ExtractSafeInlineUrls(const std::string& textUtf8) {
    std::vector<InlineUrlSpan> out;
    if (textUtf8.empty()) return out;

    // Precompute UTF-8 byte offset -> UTF-16 code unit offset map (exact with Spanish/math Unicode)
    std::vector<std::size_t> utf8To16(textUtf8.size() + 1, 0u);
    {
        std::size_t u16 = 0;
        for (std::size_t i = 0; i < textUtf8.size(); ) {
            const unsigned char ch = static_cast<unsigned char>(textUtf8[i]);
            std::size_t step = 1;
            std::size_t wStep = 1;
            if ((ch & 0xE0u) == 0xC0u) { step = 2; }
            else if ((ch & 0xF0u) == 0xE0u) { step = 3; }
            else if ((ch & 0xF8u) == 0xF0u) { step = 4; wStep = 2; }
            for (std::size_t k = 0; k < step && (i + k) < textUtf8.size(); ++k) {
                utf8To16[i + k] = u16;
            }
            i = (std::min)(textUtf8.size(), i + step);
            u16 += wStep;
        }
        utf8To16[textUtf8.size()] = u16;
    }

    auto isStartBoundary = [&](std::size_t pos) -> bool {
        if (pos == 0) return true;
        const unsigned char prev = static_cast<unsigned char>(textUtf8[pos - 1]);
        // Reject ':' or '/' or alnum right before candidate so "javascript:https://..." is NEVER matched!
        return std::isspace(prev) != 0 ||
               prev == '(' || prev == '[' || prev == '{' || prev == '<' ||
               prev == '"' || prev == '\'' || prev == ',' || prev == ';';
    };

    auto matchPrefixCI = [&](std::size_t pos, const char* prefix, std::size_t len) -> bool {
        if (pos + len > textUtf8.size()) return false;
        for (std::size_t k = 0; k < len; ++k) {
            if (std::tolower(static_cast<unsigned char>(textUtf8[pos + k])) !=
                std::tolower(static_cast<unsigned char>(prefix[k]))) {
                return false;
            }
        }
        return true;
    };

    const std::size_t n = textUtf8.size();
    for (std::size_t i = 0; i < n; ) {
        if (!isStartBoundary(i)) {
            ++i;
            continue;
        }
        bool isWww = false;
        std::size_t minLen = 0;
        if (matchPrefixCI(i, "https://", 8)) { minLen = 10; }
        else if (matchPrefixCI(i, "http://", 7)) { minLen = 9; }
        else if (matchPrefixCI(i, "mailto:", 7)) { minLen = 10; }
        else if (matchPrefixCI(i, "www.", 4)) { isWww = true; minLen = 7; }
        else {
            ++i;
            continue;
        }

        // Advance until whitespace or hard delimiter
        std::size_t j = i;
        while (j < n) {
            const unsigned char c = static_cast<unsigned char>(textUtf8[j]);
            if (std::isspace(c) != 0 || c == '<' || c == '>' || c == '"' || c == '\'' || c < 0x20u) {
                break;
            }
            ++j;
        }

        // Trim trailing punctuation while preserving balanced parentheses/brackets (e.g. Wikipedia URLs)
        while (j > i) {
            const char last = textUtf8[j - 1];
            if (last == '.' || last == ',' || last == ';' || last == ':' || last == '!' || last == '?' || last == '}') {
                --j;
                continue;
            }
            if (last == ')') {
                std::size_t openCount = 0, closeCount = 0;
                for (std::size_t k = i; k < j; ++k) {
                    if (textUtf8[k] == '(') ++openCount;
                    else if (textUtf8[k] == ')') ++closeCount;
                }
                if (closeCount > openCount) { --j; continue; }
            }
            if (last == ']') {
                std::size_t openCount = 0, closeCount = 0;
                for (std::size_t k = i; k < j; ++k) {
                    if (textUtf8[k] == '[') ++openCount;
                    else if (textUtf8[k] == ']') ++closeCount;
                }
                if (closeCount > openCount) { --j; continue; }
            }
            break;
        }

        const std::size_t rawLen = (j > i) ? (j - i) : 0u;
        if (rawLen >= minLen) {
            const std::string raw = textUtf8.substr(i, rawLen);
            const std::string norm = isWww ? ("https://" + raw) : raw;
            bool hostValid = true;
            if (isWww) {
                // Require at least "www.a.b" (a dot after "www.")
                const std::size_t secondDot = raw.find('.', 4);
                hostValid = (secondDot != std::string::npos && secondDot > 4 && secondDot + 1 < raw.size());
            }
            if (hostValid && IsSafeExternalLinkUrl(norm)) {
                InlineUrlSpan span{};
                span.utf8Start = i;
                span.utf8Len = rawLen;
                span.utf16Start = utf8To16[i];
                span.utf16Len = utf8To16[i + rawLen] - utf8To16[i];
                span.rawTextUtf8 = raw;
                span.normalizedUrlUtf8 = norm;
                span.isYouTube = IsYouTubeUrlUtf8(norm);
                out.push_back(std::move(span));
                i = j;
                continue;
            }
        }
        ++i;
    }
    return out;
}

} // namespace DocStudio
