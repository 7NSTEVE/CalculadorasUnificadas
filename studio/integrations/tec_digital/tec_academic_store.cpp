#include "tec_academic_store.hpp"

#include "../../core/cudoc_storage.hpp"
#include "../../core/utf_conv.hpp"
#include "../../pdf/pdf_writer.hpp"
#include "academic/tec_digital/vault.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;

namespace DocStudio::TecDigital {

namespace {

using json = nlohmann::json;
namespace td = academic::tec_digital;
namespace ht = academic::horarios_tec;

std::string SafeLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

std::string Round1(double v) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%.1f", v);
    return buf;
}

std::string Round2(double v) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

DocStudio::AnnotationObject MakeTextBoxObject(
    DocStudio::DocumentModel& doc,
    double x, double y, double w, double h,
    const std::string& textUtf8,
    double fontPt,
    bool bold,
    DocStudio::DocColor fg,
    DocStudio::DocColor fill) {
    DocStudio::AnnotationObject obj{};
    obj.id = doc.AllocateObjectId();
    obj.zOrder = doc.AllocateZOrder();
    obj.kind = DocStudio::ObjectKind::TextBox;
    obj.bounds = { x, y, w, h };
    obj.textUtf8 = textUtf8;
    obj.fontFamilyUtf8 = "Segoe UI";
    obj.fontSizePt = fontPt;
    obj.bold = bold;
    obj.strokeColor = fg;
    obj.fillColor = fill;
    obj.layerId = doc.ActiveLayerId();
    return obj;
}

void PlaceObjectInActiveDoc(DocStudio::DocumentModel& doc, DocStudio::AnnotationObject obj) {
    if (doc.Mode() == DocStudio::DocumentMode::InfiniteCanvas) {
        doc.AddCanvasObject(std::move(obj));
    } else {
        if (doc.Pages().empty()) {
            doc.ResetToBlank(DocStudio::DocumentMode::PaginatedPdf);
        }
        doc.AddPageObject(0, std::move(obj));
    }
}

bool IsCoursePassedInProfile(const td::StudentCurriculumProfile& prof, const std::string& code) {
    std::string target = SafeLower(code);
    for (const auto& g : prof.grades) {
        if (SafeLower(g.id_course) != target) continue;
        std::string bs = SafeLower(g.best_status);
        if (bs == "1" || bs.find("aprob") != std::string::npos || bs.find("reconoc") != std::string::npos ||
            bs.find("sufic") != std::string::npos) {
            return true;
        }
        for (const auto& att : g.grades_record) {
            if (att.state == 1 || att.grade >= 67.5) return true;
        }
    }
    return false;
}

}  // namespace

std::string TecAcademicStore::DefaultStorageRootUtf8() {
#ifdef _WIN32
    PWSTR appData = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appData)) && appData) {
        std::wstring ws(appData);
        CoTaskMemFree(appData);
        fs::path root = fs::path(ws) / L"CuadernU" / L"tec_digital";
        return WideToUtf8(root.wstring());
    }
#endif
    return (fs::temp_directory_path() / "CuadernU" / "tec_digital").string();
}

TecAcademicStore::TecAcademicStore(
    std::string storageRootUtf8,
    std::unique_ptr<td::IHttpTransport> transportOverride,
    td::ServiceConfig serviceConfigOverride) {
    storageRootUtf8_ = storageRootUtf8.empty() ? DefaultStorageRootUtf8() : std::move(storageRootUtf8);
    fs::path rootPath = fs::path(Utf8ToWide(storageRootUtf8_));
    std::error_code ec;
    fs::create_directories(rootPath / L"cache", ec);
    fs::create_directories(rootPath / L"offline", ec);

    offlineFilesRootUtf8_ = WideToUtf8((rootPath / L"offline").wstring());
    snapshotCachePathUtf8_ = WideToUtf8((rootPath / L"cache" / L"academic_snapshot.json").wstring());
    vaultPathUtf8_ = WideToUtf8((rootPath / L"tec_credentials.dpapi").wstring());

    td::ServiceConfig cfg = std::move(serviceConfigOverride);
    if (cfg.base_url.empty()) cfg.base_url = "https://tecdigital.tec.ac.cr";
    if (cfg.offline_root.empty()) cfg.offline_root = offlineFilesRootUtf8_;
    if (cfg.vault_config.vault_path.empty()) {
        cfg.vault_config.mode = td::VaultPersistenceMode::EncryptedVault;
        cfg.vault_config.vault_path = vaultPathUtf8_;
        cfg.vault_config.session_meta_path = WideToUtf8((rootPath / L"cache" / L"session_meta.json").wstring());
        cfg.vault_config.persist_session_cookies = true;
    }

    service_ = std::make_unique<td::TecDigitalService>(std::move(cfg), std::move(transportOverride));
    LoadSnapshotFromDisk(nullptr);
}

TecAcademicStore::~TecAcademicStore() = default;

td::AuthResult TecAcademicStore::LoginWithPassword(
    const std::string& emailUtf8,
    const std::string& passwordUtf8,
    bool persistEncryptedVault) {
    td::Credentials creds;
    creds.email = emailUtf8;
    creds.password = passwordUtf8;
    auto res = service_->LoginWithCredentials(creds, persistEncryptedVault);
    td::CredentialVault::ClearMemory(creds);
    authenticated_.store(res.ok, std::memory_order_release);
    if (res.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.sessionAuthenticated = true;
    }
    return res;
}

td::AuthResult TecAcademicStore::TryRestoreVaultSession() {
    auto res = service_->LoginWithEnvOrVault(false);
    authenticated_.store(res.ok, std::memory_order_release);
    if (res.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.sessionAuthenticated = true;
    }
    return res;
}

td::AuthResult TecAcademicStore::LoginWithQr(const std::string& emailUtf8) {
    auto res = service_->AttemptQrScanning(emailUtf8);
    authenticated_.store(res.ok, std::memory_order_release);
    if (res.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.sessionAuthenticated = true;
    }
    return res;
}

void TecAcademicStore::LogoutLocalOnly(bool deleteLocalVaultFile) {
    // Strictly local cleanup: NEVER sends /register/logout so browser sessions stay intact
    service_->Auth().Cookies().Clear();
    service_->GraphQlClient().ClearSessionToken();
    service_->TdaClient().ClearCachedCarne();
    authenticated_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.sessionAuthenticated = false;
    }
    if (deleteLocalVaultFile && !vaultPathUtf8_.empty()) {
        std::error_code ec;
        fs::remove(fs::path(Utf8ToWide(vaultPathUtf8_)), ec);
    }
}

