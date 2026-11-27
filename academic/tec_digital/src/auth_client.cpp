#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/crypto.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace academic::tec_digital {

namespace {

std::string JoinUrl(std::string_view base_url, std::string_view path) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    std::string p(path);
    if (p.empty() || p.front() != '/') p.insert(p.begin(), '/');
    return b + p;
}

bool StartsWithCI(std::string_view s, std::string_view prefix) {
    if (s.size() < prefix.size()) return false;
    return AsciiLower(s.substr(0, prefix.size())) == AsciiLower(prefix);
}

bool ContainsMarkerCI(std::string_view hay_low, std::string_view needle_low) {
    return hay_low.find(needle_low) != std::string_view::npos;
}

}  // namespace

AuthClient::AuthClient(IHttpTransport& transport,
                       ReadOnlyRequestPolicy policy,
                       AuthClientOptions opts)
    : transport_(transport),
      policy_(std::move(policy)),
      opts_(std::move(opts)) {}

void AuthClient::ResetSessionExpired() {
    session_expired_.store(false, std::memory_order_release);
    std::lock_guard<std::mutex> lk(state_mutex_);
    has_probe_cache_ = false;
    cached_probe_ok_ = false;
}

AuthStatus AuthClient::ParseAuthStatus(std::string_view status_str) {
    std::string s = TrimAscii(status_str);
    if (s == "ok") return AuthStatus::Ok;
    if (s == "no_account") return AuthStatus::NoAccount;
    if (s == "bad_password") return AuthStatus::BadPassword;
    if (s == "empty_email_or_password") return AuthStatus::EmptyCredentials;
    if (s == "invalid_retoken") return AuthStatus::InvalidRetoken;
    if (s == "login_expired") return AuthStatus::LoginExpired;
    if (s == "invalid_captcha") return AuthStatus::InvalidCaptcha;
    if (s == "error_ws") return AuthStatus::BackendWsError;
    if (s == "muchos_intentos") return AuthStatus::TooManyAttempts;
    if (s == "error_desconocido") return AuthStatus::UnknownServerError;
    if (s == "datic_auth_error") return AuthStatus::DaticAuthError;
    if (s == "datic_bad_password") return AuthStatus::DaticBadPassword;
    if (s == "datic_ws_communication_error") return AuthStatus::DaticWsError;
    if (s == "account_url") return AuthStatus::ActionRedirect;
    if (s == "account_message") return AuthStatus::AccountMessage;
    if (s == "register-account-closed") return AuthStatus::AccountClosed;
    if (s == "survey-vida" || s == "survey-vida-profesor" ||
        s == "insurance-policy" || s == "verificacion-correo-evaluaciondocente") {
        return AuthStatus::InterstitialRedirect;
    }
    return AuthStatus::UnknownError;
}

int AuthClient::ErrorCodeForStatus(AuthStatus status) {
    switch (status) {
        case AuthStatus::Ok: return 0;
        case AuthStatus::EmptyCredentials: return 1;
        case AuthStatus::InvalidRetoken:
        case AuthStatus::LoginExpired:
        case AuthStatus::InvalidCaptcha:
        case AuthStatus::BackendWsError:
        case AuthStatus::TooManyAttempts:
        case AuthStatus::UnknownServerError: return 2;
        case AuthStatus::NoAccount:
        case AuthStatus::BadPassword: return 5;
        case AuthStatus::MissingQrEmailClaim: return 6;
        default: return 3;
    }
}

bool AuthClient::IsLoginRedirectLocation(std::string_view location) {
    std::string trimmed = TrimAscii(location);
    if (trimmed.empty()) return false;
    ParsedUrl parsed = ParseHttpUrl(trimmed);
    std::string path_low = parsed.valid ? AsciiLower(parsed.path) : AsciiLower(trimmed);
    if (path_low.find("/register/logout") != std::string::npos) return false;
    return (path_low.rfind("/register", 0) == 0) ||
           (path_low.rfind("/login", 0) == 0) ||
           (path_low.find("/register/") != std::string::npos) ||
           (path_low.find("/login/") != std::string::npos);
}

