#pragma once

#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/types.hpp"

#include <string>

namespace academic::tec_digital::detail {

inline bool HasHeaderCI(const std::map<std::string, std::string>& headers,
                        std::string_view target_lower) {
    for (const auto& kv : headers) {
        if (AsciiLower(kv.first) == target_lower) {
            return true;
        }
    }
    return false;
}

inline HttpResponse ExecuteSessionRequest(PolicyEnforcingTransport& transport,
                                          AuthClient& auth,
                                          HttpRequest req) {
    ParsedUrl parsed = ParseHttpUrl(req.url);
    if (parsed.valid && !HasHeaderCI(req.headers, "cookie")) {
        std::string cookie_hdr = auth.Cookies().BuildCookieHeader(req.url);
        if (!cookie_hdr.empty()) {
            req.headers["Cookie"] = std::move(cookie_hdr);
        }
    }
    if (!HasHeaderCI(req.headers, "user-agent") && !auth.Options().user_agent.empty()) {
        req.headers["User-Agent"] = auth.Options().user_agent;
    }
    req.allow_insecure_tls = auth.Options().allow_insecure_tls;

    HttpResponse resp = transport.Execute(req);
    if (resp.transport_ok && !resp.set_cookies.empty()) {
        const std::string& cookie_url = resp.final_url.empty() ? req.url : resp.final_url;
        ParsedUrl fp = ParseHttpUrl(cookie_url);
        if (fp.valid) {
            auth.Cookies().ApplySetCookieHeaders(resp.set_cookies, fp.host, fp.path);
        }
    }
    return resp;
}

}  // namespace academic::tec_digital::detail
