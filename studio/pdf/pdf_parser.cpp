#include "pdf_parser.hpp"
#include "../core/utf_conv.hpp"

#include <zlib.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <sstream>
#include <unordered_set>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#else
#if !defined(__ANDROID__) && !defined(__APPLE__) && !defined(DOCSTUDIO_PORTABLE_NO_OPENSSL) && defined(__has_include)
#if __has_include(<openssl/evp.h>)
#define DOCSTUDIO_HAS_OPENSSL 1
#include <openssl/evp.h>
#endif
#endif
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace DocStudio {

namespace {

constexpr std::uint8_t kPdfStdPadding[32] = {
    0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41,
    0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
    0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80,
    0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A
};

bool ComputeMd5(const std::uint8_t* data, std::size_t len, std::uint8_t out16[16]) {
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    bool ok = false;
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_MD5_ALGORITHM, nullptr, 0))) {
        if (BCRYPT_SUCCESS(BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0))) {
            if (len == 0 || BCRYPT_SUCCESS(BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(len), 0))) {
                if (BCRYPT_SUCCESS(BCryptFinishHash(hHash, out16, 16, 0))) {
                    ok = true;
                }
            }
            BCryptDestroyHash(hHash);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    return ok;
#elif defined(DOCSTUDIO_HAS_OPENSSL)
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return false;
    unsigned int outLen = 0;
    const bool ok = (EVP_DigestInit_ex(ctx, EVP_md5(), nullptr) == 1) &&
                    (len == 0 || EVP_DigestUpdate(ctx, data, len) == 1) &&
                    (EVP_DigestFinal_ex(ctx, out16, &outLen) == 1) &&
                    (outLen == 16);
    EVP_MD_CTX_free(ctx);
    return ok;
#else
    // Self-contained RFC 1321 MD5 (Android / iOS / zero-OpenSSL POSIX builds)
    static constexpr std::uint32_t kS[64] = {
        7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,
        5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,
        4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,
        6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21
    };
    static constexpr std::uint32_t kK[64] = {
        0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
        0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
        0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
        0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
        0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
        0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
        0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
        0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u
    };
    const std::size_t newLen = ((len + 8) / 64 + 1) * 64;
    std::vector<std::uint8_t> msg(newLen, 0);
    if (len > 0 && data) std::memcpy(msg.data(), data, len);
    msg[len] = 0x80u;
    const std::uint64_t bitLen = static_cast<std::uint64_t>(len) * 8u;
    for (int i = 0; i < 8; ++i) msg[newLen - 8 + i] = static_cast<std::uint8_t>((bitLen >> (8 * i)) & 0xFFu);
    std::uint32_t a0 = 0x67452301u, b0 = 0xefcdab89u, c0 = 0x98badcfeu, d0 = 0x10325476u;
    for (std::size_t off = 0; off < newLen; off += 64) {
        std::uint32_t M[16];
        for (int i = 0; i < 16; ++i) {
            M[i] = static_cast<std::uint32_t>(msg[off + i * 4]) |
                   (static_cast<std::uint32_t>(msg[off + i * 4 + 1]) << 8) |
                   (static_cast<std::uint32_t>(msg[off + i * 4 + 2]) << 16) |
                   (static_cast<std::uint32_t>(msg[off + i * 4 + 3]) << 24);
        }
        std::uint32_t A = a0, B = b0, C = c0, D = d0;
        for (int i = 0; i < 64; ++i) {
            std::uint32_t F = 0, g = 0;
            if (i < 16) { F = (B & C) | ((~B) & D); g = static_cast<std::uint32_t>(i); }
            else if (i < 32) { F = (D & B) | ((~D) & C); g = static_cast<std::uint32_t>((5 * i + 1) % 16); }
            else if (i < 48) { F = B ^ C ^ D; g = static_cast<std::uint32_t>((3 * i + 5) % 16); }
            else { F = C ^ (B | (~D)); g = static_cast<std::uint32_t>((7 * i) % 16); }
            F += A + kK[i] + M[g];
            A = D; D = C; C = B;
            B += (F << kS[i]) | (F >> (32 - kS[i]));
        }
        a0 += A; b0 += B; c0 += C; d0 += D;
    }
    const std::uint32_t words[4] = { a0, b0, c0, d0 };
    for (int i = 0; i < 4; ++i) {
        out16[i * 4 + 0] = static_cast<std::uint8_t>(words[i] & 0xFFu);
        out16[i * 4 + 1] = static_cast<std::uint8_t>((words[i] >> 8) & 0xFFu);
        out16[i * 4 + 2] = static_cast<std::uint8_t>((words[i] >> 16) & 0xFFu);
        out16[i * 4 + 3] = static_cast<std::uint8_t>((words[i] >> 24) & 0xFFu);
    }
    return true;
#endif
}

bool Rc4Crypt(const std::uint8_t* key, std::size_t keyLen,
              const std::uint8_t* in, std::size_t len,
              std::vector<std::uint8_t>& out) {
    if (!key || keyLen == 0) return false;
    std::uint8_t s[256];
    for (int i = 0; i < 256; ++i) s[i] = static_cast<std::uint8_t>(i);
    int j = 0;
    for (int i = 0; i < 256; ++i) {
        j = (j + s[i] + key[static_cast<std::size_t>(i) % keyLen]) & 0xFF;
        std::swap(s[i], s[j]);
    }
    out.resize(len);
    int i = 0;
    j = 0;
    for (std::size_t k = 0; k < len; ++k) {
        i = (i + 1) & 0xFF;
        j = (j + s[i]) & 0xFF;
        std::swap(s[i], s[j]);
        const std::uint8_t rnd = s[(s[i] + s[j]) & 0xFF];
        out[k] = in[k] ^ rnd;
    }
    return true;
}

bool Aes128CbcDecrypt(const std::uint8_t* key16,
                      const std::uint8_t* in, std::size_t len,
                      std::vector<std::uint8_t>& out) {
    if (!key16) return false;
    if (len == 0) {
        out.clear();
        return true;
    }
    if (len < 16 || ((len - 16) % 16) != 0) {
        return false;
    }
    if (len == 16) {
        out.clear();
        return true;
    }

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    bool ok = false;
    if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0))) {
        const wchar_t* mode = BCRYPT_CHAIN_MODE_CBC;
        if (BCRYPT_SUCCESS(BCryptSetProperty(
                hAlg,
                BCRYPT_CHAINING_MODE,
                reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(mode)),
                static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_CBC)),
                0))) {
            if (BCRYPT_SUCCESS(BCryptGenerateSymmetricKey(
                    hAlg, &hKey, nullptr, 0, const_cast<PUCHAR>(key16), 16, 0))) {
                std::uint8_t iv[16];
                std::memcpy(iv, in, 16);
                out.resize(len - 16);
                ULONG cbOut = 0;
                const NTSTATUS st = BCryptDecrypt(
                    hKey,
                    const_cast<PUCHAR>(in + 16),
                    static_cast<ULONG>(len - 16),
                    nullptr,
                    iv,
                    16,
                    out.data(),
                    static_cast<ULONG>(out.size()),
                    &cbOut,
                    BCRYPT_BLOCK_PADDING);
                if (BCRYPT_SUCCESS(st) && cbOut <= out.size()) {
                    out.resize(cbOut);
                    ok = true;
                }
                BCryptDestroyKey(hKey);
            }
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    return ok;
#elif defined(DOCSTUDIO_HAS_OPENSSL)
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;
    out.resize((len - 16) + 16);
    int outLen1 = 0;
    int outLen2 = 0;
    const bool ok = (EVP_DecryptInit_ex(ctx, EVP_aes_128_cbc(), nullptr, key16, in) == 1) &&
                    (EVP_DecryptUpdate(ctx, out.data(), &outLen1, in + 16, static_cast<int>(len - 16)) == 1) &&
                    (EVP_DecryptFinal_ex(ctx, out.data() + outLen1, &outLen2) == 1);
    EVP_CIPHER_CTX_free(ctx);
    if (ok && outLen1 >= 0 && outLen2 >= 0) {
        out.resize(static_cast<std::size_t>(outLen1 + outLen2));
        return true;
    }
    out.clear();
    return false;
#else
    (void)key16; (void)in; (void)len;
    out.clear();
    return false;
#endif
}

inline bool IsPdfWhitespace(std::uint8_t c) noexcept {
    return c == 0x00 || c == 0x09 || c == 0x0A || c == 0x0C || c == 0x0D || c == 0x20;
}

inline bool IsPdfDelimiter(std::uint8_t c) noexcept {
    return c == '(' || c == ')' || c == '<' || c == '>' ||
           c == '[' || c == ']' || c == '{' || c == '}' ||
           c == '/' || c == '%';
}

inline int HexVal(std::uint8_t c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void AppendUtf8(std::string& out, std::uint32_t cp) {
    if (cp == 0 || cp > 0x10FFFFu) return;
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

std::string Utf16BeBytesToUtf8(const std::string& bytes) {
    std::string out;
    for (std::size_t i = 0; i + 1 < bytes.size(); i += 2) {
        const std::uint16_t u1 = (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[i])) << 8) |
                                 static_cast<std::uint8_t>(bytes[i + 1]);
        if (u1 >= 0xD800u && u1 <= 0xDBFFu && i + 3 < bytes.size()) {
            const std::uint16_t u2 = (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[i + 2])) << 8) |
                                     static_cast<std::uint8_t>(bytes[i + 3]);
            if (u2 >= 0xDC00u && u2 <= 0xDFFFu) {
                const std::uint32_t cp = 0x10000u + ((static_cast<std::uint32_t>(u1 - 0xD800u) << 10) | (u2 - 0xDC00u));
                AppendUtf8(out, cp);
                i += 2;
                continue;
            }
        }
        AppendUtf8(out, u1);
    }
    return out;
}

std::string WinAnsiByteToUtf8(std::uint8_t b) {
    if (b < 0x80u) {
        return std::string(1, static_cast<char>(b));
    }
    static constexpr std::uint16_t kWin1252Extra[32] = {
        0x20AC, 0x0020, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x0020, 0x017D, 0x0020,
        0x0020, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x0020, 0x017E, 0x0178
    };
    std::uint32_t cp = (b >= 0xA0u) ? static_cast<std::uint32_t>(b) : static_cast<std::uint32_t>(kWin1252Extra[b - 0x80u]);
    std::string out;
    AppendUtf8(out, cp);
    return out;
}

class PdfReader {
public:
    PdfReader(const std::uint8_t* data, std::size_t size, std::size_t pos = 0)
        : data_(data), size_(size), pos_(pos) {}

    std::size_t Pos() const noexcept { return pos_; }
    void SetPos(std::size_t p) noexcept { pos_ = (std::min)(p, size_); }
    bool Eof() const noexcept { return pos_ >= size_; }

    void SkipWsAndComments() noexcept {
        while (pos_ < size_) {
            const std::uint8_t c = data_[pos_];
            if (IsPdfWhitespace(c)) {
                ++pos_;
            } else if (c == '%') {
                while (pos_ < size_ && data_[pos_] != '\n' && data_[pos_] != '\r') ++pos_;
            } else {
                break;
            }
        }
    }

    std::string ReadToken() {
        SkipWsAndComments();
        if (pos_ >= size_) return {};
        const std::uint8_t c = data_[pos_];
        if (c == '<' && pos_ + 1 < size_ && data_[pos_ + 1] == '<') {
            pos_ += 2;
            return "<<";
        }
        if (c == '>' && pos_ + 1 < size_ && data_[pos_ + 1] == '>') {
            pos_ += 2;
            return ">>";
        }
        if (IsPdfDelimiter(c)) {
            ++pos_;
            return std::string(1, static_cast<char>(c));
        }
        const std::size_t start = pos_;
        while (pos_ < size_ && !IsPdfWhitespace(data_[pos_]) && !IsPdfDelimiter(data_[pos_])) {
            ++pos_;
        }
        return std::string(reinterpret_cast<const char*>(data_ + start), pos_ - start);
    }

    std::string PeekToken() {
        const std::size_t saved = pos_;
        std::string t = ReadToken();
        pos_ = saved;
        return t;
    }

