#include "academic/tec_digital/vault.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "vault_common.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <openssl/evp.h>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace academic::tec_digital {

namespace {

constexpr uint8_t kMagic[4] = {'T', 'D', 'V', '1'};
constexpr uint8_t kVersion = 0x01;
constexpr uint8_t kBackendLinuxGcm = 0x02;
constexpr uint32_t kMinPbkdf2Iterations = 200000;
constexpr size_t kSaltLen = 16;
constexpr size_t kIvLen = 12;
constexpr size_t kTagLen = 16;
constexpr size_t kAadLen = 40;  // 8B header + 4B iter + 16B salt + 12B iv
constexpr size_t kMinVaultBytes = kAadLen + kTagLen + 4;  // 60 bytes

void WriteU32LE(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

uint32_t ReadU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

std::string ReadMachineId() {
    const char* candidates[] = {"/etc/machine-id", "/var/lib/dbus/machine-id"};
    for (const char* path : candidates) {
        std::ifstream ifs(path);
        if (ifs.is_open()) {
            std::string content;
            std::getline(ifs, content);
            content = TrimAscii(content);
            if (!content.empty()) return content;
        }
    }
    return {};
}

bool DeriveVaultKey(const std::string& key_override,
                    const uint8_t* salt,
                    uint32_t iterations,
                    std::vector<uint8_t>& out_key,
                    std::string& out_err) {
    if (iterations < kMinPbkdf2Iterations) {
        out_err = "PBKDF2 iteration count below minimum";
        return false;
    }

    uid_t uid = ::getuid();
    std::string pw_name = "uid_" + std::to_string(uid);
    if (struct passwd* pw = ::getpwuid(uid)) {
        if (pw->pw_name != nullptr) {
            pw_name = pw->pw_name;
        }
    }

    std::string ikm;
    if (!key_override.empty()) {
        ikm = "CuadernU.TecDigital.Vault.v1|override|" + std::to_string(uid) + "|" +
              pw_name + "|" + key_override;
    } else {
        std::string machine_id = ReadMachineId();
        if (machine_id.empty()) {
            out_err = "missing /etc/machine-id and no key_override provided (fail closed)";
            return false;
        }
        ikm = "CuadernU.TecDigital.Vault.v1|" + machine_id + "|" +
              std::to_string(uid) + "|" + pw_name;
    }

    out_key.assign(32, 0);
    int rc = PKCS5_PBKDF2_HMAC(ikm.data(), static_cast<int>(ikm.size()),
                               salt, static_cast<int>(kSaltLen),
                               static_cast<int>(iterations),
                               EVP_sha256(),
                               32, out_key.data());
    SecureZeroString(ikm);
    if (rc != 1) {
        SecureZeroBytes(out_key);
        out_err = "PKCS5_PBKDF2_HMAC failed";
        return false;
    }
    return true;
}

bool EnsureSecureDir0700(const std::filesystem::path& dir, std::string& out_err) {
    if (dir.empty()) return true;
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        out_err = "create_directories failed: " + ec.message();
        return false;
    }
    if (::chmod(dir.c_str(), 0700) != 0) {
        out_err = std::string("chmod 0700 failed: ") + std::strerror(errno);
        return false;
    }
    return true;
}

}  // namespace

std::string CredentialVault::DefaultVaultPath() {
    std::filesystem::path base;
    if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg != nullptr && xdg[0] != '\0') {
        base = std::filesystem::path(xdg);
    } else if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        base = std::filesystem::path(home) / ".local" / "share";
    } else {
        base = std::filesystem::temp_directory_path();
    }
    return (base / "calculadora-unificada" / "tec_digital" / "vault.tdv").string();
}

