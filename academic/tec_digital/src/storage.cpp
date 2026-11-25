#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/vault.hpp"
#include "session_http.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <queue>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;

namespace academic::tec_digital {

namespace {

std::string JoinUrl(std::string_view base_url, std::string_view path) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    std::string p(path);
    if (p.empty() || p.front() != '/') p.insert(p.begin(), '/');
    return b + p;
}

std::string JsonStringOrEmpty(const nlohmann::json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return {};
    const auto& v = j[key];
    if (v.is_string()) return v.get<std::string>();
    if (v.is_number_integer()) return std::to_string(v.get<int64_t>());
    if (v.is_number_unsigned()) return std::to_string(v.get<uint64_t>());
    if (v.is_number_float()) return std::to_string(v.get<double>());
    if (v.is_boolean()) return v.get<bool>() ? "1" : "0";
    return {};
}

int64_t JsonIntOrZero(const nlohmann::json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return 0;
    const auto& v = j[key];
    if (v.is_number_integer()) return v.get<int64_t>();
    if (v.is_number_unsigned()) return static_cast<int64_t>(v.get<uint64_t>());
    if (v.is_string()) {
        std::string s = TrimAscii(v.get<std::string>());
        if (s.empty()) return 0;
        bool all_digits = true;
        size_t i = (s[0] == '-') ? 1 : 0;
        if (i >= s.size()) all_digits = false;
        for (; i < s.size(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
                all_digits = false;
                break;
            }
        }
        if (all_digits) {
            return std::strtoll(s.c_str(), nullptr, 10);
        }
    }
    return 0;
}

uint64_t JsonU64OrZero(const nlohmann::json& j, const char* key) {
    int64_t v = JsonIntOrZero(j, key);
    return (v > 0) ? static_cast<uint64_t>(v) : 0;
}

std::string FirstNonEmptyJsonStr(const nlohmann::json& j,
                                 std::initializer_list<const char*> keys) {
    for (const char* k : keys) {
        std::string s = JsonStringOrEmpty(j, k);
        if (!s.empty()) return s;
    }
    return {};
}

std::string UrlEncodeSegment(std::string_view sv) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(sv.size());
    for (unsigned char c : sv) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0F]);
            out.push_back(kHex[c & 0x0F]);
        }
    }
    return out;
}

// Encodes each path segment individually while keeping '/' unencoded (never emits %2F)
std::string UrlEncodePathKeepingSlashes(std::string_view rel_path) {
    std::string p(rel_path);
    while (p.rfind("../", 0) == 0) p.erase(0, 3);
    while (p.rfind("./", 0) == 0) p.erase(0, 2);
    while (!p.empty() && p.front() == '/') p.erase(0, 1);

    std::string out;
    size_t pos = 0;
    bool first = true;
    while (pos <= p.size()) {
        size_t slash = p.find('/', pos);
        if (slash == std::string::npos) slash = p.size();
        std::string seg = p.substr(pos, slash - pos);
        if (!seg.empty() && seg != "." && seg != "..") {
            if (!first) out.push_back('/');
            first = false;
            out += UrlEncodeSegment(seg);
        }
        if (slash == p.size()) break;
        pos = slash + 1;
    }
    return out;
}

std::string LowerExtension(std::string_view filename) {
    std::string fn(filename);
    size_t slash = fn.find_last_of("/\\");
    std::string base = (slash == std::string::npos) ? fn : fn.substr(slash + 1);
    size_t dot = base.rfind('.');
    if (dot == std::string::npos || dot == 0) return {};
    return AsciiLower(base.substr(dot));
}

bool IsTextOrDocExt(std::string_view ext) {
    static const std::unordered_set<std::string> kExts = {
        ".txt", ".md", ".csv", ".json", ".xml", ".rtf", ".tex", ".py",
        ".ipynb", ".cpp", ".c", ".h", ".hpp", ".java", ".m", ".nb", ".sql",
        ".log", ".in", ".out", ".dat", ".js", ".css"
    };
    return kExts.count(std::string(ext)) > 0;
}

bool ReadFileHead(const fs::path& p, size_t max_bytes, std::vector<uint8_t>& out) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return false;
    out.resize(max_bytes);
    ifs.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(max_bytes));
    std::streamsize got = ifs.gcount();
    out.resize(static_cast<size_t>(std::max<std::streamsize>(0, got)));
    return true;
}

VoidResult WriteFileAtomic(const fs::path& target, const std::string& content) {
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    if (ec) {
        return VoidResult::Failure("mkdir_failed", ec.message());
    }
    fs::path tmp = target;
    tmp += ".tmp";
    {
        std::ofstream ofs(tmp, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            return VoidResult::Failure("open_failed", "failed opening temp file");
        }
        ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        ofs.flush();
        if (!ofs.good()) {
            return VoidResult::Failure("write_failed", "failed writing temp file");
        }
    }
    fs::rename(tmp, target, ec);
    if (ec) {
        std::error_code rm_ec;
        fs::remove(target, rm_ec);
        ec.clear();
        fs::rename(tmp, target, ec);
        if (ec) {
            fs::remove(tmp, rm_ec);
            return VoidResult::Failure("rename_failed", ec.message());
        }
    }
    return VoidResult::Success();
}

VoidResult MoveOrCopyAtomic(const fs::path& src, const fs::path& dst) {
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    if (ec) return VoidResult::Failure("mkdir_failed", ec.message());

    fs::rename(src, dst, ec);
    if (!ec) return VoidResult::Success();

    ec.clear();
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) return VoidResult::Failure("copy_failed", ec.message());

    std::error_code rm_ec;
    fs::remove(src, rm_ec);
    return VoidResult::Success();
}

void SleepMs(int ms) {
    if (ms > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }
}

int64_t NowSteadyMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::string FileStatusToString(FileStatus st) {
    switch (st) {
        case FileStatus::Pending: return "pending";
        case FileStatus::Downloaded: return "downloaded";
        case FileStatus::Failed: return "failed";
        case FileStatus::Orphan404: return "orphan_404";
        case FileStatus::SkippedLarge: return "skipped_large";
        case FileStatus::SessionExpired: return "session_expired";
    }
    return "failed";
}

FileStatus FileStatusFromString(std::string_view s) {
    if (s == "downloaded") return FileStatus::Downloaded;
    if (s == "orphan_404") return FileStatus::Orphan404;
    if (s == "skipped_large") return FileStatus::SkippedLarge;
    if (s == "session_expired") return FileStatus::SessionExpired;
    if (s == "pending") return FileStatus::Pending;
    return FileStatus::Failed;
}

bool ExtractAttrCI(std::string_view tag, std::string_view attr_name, std::string& out_val) {
    std::string low_tag = AsciiLower(tag);
    std::string target = AsciiLower(attr_name);
    size_t pos = 0;
    while (pos < low_tag.size()) {
        size_t found = low_tag.find(target, pos);
        if (found == std::string::npos) return false;
        if (found > 0 && (std::isalnum(static_cast<unsigned char>(low_tag[found - 1])) ||
                          low_tag[found - 1] == '-' || low_tag[found - 1] == '_')) {
            pos = found + target.size();
            continue;
        }
        size_t after = found + target.size();
        while (after < low_tag.size() && std::isspace(static_cast<unsigned char>(low_tag[after]))) {
            ++after;
        }
        if (after >= low_tag.size() || low_tag[after] != '=') {
            pos = found + target.size();
            continue;
        }
        ++after;
        while (after < low_tag.size() && std::isspace(static_cast<unsigned char>(low_tag[after]))) {
            ++after;
        }
        if (after >= low_tag.size()) return false;
        if (low_tag[after] == '"' || low_tag[after] == '\'') {
            char q = low_tag[after];
            ++after;
            size_t end_q = tag.find(q, after);
            if (end_q == std::string_view::npos) return false;
            out_val.assign(tag.substr(after, end_q - after));
            return true;
        }
        size_t end_u = after;
        while (end_u < tag.size() && !std::isspace(static_cast<unsigned char>(tag[end_u])) &&
               tag[end_u] != '>') {
            ++end_u;
        }
        out_val.assign(tag.substr(after, end_u - after));
        return true;
    }
    return false;
}

bool HasAttrCI(std::string_view tag, std::string_view attr_name) {
    std::string dummy;
    if (ExtractAttrCI(tag, attr_name, dummy)) return true;
    std::string low = AsciiLower(tag);
    std::string target = AsciiLower(attr_name);
    size_t pos = 0;
    while ((pos = low.find(target, pos)) != std::string::npos) {
        bool left_ok = (pos == 0) ||
                       std::isspace(static_cast<unsigned char>(low[pos - 1]));
        size_t right = pos + target.size();
        bool right_ok = (right >= low.size()) ||
                        std::isspace(static_cast<unsigned char>(low[right])) ||
                        low[right] == '>' || low[right] == '/';
        if (left_ok && right_ok) return true;
        pos = right;
    }
    return false;
}

std::string StripTagsAndTrim(std::string_view html) {
    std::string out;
    bool in_tag = false;
    for (char c : html) {
        if (c == '<') in_tag = true;
        else if (c == '>') in_tag = false;
        else if (!in_tag) out.push_back(c);
    }
    return TrimAscii(HeadlessCrawler::HtmlUnescape(out));
}

int PriorityForUrl(std::string_view canonical_url, int depth) {
    std::string low = AsciiLower(canonical_url);
    if (low.find("community-member?user_id=") != std::string::npos ||
        low.find("one-community?date=") != std::string::npos ||
        low.find("one-community?view=") != std::string::npos ||
        low.find("cal-item-new") != std::string::npos ||
        low.find("send-email?") != std::string::npos ||
        low.find("request-new?") != std::string::npos ||
        low.find("whos-online") != std::string::npos ||
        low.find("catalogo") != std::string::npos) {
        return 8;
    }
    if (depth == 0 && (low.find("/dotlrn/index") != std::string::npos ||
                       low.find("/dotlrn/") != std::string::npos)) {
        return 0;
    }
    if (low.find("/tda-") != std::string::npos || low.find("/tds-") != std::string::npos) {
        return std::min(depth + 1, 3);
    }
    if (low.find("/file-storage") != std::string::npos ||
        low.find("/dotlrn/calendar") != std::string::npos) {
        return 3;
    }
    if (low.find("calendar") != std::string::npos ||
        low.find("news") != std::string::npos ||
        low.find("evaluation") != std::string::npos ||
        low.find("forums") != std::string::npos ||
        low.find("homework") != std::string::npos) {
        return 4;
    }
    if (low.find("one-community") != std::string::npos ||
        low.find("members") != std::string::npos) {
        return 6;
    }
    return 5;
}

std::string DeriveCommunitySlug(std::string_view url_or_path) {
    ParsedUrl p = ParseHttpUrl(url_or_path);
    std::string path = p.valid ? p.path : std::string(url_or_path);
    static const std::regex kClassRe(
        R"(^/dotlrn/classes/[^/]+/[^/]+/([^/]+))",
        std::regex::ECMAScript | std::regex::icase);
    std::smatch m;
    if (std::regex_search(path, m, kClassRe)) {
        return m.str(1);
    }
    static const std::regex kCatRe(
        R"(^/dotlrn/catedras\.tec/([^/]+))",
        std::regex::ECMAScript | std::regex::icase);
    if (std::regex_search(path, m, kCatRe)) {
        return m.str(1);
    }
    return "portal";
}