bool TecAcademicStore::SyncAll(
    const TecSyncOptions& opts,
    ProgressCallback onProgress,
    std::atomic<bool>* cancelFlag,
    std::string* outErr) {
    auto report = [&](int pct, const std::string& msg) {
        if (onProgress) onProgress(pct, msg);
    };
    auto cancelled = [&]() -> bool {
        return cancelFlag != nullptr && cancelFlag->load(std::memory_order_acquire);
    };
    auto politePause = [&]() {
        if (opts.politeDelayMs <= 0) return;
        int rem = opts.politeDelayMs;
        while (rem > 0 && !cancelled()) {
            const int step = std::min(rem, 40);
            std::this_thread::sleep_for(std::chrono::milliseconds(step));
            rem -= step;
        }
    };

    TecAcademicSnapshot next;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        next.foldersByPath = snapshot_.foldersByPath; // Preserve previously discovered course files
    }
    next.syncedAtUtc = td::CredentialVault::CurrentUtcIso8601();
    next.sessionAuthenticated = authenticated_.load(std::memory_order_acquire);

    if (cancelled()) {
        if (outErr) *outErr = "Sincronización cancelada por el usuario.";
        return false;
    }

    auto& client = service_->TdaClient();
    const std::string sessionCarne = client.ResolveStudentCarne();

    // 1. Communities & Courses
    if (opts.syncCommunities) {
        report(10, "Consultando cursos y comunidades...");
        auto r = service_->FetchCoursesAndCommunities(false);
        if (r.ok) {
            next.communities = std::move(r.value);
            next.sessionAuthenticated = true;
            authenticated_.store(true, std::memory_order_release);
        } else if (r.error_code == "session_expired") {
            authenticated_.store(false, std::memory_order_release);
            if (outErr) *outErr = "Sesión expirada. Inicie sesión nuevamente.";
            return false;
        } else {
            next.warnings.push_back("cursos: " + r.error_message);
        }
    }
    if (cancelled()) return false;

    // 2. Expediente & Curriculum
    if (opts.syncExpediente || opts.syncCurriculum) {
        report(28, "Consultando expediente académico y malla curricular...");
        td::TdaTdsCatalogOptions catOpts;
        catOpts.carne = sessionCarne;
        catOpts.fetch_curriculum = opts.syncCurriculum;
        catOpts.fetch_expediente = opts.syncExpediente;
        catOpts.fetch_notifications = opts.syncNotifications;
        catOpts.fetch_evaluations = false;
        catOpts.fetch_news_rss = false;
        catOpts.fetch_inclusion_key = opts.syncInclusionProbe;
        catOpts.fetch_prematricula = false;
        catOpts.fetch_gestor_inscripcion = true;
        catOpts.fetch_feria_negocios = true;

        auto cr = service_->FetchTdaTdsCatalog(catOpts);
        if (cr.ok) {
            auto& cat = cr.value;
            next.curriculumSedes = std::move(cat.sedes);
            next.curriculumCarreras = std::move(cat.carreras);
            next.curriculumPlanes = std::move(cat.planes);
            next.curriculumColors = std::move(cat.color_codes);
            next.studentCareer = std::move(cat.student_career);
            next.curriculumGraph = std::move(cat.curriculum_graph);
            next.studentProfile = std::move(cat.student_profile);
            next.expediente = std::move(cat.expediente);
            if (next.expediente && !next.expediente->structured.graduate_attributes.empty()) {
                next.graduateAttributes = next.expediente->structured.graduate_attributes;
            }
            next.notifications = std::move(cat.notifications);
            next.feriaNegocios = std::move(cat.feria_negocios);
            next.gestorInscripcionSelectorHtml = std::move(cat.gestor_inscripcion_selector_html);
            next.gestorInscripcionEventsJson = std::move(cat.gestor_inscripcion_events_json);
            next.inclusionServiceStatus = cat.inclusion_transaction_key.empty()
                                              ? "service_unavailable"
                                              : "active";
            for (const auto& w : cat.warnings) next.warnings.push_back(w);
        } else if (cr.error_code == "session_expired") {
            authenticated_.store(false, std::memory_order_release);
            if (outErr) *outErr = "Sesión expirada consultando expediente.";
            return false;
        } else {
            next.warnings.push_back("catalogo: " + cr.error_message);
        }

        if (opts.syncGraduateAttributes && next.graduateAttributes.empty() && !cancelled()) {
            if (auto ga = client.GetGraduateAttributes(sessionCarne); ga.ok) {
                next.graduateAttributes = std::move(ga.value);
            }
        }

        if (opts.syncExpediente) {
            std::string activePer = opts.activePeriodHint;
            if (activePer.empty() && next.expediente && !next.expediente->periodos_combo.empty()) {
                activePer = next.expediente->periodos_combo.front().key;
            }
            if (auto sr = client.GetCurrentPeriodSchedule(
                    td::ConvertPeriodKeyToExpedienteFormat(activePer), sessionCarne); sr.ok) {
                next.currentSchedule = std::move(sr.value);
            }
        }
    }
    if (cancelled()) return false;

    // 3. Per-Course Overviews, Detailed Evaluations, RSS & SMS (capped for politeness)
    int deepCount = 0;
    std::vector<td::DetailedEvaluationsReport> allEvalReports;
    for (const auto& comm : next.communities) {
        if (cancelled()) return false;
        if (comm.type != "CLASE") continue;
        if (!opts.activePeriodHint.empty() && comm.period != opts.activePeriodHint) continue;
        if (deepCount >= opts.maxActiveCoursesToDeepSync) break;
        ++deepCount;

        int pct = 40 + std::min(35, deepCount * 6);
        report(pct, "Sincronizando curso: " + comm.name);

        if (opts.syncCourseOverviews) {
            politePause();
            if (auto ov = client.GetCourseOverview(comm.url); ov.ok) {
                if (opts.syncCustomTabs && !cancelled()) {
                    auto tabLinks = client.CrawlCustomCommunityTabs(
                        comm.url, ov.value.tabs, opts.maxCustomTabsPerSpace, opts.politeDelayMs);
                    if (tabLinks.ok && !tabLinks.value.empty()) {
                        next.customTabResourcesByCourse[comm.url] = std::move(tabLinks.value);
                    }
                }
                next.overviewsByPath[comm.url] = std::move(ov.value);
            }
        }
        if (opts.syncEvaluations && !cancelled()) {
            politePause();
            if (auto ev = client.GetDetailedCourseEvaluations(comm.url); ev.ok) {
                allEvalReports.push_back(ev.value);
                next.evaluationsByPath[comm.url] = std::move(ev.value);
            }
        }
        if (opts.syncNewsRss && !cancelled()) {
            politePause();
            if (auto nr = client.GetCommunityNewsRss(comm.url); nr.ok) {
                next.newsByPath[comm.url] = std::move(nr.value);
            }
        }
        if (opts.syncSms && !cancelled()) {
            politePause();
            if (auto sm = client.GetCourseSmsMessages(comm.url); sm.ok && !sm.value.empty()) {
                next.smsByPath[comm.url] = std::move(sm.value);
            }
        }
        if (opts.syncForums && !cancelled()) {
            politePause();
            if (auto fr = client.GetCommunityForums(comm.url); fr.ok && !fr.value.empty()) {
                next.forumsByPath[comm.url] = std::move(fr.value);
            }
        }
        if (opts.syncMembers && !cancelled()) {
            politePause();
            if (auto mb = client.GetCommunityMembers(comm.url, comm.community_id, 200); mb.ok && !mb.value.empty()) {
                next.membersByPath[comm.url] = std::move(mb.value);
            }
        }
        if (opts.syncGaap && !cancelled()) {
            politePause();
            if (auto gp = client.GetGaapActivityHistory(comm.url); gp.ok && !gp.value.empty()) {
                next.gaapByPath[comm.url] = std::move(gp.value);
            }
        }
    }
    if (cancelled()) return false;

    // 4. Unified Calendar (Portal Calendar + Evaluations Deadlines + Matricula Appointments)
    if (opts.syncCalendar) {
        report(80, "Construyendo calendario académico unificado...");
        if (auto cal = client.GetCalendarEvents("", "list"); cal.ok) {
            next.portalCalendarEvents = std::move(cal.value);
        }
        std::vector<td::MatriculaAppointment> citas;
        if (next.expediente) citas = next.expediente->structured.citas_matricula;
        next.unifiedCalendarEvents = td::BuildUnifiedCalendarEvents(
            next.portalCalendarEvents, allEvalReports, citas);
    }

    // 5. Secondary Modules (Subscriptions, Control de Plagio, School Catalog, User Model)
    if (opts.syncNotifications) {
        if (auto subs = client.GetNotificationSubscriptions(); subs.ok) {
            next.notificationSubscriptions = std::move(subs.value);
        }
    }
    if (opts.syncPlagiarismTree) {
        report(88, "Consultando árbol de Control de Plagio...");
        if (auto pt = client.GetPlagiarismTree(); pt.ok) {
            next.plagiarismTree = std::move(pt.value);
        }
    }
    if (opts.syncSchoolCatalog) {
        if (auto sc = client.GetSchoolCatalog(); sc.ok) {
            next.schoolCatalog = std::move(sc.value);
        }
    }
    if (opts.syncUserModel) {
        if (auto um = client.GetUserModelSnapshot(); um.ok) {
            next.userModel = std::move(um.value);
        }
    }
    if (opts.syncPerformanceEval && !cancelled()) {
        report(92, "Consultando evaluación de desempeño docente...");
        if (auto pe = client.GetPerformanceEvaluationStatus(); pe.ok) {
            next.performanceEvalForms = std::move(pe.value.forms);
            next.pendingPerformanceEvals = std::move(pe.value.pending_evaluations);
        }
    }
    if (opts.syncGraphQlInclusiones && !cancelled()) {
        report(94, "Consultando expediente JSON, malla y graduación...");
        auto& gql = service_->GraphQlClient();
        auto roleRes = gql.FetchUserType();
        if (roleRes.ok) {
            next.inclusionServiceStatus = "active";
            const std::string uid = gql.CachedUserId();
            if (!sessionCarne.empty()) {
                if (auto gi = gql.FetchStudentGeneralInfo(sessionCarne); gi.ok) {
                    next.studentGeneralInfo = std::move(gi.value);
                }
                if (auto ap = gql.FetchActualPeriodCourses(sessionCarne); ap.ok) {
                    next.actualPeriodCourses = std::move(ap.value);
                }
                if (!next.curriculumGraph || next.curriculumGraph->levels.empty() || !next.studentProfile) {
                    if (auto cb = gql.FetchStudentCurriculum(sessionCarne); cb.ok) {
                        if (!next.curriculumGraph || next.curriculumGraph->levels.empty()) {
                            next.curriculumGraph = std::move(cb.value.graph);
                        }
                        if (!next.studentProfile || next.studentProfile->grades.empty()) {
                            next.studentProfile = std::move(cb.value.profile);
                        }
                    }
                }
                if (auto ah = gql.FetchAcademicHistoryFallback(sessionCarne); ah.ok) {
                    if (!next.expediente) {
                        next.expediente = td::ExpedienteSummary{};
                    }
                    auto& dst = next.expediente->structured.stats;
                    if (dst.weighted_avg_general <= 0.0 && ah.value.weighted_avg_general > 0.0) {
                        dst = ah.value;
                    }
                }
            }
            if (auto pg = gql.FetchPendingGraduationCourses(uid, sessionCarne); pg.ok) {
                next.pendingGraduationCourses = std::move(pg.value);
            }
            if (auto camp = gql.FetchStudentActiveCampaign(uid); camp.ok) {
                const std::string cid = camp.value.id;
                next.inclusionCampaign = std::move(camp.value);
                if (!cid.empty()) {
                    if (auto apps = gql.FetchStudentApplications(cid, sessionCarne); apps.ok) {
                        next.inclusionApplications = std::move(apps.value);
                    }
                    if (auto sch = gql.FetchScholarshipStatus(cid); sch.ok) {
                        next.scholarshipInfo = std::move(sch.value);
                    }
                }
            }
        }
    }

    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_ = std::move(next);
    }
    SaveSnapshotToDisk(nullptr);
    report(100, "Sincronización solo lectura completada.");
    return true;
}

bool TecAcademicStore::SyncCourseFiles(
    const std::string& communityUrlOrPath,
    bool downloadFiles,
    std::string* outSummaryUtf8,
    std::string* outErr) {
    if (downloadFiles && ComputeOfflineCacheBytes() > offlineBudgetBytes_) {
        PruneOfflineCacheOlderThanBytes(offlineBudgetBytes_ * 8 / 10);
    }
    td::SyncFolderStorageOptions opts;
    opts.community_urls_or_slugs.push_back(communityUrlOrPath);
    opts.download_files = downloadFiles;
    opts.build_material_index = downloadFiles;
    auto res = service_->SyncFolderStorage(opts);
    if (!res.ok) {
        if (outErr) *outErr = res.error_message;
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        td::FolderWalkResult fwr;
        fwr.folders = res.value.folders;
        fwr.files = res.value.files;
        fwr.url_links = res.value.url_links;
        fwr.session_expired = res.value.session_expired;
        snapshot_.foldersByPath[communityUrlOrPath] = std::move(fwr);
    }
    SaveSnapshotToDisk(nullptr);
    if (outSummaryUtf8) {
        *outSummaryUtf8 = "Carpetas: " + std::to_string(res.value.folders_discovered) +
                          " · Archivos: " + std::to_string(res.value.files_discovered) +
                          " · Descargados: " + std::to_string(res.value.files_downloaded) +
                          " · Deduplicados: " + std::to_string(res.value.files_deduplicated);
    }
    return true;
}

