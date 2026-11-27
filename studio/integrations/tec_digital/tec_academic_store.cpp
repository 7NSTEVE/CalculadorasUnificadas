#include "tec_academic_store.hpp"

#include "../../core/cudoc_storage.hpp"
#include "../../core/utf_conv.hpp"
#include "../../pdf/pdf_writer.hpp"
#include "academic/tec_digital/crypto.hpp"
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

std::string EnsureValidUtf8FromLatin1Fallback(std::string_view sv) {
    std::string out;
    out.reserve(sv.size() + 8);
    size_t i = 0;
    const size_t n = sv.size();
    while (i < n) {
        const unsigned char b0 = static_cast<unsigned char>(sv[i]);
        if (b0 < 0x80u) {
            out.push_back(static_cast<char>(b0));
            ++i;
            continue;
        }
        // Valid 2-byte UTF-8
        if ((b0 & 0xE0u) == 0xC0u && b0 >= 0xC2u && i + 1 < n) {
            const unsigned char b1 = static_cast<unsigned char>(sv[i + 1]);
            if ((b1 & 0xC0u) == 0x80u) {
                out.append(sv.data() + i, 2);
                i += 2;
                continue;
            }
        }
        // Valid 3-byte UTF-8
        if ((b0 & 0xF0u) == 0xE0u && i + 2 < n) {
            const unsigned char b1 = static_cast<unsigned char>(sv[i + 1]);
            const unsigned char b2 = static_cast<unsigned char>(sv[i + 2]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u &&
                !(b0 == 0xE0u && b1 < 0xA0u) && !(b0 == 0xEDu && b1 >= 0xA0u)) {
                out.append(sv.data() + i, 3);
                i += 3;
                continue;
            }
        }
        // Valid 4-byte UTF-8
        if ((b0 & 0xF8u) == 0xF0u && b0 <= 0xF4u && i + 3 < n) {
            const unsigned char b1 = static_cast<unsigned char>(sv[i + 1]);
            const unsigned char b2 = static_cast<unsigned char>(sv[i + 2]);
            const unsigned char b3 = static_cast<unsigned char>(sv[i + 3]);
            if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u &&
                !(b0 == 0xF0u && b1 < 0x90u) && !(b0 == 0xF4u && b1 >= 0x90u)) {
                out.append(sv.data() + i, 4);
                i += 4;
                continue;
            }
        }
        // Stray ISO-8859-1 / Windows-1252 byte -> recover into valid UTF-8 codepoint
        out.push_back(static_cast<char>(0xC0u | (b0 >> 6)));
        out.push_back(static_cast<char>(0x80u | (b0 & 0x3Fu)));
        ++i;
    }
    return out;
}

void SanitizeJsonUtf8InPlace(json& node) {
    if (node.is_string()) {
        std::string& s = node.get_ref<std::string&>();
        std::string clean = EnsureValidUtf8FromLatin1Fallback(s);
        if (clean != s) s = std::move(clean);
    } else if (node.is_array()) {
        for (auto& el : node) SanitizeJsonUtf8InPlace(el);
    } else if (node.is_object()) {
        // Check keys too in case a map path contains a stray non-UTF-8 byte
        std::vector<std::string> badKeys;
        for (auto it = node.begin(); it != node.end(); ++it) {
            SanitizeJsonUtf8InPlace(it.value());
            if (EnsureValidUtf8FromLatin1Fallback(it.key()) != it.key()) {
                badKeys.push_back(it.key());
            }
        }
        for (const auto& bk : badKeys) {
            std::string nk = EnsureValidUtf8FromLatin1Fallback(bk);
            json val = std::move(node[bk]);
            node.erase(bk);
            node[std::move(nk)] = std::move(val);
        }
    }
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
    LoadGlobalScheduleIndexFromDisk(2026, nullptr);
}