    PdfValue ParseValue(int depth = 0, bool allowStream = true) {
        PdfValue val;
        if (depth > 64) return val;
        SkipWsAndComments();
        if (pos_ >= size_) return val;

        const std::uint8_t c = data_[pos_];
        if (c == '/') {
            ++pos_;
            val.kind = PdfValue::Kind::Name;
            while (pos_ < size_ && !IsPdfWhitespace(data_[pos_]) && !IsPdfDelimiter(data_[pos_])) {
                if (data_[pos_] == '#' && pos_ + 2 < size_) {
                    const int hi = HexVal(data_[pos_ + 1]);
                    const int lo = HexVal(data_[pos_ + 2]);
                    if (hi >= 0 && lo >= 0) {
                        val.strVal.push_back(static_cast<char>((hi << 4) | lo));
                        pos_ += 3;
                        continue;
                    }
                }
                val.strVal.push_back(static_cast<char>(data_[pos_++]));
            }
            return val;
        }
        if (c == '(') {
            ++pos_;
            val.kind = PdfValue::Kind::String;
            int parens = 1;
            while (pos_ < size_ && parens > 0) {
                const std::uint8_t ch = data_[pos_++];
                if (ch == '\\' && pos_ < size_) {
                    const std::uint8_t esc = data_[pos_++];
                    switch (esc) {
                    case 'n': val.strVal.push_back('\n'); break;
                    case 'r': val.strVal.push_back('\r'); break;
                    case 't': val.strVal.push_back('\t'); break;
                    case 'b': val.strVal.push_back('\b'); break;
                    case 'f': val.strVal.push_back('\f'); break;
                    case '\r': if (pos_ < size_ && data_[pos_] == '\n') ++pos_; break;
                    case '\n': break;
                    default:
                        if (esc >= '0' && esc <= '7') {
                            int oct = esc - '0';
                            for (int k = 0; k < 2 && pos_ < size_ && data_[pos_] >= '0' && data_[pos_] <= '7'; ++k) {
                                oct = (oct << 3) | (data_[pos_++] - '0');
                            }
                            val.strVal.push_back(static_cast<char>(oct & 0xFF));
                        } else {
                            val.strVal.push_back(static_cast<char>(esc));
                        }
                        break;
                    }
                } else if (ch == '(') {
                    ++parens;
                    val.strVal.push_back('(');
                } else if (ch == ')') {
                    --parens;
                    if (parens > 0) val.strVal.push_back(')');
                } else {
                    val.strVal.push_back(static_cast<char>(ch));
                }
            }
            return val;
        }
        if (c == '<') {
            if (pos_ + 1 < size_ && data_[pos_ + 1] == '<') {
                pos_ += 2;
                PdfDict dict;
                while (true) {
                    SkipWsAndComments();
                    if (pos_ >= size_) break;
                    if (pos_ + 1 < size_ && data_[pos_] == '>' && data_[pos_ + 1] == '>') {
                        pos_ += 2;
                        break;
                    }
                    PdfValue key = ParseValue(depth + 1, false);
                    if (key.kind != PdfValue::Kind::Name) {
                        // Recover to '>>'
                        while (pos_ + 1 < size_ && !(data_[pos_] == '>' && data_[pos_ + 1] == '>')) ++pos_;
                        if (pos_ + 1 < size_) pos_ += 2;
                        break;
                    }
                    PdfValue item = ParseValue(depth + 1, false);
                    dict[key.strVal] = std::move(item);
                }
                if (allowStream) {
                    const std::size_t beforeKw = pos_;
                    SkipWsAndComments();
                    if (pos_ + 6 <= size_ && std::memcmp(data_ + pos_, "stream", 6) == 0 &&
                        (pos_ + 6 == size_ || IsPdfWhitespace(data_[pos_ + 6]))) {
                        pos_ += 6;
                        if (pos_ < size_ && data_[pos_] == '\r') ++pos_;
                        if (pos_ < size_ && data_[pos_] == '\n') ++pos_;
                        const std::size_t streamStart = pos_;
                        std::size_t streamLen = 0;
                        bool hasLen = false;
                        auto lenIt = dict.find("Length");
                        if (lenIt != dict.end() && lenIt->second.kind == PdfValue::Kind::Int && lenIt->second.intVal >= 0) {
                            streamLen = static_cast<std::size_t>(lenIt->second.intVal);
                            hasLen = (streamStart + streamLen <= size_);
                        }
                        if (!hasLen) {
                            // Scan for endstream marker safely
                            std::size_t p = streamStart;
                            while (p + 9 <= size_) {
                                if (std::memcmp(data_ + p, "endstream", 9) == 0 &&
                                    (p == streamStart || data_[p - 1] == '\n' || data_[p - 1] == '\r' || data_[p - 1] == ' ')) {
                                    break;
                                }
                                ++p;
                            }
                            if (p > streamStart && data_[p - 1] == '\n') --p;
                            if (p > streamStart && data_[p - 1] == '\r') --p;
                            streamLen = p - streamStart;
                        }
                        auto sd = std::make_shared<PdfStreamData>();
                        sd->dict = std::move(dict);
                        sd->rawBytes.assign(data_ + streamStart, data_ + (std::min)(size_, streamStart + streamLen));
                        pos_ = (std::min)(size_, streamStart + streamLen);
                        val.kind = PdfValue::Kind::Stream;
                        val.streamVal = std::move(sd);
                        return val;
                    }
                    pos_ = beforeKw;
                }
                val.kind = PdfValue::Kind::Dict;
                val.dictVal = std::move(dict);
                return val;
            }
            // Hex string
            ++pos_;
            val.kind = PdfValue::Kind::String;
            int hi = -1;
            while (pos_ < size_) {
                const std::uint8_t ch = data_[pos_++];
                if (ch == '>') break;
                if (IsPdfWhitespace(ch)) continue;
                const int hv = HexVal(ch);
                if (hv >= 0) {
                    if (hi < 0) hi = hv;
                    else {
                        val.strVal.push_back(static_cast<char>((hi << 4) | hv));
                        hi = -1;
                    }
                }
            }
            if (hi >= 0) val.strVal.push_back(static_cast<char>(hi << 4));
            return val;
        }
        if (c == '[') {
            ++pos_;
            val.kind = PdfValue::Kind::Array;
            while (true) {
                SkipWsAndComments();
                if (pos_ >= size_) break;
                if (data_[pos_] == ']') {
                    ++pos_;
                    break;
                }
                val.arrayVal.push_back(ParseValue(depth + 1, false));
            }
            return val;
        }

        const std::string tok = ReadToken();
        if (tok == "true") { val.kind = PdfValue::Kind::Bool; val.boolVal = true; return val; }
        if (tok == "false") { val.kind = PdfValue::Kind::Bool; val.boolVal = false; return val; }
        if (tok == "null") { val.kind = PdfValue::Kind::Null; return val; }

        if (!tok.empty() && (std::isdigit(static_cast<unsigned char>(tok[0])) || tok[0] == '+' || tok[0] == '-' || tok[0] == '.')) {
            const bool hasDot = (tok.find('.') != std::string::npos);
            if (!hasDot) {
                char* endp = nullptr;
                const long long iv = std::strtoll(tok.c_str(), &endp, 10);
                if (endp && *endp == '\0') {
                    // Check "<gen> R" lookahead without clobbering
                    if (iv > 0) {
                        const std::size_t saved = pos_;
                        const std::string t2 = ReadToken();
                        const std::string t3 = ReadToken();
                        if (t3 == "R" && !t2.empty() && std::isdigit(static_cast<unsigned char>(t2[0]))) {
                            char* e2 = nullptr;
                            const long long gen = std::strtoll(t2.c_str(), &e2, 10);
                            if (e2 && *e2 == '\0' && gen >= 0) {
                                val.kind = PdfValue::Kind::Ref;
                                val.refVal = { static_cast<std::uint32_t>(iv), static_cast<std::uint16_t>(gen) };
                                return val;
                            }
                        }
                        pos_ = saved;
                    }
                    val.kind = PdfValue::Kind::Int;
                    val.intVal = static_cast<std::int64_t>(iv);
                    return val;
                }
            }
            char* endp = nullptr;
            const double dv = std::strtod(tok.c_str(), &endp);
            if (endp && *endp == '\0') {
                val.kind = PdfValue::Kind::Real;
                val.realVal = dv;
                return val;
            }
        }

        // Unknown keyword as Name fallback (or Null)
        val.kind = PdfValue::Kind::Null;
        return val;
    }

private:
    const std::uint8_t* data_;
    std::size_t size_;
    std::size_t pos_;
};

bool InflateZlibStream(const std::vector<std::uint8_t>& compressed, std::vector<std::uint8_t>& out) {
    if (compressed.empty()) {
        out.clear();
        return true;
    }
    auto tryInflate = [&](int windowBits) -> bool {
        z_stream zs{};
        if (inflateInit2(&zs, windowBits) != Z_OK) return false;
        zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(compressed.data()));
        zs.avail_in = static_cast<uInt>(compressed.size());
        out.clear();
        std::uint8_t buf[16384];
        int rc = Z_OK;
        constexpr std::size_t kMaxDecompressed = 64u * 1024u * 1024u;
        do {
            zs.next_out = buf;
            zs.avail_out = sizeof(buf);
            rc = inflate(&zs, Z_NO_FLUSH);
            const std::size_t produced = sizeof(buf) - zs.avail_out;
            if (out.size() + produced > kMaxDecompressed) {
                inflateEnd(&zs);
                return false;
            }
            out.insert(out.end(), buf, buf + produced);
        } while (rc == Z_OK);
        inflateEnd(&zs);
        return (rc == Z_STREAM_END) || (!out.empty() && rc == Z_BUF_ERROR);
    };

    if (tryInflate(MAX_WBITS)) return true;
    return tryInflate(-MAX_WBITS); // raw deflate fallback
}

bool UnfilterPngPredictor(
    const std::vector<std::uint8_t>& in,
    int columns,
    int colors,
    int bpc,
    std::vector<std::uint8_t>& out)
{
    const std::size_t rowBytes = (static_cast<std::size_t>(columns) * static_cast<std::size_t>(colors) * static_cast<std::size_t>(bpc) + 7u) / 8u;
    const std::size_t stride = rowBytes + 1u;
    if (rowBytes == 0 || in.size() < stride) {
        out = in;
        return true;
    }
    const std::size_t bpp = (std::max<std::size_t>)(1u, (static_cast<std::size_t>(colors) * static_cast<std::size_t>(bpc) + 7u) / 8u);
    const std::size_t rows = in.size() / stride;
    out.assign(rows * rowBytes, 0);
    for (std::size_t r = 0; r < rows; ++r) {
        const std::uint8_t filter = in[r * stride];
        const std::uint8_t* src = &in[r * stride + 1];
        std::uint8_t* dst = &out[r * rowBytes];
        const std::uint8_t* up = (r > 0) ? &out[(r - 1) * rowBytes] : nullptr;
        for (std::size_t i = 0; i < rowBytes; ++i) {
            const std::uint8_t raw = src[i];
            const std::uint8_t a = (i >= bpp) ? dst[i - bpp] : 0;
            const std::uint8_t b = up ? up[i] : 0;
            const std::uint8_t c = (up && i >= bpp) ? up[i - bpp] : 0;
            std::uint8_t val = raw;
            switch (filter) {
            case 0: val = raw; break;
            case 1: val = static_cast<std::uint8_t>(raw + a); break;
            case 2: val = static_cast<std::uint8_t>(raw + b); break;
            case 3: val = static_cast<std::uint8_t>(raw + ((static_cast<int>(a) + static_cast<int>(b)) >> 1)); break;
            case 4: {
                const int p = static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
                const int pa = std::abs(p - static_cast<int>(a));
                const int pb = std::abs(p - static_cast<int>(b));
                const int pc = std::abs(p - static_cast<int>(c));
                const int pr = (pa <= pb && pa <= pc) ? a : ((pb <= pc) ? b : c);
                val = static_cast<std::uint8_t>(raw + pr);
                break;
            }
            default: val = raw; break;
            }
            dst[i] = val;
        }
    }
    return true;
}

