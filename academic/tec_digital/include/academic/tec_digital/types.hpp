#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace academic::tec_digital {

template <typename T>
struct Result {
    bool ok = false;
    T value{};
    std::string error_code;
    std::string error_message;

    static Result<T> Success(T val) {
        Result<T> r;
        r.ok = true;
        r.value = std::move(val);
        return r;
    }

    static Result<T> Failure(std::string code, std::string msg) {
        Result<T> r;
        r.ok = false;
        r.error_code = std::move(code);
        r.error_message = std::move(msg);
        return r;
    }
};

struct VoidResult {
    bool ok = false;
    std::string error_code;
    std::string error_message;

    static VoidResult Success() {
        VoidResult r;
        r.ok = true;
        return r;
    }

    static VoidResult Failure(std::string code, std::string msg) {
        VoidResult r;
        r.ok = false;
        r.error_code = std::move(code);
        r.error_message = std::move(msg);
        return r;
    }
};

struct HttpResponse;

struct HttpRequest {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    uint64_t range_start = 0;
    uint64_t range_end = 0;
    int connect_timeout_ms = 10000;
    int send_timeout_ms = 30000;
    int receive_timeout_ms = 30000;
    bool follow_redirects = false;
    int max_redirects = 5;
    bool allow_insecure_tls = false;
    std::function<bool(const HttpResponse& headers_meta)> header_callback;
    std::function<bool(const uint8_t* chunk, size_t len)> chunk_callback;
    std::function<bool()> cancel_check;
};

struct HttpResponse {
    int status_code = 0;
    std::string final_url;
    std::string content_type;
    std::string location;
    std::string content_range;
    uint64_t content_length = 0;
    bool has_content_length = false;
    std::vector<std::pair<std::string, std::string>> headers;
    std::vector<std::string> set_cookies;
    std::vector<uint8_t> body;
    std::string error_message;
    bool transport_ok = false;
    bool tls_verified = false;

    std::string GetHeaderCI(std::string_view name) const;
    std::vector<std::string> GetHeadersCI(std::string_view name) const;
    std::string BodyAsString() const;
};

struct Cookie {
    std::string name;
    std::string value;
    std::string domain;
    std::string path = "/";
    int64_t expires_epoch = 0;
    bool has_expires = false;
    bool host_only = true;
    bool secure = false;
    bool http_only = false;
    bool discard = false;
    std::string same_site;
};

struct CookieMeta {
    std::string name;
    std::string domain;
    std::string path = "/";
    int64_t expires_epoch = 0;
    bool has_expires = false;
    bool host_only = true;
    bool secure = false;
    bool http_only = false;
    bool discard = false;
    std::string same_site;
    std::string sha256_prefix;
};

struct Credentials {
    std::string email;
    std::string password;

    Credentials() = default;
    Credentials(std::string e, std::string p)
        : email(std::move(e)), password(std::move(p)) {}
    Credentials(const Credentials&) = default;
    Credentials& operator=(const Credentials&) = default;
    Credentials(Credentials&& other) noexcept;
    Credentials& operator=(Credentials&& other) noexcept;
    ~Credentials();

    void Clear();
};

enum class AuthStatus {
    Ok,
    NoAccount,
    BadPassword,
    EmptyCredentials,
    InvalidRetoken,
    LoginExpired,
    InvalidCaptcha,
    BackendWsError,
    TooManyAttempts,
    UnknownServerError,
    DaticAuthError,
    DaticBadPassword,
    DaticWsError,
    ActionRedirect,
    AccountMessage,
    AccountClosed,
    InterstitialRedirect,
    UnknownError,
    MissingSessionCookie,
    PortalVerificationFailed,
    TransportError,
    PolicyBlocked,
    InvalidJsonResponse,
    MissingQrEmailClaim
};

struct AuthResult {
    bool ok = false;
    AuthStatus status = AuthStatus::UnknownError;
    std::string status_string;
    std::string return_url;
    std::string message;
    int error_code = 0;
    int http_status = 0;
    bool ad_session_id_present = false;
};

enum class ResponseValidity {
    Valid,
    SessionExpired,
    PermissionDenied,
    TransportError
};

std::string AuthStatusToString(AuthStatus status);
std::string AsciiLower(std::string_view sv);
std::string TrimAscii(std::string_view sv);

}  // namespace academic::tec_digital
