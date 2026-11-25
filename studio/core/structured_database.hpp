#pragma once

#include "doc_types.hpp"
#include "document_model.hpp"
#include "command_stack.hpp"
#include "knowledge_graph.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

// ---------------- Typed Properties & Schema (Notion + Obsidian .base) ----------------

enum class DbColumnType : std::uint8_t {
    Text = 0,
    Number = 1,
    Date = 2,
    MultiSelect = 3,
    Status = 4,
    Person = 5,
    Checkbox = 6,
    FileMedia = 7,
    Relation = 8,
    Rollup = 9,
    Formula = 10
};

enum class DbRollupFunc : std::uint8_t {
    Count = 0,
    Sum = 1,
    Avg = 2,
    Min = 3,
    Max = 4,
    PercentChecked = 5
};

struct DbColumnDef {
    std::string idUtf8;
    std::string nameUtf8;
    DbColumnType type = DbColumnType::Text;
    std::vector<std::string> allowedOptionsUtf8; // for Status / MultiSelect
    // For Relation
    std::string targetDatabaseIdUtf8;
    // For Rollup
    std::string relationColumnIdUtf8;
    std::string targetPropertyIdUtf8;
    DbRollupFunc rollupFunc = DbRollupFunc::Sum;
    // For Formula
    std::string formulaExprUtf8;
};

struct DbCellValue {
    std::string textUtf8;
    double numberVal = 0.0;
    bool boolVal = false;
    std::vector<std::string> listUtf8; // MultiSelect, Relation row IDs, or File paths
};

struct DbRecord {
    std::string recordIdUtf8;
    std::string titleUtf8;
    std::string linkedMarkdownNoteIdUtf8; // optional 1:1 link with KnowledgeVault note (OB-DATA-066 / NO-DATA-033)
    std::string bodyMarkdownUtf8;         // database record as full page
    std::unordered_map<std::string, DbCellValue> cells;
};

// ---------------- Views, Filters, Sorting, Grouping, Forms & Templates ----------------

enum class DbViewKind : std::uint8_t {
    Table = 0,
    List = 1,
    CardsGallery = 2,
    Kanban = 3,
    Calendar = 4,
    Timeline = 5,
    Chart = 6,
    Dashboard = 7,
    Map = 8
};

struct DbFilterRule {
    std::string columnIdUtf8;
    std::string opUtf8 = "=="; // "==", "!=", ">", ">=", "<", "<=", "contains", "is_checked"
    std::string valueUtf8;
};

struct DbSortRule {
    std::string columnIdUtf8;
    bool ascending = true;
};

struct DbViewConfig {
    std::string viewIdUtf8;
    std::string nameUtf8;
    DbViewKind kind = DbViewKind::Table;
    std::string groupByColumnIdUtf8;
    std::string dateColumnIdUtf8;
    std::string endDateColumnIdUtf8;
    std::string latColumnIdUtf8;
    std::string lonColumnIdUtf8;
    std::string chartValueColumnIdUtf8;
    bool filterConjunctionAnd = true; // true = AND, false = OR
    std::vector<DbFilterRule> filters;
    std::vector<DbSortRule> sorts;
    std::vector<std::string> visibleColumnsUtf8;
};

struct DbRecordTemplate {
    std::string templateIdUtf8;
    std::string nameUtf8;
    std::string defaultTitlePrefixUtf8;
    std::string defaultBodyMarkdownUtf8;
    std::unordered_map<std::string, DbCellValue> defaultCells;
};

struct DbFormFieldSpec {
    std::string columnIdUtf8;
    std::string labelUtf8;
    bool required = false;
};

struct DbConnectedForm {
    std::string formIdUtf8;
    std::string titleUtf8;
    std::vector<DbFormFieldSpec> fields;
};

// ---------------- Projected View Outputs ----------------

struct DbGroupBucket {
    std::string groupKeyUtf8;
    std::vector<DbRecord> records;
    double numericSum = 0.0;
    double numericAvg = 0.0;
};

struct DbCalendarEntry {
    std::string dateIsoUtf8; // YYYY-MM-DD
    std::string recordIdUtf8;
    std::string titleUtf8;
};

struct DbTimelineBar {
    std::string recordIdUtf8;
    std::string titleUtf8;
    std::string startDateIsoUtf8;
    std::string endDateIsoUtf8;
    int durationDays = 0;
};

struct DbMapPin {
    std::string recordIdUtf8;
    std::string titleUtf8;
    double latitude = 0.0;
    double longitude = 0.0;
    double normX = 0.5; // projected [0..1]
    double normY = 0.5; // projected [0..1]
};

struct DbChartSeriesPoint {
    std::string categoryUtf8;
    double value = 0.0;
};

struct DbDashboardSummary {
    std::size_t totalRecords = 0;
    std::size_t completedCount = 0;
    double completionRatePct = 0.0;
    double primaryNumericSum = 0.0;
    double primaryNumericAvg = 0.0;
    std::vector<DbChartSeriesPoint> breakdownByGroup;
};

// ---------------- StructuredDatabase Engine ----------------

class StructuredDatabase {
public:
    explicit StructuredDatabase(std::string dbIdUtf8 = "db_main", std::string titleUtf8 = "Base de Datos");

    const std::string& Id() const noexcept { return dbIdUtf8_; }
    const std::string& Title() const noexcept { return titleUtf8_; }
    void SetTitle(std::string t) { titleUtf8_ = std::move(t); }

