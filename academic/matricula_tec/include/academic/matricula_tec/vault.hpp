#pragma once

#include <string>

namespace academic::matricula_tec {

struct ProtectedCredentials {
    std::string username;
    std::string password;

    ~ProtectedCredentials();
    void SecureClear() noexcept;
};

// RAM protection (CryptProtectMemory on Windows) + opt-in Windows DPAPI file vault
// binary-compatible with %LOCALAPPDATA%\CuadernU\matricula_vault.dpapi used by Python MCP.
class ProtectedCredentialVault {
public:
    ProtectedCredentialVault() = default;
    ~ProtectedCredentialVault();

    ProtectedCredentialVault(const ProtectedCredentialVault&) = delete;
    ProtectedCredentialVault& operator=(const ProtectedCredentialVault&) = delete;

    // Stores credentials encrypted in RAM (CryptProtectMemory).
    bool StoreInMemory(const std::string& username, const std::string& password, std::string* out_err = nullptr);
    bool HasMemoryCredentials() const noexcept;
    bool RetrieveFromMemory(ProtectedCredentials& out_creds, std::string* out_err = nullptr) const;
    void ClearMemory() noexcept;

    // Opt-in persistent Windows DPAPI vault (CurrentUser scope, CRYPTPROTECT_UI_FORBIDDEN).
    static bool SaveToDpapiFile(const std::string& path_utf8,
                                const std::string& username,
                                const std::string& password,
                                std::string* out_err = nullptr);
    static bool LoadFromDpapiFile(const std::string& path_utf8,
                                  ProtectedCredentials& out_creds,
                                  std::string* out_err = nullptr);
    static bool DpapiFileExists(const std::string& path_utf8);
    static bool DeleteDpapiFile(const std::string& path_utf8, std::string* out_err = nullptr);

private:
    std::string ram_blob_; // encrypted multiple-of-16 bytes in RAM
    size_t ram_plain_len_ = 0;
};

} // namespace academic::matricula_tec
