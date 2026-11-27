#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/canon.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "academic/tec_digital/http_transport.hpp"
#include "academic/tec_digital/material_indexer.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/service.hpp"
#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/tda_tds_client.hpp"
#include "academic/tec_digital/vault.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using namespace academic::tec_digital;

namespace {

bool IsLoopbackHostLocal(std::string_view host) {
    std::string h(host);
    std::transform(h.begin(), h.end(), h.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return h == "127.0.0.1" || h == "localhost" || h == "::1" || h == "[::1]";
}

std::string ExtractOrigin(std::string_view url) {
    auto scheme_pos = url.find("://");
    if (scheme_pos == std::string_view::npos) return {};
    auto host_start = scheme_pos + 3;
    auto slash = url.find('/', host_start);
    if (slash == std::string_view::npos) return std::string(url);
    return std::string(url.substr(0, slash));
}

std::string ExtractHostOnly(std::string_view url) {
    std::string origin = ExtractOrigin(url);
    auto scheme_pos = origin.find("://");
    if (scheme_pos == std::string::npos) return {};
    std::string authority = origin.substr(scheme_pos + 3);
    if (!authority.empty() && authority.front() == '[') {
        auto end_b = authority.find(']');
        if (end_b != std::string::npos) return authority.substr(1, end_b - 1);
    }
    auto colon = authority.rfind(':');
    if (colon != std::string::npos) return authority.substr(0, colon);
    return authority;
}

bool IsLoopbackUrl(std::string_view url) {
    return IsLoopbackHostLocal(ExtractHostOnly(url));
}

std::string AuthStatusName(AuthStatus st) {
    switch (st) {
        case AuthStatus::Ok: return "ok";
        case AuthStatus::NoAccount: return "no_account";
        case AuthStatus::BadPassword: return "bad_password";
        case AuthStatus::LoginExpired:
        case AuthStatus::PortalVerificationFailed: return "session_expired";
        case AuthStatus::PolicyBlocked: return "blocked_by_policy";
        case AuthStatus::TransportError: return "network_offline";
        case AuthStatus::EmptyCredentials: return "credentials_missing";
        default: return AuthStatusToString(st);
    }
}

bool ReadFileToString(const fs::path& p, std::string& out, std::string* err = nullptr) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) {
        if (err) *err = "failed to open file: " + p.u8string();
        return false;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();
    out = oss.str();
    return true;
}

bool WriteStringToFile(const fs::path& p, const std::string& content, std::string* err = nullptr) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        if (err) *err = "failed to open output: " + p.u8string();
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    ofs.flush();
    if (!ofs.good()) {
        if (err) *err = "failed writing output: " + p.u8string();
        return false;
    }
    return true;
}

std::string ResolveBaseUrl(const std::string& cli_base_url) {
    if (!cli_base_url.empty()) return cli_base_url;
#ifdef _WIN32
    char* env_buf = nullptr;
    size_t env_len = 0;
    if (_dupenv_s(&env_buf, &env_len, "TEC_BASE_URL") == 0 && env_buf != nullptr) {
        std::string val(env_buf);
        std::free(env_buf);
        if (!val.empty()) return val;
    }
#else
    if (const char* env_base = std::getenv("TEC_BASE_URL")) {
        if (*env_base) return std::string(env_base);
    }
#endif
    return "https://tecdigital.tec.ac.cr";
}

ServiceConfig MakeServiceConfig(const std::string& base_url,
                                const std::string& offline_root = "",
                                const std::string& profile_dir = "") {
    ServiceConfig cfg;
    cfg.base_url = base_url;
    cfg.auth_options.base_url = base_url;
    cfg.offline_root = offline_root;
    cfg.crawler_options.base_url = base_url;

    const bool loopback = IsLoopbackUrl(base_url);
    if (loopback) {
        std::string origin = ExtractOrigin(base_url);
        if (!origin.empty()) {
            cfg.policy_options.allowed_test_origins.push_back(origin);
        }
        cfg.downloader_options.min_delay_ms = 0;
        cfg.downloader_options.backoff_ms = {30, 60, 120};
        cfg.downloader_options.max_retries = 3;
        cfg.downloader_options.verify_size_with_head = false;
        cfg.crawler_options.delay_ms = 0;
        cfg.crawler_options.max_pages = 30;
        cfg.crawler_options.max_depth = 3;
    }

    if (!profile_dir.empty()) {
        fs::path prof = fs::u8path(profile_dir);
        cfg.vault_config.mode = VaultPersistenceMode::EncryptedVault;
        cfg.vault_config.vault_path = (prof / "session.dpapi").u8string();
        cfg.vault_config.session_meta_path = (prof / "session_meta.json").u8string();
        cfg.vault_config.persist_session_cookies = false;
    }

    if (auto env_creds = CredentialVault::ResolveFromEnv()) {
        cfg.sanitize_options.runtime_email = env_creds->email;
        cfg.sanitize_options.runtime_password = env_creds->password;
        CredentialVault::ClearMemory(*env_creds);
    }

    return cfg;
}

std::string GetArgVal(const std::vector<std::string>& args, const std::string& key,
                      const std::string& def = "") {
    for (size_t i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == key) return args[i + 1];
    }
    return def;
}

bool HasFlag(const std::vector<std::string>& args, const std::string& key) {
    for (const auto& a : args) {
        if (a == key) return true;
    }
    return false;
}

void PrintSanitizedJson(const nlohmann::json& j, const Sanitizer& san) {
    std::cout << san.SanitizeJsonString(j.dump(2)) << "\n";
}

int CmdLogin(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string profile = GetArgVal(args, "--profile");
    const bool check_session = HasFlag(args, "--check-session");

    ServiceConfig cfg = MakeServiceConfig(base_url, "", profile);
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();

    if (!auth.ok) {
        nlohmann::json out = {
            {"ok", false},
            {"status", AuthStatusName(auth.status)},
            {"sessionActive", false},
            {"httpStatus", auth.http_status},
            {"error", san.SanitizeText(auth.message)}
        };
        PrintSanitizedJson(out, san);
        return 2;
    }

    bool active = true;
    if (check_session) {
        active = svc.CheckSession(true);
    }
    nlohmann::json out = {
        {"ok", active},
        {"status", active ? "ok" : "session_expired"},
        {"sessionActive", active},
        {"httpStatus", auth.http_status}
    };
    PrintSanitizedJson(out, san);
    return active ? 0 : 3;
}

int CmdVaultSave(const std::vector<std::string>& args) {
    const std::string profile = GetArgVal(args, "--profile");
    const std::string base_url = GetArgVal(args, "--base-url");
    if (profile.empty()) {
        std::cerr << "vault-save requires --profile <dir>\n";
        return 64;
    }
    std::string err;
    auto creds = CredentialVault::ResolveFromEnv(&err);
    if (!creds.has_value()) {
        std::cerr << "credentials_missing: " << err << "\n";
        return 2;
    }

    if (!base_url.empty()) {
        ServiceConfig cfg = MakeServiceConfig(base_url, "", profile);
        TecDigitalService svc(cfg);
        AuthResult auth = svc.LoginWithCredentials(*creds, false);
        if (!auth.ok) {
            CredentialVault::ClearMemory(*creds);
            std::cerr << AuthStatusName(auth.status) << ": "
                      << svc.SnapshotSanitizer().SanitizeText(auth.message) << "\n";
            return 2;
        }
    }

    fs::path prof = fs::u8path(profile);
    std::error_code ec;
    fs::create_directories(prof, ec);
    const std::string vault_path = (prof / "session.dpapi").u8string();
    VoidResult saved = CredentialVault::SaveEncrypted(vault_path, *creds, {}, false);
    CredentialVault::ClearMemory(*creds);
    if (!saved.ok) {
        std::cerr << "vault_save_failed: " << saved.error_message << "\n";
        return 2;
    }
    std::cout << nlohmann::json({{"ok", true}, {"vaultSaved", true}}).dump(2) << "\n";
    return 0;
}

int CmdVaultCheck(const std::vector<std::string>& args) {
    const std::string profile = GetArgVal(args, "--profile");
    const std::string base_url = GetArgVal(args, "--base-url");
    if (profile.empty()) {
        std::cerr << "vault-check requires --profile <dir>\n";
        return 64;
    }
    const std::string vault_path = (fs::u8path(profile) / "session.dpapi").u8string();
    auto loaded = CredentialVault::LoadEncrypted(vault_path);
    if (!loaded.ok) {
        std::cerr << "vault_invalid: " << loaded.error_code << "\n";
        return 2;
    }

    bool ok = !loaded.value.credentials.email.empty() && !loaded.value.credentials.password.empty();
    if (ok && !base_url.empty()) {
        ServiceConfig cfg = MakeServiceConfig(base_url, "", profile);
        TecDigitalService svc(cfg);
        AuthResult auth = svc.LoginWithCredentials(loaded.value.credentials, false);
        ok = auth.ok;
    }
    CredentialVault::ClearMemory(loaded.value);
    if (!ok) {
        std::cerr << "vault_auth_failed\n";
        return 2;
    }
    std::cout << nlohmann::json({{"ok", true}, {"vaultValid", true}}).dump(2) << "\n";
    return 0;
}

int CmdSync(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string folder_id = GetArgVal(args, "--folder-id");
    const std::string out_dir = GetArgVal(args, "--out");
    int max_depth = 10;
    const std::string depth_str = GetArgVal(args, "--max-depth");
    if (!depth_str.empty()) {
        try {
            max_depth = std::max(0, std::stoi(depth_str));
        } catch (...) {
            max_depth = 10;
        }
    }
    if (out_dir.empty()) {
        std::cerr << "sync requires --out <dir>\n";
        return 64;
    }

    ServiceConfig cfg = MakeServiceConfig(base_url, out_dir);
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok) {
        std::cerr << AuthStatusName(auth.status) << ": " << san.SanitizeText(auth.message) << "\n";
        return 3;
    }

    if (!svc.CheckSession(true)) {
        std::cerr << "session_expired: /dotlrn/ session is not active\n";
        return 3;
    }

    SyncFolderStorageOptions opts;
    if (!folder_id.empty()) opts.seed_folder_ids.push_back(folder_id);
    opts.max_depth = max_depth;
    opts.download_files = true;
    opts.build_material_index = true;

    auto res = svc.SyncFolderStorage(opts);
    if (!res.ok) {
        std::cerr << res.error_code << ": " << san.SanitizeText(res.error_message) << "\n";
        return 4;
    }
    if (res.value.session_expired) {
        std::cerr << "session_expired: mid-sync session invalidated\n";
        return 3;
    }

    nlohmann::json out = {
        {"ok", true},
        {"folders_discovered", res.value.folders_discovered},
        {"files_discovered", res.value.files_discovered},
        {"files_downloaded", res.value.files_downloaded},
        {"files_deduplicated", res.value.files_deduplicated},
        {"files_failed", res.value.files_failed},
        {"extracted_chars", res.value.index_stats.chars_extracted_total}
    };
    PrintSanitizedJson(out, san);
    return 0;
}

