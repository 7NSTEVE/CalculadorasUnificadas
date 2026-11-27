#include "mcp_server.hpp"

#include "../core/utf_conv.hpp"
#include "../interop/universal_formats_clipper.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace DocStudio::Mcp {

namespace {

std::string LowerAscii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

json SerializeLibraryItem(const LibraryItemRecord& it) {
    return {
        { "item_id", it.itemId },
        { "folder_id", it.folderId },
        { "title", it.titleUtf8 },
        { "kind", it.kindUtf8 },
        { "default_paper", PaperPatternToString(it.defaultPaper) },
        { "favorite", it.favorite },
        { "pinned", it.pinned },
        { "in_trash", it.inTrash },
        { "priority_rank", it.priorityRank },
        { "updated_epoch_sec", it.updatedEpochSec },
        { "tags", it.tagsUtf8 },
        { "photo_count", it.photoCount },
        { "recording_count", it.recordingCount },
        { "bookmark_count", it.bookmarkCount },
        { "attachment_count", it.attachmentCount },
        { "word_count", it.wordCount },
        { "summary", it.bodySummaryUtf8 },
        { "file_path", it.filePathUtf8 }
    };
}

NotePropType ParseNotePropType(const std::string& s) {
    const std::string k = LowerAscii(s);
    if (k == "number") return NotePropType::Number;
    if (k == "date") return NotePropType::Date;
    if (k == "checkbox" || k == "bool") return NotePropType::Checkbox;
    if (k == "list") return NotePropType::List;
    return NotePropType::Text;
}

json SerializeNote(const VaultNoteRecord& n, bool includeBody = true) {
    json props = json::array();
    for (const auto& p : n.properties) {
        props.push_back({ { "key", p.keyUtf8 }, { "value", p.valueUtf8 }, { "list_values", p.listValuesUtf8 } });
    }
    json j = {
        { "note_id", n.noteIdUtf8 },
        { "title", n.titleUtf8 },
        { "folder", n.folderPathUtf8 },
        { "aliases", n.aliasesUtf8 },
        { "tags", n.tagsUtf8 },
        { "properties", std::move(props) },
        { "is_daily_note", n.isDailyNote },
        { "daily_date", n.dailyDateIsoUtf8 },
        { "in_trash", n.inTrash },
        { "paper", PaperPatternToString(n.paperPattern) },
        { "updated_epoch_sec", n.updatedEpochSec }
    };
    if (includeBody) j["body_markdown"] = n.bodyMarkdownUtf8;
    return j;
}

DbColumnType ParseDbColType(const std::string& s) {
    const std::string k = LowerAscii(s);
    if (k == "number") return DbColumnType::Number;
    if (k == "date") return DbColumnType::Date;
    if (k == "multiselect" || k == "multi_select") return DbColumnType::MultiSelect;
    if (k == "status" || k == "select") return DbColumnType::Status;
    if (k == "person") return DbColumnType::Person;
    if (k == "checkbox" || k == "bool") return DbColumnType::Checkbox;
    if (k == "file" || k == "file_media") return DbColumnType::FileMedia;
    if (k == "relation") return DbColumnType::Relation;
    if (k == "rollup") return DbColumnType::Rollup;
    if (k == "formula") return DbColumnType::Formula;
    return DbColumnType::Text;
}

DbViewKind ParseDbViewKind(const std::string& s) {
    const std::string k = LowerAscii(s);
    if (k == "list") return DbViewKind::List;
    if (k == "gallery" || k == "cards") return DbViewKind::CardsGallery;
    if (k == "kanban") return DbViewKind::Kanban;
    if (k == "calendar") return DbViewKind::Calendar;
    if (k == "timeline") return DbViewKind::Timeline;
    if (k == "chart") return DbViewKind::Chart;
    if (k == "dashboard") return DbViewKind::Dashboard;
    if (k == "map") return DbViewKind::Map;
    return DbViewKind::Table;
}

json SerializeDbRecord(const DbRecord& r) {
    json cells = json::object();
    for (const auto& [cid, val] : r.cells) {
        cells[cid] = {
            { "text", val.textUtf8 },
            { "number", val.numberVal },
            { "bool", val.boolVal },
            { "list", val.listUtf8 }
        };
    }
    return {
        { "record_id", r.recordIdUtf8 },
        { "title", r.titleUtf8 },
        { "linked_note_id", r.linkedMarkdownNoteIdUtf8 },
        { "body_markdown", r.bodyMarkdownUtf8 },
        { "cells", std::move(cells) }
    };
}

CollaborationRole ParseCollabRole(const std::string& s) {
    const std::string k = LowerAscii(s);
    if (k == "owner" || k == "propietario") return CollaborationRole::Owner;
    if (k == "editor") return CollaborationRole::Editor;
    if (k == "commenter" || k == "comentarista") return CollaborationRole::Commenter;
    return CollaborationRole::Viewer;
}

std::string CollabRoleToString(CollaborationRole r) {
    switch (r) {
    case CollaborationRole::Owner: return "owner";
    case CollaborationRole::Editor: return "editor";
    case CollaborationRole::Commenter: return "commenter";
    case CollaborationRole::Viewer: return "viewer";
    }
    return "viewer";
}

} // namespace