    // Schema management
    void AddColumn(DbColumnDef col);
    const DbColumnDef* FindColumn(const std::string& colIdUtf8) const;
    const std::vector<DbColumnDef>& Columns() const noexcept { return columns_; }

    // Record CRUD & Markdown Vault Sync (OB-DATA-066)
    std::string AddRecord(DbRecord rec);
    DbRecord* FindRecordMutable(const std::string& recordIdUtf8);
    const DbRecord* FindRecord(const std::string& recordIdUtf8) const;
    bool RemoveRecord(const std::string& recordIdUtf8);
    const std::vector<DbRecord>& Records() const noexcept { return records_; }

    bool SetCellText(const std::string& recordIdUtf8, const std::string& colIdUtf8, const std::string& valUtf8);
    bool SetCellNumber(const std::string& recordIdUtf8, const std::string& colIdUtf8, double val);
    bool SetCellBool(const std::string& recordIdUtf8, const std::string& colIdUtf8, bool val);
    bool SetCellList(const std::string& recordIdUtf8, const std::string& colIdUtf8, std::vector<std::string> listUtf8);

    // Sync records <-> KnowledgeVault note frontmatter properties (OB-DATA-066)
    void ImportRecordsFromVaultNotes(const KnowledgeVault& vault, const std::string& folderFilterUtf8 = "");
    void SyncRecordsToVaultNotes(KnowledgeVault& vault) const;

    // Relations, Rollups & Formula Evaluator
    void RegisterLinkedDatabase(const StructuredDatabase* targetDb);
    void RecomputeFormulasAndRollups(const KnowledgeVault* optionalVault = nullptr);
    DbCellValue EvaluateFormulaForRecord(
        const DbRecord& rec,
        const std::string& exprUtf8,
        const KnowledgeVault* optionalVault = nullptr) const;

    // Views, Filtering, Sorting & Grouping
    void AddOrUpdateView(DbViewConfig view);
    const DbViewConfig* FindView(const std::string& viewIdUtf8) const;
    const std::vector<DbViewConfig>& Views() const noexcept { return views_; }
    bool SetActiveView(const std::string& viewIdUtf8);
    const std::string& ActiveViewId() const noexcept { return activeViewId_; }

    std::vector<DbRecord> QueryViewRecords(const DbViewConfig& view) const;
    std::vector<DbGroupBucket> QueryGroupedBuckets(const DbViewConfig& view, const std::string& sumColIdUtf8 = "") const;

    // Kanban drag card between status columns
    bool MoveKanbanCard(const std::string& recordIdUtf8, const std::string& statusColumnIdUtf8, const std::string& newStatusUtf8);

    // View projections (Calendar, Timeline, Chart, Dashboard, Map)
    std::vector<DbCalendarEntry> BuildCalendarProjection(const DbViewConfig& view, const std::string& yearMonthPrefixUtf8 = "") const;
    std::vector<DbTimelineBar> BuildTimelineProjection(const DbViewConfig& view) const;
    std::vector<DbChartSeriesPoint> BuildChartProjection(const DbViewConfig& view) const;
    DbDashboardSummary BuildDashboardProjection(const DbViewConfig& view, const std::string& statusColIdUtf8, const std::string& numColIdUtf8) const;
    std::vector<DbMapPin> BuildMapProjection(const DbViewConfig& view) const;

    // Record Templates & Connected Forms (NO-DATA-056, NO-DATA-057)
    void AddRecordTemplate(DbRecordTemplate tpl);
    std::string CreateRecordFromTemplate(const std::string& templateIdUtf8, const std::string& customTitleSuffixUtf8 = "");

    void RegisterForm(DbConnectedForm form);
    bool SubmitForm(
        const std::string& formIdUtf8,
        const std::string& recordTitleUtf8,
        const std::unordered_map<std::string, DbCellValue>& submittedValues,
        std::string* outCreatedRecordId = nullptr,
        std::string* outValidationError = nullptr);

    // Serialization (.base file format for Obsidian Bases & TableBlock JSON)
    std::string SerializeToBaseJson() const;
    bool DeserializeFromBaseJson(const std::string& rawJsonUtf8, std::string* outErr = nullptr);
    bool SaveToBaseFile(const std::wstring& path, std::string* outErr = nullptr) const;
    bool LoadFromBaseFile(const std::wstring& path, std::string* outErr = nullptr);

    // Renders active view into a DocumentModel TableBlock + optional companion PlotBlock
    std::uint64_t SyncToDocumentTableObject(
        DocumentModel& doc,
        CommandStack& cmdStack,
        ObjectTarget target,
        double x,
        double y,
        bool includeCompanionChart = false) const;

    // Date arithmetic utilities exposed for tests
    static int ParseIsoDateToEpochDays(const std::string& isoDateUtf8);
    static std::string FormatEpochDaysToIsoDate(int epochDays);

private:
    std::string dbIdUtf8_;
    std::string titleUtf8_;
    std::uint64_t nextRecSeq_ = 1;
    std::vector<DbColumnDef> columns_;
    std::vector<DbRecord> records_;
    std::vector<DbViewConfig> views_;
    std::string activeViewId_;
    std::vector<DbRecordTemplate> templates_;
    std::vector<DbConnectedForm> forms_;
    std::unordered_map<std::string, const StructuredDatabase*> linkedDbs_;
};

} // namespace DocStudio
