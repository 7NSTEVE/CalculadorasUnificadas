#pragma once

#include "interop_types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio::Interop {

// High-Fidelity Multi-Format Interoperability & Notebook Migration Engine (Track E):
// Supports Goodnotes 6 (.goodnotes), Notability (.note), Microsoft OneNote (.one/.onepkg),
// CuadernU Legacy (.tinta), Microsoft Word (.docx), Microsoft PowerPoint (.pptx),
// PDF (.pdf), and CuadernU Native (.cudoc) across Visual, Structural, and Editable tiers.
class NotebookMigrationEngine {
public:
    static SourceFormat DetectFormat(
        const std::string& filenameUtf8,
        const std::uint8_t* bytes = nullptr,
        std::size_t size = 0);

    // Pre-conversion analysis & compatibility warnings dialog data (F-012)
    static CompatibilityReport AnalyzeCompatibilityBytes(
        const std::uint8_t* bytes,
        std::size_t size,
        const std::string& filenameHintUtf8);

    static CompatibilityReport AnalyzeCompatibilityFile(
        const std::wstring& path);

    // Full high-fidelity migration into editable DocumentModel
    static bool ImportAndMigrateBytes(
        const std::uint8_t* bytes,
        std::size_t size,
        const std::string& filenameHintUtf8,
        DocumentModel& outDoc,
        MigrationResult& outResult);

    static bool ImportAndMigrateFile(
        const std::wstring& path,
        DocumentModel& outDoc,
        MigrationResult& outResult);
};

} // namespace DocStudio::Interop