std::uint64_t ReadUintBE(const std::uint8_t* p, std::size_t bytes) noexcept {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < bytes; ++i) {
        v = (v << 8) | p[i];
    }
    return v;
}

DocRect ParsePdfRect(const PdfValue* arrVal, const DocRect& fallback) noexcept {
    if (!arrVal || !arrVal->IsArray() || arrVal->arrayVal.size() < 4) return fallback;
    const double x1 = arrVal->arrayVal[0].AsNumber(fallback.x);
    const double y1 = arrVal->arrayVal[1].AsNumber(fallback.y);
    const double x2 = arrVal->arrayVal[2].AsNumber(fallback.Right());
    const double y2 = arrVal->arrayVal[3].AsNumber(fallback.Bottom());
    return DocRect::FromMinMax((std::min)(x1, x2), (std::min)(y1, y2), (std::max)(x1, x2), (std::max)(y1, y2));
}

struct ToUnicodeMap {
    std::unordered_map<std::uint32_t, std::string> codeToUtf8;
    int maxCodeBytes = 1;
    bool isTwoByteDefault = false;

    void ParseFromStreamBytes(const std::vector<std::uint8_t>& bytes) {
        const std::string txt(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        auto parseHexNum = [](const std::string& tok, std::uint32_t& val, int& byteLen) -> bool {
            if (tok.size() < 3 || tok.front() != '<' || tok.back() != '>') return false;
            const std::string inner = tok.substr(1, tok.size() - 2);
            if (inner.empty() || inner.size() > 8) return false;
            val = 0;
            for (char ch : inner) {
                const int hv = HexVal(static_cast<std::uint8_t>(ch));
                if (hv < 0) return false;
                val = (val << 4) | static_cast<std::uint32_t>(hv);
            }
            byteLen = static_cast<int>((inner.size() + 1) / 2);
            return true;
        };
        auto parseHexUtf16 = [](const std::string& tok) -> std::string {
            if (tok.size() < 3 || tok.front() != '<' || tok.back() != '>') return {};
            std::string raw;
            int hi = -1;
            for (std::size_t i = 1; i + 1 < tok.size(); ++i) {
                const int hv = HexVal(static_cast<std::uint8_t>(tok[i]));
                if (hv < 0) continue;
                if (hi < 0) hi = hv;
                else { raw.push_back(static_cast<char>((hi << 4) | hv)); hi = -1; }
            }
            if (hi >= 0) raw.push_back(static_cast<char>(hi << 4));
            return Utf16BeBytesToUtf8(raw);
        };

        std::vector<std::string> tokens;
        for (std::size_t i = 0; i < txt.size();) {
            const unsigned char c = static_cast<unsigned char>(txt[i]);
            if (IsPdfWhitespace(c)) { ++i; continue; }
            if (c == '<') {
                std::size_t j = i + 1;
                while (j < txt.size() && txt[j] != '>') ++j;
                if (j < txt.size()) ++j;
                tokens.push_back(txt.substr(i, j - i));
                i = j;
            } else if (c == '[' || c == ']') {
                tokens.push_back(std::string(1, static_cast<char>(c)));
                ++i;
            } else {
                std::size_t j = i;
                while (j < txt.size() && !IsPdfWhitespace(static_cast<std::uint8_t>(txt[j])) && txt[j] != '<' && txt[j] != '[' && txt[j] != ']') ++j;
                tokens.push_back(txt.substr(i, j - i));
                i = j;
            }
        }

        for (std::size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "beginbfchar") {
                ++i;
                while (i + 1 < tokens.size() && tokens[i] != "endbfchar") {
                    std::uint32_t code = 0;
                    int blen = 1;
                    if (parseHexNum(tokens[i], code, blen)) {
                        if (blen > maxCodeBytes) maxCodeBytes = blen;
                        if (blen >= 2) isTwoByteDefault = true;
                        const std::string mapped = parseHexUtf16(tokens[i + 1]);
                        if (!mapped.empty()) codeToUtf8[code] = mapped;
                    }
                    i += 2;
                }
            } else if (tokens[i] == "beginbfrange") {
                ++i;
                while (i + 2 < tokens.size() && tokens[i] != "endbfrange") {
                    std::uint32_t lo = 0, hi = 0;
                    int b1 = 1, b2 = 1;
                    if (!parseHexNum(tokens[i], lo, b1) || !parseHexNum(tokens[i + 1], hi, b2)) {
                        ++i;
                        continue;
                    }
                    if (b1 > maxCodeBytes) maxCodeBytes = b1;
                    if (b1 >= 2) isTwoByteDefault = true;
                    if (hi < lo || (hi - lo) > 4096) {
                        i += 3;
                        continue;
                    }
                    if (tokens[i + 2] == "[") {
                        std::size_t k = i + 3;
                        std::uint32_t code = lo;
                        while (k < tokens.size() && tokens[k] != "]" && code <= hi) {
                            const std::string mapped = parseHexUtf16(tokens[k]);
                            if (!mapped.empty()) codeToUtf8[code] = mapped;
                            ++code;
                            ++k;
                        }
                        i = (k < tokens.size()) ? (k + 1) : tokens.size();
                    } else {
                        const std::string firstStr = parseHexUtf16(tokens[i + 2]);
                        std::uint32_t startCp = 0;
                        if (tokens[i + 2].size() >= 6 && tokens[i + 2].front() == '<') {
                            int dummy = 0;
                            parseHexNum(tokens[i + 2], startCp, dummy);
                        }
                        if (!firstStr.empty() && startCp > 0) {
                            for (std::uint32_t offset = 0; offset <= (hi - lo); ++offset) {
                                std::string s;
                                AppendUtf8(s, startCp + offset);
                                codeToUtf8[lo + offset] = std::move(s);
                            }
                        }
                        i += 3;
                    }
                }
            }
        }
    }

    std::string DecodePdfString(const std::string& rawBytes) const {
        if (rawBytes.empty()) return {};
        if (!codeToUtf8.empty()) {
            std::string out;
            const int step = isTwoByteDefault ? 2 : 1;
            for (std::size_t i = 0; i < rawBytes.size();) {
                std::uint32_t code = 0;
                std::size_t advance = 1;
                if (step == 2 && i + 1 < rawBytes.size()) {
                    code = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(rawBytes[i])) << 8) |
                           static_cast<std::uint8_t>(rawBytes[i + 1]);
                    advance = 2;
                } else {
                    code = static_cast<std::uint8_t>(rawBytes[i]);
                    advance = 1;
                }
                auto it = codeToUtf8.find(code);
                if (it != codeToUtf8.end()) {
                    out += it->second;
                } else if (advance == 1) {
                    out += WinAnsiByteToUtf8(static_cast<std::uint8_t>(code));
                }
                i += advance;
            }
            return out;
        }
        // Raw UTF-16BE BOM check
        if (rawBytes.size() >= 2 &&
            static_cast<std::uint8_t>(rawBytes[0]) == 0xFEu &&
            static_cast<std::uint8_t>(rawBytes[1]) == 0xFFu) {
            return Utf16BeBytesToUtf8(rawBytes.substr(2));
        }
        std::string out;
        out.reserve(rawBytes.size());
        for (unsigned char ch : rawBytes) {
            out += WinAnsiByteToUtf8(ch);
        }
        return out;
    }
};

} // namespace

std::string NormalizeLigaturesUtf8(const std::string& inUtf8) {
    std::string out;
    out.reserve(inUtf8.size());
    for (std::size_t i = 0; i < inUtf8.size();) {
        const std::uint8_t b0 = static_cast<std::uint8_t>(inUtf8[i]);
        if (b0 == 0xEFu && i + 2 < inUtf8.size() && static_cast<std::uint8_t>(inUtf8[i + 1]) == 0xACu) {
            const std::uint8_t b2 = static_cast<std::uint8_t>(inUtf8[i + 2]);
            if (b2 == 0x80u) { out += "ff"; i += 3; continue; }
            if (b2 == 0x81u) { out += "fi"; i += 3; continue; }
            if (b2 == 0x82u) { out += "fl"; i += 3; continue; }
            if (b2 == 0x83u) { out += "ffi"; i += 3; continue; }
            if (b2 == 0x84u) { out += "ffl"; i += 3; continue; }
        }
        out.push_back(inUtf8[i++]);
    }
    return out;
}

bool PdfDocument::LoadFromFile(const std::wstring& path, std::string* outErr) {
#ifdef _WIN32
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        if (outErr) *outErr = "No se pudo abrir archivo PDF (WinError " + std::to_string(GetLastError()) + ").";
        return false;
    }
    LARGE_INTEGER li{};
    if (!GetFileSizeEx(hFile, &li) || li.QuadPart < 8 || li.QuadPart > 512ll * 1024ll * 1024ll) {
        CloseHandle(hFile);
        if (outErr) *outErr = "Tama\xC3\xB1o de archivo PDF inv\xC3\xA1lido.";
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(li.QuadPart));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD chunk = static_cast<DWORD>((std::min<std::size_t>)(bytes.size() - offset, 4u * 1024u * 1024u));
        DWORD readNow = 0;
        if (!ReadFile(hFile, bytes.data() + offset, chunk, &readNow, nullptr) || readNow == 0) {
            CloseHandle(hFile);
            if (outErr) *outErr = "Error leyendo archivo PDF.";
            return false;
        }
        offset += readNow;
    }
    CloseHandle(hFile);
    return LoadFromBytes(std::move(bytes), outErr);
#else
    const std::string utf8Path = WideToUtf8(path);
    const int fd = ::open(utf8Path.c_str(), O_RDONLY);
    if (fd < 0) {
        if (outErr) *outErr = "No se pudo abrir archivo PDF (errno " + std::to_string(errno) + ").";
        return false;
    }
    struct stat st{};
    if (::fstat(fd, &st) != 0 || st.st_size < 8 || st.st_size > 512ll * 1024ll * 1024ll) {
        ::close(fd);
        if (outErr) *outErr = "Tama\xC3\xB1o de archivo PDF inv\xC3\xA1lido.";
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(st.st_size));
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const std::size_t chunk = (std::min<std::size_t>)(bytes.size() - offset, 4u * 1024u * 1024u);
        const ssize_t readNow = ::read(fd, bytes.data() + offset, chunk);
        if (readNow < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            if (outErr) *outErr = "Error leyendo archivo PDF.";
            return false;
        }
        if (readNow == 0) {
            ::close(fd);
            if (outErr) *outErr = "Error leyendo archivo PDF (EOF inesperado).";
            return false;
        }
        offset += static_cast<std::size_t>(readNow);
    }
    ::close(fd);
    return LoadFromBytes(std::move(bytes), outErr);
#endif
}

