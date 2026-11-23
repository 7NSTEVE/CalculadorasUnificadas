#pragma once

#include <algorithm>
#include <cstddef>
#include <cwctype>
#include <filesystem>
#include <string>
#include <vector>

namespace cg50_firmware_pairing {
namespace fs = std::filesystem;

inline std::wstring Lower(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

inline void AppendUnique(std::vector<fs::path>& paths, const fs::path& candidate)
{
    if (candidate.empty()) return;
    const auto normalized = candidate.lexically_normal();
    for (const auto& existing : paths) {
        if (Lower(existing.lexically_normal().wstring()) == Lower(normalized.wstring()))
            return;
    }
    paths.push_back(normalized);
}

inline std::vector<fs::path> UserRomCandidates(const fs::path& systemPath)
{
    std::vector<fs::path> candidates;
    const auto parent = systemPath.parent_path();
    const std::wstring stem = systemPath.stem().wstring();
    const std::wstring lowerStem = Lower(stem);
    const std::wstring extension = systemPath.extension().wstring();

    const auto derived = [&](const wchar_t* systemSuffix,
                             const wchar_t* userSuffix) {
        const std::wstring suffix(systemSuffix);
        if (lowerStem.size() < suffix.size() ||
            lowerStem.compare(lowerStem.size() - suffix.size(), suffix.size(), suffix) != 0)
            return;
        const std::wstring base = stem.substr(0, stem.size() - suffix.size());
        AppendUnique(candidates, parent / (base + userSuffix + extension));
    };

    derived(L"_system", L"_user");
    derived(L"-system", L"-user");
    derived(L".system", L".user");
    derived(L"_systemrom", L"_userrom");
    derived(L"-systemrom", L"-userrom");
    derived(L"_sysrom", L"_userrom");
    derived(L"-sysrom", L"-userrom");

    static constexpr const wchar_t* kStandardNames[] = {
        L"fxcg50_user.rom",
        L"fxcg50_user.bin",
        L"fx-cg50_user.rom",
        L"fx-cg50_user.bin",
        L"cg50_user.rom",
        L"cg50_user.bin",
    };
    for (const auto* name : kStandardNames)
        AppendUnique(candidates, parent / name);

    return candidates;
}

inline fs::path FindSizedUserRom(const fs::path& systemPath,
                                 const std::uintmax_t expectedBytes)
{
    std::error_code ec;
    for (const auto& candidate : UserRomCandidates(systemPath)) {
        if (!fs::is_regular_file(candidate, ec) || ec) {
            ec.clear();
            continue;
        }
        const auto size = fs::file_size(candidate, ec);
        if (!ec && size == expectedBytes) return candidate;
        ec.clear();
    }
    return {};
}

} // namespace cg50_firmware_pairing
