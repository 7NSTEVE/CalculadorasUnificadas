#pragma once

#include "../core/document_model.hpp"
#include "winrt_adapters.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

class OpenXmlInterop {
public:
    // Optional pre-rendered page snapshots (PNG bytes per page index) to embed alongside extracted text
    static bool ExportToDocxBytes(
        const DocumentModel& doc,
        const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng,
        std::vector<std::uint8_t>& outDocxBytes,
        std::string* outErr = nullptr);

    static bool ExportToDocxFile(
        const DocumentModel& doc,
        const std::wstring& outPath,
        const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng = {},
        std::string* outErr = nullptr);

    static bool ImportFromDocxBytes(
        const std::uint8_t* bytes,
        std::size_t size,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    static bool ImportFromDocxFile(
        const std::wstring& path,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    static bool ExportToPptxBytes(
        const DocumentModel& doc,
        const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng,
        std::vector<std::uint8_t>& outPptxBytes,
        std::string* outErr = nullptr);

    static bool ExportToPptxFile(
        const DocumentModel& doc,
        const std::wstring& outPath,
        const std::vector<std::vector<std::uint8_t>>& optionalPageSnapshotsPng = {},
        std::string* outErr = nullptr);

    static bool ImportFromPptxBytes(
        const std::uint8_t* bytes,
        std::size_t size,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    static bool ImportFromPptxFile(
        const std::wstring& path,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);
};

} // namespace DocStudio