bool TecAcademicStore::DownloadSingleCourseFile(
    const std::string& communityUrlOrPath,
    const std::string& fileObjectId,
    std::string* outLocalPathUtf8,
    std::string* outErr) {
    td::FileRecord targetRec;
    bool found = false;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        auto it = snapshot_.foldersByPath.find(communityUrlOrPath);
        if (it != snapshot_.foldersByPath.end()) {
            for (const auto& f : it->second.files) {
                if (f.object_id == fileObjectId) {
                    targetRec = f;
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found) {
        if (outErr) *outErr = "Archivo no encontrado en el árbol sincronizado del curso.";
        return false;
    }
    if (ComputeOfflineCacheBytes() > offlineBudgetBytes_) {
        PruneOfflineCacheOlderThanBytes(offlineBudgetBytes_ * 8 / 10);
    }
    td::StorageDownloader downloader(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->Config().downloader_options,
        service_->SnapshotSanitizeOptions());
    auto dr = downloader.DownloadFile(targetRec, offlineFilesRootUtf8_, communityUrlOrPath);
    if (dr.outcome != td::DownloadOutcome::Downloaded && dr.outcome != td::DownloadOutcome::Deduplicated) {
        if (outErr) *outErr = dr.error_message.empty() ? "No se pudo descargar el archivo." : dr.error_message;
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        auto it = snapshot_.foldersByPath.find(communityUrlOrPath);
        if (it != snapshot_.foldersByPath.end()) {
            for (auto& f : it->second.files) {
                if (f.object_id == fileObjectId) {
                    f = targetRec;
                    break;
                }
            }
        }
    }
    service_->MaterialIndex().SetOfflineRoot(offlineFilesRootUtf8_);
    service_->MaterialIndex().IndexDirectory(offlineFilesRootUtf8_);
    SaveSnapshotToDisk(nullptr);
    if (outLocalPathUtf8) {
        std::string rel = !targetRec.logical_rel_path.empty() ? targetRec.logical_rel_path : targetRec.blob_rel_path;
        if (!rel.empty()) {
            *outLocalPathUtf8 = WideToUtf8((fs::path(Utf8ToWide(offlineFilesRootUtf8_)) / fs::path(Utf8ToWide(rel))).wstring());
        }
    }
    return true;
}

td::Result<std::vector<td::CarreraInfo>> TecAcademicStore::FetchCurriculumCarreras(const std::string& sedeKey) {
    auto r = service_->TdaClient().GetCurriculumCarreras(sedeKey);
    if (r.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.curriculumCarreras = r.value;
    }
    return r;
}

td::Result<std::vector<td::PlanInfo>> TecAcademicStore::FetchCurriculumPlanes(
    const std::string& sedeKey, const std::string& carreraKey) {
    auto r = service_->TdaClient().GetCurriculumPlanes(sedeKey, carreraKey);
    if (r.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.curriculumPlanes = r.value;
    }
    return r;
}

bool TecAcademicStore::LoadCurriculumPlan(const std::string& planKey, std::string* outErr) {
    auto gr = service_->TdaClient().GetCurriculumGraph(planKey);
    if (!gr.ok) {
        if (outErr) *outErr = gr.error_message;
        return false;
    }
    auto pr = service_->TdaClient().GetStudentCurriculumProfile(planKey, "", 0);
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.curriculumGraph = std::move(gr.value);
        if (pr.ok) snapshot_.studentProfile = std::move(pr.value);
    }
    SaveSnapshotToDisk(nullptr);
    return true;
}

td::Result<std::vector<td::CarreraInfo>> TecAcademicStore::FetchGuiaHorarioCarreras(const std::string& sedeKey) {
    auto r = service_->TdaClient().GetGuiaHorarioCarreras(sedeKey);
    if (r.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (snapshot_.expediente) snapshot_.expediente->carreras_combo = r.value;
    }
    return r;
}

td::Result<td::RubricBreakdown> TecAcademicStore::FetchRubric(
    const std::string& rubricId,
    const std::string& communityId,
    const std::string& taskItemId,
    const std::string& taskId) {
    return service_->TdaClient().GetRubricBreakdown(rubricId, communityId, taskItemId, taskId);
}

td::Result<std::vector<td::FileVersionRecord>> TecAcademicStore::FetchFileVersions(
    const std::string& objectId) {
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto res = walker.FetchFileVersions(objectId);
    if (res.ok) {
        {
            std::lock_guard<std::mutex> lk(snapMutex_);
            snapshot_.fileVersionsByObjectId[objectId] = res.value;
        }
        SaveSnapshotToDisk(nullptr);
    }
    return res;
}

td::Result<std::vector<td::EvalSubmissionRevision>> TecAcademicStore::FetchEvaluationHistory(
    const std::string& communityUrl,
    const std::string& assignId,
    const std::string& taskItemId,
    const std::string& taskId,
    const std::string& taskName) {
    auto& client = service_->TdaClient();
    std::vector<td::EvalSubmissionRevision> merged;
    auto hist = client.GetEvaluationSubmissionHistory(communityUrl, taskItemId, "", taskName);
    if (hist.ok) {
        merged = std::move(hist.value);
    }
    auto live = client.GetEvaluationLiveRevision(communityUrl, taskItemId, "", taskName, taskId);
    if (live.ok && live.value.has_value()) {
        bool already = false;
        for (const auto& r : merged) {
            if (!live.value->revision_id.empty() && r.revision_id == live.value->revision_id) {
                already = true;
                break;
            }
        }
        if (!already) merged.insert(merged.begin(), *live.value);
    }
    if (!hist.ok && !live.ok) {
        return td::Result<std::vector<td::EvalSubmissionRevision>>::Failure(
            hist.error_code, hist.error_message);
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.evalHistoryByAssignId[assignId.empty() ? taskItemId : assignId] = merged;
    }
    SaveSnapshotToDisk(nullptr);
    return td::Result<std::vector<td::EvalSubmissionRevision>>::Success(std::move(merged));
}

td::Result<std::vector<td::ForumMessagePost>> TecAcademicStore::FetchForumThread(
    const std::string& communityUrl,
    const std::string& messageId) {
    return service_->TdaClient().GetForumThread(communityUrl, messageId);
}

td::Result<std::vector<td::CourseRequisitesCheck>>
TecAcademicStore::ValidateScheduleCoursesWithGraphQL(const std::vector<std::string>& courseCodes,
                                                       const std::string& periodKey) {
    std::vector<td::CourseRequisitesCheck> results;
    auto& gql = service_->GraphQlClient();
    const std::string carne = service_->TdaClient().ResolveStudentCarne();
    std::string uid = gql.CachedUserId();
    std::string campId;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (snapshot_.inclusionCampaign) campId = snapshot_.inclusionCampaign->id;
    }
    if (campId.empty()) {
        if (auto c = gql.FetchStudentActiveCampaign(uid); c.ok) {
            campId = c.value.id;
            uid = gql.CachedUserId();
        }
    }
    const std::string periodNorm = td::ConvertPeriodKeyToExpedienteFormat(periodKey);
    for (const auto& code : courseCodes) {
        if (code.empty()) continue;
        auto r = gql.CheckCourseRequisites(uid, carne, campId, code, code, periodNorm);
        if (r.ok) {
            results.push_back(std::move(r.value));
        } else {
            return td::Result<std::vector<td::CourseRequisitesCheck>>::Failure(
                r.error_code, r.error_message);
        }
    }
    return td::Result<std::vector<td::CourseRequisitesCheck>>::Success(std::move(results));
}

namespace {

std::size_t FilterCatalogEligibleByCurriculum(
    ht::CourseCatalog& catalog,
    const td::CurriculumGraph& graph,
    const td::StudentCurriculumProfile& prof,
    const std::function<bool(const td::StudentCurriculumProfile&, const std::string&)>& isPassedFn) {
    std::size_t filteredOut = 0;
    std::map<std::string, td::CurriculumCourseNode> nodeByCode;
    for (const auto& lv : graph.levels) {
        for (const auto& c : lv.courses) {
            nodeByCode[SafeLower(c.id_course)] = c;
        }
    }
    std::vector<ht::Course> eligible;
    for (auto& course : catalog.courses) {
        if (isPassedFn(prof, course.codeUtf8)) {
            ++filteredOut;
            continue;
        }
        auto nit = nodeByCode.find(SafeLower(course.codeUtf8));
        bool prereqsMet = true;
        if (nit != nodeByCode.end()) {
            course.prerequisitesUtf8 = nit->second.requirements;
            course.corequisitesUtf8 = nit->second.co_requirements;
            for (const auto& req : nit->second.requirements) {
                if (!isPassedFn(prof, req)) {
                    prereqsMet = false;
                    break;
                }
            }
        }
        if (prereqsMet) {
            eligible.push_back(std::move(course));
        } else {
            ++filteredOut;
        }
    }
    if (!eligible.empty()) {
        catalog.courses = std::move(eligible);
    }
    return filteredOut;
}

std::vector<ht::BusySlot> CollectEnrolledBusySlots(
    const std::vector<td::WeeklyScheduleCell>& currentSchedule) {
    std::vector<ht::BusySlot> busySlots;
    for (const auto& cell : currentSchedule) {
        if (cell.start_minute < cell.end_minute &&
            cell.day_index_0_mon >= 0 && cell.day_index_0_mon <= 6) {
            ht::BusySlot b;
            b.day = static_cast<ht::DayOfWeek>(cell.day_index_0_mon);
            b.startMinute = cell.start_minute;
            b.endMinute = cell.end_minute;
            b.labelUtf8 = cell.course_code.empty() ? cell.raw_text : cell.course_code;
            b.sourceUtf8 = "enrolled";
            busySlots.push_back(std::move(b));
        }
    }
    return busySlots;
}

ht::CourseCatalog MergeCourseCatalogs(const ht::CourseCatalog& base,
                                      const ht::CourseCatalog& incoming) {
    ht::CourseCatalog combined = base;
    if (combined.titleUtf8.empty()) combined.titleUtf8 = incoming.titleUtf8;
    if (combined.termUtf8.empty()) combined.termUtf8 = incoming.termUtf8;
    if (combined.campusUtf8.empty()) combined.campusUtf8 = incoming.campusUtf8;
    else if (!incoming.campusUtf8.empty() && SafeLower(combined.campusUtf8) != SafeLower(incoming.campusUtf8)) {
        combined.campusUtf8.clear();
    }

    for (const auto& inCourse : incoming.courses) {
        auto cit = std::find_if(combined.courses.begin(), combined.courses.end(),
                                [&](const ht::Course& existing) {
                                    return SafeLower(existing.codeUtf8) == SafeLower(inCourse.codeUtf8);
                                });
        if (cit == combined.courses.end()) {
            combined.courses.push_back(inCourse);
            continue;
        }
        if (cit->nameUtf8.empty() && !inCourse.nameUtf8.empty()) cit->nameUtf8 = inCourse.nameUtf8;
        if (cit->credits <= 0 && inCourse.credits > 0) cit->credits = inCourse.credits;
        for (const auto& inGrp : inCourse.groups) {
            bool exactDup = false;
            std::set<std::string> existingIds;
            for (const auto& eg : cit->groups) {
                existingIds.insert(SafeLower(eg.idUtf8));
                if (SafeLower(eg.idUtf8) == SafeLower(inGrp.idUtf8) &&
                    SafeLower(eg.campusUtf8) == SafeLower(inGrp.campusUtf8) &&
                    SafeLower(eg.modalityUtf8) == SafeLower(inGrp.modalityUtf8) &&
                    eg.sessions.size() == inGrp.sessions.size()) {
                    exactDup = true;
                    break;
                }
            }
            if (exactDup) continue;
            ht::Group gCopy = inGrp;
            std::string baseId = gCopy.idUtf8.empty() ? "1" : gCopy.idUtf8;
            std::string cand = baseId;
            int seq = 2;
            while (existingIds.count(SafeLower(cand)) > 0) {
                cand = baseId + "#" + std::to_string(seq++);
            }
            gCopy.idUtf8 = std::move(cand);
            cit->groups.push_back(std::move(gCopy));
        }
    }
    return combined;
}

}  // namespace

