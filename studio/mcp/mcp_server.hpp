#pragma once

#include "mcp_gui_ipc.hpp"
#include "mcp_protocol.hpp"

#include "../bridge/calc_bridge.hpp"
#include "../core/accessibility_engine.hpp"
#include "../core/ai_provider_vault.hpp"
#include "../core/audio_timeline.hpp"
#include "../core/camera.hpp"
#include "../core/collaboration_publish_sync.hpp"
#include "../core/command_stack.hpp"
#include "../core/document_model.hpp"
#include "../core/keymap_config.hpp"
#include "../core/knowledge_graph.hpp"
#include "../core/lan_remote.hpp"
#include "../core/notebook_vault.hpp"
#include "../core/smart_script_reflow.hpp"
#include "../core/structured_database.hpp"
#include "../core/studio_profile_store.hpp"
#include "../core/study_assistant_tools.hpp"
#include "../pdf/flexcil_study_overlay.hpp"
#include "../render/d2d_renderer.hpp"
#include "../tutor/tutor_sidecar_bridge.hpp"
#include "../tutor/tutor_whiteboard_protocol.hpp"

#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
#include "../integrations/gemini_notebook/gemini_notebook_client.hpp"
#include "../integrations/gemini_notebook/gemini_notebook_sync_store.hpp"
#endif

