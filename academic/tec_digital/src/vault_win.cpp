#include "academic/tec_digital/vault.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "vault_common.hpp"

#include <windows.h>
#include <wincrypt.h>

#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace academic::tec_digital {

namespace {

constexpr uint8_t kMagic[4] = {'T', 'D', 'V', '1'};
constexpr uint8_t kVersion = 0x01;
constexpr uint8_t kBackendWinDpapi = 0x01;
constexpr char kEntropy[] = "CuadernU.TecDigital.Vault.v1";

std::wstring Utf8ToWide(std::string_view s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return {};
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), needed);
    return w;
}

std::string WideToUtf8(std::wstring_view w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return {};
    std::string s(static_cast<size_t>(needed), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()),
                        s.data(), needed, nullptr, nullptr);
    return s;
}

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

}  // namespace

std::string CredentialVault::DefaultVaultPath() {
    wchar_t* local_app_data = nullptr;
    size_t len = 0;
    std::filesystem::path base;
    if (_wdupenv_s(&local_app_data, &len, L"LOCALAPPDATA") == 0 && local_app_data != nullptr) {
        base = std::filesystem::path(local_app_data);
        std::free(local_app_data);
    } else {
        base = std::filesystem::temp_directory_path();
    }
    return WideToUtf8((base / L"CalculadoraUnificada" / L"tec_digital" / L"vault.tdv").native());
}

VoidResult CredentialVault::SaveEncrypted(const std::string& path,
                                          const Credentials& creds,
                                          const std::vector<Cookie>& cookies,
                                          bool persist_session_cookies,
                                          const std::string& /*key_override*/) {
    std::string ts = CurrentUtcIso8601();
    std::string plain = detail::SerializeVaultPayloadJson(creds, cookies, persist_session_cookies, ts);

    DATA_BLOB in_blob{};
    in_blob.pbData = reinterpret_cast<BYTE*>(plain.data());
    in_blob.cbData = static_cast<DWORD>(plain.size());

    DATA_BLOB entropy_blob{};
    entropy_blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
    entropy_blob.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);

    DATA_BLOB out_blob{};
    BOOL ok = CryptProtectData(&in_blob, L"CuadernU.TecDigital.Vault",
                               &entropy_blob, nullptr, nullptr,
                               CRYPTPROTECT_UI_FORBIDDEN, &out_blob);
    SecureZeroString(plain);
    if (!ok) {
        return VoidResult::Failure("dpapi_protect_failed",
                                   "CryptProtectData failed (" + std::to_string(GetLastError()) + ")");
    }

    std::vector<uint8_t> container;
    container.reserve(12 + out_blob.cbData);
    container.insert(container.end(), kMagic, kMagic + 4);
    container.push_back(kVersion);
    container.push_back(kBackendWinDpapi);
    uint8_t flags = 0;
    if (!creds.email.empty() || !creds.password.empty()) flags |= 0x01;
    if (persist_session_cookies && !cookies.empty()) flags |= 0x02;
    container.push_back(flags);
    container.push_back(0x00);
    WriteU32LE(container, static_cast<uint32_t>(out_blob.cbData));
    container.insert(container.end(), out_blob.pbData, out_blob.pbData + out_blob.cbData);
    SecureZeroMemoryBuffer(out_blob.pbData, out_blob.cbData);
    LocalFree(out_blob.pbData);

    std::error_code ec;
    std::filesystem::path target(Utf8ToWide(path));
    if (target.has_parent_path()) {
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec) {
            return VoidResult::Failure("mkdir_failed", ec.message());
        }
    }

    std::filesystem::path tmp = target;
    tmp += L".tmp";
    std::wstring wtmp = tmp.native();
    std::wstring wtarget = target.native();

    HANDLE hFile = CreateFileW(wtmp.c_str(), GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return VoidResult::Failure("open_tmp_failed",
                                   "CreateFileW failed (" + std::to_string(GetLastError()) + ")");
    }

    DWORD written = 0;
    BOOL w_ok = WriteFile(hFile, container.data(), static_cast<DWORD>(container.size()), &written, nullptr);
    if (w_ok && written == container.size()) {
        FlushFileBuffers(hFile);
    } else {
        w_ok = FALSE;
    }
    CloseHandle(hFile);

    if (!w_ok) {
        DeleteFileW(wtmp.c_str());
        return VoidResult::Failure("write_tmp_failed", "WriteFile failed");
    }

    if (!MoveFileExW(wtmp.c_str(), wtarget.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DWORD err = GetLastError();
        DeleteFileW(wtmp.c_str());
        return VoidResult::Failure("atomic_rename_failed",
                                   "MoveFileExW failed (" + std::to_string(err) + ")");
    }

    return VoidResult::Success();
}

Result<VaultPayload> CredentialVault::LoadEncrypted(const std::string& path,
                                                    const std::string& /*key_override*/) {
    std::wstring wtarget = Utf8ToWide(path);
    HANDLE hFile = CreateFileW(wtarget.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return Result<VaultPayload>::Failure("vault_not_found", "unable to open vault file");
    }

    LARGE_INTEGER file_size{};
    if (!GetFileSizeEx(hFile, &file_size) || file_size.QuadPart < 12 ||
        file_size.QuadPart > 1024 * 1024) {
        CloseHandle(hFile);
        return Result<VaultPayload>::Failure("invalid_vault_size", "vault size out of bounds");
    }

    std::vector<uint8_t> buf(static_cast<size_t>(file_size.QuadPart));
    DWORD read_bytes = 0;
    BOOL r_ok = ReadFile(hFile, buf.data(), static_cast<DWORD>(buf.size()), &read_bytes, nullptr);
    CloseHandle(hFile);
    if (!r_ok || read_bytes != buf.size()) {
        return Result<VaultPayload>::Failure("vault_read_failed", "failed reading vault file");
    }

    if (std::memcmp(buf.data(), kMagic, 4) != 0 || buf[4] != kVersion || buf[5] != kBackendWinDpapi) {
        return Result<VaultPayload>::Failure("invalid_vault_header", "invalid TDV1 header");
    }

    uint32_t dpapi_len = ReadU32LE(buf.data() + 8);
    if (static_cast<size_t>(dpapi_len) != buf.size() - 12 || dpapi_len == 0) {
        return Result<VaultPayload>::Failure("truncated_vault", "DPAPI payload length mismatch");
    }

    DATA_BLOB in_blob{};
    in_blob.pbData = buf.data() + 12;
    in_blob.cbData = dpapi_len;

    DATA_BLOB entropy_blob{};
    entropy_blob.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(kEntropy));
    entropy_blob.cbData = static_cast<DWORD>(sizeof(kEntropy) - 1);

    DATA_BLOB out_blob{};
    if (!CryptUnprotectData(&in_blob, nullptr, &entropy_blob, nullptr, nullptr,
                            CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        return Result<VaultPayload>::Failure(
            "dpapi_unprotect_failed",
            "CryptUnprotectData failed (" + std::to_string(GetLastError()) + ")");
    }

    VaultPayload payload;
    std::string parse_err;
    bool ok = detail::DeserializeVaultPayloadJson(out_blob.pbData, out_blob.cbData, payload, parse_err);
    SecureZeroMemoryBuffer(out_blob.pbData, out_blob.cbData);
    LocalFree(out_blob.pbData);

    if (!ok) {
        return Result<VaultPayload>::Failure("vault_corrupt_json", parse_err);
    }
    return Result<VaultPayload>::Success(std::move(payload));
}

}  // namespace academic::tec_digital