void RegisterKnowledgeAndSecurityTools(McpProtocolRouter& router, McpServerContext& ctx) {
    // ---------------- Group D: Library, Knowledge Vault & Structured Database (3 tools) ----------------

    router.RegisterTool({
        "cuadernu_library",
        "Organiza, ordena, etiqueta, agrupa, busca y gestiona cuadernos, carpetas, vistas de contenido, carpetas inteligentes y la estructura jerarquica de NotebookVault (Cuaderno -> Seccion -> Pagina).",
        MakeToolSchema(
            {
                "list_folders", "create_folder", "list_items", "add_item", "rename_item", "move_item",
                "sort_items", "favorite", "pin", "trash", "restore", "duplicate", "merge",
                "tag_assign", "tag_rename", "content_view", "smart_folder_add", "smart_folder_eval",
                "vault_notebook_create", "vault_section_create", "vault_page_add", "vault_page_move", "vault_list"
            },
            {
                { "name", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "kind", { { "type", "string" } } },
                { "parent_folder_id", { { "type", "integer" } } },
                { "folder_id", { { "type", "integer" } } },
                { "item_id", { { "type", "integer" } } },
                { "secondary_item_id", { { "type", "integer" } } },
                { "priority", { { "type", "integer" } } },
                { "sort_by", { { "type", "string" }, { "enum", json::array({ "priority", "recent", "title", "kind" }) } } },
                { "ascending", { { "type", "boolean" } } },
                { "enabled", { { "type", "boolean" } } },
                { "tag", { { "type", "string" } } },
                { "old_tag", { { "type", "string" } } },
                { "new_tag", { { "type", "string" } } },
                { "view", { { "type", "string" }, { "description", "photos|recordings|bookmarks|attachments|favorites|recent|trash|kind|tag" } } },
                { "query", { { "type", "string" } } },
                { "file_path", { { "type", "string" } } },
                { "notebook_id", { { "type", "integer" } } },
                { "section_id", { { "type", "integer" } } },
                { "page_entry_id", { { "type", "integer" } } },
                { "dst_notebook_id", { { "type", "integer" } } },
                { "dst_section_id", { { "type", "integer" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            ctx.EnsureProfileLoaded();
            const std::string action = args.value("action", "");
            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);

            if (action == "list_folders") {
                json arr = json::array();
                for (const auto& f : ctx.library.Folders()) {
                    arr.push_back({ { "folder_id", f.folderId }, { "parent_folder_id", f.parentFolderId }, { "name", f.nameUtf8 }, { "priority_order", f.priorityOrder } });
                }
                return McpToolResult::Ok({ { "folders", std::move(arr) } });
            }
            if (action == "create_folder") {
                const std::uint64_t fid = ctx.library.CreateFolder(
                    args.value("name", "Carpeta"),
                    args.value("parent_folder_id", std::uint64_t{ 0 }),
                    args.value("priority", 0));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "folder_id", fid } });
            }
            if (action == "list_items" || action == "sort_items") {
                auto items = ctx.library.Items();
                const std::string sortBy = LowerAscii(args.value("sort_by", "recent"));
                const bool asc = args.value("ascending", sortBy == "title" || sortBy == "kind");
                std::sort(items.begin(), items.end(), [&](const LibraryItemRecord& a, const LibraryItemRecord& b) {
                    if (sortBy == "priority") return asc ? (a.priorityRank < b.priorityRank) : (a.priorityRank > b.priorityRank);
                    if (sortBy == "title") return asc ? (LowerAscii(a.titleUtf8) < LowerAscii(b.titleUtf8)) : (LowerAscii(a.titleUtf8) > LowerAscii(b.titleUtf8));
                    if (sortBy == "kind") return asc ? (a.kindUtf8 < b.kindUtf8) : (a.kindUtf8 > b.kindUtf8);
                    return asc ? (a.updatedEpochSec < b.updatedEpochSec) : (a.updatedEpochSec > b.updatedEpochSec);
                });
                json arr = json::array();
                const std::uint64_t fFilter = args.value("folder_id", std::uint64_t{ 0 });
                for (const auto& it : items) {
                    if (args.contains("folder_id") && it.folderId != fFilter) continue;
                    if (it.inTrash && !args.value("include_trash", false)) continue;
                    arr.push_back(SerializeLibraryItem(it));
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "items", std::move(arr) } });
            }
            if (action == "add_item") {
                LibraryItemRecord rec{};
                rec.folderId = args.value("folder_id", std::uint64_t{ 0 });
                if (rec.folderId == 0 && args.contains("folder") && args["folder"].is_string()) {
                    const std::string fName = args["folder"].get<std::string>();
                    if (!fName.empty()) {
                        for (const auto& f : ctx.library.Folders()) {
                            if (f.nameUtf8 == fName) { rec.folderId = f.folderId; break; }
                        }
                        if (rec.folderId == 0) {
                            rec.folderId = ctx.library.CreateFolder(fName, 0, 0);
                        }
                    }
                }
                rec.titleUtf8 = args.value("title", "Nuevo Cuaderno");
                rec.kindUtf8 = args.value("kind", "notebook");
                rec.defaultPaper = ParsePaperPattern(args.value("background", "grid"), PaperPattern::Grid);
                rec.favorite = args.value("favorite", false);
                rec.pinned = args.value("pinned", false);
                rec.priorityRank = args.value("priority", 0);
                rec.filePathUtf8 = args.value("file_path", "");
                rec.updatedEpochSec = static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
                if (args.contains("tags") && args["tags"].is_array()) {
                    for (const auto& t : args["tags"]) if (t.is_string()) rec.tagsUtf8.push_back(t.get<std::string>());
                }
                const std::uint64_t id = ctx.library.AddLibraryItem(std::move(rec));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "item_id", id } });
            }
            if (action == "rename_item") {
                const bool ok = ctx.library.RenameItem(args.value("item_id", std::uint64_t{ 0 }), args.value("title", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "renamed", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "move_item") {
                const bool ok = ctx.library.MoveItemToFolder(args.value("item_id", std::uint64_t{ 0 }), args.value("folder_id", std::uint64_t{ 0 }));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "moved", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "favorite") {
                const bool ok = ctx.library.SetFavorite(args.value("item_id", std::uint64_t{ 0 }), args.value("enabled", true));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "updated", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "pin") {
                const bool ok = ctx.library.SetPinned(args.value("item_id", std::uint64_t{ 0 }), args.value("enabled", true));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "updated", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "trash") {
                const bool ok = ctx.library.MoveToTrash(args.value("item_id", std::uint64_t{ 0 }));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "trashed", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "restore") {
                const bool ok = ctx.library.RestoreFromTrash(args.value("item_id", std::uint64_t{ 0 }));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "restored", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "duplicate") {
                const std::uint64_t id = ctx.library.DuplicateItem(args.value("item_id", std::uint64_t{ 0 }));
                if (id != 0) ctx.SaveProfileIfAllowed();
                return id != 0 ? McpToolResult::Ok({ { "new_item_id", id } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "merge") {
                const std::uint64_t id = ctx.library.MergeItems(
                    args.value("item_id", std::uint64_t{ 0 }),
                    args.value("secondary_item_id", std::uint64_t{ 0 }),
                    args.value("title", "Cuaderno Combinado"));
                if (id != 0) ctx.SaveProfileIfAllowed();
                return id != 0 ? McpToolResult::Ok({ { "merged_item_id", id } }) : McpToolResult::Err("No se pudieron combinar los elementos.");
            }
            if (action == "tag_assign") {
                const bool ok = ctx.library.AssignTag(args.value("item_id", std::uint64_t{ 0 }), args.value("tag", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "tagged", true } }) : McpToolResult::Err("Elemento no encontrado.");
            }
            if (action == "tag_rename") {
                const std::size_t n = ctx.library.RenameTagAcrossLibrary(args.value("old_tag", ""), args.value("new_tag", ""));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "updated_items", n } });
            }
            if (action == "content_view") {
                const std::string vw = LowerAscii(args.value("view", "recent"));
                std::vector<LibraryItemRecord> res;
                if (vw == "photos") res = ctx.library.QueryPhotosContentView();
                else if (vw == "recordings") res = ctx.library.QueryRecordingsContentView();
                else if (vw == "bookmarks") res = ctx.library.QueryBookmarksContentView();
                else if (vw == "attachments") res = ctx.library.QueryAttachmentsContentView();
                else if (vw == "favorites") res = ctx.library.QueryFavoritesAndPinned();
                else if (vw == "trash") res = ctx.library.QueryTrashItems();
                else if (vw == "kind") res = ctx.library.QueryByKind(args.value("kind", "notebook"));
                else if (vw == "tag") res = ctx.library.QueryByTag(args.value("tag", ""));
                else res = ctx.library.QueryRecentItems(args.value("limit", std::size_t{ 50 }));
                json arr = json::array();
                for (const auto& it : res) arr.push_back(SerializeLibraryItem(it));
                return McpToolResult::Ok({ { "view", vw }, { "count", arr.size() }, { "items", std::move(arr) } });
            }
            if (action == "smart_folder_add" || action == "smart_folder_eval") {
                SmartFolderRule rule{};
                rule.nameUtf8 = args.value("name", "Carpeta Inteligente");
                rule.requiredTagPrefixUtf8 = args.value("tag", "");
                rule.textQueryUtf8 = args.value("query", "");
                rule.onlyFavorites = args.value("only_favorites", false);
                rule.onlyWithRecordings = args.value("only_with_recordings", false);
                rule.onlyWithPhotos = args.value("only_with_photos", false);
                if (action == "smart_folder_add") {
                    ctx.library.AddSmartFolder(rule);
                    ctx.SaveProfileIfAllowed();
                }
                const auto hits = ctx.library.EvaluateSmartFolder(rule);
                json arr = json::array();
                for (const auto& it : hits) arr.push_back(SerializeLibraryItem(it));
                return McpToolResult::Ok({ { "rule_name", rule.nameUtf8 }, { "count", arr.size() }, { "items", std::move(arr) } });
            }
            if (action == "vault_notebook_create") {
                const std::uint64_t nid = ctx.notebookVault.CreateNotebook(args.value("title", "Cuaderno"));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "notebook_id", nid } });
            }
            if (action == "vault_section_create") {
                const std::uint64_t sid = ctx.notebookVault.CreateSection(args.value("notebook_id", std::uint64_t{ 0 }), args.value("title", "Seccion"));
                if (sid != 0) ctx.SaveProfileIfAllowed();
                return sid != 0 ? McpToolResult::Ok({ { "section_id", sid } }) : McpToolResult::Err("Notebook no encontrado.");
            }
            if (action == "vault_page_add") {
                const std::uint64_t pid = ctx.notebookVault.AddPageEntry(
                    args.value("notebook_id", std::uint64_t{ 0 }),
                    args.value("section_id", std::uint64_t{ 0 }),
                    args.value("title", "Pagina"),
                    args.value("file_path", ""));
                if (pid != 0) ctx.SaveProfileIfAllowed();
                return pid != 0 ? McpToolResult::Ok({ { "page_entry_id", pid } }) : McpToolResult::Err("Notebook o seccion no encontrada.");
            }
            if (action == "vault_page_move") {
                const bool ok = ctx.notebookVault.MovePageBetweenSections(
                    args.value("page_entry_id", std::uint64_t{ 0 }),
                    args.value("dst_notebook_id", std::uint64_t{ 0 }),
                    args.value("dst_section_id", std::uint64_t{ 0 }));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "moved", true } }) : McpToolResult::Err("No se pudo mover la pagina.");
            }
            if (action == "vault_list") {
                json nbs = json::array();
                for (const auto& nb : ctx.notebookVault.Notebooks()) {
                    json secs = json::array();
                    for (const auto& sc : nb.sections) {
                        json pgs = json::array();
                        for (const auto& pg : sc.pages) {
                            pgs.push_back({ { "id", pg.id }, { "title", pg.titleUtf8 }, { "path", pg.cudocRelPathUtf8 } });
                        }
                        secs.push_back({ { "id", sc.id }, { "title", sc.titleUtf8 }, { "pages", std::move(pgs) } });
                    }
                    nbs.push_back({ { "id", nb.id }, { "title", nb.titleUtf8 }, { "sections", std::move(secs) } });
                }
                return McpToolResult::Ok({ { "notebooks", std::move(nbs) } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_library: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_knowledge",
        "Boveda de conocimiento Markdown con propiedades YAML, plantillas, notas diarias, bloques sincronizados, transclusiones, backlinks, menciones no enlazadas, grafo 2D global/local y publicacion estatica HTML.",
        MakeToolSchema(
            {
                "note_create", "note_get", "note_list", "note_update", "note_move", "note_trash", "note_restore",
                "note_rename", "heading_rename", "block_rename", "set_property", "set_cover_paper",
                "template_register", "template_create_note", "daily_note", "synced_block_update",
                "resolve_transclusion", "search", "filter", "links_outgoing", "backlinks",
                "unlinked_mentions", "convert_mention", "broken_links",
                "graph_global", "graph_local", "graph_layout_2d", "reading_view", "publish_static_site"
            },
            {
                { "note_id", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "old_title", { { "type", "string" } } },
                { "new_title", { { "type", "string" } } },
                { "folder", { { "type", "string" } } },
                { "body_markdown", { { "type", "string" } } },
                { "body", { { "type", "string" } } },
                { "tags", { { "type", "array" }, { "items", { { "type", "string" } } } } },
                { "aliases", { { "type", "array" }, { "items", { { "type", "string" } } } } },
                { "paper", { { "type", "string" } } },
                { "key", { { "type", "string" } } },
                { "prop_type", { { "type", "string" } } },
                { "value", { { "type", "string" } } },
                { "template_name", { { "type", "string" } } },
                { "template_content", { { "type", "string" } } },
                { "date_iso", { { "type", "string" } } },
                { "sync_id", { { "type", "string" } } },
                { "target_expr", { { "type", "string" } } },
                { "query", { { "type", "string" } } },
                { "tag", { { "type", "string" } } },
                { "byte_offset", { { "type", "integer" } } },
                { "byte_length", { { "type", "integer" } } },
                { "max_depth", { { "type", "integer" } } },
                { "output_dir", { { "type", "string" } } },
                { "site_title", { { "type", "string" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            ctx.EnsureProfileLoaded();
            const std::string action = args.value("action", "");
            std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);

            if (action == "note_create") {
                const std::string bodyText = args.contains("body_markdown")
                    ? args.value("body_markdown", "")
                    : args.value("body", "");
                const std::string nid = ctx.knowledgeVault.CreateNote(
                    args.value("title", "Nota"),
                    args.value("folder", ""),
                    bodyText,
                    {},
                    ParsePaperPattern(args.value("paper", "plain"), PaperPattern::Plain));
                if (auto* nm = ctx.knowledgeVault.FindNoteMutableById(nid)) {
                    if (args.contains("tags") && args["tags"].is_array()) {
                        for (const auto& t : args["tags"]) {
                            if (t.is_string()) {
                                const std::string ts = t.get<std::string>();
                                if (!ts.empty() && std::find(nm->tagsUtf8.begin(), nm->tagsUtf8.end(), ts) == nm->tagsUtf8.end()) {
                                    nm->tagsUtf8.push_back(ts);
                                }
                            }
                        }
                    }
                    if (args.contains("aliases") && args["aliases"].is_array()) {
                        for (const auto& a : args["aliases"]) {
                            if (a.is_string()) {
                                const std::string as = a.get<std::string>();
                                if (!as.empty() && std::find(nm->aliasesUtf8.begin(), nm->aliasesUtf8.end(), as) == nm->aliasesUtf8.end()) {
                                    nm->aliasesUtf8.push_back(as);
                                }
                            }
                        }
                    }
                }
                ctx.SaveProfileIfAllowed();
                const auto* n = ctx.knowledgeVault.FindNoteById(nid);
                return McpToolResult::Ok(n ? SerializeNote(*n) : json{ { "note_id", nid } });
            }
            if (action == "note_list") {
                json arr = json::array();
                for (const auto& n : ctx.knowledgeVault.ActiveNotes()) arr.push_back(SerializeNote(n, false));
                return McpToolResult::Ok({ { "count", arr.size() }, { "notes", std::move(arr) } });
            }
            if (action == "note_get") {
                const auto* n = args.contains("note_id")
                    ? ctx.knowledgeVault.FindNoteById(args["note_id"].get<std::string>())
                    : ctx.knowledgeVault.FindNoteByTitleOrAlias(args.value("title", ""));
                return n ? McpToolResult::Ok(SerializeNote(*n)) : McpToolResult::Err("Nota no encontrada.");
            }
            if (action == "note_update") {
                auto* n = ctx.knowledgeVault.FindNoteMutableById(args.value("note_id", ""));
                if (!n) return McpToolResult::Err("Nota no encontrada.");
                if (args.contains("body_markdown")) n->bodyMarkdownUtf8 = args["body_markdown"].get<std::string>();
                else if (args.contains("body")) n->bodyMarkdownUtf8 = args["body"].get<std::string>();
                if (args.contains("title")) n->titleUtf8 = args["title"].get<std::string>();
                if (args.contains("tags") && args["tags"].is_array()) {
                    n->tagsUtf8.clear();
                    for (const auto& t : args["tags"]) if (t.is_string()) n->tagsUtf8.push_back(t.get<std::string>());
                }
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok(SerializeNote(*n));
            }
            if (action == "note_move") {
                const bool ok = ctx.knowledgeVault.MoveNote(args.value("note_id", ""), args.value("folder", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "moved", true } }) : McpToolResult::Err("Nota no encontrada.");
            }
            if (action == "note_trash") {
                const bool ok = ctx.knowledgeVault.MoveToTrash(args.value("note_id", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "trashed", true } }) : McpToolResult::Err("Nota no encontrada.");
            }
            if (action == "note_restore") {
                const bool ok = ctx.knowledgeVault.RestoreFromTrash(args.value("note_id", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "restored", true } }) : McpToolResult::Err("Nota no encontrada.");
            }
            if (action == "note_rename") {
                const auto rep = ctx.knowledgeVault.RenameNote(args.value("old_title", ""), args.value("new_title", ""));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "notes_modified", rep.notesModified }, { "references_updated", rep.referencesUpdated }, { "broken_remaining", rep.brokenLinksRemaining } });
            }
            if (action == "heading_rename") {
                const auto rep = ctx.knowledgeVault.RenameHeadingInNote(args.value("title", ""), args.value("old_title", ""), args.value("new_title", ""));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "notes_modified", rep.notesModified }, { "references_updated", rep.referencesUpdated } });
            }
            if (action == "block_rename") {
                const auto rep = ctx.knowledgeVault.RenameBlockIdInNote(args.value("title", ""), args.value("old_title", ""), args.value("new_title", ""));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "notes_modified", rep.notesModified }, { "references_updated", rep.referencesUpdated } });
            }
            if (action == "set_property") {
                NoteProperty np{};
                np.keyUtf8 = args.value("key", "");
                np.type = ParseNotePropType(args.value("prop_type", "text"));
                np.valueUtf8 = args.value("value", "");
                const bool ok = ctx.knowledgeVault.SetNoteProperty(args.value("note_id", ""), np);
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "updated", true } }) : McpToolResult::Err("Nota no encontrada.");
            }
            if (action == "set_cover_paper") {
                const bool ok = ctx.knowledgeVault.SetNoteCoverAndPaper(
                    args.value("note_id", ""),
                    args.value("cover_rgba", std::uint32_t{ 0x1D4ED8FFu }),
                    ParsePaperPattern(args.value("paper", "plain"), PaperPattern::Plain));
                if (ok) ctx.SaveProfileIfAllowed();
                return ok ? McpToolResult::Ok({ { "updated", true } }) : McpToolResult::Err("Nota no encontrada.");
            }
            if (action == "template_register") {
                NoteTemplate tpl{};
                tpl.nameUtf8 = args.value("template_name", "Plantilla");
                tpl.contentTemplateUtf8 = args.value("template_content", "# {{title}}\n\nFecha: {{date}}\n");
                ctx.knowledgeVault.RegisterTemplate(std::move(tpl));
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "registered", true }, { "template_count", ctx.knowledgeVault.Templates().size() } });
            }
            if (action == "template_create_note") {
                const std::string nid = ctx.knowledgeVault.CreateNoteFromTemplate(
                    args.value("template_name", ""),
                    args.value("title", "Nota"),
                    args.value("folder", ""),
                    args.value("date_iso", ""));
                ctx.SaveProfileIfAllowed();
                return !nid.empty() ? McpToolResult::Ok({ { "note_id", nid } }) : McpToolResult::Err("Plantilla no encontrada.");
            }
            if (action == "daily_note") {
                const std::string nid = ctx.knowledgeVault.OpenOrCreateDailyNote(
                    args.value("date_iso", "2026-11-26"), args.value("template_name", ""));
                ctx.SaveProfileIfAllowed();
                const auto* n = ctx.knowledgeVault.FindNoteById(nid);
                return McpToolResult::Ok(n ? SerializeNote(*n) : json{ { "note_id", nid } });
            }
            if (action == "synced_block_update") {
                std::size_t touched = 0;
                const bool ok = ctx.knowledgeVault.UpdateSyncedBlock(
                    args.value("sync_id", ""), args.value("body_markdown", ""), &touched);
                if (ok) ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "updated", ok }, { "notes_touched", touched } });
            }
            if (action == "resolve_transclusion") {
                return McpToolResult::Ok({ { "markdown", ctx.knowledgeVault.ResolveTransclusionMarkdown(args.value("target_expr", "")) } });
            }
            if (action == "search") {
                const auto hits = ctx.knowledgeVault.SearchWorkspace(args.value("query", ""), args.value("max_results", std::size_t{ 50 }));
                json arr = json::array();
                for (const auto& h : hits) {
                    arr.push_back({ { "note_id", h.noteIdUtf8 }, { "title", h.titleUtf8 }, { "folder", h.folderPathUtf8 }, { "snippet", h.snippetUtf8 }, { "score", h.score } });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "hits", std::move(arr) } });
            }
            if (action == "filter") {
                NoteFilterCriteria cr{};
                cr.folderPrefixUtf8 = args.value("folder", "");
                cr.requiredTagUtf8 = args.value("tag", "");
                cr.propKeyUtf8 = args.value("key", "");
                cr.propOpUtf8 = args.value("op", "==");
                cr.propValueUtf8 = args.value("value", "");
                const auto res = ctx.knowledgeVault.FilterNotes(cr);
                json arr = json::array();
                for (const auto& n : res) arr.push_back(SerializeNote(n, false));
                return McpToolResult::Ok({ { "count", arr.size() }, { "notes", std::move(arr) } });
            }
            if (action == "links_outgoing" || action == "backlinks" || action == "broken_links") {
                const auto links = (action == "links_outgoing")
                    ? ctx.knowledgeVault.OutgoingLinks(args.value("title", args.value("note_id", "")))
                    : ((action == "backlinks") ? ctx.knowledgeVault.Backlinks(args.value("title", "")) : ctx.knowledgeVault.FindBrokenWikiLinks());
                json arr = json::array();
                for (const auto& l : links) {
                    arr.push_back({
                        { "source_id", l.sourceNoteIdUtf8 },
                        { "source_title", l.sourceNoteTitleUtf8 },
                        { "target_raw", l.targetRawUtf8 },
                        { "target_title", l.targetNoteTitleUtf8 },
                        { "target_heading", l.targetHeadingUtf8 },
                        { "context", l.contextSnippetUtf8 }
                    });
                }
                return McpToolResult::Ok({ { "count", arr.size() }, { "links", std::move(arr) } });
            }
            if (action == "unlinked_mentions") {
                const auto ms = ctx.knowledgeVault.FindUnlinkedMentions(args.value("title", ""));
                json arr = json::array();
                for (const auto& m : ms) {
                    arr.push_back({
                        { "source_note_id", m.sourceNoteIdUtf8 },
                        { "source_title", m.sourceNoteTitleUtf8 },
                        { "matched", m.matchedTextUtf8 },
                        { "byte_offset", m.byteOffset },
                        { "byte_length", m.byteLength },
                        { "context", m.contextSnippetUtf8 }
                    });
                }
                return McpToolResult::Ok({ { "mentions", std::move(arr) } });
            }
            if (action == "convert_mention") {
                const bool ok = ctx.knowledgeVault.ConvertMentionToWikiLink(
                    args.value("note_id", ""),
                    args.value("byte_offset", std::size_t{ 0 }),
                    args.value("byte_length", std::size_t{ 0 }),
                    args.value("title", ""));
                if (ok) ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "converted", ok } });
            }
            if (action == "graph_global" || action == "graph_local" || action == "graph_layout_2d") {
                GraphFilterOptions flt{};
                flt.requiredTagUtf8 = args.value("tag", "");
                flt.folderPrefixUtf8 = args.value("folder", "");
                auto g = (action == "graph_local")
                    ? ctx.knowledgeVault.BuildLocalGraph(args.value("title", ""), args.value("max_depth", 1), flt)
                    : ctx.knowledgeVault.BuildGlobalGraph(flt);
                KnowledgeVault::ComputeGraphLayout2D(g, 60, args.value("width", 800.0), args.value("height", 560.0));
                json nodes = json::array(), edges = json::array();
                for (const auto& nd : g.nodes) {
                    nodes.push_back({ { "id", nd.idUtf8 }, { "label", nd.labelUtf8 }, { "degree", nd.degree }, { "x", nd.x }, { "y", nd.y }, { "resolved", nd.resolved } });
                }
                for (const auto& eg : g.edges) {
                    edges.push_back({ { "from", eg.fromIdUtf8 }, { "to", eg.toIdUtf8 }, { "resolved", eg.resolved } });
                }
                return McpToolResult::Ok({ { "nodes", std::move(nodes) }, { "edges", std::move(edges) } });
            }
            if (action == "reading_view") {
                const auto* n = args.contains("note_id")
                    ? ctx.knowledgeVault.FindNoteById(args["note_id"].get<std::string>())
                    : ctx.knowledgeVault.FindNoteByTitleOrAlias(args.value("title", ""));
                if (!n) return McpToolResult::Err("Nota no encontrada.");
                const auto rich = RichBlockDocument::FromMarkdown(n->bodyMarkdownUtf8);
                return McpToolResult::Ok({
                    { "title", n->titleUtf8 },
                    { "plain_text", rich.ToReadingPlainText(80) },
                    { "html", rich.ToHtml5() }
                });
            }
            if (action == "publish_static_site") {
                const std::wstring dirW = args.contains("output_dir") && !args["output_dir"].get<std::string>().empty()
                    ? Utf8ToWide(args["output_dir"].get<std::string>())
                    : (fs::path(ctx.EffectiveArtifactsDirW()) / L"published_site").wstring();
                std::vector<std::string> allowedIds;
                if (args.contains("note_ids") && args["note_ids"].is_array()) {
                    for (const auto& nid : args["note_ids"]) if (nid.is_string()) allowedIds.push_back(nid.get<std::string>());
                } else {
                    for (const auto& n : ctx.knowledgeVault.ActiveNotes()) allowedIds.push_back(n.noteIdUtf8);
                }
                std::string err;
                const auto rep = StaticSitePublisher::PublishVaultToStaticHtml(
                    ctx.knowledgeVault, allowedIds, dirW, &err, args.value("custom_css", ""));
                return rep.ok
                    ? McpToolResult::Ok({ { "ok", true }, { "published_count", rep.publishedNoteCount }, { "index_html", rep.indexHtmlPathUtf8 }, { "slugs", rep.publishedSlugsUtf8 } })
                    : McpToolResult::Err("Error al publicar sitio estatico: " + err);
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_knowledge: " + action);
        }
    });

    router.RegisterTool({
        "cuadernu_database",
        "Bases de datos estructuradas tipo Notion / Obsidian Bases (columnas, formulas, rollups, registros, vistas Tabla/Kanban/Calendario/Timeline/Grafico/Dashboard/Mapa, plantillas, formularios, automatizaciones y sincronizacion con boveda/tabla).",
        MakeToolSchema(
            {
                "get", "add_column", "add_record", "update_cell", "remove_record", "recompute",
                "view_add", "view_query", "view_grouped", "kanban_move", "calendar", "timeline",
                "chart", "dashboard", "map", "template_add", "record_from_template",
                "form_register", "form_submit", "sync_from_vault", "sync_to_vault",
                "sync_to_doc_table", "export_base", "import_base", "automation_register", "automation_trigger"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "column_id", { { "type", "string" } } },
                { "name", { { "type", "string" } } },
                { "column_type", { { "type", "string" } } },
                { "options", { { "type", "array" } } },
                { "formula", { { "type", "string" } } },
                { "record_id", { { "type", "string" } } },
                { "title", { { "type", "string" } } },
                { "cells", { { "type", "object" } } },
                { "value", {} },
                { "view_id", { { "type", "string" } } },
                { "view_kind", { { "type", "string" } } },
                { "group_by", { { "type", "string" } } },
                { "date_column", { { "type", "string" } } },
                { "end_date_column", { { "type", "string" } } },
                { "chart_column", { { "type", "string" } } },
                { "status_column", { { "type", "string" } } },
                { "new_status", { { "type", "string" } } },
                { "template_id", { { "type", "string" } } },
                { "form_id", { { "type", "string" } } },
                { "fields", { { "type", "array" } } },
                { "base_json", { { "type", "string" } } },
                { "path", { { "type", "string" } } },
                { "rule_id", { { "type", "string" } } },
                { "steps", { { "type", "array" } } }
            }),
        [&ctx](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            auto& db = s->database;
            const std::string action = args.value("action", "");

            auto findOrDefaultView = [&]() -> DbViewConfig {
                DbViewConfig def{};
                const std::string vid = args.value("view_id", db.ActiveViewId());
                if (const auto* v = db.FindView(vid)) {
                    def = *v;
                } else {
                    def.viewIdUtf8 = "default";
                    def.nameUtf8 = "Vista";
                }
                if (args.contains("view_kind") && args["view_kind"].is_string()) {
                    def.kind = ParseDbViewKind(args["view_kind"].get<std::string>());
                }
                if (args.contains("group_by") && args["group_by"].is_string()) {
                    def.groupByColumnIdUtf8 = args["group_by"].get<std::string>();
                } else if (args.contains("column") && args["column"].is_string()) {
                    def.groupByColumnIdUtf8 = args["column"].get<std::string>();
                }
                if (args.contains("date_column") && args["date_column"].is_string()) {
                    def.dateColumnIdUtf8 = args["date_column"].get<std::string>();
                }
                if (args.contains("end_date_column") && args["end_date_column"].is_string()) {
                    def.endDateColumnIdUtf8 = args["end_date_column"].get<std::string>();
                }
                if (args.contains("chart_column") && args["chart_column"].is_string()) {
                    def.chartValueColumnIdUtf8 = args["chart_column"].get<std::string>();
                }
                if (args.contains("lat_column") && args["lat_column"].is_string()) {
                    def.latColumnIdUtf8 = args["lat_column"].get<std::string>();
                }
                if (args.contains("lon_column") && args["lon_column"].is_string()) {
                    def.lonColumnIdUtf8 = args["lon_column"].get<std::string>();
                }
                return def;
            };

            if (action == "get") {
                json cols = json::array(), recs = json::array();
                for (const auto& c : db.Columns()) {
                    cols.push_back({ { "id", c.idUtf8 }, { "name", c.nameUtf8 }, { "formula", c.formulaExprUtf8 }, { "options", c.allowedOptionsUtf8 } });
                }
                for (const auto& r : db.Records()) recs.push_back(SerializeDbRecord(r));
                return McpToolResult::Ok({ { "id", db.Id() }, { "title", db.Title() }, { "columns", std::move(cols) }, { "records", std::move(recs) } });
            }
            if (action == "add_column") {
                DbColumnDef col{};
                col.idUtf8 = args.value("column_id", args.value("name", "col"));
                col.nameUtf8 = args.value("name", col.idUtf8);
                col.type = ParseDbColType(args.value("column_type", args.value("type", "text")));
                col.formulaExprUtf8 = args.value("formula", "");
                if (args.contains("options") && args["options"].is_array()) {
                    for (const auto& o : args["options"]) if (o.is_string()) col.allowedOptionsUtf8.push_back(o.get<std::string>());
                }
                db.AddColumn(std::move(col));
                return McpToolResult::Ok({ { "added", true }, { "column_count", db.Columns().size() } });
            }
            if (action == "add_record") {
                DbRecord r{};
                r.titleUtf8 = args.value("title", "Registro");
                r.bodyMarkdownUtf8 = args.value("body_markdown", "");
                if (args.contains("cells") && args["cells"].is_object()) {
                    for (auto it = args["cells"].begin(); it != args["cells"].end(); ++it) {
                        DbCellValue v{};
                        if (it.value().is_string()) v.textUtf8 = it.value().get<std::string>();
                        else if (it.value().is_number()) { v.numberVal = it.value().get<double>(); v.textUtf8 = std::to_string(v.numberVal); }
                        else if (it.value().is_boolean()) { v.boolVal = it.value().get<bool>(); v.textUtf8 = v.boolVal ? "true" : "false"; }
                        else if (it.value().is_array()) { for (const auto& e : it.value()) if (e.is_string()) v.listUtf8.push_back(e.get<std::string>()); }
                        r.cells[it.key()] = std::move(v);
                    }
                }
                const std::string rid = db.AddRecord(std::move(r));
                s->automations.TriggerEvent(AutomationTriggerKind::OnRecordCreated, db, rid);
                db.RecomputeFormulasAndRollups(&ctx.knowledgeVault);
                return McpToolResult::Ok({ { "record_id", rid }, { "record_count", db.Records().size() } });
            }
            if (action == "update_cell") {
                const std::string rid = args.value("record_id", "");
                const std::string cid = args.value("column_id", "");
                bool ok = false;
                std::string newValStr;
                if (args.contains("value")) {
                    const auto& val = args["value"];
                    if (val.is_number()) { ok = db.SetCellNumber(rid, cid, val.get<double>()); newValStr = std::to_string(val.get<double>()); }
                    else if (val.is_boolean()) { ok = db.SetCellBool(rid, cid, val.get<bool>()); newValStr = val.get<bool>() ? "true" : "false"; }
                    else if (val.is_array()) {
                        std::vector<std::string> ls;
                        for (const auto& e : val) if (e.is_string()) ls.push_back(e.get<std::string>());
                        ok = db.SetCellList(rid, cid, std::move(ls));
                    } else if (val.is_string()) {
                        newValStr = val.get<std::string>();
                        ok = db.SetCellText(rid, cid, newValStr);
                    }
                }
                if (ok) {
                    s->automations.TriggerEvent(AutomationTriggerKind::OnPropertyChanged, db, rid, cid, newValStr);
                    db.RecomputeFormulasAndRollups(&ctx.knowledgeVault);
                }
                return ok ? McpToolResult::Ok({ { "updated", true } }) : McpToolResult::Err("Registro no encontrado.");
            }
            if (action == "remove_record") {
                const bool ok = db.RemoveRecord(args.value("record_id", ""));
                return ok ? McpToolResult::Ok({ { "removed", true } }) : McpToolResult::Err("Registro no encontrado.");
            }
            if (action == "recompute") {
                db.RecomputeFormulasAndRollups(&ctx.knowledgeVault);
                return McpToolResult::Ok({ { "recomputed", true }, { "record_count", db.Records().size() } });
            }
            if (action == "view_add") {
                auto vw = findOrDefaultView();
                vw.viewIdUtf8 = args.value("view_id", "view_1");
                vw.nameUtf8 = args.value("name", vw.viewIdUtf8);
                vw.kind = ParseDbViewKind(args.value("view_kind", "table"));
                db.AddOrUpdateView(vw);
                db.SetActiveView(vw.viewIdUtf8);
                return McpToolResult::Ok({ { "view_id", vw.viewIdUtf8 }, { "view_count", db.Views().size() } });
            }
            if (action == "view_query") {
                const auto recs = db.QueryViewRecords(findOrDefaultView());
                json arr = json::array();
                for (const auto& r : recs) arr.push_back(SerializeDbRecord(r));
                return McpToolResult::Ok({ { "count", arr.size() }, { "records", std::move(arr) } });
            }
            if (action == "view_grouped") {
                const auto buckets = db.QueryGroupedBuckets(findOrDefaultView(), args.value("chart_column", ""));
                json arr = json::array();
                for (const auto& b : buckets) {
                    json recs = json::array();
                    for (const auto& r : b.records) recs.push_back(SerializeDbRecord(r));
                    arr.push_back({ { "group", b.groupKeyUtf8 }, { "sum", b.numericSum }, { "avg", b.numericAvg }, { "records", std::move(recs) } });
                }
                return McpToolResult::Ok({ { "buckets", std::move(arr) } });
            }
            if (action == "kanban_move") {
                const bool ok = db.MoveKanbanCard(args.value("record_id", ""), args.value("status_column", "status"), args.value("new_status", ""));
                return ok ? McpToolResult::Ok({ { "moved", true } }) : McpToolResult::Err("No se pudo mover tarjeta Kanban.");
            }
            if (action == "calendar") {
                const auto entries = db.BuildCalendarProjection(findOrDefaultView(), args.value("year_month", ""));
                json arr = json::array();
                for (const auto& e : entries) arr.push_back({ { "date", e.dateIsoUtf8 }, { "record_id", e.recordIdUtf8 }, { "title", e.titleUtf8 } });
                return McpToolResult::Ok({ { "entries", std::move(arr) } });
            }
            if (action == "timeline") {
                const auto bars = db.BuildTimelineProjection(findOrDefaultView());
                json arr = json::array();
                for (const auto& b : bars) arr.push_back({ { "record_id", b.recordIdUtf8 }, { "title", b.titleUtf8 }, { "start", b.startDateIsoUtf8 }, { "end", b.endDateIsoUtf8 }, { "days", b.durationDays } });
                return McpToolResult::Ok({ { "bars", std::move(arr) } });
            }
            if (action == "chart") {
                const auto pts = db.BuildChartProjection(findOrDefaultView());
                json arr = json::array();
                for (const auto& p : pts) arr.push_back({ { "category", p.categoryUtf8 }, { "value", p.value } });
                return McpToolResult::Ok({ { "series", std::move(arr) } });
            }
            if (action == "dashboard") {
                const auto dash = db.BuildDashboardProjection(findOrDefaultView(), args.value("status_column", "status"), args.value("chart_column", ""));
                return McpToolResult::Ok({
                    { "total_records", dash.totalRecords },
                    { "completed_count", dash.completedCount },
                    { "completion_rate_pct", dash.completionRatePct },
                    { "sum", dash.primaryNumericSum },
                    { "avg", dash.primaryNumericAvg }
                });
            }
            if (action == "map") {
                const auto pins = db.BuildMapProjection(findOrDefaultView());
                json arr = json::array();
                for (const auto& p : pins) arr.push_back({ { "record_id", p.recordIdUtf8 }, { "title", p.titleUtf8 }, { "lat", p.latitude }, { "lon", p.longitude } });
                return McpToolResult::Ok({ { "pins", std::move(arr) } });
            }
            if (action == "template_add") {
                DbRecordTemplate t{};
                t.templateIdUtf8 = args.value("template_id", "tpl_1");
                t.nameUtf8 = args.value("name", "Plantilla");
                t.defaultTitlePrefixUtf8 = args.value("title", "");
                t.defaultBodyMarkdownUtf8 = args.value("body_markdown", "");
                db.AddRecordTemplate(std::move(t));
                return McpToolResult::Ok({ { "added", true } });
            }
            if (action == "record_from_template") {
                const std::string rid = db.CreateRecordFromTemplate(args.value("template_id", ""), args.value("title", ""));
                return !rid.empty() ? McpToolResult::Ok({ { "record_id", rid } }) : McpToolResult::Err("Plantilla no encontrada.");
            }
            if (action == "form_register") {
                DbConnectedForm f{};
                f.formIdUtf8 = args.value("form_id", "form_1");
                f.titleUtf8 = args.value("title", "Formulario");
                if (args.contains("fields") && args["fields"].is_array()) {
                    for (const auto& fld : args["fields"]) {
                        f.fields.push_back({ fld.value("column_id", ""), fld.value("label", ""), fld.value("required", false) });
                    }
                }
                db.RegisterForm(std::move(f));
                return McpToolResult::Ok({ { "registered", true } });
            }
            if (action == "form_submit") {
                std::unordered_map<std::string, DbCellValue> vals;
                if (args.contains("cells") && args["cells"].is_object()) {
                    for (auto it = args["cells"].begin(); it != args["cells"].end(); ++it) {
                        DbCellValue v{};
                        if (it.value().is_string()) v.textUtf8 = it.value().get<std::string>();
                        else if (it.value().is_number()) { v.numberVal = it.value().get<double>(); v.textUtf8 = std::to_string(v.numberVal); }
                        vals[it.key()] = std::move(v);
                    }
                }
                std::string rid, verr;
                const bool ok = db.SubmitForm(args.value("form_id", ""), args.value("title", "Envio"), vals, &rid, &verr);
                return ok ? McpToolResult::Ok({ { "record_id", rid } }) : McpToolResult::Err(verr);
            }
            if (action == "sync_from_vault") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                db.ImportRecordsFromVaultNotes(ctx.knowledgeVault, args.value("folder", ""));
                return McpToolResult::Ok({ { "record_count", db.Records().size() } });
            }
            if (action == "sync_to_vault") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                db.SyncRecordsToVaultNotes(ctx.knowledgeVault);
                ctx.SaveProfileIfAllowed();
                return McpToolResult::Ok({ { "synced", true } });
            }
            if (action == "sync_to_doc_table") {
                const ObjectTarget tgt = TargetFromArgs(args, s->doc);
                const std::uint64_t oid = db.SyncToDocumentTableObject(
                    s->doc, s->cmdStack, tgt, args.value("x", 72.0), args.value("y", 72.0),
                    args.value("include_chart", false));
                return McpToolResult::Ok({ { "object_id", oid } });
            }
            if (action == "export_base") {
                const std::string raw = db.SerializeToBaseJson();
                if (args.contains("path") && !args["path"].get<std::string>().empty()) {
                    std::string err;
                    if (!db.SaveToBaseFile(Utf8ToWide(args["path"].get<std::string>()), &err)) return McpToolResult::Err(err);
                }
                return McpToolResult::Ok({ { "base_json", raw } });
            }
            if (action == "import_base") {
                std::string err;
                bool ok = false;
                if (args.contains("path") && !args["path"].get<std::string>().empty()) {
                    ok = db.LoadFromBaseFile(Utf8ToWide(args["path"].get<std::string>()), &err);
                } else {
                    ok = db.DeserializeFromBaseJson(args.value("base_json", ""), &err);
                }
                return ok ? McpToolResult::Ok({ { "imported", true }, { "record_count", db.Records().size() } })
                          : McpToolResult::Err(err);
            }
            if (action == "automation_register") {
                AutomationRule r{};
                r.ruleIdUtf8 = args.value("rule_id", "rule_1");
                r.nameUtf8 = args.value("name", "Automatizacion");
                const std::string tk = LowerAscii(args.value("trigger", "button"));
                r.trigger = (tk == "on_record_created") ? AutomationTriggerKind::OnRecordCreated
                    : ((tk == "on_property_changed") ? AutomationTriggerKind::OnPropertyChanged : AutomationTriggerKind::OnActionButtonClicked);
                r.watchColumnIdUtf8 = args.value("column_id", "");
                r.watchValueUtf8 = args.value("value", "");
                if (args.contains("steps") && args["steps"].is_array()) {
                    for (const auto& st : args["steps"]) {
                        r.steps.push_back({ st.value("action_kind", "set_property"), st.value("target_key", ""), st.value("value", "") });
                    }
                }
                s->automations.RegisterRule(std::move(r));
                return McpToolResult::Ok({ { "registered", true }, { "rule_count", s->automations.Rules().size() } });
            }
            if (action == "automation_trigger") {
                const std::size_t n = s->automations.TriggerEvent(
                    AutomationTriggerKind::OnActionButtonClicked, db,
                    args.value("record_id", ""), args.value("column_id", ""), args.value("value", ""));
                return McpToolResult::Ok({ { "actions_executed", n } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_database: " + action);
        }
    });

    // ---------------- Group I.2: Collaboration, Security, Sync, Backups & LAN CRDT (1 tool) ----------------

    router.RegisterTool({
        "cuadernu_collab_security",
        "Permisos RBAC, bloqueo de notas, enlaces compartidos, sincronizacion local/E2EE de carpetas, respaldos/recuperacion por nota, servidor colaborativo LAN HTTP/CRDT, generador QR SVG y bloqueo PIN de modo examen.",
        MakeToolSchema(
            {
                "rbac_get", "rbac_set_role", "rbac_set_private_page", "note_lock", "note_unlock",
                "share_link_create", "share_link_revoke", "share_link_list",
                "folder_sync", "backup_create", "backup_restore", "backup_recover_single_note",
                "lan_server_start", "lan_server_stop", "lan_server_status", "lan_crdt_merge",
                "lan_qr_generate", "exam_pin_lock"
            },
            {
                { "session_id", { { "type", "string" } } },
                { "user_id", { { "type", "string" } } },
                { "role", { { "type", "string" } } },
                { "page_index", { { "type", "integer" } } },
                { "is_private", { { "type", "boolean" } } },
                { "passphrase", { { "type", "string" } } },
                { "target_id", { { "type", "string" } } },
                { "is_public", { { "type", "boolean" } } },
                { "token_id", { { "type", "string" } } },
                { "local_dir", { { "type", "string" } } },
                { "remote_dir", { { "type", "string" } } },
                { "mode", { { "type", "string" } } },
                { "backup_path", { { "type", "string" } } },
                { "note_rel_path", { { "type", "string" } } },
                { "output_path", { { "type", "string" } } },
                { "port", { { "type", "integer" } } },
                { "loopback_only", { { "type", "boolean" } } },
                { "payload", { { "type", "string" } } },
                { "peer_ops", { { "type", "array" } } },
                { "pin", { { "type", "string" } } },
                { "lock", { { "type", "boolean" } } }
            }),
        [&ctx, &router](const json& args, const McpRequestContext&) -> McpToolResult {
            auto s = ctx.GetOrCreateSession(args);
            const std::string action = args.value("action", "");
            if (args.contains("passphrase") && args["passphrase"].is_string()) {
                router.Redactor().RegisterSecret(args["passphrase"].get<std::string>());
            }
            if (args.contains("pin") && args["pin"].is_string()) {
                router.Redactor().RegisterSecret(args["pin"].get<std::string>());
            }

            if (action == "rbac_get") {
                const std::string uid = args.value("user_id", "local");
                return McpToolResult::Ok({
                    { "user_id", uid },
                    { "role", CollabRoleToString(s->permissions.GetUserRole(uid)) },
                    { "can_edit", s->permissions.CanUserEditDocument(uid) },
                    { "can_comment", s->permissions.CanUserCommentDocument(uid) },
                    { "note_locked", s->permissions.IsNoteLocked() },
                    { "exam_mode_locked", s->examModeLocked }
                });
            }
            if (action == "rbac_set_role") {
                s->permissions.SetUserRole(args.value("user_id", "guest"), ParseCollabRole(args.value("role", "viewer")));
                return McpToolResult::Ok({ { "updated", true } });
            }
            if (action == "rbac_set_private_page") {
                const std::size_t idx = static_cast<std::size_t>(std::max(0, args.value("page_index", 0)));
                s->permissions.SetPagePrivate(idx, args.value("is_private", true));
                return McpToolResult::Ok({ { "page_index", idx }, { "is_private", s->permissions.IsPagePrivate(idx) } });
            }
            if (action == "note_lock") {
                const bool ok = s->permissions.LockNoteWithPassphrase(args.value("passphrase", ""));
                return ok ? McpToolResult::Ok({ { "note_locked", true } }) : McpToolResult::Err("Se requiere passphrase no vacia.");
            }
            if (action == "note_unlock") {
                const bool ok = s->permissions.UnlockNoteWithPassphrase(args.value("passphrase", ""));
                return ok ? McpToolResult::Ok({ { "note_locked", false } }) : McpToolResult::Err("Passphrase incorrecta.");
            }
            if (action == "share_link_create") {
                const auto tok = s->permissions.CreateShareLink(
                    args.value("target_id", s->sessionId),
                    args.value("is_public", false),
                    ParseCollabRole(args.value("role", "viewer")));
                return McpToolResult::Ok({
                    { "token_id", tok.tokenIdUtf8 },
                    { "target_id", tok.targetIdUtf8 },
                    { "is_public", tok.isPublic },
                    { "role", CollabRoleToString(tok.grantedRole) },
                    { "active", tok.active }
                });
            }
            if (action == "share_link_revoke") {
                const bool ok = s->permissions.RevokeShareLink(args.value("token_id", ""));
                return ok ? McpToolResult::Ok({ { "revoked", true } }) : McpToolResult::Err("Enlace no encontrado.");
            }
            if (action == "share_link_list") {
                json arr = json::array();
                for (const auto& t : s->permissions.ShareLinks()) {
                    arr.push_back({ { "token_id", t.tokenIdUtf8 }, { "target_id", t.targetIdUtf8 }, { "is_public", t.isPublic }, { "role", CollabRoleToString(t.grantedRole) }, { "active", t.active } });
                }
                return McpToolResult::Ok({ { "links", std::move(arr) } });
            }
            if (action == "folder_sync") {
                const std::string ldir = args.value("local_dir", "");
                const std::string rdir = args.value("remote_dir", "");
                if (ldir.empty() || rdir.empty()) return McpToolResult::Err("Se requieren 'local_dir' y 'remote_dir'.");
                SelectiveSyncFilter flt{};
                flt.encryptPassphraseUtf8 = args.value("passphrase", "");
                const std::string m = LowerAscii(args.value("mode", "merge"));
                const FolderSyncMode fm = (m == "upload") ? FolderSyncMode::UploadDeviceToTarget
                    : ((m == "replace") ? FolderSyncMode::ReplaceDeviceWithTarget : FolderSyncMode::MergeLossless);
                const auto rep = LocalFirstSyncEngine::SynchronizeDirectories(Utf8ToWide(ldir), Utf8ToWide(rdir), fm, flt);
                return McpToolResult::Ok({
                    { "ok", rep.ok },
                    { "uploaded", rep.uploadedFiles },
                    { "downloaded", rep.downloadedFiles },
                    { "merged_conflicts", rep.mergedConflicts },
                    { "skipped", rep.skippedBySelectiveFilter },
                    { "backup_dir", rep.backupDirUtf8 },
                    { "diagnostic", rep.diagnosticUtf8 }
                });
            }
            if (action == "backup_create") {
                const std::wstring srcW = args.contains("local_dir") && !args["local_dir"].get<std::string>().empty()
                    ? Utf8ToWide(args["local_dir"].get<std::string>()) : ctx.EffectiveProfileRootW();
                const std::wstring dstW = ctx.ResolveArtifactPathW(args.value("backup_path", ""), "cuadernu_backup", ".zip");
                std::string err;
                const bool ok = LocalFirstSyncEngine::CreateBackupPackage(srcW, dstW, args.value("passphrase", ""), &err);
                return ok ? McpToolResult::Ok({ { "backup_path", WideToUtf8(dstW) } }) : McpToolResult::Err(err);
            }
            if (action == "backup_restore") {
                const std::string bp = args.value("backup_path", "");
                const std::string dst = args.value("local_dir", "");
                if (bp.empty() || dst.empty()) return McpToolResult::Err("Se requieren 'backup_path' y 'local_dir'.");
                std::string err;
                const bool ok = LocalFirstSyncEngine::RestoreFullBackupPackage(Utf8ToWide(bp), Utf8ToWide(dst), args.value("passphrase", ""), &err);
                return ok ? McpToolResult::Ok({ { "restored", true }, { "local_dir", dst } }) : McpToolResult::Err(err);
            }
            if (action == "backup_recover_single_note") {
                const std::string bp = args.value("backup_path", "");
                const std::string rel = args.value("note_rel_path", "");
                const std::wstring outW = ctx.ResolveArtifactPathW(args.value("output_path", ""), "recovered_note", ".cudoc");
                std::string err;
                const bool ok = LocalFirstSyncEngine::RestoreSingleNoteFromBackup(Utf8ToWide(bp), rel, outW, args.value("passphrase", ""), &err);
                return ok ? McpToolResult::Ok({ { "recovered_path", WideToUtf8(outW) } }) : McpToolResult::Err(err);
            }
            if (action == "lan_server_start") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                std::string err;
                const int reqPort = args.value("port", 0);
                const bool loopback = args.value("loopback_only", true);
                const bool ok = ctx.lanServer.Start(
                    reqPort, loopback,
                    [s]() {
                        return json{ { "title", s->doc.Title() }, { "pages", s->doc.Pages().size() }, { "revision", s->doc.Revision() } }.dump();
                    },
                    [s](const std::string& author, const std::string& body, std::size_t pg) {
                        s->comments.CreateThread(pg, 0, {}, author, body);
                        return true;
                    },
                    &err);
                return ok ? McpToolResult::Ok({ { "running", true }, { "port", ctx.lanServer.BoundPort() }, { "loopback_only", loopback } })
                          : McpToolResult::Err("No se pudo iniciar servidor LAN: " + err);
            }
            if (action == "lan_server_stop") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                ctx.lanServer.Stop();
                return McpToolResult::Ok({ { "running", false } });
            }
            if (action == "lan_server_status") {
                std::lock_guard<std::recursive_mutex> lk(ctx.storeMtx);
                return McpToolResult::Ok({
                    { "running", ctx.lanServer.IsRunning() },
                    { "port", ctx.lanServer.BoundPort() },
                    { "crdt_op_count", ctx.lanServer.CurrentCrdtOps().size() }
                });
            }
            if (action == "lan_crdt_merge") {
                std::vector<LanRemote::CrdtOp> peer;
                if (args.contains("peer_ops") && args["peer_ops"].is_array()) {
                    for (const auto& op : args["peer_ops"]) {
                        peer.push_back({ op.value("op_id", ""), op.value("author", "peer"), op.value("lamport_ts", std::uint64_t{ 1 }), op.value("payload", "") });
                    }
                }
                const auto merged = LanRemote::MergeCrdtOps(ctx.lanServer.CurrentCrdtOps(), peer);
                json arr = json::array();
                for (const auto& op : merged) {
                    arr.push_back({ { "op_id", op.opId }, { "author", op.author }, { "lamport_ts", op.lamportTs }, { "payload", op.payloadText } });
                }
                return McpToolResult::Ok({ { "merged_count", arr.size() }, { "ops", std::move(arr) } });
            }
            if (action == "lan_qr_generate") {
                const std::string payload = args.value("payload", ctx.lanServer.IsRunning() ? ("http://127.0.0.1:" + std::to_string(ctx.lanServer.BoundPort())) : "");
                LanRemote::QrMatrix qr{};
                std::string err;
                if (!LanRemote::EncodeQrMatrix(payload, qr, &err)) return McpToolResult::Err(err);
                return McpToolResult::Ok({ { "size", qr.size }, { "svg", LanRemote::RenderQrSvg(qr, 6) } });
            }
            if (action == "exam_pin_lock") {
                const bool wantLock = args.value("lock", !s->examModeLocked);
                const std::string pin = args.value("pin", "");
                if (wantLock) {
                    if (pin.size() < 4) return McpToolResult::Err("El PIN de modo examen debe tener al menos 4 caracteres.");
                    s->examPinHashUtf8 = std::to_string(std::hash<std::string>{}(pin));
                    s->examModeLocked = true;
                } else {
                    if (s->examModeLocked && std::to_string(std::hash<std::string>{}(pin)) != s->examPinHashUtf8) {
                        return McpToolResult::Err("PIN incorrecto para desbloquear modo examen.");
                    }
                    s->examModeLocked = false;
                }
                return McpToolResult::Ok({ { "exam_mode_locked", s->examModeLocked } });
            }
            return McpToolResult::Err("Accion desconocida en cuadernu_collab_security: " + action);
        }
    });
}

} // namespace DocStudio::Mcp