std::string StripGlobalPortalHeader(std::string_view html) {
    std::string out(html);
    auto remove_tag_with_marker = [&](std::string_view tag_name, std::string_view attr_marker) {
        const std::string open_prefix = "<" + std::string(tag_name);
        const std::string close_tag = "</" + std::string(tag_name) + ">";
        size_t pos = 0;
        while (pos < out.size()) {
            std::string low = AsciiLower(out);
            size_t found = low.find(open_prefix, pos);
            if (found == std::string::npos) break;
            size_t gt = out.find('>', found + open_prefix.size());
            if (gt == std::string::npos) break;
            std::string open_tag_low = low.substr(found, gt - found + 1);
            if (open_tag_low.find(attr_marker) == std::string::npos) {
                pos = gt + 1;
                continue;
            }
            int depth = 1;
            size_t scan = gt + 1;
            size_t block_end = out.size();
            while (scan < low.size()) {
                size_t next_open = low.find(open_prefix, scan);
                size_t next_close = low.find(close_tag, scan);
                if (next_close == std::string::npos) {
                    block_end = out.size();
                    break;
                }
                if (next_open != std::string::npos && next_open < next_close) {
                    ++depth;
                    scan = next_open + open_prefix.size();
                } else {
                    --depth;
                    scan = next_close + close_tag.size();
                    if (depth == 0) {
                        block_end = scan;
                        break;
                    }
                }
            }
            out.erase(found, block_end - found);
            pos = found;
        }
    };
    remove_tag_with_marker("header", "header-td");
    remove_tag_with_marker("div", "platform_user_panel");
    return out;
}

}  // namespace

FileStorageWalker::FileStorageWalker(PolicyEnforcingTransport& transport,
                                     AuthClient& auth,
                                     std::string base_url,
                                     SanitizeOptions sanitize_opts)
    : transport_(transport),
      auth_(auth),
      base_url_(std::move(base_url)),
      sanitizer_(std::move(sanitize_opts)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

std::vector<std::string> FileStorageWalker::ExtractRootFolderIds(std::string_view html_text) {
    static const std::regex kPatterns[] = {
        std::regex(R"(file-storage/#/(\d{5,}))", std::regex::ECMAScript | std::regex::icase),
        std::regex(R"((?:^|[\s?&;,("'{}])folder_id\s*[=:]\s*["']?(\d{5,}))",
                   std::regex::ECMAScript | std::regex::icase),
        std::regex(R"(data-folder-id=["'](\d+)["'])", std::regex::ECMAScript | std::regex::icase),
        std::regex(R"(root_folder_id\s*[:=]\s*["']?(\d{5,}))", std::regex::ECMAScript | std::regex::icase),
        std::regex(R"(folder-chunk[^"']*folder_id=(\d{5,}))", std::regex::ECMAScript | std::regex::icase),
        std::regex(R"(file-storage/(?:view|download|folder)[^"']*(?:folder_id|object_id|file_id)=(\d{5,}))",
                   std::regex::ECMAScript | std::regex::icase)
    };

    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    std::string html = StripGlobalPortalHeader(html_text);

    for (const auto& re : kPatterns) {
        std::sregex_iterator it(html.begin(), html.end(), re);
        std::sregex_iterator end;
        for (; it != end; ++it) {
            std::string id = (*it)[1].str();
            if (!id.empty() && seen.insert(id).second) {
                out.push_back(std::move(id));
            }
        }
    }
    return out;
}

Result<std::vector<std::string>> FileStorageWalker::DiscoverRootFolderIds(
    std::string_view community_base_url,
    size_t max_candidates) {
    std::string comm(community_base_url);
    while (!comm.empty() && comm.back() == '/') comm.pop_back();
    if (comm.rfind("http://", 0) != 0 && comm.rfind("https://", 0) != 0) {
        comm = JoinUrl(base_url_, comm);
    }

    const std::string probe_urls[] = {
        comm + "/file-storage/",
        comm + "/one-community?page_num=3&loc=",
        comm + "/one-community?page_num=0&loc="
    };

    std::vector<std::string> result;
    std::unordered_set<std::string> seen;

    for (const std::string& url : probe_urls) {
        HttpRequest req;
        req.method = "GET";
        req.url = url;
        req.follow_redirects = true;
        req.headers["Accept"] = "text/html,application/xhtml+xml,*/*";
        HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
        auto validity = auth_.ValidateResponse(resp, url, true);
        if (validity == ResponseValidity::SessionExpired) {
            return Result<std::vector<std::string>>::Failure("session_expired",
                                                             "session expired probing file-storage");
        }
        if (!resp.transport_ok || resp.status_code != 200) {
            continue;
        }
        for (const auto& fid : ExtractRootFolderIds(resp.BodyAsString())) {
            if (seen.insert(fid).second) {
                result.push_back(fid);
                if (max_candidates > 0 && result.size() >= max_candidates) {
                    return Result<std::vector<std::string>>::Success(std::move(result));
                }
            }
        }
    }
    return Result<std::vector<std::string>>::Success(std::move(result));
}

Result<FileDetailedMetadata> FileStorageWalker::ParseFileDetailsJson(std::string_view json_text) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<FileDetailedMetadata>::Failure("invalid_json",
                                                         "file details JSON is not an object");
        }
        FileDetailedMetadata m;
        m.object_id = JsonIntOrZero(j, "object_id");
        m.parent_id = JsonIntOrZero(j, "parent_id");
        m.mime_type = JsonStringOrEmpty(j, "mime_type");
        m.name = JsonStringOrEmpty(j, "name");
        m.live_revision = JsonStringOrEmpty(j, "live_revision");
        m.type = JsonStringOrEmpty(j, "type");
        m.pretty_type = JsonStringOrEmpty(j, "pretty_type");
        m.last_modified_ansi = JsonStringOrEmpty(j, "last_modified_ansi");
        m.content_size = JsonU64OrZero(j, "content_size");
        m.url = JsonStringOrEmpty(j, "url");
        m.sort_key = JsonStringOrEmpty(j, "sort_key");
        m.sort_key_desc = JsonStringOrEmpty(j, "sort_key_desc");
        m.file_upload_name = JsonStringOrEmpty(j, "file_upload_name");
        m.title = JsonStringOrEmpty(j, "title");
        m.file_url = JsonStringOrEmpty(j, "file_url");
        m.new_p = static_cast<int>(JsonIntOrZero(j, "new_p"));
        m.fs_type = JsonStringOrEmpty(j, "fs_type");
        m.fs_sort_relevance = static_cast<int>(JsonIntOrZero(j, "fs_sort_relevance"));
        if (j.contains("author") && j["author"].is_object()) {
            m.author_id = JsonStringOrEmpty(j["author"], "id");
            m.author_name = JsonStringOrEmpty(j["author"], "name");
        }
        if (j.contains("privileges") && j["privileges"].is_object()) {
            const auto& p = j["privileges"];
            m.admin_p = static_cast<int>(JsonIntOrZero(p, "admin_p"));
            m.write_p = static_cast<int>(JsonIntOrZero(p, "write_p"));
            m.delete_p = static_cast<int>(JsonIntOrZero(p, "delete_p"));
            m.read_p = static_cast<int>(JsonIntOrZero(p, "read_p"));
        }
        return Result<FileDetailedMetadata>::Success(std::move(m));
    } catch (const std::exception& ex) {
        return Result<FileDetailedMetadata>::Failure("invalid_json", ex.what());
    }
}

Result<FileDetailedMetadata> FileStorageWalker::FetchFileDetails(std::string_view object_id) {
    HttpRequest req;
    req.method = "GET";
    req.url = JoinUrl(base_url_, "/dotlrn/file-storage/api/file/" + std::string(object_id));
    req.follow_redirects = true;
    req.headers["Accept"] = "application/json";
    req.headers["Referer"] = JoinUrl(base_url_, "/dotlrn/");

    HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
    auto val = auth_.ValidateResponse(resp, req.url, true);
    if (val == ResponseValidity::SessionExpired) {
        return Result<FileDetailedMetadata>::Failure("session_expired", "session expired");
    }
    if (!resp.transport_ok || resp.status_code != 200) {
        return Result<FileDetailedMetadata>::Failure(
            "http_error", "HTTP " + std::to_string(resp.status_code) + " fetching file details");
    }
    return ParseFileDetailsJson(resp.BodyAsString());
}

Result<std::vector<FileVersionRecord>> FileStorageWalker::FetchFileVersions(
    std::string_view object_id) {
    if (object_id.empty()) {
        return Result<std::vector<FileVersionRecord>>::Failure("invalid_arg", "object_id is empty");
    }
    HttpRequest req;
    req.method = "GET";
    req.url = JoinUrl(base_url_, "/dotlrn/file-storage/api/file/" + std::string(object_id) + "/versions");
    req.follow_redirects = true;
    req.headers["Accept"] = "application/json";
    req.headers["Referer"] = JoinUrl(base_url_, "/dotlrn/");

    HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
    auto val = auth_.ValidateResponse(resp, req.url, true);
    if (val == ResponseValidity::SessionExpired) {
        return Result<std::vector<FileVersionRecord>>::Failure("session_expired", "session expired");
    }
    if (!resp.transport_ok || resp.status_code != 200) {
        return Result<std::vector<FileVersionRecord>>::Failure(
            "http_error", "HTTP " + std::to_string(resp.status_code) + " fetching file versions");
    }
    auto versions = ParseFileVersionsJson(resp.BodyAsString());
    for (auto& v : versions) {
        v.title = sanitizer_.SanitizeText(v.title);
        v.author = sanitizer_.SanitizeText(v.author);
        v.description = sanitizer_.SanitizeText(v.description);
    }
    return Result<std::vector<FileVersionRecord>>::Success(std::move(versions));
}

