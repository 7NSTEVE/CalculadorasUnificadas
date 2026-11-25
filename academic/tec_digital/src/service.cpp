#include "academic/tec_digital/service.hpp"
#include "academic/tec_digital/crypto.hpp"
#include "http_common.hpp"

#include <algorithm>
#include <unordered_set>

namespace academic::tec_digital {

namespace {

class ScopedAllowLogin {
public:
    explicit ScopedAllowLogin(PolicyEnforcingTransport& transport)
        : transport_(transport) {
        transport_.AcquireAllowLogin();
    }
    ~ScopedAllowLogin() {
        transport_.ReleaseAllowLogin();
    }
    ScopedAllowLogin(const ScopedAllowLogin&) = delete;
    ScopedAllowLogin& operator=(const ScopedAllowLogin&) = delete;

private:
    PolicyEnforcingTransport& transport_;
};

void SanitizeUrlOrText(std::string& s, const Sanitizer& san) {
    if (s.empty()) return;
    if (s.find("://") != std::string::npos) {
        s = san.SanitizeUrl(s);
    } else {
        s = san.SanitizeText(s);
    }
}

void SanitizeCatalogPayload(TdaTdsCatalog& cat, const Sanitizer& san) {
    SanitizeUrlOrText(cat.gaap_server, san);
    cat.notification_request_id = san.SanitizeText(cat.notification_request_id);
    cat.inclusion_transaction_key = san.SanitizeText(cat.inclusion_transaction_key);

    for (auto& s : cat.sedes) {
        s.key = san.SanitizeText(s.key);
        s.name = san.SanitizeText(s.name);
    }
    for (auto& c : cat.carreras) {
        c.key = san.SanitizeText(c.key);
        c.name = san.SanitizeText(c.name);
    }
    for (auto& p : cat.planes) {
        p.key = san.SanitizeText(p.key);
        p.name = san.SanitizeText(p.name);
    }
    cat.color_codes.title = san.SanitizeText(cat.color_codes.title);
    cat.color_codes.additional_text = san.SanitizeText(cat.color_codes.additional_text);
    for (auto& cc : cat.color_codes.color_codes) {
        cc.label = san.SanitizeText(cc.label);
        cc.css_class = san.SanitizeText(cc.css_class);
    }

    if (cat.student_career.has_value()) {
        cat.student_career->nombre = san.SanitizeText(cat.student_career->nombre);
        cat.student_career->sede = san.SanitizeText(cat.student_career->sede);
        cat.student_career->carrera = san.SanitizeText(cat.student_career->carrera);
    }

    if (cat.curriculum_graph.has_value()) {
        cat.curriculum_graph->dsc_curriculum = san.SanitizeText(cat.curriculum_graph->dsc_curriculum);
        cat.curriculum_graph->modality = san.SanitizeText(cat.curriculum_graph->modality);
        cat.curriculum_graph->academic_degree = san.SanitizeText(cat.curriculum_graph->academic_degree);
        for (auto& lv : cat.curriculum_graph->levels) {
            for (auto& co : lv.courses) {
                co.id_course = san.SanitizeText(co.id_course);
                co.name = san.SanitizeText(co.name);
                co.truncated_name = san.SanitizeText(co.truncated_name);
                co.t_equiv = san.SanitizeText(co.t_equiv);
            }
        }
    }

    if (cat.student_profile.has_value()) {
        for (auto& g : cat.student_profile->grades) {
            g.id_course = san.SanitizeText(g.id_course);
            g.best_status = san.SanitizeText(g.best_status);
            for (auto& att : g.grades_record) {
                att.modality = san.SanitizeText(att.modality);
                att.name_equivalent = san.SanitizeText(att.name_equivalent);
                att.id_equivalent = san.SanitizeText(att.id_equivalent);
                for (auto& sch : att.schedule) {
                    sch = san.SanitizeText(sch);
                }
            }
        }
    }

    if (cat.expediente.has_value()) {
        auto& ex = *cat.expediente;
        for (auto& kv : ex.i18n_labels) kv.second = san.SanitizeText(kv.second);
        for (auto& kv : ex.validation_labels) kv.second = san.SanitizeText(kv.second);
        for (auto& s : ex.sedes_combo) {
            s.key = san.SanitizeText(s.key);
            s.name = san.SanitizeText(s.name);
        }
        for (auto& p : ex.periodos_combo) {
            p.key = san.SanitizeText(p.key);
            p.name = san.SanitizeText(p.name);
        }
        ex.detalle_notas_html = san.SanitizeHtml(ex.detalle_notas_html);
        ex.detalle_promedio_html = san.SanitizeHtml(ex.detalle_promedio_html);
        ex.tabla_promedio_html = san.SanitizeHtml(ex.tabla_promedio_html);
        ex.detalle_historial_html = san.SanitizeHtml(ex.detalle_historial_html);
        ex.citas_matricula_html = san.SanitizeHtml(ex.citas_matricula_html);
        ex.guia_horario_html = san.SanitizeHtml(ex.guia_horario_html);
        ex.weighted_average_text = san.SanitizeText(ex.weighted_average_text);
        ex.approval_pct_text = san.SanitizeText(ex.approval_pct_text);
    }

    if (cat.prematricula.has_value()) {
        auto& pm = *cat.prematricula;
        pm.rn_student = san.SanitizeText(pm.rn_student);
        for (auto& c : pm.courses) {
            c.course_code = san.SanitizeText(c.course_code);
            c.course_elective = san.SanitizeText(c.course_elective);
            c.name_elective = san.SanitizeText(c.name_elective);
        }
        for (auto& req : pm.requests) {
            req.course_code = san.SanitizeText(req.course_code);
            req.observation = san.SanitizeText(req.observation);
        }
        for (auto& dc : pm.disabled_courses) {
            dc = san.SanitizeText(dc);
        }
        for (auto& kv : pm.i18n_labels) kv.second = san.SanitizeText(kv.second);
    }

    if (cat.notifications.has_value()) {
        for (auto& n : cat.notifications->notifications) {
            n.notification_id = san.SanitizeText(n.notification_id);
            n.creation_date = san.SanitizeText(n.creation_date);
            n.expiration_date = san.SanitizeText(n.expiration_date);
            n.title = san.SanitizeText(n.title);
            n.text = san.SanitizeText(n.text);
            n.url = san.SanitizeUrl(n.url);
            n.state = san.SanitizeText(n.state);
        }
    }

    if (cat.feria_negocios.has_value()) {
        auto& fn = *cat.feria_negocios;
        for (auto& kv : fn.i18n_labels) kv.second = san.SanitizeText(kv.second);
        fn.info_estudiante_raw_json = san.SanitizeJsonString(fn.info_estudiante_raw_json);
        fn.student_location.id = san.SanitizeText(fn.student_location.id);
        fn.student_location.name = san.SanitizeText(fn.student_location.name);
        fn.active_fairs_raw_json = san.SanitizeJsonString(fn.active_fairs_raw_json);
        for (auto& c : fn.categories) {
            c.name = san.SanitizeText(c.name);
        }
    }

    cat.gestor_inscripcion_selector_html = san.SanitizeHtml(cat.gestor_inscripcion_selector_html);
    cat.gestor_inscripcion_events_json = san.SanitizeJsonString(cat.gestor_inscripcion_events_json);

    for (auto& ev : cat.evaluations) {
        ev.community_slug = san.SanitizeText(ev.community_slug);
        ev.final_grade_text = san.SanitizeText(ev.final_grade_text);
        for (auto& c : ev.categories) {
            c.category_id = san.SanitizeText(c.category_id);
            c.name = san.SanitizeText(c.name);
            c.weight_text = san.SanitizeText(c.weight_text);
            for (auto& item : c.items) {
                item.assign_id = san.SanitizeText(item.assign_id);
                item.name = san.SanitizeText(item.name);
                item.grade_text = san.SanitizeText(item.grade_text);
            }
        }
        for (auto& pf : ev.performance_forms) {
            pf.name = san.SanitizeText(pf.name);
            pf.instructions = san.SanitizeText(pf.instructions);
            for (auto& sec : pf.sections) {
                sec.name = san.SanitizeText(sec.name);
                sec.description = san.SanitizeText(sec.description);
                for (auto& it : sec.items) {
                    it.description = san.SanitizeText(it.description);
                }
            }
        }
        ev.performance_evaluations_raw_json =
            san.SanitizeJsonString(ev.performance_evaluations_raw_json);
        for (auto& kv : ev.i18n_labels) kv.second = san.SanitizeText(kv.second);
    }

    for (auto& kv : cat.news_rss_by_community) {
        for (auto& item : kv.second) {
            item.title = san.SanitizeText(item.title);
            item.link = san.SanitizeUrl(item.link);
            SanitizeUrlOrText(item.guid, san);
            item.description = san.SanitizeText(item.description);
            item.pub_date = san.SanitizeText(item.pub_date);
        }
    }

    for (auto& kv : cat.members_by_community) {
        for (auto& m : kv.second) {
            m.user_id = san.SanitizeText(m.user_id);
            m.first_name = san.SanitizeText(m.first_name);
            m.last_name = san.SanitizeText(m.last_name);
            m.full_name = san.SanitizeText(m.full_name);
            m.email = san.SanitizeText(m.email);
            m.role = san.SanitizeText(m.role);
        }
    }

    for (auto& w : cat.warnings) {
        w = san.SanitizeText(w);
    }
}

}  // namespace

TecDigitalService::TecDigitalService(ServiceConfig config,
                                     std::unique_ptr<IHttpTransport> transport)
    : config_(std::move(config)),
      raw_transport_(transport ? std::move(transport) : CreateNativeHttpTransport()),
      policy_(config_.policy_options),
      guarded_transport_(std::make_unique<PolicyEnforcingTransport>(*raw_transport_, policy_)),
      sanitizer_(config_.sanitize_options) {
    if (config_.auth_options.base_url == "https://tecdigital.tec.ac.cr" &&
        !config_.base_url.empty() && config_.base_url != config_.auth_options.base_url) {
        config_.auth_options.base_url = config_.base_url;
    } else if (config_.base_url.empty()) {
        config_.base_url = config_.auth_options.base_url;
    }
    if (config_.crawler_options.base_url == "https://tecdigital.tec.ac.cr" &&
        !config_.base_url.empty()) {
        config_.crawler_options.base_url = config_.base_url;
    }

    if (config_.graphql_url == "https://api.tec.ac.cr:3005/graphql" && !config_.base_url.empty()) {
        ParsedUrl bu = ParseHttpUrl(config_.base_url);
        if (bu.valid && IsLoopbackHost(bu.host)) {
            config_.graphql_url = bu.origin + "/graphql";
        }
    }
    if (config_.public_guia_horarios_url == "https://tec-appsext.itcr.ac.cr/guiahorarios" &&
        !config_.base_url.empty()) {
        ParsedUrl bu = ParseHttpUrl(config_.base_url);
        if (bu.valid && IsLoopbackHost(bu.host)) {
            config_.public_guia_horarios_url = bu.origin + "/guiahorarios";
        }
    }

    auth_ = std::make_unique<AuthClient>(*guarded_transport_, policy_, config_.auth_options);
    tda_client_ = std::make_unique<TdaTdsClient>(
        *guarded_transport_, *auth_, config_.base_url, config_.sanitize_options);
    graphql_client_ = std::make_unique<TecGraphQlClient>(
        *guarded_transport_, *auth_, config_.base_url, config_.graphql_url, config_.sanitize_options);
    public_guia_client_ = std::make_unique<PublicGuiaHorariosClient>(
        *guarded_transport_, config_.public_guia_horarios_url, config_.sanitize_options);
    material_index_ = std::make_unique<OfflineMaterialIndex>(
        config_.offline_root, config_.sanitize_options);
}

Sanitizer TecDigitalService::SnapshotSanitizer() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return sanitizer_;
}

