#pragma once

// ============================================================================
// cudoc_crypto.hpp
// Cross-platform Authenticated Encryption (AES-256-GCM + PBKDF2-HMAC-SHA256)
// for locked/exam .cudoc vaults.
//   - Windows: CNG BCrypt (bcrypt.lib)
//   - Linux:   OpenSSL 3 EVP (libcrypto)
// Format: "CUDOCENC\x01" (9) | salt (16) | iv (12) | tag (16) | ciphertext (N)
// Zero plaintext ZIP headers or "manifest.json" ever appear on disk.
// ============================================================================

#include "cudoc_storage.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

namespace DocStudio::CudocCrypto {

inline constexpr std::uint8_t kMagic[9] = { 'C','U','D','O','C','E','N','C', 0x01 };
inline constexpr std::size_t kSaltLen = 16;
inline constexpr std::size_t kIvLen = 12;
inline constexpr std::size_t kTagLen = 16;
inline constexpr std::size_t kKeyLen = 32;
inline constexpr std::uint32_t kPbkdf2Iters = 100000;

inline bool IsEncryptedCudocBlob(const std::uint8_t* data, std::size_t size) noexcept {
    return data && size >= sizeof(kMagic) + kSaltLen + kIvLen + kTagLen &&
           std::memcmp(data, kMagic, sizeof(kMagic)) == 0;
}

inline bool FillRandomBytes(std::uint8_t* dst, std::size_t len) {
#ifdef _WIN32
    return BCryptGenRandom(nullptr, dst, static_cast<ULONG>(len), BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    return RAND_bytes(dst, static_cast<int>(len)) == 1;
#endif
}

inline bool DeriveKeyPbkdf2(
    const std::string& passphrase,
    const std::uint8_t* salt,
    std::size_t saltLen,
    std::uint8_t outKey[kKeyLen])
{
    if (passphrase.empty() || !salt || saltLen == 0) return false;
#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) {
        return false;
    }
    NTSTATUS st = BCryptDeriveKeyPBKDF2(
        hAlg,
        reinterpret_cast<PUCHAR>(const_cast<char*>(passphrase.data())),
        static_cast<ULONG>(passphrase.size()),
        const_cast<PUCHAR>(salt),
        static_cast<ULONG>(saltLen),
        kPbkdf2Iters,
        outKey,
        static_cast<ULONG>(kKeyLen),
        0);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return st == 0;
#else
    return PKCS5_PBKDF2_HMAC(
        passphrase.data(), static_cast<int>(passphrase.size()),
        salt, static_cast<int>(saltLen),
        static_cast<int>(kPbkdf2Iters),
        EVP_sha256(),
        static_cast<int>(kKeyLen),
        outKey) == 1;
#endif
}

inline bool EncryptBytes(
    const std::vector<std::uint8_t>& plaintext,
    const std::string& passphrase,
    std::vector<std::uint8_t>& outBlob,
    std::string* outErr = nullptr)
{
    if (passphrase.empty()) {
        if (outErr) *outErr = "Empty encryption passphrase";
        return false;
    }
    std::uint8_t salt[kSaltLen]{};
    std::uint8_t iv[kIvLen]{};
    std::uint8_t key[kKeyLen]{};
    std::uint8_t tag[kTagLen]{};
    if (!FillRandomBytes(salt, kSaltLen) || !FillRandomBytes(iv, kIvLen)) {
        if (outErr) *outErr = "CSPRNG failure";
        return false;
    }
    if (!DeriveKeyPbkdf2(passphrase, salt, kSaltLen, key)) {
        if (outErr) *outErr = "PBKDF2 key derivation failed";
        return false;
    }

    std::vector<std::uint8_t> cipher(plaintext.size());
    bool ok = false;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                static_cast<ULONG>((wcslen(BCRYPT_CHAIN_MODE_GCM) + 1) * sizeof(wchar_t)), 0) == 0 &&
            BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key, static_cast<ULONG>(kKeyLen), 0) == 0) {
            BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
            BCRYPT_INIT_AUTH_MODE_INFO(info);
            info.pbNonce = iv;
            info.cbNonce = static_cast<ULONG>(kIvLen);
            info.pbAuthData = const_cast<PUCHAR>(kMagic);
            info.cbAuthData = static_cast<ULONG>(sizeof(kMagic));
            info.pbTag = tag;
            info.cbTag = static_cast<ULONG>(kTagLen);
            ULONG cbRes = 0;
            NTSTATUS st = BCryptEncrypt(
                hKey,
                plaintext.empty() ? nullptr : const_cast<PUCHAR>(plaintext.data()),
                static_cast<ULONG>(plaintext.size()),
                &info,
                nullptr, 0,
                cipher.empty() ? nullptr : cipher.data(),
                static_cast<ULONG>(cipher.size()),
                &cbRes, 0);
            if (st == 0) {
                cipher.resize(cbRes);
                ok = true;
            }
            BCryptDestroyKey(hKey);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
#else
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx) {
        int len = 0, total = 0;
        if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kIvLen), nullptr) == 1 &&
            EVP_EncryptInit_ex(ctx, nullptr, nullptr, key, iv) == 1 &&
            EVP_EncryptUpdate(ctx, nullptr, &len, kMagic, static_cast<int>(sizeof(kMagic))) == 1) {
            bool encOk = true;
            if (!plaintext.empty()) {
                encOk = (EVP_EncryptUpdate(ctx, cipher.data(), &len, plaintext.data(), static_cast<int>(plaintext.size())) == 1);
                total += len;
            }
            if (encOk && EVP_EncryptFinal_ex(ctx, cipher.data() + total, &len) == 1) {
                total += len;
                cipher.resize(static_cast<std::size_t>(total));
                if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagLen), tag) == 1) {
                    ok = true;
                }
            }
        }
        EVP_CIPHER_CTX_free(ctx);
    }