Result<FolderWalkResult> FileStorageWalker::ParseFolderJson(std::string_view json_text,
                                                            std::string_view expected_folder_id) {
    try {
        auto j = nlohmann::json::parse(json_text);
        if (!j.is_object()) {
            return Result<FolderWalkResult>::Failure("invalid_json",
                                                     "folder API response is not an object");
        }

        const nlohmann::json* coll = nullptr;
        for (const char* k : {"elements", "items", "contents"}) {
            if (j.contains(k) && (j[k].is_array() || j[k].is_object())) {
                coll = &j[k];
                break;
            }
        }
        if (coll == nullptr) {
            return Result<FolderWalkResult>::Failure(
                "missing_collection", "folder JSON lacks elements/items/contents");
        }

        FolderWalkResult out;
        std::string cur_fid = FirstNonEmptyJsonStr(j, {"object_id", "folder_id", "id"});
        if (cur_fid.empty()) cur_fid = std::string(expected_folder_id);
        out.root_folder_id = cur_fid;

        FolderRecord cur_folder;
        cur_folder.folder_id = cur_fid;
        cur_folder.name = FirstNonEmptyJsonStr(j, {"name", "title"});
        cur_folder.folder_path = JsonStringOrEmpty(j, "folder_path");

        std::vector<nlohmann::json> items;
        if (coll->is_array()) {
            for (const auto& elem : *coll) {
                items.push_back(elem);
            }
        } else if (coll->is_object()) {
            for (auto it = coll->begin(); it != coll->end(); ++it) {
                items.push_back(it.value());
            }
        }
        cur_folder.child_count = items.size();
        out.folders.push_back(std::move(cur_folder));

        if (j.contains("breadcrumbs") && j["breadcrumbs"].is_array()) {
            for (const auto& bc : j["breadcrumbs"]) {
                if (!bc.is_object()) continue;
                std::string t = JsonStringOrEmpty(bc, "title");
                std::string u = JsonStringOrEmpty(bc, "url");
                if (!t.empty() || !u.empty()) {
                    out.breadcrumbs.emplace_back(std::move(t), std::move(u));
                }
            }
        }

        for (const auto& item : items) {
            if (!item.is_object()) continue;
            std::string fs_type = AsciiLower(JsonStringOrEmpty(item, "fs_type"));
            std::string type = AsciiLower(JsonStringOrEmpty(item, "type"));
            std::string explicit_fid = JsonStringOrEmpty(item, "folder_id");

            if (fs_type == "folder" ||
                type == "folder" || type == "dir" || type == "directory" || type == "carpeta" ||
                (!explicit_fid.empty() && explicit_fid != cur_fid && fs_type != "file" && fs_type != "url")) {
                std::string sub_id = FirstNonEmptyJsonStr(item, {"object_id", "folder_id", "id"});
                if (!sub_id.empty() && sub_id != cur_fid) {
                    FolderRecord sub;
                    sub.folder_id = sub_id;
                    sub.parent_id = cur_fid;
                    sub.name = FirstNonEmptyJsonStr(item, {"name", "title"});
                    sub.folder_path = JsonStringOrEmpty(item, "folder_path");
                    out.folders.push_back(std::move(sub));
                }
            } else if (fs_type == "url" || type == "url") {
                FolderUrlLink lnk;
                lnk.object_id = FirstNonEmptyJsonStr(item, {"object_id", "id"});
                lnk.parent_folder_id = cur_fid;
                lnk.title = FirstNonEmptyJsonStr(item, {"title", "name"});
                lnk.url = JsonStringOrEmpty(item, "url");
                out.url_links.push_back(std::move(lnk));
            } else {
                std::string oid = FirstNonEmptyJsonStr(item, {"object_id", "id", "file_id"});
                if (oid.empty() || oid == "0") continue;
                FileRecord fr;
                fr.object_id = oid;
                fr.parent_folder_id = cur_fid;
                fr.file_upload_name = JsonStringOrEmpty(item, "file_upload_name");
                fr.name = FirstNonEmptyJsonStr(item, {"file_upload_name", "name", "title"});
                if (fr.name.empty()) fr.name = "object_" + oid;
                fr.title = FirstNonEmptyJsonStr(item, {"title", "name", "file_upload_name"});
                fr.mime_type = FirstNonEmptyJsonStr(item, {"mime_type", "mime", "content_type"});
                fr.content_size = JsonU64OrZero(item, "content_size");
                if (fr.content_size == 0) {
                    fr.content_size = JsonU64OrZero(item, "size");
                }
                fr.live_revision = JsonStringOrEmpty(item, "live_revision");
                fr.creation_date_ansi = JsonStringOrEmpty(item, "creation_date_ansi");
                fr.last_modified_ansi = JsonStringOrEmpty(item, "last_modified_ansi");
                if (item.contains("author") && item["author"].is_object()) {
                    fr.author_name = JsonStringOrEmpty(item["author"], "name");
                }
                if (item.contains("plagiarism") && item["plagiarism"].is_object()) {
                    const auto& pl = item["plagiarism"];
                    fr.plagiarism_status = JsonStringOrEmpty(pl, "status");
                    fr.plagiarism_text = JsonStringOrEmpty(pl, "text");
                    fr.plagiarism_doc_uid = JsonStringOrEmpty(pl, "doc_uid");
                    if (pl.contains("percentaje") && pl["percentaje"].is_number()) {
                        fr.plagiarism_percentage = pl["percentaje"].get<double>();
                    } else if (pl.contains("percentage") && pl["percentage"].is_number()) {
                        fr.plagiarism_percentage = pl["percentage"].get<double>();
                    }
                }
                if (item.contains("subscription") && item["subscription"].is_object()) {
                    const auto& sub = item["subscription"];
                    if (sub.contains("subscribed") && sub["subscribed"].is_boolean()) {
                        fr.subscription_active = sub["subscribed"].get<bool>();
                    }
                }
                fr.file_url = FirstNonEmptyJsonStr(item, {"file_url", "url"});
                out.files.push_back(std::move(fr));
            }
        }

        return Result<FolderWalkResult>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<FolderWalkResult>::Failure("invalid_json", ex.what());
    }
}

Result<FolderWalkResult> FileStorageWalker::WalkFolderTree(
    std::string_view root_folder_id,
    std::string_view community_slug,
    std::string_view community_type,
    std::string_view community_base_url,
    const WalkerOptions& opts) {
    FolderWalkResult agg;
    agg.root_folder_id = std::string(root_folder_id);
    if (agg.root_folder_id.empty()) {
        return Result<FolderWalkResult>::Failure("empty_folder_id", "root_folder_id is empty");
    }

    struct QueueEntry {
        std::string folder_id;
        std::string parent_id;
        std::string rel_path;
        int depth = 0;
    };

    std::queue<QueueEntry> q;
    std::unordered_set<std::string> visited;
    q.push({agg.root_folder_id, "", "", 0});

    while (!q.empty()) {
        QueueEntry cur = std::move(q.front());
        q.pop();

        if (cur.depth > opts.max_depth) {
            continue;
        }
        if (!visited.insert(cur.folder_id).second) {
            continue;
        }
        if (opts.max_folders > 0 && agg.folders.size() >= opts.max_folders) {
            agg.warnings.push_back("max_folders cap reached");
            break;
        }

        if (opts.request_delay_ms > 0) {
            SleepMs(opts.request_delay_ms);
        }

        HttpRequest req;
        req.method = "GET";
        req.url = JoinUrl(base_url_, "/dotlrn/file-storage/api/folder/" + cur.folder_id);
        req.follow_redirects = true;
        req.headers["Accept"] = "application/json";
        req.headers["Referer"] = JoinUrl(base_url_, "/dotlrn/");

        HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
        auto validity = auth_.ValidateResponse(resp, req.url, true);
        if (validity == ResponseValidity::SessionExpired) {
            agg.session_expired = true;
            agg.warnings.push_back("session expired while walking folder " + cur.folder_id);
            break;
        }
        if (!resp.transport_ok || resp.status_code != 200) {
            agg.warnings.push_back("folder " + cur.folder_id + " HTTP " +
                                   std::to_string(resp.status_code));
            continue;
        }

        auto parsed = ParseFolderJson(resp.BodyAsString(), cur.folder_id);
        if (!parsed.ok) {
            agg.warnings.push_back("folder " + cur.folder_id + ": " +
                                   sanitizer_.SanitizeText(parsed.error_message));
            continue;
        }

        FolderRecord this_folder = parsed.value.folders.front();
        this_folder.parent_id = cur.parent_id;
        this_folder.community = std::string(community_slug);
        this_folder.community_type = std::string(community_type);
        this_folder.depth = cur.depth;
        if (!cur.rel_path.empty()) {
            this_folder.folder_path = cur.rel_path;
        }
        std::string current_rel = cur.rel_path;
        if (current_rel.empty() && !this_folder.name.empty() && cur.depth > 0) {
            current_rel = this_folder.name;
        }
        agg.folders.push_back(this_folder);

        // Subfolders (entries after index 0)
        for (size_t i = 1; i < parsed.value.folders.size(); ++i) {
            const auto& sub = parsed.value.folders[i];
            if (sub.folder_id == cur.folder_id || visited.count(sub.folder_id) > 0) {
                continue;
            }
            if (cur.depth + 1 <= opts.max_depth) {
                std::string sub_name = sub.name.empty() ? ("folder_" + sub.folder_id) : sub.name;
                std::string sub_rel = current_rel.empty() ? sub_name : (current_rel + "/" + sub_name);
                q.push({sub.folder_id, cur.folder_id, std::move(sub_rel), cur.depth + 1});
            }
        }

        for (auto lnk : parsed.value.url_links) {
            lnk.community = std::string(community_slug);
            lnk.folder_path = current_rel;
            agg.url_links.push_back(std::move(lnk));
        }

        for (auto fr : parsed.value.files) {
            fr.community = std::string(community_slug);
            fr.community_type = std::string(community_type);
            fr.folder_path = current_rel;
            fr.candidate_urls = StorageDownloader::BuildCandidateUrls(fr, base_url_, community_base_url);
            if (!fr.candidate_urls.empty()) {
                fr.download_url = fr.candidate_urls.front();
            }
            agg.files.push_back(std::move(fr));
        }
    }

    return Result<FolderWalkResult>::Success(std::move(agg));
}

StorageDownloader::StorageDownloader(PolicyEnforcingTransport& transport,
                                     AuthClient& auth,
                                     std::string base_url,
                                     DownloaderOptions opts,
                                     SanitizeOptions sanitize_opts)
    : transport_(transport),
      auth_(auth),
      base_url_(std::move(base_url)),
      opts_(std::move(opts)),
      sanitizer_(std::move(sanitize_opts)) {
    while (!base_url_.empty() && base_url_.back() == '/') {
        base_url_.pop_back();
    }
}

std::vector<std::string> StorageDownloader::BuildCandidateUrls(
    const FileRecord& file,
    std::string_view base_url,
    std::string_view community_base_url) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();

    std::string comm(community_base_url);
    while (!comm.empty() && comm.back() == '/') comm.pop_back();
    if (!comm.empty() && comm.rfind("http://", 0) != 0 && comm.rfind("https://", 0) != 0) {
        comm = JoinUrl(b, comm);
    }

    std::vector<std::string> out;
    auto push_unique = [&](std::string u) {
        if (u.empty()) return;
        if (std::find(out.begin(), out.end(), u) == out.end()) {
            out.push_back(std::move(u));
        }
    };

    if (!file.download_url.empty()) {
        push_unique(file.download_url);
    }

    std::string title_for_url = !file.title.empty() ? file.title : file.name;
    const bool is_numeric_oid = !file.object_id.empty() &&
                                std::all_of(file.object_id.begin(), file.object_id.end(),
                                            [](unsigned char c) { return std::isdigit(c) != 0; });

    if (is_numeric_oid) {
        if (!title_for_url.empty()) {
            std::string storage_base = comm.empty() ? (b + "/dotlrn/file-storage")
                                                    : (comm + "/file-storage");
            push_unique(storage_base + "/download/" + UrlEncodeSegment(title_for_url) +
                        "?file_id=" + file.object_id);
        }
        push_unique(b + "/dotlrn/file-storage/download?file_id=" + file.object_id);
    }

    if (!file.file_url.empty()) {
        std::string enc_rel = UrlEncodePathKeepingSlashes(file.file_url);
        if (!enc_rel.empty()) {
            if (!comm.empty()) {
                push_unique(comm + "/file-storage/view/" + enc_rel);
            }
            push_unique(b + "/dotlrn/file-storage/view/" + enc_rel);
        }
    }

    if (is_numeric_oid) {
        push_unique(b + "/dotlrn/file-storage/view/" + file.object_id);
    }
    return out;
}

std::string StorageDownloader::BuildBulkZipUrl(std::string_view community_base_url,
                                               std::string_view root_folder_id) {
    std::string comm(community_base_url);
    while (!comm.empty() && comm.back() == '/') comm.pop_back();
    return comm + "/file-storage/download-zip/?folder_id=" + std::string(root_folder_id);
}

std::string StorageDownloader::BuildArchiveZipUrl(std::string_view community_base_url,
                                                  std::string_view folder_object_id) {
    std::string comm(community_base_url);
    while (!comm.empty() && comm.back() == '/') comm.pop_back();
    return comm + "/file-storage/download-archive?object_id=" + std::string(folder_object_id);
}

