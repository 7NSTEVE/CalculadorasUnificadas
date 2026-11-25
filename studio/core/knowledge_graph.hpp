#pragma once

#include "doc_types.hpp"
#include "document_model.hpp"
#include "command_stack.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

// ---------------- YAML Frontmatter Properties ----------------

enum class NotePropType : std::uint8_t {
    Text = 0,
    Number = 1,
    Date = 2,
    Checkbox = 3,
    List = 4
};

struct NoteProperty {
    std::string keyUtf8;
    NotePropType type = NotePropType::Text;
    std::string valueUtf8;
    std::vector<std::string> listValuesUtf8;
};

// ---------------- Structured Rich Blocks & Code Syntax Highlighting ----------------

enum class RichBlockKind : std::uint8_t {
    Paragraph = 0,
    Heading1 = 1,
    Heading2 = 2,
    Heading3 = 3,
    BulletList = 4,
    NumberedList = 5,
    Checklist = 6,
    CodeBlock = 7,
    Quote = 8,
    Callout = 9,
    Toggle = 10,
    Divider = 11,
    MathBlock = 12,
    SyncedBlock = 13,
    HtmlSnippet = 14
};

struct RichBlock {
    std::uint64_t blockId = 0;
    std::string stableAnchorUtf8; // e.g. "sec-1" for ^anchor links
    RichBlockKind kind = RichBlockKind::Paragraph;
    std::string textUtf8;
    std::string extraUtf8;        // code language, callout type, or syncId
    int indentLevel = 0;
    bool checked = false;         // Checklist item state
    bool collapsed = false;       // Toggle / Heading fold state
    std::vector<RichBlock> children;
};

enum class CodeTokenKind : std::uint8_t {
    Plain = 0,
    Keyword = 1,
    TypeName = 2,
    StringLit = 3,
    Comment = 4,
    Number = 5,
    Operator = 6
};

struct CodeSyntaxToken {
    CodeTokenKind kind = CodeTokenKind::Plain;
    std::size_t start = 0;
    std::size_t length = 0;
    std::string textUtf8;
};

struct OutlineHeading {
    int level = 1; // 1..6
    std::string titleUtf8;
    std::size_t lineIndex0 = 0;
    std::string anchorUtf8;
};

struct HighlightSpan {
    std::size_t lineIndex0 = 0;
    std::string highlightedTextUtf8;
};

class RichBlockDocument {
public:
    static RichBlockDocument FromMarkdown(const std::string& mdUtf8);
    std::string ToMarkdown() const;
    std::string ToReadingPlainText(std::size_t wrapCols = 80) const;
    std::string ToHtml5(const std::string& cssSnippetUtf8 = "") const;

    std::vector<OutlineHeading> ExtractOutline() const;
    std::vector<HighlightSpan> ExtractHighlights() const;

    // Word-wraps paragraph / structured text at maxCharsPerLine without breaking words when avoidable
    static std::string ReflowParagraph(const std::string& textUtf8, std::size_t maxCharsPerLine);

    // Tokenizes code blocks for C++, Python, JavaScript, JSON, SQL, etc.
    static std::vector<CodeSyntaxToken> TokenizeCodeSyntax(const std::string& codeUtf8, const std::string& langUtf8);

    std::vector<RichBlock> blocks;
    std::vector<NoteProperty> properties;
};

// ---------------- Vault Notes, Templates, Daily Notes & Workspaces ----------------

struct NoteTemplate {
    std::string nameUtf8;
    std::string contentTemplateUtf8;
    bool isBlockTemplate = false;
    std::uint32_t coverColorRgba = 0x1D4ED8FFu;
    PaperPattern paperPattern = PaperPattern::Plain;
    std::vector<NoteProperty> defaultProps;
};

struct VaultNoteRecord {
    std::string noteIdUtf8;
    std::string titleUtf8;
    std::string folderPathUtf8; // e.g. "Calculo/Derivadas"
    std::vector<std::string> aliasesUtf8;
    std::vector<std::string> tagsUtf8;
    std::vector<NoteProperty> properties;
    std::string bodyMarkdownUtf8;
    bool isDailyNote = false;
    std::string dailyDateIsoUtf8; // YYYY-MM-DD
    bool inTrash = false;
    std::uint32_t coverColorRgba = 0x1D4ED8FFu;
    PaperPattern paperPattern = PaperPattern::Plain;
    std::uint64_t updatedEpochSec = 0;
};

