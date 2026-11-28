#pragma once

#include <filesystem>
#include <string>

namespace DocStudio {

inline std::string PathToUtf8(const std::filesystem::path& p) {
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

inline std::string PathToGenericUtf8(const std::filesystem::path& p) {
    const auto u8 = p.generic_u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

} // namespace DocStudio