bool PdfDocument::LoadFromBytes(std::vector<std::uint8_t> bytes, std::string* outErr) {
    loaded_ = false;
    data_ = std::move(bytes);
    xref_.clear();
    cache_.clear();
    objStmCache_.clear();
    pages_.clear();
    rootRef_ = {};
    hasEncryptRef_ = false;
    encryptRef_ = {};
    fileId0_.clear();
    encrypted_ = false;
    encryptR_ = 0;
    fileKey_.clear();
    stmEncrypted_ = false;
    strEncrypted_ = false;
    stmUseAes_ = false;
    strUseAes_ = false;
    encryptMetadata_ = true;

    if (data_.size() < 16) {
        if (outErr) *outErr = "Archivo PDF demasiado peque\xC3\xB1o o vac\xC3\xADo.";
        return false;
    }

    // Locate "%PDF-" near start (first 1024 bytes)
    std::size_t headerPos = std::string::npos;
    const std::size_t hdrScan = (std::min<std::size_t>)(data_.size(), 1024u);
    for (std::size_t i = 0; i + 5 <= hdrScan; ++i) {
        if (std::memcmp(data_.data() + i, "%PDF-", 5) == 0) {
            headerPos = i;
            break;
        }
    }
    if (headerPos == std::string::npos) {
        if (outErr) *outErr = "Cabecera %PDF- no encontrada.";
        return false;
    }
    if (headerPos + 8 <= data_.size()) {
        version_.assign(reinterpret_cast<const char*>(data_.data() + headerPos + 5), 3);
    }

    // Find startxref from tail
    const std::size_t tailLen = (std::min<std::size_t>)(data_.size(), 2048u);
    const std::size_t tailStart = data_.size() - tailLen;
    std::size_t startXrefTokenPos = std::string::npos;
    for (std::size_t i = data_.size(); i >= tailStart + 9; --i) {
        if (std::memcmp(data_.data() + (i - 9), "startxref", 9) == 0) {
            startXrefTokenPos = i - 9;
            break;
        }
    }
    if (startXrefTokenPos == std::string::npos) {
        if (outErr) *outErr = "Marcador startxref no encontrado.";
        return false;
    }

    PdfReader r(data_.data(), data_.size(), startXrefTokenPos + 9);
    const std::string offStr = r.ReadToken();
    char* endp = nullptr;
    const long long startXrefOff = std::strtoll(offStr.c_str(), &endp, 10);
    if (!endp || *endp != '\0' || startXrefOff < 0 || static_cast<std::size_t>(startXrefOff) >= data_.size()) {
        if (outErr) *outErr = "Offset startxref inv\xC3\xA1lido.";
        return false;
    }

    if (!ParseTrailerAndXref(static_cast<std::size_t>(startXrefOff), outErr)) {
        return false;
    }

    if (rootRef_.obj == 0) {
        if (outErr) *outErr = "Cat\xC3\xA1logo /Root no encontrado en trailer PDF.";
        return false;
    }

    if (!InitializeEncryption(outErr)) {
        return false;
    }

    PdfValue catalogRef;
    catalogRef.kind = PdfValue::Kind::Ref;
    catalogRef.refVal = rootRef_;
    const PdfValue* catalog = Resolve(&catalogRef);
    const PdfValue* pagesNode = catalog ? catalog->DictGet("Pages") : nullptr;
    if (!pagesNode || !pagesNode->IsRef()) {
        if (outErr) *outErr = "Nodo ra\xC3\xADz /Pages no encontrado en PDF.";
        return false;
    }

    std::vector<std::uint32_t> visited;
    CollectPages(pagesNode->refVal, { 0.0, 0.0, 595.0, 842.0 }, { 0.0, 0.0, 595.0, 842.0 }, false, 0, nullptr, visited, 0);
    loaded_ = true;
    return true;
}

bool PdfDocument::ParseTrailerAndXref(std::size_t startOffset, std::string* outErr) {
    std::unordered_set<std::size_t> visitedOffsets;
    std::size_t curOffset = startOffset;
    bool first = true;

    while (curOffset < data_.size() && visitedOffsets.insert(curOffset).second) {
        PdfReader r(data_.data(), data_.size(), curOffset);
        r.SkipWsAndComments();
        PdfDict trailer;
        if (r.Pos() + 4 <= data_.size() && std::memcmp(data_.data() + r.Pos(), "xref", 4) == 0) {
            if (!ParseClassicXref(r.Pos(), trailer, outErr)) return false;
        } else {
            if (!ParseXrefStream(curOffset, trailer, outErr)) return false;
        }
        if (first || rootRef_.obj == 0) {
            auto rootIt = trailer.find("Root");
            if (rootIt != trailer.end() && rootIt->second.IsRef()) rootRef_ = rootIt->second.refVal;
        }
        if (!hasEncryptRef_) {
            auto encIt = trailer.find("Encrypt");
            if (encIt != trailer.end() && encIt->second.IsRef()) {
                hasEncryptRef_ = true;
                encryptRef_ = encIt->second.refVal;
            }
        }
        if (fileId0_.empty()) {
            auto idIt = trailer.find("ID");
            if (idIt != trailer.end() && idIt->second.kind == PdfValue::Kind::Array &&
                !idIt->second.arrayVal.empty() && idIt->second.arrayVal[0].kind == PdfValue::Kind::String) {
                fileId0_ = idIt->second.arrayVal[0].strVal;
            }
        }
        first = false;
        auto prevIt = trailer.find("Prev");
        if (prevIt != trailer.end() && prevIt->second.kind == PdfValue::Kind::Int && prevIt->second.intVal > 0) {
            curOffset = static_cast<std::size_t>(prevIt->second.intVal);
            continue;
        }
        break;
    }
    return true;
}

bool PdfDocument::ParseClassicXref(std::size_t offset, PdfDict& outTrailer, std::string* outErr) {
    PdfReader r(data_.data(), data_.size(), offset);
    const std::string kw = r.ReadToken();
    if (kw != "xref") {
        if (outErr) *outErr = "Se esperaba secci\xC3\xB3n xref.";
        return false;
    }
    while (!r.Eof()) {
        const std::string t = r.PeekToken();
        if (t == "trailer") {
            r.ReadToken();
            PdfValue tr = r.ParseValue(0, false);
            if (tr.kind == PdfValue::Kind::Dict) {
                outTrailer = std::move(tr.dictVal);
                return true;
            }
            if (outErr) *outErr = "Trailer PDF inv\xC3\xA1lido.";
            return false;
        }
        const std::string startStr = r.ReadToken();
        const std::string countStr = r.ReadToken();
        if (startStr.empty() || countStr.empty()) break;
        const std::uint32_t startObj = static_cast<std::uint32_t>(std::strtoul(startStr.c_str(), nullptr, 10));
        const std::uint32_t count = static_cast<std::uint32_t>(std::strtoul(countStr.c_str(), nullptr, 10));
        if (count > 2000000u) {
            if (outErr) *outErr = "Conteo xref excesivo.";
            return false;
        }
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::string offTok = r.ReadToken();
            const std::string genTok = r.ReadToken();
            const std::string flagTok = r.ReadToken();
            if (flagTok.empty()) break;
            if (flagTok == "n") {
                const std::uint32_t objNum = startObj + i;
                if (xref_.find(objNum) == xref_.end()) {
                    XRefEntry e;
                    e.inUse = true;
                    e.compressed = false;
                    e.offsetOrObjStm = std::strtoull(offTok.c_str(), nullptr, 10);
                    e.genOrIndex = static_cast<std::uint32_t>(std::strtoul(genTok.c_str(), nullptr, 10));
                    xref_[objNum] = e;
                }
            } else if (flagTok == "f") {
                const std::uint32_t objNum = startObj + i;
                if (xref_.find(objNum) == xref_.end()) {
                    xref_[objNum] = XRefEntry{ false, false, 0, 0 };
                }
            }
        }
    }
    if (outErr) *outErr = "No se encontr\xC3\xB3 trailer tras tabla xref.";
    return false;
}

bool PdfDocument::ParseXrefStream(std::size_t offset, PdfDict& outTrailer, std::string* outErr) {
    PdfReader r(data_.data(), data_.size(), offset);
    r.ReadToken(); // objNum
    r.ReadToken(); // gen
    if (r.ReadToken() != "obj") {
        if (outErr) *outErr = "XRef stream inv\xC3\xA1lido (sin 'obj').";
        return false;
    }
    PdfValue v = r.ParseValue(0, true);
    if (!v.IsStream()) {
        if (outErr) *outErr = "Objeto XRef no es un stream v\xC3\xA1lido.";
        return false;
    }
    outTrailer = v.streamVal->dict;

    std::vector<std::uint8_t> rawXref;
    if (!DecompressStream(*v.streamVal, rawXref, outErr)) return false;

    // Decode W array [w0 w1 w2]
    auto wIt = outTrailer.find("W");
    if (wIt == outTrailer.end() || !wIt->second.IsArray() || wIt->second.arrayVal.size() < 3) {
        if (outErr) *outErr = "XRef stream sin arreglo /W v\xC3\xA1lido.";
        return false;
    }
    const std::size_t w0 = static_cast<std::size_t>(std::max<std::int64_t>(0, wIt->second.arrayVal[0].intVal));
    const std::size_t w1 = static_cast<std::size_t>(std::max<std::int64_t>(0, wIt->second.arrayVal[1].intVal));
    const std::size_t w2 = static_cast<std::size_t>(std::max<std::int64_t>(0, wIt->second.arrayVal[2].intVal));
    const std::size_t entryBytes = w0 + w1 + w2;
    if (entryBytes == 0 || entryBytes > 24) {
        if (outErr) *outErr = "Tama\xC3\xB1o de entrada /W de XRef inv\xC3\xA1lido.";
        return false;
    }

    std::vector<std::pair<std::uint32_t, std::uint32_t>> ranges;
    auto idxIt = outTrailer.find("Index");
    if (idxIt != outTrailer.end() && idxIt->second.IsArray() && idxIt->second.arrayVal.size() >= 2) {
        const auto& a = idxIt->second.arrayVal;
        for (std::size_t i = 0; i + 1 < a.size(); i += 2) {
            ranges.push_back({ static_cast<std::uint32_t>(a[i].intVal), static_cast<std::uint32_t>(a[i + 1].intVal) });
        }
    } else {
        auto szIt = outTrailer.find("Size");
        const std::uint32_t sz = (szIt != outTrailer.end()) ? static_cast<std::uint32_t>(szIt->second.intVal) : 0u;
        ranges.push_back({ 0u, sz });
    }

    std::size_t pos = 0;
    for (const auto& rg : ranges) {
        for (std::uint32_t k = 0; k < rg.second; ++k) {
            if (pos + entryBytes > rawXref.size()) break;
            const std::uint64_t type = (w0 > 0) ? ReadUintBE(rawXref.data() + pos, w0) : 1ull;
            const std::uint64_t f2 = (w1 > 0) ? ReadUintBE(rawXref.data() + pos + w0, w1) : 0ull;
            const std::uint64_t f3 = (w2 > 0) ? ReadUintBE(rawXref.data() + pos + w0 + w1, w2) : 0ull;
            pos += entryBytes;

            const std::uint32_t objNum = rg.first + k;
            if (xref_.find(objNum) == xref_.end()) {
                XRefEntry e;
                if (type == 0) {
                    e.inUse = false;
                } else if (type == 1) {
                    e.inUse = true;
                    e.compressed = false;
                    e.offsetOrObjStm = f2;
                    e.genOrIndex = static_cast<std::uint32_t>(f3);
                } else if (type == 2) {
                    e.inUse = true;
                    e.compressed = true;
                    e.offsetOrObjStm = f2;
                    e.genOrIndex = static_cast<std::uint32_t>(f3);
                }
                xref_[objNum] = e;
            }
        }
    }
    return true;
}

bool PdfDocument::DecompressStream(const PdfStreamData& st, std::vector<std::uint8_t>& outBytes, std::string* outErr) const {
    auto fIt = st.dict.find("Filter");
    if (fIt == st.dict.end()) {
        outBytes = st.rawBytes;
        return true;
    }

    std::vector<std::string> filters;
    if (fIt->second.IsName()) {
        filters.push_back(fIt->second.strVal);
    } else if (fIt->second.IsArray()) {
        for (const auto& x : fIt->second.arrayVal) {
            if (x.IsName()) filters.push_back(x.strVal);
        }
    }

    std::vector<std::uint8_t> cur = st.rawBytes;
    for (const std::string& flt : filters) {
        if (flt == "FlateDecode") {
            std::vector<std::uint8_t> tmp;
            if (!InflateZlibStream(cur, tmp)) {
                if (outErr) *outErr = "Error descomprimiendo stream FlateDecode.";
                return false;
            }
            cur = std::move(tmp);
        } else if (flt == "ASCIIHexDecode") {
            std::vector<std::uint8_t> tmp;
            int hi = -1;
            for (std::uint8_t ch : cur) {
                if (ch == '>') break;
                const int hv = HexVal(ch);
                if (hv >= 0) {
                    if (hi < 0) hi = hv;
                    else { tmp.push_back(static_cast<std::uint8_t>((hi << 4) | hv)); hi = -1; }
                }
            }
            cur = std::move(tmp);
        } else {
            // Leave raw or unsupported filter unparsed without crashing
        }
    }

    // Check DecodeParms predictor (PNG 10..15)
    auto dpIt = st.dict.find("DecodeParms");
    const PdfValue* dpVal = (dpIt != st.dict.end()) ? Resolve(&dpIt->second) : nullptr;
    if (dpVal && dpVal->IsArray() && !dpVal->arrayVal.empty()) dpVal = Resolve(&dpVal->arrayVal[0]);
    if (dpVal && dpVal->IsDict()) {
        const PdfValue* pred = dpVal->DictGet("Predictor");
        if (pred && pred->intVal >= 10 && pred->intVal <= 15) {
            const int cols = dpVal->DictGet("Columns") ? static_cast<int>(dpVal->DictGet("Columns")->intVal) : 1;
            const int colors = dpVal->DictGet("Colors") ? static_cast<int>(dpVal->DictGet("Colors")->intVal) : 1;
            const int bpc = dpVal->DictGet("BitsPerComponent") ? static_cast<int>(dpVal->DictGet("BitsPerComponent")->intVal) : 8;
            std::vector<std::uint8_t> unfiltered;
            UnfilterPngPredictor(cur, cols, colors, bpc, unfiltered);
            cur = std::move(unfiltered);
        }
    }

    outBytes = std::move(cur);
    return true;
}