struct NoteFilterCriteria {
    std::string folderPrefixUtf8;
    std::string requiredTagUtf8;
    std::string propKeyUtf8;
    std::string propOpUtf8 = "=="; // "==", "contains", ">=", "<="
    std::string propValueUtf8;
    bool includeTrash = false;
};

struct KnowledgeSearchHit {
    std::string noteIdUtf8;
    std::string titleUtf8;
    std::string folderPathUtf8;
    std::string snippetUtf8;
    int score = 0;
};

// ---------------- Links, Backlinks, Unlinked Mentions & Knowledge Graph ----------------

enum class KnowledgeLinkKind : std::uint8_t {
    WikiNote = 0,      // [[Note]] or [[Note|Alias]]
    WikiHeading = 1,   // [[Note#Heading]]
    WikiBlock = 2,     // [[Note^block]] or [[Note#^block]]
    Transclusion = 3,  // ![[Note]] or ![[Note#Heading]] or ![[Note^block]]
    ExternalUrl = 4,   // https://... or [lbl](https://...)
    InternalPage = 5,  // cudoc://page/N
    InternalSection = 6,// cudoc://section/Name
    OfficeFileRef = 7  // office://... or file://...docx/.pptx/.xlsx
};

struct KnowledgeLink {
    std::string sourceNoteIdUtf8;
    std::string sourceNoteTitleUtf8;
    std::string targetRawUtf8;
    std::string targetNoteTitleUtf8;
    std::string targetHeadingUtf8;
    std::string targetBlockIdUtf8;
    std::string aliasUtf8;
    std::string contextSnippetUtf8;
    std::size_t lineIndex0 = 0;
    std::size_t byteOffset = 0;
    std::size_t byteLength = 0;
    KnowledgeLinkKind kind = KnowledgeLinkKind::WikiNote;
};

struct UnlinkedMention {
    std::string sourceNoteIdUtf8;
    std::string sourceNoteTitleUtf8;
    std::string targetNoteTitleUtf8;
    std::string matchedTextUtf8;
    std::size_t byteOffset = 0;
    std::size_t byteLength = 0;
    std::string contextSnippetUtf8;
};

struct LinkPreviewResult {
    bool found = false;
    std::string targetTitleUtf8;
    std::string resolvedSectionUtf8;
    std::string previewExcerptUtf8;
    std::string resolvedPathUtf8;
};

struct GraphFilterOptions {
    std::string requiredTagUtf8;
    std::string folderPrefixUtf8;
    std::string propKeyUtf8;
    std::string propValueUtf8;
    bool includeOrphans = true;
    bool includeUnresolved = true;
};

struct GraphNode2D {
    std::string idUtf8;
    std::string labelUtf8;
    std::string folderUtf8;
    std::vector<std::string> tagsUtf8;
    int degree = 0;
    bool resolved = true;
    bool isCenter = false;
    double x = 0.0;
    double y = 0.0;
    double vx = 0.0;
    double vy = 0.0;
};

struct GraphEdge2D {
    std::string fromIdUtf8;
    std::string toIdUtf8;
    KnowledgeLinkKind kind = KnowledgeLinkKind::WikiNote;
    bool resolved = true;
};

struct GraphViewData {
    std::vector<GraphNode2D> nodes;
    std::vector<GraphEdge2D> edges;
};

struct RenameImpactReport {
    std::size_t notesModified = 0;
    std::size_t referencesUpdated = 0;
    std::size_t brokenLinksRemaining = 0;
};

// ---------------- Reusable Elements / Stickers / Snippets & Diagrams ----------------

struct ReusableElementEntry {
    std::string idUtf8;
    std::string nameUtf8;
    std::string categoryUtf8; // "Pegatinas", "Diagramas", "Fragmentos"
    std::vector<std::string> tagsUtf8;
    std::vector<AnnotationObject> objects;
};

class ReusableElementLibrary {
public:
    std::string SaveSnippet(
        const std::string& nameUtf8,
        const std::string& categoryUtf8,
        const std::vector<AnnotationObject>& sourceObjects,
        const std::vector<std::string>& tagsUtf8 = {});

    bool UpdateSnippetName(const std::string& idUtf8, const std::string& newNameUtf8);
    bool RemoveSnippet(const std::string& idUtf8);

    // Clones stored objects shifted smoothly to (targetX, targetY) and allocates fresh IDs via DocumentModel + CommandStack
    bool InsertSnippetIntoDocument(
        const std::string& idUtf8,
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        double targetX,
        double targetY,
        std::vector<std::uint64_t>* outInsertedIds = nullptr) const;