namespace {

std::string NormalizeGroupDigitsLocal(std::string s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    s = s.substr(a, b - a);
    if (s.empty()) return s;
    bool allDig = true;
    for (char ch : s) if (!std::isdigit(static_cast<unsigned char>(ch))) { allDig = false; break; }
    if (allDig) {
        size_t nz = s.find_first_not_of('0');
        return (nz == std::string::npos) ? std::string("0") : s.substr(nz);
    }
    return s;
}

std::string FormatMinutesHHMMStore(int mins) {
    int h = std::clamp(mins / 60, 0, 23);
    int m = std::clamp(mins % 60, 0, 59);
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return buf;
}

std::vector<td::WeeklyScheduleCell> DeriveScheduleCellsFromEnrolledAndGlobal(
    const std::vector<td::CommunityInfo>& communities,
    const GlobalHorariosIndex& idx) {
    std::vector<td::WeeklyScheduleCell> out;
    if (communities.empty() || idx.entries.empty()) return out;

    auto mapCampusCode = [](const std::string& tok) -> std::string {
        std::string u = tok;
        for (char& ch : u) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        if (u == "SC") return "San Carlos";
        if (u == "CA") return "Cartago";
        if (u == "SJ") return "San José";
        if (u == "AL") return "Alajuela";
        if (u == "LI") return "Limón";
        return "";
    };

    for (const auto& c : communities) {
        if (c.type != "CLASE" || !c.is_current_period) continue;
        // Parse slug like "S-2-2026.SC.EL2113.50" or fallback from url/name
        std::string code, grp, campusShort, perShort;
        {
            std::vector<std::string> parts;
            std::string cur;
            for (char ch : c.slug) {
                if (ch == '.') {
                    if (!cur.empty()) parts.push_back(cur);
                    cur.clear();
                } else {
                    cur.push_back(ch);
                }
            }
            if (!cur.empty()) parts.push_back(cur);
            for (const auto& p : parts) {
                if (p.size() >= 3 && (p[0] == 'S' || p[0] == 'V') && p[1] == '-') {
                    perShort = p.substr(0, 3);
                    continue;
                }
                std::string mappedCamp = mapCampusCode(p);
                if (!mappedCamp.empty() && campusShort.empty()) {
                    campusShort = mappedCamp;
                    continue;
                }
                if (code.empty() && p.size() >= 5 && p.size() <= 8 &&
                    std::isalpha(static_cast<unsigned char>(p[0])) &&
                    std::isalpha(static_cast<unsigned char>(p[1])) &&
                    std::isdigit(static_cast<unsigned char>(p.back()))) {
                    code = p;
                    continue;
                }
                if (!code.empty() && grp.empty() && p.size() <= 3 &&
                    std::all_of(p.begin(), p.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
                    grp = NormalizeGroupDigitsLocal(p);
                }
            }
        }
        if (code.empty()) continue;

        const GlobalHorariosGroupEntry* best = nullptr;
        int bestScore = -1;
        for (const auto& e : idx.entries) {
            if (SafeLower(e.courseCode) != SafeLower(code)) continue;
            if (e.sessions.empty()) continue;
            int score = 1;
            if (!grp.empty() && NormalizeGroupDigitsLocal(e.groupId) == grp) score += 8;
            if (!campusShort.empty() && SafeLower(e.campusShort) == SafeLower(campusShort)) score += 4;
            if (!perShort.empty() && SafeLower(e.periodCode) == SafeLower(perShort)) score += 2;
            if (score > bestScore) {
                bestScore = score;
                best = &e;
            }
        }
        if (!best || (!grp.empty() && bestScore < 9)) continue;

        for (const auto& s : best->sessions) {
            int d = static_cast<int>(s.day);
            if (d < 0 || d > 6 || s.endMinute <= s.startMinute) continue;
            td::WeeklyScheduleCell cell;
            cell.day_index_0_mon = d;
            cell.start_minute = s.startMinute;
            cell.end_minute = s.endMinute;
            cell.time_range = FormatMinutesHHMMStore(s.startMinute) + " - " + FormatMinutesHHMMStore(s.endMinute);
            cell.course_code = best->courseCode;
            cell.course_name = c.name.empty() ? best->courseName : c.name;
            cell.group = best->groupId;
            cell.room = !s.roomUtf8.empty() ? s.roomUtf8 : (!best->room.empty() ? best->room : best->campusShort);
            cell.raw_text = best->courseCode + " Gr " + best->groupId;
            out.push_back(std::move(cell));
        }
    }
    return out;
}

}  // namespace

TecAcademicStore::~TecAcademicStore() {
    StopAutoMonitor();
}

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
        next.overviewsByPath = snapshot_.overviewsByPath;
        next.evaluationsByPath = snapshot_.evaluationsByPath;
        next.newsByPath = snapshot_.newsByPath;
        next.newsArchiveByPath = snapshot_.newsArchiveByPath;
        next.courseCalendarsByPath = snapshot_.courseCalendarsByPath;
        next.smsByPath = snapshot_.smsByPath;
        next.membersByPath = snapshot_.membersByPath;
        next.forumsByPath = snapshot_.forumsByPath;
        next.gaapByPath = snapshot_.gaapByPath;
        next.rubricsByKey = snapshot_.rubricsByKey;
        next.customPagesBySpace = snapshot_.customPagesBySpace;
        next.customTabResourcesByCourse = snapshot_.customTabResourcesByCourse;
        next.personalDocuments = snapshot_.personalDocuments;
        next.personalDocumentsFolderId = snapshot_.personalDocumentsFolderId;
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
            if (next.expediente) {
                if (!next.expediente->structured.graduate_attributes.empty()) {
                    next.graduateAttributes = next.expediente->structured.graduate_attributes;
                }
                if (next.expediente->personal_info) {
                    next.personalAcademicInfo = next.expediente->personal_info;
                }
                if (!next.expediente->dar_sync_preview.empty()) {
                    next.darSyncPreview = next.expediente->dar_sync_preview;
                }
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
            std::string activePer;
            if (next.expediente && !next.expediente->active_schedule_period.empty()) {
                activePer = next.expediente->active_schedule_period;
            }
            if (activePer.empty()) activePer = opts.activePeriodHint;
            if (activePer.empty() && next.expediente && !next.expediente->periodos_combo.empty()) {
                activePer = next.expediente->periodos_combo.front().key;
            }
            if (auto sr = client.GetCurrentPeriodSchedule(activePer, sessionCarne); sr.ok && !sr.value.empty()) {
                next.currentSchedule = std::move(sr.value);
                if (next.expediente) next.expediente->structured.current_schedule = next.currentSchedule;
            }
        }
    }
    if (cancelled()) return false;

    // 3. Per-Course Overviews, Custom Pages, Detailed Evaluations, RSS & SMS (active semester FIRST!)
    std::vector<const td::CommunityInfo*> classList;
    std::vector<const td::CommunityInfo*> nonClassList;
    for (const auto& comm : next.communities) {
        if (comm.type == "CLASE") {
            if (!opts.activePeriodHint.empty() && comm.period != opts.activePeriodHint) continue;
            classList.push_back(&comm);
        } else {
            nonClassList.push_back(&comm);
        }
    }
    std::stable_sort(classList.begin(), classList.end(),
                     [](const td::CommunityInfo* a, const td::CommunityInfo* b) {
                         if (a->is_current_period != b->is_current_period) {
                             return a->is_current_period > b->is_current_period;
                         }
                         return a->period > b->period;
                     });
    auto spaceTypePriority = [](const std::string& t) -> int {
        if (t == "CATEDRA") return 0;
        if (t == "ESCUELA") return 1;
        if (t == "SUBGRUPO") return 2;
        if (t == "PROGRAMA") return 3;
        return 4;
    };
    std::stable_sort(nonClassList.begin(), nonClassList.end(),
                     [&](const td::CommunityInfo* a, const td::CommunityInfo* b) {
                         return spaceTypePriority(a->type) < spaceTypePriority(b->type);
                     });

    int deepCount = 0;
    std::vector<td::DetailedEvaluationsReport> allEvalReports;
    for (const td::CommunityInfo* commPtr : classList) {
        if (cancelled()) return false;
        if (deepCount >= opts.maxActiveCoursesToDeepSync) break;
        const auto& comm = *commPtr;
        ++deepCount;

        int pct = 38 + std::min(28, deepCount * 5);
        report(pct, "Sincronizando curso: " + comm.name);

        if (opts.syncCourseOverviews) {
            politePause();
            if (auto ov = client.GetCourseOverview(comm.url); ov.ok) {
                if (opts.syncCustomSpacePages && !cancelled()) {
                    auto pages = client.FetchCustomSpacePages(
                        comm.url, ov.value.tabs, opts.maxCustomTabsPerSpace, opts.politeDelayMs);
                    if (pages.ok && !pages.value.empty()) {
                        // Recursively follow internal ?page_num=N subpages linked inside portlets (e.g. S1..S16 in Contenidos)
                        if (opts.syncWeeklySubpages && !cancelled()) {
                            std::set<int> fetchedPnums;
                            for (const auto& pg : pages.value) fetchedPnums.insert(pg.page_num);
                            std::vector<td::CourseTabInfo> subQueue;
                            for (const auto& pg : pages.value) {
                                for (const auto& pt : pg.portlets) {
                                    for (const auto& sub : pt.linked_subpages) {
                                        if (sub.page_num > 0 && fetchedPnums.insert(sub.page_num).second) {
                                            subQueue.push_back(sub);
                                        }
                                    }
                                }
                            }
                            int subFetched = 0;
                            for (const auto& sub : subQueue) {
                                if (cancelled() || subFetched >= opts.maxSubpagesPerSpace) break;
                                politePause();
                                if (auto sp = client.GetSpacePage(comm.url, sub.page_num, sub.label); sp.ok) {
                                    // Prefer real weekly heading from subpage portlet when sub.label is generic
                                    const bool genericLbl = sp.value.tab_label.empty() ||
                                                            sp.value.tab_label.rfind("Sección ", 0) == 0 ||
                                                            sp.value.tab_label.find("ver m") != std::string::npos ||
                                                            sp.value.tab_label.find("loupe") != std::string::npos;
                                    if (genericLbl && !sp.value.portlets.empty() && !sp.value.portlets.front().title.empty()) {
                                        sp.value.tab_label = sp.value.portlets.front().title;
                                        for (auto& pg : pages.value) {
                                            for (auto& pt : pg.portlets) {
                                                for (auto& ls : pt.linked_subpages) {
                                                    if (ls.page_num == sub.page_num) ls.label = sp.value.tab_label;
                                                }
                                            }
                                        }
                                    }
                                    pages.value.push_back(std::move(sp.value));
                                    ++subFetched;
                                }
                            }
                        }
                        std::vector<td::CourseExternalLink> flatLinks;
                        for (const auto& pg : pages.value) {
                            flatLinks.insert(flatLinks.end(), pg.links.begin(), pg.links.end());
                            for (const auto& pt : pg.portlets) {
                                flatLinks.insert(flatLinks.end(), pt.links.begin(), pt.links.end());
                            }
                        }
                        if (!flatLinks.empty()) {
                            next.customTabResourcesByCourse[comm.url] = std::move(flatLinks);
                        }
                        ov.value.custom_pages = pages.value;
                        // Pre-cache embedded weekly module infographics (strictly read-only GET, capped for fast sync)
                        int cachedImgs = 0;
                        for (const auto& pg : pages.value) {
                            for (const auto& pt : pg.portlets) {
                                for (const auto& im : pt.images) {
                                    if (cancelled() || cachedImgs >= 10) break;
                                    if (!im.image_url.empty()) {
                                        if (!EnsureMediaImageCached(im.image_url).empty()) ++cachedImgs;
                                    }
                                }
                            }
                        }
                        next.customPagesBySpace[comm.url] = std::move(pages.value);
                    }
                } else if (opts.syncCustomTabs && !cancelled()) {
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
                if (opts.syncRubrics && !cancelled()) {
                    for (const auto& cat : ev.value.categories) {
                        for (const auto& it : cat.items) {
                            if (cancelled()) break;
                            if (!it.has_rubric || it.rubric_id.empty()) continue;
                            politePause();
                            const std::string rKey = it.rubric_id + "|" + it.task_item_id + "|" + it.task_id;
                            const std::string sid = !it.student_id.empty()
                                                        ? it.student_id
                                                        : service_->GraphQlClient().CachedUserId();
                            if (auto rr = client.GetRubricBreakdown(
                                    it.rubric_id, it.community_id, it.task_item_id, it.task_id, sid);
                                rr.ok && !rr.value.criteria.empty()) {
                                next.rubricsByKey[rKey] = std::move(rr.value);
                            }
                        }
                    }
                }
                allEvalReports.push_back(ev.value);
                next.evaluationsByPath[comm.url] = std::move(ev.value);
            }
        }
        if (opts.syncNewsRss && !cancelled()) {
            politePause();
            if (auto nr = client.GetCommunityNewsRss(comm.url); nr.ok) {
                next.newsByPath[comm.url] = std::move(nr.value);
            }
            if (auto na = client.GetNewsArchive(comm.url); na.ok && !na.value.empty()) {
                next.newsArchiveByPath[comm.url] = std::move(na.value);
            }
        }
        if (opts.syncCalendar && !cancelled()) {
            politePause();
            if (auto cc = client.GetCalendarEvents("", "list", comm.url); cc.ok) {
                if (!cc.value.empty()) {
                    for (auto& ce : cc.value) {
                        if (ce.calendar_name.empty()) ce.calendar_name = comm.name;
                    }
                    next.courseCalendarsByPath[comm.url] = std::move(cc.value);
                } else {
                    next.courseCalendarsByPath.erase(comm.url);
                }
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

    // 3b. Cátedras, Escuelas y Comunidades (portada, ?page_num=N, calendario, noticias, archivo y documentos de cátedra)
    if (opts.syncCatedrasAndCommunities && opts.syncCourseOverviews) {
        int ncCount = 0;
        for (const td::CommunityInfo* commPtr : nonClassList) {
            if (cancelled()) return false;
            if (ncCount >= opts.maxNonClassSpacesToDeepSync) break;
            const auto& comm = *commPtr;
            ++ncCount;
            report(68 + std::min(8, ncCount), "Sincronizando espacio: " + comm.name);
            politePause();
            if (auto ov = client.GetCourseOverview(comm.url); ov.ok) {
                if (opts.syncCustomSpacePages && !cancelled()) {
                    auto pages = client.FetchCustomSpacePages(
                        comm.url, ov.value.tabs, opts.maxCustomTabsPerSpace, opts.politeDelayMs);
                    if (pages.ok && !pages.value.empty()) {
                        // Recursively follow internal ?page_num=N subpages linked inside portlets (e.g. S1..S16 in Contenidos)
                        if (opts.syncWeeklySubpages && !cancelled()) {
                            std::set<int> fetchedPnums;
                            for (const auto& pg : pages.value) fetchedPnums.insert(pg.page_num);
                            std::vector<td::CourseTabInfo> subQueue;
                            for (const auto& pg : pages.value) {
                                for (const auto& pt : pg.portlets) {
                                    for (const auto& sub : pt.linked_subpages) {
                                        if (sub.page_num > 0 && fetchedPnums.insert(sub.page_num).second) {
                                            subQueue.push_back(sub);
                                        }
                                    }
                                }
                            }
                            int subFetched = 0;
                            for (const auto& sub : subQueue) {
                                if (cancelled() || subFetched >= opts.maxSubpagesPerSpace) break;
                                politePause();
                                if (auto sp = client.GetSpacePage(comm.url, sub.page_num, sub.label); sp.ok) {
                                    // Prefer real weekly heading from subpage portlet when sub.label is generic
                                    const bool genericLbl = sp.value.tab_label.empty() ||
                                                            sp.value.tab_label.rfind("Sección ", 0) == 0 ||
                                                            sp.value.tab_label.find("ver m") != std::string::npos ||
                                                            sp.value.tab_label.find("loupe") != std::string::npos;
                                    if (genericLbl && !sp.value.portlets.empty() && !sp.value.portlets.front().title.empty()) {
                                        sp.value.tab_label = sp.value.portlets.front().title;
                                        for (auto& pg : pages.value) {
                                            for (auto& pt : pg.portlets) {
                                                for (auto& ls : pt.linked_subpages) {
                                                    if (ls.page_num == sub.page_num) ls.label = sp.value.tab_label;
                                                }
                                            }
                                        }
                                    }
                                    pages.value.push_back(std::move(sp.value));
                                    ++subFetched;
                                }
                            }
                        }
                        std::vector<td::CourseExternalLink> flatLinks;
                        for (const auto& pg : pages.value) {
                            flatLinks.insert(flatLinks.end(), pg.links.begin(), pg.links.end());
                            for (const auto& pt : pg.portlets) {
                                flatLinks.insert(flatLinks.end(), pt.links.begin(), pt.links.end());
                            }
                        }
                        if (!flatLinks.empty()) {
                            next.customTabResourcesByCourse[comm.url] = std::move(flatLinks);
                        }
                        ov.value.custom_pages = pages.value;
                        // Pre-cache embedded weekly module infographics (strictly read-only GET, capped for fast sync)
                        int cachedImgs = 0;
                        for (const auto& pg : pages.value) {
                            for (const auto& pt : pg.portlets) {
                                for (const auto& im : pt.images) {
                                    if (cancelled() || cachedImgs >= 10) break;
                                    if (!im.image_url.empty()) {
                                        if (!EnsureMediaImageCached(im.image_url).empty()) ++cachedImgs;
                                    }
                                }
                            }
                        }
                        next.customPagesBySpace[comm.url] = std::move(pages.value);
                    }
                }
                next.overviewsByPath[comm.url] = std::move(ov.value);
            }
            if (opts.syncNewsRss && !cancelled()) {
                politePause();
                if (auto nr = client.GetCommunityNewsRss(comm.url); nr.ok && !nr.value.empty()) {
                    next.newsByPath[comm.url] = std::move(nr.value);
                }
                if (comm.type == "CATEDRA" || comm.type == "ESCUELA") {
                    if (auto na = client.GetNewsArchive(comm.url); na.ok && !na.value.empty()) {
                        next.newsArchiveByPath[comm.url] = std::move(na.value);
                    }
                }
            }
            if (opts.syncCalendar && opts.syncCatedraCalendarsAndDocs &&
                (comm.type == "CATEDRA" || comm.type == "ESCUELA") && !cancelled()) {
                politePause();
                if (auto cc = client.GetCalendarEvents("", "list", comm.url); cc.ok && !cc.value.empty()) {
                    for (auto& ce : cc.value) {
                        if (ce.calendar_name.empty()) ce.calendar_name = comm.name;
                    }
                    next.courseCalendarsByPath[comm.url] = std::move(cc.value);
                }
            }
            // Ensure EVERY Cátedra (e.g. Cálculo, Física II, Física III) has its document tree walked
            // so Programa / Cronograma / Semanas / Folletos are always indexed automatically.
            if (opts.syncCatedraCalendarsAndDocs && comm.type == "CATEDRA" &&
                next.foldersByPath.find(comm.url) == next.foldersByPath.end() && !cancelled()) {
                politePause();
                td::FileStorageWalker walker(
                    service_->GuardedTransport(), service_->Auth(),
                    service_->Config().base_url, service_->SnapshotSanitizeOptions());
                std::string fid = comm.root_folder_id;
                if (fid.empty()) {
                    std::vector<int> docPages;
                    if (auto ovit = next.overviewsByPath.find(comm.url); ovit != next.overviewsByPath.end()) {
                        for (const auto& tb : ovit->second.tabs) {
                            std::string l = tb.label;
                            for (char& ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                            if (l.find("doc") != std::string::npos || l.find("archivo") != std::string::npos) {
                                docPages.push_back(tb.page_num);
                            }
                        }
                    }
                    if (auto rf = walker.DiscoverRootFolderIds(comm.url, 5, docPages); rf.ok && !rf.value.empty()) {
                        fid = rf.value.front();
                    }
                }
                if (!fid.empty()) {
                    td::WalkerOptions wopts;
                    wopts.max_depth = 8;
                    wopts.max_folders = 250;
                    if (auto walked = walker.WalkFolderTree(fid, comm.slug, comm.type, comm.url, wopts);
                        walked.ok) {
                        next.foldersByPath[comm.url] = std::move(walked.value);
                    }
                }
            }
        }
    }
    if (cancelled()) return false;

    // 3b-bis. Global Cross-Space File Indexing across ALL remaining accessible spaces (34 spaces -> Biblioteca General)
    if (opts.syncAllSpaceFolders && !cancelled()) {
        td::FileStorageWalker walker(
            service_->GuardedTransport(), service_->Auth(),
            service_->Config().base_url, service_->SnapshotSanitizeOptions());
        int walkedTotal = static_cast<int>(next.foldersByPath.size());
        for (const auto& comm : next.communities) {
            if (cancelled() || walkedTotal >= opts.maxAllSpacesFolderWalk) break;
            if (comm.url.empty()) continue;
            if (next.foldersByPath.find(comm.url) != next.foldersByPath.end()) continue;
            politePause();
            std::string fid = comm.root_folder_id;
            if (fid.empty()) {
                std::vector<int> docPages;
                if (auto ovit = next.overviewsByPath.find(comm.url); ovit != next.overviewsByPath.end()) {
                    for (const auto& tb : ovit->second.tabs) {
                        std::string l = tb.label;
                        for (char& ch : l) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                        if (l.find("doc") != std::string::npos || l.find("archivo") != std::string::npos) {
                            docPages.push_back(tb.page_num);
                        }
                    }
                }
                if (auto rf = walker.DiscoverRootFolderIds(comm.url, 3, docPages); rf.ok && !rf.value.empty()) {
                    fid = rf.value.front();
                }
            }
            if (!fid.empty()) {
                td::WalkerOptions wopts;
                wopts.max_depth = 8;
                wopts.max_folders = 250;
                if (auto walked = walker.WalkFolderTree(fid, comm.slug, comm.type, comm.url, wopts);
                    walked.ok && (!walked.value.files.empty() || !walked.value.folders.empty())) {
                    next.foldersByPath[comm.url] = std::move(walked.value);
                    ++walkedTotal;
                }
            }
        }
    }
    if (cancelled()) return false;

    // 3b-ter. Native Motor de Juegos & GAAP server metadata
    if (opts.syncMotorJuegos && !cancelled()) {
        if (auto gs = client.GetGaapServer(); gs.ok && !gs.value.empty()) {
            next.gaapServerUrl = std::move(gs.value);
        }
        if (auto mu = client.GetMotorJuegosUser(); mu.ok) {
            next.motorJuegosUser = std::move(mu.value);
        }
        if (auto ma = client.GetMotorJuegosActivities(); ma.ok && !ma.value.empty()) {
            next.motorJuegosActivities = std::move(ma.value);
        }
    }

    // 3c. Personal "Mis Documentos" root folder metadata (strictly read-only walk)
    if (opts.syncPersonalDocuments && !cancelled()) {
        report(77, "Consultando Mis Documentos...");
        td::FileStorageWalker walker(
            service_->GuardedTransport(), service_->Auth(),
            service_->Config().base_url, service_->SnapshotSanitizeOptions());
        if (auto pfid = walker.DiscoverPersonalRootFolderId(); pfid.ok && !pfid.value.empty()) {
            next.personalDocumentsFolderId = pfid.value;
            td::WalkerOptions wopts;
            wopts.max_depth = 3;
            wopts.max_folders = 60;
            if (auto walked = walker.WalkFolderTree(pfid.value, "mis-documentos", "PERSONAL", "/dotlrn", wopts);
                walked.ok) {
                next.personalDocuments = std::move(walked.value);
            }
        }
    }
    if (cancelled()) return false;

    // 4. Unified Calendar (Portal + Per-Course Calendars + Evaluations Deadlines + Matricula Appointments)
    if (opts.syncCalendar) {
        report(80, "Construyendo calendario académico unificado...");
        if (auto cal = client.GetCalendarEvents("", "list"); cal.ok) {
            next.portalCalendarEvents = std::move(cal.value);
        }
        std::vector<td::CalendarEventEntry> combinedCal = next.portalCalendarEvents;
        for (const auto& [cpath, cevs] : next.courseCalendarsByPath) {
            (void)cpath;
            combinedCal.insert(combinedCal.end(), cevs.begin(), cevs.end());
        }
        std::vector<td::DetailedEvaluationsReport> allEvalsMerged;
        allEvalsMerged.reserve(next.evaluationsByPath.size());
        for (const auto& [epath, rep] : next.evaluationsByPath) {
            (void)epath;
            allEvalsMerged.push_back(rep);
        }
        std::vector<td::MatriculaAppointment> citas;
        if (next.expediente) citas = next.expediente->structured.citas_matricula;
        next.unifiedCalendarEvents = td::BuildUnifiedCalendarEvents(
            combinedCal, allEvalsMerged, citas);
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
        } else {
            next.warnings.push_back("Control de Plagio: " + pt.error_message);
        }
    }
    if (opts.syncPlagiarismCourseDocs && !cancelled()) {
        int pcCount = 0;
        for (const td::CommunityInfo* commPtr : classList) {
            if (cancelled() || pcCount >= opts.maxActiveCoursesToDeepSync) break;
            const auto& comm = *commPtr;
            std::string cid = comm.community_id;
            if (cid.empty()) {
                if (auto evit = next.evaluationsByPath.find(comm.url); evit != next.evaluationsByPath.end()) {
                    for (const auto& cat : evit->second.categories) {
                        for (const auto& it : cat.items) {
                            if (!it.community_id.empty()) { cid = it.community_id; break; }
                        }
                        if (!cid.empty()) break;
                    }
                }
            }
            if (cid.empty()) continue;
            ++pcCount;
            politePause();
            if (auto pd = client.GetPlagiarismCourseDocs(cid); pd.ok && !pd.value.empty()) {
                next.plagiarismDocsByCourse[comm.url] = std::move(pd.value);
            }
        }
    }
    if (opts.syncServerClock && !cancelled()) {
        if (auto sdt = client.GetServerDateTime(); sdt.ok && !sdt.value.empty()) {
            std::string raw = sdt.value;
            std::string digits;
            for (char ch : raw) {
                if (std::isdigit(static_cast<unsigned char>(ch))) digits.push_back(ch);
            }
            if (digits.size() >= 10) {
                try {
                    long long val = std::stoll(digits);
                    std::int64_t serverSec = (digits.size() >= 13) ? (val / 1000LL) : val;
                    std::int64_t localSec = static_cast<std::int64_t>(std::time(nullptr));
                    next.serverTimeOffsetSec = serverSec - localSec;
                    std::time_t stt = static_cast<std::time_t>(serverSec);
                    std::tm utcTm{};
#ifdef _WIN32
                    gmtime_s(&utcTm, &stt);
#else
                    gmtime_r(&stt, &utcTm);
#endif
                    char isoBuf[32]{};
                    std::snprintf(isoBuf, sizeof(isoBuf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                  utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday,
                                  utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec);
                    next.serverDateTimeIso = isoBuf;
                } catch (...) {
                    next.serverDateTimeIso = raw;
                }
            } else {
                next.serverDateTimeIso = raw;
            }
        }
    }
    if (opts.syncPrematricula && !cancelled()) {
        std::string planStr;
        if (next.curriculumGraph && next.curriculumGraph->id_curriculum > 0) {
            planStr = std::to_string(next.curriculumGraph->id_curriculum);
        } else if (next.studentCareer && next.studentCareer->plan > 0) {
            planStr = std::to_string(next.studentCareer->plan);
        }
        if (auto pm = client.GetPrematriculaSnapshot("", planStr); pm.ok) {
            if (!pm.value.courses.empty() || !pm.value.requests.empty() ||
                !pm.value.disabled_courses.empty() || pm.value.max_credits > 0) {
                next.prematricula = std::move(pm.value);
            }
        }
    }
    if (opts.syncConsentStatus && !cancelled()) {
        for (const char* mod : {"tda-um", "tda-control-plagio"}) {
            if (auto cs = client.GetConsentStatus(mod); cs.ok) {
                std::string v = cs.value;
                while (!v.empty() && std::isspace(static_cast<unsigned char>(v.front()))) v.erase(v.begin());
                while (!v.empty() && std::isspace(static_cast<unsigned char>(v.back()))) v.pop_back();
                std::string low = v;
                for (char& ch : low) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                if (v.empty() || low == "{}" || low == "[]" || low == "null" ||
                    low.find("true") != std::string::npos || low.find("\"1\"") != std::string::npos ||
                    low.find("acept") != std::string::npos) {
                    next.consentStatusByModule[mod] = "Aceptado / al día";
                } else if (low.find("false") != std::string::npos || low.find("\"0\"") != std::string::npos) {
                    next.consentStatusByModule[mod] = "Pendiente";
                } else {
                    if (v.size() > 80) v = v.substr(0, 77) + "...";
                    next.consentStatusByModule[mod] = std::move(v);
                }
            }
        }
    }
    if (opts.syncCurriculum && next.curriculumColors.color_codes.empty() && !cancelled()) {
        if (auto cc = client.GetCurriculumColorCodes(); cc.ok) {
            next.curriculumColors = std::move(cc.value);
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
            std::string uid = gql.CachedUserId();
            if (!sessionCarne.empty()) {
                if (auto gi = gql.FetchStudentGeneralInfo(sessionCarne); gi.ok) {
                    if (uid.empty() && !gi.value.user_id.empty()) uid = gi.value.user_id;
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
                if (auto ah = gql.FetchAcademicHistoryFull(sessionCarne); ah.ok) {
                    if (!next.expediente) {
                        next.expediente = td::ExpedienteSummary{};
                    }
                    auto& dst = next.expediente->structured.stats;
                    const auto& src = ah.value.stats;
                    if (dst.weighted_avg_general <= 0.0 && src.weighted_avg_general > 0.0) dst.weighted_avg_general = src.weighted_avg_general;
                    if (dst.weighted_avg_passed <= 0.0 && src.weighted_avg_passed > 0.0) dst.weighted_avg_passed = src.weighted_avg_passed;
                    if (dst.weighted_avg_failed <= 0.0 && src.weighted_avg_failed > 0.0) dst.weighted_avg_failed = src.weighted_avg_failed;
                    if (dst.passed_pct <= 0.0 && src.passed_pct > 0.0) dst.passed_pct = src.passed_pct;
                    if (dst.failed_pct <= 0.0 && src.failed_pct > 0.0) dst.failed_pct = src.failed_pct;
                    if (dst.hours_taken == 0 && src.hours_taken > 0) dst.hours_taken = src.hours_taken;
                    if (dst.credits_passed == 0 && src.credits_passed > 0) dst.credits_passed = src.credits_passed;
                    if (dst.credits_failed == 0 && src.credits_failed > 0) dst.credits_failed = src.credits_failed;
                    if (dst.credits_total == 0 && src.credits_total > 0) dst.credits_total = src.credits_total;
                    if (dst.credits_no_grade == 0 && src.credits_no_grade > 0) dst.credits_no_grade = src.credits_no_grade;
                    if (dst.subjects_taken == 0 && src.subjects_taken > 0) dst.subjects_taken = src.subjects_taken;
                    if (dst.subjects_consulted == 0 && src.subjects_consulted > 0) dst.subjects_consulted = src.subjects_consulted;
                    if (dst.count_aprobadas == 0 && src.count_aprobadas > 0) dst.count_aprobadas = src.count_aprobadas;
                    if (dst.count_en_curso == 0 && src.count_en_curso > 0) dst.count_en_curso = src.count_en_curso;
                    if (dst.count_reprobadas == 0 && src.count_reprobadas > 0) dst.count_reprobadas = src.count_reprobadas;
                    if (dst.count_retiradas == 0 && src.count_retiradas > 0) dst.count_retiradas = src.count_retiradas;
                    if (dst.count_reconocidas == 0 && src.count_reconocidas > 0) dst.count_reconocidas = src.count_reconocidas;
                    if (dst.count_suficiencia == 0 && src.count_suficiencia > 0) dst.count_suficiencia = src.count_suficiencia;
                    if (dst.count_pendientes == 0 && src.count_pendientes > 0) dst.count_pendientes = src.count_pendientes;
                    if (dst.count_congeladas == 0 && src.count_congeladas > 0) dst.count_congeladas = src.count_congeladas;
                    if (!ah.value.grades.empty() &&
                        ah.value.grades.size() > next.expediente->structured.grades.size()) {
                        // detalle_notas often returns only the active semester subset;
                        // prefer the complete multi-semester history while preserving any extra rows.
                        auto merged = std::move(ah.value.grades);
                        for (const auto& dn : next.expediente->structured.grades) {
                            bool exists = false;
                            for (const auto& mg : merged) {
                                if (mg.year == dn.year && mg.period == dn.period &&
                                    ((!dn.course_code.empty() && mg.course_code == dn.course_code) ||
                                     mg.course_name == dn.course_name)) {
                                    exists = true;
                                    break;
                                }
                            }
                            if (!exists) merged.push_back(dn);
                        }
                        next.expediente->structured.grades = std::move(merged);
                    }
                }
            }
            if (uid.empty()) uid = gql.CachedUserId();
            if (!sessionCarne.empty() && !uid.empty()) {
                if (auto pg = gql.FetchPendingGraduationCourses(uid, sessionCarne); pg.ok) {
                    next.pendingGraduationCourses = std::move(pg.value);
                }
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

    std::vector<td::TecChangeEvent> newlyDetected;
    ChangeNotifyCallback notifyCbCopy;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        const TecAcademicSnapshot prevSnap = snapshot_;
        next.monitorIntervalMinutes = prevSnap.monitorIntervalMinutes;
        // Preserve already-synced folders for any space not re-walked during a quick sync
        for (const auto& [fp, fval] : prevSnap.foldersByPath) {
            if (next.foldersByPath.find(fp) == next.foldersByPath.end()) {
                next.foldersByPath[fp] = fval;
            }
        }
        newlyDetected = ComputeSnapshotChanges(prevSnap, next);
        // Merge new events at the front of activityFeed (dedup by id, cap 200)
        std::vector<td::TecChangeEvent> mergedFeed = newlyDetected;
        std::set<std::string> seenIds;
        for (const auto& ev : mergedFeed) seenIds.insert(ev.id);
        for (auto oldEv : prevSnap.activityFeed) {
            if (mergedFeed.size() >= 200) break;
            if (oldEv.kind == td::TecChangeEventKind::NewWeeklyContent) {
                auto cpit = next.customPagesBySpace.find(oldEv.space_url);
                if (cpit != next.customPagesBySpace.end()) {
                    const int pn = std::atoi(oldEv.target_id.c_str());
                    for (const auto& pg : cpit->second) {
                        if (pg.page_num == pn && !pg.tab_label.empty()) {
                            oldEv.title = "Nuevo módulo publicado: " + pg.tab_label;
                            if (!pg.portlets.empty() && !pg.portlets.front().title.empty() &&
                                pg.portlets.front().title != pg.tab_label) {
                                oldEv.detail = pg.portlets.front().title;
                            } else {
                                oldEv.detail = "Módulo semanal disponible";
                            }
                            break;
                        }
                    }
                }
            }
            if (seenIds.insert(oldEv.id).second) {
                mergedFeed.push_back(std::move(oldEv));
            }
        }
        next.activityFeed = std::move(mergedFeed);
        snapshot_ = std::move(next);
    }
    SaveSnapshotToDisk(nullptr);
    if (!newlyDetected.empty()) {
        {
            std::lock_guard<std::mutex> mlk(monitorMutex_);
            notifyCbCopy = monitorNotifyCb_;
        }
        if (notifyCbCopy) {
            notifyCbCopy(newlyDetected);
        }
    }
    report(100, newlyDetected.empty()
                    ? "Sincronización completada sin cambios nuevos."
                    : ("Sincronización completada · " + std::to_string(newlyDetected.size()) + " novedad(es) detectada(s)."));
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
    opts.max_depth = 5;
    opts.max_folders = 80;
    auto res = service_->SyncFolderStorage(opts);
    if (!res.ok) {
        if (outErr) *outErr = res.error_message;
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        td::FolderWalkResult fwr;
        if (!res.value.folders.empty()) {
            fwr.root_folder_id = res.value.folders.front().folder_id;
        }
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

bool TecAcademicStore::SyncCourseOnDemand(const std::string& communityUrlOrPath,
                                          std::string* outErr) {
    if (communityUrlOrPath.empty()) {
        if (outErr) *outErr = "Curso inválido.";
        return false;
    }
    auto& client = service_->TdaClient();
    std::string commId;
    std::string commName;
    bool isClass = true;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        for (const auto& c : snapshot_.communities) {
            if (c.url == communityUrlOrPath) {
                commId = c.community_id;
                commName = c.name;
                isClass = (c.type == "CLASE");
                break;
            }
        }
    }

    bool anyOk = false;
    std::optional<td::CourseOverviewSnapshot> ovOpt;
    std::vector<td::SpacePageSnapshot> pages;
    std::optional<td::DetailedEvaluationsReport> evOpt;
    std::vector<td::RssNewsItem> newsRss;
    std::vector<td::RssNewsItem> newsArch;
    std::vector<td::CalendarEventEntry> calEvs;
    std::vector<td::SmsMessageRecord> sms;
    std::vector<td::CommunityMember> members;
    std::vector<td::ForumThreadSummary> forums;
    std::vector<td::GaapActivityRecord> gaap;

    if (auto ov = client.GetCourseOverview(communityUrlOrPath); ov.ok) {
        anyOk = true;
        if (auto pg = client.FetchCustomSpacePages(communityUrlOrPath, ov.value.tabs, 8, 120); pg.ok) {
            pages = pg.value;
            ov.value.custom_pages = pages;
        }
        ovOpt = std::move(ov.value);
    } else if (ov.error_code == "session_expired") {
        authenticated_.store(false, std::memory_order_release);
        if (outErr) *outErr = "Sesión expirada. Inicie sesión nuevamente.";
        return false;
    }

    std::map<std::string, td::RubricBreakdown> syncedRubrics;
    if (isClass) {
        if (auto ev = client.GetDetailedCourseEvaluations(communityUrlOrPath); ev.ok) {
            anyOk = true;
            for (const auto& cat : ev.value.categories) {
                for (const auto& it : cat.items) {
                    if (!it.has_rubric || it.rubric_id.empty()) continue;
                    const std::string rKey = it.rubric_id + "|" + it.task_item_id + "|" + it.task_id;
                    const std::string sid = !it.student_id.empty()
                                                ? it.student_id
                                                : service_->GraphQlClient().CachedUserId();
                    if (auto rr = client.GetRubricBreakdown(
                            it.rubric_id, it.community_id, it.task_item_id, it.task_id, sid);
                        rr.ok && !rr.value.criteria.empty()) {
                        syncedRubrics[rKey] = std::move(rr.value);
                    }
                }
            }
            evOpt = std::move(ev.value);
        }
    }
    if (auto nr = client.GetCommunityNewsRss(communityUrlOrPath); nr.ok) {
        anyOk = true;
        newsRss = std::move(nr.value);
    }
    if (auto na = client.GetNewsArchive(communityUrlOrPath); na.ok) {
        anyOk = true;
        newsArch = std::move(na.value);
    }
    if (auto cc = client.GetCalendarEvents("", "list", communityUrlOrPath); cc.ok) {
        anyOk = true;
        for (auto& ce : cc.value) {
            if (ce.calendar_name.empty()) ce.calendar_name = commName;
        }
        calEvs = std::move(cc.value);
    }
    if (isClass) {
        if (auto sm = client.GetCourseSmsMessages(communityUrlOrPath); sm.ok) sms = std::move(sm.value);
        if (auto mb = client.GetCommunityMembers(communityUrlOrPath, commId, 200); mb.ok) members = std::move(mb.value);
        if (auto fr = client.GetCommunityForums(communityUrlOrPath); fr.ok) forums = std::move(fr.value);
        if (auto gp = client.GetGaapActivityHistory(communityUrlOrPath); gp.ok) gaap = std::move(gp.value);
    }

    if (!anyOk) {
        if (outErr) *outErr = "No se pudo consultar el curso en TEC Digital.";
        return false;
    }

    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (ovOpt) snapshot_.overviewsByPath[communityUrlOrPath] = std::move(*ovOpt);
        if (!pages.empty()) snapshot_.customPagesBySpace[communityUrlOrPath] = std::move(pages);
        if (evOpt) snapshot_.evaluationsByPath[communityUrlOrPath] = std::move(*evOpt);
        for (auto& [rk, rval] : syncedRubrics) {
            snapshot_.rubricsByKey[rk] = std::move(rval);
        }
        if (!newsRss.empty()) snapshot_.newsByPath[communityUrlOrPath] = std::move(newsRss);
        if (!newsArch.empty()) snapshot_.newsArchiveByPath[communityUrlOrPath] = std::move(newsArch);
        if (!calEvs.empty()) snapshot_.courseCalendarsByPath[communityUrlOrPath] = std::move(calEvs);
        if (!sms.empty()) snapshot_.smsByPath[communityUrlOrPath] = std::move(sms);
        if (!members.empty()) snapshot_.membersByPath[communityUrlOrPath] = std::move(members);
        if (!forums.empty()) snapshot_.forumsByPath[communityUrlOrPath] = std::move(forums);
        if (!gaap.empty()) snapshot_.gaapByPath[communityUrlOrPath] = std::move(gaap);

        // Refresh unified calendar with newly loaded course calendar & evaluation deadlines
        std::vector<td::CalendarEventEntry> combinedCal = snapshot_.portalCalendarEvents;
        for (const auto& [cp, evs] : snapshot_.courseCalendarsByPath) {
            (void)cp;
            combinedCal.insert(combinedCal.end(), evs.begin(), evs.end());
        }
        std::vector<td::DetailedEvaluationsReport> allEvals;
        allEvals.reserve(snapshot_.evaluationsByPath.size());
        for (const auto& [ep, rep] : snapshot_.evaluationsByPath) {
            (void)ep;
            allEvals.push_back(rep);
        }
        std::vector<td::MatriculaAppointment> citas;
        if (snapshot_.expediente) citas = snapshot_.expediente->structured.citas_matricula;
        snapshot_.unifiedCalendarEvents = td::BuildUnifiedCalendarEvents(combinedCal, allEvals, citas);
    }
    SaveSnapshotToDisk(nullptr);
    return true;
}

bool TecAcademicStore::SyncPersonalDocuments(
    bool downloadFiles,
    std::string* outSummaryUtf8,
    std::string* outErr) {
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto pfid = walker.DiscoverPersonalRootFolderId();
    if (!pfid.ok || pfid.value.empty()) {
        if (outErr) *outErr = pfid.error_message.empty() ? "No se encontró la carpeta Mis Documentos." : pfid.error_message;
        return false;
    }
    if (downloadFiles && ComputeOfflineCacheBytes() > offlineBudgetBytes_) {
        PruneOfflineCacheOlderThanBytes(offlineBudgetBytes_ * 8 / 10);
    }
    td::WalkerOptions wopts;
    wopts.max_depth = 5;
    wopts.max_folders = 150;
    auto walked = walker.WalkFolderTree(pfid.value, "mis-documentos", "PERSONAL", "/dotlrn", wopts);
    if (!walked.ok) {
        if (outErr) *outErr = walked.error_message;
        return false;
    }
    size_t downloaded = 0;
    if (downloadFiles) {
        td::StorageDownloader dl(
            service_->GuardedTransport(), service_->Auth(),
            service_->Config().base_url, service_->Config().downloader_options,
            service_->SnapshotSanitizeOptions());
        for (auto& f : walked.value.files) {
            auto dr = dl.DownloadFile(f, offlineFilesRootUtf8_, "/dotlrn");
            if (dr.outcome == td::DownloadOutcome::Downloaded || dr.outcome == td::DownloadOutcome::Deduplicated) {
                ++downloaded;
            }
        }
        service_->MaterialIndex().SetOfflineRoot(offlineFilesRootUtf8_);
        service_->MaterialIndex().IndexDirectory(offlineFilesRootUtf8_);
    }
    const size_t nFolders = walked.value.folders.size();
    const size_t nFiles = walked.value.files.size();
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.personalDocumentsFolderId = pfid.value;
        snapshot_.personalDocuments = std::move(walked.value);
    }
    SaveSnapshotToDisk(nullptr);
    if (outSummaryUtf8) {
        *outSummaryUtf8 = "Mis Documentos · Carpetas: " + std::to_string(nFolders) +
                          " · Archivos: " + std::to_string(nFiles) +
                          (downloadFiles ? (" · Descargados: " + std::to_string(downloaded)) : "");
    }
    return true;
}

td::Result<td::SpacePageSnapshot> TecAcademicStore::FetchSpacePage(
    const std::string& communityUrlOrPath,
    int pageNum,
    const std::string& tabLabel) {
    auto res = service_->TdaClient().GetSpacePage(communityUrlOrPath, pageNum, tabLabel);
    if (res.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        auto& vec = snapshot_.customPagesBySpace[communityUrlOrPath];
        bool replaced = false;
        for (auto& p : vec) {
            if (p.page_num == pageNum) {
                p = res.value;
                replaced = true;
                break;
            }
        }
        if (!replaced) vec.push_back(res.value);
    }
    return res;
}

td::Result<td::NewsArticleDetail> TecAcademicStore::FetchNewsArticle(const std::string& newsUrl) {
    return service_->TdaClient().GetNewsArticleDetail(newsUrl);
}

td::Result<td::EvaluationUploadResult> TecAcademicStore::UploadCourseEvaluation(
    const td::EvaluationUploadRequest& req,
    bool userConfirmed) {
    if (!userConfirmed) {
        return td::Result<td::EvaluationUploadResult>::Failure(
            "confirmation_required", "Se requiere confirmación explícita del usuario para subir una evaluación.");
    }
    td::ScopedUserMutationGuard guard(
        service_->GuardedTransport(), td::AllowedStudentMutation::UploadEvaluation);
    return service_->TdaClient().UploadEvaluationSubmission(req);
}

td::Result<bool> TecAcademicStore::DeleteEvaluationSubmission(
    const std::string& communityUrlOrPath,
    const std::string& revisionId,
    const std::string& itemId,
    bool userConfirmed) {
    if (!userConfirmed) {
        return td::Result<bool>::Failure(
            "confirmation_required", "Se requiere confirmación explícita del usuario para eliminar la entrega.");
    }
    td::ScopedUserMutationGuard guard(
        service_->GuardedTransport(), td::AllowedStudentMutation::DeleteEvaluationRevision);
    return service_->TdaClient().DeleteEvaluationRevision(communityUrlOrPath, revisionId, itemId);
}

td::Result<bool> TecAcademicStore::MarkNotificationAsRead(
    const std::string& notificationId,
    bool userConfirmed) {
    if (!userConfirmed) {
        return td::Result<bool>::Failure(
            "confirmation_required", "Se requiere confirmación explícita del usuario.");
    }
    td::ScopedUserMutationGuard guard(
        service_->GuardedTransport(), td::AllowedStudentMutation::ManageNotifications);
    auto r = service_->TdaClient().MarkNotificationRead(notificationId);
    if (r.ok && r.value) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (snapshot_.notifications) {
            for (auto& n : snapshot_.notifications->notifications) {
                if (n.notification_id == notificationId && n.state == "U") {
                    n.state = "R";
                    if (snapshot_.notifications->unread_quantity > 0) {
                        --snapshot_.notifications->unread_quantity;
                    }
                    break;
                }
            }
        }
    }
    SaveSnapshotToDisk(nullptr);
    return r;
}

td::Result<bool> TecAcademicStore::DeleteUserNotification(
    const std::string& notificationId,
    bool userConfirmed) {
    if (!userConfirmed) {
        return td::Result<bool>::Failure(
            "confirmation_required", "Se requiere confirmación explícita del usuario.");
    }
    td::ScopedUserMutationGuard guard(
        service_->GuardedTransport(), td::AllowedStudentMutation::ManageNotifications);
    auto r = service_->TdaClient().DeleteUserNotification(notificationId);
    if (r.ok && r.value) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (snapshot_.notifications) {
            auto& vec = snapshot_.notifications->notifications;
            for (auto it = vec.begin(); it != vec.end(); ++it) {
                if (it->notification_id == notificationId) {
                    if (it->state == "U" && snapshot_.notifications->unread_quantity > 0) {
                        --snapshot_.notifications->unread_quantity;
                    }
                    vec.erase(it);
                    break;
                }
            }
        }
    }
    SaveSnapshotToDisk(nullptr);
    return r;
}

td::Result<bool> TecAcademicStore::RefreshEnrolledCoursesFromDar(bool userConfirmed) {
    if (!userConfirmed) {
        return td::Result<bool>::Failure(
            "confirmation_required", "Se requiere confirmación explícita del usuario.");
    }
    td::ScopedUserMutationGuard guard(
        service_->GuardedTransport(), td::AllowedStudentMutation::RefreshEnrolledCourses);
    return service_->TdaClient().RefreshEnrolledCoursesFromDar();
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
    const std::string& taskId,
    const std::string& studentId) {
    auto res = service_->TdaClient().GetRubricBreakdown(rubricId, communityId, taskItemId, taskId, studentId);
    if (res.ok) {
        const std::string key = rubricId + "|" + taskItemId + "|" + taskId;
        {
            std::lock_guard<std::mutex> lk(snapMutex_);
            snapshot_.rubricsByKey[key] = res.value;
        }
        SaveSnapshotToDisk(nullptr);
    }
    return res;
}

td::Result<std::string> TecAcademicStore::FetchPlagiarismReportUrl(const std::string& docUid) {
    return service_->TdaClient().GetPlagiarismReportUrl(docUid);
}

td::Result<std::string> TecAcademicStore::FetchCommunityMembersCsv(const std::string& communityUrlOrPath) {
    return service_->TdaClient().GetCommunityMembersCsv(communityUrlOrPath);
}

td::Result<td::FileDetailedMetadata> TecAcademicStore::FetchFileDetails(const std::string& objectId) {
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto res = walker.FetchFileDetails(objectId);
    if (res.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        auto patchFiles = [&](td::FolderWalkResult& fwr) {
            for (auto& f : fwr.files) {
                if (f.object_id == objectId) {
                    if (!res.value.last_modified_ansi.empty()) f.last_modified_ansi = res.value.last_modified_ansi;
                    if (!res.value.live_revision.empty()) f.live_revision = res.value.live_revision;
                    if (!res.value.author_name.empty()) f.author_name = res.value.author_name;
                    if (!res.value.mime_type.empty()) f.mime_type = res.value.mime_type;
                    if (res.value.content_size > 0) f.content_size = res.value.content_size;
                }
            }
        };
        for (auto& [p, fwr] : snapshot_.foldersByPath) { (void)p; patchFiles(fwr); }
        if (snapshot_.personalDocuments) patchFiles(*snapshot_.personalDocuments);
    }
    return res;
}

std::string TecAcademicStore::CachedMediaImagePathIfPresent(const std::string& imageUrl) const {
    if (imageUrl.empty()) return {};
    const std::string hashHex = td::Sha256Hex(
        reinterpret_cast<const uint8_t*>(imageUrl.data()), imageUrl.size());
    std::string ext = ".img";
    std::string low = imageUrl;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (low.find(".png") != std::string::npos) ext = ".png";
    else if (low.find(".jpg") != std::string::npos || low.find(".jpeg") != std::string::npos) ext = ".jpg";
    else if (low.find(".gif") != std::string::npos) ext = ".gif";
    else if (low.find(".webp") != std::string::npos) ext = ".webp";

    fs::path dst = fs::path(Utf8ToWide(storageRootUtf8_)) / L"cache" / L"media_thumbs" /
                   fs::path(Utf8ToWide(hashHex.substr(0, 24) + ext));
    std::error_code ec;
    if (fs::exists(dst, ec) && fs::file_size(dst, ec) > 64) {
        return WideToUtf8(dst.wstring());
    }
    return {};
}

std::string TecAcademicStore::EnsureMediaImageCached(const std::string& imageUrl) {
    if (imageUrl.empty()) return {};
    const std::string hashHex = td::Sha256Hex(
        reinterpret_cast<const uint8_t*>(imageUrl.data()), imageUrl.size());
    std::string ext = ".img";
    std::string low = imageUrl;
    for (char& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (low.find(".png") != std::string::npos) ext = ".png";
    else if (low.find(".jpg") != std::string::npos || low.find(".jpeg") != std::string::npos) ext = ".jpg";
    else if (low.find(".gif") != std::string::npos) ext = ".gif";
    else if (low.find(".webp") != std::string::npos) ext = ".webp";

    fs::path dir = fs::path(Utf8ToWide(storageRootUtf8_)) / L"cache" / L"media_thumbs";
    std::error_code ec;
    fs::create_directories(dir, ec);
    fs::path dst = dir / fs::path(Utf8ToWide(hashHex.substr(0, 24) + ext));
    if (fs::exists(dst, ec) && fs::file_size(dst, ec) > 64) {
        return WideToUtf8(dst.wstring());
    }
    auto dl = service_->TdaClient().DownloadBinaryResource(imageUrl, 3 * 1024 * 1024);
    if (!dl.ok || dl.value.size() < 32) return {};
    // Reject HTML responses masquerading as images
    std::string head = dl.value.substr(0, std::min<size_t>(dl.value.size(), 64));
    for (char& c : head) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (head.find("<!doc") != std::string::npos || head.find("<html") != std::string::npos) return {};
    std::ofstream ofs(dst, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) return {};
    ofs.write(dl.value.data(), static_cast<std::streamsize>(dl.value.size()));
    ofs.close();
    return WideToUtf8(dst.wstring());
}

std::vector<td::TecChangeEvent> TecAcademicStore::ComputeSnapshotChanges(
    const TecAcademicSnapshot& prev,
    const TecAcademicSnapshot& next) {
    std::vector<td::TecChangeEvent> out;
    // Skip initial baseline sync so first login never floods 1,300+ fake "new file" alerts
    if (prev.communities.empty() && prev.foldersByPath.empty() && prev.evaluationsByPath.empty()) {
        return out;
    }
    const std::string nowUtc = !next.syncedAtUtc.empty() ? next.syncedAtUtc : td::CredentialVault::CurrentUtcIso8601();
    std::unordered_map<std::string, std::string> spaceNameByUrl;
    for (const auto& c : next.communities) spaceNameByUrl[c.url] = c.name;
    for (const auto& c : prev.communities) {
        if (spaceNameByUrl.find(c.url) == spaceNameByUrl.end()) spaceNameByUrl[c.url] = c.name;
    }
    auto spaceLabel = [&](const std::string& url) -> std::string {
        auto it = spaceNameByUrl.find(url);
        if (it != spaceNameByUrl.end() && !it->second.empty()) return it->second;
        return url;
    };

    // 1. Files & Folders diff across ALL spaces + Mis Documentos
    auto diffFolderWalk = [&](const std::string& spUrl, const std::string& spName,
                              const td::FolderWalkResult* oldW, const td::FolderWalkResult& newW) {
        if (!oldW) return; // Newly indexed space on first walk — baseline only
        std::unordered_map<std::string, const td::FileRecord*> oldFiles;
        for (const auto& f : oldW->files) {
            if (!f.object_id.empty()) oldFiles[f.object_id] = &f;
        }
        std::unordered_set<std::string> oldDirs;
        for (const auto& d : oldW->folders) {
            if (!d.folder_id.empty()) oldDirs.insert(d.folder_id);
        }
        for (const auto& d : newW.folders) {
            if (d.folder_id.empty() || d.folder_id == newW.root_folder_id) continue;
            if (oldDirs.count(d.folder_id) == 0) {
                td::TecChangeEvent ev;
                ev.id = "dir_new:" + d.folder_id;
                ev.kind = td::TecChangeEventKind::NewFolder;
                ev.detected_at_utc = nowUtc;
                ev.space_url = spUrl;
                ev.space_name = spName;
                ev.title = "Nueva carpeta: " + d.name;
                ev.detail = spName + (d.folder_path.empty() ? "" : (" · " + d.folder_path));
                ev.target_id = d.folder_id;
                out.push_back(std::move(ev));
            }
        }
        for (const auto& f : newW.files) {
            if (f.object_id.empty()) continue;
            auto it = oldFiles.find(f.object_id);
            if (it == oldFiles.end()) {
                td::TecChangeEvent ev;
                ev.id = "file_new:" + f.object_id;
                ev.kind = td::TecChangeEventKind::NewFile;
                ev.detected_at_utc = nowUtc;
                ev.space_url = spUrl;
                ev.space_name = spName;
                ev.title = "Archivo nuevo: " + f.name;
                ev.detail = spName + (f.folder_path.empty() ? " · Raíz" : (" · " + f.folder_path));
                if (!f.last_modified_ansi.empty()) ev.detail += " · " + f.last_modified_ansi.substr(0, std::min<size_t>(16, f.last_modified_ansi.size()));
                ev.target_id = f.object_id;
                out.push_back(std::move(ev));
            } else {
                const td::FileRecord& of = *it->second;
                const bool modTs = !f.last_modified_ansi.empty() && !of.last_modified_ansi.empty() &&
                                   f.last_modified_ansi != of.last_modified_ansi;
                const bool modRev = !f.live_revision.empty() && !of.live_revision.empty() &&
                                    f.live_revision != of.live_revision;
                const bool modSz = f.content_size > 0 && of.content_size > 0 && f.content_size != of.content_size;
                if (modTs || modRev || modSz) {
                    td::TecChangeEvent ev;
                    ev.id = "file_mod:" + f.object_id + ":" + (!f.live_revision.empty() ? f.live_revision : (!f.last_modified_ansi.empty() ? f.last_modified_ansi : std::to_string(f.content_size)));
                    ev.kind = td::TecChangeEventKind::ModifiedFile;
                    ev.detected_at_utc = nowUtc;
                    ev.space_url = spUrl;
                    ev.space_name = spName;
                    ev.title = "Archivo modificado: " + f.name;
                    ev.detail = spName + (f.folder_path.empty() ? " · Raíz" : (" · " + f.folder_path));
                    if (!f.last_modified_ansi.empty()) ev.detail += " · Actualizado: " + f.last_modified_ansi.substr(0, std::min<size_t>(16, f.last_modified_ansi.size()));
                    ev.target_id = f.object_id;
                    out.push_back(std::move(ev));
                }
            }
        }
    };

    for (const auto& [spUrl, newW] : next.foldersByPath) {
        auto pit = prev.foldersByPath.find(spUrl);
        diffFolderWalk(spUrl, spaceLabel(spUrl), pit != prev.foldersByPath.end() ? &pit->second : nullptr, newW);
    }
    if (next.personalDocuments && prev.personalDocuments) {
        diffFolderWalk("/dotlrn", "Mis Documentos", &(*prev.personalDocuments), *next.personalDocuments);
    }

    // 2. Evaluations & Grades diff (newly published grades, grade changes, teacher comments, new assignments)
    for (const auto& [cUrl, newRep] : next.evaluationsByPath) {
        auto pit = prev.evaluationsByPath.find(cUrl);
        if (pit == prev.evaluationsByPath.end()) continue;
        std::unordered_map<std::string, const td::DetailedEvaluationItem*> oldItems;
        for (const auto& cat : pit->second.categories) {
            for (const auto& it : cat.items) {
                const std::string k = !it.assign_id.empty() ? it.assign_id : (cat.name + "|" + it.name);
                oldItems[k] = &it;
            }
        }
        const std::string cName = spaceLabel(cUrl);
        for (const auto& cat : newRep.categories) {
            for (const auto& it : cat.items) {
                const std::string k = !it.assign_id.empty() ? it.assign_id : (cat.name + "|" + it.name);
                auto oit = oldItems.find(k);
                double g100 = td::ComputeEvaluationGrade100(it);
                if (oit == oldItems.end()) {
                    td::TecChangeEvent ev;
                    ev.id = "eval_new:" + cUrl + ":" + k;
                    ev.kind = td::TecChangeEventKind::NewEvaluation;
                    ev.detected_at_utc = nowUtc;
                    ev.space_url = cUrl;
                    ev.space_name = cName;
                    ev.title = "Nueva evaluación: " + it.name;
                    ev.detail = cName + " · " + cat.name + (it.due_date_text.empty() ? "" : (" · Vence: " + it.due_date_text));
                    ev.target_id = k;
                    out.push_back(std::move(ev));
                    continue;
                }
                const auto& oldIt = *oit->second;
                double oldG100 = td::ComputeEvaluationGrade100(oldIt);
                if (!oldIt.is_graded && it.is_graded) {
                    char gbuf[32]{};
                    std::snprintf(gbuf, sizeof(gbuf), "%.1f", g100 >= 0.0 ? g100 : it.earned_pct);
                    td::TecChangeEvent ev;
                    ev.id = "grade_pub:" + cUrl + ":" + k + ":" + gbuf;
                    ev.kind = td::TecChangeEventKind::GradePublished;
                    ev.detected_at_utc = nowUtc;
                    ev.space_url = cUrl;
                    ev.space_name = cName;
                    ev.title = "Evaluación calificada: " + it.name + " (" + std::string(gbuf) + " / 100)";
                    ev.detail = cName + " · " + cat.name + (it.teacher_comment.empty() ? "" : (" · " + it.teacher_comment));
                    ev.target_id = k;
                    out.push_back(std::move(ev));
                } else if (oldIt.is_graded && it.is_graded && g100 >= 0.0 && oldG100 >= 0.0 &&
                           std::abs(g100 - oldG100) >= 0.05) {
                    char gbuf[64]{};
                    std::snprintf(gbuf, sizeof(gbuf), "%.1f → %.1f", oldG100, g100);
                    td::TecChangeEvent ev;
                    ev.id = "grade_chg:" + cUrl + ":" + k + ":" + std::to_string(static_cast<int>(std::round(g100 * 10.0)));
                    ev.kind = td::TecChangeEventKind::GradeChanged;
                    ev.detected_at_utc = nowUtc;
                    ev.space_url = cUrl;
                    ev.space_name = cName;
                    ev.title = "Nota actualizada: " + it.name + " (" + std::string(gbuf) + ")";
                    ev.detail = cName + " · " + cat.name;
                    ev.target_id = k;
                    out.push_back(std::move(ev));
                }
                if (!it.teacher_comment.empty() && it.teacher_comment != oldIt.teacher_comment) {
                    td::TecChangeEvent ev;
                    ev.id = "eval_cmt:" + cUrl + ":" + k + ":" + std::to_string(it.teacher_comment.size());
                    ev.kind = td::TecChangeEventKind::NewTeacherComment;
                    ev.detected_at_utc = nowUtc;
                    ev.space_url = cUrl;
                    ev.space_name = cName;
                    ev.title = "Nuevo comentario docente en " + it.name;
                    ev.detail = cName + " · " + it.teacher_comment;
                    ev.target_id = k;
                    out.push_back(std::move(ev));
                }
            }
        }
    }

    std::unordered_set<std::string> prevCommunityUrls;
    for (const auto& c : prev.communities) prevCommunityUrls.insert(c.url);

    // 3. Rubrics newly graded or updated
    for (const auto& [rk, newRb] : next.rubricsByKey) {
        auto pit = prev.rubricsByKey.find(rk);
        const bool newlySyncedAndGraded = (pit == prev.rubricsByKey.end() && !prev.evaluationsByPath.empty() && newRb.is_graded);
        const bool transitionedToGraded = (pit != prev.rubricsByKey.end() && !pit->second.is_graded && newRb.is_graded);
        const bool scoreUpdated = (pit != prev.rubricsByKey.end() && pit->second.is_graded && newRb.is_graded &&
                                   std::abs(pit->second.total_score - newRb.total_score) >= 0.05);
        if (newlySyncedAndGraded || transitionedToGraded || scoreUpdated) {
            char sbuf[32]{};
            std::snprintf(sbuf, sizeof(sbuf), "%.1f", newRb.total_score);
            td::TecChangeEvent ev;
            ev.id = "rubric_grd:" + rk + ":" + sbuf;
            ev.kind = td::TecChangeEventKind::RubricGraded;
            ev.detected_at_utc = nowUtc;
            ev.title = "Rúbrica calificada: " + (newRb.title.empty() ? rk : newRb.title) + " (" + std::string(sbuf) + " pts)";
            ev.detail = std::to_string(newRb.criteria.size()) + " criterios evaluados";
            ev.target_id = rk;
            out.push_back(std::move(ev));
        }
    }

    // 4. New Course & Cátedra News / Announcements
    for (const auto& [spUrl, newList] : next.newsByPath) {
        auto pit = prev.newsByPath.find(spUrl);
        if (pit == prev.newsByPath.end() && prevCommunityUrls.count(spUrl) == 0) continue;
        std::unordered_set<std::string> oldTitles;
        if (pit != prev.newsByPath.end()) {
            for (const auto& n : pit->second) oldTitles.insert(n.title + "|" + n.link);
        }
        const std::string spName = spaceLabel(spUrl);
        for (const auto& n : newList) {
            if (oldTitles.count(n.title + "|" + n.link) == 0 && !n.title.empty()) {
                td::TecChangeEvent ev;
                ev.id = "news:" + spUrl + ":" + n.title;
                ev.kind = td::TecChangeEventKind::NewNews;
                ev.detected_at_utc = nowUtc;
                ev.space_url = spUrl;
                ev.space_name = spName;
                ev.title = "Nuevo aviso en " + spName + ": " + n.title;
                ev.detail = n.description.empty() ? n.pub_date : n.description;
                ev.target_id = n.link;
                out.push_back(std::move(ev));
            }
        }
    }

    // 5. New Weekly / Custom Subpages discovered in Cátedras & Courses
    for (const auto& [spUrl, newPages] : next.customPagesBySpace) {
        auto pit = prev.customPagesBySpace.find(spUrl);
        if (pit == prev.customPagesBySpace.end() && prevCommunityUrls.count(spUrl) == 0) continue;
        std::unordered_set<int> oldPnums;
        if (pit != prev.customPagesBySpace.end()) {
            for (const auto& pg : pit->second) oldPnums.insert(pg.page_num);
        }
        const std::string spName = spaceLabel(spUrl);
        for (const auto& pg : newPages) {
            if (oldPnums.count(pg.page_num) == 0) {
                td::TecChangeEvent ev;
                ev.id = "wk_page:" + spUrl + ":" + std::to_string(pg.page_num);
                ev.kind = td::TecChangeEventKind::NewWeeklyContent;
                ev.detected_at_utc = nowUtc;
                ev.space_url = spUrl;
                ev.space_name = spName;
                ev.title = "Nuevo módulo publicado: " + (pg.tab_label.empty() ? ("Sección " + std::to_string(pg.page_num)) : pg.tab_label);
                std::string wkDet;
                if (!pg.portlets.empty() && !pg.portlets.front().title.empty() &&
                    pg.portlets.front().title != pg.tab_label) {
                    wkDet = pg.portlets.front().title;
                } else {
                    wkDet = "Módulo semanal disponible";
                }
                ev.detail = std::move(wkDet);
                ev.target_id = std::to_string(pg.page_num);
                out.push_back(std::move(ev));
            }
        }
    }

    return out;
}

std::vector<GlobalFileCatalogEntry> TecAcademicStore::BuildGlobalFileCatalog(const TecAcademicSnapshot& snap) {
    std::vector<GlobalFileCatalogEntry> out;
    auto foldLower = [](const std::string& raw) -> std::string {
        std::string s;
        s.reserve(raw.size());
        for (size_t i = 0; i < raw.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(raw[i]);
            if (c == 0xC3 && i + 1 < raw.size()) {
                unsigned char n = static_cast<unsigned char>(raw[i + 1]);
                if (n == 0xA1 || n == 0x81) { s.push_back('a'); ++i; continue; }
                if (n == 0xA9 || n == 0x89) { s.push_back('e'); ++i; continue; }
                if (n == 0xAD || n == 0x8D) { s.push_back('i'); ++i; continue; }
                if (n == 0xB3 || n == 0x93) { s.push_back('o'); ++i; continue; }
                if (n == 0xBA || n == 0x9A) { s.push_back('u'); ++i; continue; }
                if (n == 0xB1 || n == 0x91) { s.push_back('n'); ++i; continue; }
            }
            s.push_back(static_cast<char>(std::tolower(c)));
        }
        return s;
    };

    std::unordered_map<std::string, const td::CommunityInfo*> commByUrl;
    for (const auto& c : snap.communities) commByUrl[c.url] = &c;

    // Collect URLs/names referenced on portadas/custom pages so we can flag "Solo en directorio" files
    std::string portadaRefsFolded;
    for (const auto& [u, ov] : snap.overviewsByPath) {
        (void)u;
        for (const auto& d : ov.official_docs) portadaRefsFolded += " " + foldLower(d.url + " " + d.label);
        for (const auto& l : ov.external_links) portadaRefsFolded += " " + foldLower(l.url + " " + l.label);
        for (const auto& p : ov.portlets) {
            for (const auto& l : p.links) portadaRefsFolded += " " + foldLower(l.url + " " + l.label);
            for (const auto& im : p.images) portadaRefsFolded += " " + foldLower(im.image_url + " " + im.link_url);
        }
    }
    for (const auto& [u, pgs] : snap.customPagesBySpace) {
        (void)u;
        for (const auto& pg : pgs) {
            for (const auto& l : pg.links) portadaRefsFolded += " " + foldLower(l.url + " " + l.label);
            for (const auto& p : pg.portlets) {
                for (const auto& l : p.links) portadaRefsFolded += " " + foldLower(l.url + " " + l.label);
            }
        }
    }

    std::unordered_set<std::string> recentChangedObjIds;
    for (const auto& ev : snap.activityFeed) {
        if (ev.unread && (ev.kind == td::TecChangeEventKind::NewFile || ev.kind == td::TecChangeEventKind::ModifiedFile)) {
            recentChangedObjIds.insert(ev.target_id);
        }
    }

    std::unordered_set<std::string> seenKeys;
    auto pushFile = [&](const std::string& spUrl, const std::string& spName, const std::string& spType,
                        const td::FileRecord& f, bool isPersonal) {
        const std::string fn = !f.name.empty() ? f.name : f.title;
        if (fn.empty()) return;
        const std::string dedup = spUrl + "|" + (!f.object_id.empty() ? f.object_id : (f.folder_path + "/" + fn));
        if (!seenKeys.insert(dedup).second) return;
        GlobalFileCatalogEntry e;
        e.spaceUrl = spUrl;
        e.spaceName = spName;
        e.spaceType = spType;
        e.folderPath = f.folder_path.empty() ? "Raíz" : f.folder_path;
        e.parentFolderId = f.parent_folder_id;
        e.fileName = fn;
        e.title = !f.title.empty() ? f.title : fn;
        e.objectId = f.object_id;
        e.mimeType = f.mime_type;
        e.contentSize = f.content_size;
        e.creationDateAnsi = f.creation_date_ansi;
        e.lastModifiedAnsi = !f.last_modified_ansi.empty() ? f.last_modified_ansi : f.creation_date_ansi;
        e.liveRevision = f.live_revision;
        e.authorName = f.author_name;
        e.downloadUrl = f.download_url;
        e.localRelPath = !f.logical_rel_path.empty() ? f.logical_rel_path : f.blob_rel_path;
        e.isLocal = (f.status == td::FileStatus::Downloaded || f.deduplicated);
        e.isPersonal = isPersonal;
        e.subscriptionActive = f.subscription_active;
        e.isRecentlyChanged = !f.object_id.empty() && recentChangedObjIds.count(f.object_id) > 0;
        const std::string fnLow = foldLower(fn);
        e.isLinkedInPortada = (!f.object_id.empty() && portadaRefsFolded.find(f.object_id) != std::string::npos) ||
                              (fnLow.size() >= 6 && portadaRefsFolded.find(fnLow) != std::string::npos);
        e.searchFolded = foldLower(fn + " " + e.title + " " + e.folderPath + " " + spName + " " + spType + " " + e.authorName);
        out.push_back(std::move(e));
    };

    for (const auto& [spUrl, fwr] : snap.foldersByPath) {
        std::string spName = spUrl;
        std::string spType = "CLASE";
        if (auto it = commByUrl.find(spUrl); it != commByUrl.end()) {
            spName = it->second->name;
            spType = it->second->type;
        }
        for (const auto& f : fwr.files) pushFile(spUrl, spName, spType, f, false);
    }
    if (snap.personalDocuments) {
        for (const auto& f : snap.personalDocuments->files) {
            pushFile("/dotlrn", "Mis Documentos", "PERSONAL", f, true);
        }
    }
    // Include evaluation attachments / enunciados / solutions / feedback links
    for (const auto& [cUrl, rep] : snap.evaluationsByPath) {
        std::string spName = rep.community_slug;
        if (auto it = commByUrl.find(cUrl); it != commByUrl.end()) spName = it->second->name;
        for (const auto& cat : rep.categories) {
            for (const auto& it : cat.items) {
                auto addEvalDoc = [&](const std::string& url, const std::string& role) {
                    if (url.empty()) return;
                    const std::string dedup = "eval|" + url;
                    if (!seenKeys.insert(dedup).second) return;
                    GlobalFileCatalogEntry e;
                    e.spaceUrl = cUrl;
                    e.spaceName = spName;
                    e.spaceType = "EVALUACION";
                    e.folderPath = "Evaluaciones / " + cat.name;
                    e.fileName = role + " — " + it.name;
                    e.title = e.fileName;
                    e.objectId = it.assign_id;
                    e.downloadUrl = url;
                    e.lastModifiedAnsi = it.due_date_iso;
                    e.creationDateAnsi = it.due_date_iso;
                    e.isEvalAttachment = true;
                    e.isLinkedInPortada = true;
                    e.searchFolded = foldLower(e.fileName + " " + e.folderPath + " " + spName + " evaluacion");
                    out.push_back(std::move(e));
                };
                addEvalDoc(it.attachment_url, "Enunciado");
                addEvalDoc(it.solution_url, "Solución");
                addEvalDoc(it.feedback_url, "Retroalimentación");
            }
        }
    }

    // Default sort: recently changed first, then newest modification/creation timestamp descending, then name
    std::stable_sort(out.begin(), out.end(), [](const GlobalFileCatalogEntry& a, const GlobalFileCatalogEntry& b) {
        if (a.isRecentlyChanged != b.isRecentlyChanged) return a.isRecentlyChanged > b.isRecentlyChanged;
        if (a.lastModifiedAnsi != b.lastModifiedAnsi) return a.lastModifiedAnsi > b.lastModifiedAnsi;
        return a.fileName < b.fileName;
    });
    return out;
}

void TecAcademicStore::MarkAllChangesRead() {
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        for (auto& ev : snapshot_.activityFeed) ev.unread = false;
    }
    SaveSnapshotToDisk(nullptr);
}

void TecAcademicStore::MarkChangeRead(const std::string& eventId) {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        for (auto& ev : snapshot_.activityFeed) {
            if (ev.id == eventId && ev.unread) {
                ev.unread = false;
                changed = true;
                break;
            }
        }
    }
    if (changed) SaveSnapshotToDisk(nullptr);
}

void TecAcademicStore::ConfigureAutoMonitor(int intervalMinutes, ChangeNotifyCallback onNewChanges) {
    StopAutoMonitor();
    intervalMinutes = std::clamp(intervalMinutes, 0, 240);
    {
        std::lock_guard<std::mutex> slk(snapMutex_);
        snapshot_.monitorIntervalMinutes = intervalMinutes;
    }
    SaveSnapshotToDisk(nullptr);

    std::lock_guard<std::mutex> mlk(monitorMutex_);
    monitorIntervalMin_ = intervalMinutes;
    monitorNotifyCb_ = std::move(onNewChanges);
    monitorStop_ = false;
    if (intervalMinutes <= 0) return;

    monitorThread_ = std::thread([this, intervalMinutes]() {
        while (true) {
            {
                std::unique_lock<std::mutex> lk(monitorMutex_);
                monitorCv_.wait_for(lk, std::chrono::minutes(intervalMinutes), [this]() { return monitorStop_; });
                if (monitorStop_) break;
            }
            if (!IsAuthenticated()) continue;
            // Lightweight periodic change-check sync (courses, catedra news/files, evaluations, rubrics, notifications)
            TecSyncOptions mopts;
            mopts.syncSchoolCatalog = false;
            mopts.syncCurriculum = false;
            mopts.syncUserModel = false;
            mopts.syncExpediente = false;
            mopts.syncAllSpaceFolders = false; // Re-walk active courses + catedras fast; full 34-space runs on manual SyncAll
            mopts.syncCatedraCalendarsAndDocs = true;
            mopts.politeDelayMs = 250;
            SyncAll(mopts, nullptr, nullptr, nullptr);
        }
    });
}

void TecAcademicStore::StopAutoMonitor() {
    {
        std::lock_guard<std::mutex> lk(monitorMutex_);
        monitorStop_ = true;
    }
    monitorCv_.notify_all();
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }
}

int TecAcademicStore::AutoMonitorIntervalMinutes() const noexcept {
    std::lock_guard<std::mutex> lk(snapMutex_);
    return snapshot_.monitorIntervalMinutes;
}

td::Result<bool> TecAcademicStore::DeleteAllUserNotifications(bool userConfirmed) {
    if (!userConfirmed) return td::Result<bool>::Failure("confirmation_required", "Confirmación requerida.");
    auto r = service_->TdaClient().DeleteAllUserNotifications();
    if (r.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (snapshot_.notifications) {
            snapshot_.notifications->notifications.clear();
            snapshot_.notifications->unread_quantity = 0;
        }
    }
    if (r.ok) SaveSnapshotToDisk(nullptr);
    return r;
}

td::Result<bool> TecAcademicStore::ToggleFolderNotificationSubscription(const std::string& objectId, bool subscribe) {
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto r = walker.ToggleFolderNotificationSubscription(objectId, subscribe);
    if (r.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        auto patch = [&](td::FolderWalkResult& fwr) {
            for (auto& d : fwr.folders) if (d.folder_id == objectId) d.subscription_active = subscribe;
            for (auto& f : fwr.files) if (f.object_id == objectId) f.subscription_active = subscribe;
        };
        for (auto& [p, fwr] : snapshot_.foldersByPath) { (void)p; patch(fwr); }
        if (snapshot_.personalDocuments) patch(*snapshot_.personalDocuments);
        SaveSnapshotToDisk(nullptr);
    }
    return r;
}

td::Result<bool> TecAcademicStore::RequestEvaluationNotifications(
    const std::string& communityUrlOrPath, const std::string& assignId, bool enabled) {
    return service_->TdaClient().RequestEvaluationNotifications(communityUrlOrPath, assignId, enabled);
}

td::Result<td::StudentMutationResult> TecAcademicStore::SyncExpedienteDarCourses(bool userConfirmed) {
    if (!userConfirmed) return td::Result<td::StudentMutationResult>::Failure("confirmation_required", "Confirmación requerida.");
    auto r = service_->TdaClient().SyncExpedienteDarCourses();
    if (r.ok) {
        service_->TdaClient().RefreshEnrolledCoursesFromDar();
    }
    return r;
}

td::Result<td::StudentMutationResult> TecAcademicStore::UploadPlagiarismFile(
    const std::string& folderId, const std::string& localFilePathUtf8) {
    fs::path p(Utf8ToWide(localFilePathUtf8));
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) {
        return td::Result<td::StudentMutationResult>::Failure("io_error", "No se pudo abrir el archivo local.");
    }
    std::string bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (bytes.empty()) {
        return td::Result<td::StudentMutationResult>::Failure("empty_file", "El archivo seleccionado está vacío.");
    }
    std::string fn = WideToUtf8(p.filename().wstring());
    std::string mime = "application/octet-stream";
    std::string ext = WideToUtf8(p.extension().wstring());
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".pdf") mime = "application/pdf";
    else if (ext == ".docx") mime = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    else if (ext == ".doc") mime = "application/msword";
    else if (ext == ".txt") mime = "text/plain";
    return service_->TdaClient().UploadPlagiarismFile(folderId, fn, mime, bytes);
}

