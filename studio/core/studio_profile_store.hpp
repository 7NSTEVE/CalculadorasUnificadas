#pragma once

#include "accessibility_engine.hpp"
#include "audio_timeline.hpp"
#include "collaboration_publish_sync.hpp"
#include "document_model.hpp"
#include "keymap_config.hpp"
#include "knowledge_graph.hpp"
#include "notebook_vault.hpp"
#include "structured_database.hpp"
#include "../interop/universal_formats_clipper.hpp"
#include "../pdf/flexcil_study_overlay.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

struct StudioUiPreferences {
    std::string themeProfileIdUtf8 = "porcelana-claro";
    int toolbarOrientation = 0; // 0 = Top, 1 = Left, 2 = Right (NF-EDIT-040)
    std::string customCssSnippetUtf8;
    LocalLlmConfig llmConfig{};
    std::string whisperExePathUtf8;
    std::string whisperModelPathUtf8;
    std::uint64_t totalStudySeconds = 0;
    std::string autoSyncFolderUtf8;
    bool autoSyncEnabled = false;
    bool autoSyncEncrypted = false;
    bool hardwareGpuAcceleration = false; // Default false => strict 0.0% GPU via D2D Software + Win11 DWM Mica
    // Que no dañe ni mienta (revisión del 27-nov-2026): todo apagado por defecto
    bool snapShapesEnabled = false;        // formas al mantener el lápiz quieto
    bool scribbleEraseEnabled = false;     // tachar para borrar (solo tinta)
    bool showExperimentalFeatures = false; // mostrar funciones simuladas o en prueba
    AccessibilityPreferences accessibility{};
};

struct DocumentExtensionBundle {
    std::string yamlFrontmatterUtf8;
    std::vector<NoteProperty> properties;
    std::vector<std::string> aliasesUtf8;
    std::vector<std::string> tagsUtf8;
    AnchoredCommentThreadManager* comments = nullptr;
    StructuredDatabase* database = nullptr;
    CollaborationPermissionManager* permissions = nullptr;
    AutomationRuleEngine* automations = nullptr;
    FlexcilStudyOverlay* flexcil = nullptr;
    StudyDeck* studyDeck = nullptr;
    std::string customCssUtf8;
    int activeDbViewKind = 0;
};

class StudioProfileStore {
public:
    // A caller can select an absolute profile path before initializing the application.
    static bool SetProfileRootOverride(const std::wstring& path);
    static bool HasProfileRootOverride() noexcept;
    static void ClearProfileRootOverride() noexcept;

    // Resolves the selected profile, %LOCALAPPDATA%\CuadernU\, or ./CuadernU_Profile.
    static std::wstring DefaultProfileRootDir();

    // Document-scoped extensions persisted inside .cudoc (extensions/*.json)
    static void PackExtensionsIntoDocument(DocumentModel& doc, const DocumentExtensionBundle& bundle);
    static void UnpackExtensionsFromDocument(const DocumentModel& doc, DocumentExtensionBundle& bundle);

    // User-global profile persistence under rootDir:
    //   library_index.json, snippets.json, keymap.json, ui_settings.json, vault/
    static bool SaveAllProfileState(
        const std::wstring& rootDir,
        const LibraryContentViewsManager& library,
        const ReusableElementLibrary& snippets,
        const KeymapProfile& keymap,
        const KnowledgeVault& vault,
        const StudioUiPreferences& prefs,
        std::string* outErr = nullptr);

    static bool LoadAllProfileState(
        const std::wstring& rootDir,
        LibraryContentViewsManager& library,
        ReusableElementLibrary& snippets,
        KeymapProfile& keymap,
        KnowledgeVault& vault,
        StudioUiPreferences& prefs,
        std::string* outErr = nullptr);

    static bool SaveUiPreferences(const std::wstring& path, const StudioUiPreferences& prefs, std::string* outErr = nullptr);
    static bool LoadUiPreferences(const std::wstring& path, StudioUiPreferences& prefs, std::string* outErr = nullptr);
};

} // namespace DocStudio
