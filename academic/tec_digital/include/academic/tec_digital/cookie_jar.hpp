#pragma once

#include "academic/tec_digital/types.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

class CookieJar {
public:
    void ApplySetCookieHeaders(const std::vector<std::string>& set_cookie_headers,
                               std::string_view request_host,
                               std::string_view request_path,
                               int64_t now_epoch = -1);

    void ApplySetCookieHeader(std::string_view header_line,
                              std::string_view request_host,
                              std::string_view request_path,
                              int64_t now_epoch = -1);

    std::string BuildCookieHeader(std::string_view url,
                                  int64_t now_epoch = -1) const;

    std::optional<Cookie> GetCookie(std::string_view name,
                                    std::string_view host = "",
                                    std::string_view path = "/",
                                    bool is_https = true,
                                    int64_t now_epoch = -1) const;

    bool HasCookie(std::string_view name,
                   std::string_view host = "",
                   std::string_view path = "/",
                   bool is_https = true,
                   int64_t now_epoch = -1) const;

    std::vector<Cookie> AllCookies() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return cookies_;
    }

    void SetCookies(std::vector<Cookie> cookies);
    void Clear();

    std::vector<CookieMeta> ExportSanitizedMetadata(int64_t now_epoch = -1) const;
    std::string ExportSanitizedMetadataJson(std::string_view saved_at_utc = "",
                                            int64_t now_epoch = -1) const;

    std::string SerializeToJson() const;
    bool DeserializeFromJson(std::string_view json_str);

    static bool ParseHttpDate(std::string_view date_str, int64_t& out_epoch);
    static int64_t CurrentUnixEpoch();

private:
    void ApplySetCookieHeaderLocked(std::string_view header_line,
                                    std::string_view request_host,
                                    std::string_view request_path,
                                    int64_t now_epoch);

    mutable std::mutex mutex_;
    std::vector<Cookie> cookies_;
};

}  // namespace academic::tec_digital