    const std::vector<ReusableElementEntry>& Entries() const noexcept { return entries_; }
    std::vector<ReusableElementEntry> FilterByCategory(const std::string& categoryUtf8) const;

    bool SaveToFile(const std::wstring& jsonPath, std::string* outErr = nullptr) const;
    bool LoadFromFile(const std::wstring& jsonPath, std::string* outErr = nullptr);

private:
    std::uint64_t nextSeq_ = 1;
    std::vector<ReusableElementEntry> entries_;
};

// ---------------- Sticky Notes, Flow Diagrams & Themes/Command Palette ----------------

struct StickyNoteExportResult {
    std::size_t count = 0;
    std::string markdownUtf8;
    std::string jsonUtf8;
};

struct DiagramNodeSpec {
    std::string labelUtf8;
    ShapeKind shape = ShapeKind::Rectangle;
    double x = 80.0;
    double y = 80.0;
    double w = 150.0;
    double h = 58.0;
    DocColor fill = DocColor::RGBA(239, 246, 255, 255);
    DocColor stroke = DocColor::RGBA(37, 99, 235, 255);
};

struct CommandPaletteEntry {
    int commandId = 0;
    std::string slashTriggerUtf8; // e.g. "/tabla", "/grafo", "/diaria"
    std::string titleUtf8;
    std::string categoryUtf8;
    std::string shortcutUtf8;
    std::string keywordsUtf8;
};

class CommandPaletteRegistry {
public:
    void RegisterDefaultCommands();
    void Register(CommandPaletteEntry entry);
    void Clear() noexcept { entries_.clear(); }

    const std::vector<CommandPaletteEntry>& All() const noexcept { return entries_; }

    // Fuzzy substring / acronym match; leading '/' prioritizes slashTriggerUtf8
    std::vector<CommandPaletteEntry> Search(const std::string& queryUtf8, std::size_t maxResults = 24) const;

private:
    std::vector<CommandPaletteEntry> entries_;
};

struct ThemeProfile {
    std::string idUtf8;
    std::string displayNameUtf8;
    bool dark = false;
    bool highContrast = false;
    DocColor canvasBg{};
    DocColor cardBg{};
    DocColor accent{};
    DocColor inkDefault{};
    std::string cssSnippetUtf8;
};

class ThemeProfileRegistry {
public:
    static std::vector<ThemeProfile> BuiltInProfiles();
    static bool SaveProfileToFile(const ThemeProfile& prof, const std::wstring& path, std::string* outErr = nullptr);
    static bool LoadProfileFromFile(const std::wstring& path, ThemeProfile& outProf, std::string* outErr = nullptr);
};

// ---------------- Main Hybrid KnowledgeVault & Graph Engine ----------------

class KnowledgeVault {
public:
    // Note CRUD, Folders, Trash, Restore
    std::string CreateNote(
        const std::string& titleUtf8,
        const std::string& folderPathUtf8 = "",
        const std::string& bodyMarkdownUtf8 = "",
        const std::vector<NoteProperty>& properties = {},
        PaperPattern paper = PaperPattern::Plain,
        std::uint32_t coverRgba = 0x1D4ED8FFu);

    VaultNoteRecord* FindNoteMutableById(const std::string& noteIdUtf8);
    const VaultNoteRecord* FindNoteById(const std::string& noteIdUtf8) const;
    const VaultNoteRecord* FindNoteByTitleOrAlias(const std::string& titleOrAliasUtf8) const;
    std::vector<VaultNoteRecord> ActiveNotes() const;
    std::size_t TotalNoteCount(bool includeTrash = false) const;

    bool MoveNote(const std::string& noteIdUtf8, const std::string& dstFolderPathUtf8);
    bool MoveToTrash(const std::string& noteIdUtf8);
    bool RestoreFromTrash(const std::string& noteIdUtf8);
    bool SetNoteProperty(const std::string& noteIdUtf8, const NoteProperty& prop);
    bool SetNoteCoverAndPaper(const std::string& noteIdUtf8, std::uint32_t coverRgba, PaperPattern paper);

    // Templates & Daily Notes
    void RegisterTemplate(NoteTemplate tpl);
    const std::vector<NoteTemplate>& Templates() const noexcept { return templates_; }
    static std::string ExpandTemplateVariables(
        const std::string& rawTemplateUtf8,
        const std::string& titleUtf8,
        const std::string& dateIsoUtf8,
        const std::string& timeHhMmUtf8,
        const std::string& uuidUtf8);
    std::string CreateNoteFromTemplate(
        const std::string& templateNameUtf8,
        const std::string& titleUtf8,
        const std::string& folderPathUtf8 = "",
        const std::string& dateIsoOverrideUtf8 = "",
        const std::string& timeOverrideUtf8 = "");
    std::string OpenOrCreateDailyNote(
        const std::string& dateIsoUtf8,
        const std::string& templateNameUtf8 = "");