LiveScheduleSolveResult TecAcademicStore::FetchAndSolveLiveSchedule(
    const std::string& sedeKey,
    const std::string& carreraKey,
    const std::string& periodoKey,
    const std::string& campusLabel,
    const ht::SolverPreferences& prefs,
    bool filterEligibleByCurriculum,
    bool includeUnifiedCalendarBusySlots) {
    LiveScheduleSolveResult out;

    const std::string expPeriod = td::ConvertPeriodKeyToExpedienteFormat(periodoKey);
    auto rowsRes = service_->TdaClient().GetGuiaHorarioRows(sedeKey, carreraKey, expPeriod);
    if (!rowsRes.ok) {
        out.errorUtf8 = rowsRes.error_message;
        return out;
    }
    out.rawRows = std::move(rowsRes.value);
    if (out.rawRows.empty()) {
        out.errorUtf8 = "No se encontraron cursos publicados para la combinación Sede/Carrera/Periodo seleccionada.";
        return out;
    }

    const std::string catalogJson = td::GuiaHorarioRowsToHorariosTecJson(
        out.rawRows, "Guía de Horarios TEC", periodoKey, campusLabel);
    auto parsed = ht::ParseCatalogJson(catalogJson);
    if (!parsed.ok) {
        out.errorUtf8 = parsed.errorUtf8;
        return out;
    }
    out.catalog = std::move(parsed.catalog);

    std::optional<td::CurriculumGraph> graphCopy;
    std::optional<td::StudentCurriculumProfile> profCopy;
    std::vector<td::WeeklyScheduleCell> schedCopy;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (filterEligibleByCurriculum) {
            graphCopy = snapshot_.curriculumGraph;
            profCopy = snapshot_.studentProfile;
        }
        if (includeUnifiedCalendarBusySlots) {
            schedCopy = snapshot_.currentSchedule;
        }
    }
    if (filterEligibleByCurriculum && graphCopy && profCopy) {
        out.filteredOutByPrereqsCount = FilterCatalogEligibleByCurriculum(
            out.catalog, *graphCopy, *profCopy, IsCoursePassedInProfile);
    }

    const std::vector<ht::BusySlot> busySlots =
        includeUnifiedCalendarBusySlots ? CollectEnrolledBusySlots(schedCopy) : std::vector<ht::BusySlot>{};

    out.solved = ht::HorariosSolver::Solve(out.catalog, busySlots, prefs);
    out.ok = true;
    return out;
}

td::Result<std::vector<td::PublicGuiaEscuelaInfo>> TecAcademicStore::FetchPublicGuiaEscuelas() {
    auto res = service_->PublicGuiaClient().FetchEscuelas();
    if (res.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.publicGuiaEscuelas = res.value;
    }
    return res;
}

td::Result<std::vector<td::PublicGuiaModalidadInfo>> TecAcademicStore::FetchPublicGuiaModalidades() {
    return service_->PublicGuiaClient().FetchModalidades();
}

LiveScheduleSolveResult TecAcademicStore::FetchAndSolvePublicGuiaSchedule(
    const PublicGuiaQuery& query,
    const ht::SolverPreferences& prefs,
    bool filterEligibleByCurriculum,
    bool includeUnifiedCalendarBusySlots,
    const ht::CourseCatalog* mergeIntoCatalog) {
    LiveScheduleSolveResult out;

    td::Result<std::vector<td::PublicGuiaSessionRow>> rowsRes;
    if (query.mode == PublicGuiaMode::Verano) {
        const int yr = (query.year >= 1990 && query.year <= 2100) ? query.year : 2026;
        rowsRes = service_->PublicGuiaClient().FetchVeranoByYear(yr);
    } else if (query.mode == PublicGuiaMode::Carnet) {
        std::string targetCarnet = query.carnet;
        if (targetCarnet.empty() && IsAuthenticated()) {
            targetCarnet = service_->TdaClient().ResolveStudentCarne();
        }
        if (targetCarnet.empty()) {
            out.errorUtf8 = "Ingrese un número de carné o inicie sesión en TEC Digital.";
            return out;
        }
        rowsRes = service_->PublicGuiaClient().FetchEstudianteByCarnet(targetCarnet, query.year);
    } else {
        if (query.escuelaCode.empty()) {
            out.errorUtf8 = "Seleccione una escuela para consultar la Guía de Horarios Pública.";
            return out;
        }
        const int yr = (query.year >= 1990 && query.year <= 2100) ? query.year : 2026;
        rowsRes = service_->PublicGuiaClient().FetchEscuelaByYear(query.escuelaCode, yr);
    }

    if (!rowsRes.ok) {
        out.errorUtf8 = rowsRes.error_message;
        return out;
    }
    if (rowsRes.value.empty()) {
        if (query.mode == PublicGuiaMode::Verano) {
            out.errorUtf8 = "No se encontraron horarios publicados de Verano para el año seleccionado.";
        } else if (query.mode == PublicGuiaMode::Carnet) {
            out.errorUtf8 = "No se encontraron horarios públicos para el carné consultado.";
        } else {
            out.errorUtf8 = "No se encontraron horarios publicados para esa escuela en el año seleccionado.";
        }
        return out;
    }

    out.rawRows = td::PublicGuiaRowsToGuiaHorarioRows(rowsRes.value, query.filter);
    if (out.rawRows.empty()) {
        out.errorUtf8 = "No hubo coincidencias tras aplicar los filtros de Sede o Periodo (sesiones sin filtrar: " +
                        std::to_string(rowsRes.value.size()) + ").";
        return out;
    }

    const std::string catalogJson = td::PublicGuiaRowsToHorariosTecJson(
        rowsRes.value, query.filter, "Guía de Horarios Pública TEC");
    auto parsed = ht::ParseCatalogJson(catalogJson);
    if (!parsed.ok) {
        out.errorUtf8 = parsed.errorUtf8;
        return out;
    }
    if (parsed.catalog.courses.empty()) {
        out.errorUtf8 = "No se pudieron construir cursos válidos desde la Guía de Horarios Pública.";
        return out;
    }

    out.catalog = (mergeIntoCatalog != nullptr && !mergeIntoCatalog->courses.empty())
                      ? MergeCourseCatalogs(*mergeIntoCatalog, parsed.catalog)
                      : std::move(parsed.catalog);

    std::optional<td::CurriculumGraph> graphCopy;
    std::optional<td::StudentCurriculumProfile> profCopy;
    std::vector<td::WeeklyScheduleCell> schedCopy;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (filterEligibleByCurriculum) {
            graphCopy = snapshot_.curriculumGraph;
            profCopy = snapshot_.studentProfile;
        }
        if (includeUnifiedCalendarBusySlots) {
            schedCopy = snapshot_.currentSchedule;
        }
    }
    if (filterEligibleByCurriculum && graphCopy && profCopy) {
        out.filteredOutByPrereqsCount = FilterCatalogEligibleByCurriculum(
            out.catalog, *graphCopy, *profCopy, IsCoursePassedInProfile);
    }

    const std::vector<ht::BusySlot> busySlots =
        includeUnifiedCalendarBusySlots ? CollectEnrolledBusySlots(schedCopy) : std::vector<ht::BusySlot>{};

    out.solved = ht::HorariosSolver::Solve(out.catalog, busySlots, prefs);
    out.ok = true;
    return out;
}