int CmdDownloadFile(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string obj_id = GetArgVal(args, "--object-id");
    const std::string out_dir = GetArgVal(args, "--out");
    const std::string mode = GetArgVal(args, "--mode");
    if (obj_id.empty() || out_dir.empty()) {
        std::cerr << "download-file requires --object-id <id> --out <dir>\n";
        return 64;
    }

    std::error_code ec;
    fs::create_directories(fs::u8path(out_dir), ec);

    ServiceConfig cfg = MakeServiceConfig(base_url, out_dir);
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok) {
        std::cerr << AuthStatusName(auth.status) << ": " << san.SanitizeText(auth.message) << "\n";
        return 3;
    }

    FileStorageWalker walker(svc.GuardedTransport(), svc.Auth(), base_url, svc.SnapshotSanitizeOptions());
    FileRecord rec;
    rec.object_id = obj_id;
    rec.name = "file_" + obj_id + ".pdf";
    rec.community = "direct";
    rec.folder_path = "direct";

    auto meta = walker.FetchFileDetails(obj_id);
    if (meta.ok) {
        if (!meta.value.name.empty()) rec.name = meta.value.name;
        rec.mime_type = meta.value.mime_type;
        if (mode.empty()) {
            rec.content_size = meta.value.content_size;
        }
        rec.live_revision = meta.value.live_revision;
    }

    std::string view_url = base_url + "/dotlrn/file-storage/view/" + obj_id;
    if (!mode.empty()) view_url += "?mode=" + mode;
    rec.download_url = view_url;
    rec.candidate_urls.clear();
    rec.candidate_urls.push_back(view_url);
    if (mode.empty()) {
        rec.candidate_urls.push_back(base_url + "/dotlrn/file-storage/download/" +
                                     StorageDownloader::SafeFilename(rec.name) + "?file_id=" + obj_id);
    }

    StorageDownloader dl(svc.GuardedTransport(), svc.Auth(), base_url,
                         cfg.downloader_options, svc.SnapshotSanitizeOptions());
    DownloadResult dr = dl.DownloadFile(rec, out_dir);

    if (dr.outcome == DownloadOutcome::Downloaded || dr.outcome == DownloadOutcome::Deduplicated) {
        fs::path blob_path = fs::u8path(out_dir) / fs::u8path(dr.blob_rel_path);
        fs::path dest_path = fs::u8path(out_dir) / fs::u8path(StorageDownloader::SafeFilename(rec.name));
        fs::copy_file(blob_path, dest_path, fs::copy_options::overwrite_existing, ec);
        fs::remove(fs::u8path(dest_path.u8string() + ".part"), ec);
        nlohmann::json out = {
            {"ok", true},
            {"object_id", obj_id},
            {"filename", StorageDownloader::SafeFilename(rec.name)},
            {"sha256", dr.sha256},
            {"bytes", dr.bytes_written}
        };
        PrintSanitizedJson(out, san);
        return 0;
    }

    if (dr.http_status == 403) {
        nlohmann::json out = {
            {"ok", false},
            {"error", "forbidden_file"},
            {"http_status", 403}
        };
        PrintSanitizedJson(out, san);
        return 5;
    }

    if (dr.outcome == DownloadOutcome::SessionExpired) {
        if (svc.CheckSession(true)) {
            nlohmann::json out = {
                {"ok", false},
                {"error", "download_quarantined_or_html"},
                {"quarantine", dr.quarantine_rel_path}
            };
            PrintSanitizedJson(out, san);
            return 6;
        }
        std::cerr << "session_expired\n";
        return 3;
    }

    nlohmann::json out = {
        {"ok", false},
        {"error", san.SanitizeText(dr.error_message)},
        {"http_status", dr.http_status},
        {"quarantine", dr.quarantine_rel_path}
    };
    PrintSanitizedJson(out, san);
    return 6;
}

int CmdIndexMaterials(const std::vector<std::string>& args) {
    const std::string mirror = GetArgVal(args, "--mirror");
    if (mirror.empty()) {
        std::cerr << "index-materials requires --mirror <dir>\n";
        return 64;
    }
    OfflineMaterialIndex idx(mirror);
    auto st = idx.IndexDirectory(mirror);
    if (!st.ok) {
        std::cerr << st.error_code << ": " << st.error_message << "\n";
        return 2;
    }
    auto sv = idx.SaveToDisk(mirror);
    if (!sv.ok) {
        std::cerr << sv.error_code << ": " << sv.error_message << "\n";
        return 2;
    }
    nlohmann::json out = {
        {"ok", true},
        {"total", st.value.total},
        {"missing_blobs", st.value.missing_blobs},
        {"extracted_chars", st.value.chars_extracted_total}
    };
    std::cout << out.dump(2) << "\n";
    return 0;
}

int CmdSearchMaterials(const std::vector<std::string>& args) {
    const std::string mirror = GetArgVal(args, "--mirror");
    const std::string query = GetArgVal(args, "--query");
    if (mirror.empty() || query.empty()) {
        std::cerr << "search-materials requires --mirror <dir> --query <q>\n";
        return 64;
    }
    OfflineMaterialIndex idx(mirror);
    auto ld = idx.LoadFromDisk(mirror);
    if (!ld.ok) {
        auto ix = idx.IndexDirectory(mirror);
        if (!ix.ok) {
            std::cerr << ix.error_code << ": " << ix.error_message << "\n";
            return 2;
        }
    }
    auto hits = idx.SearchOfflineMaterials(query);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& h : hits) {
        arr.push_back({
            {"object_id", h.document.object_id},
            {"name", h.document.name},
            {"score", h.score},
            {"snippet", h.snippet}
        });
    }
    std::cout << nlohmann::json({{"ok", true}, {"count", arr.size()}, {"hits", arr}}).dump(2) << "\n";
    return 0;
}