MagicCheckResult StorageDownloader::ValidateMagicAndSafety(
    const uint8_t* data,
    size_t len,
    std::string_view filename,
    std::string_view content_type) {
    MagicCheckResult res;
    if (data == nullptr || len == 0) {
        res.valid = false;
        res.reason = "empty file";
        return res;
    }

    std::string_view head_sv(reinterpret_cast<const char*>(data),
                             std::min<size_t>(len, 50000));
    std::string head_low = AsciiLower(head_sv.substr(0, std::min<size_t>(head_sv.size(), 4096)));

    if (AuthClient::ContainsLoginMarkers(head_sv) ||
        head_low.find("type=\"password\"") != std::string::npos ||
        head_low.find("type='password'") != std::string::npos ||
        head_low.find("api/login") != std::string::npos) {
        res.valid = false;
        res.is_html_login = true;
        res.detected_kind = "html-login";
        res.reason = "HTML login redirect detected";
        return res;
    }

    std::string trimmed_low = TrimAscii(head_low);
    std::string ct_low = AsciiLower(content_type);
    std::string ext = LowerExtension(filename);
    const bool expects_html = (ext == ".html" || ext == ".htm");
    const bool starts_html =
        (trimmed_low.rfind("<!doc", 0) == 0 || trimmed_low.rfind("<html", 0) == 0 ||
         trimmed_low.rfind("<head", 0) == 0 || trimmed_low.rfind("<body", 0) == 0);
    const bool ct_is_html = (ct_low.find("text/html") != std::string::npos ||
                             ct_low.find("application/xhtml") != std::string::npos);

    if (!expects_html && (starts_html || ct_is_html)) {
        res.valid = false;
        res.is_generic_html = true;
        res.detected_kind = "html";
        res.reason = "unexpected HTML response";
        return res;
    }

    auto has_prefix = [&](const uint8_t* pat, size_t plen) {
        return len >= plen && std::memcmp(data, pat, plen) == 0;
    };

    static constexpr uint8_t kPdf[4] = {'%', 'P', 'D', 'F'};
    static constexpr uint8_t kPk34[4] = {'P', 'K', 0x03, 0x04};
    static constexpr uint8_t kPk56[4] = {'P', 'K', 0x05, 0x06};
    static constexpr uint8_t kOle[8] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
    static constexpr uint8_t kPng[4] = {0x89, 'P', 'N', 'G'};
    static constexpr uint8_t kJpeg[2] = {0xFF, 0xD8};
    static constexpr uint8_t kGif[4] = {'G', 'I', 'F', '8'};
    static constexpr uint8_t kGzip[2] = {0x1F, 0x8B};
    static constexpr uint8_t kId3[3] = {'I', 'D', '3'};

    if (has_prefix(kPdf, 4)) res.detected_kind = "pdf";
    else if (has_prefix(kPk34, 4) || has_prefix(kPk56, 4)) res.detected_kind = "zip-family";
    else if (len >= 4 && std::memcmp(data, kOle, 4) == 0) res.detected_kind = "ole-doc";
    else if (has_prefix(kPng, 4)) res.detected_kind = "image/png";
    else if (has_prefix(kJpeg, 2)) res.detected_kind = "image/jpeg";
    else if (has_prefix(kGif, 4)) res.detected_kind = "image/gif";
    else if (has_prefix(kGzip, 2)) res.detected_kind = "gzip";
    else if (has_prefix(kId3, 3) || (len >= 2 && data[0] == 0xFF && (data[1] & 0xE0u) == 0xE0u)) {
        res.detected_kind = "audio/mpeg";
    } else if (len >= 12 && std::memcmp(data + 4, "ftyp", 4) == 0) {
        res.detected_kind = "mp4";
    } else if (IsTextOrDocExt(ext) || ct_low.rfind("text/", 0) == 0 ||
               ct_low.find("json") != std::string::npos || ct_low.find("csv") != std::string::npos) {
        res.detected_kind = "text";
    } else {
        res.detected_kind = "unknown";
    }

    auto require_kind = [&](std::string_view expected) -> bool {
        if (res.detected_kind == expected) {
            res.valid = true;
            return true;
        }
        res.valid = false;
        res.reason = "magic bytes mismatch: expected " + std::string(expected) +
                     ", got " + res.detected_kind;
        return false;
    };

    if (ext == ".pdf") {
        require_kind("pdf");
        return res;
    }
    if (ext == ".zip" || ext == ".docx" || ext == ".pptx" || ext == ".xlsx" ||
        ext == ".odt" || ext == ".ods" || ext == ".odp") {
        require_kind("zip-family");
        return res;
    }
    if (ext == ".doc" || ext == ".ppt" || ext == ".xls") {
        require_kind("ole-doc");
        return res;
    }
    if (ext == ".png") {
        require_kind("image/png");
        return res;
    }
    if (ext == ".jpg" || ext == ".jpeg") {
        require_kind("image/jpeg");
        return res;
    }
    if (ext == ".gif") {
        require_kind("image/gif");
        return res;
    }
    if (ext == ".gz" || ext == ".tgz") {
        require_kind("gzip");
        return res;
    }
    if (ext == ".mp3") {
        require_kind("audio/mpeg");
        return res;
    }
    if (ext == ".mp4" || ext == ".m4v" || ext == ".mov" || ext == ".m4a") {
        require_kind("mp4");
        return res;
    }

    // Allowed non-binary/unknown extensions when not HTML login/error page
    res.valid = true;
    return res;
}

std::string StorageDownloader::SafeFilename(std::string_view name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (c < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' ||
            c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
            out.push_back('_');
        } else {
            out.push_back(static_cast<char>(c));
        }
    }
    out = TrimAscii(out);
    while (!out.empty() && out.front() == '.') out.erase(out.begin());
    if (out.empty() || out == "." || out == "..") out = "file";
    if (out.size() > 150) {
        size_t dot = out.rfind('.');
        std::string ext = (dot != std::string::npos && out.size() - dot <= 16) ? out.substr(dot) : "";
        out = out.substr(0, 150 - ext.size()) + ext;
    }
    return out;
}

std::string StorageDownloader::SafeRelPath(std::string_view rel_path) {
    std::vector<std::string> segs;
    size_t pos = 0;
    while (pos <= rel_path.size()) {
        size_t slash = rel_path.find_first_of("/\\", pos);
        if (slash == std::string_view::npos) slash = rel_path.size();
        std::string seg(rel_path.substr(pos, slash - pos));
        seg = TrimAscii(seg);
        if (!seg.empty() && seg != "." && seg != "..") {
            std::string cleaned;
            for (unsigned char c : seg) {
                if (c < 0x20 || c == '<' || c == '>' || c == ':' || c == '"' ||
                    c == '/' || c == '\\' || c == '|' || c == '?' || c == '*') {
                    cleaned.push_back('_');
                } else {
                    cleaned.push_back(static_cast<char>(c));
                }
            }
            if (cleaned.size() > 100) cleaned.resize(100);
            if (!cleaned.empty()) segs.push_back(std::move(cleaned));
        }
        if (slash == rel_path.size()) break;
        pos = slash + 1;
    }
    std::string out;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (i > 0) out.push_back('/');
        out += segs[i];
    }
    return out;
}

namespace {

bool HasCompressedContentEncoding(const HttpResponse& r) {
    for (const auto& ce : r.GetHeadersCI("Content-Encoding")) {
        std::string low = AsciiLower(TrimAscii(ce));
        if (low.find("gzip") != std::string::npos ||
            low.find("deflate") != std::string::npos ||
            low.find("br") != std::string::npos) {
            return true;
        }
    }
    return false;
}

}  // namespace

