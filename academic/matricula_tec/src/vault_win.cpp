#include "academic/matricula_tec/vault.hpp"

#include <nlohmann/json.hpp>

#include <windows.h>
#include <dpapi.h>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace academic::matricula_tec {

namespace {

void ZeroString(std::string& s) noexcept {
    if (!s.empty()) {
        SecureZeroMemory(s.data(), s.size());
        s.clear();
    }
}

std::string CurrentUtcIso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm utc_tm{};
    gmtime_s(&utc_tm, &tt);
    char buf[32]{};
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &utc_tm);
    return std::string(buf);
}

} // namespace

ProtectedCredentials::~ProtectedCredentials() {
    SecureClear();
}

void ProtectedCredentials::SecureClear() noexcept {
    ZeroString(username);
    ZeroString(password);
}

ProtectedCredentialVault::~ProtectedCredentialVault() {
    ClearMemory();
}

bool ProtectedCredentialVault::StoreInMemory(const std::string& username,
                                             const std::string& password,
                                             std::string* out_err)
{
    ClearMemory();
    if (username.empty() || password.empty()) {
        if (out_err) *out_err = "Usuario y contrasena requeridos.";
        return false;
    }

    std::string plain = json{
        { "v", 1 },
        { "username", username },
        { "password", password }
    }.dump();

    const size_t block = CRYPTPROTECTMEMORY_BLOCK_SIZE; // 16 bytes
    const size_t padded = ((plain.size() + block - 1u) / block) * block;
    std::string buf(padded, '\0');
    std::memcpy(buf.data(), plain.data(), plain.size());
    ZeroString(plain);

    if (!CryptProtectMemory(buf.data(), static_cast<DWORD>(buf.size()), CRYPTPROTECTMEMORY_SAME_PROCESS)) {
        const DWORD err = GetLastError();
        ZeroString(buf);
        if (out_err) *out_err = "CryptProtectMemory fallo (win32=" + std::to_string(err) + ")";
        return false;
    }

    ram_blob_ = std::move(buf);
    ram_plain_len_ = padded;
    return true;
}

bool ProtectedCredentialVault::HasMemoryCredentials() const noexcept {
    return !ram_blob_.empty() && ram_plain_len_ > 0;
}

bool ProtectedCredentialVault::RetrieveFromMemory(ProtectedCredentials& out_creds,
                                                  std::string* out_err) const
{
    out_creds.SecureClear();
    if (!HasMemoryCredentials()) {
        if (out_err) *out_err = "No hay credenciales cargadas en memoria.";
        return false;
    }

    std::string copy = ram_blob_;
    if (!CryptUnprotectMemory(copy.data(), static_cast<DWORD>(copy.size()), CRYPTPROTECTMEMORY_SAME_PROCESS)) {
        const DWORD err = GetLastError();
        ZeroString(copy);
        if (out_err) *out_err = "CryptUnprotectMemory fallo (win32=" + std::to_string(err) + ")";
        return false;
    }

    bool ok = false;
    try {
        const size_t nul = copy.find('\0');
        const std::string_view sv = (nul == std::string::npos) ? std::string_view(copy) : std::string_view(copy.data(), nul);
        json parsed = json::parse(sv);
        out_creds.username = parsed.value("username", "");
        out_creds.password = parsed.value("password", "");
        ok = !out_creds.username.empty() && !out_creds.password.empty();
    } catch (const std::exception& ex) {
        if (out_err) *out_err = ex.what();
    }
    ZeroString(copy);
    return ok;
}

void ProtectedCredentialVault::ClearMemory() noexcept {
    ZeroString(ram_blob_);
    ram_plain_len_ = 0;
}

