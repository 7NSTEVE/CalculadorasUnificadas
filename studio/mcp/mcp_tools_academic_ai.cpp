#include "mcp_server.hpp"

#include "../core/accessibility_engine.hpp"
#include "../core/ai_provider_vault.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/study_assistant_tools.hpp"
#include "../core/utf_conv.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../tutor/tutor_document_baker.hpp"
#include "../tutor/tutor_whiteboard_protocol.hpp"

#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
#include "../integrations/gemini_notebook/gemini_notebook_client.hpp"
#include "../integrations/gemini_notebook/gemini_notebook_sync_store.hpp"
#endif

#ifdef DOC_STUDIO_HAS_ACADEMIC
#include "../integrations/tec_digital/tec_academic_store.hpp"
#endif

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::string LowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::uint64_t NowEpochSec() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string UrlEncodeComponent(const std::string& raw) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(raw.size() * 2u);
    for (unsigned char c : raw) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[(c >> 4) & 0x0Fu]);
            out.push_back(kHex[c & 0x0Fu]);
        }
    }
    return out;
}

McpToolResult TutorSidecarHttpCall(
    McpServerContext& ctx,
    const std::string& method,
    const std::string& pathWithQuery,
    const std::string& bodyUtf8 = "")
{
    const auto ep = ctx.tutorSidecar.Endpoint();
    if (!ep.running || ep.baseHttpUrl.empty()) {
        return McpToolResult::Err("Sidecar Python del Tutor no iniciado. Llame primero a action='sidecar_start'.");
    }
    const std::string url = ep.baseHttpUrl + pathWithQuery;
    // Note: tools/tutor_runtime/server.py authenticates via X-CuadernU-Token;
    // HttpRequestWinHttp automatically adds Content-Type: application/json when bodyPtr != nullptr
    // (adding a second Content-Type header triggers WinError 183 ERROR_ALREADY_EXISTS in WinHTTP).
    std::vector<std::pair<std::string, std::string>> hdrs = {
        { "X-CuadernU-Token", ep.sessionTokenHex }
    };
    std::string respBody, err;
    int httpStatus = 0;
    const std::wstring methodW = Utf8ToWide(method);
    const std::string* bodyPtr = bodyUtf8.empty() ? nullptr : &bodyUtf8;
    const bool ok = AiProviderVault::HttpRequestWinHttp(
        url, methodW.c_str(), bodyPtr, hdrs, 30000, respBody, &httpStatus, nullptr, &err);
    if (!ok) return McpToolResult::Err("Error llamando a sidecar Tutor: " + err);
    try {
        json parsed = json::parse(respBody);
        return (httpStatus >= 200 && httpStatus < 300)
            ? McpToolResult::Ok({ { "http_status", httpStatus }, { "data", std::move(parsed) } })
            : McpToolResult::Err("HTTP " + std::to_string(httpStatus), parsed);
    } catch (...) {
        return (httpStatus >= 200 && httpStatus < 300)
            ? McpToolResult::Ok({ { "http_status", httpStatus }, { "text", respBody } })
            : McpToolResult::Err("HTTP " + std::to_string(httpStatus) + ": " + respBody);
    }
}

} // namespace

