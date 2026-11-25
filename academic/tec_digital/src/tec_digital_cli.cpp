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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
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

    std::cerr << "Unknown command: " << cmd << "\n";
    return 64;
}