SanitizeOptions TecDigitalService::SnapshotSanitizeOptions() const {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    return config_.sanitize_options;
}

void TecDigitalService::RegisterSanitizerNeedles(const Credentials& creds) {
    std::lock_guard<std::mutex> lk(sanitizer_mutex_);
    SanitizeOptions updated = config_.sanitize_options;
    auto push_pii = [&](const std::string& val) {
        if (val.size() < 4) return;
        if (std::find(updated.pii_needles.begin(), updated.pii_needles.end(), val) ==
            updated.pii_needles.end()) {
            updated.pii_needles.push_back(val);
        }
    };
    if (!creds.email.empty()) {
        if (!updated.runtime_email.empty() && updated.runtime_email != creds.email) {
            push_pii(updated.runtime_email);
        }
        updated.runtime_email = creds.email;
    }
    if (!creds.password.empty()) {
        if (!updated.runtime_password.empty() && updated.runtime_password != creds.password) {
            push_pii(updated.runtime_password);
        }
        updated.runtime_password = creds.password;
    }
    config_.sanitize_options = updated;
    sanitizer_ = Sanitizer(updated);
    if (tda_client_) {
        tda_client_->SetSanitizeOptions(updated);
    }
    if (graphql_client_) {
        graphql_client_->SetSanitizeOptions(updated);
    }
    if (public_guia_client_) {
        public_guia_client_->SetSanitizeOptions(updated);
    }
    if (material_index_) {
        material_index_->SetSanitizeOptions(updated);
    }
}