bool AuthClient::ContainsLoginMarkers(std::string_view body) {
    if (body.empty()) return false;
    std::string_view slice = body.substr(0, std::min<size_t>(body.size(), 65536));
    std::string low = AsciiLower(slice);

    if (ContainsMarkerCI(low, "id=\"login-form\"") ||
        ContainsMarkerCI(low, "id='login-form'") ||
        ContainsMarkerCI(low, "name=\"login\"") ||
        ContainsMarkerCI(low, "name='login'") ||
        ContainsMarkerCI(low, "loginform") ||
        ContainsMarkerCI(low, "name=\"userpassword\"") ||
        ContainsMarkerCI(low, "formcontrolname=\"userpassword\"") ||
        ContainsMarkerCI(low, "api/login/new-form") ||
        ContainsMarkerCI(low, "ingresar con idtecbeta")) {
        return true;
    }

    // Check /register/ excluding /register/logout
    size_t pos = 0;
    while ((pos = low.find("/register/", pos)) != std::string::npos) {
        std::string_view after = std::string_view(low).substr(pos + 10);
        if (!StartsWithCI(after, "logout")) {
            return true;
        }
        pos += 10;
    }

    // Authenticated 2026 TEC Digital pages embed an inactivity-timeout modal with
    // "<button class=\"timeout-modal-button\" onclick=\"logout()\">Iniciar sesión nuevamente</button>"
    // alongside #header-td and #platform_user_panel. Only treat plain "iniciar sesión"
    // text as a login wall when authenticated portal chrome is absent.
    const bool has_authenticated_portal_chrome =
        ContainsMarkerCI(low, "id=\"platform_user_panel\"") ||
        ContainsMarkerCI(low, "id='platform_user_panel'") ||
        ContainsMarkerCI(low, "id=\"header-td\"") ||
        ContainsMarkerCI(low, "id='header-td'") ||
        ContainsMarkerCI(low, "timeout-modal-overlay") ||
        ContainsMarkerCI(low, "/register/logout");

    if (!has_authenticated_portal_chrome &&
        (ContainsMarkerCI(low, "iniciar sesi\xc3\xb3n") ||
         ContainsMarkerCI(low, "iniciar sesion"))) {
        return true;
    }
    return false;
}

HttpResponse AuthClient::ExecutePolicyChecked(HttpRequest req, bool allow_login) {
    ParsedUrl parsed = ParseHttpUrl(req.url);
    if (parsed.valid) {
        std::string cookie_hdr = cookies_.BuildCookieHeader(req.url);
        if (!cookie_hdr.empty()) {
            req.headers["Cookie"] = cookie_hdr;
        }
    }
    req.allow_insecure_tls = opts_.allow_insecure_tls;

    PolicyDecision decision = policy_.EvaluateHttpRequest(req, allow_login);
    if (!decision.allowed) {
        HttpResponse blocked;
        blocked.transport_ok = false;
        blocked.final_url = req.url;
        blocked.error_message = "read-only-request-guard:" + decision.reason;
        return blocked;
    }

    HttpResponse resp = transport_.Execute(req);
    if (resp.transport_ok && parsed.valid && !resp.set_cookies.empty()) {
        cookies_.ApplySetCookieHeaders(resp.set_cookies, parsed.host, parsed.path);
    }
    return resp;
}

AuthResult AuthClient::ParseLoginJsonResponse(const HttpResponse& resp) {
    AuthResult res;
    res.http_status = resp.status_code;
    if (!resp.transport_ok) {
        if (resp.error_message.rfind("read-only-request-guard:", 0) == 0) {
            res.status = AuthStatus::PolicyBlocked;
        } else {
            res.status = AuthStatus::TransportError;
        }
        res.message = resp.error_message;
        res.error_code = ErrorCodeForStatus(res.status);
        return res;
    }

    if (resp.status_code < 200 || resp.status_code >= 300) {
        res.status = AuthStatus::UnknownServerError;
        res.message = "HTTP status " + std::to_string(resp.status_code);
        res.error_code = ErrorCodeForStatus(res.status);
        return res;
    }

    try {
        auto j = nlohmann::json::parse(resp.BodyAsString());
        if (!j.is_object()) {
            res.status = AuthStatus::InvalidJsonResponse;
            res.message = "login response JSON is not an object";
            res.error_code = ErrorCodeForStatus(res.status);
            return res;
        }
        res.status_string = j.value("status", "");
        res.return_url = j.value("returnUrl", "");
        res.message = j.value("message", "");
        res.status = ParseAuthStatus(res.status_string);
        res.error_code = ErrorCodeForStatus(res.status);
        res.ok = (res.status == AuthStatus::Ok);
        return res;
    } catch (const std::exception& ex) {
        res.status = AuthStatus::InvalidJsonResponse;
        res.message = ex.what();
        res.error_code = ErrorCodeForStatus(res.status);
        return res;
    }
}

