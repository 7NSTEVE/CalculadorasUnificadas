#pragma once

#include "document_model.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace DocStudio {

class CudocStorage {
public:
    static constexpr int kCurrentFormatVersion = 1;
    static constexpr const char* kFormatMagic = "CalculadoraUnificada.DocumentStudio";

    static bool SerializeToBytes(
        const DocumentModel& doc,
        std::vector<std::uint8_t>& outZipBytes,
        std::string* outErr = nullptr);

    static bool DeserializeFromBytes(
        const std::uint8_t* bytes,
        std::size_t size,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    // Transactional atomic save: .tmp + FlushFileBuffers + ReplaceFileW(.bak) / MoveFileExW
    static bool SaveToFile(
        const std::wstring& path,
        DocumentModel& doc,
        std::string* outErr = nullptr,
        bool simulateFailBeforeRename = false);

    static bool LoadFromFile(
        const std::wstring& path,
        DocumentModel& outDoc,
        std::string* outErr = nullptr);

    static bool LoadFromFileWithRecovery(
        const std::wstring& path,
        DocumentModel& outDoc,
        bool* outUsedBackup = nullptr,
        std::string* outErr = nullptr);
};

} // namespace DocStudio