int CmdFetchCurriculum(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string out_file = GetArgVal(args, "--out");
    if (out_file.empty()) {
        std::cerr << "fetch-curriculum requires --out <file>\n";
        return 64;
    }
    ServiceConfig cfg = MakeServiceConfig(base_url);
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok) {
        std::cerr << AuthStatusName(auth.status) << ": " << san.SanitizeText(auth.message) << "\n";
        return 3;
    }

    auto sedes_r = svc.TdaClient().GetCurriculumSedes();
    if (!sedes_r.ok) {
        std::cerr << sedes_r.error_code << ": " << san.SanitizeText(sedes_r.error_message) << "\n";
        return 4;
    }

    nlohmann::json sedes_j = nlohmann::json::array();
    std::string first_sede;
    for (const auto& s : sedes_r.value) {
        if (first_sede.empty()) first_sede = s.key;
        sedes_j.push_back({{"id", s.key}, {"key", s.key}, {"nombre", s.name}, {"name", s.name}});
    }

    nlohmann::json carreras_j = nlohmann::json::array();
    std::string first_carrera;
    if (!first_sede.empty()) {
        auto car_r = svc.TdaClient().GetCurriculumCarreras(first_sede);
        if (car_r.ok) {
            for (const auto& c : car_r.value) {
                if (first_carrera.empty()) first_carrera = c.key;
                carreras_j.push_back({{"id", c.key}, {"key", c.key}, {"nombre", c.name}, {"name", c.name}});
            }
        }
    }

    nlohmann::json planes_j = nlohmann::json::array();
    if (!first_sede.empty() && !first_carrera.empty()) {
        auto pl_r = svc.TdaClient().GetCurriculumPlanes(first_sede, first_carrera);
        if (pl_r.ok) {
            for (const auto& p : pl_r.value) {
                planes_j.push_back({{"id", p.key}, {"key", p.key}, {"nombre", p.name}, {"name", p.name}});
            }
        }
    }

    nlohmann::json root = {
        {"sedes", sedes_j},
        {"carreras", carreras_j},
        {"planes", planes_j}
    };
    std::string err;
    if (!WriteStringToFile(fs::u8path(out_file), san.SanitizeJsonString(root.dump(2)), &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    std::cout << nlohmann::json({{"ok", true}, {"sedes", sedes_j.size()},
                                 {"carreras", carreras_j.size()}, {"planes", planes_j.size()}}).dump(2) << "\n";
    return 0;
}

int CmdFetchExpediente(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string out_file = GetArgVal(args, "--out");
    if (out_file.empty()) {
        std::cerr << "fetch-expediente requires --out <file>\n";
        return 64;
    }
    ServiceConfig cfg = MakeServiceConfig(base_url);
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok) {
        std::cerr << AuthStatusName(auth.status) << ": " << san.SanitizeText(auth.message) << "\n";
        return 3;
    }

    int unread = 0;
    if (auto ur = svc.TdaClient().GetUnreadNotificationCount(); ur.ok) {
        unread = ur.value;
    }
    nlohmann::json notifs_j = nlohmann::json::array();
    if (auto nr = svc.TdaClient().GetUserNotifications(); nr.ok) {
        if (unread == 0 && nr.value.unread_quantity > 0) unread = nr.value.unread_quantity;
        for (const auto& n : nr.value.notifications) {
            notifs_j.push_back({
                {"id", n.notification_id},
                {"title", san.SanitizeText(n.title)},
                {"text", san.SanitizeText(n.text)},
                {"state", n.state}
            });
        }
    }

    nlohmann::json notas_j = nlohmann::json::array();
    std::string promedio_txt;
    if (auto ex_r = svc.TdaClient().GetExpedienteSummary(); ex_r.ok) {
        promedio_txt = san.SanitizeText(ex_r.value.weighted_average_text);
        if (!ex_r.value.detalle_notas_html.empty()) {
            try {
                auto parsed = nlohmann::json::parse(ex_r.value.detalle_notas_html);
                if (parsed.is_array()) {
                    notas_j = parsed;
                } else if (parsed.is_object() && parsed.contains("notas") && parsed["notas"].is_array()) {
                    notas_j = parsed["notas"];
                }
            } catch (...) {
                notas_j.push_back({{"raw_summary", san.SanitizeHtml(ex_r.value.detalle_notas_html)}});
            }
        }
    }

    if (notas_j.empty()) {
        HttpRequest req;
        req.method = "GET";
        req.url = base_url + "/tda-expediente-estudiantil/ajax/detalle_notas";
        const std::string ck = svc.Auth().Cookies().BuildCookieHeader(req.url);
        if (!ck.empty()) req.headers["Cookie"] = ck;
        HttpResponse hr = svc.GuardedTransport().Execute(req);
        if (hr.transport_ok && hr.status_code == 200) {
            try {
                auto parsed = nlohmann::json::parse(hr.BodyAsString());
                if (parsed.is_array()) notas_j = parsed;
            } catch (...) {}
        }
    }
    if (unread == 0) {
        HttpRequest req;
        req.method = "GET";
        req.url = base_url + "/tda-notifications/ajax/has_unread_notifications";
        const std::string ck = svc.Auth().Cookies().BuildCookieHeader(req.url);
        if (!ck.empty()) req.headers["Cookie"] = ck;
        HttpResponse hr = svc.GuardedTransport().Execute(req);
        if (hr.transport_ok && hr.status_code == 200) {
            try {
                auto parsed = nlohmann::json::parse(hr.BodyAsString());
                if (parsed.is_object() && parsed.contains("unread")) {
                    unread = parsed["unread"].get<int>();
                }
            } catch (...) {}
        }
    }

    nlohmann::json root = {
        {"unread_notifications", unread},
        {"notas", notas_j},
        {"promedio_ponderado", promedio_txt},
        {"notifications", notifs_j}
    };
    std::string err;
    if (!WriteStringToFile(fs::u8path(out_file), san.SanitizeJsonString(root.dump(2)), &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    std::cout << nlohmann::json({{"ok", true}, {"unread_notifications", unread},
                                 {"notas_count", notas_j.size()}}).dump(2) << "\n";
    return 0;
}

int CmdHttpCrawl(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string out_dir = GetArgVal(args, "--out");
    if (out_dir.empty()) {
        std::cerr << "http-crawl requires --out <dir>\n";
        return 64;
    }
    ServiceConfig cfg = MakeServiceConfig(base_url, out_dir);
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok) {
        std::cerr << AuthStatusName(auth.status) << ": " << san.SanitizeText(auth.message) << "\n";
        return 3;
    }

    StorageDownloader dl(svc.GuardedTransport(), svc.Auth(), base_url,
                         cfg.downloader_options, svc.SnapshotSanitizeOptions());
    HeadlessCrawler crawler(svc.GuardedTransport(), svc.Auth(),
                            cfg.crawler_options, svc.SnapshotSanitizeOptions());
    auto crawl_r = crawler.Run(out_dir, &dl);
    if (!crawl_r.ok) {
        std::cerr << crawl_r.error_code << ": " << san.SanitizeText(crawl_r.error_message) << "\n";
        return 4;
    }

    MirrorManifestStore store(svc.SnapshotSanitizeOptions());
    (void)store.Save(out_dir, crawl_r.value.files, crawl_r.value.folders,
                     crawl_r.value.forms, crawl_r.value.url_links);

    OfflineMaterialIndex idx(out_dir, svc.SnapshotSanitizeOptions());
    (void)idx.IndexDirectory(out_dir);
    (void)idx.SaveToDisk(out_dir);

    nlohmann::json out = {
        {"ok", true},
        {"pages_visited", crawl_r.value.pages_visited},
        {"files_discovered", crawl_r.value.files.size()},
        {"folders_discovered", crawl_r.value.folders.size()}
    };
    PrintSanitizedJson(out, san);
    return 0;
}

int CmdVerifyOffline(const std::vector<std::string>& args) {
    const std::string mirror = GetArgVal(args, "--mirror");
    if (mirror.empty()) {
        std::cerr << "verify-offline requires --mirror <dir>\n";
        return 64;
    }
    fs::path root = fs::u8path(mirror);
    if (!fs::exists(root)) {
        std::cerr << "mirror_not_found: " << mirror << "\n";
        return 2;
    }

    size_t blobs_checked = 0;
    size_t corrupt_blobs = 0;
    fs::path blobs_dir = root / "downloads" / "_blobs";
    if (fs::exists(blobs_dir)) {
        for (const auto& entry : fs::recursive_directory_iterator(blobs_dir)) {
            if (!entry.is_regular_file()) continue;
            std::string raw;
            if (!ReadFileToString(entry.path(), raw)) {
                ++corrupt_blobs;
                continue;
            }
            const std::string actual = Sha256Hex(raw);
            const std::string expected = entry.path().filename().u8string();
            const std::string parent = entry.path().parent_path().filename().u8string();
            ++blobs_checked;
            if (actual != expected || actual.size() < 2 || parent != actual.substr(0, 2)) {
                ++corrupt_blobs;
            }
        }
    }

    SanitizeOptions sopts;
    if (auto env_creds = CredentialVault::ResolveFromEnv()) {
        sopts.runtime_email = env_creds->email;
        sopts.runtime_password = env_creds->password;
        CredentialVault::ClearMemory(*env_creds);
    }
    Sanitizer san(sopts);
    NeedleScanResult scan = san.ScanDirectoryForLeaks(mirror);

    const bool ok = (corrupt_blobs == 0) && scan.passed;
    nlohmann::json out = {
        {"ok", ok},
        {"blobs_checked", blobs_checked},
        {"corrupt_blobs", corrupt_blobs},
        {"leaks_passed", scan.passed}
    };
    std::cout << out.dump(2) << "\n";
    return ok ? 0 : 7;
}

int CmdPolicyRequest(const std::vector<std::string>& args) {
    const std::string method = GetArgVal(args, "--method", "GET");
    const std::string url = GetArgVal(args, "--url");
    const std::string body = GetArgVal(args, "--body");
    if (url.empty()) {
        std::cerr << "policy-request requires --url <url>\n";
        return 64;
    }
    std::string origin = ExtractOrigin(url);
    ServiceConfig cfg = MakeServiceConfig(origin.empty() ? ResolveBaseUrl("") : origin);
    TecDigitalService svc(cfg);

    HttpRequest req;
    req.method = method;
    req.url = url;
    req.body = body;
    if (!body.empty()) {
        req.headers["Content-Type"] = "application/json";
    }
    HttpResponse resp = svc.GuardedTransport().Execute(req);
    if (!resp.transport_ok) {
        std::cerr << "policy_or_transport_blocked: " << resp.error_message << "\n";
        return 10;
    }
    if (resp.status_code < 200 || resp.status_code >= 300) {
        std::cerr << "http_status_" << resp.status_code << "\n";
        return 11;
    }
    std::cout << svc.SnapshotSanitizer().SanitizeText(resp.BodyAsString()) << "\n";
    return 0;
}

int CmdCanonicalizeUrls(const std::vector<std::string>& args) {
    const std::string in_path = GetArgVal(args, "--in");
    const std::string out_path = GetArgVal(args, "--out");
    if (in_path.empty() || out_path.empty()) {
        std::cerr << "canonicalize-urls requires --in <json> --out <json>\n";
        return 64;
    }
    std::string raw, err;
    if (!ReadFileToString(fs::u8path(in_path), raw, &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    nlohmann::json arr;
    try {
        arr = nlohmann::json::parse(raw);
    } catch (const std::exception& ex) {
        std::cerr << "invalid_json: " << ex.what() << "\n";
        return 2;
    }
    if (!arr.is_array()) {
        std::cerr << "expected JSON array of URLs\n";
        return 2;
    }
    nlohmann::json mapped = nlohmann::json::object();
    for (const auto& item : arr) {
        if (!item.is_string()) continue;
        const std::string u = item.get<std::string>();
        auto cr = UrlCanonicalizer::Canonicalize(u);
        const std::string canon = cr.ok ? cr.value : u;
        const std::string pkey = UrlCanonicalizer::PageKeySha1(canon);
        mapped[u] = {{"canonical_url", canon}, {"page_key", pkey}};
    }
    if (!WriteStringToFile(fs::u8path(out_path), mapped.dump(2), &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    std::cout << nlohmann::json({{"ok", true}, {"count", mapped.size()}}).dump(2) << "\n";
    return 0;
}

int CmdSanitize(const std::vector<std::string>& args) {
    const std::string in_path = GetArgVal(args, "--in");
    const std::string out_path = GetArgVal(args, "--out");
    if (in_path.empty() || out_path.empty()) {
        std::cerr << "sanitize requires --in <file> --out <file>\n";
        return 64;
    }
    std::string raw, err;
    if (!ReadFileToString(fs::u8path(in_path), raw, &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    SanitizeOptions sopts;
    if (auto env_creds = CredentialVault::ResolveFromEnv()) {
        sopts.runtime_email = env_creds->email;
        sopts.runtime_password = env_creds->password;
        CredentialVault::ClearMemory(*env_creds);
    }
    Sanitizer san(sopts);
    std::string cleaned;
    std::string trimmed = raw;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    if (!trimmed.empty() && (trimmed.front() == '{' || trimmed.front() == '[')) {
        cleaned = san.SanitizeJsonString(raw);
    } else {
        cleaned = san.SanitizeText(raw);
    }
    if (!WriteStringToFile(fs::u8path(out_path), cleaned, &err)) {
        std::cerr << err << "\n";
        return 2;
    }
    std::cout << nlohmann::json({{"ok", true}}).dump(2) << "\n";
    return 0;
}

int CmdFetchPublicHorarios(const std::vector<std::string>& args) {
    const bool list_escuelas = HasFlag(args, "--list-escuelas");
    const bool list_modalidades = HasFlag(args, "--list-modalidades");
    const std::string escuela = GetArgVal(args, "--escuela");
    const bool verano = HasFlag(args, "--verano");
    const std::string carnet = GetArgVal(args, "--carnet");
    const std::string year_str = GetArgVal(args, "--year");
    const std::string sede = GetArgVal(args, "--sede");
    const std::string modalidad = GetArgVal(args, "--modalidad");
    const std::string periodo = GetArgVal(args, "--periodo");
    const std::string format = GetArgVal(args, "--format", "catalog");
    const std::string cli_base = GetArgVal(args, "--base-url");
    const std::string out_path = GetArgVal(args, "--out");

    if (!list_escuelas && !list_modalidades && escuela.empty() && !verano && carnet.empty()) {
        std::cerr << "fetch-public-horarios requires one of: --list-escuelas, --list-modalidades, "
                     "--escuela <code>, --verano, or --carnet <id>\n";
        return 64;
    }

    const std::string base_url = ResolveBaseUrl(cli_base);
    ServiceConfig cfg = MakeServiceConfig(base_url);
    if (!cli_base.empty() && cli_base.find("/guiahorarios") != std::string::npos) {
        cfg.public_guia_horarios_url = cli_base;
    }
    TecDigitalService svc(cfg);
    auto& client = svc.PublicGuiaClient();
    const Sanitizer san = svc.SnapshotSanitizer();

    auto emit_text = [&](const std::string& txt, int code = 0) -> int {
        std::string err;
        if (!out_path.empty()) {
            if (!WriteStringToFile(fs::u8path(out_path), txt, &err)) {
                std::cerr << err << "\n";
                return 2;
            }
            std::cout << nlohmann::json({{"ok", true}, {"out", out_path}}).dump(2) << "\n";
        } else {
            std::cout << txt << "\n";
        }
        return code;
    };

    if (list_escuelas) {
        auto res = client.FetchEscuelas();
        if (!res.ok) {
            std::cerr << res.error_code << ": " << san.SanitizeText(res.error_message) << "\n";
            return 2;
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& e : res.value) {
            arr.push_back({{"ide_depto", e.ide_depto}, {"dsc_depto", e.dsc_depto}});
        }
        return emit_text(nlohmann::json({{"ok", true}, {"count", arr.size()}, {"escuelas", std::move(arr)}}).dump(2));
    }

    if (list_modalidades) {
        auto res = client.FetchModalidades();
        if (!res.ok) {
            std::cerr << res.error_code << ": " << san.SanitizeText(res.error_message) << "\n";
            return 2;
        }
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& m : res.value) {
            arr.push_back({{"ide_modalidad", m.ide_modalidad}, {"nombre", m.nombre}, {"cant_periodos", m.cant_periodos}});
        }
        return emit_text(nlohmann::json({{"ok", true}, {"count", arr.size()}, {"modalidades", std::move(arr)}}).dump(2));
    }

    int year = year_str.empty() ? (carnet.empty() ? 2026 : 0) : std::atoi(year_str.c_str());
    Result<std::vector<PublicGuiaSessionRow>> rows_res;
    if (!carnet.empty()) {
        rows_res = client.FetchEstudianteByCarnet(carnet, year);
    } else if (verano) {
        rows_res = client.FetchVeranoByYear(year > 0 ? year : 2026);
    } else {
        rows_res = client.FetchEscuelaByYear(escuela, year > 0 ? year : 2026);
    }
    if (!rows_res.ok) {
        std::cerr << rows_res.error_code << ": " << san.SanitizeText(rows_res.error_message) << "\n";
        return 2;
    }

    PublicGuiaFilterOptions filter;
    filter.sede_filter = sede;
    filter.modalidad_filter = modalidad;
    filter.periodo_filter = periodo;

    if (format == "rows") {
        auto filtered = FilterPublicGuiaRows(rows_res.value, filter);
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& r : filtered) {
            arr.push_back({
                {"dsc_sede", r.dsc_sede},
                {"ide_materia", r.ide_materia},
                {"dsc_materia", r.dsc_materia},
                {"ide_grupo", r.ide_grupo},
                {"dsc_depto", r.dsc_depto},
                {"can_creditos", r.can_creditos},
                {"horas", r.horas},
                {"nom_dia", r.nom_dia},
                {"ide_modalidad", r.ide_modalidad},
                {"ide_per_mod", r.ide_per_mod},
                {"num_ano", r.num_ano},
                {"dsc_modalidad", r.dsc_modalidad},
                {"tipo_curso", r.tipo_curso},
                {"hinicio", r.hinicio},
                {"hfin", r.hfin},
                {"nom_profesor", r.nom_profesor}
            });
        }
        return emit_text(nlohmann::json({{"ok", true}, {"count", arr.size()}, {"rows", std::move(arr)}}).dump(2));
    }

    const std::string catalog_json =
        PublicGuiaRowsToHorariosTecJson(rows_res.value, filter, "Guía de Horarios Pública TEC");
    return emit_text(catalog_json);
}

std::string NormalizeUrlForQueue(const std::string& raw, const std::string& current_url) {
    std::string u = raw;
    // trim whitespace
    while (!u.empty() && static_cast<unsigned char>(u.front()) <= ' ') u.erase(u.begin());
    while (!u.empty() && static_cast<unsigned char>(u.back()) <= ' ') u.pop_back();
    if (u.empty()) return {};
    // Strip fragment unless SPA hash route like #/student
    size_t hash = u.find('#');
    if (hash != std::string::npos) {
        if (hash + 1 < u.size() && u[hash + 1] == '/') {
            // keep SPA route only for inventory, strip for HTTP GET
            u = u.substr(0, hash);
        } else {
            u = u.substr(0, hash);
        }
    }
    if (u.empty()) return {};
    std::string low = u;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (low.rfind("javascript:", 0) == 0 || low.rfind("mailto:", 0) == 0 ||
        low.rfind("tel:", 0) == 0 || low.rfind("data:", 0) == 0) {
        return {};
    }
    if (low.rfind("http://", 0) == 0 || low.rfind("https://", 0) == 0) {
        return u;
    }
    std::string origin = ExtractOrigin(current_url);
    if (origin.empty()) origin = "https://tecdigital.tec.ac.cr";
    if (u.front() == '/') {
        if (u.size() >= 2 && u[1] == '/') return "https:" + u;
        return origin + u;
    }
    // Relative to current path directory
    ParsedUrl cur = ParseHttpUrl(current_url);
    std::string dir = cur.valid ? cur.path : "/";
    size_t sl = dir.rfind('/');
    dir = (sl == std::string::npos) ? "/" : dir.substr(0, sl + 1);
    return origin + dir + u;
}

std::string InferModuleFromPath(const std::string& url) {
    ParsedUrl p = ParseHttpUrl(url);
    std::string low = p.valid ? p.path : url;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (p.valid && p.host.find("tec-appsext") != std::string::npos) return "guia_horarios_publica";
    if (low.find("/tda-expediente-estudiantil") != std::string::npos) return "expediente_estudiantil";
    if (low.find("/tda-control-plagio") != std::string::npos || low.find("/tda-plagio") != std::string::npos) return "control_plagio";
    if (low.find("/tda-prematricula") != std::string::npos) return "prematricula";
    if (low.find("/tda-inclusion-manager") != std::string::npos) return "gestor_inclusiones";
    if (low.find("/tda-gestor-inscripcion") != std::string::npos) return "gestor_inscripcion";
    if (low.find("/tda-um") != std::string::npos) return "modelo_usuario";
    if (low.find("/tda-feria-negocios") != std::string::npos) return "feria_negocios";
    if (low.find("/tda-evaluacion-desempeno") != std::string::npos || low.find("/tda-performance-evaluation") != std::string::npos) return "evaluacion_docente";
    if (low.find("/tda-rubrica") != std::string::npos) return "rubricas";
    if (low.find("/tda-notifications") != std::string::npos || low.find("/notifications") != std::string::npos) return "notificaciones";
    if (low.find("/tda-motor-juegos") != std::string::npos || low.find("/gaap") != std::string::npos) return "motor_juegos_gaap";
    if (low.find("/tda-consentimiento-informado") != std::string::npos) return "consentimiento_informado";
    if (low.find("/evaluation/") != std::string::npos) return "evaluaciones";
    if (low.find("/file-storage") != std::string::npos) return "documentos_file_storage";
    if (low.find("/calendar") != std::string::npos) return "calendario";
    if (low.find("/news") != std::string::npos) return "noticias";
    if (low.find("/forums") != std::string::npos) return "foros";
    if (low.find("/tda-sms") != std::string::npos) return "mensajeria_sms";
    if (low.find("/members") != std::string::npos) return "integrantes";
    if (low.find("/dotlrn/classes") != std::string::npos) return "cursos_clases";
    if (low.find("/dotlrn/catedras") != std::string::npos) return "catedras";
    if (low.find("/dotlrn/escuela") != std::string::npos) return "escuelas_comunidades";
    if (low.find("/dotlrn/dop") != std::string::npos) return "dop_bienestar";
    if (low.find("/dotlrn/programas") != std::string::npos) return "programas_institucionales";
    if (low.find("/dotlrn/estudiantes") != std::string::npos) return "espacios_estudiantiles";
    if (low.find("/dotlrn/control-panel") != std::string::npos || low.find("/pvt/") != std::string::npos || low.find("/user/") != std::string::npos) return "mi_cuenta_preferencias";
    if (low.find("/dotlrn") != std::string::npos) return "portal_dotlrn";
    return "otros_plataforma";
}

nlohmann::json JsonValueToSchema(const nlohmann::json& v, int depth = 0) {
    if (depth > 4) return "any";
    if (v.is_null()) return "null";
    if (v.is_boolean()) return "boolean";
    if (v.is_number_integer()) return "integer";
    if (v.is_number_float()) return "number";
    if (v.is_string()) return "string";
    if (v.is_array()) {
        nlohmann::json arr = nlohmann::json::array();
        if (!v.empty()) arr.push_back(JsonValueToSchema(v[0], depth + 1));
        return {{"type", "array"}, {"count_observed", v.size()}, {"item_schema", arr.empty() ? nlohmann::json("unknown") : arr[0]}};
    }
    if (v.is_object()) {
        nlohmann::json obj = nlohmann::json::object();
        size_t n = 0;
        for (auto it = v.begin(); it != v.end() && n < 40; ++it, ++n) {
            obj[it.key()] = JsonValueToSchema(it.value(), depth + 1);
        }
        return obj;
    }
    return "unknown";
}

std::string SafeFilenameFromUrl(const std::string& url) {
    std::string h = Sha256Hex(url).substr(0, 10);
    ParsedUrl p = ParseHttpUrl(url);
    std::string base = p.valid ? p.path : url;
    size_t sl = base.rfind('/');
    if (sl != std::string::npos) base = base.substr(sl + 1);
    std::string clean;
    for (char c : base) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') clean.push_back(c);
        else clean.push_back('_');
    }
    if (clean.empty()) clean = "asset";
    if (clean.size() > 60) clean = clean.substr(clean.size() - 60);
    return h + "_" + clean;
}

int CmdCartographyCrawl(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string out_dir = GetArgVal(args, "--out");
    if (out_dir.empty()) {
        std::cerr << "cartography-crawl requires --out <dir>\n";
        return 64;
    }
    std::string vault_file = GetArgVal(args, "--vault");
    if (vault_file.empty()) {
#ifdef _WIN32
        char* appbuf = nullptr;
        size_t app_len = 0;
        if (_dupenv_s(&appbuf, &app_len, "LOCALAPPDATA") == 0 && appbuf != nullptr) {
            fs::path p = fs::path(appbuf) / "CuadernU" / "tec_digital" / "tec_credentials.dpapi";
            std::free(appbuf);
            if (fs::exists(p)) vault_file = p.u8string();
        }
#endif
    }
    const int max_pages = std::max(20, std::atoi(GetArgVal(args, "--max-pages", "280").c_str()));
    const int max_assets = std::max(20, std::atoi(GetArgVal(args, "--max-assets", "160").c_str()));
    const int delay_ms = std::max(30, std::atoi(GetArgVal(args, "--delay-ms", "90").c_str()));

    ServiceConfig cfg = MakeServiceConfig(base_url, out_dir);
    if (!vault_file.empty()) {
        cfg.vault_config.mode = VaultPersistenceMode::EncryptedVault;
        cfg.vault_config.vault_path = vault_file;
        cfg.vault_config.session_meta_path = (fs::u8path(vault_file).parent_path() / "cache" / "session_meta.json").u8string();
        cfg.vault_config.persist_session_cookies = true;
    }
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok) {
        std::cerr << "auth_failed: " << AuthStatusName(auth.status) << " " << san.SanitizeText(auth.message) << "\n";
        return 3;
    }
    if (svc.GuardedTransport().ActiveMutationsMask() != 0u) {
        std::cerr << "fatal: non-zero mutation mask in read-only cartography crawl\n";
        return 9;
    }

    fs::path outRoot = fs::u8path(out_dir);
    std::error_code ec;
    fs::create_directories(outRoot / "assets", ec);
    fs::create_directories(outRoot / "dom_skeletons", ec);

    // 1. Discover ALL spaces via FetchCoursesAndCommunities
    nlohmann::json spacesJson = nlohmann::json::array();
    std::vector<CommunityInfo> allSpaces;
    if (auto sp = svc.FetchCoursesAndCommunities(false); sp.ok) {
        allSpaces = std::move(sp.value);
    }

    struct QueueEntry {
        std::string url;
        std::string parent;
        int depth = 0;
        std::string source_kind;
    };
    std::deque<QueueEntry> q;
    std::unordered_set<std::string> enqueued;
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> jsUrls;
    std::unordered_set<std::string> cssUrls;
    std::map<std::string, std::vector<std::string>> assetOrigins;
    nlohmann::json blockedList = nlohmann::json::array();
    std::unordered_set<std::string> blockedSeen;
    nlohmann::json externalList = nlohmann::json::array();
    std::unordered_set<std::string> externalSeen;
    nlohmann::json pagesJson = nlohmann::json::array();
    nlohmann::json queueAudit = nlohmann::json::array();

    auto isBinaryOrSkipExt = [](const std::string& url) -> bool {
        ParsedUrl p = ParseHttpUrl(url);
        std::string low = p.valid ? p.path : url;
        for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (const char* ext : {".pdf", ".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp", ".ico", ".woff", ".woff2",
                                ".ttf", ".eot", ".zip", ".rar", ".7z", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
                                ".mp4", ".mp3", ".wav", ".avi", ".mov", ".csv", ".txt", ".xml", ".ics"}) {
            size_t el = std::strlen(ext);
            if (low.size() >= el && low.compare(low.size() - el, el, ext) == 0) return true;
        }
        if (low.find("/file-storage/download/") != std::string::npos ||
            low.find("/file-storage/view/") != std::string::npos ||
            low.find("/shared/portrait-bits") != std::string::npos ||
            low.find("/repositorio/") != std::string::npos) {
            return true;
        }
        return false;
    };

    auto isInfiniteCalendarOrPaginationUrl = [](const std::string& url) -> bool {
        ParsedUrl p = ParseHttpUrl(url);
        if (!p.valid) return false;
        std::string lowq = p.query;
        for (char& c : lowq) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        // Avoid infinite day/week/month stepping or sort permutations
        if (lowq.find("date=") != std::string::npos || lowq.find("julian_date=") != std::string::npos ||
            lowq.find("orderby=") != std::string::npos || lowq.find("sort_by=") != std::string::npos ||
            lowq.find("cal_item_id=") != std::string::npos) {
            return true;
        }
        if (p.path.find("/news/item") != std::string::npos ||
            p.path.find("/forums/message-view") != std::string::npos) {
            return true;
        }
        return false;
    };

    auto enqueueUrl = [&](const std::string& rawUrl, const std::string& parent, int depth, const std::string& kind) {
        std::string u = NormalizeUrlForQueue(rawUrl, parent.empty() ? base_url : parent);
        if (u.empty()) return;
        ParsedUrl pu = ParseHttpUrl(u);
        if (!pu.valid) return;
        std::string lowPath = pu.path;
        for (char& c : lowPath) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        // Asset classification
        if (lowPath.size() >= 3 && lowPath.compare(lowPath.size() - 3, 3, ".js") == 0) {
            if (svc.Policy().IsAllowedOrigin(u)) {
                jsUrls.insert(u);
                if (assetOrigins[u].size() < 5) assetOrigins[u].push_back(parent);
            }
            return;
        }
        if (lowPath.size() >= 4 && lowPath.compare(lowPath.size() - 4, 4, ".css") == 0) {
            if (svc.Policy().IsAllowedOrigin(u)) {
                cssUrls.insert(u);
                if (assetOrigins[u].size() < 5) assetOrigins[u].push_back(parent);
            }
            return;
        }
        if (isBinaryOrSkipExt(u)) return;

        // External host check
        if (!svc.Policy().IsAllowedOrigin(u)) {
            if (externalSeen.insert(pu.host + pu.path).second) {
                externalList.push_back({
                    {"host", pu.host},
                    {"origin", pu.origin},
                    {"path", san.SanitizeText(pu.path)},
                    {"discovered_on", san.SanitizeText(parent)}
                });
            }
            return;
        }

        // Policy check BEFORE enqueue
        PolicyRequestContext pctx;
        pctx.method = "GET";
        pctx.url = u;
        auto dec = svc.Policy().Evaluate(pctx);
        if (!dec.allowed) {
            if (blockedSeen.insert("GET " + u).second) {
                blockedList.push_back({
                    {"method", "GET"},
                    {"url", san.SanitizeText(u)},
                    {"reason", dec.reason},
                    {"discovered_on", san.SanitizeText(parent)},
                    {"module", InferModuleFromPath(u)}
                });
            }
            return;
        }

        if (isInfiniteCalendarOrPaginationUrl(u)) return;
        if (depth > 4) return;
        if (enqueued.insert(u).second) {
            q.push_back({u, parent, depth, kind});
        }
    };

    // Seed core portal & institutional module URLs
    const std::vector<std::string> coreSeeds = {
        base_url + "/dotlrn/index",
        base_url + "/dotlrn/my-communities",
        base_url + "/dotlrn/classes/",
        base_url + "/dotlrn/classes/one-community-type?page_num=0",
        base_url + "/dotlrn/catedras.tec/",
        base_url + "/dotlrn/calendar/view",
        base_url + "/dotlrn/calendar/view?view=list",
        base_url + "/dotlrn/calendar/view?view=month",
        base_url + "/dotlrn/calendar/view?view=week",
        base_url + "/dotlrn/file-storage/",
        base_url + "/dotlrn/control-panel",
        base_url + "/dotlrn/configure",
        base_url + "/pvt/home",
        base_url + "/notifications/manage",
        base_url + "/tda-notifications/",
        base_url + "/tda-expediente-estudiantil/",
        base_url + "/tda-control-plagio/",
        base_url + "/tda-prematricula/",
        base_url + "/tda-gestor-inscripcion/",
        base_url + "/tda-um/",
        base_url + "/tda-um/info",
        base_url + "/tda-feria-negocios/",
        base_url + "/tda-feria-negocios/registro_proyectos",
        base_url + "/tda-inclusion-manager/",
        base_url + "/tda-evaluacion-desempeno/",
        base_url + "/tda-motor-juegos/",
        base_url + "/tda-consentimiento-informado/",
        base_url + "/tda-rubrica/",
        base_url + "/user/",
        "https://tec-appsext.itcr.ac.cr/guiahorarios/escuela.aspx",
        "https://tec-appsext.itcr.ac.cr/guiahorarios/verano.aspx",
        "https://tec-appsext.itcr.ac.cr/guiahorarios/estudiante.aspx"
    };
    for (const auto& s : coreSeeds) enqueueUrl(s, "seed", 0, "core_seed");

    // Seed ALL discovered spaces (all courses, catedras, schools, DOP, programs, etc.)
    for (const auto& c : allSpaces) {
        std::string su = c.url;
        if (su.empty()) continue;
        while (!su.empty() && su.back() == '/') su.pop_back();
        if (su.rfind("http", 0) != 0) su = base_url + (su.front() == '/' ? su : "/" + su);
        spacesJson.push_back({
            {"name", san.SanitizeText(c.name)},
            {"type", c.type},
            {"url", san.SanitizeText(su)},
            {"community_id", c.community_id},
            {"slug", c.slug},
            {"department", c.department},
            {"parent_url", san.SanitizeText(c.parent_url)},
            {"period", c.period},
            {"is_current_period", c.is_current_period}
        });
        enqueueUrl(su + "/one-community?page_num=0", base_url + "/dotlrn/index", 1, "space_home");
        enqueueUrl(su + "/file-storage/", su + "/one-community?page_num=0", 2, "space_files");
        enqueueUrl(su + "/news/", su + "/one-community?page_num=0", 2, "space_news");
        enqueueUrl(su + "/news/?view=archive", su + "/one-community?page_num=0", 2, "space_news_archive");
        enqueueUrl(su + "/calendar/view?view=list", su + "/one-community?page_num=0", 2, "space_calendar");
        if (c.type == "CLASE") {
            enqueueUrl(su + "/evaluation/tda-ce-estudiante/tda-index", su + "/one-community?page_num=0", 2, "space_evals");
            enqueueUrl(su + "/tda-sms/", su + "/one-community?page_num=0", 2, "space_sms");
            enqueueUrl(su + "/members", su + "/one-community?page_num=0", 2, "space_members");
            enqueueUrl(su + "/forums/", su + "/one-community?page_num=0", 2, "space_forums");
        }
    }

    // Regex extractors for structural DOM discovery (all use R"re(...)re" delimiter to allow )" inside patterns)
    const std::regex reHref(R"re(<a\b[^>]*\bhref\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);
    const std::regex reScriptSrc(R"re(<script\b[^>]*\bsrc\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);
    const std::regex reCssHref(R"re(<link\b[^>]*\bhref\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);
    const std::regex reTitle(R"re(<title[^>]*>([\s\S]*?)</title>)re", std::regex::icase);
    const std::regex reForm(R"re(<form\b([^>]*)>([\s\S]*?)</form>)re", std::regex::icase);
    const std::regex reInput(R"re(<(?:input|select|textarea)\b([^>]*)>)re", std::regex::icase);
    const std::regex reBtn(R"re(<(?:button|input\s+[^>]*type\s*=\s*["']?(?:submit|button)|a\s+[^>]*role\s*=\s*["']?button)([^>]*)>)re", std::regex::icase);
    const std::regex rePortlet(R"re(class\s*=\s*["'][^"']*portlet-title[^"']*["'][^>]*>([\s\S]*?)</)re", std::regex::icase);
    const std::regex reImg(R"re(<img\b[^>]*\bsrc\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);
    const std::regex reModal(R"re(<div\b[^>]*\b(?:class\s*=\s*["'][^"']*(?:modal|popup|dialog|pop_up)[^"']*["']|id\s*=\s*["'][^"']*(?:modal|popup|pop_up|dialog)[^"']*["'])[^>]*>)re", std::regex::icase);
    const std::regex reAccord(R"re(class\s*=\s*["'][^"']*(?:accordion|container_accor|collapse|expand)[^"']*["'])re", std::regex::icase);
    const std::regex reResp(R"re(\b(?:hidden-xs|visible-xs|hidden-sm|visible-sm|hidden-md|visible-md|hidden-lg|visible-lg|menu-left|menu-mobile|mobile-nav|d-none|d-md-block|d-lg-block)\b)re", std::regex::icase);
    const std::regex reInlineJs(R"re(<script(?:\s+[^>]*)?>([\s\S]*?)</script>)re", std::regex::icase);
    const std::regex rePageNum(R"re(one-community\?[^"'\s>]*page_num=\d+[^"'\s>]*)re", std::regex::icase);
    const std::regex reNgApp(R"re(\b(?:ng-app|ng-controller|ng-click|ng-model|ng-repeat|data-toggle|data-target|data-rubricid)\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);

    auto getAttrSimple = [](const std::string& tag, const std::string& key) -> std::string {
        std::regex r("(?:^|\\s)" + key + R"re(\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);
        std::smatch m;
        if (std::regex_search(tag, m, r)) {
            if (m[1].matched) return m.str(1);
            if (m[2].matched) return m.str(2);
            if (m[3].matched) return m.str(3);
        }
        return {};
    };

    auto stripTagsSimple = [](const std::string& s) -> std::string {
        std::string out;
        bool inTag = false;
        for (char c : s) {
            if (c == '<') inTag = true;
            else if (c == '>') { inTag = false; out.push_back(' '); }
            else if (!inTag) out.push_back(c);
        }
        std::string col;
        bool ws = true;
        for (char c : out) {
            if (static_cast<unsigned char>(c) <= ' ') {
                if (!ws) { col.push_back(' '); ws = true; }
            } else { col.push_back(c); ws = false; }
        }
        while (!col.empty() && col.back() == ' ') col.pop_back();
        return col;
    };

    std::unordered_set<std::string> skeletonModulesSaved;
    std::cerr << "[CARTOGRAPHY] Seeded " << q.size() << " URLs across " << allSpaces.size() << " spaces. Starting BFS...\n";

    while (!q.empty() && static_cast<int>(visited.size()) < max_pages) {
        QueueEntry cur = q.front();
        q.pop_front();
        if (!visited.insert(cur.url).second) continue;

        if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));

        HttpRequest req;
        req.method = "GET";
        req.url = cur.url;
        req.follow_redirects = false;
        req.connect_timeout_ms = 8000;
        req.receive_timeout_ms = 12000;
        if (cur.url.find("tecdigital.tec.ac.cr") != std::string::npos) {
            req.headers["Cookie"] = svc.Auth().Cookies().BuildCookieHeader(cur.url);
        }
        HttpResponse resp = svc.GuardedTransport().Execute(req);
        const std::string body = resp.BodyAsString();
        const std::string mod = InferModuleFromPath(cur.url);

        if (visited.size() % 25 == 0) {
            std::cerr << "[CARTOGRAPHY] Visited " << visited.size() << "/" << max_pages
                      << " | Queue: " << q.size() << " | JS: " << jsUrls.size()
                      << " | Blocked: " << blockedList.size() << "\n";
        }

        // Follow single-hop safe redirect into queue
        std::string redirTarget;
        if ((resp.status_code == 301 || resp.status_code == 302 || resp.status_code == 303 || resp.status_code == 307) &&
            !resp.location.empty()) {
            redirTarget = ResolveRedirectLocation(cur.url, resp.location);
            if (!redirTarget.empty() && redirTarget.find("/register/") == std::string::npos) {
                enqueueUrl(redirTarget, cur.url, cur.depth + 1, "redirect");
            }
        }

        nlohmann::json pageRec = {
            {"id", "PG-" + std::to_string(visited.size())},
            {"url", san.SanitizeText(cur.url)},
            {"parent_url", san.SanitizeText(cur.parent)},
            {"depth", cur.depth},
            {"source_kind", cur.source_kind},
            {"module", mod},
            {"http_status", resp.status_code},
            {"transport_ok", resp.transport_ok},
            {"content_type", resp.content_type},
            {"bytes", body.size()},
            {"redirect_location", san.SanitizeText(redirTarget)}
        };

        const bool isHtml = (resp.status_code == 200) &&
                            (resp.content_type.find("html") != std::string::npos ||
                             body.find("<html") != std::string::npos ||
                             body.find("<div") != std::string::npos ||
                             body.find("<table") != std::string::npos);

        if (isHtml && !body.empty()) {
            std::smatch tm;
            if (std::regex_search(body, tm, reTitle)) {
                pageRec["title"] = san.SanitizeText(stripTagsSimple(tm.str(1)));
            }

            // Links + dynamic page_num tabs
            size_t linkCount = 0;
            std::set<std::string> tabsFound;
            for (std::sregex_iterator it(body.begin(), body.end(), reHref), end; it != end; ++it) {
                std::string href = (*it)[1].matched ? (*it)[1].str() : ((*it)[2].matched ? (*it)[2].str() : (*it)[3].str());
                ++linkCount;
                if (href.find("page_num=") != std::string::npos) {
                    tabsFound.insert(san.SanitizeText(href));
                }
                enqueueUrl(href, cur.url, cur.depth + 1, "anchor");
            }
            for (std::sregex_iterator it(body.begin(), body.end(), rePageNum), end; it != end; ++it) {
                std::string rel = it->str(0);
                tabsFound.insert(san.SanitizeText(rel));
                enqueueUrl(rel, cur.url, cur.depth + 1, "tab_page_num");
            }
            pageRec["links_count"] = linkCount;
            if (!tabsFound.empty()) {
                nlohmann::json tarr = nlohmann::json::array();
                for (const auto& t : tabsFound) tarr.push_back(t);
                pageRec["discovered_tabs"] = std::move(tarr);
            }

            // Scripts & CSS
            nlohmann::json pageScripts = nlohmann::json::array();
            for (std::sregex_iterator it(body.begin(), body.end(), reScriptSrc), end; it != end; ++it) {
                std::string ssrc = (*it)[1].matched ? (*it)[1].str() : ((*it)[2].matched ? (*it)[2].str() : (*it)[3].str());
                std::string abs = NormalizeUrlForQueue(ssrc, cur.url);
                if (!abs.empty()) {
                    pageScripts.push_back(san.SanitizeText(abs));
                    if (svc.Policy().IsAllowedOrigin(abs)) {
                        jsUrls.insert(abs);
                        if (assetOrigins[abs].size() < 5) assetOrigins[abs].push_back(san.SanitizeText(cur.url));
                    }
                }
            }
            pageRec["scripts"] = std::move(pageScripts);

            nlohmann::json pageCss = nlohmann::json::array();
            for (std::sregex_iterator it(body.begin(), body.end(), reCssHref), end; it != end; ++it) {
                std::string chref = (*it)[1].matched ? (*it)[1].str() : ((*it)[2].matched ? (*it)[2].str() : (*it)[3].str());
                if (chref.find(".css") == std::string::npos) continue;
                std::string abs = NormalizeUrlForQueue(chref, cur.url);
                if (!abs.empty()) {
                    pageCss.push_back(san.SanitizeText(abs));
                    if (svc.Policy().IsAllowedOrigin(abs)) {
                        cssUrls.insert(abs);
                        if (assetOrigins[abs].size() < 5) assetOrigins[abs].push_back(san.SanitizeText(cur.url));
                    }
                }
            }
            pageRec["stylesheets"] = std::move(pageCss);

            // Forms (method, action, input field names/types - NO VALUES!)
            nlohmann::json formsArr = nlohmann::json::array();
            for (std::sregex_iterator fit(body.begin(), body.end(), reForm), end; fit != end; ++fit) {
                std::string fattrs = (*fit)[1].str();
                std::string finner = (*fit)[2].str();
                std::string act = getAttrSimple(fattrs, "action");
                std::string meth = getAttrSimple(fattrs, "method");
                if (meth.empty()) meth = "GET";
                nlohmann::json fields = nlohmann::json::array();
                for (std::sregex_iterator iit(finner.begin(), finner.end(), reInput), iend; iit != iend; ++iit) {
                    std::string ia = (*iit)[1].str();
                    std::string fn = getAttrSimple(ia, "name");
                    std::string ft = getAttrSimple(ia, "type");
                    std::string fid = getAttrSimple(ia, "id");
                    if (!fn.empty() || !fid.empty()) {
                        fields.push_back({{"name", fn}, {"type", ft.empty() ? "field" : ft}, {"id", fid}});
                    }
                }
                formsArr.push_back({
                    {"method", meth},
                    {"action", san.SanitizeText(act)},
                    {"id", getAttrSimple(fattrs, "id")},
                    {"fields_count", fields.size()},
                    {"fields", std::move(fields)}
                });
            }
            if (!formsArr.empty()) pageRec["forms"] = std::move(formsArr);

            // Portlets, banners, modals, accordions, responsive classes, Angular/data hooks
            nlohmann::json portlets = nlohmann::json::array();
            std::set<std::string> seenPort;
            for (std::sregex_iterator it(body.begin(), body.end(), rePortlet), end; it != end; ++it) {
                std::string pt = san.SanitizeText(stripTagsSimple((*it)[1].str()));
                if (!pt.empty() && seenPort.insert(pt).second) portlets.push_back(pt);
            }
            if (!portlets.empty()) pageRec["portlets"] = std::move(portlets);

            nlohmann::json banners = nlohmann::json::array();
            std::set<std::string> seenImg;
            for (std::sregex_iterator it(body.begin(), body.end(), reImg), end; it != end; ++it) {
                std::string isrc = (*it)[1].matched ? (*it)[1].str() : ((*it)[2].matched ? (*it)[2].str() : (*it)[3].str());
                if (isrc.find("/repositorio/") != std::string::npos || isrc.find("banner") != std::string::npos ||
                    isrc.find("/file-storage/view/") != std::string::npos) {
                    if (seenImg.insert(isrc).second && banners.size() < 20) banners.push_back(san.SanitizeText(isrc));
                }
            }
            if (!banners.empty()) pageRec["banners"] = std::move(banners);

            size_t btnCount = 0, modalCount = 0, accordCount = 0;
            for (std::sregex_iterator it(body.begin(), body.end(), reBtn), end; it != end; ++it) ++btnCount;
            for (std::sregex_iterator it(body.begin(), body.end(), reModal), end; it != end; ++it) ++modalCount;
            for (std::sregex_iterator it(body.begin(), body.end(), reAccord), end; it != end; ++it) ++accordCount;
            pageRec["buttons_count"] = btnCount;
            pageRec["modals_count"] = modalCount;
            pageRec["accordions_count"] = accordCount;

            std::set<std::string> respSet;
            for (std::sregex_iterator it(body.begin(), body.end(), reResp), end; it != end; ++it) {
                respSet.insert(it->str(0));
            }
            if (!respSet.empty()) pageRec["responsive_classes"] = respSet;

            std::set<std::string> ngHooks;
            for (std::sregex_iterator it(body.begin(), body.end(), reNgApp), end; it != end; ++it) {
                std::string full = it->str(0);
                if (full.size() <= 90) ngHooks.insert(san.SanitizeText(full));
            }
            if (!ngHooks.empty()) {
                nlohmann::json narr = nlohmann::json::array();
                for (const auto& h : ngHooks) { if (narr.size() < 40) narr.push_back(h); }
                pageRec["frontend_directives_attrs"] = std::move(narr);
            }

            // Inline JS URL / endpoint references (sanitized)
            std::set<std::string> inlineHints;
            for (std::sregex_iterator it(body.begin(), body.end(), reInlineJs), end; it != end; ++it) {
                std::string code = (*it)[1].str();
                if (code.size() < 10) continue;
                static const std::regex reUrlLit(R"re(["']((?:/[a-zA-Z0-9_./-]+|https?://[a-zA-Z0-9_.:/-]+))["'])re");
                for (std::sregex_iterator uit(code.begin(), code.end(), reUrlLit), uend; uit != uend; ++uit) {
                    std::string u = (*uit)[1].str();
                    if (u.find("/ajax/") != std::string::npos || u.find("/api/") != std::string::npos ||
                        u.find("graphql") != std::string::npos || u.find("tda-") != std::string::npos ||
                        u.find("tds-") != std::string::npos || u.find(".aspx/") != std::string::npos) {
                        inlineHints.insert(san.SanitizeText(u));
                    }
                }
            }
            if (!inlineHints.empty()) pageRec["inline_js_endpoint_refs"] = inlineHints;

            // Save 1 sanitized structural DOM skeleton per unique module
            if (skeletonModulesSaved.insert(mod).second) {
                std::ostringstream skel;
                skel << "<!-- Sanitized structural skeleton for module: " << mod << " -->\n";
                static const std::regex reOpenTag(R"(<(header|nav|main|section|aside|footer|form|table|ul|div|button|select|iframe)\b([^>]*)>)", std::regex::icase);
                size_t tagN = 0;
                for (std::sregex_iterator it(body.begin(), body.end(), reOpenTag), end; it != end && tagN < 180; ++it) {
                    std::string tn = (*it)[1].str();
                    std::string ta = (*it)[2].str();
                    std::string idv = getAttrSimple(ta, "id");
                    std::string clv = getAttrSimple(ta, "class");
                    std::string rlv = getAttrSimple(ta, "role");
                    std::string ngc = getAttrSimple(ta, "ng-controller");
                    std::string nga = getAttrSimple(ta, "ng-app");
                    if (idv.empty() && clv.empty() && rlv.empty() && ngc.empty() && nga.empty()) continue;
                    ++tagN;
                    skel << "<" << tn;
                    if (!idv.empty()) skel << " id=\"" << san.SanitizeText(idv) << "\"";
                    if (!clv.empty()) skel << " class=\"" << san.SanitizeText(clv) << "\"";
                    if (!rlv.empty()) skel << " role=\"" << san.SanitizeText(rlv) << "\"";
                    if (!nga.empty()) skel << " ng-app=\"" << san.SanitizeText(nga) << "\"";
                    if (!ngc.empty()) skel << " ng-controller=\"" << san.SanitizeText(ngc) << "\"";
                    skel << ">\n";
                }
                WriteStringToFile(outRoot / "dom_skeletons" / (mod + "_skeleton.html"), skel.str());
            }
        }
        pagesJson.push_back(std::move(pageRec));
        queueAudit.push_back({
            {"url", san.SanitizeText(cur.url)},
            {"parent", san.SanitizeText(cur.parent)},
            {"depth", cur.depth},
            {"status", "visited"},
            {"http_status", resp.status_code},
            {"module", mod}
        });
    }

    // Record any remaining pending items in queue so next agent can resume cleanly
    while (!q.empty()) {
        const auto& cur = q.front();
        queueAudit.push_back({
            {"url", san.SanitizeText(cur.url)},
            {"parent", san.SanitizeText(cur.parent)},
            {"depth", cur.depth},
            {"status", "pending"},
            {"module", InferModuleFromPath(cur.url)}
        });
        q.pop_front();
    }

    // 2. Download public client-side JS and CSS bundles (deduplicated by path basename)
    nlohmann::json assetsManifest = nlohmann::json::array();
    int assetsFetched = 0;
    std::unordered_set<std::string> fetchedBaseNames;
    auto fetchAssetList = [&](const std::unordered_set<std::string>& urls, const std::string& kind) {
        std::vector<std::string> sortedUrls(urls.begin(), urls.end());
        std::sort(sortedUrls.begin(), sortedUrls.end());
        for (const auto& u : sortedUrls) {
            if (assetsFetched >= max_assets) break;
            std::string low = u;
            for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            const bool isVendor = (low.find("jquery") != std::string::npos ||
                                   low.find("bootstrap") != std::string::npos ||
                                   low.find("moment") != std::string::npos ||
                                   low.find("handlebars") != std::string::npos ||
                                   low.find("swiper") != std::string::npos ||
                                   low.find("dropzone") != std::string::npos ||
                                   low.find("angular.min") != std::string::npos ||
                                   low.find("fontawesome") != std::string::npos ||
                                   low.find("font-awesome") != std::string::npos);
            if (isVendor) {
                assetsManifest.push_back({{"url", san.SanitizeText(u)}, {"kind", kind}, {"vendor_skipped", true}, {"discovered_on", assetOrigins[u]}});
                continue;
            }
            ParsedUrl pu = ParseHttpUrl(u);
            std::string baseKey = pu.valid ? pu.path : u;
            size_t sl = baseKey.rfind('/');
            if (sl != std::string::npos) baseKey = baseKey.substr(sl + 1);
            if (!fetchedBaseNames.insert(kind + ":" + baseKey).second) {
                assetsManifest.push_back({{"url", san.SanitizeText(u)}, {"kind", kind}, {"dedup_basename", baseKey}, {"discovered_on", assetOrigins[u]}});
                continue;
            }
            if (delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms / 2));
            HttpRequest ar;
            ar.method = "GET";
            ar.url = u;
            ar.connect_timeout_ms = 8000;
            ar.receive_timeout_ms = 12000;
            if (u.find("tecdigital.tec.ac.cr") != std::string::npos) {
                ar.headers["Cookie"] = svc.Auth().Cookies().BuildCookieHeader(u);
            }
            HttpResponse ares = svc.GuardedTransport().Execute(ar);
            ++assetsFetched;
            std::string body = ares.BodyAsString();
            std::string fname = SafeFilenameFromUrl(u);
            if (ares.status_code == 200 && !body.empty()) {
                WriteStringToFile(outRoot / "assets" / fname, body);
            }
            assetsManifest.push_back({
                {"url", san.SanitizeText(u)},
                {"kind", kind},
                {"vendor_skipped", false},
                {"http_status", ares.status_code},
                {"bytes", body.size()},
                {"file", (ares.status_code == 200 && !body.empty()) ? fname : ""},
                {"sha256", (ares.status_code == 200 && !body.empty()) ? Sha256Hex(body) : ""},
                {"discovered_on", assetOrigins[u]}
            });
        }
    };
    std::cerr << "[CARTOGRAPHY] Downloading " << jsUrls.size() << " JS and " << cssUrls.size() << " CSS bundles...\n";
    fetchAssetList(jsUrls, "js");
    fetchAssetList(cssUrls, "css");

    // 3. Probe read-only JSON endpoints and record ONLY structural JSON schemas (zero personal values)
    nlohmann::json observedSchemas = nlohmann::json::object();
    const std::vector<std::pair<std::string, std::string>> readOnlyJsonEndpoints = {
        {"GET", base_url + "/tda-notifications/ajax/has_unread_notifications"},
        {"GET", base_url + "/tda-notifications/ajax/get_user_notifications"},
        {"GET", base_url + "/tda-expediente-estudiantil/ajax/i18n"},
        {"GET", base_url + "/tda-expediente-estudiantil/ajax/i18n_validation"},
        {"GET", base_url + "/tda-expediente-estudiantil/ajax/combos/carga_sedes_tds_lib"},
        {"GET", base_url + "/tda-expediente-estudiantil/ajax/combos/carga_periodos_tds_lib"},
        {"GET", base_url + "/tda-control-plagio/ajax/cargaArbol"},
        {"GET", base_url + "/tda-performance-evaluation/api/forms/"},
        {"GET", base_url + "/tda-performance-evaluation/api/evaluations/"},
        {"GET", base_url + "/tda-motor-juegos/api/user/"},
        {"GET", base_url + "/tda-feria-negocios/ajax/internationalization"},
        {"GET", base_url + "/tda-um/ajax/internacionalization"},
        {"GET", base_url + "/tda-um/ajax/get_gaap_server"},
        {"GET", base_url + "/tda-consentimiento-informado/ajax/client/get_user_term_condition"}
    };
    for (const auto& [meth, epUrl] : readOnlyJsonEndpoints) {
        HttpRequest jr;
        jr.method = meth;
        jr.url = epUrl;
        jr.headers["Cookie"] = svc.Auth().Cookies().BuildCookieHeader(epUrl);
        jr.headers["X-Requested-With"] = "XMLHttpRequest";
        auto jres = svc.GuardedTransport().Execute(jr);
        std::string jb = jres.BodyAsString();
        nlohmann::json entry = {
            {"method", meth},
            {"http_status", jres.status_code},
            {"content_type", jres.content_type},
            {"bytes", jb.size()}
        };
        auto parsed = nlohmann::json::parse(jb, nullptr, false);
        if (!parsed.is_discarded()) {
            entry["format"] = "json";
            entry["schema"] = JsonValueToSchema(parsed);
        } else {
            entry["format"] = "text_or_html";
        }
        observedSchemas[ParseHttpUrl(epUrl).path] = std::move(entry);
    }

    // Write all sanitized raw outputs into build/cartography_work/
    WriteStringToFile(outRoot / "spaces.json", spacesJson.dump(2));
    WriteStringToFile(outRoot / "pages.json", pagesJson.dump(2));
    WriteStringToFile(outRoot / "queue_state.json", queueAudit.dump(2));
    WriteStringToFile(outRoot / "blocked.json", blockedList.dump(2));
    WriteStringToFile(outRoot / "external_services.json", externalList.dump(2));
    WriteStringToFile(outRoot / "assets_manifest.json", assetsManifest.dump(2));
    WriteStringToFile(outRoot / "observed_schemas.json", observedSchemas.dump(2));

    nlohmann::json summary = {
        {"ok", true},
        {"mutation_mask", svc.GuardedTransport().ActiveMutationsMask()},
        {"spaces_total", spacesJson.size()},
        {"pages_visited", pagesJson.size()},
        {"queue_total", queueAudit.size()},
        {"blocked_count", blockedList.size()},
        {"external_services_count", externalList.size()},
        {"js_discovered", jsUrls.size()},
        {"css_discovered", cssUrls.size()},
        {"assets_fetched", assetsFetched},
        {"json_schemas_probed", observedSchemas.size()}
    };
    WriteStringToFile(outRoot / "crawl_summary.json", summary.dump(2));
    std::cout << summary.dump(2) << "\n";
    return 0;
}

int CmdCartographyPass2(const std::vector<std::string>& args) {
    const std::string base_url = ResolveBaseUrl(GetArgVal(args, "--base-url"));
    const std::string out_dir = GetArgVal(args, "--out");
    if (out_dir.empty()) return 64;
    std::string vault_file = GetArgVal(args, "--vault");
    if (vault_file.empty()) {
#ifdef _WIN32
        char* appbuf = nullptr;
        size_t app_len = 0;
        if (_dupenv_s(&appbuf, &app_len, "LOCALAPPDATA") == 0 && appbuf != nullptr) {
            fs::path p = fs::path(appbuf) / "CuadernU" / "tec_digital" / "tec_credentials.dpapi";
            std::free(appbuf);
            if (fs::exists(p)) vault_file = p.u8string();
        }
#endif
    }
    ServiceConfig cfg = MakeServiceConfig(base_url, out_dir);
    if (!vault_file.empty()) {
        cfg.vault_config.mode = VaultPersistenceMode::EncryptedVault;
        cfg.vault_config.vault_path = vault_file;
        cfg.vault_config.session_meta_path = (fs::u8path(vault_file).parent_path() / "cache" / "session_meta.json").u8string();
        cfg.vault_config.persist_session_cookies = true;
    }
    TecDigitalService svc(cfg);
    AuthResult auth = svc.LoginWithEnvOrVault(false);
    const Sanitizer san = svc.SnapshotSanitizer();
    if (!auth.ok || svc.GuardedTransport().ActiveMutationsMask() != 0u) return 3;

    fs::path outRoot = fs::u8path(out_dir);
    std::string pRaw, qRaw, aRaw;
    if (!ReadFileToString(outRoot / "pages.json", pRaw) ||
        !ReadFileToString(outRoot / "queue_state.json", qRaw) ||
        !ReadFileToString(outRoot / "assets_manifest.json", aRaw)) {
        return 4;
    }
    auto pagesJson = nlohmann::json::parse(pRaw);
    auto queueJson = nlohmann::json::parse(qRaw);
    auto assetsJson = nlohmann::json::parse(aRaw);

    // 1. Fetch ALL distinct unfetched first-party JS bundles discovered across all pages
    std::unordered_set<std::string> fetchedBases;
    for (const auto& a : assetsJson) {
        if (a.value("kind", "") == "js" && !a.value("file", "").empty()) {
            ParsedUrl pu = ParseHttpUrl(a.value("url", ""));
            std::string b = pu.valid ? pu.path : a.value("url", "");
            size_t sl = b.rfind('/');
            if (sl != std::string::npos) b = b.substr(sl + 1);
            fetchedBases.insert(b);
        }
    }
    std::map<std::string, std::string> missingJsByBase;
    for (const auto& p : pagesJson) {
        if (!p.contains("scripts") || !p["scripts"].is_array()) continue;
        for (const auto& sv : p["scripts"]) {
            std::string u = sv.get<std::string>();
            if (!svc.Policy().IsAllowedOrigin(u)) continue;
            // Normalize /./ in Vite asset paths (e.g. /tda-inclusion-manager/./assets/index.js)
            size_t dotpos;
            while ((dotpos = u.find("/./")) != std::string::npos) u.replace(dotpos, 3, "/");
            std::string low = u;
            for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (low.find("jquery") != std::string::npos || low.find("bootstrap") != std::string::npos ||
                low.find("moment") != std::string::npos || low.find("handlebars") != std::string::npos ||
                low.find("swiper") != std::string::npos || low.find("dropzone") != std::string::npos ||
                low.find("angular.min") != std::string::npos || low.find("font") != std::string::npos) continue;
            ParsedUrl pu = ParseHttpUrl(u);
            std::string b = pu.valid ? pu.path : u;
            size_t sl = b.rfind('/');
            if (sl != std::string::npos) b = b.substr(sl + 1);
            if (!b.empty() && fetchedBases.count(b) == 0 && missingJsByBase.count(b) == 0) {
                missingJsByBase[b] = u;
            }
        }
    }
    int extraJs = 0;
    for (const auto& [base, u] : missingJsByBase) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        HttpRequest ar;
        ar.method = "GET";
        ar.url = u;
        ar.connect_timeout_ms = 8000;
        ar.receive_timeout_ms = 12000;
        if (u.find("tecdigital.tec.ac.cr") != std::string::npos) {
            ar.headers["Cookie"] = svc.Auth().Cookies().BuildCookieHeader(u);
        }
        auto ares = svc.GuardedTransport().Execute(ar);
        std::string body = ares.BodyAsString();
        std::string fname = SafeFilenameFromUrl(u);
        if (ares.status_code == 200 && !body.empty()) {
            WriteStringToFile(outRoot / "assets" / fname, body);
            ++extraJs;
        }
        assetsJson.push_back({
            {"url", san.SanitizeText(u)},
            {"kind", "js"},
            {"pass", 2},
            {"vendor_skipped", false},
            {"http_status", ares.status_code},
            {"bytes", body.size()},
            {"file", (ares.status_code == 200 && !body.empty()) ? fname : ""},
            {"sha256", (ares.status_code == 200 && !body.empty()) ? Sha256Hex(body) : ""}
        });
    }

    // 2. Visit remaining unvisited custom tabs (?page_num=N) and distinct module pages in pending queue
    int extraPages = 0;
    const std::regex reTitle(R"re(<title[^>]*>([\s\S]*?)</title>)re", std::regex::icase);
    const std::regex rePortlet(R"re(class\s*=\s*["'][^"']*portlet-title[^"']*["'][^>]*>([\s\S]*?)</)re", std::regex::icase);
    const std::regex reImg(R"re(<img\b[^>]*\bsrc\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))re", std::regex::icase);
    for (auto& qitem : queueJson) {
        if (extraPages >= 140) break;
        if (qitem.value("status", "") != "pending") continue;
        std::string u = qitem.value("url", "");
        std::string mod = qitem.value("module", "");
        // Filter out repetitive calendar period_days/day links and school-directory 2-letter listings
        if (u.find("period_days=") != std::string::npos || u.find("view=day") != std::string::npos ||
            u.find("community-member") != std::string::npos || u.find("revision_id=") != std::string::npos ||
            u.find("www.knjjk") != std::string::npos || u.find("&amp;") != std::string::npos) {
            continue;
        }
        if (mod == "cursos_clases" && u.find("page_num=") == std::string::npos) continue;
        if (mod == "calendario" && u.find("cal-options") == std::string::npos) continue;

        PolicyRequestContext pctx;
        pctx.method = "GET";
        pctx.url = u;
        auto dec = svc.Policy().Evaluate(pctx);
        if (!dec.allowed) {
            qitem["status"] = "blocked";
            qitem["block_reason"] = dec.reason;
            continue;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        HttpRequest req;
        req.method = "GET";
        req.url = u;
        req.follow_redirects = false;
        req.connect_timeout_ms = 8000;
        req.receive_timeout_ms = 12000;
        if (u.find("tecdigital.tec.ac.cr") != std::string::npos) {
            req.headers["Cookie"] = svc.Auth().Cookies().BuildCookieHeader(u);
        }
        auto resp = svc.GuardedTransport().Execute(req);
        std::string body = resp.BodyAsString();
        qitem["status"] = "visited";
        qitem["http_status"] = resp.status_code;
        ++extraPages;

        nlohmann::json pr = {
            {"id", "PG-" + std::to_string(pagesJson.size() + 1)},
            {"url", san.SanitizeText(u)},
            {"parent_url", qitem.value("parent", "")},
            {"depth", qitem.value("depth", 2)},
            {"source_kind", "pass2_pending"},
            {"module", mod},
            {"http_status", resp.status_code},
            {"transport_ok", resp.transport_ok},
            {"content_type", resp.content_type},
            {"bytes", body.size()}
        };
        if (resp.status_code == 200 && !body.empty()) {
            std::smatch tm;
            if (std::regex_search(body, tm, reTitle)) pr["title"] = san.SanitizeText(tm.str(1));
            nlohmann::json ports = nlohmann::json::array();
            std::set<std::string> sp;
            for (std::sregex_iterator it(body.begin(), body.end(), rePortlet), end; it != end; ++it) {
                std::string t = san.SanitizeText((*it)[1].str());
                if (!t.empty() && sp.insert(t).second) ports.push_back(t);
            }
            if (!ports.empty()) pr["portlets"] = std::move(ports);
            nlohmann::json bans = nlohmann::json::array();
            std::set<std::string> sb;
            for (std::sregex_iterator it(body.begin(), body.end(), reImg), end; it != end; ++it) {
                std::string isrc = (*it)[1].matched ? (*it)[1].str() : ((*it)[2].matched ? (*it)[2].str() : (*it)[3].str());
                if ((isrc.find("/repositorio/") != std::string::npos || isrc.find("/file-storage/view/") != std::string::npos) &&
                    sb.insert(isrc).second && bans.size() < 15) {
                    bans.push_back(san.SanitizeText(isrc));
                }
            }
            if (!bans.empty()) pr["banners"] = std::move(bans);
        }
        pagesJson.push_back(std::move(pr));
    }

    WriteStringToFile(outRoot / "pages.json", pagesJson.dump(2));
    WriteStringToFile(outRoot / "queue_state.json", queueJson.dump(2));
    WriteStringToFile(outRoot / "assets_manifest.json", assetsJson.dump(2));
    std::cout << "PASS2_OK extra_js=" << extraJs << " extra_pages=" << extraPages
              << " total_pages=" << pagesJson.size() << " total_assets=" << assetsJson.size() << "\n";
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: tec_digital_cli <login|vault-save|vault-check|sync|download-file|"
                     "index-materials|search-materials|fetch-curriculum|fetch-expediente|"
                     "fetch-public-horarios|http-crawl|verify-offline|policy-request|"
                     "canonicalize-urls|sanitize> [options]\n";
        return 64;
    }
    const std::string cmd = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);

    if (cmd == "login") return CmdLogin(args);
    if (cmd == "vault-save") return CmdVaultSave(args);
    if (cmd == "vault-check") return CmdVaultCheck(args);
    if (cmd == "sync") return CmdSync(args);
    if (cmd == "download-file") return CmdDownloadFile(args);
    if (cmd == "index-materials") return CmdIndexMaterials(args);
    if (cmd == "search-materials") return CmdSearchMaterials(args);
    if (cmd == "fetch-curriculum") return CmdFetchCurriculum(args);
    if (cmd == "fetch-expediente") return CmdFetchExpediente(args);
    if (cmd == "fetch-public-horarios") return CmdFetchPublicHorarios(args);
    if (cmd == "http-crawl") return CmdHttpCrawl(args);
    if (cmd == "verify-offline") return CmdVerifyOffline(args);
    if (cmd == "policy-request") return CmdPolicyRequest(args);
    if (cmd == "canonicalize-urls") return CmdCanonicalizeUrls(args);
    if (cmd == "sanitize") return CmdSanitize(args);
    if (cmd == "cartography-crawl") return CmdCartographyCrawl(args);
    if (cmd == "cartography-pass2") return CmdCartographyPass2(args);

    std::cerr << "Unknown command: " << cmd << "\n";
    return 64;
}