AuthResult TecDigitalService::LoginWithCredentials(const Credentials& creds,
                                                   bool save_to_vault_on_success) {
    Credentials local = creds;
    RegisterSanitizerNeedles(local);

    AuthResult res;
    {
        ScopedAllowLogin allow_scope(*guarded_transport_);
        res = auth_->LoginWithPassword(local.email, local.password);
    }

    if (res.ok && save_to_vault_on_success &&
        config_.vault_config.mode == VaultPersistenceMode::EncryptedVault) {
        std::string vpath = config_.vault_config.vault_path.empty()
                                ? CredentialVault::DefaultVaultPath()
                                : config_.vault_config.vault_path;
        CredentialVault::SaveEncrypted(
            vpath, local, auth_->Cookies().AllCookies(),
            config_.vault_config.persist_session_cookies,
            config_.vault_config.key_override);

        if (!config_.vault_config.session_meta_path.empty()) {
            CredentialVault::WriteSanitizedSessionMeta(
                config_.vault_config.session_meta_path, auth_->Cookies());
        }
    }

    CredentialVault::ClearMemory(local);
    {
        std::lock_guard<std::mutex> lk(sanitizer_mutex_);
        res.message = sanitizer_.SanitizeText(res.message);
    }
    return res;
}

AuthResult TecDigitalService::LoginWithEnvOrVault(bool save_to_vault_on_success) {
    std::string env_err;
    if (auto env_creds = CredentialVault::ResolveFromEnv(&env_err)) {
        AuthResult r = LoginWithCredentials(*env_creds, save_to_vault_on_success);
        CredentialVault::ClearMemory(*env_creds);
        return r;
    }

    if (config_.vault_config.mode == VaultPersistenceMode::EncryptedVault) {
        std::string vpath = config_.vault_config.vault_path.empty()
                                ? CredentialVault::DefaultVaultPath()
                                : config_.vault_config.vault_path;
        auto loaded = CredentialVault::LoadEncrypted(vpath, config_.vault_config.key_override);
        if (loaded.ok) {
            if (config_.vault_config.persist_session_cookies && !loaded.value.cookies.empty()) {
                auth_->Cookies().SetCookies(loaded.value.cookies);
                if (auth_->CheckSession(true)) {
                    AuthResult ok_cached;
                    ok_cached.ok = true;
                    ok_cached.status = AuthStatus::Ok;
                    ok_cached.status_string = "ok";
                    ok_cached.ad_session_id_present = true;
                    RegisterSanitizerNeedles(loaded.value.credentials);
                    CredentialVault::ClearMemory(loaded.value);
                    return ok_cached;
                }
            }
            AuthResult r = LoginWithCredentials(loaded.value.credentials, false);
            CredentialVault::ClearMemory(loaded.value);
            return r;
        }
    }

    AuthResult fail;
    fail.ok = false;
    fail.status = AuthStatus::EmptyCredentials;
    fail.status_string = AuthStatusToString(fail.status);
    fail.error_code = AuthClient::ErrorCodeForStatus(fail.status);
    fail.message = SnapshotSanitizer().SanitizeText(
        env_err.empty() ? "no credentials available in environment or vault" : env_err);
    return fail;
}

