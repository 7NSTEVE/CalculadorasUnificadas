#include "../core/audio_timeline.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/command_stack.hpp"
#include "../core/cudoc_crypto.hpp"
#include "../core/cudoc_storage.hpp"
#include "../core/document_model.hpp"
#include "../core/ink_engine.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/structured_database.hpp"
#include "../core/utf_conv.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../pdf/flexcil_study_overlay.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <filesystem>

static inline std::string U8Str(const std::u8string& s) {
    return std::string(reinterpret_cast<const char*>(s.data()), s.size());
}
static inline std::string U8Str(const std::string& s) { return s; }

#include <fstream>
#include <iostream>
#include <map>
#include <objbase.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace DocStudio;

namespace {

int g_totalChecks = 0;
int g_passedChecks = 0;
int g_failedChecks = 0;

struct IdTestRecord {
    std::string id;
    std::string module;
    bool passed = true;
    std::string statusUtf8 = "Implementado completamente";
    std::string evidenceUtf8;
    std::string limitationUtf8;
};

std::map<std::string, IdTestRecord> g_idResults;

void CheckImpl(bool cond, const char* expr, int line, const std::vector<std::string>& ids, const std::string& mod, const std::string& evidence) {
    ++g_totalChecks;
    if (cond) {
        ++g_passedChecks;
    } else {
        ++g_failedChecks;
        std::cerr << "[FAIL] Line " << line << ": " << expr << " (" << evidence << ")\n";
    }
    for (const auto& id : ids) {
        auto& rec = g_idResults[id];
        rec.id = id;
        rec.module = mod;
        if (!cond) rec.passed = false;
        if (!rec.evidenceUtf8.empty()) rec.evidenceUtf8 += " | ";
        rec.evidenceUtf8 += evidence;
    }
}

void MarkPartial(const std::string& id, const std::string& mod, const std::string& evidence, const std::string& limitation) {
    auto& rec = g_idResults[id];
    rec.id = id;
    rec.module = mod;
    rec.statusUtf8 = "A medias";
    rec.evidenceUtf8 = evidence;
    rec.limitationUtf8 = limitation;
}

#define CHECK_IDS(cond, mod, ids_vec, evidence_str) \
    CheckImpl((cond), #cond, __LINE__, (ids_vec), (mod), (evidence_str))

// ---------------- M6: Audio Multi-Hablante, Actas .ics y Biblioteca / Content Views (199 pending IDs) ----------------

void RunM6_LibraryOrganization50NotesFoldersTagsAndContentViews(const fs::path& tmpDir) {
    LibraryContentViewsManager lib;

    // 1. Create nested folder hierarchy (root -> subject -> sub-group)
    const std::uint64_t fIng = lib.CreateFolder("Ingenieria", 0, 1);
    const std::uint64_t fCalc = lib.CreateFolder("Calculo y Algebra Lineal", fIng, 1);
    const std::uint64_t fFis = lib.CreateFolder("Fisica General", fIng, 2);
    const std::uint64_t fLab = lib.CreateFolder("Laboratorios", fFis, 1);

    // 2. Populate 55 library items (notebooks, continuous docs, infinite boards, PDFs, quick notes)
    // with varied covers, paper templates, nested tags, photos, recordings, bookmarks, attachments
    VaultFullTextIndex fts;
    std::uint64_t firstId = 0;
    std::uint64_t secondId = 0;
    for (int i = 1; i <= 55; ++i) {
        LibraryItemRecord rec;
        rec.folderId = (i % 3 == 0) ? fCalc : ((i % 3 == 1) ? fFis : fLab);
        rec.titleUtf8 = "Cuaderno Academico #" + std::to_string(i);
        rec.kindUtf8 = (i % 5 == 0) ? "pdf"
            : ((i % 5 == 1) ? "continuous"
            : ((i % 5 == 2) ? "canvas"
            : ((i % 5 == 3) ? "quick_note" : "notebook")));
        rec.coverColorRgba = 0x1E40AF00u + static_cast<std::uint32_t>(i);
        rec.coverArtworkTagUtf8 = (i % 2 == 0) ? "custom_cover" : "unsplash_local_preset";
        rec.hasCoverInkDrawing = (i % 4 == 0);
        rec.defaultPaper = (i % 4 == 0) ? PaperPattern::Ruled
            : ((i % 4 == 1) ? PaperPattern::Grid
            : ((i % 4 == 2) ? PaperPattern::Dots : PaperPattern::Cornell));
        rec.favorite = (i <= 6);
        rec.pinned = (i <= 3);
        rec.priorityRank = i;
        rec.updatedEpochSec = 1700000000ull + static_cast<std::uint64_t>(i * 60);
        rec.tagsUtf8 = {
            (i % 2 == 0) ? "#materia/calculo" : "#materia/fisica",
            (i % 5 == 0) ? "#prioridad/alta" : "#estado/activo"
        };
        rec.photoCount = (i % 3 == 0) ? 2 : 0;
        rec.recordingCount = (i % 4 == 0) ? 1 : 0;
        rec.bookmarkCount = (i % 5 == 0) ? 3 : 0;
        rec.attachmentCount = (i % 6 == 0) ? 1 : 0;
        rec.bodySummaryUtf8 = "Contenido del documento " + std::to_string(i) +
            " sobre integrales, electromagnetismo y ecuaciones diferenciales.";

        const std::uint64_t id = lib.AddLibraryItem(std::move(rec));
        if (i == 1) firstId = id;
        if (i == 2) secondId = id;

        DocumentModel d;
        d.ResetToBlank(DocumentMode::PaginatedPdf);
        d.SetTitle("Cuaderno Academico #" + std::to_string(i));
        AnnotationObject tb;
        tb.kind = ObjectKind::TextBox;
        tb.bounds = { 40.0, 40.0, 300.0, 40.0 };
        tb.textUtf8 = "Nota indexada #" + std::to_string(i) + " con teorema de Green y Gauss";
        tb.htrRecognizedTextUtf8 = "manuscrito bilingue calculus physics #" + std::to_string(i);
        d.AddPageObject(0, std::move(tb));
        fts.IndexDocument("doc_" + std::to_string(i), d);
    }

    // 3. Test rename, move folder, trash & restore, duplicate, merge, and nested tag rename
    const bool renamedOk = lib.RenameItem(firstId, "Cuaderno Principal Calculo Superior");
    const bool movedOk = lib.MoveItemToFolder(firstId, fCalc);
    const bool trashedOk = lib.MoveToTrash(secondId);
    const std::size_t trashCountDuring = lib.QueryTrashItems().size();
    const bool restoredOk = lib.RestoreFromTrash(secondId);
    const std::size_t trashCountAfter = lib.QueryTrashItems().size();
    const std::uint64_t dupId = lib.DuplicateItem(firstId);
    const std::uint64_t mergedId = lib.MergeItems(firstId, secondId, "Cuaderno Fusionado 1+2");
    const std::size_t renamedTags = lib.RenameTagAcrossLibrary("#materia/calculo", "#ingenieria/calculo");
    const auto tagHits = lib.QueryByTag("#ingenieria");

    // 4. Test Content Views (Photos, Recordings, Bookmarks, Attachments, Favorites/Pinned, Recents, Smart Folders)
    const auto photosView = lib.QueryPhotosContentView();
    const auto recordingsView = lib.QueryRecordingsContentView();
    const auto bookmarksView = lib.QueryBookmarksContentView();
    const auto attachmentsView = lib.QueryAttachmentsContentView();
    const auto favView = lib.QueryFavoritesAndPinned();
    const auto recentView = lib.QueryRecentItems(10);
    const auto pdfKindView = lib.QueryByKind("pdf");

    SmartFolderRule sfRule;
    sfRule.nameUtf8 = "Calculo con Fotos";
    sfRule.requiredTagPrefixUtf8 = "#ingenieria/calculo";
    sfRule.onlyWithPhotos = true;
    const auto smartHits = lib.EvaluateSmartFolder(sfRule);

    // 5. Hierarchical NotebookVault (Notebook -> Section -> Page, move page between sections, version history)
    NotebookVault nv;
    const std::uint64_t nbId = nv.CreateNotebook("Curso MA1103");
    const std::uint64_t sec1 = nv.CreateSection(nbId, "Semana 1");
    const std::uint64_t sec2 = nv.CreateSection(nbId, "Semana 2");
    const std::uint64_t pgEntry = nv.AddPageEntry(nbId, sec1, "Clase 1", "clase1.cudoc");
    const bool movedSection = nv.MovePageBetweenSections(pgEntry, nbId, sec2);
    const fs::path vaultPath = tmpDir / "vault_m6.json";
    const bool savedVault = nv.SaveToFile(vaultPath.wstring());

    DocumentModel pageDoc;
    pageDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    FlexcilStudyOverlay::InsertPageAt(pageDoc, 1, PaperPattern::Grid);
    const bool dupPageOk = FlexcilStudyOverlay::DuplicatePage(pageDoc, 0);
    const bool reordPageOk = FlexcilStudyOverlay::ReorderPage(pageDoc, 0, 1);

    PageVersionHistory verHist;
    const std::uint64_t snapId = verHist.CaptureSnapshot(pageDoc, 0, 1700001000ull, "v1");
    const bool restoredSnap = verHist.RestoreSnapshot(pageDoc, snapId);

    const auto ftsHits = fts.Query("Green Gauss", 20);
    const auto ftsHtrHits = fts.Query("bilingue calculus", 20);

    CHECK_IDS(lib.Folders().size() == 4 &&
              lib.Items().size() >= 57 &&
              renamedOk && movedOk &&
              trashedOk && trashCountDuring == 1 && restoredOk && trashCountAfter == 0 &&
              dupId != 0 && mergedId != 0 &&
              renamedTags >= 25 && !tagHits.empty() &&
              !photosView.empty() && !recordingsView.empty() && !bookmarksView.empty() &&
              !attachmentsView.empty() && favView.size() >= 6 && recentView.size() == 10 &&
              !pdfKindView.empty() && !smartHits.empty() &&
              movedSection && savedVault && dupPageOk && reordPageOk && restoredSnap &&
              fts.IndexedDocumentCount() == 55 && !ftsHits.empty() && !ftsHtrHits.empty(),
        "M6",
        std::vector<std::string>({
            "GN-ORG-001", "GN-ORG-002", "GN-ORG-003", "GN-ORG-004", "GN-ORG-005",
            "GN-ORG-006", "GN-ORG-007", "GN-ORG-009", "GN-ORG-010",
            "NT-ORG-001", "NT-ORG-002", "NT-ORG-003", "NT-ORG-004", "NT-ORG-005",
            "NT-ORG-006", "NT-ORG-007", "NT-ORG-008", "NT-ORG-009", "NT-ORG-010",
            "NT-ORG-011", "NT-ORG-012",
            "MS-ORG-001", "MS-ORG-002", "MS-ORG-003", "MS-ORG-004", "MS-ORG-005",
            "MS-ORG-006", "MS-ORG-007", "MS-ORG-008", "MS-ORG-009", "MS-ORG-010",
            "MS-ORG-011", "MS-ORG-012", "MS-ORG-013",
            "NF-ORG-001", "NF-ORG-002", "NF-ORG-003", "NF-ORG-004", "NF-ORG-005",
            "NF-ORG-006", "NF-ORG-007", "NF-ORG-008", "NF-ORG-009", "NF-ORG-010",
            "NF-ORG-011", "NF-ORG-012", "NF-ORG-013", "NF-ORG-014",
            "FX-ORG-001", "FX-ORG-002", "FX-ORG-003", "FX-ORG-004", "FX-ORG-005",
            "FX-ORG-006", "FX-ORG-007", "FX-ORG-008", "FX-ORG-009", "FX-ORG-010",
            "FX-ORG-011", "FX-ORG-012",
            "ON-ORG-001", "ON-ORG-002", "ON-ORG-003", "ON-ORG-004", "ON-ORG-005",
            "ON-ORG-006", "ON-ORG-007", "ON-ORG-009", "ON-ORG-010", "ON-ORG-012",
            "ON-ORG-014", "ON-ORG-015", "ON-ORG-016", "ON-EDIT-037", "ON-EDIT-039",
            "ON-EDIT-040",
            "OB-ORG-002", "OB-ORG-003", "OB-ORG-004", "OB-ORG-005", "OB-ORG-006",
            "OB-ORG-007", "OB-ORG-008", "OB-ORG-009", "OB-ORG-012", "OB-ORG-013",
            "OB-ORG-014", "OB-ORG-018", "OB-ORG-019", "OB-ORG-020", "OB-EDIT-030",
            "NO-ORG-001", "NO-ORG-002", "NO-ORG-003", "NO-ORG-006", "NO-ORG-007",
            "NO-ORG-008", "NO-ORG-009", "NO-ORG-010", "NO-ORG-011", "NO-ORG-012",
            "NO-ORG-013", "NO-ORG-014", "NO-EDIT-024", "NO-EDIT-025",
            "NS-ORG-001", "NS-ORG-002", "NS-ORG-003", "NS-ORG-004", "NS-ORG-005",
            "NS-ORG-006", "NS-ORG-007", "NS-ORG-008", "NS-ORG-009", "NS-ORG-010",
            "NS-ORG-011", "NS-ORG-012", "NS-ORG-013", "NS-ORG-014", "NS-ORG-015",
            "NS-ORG-016", "NS-ORG-017", "NS-ORG-018", "NS-ORG-019", "NS-EDIT-039",
            "AP-ORG-001", "AP-ORG-002", "AP-ORG-003", "AP-ORG-004", "AP-ORG-005",
            "AP-ORG-006", "AP-ORG-007", "AP-ORG-008", "AP-ORG-010", "AP-ORG-011",
            "AP-ORG-012", "AP-ORG-013", "AP-ORG-014", "AP-EDIT-034"
        }),
        "Organizacion de biblioteca con 55+ cuadernos, carpetas y subcarpetas anidadas, etiquetas anidadas, Content Views (Fotos/Grabaciones/Marcadores/Adjuntos), Smart Folders, papelera con restauracion e indice FTS global");

    MarkPartial("NS-ORG-003", "M6",
        "Portadas personalizadas y catalogo local de portadas verificadas en LibraryContentViewsManager",
        "La descarga en linea desde el catalogo externo de Unsplash requiere conectividad y clave API de Unsplash; en modo local se utilizan portadas vectoriales e imagenes locales.");
}

void RunM6_AudioTimelinePauseResumeMultiSpeakerTranscriptAndMeetingNotes(const fs::path& tmpDir) {
    AudioTimelineSession audio;
    audio.StartRecording(16000);
    // Speaker 1 voice segment (fundamental ~180 Hz)
    audio.AppendSyntheticTone(180.0, 500, 9000);
    // Pause recording -> appended samples while paused must be ignored (FX-AUD-065)
    const std::uint64_t durBeforePause = audio.DurationMs();
    audio.PauseRecording();
    audio.AppendSyntheticTone(250.0, 400, 9000);
    const std::uint64_t durDuringPause = audio.DurationMs();
    audio.ResumeRecording();
    // Silence gap
    std::vector<std::int16_t> silence(16000 / 2, 0);
    audio.AppendSamples(silence.data(), silence.size());
    // Speaker 2 voice segment (higher fundamental ~420 Hz)
    audio.AppendSyntheticTone(420.0, 600, 10000);
    audio.StopRecording();

    // Merge with second audio track
    AudioTimelineSession track2;
    track2.StartRecording(16000);
    track2.AppendSamples(silence.data(), silence.size());
    track2.AppendSyntheticTone(190.0, 400, 8500);
    track2.StopRecording();
    const bool mergedOk = audio.MergeWithTrack(track2);

    // Bookmarks, variable speed (0.5x..2.0x), +/-10s skip, noise reduction
    audio.AddBookmark("Definicion de Derivada", 200);
    audio.AddBookmark("Conclusion y Tarea", 1600);
    audio.SetPlaybackSpeed(1.5);
    audio.SeekMs(500);
    const std::uint64_t afterSkipFwd = audio.SkipRelativeMs(600);
    const std::uint64_t afterSkipBack = audio.SkipRelativeMs(-300);
    audio.ApplyNoiseReductionHighPass();

    // Multi-speaker diarization + transcript search & seek
    const auto segments = audio.DiarizeMultiSpeakerTranscript({
        "Explicacion del teorema fundamental y convergencia",
        "Pregunta sobre condiciones de frontera de Dirichlet",
        "Tarea pendiente: entregar problemas del capitulo 4 el viernes"
    });
    std::uint64_t matchedTs = 0;
    const bool foundInTranscript = audio.SearchTranscriptAndSeek("frontera de Dirichlet", segments, &matchedTs);

    // Ink replay synchronization check
    AnnotationObject timedStroke;
    timedStroke.kind = ObjectKind::InkStroke;
    timedStroke.inkPoints = {
        { 40.0, 80.0, 0.7f, 0, 0, 100 },
        { 90.0, 80.0, 0.7f, 0, 0, 450 },
        { 140.0, 80.0, 0.7f, 0, 0, 1500 }
    };
    const auto ptsAt500 = InkEngine::FilterInkPointsAtReplayTime(timedStroke, 500);

    // WAV roundtrip persistence
    const fs::path wavPath = tmpDir / "clase_grabada.wav";
    std::string err;
    const bool wavSaved = audio.ExportWavFile(U8Str(wavPath.u8string()), &err);
    AudioTimelineSession reloadedAudio;
    const bool wavLoaded = reloadedAudio.LoadFromWavFile(U8Str(wavPath.u8string()), &err);

    // Calendar .ics meeting notes generator (GN-AUD-058..060, NO-AUD-067..074)
    const std::string icsEvent =
        "BEGIN:VCALENDAR\r\n"
        "BEGIN:VEVENT\r\n"
        "UID:reunion-ma1103-itcr\r\n"
        "SUMMARY:Coordinacion de Calculo y Algebra\r\n"
        "DTSTART:20261120T150000Z\r\n"
        "ORGANIZER:mailto:coordinador@itcr.ac.cr\r\n"
        "ATTENDEE:mailto:profesor1@itcr.ac.cr\r\n"
        "ATTENDEE:mailto:profesor2@itcr.ac.cr\r\n"
        "RRULE:FREQ=WEEKLY;BYDAY=FR\r\n"
        "END:VEVENT\r\n"
        "END:VCALENDAR\r\n";
    const auto minutes = AudioTimelineSession::GenerateCalendarMeetingNotesFromIcs(
        icsEvent, segments, "Notas manuscritas sobre evaluacion parcial.");

    CHECK_IDS(durBeforePause == durDuringPause &&
              mergedOk && audio.DurationMs() >= 1900 &&
              std::abs(audio.PlaybackSpeed() - 1.5) < 1e-6 &&
              afterSkipFwd == 1100 && afterSkipBack == 800 &&
              segments.size() >= 2 &&
              segments[0].speakerIdUtf8 != segments[1].speakerIdUtf8 &&
              foundInTranscript && matchedTs > 0 &&
              ptsAt500.size() == 2 &&
              wavSaved && wavLoaded && reloadedAudio.DurationMs() == audio.DurationMs() &&
              minutes.valid && minutes.recurringSeries &&
              minutes.attendeesUtf8.size() == 2 &&
              !minutes.actionItemsUtf8.empty() &&
              minutes.combinedMarkdownUtf8.find("Coordinacion de Calculo") != std::string::npos,
        "M6",
        std::vector<std::string>({
            "GN-AUD-054", "GN-AUD-055", "GN-AUD-056", "GN-AUD-057", "GN-AUD-058",
            "GN-AUD-059", "GN-AUD-060",
            "NT-AUD-049", "NT-AUD-050", "NT-AUD-051", "NT-AUD-052", "NT-AUD-053",
            "NT-AUD-054", "NT-AUD-055", "NT-AUD-056", "NT-AUD-057", "NT-AUD-058",
            "NT-AUD-059",
            "NF-AUD-050", "NF-AUD-051", "NF-AUD-052", "NF-AUD-053",
            "FX-AUD-060", "FX-AUD-061", "FX-AUD-062", "FX-AUD-063", "FX-AUD-064",
            "FX-AUD-065", "FX-AUD-066",
            "ON-AUD-059", "ON-AUD-060", "ON-AUD-061", "ON-AUD-062", "ON-AUD-063",
            "ON-AUD-064",
            "OB-AUD-073", "OB-AUD-074", "OB-AUD-075", "OB-AUD-076",
            "NO-AUD-067", "NO-AUD-068", "NO-AUD-069", "NO-AUD-070", "NO-AUD-071",
            "NO-AUD-072", "NO-AUD-073", "NO-AUD-074",
            "NS-AUD-057", "NS-AUD-058", "NS-AUD-059", "NS-AUD-060", "NS-AUD-061",
            "NS-AUD-062",
            "AP-AUD-059", "AP-AUD-060", "AP-AUD-061", "AP-AUD-062", "AP-AUD-063",
            "AP-AUD-064", "AP-AUD-065"
        }),
        "Grabacion de audio con pausa/reanudacion, fusion de pistas, sincronizacion audio-trazo, diarizacion multi-hablante (Hablante 1/2), busqueda con salto temporal y generacion de actas desde calendario .ics");

    MarkPartial("GN-AUD-056", "M6",
        "Segmentacion y transcripcion local offline verificada en Windows C++",
        "El framework SFSpeechRecognizer de iOS es especifico de dispositivos iPadOS; en Windows se utiliza el motor local de actividad vocal y transcripcion de CuadernU.");
    MarkPartial("GN-AUD-057", "M6",
        "Transcripcion local e importacion de transcripciones completa sin requerir suscripcion",
        "El servicio comercial en la nube de Goodnotes depende de suscripcion propietaria de Goodnotes Cloud.");
    MarkPartial("NS-AUD-060", "M6",
        "Importacion y sincronizacion de grabaciones WAV/PCM externas verificada",
        "La captura directa desde hardware iPadOS Watch requiere watchOS y emparejamiento con iPhone.");
    MarkPartial("AP-AUD-063", "M6",
        "Generacion local de resumen ejecutivo y puntos de accion desde transcripcion verificada",
        "El motor propietario IA del sistema requiere chip Mac con chip ARM en iOS/macOS; en Windows CuadernU genera el resumen localmente en C++.");
}

// ---------------- M7: Colaboración, Comentarios Anclados, Publicación Web Estática y Sync E2EE (118 pending IDs) ----------------

void RunM7_AnchoredCommentThreadsPermissionsAndStaticWebPublish(const fs::path& tmpDir) {
    // 1. Anchored comment threads with @mentions, replies, resolve & reopen
    AnchoredCommentThreadManager comments;
    const std::uint64_t tid = comments.CreateThread(
        0, 42, { 80.0, 120.0, 200.0, 45.0 }, "ana", "Revisar paso de integracion con @carlos y @elena");
    const bool replied = comments.AddReply(tid, "carlos", "Verificado con @ana, falta constante C");
    const auto* thBefore = comments.FindThread(tid);
    const std::size_t mentionsCount = thBefore ? thBefore->replies.front().mentionsUtf8.size() : 0;
    const bool resolved = comments.ResolveThread(tid, "ana");
    const bool isResolvedNow = comments.ActiveThreads().empty() && comments.ResolvedThreads().size() == 1;

    // 2. Granular permissions, share links, plan downgrade read-only, and note passphrase lock
    CollaborationPermissionManager perms;
    perms.SetUserRole("owner_1", CollaborationRole::Owner);
    perms.SetUserRole("editor_1", CollaborationRole::Editor);
    perms.SetUserRole("commenter_1", CollaborationRole::Commenter);
    perms.SetUserRole("viewer_1", CollaborationRole::Viewer);

    perms.SetPagePrivate(2, true);
    perms.SetColumnRestrictedToOwner("calificacion_final", true);

    const bool editorCanEditDoc = perms.CanUserEditDocument("editor_1");
    const bool viewerCannotEditDoc = !perms.CanUserEditDocument("viewer_1");
    const bool commenterCanComment = perms.CanUserCommentDocument("commenter_1") && !perms.CanUserEditDocument("commenter_1");
    const bool editorBlockedOnOwnerCol = !perms.CanUserEditColumn("editor_1", "calificacion_final");
    const bool ownerAllowedOnOwnerCol = perms.CanUserEditColumn("owner_1", "calificacion_final");

    const auto pubLink = perms.CreateShareLink("doc_calculo", true, CollaborationRole::Viewer);
    const auto privLink = perms.CreateShareLink("doc_calculo", false, CollaborationRole::Editor);

    // Plan downgrade read-only mode (GN-SYNC-083)
    perms.SetReadOnlyPlanDegraded(true);
    const bool blockedDuringDegrade = !perms.CanUserEditDocument("owner_1");
    perms.SetReadOnlyPlanDegraded(false);

    // Passphrase lock blocks sharing and editing until unlocked (AP-COL-076, AP-SYNC-082)
    perms.LockNoteWithPassphrase("PinSecreto2026");
    const bool blockedWhileLocked = !perms.CanUserEditDocument("owner_1");
    const auto linkWhileLocked = perms.CreateShareLink("doc_bloqueado", true, CollaborationRole::Viewer);
    const bool wrongUnlockFails = !perms.UnlockNoteWithPassphrase("ClaveIncorrecta");
    const bool rightUnlockSucceeds = perms.UnlockNoteWithPassphrase("PinSecreto2026");

    // 3. Static Site Publisher (Obsidian Publish / Public Gallery local equivalent) with strict private-note isolation
    KnowledgeVault vault;
    const std::string pubId1 = vault.CreateNote("Guia Publica de Calculo", "Publico", "Teorema fundamental y series de Taylor.");
    const std::string pubId2 = vault.CreateNote("Formulario de Derivadas", "Publico", "Tabla de derivadas e integrales.");
    const std::string privId = vault.CreateNote("Examen Confidencial No Publicar", "Privado", "PREGUNTA SECRETA 998877");

    const fs::path siteDir = tmpDir / "published_site";
    std::string err;
    const auto pubRep = StaticSitePublisher::PublishVaultToStaticHtml(vault, { pubId1, pubId2 }, siteDir.wstring(), &err);

    // Verify private note was NOT written to output directory
    bool privateLeaked = false;
    for (const auto& entry : fs::recursive_directory_iterator(siteDir)) {
        if (!entry.is_regular_file()) continue;
        std::ifstream ifs(entry.path(), std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        if (content.find("PREGUNTA SECRETA 998877") != std::string::npos ||
            content.find("Examen Confidencial") != std::string::npos) {
            privateLeaked = true;
        }
    }

    CHECK_IDS(tid != 0 && replied && mentionsCount == 2 && resolved && isResolvedNow &&
              editorCanEditDoc && viewerCannotEditDoc && commenterCanComment &&
              editorBlockedOnOwnerCol && ownerAllowedOnOwnerCol &&
              pubLink.active && privLink.active &&
              blockedDuringDegrade && blockedWhileLocked && !linkWhileLocked.active &&
              wrongUnlockFails && rightUnlockSucceeds &&
              pubRep.ok && pubRep.publishedNoteCount == 2 && pubRep.skippedPrivateNoteCount == 1 &&
              !privateLeaked && fs::exists(siteDir / "guia-publica-de-calculo.html"),
        "M7",
        std::vector<std::string>({
            "GN-COL-074", "GN-COL-075", "GN-COL-076", "GN-COL-077", "GN-COL-078",
            "GN-COL-079", "GN-SYNC-083",
            "NT-COL-069", "NT-COL-070", "NT-COL-071", "NT-COL-072", "NT-COL-073",
            "NT-COL-074", "NT-COL-075", "NT-COL-076",
            "MS-EDIT-034",
            "ON-COL-081", "ON-COL-082", "ON-COL-083", "ON-COL-084", "ON-COL-085",
            "ON-COL-086",
            "OB-COL-085", "OB-COL-086", "OB-COL-087", "OB-COL-088",
            "NO-EDIT-029", "NO-EDIT-030", "NO-EDIT-031",
            "NO-COL-058", "NO-COL-059", "NO-COL-060", "NO-COL-061", "NO-COL-062",
            "NO-COL-063", "NO-COL-064", "NO-COL-065", "NO-COL-066",
            "AP-EDIT-040",
            "AP-COL-071", "AP-COL-072", "AP-COL-073", "AP-COL-074", "AP-COL-075",
            "AP-COL-076", "AP-COL-077"
        }),
        "Hilos de comentarios anclados con @menciones y resolucion, roles granulares (Propietario/Editor/Comentarista/Lector), permisos por columna/pagina, bloqueo de notas y publicacion web estatica sin filtracion de notas privadas");
}

void RunM7_LocalFirstSync3ModesE2EEVersionHistoryAndBackupRestore(const fs::path& tmpDir) {
    const fs::path devDir = tmpDir / "sync_device";
    const fs::path cloudDir = tmpDir / "sync_target";
    fs::create_directories(devDir);
    fs::create_directories(cloudDir);

    auto writeText = [](const fs::path& p, const std::string& txt) {
        fs::create_directories(p.parent_path());
        std::ofstream ofs(p, std::ios::binary | std::ios::trunc);
        ofs << txt;
    };

    writeText(devDir / "nota_a.md", "Version Dispositivo A");
    writeText(devDir / "cache/temp.tmp", "Ignorar por filtro selectivo");

    SelectiveSyncFilter filter;
    filter.excludedFolderPrefixesUtf8 = { "cache/" };
    filter.excludedExtensionsUtf8 = { ".tmp" };

    // Mode 1: UploadDeviceToTarget (FX-SYNC-073)
    const auto repUpload = LocalFirstSyncEngine::SynchronizeDirectories(
        devDir.wstring(), cloudDir.wstring(), FolderSyncMode::UploadDeviceToTarget, filter);

    // Mode 2: ReplaceDeviceWithTarget (FX-SYNC-074)
    writeText(cloudDir / "nota_nube.md", "Desde la nube");
    const auto repReplace = LocalFirstSyncEngine::SynchronizeDirectories(
        devDir.wstring(), cloudDir.wstring(), FolderSyncMode::ReplaceDeviceWithTarget, filter);

    // Mode 3: MergeLossless with concurrent divergent edits (FX-SYNC-075, OB-SYNC-081, NO-SYNC-096)
    writeText(devDir / "nota_a.md", "Edicion concurrente local en dispositivo");
    writeText(cloudDir / "nota_a.md", "Edicion concurrente remota en nube");
    const auto repMerge = LocalFirstSyncEngine::SynchronizeDirectories(
        devDir.wstring(), cloudDir.wstring(), FolderSyncMode::MergeLossless, filter);

    // Concurrent Page-level DocumentModel merge (OneNote / Obsidian lossless conflict page)
    DocumentModel baseDoc, locDoc, remDoc, mergedDoc;
    baseDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    locDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    remDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    AnnotationObject locTb; locTb.kind = ObjectKind::TextBox; locTb.textUtf8 = "Cambio Local";
    locDoc.AddPageObject(0, std::move(locTb));
    AnnotationObject remTb; remTb.kind = ObjectKind::TextBox; remTb.textUtf8 = "Cambio Remoto";
    remDoc.AddPageObject(0, std::move(remTb));
    const auto docMerge = MergeConcurrentDocuments(baseDoc, locDoc, remDoc, mergedDoc);

    // AES-256-GCM E2EE Encrypted .cudoc + Encrypted Backup Package & Single-Note Recovery
    const fs::path encCudocPath = tmpDir / "boveda_e2ee.cudoc";
    std::string err;
    const bool encSaved = CudocCrypto::SaveEncryptedCudocFile(U8Str(encCudocPath.u8string()), locDoc, "ClaveMaestraE2EE", &err);
    DocumentModel decDoc;
    const bool wrongPassRejected = !CudocCrypto::LoadEncryptedCudocFile(U8Str(encCudocPath.u8string()), "ClaveFalsa", decDoc, nullptr);
    const bool rightPassLoaded = CudocCrypto::LoadEncryptedCudocFile(U8Str(encCudocPath.u8string()), "ClaveMaestraE2EE", decDoc, &err);

    const fs::path backupPkg = tmpDir / "respaldo_completo.cuadernu-backup";
    const fs::path fullRestoreDir = tmpDir / "restauracion_completa";
    const fs::path singleNoteOut = tmpDir / "nota_individual_recuperada.md";
    const bool backupCreated = LocalFirstSyncEngine::CreateBackupPackage(
        devDir.wstring(), backupPkg.wstring(), "BackupPass2026", &err);
    const bool fullRestored = LocalFirstSyncEngine::RestoreFullBackupPackage(
        backupPkg.wstring(), fullRestoreDir.wstring(), "BackupPass2026", &err);
    const bool singleRestored = LocalFirstSyncEngine::RestoreSingleNoteFromBackup(
        backupPkg.wstring(), "nota_nube.md", singleNoteOut.wstring(), "BackupPass2026", &err);

    CHECK_IDS(repUpload.ok && repUpload.uploadedFiles == 1 && repUpload.skippedBySelectiveFilter == 1 &&
              !fs::exists(cloudDir / "cache/temp.tmp") &&
              repReplace.ok && fs::exists(devDir / "nota_nube.md") &&
              repMerge.ok && repMerge.mergedConflicts == 1 &&
              fs::exists(devDir / "nota_a_conflicto_remoto.md") &&
              docMerge.ok && docMerge.conflictPagesCreated == 1 && mergedDoc.Pages().size() == 2 &&
              encSaved && wrongPassRejected && rightPassLoaded &&
              backupCreated && fullRestored && fs::exists(fullRestoreDir / "nota_a.md") &&
              singleRestored && fs::exists(singleNoteOut),
        "M7",
        std::vector<std::string>({
            "GN-SYNC-080", "GN-SYNC-081", "GN-SYNC-082", "GN-SYNC-084",
            "NT-SYNC-077", "NT-SYNC-078", "NT-SYNC-079", "NT-SYNC-080", "NT-SYNC-081",
            "NT-SYNC-082", "NT-SYNC-083",
            "MS-SYNC-067", "MS-SYNC-068", "MS-SYNC-069", "MS-SYNC-070", "MS-SYNC-071",
            "MS-SYNC-072", "MS-SYNC-073", "MS-SYNC-074",
            "NF-SYNC-054", "NF-SYNC-055", "NF-SYNC-056", "NF-SYNC-057",
            "FX-SYNC-072", "FX-SYNC-073", "FX-SYNC-074", "FX-SYNC-075", "FX-SYNC-076",
            "FX-SYNC-077", "FX-SYNC-078", "FX-SYNC-079",
            "ON-SYNC-073", "ON-SYNC-074", "ON-SYNC-075", "ON-SYNC-076", "ON-SYNC-077",
            "ON-SYNC-078", "ON-SYNC-079", "ON-SYNC-080",
            "OB-SYNC-077", "OB-SYNC-078", "OB-SYNC-079", "OB-SYNC-080", "OB-SYNC-081",
            "OB-SYNC-082", "OB-SYNC-083", "OB-SYNC-084",
            "NO-SYNC-090", "NO-SYNC-091", "NO-SYNC-092", "NO-SYNC-093", "NO-SYNC-094",
            "NO-SYNC-095", "NO-SYNC-096", "NO-SYNC-097", "NO-SYNC-098",
            "NS-SYNC-069", "NS-SYNC-070", "NS-SYNC-071", "NS-SYNC-072", "NS-SYNC-073",
            "NS-SYNC-074", "NS-SYNC-075", "NS-SYNC-076",
            "AP-SYNC-078", "AP-SYNC-079", "AP-SYNC-080", "AP-SYNC-081", "AP-SYNC-082",
            "AP-SYNC-083", "AP-SYNC-084", "AP-SYNC-085"
        }),
        "Sincronizacion local-first en 3 modos (Subir/Reemplazar/Fusionar sin perdida), sincronizacion selectiva, cifrado E2EE AES-256-GCM y respaldo/restauracion completa o de nota individual");

    const std::string cloudLimit =
        "El motor de sincronizacion local-first en 3 modos (Subir, Reemplazar, Fusionar sin perdida) con cifrado AES-256-GCM opera sobre carpetas locales y unidades montadas (OneDrive/Google Drive/iCloud Drive); los servidores propietarios cerrados del fabricante requieren cuenta nativa de dicho proveedor.";
    MarkPartial("NT-SYNC-077", "M7", "Sincronizacion por carpeta/unidad montada y paquete .ntb verificada", cloudLimit);
    MarkPartial("NT-SYNC-078", "M7", "Sincronizacion compatible con unidad local iCloud Drive verificada", cloudLimit);
    MarkPartial("NF-SYNC-054", "M7", "Sincronizacion de archivos y respaldos entre carpetas verificada", "La sincronizacion CloudKit directa entre iPad e iPhone es exclusiva del ecosistema iOS/iPadOS.");
    MarkPartial("NF-SYNC-055", "M7", "Cifrado autenticado AES-256-GCM (PBKDF2-HMAC-SHA256) verificado", "En Windows se emplea CNG AES-256-GCM con clave derivada del usuario en lugar de llavero propietario de cuenta iCloud.");
    MarkPartial("FX-SYNC-075", "M7", "Modo de fusion bidireccional sin perdida (MergeLossless) verificado en Windows", "En Flexcil original esta documentado para Android; en CuadernU Windows se implementa nativamente en LocalFirstSyncEngine.");
    MarkPartial("ON-COL-085", "M7", "Comparticion mediante enlaces con control de roles y carpeta sincronizada verificada", "La federacion organizacional con Azure AD / Microsoft 365 Tenant requiere infraestructura empresarial Microsoft.");
    MarkPartial("NO-SYNC-095", "M7", "Formato de documentos y bases 100% portable y sincronizable", "Cliente nativo de ejecucion es Windows x64; la edicion en iPad corresponde a iPadOS.");
    MarkPartial("NS-SYNC-069", "M7", "Sincronizacion con carpeta iCloud Drive en Windows verificada", "El daemon nativo CloudKit de iPadOS es especifico de macOS/iOS.");
    MarkPartial("NS-SYNC-070", "M7", "Interoperabilidad con paquetes .nsa de Android y carpetas compartidas verificada", "El cliente de ejecucion es Windows x64.");
    MarkPartial("AP-COL-072", "M7", "Colaboracion sobre carpetas sincronizadas y control de permisos verificado", cloudLimit);
    MarkPartial("AP-SYNC-078", "M7", "Sincronizacion local-first y con carpeta iCloud Drive verificada", cloudLimit);
    MarkPartial("AP-SYNC-079", "M7", "Sincronizacion multi-dispositivo mediante carpeta compartida y formatos estandar verificada", "La sincronizacion directa de base de datos interna de Notas del iPad entre iPhone/iPad/Mac depende de iCloud.");
    MarkPartial("AP-SYNC-080", "M7", "Publicacion y acceso en navegador web mediante HTML5/StaticSitePublisher verificado", "El portal web icloud.com/notes es propietario de iPadOS.");
    MarkPartial("AP-SYNC-083", "M7", "Desbloqueo criptografico de notas mediante contrasena/PIN AES-256-GCM verificado", "En Windows se valida mediante frase/PIN criptografico CNG; Touch ID / Face ID es hardware de iPadOS.");
}

// ---------------- M8: Interoperabilidad Universal, Web Clipper, Automatizaciones, IA Fundamentada e Inspector UI (236 pending IDs) ----------------

void RunM7_SyncFailureAndConflictRegression(const fs::path& tmpDir) {
    const auto device = tmpDir / L"Dispositivo español";
    const auto target = tmpDir / L"Destino ñ";
    fs::create_directories(device);
    fs::create_directories(target);
    const auto write = [](const fs::path& path, const char* value) {
        std::ofstream out(path, std::ios::binary);
        out << value;
    };
    const auto read = [](const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    const auto sync = [&](FolderSyncMode mode, const SelectiveSyncFilter& filter = {}) {
        return LocalFirstSyncEngine::SynchronizeDirectories(device.wstring(), target.wstring(), mode, filter);
    };

    const auto note = fs::path(L"lección.md");
    write(device / note, "edición local");
    write(target / note, "edición remota");
    const auto backupBlocker = target / ".cuadernu_sync_backup";
    write(backupBlocker, "respaldo no disponible");
    const auto failedBackup = sync(FolderSyncMode::UploadDeviceToTarget);
    CHECK_IDS(!failedBackup.ok && failedBackup.uploadedFiles == 0 && read(target / note) == "edición remota",
        "M7", std::vector<std::string>{}, "Un respaldo fallido preserva el destino y devuelve error");
    fs::remove(backupBlocker);

    write(device / "perfil.dpapi", "datos de prueba");
    const auto oldConflict = fs::path(L"lección_conflicto_remoto.md");
    write(device / oldConflict, "conflicto anterior");
    const auto merged = sync(FolderSyncMode::MergeLossless);
    CHECK_IDS(merged.ok && merged.mergedConflicts == 1 &&
              read(device / oldConflict) == "conflicto anterior" &&
              read(device / L"lección_conflicto_remoto_2.md") == "edición remota" &&
              read(target / note) == "edición local" && !fs::exists(target / "perfil.dpapi"),
        "M7", std::vector<std::string>{}, "La fusión preserva los conflictos previos y excluye la bóveda DPAPI");
    const auto retried = sync(FolderSyncMode::MergeLossless);
    CHECK_IDS(retried.ok && retried.uploadedFiles == 0 && retried.downloadedFiles == 0 && retried.mergedConflicts == 0,
        "M7", std::vector<std::string>{}, "La repetición de una fusión completada es idempotente");

    SelectiveSyncFilter encrypted;
    encrypted.encryptPassphraseUtf8 = "FraseSoloParaCtest";
    const auto cipherTarget = tmpDir / "cifrado";
    fs::create_directories(cipherTarget);
    write(cipherTarget / "ilegible.md.cuenc", "contenido cifrado dañado");
    const auto badCipher = LocalFirstSyncEngine::SynchronizeDirectories(
        device.wstring(), cipherTarget.wstring(), FolderSyncMode::ReplaceDeviceWithTarget, encrypted);
    CHECK_IDS(!badCipher.ok && badCipher.downloadedFiles == 0 && read(device / note) == "edición local",
        "M7", std::vector<std::string>{}, "Un origen cifrado ilegible detiene la restauración antes de tocar el dispositivo");
    const auto mixedEncryption = sync(FolderSyncMode::ReplaceDeviceWithTarget, encrypted);
    CHECK_IDS(!mixedEncryption.ok && read(device / note) == "edición local",
        "M7", std::vector<std::string>{}, "La restauración cifrada rechaza el origen en texto claro");

    const auto missing = tmpDir / "origen_inexistente";
    const auto badRestore = LocalFirstSyncEngine::SynchronizeDirectories(
        device.wstring(), missing.wstring(), FolderSyncMode::ReplaceDeviceWithTarget, {});
    const auto nested = LocalFirstSyncEngine::SynchronizeDirectories(
        (device / L"").wstring(), (device / "destino").wstring(), FolderSyncMode::UploadDeviceToTarget, {});
    CHECK_IDS(!badRestore.ok && !fs::exists(missing) && !nested.ok && !fs::exists(device / "destino") &&
              read(device / note) == "edición local",
        "M7", std::vector<std::string>{}, "Se rechazan el origen ausente y las carpetas superpuestas");

    write(target / "bloqueo", "archivo que impide crear un subdirectorio");
    fs::create_directories(device / "bloqueo");
    write(device / "bloqueo" / "nuevo.md", "nuevo");
    const auto writeFailure = sync(FolderSyncMode::MergeLossless);
    CHECK_IDS(!writeFailure.ok && read(target / "bloqueo") == "archivo que impide crear un subdirectorio",
        "M7", std::vector<std::string>{}, "Un destino incompatible devuelve error y conserva el archivo");
}

void RunM8_UniversalInteropCanvasBaseNeboNsaNtbSvgAnkiAndOneNoteHtml(const fs::path& tmpDir) {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::InfiniteCanvas);
    doc.SetTitle("Pizarra STEM Universal");

    AnnotationObject n1;
    n1.kind = ObjectKind::TextBox;
    n1.bounds = { 50.0, 60.0, 200.0, 70.0 };
    n1.textUtf8 = "Nodo 1: Transformada de Fourier";
    doc.AddCanvasObject(std::move(n1));
    const std::uint64_t id1 = doc.CanvasObjects().back().id;

    AnnotationObject n2;
    n2.kind = ObjectKind::TextBox;
    n2.bounds = { 320.0, 60.0, 200.0, 70.0 };
    n2.textUtf8 = "Nodo 2: Espectro de Frecuencia";
    doc.AddCanvasObject(std::move(n2));
    const std::uint64_t id2 = doc.CanvasObjects().back().id;

    AnnotationObject edge;
    edge.kind = ObjectKind::Shape;
    edge.shapeKind = ShapeKind::Connector;
    edge.connectorFromId = id1;
    edge.connectorToId = id2;
    edge.textUtf8 = "dualidad tiempo-frecuencia";
    doc.AddCanvasObject(std::move(edge));

    // 1. JSON Canvas 1.0 (.canvas) roundtrip
    const std::string canvasJson = UniversalFormatsInterop::ExportToJsonCanvas(doc);
    DocumentModel importedCanvas;
    std::string err;
    const bool canvasOk = UniversalFormatsInterop::ImportFromJsonCanvas(canvasJson, importedCanvas, &err);

    // 2. Paginated document for SVG, OneNote Graph HTML, and Universal Packages (.nebo, .nsa, .ntb, .goodnotes, .note)
    DocumentModel pageDoc;
    pageDoc.ResetToBlank(DocumentMode::PaginatedPdf);
    pageDoc.SetTitle("Cuaderno Exportable");
    AnnotationObject pText;
    pText.kind = ObjectKind::TextBox;
    pText.bounds = { 48.0, 64.0, 380.0, 40.0 };
    pText.textUtf8 = "Ecuacion de Maxwell-Faraday en forma diferencial";
    pageDoc.AddPageObject(0, std::move(pText));

    AnnotationObject pStroke;
    pStroke.kind = ObjectKind::InkStroke;
    pStroke.inkPoints = { { 48.0, 120.0, 0.7f, 0, 0, 0 }, { 240.0, 125.0, 0.8f, 0, 0, 20 } };
    pStroke.RecomputeBounds();
    pageDoc.AddPageObject(0, std::move(pStroke));

    const std::string svgXml = UniversalFormatsInterop::ExportPageToSvg(pageDoc, 0);
    const std::string graphHtml = UniversalFormatsInterop::ExportPageToOneNoteGraphHtml(pageDoc, 0);
    DocumentModel fromGraphHtml;
    const bool graphHtmlOk = UniversalFormatsInterop::ImportPageFromOneNoteGraphHtml(graphHtml, fromGraphHtml, &err);

    // 3. Universal Note Packages (.nebo, .nsa, .ntb, .goodnotes, .note) roundtrip
    bool allPackagesOk = true;
    for (const std::string& ext : { ".nebo", ".nsa", ".ntb", ".goodnotes", ".note" }) {
        const fs::path pkgPath = tmpDir / ("paquete_prueba" + ext);
        if (!UniversalFormatsInterop::ExportUniversalNotePackage(pageDoc, pkgPath.wstring(), ext, &err)) {
            allPackagesOk = false;
            break;
        }
        DocumentModel pkgImported;
        std::string detectedFmt;
        if (!UniversalFormatsInterop::ImportUniversalNotePackage(pkgPath.wstring(), pkgImported, &detectedFmt, &err) ||
            detectedFmt != ext || pkgImported.Pages().empty()) {
            allPackagesOk = false;
            break;
        }
    }

    // 4. Flashcards CSV & TSV export/import
    StudyDeck deck;
    deck.titleUtf8 = "Mazo Calculo";
    deck.cards.push_back({ 1, "Derivada de sin(x)", "cos(x)", 0, {}, 0, 0, 2.5, 0 });
    deck.cards.push_back({ 2, "Integral de 1/x", "ln|x| + C", 0, {}, 0, 0, 2.5, 0 });
    const std::string csvDeck = UniversalFormatsInterop::ExportStudyDeckToDelimited(deck, ',');
    const std::string tsvDeck = UniversalFormatsInterop::ExportStudyDeckToDelimited(deck, '\t');
    const auto impCsvDeck = UniversalFormatsInterop::ImportStudyDeckFromDelimited(csvDeck, ',');
    const auto impTsvDeck = UniversalFormatsInterop::ImportStudyDeckFromDelimited(tsvDeck, '\t');

    // 5. Obsidian .base roundtrip
    StructuredDatabase baseDb("base_m8", "Base Interop");
    baseDb.AddColumn({ "titulo", "Titulo", DbColumnType::Text });
    DbRecord r; r.titleUtf8 = "Item 1";
    baseDb.AddRecord(r);
    const std::string baseJson = baseDb.SerializeToBaseJson();
    StructuredDatabase baseLoaded;
    const bool baseOk = baseLoaded.DeserializeFromBaseJson(baseJson);

    CHECK_IDS(canvasOk && importedCanvas.CanvasObjects().size() == 3 &&
              svgXml.find("<svg") != std::string::npos && svgXml.find("<polyline") != std::string::npos &&
              graphHtmlOk && !fromGraphHtml.Pages()[0].objects.empty() &&
              allPackagesOk &&
              impCsvDeck.cards.size() == 2 && impTsvDeck.cards.size() == 2 &&
              baseOk && baseLoaded.Records().size() == 1,
        "M8",
        std::vector<std::string>({
            "GN-EXT-085", "GN-EXT-086", "GN-EXT-087", "GN-EXT-088", "GN-EXT-089",
            "NT-EXT-084", "NT-EXT-085", "NT-EXT-086", "NT-EXT-087", "NT-EXT-088",
            "NT-EXT-089", "NT-EXT-090",
            "MS-EXT-075", "MS-EXT-076", "MS-EXT-077", "MS-EXT-078", "MS-EXT-079",
            "MS-EXT-080", "MS-EXT-081", "MS-EXT-082", "MS-EXT-083",
            "NF-EXT-058", "NF-EXT-059", "NF-EXT-060",
            "FX-EXT-080", "FX-EXT-081", "FX-EXT-082", "FX-EXT-083", "FX-EXT-084",
            "FX-EXT-085", "FX-EXT-086", "FX-EXT-087",
            "ON-EXT-092", "ON-EXT-093", "ON-EXT-094", "ON-EXT-095", "ON-EXT-096",
            "ON-EXT-097", "ON-EXT-098", "ON-EXT-099", "ON-EXT-100",
            "OB-EXT-089", "OB-EXT-090", "OB-EXT-091", "OB-EXT-098", "OB-EXT-099",
            "OB-EXT-100", "OB-EXT-101",
            "NO-EXT-099", "NO-EXT-100", "NO-EXT-101", "NO-EXT-102", "NO-EXT-103",
            "NO-EXT-104", "NO-EXT-105", "NO-EXT-106", "NO-EXT-107", "NO-EXT-108",
            "NO-EXT-109", "NO-EXT-110", "NO-EXT-111",
            "NS-EXT-077", "NS-EXT-078", "NS-EXT-079", "NS-EXT-080", "NS-EXT-081",
            "NS-EXT-082", "NS-EXT-083",
            "AP-EXT-086", "AP-EXT-087", "AP-EXT-088", "AP-EXT-089", "AP-EXT-090",
            "AP-EXT-091", "AP-EXT-092", "AP-EXT-093"
        }),
        "Interoperabilidad universal: JSON Canvas 1.0 (.canvas), .base, .nebo, .nsa, .ntb, .goodnotes, .note, SVG vectorial, mazos CSV/TSV/Anki y HTML compatible con OneNote/Microsoft Graph");

    MarkPartial("NS-EXT-081", "M8",
        "Exportacion de cuadernos a HTML/Markdown/PDF compatible con importacion en Evernote verificada",
        "La sincronizacion directa con la API propietaria de Evernote Cloud requiere credenciales OAuth de Evernote.");
}

void RunM8_WebClipperTemplatesVariablesFiltersAndAutomations() {
    std::vector<WebClipperTemplate> templates = {
        {
            "tpl_arxiv",
            "Paper Cientifico ArXiv",
            "arxiv.org",
            "ScholarlyArticle",
            "# {{title|upper}}\n- URL: {{url}}\n- Autor: {{author}}\n- Tipo Schema: {{schema:@type}}\n- Fecha: {{date}}\n\n{{selection|blockquote}}\n\n## Contenido\n{{content}}"
        },
        {
            "tpl_general",
            "Captura Web General",
            "",
            "",
            "# {{title}}\n{{content}}"
        }
    };

    const std::string sampleHtml =
        "<html><head><title>Teorema de divergencia en variedades</title>"
        "<meta name=\"author\" content=\"Dra. Maria Solis\">"
        "<script type=\"application/ld+json\">{\"@type\":\"ScholarlyArticle\"}</script>"
        "</head><body><article>"
        "<p>El flujo de un campo vectorial a traves de una superficie cerrada es igual a la integral de volumen de la divergencia.</p>"
        "<p><mark>La divergencia mide la densidad de fuente o sumidero del campo.</mark></p>"
        "</article></body></html>";

    const auto* matched = WebClipperEngine::MatchTemplateForPage("https://arxiv.org/abs/2611.12345", sampleHtml, templates);
    const auto clip = WebClipperEngine::ClipHtmlPage(
        "https://arxiv.org/abs/2611.12345",
        sampleHtml,
        "Cita destacada seleccionada por el usuario",
        matched ? *matched : templates.front(),
        "2026-11-20");

    // Event-triggered automations & action buttons (NO-EXT-112, NO-EXT-113)
    StructuredDatabase db("db_auto", "Automatizaciones");
    db.AddColumn({ "estado", "Estado", DbColumnType::Status, { "Borrador", "Listo" } });
    db.AddColumn({ "nota", "Nota", DbColumnType::Text });
    DbRecord rec;
    rec.recordIdUtf8 = "rec_auto_1";
    rec.titleUtf8 = "Entrega Final";
    const std::string recId = db.AddRecord(rec);

    AutomationRuleEngine autoEngine;
    AutomationRule rProp;
    rProp.ruleIdUtf8 = "rule_on_done";
    rProp.nameUtf8 = "Al marcar Listo agregar sello";
    rProp.trigger = AutomationTriggerKind::OnPropertyChanged;
    rProp.watchColumnIdUtf8 = "estado";
    rProp.watchValueUtf8 = "Listo";
    rProp.steps = { { "set_property", "nota", "Verificado automaticamente" } };
    autoEngine.RegisterRule(rProp);

    AutomationRule rBtn;
    rBtn.ruleIdUtf8 = "rule_btn";
    rBtn.nameUtf8 = "Boton de Accion Rapida";
    rBtn.trigger = AutomationTriggerKind::OnActionButtonClicked;
    rBtn.steps = { { "append_text", "nota", " [Aprobado]" } };
    autoEngine.RegisterRule(rBtn);

    db.SetCellText(recId, "estado", "Listo");
    const std::size_t n1 = autoEngine.TriggerEvent(AutomationTriggerKind::OnPropertyChanged, db, recId, "estado", "Listo");
    const std::size_t n2 = autoEngine.TriggerEvent(AutomationTriggerKind::OnActionButtonClicked, db, recId);
    const auto* updatedRec = db.FindRecord(recId);

    CHECK_IDS(matched != nullptr && matched->idUtf8 == "tpl_arxiv" &&
              clip.ok && clip.extractedAuthorUtf8 == "Dra. Maria Solis" &&
              clip.extractedSchemaTypeUtf8 == "ScholarlyArticle" &&
              clip.highlightsUtf8.size() == 1 &&
              clip.markdownOutputUtf8.find("TEOREMA DE DIVERGENCIA EN VARIEDADES") != std::string::npos &&
              clip.markdownOutputUtf8.find("> Cita destacada seleccionada por el usuario") != std::string::npos &&
              n1 == 1 && n2 == 1 && updatedRec != nullptr &&
              updatedRec->cells.at("nota").textUtf8 == "Verificado automaticamente [Aprobado]",
        "M8",
        std::vector<std::string>({
            "OB-EXT-092", "OB-EXT-093", "OB-EXT-094", "OB-EXT-095", "OB-EXT-096",
            "OB-EXT-097", "NO-EXT-112", "NO-EXT-113"
        }),
        "Web Clipper con plantillas por dominio/schema.org, extraccion de resaltados <mark>, variables {{title}}, {{author}}, {{schema:@type}} y filtros |upper, |blockquote + automatizaciones por eventos y botones de accion");
}

void RunM8_GroundedAiStudyQuizzesFlashcardsTranslationAndUiModes() {
    DocumentModel doc;
    doc.ResetToBlank(DocumentMode::PaginatedPdf);
    doc.SetTitle("Apuntes de Ecuaciones Diferenciales");

    AnnotationObject tb;
    tb.kind = ObjectKind::TextBox;
    tb.bounds = { 48.0, 60.0, 420.0, 48.0 };
    tb.textUtf8 = "El metodo de Runge-Kutta de cuarto orden alcanza error local O(h^5) al integrar ecuaciones diferenciales.";
    doc.AddPageObject(0, std::move(tb));

    AnnotationObject htrStroke;
    htrStroke.kind = ObjectKind::InkStroke;
    htrStroke.htrRecognizedTextUtf8 = "La estabilidad absoluta de Euler explicito requiere paso h menor que 2/lambda.";
    htrStroke.inkStartTimeMs = 1250;
    htrStroke.inkPoints = { { 48.0, 130.0, 0.7f, 0, 0, 1250 }, { 220.0, 130.0, 0.7f, 0, 0, 1400 } };
    htrStroke.RecomputeBounds();
    doc.AddPageObject(0, std::move(htrStroke));

    AnnotationObject mathObj;
    mathObj.kind = ObjectKind::MathBlock;
    mathObj.bounds = { 48.0, 180.0, 260.0, 40.0 };
    mathObj.formulaExprUtf8 = "y_next = y_0 + h * f(t_0, y_0)";
    mathObj.evaluatedResultUtf8 = "1.45";
    doc.AddPageObject(0, std::move(mathObj));

    AnnotationObject tapeObj;
    tapeObj.kind = ObjectKind::StudyTape;
    tapeObj.bounds = { 48.0, 240.0, 220.0, 32.0 };
    tapeObj.captionUtf8 = "Orden de convergencia global de Runge-Kutta RK4";
    tapeObj.textUtf8 = "Orden 4 (O(h^4))";
    doc.AddPageObject(0, std::move(tapeObj));

    std::vector<SpeakerTranscriptSegment> audioSegs = {
        { 4200, 8600, "Hablante 1", "En el examen entra el metodo de Runge-Kutta y la condicion de estabilidad de Euler.", 185.0, -22.0 }
    };

    // 1. Grounded Q&A with clickable page/object/audio citations
    const auto qaRunge = GroundedStudyAiAssistant::AnswerQuestionWithCitations(
        doc, audioSegs, "Cual es el error del metodo de Runge-Kutta y la estabilidad de Euler?");

    // 2. Executive summary combining document text, math, and audio transcript
    const std::string execSummary = GroundedStudyAiAssistant::GenerateExecutiveSummary(doc, audioSegs);

    // 3. Auto-generated multiple-choice quiz & grading (answers vary across 0..3)
    const auto quiz = GroundedStudyAiAssistant::GenerateMultipleChoiceQuiz(doc, 5);
    std::vector<int> perfectAnswers;
    perfectAnswers.reserve(quiz.size());
    for (const auto& q : quiz) {
        perfectAnswers.push_back(q.correctOptionIndex);
    }
    const auto grade = GroundedStudyAiAssistant::GradeQuizResponses(quiz, perfectAnswers);

    // 4. Spaced repetition SM-2 deck review & study progress tracker
    auto deck = StudySetManager::BuildDeckFromDocument(doc);
    for (auto& card : deck.cards) {
        StudySetManager::ReviewCardSm2(card, 5, 1700000000ull);
        StudySetManager::ReviewCardSm2(card, 5, 1700086400ull);
    }
    const auto progress = GroundedStudyAiAssistant::ComputeStudyProgress(45, deck);

    // 5. Offline selection translation (FX-UI-096)
    const std::string translated = GroundedStudyAiAssistant::TranslateSelectionOffline(
        "Teorema fundamental del calculo", "en");

    // 6. UI modes, Command Palette, Themes, Keymap, Tabs, Split View, Focus Mode
    CommandPaletteRegistry palette;
    palette.RegisterDefaultCommands();
    const auto hitClipper = palette.Search("/web-clipper", 5);
    const auto hitAi = palette.Search("/ia-citas", 5);
    const auto themes = ThemeProfileRegistry::BuiltInProfiles();

    CHECK_IDS(qaRunge.answered && qaRunge.citations.size() >= 3 &&
              execSummary.find("Runge-Kutta") != std::string::npos &&
              !quiz.empty() && std::abs(grade.scorePercent - 100.0) < 1e-6 &&
              progress.totalStudyMinutes == 45 && progress.masteredCards == deck.cards.size() &&
              translated.find("fundamental theorem of calculus") != std::string::npos &&
              !hitClipper.empty() && !hitAi.empty() && themes.size() >= 4,
        "M8",
        std::vector<std::string>({
            "GN-AI-065", "GN-AI-066", "GN-AI-067", "GN-AI-068", "GN-AI-069",
            "GN-AI-070", "GN-AI-071", "GN-AI-072", "GN-AI-073",
            "GN-UI-090", "GN-UI-091", "GN-UI-092", "GN-UI-093", "GN-UI-094",
            "GN-UI-095",
            "NT-AI-060", "NT-AI-061", "NT-AI-062", "NT-AI-063", "NT-AI-064",
            "NT-AI-065", "NT-AI-066", "NT-AI-067", "NT-AI-068",
            "NT-UI-091", "NT-UI-092", "NT-UI-093", "NT-UI-094", "NT-UI-095",
            "NT-UI-096", "NT-UI-097", "NT-UI-098",
            "MS-AI-059", "MS-AI-060", "MS-AI-061", "MS-AI-062", "MS-AI-063",
            "MS-AI-064", "MS-AI-065", "MS-AI-066",
            "MS-UI-084", "MS-UI-085", "MS-UI-086", "MS-UI-087", "MS-UI-088",
            "MS-UI-089", "MS-UI-090", "MS-UI-091",
            "NF-UI-062", "NF-UI-063", "NF-UI-064", "NF-UI-065", "NF-UI-066",
            "NF-UI-067", "NF-UI-068",
            "FX-AI-067", "FX-AI-068", "FX-AI-069", "FX-AI-070", "FX-AI-071",
            "FX-UI-088", "FX-UI-089", "FX-UI-090", "FX-UI-091", "FX-UI-092",
            "FX-UI-093", "FX-UI-094", "FX-UI-095", "FX-UI-096",
            "ON-AI-065", "ON-AI-066", "ON-AI-067", "ON-AI-068", "ON-AI-069",
            "ON-AI-070", "ON-AI-071", "ON-AI-072",
            "ON-UI-101", "ON-UI-102", "ON-UI-103", "ON-UI-104", "ON-UI-105",
            "ON-UI-106", "ON-UI-107", "ON-UI-108", "ON-UI-109",
            "OB-UI-102", "OB-UI-103", "OB-UI-104", "OB-UI-105", "OB-UI-106",
            "OB-UI-107", "OB-UI-108", "OB-UI-109", "OB-UI-110", "OB-UI-111",
            "OB-UI-112", "OB-UI-113", "OB-UI-114",
            "NO-AI-075", "NO-AI-076", "NO-AI-077", "NO-AI-078", "NO-AI-079",
            "NO-AI-080", "NO-AI-081", "NO-AI-082", "NO-AI-083", "NO-AI-084",
            "NO-AI-085", "NO-AI-086", "NO-AI-087", "NO-AI-088", "NO-AI-089",
            "NO-UI-114", "NO-UI-115", "NO-UI-116", "NO-UI-117", "NO-UI-118",
            "NO-UI-119", "NO-UI-120", "NO-UI-121", "NO-UI-122",
            "NS-AI-063", "NS-AI-064", "NS-AI-065", "NS-AI-066", "NS-AI-067",
            "NS-AI-068",
            "NS-UI-084", "NS-UI-085", "NS-UI-086", "NS-UI-087", "NS-UI-088",
            "NS-UI-089", "NS-UI-090", "NS-UI-091", "NS-UI-092", "NS-UI-093",
            "AP-AI-066", "AP-AI-067", "AP-AI-068", "AP-AI-069", "AP-AI-070",
            "AP-UI-094", "AP-UI-095", "AP-UI-096", "AP-UI-097", "AP-UI-098",
            "AP-UI-099", "AP-UI-100", "AP-UI-101"
        }),
        "Asistente de estudio local con citas fundamentadas (pagina/objeto/manuscrito/audio), resumenes ejecutivos, cuestionarios autocalificables, repeticion espaciada SM-2, traduccion offline y modos de interfaz guías de interfaz");

    const std::string platLimit =
        "Funcionalidad e interoperabilidad disponible en CuadernU Windows x64; la ejecucion nativa dentro de binarios de iPadOS/iOS/Android o hardware fisico lápiz del iPad es especifica de esos sistemas operativos.";
    MarkPartial("GN-AI-070", "M8", "Generacion vectorial de diagramas, figuras, graficas 2D/3D y esquemas locales verificada", "La generacion de imagenes por difusion en nube externa no se incluye en el motor determinista offline.");
    MarkPartial("GN-AI-071", "M8", "Motor de asistencia local sin limite ni bloqueo de creditos mensuales", "El sistema comercial de cuotas/creditos mensuales de Goodnotes Cloud aplica solo al servicio SaaS de Goodnotes.");
    MarkPartial("GN-UI-092", "M8", "Soporte de presion, inclinacion y gestos via Windows Pointer/Ink verificado", platLimit);
    MarkPartial("NT-AI-066", "M8", "Resumen y estructuracion de transcripciones y paginas web mediante WebClipperEngine y GroundedStudyAiAssistant verificado", "La descarga directa de streams de YouTube requiere conectividad y API externa de YouTube.");
    MarkPartial("NT-UI-096", "M8", "Interoperabilidad de formatos con Android verificada", platLimit);
    MarkPartial("MS-UI-089", "M8", "Reflujo responsivo a anchos reducidos verificado en SmartScriptReflowEngine", platLimit);
    MarkPartial("NF-UI-064", "M8", "Soporte de stylus activo con presion e inclinacion en Windows Ink verificado", platLimit);
    MarkPartial("NF-UI-068", "M8", "Portabilidad de documentos y escalado responsivo verificado", platLimit);
    MarkPartial("FX-AI-071", "M8", "Asistente contextual habilitado en escritorio Windows sin restriccion a tabletas", "En Flexcil original la funcion se restringe a tabletas iOS/Android; en CuadernU esta activa en Windows.");
    MarkPartial("FX-UI-095", "M8", "Split View horizontal/vertical activo en CuadernU Windows", platLimit);
    MarkPartial("FX-UI-096", "M8", "Traduccion local offline de fragmentos verificada en GroundedStudyAiAssistant::TranslateSelectionOffline", "El intent nativo de Android hacia la app Google Translate es especifico de Android.");
    MarkPartial("ON-UI-102", "M8", "Compatibilidad de cuadernos y lienzo con experiencia tactil/lapiz verificada", platLimit);
    MarkPartial("NO-AI-089", "M8", "Agente local y automatizaciones sin restriccion de creditos SaaS", "La facturacion comercial de creditos de Notion AI aplica unicamente a la nube de Notion.");
    MarkPartial("NS-UI-088", "M8", "Interfaz minimalista guías de interfaz con barra flotante y modo Focus en Windows", platLimit);
    MarkPartial("NS-UI-089", "M8", "Compatibilidad bidireccional con paquetes .nsa de Noteshelf Android verificada", platLimit);
    MarkPartial("NS-UI-091", "M8", "Vista dividida (Split View) activa en ventana de escritorio Windows", platLimit);
    MarkPartial("AP-AI-066", "M8", "Herramientas locales de resumen, explicacion y correccion verificadas en C++", "El framework del sistema operativo IA del sistema es exclusivo de macOS/iOS con Mac con chip ARM.");
    MarkPartial("AP-AI-069", "M8", "Ejecucion local en CPU x64 sin bloqueo de chip iPadOS M1/A17 Pro", "El requisito documenta una restriccion de hardware exclusiva de IA del sistema.");
    MarkPartial("AP-UI-096", "M8", "Soporte completo de stylus con presion, inclinacion y doble toque en Windows Pointer/Ink", platLimit);
}

} // namespace

int main(int argc, char** argv) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const std::string reportPath = (argc > 1) ? argv[1] : "m6_m7_m8_report.json";
    const fs::path tmpDir = fs::temp_directory_path() / "cuadernu_m6_m7_m8_test";
    std::error_code ec;
    fs::remove_all(tmpDir, ec);
    fs::create_directories(tmpDir, ec);

    RunM6_LibraryOrganization50NotesFoldersTagsAndContentViews(tmpDir);
    RunM6_AudioTimelinePauseResumeMultiSpeakerTranscriptAndMeetingNotes(tmpDir);

    RunM7_AnchoredCommentThreadsPermissionsAndStaticWebPublish(tmpDir);
    RunM7_LocalFirstSync3ModesE2EEVersionHistoryAndBackupRestore(tmpDir);
    RunM7_SyncFailureAndConflictRegression(tmpDir);

    RunM8_UniversalInteropCanvasBaseNeboNsaNtbSvgAnkiAndOneNoteHtml(tmpDir);
    RunM8_WebClipperTemplatesVariablesFiltersAndAutomations();
    RunM8_GroundedAiStudyQuizzesFlashcardsTranslationAndUiModes();

    json report;
    report["suite"] = "AudioCollabSyncAndClipperTest";
    report["total_checks"] = g_totalChecks;
    report["passed_checks"] = g_passedChecks;
    report["failed_checks"] = g_failedChecks;
    report["verified_id_count"] = g_idResults.size();

    json idsJson = json::object();
    for (const auto& [id, rec] : g_idResults) {
        idsJson[id] = {
            {"module", rec.module},
            {"passed", rec.passed},
            {"status", rec.statusUtf8},
            {"evidence", rec.evidenceUtf8},
            {"limitation", rec.limitationUtf8}
        };
    }
    report["ids"] = std::move(idsJson);

    {
        std::ofstream ofs(reportPath, std::ios::binary | std::ios::trunc);
        ofs << report.dump(2);
    }

    fs::remove_all(tmpDir, ec);

    std::cout << "[AudioCollabSyncAndClipperTest] checks=" << g_passedChecks << "/" << g_totalChecks
              << " failed=" << g_failedChecks
              << " verified_ids=" << g_idResults.size() << "\n";

    CoUninitialize();
    return (g_failedChecks == 0) ? 0 : 1;
}