VoidResult CredentialVault::SaveEncrypted(const std::string& path,
                                          const Credentials& creds,
                                          const std::vector<Cookie>& cookies,
                                          bool persist_session_cookies,
                                          const std::string& key_override) {
    std::filesystem::path target(path);
    std::string err;
    if (target.has_parent_path()) {
        if (!EnsureSecureDir0700(target.parent_path(), err)) {
            return VoidResult::Failure("dir_permission_failed", err);
        }
    }

    std::vector<uint8_t> salt = RandomBytes(kSaltLen);
    std::vector<uint8_t> iv = RandomBytes(kIvLen);
    if (salt.size() != kSaltLen || iv.size() != kIvLen) {
        return VoidResult::Failure("rng_failed", "RAND_bytes failed");
    }

    std::vector<uint8_t> key;
    if (!DeriveVaultKey(key_override, salt.data(), kMinPbkdf2Iterations, key, err)) {
        return VoidResult::Failure("kdf_failed", err);
    }

    std::string ts = CurrentUtcIso8601();
    std::string plain = detail::SerializeVaultPayloadJson(creds, cookies, persist_session_cookies, ts);

    std::vector<uint8_t> aad;
    aad.reserve(kAadLen);
    aad.insert(aad.end(), kMagic, kMagic + 4);
    aad.push_back(kVersion);
    aad.push_back(kBackendLinuxGcm);
    uint8_t flags = 0;
    if (!creds.email.empty() || !creds.password.empty()) flags |= 0x01;
    if (persist_session_cookies && !cookies.empty()) flags |= 0x02;
    aad.push_back(flags);
    aad.push_back(0x00);
    WriteU32LE(aad, kMinPbkdf2Iterations);
    aad.insert(aad.end(), salt.begin(), salt.end());
    aad.insert(aad.end(), iv.begin(), iv.end());

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        SecureZeroBytes(key);
        SecureZeroString(plain);
        return VoidResult::Failure("evp_failed", "EVP_CIPHER_CTX_new failed");
    }

    std::vector<uint8_t> ct(plain.size() + 16);
    std::vector<uint8_t> tag(kTagLen);
    int out_len1 = 0;
    int out_len2 = 0;
    int aad_len = 0;

    bool ok = (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1) &&
              (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kIvLen), nullptr) == 1) &&
              (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) == 1) &&
              (EVP_EncryptUpdate(ctx, nullptr, &aad_len, aad.data(), static_cast<int>(aad.size())) == 1) &&
              (EVP_EncryptUpdate(ctx, ct.data(), &out_len1,
                                 reinterpret_cast<const uint8_t*>(plain.data()),
                                 static_cast<int>(plain.size())) == 1) &&
              (EVP_EncryptFinal_ex(ctx, ct.data() + out_len1, &out_len2) == 1) &&
              (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, static_cast<int>(kTagLen), tag.data()) == 1);

    EVP_CIPHER_CTX_free(ctx);
    SecureZeroBytes(key);
    SecureZeroString(plain);

    if (!ok) {
        return VoidResult::Failure("gcm_encrypt_failed", "AES-256-GCM encryption failed");
    }
    const uint32_t ct_len = static_cast<uint32_t>(out_len1 + out_len2);
    ct.resize(ct_len);

    std::vector<uint8_t> container;
    container.reserve(kAadLen + kTagLen + 4 + ct_len);
    container.insert(container.end(), aad.begin(), aad.end());
    container.insert(container.end(), tag.begin(), tag.end());
    WriteU32LE(container, ct_len);
    container.insert(container.end(), ct.begin(), ct.end());

    std::string tmp_path = path + ".tmp." + std::to_string(::getpid());
    ::unlink(tmp_path.c_str());
    int fd = ::open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (fd < 0) {
        return VoidResult::Failure("open_tmp_failed", std::strerror(errno));
    }
    if (::fchmod(fd, 0600) != 0) {
        ::close(fd);
        ::unlink(tmp_path.c_str());
        return VoidResult::Failure("fchmod_failed", std::strerror(errno));
    }

    size_t written = 0;
    while (written < container.size()) {
        ssize_t n = ::write(fd, container.data() + written, container.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            err = std::strerror(errno);
            ::close(fd);
            ::unlink(tmp_path.c_str());
            return VoidResult::Failure("write_failed", err);
        }
        written += static_cast<size_t>(n);
    }

    if (::fsync(fd) != 0) {
        err = std::strerror(errno);
        ::close(fd);
        ::unlink(tmp_path.c_str());
        return VoidResult::Failure("fsync_failed", err);
    }
    ::close(fd);

    if (::rename(tmp_path.c_str(), path.c_str()) != 0) {
        err = std::strerror(errno);
        ::unlink(tmp_path.c_str());
        return VoidResult::Failure("rename_failed", err);
    }

    if (target.has_parent_path()) {
        int dir_fd = ::open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY);
        if (dir_fd >= 0) {
            ::fsync(dir_fd);
            ::close(dir_fd);
        }
    }

    return VoidResult::Success();
}