AuthResult TecDigitalService::AttemptQrScanning(std::string_view email,
                                                std::string_view return_url) {
    if (!email.empty()) {
        Credentials c;
        c.email = std::string(email);
        RegisterSanitizerNeedles(c);
    }
    AuthResult res;
    {
        ScopedAllowLogin allow_scope(*guarded_transport_);
        res = auth_->AttemptQrScanning(email, return_url);
    }
    {
        std::lock_guard<std::mutex> lk(sanitizer_mutex_);
        res.message = sanitizer_.SanitizeText(res.message);
    }
    return res;
}

bool TecDigitalService::CheckSession(bool force_probe) {
    return auth_->CheckSession(force_probe);
}

Result<std::vector<CommunityInfo>> TecDigitalService::FetchCoursesAndCommunities(
    bool include_catalog_pages) {
    auto res = tda_client_->FetchCoursesAndCommunities(include_catalog_pages);
    const Sanitizer san = SnapshotSanitizer();
    if (res.ok) {
        for (auto& c : res.value) {
            c.name = san.SanitizeText(c.name);
            c.url = san.SanitizeUrl(c.url);
        }
    } else {
        res.error_message = san.SanitizeText(res.error_message);
    }
    return res;
}

Result<TdaTdsCatalog> TecDigitalService::FetchTdaTdsCatalog(const TdaTdsCatalogOptions& opts) {
    auto res = tda_client_->FetchCatalog(opts);
    const Sanitizer san = SnapshotSanitizer();
    if (res.ok) {
        SanitizeCatalogPayload(res.value, san);
    } else {
        res.error_message = san.SanitizeText(res.error_message);
    }
    return res;
}