bool PdfDocument::LoadObjStm(std::uint32_t objStmNum) const {
    if (objStmCache_.find(objStmNum) != objStmCache_.end()) return true;
    auto& slot = objStmCache_[objStmNum]; // insert early to break cycles

    auto it = xref_.find(objStmNum);
    if (it == xref_.end() || !it->second.inUse || it->second.compressed) return false;
    PdfValue stmObj = LookupObject(objStmNum);
    if (!stmObj.IsStream()) return false;

    const PdfValue* nVal = stmObj.DictGet("N");
    const PdfValue* firstVal = stmObj.DictGet("First");
    if (!nVal || !firstVal) return false;
    const std::size_t n = static_cast<std::size_t>(std::max<std::int64_t>(0, nVal->intVal));
    const std::size_t firstOff = static_cast<std::size_t>(std::max<std::int64_t>(0, firstVal->intVal));

    std::vector<std::uint8_t> raw;
    if (!DecompressStream(*stmObj.streamVal, raw)) return false;
    if (firstOff > raw.size()) return false;

    PdfReader hdr(raw.data(), (std::min)(firstOff, raw.size()), 0);
    std::vector<std::pair<std::uint32_t, std::size_t>> pairs;
    pairs.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::string oStr = hdr.ReadToken();
        const std::string offStr = hdr.ReadToken();
        if (offStr.empty()) break;
        pairs.push_back({
            static_cast<std::uint32_t>(std::strtoul(oStr.c_str(), nullptr, 10)),
            static_cast<std::size_t>(std::strtoull(offStr.c_str(), nullptr, 10))
        });
    }

    for (std::size_t i = 0; i < pairs.size(); ++i) {
        const std::size_t absStart = firstOff + pairs[i].second;
        if (absStart >= raw.size()) continue;
        PdfReader body(raw.data(), raw.size(), absStart);
        slot[i] = body.ParseValue(0, false);
        cache_[pairs[i].first] = slot[i];
    }
    return true;
}

PdfValue PdfDocument::LookupObject(std::uint32_t objNum) const {
    auto cit = cache_.find(objNum);
    if (cit != cache_.end()) return cit->second;

    auto it = xref_.find(objNum);
    if (it == xref_.end() || !it->second.inUse) return {};

    if (it->second.compressed) {
        const std::uint32_t stmObjNum = static_cast<std::uint32_t>(it->second.offsetOrObjStm);
        LoadObjStm(stmObjNum);
        auto cit2 = cache_.find(objNum);
        if (cit2 != cache_.end()) return cit2->second;
        auto sit = objStmCache_.find(stmObjNum);
        if (sit != objStmCache_.end()) {
            auto idxIt = sit->second.find(it->second.genOrIndex);
            if (idxIt != sit->second.end()) return idxIt->second;
        }
        return {};
    }

    const std::size_t off = static_cast<std::size_t>(it->second.offsetOrObjStm);
    if (off >= data_.size()) return {};
    PdfReader r(data_.data(), data_.size(), off);
    r.ReadToken(); // objNum
    const std::string genTok = r.ReadToken(); // gen
    if (r.ReadToken() != "obj") return {};
    std::uint16_t genNum = 0;
    for (unsigned char ch : genTok) {
        if (ch >= '0' && ch <= '9') {
            genNum = static_cast<std::uint16_t>(genNum * 10u + static_cast<unsigned>(ch - '0'));
        }
    }
    PdfValue v = r.ParseValue(0, true);
    if (encrypted_ && objNum != encryptRef_.obj) {
        DecryptValueTree(v, objNum, genNum, false);
    }
    cache_[objNum] = v;
    return v;
}

const PdfValue* PdfDocument::Resolve(const PdfValue* v) const {
    if (!v) return nullptr;
    if (!v->IsRef()) return v;
    auto cit = cache_.find(v->refVal.obj);
    if (cit != cache_.end()) return &cit->second;
    cache_[v->refVal.obj] = LookupObject(v->refVal.obj);
    return &cache_[v->refVal.obj];
}

void PdfDocument::CollectPages(
    const PdfRef& nodeRef,
    const DocRect& inheritedMedia,
    const DocRect& inheritedCrop,
    bool hasInheritedCrop,
    int inheritedRot,
    const PdfDict* inheritedRes,
    std::vector<std::uint32_t>& visited,
    int depth)
{
    if (depth > 64) return;
    if (std::find(visited.begin(), visited.end(), nodeRef.obj) != visited.end()) return;
    visited.push_back(nodeRef.obj);

    const PdfValue nodeVal = LookupObject(nodeRef.obj);
    const PdfDict* d = nodeVal.DictPtr();
    if (!d) return;

    DocRect media = ParsePdfRect(Resolve(nodeVal.DictGet("MediaBox")), inheritedMedia);
    const PdfValue* cropVal = Resolve(nodeVal.DictGet("CropBox"));
    const bool hasCrop = (cropVal && cropVal->IsArray() && cropVal->arrayVal.size() >= 4) || hasInheritedCrop;
    DocRect crop = cropVal ? ParsePdfRect(cropVal, media) : (hasInheritedCrop ? inheritedCrop : media);

    int rot = inheritedRot;
    if (const PdfValue* rVal = Resolve(nodeVal.DictGet("Rotate"))) {
        if (rVal->IsNumber()) rot = ((static_cast<int>(rVal->AsNumber()) % 360) + 360) % 360;
    }

    const PdfValue* resVal = Resolve(nodeVal.DictGet("Resources"));
    const PdfDict* resDict = (resVal && resVal->DictPtr()) ? resVal->DictPtr() : inheritedRes;

    const PdfValue* typeVal = nodeVal.DictGet("Type");
    const std::string typeName = typeVal ? typeVal->strVal : "";

    if (typeName == "Page" || nodeVal.DictGet("Kids") == nullptr) {
        PdfPageInfo pi;
        pi.pageIndex0 = pages_.size();
        pi.pageRef = nodeRef;
        pi.mediaBox = media;
        pi.cropBox = crop;
        pi.hasCropBox = hasCrop;
        pi.rotationDeg = rot;
        pages_.push_back(pi);
        pages_.back().extractedTextUtf8 = ExtractPageText(nodeVal, resDict, pages_.back().pageIndex0);

        // Extract /Annots for clickable /Link (F-082) and AcroForm /Widget fields (F-084)
        const DocRect effBox = hasCrop ? crop : media;
        const double bw = (std::max)(1.0, effBox.w);
        const double bh = (std::max)(1.0, effBox.h);
        if (const PdfValue* annotsVal = Resolve(nodeVal.DictGet("Annots"))) {
            if (annotsVal->IsArray()) {
                for (const auto& aItem : annotsVal->arrayVal) {
                    const PdfValue* annot = Resolve(&aItem);
                    if (!annot || !annot->IsDict()) continue;
                    const PdfValue* sub = Resolve(annot->DictGet("Subtype"));
                    const PdfValue* rectV = Resolve(annot->DictGet("Rect"));
                    if (!sub || !sub->IsName() || !rectV || !rectV->IsArray() || rectV->arrayVal.size() < 4) continue;
                    const double x1 = rectV->arrayVal[0].AsNumber();
                    const double y1 = rectV->arrayVal[1].AsNumber();
                    const double x2 = rectV->arrayVal[2].AsNumber();
                    const double y2 = rectV->arrayVal[3].AsNumber();
                    const double rx0 = (std::min)(x1, x2), rx1 = (std::max)(x1, x2);
                    const double ry0 = (std::min)(y1, y2), ry1 = (std::max)(y1, y2);
                    const double nl = std::clamp((rx0 - effBox.x) / bw, 0.0, 1.0);
                    const double nrR = std::clamp((rx1 - effBox.x) / bw, nl, 1.0);
                    const double nt = std::clamp(1.0 - (ry1 - effBox.y) / bh, 0.0, 1.0);
                    const double nb = std::clamp(1.0 - (ry0 - effBox.y) / bh, nt, 1.0);
                    const DocRect nr{ nl, nt, (std::max)(0.002, nrR - nl), (std::max)(0.002, nb - nt) };

                    if (sub->strVal == "Link") {
                        PdfLinkRect lr{};
                        lr.rectNorm = nr;
                        if (const PdfValue* act = Resolve(annot->DictGet("A"))) {
                            if (const PdfValue* uri = Resolve(act->DictGet("URI"))) {
                                if (uri->kind == PdfValue::Kind::String) lr.uriUtf8 = uri->strVal;
                            }
                            if (const PdfValue* dst = Resolve(act->DictGet("D"))) {
                                if (dst->IsArray() && !dst->arrayVal.empty() && dst->arrayVal[0].IsRef()) {
                                    lr.targetPage0 = static_cast<int>(dst->arrayVal[0].refVal.obj); // resolved later in PopulateDocumentModel
                                }
                            }
                        }
                        if (lr.uriUtf8.empty() && lr.targetPage0 < 0) {
                            if (const PdfValue* dst = Resolve(annot->DictGet("Dest"))) {
                                if (dst->IsArray() && !dst->arrayVal.empty() && dst->arrayVal[0].IsRef()) {
                                    lr.targetPage0 = static_cast<int>(dst->arrayVal[0].refVal.obj);
                                }
                            }
                        }
                        if (!lr.uriUtf8.empty() || lr.targetPage0 >= 0) {
                            pages_.back().pdfLinks.push_back(std::move(lr));
                        }
                    } else if (sub->strVal == "Widget") {
                        PdfFormField ff{};
                        ff.rectNorm = nr;
                        if (const PdfValue* nm = Resolve(annot->DictGet("T"))) {
                            if (nm->kind == PdfValue::Kind::String) ff.nameUtf8 = nm->strVal;
                        }
                        if (const PdfValue* ft = Resolve(annot->DictGet("FT"))) {
                            if (ft->IsName()) ff.fieldTypeUtf8 = ft->strVal;
                        }
                        if (const PdfValue* val = Resolve(annot->DictGet("V"))) {
                            if (val->kind == PdfValue::Kind::String) ff.valueUtf8 = val->strVal;
                            else if (val->IsName()) {
                                ff.valueUtf8 = val->strVal;
                                ff.checked = (val->strVal != "Off" && val->strVal != "No");
                            }
                        }
                        if (const PdfValue* asV = Resolve(annot->DictGet("AS"))) {
                            if (asV->IsName() && asV->strVal != "Off") ff.checked = true;
                        }
                        if (const PdfValue* fl = Resolve(annot->DictGet("Ff"))) {
                            if (fl->IsNumber() && (static_cast<std::int64_t>(fl->AsNumber()) & 1) != 0) ff.readOnly = true;
                        }
                        if (ff.nameUtf8.empty()) ff.nameUtf8 = "campo_" + std::to_string(pages_.back().formFields.size() + 1);
                        pages_.back().formFields.push_back(std::move(ff));
                    }
                }
            }
        }
        return;
    }

    const PdfValue* kidsVal = Resolve(nodeVal.DictGet("Kids"));
    if (kidsVal && kidsVal->IsArray()) {
        for (const auto& kid : kidsVal->arrayVal) {
            if (kid.IsRef()) {
                CollectPages(kid.refVal, media, crop, hasCrop, rot, resDict, visited, depth + 1);
            }
        }
    }
}

