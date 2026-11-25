#include "vault_common.hpp"
#include "academic/tec_digital/crypto.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#endif

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace academic::tec_digital {

namespace {

std::filesystem::path PathFromUtf8(const std::string& utf8_path) {
#ifdef _WIN32
    if (utf8_path.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8_path.data(),
                                     static_cast<int>(utf8_path.size()), nullptr, 0);
    if (needed <= 0) return std::filesystem::path(utf8_path);
    std::wstring w(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8_path.data(),
                        static_cast<int>(utf8_path.size()), w.data(), needed);
    return std::filesystem::path(w);
#else
    return std::filesystem::path(utf8_path);
#endif
}

std::string ReadEnvVarSafe(const char* name) {
#ifdef _WIN32
    char* buf = nullptr;
    size_t len = 0;
    if (_dupenv_s(&buf, &len, name) != 0 || buf == nullptr) {
        return {};
    }
    std::string val(buf);
    SecureZeroMemoryBuffer(buf, val.size());
    std::free(buf);
    return val;
#else
    const char* v = std::getenv(name);
    if (v == nullptr) return {};
    return std::string(v);
#endif
}

}  // namespace

VaultPayload::~VaultPayload() {
    Clear();
}

void VaultPayload::Clear() {
    credentials.Clear();
    for (auto& c : cookies) {
        SecureZeroString(c.value);
    }
    cookies.clear();
    saved_at_utc.clear();
}

std::optional<Credentials> CredentialVault::ResolveFromEnv(std::string* out_error) {
    std::string email = TrimAscii(ReadEnvVarSafe("TEC_EMAIL"));
    std::string password = ReadEnvVarSafe("TEC_PASSWORD");
    if (email.empty() || password.empty()) {
        SecureZeroString(email);
        SecureZeroString(password);
        if (out_error != nullptr) {
            *out_error = "TEC_EMAIL and TEC_PASSWORD environment variables are required";
        }
        return std::nullopt;
    }
    Credentials creds(std::move(email), std::move(password));
    return creds;
}

std::string CredentialVault::CurrentUtcIso8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_utc{};
#ifdef _WIN32
    gmtime_s(&tm_utc, &tt);
#else
    gmtime_r(&tt, &tm_utc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm_utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

std::string CredentialVault::BuildSanitizedSessionMeta(const CookieJar& jar,
                                                       std::string_view saved_at_utc) {
    std::string ts = saved_at_utc.empty() ? CurrentUtcIso8601() : std::string(saved_at_utc);
    return jar.ExportSanitizedMetadataJson(ts);
}

VoidResult CredentialVault::WriteSanitizedSessionMeta(const std::string& path,
                                                      const CookieJar& jar,
                                                      std::string_view saved_at_utc) {
    std::error_code ec;
    std::filesystem::path p = PathFromUtf8(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) {
            return VoidResult::Failure("mkdir_failed", ec.message());
        }
    }
    std::string json = BuildSanitizedSessionMeta(jar, saved_at_utc);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return VoidResult::Failure("open_failed", "unable to open session_meta path");
    }
    ofs.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!ofs.good()) {
        return VoidResult::Failure("write_failed", "failed writing session_meta");
    }
    return VoidResult::Success();
}

void CredentialVault::ClearMemory(Credentials& creds) {
    creds.Clear();
}

void CredentialVault::ClearMemory(VaultPayload& payload) {
    payload.Clear();
}

namespace detail {

std::string SerializeVaultPayloadJson(const Credentials& creds,
                                      const std::vector<Cookie>& cookies,
                                      bool persist_session_cookies,
                                      std::string_view saved_at_utc) {
    nlohmann::json root;
    root["version"] = 1;
    root["email"] = creds.email;
    root["password"] = creds.password;
    root["saved_at_utc"] = std::string(saved_at_utc);
    nlohmann::json carr = nlohmann::json::array();
    if (persist_session_cookies) {
        for (const auto& c : cookies) {
            carr.push_back({
                {"name", c.name},
                {"value", c.value},
                {"domain", c.domain},
                {"path", c.path},
                {"expires_epoch", c.expires_epoch},
                {"has_expires", c.has_expires},
                {"host_only", c.host_only},
                {"secure", c.secure},
                {"http_only", c.http_only},
                {"discard", c.discard},
                {"same_site", c.same_site}
            });
        }
    }
    root["cookies"] = std::move(carr);
    return root.dump();
}

bool DeserializeVaultPayloadJson(const uint8_t* data,
                                 size_t len,
                                 VaultPayload& out_payload,
                                 std::string& out_error) {
    out_payload.Clear();
    try {
        std::string_view sv(reinterpret_cast<const char*>(data), len);
        auto j = nlohmann::json::parse(sv);
        if (!j.is_object()) {
            out_error = "vault JSON is not an object";
            return false;
        }
        out_payload.version = j.value("version", 1);
        out_payload.credentials.email = j.value("email", "");
        out_payload.credentials.password = j.value("password", "");
        out_payload.saved_at_utc = j.value("saved_at_utc", "");
        if (j.contains("cookies") && j["cookies"].is_array()) {
            for (const auto& item : j["cookies"]) {
                if (!item.is_object()) continue;
                Cookie c;
                c.name = item.value("name", "");
                c.value = item.value("value", "");
                c.domain = item.value("domain", "");
                c.path = item.value("path", "/");
                c.expires_epoch = item.value("expires_epoch", int64_t{0});
                c.has_expires = item.value("has_expires", false);
                c.host_only = item.value("host_only", true);
                c.secure = item.value("secure", false);
                c.http_only = item.value("http_only", false);
                c.discard = item.value("discard", false);
                c.same_site = item.value("same_site", "");
                if (!c.name.empty()) {
                    out_payload.cookies.push_back(std::move(c));
                }
            }
        }
        return true;
    } catch (const std::exception& ex) {
        out_payload.Clear();
        out_error = ex.what();
        return false;
    }
}

}  // namespace detail

}  // namespace academic::tec_digital
