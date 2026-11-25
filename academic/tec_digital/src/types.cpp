#include "academic/tec_digital/types.hpp"
#include "academic/tec_digital/crypto.hpp"

#include <algorithm>
#include <cctype>

namespace academic::tec_digital {

namespace {

bool EqualsCI(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::string HttpResponse::GetHeaderCI(std::string_view name) const {
    for (const auto& kv : headers) {
        if (EqualsCI(kv.first, name)) {
            return kv.second;
        }
    }
    return {};
}

std::vector<std::string> HttpResponse::GetHeadersCI(std::string_view name) const {
    std::vector<std::string> out;
    for (const auto& kv : headers) {
        if (EqualsCI(kv.first, name)) {
            out.push_back(kv.second);
        }
    }
    return out;
}

std::string HttpResponse::BodyAsString() const {
    if (body.empty()) return {};
    return std::string(reinterpret_cast<const char*>(body.data()), body.size());
}

Credentials::Credentials(Credentials&& other) noexcept
    : email(std::move(other.email)), password(std::move(other.password)) {
    other.Clear();
}

Credentials& Credentials::operator=(Credentials&& other) noexcept {
    if (this != &other) {
        Clear();
        email = std::move(other.email);
        password = std::move(other.password);
        other.Clear();
    }
    return *this;
}

Credentials::~Credentials() {
    Clear();
}

void Credentials::Clear() {
    SecureZeroString(email);
    SecureZeroString(password);
}

std::string AuthStatusToString(AuthStatus status) {
    switch (status) {
        case AuthStatus::Ok: return "ok";
        case AuthStatus::NoAccount: return "no_account";
        case AuthStatus::BadPassword: return "bad_password";
        case AuthStatus::EmptyCredentials: return "empty_email_or_password";
        case AuthStatus::InvalidRetoken: return "invalid_retoken";
        case AuthStatus::LoginExpired: return "login_expired";
        case AuthStatus::InvalidCaptcha: return "invalid_captcha";
        case AuthStatus::BackendWsError: return "error_ws";
        case AuthStatus::TooManyAttempts: return "muchos_intentos";
        case AuthStatus::UnknownServerError: return "error_desconocido";
        case AuthStatus::DaticAuthError: return "datic_auth_error";
        case AuthStatus::DaticBadPassword: return "datic_bad_password";
        case AuthStatus::DaticWsError: return "datic_ws_communication_error";
        case AuthStatus::ActionRedirect: return "account_url";
        case AuthStatus::AccountMessage: return "account_message";
        case AuthStatus::AccountClosed: return "register-account-closed";
        case AuthStatus::InterstitialRedirect: return "interstitial_redirect";
        case AuthStatus::UnknownError: return "unknown_error";
        case AuthStatus::MissingSessionCookie: return "missing_session_cookie";
        case AuthStatus::PortalVerificationFailed: return "portal_verification_failed";
        case AuthStatus::TransportError: return "transport_error";
        case AuthStatus::PolicyBlocked: return "policy_blocked";
        case AuthStatus::InvalidJsonResponse: return "invalid_json_response";
        case AuthStatus::MissingQrEmailClaim: return "missing_qr_email_claim";
    }
    return "unknown_error";
}

std::string AsciiLower(std::string_view sv) {
    std::string out(sv);
    for (char& ch : out) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return out;
}

std::string TrimAscii(std::string_view sv) {
    size_t start = 0;
    while (start < sv.size() &&
           (sv[start] == ' ' || sv[start] == '\t' || sv[start] == '\r' || sv[start] == '\n')) {
        ++start;
    }
    size_t end = sv.size();
    while (end > start &&
           (sv[end - 1] == ' ' || sv[end - 1] == '\t' || sv[end - 1] == '\r' || sv[end - 1] == '\n')) {
        --end;
    }
    return std::string(sv.substr(start, end - start));
}

}  // namespace academic::tec_digital