std::vector<std::uint8_t> PdfDocument::GetPageContentBytes(std::size_t pageIndex0) const {
    if (pageIndex0 >= pages_.size()) return {};
    const PdfValue pageVal = LookupObject(pages_[pageIndex0].pageRef.obj);
    const PdfValue* contents = pageVal.DictGet("Contents");
    if (!contents) return {};

    std::vector<PdfRef> refs;
    if (contents->IsRef()) {
        const PdfValue* resolved = Resolve(contents);
        if (resolved && resolved->IsArray()) {
            for (const auto& item : resolved->arrayVal) if (item.IsRef()) refs.push_back(item.refVal);
        } else {
            refs.push_back(contents->refVal);
        }
    } else if (contents->IsArray()) {
        for (const auto& item : contents->arrayVal) if (item.IsRef()) refs.push_back(item.refVal);
    }

    std::vector<std::uint8_t> combined;
    for (const auto& rf : refs) {
        const PdfValue stm = LookupObject(rf.obj);
        if (stm.IsStream()) {
            std::vector<std::uint8_t> dec;
            if (DecompressStream(*stm.streamVal, dec)) {
                combined.insert(combined.end(), dec.begin(), dec.end());
                combined.push_back('\n');
            }
        }
    }
    return combined;
}

std::string PdfDocument::ExtractPageText(const PdfValue& pageDictVal, const PdfDict* inheritedRes, std::size_t pageIndex0) const {
    // Build ToUnicode maps per font resource name
    std::unordered_map<std::string, ToUnicodeMap> fontMaps;
    const PdfValue* localResVal = Resolve(pageDictVal.DictGet("Resources"));
    const PdfDict* resDict = (localResVal && localResVal->DictPtr()) ? localResVal->DictPtr() : inheritedRes;
    if (resDict) {
        auto fIt = resDict->find("Font");
        const PdfValue* fontDictVal = (fIt != resDict->end()) ? Resolve(&fIt->second) : nullptr;
        if (fontDictVal && fontDictVal->IsDict()) {
            for (const auto& kv : fontDictVal->dictVal) {
                const PdfValue* fObj = Resolve(&kv.second);
                if (!fObj || !fObj->IsDict()) continue;
                ToUnicodeMap tum;
                if (const PdfValue* enc = Resolve(fObj->DictGet("Encoding"))) {
                    if (enc->IsName() && (enc->strVal == "Identity-H" || enc->strVal == "Identity-V")) {
                        tum.isTwoByteDefault = true;
                    }
                }
                if (const PdfValue* tu = Resolve(fObj->DictGet("ToUnicode"))) {
                    if (tu->IsStream()) {
                        std::vector<std::uint8_t> cmapBytes;
                        if (DecompressStream(*tu->streamVal, cmapBytes)) {
                            tum.ParseFromStreamBytes(cmapBytes);
                        }
                    }
                }
                fontMaps[kv.first] = std::move(tum);
            }
        }
    }

    const std::vector<std::uint8_t> streamBytes = GetPageContentBytes(pageIndex0);
    if (streamBytes.empty()) return {};

    PdfReader pr(streamBytes.data(), streamBytes.size(), 0);
    std::vector<PdfValue> operands;
    std::string activeFont;
    double fontSizePt = 11.0;
    double curX = 0.0;
    double curY = 0.0;
    bool inText = false;
    std::string out;

    const DocRect effBox = (pageIndex0 < pages_.size())
        ? (pages_[pageIndex0].hasCropBox ? pages_[pageIndex0].cropBox : pages_[pageIndex0].mediaBox)
        : DocRect{ 0.0, 0.0, 595.0, 842.0 };
    const double bw = (std::max)(1.0, effBox.w);
    const double bh = (std::max)(1.0, effBox.h);

    auto recordTextRun = [&](const std::string& decoded) {
        if (decoded.empty() || pageIndex0 >= pages_.size()) return;
        const std::string clean = NormalizeLigaturesUtf8(decoded);
        if (clean.empty()) return;
        const double estW = (std::max)(4.0, static_cast<double>(clean.size()) * fontSizePt * 0.52);
        const double estH = (std::max)(6.0, fontSizePt * 1.15);
        PdfTextRunBox box{};
        box.textUtf8 = clean;
        const double xl = std::clamp((curX - effBox.x) / bw, 0.0, 0.99);
        const double xr = std::clamp((curX + estW - effBox.x) / bw, xl + 0.005, 1.0);
        const double yt = std::clamp(1.0 - (curY + estH * 0.85 - effBox.y) / bh, 0.0, 0.99);
        const double yb = std::clamp(1.0 - (curY - estH * 0.25 - effBox.y) / bh, yt + 0.005, 1.0);
        box.rectNorm = { xl, yt, xr - xl, yb - yt };
        if (pages_[pageIndex0].textBoxes.size() < 2500u) {
            const_cast<PdfDocument*>(this)->pages_[pageIndex0].textBoxes.push_back(std::move(box));
        }
        curX += estW;
    };

    auto decodeStr = [&](const std::string& raw) -> std::string {
        auto it = fontMaps.find(activeFont);
        if (it != fontMaps.end()) return it->second.DecodePdfString(raw);
        ToUnicodeMap defMap;
        return defMap.DecodePdfString(raw);
    };

    auto appendLineBreakIfNeeded = [&]() {
        if (!out.empty() && out.back() != '\n' && out.back() != ' ') out.push_back('\n');
    };

    while (!pr.Eof()) {
        pr.SkipWsAndComments();
        if (pr.Eof()) break;
        const std::uint8_t c = streamBytes[pr.Pos()];
        if (c == '/' || c == '(' || c == '<' || c == '[') {
            operands.push_back(pr.ParseValue(0, false));
            continue;
        }
        const std::string tok = pr.ReadToken();
        if (tok.empty()) break;

        const bool looksNumeric = (std::isdigit(static_cast<unsigned char>(tok[0])) || tok[0] == '-' || tok[0] == '+' || tok[0] == '.');
        if (looksNumeric) {
            char* ep = nullptr;
            const double d = std::strtod(tok.c_str(), &ep);
            if (ep && *ep == '\0') {
                PdfValue nv;
                nv.kind = PdfValue::Kind::Real;
                nv.realVal = d;
                operands.push_back(std::move(nv));
                continue;
            }
        }
        if (tok == "true" || tok == "false" || tok == "null") {
            operands.push_back(PdfValue{});
            continue;
        }

        // Operator dispatch
        if (tok == "BT") {
            inText = true;
            curX = 0.0;
            curY = 0.0;
        } else if (tok == "ET") {
            inText = false;
            appendLineBreakIfNeeded();
        } else if (tok == "Tf") {
            if (operands.size() >= 2 && operands[operands.size() - 2].IsName()) {
                activeFont = operands[operands.size() - 2].strVal;
                const double fs = std::abs(operands.back().AsNumber(11.0));
                if (fs >= 3.0 && fs <= 144.0) fontSizePt = fs;
            }
        } else if ((tok == "Td" || tok == "TD") && inText) {
            if (operands.size() >= 2) {
                const double dx = operands[operands.size() - 2].AsNumber();
                const double dy = operands.back().AsNumber();
                curX += dx;
                curY += dy;
                if (std::abs(dy) > 0.5) appendLineBreakIfNeeded();
                else if (!out.empty() && out.back() != ' ' && out.back() != '\n') out.push_back(' ');
            }
        } else if (tok == "Tm" && inText) {
            if (operands.size() >= 6) {
                const double nx = operands[operands.size() - 2].AsNumber();
                const double ny = operands.back().AsNumber();
                if (std::abs(ny - curY) > 1.0) appendLineBreakIfNeeded();
                else if (!out.empty() && out.back() != ' ' && out.back() != '\n') out.push_back(' ');
                curX = nx;
                curY = ny;
            }
        } else if (tok == "T*" && inText) {
            curY -= fontSizePt * 1.2;
            appendLineBreakIfNeeded();
        } else if ((tok == "Tj" || tok == "'") && inText) {
            if (tok == "'") {
                curY -= fontSizePt * 1.2;
                appendLineBreakIfNeeded();
            }
            if (!operands.empty() && operands.back().kind == PdfValue::Kind::String) {
                const std::string dec = decodeStr(operands.back().strVal);
                out += dec;
                recordTextRun(dec);
            }
        } else if (tok == "\"" && inText) {
            curY -= fontSizePt * 1.2;
            appendLineBreakIfNeeded();
            if (!operands.empty() && operands.back().kind == PdfValue::Kind::String) {
                const std::string dec = decodeStr(operands.back().strVal);
                out += dec;
                recordTextRun(dec);
            }
        } else if (tok == "TJ" && inText) {
            if (!operands.empty() && operands.back().IsArray()) {
                std::string joined;
                for (const auto& elem : operands.back().arrayVal) {
                    if (elem.kind == PdfValue::Kind::String) {
                        const std::string dec = decodeStr(elem.strVal);
                        out += dec;
                        joined += dec;
                    } else if (elem.IsNumber()) {
                        // Large negative kerning (< -120 in thousandths of text space) indicates word space
                        if (elem.AsNumber() < -120.0 && !out.empty() && out.back() != ' ' && out.back() != '\n') {
                            out.push_back(' ');
                            joined.push_back(' ');
                        }
                    }
                }
                recordTextRun(joined);
            }
        }
        operands.clear();
    }

    return NormalizeLigaturesUtf8(out);
}

