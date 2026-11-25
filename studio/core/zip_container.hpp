#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace DocStudio {

struct ZipEntry {
    std::string pathUtf8;
    std::vector<std::uint8_t> data;
};

// RFC-1951 Deflate/Store ZIP archive reader & writer backed by zlib.
class ZipArchive {
public:
    void SetEntry(const std::string& pathUtf8, std::vector<std::uint8_t> data);
    void SetTextEntry(const std::string& pathUtf8, const std::string& textUtf8);

    bool HasEntry(const std::string& pathUtf8) const;
    const std::vector<std::uint8_t>* FindEntry(const std::string& pathUtf8) const;
    std::vector<std::string> ListEntries() const;

    bool Serialize(std::vector<std::uint8_t>& outZipBytes, bool compress = true, std::string* outErr = nullptr) const;
    bool Deserialize(const std::uint8_t* bytes, std::size_t size, std::string* outErr = nullptr);

private:
    std::unordered_map<std::string, std::vector<std::uint8_t>> entries_;
};

} // namespace DocStudio