AuthResult AuthClient::LoginWithPassword(std::string_view email,
                                         std::string_view password,
                                         std::string_view return_url) {
    std::string norm_email = AsciiLower(TrimAscii(email));
    std::string pass_str(password);
    if (norm_email.empty() || pass_str.empty()) {
        SecureZeroString(pass_str);
        AuthResult r;
        r.status = AuthStatus::EmptyCredentials;
        r.status_string = "empty_email_or_password";
        r.error_code = ErrorCodeForStatus(r.status);
        r.message = "email and password are required";
        return r;
    }

    ResetSessionExpired();

    ParsedUrl base = ParseHttpUrl(opts_.base_url);
    if (!base.valid) {
        SecureZeroString(pass_str);
        AuthResult r;
        r.status = AuthStatus::TransportError;
        r.message = "invalid base_url";
        return r;
    }

    // 1. Pre-login handshake (GET /) to acquire AlteonP sticky cookie on Path=/
    if (opts_.perform_prelogin_handshake) {
        HttpRequest pre;
        pre.method = "GET";
        pre.url = JoinUrl(opts_.base_url, "/");
        pre.follow_redirects = false;
        pre.headers["User-Agent"] = opts_.user_agent;
        pre.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";
        HttpResponse pre_resp = ExecutePolicyChecked(std::move(pre), false);
        if (!pre_resp.transport_ok) {
            SecureZeroString(pass_str);
            AuthResult r;
            r.status = (pre_resp.error_message.rfind("read-only-request-guard:", 0) == 0)
                           ? AuthStatus::PolicyBlocked
                           : AuthStatus::TransportError;
            r.message = pre_resp.error_message;
            return r;
        }
    }

    // 2. POST /api/login/new-form/
    std::string effective_return =
        return_url.empty() ? opts_.default_return_url : std::string(return_url);
    nlohmann::json payload = {
        {"email", norm_email},
        {"password", pass_str},
        {"returnUrl", effective_return}
    };
    std::string body_str = payload.dump();
    SecureZeroString(pass_str);
    payload.clear();

    HttpRequest req;
    req.method = "POST";
    req.url = JoinUrl(opts_.base_url, "/api/login/new-form/");
    req.follow_redirects = false;
    req.body = std::move(body_str);
    req.headers["Content-Type"] = "application/json";
    req.headers["Accept"] = "application/json, text/plain, */*";
    req.headers["X-Content-Type-Options"] = "nosniff";
    req.headers["Origin"] = base.origin;
    req.headers["Referer"] = JoinUrl(opts_.base_url, "/");
    req.headers["User-Agent"] = opts_.user_agent;

    HttpResponse resp = ExecutePolicyChecked(std::move(req), true);
    AuthResult result = ParseLoginJsonResponse(resp);
    if (!result.ok) {
        return result;
    }

    // 3. Verify ad_session_id cookie is present and non-empty
    const bool is_https = (base.scheme == "https");
    result.ad_session_id_present = cookies_.HasCookie("ad_session_id", base.host, "/", is_https);
    if (!result.ad_session_id_present) {
        result.ok = false;
        result.status = AuthStatus::MissingSessionCookie;
        result.status_string = AuthStatusToString(result.status);
        result.message = "ad_session_id cookie missing after login";
        return result;
    }

    // 4. Verify active portal session (GET /dotlrn/ returns 200 without /register redirect/markers)
    if (!CheckSession(true)) {
        result.ok = false;
        result.status = AuthStatus::PortalVerificationFailed;
        result.status_string = AuthStatusToString(result.status);
        result.message = "GET /dotlrn/ verification failed";
        return result;
    }

    return result;
}

AuthResult AuthClient::AttemptQrScanning(std::string_view email,
                                         std::string_view return_url) {
    std::string norm_email = AsciiLower(TrimAscii(email));
    if (norm_email.empty()) {
        AuthResult r;
        r.status = AuthStatus::MissingQrEmailClaim;
        r.status_string = AuthStatusToString(r.status);
        r.error_code = ErrorCodeForStatus(r.status);
        r.message = "QR email claim is empty";
        return r;
    }

    ParsedUrl base = ParseHttpUrl(opts_.base_url);
    if (!base.valid) {
        AuthResult r;
        r.status = AuthStatus::TransportError;
        r.message = "invalid base_url";
        return r;
    }

    std::string effective_return =
        return_url.empty() ? "/dotlrn/index" : std::string(return_url);
    nlohmann::json payload = {
        {"email", norm_email},
        {"returnUrl", effective_return}
    };

    HttpRequest req;
    req.method = "POST";
    req.url = JoinUrl(opts_.base_url, "/api/login/scanning/");
    req.follow_redirects = false;
    req.body = payload.dump();
    req.headers["Content-Type"] = "application/json";
    req.headers["Accept"] = "application/json, text/plain, */*";
    req.headers["Origin"] = base.origin;
    req.headers["Referer"] = JoinUrl(opts_.base_url, "/");
    req.headers["User-Agent"] = opts_.user_agent;

    HttpResponse resp = ExecutePolicyChecked(std::move(req), true);
    AuthResult result = ParseLoginJsonResponse(resp);
    if (!result.ok) {
        return result;
    }

    const bool is_https = (base.scheme == "https");
    result.ad_session_id_present = cookies_.HasCookie("ad_session_id", base.host, "/", is_https);
    if (!result.ad_session_id_present) {
        result.ok = false;
        result.status = AuthStatus::MissingSessionCookie;
        result.status_string = AuthStatusToString(result.status);
        result.message = "ad_session_id cookie missing after QR scanning";
        return result;
    }

    if (!CheckSession(true)) {
        result.ok = false;
        result.status = AuthStatus::PortalVerificationFailed;
        result.status_string = AuthStatusToString(result.status);
        result.message = "GET /dotlrn/ verification failed";
        return result;
    }
    return result;
}

