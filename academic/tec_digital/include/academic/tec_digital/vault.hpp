#pragma once

#include "academic/tec_digital/cookie_jar.hpp"
#include "academic/tec_digital/types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

enum class VaultPersistenceMode {
    MemoryOnly,
    EncryptedVault
};

struct VaultConfig {
    VaultPersistenceMode mode = VaultPersistenceMode::MemoryOnly;
    std::string vault_path;
    std::string session_meta_path;
    bool persist_session_cookies = false;
    std::string key_override;  // Optional Linux testing/passphrase override
};

struct VaultPayload {
    int version = 1;
    Credentials credentials;
    std::string saved_at_utc;
    std::vector<Cookie> cookies;

    VaultPayload() = default;
    VaultPayload(const VaultPayload&) = default;
    VaultPayload& operator=(const VaultPayload&) = default;
    VaultPayload(VaultPayload&&) noexcept = default;
    VaultPayload& operator=(VaultPayload&&) noexcept = default;
    ~VaultPayload();

    void Clear();
};

class CredentialVault {
public:
    static std::optional<Credentials> ResolveFromEnv(std::string* out_error = nullptr);
    static std::string DefaultVaultPath();
    static std::string CurrentUtcIso8601();

    static VoidResult SaveEncrypted(const std::string& path,
                                    const Credentials& creds,
                                    const std::vector<Cookie>& cookies = {},
                                    bool persist_session_cookies = false,
                                    const std::string& key_override = "");

    static Result<VaultPayload> LoadEncrypted(const std::string& path,
                                              const std::string& key_override = "");

    static std::string BuildSanitizedSessionMeta(const CookieJar& jar,
                                                 std::string_view saved_at_utc = "");

    static VoidResult WriteSanitizedSessionMeta(const std::string& path,
                                                const CookieJar& jar,
                                                std::string_view saved_at_utc = "");

    static void ClearMemory(Credentials& creds);
    static void ClearMemory(VaultPayload& payload);
};

}  // namespace academic::tec_digital