DownloadResult StorageDownloader::DownloadFile(FileRecord& file,
                                               const std::string& offline_root,
                                               std::string_view community_base_url) {
    DownloadResult res;
    if (auth_.IsSessionExpired()) {
        res.outcome = DownloadOutcome::SessionExpired;
        file.status = FileStatus::SessionExpired;
        res.error_message = "session already expired";
        file.error_message = res.error_message;
        return res;
    }

    if (opts_.max_file_bytes > 0 && file.content_size > opts_.max_file_bytes) {
        res.outcome = DownloadOutcome::SkippedLarge;
        file.status = FileStatus::SkippedLarge;
        res.error_message = "file exceeds max_file_bytes";
        file.error_message = res.error_message;
        return res;
    }

    if (has_last_start_ && opts_.min_delay_ms > 0) {
        int64_t elapsed = NowSteadyMs() - last_start_steady_ms_;
        if (elapsed < opts_.min_delay_ms) {
            SleepMs(static_cast<int>(opts_.min_delay_ms - elapsed));
        }
    }
    has_last_start_ = true;
    last_start_steady_ms_ = NowSteadyMs();

    const fs::path root = fs::u8path(offline_root);
    const fs::path blobs_root = root / "downloads" / "_blobs";
    const fs::path tmp_root = root / "downloads" / "_tmp";
    const fs::path quarantine_root = root / "downloads" / "_quarantine";
    std::error_code ec;
    fs::create_directories(blobs_root, ec);
    fs::create_directories(tmp_root, ec);

    std::string safe_oid = SafeFilename(file.object_id.empty() ? file.name : file.object_id);
    const fs::path part_path = tmp_root / (safe_oid + ".part");

    std::vector<std::string> candidates = file.candidate_urls;
    if (candidates.empty()) {
        candidates = BuildCandidateUrls(file, base_url_, community_base_url);
    }
    if (candidates.empty()) {
        res.outcome = DownloadOutcome::Failed;
        file.status = FileStatus::Failed;
        res.error_message = "no download candidate URLs";
        file.error_message = res.error_message;
        return res;
    }

    uint64_t remote_size = file.content_size;
    std::string active_url = candidates.front();
    std::string last_content_type = file.mime_type;
    std::string last_error_msg;
    bool all_404 = true;
    bool committed_part = false;

    for (size_t cand_idx = 0; cand_idx < candidates.size(); ++cand_idx) {
        active_url = candidates[cand_idx];

        if (remote_size == 0 && opts_.verify_size_with_head) {
            HttpRequest hreq;
            hreq.method = "HEAD";
            hreq.url = active_url;
            hreq.follow_redirects = true;
            hreq.max_redirects = 5;
            HttpResponse hresp = detail::ExecuteSessionRequest(transport_, auth_, std::move(hreq));
            auto hval = auth_.ValidateResponse(hresp, active_url, false);
            if (hval == ResponseValidity::SessionExpired) {
                fs::remove(part_path, ec);
                res.outcome = DownloadOutcome::SessionExpired;
                file.status = FileStatus::SessionExpired;
                res.error_message = "session expired on HEAD";
                file.error_message = res.error_message;
                return res;
            }
            if (hresp.transport_ok && hresp.status_code == 200 && hresp.has_content_length &&
                !HasCompressedContentEncoding(hresp)) {
                remote_size = hresp.content_length;
            }
        }

        if (opts_.max_file_bytes > 0 && remote_size > opts_.max_file_bytes) {
            fs::remove(part_path, ec);
            res.outcome = DownloadOutcome::SkippedLarge;
            file.status = FileStatus::SkippedLarge;
            res.error_message = "remote size exceeds max_file_bytes";
            file.error_message = res.error_message;
            return res;
        }

        bool candidate_404 = false;
        committed_part = false;
        bool retried_416 = false;
        for (int attempt = 0; attempt <= opts_.max_retries; ++attempt) {
            uint64_t start_byte = 0;
            if (fs::is_regular_file(part_path, ec)) {
                start_byte = fs::file_size(part_path, ec);
                if (ec) start_byte = 0;
            }
            if (remote_size > 0 && start_byte > remote_size) {
                fs::remove(part_path, ec);
                start_byte = 0;
            }
            if (remote_size > 0 && start_byte == remote_size && start_byte > 0) {
                committed_part = true;
                all_404 = false;
                break;
            }

            HttpRequest req;
            req.method = "GET";
            req.url = active_url;
            req.follow_redirects = true;
            req.max_redirects = 5;
            req.range_start = (start_byte > 0) ? start_byte : 0;
            req.range_end = 0;
            if (start_byte == 0) {
                res.saw_range_header_omitted_on_zero =
                    (req.range_start == 0) && (req.headers.find("Range") == req.headers.end());
            }
            if (remote_size > 200ULL * 1024ULL * 1024ULL) {
                req.receive_timeout_ms = opts_.timeout_large_ms;
            } else if (remote_size > 50ULL * 1024ULL * 1024ULL) {
                req.receive_timeout_ms = opts_.timeout_medium_ms;
            } else {
                req.receive_timeout_ms = opts_.timeout_small_ms;
            }

            bool header_cb_called = false;
            int stream_status = 0;
            bool stream_is_redirect = false;
            bool stream_login_redirect = false;
            bool stall_aborted = false;
            bool size_exceeded = false;
            uint64_t active_base_offset = start_byte;
            uint64_t bytes_this_session = 0;
            std::ofstream part_ofs;
            const int64_t t_start_ms = NowSteadyMs();

            auto open_part_stream = [&](int status) -> bool {
                if (part_ofs.is_open()) part_ofs.close();
                if (status == 200) {
                    // Server ignored Range or fresh download -> truncate cleanly
                    active_base_offset = 0;
                    part_ofs.open(part_path, std::ios::binary | std::ios::trunc);
                } else if (status == 206) {
                    active_base_offset = start_byte;
                    part_ofs.open(part_path, std::ios::binary | std::ios::app);
                } else {
                    return false;
                }
                return part_ofs.is_open();
            };

            req.header_callback = [&](const HttpResponse& meta) -> bool {
                header_cb_called = true;
                stream_status = meta.status_code;
                if (!meta.content_type.empty()) last_content_type = meta.content_type;
                if (meta.status_code == 301 || meta.status_code == 302 ||
                    meta.status_code == 303 || meta.status_code == 307 || meta.status_code == 308) {
                    stream_is_redirect = true;
                    if (AuthClient::IsLoginRedirectLocation(meta.location)) {
                        stream_login_redirect = true;
                        return false;
                    }
                    return true;
                }
                stream_is_redirect = false;
                if (meta.status_code == 200 && meta.has_content_length &&
                    !HasCompressedContentEncoding(meta)) {
                    remote_size = meta.content_length;
                }
                if (meta.status_code == 200 || meta.status_code == 206) {
                    return open_part_stream(meta.status_code);
                }
                return true;
            };

            req.chunk_callback = [&](const uint8_t* chunk, size_t len) -> bool {
                if (stream_is_redirect) {
                    return true;
                }
                if (header_cb_called && stream_status != 200 && stream_status != 206) {
                    return true;
                }
                if (!part_ofs.is_open()) {
                    if (!open_part_stream(start_byte > 0 ? 206 : 200)) return false;
                }
                if (len > 0) {
                    if (opts_.max_file_bytes > 0 &&
                        active_base_offset + bytes_this_session + len > opts_.max_file_bytes) {
                        size_exceeded = true;
                        return false;
                    }
                    part_ofs.write(reinterpret_cast<const char*>(chunk),
                                   static_cast<std::streamsize>(len));
                    if (!part_ofs.good()) return false;
                    bytes_this_session += len;
                }
                int64_t elapsed = NowSteadyMs() - t_start_ms;
                if (opts_.stall_grace_ms >= 0 && elapsed >= opts_.stall_grace_ms) {
                    double secs = std::max(0.001, static_cast<double>(elapsed) / 1000.0);
                    double bps = static_cast<double>(bytes_this_session) / secs;
                    if (bps < opts_.stall_min_bps) {
                        stall_aborted = true;
                        return false;
                    }
                }
                return true;
            };

            HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
            if (part_ofs.is_open()) {
                part_ofs.flush();
                part_ofs.close();
            }

            res.http_status = resp.status_code;
            file.http_status = resp.status_code;
            if (!resp.content_type.empty()) last_content_type = resp.content_type;

            if (size_exceeded) {
                fs::remove(part_path, ec);
                res.outcome = DownloadOutcome::SkippedLarge;
                file.status = FileStatus::SkippedLarge;
                res.error_message = "stream exceeded max_file_bytes";
                file.error_message = res.error_message;
                return res;
            }

            if (stream_login_redirect) {
                fs::remove(part_path, ec);
                auth_.ValidateResponse(resp, active_url, false);
                res.outcome = DownloadOutcome::SessionExpired;
                file.status = FileStatus::SessionExpired;
                res.error_message = "login redirect during download";
                file.error_message = res.error_message;
                return res;
            }

            if (resp.error_message.rfind("OffOriginRedirect", 0) == 0 ||
                resp.error_message.rfind("read-only-request-guard:", 0) == 0) {
                if (fs::is_regular_file(part_path, ec) && fs::file_size(part_path, ec) == 0) {
                    fs::remove(part_path, ec);
                }
                all_404 = false;
                res.outcome = DownloadOutcome::Failed;
                file.status = FileStatus::Failed;
                res.error_message = sanitizer_.SanitizeText(resp.error_message);
                file.error_message = res.error_message;
                return res;
            }

            if (resp.transport_ok && resp.status_code == 404) {
                fs::remove(part_path, ec);
                candidate_404 = true;
                break;
            }

            if (resp.transport_ok && (resp.status_code == 401 || resp.status_code == 403)) {
                all_404 = false;
                bool session_ok = auth_.CheckSession(true);
                if (!session_ok) {
                    fs::remove(part_path, ec);
                    res.outcome = DownloadOutcome::SessionExpired;
                    file.status = FileStatus::SessionExpired;
                    res.error_message = "session expired (HTTP " + std::to_string(resp.status_code) + ")";
                    file.error_message = res.error_message;
                    return res;
                }
                res.outcome = DownloadOutcome::Failed;
                file.status = FileStatus::Failed;
                res.error_message = "HTTP " + std::to_string(resp.status_code) + " permission denied";
                file.error_message = res.error_message;
                return res;
            }

            auto val = auth_.ValidateResponse(resp, active_url, false);
            if (val == ResponseValidity::SessionExpired) {
                fs::remove(part_path, ec);
                res.outcome = DownloadOutcome::SessionExpired;
                file.status = FileStatus::SessionExpired;
                res.error_message = "session expired";
                file.error_message = res.error_message;
                return res;
            }

            if (resp.transport_ok && resp.status_code == 416 && start_byte > 0 && !retried_416) {
                retried_416 = true;
                fs::remove(part_path, ec);
                --attempt;
                continue;
            }

            // If transport/mock populated resp.body without invoking chunk_callback, write it now
            if (resp.transport_ok && (resp.status_code == 200 || resp.status_code == 206) &&
                !header_cb_called && bytes_this_session == 0 && !resp.body.empty()) {
                uint64_t body_base = (resp.status_code == 200) ? 0 : start_byte;
                if (opts_.max_file_bytes > 0 && body_base + resp.body.size() > opts_.max_file_bytes) {
                    fs::remove(part_path, ec);
                    res.outcome = DownloadOutcome::SkippedLarge;
                    file.status = FileStatus::SkippedLarge;
                    res.error_message = "stream exceeded max_file_bytes";
                    file.error_message = res.error_message;
                    return res;
                }
                if (open_part_stream(resp.status_code)) {
                    part_ofs.write(reinterpret_cast<const char*>(resp.body.data()),
                                   static_cast<std::streamsize>(resp.body.size()));
                    part_ofs.flush();
                    part_ofs.close();
                }
            }

            const bool retryable =
                stall_aborted || !resp.transport_ok ||
                resp.status_code == 429 || resp.status_code >= 500;

            if (!retryable && (resp.status_code == 200 || resp.status_code == 206)) {
                all_404 = false;
                const bool compressed = HasCompressedContentEncoding(resp);
                if (resp.status_code == 200 && resp.has_content_length && !compressed) {
                    remote_size = resp.content_length;
                }
                uint64_t disk_sz = fs::is_regular_file(part_path, ec) ? fs::file_size(part_path, ec) : 0;
                if (!compressed && remote_size > 0 && disk_sz < remote_size) {
                    // Partial/throttled response -> retry with Range resume
                    last_error_msg = "incomplete download (" + std::to_string(disk_sz) +
                                     "/" + std::to_string(remote_size) + " bytes)";
                    if (attempt < opts_.max_retries) {
                        if (!opts_.backoff_ms.empty()) {
                            int delay = opts_.backoff_ms[std::min<size_t>(
                                static_cast<size_t>(attempt), opts_.backoff_ms.size() - 1)];
                            SleepMs(delay);
                        }
                        continue;
                    }
                    break;
                }
                committed_part = true;
                break;
            }

            all_404 = false;
            if (retryable && attempt < opts_.max_retries) {
                if (!opts_.backoff_ms.empty()) {
                    int delay = opts_.backoff_ms[std::min<size_t>(
                        static_cast<size_t>(attempt), opts_.backoff_ms.size() - 1)];
                    SleepMs(delay);
                }
                continue;
            }

            res.outcome = DownloadOutcome::Failed;
            file.status = FileStatus::Failed;
            res.error_message = stall_aborted
                                    ? "stalled download aborted"
                                    : (resp.error_message.empty()
                                           ? ("HTTP " + std::to_string(resp.status_code))
                                           : sanitizer_.SanitizeText(resp.error_message));
            file.error_message = res.error_message;
            return res;
        }

        if (committed_part) {
            break;
        }
        if (candidate_404) {
            continue;
        }
    }

    if (all_404) {
        fs::remove(part_path, ec);
        res.outcome = DownloadOutcome::Orphan404;
        file.status = FileStatus::Orphan404;
        res.error_message = "HTTP 404 on all candidate URLs";
        file.error_message = res.error_message;
        return res;
    }

    if (!committed_part) {
        res.outcome = DownloadOutcome::Failed;
        file.status = FileStatus::Failed;
        res.error_message = last_error_msg.empty() ? "download incomplete after retries" : last_error_msg;
        file.error_message = res.error_message;
        return res;
    }

    if (!fs::is_regular_file(part_path, ec)) {
        res.outcome = DownloadOutcome::Failed;
        file.status = FileStatus::Failed;
        res.error_message = "downloaded .part file missing";
        file.error_message = res.error_message;
        return res;
    }

    // Validate magic bytes & login markers on .part BEFORE CAS commit
    std::vector<uint8_t> head_bytes;
    ReadFileHead(part_path, 50000, head_bytes);
    MagicCheckResult mcheck = ValidateMagicAndSafety(
        head_bytes.data(), head_bytes.size(), file.name, last_content_type);
    file.detected_magic = mcheck.detected_kind;

    if (mcheck.is_html_login) {
        fs::remove(part_path, ec);
        // Mark auth session expired
        HttpResponse fake_login_resp;
        fake_login_resp.transport_ok = true;
        fake_login_resp.status_code = 200;
        fake_login_resp.content_type = "text/html";
        fake_login_resp.body = head_bytes;
        auth_.ValidateResponse(fake_login_resp, active_url, false);

        res.outcome = DownloadOutcome::SessionExpired;
        file.status = FileStatus::SessionExpired;
        res.error_message = "HTML login page received in download";
        file.error_message = res.error_message;
        return res;
    }

    if (!mcheck.valid) {
        fs::create_directories(quarantine_root, ec);
        std::string bad_name = safe_oid + "_" + SafeFilename(file.name) + ".bad";
        fs::path bad_path = quarantine_root / bad_name;
        MoveOrCopyAtomic(part_path, bad_path);

        res.outcome = DownloadOutcome::Failed;
        file.status = FileStatus::Failed;
        res.magic_ok = false;
        file.magic_ok = false;
        res.quarantine_rel_path = "downloads/_quarantine/" + bad_name;
        file.quarantine_rel_path = res.quarantine_rel_path;
        res.error_message = mcheck.reason;
        file.error_message = mcheck.reason;
        return res;
    }

    // Compute SHA-256 and commit to CAS _blobs/<sha[:2]>/<sha256>
    std::string sha_err;
    std::string sha = Sha256FileHex(part_path.u8string(), sha_err);
    if (sha.size() != 64) {
        res.outcome = DownloadOutcome::Failed;
        file.status = FileStatus::Failed;
        res.error_message = "sha256 failed: " + sha_err;
        file.error_message = res.error_message;
        return res;
    }

    const uint64_t final_bytes = fs::file_size(part_path, ec);
    const fs::path blob_rel = fs::path("downloads") / "_blobs" / sha.substr(0, 2) / sha;
    const fs::path blob_abs = root / blob_rel;
    bool deduplicated = false;

    if (fs::is_regular_file(blob_abs, ec)) {
        deduplicated = true;
        fs::remove(part_path, ec);
    } else {
        auto mv = MoveOrCopyAtomic(part_path, blob_abs);
        if (!mv.ok) {
            res.outcome = DownloadOutcome::Failed;
            file.status = FileStatus::Failed;
            res.error_message = mv.error_message;
            file.error_message = mv.error_message;
            return res;
        }
    }

    std::string logical_rel_str;
    if (opts_.create_logical_mirror) {
        std::string bucket = (AsciiLower(file.community_type).find("cat") != std::string::npos)
                                 ? "catedras"
                                 : "courses";
        std::string safe_comm = SafeRelPath(file.community.empty() ? "portal" : file.community);
        std::string safe_folder = SafeRelPath(file.folder_path);
        std::string safe_name = SafeFilename(file.name);

        fs::path logical_dir = fs::path("downloads") / bucket / safe_comm;
        if (!safe_folder.empty()) logical_dir /= fs::u8path(safe_folder);

        fs::path logical_rel = logical_dir / fs::u8path(safe_name);
        fs::path logical_abs = root / logical_rel;

        if (fs::is_regular_file(logical_abs, ec)) {
            std::string existing_err;
            std::string existing_sha = Sha256FileHex(logical_abs.u8string(), existing_err);
            if (existing_sha != sha) {
                std::string stem = fs::u8path(safe_name).stem().u8string();
                std::string ext = fs::u8path(safe_name).extension().u8string();
                std::string sha8 = sha.substr(0, 8);
                fs::path cand_rel = logical_dir / fs::u8path(stem + "_" + sha8 + ext);
                int idx = 2;
                while (fs::is_regular_file(root / cand_rel, ec)) {
                    std::string e2;
                    if (Sha256FileHex((root / cand_rel).u8string(), e2) == sha) break;
                    cand_rel = logical_dir / fs::u8path(
                        stem + "_" + sha8 + "_" + std::to_string(idx++) + ext);
                }
                logical_rel = cand_rel;
                logical_abs = root / logical_rel;
            }
        }

        if (!fs::is_regular_file(logical_abs, ec)) {
            fs::create_directories(logical_abs.parent_path(), ec);
            ec.clear();
            fs::create_hard_link(blob_abs, logical_abs, ec);
            if (ec) {
                ec.clear();
                fs::copy_file(blob_abs, logical_abs, fs::copy_options::overwrite_existing, ec);
            }
        }
        logical_rel_str = logical_rel.generic_u8string();
    }

    res.outcome = deduplicated ? DownloadOutcome::Deduplicated : DownloadOutcome::Downloaded;
    res.bytes_written = final_bytes;
    res.sha256 = sha;
    res.blob_rel_path = blob_rel.generic_u8string();
    res.logical_rel_path = logical_rel_str;
    res.deduplicated = deduplicated;
    res.magic_ok = true;

    file.status = FileStatus::Downloaded;
    file.bytes_written = final_bytes;
    file.sha256 = sha;
    file.blob_rel_path = res.blob_rel_path;
    file.logical_rel_path = res.logical_rel_path;
    file.deduplicated = deduplicated;
    file.magic_ok = true;
    file.error_message.clear();
    return res;
}