void RegisterAcademicAndAiTools(McpProtocolRouter& router, McpServerContext& ctx) {
    // ---------------- Group G: TEC Digital & HorariosTEC Complete Suite (3 tools) ----------------

    router.RegisterTool({
        "cuadernu_tec_academic",
        "Suite completa de solo lectura, sincronizacion, biblioteca global de archivos, FTS5 offline, Radar de cambios y constructores de lienzo de TEC Digital.",
        MakeToolSchema(
            {
                "status", "restore_session", "login_password", "login_qr", "logout_local",
                "sync_all", "sync_course_files", "sync_course_on_demand", "sync_personal_docs",
                "get_snapshot", "fetch_space_page", "fetch_news_article", "fetch_rubric",
                "fetch_file_details", "fetch_file_versions", "fetch_eval_history", "fetch_forum_thread",
                "fetch_members_csv", "fetch_plagiarism_report_url", "download_file",
                "global_file_catalog", "search_offline_fts", "cache_stats", "cache_prune",
                "radar_mark_read", "radar_mark_all_read", "auto_monitor_configure",
                "insert_course_to_canvas", "insert_curriculum_to_canvas", "import_material_to_canvas",
                "save_course_cnu", "generate_sm2_flashcards", "plan_gemini_candidates"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "email", { { "type", "string" } } },
                { "password", { { "type", "string" } } },
                { "persist_vault", { { "type", "boolean" } } },
                { "delete_vault", { { "type", "boolean" } } },
                { "community_url", { { "type", "string" } } },
                { "download_files", { { "type", "boolean" } } },
                { "section", { { "type", "string" }, { "description", "summary|communities|evaluations|rubrics|calendar|news|forums|expediente|curriculum|plagiarism|prematricula|feria|notifications|personal_docs|radar|custom_pages" } } },
                { "page_num", { { "type", "integer" } } },
                { "tab_label", { { "type", "string" } } },
                { "article_url", { { "type", "string" } } },
                { "rubric_id", { { "type", "string" } } },
                { "community_id", { { "type", "string" } } },
                { "task_item_id", { { "type", "string" } } },
                { "task_id", { { "type", "string" } } },
                { "assign_id", { { "type", "string" } } },
                { "object_id", { { "type", "string" } } },
                { "message_id", { { "type", "string" } } },
                { "doc_uid", { { "type", "string" } } },
                { "query", { { "type", "string" } } },
                { "space_type", { { "type", "string" } } },
                { "max_results", { { "type", "integer" } } },
                { "target_max_bytes", { { "type", "integer" } } },
                { "event_id", { { "type", "string" } } },
                { "interval_minutes", { { "type", "integer" } } },
                { "local_file_path", { { "type", "string" } } },
                { "display_name", { { "type", "string" } } },
                { "output_path", { { "type", "string" } } }
            }),
        [&ctx, &router](const json& args, const McpRequestContext& reqCtx) -> McpToolResult {
#ifndef DOC_STUDIO_HAS_ACADEMIC
            (void)args; (void)reqCtx; (void)router;
            return McpToolResult::Err("Modulo TEC Digital deshabilitado en esta compilacion.");
#else
            const std::string action = args.value("action", "");
            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
            auto& store = ctx.EnsureTecStore();

            if (action == "status") {
                const auto snap = store.Snapshot();
                return McpToolResult::Ok({
                    { "authenticated", store.IsAuthenticated() },
                    { "synced_at_utc", snap.syncedAtUtc },
                    { "communities_count", snap.communities.size() },
                    { "evaluations_courses", snap.evaluationsByPath.size() },
                    { "rubrics_count", snap.rubricsByKey.size() },
                    { "folders_spaces", snap.foldersByPath.size() },
                    { "calendar_events", snap.unifiedCalendarEvents.size() },
                    { "radar_events", snap.activityFeed.size() },
                    { "monitor_interval_min", store.AutoMonitorIntervalMinutes() },
                    { "offline_cache_bytes", store.ComputeOfflineCacheBytes() },
                    { "warnings", snap.warnings }
                });
            }
            if (action == "restore_session") {
                const auto r = store.TryRestoreVaultSession();
                return McpToolResult::Ok({ { "ok", r.ok }, { "authenticated", store.IsAuthenticated() }, { "message", r.message } });
            }
            if (action == "login_password") {
                const std::string email = args.value("email", "");
                const std::string pass = args.value("password", "");
                if (!pass.empty()) router.Redactor().RegisterSecret(pass);
                const auto r = store.LoginWithPassword(email, pass, args.value("persist_vault", true));
                return McpToolResult::Ok({ { "ok", r.ok }, { "authenticated", store.IsAuthenticated() }, { "message", r.message } });
            }
            if (action == "login_qr") {
                const auto r = store.LoginWithQr(args.value("email", ""));
                return McpToolResult::Ok({ { "ok", r.ok }, { "authenticated", store.IsAuthenticated() }, { "message", r.message } });
            }
            if (action == "logout_local") {
                store.LogoutLocalOnly(args.value("delete_vault", false));
                return McpToolResult::Ok({ { "authenticated", false } });
            }
            if (action == "sync_all") {
                TecDigital::TecSyncOptions opts{};
                std::string err;
                const bool ok = store.SyncAll(
                    opts,
                    [&reqCtx](int pct, const std::string& msg) { reqCtx.Progress(static_cast<double>(pct), 100.0, msg); },
                    reqCtx.cancelled.get(),
                    &err);
                if (!ok) return McpToolResult::Err("Error en SyncAll: " + err);
                if (!ctx.Options().noProfileWrite) store.SaveSnapshotToDisk();
                const auto snap = store.Snapshot();
                return McpToolResult::Ok({
                    { "ok", true },
                    { "synced_at_utc", snap.syncedAtUtc },
                    { "communities", snap.communities.size() },
                    { "radar_events", snap.activityFeed.size() },
                    { "warnings", snap.warnings }
                });
            }
            if (action == "sync_course_files") {
                std::string sum, err;
                const bool ok = store.SyncCourseFiles(args.value("community_url", ""), args.value("download_files", false), &sum, &err);
                return ok ? McpToolResult::Ok({ { "ok", true }, { "summary", sum } }) : McpToolResult::Err(err);
            }
            if (action == "sync_course_on_demand") {
                std::string err;
                const bool ok = store.SyncCourseOnDemand(args.value("community_url", ""), &err);
                return ok ? McpToolResult::Ok({ { "ok", true } }) : McpToolResult::Err(err);
            }
            if (action == "sync_personal_docs") {
                std::string sum, err;
                const bool ok = store.SyncPersonalDocuments(args.value("download_files", false), &sum, &err);
                return ok ? McpToolResult::Ok({ { "ok", true }, { "summary", sum } }) : McpToolResult::Err(err);
            }
            if (action == "get_snapshot") {
                const auto snap = store.Snapshot();
                const std::string sec = LowerAscii(args.value("section", "summary"));
                if (sec == "communities") {
                    json arr = json::array();
                    for (const auto& c : snap.communities) {
                        arr.push_back({ { "id", c.community_id }, { "name", c.name }, { "type", c.type }, { "period", c.period }, { "url", c.url }, { "department", c.department } });
                    }
                    return McpToolResult::Ok({ { "count", arr.size() }, { "communities", std::move(arr) } });
                }
                if (sec == "evaluations") {
                    json out = json::object();
                    for (const auto& [url, rep] : snap.evaluationsByPath) {
                        json cats = json::array();
                        for (const auto& cat : rep.categories) {
                            json items = json::array();
                            for (const auto& it : cat.items) {
                                items.push_back({
                                    { "name", it.name }, { "assign_id", it.assign_id }, { "task_id", it.task_id },
                                    { "task_item_id", it.task_item_id }, { "due_date", it.due_date_iso.empty() ? it.due_date_text : it.due_date_iso },
                                    { "weight_pct", it.weight_pct }, { "earned_pct", it.earned_pct }, { "grade_100", it.raw_grade_100 },
                                    { "is_graded", it.is_graded }, { "has_rubric", it.has_rubric }, { "rubric_id", it.rubric_id }, { "status", it.submission_status }
                                });
                            }
                            cats.push_back({ { "name", cat.name }, { "weight_pct", cat.weight_pct }, { "earned_pct", cat.earned_pct }, { "items", std::move(items) } });
                        }
                        out[url] = { { "unrounded_total", rep.unrounded_total }, { "final_grade", rep.final_grade }, { "has_final_grade", rep.has_final_grade }, { "categories", std::move(cats) } };
                    }
                    return McpToolResult::Ok({ { "evaluations_by_course", std::move(out) } });
                }
                if (sec == "rubrics") {
                    json arr = json::array();
                    for (const auto& [rk, rb] : snap.rubricsByKey) {
                        json crits = json::array();
                        for (const auto& c : rb.criteria) {
                            crits.push_back({ { "name", c.category_name }, { "weight_pct", c.weight_pct }, { "points", c.earned_points }, { "max_points", c.max_points }, { "comment", c.comment } });
                        }
                        arr.push_back({ { "key", rk }, { "title", rb.title }, { "total_score", rb.total_score }, { "max_score", rb.max_score }, { "is_graded", rb.is_graded }, { "criteria", std::move(crits) } });
                    }
                    return McpToolResult::Ok({ { "count", arr.size() }, { "rubrics", std::move(arr) } });
                }
                if (sec == "calendar") {
                    json arr = json::array();
                    for (const auto& ev : snap.unifiedCalendarEvents) {
                        arr.push_back({ { "title", ev.title }, { "date", ev.date_iso }, { "start_time", ev.start_time }, { "end_time", ev.end_time }, { "calendar", ev.calendar_name }, { "description", ev.description } });
                    }
                    return McpToolResult::Ok({ { "count", arr.size() }, { "events", std::move(arr) } });
                }
                if (sec == "radar") {
                    json arr = json::array();
                    for (const auto& ev : snap.activityFeed) {
                        arr.push_back({ { "id", ev.id }, { "kind", static_cast<int>(ev.kind) }, { "space", ev.space_name }, { "title", ev.title }, { "detail", ev.detail }, { "detected_at_utc", ev.detected_at_utc }, { "unread", ev.unread } });
                    }
                    return McpToolResult::Ok({ { "count", arr.size() }, { "events", std::move(arr) } });
                }
                if (sec == "news") {
                    json out = json::object();
                    for (const auto& [url, ls] : snap.newsByPath) {
                        json arr = json::array();
                        for (const auto& n : ls) arr.push_back({ { "title", n.title }, { "pub_date", n.pub_date }, { "link", n.link }, { "description", n.description } });
                        out[url] = std::move(arr);
                    }
                    return McpToolResult::Ok({ { "news_by_space", std::move(out) } });
                }
                if (sec == "custom_pages") {
                    json out = json::object();
                    for (const auto& [url, pgs] : snap.customPagesBySpace) {
                        json arr = json::array();
                        for (const auto& pg : pgs) {
                            arr.push_back({ { "page_num", pg.page_num }, { "tab_label", pg.tab_label }, { "portlet_count", pg.portlets.size() }, { "excerpt", pg.plain_excerpt } });
                        }
                        out[url] = std::move(arr);
                    }
                    return McpToolResult::Ok({ { "custom_pages_by_space", std::move(out) } });
                }
                if (sec == "notifications") {
                    json arr = json::array();
                    if (snap.notifications) {
                        for (const auto& n : snap.notifications->notifications) {
                            arr.push_back({ { "id", n.notification_id }, { "title", n.title }, { "text", n.text }, { "state", n.state }, { "date", n.creation_date } });
                        }
                    }
                    return McpToolResult::Ok({ { "count", arr.size() }, { "notifications", std::move(arr) }, { "subscriptions_count", snap.notificationSubscriptions.size() } });
                }
                return McpToolResult::Ok({
                    { "synced_at_utc", snap.syncedAtUtc },
                    { "authenticated", snap.sessionAuthenticated },
                    { "communities", snap.communities.size() },
                    { "evaluations_courses", snap.evaluationsByPath.size() },
                    { "rubrics", snap.rubricsByKey.size() },
                    { "calendar_events", snap.unifiedCalendarEvents.size() },
                    { "radar_events", snap.activityFeed.size() },
                    { "folder_spaces", snap.foldersByPath.size() },
                    { "server_time_iso", snap.serverDateTimeIso },
                    { "warnings", snap.warnings }
                });
            }
            if (action == "global_file_catalog") {
                const auto snap = store.Snapshot();
                const auto all = TecDigital::TecAcademicStore::BuildGlobalFileCatalog(snap);
                const std::string q = LowerAscii(args.value("query", ""));
                const std::string stFilter = LowerAscii(args.value("space_type", ""));
                const std::size_t limit = std::clamp(args.value("max_results", std::size_t{ 100 }), std::size_t{ 1 }, std::size_t{ 500 });
                json arr = json::array();
                std::size_t matched = 0;
                for (const auto& f : all) {
                    if (!stFilter.empty() && LowerAscii(f.spaceType) != stFilter) continue;
                    if (!q.empty() && f.searchFolded.find(q) == std::string::npos) continue;
                    ++matched;
                    if (arr.size() < limit) {
                        arr.push_back({
                            { "space_name", f.spaceName }, { "space_type", f.spaceType }, { "space_url", f.spaceUrl },
                            { "folder_path", f.folderPath }, { "file_name", f.fileName }, { "title", f.title },
                            { "object_id", f.objectId }, { "mime_type", f.mimeType }, { "size_bytes", f.contentSize },
                            { "created", f.creationDateAnsi }, { "modified", f.lastModifiedAnsi },
                            { "revision", f.liveRevision }, { "author", f.authorName },
                            { "is_local", f.isLocal }, { "is_linked_in_portada", f.isLinkedInPortada }
                        });
                    }
                }
                return McpToolResult::Ok({ { "total_catalog_files", all.size() }, { "matched", matched }, { "returned", arr.size() }, { "files", std::move(arr) } });
            }
            if (action == "search_offline_fts") {
                const auto hits = store.SearchOfflineMaterials(
                    args.value("query", ""), args.value("community_url", ""), args.value("max_results", std::size_t{ 40 }));
                json arr = json::array();
                for (const auto& h : hits) {
                    arr.push_back({ { "course", h.document.course }, { "file", h.document.name }, { "path", h.document.blob_rel_path }, { "snippet", h.snippet }, { "score", h.score } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "hits", std::move(arr) } });
            }
            if (action == "fetch_space_page") {
                const auto r = store.FetchSpacePage(args.value("community_url", ""), args.value("page_num", 1), args.value("tab_label", ""));
                return r.ok ? McpToolResult::Ok({ { "page_num", r.value.page_num }, { "tab_label", r.value.tab_label }, { "portlets", r.value.portlets.size() }, { "excerpt", r.value.plain_excerpt } })
                            : McpToolResult::Err(r.error_message);
            }
            if (action == "fetch_news_article") {
                const auto r = store.FetchNewsArticle(args.value("article_url", ""));
                return r.ok ? McpToolResult::Ok({ { "title", r.value.title }, { "pub_date", r.value.pub_date }, { "body", r.value.body_text } })
                            : McpToolResult::Err(r.error_message);
            }
            if (action == "fetch_rubric") {
                const auto r = store.FetchRubric(args.value("rubric_id", ""), args.value("community_id", ""), args.value("task_item_id", ""), args.value("task_id", ""));
                return r.ok ? McpToolResult::Ok({ { "title", r.value.title }, { "total_score", r.value.total_score }, { "max_score", r.value.max_score }, { "criteria_count", r.value.criteria.size() } })
                            : McpToolResult::Err(r.error_message);
            }
            if (action == "fetch_file_details") {
                const auto r = store.FetchFileDetails(args.value("object_id", ""));
                return r.ok ? McpToolResult::Ok({ { "object_id", r.value.object_id }, { "name", r.value.name }, { "title", r.value.title }, { "mime_type", r.value.mime_type }, { "modified", r.value.last_modified_ansi }, { "revision", r.value.live_revision }, { "size_bytes", r.value.content_size } })
                            : McpToolResult::Err(r.error_message);
            }
            if (action == "fetch_file_versions") {
                const auto r = store.FetchFileVersions(args.value("object_id", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& v : r.value) arr.push_back({ { "revision", v.live_revision }, { "title", v.title }, { "author", v.author }, { "date", v.last_modified_ansi }, { "size_bytes", v.content_size } });
                return McpToolResult::Ok({ { "versions", std::move(arr) } });
            }
            if (action == "fetch_eval_history") {
                const auto r = store.FetchEvaluationHistory(args.value("community_url", ""), args.value("assign_id", ""), args.value("task_item_id", ""), args.value("task_id", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& h : r.value) arr.push_back({ { "revision_id", h.revision_id }, { "file_name", h.file_name }, { "date", h.delivered_date }, { "time", h.delivered_time }, { "is_live_revision", h.is_live_revision } });
                return McpToolResult::Ok({ { "revisions", std::move(arr) } });
            }
            if (action == "fetch_forum_thread") {
                const auto r = store.FetchForumThread(args.value("community_url", ""), args.value("message_id", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& p : r.value) arr.push_back({ { "subject", p.subject }, { "author", p.author }, { "date", p.posted_date }, { "content", p.body_text } });
                return McpToolResult::Ok({ { "posts", std::move(arr) } });
            }
            if (action == "fetch_members_csv") {
                const auto r = store.FetchCommunityMembersCsv(args.value("community_url", ""));
                return r.ok ? McpToolResult::Ok({ { "csv", r.value } }) : McpToolResult::Err(r.error_message);
            }
            if (action == "fetch_plagiarism_report_url") {
                const auto r = store.FetchPlagiarismReportUrl(args.value("doc_uid", ""));
                return r.ok ? McpToolResult::Ok({ { "url", r.value } }) : McpToolResult::Err(r.error_message);
            }
            if (action == "download_file") {
                std::string localPath, err;
                const bool ok = store.DownloadSingleCourseFile(args.value("community_url", ""), args.value("object_id", ""), &localPath, &err);
                return ok ? McpToolResult::Ok({ { "downloaded", true }, { "local_path", localPath } }) : McpToolResult::Err(err);
            }
            if (action == "cache_stats") {
                return McpToolResult::Ok({ { "cache_bytes", store.ComputeOfflineCacheBytes() }, { "budget_bytes", store.OfflineBudgetBytes() } });
            }
            if (action == "cache_prune") {
                const std::size_t pruned = store.PruneOfflineCacheOlderThanBytes(args.value("target_max_bytes", store.OfflineBudgetBytes()));
                return McpToolResult::Ok({ { "pruned_files", pruned }, { "cache_bytes", store.ComputeOfflineCacheBytes() } });
            }
            if (action == "radar_mark_read") {
                store.MarkChangeRead(args.value("event_id", ""));
                return McpToolResult::Ok({ { "marked", true } });
            }
            if (action == "radar_mark_all_read") {
                store.MarkAllChangesRead();
                return McpToolResult::Ok({ { "marked_all", true } });
            }
            if (action == "auto_monitor_configure") {
                store.ConfigureAutoMonitor(args.value("interval_minutes", 15));
                return McpToolResult::Ok({ { "interval_minutes", store.AutoMonitorIntervalMinutes() } });
            }
            if (action == "insert_course_to_canvas" || action == "save_course_cnu" || action == "generate_sm2_flashcards") {
                const auto snap = store.Snapshot();
                const std::string curl = args.value("community_url", "");
                const academic::tec_digital::CommunityInfo* found = nullptr;
                for (const auto& c : snap.communities) {
                    if (c.url == curl || LowerAscii(c.name).find(LowerAscii(curl)) != std::string::npos) { found = &c; break; }
                }
                if (!found && !snap.communities.empty()) found = &snap.communities.front();
                if (!found) return McpToolResult::Err("No hay cursos sincronizados en el snapshot TEC.");
                const auto ovIt = snap.overviewsByPath.find(found->url);
                const auto evIt = snap.evaluationsByPath.find(found->url);
                const auto* ovPtr = (ovIt != snap.overviewsByPath.end()) ? &ovIt->second : nullptr;
                const auto* evPtr = (evIt != snap.evaluationsByPath.end()) ? &evIt->second : nullptr;
                if (action == "insert_course_to_canvas") {
                    auto s = ctx.GetOrCreateSession(args);
                    const std::size_t n = TecDigital::TecAcademicStore::InsertCourseOverviewToCanvas(s->doc, *found, ovPtr, evPtr);
                    return McpToolResult::Ok({ { "inserted_objects", n }, { "course", found->name } });
                }
                if (action == "save_course_cnu") {
                    const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "tec_course", ".cudoc");
                    std::string err;
                    const bool ok = TecDigital::TecAcademicStore::SaveCourseNotebookCnu(*found, ovPtr, evPtr, outW, &err);
                    return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) }, { "course", found->name } }) : McpToolResult::Err(err);
                }
                const auto cards = TecDigital::TecAcademicStore::GenerateSm2FlashcardsFromCourse(*found, ovPtr, evPtr);
                json arr = json::array();
                for (const auto& c : cards) arr.push_back({ { "front", c.frontUtf8 }, { "back", c.backUtf8 }, { "tag", c.tagUtf8 } });
                return McpToolResult::Ok({ { "count", arr.size() }, { "cards", std::move(arr) } });
            }
            if (action == "insert_curriculum_to_canvas") {
                const auto snap = store.Snapshot();
                if (!snap.curriculumGraph) return McpToolResult::Err("Malla curricular no sincronizada.");
                auto s = ctx.GetOrCreateSession(args);
                const std::size_t n = TecDigital::TecAcademicStore::InsertCurriculumSummaryToCanvas(
                    s->doc, *snap.curriculumGraph, snap.studentProfile ? &(*snap.studentProfile) : nullptr);
                return McpToolResult::Ok({ { "inserted_objects", n } });
            }
            if (action == "import_material_to_canvas") {
                auto s = ctx.GetOrCreateSession(args);
                const bool ok = TecDigital::TecAcademicStore::ImportDownloadedMaterialToCanvas(
                    s->doc, args.value("local_file_path", ""), args.value("display_name", "Material"),
                    args.value("community_url", ""), args.value("article_url", ""));
                return ok ? McpToolResult::Ok({ { "imported", true } }) : McpToolResult::Err("No se pudo importar el material al lienzo.");
            }
            if (action == "plan_gemini_candidates") {
#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
                const auto snap = store.Snapshot();
                json out = json::array();
                for (const auto& c : snap.communities) {
                    const auto ovIt = snap.overviewsByPath.find(c.url);
                    const auto fIt = snap.foldersByPath.find(c.url);
                    const auto cands = TecDigital::TecAcademicStore::BuildGeminiNotebookCandidatesForCourse(
                        c, ovIt != snap.overviewsByPath.end() ? &ovIt->second : nullptr,
                        fIt != snap.foldersByPath.end() ? fIt->second.files : std::vector<academic::tec_digital::FileRecord>{},
                        WideToUtf8(ctx.EffectiveProfileRootW()));
                    if (!cands.empty()) out.push_back({ { "course", c.name }, { "candidate_count", cands.size() } });
                }
                return McpToolResult::Ok({ { "courses", std::move(out) } });
#else
                return McpToolResult::Err("Modulo Gemini Notebook deshabilitado.");
#endif
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_tec_academic: " + action);
#endif
        }
    });

    router.RegisterTool({
        "cuadernu_tec_horarios",
        "Buscador global de horarios TEC (guiahorarios), solucionador automatico sin choques, validacion de requisitos GraphQL y exportacion al lienzo/ICS/PDF.",
        MakeToolSchema(
            {
                "fetch_public_escuelas", "fetch_public_modalidades", "fetch_curriculum_carreras",
                "fetch_curriculum_planes", "load_curriculum_plan", "fetch_guia_carreras",
                "build_global_index", "search_global_index", "validate_requisites_graphql",
                "solve_selected_from_index"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "sede_key", { { "type", "string" } } },
                { "carrera_key", { { "type", "string" } } },
                { "plan_key", { { "type", "string" } } },
                { "year", { { "type", "integer" } } },
                { "query", { { "type", "string" } } },
                { "campus", { { "type", "string" } } },
                { "school", { { "type", "string" } } },
                { "period", { { "type", "string" } } },
                { "modality", { { "type", "string" } } },
                { "course_codes", { { "type", "array" } } },
                { "max_results", { { "type", "integer" } } },
                { "export_ics", { { "type", "boolean" } } },
                { "insert_to_canvas", { { "type", "boolean" } } }
            }),
        [&ctx](const json& args, const McpRequestContext& reqCtx) -> McpToolResult {
#ifndef DOC_STUDIO_HAS_ACADEMIC
            (void)args; (void)reqCtx;
            return McpToolResult::Err("Modulo HorariosTEC deshabilitado en esta compilacion.");
#else
            const std::string action = args.value("action", "");
            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
            auto& store = ctx.EnsureTecStore();

            if (action == "fetch_public_escuelas") {
                const auto r = store.FetchPublicGuiaEscuelas();
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& e : r.value) arr.push_back({ { "code", e.ide_depto }, { "name", e.dsc_depto } });
                return McpToolResult::Ok({ { "count", arr.size() }, { "escuelas", std::move(arr) } });
            }
            if (action == "fetch_public_modalidades") {
                const auto r = store.FetchPublicGuiaModalidades();
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& m : r.value) arr.push_back({ { "code", m.ide_modalidad }, { "name", m.nombre }, { "periods", m.cant_periodos } });
                return McpToolResult::Ok({ { "modalidades", std::move(arr) } });
            }
            if (action == "fetch_curriculum_carreras") {
                const auto r = store.FetchCurriculumCarreras(args.value("sede_key", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& c : r.value) arr.push_back({ { "key", c.key }, { "name", c.name } });
                return McpToolResult::Ok({ { "carreras", std::move(arr) } });
            }
            if (action == "fetch_curriculum_planes") {
                const auto r = store.FetchCurriculumPlanes(args.value("sede_key", ""), args.value("carrera_key", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& p : r.value) arr.push_back({ { "key", p.key }, { "name", p.name } });
                return McpToolResult::Ok({ { "planes", std::move(arr) } });
            }
            if (action == "load_curriculum_plan") {
                std::string err;
                const bool ok = store.LoadCurriculumPlan(args.value("plan_key", ""), &err);
                return ok ? McpToolResult::Ok({ { "loaded", true } }) : McpToolResult::Err(err);
            }
            if (action == "fetch_guia_carreras") {
                const auto r = store.FetchGuiaHorarioCarreras(args.value("sede_key", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& c : r.value) arr.push_back({ { "key", c.key }, { "name", c.name } });
                return McpToolResult::Ok({ { "carreras", std::move(arr) } });
            }
            if (action == "build_global_index") {
                std::string err;
                const bool ok = store.BuildGlobalScheduleIndex(
                    args.value("year", 2026),
                    [&reqCtx](int pct, const std::string& msg) { reqCtx.Progress(static_cast<double>(pct), 100.0, msg); },
                    reqCtx.cancelled.get(), &err);
                if (!ok) return McpToolResult::Err(err);
                const auto idx = store.GetGlobalScheduleIndex();
                return McpToolResult::Ok({
                    { "year", idx.year }, { "unique_courses", idx.uniqueCoursesCount },
                    { "total_groups", idx.entries.size() }, { "campuses", idx.campuses }
                });
            }
            if (action == "search_global_index") {
                TecDigital::GlobalHorariosSearchFilter flt{};
                flt.queryUtf8 = args.value("query", "");
                flt.campusFilter = args.value("campus", "");
                flt.schoolOrCareerFilter = args.value("school", "");
                flt.periodFilter = args.value("period", "");
                flt.modalityFilter = args.value("modality", "");
                flt.maxResults = std::clamp(args.value("max_results", std::size_t{ 80 }), std::size_t{ 1 }, std::size_t{ 500 });
                std::size_t total = 0;
                const auto hits = store.SearchGlobalScheduleIndex(flt, &total);
                json arr = json::array();
                for (const auto& g : hits) {
                    arr.push_back({
                        { "course_code", g.courseCode }, { "course_name", g.courseName }, { "group", g.groupId },
                        { "credits", g.credits }, { "campus", g.campusShort }, { "period", g.periodCode },
                        { "modality", g.modality }, { "professor", g.professor }, { "room", g.room },
                        { "schedule", g.scheduleSummary }
                    });
                }
                return McpToolResult::Ok({ { "total_matches", total }, { "returned", arr.size() }, { "groups", std::move(arr) } });
            }
            if (action == "validate_requisites_graphql") {
                std::vector<std::string> codes;
                if (args.contains("course_codes") && args["course_codes"].is_array()) {
                    for (const auto& c : args["course_codes"]) if (c.is_string()) codes.push_back(c.get<std::string>());
                }
                const auto r = store.ValidateScheduleCoursesWithGraphQL(codes, args.value("period", ""));
                if (!r.ok) return McpToolResult::Err(r.error_message);
                json arr = json::array();
                for (const auto& rc : r.value) {
                    arr.push_back({ { "course_code", rc.course_code }, { "can_enroll", rc.all_requirements_met && rc.all_corequirements_met }, { "missing_reqs", rc.missing_requirements }, { "missing_coreqs", rc.missing_corequirements } });
                }
                return McpToolResult::Ok({ { "results", std::move(arr) } });
            }
            if (action == "solve_selected_from_index") {
                std::vector<std::string> codes;
                if (args.contains("course_codes") && args["course_codes"].is_array()) {
                    for (const auto& c : args["course_codes"]) if (c.is_string()) codes.push_back(c.get<std::string>());
                }
                TecDigital::GlobalHorariosSearchFilter flt{};
                flt.campusFilter = args.value("campus", "");
                flt.periodFilter = args.value("period", "");
                academic::horarios_tec::SolverPreferences prefs{};
                const auto res = store.SolveSelectedCoursesFromGlobalIndex(codes, flt, prefs);
                if (!res.ok) return McpToolResult::Err(res.errorUtf8);
                json out = { { "ok", true }, { "solutions_count", res.solved.schedules.size() } };
                if (!res.solved.schedules.empty()) {
                    const auto& best = res.solved.schedules.front();
                    out["best_score"] = best.metrics.totalScore;
                    if (args.value("export_ics", false)) {
                        out["ics"] = TecDigital::TecAcademicStore::ExportRankedScheduleToIcs(best);
                    }
                    if (args.value("insert_to_canvas", false)) {
                        auto s = ctx.GetOrCreateSession(args);
                        out["canvas_objects"] = TecDigital::TecAcademicStore::InsertScheduleToCanvas(s->doc, best);
                    }
                }
                return McpToolResult::Ok(std::move(out));
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_tec_horarios: " + action);
#endif
        }
    });

    router.RegisterTool({
        "cuadernu_tec_mutation",
        "Ejecuta cualquiera de las 21 acciones estudiantiles de escritura en TEC Digital con compuerta obligatoria independiente: requiere confirm_mutation=true (soporta dry_run=true para vista previa y respeta --readonly-tec / MUTATION_MASK=0).",
        MakeToolSchema(
            {
                "upload_evaluation", "delete_evaluation_submission", "mark_notification_read",
                "delete_notification", "delete_all_notifications", "refresh_courses_dar",
                "sync_expediente_dar", "toggle_folder_subscription", "request_eval_notifications",
                "upload_plagiarism_file", "submit_plagiarism_ephorus", "update_plagiarism_visibility",
                "save_prematricula", "submit_inclusion", "delete_inclusion", "register_fair_project",
                "save_privacy_settings", "submit_consent", "create_personal_folder",
                "upload_personal_file", "delete_personal_item"
            },
            {
                { "confirm_mutation", { { "type", "boolean" }, { "description", "OBLIGATORIO true para ejecutar la escritura real en TEC Digital." } } },
                { "dry_run", { { "type", "boolean" }, { "description", "Si es true, valida parametros y politica sin ejecutar la mutacion." } } },
                { "community_url", { { "type", "string" } } },
                { "task_id", { { "type", "string" } } },
                { "task_item_id", { { "type", "string" } } },
                { "grade_id", { { "type", "string" } } },
                { "revision_id", { { "type", "string" } } },
                { "item_id", { { "type", "string" } } },
                { "local_file_path", { { "type", "string" } } },
                { "submission_url", { { "type", "string" } } },
                { "notification_id", { { "type", "string" } } },
                { "object_id", { { "type", "string" } } },
                { "subscribe", { { "type", "boolean" } } },
                { "assign_id", { { "type", "string" } } },
                { "enabled", { { "type", "boolean" } } },
                { "folder_id", { { "type", "string" } } },
                { "folder_name", { { "type", "string" } } },
                { "doc_uid", { { "type", "string" } } },
                { "visible", { { "type", "boolean" } } },
                { "pre_enroll_id", { { "type", "string" } } },
                { "curriculum_id", { { "type", "string" } } },
                { "course_codes_csv", { { "type", "string" } } },
                { "campaign_id", { { "type", "string" } } },
                { "course_code", { { "type", "string" } } },
                { "group", { { "type", "string" } } },
                { "comment", { { "type", "string" } } },
                { "request_id", { { "type", "string" } } },
                { "fair_id", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "description", { { "type", "string" } } },
                { "category_id", { { "type", "string" } } },
                { "sede_id", { { "type", "string" } } },
                { "query_params", { { "type", "string" } } },
                { "module_key", { { "type", "string" } } },
                { "accepted", { { "type", "boolean" } } },
                { "is_folder", { { "type", "boolean" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
#ifndef DOC_STUDIO_HAS_ACADEMIC
            (void)args;
            return McpToolResult::Err("Modulo TEC Digital deshabilitado.");
#else
            const std::string action = args.value("action", "");
            const bool dryRun = args.value("dry_run", false);
            const bool confirmed = args.value("confirm_mutation", false);

            std::string policyReason;
            const bool policyOk = ctx.IsTecMutationPolicyAllowed(&policyReason);

            if (dryRun) {
                return McpToolResult::Ok({
                    { "dry_run", true },
                    { "action", action },
                    { "policy_allowed", policyOk },
                    { "policy_reason", policyReason },
                    { "confirm_mutation_provided", confirmed },
                    { "would_execute", policyOk && confirmed }
                });
            }
            if (!policyOk) {
                return McpToolResult::Err("Mutacion TEC Digital bloqueada por politica: " + policyReason);
            }
            if (!confirmed) {
                return McpToolResult::Err(
                    "Bloqueo de seguridad MCP: todas las 21 acciones de escritura en TEC Digital requieren 'confirm_mutation': true (o 'dry_run': true para vista previa).");
            }

            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
            auto& store = ctx.EnsureTecStore();

            auto wrapMutRes = [](const academic::tec_digital::Result<academic::tec_digital::StudentMutationResult>& r) -> McpToolResult {
                if (!r.ok) return McpToolResult::Err(r.error_message);
                return McpToolResult::Ok({
                    { "ok", r.value.ok },
                    { "http_status", r.value.http_status },
                    { "message", r.value.message },
                    { "created_id", r.value.created_id }
                });
            };
            auto wrapBoolRes = [](const academic::tec_digital::Result<bool>& r) -> McpToolResult {
                return r.ok ? McpToolResult::Ok({ { "ok", r.value } }) : McpToolResult::Err(r.error_message);
            };

            if (action == "upload_evaluation") {
                academic::tec_digital::EvaluationUploadRequest req{};
                req.community_path = args.value("community_url", "");
                req.task_id = args.value("task_id", "");
                req.task_item_id = args.value("task_item_id", "");
                req.grade_id = args.value("grade_id", "");
                req.submission_url = args.value("submission_url", "");
                const std::string lfp = args.value("local_file_path", "");
                if (!lfp.empty()) {
                    std::vector<std::uint8_t> bytes;
                    std::string err;
                    if (!ctx.ReadBytesFromFile(Utf8ToWide(lfp), bytes, &err)) return McpToolResult::Err(err);
                    req.file_name = WideToUtf8(fs::path(Utf8ToWide(lfp)).filename().wstring());
                    req.file_bytes.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                }
                const auto r = store.UploadCourseEvaluation(req, true);
                return r.ok ? McpToolResult::Ok({ { "ok", r.value.ok }, { "revision_id", r.value.revision_id }, { "item_id", r.value.item_id } })
                            : McpToolResult::Err(r.error_message);
            }
            if (action == "delete_evaluation_submission") {
                return wrapBoolRes(store.DeleteEvaluationSubmission(args.value("community_url", ""), args.value("revision_id", ""), args.value("item_id", ""), true));
            }
            if (action == "mark_notification_read") return wrapBoolRes(store.MarkNotificationAsRead(args.value("notification_id", ""), true));
            if (action == "delete_notification") return wrapBoolRes(store.DeleteUserNotification(args.value("notification_id", ""), true));
            if (action == "delete_all_notifications") return wrapBoolRes(store.DeleteAllUserNotifications(true));
            if (action == "refresh_courses_dar") return wrapBoolRes(store.RefreshEnrolledCoursesFromDar(true));
            if (action == "sync_expediente_dar") return wrapMutRes(store.SyncExpedienteDarCourses(true));
            if (action == "toggle_folder_subscription") return wrapBoolRes(store.ToggleFolderNotificationSubscription(args.value("object_id", ""), args.value("subscribe", true)));
            if (action == "request_eval_notifications") return wrapBoolRes(store.RequestEvaluationNotifications(args.value("community_url", ""), args.value("assign_id", ""), args.value("enabled", true)));
            if (action == "upload_plagiarism_file") return wrapMutRes(store.UploadPlagiarismFile(args.value("folder_id", ""), args.value("local_file_path", "")));
            if (action == "submit_plagiarism_ephorus") return wrapMutRes(store.SubmitPlagiarismEphorus(args.value("doc_uid", ""), args.value("item_id", "")));
            if (action == "update_plagiarism_visibility") return wrapMutRes(store.UpdatePlagiarismVisibility(args.value("doc_uid", ""), args.value("visible", true)));
            if (action == "save_prematricula") return wrapMutRes(store.SavePrematriculaSelection(args.value("pre_enroll_id", ""), args.value("curriculum_id", ""), args.value("course_codes_csv", "")));
            if (action == "submit_inclusion") return wrapMutRes(store.SubmitInclusionRequest(args.value("campaign_id", ""), args.value("course_code", ""), args.value("group", ""), args.value("comment", "")));
            if (action == "delete_inclusion") return wrapMutRes(store.DeleteInclusionRequest(args.value("request_id", ""), true));
            if (action == "register_fair_project") return wrapMutRes(store.RegisterFairProject(args.value("fair_id", ""), args.value("title", ""), args.value("description", ""), args.value("category_id", ""), args.value("sede_id", "")));
            if (action == "save_privacy_settings") return wrapMutRes(store.SaveUserPrivacySettings(args.value("query_params", "")));
            if (action == "submit_consent") return wrapMutRes(store.SubmitConsentAnswer(args.value("module_key", ""), args.value("accepted", true)));
            if (action == "create_personal_folder") {
                const auto r = store.CreatePersonalFolder(args.value("folder_id", ""), args.value("folder_name", "Nueva Carpeta"));
                return r.ok ? McpToolResult::Ok({ { "folder_id", r.value } }) : McpToolResult::Err(r.error_message);
            }
            if (action == "upload_personal_file") return wrapBoolRes(store.UploadPersonalFile(args.value("folder_id", ""), args.value("local_file_path", "")));
            if (action == "delete_personal_item") return wrapBoolRes(store.DeletePersonalItem(args.value("object_id", ""), args.value("is_folder", false), true));
            return McpToolResult::Err("Accion desconocida en cuadernu_tec_mutation: " + action);
#endif
        }
    });

    // ---------------- Group H: Gemini Notebook / NotebookLM, Tutor Whiteboard & Study/AI (3 tools) ----------------

    router.RegisterTool({
        "cuadernu_gemini_notebook",
        "Suite hibrida completa de Google Gemini Notebook / NotebookLM: OAuth/gcloud con auto-renovacion, CRUD de cuadernos y fuentes (URL, YouTube, texto, archivos hasta 200 MiB), proxy MCP remoto y respaldo local de IA fundamentada.",
        MakeToolSchema(
            {
                "status", "auth_import_gcloud", "auth_refresh", "config_get", "config_set",
                "notebooks_list", "notebook_create", "notebook_get", "notebook_rename",
                "sources_list", "source_get", "source_add_url", "source_add_text", "source_upload_file",
                "mcp_connect_discover", "mcp_call_remote_tool", "query_notebook", "generate_artifact"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "project_id", { { "type", "string" } } },
                { "oauth_client_id", { { "type", "string" } } },
                { "notebook_name", { { "type", "string" } } },
                { "display_name", { { "type", "string" } } },
                { "source_name", { { "type", "string" } } },
                { "url", { { "type", "string" } } },
                { "text", { { "type", "string" } } },
                { "file_path", { { "type", "string" } } },
                { "mime_type", { { "type", "string" } } },
                { "tool_name", { { "type", "string" } } },
                { "tool_args", { { "type", "object" } } },
                { "question", { { "type", "string" } } },
                { "artifact_kind", { { "type", "string" } } }
            }),
        [&ctx, &router](const json& args, const McpRequestContext&) -> McpToolResult {
#ifndef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
            (void)args; (void)router;
            return McpToolResult::Err("Modulo Gemini Notebook deshabilitado.");
#else
            const std::string action = args.value("action", "");
            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
            auto& gn = ctx.EnsureGeminiStore();
            GeminiNotebook::GeminiNotebookRestClient rest(gn, ctx.geminiTransportOverride);
            std::string err;

            if (action == "status") {
                return McpToolResult::Ok({
                    { "enabled", gn.Config().enabled },
                    { "has_credential", gn.Tokens().HasAnyCredential() },
                    { "has_usable_token", gn.Tokens().HasUsableAccessToken(static_cast<std::int64_t>(NowEpochSec())) },
                    { "notebooks_mapped", gn.NotebookMappings().size() },
                    { "sources_tracked", gn.SyncedSources().size() },
                    { "project_id", gn.Config().projectIdUtf8 },
                    { "mcp_endpoint", gn.Config().mcpEndpointUrlUtf8 }
                });
            }
            if (action == "auth_import_gcloud") {
                std::string tok;
                if (!GeminiNotebook::GoogleOAuthPkceHelper::TryImportGcloudAdcToken(tok, &err)) return McpToolResult::Err(err);
                if (!tok.empty()) router.Redactor().RegisterSecret(tok);
                gn.MutableTokens().accessTokenUtf8 = tok;
                if (!ctx.Options().noProfileWrite) gn.SaveTokensDpapi();
                return McpToolResult::Ok({ { "imported", true }, { "has_usable_token", true } });
            }
            if (action == "auth_refresh") {
                const bool ok = rest.EnsureValidAccessToken(&err);
                return ok ? McpToolResult::Ok({ { "refreshed", true } }) : McpToolResult::Err(err);
            }
            if (action == "config_get") {
                const auto& c = gn.Config();
                return McpToolResult::Ok({
                    { "enabled", c.enabled }, { "api_base", c.apiBaseUrlUtf8 }, { "mcp_endpoint", c.mcpEndpointUrlUtf8 },
                    { "project_id", c.projectIdUtf8 }, { "has_client_id", !c.oauthClientIdUtf8.empty() },
                    { "bundle_small_notes", c.bundleSmallNotesByChapter }, { "max_uploads_per_min", c.maxUploadsPerMinute }
                });
            }
            if (action == "config_set") {
                if (args.contains("project_id")) gn.MutableConfig().projectIdUtf8 = args["project_id"].get<std::string>();
                if (args.contains("oauth_client_id")) gn.MutableConfig().oauthClientIdUtf8 = args["oauth_client_id"].get<std::string>();
                if (!ctx.Options().noProfileWrite) gn.SaveConfig();
                return McpToolResult::Ok({ { "updated", true } });
            }
            if (action == "notebooks_list") {
                std::vector<GeminiNotebook::RemoteNotebookRecord> nbs;
                if (!rest.ListNotebooks(nbs, &err)) return McpToolResult::Err(err);
                json arr = json::array();
                for (const auto& n : nbs) arr.push_back({ { "name", n.resourceNameUtf8 }, { "display_name", n.displayNameUtf8 }, { "web_url", n.webUrlUtf8 }, { "source_count", n.remoteSourceCount } });
                return McpToolResult::Ok({ { "notebooks", std::move(arr) } });
            }
            if (action == "notebook_create") {
                GeminiNotebook::RemoteNotebookRecord nb{};
                if (!rest.CreateNotebook(args.value("display_name", "Cuaderno CuadernU"), nb, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", nb.resourceNameUtf8 }, { "display_name", nb.displayNameUtf8 }, { "web_url", nb.webUrlUtf8 } });
            }
            if (action == "notebook_get") {
                GeminiNotebook::RemoteNotebookRecord nb{};
                if (!rest.GetNotebook(args.value("notebook_name", ""), nb, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", nb.resourceNameUtf8 }, { "display_name", nb.displayNameUtf8 }, { "web_url", nb.webUrlUtf8 } });
            }
            if (action == "notebook_rename") {
                GeminiNotebook::RemoteNotebookRecord nb{};
                if (!rest.RenameNotebook(args.value("notebook_name", ""), args.value("display_name", ""), nb, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", nb.resourceNameUtf8 }, { "display_name", nb.displayNameUtf8 } });
            }
            if (action == "sources_list") {
                std::vector<GeminiNotebook::RemoteSourceRecord> srcs;
                if (!rest.ListSources(args.value("notebook_name", ""), srcs, &err)) return McpToolResult::Err(err);
                json arr = json::array();
                for (const auto& sc : srcs) arr.push_back({ { "name", sc.resourceNameUtf8 }, { "display_name", sc.displayNameUtf8 }, { "mime_type", sc.mimeTypeUtf8 }, { "state", sc.stateUtf8 }, { "url", sc.sourceUrlUtf8 } });
                return McpToolResult::Ok({ { "sources", std::move(arr) } });
            }
            if (action == "source_get") {
                GeminiNotebook::RemoteSourceRecord sc{};
                if (!rest.GetSource(args.value("source_name", ""), sc, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", sc.resourceNameUtf8 }, { "display_name", sc.displayNameUtf8 }, { "state", sc.stateUtf8 } });
            }
            if (action == "source_add_url") {
                GeminiNotebook::RemoteSourceRecord sc{};
                if (!rest.CreateWebOrYouTubeSource(args.value("notebook_name", ""), args.value("url", ""), args.value("display_name", ""), sc, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", sc.resourceNameUtf8 }, { "state", sc.stateUtf8 } });
            }
            if (action == "source_add_text") {
                GeminiNotebook::RemoteSourceRecord sc{};
                if (!rest.CreateTextOrMarkdownSource(args.value("notebook_name", ""), args.value("display_name", "Nota"), args.value("text", ""), args.value("mime_type", "text/markdown"), sc, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", sc.resourceNameUtf8 }, { "state", sc.stateUtf8 } });
            }
            if (action == "source_upload_file") {
                GeminiNotebook::RemoteSourceRecord sc{};
                const std::wstring fpW = Utf8ToWide(args.value("file_path", ""));
                const std::string mime = args.value("mime_type", GeminiNotebook::DetectSupportedMimeType(fpW));
                const std::string dispName = args.value("display_name", WideToUtf8(fs::path(fpW).filename().wstring()));
                if (!rest.UploadBinaryFileSource(args.value("notebook_name", ""), dispName, fpW, mime, sc, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "name", sc.resourceNameUtf8 }, { "state", sc.stateUtf8 } });
            }
            if (action == "mcp_connect_discover" || action == "mcp_call_remote_tool") {
                rest.EnsureValidAccessToken();
                GeminiNotebook::GeminiNotebookMcpClient mcpCli(gn, ctx.geminiTransportOverride);
                std::string info;
                if (!mcpCli.Initialize(&info, &err)) return McpToolResult::Err(err);
                if (action == "mcp_connect_discover") {
                    GeminiNotebook::GeminiNotebookCapabilities caps{};
                    mcpCli.DiscoverToolsAndCapabilities(caps, &err);
                    return McpToolResult::Ok({
                        { "server_info", info },
                        { "remote_query_supported", caps.queryNotebookApi },
                        { "remote_artifacts_supported", caps.generateArtifactsApi },
                        { "discovered_tools", caps.discoveredMcpTools }
                    });
                }
                std::string resJson;
                if (!mcpCli.CallTool(args.value("tool_name", ""), args.value("tool_args", json::object()).dump(), resJson, &err)) return McpToolResult::Err(err);
                try { return McpToolResult::Ok(json::parse(resJson)); } catch (...) { return McpToolResult::Ok({ { "raw", resJson } }); }
            }
            if (action == "query_notebook") {
                std::string remoteAns;
                if (gn.Tokens().HasAnyCredential() && args.contains("notebook_name") &&
                    rest.QueryNotebookRemote(args["notebook_name"].get<std::string>(), args.value("question", ""), remoteAns, &err)) {
                    return McpToolResult::Ok({ { "source", "remote_gemini_notebook" }, { "answer", remoteAns } });
                }
                auto s = ctx.GetOrCreateSession(args);
                const auto localAns = GroundedStudyAiAssistant::AnswerQuestionWithCitations(
                    s->doc, {}, args.value("question", ""), &ctx.uiPrefs.llmConfig, &ctx.aiVault);
                return McpToolResult::Ok({ { "source", "local_grounded_fallback" }, { "answer", localAns.answerTextUtf8 }, { "citations_count", localAns.citations.size() } });
            }
            if (action == "generate_artifact") {
                std::string remoteArt;
                if (gn.Tokens().HasAnyCredential() && args.contains("notebook_name") &&
                    rest.GenerateArtifactRemote(args["notebook_name"].get<std::string>(), args.value("artifact_kind", "study_guide"), remoteArt, &err)) {
                    return McpToolResult::Ok({ { "source", "remote_gemini_notebook" }, { "artifact", remoteArt } });
                }
                auto s = ctx.GetOrCreateSession(args);
                const std::string kind = LowerAscii(args.value("artifact_kind", "summary"));
                if (kind == "quiz") {
                    const auto qs = GroundedStudyAiAssistant::GenerateMultipleChoiceQuiz(s->doc, 5);
                    json qArr = json::array();
                    for (const auto& q : qs) qArr.push_back({ { "question", q.questionUtf8 }, { "options", q.optionsUtf8 }, { "correct_index", q.correctOptionIndex } });
                    return McpToolResult::Ok({ { "source", "local_study_tools" }, { "quiz", std::move(qArr) } });
                }
                const std::string sum = GroundedStudyAiAssistant::GenerateExecutiveSummary(s->doc, {}, &ctx.uiPrefs.llmConfig, &ctx.aiVault);
                return McpToolResult::Ok({ { "source", "local_study_tools" }, { "summary", sum } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_gemini_notebook: " + action);
#endif
        }
    });

    router.RegisterTool({
        "cuadernu_tutor_whiteboard",
        "Pizarra Interactiva y Tutor Audiovisual: protocolo C++ de beats/interrupciones/rewind/provenance, horneado al documento .cudoc, gestion del sidecar Python (banco 158 problemas Fisica 3, auditoria PDF, verificador simbolico) y panel visual WebView2.",
        MakeToolSchema(
            {
                "sidecar_start", "sidecar_stop", "sidecar_health", "lessons_list", "pdf_audit",
                "verify_step", "memory_summary", "telemetry_summary", "banco_list", "banco_search",
                "session_apply_beat", "session_interrupt", "session_rewind", "session_origin_chain",
                "session_export_json", "bake_to_document",
                "panel_open_background", "panel_capture_png", "panel_close"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "beat_json", { { "type", "string" } } },
                { "beat_id", { { "type", "string" } } },
                { "checkpoint_id", { { "type", "string" } } },
                { "target_ref", { { "type", "string" } } },
                { "query", { { "type", "string" } } },
                { "expr", { { "type", "string" } } },
                { "expected", { { "type", "string" } } },
                { "pdf_path", { { "type", "string" } } },
                { "doc_id", { { "type", "string" } } },
                { "wait_ready_ms", { { "type", "integer" } } },
                { "output_path", { { "type", "string" } } }
            }),
        [&ctx, &router](const json& args, const McpRequestContext&) -> McpToolResult {
            const std::string action = args.value("action", "");
            if (action == "sidecar_start") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                auto cfg = Tutor::TutorSidecarBridge::DiscoverDefaultConfig();
                std::string err;
                const bool ok = ctx.tutorSidecar.Start(cfg, &ctx.aiVault, &err);
                const auto ep = ctx.tutorSidecar.Endpoint();
                if (ok && !ep.sessionTokenHex.empty()) router.Redactor().RegisterSecret(ep.sessionTokenHex);
                return ok ? McpToolResult::Ok({ { "running", true }, { "port", ep.port }, { "base_url", ep.baseHttpUrl } })
                          : McpToolResult::Err("No se pudo iniciar sidecar Tutor: " + err);
            }
            if (action == "sidecar_stop") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                ctx.tutorSidecar.Stop();
                return McpToolResult::Ok({ { "running", false } });
            }
            if (action == "sidecar_health") {
                const auto ep = ctx.tutorSidecar.Endpoint();
                if (!ep.running) return McpToolResult::Ok({ { "running", false } });
                return TutorSidecarHttpCall(ctx, "GET", "/api/health");
            }
            if (action == "lessons_list") return TutorSidecarHttpCall(ctx, "GET", "/api/lessons");
            if (action == "memory_summary") return TutorSidecarHttpCall(ctx, "GET", "/api/memory/summary");
            if (action == "telemetry_summary") return TutorSidecarHttpCall(ctx, "GET", "/api/telemetry/summary");
            if (action == "banco_list") return TutorSidecarHttpCall(ctx, "GET", "/api/banco");
            if (action == "banco_search") return TutorSidecarHttpCall(ctx, "GET", "/api/banco/buscar?q=" + UrlEncodeComponent(args.value("query", "")));
            if (action == "pdf_audit") {
                const std::string docId = args.value("doc_id", "");
                if (!docId.empty()) {
                    return TutorSidecarHttpCall(ctx, "GET", "/api/pdf/audit/" + UrlEncodeComponent(docId));
                }
                const std::string pdfPath = args.value("pdf_path", "");
                if (pdfPath.empty()) return McpToolResult::Err("Se requiere 'pdf_path' o 'doc_id' para pdf_audit.");
                return TutorSidecarHttpCall(ctx, "POST", "/api/pdf/open_local", json{ { "path", pdfPath } }.dump());
            }
            if (action == "verify_step") {
                return TutorSidecarHttpCall(ctx, "POST", "/api/verify/step",
                    json{ { "student_expr", args.value("expr", "") }, { "expected_expr", args.value("expected", "") } }.dump());
            }
            if (action == "panel_open_background" || action == "panel_capture_png" || action == "panel_close") {
                if (!ctx.guiManager.HasActiveConnection()) {
                    return McpToolResult::Err("Inicie una instancia GUI en segundo plano con cuadernu_gui (spawn_background) primero.");
                }
                json req;
                std::wstring outW;
                if (action == "panel_open_background") {
                    req = { { "action", "tutor_panel_open_background" } };
                } else if (action == "panel_close") {
                    req = { { "action", "tutor_panel_close" }, { "close", true } };
                } else {
                    outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "tutor_panel", ".png");
                    const int waitReadyMs = args.value("wait_ready_ms", 45000);
                    req = { { "action", "tutor_panel_png" }, { "out_path", WideToUtf8(outW) }, { "wait_ready_ms", waitReadyMs } };
                }
                json resp;
                std::string err;
                const int waitReady = req.value("wait_ready_ms", 0);
                const int reqTimeoutMs = (action == "panel_capture_png")
                    ? std::clamp(waitReady + 30000, 65000, 125000)
                    : 25000;
                const bool ok = ctx.guiManager.SendRequest(req, resp, "", reqTimeoutMs, &err);
                if (!ok) return McpToolResult::Err(err);
                if (action == "panel_capture_png" && resp.value("ok", false)) {
                    std::vector<std::uint8_t> bytes;
                    if (ctx.ReadBytesFromFile(outW, bytes) && !bytes.empty()) {
                        return McpToolResult::OkWithImage(std::move(resp), std::move(bytes));
                    }
                }
                return McpToolResult::Ok(std::move(resp));
            }

            auto s = ctx.GetOrCreateSession(args);
            if (action == "session_apply_beat") {
                Tutor::TutorBeat beat{};
                Tutor::TutorValidationResult val{};
                if (!Tutor::ParseAndValidateTutorBeat(args.value("beat_json", "{}"), beat, val) || !val.ok) {
                    return McpToolResult::Err("Beat invalido.", { { "errors", val.errors } });
                }
                std::string err;
                const bool ok = s->tutorSession.ApplyBeat(beat, &err);
                return ok ? McpToolResult::Ok({ { "applied", true }, { "beat_id", beat.beatId }, { "scene_size", s->tutorSession.SceneEntries().size() } })
                          : McpToolResult::Err(err);
            }
            if (action == "session_interrupt") {
                Tutor::TutorInterruptSnapshot snap{};
                snap.beatId = args.value("beat_id", "");
                snap.activeCheckpointId = args.value("checkpoint_id", "");
                const std::string res = s->tutorSession.CommitInterruption(snap);
                return McpToolResult::Ok({ { "interrupted", true }, { "summary", res } });
            }
            if (action == "session_rewind") {
                const bool ok = s->tutorSession.RewindToCheckpoint(args.value("checkpoint_id", ""));
                return ok ? McpToolResult::Ok({ { "rewound", true } }) : McpToolResult::Err("Checkpoint no encontrado.");
            }
            if (action == "session_origin_chain") {
                const auto edges = s->tutorSession.FindOriginChain(args.value("target_ref", ""), 6);
                json arr = json::array();
                for (const auto& e : edges) arr.push_back({ { "target", e.targetRef }, { "source", e.sourceRef }, { "rule", e.ruleEs } });
                return McpToolResult::Ok({ { "edges", std::move(arr) } });
            }
            if (action == "session_export_json") {
                const std::string raw = s->tutorSession.ExportStateJson();
                try { return McpToolResult::Ok(json::parse(raw)); } catch (...) { return McpToolResult::Ok({ { "raw", raw } }); }
            }
            if (action == "bake_to_document") {
                const ObjectTarget tgt = TargetFromArgs(args, s->doc);
                const auto rep = Tutor::BakeTutorSessionToDocument(s->doc, s->cmdStack, tgt, s->tutorSession);
                return McpToolResult::Ok({
                    { "ok", rep.ok }, { "baked_objects", rep.bakedObjectsCount },
                    { "math_blocks", rep.mathBlocksCount }, { "text_blocks", rep.textBlocksCount },
                    { "plot_blocks", rep.plotBlocksCount }, { "summary", rep.summaryUtf8 }
                });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_tutor_whiteboard: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_study_ai",
        "Boveda multi-proveedor IA (sin exponer claves), preguntas fundamentadas con citas, resumenes, quizzes, traduccion, tutor socratico, flashcards SM-2, mapas conceptuales, formulario/glosario, Pomodoro, extractor de tareas/ICS y citas APA7/IEEE/BibTeX.",
        MakeToolSchema(
            {
                "ai_vault_list_providers", "ai_vault_set_active", "grounded_qa", "summarize",
                "quiz_generate", "translate", "socratic_tutor_step", "flashcards_generate",
                "flashcards_review_sm2", "flashcards_due_count", "flashcards_build_from_doc",
                "concept_map_generate", "formula_glossary_extract", "pomodoro_control",
                "extract_tasks_to_db_ics", "post_lecture_pipeline", "citations_format"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "provider_id", { { "type", "string" } } },
                { "question", { { "type", "string" } } },
                { "text", { { "type", "string" } } },
                { "target_lang", { { "type", "string" } } },
                { "max_questions", { { "type", "integer" } } },
                { "max_cards", { { "type", "integer" } } },
                { "hint_level", { { "type", "integer" } } },
                { "card_index", { { "type", "integer" } } },
                { "quality", { { "type", "integer" } } },
                { "topic", { { "type", "string" } } },
                { "pomodoro_op", { { "type", "string" }, { "description", "status|configure|start|pause|reset" } } },
                { "focus_min", { { "type", "integer" } } },
                { "break_min", { { "type", "integer" } } },
                { "style", { { "type", "string" }, { "enum", json::array({ "apa7", "ieee", "bibtex" }) } } },
                { "authors", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "year", { { "type", "string" } } },
                { "venue", { { "type", "string" } } },
                { "url", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            ctx.EnsureProfileLoaded();
            const std::string action = args.value("action", "");

            if (action == "ai_vault_list_providers") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                json arr = json::array();
                for (const auto& p : ctx.aiVault.MutableProviders()) {
                    arr.push_back({
                        { "id", p.idUtf8 }, { "name", p.displayNameUtf8 }, { "model", p.modelNameUtf8 },
                        { "enabled", p.enabled }, { "has_api_key", !p.apiKeyUtf8.empty() }
                    });
                }
                return McpToolResult::Ok({ { "enabled", ctx.aiVault.IsEnabled() }, { "providers", std::move(arr) } });
            }
            if (action == "ai_vault_set_active") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                const bool ok = ctx.aiVault.SetActiveProviderId(args.value("provider_id", ""));
                return ok ? McpToolResult::Ok({ { "updated", true } }) : McpToolResult::Err("Proveedor no encontrado.");
            }
            if (action == "pomodoro_control") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                const std::string op = LowerAscii(args.value("pomodoro_op", "status"));
                const std::uint64_t nowMs = NowEpochSec() * 1000ull;
                if (op == "configure") ctx.pomodoro.Configure(args.value("focus_min", 25), args.value("break_min", 5));
                else if (op == "start") ctx.pomodoro.Start(nowMs);
                else if (op == "pause") ctx.pomodoro.TogglePause(nowMs);
                else if (op == "reset") ctx.pomodoro.Reset();
                return McpToolResult::Ok({
                    { "running", ctx.pomodoro.running }, { "paused", ctx.pomodoro.paused },
                    { "remaining_sec", ctx.pomodoro.remainingSeconds }, { "completed_cycles", ctx.pomodoro.completedCycles },
                    { "badge", ctx.pomodoro.FormatHudBadgeUtf8() }
                });
            }
            if (action == "citations_format") {
                AcademicSourceInfo src{};
                src.authorsUtf8 = args.value("authors", "Autor");
                src.titleUtf8 = args.value("title", "Titulo");
                src.yearUtf8 = args.value("year", "2026");
                src.venueOrPublisherUtf8 = args.value("venue", "ITCR");
                src.urlUtf8 = args.value("url", "");
                const std::string st = LowerAscii(args.value("style", "apa7"));
                const CitationStyle cs = (st == "ieee") ? CitationStyle::Ieee : ((st == "bibtex") ? CitationStyle::BibTeX : CitationStyle::Apa7);
                return McpToolResult::Ok({ { "style", st }, { "formatted", StudyAssistantTools::FormatAcademicCitation(src, cs, 1) } });
            }

            auto s = ctx.GetOrCreateSession(args);
            const ObjectTarget tgt = TargetFromArgs(args, s->doc);

            if (action == "grounded_qa") {
                const auto ans = GroundedStudyAiAssistant::AnswerQuestionWithCitations(s->doc, {}, args.value("question", ""), &ctx.uiPrefs.llmConfig, &ctx.aiVault);
                json cits = json::array();
                for (const auto& c : ans.citations) cits.push_back({ { "doc", c.docTitleUtf8 }, { "page", c.pageIndex0 }, { "quote", c.quoteSnippetUtf8 } });
                return McpToolResult::Ok({ { "answered", ans.answered }, { "provider", ans.providerUsedUtf8 }, { "answer", ans.answerTextUtf8 }, { "citations", std::move(cits) } });
            }
            if (action == "summarize") {
                return McpToolResult::Ok({ { "summary", GroundedStudyAiAssistant::GenerateExecutiveSummary(s->doc, {}, &ctx.uiPrefs.llmConfig, &ctx.aiVault) } });
            }
            if (action == "quiz_generate") {
                const auto qs = GroundedStudyAiAssistant::GenerateMultipleChoiceQuiz(s->doc, args.value("max_questions", std::size_t{ 5 }));
                json arr = json::array();
                for (const auto& q : qs) arr.push_back({ { "question", q.questionUtf8 }, { "options", q.optionsUtf8 }, { "correct_index", q.correctOptionIndex } });
                return McpToolResult::Ok({ { "count", arr.size() }, { "questions", std::move(arr) } });
            }
            if (action == "translate") {
                return McpToolResult::Ok({ { "translated", GroundedStudyAiAssistant::TranslateSelectionOffline(args.value("text", ""), args.value("target_lang", "en"), &ctx.uiPrefs.llmConfig, &ctx.aiVault) } });
            }
            if (action == "socratic_tutor_step") {
                const int lv = std::clamp(args.value("hint_level", 1), 1, 3);
                const auto res = StudyAssistantTools::RunGuidedTutor(s->doc, args.value("question", ""), static_cast<TutorHintLevel>(lv), true, &ctx.aiVault);
                return McpToolResult::Ok({ { "ok", res.ok }, { "provider", res.providerUsedUtf8 }, { "markdown", res.responseMarkdownUtf8 } });
            }
            if (action == "flashcards_generate" || action == "flashcards_build_from_doc") {
                if (action == "flashcards_build_from_doc") {
                    s->studyDeck = StudySetManager::BuildDeckFromDocument(s->doc, s->doc.Title());
                } else {
                    s->studyDeck.cards = StudyAssistantTools::GenerateSmartFlashcards(s->doc, args.value("max_cards", std::size_t{ 8 }), &ctx.aiVault);
                }
                json arr = json::array();
                for (const auto& c : s->studyDeck.cards) arr.push_back({ { "id", c.id }, { "question", c.questionUtf8 }, { "answer", c.answerUtf8 }, { "interval_days", c.intervalDays } });
                return McpToolResult::Ok({ { "count", arr.size() }, { "cards", std::move(arr) } });
            }
            if (action == "flashcards_review_sm2") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("card_index", 0)));
                if (idx >= s->studyDeck.cards.size()) return McpToolResult::Err("Tarjeta fuera de rango.");
                StudySetManager::ReviewCardSm2(s->studyDeck.cards[idx], std::clamp(args.value("quality", 4), 0, 5), NowEpochSec());
                const auto& c = s->studyDeck.cards[idx];
                return McpToolResult::Ok({ { "repetitions", c.repetitions }, { "interval_days", c.intervalDays }, { "ease_factor", c.easeFactor } });
            }
            if (action == "flashcards_due_count") {
                return McpToolResult::Ok({ { "due_count", StudySetManager::CountDueCards(s->studyDeck, NowEpochSec()) }, { "total_cards", s->studyDeck.cards.size() } });
            }
            if (action == "concept_map_generate") {
                const auto res = StudyAssistantTools::GenerateConceptMapInDocument(s->doc, s->cmdStack, tgt, args.value("topic", s->doc.Title()), &ctx.aiVault);
                return McpToolResult::Ok({ { "ok", res.ok }, { "node_count", res.nodeCount }, { "connector_count", res.connectorCount }, { "summary", res.summaryUtf8 } });
            }
            if (action == "formula_glossary_extract") {
                const auto res = StudyAssistantTools::BuildFormulaSheetAndGlossary(s->doc, &ctx.knowledgeVault);
                return McpToolResult::Ok({ { "formula_count", res.formulaCount }, { "glossary_count", res.glossaryCount }, { "markdown", res.markdownUtf8 } });
            }
            if (action == "extract_tasks_to_db_ics") {
                const auto rep = StudyAssistantTools::ExtractTasksAndDeadlines(s->doc, &ctx.knowledgeVault, &s->database);
                json arr = json::array();
                for (const auto& t : rep.extracted) arr.push_back({ { "title", t.titleUtf8 }, { "due_date", t.dueDateIsoUtf8 }, { "category", t.categoryUtf8 } });
                return McpToolResult::Ok({ { "inserted_into_db", rep.insertedIntoDatabaseCount }, { "tasks", std::move(arr) }, { "ics", rep.icsCalendarUtf8 } });
            }
            if (action == "post_lecture_pipeline") {
                const auto rep = StudyAssistantTools::RunPostLectureAutomation(s->doc, s->cmdStack, tgt, s->audioSession, s->database, &ctx.aiVault);
                return McpToolResult::Ok({ { "ok", rep.ok }, { "segments", rep.transcriptSegments }, { "tasks_inserted", rep.tasksInserted }, { "card_id", rep.insertedCardObjId }, { "summary", rep.summaryMarkdownUtf8 } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_study_ai: " + action);
        }
    });

    // ---------------- Group I.1: Audio, TTS, Accessibility, Keymap & Themes (1 tool) ----------------

    router.RegisterTool({
        "cuadernu_audio_a11y",
        "Audio sincronizado (WAV, grabacion, marcadores, transcripcion aislada por PID, diarizacion, reduccion de ruido, actas ICS), sintesis de voz TTS, accesibilidad (regla de lectura, dislexia, tintes, Okabe-Ito), atajos de teclado y temas.",
        MakeToolSchema(
            {
                "audio_import_wav", "audio_export_wav", "audio_bookmark_add", "audio_transcribe",
                "audio_diarize", "audio_noise_reduce", "audio_search_seek", "audio_minutes_ics",
                "tts_list_voices", "tts_speak", "tts_stop", "a11y_get_prefs", "a11y_set_prefs",
                "keymap_get", "keymap_set", "theme_list", "theme_set"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "path", { { "type", "string" } } },
                { "label", { { "type", "string" } } },
                { "timestamp_ms", { { "type", "integer" } } },
                { "query", { { "type", "string" } } },
                { "ics_content", { { "type", "string" } } },
                { "text", { { "type", "string" } } },
                { "voice", { { "type", "string" } } },
                { "reading_ruler", { { "type", "boolean" } } },
                { "dyslexia_mode", { { "type", "boolean" } } },
                { "ui_font_scale_pct", { { "type", "integer" } } },
                { "action_key", { { "type", "string" } } },
                { "shortcut", { { "type", "string" } } },
                { "theme_id", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            ctx.EnsureProfileLoaded();
            const std::string action = args.value("action", "");

            if (action == "tts_list_voices") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                ctx.speechSynth.Initialize();
                return McpToolResult::Ok({ { "voices", ctx.speechSynth.EnumerateInstalledVoices() } });
            }
            if (action == "tts_speak") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                std::string err;
                ctx.speechSynth.Initialize(&err);
                if (args.contains("voice") && !args["voice"].get<std::string>().empty()) {
                    ctx.speechSynth.SelectVoiceByName(args["voice"].get<std::string>());
                }
                const bool ok = ctx.speechSynth.SpeakAsync(args.value("text", ""), &err);
                return ok ? McpToolResult::Ok({ { "speaking", true } }) : McpToolResult::Err(err);
            }
            if (action == "tts_stop") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                ctx.speechSynth.Stop();
                return McpToolResult::Ok({ { "speaking", false } });
            }
            if (action == "a11y_get_prefs") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                const auto& a = ctx.uiPrefs.accessibility;
                return McpToolResult::Ok({
                    { "reading_ruler", a.readingRulerEnabled },
                    { "dyslexia_mode", a.dyslexiaFriendlyMode },
                    { "ui_font_scale_pct", a.uiFontScalePct },
                    { "enhanced_focus_ring", a.enhancedFocusRing },
                    { "tts_rate", a.ttsRate },
                    { "tts_volume", a.ttsVolume },
                    { "theme_id", ctx.uiPrefs.themeProfileIdUtf8 }
                });
            }
            if (action == "a11y_set_prefs") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                auto& a = ctx.uiPrefs.accessibility;
                if (args.contains("reading_ruler")) a.readingRulerEnabled = args["reading_ruler"].get<bool>();
                if (args.contains("dyslexia_mode")) a.dyslexiaFriendlyMode = args["dyslexia_mode"].get<bool>();
                if (args.contains("ui_font_scale_pct")) a.uiFontScalePct = std::clamp(args["ui_font_scale_pct"].get<int>(), 100, 150);
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "updated", true } });
            }
            if (action == "keymap_get") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                return McpToolResult::Ok({ { "shortcuts", ctx.keymap.actionToShortcut }, { "pen_stabilization", ctx.keymap.penStabilization } });
            }
            if (action == "keymap_set") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                const std::string k = args.value("action_key", "");
                if (k.empty()) return McpToolResult::Err("Se requiere 'action_key'.");
                ctx.keymap.actionToShortcut[k] = args.value("shortcut", "");
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "updated", true }, { "action_key", k } });
            }
            if (action == "theme_list") {
                json arr = json::array();
                for (const auto& t : ThemeProfileRegistry::BuiltInProfiles()) {
                    arr.push_back({ { "id", t.idUtf8 }, { "name", t.displayNameUtf8 }, { "dark", t.dark }, { "high_contrast", t.highContrast } });
                }
                return McpToolResult::Ok({ { "active_theme", ctx.uiPrefs.themeProfileIdUtf8 }, { "themes", std::move(arr) } });
            }
            if (action == "theme_set") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                ctx.uiPrefs.themeProfileIdUtf8 = args.value("theme_id", "porcelana-claro");
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "theme_id", ctx.uiPrefs.themeProfileIdUtf8 } });
            }

            auto s = ctx.GetOrCreateSession(args);
            std::string err;
            if (action == "audio_import_wav") {
                const bool ok = s->audioSession.LoadFromWavFile(args.value("path", ""), &err);
                return ok ? McpToolResult::Ok({ { "duration_ms", s->audioSession.DurationMs() }, { "sample_rate", s->audioSession.SampleRateHz() } }) : McpToolResult::Err(err);
            }
            if (action == "audio_export_wav") {
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("path", ""), s->sessionId + "_audio", ".wav");
                const bool ok = s->audioSession.ExportWavFile(WideToUtf8(outW), &err);
                return ok ? McpToolResult::Ok({ { "path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "audio_bookmark_add") {
                s->audioSession.AddBookmark(args.value("label", "Marcador"), args.value("timestamp_ms", std::uint64_t{ 0 }));
                return McpToolResult::Ok({ { "added", true } });
            }
            if (action == "audio_transcribe") {
                const auto segs = s->audioSession.TranscribeOffline();
                json arr = json::array();
                for (const auto& sg : segs) arr.push_back({ { "start_ms", sg.startMs }, { "end_ms", sg.endMs }, { "text", sg.textUtf8 } });
                return McpToolResult::Ok({ { "count", arr.size() }, { "segments", std::move(arr) } });
            }
            if (action == "audio_diarize") {
                const auto segs = s->audioSession.DiarizeMultiSpeakerTranscript();
                json arr = json::array();
                for (const auto& sg : segs) arr.push_back({ { "speaker", sg.speakerIdUtf8 }, { "start_ms", sg.startMs }, { "end_ms", sg.endMs }, { "text", sg.textUtf8 } });
                return McpToolResult::Ok({ { "segments", std::move(arr) } });
            }
            if (action == "audio_noise_reduce") {
                s->audioSession.ApplyNoiseReductionHighPass();
                return McpToolResult::Ok({ { "applied", true } });
            }
            if (action == "audio_search_seek") {
                const auto segs = s->audioSession.DiarizeMultiSpeakerTranscript();
                std::uint64_t ts = 0;
                const bool found = s->audioSession.SearchTranscriptAndSeek(args.value("query", ""), segs, &ts);
                return McpToolResult::Ok({ { "found", found }, { "timestamp_ms", ts } });
            }
            if (action == "audio_minutes_ics") {
                const auto segs = s->audioSession.DiarizeMultiSpeakerTranscript();
                const auto rep = AudioTimelineSession::GenerateCalendarMeetingNotesFromIcs(args.value("ics_content", ""), segs);
                return McpToolResult::Ok({ { "valid", rep.valid }, { "title", rep.eventTitleUtf8 }, { "markdown", rep.combinedMarkdownUtf8 } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_audio_a11y: " + action);
        }
    });
}

} // namespace DocStudio::Mcp
