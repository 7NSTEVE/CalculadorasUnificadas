#include "academic/tec_digital/crypto.hpp"

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>

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

template <size_t DigestSize>
bool ComputeBCryptHash(LPCWSTR alg_id, const uint8_t* data, size_t len, std::array<uint8_t, DigestSize>& out) {
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, alg_id, nullptr, 0) != 0) {
        return false;
    }
    DWORD hash_obj_size = 0;
    DWORD cb_data = 0;
    bool ok = false;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hash_obj_size),
                          sizeof(DWORD), &cb_data, 0) == 0) {
        std::vector<uint8_t> hash_obj(hash_obj_size);
        if (BCryptCreateHash(hAlg, &hHash, hash_obj.data(), hash_obj_size, nullptr, 0, 0) == 0) {
            bool fed = true;
            if (len > 0 && data != nullptr) {
                fed = (BCryptHashData(hHash, const_cast<PUCHAR>(data), static_cast<ULONG>(len), 0) == 0);
            }
            if (fed && BCryptFinishHash(hHash, out.data(), static_cast<ULONG>(DigestSize), 0) == 0) {
                ok = true;
            }
            BCryptDestroyHash(hHash);
        }
    }
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return ok;
}

}  // namespace

std::array<uint8_t, 32> Sha256Bytes(const uint8_t* data, size_t len) {
    std::array<uint8_t, 32> digest{};
    ComputeBCryptHash<32>(BCRYPT_SHA256_ALGORITHM, data, len, digest);
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
    int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    if (wlen <= 0) {
        out_error = "invalid UTF-8 path";
        return {};
    }
    std::wstring wpath(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, wpath.data(), wlen);

    HANDLE hFile = CreateFileW(wpath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        out_error = "failed to open file";
        return {};
    }

    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) {
        CloseHandle(hFile);
        out_error = "BCryptOpenAlgorithmProvider failed";
        return {};
    }

    DWORD hash_obj_size = 0;
    DWORD cb_data = 0;
    std::string result;
    if (BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&hash_obj_size),
                          sizeof(DWORD), &cb_data, 0) == 0) {
        std::vector<uint8_t> hash_obj(hash_obj_size);
        if (BCryptCreateHash(hAlg, &hHash, hash_obj.data(), hash_obj_size, nullptr, 0, 0) == 0) {
            std::vector<uint8_t> buf(65536);
            DWORD bytes_read = 0;
            bool read_ok = true;
            while (ReadFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &bytes_read, nullptr)) {
                if (bytes_read == 0) break;
                if (BCryptHashData(hHash, buf.data(), bytes_read, 0) != 0) {
                    read_ok = false;
                    out_error = "BCryptHashData failed";
                    break;
                }
            }
            if (read_ok) {
                std::array<uint8_t, 32> digest{};
                if (BCryptFinishHash(hHash, digest.data(), 32, 0) == 0) {
                    result = BytesToLowerHex(digest.data(), digest.size());
                } else {
                    out_error = "BCryptFinishHash failed";
                }
            }
            BCryptDestroyHash(hHash);
        } else {
            out_error = "BCryptCreateHash failed";
        }
    } else {
        out_error = "BCryptGetProperty failed";
    }

    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    return result;
}

std::string Sha1Hex(const uint8_t* data, size_t len) {
    std::array<uint8_t, 20> digest{};
    ComputeBCryptHash<20>(BCRYPT_SHA1_ALGORITHM, data, len, digest);
    return BytesToLowerHex(digest.data(), digest.size());
}

std::string Sha1Hex(std::string_view data) {
    return Sha1Hex(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

std::vector<uint8_t> RandomBytes(size_t count) {
    std::vector<uint8_t> out(count);
    if (count > 0) {
        if (BCryptGenRandom(nullptr, out.data(), static_cast<ULONG>(count),
                            BCRYPT_USE_SYSTEM_PREFERRED_RNG) != 0) {
            out.clear();
        }
    }
    return out;
}

std::string Base64Encode(const uint8_t* data, size_t len) {
    if (len == 0 || data == nullptr) return {};
    DWORD out_len = 0;
    if (!CryptBinaryToStringA(data, static_cast<DWORD>(len),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &out_len)) {
        return {};
    }
    std::string out(out_len, '\0');
    if (!CryptBinaryToStringA(data, static_cast<DWORD>(len),
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, out.data(), &out_len)) {
        return {};
    }
    while (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
}

std::string Base64Encode(std::string_view data) {
    return Base64Encode(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

bool Base64Decode(std::string_view encoded, std::vector<uint8_t>& out_bytes) {
    out_bytes.clear();
    if (encoded.empty()) return true;
    DWORD out_len = 0;
    if (!CryptStringToBinaryA(encoded.data(), static_cast<DWORD>(encoded.size()),
                              CRYPT_STRING_BASE64, nullptr, &out_len, nullptr, nullptr)) {
        return false;
    }
    out_bytes.resize(out_len);
    if (!CryptStringToBinaryA(encoded.data(), static_cast<DWORD>(encoded.size()),
                              CRYPT_STRING_BASE64, out_bytes.data(), &out_len, nullptr, nullptr)) {
        out_bytes.clear();
        return false;
    }
    out_bytes.resize(out_len);
    return true;
}

void SecureZeroMemoryBuffer(void* ptr, size_t len) {
    if (ptr != nullptr && len > 0) {
        SecureZeroMemory(ptr, len);
    }
}

void SecureZeroString(std::string& s) {
    if (!s.empty()) {
        SecureZeroMemory(s.data(), s.size());
        s.clear();
    }
}

void SecureZeroBytes(std::vector<uint8_t>& b) {
    if (!b.empty()) {
        SecureZeroMemory(b.data(), b.size());
        b.clear();
    }
}

}  // namespace academic::tec_digital
