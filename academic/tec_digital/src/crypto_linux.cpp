#include "academic/tec_digital/crypto.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <cctype>
#include <fstream>
#include <vector>

namespace academic::tec_digital {

namespace {

std::string BytesToLowerHex(const uint8_t* data, size_t len) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out[i * 2] = kHex[(data[i] >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[data[i] & 0x0F];
    }
    return out;
}

}  // namespace

std::array<uint8_t, 32> Sha256Bytes(const uint8_t* data, size_t len) {
    std::array<uint8_t, 32> digest{};
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx != nullptr) {
        unsigned int out_len = 0;
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1 &&
            (len == 0 || data == nullptr || EVP_DigestUpdate(ctx, data, len) == 1)) {
            EVP_DigestFinal_ex(ctx, digest.data(), &out_len);
        }
        EVP_MD_CTX_free(ctx);
    }
    return digest;
}

std::array<uint8_t, 32> Sha256Bytes(std::string_view data) {
    return Sha256Bytes(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string Sha256Hex(const uint8_t* data, size_t len) {
    auto digest = Sha256Bytes(data, len);
    return BytesToLowerHex(digest.data(), digest.size());
}

std::string Sha256Hex(std::string_view data) {
    return Sha256Hex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::string Sha256FileHex(const std::string& path, std::string& out_error) {
    out_error.clear();
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs.is_open()) {
        out_error = "failed to open file";
        return {};
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        out_error = "EVP_MD_CTX_new failed";
        return {};
    }
    if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(ctx);
        out_error = "EVP_DigestInit_ex failed";
        return {};
    }

    std::vector<char> buf(65536);
    while (ifs.good()) {
        ifs.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        const std::streamsize got = ifs.gcount();
        if (got > 0) {
            if (EVP_DigestUpdate(ctx, buf.data(), static_cast<size_t>(got)) != 1) {
                EVP_MD_CTX_free(ctx);
                out_error = "EVP_DigestUpdate failed";
                return {};
            }
        }
    }

    std::array<uint8_t, 32> digest{};
    unsigned int out_len = 0;
    if (EVP_DigestFinal_ex(ctx, digest.data(), &out_len) != 1) {
        EVP_MD_CTX_free(ctx);
        out_error = "EVP_DigestFinal_ex failed";
        return {};
    }
    EVP_MD_CTX_free(ctx);
    return BytesToLowerHex(digest.data(), digest.size());
}

std::string Sha1Hex(const uint8_t* data, size_t len) {
    std::array<uint8_t, 20> digest{};
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx != nullptr) {
        unsigned int out_len = 0;
        if (EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr) == 1 &&
            (len == 0 || data == nullptr || EVP_DigestUpdate(ctx, data, len) == 1)) {
            EVP_DigestFinal_ex(ctx, digest.data(), &out_len);
        }
        EVP_MD_CTX_free(ctx);
    }
    return BytesToLowerHex(digest.data(), digest.size());
}

std::string Sha1Hex(std::string_view data) {
    return Sha1Hex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> RandomBytes(size_t count) {
    std::vector<uint8_t> out(count);
    if (count > 0) {
        if (RAND_bytes(out.data(), static_cast<int>(count)) != 1) {
            out.clear();
        }
    }
    return out;
}

std::string Base64Encode(const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return {};
    const size_t out_cap = ((len + 2) / 3) * 4 + 1;
    std::string out(out_cap, '\0');
    int written = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(out.data()),
                                  data, static_cast<int>(len));
    if (written < 0) return {};
    out.resize(static_cast<size_t>(written));
    return out;
}

std::string Base64Encode(std::string_view data) {
    return Base64Encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool Base64Decode(std::string_view encoded, std::vector<uint8_t>& out_bytes) {
    out_bytes.clear();
    std::string clean;
    clean.reserve(encoded.size());
    for (char ch : encoded) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            clean.push_back(ch);
        }
    }
    if (clean.empty()) return true;
    if (clean.size() % 4 != 0) return false;

    size_t pad = 0;
    if (clean.size() >= 1 && clean[clean.size() - 1] == '=') ++pad;
    if (clean.size() >= 2 && clean[clean.size() - 2] == '=') ++pad;

    for (size_t i = 0; i < clean.size() - pad; ++i) {
        const unsigned char c = static_cast<unsigned char>(clean[i]);
        const bool ok_char = std::isalnum(c) || c == '+' || c == '/';
        if (!ok_char) return false;
    }

    std::vector<uint8_t> tmp((clean.size() / 4) * 3);
    int decoded_len = EVP_DecodeBlock(tmp.data(),
                                      reinterpret_cast<const unsigned char*>(clean.data()),
                                      static_cast<int>(clean.size()));
    if (decoded_len < 0) return false;
    if (static_cast<size_t>(decoded_len) < pad) return false;
    tmp.resize(static_cast<size_t>(decoded_len) - pad);
    out_bytes = std::move(tmp);
    return true;
}

void SecureZeroMemoryBuffer(void* ptr, size_t len) {
    if (ptr != nullptr && len > 0) {
        OPENSSL_cleanse(ptr, len);
    }
}

void SecureZeroString(std::string& s) {
    if (!s.empty()) {
        OPENSSL_cleanse(s.data(), s.size());
        s.clear();
    }
}

void SecureZeroBytes(std::vector<uint8_t>& b) {
    if (!b.empty()) {
        OPENSSL_cleanse(b.data(), b.size());
        b.clear();
    }
}

}  // namespace academic::tec_digital
