#pragma once
// ============================================================================
// knowledge_catalog_linux.hpp
// Offline CASIO Knowledge Catalog + 16 Official Manuals + TSV Index + Search
// Loads catalog/casio_knowledge_catalog.json (nlohmann::json) and
// manuales/indice_paginas_pdf.tsv cleanly on Linux.
// ============================================================================

#include "../engines/unified_engines.hpp"

#include <string>
#include <vector>

namespace CasioKnowledgeLinux {

struct KeyModeFunc {
    bool available = false;
    std::string label;
    std::string category;
    std::string summary;
    std::string latexFormula;
    std::string latexExample;
    std::string tipOrWarning;
};

struct KeyEntry {
    int calcIdx = 0;
    int keyId = 0;
    std::string keyName;
    std::string pdfFile;
    int pdfPage = 1;
    std::vector<int> demoSeq;
    KeyModeFunc normal;
    KeyModeFunc shift;
    KeyModeFunc alpha;
};

struct IndicatorEntry {
    int calcIdx = 0;
    int spriteIndex = 0;
    std::string symbol;
    std::string title;
    std::string meaning;
    std::string whenAppears;
    std::string relatedKeys;
    std::string pdfFile;
    int pdfPage = 1;
};

struct ProcModelVariant {
    int calcIdx = 0;
    bool supported = false;
    std::vector<int> keys;
    std::string stepsSummary;
    std::string expectedLcd;
    std::string pdfFile;
    int pdfPage = 1;
};

struct ProcedureEntry {
    std::string id;
    std::string category;
    std::string title;
    std::string goalSummary;
    std::string latexFormula;
    std::string commonErrors;
    std::string tags;
    std::vector<ProcModelVariant> models;
};

struct DiagnosticEntry {
    std::string id;
    std::string symptom;
    std::string category;
    std::string why;
    std::string latex;
    std::string fix;
    int quickFixCalcIdx = -1;
    std::vector<int> quickFixSeq;
};

struct ErrorEntry {
    std::string name;
    std::string appliesTo;
    std::string cause;
    std::string resolution;
    std::string pdfFile;
    int pdfPage = 1;
};

struct OfficialPdfManual {
    std::string fileName;
    std::string shortLabel;
    std::string fullTitle;
    std::string modelCategory;
    std::string manualKind;
    int catId = 1;     // 1=Blanca/991CW, 2=Gris, 3=CG50, 4=Software
    int calcMask = 0;  // bitmask: 1=Blanca, 2=Gris, 4=CG50, 8=991CW
};

struct PdfTsvEntry {
    std::string fileName;
    std::string modelCat;
    std::string docType;
    int pageNo = 1;
    std::string heading;
    std::string snippet;
};

struct CuratedGuide {
    std::string title;
    std::string category;
    std::string difficulty;
    std::string latexFormula;
    std::string stepsSummary;
    std::string expectedResult;
    std::string whyItWorks;
    std::string pdfFile;
    int pdfPage = 1;
    int calcIdx = 0;
    std::vector<int> keys;
};

enum class HelpResultKind {
    Guide = 0,
    Procedure = 1,
    Key = 2,
    Indicator = 3,
    Diagnostic = 4,
    Error = 5,
    ManualPage = 6
};

struct SmartHelpItem {
    HelpResultKind kind = HelpResultKind::Guide;
    std::string title;
    std::string badge;
    std::string category;
    std::string summary;
    std::string latexFormula;
    std::string latexExample;
    std::string tipOrWarning;
    std::string stepsText;
    int calcIdx = 0;
    std::vector<int> demoSeq;
    std::string pdfFile;
    int pdfPage = 1;
    int score = 0;
};

class KnowledgeCatalog {
public:
    bool Load(const CasioLinux::AssetPaths& paths, std::string* outDiag = nullptr);
    bool IsLoaded() const noexcept { return loaded_; }

    const std::vector<OfficialPdfManual>& OfficialManuals() const noexcept;
    const std::vector<KeyEntry>& Keys() const noexcept { return keys_; }
    const std::vector<IndicatorEntry>& Indicators() const noexcept { return indicators_; }
    const std::vector<ProcedureEntry>& Procedures() const noexcept { return procedures_; }
    const std::vector<DiagnosticEntry>& Diagnostics() const noexcept { return diagnostics_; }
    const std::vector<ErrorEntry>& Errors() const noexcept { return errors_; }
    const std::vector<PdfTsvEntry>& TsvPages() const noexcept { return tsvPages_; }
    const std::vector<CuratedGuide>& CuratedGuides() const noexcept { return curated_; }

    const KeyEntry* FindKey(int calcIdx, int keyId) const;
    const IndicatorEntry* FindIndicator(int calcIdx, int spriteIndex) const;

    // categoryFilter: -1=All, 0=Basicas/Guias, 1=Algebra/Calculo, 2=Teclas/Indicadores, 3=Diagnostico/Errores, 4=Manuales TSV
    // modelFilter: -1=All, 0=Blanca, 1=Gris, 2=CG50, 3=991CW
    std::vector<SmartHelpItem> Search(const std::string& query,
                                      int categoryFilter = -1,
                                      int modelFilter = -1,
                                      size_t maxResults = 120) const;

private:
    bool loaded_ = false;
    std::vector<KeyEntry> keys_;
    std::vector<IndicatorEntry> indicators_;
    std::vector<ProcedureEntry> procedures_;
    std::vector<DiagnosticEntry> diagnostics_;
    std::vector<ErrorEntry> errors_;
    std::vector<PdfTsvEntry> tsvPages_;
    std::vector<CuratedGuide> curated_;

    void InitCuratedGuides();
};

std::string NormalizeSearchText(const std::string& input);
std::vector<std::string> TokenizeSearchQuery(const std::string& query);

std::string CalcModelDisplayName(int calcIdx);
bool TryConvertKeySequenceBetweenModels(int srcCalcIdx, const std::vector<int>& srcKeys,
                                        int dstCalcIdx, std::vector<int>& outDstKeys);

struct LinearAndLatex {
    std::string linearText;
    std::string latexDisplay;
};

LinearAndLatex ConvertKeyNamesToLinearAndLatex(const std::vector<std::string>& keyNames);

} // namespace CasioKnowledgeLinux