#endif
    volatile std::uint8_t* kp = key;
    for (std::size_t i = 0; i < kKeyLen; ++i) kp[i] = 0;

    if (!ok) {
        if (outErr) *outErr = "AES-256-GCM encryption failed";
        return false;
    }

    outBlob.clear();
    outBlob.reserve(sizeof(kMagic) + kSaltLen + kIvLen + kTagLen + cipher.size());
    outBlob.insert(outBlob.end(), kMagic, kMagic + sizeof(kMagic));
    outBlob.insert(outBlob.end(), salt, salt + kSaltLen);
    outBlob.insert(outBlob.end(), iv, iv + kIvLen);
    outBlob.insert(outBlob.end(), tag, tag + kTagLen);
    outBlob.insert(outBlob.end(), cipher.begin(), cipher.end());
    return true;
}

inline bool DecryptBytes(
    const std::uint8_t* blob,
    std::size_t blobLen,
    const std::string& passphrase,
    std::vector<std::uint8_t>& outPlain,
    std::string* outErr = nullptr)
{
    if (!IsEncryptedCudocBlob(blob, blobLen)) {
        if (outErr) *outErr = "Not an encrypted .cudoc container";
        return false;
    }
    if (passphrase.empty()) {
        if (outErr) *outErr = "Empty decryption passphrase";
        return false;
    }
    const std::uint8_t* salt = blob + sizeof(kMagic);
    const std::uint8_t* iv = salt + kSaltLen;
    const std::uint8_t* tag = iv + kIvLen;
    const std::uint8_t* cipher = tag + kTagLen;
    const std::size_t cipherLen = blobLen - (sizeof(kMagic) + kSaltLen + kIvLen + kTagLen);

    std::uint8_t key[kKeyLen]{};
    if (!DeriveKeyPbkdf2(passphrase, salt, kSaltLen, key)) {
        if (outErr) *outErr = "PBKDF2 derivation failed";
        return false;
    }

    outPlain.assign(cipherLen, 0);
    bool ok = false;

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE,
                reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                static_cast<ULONG>((wcslen(BCRYPT_CHAIN_MODE_GCM) + 1) * sizeof(wchar_t)), 0) == 0 &&
            BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, key, static_cast<ULONG>(kKeyLen), 0) == 0) {
            BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info;
            BCRYPT_INIT_AUTH_MODE_INFO(info);
            info.pbNonce = const_cast<PUCHAR>(iv);
            info.cbNonce = static_cast<ULONG>(kIvLen);
            info.pbAuthData = const_cast<PUCHAR>(kMagic);
            info.cbAuthData = static_cast<ULONG>(sizeof(kMagic));
            info.pbTag = const_cast<PUCHAR>(tag);
            info.cbTag = static_cast<ULONG>(kTagLen);
            ULONG cbRes = 0;
            NTSTATUS st = BCryptDecrypt(
                hKey,
                cipherLen ? const_cast<PUCHAR>(cipher) : nullptr,
                static_cast<ULONG>(cipherLen),
                &info,
                nullptr, 0,
                outPlain.empty() ? nullptr : outPlain.data(),
                static_cast<ULONG>(outPlain.size()),
                &cbRes, 0);
            if (st == 0) {
                outPlain.resize(cbRes);
                ok = true;
            }
            BCryptDestroyKey(hKey);
        }
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
#else
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx) {
        int len = 0, total = 0;
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1 &&
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kIvLen), nullptr) == 1 &&
            EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, iv) == 1 &&
            EVP_DecryptUpdate(ctx, nullptr, &len, kMagic, static_cast<int>(sizeof(kMagic))) == 1) {
            bool decOk = true;
            if (cipherLen > 0) {
                decOk = (EVP_DecryptUpdate(ctx, outPlain.data(), &len, cipher, static_cast<int>(cipherLen)) == 1);
                total += len;
            }
            if (decOk &&
                EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagLen), const_cast<std::uint8_t*>(tag)) == 1 &&
                EVP_DecryptFinal_ex(ctx, outPlain.data() + total, &len) == 1) {
                total += len;
                outPlain.resize(static_cast<std::size_t>(total));
                ok = true;
            }
        }
        EVP_CIPHER_CTX_free(ctx);
    }
