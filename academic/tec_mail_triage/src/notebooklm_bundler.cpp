#include "academic/tec_mail_triage/triage_engine.hpp"
#include "academic/tec_mail_triage/triage_c_api.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <map>
#include <ranges>
#include <set>

namespace fs = std::filesystem;

namespace academic::tec_mail_triage {

namespace {

std::string LowerAscii(std::string_view sv) {
    std::string out(sv);
    for (char& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

std::string SanitizeFolderName(std::string s) {
    for (char& c : s) {
        if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
            c == '\\' || c == '|' || c == '?' || c == '*' ||
            static_cast<unsigned char>(c) < 32u) {
            c = ' ';
        }
    }
    std::string out;
    bool prev_sp = true;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!prev_sp) out.push_back(' ');
            prev_sp = true;
        } else {
            out.push_back(c);
            prev_sp = false;
        }
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '.')) out.pop_back();
    if (out.empty()) return "sin-nombre";
    if (out.size() > 70u) {
        out.resize(70u);
        while (!out.empty() && out.back() == ' ') out.pop_back();
    }
    return out;
}

std::string PathToUtf8(const fs::path& p) {
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

fs::path Utf8ToPath(const std::string& s) {
    return fs::path(std::u8string(reinterpret_cast<const char8_t*>(s.data()), s.size()));
}

std::string MakeUniqueFilename(std::set<std::string>& used_ci, const std::string& filename) {
    if (used_ci.insert(LowerAscii(filename)).second) return filename;
    const fs::path p = Utf8ToPath(filename);
    const std::string stem = PathToUtf8(p.stem());
    const std::string ext = PathToUtf8(p.extension());
    for (int i = 2; ; ++i) {
        const std::string cand = stem + " (" + std::to_string(i) + ")" + ext;
        if (used_ci.insert(LowerAscii(cand)).second) return cand;
    }
}

bool TryCreateHardLink(const fs::path& src, const fs::path& dst) {
#ifdef _WIN32
    return CreateHardLinkW(dst.c_str(), src.c_str(), nullptr) != FALSE;
#else
    std::error_code ec;
    fs::create_hard_link(src, dst, ec);
    return !ec;
#endif
}

} // namespace

std::string NotebookLmBundler::RealExtension(std::string_view filename) {
    char buf[32]{};
    const size_t n = tec_notebooklm_real_ext(filename.data(), filename.size(), buf, sizeof(buf));
    return std::string(buf, n);
}

bool NotebookLmBundler::IsCompatible(std::string_view filename) {
    return tec_notebooklm_is_compatible(filename.data(), filename.size());
}

NotebookLmPlan NotebookLmBundler::Plan(const std::vector<NotebookLmFile>& files, int max_per_notebook) {
    if (max_per_notebook < 1) max_per_notebook = kMaxPerNotebook;
    NotebookLmPlan plan{};

    std::map<std::string, std::vector<NotebookLmFile>> by_comm;
    for (const auto& f : files) {
        if (!IsCompatible(f.filename)) {
            std::string ext = RealExtension(f.filename);
            if (ext.empty()) ext = "(sin extension)";
            plan.discarded_by_ext[ext] += 1;
        } else {
            by_comm[f.community].push_back(f);
        }
    }

    // Order communities by descending file count (matches ArmadorBundles.Planificar)
    std::vector<std::pair<std::string, std::vector<NotebookLmFile>>> groups(by_comm.begin(), by_comm.end());
    std::ranges::sort(groups, [](const auto& a, const auto& b) {
        if (a.second.size() != b.second.size()) return a.second.size() > b.second.size();
        return a.first < b.first;
    });

    std::vector<NotebookLmFile> loose;
    const size_t max_u = static_cast<size_t>(max_per_notebook);
    const size_t solo_threshold = static_cast<size_t>(max_per_notebook / 6);

    auto make_bundle = [&](std::string name, std::vector<NotebookLmFile> slice) {
        NotebookLmBundle b{};
        b.name = std::move(name);
        for (const auto& item : slice) b.total_bytes += item.bytes;
        plan.total_included += static_cast<int>(slice.size());
        b.files = std::move(slice);
        plan.bundles.push_back(std::move(b));
    };

    for (auto& [comm, list] : groups) {
        std::ranges::sort(list, [](const NotebookLmFile& a, const NotebookLmFile& b) {
            const std::string fa = LowerAscii(a.folder), fb = LowerAscii(b.folder);
            if (fa != fb) return fa < fb;
            return LowerAscii(a.filename) < LowerAscii(b.filename);
        });

        if (list.size() > max_u) {
            const size_t parts = (list.size() + max_u - 1u) / max_u;
            for (size_t i = 0; i < parts; ++i) {
                const size_t beg = i * max_u;
                const size_t end = std::min(list.size(), beg + max_u);
                std::vector<NotebookLmFile> chunk(list.begin() + static_cast<std::ptrdiff_t>(beg),
                                                  list.begin() + static_cast<std::ptrdiff_t>(end));
                make_bundle(SanitizeFolderName(comm) + " (" + std::to_string(i + 1) + " de " + std::to_string(parts) + ")",
                            std::move(chunk));
            }
        } else if (list.size() >= solo_threshold) {
            make_bundle(SanitizeFolderName(comm), std::move(list));
        } else {
            loose.insert(loose.end(), list.begin(), list.end());
        }
    }

    if (!loose.empty()) {
        const size_t total = (loose.size() + max_u - 1u) / max_u;
        for (size_t i = 0; i < total; ++i) {
            const size_t beg = i * max_u;
            const size_t end = std::min(loose.size(), beg + max_u);
            std::vector<NotebookLmFile> chunk(loose.begin() + static_cast<std::ptrdiff_t>(beg),
                                              loose.begin() + static_cast<std::ptrdiff_t>(end));
            const std::string name = (total == 1)
                                         ? "Varios"
                                         : ("Varios (" + std::to_string(i + 1) + " de " + std::to_string(total) + ")");
            make_bundle(name, std::move(chunk));
        }
    }

    return plan;
}

NotebookLmBuildSummary NotebookLmBundler::BuildHardlinkFolders(const NotebookLmPlan& plan,
                                                               const std::string& dest_dir_utf8)
{
    NotebookLmBuildSummary sum{};
    std::error_code ec;
    const fs::path root = Utf8ToPath(dest_dir_utf8);
    fs::create_directories(root, ec);

    for (const auto& b : plan.bundles) {
        const fs::path folder = root / Utf8ToPath(SanitizeFolderName(b.name));
        fs::create_directories(folder, ec);

        std::set<std::string> used;
        for (const auto& f : b.files) {
            const fs::path src = Utf8ToPath(f.source_path);
            if (!fs::is_regular_file(src, ec)) {
                ++sum.missing_sources;
                continue;
            }
            const std::string uniq = MakeUniqueFilename(used, f.filename);
            const fs::path dst = folder / Utf8ToPath(uniq);
            if (fs::exists(dst, ec)) {
                ++sum.already_present;
                continue;
            }
            if (TryCreateHardLink(src, dst)) {
                ++sum.hardlinked;
            } else {
                fs::copy_file(src, dst, fs::copy_options::none, ec);
                if (!ec) ++sum.copied;
                else ++sum.failures;
            }
        }
        ++sum.folders_created;
    }
    return sum;
}

} // namespace academic::tec_mail_triage
