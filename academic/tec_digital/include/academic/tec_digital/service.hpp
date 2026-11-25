#pragma once

#include "academic/tec_digital/auth_client.hpp"
#include "academic/tec_digital/graphql_client.hpp"
#include "academic/tec_digital/material_indexer.hpp"
#include "academic/tec_digital/policy.hpp"
#include "academic/tec_digital/public_guia_client.hpp"
#include "academic/tec_digital/sanitizer.hpp"
#include "academic/tec_digital/storage.hpp"
#include "academic/tec_digital/tda_tds_client.hpp"
#include "academic/tec_digital/types.hpp"
#include "academic/tec_digital/vault.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace academic::tec_digital {

struct ServiceConfig {
    std::string base_url = "https://tecdigital.tec.ac.cr";
    std::string graphql_url = "https://api.tec.ac.cr:3005/graphql";
    std::string public_guia_horarios_url = "https://tec-appsext.itcr.ac.cr/guiahorarios";
    std::string offline_root;
    AuthClientOptions auth_options;
    PolicyOptions policy_options;
    VaultConfig vault_config;
    SanitizeOptions sanitize_options;
    DownloaderOptions downloader_options;
    CrawlerOptions crawler_options;
};

struct SyncFolderStorageOptions {
    std::vector<std::string> community_urls_or_slugs;
    std::vector<std::string> seed_folder_ids;
    bool run_headless_bfs_fallback = false;
    bool download_files = true;
    bool build_material_index = true;
    int max_depth = 10;
    size_t max_folders = 400;
};

struct SyncFolderStorageResult {
    size_t communities_scanned = 0;
    size_t folders_discovered = 0;
    size_t files_discovered = 0;
    size_t files_downloaded = 0;
    size_t files_deduplicated = 0;
    size_t files_failed = 0;
    size_t files_orphan_404 = 0;
    bool session_expired = false;
    OfflineMaterialStats index_stats;
    std::vector<FolderRecord> folders;
    std::vector<FileRecord> files;
    std::vector<FolderUrlLink> url_links;
    std::vector<std::string> warnings;
};

class TecDigitalService {
public:
    explicit TecDigitalService(ServiceConfig config = {},
                               std::unique_ptr<IHttpTransport> transport = nullptr);

    AuthResult LoginWithEnvOrVault(bool save_to_vault_on_success = false);
    AuthResult LoginWithCredentials(const Credentials& creds,
                                    bool save_to_vault_on_success = false);
    AuthResult AttemptQrScanning(std::string_view email,
                                 std::string_view return_url = "");

    bool CheckSession(bool force_probe = false);

    Result<std::vector<CommunityInfo>> FetchCoursesAndCommunities(bool include_catalog_pages = false);
    Result<TdaTdsCatalog> FetchTdaTdsCatalog(const TdaTdsCatalogOptions& opts = {});
    Result<SyncFolderStorageResult> SyncFolderStorage(const SyncFolderStorageOptions& opts = {});

    Result<std::vector<MaterialSearchHit>> SearchOfflineMaterials(
        std::string_view query,
        const MaterialSearchFilters& filters = {});
    Result<OfflineMaterialStats> GetOfflineStats();

    const ServiceConfig& Config() const { return config_; }
    const Sanitizer& GetSanitizer() const { return sanitizer_; }
    Sanitizer SnapshotSanitizer() const;
    SanitizeOptions SnapshotSanitizeOptions() const;
    const ReadOnlyRequestPolicy& Policy() const { return policy_; }
    PolicyEnforcingTransport& GuardedTransport() { return *guarded_transport_; }
    AuthClient& Auth() { return *auth_; }
    TdaTdsClient& TdaClient() { return *tda_client_; }
    TecGraphQlClient& GraphQlClient() { return *graphql_client_; }
    PublicGuiaHorariosClient& PublicGuiaClient() { return *public_guia_client_; }
    OfflineMaterialIndex& MaterialIndex() { return *material_index_; }

private:
    void RegisterSanitizerNeedles(const Credentials& creds);

    ServiceConfig config_;
    std::unique_ptr<IHttpTransport> raw_transport_;
    ReadOnlyRequestPolicy policy_;
    std::unique_ptr<PolicyEnforcingTransport> guarded_transport_;
    std::unique_ptr<AuthClient> auth_;
    mutable std::mutex sanitizer_mutex_;
    Sanitizer sanitizer_;
    std::unique_ptr<TdaTdsClient> tda_client_;
    std::unique_ptr<TecGraphQlClient> graphql_client_;
    std::unique_ptr<PublicGuiaHorariosClient> public_guia_client_;
    std::unique_ptr<OfflineMaterialIndex> material_index_;
};

}  // namespace academic::tec_digital