#ifdef DOC_STUDIO_HAS_ACADEMIC
#include "../integrations/tec_digital/tec_academic_store.hpp"
#endif

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio::Mcp {

struct McpServerOptions {
    std::wstring profileDirW;
    bool sandboxMode = false;
    bool readonlyTec = false;
    bool noProfileWrite = false;
    std::uint16_t httpPort = 0;
    std::wstring cuadernuExePathW;
    std::wstring repoRootW;
    std::wstring buildDirW;
};

struct SessionEntry {
    std::string sessionId;
    std::wstring filePathW;
    DocumentModel doc;
    CommandStack cmdStack;
    Camera2D camera;
    PageRenderCache pageCache{ 32ull * 1024ull * 1024ull };
    FlexcilStudyOverlay flexcil;
    AnchoredCommentThreadManager comments;
    StructuredDatabase database{ "db_session", "Base de Datos" };
    CollaborationPermissionManager permissions;
    AutomationRuleEngine automations;
    StudyDeck studyDeck;
    PageVersionHistory versionHistory;
    AudioTimelineSession audioSession;
    ZoomWritingBoxState zoomBox{};
    FloatingInkToolbarState floatingToolbar{};
    RulerState rulerState{};
    LaserPointerState laserPointer{};
    Tutor::TutorSessionStateMachine tutorSession;

    std::string yamlFrontmatterUtf8;
    std::vector<NoteProperty> properties;
    std::vector<std::string> aliasesUtf8;
    std::vector<std::string> tagsUtf8;
    std::string customCssUtf8;
    int activeDbViewKind = 0;
    std::vector<std::uint64_t> selectionIds;
    bool examModeLocked = false;
    std::string examPinHashUtf8;
    std::size_t activePageIndex = 0;
};

class McpServerContext {
public:
    explicit McpServerContext(McpServerOptions opts);
    ~McpServerContext();

    McpServerContext(const McpServerContext&) = delete;
    McpServerContext& operator=(const McpServerContext&) = delete;

    const McpServerOptions& Options() const noexcept { return options_; }
    std::wstring EffectiveProfileRootW() const;
    std::wstring EffectiveArtifactsDirW() const;

    // Multi-session management
    std::shared_ptr<SessionEntry> CreateSession(
        const std::string& titleUtf8,
        DocumentMode mode,
        PaperPattern bg = PaperPattern::Grid,
        std::string optionalCustomId = "");
    std::shared_ptr<SessionEntry> FindSession(const std::string& sessionId) const;
    std::shared_ptr<SessionEntry> GetOrCreateSession(const nlohmann::json& args);
    bool CloseSession(const std::string& sessionId);
    bool SwitchActiveSession(const std::string& sessionId);
    std::string ActiveSessionId() const;
    std::vector<std::string> ListSessionIds() const;

    void PackExtensions(SessionEntry& s) const;
    void UnpackExtensions(SessionEntry& s) const;

    // Lazy stores & profile
    void EnsureProfileLoaded();
    bool SaveProfileIfAllowed(std::string* outErr = nullptr);

    D2DRenderer& EnsureRenderer();

    // Offscreen PNG rendering (Plane 1, 0 HWND, 0% GPU)
    bool RenderPagePng(SessionEntry& s, std::size_t pageIdx, int widthPx, std::vector<std::uint8_t>& outPng, std::string* outErr = nullptr);
    bool RenderCanvasRegionPng(SessionEntry& s, const DocRect& worldRect, int widthPx, int heightPx, std::vector<std::uint8_t>& outPng, std::string* outErr = nullptr);
    bool RenderViewportPng(SessionEntry& s, int widthPx, int heightPx, bool presenterMode, bool darkTheme, std::vector<std::uint8_t>& outPng, std::string* outErr = nullptr);

    bool WriteBytesToFile(const std::wstring& pathW, const std::vector<std::uint8_t>& bytes, std::string* outErr = nullptr) const;
    bool ReadBytesFromFile(const std::wstring& pathW, std::vector<std::uint8_t>& outBytes, std::string* outErr = nullptr) const;
    std::wstring ResolveArtifactPathW(const std::string& requestedPathUtf8, const std::string& defaultStem, const std::string& extWithDot) const;

    // Secret scrubbing registration
    void SyncRedactorSecrets(SecretRedactor& redactor);

    // Subsystem stores (guarded by storeMtx)
    std::recursive_mutex storeMtx;
    LibraryContentViewsManager library;
    ReusableElementLibrary snippets;
    KeymapProfile keymap = KeymapProfile::Default();
    KnowledgeVault knowledgeVault;
    NotebookVault notebookVault;
    VaultFullTextIndex ftsIndex;
    StudioUiPreferences uiPrefs{};
    AiProviderVault aiVault;
    SpeechSynthesizerWin32 speechSynth;
    PomodoroTimerState pomodoro{};
    LanRemote::LanCollabHttpServer lanServer;
    CommandPaletteRegistry commandPalette;
    CalcBridge::CalcIpcChannel calcIpc;
    Tutor::TutorSidecarBridge tutorSidecar;
    McpGuiInstanceManager guiManager;

#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
    GeminiNotebook::GeminiNotebookSyncStore& EnsureGeminiStore();
    GeminiNotebook::INotebookHttpTransport* geminiTransportOverride = nullptr;
#endif

#ifdef DOC_STUDIO_HAS_ACADEMIC
    TecDigital::TecAcademicStore& EnsureTecStore();
#endif

    // Policy check for TEC mutations: true if mutations are permitted at server/env level
    bool IsTecMutationPolicyAllowed(std::string* outReason = nullptr) const;

    // Test sandbox reset helper
    bool ResetSandbox(std::string* outErr = nullptr);

private:
    McpServerOptions options_;
    std::wstring sandboxRootW_;
    bool profileLoaded_ = false;
    bool rendererInitialized_ = false;
    D2DRenderer renderer_;

    mutable std::mutex sessionMtx_;
    std::uint64_t nextSessionSeq_ = 1;
    std::string activeSessionId_;
    std::unordered_map<std::string, std::shared_ptr<SessionEntry>> sessions_;

#ifdef DOC_STUDIO_HAS_GEMINI_NOTEBOOK
    std::unique_ptr<GeminiNotebook::GeminiNotebookSyncStore> geminiStore_;
#endif
#ifdef DOC_STUDIO_HAS_ACADEMIC
    std::unique_ptr<TecDigital::TecAcademicStore> tecStore_;
#endif
};

// Shared JSON conversion helpers used across all mcp_tools_*.cpp modules
ObjectTarget TargetFromArgs(const nlohmann::json& args, const DocumentModel& doc);
DocColor ParseColorJson(const nlohmann::json& j, DocColor fallback = DocColor::RGBA(26, 115, 232, 255));
std::string FormatColorHex(DocColor c);
PaperPattern ParsePaperPattern(const std::string& s, PaperPattern fallback = PaperPattern::Grid);
std::string PaperPatternToString(PaperPattern p);
std::string ObjectKindToString(ObjectKind k);
InkPenStyle ParseInkPenStyle(const std::string& s);
ShapeKind ParseShapeKind(const std::string& s);
nlohmann::json SerializeObjectSummary(const AnnotationObject& o);
nlohmann::json SerializeObjectFull(const AnnotationObject& o);
nlohmann::json MakeToolSchema(
    const std::vector<std::string>& actions,
    nlohmann::json extraProps = nlohmann::json::object(),
    std::vector<std::string> required = { "action" });

// Modular domain registration functions
void RegisterDocumentAndCanvasTools(McpProtocolRouter& router, McpServerContext& ctx);
void RegisterKnowledgeAndSecurityTools(McpProtocolRouter& router, McpServerContext& ctx);
void RegisterInteropAndStemTools(McpProtocolRouter& router, McpServerContext& ctx);
void RegisterAcademicAndAiTools(McpProtocolRouter& router, McpServerContext& ctx);
void RegisterCaptureGuiAndTestTools(McpProtocolRouter& router, McpServerContext& ctx);

class McpServer {
public:
    explicit McpServer(McpServerOptions opts = {});
    ~McpServer() = default;

    McpProtocolRouter& Router() noexcept { return router_; }
    const McpProtocolRouter& Router() const noexcept { return router_; }
    McpServerContext& Context() noexcept { return ctx_; }
    const McpServerContext& Context() const noexcept { return ctx_; }

    std::string HandleJsonRpcLine(const std::string& lineUtf8);
    McpToolResult CallTool(const std::string& toolName, const nlohmann::json& args);
    int RunStdio();
    int RunHttp(std::uint16_t port, const std::string& bearerTokenUtf8, std::atomic<bool>* stopFlag = nullptr);

    static std::string GenerateAgentMcpConfigJson(
        const std::wstring& cuadernuExePathW,
        const std::string& optionalGoogleMcpEndpointUtf8 = "https://notebook.googleapis.com/mcp");

private:
    void RegisterResourcesAndPrompts();

    McpServerContext ctx_;
    McpProtocolRouter router_;
};

} // namespace DocStudio::Mcp