td::Result<td::StudentMutationResult> TecAcademicStore::SubmitPlagiarismEphorus(
    const std::string& docUid, const std::string& itemId) {
    return service_->TdaClient().SubmitPlagiarismEphorus(docUid, itemId);
}

td::Result<td::StudentMutationResult> TecAcademicStore::UpdatePlagiarismVisibility(
    const std::string& docUid, bool visible) {
    return service_->TdaClient().UpdatePlagiarismVisibility(docUid, visible);
}

td::Result<td::StudentMutationResult> TecAcademicStore::SavePrematriculaSelection(
    const std::string& preEnrollId, const std::string& curriculumId, const std::string& courseCodesCsv) {
    return service_->TdaClient().SavePrematriculaSelection(preEnrollId, curriculumId, courseCodesCsv);
}

td::Result<td::StudentMutationResult> TecAcademicStore::SubmitInclusionRequest(
    const std::string& campaignId, const std::string& courseCode,
    const std::string& group, const std::string& comment) {
    const std::string carne = service_->TdaClient().ResolveStudentCarne();
    return service_->GraphQlClient().SubmitInclusionRequest(campaignId, carne, courseCode, group, comment);
}

td::Result<td::StudentMutationResult> TecAcademicStore::DeleteInclusionRequest(
    const std::string& requestId, bool userConfirmed) {
    if (!userConfirmed) return td::Result<td::StudentMutationResult>::Failure("confirmation_required", "Confirmación requerida.");
    return service_->GraphQlClient().DeleteInclusionRequest(requestId);
}

