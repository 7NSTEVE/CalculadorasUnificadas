#pragma once

#include "academic/tec_digital/types.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

struct CanonOptions {
    bool strip_tracking_and_cache_busters = true;
    bool collapse_bug059_calendar = true;
    bool strip_trailing_slash = false;
    std::vector<std::string> extra_irrelevant_params;
};

class UrlCanonicalizer {
public:
    static Result<std::string> Canonicalize(std::string_view raw_url,
                                            std::string_view base_url = "",
                                            const CanonOptions& opts = {});

    // First 12 lowercase hex chars of SHA-1(canonical_url) (matches tecaudit/canon.py)
    static std::string PageKeySha1(std::string_view canonical_url);

    // Lowercase hex chars of SHA-256(canonical_url) (12 chars default, 64 if prefix_len == 0 or >= 64)
    static std::string PageKeySha256(std::string_view canonical_url,
                                     size_t prefix_len = 12);

    // "url:" + SHA-256(canonical_url)[:32]
    static std::string UrlObjectId(std::string_view canonical_url);
};

}  // namespace academic::tec_digital