#endif
    volatile std::uint8_t* kp = key;
    for (std::size_t i = 0; i < kKeyLen; ++i) kp[i] = 0;

    if (!ok) {
        outPlain.clear();
        if (outErr) *outErr = "AES-256-GCM authentication tag mismatch or wrong password";
        return false;
    }
    return true;
}

inline bool SaveEncryptedCudocFile(
    const std::string& pathUtf8,
    const DocumentModel& doc,
    const std::string& passphrase,
    std::string* outErr = nullptr)
{
    std::vector<std::uint8_t> zipBytes;
    if (!CudocStorage::SerializeToBytes(doc, zipBytes, outErr)) return false;
    std::vector<std::uint8_t> enc;
    if (!EncryptBytes(zipBytes, passphrase, enc, outErr)) return false;
    std::error_code ec;
    const auto p = std::filesystem::u8path(pathUtf8);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (outErr) *outErr = "Unable to open output encrypted .cudoc: " + pathUtf8;
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(enc.data()), static_cast<std::streamsize>(enc.size()));
    ofs.flush();
    return ofs.good();
}

inline bool LoadEncryptedCudocFile(
    const std::string& pathUtf8,
    const std::string& passphrase,
    DocumentModel& outDoc,
    std::string* outErr = nullptr)
{
    std::ifstream ifs(std::filesystem::u8path(pathUtf8), std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        if (outErr) *outErr = "Unable to open encrypted .cudoc: " + pathUtf8;
        return false;
    }
    const auto sz = ifs.tellg();
    if (sz <= 0) {
        if (outErr) *outErr = "Empty encrypted .cudoc file";
        return false;
    }
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(sz));
    ifs.seekg(0, std::ios::beg);
    if (!ifs.read(reinterpret_cast<char*>(raw.data()), sz)) {
        if (outErr) *outErr = "Read failure on encrypted .cudoc";
        return false;
    }
    std::vector<std::uint8_t> plain;
    if (!DecryptBytes(raw.data(), raw.size(), passphrase, plain, outErr)) return false;
    return CudocStorage::DeserializeFromBytes(plain.data(), plain.size(), outDoc, outErr);
}

} // namespace DocStudio::CudocCrypto