Result<VaultPayload> CredentialVault::LoadEncrypted(const std::string& path,
                                                    const std::string& key_override) {
    int fd = ::open(path.c_str(), O_RDONLY | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ELOOP) {
            return Result<VaultPayload>::Failure("symlink_rejected", "vault path is a symlink");
        }
        return Result<VaultPayload>::Failure("vault_not_found", std::strerror(errno));
    }

    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        return Result<VaultPayload>::Failure("fstat_failed", std::strerror(errno));
    }
    if (!S_ISREG(st.st_mode)) {
        ::close(fd);
        return Result<VaultPayload>::Failure("not_regular_file", "vault is not a regular file");
    }
    if (st.st_uid != ::geteuid()) {
        ::close(fd);
        return Result<VaultPayload>::Failure("wrong_owner", "vault owner mismatch");
    }
    if ((st.st_mode & 077) != 0) {
        ::close(fd);
        return Result<VaultPayload>::Failure("permissive_mode", "vault file permissions must be 0600");
    }
    if (st.st_size < static_cast<off_t>(kMinVaultBytes) || st.st_size > 1024 * 1024) {
        ::close(fd);
        return Result<VaultPayload>::Failure("invalid_vault_size", "vault size out of bounds");
    }

    std::vector<uint8_t> buf(static_cast<size_t>(st.st_size));
    size_t total = 0;
    while (total < buf.size()) {
        ssize_t n = ::read(fd, buf.data() + total, buf.size() - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            ::close(fd);
            return Result<VaultPayload>::Failure("read_failed", std::strerror(errno));
        }
        if (n == 0) break;
        total += static_cast<size_t>(n);
    }
    ::close(fd);
    if (total != buf.size()) {
        return Result<VaultPayload>::Failure("short_read", "incomplete vault read");
    }

    if (std::memcmp(buf.data(), kMagic, 4) != 0 || buf[4] != kVersion || buf[5] != kBackendLinuxGcm) {
        return Result<VaultPayload>::Failure("invalid_vault_header", "invalid TDV1 header");
    }

    const uint32_t iterations = ReadU32LE(buf.data() + 8);
    const uint8_t* salt = buf.data() + 12;
    const uint8_t* iv = buf.data() + 28;
    const uint8_t* tag = buf.data() + 40;
    const uint32_t ct_len = ReadU32LE(buf.data() + 56);

    if (static_cast<size_t>(ct_len) != buf.size() - kMinVaultBytes) {
        return Result<VaultPayload>::Failure("truncated_vault", "ciphertext length mismatch");
    }

    std::vector<uint8_t> key;
    std::string err;
    if (!DeriveVaultKey(key_override, salt, iterations, key, err)) {
        return Result<VaultPayload>::Failure("kdf_failed", err);
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (ctx == nullptr) {
        SecureZeroBytes(key);
        return Result<VaultPayload>::Failure("evp_failed", "EVP_CIPHER_CTX_new failed");
    }

    std::vector<uint8_t> plain(ct_len + 16);
    int out_len1 = 0;
    int out_len2 = 0;
    int aad_len = 0;

    bool ok = (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1) &&
              (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(kIvLen), nullptr) == 1) &&
              (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv) == 1) &&
              (EVP_DecryptUpdate(ctx, nullptr, &aad_len, buf.data(), static_cast<int>(kAadLen)) == 1) &&
              (ct_len == 0 || EVP_DecryptUpdate(ctx, plain.data(), &out_len1,
                                                buf.data() + kMinVaultBytes,
                                                static_cast<int>(ct_len)) == 1) &&
              (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(kTagLen),
                                   const_cast<uint8_t*>(tag)) == 1) &&
              (EVP_DecryptFinal_ex(ctx, plain.data() + out_len1, &out_len2) == 1);

    EVP_CIPHER_CTX_free(ctx);
    SecureZeroBytes(key);

    if (!ok) {
        SecureZeroBytes(plain);
        return Result<VaultPayload>::Failure("gcm_auth_failed", "GCM authentication tag verification failed");
    }
    plain.resize(static_cast<size_t>(out_len1 + out_len2));

    VaultPayload payload;
    bool json_ok = detail::DeserializeVaultPayloadJson(plain.data(), plain.size(), payload, err);
    SecureZeroBytes(plain);

    if (!json_ok) {
        return Result<VaultPayload>::Failure("vault_corrupt_json", err);
    }
    return Result<VaultPayload>::Success(std::move(payload));
}

}  // namespace academic::tec_digital
