#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace DocStudio {

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (n <= 0) return {};
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

inline std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

} // namespace DocStudio

#else

namespace DocStudio {

inline std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    std::wstring out;
    out.reserve(utf8.size());
    const std::size_t n = utf8.size();
    for (std::size_t i = 0; i < n;) {
        const unsigned char b0 = static_cast<unsigned char>(utf8[i]);
        std::uint32_t cp = 0xFFFDu;
        std::size_t adv = 1;
        if (b0 < 0x80u) {
            cp = b0;
        } else if ((b0 & 0xE0u) == 0xC0u && i + 1 < n) {
            const unsigned char b1 = static_cast<unsigned char>(utf8[i + 1]);
            if ((b1 & 0xC0u) == 0x80u) {
                cp = (static_cast<std::uint32_t>(b0 & 0x1Fu) << 6) | static_cast<std::uint32_t>(b1 & 0x3Fu);
                if (cp < 0x80u) cp = 0xFFFDu;
                adv = 2;
            }
        } else if ((b0 & 0xF0u) == 0xE0u && i + 2 < n) {
            const unsigned char b1 = static_cast<unsigned char>(utf8[i + 1]);
            const unsigned char b2 = static_cast<unsigned char>(utf8[i + 2]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
                cp = (static_cast<std::uint32_t>(b0 & 0x0Fu) << 12) |
                     (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6) |
                     static_cast<std::uint32_t>(b2 & 0x3Fu);
                if (cp < 0x800u || (cp >= 0xD800u && cp <= 0xDFFFu)) cp = 0xFFFDu;
                adv = 3;
            }
        } else if ((b0 & 0xF8u) == 0xF0u && i + 3 < n) {
            const unsigned char b1 = static_cast<unsigned char>(utf8[i + 1]);
            const unsigned char b2 = static_cast<unsigned char>(utf8[i + 2]);
            const unsigned char b3 = static_cast<unsigned char>(utf8[i + 3]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
                cp = (static_cast<std::uint32_t>(b0 & 0x07u) << 18) |
                     (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12) |
                     (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6) |
                     static_cast<std::uint32_t>(b3 & 0x3Fu);
                if (cp < 0x10000u || cp > 0x10FFFFu) cp = 0xFFFDu;
                adv = 4;
            }
        }
        i += adv;
        if constexpr (sizeof(wchar_t) == 2) {
            if (cp <= 0xFFFFu) {
                out.push_back(static_cast<wchar_t>(cp));
            } else {
                cp -= 0x10000u;
                out.push_back(static_cast<wchar_t>(0xD800u + (cp >> 10)));
                out.push_back(static_cast<wchar_t>(0xDC00u + (cp & 0x3FFu)));
            }
        } else {
            out.push_back(static_cast<wchar_t>(cp));
        }
    }
    return out;
}

inline std::string WideToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    std::string out;
    out.reserve(wide.size());
    const std::size_t n = wide.size();
    for (std::size_t i = 0; i < n; ++i) {
        std::uint32_t cp = static_cast<std::uint32_t>(wide[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < n) {
                const std::uint32_t lo = static_cast<std::uint32_t>(wide[i + 1]);
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    cp = 0x10000u + (((cp - 0xD800u) << 10) | (lo - 0xDC00u));
                    ++i;
                }
            }
        }
        if (cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) cp = 0xFFFDu;
        if (cp <= 0x7Fu) {
            out.push_back(static_cast<char>(cp));
        } else if (cp <= 0x7FFu) {
            out.push_back(static_cast<char>(0xC0u | ((cp >> 6) & 0x1Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp <= 0xFFFFu) {
            out.push_back(static_cast<char>(0xE0u | ((cp >> 12) & 0x0Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | ((cp >> 18) & 0x07u)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}

} // namespace DocStudio
#endif