Result<SyncFolderStorageResult> TecDigitalService::SyncFolderStorage(
    const SyncFolderStorageOptions& opts) {
    if (config_.offline_root.empty() && opts.download_files) {
        return Result<SyncFolderStorageResult>::Failure(
            "empty_offline_root", "ServiceConfig::offline_root is required for downloads");
    }

    const SanitizeOptions san_opts = SnapshotSanitizeOptions();
    SyncFolderStorageResult out;
    FileStorageWalker walker(*guarded_transport_, *auth_, config_.base_url, san_opts);
    StorageDownloader downloader(
        *guarded_transport_, *auth_, config_.base_url, config_.downloader_options, san_opts);

    std::vector<FolderRecord> all_folders;
    std::vector<FileRecord> all_files;
    std::vector<FolderUrlLink> all_links;
    std::vector<DiscoveredFormMeta> all_forms;
    std::unordered_set<std::string> walked_roots;

    WalkerOptions wopts;
    wopts.max_depth = opts.max_depth;
    wopts.max_folders = opts.max_folders;

    for (const auto& fid : opts.seed_folder_ids) {
        if (!walked_roots.insert(fid).second) continue;
        auto wr = walker.WalkFolderTree(fid, "portal", "CLASE", "", wopts);
        const Sanitizer san = SnapshotSanitizer();
        if (!wr.ok) {
            out.warnings.push_back(san.SanitizeText(wr.error_message));
            continue;
        }
        if (wr.value.session_expired) out.session_expired = true;
        all_folders.insert(all_folders.end(), wr.value.folders.begin(), wr.value.folders.end());
        all_files.insert(all_files.end(), wr.value.files.begin(), wr.value.files.end());
        all_links.insert(all_links.end(), wr.value.url_links.begin(), wr.value.url_links.end());
        for (const auto& w : wr.value.warnings) out.warnings.push_back(san.SanitizeText(w));
    }

    for (const auto& comm_url : opts.community_urls_or_slugs) {
        if (out.session_expired) break;
        ++out.communities_scanned;
        auto roots = walker.DiscoverRootFolderIds(comm_url, 3);
        if (!roots.ok) {
            if (roots.error_code == "session_expired") {
                out.session_expired = true;
                break;
            }
            out.warnings.push_back(SnapshotSanitizer().SanitizeText(roots.error_message));
            continue;
        }
        std::string comm_type =
            (AsciiLower(comm_url).find("catedras.tec") != std::string::npos) ? "CATEDRA" : "CLASE";
        std::string slug = comm_url;
        size_t sl = slug.find_last_of('/');
        if (sl != std::string::npos && sl + 1 < slug.size()) slug = slug.substr(sl + 1);

        for (const auto& fid : roots.value) {
            if (!walked_roots.insert(fid).second) continue;
            auto wr = walker.WalkFolderTree(fid, slug, comm_type, comm_url, wopts);
            if (!wr.ok) continue;
            const Sanitizer san = SnapshotSanitizer();
            if (wr.value.session_expired) out.session_expired = true;
            all_folders.insert(all_folders.end(), wr.value.folders.begin(), wr.value.folders.end());
            all_files.insert(all_files.end(), wr.value.files.begin(), wr.value.files.end());
            all_links.insert(all_links.end(), wr.value.url_links.begin(), wr.value.url_links.end());
            for (const auto& w : wr.value.warnings) out.warnings.push_back(san.SanitizeText(w));
        }
    }

    if (opts.run_headless_bfs_fallback && !out.session_expired) {
        CrawlerOptions copts = config_.crawler_options;
        copts.base_url = config_.base_url;
        copts.download_discovered_files = false;
        HeadlessCrawler crawler(*guarded_transport_, *auth_, copts, SnapshotSanitizeOptions());
        auto cr = crawler.Run();
        if (cr.ok) {
            if (cr.value.session_expired) out.session_expired = true;
            all_folders.insert(all_folders.end(), cr.value.folders.begin(), cr.value.folders.end());
            all_files.insert(all_files.end(), cr.value.files.begin(), cr.value.files.end());
            all_links.insert(all_links.end(), cr.value.url_links.begin(), cr.value.url_links.end());
            all_forms.insert(all_forms.end(), cr.value.forms.begin(), cr.value.forms.end());
        }
    }

    if (opts.download_files && !config_.offline_root.empty() && !out.session_expired) {
        for (auto& fr : all_files) {
            if (out.session_expired) break;
            auto dr = downloader.DownloadFile(fr, config_.offline_root);
            if (dr.outcome == DownloadOutcome::Downloaded) {
                ++out.files_downloaded;
            } else if (dr.outcome == DownloadOutcome::Deduplicated) {
                ++out.files_downloaded;
                ++out.files_deduplicated;
            } else if (dr.outcome == DownloadOutcome::Orphan404) {
                ++out.files_orphan_404;
            } else if (dr.outcome == DownloadOutcome::SessionExpired) {
                out.session_expired = true;
                ++out.files_failed;
            } else {
                ++out.files_failed;
            }
        }
    }

    out.folders_discovered = all_folders.size();
    out.files_discovered = all_files.size();

    if (!config_.offline_root.empty()) {
        MirrorManifestStore store(SnapshotSanitizeOptions());
        store.Save(config_.offline_root, all_files, all_folders, all_forms, all_links);

        if (opts.build_material_index) {
            material_index_->SetOfflineRoot(config_.offline_root);
            auto ir = material_index_->IndexDirectory(config_.offline_root);
            if (ir.ok) {
                out.index_stats = std::move(ir.value);
            } else {
                out.warnings.push_back(SnapshotSanitizer().SanitizeText(ir.error_message));
            }
        }
    }

    out.folders = std::move(all_folders);
    out.files = std::move(all_files);
    out.url_links = std::move(all_links);

    return Result<SyncFolderStorageResult>::Success(std::move(out));
}

Result<std::vector<MaterialSearchHit>> TecDigitalService::SearchOfflineMaterials(
    std::string_view query,
    const MaterialSearchFilters& filters) {
    if (material_index_->Documents().empty() && !config_.offline_root.empty()) {
        material_index_->LoadFromDisk(config_.offline_root);
    }
    return Result<std::vector<MaterialSearchHit>>::Success(
        material_index_->SearchOfflineMaterials(query, filters));
}

Result<OfflineMaterialStats> TecDigitalService::GetOfflineStats() {
    if (material_index_->Documents().empty() && !config_.offline_root.empty()) {
        material_index_->LoadFromDisk(config_.offline_root);
    }
    return Result<OfflineMaterialStats>::Success(material_index_->GetOfflineStats());
}

}  // namespace academic::tec_digital
