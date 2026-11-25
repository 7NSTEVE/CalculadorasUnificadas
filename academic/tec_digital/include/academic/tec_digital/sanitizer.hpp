#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace academic::tec_digital {

struct SanitizeOptions {
    bool include_sha256_prefix = true;
    bool shareable_mode = true;
    bool allow_personal_email = false;
    std::string runtime_email;
    std::string runtime_password;
    std::vector<std::string> pii_needles;
    size_t max_sample_chars = 8000;
};

struct NeedleScanResult {
    bool passed = true;
    bool has_password_leak = false;
    bool has_email_leak = false;
    bool has_pii_leak = false;
    bool personal_data_detected = false;
    size_t findings_count = 0;
    std::vector<std::string> finding_categories;
};

class Sanitizer {
public:
    explicit Sanitizer(SanitizeOptions opts = {});

    std::string SanitizeHeaderValue(std::string_view header_name,
                                    std::string_view header_value) const;
    std::string SanitizeCookieHeader(std::string_view cookie_header_val) const;
    std::string SanitizeSetCookieHeader(std::string_view set_cookie_val) const;
    std::vector<std::pair<std::string, std::string>> SanitizeHeaders(
        const std::vector<std::pair<std::string, std::string>>& headers) const;

    std::string SanitizeUrl(std::string_view url) const;
    std::string SanitizeText(std::string_view text) const;
    std::string SanitizeSample(std::string_view text) const;
    std::string SanitizeJsonString(std::string_view json_text) const;
    std::string SanitizeFormUrlEncoded(std::string_view form_body) const;
    std::string SanitizeHtml(std::string_view html) const;
    std::string SanitizeFilename(std::string_view path_or_name) const;
    std::string SafeDiagnosticPath(std::string_view path) const;

    std::vector<std::string> ScanReusableTokens(std::string_view text) const;
    std::vector<std::string> ScanReusableTokensFile(const std::string& file_path,
                                                    std::string* out_error = nullptr) const;

    NeedleScanResult ScanForLeaks(std::string_view data) const;
    NeedleScanResult ScanStreamForLeaks(
        const std::function<size_t(uint8_t* buf, size_t cap)>& read_chunk) const;
    NeedleScanResult ScanDirectoryForLeaks(const std::string& dir_path) const;

private:
    std::string RedactTokenValue(std::string_view val) const;
    std::string ScrubRuntimeNeedles(std::string_view input) const;

    SanitizeOptions opts_;
};

}  // namespace academic::tec_digital
