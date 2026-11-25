#pragma once

#include "academic/tec_digital/types.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace academic::tec_digital {

struct ParsedUrl {
    bool valid = false;
    std::string scheme;     // lowercased "http" or "https"
    std::string host;       // lowercased hostname without brackets
    uint16_t port = 0;      // explicit or default (80/443)
    bool has_explicit_port = false;
    std::string host_port;  // host or host:nondefault_port
    std::string origin;     // scheme://host[:nondefault_port]
    std::string path;       // starts with '/'
    std::string query;      // without leading '?'
    std::string target;     // path[?query]
};

ParsedUrl ParseHttpUrl(std::string_view url);
bool IsLoopbackHost(std::string_view host);
bool IsSameOrigin(std::string_view url_a, std::string_view url_b);
std::string ResolveRedirectLocation(std::string_view base_url, std::string_view location);
std::string FormatRangeHeader(uint64_t start_byte, uint64_t end_byte);

class IHttpTransport {
public:
    virtual ~IHttpTransport() = default;
    virtual HttpResponse Execute(const HttpRequest& req) = 0;
};

std::unique_ptr<IHttpTransport> CreateNativeHttpTransport();

}  // namespace academic::tec_digital