td::Result<td::StudentMutationResult> TecAcademicStore::RegisterFairProject(
    const std::string& fairId, const std::string& title,
    const std::string& description, const std::string& categoryId, const std::string& sedeId) {
    return service_->TdaClient().RegisterFairProject(fairId, title, description, categoryId, sedeId);
}

td::Result<td::StudentMutationResult> TecAcademicStore::SaveUserPrivacySettings(const std::string& queryParams) {
    return service_->TdaClient().SaveUserPrivacySettings(queryParams);
}

td::Result<td::StudentMutationResult> TecAcademicStore::SubmitConsentAnswer(
    const std::string& moduleKey, bool accepted) {
    auto r = service_->TdaClient().SubmitConsentAnswer(moduleKey, accepted);
    if (r.ok) {
        std::lock_guard<std::mutex> lk(snapMutex_);
        snapshot_.consentStatusByModule[moduleKey] = accepted ? "Aceptado / al día" : "Rechazado";
        SaveSnapshotToDisk(nullptr);
    }
    return r;
}

td::Result<std::string> TecAcademicStore::CreatePersonalFolder(
    const std::string& parentFolderId, const std::string& folderName) {
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto r = walker.CreatePersonalFolder(parentFolderId, folderName);
    if (r.ok) SyncPersonalDocuments(false, nullptr, nullptr);
    return r;
}

td::Result<bool> TecAcademicStore::UploadPersonalFile(
    const std::string& parentFolderId, const std::string& localFilePathUtf8) {
    fs::path p(Utf8ToWide(localFilePathUtf8));
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs.is_open()) return td::Result<bool>::Failure("io_error", "No se pudo leer el archivo local.");
    std::string bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return td::Result<bool>::Failure("empty_file", "El archivo seleccionado está vacío.");
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto r = walker.UploadPersonalFile(parentFolderId, WideToUtf8(p.filename().wstring()), "application/octet-stream", bytes);
    if (r.ok) SyncPersonalDocuments(false, nullptr, nullptr);
    return r;
}