bool TecAcademicStore::SaveSnapshotToDisk(std::string* outErr) const {
    TecAcademicSnapshot snap = Snapshot();
    try {
        json j;
        j["version"] = 1;
        j["synced_at_utc"] = snap.syncedAtUtc;
        j["inclusion_service_status"] = snap.inclusionServiceStatus;

        json comms = json::array();
        for (const auto& c : snap.communities) {
            comms.push_back({
                {"community_id", c.community_id},
                {"slug", c.slug},
                {"name", c.name},
                {"type", c.type},
                {"department", c.department},
                {"period", c.period},
                {"url", c.url},
                {"root_folder_id", c.root_folder_id}
            });
        }
        j["communities"] = std::move(comms);

        json evals = json::object();
        for (const auto& [path, rep] : snap.evaluationsByPath) {
            json cats = json::array();
            for (const auto& cat : rep.categories) {
                json items = json::array();
                for (const auto& it : cat.items) {
                    items.push_back({
                        {"assign_id", it.assign_id},
                        {"name", it.name},
                        {"weight_pct", it.weight_pct},
                        {"earned_pct", it.earned_pct},
                        {"raw_grade_100", it.raw_grade_100},
                        {"is_graded", it.is_graded},
                        {"due_date_iso", it.due_date_iso},
                        {"due_date_text", it.due_date_text},
                        {"has_rubric", it.has_rubric},
                        {"rubric_id", it.rubric_id},
                        {"submission_status", it.submission_status}
                    });
                }
                cats.push_back({
                    {"name", cat.name},
                    {"weight_pct", cat.weight_pct},
                    {"earned_pct", cat.earned_pct},
                    {"items", std::move(items)}
                });
            }
            evals[path] = {
                {"community_slug", rep.community_slug},
                {"unrounded_total", rep.unrounded_total},
                {"final_grade", rep.final_grade},
                {"has_final_grade", rep.has_final_grade},
                {"categories", std::move(cats)}
            };
        }
        j["evaluations"] = std::move(evals);

        json cal = json::array();
        for (const auto& ev : snap.unifiedCalendarEvents) {
            cal.push_back({
                {"uid", ev.uid},
                {"date_iso", ev.date_iso},
                {"start_time", ev.start_time},
                {"end_time", ev.end_time},
                {"title", ev.title},
                {"calendar_name", ev.calendar_name},
                {"description", ev.description},
                {"source_kind", ev.source_kind}
            });
        }
        j["unified_calendar"] = std::move(cal);

        if (snap.expediente) {
            const auto& st = snap.expediente->structured.stats;
            j["expediente_stats"] = {
                {"weighted_avg_general", st.weighted_avg_general},
                {"weighted_avg_passed", st.weighted_avg_passed},
                {"weighted_avg_failed", st.weighted_avg_failed},
                {"passed_pct", st.passed_pct},
                {"failed_pct", st.failed_pct},
                {"hours_taken", st.hours_taken},
                {"credits_passed", st.credits_passed},
                {"credits_failed", st.credits_failed},
                {"credits_total", st.credits_total},
                {"aprobadas", st.count_aprobadas},
                {"en_curso", st.count_en_curso},
                {"reprobadas", st.count_reprobadas},
                {"retiradas", st.count_retiradas},
                {"reconocidas", st.count_reconocidas},
                {"suficiencia", st.count_suficiencia},
                {"pendientes", st.count_pendientes},
                {"congeladas", st.count_congeladas}
            };
        }

        if (snap.curriculumGraph) {
            json cg;
            cg["dsc_curriculum"] = snap.curriculumGraph->dsc_curriculum;
            cg["id_curriculum"] = snap.curriculumGraph->id_curriculum;
            cg["modality"] = snap.curriculumGraph->modality;
            cg["academic_degree"] = snap.curriculumGraph->academic_degree;
            cg["first_level"] = snap.curriculumGraph->first_level;
            json lvs = json::array();
            for (const auto& lv : snap.curriculumGraph->levels) {
                json cs = json::array();
                for (const auto& c : lv.courses) {
                    cs.push_back({
                        {"id_course", c.id_course},
                        {"name", c.name},
                        {"truncated_name", c.truncated_name},
                        {"credits", c.credits},
                        {"hours", c.hours},
                        {"requirements", c.requirements},
                        {"co_requirements", c.co_requirements},
                        {"required_by", c.required_by}
                    });
                }
                lvs.push_back({{"id", lv.id}, {"courses", std::move(cs)}});
            }
            cg["levels"] = std::move(lvs);
            j["curriculum_graph"] = std::move(cg);
        }

        if (snap.studentProfile) {
            json grades = json::array();
            for (const auto& g : snap.studentProfile->grades) {
                json atts = json::array();
                for (const auto& a : g.grades_record) {
                    atts.push_back({
                        {"year", a.year},
                        {"period", a.period},
                        {"modality", a.modality},
                        {"state", a.state},
                        {"grade", a.grade},
                        {"group", a.group}
                    });
                }
                grades.push_back({
                    {"id_course", g.id_course},
                    {"best_status", g.best_status},
                    {"grades_record", std::move(atts)}
                });
            }
            j["student_profile"] = {{"grades", std::move(grades)}};
        }

        if (!snap.currentSchedule.empty()) {
            json cs = json::array();
            for (const auto& cell : snap.currentSchedule) {
                cs.push_back({
                    {"day_index_0_mon", cell.day_index_0_mon},
                    {"time_range", cell.time_range},
                    {"start_minute", cell.start_minute},
                    {"end_minute", cell.end_minute},
                    {"course_code", cell.course_code},
                    {"course_name", cell.course_name},
                    {"group", cell.group},
                    {"room", cell.room},
                    {"raw_text", cell.raw_text}
                });
            }
            j["current_schedule"] = std::move(cs);
        }

        if (!snap.foldersByPath.empty()) {
            json fbp = json::object();
            for (const auto& [path, fwr] : snap.foldersByPath) {
                json fls = json::array();
                for (const auto& f : fwr.files) {
                    fls.push_back({
                        {"object_id", f.object_id},
                        {"name", f.name},
                        {"title", f.title},
                        {"folder_path", f.folder_path},
                        {"content_size", f.content_size},
                        {"status", static_cast<int>(f.status)},
                        {"logical_rel_path", f.logical_rel_path},
                        {"blob_rel_path", f.blob_rel_path},
                        {"plagiarism_percentage", f.plagiarism_percentage},
                        {"plagiarism_status", f.plagiarism_status},
                        {"plagiarism_doc_uid", f.plagiarism_doc_uid}
                    });
                }
                fbp[path] = {{"files", std::move(fls)}};
            }
            j["folders_by_path"] = std::move(fbp);
        }

        if (snap.plagiarismTree) {
            json docs = json::array();
            for (const auto& d : snap.plagiarismTree->documents) {
                docs.push_back({
                    {"doc_uid", d.doc_uid},
                    {"file_name", d.file_name},
                    {"status_code", d.status_code},
                    {"status_text", d.status_text},
                    {"similarity_pct", d.similarity_pct}
                });
            }
            j["plagiarism_tree"] = {
                {"my_files_name", snap.plagiarismTree->my_files_name},
                {"my_files_folder_id", snap.plagiarismTree->my_files_folder_id},
                {"documents", std::move(docs)}
            };
        }

        if (!snap.graduateAttributes.empty()) {
            json ga = json::array();
            for (const auto& g : snap.graduateAttributes) {
                ga.push_back({
                    {"attribute_name", g.attribute_name},
                    {"description", g.description},
                    {"courses", g.courses},
                    {"evaluated_pct", g.evaluated_pct},
                    {"initial_pct", g.initial_pct},
                    {"medium_pct", g.medium_pct},
                    {"advanced_pct", g.advanced_pct},
                    {"total_pct", g.total_pct}
                });
            }
            j["graduate_attributes"] = std::move(ga);
        }

        if (!snap.pendingGraduationCourses.empty()) {
            json pg = json::array();
            for (const auto& c : snap.pendingGraduationCourses) {
                pg.push_back({{"id", c.id}, {"code", c.code}, {"name", c.name}, {"credits", c.credits}});
            }
            j["pending_graduation_courses"] = std::move(pg);
        }

        if (snap.inclusionCampaign) {
            j["inclusion_campaign"] = {
                {"id", snap.inclusionCampaign->id},
                {"name", snap.inclusionCampaign->name},
                {"period", snap.inclusionCampaign->period},
                {"start_date", snap.inclusionCampaign->start_date},
                {"end_date", snap.inclusionCampaign->end_date},
                {"student_entity", snap.inclusionCampaign->student_entity},
                {"active", snap.inclusionCampaign->active}
            };
        }

        if (!snap.inclusionApplications.empty()) {
            json apps = json::array();
            for (const auto& a : snap.inclusionApplications) {
                apps.push_back({
                    {"id", a.id},
                    {"course_code", a.course_code},
                    {"course_name", a.course_name},
                    {"group", a.group},
                    {"status", a.status},
                    {"comment", a.comment},
                    {"updated_at", a.updated_at}
                });
            }
            j["inclusion_applications"] = std::move(apps);
        }

        if (snap.scholarshipInfo) {
            j["scholarship_info"] = {
                {"campaign_id", snap.scholarshipInfo->campaign_id},
                {"status", snap.scholarshipInfo->status},
                {"scholarship_type", snap.scholarshipInfo->scholarship_type},
                {"detail", snap.scholarshipInfo->detail}
            };
        }

        if (snap.studentGeneralInfo) {
            json rns = json::array();
            for (const auto& r : snap.studentGeneralInfo->rn_courses) {
                rns.push_back({{"course_code", r.course_code}, {"rn", r.rn}, {"course_name", r.course_name}});
            }
            j["student_general_info"] = {
                {"career", snap.studentGeneralInfo->career},
                {"weighted_average", snap.studentGeneralInfo->weighted_average},
                {"outstanding_courses", snap.studentGeneralInfo->outstanding_courses},
                {"available_registration_credits", snap.studentGeneralInfo->available_registration_credits},
                {"rn_courses", std::move(rns)}
            };
        }

        if (!snap.actualPeriodCourses.empty()) {
            json apc = json::array();
            for (const auto& c : snap.actualPeriodCourses) {
                apc.push_back({
                    {"id", c.id},
                    {"code", c.code},
                    {"materia", c.materia},
                    {"credits", c.credits},
                    {"professors", c.professors},
                    {"schedule", c.schedule}
                });
            }
            j["actual_period_courses"] = std::move(apc);
        }

        if (!snap.pendingPerformanceEvals.empty()) {
            json pe = json::array();
            for (const auto& e : snap.pendingPerformanceEvals) {
                pe.push_back({
                    {"evaluation_id", e.evaluation_id},
                    {"course_name", e.course_name},
                    {"teacher_name", e.teacher_name},
                    {"form_name", e.form_name},
                    {"status", e.status},
                    {"deadline", e.deadline},
                    {"url", e.url}
                });
            }
            j["pending_performance_evals"] = std::move(pe);
        }

        if (!snap.customTabResourcesByCourse.empty()) {
            json ctr = json::object();
            for (const auto& [path, links] : snap.customTabResourcesByCourse) {
                json arr = json::array();
                for (const auto& l : links) {
                    arr.push_back({{"label", l.label}, {"url", l.url}, {"category", l.category}});
                }
                ctr[path] = std::move(arr);
            }
            j["custom_tab_resources"] = std::move(ctr);
        }

        fs::path target = fs::path(Utf8ToWide(snapshotCachePathUtf8_));
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream ofs(target, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (outErr) *outErr = "No se pudo escribir la caché académica local.";
            return false;
        }
        const std::string serialized = j.dump(2);
        ofs.write(serialized.data(), static_cast<std::streamsize>(serialized.size()));
        return ofs.good();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool TecAcademicStore::LoadSnapshotFromDisk(std::string* outErr) {
    fs::path target = fs::path(Utf8ToWide(snapshotCachePathUtf8_));
    std::error_code ec;
    if (!fs::exists(target, ec)) return false;
    try {
        std::ifstream ifs(target, std::ios::binary);
        if (!ifs.is_open()) return false;
        std::ostringstream oss;
        oss << ifs.rdbuf();
        auto j = json::parse(oss.str());
        if (!j.is_object()) return false;

        TecAcademicSnapshot loaded;
        if (j.contains("synced_at_utc") && j["synced_at_utc"].is_string()) {
            loaded.syncedAtUtc = j["synced_at_utc"].get<std::string>();
        }
        if (j.contains("inclusion_service_status") && j["inclusion_service_status"].is_string()) {
            loaded.inclusionServiceStatus = j["inclusion_service_status"].get<std::string>();
        }
        if (j.contains("communities") && j["communities"].is_array()) {
            for (const auto& c : j["communities"]) {
                td::CommunityInfo ci;
                ci.community_id = c.value("community_id", "");
                ci.slug = c.value("slug", "");
                ci.name = c.value("name", "");
                ci.type = c.value("type", "");
                ci.department = c.value("department", "");
                ci.period = c.value("period", "");
                ci.url = c.value("url", "");
                ci.root_folder_id = c.value("root_folder_id", "");
                loaded.communities.push_back(std::move(ci));
            }
        }
        if (j.contains("evaluations") && j["evaluations"].is_object()) {
            for (auto it = j["evaluations"].begin(); it != j["evaluations"].end(); ++it) {
                td::DetailedEvaluationsReport rep;
                rep.community_slug = it.value().value("community_slug", "");
                rep.unrounded_total = it.value().value("unrounded_total", 0.0);
                rep.final_grade = it.value().value("final_grade", 0.0);
                rep.has_final_grade = it.value().value("has_final_grade", false);
                if (it.value().contains("categories") && it.value()["categories"].is_array()) {
                    for (const auto& cj : it.value()["categories"]) {
                        td::DetailedEvaluationCategory cat;
                        cat.name = cj.value("name", "");
                        cat.weight_pct = cj.value("weight_pct", 0.0);
                        cat.earned_pct = cj.value("earned_pct", 0.0);
                        if (cj.contains("items") && cj["items"].is_array()) {
                            for (const auto& ij : cj["items"]) {
                                td::DetailedEvaluationItem item;
                                item.assign_id = ij.value("assign_id", "");
                                item.category_name = cat.name;
                                item.name = ij.value("name", "");
                                item.weight_pct = ij.value("weight_pct", 0.0);
                                item.earned_pct = ij.value("earned_pct", 0.0);
                                item.raw_grade_100 = ij.value("raw_grade_100", -1.0);
                                item.is_graded = ij.value("is_graded", false);
                                item.due_date_iso = ij.value("due_date_iso", "");
                                item.due_date_text = ij.value("due_date_text", "");
                                item.has_rubric = ij.value("has_rubric", false);
                                item.rubric_id = ij.value("rubric_id", "");
                                item.submission_status = ij.value("submission_status", "");
                                cat.items.push_back(std::move(item));
                            }
                        }
                        rep.categories.push_back(std::move(cat));
                    }
                }
                loaded.evaluationsByPath[it.key()] = std::move(rep);
            }
        }
        if (j.contains("unified_calendar") && j["unified_calendar"].is_array()) {
            for (const auto& ej : j["unified_calendar"]) {
                td::CalendarEventEntry ev;
                ev.uid = ej.value("uid", "");
                ev.date_iso = ej.value("date_iso", "");
                ev.start_time = ej.value("start_time", "");
                ev.end_time = ej.value("end_time", "");
                ev.title = ej.value("title", "");
                ev.calendar_name = ej.value("calendar_name", "");
                ev.description = ej.value("description", "");
                ev.source_kind = ej.value("source_kind", "calendar");
                loaded.unifiedCalendarEvents.push_back(std::move(ev));
            }
        }
        if (j.contains("expediente_stats") && j["expediente_stats"].is_object()) {
            td::ExpedienteSummary ex;
            const auto& sj = j["expediente_stats"];
            auto& st = ex.structured.stats;
            st.weighted_avg_general = sj.value("weighted_avg_general", 0.0);
            st.weighted_avg_passed = sj.value("weighted_avg_passed", 0.0);
            st.weighted_avg_failed = sj.value("weighted_avg_failed", 0.0);
            st.passed_pct = sj.value("passed_pct", 0.0);
            st.failed_pct = sj.value("failed_pct", 0.0);
            st.hours_taken = sj.value("hours_taken", 0);
            st.credits_passed = sj.value("credits_passed", 0);
            st.credits_failed = sj.value("credits_failed", 0);
            st.credits_total = sj.value("credits_total", 0);
            st.count_aprobadas = sj.value("aprobadas", 0);
            st.count_en_curso = sj.value("en_curso", 0);
            st.count_reprobadas = sj.value("reprobadas", 0);
            st.count_retiradas = sj.value("retiradas", 0);
            st.count_reconocidas = sj.value("reconocidas", 0);
            st.count_suficiencia = sj.value("suficiencia", 0);
            st.count_pendientes = sj.value("pendientes", 0);
            st.count_congeladas = sj.value("congeladas", 0);
            loaded.expediente = std::move(ex);
        }
        if (j.contains("curriculum_graph") && j["curriculum_graph"].is_object()) {
            const auto& cg = j["curriculum_graph"];
            td::CurriculumGraph g;
            g.dsc_curriculum = cg.value("dsc_curriculum", "");
            g.id_curriculum = cg.value("id_curriculum", int64_t{0});
            g.modality = cg.value("modality", "");
            g.academic_degree = cg.value("academic_degree", "");
            g.first_level = cg.value("first_level", 0);
            if (cg.contains("levels") && cg["levels"].is_array()) {
                for (const auto& lvj : cg["levels"]) {
                    td::CurriculumLevel lv;
                    lv.id = lvj.value("id", "");
                    if (lvj.contains("courses") && lvj["courses"].is_array()) {
                        for (const auto& cj : lvj["courses"]) {
                            td::CurriculumCourseNode cn;
                            cn.id_course = cj.value("id_course", "");
                            cn.name = cj.value("name", "");
                            cn.truncated_name = cj.value("truncated_name", "");
                            cn.credits = cj.value("credits", 0);
                            cn.hours = cj.value("hours", 0);
                            if (cj.contains("requirements") && cj["requirements"].is_array()) {
                                cn.requirements = cj["requirements"].get<std::vector<std::string>>();
                            }
                            if (cj.contains("co_requirements") && cj["co_requirements"].is_array()) {
                                cn.co_requirements = cj["co_requirements"].get<std::vector<std::string>>();
                            }
                            if (cj.contains("required_by") && cj["required_by"].is_array()) {
                                cn.required_by = cj["required_by"].get<std::vector<std::string>>();
                            }
                            lv.courses.push_back(std::move(cn));
                        }
                    }
                    g.levels.push_back(std::move(lv));
                }
            }
            loaded.curriculumGraph = std::move(g);
        }
        if (j.contains("student_profile") && j["student_profile"].is_object()) {
            const auto& sp = j["student_profile"];
            td::StudentCurriculumProfile prof;
            if (sp.contains("grades") && sp["grades"].is_array()) {
                for (const auto& gj : sp["grades"]) {
                    td::StudentCourseRecord rec;
                    rec.id_course = gj.value("id_course", "");
                    rec.best_status = gj.value("best_status", "");
                    if (gj.contains("grades_record") && gj["grades_record"].is_array()) {
                        for (const auto& aj : gj["grades_record"]) {
                            td::CourseGradeAttempt att;
                            att.year = aj.value("year", 0);
                            att.period = aj.value("period", 0);
                            att.modality = aj.value("modality", "");
                            att.state = aj.value("state", 0);
                            att.grade = aj.value("grade", 0.0);
                            att.group = aj.value("group", 0);
                            rec.grades_record.push_back(std::move(att));
                        }
                    }
                    prof.grades.push_back(std::move(rec));
                }
            }
            loaded.studentProfile = std::move(prof);
        }
        if (j.contains("current_schedule") && j["current_schedule"].is_array()) {
            for (const auto& cj : j["current_schedule"]) {
                td::WeeklyScheduleCell cell;
                cell.day_index_0_mon = cj.value("day_index_0_mon", 0);
                cell.time_range = cj.value("time_range", "");
                cell.start_minute = cj.value("start_minute", 0);
                cell.end_minute = cj.value("end_minute", 0);
                cell.course_code = cj.value("course_code", "");
                cell.course_name = cj.value("course_name", "");
                cell.group = cj.value("group", "");
                cell.room = cj.value("room", "");
                cell.raw_text = cj.value("raw_text", "");
                loaded.currentSchedule.push_back(std::move(cell));
            }
        }
        if (j.contains("folders_by_path") && j["folders_by_path"].is_object()) {
            for (auto it = j["folders_by_path"].begin(); it != j["folders_by_path"].end(); ++it) {
                td::FolderWalkResult fwr;
                if (it.value().contains("files") && it.value()["files"].is_array()) {
                    for (const auto& fj : it.value()["files"]) {
                        td::FileRecord f;
                        f.object_id = fj.value("object_id", "");
                        f.name = fj.value("name", "");
                        f.title = fj.value("title", "");
                        f.folder_path = fj.value("folder_path", "");
                        f.content_size = fj.value("content_size", uint64_t{0});
                        f.status = static_cast<td::FileStatus>(fj.value("status", 0));
                        f.logical_rel_path = fj.value("logical_rel_path", "");
                        f.blob_rel_path = fj.value("blob_rel_path", "");
                        f.plagiarism_percentage = fj.value("plagiarism_percentage", -1.0);
                        f.plagiarism_status = fj.value("plagiarism_status", "");
                        f.plagiarism_doc_uid = fj.value("plagiarism_doc_uid", "");
                        fwr.files.push_back(std::move(f));
                    }
                }
                loaded.foldersByPath[it.key()] = std::move(fwr);
            }
        }
        if (j.contains("plagiarism_tree") && j["plagiarism_tree"].is_object()) {
            const auto& ptj = j["plagiarism_tree"];
            td::PlagiarismTreeSnapshot pt;
            pt.my_files_name = ptj.value("my_files_name", "");
            pt.my_files_folder_id = ptj.value("my_files_folder_id", "");
            if (ptj.contains("documents") && ptj["documents"].is_array()) {
                for (const auto& dj : ptj["documents"]) {
                    td::PlagiarismDocRecord dr;
                    dr.doc_uid = dj.value("doc_uid", "");
                    dr.file_name = dj.value("file_name", "");
                    dr.status_code = dj.value("status_code", "");
                    dr.status_text = dj.value("status_text", "");
                    dr.similarity_pct = dj.value("similarity_pct", -1.0);
                    pt.documents.push_back(std::move(dr));
                }
            }
            loaded.plagiarismTree = std::move(pt);
        }
        if (j.contains("graduate_attributes") && j["graduate_attributes"].is_array()) {
            for (const auto& gj : j["graduate_attributes"]) {
                td::GraduateAttributeRow g;
                g.attribute_name = gj.value("attribute_name", "");
                g.description = gj.value("description", "");
                g.courses = gj.value("courses", "");
                g.evaluated_pct = gj.value("evaluated_pct", 0.0);
                g.initial_pct = gj.value("initial_pct", 0.0);
                g.medium_pct = gj.value("medium_pct", 0.0);
                g.advanced_pct = gj.value("advanced_pct", 0.0);
                g.total_pct = gj.value("total_pct", 0.0);
                loaded.graduateAttributes.push_back(std::move(g));
            }
        }
        if (j.contains("pending_graduation_courses") && j["pending_graduation_courses"].is_array()) {
            for (const auto& cj : j["pending_graduation_courses"]) {
                td::PendingGradCourse pg;
                pg.id = cj.value("id", "");
                pg.code = cj.value("code", "");
                pg.name = cj.value("name", "");
                pg.credits = cj.value("credits", 0);
                loaded.pendingGraduationCourses.push_back(std::move(pg));
            }
        }
        if (j.contains("inclusion_campaign") && j["inclusion_campaign"].is_object()) {
            const auto& icj = j["inclusion_campaign"];
            td::InclusionCampaignInfo ic;
            ic.id = icj.value("id", "");
            ic.name = icj.value("name", "");
            ic.period = icj.value("period", "");
            ic.start_date = icj.value("start_date", "");
            ic.end_date = icj.value("end_date", "");
            ic.student_entity = icj.value("student_entity", "");
            ic.active = icj.value("active", false);
            loaded.inclusionCampaign = std::move(ic);
        }
        if (j.contains("inclusion_applications") && j["inclusion_applications"].is_array()) {
            for (const auto& aj : j["inclusion_applications"]) {
                td::InclusionApplicationRecord rec;
                rec.id = aj.value("id", "");
                rec.course_code = aj.value("course_code", "");
                rec.course_name = aj.value("course_name", "");
                rec.group = aj.value("group", "");
                rec.status = aj.value("status", "");
                rec.comment = aj.value("comment", "");
                rec.updated_at = aj.value("updated_at", "");
                loaded.inclusionApplications.push_back(std::move(rec));
            }
        }
        if (j.contains("scholarship_info") && j["scholarship_info"].is_object()) {
            const auto& sj = j["scholarship_info"];
            td::ScholarshipStatusInfo sc;
            sc.campaign_id = sj.value("campaign_id", "");
            sc.status = sj.value("status", "");
            sc.scholarship_type = sj.value("scholarship_type", "");
            sc.detail = sj.value("detail", "");
            loaded.scholarshipInfo = std::move(sc);
        }
        if (j.contains("student_general_info") && j["student_general_info"].is_object()) {
            const auto& gj = j["student_general_info"];
            td::StudentGeneralInfo gi;
            gi.career = gj.value("career", "");
            gi.weighted_average = gj.value("weighted_average", 0.0);
            gi.outstanding_courses = gj.value("outstanding_courses", 0);
            gi.available_registration_credits = gj.value("available_registration_credits", 0);
            if (gj.contains("rn_courses") && gj["rn_courses"].is_array()) {
                for (const auto& rj : gj["rn_courses"]) {
                    gi.rn_courses.push_back({rj.value("course_code", ""), rj.value("rn", 0), rj.value("course_name", "")});
                }
            }
            loaded.studentGeneralInfo = std::move(gi);
        }
        if (j.contains("actual_period_courses") && j["actual_period_courses"].is_array()) {
            for (const auto& cj : j["actual_period_courses"]) {
                td::GraphQlActualCourse c;
                c.id = cj.value("id", "");
                c.code = cj.value("code", "");
                c.materia = cj.value("materia", "");
                c.credits = cj.value("credits", 0);
                if (cj.contains("professors") && cj["professors"].is_array()) {
                    for (const auto& p : cj["professors"]) if (p.is_string()) c.professors.push_back(p.get<std::string>());
                }
                if (cj.contains("schedule") && cj["schedule"].is_array()) {
                    for (const auto& sc : cj["schedule"]) if (sc.is_string()) c.schedule.push_back(sc.get<std::string>());
                }
                loaded.actualPeriodCourses.push_back(std::move(c));
            }
        }
        if (j.contains("pending_performance_evals") && j["pending_performance_evals"].is_array()) {
            for (const auto& ej : j["pending_performance_evals"]) {
                td::PerformanceEvalPending ev;
                ev.evaluation_id = ej.value("evaluation_id", "");
                ev.course_name = ej.value("course_name", "");
                ev.teacher_name = ej.value("teacher_name", "");
                ev.form_name = ej.value("form_name", "");
                ev.status = ej.value("status", "");
                ev.deadline = ej.value("deadline", "");
                ev.url = ej.value("url", "");
                loaded.pendingPerformanceEvals.push_back(std::move(ev));
            }
        }
        if (j.contains("custom_tab_resources") && j["custom_tab_resources"].is_object()) {
            for (auto it = j["custom_tab_resources"].begin(); it != j["custom_tab_resources"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::CourseExternalLink> links;
                for (const auto& lj : it.value()) {
                    links.push_back({lj.value("label", ""), lj.value("url", ""), lj.value("category", "")});
                }
                loaded.customTabResourcesByCourse[it.key()] = std::move(links);
            }
        }

        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_ = std::move(loaded);
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

std::uint64_t TecAcademicStore::ComputeOfflineCacheBytes() const {
    std::uint64_t total = 0;
    std::error_code ec;
    fs::path root = fs::path(Utf8ToWide(offlineFilesRootUtf8_));
    if (!fs::exists(root, ec)) return 0;
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (entry.is_regular_file(ec)) {
            total += entry.file_size(ec);
        }
    }
    return total;
}

std::size_t TecAcademicStore::PruneOfflineCacheOlderThanBytes(std::uint64_t targetMaxBytes) {
    struct EntryMeta {
        fs::path path;
        fs::file_time_type mtime;
        std::uint64_t size = 0;
    };
    std::vector<EntryMeta> files;
    std::uint64_t total = 0;
    std::error_code ec;
    fs::path root = fs::path(Utf8ToWide(offlineFilesRootUtf8_));
    if (!fs::exists(root, ec)) return 0;
    for (const auto& entry : fs::recursive_directory_iterator(root, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string name = entry.path().filename().string();
        if (name == "manifest.json" || name == "files.json" || name == "material_index.json") continue;
        std::uint64_t sz = entry.file_size(ec);
        total += sz;
        files.push_back({entry.path(), entry.last_write_time(ec), sz});
    }
    if (total <= targetMaxBytes) return 0;
    std::sort(files.begin(), files.end(),
              [](const EntryMeta& a, const EntryMeta& b) { return a.mtime < b.mtime; });
    std::size_t removed = 0;
    for (const auto& f : files) {
        if (total <= targetMaxBytes) break;
        if (fs::remove(f.path, ec)) {
            total = (total >= f.size) ? (total - f.size) : 0;
            ++removed;
        }
    }
    return removed;
}

std::vector<td::MaterialSearchHit> TecAcademicStore::SearchOfflineMaterials(
    const std::string& queryUtf8,
    const std::string& courseFilter,
    std::size_t maxHits) {
    td::MaterialSearchFilters filters;
    filters.community = courseFilter;
    filters.max_results = maxHits;
    auto r = service_->SearchOfflineMaterials(queryUtf8, filters);
    if (!r.ok) return {};
    return r.value;
}

TecAcademicSnapshot TecAcademicStore::Snapshot() const {
    std::lock_guard<std::mutex> lk(snapMutex_);
    return snapshot_;
}

void TecAcademicStore::SetSnapshotForTesting(TecAcademicSnapshot snap) {
    std::lock_guard<std::mutex> lk(snapMutex_);
    snapshot_ = std::move(snap);
}

// ============================================================================
// 1-Click CuadernU Canvas & SM-2 Builders
// ============================================================================

std::size_t TecAcademicStore::InsertCourseOverviewToCanvas(
    DocStudio::DocumentModel& doc,
    const td::CommunityInfo& course,
    const td::CourseOverviewSnapshot* overview,
    const td::DetailedEvaluationsReport* evals) {
    std::size_t inserted = 0;
    double y = 44.0;
    const double x = 44.0;
    const double w = 510.0;

    std::string header = course.name;
    if (!course.period.empty()) header += "  (" + course.period + ")";
    PlaceObjectInActiveDoc(doc, MakeTextBoxObject(
        doc, x, y, w, 46.0, header, 18.0, true,
        DocStudio::DocColor::RGBA(28, 36, 52, 255),
        DocStudio::DocColor::RGBA(244, 246, 250, 255)));
    y += 58.0;
    ++inserted;

    if (overview) {
        std::ostringstream oss;
        if (!overview->teacher.name.empty() || !overview->teacher.office.empty() ||
            !overview->teacher.consultation_hours.empty()) {
            oss << "Docente y Consulta\n";
            if (!overview->teacher.name.empty()) oss << "Profesor: " << overview->teacher.name << "\n";
            if (!overview->teacher.office.empty()) oss << "Oficina: " << overview->teacher.office << "\n";
            if (!overview->teacher.teams_handle.empty()) oss << "Teams: " << overview->teacher.teams_handle << "\n";
            for (const auto& h : overview->teacher.consultation_hours) {
                oss << "Consulta: " << h << "\n";
            }
            oss << "\n";
        }
        if (!overview->official_docs.empty()) {
            oss << "Documentos Oficiales\n";
            for (const auto& d : overview->official_docs) {
                oss << "• " << d.label << " (" << d.category << "): " << d.url << "\n";
            }
        }
        if (!oss.str().empty()) {
            PlaceObjectInActiveDoc(doc, MakeTextBoxObject(
                doc, x, y, w, 130.0, oss.str(), 11.5, false,
                DocStudio::DocColor::RGBA(45, 55, 72, 255),
                DocStudio::DocColor::RGBA(250, 251, 253, 255)));
            y += 142.0;
            ++inserted;
        }
    }

    if (evals && !evals->categories.empty()) {
        const auto sc = td::ComputeGradeWhatIf(*evals, 70.0);
        std::ostringstream oss;
        oss << "Evaluación del Curso — Acumulado: " << Round2(sc.current_earned_pct)
            << " / " << Round1(sc.total_weight_pct)
            << "  |  Para aprobar (70): "
            << (sc.already_reached ? "Aprobado" : (sc.mathematically_possible ? (Round1(sc.required_avg_on_remaining_100) + "/100 en restantes") : "Meta excede 100"))
            << "\n";
        for (const auto& cat : evals->categories) {
            oss << "• " << cat.name << ": " << Round2(cat.earned_pct) << " / " << Round1(cat.weight_pct) << "%\n";
        }
        PlaceObjectInActiveDoc(doc, MakeTextBoxObject(
            doc, x, y, w, 140.0, oss.str(), 11.0, false,
            DocStudio::DocColor::RGBA(32, 42, 58, 255),
            DocStudio::DocColor::RGBA(246, 250, 247, 255)));
        ++inserted;
    }

    doc.AddBookmark(course.name, 0, 44.0, 0);
    doc.SetDirty(true);
    return inserted;
}

std::size_t TecAcademicStore::InsertScheduleToCanvas(
    DocStudio::DocumentModel& doc,
    const ht::RankedSchedule& schedule,
    const std::string& titleUtf8) {
    std::ostringstream oss;
    oss << titleUtf8 << "  (" << schedule.metrics.totalCredits << " créditos · "
        << schedule.metrics.activeDaysCount << " días activos · "
        << schedule.metrics.totalGapMinutes << " min de huecos)\n\n";
    for (const auto& sel : schedule.selections) {
        oss << "• " << sel.courseCodeUtf8 << " — " << sel.courseNameUtf8
            << " [Gr " << sel.group.idUtf8 << ", " << sel.credits << " cr]";
        if (!sel.group.professorUtf8.empty()) oss << " · " << sel.group.professorUtf8;
        oss << "\n   ";
        for (size_t i = 0; i < sel.group.sessions.size(); ++i) {
            const auto& s = sel.group.sessions[i];
            if (i > 0) oss << " | ";
            oss << ht::DayToShortEs(s.day) << " " << ht::FormatTimeHHMM(s.startMinute)
                << "-" << ht::FormatTimeHHMM(s.endMinute);
            if (!s.roomUtf8.empty()) oss << " (" << s.roomUtf8 << ")";
        }
        oss << "\n";
    }
    PlaceObjectInActiveDoc(doc, MakeTextBoxObject(
        doc, 44.0, 44.0, 520.0, 260.0, oss.str(), 11.5, false,
        DocStudio::DocColor::RGBA(28, 36, 52, 255),
        DocStudio::DocColor::RGBA(248, 250, 252, 255)));
    doc.AddBookmark(titleUtf8, 0, 44.0, 0);
    doc.SetDirty(true);
    return 1;
}

std::string TecAcademicStore::ExportRankedScheduleToIcs(
    const ht::RankedSchedule& schedule,
    const std::string& titleUtf8,
    const std::string& mondayAnchorIso) {
    int y = 2026, m = 7, d = 20;
    if (mondayAnchorIso.size() >= 10) {
        sscanf_s(mondayAnchorIso.c_str(), "%d-%d-%d", &y, &m, &d);
    }
    auto fmtDateOffset = [&](int offsetDays) -> std::string {
        std::tm tmVal{};
        tmVal.tm_year = y - 1900;
        tmVal.tm_mon = m - 1;
        tmVal.tm_mday = d + offsetDays;
        tmVal.tm_hour = 12;
        std::mktime(&tmVal);
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%04d%02d%02d", tmVal.tm_year + 1900, tmVal.tm_mon + 1, tmVal.tm_mday);
        return buf;
    };
    auto fmtMin = [](int mins) -> std::string {
        char buf[16]{};
        std::snprintf(buf, sizeof(buf), "%02d%02d00", (mins / 60) % 24, mins % 60);
        return buf;
    };

    std::ostringstream oss;
    oss << "BEGIN:VCALENDAR\r\n"
        << "VERSION:2.0\r\n"
        << "PRODID:-//CuadernU//HorariosTEC//ES\r\n"
        << "X-WR-CALNAME:" << titleUtf8 << "\r\n"
        << "CALSCALE:GREGORIAN\r\n"
        << "METHOD:PUBLISH\r\n";
    int uidSeq = 0;
    for (const auto& sel : schedule.selections) {
        for (const auto& s : sel.group.sessions) {
            const int dayOff = std::clamp(static_cast<int>(s.day), 0, 6);
            const std::string d8 = fmtDateOffset(dayOff);
            oss << "BEGIN:VEVENT\r\n"
                << "UID:cuadernu-hor-" << (++uidSeq) << "-" << sel.courseCodeUtf8 << "@cuadernu.local\r\n"
                << "DTSTART:" << d8 << "T" << fmtMin(s.startMinute) << "\r\n"
                << "DTEND:" << d8 << "T" << fmtMin(s.endMinute) << "\r\n"
                << "RRULE:FREQ=WEEKLY;COUNT=16\r\n"
                << "SUMMARY:" << sel.courseCodeUtf8 << " " << sel.courseNameUtf8 << " (Gr " << sel.group.idUtf8 << ")\r\n";
            if (!s.roomUtf8.empty()) oss << "LOCATION:" << s.roomUtf8 << "\r\n";
            if (!sel.group.professorUtf8.empty()) oss << "DESCRIPTION:Docente: " << sel.group.professorUtf8 << "\r\n";
            oss << "END:VEVENT\r\n";
        }
    }
    oss << "END:VCALENDAR\r\n";
    return oss.str();
}

bool TecAcademicStore::ExportRankedScheduleToPdfFile(
    const ht::RankedSchedule& schedule,
    const std::wstring& outPdfPathW,
    const std::string& titleUtf8,
    std::string* outErr) {
    DocStudio::DocumentModel tmpDoc;
    tmpDoc.ResetToBlank(DocStudio::DocumentMode::PaginatedPdf);
    tmpDoc.SetTitle(titleUtf8);
    InsertScheduleToCanvas(tmpDoc, schedule, titleUtf8);
    return DocStudio::PdfWriter::ExportDocumentToFile(tmpDoc, outPdfPathW, outErr);
}

bool TecAcademicStore::SaveCourseNotebookCnu(
    const td::CommunityInfo& course,
    const td::CourseOverviewSnapshot* overview,
    const td::DetailedEvaluationsReport* evals,
    const std::wstring& outCnuPathW,
    std::string* outErr) {
    DocStudio::DocumentModel nbDoc;
    nbDoc.ResetToBlank(DocStudio::DocumentMode::PaginatedPdf);
    std::string title = course.name;
    if (!course.period.empty()) title = course.period + " — " + title;
    nbDoc.SetTitle(title);
    InsertCourseOverviewToCanvas(nbDoc, course, overview, evals);
    return DocStudio::CudocStorage::SaveToFile(outCnuPathW, nbDoc, outErr);
}

std::size_t TecAcademicStore::InsertCurriculumSummaryToCanvas(
    DocStudio::DocumentModel& doc,
    const td::CurriculumGraph& graph,
    const td::StudentCurriculumProfile* profile) {
    int totalCourses = 0;
    int passedCourses = 0;
    int totalCredits = 0;
    int passedCredits = 0;

    std::ostringstream oss;
    oss << "Plan de Estudios: " << graph.dsc_curriculum << " (Plan " << graph.id_curriculum << ")\n";
    for (const auto& lv : graph.levels) {
        oss << "\nNivel " << lv.id << ":\n";
        for (const auto& c : lv.courses) {
            ++totalCourses;
            totalCredits += c.credits;
            bool passed = profile && IsCoursePassedInProfile(*profile, c.id_course);
            if (passed) {
                ++passedCourses;
                passedCredits += c.credits;
            }
            oss << (passed ? "  [Aprobado] " : "  [Pendiente] ")
                << c.id_course << " — " << c.name << " (" << c.credits << " cr)\n";
        }
    }
    std::string summaryHeader = "Avance Curricular: " + std::to_string(passedCourses) + "/" +
                                std::to_string(totalCourses) + " cursos · " +
                                std::to_string(passedCredits) + "/" + std::to_string(totalCredits) + " créditos";
    PlaceObjectInActiveDoc(doc, MakeTextBoxObject(
        doc, 44.0, 44.0, 520.0, 44.0, summaryHeader, 14.0, true,
        DocStudio::DocColor::RGBA(24, 34, 48, 255),
        DocStudio::DocColor::RGBA(240, 245, 252, 255)));
    PlaceObjectInActiveDoc(doc, MakeTextBoxObject(
        doc, 44.0, 98.0, 520.0, 420.0, oss.str(), 10.5, false,
        DocStudio::DocColor::RGBA(36, 44, 58, 255),
        DocStudio::DocColor::RGBA(252, 253, 254, 255)));
    doc.AddBookmark("Plan de Estudios: " + graph.dsc_curriculum, 0, 44.0, 0);
    doc.SetDirty(true);
    return 2;
}

bool TecAcademicStore::ImportDownloadedMaterialToCanvas(
    DocStudio::DocumentModel& doc,
    const std::string& localFilePathUtf8,
    const std::string& displayNameUtf8,
    const std::string& courseLabelUtf8,
    const std::string& sourceUrlUtf8) {
    DocStudio::AnnotationObject card{};
    card.id = doc.AllocateObjectId();
    card.zOrder = doc.AllocateZOrder();
    card.layerId = doc.ActiveLayerId();
    card.bounds = { 48.0, 48.0, 460.0, 74.0 };
    if (!sourceUrlUtf8.empty() && sourceUrlUtf8.rfind("http", 0) == 0) {
        card.kind = DocStudio::ObjectKind::LinkCard;
        card.linkUrlUtf8 = sourceUrlUtf8;
        card.linkTitleUtf8 = displayNameUtf8.empty() ? localFilePathUtf8 : displayNameUtf8;
        card.linkDescriptionUtf8 = "Material de " + courseLabelUtf8 +
                                   (localFilePathUtf8.empty() ? "" : (" · Archivo local: " + localFilePathUtf8));
    } else {
        card.kind = DocStudio::ObjectKind::TextBox;
        card.textUtf8 = "Material de " + courseLabelUtf8 + "\n" +
                        (displayNameUtf8.empty() ? localFilePathUtf8 : displayNameUtf8) + "\n" +
                        localFilePathUtf8;
        card.fontFamilyUtf8 = "Segoe UI";
        card.fontSizePt = 11.5;
    }
    PlaceObjectInActiveDoc(doc, std::move(card));
    doc.SetDirty(true);
    return true;
}

std::vector<Sm2FlashcardDraft> TecAcademicStore::GenerateSm2FlashcardsFromCourse(
    const td::CommunityInfo& course,
    const td::CourseOverviewSnapshot* overview,
    const td::DetailedEvaluationsReport* evals) {
    std::vector<Sm2FlashcardDraft> out;
    const std::string tag = "#" + (course.slug.empty() ? course.department : course.slug);
    if (overview) {
        for (const auto& wk : overview->weekly_sections) {
            out.push_back({
                "¿Qué tema central corresponde a " + wk + " en " + course.name + "?",
                "Revisar material y ejercicios asignados de " + wk + ".",
                tag
            });
        }
        for (const auto& docLink : overview->official_docs) {
            out.push_back({
                "Documento oficial clave de " + course.name + ": " + docLink.label,
                "Categoría: " + docLink.category + " · Referencia en TEC Digital.",
                tag
            });
        }
    }
    if (evals) {
        for (const auto& cat : evals->categories) {
            for (const auto& it : cat.items) {
                if (!it.is_graded && !it.due_date_text.empty() &&
                    it.due_date_text.find("no definida") == std::string::npos) {
                    out.push_back({
                        "¿Cuál es la fecha límite y el peso de " + it.name + " (" + course.name + ")?",
                        "Fecha: " + it.due_date_text + " · Rubro: " + cat.name +
                        " (" + Round1(it.weight_pct) + "%).",
                        tag
                    });
                }
            }
        }
    }
    return out;
}

#if defined(DOC_STUDIO_HAS_GEMINI_NOTEBOOK)
std::vector<DocStudio::GeminiNotebook::AcademicMaterialCandidate>
TecAcademicStore::BuildGeminiNotebookCandidatesForCourse(
    const td::CommunityInfo& course,
    const td::CourseOverviewSnapshot* overview,
    const std::vector<td::FileRecord>& downloadedFiles,
    const std::string& offlineRootUtf8) {
    std::vector<DocStudio::GeminiNotebook::AcademicMaterialCandidate> out;
    const fs::path root(Utf8ToWide(offlineRootUtf8));

    for (const auto& f : downloadedFiles) {
        if (f.status != td::FileStatus::Downloaded && !f.deduplicated) continue;
        std::string rel = !f.logical_rel_path.empty() ? f.logical_rel_path : f.blob_rel_path;
        if (rel.empty()) continue;
        fs::path full = root / fs::path(Utf8ToWide(rel));
        std::error_code ec;
        if (!fs::exists(full, ec)) continue;

        DocStudio::GeminiNotebook::AcademicMaterialCandidate c{};
        c.inputKind = DocStudio::GeminiNotebook::SourceInputKind::LocalFile;
        c.localFilePathUtf8 = WideToUtf8(full.wstring());
        c.displayNameUtf8 = f.title.empty() ? f.name : f.title;
        out.push_back(std::move(c));
    }

    if (overview) {
        for (const auto& lnk : overview->external_links) {
            if (lnk.url.rfind("https://", 0) != 0 && lnk.url.rfind("http://", 0) != 0) continue;
            DocStudio::GeminiNotebook::AcademicMaterialCandidate c{};
            c.sourceUrlUtf8 = lnk.url;
            c.displayNameUtf8 = lnk.label.empty() ? course.name : lnk.label;
            c.inputKind = (lnk.category == "YouTube")
                              ? DocStudio::GeminiNotebook::SourceInputKind::YouTubeUrl
                              : DocStudio::GeminiNotebook::SourceInputKind::WebUrl;
            out.push_back(std::move(c));
        }
    }
    return out;
}
#endif

}  // namespace DocStudio::TecDigital