HeadlessCrawler::HeadlessCrawler(PolicyEnforcingTransport& transport,
                                 AuthClient& auth,
                                 CrawlerOptions opts,
                                 SanitizeOptions sanitize_opts)
    : transport_(transport),
      auth_(auth),
      opts_(std::move(opts)),
      sanitize_opts_(std::move(sanitize_opts)),
      sanitizer_(sanitize_opts_) {
    while (!opts_.base_url.empty() && opts_.base_url.back() == '/') {
        opts_.base_url.pop_back();
    }
}

std::vector<std::string> HeadlessCrawler::DefaultSeeds(std::string_view base_url) {
    std::string b(base_url);
    while (!b.empty() && b.back() == '/') b.pop_back();
    return {
        b + "/dotlrn/",
        b + "/dotlrn/index",
        b + "/dotlrn/control-panel",
        b + "/dotlrn/calendar/view",
        b + "/dotlrn/community-member",
        b + "/dotlrn/file-storage/",
        b + "/dotlrn/classes/",
        b + "/dotlrn/classes/one-community-type",
        b + "/dotlrn/catedras.tec/",
        b + "/dotlrn/catedras.tec/one-community-type",
        b + "/dotlrn/site-map",
        b + "/dotlrn/my-communities",
        b + "/tda-expediente-estudiantil/",
        b + "/tda-prematricula/",
        b + "/tda-gestor-inscripcion/",
        b + "/robots.txt",
        b + "/sitemap.xml"
    };
}

namespace {

void AppendUtf8CodepointStorage(uint32_t cp, std::string& out) {
    if (cp == 0 || (cp >= 0xD800u && cp <= 0xDFFFu) || cp > 0x10FFFFu) return;
    if (cp <= 0x7Fu) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FFu) {
        out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else if (cp <= 0xFFFFu) {
        out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    } else {
        out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
    }
}

uint32_t LookupNamedHtmlEntity(std::string_view ent) {
    struct Entry {
        const char* name;
        uint32_t cp;
    };
    static constexpr Entry kEntities[] = {
        {"aacute", 0x00E1}, {"eacute", 0x00E9}, {"iacute", 0x00ED}, {"oacute", 0x00F3},
        {"uacute", 0x00FA}, {"uuml", 0x00FC},   {"ntilde", 0x00F1}, {"ccedil", 0x00E7},
        {"Aacute", 0x00C1}, {"Eacute", 0x00C9}, {"Iacute", 0x00CD}, {"Oacute", 0x00D3},
        {"Uacute", 0x00DA}, {"Uuml", 0x00DC},   {"Ntilde", 0x00D1}, {"Ccedil", 0x00C7},
        {"agrave", 0x00E0}, {"egrave", 0x00E8}, {"igrave", 0x00EC}, {"ograve", 0x00F2},
        {"ugrave", 0x00F9}, {"Agrave", 0x00C0}, {"Egrave", 0x00C8}, {"Igrave", 0x00CC},
        {"Ograve", 0x00D2}, {"Ugrave", 0x00D9}, {"acirc", 0x00E2},  {"ecirc", 0x00EA},
        {"icirc", 0x00EE},  {"ocirc", 0x00F4},  {"ucirc", 0x00FB},  {"Acirc", 0x00C2},
        {"Ecirc", 0x00CA},  {"Icirc", 0x00CE},  {"Ocirc", 0x00D4},  {"Ucirc", 0x00DB},
        {"iquest", 0x00BF}, {"iexcl", 0x00A1},  {"laquo", 0x00AB},  {"raquo", 0x00BB},
        {"deg", 0x00B0},    {"copy", 0x00A9},   {"reg", 0x00AE},    {"ndash", 0x2013},
        {"mdash", 0x2014},  {"lsquo", 0x2018},  {"rsquo", 0x2019},  {"ldquo", 0x201C},
        {"rdquo", 0x201D},  {"hellip", 0x2026}
    };
    for (const auto& e : kEntities) {
        if (ent == e.name) return e.cp;
    }
    return 0;
}

}  // namespace

std::string HeadlessCrawler::HtmlUnescape(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        if (sv[i] == '&') {
            size_t semi = sv.find(';', i + 1);
            if (semi != std::string_view::npos && semi - i <= 12) {
                std::string_view ent = sv.substr(i + 1, semi - i - 1);
                if (ent == "amp") { out.push_back('&'); i = semi; continue; }
                if (ent == "quot") { out.push_back('"'); i = semi; continue; }
                if (ent == "apos" || ent == "#39") { out.push_back('\''); i = semi; continue; }
                if (ent == "lt") { out.push_back('<'); i = semi; continue; }
                if (ent == "gt") { out.push_back('>'); i = semi; continue; }
                if (ent == "nbsp") { out.push_back(' '); i = semi; continue; }
                if (uint32_t named_cp = LookupNamedHtmlEntity(ent); named_cp != 0) {
                    AppendUtf8CodepointStorage(named_cp, out);
                    i = semi;
                    continue;
                }
                if (!ent.empty() && ent[0] == '#') {
                    uint32_t cp = 0;
                    bool ok = false;
                    if (ent.size() > 2 && (ent[1] == 'x' || ent[1] == 'X')) {
                        ok = true;
                        for (size_t k = 2; k < ent.size(); ++k) {
                            char c = ent[k];
                            cp <<= 4;
                            if (c >= '0' && c <= '9') cp |= static_cast<uint32_t>(c - '0');
                            else if (c >= 'a' && c <= 'f') cp |= static_cast<uint32_t>(10 + c - 'a');
                            else if (c >= 'A' && c <= 'F') cp |= static_cast<uint32_t>(10 + c - 'A');
                            else { ok = false; break; }
                        }
                    } else if (ent.size() > 1) {
                        ok = true;
                        for (size_t k = 1; k < ent.size(); ++k) {
                            if (ent[k] < '0' || ent[k] > '9') { ok = false; break; }
                            cp = cp * 10u + static_cast<uint32_t>(ent[k] - '0');
                        }
                    }
                    if (ok && cp > 0 && cp <= 0x10FFFFu &&
                        !(cp >= 0xD800u && cp <= 0xDFFFu)) {
                        AppendUtf8CodepointStorage(cp, out);
                        i = semi;
                        continue;
                    }
                }
            }
        }
        out.push_back(sv[i]);
    }
    return out;
}

bool HeadlessCrawler::ShouldSkipPrefix(std::string_view path) {
    std::string low = AsciiLower(path);
    static const char* kSkip[] = {
        "/resources/", "/theme-td/", "/tda-consentimiento",
        "/api/login/", "/verifier", "/middleware"
    };
    for (const char* p : kSkip) {
        if (low.rfind(p, 0) == 0) return true;
    }
    return false;
}

bool HeadlessCrawler::IsDownloadLink(std::string_view canonical_url) {
    ParsedUrl p = ParseHttpUrl(canonical_url);
    std::string path_low = p.valid ? AsciiLower(p.path) : AsciiLower(canonical_url);
    if (path_low.find("/file-storage/view/") != std::string::npos ||
        path_low.find("/file-storage/download") != std::string::npos) {
        return true;
    }
    static const std::unordered_set<std::string> kDlExts = {
        ".pdf", ".ppt", ".pptx", ".doc", ".docx", ".xls", ".xlsx",
        ".zip", ".rar", ".7z", ".tar", ".gz",
        ".png", ".jpg", ".jpeg", ".gif",
        ".mp4", ".mp3", ".mov", ".avi",
        ".csv", ".txt", ".rtf", ".odt", ".ods", ".odp",
        ".py", ".ipynb", ".java", ".c", ".cpp", ".m", ".nb", ".dvi", ".tex"
    };
    return kDlExts.count(LowerExtension(path_low)) > 0;
}