    // Synced Blocks (NO-EDIT-027 / OB-EDIT-031)
    bool UpdateSyncedBlock(const std::string& syncIdUtf8, const std::string& newInnerTextUtf8, std::size_t* outNotesTouched = nullptr);
    std::string ResolveTransclusionMarkdown(const std::string& targetExprUtf8, int maxDepth = 3) const;

    // Property Filtering & Full-Workspace Search
    std::vector<VaultNoteRecord> FilterNotes(const NoteFilterCriteria& criteria) const;
    std::vector<KnowledgeSearchHit> SearchWorkspace(const std::string& queryUtf8, std::size_t maxResults = 64) const;

    // Graph, Links, Backlinks, Unlinked Mentions & Atomic Rename
    std::vector<KnowledgeLink> ExtractLinksFromNote(const VaultNoteRecord& note) const;
    std::vector<KnowledgeLink> CollectAllLinks() const;
    std::vector<KnowledgeLink> OutgoingLinks(const std::string& noteIdOrTitleUtf8) const;
    std::vector<KnowledgeLink> Backlinks(const std::string& targetTitleOrAliasUtf8) const;
    std::vector<UnlinkedMention> FindUnlinkedMentions(const std::string& targetTitleUtf8) const;
    bool ConvertMentionToWikiLink(const std::string& sourceNoteIdUtf8, std::size_t byteOffset, std::size_t byteLength, const std::string& targetTitleUtf8);

    LinkPreviewResult ResolveLinkPreview(const std::string& targetExprUtf8) const;
    RenameImpactReport RenameNote(const std::string& oldTitleUtf8, const std::string& newTitleUtf8);
    RenameImpactReport RenameHeadingInNote(const std::string& noteTitleUtf8, const std::string& oldHeadingUtf8, const std::string& newHeadingUtf8);
    RenameImpactReport RenameBlockIdInNote(const std::string& noteTitleUtf8, const std::string& oldBlockIdUtf8, const std::string& newBlockIdUtf8);
    std::vector<KnowledgeLink> FindBrokenWikiLinks() const;

    GraphViewData BuildGlobalGraph(const GraphFilterOptions& filter = {}) const;
    GraphViewData BuildLocalGraph(const std::string& centerTitleUtf8, int maxDepth = 1, const GraphFilterOptions& filter = {}) const;
    static void ComputeGraphLayout2D(GraphViewData& graph, int iterations = 60, double width = 800.0, double height = 560.0);

    // Frontmatter YAML Helpers
    static bool ParseFrontmatterAndBody(
        const std::string& rawMarkdownUtf8,
        std::vector<NoteProperty>& outProps,
        std::vector<std::string>& outAliases,
        std::vector<std::string>& outTags,
        std::string& outBodyUtf8);
    static std::string SerializeFrontmatterAndBody(
        const std::vector<NoteProperty>& props,
        const std::vector<std::string>& aliases,
        const std::vector<std::string>& tags,
        const std::string& bodyUtf8);

    // Local Disk Vault Synchronization (real .md + vault_meta.json directory persistence)
    bool SaveVaultToDirectory(const std::wstring& rootDirPath, std::string* outErr = nullptr) const;
    bool LoadVaultFromDirectory(const std::wstring& rootDirPath, std::string* outErr = nullptr);

private:
    std::uint64_t nextSeq_ = 1;
    std::vector<VaultNoteRecord> notes_;
    std::vector<NoteTemplate> templates_;
};

// ---------------- Document-Level M1 Helpers (Sticky Notes & Diagram Builders) ----------------

std::uint64_t CreateStickyNoteObject(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    double x,
    double y,
    const std::string& textUtf8,
    DocColor paperFill = DocColor::RGBA(254, 249, 195, 248));

StickyNoteExportResult ExportStickyNotesFromDocument(const DocumentModel& doc);

bool CreateDiagramBlockWithConnectors(
    DocumentModel& doc,
    CommandStack& cmdStack,
    ObjectTarget target,
    const std::vector<DiagramNodeSpec>& nodes,
    const std::vector<std::pair<std::size_t, std::size_t>>& edges,
    std::vector<std::uint64_t>* outNodeIds = nullptr,
    std::vector<std::uint64_t>* outConnectorIds = nullptr);

} // namespace DocStudio