bool ProtectedCredentialVault::SaveToDpapiFile(const std::string& path_utf8,
                                               const std::string& username,
                                               const std::string& password,
                                               std::string* out_err)
{
    if (username.empty() || password.empty()) {
        if (out_err) *out_err = "Usuario y contrasena requeridos para DPAPI.";
        return false;
    }

    std::string plain = json{
        { "v", 1 },
        { "username", username },
        { "password", password },
        { "saved_utc", CurrentUtcIso8601() }
    }.dump();

    DATA_BLOB in_blob{};
    in_blob.cbData = static_cast<DWORD>(plain.size());
    in_blob.pbData = reinterpret_cast<BYTE*>(plain.data());

    DATA_BLOB out_blob{};
    const BOOL ok = CryptProtectData(
        &in_blob,
        L"CuadernU.MatriculaTEC.v1",
        nullptr,
        nullptr,
        nullptr,
        CRYPTPROTECT_UI_FORBIDDEN,
        &out_blob);

    ZeroString(plain);
    if (!ok) {
        if (out_err) *out_err = "CryptProtectData fallo (win32=" + std::to_string(GetLastError()) + ")";
        return false;
    }

    bool written = false;
    try {
        const fs::path p = fs::u8path(path_utf8);
        if (p.has_parent_path()) fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        if (ofs) {
            ofs.write(reinterpret_cast<const char*>(out_blob.pbData),
                      static_cast<std::streamsize>(out_blob.cbData));
            written = static_cast<bool>(ofs);
        } else if (out_err) {
            *out_err = "No se pudo abrir archivo DPAPI para escritura.";
        }
    } catch (const std::exception& ex) {
        if (out_err) *out_err = ex.what();
    }

    if (out_blob.pbData) {
        SecureZeroMemory(out_blob.pbData, out_blob.cbData);
        LocalFree(out_blob.pbData);
    }
    return written;
}

bool ProtectedCredentialVault::LoadFromDpapiFile(const std::string& path_utf8,
                                                 ProtectedCredentials& out_creds,
                                                 std::string* out_err)
{
    out_creds.SecureClear();
    std::vector<BYTE> cipher;
    try {
        const fs::path p = fs::u8path(path_utf8);
        std::ifstream ifs(p, std::ios::binary);
        if (!ifs) {
            if (out_err) *out_err = "Archivo DPAPI no existe.";
            return false;
        }
        cipher.assign(std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>());
    } catch (const std::exception& ex) {
        if (out_err) *out_err = ex.what();
        return false;
    }

    if (cipher.empty()) {
        if (out_err) *out_err = "Archivo DPAPI vacio.";
        return false;
    }

    DATA_BLOB in_blob{};
    in_blob.cbData = static_cast<DWORD>(cipher.size());
    in_blob.pbData = cipher.data();

    DATA_BLOB out_blob{};
    LPWSTR desc = nullptr;
    if (!CryptUnprotectData(&in_blob, &desc, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        if (out_err) *out_err = "CryptUnprotectData fallo (win32=" + std::to_string(GetLastError()) + ")";
        return false;
    }

    bool ok = false;
    try {
        const std::string_view sv(reinterpret_cast<const char*>(out_blob.pbData), out_blob.cbData);
        json parsed = json::parse(sv);
        out_creds.username = parsed.value("username", "");
        out_creds.password = parsed.value("password", "");
        ok = !out_creds.username.empty() && !out_creds.password.empty();
    } catch (const std::exception& ex) {
        if (out_err) *out_err = ex.what();
    }

    if (desc) LocalFree(desc);
    if (out_blob.pbData) {
        SecureZeroMemory(out_blob.pbData, out_blob.cbData);
        LocalFree(out_blob.pbData);
    }
    return ok;
}

bool ProtectedCredentialVault::DpapiFileExists(const std::string& path_utf8) {
    std::error_code ec;
    return fs::is_regular_file(fs::u8path(path_utf8), ec);
}

bool ProtectedCredentialVault::DeleteDpapiFile(const std::string& path_utf8, std::string* out_err) {
    std::error_code ec;
    const fs::path p = fs::u8path(path_utf8);
    if (!fs::exists(p, ec)) return true;
    // Overwrite bytes with zeros before removing encrypted vault
    {
        std::ofstream ofs(p, std::ios::binary | std::ios::in | std::ios::out);
        if (ofs) {
            ofs.seekp(0, std::ios::end);
            const auto sz = ofs.tellp();
            if (sz > 0) {
                ofs.seekp(0, std::ios::beg);
                std::vector<char> zeros(static_cast<size_t>(sz), '\0');
                ofs.write(zeros.data(), sz);
                ofs.flush();
            }
        }
    }
    const bool removed = fs::remove(p, ec);
    if (!removed && out_err) *out_err = ec.message();
    return removed;
}

} // namespace academic::matricula_tec
