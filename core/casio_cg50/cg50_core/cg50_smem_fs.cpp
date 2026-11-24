#include "cg50_smem_fs.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <ctime>
#include <unordered_set>

namespace cg50 {

namespace {

inline std::uint16_t ReadLe16(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

inline std::uint32_t ReadLe32(const std::uint8_t* p) noexcept
{
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

inline void WriteLe16(std::uint8_t* p, std::uint16_t v) noexcept
{
    p[0] = static_cast<std::uint8_t>(v & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

inline void WriteLe32(std::uint8_t* p, std::uint32_t v) noexcept
{
    p[0] = static_cast<std::uint8_t>(v & 0xFFu);
    p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFu);
    p[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFu);
}

void GetCurrentDosDateTime(std::uint16_t& outDate, std::uint16_t& outTime) noexcept
{
    const std::time_t now = std::time(nullptr);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &now);
#else
    localtime_r(&now, &tmBuf);
#endif
    int year = tmBuf.tm_year + 1900;
    if (year < 1980) year = 1980;
    if (year > 2107) year = 2107;
    const int mon = std::clamp(tmBuf.tm_mon + 1, 1, 12);
    const int day = std::clamp(tmBuf.tm_mday, 1, 31);
    const int hr = std::clamp(tmBuf.tm_hour, 0, 23);
    const int min = std::clamp(tmBuf.tm_min, 0, 59);
    const int sec = std::clamp(tmBuf.tm_sec, 0, 59);
    outDate = static_cast<std::uint16_t>(((year - 1980) << 9) | (mon << 5) | day);
    outTime = static_cast<std::uint16_t>((hr << 11) | (min << 5) | (sec >> 1));
}

std::string ToLowerAscii(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

bool EqualsCaseInsensitive(const std::string& a, const std::string& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const unsigned char ca = static_cast<unsigned char>(a[i]);
        const unsigned char cb = static_cast<unsigned char>(b[i]);
        if (std::tolower(ca) != std::tolower(cb)) return false;
    }
    return true;
}

std::vector<std::uint16_t> Utf8ToUtf16(const std::string& utf8)
{
    std::vector<std::uint16_t> out;
    std::size_t i = 0;
    while (i < utf8.size()) {
        const std::uint8_t c = static_cast<std::uint8_t>(utf8[i]);
        std::uint32_t cp = '?';
        if (c < 0x80u) {
            cp = c;
            ++i;
        } else if ((c & 0xE0u) == 0xC0u && i + 1 < utf8.size()) {
            cp = ((c & 0x1Fu) << 6) | (static_cast<std::uint8_t>(utf8[i + 1]) & 0x3Fu);
            i += 2;
        } else if ((c & 0xF0u) == 0xE0u && i + 2 < utf8.size()) {
            cp = ((c & 0x0Fu) << 12) |
                 ((static_cast<std::uint8_t>(utf8[i + 1]) & 0x3Fu) << 6) |
                 (static_cast<std::uint8_t>(utf8[i + 2]) & 0x3Fu);
            i += 3;
        } else if ((c & 0xF8u) == 0xF0u && i + 3 < utf8.size()) {
            cp = ((c & 0x07u) << 18) |
                 ((static_cast<std::uint8_t>(utf8[i + 1]) & 0x3Fu) << 12) |
                 ((static_cast<std::uint8_t>(utf8[i + 2]) & 0x3Fu) << 6) |
                 (static_cast<std::uint8_t>(utf8[i + 3]) & 0x3Fu);
            i += 4;
        } else {
            ++i;
        }
        if (cp <= 0xFFFFu) {
            out.push_back(static_cast<std::uint16_t>(cp));
        } else {
            cp -= 0x10000u;
            out.push_back(static_cast<std::uint16_t>(0xD800u + (cp >> 10)));
            out.push_back(static_cast<std::uint16_t>(0xDC00u + (cp & 0x3FFu)));
        }
    }
    return out;
}

std::string Utf16ToUtf8(const std::vector<std::uint16_t>& u16)
{
    std::string out;
    for (std::size_t i = 0; i < u16.size(); ++i) {
        std::uint32_t cp = u16[i];
        if (cp == 0u) break;
        if (cp >= 0xD800u && cp <= 0xDBFFu && i + 1 < u16.size() &&
            u16[i + 1] >= 0xDC00u && u16[i + 1] <= 0xDFFFu) {
            cp = 0x10000u + (((cp - 0xD800u) << 10) | (u16[i + 1] - 0xDC00u));
            ++i;
        }
        if (cp < 0x80u) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (cp >> 6)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else if (cp < 0x10000u) {
            out.push_back(static_cast<char>(0xE0u | (cp >> 12)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (cp >> 18)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((cp >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (cp & 0x3Fu)));
        }
    }
    return out;
}

std::uint8_t ComputeShortNameChecksum(const std::uint8_t short11[11]) noexcept
{
    std::uint8_t sum = 0u;
    for (int i = 0; i < 11; ++i) {
        sum = static_cast<std::uint8_t>(((sum & 1u) ? 0x80u : 0u) + (sum >> 1) + short11[i]);
    }
    return sum;
}

std::string FormatShortName(const std::uint8_t raw11[11], const std::uint8_t ntRes)
{
    std::string base(reinterpret_cast<const char*>(raw11), 8);
    std::string ext(reinterpret_cast<const char*>(raw11 + 8), 3);
    while (!base.empty() && base.back() == ' ') base.pop_back();
    while (!ext.empty() && ext.back() == ' ') ext.pop_back();
    if ((ntRes & 0x08u) != 0u) base = ToLowerAscii(base);
    if ((ntRes & 0x10u) != 0u) ext = ToLowerAscii(ext);
    if (ext.empty()) return base;
    return base + "." + ext;
}

bool IsValidShortChar(const unsigned char c) noexcept
{
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    switch (c) {
        case '!': case '#': case '$': case '%': case '&': case '\'':
        case '(': case ')': case '-': case '@': case '^': case '_':
        case '`': case '{': case '}': case '~':
            return true;
        default:
            return false;
    }
}

bool IsValidFatFileName(const std::string& name)
{
    if (name.empty() || name.size() > 255u) return false;
    if (name == "." || name == "..") return false;
    if (name.back() == '.' || name.back() == ' ') return false;
    for (unsigned char c : name) {
        if (c < 0x20u) return false;
        switch (c) {
            case '\\': case '/': case ':': case '*':
            case '?':  case '"': case '<': case '>': case '|':
                return false;
            default: break;
        }
    }
    return true;
}

bool IsStrictUpper83(const std::string& name, std::array<std::uint8_t, 11>& outRaw11)
{
    outRaw11.fill(' ');
    const std::size_t dot = name.find('.');
    if (dot != std::string::npos && name.find('.', dot + 1) != std::string::npos) {
        return false; // multiple dots
    }
    const std::string base = (dot == std::string::npos) ? name : name.substr(0, dot);
    const std::string ext = (dot == std::string::npos) ? std::string() : name.substr(dot + 1);
    if (base.empty() || base.size() > 8u || ext.size() > 3u) return false;
    for (std::size_t i = 0; i < base.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(base[i]);
        if (!IsValidShortChar(c)) return false;
        outRaw11[i] = c;
    }
    for (std::size_t i = 0; i < ext.size(); ++i) {
        const unsigned char c = static_cast<unsigned char>(ext[i]);
        if (!IsValidShortChar(c)) return false;
        outRaw11[8u + i] = c;
    }
    return true;
}

std::array<std::uint8_t, 11> GenerateUniqueShortName(
    const std::string& name,
    const std::unordered_set<std::string>& existingRaw11,
    bool& outNeedsLfn)
{
    std::array<std::uint8_t, 11> raw{};
    if (IsStrictUpper83(name, raw)) {
        const std::string key(reinterpret_cast<const char*>(raw.data()), 11);
        if (existingRaw11.find(key) == existingRaw11.end()) {
            outNeedsLfn = false;
            return raw;
        }
    }

    outNeedsLfn = true;
    const std::size_t lastDot = name.rfind('.');
    std::string rawBase = (lastDot == std::string::npos || lastDot == 0)
                              ? name
                              : name.substr(0, lastDot);
    std::string rawExt = (lastDot == std::string::npos || lastDot == 0)
                             ? std::string()
                             : name.substr(lastDot + 1);

    std::string cleanBase;
    for (unsigned char c : rawBase) {
        if (c == ' ' || c == '.') continue;
        unsigned char u = static_cast<unsigned char>(std::toupper(c));
        cleanBase.push_back(IsValidShortChar(u) ? static_cast<char>(u) : '_');
    }
    if (cleanBase.empty()) cleanBase = "FILE";

    std::string cleanExt;
    for (unsigned char c : rawExt) {
        if (c == ' ' || c == '.') continue;
        unsigned char u = static_cast<unsigned char>(std::toupper(c));
        cleanExt.push_back(IsValidShortChar(u) ? static_cast<char>(u) : '_');
        if (cleanExt.size() == 3u) break;
    }

    for (unsigned int n = 1; n <= 999999u; ++n) {
        const std::string suffix = "~" + std::to_string(n);
        const std::size_t keep = (suffix.size() < 8u) ? (8u - suffix.size()) : 1u;
        std::string b = cleanBase.substr(0, keep) + suffix;
        raw.fill(' ');
        for (std::size_t i = 0; i < b.size() && i < 8u; ++i) {
            raw[i] = static_cast<std::uint8_t>(b[i]);
        }
        for (std::size_t i = 0; i < cleanExt.size() && i < 3u; ++i) {
            raw[8u + i] = static_cast<std::uint8_t>(cleanExt[i]);
        }
        const std::string key(reinterpret_cast<const char*>(raw.data()), 11);
        if (existingRaw11.find(key) == existingRaw11.end()) {
            return raw;
        }
    }
    return raw;
}

std::vector<std::array<std::uint8_t, 32>> BuildDirEntriesForName(
    const std::string& name,
    const std::array<std::uint8_t, 11>& short11,
    const bool needsLfn,
    const std::uint8_t attr,
    const std::uint16_t firstCluster,
    const std::uint32_t fileSize)
{
    std::vector<std::array<std::uint8_t, 32>> out;
    if (needsLfn) {
        const std::vector<std::uint16_t> u16 = Utf8ToUtf16(name);
        const std::size_t lfnCount = (u16.size() + 12u) / 13u;
        const std::uint8_t cksum = ComputeShortNameChecksum(short11.data());
        for (std::size_t ord = lfnCount; ord >= 1; --ord) {
            std::array<std::uint8_t, 32> e{};
            e[0] = static_cast<std::uint8_t>(ord | ((ord == lfnCount) ? 0x40u : 0x00u));
            e[11] = 0x0Fu;
            e[12] = 0x00u;
            e[13] = cksum;
            e[26] = 0x00u;
            e[27] = 0x00u;
            const std::size_t baseIdx = (ord - 1u) * 13u;
            static constexpr std::size_t kOffsets[13] = {
                1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
            };
            for (std::size_t k = 0; k < 13u; ++k) {
                std::uint16_t ch = 0xFFFFu;
                const std::size_t idx = baseIdx + k;
                if (idx < u16.size()) {
                    ch = u16[idx];
                } else if (idx == u16.size()) {
                    ch = 0x0000u;
                }
                WriteLe16(e.data() + kOffsets[k], ch);
            }
            out.push_back(e);
        }
    }

    std::array<std::uint8_t, 32> s{};
    std::memcpy(s.data(), short11.data(), 11u);
    s[11] = attr;
    s[12] = 0x00u;
    std::uint16_t dosDate = 0, dosTime = 0;
    GetCurrentDosDateTime(dosDate, dosTime);
    WriteLe16(s.data() + 14, dosTime);
    WriteLe16(s.data() + 16, dosDate);
    WriteLe16(s.data() + 18, dosDate);
    WriteLe16(s.data() + 20, 0u);
    WriteLe16(s.data() + 22, dosTime);
    WriteLe16(s.data() + 24, dosDate);
    WriteLe16(s.data() + 26, firstCluster);
    WriteLe32(s.data() + 28, fileSize);
    out.push_back(s);
    return out;
}

std::vector<std::string> SplitPath(const std::string& rawPath, bool& outValid)
{
    outValid = true;
    std::vector<std::string> parts;
    std::string cur;
    for (char ch : rawPath) {
        if (ch == '/' || ch == '\\') {
            if (!cur.empty()) {
                if (cur == "." || cur == "..") {
                    outValid = false;
                    return {};
                }
                parts.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) {
        if (cur == "." || cur == "..") {
            outValid = false;
            return {};
        }
        parts.push_back(cur);
    }
    return parts;
}

std::string JoinPath(const std::string& parent, const std::string& leaf)
{
    if (parent.empty() || parent == "/") return "/" + leaf;
    return parent + "/" + leaf;
}

} // namespace

bool Cg50SmemFs::Mount(std::uint8_t* userRom, const std::size_t userRomSize,
                       const std::uint8_t* c2Rom, const std::size_t c2RomSize)
{
    mounted_ = false;
    lastError_.clear();
    if (!ftl_.Mount(userRom, userRomSize, c2Rom, c2RomSize)) {
        lastError_ = ftl_.LastError();
        return false;
    }
    if (!ParseBpb()) {
        return false;
    }
    mounted_ = true;
    return true;
}

bool Cg50SmemFs::ParseBpb()
{
    std::uint8_t sec0[512]{};
    if (!ftl_.ReadSector(0u, sec0)) {
        lastError_ = "Failed reading LBA 0";
        return false;
    }

    auto tryVbr = [&](const std::uint32_t lba) -> bool {
        std::uint8_t vbr[512]{};
        if (!ftl_.ReadSector(lba, vbr)) return false;
        if (vbr[510] != 0x55u || vbr[511] != 0xAAu) return false;
        const std::uint16_t bps = ReadLe16(vbr + 11);
        const std::uint8_t spc = vbr[13];
        const std::uint16_t rsv = ReadLe16(vbr + 14);
        const std::uint8_t nfats = vbr[16];
        const std::uint16_t rootEnt = ReadLe16(vbr + 17);
        const std::uint16_t tot16 = ReadLe16(vbr + 19);
        const std::uint16_t spf = ReadLe16(vbr + 22);
        const std::uint32_t tot32 = ReadLe32(vbr + 32);
        const std::uint32_t tot = (tot16 != 0u) ? tot16 : tot32;
        if (bps != 512u || spc == 0u || (spc & (spc - 1u)) != 0u ||
            rsv == 0u || nfats == 0u || rootEnt == 0u || spf == 0u || tot < 100u) {
            return false;
        }
        vbrLba_ = lba;
        bytesPerSector_ = bps;
        sectorsPerCluster_ = spc;
        reservedSectors_ = rsv;
        numFats_ = nfats;
        rootEntryCount_ = rootEnt;
        totalSectors_ = tot;
        sectorsPerFat_ = spf;
        fat1StartLba_ = vbrLba_ + reservedSectors_;
        rootDirStartLba_ = fat1StartLba_ + static_cast<std::uint32_t>(numFats_) * sectorsPerFat_;
        rootDirSectors_ = (static_cast<std::uint32_t>(rootEntryCount_) * 32u + bytesPerSector_ - 1u) /
                          bytesPerSector_;
        dataStartLba_ = rootDirStartLba_ + rootDirSectors_;
        const std::uint32_t dataSectors = totalSectors_ - (dataStartLba_ - vbrLba_);
        totalClusters_ = dataSectors / sectorsPerCluster_;

        volumeLabel_.assign(reinterpret_cast<const char*>(vbr + 43), 11u);
        while (!volumeLabel_.empty() && volumeLabel_.back() == ' ') volumeLabel_.pop_back();
        return totalClusters_ >= 16u;
    };

    // 1. Check MBR partition table at LBA 0 (0x1BE..0x1FE)
    if (sec0[510] == 0x55u && sec0[511] == 0xAAu) {
        for (int p = 0; p < 4; ++p) {
            const std::uint32_t partLba = ReadLe32(sec0 + 0x1BEu + p * 16u + 8u);
            if (partLba > 0u && partLba < 4096u && tryVbr(partLba)) {
                return true;
            }
        }
        // 2. Superfloppy (VBR directly at LBA 0)
        if (tryVbr(0u)) return true;
    }

    // 3. Standard Casio fx-CG50 Fugue hidden partition offset (LBA 29)
    if (tryVbr(29u)) return true;

    lastError_ = "Valid FAT16 BPB not found inside Fugue FTL volume";
    return false;
}

bool Cg50SmemFs::ReadFatTable(const std::uint8_t fatIndex,
                              std::vector<std::uint16_t>& outFat) const
{
    if (!mounted_ || fatIndex >= numFats_) return false;
    const std::size_t entries = static_cast<std::size_t>(sectorsPerFat_) * (bytesPerSector_ / 2u);
    outFat.assign(entries, 0u);
    const std::uint32_t baseLba = fat1StartLba_ + static_cast<std::uint32_t>(fatIndex) * sectorsPerFat_;
    std::uint8_t sec[512]{};
    for (std::uint32_t s = 0; s < sectorsPerFat_; ++s) {
        if (!ftl_.ReadSector(baseLba + s, sec)) return false;
        for (std::size_t i = 0; i < 256u; ++i) {
            outFat[s * 256u + i] = ReadLe16(sec + i * 2u);
        }
    }
    return true;
}

bool Cg50SmemFs::WriteFatTables(const std::vector<std::uint16_t>& fat)
{
    if (!mounted_) return false;
    std::uint8_t sec[512]{};
    for (std::uint8_t fi = 0; fi < numFats_; ++fi) {
        const std::uint32_t baseLba = fat1StartLba_ + static_cast<std::uint32_t>(fi) * sectorsPerFat_;
        for (std::uint32_t s = 0; s < sectorsPerFat_; ++s) {
            for (std::size_t i = 0; i < 256u; ++i) {
                const std::size_t idx = s * 256u + i;
                const std::uint16_t v = (idx < fat.size()) ? fat[idx] : 0u;
                WriteLe16(sec + i * 2u, v);
            }
            if (!ftl_.WriteSector(baseLba + s, sec)) {
                lastError_ = ftl_.LastError();
                return false;
            }
        }
    }
    return true;
}

bool Cg50SmemFs::ReadClusterChain(const std::vector<std::uint16_t>& fat,
                                  const std::uint16_t firstCluster,
                                  std::vector<std::uint16_t>& outChain) const
{
    outChain.clear();
    if (firstCluster < 2u) return true;
    std::uint16_t cur = firstCluster;
    std::size_t guard = 0;
    while (cur >= 2u && cur < 0xFFF7u) {
        if (cur >= 2u + totalClusters_ || cur >= fat.size()) {
            lastError_ = "Corrupt FAT16 cluster reference";
            return false;
        }
        outChain.push_back(cur);
        if (++guard > totalClusters_) {
            lastError_ = "Cyclic FAT16 cluster chain detected";
            return false;
        }
        cur = fat[cur];
    }
    return true;
}

bool Cg50SmemFs::ReadDirSlots(const std::uint16_t dirCluster,
                              std::vector<std::uint8_t>& outBuf,
                              std::vector<std::uint32_t>& outSectors) const
{
    outBuf.clear();
    outSectors.clear();
    if (dirCluster == 0u) {
        outSectors.reserve(rootDirSectors_);
        for (std::uint32_t s = 0; s < rootDirSectors_; ++s) {
            outSectors.push_back(rootDirStartLba_ + s);
        }
    } else {
        std::vector<std::uint16_t> fat;
        if (!ReadFatTable(0, fat)) return false;
        std::vector<std::uint16_t> chain;
        if (!ReadClusterChain(fat, dirCluster, chain)) return false;
        outSectors.reserve(chain.size() * sectorsPerCluster_);
        for (const std::uint16_t cl : chain) {
            const std::uint32_t lba0 = ClusterToLba(cl);
            for (std::uint32_t s = 0; s < sectorsPerCluster_; ++s) {
                outSectors.push_back(lba0 + s);
            }
        }
    }
    outBuf.resize(outSectors.size() * 512u);
    for (std::size_t i = 0; i < outSectors.size(); ++i) {
        if (!ftl_.ReadSector(outSectors[i], outBuf.data() + i * 512u)) {
            lastError_ = "Failed reading directory sector";
            return false;
        }
    }
    return true;
}

bool Cg50SmemFs::WriteDirSlot(const std::vector<std::uint32_t>& dirSectors,
                              const std::uint32_t slotIdx,
                              const std::uint8_t entry32[32])
{
    const std::size_t secIdx = slotIdx / 16u;
    const std::size_t offInSec = (slotIdx % 16u) * 32u;
    if (secIdx >= dirSectors.size()) {
        lastError_ = "Directory slot index out of range";
        return false;
    }
    std::uint8_t sec[512]{};
    if (!ftl_.ReadSector(dirSectors[secIdx], sec)) return false;
    std::memcpy(sec + offInSec, entry32, 32u);
    return ftl_.WriteSector(dirSectors[secIdx], sec);
}

bool Cg50SmemFs::ParseDirectory(const std::uint16_t dirCluster,
                                const std::string& dirPath,
                                std::vector<DirEntryLoc>& outLocs) const
{
    outLocs.clear();
    std::vector<std::uint8_t> buf;
    std::vector<std::uint32_t> sectors;
    if (!ReadDirSlots(dirCluster, buf, sectors)) return false;

    const std::uint32_t totalSlots = static_cast<std::uint32_t>(buf.size() / 32u);
    struct LfnPart {
        std::uint8_t ord = 0;
        std::uint8_t checksum = 0;
        std::uint32_t slot = 0;
        std::array<std::uint16_t, 13> chars{};
    };
    std::vector<LfnPart> pendingLfn;

    for (std::uint32_t slot = 0; slot < totalSlots; ++slot) {
        const std::uint8_t* e = buf.data() + static_cast<std::size_t>(slot) * 32u;
        if (e[0] == 0x00u) break;
        if (e[0] == 0xE5u) {
            pendingLfn.clear();
            continue;
        }
        const std::uint8_t attr = e[11];
        if (attr == 0x0Fu) {
            LfnPart part;
            part.ord = static_cast<std::uint8_t>(e[0] & 0x1Fu);
            part.checksum = e[13];
            part.slot = slot;
            static constexpr std::size_t kOffsets[13] = {
                1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
            };
            for (std::size_t k = 0; k < 13u; ++k) {
                part.chars[k] = ReadLe16(e + kOffsets[k]);
            }
            if ((e[0] & 0x40u) != 0u) pendingLfn.clear();
            pendingLfn.push_back(part);
            continue;
        }
        if ((attr & 0x08u) != 0u) {
            // Volume label entry
            pendingLfn.clear();
            continue;
        }

        const std::string shortName = FormatShortName(e, e[12]);
        if (shortName == "." || shortName == "..") {
            pendingLfn.clear();
            continue;
        }

        std::string resolvedName = shortName;
        std::uint32_t startSlot = slot;
        std::uint32_t slotCount = 1u;

        if (!pendingLfn.empty()) {
            const std::uint8_t expectedCksum = ComputeShortNameChecksum(e);
            bool lfnOk = true;
            for (std::size_t i = 0; i < pendingLfn.size(); ++i) {
                const std::uint8_t expectedOrd = static_cast<std::uint8_t>(pendingLfn.size() - i);
                if (pendingLfn[i].ord != expectedOrd || pendingLfn[i].checksum != expectedCksum) {
                    lfnOk = false;
                    break;
                }
            }
            if (lfnOk) {
                std::vector<std::uint16_t> u16;
                for (std::size_t i = pendingLfn.size(); i > 0; --i) {
                    const auto& p = pendingLfn[i - 1u];
                    for (std::uint16_t ch : p.chars) {
                        if (ch == 0x0000u || ch == 0xFFFFu) break;
                        u16.push_back(ch);
                    }
                }
                const std::string decoded = Utf16ToUtf8(u16);
                if (!decoded.empty()) {
                    resolvedName = decoded;
                    startSlot = pendingLfn.front().slot;
                    slotCount = static_cast<std::uint32_t>(pendingLfn.size()) + 1u;
                }
            }
            pendingLfn.clear();
        }

        DirEntryLoc loc;
        loc.dirCluster = dirCluster;
        loc.startSlot = startSlot;
        loc.slotCount = slotCount;
        loc.shortSlot = slot;
        loc.entry.name = resolvedName;
        loc.entry.shortName = shortName;
        loc.entry.fullPath = JoinPath(dirPath, resolvedName);
        loc.entry.isDir = (attr & 0x10u) != 0u;
        loc.entry.size = ReadLe32(e + 28);
        loc.entry.firstCluster = ReadLe16(e + 26);
        loc.entry.modTime = ReadLe16(e + 22);
        loc.entry.modDate = ReadLe16(e + 24);
        loc.entry.attr = attr;
        outLocs.push_back(loc);
    }
    return true;
}

bool Cg50SmemFs::FindInDir(const std::uint16_t dirCluster,
                           const std::string& dirPath,
                           const std::string& name,
                           DirEntryLoc& outLoc) const
{
    std::vector<DirEntryLoc> locs;
    if (!ParseDirectory(dirCluster, dirPath, locs)) return false;
    for (const auto& l : locs) {
        if (EqualsCaseInsensitive(l.entry.name, name) ||
            EqualsCaseInsensitive(l.entry.shortName, name)) {
            outLoc = l;
            return true;
        }
    }
    return false;
}

bool Cg50SmemFs::ResolveParentAndLeaf(const std::string& path,
                                      std::uint16_t& outParentCluster,
                                      std::string& outParentPath,
                                      std::string& outLeaf) const
{
    bool valid = false;
    const auto parts = SplitPath(path, valid);
    if (!valid || parts.empty()) {
        lastError_ = "Invalid SMEM path";
        return false;
    }
    std::uint16_t curCluster = 0u;
    std::string curPath = "/";
    for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
        DirEntryLoc loc;
        if (!FindInDir(curCluster, curPath, parts[i], loc)) {
            lastError_ = "Parent directory not found: " + parts[i];
            return false;
        }
        if (!loc.entry.isDir) {
            lastError_ = "Path component is not a directory: " + parts[i];
            return false;
        }
        curCluster = loc.entry.firstCluster;
        curPath = loc.entry.fullPath;
    }
    outParentCluster = curCluster;
    outParentPath = curPath;
    outLeaf = parts.back();
    return true;
}

bool Cg50SmemFs::GetVolumeInfo(SmemVolumeInfo& outInfo) const
{
    if (!mounted_) {
        lastError_ = "SMEM filesystem not mounted";
        return false;
    }
    std::vector<std::uint16_t> fat1, fat2;
    if (!ReadFatTable(0, fat1)) return false;
    bool fatsEqual = true;
    if (numFats_ >= 2u) {
        if (!ReadFatTable(1, fat2)) return false;
        fatsEqual = (fat1 == fat2);
    }

    std::uint32_t freeCount = 0u;
    for (std::uint32_t c = 2u; c < 2u + totalClusters_ && c < fat1.size(); ++c) {
        if (fat1[c] == 0x0000u) ++freeCount;
    }

    outInfo.volumeLabel = volumeLabel_;
    outInfo.bytesPerSector = bytesPerSector_;
    outInfo.sectorsPerCluster = sectorsPerCluster_;
    outInfo.clusterSize = static_cast<std::uint32_t>(bytesPerSector_) * sectorsPerCluster_;
    outInfo.totalSectors = totalSectors_;
    outInfo.totalClusters = totalClusters_;
    outInfo.freeClusters = freeCount;
    outInfo.totalBytes = static_cast<std::uint64_t>(totalClusters_) * outInfo.clusterSize;
    outInfo.freeBytes = static_cast<std::uint64_t>(freeCount) * outInfo.clusterSize;
    outInfo.usedBytes = outInfo.totalBytes - outInfo.freeBytes;
    outInfo.fatTablesEqual = fatsEqual;
    return true;
}

bool Cg50SmemFs::ListDirectory(const std::string& path,
                               std::vector<SmemEntry>& outEntries) const
{
    outEntries.clear();
    if (!mounted_) {
        lastError_ = "SMEM filesystem not mounted";
        return false;
    }
    bool valid = false;
    const auto parts = SplitPath(path, valid);
    if (!valid) {
        lastError_ = "Invalid SMEM directory path";
        return false;
    }

    std::uint16_t curCluster = 0u;
    std::string curPath = "/";
    for (const auto& part : parts) {
        DirEntryLoc loc;
        if (!FindInDir(curCluster, curPath, part, loc)) {
            lastError_ = "Directory not found: " + path;
            return false;
        }
        if (!loc.entry.isDir) {
            lastError_ = "Not a directory: " + path;
            return false;
        }
        curCluster = loc.entry.firstCluster;
        curPath = loc.entry.fullPath;
    }

    std::vector<DirEntryLoc> locs;
    if (!ParseDirectory(curCluster, curPath, locs)) return false;
    outEntries.reserve(locs.size());
    for (const auto& l : locs) {
        outEntries.push_back(l.entry);
    }
    std::sort(outEntries.begin(), outEntries.end(), [](const SmemEntry& a, const SmemEntry& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return ToLowerAscii(a.name) < ToLowerAscii(b.name);
    });
    return true;
}

bool Cg50SmemFs::ReadFile(const std::string& path,
                          std::vector<std::uint8_t>& outData) const
{
    outData.clear();
    if (!mounted_) {
        lastError_ = "SMEM filesystem not mounted";
        return false;
    }
    std::uint16_t parentCluster = 0u;
    std::string parentPath, leaf;
    if (!ResolveParentAndLeaf(path, parentCluster, parentPath, leaf)) return false;

    DirEntryLoc loc;
    if (!FindInDir(parentCluster, parentPath, leaf, loc)) {
        lastError_ = "File not found: " + path;
        return false;
    }
    if (loc.entry.isDir) {
        lastError_ = "Path is a directory: " + path;
        return false;
    }
    if (loc.entry.size == 0u || loc.entry.firstCluster < 2u) {
        return true;
    }

    std::vector<std::uint16_t> fat;
    if (!ReadFatTable(0, fat)) return false;
    std::vector<std::uint16_t> chain;
    if (!ReadClusterChain(fat, loc.entry.firstCluster, chain)) return false;

    const std::size_t clusterBytes = static_cast<std::size_t>(bytesPerSector_) * sectorsPerCluster_;
    outData.reserve(loc.entry.size);
    std::uint8_t sec[512]{};
    std::size_t remaining = loc.entry.size;
    for (const std::uint16_t cl : chain) {
        const std::uint32_t lba0 = ClusterToLba(cl);
        for (std::uint32_t s = 0; s < sectorsPerCluster_ && remaining > 0; ++s) {
            if (!ftl_.ReadSector(lba0 + s, sec)) {
                lastError_ = "Failed reading file sector";
                outData.clear();
                return false;
            }
            const std::size_t take = std::min<std::size_t>(remaining, 512u);
            outData.insert(outData.end(), sec, sec + take);
            remaining -= take;
        }
        if (remaining == 0) break;
    }
    if (remaining > 0) {
        lastError_ = "Truncated FAT16 cluster chain for " + path +
                     " (clusterBytes=" + std::to_string(clusterBytes) + ")";
        outData.clear();
        return false;
    }
    return true;
}

bool Cg50SmemFs::AllocateChain(std::vector<std::uint16_t>& fat,
                               const std::size_t count,
                               std::vector<std::uint16_t>& outClusters) const
{
    outClusters.clear();
    if (count == 0u) return true;
    outClusters.reserve(count);
    for (std::uint32_t c = 2u; c < 2u + totalClusters_ && c < fat.size(); ++c) {
        if (fat[c] == 0x0000u) {
            outClusters.push_back(static_cast<std::uint16_t>(c));
            if (outClusters.size() == count) break;
        }
    }
    if (outClusters.size() < count) {
        lastError_ = "Not enough free SMEM space (FAT16 clusters exhausted)";
        outClusters.clear();
        return false;
    }
    for (std::size_t i = 0; i < outClusters.size(); ++i) {
        fat[outClusters[i]] = (i + 1u < outClusters.size()) ? outClusters[i + 1u] : 0xFFFFu;
    }
    return true;
}

void Cg50SmemFs::FreeChain(std::vector<std::uint16_t>& fat,
                           const std::uint16_t firstCluster) const
{
    std::vector<std::uint16_t> chain;
    if (!ReadClusterChain(fat, firstCluster, chain)) return;
    for (const std::uint16_t c : chain) {
        if (c < fat.size()) fat[c] = 0x0000u;
    }
}

bool Cg50SmemFs::WriteClusterData(const std::uint16_t cluster,
                                  const std::uint8_t* data,
                                  const std::size_t len)
{
    const std::uint32_t lba0 = ClusterToLba(cluster);
    std::uint8_t sec[512]{};
    for (std::uint32_t s = 0; s < sectorsPerCluster_; ++s) {
        std::memset(sec, 0x00, 512u);
        const std::size_t off = static_cast<std::size_t>(s) * 512u;
        if (data != nullptr && off < len) {
            const std::size_t copyLen = std::min<std::size_t>(512u, len - off);
            std::memcpy(sec, data + off, copyLen);
        }
        if (!ftl_.WriteSector(lba0 + s, sec)) {
            lastError_ = ftl_.LastError();
            return false;
        }
    }
    return true;
}

bool Cg50SmemFs::AppendEntriesToDir(
    const std::uint16_t dirCluster,
    std::vector<std::uint16_t>& fat,
    const std::vector<std::array<std::uint8_t, 32>>& entries)
{
    std::vector<std::uint8_t> buf;
    std::vector<std::uint32_t> sectors;
    if (!ReadDirSlots(dirCluster, buf, sectors)) return false;

    const std::uint32_t totalSlots = static_cast<std::uint32_t>(buf.size() / 32u);
    const std::uint32_t needed = static_cast<std::uint32_t>(entries.size());

    // Find contiguous run of free (0x00 or 0xE5) slots.
    std::uint32_t runStart = 0u;
    std::uint32_t runLen = 0u;
    bool found = false;
    for (std::uint32_t s = 0; s < totalSlots; ++s) {
        const std::uint8_t first = buf[static_cast<std::size_t>(s) * 32u];
        if (first == 0x00u || first == 0xE5u) {
            if (runLen == 0u) runStart = s;
            ++runLen;
            if (runLen >= needed) {
                found = true;
                break;
            }
        } else {
            runLen = 0u;
        }
    }

    if (!found) {
        if (dirCluster == 0u) {
            lastError_ = "FAT16 root directory is full (512 entries)";
            return false;
        }
        // Extend subdirectory by one zeroed cluster.
        std::vector<std::uint16_t> existingChain;
        if (!ReadClusterChain(fat, dirCluster, existingChain) || existingChain.empty()) {
            return false;
        }
        std::vector<std::uint16_t> extra;
        if (!AllocateChain(fat, 1u, extra)) return false;
        fat[existingChain.back()] = extra[0];
        if (!WriteClusterData(extra[0], nullptr, 0u)) return false;

        runStart = totalSlots;
        const std::uint32_t newLba0 = ClusterToLba(extra[0]);
        for (std::uint32_t s = 0; s < sectorsPerCluster_; ++s) {
            sectors.push_back(newLba0 + s);
        }
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (!WriteDirSlot(sectors, runStart + static_cast<std::uint32_t>(i), entries[i].data())) {
            return false;
        }
    }
    return true;
}

bool Cg50SmemFs::WriteFile(const std::string& path,
                           const std::uint8_t* data,
                           const std::size_t len,
                           const bool overwrite)
{
    if (!mounted_) {
        lastError_ = "SMEM filesystem not mounted";
        return false;
    }
    if (len > 0 && data == nullptr) {
        lastError_ = "Null data pointer for non-empty file";
        return false;
    }

    std::uint16_t parentCluster = 0u;
    std::string parentPath, leaf;
    if (!ResolveParentAndLeaf(path, parentCluster, parentPath, leaf)) return false;
    if (!IsValidFatFileName(leaf)) {
        lastError_ = "Invalid FAT filename: " + leaf;
        return false;
    }

    std::vector<std::uint16_t> fat;
    if (!ReadFatTable(0, fat)) return false;

    const std::size_t clusterBytes = static_cast<std::size_t>(bytesPerSector_) * sectorsPerCluster_;
    const std::size_t neededClusters = (len == 0u) ? 0u : ((len + clusterBytes - 1u) / clusterBytes);

    DirEntryLoc existing;
    const bool exists = FindInDir(parentCluster, parentPath, leaf, existing);
    if (exists) {
        if (existing.entry.isDir) {
            lastError_ = "Cannot overwrite directory with file: " + path;
            return false;
        }
        if (!overwrite) {
            lastError_ = "File already exists: " + path;
            return false;
        }
        FreeChain(fat, existing.entry.firstCluster);
    }

    std::vector<std::uint16_t> clusters;
    if (!AllocateChain(fat, neededClusters, clusters)) {
        ftl_.DiscardDirty();
        return false;
    }

    for (std::size_t i = 0; i < clusters.size(); ++i) {
        const std::size_t off = i * clusterBytes;
        const std::size_t chunk = std::min<std::size_t>(clusterBytes, len - off);
        if (!WriteClusterData(clusters[i], data + off, chunk)) {
            ftl_.DiscardDirty();
            return false;
        }
    }

    const std::uint16_t firstCluster = clusters.empty() ? 0u : clusters[0];
    if (exists) {
        std::vector<std::uint8_t> buf;
        std::vector<std::uint32_t> sectors;
        if (!ReadDirSlots(parentCluster, buf, sectors)) {
            ftl_.DiscardDirty();
            return false;
        }
        std::uint8_t entry32[32]{};
        std::memcpy(entry32, buf.data() + static_cast<std::size_t>(existing.shortSlot) * 32u, 32u);
        std::uint16_t dosDate = 0, dosTime = 0;
        GetCurrentDosDateTime(dosDate, dosTime);
        WriteLe16(entry32 + 18, dosDate);
        WriteLe16(entry32 + 22, dosTime);
        WriteLe16(entry32 + 24, dosDate);
        WriteLe16(entry32 + 26, firstCluster);
        WriteLe32(entry32 + 28, static_cast<std::uint32_t>(len));
        if (!WriteDirSlot(sectors, existing.shortSlot, entry32)) {
            ftl_.DiscardDirty();
            return false;
        }
    } else {
        // Collect existing raw 8.3 names in parent dir to guarantee collision-free short alias.
        std::vector<std::uint8_t> buf;
        std::vector<std::uint32_t> sectors;
        if (!ReadDirSlots(parentCluster, buf, sectors)) {
            ftl_.DiscardDirty();
            return false;
        }
        std::unordered_set<std::string> existingRaw11;
        const std::size_t nSlots = buf.size() / 32u;
        for (std::size_t s = 0; s < nSlots; ++s) {
            const std::uint8_t* e = buf.data() + s * 32u;
            if (e[0] == 0x00u) break;
            if (e[0] == 0xE5u || e[11] == 0x0Fu) continue;
            existingRaw11.emplace(reinterpret_cast<const char*>(e), 11u);
        }
        bool needsLfn = false;
        const auto short11 = GenerateUniqueShortName(leaf, existingRaw11, needsLfn);
        const auto newEntries = BuildDirEntriesForName(
            leaf, short11, needsLfn, 0x20u /* ARCHIVE */, firstCluster,
            static_cast<std::uint32_t>(len));
        if (!AppendEntriesToDir(parentCluster, fat, newEntries)) {
            ftl_.DiscardDirty();
            return false;
        }
    }

    if (!WriteFatTables(fat) || !ftl_.Flush()) {
        lastError_ = ftl_.LastError().empty() ? lastError_ : ftl_.LastError();
        ftl_.DiscardDirty();
        return false;
    }
    return true;
}

bool Cg50SmemFs::MakeDirectory(const std::string& path)
{
    if (!mounted_) {
        lastError_ = "SMEM filesystem not mounted";
        return false;
    }
    std::uint16_t parentCluster = 0u;
    std::string parentPath, leaf;
    if (!ResolveParentAndLeaf(path, parentCluster, parentPath, leaf)) return false;
    if (!IsValidFatFileName(leaf)) {
        lastError_ = "Invalid FAT directory name: " + leaf;
        return false;
    }

    DirEntryLoc existing;
    if (FindInDir(parentCluster, parentPath, leaf, existing)) {
        if (existing.entry.isDir) return true; // idempotent mkdir
        lastError_ = "A file with the same name already exists: " + path;
        return false;
    }

    std::vector<std::uint16_t> fat;
    if (!ReadFatTable(0, fat)) return false;
    std::vector<std::uint16_t> cl;
    if (!AllocateChain(fat, 1u, cl)) {
        ftl_.DiscardDirty();
        return false;
    }

    // Initialize new directory cluster with "." and ".." entries.
    const std::size_t clusterBytes = static_cast<std::size_t>(bytesPerSector_) * sectorsPerCluster_;
    std::vector<std::uint8_t> initBuf(clusterBytes, 0x00u);
    std::uint16_t dosDate = 0, dosTime = 0;
    GetCurrentDosDateTime(dosDate, dosTime);

    auto fillDot = [&](std::uint8_t* e, const char* name11, const std::uint16_t cluster) {
        std::memcpy(e, name11, 11u);
        e[11] = 0x10u; // DIRECTORY
        WriteLe16(e + 14, dosTime);
        WriteLe16(e + 16, dosDate);
        WriteLe16(e + 18, dosDate);
        WriteLe16(e + 22, dosTime);
        WriteLe16(e + 24, dosDate);
        WriteLe16(e + 26, cluster);
        WriteLe32(e + 28, 0u);
    };
    fillDot(initBuf.data(), ".          ", cl[0]);
    fillDot(initBuf.data() + 32u, "..         ", parentCluster);

    if (!WriteClusterData(cl[0], initBuf.data(), initBuf.size())) {
        ftl_.DiscardDirty();
        return false;
    }

    std::vector<std::uint8_t> buf;
    std::vector<std::uint32_t> sectors;
    if (!ReadDirSlots(parentCluster, buf, sectors)) {
        ftl_.DiscardDirty();
        return false;
    }
    std::unordered_set<std::string> existingRaw11;
    const std::size_t nSlots = buf.size() / 32u;
    for (std::size_t s = 0; s < nSlots; ++s) {
        const std::uint8_t* e = buf.data() + s * 32u;
        if (e[0] == 0x00u) break;
        if (e[0] == 0xE5u || e[11] == 0x0Fu) continue;
        existingRaw11.emplace(reinterpret_cast<const char*>(e), 11u);
    }
    bool needsLfn = false;
    const auto short11 = GenerateUniqueShortName(leaf, existingRaw11, needsLfn);
    const auto newEntries = BuildDirEntriesForName(
        leaf, short11, needsLfn, 0x10u /* DIRECTORY */, cl[0], 0u);

    if (!AppendEntriesToDir(parentCluster, fat, newEntries) ||
        !WriteFatTables(fat) ||
        !ftl_.Flush()) {
        lastError_ = ftl_.LastError().empty() ? lastError_ : ftl_.LastError();
        ftl_.DiscardDirty();
        return false;
    }
    return true;
}

bool Cg50SmemFs::DeleteDirLocRecursive(const DirEntryLoc& loc,
                                       std::vector<std::uint16_t>& fat,
                                       const bool recursive)
{
    if (loc.entry.isDir) {
        std::vector<DirEntryLoc> children;
        if (!ParseDirectory(loc.entry.firstCluster, loc.entry.fullPath, children)) {
            return false;
        }
        if (!children.empty()) {
            if (!recursive) {
                lastError_ = "Directory is not empty: " + loc.entry.fullPath;
                return false;
            }
            for (const auto& child : children) {
                if (!DeleteDirLocRecursive(child, fat, true)) return false;
            }
        }
    }

    // Mark LFN + 8.3 slots as deleted (0xE5).
    std::vector<std::uint8_t> buf;
    std::vector<std::uint32_t> sectors;
    if (!ReadDirSlots(loc.dirCluster, buf, sectors)) return false;
    for (std::uint32_t i = 0; i < loc.slotCount; ++i) {
        const std::uint32_t slot = loc.startSlot + i;
        std::uint8_t e[32]{};
        std::memcpy(e, buf.data() + static_cast<std::size_t>(slot) * 32u, 32u);
        e[0] = 0xE5u;
        if (!WriteDirSlot(sectors, slot, e)) return false;
    }

    FreeChain(fat, loc.entry.firstCluster);
    return true;
}

bool Cg50SmemFs::DeletePath(const std::string& path, const bool recursive)
{
    if (!mounted_) {
        lastError_ = "SMEM filesystem not mounted";
        return false;
    }
    std::uint16_t parentCluster = 0u;
    std::string parentPath, leaf;
    if (!ResolveParentAndLeaf(path, parentCluster, parentPath, leaf)) return false;

    DirEntryLoc loc;
    if (!FindInDir(parentCluster, parentPath, leaf, loc)) {
        lastError_ = "Path not found: " + path;
        return false;
    }

    std::vector<std::uint16_t> fat;
    if (!ReadFatTable(0, fat)) return false;

    if (!DeleteDirLocRecursive(loc, fat, recursive) ||
        !WriteFatTables(fat) ||
        !ftl_.Flush()) {
        lastError_ = ftl_.LastError().empty() ? lastError_ : ftl_.LastError();
        ftl_.DiscardDirty();
        return false;
    }
    return true;
}

bool Cg50SmemFs::ReadRawSector(const std::uint32_t lba, std::uint8_t out512[512]) const
{
    return mounted_ && ftl_.ReadSector(lba, out512);
}

bool Cg50SmemFs::WriteRawSector(const std::uint32_t lba, const std::uint8_t in512[512])
{
    return mounted_ && ftl_.WriteSector(lba, in512);
}

bool Cg50SmemFs::FlushRawWrites()
{
    return mounted_ && ftl_.Flush();
}

} // namespace cg50
