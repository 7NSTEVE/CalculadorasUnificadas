#pragma once

#include "../core/document_model.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio::Interop {

enum class SourceFormat {
    Unknown = 0,
    Goodnotes6,     // .goodnotes (ZIP + Protobuf/JSON strokes + PDF + media)
    Notability,     // .note (ZIP + bplist00/XML NSKeyedArchiver curves + PDF + audio)
    OneNoteSection, // .one ([MS-ONESTORE] / [MS-ONE] binary + ISF/InkML + media)
    OneNotePackage, // .onepkg (MSCF CAB archive of .one sections + hierarchy)
    CuadernuTinta,  // .tinta (CuadernU native binary v1..v5)
    WordDocx,       // .docx (OOXML paragraphs, styles, tables, images, OMML math)
    PowerPointPptx, // .pptx (OOXML slides, xfrm shapes, images, tables, OMML, vector ink)
    PdfDocument,    // .pdf (ISO 32000-1/2 pages, outlines, links, forms, embedded files)
    CudocNative     // .cudoc (native CuadernU container)
};

enum class CompatLevelStatus {
    Full = 0,
    Partial,
    Fallback,
    Unsupported
};

enum class WarningSeverity {
    Info = 0,
    Warning,
    Lossy
};

struct CompatWarning {
    WarningSeverity severity = WarningSeverity::Info;
    std::string codeUtf8;
    std::string titleUtf8;
    std::string detailUtf8;
};

struct CompatibilityReport {
    SourceFormat format = SourceFormat::Unknown;
    std::string formatLabelUtf8;
    std::string sourceNameUtf8;

    // 3 Explicit Compatibility Tiers (Visual, Structural, Editable)
    CompatLevelStatus visualTier = CompatLevelStatus::Unsupported;
    CompatLevelStatus structuralTier = CompatLevelStatus::Unsupported;
    CompatLevelStatus editableTier = CompatLevelStatus::Unsupported;

    std::string visualSummaryUtf8;
    std::string structuralSummaryUtf8;
    std::string editableSummaryUtf8;

    // Quantified inventory of extracted / detected content
    std::size_t pageCount = 0;
    std::size_t editableStrokeCount = 0;
    std::size_t textBoxCount = 0;
    std::size_t shapeCount = 0;
    std::size_t imageCount = 0;
    std::size_t tableCount = 0;
    std::size_t mathBlockCount = 0;
    std::size_t audioClipCount = 0;
    std::size_t attachmentCount = 0;
    std::size_t bookmarkOrSectionCount = 0;
    bool hasBackgroundPdf = false;

    std::vector<CompatWarning> warnings;

    std::string ToJsonUtf8() const;
    std::string FormatHumanSummaryUtf8() const;
};

struct MigrationResult {
    bool ok = false;
    std::string errorUtf8;
    CompatibilityReport report;
    std::vector<std::uint8_t> primaryAudioWavBytes;
    std::vector<std::string> notebookSectionsUtf8;
};

inline const char* FormatName(SourceFormat f) noexcept {
    switch (f) {
    case SourceFormat::Goodnotes6:     return "Goodnotes 6 (.goodnotes)";
    case SourceFormat::Notability:     return "Notability (.note)";
    case SourceFormat::OneNoteSection: return "Microsoft OneNote (.one)";
    case SourceFormat::OneNotePackage: return "Microsoft OneNote Package (.onepkg)";
    case SourceFormat::CuadernuTinta:  return "CuadernU Cl\xC3\xA1sico (.tinta)";
    case SourceFormat::WordDocx:       return "Microsoft Word (.docx)";
    case SourceFormat::PowerPointPptx: return "Microsoft PowerPoint (.pptx)";
    case SourceFormat::PdfDocument:    return "Documento PDF (.pdf)";
    case SourceFormat::CudocNative:    return "CuadernU Nativo (.cudoc)";
    default:                           return "Formato desconocido";
    }
}

inline const char* TierStatusLabel(CompatLevelStatus st) noexcept {
    switch (st) {
    case CompatLevelStatus::Full:        return "Completa";
    case CompatLevelStatus::Partial:     return "Parcial";
    case CompatLevelStatus::Fallback:    return "Respaldo visual";
    case CompatLevelStatus::Unsupported: return "No compatible";
    }
    return "Desconocida";
}

} // namespace DocStudio::Interop