Result<CrawlSummary> HeadlessCrawler::Run(const std::string& offline_root,
                                          StorageDownloader* optional_downloader) {
    CrawlSummary summary;

    struct FrontierItem {
        int priority = 5;
        uint64_t seq = 0;
        int depth = 0;
        std::string url;
    };
    struct FrontierCmp {
        bool operator()(const FrontierItem& a, const FrontierItem& b) const {
            if (a.priority != b.priority) return a.priority > b.priority;
            return a.seq > b.seq;
        }
    };

    std::priority_queue<FrontierItem, std::vector<FrontierItem>, FrontierCmp> pq;
    std::unordered_set<std::string> seen_urls;
    std::unordered_set<std::string> capped_urls;
    std::unordered_set<std::string> seen_file_ids;
    std::unordered_set<std::string> seen_folder_ids;
    std::unordered_set<std::string> seen_file_api_ids;
    std::map<std::string, int> calendar_date_counts;
    std::map<std::string, int> one_comm_date_counts;
    int cal_item_new_count = 0;
    int member_profile_count = 0;
    uint64_t next_seq = 0;

    auto try_enqueue = [&](std::string_view raw_url,
                           std::string_view cur_url,
                           int depth,
                           std::string_view link_label = "") -> bool {
        std::string unescaped = TrimAscii(HtmlUnescape(raw_url));
        if (unescaped.empty() || unescaped.front() == '#') return false;
        std::string low = AsciiLower(unescaped);
        if (low.rfind("javascript:", 0) == 0 || low.rfind("mailto:", 0) == 0 ||
            low.rfind("data:", 0) == 0 || low.rfind("tel:", 0) == 0) {
            return false;
        }

        auto canon = UrlCanonicalizer::Canonicalize(unescaped, cur_url);
        if (!canon.ok) return false;
        const std::string& cu = canon.value;

        PolicyRequestContext pctx;
        pctx.url = cu;
        pctx.method = "GET";
        pctx.resource_type = "document";
        pctx.link_label = std::string(link_label);
        if (!transport_.Policy().Evaluate(pctx).allowed) {
            ++summary.pages_skipped_policy;
            return false;
        }

        ParsedUrl pu = ParseHttpUrl(cu);
        if (!pu.valid || ShouldSkipPrefix(pu.path)) {
            ++summary.pages_skipped_policy;
            return false;
        }

        if (IsDownloadLink(cu)) {
            std::string oid = UrlCanonicalizer::UrlObjectId(cu);
            if (seen_file_ids.insert(oid).second) {
                FileRecord fr;
                fr.object_id = oid;
                fr.download_url = cu;
                fr.candidate_urls = {cu};
                fr.community = DeriveCommunitySlug(cu);
                fr.community_type = (AsciiLower(cu).find("catedras.tec") != std::string::npos)
                                        ? "CATEDRA"
                                        : "CLASE";
                size_t slash = pu.path.rfind('/');
                fr.name = (slash != std::string::npos && slash + 1 < pu.path.size())
                              ? pu.path.substr(slash + 1)
                              : oid;
                summary.files.push_back(std::move(fr));
            }
            return false;
        }

        if (depth > opts_.max_depth) {
            return false;
        }

        // Deduplicate BEFORE checking/incrementing sampling caps so repeated links
        // (or kAnchorRe + kBareHrefRe) never double-count or exhaust caps.
        if (seen_urls.count(cu) > 0) {
            return false;
        }

        // Sampling caps
        if (pu.path.find("/calendar/view") != std::string::npos &&
            pu.query.find("date=") != std::string::npos) {
            int& cnt = calendar_date_counts[pu.path];
            if (cnt >= opts_.calendar_date_cap) {
                if (capped_urls.insert(cu).second) ++summary.pages_skipped_cap;
                return false;
            }
            ++cnt;
        }
        if (pu.path.find("one-community") != std::string::npos &&
            pu.query.find("date=") != std::string::npos) {
            int& cnt = one_comm_date_counts[pu.path];
            if (cnt >= opts_.one_comm_date_cap) {
                if (capped_urls.insert(cu).second) ++summary.pages_skipped_cap;
                return false;
            }
            ++cnt;
        }
        if (cu.find("cal-item-new") != std::string::npos) {
            if (cal_item_new_count >= opts_.cal_item_new_cap) {
                if (capped_urls.insert(cu).second) ++summary.pages_skipped_cap;
                return false;
            }
            ++cal_item_new_count;
        }
        if (cu.find("community-member?user_id=") != std::string::npos) {
            if (member_profile_count >= opts_.member_profile_cap) {
                if (capped_urls.insert(cu).second) ++summary.pages_skipped_cap;
                return false;
            }
            ++member_profile_count;
        }

        seen_urls.insert(cu);
        pq.push({PriorityForUrl(cu, depth), next_seq++, depth, cu});
        return true;
    };

    const auto seeds = opts_.seed_urls.empty() ? DefaultSeeds(opts_.base_url) : opts_.seed_urls;
    for (const auto& s : seeds) {
        try_enqueue(s, opts_.base_url + "/", 0);
    }
    for (const auto& fid : opts_.seed_folder_ids) {
        if (seen_folder_ids.insert(fid).second) {
            summary.discovered_folder_ids.push_back(fid);
        }
        try_enqueue(opts_.base_url + "/dotlrn/file-storage/api/folder/" + fid,
                    opts_.base_url + "/", 1);
    }

    int consec_5xx = 0;
    int idle_rounds = 0;

    while (!pq.empty()) {
        if (opts_.max_pages > 0 && summary.pages_visited >= opts_.max_pages) {
            break;
        }

        FrontierItem cur = pq.top();
        pq.pop();

        if (opts_.delay_ms > 0) {
            SleepMs(opts_.delay_ms);
        }

        HttpRequest req;
        req.method = "GET";
        req.url = cur.url;
        req.follow_redirects = true;
        req.max_redirects = 5;
        req.headers["Accept"] = "text/html,application/xhtml+xml,application/json,*/*";

        HttpResponse resp = detail::ExecuteSessionRequest(transport_, auth_, std::move(req));
        auto val = auth_.ValidateResponse(resp, cur.url, true);
        if (val == ResponseValidity::SessionExpired) {
            summary.session_expired = true;
            summary.warnings.push_back("session expired during crawl");
            break;
        }

        if (!resp.transport_ok || resp.status_code >= 500) {
            ++summary.pages_failed;
            ++consec_5xx;
            if (opts_.consec_5xx_break > 0 && consec_5xx >= opts_.consec_5xx_break) {
                summary.circuit_broken = true;
                summary.warnings.push_back("5xx circuit breaker tripped");
                break;
            }
            continue;
        }
        consec_5xx = 0;

        if (resp.status_code != 200) {
            ++summary.pages_failed;
            continue;
        }

        ++summary.pages_visited;
        const std::string body = resp.BodyAsString();
        const std::string ct_low = AsciiLower(resp.content_type);
        const std::string final_u = resp.final_url.empty() ? cur.url : resp.final_url;

        size_t new_links_this_page = 0;
        size_t forms_this_page = 0;
        std::string page_title;

        // JSON file-storage API response
        if (ct_low.find("json") != std::string::npos ||
            final_u.find("/dotlrn/file-storage/api/folder/") != std::string::npos) {
            auto fwalk = FileStorageWalker::ParseFolderJson(body);
            if (fwalk.ok) {
                std::string comm = DeriveCommunitySlug(final_u);
                for (auto f : fwalk.value.folders) {
                    if (f.folder_id.empty()) continue;
                    if (seen_folder_ids.insert(f.folder_id).second) {
                        summary.discovered_folder_ids.push_back(f.folder_id);
                    }
                    f.community = comm;
                    if (summary.folders.size() < opts_.max_folders) {
                        if (try_enqueue("/dotlrn/file-storage/api/folder/" + f.folder_id,
                                        final_u, cur.depth + 1)) {
                            ++new_links_this_page;
                        }
                        summary.folders.push_back(std::move(f));
                    }
                }
                for (auto fr : fwalk.value.files) {
                    if (seen_file_ids.insert(fr.object_id).second) {
                        fr.community = comm;
                        fr.candidate_urls = StorageDownloader::BuildCandidateUrls(fr, opts_.base_url);
                        if (!fr.candidate_urls.empty()) fr.download_url = fr.candidate_urls.front();
                        summary.files.push_back(std::move(fr));
                        ++new_links_this_page;
                    }
                }
                for (auto lnk : fwalk.value.url_links) {
                    if (try_enqueue(lnk.url, final_u, cur.depth + 1, lnk.title)) {
                        ++new_links_this_page;
                    }
                    summary.url_links.push_back(std::move(lnk));
                }
            }
        } else {
            // HTML/text page parsing
            // 1. <title>
            static const std::regex kTitleRe(
                R"(<title[^>]*>([\s\S]*?)</title>)",
                std::regex::ECMAScript | std::regex::icase);
            std::smatch tm;
            if (std::regex_search(body, tm, kTitleRe)) {
                page_title = sanitizer_.SanitizeText(StripTagsAndTrim(tm.str(1)));
            }

            // 2. <a href> and <area href>
            static const std::regex kAnchorRe(
                R"(<(?:a|area)\b([^>]*)>([\s\S]*?)</(?:a|area)>)",
                std::regex::ECMAScript | std::regex::icase);
            std::vector<std::pair<size_t, size_t>> anchor_spans;
            {
                std::sregex_iterator it(body.begin(), body.end(), kAnchorRe);
                std::sregex_iterator end;
                for (; it != end; ++it) {
                    size_t m_start = static_cast<size_t>((*it).position());
                    size_t m_end = m_start + static_cast<size_t>((*it).length());
                    anchor_spans.emplace_back(m_start, m_end);
                    std::string href;
                    if (ExtractAttrCI((*it)[1].str(), "href", href)) {
                        std::string label = StripTagsAndTrim((*it)[2].str());
                        if (try_enqueue(href, final_u, cur.depth + 1, label)) {
                            ++new_links_this_page;
                        }
                    }
                }
            }
            // Also standalone <a ... href=...> without closing tag
            static const std::regex kBareHrefRe(
                R"re(<(?:a|area)\b[^>]*\bhref\s*=\s*(?:"([^"]*)"|'([^']*)'))re",
                std::regex::ECMAScript | std::regex::icase);
            {
                std::sregex_iterator it(body.begin(), body.end(), kBareHrefRe);
                std::sregex_iterator end;
                size_t span_idx = 0;
                for (; it != end; ++it) {
                    size_t m_pos = static_cast<size_t>((*it).position());
                    while (span_idx < anchor_spans.size() && anchor_spans[span_idx].second <= m_pos) {
                        ++span_idx;
                    }
                    if (span_idx < anchor_spans.size() &&
                        m_pos >= anchor_spans[span_idx].first &&
                        m_pos < anchor_spans[span_idx].second) {
                        continue;
                    }
                    std::string href = (*it)[1].matched ? (*it)[1].str() : (*it)[2].str();
                    if (try_enqueue(href, final_u, cur.depth + 1)) {
                        ++new_links_this_page;
                    }
                }
            }

            // 3. <form> metadata (NEVER submit, NEVER extract values)
            static const std::regex kFormRe(
                R"(<form\b([^>]*)>([\s\S]*?)</form>)",
                std::regex::ECMAScript | std::regex::icase);
            static const std::regex kFieldRe(
                R"(<(input|select|textarea|button)\b([^>]*)>)",
                std::regex::ECMAScript | std::regex::icase);
            {
                std::sregex_iterator it(body.begin(), body.end(), kFormRe);
                std::sregex_iterator end;
                for (; it != end; ++it) {
                    DiscoveredFormMeta fm;
                    fm.page_url = sanitizer_.SanitizeUrl(final_u);
                    std::string attrs = (*it)[1].str();
                    std::string action;
                    if (ExtractAttrCI(attrs, "action", action)) {
                        auto c_act = UrlCanonicalizer::Canonicalize(HtmlUnescape(action), final_u);
                        fm.action = c_act.ok ? sanitizer_.SanitizeUrl(c_act.value)
                                             : sanitizer_.SanitizeUrl(action);
                    }
                    std::string method;
                    if (ExtractAttrCI(attrs, "method", method)) {
                        fm.method = AsciiLower(method);
                    }
                    ExtractAttrCI(attrs, "enctype", fm.enctype);

                    std::string fbody = (*it)[2].str();
                    std::sregex_iterator fit(fbody.begin(), fbody.end(), kFieldRe);
                    for (; fit != end; ++fit) {
                        FormFieldMeta fld;
                        fld.tag = AsciiLower((*fit)[1].str());
                        std::string fattrs = (*fit)[2].str();
                        ExtractAttrCI(fattrs, "name", fld.name);
                        ExtractAttrCI(fattrs, "type", fld.type);
                        fld.type = AsciiLower(fld.type);
                        fld.required = HasAttrCI(fattrs, "required");
                        fld.disabled = HasAttrCI(fattrs, "disabled");
                        fld.hidden = (fld.type == "hidden") || HasAttrCI(fattrs, "hidden");
                        ExtractAttrCI(fattrs, "autocomplete", fld.autocomplete);
                        fm.fields.push_back(std::move(fld));
                    }
                    summary.forms.push_back(std::move(fm));
                    ++forms_this_page;
                }
            }

            // 4. Embedded API folder/file IDs and SPA folder IDs
            static const std::regex kApiRefRe(
                R"(/dotlrn/file-storage/api/(folder|file)/(\d+))",
                std::regex::ECMAScript | std::regex::icase);
            {
                std::sregex_iterator it(body.begin(), body.end(), kApiRefRe);
                std::sregex_iterator end;
                for (; it != end; ++it) {
                    std::string kind = AsciiLower((*it)[1].str());
                    std::string id = (*it)[2].str();
                    if (kind == "folder") {
                        if (seen_folder_ids.insert(id).second) {
                            summary.discovered_folder_ids.push_back(id);
                        }
                        if (try_enqueue("/dotlrn/file-storage/api/folder/" + id, final_u,
                                        cur.depth + 1)) {
                            ++new_links_this_page;
                        }
                    } else if (kind == "file") {
                        if (seen_file_api_ids.insert(id).second) {
                            summary.discovered_file_api_ids.push_back(id);
                        }
                    }
                }
            }

            for (const auto& fid : FileStorageWalker::ExtractRootFolderIds(body)) {
                if (seen_folder_ids.insert(fid).second) {
                    summary.discovered_folder_ids.push_back(fid);
                    if (try_enqueue("/dotlrn/file-storage/api/folder/" + fid, final_u,
                                    cur.depth + 1)) {
                        ++new_links_this_page;
                    }
                }
            }
        }

        CrawledPageSummary ps;
        ps.url = sanitizer_.SanitizeUrl(final_u);
        ps.page_key = UrlCanonicalizer::PageKeySha256(final_u);
        ps.depth = cur.depth;
        ps.http_status = resp.status_code;
        ps.content_type = resp.content_type;
        ps.title = std::move(page_title);
        ps.links_found = new_links_this_page;
        ps.forms_found = forms_this_page;
        summary.pages.push_back(std::move(ps));

        if (new_links_this_page == 0) {
            ++idle_rounds;
            if (opts_.saturation_rounds > 0 && idle_rounds >= opts_.saturation_rounds &&
                pq.empty()) {
                summary.saturated = true;
                break;
            }
        } else {
            idle_rounds = 0;
        }
    }

    if (opts_.download_discovered_files && optional_downloader != nullptr && !offline_root.empty()) {
        for (auto& fr : summary.files) {
            if (summary.session_expired) break;
            auto dr = optional_downloader->DownloadFile(fr, offline_root);
            if (dr.outcome == DownloadOutcome::SessionExpired) {
                summary.session_expired = true;
                break;
            }
        }
    }

    if (!offline_root.empty()) {
        MirrorManifestStore store(sanitize_opts_);
        store.Save(offline_root, summary.files, summary.folders, summary.forms, summary.url_links);
    }

    return Result<CrawlSummary>::Success(std::move(summary));
}

