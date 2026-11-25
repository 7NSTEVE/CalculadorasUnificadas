#pragma once

#include "academic/tec_digital/cookie_jar.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/types.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <string>
#include <string_view>

namespace academic::tec_digital {

struct AuthClientOptions {
    std::string base_url = "https://tecdigital.tec.ac.cr";
    std::string default_return_url = "/dotlrn/";
    std::string user_agent =
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
        "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36";
    bool perform_prelogin_handshake = true;
    int session_probe_ttl_seconds = 120;
    bool allow_insecure_tls = false;
};

class AuthClient {
public:
    explicit AuthClient(IHttpTransport& transport,
                        ReadOnlyRequestPolicy policy = ReadOnlyRequestPolicy{},
                        AuthClientOptions opts = {});

    AuthResult LoginWithPassword(std::string_view email,
                                 std::string_view password,
                                 std::string_view return_url = "");

    AuthResult AttemptQrScanning(std::string_view email,
                                 std::string_view return_url = "");

    bool CheckSession(bool force_probe = false);

    ResponseValidity ValidateResponse(const HttpResponse& resp,
                                      std::string_view request_url = "",
                                      bool probe_on_401_403 = true);

    bool IsSessionExpired() const { return session_expired_.load(std::memory_order_acquire); }
    void ResetSessionExpired();

    CookieJar& Cookies() { return cookies_; }
    const CookieJar& Cookies() const { return cookies_; }

    const AuthClientOptions& Options() const { return opts_; }

    static AuthStatus ParseAuthStatus(std::string_view status_str);
    static int ErrorCodeForStatus(AuthStatus status);
    static bool ContainsLoginMarkers(std::string_view body);
    static bool IsLoginRedirectLocation(std::string_view location);

private:
    HttpResponse ExecutePolicyChecked(HttpRequest req, bool allow_login);
    AuthResult ParseLoginJsonResponse(const HttpResponse& resp);

    IHttpTransport& transport_;
    ReadOnlyRequestPolicy policy_;
    AuthClientOptions opts_;
    CookieJar cookies_;
    std::atomic<bool> session_expired_{false};
    mutable std::mutex state_mutex_;
    bool has_probe_cache_ = false;
    bool cached_probe_ok_ = false;
    std::chrono::steady_clock::time_point last_probe_time_{};
};

}  // namespace academic::tec_digital