td::Result<bool> TecAcademicStore::DeletePersonalItem(
    const std::string& objectId, bool isFolder, bool userConfirmed) {
    if (!userConfirmed) return td::Result<bool>::Failure("confirmation_required", "Confirmación requerida.");
    td::FileStorageWalker walker(
        service_->GuardedTransport(), service_->Auth(),
        service_->Config().base_url, service_->SnapshotSanitizeOptions());
    auto r = walker.DeletePersonalItem(objectId, isFolder);
    if (r.ok) SyncPersonalDocuments(false, nullptr, nullptr);
    return r;
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

// ============================================================================
// Global All-TEC Schedule Search Index (All Schools, Campuses, Careers)
// ============================================================================
namespace {

std::string FoldSpanishLowerStore(std::string_view sv) {
    std::string out;
    out.reserve(sv.size());
    for (size_t i = 0; i < sv.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(sv[i]);
        if (c == 0xC3u && i + 1 < sv.size()) {
            unsigned char n = static_cast<unsigned char>(sv[i + 1]);
            switch (n) {
                case 0xA1u: case 0x81u: out.push_back('a'); ++i; continue;
                case 0xA9u: case 0x89u: out.push_back('e'); ++i; continue;
                case 0xADu: case 0x8Du: out.push_back('i'); ++i; continue;
                case 0xB3u: case 0x93u: out.push_back('o'); ++i; continue;
                case 0xBAu: case 0x9Au: case 0xBCu: case 0x9Cu: out.push_back('u'); ++i; continue;
                case 0xB1u: case 0x91u: out.push_back('n'); ++i; continue;
                default: break;
            }
        }
        out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

std::string TrimStore(std::string_view sv) {
    size_t b = 0;
    while (b < sv.size() && std::isspace(static_cast<unsigned char>(sv[b]))) ++b;
    size_t e = sv.size();
    while (e > b && std::isspace(static_cast<unsigned char>(sv[e - 1]))) --e;
    return std::string(sv.substr(b, e - b));
}

std::string NormalizeCampusShortStore(std::string_view raw) {
    std::string low = FoldSpanishLowerStore(raw);
    if (low.find("cartago") != std::string::npos || low == "ca") return "Cartago";
    if (low.find("san jose") != std::string::npos || low == "sj") return "San José";
    if (low.find("san carlos") != std::string::npos || low == "sc") return "San Carlos";
    if (low.find("alajuela") != std::string::npos || low == "al") return "Alajuela";
    if (low.find("limon") != std::string::npos || low == "lm" || low == "li") return "Limón";
    return TrimStore(raw);
}

int ParseDayIndexStore(std::string_view raw) {
    std::string low = FoldSpanishLowerStore(TrimStore(raw));
    if (low == "l" || low.find("lunes") != std::string::npos) return 0;
    if (low == "k" || low.find("martes") != std::string::npos) return 1;
    if (low == "m" || low.find("miercoles") != std::string::npos) return 2;
    if (low == "j" || low.find("jueves") != std::string::npos) return 3;
    if (low == "v" || low.find("viernes") != std::string::npos) return 4;
    if (low == "s" || low.find("sabado") != std::string::npos) return 5;
    if (low == "d" || low.find("domingo") != std::string::npos) return 6;
    return -1;
}

int ParseHhMmMinutesStore(std::string_view raw) {
    std::string s = TrimStore(raw);
    size_t colon = s.find(':');
    if (colon == std::string::npos) return -1;
    int hh = std::atoi(s.substr(0, colon).c_str());
    int mm = std::atoi(s.substr(colon + 1).c_str());
    if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return -1;
    return hh * 60 + mm;
}

std::string FormatMinutesStore(int mins) {
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "%02d:%02d", mins / 60, mins % 60);
    return buf;
}

std::string DayShortLabelStore(ht::DayOfWeek d) {
    static const char* kDays[7] = {"L", "K", "M", "J", "V", "S", "D"};
    const auto idx = static_cast<size_t>(d);
    return idx < 7 ? kDays[idx] : "?";
}

std::string BuildPeriodLabelStore(const std::string& modCode, const std::string& perNum, const std::string& dscMod) {
    std::string mc = SafeLower(TrimStore(modCode));
    std::string pn = TrimStore(perNum);
    if (mc == "s") return "Semestre " + (pn.empty() ? "1" : pn);
    if (mc == "v") return "Verano";
    if (mc == "b") return "Bimestre " + (pn.empty() ? "1" : pn);
    if (mc == "c") return "Cuatrimestre " + (pn.empty() ? "1" : pn);
    if (mc == "t") return "Trimestre " + (pn.empty() ? "1" : pn);
    if (mc == "h") return "Humanísticas " + (pn.empty() ? "1" : pn);
    if (!dscMod.empty()) return dscMod + (pn.empty() ? "" : (" " + pn));
    return modCode + (pn.empty() ? "" : ("-" + pn));
}

void FinalizeGroupEntryMetadata(GlobalHorariosGroupEntry& e) {
    std::sort(e.sessions.begin(), e.sessions.end(),
              [](const ht::TimeSlot& a, const ht::TimeSlot& b) {
                  if (a.day != b.day) return static_cast<int>(a.day) < static_cast<int>(b.day);
                  return a.startMinute < b.startMinute;
              });
    e.daysMask = 0;
    e.hasNightSession = false;
    std::ostringstream sum;
    for (size_t i = 0; i < e.sessions.size(); ++i) {
        const auto& s = e.sessions[i];
        const auto dIdx = static_cast<unsigned>(s.day);
        if (dIdx < 7u) e.daysMask = static_cast<std::uint8_t>(e.daysMask | (1u << dIdx));
        if (s.startMinute >= 17 * 60 || s.endMinute > 19 * 60) e.hasNightSession = true;
        if (i > 0) sum << " · ";
        sum << DayShortLabelStore(s.day) << " " << FormatMinutesStore(s.startMinute)
            << "-" << FormatMinutesStore(s.endMinute);
        if (!s.roomUtf8.empty() && e.room.empty()) e.room = s.roomUtf8;
    }
    e.scheduleSummary = sum.str();
    std::string blob = e.courseCode + " " + e.courseName + " " + e.groupId + " " +
                       e.schoolCode + " " + e.schoolName + " " + e.careerName + " " +
                       e.campusShort + " " + e.campusFull + " " + e.periodCode + " " +
                       e.periodLabel + " " + e.modality + " " + e.professor + " " +
                       e.room + " " + e.scheduleSummary;
    e.searchFolded = FoldSpanishLowerStore(blob);
}

void RebuildGlobalIndexFacets(GlobalHorariosIndex& idx) {
    std::set<std::string> campuses, schools, careers, periods, modalities, uniqueCourses;
    for (auto& e : idx.entries) {
        FinalizeGroupEntryMetadata(e);
        if (!e.courseCode.empty()) uniqueCourses.insert(SafeLower(e.courseCode));
        if (!e.campusShort.empty()) campuses.insert(e.campusShort);
        if (!e.schoolName.empty()) schools.insert(e.schoolName);
        if (!e.careerName.empty()) careers.insert(e.careerName);
        if (!e.periodCode.empty()) periods.insert(e.periodCode);
        if (!e.modality.empty()) modalities.insert(e.modality);
    }
    idx.uniqueCoursesCount = uniqueCourses.size();
    idx.campuses.assign(campuses.begin(), campuses.end());
    idx.schools.assign(schools.begin(), schools.end());
    idx.careers.assign(careers.begin(), careers.end());
    idx.periods.assign(periods.begin(), periods.end());
    idx.modalities.assign(modalities.begin(), modalities.end());

    std::sort(idx.entries.begin(), idx.entries.end(),
              [](const GlobalHorariosGroupEntry& a, const GlobalHorariosGroupEntry& b) {
                  if (a.courseCode != b.courseCode) return a.courseCode < b.courseCode;
                  if (a.periodCode != b.periodCode) return a.periodCode < b.periodCode;
                  if (a.campusShort != b.campusShort) return a.campusShort < b.campusShort;
                  return a.groupId < b.groupId;
              });
}

}  // namespace

GlobalHorariosIndex TecAcademicStore::BuildGlobalScheduleIndexFromRows(
    const std::vector<td::PublicGuiaSessionRow>& rows,
    int year,
    int totalSchoolsFetched,
    int schoolsWithCourses) {
    GlobalHorariosIndex idx;
    idx.year = year;
    idx.indexedAtUtc = td::CredentialVault::CurrentUtcIso8601();
    idx.totalSchoolsFetched = totalSchoolsFetched;
    idx.schoolsWithCourses = schoolsWithCourses;

    std::map<std::string, size_t> groupSlotByKey;
    for (const auto& r : rows) {
        std::string code = TrimStore(r.ide_materia);
        if (code.empty()) continue;
        for (char& ch : code) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        std::string grp = TrimStore(r.ide_grupo);
        if (grp.empty()) grp = "1";
        std::string campusFull = TrimStore(r.dsc_sede);
        std::string campusShort = NormalizeCampusShortStore(campusFull);
        std::string modCode = TrimStore(r.ide_modalidad);
        for (char& ch : modCode) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        std::string perNum = TrimStore(r.ide_per_mod);
        std::string periodCode = modCode.empty() ? "S-1" : (modCode + (perNum.empty() ? "" : ("-" + perNum)));
        int rowYear = (r.num_ano > 1990) ? r.num_ano : year;

        std::string key = SafeLower(code) + "|" + SafeLower(grp) + "|" +
                          SafeLower(campusShort) + "|" + SafeLower(periodCode) + "|" +
                          std::to_string(rowYear);

        size_t pos = 0;
        auto it = groupSlotByKey.find(key);
        if (it == groupSlotByKey.end()) {
            pos = idx.entries.size();
            groupSlotByKey[key] = pos;
            GlobalHorariosGroupEntry ge;
            ge.courseCode = code;
            ge.courseName = TrimStore(r.dsc_materia);
            ge.credits = r.can_creditos;
            ge.hours = r.horas;
            ge.groupId = grp;
            ge.schoolName = TrimStore(r.dsc_depto);
            if (!r.dsc_plan.empty()) ge.careerName = TrimStore(r.dsc_plan);
            ge.campusFull = campusFull;
            ge.campusShort = campusShort;
            ge.periodCode = periodCode;
            ge.periodLabel = BuildPeriodLabelStore(modCode, perNum, TrimStore(r.dsc_modalidad));
            ge.year = rowYear;
            ge.modality = !TrimStore(r.tipo_curso).empty() ? TrimStore(r.tipo_curso) : "Regular";
            ge.professor = TrimStore(r.nom_profesor);
            std::string resNote;
            if (!TrimStore(r.reserva_sede).empty()) resNote += "Sede " + TrimStore(r.reserva_sede);
            if (!TrimStore(r.reserva_depto).empty()) {
                if (!resNote.empty()) resNote += " · ";
                resNote += "Depto " + TrimStore(r.reserva_depto);
            }
            if (!TrimStore(r.reserva_plan).empty()) {
                if (!resNote.empty()) resNote += " · ";
                resNote += "Plan " + TrimStore(r.reserva_plan);
            }
            ge.reservedNote = std::move(resNote);
            idx.entries.push_back(std::move(ge));
        } else {
            pos = it->second;
            auto& ge = idx.entries[pos];
            if (ge.professor.empty() && !TrimStore(r.nom_profesor).empty()) ge.professor = TrimStore(r.nom_profesor);
            if (ge.schoolName.empty() && !TrimStore(r.dsc_depto).empty()) ge.schoolName = TrimStore(r.dsc_depto);
            if (ge.credits <= 0 && r.can_creditos > 0) ge.credits = r.can_creditos;
        }

        const int dayIdx = ParseDayIndexStore(r.nom_dia);
        const int sm = ParseHhMmMinutesStore(r.hinicio);
        const int em = ParseHhMmMinutesStore(r.hfin);
        if (dayIdx >= 0 && dayIdx <= 6 && sm >= 0 && em > sm) {
            auto& sess = idx.entries[pos].sessions;
            bool dup = false;
            for (const auto& existing : sess) {
                if (static_cast<int>(existing.day) == dayIdx &&
                    existing.startMinute == sm && existing.endMinute == em) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                ht::TimeSlot ts;
                ts.day = static_cast<ht::DayOfWeek>(dayIdx);
                ts.startMinute = sm;
                ts.endMinute = em;
                ts.campusUtf8 = campusShort;
                sess.push_back(std::move(ts));
            }
        }
    }

    RebuildGlobalIndexFacets(idx);
    return idx;
}

bool TecAcademicStore::BuildGlobalScheduleIndex(
    int year,
    ProgressCallback onProgress,
    std::atomic<bool>* cancelFlag,
    std::string* outErr,
    int maxConcurrency) {
    if (year < 1990 || year > 2100) year = 2026;
    if (onProgress) onProgress(5, "Obteniendo lista oficial de escuelas del TEC...");

    std::vector<td::PublicGuiaEscuelaInfo> escuelas;
    td::PublicGuiaAllFetchStats stats;
    auto rowsRes = service_->PublicGuiaClient().FetchAllEscuelasByYear(
        year,
        [&](int done, int total, const std::string& curSchool) {
            if (!onProgress) return;
            int pct = 8 + (total > 0 ? (done * 84) / total : 84);
            onProgress(std::clamp(pct, 8, 92),
                       "Indexando " + std::to_string(done) + "/" + std::to_string(total) +
                       " escuelas: " + curSchool);
        },
        cancelFlag,
        maxConcurrency,
        &escuelas,
        &stats,
        true);

    if (!rowsRes.ok) {
        if (outErr) *outErr = rowsRes.error_message;
        return false;
    }

    if (onProgress) onProgress(94, "Construyendo índice global de búsqueda...");
    GlobalHorariosIndex built = BuildGlobalScheduleIndexFromRows(
        rowsRes.value, year, stats.total_schools, stats.schools_with_rows);

    {
        std::lock_guard<std::mutex> lk(globalHorariosMutex_);
        globalHorariosIndex_ = built;
    }
    {
        std::lock_guard<std::mutex> lk(snapMutex_);
        if (!escuelas.empty()) {
            snapshot_.publicGuiaEscuelas = std::move(escuelas);
        }
        if (snapshot_.currentSchedule.empty() && !snapshot_.communities.empty()) {
            auto derived = DeriveScheduleCellsFromEnrolledAndGlobal(snapshot_.communities, built);
            if (!derived.empty()) {
                snapshot_.currentSchedule = std::move(derived);
            }
        }
    }

    SaveGlobalScheduleIndexToDisk(built, nullptr);
    if (onProgress) {
        onProgress(100, "Índice global listo: " + std::to_string(built.uniqueCoursesCount) +
                            " cursos en " + std::to_string(built.campuses.size()) + " sedes.");
    }
    return true;
}

bool TecAcademicStore::MergeExpedienteCarreraIntoGlobalIndex(
    const std::string& sedeKey,
    const std::string& carreraKey,
    const std::string& periodoKey,
    const std::string& campusLabel,
    const std::string& carreraName,
    std::string* outErr) {
    const std::string expPeriod = td::ConvertPeriodKeyToExpedienteFormat(periodoKey);
    auto rowsRes = service_->TdaClient().GetGuiaHorarioRows(sedeKey, carreraKey, expPeriod);
    if (!rowsRes.ok) {
        if (outErr) *outErr = rowsRes.error_message;
        return false;
    }
    if (rowsRes.value.empty()) {
        if (outErr) *outErr = "No se encontraron grupos en el Expediente para la carrera seleccionada.";
        return false;
    }

    const std::string catJson = td::GuiaHorarioRowsToHorariosTecJson(
        rowsRes.value, carreraName.empty() ? "Expediente TEC" : carreraName, periodoKey, campusLabel);
    auto parsed = ht::ParseCatalogJson(catJson);
    if (!parsed.ok) {
        if (outErr) *outErr = parsed.errorUtf8;
        return false;
    }

    const std::string campShort = NormalizeCampusShortStore(campusLabel);
    std::string perShort = "S-2";
    int yr = 2026;
    {
        std::string pl = SafeLower(periodoKey);
        if (pl.find("s-1") != std::string::npos || pl.find("_s_1") != std::string::npos) perShort = "S-1";
        else if (pl.find("s-2") != std::string::npos || pl.find("_s_2") != std::string::npos) perShort = "S-2";
        else if (pl.find("v-1") != std::string::npos || pl.find("_v_") != std::string::npos) perShort = "V-1";
        for (int candidate : {2025, 2026, 2027, 2028}) {
            if (pl.find(std::to_string(candidate)) != std::string::npos) { yr = candidate; break; }
        }
    }

    std::lock_guard<std::mutex> lk(globalHorariosMutex_);
    if (globalHorariosIndex_.year <= 0) globalHorariosIndex_.year = yr;

    auto normalizeGroupId = [](std::string raw) {
        std::string s = TrimStore(raw);
        if (s.empty()) return s;
        bool allDigits = true;
        for (char ch : s) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) { allDigits = false; break; }
        }
        if (allDigits) {
            size_t nz = s.find_first_not_of('0');
            if (nz == std::string::npos) return std::string("0");
            return s.substr(nz);
        }
        return s;
    };

    for (const auto& c : parsed.catalog.courses) {
        std::string codeUp = TrimStore(c.codeUtf8);
        for (char& ch : codeUp) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        for (const auto& g : c.groups) {
            std::string gid = normalizeGroupId(g.idUtf8);
            std::string groupRoom;
            for (const auto& s : g.sessions) {
                if (!s.roomUtf8.empty()) { groupRoom = s.roomUtf8; break; }
            }
            bool merged = false;
            for (auto& existing : globalHorariosIndex_.entries) {
                if (SafeLower(existing.courseCode) == SafeLower(codeUp) &&
                    SafeLower(normalizeGroupId(existing.groupId)) == SafeLower(gid) &&
                    SafeLower(existing.campusShort) == SafeLower(campShort) &&
                    SafeLower(existing.periodCode) == SafeLower(perShort)) {
                    if (g.seatsAvailable >= 0) existing.seatsAvailable = g.seatsAvailable;
                    if (!carreraName.empty()) existing.careerName = carreraName;
                    if (existing.professor.empty() && !g.professorUtf8.empty()) existing.professor = g.professorUtf8;
                    if (!groupRoom.empty()) {
                        existing.room = groupRoom;
                        for (auto& es : existing.sessions) {
                            if (es.roomUtf8.empty()) es.roomUtf8 = groupRoom;
                        }
                    }
                    merged = true;
                    break;
                }
            }
            if (!merged) {
                GlobalHorariosGroupEntry ge;
                ge.courseCode = codeUp;
                ge.courseName = c.nameUtf8;
                ge.credits = c.credits;
                ge.hours = c.hours;
                ge.groupId = gid;
                ge.schoolName = carreraName;
                ge.careerName = carreraName;
                ge.campusFull = campusLabel;
                ge.campusShort = campShort;
                ge.periodCode = perShort;
                ge.periodLabel = (perShort == "S-1") ? "Semestre 1" : ((perShort == "V-1") ? "Verano" : "Semestre 2");
                ge.year = yr;
                ge.modality = g.modalityUtf8.empty() ? "Regular" : g.modalityUtf8;
                ge.professor = g.professorUtf8;
                ge.room = groupRoom;
                ge.seatsAvailable = g.seatsAvailable;
                ge.sessions = g.sessions;
                globalHorariosIndex_.entries.push_back(std::move(ge));
            }
        }
    }
    RebuildGlobalIndexFacets(globalHorariosIndex_);
    SaveGlobalScheduleIndexToDisk(globalHorariosIndex_, nullptr);
    return true;
}