bool PdfDocument::PopulateDocumentModel(
    DocumentModel& doc,
    const std::string& sourcePathUtf8,
    std::string* outErr) const
{
    if (!loaded_ || pages_.empty()) {
        if (outErr) *outErr = "PDF no cargado o sin p\xC3\xA1ginas.";
        return false;
    }

    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.MutablePages().clear();

    std::string sourceAssetKey;
    if (!data_.empty()) {
        std::uint64_t h = 14695981039346656037ull;
        for (std::uint8_t b : data_) {
            h ^= b;
            h *= 1099511628211ull;
        }
        char hex[32]{};
        std::snprintf(hex, sizeof(hex), "src_pdf_%016llx", static_cast<unsigned long long>(h));
        sourceAssetKey = hex;
        if (!doc.FindAsset(sourceAssetKey)) {
            doc.SetNamedAsset(sourceAssetKey, data_);
        }
    }

    std::unordered_map<std::uint32_t, std::size_t> objNumToPageIdx;
    for (const auto& pi : pages_) {
        objNumToPageIdx[pi.pageRef.obj] = pi.pageIndex0;
    }

    for (const auto& pi : pages_) {
        PageModel pm;
        pm.pageId = doc.AllocatePageId();
        const DocRect effectiveBox = pi.hasCropBox ? pi.cropBox : pi.mediaBox;
        pm.widthPt = (std::max)(36.0, effectiveBox.w);
        pm.heightPt = (std::max)(36.0, effectiveBox.h);
        pm.rotationDeg = pi.rotationDeg;
        pm.background = PaperPattern::Plain;
        pm.hasPdfSource = true;
        pm.sourcePdfPathUtf8 = sourcePathUtf8;
        pm.sourcePdfAssetKey = sourceAssetKey;
        pm.sourcePageIndex0 = static_cast<int>(pi.pageIndex0);
        pm.extractedTextUtf8 = pi.extractedTextUtf8;
        pm.pdfLinks = pi.pdfLinks;
        for (auto& lr : pm.pdfLinks) {
            if (lr.targetPage0 >= 0) {
                auto it = objNumToPageIdx.find(static_cast<std::uint32_t>(lr.targetPage0));
                lr.targetPage0 = (it != objNumToPageIdx.end()) ? static_cast<int>(it->second) : -1;
            }
        }
        pm.formFields = pi.formFields;
        pm.textBoxes = pi.textBoxes;
        doc.MutablePages().push_back(std::move(pm));
    }

    // Extract PDF /Outlines (Table of Contents) -> DocumentModel::Bookmarks() (F-083)
    const PdfValue catalog = LookupObject(rootRef_.obj);
    if (const PdfValue* outlines = Resolve(catalog.DictGet("Outlines"))) {
        std::unordered_set<std::uint32_t> visitedOutlines;
        auto decodePdfTextStr = [](const std::string& raw) -> std::string {
            if (raw.size() >= 2 && static_cast<std::uint8_t>(raw[0]) == 0xFEu && static_cast<std::uint8_t>(raw[1]) == 0xFFu) {
                return Utf16BeBytesToUtf8(raw.substr(2));
            }
            std::string out;
            for (unsigned char b : raw) out += WinAnsiByteToUtf8(b);
            return out;
        };
        auto resolveDestPage = [&](const PdfValue* item) -> std::size_t {
            if (!item) return 0;
            const PdfValue* dest = Resolve(item->DictGet("Dest"));
            if (!dest) {
                if (const PdfValue* act = Resolve(item->DictGet("A"))) {
                    dest = Resolve(act->DictGet("D"));
                }
            }
            if (dest && dest->IsArray() && !dest->arrayVal.empty() && dest->arrayVal[0].IsRef()) {
                auto it = objNumToPageIdx.find(dest->arrayVal[0].refVal.obj);
                if (it != objNumToPageIdx.end()) return it->second;
            }
            return 0;
        };
        std::function<void(const PdfValue*, int)> walkOutlines = [&](const PdfValue* nodeRefVal, int level) {
            if (!nodeRefVal || level > 12 || doc.Bookmarks().size() >= 512u) return;
            const PdfValue* curRef = nodeRefVal;
            while (curRef) {
                std::uint32_t objId = curRef->IsRef() ? curRef->refVal.obj : 0u;
                if (objId != 0 && !visitedOutlines.insert(objId).second) break;
                const PdfValue* item = Resolve(curRef);
                if (!item || !item->IsDict()) break;
                if (const PdfValue* titleV = Resolve(item->DictGet("Title"))) {
                    if (titleV->kind == PdfValue::Kind::String && !titleV->strVal.empty()) {
                        doc.AddBookmark(decodePdfTextStr(titleV->strVal), resolveDestPage(item), 0.0, level);
                    }
                }
                if (const PdfValue* firstChild = item->DictGet("First")) {
                    walkOutlines(firstChild, level + 1);
                }
                curRef = item->DictGet("Next");
            }
        };
        if (const PdfValue* firstTop = outlines->DictGet("First")) {
            walkOutlines(firstTop, 0);
        }
    }

    // Extract PDF /Names /EmbeddedFiles -> DocumentModel::Attachments() (F-085)
    if (const PdfValue* names = Resolve(catalog.DictGet("Names"))) {
        if (const PdfValue* efTree = Resolve(names->DictGet("EmbeddedFiles"))) {
            std::function<void(const PdfValue*, int)> walkNameTree = [&](const PdfValue* node, int depth) {
                if (!node || !node->IsDict() || depth > 16 || doc.Attachments().size() >= 64u) return;
                if (const PdfValue* pairs = Resolve(node->DictGet("Names"))) {
                    if (pairs->IsArray()) {
                        for (std::size_t i = 0; i + 1 < pairs->arrayVal.size(); i += 2) {
                            const std::string fname = pairs->arrayVal[i].strVal;
                            const PdfValue* fspec = Resolve(&pairs->arrayVal[i + 1]);
                            if (!fspec || !fspec->IsDict()) continue;
                            const PdfValue* efDict = Resolve(fspec->DictGet("EF"));
                            const PdfValue* fStm = efDict ? Resolve(efDict->DictGet("F")) : nullptr;
                            if (!fStm && efDict) fStm = Resolve(efDict->DictGet("UF"));
                            if (fStm && fStm->IsStream()) {
                                std::vector<std::uint8_t> raw;
                                if (DecompressStream(*fStm->streamVal, raw) && !raw.empty()) {
                                    std::string finalName = fname;
                                    if (const PdfValue* uf = Resolve(fspec->DictGet("UF"))) {
                                        if (uf->kind == PdfValue::Kind::String && !uf->strVal.empty()) finalName = uf->strVal;
                                    } else if (const PdfValue* f = Resolve(fspec->DictGet("F"))) {
                                        if (f->kind == PdfValue::Kind::String && !f->strVal.empty()) finalName = f->strVal;
                                    }
                                    doc.AddAttachment(finalName.empty() ? "adjunto.bin" : finalName, "application/octet-stream", std::move(raw));
                                }
                            }
                        }
                    }
                }
                if (const PdfValue* kids = Resolve(node->DictGet("Kids"))) {
                    if (kids->IsArray()) {
                        for (const auto& k : kids->arrayVal) walkNameTree(Resolve(&k), depth + 1);
                    }
                }
            };
            walkNameTree(efTree, 0);
        }
    }

    doc.RebuildLayout();
    doc.SetDirty(false);
    return true;
}

TextEditCapability PdfDocument::InspectTextReplacement(
    std::size_t pageIndex0,
    const std::string& searchUtf8) const
{
    TextEditCapability cap;
    cap.supportedVisualRedaction = true;
    if (!loaded_ || pageIndex0 >= pages_.size() || searchUtf8.empty()) {
        cap.supportedDirectStream = false;
        cap.diagnosticUtf8 = "P\xC3\xA1gina inv\xC3\xA1lida o consulta vac\xC3\xAD" "a.";
        return cap;
    }

    const std::vector<std::uint8_t> raw = GetPageContentBytes(pageIndex0);
    const std::string rawStr(reinterpret_cast<const char*>(raw.data()), raw.size());
    const std::string escaped = "(" + searchUtf8 + ")";
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = rawStr.find(escaped, pos)) != std::string::npos) {
        ++count;
        pos += escaped.size();
    }
    cap.matchCountInStream = count;
    if (count > 0) {
        cap.supportedDirectStream = true;
        cap.diagnosticUtf8 = "Texto literal ANSI encontrado directamente en el stream PDF (reemplazo directo o cobertura disponible).";
    } else {
        cap.supportedDirectStream = false;
        cap.diagnosticUtf8 = "Fuente con codificaci\xC3\xB3n CID/subconjunto o glyphs fragmentados: se usar\xC3\xA1 redactado opaco + TextBox superpuesto (reportado con precisi\xC3\xB3n).";
    }
    return cap;
}

bool PdfDocument::InitializeEncryption(std::string* outErr) {
    if (!hasEncryptRef_) {
        encrypted_ = false;
        return true;
    }

    PdfValue encVal = LookupObject(encryptRef_.obj);
    if (!encVal.IsDict()) {
        if (outErr) *outErr = "Diccionario /Encrypt inv\xC3\xA1lido o ilegible en el PDF.";
        return false;
    }

    const PdfDict& enc = encVal.dictVal;
    auto filterIt = enc.find("Filter");
    if (filterIt == enc.end() || filterIt->second.kind != PdfValue::Kind::Name ||
        filterIt->second.strVal != "Standard") {
        if (outErr) *outErr = "El filtro de seguridad del PDF no es /Standard.";
        return false;
    }

    auto getInt = [](const PdfDict& d, const char* key, std::int64_t defVal) -> std::int64_t {
        auto it = d.find(key);
        return (it != d.end() && it->second.kind == PdfValue::Kind::Int) ? it->second.intVal : defVal;
    };

    const int vVer = static_cast<int>(getInt(enc, "V", 0));
    const int rRev = static_cast<int>(getInt(enc, "R", 2));
    if (vVer < 1 || vVer > 4 || rRev < 2 || rRev > 4) {
        if (outErr) *outErr = "Versi\xC3\xB3n de cifrado PDF no soportada (se soporta Standard V=1..4, R=2..4).";
        return false;
    }

    std::size_t keyBytes = (vVer == 1) ? 5u : static_cast<std::size_t>(getInt(enc, "Length", 128) / 8);
    if (keyBytes < 5u) keyBytes = 5u;
    if (keyBytes > 16u) keyBytes = 16u;

    encryptMetadata_ = true;
    auto metaIt = enc.find("EncryptMetadata");
    if (metaIt != enc.end() && metaIt->second.kind == PdfValue::Kind::Bool) {
        encryptMetadata_ = metaIt->second.boolVal;
    }

    if (vVer == 4) {
        std::string stmF = "Identity";
        std::string strF = "Identity";
        auto stmIt = enc.find("StmF");
        if (stmIt != enc.end() && stmIt->second.kind == PdfValue::Kind::Name) stmF = stmIt->second.strVal;
        auto strIt = enc.find("StrF");
        if (strIt != enc.end() && strIt->second.kind == PdfValue::Kind::Name) strF = strIt->second.strVal;

        PdfValue cfCopy;
        const PdfDict* cfDict = nullptr;
        auto cfIt = enc.find("CF");
        if (cfIt != enc.end()) {
            const PdfValue* cfPtr = Resolve(&cfIt->second);
            if (cfPtr && cfPtr->IsDict()) {
                cfCopy = *cfPtr;
                cfDict = &cfCopy.dictVal;
            }
        }

        auto configureCf = [&](const std::string& name, bool& outOn, bool& outAes) -> bool {
            if (name.empty() || name == "Identity") {
                outOn = false;
                outAes = false;
                return true;
            }
            if (!cfDict) return false;
            auto fIt = cfDict->find(name);
            if (fIt == cfDict->end()) return false;
            const PdfValue* fPtr = Resolve(&fIt->second);
            if (!fPtr || !fPtr->IsDict()) return false;
            const PdfDict& fDict = fPtr->dictVal;
            auto cfmIt = fDict.find("CFM");
            if (cfmIt == fDict.end() || cfmIt->second.kind != PdfValue::Kind::Name) return false;
            const std::string& cfm = cfmIt->second.strVal;
            if (cfm == "V2") {
                outOn = true;
                outAes = false;
            } else if (cfm == "AESV2") {
                outOn = true;
                outAes = true;
            } else if (cfm == "None") {
                outOn = false;
                outAes = false;
            } else {
                return false;
            }
            const std::int64_t cfLen = getInt(fDict, "Length", 0);
            if (cfLen >= 5 && cfLen <= 16) {
                keyBytes = static_cast<std::size_t>(cfLen);
            } else if (cfLen >= 40 && (cfLen % 8) == 0 && (cfLen / 8) <= 16) {
                keyBytes = static_cast<std::size_t>(cfLen / 8);
            }
            return true;
        };

        if (!configureCf(stmF, stmEncrypted_, stmUseAes_) ||
            !configureCf(strF, strEncrypted_, strUseAes_)) {
            if (outErr) *outErr = "Crypt Filter (/CF) de PDF V=4 no soportado.";
            return false;
        }
    } else {
        stmEncrypted_ = true;
        strEncrypted_ = true;
        stmUseAes_ = false;
        strUseAes_ = false;
    }

    auto oIt = enc.find("O");
    auto uIt = enc.find("U");
    auto pIt = enc.find("P");
    if (oIt == enc.end() || oIt->second.kind != PdfValue::Kind::String ||
        uIt == enc.end() || uIt->second.kind != PdfValue::Kind::String ||
        pIt == enc.end() || pIt->second.kind != PdfValue::Kind::Int) {
        if (outErr) *outErr = "Entradas /O, /U o /P faltantes en /Encrypt del PDF.";
        return false;
    }

    std::uint8_t oBytes[32]{};
    const std::string& oStr = oIt->second.strVal;
    const std::size_t oCopy = (std::min<std::size_t>)(oStr.size(), 32u);
    if (oCopy > 0) std::memcpy(oBytes, oStr.data(), oCopy);

    const std::uint32_t pU32 = static_cast<std::uint32_t>(static_cast<std::int32_t>(pIt->second.intVal));

    std::vector<std::uint8_t> hashInput;
    hashInput.reserve(32u + 32u + 4u + fileId0_.size() + 4u);
    hashInput.insert(hashInput.end(), kPdfStdPadding, kPdfStdPadding + 32u);
    hashInput.insert(hashInput.end(), oBytes, oBytes + 32u);
    hashInput.push_back(static_cast<std::uint8_t>(pU32 & 0xFFu));
    hashInput.push_back(static_cast<std::uint8_t>((pU32 >> 8) & 0xFFu));
    hashInput.push_back(static_cast<std::uint8_t>((pU32 >> 16) & 0xFFu));
    hashInput.push_back(static_cast<std::uint8_t>((pU32 >> 24) & 0xFFu));
    if (!fileId0_.empty()) {
        const auto* idPtr = reinterpret_cast<const std::uint8_t*>(fileId0_.data());
        hashInput.insert(hashInput.end(), idPtr, idPtr + fileId0_.size());
    }
    if (rRev >= 4 && !encryptMetadata_) {
        hashInput.push_back(0xFFu);
        hashInput.push_back(0xFFu);
        hashInput.push_back(0xFFu);
        hashInput.push_back(0xFFu);
    }

    std::uint8_t digest[16]{};
    if (!ComputeMd5(hashInput.data(), hashInput.size(), digest)) {
        if (outErr) *outErr = "Error interno calculando MD5 para descifrado PDF.";
        return false;
    }
    if (rRev >= 3) {
        for (int i = 0; i < 50; ++i) {
            if (!ComputeMd5(digest, keyBytes, digest)) {
                if (outErr) *outErr = "Error interno iterando clave MD5 para PDF.";
                return false;
            }
        }
    }
    fileKey_.assign(digest, digest + keyBytes);

    const std::string& uStr = uIt->second.strVal;
    bool uVerified = false;
    std::vector<std::uint8_t> uTest;
    if (rRev == 2) {
        if (uStr.size() >= 32u && Rc4Crypt(fileKey_.data(), fileKey_.size(), kPdfStdPadding, 32u, uTest) && uTest.size() >= 32u) {
            uVerified = (std::memcmp(uTest.data(), uStr.data(), 32u) == 0);
        }
    } else {
        std::vector<std::uint8_t> uHashIn;
        uHashIn.reserve(32u + fileId0_.size());
        uHashIn.insert(uHashIn.end(), kPdfStdPadding, kPdfStdPadding + 32u);
        if (!fileId0_.empty()) {
            const auto* idPtr = reinterpret_cast<const std::uint8_t*>(fileId0_.data());
            uHashIn.insert(uHashIn.end(), idPtr, idPtr + fileId0_.size());
        }
        std::uint8_t uDigest[16]{};
        if (uStr.size() >= 16u &&
            ComputeMd5(uHashIn.data(), uHashIn.size(), uDigest) &&
            Rc4Crypt(fileKey_.data(), fileKey_.size(), uDigest, 16u, uTest)) {
            std::vector<std::uint8_t> tmpKey(fileKey_.size());
            std::vector<std::uint8_t> nextTest;
            bool stepOk = true;
            for (int iter = 1; iter <= 19; ++iter) {
                for (std::size_t k = 0; k < fileKey_.size(); ++k) {
                    tmpKey[k] = static_cast<std::uint8_t>(fileKey_[k] ^ static_cast<std::uint8_t>(iter));
                }
                if (!Rc4Crypt(tmpKey.data(), tmpKey.size(), uTest.data(), uTest.size(), nextTest)) {
                    stepOk = false;
                    break;
                }
                uTest = std::move(nextTest);
            }
            if (stepOk && uTest.size() >= 16u) {
                uVerified = (std::memcmp(uTest.data(), uStr.data(), 16u) == 0);
            }
        }
    }

    if (!uVerified) {
        if (outErr) *outErr = "El PDF est\xC3\xA1 protegido con contrase\xC3\xB1""a de usuario no vac\xC3\xAD""a.";
        return false;
    }

    encryptR_ = rRev;
    encrypted_ = (stmEncrypted_ || strEncrypted_);
    return true;
}