MirrorManifestStore::MirrorManifestStore(SanitizeOptions sanitize_opts)
    : sanitizer_(std::move(sanitize_opts)) {}

VoidResult MirrorManifestStore::Save(const std::string& offline_root,
                                     const std::vector<FileRecord>& files,
                                     const std::vector<FolderRecord>& folders,
                                     const std::vector<DiscoveredFormMeta>& forms,
                                     const std::vector<FolderUrlLink>& url_links) const {
    if (offline_root.empty()) {
        return VoidResult::Failure("empty_root", "offline_root is empty");
    }
    const fs::path root = fs::u8path(offline_root);

    nlohmann::json files_j = nlohmann::json::array();
    size_t downloaded_count = 0;
    size_t deduplicated_count = 0;
    for (const auto& f : files) {
        if (f.status == FileStatus::Downloaded) ++downloaded_count;
        if (f.deduplicated) ++deduplicated_count;
        files_j.push_back({
            {"object_id", f.object_id},
            {"parent_folder_id", f.parent_folder_id},
            {"community", sanitizer_.SanitizeText(f.community)},
            {"community_type", f.community_type},
            {"folder_path", sanitizer_.SanitizeText(f.folder_path)},
            {"name", sanitizer_.SanitizeText(f.name)},
            {"title", sanitizer_.SanitizeText(f.title)},
            {"mime_type", f.mime_type},
            {"content_size", f.content_size},
            {"download_url", sanitizer_.SanitizeUrl(f.download_url)},
            {"status", FileStatusToString(f.status)},
            {"sha256", f.sha256},
            {"blob_rel_path", f.blob_rel_path},
            {"logical_rel_path", sanitizer_.SanitizeText(f.logical_rel_path)},
            {"quarantine_rel_path", sanitizer_.SanitizeText(f.quarantine_rel_path)},
            {"bytes_written", f.bytes_written},
            {"deduplicated", f.deduplicated},
            {"magic_ok", f.magic_ok},
            {"detected_magic", f.detected_magic},
            {"http_status", f.http_status},
            {"error_message", sanitizer_.SanitizeText(f.error_message)}
        });
    }
    auto r1 = WriteFileAtomic(root / "files_manifest.json", files_j.dump(2));
    if (!r1.ok) return r1;

    nlohmann::json folders_j = nlohmann::json::array();
    for (const auto& fd : folders) {
        folders_j.push_back({
            {"folder_id", fd.folder_id},
            {"parent_id", fd.parent_id},
            {"name", sanitizer_.SanitizeText(fd.name)},
            {"community", sanitizer_.SanitizeText(fd.community)},
            {"community_type", fd.community_type},
            {"folder_path", sanitizer_.SanitizeText(fd.folder_path)},
            {"depth", fd.depth},
            {"child_count", fd.child_count}
        });
    }
    auto r2 = WriteFileAtomic(root / "folders_manifest.json", folders_j.dump(2));
    if (!r2.ok) return r2;

    nlohmann::json forms_j = nlohmann::json::array();
    for (const auto& fm : forms) {
        nlohmann::json flds = nlohmann::json::array();
        for (const auto& fl : fm.fields) {
            flds.push_back({
                {"tag", fl.tag},
                {"name", fl.name},
                {"type", fl.type},
                {"required", fl.required},
                {"disabled", fl.disabled},
                {"hidden", fl.hidden},
                {"autocomplete", fl.autocomplete}
            });
        }
        forms_j.push_back({
            {"page_url", sanitizer_.SanitizeUrl(fm.page_url)},
            {"action", sanitizer_.SanitizeUrl(fm.action)},
            {"method", fm.method},
            {"enctype", fm.enctype},
            {"fields", std::move(flds)}
        });
    }
    auto r3 = WriteFileAtomic(root / "forms_manifest.json", forms_j.dump(2));
    if (!r3.ok) return r3;

    nlohmann::json links_j = nlohmann::json::array();
    for (const auto& lnk : url_links) {
        links_j.push_back({
            {"object_id", lnk.object_id},
            {"parent_folder_id", lnk.parent_folder_id},
            {"title", sanitizer_.SanitizeText(lnk.title)},
            {"url", sanitizer_.SanitizeUrl(lnk.url)},
            {"community", sanitizer_.SanitizeText(lnk.community)},
            {"folder_path", sanitizer_.SanitizeText(lnk.folder_path)}
        });
    }

    nlohmann::json summary_j = {
        {"generated_at_utc", CredentialVault::CurrentUtcIso8601()},
        {"files_total", files.size()},
        {"files_downloaded", downloaded_count},
        {"files_deduplicated", deduplicated_count},
        {"folders_total", folders.size()},
        {"forms_total", forms.size()},
        {"url_links_total", url_links.size()},
        {"url_links", std::move(links_j)}
    };
    return WriteFileAtomic(root / "manifest.json", summary_j.dump(2));
}

Result<std::vector<FileRecord>> MirrorManifestStore::LoadFilesManifest(
    const std::string& offline_root) const {
    const fs::path mp = fs::u8path(offline_root) / "files_manifest.json";
    if (!fs::is_regular_file(mp)) {
        return Result<std::vector<FileRecord>>::Failure("not_found", "files_manifest.json missing");
    }
    std::ifstream ifs(mp, std::ios::binary);
    if (!ifs.is_open()) {
        return Result<std::vector<FileRecord>>::Failure("open_failed", "cannot open files_manifest.json");
    }
    try {
        nlohmann::json j;
        ifs >> j;
        if (!j.is_array()) {
            return Result<std::vector<FileRecord>>::Failure("invalid_json", "manifest not array");
        }
        std::vector<FileRecord> out;
        for (const auto& item : j) {
            if (!item.is_object()) continue;
            FileRecord fr;
            fr.object_id = item.value("object_id", "");
            fr.parent_folder_id = item.value("parent_folder_id", "");
            fr.community = item.value("community", "");
            fr.community_type = item.value("community_type", "CLASE");
            fr.folder_path = item.value("folder_path", "");
            fr.name = item.value("name", "");
            fr.title = item.value("title", "");
            fr.mime_type = item.value("mime_type", "");
            fr.content_size = item.value("content_size", uint64_t{0});
            fr.download_url = item.value("download_url", "");
            fr.status = FileStatusFromString(item.value("status", "pending"));
            fr.sha256 = item.value("sha256", "");
            fr.blob_rel_path = item.value("blob_rel_path", "");
            fr.logical_rel_path = item.value("logical_rel_path", "");
            fr.quarantine_rel_path = item.value("quarantine_rel_path", "");
            fr.bytes_written = item.value("bytes_written", uint64_t{0});
            fr.deduplicated = item.value("deduplicated", false);
            fr.magic_ok = item.value("magic_ok", false);
            fr.detected_magic = item.value("detected_magic", "");
            fr.http_status = item.value("http_status", 0);
            fr.error_message = item.value("error_message", "");
            out.push_back(std::move(fr));
        }
        return Result<std::vector<FileRecord>>::Success(std::move(out));
    } catch (const std::exception& ex) {
        return Result<std::vector<FileRecord>>::Failure("invalid_json", ex.what());
    }
}

}  // namespace academic::tec_digital