bool AuthClient::CheckSession(bool force_probe) {
    if (!force_probe && session_expired_.load(std::memory_order_acquire)) {
        return false;
    }
    auto now = std::chrono::steady_clock::now();
    if (!force_probe) {
        std::lock_guard<std::mutex> lk(state_mutex_);
        if (has_probe_cache_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_probe_time_).count();
            if (elapsed >= 0 && elapsed < opts_.session_probe_ttl_seconds) {
                return cached_probe_ok_;
            }
        }
    }

    HttpRequest req;
    req.method = "GET";
    req.url = JoinUrl(opts_.base_url, "/dotlrn/");
    req.follow_redirects = false;
    req.headers["User-Agent"] = opts_.user_agent;
    req.headers["Accept"] = "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8";

    HttpResponse resp = ExecutePolicyChecked(std::move(req), false);
    bool valid = false;
    if (resp.transport_ok && resp.status_code == 200) {
        if (!IsLoginRedirectLocation(resp.final_url) &&
            !ContainsLoginMarkers(resp.BodyAsString())) {
            valid = true;
        }
    }

    {
        std::lock_guard<std::mutex> lk(state_mutex_);
        has_probe_cache_ = true;
        last_probe_time_ = now;
        cached_probe_ok_ = valid;
    }
    if (!valid) {
        session_expired_.store(true, std::memory_order_release);
    }
    return valid;
}

ResponseValidity AuthClient::ValidateResponse(const HttpResponse& resp,
                                              std::string_view request_url,
                                              bool probe_on_401_403) {
    if (!resp.transport_ok) {
        return ResponseValidity::TransportError;
    }

    auto mark_expired = [&]() {
        session_expired_.store(true, std::memory_order_release);
        std::lock_guard<std::mutex> lk(state_mutex_);
        has_probe_cache_ = false;
        cached_probe_ok_ = false;
    };

    const int code = resp.status_code;
    const bool is_redirect = (code == 301 || code == 302 || code == 303 || code == 307 || code == 308);
    if (is_redirect && IsLoginRedirectLocation(resp.location)) {
        if (!IsLoginRedirectLocation(request_url)) {
            mark_expired();
            return ResponseValidity::SessionExpired;
        }
    }

    if (IsLoginRedirectLocation(resp.final_url) && !IsLoginRedirectLocation(request_url)) {
        mark_expired();
        return ResponseValidity::SessionExpired;
    }

    std::string ct_low = AsciiLower(resp.content_type);
    std::string body_str = resp.BodyAsString();
    size_t ws = 0;
    while (ws < body_str.size() &&
           (body_str[ws] == ' ' || body_str[ws] == '\t' ||
            body_str[ws] == '\r' || body_str[ws] == '\n')) {
        ++ws;
    }
    std::string_view trimmed_body(body_str.data() + ws, body_str.size() - ws);
    std::string prefix_low = AsciiLower(trimmed_body.substr(0, std::min<size_t>(trimmed_body.size(), 512)));
    const bool looks_html =
        (ct_low.find("html") != std::string::npos) ||
        (ct_low.find("text/") != std::string::npos) ||
        (prefix_low.find("<!doctype") != std::string::npos) ||
        (prefix_low.find("<html") != std::string::npos);

    if (looks_html && ContainsLoginMarkers(body_str)) {
        mark_expired();
        return ResponseValidity::SessionExpired;
    }

    if (code == 401 || code == 403) {
        if (probe_on_401_403) {
            if (CheckSession(false)) {
                return ResponseValidity::PermissionDenied;
            }
            mark_expired();
            return ResponseValidity::SessionExpired;
        }
        return ResponseValidity::PermissionDenied;
    }

    return ResponseValidity::Valid;
}

}  // namespace academic::tec_digital