std::vector<std::uint8_t> PdfDocument::ComputeObjectKey(std::uint32_t objNum, std::uint16_t gen, bool aes) const {
    std::vector<std::uint8_t> in = fileKey_;
    in.push_back(static_cast<std::uint8_t>(objNum & 0xFFu));
    in.push_back(static_cast<std::uint8_t>((objNum >> 8) & 0xFFu));
    in.push_back(static_cast<std::uint8_t>((objNum >> 16) & 0xFFu));
    in.push_back(static_cast<std::uint8_t>(gen & 0xFFu));
    in.push_back(static_cast<std::uint8_t>((gen >> 8) & 0xFFu));
    if (aes) {
        in.push_back(0x73u);
        in.push_back(0x41u);
        in.push_back(0x6Cu);
        in.push_back(0x54u);
    }
    std::uint8_t digest[16]{};
    if (!ComputeMd5(in.data(), in.size(), digest)) {
        return {};
    }
    const std::size_t outLen = (std::min<std::size_t>)(fileKey_.size() + 5u, 16u);
    return std::vector<std::uint8_t>(digest, digest + outLen);
}

bool PdfDocument::DecryptBufferForObject(std::uint32_t objNum, std::uint16_t gen, bool aes,
                                         const std::vector<std::uint8_t>& in,
                                         std::vector<std::uint8_t>& out) const {
    if (in.empty()) {
        out.clear();
        return true;
    }
    const std::vector<std::uint8_t> key = ComputeObjectKey(objNum, gen, aes);
    if (key.empty()) return false;
    if (aes) {
        if (key.size() != 16u) return false;
        return Aes128CbcDecrypt(key.data(), in.data(), in.size(), out);
    }
    return Rc4Crypt(key.data(), key.size(), in.data(), in.size(), out);
}

void PdfDocument::DecryptValueTree(PdfValue& v, std::uint32_t objNum, std::uint16_t gen, bool insideTopStream) const {
    (void)insideTopStream;
    switch (v.kind) {
    case PdfValue::Kind::String:
        if (strEncrypted_ && !v.strVal.empty()) {
            const auto* ptr = reinterpret_cast<const std::uint8_t*>(v.strVal.data());
            std::vector<std::uint8_t> in(ptr, ptr + v.strVal.size());
            std::vector<std::uint8_t> out;
            if (DecryptBufferForObject(objNum, gen, strUseAes_, in, out)) {
                v.strVal.assign(reinterpret_cast<const char*>(out.data()), out.size());
            }
        }
        break;
    case PdfValue::Kind::Array:
        for (auto& el : v.arrayVal) {
            DecryptValueTree(el, objNum, gen, insideTopStream);
        }
        break;
    case PdfValue::Kind::Dict:
        for (auto& kv : v.dictVal) {
            DecryptValueTree(kv.second, objNum, gen, insideTopStream);
        }
        break;
    case PdfValue::Kind::Stream:
        if (!v.streamVal) break;
        {
            auto typeIt = v.streamVal->dict.find("Type");
            if (typeIt != v.streamVal->dict.end() &&
                typeIt->second.kind == PdfValue::Kind::Name &&
                typeIt->second.strVal == "XRef") {
                break;
            }
            for (auto& kv : v.streamVal->dict) {
                DecryptValueTree(kv.second, objNum, gen, true);
            }
            const bool isMetadata = (typeIt != v.streamVal->dict.end() &&
                                     typeIt->second.kind == PdfValue::Kind::Name &&
                                     typeIt->second.strVal == "Metadata");
            bool skipCryptIdentity = false;
            auto fltIt = v.streamVal->dict.find("Filter");
            if (fltIt != v.streamVal->dict.end()) {
                auto isIdentityParm = [](const PdfValue& dp) -> bool {
                    if (dp.kind == PdfValue::Kind::Null) return true;
                    if (!dp.IsDict()) return false;
                    auto nIt = dp.dictVal.find("Name");
                    return (nIt == dp.dictVal.end()) ||
                           (nIt->second.kind == PdfValue::Kind::Name && nIt->second.strVal == "Identity");
                };
                if (fltIt->second.kind == PdfValue::Kind::Name && fltIt->second.strVal == "Crypt") {
                    auto dpIt = v.streamVal->dict.find("DecodeParms");
                    if (dpIt == v.streamVal->dict.end() || isIdentityParm(dpIt->second)) {
                        skipCryptIdentity = true;
                    }
                } else if (fltIt->second.kind == PdfValue::Kind::Array) {
                    auto dpIt = v.streamVal->dict.find("DecodeParms");
                    for (std::size_t idx = 0; idx < fltIt->second.arrayVal.size(); ++idx) {
                        if (fltIt->second.arrayVal[idx].kind == PdfValue::Kind::Name &&
                            fltIt->second.arrayVal[idx].strVal == "Crypt") {
                            if (dpIt == v.streamVal->dict.end()) {
                                skipCryptIdentity = true;
                            } else if (dpIt->second.kind == PdfValue::Kind::Array &&
                                       idx < dpIt->second.arrayVal.size() &&
                                       isIdentityParm(dpIt->second.arrayVal[idx])) {
                                skipCryptIdentity = true;
                            }
                        }
                    }
                }
            }
            if (stmEncrypted_ && !(isMetadata && !encryptMetadata_) && !skipCryptIdentity &&
                !v.streamVal->rawBytes.empty()) {
                std::vector<std::uint8_t> out;
                if (DecryptBufferForObject(objNum, gen, stmUseAes_, v.streamVal->rawBytes, out)) {
                    v.streamVal->rawBytes = std::move(out);
                }
            }
        }
        break;
    default:
        break;
    }
}

std::string PdfDocument::SerializeValue(const PdfValue& v) {
    std::ostringstream oss;
    switch (v.kind) {
    case PdfValue::Kind::Null:
        oss << "null";
        break;
    case PdfValue::Kind::Bool:
        oss << (v.boolVal ? "true" : "false");
        break;
    case PdfValue::Kind::Int:
        oss << v.intVal;
        break;
    case PdfValue::Kind::Real: {
        char buf[64]{};
        std::snprintf(buf, sizeof(buf), "%.4f", v.realVal);
        oss << buf;
        break;
    }
    case PdfValue::Kind::Name:
        oss << '/' << v.strVal;
        break;
    case PdfValue::Kind::String: {
        bool binary = false;
        for (unsigned char ch : v.strVal) {
            if (ch < 0x20 || ch > 0x7Eu) { binary = true; break; }
        }
        if (binary) {
            oss << '<';
            static constexpr char kHex[] = "0123456789ABCDEF";
            for (unsigned char ch : v.strVal) {
                oss << kHex[(ch >> 4) & 0x0Fu] << kHex[ch & 0x0Fu];
            }
            oss << '>';
        } else {
            oss << '(';
            for (char ch : v.strVal) {
                if (ch == '(' || ch == ')' || ch == '\\') oss << '\\';
                oss << ch;
            }
            oss << ')';
        }
        break;
    }
    case PdfValue::Kind::Array:
        oss << '[';
        for (std::size_t i = 0; i < v.arrayVal.size(); ++i) {
            if (i > 0) oss << ' ';
            oss << SerializeValue(v.arrayVal[i]);
        }
        oss << ']';
        break;
    case PdfValue::Kind::Dict:
        oss << "<<";
        for (const auto& kv : v.dictVal) {
            oss << '/' << kv.first << ' ' << SerializeValue(kv.second) << '\n';
        }
        oss << ">>";
        break;
    case PdfValue::Kind::Ref:
        oss << v.refVal.obj << ' ' << v.refVal.gen << " R";
        break;
    case PdfValue::Kind::Stream:
        if (v.streamVal) {
            PdfDict d = v.streamVal->dict;
            PdfValue lenV;
            lenV.kind = PdfValue::Kind::Int;
            lenV.intVal = static_cast<std::int64_t>(v.streamVal->rawBytes.size());
            d["Length"] = lenV;
            oss << "<<";
            for (const auto& kv : d) {
                oss << '/' << kv.first << ' ' << SerializeValue(kv.second) << '\n';
            }
            oss << ">>\nstream\n";
            oss.write(reinterpret_cast<const char*>(v.streamVal->rawBytes.data()), static_cast<std::streamsize>(v.streamVal->rawBytes.size()));
            oss << "\nendstream";
        }
        break;
    }
    return oss.str();
}

} // namespace DocStudio
