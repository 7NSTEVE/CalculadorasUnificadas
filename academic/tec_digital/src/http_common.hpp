#pragma once

#include "academic/tec_digital/http_transport.hpp"

#include <functional>

namespace academic::tec_digital::detail {

// Executes an HTTP request with manual same-origin redirect handling when req.follow_redirects is true.
// single_exec MUST execute a single hop without automatically following redirects.
HttpResponse ExecuteWithSameOriginRedirects(
    const HttpRequest& req,
    const std::function<HttpResponse(const HttpRequest&)>& single_exec);

// Pre-initializes std::locale::classic() ctype<char> tables once via std::call_once
// so concurrent static std::regex construction/use does not trigger lazy locale writes under TSan.
void WarmupRegexLocale();

}  // namespace academic::tec_digital::detail