bool TecAcademicStore::SaveGlobalScheduleIndexToDisk(const GlobalHorariosIndex& idx, std::string* outErr) const {
    try {
        json j;
        j["version"] = 1;
        j["year"] = idx.year;
        j["indexed_at_utc"] = idx.indexedAtUtc;
        j["total_schools_fetched"] = idx.totalSchoolsFetched;
        j["schools_with_courses"] = idx.schoolsWithCourses;
        json arr = json::array();
        for (const auto& e : idx.entries) {
            json sess = json::array();
            for (const auto& s : e.sessions) {
                sess.push_back({
                    {"d", static_cast<int>(s.day)},
                    {"s", s.startMinute},
                    {"e", s.endMinute},
                    {"r", s.roomUtf8}
                });
            }
            arr.push_back({
                {"cc", e.courseCode},
                {"cn", e.courseName},
                {"cr", e.credits},
                {"hr", e.hours},
                {"g", e.groupId},
                {"sc", e.schoolCode},
                {"sn", e.schoolName},
                {"car", e.careerName},
                {"cf", e.campusFull},
                {"cs", e.campusShort},
                {"pc", e.periodCode},
                {"pl", e.periodLabel},
                {"y", e.year},
                {"m", e.modality},
                {"p", e.professor},
                {"rm", e.room},
                {"st", e.seatsAvailable},
                {"rn", e.reservedNote},
                {"ss", std::move(sess)}
            });
        }
        j["entries"] = std::move(arr);

        fs::path target = fs::path(Utf8ToWide(storageRootUtf8_)) / L"cache" /
                          Utf8ToWide("horarios_global_" + std::to_string(idx.year) + ".json");
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream ofs(target, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (outErr) *outErr = "No se pudo guardar la caché global de horarios.";
            return false;
        }
        SanitizeJsonUtf8InPlace(j);
        const std::string dump = j.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        ofs.write(dump.data(), static_cast<std::streamsize>(dump.size()));
        return ofs.good();
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

bool TecAcademicStore::LoadGlobalScheduleIndexFromDisk(int year, std::string* outErr) {
    fs::path target = fs::path(Utf8ToWide(storageRootUtf8_)) / L"cache" /
                      Utf8ToWide("horarios_global_" + std::to_string(year) + ".json");
    std::error_code ec;
    if (!fs::exists(target, ec)) return false;
    try {
        std::ifstream ifs(target, std::ios::binary);
        if (!ifs.is_open()) return false;
        std::ostringstream oss;
        oss << ifs.rdbuf();
        auto j = json::parse(oss.str());
        if (!j.is_object() || !j.contains("entries") || !j["entries"].is_array()) return false;

        GlobalHorariosIndex loaded;
        loaded.year = j.value("year", year);
        loaded.indexedAtUtc = j.value("indexed_at_utc", "");
        loaded.totalSchoolsFetched = j.value("total_schools_fetched", 0);
        loaded.schoolsWithCourses = j.value("schools_with_courses", 0);
        loaded.entries.reserve(j["entries"].size());

        for (const auto& ej : j["entries"]) {
            if (!ej.is_object()) continue;
            GlobalHorariosGroupEntry e;
            e.courseCode = ej.value("cc", "");
            e.courseName = ej.value("cn", "");
            e.credits = ej.value("cr", 0);
            e.hours = ej.value("hr", 0);
            e.groupId = ej.value("g", "");
            e.schoolCode = ej.value("sc", "");
            e.schoolName = ej.value("sn", "");
            e.careerName = ej.value("car", "");
            e.campusFull = ej.value("cf", "");
            e.campusShort = ej.value("cs", "");
            e.periodCode = ej.value("pc", "");
            e.periodLabel = ej.value("pl", "");
            e.year = ej.value("y", loaded.year);
            e.modality = ej.value("m", "Regular");
            e.professor = ej.value("p", "");
            e.room = ej.value("rm", "");
            e.seatsAvailable = ej.value("st", -1);
            e.reservedNote = ej.value("rn", "");
            if (ej.contains("ss") && ej["ss"].is_array()) {
                for (const auto& sj : ej["ss"]) {
                    ht::TimeSlot ts;
                    ts.day = static_cast<ht::DayOfWeek>(std::clamp(sj.value("d", 0), 0, 6));
                    ts.startMinute = sj.value("s", 0);
                    ts.endMinute = sj.value("e", 0);
                    ts.roomUtf8 = sj.value("r", "");
                    ts.campusUtf8 = e.campusShort;
                    if (ts.IsValid()) e.sessions.push_back(std::move(ts));
                }
            }
            loaded.entries.push_back(std::move(e));
        }

        RebuildGlobalIndexFacets(loaded);
        {
            std::lock_guard<std::mutex> lk(globalHorariosMutex_);
            globalHorariosIndex_ = loaded;
        }
        {
            std::lock_guard<std::mutex> lk(snapMutex_);
            if (snapshot_.currentSchedule.empty() && !snapshot_.communities.empty()) {
                auto derived = DeriveScheduleCellsFromEnrolledAndGlobal(snapshot_.communities, loaded);
                if (!derived.empty()) snapshot_.currentSchedule = std::move(derived);
            }
        }
        return true;
    } catch (const std::exception& ex) {
        if (outErr) *outErr = ex.what();
        return false;
    }
}

GlobalHorariosIndex TecAcademicStore::GetGlobalScheduleIndex() const {
    std::lock_guard<std::mutex> lk(globalHorariosMutex_);
    return globalHorariosIndex_;
}

void TecAcademicStore::SetGlobalScheduleIndexForTesting(GlobalHorariosIndex idx) {
    RebuildGlobalIndexFacets(idx);
    std::lock_guard<std::mutex> lk(globalHorariosMutex_);
    globalHorariosIndex_ = std::move(idx);
}

std::vector<GlobalHorariosGroupEntry> TecAcademicStore::SearchGlobalScheduleIndex(
    const GlobalHorariosSearchFilter& filter,
    std::size_t* outTotalMatches) const {
    GlobalHorariosIndex idxCopy;
    {
        std::lock_guard<std::mutex> lk(globalHorariosMutex_);
        idxCopy = globalHorariosIndex_;
    }

    // Optional curriculum eligibility set
    std::optional<std::unordered_set<std::string>> eligibleSet;
    if (filter.onlyEligibleByCurriculum) {
        std::optional<td::CurriculumGraph> gCopy;
        std::optional<td::StudentCurriculumProfile> pCopy;
        {
            std::lock_guard<std::mutex> lk(snapMutex_);
            gCopy = snapshot_.curriculumGraph;
            pCopy = snapshot_.studentProfile;
        }
        if (gCopy && pCopy) {
            std::unordered_set<std::string> okCodes;
            for (const auto& lv : gCopy->levels) {
                for (const auto& node : lv.courses) {
                    if (IsCoursePassedInProfile(*pCopy, node.id_course)) continue;
                    bool met = true;
                    for (const auto& req : node.requirements) {
                        if (!IsCoursePassedInProfile(*pCopy, req)) { met = false; break; }
                    }
                    if (met) okCodes.insert(SafeLower(node.id_course));
                }
            }
            eligibleSet = std::move(okCodes);
        }
    }

    // Tokenize search query (space-separated AND tokens, accent-folded)
    std::vector<std::string> tokens;
    {
        std::istringstream iss(FoldSpanishLowerStore(filter.queryUtf8));
        std::string tok;
        while (iss >> tok) {
            if (!tok.empty()) tokens.push_back(std::move(tok));
        }
    }
    const std::string campF = FoldSpanishLowerStore(TrimStore(filter.campusFilter));
    const std::string schF = FoldSpanishLowerStore(TrimStore(filter.schoolOrCareerFilter));
    const std::string perF = FoldSpanishLowerStore(TrimStore(filter.periodFilter));
    const std::string modF = FoldSpanishLowerStore(TrimStore(filter.modalityFilter));
    const std::string exactCodeQ = (tokens.size() == 1) ? tokens[0] : "";

    std::vector<std::pair<int, GlobalHorariosGroupEntry>> scored;
    std::size_t totalMatches = 0;
    for (const auto& e : idxCopy.entries) {
        if (!campF.empty() && FoldSpanishLowerStore(e.campusShort).find(campF) == std::string::npos &&
            FoldSpanishLowerStore(e.campusFull).find(campF) == std::string::npos) {
            continue;
        }
        if (!schF.empty()) {
            const std::string sn = FoldSpanishLowerStore(e.schoolName);
            const std::string sc = FoldSpanishLowerStore(e.schoolCode);
            const std::string cn = FoldSpanishLowerStore(e.careerName);
            if (sn.find(schF) == std::string::npos && sc != schF && cn.find(schF) == std::string::npos) {
                continue;
            }
        }
        if (!perF.empty()) {
            const std::string pc = FoldSpanishLowerStore(e.periodCode);
            const std::string pl = FoldSpanishLowerStore(e.periodLabel);
            if (pc.rfind(perF, 0) != 0 && pl.find(perF) == std::string::npos) {
                continue;
            }
        }
        if (!modF.empty() && FoldSpanishLowerStore(e.modality).find(modF) == std::string::npos) {
            continue;
        }
        if (filter.dayFilter >= 0 && filter.dayFilter <= 6) {
            if ((e.daysMask & (1u << static_cast<unsigned>(filter.dayFilter))) == 0) {
                continue;
            }
        }
        if (filter.excludeNight && e.hasNightSession) continue;
        if (filter.onlyWithSeats && e.seatsAvailable == 0) continue;
        if (eligibleSet && eligibleSet->count(SafeLower(e.courseCode)) == 0) continue;

        bool matchAll = true;
        for (const auto& tok : tokens) {
            if (e.searchFolded.find(tok) == std::string::npos) {
                matchAll = false;
                break;
            }
        }
        if (!matchAll) continue;

        ++totalMatches;
        int rank = 3;
        if (!exactCodeQ.empty()) {
            std::string cl = SafeLower(e.courseCode);
            if (cl == exactCodeQ) rank = 0;
            else if (cl.rfind(exactCodeQ, 0) == 0) rank = 1;
            else if (FoldSpanishLowerStore(e.courseName).find(exactCodeQ) != std::string::npos) rank = 2;
        }
        scored.emplace_back(rank, e);
    }

    if (outTotalMatches) *outTotalMatches = totalMatches;

    std::stable_sort(scored.begin(), scored.end(),
                     [](const auto& a, const auto& b) { return a.first < b.first; });

    std::vector<GlobalHorariosGroupEntry> out;
    const size_t limit = (filter.maxResults > 0) ? std::min(scored.size(), filter.maxResults) : scored.size();
    out.reserve(limit);
    for (size_t i = 0; i < limit; ++i) {
        out.push_back(std::move(scored[i].second));
    }
    return out;
}

LiveScheduleSolveResult TecAcademicStore::SolveSelectedCoursesFromGlobalIndex(
    const std::vector<std::string>& selectedCourseCodes,
    const GlobalHorariosSearchFilter& baseFilter,
    const ht::SolverPreferences& prefs,
    bool filterEligibleByCurriculum,
    bool includeUnifiedCalendarBusySlots,
    const std::vector<ht::BusySlot>& extraBusySlots) const {
    LiveScheduleSolveResult out;
    if (selectedCourseCodes.empty()) {
        out.errorUtf8 = "Agrega al menos un curso desde el buscador para armar el horario.";
        return out;
    }

    std::unordered_set<std::string> wantedCodes;
    for (const auto& c : selectedCourseCodes) {
        std::string t = SafeLower(TrimStore(c));
        if (!t.empty()) wantedCodes.insert(std::move(t));
    }
    if (wantedCodes.empty()) {
        out.errorUtf8 = "Selecciona cursos válidos para armar combinaciones.";
        return out;
    }

    GlobalHorariosSearchFilter f = baseFilter;
    f.queryUtf8.clear();
    f.schoolOrCareerFilter.clear(); // Selected courses may span ANY combination of schools/careers!
    f.maxResults = 0;               // Inspect all matching entries
    auto matched = SearchGlobalScheduleIndex(f, nullptr);

    ht::CourseCatalog cat;
    cat.titleUtf8 = "Buscador Global de Horarios TEC";
    cat.campusUtf8 = baseFilter.campusFilter;
    cat.termUtf8 = baseFilter.periodFilter;

    std::map<std::string, size_t> courseIdxByCode;
    for (const auto& e : matched) {
        std::string cl = SafeLower(e.courseCode);
        if (wantedCodes.count(cl) == 0) continue;
        if (e.sessions.empty()) continue;

        size_t cIdx = 0;
        auto it = courseIdxByCode.find(cl);
        if (it == courseIdxByCode.end()) {
            cIdx = cat.courses.size();
            courseIdxByCode[cl] = cIdx;
            ht::Course hc;
            hc.codeUtf8 = e.courseCode;
            hc.nameUtf8 = e.courseName;
            hc.credits = e.credits;
            hc.hours = e.hours;
            cat.courses.push_back(std::move(hc));
        } else {
            cIdx = it->second;
        }

        ht::Group g;
        g.idUtf8 = e.groupId;
        if (baseFilter.campusFilter.empty() && !e.campusShort.empty()) {
            g.idUtf8 += " (" + e.campusShort + ")";
        }
        if (baseFilter.periodFilter.empty() && !e.periodCode.empty()) {
            g.idUtf8 += " · " + e.periodCode;
        }
        g.modalityUtf8 = e.modality;
        g.professorUtf8 = e.professor;
        g.campusUtf8 = e.campusShort;
        g.seatsAvailable = e.seatsAvailable;
        g.sessions = e.sessions;
        cat.courses[cIdx].groups.push_back(std::move(g));
    }

    if (cat.courses.empty()) {
        out.errorUtf8 = "Ninguno de los cursos seleccionados tiene sesiones con los filtros actuales de sede/periodo/modalidad.";
        return out;
    }

    out.catalog = std::move(cat);

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

    std::vector<ht::BusySlot> busySlots = extraBusySlots;
    if (includeUnifiedCalendarBusySlots) {
        auto enrolled = CollectEnrolledBusySlots(schedCopy);
        busySlots.insert(busySlots.end(), enrolled.begin(), enrolled.end());
    }

    out.solved = ht::HorariosSolver::Solve(out.catalog, busySlots, prefs);
    out.ok = true;
    return out;
}

bool TecAcademicStore::SaveSnapshotToDisk(std::string* outErr) const {
    TecAcademicSnapshot snap = Snapshot();
    try {
        json j;
        j["version"] = 2;
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
                {"root_folder_id", c.root_folder_id},
                {"parent_url", c.parent_url},
                {"is_current_period", c.is_current_period}
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
                        {"category_name", it.category_name},
                        {"name", it.name},
                        {"description", it.description},
                        {"teacher_comment", it.teacher_comment},
                        {"attachment_url", it.attachment_url},
                        {"solution_url", it.solution_url},
                        {"feedback_url", it.feedback_url},
                        {"weight_pct", it.weight_pct},
                        {"earned_pct", it.earned_pct},
                        {"raw_grade_100", it.raw_grade_100},
                        {"is_graded", it.is_graded},
                        {"due_date_iso", it.due_date_iso},
                        {"due_date_text", it.due_date_text},
                        {"due_date_gmt", it.due_date_gmt},
                        {"allow_late_submission", it.allow_late_submission},
                        {"group_size", it.group_size},
                        {"group_members", it.group_members},
                        {"has_rubric", it.has_rubric},
                        {"rubric_id", it.rubric_id},
                        {"task_id", it.task_id},
                        {"task_item_id", it.task_item_id},
                        {"community_id", it.community_id},
                        {"grade_id", it.grade_id},
                        // student_id intentionally omitted from offline JSON cache (zero-PII contract)
                        {"with_group", it.with_group},
                        {"can_upload", it.can_upload},
                        {"can_upload_url", it.can_upload_url},
                        {"max_file_size_mb", it.max_file_size_mb},
                        {"submitted_files", it.submitted_files},
                        {"latest_revision_id", it.latest_revision_id},
                        {"latest_item_id", it.latest_item_id},
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

        auto serializeCalArr = [](const std::vector<td::CalendarEventEntry>& vec) {
            json arr = json::array();
            for (const auto& ev : vec) {
                arr.push_back({
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
            return arr;
        };
        j["unified_calendar"] = serializeCalArr(snap.unifiedCalendarEvents);
        if (!snap.portalCalendarEvents.empty()) {
            j["portal_calendar"] = serializeCalArr(snap.portalCalendarEvents);
        }
        if (!snap.courseCalendarsByPath.empty()) {
            json cc = json::object();
            for (const auto& [path, vec] : snap.courseCalendarsByPath) {
                cc[path] = serializeCalArr(vec);
            }
            j["course_calendars_by_path"] = std::move(cc);
        }

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
                {"credits_no_grade", st.credits_no_grade},
                {"subjects_consulted", st.subjects_consulted},
                {"subjects_taken", st.subjects_taken},
                {"aprobadas", st.count_aprobadas},
                {"en_curso", st.count_en_curso},
                {"reprobadas", st.count_reprobadas},
                {"retiradas", st.count_retiradas},
                {"reconocidas", st.count_reconocidas},
                {"suficiencia", st.count_suficiencia},
                {"pendientes", st.count_pendientes},
                {"congeladas", st.count_congeladas},
                {"active_schedule_period", snap.expediente->active_schedule_period},
                {"weighted_average_text", snap.expediente->weighted_average_text},
                {"approval_pct_text", snap.expediente->approval_pct_text}
            };
            json eg = json::array();
            for (const auto& r : snap.expediente->structured.grades) {
                eg.push_back({
                    {"year", r.year}, {"state", r.state}, {"grade", r.grade},
                    {"grade_text", r.grade_text}, {"course_code", r.course_code},
                    {"course_name", r.course_name}, {"modality", r.modality},
                    {"period", r.period}, {"credits", r.credits}
                });
            }
            j["expediente_grades"] = std::move(eg);
            json wa = json::array();
            for (const auto& w : snap.expediente->structured.weighted_averages) {
                wa.push_back({
                    {"year", w.year}, {"modality", w.modality},
                    {"period", w.period}, {"weighted_avg", w.weighted_avg}
                });
            }
            j["expediente_weighted_averages"] = std::move(wa);
            json ec = json::array();
            for (const auto& c : snap.expediente->structured.citas_matricula) {
                ec.push_back({{"date", c.date}, {"period", c.period}, {"modality", c.modality}, {"time", c.time}});
            }
            j["expediente_citas"] = std::move(ec);
            json es = json::array();
            for (const auto& s : snap.expediente->sedes_combo) es.push_back({{"key", s.key}, {"name", s.name}});
            j["expediente_sedes"] = std::move(es);
            json ep = json::array();
            for (const auto& p : snap.expediente->periodos_combo) ep.push_back({{"key", p.key}, {"name", p.name}});
            j["expediente_periodos"] = std::move(ep);
        }

        if (!snap.curriculumSedes.empty()) {
            json arr = json::array();
            for (const auto& s : snap.curriculumSedes) arr.push_back({{"key", s.key}, {"name", s.name}});
            j["curriculum_sedes"] = std::move(arr);
        }
        if (!snap.curriculumCarreras.empty()) {
            json arr = json::array();
            for (const auto& c : snap.curriculumCarreras) arr.push_back({{"key", c.key}, {"name", c.name}});
            j["curriculum_carreras"] = std::move(arr);
        }
        if (!snap.curriculumPlanes.empty()) {
            json arr = json::array();
            for (const auto& p : snap.curriculumPlanes) arr.push_back({{"key", p.key}, {"name", p.name}});
            j["curriculum_planes"] = std::move(arr);
        }
        if (!snap.publicGuiaEscuelas.empty()) {
            json arr = json::array();
            for (const auto& e : snap.publicGuiaEscuelas) arr.push_back({{"ide_depto", e.ide_depto}, {"dsc_depto", e.dsc_depto}});
            j["public_guia_escuelas"] = std::move(arr);
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
                    {"building", cell.building},
                    {"professor", cell.professor},
                    {"credits", cell.credits},
                    {"raw_text", cell.raw_text}
                });
            }
            j["current_schedule"] = std::move(cs);
        }

        auto serializeFolderWalk = [](const td::FolderWalkResult& fwr) -> json {
            json dirs = json::array();
            for (const auto& d : fwr.folders) {
                dirs.push_back({
                    {"folder_id", d.folder_id},
                    {"parent_id", d.parent_id},
                    {"name", d.name},
                    {"folder_path", d.folder_path},
                    {"creation_date_ansi", d.creation_date_ansi},
                    {"last_modified_ansi", d.last_modified_ansi},
                    {"author_name", d.author_name},
                    {"subscription_active", d.subscription_active},
                    {"depth", d.depth},
                    {"child_count", d.child_count}
                });
            }
            json fls = json::array();
            for (const auto& f : fwr.files) {
                fls.push_back({
                    {"object_id", f.object_id},
                    {"parent_folder_id", f.parent_folder_id},
                    {"name", f.name},
                    {"title", f.title},
                    {"folder_path", f.folder_path},
                    {"mime_type", f.mime_type},
                    {"content_size", f.content_size},
                    {"live_revision", f.live_revision},
                    {"creation_date_ansi", f.creation_date_ansi},
                    {"last_modified_ansi", f.last_modified_ansi},
                    {"author_name", f.author_name},
                    {"subscription_active", f.subscription_active},
                    {"download_url", f.download_url},
                    {"status", static_cast<int>(f.status)},
                    {"logical_rel_path", f.logical_rel_path},
                    {"blob_rel_path", f.blob_rel_path},
                    {"plagiarism_percentage", f.plagiarism_percentage},
                    {"plagiarism_status", f.plagiarism_status},
                    {"plagiarism_doc_uid", f.plagiarism_doc_uid}
                });
            }
            json urls = json::array();
            for (const auto& u : fwr.url_links) {
                urls.push_back({
                    {"object_id", u.object_id},
                    {"parent_folder_id", u.parent_folder_id},
                    {"title", u.title},
                    {"url", u.url},
                    {"folder_path", u.folder_path}
                });
            }
            return {
                {"root_folder_id", fwr.root_folder_id},
                {"folders", std::move(dirs)},
                {"files", std::move(fls)},
                {"url_links", std::move(urls)}
            };
        };

        auto serializePortlet = [](const td::SpacePortletBlock& p) -> json {
            json pl = json::array();
            for (const auto& l : p.links) pl.push_back({{"label", l.label}, {"url", l.url}, {"category", l.category}});
            json imgs = json::array();
            for (const auto& im : p.images) imgs.push_back({{"image_url", im.image_url}, {"alt_text", im.alt_text}, {"link_url", im.link_url}});
            json subs = json::array();
            for (const auto& sb : p.linked_subpages) subs.push_back({{"page_num", sb.page_num}, {"label", sb.label}, {"url", sb.url}, {"is_custom", sb.is_custom}});
            return {
                {"title", p.title},
                {"body_preview", p.body_preview},
                {"module_kind", p.module_kind},
                {"links", std::move(pl)},
                {"images", std::move(imgs)},
                {"linked_subpages", std::move(subs)}
            };
        };

        if (!snap.foldersByPath.empty()) {
            json fbp = json::object();
            for (const auto& [path, fwr] : snap.foldersByPath) {
                fbp[path] = serializeFolderWalk(fwr);
            }
            j["folders_by_path"] = std::move(fbp);
        }

        if (snap.personalDocuments) {
            j["personal_documents_folder_id"] = snap.personalDocumentsFolderId;
            j["personal_documents"] = serializeFolderWalk(*snap.personalDocuments);
        }

        if (!snap.overviewsByPath.empty()) {
            json ovs = json::object();
            for (const auto& [path, ov] : snap.overviewsByPath) {
                json tabs = json::array();
                for (const auto& t : ov.tabs) {
                    tabs.push_back({{"page_num", t.page_num}, {"label", t.label}, {"url", t.url}, {"is_custom", t.is_custom}});
                }
                json bns = json::array();
                for (const auto& b : ov.banners) {
                    bns.push_back({{"image_url", b.image_url}, {"alt_text", b.alt_text}, {"link_url", b.link_url}});
                }
                json pts = json::array();
                for (const auto& p : ov.portlets) pts.push_back(serializePortlet(p));
                json od = json::array();
                for (const auto& d : ov.official_docs) od.push_back({{"label", d.label}, {"url", d.url}, {"category", d.category}});
                ovs[path] = {
                    {"title", ov.title},
                    {"description", ov.description},
                    {"teacher_name", ov.teacher.name},
                    {"teacher_office", ov.teacher.office},
                    {"teacher_teams", ov.teacher.teams_handle},
                    {"consultation_hours", ov.teacher.consultation_hours},
                    {"weekly_sections", ov.weekly_sections},
                    {"tabs", std::move(tabs)},
                    {"banners", std::move(bns)},
                    {"portlets", std::move(pts)},
                    {"official_docs", std::move(od)}
                };
            }
            j["overviews_by_path"] = std::move(ovs);
        }

        if (!snap.customPagesBySpace.empty()) {
            json cps = json::object();
            for (const auto& [path, pages] : snap.customPagesBySpace) {
                json parr = json::array();
                for (const auto& pg : pages) {
                    json bns = json::array();
                    for (const auto& b : pg.banners) bns.push_back({{"image_url", b.image_url}, {"alt_text", b.alt_text}, {"link_url", b.link_url}});
                    json pts = json::array();
                    for (const auto& p : pg.portlets) pts.push_back(serializePortlet(p));
                    json lnks = json::array();
                    for (const auto& l : pg.links) lnks.push_back({{"label", l.label}, {"url", l.url}, {"category", l.category}});
                    parr.push_back({
                        {"page_num", pg.page_num},
                        {"tab_label", pg.tab_label},
                        {"page_url", pg.page_url},
                        {"plain_excerpt", pg.plain_excerpt},
                        {"banners", std::move(bns)},
                        {"portlets", std::move(pts)},
                        {"links", std::move(lnks)}
                    });
                }
                cps[path] = std::move(parr);
            }
            j["custom_pages_by_space"] = std::move(cps);
        }

        j["monitor_interval_minutes"] = snap.monitorIntervalMinutes;
        if (!snap.gaapServerUrl.empty()) j["gaap_server_url"] = snap.gaapServerUrl;
        if (!snap.activityFeed.empty()) {
            json af = json::array();
            for (const auto& ev : snap.activityFeed) {
                af.push_back({
                    {"id", ev.id},
                    {"kind", static_cast<int>(ev.kind)},
                    {"detected_at_utc", ev.detected_at_utc},
                    {"space_url", ev.space_url},
                    {"space_name", ev.space_name},
                    {"title", ev.title},
                    {"detail", ev.detail},
                    {"target_id", ev.target_id},
                    {"unread", ev.unread}
                });
            }
            j["activity_feed"] = std::move(af);
        }
        if (snap.motorJuegosUser) {
            j["motor_juegos_user"] = {
                {"user_id", snap.motorJuegosUser->user_id},
                {"is_professor", snap.motorJuegosUser->is_professor}
            };
        }
        if (!snap.motorJuegosActivities.empty()) {
            json mja = json::array();
            for (const auto& a : snap.motorJuegosActivities) {
                mja.push_back({
                    {"id", a.id}, {"title", a.title}, {"community_id", a.community_id},
                    {"status", a.status}, {"due_date", a.due_date}, {"score", a.score},
                    {"grade_published", a.grade_published}, {"url", a.url}
                });
            }
            j["motor_juegos_activities"] = std::move(mja);
        }

        if (snap.notifications) {
            json narr = json::array();
            for (const auto& n : snap.notifications->notifications) {
                narr.push_back({
                    {"notification_id", n.notification_id},
                    {"origin", n.origin},
                    {"creation_date", n.creation_date},
                    {"expiration_date", n.expiration_date},
                    {"category_id", n.category_id},
                    {"title", n.title},
                    {"text", n.text},
                    {"url", n.url},
                    {"state", n.state}
                });
            }
            j["notifications"] = {
                {"unread_quantity", snap.notifications->unread_quantity},
                {"category_sort", snap.notifications->category_sort},
                {"items", std::move(narr)}
            };
        }

        if (!snap.newsByPath.empty()) {
            json nbp = json::object();
            for (const auto& [path, items] : snap.newsByPath) {
                json arr = json::array();
                for (const auto& it : items) {
                    arr.push_back({{"title", it.title}, {"link", it.link}, {"description", it.description}, {"pub_date", it.pub_date}});
                }
                nbp[path] = std::move(arr);
            }
            j["news_by_path"] = std::move(nbp);
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

        // Version 2 lossless persistence of all secondary & per-course TEC Digital modules
        if (snap.personalAcademicInfo) {
            const auto& pi = *snap.personalAcademicInfo;
            json flds = json::array();
            for (const auto& kv : pi.fields) flds.push_back({{"k", kv.first}, {"v", kv.second}});
            j["personal_academic_info"] = {
                {"full_name", pi.full_name}, {"campus", pi.campus}, {"career", pi.career},
                {"study_plan", pi.study_plan}, {"academic_status", pi.academic_status},
                {"entry_year", pi.entry_year}, {"modality", pi.modality},
                {"photo_url", pi.photo_url}, {"fields", std::move(flds)}
            };
        }
        if (!snap.darSyncPreview.empty()) {
            json arr = json::array();
            for (const auto& d : snap.darSyncPreview) {
                arr.push_back({{"course_name", d.course_name}, {"status_text", d.status_text}, {"state_code", d.state_code}});
            }
            j["dar_sync_preview"] = std::move(arr);
        }
        if (!snap.newsArchiveByPath.empty()) {
            json nabp = json::object();
            for (const auto& [path, items] : snap.newsArchiveByPath) {
                json arr = json::array();
                for (const auto& it : items) {
                    arr.push_back({{"title", it.title}, {"link", it.link}, {"description", it.description}, {"pub_date", it.pub_date}});
                }
                nabp[path] = std::move(arr);
            }
            j["news_archive_by_path"] = std::move(nabp);
        }
        if (!snap.smsByPath.empty()) {
            json obj = json::object();
            for (const auto& [path, items] : snap.smsByPath) {
                json arr = json::array();
                for (const auto& m : items) {
                    arr.push_back({{"content", m.content}, {"transaction_date", m.transaction_date},
                                   {"community", m.community}, {"sent_by", m.sent_by}});
                }
                obj[path] = std::move(arr);
            }
            j["sms_by_path"] = std::move(obj);
        }
        if (!snap.membersByPath.empty()) {
            json obj = json::object();
            for (const auto& [path, items] : snap.membersByPath) {
                json arr = json::array();
                for (const auto& m : items) {
                    arr.push_back({{"user_id", m.user_id}, {"first_name", m.first_name}, {"last_name", m.last_name},
                                   {"full_name", m.full_name}, {"email", m.email}, {"role", m.role}});
                }
                obj[path] = std::move(arr);
            }
            j["members_by_path"] = std::move(obj);
        }
        if (!snap.forumsByPath.empty()) {
            json obj = json::object();
            for (const auto& [path, items] : snap.forumsByPath) {
                json arr = json::array();
                for (const auto& f : items) {
                    arr.push_back({{"message_id", f.message_id}, {"subject", f.subject}, {"author", f.author},
                                   {"posted_date", f.posted_date}, {"replies", f.replies}, {"url", f.url}});
                }
                obj[path] = std::move(arr);
            }
            j["forums_by_path"] = std::move(obj);
        }
        if (!snap.gaapByPath.empty()) {
            json obj = json::object();
            for (const auto& [path, items] : snap.gaapByPath) {
                json arr = json::array();
                for (const auto& g : items) {
                    arr.push_back({{"activity_id", g.activity_id}, {"title", g.title}, {"activity_type", g.activity_type},
                                   {"date_text", g.date_text}, {"score", g.score}, {"status", g.status}});
                }
                obj[path] = std::move(arr);
            }
            j["gaap_by_path"] = std::move(obj);
        }
        if (!snap.rubricsByKey.empty()) {
            json obj = json::object();
            for (const auto& [key, rb] : snap.rubricsByKey) {
                json crits = json::array();
                for (const auto& c : rb.criteria) {
                    json lvs = json::array();
                    for (const auto& lv : c.levels) {
                        lvs.push_back({{"label", lv.label}, {"points", lv.points}, {"description", lv.description}, {"achieved", lv.achieved}});
                    }
                    crits.push_back({{"category_name", c.category_name}, {"earned_points", c.earned_points},
                                     {"max_points", c.max_points}, {"weight_pct", c.weight_pct},
                                     {"earned_weight_pct", c.earned_weight_pct}, {"comment", c.comment},
                                     {"levels", std::move(lvs)}});
                }
                obj[key] = {{"rubric_id", rb.rubric_id}, {"title", rb.title}, {"total_score", rb.total_score},
                            {"max_score", rb.max_score}, {"evaluated_target", rb.evaluated_target},
                            {"max_level_points", rb.max_level_points}, {"is_graded", rb.is_graded},
                            {"criteria", std::move(crits)}};
            }
            j["rubrics_by_key"] = std::move(obj);
        }
        if (!snap.notificationSubscriptions.empty()) {
            json arr = json::array();
            for (const auto& s : snap.notificationSubscriptions) {
                arr.push_back({{"request_id", s.request_id}, {"community", s.community}, {"notice_type", s.notice_type},
                               {"element", s.element}, {"frequency", s.frequency}});
            }
            j["notification_subscriptions"] = std::move(arr);
        }
        if (!snap.schoolCatalog.empty()) {
            json arr = json::array();
            for (const auto& sc : snap.schoolCatalog) {
                arr.push_back({{"dept_code", sc.dept_code}, {"name", sc.name}, {"url", sc.url}});
            }
            j["school_catalog"] = std::move(arr);
        }
        if (snap.userModel) {
            json dims = json::array();
            for (const auto& d : snap.userModel->dimensions) {
                json mets = json::array();
                for (const auto& kv : d.metrics) mets.push_back({{"k", kv.first}, {"v", kv.second}});
                dims.push_back({{"dimension_key", d.dimension_key}, {"title", d.title}, {"metrics", std::move(mets)}});
            }
            j["user_model"] = {{"consent_status", snap.userModel->consent_status}, {"dimensions", std::move(dims)}};
        }
        if (!snap.performanceEvalForms.empty()) {
            json forms = json::array();
            for (const auto& f : snap.performanceEvalForms) {
                json secs = json::array();
                for (const auto& s : f.sections) {
                    json its = json::array();
                    for (const auto& it : s.items) its.push_back({{"id", it.id}, {"description", it.description}, {"order", it.order}, {"type", it.type}});
                    secs.push_back({{"id", s.id}, {"name", s.name}, {"description", s.description}, {"order", s.order}, {"type", s.type}, {"items", std::move(its)}});
                }
                forms.push_back({{"id", f.id}, {"name", f.name}, {"instructions", f.instructions}, {"sections", std::move(secs)}});
            }
            j["performance_eval_forms"] = std::move(forms);
        }
        if (snap.feriaNegocios) {
            const auto& fn = *snap.feriaNegocios;
            json cats = json::array();
            for (const auto& c : fn.categories) cats.push_back({{"id", c.id}, {"name", c.name}});
            json rows = json::array();
            for (const auto& r : fn.feria_rows) {
                rows.push_back({{"name", r.name}, {"start_date", r.start_date}, {"end_date", r.end_date}, {"bases_url", r.bases_url}});
            }
            j["feria_negocios"] = {
                {"location_id", fn.student_location.id}, {"location_name", fn.student_location.name},
                {"categories", std::move(cats)}, {"feria_rows", std::move(rows)}
            };
        }
        if (snap.studentCareer) {
            j["student_career"] = {{"nombre", snap.studentCareer->nombre}, {"sede", snap.studentCareer->sede},
                                   {"carrera", snap.studentCareer->carrera}, {"plan", snap.studentCareer->plan}};
        }
        if (!snap.gestorInscripcionSelectorHtml.empty()) {
            j["gestor_inscripcion_selector_html"] = snap.gestorInscripcionSelectorHtml;
        }
        if (!snap.gestorInscripcionEventsJson.empty()) {
            j["gestor_inscripcion_events_json"] = snap.gestorInscripcionEventsJson;
        }
        if (!snap.plagiarismDocsByCourse.empty()) {
            json pdc = json::object();
            for (const auto& [cpath, docs] : snap.plagiarismDocsByCourse) {
                json arr = json::array();
                for (const auto& d : docs) {
                    arr.push_back({{"item_id", d.item_id}, {"doc_id", d.doc_id}, {"file_name", d.file_name},
                                   {"status", d.status}, {"visibility", d.visibility},
                                   {"similarity_pct", d.similarity_pct}, {"package_from", d.package_from},
                                   {"community_id", d.community_id}});
                }
                pdc[cpath] = std::move(arr);
            }
            j["plagiarism_docs_by_course"] = std::move(pdc);
        }
        if (snap.prematricula) {
            const auto& pm = *snap.prematricula;
            json courses = json::array();
            for (const auto& c : pm.courses) {
                courses.push_back({{"course_code", c.course_code}, {"priority", c.priority},
                                   {"course_elective", c.course_elective}, {"name_elective", c.name_elective}});
            }
            json reqs = json::array();
            for (const auto& r : pm.requests) {
                reqs.push_back({{"course_code", r.course_code}, {"observation", r.observation}});
            }
            j["prematricula"] = {
                {"max_credits", pm.max_credits}, {"rn_student", pm.rn_student},
                {"validated_electives_count", pm.validated_electives_count},
                {"courses", std::move(courses)}, {"requests", std::move(reqs)},
                {"disabled_courses", pm.disabled_courses}
            };
        }
        if (!snap.serverDateTimeIso.empty()) {
            j["server_datetime_iso"] = snap.serverDateTimeIso;
            j["server_time_offset_sec"] = snap.serverTimeOffsetSec;
        }
        if (!snap.consentStatusByModule.empty()) {
            j["consent_status_by_module"] = snap.consentStatusByModule;
        }
        if (!snap.curriculumColors.color_codes.empty()) {
            json ccs = json::array();
            for (const auto& cc : snap.curriculumColors.color_codes) {
                ccs.push_back({{"label", cc.label}, {"css_class", cc.css_class}});
            }
            j["curriculum_colors"] = {{"title", snap.curriculumColors.title},
                                      {"additional_text", snap.curriculumColors.additional_text},
                                      {"color_codes", std::move(ccs)}};
        }
        if (!snap.warnings.empty()) {
            j["warnings"] = snap.warnings;
        }

        fs::path target = fs::path(Utf8ToWide(snapshotCachePathUtf8_));
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream ofs(target, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            if (outErr) *outErr = "No se pudo escribir la caché académica local.";
            return false;
        }
        SanitizeJsonUtf8InPlace(j);
        const std::string serialized = j.dump(2, ' ', false, nlohmann::json::error_handler_t::replace);
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
                ci.parent_url = c.value("parent_url", "");
                ci.is_current_period = c.value("is_current_period", false);
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
                                item.category_name = ij.value("category_name", cat.name);
                                item.name = ij.value("name", "");
                                item.description = ij.value("description", "");
                                item.teacher_comment = ij.value("teacher_comment", "");
                                item.attachment_url = ij.value("attachment_url", "");
                                item.solution_url = ij.value("solution_url", "");
                                item.feedback_url = ij.value("feedback_url", "");
                                item.weight_pct = ij.value("weight_pct", 0.0);
                                item.earned_pct = ij.value("earned_pct", 0.0);
                                item.raw_grade_100 = ij.value("raw_grade_100", -1.0);
                                item.is_graded = ij.value("is_graded", false);
                                item.due_date_iso = ij.value("due_date_iso", "");
                                item.due_date_text = ij.value("due_date_text", "");
                                item.due_date_gmt = ij.value("due_date_gmt", "");
                                item.allow_late_submission = ij.value("allow_late_submission", false);
                                item.group_size = ij.value("group_size", 1);
                                if (ij.contains("group_members") && ij["group_members"].is_array()) {
                                    item.group_members = ij["group_members"].get<std::vector<std::string>>();
                                }
                                item.has_rubric = ij.value("has_rubric", false);
                                item.rubric_id = ij.value("rubric_id", "");
                                item.task_id = ij.value("task_id", "");
                                item.task_item_id = ij.value("task_item_id", "");
                                item.community_id = ij.value("community_id", "");
                                item.grade_id = ij.value("grade_id", "");
                                // student_id intentionally never loaded from disk (fetched at runtime)
                                item.with_group = ij.value("with_group", "0");
                                item.can_upload = ij.value("can_upload", false);
                                item.can_upload_url = ij.value("can_upload_url", false);
                                item.max_file_size_mb = ij.value("max_file_size_mb", 80);
                                if (ij.contains("submitted_files") && ij["submitted_files"].is_array()) {
                                    item.submitted_files = ij["submitted_files"].get<std::vector<std::string>>();
                                }
                                item.latest_revision_id = ij.value("latest_revision_id", "");
                                item.latest_item_id = ij.value("latest_item_id", "");
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
        auto deserializeCalArr = [](const json& arr) {
            std::vector<td::CalendarEventEntry> out;
            if (!arr.is_array()) return out;
            for (const auto& ej : arr) {
                td::CalendarEventEntry ev;
                ev.uid = ej.value("uid", "");
                ev.date_iso = ej.value("date_iso", "");
                ev.start_time = ej.value("start_time", "");
                ev.end_time = ej.value("end_time", "");
                ev.title = ej.value("title", "");
                ev.calendar_name = ej.value("calendar_name", "");
                ev.description = ej.value("description", "");
                ev.source_kind = ej.value("source_kind", "calendar");
                out.push_back(std::move(ev));
            }
            return out;
        };
        if (j.contains("unified_calendar")) loaded.unifiedCalendarEvents = deserializeCalArr(j["unified_calendar"]);
        if (j.contains("portal_calendar")) loaded.portalCalendarEvents = deserializeCalArr(j["portal_calendar"]);
        if (j.contains("course_calendars_by_path") && j["course_calendars_by_path"].is_object()) {
            for (auto it = j["course_calendars_by_path"].begin(); it != j["course_calendars_by_path"].end(); ++it) {
                loaded.courseCalendarsByPath[it.key()] = deserializeCalArr(it.value());
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
            st.credits_no_grade = sj.value("credits_no_grade", 0);
            st.subjects_consulted = sj.value("subjects_consulted", 0);
            st.subjects_taken = sj.value("subjects_taken", 0);
            st.count_aprobadas = sj.value("aprobadas", 0);
            st.count_en_curso = sj.value("en_curso", 0);
            st.count_reprobadas = sj.value("reprobadas", 0);
            st.count_retiradas = sj.value("retiradas", 0);
            st.count_reconocidas = sj.value("reconocidas", 0);
            st.count_suficiencia = sj.value("suficiencia", 0);
            st.count_pendientes = sj.value("pendientes", 0);
            st.count_congeladas = sj.value("congeladas", 0);
            ex.active_schedule_period = sj.value("active_schedule_period", "");
            ex.weighted_average_text = sj.value("weighted_average_text", "");
            ex.approval_pct_text = sj.value("approval_pct_text", "");
            if (j.contains("expediente_grades") && j["expediente_grades"].is_array()) {
                for (const auto& rj : j["expediente_grades"]) {
                    td::GradeRecordRow r;
                    r.year = rj.value("year", 0);
                    r.state = rj.value("state", "");
                    r.grade = rj.value("grade", 0.0);
                    r.grade_text = rj.value("grade_text", "");
                    r.course_code = rj.value("course_code", "");
                    r.course_name = rj.value("course_name", "");
                    r.modality = rj.value("modality", "");
                    r.period = rj.value("period", "");
                    r.credits = rj.value("credits", 0);
                    ex.structured.grades.push_back(std::move(r));
                }
            }
            if (j.contains("expediente_weighted_averages") && j["expediente_weighted_averages"].is_array()) {
                for (const auto& wj : j["expediente_weighted_averages"]) {
                    ex.structured.weighted_averages.push_back({
                        wj.value("year", 0), wj.value("modality", ""),
                        wj.value("period", ""), wj.value("weighted_avg", 0.0)
                    });
                }
            }
            if (j.contains("expediente_citas") && j["expediente_citas"].is_array()) {
                for (const auto& cj : j["expediente_citas"]) {
                    ex.structured.citas_matricula.push_back({
                        cj.value("date", ""), cj.value("period", ""),
                        cj.value("modality", ""), cj.value("time", "")
                    });
                }
            }
            if (j.contains("expediente_sedes") && j["expediente_sedes"].is_array()) {
                for (const auto& xj : j["expediente_sedes"]) {
                    ex.sedes_combo.push_back({xj.value("key", ""), xj.value("name", "")});
                }
            }
            if (j.contains("expediente_periodos") && j["expediente_periodos"].is_array()) {
                for (const auto& xj : j["expediente_periodos"]) {
                    ex.periodos_combo.push_back({xj.value("key", ""), xj.value("name", "")});
                }
            }
            loaded.expediente = std::move(ex);
        }
        if (j.contains("curriculum_sedes") && j["curriculum_sedes"].is_array()) {
            for (const auto& xj : j["curriculum_sedes"]) {
                loaded.curriculumSedes.push_back({xj.value("key", ""), xj.value("name", "")});
            }
        }
        if (j.contains("curriculum_carreras") && j["curriculum_carreras"].is_array()) {
            for (const auto& xj : j["curriculum_carreras"]) {
                loaded.curriculumCarreras.push_back({xj.value("key", ""), xj.value("name", "")});
            }
        }
        if (j.contains("curriculum_planes") && j["curriculum_planes"].is_array()) {
            for (const auto& xj : j["curriculum_planes"]) {
                loaded.curriculumPlanes.push_back({xj.value("key", ""), xj.value("name", "")});
            }
        }
        if (j.contains("public_guia_escuelas") && j["public_guia_escuelas"].is_array()) {
            for (const auto& xj : j["public_guia_escuelas"]) {
                loaded.publicGuiaEscuelas.push_back({xj.value("ide_depto", ""), xj.value("dsc_depto", "")});
            }
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
                cell.building = cj.value("building", "");
                cell.professor = cj.value("professor", "");
                cell.credits = cj.value("credits", 0);
                cell.raw_text = cj.value("raw_text", "");
                loaded.currentSchedule.push_back(std::move(cell));
            }
        }
        auto deserializeFolderWalk = [](const json& obj) -> td::FolderWalkResult {
            td::FolderWalkResult fwr;
            fwr.root_folder_id = obj.value("root_folder_id", "");
            if (obj.contains("folders") && obj["folders"].is_array()) {
                for (const auto& dj : obj["folders"]) {
                    td::FolderRecord d;
                    d.folder_id = dj.value("folder_id", "");
                    d.parent_id = dj.value("parent_id", "");
                    d.name = dj.value("name", "");
                    d.folder_path = dj.value("folder_path", "");
                    d.creation_date_ansi = dj.value("creation_date_ansi", "");
                    d.last_modified_ansi = dj.value("last_modified_ansi", "");
                    d.author_name = dj.value("author_name", "");
                    d.subscription_active = dj.value("subscription_active", false);
                    d.depth = dj.value("depth", 0);
                    d.child_count = dj.value("child_count", size_t{0});
                    fwr.folders.push_back(std::move(d));
                }
            }
            if (obj.contains("files") && obj["files"].is_array()) {
                for (const auto& fj : obj["files"]) {
                    td::FileRecord f;
                    f.object_id = fj.value("object_id", "");
                    f.parent_folder_id = fj.value("parent_folder_id", "");
                    f.name = fj.value("name", "");
                    f.title = fj.value("title", "");
                    f.folder_path = fj.value("folder_path", "");
                    f.mime_type = fj.value("mime_type", "");
                    f.content_size = fj.value("content_size", uint64_t{0});
                    f.live_revision = fj.value("live_revision", "");
                    f.creation_date_ansi = fj.value("creation_date_ansi", "");
                    f.last_modified_ansi = fj.value("last_modified_ansi", "");
                    f.author_name = fj.value("author_name", "");
                    f.subscription_active = fj.value("subscription_active", false);
                    f.download_url = fj.value("download_url", "");
                    f.status = static_cast<td::FileStatus>(fj.value("status", 0));
                    f.logical_rel_path = fj.value("logical_rel_path", "");
                    f.blob_rel_path = fj.value("blob_rel_path", "");
                    f.plagiarism_percentage = fj.value("plagiarism_percentage", -1.0);
                    f.plagiarism_status = fj.value("plagiarism_status", "");
                    f.plagiarism_doc_uid = fj.value("plagiarism_doc_uid", "");
                    fwr.files.push_back(std::move(f));
                }
            }
            if (obj.contains("url_links") && obj["url_links"].is_array()) {
                for (const auto& uj : obj["url_links"]) {
                    td::FolderUrlLink u;
                    u.object_id = uj.value("object_id", "");
                    u.parent_folder_id = uj.value("parent_folder_id", "");
                    u.title = uj.value("title", "");
                    u.url = uj.value("url", "");
                    u.folder_path = uj.value("folder_path", "");
                    fwr.url_links.push_back(std::move(u));
                }
            }
            return fwr;
        };

        auto deserializePortlet = [](const json& pj) -> td::SpacePortletBlock {
            td::SpacePortletBlock pb;
            pb.title = pj.value("title", "");
            pb.body_preview = pj.value("body_preview", "");
            pb.module_kind = pj.value("module_kind", "");
            if (pj.contains("links") && pj["links"].is_array()) {
                for (const auto& lj : pj["links"]) pb.links.push_back({lj.value("label", ""), lj.value("url", ""), lj.value("category", "")});
            }
            if (pj.contains("images") && pj["images"].is_array()) {
                for (const auto& ij : pj["images"]) pb.images.push_back({ij.value("image_url", ""), ij.value("alt_text", ""), ij.value("link_url", "")});
            }
            if (pj.contains("linked_subpages") && pj["linked_subpages"].is_array()) {
                for (const auto& sj : pj["linked_subpages"]) pb.linked_subpages.push_back({sj.value("page_num", 0), sj.value("label", ""), sj.value("url", ""), sj.value("is_custom", true)});
            }
            return pb;
        };

        if (j.contains("folders_by_path") && j["folders_by_path"].is_object()) {
            for (auto it = j["folders_by_path"].begin(); it != j["folders_by_path"].end(); ++it) {
                loaded.foldersByPath[it.key()] = deserializeFolderWalk(it.value());
            }
        }
        if (j.contains("personal_documents") && j["personal_documents"].is_object()) {
            loaded.personalDocumentsFolderId = j.value("personal_documents_folder_id", "");
            loaded.personalDocuments = deserializeFolderWalk(j["personal_documents"]);
        }
        if (j.contains("overviews_by_path") && j["overviews_by_path"].is_object()) {
            for (auto it = j["overviews_by_path"].begin(); it != j["overviews_by_path"].end(); ++it) {
                const auto& o = it.value();
                td::CourseOverviewSnapshot ov;
                ov.community_path = it.key();
                ov.title = o.value("title", "");
                ov.description = o.value("description", "");
                ov.teacher.name = o.value("teacher_name", "");
                ov.teacher.office = o.value("teacher_office", "");
                ov.teacher.teams_handle = o.value("teacher_teams", "");
                if (o.contains("consultation_hours") && o["consultation_hours"].is_array()) {
                    ov.teacher.consultation_hours = o["consultation_hours"].get<std::vector<std::string>>();
                }
                if (o.contains("weekly_sections") && o["weekly_sections"].is_array()) {
                    ov.weekly_sections = o["weekly_sections"].get<std::vector<std::string>>();
                }
                if (o.contains("tabs") && o["tabs"].is_array()) {
                    for (const auto& tj : o["tabs"]) {
                        ov.tabs.push_back({tj.value("page_num", 0), tj.value("label", ""), tj.value("url", ""), tj.value("is_custom", false)});
                    }
                }
                if (o.contains("banners") && o["banners"].is_array()) {
                    for (const auto& bj : o["banners"]) {
                        ov.banners.push_back({bj.value("image_url", ""), bj.value("alt_text", ""), bj.value("link_url", "")});
                    }
                }
                if (o.contains("portlets") && o["portlets"].is_array()) {
                    for (const auto& pj : o["portlets"]) ov.portlets.push_back(deserializePortlet(pj));
                }
                if (o.contains("official_docs") && o["official_docs"].is_array()) {
                    for (const auto& dj : o["official_docs"]) {
                        ov.official_docs.push_back({dj.value("label", ""), dj.value("url", ""), dj.value("category", "")});
                    }
                }
                loaded.overviewsByPath[it.key()] = std::move(ov);
            }
        }
        if (j.contains("custom_pages_by_space") && j["custom_pages_by_space"].is_object()) {
            for (auto it = j["custom_pages_by_space"].begin(); it != j["custom_pages_by_space"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::SpacePageSnapshot> pages;
                for (const auto& pj : it.value()) {
                    td::SpacePageSnapshot pg;
                    pg.page_num = pj.value("page_num", 0);
                    pg.tab_label = pj.value("tab_label", "");
                    pg.page_url = pj.value("page_url", "");
                    pg.plain_excerpt = pj.value("plain_excerpt", "");
                    if (pj.contains("banners") && pj["banners"].is_array()) {
                        for (const auto& bj : pj["banners"]) pg.banners.push_back({bj.value("image_url", ""), bj.value("alt_text", ""), bj.value("link_url", "")});
                    }
                    if (pj.contains("portlets") && pj["portlets"].is_array()) {
                        for (const auto& ptj : pj["portlets"]) pg.portlets.push_back(deserializePortlet(ptj));
                    }
                    if (pj.contains("links") && pj["links"].is_array()) {
                        for (const auto& lj : pj["links"]) pg.links.push_back({lj.value("label", ""), lj.value("url", ""), lj.value("category", "")});
                    }
                    pages.push_back(std::move(pg));
                }
                loaded.customPagesBySpace[it.key()] = std::move(pages);
            }
        }
        loaded.monitorIntervalMinutes = j.value("monitor_interval_minutes", 15);
        loaded.gaapServerUrl = j.value("gaap_server_url", "");
        if (j.contains("activity_feed") && j["activity_feed"].is_array()) {
            for (const auto& ej : j["activity_feed"]) {
                td::TecChangeEvent ev;
                ev.id = ej.value("id", "");
                ev.kind = static_cast<td::TecChangeEventKind>(ej.value("kind", 0));
                ev.detected_at_utc = ej.value("detected_at_utc", "");
                ev.space_url = ej.value("space_url", "");
                ev.space_name = ej.value("space_name", "");
                ev.title = ej.value("title", "");
                ev.detail = ej.value("detail", "");
                ev.target_id = ej.value("target_id", "");
                ev.unread = ej.value("unread", false);
                if (!ev.id.empty()) loaded.activityFeed.push_back(std::move(ev));
            }
        }
        if (j.contains("motor_juegos_user") && j["motor_juegos_user"].is_object()) {
            td::MotorJuegosUserInfo u;
            u.user_id = j["motor_juegos_user"].value("user_id", int64_t{0});
            u.is_professor = j["motor_juegos_user"].value("is_professor", false);
            loaded.motorJuegosUser = u;
        }
        if (j.contains("motor_juegos_activities") && j["motor_juegos_activities"].is_array()) {
            for (const auto& aj : j["motor_juegos_activities"]) {
                td::MotorJuegosActivity a;
                a.id = aj.value("id", "");
                a.title = aj.value("title", "");
                a.community_id = aj.value("community_id", "");
                a.status = aj.value("status", "");
                a.due_date = aj.value("due_date", "");
                a.score = aj.value("score", -1.0);
                a.grade_published = aj.value("grade_published", false);
                a.url = aj.value("url", "");
                loaded.motorJuegosActivities.push_back(std::move(a));
            }
        }
        if (j.contains("notifications") && j["notifications"].is_object()) {
            const auto& nj = j["notifications"];
            td::NotificationsFeed nf;
            nf.unread_quantity = nj.value("unread_quantity", 0);
            nf.category_sort = nj.value("category_sort", false);
            if (nj.contains("items") && nj["items"].is_array()) {
                for (const auto& ij : nj["items"]) {
                    td::NotificationItem ni;
                    ni.notification_id = ij.value("notification_id", "");
                    ni.origin = ij.value("origin", int64_t{0});
                    ni.creation_date = ij.value("creation_date", "");
                    ni.expiration_date = ij.value("expiration_date", "");
                    ni.category_id = ij.value("category_id", 0);
                    ni.title = ij.value("title", "");
                    ni.text = ij.value("text", "");
                    ni.url = ij.value("url", "");
                    ni.state = ij.value("state", "R");
                    nf.notifications.push_back(std::move(ni));
                }
            }
            loaded.notifications = std::move(nf);
        }
        if (j.contains("news_by_path") && j["news_by_path"].is_object()) {
            for (auto it = j["news_by_path"].begin(); it != j["news_by_path"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::RssNewsItem> items;
                for (const auto& nj : it.value()) {
                    td::RssNewsItem r;
                    r.title = nj.value("title", "");
                    r.link = nj.value("link", "");
                    r.description = nj.value("description", "");
                    r.pub_date = nj.value("pub_date", "");
                    items.push_back(std::move(r));
                }
                loaded.newsByPath[it.key()] = std::move(items);
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

        // Version 2 lossless deserialization
        if (j.contains("personal_academic_info") && j["personal_academic_info"].is_object()) {
            const auto& pj = j["personal_academic_info"];
            td::PersonalAcademicInfo pi;
            pi.full_name = pj.value("full_name", "");
            pi.campus = pj.value("campus", "");
            pi.career = pj.value("career", "");
            pi.study_plan = pj.value("study_plan", "");
            pi.academic_status = pj.value("academic_status", "");
            pi.entry_year = pj.value("entry_year", "");
            pi.modality = pj.value("modality", "");
            pi.photo_url = pj.value("photo_url", "");
            if (pj.contains("fields") && pj["fields"].is_array()) {
                for (const auto& fj : pj["fields"]) pi.fields.emplace_back(fj.value("k", ""), fj.value("v", ""));
            }
            loaded.personalAcademicInfo = std::move(pi);
        }
        if (j.contains("dar_sync_preview") && j["dar_sync_preview"].is_array()) {
            for (const auto& dj : j["dar_sync_preview"]) {
                loaded.darSyncPreview.push_back({dj.value("course_name", ""), dj.value("status_text", ""), dj.value("state_code", "")});
            }
        }
        if (j.contains("news_archive_by_path") && j["news_archive_by_path"].is_object()) {
            for (auto it = j["news_archive_by_path"].begin(); it != j["news_archive_by_path"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::RssNewsItem> items;
                for (const auto& nj : it.value()) {
                    td::RssNewsItem r;
                    r.title = nj.value("title", "");
                    r.link = nj.value("link", "");
                    r.description = nj.value("description", "");
                    r.pub_date = nj.value("pub_date", "");
                    items.push_back(std::move(r));
                }
                loaded.newsArchiveByPath[it.key()] = std::move(items);
            }
        }
        if (j.contains("sms_by_path") && j["sms_by_path"].is_object()) {
            for (auto it = j["sms_by_path"].begin(); it != j["sms_by_path"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::SmsMessageRecord> vec;
                for (const auto& mj : it.value()) {
                    vec.push_back({mj.value("content", ""), mj.value("transaction_date", ""),
                                   mj.value("community", ""), mj.value("sent_by", "")});
                }
                loaded.smsByPath[it.key()] = std::move(vec);
            }
        }
        if (j.contains("members_by_path") && j["members_by_path"].is_object()) {
            for (auto it = j["members_by_path"].begin(); it != j["members_by_path"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::CommunityMember> vec;
                for (const auto& mj : it.value()) {
                    vec.push_back({mj.value("user_id", ""), mj.value("first_name", ""), mj.value("last_name", ""),
                                   mj.value("full_name", ""), mj.value("email", ""), mj.value("role", "")});
                }
                loaded.membersByPath[it.key()] = std::move(vec);
            }
        }
        if (j.contains("forums_by_path") && j["forums_by_path"].is_object()) {
            for (auto it = j["forums_by_path"].begin(); it != j["forums_by_path"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::ForumThreadSummary> vec;
                for (const auto& fj : it.value()) {
                    vec.push_back({fj.value("message_id", ""), fj.value("subject", ""), fj.value("author", ""),
                                   fj.value("posted_date", ""), fj.value("replies", 0), fj.value("url", "")});
                }
                loaded.forumsByPath[it.key()] = std::move(vec);
            }
        }
        if (j.contains("gaap_by_path") && j["gaap_by_path"].is_object()) {
            for (auto it = j["gaap_by_path"].begin(); it != j["gaap_by_path"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::GaapActivityRecord> vec;
                for (const auto& gj : it.value()) {
                    vec.push_back({gj.value("activity_id", ""), gj.value("title", ""), gj.value("activity_type", ""),
                                   gj.value("date_text", ""), gj.value("score", -1.0), gj.value("status", "")});
                }
                loaded.gaapByPath[it.key()] = std::move(vec);
            }
        }
        if (j.contains("rubrics_by_key") && j["rubrics_by_key"].is_object()) {
            for (auto it = j["rubrics_by_key"].begin(); it != j["rubrics_by_key"].end(); ++it) {
                const auto& rj = it.value();
                td::RubricBreakdown rb;
                rb.rubric_id = rj.value("rubric_id", "");
                rb.title = rj.value("title", "");
                rb.total_score = rj.value("total_score", 0.0);
                rb.max_score = rj.value("max_score", 0.0);
                rb.evaluated_target = rj.value("evaluated_target", "");
                rb.max_level_points = rj.value("max_level_points", 0.0);
                rb.is_graded = rj.value("is_graded", false);
                if (rj.contains("criteria") && rj["criteria"].is_array()) {
                    for (const auto& cj : rj["criteria"]) {
                        td::RubricCriterion cr;
                        cr.category_name = cj.value("category_name", "");
                        cr.earned_points = cj.value("earned_points", 0.0);
                        cr.max_points = cj.value("max_points", 0.0);
                        cr.weight_pct = cj.value("weight_pct", 0.0);
                        cr.earned_weight_pct = cj.value("earned_weight_pct", 0.0);
                        cr.comment = cj.value("comment", "");
                        if (cj.contains("levels") && cj["levels"].is_array()) {
                            for (const auto& lvj : cj["levels"]) {
                                cr.levels.push_back({lvj.value("label", ""), lvj.value("points", 0.0),
                                                     lvj.value("description", ""), lvj.value("achieved", false)});
                            }
                        }
                        rb.criteria.push_back(std::move(cr));
                    }
                }
                loaded.rubricsByKey[it.key()] = std::move(rb);
            }
        }
        if (j.contains("notification_subscriptions") && j["notification_subscriptions"].is_array()) {
            for (const auto& sj : j["notification_subscriptions"]) {
                loaded.notificationSubscriptions.push_back({
                    sj.value("request_id", ""), sj.value("community", ""), sj.value("notice_type", ""),
                    sj.value("element", ""), sj.value("frequency", "")
                });
            }
        }
        if (j.contains("school_catalog") && j["school_catalog"].is_array()) {
            for (const auto& sj : j["school_catalog"]) {
                loaded.schoolCatalog.push_back({sj.value("dept_code", ""), sj.value("name", ""), sj.value("url", "")});
            }
        }
        if (j.contains("user_model") && j["user_model"].is_object()) {
            const auto& uj = j["user_model"];
            td::UserModelSnapshot um;
            um.consent_status = uj.value("consent_status", "");
            if (uj.contains("dimensions") && uj["dimensions"].is_array()) {
                for (const auto& dj : uj["dimensions"]) {
                    td::UserModelDimension dim;
                    dim.dimension_key = dj.value("dimension_key", "");
                    dim.title = dj.value("title", "");
                    if (dj.contains("metrics") && dj["metrics"].is_array()) {
                        for (const auto& mj : dj["metrics"]) dim.metrics.emplace_back(mj.value("k", ""), mj.value("v", ""));
                    }
                    um.dimensions.push_back(std::move(dim));
                }
            }
            loaded.userModel = std::move(um);
        }
        if (j.contains("performance_eval_forms") && j["performance_eval_forms"].is_array()) {
            for (const auto& fj : j["performance_eval_forms"]) {
                td::PerformanceForm pf;
                pf.id = fj.value("id", int64_t{0});
                pf.name = fj.value("name", "");
                pf.instructions = fj.value("instructions", "");
                if (fj.contains("sections") && fj["sections"].is_array()) {
                    for (const auto& sj : fj["sections"]) {
                        td::PerformanceFormSection sec;
                        sec.id = sj.value("id", int64_t{0});
                        sec.name = sj.value("name", "");
                        sec.description = sj.value("description", "");
                        sec.order = sj.value("order", 0);
                        sec.type = sj.value("type", "");
                        if (sj.contains("items") && sj["items"].is_array()) {
                            for (const auto& ij : sj["items"]) {
                                sec.items.push_back({ij.value("id", int64_t{0}), ij.value("description", ""), ij.value("order", 0), ij.value("type", "")});
                            }
                        }
                        pf.sections.push_back(std::move(sec));
                    }
                }
                loaded.performanceEvalForms.push_back(std::move(pf));
            }
        }
        if (j.contains("feria_negocios") && j["feria_negocios"].is_object()) {
            const auto& fnj = j["feria_negocios"];
            td::FeriaNegociosSnapshot fn;
            fn.student_location.id = fnj.value("location_id", "");
            fn.student_location.name = fnj.value("location_name", "");
            if (fnj.contains("categories") && fnj["categories"].is_array()) {
                for (const auto& cj : fnj["categories"]) fn.categories.push_back({cj.value("id", int64_t{0}), cj.value("name", "")});
            }
            if (fnj.contains("feria_rows") && fnj["feria_rows"].is_array()) {
                for (const auto& rj : fnj["feria_rows"]) {
                    fn.feria_rows.push_back({rj.value("name", ""), rj.value("start_date", ""), rj.value("end_date", ""), rj.value("bases_url", "")});
                }
            }
            loaded.feriaNegocios = std::move(fn);
        }
        if (j.contains("student_career") && j["student_career"].is_object()) {
            const auto& scj = j["student_career"];
            loaded.studentCareer = td::StudentCareerRecord{
                scj.value("nombre", ""), scj.value("sede", ""), scj.value("carrera", ""), scj.value("plan", int64_t{0})
            };
        }
        loaded.gestorInscripcionSelectorHtml = j.value("gestor_inscripcion_selector_html", "");
        loaded.gestorInscripcionEventsJson = j.value("gestor_inscripcion_events_json", "");
        if (j.contains("plagiarism_docs_by_course") && j["plagiarism_docs_by_course"].is_object()) {
            for (auto it = j["plagiarism_docs_by_course"].begin(); it != j["plagiarism_docs_by_course"].end(); ++it) {
                if (!it.value().is_array()) continue;
                std::vector<td::PlagiarismMisDocItem> vec;
                for (const auto& dj : it.value()) {
                    td::PlagiarismMisDocItem item;
                    item.item_id = dj.value("item_id", "");
                    item.doc_id = dj.value("doc_id", "");
                    item.file_name = dj.value("file_name", "");
                    item.status = dj.value("status", "");
                    item.visibility = dj.value("visibility", "");
                    item.similarity_pct = dj.value("similarity_pct", -1.0);
                    item.package_from = dj.value("package_from", "");
                    item.community_id = dj.value("community_id", "");
                    vec.push_back(std::move(item));
                }
                loaded.plagiarismDocsByCourse[it.key()] = std::move(vec);
            }
        }
        if (j.contains("prematricula") && j["prematricula"].is_object()) {
            const auto& pmj = j["prematricula"];
            td::PrematriculaSnapshot pm;
            pm.max_credits = pmj.value("max_credits", 0);
            pm.rn_student = pmj.value("rn_student", "");
            pm.validated_electives_count = pmj.value("validated_electives_count", 0);
            if (pmj.contains("courses") && pmj["courses"].is_array()) {
                for (const auto& cj : pmj["courses"]) {
                    pm.courses.push_back({cj.value("course_code", ""), cj.value("priority", 0),
                                          cj.value("course_elective", ""), cj.value("name_elective", "")});
                }
            }
            if (pmj.contains("requests") && pmj["requests"].is_array()) {
                for (const auto& rj : pmj["requests"]) {
                    pm.requests.push_back({rj.value("course_code", ""), rj.value("observation", "")});
                }
            }
            if (pmj.contains("disabled_courses") && pmj["disabled_courses"].is_array()) {
                for (const auto& dc : pmj["disabled_courses"]) if (dc.is_string()) pm.disabled_courses.push_back(dc.get<std::string>());
            }
            loaded.prematricula = std::move(pm);
        }
        loaded.serverDateTimeIso = j.value("server_datetime_iso", "");
        loaded.serverTimeOffsetSec = j.value("server_time_offset_sec", std::int64_t{0});
        if (j.contains("consent_status_by_module") && j["consent_status_by_module"].is_object()) {
            for (auto it = j["consent_status_by_module"].begin(); it != j["consent_status_by_module"].end(); ++it) {
                if (it.value().is_string()) loaded.consentStatusByModule[it.key()] = it.value().get<std::string>();
            }
        }
        if (j.contains("curriculum_colors") && j["curriculum_colors"].is_object()) {
            const auto& ccj = j["curriculum_colors"];
            loaded.curriculumColors.title = ccj.value("title", "");
            loaded.curriculumColors.additional_text = ccj.value("additional_text", "");
            if (ccj.contains("color_codes") && ccj["color_codes"].is_array()) {
                for (const auto& c : ccj["color_codes"]) {
                    loaded.curriculumColors.color_codes.push_back({c.value("label", ""), c.value("css_class", "")});
                }
            }
        }
        if (j.contains("warnings") && j["warnings"].is_array()) {
            for (const auto& w : j["warnings"]) if (w.is_string()) loaded.warnings.push_back(w.get<std::string>());
        }

        if (loaded.currentSchedule.empty() && !loaded.communities.empty()) {
            GlobalHorariosIndex idxCopy;
            {
                std::lock_guard<std::mutex> glk(globalHorariosMutex_);
                idxCopy = globalHorariosIndex_;
            }
            if (!idxCopy.entries.empty()) {
                auto derived = DeriveScheduleCellsFromEnrolledAndGlobal(loaded.communities, idxCopy);
                if (!derived.empty()) loaded.currentSchedule = std::move(derived);
            }
        }
        // Purge any legacy non-ISO calendar rows (e.g. from pre-fix #at-a-glance snapshots)
        for (auto it = loaded.courseCalendarsByPath.begin(); it != loaded.courseCalendarsByPath.end(); ) {
            auto& vec = it->second;
            vec.erase(std::remove_if(vec.begin(), vec.end(), [](const td::CalendarEventEntry& e) {
                          return e.date_iso.size() != 10 || e.date_iso[4] != '-' || e.date_iso[7] != '-';
                      }),
                      vec.end());
            if (vec.empty()) it = loaded.courseCalendarsByPath.erase(it);
            else ++it;
        }
        for (auto& [mk, mv] : loaded.consentStatusByModule) {
            std::string t = mv;
            while (!t.empty() && std::isspace(static_cast<unsigned char>(t.front()))) t.erase(t.begin());
            while (!t.empty() && std::isspace(static_cast<unsigned char>(t.back()))) t.pop_back();
            if (t.empty()) mv = "Aceptado / al día";
        }
        loaded.sessionAuthenticated = authenticated_.load(std::memory_order_acquire);
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
